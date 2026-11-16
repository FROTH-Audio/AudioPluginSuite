// Pro preset tests for Micro-Timing Chaos Sequencer. Compiles the real processor and editor
// sources, then checks the factory bank, the Lerp curve and MIDI output.

#include "PluginProcessor.h"
#include "PluginTestUtils.h"
#include "PresetTestUtils.h"

#include <map>

namespace
{
    using namespace PresetTest;
    using Processor = MicroTimingChaosProcessor;

    // A host playhead the test controls, so the transport can be stopped cleanly at the end.
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

    // A preset with a non-default Lerp curve must reach the curve atomics on load.
    void testMtcCurveRestored()
    {
        Processor processor;
        const auto presets = presetsFor ("MicroTimingChaos");

        const SuitePresets::PresetInfo* storm = nullptr;

        for (const auto& preset : presets)
            if (preset.name == "Lorenz Storm")
                storm = &preset;

        assert (storm != nullptr);

        const auto before = processor.getLerpCurve();
        processor.applyPresetState (storm->state);
        const auto after = processor.getLerpCurve();

        assert (std::abs (after[1].y - 0.2f) < 1.0e-4f);
        assert (std::abs (after[3].y - 0.8f) < 1.0e-4f);
        assert (std::abs (after[1].y - before[1].y) > 1.0e-3f);

        pass ("mtcCurveRestored");
    }

    // Four bars with the internal clock, then a clean stop: every note-on must get a note-off.
    void testMidiSanity()
    {
        const auto presets = presetsFor ("MicroTimingChaos");
        assert (! presets.empty());

        juce::AudioBuffer<float> audio (2, blockSize);

        for (const auto& preset : presets)
        {
            Processor processor;
            processor.applyPresetState (preset.state);

            // Host sync is off in every factory preset (the bank audit enforces it). Turn it on here
            // so the test can stop the transport and flush every sounding note.
            PluginTest::setParameter (processor.apvts, Processor::syncId, 1.0f);
            PluginTest::setParameter (processor.apvts, Processor::bpmId, 120.0f);
            processor.prepareToPlay (sampleRate, blockSize);

            FakePlayHead playhead;
            playhead.playing = true;
            processor.setPlayHead (&playhead);

            std::map<int, int> sounding;
            int notesOn = 0;
            int notesOff = 0;

            auto runBlock = [&] (bool playing)
            {
                playhead.playing = playing;

                juce::MidiBuffer midi;
                audio.clear();
                processor.processBlock (audio, midi);

                assert (allFinite (audio));

                for (const auto meta : midi)
                {
                    const auto message = meta.getMessage();

                    assert (meta.samplePosition >= 0);
                    assert (meta.samplePosition < blockSize);

                    if (message.isNoteOn())
                    {
                        ++sounding[message.getNoteNumber()];
                        ++notesOn;
                    }
                    else if (message.isNoteOff())
                    {
                        const auto found = sounding.find (message.getNoteNumber());
                        assert (found != sounding.end());
                        assert (found->second > 0);

                        if (--found->second == 0)
                            sounding.erase (found);

                        ++notesOff;
                    }
                }
            };

            // Four bars at 120 BPM (16 beats, 8 s).
            const int blocks = (int) (8.0 * sampleRate / blockSize);

            for (int block = 0; block < blocks; ++block)
            {
                runBlock (true);
                playhead.ppq += (double) blockSize / sampleRate * (playhead.bpm / 60.0);
            }

            // Stop the transport and give the queued note-offs a block or two to flush.
            for (int block = 0; block < 100; ++block)
                runBlock (false);

            processor.setPlayHead (nullptr);

            assert (notesOn > 0);
            assert (notesOn == notesOff);
            assert (sounding.empty());
            assert (processor.getLatencySamples()
                    == (int) std::lround (50.0 * sampleRate / 1000.0));
        }

        pass ("midiSanity");
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::puts ("Running Micro-Timing Chaos Pro preset tests...");

    using namespace PresetTest;

    runBankShape<Processor> ("MicroTimingChaos");

    {
        Processor processor;
        runBankRanges (processor);
        runBankNotDefault (processor);
        runApplyMatchesFile (processor);
        runApplyResetsMissing (processor);
        runApplyTwiceIdempotent (processor);
    }

    {
        Processor processor;
        runEditorWithBar (processor);
    }

    // Plan 09 1.4: the bar's preset selection survives the editor being recreated.
    {
        Processor processor;
        PresetTest::runSelectionSurvivesReopen (processor);
    }

#if SUITE_PRESET_BANK_AS_PROGRAMS
    {
        Processor processor;
        PresetTest::runProgramList (processor);
    }
#endif

    runMenuModel ("MicroTimingChaos");

    testMtcCurveRestored();
    testMidiSanity();

    std::puts ("All Micro-Timing Chaos Pro preset tests passed.");
    return 0;
}
