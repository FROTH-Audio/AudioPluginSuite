#include "PluginProcessor.h"
#include "PluginEditor.h"

MultibandSaturationProcessor::MultibandSaturationProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    mode         = apvts.getRawParameterValue (modeId);
    crossover    = apvts.getRawParameterValue (crossoverId);
    driveLow     = apvts.getRawParameterValue (driveLowId);
    driveHigh    = apvts.getRawParameterValue (driveHighId);
    sensitivity  = apvts.getRawParameterValue (sensitivityId);
    attack       = apvts.getRawParameterValue (attackId);
    release      = apvts.getRawParameterValue (releaseId);
    toneLow      = apvts.getRawParameterValue (toneLowId);
    toneHigh     = apvts.getRawParameterValue (toneHighId);
    mix          = apvts.getRawParameterValue (mixId);
    outputLevel  = apvts.getRawParameterValue (outputLevelId);
}

juce::AudioProcessorValueTreeState::ParameterLayout MultibandSaturationProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { modeId, 1 }, "Saturation Mode",
        juce::StringArray { "Tube", "Tape", "Diode" }, 0));

    // Log scale: 1 kHz sits at the centre of the slider travel.
    juce::NormalisableRange<float> crossoverRange (80.0f, 8000.0f, 1.0f);
    crossoverRange.setSkewForCentre (1000.0f);
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { crossoverId, 1 }, "Crossover", crossoverRange, 500.0f, "Hz"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { driveLowId, 1 }, "Drive Low",
        juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f), 6.0f, "dB"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { driveHighId, 1 }, "Drive High",
        juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f), 6.0f, "dB"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { sensitivityId, 1 }, "Envelope Sensitivity",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { attackId, 1 }, "Attack",
        juce::NormalisableRange<float> (1.0f, 100.0f, 0.1f, 0.35f), 5.0f, "ms"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { releaseId, 1 }, "Release",
        juce::NormalisableRange<float> (50.0f, 2000.0f, 0.1f, 0.4f), 150.0f, "ms"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { toneLowId, 1 }, "Tone Low",
        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { toneHighId, 1 }, "Tone High",
        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { mixId, 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, "%"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { outputLevelId, 1 }, "Output Level",
        juce::NormalisableRange<float> (-12.0f, 6.0f, 0.01f), 0.0f, "dB"));

    return { params.begin(), params.end() };
}

void MultibandSaturationProcessor::prepareToPlay (double sampleRate, int)
{
    preparedSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    for (auto& split : splits)
        split.prepare (preparedSampleRate, crossover->load());

    const float attackMs = juce::jlimit (1.0f, 100.0f, attack->load());
    const float releaseMs = juce::jlimit (50.0f, 2000.0f, release->load());
    for (auto& follower : envelopes)
        follower.prepare (preparedSampleRate, attackMs, releaseMs);

    for (auto& tone : tones)
        tone.prepare (preparedSampleRate);

    driveLowSmoother.reset (preparedSampleRate, 0.05);
    driveHighSmoother.reset (preparedSampleRate, 0.05);
    toneLowSmoother.reset (preparedSampleRate, 0.05);
    toneHighSmoother.reset (preparedSampleRate, 0.05);
    mixSmoother.reset (preparedSampleRate, 0.05);
    outputGainSmoother.reset (preparedSampleRate, 0.05);

    driveLowSmoother.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (driveLow->load()));
    driveHighSmoother.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (driveHigh->load()));
    toneLowSmoother.setCurrentAndTargetValue (toneLow->load());
    toneHighSmoother.setCurrentAndTargetValue (toneHigh->load());
    mixSmoother.setCurrentAndTargetValue (mix->load() / 100.0f);
    outputGainSmoother.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputLevel->load()));
}

bool MultibandSaturationProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& output = layouts.getMainOutputChannelSet();
    const auto& input = layouts.getMainInputChannelSet();

    if (output != juce::AudioChannelSet::mono() && output != juce::AudioChannelSet::stereo())
        return false;

    return input == output;
}

void MultibandSaturationProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = juce::jmin (buffer.getNumChannels(), (int) splits.size());
    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0)
        return;

    // Block settings. The smoothers ramp over 50 ms, so automation never zips.
    const auto mode = (CoreDSP::Saturation::Mode) juce::jlimit (0, 2, (int) this->mode->load());
    for (auto& split : splits)
        split.setCrossoverHz (crossover->load());

    const float sensitivityAmount = juce::jlimit (0.0f, 1.0f, sensitivity->load());
    driveLowSmoother.setTargetValue (juce::Decibels::decibelsToGain (driveLow->load()));
    driveHighSmoother.setTargetValue (juce::Decibels::decibelsToGain (driveHigh->load()));
    toneLowSmoother.setTargetValue (toneLow->load());
    toneHighSmoother.setTargetValue (toneHigh->load());
    mixSmoother.setTargetValue (juce::jlimit (0.0f, 100.0f, mix->load()) / 100.0f);
    outputGainSmoother.setTargetValue (juce::Decibels::decibelsToGain (outputLevel->load()));

    const float attackMs = juce::jlimit (1.0f, 100.0f, attack->load());
    const float releaseMs = juce::jlimit (50.0f, 2000.0f, release->load());
    for (auto& follower : envelopes)
        follower.setTimes (attackMs, releaseMs);

    for (int n = 0; n < numSamples; ++n)
    {
        const float driveLowGain = driveLowSmoother.getNextValue();
        const float driveHighGain = driveHighSmoother.getNextValue();
        const float toneLowValue = toneLowSmoother.getNextValue();
        const float toneHighValue = toneHighSmoother.getNextValue();
        const float mixAmount = mixSmoother.getNextValue();
        const float outputGain = outputGainSmoother.getNextValue();

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);
            const float in = data[n];

            float low = 0.0f, high = 0.0f;
            splits[(size_t) channel].process (channel, in, low, high);

            // Each band gets the Dynamic Drive rule: Drive x (1 + Sensitivity x Envelope).
            const float envLow = envelopes[(size_t) channel].process (low);
            const float envHigh = envelopes[2 + (size_t) channel].process (high);
            const float preLow = CoreDSP::Saturation::dynamicDrive (driveLowGain, sensitivityAmount, envLow);
            const float preHigh = CoreDSP::Saturation::dynamicDrive (driveHighGain, sensitivityAmount, envHigh);

            float wetLow = CoreDSP::Saturation::shape (preLow * low, mode);
            float wetHigh = CoreDSP::Saturation::shape (preHigh * high, mode);

            wetLow = tones[(size_t) channel].process (wetLow, toneLowValue);
            wetHigh = tones[2 + (size_t) channel].process (wetHigh, toneHighValue);

            // The sum is clamped to the rails (the Dynamic Saturation ceiling rule), then the
            // dry/wet mix and the output level. No added latency, so the dry path needs no delay.
            float wet = juce::jlimit (-1.0f, 1.0f, wetLow + wetHigh);
            data[n] = (in + mixAmount * (wet - in)) * outputGain;
        }
    }
}

juce::AudioProcessorEditor* MultibandSaturationProcessor::createEditor()
{
    return new MultibandSaturationEditor (*this);
}

void MultibandSaturationProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    destData = SuiteState::write (apvts, stateVersion);
}

void MultibandSaturationProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    SuiteState::read (data, sizeInBytes, apvts, stateVersion, migrations);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MultibandSaturationProcessor();
}
