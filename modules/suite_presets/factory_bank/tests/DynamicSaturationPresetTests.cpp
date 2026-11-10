// Pro preset tests for Dynamic Saturation Companion. Compiles the real processor and editor
// sources, then checks the factory bank against the live APVTS.

#include "PluginProcessor.h"
#include "PresetTestUtils.h"

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::puts ("Running Dynamic Saturation Pro preset tests...");

    PresetTest::runBankShape<DynamicSaturationProcessor> ("DynamicSaturation");

    {
        DynamicSaturationProcessor processor;
        PresetTest::runBankRanges (processor);
        PresetTest::runBankNotDefault (processor);
        PresetTest::runApplyMatchesFile (processor);
        PresetTest::runApplyResetsMissing (processor);
        PresetTest::runApplyTwiceIdempotent (processor);
        PresetTest::runAudioSanity (processor);
    }

    {
        DynamicSaturationProcessor processor;
        PresetTest::runEditorWithBar (processor);
    }

    {
        DynamicSaturationProcessor processor;
        PresetTest::runProgramList (processor);
    }

    PresetTest::runMenuModel ("DynamicSaturation");

    std::puts ("All Dynamic Saturation Pro preset tests passed.");
    return 0;
}
