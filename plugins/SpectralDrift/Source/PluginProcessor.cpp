#include "PluginProcessor.h"

#include "SuiteState.h"
#include "PluginEditor.h"

#if SUITE_PRESET_BANK_AS_PROGRAMS
 #include "PresetBank.h"
#endif

#include <iterator>

namespace
{
    constexpr float defaultDepthPercent = 19.1f;   // 0.6 rad, the engine default, as a percentage of pi

    /** The controls the routing can move. Choice index n is entry n - 1, so the order matches
        ModTarget and is saved in sessions: append only. */
    const SuiteModulation::TargetInfo modulationTargets[] {
        { SpectralDriftProcessor::depthId,           "Depth" },
        { SpectralDriftProcessor::spreadId,          "Bin Spread" },
        { SpectralDriftProcessor::stereoPhaseId,     "Stereo Phase" },
        { SpectralDriftProcessor::shiftId,           "Spectral Shift" },
        { SpectralDriftProcessor::tiltId,            "Spectral Tilt" },
        { SpectralDriftProcessor::gateId,            "Spectral Gate" },
        { SpectralDriftProcessor::feedbackId,        "Feedback" },
        { SpectralDriftProcessor::feedbackDampingId, "FB Damping" },
        { SpectralDriftProcessor::driftStartId,      "Drift Start" },
        { SpectralDriftProcessor::outputGainId,      "Output" },
        { SpectralDriftProcessor::mixId,             "Mix" },
        { nullptr,                                   "Drift" },   // no parameter: the routed LFO drives the drift directly
    };
}

SpectralDriftProcessor::SpectralDriftProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout()),
      modulation (apvts, modulationTargets, (int) std::size (modulationTargets), nullptr)
{
    stateVersion = 2;
    // Version 2 removed the Drift Rate parameter and appended the Drift target: routing
    // target indices shift down by one past the removed entry, and a route that pointed at
    // Drift Rate points at nothing now.
    migrations =
    {
        { 1, [] (juce::ValueTree& tree)
             {
                 for (int i = 0; i < tree.getNumChildren(); ++i)
                 {
                     auto child = tree.getChild (i);
                     const auto id = child.getProperty ("id").toString();

                     if (! id.startsWith ("mod") || ! id.endsWith ("Target"))
                         continue;

                     const int oldIndex = (int) (double) child.getProperty ("value", 0.0);

                     // 1 was Drift Rate (gone); 0 is None. Indices above it shift down one.
                     const int newIndex = oldIndex <= 1 ? 0 : oldIndex - 1;
                     child.setProperty ("value", (double) newIndex, nullptr);
                 }
             } }
    };
    depth = apvts.getRawParameterValue (depthId);
    spread = apvts.getRawParameterValue (spreadId);
    stereoPhase = apvts.getRawParameterValue (stereoPhaseId);
    shift = apvts.getRawParameterValue (shiftId);
    tilt = apvts.getRawParameterValue (tiltId);
    gate = apvts.getRawParameterValue (gateId);
    feedback = apvts.getRawParameterValue (feedbackId);
    mix = apvts.getRawParameterValue (mixId);
    freeze = apvts.getRawParameterValue (freezeId);
    feedbackDamping = apvts.getRawParameterValue (feedbackDampingId);
    driftStart = apvts.getRawParameterValue (driftStartId);
    outputGain = apvts.getRawParameterValue (outputGainId);
    sidechainAmount = apvts.getRawParameterValue (sidechainAmountId);
    sidechainTarget = apvts.getRawParameterValue (sidechainTargetId);
}

juce::AudioProcessorValueTreeState::ParameterLayout SpectralDriftProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { depthId, 1 }, "Drift Depth", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), defaultDepthPercent, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { spreadId, 1 }, "Bin Spread", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), CoreDSP::Spectral::defaultBinSpread, "rad"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { stereoPhaseId, 1 }, "Stereo Phase", juce::NormalisableRange<float> (0.0f, 180.0f, 0.1f), 90.0f, "deg"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shiftId, 1 }, "Spectral Shift", juce::NormalisableRange<float> (-32.0f, 32.0f, 0.01f), 0.0f, "bins"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { tiltId, 1 }, "Spectral Tilt", juce::NormalisableRange<float> (-2.0f, 2.0f, 0.001f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { gateId, 1 }, "Spectral Gate", juce::NormalisableRange<float> (CoreDSP::Spectral::gateOffDb, 0.0f, 0.1f), CoreDSP::Spectral::gateOffDb, "dB"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { feedbackId, 1 }, "Feedback", juce::NormalisableRange<float> (0.0f, 12.0f, 0.01f), 0.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { mixId, 1 }, "Mix", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { freezeId, 1 }, "Freeze", false));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { feedbackDampingId, 1 }, "Feedback Damping", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { driftStartId, 1 }, "Drift Start", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { outputGainId, 1 }, "Output", juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f, "dB"));

    // LFOs and modulation routing, appended last so the existing parameter order never moves.
    SuiteModulation::Router::addParameters (params, modTargetNames());

    // Sidechain ducking (plan 06 idea 2.5), appended so no saved index moves. The amount
    // defaults to 0, so the sound is unchanged at the defaults.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { sidechainAmountId, 1 }, "Sidechain Amount",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, "%"));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { sidechainTargetId, 1 }, "Sidechain Target",
        juce::StringArray { "Drift", "Gate" }, 0));

    return { params.begin(), params.end() };
}

juce::StringArray SpectralDriftProcessor::modTargetNames()
{
    // Must follow the ModTarget enum order exactly.
    return { "None", "Drift Depth", "Bin Spread", "Stereo Phase", "Spectral Shift",
             "Spectral Tilt", "Spectral Gate", "Feedback", "FB Damping", "Drift Start", "Output",
             "Mix", "Drift" };
}

void SpectralDriftProcessor::readSettings (CoreDSP::Spectral::DriftEngine::Settings& settings,
                                           const SuiteModulation::Router::Offsets& offsets) const noexcept
{
    // The drift is driven from the outside (plan 05.1 pass 2): the routed LFO's value,
    // normalised 0..1 around no-drift, mapped to -1..1. The depth parameter scales it to
    // radians in the engine. With no routing assigned the value sits at 0: no drift.
    const float driftNormalised = modulation.modulate ((int) ModTarget::Drift, 0.5f, offsets);
    settings.driftValue = driftNormalised * 2.0f - 1.0f;
    settings.driftDepth = modulation.modulate ((int) ModTarget::Depth, depth->load(), offsets) / 100.0f * juce::MathConstants<float>::pi;
    settings.binSpread = modulation.modulate ((int) ModTarget::BinSpread, spread->load(), offsets);
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
    sidechainEnv.prepare (preparedSampleRate, 5.0f, 150.0f);   // the duck's detector: fast attack, breathing release
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

    if (input != output)
        return false;

    // The sidechain bus (plan 06 idea 2.5) is declared disabled by default. When a host
    // enables it, accept mono or stereo; a disabled bus keeps its empty set.
    if (layouts.inputBuses.size() > 1)
    {
        const auto& sidechain = layouts.inputBuses[(size_t) 1];
        if (! sidechain.isDisabled()
            && sidechain != juce::AudioChannelSet::mono()
            && sidechain != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}

void SpectralDriftProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // The main output bus, not the buffer: with the sidechain bus enabled the buffer also
    // carries the sidechain channels behind the main ones (plan 06 idea 2.5).
    const int numChannels = juce::jmin (getMainBusNumOutputChannels(), (int) engines.size());
    const int numSamples = buffer.getNumSamples();

    // The LFOs run on a tempo-locked internal beat clock: an effect has no sequencer position to
    // follow, and a tempo-locked clock keeps the synced shapes moving when the transport is stopped.
    const auto offsets = modulation.computeOffsets ((double) streamSample, beatClock, (double) streamSample);
    modulation.publish (offsets, (double) streamSample, beatClock);

    CoreDSP::Spectral::DriftEngine::Settings settings;
    readSettings (settings, offsets);

    // Sidechain ducking (plan 06 idea 2.5). The bus is disabled by default, so with no
    // sidechain routed the duck stays 0 and the sound is unchanged. The detector is a peak
    // follower (5 ms attack, 150 ms release) over the sidechain channels; the block takes
    // the loudest level the follower reached in it.
    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
    {
        const auto sidechain = getBusBuffer (buffer, true, 1);
        const auto* sidechainLeft = sidechain.getReadPointer (0);
        const auto* sidechainRight = sidechain.getNumChannels() > 1 ? sidechain.getReadPointer (1) : nullptr;

        float duck = 0.0f;
        for (int n = 0; n < numSamples; ++n)
        {
            const float in = sidechainRight != nullptr
                ? juce::jmax (std::abs (sidechainLeft[n]), std::abs (sidechainRight[n]))
                : std::abs (sidechainLeft[n]);
            duck = juce::jmax (duck, sidechainEnv.process (in));
        }

        duck = juce::jlimit (0.0f, 1.0f, duck) * juce::jlimit (0.0f, 1.0f, sidechainAmount->load() / 100.0f);

        if (sidechainTarget->load() > 0.5f)
        {
            // Gate: the threshold sweeps toward 0 dB, closing bins from the bottom up, so
            // the wet spectrum thins while the sidechain sounds.
            settings.gateDb += duck * (0.0f - settings.gateDb);
        }
        else
        {
            // Drift: the depth scales down, so the phase swing flattens while it ducks.
            settings.driftDepth *= 1.0f - duck;
        }
    }

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

    // Clear any output channel the engines did not fill. Bounded by the main output bus, so
    // the sidechain input channels (behind the main ones) are left alone (plan 06 idea 2.5).
    for (int channel = numChannels; channel < getMainBusNumOutputChannels(); ++channel)
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
    destData = SuiteState::write (apvts, stateVersion);
}

void SpectralDriftProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    SuiteState::read (data, sizeInBytes, apvts, stateVersion, migrations);

    // The preset identity rides as a property (plan 09 1.4, D3): restore the bar's
    // selection from the replaced state. A session without the property — every
    // session saved before plan 09 — reads as Init.
    lastPresetSelection = SuitePresets::decodePresetSelection (
        apvts.state.getProperty (presetSelectionProperty).toString());
}

void SpectralDriftProcessor::setPresetSelection (const SuitePresets::PresetSelection& selection)
{
    lastPresetSelection = selection;

    // The lerpCurve / samplePath pattern (plan 09 1.4): keep the property in sync at
    // change time, so the identity rides in every saved session and host program dump.
    if (selection.kind == SuitePresets::PresetSelection::Kind::none)
        apvts.state.removeProperty (presetSelectionProperty, nullptr);
    else
        apvts.state.setProperty (presetSelectionProperty,
                                 SuitePresets::encodePresetSelection (selection), nullptr);
}

void SpectralDriftProcessor::applyPresetState (const juce::ValueTree& tree)
{
    // Presets use resetMissing=true so absent parameters are set to default. The tree is encoded into the
    // binary state format first, so it goes through the same read path as a saved session: version check,
    // migrations, replaceState, resetMissing.
    const juce::MemoryBlock block = SuiteState::detail::encodeXml (*tree.createXml());
    const auto result = SuiteState::read (block.getData(), (int) block.getSize(), apvts, stateVersion,
                                          migrations, {}, true);

    // Applying any tree resets the preset identity (plan 09 1.4): the caller — the bar
    // or the host-program bridge — re-states it right after, when it knows the preset.
    if (result == SuiteState::LoadResult::loaded)
        lastPresetSelection = {};

#if SUITE_PRESET_BANK_AS_PROGRAMS
    // A preset applied from the bar or a file is no longer one of the host programs.
    if (result == SuiteState::LoadResult::loaded)
        currentProgramIndex.store (-1, std::memory_order_relaxed);
#else
    juce::ignoreUnused (result);
#endif
}

//==============================================================================
// Host program list. Program 0 is Init; 1 to N are the factory presets in bank order. The list only
// exists in a Pro build with SUITE_PRESET_BANK_AS_PROGRAMS on; otherwise the plugin reports one
// program, as it did before the preset system.

int SpectralDriftProcessor::getNumPrograms()
{
#if SUITE_PRESET_BANK_AS_PROGRAMS
    return SuitePresets::PresetPrograms::getNumPrograms (getPresetPluginId());
#else
    return 1;
#endif
}

int SpectralDriftProcessor::getCurrentProgram()
{
#if SUITE_PRESET_BANK_AS_PROGRAMS
    return currentProgramIndex.load (std::memory_order_relaxed);
#else
    return 0;
#endif
}

void SpectralDriftProcessor::setCurrentProgram (int index)
{
#if SUITE_PRESET_BANK_AS_PROGRAMS
    if (SuitePresets::PresetPrograms::applyProgram (*this, index))
    {
        currentProgramIndex.store (index, std::memory_order_relaxed);

        // The host program and the bar's selection describe the same event (plan 09 1.4):
        // program 0 is Init, program N is factory preset N-1 in bank order.
        SuitePresets::PresetSelection selection;

        if (index > 0)
        {
            selection.kind = SuitePresets::PresetSelection::Kind::factory;
            selection.factoryIndex = index - 1;
        }

        setPresetSelection (selection);
    }
#else
    juce::ignoreUnused (index);
#endif
}

const juce::String SpectralDriftProcessor::getProgramName (int index)
{
#if SUITE_PRESET_BANK_AS_PROGRAMS
    return SuitePresets::PresetPrograms::getProgramName (getPresetPluginId(), index);
#else
    juce::ignoreUnused (index);
    return {};
#endif
}

void SpectralDriftProcessor::changeProgramName (int, const juce::String&)
{
    // The factory programs are read-only.
}

void SpectralDriftProcessor::getCurrentProgramStateInformation (juce::MemoryBlock& destData)
{
    getStateInformation (destData);
}

void SpectralDriftProcessor::setCurrentProgramStateInformation (const void* data, int sizeInBytes)
{
    setStateInformation (data, sizeInBytes);

#if SUITE_PRESET_BANK_AS_PROGRAMS
    currentProgramIndex.store (-1, std::memory_order_relaxed);
#endif
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


