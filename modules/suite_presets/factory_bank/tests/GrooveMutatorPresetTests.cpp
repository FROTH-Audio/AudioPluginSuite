// Pro preset tests for Groove Mutator. Compiles the real processor and editor sources,
// then checks the factory bank against the live APVTS.

#include "PluginProcessor.h"
#include "PresetTestUtils.h"

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::puts ("Running Groove Mutator Pro preset tests...");

    // Plan 19 Phase 5: 19 factory presets for the mutated groove engine.
    PresetTest::runBankShape<GrooveMutatorProcessor> ("GrooveMutator", 15);

    {
        GrooveMutatorProcessor processor;
        PresetTest::runBankRanges (processor);
        PresetTest::runBankNotDefault (processor);
        PresetTest::runApplyMatchesFile (processor);
        PresetTest::runApplyResetsMissing (processor);
        PresetTest::runApplyTwiceIdempotent (processor);
        PresetTest::runAudioSanity (processor);

        // The audit's GrooveMutator rules, checked against the bank itself: every preset
        // runs on the internal clock (host sync off, authored) and the trigger key reads
        // back at 36 after the apply (resetMissing restores the default the audit assumes).
        for (const auto& preset : PresetTest::presetsFor ("GrooveMutator"))
        {
            const auto sync = PresetTest::treeValue (preset.state, GrooveMutatorProcessor::syncId);
            assert (sync == 0.0f);

            processor.applyPresetState (preset.state);
            const auto triggerNote = processor.apvts.getRawParameterValue (
                GrooveMutatorProcessor::triggerNoteId)->load();
            assert (juce::roundToInt (triggerNote) == 36);
        }

        // Playable from one held note: apply each preset, press the trigger key and run
        // eight seconds — the groove delivers notes (the plan-19 playability rule).
        {
            constexpr double sampleRate = 48000.0;
            constexpr int blockSize = 512;
            juce::AudioBuffer<float> buffer (2, blockSize);
            std::int64_t sample = 0;

            for (const auto& preset : PresetTest::presetsFor ("GrooveMutator"))
            {
                processor.applyPresetState (preset.state);
                processor.prepareToPlay (sampleRate, blockSize);
                processor.reset();

                juce::MidiBuffer midi;
                midi.addEvent (juce::MidiMessage::noteOn (1, 36, (std::uint8_t) 100), 0);

                int noteOns = 0;
                const int blocks = (int) (8.0 * sampleRate / blockSize);

                for (int block = 0; block < blocks; ++block)
                {
                    buffer.clear();
                    processor.processBlock (buffer, midi);

                    for (const auto metadata : midi)
                        if (metadata.getMessage().isNoteOn())
                            ++noteOns;

                    sample += blockSize;
                }

                if (noteOns == 0)
                    std::printf ("   preset delivered no notes: %s\n",
                                 preset.name.toRawUTF8());
                assert (noteOns > 0);
            }

            processor.releaseResources();
            PresetTest::pass ("playableFromOneHeldNote");
        }
    }

    {
        GrooveMutatorProcessor processor;
        PresetTest::runEditorWithBar (processor);
    }

    // Plan 09 1.4: the bar's preset selection survives the editor being recreated.
    {
        GrooveMutatorProcessor processor;
        PresetTest::runSelectionSurvivesReopen (processor);
    }

#if SUITE_PRESET_BANK_AS_PROGRAMS
    {
        GrooveMutatorProcessor processor;
        PresetTest::runProgramList (processor);
    }
#endif

    PresetTest::runMenuModel ("GrooveMutator");

    std::puts ("All Groove Mutator Pro preset tests passed.");
    return 0;
}
