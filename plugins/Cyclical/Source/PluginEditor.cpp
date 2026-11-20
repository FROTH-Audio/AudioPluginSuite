#include "PluginEditor.h"

#include "BinaryData.h"
#include "SuiteBrand.h"
#include "SuiteSettings.h"

namespace
{
    // Plan 05 R4: the default window is 980 x 700, resizable from 940 x 660 (plan 6.1).
    constexpr int editorWidth = 980;
    constexpr int editorHeight = 700;
    constexpr int headerHeight = 44;
    constexpr int footerHeight = 26;
    constexpr int groupGap = 8;   // the suite's one-gap rule (plan 05 R7)
    constexpr int groupTitleHeight = SuiteUI::groupTitleHeight;

    using Processor = CyclicalProcessor;

    const juce::Image& brandLockup()
    {
        static const juce::Image image = juce::ImageCache::getFromMemory (BinaryData::lockup_cyclical_png,
                                                                         BinaryData::lockup_cyclical_pngSize);
        return image;
    }
}

//==============================================================================
CyclicalEditor::SlotCard::SlotCard (int slotIndex, juce::Colour accentColour)
    : accentColour (accentColour),
      slot (slotIndex),
      waveform (accentColour)
{
    const int note = CyclicalProcessor::firstSlotNote + slot;
    title.setText ("SLOT " + juce::String (slot + 1) + "  -  "
                       + juce::MidiMessage::getMidiNoteName (note, true, false, 4),
                   juce::dontSendNotification);
    title.setFont (SuiteUI::uiFont (15.0f, true));
    title.setColour (juce::Label::textColourId, accentColour);
    addAndMakeVisible (title);

    addAndMakeVisible (waveform);
    addAndMakeVisible (start);
    addAndMakeVisible (length);
    addAndMakeVisible (speed);
    addAndMakeVisible (velocity);

    direction.setOptions ({ "FWD", "REV", "PP" });
    addAndMakeVisible (direction);

    addAndMakeVisible (pingRand);
    addAndMakeVisible (granular);

    start.slider.setTextValueSuffix (" %");
    start.slider.setNumDecimalPlacesToDisplay (1);
    length.slider.setTextValueSuffix (" %");
    length.slider.setNumDecimalPlacesToDisplay (1);
    speed.slider.setNumDecimalPlacesToDisplay (2);   // the "x" unit comes from the parameter
    velocity.slider.setTextValueSuffix (" %");
    velocity.slider.setNumDecimalPlacesToDisplay (0);

    for (auto* pill : { &pingRand, &granular })
        pill->setClickingTogglesState (true);
}

void CyclicalEditor::SlotCard::resized()
{
    auto area = getLocalBounds();
    title.setBounds (area.removeFromTop (20));
    area.removeFromTop (2);

    waveform.setBounds (area.removeFromTop (110));
    area.removeFromTop (groupGap);

    // Four knobs at the suite size (R1/R2).
    const int knobWidth = area.getWidth() / 4;
    auto knobRow = area.removeFromTop (SuiteUI::knobRowHeight);
    start.setBounds (knobRow.removeFromLeft (knobWidth).reduced (3));
    length.setBounds (knobRow.removeFromLeft (knobWidth).reduced (3));
    speed.setBounds (knobRow.removeFromLeft (knobWidth).reduced (3));
    velocity.setBounds (knobRow.removeFromLeft (knobWidth).reduced (3));
    area.removeFromTop (groupGap);

    // Direction selector and the two switches (pills at 36 px, R3). Ping Rand's caption
    // needs the wide pill: the look and feel trims the dot's column from the text bounds.
    auto pillRow = area;
    direction.setBounds (pillRow.removeFromLeft (150).reduced (6, 2)
                             .withHeight (SuiteUI::pillHeight)
                             .withY (pillRow.getY() + (pillRow.getHeight() - SuiteUI::pillHeight) / 2));
    pingRand.setBounds (pillRow.removeFromLeft (175).reduced (4, 2)
                            .withSizeKeepingCentre (167, SuiteUI::pillHeight));
    granular.setBounds (pillRow.reduced (4, 2)
                            .withSizeKeepingCentre (juce::jmin (96, pillRow.getWidth() - 6), SuiteUI::pillHeight));
}

//==============================================================================
CyclicalEditor::CyclicalEditor (CyclicalProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p)
{
    setLookAndFeel (&lookAndFeel);

    // The bar exists before setSize(), because setSize() runs resized(), which lays it out.
    presetBar.reset (new SuitePresets::PresetBar (processor, juce::Colour (0xff8fa3b8)));

    // The cards exist before setSize() too: resized() lays the page out.
    auto& apvts = processor.apvts;

    auto bind = [this, &apvts] (const char* id, juce::Slider& slider)
    {
        sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider));
    };

    for (int slot = 0; slot < Processor::numSlots; ++slot)
    {
        cards[(size_t) slot] = std::make_unique<SlotCard> (slot, juce::Colour (0xff8fa3b8));
        auto& card = *cards[(size_t) slot];
        addAndMakeVisible (card);

        bind (Processor::slotStartIds[(size_t) slot], card.start.slider);
        bind (Processor::slotLengthIds[(size_t) slot], card.length.slider);
        bind (Processor::slotSpeedIds[(size_t) slot], card.speed.slider);
        bind (Processor::slotVelocityIds[(size_t) slot], card.velocity.slider);

        // A choice parameter through a ParameterAttachment: the denormalised value is the
        // choice index as a float (the Micro-Timing Chaos rate selector pattern).
        choiceAttachments.push_back (std::make_unique<juce::ParameterAttachment> (
            *apvts.getParameter (Processor::slotDirectionIds[(size_t) slot]),
            [&card] (float value) { card.direction.setSelectedIndex (juce::roundToInt (value)); }));
        cards[(size_t) slot]->direction.onChange = [this, slot] (int index)
        {
            choiceAttachments[(size_t) slot]->setValueAsCompleteGesture ((float) index);
        };
        choiceAttachments.back()->sendInitialUpdate();

        buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            apvts, Processor::slotPingRandomIds[(size_t) slot], card.pingRand));
        buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            apvts, Processor::slotGranularIds[(size_t) slot], card.granular));

        card.waveform.onLoadClicked = [this, slot] { browseForLoop (slot); };

        SuiteUI::describe (card.start, "Where the loop starts, as a share of its length (capped at 95 %).");
        SuiteUI::describe (card.length, "How much of the loop plays, as a share of its length.");
        SuiteUI::describe (card.speed, "Playback speed. Inert while Grain is on.");
        SuiteUI::describe (card.velocity, "How strongly the slot's key velocity scales the level.");
        SuiteUI::describe (card.direction, "Forward, Reverse, or Ping-Pong (turns at both bounds).");
        SuiteUI::describe (card.pingRand, "Randomises the ping-pong turn points per trigger. Only in Ping-Pong.");
        SuiteUI::describe (card.granular, "80 ms grains instead of straight playback. Speed is inert.");
        SuiteUI::describe (card.waveform, "The loaded loop, its region band and the playhead. LOAD opens a browser.");
    }

    SuiteUI::describe (pageSelector, "SLOTS: the four loop cards. GLOBAL: the shared knobs and the transport.");
    SuiteUI::describe (*presetBar, "Factory presets, INIT and SAVE. The menu lists user presets.");
    SuiteUI::describe (headerMenu, "Settings... and About...");

    setSize (editorWidth, editorHeight);
    setResizable (true, true);
    setResizeLimits (editorWidth - 40, editorHeight - 40, 1200, 1200);

    // The shared help line (plan 12): one line, one idle text, no stale strings. The
    // footer sentence is the idle text, so the band reads the same before a hover.
    helpLine.setIdleText ("Keys C4-D#4 trigger slots 1-4. No recording: loops load from the library or disk.");
    helpLine.setComponentID ("cyclical-help");
    helpLine.setEnablePredicate ([] { return SuiteSettings::shared().getBool (SuiteSettings::tooltipsKey, true); });
    SuiteUI::attachHelpLine (*this, helpLine, helpTracker);
    addAndMakeVisible (helpLine);

    // GLOBAL page: the shared knobs and selectors.
    addAndMakeVisible (masterCtrl);
    addAndMakeVisible (panCtrl);
    addAndMakeVisible (pitchCtrl);
    addAndMakeVisible (crossfadeCtrl);
    addAndMakeVisible (swingCtrl);
    addAndMakeVisible (quantizeCaption);
    addAndMakeVisible (quantizeSelector);
    addAndMakeVisible (triggerCaption);
    addAndMakeVisible (triggerSelector);
    addAndMakeVisible (beatReadout);

    masterCtrl.slider.setTextValueSuffix (" dB");
    masterCtrl.slider.setNumDecimalPlacesToDisplay (1);
    panCtrl.slider.setTextValueSuffix (" %");
    panCtrl.slider.setNumDecimalPlacesToDisplay (1);
    crossfadeCtrl.slider.setTextValueSuffix (" ms");
    crossfadeCtrl.slider.setNumDecimalPlacesToDisplay (0);
    swingCtrl.slider.setTextValueSuffix (" %");
    swingCtrl.slider.setNumDecimalPlacesToDisplay (1);
    pitchCtrl.slider.setNumDecimalPlacesToDisplay (0);

    bind (Processor::masterVolumeId, masterCtrl.slider);
    bind (Processor::panId, panCtrl.slider);
    bind (Processor::crossfadeLengthId, crossfadeCtrl.slider);
    bind (Processor::swingId, swingCtrl.slider);
    bind (Processor::pitchShiftId, pitchCtrl.slider);

    quantizeSelector.setOptions ({ "OFF", "1/16", "1/8", "1/4", "1/2", "BAR" });
    {
        auto attachment = std::make_unique<juce::ParameterAttachment> (
            *apvts.getParameter (Processor::quantizeId),
            [&selector = quantizeSelector] (float value) { selector.setSelectedIndex (juce::roundToInt (value)); });
        quantizeSelector.onChange = [raw = attachment.get()] (int index)
        {
            raw->setValueAsCompleteGesture ((float) index);
        };
        attachment->sendInitialUpdate();
        choiceAttachments.push_back (std::move (attachment));
    }

    triggerSelector.setOptions ({ "LATCH", "GATE" });
    {
        auto attachment = std::make_unique<juce::ParameterAttachment> (
            *apvts.getParameter (Processor::triggerModeId),
            [&selector = triggerSelector] (float value) { selector.setSelectedIndex (juce::roundToInt (value)); });
        triggerSelector.onChange = [raw = attachment.get()] (int index)
        {
            raw->setValueAsCompleteGesture ((float) index);
        };
        attachment->sendInitialUpdate();
        choiceAttachments.push_back (std::move (attachment));
    }

    for (auto* caption : { &quantizeCaption, &triggerCaption })
    {
        caption->setFont (SuiteUI::uiFont (13.0f, true));
        caption->setColour (juce::Label::textColourId, SuiteUI::Colours::textDim);
        caption->setJustificationType (juce::Justification::centred);
    }
    quantizeCaption.setText ("QUANTIZE", juce::dontSendNotification);
    triggerCaption.setText ("TRIGGER", juce::dontSendNotification);

    beatReadout.setFont (SuiteUI::uiFont (15.0f, true));
    beatReadout.setColour (juce::Label::textColourId, SuiteUI::Colours::text);
    beatReadout.setJustificationType (juce::Justification::centred);
    beatReadout.setText ("0.00", juce::dontSendNotification);

    SuiteUI::describe (masterCtrl, "Output level of the whole instrument.");
    SuiteUI::describe (panCtrl, "Constant-power pan: +100 % pans right, -100 % pans left.");
    SuiteUI::describe (pitchCtrl, "Detunes all four slots together (speed and pitch move, like tape).");
    SuiteUI::describe (crossfadeCtrl, "The loop-seam blend, shared by all slots.");
    SuiteUI::describe (swingCtrl, "Shifts odd grid points. Positive is late.");
    SuiteUI::describe (quantizeSelector, "Triggers start on the next grid point.");
    SuiteUI::describe (triggerSelector, "Latch: a note runs until retriggered. Gate: only while held.");
    SuiteUI::describe (beatReadout, "The beat clock's position, for reference.");

    // The header contract: preset bar (R12), page selector (R8), menu (R13).
    if (presetBar->getPreferredHeight() > 0)
        addAndMakeVisible (*presetBar);
    presetBar->onPresetLoaded = [this] { repaint(); };

    pageSelector.setOptions ({ "SLOTS", "GLOBAL" });
    pageSelector.onChange = [this] (int index) { setPage (index); };
    addAndMakeVisible (pageSelector);

    addAndMakeVisible (headerMenu);
    headerMenu.onAbout = [this]
    {
        SuiteUI::showAboutDialog (brandLockup(), SUITE_PLUGIN_NAME, SUITE_PLUGIN_VERSION,
                                  SUITE_PLUGIN_CODE, SUITE_PLUGIN_ID, this);
    };
    headerMenu.onSettings = [this]
    {
        SuiteSettings::showSettingsDialog (SuiteSettings::shared(), this);
    };

    setPage (0);
    updateAffinity();
    startTimerHz (SuiteUI::kRefreshHz);
}

CyclicalEditor::~CyclicalEditor()
{
    stopTimer();
    removeMouseListener (&helpTracker);
    setLookAndFeel (nullptr);
}

void CyclicalEditor::setPage (int newPage)
{
    page = juce::jlimit (0, 1, newPage);
    pageSelector.setSelectedIndex (page);

    for (auto& card : cards)
        card->setVisible (page == 0);

    for (auto* component : { static_cast<juce::Component*> (&masterCtrl), static_cast<juce::Component*> (&panCtrl),
                             static_cast<juce::Component*> (&pitchCtrl), static_cast<juce::Component*> (&crossfadeCtrl),
                             static_cast<juce::Component*> (&swingCtrl), static_cast<juce::Component*> (&quantizeCaption),
                             static_cast<juce::Component*> (&quantizeSelector), static_cast<juce::Component*> (&triggerCaption),
                             static_cast<juce::Component*> (&triggerSelector), static_cast<juce::Component*> (&beatReadout) })
        component->setVisible (page == 1);

    resized();
    repaint();
}

void CyclicalEditor::browseForLoop (int slot)
{
    chooserSlot = slot;
    loopChooser = std::make_unique<juce::FileChooser> ("Choose a loop for slot "
                                                           + juce::String (slot + 1),
                                                       CyclicalProcessor::defaultUserLoopFolder(),
                                                       "*.wav;*.aiff;*.aif");

    loopChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser)
        {
            const auto result = chooser.getResult();

            if (result.existsAsFile())
                processor.loadLoopFromFileAsync (chooserSlot, result, result.getFullPathName());

            // The chooser stays alive until this callback has run.
            loopChooser.reset();
        });
}

void CyclicalEditor::updateAffinity()
{
    // Plan 10 G1: controls that only work in one mode are dimmed, never hidden.
    for (int slot = 0; slot < Processor::numSlots; ++slot)
    {
        auto& card = *cards[(size_t) slot];

        const bool grainOn = processor.apvts.getRawParameterValue (Processor::slotGranularIds[(size_t) slot])->load() > 0.5f;
        const bool pingPong = processor.apvts.getRawParameterValue (Processor::slotDirectionIds[(size_t) slot])->load() > 1.5f;

        card.speed.setEnabled (! grainOn);
        card.pingRand.setEnabled (pingPong);
    }
}

void CyclicalEditor::timerCallback()
{
    updateAffinity();

    for (int slot = 0; slot < Processor::numSlots; ++slot)
    {
        auto& card = *cards[(size_t) slot];
        card.waveform.setLoop (processor.loopForUi (slot));
        card.waveform.setRegion (processor.apvts.getRawParameterValue (Processor::slotStartIds[(size_t) slot])->load(),
                                 processor.apvts.getRawParameterValue (Processor::slotLengthIds[(size_t) slot])->load());
        card.waveform.setPlayhead (processor.slotPlayhead (slot), processor.slotIsActive (slot));
        card.waveform.setMissing (processor.isLoopMissing (slot));
    }

    beatReadout.setText (juce::String (processor.beatPositionUi(), 2) + " beats",
                         juce::dontSendNotification);
}

void CyclicalEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (SuiteUI::Colours::panelLight, 0.0f, 0.0f,
                                     SuiteUI::Colours::background, 0.0f, (float) getHeight(), false);
    g.setGradientFill (background);
    g.fillAll();

    SuiteUI::drawBrandLockup (g, getLocalBounds().removeFromTop (headerHeight).reduced (16, 0), brandLockup());

    // The four slot cards are their own frames; the GLOBAL page draws one group.
    if (page == 1)
    {
        const auto frame = globalGroupBounds.toFloat();

        g.setColour (SuiteUI::Colours::panel.withAlpha (0.55f));
        g.fillRoundedRectangle (frame, 8.0f);
        g.setColour (SuiteUI::Colours::track);
        g.drawRoundedRectangle (frame, 8.0f, 1.0f);

        g.setColour (lookAndFeel.getAccent());
        g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
        g.drawText ("GLOBAL", globalGroupBounds.withHeight (groupTitleHeight).translated (0, 4).reduced (12, 0),
                    juce::Justification::centredLeft);
    }

    // The cards' frames, on the SLOTS page only (their bounds are stale on GLOBAL).
    if (page == 0)
    {
        for (auto& card : cards)
        {
            const auto frame = card->getBounds().toFloat();

            g.setColour (SuiteUI::Colours::panel.withAlpha (0.35f));
            g.fillRoundedRectangle (frame, 8.0f);
            g.setColour (SuiteUI::Colours::track);
            g.drawRoundedRectangle (frame, 8.0f, 1.0f);
        }
    }
}

void CyclicalEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    auto header = area.removeFromTop (headerHeight);

    // The menu button sits at the far right, right of the preset bar (plan 08 Q4).
    headerMenu.setBounds (header.removeFromRight (26).withSizeKeepingCentre (26, 26));

    if (presetBar->getPreferredHeight() > 0)
        presetBar->setBounds (header.removeFromRight (300).withHeight (30)
                                .withY (header.getCentreY() - 15));

    // The page switch at the top right of the header (R8), left of the bar.
    pageSelector.setBounds (header.removeFromRight (170).withHeight (26)
                              .withY (header.getCentreY() - 13));

    area.removeFromTop (groupGap);

    if (page == 0)
    {
        // Two columns of two cards.
        const int cardWidth = area.getWidth() / 2;
        const int cardHeight = area.getHeight() / 2;

        for (int slot = 0; slot < Processor::numSlots; ++slot)
        {
            const int column = slot % 2;
            const int row = slot / 2;
            cards[(size_t) slot]->setBounds (juce::Rectangle<int> (area.getX() + column * cardWidth,
                                                                    area.getY() + row * cardHeight,
                                                                    cardWidth, cardHeight).reduced (groupGap / 2, 2));
        }
    }
    else
    {
        globalGroupBounds = area;
        auto inner = area.withTrimmedTop (groupTitleHeight + 4).reduced (12, 6);

        const int cellWidth = inner.getWidth() / 4;
        auto row1 = inner.removeFromTop (SuiteUI::knobRowHeight);
        masterCtrl.setBounds (row1.removeFromLeft (cellWidth).reduced (3));
        panCtrl.setBounds (row1.removeFromLeft (cellWidth).reduced (3));
        pitchCtrl.setBounds (row1.removeFromLeft (cellWidth).reduced (3));
        crossfadeCtrl.setBounds (row1.removeFromLeft (cellWidth).reduced (3));

        inner.removeFromTop (groupGap);
        auto row2 = inner.removeFromTop (SuiteUI::knobRowHeight);   // one knob row, not the whole rest
        swingCtrl.setBounds (row2.removeFromLeft (cellWidth).reduced (3));

        auto quantizeCell = row2.removeFromLeft (cellWidth).reduced (6, 4);
        quantizeCaption.setBounds (quantizeCell.removeFromTop (18));
        quantizeSelector.setBounds (quantizeCell.withHeight (SuiteUI::pillHeight).withY (quantizeCell.getY()));

        auto triggerCell = row2.removeFromLeft (cellWidth).reduced (6, 4);
        triggerCaption.setBounds (triggerCell.removeFromTop (18));
        triggerSelector.setBounds (triggerCell.withHeight (SuiteUI::pillHeight).withY (triggerCell.getY()));

        beatReadout.setBounds (row2.reduced (6, 4));
    }

    // The help line lives in the band the paint used to draw its footer sentence in.
    helpLine.setBounds (getLocalBounds().removeFromBottom (footerHeight));
}
