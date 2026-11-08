#include "PluginEditor.h"

#include "BinaryData.h"
#include "SuiteBrand.h"

namespace
{
    const juce::Image& brandLockup()
    {
        static const juce::Image image = juce::ImageCache::getFromMemory (BinaryData::lockup_spectral_drift_png,
                                                                         BinaryData::lockup_spectral_drift_pngSize);
        return image;
    }
}

namespace
{
    constexpr int editorWidth = 780;
    constexpr int editorHeight = 776;
    constexpr double fallbackSampleRate = 48000.0;
    constexpr float signalThreshold = 1.0e-4f;

    constexpr int headerHeight = 40;
    constexpr int pageSelectorHeight = 28;
    constexpr int groupGap = 8;
    constexpr int groupTitleHeight = 26;
    constexpr int helpHeight = 26;

    // Places components side by side in equal-width cells.
    void layoutRow (juce::Rectangle<int> row, std::initializer_list<juce::Component*> cells)
    {
        const int width = row.getWidth() / (int) cells.size();

        for (auto* cell : cells)
            cell->setBounds (row.removeFromLeft (width).reduced (4));
    }
}

SpectralDriftEditor::SpectralDriftEditor (SpectralDriftProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      inputMagnitudes (CoreDSP::Spectral::numBins, 0.0f),
      outputMagnitudes (CoreDSP::Spectral::numBins, 0.0f)
{
    setLookAndFeel (&lookAndFeel);
    setSize (editorWidth, editorHeight);
    setResizable (true, true);
    setResizeLimits (640, 656, 1400, 1100);

    helpTracker.onMove = [this] (const juce::MouseEvent& e) { showHelp (e); };
    addMouseListener (&helpTracker, true);

    helpLabel.setFont (juce::Font (juce::FontOptions (14.0f)));
    helpLabel.setColour (juce::Label::textColourId, SuiteUI::Colours::text);
    helpLabel.setText ("Hover a control to see what it does.", juce::dontSendNotification);

    for (auto* component : { static_cast<juce::Component*> (&spectrum),
                             static_cast<juce::Component*> (&rateSlider),
                             static_cast<juce::Component*> (&depthSlider),
                             static_cast<juce::Component*> (&spreadSlider),
                             static_cast<juce::Component*> (&stereoSlider),
                             static_cast<juce::Component*> (&shiftSlider),
                             static_cast<juce::Component*> (&tiltSlider),
                             static_cast<juce::Component*> (&gateSlider),
                             static_cast<juce::Component*> (&shapeCaption),
                             static_cast<juce::Component*> (&shapeSelector),
                             static_cast<juce::Component*> (&feedbackSlider),
                             static_cast<juce::Component*> (&dampingSlider),
                             static_cast<juce::Component*> (&driftStartSlider),
                             static_cast<juce::Component*> (&outputSlider),
                             static_cast<juce::Component*> (&mixSlider),
                             static_cast<juce::Component*> (&freezeButton),
                             static_cast<juce::Component*> (&pageSelector),
                             static_cast<juce::Component*> (&modulation) })
        addAndMakeVisible (component);

    rateSlider.slider.setTextValueSuffix (" Hz");
    rateSlider.slider.setNumDecimalPlacesToDisplay (2);
    depthSlider.slider.setTextValueSuffix (" %");
    spreadSlider.slider.setNumDecimalPlacesToDisplay (3);
    stereoSlider.slider.setTextValueSuffix (" deg");
    shiftSlider.slider.setTextValueSuffix (" bins");
    shiftSlider.slider.setNumDecimalPlacesToDisplay (1);
    tiltSlider.slider.setNumDecimalPlacesToDisplay (2);
    gateSlider.slider.setTextValueSuffix (" dB");
    feedbackSlider.slider.setTextValueSuffix (" %");
    feedbackSlider.slider.setNumDecimalPlacesToDisplay (1);
    dampingSlider.slider.setTextValueSuffix (" %");
    driftStartSlider.slider.setTextValueSuffix (" %");
    outputSlider.slider.setTextValueSuffix (" dB");
    outputSlider.slider.setNumDecimalPlacesToDisplay (1);
    mixSlider.slider.setTextValueSuffix (" %");

    shapeCaption.setText ("LFO SHAPE", juce::dontSendNotification);
    shapeCaption.setJustificationType (juce::Justification::centred);
    shapeCaption.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));

    auto& apvts = processor.apvts;

    auto bind = [this, &apvts] (const char* id, juce::Slider& slider)
    {
        sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider));
    };

    bind (SpectralDriftProcessor::driftRateId, rateSlider.slider);
    bind (SpectralDriftProcessor::depthId, depthSlider.slider);
    bind (SpectralDriftProcessor::spreadId, spreadSlider.slider);
    bind (SpectralDriftProcessor::stereoPhaseId, stereoSlider.slider);
    bind (SpectralDriftProcessor::shiftId, shiftSlider.slider);
    bind (SpectralDriftProcessor::tiltId, tiltSlider.slider);
    bind (SpectralDriftProcessor::gateId, gateSlider.slider);
    bind (SpectralDriftProcessor::feedbackId, feedbackSlider.slider);
    bind (SpectralDriftProcessor::feedbackDampingId, dampingSlider.slider);
    bind (SpectralDriftProcessor::driftStartId, driftStartSlider.slider);
    bind (SpectralDriftProcessor::outputGainId, outputSlider.slider);
    bind (SpectralDriftProcessor::mixId, mixSlider.slider);

    freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, SpectralDriftProcessor::freezeId, freezeButton);

    // The shape switch mirrors a choice parameter: the segment index equals the choice index.
    shapeSelector.setOptions ({ "SINE", "TRI", "SAW", "SQUARE" });
    shapeAttachment = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter (SpectralDriftProcessor::lfoShapeId),
        [this] (float value) { shapeSelector.setSelectedIndex (juce::roundToInt (value)); });
    shapeSelector.onChange = [this] (int index) { shapeAttachment->setValueAsCompleteGesture ((float) index); };
    shapeAttachment->sendInitialUpdate();

    // Each control that the routing can move, and the target it shows.
    using Target = SpectralDriftProcessor::ModTarget;
    controls = std::make_unique<SuiteModulation::ControlDisplay> (processor.getRouter(),
        std::vector<SuiteModulation::ControlDisplay::Entry> {
            { rateSlider.slider,        (int) Target::DriftRate },
            { depthSlider.slider,       (int) Target::Depth },
            { spreadSlider.slider,      (int) Target::BinSpread },
            { stereoSlider.slider,      (int) Target::StereoPhase },
            { shiftSlider.slider,       (int) Target::Shift },
            { tiltSlider.slider,        (int) Target::Tilt },
            { gateSlider.slider,        (int) Target::Gate },
            { feedbackSlider.slider,    (int) Target::Feedback },
            { dampingSlider.slider,     (int) Target::FeedbackDamping },
            { driftStartSlider.slider,  (int) Target::DriftStart },
            { outputSlider.slider,      (int) Target::OutputGain },
            { mixSlider.slider,         (int) Target::Mix },
        });

    // Preset bar (height 0 in public build, so layout is unchanged)
    presetBar = std::make_unique<SuitePresets::PresetBar> (processor, lookAndFeel.getAccent());
    addAndMakeVisible (*presetBar);

    pageSelector.setOptions ({ "SOUND", "MOD" });
    pageSelector.onChange = [this] (int index) { setPage (index); };

    setPage (0);
    startTimerHz (SuiteUI::kRefreshHz);
}

SpectralDriftEditor::~SpectralDriftEditor()
{
    stopTimer();
    removeMouseListener (&helpTracker);
    setLookAndFeel (nullptr);
}

SuiteModulation::ModulationPanel::Style SpectralDriftEditor::modulationStyle()
{
    return { { juce::Colour (0xff2de2ff), juce::Colour (0xffff5ad1) },   // LFO 1 cyan, LFO 2 magenta
             SuiteUI::Colours::text, SuiteUI::Colours::textDim,
             SuiteUI::Colours::panel, SuiteUI::Colours::track };
}

void SpectralDriftEditor::showHelp (const juce::MouseEvent& e)
{
    if (auto* component = e.eventComponent)
    {
        const auto text = component->getHelpText();

        if (text.isNotEmpty())
            helpLabel.setText (text, juce::dontSendNotification);
    }
}

void SpectralDriftEditor::setPage (int newPage)
{
    page = juce::jlimit (0, 1, newPage);
    pageSelector.setSelectedIndex (page);

    for (auto* component : { static_cast<juce::Component*> (&rateSlider),
                             static_cast<juce::Component*> (&depthSlider),
                             static_cast<juce::Component*> (&spreadSlider),
                             static_cast<juce::Component*> (&stereoSlider),
                             static_cast<juce::Component*> (&shiftSlider),
                             static_cast<juce::Component*> (&tiltSlider),
                             static_cast<juce::Component*> (&gateSlider),
                             static_cast<juce::Component*> (&shapeCaption),
                             static_cast<juce::Component*> (&shapeSelector),
                             static_cast<juce::Component*> (&feedbackSlider),
                             static_cast<juce::Component*> (&dampingSlider),
                             static_cast<juce::Component*> (&driftStartSlider),
                             static_cast<juce::Component*> (&outputSlider),
                             static_cast<juce::Component*> (&mixSlider),
                             static_cast<juce::Component*> (&freezeButton) })
        component->setVisible (page == 0);

    modulation.setVisible (page == 1);
    helpLabel.setVisible (page == 1);

    resized();
    repaint();
}

void SpectralDriftEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (SuiteUI::Colours::panelLight, 0.0f, 0.0f,
                                     SuiteUI::Colours::background, 0.0f, (float) getHeight(), false);
    g.setGradientFill (background);
    g.fillAll();

    SuiteUI::drawBrandLockup (g, getLocalBounds().reduced (16, 0).removeFromTop (headerHeight), brandLockup());

    g.setColour (SuiteUI::Colours::textDim);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText ("FFT 2048 / 75 % overlap / 2048 samples latency",
                getLocalBounds().reduced (16, 0).removeFromTop (headerHeight), juce::Justification::centredRight);

    if (page == 1)
    {
        const auto bounds = modulationGroupBounds.toFloat();

        g.setColour (SuiteUI::Colours::panel.withAlpha (0.55f));
        g.fillRoundedRectangle (bounds, 8.0f);
        g.setColour (SuiteUI::Colours::track);
        g.drawRoundedRectangle (bounds, 8.0f, 1.0f);

        g.setColour (lookAndFeel.getAccent());
        g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
        g.drawText ("MODULATION", modulationGroupBounds.withHeight (groupTitleHeight).translated (0, 4).reduced (12, 0),
                    juce::Justification::centredLeft);
    }
}

void SpectralDriftEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (headerHeight);

    pageSelector.setBounds (area.removeFromTop (pageSelectorHeight).withWidth (240));
    area.removeFromTop (groupGap);

    if (page == 0)
        layoutSoundPage (area);
    else
        layoutModPage (area);
}

void SpectralDriftEditor::layoutSoundPage (juce::Rectangle<int> area)
{
    spectrum.setBounds (area.removeFromTop ((int) ((float) area.getHeight() * 0.34f)));
    area.removeFromTop (groupGap);

    const int rowHeight = area.getHeight() / 4;

    // Drift
    layoutRow (area.removeFromTop (rowHeight), { &rateSlider, &depthSlider, &spreadSlider, &stereoSlider });

    // Spectral shaping: the last cell holds the LFO shape caption and switch.
    auto shapingRow = area.removeFromTop (rowHeight);
    const int cellWidth = shapingRow.getWidth() / 4;
    shiftSlider.setBounds (shapingRow.removeFromLeft (cellWidth).reduced (4));
    tiltSlider.setBounds (shapingRow.removeFromLeft (cellWidth).reduced (4));
    gateSlider.setBounds (shapingRow.removeFromLeft (cellWidth).reduced (4));

    auto shapeCell = shapingRow.reduced (8, 4);
    shapeCaption.setBounds (shapeCell.removeFromTop (20));
    shapeCell.removeFromTop (12);
    shapeSelector.setBounds (shapeCell.withSizeKeepingCentre (shapeCell.getWidth(), juce::jmin (30, shapeCell.getHeight())));

    // Feedback, damping, mix and freeze.
    auto feedbackRow = area.removeFromTop (rowHeight);
    auto freezeCell = feedbackRow.removeFromRight (feedbackRow.getWidth() / 4);
    layoutRow (feedbackRow, { &feedbackSlider, &dampingSlider, &mixSlider });
    freezeButton.setBounds (freezeCell.withSizeKeepingCentre (juce::jmin (180, freezeCell.getWidth()), 44));

    // Drift start and output gain.
    layoutRow (area, { &driftStartSlider, &outputSlider });
}

void SpectralDriftEditor::layoutModPage (juce::Rectangle<int> area)
{
    helpLabel.setBounds (area.removeFromBottom (helpHeight));
    area.removeFromBottom (groupGap);

    spectrum.setBounds (area.removeFromTop (juce::jmax (150, (int) ((float) area.getHeight() * 0.30f))));
    area.removeFromTop (groupGap);

    modulationGroupBounds = area;
    modulation.setBounds (area.withTrimmedTop (groupTitleHeight + 4).reduced (12, 6));
}

void SpectralDriftEditor::timerCallback()
{
    modulation.refresh();
    controls->update();

    const auto& snapshot = processor.getSpectrumSnapshot();
    float peak = 0.0f;

    for (int k = 0; k < CoreDSP::Spectral::numBins; ++k)
    {
        const float in = snapshot.input[(size_t) k].load (std::memory_order_relaxed);
        inputMagnitudes[(size_t) k] = in;
        outputMagnitudes[(size_t) k] = snapshot.output[(size_t) k].load (std::memory_order_relaxed);
        peak = juce::jmax (peak, in);
    }

    const auto frames = snapshot.frames.load (std::memory_order_relaxed);
    const bool signal = frames > 0 && peak > signalThreshold;
    spectrum.setSignalPresent (signal);

    // Say which of the two causes applies, so a silent preview can be diagnosed from the screen alone.
    if (frames == 0)
        spectrum.setWaitingMessage ("No audio is reaching the plugin yet. Check that it is on an audible track or the Standalone input.");
    else
        spectrum.setWaitingMessage ("Audio is arriving, but the input is silent. Play or route a signal into the plugin.");

    const double sampleRate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : fallbackSampleRate;
    const double binWidth = sampleRate / (double) CoreDSP::Spectral::frameSize;

    // Scales a full-scale Hann-windowed sine to about -6 dB on the display.
    const float magnitudeScale = 2.0f / (float) CoreDSP::Spectral::frameSize;

    spectrum.setSpectra (inputMagnitudes.data(), outputMagnitudes.data(),
                         CoreDSP::Spectral::numBins, binWidth, magnitudeScale);
}
