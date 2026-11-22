// Plugin-level tests for Stereo Tool Effect. Compiles the real processor and editor sources.

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

    using Processor = StereoToolProcessor;

    void testParameters()
    {
        Processor processor;

        checkParameterIds (processor, processor.apvts,
            { Processor::widthId, Processor::driveId, Processor::mixId,
              Processor::characterId, Processor::transientAttackId, Processor::transientReleaseId,
              Processor::transientAmountId, Processor::transientTargetId, Processor::midFocusId,
              Processor::sideFilterId, Processor::sideCrossoverId,
              Processor::inputLevelId, Processor::outputLevelId, Processor::panId,
              Processor::lfo1RateId, Processor::lfo1ShapeId, Processor::lfo1SyncId, Processor::lfo1DivisionId, Processor::lfo1OffsetId,
              Processor::lfo2RateId, Processor::lfo2ShapeId, Processor::lfo2SyncId, Processor::lfo2DivisionId, Processor::lfo2OffsetId,
              Processor::modSourceIds[0], Processor::modTargetIds[0], Processor::modAmountIds[0], Processor::modDelayIds[0],
              Processor::modSourceIds[5], Processor::modTargetIds[5], Processor::modAmountIds[5], Processor::modDelayIds[5] },
            Processor::numControls);
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
        checkRange (processor.apvts, Processor::inputLevelId,      -24.0f, 24.0f, 0.0f);
        checkRange (processor.apvts, Processor::outputLevelId,     -24.0f, 24.0f, 0.0f);
        checkRange (processor.apvts, Processor::panId,             -100.0f, 100.0f, 0.0f);

        // Plan 12: the final gain stage is named "Output" everywhere in the suite.
        assert (processor.apvts.getParameter (Processor::outputLevelId)->getName (100) == "Output");

        // Plan 13: the routing table has 10 targets.
        assert (processor.getRouter().getNumTargets() == 10);
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
        checkStateRoundTrip (processor, processor.apvts, Processor::inputLevelId, -6.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::outputLevelId, 3.5f);

        // Plan 13: the routing parameters round-trip too.
        checkStateRoundTrip (processor, processor.apvts, Processor::lfo1RateId, 3.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::modAmountIds[2], 40.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::modTargetIds[3], 5.0f);
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

    /** Plan 6.1 and plan 12: a state saved before the new controls loads with every new
        parameter at its default. The changes are additive, so the version stays 1. */
    void testAdditiveStateLoad()
    {
        Processor processor;
        setParameter (processor.apvts, Processor::characterId, 3.0f);
        setParameter (processor.apvts, Processor::midFocusId, 40.0f);
        setParameter (processor.apvts, Processor::sideCrossoverId, 3000.0f);
        setParameter (processor.apvts, Processor::inputLevelId, -6.0f);
        setParameter (processor.apvts, Processor::outputLevelId, 6.0f);

        juce::MemoryBlock state;
        processor.getStateInformation (state);

        // Strip the appended parameters from the state tree, like an old session.
        auto xml = SuiteState::detail::decodeXml (state.getData(), (int) state.getSize());
        assert (xml != nullptr);
        auto tree = juce::ValueTree::fromXml (*xml);

        for (const char* id : { Processor::characterId, Processor::transientAttackId,
                                Processor::transientReleaseId, Processor::transientAmountId,
                                Processor::transientTargetId, Processor::midFocusId,
                                Processor::sideFilterId, Processor::sideCrossoverId,
                                Processor::inputLevelId, Processor::outputLevelId })
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
        assert (std::abs (getParameter (processor.apvts, Processor::inputLevelId) - 0.0f) < 1.0e-3f);
        assert (std::abs (getParameter (processor.apvts, Processor::outputLevelId) - 0.0f) < 1.0e-3f);
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

        // Mix 0 passes the untouched dry input through. The 20 ms mix ramp (plan 13) needs a
        // couple of blocks to settle before the comparison is exact.
        setParameter (processor.apvts, Processor::mixId, 0.0f);
        for (int settle = 0; settle < 4; ++settle)
        {
            buffer.clear();
            addSine (buffer, 0, sampleRate, 220.0, 0.9f, 0);
            addSine (buffer, 1, sampleRate, 220.0, 0.9f, 0);
            processor.processBlock (buffer, midi);
        }
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

    /** Plan 15 (owner): the controls that do not apply right now are dimmed. Side Crossover
        while the Side Filter is Off; the transient follower's Attack, Release and Target
        while Amount is 0 (plan 10 G1: dimmed, never hidden). */
    void testModeAffinity()
    {
        Processor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        assert (editor != nullptr);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (120);   // one timer tick

        auto* stereoEditor = dynamic_cast<StereoToolEditor*> (editor.get());
        assert (stereoEditor != nullptr);

        // Defaults: Side Filter Off, Amount 0 — the crossover and the follower cells are dimmed.
        assert (! stereoEditor->isCrossoverEnabled());
        assert (! stereoEditor->isTransientRowEnabled());

        // Filter on, amount up: both live.
        setParameter (processor.apvts, Processor::sideFilterId, 1.0f);
        setParameter (processor.apvts, Processor::transientAmountId, 60.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
        assert (stereoEditor->isCrossoverEnabled());
        assert (stereoEditor->isTransientRowEnabled());

        // Amount back to 0: the follower cells dim again, the crossover stays live.
        setParameter (processor.apvts, Processor::transientAmountId, 0.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
        assert (stereoEditor->isCrossoverEnabled());
        assert (! stereoEditor->isTransientRowEnabled());

        // Filter off again: the crossover dims, the follower cells stay dimmed.
        setParameter (processor.apvts, Processor::sideFilterId, 0.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
        assert (! stereoEditor->isCrossoverEnabled());
        assert (! stereoEditor->isTransientRowEnabled());

        processor.editorBeingDeleted (editor.get());
        pass ("testModeAffinity");
    }

    /** Plan 13: LEVEL shares a row with IMAGE and CHARACTER spans a full row; plan 15 moved
        the preview to the top of the window, removed the stereo meter pair and made the
        default height the minimum. The long choice lists are dropdowns, and the help line
        answers the owner's report. */
    void testLevelGroupAndHelp()
    {
        Processor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        assert (editor != nullptr);
        assert (editor->getWidth() == 720 && editor->getHeight() == 658);

        // A slider whose caption reads INPUT, in the second row on the left (LEVEL).
        juce::Component* inputSlider = nullptr;
        std::function<juce::Component* (juce::Component&)> findByCaption = [&] (juce::Component& root)
            -> juce::Component*
        {
            if (auto* cell = dynamic_cast<SuiteUI::LabelledSlider*> (&root))
                if (cell->label.getText() == "INPUT")
                    return cell;
            for (auto* child : root.getChildren())
                if (auto* found = findByCaption (*child))
                    return found;
            return nullptr;
        };
        inputSlider = findByCaption (*editor);
        assert (inputSlider != nullptr);
        assert (inputSlider->getRight() < editor->getWidth() / 2);   // left half: the LEVEL group
        assert (inputSlider->getY() > 200);                         // below the preview row

        PluginTest::checkHelpLine (*editor, "stereotool-help", *inputSlider);

        // Owner finding: the long lists (character, side filter) are dropdowns, the short
        // target list stays a pill.
        int dropdowns = 0, selectors = 0;
        std::function<void (juce::Component&)> countChoices = [&] (juce::Component& root)
        {
            if (auto* cell = dynamic_cast<SuiteUI::LabelledDropdown*> (&root))
            {
                ++dropdowns;
                assert (cell->getWidth() > 0 && cell->getHeight() > 0);
            }
            if (dynamic_cast<SuiteUI::LabelledSelector*> (&root) != nullptr)
                ++selectors;
            for (auto* child : root.getChildren())
                countChoices (*child);
        };
        countChoices (*editor);
        assert (dropdowns == 2);   // CHARACTER, SIDE FILTER
        assert (selectors == 1);   // TARGET

        // Plan 15 (owner): the stereo meter pair is gone from this plugin, and the preview
        // sits at the top of the window, above the LEVEL and IMAGE row.
        std::function<void (juce::Component&)> findMeters = [&] (juce::Component& root)
        {
            assert (dynamic_cast<SuiteUI::StereoLevelMeter*> (&root) == nullptr);
            for (auto* child : root.getChildren())
                findMeters (*child);
        };
        findMeters (*editor);

        auto* visualizer = PluginTest::findComponentById (*editor, "stereotool-preview");
        assert (visualizer != nullptr);
        assert (visualizer->getY() < editor->getHeight() / 2);        // top of the window
        assert (visualizer->getBottom() < editor->getHeight() - 26);  // clear of the footer

        processor.editorBeingDeleted (editor.get());
        pass ("testLevelGroupAndHelp");
    }

    /** Plan 12: the level stage scales the output without touching the character. */
    void testLevelStageScalesThePeak()
    {
        const auto run = [&] (float inputDb, float outputDb)
        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            setParameter (processor.apvts, Processor::inputLevelId, inputDb);
            setParameter (processor.apvts, Processor::outputLevelId, outputDb);

            // Let the 20 ms smoothing settle, regenerating the signal every block so the
            // blocks are independent (the plugin is not a shaper of its own output here).
            for (int i = 0; i < 10; ++i)
            {
                juce::MidiBuffer midi;
                juce::AudioBuffer<float> audio (2, blockSize);
                audio.clear();
                addSine (audio, 0, sampleRate, 440.0, 0.25f, 0);
                addSine (audio, 1, sampleRate, 440.0, 0.25f, 0);
                processor.processBlock (audio, midi);
            }

            juce::MidiBuffer midi;
            juce::AudioBuffer<float> audio (2, blockSize);
            audio.clear();
            addSine (audio, 0, sampleRate, 440.0, 0.25f, 0);
            addSine (audio, 1, sampleRate, 440.0, 0.25f, 0);
            processor.processBlock (audio, midi);
            return peak (audio);
        };

        const float unity = run (0.0f, 0.0f);
        assert (std::abs (unity - 0.25f) < 0.02f);   // 0 dB is exactly unity

        assert (std::abs (run (-6.0f, 0.0f) - unity * 0.5012f) < 0.02f);
        assert (std::abs (run (0.0f, -6.0f) - unity * 0.5012f) < 0.02f);
        assert (std::abs (run (6.0f, 0.0f) - unity * 1.9953f) < 0.03f);
        assert (std::abs (run (0.0f, 6.0f) - unity * 1.9953f) < 0.03f);

        // The stage does not colour the tone: -6 dB on both ends is the same as one -12 dB.
        assert (std::abs (run (-6.0f, -6.0f) - run (-12.0f, 0.0f)) < 0.02f);

        pass ("testLevelStageScalesThePeak");
    }

    /** Plan 17 Phase 4: the pan stage. Centre is bit-transparent; pan right attenuates the
        left channel (a balance law), pan left attenuates the right; the kept channel stays
        at unity. */
    void testPanStage()
    {
        const auto run = [&] (float pan)
        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            setParameter (processor.apvts, Processor::panId, pan);

            // Let the 20 ms smoothing settle, regenerating the signal every block.
            for (int i = 0; i < 10; ++i)
            {
                juce::MidiBuffer midi;
                juce::AudioBuffer<float> audio (2, blockSize);
                audio.clear();
                addSine (audio, 0, sampleRate, 440.0, 0.25f, 0);
                addSine (audio, 1, sampleRate, 440.0, 0.25f, 0);
                processor.processBlock (audio, midi);
            }

            juce::MidiBuffer midi;
            juce::AudioBuffer<float> audio (2, blockSize);
            audio.clear();
            addSine (audio, 0, sampleRate, 440.0, 0.25f, 0);
            addSine (audio, 1, sampleRate, 440.0, 0.25f, 0);
            processor.processBlock (audio, midi);
            return std::make_pair ((double) audio.getMagnitude (0, 0, blockSize),
                                   (double) audio.getMagnitude (1, 0, blockSize));
        };

        // Centre: bit-transparent (the identity path skips the pan stage entirely).
        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);

            juce::MidiBuffer midi;
            juce::AudioBuffer<float> in (2, blockSize);
            in.clear();
            addSine (in, 0, sampleRate, 440.0, 0.25f, 0);
            addSine (in, 1, sampleRate, 440.0, 0.25f, 0);
            processor.processBlock (in, midi);

            juce::AudioBuffer<float> reference (2, blockSize);
            reference.makeCopyOf (in);

            in.clear();
            addSine (in, 0, sampleRate, 440.0, 0.25f, 0);
            addSine (in, 1, sampleRate, 440.0, 0.25f, 0);
            processor.processBlock (in, midi);

            for (int n = 0; n < blockSize; ++n)
            {
                assert (in.getSample (0, n) == reference.getSample (0, n));
                assert (in.getSample (1, n) == reference.getSample (1, n));
            }
        }

        // Pan right: the left channel attenuates (cos of the pan angle), the right stays.
        const auto rightPair = run (100.0f);
        const auto centrePair = run (0.0f);
        assert (std::abs (rightPair.first - centrePair.first * 0.7071f) < 0.01f);
        assert (std::abs (rightPair.second - centrePair.second) < 0.005f);

        // Pan left: mirrored.
        const auto leftPair = run (-100.0f);
        assert (std::abs (leftPair.first - centrePair.first) < 0.005f);
        assert (std::abs (leftPair.second - centrePair.second * 0.7071f) < 0.01f);

        pass ("testPanStage");
    }

    /** Plan 11 D7: the meter snapshot describes the signal — mid carries the correlated
        sine, side rises only off-centre, the follower moves with amount 1, and everything
        stays at zero when idle. */
    void testVisualSnapshot()
    {
        // Idle at the defaults: the identity path still publishes the untouched input.
        Processor idle;
        idle.prepareToPlay (sampleRate, blockSize);
        {
            juce::MidiBuffer midi;
            juce::AudioBuffer<float> silence (2, blockSize);
            silence.clear();
            idle.processBlock (silence, midi);
            assert (idle.getMeterSnapshot().midLevel.load() < 1.0e-6f);
            assert (idle.getMeterSnapshot().followerEnvelope.load() == 0.0f);
        }

        // A centred sine: everything lands in the mid channel.
        Processor mono;
        mono.prepareToPlay (sampleRate, blockSize);
        {
            juce::MidiBuffer midi;
            juce::AudioBuffer<float> audio (2, blockSize);
            audio.clear();
            addSine (audio, 0, sampleRate, 440.0, 0.5f, 0);
            addSine (audio, 1, sampleRate, 440.0, 0.5f, 0);
            mono.processBlock (audio, midi);

            assert (mono.getMeterSnapshot().midLevel.load() > 0.1f);
            assert (mono.getMeterSnapshot().sideLevel.load() < 0.01f);
        }

        // An off-centre sine (L only): the side channel carries it.
        Processor wide;
        wide.prepareToPlay (sampleRate, blockSize);
        {
            juce::MidiBuffer midi;
            juce::AudioBuffer<float> audio (2, blockSize);
            audio.clear();
            addSine (audio, 0, sampleRate, 440.0, 0.5f, 0);
            wide.processBlock (audio, midi);

            assert (wide.getMeterSnapshot().sideLevel.load() > 0.1f);
        }

        // The follower moves at amount 1 on a transient signal.
        Processor transient;
        transient.prepareToPlay (sampleRate, blockSize);
        setParameter (transient.apvts, Processor::transientAmountId, 100.0f);
        {
            juce::MidiBuffer midi;
            juce::AudioBuffer<float> audio (2, blockSize);
            audio.clear();
            addSine (audio, 0, sampleRate, 440.0, 0.9f, 0);
            addSine (audio, 1, sampleRate, 440.0, 0.9f, 0);
            transient.processBlock (audio, midi);

            assert (transient.getMeterSnapshot().followerEnvelope.load() > 0.01f);
        }

        pass ("testVisualSnapshot");
    }

    /** Plan 13: a routed parameter changes the audio; with amount 0 the output is unchanged. */
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

        const auto diff = [] (const std::vector<float>& a, const std::vector<float>& b)
        {
            assert (a.size() == b.size());
            double sum = 0.0;
            for (size_t n = 0; n < a.size(); ++n)
                sum += std::abs (a[(size_t) n] - b[(size_t) n]);
            return sum / (double) a.size();
        };

        Processor plain;
        const auto plainOutput = render (plain);

        // Output routed: the final gain sweeps with the square LFO.
        Processor modulatedOutput;
        setLfo (modulatedOutput, 0, 4.0f, 3.0f, false, 4.0f);   // square, 4 Hz, free
        setRoute (modulatedOutput, 0, 0, (int) Processor::ModTarget::Output, 50.0f);
        assert (diff (plainOutput, render (modulatedOutput)) > 1.0e-3);

        // Drive routed: the shaper opens and closes.
        Processor modulatedDrive;
        setLfo (modulatedDrive, 0, 4.0f, 3.0f, false, 4.0f);
        setRoute (modulatedDrive, 0, 0, (int) Processor::ModTarget::Drive, 100.0f);
        assert (diff (plainOutput, render (modulatedDrive)) > 1.0e-3);

        // Amount 0 is bit-identical, even with the LFO running.
        Processor idle;
        setLfo (idle, 0, 4.0f, 3.0f, false, 4.0f);
        setRoute (idle, 0, 0, (int) Processor::ModTarget::Output, 0.0f);
        assert (render (idle) == plainOutput);

        pass ("testModulationReachesAudio");
    }

    /** Plan 13: modulating the Side Crossover is inert while the side filter is Off. */
    void testSideCrossoverInertWhenFilterOff()
    {
        auto render = [] (Processor& processor)
        {
            processor.prepareToPlay (sampleRate, blockSize);
            std::vector<float> out;
            int64_t first = 0;
            juce::MidiBuffer midi;

            for (int block = 0; block < 16; ++block)
            {
                juce::AudioBuffer<float> audio (2, blockSize);
                audio.clear();
                addSine (audio, 0, sampleRate, 220.0, 0.5f, first);
                addSine (audio, 1, sampleRate, 220.0, 0.25f, first);
                processor.processBlock (audio, midi);

                const auto* left = audio.getReadPointer (0);
                out.insert (out.end(), left, left + blockSize);
                first += blockSize;
            }

            return out;
        };

        Processor plain;
        const auto plainOutput = render (plain);

        // The routed crossover with the filter Off: identical, because no filter node runs.
        Processor routed;
        setLfo (routed, 0, 6.0f, 0.0f, false, 4.0f);
        setRoute (routed, 0, 0, (int) Processor::ModTarget::SideCrossover, 100.0f);
        assert (render (routed) == plainOutput);

        // The same routing with the filter on: different, so the target really is wired.
        setParameter (routed.apvts, Processor::sideFilterId, 2.0f);   // Low Pass
        const auto filteredOutput = render (routed);

        double difference = 0.0;
        for (size_t n = 0; n < plainOutput.size(); ++n)
            difference += std::abs (plainOutput[(size_t) n] - filteredOutput[(size_t) n]);
        assert (difference / (double) plainOutput.size() > 1.0e-3);

        pass ("testSideCrossoverInertWhenFilterOff");
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
        auto* editor = dynamic_cast<StereoToolEditor*> (processor.createEditorAndMakeActive());
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

        auto* stereoEditor = dynamic_cast<StereoToolEditor*> (editor.get());
        assert (stereoEditor != nullptr);
        stereoEditor->setPage (1);

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
        auto* stereoEditor = dynamic_cast<StereoToolEditor*> (editor.get());
        assert (stereoEditor != nullptr);

        const juce::File file (juce::File (folder).getChildFile ("stereotool-default.png"));
        assert (PluginTest::writeDeterministicPng (*editor, file));

        // Plan 13: the MOD page renders too.
        stereoEditor->setPage (1);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
        const juce::File modFile (juce::File (folder).getChildFile ("stereotool-mod.png"));
        assert (PluginTest::writeDeterministicPng (*editor, modFile));

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
    testModeAffinity();
    testLevelGroupAndHelp();
    testLevelStageScalesThePeak();
    testPanStage();
    testVisualSnapshot();
    testModulationReachesAudio();
    testSideCrossoverInertWhenFilterOff();
    testSyncedLfoFollowsTempo();
    testPageSwitchChangesNoParameter();
    testModPagePaints();
    testHeaderContract();
    testRenderBaseline();

    std::puts ("All Stereo Tool Effect plugin tests passed.");
    return 0;
}
