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

    void testRegionMapping()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        // Set full span: key 0 starts at 0, key 127 ends at 1.0
        setParameter (processor.apvts, Processor::regionSpanId, 100.0f);
        setParameter (processor.apvts, Processor::regionLengthId, 25.0f);

        juce::MidiBuffer midiOn;
        midiOn.addEvent (juce::MidiMessage::noteOn (1, 0, 0.8f), 0);

        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();
        processor.processBlock (buffer, midiOn);

        // Key 0 should start at region start 0
        const auto& voice0 = processor.voicePool.voices[0];
        assert (voice0.active);
        assert (std::abs (voice0.regionStart - 0.0) < 1.0);
        assert (voice0.regionEnd > voice0.regionStart);

        // Key 127 should end at region end near sample length
        juce::MidiBuffer midiOn2;
        midiOn2.addEvent (juce::MidiMessage::noteOn (1, 127, 0.8f), 0);
        processor.processBlock (buffer, midiOn2);

        // Find the voice for key 127
        bool found = false;
        for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
        {
            if (processor.voicePool.voices[i].active && processor.voicePool.voices[i].key == 127)
            {
                // With 25% length starting at position ~0.99, end should be > start
                assert (processor.voicePool.voices[i].regionEnd > processor.voicePool.voices[i].regionStart);
                found = true;
                break;
            }
        }
        assert (found);
        pass ("testRegionMapping");
    }

    void testVoiceProducesOutput()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        // Create a simple test buffer (sine wave)
        juce::AudioBuffer<float> testBuffer (2, 48000);
        for (int ch = 0; ch < 2; ++ch)
        {
            float* dst = testBuffer.getWritePointer (ch);
            for (int i = 0; i < 48000; ++i)
                dst[i] = 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f * i / 48000.0f);
        }

        // Load the test buffer directly into the processor
        processor.loadedSample = std::make_unique<LoadedSample>();
        processor.loadedSample->buffer = testBuffer;
        processor.loadedSample->sourceSampleRate = sampleRate;
        processor.sampleBuffer.store (&processor.loadedSample->buffer, std::memory_order_release);

        juce::MidiBuffer midiOn;
        midiOn.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);

        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();
        processor.processBlock (buffer, midiOn);

        // Voice should have advanced its position
        bool voiceAdvanced = false;
        for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
        {
            if (processor.voicePool.voices[i].active && processor.voicePool.voices[i].key == 60)
            {
                voiceAdvanced = processor.voicePool.voices[i].position > 0.0;
                break;
            }
        }
        assert (voiceAdvanced);
        pass ("testVoiceProducesOutput");
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
    testRegionMapping();
    testVoiceProducesOutput();
    testEditor();

    std::puts ("All Sgraffito plugin tests passed.");
    return 0;
}
