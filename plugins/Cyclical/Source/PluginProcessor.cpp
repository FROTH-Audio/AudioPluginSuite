#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "BinaryData.h"

#include <juce_audio_formats/juce_audio_formats.h>

#if SUITE_PRESET_BANK_AS_PROGRAMS
 #include "PresetBank.h"
#endif

#include <algorithm>
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
        noteParams[(size_t) slot]         = apvts.getRawParameterValue (slotNoteIds[(size_t) slot]);
        beatsParams[(size_t) slot]        = apvts.getRawParameterValue (slotBeatsIds[(size_t) slot]);
        chopParams[(size_t) slot]         = apvts.getRawParameterValue (slotChopIds[(size_t) slot]);
        slicesParams[(size_t) slot]       = apvts.getRawParameterValue (slotSlicesIds[(size_t) slot]);
        sliceLengthParams[(size_t) slot]  = apvts.getRawParameterValue (slotSliceLengthIds[(size_t) slot]);

        loadGenerators[(size_t) slot] = std::make_shared<juce::Atomic<int>> (0);
    }

    pitchShiftParam      = apvts.getRawParameterValue (pitchShiftId);
    crossfadeLengthParam = apvts.getRawParameterValue (crossfadeLengthId);
    tempoParam           = apvts.getRawParameterValue (tempoId);
    syncParam            = apvts.getRawParameterValue (syncId);

    // The auto-chop boundaries: a manual Slice Length edit drops the analyser's shares and
    // falls back to the equal/skew split. A Slices edit needs no listener — the count
    // mismatch already makes activeChopBounds fall back (and the analyser itself writes
    // Slices, which must not clear its own boundaries). The APVTS listener broadcasts on
    // the message thread, so the property write is safe.
    for (int slot = 0; slot < numSlots; ++slot)
        apvts.addParameterListener (slotSliceLengthIds[(size_t) slot], &chopBoundsClearer);

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

    //---- Global (5). Plan 12: the final gain stage is named "Output" everywhere.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { masterVolumeId, 1 }, "Output",
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
            juce::StringArray { "Latch", "Gate", "Toggle" }, 0));

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

        // Plan 14: the slot's trigger note. The defaults are the old fixed map 60-63, so a
        // session without these IDs (resetMissing) reads the same keys.
        params.push_back (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { slotNoteIds[(size_t) slot], 1 }, "Note " + juce::String (slot + 1),
            24, 96, firstSlotNote + slot));

        // Plan 14: the musical length of the whole loaded loop, for Host Sync.
        params.push_back (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { slotBeatsIds[(size_t) slot], 1 }, "Beats " + juce::String (slot + 1),
            1, 16, 4));

        // Plan 14 Phase 3: the chop mode. Sync is disabled for a chopped slot; slice k of
        // the region triggers on Note + k.
        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { slotChopIds[(size_t) slot], 1 }, "Chop " + juce::String (slot + 1), false));

        params.push_back (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { slotSlicesIds[(size_t) slot], 1 }, "Slices " + juce::String (slot + 1),
            2, maxSlices, 8));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { slotSliceLengthIds[(size_t) slot], 1 }, "Slice Length " + juce::String (slot + 1),
            juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, "%"));
    }

    //---- Playback (2)
    params.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { pitchShiftId, 1 }, "Pitch", -12, 12, 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { crossfadeLengthId, 1 }, "Crossfade",
        juce::NormalisableRange<float> (0.0f, 500.0f, 1.0f), 5.0f, "ms"));

    //---- Musical clock (plan 14, appended so no saved index moves)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { tempoId, 1 }, "Tempo",
        juce::NormalisableRange<float> (40.0f, 240.0f, 0.1f), 120.0f, "BPM"));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { syncId, 1 }, "Host Sync", juce::StringArray { "Off", "On" }, 0));

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
    std::fill (toggledOn.begin(), toggledOn.end(), false);
}

void CyclicalProcessor::reset()
{
    // A host reset flushes the pending triggers and silences the voices.
    flushPendingTriggers();
    std::fill (toggledOn.begin(), toggledOn.end(), false);   // plan 14: the toggle state goes too

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

    //---- The beat clock (plan 14). Sync on: the host's position and tempo (the host tempo
    // is authoritative when it reports one). Sync off: the Tempo parameter. The clock only
    // accumulates, so loops never jump on relocate. The effective tempo also drives the
    // synced loop rate (regionFor reads blockBpm).
    const auto* playHead = getPlayHead();
    const auto position = playHead != nullptr ? playHead->getPosition()
                                              : juce::Optional<juce::AudioPlayHead::PositionInfo>();
    blockBpm = effectiveBpm (position);
    const double bpm = blockBpm;

    const double samplesPerBeat = 60.0 / bpm * preparedSampleRate;

    const bool hostDrives = syncParam->load() > 0.5f && position.hasValue()
                         && position->getIsPlaying() && position->getPpqPosition().hasValue();

    if (hostDrives)
    {
        const double ppq = *position->getPpqPosition();

        if (! wasRunning || std::abs (ppq - beatPosition) > 1.0e-3)
        {
            beatPosition = ppq;
            flushPendingTriggers();     // a relocate invalidates the pending grid times
            std::fill (toggledOn.begin(), toggledOn.end(), false);   // plan 14: a relocate clears the toggle state
        }

        wasRunning = true;
    }
    else
    {
        if (wasRunning)
        {
            flushPendingTriggers();     // a transport stop flushes the pending list

            // Plan 14: in Toggle mode a stopped transport stops the toggled slots (the
            // suite's transport-stop rule) and clears the toggle state. Latch keeps its
            // one-shot tails, as today.
            if (juce::jlimit (0, 2, (int) (triggerModeParam->load() + 0.5f)) == 2)
            {
                for (auto& state : slotStates)
                    state.voices[(size_t) 0].release();
            }
            std::fill (toggledOn.begin(), toggledOn.end(), false);
        }

        wasRunning = false;
    }

    const double blockStartBeat = beatPosition;
    const double blockEndBeat = blockStartBeat + numSamples / samplesPerBeat;

    //---- MIDI (plan 14): a slot claims the key its Note parameter names (chop mode widens
    // the map, Phase 3). Other notes are ignored; so is every non-note event.
    const int triggerMode = juce::jlimit (0, 2, (int) (triggerModeParam->load() + 0.5f));   // latch, gate, toggle
    const bool gateMode = triggerMode == 1;
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
        const int slot = slotForNote (key);
        if (slot < 0)
            continue;

        const int slice = sliceForNote (slot, key);

        if (noteOn)
        {
            if (triggerMode == 2)
            {
                // Toggle (plan 14): the first press starts the slot looping (scheduled on
                // the next grid point when Quantize is on); the second press stops it
                // immediately, never delayed to the grid. A note-off does nothing. A
                // chopped slot shares one toggle across its slices (the slot plays at
                // most one looping voice).
                auto& pending = pendingTriggers[(size_t) slot];
                if (toggledOn[(size_t) slot] || pending.active)
                {
                    pending = {};
                    toggledOn[(size_t) slot] = false;
                    releaseSlotVoice (slot);
                }
                else if (grid <= 0.0)
                {
                    startSlotVoice (slot, message.getChannel(), message.getFloatVelocity(), sample, slice);
                    toggledOn[(size_t) slot] = true;
                }
                else
                {
                    pending.active = true;
                    pending.channel = message.getChannel();
                    pending.velocity = message.getFloatVelocity();
                    pending.beatTime = nextGridBeat (blockStartBeat, sample, samplesPerBeat, grid, swingNorm);
                    pending.slice = slice;
                    toggledOn[(size_t) slot] = true;
                }
                continue;
            }

            if (grid <= 0.0)
            {
                startSlotVoice (slot, message.getChannel(), message.getFloatVelocity(), sample, slice);
                pendingTriggers[(size_t) slot] = {};
            }
            else
            {
                // Schedule on the next grid point; a second trigger replaces the pending
                // one, and odd grid points swing by the suite's swingBeats convention.
                auto& pending = pendingTriggers[(size_t) slot];
                pending.active = true;
                pending.channel = message.getChannel();
                pending.velocity = message.getFloatVelocity();
                pending.beatTime = nextGridBeat (blockStartBeat, sample, samplesPerBeat, grid, swingNorm);
                pending.slice = slice;
            }
        }
        else
        {
            if (triggerMode == 2)
                continue;   // a note-off never stops a toggled slot (plan 14)

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
        startSlotVoice (slot, pending.channel, pending.velocity, at, pending.slice);
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

CoreDSP::Loop::Region CyclicalProcessor::regionFor (int slot, const CoreDSP::Loop::Source& view, int slice)
{
    CoreDSP::Loop::Region region;
    const auto frames = CoreDSP::Loop::regionFrames (view.numFrames,
        startParams[(size_t) slot]->load(), lengthParams[(size_t) slot]->load());
    region.startFrame = frames[0];
    region.lengthFrames = frames[1];

    // Plan 14 (G-B): with Host Sync on the whole loop plays in exactly Beats beats — the
    // rate comes from the loop's own duration, so the Speed knob is inactive (the editor
    // dims it). Start and Length choose the played span; they do not change the rate.
    // The voice computes rate = speed x sourceRate / hostRate, so speed here is
    // loopFrames x bpm / (beats x 60 x sourceRate).
    // A chopped slot ignores sync (owner decision): its Speed knob stays live and its
    // Beats value is not used.
    const bool chopped = slotChopped (slot);

    if (syncParam->load() > 0.5f && ! chopped)
    {
        region.speed = (double) view.numFrames * blockBpm
                     / ((double) slotBeats (slot) * 60.0 * std::max (1.0, view.sourceSampleRate));
    }
    else
    {
        region.speed = juce::jlimit (0.25, 4.0, (double) speedParams[(size_t) slot]->load());
    }

    // Plan 14 Phase 3: a chopped slot plays slice k of its region. The slice boundaries
    // come from activeChopBounds(): the auto-chop property when its count matches Slices,
    // the shared power curve otherwise (the same values the waveform draws). Rounded per
    // boundary, the slices sum exactly to the region length.
    // Plan 16 Phase 2: the arrangement maps key slice k to the source slice the order
    // names (identity when the property is absent or its count does not match).
    if (chopped)
    {
        const int slices = slotSlices (slot);
        const int k = juce::jlimit (0, slices - 1, slice);
        const auto bounds = activeChopBounds (slot);

        int sourceSlice = k;
        const auto order = chopOrder (slot);
        if ((int) order.size() == slices && k < (int) order.size())
            sourceSlice = juce::jlimit (0, slices - 1, order[(size_t) k]);

        int sliceStart = region.startFrame;
        int sliceEnd = region.startFrame + region.lengthFrames;

        for (int i = 0; i <= slices; ++i)
        {
            const double share = (i == 0 || i == slices)
                ? (i == 0 ? 0.0 : 1.0)
                : (double) bounds[(size_t) (i - 1)];

            const int boundary = region.startFrame
                + (int) std::lround (share * (double) region.lengthFrames);

            if (i == sourceSlice)
                sliceStart = boundary;
            if (i == sourceSlice + 1)
                sliceEnd = boundary;
        }

        region.startFrame = sliceStart;
        region.lengthFrames = std::max (1, sliceEnd - sliceStart);
    }

    region.direction = (CoreDSP::Loop::Direction) juce::jlimit (0, 2, (int) (directionParams[(size_t) slot]->load() + 0.5f));
    region.granular = granularParams[(size_t) slot]->load() > 0.5f;
    region.pingRandom = pingRandomParams[(size_t) slot]->load() > 0.5f;
    region.crossfadeFrames = (int) std::lround (
        juce::jlimit (0.0f, 500.0f, crossfadeLengthParam->load()) * 0.001 * preparedSampleRate);

    return region;
}

void CyclicalProcessor::startSlotVoice (int slot, int channel, float velocity, int sampleInBlock, int slice)
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

    state.voices[0].start (view, regionFor (slot, view, slice), pitchRatio, gain,
                           triggerCounter.fetch_add (1, std::memory_order_relaxed));
    state.renderFrom[0] = sampleInBlock;
}

void CyclicalProcessor::releaseSlotVoice (int slot)
{
    slotStates[(size_t) slot].voices[(size_t) 0].release();
}

double CyclicalProcessor::effectiveBpm (const juce::Optional<juce::AudioPlayHead::PositionInfo>& position) const
{
    const bool synced = syncParam->load() > 0.5f;
    double bpm = juce::jlimit (40.0f, 240.0f, tempoParam->load());

    if (synced && position.hasValue())
        if (const auto hostBpm = position->getBpm(); hostBpm.hasValue() && *hostBpm > 1.0)
            bpm = juce::jlimit (20.0, 300.0, (double) *hostBpm);

    return bpm;
}

int CyclicalProcessor::slotNote (int slot) const
{
    return juce::jlimit (24, 96, (int) (noteParams[(size_t) slot]->load() + 0.5f));
}

int CyclicalProcessor::slotForNote (int key) const
{
    for (int slot = 0; slot < numSlots; ++slot)
    {
        const int base = slotNote (slot);

        if (slotChopped (slot))
        {
            // A chopped slot claims base .. base + slices - 1 (slice k on base + k).
            if (key >= base && key < base + slotSlices (slot))
                return slot;
        }
        else if (key == base)
        {
            return slot;
        }
    }

    return -1;
}

int CyclicalProcessor::sliceForNote (int slot, int key) const
{
    if (! slotChopped (slot))
        return 0;

    return juce::jlimit (0, slotSlices (slot) - 1, key - slotNote (slot));
}

int CyclicalProcessor::slotBeats (int slot) const
{
    return juce::jlimit (1, 16, (int) (beatsParams[(size_t) slot]->load() + 0.5f));
}

bool CyclicalProcessor::slotChopped (int slot) const
{
    return chopParams[(size_t) slot]->load() > 0.5f;
}

int CyclicalProcessor::slotSlices (int slot) const
{
    return juce::jlimit (2, maxSlices, (int) (slicesParams[(size_t) slot]->load() + 0.5f));
}

double CyclicalProcessor::chopBoundary (int i, int slices, float sliceLengthPercent)
{
    // b_i = (i/S)^e with e = 2^(2 v): v = 0 is the equal split, positive v lengthens the
    // late slices (the derivative e x^(e-1) grows with x), negative the early ones. The
    // ends are pinned exactly, so the boundaries always span the whole region.
    const double s = std::max (2, slices);
    const double v = juce::jlimit (-1.0f, 1.0f, sliceLengthPercent / 100.0f);

    if (i <= 0)
        return 0.0;
    if (i >= (int) s)
        return 1.0;

    const double e = std::pow (2.0, 2.0 * v);
    return std::pow ((double) i / s, e);
}

double CyclicalProcessor::nextGridBeat (double blockStartBeat, int sampleInBlock, double samplesPerBeat,
                                        double grid, float swingNorm) const
{
    const double beatAtEvent = blockStartBeat + (double) sampleInBlock / samplesPerBeat;
    const int gridIndex = (int) std::ceil (beatAtEvent / grid - 1.0e-9);

    return (double) gridIndex * grid + CoreDSP::Sequencer::swingBeats (gridIndex, swingNorm, grid);
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

juce::String CyclicalProcessor::chopBoundsPropertyId (int slot) const
{
    switch (slot)
    {
        case 0: return chop1BoundsProperty;
        case 1: return chop2BoundsProperty;
        case 2: return chop3BoundsProperty;
        default: return chop4BoundsProperty;
    }
}

void CyclicalProcessor::setChopBoundsProperty (int slot, const std::vector<float>& shares)
{
    if (shares.empty())
    {
        apvts.state.removeProperty (chopBoundsPropertyId (slot), nullptr);
        return;
    }

    juce::String text;

    for (size_t i = 0; i < shares.size(); ++i)
    {
        if (i > 0)
            text << ",";
        text << juce::String (shares[i], 4);
    }

    apvts.state.setProperty (chopBoundsPropertyId (slot), text, nullptr);
}

std::vector<float> CyclicalProcessor::chopBounds (int slot) const
{
    const auto text = apvts.state.getProperty (chopBoundsPropertyId (slot)).toString();

    std::vector<float> shares;
    juce::StringArray parts;
    parts.addTokens (text, ",", "");

    for (const auto& part : parts)
    {
        const float share = (float) part.trim().getFloatValue();

        if (share > 0.0f && share < 1.0f)
            shares.push_back (share);
    }

    return shares;
}

std::vector<float> CyclicalProcessor::activeChopBounds (int slot) const
{
    const auto autoBounds = chopBounds (slot);

    if (slotChopped (slot) && (int) autoBounds.size() + 1 == slotSlices (slot))
        return autoBounds;

    const int slices = slotSlices (slot);
    const float sliceLength = sliceLengthParams[(size_t) slot]->load();

    std::vector<float> shares;
    for (int i = 1; i < slices; ++i)
        shares.push_back ((float) chopBoundary (i, slices, sliceLength));

    return shares;
}

juce::String CyclicalProcessor::chopOrderPropertyId (int slot) const
{
    switch (slot)
    {
        case 0: return chop1OrderProperty;
        case 1: return chop2OrderProperty;
        case 2: return chop3OrderProperty;
        default: return chop4OrderProperty;
    }
}

std::vector<int> CyclicalProcessor::chopOrder (int slot) const
{
    const auto text = apvts.state.getProperty (chopOrderPropertyId (slot)).toString();

    std::vector<int> order;
    juce::StringArray parts;
    parts.addTokens (text, ",", "");

    for (const auto& part : parts)
        order.push_back ((int) part.trim().getIntValue());

    return order;
}

void CyclicalProcessor::setChopOrderProperty (int slot, const std::vector<int>& order)
{
    if (order.empty())
    {
        apvts.state.removeProperty (chopOrderPropertyId (slot), nullptr);
        return;
    }

    juce::String text;

    for (size_t i = 0; i < order.size(); ++i)
    {
        if (i > 0)
            text << ",";
        text << order[i];
    }

    apvts.state.setProperty (chopOrderPropertyId (slot), text, nullptr);
}

void CyclicalProcessor::cycleSliceOrder (int slot, int index)
{
    if (! slotChopped (slot))
        return;

    const int slices = slotSlices (slot);
    auto order = chopOrder (slot);

    if ((int) order.size() != slices)   // identity arrangement: build it first
    {
        order.resize ((size_t) slices);
        for (int i = 0; i < slices; ++i)
            order[(size_t) i] = i;
    }

    const int at = juce::jlimit (0, slices - 1, index);
    order[(size_t) at] = (order[(size_t) at] + 1) % slices;

    // A full identity round returns to no property, so the strip can reset.
    bool identity = true;
    for (int i = 0; i < slices; ++i)
        if (order[(size_t) i] != i)
            identity = false;

    setChopOrderProperty (slot, identity ? std::vector<int> {} : order);
}

//==============================================================================
// Auto-chop (plan 16 Phase 1): the transient analyser behind the AUTO button.

void CyclicalProcessor::autoChopSlot (int slot)
{
    const LoadedLoop* loaded = slotStates[(size_t) slot].loop.current();

    if (loaded == nullptr)
        return;

    const auto view = loaded->source();
    if (view.numFrames <= 0)
        return;

    const auto region = CoreDSP::Loop::regionFrames (view.numFrames,
        startParams[(size_t) slot]->load(), lengthParams[(size_t) slot]->load());
    const int regionStart = region[0];
    const int regionLength = region[1];
    if (regionLength < 4 * maxSlices)      // too short to chop usefully
        return;

    // The envelope: block maxima over 256-frame windows inside the region.
    constexpr int window = 256;
    const int numWindows = regionLength / window;
    if (numWindows < 4)
        return;

    std::vector<float> envelope ((size_t) numWindows);
    float envelopeMax = 0.0f;

    for (int w = 0; w < numWindows; ++w)
    {
        float level = 0.0f;
        for (int n = 0; n < window; ++n)
            level = juce::jmax (level, std::abs (view.channels[0][regionStart + w * window + n]));
        envelope[(size_t) w] = level;
        envelopeMax = juce::jmax (envelopeMax, level);
    }

    if (envelopeMax <= 0.0f)
        return;

    // Onsets: a window rising through 15 % of the loop's peak with a clear rise, merged
    // closer than 100 ms (the threshold keeps quiet tails from splitting slices).
    constexpr float thresholdShare = 0.15f;
    constexpr int minGapMs = 100;
    const int minGap = std::max (1, (int) (minGapMs * 0.001 * loaded->sourceSampleRate / window));

    std::vector<int> onsets;   // window indices

    for (int w = 1; w < numWindows; ++w)
    {
        const float level = envelope[(size_t) w];

        if (level < envelopeMax * thresholdShare)
            continue;
        if (level < envelope[(size_t) (w - 1)] * 1.5f)
            continue;

        if (! onsets.empty() && w - onsets.back() < minGap)
            continue;

        onsets.push_back (w);
    }

    // Too many onsets for 16 slices: widen the merge window until the count fits.
    while ((int) onsets.size() > maxSlices - 1)
    {
        std::vector<int> merged;

        for (size_t i = 0; i < onsets.size(); i += 2)
        {
            if (i + 1 < onsets.size() && envelope[(size_t) onsets[i + 1]] > envelope[(size_t) onsets[i]])
                merged.push_back (onsets[i + 1]);
            else
                merged.push_back (onsets[i]);
        }

        onsets = merged;
    }

    if (onsets.empty())
        return;

    // The shares are the onset positions as a fraction of the region; each is the START of
    // a slice, so the list is the interior boundaries (slice 0 always starts the region).
    std::vector<float> shares;

    for (int windowIndex : onsets)
        shares.push_back (juce::jlimit (0.0f, 1.0f, (float) (windowIndex * window) / (float) regionLength));

    setChopBoundsProperty (slot, shares);
    setChopOrderProperty (slot, {});   // a fresh analysis resets the arrangement
    apvts.getParameter (slotSlicesIds[(size_t) slot])->setValueNotifyingHost (
        apvts.getParameter (slotSlicesIds[(size_t) slot])->convertTo0to1 ((float) ((int) shares.size() + 1)));
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
