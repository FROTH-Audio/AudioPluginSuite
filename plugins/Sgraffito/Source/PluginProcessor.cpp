#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

#include "SuiteState.h"

#if SUITE_PRESET_BANK_AS_PROGRAMS
 #include "PresetBank.h"
#endif

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

SgraffitoProcessor::~SgraffitoProcessor()
{
    // Dead first, so a queued handoff lambda does nothing (plan 07 phase 4). Then stop
    // the pool: interrupt running jobs and wait, so no job outlives the members it
    // would hand off to.
    loadLifetime->alive = false;
    sampleLoadPool.removeAllJobs (true, 5000);
}

//==============================================================================
// Background sample loads (plan 07 phase 4).

/** The read + resample of one file, run on a pooled thread. The job touches none of the
    processor's members: it captures the request by value and hands the finished buffer
    to the message thread, where the install happens. */
class SgraffitoProcessor::SampleLoadJob : public juce::ThreadPoolJob
{
public:
    SampleLoadJob (juce::File fileToRead, int generationIn, double targetRateIn,
                   std::shared_ptr<LoadLifetime> lifetimeIn, SgraffitoProcessor* ownerIn)
        : juce::ThreadPoolJob ("Sgraffito sample load"),
          file (std::move (fileToRead)), generation (generationIn), targetRate (targetRateIn),
          lifetime (std::move (lifetimeIn)), owner (ownerIn)
    {
    }

    JobStatus runJob() override
    {
        auto buffer = readSampleFile (file, targetRate);
        const double sourceRate = bufferSampleRate;

        // A shutdown interrupt skips the handoff: the processor is going away anyway.
        if (shouldExit())
            return jobHasFinished;

        // Locals for the capture list: members cannot be captured by name.
        const auto lifetimeCopy = lifetime;
        auto* const ownerCopy = owner;
        const int generationCopy = generation;
        const auto pathCopy = file.getFullPathName();

        // Handoff runs on the message thread; the copy of the token keeps the check
        // valid even if the processor is destroyed before the lambda runs.
        juce::MessageManager::callAsync (
            [lifetimeCopy, ownerCopy, generationCopy, buffer = std::move (buffer),
             pathCopy, sourceRate]() mutable
            {
                if (lifetimeCopy == nullptr || ! lifetimeCopy->alive)
                    return;

                ownerCopy->pendingSampleLoads.fetch_sub (1, std::memory_order_relaxed);

                if (buffer.getNumSamples() <= 0)
                    return;   // unreadable file: keep the current sample, as the sync path does

                // The latest request wins: an install that arrives out of order is discarded.
                if (generationCopy != ownerCopy->loadGeneration.load (std::memory_order_relaxed))
                    return;

                ownerCopy->finishSampleLoad (std::move (buffer), pathCopy, sourceRate);
            });

        return jobHasFinished;
    }

    juce::AudioBuffer<float> readSampleFile (const juce::File& fileToRead, double rate)
    {
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader (
            formatManager.createReaderFor (fileToRead));

        if (reader == nullptr)
            return {};

        bufferSampleRate = reader->sampleRate;
        return readAndResample (*reader, rate);
    }

    /** The read and (linear) resample shared with the sync loaders. */
    static juce::AudioBuffer<float> readAndResample (juce::AudioFormatReader& reader, double targetRate)
    {
        juce::AudioBuffer<float> fileBuffer ((int) reader.numChannels,
                                             (int) reader.lengthInSamples);
        reader.read (&fileBuffer,
                     0,
                     (int) reader.lengthInSamples,
                     0,
                     true,
                     true);

        if (std::abs (reader.sampleRate - targetRate) <= 0.1)
            return fileBuffer;

        juce::AudioBuffer<float> resampled ((int) reader.numChannels,
                                            (int) (reader.lengthInSamples * targetRate / reader.sampleRate));

        for (int ch = 0; ch < (int) reader.numChannels; ++ch)
        {
            const float* src = fileBuffer.getReadPointer (ch);
            float* dst = resampled.getWritePointer (ch);

            const double ratio = reader.sampleRate / targetRate;
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
        return resampled;
    }

    juce::File file;
    int generation = 0;
    double targetRate = 44100.0;
    double bufferSampleRate = 0.0;
    std::shared_ptr<LoadLifetime> lifetime;
    SgraffitoProcessor* owner = nullptr;
};

void SgraffitoProcessor::loadSampleAsync (const juce::File& file)
{
    if (! file.existsAsFile())
        return;

    // Any sample still in flight from an earlier request is superseded. The generation
    // this request creates is stamped onto the job in the same atomic operation, so the
    // latest-wins check in the handoff compares against the number this request made
    // (plan 09 1.2: no second load() between the increment and the stamp).
    const int generation = loadGeneration.fetch_add (1, std::memory_order_relaxed) + 1;
    pendingSampleLoads.fetch_add (1, std::memory_order_relaxed);

    const double targetRate = preparedSampleRate > 0.0 ? preparedSampleRate : 44100.0;
    sampleLoadPool.addJob (new SampleLoadJob (file, generation, targetRate, loadLifetime, this),
                           true);
}

void SgraffitoProcessor::finishSampleLoad (juce::AudioBuffer<float>&& buffer,
                                           const juce::String& sourcePath, double sourceSampleRate)
{
    // Retire the old sample through the capped queue (plan 09 1.1): a block that started
    // before this call may still hold the old buffer pointer from the atomic load, and
    // two installs inside one running block would have freed it under the old single slot.
    retireCurrentSample();
    loadedSample = std::make_unique<LoadedSample>();
    loadedSample->buffer = std::move (buffer);
    loadedSample->sourcePath = sourcePath;
    loadedSample->sourceSampleRate = sourceSampleRate;

    if (sourcePath.startsWith ("embedded:"))
        sampleFile = juce::File();
    else
        sampleFile = juce::File (sourcePath);

    sampleBuffer.store (&loadedSample->buffer, std::memory_order_release);

    // The sample is part of the preset/session state (plan 07): keep the property in
    // sync at change time, like the Micro-Timing lerp curve does.
    sampleMissing.store (false, std::memory_order_release);
    apvts.state.setProperty (samplePathProperty, loadedSample->sourcePath, nullptr);
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

void SgraffitoProcessor::setupAllocatedVoice (int idx, int key, float velocity, float rootKeyMod)
{
    auto& voice = voicePool.voices[idx];

    // Pitch: the key's distance from the Root Key (the pitch centre, plan 10 phase 4 —
    // the root key's note plays the sample at its natural pitch), plus the fine tune in
    // cents. The old centre was a fixed middle C 60, so the Root Key knob changed nothing.
    const int rootKey = juce::roundToInt (rootKeyMod);
    const double semitones = (double) (key - rootKey) + (double) pitchFineParam->load() / 100.0;
    voice.rate = std::pow (2.0, semitones / 12.0);

    // Velocity curve: the curve is the exponent of the velocity (1.0 = linear).
    const float curve = juce::jmax (0.1f, velocityCurveParam->load());
    voice.velocity = std::pow (juce::jlimit (0.0f, 1.0f, velocity), curve);

    // The voice reads from the region's start: allocateVoice does not set the
    // position, and without this every note read the sample from sample 0
    // (the "always sounds the same" bug, plan 05.1).
    if (auto* sampleNow = getSampleBuffer())
        voice.position = voice.regionStart * (double) sampleNow->getNumSamples();
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
        // The absolute stream position is the envelope's start (plan 10 phase 3): the
        // default 0.0 put envStart tens of thousands of samples behind the playhead, so
        // the attack ramp was always "finished" and ATTACK did nothing on direct notes.
        const int idx = voicePool.allocateVoice (key, velocity, spanMod, lengthMod,
                                                 modeParam->load(), sliceCountParam->load(),
                                                 (double) streamSample + (double) samplePosition);

        if (idx >= 0)
        {
            setupAllocatedVoice (idx, key, velocity, rootKeyMod);

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

    // Output (plan 12: one name for the final gain stage; the ID never changes)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { levelId, 1 }, "Output",
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

    // The output gain (plan 12): 20 ms smoothing, started at the target so the first
    // block is bit-exact at the 0 dB default.
    outputGainSmoothed.reset (sampleRate > 0.0 ? sampleRate : 44100.0, 0.02);
    outputGainSmoothed.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (juce::jlimit (-60.0f, 6.0f, levelParam->load())));

    modulation.prepare (sampleRate);
    streamSample = 0;
    beatClock = 0.0;

    // First call: load the bundled factory sample so the plugin makes sound, and the
    // editor's waveform view has something to show, out of the box. A sample the user
    // loaded later is left alone.
    if (loadedSample == nullptr)
        loadEmbeddedSample (embeddedSampleUri);

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
            // The keyboard's note-off releases at this block's start (the old 0 made
            // releaseVoice set envEnd = 0, which left a silent zombie in the pool).
            handleNoteOff (note.key, (int) blockStartSample,
                           modulated (ModTarget::SustainHold, sustainHoldParam->load()) > 0.5f,
                           releaseMs, blockStartSample, tapeStopTimeParameter, tapeStopCurveParameter);
    }

    // Get the current sample buffer (atomic read).
    auto* samplePtr = getSampleBuffer();
    if (samplePtr == nullptr || samplePtr->getNumSamples() == 0)
    {
        advanceClocks();
        return;
    }

    const int numChannels = buffer.getNumChannels();

    // Advance Lorenz attractor by driftSpeed * seconds in this block.
    const float driftSpeed = modulated (ModTarget::DriftSpeed, driftSpeedParam->load());
    const double advanceSeconds = static_cast<double> (numSamples) / preparedSampleRate;
    lorenz.advance (driftSpeed * advanceSeconds);
    // Chaos drift is not wired to the audio yet (a follow-up plan owns it, like the
    // other unwired texture parameters documented in AGENTS.md).

    // Memory and surface settings for the whole block, hoisted out of the voice loop.
    //
    // Memory (the "sound disappears completely" bug): the note's gain fades toward
    // (1 - amount) over the decay time, so amount 0 leaves the note alone. The old gate
    // ("amount > 0.5" on a 0-100 % parameter whose default is 20) faded every voice's
    // gain exponentially toward true silence, which silenced sustained notes after
    // about a minute and left dead voices holding their pool slots.
    const double memoryAmount = juce::jlimit (0.0, 1.0,
        (double) modulated (ModTarget::MemoryAmount, memoryAmountParam->load()) / 100.0);
    const double memoryTau = juce::jmax (0.01, (double) memoryDecayParam->load());
    const double memoryFloor = 1.0 - memoryAmount;

    // Surface effects (dust/crackle/wow/flutter): derived per block from the voice's own
    // phase state. The old code multiplied their factors straight into voice.rate /
    // voice.gain once per block, so any nonzero amount grew exponentially (crackle built
    // up until the output clipped, wow's rate ran away and the grain wrap loops span).
    // Amounts are 0-100 %; an amount of zero leaves the voice untouched.
    const float dustAmount = juce::jlimit (0.0f, 1.0f,
        modulated (ModTarget::Dust, apvts.getRawParameterValue (dustId)->load()) / 100.0f);
    const float crackleAmount = juce::jlimit (0.0f, 1.0f,
        modulated (ModTarget::Crackle, apvts.getRawParameterValue (crackleId)->load()) / 100.0f);
    const float wowAmount = juce::jlimit (0.0f, 1.0f,
        modulated (ModTarget::Wow, apvts.getRawParameterValue (wowId)->load()) / 100.0f);
    const float flutterAmount = juce::jlimit (0.0f, 1.0f,
        modulated (ModTarget::Flutter, apvts.getRawParameterValue (flutterId)->load()) / 100.0f);

    // Phase rates in Hz (block-size independent): wow is a slow pitch wobble, flutter a
    // faster level wobble. Dust and crackle take one deterministic step per block.
    constexpr double wowHz = 1.5;
    constexpr double flutterHz = 8.0;
    constexpr double maxDustJitterFrames = 8.0;     // sparse specks, a few frames wide
    constexpr float maxCrackleStep = 0.3f;          // +-30 % level steps at full amount
    const double wowPhaseInc = wowHz * advanceSeconds;
    const double flutterPhaseInc = flutterHz * advanceSeconds;

    for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
    {
        auto& voice = voicePool.voices[i];
        if (! voice.active)
            continue;

        // The memory fade is bounded: the note keeps its (1 - amount) floor, and only a
        // full Memory Amount lets it decay to silence over the decay time.
        if (memoryAmount > 0.0)
        {
            const double decayed = std::exp (- (double) numSamples / (memoryTau * preparedSampleRate));
            voice.gain = (float) (memoryFloor + ((double) voice.gain - memoryFloor) * decayed);
        }

        // Surface factors are recomputed from the base values every block, never compounded.
        voice.surfaceGainFactor = 1.0f;
        voice.surfaceRateFactor = 1.0;
        voice.surfaceReadOffset = 0.0;

        if (wowAmount > 0.0f)
        {
            voice.surfaceState.phaseWow += wowPhaseInc;
            voice.surfaceState.phaseWow -= std::floor (voice.surfaceState.phaseWow);
            voice.surfaceRateFactor = 1.0 + (double) wowAmount * 0.02
                * std::sin (voice.surfaceState.phaseWow * juce::MathConstants<double>::twoPi);
        }

        if (flutterAmount > 0.0f)
        {
            voice.surfaceState.phaseFlutter += flutterPhaseInc;
            voice.surfaceState.phaseFlutter -= std::floor (voice.surfaceState.phaseFlutter);
            voice.surfaceGainFactor *= 1.0f + flutterAmount * 0.05f * static_cast<float> (
                std::sin (voice.surfaceState.phaseFlutter * juce::MathConstants<double>::twoPi));
        }

        if (crackleAmount > 0.0f)
        {
            voice.surfaceState.sampleCounter += 1;
            const double unit = CoreDSP::Sequencer::hashUnit (voice.noteId,
                                                              (std::uint32_t) voice.surfaceState.sampleCounter);
            voice.surfaceGainFactor *= 1.0f + crackleAmount * maxCrackleStep
                                           * static_cast<float> (2.0 * unit - 1.0);
        }

        // Dust: a bounded read jitter (a few frames), not a permanent position offset.
        // The old code pushed voice.position itself, which accumulated without limit.
        if (dustAmount > 0.0f)
        {
            const double unit = CoreDSP::Sequencer::hashUnit (voice.noteId,
                                                              (std::uint32_t) voice.surfaceState.sampleCounter + 0x9E3779B9u);
            voice.surfaceReadOffset = (unit - 0.5) * 2.0 * (double) dustAmount * maxDustJitterFrames;
        }

        // The tape-stop ramp ticks per output sample in the audio loop below, where its
        // level factor is also applied. (It used to tick once per block - one sample's
        // worth per tick, so a 400 ms ramp took minutes - and the level was ignored.)
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
                    // The shared post-allocation setup (plan 09 1.3): pitch and velocity
                    // curve like every other path. envStart comes from allocateVoice's
                    // scheduledPos; envValue tracks the envelope for a later release.
                    setupAllocatedVoice (ghostIdx, key, velocity,
                                         modulated (ModTarget::RootKey, rootKeyParam->load()));
                    voicePool.voices[ghostIdx].envValue = 1.0f;
                }

                pendingNotes.erase (pendingNotes.begin () + static_cast<std::ptrdiff_t> (i));
                continue;
            }

            // Allocate through the pool exactly like the ghost fire (plan 09 1.3, owner
            // decision D1): stealing when all voices are busy (tape-stopping, then oldest
            // release, then oldest active), Cloud lanes for mode 2, regions from the
            // shared engine maps. The old hand-rolled scan never stole and dropped the
            // note silently, and a Cloud note lost its lanes.
            const int idx = voicePool.allocateVoice (key, velocity,
                                                     modulated (ModTarget::RegionSpan, regionSpanParam->load()),
                                                     modulated (ModTarget::RegionLength, regionLengthParam->load()),
                                                     std::lround (modeParam->load()),
                                                     static_cast<int> (juce::jmax (0.0f, sliceCountParam->load())),
                                                     scheduledPos, key);

            if (idx >= 0)
            {
                setupAllocatedVoice (idx, key, velocity,
                                     modulated (ModTarget::RootKey, rootKeyParam->load()));

                // envStart from the scheduled position, envValue for a later release. A
                // note scheduled in the past (relative to the block start) plays now.
                voicePool.voices[idx].envStart = scheduledPos;
                voicePool.voices[idx].envValue = 1.0f;

                if (scheduledPos <= blockStartSample)
                    voicePool.voices[idx].envStart = blockStartSample;
            }

            pendingNotes.erase (pendingNotes.begin () + static_cast<std::ptrdiff_t> (i));
        }
    }

    // AUDIO PLAYBACK: read from sample buffer and write to output.
    // The voice state machine runs ONCE per output sample (the old loop ran it once per
    // output *channel*, so stereo output advanced every voice twice as fast - Scrub,
    // Chop and Swell played at double speed - and the second channel's pass read the
    // grain state the first pass had already moved, which decorrelated L/R and crackled).
    // Each voice is advanced once, then its contribution is read per channel: a mono
    // sample feeds every output channel, a stereo sample keeps its own channels.
    if (samplePtr != nullptr && samplePtr->getNumSamples() > 0)
    {
        const auto* sample = static_cast<const juce::AudioBuffer<float>*>(samplePtr);
        const int sampleChannels = std::max (1, sample->getNumChannels());
        const int numSampleFrames = sample->getNumSamples();
        const double sampleLength = (double) numSampleFrames;

        // Cloud grain timing: grain size in milliseconds, density in grains per second.
        // The guards keep a grain at least one sample long and one grain per second at
        // most apart (plan 09 D2: named, file-local, no behaviour change).
        constexpr double minGrainSizeSamples = 1.0;
        constexpr double minGrainIntervalSamples = 1.0;
        const double grainSizeSamples = juce::jmax (minGrainSizeSamples, (double) grainSizeParam->load() * preparedSampleRate / 1000.0);
        const double grainInterval = juce::jmax (minGrainIntervalSamples, preparedSampleRate / (double) juce::jmax (1.0f, grainDensityParam->load()));
        const double grainSpread = juce::jlimit (0.0, 1.0, (double) grainSpreadParam->load() / 100.0);

        // The region-boundary fade (plan 12): the last 5 ms of the region ramps the voice
        // down, so a one-shot that reaches the boundary never cuts mid-waveform. Distance
        // based, so any playback rate lands on ~0 at the cut. Cloud is untouched (its
        // grains are already windowed).
        const double declickFrames = juce::jmax (1.0, 0.005 * preparedSampleRate);

        // The output gain (plan 12), one smoothed value per block target, advanced per sample.
        outputGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (
            juce::jlimit (-60.0f, 6.0f, levelParam->load())));

        // The plugin is stereo-only; a host that offered more channels would leave the
        // extras silent (the buffer is cleared), never out of bounds.
        constexpr int maxOutChannels = 2;
        const int outChannels = juce::jmin (numChannels, maxOutChannels);
        const float* readPtrs[maxOutChannels] = {};
        float* writePtrs[maxOutChannels] = {};

        for (int ch = 0; ch < outChannels; ++ch)
        {
            readPtrs[ch] = sample->getReadPointer (ch % sampleChannels);
            writePtrs[ch] = buffer.getWritePointer (ch);
        }

        for (int n = 0; n < numSamples; ++n)
        {
            const double currentSampleIdx = blockStartSample + static_cast<double> (n);

            // The output gain advances once per sample, whatever the voice modes (plan 12).
            const float outputGain = outputGainSmoothed.getNextValue();

            for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
            {
                auto& voice = voicePool.voices[i];
                if (! voice.active)
                    continue;

                // Envelope value at this sample, cached for a later release.
                const float envValue = voice.envelope (currentSampleIdx, attackParam->load(), releaseMs, preparedSampleRate);
                voice.envValue = envValue;

                // Tape stop: the ramp ticks per output sample (the old code ticked once
                // per block - one sample's worth per tick, so a 400 ms ramp took minutes -
                // and ignored the returned level, so released voices kept sounding at
                // full level and piled up in the pool). This must run before the
                // envelope-zero checks below: a release envelope that hits zero while the
                // ramp is still running would otherwise starve the ramp, and the voice
                // would never finish (found by testTapeStopReleaseFreesVoice, plan 10).
                float tapeLevel = 1.0f;
                if (voice.tapeStopping)
                {
                    tapeLevel = voice.tapeStop.tick (preparedSampleRate);

                    if (! voice.tapeStop.active())
                    {
                        // The ramp is over (plan 10 phase 3, decision G2: the voice ends
                        // when BOTH the ramp and the release envelope are finished). The
                        // envelope tail - if it is still falling - plays on silently at
                        // the ramp's zero level until it too is done.
                        voice.tapeStopping = false;

                        if (envValue <= 0.0f && voice.envEnd >= 0.0)
                        {
                            voice.active = false;
                            continue;
                        }
                    }
                }

                // The release envelope has decayed to zero AND the tape-stop ramp (if any)
                // has run out: the voice is done (plan 10 phase 3, decision G2). envEnd is
                // -1 until a release was requested, so the test is >= 0 (not > 0): a
                // release at stream sample 0 used to leave a silent zombie holding its
                // slot forever.
                if (envValue <= 0.0f && voice.envEnd >= 0.0 && ! voice.tapeStopping)
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
                            numSampleFrames);
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

                            const double window = std::sin (progress * juce::MathConstants<double>::pi);
                            const double level = window * voice.gain * voice.velocity * envValue
                                                 * tapeLevel * voice.surfaceGainFactor * outputGain;

                            // Linear interpolation: the old integer truncation stepped
                            // between frames, an audible crackle at high rates.
                            for (int ch = 0; ch < outChannels; ++ch)
                                writePtrs[ch][n] += CoreDSP::Sampler::readLinear (readPtrs[ch], numSampleFrames, voice.grainPosition)
                                                    * (float) level;

                            voice.grainPosition += voice.rate * voice.surfaceRateFactor
                                                   * CoreDSP::Sampler::cloudLaneDetune (voice.cloudLane);
                        }
                    }
                }
                else
                {
                    // Scrub / Chop / Swell: read the current position once (interpolated,
                    // with the dust jitter), advance one frame per output sample (backwards
                    // for Swell), and end as a one-shot at the region end.
                    const double readPos = voice.position + voice.surfaceReadOffset;

                    // The boundary fade (plan 12): the level ramps down over the last
                    // declickFrames before the region end, so the cut lands on ~zero.
                    // Distance based, so any playback rate lands on ~0 at the cut.
                    const double distanceToEnd = voice.mode == 3
                        ? (voice.position - regionStartF)
                        : (regionEndF - voice.position);
                    const float declickFactor = distanceToEnd < declickFrames
                        ? (float) juce::jmax (0.0, distanceToEnd) / (float) declickFrames
                        : 1.0f;

                    const double level = voice.gain * voice.velocity * envValue
                                         * tapeLevel * voice.surfaceGainFactor
                                         * declickFactor * outputGain;

                    for (int ch = 0; ch < outChannels; ++ch)
                        writePtrs[ch][n] += CoreDSP::Sampler::readLinear (readPtrs[ch], numSampleFrames, readPos)
                                            * (float) level;

                    if (voice.mode == 3)
                    {
                        voice.position -= voice.rate * voice.surfaceRateFactor;
                        if (voice.position <= regionStartF)
                            voice.active = false;
                    }
                    else
                    {
                        voice.position += voice.rate * voice.surfaceRateFactor;
                        if (voice.position >= regionEndF)
                            voice.active = false;
                    }
                }
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
                                          stateVersion, migrations,
                                          [this, &tree] (const juce::ValueTree&)
                                          {
                                              // The incoming tree, not the replaced state: an
                                              // empty INIT tree gains a version property when
                                              // it is read, so the empty check must run on the
                                              // tree the caller passed (plan 07 C1/P1).
                                              applySampleFromState (tree);
                                          }, true);

    // Applying any tree resets the preset identity (plan 09 1.4): the replaced state no
    // longer carries the property, and the caller — the bar or the host-program bridge —
    // re-states it right after, when it knows which preset it applied.
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
// Host program list. Program 0 is Init; 1 to N are the factory presets in bank order. The list
// only exists in a bank build with SUITE_PRESET_BANK_AS_PROGRAMS on; otherwise the plugin
// reports one program, as it did before the preset system.

int SgraffitoProcessor::getNumPrograms()
{
#if SUITE_PRESET_BANK_AS_PROGRAMS
    return SuitePresets::PresetPrograms::getNumPrograms (getPresetPluginId());
#else
    return 1;
#endif
}

int SgraffitoProcessor::getCurrentProgram()
{
#if SUITE_PRESET_BANK_AS_PROGRAMS
    return currentProgramIndex.load (std::memory_order_relaxed);
#else
    return 0;
#endif
}

void SgraffitoProcessor::setCurrentProgram (int index)
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

const juce::String SgraffitoProcessor::getProgramName (int index)
{
#if SUITE_PRESET_BANK_AS_PROGRAMS
    return SuitePresets::PresetPrograms::getProgramName (getPresetPluginId(), index);
#else
    juce::ignoreUnused (index);
    return {};
#endif
}

void SgraffitoProcessor::getCurrentProgramStateInformation (juce::MemoryBlock& destData)
{
    getStateInformation (destData);
}

void SgraffitoProcessor::setCurrentProgramStateInformation (const void* data, int sizeInBytes)
{
    setStateInformation (data, sizeInBytes);

#if SUITE_PRESET_BANK_AS_PROGRAMS
    currentProgramIndex.store (-1, std::memory_order_relaxed);
#endif
}

void SgraffitoProcessor::loadSample (const juce::File& file)
{
    if (! file.existsAsFile())
        return;

    // A sync install supersedes anything still in flight (tests rely on sync loads).
    loadGeneration.fetch_add (1, std::memory_order_relaxed);

    const double targetRate = preparedSampleRate > 0.0 ? preparedSampleRate : 44100.0;

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (
        formatManager.createReaderFor (file));

    if (reader == nullptr)
        return;

    finishSampleLoad (SampleLoadJob::readAndResample (*reader, targetRate),
                      file.getFullPathName(), reader->sampleRate);
}

void SgraffitoProcessor::retireCurrentSample()
{
    if (loadedSample == nullptr)
        return;

    retiredSamples.push_back (std::move (loadedSample));

    // A block holds at most one pointer, read at its start. After two more installs, no
    // block can still reference the oldest entry (blocks are milliseconds; installs are
    // message-thread serialised), so the front is safe to destroy (plan 09 1.1).
    while ((int) retiredSamples.size() > retiredSampleGenerations)
        retiredSamples.pop_front();
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

    finishSampleLoad (SampleLoadJob::readAndResample (*reader, targetRate),
                      "embedded:" + filePath, reader->sampleRate);
}

void SgraffitoProcessor::clearSample()
{
    // A pending background load must not reinstall after a clear.
    loadGeneration.fetch_add (1, std::memory_order_relaxed);

    retireCurrentSample();
    sampleFile = juce::File();
    sampleBuffer.store (nullptr, std::memory_order_release);

    apvts.state.removeProperty (samplePathProperty, nullptr);
}

void SgraffitoProcessor::storeSampleToState (juce::ValueTree& state) const
{
    // A tree without the property speaks about no sample: applying it leaves the
    // sample alone. So only write it when a sample is actually loaded.
    if (sampleBuffer.load (std::memory_order_acquire) == nullptr)
    {
        state.removeProperty (samplePathProperty, nullptr);
        return;
    }

    state.setProperty (samplePathProperty, loadedSample->sourcePath, nullptr);
}

void SgraffitoProcessor::applySampleFromState (const juce::ValueTree& tree)
{
    // Any in-flight background load is superseded by what this tree asks for.
    loadGeneration.fetch_add (1, std::memory_order_relaxed);

    // INIT (the bar and host program 0) applies a hand-made empty tree: the sample
    // resets to the embedded default, like every other parameter resets (Q4).
    // A real preset always carries parameter children, so an empty tree is unambiguous.
    if (tree.isValid() && tree.getNumChildren() == 0 && tree.getNumProperties() == 0)
    {
        loadEmbeddedSample (embeddedSampleUri);
        return;
    }

    // A preset without the property does not speak about samples: leave the sample
    // alone (delta semantics, like the parameters).
    const auto path = tree.getProperty (samplePathProperty).toString();
    if (path.isEmpty())
        return;

    if (path.startsWith ("embedded:"))
    {
        loadEmbeddedSample (path);
        return;
    }

    const juce::File file (path);
    if (file.existsAsFile())
    {
        // The read runs on a background thread (plan 07 phase 4); the current sample
        // keeps playing until the load installs on the message thread.
        loadSampleAsync (file);
        return;
    }

    // Resolution order (plan 07 2.2): the stored path, then the same file name in the
    // user sample folder (a moved sample library). Loading the fallback updates the
    // saved path, so the session remembers the resolution. The embedded fallback is
    // never automatic: a missing sample is reported, never silently substituted.
    const auto searchFolder = sampleSearchFolder.isDirectory() ? sampleSearchFolder : userSampleFolder();
    const auto fallback = searchFolder.getChildFile (file.getFileName());

    if (fallback.existsAsFile())
    {
        loadSampleAsync (fallback);
        return;
    }

    // Missing: keep the current sample playing and report it for the UI. Never silence,
    // never a silent substitution.
    missingSamplePath = path;
    sampleMissing.store (true, std::memory_order_release);
}

void SgraffitoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // The sample is not a parameter; it rides along as the samplePath property
    // (plan 07). It is normally already on the state, the extra callback makes the
    // save not depend on that.
    destData = SuiteState::write (apvts, stateVersion, [this] (juce::ValueTree& state)
    {
        storeSampleToState (state);
    });
}

void SgraffitoProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Old sessions carry no samplePath property: the hook leaves the sample alone,
    // which is the embedded default loaded in prepareToPlay.
    SuiteState::read (data, sizeInBytes, apvts, stateVersion, migrations,
                      [this] (const juce::ValueTree&)
                      {
                          applySampleFromState (apvts.state);
                      });

    // The preset identity rides as a property (plan 09 1.4, D3): restore the bar's
    // selection from the replaced state. A session without the property — every
    // session saved before plan 09 — reads as Init.
    lastPresetSelection = SuitePresets::decodePresetSelection (
        apvts.state.getProperty (presetSelectionProperty).toString());
}

void SgraffitoProcessor::setPresetSelection (const SuitePresets::PresetSelection& selection)
{
    lastPresetSelection = selection;

    // The lerpCurve / samplePath pattern: keep the property in sync at change time, so
    // the identity rides in every saved session and host program dump (copyState
    // serialises the state tree's properties). Nothing changes for absent = Init.
    if (selection.kind == SuitePresets::PresetSelection::Kind::none)
        apvts.state.removeProperty (presetSelectionProperty, nullptr);
    else
        apvts.state.setProperty (presetSelectionProperty,
                                 SuitePresets::encodePresetSelection (selection), nullptr);
}

juce::AudioProcessorEditor* SgraffitoProcessor::createEditor()
{
    return new SgraffitoEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SgraffitoProcessor();
}
