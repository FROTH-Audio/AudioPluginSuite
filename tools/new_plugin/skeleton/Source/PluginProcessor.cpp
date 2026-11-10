#include "PluginProcessor.h"
#include "PluginEditor.h"

__NAME__Processor::__NAME__Processor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    gain = apvts.getRawParameterValue (gainId);
}

juce::AudioProcessorValueTreeState::ParameterLayout __NAME__Processor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { gainId, 1 }, "Gain", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f, "dB"));

    return { params.begin(), params.end() };
}

void __NAME__Processor::prepareToPlay (double, int)
{
}

bool __NAME__Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void __NAME__Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.applyGain (juce::Decibels::decibelsToGain (gain->load()));
}

juce::AudioProcessorEditor* __NAME__Processor::createEditor()
{
    return new __NAME__Editor (*this);
}

void __NAME__Processor::getStateInformation (juce::MemoryBlock& destData)
{
    destData = SuiteState::write (apvts, stateVersion);
}

void __NAME__Processor::setStateInformation (const void* data, int sizeInBytes)
{
    SuiteState::read (data, sizeInBytes, apvts, stateVersion, migrations);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new __NAME__Processor();
}
