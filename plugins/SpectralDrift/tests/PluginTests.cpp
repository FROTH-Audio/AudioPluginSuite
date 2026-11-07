// Plugin-level tests for Spectral Drift Shifter. Compiles the real processor and editor sources.

#include "PluginTestUtils.h"
#include "ModulationTestUtils.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <cstdint>

namespace
{
    using namespace PluginTest;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    using Processor = SpectralDriftProcessor;

    void testParameters()
    {
        SpectralDriftProcessor processor;

        checkParameterIds (processor, processor.apvts,
                           { SpectralDriftProcessor::driftRateId, SpectralDriftProcessor::depthId,
                             SpectralDriftProcessor::spreadId, SpectralDriftProcessor::stereoPhaseId,
                             SpectralDriftProcessor::lfoShapeId, SpectralDriftProcessor::shiftId,
                             SpectralDriftProcessor::tiltId, SpectralDriftProcessor::gateId,
                             SpectralDriftProcessor::feedbackId, SpectralDriftProcessor::mixId,
                             SpectralDriftProcessor::freezeId, SpectralDriftProcessor::feedbackDampingId,
                             SpectralDriftProcessor::driftStartId, SpectralDriftProcessor::outputGainId,
                             SpectralDriftProcessor::lfo1RateId, SpectralDriftProcessor::lfo1ShapeId,
                             SpectralDriftProcessor::lfo1SyncId, SpectralDriftProcessor::lfo1DivisionId,
                             SpectralDriftProcessor::lfo1OffsetId,
                             SpectralDriftProcessor::lfo2RateId, SpectralDriftProcessor::lfo2ShapeId,
                             SpectralDriftProcessor::lfo2SyncId, SpectralDriftProcessor::lfo2DivisionId,
                             SpectralDriftProcessor::lfo2OffsetId,
                             SpectralDriftProcessor::modSourceIds[0], SpectralDriftProcessor::modSourceIds[5],
                             SpectralDriftProcessor::modTargetIds[0], SpectralDriftProcessor::modTargetIds[5],
                             SpectralDriftProcessor::modAmountIds[0], SpectralDriftProcessor::modAmountIds[5],
                             SpectralDriftProcessor::modDelayIds[0], SpectralDriftProcessor::modDelayIds[5] },
                           SpectralDriftProcessor::numControls);

        checkRange (processor.apvts, SpectralDriftProcessor::driftRateId, 0.1f, 10.0f, 1.0f);
        checkRange (processor.apvts, SpectralDriftProcessor::depthId, 0.0f, 100.0f, 19.1f);
        checkRange (processor.apvts, SpectralDriftProcessor::spreadId, 0.0f, 1.0f, 0.05f);
        checkRange (processor.apvts, SpectralDriftProcessor::stereoPhaseId, 0.0f, 180.0f, 90.0f);
        checkRange (processor.apvts, SpectralDriftProcessor::lfoShapeId, 0.0f, 3.0f, 0.0f);
        checkRange (processor.apvts, SpectralDriftProcessor::shiftId, -32.0f, 32.0f, 0.0f);
        checkRange (processor.apvts, SpectralDriftProcessor::tiltId, -2.0f, 2.0f, 0.0f);
        checkRange (processor.apvts, SpectralDriftProcessor::gateId, -90.0f, 0.0f, -90.0f);
        checkRange (processor.apvts, SpectralDriftProcessor::feedbackId, 0.0f, 20.0f, 0.0f);
        checkRange (processor.apvts, SpectralDriftProcessor::feedbackDampingId, 0.0f, 100.0f, 30.0f);
        checkRange (processor.apvts, SpectralDriftProcessor::driftStartId, 0.0f, 100.0f, 0.0f);
        checkRange (processor.apvts, SpectralDriftProcessor::outputGainId, -12.0f, 12.0f, 0.0f);
        checkRange (processor.apvts, SpectralDriftProcessor::mixId, 0.0f, 100.0f, 100.0f);
        checkRange (processor.apvts, SpectralDriftProcessor::freezeId, 0.0f, 1.0f, 0.0f);

        // The 34 shared LFO and routing parameters.
        checkRange (processor.apvts, Processor::lfo1RateId, 0.02f, 20.0f, 0.25f);
        checkRange (processor.apvts, Processor::lfo1ShapeId, 0.0f, 6.0f, 0.0f);
        checkRange (processor.apvts, Processor::lfo1SyncId, 0.0f, 1.0f, 0.0f);
        checkRange (processor.apvts, Processor::lfo1DivisionId, 0.0f, 6.0f, 4.0f);
        checkRange (processor.apvts, Processor::lfo1OffsetId, -100.0f, 100.0f, 0.0f);
        checkRange (processor.apvts, Processor::lfo2RateId, 0.02f, 20.0f, 0.25f);
        checkRange (processor.apvts, Processor::lfo2ShapeId, 0.0f, 6.0f, 0.0f);
        checkRange (processor.apvts, Processor::lfo2SyncId, 0.0f, 1.0f, 0.0f);
        checkRange (processor.apvts, Processor::lfo2DivisionId, 0.0f, 6.0f, 4.0f);
        checkRange (processor.apvts, Processor::lfo2OffsetId, -100.0f, 100.0f, 0.0f);

        checkRange (processor.apvts, Processor::modSourceIds[0], 0.0f, 1.0f, 0.0f);
        checkRange (processor.apvts, Processor::modTargetIds[0], 0.0f, 12.0f, 0.0f);
        checkRange (processor.apvts, Processor::modAmountIds[0], -100.0f, 100.0f, 0.0f);
        checkRange (processor.apvts, Processor::modDelayIds[0], 0.0f, 10.0f, 0.0f);

        // The target choices must match the enum order the sessions save.
        auto* target = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (Processor::modTargetIds[0]));
        assert (target != nullptr);
        assert (target->choices == Processor::modTargetNames());
        assert (target->choices.size() == (int) Processor::ModTarget::count);

        assert (processor.getRouter().getNumTargets() == 12);

        pass ("testParameters");
    }

    void testStateRoundTrip()
    {
        SpectralDriftProcessor processor;
        checkStateRoundTrip (processor, processor.apvts, SpectralDriftProcessor::driftRateId, 5.0f);
        checkStateRoundTrip (processor, processor.apvts, SpectralDriftProcessor::lfoShapeId, 2.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::lfo1RateId, 3.5f);
        checkStateRoundTrip (processor, processor.apvts, Processor::modTargetIds[1], 7.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::modAmountIds[1], -42.0f);

        pass ("testStateRoundTrip");
    }

    void testLatencyReported()
    {
        SpectralDriftProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        assert (processor.getLatencySamples() == 2048);

        pass ("testLatencyReported");
    }

    void testBusLayouts()
    {
        SpectralDriftProcessor processor;

        juce::AudioProcessor::BusesLayout stereo;
        stereo.inputBuses.add (juce::AudioChannelSet::stereo());
        stereo.outputBuses.add (juce::AudioChannelSet::stereo());
        assert (processor.isBusesLayoutSupported (stereo));

        juce::AudioProcessor::BusesLayout mono;
        mono.inputBuses.add (juce::AudioChannelSet::mono());
        mono.outputBuses.add (juce::AudioChannelSet::mono());
        assert (processor.isBusesLayoutSupported (mono));

        juce::AudioProcessor::BusesLayout mismatched;
        mismatched.inputBuses.add (juce::AudioChannelSet::mono());
        mismatched.outputBuses.add (juce::AudioChannelSet::stereo());
        assert (! processor.isBusesLayoutSupported (mismatched));

        pass ("testBusLayouts");
    }

    // Regression test for the empty preview: once audio flows, the spectrum must show the tone.
    void testSignalIsPublishedToSpectrum()
    {
        SpectralDriftProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> audio (2, blockSize);
        juce::MidiBuffer midi;
        int64_t first = 0;

        for (int block = 0; block < 40; ++block)
        {
            audio.clear();
            addSine (audio, 0, sampleRate, 1000.0, 0.5f, first);
            addSine (audio, 1, sampleRate, 1000.0, 0.5f, first);
            processor.processBlock (audio, midi);
            first += blockSize;
        }

        const auto& snapshot = processor.getSpectrumSnapshot();
        assert (snapshot.frames.load() > 0);

        // 1 kHz sits in bin 1000 / (48000 / 2048), which is about 43.
        int peakBin = 1;
        float peakValue = 0.0f;

        for (int k = 1; k < SpectralDriftProcessor::numBins; ++k)
        {
            const float value = snapshot.input[(size_t) k].load();
            if (value > peakValue)
            {
                peakValue = value;
                peakBin = k;
            }
        }

        assert (peakValue > 10.0f);
        assert (std::abs (peakBin - 43) <= 1);

        pass ("testSignalIsPublishedToSpectrum");
    }

    void testSilenceIsNotShownAsSignal()
    {
        SpectralDriftProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> audio (2, blockSize);
        juce::MidiBuffer midi;

        for (int block = 0; block < 20; ++block)
        {
            audio.clear();
            processor.processBlock (audio, midi);
        }

        const auto& snapshot = processor.getSpectrumSnapshot();
        float peakValue = 0.0f;

        for (int k = 0; k < SpectralDriftProcessor::numBins; ++k)
            peakValue = juce::jmax (peakValue, snapshot.input[(size_t) k].load());

        assert (peakValue < 1.0e-6f);

        pass ("testSilenceIsNotShownAsSignal");
    }

    void testFreezeSustainsSpectrum()
    {
        SpectralDriftProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> audio (2, blockSize);
        juce::MidiBuffer midi;
        int64_t first = 0;

        for (int block = 0; block < 30; ++block)
        {
            audio.clear();
            addSine (audio, 0, sampleRate, 1000.0, 0.5f, first);
            addSine (audio, 1, sampleRate, 1000.0, 0.5f, first);
            processor.processBlock (audio, midi);
            first += blockSize;
        }

        setParameter (processor.apvts, SpectralDriftProcessor::freezeId, 1.0f);

        for (int block = 0; block < 30; ++block)
        {
            audio.clear();
            processor.processBlock (audio, midi);
        }

        // Input is silent, yet the frozen spectrum keeps the output sounding.
        assert (peak (audio) > 0.05f);
        assert (allFinite (audio));

        pass ("testFreezeSustainsSpectrum");
    }

    void testStereoPhaseDecorrelatesChannels()
    {
        auto channelDifference = [] (float stereoPhase)
        {
            SpectralDriftProcessor processor;
            setParameter (processor.apvts, SpectralDriftProcessor::stereoPhaseId, stereoPhase);
            processor.prepareToPlay (sampleRate, blockSize);

            juce::AudioBuffer<float> audio (2, blockSize);
            juce::MidiBuffer midi;
            int64_t first = 0;
            float difference = 0.0f;

            for (int block = 0; block < 30; ++block)
            {
                audio.clear();
                addSine (audio, 0, sampleRate, 700.0, 0.5f, first);
                addSine (audio, 1, sampleRate, 700.0, 0.5f, first);
                processor.processBlock (audio, midi);
                first += blockSize;

                for (int n = 0; n < blockSize; ++n)
                    difference = juce::jmax (difference, std::abs (audio.getSample (0, n) - audio.getSample (1, n)));
            }

            return difference;
        };

        // Identical channels stay identical when the LFOs are in phase, and separate when they are not.
        assert (channelDifference (0.0f) < 1.0e-5f);
        assert (channelDifference (90.0f) > 1.0e-3f);

        pass ("testStereoPhaseDecorrelatesChannels");
    }

    void testMixZeroIsDelayedDry()
    {
        SpectralDriftProcessor processor;
        setParameter (processor.apvts, SpectralDriftProcessor::mixId, 0.0f);
        processor.prepareToPlay (sampleRate, blockSize);

        const int latency = processor.getLatencySamples();
        const int blocks = 24;
        std::vector<float> input, output;

        for (int block = 0; block < blocks; ++block)
        {
            juce::AudioBuffer<float> audio (2, blockSize);
            audio.clear();
            addSine (audio, 0, sampleRate, 333.0, 0.4f, (int64_t) block * blockSize);
            juce::MidiBuffer midi;

            for (int n = 0; n < blockSize; ++n)
                input.push_back (audio.getSample (0, n));

            processor.processBlock (audio, midi);

            for (int n = 0; n < blockSize; ++n)
                output.push_back (audio.getSample (0, n));
        }

        float maxError = 0.0f;

        for (size_t n = (size_t) latency; n < output.size(); ++n)
            maxError = juce::jmax (maxError, std::abs (output[n] - input[n - (size_t) latency]));

        assert (maxError < 1.0e-5f);

        pass ("testMixZeroIsDelayedDry");
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

    void testOutputGainScalesSignal()
    {
        Processor quiet, loud;
        setParameter (quiet.apvts, Processor::mixId, 0.0f);
        setParameter (loud.apvts, Processor::mixId, 0.0f);
        setParameter (loud.apvts, Processor::outputGainId, 6.0f);

        const float ratio = rmsOfTail (renderLeft (loud, 40), 8192) / rmsOfTail (renderLeft (quiet, 40), 8192);

        // +6 dB is a factor of about 2.
        assert (std::abs (ratio - 1.995f) < 0.02f);

        pass ("testOutputGainScalesSignal");
    }

    // The output gain ramps through the first block after a change. Both channels must see the same ramp.
    // With Mix at 0 the output is the dry signal only, so identical input gives identical output on each channel.
    void testOutputGainRampMatchesChannels()
    {
        Processor processor;
        setParameter (processor.apvts, Processor::mixId, 0.0f);
        processor.prepareToPlay (sampleRate, blockSize);

        setParameter (processor.apvts, Processor::outputGainId, -12.0f);   // the ramp runs through the next block

        juce::AudioBuffer<float> audio (2, blockSize);
        audio.clear();
        addSine (audio, 0, sampleRate, 1000.0, 0.5f, 0);
        addSine (audio, 1, sampleRate, 1000.0, 0.5f, 0);

        juce::MidiBuffer midi;
        processor.processBlock (audio, midi);

        for (int n = 0; n < blockSize; ++n)
            assert (audio.getSample (0, n) == audio.getSample (1, n));

        pass ("testOutputGainRampMatchesChannels");
    }

    void testFeedbackDampingChangesEcho()
    {
        // Feedback at its maximum in the plugin. A single click is repeated by the feedback loop.
        auto echoes = [] (float damping)
        {
            Processor processor;
            setParameter (processor.apvts, Processor::feedbackId, 12.0f);
            setParameter (processor.apvts, Processor::feedbackDampingId, damping);
            processor.prepareToPlay (sampleRate, blockSize);
            juce::MidiBuffer midi;
            std::vector<float> out;

            for (int block = 0; block < 60; ++block)
            {
                juce::AudioBuffer<float> audio (2, blockSize);
                audio.clear();
                if (block == 0)
                    audio.setSample (0, 0, 1.0f);
                processor.processBlock (audio, midi);
                for (int n = 0; n < blockSize; ++n)
                    out.push_back (audio.getSample (0, n));
            }

            return out;
        };

        const auto bright = echoes (0.0f);
        const auto dark = echoes (100.0f);

        float difference = 0.0f;
        for (size_t n = 0; n < bright.size(); ++n)
            difference = juce::jmax (difference, std::abs (bright[n] - dark[n]));

        assert (difference > 1.0e-3f);

        pass ("testFeedbackDampingChangesEcho");
    }

    void testDriftStartLeavesLowBinsAlone()
    {
        // With drift starting at 100 % no bin is drifted, which matches a drift depth of zero.
        Processor undrifted, startedAtTop;
        setParameter (undrifted.apvts, Processor::depthId, 0.0f);
        setParameter (startedAtTop.apvts, Processor::driftStartId, 100.0f);

        const auto a = renderLeft (undrifted, 20);
        const auto b = renderLeft (startedAtTop, 20);

        float difference = 0.0f;
        for (size_t n = 0; n < a.size(); ++n)
            difference = juce::jmax (difference, std::abs (a[n] - b[n]));

        assert (difference < 1.0e-5f);

        // With drift starting at 0 % the same settings do change the sound.
        Processor drifted;
        const auto c = renderLeft (drifted, 20);
        float driftDifference = 0.0f;
        for (size_t n = 0; n < a.size(); ++n)
            driftDifference = juce::jmax (driftDifference, std::abs (a[n] - c[n]));

        assert (driftDifference > 1.0e-3f);

        pass ("testDriftStartLeavesLowBinsAlone");
    }

    void testEditor()
    {
        SpectralDriftProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        checkEditor (processor);

        pass ("testEditor");
    }

    //==============================================================================
    // LFOs and modulation routing

    struct TargetEntry { Processor::ModTarget target; const char* id; };

    const TargetEntry targetEntries[] {
        { Processor::ModTarget::DriftRate,        Processor::driftRateId },
        { Processor::ModTarget::Depth,            Processor::depthId },
        { Processor::ModTarget::BinSpread,        Processor::spreadId },
        { Processor::ModTarget::StereoPhase,      Processor::stereoPhaseId },
        { Processor::ModTarget::Shift,            Processor::shiftId },
        { Processor::ModTarget::Tilt,             Processor::tiltId },
        { Processor::ModTarget::Gate,             Processor::gateId },
        { Processor::ModTarget::Feedback,         Processor::feedbackId },
        { Processor::ModTarget::FeedbackDamping,  Processor::feedbackDampingId },
        { Processor::ModTarget::DriftStart,       Processor::driftStartId },
        { Processor::ModTarget::OutputGain,       Processor::outputGainId },
        { Processor::ModTarget::Mix,              Processor::mixId },
    };

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
        checkRange (processor.apvts, Processor::modTargetIds[0], 0.0f, 12.0f, 0.0f);
        checkRange (processor.apvts, Processor::modAmountIds[0], -100.0f, 100.0f, 0.0f);
        checkRange (processor.apvts, Processor::modDelayIds[0], 0.0f, 10.0f, 0.0f);

        auto* target = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (Processor::modTargetIds[0]));
        assert (target != nullptr);
        assert (target->choices == Processor::modTargetNames());
        assert (target->choices.size() == (int) Processor::ModTarget::count);

        assert (processor.getRouter().getNumTargets() == 12);

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

        for (const auto& entry : targetEntries)
        {
            assert (processor.getRouter().getTargetOffset ((int) entry.target) == 0.0f);
            assert (processor.getModulatedValue (entry.target) == getParameter (processor.apvts, entry.id));
        }

        // A routing aimed at a control with no amount is the same as no routing at all.
        Processor routed;
        setRoute (routed, 0, 0, (int) Processor::ModTarget::OutputGain, 0.0f);

        const auto plainOutput = renderLeft (processor, 40);
        const auto routedOutput = renderLeft (routed, 40);

        assert (plainOutput == routedOutput);

        pass ("testDefaultRoutingIsSilent");
    }

    // The routing reaches the audio: a routed output gain changes the output, and removing the amount restores it.
    void testModulationReachesAudio()
    {
        Processor plain;
        const auto plainOutput = renderLeft (plain, 94);   // 1 s

        Processor modulated;
        setLfo (modulated, 0, 4.0f, 3.0f, false, 4.0f);            // square, 4 Hz, free
        setRoute (modulated, 0, 0, (int) Processor::ModTarget::OutputGain, 100.0f);
        const auto modulatedOutput = renderLeft (modulated, 94);

        assert (plainOutput.size() == modulatedOutput.size());

        double difference = 0.0;
        for (size_t n = 0; n < plainOutput.size(); ++n)
            difference += std::abs (plainOutput[n] - modulatedOutput[n]);

        assert (difference / (double) plainOutput.size() > 1.0e-3);

        // The same routing with no amount is bit-identical to no routing.
        Processor idle;
        setLfo (idle, 0, 4.0f, 3.0f, false, 4.0f);
        setRoute (idle, 0, 0, (int) Processor::ModTarget::OutputGain, 0.0f);
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

        // 24 blocks ≈ 0.25 s: one beat every 0.5 s, so half a cycle -> phase ≈ 0.5.
        for (int block = 0; block < 24; ++block)
        {
            audio.clear();
            midi.clear();
            processor.processBlock (audio, midi);
        }
        const float quarter = processor.getRouter().getLfoPhase (0);

        // 24 more blocks ≈ 0.5 s: a full cycle, back to phase ≈ 0.
        for (int block = 0; block < 24; ++block)
        {
            audio.clear();
            midi.clear();
            processor.processBlock (audio, midi);
        }
        const float half = processor.getRouter().getLfoPhase (0);

        assert (std::abs (quarter - 0.5f) < 0.02f);
        assert (std::abs (half - 0.0f) < 0.02f);

        pass ("testSyncedLfoFollowsTempo");
    }

    // A routed control stays inside its own range, whatever the LFO does.
    void testModulationClampsToRange()
    {
        Processor processor;
        setLfo (processor, 0, 3.0f, 3.0f, false, 4.0f);                  // square, 3 Hz
        setRoute (processor, 0, 0, (int) Processor::ModTarget::Shift, 100.0f);
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> audio (2, blockSize);
        juce::MidiBuffer midi;
        float smallest = 32.0f;
        float largest = -32.0f;

        for (int block = 0; block < 240; ++block)
        {
            audio.clear();
            midi.clear();
            processor.processBlock (audio, midi);

            const float value = processor.getModulatedValue (Processor::ModTarget::Shift);
            assert (std::isfinite (value));
            smallest = juce::jmin (smallest, value);
            largest = juce::jmax (largest, value);
        }

        assert (smallest >= -32.0f - 1.0e-3f);
        assert (largest <= 32.0f + 1.0e-3f);

        pass ("testModulationClampsToRange");
    }

    // Both editor pages paint, and switching pages does not change a parameter.
    void testEditorPages()
    {
        SpectralDriftProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        assert (editor != nullptr);

        auto* driftEditor = dynamic_cast<SpectralDriftEditor*> (editor.get());
        assert (driftEditor != nullptr);

        juce::MessageManager::getInstance()->runDispatchLoopUntil (120);

        const float mixBefore = getParameter (processor.apvts, Processor::mixId);
        const float amountBefore = getParameter (processor.apvts, Processor::modAmountIds[0]);

        // A page must draw content of its own. Compare the page as shown with the same page with every
        // child hidden. The difference is what the page's controls draw, so a blank page gives zero.
        auto inkOnPage = [&editor, driftEditor] (int page)
        {
            driftEditor->setPage (page);
            auto shown = PluginTest::paintComponent (*editor);

            for (int i = 0; i < editor->getNumChildComponents(); ++i)
                editor->getChildComponent (i)->setVisible (false);

            auto bare = PluginTest::paintComponent (*editor);

            // Show every child again, then apply the page's own visibility.
            for (int i = 0; i < editor->getNumChildComponents(); ++i)
                editor->getChildComponent (i)->setVisible (true);
            driftEditor->setPage (page);

            return PluginTest::countChangedPixels (shown, bare);
        };

        assert (driftEditor->getPage() == 0);
        assert (inkOnPage (0) > 500);
        assert (driftEditor->getPage() == 0);

        assert (inkOnPage (1) > 500);
        assert (driftEditor->getPage() == 1);

        driftEditor->setPage (0);
        assert (driftEditor->getPage() == 0);

        // Showing the pages must not touch the parameters.
        assert (getParameter (processor.apvts, Processor::mixId) == mixBefore);
        assert (getParameter (processor.apvts, Processor::modAmountIds[0]) == amountBefore);

        processor.editorBeingDeleted (editor.get());
        pass ("testEditorPages");
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::puts ("Running Spectral Drift plugin tests...");

    testParameters();
    testStateRoundTrip();
    testLatencyReported();
    testBusLayouts();
    testSignalIsPublishedToSpectrum();
    testSilenceIsNotShownAsSignal();
    testFreezeSustainsSpectrum();
    testStereoPhaseDecorrelatesChannels();
    testMixZeroIsDelayedDry();
    testOutputGainScalesSignal();
    testOutputGainRampMatchesChannels();
    testFeedbackDampingChangesEcho();
    testDriftStartLeavesLowBinsAlone();
    testEditor();
    testModulationParameters();
    testDefaultRoutingIsSilent();
    testModulationReachesAudio();
    testSyncedLfoFollowsTempo();
    testModulationClampsToRange();
    testEditorPages();

    std::puts ("All Spectral Drift plugin tests passed.");
    return 0;
}
