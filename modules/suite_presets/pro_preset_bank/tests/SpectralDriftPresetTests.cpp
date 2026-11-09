// Pro preset tests for Spectral Drift Shifter. Compiles the real processor and editor sources,
// then checks the factory bank against the live APVTS.

#include "PluginProcessor.h"
#include "PresetTestUtils.h"

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::puts ("Running Spectral Drift Pro preset tests...");

    PresetTest::runBankShape<SpectralDriftProcessor> ("SpectralDrift");

    {
        SpectralDriftProcessor processor;
        PresetTest::runBankRanges (processor);
        PresetTest::runBankNotDefault (processor);
        PresetTest::runApplyMatchesFile (processor);
        PresetTest::runApplyResetsMissing (processor);
        PresetTest::runApplyTwiceIdempotent (processor);
        PresetTest::runAudioSanity (processor);
    }

    {
        SpectralDriftProcessor processor;
        PresetTest::runEditorWithBar (processor);
    }

#if SUITE_PRO_PRESETS_AS_PROGRAMS
    {
        SpectralDriftProcessor processor;
        PresetTest::runProgramList (processor);
    }
#endif

    PresetTest::runMenuModel ("SpectralDrift");

    std::puts ("All Spectral Drift Pro preset tests passed.");
    return 0;
}
