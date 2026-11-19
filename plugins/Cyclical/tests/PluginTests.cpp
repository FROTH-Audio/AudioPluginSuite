// Plugin-level tests for Cyclical. Compiles the real processor and editor sources.

#include "PluginTestUtils.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "SuiteSettings.h"

#include <juce_audio_formats/juce_audio_formats.h>

#undef NDEBUG
#include <cassert>

namespace
{
    using namespace PluginTest;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    using Processor = CyclicalProcessor;

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

    void allParameterIds (std::vector<const char*>& ids)
    {
        ids.push_back (Processor::masterVolumeId);
        ids.push_back (Processor::panId);
        ids.push_back (Processor::quantizeId);
        ids.push_back (Processor::swingId);
        ids.push_back (Processor::triggerModeId);

        for (int slot = 0; slot < Processor::numSlots; ++slot)
        {
            ids.push_back (Processor::slotStartIds[(size_t) slot]);
            ids.push_back (Processor::slotLengthIds[(size_t) slot]);
            ids.push_back (Processor::slotSpeedIds[(size_t) slot]);
            ids.push_back (Processor::slotDirectionIds[(size_t) slot]);
            ids.push_back (Processor::slotPingRandomIds[(size_t) slot]);
            ids.push_back (Processor::slotGranularIds[(size_t) slot]);
            ids.push_back (Processor::slotVelocityIds[(size_t) slot]);
        }

        ids.push_back (Processor::pitchShiftId);
        ids.push_back (Processor::crossfadeLengthId);
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
        checkRange (processor.apvts, Processor::panId, -100.0f, 100.0f, 0.0f);
        checkRange (processor.apvts, Processor::quantizeId, 0.0f, 5.0f, 0.0f);
        checkRange (processor.apvts, Processor::swingId, -20.0f, 20.0f, 0.0f);
        checkRange (processor.apvts, Processor::triggerModeId, 0.0f, 1.0f, 0.0f);
        checkRange (processor.apvts, Processor::pitchShiftId, -12.0f, 12.0f, 0.0f);
        checkRange (processor.apvts, Processor::crossfadeLengthId, 0.0f, 500.0f, 5.0f);

        for (int slot = 0; slot < Processor::numSlots; ++slot)
        {
            checkRange (processor.apvts, Processor::slotStartIds[(size_t) slot], 0.0f, 100.0f, 0.0f);
            checkRange (processor.apvts, Processor::slotLengthIds[(size_t) slot], 5.0f, 100.0f, 100.0f);
            checkRange (processor.apvts, Processor::slotSpeedIds[(size_t) slot], 0.25f, 4.0f, 1.0f);
            checkRange (processor.apvts, Processor::slotDirectionIds[(size_t) slot], 0.0f, 2.0f, 0.0f);
            checkRange (processor.apvts, Processor::slotPingRandomIds[(size_t) slot], 0.0f, 1.0f, 0.0f);
            checkRange (processor.apvts, Processor::slotGranularIds[(size_t) slot], 0.0f, 1.0f, 0.0f);
            checkRange (processor.apvts, Processor::slotVelocityIds[(size_t) slot], 30.0f, 150.0f, 100.0f);
        }

        pass ("testParameters");
    }

    void testStateRoundTrip()
    {
        Processor processor;

        checkStateRoundTrip (processor, processor.apvts, Processor::masterVolumeId, -6.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::panId, 40.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::quantizeId, 2.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::swingId, 12.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::triggerModeId, 1.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::pitchShiftId, 5.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::crossfadeLengthId, 120.0f);

        for (int slot = 0; slot < Processor::numSlots; ++slot)
        {
            checkStateRoundTrip (processor, processor.apvts, Processor::slotStartIds[(size_t) slot], 20.0f);
            checkStateRoundTrip (processor, processor.apvts, Processor::slotLengthIds[(size_t) slot], 60.0f);
            checkStateRoundTrip (processor, processor.apvts, Processor::slotSpeedIds[(size_t) slot], 2.0f);
            checkStateRoundTrip (processor, processor.apvts, Processor::slotDirectionIds[(size_t) slot], 1.0f);
            checkStateRoundTrip (processor, processor.apvts, Processor::slotPingRandomIds[(size_t) slot], 1.0f);
            checkStateRoundTrip (processor, processor.apvts, Processor::slotGranularIds[(size_t) slot], 1.0f);
            checkStateRoundTrip (processor, processor.apvts, Processor::slotVelocityIds[(size_t) slot], 80.0f);
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

    /** Gate releases on note-off; Latch ignores it. */
    void testGateVsLatch()
    {
        // Gate: the voice stops after the note-off (5 ms fade, then inactive).
        Processor gate;
        gate.prepareToPlay (sampleRate, blockSize);
        setParameter (gate.apvts, Processor::triggerModeId, 1.0f);

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
        setParameter (processor.apvts, Processor::quantizeId, 1.0f);     // 1/16

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
        setParameter (processor.apvts, Processor::quantizeId, 0.0f);
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
        setParameter (processor.apvts, Processor::quantizeId, 1.0f);     // 1/16
        setParameter (processor.apvts, Processor::swingId, 20.0f);       // +20 %

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
        setParameter (processor.apvts, Processor::quantizeId, 1.0f);
        setParameter (processor.apvts, Processor::triggerModeId, 1.0f);   // Gate

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 100);
        midi.addEvent (juce::MidiMessage::noteOff (1, 60), 200);

        for (int b = 0; b < 14; ++b)
            processOne (processor, midi);
        assert (! processor.slotIsActive (0));      // the tap produced nothing

        // Replace: two triggers in a row produce a single grid start, not two.
        Processor replaced;
        replaced.prepareToPlay (sampleRate, blockSize);
        setParameter (replaced.apvts, Processor::quantizeId, 1.0f);

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
        setParameter (processor.apvts, Processor::panId, 100.0f);
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

    /** Plan 10 G1: the mode-dependent controls are dimmed, never hidden. */
    void testAffinity()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        auto* cyclicalEditor = dynamic_cast<CyclicalEditor*> (editor.get());
        assert (cyclicalEditor != nullptr);

        // Default: forward, no grain. Speed is live; Ping Rand is dimmed outside Ping-Pong.
        assert (cyclicalEditor->isSpeedEnabled (0));
        assert (! cyclicalEditor->isPingRandomEnabled (0));
        assert (cyclicalEditor->isSpeedVisible (0));

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
                            juce::File (folder).getChildFile ("cyclical-global.png")));
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
    testRetriggerRestarts();
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
    testAffinity();
    testRenderBaseline();

    std::puts ("All Cyclical plugin tests passed.");
    return 0;
}
