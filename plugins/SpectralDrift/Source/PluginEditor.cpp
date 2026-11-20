#include "PluginEditor.h"

#include "BinaryData.h"
#include "SuiteBrand.h"
#include "SuiteSettings.h"

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
    constexpr int editorWidth = 720;
    constexpr int editorHeight = 660;
    constexpr double fallbackSampleRate = 48000.0;
    constexpr float signalThreshold = 1.0e-4f;

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

SpectralDriftEditor::SpectralDriftEditor (SpectralDriftProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      inputMagnitudes (CoreDSP::Spectral::numBins, 0.0f),
      outputMagnitudes (CoreDSP::Spectral::numBins, 0.0f)
{
    setLookAndFeel (&lookAndFeel);

    // The bar exists before setSize(), because setSize() runs resized(), which lays the bar out.
    presetBar.reset(new SuitePresets::PresetBar(static_cast<SuitePresets::StateHolder&>(processor), juce::Colour(0xff2de2ff)));

    setSize (editorWidth, editorHeight);
    setResizable (true, true);
    setResizeLimits (620, 660, 1200, 1200);

    // The shared help line (plan 12): the footer sentence is the idle text, and hovering
    // a control without help text returns to it.
    helpLine.setIdleText ("FFT 2048 / 75 % overlap / 2048 samples latency");
    helpLine.setComponentID ("spectraldrift-help");
    helpLine.setEnablePredicate ([] { return SuiteSettings::shared().getBool (SuiteSettings::tooltipsKey, true); });
    SuiteUI::attachHelpLine (*this, helpLine, helpTracker);

    for (auto* component : { static_cast<juce::Component*> (&helpLine),
                             static_cast<juce::Component*> (&spectrum),
                             static_cast<juce::Component*> (&depthSlider),
                             static_cast<juce::Component*> (&spreadSlider),
                             static_cast<juce::Component*> (&stereoSlider),
                             static_cast<juce::Component*> (&shiftSlider),
                             static_cast<juce::Component*> (&tiltSlider),
                             static_cast<juce::Component*> (&gateSlider),
                             static_cast<juce::Component*> (&feedbackSlider),
                             static_cast<juce::Component*> (&dampingSlider),
                             static_cast<juce::Component*> (&driftStartSlider),
                             static_cast<juce::Component*> (&outputSlider),
                             static_cast<juce::Component*> (&mixSlider),
                             static_cast<juce::Component*> (&freezeButton),
                             static_cast<juce::Component*> (&pageSelector),
                             static_cast<juce::Component*> (&modulation) })
        addAndMakeVisible (component);

    // Drift Rate's value text comes from the parameter's stringFromValue (includes the unit).
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

    // The drift engine's own LFO Shape stays automatable but is not on the UI any more
    // (plan 05.1 pass 2): the routing panel's LFOs cover the need.

    auto& apvts = processor.apvts;

    auto bind = [this, &apvts] (const char* id, juce::Slider& slider)
    {
        sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider));
    };

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

    // Help text on every control (plan 12), through the shared describe() helper.
    SuiteUI::describe (depthSlider, "How far the routed LFO drifts the bin phases. 19.1 % is 0.6 radians.");
    SuiteUI::describe (spreadSlider, "Lag, in frames, between the drift of neighbouring bins.");
    SuiteUI::describe (stereoSlider, "Extra drift lag, in frames, for the right channel.");
    SuiteUI::describe (shiftSlider, "Reads the magnitudes from a neighbouring bin: a spectral shift.");
    SuiteUI::describe (tiltSlider, "A spectral tilt around the middle of the spectrum.");
    SuiteUI::describe (gateSlider, "Bins below the threshold are gated out of the wet spectrum.");
    SuiteUI::describe (feedbackSlider, "The engine's output feeds back into its input ring, through tanh.");
    SuiteUI::describe (dampingSlider, "A one-pole low-pass in the feedback loop.");
    SuiteUI::describe (driftStartSlider, "Bins below this fraction of the spectrum are not drifted.");
    SuiteUI::describe (outputSlider, "The final gain stage, applied to the whole output.");
    SuiteUI::describe (mixSlider, "Dry/wet balance. The dry path is delayed by the frame latency.");
    SuiteUI::describe (freezeButton, "Holds the captured magnitudes; phases still advance.");
    SuiteUI::describe (spectrum, "The input (dim) and output (bright) spectra. Says when it is waiting for audio.");
    SuiteUI::describe (pageSelector, "SOUND: the spectral controls. MOD: the two LFOs and the six routing slots.");
    SuiteUI::describe (*presetBar, "Factory presets, INIT and SAVE. The menu lists user presets.");
    SuiteUI::describe (headerMenu, "Settings... and About...");

    // Each control that the routing can move, and the target it shows.
    using Target = SpectralDriftProcessor::ModTarget;
    controls = std::make_unique<SuiteModulation::ControlDisplay> (processor.getRouter(),
        std::vector<SuiteModulation::ControlDisplay::Entry> {
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

SpectralDriftEditor::~SpectralDriftEditor()
{
    stopTimer();
    removeMouseListener (&helpTracker);
    setLookAndFeel (nullptr);
}

SuiteModulation::ModulationPanel::Style SpectralDriftEditor::modulationStyle()
{
    return { { juce::Colour (0xffff5ad1), juce::Colour (0xffff5ad1) },   // both LFOs in LFO 2's magenta (plan 05.1)
             SuiteUI::Colours::text, SuiteUI::Colours::textDim,
             SuiteUI::Colours::panel, SuiteUI::Colours::track };
}

void SpectralDriftEditor::setPage (int newPage)
{
    page = juce::jlimit (0, 1, newPage);
    pageSelector.setSelectedIndex (page);

    for (auto* component : { static_cast<juce::Component*> (&depthSlider),
                             static_cast<juce::Component*> (&spreadSlider),
                             static_cast<juce::Component*> (&stereoSlider),
                             static_cast<juce::Component*> (&shiftSlider),
                             static_cast<juce::Component*> (&tiltSlider),
                             static_cast<juce::Component*> (&gateSlider),
                             static_cast<juce::Component*> (&feedbackSlider),
                             static_cast<juce::Component*> (&dampingSlider),
                             static_cast<juce::Component*> (&driftStartSlider),
                             static_cast<juce::Component*> (&outputSlider),
                             static_cast<juce::Component*> (&mixSlider),
                             static_cast<juce::Component*> (&freezeButton) })
        component->setVisible (page == 0);

    for (auto* component : { static_cast<juce::Component*> (&modulation) })
        component->setVisible (page == 1);

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

    if (page == 1)
    {
        // The shared group frame (plan 12).
        SuiteUI::drawGroupFrame (g, modulationGroupBounds, "MODULATION", lookAndFeel.getAccent());
    }
}

void SpectralDriftEditor::resized()
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

void SpectralDriftEditor::layoutSoundPage (juce::Rectangle<int> area)
{
    // The spectrum on top takes what the three knob rows leave (plan 05.1 pass 2: the
    // drift-rate knob is gone, freeze moved into its cell, the pill row is gone).
    const int fixedBelow = 3 * (SuiteUI::knobRowHeight + groupGap);
    spectrum.setBounds (area.removeFromTop (juce::jmax (150, area.getHeight() - fixedBelow)));
    area.removeFromTop (groupGap);

    // Row 1: Depth, Bin Spread, Stereo Phase, Shift.
    layoutRow (area.removeFromTop (SuiteUI::knobRowHeight), { &depthSlider, &spreadSlider, &stereoSlider, &shiftSlider });

    area.removeFromTop (groupGap);

    // Row 2: Tilt, Gate, Mix and the freeze switch in the freed cell.
    auto shapingRow = area.removeFromTop (SuiteUI::knobRowHeight);
    const int shapingCell = shapingRow.getWidth() / 4;
    tiltSlider.setBounds (shapingRow.removeFromLeft (shapingCell).reduced (4));
    gateSlider.setBounds (shapingRow.removeFromLeft (shapingCell).reduced (4));
    mixSlider.setBounds (shapingRow.removeFromLeft (shapingCell).reduced (4));
    freezeButton.setBounds (shapingRow.removeFromLeft (shapingCell).reduced (8, 18)
                                .withSizeKeepingCentre (shapingCell - 16, SuiteUI::pillHeight));

    area.removeFromTop (groupGap);

    // Row 3: Feedback, FB Damping, Drift Start, Output.
    layoutRow (area.removeFromTop (SuiteUI::knobRowHeight),
               { &feedbackSlider, &dampingSlider, &driftStartSlider, &outputSlider });
}

void SpectralDriftEditor::layoutModPage (juce::Rectangle<int> area)
{
    area.removeFromBottom (helpHeight + groupGap);

    spectrum.setBounds (area.removeFromTop (juce::jmax (150, (int) ((float) area.getHeight() * 0.30f))));
    area.removeFromTop (groupGap);

    // The drift engine's own LFO Shape selector is gone from the UI (plan 05.1 pass 2):
    // the routing panel's LFOs cover the need; the parameter stays automatable.
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
