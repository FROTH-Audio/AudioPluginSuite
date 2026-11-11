#include "PluginEditor.h"

#include "SuiteBrand.h"
#include "BinaryData.h"

namespace
{
    constexpr int editorWidth = 880;
    constexpr int editorHeight = 820;
    constexpr int headerHeight = 44;
    constexpr int pageSelectorHeight = 26;
    constexpr int pageSelectorWidth = 240;
    constexpr int groupGap = 8;
    constexpr int groupTitleHeight = 20;
    constexpr int helpHeight = 26;
    constexpr int knobRowHeight = 120;   // caption 18 + knob ~90 + value box 18
    constexpr int pillHeight = 36;       // toggle buttons and selectors
    constexpr int selectorRowHeight = 36;

    // A knob never fills its cell: the slider is capped to ~90 px of knob and centred.
    void layoutKnobCell (juce::Rectangle<int> cell, SuiteUI::LabelledSlider& slider)
    {
        auto area = cell.reduced (4);
        slider.setBounds (area.withSizeKeepingCentre (juce::jmin (132, area.getWidth()), area.getHeight()));
    }

    // A pill control (toggle button or selector) sits centred in its cell at a fixed height.
    void layoutPill (juce::Rectangle<int> cell, juce::Component& component, int width, int height = pillHeight)
    {
        component.setBounds (cell.reduced (4).withSizeKeepingCentre (juce::jmin (width, cell.getWidth() - 8), height));
    }

    // A row of equal knob cells.
    void layoutKnobRow (juce::Rectangle<int> row, std::initializer_list<SuiteUI::LabelledSlider*> knobs)
    {
        const int width = row.getWidth() / (int) knobs.size();

        for (auto* knob : knobs)
            layoutKnobCell (row.removeFromLeft (width), *knob);
    }

    const juce::Image& brandLockup()
    {
        static const juce::Image image = juce::ImageCache::getFromMemory (BinaryData::lockup_sgraffito_png,
                                                                         BinaryData::lockup_sgraffito_pngSize);
        return image;
    }
}

SgraffitoEditor::SgraffitoEditor (SgraffitoProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p)
{
    setLookAndFeel (&lookAndFeel);

    setSize (editorWidth, editorHeight);
    setResizable (true, true);
    setResizeLimits (720, 600, 1200, 1200);

    helpTracker.onMove = [this] (const juce::MouseEvent& e) { showHelp (e); };
    addMouseListener (&helpTracker, true);

    helpLabel.setFont (juce::Font (juce::FontOptions (14.0f)));
    helpLabel.setColour (juce::Label::textColourId, SuiteUI::Colours::text);
    helpLabel.setText ("Hover a control to see what it does.", juce::dontSendNotification);

    modeCaption.setText ("MODE", juce::dontSendNotification);
    modeCaption.setJustificationType (juce::Justification::centred);
    modeCaption.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));

    modeSelector.setOptions ({ "SCRUB", "CHOP", "CLOUD", "SWELL" });
    modeSelector.setFontHeight (12.0f);

    auto& apvts = processor.apvts;

    auto bind = [this, &apvts] (const char* id, juce::Slider& slider)
    {
        sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider));
    };

    bind (SgraffitoProcessor::regionSpanId, regionSpanSlider.slider);
    bind (SgraffitoProcessor::regionLengthId, regionLengthSlider.slider);
    bind (SgraffitoProcessor::rootKeyId, rootKeySlider.slider);
    bind (SgraffitoProcessor::pitchFineId, pitchFineSlider.slider);
    bind (SgraffitoProcessor::glideId, glideSlider.slider);
    bind (SgraffitoProcessor::levelId, levelSlider.slider);
    bind (SgraffitoProcessor::velocityCurveId, velocityCurveSlider.slider);
    bind (SgraffitoProcessor::attackId, attackSlider.slider);
    bind (SgraffitoProcessor::releaseId, releaseSlider.slider);

    bind (SgraffitoProcessor::tapeStopTimeId, tapeStopTimeSlider.slider);
    bind (SgraffitoProcessor::memoryAmountId, memoryAmountSlider.slider);
    bind (SgraffitoProcessor::memoryDecayId, memoryDecaySlider.slider);
    bind (SgraffitoProcessor::driftAmountId, driftAmountSlider.slider);
    bind (SgraffitoProcessor::driftSpeedId, driftSpeedSlider.slider);
    bind (SgraffitoProcessor::layBackId, layBackSlider.slider);
    bind (SgraffitoProcessor::swingId, swingSlider.slider);
    bind (SgraffitoProcessor::humaniseId, humaniseSlider.slider);
    bind (SgraffitoProcessor::ghostThresholdId, ghostThresholdSlider.slider);
    bind (SgraffitoProcessor::ghostLevelId, ghostLevelSlider.slider);
    bind (SgraffitoProcessor::ghostDelayId, ghostDelaySlider.slider);
    bind (SgraffitoProcessor::sliceCountId, sliceCountSlider.slider);
    bind (SgraffitoProcessor::grainSizeId, grainSizeSlider.slider);
    bind (SgraffitoProcessor::grainDensityId, grainDensitySlider.slider);
    bind (SgraffitoProcessor::grainSpreadId, grainSpreadSlider.slider);
    bind (SgraffitoProcessor::swellTimeId, swellTimeSlider.slider);
    bind (SgraffitoProcessor::swellCrossfadeId, swellCrossfadeSlider.slider);
    bind (SgraffitoProcessor::dustId, dustSlider.slider);
    bind (SgraffitoProcessor::crackleId, crackleSlider.slider);
    bind (SgraffitoProcessor::wowId, wowSlider.slider);
    bind (SgraffitoProcessor::flutterId, flutterSlider.slider);
    bind (SgraffitoProcessor::shuffleSeedId, shuffleSeedSlider.slider);

    // Set value suffixes.
    regionSpanSlider.slider.setTextValueSuffix (" %");
    regionSpanSlider.slider.setNumDecimalPlacesToDisplay (0);
    regionLengthSlider.slider.setTextValueSuffix (" %");
    regionLengthSlider.slider.setNumDecimalPlacesToDisplay (0);
    rootKeySlider.slider.setNumDecimalPlacesToDisplay (0);
    pitchFineSlider.slider.setTextValueSuffix (" ct");
    pitchFineSlider.slider.setNumDecimalPlacesToDisplay (0);
    glideSlider.slider.setTextValueSuffix (" ms");
    glideSlider.slider.setNumDecimalPlacesToDisplay (0);
    levelSlider.slider.setTextValueSuffix (" dB");
    levelSlider.slider.setNumDecimalPlacesToDisplay (1);
    velocityCurveSlider.slider.setNumDecimalPlacesToDisplay (2);
    attackSlider.slider.setTextValueSuffix (" ms");
    attackSlider.slider.setNumDecimalPlacesToDisplay (0);
    releaseSlider.slider.setTextValueSuffix (" ms");
    releaseSlider.slider.setNumDecimalPlacesToDisplay (0);
    tapeStopTimeSlider.slider.setTextValueSuffix (" ms");
    tapeStopTimeSlider.slider.setNumDecimalPlacesToDisplay (0);
    memoryAmountSlider.slider.setTextValueSuffix (" %");
    memoryAmountSlider.slider.setNumDecimalPlacesToDisplay (0);
    memoryDecaySlider.slider.setTextValueSuffix (" s");
    memoryDecaySlider.slider.setNumDecimalPlacesToDisplay (1);
    driftAmountSlider.slider.setTextValueSuffix (" %");
    driftAmountSlider.slider.setNumDecimalPlacesToDisplay (0);
    driftSpeedSlider.slider.setNumDecimalPlacesToDisplay (2);
    layBackSlider.slider.setTextValueSuffix (" ms");
    layBackSlider.slider.setNumDecimalPlacesToDisplay (0);
    swingSlider.slider.setTextValueSuffix (" %");
    swingSlider.slider.setNumDecimalPlacesToDisplay (0);
    humaniseSlider.slider.setTextValueSuffix (" ms");
    humaniseSlider.slider.setNumDecimalPlacesToDisplay (0);
    ghostThresholdSlider.slider.setTextValueSuffix (" %");
    ghostThresholdSlider.slider.setNumDecimalPlacesToDisplay (0);
    ghostLevelSlider.slider.setTextValueSuffix (" dB");
    ghostLevelSlider.slider.setNumDecimalPlacesToDisplay (0);
    ghostDelaySlider.slider.setTextValueSuffix (" ms");
    ghostDelaySlider.slider.setNumDecimalPlacesToDisplay (0);
    sliceCountSlider.slider.setNumDecimalPlacesToDisplay (0);
    grainSizeSlider.slider.setTextValueSuffix (" ms");
    grainSizeSlider.slider.setNumDecimalPlacesToDisplay (0);
    grainDensitySlider.slider.setTextValueSuffix (" /s");
    grainDensitySlider.slider.setNumDecimalPlacesToDisplay (0);
    grainSpreadSlider.slider.setTextValueSuffix (" %");
    grainSpreadSlider.slider.setNumDecimalPlacesToDisplay (0);
    swellTimeSlider.slider.setTextValueSuffix (" ms");
    swellTimeSlider.slider.setNumDecimalPlacesToDisplay (0);
    swellCrossfadeSlider.slider.setTextValueSuffix (" ms");
    swellCrossfadeSlider.slider.setNumDecimalPlacesToDisplay (0);
    dustSlider.slider.setNumDecimalPlacesToDisplay (0);
    crackleSlider.slider.setNumDecimalPlacesToDisplay (0);
    wowSlider.slider.setNumDecimalPlacesToDisplay (0);
    flutterSlider.slider.setNumDecimalPlacesToDisplay (0);
    shuffleSeedSlider.slider.setNumDecimalPlacesToDisplay (0);

    // Bool attachments.
    scaleLockAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, SgraffitoProcessor::scaleLockId, scaleLockButton);
    ghostOnAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, SgraffitoProcessor::ghostOnId, ghostOnButton);
    sustainHoldAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, SgraffitoProcessor::sustainHoldId, sustainHoldButton);

    // Choice attachments via SegmentedSelector.
    modeAttachment = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter (SgraffitoProcessor::modeId),
        [this] (float value) { modeSelector.setSelectedIndex (juce::roundToInt (value)); });
    modeSelector.onChange = [this] (int index) { modeAttachment->setValueAsCompleteGesture ((float) index); };
    modeAttachment->sendInitialUpdate();

    scaleAttachment = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter (SgraffitoProcessor::scaleId),
        [this] (float value) { scaleSelector.setSelectedIndex (juce::roundToInt (value)); });
    scaleSelector.onChange = [this] (int index) { scaleAttachment->setValueAsCompleteGesture ((float) index); };
    scaleAttachment->sendInitialUpdate();

    tapeStopCurveAttachment = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter (SgraffitoProcessor::tapeStopCurveId),
        [this] (float value) { tapeStopCurveSelector.setSelectedIndex (juce::roundToInt (value)); });
    tapeStopCurveSelector.onChange = [this] (int index) { tapeStopCurveAttachment->setValueAsCompleteGesture ((float) index); };
    tapeStopCurveAttachment->sendInitialUpdate();

    tapeStopCurveSelector.setOptions ({ "Linear", "Exponential", "Slow Then Fast" });
    tapeStopCurveSelector.setFontHeight (12.0f);

    scaleSelector.setOptions ({ "Chromatic", "Major", "Minor", "Minor Pent.", "Dorian", "Whole Tone" });
    scaleSelector.setFontHeight (12.0f);

    pageSelector.setOptions ({ "SOUND", "TEXTURE" });
    pageSelector.onChange = [this] (int index) { setPage (index); };

    for (auto* component : { static_cast<juce::Component*> (&modeCaption),
                             static_cast<juce::Component*> (&modeSelector),
                             static_cast<juce::Component*> (&modeBanner),
                             static_cast<juce::Component*> (&waveformView),
                             static_cast<juce::Component*> (&keyStrip),
                             static_cast<juce::Component*> (&regionSpanSlider),
                             static_cast<juce::Component*> (&regionLengthSlider),
                             static_cast<juce::Component*> (&rootKeySlider),
                             static_cast<juce::Component*> (&pitchFineSlider),
                             static_cast<juce::Component*> (&scaleLockButton),
                             static_cast<juce::Component*> (&scaleSelector),
                             static_cast<juce::Component*> (&glideSlider),
                             static_cast<juce::Component*> (&levelSlider),
                             static_cast<juce::Component*> (&velocityCurveSlider),
                             static_cast<juce::Component*> (&attackSlider),
                             static_cast<juce::Component*> (&releaseSlider),
                             static_cast<juce::Component*> (&tapeStopTimeSlider),
                             static_cast<juce::Component*> (&tapeStopCurveSelector),
                             static_cast<juce::Component*> (&memoryAmountSlider),
                             static_cast<juce::Component*> (&memoryDecaySlider),
                             static_cast<juce::Component*> (&driftAmountSlider),
                             static_cast<juce::Component*> (&driftSpeedSlider),
                             static_cast<juce::Component*> (&layBackSlider),
                             static_cast<juce::Component*> (&swingSlider),
                             static_cast<juce::Component*> (&humaniseSlider),
                             static_cast<juce::Component*> (&ghostOnButton),
                             static_cast<juce::Component*> (&ghostThresholdSlider),
                             static_cast<juce::Component*> (&ghostLevelSlider),
                             static_cast<juce::Component*> (&ghostDelaySlider),
                             static_cast<juce::Component*> (&sliceCountSlider),
                             static_cast<juce::Component*> (&grainSizeSlider),
                             static_cast<juce::Component*> (&grainDensitySlider),
                             static_cast<juce::Component*> (&grainSpreadSlider),
                             static_cast<juce::Component*> (&sustainHoldButton),
                             static_cast<juce::Component*> (&swellTimeSlider),
                             static_cast<juce::Component*> (&swellCrossfadeSlider),
                             static_cast<juce::Component*> (&dustSlider),
                             static_cast<juce::Component*> (&crackleSlider),
                             static_cast<juce::Component*> (&wowSlider),
                             static_cast<juce::Component*> (&flutterSlider),
                             static_cast<juce::Component*> (&shuffleSeedSlider),
                             static_cast<juce::Component*> (&pageSelector) })
        addAndMakeVisible (component);

    addAndMakeVisible (helpLabel);

    setPage (0);
    startTimerHz (SuiteUI::kRefreshHz);
}

SgraffitoEditor::~SgraffitoEditor()
{
    stopTimer();
    removeMouseListener (&helpTracker);
    setLookAndFeel (nullptr);
}

void SgraffitoEditor::showHelp (const juce::MouseEvent& e)
{
    if (auto* component = e.eventComponent)
    {
        const auto text = component->getHelpText();

        if (text.isNotEmpty())
            helpLabel.setText (text, juce::dontSendNotification);
    }
}

void SgraffitoEditor::setPage (int newPage)
{
    page = juce::jlimit (0, 1, newPage);
    pageSelector.setSelectedIndex (page);

    bool soundPage = (page == 0);

    modeBanner.setVisible (soundPage);
    waveformView.setVisible (soundPage);
    keyStrip.setVisible (soundPage);

    for (auto* component : { static_cast<juce::Component*> (&modeCaption),
                             static_cast<juce::Component*> (&modeSelector),
                             static_cast<juce::Component*> (&regionSpanSlider),
                             static_cast<juce::Component*> (&regionLengthSlider),
                             static_cast<juce::Component*> (&rootKeySlider),
                             static_cast<juce::Component*> (&pitchFineSlider),
                             static_cast<juce::Component*> (&scaleLockButton),
                             static_cast<juce::Component*> (&scaleSelector),
                             static_cast<juce::Component*> (&glideSlider),
                             static_cast<juce::Component*> (&levelSlider),
                             static_cast<juce::Component*> (&velocityCurveSlider),
                             static_cast<juce::Component*> (&attackSlider),
                             static_cast<juce::Component*> (&releaseSlider) })
        component->setVisible (soundPage);

    for (auto* component : { static_cast<juce::Component*> (&tapeStopTimeSlider),
                             static_cast<juce::Component*> (&tapeStopCurveSelector),
                             static_cast<juce::Component*> (&memoryAmountSlider),
                             static_cast<juce::Component*> (&memoryDecaySlider),
                             static_cast<juce::Component*> (&driftAmountSlider),
                             static_cast<juce::Component*> (&driftSpeedSlider),
                             static_cast<juce::Component*> (&layBackSlider),
                             static_cast<juce::Component*> (&swingSlider),
                             static_cast<juce::Component*> (&humaniseSlider),
                             static_cast<juce::Component*> (&ghostOnButton),
                             static_cast<juce::Component*> (&ghostThresholdSlider),
                             static_cast<juce::Component*> (&ghostLevelSlider),
                             static_cast<juce::Component*> (&ghostDelaySlider),
                             static_cast<juce::Component*> (&sliceCountSlider),
                             static_cast<juce::Component*> (&grainSizeSlider),
                             static_cast<juce::Component*> (&grainDensitySlider),
                             static_cast<juce::Component*> (&grainSpreadSlider),
                             static_cast<juce::Component*> (&sustainHoldButton),
                             static_cast<juce::Component*> (&swellTimeSlider),
                             static_cast<juce::Component*> (&swellCrossfadeSlider),
                             static_cast<juce::Component*> (&dustSlider),
                             static_cast<juce::Component*> (&crackleSlider),
                             static_cast<juce::Component*> (&wowSlider),
                             static_cast<juce::Component*> (&flutterSlider),
                             static_cast<juce::Component*> (&shuffleSeedSlider) })
        component->setVisible (! soundPage);

    helpLabel.setVisible (! soundPage);

    resized();
    repaint();
}

void SgraffitoEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (SuiteUI::Colours::panelLight, 0.0f, 0.0f,
                                     SuiteUI::Colours::background, 0.0f, (float) getHeight(), false);
    g.setGradientFill (background);
    g.fillAll();

    // Brand lockup at the left of the header, as in the other plugins.
    SuiteUI::drawBrandLockup (g, getLocalBounds().removeFromTop (headerHeight).reduced (16, 0), brandLockup());

    g.setColour (SuiteUI::Colours::textDim);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText ("Memory sampler  |  Scrub / Chop / Cloud / Swell  |  "
                    + juce::String (processor.voicePool.countActive()) + " voices",
                getLocalBounds().removeFromBottom (helpHeight).reduced (16), juce::Justification::centredLeft);

    if (page == 1)
    {
        const auto bounds = textureGroupBounds.toFloat();

        g.setColour (SuiteUI::Colours::panel.withAlpha (0.55f));
        g.fillRoundedRectangle (bounds, 8.0f);
        g.setColour (SuiteUI::Colours::track);
        g.drawRoundedRectangle (bounds, 8.0f, 1.0f);

        g.setColour (lookAndFeel.getAccent());
        g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
        g.drawText ("TEXTURE", textureGroupBounds.withHeight (groupTitleHeight).translated (0, 4).reduced (12, 0),
                    juce::Justification::centredLeft);
    }
}

void SgraffitoEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    helpLabel.setBounds (area.removeFromBottom (helpHeight).reduced (16, 0));
    auto headerArea = area.removeFromTop (headerHeight);
    pageSelector.setBounds (headerArea.removeFromRight (pageSelectorWidth)
                                .withSizeKeepingCentre (pageSelectorWidth, pageSelectorHeight));

    area.removeFromTop (groupGap);

    if (page == 0)
        layoutSoundPage (area);
    else
        layoutTexturePage (area);
}

void SgraffitoEditor::layoutSoundPage (juce::Rectangle<int> area)
{
    // Mode: caption, selector, and the mode banner.
    auto modeRow = area.removeFromTop (16 + pillHeight + 24);
    modeCaption.setBounds (modeRow.removeFromTop (16));
    modeSelector.setBounds (modeRow.removeFromTop (pillHeight).reduced (8, 0));
    modeBanner.setBounds (modeRow.removeFromTop (24).reduced (8, 0));

    area.removeFromTop (groupGap);

    // The waveform takes the space the fixed rows below leave, so the page has no dead area.
    constexpr int keysHeight = 46;
    const int fixedBelow = keysHeight + 2 * knobRowHeight + selectorRowHeight + 5 * groupGap;
    waveformView.setBounds (area.removeFromTop (juce::jmin (260, juce::jmax (110, area.getHeight() - fixedBelow))));

    area.removeFromTop (groupGap);

    // Visual row: key map strip.
    keyStrip.setBounds (area.removeFromTop (keysHeight));

    area.removeFromTop (groupGap);

    // Row 1: Region Span, Region Length, Root Key, Pitch Fine, Glide.
    layoutKnobRow (area.removeFromTop (knobRowHeight),
                   { &regionSpanSlider, &regionLengthSlider, &rootKeySlider, &pitchFineSlider, &glideSlider });

    area.removeFromTop (groupGap);

    // Row 2: Level, Velocity Curve, Attack, Release, Scale Lock.
    auto row2 = area.removeFromTop (knobRowHeight);
    const int fifth = row2.getWidth() / 5;
    layoutKnobCell (row2.removeFromLeft (fifth), levelSlider);
    layoutKnobCell (row2.removeFromLeft (fifth), velocityCurveSlider);
    layoutKnobCell (row2.removeFromLeft (fifth), attackSlider);
    layoutKnobCell (row2.removeFromLeft (fifth), releaseSlider);
    layoutPill (row2.removeFromLeft (fifth), scaleLockButton, 150);

    area.removeFromTop (groupGap);

    // Scale: full-width selector.
    scaleSelector.setBounds (area.removeFromTop (selectorRowHeight).reduced (8, 0));
}

void SgraffitoEditor::layoutTexturePage (juce::Rectangle<int> area)
{
    textureGroupBounds = area;

    // The group title is drawn over the top of the frame; keep the first row clear of it.
    area.removeFromTop (groupTitleHeight);
    area.removeFromTop (groupGap);

    // Row 1: Tape Stop Time, Memory Amount, Memory Decay, Drift Amount, Drift Speed.
    layoutKnobRow (area.removeFromTop (knobRowHeight),
                   { &tapeStopTimeSlider, &memoryAmountSlider, &memoryDecaySlider, &driftAmountSlider, &driftSpeedSlider });

    area.removeFromTop (groupGap);

    // Tape stop curve: full-width selector.
    tapeStopCurveSelector.setBounds (area.removeFromTop (selectorRowHeight).reduced (8, 0));

    area.removeFromTop (groupGap);

    // Row 2: Lay Back, Swing, Humanise, Shuffle Seed, Ghost On.
    auto row = area.removeFromTop (knobRowHeight);
    const int fifth = row.getWidth() / 5;
    layoutKnobCell (row.removeFromLeft (fifth), layBackSlider);
    layoutKnobCell (row.removeFromLeft (fifth), swingSlider);
    layoutKnobCell (row.removeFromLeft (fifth), humaniseSlider);
    layoutKnobCell (row.removeFromLeft (fifth), shuffleSeedSlider);
    layoutPill (row.removeFromLeft (fifth), ghostOnButton, 150);

    area.removeFromTop (groupGap);

    // Row 3: Ghost Threshold, Ghost Level, Ghost Delay, Slice Count, Grain Size.
    layoutKnobRow (area.removeFromTop (knobRowHeight),
                   { &ghostThresholdSlider, &ghostLevelSlider, &ghostDelaySlider, &sliceCountSlider, &grainSizeSlider });

    area.removeFromTop (groupGap);

    // Row 4: Grain Density, Grain Spread, Sustain Hold, Swell Time, Swell Crossfade.
    row = area.removeFromTop (knobRowHeight);
    layoutKnobCell (row.removeFromLeft (fifth), grainDensitySlider);
    layoutKnobCell (row.removeFromLeft (fifth), grainSpreadSlider);
    layoutPill (row.removeFromLeft (fifth), sustainHoldButton, 150);
    layoutKnobCell (row.removeFromLeft (fifth), swellTimeSlider);
    layoutKnobCell (row.removeFromLeft (fifth), swellCrossfadeSlider);

    area.removeFromTop (groupGap);

    // Row 5: Dust, Crackle, Wow, Flutter.
    layoutKnobRow (area.removeFromTop (knobRowHeight),
                   { &dustSlider, &crackleSlider, &wowSlider, &flutterSlider });
}

void SgraffitoEditor::timerCallback()
{
    repaint();
}
