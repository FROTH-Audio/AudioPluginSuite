// Pro preset tests for Stereo Tool Effect. Compiles the real processor and editor sources,
// then checks the factory bank against the live APVTS.

#include "PluginProcessor.h"
#include "PresetTestUtils.h"

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::puts ("Running Stereo Tool Pro preset tests...");

    // Plan 6.1: 15 factory presets for the character stereo processor.
    PresetTest::runBankShape<StereoToolProcessor> ("StereoTool", 15);

    {
        StereoToolProcessor processor;
        PresetTest::runBankRanges (processor);
        PresetTest::runBankNotDefault (processor);
        PresetTest::runApplyMatchesFile (processor);
        PresetTest::runApplyResetsMissing (processor);
        PresetTest::runApplyTwiceIdempotent (processor);
        PresetTest::runAudioSanity (processor);

        // Plan 6.1: the Tube character (index 0) is the original saturate() exactly.
        auto* character = dynamic_cast<juce::AudioParameterChoice*> (
            processor.apvts.getParameter (StereoToolProcessor::characterId));
        assert (character != nullptr);
        assert (character->choices.size() == 5);
        assert (character->choices[0] == "Tube" && character->choices[4] == "Clip");
    }

    {
        StereoToolProcessor processor;
        PresetTest::runEditorWithBar (processor);
    }

    // Plan 09 1.4: the bar's preset selection survives the editor being recreated.
    {
        StereoToolProcessor processor;
        PresetTest::runSelectionSurvivesReopen (processor);
    }

#if SUITE_PRESET_BANK_AS_PROGRAMS
    {
        StereoToolProcessor processor;
        PresetTest::runProgramList (processor);
    }
#endif

    PresetTest::runMenuModel ("StereoTool");

    std::puts ("All Stereo Tool Pro preset tests passed.");
    return 0;
}
