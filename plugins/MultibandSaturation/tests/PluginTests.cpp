// Plugin-level tests for Multiband Saturation. Compiles the real processor and editor sources.

#include "PluginTestUtils.h"
#include "ModulationTestUtils.h"
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
              Processor::toneLowId, Processor::toneHighId, Processor::mixId, Processor::outputLevelId,
              Processor::mixLowId, Processor::mixHighId, Processor::thermalDriftId,
              Processor::foldLowId, Processor::foldHighId,
              Processor::lfo1RateId, Processor::lfo1ShapeId, Processor::lfo1SyncId, Processor::lfo1DivisionId, Processor::lfo1OffsetId,
              Processor::lfo2RateId, Processor::lfo2ShapeId, Processor::lfo2SyncId, Processor::lfo2DivisionId, Processor::lfo2OffsetId,
              Processor::modSourceIds[0], Processor::modTargetIds[0], Processor::modAmountIds[0], Processor::modDelayIds[0],
              Processor::modSourceIds[5], Processor::modTargetIds[5], Processor::modAmountIds[5], Processor::modDelayIds[5] },
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

        // Plan 17 Phase 9: the interest parameters (appended after the router's).
        checkRange (processor.apvts, Processor::mixLowId,      0.0f, 100.0f, 100.0f);
        checkRange (processor.apvts, Processor::mixHighId,     0.0f, 100.0f, 100.0f);
        checkRange (processor.apvts, Processor::thermalDriftId, 0.0f, 100.0f, 0.0f);
        checkRange (processor.apvts, Processor::foldLowId,     0.0f, 50.0f, 0.0f);
        checkRange (processor.apvts, Processor::foldHighId,    0.0f, 50.0f, 0.0f);

        // Plan 13: shared router parameters.
        checkRange (processor.apvts, Processor::lfo1RateId,    0.02f, 20.0f, 0.25f);
        checkRange (processor.apvts, Processor::lfo1ShapeId,   0.0f, 7.0f, 0.0f);
        checkRange (processor.apvts, Processor::lfo1SyncId,    0.0f, 1.0f, 0.0f);
        checkRange (processor.apvts, Processor::lfo1DivisionId, 0.0f, 6.0f, 4.0f);
        checkRange (processor.apvts, Processor::lfo1OffsetId, -100.0f, 100.0f, 0.0f);
        checkRange (processor.apvts, Processor::modSourceIds[0], 0.0f, 1.0f, 0.0f);
        checkRange (processor.apvts, Processor::modTargetIds[0], 0.0f, 10.0f, 0.0f);
        checkRange (processor.apvts, Processor::modAmountIds[0], -100.0f, 100.0f, 0.0f);
        checkRange (processor.apvts, Processor::modDelayIds[0],   0.0f, 10.0f, 0.0f);

        // The target choices must match the enum order the sessions save.
        auto* target = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (Processor::modTargetIds[0]));
        assert (target != nullptr);
        assert (target->choices == Processor::modTargetNames());
        assert (target->choices.size() == (int) Processor::ModTarget::count);

        assert (processor.getRouter().getNumTargets() == 10);
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

        // Plan 13: routing parameters round-trip explicitly.
        assert (std::abs (getParameter (reader.apvts, Processor::lfo1RateId) - getParameter (writer.apvts, Processor::lfo1RateId)) < 1.0e-3f);
        assert (std::abs (getParameter (reader.apvts, Processor::modAmountIds[2]) - getParameter (writer.apvts, Processor::modAmountIds[2])) < 1.0e-3f);
        assert (std::abs (getParameter (reader.apvts, Processor::modTargetIds[3]) - getParameter (writer.apvts, Processor::modTargetIds[3])) < 1.0e-3f);

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

    /** RMS of one rendered stream, in samples. */
    float rmsOf (const std::vector<float>& samples, int from, int count)
    {
        assert (from >= 0 && from + count <= (int) samples.size());
        double acc = 0.0;
        for (int i = from; i < from + count; ++i)
            acc += samples[(size_t) i] * samples[(size_t) i];
        return (float) std::sqrt (acc / (double) count);
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

    /** Plan 17 Phase 9: Mix Low 0 leaves the low band clean — a low sine's output differs
        from the fully-processed default, and the blend direction is verifiable through the
        high band (Mix High 0 with a high sine likewise). Bounded either way. */
    void testPerBandMix()
    {
        {
            Processor full;
            setParameter (full.apvts, Processor::driveLowId, 12.0f);
            const auto fullLow = render (full, 80.0, 0.9f, 8);

            Processor clean;
            setParameter (clean.apvts, Processor::driveLowId, 12.0f);
            setParameter (clean.apvts, Processor::mixLowId, 0.0f);
            const auto cleanLow = render (clean, 80.0, 0.9f, 8);

            float diff = 0.0f;
            for (size_t i = 0; i < fullLow.size(); ++i)
                diff = juce::jmax (diff, std::abs (fullLow[(size_t) i] - cleanLow[(size_t) i]));
            std::printf ("   per-band mix probe: max diff = %f\n", (double) diff);
            assert (diff > 0.05f);   // the low band is audibly cleaner at Mix Low 0

            for (float x : cleanLow)
                assert (std::abs (x) <= 1.0f + 1.0e-3f);
        }

        pass ("testPerBandMix");
    }

    /** Plan 17 Phase 9: the per-band fold adds upper harmonics — Fold Low 50 % on a loud
        low sine differs from the default and stays bounded. */
    void testPerBandFold()
    {
        Processor folded;
        setParameter (folded.apvts, Processor::driveLowId, 12.0f);
        setParameter (folded.apvts, Processor::foldLowId, 50.0f);
        const auto foldedLow = render (folded, 80.0, 0.9f, 8);

        Processor plain;
        setParameter (plain.apvts, Processor::driveLowId, 12.0f);
        const auto plainLow = render (plain, 80.0, 0.9f, 8);

        float diff = 0.0f;
        for (size_t i = 0; i < plainLow.size(); ++i)
            diff = juce::jmax (diff, std::abs (foldedLow[(size_t) i] - plainLow[(size_t) i]));
        std::printf ("   per-band fold probe: max diff = %f\n", (double) diff);
        assert (diff > 0.05f);

        for (float x : foldedLow)
            assert (std::abs (x) <= 1.0f + 1.0e-3f);

        pass ("testPerBandFold");
    }

    /** Plan 17 Phase 9: the thermal drift sags the drive slowly — with a loud sustained
        signal at 100 % the level eases down over seconds; at 0 the level holds. */
    void testThermalDrift()
    {
        const int blocks = (int) (8.0 * sampleRate / (double) blockSize);   // 8 seconds
        const int samplesPerSecond = blockSize * (int) ((double) blocks / 8.0);

        // A clean drive: the sag must move the level, and the tube curve is steepest
        // below its knee — 0 dB shows the eased drive clearly.
        Processor drifting;
        setParameter (drifting.apvts, Processor::driveLowId, 0.0f);
        setParameter (drifting.apvts, Processor::driveHighId, 0.0f);
        setParameter (drifting.apvts, Processor::thermalDriftId, 100.0f);
        const auto hot = render (drifting, 110.0, 0.95f, blocks);

        // The first 1.5 s vs the last 1.5 s: the sag (2 s attack) eases the drive.
        const int window = samplesPerSecond * 3 / 2;
        const float early = rmsOf (hot, 0, window);
        const float late = rmsOf (hot, (int) hot.size() - window, window);
        std::printf ("   thermal probe: early RMS = %f, late RMS = %f\n", (double) early, (double) late);
        assert (late < early * 0.9f);

        // The control: thermal 0 holds the level.
        Processor steady;
        setParameter (steady.apvts, Processor::driveLowId, 0.0f);
        setParameter (steady.apvts, Processor::driveHighId, 0.0f);
        const auto cold = render (steady, 110.0, 0.95f, blocks);
        const float coldEarly = rmsOf (cold, 0, window);
        const float coldLate = rmsOf (cold, (int) cold.size() - window, window);
        assert (std::abs (coldLate - coldEarly) < 0.02f * coldEarly);

        pass ("testThermalDrift");
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
        assert (editor->getWidth() == 760 && editor->getHeight() == 860);   // plan 17 Phase 9: the SHAPER group grew to three knob rows   // plan 20: the MOD panel grew to 428

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

        // Plan 15 (owner): the stereo meter pair is gone from this plugin, and the SOUND/MOD
        // switch sits in the header row, left of the preset bar.
        auto* visualComponent = multibandEditor->visualComponent();
        for (auto* child : visualComponent->getChildren())
            assert (dynamic_cast<SuiteUI::StereoLevelMeter*> (child) == nullptr);

        juce::Component* pageSwitch = PluginTest::findComponentById (*editor, "multiband-pages");
        assert (pageSwitch != nullptr);

        SuitePresets::PresetBar* bar = nullptr;
        for (int i = 0; i < editor->getNumChildComponents(); ++i)
            if (auto* found = dynamic_cast<SuitePresets::PresetBar*> (editor->getChildComponent (i)))
                bar = found;
        assert (bar != nullptr);

        assert (pageSwitch->getRight() <= bar->getX());                              // left of the preset bar
        assert (std::abs (pageSwitch->getBounds().getCentreY() - bar->getBounds().getCentreY()) <= 2); // same row

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
        }

        pass ("testVisualSnapshot");
    }

    /** Plan 13: a routed output gain changes the audio; with amount 0 the output is unchanged. */
    void testModulationReachesAudio()
    {
        auto render = [] (Processor& processor)
        {
            processor.prepareToPlay (sampleRate, blockSize);
            std::vector<float> out;
            int64_t first = 0;
            juce::MidiBuffer midi;

            for (int block = 0; block < 20; ++block)
            {
                juce::AudioBuffer<float> audio (2, blockSize);
                audio.clear();
                addSine (audio, 0, sampleRate, 220.0, 0.5f, first);
                addSine (audio, 1, sampleRate, 220.0, 0.5f, first);
                processor.processBlock (audio, midi);

                const auto* left = audio.getReadPointer (0);
                out.insert (out.end(), left, left + blockSize);
                first += blockSize;
            }

            return out;
        };

        Processor plain;
        const auto plainOutput = render (plain);

        Processor modulated;
        setLfo (modulated, 0, 4.0f, 3.0f, false, 4.0f);   // square, 4 Hz, free
        setRoute (modulated, 0, 0, (int) Processor::ModTarget::Output, 100.0f);
        const auto modulatedOutput = render (modulated);

        assert (plainOutput.size() == modulatedOutput.size());

        double difference = 0.0;
        for (size_t n = 0; n < plainOutput.size(); ++n)
            difference += std::abs (plainOutput[n] - modulatedOutput[n]);

        assert (difference / (double) plainOutput.size() > 1.0e-3);

        Processor idle;
        setLfo (idle, 0, 4.0f, 3.0f, false, 4.0f);
        setRoute (idle, 0, 0, (int) Processor::ModTarget::Output, 0.0f);
        const auto idleOutput = render (idle);
        assert (idleOutput == plainOutput);

        pass ("testModulationReachesAudio");
    }

    /** Plan 13: a synced LFO runs on the internal 120 BPM tempo clock with no host playhead. */
    void testSyncedLfoFollowsTempo()
    {
        Processor processor;
        setParameter (processor.apvts, Processor::lfo1SyncId, 1.0f);
        setParameter (processor.apvts, Processor::lfo1DivisionId, 4.0f);   // 1/4 = 1 beat per cycle
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> audio (2, blockSize);
        juce::MidiBuffer midi;

        for (int block = 0; block < 24; ++block)
        {
            audio.clear();
            processor.processBlock (audio, midi);
        }
        const float quarter = processor.getRouter().getLfoPhase (0);

        for (int block = 0; block < 24; ++block)
        {
            audio.clear();
            processor.processBlock (audio, midi);
        }
        const float half = processor.getRouter().getLfoPhase (0);

        assert (std::abs (quarter - 0.5f) < 0.02f);
        assert (std::abs (half - 0.0f) < 0.02f);

        pass ("testSyncedLfoFollowsTempo");
    }

    /** Plan 13: switching pages only toggles visibility; no parameter changes. */
    void testPageSwitchChangesNoParameter()
    {
        Processor processor;
        auto* editor = dynamic_cast<MultibandSaturationEditor*> (processor.createEditorAndMakeActive());
        assert (editor != nullptr);

        juce::MemoryBlock before;
        processor.getStateInformation (before);

        editor->setPage (1);
        assert (editor->getPage() == 1);

        editor->setPage (0);
        assert (editor->getPage() == 0);

        juce::MemoryBlock after;
        processor.getStateInformation (after);
        assert (before == after);

        processor.editorBeingDeleted (editor);
        delete editor;
        pass ("testPageSwitchChangesNoParameter");
    }

    /** Plan 13: the MOD page paints the modulation panel. */
    void testModPagePaints()
    {
        Processor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        assert (editor != nullptr);

        auto* multibandEditor = dynamic_cast<MultibandSaturationEditor*> (editor.get());
        assert (multibandEditor != nullptr);
        multibandEditor->setPage (1);

        juce::MessageManager::getInstance()->runDispatchLoopUntil (150);

        juce::Image image (juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
        {
            juce::Graphics g (image);
            editor->paintEntireComponent (g, true);
        }

        int painted = 0;
        for (int y = 0; y < image.getHeight(); y += 2)
            for (int x = 0; x < image.getWidth(); x += 2)
                if (image.getPixelAt (x, y).getAlpha() > 0)
                    ++painted;

        assert (painted > 1000);

        processor.editorBeingDeleted (editor.get());
        pass ("testModPagePaints");
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
        auto* multibandEditor = dynamic_cast<MultibandSaturationEditor*> (editor.get());
        assert (multibandEditor != nullptr);

        const juce::File file (juce::File (folder).getChildFile ("multibandsaturation-default.png"));
        assert (PluginTest::writeDeterministicPng (*editor, file));

        // Plan 13: the MOD page renders too.
        multibandEditor->setPage (1);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
        const juce::File modFile (juce::File (folder).getChildFile ("multibandsaturation-mod.png"));
        assert (PluginTest::writeDeterministicPng (*editor, modFile));
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
    testPerBandMix();
    testPerBandFold();
    testThermalDrift();
    testEditor();
    testLayoutAndHelp();
    testVisualSnapshot();
    testModulationReachesAudio();
    testSyncedLfoFollowsTempo();
    testPageSwitchChangesNoParameter();
    testModPagePaints();
    testHeaderContract();
    testRenderBaseline();

    std::puts ("All Multiband Saturation plugin tests passed.");
    return 0;
}
