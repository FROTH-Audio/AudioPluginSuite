// Pro preset tests for Sgraffito. Compiles the real processor and editor sources,
// then checks the factory bank against the live APVTS.

#include "PluginProcessor.h"
#include "PresetTestUtils.h"

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::puts ("Running Sgraffito Pro preset tests...");

    // Sgraffito ships the documented minimum of 15 factory presets.
    PresetTest::runBankShape<SgraffitoProcessor> ("Sgraffito", 15);

    {
        SgraffitoProcessor processor;
        PresetTest::runBankRanges (processor);
        PresetTest::runBankNotDefault (processor);
        PresetTest::runApplyMatchesFile (processor);
        PresetTest::runApplyResetsMissing (processor);
        PresetTest::runApplyTwiceIdempotent (processor);
        PresetTest::runAudioSanity (processor);
    }

    {
        SgraffitoProcessor processor;
        PresetTest::runEditorWithBar (processor);
    }

#if SUITE_PRESET_BANK_AS_PROGRAMS
    {
        SgraffitoProcessor processor;
        PresetTest::runProgramList (processor);
    }
#endif

    PresetTest::runMenuModel ("Sgraffito");

    std::puts ("All Sgraffito Pro preset tests passed.");
    return 0;
}
