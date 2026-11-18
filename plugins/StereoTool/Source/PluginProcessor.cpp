#include "PluginProcessor.h"
#include "PluginEditor.h"

StereoToolProcessor::StereoToolProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout StereoToolProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Width: -100 to +100 %, where 0 = correlated stereo
    // +100 = fully uncorrelated, -100 = mono (both channels identical)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { widthId, 1 }, "Width", juce::NormalisableRange<float> (-100.0f, 100.0f, 0.0f), 0.0f, "%"));

    // Mix: dry/wet in % (0-100)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { mixId, 1 }, "Mix", juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, "%"));

    // Drive: tanh saturation gain in dB
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { driveId, 1 }, "Drive", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, "dB"));

    return { params.begin(), params.end() };
}

void StereoToolProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Prepare temp buffer
    msBuffer.setSize (2, samplesPerBlock, false, false, true);
}

bool StereoToolProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void StereoToolProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    auto* leftChannel  = buffer.getWritePointer  (0);
    auto* rightChannel = buffer.getWritePointer (1);

    const int numSamples = buffer.getNumSamples();

    // Save original samples for dry/wet mix
    juce::AudioBuffer<float> original (2, numSamples);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < numSamples; ++i)
            original.setSample (ch, i, ch == 0 ? buffer.getSample (ch, i) : buffer.getSample (ch, i));

    // --- 1. Split to mid/side ---
    // mid = (left + right) / 2
    // side = (left - right) / 2
    for (int i = 0; i < numSamples; ++i)
    {
        const float l = leftChannel[i];
        const float r = rightChannel[i];
        msBuffer.setSample (0, i, (l + r) * 0.5f);  // mid
        msBuffer.setSample (1, i, (l - r) * 0.5f);  // side
    }

    // --- 2. Apply tanh saturation to mid/side ---
    const float driveGain = juce::Decibels::decibelsToGain (driveParam ? *driveParam : 0.0f);
    for (int i = 0; i < numSamples; ++i)
    {
        const float m = msBuffer.getSample (0, i);
        const float s = msBuffer.getSample (1, i);
        const float satM = std::tanh (driveGain * m);
        const float satS = std::tanh (driveGain * s);
        msBuffer.setSample (0, i, satM);
        msBuffer.setSample (1, i, satS);
    }

    // --- 3. Width control (correlated/uncorrelated balance) ---
    // width = 0: correlated stereo (side = 0, output = original)
    // width > 0: add uncorrelated component (widen)
    // width < 0: make more correlated (narrow)
    const float w = jlimit (-1.0f, 1.0f, widthParam ? *widthParam : 0.0f) / 100.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float m = msBuffer.getSample (0, i);
        const float s = msBuffer.getSample (1, i);

        // Width control: out = mid + w * side
        // This preserves total level approx and controls correlation
        float outL = m + w * s;  // left = mid + width * side
        float outR = m - w * s;  // right = mid - width * side

        leftChannel[i]  = outL;
        rightChannel[i] = outR;
    }

    // --- 4. Apply dry/wet mix ---
    const float wet = mixParam ? juce::jlimit (0.0f, 100.0f, *mixParam) / 100.0f : 1.0f;
    const float dry = 1.0f - wet;

    // Mix dry (original) with wet (width-processed signal)
    for (int i = 0; i < numSamples; ++i)
    {
        leftChannel[i]  = leftChannel[i]  * dry + original.getSample (0, i) * wet;
        rightChannel[i] = rightChannel[i] * dry + original.getSample (1, i) * wet;
    }
}

//==============================================================================
juce::AudioProcessorEditor* StereoToolProcessor::createEditor()
{
    return new StereoToolEditor (*this);
}

void StereoToolProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    destData = SuiteState::write (apvts, 1);
}

void StereoToolProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    SuiteState::read (data, sizeInBytes, apvts, 1, {});
}