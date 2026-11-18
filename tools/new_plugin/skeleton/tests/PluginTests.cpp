// Plugin-level tests for __PRODUCT__. Compiles the real processor and editor sources.

#include "PluginTestUtils.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "SuiteSettings.h"

namespace
{
    using namespace PluginTest;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    using Processor = __NAME__Processor;

    void testParameters()
    {
        Processor processor;

        checkParameterIds (processor, processor.apvts, { Processor::gainId }, 1);
        checkRange (processor.apvts, Processor::gainId, -24.0f, 24.0f, 0.0f);
        pass ("testParameters");
    }

    void testStateRoundTrip()
    {
        Processor processor;

        checkStateRoundTrip (processor, processor.apvts, Processor::gainId, 6.0f);
        pass ("testStateRoundTrip");
    }

    void testGainScalesSignal()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        setParameter (processor.apvts, Processor::gainId, -6.0f);

        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();
        addSine (buffer, 0, sampleRate, 1000.0, 0.5f, 0);
        addSine (buffer, 1, sampleRate, 1000.0, 0.5f, 0);

        juce::MidiBuffer midi;
        processor.processBlock (buffer, midi);

        // -6 dB is about 0.501 times the amplitude: 0.5 becomes about 0.25.
        const auto out = peak (buffer);
        assert (allFinite (buffer));
        assert (out > 0.24f && out < 0.26f);
        pass ("testGainScalesSignal");
    }

    void testEditor()
    {
        Processor processor;
        checkEditor (processor);
        pass ("testEditor");
    }

    // R12: the processor implements SuitePresets::StateHolder, and the preset identity
    // survives a save/load round trip (plan 09 1.4).
    void testPresetSeam()
    {
        Processor processor;

        assert (processor.getPresetPluginId() == "__NAME__");
        assert (processor.getPresetStateVersion() == processor.stateVersion);
        assert (&processor.getPresetStateTree() == &processor.apvts);

        SuitePresets::PresetSelection selection;
        selection.kind = SuitePresets::PresetSelection::Kind::factory;
        selection.factoryIndex = 3;
        processor.setPresetSelection (selection);

        assert (processor.apvts.state.getProperty (Processor::presetSelectionProperty).toString()
                == "factory:3");

        juce::MemoryBlock state;
        processor.getStateInformation (state);

        processor.setPresetSelection ({});
        assert (processor.getPresetSelection().kind == SuitePresets::PresetSelection::Kind::none);

        processor.setStateInformation (state.getData(), (int) state.getSize());
        assert (processor.getPresetSelection() == selection);

        // A preset tree is a delta: applied with resetMissing, absent IDs go back to default.
        auto tree = processor.apvts.copyState();
        processor.applyPresetState (tree);
        assert (processor.getPresetSelection().kind == SuitePresets::PresetSelection::Kind::none);

        pass ("testPresetSeam");
    }

    // R12/R13: the preset bar and the gear menu are in the editor header, and the gear's two
    // dialogs build and paint. The dialogs themselves are async (plan 08), so this checks
    // their content rather than opening modal windows.
    void testHeaderContract()
    {
        Processor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        assert (editor != nullptr);

        SuitePresets::PresetBar* bar = nullptr;
        SuiteUI::HeaderMenuButton* gear = nullptr;

        for (int i = 0; i < editor->getNumChildComponents(); ++i)
        {
            auto* child = editor->getChildComponent (i);

            if (auto* found = dynamic_cast<SuitePresets::PresetBar*> (child))
                bar = found;

            if (auto* found = dynamic_cast<SuiteUI::HeaderMenuButton*> (child))
                gear = found;
        }

        assert (bar != nullptr);
        assert (gear != nullptr);
        assert (gear->onAbout != nullptr);
        assert (gear->onSettings != nullptr);

        // The gear is the rightmost element of the header, with the bar to its left.
        assert (gear->getRight() > editor->getWidth() - 40);
        assert (bar->getRight() <= gear->getX());

        {
            SuiteUI::AboutPanel about ({}, "__PRODUCT__", "1.0.0", "__CODE__", "__NAME__");
            const auto image = paintComponent (about);
            assert (image.getPixelAt (8, 8) == SuiteUI::Colours::panel);
        }

        {
            // A redirected store: the test never touches the user's settings file.
            const auto file = juce::File::getCurrentWorkingDirectory()
                                  .getChildFile ("__NAME__HeaderContractSettings.xml");
            file.deleteFile();

            SuiteSettings::Store store (file);
            SuiteSettings::SettingsDialog dialog (store, false);
            const auto image = paintComponent (dialog);
            assert (image.getPixelAt (8, 8) == SuiteUI::Colours::panel);

            file.deleteFile();
        }

        processor.editorBeingDeleted (editor.get());
        editor.reset();

        pass ("testHeaderContract");
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::puts ("Running __PRODUCT__ plugin tests...");

    testParameters();
    testStateRoundTrip();
    testGainScalesSignal();
    testEditor();
    testPresetSeam();
    testHeaderContract();

    std::puts ("All __PRODUCT__ plugin tests passed.");
    return 0;
}
