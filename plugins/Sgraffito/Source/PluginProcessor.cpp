#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

#include "SuiteState.h"

#include <algorithm>
#include <cmath>

#include <juce_audio_utils/juce_audio_utils.h>

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
void SgraffitoProcessor::handleNoteOn (int keyIn, float velocity, int samplePosition,
                                       float rootKeyMod, float layBackMod,
                                       float spanMod, float lengthMod)
{
    int key = keyIn;

    // Scale Lock: quantize the played key to the nearest scale degree.
    if (scaleLockParam->load() > 0.5f)
    {
        const int rootKey = (int) std::lround (rootKeyMod);
        const int scaleIndex = (int) std::lround (scaleParam->load());
        key = CoreDSP::Sampler::quantizeToScale (key, rootKey, scaleIndex);
    }

    // If lay back is enabled, queue the note for later playback.
    if (layBackMod > 0.0f)
    {
        pendingNotes.push_back ({ key, velocity, static_cast<double> (samplePosition) });
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
        }
    }
}

void SgraffitoProcessor::handleNoteOff (int key, int samplePosition)
{
    for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
    {
        if (voicePool.voices[i].active && voicePool.voices[i].key == key)
        {
            voicePool.releaseVoice (i, samplePosition, tapeStopTimeParam->load(), tapeStopCurveParam->load());
            break;
        }
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
        juce::NormalisableRange<float> (24.0f, 96.0f, 1.0f), 60.0f));

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
    auto advanceClocks = [this, numSamples]()
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

    // Process MIDI messages first: note-on allocates a voice, note-off releases it.
    // This must happen even without a sample loaded, so voices are tracked correctly.
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();

        if (msg.isNoteOn())
            handleNoteOn (msg.getNoteNumber(), msg.getVelocity() / 127.0f,
                          metadata.samplePosition,
                          modulated (ModTarget::RootKey, rootKeyParam->load()),
                          modulated (ModTarget::LayBack, layBackParam->load()),
                          modulated (ModTarget::RegionSpan, regionSpanParam->load()),
                          modulated (ModTarget::RegionLength, regionLengthParam->load()));
        else if (msg.isNoteOff())
            handleNoteOff (msg.getNoteNumber(), (int) (streamSample + metadata.samplePosition));
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
                          modulated (ModTarget::RegionLength, regionLengthParam->load()));
        else
            handleNoteOff (note.key, 0);
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
    const float level = juce::Decibels::decibelsToGain (modulated (ModTarget::Level, levelParam->load()));
    const float attackMs = modulated (ModTarget::Attack, attackParam->load());
    const float releaseMs = modulated (ModTarget::Release, releaseParam->load());
    // The absolute sample position of this block's first sample: the envelope's time base.
    // (streamSample advances at the end of the block; the old block-relative base stuck the
    // envelope at the attack of the first block forever.)
    const double blockStartSample = (double) streamSample;

    // Advance Lorenz attractor by driftSpeed * seconds in this block.
    const float driftSpeed = modulated (ModTarget::DriftSpeed, driftSpeedParam->load());
    const double advanceSeconds = static_cast<double> (numSamples) / preparedSampleRate;
    lorenz.advance (driftSpeed * advanceSeconds);

    const float driftAmountPercent = modulated (ModTarget::DriftAmount, driftAmountParam->load()) / 100.0f;

    // Mode-specific processing: switch on the current mode.
    const int mode = std::lround (modeParam->load());

    for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
    {
        auto& voice = voicePool.voices[i];
        if (! voice.active)
            continue;

        // Initialize envStart on first sample: the voice reads from its region, forwards
        // for Scrub and Chop, from the region end backwards for Swell, and cloud schedules
        // its first grain at once.
        if (voice.envStart < 0.0)
        {
            voice.envStart = blockStartSample;
            voice.position = (mode == 3 ? voice.regionEnd - 1.0 / (double) sample->getNumSamples()
                                        : voice.regionStart) * sample->getNumSamples();
            voice.grainPosition = voice.regionStart * sample->getNumSamples();
            voice.nextGrainSample = 0.0;
        }

        // Cloud mode: read from random positions across the region.
        bool isCloud = (mode == 2);

        // Apply tape-stop rate reduction.
        float rate = voice.rate;
        float rateFactor = 1.0f;
        if (voice.tapeStopping)
            rateFactor = voice.tapeStop.tick (preparedSampleRate);

        // Apply chaos drift: chaos * driftAmount cents, converted to rate multiplier.
        const float chaos = lorenz.normalisedX();
        const float driftCents = driftAmountPercent * chaos;
        const float driftRatio = std::pow (2.0f, driftCents / 1200.0f);
        rate *= driftRatio * rateFactor;

        // Apply memory trace: the note's layer fades over the memory decay time, once per
        // block (memoryDecay seconds is the time constant).
        if (modulated (ModTarget::MemoryAmount, memoryAmountParam->load()) > 0.5f)
        {
            const double tau = juce::jmax (0.01, (double) memoryDecayParam->load());  // seconds
            voice.gain *= (float) std::exp (- (double) numSamples / (tau * preparedSampleRate));
        }

        // Process each sample in the block.
        for (int s = 0; s < numSamples; ++s)
        {
            const double sampleIndex = blockStartSample + static_cast<double> (s);

            // Envelope value.
            const float env = voice.envelope (sampleIndex, attackMs, releaseMs, preparedSampleRate) * rateFactor;

            // Read sample with linear interpolation (normal playback: Scrub, Chop, Swell).
            if (! isCloud)
            {
                const double pos = voice.position;
                const int idx0 = static_cast<int> (std::floor (pos));
                const int idx1 = idx0 + 1;
                const float frac = static_cast<float> (pos - idx0);

                if (idx0 >= 0 && idx1 < sample->getNumSamples())
                {
                    const int sampleChannels = sample->getNumChannels();

                    for (int ch = 0; ch < numChannels; ++ch)
                    {
                        // A mono sample feeds every output channel.
                        const int srcChannel = std::clamp (ch, 0, sampleChannels - 1);
                        const float* src = sample->getReadPointer (srcChannel);
                        const int safeIdx0 = std::clamp(static_cast<int>(idx0), 0, static_cast<int>(sample->getNumSamples()) - 1);
                        const int safeIdx1 = std::clamp(static_cast<int>(idx1), 0, static_cast<int>(sample->getNumSamples()) - 1);
                        const float s0 = src[safeIdx0];
                        const float s1 = src[safeIdx1];
                        const float v = s0 * (1.0f - frac) + s1 * frac;

                        buffer.addSample (ch, s, v * voice.gain * voice.velocity * level * env);
                    }
                }
            }
            else
            {
                // Cloud mode: a grain scheduler. Grains spawn at grainDensity per second,
                // read for grainSize milliseconds from a position spread across the region
                // (grainSpread), and fade in and out (plan 05.1: the grain parameters must
                // be audible).
                const int sampleSize = static_cast<int> (sample->getNumSamples());
                const double regionLenSamples = (voice.regionEnd - voice.regionStart) * (double) sampleSize;

                if (sampleIndex >= voice.nextGrainSample)
                {
                    voice.grainActive = true;
                    ++voice.grainIndex;

                    // Deterministic per-grain position: hash of the note and the grain index.
                    const std::uint32_t hash = voice.noteId * 74747909u + voice.grainIndex * 2654435761u;
                    const float fraction = (float) ((hash >> 8) & 0xFFFFu) / 65535.0f;

                    const double spread = std::clamp (grainSpreadParam->load() / 100.0, 0.0, 1.0);
                    voice.grainStartSample = voice.regionStart * (double) sampleSize
                                             + (double) fraction * spread * regionLenSamples;
                    voice.grainStartSample = std::clamp (voice.grainStartSample, 0.0, (double) sampleSize - 2.0);
                    voice.grainPosition = voice.grainStartSample;
                    voice.grainDurationSamples = juce::jmax (1.0,
                        (double) grainSizeParam->load() * preparedSampleRate / 1000.0);
                    voice.nextGrainSample = sampleIndex + (double) preparedSampleRate
                                            / juce::jmax (1.0f, grainDensityParam->load());
                }

                if (voice.grainActive)
                {
                    const double progress = (voice.grainPosition - voice.grainStartSample) / voice.grainDurationSamples;

                    if (progress >= 1.0)
                    {
                        voice.grainActive = false;
                    }
                    else
                    {
                        // Triangular grain envelope, up and down over the grain's duration.
                        const float grainEnv = juce::jlimit (0.0f, 1.0f,
                            progress < 0.5 ? (float) (progress * 2.0) : (float) ((1.0 - progress) * 2.0));

                        const double pos = voice.grainPosition;
                        const int idx0 = static_cast<int> (std::floor (pos));
                        const int idx1 = idx0 + 1;
                        const float frac = static_cast<float> (pos - idx0);
                        const float* src = sample->getReadPointer (0);
                        const int safeIdx0 = std::clamp (idx0, 0, sampleSize - 1);
                        const int safeIdx1 = std::clamp (idx1, 0, sampleSize - 1);
                        const float s0 = src[safeIdx0];
                        const float s1 = src[safeIdx1];
                        const float v = s0 * (1.0f - frac) + s1 * frac;

                        for (int ch = 0; ch < numChannels; ++ch)
                            buffer.addSample (ch, s, v * grainEnv * voice.gain * voice.velocity * level * env);

                        voice.grainPosition += rate;
                    }
                }

                continue;  // Cloud grains skip the mode-specific advance below.
            }

            // Mode-specific handling per voice.
            const int mode = std::lround (modeParam->load());
            if (mode == 3)  // Swell mode: reverse playback, wrapping across the region.
            {
                voice.position -= rate;

                if (voice.position < voice.regionStart * (double) sample->getNumSamples())
                    voice.position = voice.regionEnd * (double) sample->getNumSamples() - 1.0;
            }
            else  // Normal forward playback (Scrub and Chop; Cloud handled above).
            {
                voice.position += rate;
            }

            // Release voice if past region end.
            if (voice.position >= voice.regionEnd * sample->getNumSamples())
            {
                voice.active = false;
                break;
            }

            // Release voice if tape stop complete.
            if (voice.tapeStopping && rateFactor < 0.001f)
            {
                voice.active = false;
                voice.tapeStopping = false;
                break;
            }
        }

        // Release voice if tape stop complete after full block.
        if (voice.tapeStopping && ! voice.tapeStop.active())
        {
            voice.active = false;
            voice.tapeStopping = false;
        }
    }

    // Lay back: queue note-ons and fire them at the scheduled position.
    const float layBackMs = modulated (ModTarget::LayBack, layBackParam->load());
    if (layBackMs > 0.0f)
    {
        const double delaySamples = layBackMs * preparedSampleRate / 1000.0;

        for (const auto metadata : pendingNotes)
        {
            const double scheduledPos = (double) streamSample + static_cast<double> (metadata.samplePosition) + delaySamples;
            const int key = std::clamp(static_cast<int>(metadata.key), 0, 127);
            const float velocity = metadata.velocity;

            // Find or allocate a voice for this note.
            int idx = -1;
            for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
            {
                if (! voicePool.voices[i].active)
                {
                    idx = i;
                    break;
                }
            }

            if (idx >= 0)
            {
                CoreDSP::Sampler::Voice voice = CoreDSP::Sampler::Voice{};
                voicePool.voices[idx] = voice;
                voicePool.voices[idx].active = true;
                voicePool.voices[idx].key = key;
                voicePool.voices[idx].velocity = velocity;

                // Pitch and velocity curve, as in the direct path (plan 05.1).
                const double semitones = (double) (key - 60) + (double) pitchFineParam->load() / 100.0;
                voicePool.voices[idx].rate = std::pow (2.0, semitones / 12.0);
                const float velCurve = juce::jmax (0.1f, velocityCurveParam->load());
                voicePool.voices[idx].velocity = std::pow (juce::jlimit (0.0f, 1.0f, velocity), velCurve);
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
        }
    }

    pendingNotes.clear();

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

    // Atomic swap: release old pointer first, then install new sample.
    sampleBuffer.store (nullptr, std::memory_order_release);
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

    // Atomic swap: release old pointer first, then install new sample.
    sampleBuffer.store (nullptr, std::memory_order_release);
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
    loadedSample.reset();
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
