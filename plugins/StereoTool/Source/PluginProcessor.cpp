#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "StereoEngine.h"

#if SUITE_PRESET_BANK_AS_PROGRAMS
 #include "PresetBank.h"
#endif

#include <iterator>

namespace
{
    /** The controls the routing can move (plan 13). Choice index n is entry n - 1, so the
        order matches ModTarget and is saved in sessions: append only. */
    const SuiteModulation::TargetInfo modulationTargets[] {
        { StereoToolProcessor::widthId,            "Width" },
        { StereoToolProcessor::driveId,            "Drive" },
        { StereoToolProcessor::mixId,              "Mix" },
        { StereoToolProcessor::transientAttackId,  "Transient Attack" },
        { StereoToolProcessor::transientReleaseId, "Transient Release" },
        { StereoToolProcessor::transientAmountId,  "Transient Amount" },
        { StereoToolProcessor::midFocusId,         "Mid Focus" },
        { StereoToolProcessor::sideCrossoverId,    "Side Crossover" },
        { StereoToolProcessor::inputLevelId,       "Input" },
        { StereoToolProcessor::outputLevelId,      "Output" },
    };
}

StereoToolProcessor::StereoToolProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout()),
      modulation (apvts, modulationTargets, (int) std::size (modulationTargets), nullptr)
{
    widthParam = apvts.getRawParameterValue (widthId);
    driveParam = apvts.getRawParameterValue (driveId);
    mixParam   = apvts.getRawParameterValue (mixId);
    characterParam        = apvts.getRawParameterValue (characterId);
    transientAttackParam  = apvts.getRawParameterValue (transientAttackId);
    transientReleaseParam = apvts.getRawParameterValue (transientReleaseId);
    transientAmountParam  = apvts.getRawParameterValue (transientAmountId);
    transientTargetParam  = apvts.getRawParameterValue (transientTargetId);
    midFocusParam         = apvts.getRawParameterValue (midFocusId);
    sideFilterParam       = apvts.getRawParameterValue (sideFilterId);
    sideCrossoverParam    = apvts.getRawParameterValue (sideCrossoverId);
    inputLevelParam       = apvts.getRawParameterValue (inputLevelId);
    outputLevelParam      = apvts.getRawParameterValue (outputLevelId);
    panParam              = apvts.getRawParameterValue (panId);
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

    //---- Plan 6.1: the character stereo processor, appended so no saved parameter index
    // moves. Every default is neutral, so the sound is unchanged.
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { characterId, 1 }, "Character",
        juce::StringArray { "Tube", "Tape", "Diode", "Console", "Clip" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { transientAttackId, 1 }, "Transient Attack",
        juce::NormalisableRange<float> (1.0f, 200.0f, 0.1f, 0.35f), 5.0f, "ms"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { transientReleaseId, 1 }, "Transient Release",
        juce::NormalisableRange<float> (20.0f, 2000.0f, 0.1f, 0.4f), 150.0f, "ms"));

    // 0 bypasses the follower entirely, so the static settings stand.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { transientAmountId, 1 }, "Transient Amount",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { transientTargetId, 1 }, "Transient Target",
        juce::StringArray { "Width", "Drive", "Both" }, 2));

    // midGain = 1 + f, sideGain = 1 - f: positive pulls the centre forward, negative widens.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { midFocusId, 1 }, "Mid Focus",
        juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, "%"));

    // Off is a true bypass: no filter node runs.
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { sideFilterId, 1 }, "Side Filter",
        juce::StringArray { "Off", "High Pass", "Low Pass", "Band Pass" }, 0));

    // Log sweep, so a 100 Hz step is as big as a 400 Hz one.
    const juce::NormalisableRange<float> crossoverRange (
        20.0f, 8000.0f,
        [] (float start, float end, float normalised) { return start * std::pow (end / start, normalised); },
        [] (float start, float end, float value) { return std::log (value / start) / std::log (end / start); });

    juce::AudioParameterFloatAttributes crossoverAttributes;
    crossoverAttributes = crossoverAttributes.withStringFromValueFunction (
        [] (float value, int) { return juce::String (value, 1) + " Hz"; });
    crossoverAttributes = crossoverAttributes.withLabel ("Hz");

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { sideCrossoverId, 1 }, "Side Crossover", crossoverRange, 200.0f, crossoverAttributes));

    //---- Plan 12: the level stage, appended so no saved parameter index moves.
    // 0 dB is exactly unity, so the identity path still holds at the defaults.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { inputLevelId, 1 }, "Input",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, "dB"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { outputLevelId, 1 }, "Output",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, "dB"));

    //---- Plan 17 Phase 4: the pan stage, appended so no saved index moves. 0 % is centre
    // (constant-power law), so the identity path still holds at the defaults.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { panId, 1 }, "Pan",
        juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, "%"));

    //---- Plan 17 Phase 8: interest features, appended so no saved index moves.
    // 1. Mono-maker: frequency below which the stereo image collapses to mono.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { monoMakerCrossoverId, 1 }, "Mono Maker Crossover",
        juce::NormalisableRange<float> (20.0f, 800.0f, 0.1f), 200.0f, "Hz")));

    // 2. Multiband width: per-band width controls (low/mid/high).
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { multibandWidthLowId, 1 }, "Multiband Width Low",
        juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, "%"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { multibandWidthMidId, 1 }, "Multiband Width Mid",
        juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, "%"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { multibandWidthHighId, 1 }, "Multiband Width High",
        juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, "%"));

    // 3. M/S tone shaping: separate tilt/bell for mid and side channels.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { msToneMidId, 1 }, "MS Tone Mid",
        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f, ""));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { msToneSideId, 1 }, "MS Tone Side",
        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f, ""));

    // 4. Image rotation: slow LFO that swings the stereo image left-right.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { imageRotationRateId, 1 }, "Image Rotation Rate",
        juce::NormalisableRange<float> (0.02f, 2.0f, 0.01f), 0.25f, "Hz")));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { imageRotationDepthId, 1 }, "Image Rotation Depth",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, "%"));

    //---- Plan 13: the LFOs and the six routing slots, appended last so no saved parameter
    // index moves. Every routing amount defaults to 0, so the sound is bit-identical.
    SuiteModulation::Router::addParameters (params, modTargetNames());

    return { params.begin(), params.end() };
}

juce::StringArray StereoToolProcessor::modTargetNames()
{
    // Must follow the ModTarget enum order exactly.
    return { "None", "Width", "Drive", "Mix", "Transient Attack", "Transient Release",
             "Transient Amount", "Mid Focus", "Side Crossover", "Input", "Output", "Pan",
             "Mono Maker Crossover", "Multiband Width Low", "Multiband Width Mid",
             "Multiband Width High", "MS Tone Mid", "MS Tone Side",
             "Image Rotation Rate", "Image Rotation Depth" };
}

void StereoToolProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    dryBuffer.setSize (2, samplesPerBlock, false, false, true);
    dryBuffer.clear();

    // The rate is the one prepareToPlay was given: getSampleRate() is 0 in a bare test harness.
    preparedSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    // The level stage (plan 12): 20 ms smoothing, started at the target so the first
    // block is bit-exact at the 0 dB defaults.
    inputGainSmoothed.reset (preparedSampleRate, 0.02);
    inputGainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (
        juce::jlimit (-24.0f, 24.0f, inputLevelParam->load())));
    outputGainSmoothed.reset (preparedSampleRate, 0.02);
    outputGainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (
        juce::jlimit (-24.0f, 24.0f, outputLevelParam->load())));

    // The pan stage (plan 17 Phase 4): 20 ms smoothing, started at the target so the first
    // block is bit-exact at the centre default (the balance law's unity).
    panSmoother.reset (preparedSampleRate, 0.02);
    panSmoother.setCurrentAndTargetValue (juce::jlimit (-100.0f, 100.0f, panParam->load()) / 100.0f);

    // Plan 13: Mix and Drive ramp over 20 ms, starting at the parameter values so the first
    // block is bit-exact at the defaults.
    mixSmoothed.reset (preparedSampleRate, 0.02);
    mixSmoothed.setCurrentAndTargetValue (juce::jlimit (0.0f, 100.0f, mixParam->load()) / 100.0f);
    driveSmoothed.reset (preparedSampleRate, 0.02);
    driveSmoothed.setCurrentAndTargetValue (juce::jlimit (-24.0f, 24.0f, driveParam->load()));

    // The LFOs run on a tempo-locked clock of their own (an effect has no sequencer position).
    modulation.prepare (preparedSampleRate);
    streamSample = 0;
    beatClock = 0.0;

    transientEnv.prepare (preparedSampleRate, transientAttackParam->load(), transientReleaseParam->load());

    if (sampleRate > 0.0)
    {
        sideFilter.prepare ({ sampleRate, 1, (juce::uint32) juce::jmax (1, samplesPerBlock) });
        sideFilter.reset();
        sideFilter.setResonance (1.0f / std::sqrt (2.0f));   // a standard 12 dB/octave filter
    }
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

    // The LFOs run on a tempo-locked internal beat clock: an effect has no sequencer position
    // to follow, and a tempo-locked clock keeps the synced shapes moving when the transport stops.
    const auto offsets = modulation.computeOffsets ((double) streamSample, beatClock, (double) streamSample);
    modulation.publish (offsets, (double) streamSample, beatClock);

    // Settings for this block, each moved by its routing offset (plan 13). Every routing
    // amount defaults to 0, so modulate() returns the base value exactly.
    const float width = juce::jlimit (-100.0f, 100.0f,
        modulation.modulate ((int) ModTarget::Width, widthParam->load(), offsets)) / 100.0f;
    const float sideGain = CoreDSP::Stereo::sideGain (width);

    const float driveDb = juce::jlimit (-24.0f, 24.0f,
        modulation.modulate ((int) ModTarget::Drive, driveParam->load(), offsets));
    const float mixValue = juce::jlimit (0.0f, 100.0f,
        modulation.modulate ((int) ModTarget::Mix, mixParam->load(), offsets));

    const int characterIndex = juce::jlimit (0, 4, (int) (characterParam->load() + 0.5f));
    const float transientAmount = juce::jlimit (0.0f, 100.0f,
        modulation.modulate ((int) ModTarget::TransientAmount, transientAmountParam->load(), offsets)) / 100.0f;
    const int transientTarget = juce::jlimit (0, 2, (int) (transientTargetParam->load() + 0.5f));
    const float midFocus = juce::jlimit (-100.0f, 100.0f,
        modulation.modulate ((int) ModTarget::MidFocus, midFocusParam->load(), offsets)) / 100.0f;
    const int sideFilterIndex = juce::jlimit (0, 3, (int) (sideFilterParam->load() + 0.5f));

    // Mid focus is a static gain pair: midGain = 1 + f, sideGain = 1 - f.
    const float midGainFactor = 1.0f + midFocus;
    const float sideFocusFactor = 1.0f - midFocus;

    if (sideFilterIndex != 0)
    {
        using Type = juce::dsp::StateVariableTPTFilterType;

        sideFilter.setType (sideFilterIndex == 1 ? Type::highpass
                            : sideFilterIndex == 2 ? Type::lowpass
                                                   : Type::bandpass);

        // The filter's assert keeps the corner below Nyquist, so clamp for low-rate hosts.
        const float highestCorner = juce::jmin (8000.0f, 0.45f * (float) preparedSampleRate);
        const float crossover = juce::jlimit (20.0f, 8000.0f,
            modulation.modulate ((int) ModTarget::SideCrossover, sideCrossoverParam->load(), offsets));
        sideFilter.setCutoffFrequency (juce::jlimit (20.0f, highestCorner, crossover));
    }

    // The follower's times are cheap to set; a block-rate update tracks the parameters.
    if (transientAmount > 0.0f)
        transientEnv.setTimes (juce::jlimit (1.0f, 200.0f,
                                   modulation.modulate ((int) ModTarget::TransientAttack, transientAttackParam->load(), offsets)),
                               juce::jlimit (20.0f, 2000.0f,
                                   modulation.modulate ((int) ModTarget::TransientRelease, transientReleaseParam->load(), offsets)));

    // Drive amount: 0 dB is transparent. Negative Drive is a clean gain reduction; positive
    // Drive interpolates towards the shaper's pre-gain drive, so loud parts sit near the
    // rails and the small-signal level rises, as a drive knob should. The smoothing in the
    // loop carries automation and routing without a zipper.
    const float cleanGain = driveDb < 0.0f ? juce::Decibels::decibelsToGain (driveDb) : 1.0f;
    const float satAmount = driveDb > 0.0f ? driveDb / 24.0f : 0.0f;
    const float satGain   = driveDb > 0.0f ? juce::Decibels::decibelsToGain (driveDb) : 1.0f;

    // The level stage (plan 12): smoothed 20 ms, block-rate targets.
    const float inputLevelDb = juce::jlimit (-24.0f, 24.0f,
        modulation.modulate ((int) ModTarget::Input, inputLevelParam->load(), offsets));
    const float outputLevelDb = juce::jlimit (-24.0f, 24.0f,
        modulation.modulate ((int) ModTarget::Output, outputLevelParam->load(), offsets));
    inputGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (inputLevelDb));
    outputGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (outputLevelDb));
    mixSmoothed.setTargetValue (mixValue / 100.0f);
    driveSmoothed.setTargetValue (driveDb);

    // The pan stage (plan 17 Phase 4): constant-power law, smoothed 20 ms like the levels.
    // Routing reads through ModTarget::Pan.
    const float panNorm = juce::jlimit (-100.0f, 100.0f,
        modulation.modulate ((int) ModTarget::Pan, panParam->load(), offsets)) / 100.0f;
    panSmoother.setTargetValue (panNorm);

    // When neither the drive, nor the width, nor any plan-6.1 control touches the signal,
    // the wet path is mathematically the input. Taking it out keeps the plugin bit-exact at
    // the defaults (the mid/side round trip alone is not bit-exact in float) and skips work.
    // The test reads the *modulated* values, so a routed parameter disables the shortcut.
    const bool identityPath = satAmount == 0.0f && cleanGain == 1.0f && sideGain == 1.0f
                           && characterIndex == 0 && transientAmount == 0.0f
                           && midFocus == 0.0f && sideFilterIndex == 0
                           && panParam->load() == 0.0f
                           && inputLevelDb == 0.0f && outputLevelDb == 0.0f;

    // The visual snapshot's accumulators (plan 11 D7): mid/side RMS, follower level.
    float midSum = 0.0f, sideSum = 0.0f, envLast = 0.0f;
    const auto measureInputs = [&] (int n)
    {
        float m = 0.0f, s = 0.0f;
        CoreDSP::Stereo::encode (left[n], right[n], m, s);
        midSum += m * m;
        sideSum += s * s;
    };
    const auto publishSnapshot = [&]
    {
        meterSnapshot.midLevel.store (std::sqrt (midSum / (float) numSamples), std::memory_order_relaxed);
        meterSnapshot.sideLevel.store (std::sqrt (sideSum / (float) numSamples), std::memory_order_relaxed);
        meterSnapshot.followerEnvelope.store (envLast, std::memory_order_relaxed);
    };
    const auto advanceClock = [&]
    {
        // Advance the clock that drives the free LFOs and the synced beat position. With no
        // host tempo (including the Standalone) a fixed 120 BPM keeps the synced LFOs moving.
        beatClock += (double) numSamples * SuiteModulation::hostTempoOrDefault (getPlayHead()) / 60.0 / preparedSampleRate;
        streamSample += numSamples;
        modulation.advanceBlock (numSamples);
    };

    if (identityPath && mixValue == 100.0f)
    {
        // The smoothing clocks still advance, so returning to the identity from a gain
        // automation does not apply a stale ramp later.
        inputGainSmoothed.skip (numSamples);
        outputGainSmoothed.skip (numSamples);
        mixSmoothed.skip (numSamples);
        driveSmoothed.skip (numSamples);

        // The meters still describe the untouched signal (plan 11 D7).
        for (int n = 0; n < numSamples; ++n)
            measureInputs (n);
        publishSnapshot();
        advanceClock();
        return;
    }

    // Dry copy for the mix at the end, into the pre-sized scratch buffer (no allocation).
    dryBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);
    dryBuffer.copyFrom (1, 0, buffer, 1, 0, numSamples);
    auto* dryL = dryBuffer.getWritePointer (0);
    auto* dryR = dryBuffer.getWritePointer (1);

    for (int n = 0; n < numSamples; ++n)
    {
        // Drive is smoothed per sample (plan 13), so routing and automation never step.
        const float driveDbN = driveSmoothed.getNextValue();
        const float driveGainN = driveDbN != 0.0f ? juce::Decibels::decibelsToGain (driveDbN) : 1.0f;
        float cleanGainN = driveDbN < 0.0f ? driveGainN : 1.0f;
        float satAmountN = driveDbN > 0.0f ? driveDbN / 24.0f : 0.0f;
        float satGainN   = driveDbN > 0.0f ? driveGainN : 1.0f;
        float sideGainN = sideGain;

        // The level stage (plan 12). The dry copy was taken before the loop, so the input
        // gain feeds the chain only, and the output gain lands after the mix.
        const float inputGainN = inputGainSmoothed.getNextValue();
        left[n]  *= inputGainN;
        right[n] *= inputGainN;

        // The transient follower (plan 6.1): a peak envelope over the linked input moves
        // the width and/or the drive. At amount 0 the follower is not applied and the
        // static settings stand.
        if (transientAmount > 0.0f)
        {
            const float env = juce::jlimit (0.0f, 1.0f,
                transientEnv.process (juce::jmax (std::abs (left[n]), std::abs (right[n]))));
            envLast = env;

            if (transientTarget != 1)   // Width or Both
                sideGainN = CoreDSP::Stereo::sideGain (
                    juce::jlimit (-1.0f, 1.0f, width + transientAmount * env));

            if (transientTarget != 0)   // Drive or Both
            {
                const float driveDbN2 = juce::jlimit (-24.0f, 24.0f,
                    driveDbN + transientAmount * env * maxTransientDriveBoostDb);

                cleanGainN = driveDbN2 < 0.0f ? juce::Decibels::decibelsToGain (driveDbN2) : 1.0f;
                satAmountN = driveDbN2 > 0.0f ? driveDbN2 / 24.0f : 0.0f;
                satGainN   = driveDbN2 > 0.0f ? juce::Decibels::decibelsToGain (driveDbN2) : 1.0f;
            }
        }

        float m = 0.0f, s = 0.0f;
        CoreDSP::Stereo::encode (left[n], right[n], m, s);

        // The visual snapshot's mid/side accumulation (plan 11 D7).
        midSum += m * m;
        sideSum += s * s;

        if (satAmountN > 0.0f)
        {
            // Tube is the original saturate() exactly, so the pre-plan-6.1 sound and its
            // tests are untouched; the other characters share its blend structure.
            if (characterIndex == 0)
            {
                m = CoreDSP::Stereo::saturate (m, satAmountN, satGainN);
                s = CoreDSP::Stereo::saturate (s, satAmountN, satGainN);
            }
            else
            {
                const auto character = (CoreDSP::Stereo::Character) characterIndex;
                m = CoreDSP::Stereo::saturateCharacter (m, satAmountN, satGainN, character);
                s = CoreDSP::Stereo::saturateCharacter (s, satAmountN, satGainN, character);
            }
        }
        else
        {
            m *= cleanGainN;
            s *= cleanGainN;
        }

        // The side filter (plan 6.1) shapes the side component only. Off skips the node.
        if (sideFilterIndex != 0)
            s = sideFilter.processSample (0, s);

        // Mid focus (plan 6.1), then the width side gain, then decode.
        m *= midGainFactor;
        s *= sideFocusFactor;
        s *= sideGainN;

        float outL = 0.0f, outR = 0.0f;
        CoreDSP::Stereo::decode (m, s, outL, outR);

        const float outputGainN = outputGainSmoothed.getNextValue();
        const float mixN = mixSmoothed.getNextValue();
        left[n]  = ((1.0f - mixN) * dryL[n] + mixN * outL) * outputGainN;
        right[n] = ((1.0f - mixN) * dryR[n] + mixN * outR) * outputGainN;

        // The pan stage (plan 17 Phase 4): a balance law with EXACT unity at centre, so the
        // defaults stay bit-transparent (an equal-power law would sit at -3 dB on both
        // channels). Turning right attenuates the left channel, left attenuates the right;
        // full pan keeps the kept channel at unity.
        const float panN = panSmoother.getNextValue();
        const float panGain = std::cos (std::abs (panN) * juce::MathConstants<float>::pi * 0.25f);

        if (panN >= 0.0f)
            left[n] *= panGain;
        else
            right[n] *= panGain;
    }

    publishSnapshot();
    advanceClock();
}

float StereoToolProcessor::getModulatedValue (ModTarget target) const
{
    return modulation.getModulatedValue ((int) target);
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

    // The preset identity rides as a property (plan 09 1.4, D3): restore the bar's
    // selection from the replaced state. A session without the property — every
    // session saved before plan 09 — reads as Init.
    lastPresetSelection = SuitePresets::decodePresetSelection (
        apvts.state.getProperty (presetSelectionProperty).toString());
}

void StereoToolProcessor::setPresetSelection (const SuitePresets::PresetSelection& selection)
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

void StereoToolProcessor::applyPresetState (const juce::ValueTree& tree)
{
    // Presets use resetMissing=true so absent parameters are set to default. The tree is
    // encoded into the binary state format first, so it goes through the same read path
    // as a saved session: version check, migrations, replaceState, resetMissing.
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

int StereoToolProcessor::getNumPrograms()
{
#if SUITE_PRESET_BANK_AS_PROGRAMS
    return SuitePresets::PresetPrograms::getNumPrograms (getPresetPluginId());
#else
    return 1;
#endif
}

int StereoToolProcessor::getCurrentProgram()
{
#if SUITE_PRESET_BANK_AS_PROGRAMS
    return currentProgramIndex.load (std::memory_order_relaxed);
#else
    return 0;
#endif
}

void StereoToolProcessor::setCurrentProgram (int index)
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

const juce::String StereoToolProcessor::getProgramName (int index)
{
#if SUITE_PRESET_BANK_AS_PROGRAMS
    return SuitePresets::PresetPrograms::getProgramName (getPresetPluginId(), index);
#else
    juce::ignoreUnused (index);
    return {};
#endif
}

void StereoToolProcessor::changeProgramName (int, const juce::String&)
{
    // The factory programs are read-only.
}

void StereoToolProcessor::getCurrentProgramStateInformation (juce::MemoryBlock& destData)
{
    getStateInformation (destData);
}

void StereoToolProcessor::setCurrentProgramStateInformation (const void* data, int sizeInBytes)
{
    setStateInformation (data, sizeInBytes);

#if SUITE_PRESET_BANK_AS_PROGRAMS
    currentProgramIndex.store (-1, std::memory_order_relaxed);
#endif
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StereoToolProcessor();
}
