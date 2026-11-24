// Pro preset tests for Multiband Saturation. Compiles the real processor and editor sources,
// then checks the factory bank against the live APVTS.

#include "PluginProcessor.h"
#include "PresetTestUtils.h"

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::puts ("Running Multiband Saturation Pro preset tests...");

    // Plan 11: 15 factory presets for the two-band split.
    PresetTest::runBankShape<MultibandSaturationProcessor> ("MultibandSaturation", 15);

    {
        MultibandSaturationProcessor processor;
        PresetTest::runBankRanges (processor);
        PresetTest::runBankNotDefault (processor);
        PresetTest::runApplyMatchesFile (processor);
        PresetTest::runApplyResetsMissing (processor);
        PresetTest::runApplyTwiceIdempotent (processor);
        PresetTest::runAudioSanity (processor);

        // The choice list the presets index into: three saturation modes.
        auto* mode = dynamic_cast<juce::AudioParameterChoice*> (
            processor.apvts.getParameter (MultibandSaturationProcessor::modeId));
        assert (mode != nullptr);
        assert (mode->choices.size() == 3);
        assert (mode->choices[0] == "Tube" && mode->choices[2] == "Diode");
    }

    {
        MultibandSaturationProcessor processor;
        PresetTest::runEditorWithBar (processor);
    }

    // Plan 09 1.4: the bar's preset selection survives the editor being recreated.
    {
        MultibandSaturationProcessor processor;
        PresetTest::runSelectionSurvivesReopen (processor);
    }

#if SUITE_PRESET_BANK_AS_PROGRAMS
    {
        MultibandSaturationProcessor processor;
        PresetTest::runProgramList (processor);
    }
#endif

    PresetTest::runMenuModel ("MultibandSaturation");

    std::puts ("All Multiband Saturation Pro preset tests passed.");
    return 0;
}
