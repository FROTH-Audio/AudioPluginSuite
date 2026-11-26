// Plugin-level tests for Groove Mutator. Compiles the real processor and editor sources.

#include "PluginTestUtils.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "SuiteSettings.h"

#include <algorithm>
#include <cmath>
#include <vector>

#undef NDEBUG
#include <cassert>

namespace
{
    using namespace PluginTest;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    using Processor = GrooveMutatorProcessor;

    // A host playhead whose transport and tempo the test controls (the MTC pattern).
    struct FakePlayHead : juce::AudioPlayHead
    {
        bool playing = false;
        double ppq = 0.0;
        double bpm = 120.0;

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setIsPlaying (playing);
            info.setPpqPosition (ppq);
            info.setBpm (bpm);
            return info;
        }
    };

    struct Delivered
    {
        std::int64_t sample;
        juce::MidiMessage message;
    };

    struct Injected
    {
        int block;
        juce::MidiMessage message;
    };

    // Runs the processor for a number of blocks, starting at globalSample, and returns
    // every MIDI message with its absolute time. Injected messages enter their block. A
    // playhead advances by the block at the host tempo after each block (a real host's
    // PPQ moves continuously; a static one looks like a jump to every block).
    std::vector<Delivered> run (Processor& processor, std::int64_t& globalSample, int blocks,
                                const std::vector<Injected>& injected = {},
                                FakePlayHead* playhead = nullptr)
    {
        std::vector<Delivered> out;
        juce::AudioBuffer<float> audio (2, blockSize);

        for (int block = 0; block < blocks; ++block)
        {
            juce::MidiBuffer midi;
            for (const auto& entry : injected)
                if (entry.block == block)
                    midi.addEvent (entry.message, 0);

            audio.clear();
            const std::int64_t start = globalSample;

            processor.processBlock (audio, midi);

            if (playhead != nullptr && playhead->playing)
                playhead->ppq += blockSize * playhead->bpm / 60.0 / sampleRate;

            for (const auto meta : midi)
                out.push_back ({ start + meta.samplePosition, meta.getMessage() });

            globalSample += blockSize;
        }

        return out;
    }

    juce::MidiBuffer singleEvent (const juce::MidiMessage& message)
    {
        juce::MidiBuffer midi;
        midi.addEvent (message, 0);
        return midi;
    }

    std::vector<Injected> at (int block, const juce::MidiMessage& message)
    {
        return { { block, message } };
    }

    std::vector<Delivered> onsOnly (const std::vector<Delivered>& events)
    {
        std::vector<Delivered> result;
        for (const auto& event : events)
            if (event.message.isNoteOn())
                result.push_back (event);
        return result;
    }

    std::vector<Delivered> offsOnly (const std::vector<Delivered>& events)
    {
        std::vector<Delivered> result;
        for (const auto& event : events)
            if (event.message.isNoteOff())
                result.push_back (event);
        return result;
    }

    /** Every parameter at a quiet, predictable start: no chaos, no fills, no swing, no
        humanise, the transport synced off, and all eight tracks on. */
    void setDeterministic (Processor& p)
    {
        setParameter (p.apvts, Processor::chaosId, 0.0f);
        setParameter (p.apvts, Processor::swingId, 0.0f);
        setParameter (p.apvts, Processor::humaniseTimeId, 0.0f);
        setParameter (p.apvts, Processor::humaniseVelocityId, 0.0f);
        setParameter (p.apvts, Processor::fillProbId, 0.0f);
        setParameter (p.apvts, Processor::polyRatioId, 0.0f);
        setParameter (p.apvts, Processor::syncId, 0.0f);
        setParameter (p.apvts, Processor::velCurveId, 0.0f);   // Flat

        for (int track = 0; track < Processor::numTracks; ++track)
            setParameter (p.apvts, Processor::trackOnIds[track], 1.0f);
    }

    /** The trigger note-on as an injection (the default trigger key is C2/36). */
    std::vector<Injected> triggerOn (int block = 0, int note = 36)
    {
        return { { block, juce::MidiMessage::noteOn (1, note, (std::uint8_t) 100) } };
    }

    std::vector<Injected> triggerOff (int block, int note = 36)
    {
        return { { block, juce::MidiMessage::noteOff (1, note) } };
    }

    std::vector<Delivered> runWithModulation (Processor::ModTarget target, const char* parameterId,
                                               float baseValue, float amount)
    {
        Processor processor;
        setDeterministic (processor);
        setParameter (processor.apvts, Processor::trackVelIds[0], 100.0f);
        setParameter (processor.apvts, Processor::trackGainIds[0], 0.0f);
        setParameter (processor.apvts, Processor::trackAccentIds[0], 0.0f);
        setParameter (processor.apvts, Processor::trackFlamIds[0], 0.0f);
        setParameter (processor.apvts, Processor::trackGhostIds[0], 0.0f);
        setParameter (processor.apvts, parameterId, baseValue);
        setParameter (processor.apvts, Processor::lfo1OffsetId, 100.0f);
        setParameter (processor.apvts, Processor::modTargetIds[0], (float) target);
        setParameter (processor.apvts, Processor::modAmountIds[0], amount);
        processor.prepareToPlay (sampleRate, blockSize);

        std::int64_t sample = 0;
        return run (processor, sample, 20, triggerOn());
    }

    int hitsOf (const std::vector<Delivered>& events, int note)
    {
        int count = 0;
        for (const auto& event : events)
            if (event.message.isNoteOn() && event.message.getNoteNumber() == note)
                ++count;
        return count;
    }

    /** No stuck notes: every delivered note-on has a matching note-off after it. */
    void assertNoStuckNotes (const std::vector<Delivered>& events, const char* tag = "")
    {
        std::vector<int> held;
        for (const auto& event : events)
        {
            if (event.message.isNoteOn())
                held.push_back (event.message.getNoteNumber());
            else if (event.message.isNoteOff())
            {
                const auto found = std::find (held.begin(), held.end(), event.message.getNoteNumber());
                assert (found != held.end());
                held.erase (found);
            }
        }
        if (! held.empty())
        {
            std::printf ("   stuck notes (%s):", tag);
            for (int n : held)
                std::printf (" %d", n);
            std::printf ("\n");
            std::fflush (stdout);
        }
        assert (held.empty());
    }
    /** The concatenation of two delivered streams (the groove + its flush tail). */
    std::vector<Delivered> concat (const std::vector<Delivered>& a, const std::vector<Delivered>& b)
    {
        std::vector<Delivered> out = a;
        out.insert (out.end(), b.begin(), b.end());
        return out;
    }

    //==============================================================================
    void testParameters()
    {
        Processor processor;

        checkParameterIds (processor, processor.apvts,
            { Processor::syncId, Processor::tempoId, Processor::triggerModeId,
              Processor::triggerNoteId, Processor::swingId, Processor::humaniseTimeId,
              Processor::humaniseVelocityId, Processor::velCurveId, Processor::polyRatioId,
              Processor::chaosId, Processor::evolveRateId, Processor::mutateScaleId,
              Processor::freezeId, Processor::fillProbId, Processor::fillStyleId,
              Processor::fillBarsId, Processor::seedId,
              Processor::lfo1RateId, Processor::lfo1ShapeId, Processor::lfo1SyncId,
              Processor::lfo1DivisionId, Processor::lfo1OffsetId, Processor::lfo1InvertId,
              Processor::lfo1RectifyId, Processor::lfo1QuantiseId, Processor::lfo1SmoothId,
              Processor::lfo1PulseWidthId, Processor::lfo1SkewId, Processor::lfo2RateId,
              Processor::lfo2ShapeId, Processor::lfo2SyncId, Processor::lfo2DivisionId,
              Processor::lfo2OffsetId, Processor::lfo2InvertId, Processor::lfo2RectifyId,
              Processor::lfo2QuantiseId, Processor::lfo2SmoothId, Processor::lfo2PulseWidthId,
              Processor::lfo2SkewId, Processor::modSourceIds[0], Processor::modTargetIds[0],
              Processor::modAmountIds[0], Processor::modDelayIds[0],
              Processor::modSourceIds[5], Processor::modTargetIds[5],
              Processor::modAmountIds[5], Processor::modDelayIds[5] },
            Processor::numControls);

        // Master ranges (plan 19 §1's table).
        // Plan 22 (owner finding 5): Host Sync ships on. A bool has no checkRange form, so
        // assert the parameter's default and the value the APVTS holds before any set.
        assert (processor.apvts.getParameter (Processor::syncId)->getDefaultValue() == 1.0f);
        assert (processor.apvts.getRawParameterValue (Processor::syncId)->load() > 0.5f);

        checkRange (processor.apvts, Processor::tempoId, 40.0f, 240.0f, 120.0f);
        checkRange (processor.apvts, Processor::triggerNoteId, 24.0f, 96.0f, 36.0f);
        checkRange (processor.apvts, Processor::swingId, -50.0f, 50.0f, 0.0f);
        checkRange (processor.apvts, Processor::humaniseTimeId, 0.0f, 20.0f, 2.0f);
        checkRange (processor.apvts, Processor::humaniseVelocityId, 0.0f, 100.0f, 10.0f);
        checkRange (processor.apvts, Processor::chaosId, 0.0f, 100.0f, 25.0f);
        checkRange (processor.apvts, Processor::mutateScaleId, 0.0f, 100.0f, 50.0f);
        checkRange (processor.apvts, Processor::fillProbId, 0.0f, 100.0f, 15.0f);
        checkRange (processor.apvts, Processor::seedId, 0.0f, 999.0f, 42.0f);

        // Track 1 spot checks (the defaults walk the GM map and stay musical), and every
        // track's IDs exist.
        checkRange (processor.apvts, Processor::trackStepsIds[0], 1.0f, 32.0f, 16.0f);
        checkRange (processor.apvts, Processor::trackPulsesIds[0], 0.0f, 32.0f, 4.0f);
        checkRange (processor.apvts, Processor::trackNoteIds[0], 24.0f, 96.0f, 36.0f);
        checkRange (processor.apvts, Processor::trackVelIds[0], 30.0f, 150.0f, 100.0f);
        checkRange (processor.apvts, Processor::trackAccentIds[0], 0.0f, 100.0f, 30.0f);
        checkRange (processor.apvts, Processor::trackGhostIds[0], 0.0f, 100.0f, 10.0f);
        checkRange (processor.apvts, Processor::trackGainIds[0], -24.0f, 12.0f, 0.0f);
        checkRange (processor.apvts, Processor::trackNoteIds[7], 24.0f, 96.0f, 75.0f);
        for (int track = 0; track < Processor::numTracks; ++track)
        {
            assert (processor.apvts.getParameter (Processor::trackOnIds[track]) != nullptr);
            assert (processor.apvts.getParameter (Processor::trackPatternIds[track]) != nullptr);
            assert (processor.apvts.getParameter (Processor::trackStepsIds[track]) != nullptr);
            assert (processor.apvts.getParameter (Processor::trackPulsesIds[track]) != nullptr);
            assert (processor.apvts.getParameter (Processor::trackRotationIds[track]) != nullptr);
            assert (processor.apvts.getParameter (Processor::trackDivisionIds[track]) != nullptr);
            assert (processor.apvts.getParameter (Processor::trackNoteIds[track]) != nullptr);
            assert (processor.apvts.getParameter (Processor::trackVelIds[track]) != nullptr);
            assert (processor.apvts.getParameter (Processor::trackAccentIds[track]) != nullptr);
            assert (processor.apvts.getParameter (Processor::trackFlamIds[track]) != nullptr);
            assert (processor.apvts.getParameter (Processor::trackGhostIds[track]) != nullptr);
            assert (processor.apvts.getParameter (Processor::trackGainIds[track]) != nullptr);
        }

        // The router's targets: the choice list matches the enum order the sessions save.
        auto* target = dynamic_cast<juce::AudioParameterChoice*> (
            processor.apvts.getParameter (Processor::modTargetIds[0]));
        assert (target != nullptr);
        assert (target->choices == Processor::modTargetNames());
        assert (target->choices.size() == (int) Processor::ModTarget::count);
        assert ((int) Processor::ModTarget::count == 47);
        assert (processor.getRouter().getNumTargets() == 46);

        pass ("testParameters");
    }

    void testModulationTargetsAffectOutput()
    {
        const auto velocity = runWithModulation (Processor::ModTarget::TrackVel1,
                                                  Processor::trackVelIds[0], 40.0f, 20.0f);
        bool sawModulatedVelocity = false;
        for (const auto& event : velocity)
            if (event.message.isNoteOn() && event.message.getNoteNumber() == 36)
            {
                assert (event.message.getVelocity() == 64);
                sawModulatedVelocity = true;
                break;
            }
        assert (sawModulatedVelocity);

        const auto level = runWithModulation (Processor::ModTarget::TrackLevel1,
                                               Processor::trackGainIds[0], -12.0f, 20.0f);
        int levelVelocity = -1;
        for (const auto& event : level)
            if (event.message.isNoteOn() && event.message.getNoteNumber() == 36)
            {
                levelVelocity = event.message.getVelocity();
                break;
            }
        assert (levelVelocity > 40 && levelVelocity < 100);

        const auto accent = runWithModulation (Processor::ModTarget::TrackAccent1,
                                                Processor::trackAccentIds[0], 0.0f, 100.0f);
        bool sawAccent = false;
        for (const auto& event : accent)
            if (event.message.isNoteOn() && event.message.getNoteNumber() == 36)
            {
                assert (event.message.getVelocity() == 127);
                sawAccent = true;
                break;
            }
        assert (sawAccent);

        const auto ghost = runWithModulation (Processor::ModTarget::TrackGhost1,
                                               Processor::trackGhostIds[0], 0.0f, 100.0f);
        bool sawGhost = false;
        for (const auto& event : ghost)
            if (event.message.isNoteOn() && event.message.getNoteNumber() == 36)
            {
                assert (event.message.getVelocity() < 100);
                sawGhost = true;
                break;
            }
        assert (sawGhost);

        const auto flam = runWithModulation (Processor::ModTarget::TrackFlam1,
                                              Processor::trackFlamIds[0], 0.0f, 100.0f);
        assert (hitsOf (flam, 36) >= 2);

        pass ("testModulationTargetsAffectOutput");
    }

    void testStateRoundTrip()
    {
        Processor processor;

        // Move a spread of parameters off their defaults, save, change, restore.
        setParameter (processor.apvts, Processor::tempoId, 140.0f);
        setParameter (processor.apvts, Processor::swingId, -20.0f);
        setParameter (processor.apvts, Processor::chaosId, 60.0f);
        setParameter (processor.apvts, Processor::trackStepsIds[0], 8.0f);
        setParameter (processor.apvts, Processor::trackPulsesIds[0], 3.0f);
        setParameter (processor.apvts, Processor::trackNoteIds[3], 47.0f);
        setParameter (processor.apvts, Processor::trackGainIds[1], -6.0f);

        juce::MemoryBlock state;
        processor.getStateInformation (state);
        assert (state.getSize() > 0);

        setParameter (processor.apvts, Processor::tempoId, 60.0f);
        setParameter (processor.apvts, Processor::trackNoteIds[3], 80.0f);

        processor.setStateInformation (state.getData(), (int) state.getSize());

        assert (std::abs (getParameter (processor.apvts, Processor::tempoId) - 140.0f) < 1.0e-3f);
        assert (std::abs (getParameter (processor.apvts, Processor::swingId) + 20.0f) < 1.0e-3f);
        assert (std::abs (getParameter (processor.apvts, Processor::chaosId) - 60.0f) < 1.0e-3f);
        assert (std::abs (getParameter (processor.apvts, Processor::trackStepsIds[0]) - 8.0f) < 1.0e-3f);
        assert (std::abs (getParameter (processor.apvts, Processor::trackPulsesIds[0]) - 3.0f) < 1.0e-3f);
        assert (std::abs (getParameter (processor.apvts, Processor::trackNoteIds[3]) - 47.0f) < 1.0e-3f);
        assert (std::abs (getParameter (processor.apvts, Processor::trackGainIds[1]) + 6.0f) < 1.0e-3f);

        // A newer state is refused and leaves the current state unchanged.
        juce::MemoryBlock newer (state.getData(), state.getSize());
        {   // rewrite the suiteStateVersion attribute
            auto tree = juce::ValueTree::fromXml (juce::String::createStringFromData (newer.getData(), (int) newer.getSize()));
            tree.setProperty ("suiteStateVersion", 99, nullptr);
            const auto xml = tree.toXmlString();
            newer.setSize (xml.getNumBytesAsUTF8() + 1);
            std::memcpy (newer.getData(), xml.toRawUTF8(), (size_t) xml.getNumBytesAsUTF8() + 1);
        }
        processor.setStateInformation (newer.getData(), (int) newer.getSize());
        assert (std::abs (getParameter (processor.apvts, Processor::tempoId) - 140.0f) < 1.0e-3f);

        pass ("testStateRoundTrip");
    }

    void testLatencyFixed()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        const int expected = (int) std::llround (50.0 * sampleRate / 1000.0);
        assert (processor.getLatencySamples() == expected);

        // The knobs never move it (the MTC law).
        setParameter (processor.apvts, Processor::humaniseTimeId, 20.0f);
        setParameter (processor.apvts, Processor::tempoId, 240.0f);
        setParameter (processor.apvts, Processor::trackStepsIds[0], 1.0f);
        assert (processor.getLatencySamples() == expected);

        pass ("testLatencyFixed");
    }

    void testDefaultSilence()
    {
        Processor processor;
        setDeterministic (processor);
        processor.prepareToPlay (sampleRate, blockSize);

        std::int64_t sample = 0;
        // The transport plays, but no trigger note has arrived: silence.
        FakePlayHead playhead;
        playhead.playing = true;
        processor.setPlayHead (&playhead);

        const auto events = run (processor, sample, 40);
        assert (onsOnly (events).empty());

        processor.setPlayHead (nullptr);
        pass ("testDefaultSilence");
    }

    void testLatchRunsDeterministicGroove()
    {
        Processor processor;
        setDeterministic (processor);
        processor.prepareToPlay (sampleRate, blockSize);

        std::int64_t sample = 0;
        // The trigger starts the groove; the INIT defaults are the musical GM groove.
        const auto events = run (processor, sample, 200, triggerOn());

        // The latch runs forever with sync off, so the flush comes from an ending path:
        // the trigger-mode change (plan 19) stops the groove, then the tail delivers every
        // still-ringing note-off.
        setParameter (processor.apvts, Processor::triggerModeId, 1.0f);
        const auto tail = run (processor, sample, 16);
        const auto all = concat (events, tail);

        // The kick fires on four-on-the-floor: E(4,16) rot 0 = steps 0, 4, 8, 12 at 1/16,
        // 120 BPM → one beat apart = 24000 samples.
        const auto kickOns = onsOnly (events);
        assert (hitsOf (kickOns, 36) >= 3);

        std::vector<std::int64_t> kickTimes;
        for (const auto& event : kickOns)
            if (event.message.getNoteNumber() == 36)
                kickTimes.push_back (event.sample);

        for (size_t k = 1; k < kickTimes.size(); ++k)
            assert (kickTimes[k] - kickTimes[k - 1] == 24000);

    // The snare fires its backbeat (E(2,16) rot 4 = steps 4, 12), interleaved.
        assert (hitsOf (kickOns, 38) >= 2);

        // Every note number is from the GM defaults.
        for (const auto& event : onsOnly (all))
        {
            const int note = event.message.getNoteNumber();
            bool known = false;
            for (int track = 0; track < Processor::numTracks; ++track)
                if (note == Processor::defaultTrackNotes[track])
                    known = true;
            assert (known);
        }

        assertNoStuckNotes (all);
        pass ("testLatchRunsDeterministicGroove");
    }

    void testGrooveIsReproducible()
    {
        // Two fresh processors with the same seed produce identical streams.
        Processor a;
        setDeterministic (a);
        a.prepareToPlay (sampleRate, blockSize);
        std::int64_t sample = 0;
        const auto eventsA = run (a, sample, 216, triggerOn());

        Processor b;
        setDeterministic (b);
        b.prepareToPlay (sampleRate, blockSize);
        sample = 0;
        const auto eventsB = run (b, sample, 216, triggerOn());

        assert (eventsA.size() == eventsB.size());
        for (size_t i = 0; i < eventsA.size(); ++i)
        {
            assert (eventsA[i].sample == eventsB[i].sample);
            assert (eventsA[i].message.getNoteNumber() == eventsB[i].message.getNoteNumber());
            assert (eventsA[i].message.getVelocity() == eventsB[i].message.getVelocity());
        }

        // Both grooves are still latched: stop them (the mode change) and flush, then the
        // no-stuck rule holds on the combined streams.
        setParameter (a.apvts, Processor::triggerModeId, 1.0f);
        setParameter (b.apvts, Processor::triggerModeId, 1.0f);
        const auto tailA = run (a, sample, 16);
        const auto tailB = run (b, sample, 16);
        assertNoStuckNotes (concat (eventsA, tailA), "repro-a");
        assertNoStuckNotes (concat (eventsB, tailB), "repro-b");

        pass ("testGrooveIsReproducible");
    }

    void testGateStopsOnRelease()
    {
        Processor processor;
        setDeterministic (processor);
        setParameter (processor.apvts, Processor::triggerModeId, 1.0f);   // Gate
        processor.prepareToPlay (sampleRate, blockSize);

        std::int64_t sample = 0;
        // The trigger on in block 0, the release in block 6: the groove stops at the off.
        // The tail flushes every still-ringing note-off.
        std::vector<Injected> input = triggerOn();
        auto release = triggerOff (6);
        input.insert (input.end(), release.begin(), release.end());

        const auto events = run (processor, sample, 216, input);
        assert (! onsOnly (events).empty());
        assertNoStuckNotes (events, "run");

        // After the release, no new note-ons arrive: the last on is before the off plus one step.
        const std::int64_t releaseSample = 6 * blockSize;
        for (const auto& event : onsOnly (events))
            assert (event.sample < releaseSample + 24000 + 512);

        pass ("testGateStopsOnRelease");
    }

    void testNoteOffGuaranteePaths()
    {
        // The plan-19 list: transport stop, trigger release, mode change, preset load /
        // setStateInformation, prepareToPlay, releaseResources. One block set per path.
        {
            // Transport stop: sync stays on here, so the playhead governs the transport.
            Processor p;
            p.prepareToPlay (sampleRate, blockSize);
            FakePlayHead playhead;
            playhead.playing = true;
            p.setPlayHead (&playhead);
            std::int64_t sample = 0;
            const auto events = run (p, sample, 30, triggerOn(), &playhead);
            assert (! onsOnly (events).empty());
            playhead.playing = false;   // the transport stops
            const auto after = run (p, sample, 16);
            assert (onsOnly (after).empty());
            assertNoStuckNotes (concat (events, after), "mode-change");
            p.setPlayHead (nullptr);
        }

        {
            // Trigger release (Gate).
            Processor p;
            setDeterministic (p);
            setParameter (p.apvts, Processor::triggerModeId, 1.0f);
            p.prepareToPlay (sampleRate, blockSize);
            std::int64_t sample = 0;
            std::vector<Injected> input = triggerOn();
            auto release = triggerOff (4);
            input.insert (input.end(), release.begin(), release.end());
            const auto events = run (p, sample, 40, input);
            assertNoStuckNotes (events, "run");
        }

        {
            // The trigger-mode change stops the groove and flushes the sounding notes.
            Processor p;
            setDeterministic (p);
            p.prepareToPlay (sampleRate, blockSize);
            std::int64_t sample = 0;
            const auto events = run (p, sample, 20, triggerOn());
            setParameter (p.apvts, Processor::triggerModeId, 1.0f);
            const auto after = run (p, sample, 6);
            assert (onsOnly (after).empty());
            assertNoStuckNotes (concat (events, after), "mode-change");
        }

        {
            // setStateInformation (the state-load path): the panic stops the sounding
            // notes and the latched trigger restarts the groove with the loaded state.
            // A latched groove runs forever, so any fixed tail is too short for the last
            // note-off — stop it through the mode-change ending path first, then the tail
            // delivers every remaining note-off.
            Processor p;
            setDeterministic (p);
            p.prepareToPlay (sampleRate, blockSize);
            std::int64_t sample = 0;
            const auto events = run (p, sample, 20, triggerOn());

            juce::MemoryBlock state;
            p.getStateInformation (state);
            p.setStateInformation (state.getData(), (int) state.getSize());
            setParameter (p.apvts, Processor::triggerModeId, 1.0f);
            const auto after = run (p, sample, 16);

            assert (onsOnly (after).empty());
            assertNoStuckNotes (concat (events, after), "state-load");
        }

        {
            // prepareToPlay (the reset) and releaseResources cancel everything.
            Processor p;
            setDeterministic (p);
            p.prepareToPlay (sampleRate, blockSize);
            std::int64_t sample = 0;
            const auto events = run (p, sample, 20, triggerOn());

            p.releaseResources();
            const auto after = run (p, sample, 6);
            assert (onsOnly (after).empty());

            p.prepareToPlay (sampleRate, blockSize);
            const auto after2 = run (p, sample, 6);
            assert (onsOnly (after2).empty());
        }

        pass ("testNoteOffGuaranteePaths");
    }

    void testSwingMovesOddSteps()
    {
        // The snare lane at E(7,16) fires both odd and even steps; swing 50 % moves the odd
        // pattern indices late by exactly half a step (3000 samples at 120 BPM, 1/16).
        std::vector<std::int64_t> straight, swung;
        for (int pass = 0; pass < 2; ++pass)
        {
            Processor p;
            setDeterministic (p);
            setParameter (p.apvts, Processor::trackPulsesIds[1], 7.0f);
            if (pass == 1)
                setParameter (p.apvts, Processor::swingId, 50.0f);
            p.prepareToPlay (sampleRate, blockSize);

            std::int64_t sample = 0;
            const auto events = run (p, sample, 160, triggerOn());
            for (const auto& event : onsOnly (events))
                if (event.message.getNoteNumber() == 38)
                    (pass == 0 ? straight : swung).push_back (event.sample);
        }

        assert (straight.size() == swung.size() && ! straight.empty());
        for (size_t k = 0; k < straight.size(); ++k)
        {
            const std::int64_t shift = swung[k] - straight[k];
            // A 1/16 step is 0.25 beats = 6000 samples at 120 BPM; swing 50 % moves the odd
            // steps half a step late = 3000 samples.
            assert (shift == 0 || shift == 3000);
        }

        pass ("testSwingMovesOddSteps");
    }

    void testPassThrough()
    {
        Processor processor;
        setDeterministic (processor);
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();
        addSine (buffer, 0, sampleRate, 1000.0, 0.5f, 0);
        addSine (buffer, 1, sampleRate, 1000.0, 0.5f, 0);
        const auto* beforeL = buffer.getReadPointer (0);
        const auto* beforeR = buffer.getReadPointer (1);

        // Input MIDI: an unrelated note, which must come back out with the latency. The
        // queued time (stream + 100 + latency) is past this block, so the flush runs more
        // blocks until it is delivered.
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 64, (std::uint8_t) 90), 100);
        processor.processBlock (buffer, midi);

        for (int n = 0; n < blockSize; ++n)
        {
            assert (buffer.getSample (0, n) == beforeL[n]);
            assert (buffer.getSample (1, n) == beforeR[n]);
        }

        bool passed = false;
        const int latency = (int) std::llround (50.0 * sampleRate / 1000.0);
        juce::AudioBuffer<float> flush (2, blockSize);
        for (int block = 0; block < 8 && ! passed; ++block)
        {
            flush.clear();
            juce::MidiBuffer empty;
            processor.processBlock (flush, empty);
            for (const auto meta : empty)
            {
                if (meta.getMessage().isNoteOn() && meta.getMessage().getNoteNumber() == 64)
                {
                    assert (meta.samplePosition == (latency + 100) % blockSize);
                    passed = true;
                }
            }
        }
        assert (passed);

        pass ("testPassThrough");
    }

    void testPresetSeam()
    {
        Processor processor;

        assert (processor.getPresetPluginId() == "GrooveMutator");
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

    void testEditorPages()
    {
        Processor processor;
        setDeterministic (processor);
        processor.prepareToPlay (sampleRate, blockSize);

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        assert (editor != nullptr);
        auto& gm = *dynamic_cast<GrooveMutatorEditor*> (editor.get());
        assert (&gm != nullptr);

        SuiteUI::LabelledDropdown* noteDropdown = nullptr;
        int noteDropdownCount = 0;
        for (int child = 0; child < gm.getNumChildComponents(); ++child)
        {
            auto* component = gm.getChildComponent (child);
            if (auto* dropdown = dynamic_cast<SuiteUI::LabelledDropdown*> (component))
            {
                if (component->getComponentID().startsWith ("track-note-selector-"))
                {
                    ++noteDropdownCount;
                    if (component->getComponentID() == "track-note-selector-1")
                        noteDropdown = dropdown;
                }
            }
        }
        assert (noteDropdownCount == Processor::numTracks);
        assert (noteDropdown != nullptr);
        noteDropdown->box.setSelectedId (38 - 24 + 1, juce::sendNotificationSync);
        assert (juce::roundToInt (processor.apvts.getRawParameterValue (
                   Processor::trackNoteIds[0])->load()) == 38);
        setParameter (processor.apvts, Processor::trackNoteIds[0], 36.0f);

        // Populate the playhead, trail and LFO before capturing deterministic R9 renders.
        std::int64_t sample = 0;
        const auto renderedEvents = run (processor, sample, 40, triggerOn());
        assert (std::any_of (renderedEvents.begin(), renderedEvents.end(),
                             [] (const Delivered& event) { return event.message.isNoteOn(); }));
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

        // Every page paints content of its own, and each page renders identically twice
        // without processing another block between the two captures.
        juce::Image previous;
        for (int page = 0; page < 4; ++page)
        {
            gm.setPage (page);
            for (int track = 0; track < Processor::numTracks; ++track)
                assert (gm.isNoteDropdownVisible (track) == (page == 0));
            const auto first = paintComponent (gm);
            const auto second = paintComponent (gm);
            assert (countChangedPixels (const_cast<juce::Image&> (first),
                                        const_cast<juce::Image&> (second), 1) == 0);

            if (page > 0)
                assert (countChangedPixels (const_cast<juce::Image&> (first),
                                            const_cast<juce::Image&> (previous), 2) > 0);
            previous = first;
        }

        // Switching pages changes no parameter (the SD/DS rule).
        std::vector<float> before, after;
        for (const auto* parameter : processor.getParameters())
        {
            const auto* ranged = dynamic_cast<const juce::RangedAudioParameter*> (parameter);
            assert (ranged != nullptr);
            before.push_back (ranged->getValue());
        }

        gm.setPage (3);
        gm.setPage (0);
        gm.setPage (3);
        gm.setPage (0);

        for (size_t i = 0; i < before.size(); ++i)
        {
            const auto* ranged = dynamic_cast<const juce::RangedAudioParameter*> (processor.getParameters()[(int) i]);
            after.push_back (ranged->getValue());
            assert (after.back() == before[i]);
        }

        // The render test (R9): every page's screenshot when SUITE_RENDER_DIR is set.
        const auto folder = juce::SystemStats::getEnvironmentVariable ("SUITE_RENDER_DIR", {});
        if (! folder.isEmpty())
        {
            static constexpr const char* pageFiles[4] = { "groovemutator-grid.png", "groovemutator-lanes.png",
                                                          "groovemutator-mutate.png", "groovemutator-mod.png" };
            for (int page = 0; page < 4; ++page)
            {
                gm.setPage (page);
                assert (writeDeterministicPng (gm, juce::File (folder).getChildFile (pageFiles[page])));
            }
        }
        else
        {
            std::printf ("[SKIP] editor renders: set SUITE_RENDER_DIR to write them\n");
        }

        processor.editorBeingDeleted (editor.get());
        pass ("testEditorPages");
    }

    void testAffinity()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        assert (editor != nullptr);
        auto& gm = *dynamic_cast<GrooveMutatorEditor*> (editor.get());

        // The lane affinity: Steps dims Pulses and Rotation; Hash dims Rotation only;
        // Euclid dims nothing (decision G1, one test for the editor).
        const std::pair<int, std::pair<bool, bool>> cases[3] = {
            { 0, { true, true } },    // Euclid: both live
            { 1, { true, false } },   // Hash: rotation dimmed (the maths ignores it)
            { 2, { false, false } },  // Steps: both dimmed
        };
        for (const auto& [kind, expect] : cases)
        {
            setParameter (processor.apvts, Processor::trackPatternIds[0], (float) kind);
            gm.setPage (1);
            gm.setPage (0);   // the timer is not pumping here: re-evaluate through the page
            assert (gm.isPulsesEnabled (0) == expect.first);
            assert (gm.isRotationEnabled (0) == expect.second);
        }

        // Tempo dims while Host Sync is on, live again when sync goes off.
        setParameter (processor.apvts, Processor::syncId, 1.0f);
        gm.setPage (2);
        assert (! gm.isTempoEnabled());
        setParameter (processor.apvts, Processor::syncId, 0.0f);
        gm.setPage (2);
        assert (gm.isTempoEnabled());

        processor.editorBeingDeleted (editor.get());
        pass ("testAffinity");
    }

    void testStepsPaint()
    {
        Processor processor;
        setDeterministic (processor);
        processor.prepareToPlay (sampleRate, blockSize);

        // Lane 1 becomes a Steps lane; the test paints two steps through the processor's
        // public API (what the LaneView's draw mode calls).
        setParameter (processor.apvts, Processor::trackPatternIds[1], 2.0f);
        setParameter (processor.apvts, Processor::trackNoteIds[1], 38.0f);
        processor.setLaneStep (1, 2, true);
        processor.setLaneStep (1, 10, true);
        assert (processor.getLaneStepsMask (1) == ((1u << 2) | (1u << 10)));

        std::int64_t sample = 0;
        const auto events = run (processor, sample, 160, triggerOn());

        std::vector<std::int64_t> times;
        for (const auto& event : events)
            if (event.message.isNoteOn() && event.message.getNoteNumber() == 38)
                times.push_back (event.sample);
        assert (times.size() >= 2);

        // Only the painted steps fire: with the grid aligned to beat 0, step 2 lands at
        // beat 0.5 and step 10 at beat 2.5 (a 1/16 step is 6000 samples at 120 BPM; the
        // full 16-step cycle is 96000). Fold the fixed 2400-sample latency in.
        for (const auto t : times)
        {
            const std::int64_t scheduled = t - 2400;
            const std::int64_t phase = scheduled % 96000;
            assert (phase == 12000 || phase == 60000);
        }

        // The masks survive a state round trip (the stepsMasks property, the lerpCurve pattern).
        juce::MemoryBlock state;
        processor.getStateInformation (state);
        processor.setLaneStep (1, 2, false);
        assert (processor.getLaneStepsMask (1) == (1u << 10));
        processor.setStateInformation (state.getData(), (int) state.getSize());
        assert (processor.getLaneStepsMask (1) == ((1u << 2) | (1u << 10)));

        pass ("testStepsPaint");
    }

    void testStepsLanesDoNotMutate()
    {
        // Plan 21, C3: Steps lanes are the authoring mode — what you painted is what
        // plays. Full chaos at the fastest evolve rate must not move a painted pattern,
        // while the generative kinds keep mutating (that is the plan-19 behaviour).
        Processor processor;
        setDeterministic (processor);                       // fills off, sync off
        setParameter (processor.apvts, Processor::chaosId, 100.0f);
        setParameter (processor.apvts, Processor::mutateScaleId, 100.0f);
        setParameter (processor.apvts, Processor::evolveRateId, 0.0f);   // per Beat
        setParameter (processor.apvts, Processor::trackPatternIds[2], 2.0f);
        processor.prepareToPlay (sampleRate, blockSize);

        processor.setLaneStep (2, 0, true);
        processor.setLaneStep (2, 5, true);
        processor.setLaneStep (2, 10, true);

        std::int64_t sample = 0;
        const auto events = run (processor, sample, 220, triggerOn());

        // A 16-step 1/16 cycle at 120 BPM is 96000 samples; steps 0, 5 and 10 sit at
        // 0, 30000 and 60000 (the 2400-sample latency folds out). Every hit must land on
        // a painted step, and all three must fire.
        std::vector<int> phases;
        for (const auto& event : events)
        {
            if (! event.message.isNoteOn() || event.message.getNoteNumber() != 42)
                continue;
            const std::int64_t phase = (event.sample - 2400) % 96000;
            phases.push_back ((int) phase);
            assert (phase == 0 || phase == 30000 || phase == 60000);
        }
        assert (phases.size() >= 3);
        pass ("testStepsLanesDoNotMutate");
    }

    void testFillsApplyToStepsLanes()
    {
        // Plan 21, C3: fills stay live on Steps lanes (they only add on the tail, they
        // never remove), and the Fill Length choice means what it says — "1 Bar" is a
        // one-bar window, not "no window" (the index-to-bars bug this test pins).
        Processor processor;
        setDeterministic (processor);
        setParameter (processor.apvts, Processor::trackPatternIds[2], 2.0f);
        setParameter (processor.apvts, Processor::fillProbId, 100.0f);
        setParameter (processor.apvts, Processor::fillBarsId, 0.0f);   // "1 Bar"
        processor.prepareToPlay (sampleRate, blockSize);

        processor.setLaneStep (2, 0, true);

        std::int64_t sample = 0;
        const auto events = run (processor, sample, 220, triggerOn());

        // A 16-step 1/16 cycle is one bar, so a 1-bar fill window covers the whole
        // cycle: Tom Roll (every other step) adds the unpainted even steps 2..14 at
        // probability 1. No odd step may fire, and every even step must.
        std::vector<bool> evenFired (8, false);
        for (const auto& event : events)
        {
            if (! event.message.isNoteOn() || event.message.getNoteNumber() != 42)
                continue;
            const std::int64_t phase = (event.sample - 2400) % 96000;
            assert (phase % 12000 == 0);                    // even steps only (0, 12000, ...)
            evenFired[(size_t) (phase / 12000)] = true;
        }
        for (int step = 0; step < 8; ++step)
            assert (evenFired[(size_t) step]);

        // The view's truth: the processor exposes exactly the even-step additions.
        const std::uint32_t expected = (1u << 2) | (1u << 4) | (1u << 6) | (1u << 8)
                                     | (1u << 10) | (1u << 12) | (1u << 14);
        assert (processor.laneFillMask (2) == expected);

        pass ("testFillsApplyToStepsLanes");
    }

    void testTrailPublished()
    {
        Processor processor;
        setDeterministic (processor);
        processor.prepareToPlay (sampleRate, blockSize);

        // Nothing ran: the trail is flat at its initial state.
        const auto idle = processor.getTrail();

        std::int64_t sample = 0;
        run (processor, sample, 40, triggerOn());

        // The attractor followed the musical time: the published points move.
        const auto moved = processor.getTrail();
        float minX = 2.0f, maxX = -2.0f;
        for (const auto& point : moved)
        {
            minX = juce::jmin (minX, point.x);
            maxX = juce::jmax (maxX, point.x);
        }
        assert (maxX > minX);

        // Idle values stay in [-1, 1].
        for (const auto& point : idle)
        {
            assert (point.x >= -1.0f && point.x <= 1.0f);
            assert (point.y >= -1.0f && point.y <= 1.0f);
        }

        pass ("testTrailPublished");
    }

    void testHostSyncAlignment()
    {
        // Host Sync on (the default): the groove's grid follows the host's PPQ. The host
        // starts off-grid (9.3 quarters in), so the first scheduled step is the next
        // quarter-beat boundary — every kick lands on an absolute quarter-beat of the
        // host timeline, whatever the start position was.
        Processor processor;
        setDeterministic (processor);
        setParameter (processor.apvts, Processor::syncId, 1.0f);   // the host drives the clock
        processor.prepareToPlay (sampleRate, blockSize);

        FakePlayHead playhead;
        playhead.playing = true;
        playhead.bpm = 120.0;
        playhead.ppq = 9.3;
        processor.setPlayHead (&playhead);

        std::int64_t sample = 0;
        // Two beats of host time, and NO trigger note: since plan 21 the synced groove
        // runs from the transport alone. The kick fires at beats 10.0 and 11.0 (its
        // pattern index starts mid-cycle because the host starts mid-bar).
        const auto events = run (processor, sample, 100, {}, &playhead);

        // One quarter-beat at 120 BPM / 48 kHz is 6000 samples; the fixed latency folds out.
        const std::int64_t quarterBeat = 6000;
        const auto phaseOffset = (std::int64_t) std::llround (9.3 * 24000.0);
        int kickCount = 0;
        for (const auto& event : events)
        {
            if (! event.message.isNoteOn() || event.message.getNoteNumber() != 36)
                continue;
            ++kickCount;
            const std::int64_t scheduled = event.sample - 2400 + phaseOffset;
            assert (scheduled % quarterBeat == 0);
        }
        assert (kickCount >= 2);

        // A host relocate: the grid re-aligns to the new position (the MTC jump rule).
        playhead.ppq += 37.0;   // a jump the scheduler cannot mistake for a step
        const auto after = run (processor, sample, 40, {}, &playhead);

        for (const auto& event : after)
        {
            if (! event.message.isNoteOn() || event.message.getNoteNumber() != 36)
                continue;
            const std::int64_t scheduled = event.sample - 2400
                                         + (std::int64_t) std::llround ((9.3 + 37.0) * 24000.0);
            assert (scheduled % quarterBeat == 0);
        }

        playhead.playing = false;
        const auto stop = run (processor, sample, 8);   // the transport stop flushes
        assertNoStuckNotes (concat (events, concat (after, stop)), "host-sync stop");

        // While synced, the trigger key is not a run source: a stopped transport stays
        // silent even with the trigger key held (plan 21).
        const auto quiet = run (processor, sample, 8, triggerOn());
        assert (onsOnly (quiet).empty());

        processor.setPlayHead (nullptr);

        pass ("testHostSyncAlignment");
    }

    void testHeaderContract()
    {
        Processor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        assert (editor != nullptr);

        SuitePresets::PresetBar* bar = nullptr;
        SuiteUI::HeaderMenuButton* menu = nullptr;

        for (int i = 0; i < editor->getNumChildComponents(); ++i)
        {
            if (auto* candidate = dynamic_cast<SuitePresets::PresetBar*> (editor->getChildComponent (i)))
                bar = candidate;
            if (auto* candidate = dynamic_cast<SuiteUI::HeaderMenuButton*> (editor->getChildComponent (i)))
                menu = candidate;
        }

        assert (bar != nullptr);
        assert (menu != nullptr);

        // The menu's two callbacks are wired (the dialogs themselves are async, plan 08).
        assert (menu->onAbout != nullptr);
        assert (menu->onSettings != nullptr);

        processor.editorBeingDeleted (editor.get());
        pass ("testHeaderContract");
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::puts ("Running Groove Mutator plugin tests...");
    std::fflush (stdout);

    testParameters();
    testModulationTargetsAffectOutput();
    testStateRoundTrip();
    testLatencyFixed();
    testDefaultSilence();
    testLatchRunsDeterministicGroove();
    testGrooveIsReproducible();
    testGateStopsOnRelease();
    testNoteOffGuaranteePaths();
    testSwingMovesOddSteps();
    testPassThrough();
    testPresetSeam();
    testEditorPages();
    testAffinity();
    testStepsPaint();
    testStepsLanesDoNotMutate();
    testFillsApplyToStepsLanes();
    testTrailPublished();
    testHostSyncAlignment();
    testHeaderContract();

    std::puts ("All Groove Mutator plugin tests passed.");
    return 0;
}
