#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "StereoEngine.h"

#if SUITE_PRESET_BANK_AS_PROGRAMS
 #include "PresetBank.h"
#endif

StereoToolProcessor::StereoToolProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
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

    return { params.begin(), params.end() };
}

void StereoToolProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    dryBuffer.setSize (2, samplesPerBlock, false, false, true);
    dryBuffer.clear();

    // The rate is the one prepareToPlay was given: getSampleRate() is 0 in a bare test harness.
    preparedSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

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

    // Settings for this block.
    const float width = juce::jlimit (-100.0f, 100.0f, widthParam->load()) / 100.0f;
    const float sideGain = CoreDSP::Stereo::sideGain (width);

    const float driveDb = juce::jlimit (-24.0f, 24.0f, driveParam->load());
    const float mix = juce::jlimit (0.0f, 100.0f, mixParam->load()) / 100.0f;

    const int characterIndex = juce::jlimit (0, 4, (int) (characterParam->load() + 0.5f));
    const float transientAmount = juce::jlimit (0.0f, 100.0f, transientAmountParam->load()) / 100.0f;
    const int transientTarget = juce::jlimit (0, 2, (int) (transientTargetParam->load() + 0.5f));
    const float midFocus = juce::jlimit (-100.0f, 100.0f, midFocusParam->load()) / 100.0f;
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
        sideFilter.setCutoffFrequency (juce::jlimit (20.0f, highestCorner, sideCrossoverParam->load()));
    }

    // The follower's times are cheap to set; a block-rate update tracks the parameters.
    if (transientAmount > 0.0f)
        transientEnv.setTimes (juce::jlimit (1.0f, 200.0f, transientAttackParam->load()),
                               juce::jlimit (20.0f, 2000.0f, transientReleaseParam->load()));

    // Drive amount: 0 dB is transparent. Negative Drive is a clean gain reduction; positive
    // Drive interpolates towards the shaper's pre-gain drive, so loud parts sit near the
    // rails and the small-signal level rises, as a drive knob should.
    const float cleanGain = driveDb < 0.0f ? juce::Decibels::decibelsToGain (driveDb) : 1.0f;
    const float satAmount = driveDb > 0.0f ? driveDb / 24.0f : 0.0f;
    const float satGain   = driveDb > 0.0f ? juce::Decibels::decibelsToGain (driveDb) : 1.0f;

    // When neither the drive, nor the width, nor any plan-6.1 control touches the signal,
    // the wet path is mathematically the input. Taking it out keeps the plugin bit-exact at
    // the defaults (the mid/side round trip alone is not bit-exact in float) and skips work.
    const bool identityPath = satAmount == 0.0f && cleanGain == 1.0f && sideGain == 1.0f
                           && characterIndex == 0 && transientAmount == 0.0f
                           && midFocus == 0.0f && sideFilterIndex == 0;
    if (identityPath && mix == 1.0f)
        return;

    // Dry copy for the mix at the end, into the pre-sized scratch buffer (no allocation).
    dryBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);
    dryBuffer.copyFrom (1, 0, buffer, 1, 0, numSamples);
    auto* dryL = dryBuffer.getWritePointer (0);
    auto* dryR = dryBuffer.getWritePointer (1);

    for (int n = 0; n < numSamples; ++n)
    {
        float sideGainN = sideGain;
        float cleanGainN = cleanGain;
        float satAmountN = satAmount;
        float satGainN = satGain;

        // The transient follower (plan 6.1): a peak envelope over the linked input moves
        // the width and/or the drive. At amount 0 the follower is not applied and the
        // static settings stand.
        if (transientAmount > 0.0f)
        {
            const float env = juce::jlimit (0.0f, 1.0f,
                transientEnv.process (juce::jmax (std::abs (left[n]), std::abs (right[n]))));

            if (transientTarget != 1)   // Width or Both
                sideGainN = CoreDSP::Stereo::sideGain (
                    juce::jlimit (-1.0f, 1.0f, width + transientAmount * env));

            if (transientTarget != 0)   // Drive or Both
            {
                const float driveDbN = juce::jlimit (-24.0f, 24.0f,
                    driveDb + transientAmount * env * maxTransientDriveBoostDb);

                cleanGainN = driveDbN < 0.0f ? juce::Decibels::decibelsToGain (driveDbN) : 1.0f;
                satAmountN = driveDbN > 0.0f ? driveDbN / 24.0f : 0.0f;
                satGainN   = driveDbN > 0.0f ? juce::Decibels::decibelsToGain (driveDbN) : 1.0f;
            }
        }

        float m = 0.0f, s = 0.0f;
        CoreDSP::Stereo::encode (left[n], right[n], m, s);

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
