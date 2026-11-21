// Plugin-level tests for MIDI Chord/Arp/Scale Tool. Compiles the real processor and editor sources.

#include "PluginTestUtils.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "SuiteSettings.h"

#include <random>

namespace
{
    using namespace PluginTest;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    using Processor = MidiEffectProcessor;

    /** Collects the note numbers of the note-on (or note-off) messages in a buffer. */
    std::vector<int> collectNotes (const juce::MidiBuffer& buffer, bool noteOns)
    {
        std::vector<int> notes;
        for (const auto metadata : buffer)
        {
            const auto& message = metadata.getMessage();
            if (noteOns ? message.isNoteOn() : message.isNoteOff())
                notes.push_back (message.getNoteNumber());
        }
        return notes;
    }

    void containsAll (const std::vector<int>& notes, std::initializer_list<int> expected)
    {
        assert ((int) notes.size() == (int) expected.size());
        for (int note : expected)
            assert (std::find (notes.begin(), notes.end(), note) != notes.end());
    }

    void testParameters()
    {
        Processor processor;

        checkParameterIds (processor, processor.apvts,
            { Processor::rootId, Processor::scaleId, Processor::chordId,
              Processor::arpId, Processor::octaveId, Processor::velocityId,
              Processor::arpModeId, Processor::stepRateId, Processor::hostSyncId,
              Processor::tempoId, Processor::gateLengthId, Processor::velocityCurveId,
              Processor::velocityDirectionId, Processor::octaveSpreadId }, 14);
        checkRange (processor.apvts, Processor::rootId,     24.0f, 84.0f, 60.0f);
        checkRange (processor.apvts, Processor::scaleId,    0.0f, 5.0f, 2.0f);
        checkRange (processor.apvts, Processor::chordId,    0.0f, 7.0f, 0.0f);
        checkRange (processor.apvts, Processor::arpId,      0.0f, 4.0f, 0.0f);
        checkRange (processor.apvts, Processor::octaveId,  -2.0f, 2.0f, 0.0f);
        checkRange (processor.apvts, Processor::velocityId, 0.0f, 150.0f, 100.0f);
        checkRange (processor.apvts, Processor::arpModeId,           0.0f, 1.0f, 0.0f);
        checkRange (processor.apvts, Processor::stepRateId,          0.0f, 15.0f, 6.0f);
        checkRange (processor.apvts, Processor::hostSyncId,          0.0f, 1.0f, 0.0f);
        checkRange (processor.apvts, Processor::tempoId,            40.0f, 240.0f, 120.0f);
        checkRange (processor.apvts, Processor::gateLengthId,       10.0f, 100.0f, 60.0f);
        checkRange (processor.apvts, Processor::velocityCurveId,     0.0f, 3.0f, 0.0f);
        checkRange (processor.apvts, Processor::velocityDirectionId, 0.0f, 1.0f, 1.0f);
        checkRange (processor.apvts, Processor::octaveSpreadId,     -1.0f, 1.0f, 0.0f);

        // The appended choice keeps the saved indices: 0-3 are the original patterns.
        auto* arp = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (Processor::arpId));
        assert (arp != nullptr);
        assert (arp->choices.size() == 5);
        assert (arp->choices[0] == "Up" && arp->choices[3] == "Random" && arp->choices[4] == "Scattered");

        // Plan 12: the twelve step rates in the clean order, and the plugin is an instrument.
        auto* stepRate = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (Processor::stepRateId));
        assert (stepRate != nullptr);
        assert (stepRate->choices.size() == 16);
        assert (stepRate->choices[0] == "1/4" && stepRate->choices[2] == "1/4D"
             && stepRate->choices[6] == "1/16" && stepRate->choices[11] == "1/32D");
        // Plan 15: the appended slow plain rates, append-only so a saved index keeps its
        // meaning (the migration stays v2).
        assert (stepRate->choices[12] == "1/2" && stepRate->choices[13] == "1 Bar"
             && stepRate->choices[14] == "2 Bars" && stepRate->choices[15] == "4 Bars");
        pass ("testParameters");
    }

    void testStateRoundTrip()
    {
        Processor processor;

        checkStateRoundTrip (processor, processor.apvts, Processor::rootId,     48.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::scaleId,    4.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::chordId,    6.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::arpId,      2.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::octaveId,  -1.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::velocityId, 75.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::arpModeId,           1.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::stepRateId,          9.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::hostSyncId,          1.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::tempoId,           180.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::gateLengthId,       85.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::velocityCurveId,    2.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::velocityDirectionId, 0.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::octaveSpreadId,      0.5f);
        pass ("testStateRoundTrip");
    }

    /** Plan 6.1: a state saved before the arpeggiator (six parameters) loads with every
        new parameter at its default. The change is additive, so the version stays 1. */
    void testAdditiveStateLoad()
    {
        Processor processor;
        setParameter (processor.apvts, Processor::arpModeId, 1.0f);
        setParameter (processor.apvts, Processor::tempoId, 150.0f);
        setParameter (processor.apvts, Processor::octaveSpreadId, 0.75f);

        juce::MemoryBlock state;
        processor.getStateInformation (state);

        // Strip the eight plan-6.1 parameters from the state tree, like an old session.
        auto xml = SuiteState::detail::decodeXml (state.getData(), (int) state.getSize());
        assert (xml != nullptr);
        auto tree = juce::ValueTree::fromXml (*xml);

        for (const char* id : { Processor::arpModeId, Processor::stepRateId, Processor::hostSyncId,
                                Processor::tempoId, Processor::gateLengthId, Processor::velocityCurveId,
                                Processor::velocityDirectionId, Processor::octaveSpreadId })
        {
            const auto child = tree.getChildWithProperty ("id", juce::String (id));
            assert (child.isValid());
            tree.removeChild (child, nullptr);
        }

        const juce::MemoryBlock block = SuiteState::detail::encodeXml (*tree.createXml());
        processor.setStateInformation (block.getData(), (int) block.getSize());

        assert (std::abs (getParameter (processor.apvts, Processor::arpModeId) - 0.0f) < 1.0e-6f);
        assert (std::abs (getParameter (processor.apvts, Processor::tempoId) - 120.0f) < 1.0e-3f);
        assert (std::abs (getParameter (processor.apvts, Processor::octaveSpreadId) - 0.0f) < 1.0e-6f);
        // The original parameters came back with the stripped state's values (their defaults,
        // because the stripped tree only carried what it had — the round trip above proved
        // the originals restore; here the whole state was re-encoded from the same tree).
        assert (std::abs (getParameter (processor.apvts, Processor::rootId) - 60.0f) < 1.0e-3f);
        pass ("testAdditiveStateLoad");
    }

    /** Plan 12: a v1 state's Step Rate index remaps to the same note value's new position:
        old 2 (1/16) loads as 6, old 0 (1/8) as 3, old 4 (1/32) as 9. */
    void testStepRateMigration()
    {
        static constexpr int oldIndex[5] = { 0, 1, 2, 3, 4 };
        static constexpr int expected[5] = { 3, 4, 6, 7, 9 };

        for (int i = 0; i < 5; ++i)
        {
            Processor processor;
            setParameter (processor.apvts, Processor::stepRateId, (float) oldIndex[i]);

            juce::MemoryBlock state;
            processor.getStateInformation (state);

            auto xml = SuiteState::detail::decodeXml (state.getData(), (int) state.getSize());
            assert (xml != nullptr);
            auto tree = juce::ValueTree::fromXml (*xml);

            // Make it a v1 state, like a session saved before plan 12.
            tree.setProperty ("suiteStateVersion", 1, nullptr);

            const juce::MemoryBlock block = SuiteState::detail::encodeXml (*tree.createXml());
            processor.setStateInformation (block.getData(), (int) block.getSize());

            assert (std::abs (getParameter (processor.apvts, Processor::stepRateId) - (float) expected[i]) < 1.0e-6f);
        }

        pass ("testStepRateMigration");
    }

    /** Plan 12: the plugin is an instrument now — output-only buses, no audio input. */
    void testInstrumentBuses()
    {
        Processor processor;

        assert (processor.getBusCount (true) == 0);    // no input bus
        assert (processor.getBusCount (false) == 1);   // one stereo output
        assert (processor.getBus (false, 0) != nullptr);
        assert (processor.getBus (false, 0)->getName() == "Output");
        assert (processor.acceptsMidi() && processor.producesMidi());

        pass ("testInstrumentBuses");
    }

    /** Plan 06 idea 2.11 first step: a note routed through the chord generator produces the
        expected output note IDs, and the note-off releases the whole chord. Plan 6.1: the
        Chord mode stays the committed behaviour event for event — same positions, same
        velocities, chord mode untouched by the new parameters. */
    void testChordNoteIds()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        // The committed event log: with the defaults (Chord mode), a note-on 60 at 0 with
        // velocity 0.8 fires 60/64/67 at position 0, and the note-off at 100 releases them.
        // New parameters are at their neutral defaults and change nothing here.
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        midi.addEvent (juce::MidiMessage::noteOff (1, 60), 100);

        juce::AudioBuffer<float> audio (2, blockSize);
        audio.clear();
        processor.processBlock (audio, midi);

        std::vector<int> onNotes, offNotes;
        std::vector<float> onVelocities;
        std::vector<int> onPositions, offPositions;

        for (const auto metadata : midi)
        {
            const auto& message = metadata.getMessage();
            if (message.isNoteOn())
            {
                onNotes.push_back (message.getNoteNumber());
                onVelocities.push_back (message.getFloatVelocity());
                onPositions.push_back ((int) metadata.samplePosition);
            }
            else if (message.isNoteOff())
            {
                offNotes.push_back (message.getNoteNumber());
                offPositions.push_back ((int) metadata.samplePosition);
            }
        }

        containsAll (onNotes, { 60, 64, 67 });
        containsAll (offNotes, { 60, 64, 67 });
        for (int position : onPositions)
            assert (position == 0);
        for (int position : offPositions)
            assert (position == 100);
        for (float velocity : onVelocities)
            assert (std::abs (velocity - 0.8f) < 0.02f);   // 7-bit quantisation inside MidiMessage
        pass ("testChordNoteIds");
    }

    /** The note is quantised to the scale before the chord is built on it. */
    void testScaleQuantisationMovesTheChordRoot()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        // Scale Major (1): note 61 ties to the root side, so the chord lands on 60.
        setParameter (processor.apvts, Processor::scaleId, 1.0f);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 61, 0.8f), 0);
        juce::AudioBuffer<float> audio (2, blockSize);
        audio.clear();
        processor.processBlock (audio, midi);
        containsAll (collectNotes (midi, true), { 60, 64, 67 });

        // Note 66 (F#) is out of the scale and sits nearer F (65) than G (67).
        midi.clear();
        midi.addEvent (juce::MidiMessage::noteOn (1, 66, 0.8f), 0);
        processor.processBlock (audio, midi);
        containsAll (collectNotes (midi, true), { 65, 69, 72 });

        // Release the held chord from the first part before the next check on the same key.
        midi.clear();
        midi.addEvent (juce::MidiMessage::noteOff (1, 61), 0);
        processor.processBlock (audio, midi);

        // Chromatic scale: no quantisation, the chord root is the note itself.
        setParameter (processor.apvts, Processor::scaleId, 0.0f);
        midi.clear();
        midi.addEvent (juce::MidiMessage::noteOn (1, 61, 0.8f), 0);
        processor.processBlock (audio, midi);
        containsAll (collectNotes (midi, true), { 61, 65, 68 });
        pass ("testScaleQuantisationMovesTheChordRoot");
    }

    /** Octave shifts the chord; velocity scales the note-on velocity (0-1 floats). */
    void testOctaveAndVelocity()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        setParameter (processor.apvts, Processor::octaveId, -1.0f);
        setParameter (processor.apvts, Processor::velocityId, 50.0f);   // half velocity

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        juce::AudioBuffer<float> audio (2, blockSize);
        audio.clear();
        processor.processBlock (audio, midi);

        containsAll (collectNotes (midi, true), { 48, 52, 55 });
        for (const auto metadata : midi)
            if (metadata.getMessage().isNoteOn())
            {
                // Velocities are 7-bit inside MidiMessage, so allow the quantisation step.
                assert (std::abs (metadata.getMessage().getFloatVelocity() - 0.4f) < 0.02f);
            }
        pass ("testOctaveAndVelocity");
    }

    /** A retrigger before note-off releases the previous chord first, so nothing hangs. */
    void testRetriggerReleasesPreviousChord()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 10);

        juce::AudioBuffer<float> audio (2, blockSize);
        audio.clear();
        processor.processBlock (audio, midi);

        // The second note-on emitted the first chord's note-offs, then its own note-ons.
        assert ((int) collectNotes (midi, false).size() == 3);
        assert ((int) collectNotes (midi, true).size() == 6);
        pass ("testRetriggerReleasesPreviousChord");
    }

    /** Plan 6.1: the appended Scattered pattern orders the chord in Chord mode too, and is
        deterministic for the same key. */
    void testScatteredPattern()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameter (processor.apvts, Processor::arpId, 4.0f);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        juce::AudioBuffer<float> audio (2, blockSize);
        audio.clear();
        processor.processBlock (audio, midi);

        const auto first = collectNotes (midi, true);
        containsAll (first, { 60, 64, 67 });   // a permutation of the same chord

        // Same key, deterministic: after releasing, the scattered order repeats.
        midi.clear();
        midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
        processor.processBlock (audio, midi);
        midi.clear();
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        processor.processBlock (audio, midi);
        const auto second = collectNotes (midi, true);
        assert (first == second);
        pass ("testScatteredPattern");
    }

    /** Plan 6.1: the arp step clock. At 120 BPM, 1/16 steps land every 6000 samples, and
        the 8-step sequence cycles the ordered chord. */
    void testArpStepClock()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        setParameter (processor.apvts, Processor::arpModeId, 1.0f);

        juce::AudioBuffer<float> audio (2, blockSize);
        std::vector<std::pair<int, int>> noteOns;      // (absolute sample, note)
        std::vector<std::pair<int, int>> noteOffs;
        const int totalBlocks = (int) (47000 / blockSize);   // 8 steps at 6000 samples fit inside

        for (int b = 0; b < totalBlocks; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);

            audio.clear();
            processor.processBlock (audio, midi);

            for (const auto metadata : midi)
            {
                const auto& message = metadata.getMessage();
                const int absolute = b * blockSize + (int) metadata.samplePosition;

                if (message.isNoteOn())
                    noteOns.push_back ({ absolute, message.getNoteNumber() });
                else if (message.isNoteOff())
                    noteOffs.push_back ({ absolute, message.getNoteNumber() });
            }
        }

        // Eight steps, cycling [60, 64, 67] twice, at one step per 6000 samples.
        const int expected[8] = { 60, 64, 67, 60, 64, 67, 60, 64 };
        assert ((int) noteOns.size() == 8);
        for (int s = 0; s < 8; ++s)
        {
            assert (noteOns[(size_t) s].second == expected[s]);
            assert (std::abs (noteOns[(size_t) s].first - s * 6000) <= 2);
        }

        // Gate 60 % of a step: each note ends 3600 samples after it started; step 7's
        // gate-off at 45600 is also inside the run.
        assert ((int) noteOffs.size() == 8);
        for (int i = 0; i < 8; ++i)
        {
            assert (noteOffs[(size_t) i].second == expected[i]);
            assert (std::abs (noteOffs[(size_t) i].first - (i * 6000 + 3600)) <= 2);
        }
        pass ("testArpStepClock");
    }

    /** Plan 15: the appended slow step rates. At 120 BPM a sample is 48 kHz, so one beat is
        24000 samples: 1 Bar (index 13) is 96000 samples per step and 4 Bars (index 15) is
        384000 — the clock fires the first step immediately and then waits, which is what
        the long rates are for. */
    void testArpStepClockSlowRates()
    {
        juce::AudioBuffer<float> audio (2, blockSize);

        // 1 Bar: two steps (96000 samples apart) inside a 97000-sample run; the first
        // note's gate-off (60 % of 96000 = 57600) lands between them.
        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            setParameter (processor.apvts, Processor::arpModeId, 1.0f);
            setParameter (processor.apvts, Processor::stepRateId, 13.0f);

            std::vector<std::pair<int, int>> noteOns, noteOffs;
            const int totalBlocks = 97000 / blockSize;

            for (int b = 0; b < totalBlocks; ++b)
            {
                juce::MidiBuffer midi;
                if (b == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
                audio.clear();
                processor.processBlock (audio, midi);

                for (const auto metadata : midi)
                {
                    const auto& message = metadata.getMessage();
                    const int absolute = b * blockSize + (int) metadata.samplePosition;
                    if (message.isNoteOn())
                        noteOns.push_back ({ absolute, message.getNoteNumber() });
                    else if (message.isNoteOff())
                        noteOffs.push_back ({ absolute, message.getNoteNumber() });
                }
            }

            assert ((int) noteOns.size() == 2);
            assert (noteOns[0].second == 60 && noteOns[1].second == 64);
            assert (std::abs (noteOns[1].first - 96000) <= 2);
            assert ((int) noteOffs.size() == 1);
            assert (noteOffs[0].second == 60 && std::abs (noteOffs[0].first - 57600) <= 2);
        }

        // 4 Bars: one step per 384000 samples — the run's single note-on at step 0, and its
        // gate-off scheduled beyond the run.
        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            setParameter (processor.apvts, Processor::arpModeId, 1.0f);
            setParameter (processor.apvts, Processor::stepRateId, 15.0f);

            int noteOns = 0, noteOffs = 0;
            const int totalBlocks = 97000 / blockSize;

            for (int b = 0; b < totalBlocks; ++b)
            {
                juce::MidiBuffer midi;
                if (b == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
                audio.clear();
                processor.processBlock (audio, midi);

                for (const auto metadata : midi)
                {
                    const auto& message = metadata.getMessage();
                    if (message.isNoteOn())  ++noteOns;
                    else if (message.isNoteOff()) ++noteOffs;
                }
            }

            assert (noteOns == 1 && noteOffs == 0);
        }

        pass ("testArpStepClockSlowRates");
    }

    /** Plan 6.1: the step clock holds the span at any tempo. At 40 BPM a 1/16 step is
        18000 samples, at 240 it is 3000; the eight steps land accordingly. */
    void testArpStepClockAtTempos()
    {
        for (const float tempo : { 40.0f, 120.0f, 240.0f })
        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            setParameter (processor.apvts, Processor::arpModeId, 1.0f);
            setParameter (processor.apvts, Processor::tempoId, tempo);

            // One 1/16 step at this tempo, and a run long enough for all 8 steps but short
            // enough that step 8 (which would make it nine) never lands.
            const int stepSamples = (int) std::lround (0.25 * 60.0 / (double) tempo * sampleRate);
            const int runSamples = (int) (7.5 * stepSamples);
            const int totalBlocks = runSamples / blockSize + 1;

            juce::AudioBuffer<float> audio (2, blockSize);
            std::vector<int> noteOnSamples;

            for (int b = 0; b < totalBlocks; ++b)
            {
                juce::MidiBuffer midi;
                if (b == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);

                audio.clear();
                processor.processBlock (audio, midi);

                for (const auto metadata : midi)
                    if (metadata.getMessage().isNoteOn())
                        noteOnSamples.push_back (b * blockSize + (int) metadata.samplePosition);
            }

            // Eight note-ons, one per step, at stepSamples apart.
            assert ((int) noteOnSamples.size() == 8);
            for (int s = 1; s < 8; ++s)
                assert (std::abs (noteOnSamples[(size_t) s] - noteOnSamples[(size_t) (s - 1)] - stepSamples) <= 2);
        }

        pass ("testArpStepClockAtTempos");
    }

    /** Plan 6.1: the velocity curve shapes the per-step velocity; Flat keeps it uniform. */
    void testArpVelocityCurve()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameter (processor.apvts, Processor::arpModeId, 1.0f);
        setParameter (processor.apvts, Processor::velocityCurveId, 1.0f);        // linear
        setParameter (processor.apvts, Processor::velocityDirectionId, 0.0f);    // ascending

        juce::AudioBuffer<float> audio (2, blockSize);
        std::vector<float> velocities;
        const int totalBlocks = (int) (47000 / blockSize);

        for (int b = 0; b < totalBlocks; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);

            audio.clear();
            processor.processBlock (audio, midi);

            for (const auto metadata : midi)
                if (metadata.getMessage().isNoteOn())
                    velocities.push_back (metadata.getMessage().getFloatVelocity());
        }

        assert ((int) velocities.size() == 8);
        // Linear ascending: step 0 at 0.2 of the curve, step 7 at 1.0. Base 0.8 x Velocity 1.
        assert (std::abs (velocities[0] - 0.16f) < 0.02f);
        assert (std::abs (velocities[7] - 0.8f) < 0.02f);
        for (int s = 1; s < 8; ++s)
            assert (velocities[(size_t) s] > velocities[(size_t) (s - 1)] - 1.0e-3f);
        pass ("testArpVelocityCurve");
    }

    /** Plan 6.1: a note-off truncates the arp immediately: the sounding note stops, the
        scheduled gate-off is removed, and no further steps fire. */
    void testArpNoteOffTruncates()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameter (processor.apvts, Processor::arpModeId, 1.0f);

        juce::AudioBuffer<float> audio (2, blockSize);
        std::vector<std::pair<int, int>> noteOns, noteOffs;

        for (int b = 0; b < 6; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
            else if (b == 1)
                midi.addEvent (juce::MidiMessage::noteOff (1, 60), 100);

            audio.clear();
            processor.processBlock (audio, midi);

            for (const auto metadata : midi)
            {
                const auto& message = metadata.getMessage();
                const int absolute = b * blockSize + (int) metadata.samplePosition;

                if (message.isNoteOn())
                    noteOns.push_back ({ absolute, message.getNoteNumber() });
                else if (message.isNoteOff())
                    noteOffs.push_back ({ absolute, message.getNoteNumber() });
            }
        }

        // Step 0 fired at 0 and was truncated at 612; the gate-off scheduled for 3600 is
        // gone, and the arp never reached step 1 (6000).
        assert ((int) noteOns.size() == 1);
        assert (noteOns[0].first == 0);
        assert ((int) noteOffs.size() == 1);
        assert (noteOffs[0].first == blockSize + 100);
        pass ("testArpNoteOffTruncates");
    }

    /** Plan 6.1: an arp retrigger cuts the sounding notes before restarting. */
    void testArpRetrigger()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameter (processor.apvts, Processor::arpModeId, 1.0f);

        juce::AudioBuffer<float> audio (2, blockSize);
        std::vector<std::pair<int, int>> ons, offs;

        for (int b = 0; b < 3; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
            else if (b == 1)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 10);

            audio.clear();
            processor.processBlock (audio, midi);

            for (const auto metadata : midi)
            {
                const auto& message = metadata.getMessage();
                const int absolute = b * blockSize + (int) metadata.samplePosition;

                if (message.isNoteOn())
                    ons.push_back ({ absolute, message.getNoteNumber() });
                else if (message.isNoteOff())
                    offs.push_back ({ absolute, message.getNoteNumber() });
            }
        }

        // Step 0 fired at 0; the retrigger cut it at 522 and restarted, firing step 0 there.
        assert ((int) ons.size() == 2);
        assert (ons[0].first == 0 && ons[0].second == 60);
        assert (ons[1].first == blockSize + 10 && ons[1].second == 60);
        assert ((int) offs.size() == 1);
        assert (offs[0].first == blockSize + 10 && offs[0].second == 60);
        pass ("testArpRetrigger");
    }

    /** Plan 6.1: the reported latency is a fixed 50 ms, whatever the rate or gate. */
    void testLatencyIsFixed()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        assert (processor.getLatencySamples() == (int) std::lround (0.05 * sampleRate));

        setParameter (processor.apvts, Processor::arpModeId, 1.0f);
        setParameter (processor.apvts, Processor::stepRateId, 4.0f);
        setParameter (processor.apvts, Processor::gateLengthId, 100.0f);
        assert (processor.getLatencySamples() == (int) std::lround (0.05 * sampleRate));
        pass ("testLatencyIsFixed");
    }

    /** Plan 12: the plugin is an instrument, so the audio buffer is cleared, not passed
        through — in both modes. */
    void testAudioCleared()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);

        for (float arpMode : { 0.0f, 1.0f })
        {
            setParameter (processor.apvts, Processor::arpModeId, arpMode);

            juce::AudioBuffer<float> buffer (2, blockSize);
            buffer.clear();
            addSine (buffer, 0, sampleRate, 440.0, 0.5f, 0);
            addSine (buffer, 1, sampleRate, 880.0, 0.25f, 0);
            processor.processBlock (buffer, midi);

            for (int ch = 0; ch < 2; ++ch)
                for (int n = 0; n < blockSize; ++n)
                    assert (buffer.getSample (ch, n) == 0.0f);
        }

        pass ("testAudioCleared");
    }

    void testEditor()
    {
        Processor processor;
        checkEditor (processor);
        pass ("testEditor");
    }

    /** Plan 12: the five dropdowns, the three pills, the help line and the group frames. */
    void testEditorControls()
    {
        Processor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        assert (editor != nullptr);

        std::function<int (juce::Component&, bool)> countChoices = [&] (juce::Component& root, bool dropdown)
            -> int
        {
            int count = 0;
            if (dropdown ? dynamic_cast<SuiteUI::LabelledDropdown*> (&root) != nullptr
                         : dynamic_cast<SuiteUI::LabelledSelector*> (&root) != nullptr)
                ++count;
            for (auto* child : root.getChildren())
                count += countChoices (*child, dropdown);
            return count;
        };

        assert (countChoices (*editor, true) == 5);    // Scale, Chord, Pattern, Step Rate, Vel Curve
        assert (countChoices (*editor, false) == 3);   // Mode, Sync, Vel Dir
        assert (countChoices (*editor, false) != 0);

        std::function<juce::Component* (juce::Component&)> findSlider = [&] (juce::Component& root)
            -> juce::Component*
        {
            if (auto* cell = dynamic_cast<SuiteUI::LabelledSlider*> (&root))
                if (cell->label.getText() == "ROOT")
                    return cell;
            for (auto* child : root.getChildren())
                if (auto* found = findSlider (*child))
                    return found;
            return nullptr;
        };

        auto* slider = findSlider (*editor);
        assert (slider != nullptr && slider->getHelpText().isNotEmpty());
        PluginTest::checkHelpLine (*editor, "midieffect-help", *slider);

        processor.editorBeingDeleted (editor.get());
        pass ("testEditorControls");
    }

    /** Plan 15 (owner): the arp-only controls are dimmed in Chord mode (no clock, no gate,
        no per-step shape) and live in Arp mode. The chord parameters and the Arp pattern's
        ordering stay live in both (plan 10 G1: dimmed, never hidden). */
    void testModeAffinity()
    {
        Processor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        assert (editor != nullptr);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (120);   // one timer tick

        auto* midiEditor = dynamic_cast<MidiEffectEditor*> (editor.get());
        assert (midiEditor != nullptr);

        // Default mode is Chord: the seven arp-only cells are dimmed.
        assert (! midiEditor->isArpRowEnabled());

        // Arp mode: live again.
        setParameter (processor.apvts, Processor::arpModeId, 1.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
        assert (midiEditor->isArpRowEnabled());

        // Back to Chord: dimmed again.
        setParameter (processor.apvts, Processor::arpModeId, 0.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
        assert (! midiEditor->isArpRowEnabled());

        processor.editorBeingDeleted (editor.get());
        pass ("testModeAffinity");
    }

    /** Plan 11 D7: the visual snapshot publishes the held key while the arp runs and
        clears its sounding flags when the key ends. */
    void testVisualSnapshot()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameter (processor.apvts, Processor::arpModeId, 1.0f);   // Arp

        // Idle: no key published.
        {
            juce::MidiBuffer empty;
            juce::AudioBuffer<float> audio (2, blockSize);
            audio.clear();
            processor.processBlock (audio, empty);
            assert (processor.getArpSnapshot().key.load() == -1);
        }

        // A note-on publishes the key and its 8-step sequence.
        {
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.9f), 0);
            juce::AudioBuffer<float> audio (2, blockSize);
            audio.clear();
            processor.processBlock (audio, midi);

            assert (processor.getArpSnapshot().key.load() == 60);

            int filled = 0;
            for (int i = 0; i < Processor::arpSteps; ++i)
                if (processor.getArpSnapshot().notes[(size_t) i].load() >= 0)
                    ++filled;
            assert (filled > 0);
        }

        // The note-off clears every sounding flag; the sequence stays for the display.
        {
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            juce::AudioBuffer<float> audio (2, blockSize);
            audio.clear();
            processor.processBlock (audio, midi);

            assert (processor.getArpSnapshot().key.load() == 60);
            for (int i = 0; i < Processor::arpSteps; ++i)
                assert (! processor.getArpSnapshot().sounding[(size_t) i].load());
        }

        pass ("testVisualSnapshot");
    }

    /** R13: the header carries the preset bar and the menu menu, the menu's two actions are
        wired, the menu sits at the far right of the header with the bar left of it, and both
        dialogs build and paint. */
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
            SuiteUI::AboutPanel about ({}, "MIDI Chord Arp Scale Tool", "1.0.0", "Marc", "MidiEffect");
            const auto image = paintComponent (about);
            assert (image.getPixelAt (8, 8) == SuiteUI::Colours::panel);
        }

        {
            const auto file = juce::File::getCurrentWorkingDirectory()
                                  .getChildFile ("MidiEffectHeaderContractSettings.xml");
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

    /** Writes the editor render when SUITE_RENDER_DIR is set (plan 05 R9). */
    //==============================================================================
    // The UI-note guarantee (plan 13): the editor's keyboard and chord click play through
    // the same chord/arp logic, and every ending path sends the matching note-offs.

    struct TestPlayHead : juce::AudioPlayHead
    {
        juce::Optional<PositionInfo> position;
        juce::Optional<PositionInfo> getPosition() const override { return position; }
    };

    /** Runs one block and counts the note-on / note-off events the plugin emitted. */
    void runBlockCounting (Processor& processor, int& ons, int& offs)
    {
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> silence (2, blockSize);
        silence.clear();
        processor.processBlock (silence, midi);

        for (const auto metadata : midi)
        {
            const auto& message = metadata.getMessage();
            if (message.isNoteOn())
                ++ons;
            else if (message.isNoteOff())
                ++offs;
        }
    }

    template <class ComponentType>
    ComponentType* findChildOfTypeDeep (juce::Component* parent)
    {
        for (int i = 0; i < parent->getNumChildComponents(); ++i)
        {
            auto* child = parent->getChildComponent (i);

            if (auto* match = dynamic_cast<ComponentType*> (child))
                return match;

            if (auto* deeper = findChildOfTypeDeep<ComponentType> (child))
                return deeper;
        }

        return nullptr;
    }

    /** A pointer event at 80% of the strip's height (below the black-key band). */
    auto keyEventOn = [] (juce::Component& component, float x)
    {
        const float y = component.getHeight() * 0.8f;
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(),
                                 juce::Point<float> (x, y), juce::ModifierKeys(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &component, &component, juce::Time::getCurrentTime(),
                                 juce::Point<float> (x, y), juce::Time::getCurrentTime(), 1, false);
    };

    /** The x of a key's centre on the strip (73 keys, MIDI 24-96). */
    auto keyX = [] (juce::Component& strip, int note)
    {
        return ((float) (note - 24) + 0.5f) / 73.0f * (float) strip.getWidth();
    };

    void testUiNoteChordMatchesHostMidi()
    {
        Processor ui, host;
        ui.prepareToPlay (sampleRate, blockSize);
        host.prepareToPlay (sampleRate, blockSize);

        ui.pushUiNote (60, true, 0.8f);
        juce::MidiBuffer uiMidi, hostMidi;
        hostMidi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        juce::AudioBuffer<float> silence (2, blockSize);
        silence.clear();
        ui.processBlock (silence, uiMidi);
        host.processBlock (silence, hostMidi);

        // The UI note takes the same chord path: identical notes in the same order.
        const auto uiNotes = collectNotes (uiMidi, true);
        const auto hostNotes = collectNotes (hostMidi, true);
        assert (uiNotes == hostNotes);
        assert (! uiNotes.empty());

        // The bookkeeping holds the note until its note-off.
        assert (ui.hasUiNotesSounding());
        ui.pushUiNote (60, false);
        juce::MidiBuffer offMidi;
        ui.processBlock (silence, offMidi);
        assert (! collectNotes (offMidi, false).empty());
        assert (! ui.hasUiNotesSounding());

        pass ("testUiNoteChordMatchesHostMidi");
    }

    /** One test per ending path (plan 13): each asserts note-ons equal note-offs and no
        UI note sounding afterwards. */
    void testUiNoteEndingPaths()
    {
        // -- mouse-up on the key ------------------------------------------------------
        {
            Processor processor;
            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
            juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
            auto* strip = findChildOfTypeDeep<SuiteUI::PlayableKeyboardStrip> (editor.get());
            assert (strip != nullptr);

            int ons = 0, offs = 0;
            strip->mouseDown (keyEventOn (*strip, keyX (*strip, 60)));
            runBlockCounting (processor, ons, offs);
            assert (ons > 0 && processor.hasUiNotesSounding());

            strip->mouseUp (keyEventOn (*strip, keyX (*strip, 60)));
            runBlockCounting (processor, ons, offs);
            assert (ons == offs);
            assert (! processor.hasUiNotesSounding());

            processor.editorBeingDeleted (editor.get());
        }

        // -- a drag that leaves the key ----------------------------------------------
        {
            Processor processor;
            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
            juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
            auto* strip = findChildOfTypeDeep<SuiteUI::PlayableKeyboardStrip> (editor.get());
            assert (strip != nullptr);

            int ons = 0, offs = 0;
            strip->mouseDown (keyEventOn (*strip, keyX (*strip, 60)));
            strip->mouseDrag (keyEventOn (*strip, keyX (*strip, 64)));
            runBlockCounting (processor, ons, offs);
            assert (processor.hasUiNotesSounding());

            strip->mouseUp (keyEventOn (*strip, keyX (*strip, 64)));
            runBlockCounting (processor, ons, offs);
            assert (ons == offs);
            assert (! processor.hasUiNotesSounding());

            processor.editorBeingDeleted (editor.get());
        }

        // -- the mouse leaving the window ---------------------------------------------
        {
            Processor processor;
            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
            juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
            auto* strip = findChildOfTypeDeep<SuiteUI::PlayableKeyboardStrip> (editor.get());
            assert (strip != nullptr);

            int ons = 0, offs = 0;
            strip->mouseDown (keyEventOn (*strip, keyX (*strip, 60)));
            strip->mouseExit (keyEventOn (*strip, keyX (*strip, 60)));
            runBlockCounting (processor, ons, offs);
            assert (ons == offs);
            assert (! processor.hasUiNotesSounding());

            processor.editorBeingDeleted (editor.get());
        }

        // -- the editor destroyed while a key is held ----------------------------------
        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            int ons = 0, offs = 0;

            {
                std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
                juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
                auto* strip = findChildOfTypeDeep<SuiteUI::PlayableKeyboardStrip> (editor.get());
                assert (strip != nullptr);

                strip->mouseDown (keyEventOn (*strip, keyX (*strip, 60)));
                runBlockCounting (processor, ons, offs);
                assert (processor.hasUiNotesSounding());

                processor.editorBeingDeleted (editor.get());
            }   // the editor's destructor flushes the held note into the ring

            runBlockCounting (processor, ons, offs);
            assert (ons == offs);
            assert (! processor.hasUiNotesSounding());
        }

        // -- focus lost while a key is held --------------------------------------------
        {
            Processor processor;
            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
            juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
            auto* strip = findChildOfTypeDeep<SuiteUI::PlayableKeyboardStrip> (editor.get());
            assert (strip != nullptr);

            int ons = 0, offs = 0;
            strip->mouseDown (keyEventOn (*strip, keyX (*strip, 60)));
            runBlockCounting (processor, ons, offs);
            assert (processor.hasUiNotesSounding());

            editor->focusLost (juce::Component::focusChangedByMouseClick);
            runBlockCounting (processor, ons, offs);
            assert (ons == offs);
            assert (! processor.hasUiNotesSounding());

            processor.editorBeingDeleted (editor.get());
        }

        // -- a chord-strip click ends on mouse-up and on the mouse leaving --------------
        {
            Processor processor;
            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
            juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
            auto* visualizer = findChildOfTypeDeep<MidiEffectEditor::ChordGridVisualizer> (editor.get());
            assert (visualizer != nullptr);

            int ons = 0, offs = 0;

            // No chord yet: a click is a no-op.
            visualizer->mouseDown (keyEventOn (*visualizer, 100.0f));
            assert (visualizer->heldChordKey < 0);

            // A key press gives the strip a chord; a click then replays it.
            processor.pushUiNote (60, true, 0.8f);
            runBlockCounting (processor, ons, offs);
            const int onsAfterKey = ons;

            visualizer->mouseDown (keyEventOn (*visualizer, 100.0f));
            assert (visualizer->heldChordKey >= 0);
            runBlockCounting (processor, ons, offs);
            assert (ons > onsAfterKey);

            visualizer->mouseUp (keyEventOn (*visualizer, 100.0f));
            runBlockCounting (processor, ons, offs);
            assert (visualizer->heldChordKey < 0);

            // Everything the key and the click started is released when the key ends.
            processor.pushUiNote (60, false);
            runBlockCounting (processor, ons, offs);
            assert (ons == offs);
            assert (! processor.hasUiNotesSounding());

            processor.editorBeingDeleted (editor.get());
        }

        // -- transport stop -------------------------------------------------------------
        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);

            TestPlayHead playHead;
            {
                juce::AudioPlayHead::PositionInfo info;
                info.setIsPlaying (true);
                playHead.position = info;
            }
            processor.setPlayHead (&playHead);

            int ons = 0, offs = 0;
            runBlockCounting (processor, ons, offs);   // the host is playing
            processor.pushUiNote (60, true, 0.8f);
            runBlockCounting (processor, ons, offs);
            assert (processor.hasUiNotesSounding());

            {
                juce::AudioPlayHead::PositionInfo info;
                info.setIsPlaying (false);
                playHead.position = info;
            }
            runBlockCounting (processor, ons, offs);
            assert (ons == offs);
            assert (! processor.hasUiNotesSounding());

            processor.setPlayHead (nullptr);
        }

        // -- reset and releaseResources --------------------------------------------------
        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            int ons = 0, offs = 0;

            processor.pushUiNote (60, true, 0.8f);
            runBlockCounting (processor, ons, offs);
            assert (processor.hasUiNotesSounding());

            processor.reset();
            assert (! processor.hasUiNotesSounding());

            processor.pushUiNote (64, true, 0.8f);
            runBlockCounting (processor, ons, offs);
            assert (processor.hasUiNotesSounding());

            processor.releaseResources();
            assert (! processor.hasUiNotesSounding());
        }

        // -- a randomised click sequence with a fixed seed balances ----------------------
        {
            Processor processor;
            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
            juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
            auto* strip = findChildOfTypeDeep<SuiteUI::PlayableKeyboardStrip> (editor.get());
            assert (strip != nullptr);

            std::mt19937 random (2026u);
            std::uniform_real_distribution<float> position (0.0f, 1.0f);
            std::uniform_int_distribution<int> action (0, 3);
            int ons = 0, offs = 0;

            for (int i = 0; i < 64; ++i)
            {
                const float x = keyX (*strip, 24 + (int) (position (random) * 72.0f));

                switch (action (random))
                {
                    case 0: strip->mouseDown (keyEventOn (*strip, x)); break;
                    case 1: strip->mouseUp (keyEventOn (*strip, x)); break;
                    case 2: strip->mouseDrag (keyEventOn (*strip, x)); break;
                    default: strip->mouseExit (keyEventOn (*strip, x)); break;
                }

                runBlockCounting (processor, ons, offs);
            }

            // The flush releases whatever is left, so the books balance exactly.
            strip->releaseHeldNotes();
            runBlockCounting (processor, ons, offs);
            assert (ons == offs);
            assert (! processor.hasUiNotesSounding());

            processor.editorBeingDeleted (editor.get());
        }

        pass ("testUiNoteEndingPaths");
    }

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
            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
            assert (editor != nullptr);
            const juce::File file (juce::File (folder).getChildFile ("midieffect-default.png"));
            assert (PluginTest::writeDeterministicPng (*editor, file));
            processor.editorBeingDeleted (editor.get());
        }

        // Arp mode render: both modes of the plugin get a manual screenshot (rule R9).
        {
            setParameter (processor.apvts, Processor::arpModeId, 1.0f);
            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
            assert (editor != nullptr);
            const juce::File file (juce::File (folder).getChildFile ("midieffect-arp.png"));
            assert (PluginTest::writeDeterministicPng (*editor, file));
            processor.editorBeingDeleted (editor.get());
        }

        pass ("testRenderBaseline");
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::puts ("Running MIDI Chord/Arp/Scale Tool plugin tests...");

    testParameters();
    testStateRoundTrip();
    testAdditiveStateLoad();
    testStepRateMigration();
    testInstrumentBuses();
    testChordNoteIds();
    testScaleQuantisationMovesTheChordRoot();
    testOctaveAndVelocity();
    testRetriggerReleasesPreviousChord();
    testScatteredPattern();
    testArpStepClock();
    testArpStepClockSlowRates();
    testArpStepClockAtTempos();
    testArpVelocityCurve();
    testArpNoteOffTruncates();
    testArpRetrigger();
    testLatencyIsFixed();
    testAudioCleared();
    testEditor();
    testEditorControls();
    testModeAffinity();
    testVisualSnapshot();
    testUiNoteChordMatchesHostMidi();
    testUiNoteEndingPaths();
    testHeaderContract();
    testRenderBaseline();

    std::puts ("All MIDI Chord/Arp/Scale Tool plugin tests passed.");
    return 0;
}
