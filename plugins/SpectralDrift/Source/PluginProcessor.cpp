#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <iterator>

namespace
{
    constexpr float defaultDepthPercent = 19.1f;   // 0.6 rad, the engine default, as a percentage of pi

    /** The controls the routing can move. Choice index n is entry n - 1, so the order matches
        ModTarget and is saved in sessions: append only. */
    const SuiteModulation::TargetInfo modulationTargets[] {
        { SpectralDriftProcessor::driftRateId,       "Drift Rate" },
        { SpectralDriftProcessor::depthId,           "Drift Depth" },
        { SpectralDriftProcessor::spreadId,          "Bin Spread" },
        { SpectralDriftProcessor::stereoPhaseId,     "Stereo Phase" },
        { SpectralDriftProcessor::shiftId,           "Spectral Shift" },
        { SpectralDriftProcessor::tiltId,            "Spectral Tilt" },
        { SpectralDriftProcessor::gateId,            "Spectral Gate" },
        { SpectralDriftProcessor::feedbackId,        "Feedback" },
        { SpectralDriftProcessor::feedbackDampingId, "FB Damping" },
        { SpectralDriftProcessor::driftStartId,      "Drift Start" },
        { SpectralDriftProcessor::outputGainId,      "Output Gain" },
        { SpectralDriftProcessor::mixId,             "Mix" },
    };
}

SpectralDriftProcessor::SpectralDriftProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout()),
      modulation (apvts, modulationTargets, (int) std::size (modulationTargets), nullptr)
{
    driftRate = apvts.getRawParameterValue (driftRateId);
    depth = apvts.getRawParameterValue (depthId);
    spread = apvts.getRawParameterValue (spreadId);
    stereoPhase = apvts.getRawParameterValue (stereoPhaseId);
    lfoShape = apvts.getRawParameterValue (lfoShapeId);
    shift = apvts.getRawParameterValue (shiftId);
    tilt = apvts.getRawParameterValue (tiltId);
    gate = apvts.getRawParameterValue (gateId);
    feedback = apvts.getRawParameterValue (feedbackId);
    mix = apvts.getRawParameterValue (mixId);
    freeze = apvts.getRawParameterValue (freezeId);
    feedbackDamping = apvts.getRawParameterValue (feedbackDampingId);
    driftStart = apvts.getRawParameterValue (driftStartId);
    outputGain = apvts.getRawParameterValue (outputGainId);
}

juce::AudioProcessorValueTreeState::ParameterLayout SpectralDriftProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Logarithmic sweep: equal distance per octave, so 1 Hz sits in the middle of the knob's travel.
    const juce::NormalisableRange<float> rateRange (
        0.1f, 10.0f,
        [] (float start, float end, float normalised) { return start * std::pow (end / start, normalised); },
        [] (float start, float end, float value) { return std::log (value / start) / std::log (end / start); });

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { driftRateId, 1 }, "Drift Rate", rateRange, 1.0f, "Hz"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { depthId, 1 }, "Drift Depth", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), defaultDepthPercent, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { spreadId, 1 }, "Bin Spread", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), CoreDSP::Spectral::defaultBinSpread, "rad"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { stereoPhaseId, 1 }, "Stereo Phase", juce::NormalisableRange<float> (0.0f, 180.0f, 0.1f), 90.0f, "deg"));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { lfoShapeId, 1 }, "LFO Shape", juce::StringArray { "Sine", "Triangle", "Saw", "Square" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shiftId, 1 }, "Spectral Shift", juce::NormalisableRange<float> (-32.0f, 32.0f, 0.01f), 0.0f, "bins"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { tiltId, 1 }, "Spectral Tilt", juce::NormalisableRange<float> (-2.0f, 2.0f, 0.001f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { gateId, 1 }, "Spectral Gate", juce::NormalisableRange<float> (CoreDSP::Spectral::gateOffDb, 0.0f, 0.1f), CoreDSP::Spectral::gateOffDb, "dB"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { feedbackId, 1 }, "Feedback", juce::NormalisableRange<float> (0.0f, 20.0f, 0.01f), 0.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { mixId, 1 }, "Mix", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { freezeId, 1 }, "Freeze", false));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { feedbackDampingId, 1 }, "Feedback Damping", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { driftStartId, 1 }, "Drift Start", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { outputGainId, 1 }, "Output Gain", juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f, "dB"));

    // LFOs and modulation routing, appended last so the existing parameter order never moves.
    SuiteModulation::Router::addParameters (params, modTargetNames());

    return { params.begin(), params.end() };
}

juce::StringArray SpectralDriftProcessor::modTargetNames()
{
    // Must follow the ModTarget enum order exactly.
    return { "None", "Drift Rate", "Drift Depth", "Bin Spread", "Stereo Phase", "Spectral Shift",
             "Spectral Tilt", "Spectral Gate", "Feedback", "FB Damping", "Drift Start", "Output Gain",
             "Mix" };
}

void SpectralDriftProcessor::readSettings (CoreDSP::Spectral::DriftEngine::Settings& settings,
                                           const SuiteModulation::Router::Offsets& offsets) const noexcept
{
    settings.driftRateHz = modulation.modulate ((int) ModTarget::DriftRate, driftRate->load(), offsets);
    settings.driftDepth = modulation.modulate ((int) ModTarget::Depth, depth->load(), offsets) / 100.0f * juce::MathConstants<float>::pi;
    settings.binSpread = modulation.modulate ((int) ModTarget::BinSpread, spread->load(), offsets);
    settings.lfoShape = static_cast<CoreDSP::Spectral::LfoShape> (juce::roundToInt (lfoShape->load()));
    settings.shiftBins = modulation.modulate ((int) ModTarget::Shift, shift->load(), offsets);
    settings.tilt = modulation.modulate ((int) ModTarget::Tilt, tilt->load(), offsets);
    settings.gateDb = modulation.modulate ((int) ModTarget::Gate, gate->load(), offsets);
    // Clamped, so sessions saved with the old, larger feedback range come back within the new limit.
    // feedback and feedbackDamping are smoothed per-sample in processBlock, so they stay raw here.
    settings.feedback = juce::jmin (feedback->load(), maxFeedbackPercent) / 100.0f;
    settings.feedbackDamping = feedbackDamping->load() / 100.0f;
    settings.driftStart = modulation.modulate ((int) ModTarget::DriftStart, driftStart->load(), offsets) / 100.0f;
    // mix is smoothed per-sample in processBlock, so it stays raw here.
    settings.mix = mix->load() / 100.0f;
    settings.frozen = freeze->load() > 0.5f;
}

void SpectralDriftProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    preparedSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    for (auto& engine : engines)
        engine.prepare (sampleRate);

    setLatencySamples (CoreDSP::Spectral::DriftEngine::getLatencySamples());

    modulation.prepare (sampleRate);
    streamSample = 0;
    beatClock = 0.0;

    mixSmoother.reset (sampleRate, 0.02);
    feedbackSmoother.reset (sampleRate, 0.02);
    dampingSmoother.reset (sampleRate, 0.02);
    outputGainSmoother.reset (sampleRate, 0.02);

    mixSmoother.setCurrentAndTargetValue (mix->load() / 100.0f);
    feedbackSmoother.setCurrentAndTargetValue (juce::jmin (feedback->load(), maxFeedbackPercent) / 100.0f);
    dampingSmoother.setCurrentAndTargetValue (feedbackDamping->load() / 100.0f);
    outputGainSmoother.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputGain->load()));

    const auto size = (size_t) juce::jmax (1, samplesPerBlock);
    mixCurve.assign (size, 0.0f);
    feedbackCurve.assign (size, 0.0f);
    dampingCurve.assign (size, 0.0f);
    gainCurve.assign (size, 1.0f);
}

bool SpectralDriftProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& output = layouts.getMainOutputChannelSet();
    const auto& input = layouts.getMainInputChannelSet();

    if (output != juce::AudioChannelSet::mono() && output != juce::AudioChannelSet::stereo())
        return false;

    return input == output;
}

void SpectralDriftProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = juce::jmin (buffer.getNumChannels(), (int) engines.size());
    const int numSamples = buffer.getNumSamples();

    // The LFOs run on a tempo-locked internal beat clock: an effect has no sequencer position to
    // follow, and a tempo-locked clock keeps the synced shapes moving when the transport is stopped.
    const auto offsets = modulation.computeOffsets ((double) streamSample, beatClock, (double) streamSample);
    modulation.publish (offsets, (double) streamSample, beatClock);

    CoreDSP::Spectral::DriftEngine::Settings settings;
    readSettings (settings, offsets);

    // Set targets once per block, then fill the per-sample curves once. Both channels read the same curves,
    // so each channel sees the same ramp. Advancing the smoothers inside the channel loop would make the
    // right channel consume the next block's ramp.
    mixSmoother.setTargetValue (modulation.modulate ((int) ModTarget::Mix, mix->load(), offsets) / 100.0f);
    feedbackSmoother.setTargetValue (juce::jmin (modulation.modulate ((int) ModTarget::Feedback, feedback->load(), offsets), maxFeedbackPercent) / 100.0f);
    dampingSmoother.setTargetValue (modulation.modulate ((int) ModTarget::FeedbackDamping, feedbackDamping->load(), offsets) / 100.0f);
    outputGainSmoother.setTargetValue (juce::Decibels::decibelsToGain (modulation.modulate ((int) ModTarget::OutputGain, outputGain->load(), offsets)));

    if (mixCurve.size() < (size_t) numSamples)
    {
        const auto size = (size_t) numSamples;
        mixCurve.resize (size);
        feedbackCurve.resize (size);
        dampingCurve.resize (size);
        gainCurve.resize (size);
    }

    for (int n = 0; n < numSamples; ++n)
    {
        mixCurve[(size_t) n] = mixSmoother.getNextValue();
        feedbackCurve[(size_t) n] = feedbackSmoother.getNextValue();
        dampingCurve[(size_t) n] = dampingSmoother.getNextValue();
        gainCurve[(size_t) n] = outputGainSmoother.getNextValue();
    }

    // The right channel's LFO is offset by the Stereo Phase control; the left channel is the reference.
    const float stereoOffset = stereoPhase->load() / 180.0f * juce::MathConstants<float>::pi;

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto& engine = engines[(size_t) channel];

        settings.stereoPhase = channel == 0 ? 0.0f : stereoOffset;
        engine.setSettings (settings);

        auto* data = buffer.getWritePointer (channel);

        for (int n = 0; n < numSamples; ++n)
        {
            const auto index = (size_t) n;
            const CoreDSP::Spectral::DriftEngine::PerSampleSettings perSample {
                mixCurve[index],
                feedbackCurve[index],
                dampingCurve[index]
            };

            data[n] = engine.processSample (data[n], perSample);
        }
    }

    for (int channel = numChannels; channel < buffer.getNumChannels(); ++channel)
        buffer.clear (channel, 0, numSamples);

    // Output gain is applied per sample after the engines, so it scales the wet and dry signal alike.
    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* data = buffer.getWritePointer (channel);

        for (int n = 0; n < numSamples; ++n)
            data[n] *= gainCurve[(size_t) n];
    }

    // Advance the clock that drives the free LFOs and the synced beat position.
    // No host tempo (including the Standalone): a fixed 120 BPM keeps the synced LFOs moving.
    beatClock += (double) numSamples * SuiteModulation::hostTempoOrDefault (getPlayHead()) / 60.0 / preparedSampleRate;
    streamSample += numSamples;
    modulation.advanceBlock (numSamples);

    // The editor shows the left channel only.
    if (numChannels > 0)
    {
        const auto& in = engines[0].getInputMagnitudes();
        const auto& out = engines[0].getOutputMagnitudes();

        for (int k = 0; k < numBins; ++k)
        {
            snapshot.input[(size_t) k].store (in[(size_t) k], std::memory_order_relaxed);
            snapshot.output[(size_t) k].store (out[(size_t) k], std::memory_order_relaxed);
        }

        snapshot.frames.store (engines[0].getFrameCount(), std::memory_order_relaxed);
    }
}

juce::AudioProcessorEditor* SpectralDriftProcessor::createEditor()
{
    return new SpectralDriftEditor (*this);
}

void SpectralDriftProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SpectralDriftProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

float SpectralDriftProcessor::getModulatedValue (ModTarget target) const
{
    return modulation.getModulatedValue ((int) target);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpectralDriftProcessor();
}
