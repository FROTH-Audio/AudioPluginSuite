// Plugin-level tests for __PRODUCT__. Compiles the real processor and editor sources.

#include "PluginTestUtils.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

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
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::puts ("Running __PRODUCT__ plugin tests...");

    testParameters();
    testStateRoundTrip();
    testGainScalesSignal();
    testEditor();

    std::puts ("All __PRODUCT__ plugin tests passed.");
    return 0;
}
