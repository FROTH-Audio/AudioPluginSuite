#include "PluginEditor.h"

#include "SuiteBrand.h"
#include "BinaryData.h"

namespace
{
    constexpr int editorWidth = 880;
    constexpr int editorHeight = 700;
    constexpr int headerHeight = 44;
    constexpr int pageSelectorHeight = 26;
    constexpr int pageSelectorWidth = 240;
    constexpr int groupGap = 8;
    constexpr int helpHeight = 26;
    constexpr int keyboardHeight = 48;

    const juce::Image& brandLockup()
    {
        static const juce::Image image = juce::ImageCache::getFromMemory (BinaryData::lockup_sgraffito_png,
                                                                         BinaryData::lockup_sgraffito_pngSize);
        return image;
    }

    // A pill control sits centred in its cell at the suite height.
    void layoutPill (juce::Rectangle<int> cell, juce::Component& component, int width)
    {
        component.setBounds (cell.reduced (4).withSizeKeepingCentre (juce::jmin (width, cell.getWidth() - 8),
                                                                     SuiteUI::pillHeight));
    }

    // A row of equal knob cells at the suite size.
    void layoutKnobRow (juce::Rectangle<int> row, std::initializer_list<SuiteUI::LabelledSlider*> knobs)
    {
        const int width = row.getWidth() / (int) knobs.size();

        for (auto* knob : knobs)
            knob->setBounds (row.removeFromLeft (width).reduced (4));
    }
}

SgraffitoEditor::SgraffitoEditor (SgraffitoProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p)
{
    setLookAndFeel (&lookAndFeel);

    // The bar exists before setSize(), because setSize() runs resized().
    presetBar = std::make_unique<SuitePresets::PresetBar> (processor, juce::Colour (0xff9aa0a6));

    setSize (editorWidth, editorHeight);
    setResizable (true, true);
    setResizeLimits (editorWidth - 40, editorHeight - 40, 1200, 1200);

    helpTracker.onMove = [this] (const juce::MouseEvent& e) { showHelp (e); };
    addMouseListener (&helpTracker, true);

    helpLabel.setFont (SuiteUI::uiFont (14.0f));
    helpLabel.setColour (juce::Label::textColourId, SuiteUI::Colours::text);
    helpLabel.setText ("Hover a control to see what it does.", juce::dontSendNotification);

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

    pageSelector.setOptions ({ "SOUND", "TEXTURE", "MOD" });
    pageSelector.onChange = [this] (int index) { setPage (index); };

    // The keyboard plays through the processor's SPSC note queue (plan 05.1).
    keyboard.onNoteOn = [this] (int note, float velocity) { processor.pushUiNote (note, true, velocity); };
    keyboard.onNoteOff = [this] (int note) { processor.pushUiNote (note, false); };

    // The control display shows the modulated value of every routable control on the MOD page.
    using Target = SgraffitoProcessor::ModTarget;
    controls = std::make_unique<SuiteModulation::ControlDisplay> (processor.getRouter(), std::vector<SuiteModulation::ControlDisplay::Entry> {
        { regionSpanSlider.slider,     (int) Target::RegionSpan },
        { regionLengthSlider.slider,   (int) Target::RegionLength },
        { rootKeySlider.slider,        (int) Target::RootKey },
        { pitchFineSlider.slider,      (int) Target::PitchFine },
        { glideSlider.slider,          (int) Target::Glide },
        { levelSlider.slider,          (int) Target::Level },
        { attackSlider.slider,         (int) Target::Attack },
        { releaseSlider.slider,        (int) Target::Release },
        { memoryAmountSlider.slider,   (int) Target::MemoryAmount },
        { driftAmountSlider.slider,    (int) Target::DriftAmount },
        { driftSpeedSlider.slider,     (int) Target::DriftSpeed },
        { layBackSlider.slider,        (int) Target::LayBack },
        { swingSlider.slider,          (int) Target::Swing },
        { humaniseSlider.slider,       (int) Target::Humanise },
        { ghostThresholdSlider.slider, (int) Target::GhostThreshold },
        { ghostLevelSlider.slider,     (int) Target::GhostLevel },
        { grainSizeSlider.slider,      (int) Target::GrainSize },
        { grainDensitySlider.slider,   (int) Target::GrainDensity },
        { swellTimeSlider.slider,      (int) Target::SwellTime },
        { dustSlider.slider,           (int) Target::Dust },
        { crackleSlider.slider,        (int) Target::Crackle },
        { wowSlider.slider,            (int) Target::Wow },
        { flutterSlider.slider,        (int) Target::Flutter },
        { shuffleSeedSlider.slider,    (int) Target::ShuffleSeed },
    });

    for (auto* component : { static_cast<juce::Component*> (&helpLabel),
                              static_cast<juce::Component*> (&modeSelector),
                              static_cast<juce::Component*> (&waveformView),
                              static_cast<juce::Component*> (&keyboard),
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
                              static_cast<juce::Component*> (&modulation),
                              static_cast<juce::Component*> (&pageSelector) })
        addAndMakeVisible (component);

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
    page = juce::jlimit (0, 2, newPage);
    pageSelector.setSelectedIndex (page);

    const bool soundPage = page == 0;
    const bool texturePage = page == 1;
    const bool modPage = page == 2;

    for (auto* component : { static_cast<juce::Component*> (&modeSelector),
                              static_cast<juce::Component*> (&waveformView),
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
                              static_cast<juce::Component*> (&ghostDelaySlider) })
        component->setVisible (texturePage);

    for (auto* component : { static_cast<juce::Component*> (&sliceCountSlider),
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
                              static_cast<juce::Component*> (&modulation) })
        component->setVisible (modPage);

    resized();
    repaint();
}

void SgraffitoEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (SuiteUI::Colours::panelLight, 0.0f, 0.0f,
                                     SuiteUI::Colours::background, 0.0f, (float) getHeight(), false);
    g.setGradientFill (background);
    g.fillAll();

    // Brand lockup at the left of the header.
    SuiteUI::drawBrandLockup (g, getLocalBounds().removeFromTop (headerHeight).reduced (16, 0), brandLockup());

    // Group frames, per page.
    for (const auto& group : groups)
    {
        if (group.page != page)
            continue;

        const auto bounds = group.bounds.toFloat();

        g.setColour (SuiteUI::Colours::panel.withAlpha (0.55f));
        g.fillRoundedRectangle (bounds, SuiteUI::controlRadius);
        g.setColour (SuiteUI::Colours::track);
        g.drawRoundedRectangle (bounds, SuiteUI::controlRadius, 1.0f);

        g.setColour (lookAndFeel.getAccent());
        g.setFont (SuiteUI::uiFont (15.0f, true));
        g.drawText (group.title, group.bounds.withHeight (SuiteUI::groupTitleHeight).translated (0, 4).reduced (12, 0),
                    juce::Justification::centredLeft);
    }
}

void SgraffitoEditor::resized()
{
    groups.clear();

    // Header: lockup left, preset bar and page selector right.
    auto area = getLocalBounds().reduced (16);
    auto headerArea = area.removeFromTop (headerHeight);

    if (presetBar != nullptr && presetBar->getPreferredHeight() > 0)
        presetBar->setBounds (headerArea.removeFromRight (300)
                                .withHeight (presetBar->getPreferredHeight())
                                .withY (headerArea.getCentreY() - presetBar->getPreferredHeight() / 2));

    pageSelector.setBounds (headerArea.removeFromRight (pageSelectorWidth)
                                .withSizeKeepingCentre (pageSelectorWidth, pageSelectorHeight));

    // The playable keyboard sits at the very bottom of every page.
    keyboard.setBounds (area.removeFromBottom (keyboardHeight));
    helpLabel.setBounds (area.removeFromBottom (helpHeight).reduced (16, 0));
    area.removeFromBottom (groupGap);

    if (page == 0)
        layoutSoundPage (area);
    else if (page == 1)
        layoutTexturePage (area);
    else
        layoutModPage (area);
}

void SgraffitoEditor::layoutSoundPage (juce::Rectangle<int> area)
{
    // Mode: full-width selector.
    modeSelector.setBounds (area.removeFromTop (SuiteUI::pillHeight).reduced (8, 0));

    area.removeFromTop (groupGap);

    // The waveform takes the space the fixed rows below leave, so the page has no dead area.
    const int fixedBelow = 2 * SuiteUI::knobRowHeight + SuiteUI::pillHeight + 4 * groupGap;
    waveformView.setBounds (area.removeFromTop (juce::jmax (100, juce::jmin (260, area.getHeight() - fixedBelow))));

    area.removeFromTop (groupGap);

    // Row 1: Region Span, Region Length, Root Key, Pitch Fine, Glide.
    layoutKnobRow (area.removeFromTop (SuiteUI::knobRowHeight),
                   { &regionSpanSlider, &regionLengthSlider, &rootKeySlider, &pitchFineSlider, &glideSlider });

    area.removeFromTop (groupGap);

    // Row 2: Level, Velocity Curve, Attack, Release, Scale Lock.
    auto row2 = area.removeFromTop (SuiteUI::knobRowHeight);
    const int fifth = row2.getWidth() / 5;
    levelSlider.setBounds (row2.removeFromLeft (fifth).reduced (4));
    velocityCurveSlider.setBounds (row2.removeFromLeft (fifth).reduced (4));
    attackSlider.setBounds (row2.removeFromLeft (fifth).reduced (4));
    releaseSlider.setBounds (row2.removeFromLeft (fifth).reduced (4));
    layoutPill (row2.removeFromLeft (fifth), scaleLockButton, 150);

    area.removeFromTop (groupGap);

    // Scale: full-width selector.
    scaleSelector.setBounds (area.removeFromTop (SuiteUI::pillHeight).reduced (8, 0));
}

void SgraffitoEditor::layoutTexturePage (juce::Rectangle<int> area)
{
    // Three framed rows, grouped by meaning (plan 05.1); each frame takes an equal share.
    const int frameHeight = (area.getHeight() - 2 * groupGap) / 3;
    const juce::Rectangle<int> tapeMemory (area.getX(), area.getY(), area.getWidth(), frameHeight);
    const juce::Rectangle<int> driftTiming (area.getX(), tapeMemory.getBottom() + groupGap, area.getWidth(), frameHeight);
    const juce::Rectangle<int> ghost (area.getX(), driftTiming.getBottom() + groupGap, area.getWidth(), frameHeight);

    groups.push_back ({ "TAPE & MEMORY", tapeMemory, 1 });
    groups.push_back ({ "DRIFT & TIMING", driftTiming, 1 });
    groups.push_back ({ "GHOST", ghost, 1 });

    // TAPE & MEMORY: the tape stop curve selector takes the second cell.
    auto tapeInner = tapeMemory.withTrimmedTop (SuiteUI::groupTitleHeight + 4).reduced (10, 4);
    const int tapeCell = tapeInner.getWidth() / 4;
    tapeStopTimeSlider.setBounds (tapeInner.removeFromLeft (tapeCell).reduced (4));
    tapeStopCurveSelector.setBounds (tapeInner.removeFromLeft (tapeCell).reduced (8, 14));
    memoryAmountSlider.setBounds (tapeInner.removeFromLeft (tapeCell).reduced (4));
    memoryDecaySlider.setBounds (tapeInner.removeFromLeft (tapeCell).reduced (4));

    // DRIFT & TIMING: six cells.
    auto driftInner = driftTiming.withTrimmedTop (SuiteUI::groupTitleHeight + 4).reduced (10, 4);
    const int driftCell = driftInner.getWidth() / 6;
    driftAmountSlider.setBounds (driftInner.removeFromLeft (driftCell).reduced (4));
    driftSpeedSlider.setBounds (driftInner.removeFromLeft (driftCell).reduced (4));
    layBackSlider.setBounds (driftInner.removeFromLeft (driftCell).reduced (4));
    swingSlider.setBounds (driftInner.removeFromLeft (driftCell).reduced (4));
    humaniseSlider.setBounds (driftInner.removeFromLeft (driftCell).reduced (4));
    shuffleSeedSlider.setBounds (driftInner.removeFromLeft (driftCell).reduced (4));

    // GHOST.
    auto ghostInner = ghost.withTrimmedTop (SuiteUI::groupTitleHeight + 4).reduced (10, 4);
    const int ghostCell = ghostInner.getWidth() / 4;
    layoutPill (ghostInner.removeFromLeft (ghostCell), ghostOnButton, 140);
    ghostThresholdSlider.setBounds (ghostInner.removeFromLeft (ghostCell).reduced (4));
    ghostLevelSlider.setBounds (ghostInner.removeFromLeft (ghostCell).reduced (4));
    ghostDelaySlider.setBounds (ghostInner.removeFromLeft (ghostCell).reduced (4));
}

void SgraffitoEditor::layoutModPage (juce::Rectangle<int> area)
{
    // The modulation panel on top; cloud and swell/surface frames below it.
    const int frameHeight = (area.getHeight() - 2 * groupGap - SuiteUI::knobRowHeight - 10) / 2;
    const juce::Rectangle<int> modGroup (area.getX(), area.getY(), area.getWidth(),
                                         area.getHeight() - 2 * frameHeight - 2 * groupGap);
    const juce::Rectangle<int> cloud (area.getX(), modGroup.getBottom() + groupGap, area.getWidth(), frameHeight);
    const juce::Rectangle<int> swellSurface (area.getX(), cloud.getBottom() + groupGap, area.getWidth(), frameHeight);

    groups.push_back ({ "MODULATION", modGroup, 2 });
    groups.push_back ({ "CLOUD", cloud, 2 });
    groups.push_back ({ "SWELL & SURFACE", swellSurface, 2 });

    modulation.setBounds (modGroup.withTrimmedTop (SuiteUI::groupTitleHeight + 4).reduced (10, 4));

    auto cloudInner = cloud.withTrimmedTop (SuiteUI::groupTitleHeight + 4).reduced (10, 4);
    const int cloudCell = cloudInner.getWidth() / 4;
    sliceCountSlider.setBounds (cloudInner.removeFromLeft (cloudCell).reduced (4));
    grainSizeSlider.setBounds (cloudInner.removeFromLeft (cloudCell).reduced (4));
    grainDensitySlider.setBounds (cloudInner.removeFromLeft (cloudCell).reduced (4));
    grainSpreadSlider.setBounds (cloudInner.removeFromLeft (cloudCell).reduced (4));

    auto surfaceInner = swellSurface.withTrimmedTop (SuiteUI::groupTitleHeight + 4).reduced (10, 4);
    const int cell = surfaceInner.getWidth() / 7;
    layoutPill (surfaceInner.removeFromLeft (cell), sustainHoldButton, 120);
    swellTimeSlider.setBounds (surfaceInner.removeFromLeft (cell).reduced (4));
    swellCrossfadeSlider.setBounds (surfaceInner.removeFromLeft (cell).reduced (4));
    dustSlider.setBounds (surfaceInner.removeFromLeft (cell).reduced (4));
    crackleSlider.setBounds (surfaceInner.removeFromLeft (cell).reduced (4));
    wowSlider.setBounds (surfaceInner.removeFromLeft (cell).reduced (4));
    flutterSlider.setBounds (surfaceInner.removeFromLeft (cell).reduced (4));
}

SuiteModulation::ModulationPanel::Style SgraffitoEditor::modulationStyle()
{
    return { { juce::Colour (0xff9aa0a6), juce::Colour (0xff9aa0a6) },   // both LFOs in the slate accent
             SuiteUI::Colours::text, SuiteUI::Colours::textDim,
             SuiteUI::Colours::panel, SuiteUI::Colours::track };
}

void SgraffitoEditor::timerCallback()
{
    // The keyboard lights the keys the voices hold.
    bool notes[128] = {};
    for (const auto& voice : processor.voicePool.voices)
        if (voice.active)
            notes[juce::jlimit (0, 127, voice.key)] = true;
    keyboard.setActiveNotes (notes);
    keyboard.setRootKey (juce::roundToInt (processor.getParamValue (SgraffitoProcessor::rootKeyId)));

    modulation.refresh();
    controls->update();
    repaint();
}
