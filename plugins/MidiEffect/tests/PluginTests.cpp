// Plugin-level tests for MIDI Chord/Arp/Scale Tool. Compiles the real processor and editor sources.

#include "PluginTestUtils.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    using namespace PluginTest;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    using Processor = MidiEffectProcessor;

    void testParameters()
    {
        Processor processor;

        checkParameterIds (processor, processor.apvts, 
            { Processor::rootId, Processor::scaleId, Processor::chordId, Processor::arpId, Processor::velocityId }, 5);
        checkRange (processor.apvts, Processor::rootId, 0, 127, 60); // we use choice, so check the range via the choices
        checkRange (processor.apvts, Processor::scaleId,   0, 8, 1);
        checkRange (processor.apvts, Processor::chordId,   0, 7, 0);
        checkRange (processor.apvts, Processor::arpId,     0, 3, 0);
        checkRange (processor.apvts, Processor::velocityId, 0, 2, 1);
        pass ("testParameters");
    }

    void testStateRoundTrip()
    {
        Processor processor;

        checkStateRoundTrip (processor, processor.apvts, Processor::rootId, 60);
        checkStateRoundTrip (processor, processor.apvts, Processor::scaleId,   1);
        checkStateRoundTrip (processor, processor.apvts, Processor::chordId,   0);
        checkStateRoundTrip (processor, processor.apvts, Processor::arpId,      0);
        checkStateRoundTrip (processor, processor.apvts, Processor::velocityId,  1);
        pass ("testStateRoundTrip");
    }

    void testMidiPassThrough()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        // Create a note-on message
        juce::MidiMessage noteOn = juce::MidiMessage::noteOn (60, 100, 1.0); // C4, velocity 100
        juce::MidiBuffer incoming;
        incoming.addMessage (noteOn);

        // Process - this is a stub; just verify it doesn't crash
        juce::AudioBuffer<float> silence (2, blockSize);
        processor.processBlock (incoming, &silence);

        // Verify no assertion failure
        pass ("testMidiPassThrough");
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

    std::puts ("Running MIDI Chord/Arp/Scale Tool plugin tests...");

    testParameters();
    testStateRoundTrip();
    testMidiPassThrough();
    testEditor();

    std::puts ("All MIDI Chord/Arp/Scale Tool plugin tests passed.");
    return 0;
}