#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "BinaryData.h"

#include <juce_audio_formats/juce_audio_formats.h>

#if SUITE_PRESET_BANK_AS_PROGRAMS
 #include "PresetBank.h"
#endif

#include <cmath>
#include <cstring>

//==============================================================================
// Loop decoding helpers. registerBasicFormats() reads WAV and AIFF only, so those are the
// accepted file patterns.

namespace
{
    std::unique_ptr<LoadedLoop> readLoopFromFile (const juce::File& file)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        if (reader == nullptr)
            return {};

        const int frames = (int) reader->lengthInSamples;
        if (frames <= 0)
            return {};

        juce::AudioBuffer<float> buffer ((int) reader->numChannels, frames);
        reader->read (&buffer, 0, frames, 0, true, true);

        return makeLoadedLoop (std::move (buffer), reader->sampleRate, file.getFullPathName());
    }

    std::unique_ptr<LoadedLoop> readEmbeddedLoop (const juce::String& name)
    {
        // Resource names keep only alphanumerics ("kick_4k.wav" becomes "kick_4k_wav").
        const juce::String resourceName = name.replaceCharacter ('.', '_').replaceCharacter ('-', '_');
        int dataSize = 0;
        const char* data = BinaryData::getNamedResource (resourceName.toRawUTF8(), dataSize);
        if (data == nullptr || dataSize <= 0)
            return {};      // an unknown name silently keeps the current loop

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (
            std::unique_ptr<juce::InputStream> (new juce::MemoryInputStream (data, (size_t) dataSize, false))));

        if (reader == nullptr)
            return {};

        const int frames = (int) reader->lengthInSamples;
        if (frames <= 0)
            return {};

        juce::AudioBuffer<float> buffer ((int) reader->numChannels, frames);
        reader->read (&buffer, 0, frames, 0, true, true);

        return makeLoadedLoop (std::move (buffer), reader->sampleRate, "embedded:" + name);
    }
}

//==============================================================================
CyclicalProcessor::CyclicalProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    masterVolumeParam = apvts.getRawParameterValue (masterVolumeId);
    panParam          = apvts.getRawParameterValue (panId);
    quantizeParam     = apvts.getRawParameterValue (quantizeId);
    swingParam        = apvts.getRawParameterValue (swingId);
    triggerModeParam  = apvts.getRawParameterValue (triggerModeId);

    for (int slot = 0; slot < numSlots; ++slot)
    {
        startParams[(size_t) slot]        = apvts.getRawParameterValue (slotStartIds[(size_t) slot]);
        lengthParams[(size_t) slot]       = apvts.getRawParameterValue (slotLengthIds[(size_t) slot]);
        speedParams[(size_t) slot]        = apvts.getRawParameterValue (slotSpeedIds[(size_t) slot]);
        directionParams[(size_t) slot]    = apvts.getRawParameterValue (slotDirectionIds[(size_t) slot]);
        pingRandomParams[(size_t) slot]   = apvts.getRawParameterValue (slotPingRandomIds[(size_t) slot]);
        granularParams[(size_t) slot]     = apvts.getRawParameterValue (slotGranularIds[(size_t) slot]);
        velocityParams[(size_t) slot]     = apvts.getRawParameterValue (slotVelocityIds[(size_t) slot]);

        loadGenerators[(size_t) slot] = std::make_shared<juce::Atomic<int>> (0);
    }

    pitchShiftParam      = apvts.getRawParameterValue (pitchShiftId);
    crossfadeLengthParam = apvts.getRawParameterValue (crossfadeLengthId);

    sampleSearchFolder = defaultUserLoopFolder();

    // INIT installs one embedded loop per slot, so a fresh instance makes sound on the four
    // keys without loading anything. The saved paths ride as state properties at change time.
    for (int slot = 0; slot < numSlots; ++slot)
    {
        const juce::String name = juce::String (defaultLoopNames[(size_t) slot]) + "_4k.wav";
        loadEmbeddedLoop (slot, name, juce::String (embeddedUriPrefix) + name);
    }
}

CyclicalProcessor::~CyclicalProcessor()
{
    // Kill the in-flight load handoffs first, so a queued lambda becomes a no-op before the
    // members it would touch go away. The pool (declared last) then waits for its jobs.
    for (auto& token : loadAliveTokens)
        if (token != nullptr)
            token->store (false, std::memory_order_release);
}

juce::AudioProcessorValueTreeState::ParameterLayout CyclicalProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    //---- Global (5)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { masterVolumeId, 1 }, "Master Volume",
        juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, "dB"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { panId, 1 }, "Pan",
        juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { quantizeId, 1 }, "Quantize",
        juce::StringArray { "Off", "1/16", "1/8", "1/4", "1/2", "1 Bar" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { swingId, 1 }, "Swing",
        juce::NormalisableRange<float> (-20.0f, 20.0f, 0.1f), 0.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { triggerModeId, 1 }, "Trigger Mode",
        juce::StringArray { "Latch", "Gate" }, 0));

    //---- Per slot (7 x 4), slot 1 first so the bank order is stable
    for (int slot = 0; slot < numSlots; ++slot)
    {
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { slotStartIds[(size_t) slot], 1 }, "Start " + juce::String (slot + 1),
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, "%"));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { slotLengthIds[(size_t) slot], 1 }, "Length " + juce::String (slot + 1),
            juce::NormalisableRange<float> (5.0f, 100.0f, 0.1f), 100.0f, "%"));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { slotSpeedIds[(size_t) slot], 1 }, "Speed " + juce::String (slot + 1),
            juce::NormalisableRange<float> (0.25f, 4.0f, 0.0f, 0.5f), 1.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                [] (float value, int) { return juce::String (value, 2) + " x"; })));

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { slotDirectionIds[(size_t) slot], 1 }, "Direction " + juce::String (slot + 1),
            juce::StringArray { "Forward", "Reverse", "Ping-Pong" }, 0));

        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { slotPingRandomIds[(size_t) slot], 1 }, "Ping Rand " + juce::String (slot + 1), false));

        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { slotGranularIds[(size_t) slot], 1 }, "Grain " + juce::String (slot + 1), false));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { slotVelocityIds[(size_t) slot], 1 }, "Velocity " + juce::String (slot + 1),
            juce::NormalisableRange<float> (30.0f, 150.0f, 0.1f), 100.0f, "%"));
    }

    //---- Playback (2)
    params.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { pitchShiftId, 1 }, "Pitch", -12, 12, 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { crossfadeLengthId, 1 }, "Crossfade",
        juce::NormalisableRange<float> (0.0f, 500.0f, 1.0f), 5.0f, "ms"));

    return { params.begin(), params.end() };
}

void CyclicalProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    preparedSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    for (auto& state : slotStates)
    {
        for (auto& voice : state.voices)
            voice.reset();
        state.renderFrom = { 0, 0 };
    }

    flushPendingTriggers();

    masterSmoother.reset (preparedSampleRate, 0.02);
    panSmoother.reset (preparedSampleRate, 0.02);
    masterSmoother.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (masterVolumeParam->load()));
    panSmoother.setCurrentAndTargetValue (juce::jlimit (-100.0f, 100.0f, panParam->load()) / 100.0f);

    const auto size = (size_t) juce::jmax (1, samplesPerBlock);
    gainCurveL.assign (size, 0.0f);
    gainCurveR.assign (size, 1.0f);

    beatPosition = 0.0;
    wasRunning = false;
}

void CyclicalProcessor::reset()
{
    // A host reset flushes the pending triggers and silences the voices.
    flushPendingTriggers();

    for (auto& state : slotStates)
    {
        for (auto& voice : state.voices)
            voice.reset();
        state.renderFrom = { 0, 0 };
    }
}

bool CyclicalProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // An instrument: output only, stereo.
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void CyclicalProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0)
        return;

    buffer.clear();

    // Every loop is bounded by the buffer's own channel count, never by the declared output
    // width: the host-load test hands an instrument a mono buffer while reporting two
    // outputs. The slot mix folds into the available width (mono: the pan gains summed).
    const int outChannels = buffer.getNumChannels();

    // The host rate is read per block, so a rate change mid-note keeps the speed right.
    for (auto& state : slotStates)
        for (auto& voice : state.voices)
            voice.prepare (preparedSampleRate);

    //---- The beat clock. Host PPQ while the host plays; otherwise accrued at the host
    // tempo (120 BPM with no playhead). It only accumulates, so loops never jump.
    const auto* playHead = getPlayHead();
    const auto position = playHead != nullptr ? playHead->getPosition()
                                              : juce::Optional<juce::AudioPlayHead::PositionInfo>();
    double bpm = 120.0;

    if (position.hasValue())
        if (const auto hostBpm = position->getBpm(); hostBpm.hasValue() && *hostBpm > 1.0)
            bpm = juce::jlimit (20.0, 300.0, (double) *hostBpm);

    const double samplesPerBeat = 60.0 / bpm * preparedSampleRate;

    const bool hostDrives = position.hasValue() && position->getIsPlaying()
                         && position->getPpqPosition().hasValue();

    if (hostDrives)
    {
        const double ppq = *position->getPpqPosition();

        if (! wasRunning || std::abs (ppq - beatPosition) > 1.0e-3)
        {
            beatPosition = ppq;
            flushPendingTriggers();     // a relocate invalidates the pending grid times
        }

        wasRunning = true;
    }
    else
    {
        if (wasRunning)
            flushPendingTriggers();     // a transport stop flushes the pending list

        wasRunning = false;
    }

    const double blockStartBeat = beatPosition;
    const double blockEndBeat = blockStartBeat + numSamples / samplesPerBeat;

    //---- MIDI: the fixed map, notes 60-63. Other notes are ignored; so is every
    // non-note event.
    const bool gateMode = triggerModeParam->load() > 0.5f;
    static constexpr double grids[6] = { 0.0, 0.25, 0.5, 1.0, 2.0, 4.0 };
    const double grid = grids[juce::jlimit (0, 5, (int) (quantizeParam->load() + 0.5f))];
    const float swingNorm = juce::jlimit (-0.2f, 0.2f, swingParam->load() / 100.0f);

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        const int sample = (int) metadata.samplePosition;

        // A note-on with velocity 0 is a note-off (the host convention).
        const bool noteOn = message.isNoteOn() && message.getVelocity() > 0;
        const bool noteOff = message.isNoteOff() || (message.isNoteOn() && message.getVelocity() == 0);

        if (! noteOn && ! noteOff)
            continue;

        const int key = message.getNoteNumber();
        if (key < firstSlotNote || key >= firstSlotNote + numSlots)
            continue;

        const int slot = key - firstSlotNote;

        if (noteOn)
        {
            if (grid <= 0.0)
            {
                startSlotVoice (slot, message.getChannel(), message.getFloatVelocity(), sample);
                pendingTriggers[(size_t) slot] = {};
            }
            else
            {
                // Schedule on the next grid point; a second trigger replaces the pending
                // one, and odd grid points swing by the suite's swingBeats convention.
                const double beatAtEvent = blockStartBeat + (double) sample / samplesPerBeat;
                const int gridIndex = (int) std::ceil (beatAtEvent / grid - 1.0e-9);
                const double beatTime = (double) gridIndex * grid
                    + CoreDSP::Sequencer::swingBeats (gridIndex, swingNorm, grid);

                auto& pending = pendingTriggers[(size_t) slot];
                pending.active = true;
                pending.channel = message.getChannel();
                pending.velocity = message.getFloatVelocity();
                pending.beatTime = beatTime;
            }
        }
        else
        {
            // A note-off before the scheduled start cancels the pending trigger (a tap
            // shorter than the grid step produces nothing in Gate mode).
            pendingTriggers[(size_t) slot] = {};

            if (gateMode)
                releaseSlotVoice (slot);
        }
    }

    //---- Fire the pending triggers that fall inside this block.
    for (int slot = 0; slot < numSlots; ++slot)
    {
        auto& pending = pendingTriggers[(size_t) slot];
        if (! pending.active || pending.beatTime >= blockEndBeat)
            continue;

        const int at = juce::jlimit (0, juce::jmax (0, numSamples - 1),
            (int) std::lround ((pending.beatTime - blockStartBeat) * samplesPerBeat));
        startSlotVoice (slot, pending.channel, pending.velocity, at);
        pending = {};
    }

    //---- Render the voices. Each slot renders at most two voices: the active one and the
    // fading tail of a retrigger.
    for (auto& state : slotStates)
    {
        for (int v = 0; v < 2; ++v)
        {
            auto& voice = state.voices[(size_t) v];
            if (! voice.isActive())
                continue;

            const int from = juce::jlimit (0, juce::jmax (0, numSamples - 1), state.renderFrom[(size_t) v]);
            voice.render (buffer, from, numSamples - from);
            state.renderFrom[(size_t) v] = 0;
        }
    }

    //---- Master stage: smoothed volume and constant-power pan as per-sample curves, both
    // channels reading the same curve (the Spectral Drift lesson).
    masterSmoother.setTargetValue (juce::Decibels::decibelsToGain (
        juce::jlimit (-24.0f, 12.0f, masterVolumeParam->load())));
    panSmoother.setTargetValue (juce::jlimit (-100.0f, 100.0f, panParam->load()) / 100.0f);

    if ((size_t) numSamples > gainCurveL.size())
    {
        gainCurveL.resize ((size_t) numSamples);
        gainCurveR.resize ((size_t) numSamples);
    }

    for (int n = 0; n < numSamples; ++n)
    {
        const float master = masterSmoother.getNextValue();
        const float pan = panSmoother.getNextValue();

        // Constant-power law: -3.01 dB at centre, the two gains always sum in power.
        const double angle = (double) (pan + 1.0f) * juce::MathConstants<double>::pi * 0.25;
        gainCurveL[(size_t) n] = (float) (master * std::cos (angle));
        gainCurveR[(size_t) n] = (float) (master * std::sin (angle));
    }

    if (outChannels >= 2)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            const auto& curve = ch == 0 ? gainCurveL : gainCurveR;

            for (int n = 0; n < numSamples; ++n)
                data[n] *= curve[(size_t) n];
        }
    }
    else if (outChannels == 1)
    {
        // Mono: fold the pan's two gains into one (the energy is preserved).
        auto* data = buffer.getWritePointer (0);

        for (int n = 0; n < numSamples; ++n)
            data[n] *= gainCurveL[(size_t) n] + gainCurveR[(size_t) n];
    }

    //---- UI snapshots and the clock advance.
    for (int slot = 0; slot < numSlots; ++slot)
    {
        auto& state = slotStates[(size_t) slot];
        activeFlags[(size_t) slot].store (state.voices[(size_t) 0].isActive(), std::memory_order_relaxed);
        playheads[(size_t) slot].store (state.voices[(size_t) 0].isActive()
            ? (float) state.voices[(size_t) 0].playheadNormalised() : 0.0f, std::memory_order_relaxed);
    }

    beatPositionUiValue.store (blockStartBeat, std::memory_order_relaxed);
    beatPosition = blockEndBeat;
}

//==============================================================================
// Trigger plumbing

CoreDSP::Loop::Region CyclicalProcessor::regionFor (int slot, int numFrames) const
{
    CoreDSP::Loop::Region region;
    const auto frames = CoreDSP::Loop::regionFrames (numFrames,
        startParams[(size_t) slot]->load(), lengthParams[(size_t) slot]->load());
    region.startFrame = frames[0];
    region.lengthFrames = frames[1];

    region.speed = juce::jlimit (0.25, 4.0, (double) speedParams[(size_t) slot]->load());
    region.direction = (CoreDSP::Loop::Direction) juce::jlimit (0, 2, (int) (directionParams[(size_t) slot]->load() + 0.5f));
    region.granular = granularParams[(size_t) slot]->load() > 0.5f;
    region.pingRandom = pingRandomParams[(size_t) slot]->load() > 0.5f;
    region.crossfadeFrames = (int) std::lround (
        juce::jlimit (0.0f, 500.0f, crossfadeLengthParam->load()) * 0.001 * preparedSampleRate);

    return region;
}

void CyclicalProcessor::startSlotVoice (int slot, int channel, float velocity, int sampleInBlock)
{
    auto& state = slotStates[(size_t) slot];
    const LoadedLoop* loaded = state.loop.current();

    if (loaded == nullptr)
        return;

    const auto view = loaded->source();
    if (view.numFrames <= 0)
        return;

    const double pitchRatio = std::pow (2.0,
        juce::jlimit (-12, 12, (int) (pitchShiftParam->load() + 0.5f)) / 12.0);
    const double gain = juce::jlimit (0.0f, 1.0f, velocity)
        * juce::jlimit (30.0f, 150.0f, velocityParams[(size_t) slot]->load()) / 100.0f;

    // A retrigger cuts the active voice into the tail (stopping a tail that is still
    // fading - a 2 ms cut is inaudible), then starts the fresh voice.
    if (state.voices[0].isActive())
    {
        state.voices[1] = state.voices[0];
        state.voices[1].cut();
        state.renderFrom[1] = state.renderFrom[0];
    }

    state.voices[0].start (view, regionFor (slot, view.numFrames), pitchRatio, gain,
                           triggerCounter.fetch_add (1, std::memory_order_relaxed));
    state.renderFrom[0] = sampleInBlock;
}

void CyclicalProcessor::releaseSlotVoice (int slot)
{
    slotStates[(size_t) slot].voices[(size_t) 0].release();
}

void CyclicalProcessor::flushPendingTriggers()
{
    for (auto& pending : pendingTriggers)
        pending = {};
}

//==============================================================================
// Loop loading (plan 07 semantics)

juce::String CyclicalProcessor::loopPathPropertyId (int slot)
{
    switch (slot)
    {
        case 0: return loop1PathProperty;
        case 1: return loop2PathProperty;
        case 2: return loop3PathProperty;
        default: return loop4PathProperty;
    }
}

juce::String CyclicalProcessor::loopPath (int slot) const
{
    return apvts.state.getProperty (loopPathPropertyId (slot)).toString();
}

juce::File CyclicalProcessor::defaultUserLoopFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("AudioSuite").getChildFile ("Cyclical").getChildFile ("Samples");
}

void CyclicalProcessor::setLoopPathProperty (int slot, const juce::String& path)
{
    // The samplePath pattern (plan 07): keep the property in sync at change time, so the
    // reference rides in every saved session and host program dump.
    if (path.isEmpty())
        apvts.state.removeProperty (loopPathPropertyId (slot), nullptr);
    else
        apvts.state.setProperty (loopPathPropertyId (slot), path, nullptr);
}

void CyclicalProcessor::loadEmbeddedLoop (int slot, const juce::String& name, const juce::String& pathToSave)
{
    auto loop = readEmbeddedLoop (name);
    if (loop == nullptr)
        return;     // an unknown resource name silently keeps the current loop

    loopMissing[(size_t) slot].store (false, std::memory_order_relaxed);
    slotStates[(size_t) slot].loop.install (std::move (loop));
    pendingTriggers[(size_t) slot] = {};     // a loop swap on the slot flushes its pending

    if (pathToSave.isNotEmpty())
        setLoopPathProperty (slot, pathToSave);
}

void CyclicalProcessor::loadLoopFromFileAsync (int slot, const juce::File& file, const juce::String& pathToSave)
{
    auto alive = std::make_shared<std::atomic<bool>> (true);
    loadAliveTokens[(size_t) slot] = alive;

    auto generator = loadGenerators[(size_t) slot];
    const int stamp = (*generator) += 1;

    pendingLoopLoads.fetch_add (1, std::memory_order_relaxed);

    sampleLoadPool.addJob ([this, slot, file, pathToSave, alive, generator, stamp]
    {
        // The read runs off the message thread; the install happens on it.
        auto loaded = readLoopFromFile (file);

        juce::MessageManager::callAsync ([this, slot, pathToSave, alive, generator, stamp,
                                          loaded = std::move (loaded)]() mutable
        {
            if (alive != nullptr && ! alive->load (std::memory_order_acquire))
                return;     // the processor is gone: a queued handoff is a no-op

            pendingLoopLoads.fetch_sub (1, std::memory_order_relaxed);

            if (stamp != generator->get())
                return;     // an out-of-order install: the latest request wins

            if (loaded != nullptr)
                finishLoopLoad (slot, std::move (loaded), pathToSave);
        });
    });
}

void CyclicalProcessor::finishLoopLoad (int slot, std::unique_ptr<LoadedLoop> loop, const juce::String& pathToSave)
{
    loopMissing[(size_t) slot].store (false, std::memory_order_relaxed);
    slotStates[(size_t) slot].loop.install (std::move (loop));
    pendingTriggers[(size_t) slot] = {};

    if (pathToSave.isNotEmpty())
        setLoopPathProperty (slot, pathToSave);
}

void CyclicalProcessor::applyLoopsFromState (const juce::ValueTree& tree)
{
    // An empty <PARAMETERS> tree is INIT (the bar and host program 0 apply hand-made empty
    // trees): every slot resets to its embedded default. A delta without a loop reference
    // leaves that slot alone. Reads the incoming tree, not the replaced state (the plan 07
    // phase 1 lesson).
    const bool emptyInit = tree.getNumChildren() == 0
        && ! tree.hasProperty (loop1PathProperty) && ! tree.hasProperty (loop2PathProperty)
        && ! tree.hasProperty (loop3PathProperty) && ! tree.hasProperty (loop4PathProperty);

    for (int slot = 0; slot < numSlots; ++slot)
    {
        const auto path = emptyInit ? juce::String() : tree.getProperty (loopPathPropertyId (slot)).toString();

        if (emptyInit)
        {
            const juce::String name = juce::String (defaultLoopNames[(size_t) slot]) + "_4k.wav";
            loadEmbeddedLoop (slot, name, juce::String (embeddedUriPrefix) + name);
            continue;
        }

        if (path.isEmpty())
            continue;       // delta semantics: no property, no action

        if (path.startsWith (embeddedUriPrefix))
        {
            loadEmbeddedLoop (slot, path.substring ((int) std::strlen (embeddedUriPrefix)), path);
            continue;
        }

        // A file path: existing files load asynchronously; a missing one is searched for
        // by name in the user folder, and if that fails too, the current loop keeps
        // playing with the missing overlay up.
        juce::File file (path);

        if (! file.existsAsFile())
            file = sampleSearchFolder.getChildFile (file.getFileName());

        if (file.existsAsFile())
        {
            loopMissing[(size_t) slot].store (false, std::memory_order_relaxed);
            loadLoopFromFileAsync (slot, file, path);
        }
        else
        {
            loopMissing[(size_t) slot].store (true, std::memory_order_relaxed);
            missingPaths[(size_t) slot] = path;
        }
    }
}

//==============================================================================
// State

juce::AudioProcessorEditor* CyclicalProcessor::createEditor()
{
    return new CyclicalEditor (*this);
}

void CyclicalProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    destData = SuiteState::write (apvts, stateVersion);
}

void CyclicalProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    SuiteState::read (data, sizeInBytes, apvts, stateVersion, migrations);

    // The loop references ride as properties on the replaced state; resolve them.
    applyLoopsFromState (apvts.state);

    // The preset identity rides as a property (plan 09 1.4): restore it from the replaced
    // state. A session without the property reads as Init.
    lastPresetSelection = SuitePresets::decodePresetSelection (
        apvts.state.getProperty (presetSelectionProperty).toString());
}

void CyclicalProcessor::setPresetSelection (const SuitePresets::PresetSelection& selection)
{
    lastPresetSelection = selection;

    if (selection.kind == SuitePresets::PresetSelection::Kind::none)
        apvts.state.removeProperty (presetSelectionProperty, nullptr);
    else
        apvts.state.setProperty (presetSelectionProperty,
                                 SuitePresets::encodePresetSelection (selection), nullptr);
}

void CyclicalProcessor::applyPresetState (const juce::ValueTree& tree)
{
    // A preset is a delta on the default state, so resetMissing is true. The tree goes
    // through the same read path as a saved session: version check, migrations, replaceState.
    const juce::MemoryBlock block = SuiteState::detail::encodeXml (*tree.createXml());
    const auto result = SuiteState::read (block.getData(), (int) block.getSize(), apvts, stateVersion,
                                          migrations, {}, true);

    if (result == SuiteState::LoadResult::loaded)
    {
        // The caller (the bar or the host program bridge) re-states the selection right
        // after, when it knows the preset.
        lastPresetSelection = {};

#if SUITE_PRESET_BANK_AS_PROGRAMS
        // A preset applied from the bar or a file is no longer one of the host programs.
        currentProgramIndex.store (-1, std::memory_order_relaxed);
#endif
    }

    // The loops resolve from the incoming tree (the plan 07 phase 1 lesson), so an INIT's
    // empty tree resets the slots even though the replaced state gains a version attribute.
    applyLoopsFromState (tree);
}

//==============================================================================
// Host program list. Program 0 is Init; 1 to N are the factory presets in bank order. The list only
// exists in a Pro build with SUITE_PRESET_BANK_AS_PROGRAMS on; otherwise the plugin reports one
// program, as it did before the preset system.

int CyclicalProcessor::getNumPrograms()
{
#if SUITE_PRESET_BANK_AS_PROGRAMS
    return SuitePresets::PresetPrograms::getNumPrograms (getPresetPluginId());
#else
    return 1;
#endif
}

int CyclicalProcessor::getCurrentProgram()
{
#if SUITE_PRESET_BANK_AS_PROGRAMS
    return currentProgramIndex.load (std::memory_order_relaxed);
#else
    return 0;
#endif
}

void CyclicalProcessor::setCurrentProgram (int index)
{
#if SUITE_PRESET_BANK_AS_PROGRAMS
    if (SuitePresets::PresetPrograms::applyProgram (*this, index))
    {
        currentProgramIndex.store (index, std::memory_order_relaxed);

        // The host program and the bar's selection describe the same event (plan 09 1.4).
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

const juce::String CyclicalProcessor::getProgramName (int index)
{
#if SUITE_PRESET_BANK_AS_PROGRAMS
    return SuitePresets::PresetPrograms::getProgramName (getPresetPluginId(), index);
#else
    juce::ignoreUnused (index);
    return {};
#endif
}

void CyclicalProcessor::changeProgramName (int, const juce::String&)
{
    // The factory programs are read-only.
}

void CyclicalProcessor::getCurrentProgramStateInformation (juce::MemoryBlock& destData)
{
    getStateInformation (destData);
}

void CyclicalProcessor::setCurrentProgramStateInformation (const void* data, int sizeInBytes)
{
    setStateInformation (data, sizeInBytes);

#if SUITE_PRESET_BANK_AS_PROGRAMS
    currentProgramIndex.store (-1, std::memory_order_relaxed);
#endif
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CyclicalProcessor();
}
