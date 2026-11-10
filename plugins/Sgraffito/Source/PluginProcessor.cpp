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
juce::AudioProcessorValueTreeState::ParameterLayout SgraffitoProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto percent = [] (float v, int) { return juce::String (v, 0) + " %"; };
    auto ms = [] (float v, int) { return juce::String (v, 0) + " ms"; };
    auto db = [] (float v, int) { return juce::String (v, 1) + " dB"; };
    auto hz = [] (float v, int) { return juce::String (v, 2); };
    auto cents = [] (float v, int) { return juce::String (v, 0) + " ct"; };

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
            int key = msg.getNoteNumber();

            // Scale Lock: quantize the played key to the nearest scale degree.
            if (scaleLockParam->load() > 0.5f)
            {
                const int rootKey = (int) std::lround (rootKeyParam->load());
                const int scaleIndex = (int) std::lround (scaleParam->load());
                key = CoreDSP::Sampler::quantizeToScale (key, rootKey, scaleIndex);
            }
            const float velocity = msg.getVelocity() / 127.0f;

            // If lay back is enabled, queue the note for later playback.
            const float layBackMs = layBackParam->load();
            if (layBackMs > 0.0f)
            {
                pendingNotes.push_back({ key, velocity, static_cast<double>(metadata.samplePosition) });
            }
            else
            {
                voicePool.allocateVoice (key, velocity, regionSpanParam->load(), regionLengthParam->load(),
                                         modeParam->load(), sliceCountParam->load());
            }
        }
        else if (msg.isNoteOff())
        {
            const int key = msg.getNoteNumber();
            for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
            {
                if (voicePool.voices[i].active && voicePool.voices[i].key == key)
                {
                    voicePool.releaseVoice (i, metadata.samplePosition, tapeStopTimeParam->load(), tapeStopCurveParam->load());
                    break;
                }
            }
        }
    }

    midiMessages.clear();

    // Get the current sample buffer (atomic read).
    auto* samplePtr = getSampleBuffer();
    if (samplePtr == nullptr || samplePtr->getNumSamples() == 0)
        return;

    const auto* sample = static_cast<const juce::AudioBuffer<float>*>(samplePtr);
    const int numChannels = buffer.getNumChannels();
    const float level = juce::Decibels::decibelsToGain (levelParam->load());
    const float attackMs = attackParam->load();
    const float releaseMs = releaseParam->load();
    const double blockStartSample = static_cast<double> (buffer.getNumSamples()) - static_cast<double> (numSamples);

    // Advance Lorenz attractor by driftSpeed * seconds in this block.
    const float driftSpeed = driftSpeedParam->load();
    const double advanceSeconds = static_cast<double> (numSamples) / preparedSampleRate;
    lorenz.advance (driftSpeed * advanceSeconds);

    const float driftAmountPercent = driftAmountParam->load() / 100.0f;

    // Mode-specific processing: switch on the current mode.
    const int mode = std::lround (modeParam->load());

    for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
    {
        auto& voice = voicePool.voices[i];
        if (! voice.active)
            continue;

        // Initialize envStart on first sample.
        if (voice.envStart < 0.0)
        {
            voice.envStart = blockStartSample;
            voice.position = voice.regionStart * sample->getNumSamples();
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

        // Process each sample in the block.
        for (int s = 0; s < numSamples; ++s)
        {
            const double sampleIndex = blockStartSample + static_cast<double> (s);

            // Envelope value.
            const float env = voice.envelope (sampleIndex, attackMs, releaseMs, preparedSampleRate) * rateFactor;

            // Apply memory trace: slowly decay current gain toward the note's values.
            if (memoryAmountParam->load() > 0.5f)
            {
                const int sampleSize = static_cast<int> (sample->getNumSamples());
                voice.gain *= std::pow(0.999, sampleSize);  // decay slightly each sample

                // Get the current note's gain value from level and velocity
                const float currentGainDb = juce::Decibels::decibelsToGain (levelParam->load()) * voice.velocity;
                
                // Move trace toward current values (simplified - would need full memory trace system)
            }

            // Read sample with linear interpolation.
            const double pos = voice.position;
            const int idx0 = static_cast<int> (std::floor (pos));
            const int idx1 = idx0 + 1;
            const float frac = static_cast<float> (pos - idx0);

            if (idx0 >= 0 && idx1 < sample->getNumSamples())
            {
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    const float* src = sample->getReadPointer (ch);
                    const int safeIdx0 = std::clamp(static_cast<int>(idx0), 0, static_cast<int>(sample->getNumSamples()) - 1);
                    const int safeIdx1 = std::clamp(static_cast<int>(idx1), 0, static_cast<int>(sample->getNumSamples()) - 1);
                    const float s0 = src[safeIdx0];
                    const float s1 = src[safeIdx1];
                    const float v = s0 * (1.0f - frac) + s1 * frac;

                    buffer.addSample (ch, i, v * voice.gain * voice.velocity * level * env);
                }
            }

            // Cloud mode: read from grain position across region.
            if (isCloud)
            {
                const int sampleSize = static_cast<int> (sample->getNumSamples());
                const float spread = grainSpreadParam->load() / 100.0f;  // fraction of region to spread over
                
                // Get current gain from memory trace (if active)
                if (memoryAmountParam->load() > 0.5f)
                {
                    voice.gain *= std::pow(0.999, sampleSize);  // decay slightly
                }

                // Read sample with linear interpolation using the grain's position within region
                const double pos = voice.grainPosition;
                const int idx0 = static_cast<int> (std::floor (pos));
                const int idx1 = idx0 + 1;
                const float frac = static_cast<float> (pos - idx0);

                if (idx0 >= 0 && idx1 < sampleSize)
                {
                    const float* src = sample->getReadPointer (0);  // Mono for now
                    const int safeIdx0 = std::clamp(idx0, 0, sampleSize - 1);
                    const int safeIdx1 = std::clamp(idx1, 0, sampleSize - 1);
                    const float s0 = src[safeIdx0];
                    const float s1 = src[safeIdx1];
                    const float v = s0 * (1.0f - frac) + s1 * frac;

                    buffer.addSample (0, i, v * voice.gain * voice.velocity * level * env);
                }

                // Advance grain position at the current rate
                voice.grainPosition += voice.grainRate;

                // If grain goes past region end, wrap around for continuous sound
                if (voice.grainPosition >= voice.regionEnd)
                {
                    voice.grainPosition = voice.regionStart;
                }

                continue;  // Skip normal processing for cloud grains
            }

            // Mode-specific handling per voice.
            const int mode = std::lround (modeParam->load());
            if (mode == 2)  // Cloud mode - grain cloud with multiple grains
            {
                // Already handled above with grainPosition and grainRate
            }
            else if (mode == 3)  // Swell mode - reverse playback with crossfade
            {
                // Reverse direction
                voice.position -= rate;
                
                // Crossfade from normal to reverse: use envelope value as crossfade amount
                const float crossfade = env;  // 0.0 to 1.0
                double pos = voice.position - (1.0 - crossfade) * rate;
                
                if (pos < voice.regionStart)
                    pos = voice.regionStart;
                
                voice.position = pos;
            }
            else  // Normal forward playback (mode == 0 or mode == 1)
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
    const float layBackMs = layBackParam->load();
    if (layBackMs > 0.0f)
    {
        const double delaySamples = layBackMs * preparedSampleRate / 1000.0;

        for (const auto metadata : pendingNotes)
        {
            const double scheduledPos = static_cast<double> (metadata.samplePosition) + delaySamples;
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
                voicePool.voices[idx].envStart = scheduledPos;
                voicePool.voices[idx].envValue = 1.0f;

                // Compute region for this note.
                const double keyPos = (double) key / 127.0;
                double regionStart = 0.0, regionEnd = 1.0;

                const int mode = std::lround (modeParam->load());
                if (mode == 1)  // Chop mode
                {
                    const int sliceCountClamped = std::clamp(static_cast<int>(sliceCountParam->load()), 2, CoreDSP::Sampler::maxVoices);
                    const int sliceIndex = std::clamp((int)(keyPos * sliceCountClamped), 0, sliceCountClamped - 1);
                    const double sliceLen = 1.0 / sliceCountClamped;
                    regionStart = sliceIndex * sliceLen;
                    regionEnd = (sliceIndex + 1) * sliceLen;
                }
                else if (mode == 3)  // Swell mode
                {
                    const double maxStart = 1.0 - std::clamp(static_cast<double>(regionLengthParam->load()), 0.0, 1.0);
                    const double start = std::clamp((keyPos * std::clamp(static_cast<double>(regionSpanParam->load()), 0.0, 1.0)), 0.0, maxStart);
                    regionStart = start;
                    regionEnd = start + std::clamp(static_cast<double>(regionLengthParam->load()), 0.0, 1.0);
                }
                else  // Scrub/Cloud: linear mapping
                {
                    const double maxStart = 1.0 - std::clamp(static_cast<double>(regionLengthParam->load()), 0.0, 1.0);
                    const double start = std::clamp((keyPos * std::clamp(static_cast<double>(regionSpanParam->load()), 0.0, 1.0)), 0.0, maxStart);
                    regionStart = start;
                    regionEnd = start + std::clamp(static_cast<double>(regionLengthParam->load()), 0.0, 1.0);
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
