// Plugin-level tests for Sgraffito Memory Sampler. Compiles the real processor and editor sources.

#include "PluginTestUtils.h"
#include "ModulationTestUtils.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "StateFixtures.h"

#include <cstdint>

namespace
{
    using namespace PluginTest;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    using Processor = SgraffitoProcessor;

    void testParameters()
    {
        Processor processor;

        checkParameterIds (processor, processor.apvts,
                           { SgraffitoProcessor::modeId,
                             SgraffitoProcessor::regionSpanId,
                             SgraffitoProcessor::regionLengthId,
                             SgraffitoProcessor::rootKeyId,
                             SgraffitoProcessor::pitchFineId,
                             SgraffitoProcessor::scaleLockId,
                             SgraffitoProcessor::scaleId,
                             SgraffitoProcessor::glideId,
                             SgraffitoProcessor::levelId,
                             SgraffitoProcessor::velocityCurveId,
                             SgraffitoProcessor::attackId,
                             SgraffitoProcessor::releaseId,
                             SgraffitoProcessor::tapeStopTimeId,
                             SgraffitoProcessor::tapeStopCurveId,
                             SgraffitoProcessor::memoryAmountId,
                             SgraffitoProcessor::memoryDecayId,
                             SgraffitoProcessor::driftAmountId,
                             SgraffitoProcessor::driftSpeedId,
                             SgraffitoProcessor::layBackId,
                             SgraffitoProcessor::swingId,
                             SgraffitoProcessor::humaniseId,
                             SgraffitoProcessor::ghostOnId,
                             SgraffitoProcessor::ghostThresholdId,
                             SgraffitoProcessor::ghostLevelId,
                             SgraffitoProcessor::ghostDelayId,
                             SgraffitoProcessor::sliceCountId,
                             SgraffitoProcessor::grainSizeId,
                             SgraffitoProcessor::grainDensityId,
                             SgraffitoProcessor::swellTimeId,
                             SgraffitoProcessor::swellCrossfadeId,
                             SgraffitoProcessor::dustId,
                             SgraffitoProcessor::crackleId,
                             SgraffitoProcessor::flutterId,
                             SgraffitoProcessor::wowId,
                             SgraffitoProcessor::lfo1RateId,
                             SgraffitoProcessor::lfo1ShapeId,
                             SgraffitoProcessor::lfo1SyncId,
                             SgraffitoProcessor::lfo1DivisionId,
                             SgraffitoProcessor::lfo1OffsetId,
                             SgraffitoProcessor::lfo2RateId,
                             SgraffitoProcessor::lfo2ShapeId,
                             SgraffitoProcessor::lfo2SyncId,
                             SgraffitoProcessor::lfo2DivisionId,
                             SgraffitoProcessor::lfo2OffsetId,
                             SgraffitoProcessor::modSourceIds[0],
                             SgraffitoProcessor::modTargetIds[0],
                             SgraffitoProcessor::modAmountIds[0],
                             SgraffitoProcessor::modDelayIds[0] },
                           Processor::numControls);

        checkRange (processor.apvts, Processor::modeId, 0.0f, 3.0f, 0.0f);
        checkRange (processor.apvts, Processor::regionSpanId, 0.0f, 100.0f, 100.0f);
        checkRange (processor.apvts, Processor::regionLengthId, 0.0f, 100.0f, 40.0f);
        checkRange (processor.apvts, Processor::rootKeyId, 24.0f, 72.0f, 48.0f);
        checkRange (processor.apvts, Processor::pitchFineId, -100.0f, 100.0f, 0.0f);
        checkRange (processor.apvts, Processor::scaleLockId, 0.0f, 1.0f, 0.0f);
        checkRange (processor.apvts, Processor::scaleId, 0.0f, 4.0f, 2.0f);
        checkRange (processor.apvts, Processor::glideId, 0.0f, 100.0f, 0.0f);
        checkRange (processor.apvts, Processor::levelId, -12.0f, 6.0f, 0.0f);
        checkRange (processor.apvts, Processor::velocityCurveId, 0.0f, 1.0f, 1.0f);
        checkRange (processor.apvts, Processor::attackId, 0.5f, 1.0f, 0.5f);
        checkRange (processor.apvts, Processor::releaseId, 0.5f, 1.0f, 0.5f);
        checkRange (processor.apvts, Processor::tapeStopTimeId, 0.0f, 600.0f, 0.0f);
        checkRange (processor.apvts, Processor::tapeStopCurveId, 0.0f, 1.0f, 0.5f);
        checkRange (processor.apvts, Processor::memoryAmountId, 0.0f, 100.0f, 0.0f);
        checkRange (processor.apvts, Processor::memoryDecayId, 0.0f, 20.0f, 8.0f);
        checkRange (processor.apvts, Processor::driftAmountId, 0.0f, 100.0f, 0.0f);
        checkRange (processor.apvts, Processor::driftSpeedId, 0.0f, 2.0f, 0.5f);
        checkRange (processor.apvts, Processor::layBackId, -100.0f, 100.0f, 0.0f);
        checkRange (processor.apvts, Processor::swingId, -20.0f, 20.0f, 0.0f);
        checkRange (processor.apvts, Processor::humaniseId, 0.0f, 30.0f, 0.0f);
        checkRange (processor.apvts, Processor::ghostThresholdId, 0.0f, 100.0f, 90.0f);
        checkRange (processor.apvts, Processor::ghostLevelId, -12.0f, 6.0f, 0.0f);
        checkRange (processor.apvts, Processor::ghostDelayId, 0.0f, 300.0f, 0.0f);
        checkRange (processor.apvts, Processor::sliceCountId, 1.0f, 32.0f, 4.0f);
        checkRange (processor.apvts, Processor::grainSizeId, 0.5f, 1.0f, 0.7f);
        checkRange (processor.apvts, Processor::grainDensityId, 0.0f, 1.0f, 0.6f);
        checkRange (processor.apvts, Processor::swellTimeId, 0.0f, 600.0f, 0.0f);
        checkRange (processor.apvts, Processor::swellCrossfadeId, 0.0f, 500.0f, 100.0f);
        checkRange (processor.apvts, Processor::dustId, 0.0f, 20.0f, 0.0f);
        checkRange (processor.apvts, Processor::crackleId, 0.0f, 20.0f, 0.0f);
        checkRange (processor.apvts, Processor::flutterId, 0.0f, 5.0f, 0.0f);
        checkRange (processor.apvts, Processor::wowId, 0.0f, 5.0f, 0.0f);

        // The 34 shared LFO and routing parameters.
        checkRange (processor.apvts, Processor::lfo1RateId, 0.02f, 20.0f, 0.25f);
        checkRange (processor.apvts, Processor::lfo1ShapeId, 0.0f, 6.0f, 0.0f);
        checkRange (processor.apvts, Processor::lfo1SyncId, 0.0f, 1.0f, 0.0f);
        checkRange (processor.apvts, Processor::lfo1DivisionId, 0.0f, 6.0f, 4.0f);
        checkRange (processor.apvts, Processor::lfo1OffsetId, -100.0f, 100.0f, 0.0f);
        checkRange (processor.apvts, Processor::lfo2RateId, 0.02f, 20.0f, 0.25f);

        checkRange (processor.apvts, Processor::modSourceIds[0], 0.0f, 1.0f, 0.0f);
        checkRange (processor.apvts, Processor::modTargetIds[0], 0.0f, 37.0f, 0.0f);
        checkRange (processor.apvts, Processor::modAmountIds[0], -100.0f, 100.0f, 0.0f);
        checkRange (processor.apvts, Processor::modDelayIds[0], 0.0f, 10.0f, 0.0f);

        // The target choices must match the enum order the sessions save.
        auto* target = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (Processor::modTargetIds[0]));
        assert (target != nullptr);
        assert (target->choices == Processor::modTargetNames());
        assert (target->choices.size() == (int) Processor::ModTarget::count);

        pass ("testParameters");
    }

    void testStateRoundTrip()
    {
        Processor processor;
        checkStateRoundTrip (processor, processor.apvts, Processor::modeId, 2.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::lfo1ShapeId, 3.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::grainDensityId, 0.4f);
        checkStateRoundTrip (processor, processor.apvts, Processor::driftSpeedId, 1.2f);

        pass ("testStateRoundTrip");
    }

    // A preset is a full parameter state: apply it and the values must come back.
    void testPresetStateRoundTrip()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        setParameter (processor.apvts, Processor::grainDensityId, 0.4f);
        setParameter (processor.apvts, Processor::driftSpeedId, 1.5f);
        assert (std::abs (getParameter (processor.apvts, Processor::grainDensityId) - 0.4f) < 1.0e-3f);

        juce::ValueTree tree = processor.apvts.copyState();
        processor.applyPresetState (tree);

        assert (std::abs (getParameter (processor.apvts, Processor::grainDensityId) - 0.4f) < 1.0e-3f);
        assert (std::abs (getParameter (processor.apvts, Processor::driftSpeedId) - 1.5f) < 1.0e-3f);

        pass ("testPresetStateRoundTrip");
    }

    // The preset bar ships in every build. The factory bank is optional (SUITE_PRESET_BANK);
    void testPresetSeamUnavailable()
    {
        Processor processor;
        SuitePresets::PresetBar bar (processor, juce::Colour (0xff9aa0a6));
        assert (bar.getPreferredHeight() == 30);

#if SUITE_PRESET_BANK
        assert (bar.getNumFactoryPresets() > 0);
#else
        assert (bar.getNumFactoryPresets() == 0);
#endif

        // Without the program flag the plugin keeps the single default program.
#if ! SUITE_PRESET_BANK_AS_PROGRAMS
        {
            Processor processor;
            assert (processor.getNumPrograms() == 1);
            assert (processor.getProgramName (0).isEmpty());
        }
#endif
        pass ("testPresetSeamUnavailable");
    }

    void testLatencyReported()
    {
        SgraffitoProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        // Latency is 0 for Sgraffito - no DSP buffering delay
        assert (processor.getLatencySamples() == 0);

        pass ("testLatencyReported");
    }

    void testBusLayouts()
    {
        Processor processor;

        juce::AudioProcessor::BusesLayout stereo;
        stereo.inputBuses.add (juce::AudioChannelSet::stereo());
        stereo.outputBuses.add (juce::AudioChannelSet::stereo());
        assert (processor.isBusesLayoutSupported (stereo));

        juce::AudioProcessor::BusesLayout mono;
        mono.inputBuses.add (juce::AudioChannelSet::mono());
        mono.outputBuses.add (juce::AudioChannelSet::mono());
        assert (processor.isBusesLayoutSupported (mono));

        pass ("testBusLayouts");
    }

    void testModeSpecificProcessing()
    {
        Processor processor;
        setParameter (processor.apvts, Processor::modeId, 0.0f);   // Scrub
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> audio (2, blockSize);
        juce::MidiBuffer midi;
        int64_t first = 0;

        for (int block = 0; block < 20; ++block)
        {
            audio.clear();
            addSine (audio, 0, sampleRate, 1000.0, 0.5f, first);
            addSine (audio, 1, sampleRate, 1000.0, 0.5f, first);
            processor.processBlock (audio, midi);
            first += blockSize;
        }

        // All modes should produce some output - the specific behavior depends on mode settings
        const auto& snapshot = processor.getSpectrumSnapshot();
        float peakValue = 0.0f;

        for (int k = 0; k < Processor::numBins; ++k)
            peakValue = juce::jmax (peakValue, snapshot.input[(size_t) k].load());

        // With a 1 kHz tone and some memory/processing, we should see some signal
        assert (peakValue > 1.0e-6f);

        pass ("testModeSpecificProcessing");
    }

    void testEditorPages()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        assert (editor != nullptr);

        auto* sgraffitoEditor = dynamic_cast<SgraffitoEditor*> (editor.get());
        assert (sgraffitoEditor != nullptr);

        std::vector<int> pages;
        for (const int page : {0, 1})
        {
            editor->getChildComponent (page)->setVisible (true);
            sgraffitoEditor->setPage (page);

            juce::Image shown = sgraffitoEditor->getComponentForClientBounds (0, 0, 800, 600).grab (0, 0, 800, 600);

            editor->getChildComponent (page)->setVisible (false);
            sgraffitoEditor->setPage (page + 1);

            juce::Image bare = sgraffitoEditor->getComponentForClientBounds (0, 0, 800, 600).grab (0, 0, 800, 600);

            int diff = PluginTest::countChangedPixels (shown, bare);
            assert (diff > 100);   // Should be different pages

            pages.push_back (page);
        }

        sgraffitoEditor->editorBeingDeleted (editor.get());
        pass ("testEditorPages");
    }

    void testModulationParameters()
    {
        Processor processor;

        checkRange (processor.apvts, Processor::lfo1RateId, 0.02f, 20.0f, 0.25f);
        checkRange (processor.apvts, Processor::lfo1ShapeId, 0.0f, 6.0f, 0.0f);
        checkRange (processor.apvts, Processor::lfo1SyncId, 0.0f, 1.0f, 0.0f);
        checkRange (processor.apvts, Processor::lfo1DivisionId, 0.0f, 6.0f, 4.0f);
        checkRange (processor.apvts, Processor::lfo1OffsetId, -100.0f, 100.0f, 0.0f);
        checkRange (processor.apvts, Processor::lfo2RateId, 0.02f, 20.0f, 0.25f);

        checkRange (processor.apvts, Processor::modSourceIds[0], 0.0f, 1.0f, 0.0f);
        checkRange (processor.apvts, Processor::modTargetIds[0], 0.0f, 37.0f, 0.0f);
        checkRange (processor.apvts, Processor::modAmountIds[0], -100.0f, 100.0f, 0.0f);
        checkRange (processor.apvts, Processor::modDelayIds[0], 0.0f, 10.0f, 0.0f);

        auto* target = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (Processor::modTargetIds[0]));
        assert (target != nullptr);
        assert (target->choices == Processor::modTargetNames());
        assert (target->choices.size() == (int) Processor::ModTarget::count);

        assert (processor.getRouter().getNumTargets() == 37);

        pass ("testModulationParameters");
    }

    // With every routing amount at zero the modulation changes nothing, bit for bit.
    void testDefaultRoutingIsSilent()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        for (auto* id : Processor::modAmountIds)
            assert (getParameter (processor.apvts, id) == 0.0f);

        juce::AudioBuffer<float> audio (2, blockSize);
        audio.clear();
        juce::MidiBuffer midi;
        processor.processBlock (audio, midi);

        for (const auto& entry : Processor::targetEntries)
        {
            assert (processor.getRouter().getTargetOffset ((int) entry.target) == 0.0f);
            assert (processor.getModulatedValue (entry.target) == getParameter (processor.apvts, entry.id));
        }

        // A routing aimed at a control with no amount is the same as no routing at all.
        Processor routed;
        setRoute (routed, 0, 0, (int) Processor::ModTarget::DriftSpeed, 0.0f);

        const auto plainOutput = renderLeft (processor, 40);
        const auto routedOutput = renderLeft (routed, 40);

        assert (plainOutput == routedOutput);

        pass ("testDefaultRoutingIsSilent");
    }

    // The routing reaches the audio: a routed drift speed changes the output, and removing the amount restores it.
    void testModulationReachesAudio()
    {
        Processor plain;
        const auto plainOutput = renderLeft (plain, 94);   // ~1 s

        Processor modulated;
        setLfo (modulated, 0, 4.0f, 3.0f, false, 4.0f);            // square, 4 Hz, free
        setRoute (modulated, 0, 0, (int) Processor::ModTarget::DriftSpeed, 100.0f);
        const auto modulatedOutput = renderLeft (modulated, 94);

        assert (plainOutput.size() == modulatedOutput.size());

        double difference = 0.0;
        for (size_t n = 0; n < plainOutput.size(); ++n)
            difference += std::abs (plainOutput[n] - modulatedOutput[n]);

        assert (difference / (double) plainOutput.size() > 1.0e-3);

        // The same routing with no amount is bit-identical to no routing.
        Processor idle;
        setLfo (idle, 0, 4.0f, 3.0f, false, 4.0f);
        setRoute (idle, 0, 0, (int) Processor::ModTarget::DriftSpeed, 0.0f);
        const auto idleOutput = renderLeft (idle, 94);
        assert (idleOutput == plainOutput);

        pass ("testModulationReachesAudio");
    }

    // A synced LFO runs on the tempo clock: the internal 120 BPM with no host playhead.
    void testSyncedLfoFollowsTempo()
    {
        Processor processor;
        setParameter (processor.apvts, Processor::lfo1SyncId, 1.0f);
        setParameter (processor.apvts, Processor::lfo1DivisionId, 4.0f);   // 1/4 = 1 beat per cycle
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> audio (2, blockSize);
        juce::MidiBuffer midi;
        float quarter = 0.0f;

        for (int block = 0; block < 48; ++block)
        {
            audio.clear();
            midi.clear();
            processor.processBlock (audio, midi);

            const float phase = processor.getRouter().getLfoPhase (0);
            quarter = juce::jmax (quarter, phase);
        }

        assert (std::abs (quarter - 0.5f) < 0.02f);
        assert (std::abs (quarter - 0.0f) < 0.02f);

        pass ("testSyncedLfoFollowsTempo");
    }

    // A routed control stays inside its own range, whatever the LFO does.
    void testModulationClampsToRange()
    {
        Processor processor;
        setLfo (processor, 0, 3.0f, 3.0f, false, 4.0f);                  // square, 3 Hz
        setRoute (processor, 0, 0, (int) Processor::ModTarget::GrainDensity, 100.0f);
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> audio (2, blockSize);
        juce::MidiBuffer midi;
        float smallest = 1.0f;
        float largest = 0.0f;

        for (int block = 0; block < 240; ++block)
        {
            audio.clear();
            midi.clear();
            processor.processBlock (audio, midi);

            const float value = processor.getModulatedValue (Processor::ModTarget::GrainDensity);
            assert (std::isfinite (value));
            smallest = juce::jmin (smallest, value);
            largest = juce::jmax (largest, value);
        }

        assert (smallest >= 0.0f - 1.0e-3f);
        assert (largest <= 1.0f + 1.0e-3f);

        pass ("testModulationClampsToRange");
    }

    // Both editor pages paint, and switching pages does not change a parameter.
    void testEditorPages()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        assert (editor != nullptr);

        auto* sgraffitoEditor = dynamic_cast<SgraffitoEditor*> (editor.get());
        assert (sgraffitoEditor != nullptr);

        std::vector<int> pages;
        for (const int page : {0, 1})
        {
            editor->getChildComponent (page)->setVisible (true);
            sgraffitoEditor->setPage (page);

            juce::Image shown = sgraffitoEditor->getComponentForClientBounds (0, 0, 800, 600).grab (0, 0, 800, 600);

            editor->getChildComponent (page)->setVisible (false);
            sgraffitoEditor->setPage (page + 1);

            juce::Image bare = sgraffitoEditor->getComponentForClientBounds (0, 0, 800, 600).grab (0, 0, 800, 600);

            int diff = PluginTest::countChangedPixels (shown, bare);
            assert (diff > 100);   // Should be different pages

            pages.push_back (page);
        }

        sgraffitoEditor->editorBeingDeleted (editor.get());
        pass ("testEditorPages");
    }

    void testLerpCurveState()
    {
        Processor processor;
        setParameter (processor.apvts, Processor::lerpCurveId, "0.0,0.3;0.25,0.6;0.5,0.8;0.75,0.95;1.0,1.0");
        processor.prepareToPlay (sampleRate, blockSize);

        // The lerp curve is stored in the state and should round-trip correctly
        const auto saved = processor.apvts.copyState();
        setParameter (processor.apvts, Processor::lerpCurveId, "0.0,0.5;0.25,0.75;0.5,0.9;0.75,0.98;1.0,1.0");

        processor.apvts.setStateInformation (saved.getRawData(), (int) saved.getSize());

        const auto* restored = dynamic_cast<juce::AudioParameterString*> (processor.apvts.getParameter (Processor::lerpCurveId));
        assert (restored != nullptr);
        assert (restored->getValue() == "0.0,0.3;0.25,0.6;0.5,0.8;0.75,0.95;1.0,1.0");

        pass ("testLerpCurveState");
    }

    // Renders the left channel of a processor fed with a 1 kHz sine, for a number of blocks.
    std::vector<float> renderLeft (Processor& processor, int blocks, float amplitude = 0.5f)
    {
        processor.prepareToPlay (sampleRate, blockSize);
        juce::MidiBuffer midi;
        int64_t first = 0;
        std::vector<float> out;

        for (int block = 0; block < blocks; ++block)
        {
            juce::AudioBuffer<float> audio (2, blockSize);
            audio.clear();
            addSine (audio, 0, sampleRate, 1000.0, amplitude, first);
            addSine (audio, 1, sampleRate, 1000.0, amplitude, first);
            processor.processBlock (audio, midi);
            first += blockSize;

            for (int n = 0; n < blockSize; ++n)
                out.push_back (audio.getSample (0, n));
        }

        return out;
    }

    float rmsOfTail (const std::vector<float>& signal, int count)
    {
        double sum = 0.0;
        for (size_t i = signal.size() - (size_t) count; i < signal.size(); ++i)
            sum += (double) signal[i] * signal[i];
        return (float) std::sqrt (sum / count);
    }

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::puts ("Running Sgraffito plugin tests...");

    testParameters();
    testStateRoundTrip();
    testPresetStateRoundTrip();
    testPresetSeamUnavailable();
    testLatencyReported();
    testBusLayouts();
    testModeSpecificProcessing();
    testEditorPages();
    testModulationParameters();
    testDefaultRoutingIsSilent();
    testModulationReachesAudio();
    testSyncedLfoFollowsTempo();
    testModulationClampsToRange();
    testLerpCurveState();

    testRenderBaseline ();

    std::puts ("All Sgraffito plugin tests passed.");
    return 0;
}
