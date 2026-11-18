// Plugin-level tests for Stereo Tool Effect. Compiles the real processor and editor sources.

#include "PluginTestUtils.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

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
            { Processor::widthId, Processor::driveId, Processor::mixId }, 3);
        checkRange (processor.apvts, Processor::widthId, -100.0f, 100.0f, 0.0f);
        checkRange (processor.apvts, Processor::driveId, -24.0f, 24.0f, 0.0f);
        checkRange (processor.apvts, Processor::mixId,   0.0f, 100.0f, 100.0f);
        pass ("testParameters");
    }

    void testStateRoundTrip()
    {
        Processor processor;

        checkStateRoundTrip (processor, processor.apvts, Processor::widthId, 40.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::driveId, 6.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::mixId,   50.0f);
        pass ("testStateRoundTrip");
    }

    /** The key regression: at the defaults (width 0, drive 0) the plugin is bit-transparent,
        whatever the mix, because the wet path equals the dry input exactly. */
    void testTransparentAtDefaults()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();
        addSine (buffer, 0, sampleRate, 440.0, 0.5f, 0);
        addSine (buffer, 1, sampleRate, 880.0, 0.25f, 0);   // different per channel: real stereo

        juce::AudioBuffer<float> expected (buffer);

        juce::MidiBuffer midi;
        processor.processBlock (buffer, midi);

        assert (allFinite (buffer));
        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < blockSize; ++n)
                assert (buffer.getSample (ch, n) == expected.getSample (ch, n));
        pass ("testTransparentAtDefaults");
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

    void testEditor()
    {
        Processor processor;
        checkEditor (processor);
        pass ("testEditor");
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::puts ("Running Stereo Tool Effect plugin tests...");

    testParameters();
    testStateRoundTrip();
    testTransparentAtDefaults();
    testWidthControlsStereoImage();
    testDriveSaturatesAndStaysBounded();
    testEditor();

    std::puts ("All Stereo Tool Effect plugin tests passed.");
    return 0;
}
