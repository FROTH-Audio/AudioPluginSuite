// Plugin-level tests for Sgraffito. Compiles the real processor and editor sources.

#include "PluginTestUtils.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    using namespace PluginTest;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    using Processor = SgraffitoProcessor;

    void testParameters()
    {
        Processor processor;

        // 33 parameters total (as defined in numControls).
        checkParameterIds (processor, processor.apvts,
                           { Processor::modeId, Processor::levelId, Processor::driftAmountId },
                           Processor::numControls);
        checkRange (processor.apvts, Processor::modeId, 0.0f, 3.0f, 0.0f);
        checkRange (processor.apvts, Processor::levelId, -60.0f, 6.0f, 0.0f);
        checkRange (processor.apvts, Processor::driftAmountId, 0.0f, 100.0f, 0.0f);
        checkRange (processor.apvts, Processor::memoryAmountId, 0.0f, 100.0f, 20.0f);
        pass ("testParameters");
    }

    void testStateRoundTrip()
    {
        Processor processor;

        checkStateRoundTrip (processor, processor.apvts, Processor::levelId, 6.0f);
        pass ("testStateRoundTrip");
    }

    void testMidiNoteOnAllocatesVoice()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);

        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();
        processor.processBlock (buffer, midi);

        assert (processor.getNumActiveVoices() > 0);
        pass ("testMidiNoteOnAllocatesVoice");
    }

    void testMidiNoteOffReleasesVoice()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::MidiBuffer midiOn;
        midiOn.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);

        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();
        processor.processBlock (buffer, midiOn);
        assert (processor.getNumActiveVoices() > 0);

        juce::MidiBuffer midiOff;
        midiOff.addEvent (juce::MidiMessage::noteOff (1, 60, 0.8f), blockSize);
        processor.processBlock (buffer, midiOff);
        assert (processor.getNumActiveVoices() == 0);
        pass ("testMidiNoteOffReleasesVoice");
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

    std::puts ("Running Sgraffito plugin tests...");

    testParameters();
    testStateRoundTrip();
    testMidiNoteOnAllocatesVoice();
    testMidiNoteOffReleasesVoice();
    testEditor();

    std::puts ("All Sgraffito plugin tests passed.");
    return 0;
}
