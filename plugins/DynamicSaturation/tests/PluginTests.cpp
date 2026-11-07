// Plugin-level tests for Dynamic Saturation Companion. Compiles the real processor and editor sources.

#include "PluginTestUtils.h"
#include "ModulationTestUtils.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <cstdint>

namespace
{
    using namespace PluginTest;

    // A host playhead whose tempo the test controls. The effects use it only for the tempo.
    struct FakePlayHead : juce::AudioPlayHead
    {
        double bpm = 120.0;

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setBpm (bpm);
            return info;
        }
    };

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    using Processor = DynamicSaturationProcessor;

    // Runs a processor over a sine for a number of blocks and returns the output of the left channel.
    std::vector<float> renderSine (Processor& processor, double hz, float amplitude, int blocks)
    {
        processor.prepareToPlay (sampleRate, blockSize);

        std::vector<float> output;
        juce::MidiBuffer midi;

        for (int block = 0; block < blocks; ++block)
        {
            juce::AudioBuffer<float> audio (2, blockSize);
            audio.clear();
            addSine (audio, 0, sampleRate, hz, amplitude, (int64_t) block * blockSize);
            addSine (audio, 1, sampleRate, hz, amplitude, (int64_t) block * blockSize);

            processor.processBlock (audio, midi);

            for (int n = 0; n < blockSize; ++n)
                output.push_back (audio.getSample (0, n));
        }

        return output;
    }

    void testParameters()
    {
        Processor processor;

        checkParameterIds (processor, processor.apvts,
                           { Processor::driveId, Processor::sensitivityId, Processor::modeId, Processor::mixId,
                             Processor::outputLevelId, Processor::attackId, Processor::releaseId,
                             Processor::detectHpfId, Processor::stereoLinkId, Processor::biasId,
                             Processor::foldId, Processor::toneId, Processor::curveId,
                             Processor::lfo1RateId, Processor::lfo1ShapeId, Processor::lfo1SyncId,
                             Processor::lfo1DivisionId, Processor::lfo1OffsetId,
                             Processor::lfo2RateId, Processor::lfo2ShapeId, Processor::lfo2SyncId,
                             Processor::lfo2DivisionId, Processor::lfo2OffsetId,
                             Processor::modSourceIds[0], Processor::modSourceIds[5],
                             Processor::modTargetIds[0], Processor::modTargetIds[5],
                             Processor::modAmountIds[0], Processor::modAmountIds[5],
                             Processor::modDelayIds[0], Processor::modDelayIds[5] },
                           Processor::numControls);

        checkRange (processor.apvts, Processor::driveId, 0.0f, 24.0f, 6.0f);
        checkRange (processor.apvts, Processor::sensitivityId, 0.0f, 1.0f, 0.5f);
        checkRange (processor.apvts, Processor::modeId, 0.0f, 2.0f, 0.0f);
        checkRange (processor.apvts, Processor::mixId, 0.0f, 100.0f, 100.0f);
        checkRange (processor.apvts, Processor::outputLevelId, -12.0f, 6.0f, 0.0f);
        checkRange (processor.apvts, Processor::attackId, 1.0f, 100.0f, 5.0f);
        checkRange (processor.apvts, Processor::releaseId, 50.0f, 2000.0f, 150.0f);
        checkRange (processor.apvts, Processor::detectHpfId, 20.0f, 800.0f, 20.0f);
        checkRange (processor.apvts, Processor::stereoLinkId, 0.0f, 100.0f, 100.0f);
        checkRange (processor.apvts, Processor::biasId, 0.0f, 0.25f, 0.2f);
        checkRange (processor.apvts, Processor::foldId, 0.0f, 50.0f, 0.0f);
        checkRange (processor.apvts, Processor::toneId, -1.0f, 1.0f, 0.0f);
        checkRange (processor.apvts, Processor::curveId, 0.5f, 3.0f, 1.0f);

        pass ("testParameters");
    }

    void testStateRoundTrip()
    {
        Processor processor;
        checkStateRoundTrip (processor, processor.apvts, Processor::driveId, 18.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::foldId, 50.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::lfo1RateId, 3.5f);
        checkStateRoundTrip (processor, processor.apvts, Processor::modTargetIds[1], 7.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::modAmountIds[1], -42.0f);

        pass ("testStateRoundTrip");
    }

    void testLatencyMatchesOversampler()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        assert (processor.getLatencySamples() == processor.getOversamplingLatency());
        assert (processor.getLatencySamples() > 0);

        pass ("testLatencyMatchesOversampler");
    }

    void testSilenceStaysSilent()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::MidiBuffer midi;

        for (int block = 0; block < 10; ++block)
        {
            juce::AudioBuffer<float> audio (2, blockSize);
            audio.clear();
            processor.processBlock (audio, midi);
            assert (peak (audio) < 1.0e-9f);
        }

        pass ("testSilenceStaysSilent");
    }

    void testOutputIsBounded()
    {
        // Extreme settings: maximum drive, full sensitivity and maximum output level (+6 dB, about 2.0).
        for (int mode = 0; mode < 3; ++mode)
        {
            Processor processor;
            setParameter (processor.apvts, Processor::driveId, 24.0f);
            setParameter (processor.apvts, Processor::sensitivityId, 1.0f);
            setParameter (processor.apvts, Processor::outputLevelId, 6.0f);
            setParameter (processor.apvts, Processor::modeId, (float) mode);
            setParameter (processor.apvts, Processor::foldId, 100.0f);
            setParameter (processor.apvts, Processor::biasId, 0.5f);

            const auto output = renderSine (processor, 440.0, 1.0f, 30);

            for (float sample : output)
            {
                assert (std::isfinite (sample));
                assert (std::abs (sample) <= 2.0f + 1.0e-3f);
            }
        }

        pass ("testOutputIsBounded");
    }

    void testModesDiffer()
    {
        Processor tube, diode;
        setParameter (tube.apvts, Processor::modeId, 0.0f);
        setParameter (diode.apvts, Processor::modeId, 2.0f);

        const auto a = renderSine (tube, 440.0, 0.8f, 20);
        const auto b = renderSine (diode, 440.0, 0.8f, 20);

        double difference = 0.0;
        for (size_t n = a.size() - 2048; n < a.size(); ++n)
            difference += std::abs (a[n] - b[n]);

        assert (difference / 2048.0 > 1.0e-3);

        pass ("testModesDiffer");
    }

    void testMixZeroIsDelayedDry()
    {
        Processor processor;
        setParameter (processor.apvts, Processor::mixId, 0.0f);
        setParameter (processor.apvts, Processor::driveId, 18.0f);
        setParameter (processor.apvts, Processor::outputLevelId, 0.0f);

        const int blocks = 24;
        std::vector<float> input, output;
        processor.prepareToPlay (sampleRate, blockSize);
        juce::MidiBuffer midi;

        for (int block = 0; block < blocks; ++block)
        {
            juce::AudioBuffer<float> audio (2, blockSize);
            audio.clear();
            addSine (audio, 0, sampleRate, 250.0, 0.5f, (int64_t) block * blockSize);
            addSine (audio, 1, sampleRate, 250.0, 0.5f, (int64_t) block * blockSize);

            for (int n = 0; n < blockSize; ++n)
                input.push_back (audio.getSample (0, n));

            processor.processBlock (audio, midi);

            for (int n = 0; n < blockSize; ++n)
                output.push_back (audio.getSample (0, n));
        }

        const auto latency = (size_t) processor.getOversamplingLatency();
        float maxError = 0.0f;

        for (size_t n = latency; n < output.size(); ++n)
            maxError = juce::jmax (maxError, std::abs (output[n] - input[n - latency]));

        assert (maxError < 1.0e-4f);

        pass ("testMixZeroIsDelayedDry");
    }

    void testStereoLinkDrivesQuietChannelHarder()
    {
        // Left is loud, right is quiet. With a linked detector the quiet side is pushed by the loud one.
        auto rightRms = [] (float link)
        {
            Processor processor;
            setParameter (processor.apvts, Processor::stereoLinkId, link);
            setParameter (processor.apvts, Processor::sensitivityId, 1.0f);
            processor.prepareToPlay (sampleRate, blockSize);

            juce::MidiBuffer midi;
            double sum = 0.0;
            int count = 0;

            for (int block = 0; block < 40; ++block)
            {
                juce::AudioBuffer<float> audio (2, blockSize);
                audio.clear();
                addSine (audio, 0, sampleRate, 300.0, 0.9f, (int64_t) block * blockSize);
                addSine (audio, 1, sampleRate, 300.0, 0.05f, (int64_t) block * blockSize);
                processor.processBlock (audio, midi);

                if (block >= 20)
                    for (int n = 0; n < blockSize; ++n, ++count)
                        sum += (double) audio.getSample (1, n) * audio.getSample (1, n);
            }

            return (float) std::sqrt (sum / count);
        };

        assert (rightRms (100.0f) > rightRms (0.0f) * 1.05f);

        pass ("testStereoLinkDrivesQuietChannelHarder");
    }

    void testFoldChangesSound()
    {
        Processor plain, folded;
        setParameter (folded.apvts, Processor::foldId, 100.0f);

        const auto a = renderSine (plain, 400.0, 0.8f, 20);
        const auto b = renderSine (folded, 400.0, 0.8f, 20);

        double difference = 0.0;
        for (size_t n = a.size() - 2048; n < a.size(); ++n)
            difference += std::abs (a[n] - b[n]);

        assert (difference / 2048.0 > 1.0e-3);

        pass ("testFoldChangesSound");
    }

    void testEditor()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        checkEditor (processor);

        pass ("testEditor");
    }

    //==============================================================================
    // LFOs and modulation routing

    /** The twelve routable controls, in ModTarget order, with the parameter each one moves. */
    struct TargetEntry { Processor::ModTarget target; const char* id; };

    const TargetEntry targetEntries[] {
        { Processor::ModTarget::Drive,       Processor::driveId },
        { Processor::ModTarget::Sensitivity, Processor::sensitivityId },
        { Processor::ModTarget::Mix,         Processor::mixId },
        { Processor::ModTarget::OutputLevel, Processor::outputLevelId },
        { Processor::ModTarget::Attack,      Processor::attackId },
        { Processor::ModTarget::Release,     Processor::releaseId },
        { Processor::ModTarget::DetectHpf,   Processor::detectHpfId },
        { Processor::ModTarget::StereoLink,  Processor::stereoLinkId },
        { Processor::ModTarget::Bias,        Processor::biasId },
        { Processor::ModTarget::Fold,        Processor::foldId },
        { Processor::ModTarget::Tone,        Processor::toneId },
        { Processor::ModTarget::Curve,       Processor::curveId },
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

        // The target choices must match the enum order the sessions save.
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

        // One block publishes the offsets the display reads.
        juce::AudioBuffer<float> audio (2, blockSize);
        audio.clear();
        juce::MidiBuffer midi;
        processor.processBlock (audio, midi);

        for (const auto& entry : targetEntries)
        {
            assert (processor.getRouter().getTargetOffset ((int) entry.target) == 0.0f);
            assert (processor.getModulatedValue (entry.target) == getParameter (processor.apvts, entry.id));
        }

        // A routing that is aimed at a control but has no amount is the same as no routing at all.
        Processor routed;
        setRoute (routed, 0, 0, (int) Processor::ModTarget::Drive, 0.0f);

        const auto plainOutput = renderSine (processor, 440.0, 0.3f, 40);
        const auto routedOutput = renderSine (routed, 440.0, 0.3f, 40);

        assert (plainOutput == routedOutput);

        pass ("testDefaultRoutingIsSilent");
    }

    // A routing really moves the control, in the direction the amount says.
    void testModulationDirection()
    {
        auto modulatedDrive = [] (float amount)
        {
            Processor processor;
            setLfo (processor, 0, 1.0f, 3.0f, false, 4.0f);   // square: +1 for the first half cycle
            setRoute (processor, 0, 0, (int) Processor::ModTarget::Drive, amount);
            processor.prepareToPlay (sampleRate, blockSize);

            juce::AudioBuffer<float> audio (2, blockSize);
            audio.clear();
            juce::MidiBuffer midi;
            processor.processBlock (audio, midi);

            return processor.getModulatedValue (Processor::ModTarget::Drive);
        };

        const float base = 6.0f;
        assert (modulatedDrive (100.0f) > base + 1.0f);
        assert (modulatedDrive (-100.0f) < base - 1.0f);

        pass ("testModulationDirection");
    }

    // The routing reaches the audio: a routed drive changes the output, and removing the amount restores it.
    void testModulationReachesAudio()
    {
        Processor plain;
        const auto plainOutput = renderSine (plain, 440.0, 0.3f, 60);

        Processor modulated;
        setLfo (modulated, 0, 2.0f, 3.0f, false, 4.0f);                       // square, 2 Hz
        setRoute (modulated, 0, 0, (int) Processor::ModTarget::Drive, 100.0f);
        const auto modulatedOutput = renderSine (modulated, 440.0, 0.3f, 60);

        assert (plainOutput.size() == modulatedOutput.size());

        double difference = 0.0;
        for (size_t n = 0; n < plainOutput.size(); ++n)
            difference += std::abs (plainOutput[n] - modulatedOutput[n]);

        assert (difference / (double) plainOutput.size() > 1.0e-3);

        // The same routing with no amount is bit-identical to no routing.
        Processor idle;
        setLfo (idle, 0, 2.0f, 3.0f, false, 4.0f);
        setRoute (idle, 0, 0, (int) Processor::ModTarget::Drive, 0.0f);
        const auto idleOutput = renderSine (idle, 440.0, 0.3f, 60);
        assert (idleOutput == plainOutput);

        pass ("testModulationReachesAudio");
    }

    // A synced LFO runs on the tempo clock: the internal 120 BPM with no host, the host tempo when there is one.
    void testSyncedLfoFollowsTempo()
    {
        auto phaseAfterBlocks = [] (double hostBpm, int blocks)
        {
            Processor processor;
            FakePlayHead playhead;
            playhead.bpm = hostBpm;
            processor.setPlayHead (&playhead);

            setLfo (processor, 0, 0.25f, 0.0f, true, 4.0f);   // sine, 1/4 note = one beat per cycle
            processor.prepareToPlay (sampleRate, blockSize);

            juce::AudioBuffer<float> audio (2, blockSize);
            juce::MidiBuffer midi;

            for (int block = 0; block < blocks; ++block)
            {
                audio.clear();
                midi.clear();
                processor.processBlock (audio, midi);
            }

            processor.setPlayHead (nullptr);
            return processor.getRouter().getLfoPhase (0);
        };

        // The published phase describes the start of the last block.
        const double elapsed = 47.0 * blockSize / sampleRate;   // 48 blocks processed

        const float at120 = phaseAfterBlocks (120.0, 48);       // two beats per second, one beat per cycle
        const float at60 = phaseAfterBlocks (60.0, 48);         // half the speed

        assert (std::abs (at120 - (float) (2.0 * elapsed - std::floor (2.0 * elapsed))) < 1.0e-3f);
        assert (std::abs (at60 - (float) (elapsed - std::floor (elapsed))) < 1.0e-3f);

        pass ("testSyncedLfoFollowsTempo");
    }

    // Every routable control stays inside its own range, whatever the LFO does.
    void testModulationStaysInRange()
    {
        for (const auto& entry : targetEntries)
        {
            Processor processor;
            setLfo (processor, 0, 3.0f, 3.0f, false, 4.0f);                  // square, 3 Hz
            setRoute (processor, 0, 0, (int) entry.target, 100.0f);
            processor.prepareToPlay (sampleRate, blockSize);

            auto* parameter = processor.apvts.getParameter (entry.id);
            assert (parameter != nullptr);
            const auto range = parameter->getNormalisableRange();

            juce::AudioBuffer<float> audio (2, blockSize);
            juce::MidiBuffer midi;
            float smallest = range.end;
            float largest = range.start;

            for (int block = 0; block < 120; ++block)
            {
                audio.clear();
                midi.clear();
                processor.processBlock (audio, midi);

                const float value = processor.getModulatedValue (entry.target);
                assert (std::isfinite (value));
                smallest = juce::jmin (smallest, value);
                largest = juce::jmax (largest, value);
            }

            assert (smallest >= range.start - 1.0e-3f);
            assert (largest <= range.end + 1.0e-3f);
        }

        pass ("testModulationStaysInRange");
    }

    // Both editor pages paint, and switching pages does not change a parameter.
    void testEditorPages()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        assert (editor != nullptr);

        auto* saturationEditor = dynamic_cast<DynamicSaturationEditor*> (editor.get());
        assert (saturationEditor != nullptr);

        juce::MessageManager::getInstance()->runDispatchLoopUntil (120);

        const float driveBefore = getParameter (processor.apvts, Processor::driveId);
        const float amountBefore = getParameter (processor.apvts, Processor::modAmountIds[0]);

        // A page must draw content of its own. Compare the page as shown with the same page with every
        // child hidden. The difference is what the page's controls draw, so a blank page gives zero.
        auto inkOnPage = [&editor, saturationEditor] (int page)
        {
            saturationEditor->setPage (page);
            auto shown = PluginTest::paintComponent (*editor);

            for (int i = 0; i < editor->getNumChildComponents(); ++i)
                editor->getChildComponent (i)->setVisible (false);

            auto bare = PluginTest::paintComponent (*editor);

            // Show every child again, then apply the page's own visibility.
            for (int i = 0; i < editor->getNumChildComponents(); ++i)
                editor->getChildComponent (i)->setVisible (true);
            saturationEditor->setPage (page);

            return PluginTest::countChangedPixels (shown, bare);
        };

        assert (saturationEditor->getPage() == 0);
        assert (inkOnPage (0) > 500);
        assert (saturationEditor->getPage() == 0);

        assert (inkOnPage (1) > 500);
        assert (saturationEditor->getPage() == 1);

        saturationEditor->setPage (0);
        assert (saturationEditor->getPage() == 0);

        // Showing the pages must not touch the parameters.
        assert (getParameter (processor.apvts, Processor::driveId) == driveBefore);
        assert (getParameter (processor.apvts, Processor::modAmountIds[0]) == amountBefore);

        processor.editorBeingDeleted (editor.get());
        pass ("testEditorPages");
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::puts ("Running Dynamic Saturation plugin tests...");

    testParameters();
    testStateRoundTrip();
    testLatencyMatchesOversampler();
    testSilenceStaysSilent();
    testOutputIsBounded();
    testModesDiffer();
    testMixZeroIsDelayedDry();
    testStereoLinkDrivesQuietChannelHarder();
    testFoldChangesSound();
    testEditor();
    testModulationParameters();
    testDefaultRoutingIsSilent();
    testModulationDirection();
    testModulationReachesAudio();
    testSyncedLfoFollowsTempo();
    testModulationStaysInRange();
    testEditorPages();

    std::puts ("All Dynamic Saturation plugin tests passed.");
    return 0;
}
