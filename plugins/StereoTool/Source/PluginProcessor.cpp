#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "StereoEngine.h"

StereoToolProcessor::StereoToolProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    widthParam = apvts.getRawParameterValue (widthId);
    driveParam = apvts.getRawParameterValue (driveId);
    mixParam   = apvts.getRawParameterValue (mixId);
}

juce::AudioProcessorValueTreeState::ParameterLayout StereoToolProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Width -100..+100 %: 0 leaves the stereo image unchanged, -100 folds to mono,
    // +100 doubles the side energy. The engine clamps, so the decode never folds.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { widthId, 1 }, "Width",
        juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, "%"));

    // Drive in dB. 0 dB is transparent; below 0 is a clean gain reduction, above 0
    // saturates with a unity small-signal gain.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { driveId, 1 }, "Drive",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, "dB"));

    // Mix 0..100 %: 0 is the dry input, 100 the fully processed signal.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { mixId, 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, "%"));

    return { params.begin(), params.end() };
}

void StereoToolProcessor::prepareToPlay (double, int samplesPerBlock)
{
    dryBuffer.setSize (2, samplesPerBlock, false, false, true);
    dryBuffer.clear();
}

bool StereoToolProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void StereoToolProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0 || buffer.getNumChannels() < 2)
        return;

    // Hosts may exceed the announced block size; grow the scratch buffer if so (rare path).
    if (numSamples > dryBuffer.getNumSamples())
        dryBuffer.setSize (2, numSamples, false, true, true);

    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);

    // Settings for this block.
    const float width = juce::jlimit (-100.0f, 100.0f, widthParam->load()) / 100.0f;
    const float sideGain = CoreDSP::Stereo::sideGain (width);

    const float driveDb = juce::jlimit (-24.0f, 24.0f, driveParam->load());
    const float mix = juce::jlimit (0.0f, 100.0f, mixParam->load()) / 100.0f;

    // Drive amount: 0 dB is transparent. Negative Drive is a clean gain reduction; positive
    // Drive interpolates towards tanh pre-gain drive (CoreDSP::Stereo::saturate), so loud
    // parts sit near the rails and the small-signal level rises, as a drive knob should.
    const float cleanGain = driveDb < 0.0f ? juce::Decibels::decibelsToGain (driveDb) : 1.0f;
    const float satAmount = driveDb > 0.0f ? driveDb / 24.0f : 0.0f;
    const float satGain   = driveDb > 0.0f ? juce::Decibels::decibelsToGain (driveDb) : 1.0f;

    // When neither the drive nor the width touches the signal, the wet path is mathematically
    // the input. Taking it out keeps the plugin bit-exact at the defaults (the mid/side round
    // trip alone is not bit-exact in float) and skips the work.
    const bool identityPath = satAmount == 0.0f && cleanGain == 1.0f && sideGain == 1.0f;
    if (identityPath && mix == 1.0f)
        return;

    // Dry copy for the mix at the end, into the pre-sized scratch buffer (no allocation).
    dryBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);
    dryBuffer.copyFrom (1, 0, buffer, 1, 0, numSamples);
    auto* dryL = dryBuffer.getWritePointer (0);
    auto* dryR = dryBuffer.getWritePointer (1);

    for (int n = 0; n < numSamples; ++n)
    {
        float m = 0.0f, s = 0.0f;
        CoreDSP::Stereo::encode (left[n], right[n], m, s);

        if (satAmount > 0.0f)
        {
            m = CoreDSP::Stereo::saturate (m, satAmount, satGain);
            s = CoreDSP::Stereo::saturate (s, satAmount, satGain);
        }
        else
        {
            m *= cleanGain;
            s *= cleanGain;
        }

        s *= sideGain;

        float outL = 0.0f, outR = 0.0f;
        CoreDSP::Stereo::decode (m, s, outL, outR);

        left[n]  = (1.0f - mix) * dryL[n] + mix * outL;
        right[n] = (1.0f - mix) * dryR[n] + mix * outR;
    }
}

juce::AudioProcessorEditor* StereoToolProcessor::createEditor()
{
    return new StereoToolEditor (*this);
}

void StereoToolProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    destData = SuiteState::write (apvts, stateVersion);
}

void StereoToolProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    SuiteState::read (data, sizeInBytes, apvts, stateVersion, migrations);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StereoToolProcessor();
}
