#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "SuiteState.h"

#include <algorithm>
#include <cmath>

#include <juce_audio_utils/juce_audio_utils.h>

//==============================================================================
SgraffitoProcessor::SgraffitoProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    // Cache raw parameter pointers for the audio thread (lock-free reads).
    modeParam = apvts.getRawParameterValue (modeId);
    regionSpanParam = apvts.getRawParameterValue (regionSpanId);
    regionLengthParam = apvts.getRawParameterValue (regionLengthId);
    rootKeyParam = apvts.getRawParameterValue (rootKeyId);
    pitchFineParam = apvts.getRawParameterValue (pitchFineId);
    scaleLockParam = apvts.getRawParameterValue (scaleLockId);
    scaleParam = apvts.getRawParameterValue (scaleId);
    glideParam = apvts.getRawParameterValue (glideId);
    levelParam = apvts.getRawParameterValue (levelId);
    velocityCurveParam = apvts.getRawParameterValue (velocityCurveId);
    attackParam = apvts.getRawParameterValue (attackId);
    releaseParam = apvts.getRawParameterValue (releaseId);
    tapeStopTimeParam = apvts.getRawParameterValue (tapeStopTimeId);
    tapeStopCurveParam = apvts.getRawParameterValue (tapeStopCurveId);
    memoryAmountParam = apvts.getRawParameterValue (memoryAmountId);
    memoryDecayParam = apvts.getRawParameterValue (memoryDecayId);
    driftAmountParam = apvts.getRawParameterValue (driftAmountId);
    driftSpeedParam = apvts.getRawParameterValue (driftSpeedId);
    layBackParam = apvts.getRawParameterValue (layBackId);
    swingParam = apvts.getRawParameterValue (swingId);
    humaniseParam = apvts.getRawParameterValue (humaniseId);
    ghostOnParam = apvts.getRawParameterValue (ghostOnId);
    ghostThresholdParam = apvts.getRawParameterValue (ghostThresholdId);
    ghostLevelParam = apvts.getRawParameterValue (ghostLevelId);
    ghostDelayParam = apvts.getRawParameterValue (ghostDelayId);
    sliceCountParam = apvts.getRawParameterValue (sliceCountId);
    grainSizeParam = apvts.getRawParameterValue (grainSizeId);
    grainDensityParam = apvts.getRawParameterValue (grainDensityId);
    grainSpreadParam = apvts.getRawParameterValue (grainSpreadId);
    sustainHoldParam = apvts.getRawParameterValue (sustainHoldId);
    swellTimeParam = apvts.getRawParameterValue (swellTimeId);
    swellCrossfadeParam = apvts.getRawParameterValue (swellCrossfadeId);
    dustParam = apvts.getRawParameterValue (dustId);
    crackleParam = apvts.getRawParameterValue (crackleId);
    wowParam = apvts.getRawParameterValue (wowId);
    flutterParam = apvts.getRawParameterValue (flutterId);
    shuffleSeedParam = apvts.getRawParameterValue (shuffleSeedId);
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout SgraffitoProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto percent = [] (float v, int) { return juce::String (v, 0) + " %"; };
    auto percentParse = [] (const juce::String& t) { return t.getFloatValue(); };
    auto ms = [] (float v, int) { return juce::String (v, 0) + " ms"; };
    auto msParse = [] (const juce::String& t) { return t.getFloatValue(); };
    auto db = [] (float v, int) { return juce::String (v, 1) + " dB"; };
    auto dbParse = [] (const juce::String& t) { return t.getFloatValue(); };
    auto hz = [] (float v, int) { return juce::String (v, 2); };
    auto hzParse = [] (const juce::String& t) { return t.getFloatValue(); };
    auto cents = [] (float v, int) { return juce::String (v, 0) + " ct"; };
    auto centsParse = [] (const juce::String& t) { return t.getFloatValue(); };

    // Mode: Scrub, Chop, Cloud, Swell
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { modeId, 1 }, "Mode",
        juce::StringArray { "Scrub", "Chop", "Cloud", "Swell" }, 0));

    // Region
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { regionSpanId, 1 }, "Region Span",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, "%"));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { regionLengthId, 1 }, "Region Length",
        juce::NormalisableRange<float> (1.0f, 100.0f, 1.0f), 25.0f, "%"));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { rootKeyId, 1 }, "Root Key",
        juce::NormalisableRange<float> (24.0f, 96.0f, 1.0f), 60.0f));

    // Pitch
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pitchFineId, 1 }, "Pitch Fine",
        juce::NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 0.0f, "ct"));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { scaleLockId, 1 }, "Scale Lock", false));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { scaleId, 1 }, "Scale",
        juce::StringArray { "Chromatic", "Major", "Minor", "Minor Pentatonic", "Dorian", "Whole Tone" },
        2));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { glideId, 1 }, "Glide",
        juce::NormalisableRange<float> (0.0f, 500.0f, 1.0f), 50.0f, "ms"));

    // Level
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { levelId, 1 }, "Level",
        juce::NormalisableRange<float> (-60.0f, 6.0f, 0.1f), 0.0f, "dB"));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { velocityCurveId, 1 }, "Velocity Curve",
        juce::NormalisableRange<float> (0.5f, 2.0f, 0.1f), 1.0f));

    // Envelope
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { attackId, 1 }, "Attack",
        juce::NormalisableRange<float> (1.0f, 500.0f, 1.0f), 5.0f, "ms"));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { releaseId, 1 }, "Release",
        juce::NormalisableRange<float> (10.0f, 5000.0f, 1.0f), 200.0f, "ms"));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { tapeStopTimeId, 1 }, "Tape Stop Time",
        juce::NormalisableRange<float> (0.0f, 5000.0f, 1.0f), 400.0f, "ms"));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { tapeStopCurveId, 1 }, "Tape Stop Curve",
        juce::StringArray { "Linear", "Exponential", "Slow Then Fast" }, 1));

    // Memory
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { memoryAmountId, 1 }, "Memory Amount",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 20.0f, "%"));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { memoryDecayId, 1 }, "Memory Decay",
        juce::NormalisableRange<float> (0.0f, 60.0f, 1.0f), 10.0f, "s"));

    // Drift
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { driftAmountId, 1 }, "Drift Amount",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f, "%"));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { driftSpeedId, 1 }, "Drift Speed",
        juce::NormalisableRange<float> (0.01f, 2.0f, 0.01f), 0.1f));

    // Timing
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { layBackId, 1 }, "Lay Back",
        juce::NormalisableRange<float> (0.0f, 30.0f, 1.0f), 0.0f, "ms"));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { swingId, 1 }, "Swing",
        juce::NormalisableRange<float> (50.0f, 75.0f, 1.0f), 50.0f, "%"));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { humaniseId, 1 }, "Humanise",
        juce::NormalisableRange<float> (0.0f, 15.0f, 1.0f), 0.0f, "ms"));

    // Ghosts
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ghostOnId, 1 }, "Ghost On", false));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ghostThresholdId, 1 }, "Ghost Threshold",
        juce::NormalisableRange<float> (1.0f, 100.0f, 1.0f), 40.0f, "%"));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ghostLevelId, 1 }, "Ghost Level",
        juce::NormalisableRange<float> (-40.0f, 0.0f, 1.0f), -12.0f, "dB"));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ghostDelayId, 1 }, "Ghost Delay",
        juce::NormalisableRange<float> (0.0f, 50.0f, 1.0f), 15.0f, "ms"));

    // Chop
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { sliceCountId, 1 }, "Slice Count",
        juce::NormalisableRange<float> (2.0f, 32.0f, 1.0f), 16.0f));

    // Cloud
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { grainSizeId, 1 }, "Grain Size",
        juce::NormalisableRange<float> (5.0f, 300.0f, 1.0f), 60.0f, "ms"));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { grainDensityId, 1 }, "Grain Density",
        juce::NormalisableRange<float> (1.0f, 60.0f, 1.0f), 12.0f, "/s"));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { grainSpreadId, 1 }, "Grain Spread",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 40.0f, "%"));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { sustainHoldId, 1 }, "Sustain Hold", false));

    // Swell
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { swellTimeId, 1 }, "Swell Time",
        juce::NormalisableRange<float> (50.0f, 4000.0f, 1.0f), 800.0f, "ms"));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { swellCrossfadeId, 1 }, "Swell Crossfade",
        juce::NormalisableRange<float> (5.0f, 200.0f, 1.0f), 40.0f, "ms"));

    // Vinyl
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { dustId, 1 }, "Dust",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { crackleId, 1 }, "Crackle",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { wowId, 1 }, "Wow",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { flutterId, 1 }, "Flutter",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f));

    // Map (state-only, not automated)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shuffleSeedId, 1 }, "Shuffle Seed",
        juce::NormalisableRange<float> (0.0f, 9999.0f, 1.0f), 1.0f));

    return layout;
}

//==============================================================================
void SgraffitoProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    preparedSampleRate = sampleRate;

    // Reset all voices.
    for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
        voicePool.voices[i] = CoreDSP::Sampler::Voice {};
}

bool SgraffitoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void SgraffitoProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midiMessages)
{
    const int numSamples = buffer.getNumSamples();
    juce::ignoreUnused (numSamples);

    // Clear the output.
    buffer.clear();

    // Process MIDI messages first: note-on allocates a voice, note-off releases it.
    // This must happen even without a sample loaded, so voices are tracked correctly.
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();

        if (msg.isNoteOn())
        {
            const int key = msg.getNoteNumber();
            const float velocity = msg.getVelocity() / 127.0f;
            voicePool.allocateVoice (key, velocity);
        }
        else if (msg.isNoteOff())
        {
            const int key = msg.getNoteNumber();
            for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
            {
                if (voicePool.voices[i].active && voicePool.voices[i].key == key)
                {
                    voicePool.releaseVoice (i, metadata.samplePosition);
                    break;
                }
            }
        }
    }

    midiMessages.clear();

    // Get the current sample buffer (atomic read).
    const auto* sample = getSampleBuffer();
    if (sample == nullptr || sample->getNumSamples() == 0)
        return;
}

void SgraffitoProcessor::applyPresetState (const juce::ValueTree& tree)
{
    // Presets use resetMissing=true so absent parameters are set to default.
    const juce::MemoryBlock block = SuiteState::detail::encodeXml (*tree.createXml());
    const auto result = SuiteState::read (block.getData(), (int) block.getSize(), apvts,
                                          stateVersion, migrations, {}, true);
    juce::ignoreUnused (result);
}

void SgraffitoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    destData = SuiteState::write (apvts, stateVersion);
}

void SgraffitoProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    SuiteState::read (data, sizeInBytes, apvts, stateVersion, migrations);
}

juce::AudioProcessorEditor* SgraffitoProcessor::createEditor()
{
    return new SgraffitoEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SgraffitoProcessor();
}
