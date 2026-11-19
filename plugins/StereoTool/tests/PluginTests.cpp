// Plugin-level tests for Stereo Tool Effect. Compiles the real processor and editor sources.

#include "PluginTestUtils.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "SuiteSettings.h"

namespace
{
    using namespace PluginTest;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    using Processor = StereoToolProcessor;

    void testParameters()
    {
        Processor processor;

        checkParameterIds (processor, processor.apvts,
            { Processor::widthId, Processor::driveId, Processor::mixId,
              Processor::characterId, Processor::transientAttackId, Processor::transientReleaseId,
              Processor::transientAmountId, Processor::transientTargetId, Processor::midFocusId,
              Processor::sideFilterId, Processor::sideCrossoverId }, 11);
        checkRange (processor.apvts, Processor::widthId, -100.0f, 100.0f, 0.0f);
        checkRange (processor.apvts, Processor::driveId, -24.0f, 24.0f, 0.0f);
        checkRange (processor.apvts, Processor::mixId,   0.0f, 100.0f, 100.0f);
        checkRange (processor.apvts, Processor::characterId,        0.0f, 4.0f, 0.0f);
        checkRange (processor.apvts, Processor::transientAttackId,  1.0f, 200.0f, 5.0f);
        checkRange (processor.apvts, Processor::transientReleaseId, 20.0f, 2000.0f, 150.0f);
        checkRange (processor.apvts, Processor::transientAmountId,  0.0f, 100.0f, 0.0f);
        checkRange (processor.apvts, Processor::transientTargetId,  0.0f, 2.0f, 2.0f);
        checkRange (processor.apvts, Processor::midFocusId,       -100.0f, 100.0f, 0.0f);
        checkRange (processor.apvts, Processor::sideFilterId,        0.0f, 3.0f, 0.0f);
        checkRange (processor.apvts, Processor::sideCrossoverId,    20.0f, 8000.0f, 200.0f);
        pass ("testParameters");
    }

    void testStateRoundTrip()
    {
        Processor processor;

        checkStateRoundTrip (processor, processor.apvts, Processor::widthId, 40.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::driveId, 6.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::mixId,   50.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::characterId, 2.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::transientAttackId, 25.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::transientReleaseId, 400.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::transientAmountId, 60.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::transientTargetId, 0.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::midFocusId, -30.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::sideFilterId, 1.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::sideCrossoverId, 4000.0f);
        pass ("testStateRoundTrip");
    }

    /** A state from a newer plugin changes nothing. */
    void testNewerStateRefused()
    {
        Processor processor;
        setParameter (processor.apvts, Processor::driveId, 9.0f);

        juce::MemoryBlock state;
        processor.getStateInformation (state);
        auto xml = SuiteState::detail::decodeXml (state.getData(), (int) state.getSize());
        assert (xml != nullptr);
        xml->setAttribute (SuiteState::versionProperty, 99);

        const juce::MemoryBlock newer = SuiteState::detail::encodeXml (*xml);
        processor.setStateInformation (newer.getData(), (int) newer.getSize());

        assert (std::abs (getParameter (processor.apvts, Processor::driveId) - 9.0f) < 1.0e-3f);
        pass ("testNewerStateRefused");
    }

    /** Plan 6.1: a state saved before the new controls loads with every new parameter at
        its default. The change is additive, so the version stays 1. */
    void testAdditiveStateLoad()
    {
        Processor processor;
        setParameter (processor.apvts, Processor::characterId, 3.0f);
        setParameter (processor.apvts, Processor::midFocusId, 40.0f);
        setParameter (processor.apvts, Processor::sideCrossoverId, 3000.0f);

        juce::MemoryBlock state;
        processor.getStateInformation (state);

        // Strip the eight plan-6.1 parameters from the state tree, like an old session.
        auto xml = SuiteState::detail::decodeXml (state.getData(), (int) state.getSize());
        assert (xml != nullptr);
        auto tree = juce::ValueTree::fromXml (*xml);

        for (const char* id : { Processor::characterId, Processor::transientAttackId,
                                Processor::transientReleaseId, Processor::transientAmountId,
                                Processor::transientTargetId, Processor::midFocusId,
                                Processor::sideFilterId, Processor::sideCrossoverId })
        {
            const auto child = tree.getChildWithProperty ("id", juce::String (id));
            assert (child.isValid());
            tree.removeChild (child, nullptr);
        }

        const juce::MemoryBlock block = SuiteState::detail::encodeXml (*tree.createXml());
        processor.setStateInformation (block.getData(), (int) block.getSize());

        assert (std::abs (getParameter (processor.apvts, Processor::characterId) - 0.0f) < 1.0e-6f);
        assert (std::abs (getParameter (processor.apvts, Processor::midFocusId) - 0.0f) < 1.0e-6f);
        assert (std::abs (getParameter (processor.apvts, Processor::sideCrossoverId) - 200.0f) < 1.0e-3f);
        pass ("testAdditiveStateLoad");
    }

    /** A stereo test signal: different content per channel, so the mid and side both carry energy. */
    void fillStereoSignal (juce::AudioBuffer<float>& buffer)
    {
        buffer.clear();
        addSine (buffer, 0, sampleRate, 440.0, 0.5f, 0);
        addSine (buffer, 1, sampleRate, 880.0, 0.25f, 0);
    }

    /** True when two buffers differ somewhere (both must have the same shape). */
    bool buffersDiffer (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
    {
        assert (a.getNumChannels() == b.getNumChannels() && a.getNumSamples() == b.getNumSamples());
        for (int ch = 0; ch < a.getNumChannels(); ++ch)
            for (int n = 0; n < a.getNumSamples(); ++n)
                if (a.getSample (ch, n) != b.getSample (ch, n))
                    return true;
        return false;
    }

    /** The key regression: at the defaults (width 0, drive 0) the plugin is bit-transparent,
        whatever the mix, because the wet path equals the dry input exactly. */
    void testTransparentAtDefaults()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> buffer (2, blockSize);
        fillStereoSignal (buffer);
        juce::AudioBuffer<float> expected (buffer);

        juce::MidiBuffer midi;
        processor.processBlock (buffer, midi);

        assert (allFinite (buffer));
        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < blockSize; ++n)
                assert (buffer.getSample (ch, n) == expected.getSample (ch, n));
        pass ("testTransparentAtDefaults");
    }

    /** Plan 6.1: non-neutral values of controls that are inactive at their neutral setting
        still leave the default path bit-exact: the follower is bypassed at Amount 0 and the
        side filter is bypassed while Off. */
    void testInactiveControlsStayNeutral()
    {
        Processor reference;
        reference.prepareToPlay (sampleRate, blockSize);

        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameter (processor.apvts, Processor::transientAttackId, 200.0f);
        setParameter (processor.apvts, Processor::transientReleaseId, 20.0f);
        setParameter (processor.apvts, Processor::transientTargetId, 0.0f);
        setParameter (processor.apvts, Processor::sideCrossoverId, 8000.0f);
        setParameter (processor.apvts, Processor::mixId, 100.0f);

        juce::AudioBuffer<float> referenceBuffer (2, blockSize), buffer (2, blockSize);
        fillStereoSignal (referenceBuffer);
        buffer = referenceBuffer;

        juce::MidiBuffer midi;
        reference.processBlock (referenceBuffer, midi);
        processor.processBlock (buffer, midi);

        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < blockSize; ++n)
                assert (buffer.getSample (ch, n) == referenceBuffer.getSample (ch, n));
        pass ("testInactiveControlsStayNeutral");
    }

    /** Width scales the side component: +100 doubles it, -100 folds to mono. */
    void testWidthControlsStereoImage()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();
        addSine (buffer, 0, sampleRate, 440.0, 0.5f, 0);
        addSine (buffer, 1, sampleRate, 440.0, -0.5f, 0);   // anti-phase: pure side

        // Width +100: side doubled -> peak about 1.0 from a 0.5 input.
        setParameter (processor.apvts, Processor::widthId, 100.0f);
        juce::MidiBuffer midi;
        processor.processBlock (buffer, midi);
        const float widePeak = peak (buffer);
        assert (widePeak > 0.95f && widePeak < 1.01f);

        // Width -100: side removed -> silence (the input was pure side).
        setParameter (processor.apvts, Processor::widthId, -100.0f);
        buffer.clear();
        addSine (buffer, 0, sampleRate, 440.0, 0.5f, 0);
        addSine (buffer, 1, sampleRate, 440.0, -0.5f, 0);
        processor.processBlock (buffer, midi);
        assert (peak (buffer) < 1e-4f);
        pass ("testWidthControlsStereoImage");
    }

    /** Drive 0 dB is transparent; +24 dB pushes a loud signal to the rails and stays bounded. */
    void testDriveSaturatesAndStaysBounded()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        // At Drive 24 dB the wet path saturates: a 0.9 sine sits near the rail, bounded.
        setParameter (processor.apvts, Processor::driveId, 24.0f);

        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();
        addSine (buffer, 0, sampleRate, 220.0, 0.9f, 0);
        addSine (buffer, 1, sampleRate, 220.0, 0.9f, 0);

        juce::MidiBuffer midi;
        processor.processBlock (buffer, midi);

        assert (allFinite (buffer));
        const float satPeak = peak (buffer);
        assert (satPeak > 0.95f && satPeak <= 1.001f);

        // Mix 0 passes the untouched dry input through.
        setParameter (processor.apvts, Processor::mixId, 0.0f);
        buffer.clear();
        addSine (buffer, 0, sampleRate, 220.0, 0.9f, 0);
        addSine (buffer, 1, sampleRate, 220.0, 0.9f, 0);
        juce::AudioBuffer<float> expected (buffer);
        processor.processBlock (buffer, midi);
        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < blockSize; ++n)
                assert (buffer.getSample (ch, n) == expected.getSample (ch, n));
        pass ("testDriveSaturatesAndStaysBounded");
    }

    /** Plan 6.1: each character produces a measurably different output at the same drive,
        and every character stays bounded. */
    void testCharactersDiffer()
    {
        const float driveDb = 12.0f;

        std::vector<juce::AudioBuffer<float>> outputs;

        for (int character = 0; character < 5; ++character)
        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            setParameter (processor.apvts, Processor::driveId, driveDb);
            setParameter (processor.apvts, Processor::characterId, (float) character);

            juce::AudioBuffer<float> buffer (2, blockSize);
            buffer.clear();
            addSine (buffer, 0, sampleRate, 220.0, 0.5f, 0);
            addSine (buffer, 1, sampleRate, 220.0, 0.5f, 0);

            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);

            assert (allFinite (buffer));
            assert (peak (buffer) <= 1.0f + 1.0e-3f);   // bounded for every character
            outputs.push_back (buffer);
        }

        // Tube is the original curve; the other four are pairwise different from it and
        // from each other somewhere in the buffer.
        for (int a = 0; a < 5; ++a)
            for (int b = a + 1; b < 5; ++b)
                assert (buffersDiffer (outputs[(size_t) a], outputs[(size_t) b]));
        pass ("testCharactersDiffer");
    }

    /** Plan 6.1: mid focus follows the gain law mid = 1 + f, side = 1 - f. */
    void testMidFocusGainLaw()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameter (processor.apvts, Processor::midFocusId, 100.0f);   // f = 1

        // Mono input: pure mid. The mid doubles; the side is zero either way.
        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();
        addSine (buffer, 0, sampleRate, 440.0, 0.25f, 0);
        addSine (buffer, 1, sampleRate, 440.0, 0.25f, 0);
        juce::AudioBuffer<float> expected (buffer);

        juce::MidiBuffer midi;
        processor.processBlock (buffer, midi);

        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < blockSize; ++n)
                assert (buffer.getSample (ch, n) == expected.getSample (ch, n) * 2.0f);

        // f = -1: the mid is silenced; the side (zero here) doubles to still zero.
        setParameter (processor.apvts, Processor::midFocusId, -100.0f);
        buffer.clear();
        addSine (buffer, 0, sampleRate, 440.0, 0.25f, 0);
        addSine (buffer, 1, sampleRate, 440.0, 0.25f, 0);
        processor.processBlock (buffer, midi);
        assert (peak (buffer) < 1.0e-4f);
        pass ("testMidFocusGainLaw");
    }

    /** Plan 6.1: the side filter shapes the side component only, and Off is a true bypass. */
    void testSideFilter()
    {
        Processor on;
        on.prepareToPlay (sampleRate, blockSize);
        setParameter (on.apvts, Processor::driveId, 6.0f);
        setParameter (on.apvts, Processor::sideFilterId, 1.0f);     // high pass
        setParameter (on.apvts, Processor::sideCrossoverId, 5000.0f);

        // Off with the same crossover is bit-identical to the drive-only path.
        Processor off;
        off.prepareToPlay (sampleRate, blockSize);
        setParameter (off.apvts, Processor::driveId, 6.0f);
        setParameter (off.apvts, Processor::sideCrossoverId, 5000.0f);

        juce::AudioBuffer<float> onBuffer (2, blockSize), offBuffer (2, blockSize);
        fillStereoSignal (onBuffer);
        offBuffer = onBuffer;

        juce::MidiBuffer midi;
        on.processBlock (onBuffer, midi);
        off.processBlock (offBuffer, midi);

        // A pure-side, low-frequency input is strongly attenuated by the high pass...
        Processor sideIn;
        sideIn.prepareToPlay (sampleRate, blockSize);
        setParameter (sideIn.apvts, Processor::sideFilterId, 1.0f);
        setParameter (sideIn.apvts, Processor::sideCrossoverId, 5000.0f);

        juce::AudioBuffer<float> filtered (2, blockSize);
        filtered.clear();
        addSine (filtered, 0, sampleRate, 100.0, 0.5f, 0);
        addSine (filtered, 1, sampleRate, 100.0, -0.5f, 0);   // anti-phase: pure side
        sideIn.processBlock (filtered, midi);

        // ...while a mid-only signal passes the same filter untouched.
        juce::AudioBuffer<float> midOnly (2, blockSize);
        midOnly.clear();
        addSine (midOnly, 0, sampleRate, 100.0, 0.25f, 0);
        addSine (midOnly, 1, sampleRate, 100.0, 0.25f, 0);
        sideIn.processBlock (midOnly, midi);

        // The filtered side peak is far below the unfiltered level, and the mid survived.
        assert (peak (filtered) < 0.1f);
        assert (peak (midOnly) > 0.2f);

        // The Off path with the same drive is exactly the tube drive path (bit for bit).
        Processor driveOnly;
        driveOnly.prepareToPlay (sampleRate, blockSize);
        setParameter (driveOnly.apvts, Processor::driveId, 6.0f);

        juce::AudioBuffer<float> driveBuffer (2, blockSize);
        fillStereoSignal (driveBuffer);
        driveOnly.processBlock (driveBuffer, midi);

        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < blockSize; ++n)
                assert (offBuffer.getSample (ch, n) == driveBuffer.getSample (ch, n));

        // And the filtered buffer differs from the unfiltered one.
        assert (buffersDiffer (onBuffer, offBuffer));
        pass ("testSideFilter");
    }

    /** Plan 6.1: the transient follower moves its target on transients, and Amount 0 is
        exactly the static path. */
    void testTransientFollower()
    {
        // Amount 0: identical to the same settings without a follower, whatever the times.
        Processor neutral, follower;
        neutral.prepareToPlay (sampleRate, blockSize);
        follower.prepareToPlay (sampleRate, blockSize);
        setParameter (neutral.apvts, Processor::driveId, 3.0f);
        setParameter (follower.apvts, Processor::driveId, 3.0f);
        setParameter (follower.apvts, Processor::transientAttackId, 10.0f);
        setParameter (follower.apvts, Processor::transientReleaseId, 400.0f);
        setParameter (follower.apvts, Processor::transientAmountId, 0.0f);   // still bypassed

        juce::AudioBuffer<float> neutralBuffer (2, blockSize), followerBuffer (2, blockSize);
        fillStereoSignal (neutralBuffer);
        followerBuffer = neutralBuffer;

        juce::MidiBuffer midi;
        neutral.processBlock (neutralBuffer, midi);
        follower.processBlock (followerBuffer, midi);

        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < blockSize; ++n)
                assert (followerBuffer.getSample (ch, n) == neutralBuffer.getSample (ch, n));

        // Amount 60 on the drive: the transient pushes the drive up, so the output changes.
        Processor pumping;
        pumping.prepareToPlay (sampleRate, blockSize);
        setParameter (pumping.apvts, Processor::driveId, 3.0f);
        setParameter (pumping.apvts, Processor::transientAmountId, 60.0f);
        setParameter (pumping.apvts, Processor::transientTargetId, 1.0f);    // drive

        juce::AudioBuffer<float> pumpingBuffer (2, blockSize);
        fillStereoSignal (pumpingBuffer);
        pumping.processBlock (pumpingBuffer, midi);
        assert (buffersDiffer (pumpingBuffer, neutralBuffer));
        assert (peak (pumpingBuffer) > peak (neutralBuffer));   // more drive, more level

        // Width target: a width-envelope raises the side energy on transients.
        Processor widening;
        widening.prepareToPlay (sampleRate, blockSize);
        setParameter (widening.apvts, Processor::transientAmountId, 60.0f);
        setParameter (widening.apvts, Processor::transientTargetId, 0.0f);   // width

        juce::AudioBuffer<float> wideningBuffer (2, blockSize);
        fillStereoSignal (wideningBuffer);
        widening.processBlock (wideningBuffer, midi);
        assert (buffersDiffer (wideningBuffer, neutralBuffer));
        assert (allFinite (wideningBuffer));
        pass ("testTransientFollower");
    }

    void testEditor()
    {
        Processor processor;
        checkEditor (processor);
        pass ("testEditor");
    }

    /** R13: the header carries the preset bar and the gear menu, the gear's two actions are
        wired, the gear sits at the far right of the header with the bar left of it, and both
        dialogs build and paint. */
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

        assert (gear->getRight() > editor->getWidth() - 40);
        assert (bar->getRight() <= gear->getX());

        {
            SuiteUI::AboutPanel about ({}, "Stereo Tool Effect", "1.0.0", "Ster", "StereoTool");
            const auto image = paintComponent (about);
            assert (image.getPixelAt (8, 8) == SuiteUI::Colours::panel);
        }

        {
            const auto file = juce::File::getCurrentWorkingDirectory()
                                  .getChildFile ("StereoToolHeaderContractSettings.xml");
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
        const juce::File file (juce::File (folder).getChildFile ("stereotool-default.png"));
        assert (PluginTest::writeDeterministicPng (*editor, file));
        processor.editorBeingDeleted (editor.get());

        pass ("testRenderBaseline");
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::puts ("Running Stereo Tool Effect plugin tests...");

    testParameters();
    testStateRoundTrip();
    testNewerStateRefused();
    testAdditiveStateLoad();
    testTransparentAtDefaults();
    testInactiveControlsStayNeutral();
    testWidthControlsStereoImage();
    testDriveSaturatesAndStaysBounded();
    testCharactersDiffer();
    testMidFocusGainLaw();
    testSideFilter();
    testTransientFollower();
    testEditor();
    testHeaderContract();
    testRenderBaseline();

    std::puts ("All Stereo Tool Effect plugin tests passed.");
    return 0;
}
