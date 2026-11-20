// Plugin-level tests for Multiband Saturation. Compiles the real processor and editor sources.

#include "PluginTestUtils.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "SuiteSettings.h"

namespace
{
    using namespace PluginTest;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    using Processor = MultibandSaturationProcessor;

    void testParameters()
    {
        Processor processor;

        checkParameterIds (processor, processor.apvts,
            { Processor::modeId, Processor::crossoverId, Processor::driveLowId, Processor::driveHighId,
              Processor::sensitivityId, Processor::attackId, Processor::releaseId,
              Processor::toneLowId, Processor::toneHighId, Processor::mixId, Processor::outputLevelId },
            Processor::numControls);

        checkRange (processor.apvts, Processor::crossoverId,   80.0f, 8000.0f, 500.0f);
        checkRange (processor.apvts, Processor::driveLowId,    0.0f, 24.0f, 6.0f);
        checkRange (processor.apvts, Processor::driveHighId,   0.0f, 24.0f, 6.0f);
        checkRange (processor.apvts, Processor::sensitivityId, 0.0f, 1.0f, 0.5f);
        checkRange (processor.apvts, Processor::attackId,      1.0f, 100.0f, 5.0f);
        checkRange (processor.apvts, Processor::releaseId,     50.0f, 2000.0f, 150.0f);
        checkRange (processor.apvts, Processor::toneLowId,     -1.0f, 1.0f, 0.0f);
        checkRange (processor.apvts, Processor::toneHighId,    -1.0f, 1.0f, 0.0f);
        checkRange (processor.apvts, Processor::mixId,         0.0f, 100.0f, 100.0f);
        checkRange (processor.apvts, Processor::outputLevelId, -12.0f, 6.0f, 0.0f);
        pass ("testParameters");
    }

    /** Saves the state with every parameter moved, restores it into a fresh processor and
        checks each value came back. */
    void testStateRoundTrip()
    {
        Processor writer;
        writer.prepareToPlay (sampleRate, blockSize);

        for (auto* parameter : writer.getParameters())
            parameter->setValueNotifyingHost (dynamic_cast<juce::AudioParameterBool*> (parameter) != nullptr ? 1.0f : 0.73f);

        juce::MemoryBlock saved;
        writer.getStateInformation (saved);

        Processor reader;
        reader.prepareToPlay (sampleRate, blockSize);
        reader.setStateInformation (saved.getData(), (int) saved.getSize());

        int compared = 0;
        for (auto* parameter : writer.getParameters())
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
            {
                const auto* restored = reader.apvts.getParameter (withId->paramID);
                assert (restored != nullptr);
                assert (std::abs (restored->getValue() - parameter->getValue()) < 1.0e-6f);
                ++compared;
            }

        assert (compared == Processor::numControls);
        pass ("testStateRoundTrip");
    }

    /** Left output of blocks of one sine. */
    std::vector<float> render (Processor& processor, double hz, float amplitude, int blocks)
    {
        processor.prepareToPlay (sampleRate, blockSize);
        juce::MidiBuffer midi;
        int64_t first = 0;
        std::vector<float> out;

        for (int block = 0; block < blocks; ++block)
        {
            juce::AudioBuffer<float> audio (2, blockSize);
            audio.clear();
            addSine (audio, 0, sampleRate, hz, amplitude, first);
            addSine (audio, 1, sampleRate, hz, amplitude, first);
            processor.processBlock (audio, midi);

            const auto* left = audio.getReadPointer (0);
            out.insert (out.end(), left, left + blockSize);
            first += blockSize;
        }

        return out;
    }

    /** No sample may exceed the rails: the bands are clamped after they sum. */
    void testOutputStaysBounded()
    {
        Processor processor;
        setParameter (processor.apvts, Processor::driveLowId, 24.0f);
        setParameter (processor.apvts, Processor::driveHighId, 24.0f);

        const auto out = render (processor, 440.0, 0.95f, 8);
        float largest = 0.0f;
        for (float x : out)
            largest = juce::jmax (largest, std::abs (x));

        assert (largest <= 1.0f + 1.0e-3f);
        pass ("testOutputStaysBounded");
    }

    /** No latency is added, so Mix 0 is the untouched dry input once the smoother has settled. */
    void testMixZeroIsDry()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameter (processor.apvts, Processor::driveLowId, 24.0f);
        setParameter (processor.apvts, Processor::driveHighId, 24.0f);
        setParameter (processor.apvts, Processor::mixId, 0.0f);

        // The mix smoother ramps over 50 ms; pump it to its target first.
        juce::MidiBuffer warmupMidi;
        juce::AudioBuffer<float> warmup (2, blockSize);
        for (int block = 0; block < 8; ++block)
        {
            warmup.clear();
            processor.processBlock (warmup, warmupMidi);
        }

        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();
        addSine (buffer, 0, sampleRate, 440.0, 0.5f, 0);
        addSine (buffer, 1, sampleRate, 440.0, 0.5f, 0);
        juce::AudioBuffer<float> expected (buffer);

        juce::MidiBuffer midi;
        processor.processBlock (buffer, midi);

        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < blockSize; ++n)
                assert (buffer.getSample (ch, n) == expected.getSample (ch, n));
        pass ("testMixZeroIsDry");
    }

    /** A tone well below the crossover lands in the low band: the low drive shapes it and the
        high drive moves it far less. A multiband behaviour test, not just a gain test. */
    void testBandsShapeIndependently()
    {
        auto out = [&] (float driveLow, float driveHigh)
        {
            Processor processor;
            setParameter (processor.apvts, Processor::driveLowId, driveLow);
            setParameter (processor.apvts, Processor::driveHighId, driveHigh);
            return render (processor, 100.0, 0.5f, 8);   // 100 Hz, ~2.3 octaves below 500 Hz
        };

        const auto bothIdle = out (0.0f, 0.0f);
        const auto lowDriven = out (24.0f, 0.0f);
        const auto highDriven = out (0.0f, 24.0f);

        auto maxDiff = [] (const std::vector<float>& a, const std::vector<float>& b)
        {
            float d = 0.0f;
            for (size_t i = 0; i < a.size(); ++i)
                d = juce::jmax (d, std::abs (a[i] - b[i]));
            return d;
        };

        // The low drive changes a below-crossover tone strongly.
        const float lowDifference = maxDiff (bothIdle, lowDriven);
        assert (lowDifference > 0.05f);

        // The high drive moves it far less (its band is tens of dB down; a +24 dB drive still
        // amplifies the leaked remnant, so the bound stays qualitative).
        const float highDifference = maxDiff (bothIdle, highDriven);
        assert (highDifference < 0.5f * lowDifference);

        pass ("testBandsShapeIndependently");
    }

    void testEditor()
    {
        Processor processor;
        checkEditor (processor);
        pass ("testEditor");
    }

    /** Plan 12: the narrower window, the taller visualiser, the group frames and the
        shared help line. */
    void testLayoutAndHelp()
    {
        Processor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        assert (editor != nullptr);
        assert (editor->getWidth() == 760 && editor->getHeight() == 700);

        // The layout exposes its bands for the tests: the visualiser is at least 50 %
        // higher than the old 130 and the window is narrower than before.
        auto* multibandEditor = dynamic_cast<MultibandSaturationEditor*> (editor.get());
        assert (multibandEditor != nullptr);
        const auto visual = multibandEditor->visualRect();
        assert (visual.getHeight() >= 195);
        assert (visual.getWidth() < 900);
        assert (multibandEditor->splitRect().getHeight() < SuiteUI::knobRowHeight + 40);   // one row

        // The rename: the host-visible name and the parameter's display name.
        assert (processor.getName() == "MultiSat");
        assert (processor.apvts.getParameter (MultibandSaturationProcessor::outputLevelId)
                    ->getName (100) == "Output");

        PluginTest::checkHelpLine (*editor, "multiband-help", *multibandEditor->visualComponent());

        processor.editorBeingDeleted (editor.get());
        pass ("testLayoutAndHelp");
    }

    /** Plan 11 D7: the band snapshot's envelopes rise with signal and stay at zero when idle. */
    void testVisualSnapshot()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        // Idle: no envelope movement.
        {
            juce::MidiBuffer midi;
            juce::AudioBuffer<float> silence (2, blockSize);
            silence.clear();
            processor.processBlock (silence, midi);
            assert (processor.getBandSnapshot().envelope[0].load() < 1.0e-6f);
            assert (processor.getBandSnapshot().envelope[1].load() < 1.0e-6f);
        }

        // Signal: both bands see envelope movement (the input spans the crossover).
        {
            juce::MidiBuffer midi;
            juce::AudioBuffer<float> audio (2, blockSize);
            audio.clear();
            addSine (audio, 0, sampleRate, 440.0, 0.5f, 0);
            addSine (audio, 1, sampleRate, 440.0, 0.5f, 0);
            processor.processBlock (audio, midi);

            assert (processor.getBandSnapshot().envelope[0].load() > 0.01f);
            assert (processor.getBandSnapshot().envelope[1].load() > 0.01f);
            assert (processor.getBandSnapshot().inputPeak.load() > 0.1f);
            assert (processor.getBandSnapshot().outputPeak.load() > 0.01f);
        }

        pass ("testVisualSnapshot");
    }

    /** R13: the header carries the preset bar and the menu menu, the menu's two actions are
        wired, the menu sits at the far right of the header with the bar left of it, and both
        dialogs build and paint. */
    void testHeaderContract()
    {
        Processor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        assert (editor != nullptr);

        SuitePresets::PresetBar* bar = nullptr;
        SuiteUI::HeaderMenuButton* menu = nullptr;

        for (int i = 0; i < editor->getNumChildComponents(); ++i)
        {
            auto* child = editor->getChildComponent (i);

            if (auto* found = dynamic_cast<SuitePresets::PresetBar*> (child))
                bar = found;

            if (auto* found = dynamic_cast<SuiteUI::HeaderMenuButton*> (child))
                menu = found;
        }

        assert (bar != nullptr);
        assert (menu != nullptr);
        assert (menu->onAbout != nullptr);
        assert (menu->onSettings != nullptr);

        assert (menu->getRight() > editor->getWidth() - 40);
        assert (bar->getRight() <= menu->getX());

        {
            SuiteUI::AboutPanel about ({}, "Multiband Saturation", "1.0.0", "Msat", "MultibandSaturation");
            const auto image = paintComponent (about);
            assert (image.getPixelAt (8, 8) == SuiteUI::Colours::panel);
        }

        {
            const auto file = juce::File::getCurrentWorkingDirectory()
                                  .getChildFile ("MultibandSaturationHeaderContractSettings.xml");
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

    /** Writes the editor render when SUITE_RENDER_DIR is set (plan 05 R9). */
    void testRenderBaseline()
    {
        const auto folder = juce::SystemStats::getEnvironmentVariable ("SUITE_RENDER_DIR", {});

        if (folder.isEmpty())
        {
            std::printf ("[SKIP] editor renders: set SUITE_RENDER_DIR to write them\n");
            return;
        }

        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        assert (editor != nullptr);
        const juce::File file (juce::File (folder).getChildFile ("multibandsaturation-default.png"));
        assert (PluginTest::writeDeterministicPng (*editor, file));
        processor.editorBeingDeleted (editor.get());

        pass ("testRenderBaseline");
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::puts ("Running Multiband Saturation plugin tests...");

    testParameters();
    testStateRoundTrip();
    testOutputStaysBounded();
    testMixZeroIsDry();
    testBandsShapeIndependently();
    testEditor();
    testLayoutAndHelp();
    testVisualSnapshot();
    testHeaderContract();
    testRenderBaseline();

    std::puts ("All Multiband Saturation plugin tests passed.");
    return 0;
}
