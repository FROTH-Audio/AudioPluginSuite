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
        // Plan 07 phase 4 (Q3 revised): factory presets may carry `embedded:` sample
        // variants. Every URI in the bank must name a real embedded resource, so
        // applying the preset actually loads it (a typo would silently keep the
        // current sample). File paths are rejected by the audit.
        SgraffitoProcessor processor;
        processor.prepareToPlay (PresetTest::sampleRate, PresetTest::blockSize);

        int embeddedCount = 0;

        for (const auto& preset : PresetTest::presetsFor (processor.getPresetPluginId()))
        {
            const auto uri = preset.state.getProperty (SgraffitoProcessor::samplePathProperty).toString();

            if (uri.isEmpty())
                continue;

            ++embeddedCount;
            assert (uri.startsWith ("embedded:"));

            processor.applyPresetState (preset.state);
            assert (processor.hasSample());
            assert (! processor.isSampleMissing());
            assert (processor.loadedSample->sourcePath == uri);
            assert (processor.apvts.state.getProperty (SgraffitoProcessor::samplePathProperty).toString() == uri);
        }

        // Plan 10 phase 7: every preset ships its own generated sample, so switching
        // presets always switches the sample (finding #7/#8). The per-URI load above is
        // the typo guard: an unknown resource name would silently keep the current sample.
        assert (embeddedCount == 20);
        PresetTest::pass ("embeddedSamples");
    }

    {
        SgraffitoProcessor processor;
        PresetTest::runEditorWithBar (processor);
    }

    // Plan 09 1.4: the bar's preset selection survives the editor being destroyed and
    // recreated, keeps the dirty mark, resets on INIT, and falls back to Init for a
    // user file that no longer resolves.
    {
        SgraffitoProcessor processor;
        PresetTest::runSelectionSurvivesReopen (processor);
    }

#if SUITE_PRESET_BANK_AS_PROGRAMS
    {
        SgraffitoProcessor processor;
        PresetTest::runProgramList (processor);

        // Plan 07 test 11: program changes under audio, without an editor (the host
        // context). Five factory programs load the embedded cloud sample (plan 07 Q3
        // revised) and program 0 (INIT) resets to the embedded default, so every cycle
        // reloads an embedded sample while processBlock runs.
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
