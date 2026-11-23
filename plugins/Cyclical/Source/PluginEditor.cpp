#include "PluginEditor.h"

#include "BinaryData.h"
#include "SuiteBrand.h"
#include "SuiteSettings.h"

namespace
{
    // Plan 05 R4: the window is per plugin and owner-approved (AGENTS R4). Plan 18 Phase 4
    // (owner decision): one tab per slot — the former GLOBAL controls are per slot and
    // live on the card, so each page carries that slot's complete set. The height is
    // derived from the layout: title + waveform + six knob rows + the pill row + the
    // master Output row.
    constexpr int headerHeight = 44;
    constexpr int footerHeight = 26;
    constexpr int groupGap = 8;   // the suite's one-gap rule (plan 05 R7)
    constexpr int groupTitleHeight = SuiteUI::groupTitleHeight;
    constexpr int waveHeight = 110;
    constexpr int pillRowHeight = SuiteUI::pillHeight + 8;
    constexpr int knobRows = 6;
    constexpr int cardContentHeight = 20 + 2 + waveHeight + groupGap
                                    + knobRows * SuiteUI::knobRowHeight
                                    + (knobRows - 1) * groupGap
                                    + pillRowHeight;
    constexpr int masterRowHeight = SuiteUI::knobRowHeight;
    constexpr int editorWidth = 1000;
    constexpr int editorHeight = 16 + headerHeight + groupGap
                               + cardContentHeight + groupGap
                               + masterRowHeight
                               + footerHeight + groupGap;

    const juce::Image& brandLockup()
    {
        static const juce::Image image = juce::ImageCache::getFromMemory (BinaryData::lockup_cyclical_png,
                                                                          BinaryData::lockup_cyclical_pngSize);
        return image;
    }

    using Processor = CyclicalProcessor;
}

//==============================================================================
CyclicalEditor::SlotCard::SlotCard (int slotIndex, juce::Colour accentColour)
    : accentColour (accentColour),
      slot (slotIndex),
      waveform (accentColour)
{
    const int initialNote = CyclicalProcessor::firstSlotNote + slot;
    titleNote = initialNote;
    title.setText ("SLOT " + juce::String (slot + 1) + "  -  "
                       + juce::MidiMessage::getMidiNoteName (initialNote, true, false, 4),
                   juce::dontSendNotification);
    title.setFont (SuiteUI::uiFont (15.0f, true));
    title.setColour (juce::Label::textColourId, accentColour);
    addAndMakeVisible (title);

    addAndMakeVisible (waveform);
    addAndMakeVisible (start);
    addAndMakeVisible (length);
    addAndMakeVisible (speed);
    addAndMakeVisible (velocity);
    addAndMakeVisible (note);
    addAndMakeVisible (beats);
    addAndMakeVisible (slices);
    addAndMakeVisible (sliceLength);

    // Plan 17 Phase 2 + plan 18 §1.1: per-slot creative controls with cutoff and resonance.
    filter.addItem ("Off", 1);
    filter.addItem ("High Pass", 2);
    filter.addItem ("Low Pass", 3);
    filter.addItem ("Band Pass", 4);
    filter.setSelectedItemIndex (0, juce::dontSendNotification);
    addAndMakeVisible (filter);

    cutoff.slider.setTextValueSuffix (" Hz");
    cutoff.slider.setNumDecimalPlacesToDisplay (0);
    reso.slider.setNumDecimalPlacesToDisplay (2);
    attack.slider.setTextValueSuffix (" ms");
    attack.slider.setNumDecimalPlacesToDisplay (1);
    release.slider.setTextValueSuffix (" ms");
    release.slider.setNumDecimalPlacesToDisplay (1);
    level.slider.setTextValueSuffix (" %");
    level.slider.setNumDecimalPlacesToDisplay (1);
    addAndMakeVisible (cutoff);
    addAndMakeVisible (reso);
    addAndMakeVisible (attack);
    addAndMakeVisible (release);
    addAndMakeVisible (level);

    // Plan 18 Phase 4: the slot's own transport and mix controls (the former GLOBALs).
    panCtrl.slider.setTextValueSuffix (" %");
    panCtrl.slider.setNumDecimalPlacesToDisplay (1);
    pitchCtrl.slider.setNumDecimalPlacesToDisplay (0);
    swingCtrl.slider.setTextValueSuffix (" %");
    swingCtrl.slider.setNumDecimalPlacesToDisplay (1);
    tempoCtrl.slider.setTextValueSuffix (" BPM");
    tempoCtrl.slider.setNumDecimalPlacesToDisplay (1);
    crossfadeCtrl.slider.setTextValueSuffix (" ms");
    crossfadeCtrl.slider.setNumDecimalPlacesToDisplay (0);
    addAndMakeVisible (panCtrl);
    addAndMakeVisible (pitchCtrl);
    addAndMakeVisible (swingCtrl);
    addAndMakeVisible (tempoCtrl);
    addAndMakeVisible (crossfadeCtrl);
    addAndMakeVisible (syncToggle);

    quantizeSelector.setOptions ({ "OFF", "1/16", "1/8", "1/4", "1/2", "BAR" });
    triggerSelector.setOptions ({ "LATCH", "GATE", "TOGGLE" });
    for (auto* caption : { &quantizeCaption, &triggerCaption })
    {
        caption->setFont (SuiteUI::uiFont (13.0f, true));
        caption->setColour (juce::Label::textColourId, SuiteUI::Colours::textDim);
        caption->setJustificationType (juce::Justification::centred);
    }
    quantizeCaption.setText ("QUANTIZE", juce::dontSendNotification);
    triggerCaption.setText ("TRIGGER", juce::dontSendNotification);
    addAndMakeVisible (quantizeCaption);
    addAndMakeVisible (quantizeSelector);
    addAndMakeVisible (triggerCaption);
    addAndMakeVisible (triggerSelector);

    direction.setOptions ({ "FWD", "REV", "PP" });
    addAndMakeVisible (direction);

    addAndMakeVisible (pingRand);
    addAndMakeVisible (granular);
    addAndMakeVisible (chop);
    syncToggle.setClickingTogglesState (true);

    start.slider.setTextValueSuffix (" %");
    start.slider.setNumDecimalPlacesToDisplay (1);
    length.slider.setTextValueSuffix (" %");
    length.slider.setNumDecimalPlacesToDisplay (1);
    speed.slider.setNumDecimalPlacesToDisplay (2);   // the "x" unit comes from the parameter
    velocity.slider.setTextValueSuffix (" %");
    velocity.slider.setNumDecimalPlacesToDisplay (0);
    note.slider.setNumDecimalPlacesToDisplay (0);
    beats.slider.setNumDecimalPlacesToDisplay (0);
    slices.slider.setNumDecimalPlacesToDisplay (0);
    sliceLength.slider.setTextValueSuffix (" %");
    sliceLength.slider.setNumDecimalPlacesToDisplay (1);

    for (auto* pill : { &pingRand, &granular, &chop, &syncToggle })
        pill->setClickingTogglesState (true);
}

void CyclicalEditor::SlotCard::setTriggerNote (int note)
{
    if (note == titleNote)
        return;

    titleNote = note;
    title.setText ("SLOT " + juce::String (slot + 1) + "  -  "
                       + juce::MidiMessage::getMidiNoteName (note, true, false, 4),
                   juce::dontSendNotification);
}

void CyclicalEditor::SlotCard::resized()
{
    auto area = getLocalBounds();
    title.setBounds (area.removeFromTop (20));
    area.removeFromTop (2);

    waveform.setBounds (area.removeFromTop (waveHeight));
    area.removeFromTop (groupGap);

    // Six knob rows at the suite size (R1/R2), four cells each: playback, then the
    // creative controls, then the slot's own transport and mix (plan 18 Phase 4).
    const int knobWidth = area.getWidth() / 4;
    auto row = [&area, knobWidth] (int index)
    {
        if (index > 0)
            area.removeFromTop (groupGap);
        return area.removeFromTop (SuiteUI::knobRowHeight);
    };

    auto row1 = row (0);
    start.setBounds (row1.removeFromLeft (knobWidth).reduced (3));
    length.setBounds (row1.removeFromLeft (knobWidth).reduced (3));
    speed.setBounds (row1.removeFromLeft (knobWidth).reduced (3));
    velocity.setBounds (row1.removeFromLeft (knobWidth).reduced (3));

    auto row2 = row (1);
    note.setBounds (row2.removeFromLeft (knobWidth).reduced (3));
    beats.setBounds (row2.removeFromLeft (knobWidth).reduced (3));
    slices.setBounds (row2.removeFromLeft (knobWidth).reduced (3));
    sliceLength.setBounds (row2.removeFromLeft (knobWidth).reduced (3));

    auto row3 = row (2);   // creative: filter type, cutoff, resonance, level
    filter.setBounds (row3.removeFromLeft (knobWidth).reduced (3));
    cutoff.setBounds (row3.removeFromLeft (knobWidth).reduced (3));
    reso.setBounds (row3.removeFromLeft (knobWidth).reduced (3));
    level.setBounds (row3.removeFromLeft (knobWidth).reduced (3));

    auto row4 = row (3);   // envelope tail and the slot's image: attack, release, pan, pitch
    attack.setBounds (row4.removeFromLeft (knobWidth).reduced (3));
    release.setBounds (row4.removeFromLeft (knobWidth).reduced (3));
    panCtrl.setBounds (row4.removeFromLeft (knobWidth).reduced (3));
    pitchCtrl.setBounds (row4.removeFromLeft (knobWidth).reduced (3));

    auto row5 = row (4);   // the slot's clock: swing, tempo, crossfade, quantise
    swingCtrl.setBounds (row5.removeFromLeft (knobWidth).reduced (3));
    tempoCtrl.setBounds (row5.removeFromLeft (knobWidth).reduced (3));
    crossfadeCtrl.setBounds (row5.removeFromLeft (knobWidth).reduced (3));

    auto quantizeCell = row5.removeFromLeft (knobWidth).reduced (6, 4);
    quantizeCaption.setBounds (quantizeCell.removeFromTop (18));
    quantizeSelector.setBounds (quantizeCell.withHeight (SuiteUI::pillHeight).withY (quantizeCell.getY()));

    auto row6 = row (5);   // trigger selector, host sync and the two spare cells
    auto triggerCell = row6.removeFromLeft (knobWidth).reduced (6, 4);
    triggerCaption.setBounds (triggerCell.removeFromTop (18));
    triggerSelector.setBounds (triggerCell.withHeight (SuiteUI::pillHeight).withY (triggerCell.getY()));

    auto syncCell = row6.removeFromLeft (knobWidth).reduced (6, 4);
    syncToggle.setBounds (syncCell.withSizeKeepingCentre (syncCell.getWidth() - 12, SuiteUI::pillHeight));

    // Direction selector and the four switches (pills at 36 px, R3). The widths sum to
    // the card's inner width; Ping Rand keeps the wider pill for its caption.
    auto pillRow = area.removeFromTop (pillRowHeight);
    direction.setBounds (pillRow.removeFromLeft (132).reduced (6, 2)
                             .withHeight (SuiteUI::pillHeight)
                             .withY (pillRow.getY() + (pillRow.getHeight() - SuiteUI::pillHeight) / 2));
    pingRand.setBounds (pillRow.removeFromLeft (148).reduced (4, 2)
                            .withSizeKeepingCentre (140, SuiteUI::pillHeight));

    auto rest = pillRow;
    granular.setBounds (rest.removeFromLeft (96).reduced (4, 2)
                            .withSizeKeepingCentre (88, SuiteUI::pillHeight));
    chop.setBounds (rest.reduced (4, 2)
                        .withSizeKeepingCentre (juce::jmin (88, rest.getWidth() - 6), SuiteUI::pillHeight));
}

//==============================================================================
CyclicalEditor::CyclicalEditor (CyclicalProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p)
{
    setLookAndFeel (&lookAndFeel);

    // The bar exists before setSize(), because setSize() runs resized(), which lays it out.
    presetBar.reset (new SuitePresets::PresetBar (processor, juce::Colour (0xffFF7A45)));

    // The cards exist before setSize() too: resized() lays the page out.
    auto& apvts = processor.apvts;

    auto bind = [this, &apvts] (const char* id, juce::Slider& slider)
    {
        sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider));
    };

    for (int slot = 0; slot < Processor::numSlots; ++slot)
    {
        cards[(size_t) slot] = std::make_unique<SlotCard> (slot, juce::Colour (0xffFF7A45));
        auto& card = *cards[(size_t) slot];
        addAndMakeVisible (card);

        bind (Processor::slotStartIds[(size_t) slot], card.start.slider);
        bind (Processor::slotLengthIds[(size_t) slot], card.length.slider);
        bind (Processor::slotSpeedIds[(size_t) slot], card.speed.slider);
        bind (Processor::slotVelocityIds[(size_t) slot], card.velocity.slider);
        bind (Processor::slotNoteIds[(size_t) slot], card.note.slider);
        bind (Processor::slotBeatsIds[(size_t) slot], card.beats.slider);
        bind (Processor::slotSlicesIds[(size_t) slot], card.slices.slider);
        bind (Processor::slotSliceLengthIds[(size_t) slot], card.sliceLength.slider);
        /// Plan 17 Phase 2: filter is a ComboBox, not a Slider — use ParameterAttachment directly.
        choiceAttachments.push_back (std::make_unique<juce::ParameterAttachment> (
            *apvts.getParameter (Processor::slotFilterIds[(size_t) slot]),
            [&card] (float value) { card.filter.setSelectedItemIndex (juce::roundToInt (value), juce::dontSendNotification); }));

        bind (Processor::slotAttackIds[(size_t) slot], card.attack.slider);
        bind (Processor::slotReleaseIds[(size_t) slot], card.release.slider);
        bind (Processor::slotLevelIds[(size_t) slot], card.level.slider);
        bind (Processor::slotCutoffIds[(size_t) slot], card.cutoff.slider);
        bind (Processor::slotResoIds[(size_t) slot], card.reso.slider);

        // Plan 18 Phase 4: the slot's own transport and mix controls (the former globals).
        bind (Processor::slotPanIds[(size_t) slot], card.panCtrl.slider);
        bind (Processor::slotPitchIds[(size_t) slot], card.pitchCtrl.slider);
        bind (Processor::slotSwingIds[(size_t) slot], card.swingCtrl.slider);
        bind (Processor::slotTempoIds[(size_t) slot], card.tempoCtrl.slider);
        bind (Processor::slotCrossfadeIds[(size_t) slot], card.crossfadeCtrl.slider);
        buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            apvts, Processor::slotSyncIds[(size_t) slot], card.syncToggle));

        // The per-slot quantise and trigger selectors (the ParameterAttachment pattern).
        {
            auto quantizeAttachment = std::make_unique<juce::ParameterAttachment> (
                *apvts.getParameter (Processor::slotQuantizeIds[(size_t) slot]),
                [&card] (float value) { card.quantizeSelector.setSelectedIndex (juce::roundToInt (value)); });
            card.quantizeSelector.onChange = [raw = quantizeAttachment.get()] (int index)
            {
                raw->setValueAsCompleteGesture ((float) index);
            };
            quantizeAttachment->sendInitialUpdate();
            choiceAttachments.push_back (std::move (quantizeAttachment));
        }
        {
            auto triggerAttachment = std::make_unique<juce::ParameterAttachment> (
                *apvts.getParameter (Processor::slotTriggerIds[(size_t) slot]),
                [&card] (float value) { card.triggerSelector.setSelectedIndex (juce::roundToInt (value)); });
            card.triggerSelector.onChange = [raw = triggerAttachment.get()] (int index)
            {
                raw->setValueAsCompleteGesture ((float) index);
            };
            triggerAttachment->sendInitialUpdate();
            choiceAttachments.push_back (std::move (triggerAttachment));
        }

        // A choice parameter through a ParameterAttachment: the denormalised value is the
        // choice index as a float (the Micro-Timing Chaos rate selector pattern). The raw
        // pointer is captured, so no index arithmetic breaks when other attachments join.
        auto directionAttachment = std::make_unique<juce::ParameterAttachment> (
            *apvts.getParameter (Processor::slotDirectionIds[(size_t) slot]),
            [&card] (float value) { card.direction.setSelectedIndex (juce::roundToInt (value)); });
        card.direction.onChange = [raw = directionAttachment.get()] (int index)
        {
            raw->setValueAsCompleteGesture ((float) index);
        };
        directionAttachment->sendInitialUpdate();
        choiceAttachments.push_back (std::move (directionAttachment));

        buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            apvts, Processor::slotPingRandomIds[(size_t) slot], card.pingRand));
        buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            apvts, Processor::slotGranularIds[(size_t) slot], card.granular));
        buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            apvts, Processor::slotChopIds[(size_t) slot], card.chop));

        card.waveform.onLoadClicked = [this, slot] { browseForLoop (slot); };
        card.waveform.onAutoClicked = [this, slot] { processor.autoChopSlot (slot); };
        card.waveform.onOrderCellClicked = [this, slot] (int index) { processor.cycleSliceOrder (slot, index); };
        card.waveform.onOrderCellSelected = [this, slot] (int index) { processor.selectChopSlice (slot, index); };
        card.waveform.onReverseClicked = [this, slot] { processor.loopToolReverse (slot); };
        card.waveform.onNormaliseClicked = [this, slot] { processor.loopToolNormalise (slot); };
        card.waveform.onTrimClicked = [this, slot] { processor.loopToolTrim (slot); };

        // Plan 18 §1.4 (owner answer): the region knobs apply live — a drag that moved
        // the value retriggers the slot's voice on release, so Start/Length/Slices/Slice
        // Length/Beats are heard immediately.
        for (auto* slider : { &card.start.slider, &card.length.slider, &card.slices.slider,
                              &card.sliceLength.slider, &card.beats.slider })
        {
            regionRetriggerListeners.push_back (std::make_unique<RegionRetriggerListener> (*this, slot));
            slider->addListener (regionRetriggerListeners.back().get());
        }

        SuiteUI::describe (card.start, "Where the loop starts, as a share of its length (capped at 95 %).");
        SuiteUI::describe (card.length, "How much of the loop plays, as a share of its length.");
        SuiteUI::describe (card.speed, "Playback speed. Inert while Grain is on, and while Host Sync is on (the loop then plays in exactly Beats beats).");
        SuiteUI::describe (card.velocity, "How strongly the slot's key velocity scales the level.");
        SuiteUI::describe (card.note, "The MIDI key that triggers the slot. Chopped slots add one key per slice above it.");
        SuiteUI::describe (card.beats, "The musical length of the whole loop, in beats. Used while Host Sync is on; dimmed in Chop mode (sync is disabled there).");
        SuiteUI::describe (card.slices, "How many slices Chop divides the region into. Only in Chop mode.");
        SuiteUI::describe (card.sliceLength, "Skews the slice boundaries: positive lengthens the late slices, negative the early ones. 0 % is the equal split.");
        SuiteUI::describe (card.chop, "Chop mode: the slot's region splits into Slices, one key each (Dr. Rex style). Sync is disabled for a chopped slot.");
        SuiteUI::describe (card.direction, "Forward, Reverse, or Ping-Pong (turns at both bounds).");
        SuiteUI::describe (card.pingRand, "Randomises the ping-pong turn points per trigger. Only in Ping-Pong.");
        SuiteUI::describe (card.granular, "80 ms grains instead of straight playback. Speed is inert.");
        SuiteUI::describe (card.filter, "The slot's filter type. Off is a true bypass.");
        SuiteUI::describe (card.cutoff, "The slot filter's corner frequency.");
        SuiteUI::describe (card.reso, "The slot filter's resonance.");
        SuiteUI::describe (card.attack, "How fast the slot's level rises on a trigger.");
        SuiteUI::describe (card.release, "How fast the slot's level falls on stop or retrigger.");
        SuiteUI::describe (card.level, "The slot's output level before the master stage.");
        SuiteUI::describe (card.panCtrl, "The slot's own constant-power pan before the master sum.");
        SuiteUI::describe (card.pitchCtrl, "Detunes this slot (speed and pitch move, like tape).");
        SuiteUI::describe (card.swingCtrl, "Shifts this slot's odd grid points. Positive is late.");
        SuiteUI::describe (card.tempoCtrl, "This slot's clock tempo while its Host Sync is off.");
        SuiteUI::describe (card.crossfadeCtrl, "This slot's loop-seam blend.");
        SuiteUI::describe (card.quantizeSelector, "This slot's triggers start on the next grid point.");
        SuiteUI::describe (card.triggerSelector, "Latch: a note runs until retriggered. Gate: only while held. Toggle: the first press starts the loop, the second stops it.");
        SuiteUI::describe (card.syncToggle, "This slot follows the host's position and tempo. Chopped slots ignore it.");
        SuiteUI::describe (card.waveform, "The loaded loop, its region band, the chop lines, the arrangement strip and the playhead. LOAD opens a browser; AUTO slices at the loop's transients; the strip's cells rearrange the slices (click) and select one for the tools (right click); REV reverses the loop or the selected slice, NORM matches the peak, TRIM aligns the region to the content.");
    }

    SuiteUI::describe (pageSelector, "One tab per slot: the card carries that slot's complete control set, and the former GLOBAL controls work independently per slot.");
    SuiteUI::describe (*presetBar, "Factory presets, INIT and SAVE. The menu lists user presets.");
    SuiteUI::describe (headerMenu, "Settings... and About...");

    setSize (editorWidth, editorHeight);
    setResizable (true, true);
    setResizeLimits (editorWidth - 40, editorHeight - 40, 1200, 1200);

    // The shared help line (plan 12): one line, one idle text, no stale strings. The
    // footer sentence is the idle text, so the band reads the same before a hover.
    helpLine.setIdleText ("Each slot's Note key triggers it; chopped slots add one key per slice above the base note. Every slot runs its own grid, swing, trigger and clock. No recording: loops load from the library or disk.");
    helpLine.setComponentID ("cyclical-help");
    helpLine.setEnablePredicate ([] { return SuiteSettings::shared().getBool (SuiteSettings::tooltipsKey, true); });
    SuiteUI::attachHelpLine (*this, helpLine, helpTracker);
    addAndMakeVisible (helpLine);

    // The one global (plan 18 Phase 4): master Output, shown on every slot page.
    addAndMakeVisible (masterCtrl);
    masterCtrl.slider.setTextValueSuffix (" dB");
    masterCtrl.slider.setNumDecimalPlacesToDisplay (1);
    bind (Processor::masterVolumeId, masterCtrl.slider);
    SuiteUI::describe (masterCtrl, "Output level of the whole instrument.");

    // Plan 18 Phase 6: the MOD page (the panel is built as a member; the display wires
    // the per-slot knobs to their routing targets).
    addAndMakeVisible (modulation);
    controls = std::make_unique<SuiteModulation::ControlDisplay> (processor.getRouter(),
        std::vector<SuiteModulation::ControlDisplay::Entry> {
            { cards[0]->level.slider,  (int) CyclicalProcessor::ModTarget::Level1 },
            { cards[1]->level.slider,  (int) CyclicalProcessor::ModTarget::Level2 },
            { cards[2]->level.slider,  (int) CyclicalProcessor::ModTarget::Level3 },
            { cards[3]->level.slider,  (int) CyclicalProcessor::ModTarget::Level4 },
            { cards[0]->cutoff.slider, (int) CyclicalProcessor::ModTarget::Cutoff1 },
            { cards[1]->cutoff.slider, (int) CyclicalProcessor::ModTarget::Cutoff2 },
            { cards[2]->cutoff.slider, (int) CyclicalProcessor::ModTarget::Cutoff3 },
            { cards[3]->cutoff.slider, (int) CyclicalProcessor::ModTarget::Cutoff4 },
            { cards[0]->speed.slider,  (int) CyclicalProcessor::ModTarget::Speed1 },
            { cards[1]->speed.slider,  (int) CyclicalProcessor::ModTarget::Speed2 },
            { cards[2]->speed.slider,  (int) CyclicalProcessor::ModTarget::Speed3 },
            { cards[3]->speed.slider,  (int) CyclicalProcessor::ModTarget::Speed4 },
            { cards[0]->panCtrl.slider, (int) CyclicalProcessor::ModTarget::Pan1 },
            { cards[1]->panCtrl.slider, (int) CyclicalProcessor::ModTarget::Pan2 },
            { cards[2]->panCtrl.slider, (int) CyclicalProcessor::ModTarget::Pan3 },
            { cards[3]->panCtrl.slider, (int) CyclicalProcessor::ModTarget::Pan4 },
        });
    SuiteUI::describe (modulation, "Two LFOs (with the waveform transforms) and six routing slots over the per-slot Level, Cutoff, Speed and Pan controls.");

    // The header contract: preset bar (R12), page selector (R8), menu (R13).
    if (presetBar->getPreferredHeight() > 0)
        addAndMakeVisible (*presetBar);
    presetBar->onPresetLoaded = [this] { repaint(); };

    pageSelector.setOptions ({ "SLOT 1", "SLOT 2", "SLOT 3", "SLOT 4", "MOD" });
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
    page = juce::jlimit (0, Processor::numSlots, newPage);   // numSlots = the MOD page
    pageSelector.setSelectedIndex (page);

    const bool modPage = page == Processor::numSlots;
    for (int slot = 0; slot < Processor::numSlots; ++slot)
        cards[(size_t) slot]->setVisible (! modPage && slot == page);

    // The one global sits on every slot page; the MOD page is the panel alone.
    masterCtrl.setVisible (! modPage);
    modulation.setVisible (modPage);

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
    // Plan 14 adds three rules, tested in every combination: Beats is dimmed while the
    // slot is chopped (sync is disabled there), Slices and Slice Length are dimmed while
    // chop is off, and Speed is dimmed while THAT slot's Host Sync is on - unless the slot
    // is chopped (a chopped slot ignores sync, so its Speed stays live). Plan 18: sync is
    // a per-slot parameter, so each slot's rule reads its own.
    for (int slot = 0; slot < Processor::numSlots; ++slot)
    {
        auto& card = *cards[(size_t) slot];

        const bool synced = processor.apvts.getRawParameterValue (Processor::slotSyncIds[(size_t) slot])->load() > 0.5f;
        const bool grainOn = processor.apvts.getRawParameterValue (Processor::slotGranularIds[(size_t) slot])->load() > 0.5f;
        const bool pingPong = processor.apvts.getRawParameterValue (Processor::slotDirectionIds[(size_t) slot])->load() > 1.5f;
        const bool chopped = processor.apvts.getRawParameterValue (Processor::slotChopIds[(size_t) slot])->load() > 0.5f;

        card.speed.setEnabled (! grainOn && (! synced || chopped));
        card.pingRand.setEnabled (pingPong);
        card.beats.setEnabled (! chopped);
        card.slices.setEnabled (chopped);
        card.sliceLength.setEnabled (chopped);
        card.waveform.setAutoEnabled (chopped);   // plan 16: AUTO slices only in chop mode
    }
}

void CyclicalEditor::timerCallback()
{
    updateAffinity();
    modulation.refresh();
    controls->update();

    for (int slot = 0; slot < Processor::numSlots; ++slot)
    {
        auto& card = *cards[(size_t) slot];
        card.setTriggerNote (processor.slotNote (slot));
        card.waveform.setLoop (processor.loopForUi (slot));
        card.waveform.setRegion (processor.apvts.getRawParameterValue (Processor::slotStartIds[(size_t) slot])->load(),
                                 processor.apvts.getRawParameterValue (Processor::slotLengthIds[(size_t) slot])->load());

        // The chop lines come from the same function the processor plays (plan 14 Phase 5,
        // plan 16: the auto-chop property when its count matches Slices). The arrangement
        // strip shows the slot's slice order (identity when the property is absent).
        if (processor.slotChopped (slot))
        {
            std::vector<float> shares;
            const int slices = processor.slotSlices (slot);
            const auto bounds = processor.activeChopBounds (slot);

            for (int i = 1; i < slices; ++i)   // the outer boundaries are the region itself
                if (i - 1 < (int) bounds.size())
                    shares.push_back (bounds[(size_t) (i - 1)]);

            card.waveform.setChopLines (shares);
            card.waveform.setSliceOrder (processor.chopOrder (slot), slices, processor.selectedChopSlice (slot));
        }
        else
        {
            card.waveform.setChopLines ({});
            card.waveform.setSliceOrder ({}, 0, -1);
        }

        card.waveform.setPlayhead (processor.slotPlayhead (slot), processor.slotIsActive (slot));
        card.waveform.setMissing (processor.isLoopMissing (slot));
    }
}

void CyclicalEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (SuiteUI::Colours::panelLight, 0.0f, 0.0f,
                                     SuiteUI::Colours::background, 0.0f, (float) getHeight(), false);
    g.setGradientFill (background);
    g.fillAll();

    SuiteUI::drawBrandLockup (g, getLocalBounds().removeFromTop (headerHeight).reduced (16, 0), brandLockup());

    // The MOD page draws one group frame (plan 18 Phase 6); each slot's card is its own
    // frame and only the visible page draws.
    if (page == Processor::numSlots)
    {
        const auto frame = modulationGroupBounds.toFloat();

        g.setColour (SuiteUI::Colours::panel.withAlpha (0.55f));
        g.fillRoundedRectangle (frame, 8.0f);
        g.setColour (SuiteUI::Colours::track);
        g.drawRoundedRectangle (frame, 8.0f, 1.0f);

        g.setColour (lookAndFeel.getAccent());
        g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
        g.drawText ("MODULATION", modulationGroupBounds.withHeight (groupTitleHeight).translated (0, 4).reduced (12, 0),
                    juce::Justification::centredLeft);
    }

    for (int slot = 0; slot < Processor::numSlots; ++slot)
    {
        if (! cards[(size_t) slot]->isVisible())
            continue;

        const auto frame = cards[(size_t) slot]->getBounds().toFloat();

        g.setColour (SuiteUI::Colours::panel.withAlpha (0.35f));
        g.fillRoundedRectangle (frame, 8.0f);
        g.setColour (SuiteUI::Colours::track);
        g.drawRoundedRectangle (frame, 8.0f, 1.0f);
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

    // The page switch at the top right of the header (R8), left of the bar. Plan 18:
    // five options (SLOT 1-4 + MOD), so it is wider than the old two-option switch.
    pageSelector.setBounds (header.removeFromRight (300).withHeight (26)
                              .withY (header.getCentreY() - 13));

    area.removeFromTop (groupGap);

    if (page == Processor::numSlots)
    {
        // Plan 18 Phase 6: the MOD page — the shared panel fills the page.
        modulationGroupBounds = area;
        modulation.setBounds (area.withTrimmedTop (groupTitleHeight + 4).reduced (12, 6));
    }
    else
    {
        // One slot per page (plan 18 Phase 4): the card takes the full width, and the one
        // global (master Output) sits in its own row beneath it.
        modulationGroupBounds = {};
        modulation.setBounds ({});
        cards[(size_t) page]->setBounds (area.removeFromTop (cardContentHeight));
        area.removeFromTop (groupGap);
        masterCtrl.setBounds (area.removeFromTop (masterRowHeight)
                                  .removeFromRight (area.getWidth() / 4).reduced (3));
    }

    // The help line lives in the band the paint used to draw its footer sentence in.
    helpLine.setBounds (getLocalBounds().removeFromBottom (footerHeight));
}

SuiteModulation::ModulationPanel::Style CyclicalEditor::modulationStyle()
{
    return { { juce::Colour (0xffFF7A45), juce::Colour (0xffFF7A45) },
             SuiteUI::Colours::text, SuiteUI::Colours::textDim,
             SuiteUI::Colours::panel, SuiteUI::Colours::track };
}
