#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

#include "SuiteState.h"

#include <algorithm>
#include <cmath>

#include <juce_audio_utils/juce_audio_utils.h>

namespace
{
// Deterministic per-note unit for the humanise offset (plan 05.4): an LCG that starts from a
// fixed seed, so repeated runs of the same note sequence shift by the same amount.
inline float nextHumaniseUnit (std::uint32_t& state) noexcept
{
    state = state * 1664525u + 1013904223u;
    return static_cast<float> ((state >> 8) & 0x00FFFFFFu) / 16777216.0f;
}
}

//==============================================================================
SgraffitoProcessor::SgraffitoProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout()),
        modulation(apvts, SgraffitoProcessor::modulationTargets, SgraffitoProcessor::numModulationTargets, nullptr),
        voicePool {}, lorenz {}  // Voice pool and Lorenz attractor
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

    levelParam = apvts.getRawParameterValue(levelId);
    velocityCurveParam = apvts.getRawParameterValue(velocityCurveId);
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
//==============================================================================
// Note handling shared by the host MIDI and the editor keyboard (plan 05.1).
void SgraffitoProcessor::queueGhost (int key, float ghostVelocity, double samplePosition, double ghostDelaySamples, double mainDelaySamples)
{
    // Ghosts are one-shot: a live queued ghost consumes one of the maxGhostVoices slots, and it
    // frees the slot again when its delayed copy fires (the fired ghost allocates a real voice).
    int liveGhosts = 0;
    for (const auto& note : pendingNotes)
        if (note.ghost)
            ++liveGhosts;

    if (liveGhosts >= maxGhostVoices)
        return;

    PendingNote ghost;
    ghost.ghost = true;
    ghost.key = key;
    ghost.velocity = ghostVelocity;
    ghost.samplePosition = samplePosition;
    ghost.delaySamples = ghostDelaySamples;
    ghost.scheduledPos = (double) streamSample + samplePosition + mainDelaySamples + ghostDelaySamples;
    pendingNotes.push_back (ghost);
}

void SgraffitoProcessor::handleNoteOn (int keyIn, float velocity, int samplePosition,
    float rootKeyMod, float layBackMod,
    float spanMod, float lengthMod, float humaniseMod,
    std::atomic<float>* sustainHoldParam, float releaseMs)
{
    int key = keyIn;
    
    auto* samplePtr = getSampleBuffer();
    if (samplePtr == nullptr || samplePtr->getNumSamples() == 0)
        return;

    if (scaleLockParam->load() > 0.5f)
    {
        const int rootKey = (int) std::lround (rootKeyMod);
        const int scaleIndex = (int) std::lround (scaleParam->load());
        key = CoreDSP::Sampler::quantizeToScale (key, rootKey, scaleIndex);
    }

    // If the note start is delayed (lay-back, swing, humanise), queue it for later playback.
    // The delay is the sum of the lay-back, the swing delay (repeated notes of a key) and the
    // humanise offset (plan 05.4).
    const float humaniseMs = humaniseMod;
    const float humaniseUnit = nextHumaniseUnit (humaniseSeed);
    float delayMs = layBackMod + humaniseUnit * humaniseMs;

    // Swing: repeated notes of the same key alternate straight/swung (plan 05.4). Swing 50 % is
    // straight, 75 % delays the swung copy by half a beat at the current tempo, linearly between.
    const std::size_t keyIndex = static_cast<std::size_t> (juce::jlimit (0, 127, key));
    const bool swung = swingState[keyIndex] != 0;
    swingState[keyIndex] = swung ? 0 : 1;

    if (swung)
    {
        const double tempoBpm = SuiteModulation::hostTempoOrDefault (getPlayHead ());
        const double halfBeatSamples = 30.0 / tempoBpm * preparedSampleRate;
        const float swungFraction = (swingParam->load () - 50.0f) / (75.0f - 50.0f);
        const double swungDelaySamples = std::min (halfBeatSamples * static_cast<double> (juce::jmax (0.0f, swungFraction)),
                                                    halfBeatSamples);
        delayMs += static_cast<float> (swungDelaySamples / preparedSampleRate * 1000.0);
    }

    const double mainDelaySamples = delayMs > 0.0f ? delayMs * preparedSampleRate / 1000.0 : 0.0;

    // Ghost notes (plan 05.4 Phase 2): a quieter, delayed copy of this note when it plays below
    // the ghost threshold. The ghost carries its own delay past the note's fire time and allocates
    // a real voice when the delayed copy comes due (one-shot).
    const bool ghostWanted = ghostOnParam->load () > 0.5f
                          && velocity < ghostThresholdParam->load () / 100.0f;
    const float ghostVelocity = ghostWanted
        ? std::pow (10.0, ghostLevelParam->load () / 20.0) * velocity
        : velocity;
    const double ghostDelaySamples = ghostDelayParam->load () * preparedSampleRate / 1000.0;

    if (delayMs > 0.0f)
    {
        PendingNote pending;
        pending.key = key;
        pending.velocity = velocity;
        pending.samplePosition = static_cast<double> (samplePosition);
        pending.delaySamples = mainDelaySamples;
        pending.scheduledPos = (double) streamSample + pending.samplePosition + pending.delaySamples;
        pendingNotes.push_back (pending);

        if (ghostWanted)
            queueGhost (key, ghostVelocity, (double) samplePosition, ghostDelaySamples, mainDelaySamples);
    }
    else
    {
        const int idx = voicePool.allocateVoice (key, velocity, spanMod, lengthMod,
                                                 modeParam->load(), sliceCountParam->load());

        if (idx >= 0)
        {
            // Pitch: the key's distance from middle C, plus the fine tune in cents
            // (plan 05.1: the key must change the pitch, and PITCH FINE must be audible).
            const double semitones = (double) (key - 60) + (double) pitchFineParam->load() / 100.0;
            voicePool.voices[idx].rate = std::pow (2.0, semitones / 12.0);

            // Velocity curve: the curve is the exponent of the velocity (1.0 = linear).
            const float curve = juce::jmax (0.1f, velocityCurveParam->load());
            voicePool.voices[idx].velocity = std::pow (juce::jlimit (0.0f, 1.0f, velocity), curve);

            // The voice reads from the region's start: allocateVoice does not set the
            // position, and without this every note read the sample from sample 0
            // (the "always sounds the same" bug, plan 05.1).
            if (auto* sampleNow = getSampleBuffer())
                voicePool.voices[idx].position = voicePool.voices[idx].regionStart
                                                 * (double) sampleNow->getNumSamples();

            // Sustain Hold: latch release when note is held. If sustain hold is on, the voice
            // continues playing after note-off until the next note of the same key or a timeout.
            if (getModulatedValue(ModTarget::SustainHold) > 0.5f)
            {
                voicePool.voices[idx].sustainActive = true;
                voicePool.voices[idx].sustainKey = key;
                voicePool.voices[idx].sustainTimeout = getModulatedValue(ModTarget::Release);
                // The note will be released when a new note of the same key arrives or timeout expires.
            }
        }
        if (ghostWanted)
            queueGhost (key, ghostVelocity, (double) samplePosition, ghostDelaySamples, 0.0);
    }
}

void SgraffitoProcessor::handleNoteOff (int key, int samplePosition, bool sustainOn, float releaseMs, double blockStartSample, juce::AudioParameterFloat* tapeStopTimeParam, juce::AudioParameterFloat* tapeStopCurveParam)
{
    // A new phrase of repeated notes of this key starts straight again (plan 05.4).
    swingState[static_cast<std::size_t> (juce::jlimit (0, 127, key))] = 0;

    // Sustain Hold: latch release when note is held. If sustain hold is on and the voice
    // belongs to the same key as this note-off, the voice continues playing until a new
    // note of the same key arrives or the timeout expires (plan 05.4 Phase 3).
    if (sustainOn)
    {
        for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
        {
            auto& voice = voicePool.voices[i];
            if (voice.active && voice.key == key && !voice.sustainActive)
            {
                // Mark this voice as sustain-active so it will be released by a new note of the same key.
                voice.sustainActive = true;
                break;
            }
        }
    }

    // Sustain Hold: process each active voice matching this key.

    for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
    {
        auto& voice = voicePool.voices[i];
        if (!voice.active)
            continue;

        // If the voice belongs to a different key, skip it.
        if (voice.key != key)
            continue;

        // Sustain hold: if this voice is already marked as sustain-active, check for timeout.
        if (voice.sustainActive)
        {
            const double now = blockStartSample + static_cast<double>(i);
            const double timeoutSamples = voice.sustainTimeout * preparedSampleRate;

            // If the note has been held long enough, release it.
            if ((now - voice.envStart) > timeoutSamples)
            {
                voice.active = false;
                continue;
            }
        }
        else
        {
            // First time seeing this key with sustain hold on: mark as sustain-active and release.
            voice.sustainActive = true;
                const double sampleIndex = static_cast<double>(samplePosition);
                voicePool.releaseVoice (i, sampleIndex, tapeStopTimeParam->get(), tapeStopCurveParam->get());
            continue;
        }

        // Voice is active and either not sustain-held or timeout hasn't expired.
        const double sampleIndex = static_cast<double>(samplePosition);
        voicePool.releaseVoice (i, sampleIndex, tapeStopTimeParam->get(), tapeStopCurveParam->get());
    }
}

juce::StringArray SgraffitoProcessor::modTargetNames(){
    juce::StringArray names { "None" };

    for (int i = 0; i < numModulationTargets; ++i)
        names.add (modulationTargets[i].name);

    return names;
}

juce::AudioProcessorValueTreeState::ParameterLayout SgraffitoProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Mode: Scrub, Chop, Cloud, Swell
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { modeId, 1 }, "Mode",
        juce::StringArray { "Scrub", "Chop", "Cloud", "Swell" }, 0));

    // Region
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { regionSpanId, 1 }, "Region Span",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, "%"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { regionLengthId, 1 }, "Region Length",
        juce::NormalisableRange<float> (1.0f, 100.0f, 1.0f), 25.0f, "%"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { rootKeyId, 1 }, "Root Key",
        juce::NormalisableRange<float> (24.0f, 96.0f, 1.0f), 60.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float value, int)
            {
                const int note = juce::roundToInt (value);
                return juce::MidiMessage::getMidiNoteName (note, true, true, 3) + " (" + juce::String (note) + ")";
            })));

    // Pitch
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pitchFineId, 1 }, "Pitch Fine",
        juce::NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 0.0f, "ct"));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { scaleLockId, 1 }, "Scale Lock", false));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { scaleId, 1 }, "Scale",
        juce::StringArray { "Chromatic", "Major", "Minor", "Minor Pentatonic", "Dorian", "Whole Tone" },
        2));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { glideId, 1 }, "Glide",
        juce::NormalisableRange<float> (0.0f, 500.0f, 1.0f), 50.0f, "ms"));

    // Level
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { levelId, 1 }, "Level",
        juce::NormalisableRange<float> (-60.0f, 6.0f, 0.1f), 0.0f, "dB"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { velocityCurveId, 1 }, "Velocity Curve",
        juce::NormalisableRange<float> (0.5f, 2.0f, 0.1f), 1.0f));

    // Envelope
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { attackId, 1 }, "Attack",
        juce::NormalisableRange<float> (1.0f, 500.0f, 1.0f), 5.0f, "ms"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { releaseId, 1 }, "Release",
        juce::NormalisableRange<float> (10.0f, 5000.0f, 1.0f), 200.0f, "ms"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { tapeStopTimeId, 1 }, "Tape Stop Time",
        juce::NormalisableRange<float> (0.0f, 5000.0f, 1.0f), 400.0f, "ms"));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { tapeStopCurveId, 1 }, "Tape Stop Curve",
        juce::StringArray { "Linear", "Exponential", "Slow Then Fast" }, 1));

    // Memory
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { memoryAmountId, 1 }, "Memory Amount",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 20.0f, "%"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { memoryDecayId, 1 }, "Memory Decay",
        juce::NormalisableRange<float> (0.0f, 60.0f, 1.0f), 10.0f, "s"));

    // Drift
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { driftAmountId, 1 }, "Drift Amount",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f, "%"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { driftSpeedId, 1 }, "Drift Speed",
        juce::NormalisableRange<float> (0.01f, 2.0f, 0.01f), 0.1f));

    // Timing
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { layBackId, 1 }, "Lay Back",
        juce::NormalisableRange<float> (0.0f, 30.0f, 1.0f), 0.0f, "ms"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { swingId, 1 }, "Swing",
        juce::NormalisableRange<float> (50.0f, 75.0f, 1.0f), 50.0f, "%"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { humaniseId, 1 }, "Humanise",
        juce::NormalisableRange<float> (0.0f, 15.0f, 1.0f), 0.0f, "ms"));

    // Ghosts
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ghostOnId, 1 }, "Ghost On", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ghostThresholdId, 1 }, "Ghost Threshold",
        juce::NormalisableRange<float> (1.0f, 100.0f, 1.0f), 40.0f, "%"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ghostLevelId, 1 }, "Ghost Level",
        juce::NormalisableRange<float> (-40.0f, 0.0f, 1.0f), -12.0f, "dB"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ghostDelayId, 1 }, "Ghost Delay",
        juce::NormalisableRange<float> (0.0f, 50.0f, 1.0f), 15.0f, "ms"));

    // Chop
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { sliceCountId, 1 }, "Slice Count",
        juce::NormalisableRange<float> (2.0f, 32.0f, 1.0f), 16.0f));

    // Cloud
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { grainSizeId, 1 }, "Grain Size",
        juce::NormalisableRange<float> (5.0f, 300.0f, 1.0f), 60.0f, "ms"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { grainDensityId, 1 }, "Grain Density",
        juce::NormalisableRange<float> (1.0f, 60.0f, 1.0f), 12.0f, "/s"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { grainSpreadId, 1 }, "Grain Spread",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 40.0f, "%"));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { sustainHoldId, 1 }, "Sustain Hold", false));

    // Swell
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { swellTimeId, 1 }, "Swell Time",
        juce::NormalisableRange<float> (50.0f, 4000.0f, 1.0f), 800.0f, "ms"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { swellCrossfadeId, 1 }, "Swell Crossfade",
        juce::NormalisableRange<float> (5.0f, 200.0f, 1.0f), 40.0f, "ms"));

    // Vinyl
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { dustId, 1 }, "Dust",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { crackleId, 1 }, "Crackle",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { wowId, 1 }, "Wow",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { flutterId, 1 }, "Flutter",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f));

    // Map (state-only, not automated)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { shuffleSeedId, 1 }, "Shuffle Seed",
        juce::NormalisableRange<float> (0.0f, 9999.0f, 1.0f), 1.0f));

    // The two LFOs and the six routing slots, appended last so the sound-parameter
    // order never moves (SuiteModulation::Router).
    SuiteModulation::Router::addParameters (params, modTargetNames());

    return { params.begin(), params.end() };
}

//==============================================================================
void SgraffitoProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    preparedSampleRate = sampleRate;

    modulation.prepare (sampleRate);
    streamSample = 0;
    beatClock = 0.0;

    // First call: load the bundled factory sample so the plugin makes sound, and the
    // editor's waveform view has something to show, out of the box. A sample the user
    // loaded later is left alone.
    if (loadedSample == nullptr)
        loadEmbeddedSample ("embedded:grain_4k.wav");

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

    // Clear the output.
    buffer.clear();

    // Advance the clock that drives the free LFOs and the synced beat position.
    // No host tempo (including the Standalone): a fixed 120 BPM keeps the synced LFOs moving.
    auto advanceClocks = [this, numSamples]() -> void
    {
        beatClock += (double) numSamples * SuiteModulation::hostTempoOrDefault (getPlayHead()) / 60.0 / preparedSampleRate;
        streamSample += numSamples;
        modulation.advanceBlock (numSamples);
    };

    // The LFOs run on an internal beat clock (a sampler has no sequencer position to follow).
    const auto offsets = modulation.computeOffsets ((double) streamSample, beatClock, (double) streamSample);
    modulation.publish (offsets, (double) streamSample, beatClock);

    auto modulated = [this, &offsets] (ModTarget target, float base)
    {
        return modulation.modulate ((int) target, base, offsets);
    };

    const float releaseMs = modulated (ModTarget::Release, releaseParam->load());
    // The absolute sample position of this block's first sample: the envelope's time base.
    // (streamSample advances at the end of the block; the old block-relative base stuck the
    // envelope at the attack of the first block forever.)
    const double blockStartSample = (double) streamSample;

    auto* tapeStopTimeParameter = static_cast<juce::AudioParameterFloat*> (apvts.getParameter (tapeStopTimeId));
    auto* tapeStopCurveParameter = static_cast<juce::AudioParameterFloat*> (apvts.getParameter (tapeStopCurveId));

    // Process MIDI messages first: note-on allocates a voice, note-off releases it.
    for (const auto& metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();

        if (msg.isNoteOn())
            handleNoteOn (msg.getNoteNumber(), msg.getVelocity() / 127.0f,
                          metadata.samplePosition,
                          modulated (ModTarget::RootKey, rootKeyParam->load()),
                          modulated (ModTarget::LayBack, layBackParam->load()),
                          modulated (ModTarget::RegionSpan, regionSpanParam->load()),
                          modulated (ModTarget::RegionLength, regionLengthParam->load()),
                          modulated (ModTarget::Humanise, humaniseParam->load()),
                          sustainHoldParam, releaseMs);
        else if (msg.isNoteOff())
            handleNoteOff (msg.getNoteNumber(), (int) (streamSample + metadata.samplePosition),
                           modulated (ModTarget::SustainHold, sustainHoldParam->load()) > 0.5f,
                           releaseMs, blockStartSample, tapeStopTimeParameter, tapeStopCurveParameter);
    }
    
    midiMessages.clear();

    // The editor keyboard's notes run through the same handling (SPSC ring, plan 05.1).
    for (;;)
    {
        const int read = uiNoteRead.load (std::memory_order_acquire);
        if (read == uiNoteWrite.load (std::memory_order_acquire))
            break;

        const auto note = uiNoteRing[(size_t) read];
        uiNoteRead.store ((read + 1) % uiNoteCapacity, std::memory_order_release);

        if (note.on)
            handleNoteOn (note.key, note.velocity, 0,
                          modulated (ModTarget::RootKey, rootKeyParam->load()),
                          modulated (ModTarget::LayBack, layBackParam->load()),
                          modulated (ModTarget::RegionSpan, regionSpanParam->load()),
                          modulated (ModTarget::RegionLength, regionLengthParam->load()),
                          modulated (ModTarget::Humanise, humaniseParam->load()),
                          sustainHoldParam, releaseMs);
        else
            handleNoteOff (note.key, 0, modulated (ModTarget::SustainHold, sustainHoldParam->load()) > 0.5f, releaseMs, blockStartSample, tapeStopTimeParameter, tapeStopCurveParameter);
    }

    // Get the current sample buffer (atomic read).
    auto* samplePtr = getSampleBuffer();
    if (samplePtr == nullptr || samplePtr->getNumSamples() == 0)
    {
        advanceClocks();
        return;
    }

    const auto* sample = static_cast<const juce::AudioBuffer<float>*>(samplePtr);
    const int numChannels = buffer.getNumChannels();

    // Advance Lorenz attractor by driftSpeed * seconds in this block.
    const float driftSpeed = modulated (ModTarget::DriftSpeed, driftSpeedParam->load());
    const double advanceSeconds = static_cast<double> (numSamples) / preparedSampleRate;
    lorenz.advance (driftSpeed * advanceSeconds);
    // Chaos drift is not wired to the audio yet (a follow-up plan owns it, like the
    // other unwired texture parameters documented in AGENTS.md).

    for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
    {
        auto& voice = voicePool.voices[i];
        if (! voice.active)
            continue;

        // Advance the tape-stop ramp; the per-sample loop keeps playing at voice.rate.
        if (voice.tapeStopping)
            voice.tapeStop.tick (preparedSampleRate);

        // Apply memory trace: the note's layer fades over the memory decay time, once per
        // block (memoryDecay seconds is the time constant).
        if (modulated (ModTarget::MemoryAmount, memoryAmountParam->load()) > 0.5f)
        {
            const double tau = juce::jmax (0.01, (double) memoryDecayParam->load());  // seconds
            voice.gain *= (float) std::exp (- (double) numSamples / (tau * preparedSampleRate));
        }

        // Swell reads backwards from the region end in the per-sample loop below.
        // The swellTime/swellCrossfade crossfade is not wired to the audio yet
        // (documented in AGENTS.md; a follow-up plan owns it).

        // Surface effects: per-block modulation of playback rate and level for dust/crackle/wow/flutter.
        // Moved from per-sample to block-rate for efficiency and stability (plan 05.2).
        // These are applied after the main processing, before output.
        if (modulated (ModTarget::Dust, apvts.getRawParameterValue(dustId)->load()) > 0.5f)
        {
            voice.surfaceState.phaseDust += 0.1;
            if (voice.surfaceState.phaseDust >= 1.0)
                voice.surfaceState.phaseDust -= 1.0;
            const double phaseInc = voice.surfaceState.phaseDust * 0.001f;
            voice.position += phaseInc;
        }
        if (modulated (ModTarget::Crackle, apvts.getRawParameterValue(crackleId)->load()) > 0.5f)
        {
            voice.surfaceState.phaseCrackle += 10.0;
            if (voice.surfaceState.phaseCrackle >= 1.0)
                voice.surfaceState.phaseCrackle -= 1.0;
            const double phaseInc = voice.surfaceState.phaseCrackle * 0.01f;
            voice.gain *= std::clamp (1.0 + phaseInc * 0.3, 0.7, 1.3);
        }
        if (modulated (ModTarget::Wow, apvts.getRawParameterValue(wowId)->load()) > 0.5f)
        {
            voice.surfaceState.phaseWow += 0.5;
            if (voice.surfaceState.phaseWow >= 1.0)
                voice.surfaceState.phaseWow -= 1.0;
            const double phaseInc = voice.surfaceState.phaseWow * 0.0005f;
            voice.rate *= std::clamp (1.0 + phaseInc * 0.2, 0.8, 1.2);
        }
        if (modulated (ModTarget::Flutter, apvts.getRawParameterValue(flutterId)->load()) > 0.5f)
        {
            voice.surfaceState.phaseFlutter += 5.0;
            if (voice.surfaceState.phaseFlutter >= 1.0)
                voice.surfaceState.phaseFlutter -= 1.0;
            const double phaseInc = voice.surfaceState.phaseFlutter * 0.005f;
            voice.gain *= std::clamp (1.0 + phaseInc * 0.5, 0.5, 1.5);
        }

        // Release voice if tape stop complete after full block.
        if (voice.tapeStopping && ! voice.tapeStop.active())
        {
            voice.active = false;
            voice.tapeStopping = false;
        }
    }

    // Lay back, swing and humanise: fire the pending notes whose scheduled position is due
    // in this block. A swing delay (up to half a beat) or a humanise offset can cross a block
    // boundary, so notes that are not due yet stay queued (plan 05.4).
    if (! pendingNotes.empty())
    {
        const double blockEndSample = (double) streamSample + static_cast<double> (numSamples);

        for (size_t i = 0; i < pendingNotes.size(); )
        {
            PendingNote& metadata = pendingNotes[i];
            const double scheduledPos = metadata.scheduledPos;

            if (scheduledPos > blockEndSample)
            {
                ++i;
                continue;
            }

            const int key = std::clamp(static_cast<int>(metadata.key), 0, 127);
            const float velocity = metadata.velocity;

            if (metadata.ghost)
            {
                // Ghosts allocate a real voice through the pool at fire time, so stealing and
                // regions stay consistent with a directly played note (plan 05.4 Phase 2).
                const double ghostSpan = modulated (ModTarget::RegionSpan, regionSpanParam->load ());
                const double ghostLength = modulated (ModTarget::RegionLength, regionLengthParam->load ());
                const int ghostIdx = voicePool.allocateVoice (key, velocity, ghostSpan, ghostLength,
                                                               std::lround (modeParam->load ()),
                                                               static_cast<int> (juce::jmax (0.0f, sliceCountParam->load ())),
                                                               scheduledPos, key);
                if (ghostIdx >= 0)
                {
                    auto& ghostVoice = voicePool.voices[ghostIdx];
                    const float ghostVelCurve = juce::jmax (0.1f, velocityCurveParam->load ());
                    ghostVoice.velocity = std::pow (juce::jlimit (0.0f, 1.0f, velocity), ghostVelCurve);
                    ghostVoice.envStart = scheduledPos;
                    ghostVoice.envValue = 1.0f;
                    ghostVoice.position = ghostVoice.regionStart * (double) sample->getNumSamples();
                }

                pendingNotes.erase (pendingNotes.begin () + static_cast<std::ptrdiff_t> (i));
                continue;
            }

            // Find or allocate a voice for this note.
            int idx = -1;
            for (int n = 0; n < CoreDSP::Sampler::maxVoices; ++n)
            {
                const bool isAvailable = ! voicePool.voices[n].active;
                if (isAvailable)
                {
                    idx = n;
                    break;
                }
            }

            if (idx >= 0)
            {
                voicePool.voices[idx] = CoreDSP::Sampler::Voice{};
                voicePool.voices[idx].active = true;
                voicePool.voices[idx].key = key;
                voicePool.voices[idx].velocity = velocity;

                // Pitch and velocity curve, as in the direct path (plan 05.1).
                const double semitones = (double) (key - 60) + (double) pitchFineParam->load() / 100.0;
                voicePool.voices[idx].rate = std::pow (2.0, semitones / 12.0);
                const float velCurve = juce::jmax (0.1f, velocityCurveParam->load());
                voicePool.voices[idx].velocity = std::pow (juce::jlimit (0.0f, 1.0f, velocity), velCurve);
                
                // Reset envEnd-related fields to prevent stale values from previous note
                voicePool.voices[idx].envEnd = -1.0;
                voicePool.voices[idx].envEndSet = false;
                
                voicePool.voices[idx].envStart = scheduledPos;
                voicePool.voices[idx].envValue = 1.0f;

                // Compute region for this note through the shared engine maps, so this
                // path produces exactly the regions of VoicePool::allocateVoice.
                double regionStart = 0.0, regionEnd = 1.0;

                const int mode = std::lround (modeParam->load());
                if (mode == 1)  // Chop mode
                {
                    const int sliceCountClamped = std::clamp (static_cast<int> (sliceCountParam->load()), 2, CoreDSP::Sampler::maxRegions);
                    const int sliceIndex = std::clamp (key * sliceCountClamped / CoreDSP::Sampler::maxKeys, 0, sliceCountClamped - 1);
                    const CoreDSP::Sampler::Region region = CoreDSP::Sampler::chopRegion (sliceIndex, sliceCountClamped);
                    regionStart = region.start;
                    regionEnd = region.start + region.length;
                }
                else  // Scrub / Cloud / Swell: linear mapping through the span.
                {
                    const CoreDSP::Sampler::Region region = CoreDSP::Sampler::keyRegion (key, 60,
                        modulated (ModTarget::RegionSpan, regionSpanParam->load()) / 100.0,
                        modulated (ModTarget::RegionLength, regionLengthParam->load()) / 100.0);
                    regionStart = region.start;
                    regionEnd = region.start + region.length;
                }

                voicePool.voices[idx].regionStart = regionStart;
                voicePool.voices[idx].regionEnd = regionEnd;
                voicePool.voices[idx].envStart = scheduledPos;
                voicePool.voices[idx].position = regionStart * sample->getNumSamples();

                // If the scheduled position is in the past (relative to block start), play it now.
                if (scheduledPos <= blockStartSample)
                {
                    voicePool.voices[idx].envStart = blockStartSample;
                    voicePool.voices[idx].position = regionStart * sample->getNumSamples();
                }
            }

            pendingNotes.erase (pendingNotes.begin () + static_cast<std::ptrdiff_t> (i));
        }
    }

    // AUDIO PLAYBACK: read from sample buffer and write to output.
    // This is the critical missing piece - we must actually play audio!
    if (samplePtr != nullptr && samplePtr->getNumSamples() > 0)
    {
        const auto* sample = static_cast<const juce::AudioBuffer<float>*>(samplePtr);
        const int sampleChannels = std::max (1, sample->getNumChannels());
        const double sampleLength = (double) sample->getNumSamples();

        // Cloud grain timing: grain size in milliseconds, density in grains per second.
        const double grainSizeSamples = juce::jmax (1.0, (double) grainSizeParam->load() * preparedSampleRate / 1000.0);
        const double grainInterval = juce::jmax (1.0, preparedSampleRate / (double) juce::jmax (1.0f, grainDensityParam->load()));
        const double grainSpread = juce::jlimit (0.0, 1.0, (double) grainSpreadParam->load() / 100.0);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* outPtr = buffer.getWritePointer(ch);
            // A mono sample feeds every output channel (the embedded factory sample is mono).
            const int srcCh = ch % sampleChannels;
            const float* sampleData = sample->getReadPointer(srcCh);
            
            // Process each output sample - voices contribute to all samples in the block
            for (int n = 0; n < numSamples; ++n)
            {
                float outputValue = 0.0f;
                
                const double currentSampleIdx = blockStartSample + static_cast<double>(n);
                
                // Process each active voice for this sample
                for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
                {
                    auto& voice = voicePool.voices[i];
                    if (!voice.active)
                        continue;
                    
                    // Get envelope value at this sample
                    const float envValue = voice.envelope(currentSampleIdx, attackParam->load(), releaseMs, preparedSampleRate);
                    
                    // Cache the current envelope value: releaseVoice reads it as the release level,
                    // so a note-off fades from where the envelope actually is.
                    voice.envValue = envValue;
                    
                    // The release envelope has decayed to zero: the voice is done.
                    if (envValue <= 0.0f && voice.envEnd > 0.0)
                    {
                        voice.active = false;
                        continue;
                    }

                    if (envValue <= 0.0f)
                        continue;

                    // Swell starts its backwards read at the region end; the other modes
                    // already have their frame position from allocation.
                    if (! voice.positionInitialised)
                    {
                        voice.positionInitialised = true;
                        if (voice.mode == 3)
                            voice.position = voice.regionEnd * sampleLength - 1.0;
                    }

                    const double regionStartF = voice.regionStart * sampleLength;
                    const double regionEndF = voice.regionEnd * sampleLength;
                    const double regionLenF = juce::jmax (1.0, regionEndF - regionStartF);

                    if (voice.mode == 2)
                    {
                        // Cloud: each lane fires grains on a density grid, reads them with a
                        // half-sine window, and advances through the region (wrapping).
                        if (! voice.grainActive && currentSampleIdx >= voice.nextGrainSample)
                        {
                            voice.grainActive = true;
                            voice.grainStartSample = currentSampleIdx;
                            voice.grainDurationSamples = grainSizeSamples;
                            voice.grainPosition = CoreDSP::Sampler::cloudGrainPosition (
                                voice.noteId, voice.grainIndex, voice.cloudLane,
                                voice.regionStart, voice.regionEnd, grainSpread,
                                sample->getNumSamples());
                            voice.nextGrainSample = currentSampleIdx + grainInterval;
                        }

                        if (voice.grainActive)
                        {
                            const double progress = (currentSampleIdx - voice.grainStartSample)
                                                    / voice.grainDurationSamples;

                            if (progress >= 1.0)
                            {
                                voice.grainActive = false;
                                ++voice.grainIndex;
                            }
                            else
                            {
                                while (voice.grainPosition >= regionEndF)
                                    voice.grainPosition -= regionLenF;
                                while (voice.grainPosition < regionStartF)
                                    voice.grainPosition += regionLenF;

                                const int grainSample = (int) voice.grainPosition;
                                if (grainSample >= 0 && grainSample < sample->getNumSamples())
                                {
                                    const double window = std::sin (progress * juce::MathConstants<double>::pi);
                                    outputValue += (float) (sampleData[grainSample] * window)
                                                   * voice.gain * voice.velocity * envValue;
                                }

                                voice.grainPosition += voice.rate
                                                       * CoreDSP::Sampler::cloudLaneDetune (voice.cloudLane);
                            }
                        }
                    }
                    else
                    {
                        // Scrub / Chop / Swell: read the current position, then advance one
                        // frame per output sample (backwards for Swell). The voice ends as a
                        // one-shot when it runs past its region.
                        const int readIndex = (int) voice.position;
                        if (readIndex >= 0 && readIndex < sample->getNumSamples())
                            outputValue += sampleData[readIndex] * voice.gain * voice.velocity * envValue;

                        if (voice.mode == 3)
                        {
                            voice.position -= voice.rate;
                            if (voice.position <= regionStartF)
                                voice.active = false;
                        }
                        else
                        {
                            voice.position += voice.rate;
                            if (voice.position >= regionEndF)
                                voice.active = false;
                        }
                    }
                }
                
                outPtr[n] = outputValue;
            }
        }
    }

    advanceClocks();
}

void SgraffitoProcessor::applyPresetState (const juce::ValueTree& tree)
{
    // Presets use resetMissing=true so absent parameters are set to default.
    const juce::MemoryBlock block = SuiteState::detail::encodeXml (*tree.createXml());
    const auto result = SuiteState::read (block.getData(), (int) block.getSize(), apvts,
                                          stateVersion, migrations, {}, true);
    juce::ignoreUnused (result);
}

void SgraffitoProcessor::loadSample (const juce::File& file)
{
    if (! file.existsAsFile())
        return;

    const double targetRate = preparedSampleRate > 0.0 ? preparedSampleRate : 44100.0;

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (
        formatManager.createReaderFor (file));

    if (reader == nullptr)
        return;

    juce::AudioBuffer<float> fileBuffer ((int) reader->numChannels,
                                         (int) reader->lengthInSamples);
    reader->read (&fileBuffer,
                  0,
                  (int) reader->lengthInSamples,
                  0,
                  true,
                  true);

    // Resample to the target sample rate if needed.
    if (std::abs (reader->sampleRate - targetRate) > 0.1)
    {
        juce::AudioBuffer<float> resampled ((int) reader->numChannels,
                                            (int) (reader->lengthInSamples * targetRate / reader->sampleRate));

        for (int ch = 0; ch < (int) reader->numChannels; ++ch)
        {
            const float* src = fileBuffer.getReadPointer (ch);
            float* dst = resampled.getWritePointer (ch);

            const double ratio = reader->sampleRate / targetRate;
            for (int i = 0; i < resampled.getNumSamples(); ++i)
            {
                const double srcPos = i * ratio;
                const int srcIdx = (int) srcPos;
                const float frac = (float) (srcPos - srcIdx);

                if (srcIdx + 1 < fileBuffer.getNumSamples())
                    dst[i] = src[srcIdx] * (1.0f - frac) + src[srcIdx + 1] * frac;
                else
                    dst[i] = src[srcIdx];
            }
        }
        fileBuffer = std::move (resampled);
    }

    // Retire the old sample instead of destroying it: an audio block that started before
    // this call may still hold the old buffer pointer from the atomic load.
    retiredSample = std::move (loadedSample);
    loadedSample = std::make_unique<LoadedSample>();
    loadedSample->buffer = std::move (fileBuffer);
    loadedSample->sourcePath = file.getFullPathName();
    loadedSample->sourceSampleRate = reader->sampleRate;
    sampleFile = file;
    sampleBuffer.store (&loadedSample->buffer, std::memory_order_release);
}

void SgraffitoProcessor::loadEmbeddedSample (const juce::String& embeddedPath)
{
    // Extract the resource name from "embedded:grain_4k.wav".
    const auto colonPos = embeddedPath.indexOfChar (':');
    if (colonPos == -1 || colonPos == 0)
        return;

    const juce::String filePath = embeddedPath.substring (colonPos + 1);

    // Binary data resource names keep only alphanumerics: "grain_4k.wav" becomes "grain_4k_wav".
    juce::String resourceName;
    for (const auto character : filePath)
        resourceName += (juce::CharacterFunctions::isLetterOrDigit (character) ? character : '_');

    int resourceSize = 0;
    const char* resourceData = BinaryData::getNamedResource (resourceName.toRawUTF8(), resourceSize);
    if (resourceData == nullptr || resourceSize <= 0)
        return;

    juce::MemoryBlock memoryBlock (resourceData, (size_t) resourceSize);

    const double targetRate = preparedSampleRate > 0.0 ? preparedSampleRate : 44100.0;

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (
        formatManager.createReaderFor (std::make_unique<juce::MemoryInputStream> (memoryBlock, false)));

    if (reader == nullptr)
        return;

    juce::AudioBuffer<float> fileBuffer ((int) reader->numChannels,
                                         (int) reader->lengthInSamples);
    reader->read (&fileBuffer,
                  0,
                  (int) reader->lengthInSamples,
                  0,
                  true,
                  true);

    // Resample to the target sample rate if needed.
    if (std::abs (reader->sampleRate - targetRate) > 0.1)
    {
        juce::AudioBuffer<float> resampled ((int) reader->numChannels,
                                            (int) (reader->lengthInSamples * targetRate / reader->sampleRate));

        for (int ch = 0; ch < (int) reader->numChannels; ++ch)
        {
            const float* src = fileBuffer.getReadPointer (ch);
            float* dst = resampled.getWritePointer (ch);

            const double ratio = reader->sampleRate / targetRate;
            for (int i = 0; i < resampled.getNumSamples(); ++i)
            {
                const double srcPos = i * ratio;
                const int srcIdx = (int) srcPos;
                const float frac = (float) (srcPos - srcIdx);

                if (srcIdx + 1 < fileBuffer.getNumSamples())
                    dst[i] = src[srcIdx] * (1.0f - frac) + src[srcIdx + 1] * frac;
                else
                    dst[i] = src[srcIdx];
            }
        }
        fileBuffer = std::move (resampled);
    }

    // Retire the old sample instead of destroying it (see loadSample).
    retiredSample = std::move (loadedSample);
    loadedSample = std::make_unique<LoadedSample>();
    loadedSample->buffer = std::move (fileBuffer);
    loadedSample->sourcePath = "embedded:" + filePath;
    loadedSample->sourceSampleRate = reader->sampleRate;
    sampleFile = juce::File(); // File path is not available for embedded data
    sampleBuffer.store (&loadedSample->buffer, std::memory_order_release);
}

void SgraffitoProcessor::clearSample()
{
    sampleFile = juce::File();
    retiredSample = std::move (loadedSample);
    sampleBuffer.store (nullptr, std::memory_order_release);
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
