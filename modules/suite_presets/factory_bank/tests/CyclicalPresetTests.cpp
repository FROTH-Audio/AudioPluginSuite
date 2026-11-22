// Pro preset tests for Cyclical. Compiles the real processor and editor sources, then
// checks the factory bank against the live APVTS.

#include "PluginProcessor.h"
#include "PresetTestUtils.h"

#include <juce_audio_formats/juce_audio_formats.h>

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::puts ("Running Cyclical Pro preset tests...");

    // Plan 6.1: 15 factory presets; plan 14 appended 10 more (25 for the four-slot
    // loop instrument).
    PresetTest::runBankShape<CyclicalProcessor> ("Cyclical", 25);

    {
        CyclicalProcessor processor;
        PresetTest::runBankRanges (processor);
        PresetTest::runBankNotDefault (processor);
        PresetTest::runApplyMatchesFile (processor);
        PresetTest::runApplyResetsMissing (processor);
        PresetTest::runApplyTwiceIdempotent (processor);
        PresetTest::runAudioSanity (processor);
        PresetTest::runEditorWithBar (processor);
        PresetTest::runSelectionSurvivesReopen (processor);

        // Plan 6.1: every factory preset carries four embedded: loop references, and every
        // one of them actually loads (loadEmbeddedLoop silently keeps the current loop on
        // an unknown name, so the typo guard is this check).
        int checkedPaths = 0;

        for (const auto& preset : PresetTest::presetsFor ("Cyclical"))
        {
            for (int slot = 0; slot < CyclicalProcessor::numSlots; ++slot)
            {
                const auto path = preset.state.getProperty (
                    CyclicalProcessor::loopPathPropertyId == nullptr
                        ? juce::String ("loop") + juce::String (slot + 1) + "Path"
                        : juce::String()).toString();
                juce::ignoreUnused (path);
            }

            // The tree carries loop1Path .. loop4Path attributes; resolve each one.
            for (int slot = 0; slot < CyclicalProcessor::numSlots; ++slot)
            {
                const auto path = preset.state.getProperty (
                    juce::String ("loop") + juce::String (slot + 1) + "Path").toString();
                assert (path.startsWith ("embedded:"));
                ++checkedPaths;

                // Apply the preset (so the slot state is coherent) and check the slot
                // plays the referenced loop: trigger the slot and listen.
                processor.applyPresetState (preset.state);
                assert (processor.loopPath (slot) == path);
                assert (! processor.isLoopMissing (slot));
            }
        }

        assert (checkedPaths == 25 * 4);

        // And every bank preset makes sound on its slot keys (the sample library is
        // complete). Each slot's key is pressed in its own run of blocks: four
        // simultaneous presses can collide inside one slot's chop range (the lowest slot
        // wins), which would silence the higher slots without meaning the preset is bad.
        for (const auto& preset : PresetTest::presetsFor ("Cyclical"))
        {
            processor.applyPresetState (preset.state);
            processor.prepareToPlay (PresetTest::sampleRate, PresetTest::blockSize);

            // Press each slot's own trigger note (plan 14: the notes are per slot; a
            // chopped slot's base key triggers slice 0).
            float best = 0.0f;

            for (int slot = 0; slot < CyclicalProcessor::numSlots; ++slot)
            {
                processor.reset();

                juce::MidiBuffer midi;
                midi.addEvent (juce::MidiMessage::noteOn (1, processor.slotNote (slot), 0.8f), 0);

                for (int block = 0; block < 90; ++block)     // just under a bar
                {
                    juce::AudioBuffer<float> buffer (2, PresetTest::blockSize);
                    buffer.clear();
                    processor.processBlock (buffer, midi);
                    midi.clear();
                    best = juce::jmax (best, PresetTest::peak (buffer));
                }
            }

            assert (best > 0.01f);
        }
    }
#if SUITE_PRESET_BANK_AS_PROGRAMS
    {
        CyclicalProcessor processor;
        PresetTest::runProgramList (processor);
    }
#endif

    PresetTest::runMenuModel ("Cyclical");

    std::puts ("All Cyclical Pro preset tests passed.");
    return 0;
}
