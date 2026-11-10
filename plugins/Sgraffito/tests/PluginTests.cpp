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

        // Output should be non-zero (voice is playing)
        float peak = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int i = 0; i < blockSize; ++i)
                peak = std::max (peak, std::abs (buffer.getSample (ch, i)));
        }

        assert (peak > 0.01f);
        pass ("testVoiceProducesOutput");
    }

    void testChopModeRegionMapping()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        // Set Chop mode (mode 1) with 4 slices
        setParameter (processor.apvts, Processor::modeId, 1.0f);
        setParameter (processor.apvts, Processor::sliceCountId, 4.0f);
        setParameter (processor.apvts, Processor::regionSpanId, 100.0f);

        // Key 0 should map to slice 0 (first quarter)
        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();

        juce::MidiBuffer midi0;
        midi0.addEvent (juce::MidiMessage::noteOn (1, 0, 0.8f), 0);
        processor.processBlock (buffer, midi0);
        
        // Find voice for key 0
        bool found0 = false;
        for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
        {
            if (processor.voicePool.voices[i].active && processor.voicePool.voices[i].key == 0)
            {
                // Slice 0 should start at 0.0
                assert (std::abs (processor.voicePool.voices[i].regionStart - 0.0) < 1e-6);
                // Slice 0 should end at 0.25 of sample length
                assert (std::abs (processor.voicePool.voices[i].regionEnd - 0.25) < 1e-6);
                found0 = true;
                break;
            }
        }
        assert (found0);
        pass ("testChopModeRegionMapping");
    }

    void testChopModeSliceCoverage()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        // Set Chop mode with 8 slices
        setParameter (processor.apvts, Processor::modeId, 1.0f);
        setParameter (processor.apvts, Processor::sliceCountId, 8.0f);
        setParameter (processor.apvts, Processor::regionSpanId, 100.0f);

        // Each key should map to a different slice
        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();

        double prevRegionStart = -1.0;
        bool slicesCovered = true;

        for (int key = 0; key < 128; key += 16)
        {
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, key, 0.8f), 0);
            processor.processBlock (buffer, midi);

            bool foundKey = false;
            for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
            {
                if (processor.voicePool.voices[i].active && processor.voicePool.voices[i].key == key)
                {
                    foundKey = true;
                    if (processor.voicePool.voices[i].regionStart < prevRegionStart - 1e-6)
                    {
                        slicesCovered = false;
                    }
                    prevRegionStart = processor.voicePool.voices[i].regionStart;
                    break;
                }
            }
            assert (foundKey);
        }
        assert (slicesCovered);
        pass ("testChopModeSliceCoverage");
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
    testChopModeRegionMapping();
    testChopModeSliceCoverage();
    testVoiceProducesOutput();
    testEditor();

    std::puts ("All Sgraffito plugin tests passed.");
    return 0;
}
