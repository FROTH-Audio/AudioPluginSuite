// Pro preset tests for Sgraffito. Compiles the real processor and editor sources,
// then checks the factory bank against the live APVTS.

#include "PluginProcessor.h"
#include "PresetTestUtils.h"

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::puts ("Running Sgraffito Pro preset tests...");

    // Sgraffito ships the documented minimum of 20 factory presets (the minimum is 15; F1.3 added
    // five more that use the attack and release envelopes and showcase the performance controls).
    PresetTest::runBankShape<SgraffitoProcessor> ("Sgraffito", 20);

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

        // Plan 07 test 11: program changes under audio, without an editor (the host
        // context). The factory programs are sample-free (plan 07 Q3), but program 0
        // (INIT) resets the sample to the embedded default, so every cycle reloads it
        // while processBlock runs.
        processor.prepareToPlay (PresetTest::sampleRate, PresetTest::blockSize);

        for (int program = 0; program < processor.getNumPrograms(); ++program)
        {
            processor.setCurrentProgram (program);

            juce::AudioBuffer<float> audio (2, PresetTest::blockSize);
            audio.clear();
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
            processor.processBlock (audio, midi);

            assert (PresetTest::allFinite (audio));
        }

        // The last change put INIT back: the embedded default is loaded, unreported, and
        // the state carries the embedded URI for the next save.
        processor.setCurrentProgram (0);
        assert (processor.hasSample());
        assert (processor.apvts.state.getProperty (SgraffitoProcessor::samplePathProperty).toString()
                == SgraffitoProcessor::embeddedSampleUri);
        assert (! processor.isSampleMissing());
    }
#endif

    PresetTest::runMenuModel ("Sgraffito");

    std::puts ("All Sgraffito Pro preset tests passed.");
    return 0;
}
