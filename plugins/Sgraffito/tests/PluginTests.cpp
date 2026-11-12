// Plugin-level tests for Sgraffito. Compiles the real processor and editor sources.

#include "PluginTestUtils.h"
#include "ModulationTestUtils.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    using namespace PluginTest;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    using Processor = SgraffitoProcessor;

    void testParameters()
    {
        Processor processor;

        // 71 parameters total (as defined in numControls).
        checkParameterIds (processor, processor.apvts,
                           { Processor::modeId, Processor::levelId, Processor::driftAmountId,
                             Processor::lfo1RateId, Processor::lfo1ShapeId, Processor::lfo1SyncId,
                             Processor::lfo1DivisionId, Processor::lfo1OffsetId,
                             Processor::lfo2RateId, Processor::lfo2ShapeId, Processor::lfo2SyncId,
                             Processor::lfo2DivisionId, Processor::lfo2OffsetId,
                             Processor::modSourceIds[0], Processor::modSourceIds[5],
                             Processor::modTargetIds[0], Processor::modTargetIds[5],
                             Processor::modAmountIds[0], Processor::modAmountIds[5],
                             Processor::modDelayIds[0], Processor::modDelayIds[5] },
                           Processor::numControls);
        checkRange (processor.apvts, Processor::modeId, 0.0f, 3.0f, 0.0f);
        checkRange (processor.apvts, Processor::levelId, -60.0f, 6.0f, 0.0f);
        checkRange (processor.apvts, Processor::driftAmountId, 0.0f, 100.0f, 0.0f);
        checkRange (processor.apvts, Processor::memoryAmountId, 0.0f, 100.0f, 20.0f);
        pass ("testParameters");
    }

    void testStateRoundTrip()
    {
        Processor processor;

        checkStateRoundTrip (processor, processor.apvts, Processor::levelId, 6.0f);
        pass ("testStateRoundTrip");
    }

    void testMidiNoteOnAllocatesVoice()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);

        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();
        processor.processBlock (buffer, midi);

        assert (processor.voicePool.countActive() > 0);
        pass ("testMidiNoteOnAllocatesVoice");
    }

    void testMidiNoteOffReleasesVoice()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::MidiBuffer midiOn;
        midiOn.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);

        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();
        processor.processBlock (buffer, midiOn);
        assert (processor.voicePool.countActive() > 0);

        // Set tape stop time to 0 so note-off releases without a tape spin-down.
        setParameter (processor.apvts, Processor::tapeStopTimeId, 0.0f);

        juce::MidiBuffer midiOff;
        midiOff.addEvent (juce::MidiMessage::noteOff (1, 60, 0.8f), 0);
        processor.processBlock (buffer, midiOff);

        // A released voice keeps playing its tail until its region has played out.
        juce::MidiBuffer noMidi;

        for (int block = 0; block < 40 && processor.voicePool.countActive() > 0; ++block)
        {
            buffer.clear();
            processor.processBlock (buffer, noMidi);
        }

        assert (processor.voicePool.countActive() == 0);
        pass ("testMidiNoteOffReleasesVoice");
    }

    // Swing (plan 05.4): the first note of a key plays straight, the next one is swung and
    // starts half a beat later (swing 75 % at the fallback tempo of 120 BPM = 12000 samples).
    void testSwingDelaysRepeatedNotes()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameter (processor.apvts, Processor::swingId, 75.0f);
        setParameter (processor.apvts, Processor::humaniseId, 0.0f);
        setParameter (processor.apvts, Processor::layBackId, 0.0f);
        setParameter (processor.apvts, Processor::regionLengthId, 100.0f);

        juce::AudioBuffer<float> testBuffer (2, 48000);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 48000; ++i)
                testBuffer.setSample (ch, i,
                    0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / sampleRate));
        processor.loadedSample = std::make_unique<LoadedSample>();
        processor.loadedSample->buffer = testBuffer;
        processor.loadedSample->sourceSampleRate = sampleRate;
        processor.sampleBuffer.store (&processor.loadedSample->buffer, std::memory_order_release);

        juce::AudioBuffer<float> audio (2, blockSize);
        audio.clear();

        // First note of key 60: swing is 75 but the first note of a phrase is straight.
        juce::MidiBuffer midi1;
        midi1.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        processor.processBlock (audio, midi1);

        int firstVoice = -1;
        for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
            if (processor.voicePool.voices[i].active && processor.voicePool.voices[i].key == 60)
            {
                firstVoice = i;
                break;
            }
        assert (firstVoice >= 0);
        const double firstOnset = processor.voicePool.voices[firstVoice].envStart;
        assert (firstOnset <= 0.0);

        // Second note of key 60: swung, queued with the swing delay instead of playing straight.
        juce::MidiBuffer midi2;
        midi2.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        processor.processBlock (audio, midi2);
        assert (processor.pendingNotes.size () == 1);
        assert (processor.pendingNotes[0].key == 60);
        assert (std::abs (processor.pendingNotes[0].delaySamples - 12000.0) < 1.0);

        // The swung note fires, late, at the delayed position (stream 1024 + the 12000-sample
        // swing delay = 12512), and the queue drains.
        juce::MidiBuffer noMidi;
        for (int block = 0; block < 40; ++block)
        {
            audio.clear();
            processor.processBlock (audio, noMidi);
        }

        assert (processor.pendingNotes.empty ());

        bool firedWithDelay = false;
        for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
            if (processor.voicePool.voices[i].active && processor.voicePool.voices[i].key == 60
                && std::abs (processor.voicePool.voices[i].envStart - 12512.0) < 1.0)
                firedWithDelay = true;

        assert (firedWithDelay);
        pass ("testSwingDelaysRepeatedNotes");
    }

    // Humanise (plan 05.4): every note gets a deterministic start offset before the (zero)
    // lay-back, so the note is queued and starts inside the humanise window.
    void testHumaniseDelaysNoteOnset()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameter (processor.apvts, Processor::swingId, 50.0f);
        setParameter (processor.apvts, Processor::humaniseId, 10.0f);
        setParameter (processor.apvts, Processor::layBackId, 0.0f);
        setParameter (processor.apvts, Processor::regionLengthId, 100.0f);

        juce::AudioBuffer<float> testBuffer (2, 48000);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 48000; ++i)
                testBuffer.setSample (ch, i,
                    0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / sampleRate));
        processor.loadedSample = std::make_unique<LoadedSample>();
        processor.loadedSample->buffer = testBuffer;
        processor.loadedSample->sourceSampleRate = sampleRate;
        processor.sampleBuffer.store (&processor.loadedSample->buffer, std::memory_order_release);

        juce::AudioBuffer<float> audio (2, blockSize);
        audio.clear();
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        processor.processBlock (audio, midi);

        // The humanise offset (10 ms at 48 kHz, deterministic per note) makes the delay small
        // enough to sit inside one block, so the queued note fires within this same block and
        // the queue drains. The voice must still start late, at the humanise offset.
        assert (processor.pendingNotes.empty ());

        int voice = -1;
        for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
            if (processor.voicePool.voices[i].active && processor.voicePool.voices[i].key == 60)
            {
                voice = i;
                break;
            }
        assert (voice >= 0);
        const double humaniseDelaySamples = processor.voicePool.voices[voice].envStart;
        // First note of the phrase, swing straight (50 %), lay-back off: the humanise offset is
        // unit * 10 ms = 0.3363 * 10 * 48 kHz = ~161 samples. Before the fix the humanise delay
        // was ignored and this onset sat at sample 0.
        assert (humaniseDelaySamples > 0.0);
        assert (std::abs (humaniseDelaySamples - 161.42) < 1.0);
        pass ("testHumaniseDelaysNoteOnset");
    }

    // Ghost notes (plan 05.4 Phase 2): a quiet note below the threshold becomes a quieter,
    // delayed copy; the ghost allocates a real voice when its delayed copy comes due.
    void testGhostDelaysQuietNote()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameter (processor.apvts, Processor::swingId, 50.0f);
        setParameter (processor.apvts, Processor::humaniseId, 0.0f);
        setParameter (processor.apvts, Processor::layBackId, 0.0f);
        setParameter (processor.apvts, Processor::regionLengthId, 100.0f);
        setParameter (processor.apvts, Processor::ghostOnId, 1.0f);
        setParameter (processor.apvts, Processor::ghostThresholdId, 50.0f);
        setParameter (processor.apvts, Processor::ghostLevelId, -12.0f);
        setParameter (processor.apvts, Processor::ghostDelayId, 10.0f);

        juce::AudioBuffer<float> testBuffer (2, 48000);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 48000; ++i)
                testBuffer.setSample (ch, i,
                    0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / sampleRate));
        processor.loadedSample = std::make_unique<LoadedSample>();
        processor.loadedSample->buffer = testBuffer;
        processor.loadedSample->sourceSampleRate = sampleRate;
        processor.sampleBuffer.store (&processor.loadedSample->buffer, std::memory_order_release);

        juce::AudioBuffer<float> audio (2, blockSize);
        audio.clear();
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.3f), 0);   // below the 50 % threshold
        processor.processBlock (audio, midi);

        // The ghost delay (10 ms = 480 samples) is shorter than one block, so the ghost fired
        // inside this block; the main note also played, so the queue drains and two voices sound.
        assert (processor.pendingNotes.empty ());

        int activeCount = 0;
        int ghostVoice = -1;
        for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
        {
            const auto& voice = processor.voicePool.voices[i];
            if (! voice.active)
                continue;
            ++activeCount;
            if (voice.envStart > 0.0 && voice.key == 60)
                ghostVoice = i;
        }
        assert (activeCount == 2);   // the main note and its ghost
        assert (ghostVoice >= 0);
        // The ghost started ghostDelay after the note, and carries the ghost level in its velocity.
        assert (std::abs (processor.voicePool.voices[ghostVoice].envStart - 480.0) < 1.0);
        const float expectedGhostVelocity = 0.3f * std::pow (10.0, -12.0 / 20.0);
        assert (std::abs (processor.voicePool.voices[ghostVoice].velocity - expectedGhostVelocity) < 1e-3);
        pass ("testGhostDelaysQuietNote");
    }

    // Ghosts need the switch on and the note below the threshold; with the velocity at or above
    // the threshold no ghost is spawned and only the main note plays.
    void testNoGhostWithoutQuietNote()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameter (processor.apvts, Processor::swingId, 50.0f);
        setParameter (processor.apvts, Processor::humaniseId, 0.0f);
        setParameter (processor.apvts, Processor::layBackId, 0.0f);
        setParameter (processor.apvts, Processor::regionLengthId, 100.0f);
        setParameter (processor.apvts, Processor::ghostOnId, 1.0f);
        setParameter (processor.apvts, Processor::ghostThresholdId, 50.0f);
        setParameter (processor.apvts, Processor::ghostLevelId, -12.0f);
        setParameter (processor.apvts, Processor::ghostDelayId, 10.0f);

        juce::AudioBuffer<float> testBuffer (2, 48000);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 48000; ++i)
                testBuffer.setSample (ch, i,
                    0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / sampleRate));
        processor.loadedSample = std::make_unique<LoadedSample>();
        processor.loadedSample->buffer = testBuffer;
        processor.loadedSample->sourceSampleRate = sampleRate;
        processor.sampleBuffer.store (&processor.loadedSample->buffer, std::memory_order_release);

        juce::AudioBuffer<float> audio (2, blockSize);
        audio.clear();
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);   // at the threshold, not below
        processor.processBlock (audio, midi);

        assert (processor.pendingNotes.empty ());

        int activeCount = 0;
        for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
            if (processor.voicePool.voices[i].active)
                ++activeCount;

        assert (activeCount == 1);   // main note only, no ghost
        pass ("testNoGhostWithoutQuietNote");
    }

    void testEditor()
    {
        Processor processor;
        checkEditor (processor);
        pass ("testEditor");
    }

    // When SUITE_RENDER_DIR is set, writes a PNG of each editor page and checks the
    // two renders of the same view match within one process.
    void testRenderBaseline()
    {
        const auto folder = juce::SystemStats::getEnvironmentVariable ("SUITE_RENDER_DIR", {});

        if (folder.isEmpty())
        {
            std::printf ("[SKIP] editor renders: set SUITE_RENDER_DIR to write them\n");
            return;
        }

        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        for (int renderPage = 0; renderPage < 3; ++renderPage)
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
            assert (editor != nullptr);
            static_cast<SgraffitoEditor*> (editor.get())->setPage (renderPage);

            const juce::File file (juce::File (folder).getChildFile (
                renderPage == 0 ? "sgraffito-sound.png"
                                : renderPage == 1 ? "sgraffito-texture.png" : "sgraffito-mod.png"));
            assert (PluginTest::writeDeterministicPng (*editor, file));
            processor.editorBeingDeleted (editor.get());
        }

        pass ("testRenderBaseline");
    }

    //==============================================================================
    // LFOs and modulation routing

    /** The routable controls, in ModTarget order, with the parameter each one moves. */
    struct TargetEntry { Processor::ModTarget target; const char* id; };

    const TargetEntry targetEntries[] {
        { Processor::ModTarget::RegionSpan,     Processor::regionSpanId },
        { Processor::ModTarget::RegionLength,   Processor::regionLengthId },
        { Processor::ModTarget::RootKey,        Processor::rootKeyId },
        { Processor::ModTarget::PitchFine,      Processor::pitchFineId },
        { Processor::ModTarget::Glide,          Processor::glideId },
        { Processor::ModTarget::Level,          Processor::levelId },
        { Processor::ModTarget::Attack,         Processor::attackId },
        { Processor::ModTarget::Release,        Processor::releaseId },
        { Processor::ModTarget::MemoryAmount,   Processor::memoryAmountId },
        { Processor::ModTarget::DriftAmount,    Processor::driftAmountId },
        { Processor::ModTarget::DriftSpeed,     Processor::driftSpeedId },
        { Processor::ModTarget::LayBack,        Processor::layBackId },
        { Processor::ModTarget::Swing,          Processor::swingId },
        { Processor::ModTarget::Humanise,       Processor::humaniseId },
        { Processor::ModTarget::GhostThreshold, Processor::ghostThresholdId },
        { Processor::ModTarget::GhostLevel,     Processor::ghostLevelId },
        { Processor::ModTarget::GrainSize,      Processor::grainSizeId },
        { Processor::ModTarget::GrainDensity,   Processor::grainDensityId },
        { Processor::ModTarget::SwellTime,      Processor::swellTimeId },
        { Processor::ModTarget::Dust,           Processor::dustId },
        { Processor::ModTarget::Crackle,        Processor::crackleId },
        { Processor::ModTarget::Wow,            Processor::wowId },
        { Processor::ModTarget::Flutter,        Processor::flutterId },
        { Processor::ModTarget::ShuffleSeed,    Processor::shuffleSeedId },
    };

    /** Runs the processor with a held note over a one-second sine sample and returns the left output. */
    std::vector<float> renderNote (Processor& processor, int blocks, int key = 60)
    {
        juce::AudioBuffer<float> testBuffer (2, 48000);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 48000; ++i)
                testBuffer.setSample (ch, i,
                    0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / sampleRate));

        processor.loadedSample = std::make_unique<LoadedSample>();
        processor.loadedSample->buffer = testBuffer;
        processor.loadedSample->sourceSampleRate = sampleRate;
        processor.sampleBuffer.store (&processor.loadedSample->buffer, std::memory_order_release);

        // The whole sample is the region, so the note plays for the whole render.
        setParameter (processor.apvts, Processor::regionLengthId, 100.0f);

        processor.prepareToPlay (sampleRate, blockSize);

        std::vector<float> output;

        for (int block = 0; block < blocks; ++block)
        {
            juce::AudioBuffer<float> audio (2, blockSize);
            audio.clear();
            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, key, 0.8f), 0);

            processor.processBlock (audio, midi);

            for (int n = 0; n < blockSize; ++n)
                output.push_back (audio.getSample (0, n));
        }

        return output;
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
        checkRange (processor.apvts, Processor::modTargetIds[0], 0.0f, 24.0f, 0.0f);
        checkRange (processor.apvts, Processor::modAmountIds[0], -100.0f, 100.0f, 0.0f);
        checkRange (processor.apvts, Processor::modDelayIds[0], 0.0f, 10.0f, 0.0f);

        // The target choices must match the enum order the sessions save.
        auto* target = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (Processor::modTargetIds[0]));
        assert (target != nullptr);
        assert (target->choices == Processor::modTargetNames());
        assert (target->choices.size() == (int) Processor::ModTarget::count);

        assert (processor.getRouter().getNumTargets() == (int) Processor::ModTarget::count - 1);

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
        setRoute (routed, 0, 0, (int) Processor::ModTarget::Level, 0.0f);

        const auto plainOutput = renderNote (processor, 40);
        const auto routedOutput = renderNote (routed, 40);

        assert (plainOutput == routedOutput);

        pass ("testDefaultRoutingIsSilent");
    }

    // The routing reaches the audio: a routed level changes the output, and removing the amount restores it.
    void testModulationReachesAudio()
    {
        Processor plain;
        const auto plainOutput = renderNote (plain, 40);

        Processor modulated;
        setLfo (modulated, 0, 2.0f, 3.0f, false, 4.0f);   // square, 2 Hz
        setRoute (modulated, 0, 0, (int) Processor::ModTarget::Level, 100.0f);
        const auto modulatedOutput = renderNote (modulated, 40);

        assert (plainOutput.size() == modulatedOutput.size());

        double difference = 0.0;
        for (size_t n = 0; n < plainOutput.size(); ++n)
            difference += std::abs (plainOutput[n] - modulatedOutput[n]);

        assert (difference / (double) plainOutput.size() > 1.0e-3);

        // The same routing with no amount is bit-identical to no routing.
        Processor idle;
        setLfo (idle, 0, 2.0f, 3.0f, false, 4.0f);
        setRoute (idle, 0, 0, (int) Processor::ModTarget::Level, 0.0f);
        const auto idleOutput = renderNote (idle, 40);
        assert (idleOutput == plainOutput);

        pass ("testModulationReachesAudio");
    }

    // Every routable control stays inside its own range, whatever the LFO does.
    void testModulationStaysInRange()
    {
        for (const auto& entry : targetEntries)
        {
            Processor processor;
            setLfo (processor, 0, 3.0f, 3.0f, false, 4.0f);   // square, 3 Hz
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

    void testRegionMapping()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        // Set full span: key 0 starts at 0, key 127 ends at 1.0
        setParameter (processor.apvts, Processor::regionSpanId, 100.0f);
        setParameter (processor.apvts, Processor::regionLengthId, 25.0f);

        juce::MidiBuffer midiOn;
        midiOn.addEvent (juce::MidiMessage::noteOn (1, 0, 0.8f), 0);

        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();
        processor.processBlock (buffer, midiOn);

        // Key 0 should start at region start 0
        const auto& voice0 = processor.voicePool.voices[0];
        assert (voice0.active);
        assert (std::abs (voice0.regionStart - 0.0) < 1.0);
        assert (voice0.regionEnd > voice0.regionStart);

        // Key 127 should end at region end near sample length
        juce::MidiBuffer midiOn2;
        midiOn2.addEvent (juce::MidiMessage::noteOn (1, 127, 0.8f), 0);
        processor.processBlock (buffer, midiOn2);

        // Find the voice for key 127. It may already have finished (key 127 plays at ~48x
        // rate since plan 05.1), but its region fields persist in the slot.
        bool found = false;
        for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
        {
            if (processor.voicePool.voices[i].key == 127)
            {
                // With 25% length starting at position ~0.99, end should be > start
                assert (processor.voicePool.voices[i].regionEnd > processor.voicePool.voices[i].regionStart);
                found = true;
                break;
            }
        }
        assert (found);
        pass ("testRegionMapping");
    }

    void testVoiceProducesOutput()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        // Create a simple test buffer (sine wave)
        juce::AudioBuffer<float> testBuffer (2, 48000);
        for (int ch = 0; ch < 2; ++ch)
        {
            float* dst = testBuffer.getWritePointer (ch);
            for (int i = 0; i < 48000; ++i)
                dst[i] = 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f * i / 48000.0f);
        }

        // Load the test buffer directly into the processor
        processor.loadedSample = std::make_unique<LoadedSample>();
        processor.loadedSample->buffer = testBuffer;
        processor.loadedSample->sourceSampleRate = sampleRate;
        processor.sampleBuffer.store (&processor.loadedSample->buffer, std::memory_order_release);

        juce::MidiBuffer midiOn;
        midiOn.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);

        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();
        processor.processBlock (buffer, midiOn);

        // Output should be non-zero (voice is playing)
        float peak = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int i = 0; i < blockSize; ++i)
                peak = std::max (peak, std::abs (buffer.getSample (ch, i)));
        }

        assert (peak > 0.01f);
        pass ("testVoiceProducesOutput");
    }

    void testChopModeRegionMapping()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        // Set Chop mode (mode 1) with 4 slices
        setParameter (processor.apvts, Processor::modeId, 1.0f);
        setParameter (processor.apvts, Processor::sliceCountId, 4.0f);
        setParameter (processor.apvts, Processor::regionSpanId, 100.0f);

        // Key 0 should map to slice 0 (first quarter)
        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();

        juce::MidiBuffer midi0;
        midi0.addEvent (juce::MidiMessage::noteOn (1, 0, 0.8f), 0);
        processor.processBlock (buffer, midi0);
        
        // Find voice for key 0
        bool found0 = false;
        for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
        {
            if (processor.voicePool.voices[i].active && processor.voicePool.voices[i].key == 0)
            {
                // Slice 0 should start at 0.0
                assert (std::abs (processor.voicePool.voices[i].regionStart - 0.0) < 1e-6);
                // Slice 0 should end at 0.25 of sample length
                assert (std::abs (processor.voicePool.voices[i].regionEnd - 0.25) < 1e-6);
                found0 = true;
                break;
            }
        }
        assert (found0);
        pass ("testChopModeRegionMapping");
    }

    void testChopModeSliceCoverage()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        // Set Chop mode with 8 slices
        setParameter (processor.apvts, Processor::modeId, 1.0f);
        setParameter (processor.apvts, Processor::sliceCountId, 8.0f);
        setParameter (processor.apvts, Processor::regionSpanId, 100.0f);

        // Each key should map to a different slice
        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();

        double prevRegionStart = -1.0;
        bool slicesCovered = true;

        for (int key = 0; key < 128; key += 16)
        {
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, key, 0.8f), 0);
            processor.processBlock (buffer, midi);

            bool foundKey = false;
            for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
            {
                // The voice may already have finished (high keys play at high rates), but
                // its region fields persist in the slot.
                if (processor.voicePool.voices[i].key == key)
                {
                    foundKey = true;
                    if (processor.voicePool.voices[i].regionStart < prevRegionStart - 1e-6)
                    {
                        slicesCovered = false;
                    }
                    prevRegionStart = processor.voicePool.voices[i].regionStart;
                    break;
                }
            }
            assert (foundKey);
        }
        assert (slicesCovered);
        pass ("testChopModeSliceCoverage");
    }

    void testModesProduceDifferentAudio()
    {
        // The owner reported "nearly always the same" sound: the four modes must produce
        // different output (plan 05.1).
        const int key = 60;

        std::vector<float> outputs[4];

        for (int mode = 0; mode < 4; ++mode)
        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            setParameter (processor.apvts, Processor::modeId, (float) mode);

            // A plain sine, so every mode's region reads loud audio (a decaying sample
            // would make Swell's backwards read start near silence).
            juce::AudioBuffer<float> testBuffer (2, 48000);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 48000; ++i)
                    testBuffer.setSample (ch, i,
                        0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / sampleRate));

            processor.loadedSample = std::make_unique<LoadedSample>();
            processor.loadedSample->buffer = testBuffer;
            processor.loadedSample->sourceSampleRate = sampleRate;
            processor.sampleBuffer.store (&processor.loadedSample->buffer, std::memory_order_release);

            outputs[mode] = renderNote (processor, 24);

            for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
                if (processor.voicePool.voices[i].key == key)
                {
                    std::printf ("mode %d: region %.4f - %.4f, rate %.3f, pos %.1f, envStart %.1f, gain %.4f, vel %.3f\n",
                                 mode,
                                 processor.voicePool.voices[i].regionStart,
                                 processor.voicePool.voices[i].regionEnd,
                                 processor.voicePool.voices[i].rate,
                                 processor.voicePool.voices[i].position,
                                 processor.voicePool.voices[i].envStart,
                                 processor.voicePool.voices[i].gain,
                                 processor.voicePool.voices[i].velocity);
                }
        }

        for (int a = 0; a < 4; ++a)
        {
            double energy = 0.0;
            for (float v : outputs[a])
                energy += std::abs (v);
            std::printf ("mode %d: total |x| = %.3f, at2000:", a, energy);
            for (int n = 0; n < 8; ++n)
                std::printf (" %.4f", outputs[a][(size_t) 2000 + n]);
            std::printf ("\n");
        }

        // What is actually in the processor's sample buffer?
        {
            Processor probe;
            probe.prepareToPlay (sampleRate, blockSize);
            juce::AudioBuffer<float> testBuffer (2, 48000);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 48000; ++i)
                    testBuffer.setSample (ch, i,
                        0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / sampleRate));
            probe.loadedSample = std::make_unique<LoadedSample>();
            probe.loadedSample->buffer = testBuffer;
            probe.loadedSample->sourceSampleRate = sampleRate;
            probe.sampleBuffer.store (&probe.loadedSample->buffer, std::memory_order_release);
            std::printf ("probe buffer: numSamples %d, at2000:", probe.loadedSample->buffer.getNumSamples());
            for (int n = 0; n < 6; ++n)
                std::printf (" %.4f", probe.loadedSample->buffer.getSample (0, 2000 + n));
            std::printf ("\n");
        }

        for (int a = 0; a < 4; ++a)
            for (int b = a + 1; b < 4; ++b)
            {
                double difference = 0.0;
                for (size_t n = 0; n < outputs[a].size(); ++n)
                    difference += std::abs (outputs[a][n] - outputs[b][n]);

                const double average = difference / (double) outputs[a].size();
                std::printf ("modes %d vs %d: avg diff %.6f\n", a, b, average);
                assert (average > 1.0e-3);
            }

        pass ("testModesProduceDifferentAudio");
    }

    void testKeyChangesPitch()
    {
        // The key's distance from middle C must change the playback rate (plan 05.1):
        // a note an octave up reads the sample at twice the rate.
        auto renderAtKey = [] (int key)
        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);

            juce::AudioBuffer<float> testBuffer (2, 48000);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 48000; ++i)
                    testBuffer.setSample (ch, i,
                        0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / sampleRate));

            processor.loadedSample = std::make_unique<LoadedSample>();
            processor.loadedSample->buffer = testBuffer;
            processor.loadedSample->sourceSampleRate = sampleRate;
            processor.sampleBuffer.store (&processor.loadedSample->buffer, std::memory_order_release);

            setParameter (processor.apvts, Processor::regionLengthId, 100.0f);
            return renderNote (processor, 40, key);
        };

        const auto low = renderAtKey (48);
        const auto high = renderAtKey (72);

        // The octave-up note reads the sample at twice the rate: twice the apparent
        // frequency. Count positive-going zero crossings in the first 2048 samples.
        auto crossings = [] (const std::vector<float>& output)
        {
            int count = 0;
            for (size_t n = 1; n < output.size() && n < 2048; ++n)
                if (output[n - 1] <= 0.0f && output[n] > 0.0f)
                    ++count;
            return count;
        };

        const int lowCrossings = crossings (low);
        const int highCrossings = crossings (high);
        std::printf ("pitch: crossings low %d, high %d\n", lowCrossings, highCrossings);
        assert (highCrossings > lowCrossings + 2);
        pass ("testKeyChangesPitch");
    }
}

int main()
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    juce::ScopedJuceInitialiser_GUI gui;

    std::puts ("Running Sgraffito plugin tests...");

    testParameters();
    testStateRoundTrip();
    testMidiNoteOnAllocatesVoice();
    testMidiNoteOffReleasesVoice();
    testSwingDelaysRepeatedNotes();
    testHumaniseDelaysNoteOnset();
    testGhostDelaysQuietNote();
    testNoGhostWithoutQuietNote();
    testRegionMapping();
    testChopModeRegionMapping();
    testChopModeSliceCoverage();
    testVoiceProducesOutput();
    testEditor();
    testRenderBaseline();
    testModulationParameters();
    testDefaultRoutingIsSilent();
    testModulationReachesAudio();
    testModulationStaysInRange();
    testModesProduceDifferentAudio();
    testKeyChangesPitch();

    std::puts ("All Sgraffito plugin tests passed.");
    return 0;
}
