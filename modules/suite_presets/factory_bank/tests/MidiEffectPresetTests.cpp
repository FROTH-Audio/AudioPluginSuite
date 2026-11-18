// Pro preset tests for MIDI Chord/Arp/Scale Tool. Compiles the real processor and editor sources,
// then checks the factory bank against the live APVTS.

#include "PluginProcessor.h"
#include "PresetTestUtils.h"

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::puts ("Running MIDI Chord/Arp/Scale Tool Pro preset tests...");

    // Plan 6.1: 15 factory presets for the chord/arp engine.
    PresetTest::runBankShape<MidiEffectProcessor> ("MidiEffect", 15);

    {
        MidiEffectProcessor processor;
        PresetTest::runBankRanges (processor);
        PresetTest::runBankNotDefault (processor);
        PresetTest::runApplyMatchesFile (processor);
        PresetTest::runApplyResetsMissing (processor);
        PresetTest::runApplyTwiceIdempotent (processor);
        PresetTest::runAudioSanity (processor);

        // Plan 6.1: every factory preset runs the step clock (arpMode = Arp), so none
        // equals the default (Chord) state.
        for (const auto& preset : PresetTest::presetsFor ("MidiEffect"))
        {
            const auto mode = PresetTest::treeValue (preset.state, MidiEffectProcessor::arpModeId);
            assert (mode == 1.0f);
        }
    }

    {
        MidiEffectProcessor processor;
        PresetTest::runEditorWithBar (processor);
    }

    // Plan 09 1.4: the bar's preset selection survives the editor being recreated.
    {
        MidiEffectProcessor processor;
        PresetTest::runSelectionSurvivesReopen (processor);
    }

#if SUITE_PRESET_BANK_AS_PROGRAMS
    {
        MidiEffectProcessor processor;
        PresetTest::runProgramList (processor);
    }
#endif

    PresetTest::runMenuModel ("MidiEffect");

    std::puts ("All MIDI Chord/Arp/Scale Tool Pro preset tests passed.");
    return 0;
}
