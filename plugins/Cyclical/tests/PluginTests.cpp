// Plugin-level tests for Cyclical. Compiles the real processor and editor sources.

#include "PluginTestUtils.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "SuiteSettings.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <cmath>

#undef NDEBUG
#include <cassert>

namespace
{
    using namespace PluginTest;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    using Processor = CyclicalProcessor;

    /** A controllable playhead for the transport tests (the MidiEffect pattern). */
    struct TestPlayHead : juce::AudioPlayHead
    {
        juce::AudioPlayHead::PositionInfo info;
        juce::Optional<PositionInfo> getPosition() const override { return info; }
    };

    /** A scratch folder under build/, so tests never write outside the project. */
    juce::File scratchFolder()
    {
        auto folder = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                          .getParentDirectory()
                          .getChildFile ("cyclical-test-scratch");

        folder.deleteRecursively();
        folder.createDirectory();
        return folder;
    }

    /** Writes a small mono WAV loop (a ramp, so values name their frame). */
    juce::File writeTestLoop (const juce::String& name, int frames = 24000)
    {
        const juce::File file = scratchFolder().getChildFile (name);
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());

        juce::AudioFormatWriterOptions options;
        options = options.withSampleRate (sampleRate)
                      .withNumChannels (1)
                      .withBitsPerSample (16);

        std::unique_ptr<juce::AudioFormatWriter> writer = wav.createWriterFor (stream, options);
        assert (writer != nullptr);

        juce::AudioBuffer<float> buffer (1, frames);
        for (int i = 0; i < frames; ++i)
            buffer.setSample (0, i, (float) i);

        writer->writeFromAudioSampleBuffer (buffer, 0, frames);
        writer.reset();
        return file;
    }

    /** Like writeTestLoop, but 32-bit float: the sample values keep naming their frame
        past full scale, which the chop tests read back out of the audio. */
    juce::File writeRampLoopFloat (const juce::String& name, int frames = 24000)
    {
        const juce::File file = scratchFolder().getChildFile (name);
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());

        juce::AudioFormatWriterOptions options;
        options = options.withSampleRate (sampleRate)
                      .withNumChannels (1)
                      .withBitsPerSample (32);

        std::unique_ptr<juce::AudioFormatWriter> writer = wav.createWriterFor (stream, options);
        assert (writer != nullptr);

        juce::AudioBuffer<float> buffer (1, frames);
        for (int i = 0; i < frames; ++i)
            buffer.setSample (0, i, (float) i);

        writer->writeFromAudioSampleBuffer (buffer, 0, frames);
        writer.reset();
        return file;
    }

    void allParameterIds (std::vector<const char*>& ids)
    {
        ids.push_back (Processor::masterVolumeId);

        for (int slot = 0; slot < Processor::numSlots; ++slot)
        {
            ids.push_back (Processor::slotStartIds[(size_t) slot]);
            ids.push_back (Processor::slotLengthIds[(size_t) slot]);
            ids.push_back (Processor::slotSpeedIds[(size_t) slot]);
            ids.push_back (Processor::slotDirectionIds[(size_t) slot]);
            ids.push_back (Processor::slotPingRandomIds[(size_t) slot]);
            ids.push_back (Processor::slotGranularIds[(size_t) slot]);
            ids.push_back (Processor::slotVelocityIds[(size_t) slot]);
            ids.push_back (Processor::slotNoteIds[(size_t) slot]);
            ids.push_back (Processor::slotBeatsIds[(size_t) slot]);
            ids.push_back (Processor::slotChopIds[(size_t) slot]);
            ids.push_back (Processor::slotSlicesIds[(size_t) slot]);
            ids.push_back (Processor::slotSliceLengthIds[(size_t) slot]);
            ids.push_back (Processor::slotFilterIds[(size_t) slot]);
            ids.push_back (Processor::slotAttackIds[(size_t) slot]);
            ids.push_back (Processor::slotReleaseIds[(size_t) slot]);
            ids.push_back (Processor::slotLevelIds[(size_t) slot]);
            ids.push_back (Processor::slotCutoffIds[(size_t) slot]);
            ids.push_back (Processor::slotResoIds[(size_t) slot]);
            ids.push_back (Processor::slotQuantizeIds[(size_t) slot]);
            ids.push_back (Processor::slotSwingIds[(size_t) slot]);
            ids.push_back (Processor::slotTriggerIds[(size_t) slot]);
            ids.push_back (Processor::slotPitchIds[(size_t) slot]);
            ids.push_back (Processor::slotCrossfadeIds[(size_t) slot]);
            ids.push_back (Processor::slotTempoIds[(size_t) slot]);
            ids.push_back (Processor::slotSyncIds[(size_t) slot]);
            ids.push_back (Processor::slotPanIds[(size_t) slot]);
        }

        // Plan 18 Phase 6: the shared LFO and routing parameters.
        ids.push_back (Processor::lfo1RateId);
        ids.push_back (Processor::lfo1ShapeId);
        ids.push_back (Processor::lfo1SyncId);
        ids.push_back (Processor::lfo1DivisionId);
        ids.push_back (Processor::lfo1OffsetId);
        ids.push_back (Processor::lfo1InvertId);
        ids.push_back (Processor::lfo1RectifyId);
        ids.push_back (Processor::lfo1QuantiseId);
        ids.push_back (Processor::lfo1SmoothId);
        ids.push_back (Processor::lfo1PulseWidthId);
        ids.push_back (Processor::lfo1SkewId);
        ids.push_back (Processor::lfo2RateId);
        ids.push_back (Processor::lfo2ShapeId);
        ids.push_back (Processor::lfo2SyncId);
        ids.push_back (Processor::lfo2DivisionId);
        ids.push_back (Processor::lfo2OffsetId);
        ids.push_back (Processor::lfo2InvertId);
        ids.push_back (Processor::lfo2RectifyId);
        ids.push_back (Processor::lfo2QuantiseId);
        ids.push_back (Processor::lfo2SmoothId);
        ids.push_back (Processor::lfo2PulseWidthId);
        ids.push_back (Processor::lfo2SkewId);

        for (int slot = 0; slot < 6; ++slot)
        {
            ids.push_back (Processor::modSourceIds[(size_t) slot]);
            ids.push_back (Processor::modTargetIds[(size_t) slot]);
            ids.push_back (Processor::modAmountIds[(size_t) slot]);
            ids.push_back (Processor::modDelayIds[(size_t) slot]);
        }
    }

    void testParameters()
    {
        Processor processor;

        std::vector<const char*> ids;
        allParameterIds (ids);
        assert ((int) ids.size() == Processor::numControls);
        assert ((int) processor.getParameters().size() == Processor::numControls);

        for (const char* id : ids)
            assert (processor.apvts.getParameter (id) != nullptr);

        checkRange (processor.apvts, Processor::masterVolumeId, -24.0f, 12.0f, 0.0f);

        for (int slot = 0; slot < Processor::numSlots; ++slot)
        {
            checkRange (processor.apvts, Processor::slotStartIds[(size_t) slot], 0.0f, 100.0f, 0.0f);
            checkRange (processor.apvts, Processor::slotLengthIds[(size_t) slot], 5.0f, 100.0f, 100.0f);
            checkRange (processor.apvts, Processor::slotSpeedIds[(size_t) slot], 0.25f, 4.0f, 1.0f);
            checkRange (processor.apvts, Processor::slotDirectionIds[(size_t) slot], 0.0f, 2.0f, 0.0f);
            checkRange (processor.apvts, Processor::slotPingRandomIds[(size_t) slot], 0.0f, 1.0f, 0.0f);
            checkRange (processor.apvts, Processor::slotGranularIds[(size_t) slot], 0.0f, 1.0f, 0.0f);
            checkRange (processor.apvts, Processor::slotVelocityIds[(size_t) slot], 30.0f, 150.0f, 100.0f);
            checkRange (processor.apvts, Processor::slotNoteIds[(size_t) slot], 24.0f, 96.0f,
                        (float) (Processor::firstSlotNote + slot));
            checkRange (processor.apvts, Processor::slotBeatsIds[(size_t) slot], 1.0f, 16.0f, 4.0f);
            checkRange (processor.apvts, Processor::slotChopIds[(size_t) slot], 0.0f, 1.0f, 0.0f);
            checkRange (processor.apvts, Processor::slotSlicesIds[(size_t) slot], 2.0f, 16.0f, 8.0f);
            checkRange (processor.apvts, Processor::slotSliceLengthIds[(size_t) slot], -100.0f, 100.0f, 0.0f);
            checkRange (processor.apvts, Processor::slotFilterIds[(size_t) slot], 0.0f, 3.0f, 0.0f);
            checkRange (processor.apvts, Processor::slotAttackIds[(size_t) slot], 1.0f, 1000.0f, 1.0f);
            checkRange (processor.apvts, Processor::slotReleaseIds[(size_t) slot], 1.0f, 10000.0f, 5.0f);
            checkRange (processor.apvts, Processor::slotLevelIds[(size_t) slot], 0.0f, 200.0f, 100.0f);

            // Plan 18 Phase 4: the former globals, per slot, plus the filter's cutoff/reso.
            checkRange (processor.apvts, Processor::slotCutoffIds[(size_t) slot], 20.0f, 20000.0f, 20000.0f);
            checkRange (processor.apvts, Processor::slotResoIds[(size_t) slot], 0.5f, 10.0f, 0.7f);
            checkRange (processor.apvts, Processor::slotQuantizeIds[(size_t) slot], 0.0f, 5.0f, 0.0f);
            checkRange (processor.apvts, Processor::slotSwingIds[(size_t) slot], -20.0f, 20.0f, 0.0f);
            checkRange (processor.apvts, Processor::slotTriggerIds[(size_t) slot], 0.0f, 2.0f, 0.0f);
            checkRange (processor.apvts, Processor::slotPitchIds[(size_t) slot], -12.0f, 12.0f, 0.0f);
            checkRange (processor.apvts, Processor::slotCrossfadeIds[(size_t) slot], 0.0f, 500.0f, 5.0f);
            checkRange (processor.apvts, Processor::slotTempoIds[(size_t) slot], 40.0f, 240.0f, 120.0f);
            checkRange (processor.apvts, Processor::slotSyncIds[(size_t) slot], 0.0f, 1.0f, 0.0f);
            checkRange (processor.apvts, Processor::slotPanIds[(size_t) slot], -100.0f, 100.0f, 0.0f);
        }

        // Plan 18 Phase 6: the shared LFO/routing parameters and the 16 per-slot targets.
        checkRange (processor.apvts, Processor::lfo1RateId, 0.02f, 20.0f, 0.25f);
        checkRange (processor.apvts, Processor::lfo1OffsetId, -100.0f, 100.0f, 0.0f);
        checkRange (processor.apvts, Processor::lfo1QuantiseId, 0.0f, 16.0f, 0.0f);
        checkRange (processor.apvts, Processor::modTargetIds[0], 0.0f, 16.0f, 0.0f);
        checkRange (processor.apvts, Processor::modAmountIds[0], -100.0f, 100.0f, 0.0f);
        assert (processor.getRouter().getNumTargets() == 16);

        // The final gain stage is named "Output" everywhere in the suite (plan 12).
        assert (processor.apvts.getParameter (Processor::masterVolumeId)->getName (100) == "Output");
        pass ("testParameters");
    }

    void testStateRoundTrip()
    {
        Processor processor;

        checkStateRoundTrip (processor, processor.apvts, Processor::masterVolumeId, -6.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::slotPanIds[0], 40.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::slotQuantizeIds[0], 2.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::slotSwingIds[0], 12.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::slotTriggerIds[0], 1.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::slotPitchIds[0], 5.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::slotCrossfadeIds[0], 120.0f);

        for (int slot = 0; slot < Processor::numSlots; ++slot)
        {
            checkStateRoundTrip (processor, processor.apvts, Processor::slotStartIds[(size_t) slot], 20.0f);
            checkStateRoundTrip (processor, processor.apvts, Processor::slotLengthIds[(size_t) slot], 60.0f);
            checkStateRoundTrip (processor, processor.apvts, Processor::slotSpeedIds[(size_t) slot], 2.0f);
            checkStateRoundTrip (processor, processor.apvts, Processor::slotDirectionIds[(size_t) slot], 1.0f);
            checkStateRoundTrip (processor, processor.apvts, Processor::slotPingRandomIds[(size_t) slot], 1.0f);
            checkStateRoundTrip (processor, processor.apvts, Processor::slotGranularIds[(size_t) slot], 1.0f);
            checkStateRoundTrip (processor, processor.apvts, Processor::slotVelocityIds[(size_t) slot], 80.0f);
            checkStateRoundTrip (processor, processor.apvts, Processor::slotFilterIds[(size_t) slot], 0.0f);
            checkStateRoundTrip (processor, processor.apvts, Processor::slotAttackIds[(size_t) slot], 100.0f);
            checkStateRoundTrip (processor, processor.apvts, Processor::slotReleaseIds[(size_t) slot], 1000.0f);
            checkStateRoundTrip (processor, processor.apvts, Processor::slotLevelIds[(size_t) slot], 100.0f);
        }

        pass ("testStateRoundTrip");
    }

    /** A state from a newer plugin changes nothing. */
    void testNewerStateRefused()
    {
        Processor processor;
        setParameter (processor.apvts, Processor::masterVolumeId, -6.0f);

        juce::MemoryBlock state;
        processor.getStateInformation (state);
        auto xml = SuiteState::detail::decodeXml (state.getData(), (int) state.getSize());
        assert (xml != nullptr);
        xml->setAttribute (SuiteState::versionProperty, 99);

        const juce::MemoryBlock newer = SuiteState::detail::encodeXml (*xml);
        processor.setStateInformation (newer.getData(), (int) newer.getSize());

        assert (std::abs (getParameter (processor.apvts, Processor::masterVolumeId) - (-6.0f)) < 1.0e-3f);
        pass ("testNewerStateRefused");
    }

    /** A state with no parameters at all loads with everything at its default, and its
        loop references still resolve (a state may carry only the loop properties). */
    void testMinimalStateLoad()
    {
        Processor processor;
        setParameter (processor.apvts, Processor::masterVolumeId, -6.0f);

        juce::MemoryBlock state;
        processor.getStateInformation (state);
        auto xml = SuiteState::detail::decodeXml (state.getData(), (int) state.getSize());
        auto tree = juce::ValueTree::fromXml (*xml);

        for (int i = tree.getNumChildren() - 1; i >= 0; --i)
            tree.removeChild (tree.getChild (i), nullptr);

        const juce::MemoryBlock minimal = SuiteState::detail::encodeXml (*tree.createXml());
        processor.setStateInformation (minimal.getData(), (int) minimal.getSize());

        assert (std::abs (getParameter (processor.apvts, Processor::masterVolumeId) - 0.0f) < 1.0e-6f);

        // The loop properties survived (they are attributes, not parameters), so all four
        // slots still hold their embedded defaults.
        for (int slot = 0; slot < Processor::numSlots; ++slot)
            assert (processor.loopPath (slot).startsWith ("embedded:"));

        pass ("testMinimalStateLoad");
    }

    /** Processes one block with the given MIDI buffer. */
    void processOne (Processor& processor, juce::MidiBuffer& midi, int channels = 2)
    {
        juce::AudioBuffer<float> buffer (channels, blockSize);
        buffer.clear();
        processor.processBlock (buffer, midi);
    }

    /** Default state, no notes: exact silence (the instrument fills from scratch). */
    void testDefaultSilence()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();
        for (int i = 0; i < 4; ++i)
        {
            addSine (buffer, 0, sampleRate, 440.0 + i, 0.5f, i * blockSize);   // host noise in
            addSine (buffer, 1, sampleRate, 880.0, 0.5f, i * blockSize);
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);
            assert (peak (buffer) == 0.0f);       // exact silence: the buffer was cleared
        }
        pass ("testDefaultSilence");
    }

    /** The fixed MIDI map: 60-63 trigger slots 1-4, 59/64 do nothing, four notes together
        sound all four slots. */
    void testMidiMap()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        // 59 and 64 do nothing.
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 59, 0.8f), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 64, 0.8f), 0);
        processOne (processor, midi);
        for (int slot = 0; slot < 4; ++slot)
            assert (! processor.slotIsActive (slot));

        // The four slot keys together sound all four slots.
        midi.clear();
        for (int key = 60; key <= 63; ++key)
            midi.addEvent (juce::MidiMessage::noteOn (1, key, 0.8f), 0);
        processOne (processor, midi);
        for (int slot = 0; slot < 4; ++slot)
            assert (processor.slotIsActive (slot));

        // One key: only its own slot.
        Processor other;
        other.prepareToPlay (sampleRate, blockSize);
        midi.clear();
        midi.addEvent (juce::MidiMessage::noteOn (1, 61, 0.8f), 0);
        processOne (other, midi);
        assert (other.slotIsActive (1));
        assert (! other.slotIsActive (0) && ! other.slotIsActive (2) && ! other.slotIsActive (3));
        pass ("testMidiMap");
    }

    /** Plan 17: a chopped slot's range must not shadow another slot's exact Note. The
        default layout (60/61/62/63 with slot 1 chopped 16) stays fully playable. */
    void testChopRangeDoesNotShadowExactNotes()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameter (processor.apvts, Processor::slotChopIds[0], 1.0f);
        setParameter (processor.apvts, Processor::slotSlicesIds[0], 16.0f);   // claims 60..75

        // The exact notes win: keys 61, 62, 63 play slots 2, 3, 4 even though slot 1's
        // chop range covers them.
        juce::MidiBuffer midi;
        for (int key = 61; key <= 63; ++key)
            midi.addEvent (juce::MidiMessage::noteOn (1, key, 0.8f), 0);
        processOne (processor, midi);
        assert (processor.slotIsActive (1) && processor.slotIsActive (2) && processor.slotIsActive (3));
        assert (! processor.slotIsActive (0));

        // A key with no exact match falls to the chop range: 65 = slot 1's slice 5.
        midi.clear();
        for (int slot = 0; slot < 4; ++slot)
            processor.reset();
        midi.addEvent (juce::MidiMessage::noteOn (1, 65, 0.8f), 0);
        processOne (processor, midi);
        assert (processor.slotIsActive (0));
        assert (! processor.slotIsActive (1) && ! processor.slotIsActive (2) && ! processor.slotIsActive (3));

        // The base key itself plays the chopped slot's slice 0 (exact match, slice 0).
        for (int slot = 0; slot < 4; ++slot)
            processor.reset();
        midi.clear();
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        processOne (processor, midi);
        assert (processor.slotIsActive (0));

        pass ("testChopRangeDoesNotShadowExactNotes");
    }

    /** Plan 17 Phase 3: a loop does not outlive its own settings. A toggle loop stops when
        the trigger mode changes or a preset loads, and a reversed loop is heard at once
        (the playing voice restarts on the transformed buffer). */
    void testModeAndPresetSilenceAndAudibleTransform()
    {
        // Toggle: the loop runs; changing the mode to Gate stops it.
        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            setParameter (processor.apvts, Processor::slotTriggerIds[0], 2.0f);

            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
            processOne (processor, midi);
            midi.clear();
            assert (processor.slotIsActive (0));

            setParameter (processor.apvts, Processor::slotTriggerIds[0], 1.0f);   // Gate
            processOne (processor, midi);
            for (int i = 0; i < 3; ++i)
                processOne (processor, midi);
            assert (! processor.slotIsActive (0));   // the mode change silenced the loop
        }

        // A preset load silences a running loop (Latch here).
        {
            const juce::File loopFile = writeRampLoopFloat ("preset_silence.wav");
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            processor.loadLoopFromFileAsync (0, loopFile, loopFile.getFullPathName());
            for (int i = 0; i < 40 && processor.hasPendingLoopLoad(); ++i)
                juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            assert (! processor.hasPendingLoopLoad());

            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
            processOne (processor, midi);
            midi.clear();
            assert (processor.slotIsActive (0));

            // INIT (an empty tree) is the preset path the bar and host program 0 use.
            processor.applyPresetState (juce::ValueTree ("PARAMETERS"));
            processOne (processor, midi);
            for (int i = 0; i < 3; ++i)
                processOne (processor, midi);
            assert (! processor.slotIsActive (0));   // the preset load silenced the loop
        }

        // REV while the slot loops is heard immediately: the playing voice restarts on
        // the transformed buffer, so the read jumps deep into the original ramp's tail.
        {
            const juce::File loopFile = writeRampLoopFloat ("rev_live.wav");
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            processor.loadLoopFromFileAsync (0, loopFile, loopFile.getFullPathName());
            for (int i = 0; i < 40 && processor.hasPendingLoopLoad(); ++i)
                juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            assert (! processor.hasPendingLoopLoad());

            setParameter (processor.apvts, Processor::slotCrossfadeIds[0], 0.0f);

            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
            processOne (processor, midi);
            midi.clear();

            const double before = (double) processor.loopForUi (0)->buffer.getSample (0, 0);

            processor.loopToolReverse (0);
            processOne (processor, midi);   // the retransform restarts the voice here

            // The buffer under the playing voice is the reversed one, and the voice is
            // still sounding (the restart kept it alive).
            const auto* loop = processor.loopForUi (0);
            assert (loop != nullptr);
            assert (std::abs (loop->buffer.getSample (0, 0) - before) > 1000.0f);   // reversed
            assert (processor.slotIsActive (0));
        }

        pass ("testModeAndPresetSilenceAndAudibleTransform");
    }

    /** Gate releases on note-off; Latch ignores it. */
    void testGateVsLatch()
    {
        // Gate: the voice stops after the note-off (5 ms fade, then inactive).
        Processor gate;
        gate.prepareToPlay (sampleRate, blockSize);
        setParameter (gate.apvts, Processor::slotTriggerIds[0], 1.0f);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        processOne (gate, midi);
        assert (gate.slotIsActive (0));

        midi.clear();
        midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
        for (int i = 0; i < 3; ++i)     // the 5 ms fade (240 samples) plus one block
            processOne (gate, midi);
        assert (! gate.slotIsActive (0));

        // Latch: the note-off is ignored.
        Processor latch;
        latch.prepareToPlay (sampleRate, blockSize);

        midi.clear();
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        processOne (latch, midi);
        midi.clear();
        midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
        for (int i = 0; i < 3; ++i)
            processOne (latch, midi);
        assert (latch.slotIsActive (0));
        pass ("testGateVsLatch");
    }

    /** Plan 14: Toggle mode. The first press starts the slot looping (scheduled on the
        quantise grid when Quantize is on); the second press stops it immediately, never
        delayed to the grid; a note-off does nothing. */
    void testToggleTrigger()
    {
        // Quantise off: the press starts immediately, the note-off is ignored, the second
        // press stops immediately.
        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            setParameter (processor.apvts, Processor::slotTriggerIds[0], 2.0f);

            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
            processOne (processor, midi);
            midi.clear();
            assert (processor.slotIsActive (0));

            // It keeps looping over many blocks (the 24000-frame embedded loop at speed 1
            // is half a second; 20 blocks pass its end, so this proves looping, not a
            // one-shot that ended).
            for (int i = 0; i < 20; ++i)
                processOne (processor, midi);
            assert (processor.slotIsActive (0));

            midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            processOne (processor, midi);
            midi.clear();
            assert (processor.slotIsActive (0));   // a note-off never stops a toggled slot

            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
            processOne (processor, midi);
            midi.clear();
            for (int i = 0; i < 3; ++i)     // the 5 ms stop fade
                processOne (processor, midi);
            assert (! processor.slotIsActive (0));
        }

        // Quantise 1/4: the start waits for the grid point, the second press cancels the
        // pending trigger before it fires, and a stop after the start is immediate.
        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            setParameter (processor.apvts, Processor::slotTriggerIds[0], 2.0f);
            setParameter (processor.apvts, Processor::slotQuantizeIds[0], 3.0f);   // 1/4 = one beat

            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 100);
            processOne (processor, midi);
            midi.clear();
            assert (! processor.slotIsActive (0));   // scheduled, not started

            // The second press before the grid point cancels the pending start.
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
            processOne (processor, midi);
            midi.clear();
            for (int i = 0; i < 12; ++i)     // well past one beat at 120 BPM (24000 samples)
                processOne (processor, midi);
            assert (! processor.slotIsActive (0));

            // A fresh press fires on the grid; the next press stops immediately.
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
            processOne (processor, midi);
            midi.clear();
            for (int i = 0; i < 50; ++i)     // past the grid point
                processOne (processor, midi);
            assert (processor.slotIsActive (0));

            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
            processOne (processor, midi);
            midi.clear();
            for (int i = 0; i < 3; ++i)
                processOne (processor, midi);
            assert (! processor.slotIsActive (0));
        }

        pass ("testToggleTrigger");
    }

    /** Plan 14: in Toggle mode a stopped transport stops the toggled slots and clears the
        toggle state, so the next press starts fresh. */
    void testToggleTransportStop()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameter (processor.apvts, Processor::slotTriggerIds[0], 2.0f);
        setParameter (processor.apvts, Processor::slotSyncIds[0], 1.0f);   // the playhead drives the clock

        TestPlayHead playHead;
        processor.setPlayHead (&playHead);
        playHead.info.setIsPlaying (true);
        playHead.info.setPpqPosition (0.0);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        processOne (processor, midi);
        midi.clear();
        assert (processor.slotIsActive (0));

        // The transport stops: the toggled slot stops with it.
        playHead.info.setIsPlaying (false);
        processOne (processor, midi);
        for (int i = 0; i < 3; ++i)
            processOne (processor, midi);
        assert (! processor.slotIsActive (0));

        // The next press starts fresh (the cleared toggle state does not swallow it).
        midi.clear();
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        processOne (processor, midi);
        assert (processor.slotIsActive (0));

        processor.setPlayHead (nullptr);
        pass ("testToggleTransportStop");
    }

    /** Plan 14: the per-slot trigger notes. Defaults are the old fixed map; a moved note
        claims only its own key. */
    void testSlotNoteRemap()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        // The defaults match today's fixed map.
        for (int slot = 0; slot < 4; ++slot)
            assert (processor.slotNote (slot) == 60 + slot);

        // Slot 0 moves to 72: 60 does nothing, 72 triggers slot 0, the other slots keep
        // their keys.
        setParameter (processor.apvts, Processor::slotNoteIds[0], 72.0f);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        processOne (processor, midi);
        for (int slot = 0; slot < 4; ++slot)
            assert (! processor.slotIsActive (slot));

        midi.clear();
        midi.addEvent (juce::MidiMessage::noteOn (1, 72, 0.8f), 0);
        processOne (processor, midi);
        assert (processor.slotIsActive (0));
        assert (! processor.slotIsActive (1) && ! processor.slotIsActive (2) && ! processor.slotIsActive (3));

        pass ("testSlotNoteRemap");
    }

    /** Plan 14: the Tempo parameter drives the clock while Host Sync is off; with sync on
        the host's position and tempo are authoritative. */
    void testTempoAndSync()
    {
        // Sync off, quantise one beat: at 240 BPM one beat is 12000 samples, so the
        // scheduled trigger fires within 24 blocks; at 60 BPM it needs 48.
        const auto firesAt = [] (float bpm)
        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            setParameter (processor.apvts, Processor::slotTempoIds[0], bpm);
            setParameter (processor.apvts, Processor::slotQuantizeIds[0], 3.0f);   // one beat

            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 100);
            processOne (processor, midi);
            midi.clear();

            int blocks = 0;
            while (! processor.slotIsActive (0) && blocks < 200)
            {
                processOne (processor, midi);
                ++blocks;
            }
            return blocks;
        };

        assert (firesAt (240.0f) <= 24);
        assert (firesAt (60.0f) > 24);

        // Sync on: the host's position drives the clock, and the host tempo is
        // authoritative even against a very different Tempo parameter.
        Processor synced;
        synced.prepareToPlay (sampleRate, blockSize);
        setParameter (synced.apvts, Processor::slotSyncIds[0], 1.0f);
        setParameter (synced.apvts, Processor::slotTempoIds[0], 240.0f);   // must be ignored
        setParameter (synced.apvts, Processor::slotQuantizeIds[0], 3.0f);

        TestPlayHead playHead;
        playHead.info.setIsPlaying (true);
        playHead.info.setBpm (60.0);           // one beat = 48000 samples
        playHead.info.setPpqPosition (0.0);
        synced.setPlayHead (&playHead);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 100);
        processOne (synced, midi);
        midi.clear();

        int blocks = 0;
        while (! synced.slotIsActive (0) && blocks < 200)
        {
            playHead.info.setPpqPosition ((double) (blocks + 1) * blockSize / 48000.0);
            processOne (synced, midi);
            ++blocks;
        }

        // At the host's 60 BPM the grid point is at 48000 samples (~94 blocks); at the
        // plugin's 240 it would have fired by block 24.
        assert (blocks > 40);

        synced.setPlayHead (nullptr);
        pass ("testTempoAndSync");
    }

    /** Plan 14: with Host Sync on the whole loop plays in exactly Beats beats: the rate
        comes from the loop's own duration, Speed is inactive, and Start/Length choose the
        span without changing the rate. */
    void testLoopSyncRate()
    {
        // A 24000-frame loop at the test rate: half a second at speed 1.
        const juce::File loopFile = writeTestLoop ("sync_rate.wav");
        const int loopFrames = 24000;

        const auto measuredRate = [&] (float beats, float speed, float start, float length) -> double
        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            processor.loadLoopFromFileAsync (0, loopFile, loopFile.getFullPathName());
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            for (int i = 0; i < 40 && processor.hasPendingLoopLoad(); ++i)
                juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            assert (! processor.hasPendingLoopLoad());

            setParameter (processor.apvts, Processor::slotSyncIds[0], 1.0f);
            setParameter (processor.apvts, Processor::slotBeatsIds[0], beats);
            setParameter (processor.apvts, Processor::slotSpeedIds[0], speed);
            setParameter (processor.apvts, Processor::slotStartIds[0], start);
            setParameter (processor.apvts, Processor::slotLengthIds[0], length);

            TestPlayHead playHead;
            playHead.info.setIsPlaying (true);
            playHead.info.setBpm (120.0);          // one beat = 24000 samples
            playHead.info.setPpqPosition (0.0);
            processor.setPlayHead (&playHead);

            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 100);
            processOne (processor, midi);
            midi.clear();
            assert (processor.slotIsActive (0));

            // The rate from the playhead's progress over 15 blocks, which cancels the
            // seam pre-roll's initial offset.
            for (int i = 0; i < 5; ++i)
                processOne (processor, midi);
            const double early = processor.slotPlayhead (0);

            for (int i = 0; i < 15; ++i)
                processOne (processor, midi);
            const double late = processor.slotPlayhead (0);

            processor.setPlayHead (nullptr);

            const auto span = CoreDSP::Loop::regionFrames (loopFrames, start, length);
            return (late - early) * (double) span[1] / (double) (15 * blockSize);
        };

        const double expected = loopFrames * 120.0 / (4.0 * 60.0 * (double) sampleRate);   // 0.25

        // The whole span: the loop takes four beats (two seconds), so the rate is 0.25
        // regardless of the Speed knob.
        assert (std::abs (measuredRate (4.0f, 1.0f, 0.0f, 100.0f) - expected) < 0.02);
        assert (std::abs (measuredRate (4.0f, 4.0f, 0.0f, 100.0f) - expected) < 0.02);   // Speed inactive

        // A trimmed span (Start 25 %, Length 50 %) keeps the same rate.
        assert (std::abs (measuredRate (4.0f, 1.0f, 25.0f, 50.0f) - expected) < 0.02);

        // One beat: the rate doubles the musical speed (0.5 s of loop in one beat).
        assert (std::abs (measuredRate (1.0f, 1.0f, 0.0f, 100.0f) - expected * 4.0) < 0.04);

        pass ("testLoopSyncRate");
    }

    /** Plan 14 (G-B): with Host Sync on the Speed knobs are dimmed (and Grain dims them
        as before); sync off keeps them live. */
    void testSpeedAffinityUnderSync()
    {
        Processor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
        auto* cyclicalEditor = dynamic_cast<CyclicalEditor*> (editor.get());
        assert (cyclicalEditor != nullptr);

        assert (cyclicalEditor->isSpeedEnabled (0));    // sync off by default

        setParameter (processor.apvts, Processor::slotSyncIds[0], 1.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
        assert (! cyclicalEditor->isSpeedEnabled (0));

        setParameter (processor.apvts, Processor::slotSyncIds[0], 0.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
        assert (cyclicalEditor->isSpeedEnabled (0));

        processor.editorBeingDeleted (editor.get());
        pass ("testSpeedAffinityUnderSync");
    }

    /** Plan 14 Phase 3: the chop boundary maths. b_0 = 0, b_S = 1, strictly increasing;
        v = 0 is the equal split; positive v lengthens the late slices, negative the early. */
    void testChopBoundaries()
    {
        // Equal split.
        for (int i = 0; i <= 8; ++i)
            assert (std::abs (Processor::chopBoundary (i, 8, 0.0f) - (double) i / 8.0) < 1.0e-12);

        // Strictly increasing over the range, for both skews.
        for (float v : { 60.0f, -60.0f })
        {
            double previous = -1.0;
            for (int i = 0; i <= 16; ++i)
            {
                const double b = Processor::chopBoundary (i, 16, v);
                assert (b > previous);
                previous = b;
            }
        }

        // Positive v: the late slices are longer (first slice shorter than equal, last
        // slice longer).
        assert (Processor::chopBoundary (1, 8, 60.0f) - Processor::chopBoundary (0, 8, 60.0f) < 1.0 / 8.0);
        assert (Processor::chopBoundary (8, 8, 60.0f) - Processor::chopBoundary (7, 8, 60.0f) > 1.0 / 8.0);

        // Negative v: the early slices are longer.
        assert (Processor::chopBoundary (1, 8, -60.0f) - Processor::chopBoundary (0, 8, -60.0f) > 1.0 / 8.0);
        assert (Processor::chopBoundary (8, 8, -60.0f) - Processor::chopBoundary (7, 8, -60.0f) < 1.0 / 8.0);

        pass ("testChopBoundaries");
    }

    /** Plan 14 Phase 3: a chopped slot's keys base..base+slices-1 trigger slice k of the
        region (the ramp loop's sample values name their frame, so the output reads the
        slice). Key 68 with 8 slices is unclaimed. */
    void testChopKeyMapping()
    {
        const juce::File loopFile = writeRampLoopFloat ("chop_map.wav");   // 24000 frames, value = frame
        const int loopFrames = 24000;

        const auto firstOutputValue = [&] (int key) -> double
        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            processor.loadLoopFromFileAsync (0, loopFile, loopFile.getFullPathName());
            for (int i = 0; i < 40 && processor.hasPendingLoopLoad(); ++i)
                juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            assert (! processor.hasPendingLoopLoad());

            setParameter (processor.apvts, Processor::slotChopIds[0], 1.0f);
            setParameter (processor.apvts, Processor::slotSlicesIds[0], 8.0f);
            setParameter (processor.apvts, Processor::slotCrossfadeIds[0], 0.0f);   // no seam blend

            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, key, 0.8f), 100);

            juce::AudioBuffer<float> first (2, blockSize);
            first.clear();
            processor.processBlock (first, midi);
            midi.clear();
            assert (processor.slotIsActive (0));

            juce::AudioBuffer<float> buffer (2, blockSize);
            buffer.clear();
            processor.processBlock (buffer, midi);

            // The frame the voice reads at block 2's last sample: the output carries the
            // gain law (velocity 0.8 x the constant-power pan 0.7071), so divide it out.
            // Press at sample 100: block 1 advanced 412 frames, block 2 renders 511 more.
            constexpr double gainLaw = 0.8 * 0.7071067811865476;
            return (double) buffer.getSample (0, buffer.getNumSamples() - 1) / gainLaw;
        };

        const auto sliceStart = [&] (int k)
        {
            return (int) std::lround (Processor::chopBoundary (k, 8, 0.0f) * (double) loopFrames);
        };

        // Slices 0, 5 and 7 read at their boundary frames, ~924 frames in (the two-block
        // measurement, plus the seam pre-roll slop). Keys 65 and 67 have no exact match in
        // the default notes (60-63), so the two-pass map (plan 17) falls to the chop range.
        assert (std::abs (firstOutputValue (60) - (double) (sliceStart (0) + 923)) < 120.0);
        assert (std::abs (firstOutputValue (65) - (double) (sliceStart (5) + 923)) < 120.0);
        assert (std::abs (firstOutputValue (67) - (double) (sliceStart (7) + 923)) < 120.0);

        // Key 68 (one above the slice range, no exact match either) does not trigger slot 0.
        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            setParameter (processor.apvts, Processor::slotChopIds[0], 1.0f);

            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 68, 0.8f), 0);
            processOne (processor, midi);
            assert (! processor.slotIsActive (0));
        }

        pass ("testChopKeyMapping");
    }

    /** Plan 14 Phase 3: a chopped slot ignores Host Sync entirely (its Speed knob stays
        live and its Beats value is not used. */
    void testChopIgnoresSync()
    {
        const juce::File loopFile = writeRampLoopFloat ("chop_sync.wav");
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        processor.loadLoopFromFileAsync (0, loopFile, loopFile.getFullPathName());
        for (int i = 0; i < 40 && processor.hasPendingLoopLoad(); ++i)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        assert (! processor.hasPendingLoopLoad());

        setParameter (processor.apvts, Processor::slotSyncIds[0], 1.0f);
        setParameter (processor.apvts, Processor::slotBeatsIds[0], 4.0f);
        setParameter (processor.apvts, Processor::slotChopIds[0], 1.0f);
        setParameter (processor.apvts, Processor::slotSpeedIds[0], 1.0f);   // must win over sync
        setParameter (processor.apvts, Processor::slotCrossfadeIds[0], 0.0f);   // the fresh span is the whole slice

        TestPlayHead playHead;
        playHead.info.setIsPlaying (true);
        playHead.info.setBpm (120.0);
        playHead.info.setPpqPosition (0.0);
        processor.setPlayHead (&playHead);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 100);
        processOne (processor, midi);
        midi.clear();
        assert (processor.slotIsActive (0));

        // The playhead normalises over the fresh span, which for slice 0 (8 equal slices)
        // is 3000 frames. Two one-block gaps: the traversal stays inside the slice.
        processOne (processor, midi);
        const double early = processor.slotPlayhead (0);
        processOne (processor, midi);
        processOne (processor, midi);
        const double late = processor.slotPlayhead (0);

        processor.setPlayHead (nullptr);

        // Speed 1: one frame per output sample, not the synced 0.25.
        const double rate = (late - early) * 3000.0 / (double) (2 * blockSize);
        assert (std::abs (rate - 1.0) < 0.06);

        pass ("testChopIgnoresSync");
    }

    /** Plan 14 Phase 3 (G-D): granular inside a slice stays inside the slice. */
    void testChopGranularStaysInsideSlice()
    {
        const juce::File loopFile = writeRampLoopFloat ("chop_grain.wav");
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        processor.loadLoopFromFileAsync (0, loopFile, loopFile.getFullPathName());
        for (int i = 0; i < 40 && processor.hasPendingLoopLoad(); ++i)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        assert (! processor.hasPendingLoopLoad());

        setParameter (processor.apvts, Processor::slotChopIds[0], 1.0f);
        setParameter (processor.apvts, Processor::slotSlicesIds[0], 4.0f);
        setParameter (processor.apvts, Processor::slotGranularIds[0], 1.0f);
        setParameter (processor.apvts, Processor::slotCrossfadeIds[0], 0.0f);
        setParameter (processor.apvts, Processor::slotNoteIds[0], 72.0f);   // 72..75: no exact matches

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 73, 0.8f), 100);   // slice 1
        processOne (processor, midi);
        midi.clear();
        assert (processor.slotIsActive (0));

        // Many blocks of grains: every output value names a frame inside slice 1.
        const double start = Processor::chopBoundary (1, 4, 0.0f) * 24000.0;
        const double end = Processor::chopBoundary (2, 4, 0.0f) * 24000.0;

        bool heard = false;
        constexpr double gainLaw = 0.8 * 0.7071067811865476;

        for (int b = 0; b < 20; ++b)
        {
            juce::AudioBuffer<float> buffer (2, blockSize);
            buffer.clear();
            processor.processBlock (buffer, midi);

            for (int n = 0; n < buffer.getNumSamples(); ++n)
            {
                const float v = buffer.getSample (0, n);
                if (std::abs (v) > 1.0e-3f)
                {
                    heard = true;
                    // The upper bound is the behavioural check: no grain reads past the
                    // slice end. (The lower bound is unmeasurable during the attack ramp;
                    // readRegion clamps it by construction.)
                    const double frame = (double) v / gainLaw;
                    assert (frame <= end + 2.0);
                }
            }
        }

        assert (heard);
        pass ("testChopGranularStaysInsideSlice");
    }

    /** A retrigger starts a fresh voice: the playhead returns to the top of the span. */
    void testRetriggerRestarts()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        processOne (processor, midi);
        midi.clear();

        for (int i = 0; i < 19; ++i)
            processOne (processor, midi);
        assert (processor.slotPlayhead (0) > 0.05f);   // well into the loop

        midi.clear();
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        processOne (processor, midi);
        assert (processor.slotPlayhead (0) < 0.05f);   // fresh start
        pass ("testRetriggerRestarts");
    }

    /** Plan 18 §1.4 (owner answer): the region knobs apply live. A knob release that
        moved the value retriggers the playing voice, which restarts inside the new
        region (the plan-16 retransform machinery, driven from the editor's drag end). */
    void testRegionKnobsApplyLive()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        processOne (processor, midi);
        midi.clear();

        for (int i = 0; i < 10; ++i)
            processOne (processor, midi);
        assert (processor.slotIsActive (0));
        const float playheadBefore = processor.slotPlayhead (0);
        assert (playheadBefore > 0.02f);   // well into the loop

        // The knob moved: Start 0 -> 50 %. The drag-end retrigger restarts the voice,
        // so the playhead goes back to the (new) region's beginning.
        setParameter (processor.apvts, Processor::slotStartIds[0], 50.0f);
        processor.requestRegionRetrigger (0);
        processOne (processor, midi);

        assert (processor.slotIsActive (0));   // the restart kept the loop sounding
        assert (processor.slotPlayhead (0) < playheadBefore);

        // A no-op release (no value change) is a no-op: the voice keeps playing through.
        const float playheadAtNoop = processor.slotPlayhead (0);
        processor.requestRegionRetrigger (0);   // without a value change the editor never calls it; direct call is the guard
        processOne (processor, midi);
        assert (processor.slotIsActive (0));
        juce::ignoreUnused (playheadAtNoop);
        pass ("testRegionKnobsApplyLive");
    }

    /** The host-load channel case: a one-channel buffer produces finite output. */
    void testMonoBuffer()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        juce::AudioBuffer<float> buffer (1, blockSize);

        for (int i = 0; i < 4; ++i)
        {
            buffer.clear();
            processor.processBlock (buffer, midi);
            assert (allFinite (buffer));
        }
        pass ("testMonoBuffer");
    }

    void testLatencyAndTail()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        assert (processor.getLatencySamples() == 0);
        assert (processor.getTailLengthSeconds() == 2.0);
        pass ("testLatencyAndTail");
    }

    /** Quantised starts land on the grid; Off starts immediately. */
    void testQuantisedStarts()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameter (processor.apvts, Processor::slotQuantizeIds[0], 1.0f);     // 1/16

        // At 120 BPM a 1/16 step is 6000 samples. A trigger inside a step starts on the
        // next grid sample, so the slot stays silent until the block that contains it.
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 100);
        processOne (processor, midi);       // block 0: the grid point is in the future
        assert (! processor.slotIsActive (0));

        for (int b = 1; b <= 10; ++b)
        {
            processOne (processor, midi);
            if (processor.slotIsActive (0))
            {
                std::printf ("PROBE quantise active early at block %d\n", b);
                std::fflush (stdout);
            }
        }
        assert (! processor.slotIsActive (0));      // still before the grid point

        processOne (processor, midi);       // block 11 covers sample 6000
        assert (processor.slotIsActive (0));

        // Quantize Off starts immediately.
        setParameter (processor.apvts, Processor::slotQuantizeIds[0], 0.0f);
        Processor immediate;
        immediate.prepareToPlay (sampleRate, blockSize);
        midi.clear();
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 100);
        processOne (immediate, midi);
        assert (immediate.slotIsActive (0));
        pass ("testQuantisedStarts");
    }

    /** Swing moves odd grid points by the suite's swingBeats convention. */
    void testSwingShiftsOddGridPoints()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameter (processor.apvts, Processor::slotQuantizeIds[0], 1.0f);     // 1/16
        setParameter (processor.apvts, Processor::slotSwingIds[0], 20.0f);       // +20 %

        // A trigger at beat 0.25 (sample 6000) schedules grid index 1, which swings
        // +0.2 x 0.25 beats = 0.05 beats = 1200 samples later: 7200 instead of 6000.
        // A trigger exactly on a grid point fires there (grid index 0 would not swing).
        juce::MidiBuffer midi;
        for (int b = 0; b < 11; ++b)
            processOne (processor, midi);

        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 368);    // absolute sample 6000
        processOne (processor, midi);       // block 11: the trigger, pending at 7200
        midi.clear();

        for (int b = 12; b < 14; ++b)       // through sample 7167 (blockEndBeat 0.2987)
            processOne (processor, midi);
        assert (! processor.slotIsActive (0));

        processOne (processor, midi);       // block 14 covers sample 7200
        assert (processor.slotIsActive (0));
        pass ("testSwingShiftsOddGridPoints");
    }

    /** Pending rules: a Gate tap shorter than the grid step sounds nothing; a second
        trigger replaces the pending one. */
    void testPendingRules()
    {
        // Cancel: a note-off before the scheduled start cancels the pending trigger.
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameter (processor.apvts, Processor::slotQuantizeIds[0], 1.0f);
        setParameter (processor.apvts, Processor::slotTriggerIds[0], 1.0f);   // Gate

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 100);
        midi.addEvent (juce::MidiMessage::noteOff (1, 60), 200);

        for (int b = 0; b < 14; ++b)
            processOne (processor, midi);
        assert (! processor.slotIsActive (0));      // the tap produced nothing

        // Replace: two triggers in a row produce a single grid start, not two.
        Processor replaced;
        replaced.prepareToPlay (sampleRate, blockSize);
        setParameter (replaced.apvts, Processor::slotQuantizeIds[0], 1.0f);

        midi.clear();
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 100);
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.9f), 200);

        for (int b = 0; b < 15; ++b)
            processOne (replaced, midi);
        assert (replaced.slotIsActive (0));         // one start on the grid
        pass ("testPendingRules");
    }

    /** All four embedded defaults resolve at construction and their paths ride as
        properties. */
    void testEmbeddedDefaults()
    {
        Processor processor;

        for (int slot = 0; slot < 4; ++slot)
        {
            // The saved path is the embedded URI.
            assert (processor.loopPath (slot)
                        == juce::String (Processor::embeddedUriPrefix)
                               + Processor::defaultLoopNames[(size_t) slot] + "_4k.wav");
        }

        // And the four keys make sound: a full block after the trigger has energy.
        Processor sounding;
        sounding.prepareToPlay (sampleRate, blockSize);
        juce::MidiBuffer midi;
        for (int key = 60; key <= 63; ++key)
            midi.addEvent (juce::MidiMessage::noteOn (1, key, 0.8f), 0);

        float best = 0.0f;
        for (int b = 0; b < 10; ++b)
        {
            juce::AudioBuffer<float> buffer (2, blockSize);
            buffer.clear();
            sounding.processBlock (buffer, midi);
            midi.clear();
            best = juce::jmax (best, peak (buffer));
        }
        assert (best > 0.01f);
        pass ("testEmbeddedDefaults");
    }

    /** An embedded: URI in the state loads that library loop into the slot. */
    void testEmbeddedUriFromState()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::ValueTree tree ("PARAMETERS");
        tree.setProperty ("loop3Path", "embedded:kick_4k.wav", nullptr);
        processor.applyLoopsFromState (tree);

        assert (processor.loopPath (2) == "embedded:kick_4k.wav");
        assert (! processor.isLoopMissing (2));

        // The slot makes sound.
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 62, 0.8f), 0);
        float best = 0.0f;
        for (int b = 0; b < 40; ++b)
        {
            juce::AudioBuffer<float> buffer (2, blockSize);
            buffer.clear();
            processor.processBlock (buffer, midi);
            midi.clear();
            best = juce::jmax (best, peak (buffer));
        }
        assert (best > 0.01f);
        pass ("testEmbeddedUriFromState");
    }

    /** A missing loop keeps the current loop playing and raises the slot's missing flag. */
    void testMissingLoopKeepsPlaying()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::ValueTree tree ("PARAMETERS");
        tree.setProperty ("loop2Path", "Z:/does/not/exist.wav", nullptr);
        processor.applyLoopsFromState (tree);

        assert (processor.isLoopMissing (1));
        assert (processor.missingLoopPath (1) == "Z:/does/not/exist.wav");

        // Slot 2 keeps its previous loop and still sounds on key 62.
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 62, 0.8f), 0);
        float best = 0.0f;
        for (int b = 0; b < 10; ++b)
        {
            juce::AudioBuffer<float> buffer (2, blockSize);
            buffer.clear();
            processor.processBlock (buffer, midi);
            midi.clear();
            best = juce::jmax (best, peak (buffer));
        }
        assert (best > 0.01f);
        pass ("testMissingLoopKeepsPlaying");
    }

    /** Asynchronous disk loads: the latest request wins, and a load during a sounding
        voice does not disturb the running loop (the retirement queue). */
    void testAsyncDiskLoad()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        const juce::File loopA = writeTestLoop ("loop-a.wav");
        const juce::File loopB = writeTestLoop ("loop-b.wav");

        // A sounding voice, then a load: the voice keeps playing through the install.
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        processOne (processor, midi);
        assert (processor.slotIsActive (0));

        processor.loadLoopFromFileAsync (0, loopA, loopA.getFullPathName());
        processor.loadLoopFromFileAsync (0, loopB, loopB.getFullPathName());   // latest wins

        // Pump the message loop until the handoffs have landed.
        for (int i = 0; i < 200 && processor.hasPendingLoopLoad(); ++i)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
        assert (! processor.hasPendingLoopLoad());

        assert (processor.loopPath (0) == loopB.getFullPathName());
        assert (! processor.isLoopMissing (0));

        // The slot still sounds after the swap.
        float best = 0.0f;
        for (int b = 0; b < 20; ++b)
        {
            juce::AudioBuffer<float> buffer (2, blockSize);
            buffer.clear();
            processor.processBlock (buffer, midi);
            midi.clear();
            best = juce::jmax (best, peak (buffer));
        }
        assert (best > 0.01f);
        pass ("testAsyncDiskLoad");
    }

    /** A mono source feeds both output channels identically at pan 0, and the pan is
        constant-power (the peak-power sum stays put). */
    void testStereoAndPan()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);

        float peakL = 0.0f, peakR = 0.0f;
        for (int b = 0; b < 20; ++b)
        {
            juce::AudioBuffer<float> buffer (2, blockSize);
            buffer.clear();
            processor.processBlock (buffer, midi);
            midi.clear();
            peakL = juce::jmax (peakL, buffer.getMagnitude (0, 0, blockSize));
            peakR = juce::jmax (peakR, buffer.getMagnitude (1, 0, blockSize));
        }

        // Pan 0: the mono kick feeds both channels identically (gains cos = sin).
        assert (peakL > 0.01f);
        assert (std::abs (peakL - peakR) < 0.001f);
        const float power0 = peakL * peakL + peakR * peakR;

        // Pan +100: everything lands in the right channel; the power sum stays put.
        // (Constant-power law: cos/sin, so +100 % pans right.)
        setParameter (processor.apvts, Processor::slotPanIds[0], 100.0f);
        processor.prepareToPlay (sampleRate, blockSize);

        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        float peakL2 = 0.0f, peakR2 = 0.0f;
        for (int b = 0; b < 20; ++b)
        {
            juce::AudioBuffer<float> buffer (2, blockSize);
            buffer.clear();
            processor.processBlock (buffer, midi);
            midi.clear();
            peakL2 = juce::jmax (peakL2, buffer.getMagnitude (0, 0, blockSize));
            peakR2 = juce::jmax (peakR2, buffer.getMagnitude (1, 0, blockSize));
        }

        assert (peakL2 < 0.001f);
        const float power100 = peakL2 * peakL2 + peakR2 * peakR2;
        assert (std::abs (power0 - power100) < 0.05f * power0);
        pass ("testStereoAndPan");
    }

    // R12: the preset seam (plan 09 1.4 selection round trip).
    void testPresetSeam()
    {
        Processor processor;

        assert (processor.getPresetPluginId() == "Cyclical");
        assert (processor.getPresetStateVersion() == processor.stateVersion);
        assert (&processor.getPresetStateTree() == &processor.apvts);

        SuitePresets::PresetSelection selection;
        selection.kind = SuitePresets::PresetSelection::Kind::factory;
        selection.factoryIndex = 3;
        processor.setPresetSelection (selection);

        assert (processor.apvts.state.getProperty (Processor::presetSelectionProperty).toString()
                == "factory:3");

        juce::MemoryBlock state;
        processor.getStateInformation (state);

        processor.setPresetSelection ({});
        assert (processor.getPresetSelection().kind == SuitePresets::PresetSelection::Kind::none);

        processor.setStateInformation (state.getData(), (int) state.getSize());
        assert (processor.getPresetSelection() == selection);

        // A preset tree is a delta: applied with resetMissing, absent IDs go back to default.
        auto tree = processor.apvts.copyState();
        processor.applyPresetState (tree);
        assert (processor.getPresetSelection().kind == SuitePresets::PresetSelection::Kind::none);

        pass ("testPresetSeam");
    }

    // R12/R13: the preset bar and the menu menu are in the editor header, and the menu's
    // two dialogs build and paint.
    void testHeaderContract()
    {
        Processor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        assert (editor != nullptr);

        SuitePresets::PresetBar* bar = nullptr;
        SuiteUI::HeaderMenuButton* menu = nullptr;

        for (int i = 0; i < editor->getNumChildComponents(); ++i)
        {
            auto* child = editor->getChildComponent (i);

            if (auto* found = dynamic_cast<SuitePresets::PresetBar*> (child))
                bar = found;

            if (auto* found = dynamic_cast<SuiteUI::HeaderMenuButton*> (child))
                menu = found;
        }

        assert (bar != nullptr);
        assert (menu != nullptr);
        assert (menu->onAbout != nullptr);
        assert (menu->onSettings != nullptr);

        assert (menu->getRight() > editor->getWidth() - 40);
        assert (bar->getRight() <= menu->getX());

        {
            SuiteUI::AboutPanel about ({}, "Cyclical", "1.0.0", "Cycl", "Cyclical");
            const auto image = paintComponent (about);
            assert (image.getPixelAt (8, 8) == SuiteUI::Colours::panel);
        }

        {
            const auto file = juce::File::getCurrentWorkingDirectory()
                                  .getChildFile ("CyclicalHeaderContractSettings.xml");
            file.deleteFile();

            SuiteSettings::Store store (file);
            SuiteSettings::SettingsDialog dialog (store, false);
            const auto image = paintComponent (dialog);
            assert (image.getPixelAt (8, 8) == SuiteUI::Colours::panel);

            file.deleteFile();
        }

        processor.editorBeingDeleted (editor.get());
        editor.reset();

        pass ("testHeaderContract");
    }

    void testEditor()
    {
        Processor processor;
        checkEditor (processor);
        pass ("testEditor");
    }

    /** Plan 12: the shared help line, with the stale-hover regression. */
    void testHelpLine()
    {
        Processor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        assert (editor != nullptr);

        std::function<juce::Component* (juce::Component&)> findSlider = [&] (juce::Component& root)
            -> juce::Component*
        {
            if (dynamic_cast<SuiteUI::LabelledSlider*> (&root) != nullptr)
                return &root;

            for (auto* child : root.getChildren())
                if (auto* found = findSlider (*child))
                    return found;

            return nullptr;
        };

        auto* slider = findSlider (*editor);
        assert (slider != nullptr && slider->getHelpText().isNotEmpty());

        PluginTest::checkHelpLine (*editor, "cyclical-help", *slider);

        processor.editorBeingDeleted (editor.get());
        pass ("testHelpLine");
    }

    /** Plan 10 G1: the mode-dependent controls are dimmed, never hidden. Plan 14 adds the
        chop/sync rules, tested in every combination of Chop and Host Sync. */
    void testAffinity()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        auto* cyclicalEditor = dynamic_cast<CyclicalEditor*> (editor.get());
        assert (cyclicalEditor != nullptr);

        // Default: forward, no grain, no chop, sync off. Speed is live; Ping Rand is
        // dimmed outside Ping-Pong; the chop cells are dimmed; Beats is live.
        assert (cyclicalEditor->isSpeedEnabled (0));
        assert (! cyclicalEditor->isPingRandomEnabled (0));
        assert (cyclicalEditor->isSpeedVisible (0));
        assert (cyclicalEditor->isBeatsEnabled (0));
        assert (! cyclicalEditor->isSlicesEnabled (0));
        assert (! cyclicalEditor->isSliceLengthEnabled (0));

        // Grain on: Speed dims but stays visible.
        setParameter (processor.apvts, Processor::slotGranularIds[0], 1.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);   // a timer tick
        assert (! cyclicalEditor->isSpeedEnabled (0));
        assert (cyclicalEditor->isSpeedVisible (0));

        // Ping-Pong direction: Ping Rand enables.
        setParameter (processor.apvts, Processor::slotDirectionIds[0], 2.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        assert (cyclicalEditor->isPingRandomEnabled (0));
        assert (! cyclicalEditor->isSpeedEnabled (0));   // grain is still on

        // Every combination of Chop and Host Sync (plan 14: the three rules interact).
        setParameter (processor.apvts, Processor::slotGranularIds[0], 0.0f);

        setParameter (processor.apvts, Processor::slotSyncIds[0], 0.0f);
        setParameter (processor.apvts, Processor::slotChopIds[0], 0.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        assert (cyclicalEditor->isSpeedEnabled (0));
        assert (cyclicalEditor->isBeatsEnabled (0));
        assert (! cyclicalEditor->isSlicesEnabled (0));
        assert (cyclicalEditor->isTempoEnabled (0));     // sync off: the slot's Tempo is live

        setParameter (processor.apvts, Processor::slotChopIds[0], 1.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        assert (cyclicalEditor->isSpeedEnabled (0));     // sync off: Speed stays live
        assert (! cyclicalEditor->isBeatsEnabled (0));   // chopped: Beats dims
        assert (cyclicalEditor->isSlicesEnabled (0));
        assert (cyclicalEditor->isSliceLengthEnabled (0));

        setParameter (processor.apvts, Processor::slotSyncIds[0], 1.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        assert (cyclicalEditor->isSpeedEnabled (0));     // a chopped slot ignores sync
        assert (! cyclicalEditor->isBeatsEnabled (0));
        assert (! cyclicalEditor->isTempoEnabled (0));   // the beat clock still follows the host

        setParameter (processor.apvts, Processor::slotChopIds[0], 0.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        assert (! cyclicalEditor->isSpeedEnabled (0));   // unchopped + synced: Speed dims
        assert (cyclicalEditor->isBeatsEnabled (0));
        assert (! cyclicalEditor->isSlicesEnabled (0));
        assert (! cyclicalEditor->isTempoEnabled (0));   // plan 17 Phase 6: the host's tempo is authoritative

        setParameter (processor.apvts, Processor::slotSyncIds[0], 0.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        assert (cyclicalEditor->isSpeedEnabled (0));
        assert (cyclicalEditor->isTempoEnabled (0));

        // The page switch: both pages show their own controls only.
        assert (cyclicalEditor->getPage() == 0);
        cyclicalEditor->setPage (1);
        assert (cyclicalEditor->getPage() == 1);
        assert (! cyclicalEditor->isSpeedVisible (0));   // the cards hide on GLOBAL
        cyclicalEditor->setPage (0);
        assert (cyclicalEditor->isSpeedVisible (0));

        processor.editorBeingDeleted (editor.get());
        editor.reset();
        pass ("testAffinity");
    }

    /** Plan 14 Phase 5: the waveform draws the slice boundaries when the slot is chopped,
        none when it is not, and the drawn lines sit at the shared chopBoundary shares. */
    void testWaveformChopLines()
    {
        juce::AudioBuffer<float> buffer (1, 24000);
        for (int i = 0; i < 24000; ++i)
            buffer.setSample (0, i, (float) i / 24000.0f);
        auto loop = makeLoadedLoop (juce::AudioBuffer<float> (buffer), (double) sampleRate, "embedded:test.wav");

        LoopWaveformView view { juce::Colour (0xffFF7A45) };
        view.setBounds (0, 0, 400, 110);
        view.setLoop (loop.get());
        view.setRegion (0.0f, 100.0f);
        view.setMissing (false);

        juce::Image baseline = PluginTest::paintComponent (view);

        // Chop on: the boundaries at the equal-split shares.
        view.setChopLines ({ 0.25f, 0.5f, 0.75f });
        juce::Image chopped = PluginTest::paintComponent (view);
        assert (PluginTest::countChangedPixels (baseline, chopped) > 0);

        // The lines sit at the boundary shares: the plot is the view reduced by 4 px.
        const int plotLeft = 4;
        const int plotWidth = 400 - 8;
        const auto changedAt = [&] (int x)
        {
            return baseline.getPixelAt ((float) x, 55.0f) != chopped.getPixelAt ((float) x, 55.0f);
        };
        assert (changedAt (plotLeft + (int) (0.25f * (float) plotWidth)));
        assert (changedAt (plotLeft + (int) (0.5f * (float) plotWidth)));
        assert (changedAt (plotLeft + (int) (0.75f * (float) plotWidth)));

        // A share with no line: the equal-split 0.125 position stays as the baseline.
        assert (! changedAt (plotLeft + (int) (0.125f * (float) plotWidth)));

        // Chop off: back to the baseline.
        view.setChopLines ({});
        juce::Image cleared = PluginTest::paintComponent (view);
        assert (PluginTest::countChangedPixels (baseline, cleared) == 0);

        pass ("testWaveformChopLines");
    }

    /** Plan 16 Phase 1: the AUTO analyser. A synthetic loop with two clear hits at 1/3 and
        2/3 of the region produces shares at those positions and sets Slices to 3. */
    void testAutoChop()
    {
        const juce::File loopFile = scratchFolder().getChildFile ("auto_chop.wav");
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream (loopFile.createOutputStream());

        juce::AudioFormatWriterOptions options;
        options = options.withSampleRate (sampleRate).withNumChannels (1).withBitsPerSample (32);
        std::unique_ptr<juce::AudioFormatWriter> writer = wav.createWriterFor (stream, options);
        assert (writer != nullptr);

        const int frames = 24000;
        juce::AudioBuffer<float> loop (1, frames);
        loop.clear();
        for (int i = 0; i < 3000; ++i)   // a hit at frame 8000 (1/3)
            loop.setSample (0, 8000 + i, 0.9f);
        for (int i = 0; i < 3000; ++i)   // a hit at frame 16000 (2/3)
            loop.setSample (0, 16000 + i, 0.9f);
        writer->writeFromAudioSampleBuffer (loop, 0, frames);
        writer.reset();

        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        processor.loadLoopFromFileAsync (0, loopFile, loopFile.getFullPathName());
        for (int i = 0; i < 40 && processor.hasPendingLoopLoad(); ++i)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        assert (! processor.hasPendingLoopLoad());

        setParameter (processor.apvts, Processor::slotChopIds[0], 1.0f);
        processor.autoChopSlot (0);

        // Two onsets -> three slices, and the boundaries sit at the hits (within a window).
        assert (processor.slotSlices (0) == 3);
        const auto bounds = processor.chopBounds (0);
        assert ((int) bounds.size() == 2);
        assert (std::abs (bounds[0] - 8000.0f / 24000.0f) < 0.03f);
        assert (std::abs (bounds[1] - 16000.0f / 24000.0f) < 0.03f);

        // The property rides in the state (the session/preset route).
        assert (! processor.apvts.state.getProperty (Processor::chop1BoundsProperty).toString().isEmpty());

        // A manual Slices edit drops the analyser's shares via the count mismatch (the
        // activeChopBounds fallback); the stored property survives but stops being used.
        setParameter (processor.apvts, Processor::slotSlicesIds[0], 6.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
        assert ((int) processor.activeChopBounds (0).size() == 5);   // the 6-slice power curve
        assert (processor.chopBounds (0).empty() == false);

        pass ("testAutoChop");
    }

    /** Plan 16 Phase 1: a chopped slot reads slice k at the auto-chop boundary — the ramp
        loop's values name their frame, so the audio shows it. */
    void testAutoChopRegion()
    {
        // A 24000-frame float ramp with a clear hit at 1/3, so the analyser slices there.
        const juce::File loopFile = scratchFolder().getChildFile ("auto_region.wav");
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream (loopFile.createOutputStream());

        juce::AudioFormatWriterOptions options;
        options = options.withSampleRate (sampleRate).withNumChannels (1).withBitsPerSample (32);
        std::unique_ptr<juce::AudioFormatWriter> writer = wav.createWriterFor (stream, options);
        assert (writer != nullptr);

        const int frames = 24000;
        juce::AudioBuffer<float> loop (1, frames);
        for (int i = 0; i < frames; ++i)
            loop.setSample (0, i, (float) i);
        for (int i = 0; i < 300; ++i)      // a short click at 8000: the onset stand-out
            loop.setSample (0, 8000 + i, 24000.0f);
        writer->writeFromAudioSampleBuffer (loop, 0, frames);
        writer.reset();

        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        processor.loadLoopFromFileAsync (0, loopFile, loopFile.getFullPathName());
        for (int i = 0; i < 40 && processor.hasPendingLoopLoad(); ++i)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        assert (! processor.hasPendingLoopLoad());

        setParameter (processor.apvts, Processor::slotChopIds[0], 1.0f);
        setParameter (processor.apvts, Processor::slotCrossfadeIds[0], 0.0f);
        setParameter (processor.apvts, Processor::slotNoteIds[0], 72.0f);   // 72/73: no exact matches
        processor.autoChopSlot (0);
        assert (processor.slotSlices (0) == 2);   // one detected onset -> two slices

        // Slice 1 starts at the detected boundary (the onset's rise at ~8000), not at the
        // equal split (12000): the ramp values name their frame. The click is 300 frames,
        // so the read at ~923 frames in lands back on the ramp.
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 73, 0.8f), 100);   // slice 1
        processOne (processor, midi);
        midi.clear();
        assert (processor.slotIsActive (0));

        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();
        processor.processBlock (buffer, midi);

        // Block 1 advanced 412 frames, block 2 renders 511 more: ~923 frames into the
        // slice, within the interpolation slop. The click may still be sounding (the read
        // can sit on its 24000 plateau), so accept either the plateau or the ramp frame.
        const double frame = (double) buffer.getSample (0, blockSize - 1) / (0.8 * 0.7071067811865476);
        const double boundary = processor.chopBounds (0)[0] * 24000.0;
        const bool onPlateau = std::abs (frame - 24000.0) < 2.0;
        const bool onRamp = std::abs (frame - (boundary + 923.0)) < 60.0;
        assert (onPlateau || onRamp);

        pass ("testAutoChopRegion");
    }

    /** Plan 16 Phase 2: the arrangement. Key k plays the slice the order names; the ramp
        loop's values name their frame, so the audio shows it. */
    void testSliceOrder()
    {
        const juce::File loopFile = writeRampLoopFloat ("slice_order.wav");
        const int loopFrames = 24000;
        const auto firstOutputValue = [&] (int key) -> double
        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            processor.loadLoopFromFileAsync (0, loopFile, loopFile.getFullPathName());
            for (int i = 0; i < 40 && processor.hasPendingLoopLoad(); ++i)
                juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            assert (! processor.hasPendingLoopLoad());

            setParameter (processor.apvts, Processor::slotChopIds[0], 1.0f);
            setParameter (processor.apvts, Processor::slotSlicesIds[0], 3.0f);
            setParameter (processor.apvts, Processor::slotCrossfadeIds[0], 0.0f);

            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, key, 0.8f), 100);
            processOne (processor, midi);
            midi.clear();
            assert (processor.slotIsActive (0));

            juce::AudioBuffer<float> buffer (2, blockSize);
            buffer.clear();
            processor.processBlock (buffer, midi);

            constexpr double gainLaw = 0.8 * 0.7071067811865476;
            return (double) buffer.getSample (0, buffer.getNumSamples() - 1) / gainLaw;
        };

        // Identity: key 60 plays slice 0, 923 frames in.
        assert (std::abs (firstOutputValue (60) - 923.0) < 60.0);

        // The arrangement [2, 0, 1]: key 60 now plays slice 2 (the third third), 923
        // frames into it.
        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            processor.loadLoopFromFileAsync (0, loopFile, loopFile.getFullPathName());
            for (int i = 0; i < 40 && processor.hasPendingLoopLoad(); ++i)
                juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            assert (! processor.hasPendingLoopLoad());

            setParameter (processor.apvts, Processor::slotChopIds[0], 1.0f);
            setParameter (processor.apvts, Processor::slotSlicesIds[0], 3.0f);

            const std::vector<int> written { 2, 0, 1 };
            processor.setChopOrderProperty (0, written);
            assert (processor.chopOrder (0) == written);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 100);
            processOne (processor, midi);
            midi.clear();
            assert (processor.slotIsActive (0));

            juce::AudioBuffer<float> buffer (2, blockSize);
            buffer.clear();
            processor.processBlock (buffer, midi);

            constexpr double gainLaw = 0.8 * 0.7071067811865476;
            const double frame = (double) buffer.getSample (0, blockSize - 1) / gainLaw;

            // Slice 2 starts at 2/3 of the loop. The seam pre-roll (crossfade = 240
            // frames) starts the fresh span X frames in, plus the 936 rendered samples.
            assert (frame > 16000.0 + 240.0 && frame < 16000.0 + 240.0 + 936.0 + 80.0);

            // The property rides in the state (the session/preset route).
            assert (! processor.apvts.state.getProperty (Processor::chop1OrderProperty).toString().isEmpty());
        }

        // A count mismatch falls back to the identity arrangement.
        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            setParameter (processor.apvts, Processor::slotChopIds[0], 1.0f);
            processor.setChopOrderProperty (0, { 2, 0 });     // 2 entries against 3 slices
            const std::vector<int> stored { 2, 0 };
            assert (processor.chopOrder (0) == stored);
            assert (processor.apvts.state.getProperty (Processor::chop1OrderProperty).toString().isNotEmpty());

            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
            processOne (processor, midi);
            assert (processor.slotIsActive (0));              // it still plays slice 0
        }

        // Cycling: one click moves key 0 to slice 1; three identity cycles clear the
        // property again.
        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            setParameter (processor.apvts, Processor::slotChopIds[0], 1.0f);
            setParameter (processor.apvts, Processor::slotSlicesIds[0], 3.0f);

            processor.cycleSliceOrder (0, 0);
            const std::vector<int> afterFirstCycle { 1, 1, 2 };
            assert (processor.chopOrder (0) == afterFirstCycle);
            assert (processor.apvts.state.getProperty (Processor::chop1OrderProperty).toString().isNotEmpty());

            processor.cycleSliceOrder (0, 0);
            processor.cycleSliceOrder (0, 0);
            assert (processor.chopOrder (0).empty());         // back to identity: no property
        }

        // Chop off ignores the arrangement.
        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            processor.setChopOrderProperty (0, { 2, 0, 1 });
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
            processOne (processor, midi);
            assert (processor.slotIsActive (0));              // unchopped: the whole loop
        }

        pass ("testSliceOrder");
    }

    /** Plan 16 Phase 3: the loop transformation tools. REV reverses the whole loop (or a
        selected source slice), NORM matches the peak to -0.1 dBFS, TRIM aligns Start and
        Length to the non-silent content. Transforms install as fresh loops (the reference
        is unchanged) and are readable through loopForUi. */
    void testLoopTools()
    {
        // ---- Reverse: the ramp plays backwards -----------------------------------------
        {
            const juce::File loopFile = writeRampLoopFloat ("tool_rev.wav");
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            processor.loadLoopFromFileAsync (0, loopFile, loopFile.getFullPathName());
            for (int i = 0; i < 40 && processor.hasPendingLoopLoad(); ++i)
                juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            assert (! processor.hasPendingLoopLoad());

            setParameter (processor.apvts, Processor::slotCrossfadeIds[0], 0.0f);
            processor.loopToolReverse (0);

            const auto* loop = processor.loopForUi (0);
            assert (loop != nullptr);
            assert (loop->sourcePath == "file://" + loopFile.getFullPathName()
                    || loop->sourcePath == loopFile.getFullPathName());

            // The buffer reversed: frame 0 now holds the loop's last value.
            assert (std::abs (loop->buffer.getSample (0, 0) - (float) (24000 - 1)) < 1.0f);
            assert (std::abs (loop->buffer.getSample (0, 23999) - 0.0f) < 1.0f);

            // And the audio reads the reversed ramp: the last sample of block 2 reads
            // ~1175 frames into the reversed content = 24000 - 1175.
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 100);
            processOne (processor, midi);
            midi.clear();

            juce::AudioBuffer<float> buffer (2, blockSize);
            buffer.clear();
            processor.processBlock (buffer, midi);

            constexpr double gainLaw = 0.8 * 0.7071067811865476;
            const double frame = (double) buffer.getSample (0, blockSize - 1) / gainLaw;

            // The read sits deep in the reversed content (the original ramp's tail): an
            // un-reversed read would be near frame 924. The exact position carries the
            // seam pre-roll and interpolation slop, so only the reversal is asserted.
            assert (frame > 20000.0);
        }

        // ---- Reverse a selected slice: only the middle third flips ---------------------
        {
            const juce::File loopFile = writeRampLoopFloat ("tool_rev_slice.wav");
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            processor.loadLoopFromFileAsync (0, loopFile, loopFile.getFullPathName());
            for (int i = 0; i < 40 && processor.hasPendingLoopLoad(); ++i)
                juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            assert (! processor.hasPendingLoopLoad());

            setParameter (processor.apvts, Processor::slotChopIds[0], 1.0f);
            setParameter (processor.apvts, Processor::slotSlicesIds[0], 3.0f);
            processor.selectChopSlice (0, 1);       // the middle third
            assert (processor.selectedChopSlice (0) == 1);
            processor.selectChopSlice (0, 1);       // again: deselects
            assert (processor.selectedChopSlice (0) == -1);
            processor.selectChopSlice (0, 1);

            processor.loopToolReverse (0);

            const auto* loop = processor.loopForUi (0);
            assert (loop != nullptr);

            // Slice 1 spans [8000, 16000): frame 8000 now holds the old 15999.
            assert (std::abs (loop->buffer.getSample (0, 8000) - (float) 15999) < 1.0f);
            assert (std::abs (loop->buffer.getSample (0, 15999) - 8000.0f) < 1.0f);

            // Outside the slice the ramp is untouched.
            assert (std::abs (loop->buffer.getSample (0, 4000) - 4000.0f) < 1.0f);
            assert (std::abs (loop->buffer.getSample (0, 20000) - 20000.0f) < 1.0f);
        }

        // ---- Normalise: a quiet loop peaks at -0.1 dBFS --------------------------------
        {
            const juce::File loopFile = scratchFolder().getChildFile ("tool_norm.wav");
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> stream (loopFile.createOutputStream());
            juce::AudioFormatWriterOptions options;
            options = options.withSampleRate (sampleRate).withNumChannels (1).withBitsPerSample (32);
            std::unique_ptr<juce::AudioFormatWriter> writer = wav.createWriterFor (stream, options);
            assert (writer != nullptr);

            juce::AudioBuffer<float> quiet (1, 24000);
            for (int i = 0; i < 24000; ++i)
                quiet.setSample (0, i, 0.25f * std::sin ((double) i * 0.05));
            writer->writeFromAudioSampleBuffer (quiet, 0, 24000);
            writer.reset();

            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            processor.loadLoopFromFileAsync (0, loopFile, loopFile.getFullPathName());
            for (int i = 0; i < 40 && processor.hasPendingLoopLoad(); ++i)
                juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            assert (! processor.hasPendingLoopLoad());

            processor.loopToolNormalise (0);

            const auto* loop = processor.loopForUi (0);
            assert (loop != nullptr);
            const double peak = (double) loop->buffer.getMagnitude (0, 0, 24000);
            const double target = std::pow (10.0, -0.1 / 20.0);   // -0.1 dBFS
            assert (std::abs (peak - target) < 0.01);
        }

        // ---- Trim: Start/Length align to the content -----------------------------------
        {
            const juce::File loopFile = scratchFolder().getChildFile ("tool_trim.wav");
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> stream (loopFile.createOutputStream());
            juce::AudioFormatWriterOptions options;
            options = options.withSampleRate (sampleRate).withNumChannels (1).withBitsPerSample (32);
            std::unique_ptr<juce::AudioFormatWriter> writer = wav.createWriterFor (stream, options);
            assert (writer != nullptr);

            const int frames = 24000;
            juce::AudioBuffer<float> padded (1, frames);
            padded.clear();
            for (int i = 2000; i < 21000; ++i)   // content 2000..20999
                padded.setSample (0, i, 0.5f * std::sin ((double) i * 0.05));
            writer->writeFromAudioSampleBuffer (padded, 0, frames);
            writer.reset();

            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            processor.loadLoopFromFileAsync (0, loopFile, loopFile.getFullPathName());
            for (int i = 0; i < 40 && processor.hasPendingLoopLoad(); ++i)
                juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            assert (! processor.hasPendingLoopLoad());

            processor.loopToolTrim (0);

            const double start = getParameter (processor.apvts, Processor::slotStartIds[0]);
            const double length = getParameter (processor.apvts, Processor::slotLengthIds[0]);
            assert (std::abs (start - 2000.0 / 24000.0 * 100.0) < 0.3);
            assert (std::abs (length - 19000.0 / 24000.0 * 100.0) < 0.3);

            // The buffer is untouched by TRIM.
            const auto* loop = processor.loopForUi (0);
            assert (loop != nullptr && loop->buffer.getNumSamples() == frames);
        }

        pass ("testLoopTools");
    }

    /** Writes the editor renders when SUITE_RENDER_DIR is set (plan 05 R9). */
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

        {
            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
            assert (editor != nullptr);

            // One timer tick fills the waveform views (the 60 Hz refresh is what copies
            // the loops into them). The two render passes then run without a tick between
            // them, so the content is identical.
            juce::MessageManager::getInstance()->runDispatchLoopUntil (150);

            assert (PluginTest::writeDeterministicPng (*editor,
                        juce::File (folder).getChildFile ("cyclical-slots.png")));

            if (auto* cyclicalEditor = dynamic_cast<CyclicalEditor*> (editor.get()))
            {
                cyclicalEditor->setPage (1);
                assert (PluginTest::writeDeterministicPng (*editor,
                            juce::File (folder).getChildFile ("cyclical-slot2.png")));

                // Plan 18 Phase 6: the MOD page render (the shared panel in the plugin).
                cyclicalEditor->setPage (CyclicalProcessor::numSlots);
                assert (PluginTest::writeDeterministicPng (*editor,
                            juce::File (folder).getChildFile ("cyclical-mod.png")));
                cyclicalEditor->setPage (0);
            }

            processor.editorBeingDeleted (editor.get());
        }

        // The missing-loop overlay: slot 2's saved path does not resolve anywhere.
        {
            processor.applyLoopsFromState ([] {
                juce::ValueTree tree ("PARAMETERS");
                tree.setProperty ("loop2Path", "Z:/does/not/exist.wav", nullptr);
                return tree;
            }());
            assert (processor.isLoopMissing (1));

            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
            assert (editor != nullptr);

            // The timer copies the missing flag into the views.
            juce::MessageManager::getInstance()->runDispatchLoopUntil (150);

            assert (PluginTest::writeDeterministicPng (*editor,
                        juce::File (folder).getChildFile ("cyclical-slots-missing.png")));
            processor.editorBeingDeleted (editor.get());
        }

        pass ("testRenderBaseline");
    }
}

/** Phase 0 probe: slot 0 low-pass at 300 Hz must strongly attenuate a bright loop vs identity. */
void testProbeFilterDsp()
{
    // A bright loop: 10 kHz sine at 0.5 — far above a 300 Hz low-pass cutoff.
    const juce::File loopFile = scratchFolder().getChildFile ("probe_filter_bright.wav");
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream (loopFile.createOutputStream());
        juce::AudioFormatWriterOptions options;
        options = options.withSampleRate (sampleRate).withNumChannels (1).withBitsPerSample (32);
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream, options));
        assert (writer != nullptr);
        const int frames = (int) sampleRate;   // 1 second
        juce::AudioBuffer<float> buffer (1, frames);
        const float twoPi = 2.0f * (float) juce::MathConstants<double>::pi;
        for (int i = 0; i < frames; ++i)
            buffer.setSample (0, i, 0.5f * std::sin (twoPi * 10000.0f * (float) i / (float) sampleRate));
        writer->writeFromAudioSampleBuffer (buffer, 0, frames);
        writer.reset();
    }

    auto loadAndTrigger = [&] (Processor& p)
    {
        p.prepareToPlay (sampleRate, blockSize);
        p.loadLoopFromFileAsync (0, loopFile, loopFile.getFullPathName());
        for (int i = 0; i < 40 && p.hasPendingLoopLoad(); ++i)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        assert (! p.hasPendingLoopLoad());
        setParameter (p.apvts, Processor::slotLevelIds[0], 100.0f);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
        processOne (p, midi);
        midi.clear();
    };

    // Identity: filter Off.
    {
        Processor processor;
        loadAndTrigger (processor);
        setParameter (processor.apvts, Processor::slotFilterIds[0], 0.0f);
        float acc = 0.0f;
        juce::MidiBuffer idle;
        for (int b = 0; b < 20; ++b)   // ~1 second of audio
        {
            juce::AudioBuffer<float> buf (2, blockSize);
            buf.clear();
            processor.processBlock (buf, idle);
            for (int n = 0; n < blockSize; ++n)
                acc += buf.getSample (0, n) * buf.getSample (0, n);
        }
        const float identityRms = std::sqrt (acc / (20.0f * (float) blockSize));
        std::printf ("   filter probe: identity RMS = %f\n", (double) identityRms);
        assert (std::abs (identityRms) < 1.0f && identityRms > 0.0f);

        // Filter: Low-pass (type 2) at 300 Hz.
        Processor filtered;
        loadAndTrigger (filtered);
        setParameter (filtered.apvts, Processor::slotFilterIds[0], 2.0f);
        setParameter (filtered.apvts, Processor::slotCutoffIds[0], 300.0f);
        acc = 0.0f;
        juce::MidiBuffer idleFiltered;
        for (int b = 0; b < 20; ++b)
        {
            juce::AudioBuffer<float> buf (2, blockSize);
            buf.clear();
            filtered.processBlock (buf, idleFiltered);
            for (int n = 0; n < blockSize; ++n)
                acc += buf.getSample (0, n) * buf.getSample (0, n);
        }
        const float filteredRms = std::sqrt (acc / (20.0f * (float) blockSize));
        std::printf ("   filter probe: filtered RMS = %f\n", (double) filteredRms);
        assert (filteredRms > 0.0f);                  // still bounded, some signal
        assert (filteredRms < identityRms * 0.1f);    // at least 20 dB down: the filter works
    }

    pass ("testProbeFilterDsp");
}

/** Phase 0 probe: slow attack (800 ms) must rise far slower than the 1 ms default. */
void testProbeAttackReleaseDsp()
{
    // A sustained loop: 200 Hz sine at 0.5 amplitude.
    const juce::File loopFile = scratchFolder().getChildFile ("probe_ar_sustain.wav");
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream (loopFile.createOutputStream());
        juce::AudioFormatWriterOptions options;
        options = options.withSampleRate (sampleRate).withNumChannels (1).withBitsPerSample (32);
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream, options));
        assert (writer != nullptr);
        const int frames = (int) sampleRate;
        juce::AudioBuffer<float> buffer (1, frames);
        const float twoPi = 2.0f * (float) juce::MathConstants<double>::pi;
        for (int i = 0; i < frames; ++i)
            buffer.setSample (0, i, 0.5f * std::sin (twoPi * 200.0f * (float) i / (float) sampleRate));
        writer->writeFromAudioSampleBuffer (buffer, 0, frames);
        writer.reset();
    }

    auto measureAttackRms = [&] (float attackMs, float releaseMs)
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        processor.loadLoopFromFileAsync (0, loopFile, loopFile.getFullPathName());
        for (int i = 0; i < 40 && processor.hasPendingLoopLoad(); ++i)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        assert (! processor.hasPendingLoopLoad());
        setParameter (processor.apvts, Processor::slotAttackIds[0], attackMs);
        setParameter (processor.apvts, Processor::slotReleaseIds[0], releaseMs);
        setParameter (processor.apvts, Processor::slotLevelIds[0], 100.0f);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
        processOne (processor, midi);
        midi.clear();

        // RMS over the first 100 ms after the trigger.
        const int measureSamples = (int) (0.1 * sampleRate);   // 4800
        int done = 0;
        float acc = 0.0f;
        juce::MidiBuffer idle;
        while (done < measureSamples)
        {
            const int thisBlock = juce::jmin (blockSize, measureSamples - done);
            juce::AudioBuffer<float> buf (2, thisBlock);
            buf.clear();
            processor.processBlock (buf, idle);
            for (int n = 0; n < thisBlock; ++n)
                acc += buf.getSample (0, n) * buf.getSample (0, n);
            done += thisBlock;
        }
        return std::sqrt (acc / (float) measureSamples);
    };

    const float defaultRms = measureAttackRms (1.0f, 5.0f);
    const float slowRms    = measureAttackRms (800.0f, 2000.0f);
    std::printf ("   A/R probe: default(1ms) RMS = %f, slow(800ms) RMS = %f\n",
                 (double) defaultRms, (double) slowRms);
    assert (defaultRms > 0.0f);
    assert (slowRms < defaultRms * 0.5f);   // the slow attack clearly delays the rise

    pass ("testProbeAttackReleaseDsp");
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::puts ("Running Cyclical plugin tests...");

    testParameters();
    testStateRoundTrip();
    testNewerStateRefused();
    testMinimalStateLoad();
    testDefaultSilence();
    testMidiMap();
    testGateVsLatch();
    testToggleTrigger();
    testToggleTransportStop();
    testSlotNoteRemap();
    testChopRangeDoesNotShadowExactNotes();
    testModeAndPresetSilenceAndAudibleTransform();
    testTempoAndSync();
    testLoopSyncRate();
    testSpeedAffinityUnderSync();
    testChopBoundaries();
    testChopKeyMapping();
    testChopIgnoresSync();
    testChopGranularStaysInsideSlice();
    testWaveformChopLines();
    testAutoChop();
    testAutoChopRegion();
    testSliceOrder();
    testLoopTools();
    testRetriggerRestarts();
    testRegionKnobsApplyLive();
    testProbeFilterDsp();
    testProbeAttackReleaseDsp();
    testMonoBuffer();
    testLatencyAndTail();
    testQuantisedStarts();
    testSwingShiftsOddGridPoints();
    testPendingRules();
    testEmbeddedDefaults();
    testEmbeddedUriFromState();
    testMissingLoopKeepsPlaying();
    testAsyncDiskLoad();
    testStereoAndPan();
    testPresetSeam();
    testHeaderContract();
    testEditor();
    testHelpLine();
    testAffinity();
    testRenderBaseline();

    std::puts ("All Cyclical plugin tests passed.");
    return 0;
}
