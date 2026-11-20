#include "PluginEditor.h"

#include "BinaryData.h"
#include "SuiteBrand.h"
#include "SuiteSettings.h"

namespace
{
    const juce::Image& brandLockup()
    {
        static const juce::Image image = juce::ImageCache::getFromMemory (BinaryData::lockup_dynamic_saturation_png,
                                                                         BinaryData::lockup_dynamic_saturation_pngSize);
        return image;
    }
}

namespace
{
    constexpr int editorWidth = 780;
    constexpr int editorHeight = 680;

    constexpr int headerHeight = 40;
    constexpr int pageSelectorHeight = 28;
    constexpr int groupGap = 8;
    constexpr int groupTitleHeight = SuiteUI::groupTitleHeight;
    constexpr int helpHeight = 26;

    // Places components side by side in equal-width cells.
    void layoutRow (juce::Rectangle<int> row, std::initializer_list<juce::Component*> cells)
    {
        const int width = row.getWidth() / (int) cells.size();

        for (auto* cell : cells)
            cell->setBounds (row.removeFromLeft (width).reduced (4));
    }
}

DynamicSaturationEditor::DynamicSaturationEditor (DynamicSaturationProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p)
{
    setLookAndFeel (&lookAndFeel);

    // The bar exists before setSize(), because setSize() runs resized(), which lays the bar out.
    presetBar = std::make_unique<SuitePresets::PresetBar>(processor, juce::Colour(0xffff9f1c));

    setSize (editorWidth, editorHeight);
    setResizable (true, true);
    setResizeLimits (700, 680, 1200, 1200);

    // The shared help line (plan 12): visible on both pages, the footer sentence is the
    // idle text, and hovering a control without help text returns to it.
    helpLine.setIdleText ("2x oversampled  |  transfer curve tracks the input level");
    helpLine.setComponentID ("dynamicsaturation-help");
    helpLine.setEnablePredicate ([] { return SuiteSettings::shared().getBool (SuiteSettings::tooltipsKey, true); });
    SuiteUI::attachHelpLine (*this, helpLine, helpTracker);

    for (auto* component : { static_cast<juce::Component*> (&helpLine),
                             static_cast<juce::Component*> (&transferCurve),
                             static_cast<juce::Component*> (&outputMeterL),
                             static_cast<juce::Component*> (&outputMeterR),
                             static_cast<juce::Component*> (&modeSelector),
                             static_cast<juce::Component*> (&driveSlider),
                             static_cast<juce::Component*> (&sensitivitySlider),
                             static_cast<juce::Component*> (&curveSlider),
                             static_cast<juce::Component*> (&attackSlider),
                             static_cast<juce::Component*> (&releaseSlider),
                             static_cast<juce::Component*> (&mixSlider),
                             static_cast<juce::Component*> (&detectHpfSlider),
                             static_cast<juce::Component*> (&stereoLinkSlider),
                             static_cast<juce::Component*> (&biasSlider),
                             static_cast<juce::Component*> (&foldSlider),
                             static_cast<juce::Component*> (&toneSlider),
                             static_cast<juce::Component*> (&outputSlider),
                             static_cast<juce::Component*> (&pageSelector),
                             static_cast<juce::Component*> (&modulation) })
        addAndMakeVisible (component);

    driveSlider.slider.setTextValueSuffix (" dB");
    sensitivitySlider.slider.setNumDecimalPlacesToDisplay (2);
    curveSlider.slider.setNumDecimalPlacesToDisplay (2);
    attackSlider.slider.setTextValueSuffix (" ms");
    releaseSlider.slider.setTextValueSuffix (" ms");
    mixSlider.slider.setTextValueSuffix (" %");
    // Detect HPF's value text comes from the parameter's stringFromValue (includes the unit).
    stereoLinkSlider.slider.setTextValueSuffix (" %");
    biasSlider.slider.setNumDecimalPlacesToDisplay (2);
    foldSlider.slider.setTextValueSuffix (" %");
    toneSlider.slider.setNumDecimalPlacesToDisplay (2);
    outputSlider.slider.setTextValueSuffix (" dB");

    auto& apvts = processor.apvts;

    auto bind = [this, &apvts] (const char* id, juce::Slider& slider)
    {
        sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider));
    };

    bind (DynamicSaturationProcessor::driveId, driveSlider.slider);
    bind (DynamicSaturationProcessor::sensitivityId, sensitivitySlider.slider);
    bind (DynamicSaturationProcessor::curveId, curveSlider.slider);
    bind (DynamicSaturationProcessor::attackId, attackSlider.slider);
    bind (DynamicSaturationProcessor::releaseId, releaseSlider.slider);
    bind (DynamicSaturationProcessor::mixId, mixSlider.slider);
    bind (DynamicSaturationProcessor::detectHpfId, detectHpfSlider.slider);
    bind (DynamicSaturationProcessor::stereoLinkId, stereoLinkSlider.slider);
    bind (DynamicSaturationProcessor::biasId, biasSlider.slider);
    bind (DynamicSaturationProcessor::foldId, foldSlider.slider);
    bind (DynamicSaturationProcessor::toneId, toneSlider.slider);
    bind (DynamicSaturationProcessor::outputLevelId, outputSlider.slider);

    // Help text on every control (plan 12), through the shared describe() helper.
    SuiteUI::describe (modeSelector, "The saturation curve: Tube, Tape or Diode.");
    SuiteUI::describe (driveSlider, "Gain before the shaper, smoothed 50 ms. The envelope pushes it further at Sensitivity > 0.");
    SuiteUI::describe (sensitivitySlider, "How far the envelope pushes the drive: full drive = base x (1 + sensitivity).");
    SuiteUI::describe (curveSlider, "The envelope's exponent. 1 is linear; higher values emphasise transients.");
    SuiteUI::describe (attackSlider, "Envelope rise time.");
    SuiteUI::describe (releaseSlider, "Envelope fall time.");
    SuiteUI::describe (mixSlider, "Dry/wet balance. The dry path is delayed by the oversampler's latency, so the mix stays phase-aligned.");
    SuiteUI::describe (detectHpfSlider, "A high-pass on the detector only, so bass does not pump the envelope.");
    SuiteUI::describe (stereoLinkSlider, "100 % = one shared envelope (the louder channel); lower values give each channel its own.");
    SuiteUI::describe (biasSlider, "Tube asymmetry. Tube mode only.");
    SuiteUI::describe (foldSlider, "A wavefolder after the shaper.");
    SuiteUI::describe (toneSlider, "A tilt around 1 kHz after the shaper.");
    SuiteUI::describe (outputSlider, "The final gain stage, smoothed 50 ms.");
    SuiteUI::describe (transferCurve, "The transfer curve at the current drive, bias and fold, drawn at the input level.");
    SuiteUI::describe (outputMeterL, "The left channel's output level.");
    SuiteUI::describe (outputMeterR, "The right channel's output level.");
    SuiteUI::describe (pageSelector, "SOUND: the shaper. MOD: the two LFOs and the six routing slots.");
    SuiteUI::describe (*presetBar, "Factory presets, INIT and SAVE. The menu lists user presets.");
    SuiteUI::describe (headerMenu, "Settings... and About...");

    // The mode switch mirrors a choice parameter: the segment index equals the choice index.
    modeSelector.setOptions ({ "TUBE", "TAPE", "DIODE" });
    modeAttachment = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter (DynamicSaturationProcessor::modeId),
        [this] (float value) { modeSelector.setSelectedIndex (juce::roundToInt (value)); });
    modeSelector.onChange = [this] (int index) { modeAttachment->setValueAsCompleteGesture ((float) index); };
    modeAttachment->sendInitialUpdate();

    // Each control that the routing can move, and the target it shows. Whole-number targets are rounded by the router.
    using Target = DynamicSaturationProcessor::ModTarget;
    controls = std::make_unique<SuiteModulation::ControlDisplay> (processor.getRouter(),
        std::vector<SuiteModulation::ControlDisplay::Entry> {
            { driveSlider.slider,       (int) Target::Drive },
            { sensitivitySlider.slider, (int) Target::Sensitivity },
            { mixSlider.slider,         (int) Target::Mix },
            { outputSlider.slider,      (int) Target::OutputLevel },
            { attackSlider.slider,      (int) Target::Attack },
            { releaseSlider.slider,     (int) Target::Release },
            { detectHpfSlider.slider,   (int) Target::DetectHpf },
            { stereoLinkSlider.slider,  (int) Target::StereoLink },
            { biasSlider.slider,        (int) Target::Bias },
            { foldSlider.slider,        (int) Target::Fold },
            { toneSlider.slider,        (int) Target::Tone },
            { curveSlider.slider,       (int) Target::Curve },
        });

    pageSelector.setOptions ({ "SOUND", "MOD" });
    pageSelector.onChange = [this] (int index) { setPage (index); };

    if (presetBar->getPreferredHeight() > 0)
        addAndMakeVisible (*presetBar);
    presetBar->onPresetLoaded = [this] { repaint(); };

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
    startTimerHz (SuiteUI::kRefreshHz);
}

DynamicSaturationEditor::~DynamicSaturationEditor()
{
    stopTimer();
    removeMouseListener (&helpTracker);
    setLookAndFeel (nullptr);
}

SuiteModulation::ModulationPanel::Style DynamicSaturationEditor::modulationStyle()
{
    return { { juce::Colour (0xff4fd1c5), juce::Colour (0xff4fd1c5) },   // both LFOs in LFO 2's teal (plan 05.1)
             SuiteUI::Colours::text, SuiteUI::Colours::textDim,
             SuiteUI::Colours::panel, SuiteUI::Colours::track };
}

void DynamicSaturationEditor::setPage (int newPage)
{
    page = juce::jlimit (0, 1, newPage);
    pageSelector.setSelectedIndex (page);

    for (auto* component : { static_cast<juce::Component*> (&modeSelector),
                             static_cast<juce::Component*> (&driveSlider),
                             static_cast<juce::Component*> (&sensitivitySlider),
                             static_cast<juce::Component*> (&curveSlider),
                             static_cast<juce::Component*> (&attackSlider),
                             static_cast<juce::Component*> (&releaseSlider),
                             static_cast<juce::Component*> (&mixSlider),
                             static_cast<juce::Component*> (&detectHpfSlider),
                             static_cast<juce::Component*> (&stereoLinkSlider),
                             static_cast<juce::Component*> (&biasSlider),
                             static_cast<juce::Component*> (&foldSlider),
                             static_cast<juce::Component*> (&toneSlider),
                             static_cast<juce::Component*> (&outputSlider) })
        component->setVisible (page == 0);

    modulation.setVisible (page == 1);

    resized();
    repaint();
}

void DynamicSaturationEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (SuiteUI::Colours::panelLight, 0.0f, 0.0f,
                                     SuiteUI::Colours::background, 0.0f, (float) getHeight(), false);
    g.setGradientFill (background);
    g.fillAll();

    SuiteUI::drawBrandLockup (g, getLocalBounds().reduced (16, 0).removeFromTop (headerHeight), brandLockup());

    if (page == 1)
    {
        // The shared group frame (plan 12).
        SuiteUI::drawGroupFrame (g, modulationGroupBounds, "MODULATION", lookAndFeel.getAccent());
    }
}

void DynamicSaturationEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    // Reserve space for the preset bar (300x30), right-aligned in the header band. The bar ships
    // in every build now.
    auto headerArea = area.removeFromTop (headerHeight);
    const int barWidth = 300;
    const int barHeight = presetBar != nullptr ? presetBar->getPreferredHeight() : 0;

    // The menu button sits at the far right of the header, right of the preset bar (plan 08 Q4).
    headerMenu.setBounds (headerArea.removeFromRight (26).withHeight (26)
                            .withY (headerArea.getCentreY() - 13));

    if (barHeight > 0)
        presetBar->setBounds (headerArea.removeFromRight (barWidth).withHeight (barHeight)
                                .withY (headerArea.getCentreY() - barHeight / 2));

    pageSelector.setBounds (area.removeFromTop (pageSelectorHeight).withWidth (240));
    area.removeFromTop (groupGap);

    if (page == 0)
        layoutSoundPage (area);
    else
        layoutModPage (area);

    // The help line lives on both pages now (plan 12), in the band the paint used to
    // draw its footer sentence in.
    helpLine.setBounds (getLocalBounds().removeFromBottom (helpHeight));
}

void DynamicSaturationEditor::layoutSoundPage (juce::Rectangle<int> area)
{
    // The knob grid sits at the bottom; the transfer curve and the stereo output meters
    // take all the remaining space above it (plan 05.1 pass 2).
    const int gridHeight = SuiteUI::pillHeight + 3 * (SuiteUI::knobRowHeight + groupGap);
    auto gridArea = area.removeFromBottom (gridHeight);

    // Stereo output meters: one bar per channel of the real output (plan 05.1 pass 2).
    auto meterColumn = area.removeFromRight (56);
    outputMeterR.setBounds (meterColumn.removeFromRight (26).reduced (4, 6));
    outputMeterL.setBounds (meterColumn.removeFromRight (26).reduced (4, 6));
    transferCurve.setBounds (area.reduced (4, 0));

    modeSelector.setBounds (gridArea.removeFromTop (SuiteUI::pillHeight).reduced (120, 0));
    gridArea.removeFromTop (groupGap);

    // Three grouped rows of four (plan 05.1): drive, dynamics, character with output.
    layoutRow (gridArea.removeFromTop (SuiteUI::knobRowHeight),
               { &driveSlider, &sensitivitySlider, &curveSlider, &mixSlider });
    gridArea.removeFromTop (groupGap);
    layoutRow (gridArea.removeFromTop (SuiteUI::knobRowHeight),
               { &attackSlider, &releaseSlider, &detectHpfSlider, &stereoLinkSlider });
    gridArea.removeFromTop (groupGap);
    auto row3 = gridArea.removeFromTop (SuiteUI::knobRowHeight);
    const int quarter = row3.getWidth() / 4;
    biasSlider.setBounds (row3.removeFromLeft (quarter).reduced (4));
    foldSlider.setBounds (row3.removeFromLeft (quarter).reduced (4));
    toneSlider.setBounds (row3.removeFromLeft (quarter).reduced (4));
    outputSlider.setBounds (row3.removeFromLeft (quarter).reduced (4));
}

void DynamicSaturationEditor::layoutModPage (juce::Rectangle<int> area)
{
    area.removeFromBottom (helpHeight + groupGap);

    // The stereo output meters also live on the MOD page (plan 05.1 pass 2).
    auto meterColumn = area.removeFromRight (56);
    outputMeterR.setBounds (meterColumn.removeFromRight (26).reduced (4, 6));
    outputMeterL.setBounds (meterColumn.removeFromRight (26).reduced (4, 6));

    // The curve preview sits above the modulation group, not behind it: the old layout
    // gave the curve the whole page and let the group draw over it. The split mirrors
    // SpectralDrift's MOD page; the group gets its natural height.
    const int groupHeight = groupTitleHeight + 4 + modulation.getPreferredHeight() + 12;
    auto groupArea = area.removeFromBottom (groupHeight);

    transferCurve.setBounds (area.reduced (4, 0));
    area.removeFromBottom (groupGap);

    modulationGroupBounds = groupArea;
    modulation.setBounds (groupArea.withTrimmedTop (groupTitleHeight + 4).reduced (12, 6));
}

void DynamicSaturationEditor::timerCallback()
{
    modulation.refresh();
    controls->update();

    const float env = processor.getEnvelopeLevel();

    // The curve is drawn at the values the processor is using right now, including any modulation.
    using Target = DynamicSaturationProcessor::ModTarget;
    const float baseGain = CoreDSP::decibelsToGain (processor.getModulatedValue (Target::Drive));
    const float sens = processor.getModulatedValue (Target::Sensitivity);
    const float curveValue = processor.getModulatedValue (Target::Curve);
    const float biasValue = processor.getModulatedValue (Target::Bias);
    const float foldAmount = processor.getModulatedValue (Target::Fold) / 100.0f;
    const auto shaping = static_cast<CoreDSP::Saturation::Mode> (juce::roundToInt (
        processor.apvts.getRawParameterValue (DynamicSaturationProcessor::modeId)->load()));

    const float preGain = CoreDSP::Saturation::dynamicDrive (baseGain, sens,
                                                             CoreDSP::Saturation::shapeEnvelope (env, curveValue));

    outputMeterL.setLevel (processor.getOutputPeak (0));
    outputMeterR.setLevel (processor.getOutputPeak (1));
    transferCurve.setCurve ([preGain, shaping, biasValue, foldAmount] (float x)
                            {
                                const float shaped = CoreDSP::Saturation::shape (x * preGain, shaping, biasValue);
                                return CoreDSP::Saturation::fold (shaped, foldAmount);
                            },
                            env);
}
