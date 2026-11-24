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

namespace
{
    /** Plan 18 Phase 6: the per-slot modulation targets, in ModTarget order (append-only;
        the choice index is the position + 1). Level, Cutoff, Speed and Pan, one entry per
        slot — the families whose values act per block. */
    const SuiteModulation::TargetInfo modulationTargets[] {
        { CyclicalProcessor::slotLevelIds[0],     "Level 1" },
        { CyclicalProcessor::slotLevelIds[1],     "Level 2" },
        { CyclicalProcessor::slotLevelIds[2],     "Level 3" },
        { CyclicalProcessor::slotLevelIds[3],     "Level 4" },
        { CyclicalProcessor::slotCutoffIds[0],    "Cutoff 1" },
        { CyclicalProcessor::slotCutoffIds[1],    "Cutoff 2" },
        { CyclicalProcessor::slotCutoffIds[2],    "Cutoff 3" },
        { CyclicalProcessor::slotCutoffIds[3],    "Cutoff 4" },
        { CyclicalProcessor::slotSpeedIds[0],     "Speed 1" },
        { CyclicalProcessor::slotSpeedIds[1],     "Speed 2" },
        { CyclicalProcessor::slotSpeedIds[2],     "Speed 3" },
        { CyclicalProcessor::slotSpeedIds[3],     "Speed 4" },
        { CyclicalProcessor::slotPanIds[0],       "Pan 1" },
        { CyclicalProcessor::slotPanIds[1],       "Pan 2" },
        { CyclicalProcessor::slotPanIds[2],       "Pan 3" },
        { CyclicalProcessor::slotPanIds[3],       "Pan 4" },
    };
}

//==============================================================================
CyclicalProcessor::CyclicalProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout()),
      modulation (apvts, modulationTargets, (int) std::size (modulationTargets), nullptr)
{
    masterVolumeParam = apvts.getRawParameterValue (masterVolumeId);

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
        levelParams[(size_t) slot]        = apvts.getRawParameterValue (slotLevelIds[(size_t) slot]);
        beatsParams[(size_t) slot]        = apvts.getRawParameterValue (slotBeatsIds[(size_t) slot]);
        chopParams[(size_t) slot]         = apvts.getRawParameterValue (slotChopIds[(size_t) slot]);
        slicesParams[(size_t) slot]       = apvts.getRawParameterValue (slotSlicesIds[(size_t) slot]);
        sliceLengthParams[(size_t) slot]  = apvts.getRawParameterValue (slotSliceLengthIds[(size_t) slot]);

        // Plan 17 Phase 2 (creative) and plan 18 Phase 4 (the former globals, per slot).
        filterParams[(size_t) slot]     = apvts.getRawParameterValue (slotFilterIds[(size_t) slot]);
        attackParams[(size_t) slot]     = apvts.getRawParameterValue (slotAttackIds[(size_t) slot]);
        releaseParams[(size_t) slot]    = apvts.getRawParameterValue (slotReleaseIds[(size_t) slot]);
        quantizeParams[(size_t) slot]   = apvts.getRawParameterValue (slotQuantizeIds[(size_t) slot]);
        swingParams[(size_t) slot]      = apvts.getRawParameterValue (slotSwingIds[(size_t) slot]);
        triggerParams[(size_t) slot]    = apvts.getRawParameterValue (slotTriggerIds[(size_t) slot]);
        pitchParams[(size_t) slot]      = apvts.getRawParameterValue (slotPitchIds[(size_t) slot]);
        crossfadeParams[(size_t) slot]  = apvts.getRawParameterValue (slotCrossfadeIds[(size_t) slot]);
        tempoParams[(size_t) slot]      = apvts.getRawParameterValue (slotTempoIds[(size_t) slot]);
        syncParams[(size_t) slot]       = apvts.getRawParameterValue (slotSyncIds[(size_t) slot]);
        panParams[(size_t) slot]        = apvts.getRawParameterValue (slotPanIds[(size_t) slot]);
        cutoffParams[(size_t) slot]     = apvts.getRawParameterValue (slotCutoffIds[(size_t) slot]);
        resoParams[(size_t) slot]       = apvts.getRawParameterValue (slotResoIds[(size_t) slot]);

        panSmoothers[(size_t) slot].reset (44100.0, 0.02);
        panSmoothers[(size_t) slot].setCurrentAndTargetValue (0.0f);

        loadGenerators[(size_t) slot] = std::make_shared<juce::Atomic<int>> (0);
    }

    // The auto-chop boundaries: a manual Slice Length edit drops the analyser's shares and
    // falls back to the equal/skew split. A Slices edit needs no listener â€” the count
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

    //---- Global (1). Plan 12: the final gain stage is named "Output" everywhere.
    // Plan 18 Phase 4: everything else from the old GLOBAL page is now per slot.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { masterVolumeId, 1 }, "Output",
        juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, "dB"));

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
        // session without these IDs (resetMissing) reads the same keys. The value box shows
        // the note name and number, the MTC/Sgraffito pattern (plan 20, owner ask).
        params.push_back (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { slotNoteIds[(size_t) slot], 1 }, "Note " + juce::String (slot + 1),
            24, 96, firstSlotNote + slot,
            juce::AudioParameterIntAttributes().withStringFromValueFunction (
                [] (int value, int) { return juce::MidiMessage::getMidiNoteName (value, true, true, 3)
                                          + " (" + juce::String (value) + ")"; })));

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

    //---- Per-slot creative controls (Plan 17 Phase 2)
    for (int slot = 0; slot < numSlots; ++slot)
    {
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { slotFilterIds[(size_t) slot], 1 }, "Filter " + juce::String (slot + 1),
            juce::StringArray { "Off", "High Pass", "Low Pass", "Band Pass" }, 0));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { slotAttackIds[(size_t) slot], 1 }, "Attack " + juce::String (slot + 1),
            juce::NormalisableRange<float> (1.0f, 1000.0f, 1.0f), 1.0f, "ms"));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { slotReleaseIds[(size_t) slot], 1 }, "Release " + juce::String (slot + 1),
            juce::NormalisableRange<float> (1.0f, 10000.0f, 1.0f), 5.0f, "ms"));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { slotLevelIds[(size_t) slot], 1 }, "Level " + juce::String (slot + 1),
            juce::NormalisableRange<float> (0.0f, 200.0f, 0.1f), 100.0f, "%"));

        // Plan 18 Â§1.1 (owner answer: default): the filter's cutoff and resonance.
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { slotCutoffIds[(size_t) slot], 1 }, "Cutoff " + juce::String (slot + 1),
            juce::NormalisableRange<float> (20.0f, 20000.0f, 0.0f, 0.25f), 20000.0f, "Hz"));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { slotResoIds[(size_t) slot], 1 }, "Reso " + juce::String (slot + 1),
            juce::NormalisableRange<float> (0.5f, 10.0f, 0.01f), 0.7f, ""));
    }

    //---- The former GLOBAL controls, per slot (plan 18 Phase 4, appended so no saved
    // index moves; the old globals are removed, which is safe before a release).
    for (int slot = 0; slot < numSlots; ++slot)
    {
        const auto n = juce::String (slot + 1);

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { slotQuantizeIds[(size_t) slot], 1 }, "Quantize " + n,
            juce::StringArray { "Off", "1/16", "1/8", "1/4", "1/2", "1 Bar" }, 0));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { slotSwingIds[(size_t) slot], 1 }, "Swing " + n,
            juce::NormalisableRange<float> (-20.0f, 20.0f, 0.1f), 0.0f, "%"));

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { slotTriggerIds[(size_t) slot], 1 }, "Trigger " + n,
            juce::StringArray { "Latch", "Gate", "Toggle" }, 0));

        params.push_back (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { slotPitchIds[(size_t) slot], 1 }, "Pitch " + n, -12, 12, 0));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { slotCrossfadeIds[(size_t) slot], 1 }, "Crossfade " + n,
            juce::NormalisableRange<float> (0.0f, 500.0f, 1.0f), 5.0f, "ms"));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { slotTempoIds[(size_t) slot], 1 }, "Tempo " + n,
            juce::NormalisableRange<float> (40.0f, 240.0f, 0.1f), 120.0f, "BPM"));

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { slotSyncIds[(size_t) slot], 1 }, "Host Sync " + n,
            juce::StringArray { "Off", "On" }, 0));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { slotPanIds[(size_t) slot], 1 }, "Pan " + n,
            juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, "%"));
    }

    //---- Plan 18 Phase 6: the shared LFO and routing parameters (34 + the 12 transforms),
    // appended last so no saved index moves.
    SuiteModulation::Router::addParameters (params, modTargetNames());

    return { params.begin(), params.end() };
}

juce::StringArray CyclicalProcessor::modTargetNames()
{
    // Must follow the ModTarget enum order exactly.
    return { "None",
             "Level 1", "Level 2", "Level 3", "Level 4",
             "Cutoff 1", "Cutoff 2", "Cutoff 3", "Cutoff 4",
             "Speed 1", "Speed 2", "Speed 3", "Speed 4",
             "Pan 1", "Pan 2", "Pan 3", "Pan 4" };
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
    masterSmoother.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (masterVolumeParam->load()));

    // The per-slot stage (plan 18 Phase 4): pan smoothing, the filter nodes and the
    // scratch buffers.
    const auto size = (size_t) juce::jmax (1, samplesPerBlock);
    for (int slot = 0; slot < numSlots; ++slot)
    {
        panSmoothers[(size_t) slot].reset (preparedSampleRate, 0.02);
        panSmoothers[(size_t) slot].setCurrentAndTargetValue (
            juce::jlimit (-100.0f, 100.0f, panParams[(size_t) slot]->load()) / 100.0f);

        levelSmoothers[(size_t) slot].reset (preparedSampleRate, 0.02);
        levelSmoothers[(size_t) slot].setCurrentAndTargetValue (
            juce::jlimit (0.0f, 2.0f, levelParams[(size_t) slot]->load()) / 100.0f);

        slotFilters[(size_t) slot].prepare ({ preparedSampleRate, 2, (juce::uint32) size });
        slotFilters[(size_t) slot].reset();
        slotFilters[(size_t) slot].setResonance (0.7f);

        slotScratch[(size_t) slot].setSize (2, size, false, true, true);
        slotScratch[(size_t) slot].clear();
    }

    for (int slot = 0; slot < numSlots; ++slot)
    {
        slotClockBeats[(size_t) slot] = 0.0;
        slotWasRunning[(size_t) slot] = false;
        lastBlockTriggerMode[(size_t) slot] = -1;
    }
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

    // The host-load test hands an instrument a mono buffer while reporting two outputs;
    // the slot mix folds into the available width (mono: the pan gains summed).
    const int outChannels = buffer.getNumChannels();

    for (auto& state : slotStates)
        for (auto& voice : state.voices)
            voice.prepare (preparedSampleRate);

    //---- Plan 18 Phase 6: the modulation offsets. The delay clock runs from the first
    // block (computeOffsets' zero arguments make the LFO phase read from the router's own
    // accumulated cycles; the shared tempo fallback covers the synced LFOs).
    const auto offsets = modulation.computeOffsets (0.0, 0.0, 0.0);
    modulation.publish (offsets, 0.0, 0.0);
    blockOffsets = offsets;

    //---- The per-slot beat clocks (plan 18 Phase 4). Each slot runs its own clock: the
    // host's position and tempo while THAT slot's Host Sync is on, its own Tempo parameter
    // otherwise. The clocks only accumulate, so loops never jump on relocate.
    const auto* playHead = getPlayHead();
    const auto position = playHead != nullptr ? playHead->getPosition()
                                              : juce::Optional<juce::AudioPlayHead::PositionInfo>();
    {
        double hostBpm = 120.0;
        if (position.hasValue())
            if (const auto reported = position->getBpm(); reported.hasValue() && *reported > 1.0)
                hostBpm = juce::jlimit (20.0, 300.0, (double) *reported);
        blockBpm = hostBpm;
    }

    const bool hostPlaying = position.hasValue() && position->getIsPlaying()
                          && position->getPpqPosition().hasValue();
    const double hostPpq = hostPlaying ? *position->getPpqPosition() : 0.0;

    static constexpr double grids[6] = { 0.0, 0.25, 0.5, 1.0, 2.0, 4.0 };

    std::array<double, numSlots> slotSamplesPerBeat { 60.0 / 120.0 * preparedSampleRate,
                                                     60.0 / 120.0 * preparedSampleRate,
                                                     60.0 / 120.0 * preparedSampleRate,
                                                     60.0 / 120.0 * preparedSampleRate };

    for (int slot = 0; slot < numSlots; ++slot)
    {
        const auto i = (size_t) slot;

        // The slot's effective tempo: the host's while its sync is on (with a reported
        // tempo), its own Tempo parameter otherwise.
        const bool synced = syncParams[i]->load() > 0.5f;
        double bpm = juce::jlimit (40.0, 240.0, (double) tempoParams[i]->load());
        if (synced && position.hasValue())
            if (const auto reported = position->getBpm(); reported.hasValue() && *reported > 1.0)
                bpm = blockBpm;
        slotBpmNow[i] = bpm;
        slotSamplesPerBeat[i] = 60.0 / bpm * preparedSampleRate;

        const bool hostDrives = synced && hostPlaying;

        if (hostDrives)
        {
            if (! slotWasRunning[i] || std::abs (hostPpq - slotClockBeats[i]) > 1.0e-3)
            {
                slotClockBeats[i] = hostPpq;
                pendingTriggers[i] = {};      // a relocate invalidates the pending grid time
                toggledOn[i] = false;         // plan 14: a relocate clears the toggle state
            }
            slotWasRunning[i] = true;
        }
        else
        {
            if (slotWasRunning[i])
            {
                pendingTriggers[i] = {};      // a transport stop flushes the pending list

                // Plan 14: in Toggle mode a stopped transport stops the toggled slot and
                // clears the toggle state. Latch keeps its one-shot tails, as today.
                if (juce::jlimit (0, 2, (int) (triggerParams[i]->load() + 0.5f)) == 2)
                    slotStates[i].voices[(size_t) 0].release();
                toggledOn[i] = false;
            }
            slotWasRunning[i] = false;
        }

        // Plan 17 Phase 3, per slot: a trigger-mode change stops the slot's voices and
        // clears its toggle state. A preset/state load raises the shared command.
        const int triggerMode = juce::jlimit (0, 2, (int) (triggerParams[i]->load() + 0.5f));
        if (triggerMode != lastBlockTriggerMode[i] || stopAllVoicesPending.exchange (false))
        {
            for (auto& voice : slotStates[i].voices)
                voice.reset();
            toggledOn[i] = false;
        }
        lastBlockTriggerMode[i] = triggerMode;
    }
    stopAllVoicesPending.store (false, std::memory_order_release);

    // Plan 17 Phase 3: a transformed loop restarts the slot's playing voice with the
    // articulation it started with (Latch/Toggle only).
    for (int slot = 0; slot < numSlots; ++slot)
        if (retransformPending[(size_t) slot].exchange (false, std::memory_order_acq_rel))
            startSlotVoice (slot,
                            lastTriggerChannel[(size_t) slot].load (std::memory_order_relaxed),
                            lastTriggerVelocity[(size_t) slot].load (std::memory_order_relaxed),
                            0,
                            lastTriggerSlice[(size_t) slot].load (std::memory_order_relaxed));

    //---- MIDI (plan 14): a slot claims the key its Note parameter names (chop mode widens
    // the map, Phase 3). Each note is handled with ITS slot's trigger mode, grid, swing
    // and clock (plan 18 Phase 4: the controls work independently per slot).
    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        const int sample = (int) metadata.samplePosition;

        const bool noteOn = message.isNoteOn() && message.getVelocity() > 0;
        const bool noteOff = message.isNoteOff() || (message.isNoteOn() && message.getVelocity() == 0);

        if (! noteOn && ! noteOff)
            continue;

        const int key = message.getNoteNumber();
        const int slot = slotForNote (key);
        if (slot < 0)
            continue;

        const auto i = (size_t) slot;
        const double blockStartBeat = slotClockBeats[i];
        const double blockEndBeat = blockStartBeat + numSamples / slotSamplesPerBeat[i];
        const double grid = grids[juce::jlimit (0, 5, (int) (quantizeParams[i]->load() + 0.5f))];
        const float swingNorm = juce::jlimit (-0.2f, 0.2f, swingParams[i]->load() / 100.0f);
        const int triggerMode = juce::jlimit (0, 2, (int) (triggerParams[i]->load() + 0.5f));
        const int slice = sliceForNote (slot, key);

        if (noteOn)
        {
            if (triggerMode == 2)
            {
                // Toggle (plan 14): the first press starts the slot looping (scheduled on
                // the next grid point when Quantize is on); the second press stops it
                // immediately, never delayed to the grid. A note-off does nothing.
                auto& pending = pendingTriggers[i];
                if (toggledOn[i] || pending.active)
                {
                    pending = {};
                    toggledOn[i] = false;
                    releaseSlotVoice (slot);
                }
                else if (grid <= 0.0)
                {
                    startSlotVoice (slot, message.getChannel(), message.getFloatVelocity(), sample, slice);
                    toggledOn[i] = true;
                }
                else
                {
                    pending.active = true;
                    pending.channel = message.getChannel();
                    pending.velocity = message.getFloatVelocity();
                    pending.beatTime = nextGridBeat (blockStartBeat, sample, slotSamplesPerBeat[i], grid, swingNorm);
                    pending.slice = slice;
                    toggledOn[i] = true;
                }
                continue;
            }

            if (grid <= 0.0)
            {
                startSlotVoice (slot, message.getChannel(), message.getFloatVelocity(), sample, slice);
                pendingTriggers[i] = {};
            }
            else
            {
                // Schedule on the slot's next grid point; a second trigger replaces the
                // pending one, and odd grid points swing by the suite's swingBeats rule.
                auto& pending = pendingTriggers[i];
                pending.active = true;
                pending.channel = message.getChannel();
                pending.velocity = message.getFloatVelocity();
                pending.beatTime = nextGridBeat (blockStartBeat, sample, slotSamplesPerBeat[i], grid, swingNorm);
                pending.slice = slice;
            }
        }
        else
        {
            if (triggerMode == 2)
                continue;   // a note-off never stops a toggled slot (plan 14)

            // A note-off before the scheduled start cancels the pending trigger; in Gate
            // mode a note-off after the start releases the voice.
            pendingTriggers[i] = {};

            if (triggerMode == 1)
                releaseSlotVoice (slot);
        }
    }

    //---- Fire the pending triggers that fall inside this block (each on its own clock).
    for (int slot = 0; slot < numSlots; ++slot)
    {
        const auto i = (size_t) slot;
        auto& pending = pendingTriggers[i];
        if (! pending.active)
            continue;

        const double blockEndBeat = slotClockBeats[i] + numSamples / slotSamplesPerBeat[i];
        if (pending.beatTime >= blockEndBeat)
            continue;

        const int at = juce::jlimit (0, juce::jmax (0, numSamples - 1),
            (int) std::lround ((pending.beatTime - slotClockBeats[i]) * slotSamplesPerBeat[i]));
        startSlotVoice (slot, pending.channel, pending.velocity, at, pending.slice);
        pending = {};
    }

    //---- Render: each slot into its own scratch, then the per-slot stage (filter, level,
    // pan, plan 18 Phase 4) before the master sum.
    for (int slot = 0; slot < numSlots; ++slot)
    {
        const auto i = (size_t) slot;
        auto& scratch = slotScratch[i];
        if ((size_t) numSamples > (size_t) scratch.getNumSamples())
            scratch.setSize (2, numSamples, false, true, true);
        scratch.clear();

        auto& state = slotStates[i];
        for (int v = 0; v < 2; ++v)
        {
            const int from = juce::jlimit (0, juce::jmax (0, numSamples - 1), state.renderFrom[(size_t) v]);
            state.voices[(size_t) v].render (scratch, from, numSamples - from);
            state.renderFrom[(size_t) v] = 0;
        }

        // The per-slot filter (plan 17 Phase 2, wired in plan 18): Off/HP/LP/BP with the
        // slot's cutoff and resonance — the cutoff moves with its routing offset (plan 18
        // Phase 6). Off skips the node.
        const int filterIndex = juce::jlimit (0, 3, (int) (filterParams[i]->load() + 0.5f));
        if (filterIndex != 0)
        {
            using Type = juce::dsp::StateVariableTPTFilterType;
            slotFilters[i].setType (filterIndex == 1 ? Type::highpass
                                    : filterIndex == 2 ? Type::lowpass
                                                       : Type::bandpass);

            const float highestCorner = juce::jmin (20000.0f, 0.45f * (float) preparedSampleRate);
            const float cutoff = juce::jlimit (20.0f, highestCorner,
                modulation.modulate ((int) ModTarget::Cutoff1 + slot, cutoffParams[i]->load(), offsets));
            slotFilters[i].setCutoffFrequency (cutoff);
            slotFilters[i].setResonance (juce::jlimit (0.5f, 10.0f, resoParams[i]->load()));

            for (int ch = 0; ch < juce::jmin (2, outChannels); ++ch)
                for (int n = 0; n < numSamples; ++n)
                    scratch.setSample (ch, n, slotFilters[i].processSample (ch, scratch.getSample (ch, n)));
        }

        // The per-slot level (plan 17 Phase 2), smoothed 20 ms — the level moves with its
        // routing offset (plan 18 Phase 6). The jlimit is the parameter's own 0..200 %
        // range, before the /100.
        levelSmoothers[i].setTargetValue (juce::jlimit (0.0f, 200.0f,
            modulation.modulate ((int) ModTarget::Level1 + slot, levelParams[i]->load(), offsets)) / 100.0f);

        // The per-slot pan (plan 18 Phase 4): a constant-power law, smoothed 20 ms, moved
        // by its routing offset.
        panSmoothers[i].setTargetValue (juce::jlimit (-100.0f, 100.0f,
            modulation.modulate ((int) ModTarget::Pan1 + slot, panParams[i]->load(), offsets)) / 100.0f);

        const float panNorm = panSmoothers[i].getNextValue();
        const double angle = (double) (panNorm + 1.0f) * juce::MathConstants<double>::pi * 0.25;
        const float panGainL = (float) std::cos (angle);
        const float panGainR = (float) std::sin (angle);

        // Fold the panned scratch into the master buffer (mono: the two gains summed), and
        // track the slot's post-level peak for the mix row's meter (plan 20, owner ask).
        float slotPeak = 0.0f;
        if (outChannels >= 2)
        {
            for (int n = 0; n < numSamples; ++n)
            {
                const float levelGain = levelSmoothers[i].getNextValue();
                const float l = scratch.getSample (0, n) * levelGain;
                const float r = scratch.getSample (1, n) * levelGain;
                buffer.addSample (0, n, l * panGainL);
                buffer.addSample (1, n, r * panGainR);
                slotPeak = juce::jmax (slotPeak, std::abs (l), std::abs (r));
            }
        }
        else if (outChannels == 1)
        {
            for (int n = 0; n < numSamples; ++n)
            {
                const float levelGain = levelSmoothers[i].getNextValue();
                const float mixed = (scratch.getSample (0, n) * panGainL
                                     + scratch.getSample (1, n) * panGainR) * levelGain;
                buffer.addSample (0, n, mixed);
                slotPeak = juce::jmax (slotPeak, std::abs (mixed));
            }
        }

        slotLevelPeaks[(size_t) slot].store (slotPeak, std::memory_order_relaxed);
    }

    //---- Master stage (plan 18): the one global Output, smoothed 20 ms.
    masterSmoother.setTargetValue (juce::Decibels::decibelsToGain (
        juce::jlimit (-24.0f, 12.0f, masterVolumeParam->load())));

    for (int ch = 0; ch < outChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int n = 0; n < numSamples; ++n)
            data[n] *= masterSmoother.getNextValue();
    }

    //---- UI snapshots and the clock advance (the clocks only accumulate).
    for (int slot = 0; slot < numSlots; ++slot)
    {
        const auto i = (size_t) slot;
        activeFlags[i].store (slotStates[i].voices[(size_t) 0].isActive(), std::memory_order_relaxed);
        playheads[i].store (slotStates[i].voices[(size_t) 0].isActive()
            ? (float) slotStates[i].voices[(size_t) 0].playheadNormalised() : 0.0f, std::memory_order_relaxed);

        if (! (syncParams[i]->load() > 0.5f && hostPlaying))
            slotClockBeats[i] += (double) numSamples / slotSamplesPerBeat[i];
    }
    beatPositionUiValue.store (slotClockBeats[0], std::memory_order_relaxed);

    // Plan 18 Phase 6: the free LFOs advance at the end of the block (the suite rule).
    modulation.advanceBlock (numSamples);
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

    // Plan 14 (G-B): with Host Sync on the whole loop plays in exactly Beats beats â€” the
    // rate comes from the loop's own duration, so the Speed knob is inactive (the editor
    // dims it). Start and Length choose the played span; they do not change the rate.
    // The voice computes rate = speed x sourceRate / hostRate, so speed here is
    // loopFrames x bpm / (beats x 60 x sourceRate).
    // A chopped slot ignores sync (owner decision): its Speed knob stays live and its
    // Beats value is not used.
    const bool chopped = slotChopped (slot);

    if (syncParams[(size_t) slot]->load() > 0.5f && ! chopped)
    {
        region.speed = (double) view.numFrames * slotBpmNow[(size_t) slot]
                     / ((double) slotBeats (slot) * 60.0 * std::max (1.0, view.sourceSampleRate));
    }
    else
    {
        // The speed moves with its routing offset (plan 18 Phase 6).
        region.speed = juce::jlimit (0.25f, 4.0f,
            modulation.modulate ((int) ModTarget::Speed1 + slot, speedParams[(size_t) slot]->load(), blockOffsets));
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
        juce::jlimit (0.0f, 500.0f, crossfadeParams[(size_t) slot]->load()) * 0.001 * preparedSampleRate);

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
        juce::jlimit (-12, 12, (int) (pitchParams[(size_t) slot]->load() + 0.5f)) / 12.0);
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

    // The slot's own attack and release shape the loop's gain envelope (plan 17 Phase 5,
    // wired in plan 18): the attack replaces the engine's 5 ms default, the release its
    // 5 ms fade on stop. The knob times apply from this trigger.
    state.voices[0].setEnvelopeTimes (juce::jlimit (1.0f, 1000.0f, attackParams[(size_t) slot]->load()),
                                      juce::jlimit (1.0f, 10000.0f, releaseParams[(size_t) slot]->load()));

    state.renderFrom[0] = sampleInBlock;
    lastTriggerChannel[(size_t) slot].store (channel, std::memory_order_relaxed);
    lastTriggerVelocity[(size_t) slot].store (velocity, std::memory_order_relaxed);
    lastTriggerSlice[(size_t) slot].store (slice, std::memory_order_relaxed);
}

void CyclicalProcessor::releaseSlotVoice (int slot)
{
    slotStates[(size_t) slot].voices[(size_t) 0].release();
}

void CyclicalProcessor::requestRegionRetrigger (int slot)
{
    // Plan 18 §1.4 (owner answer 2026-11-23): Start/Length/Slices/Slice Length/Beats
    // apply live — the knob's release retriggers the playing voice, which reads the new
    // region (and the new sync rate) at its restart. Gate slots wait for the next press.
    if (slotStates[(size_t) slot].voices[0].isActive()
        && juce::jlimit (0, 2, (int) (triggerParams[(size_t) slot]->load() + 0.5f)) != 1)
        retransformPending[(size_t) slot].store (true, std::memory_order_release);
}

int CyclicalProcessor::slotNote (int slot) const
{
    return juce::jlimit (24, 96, (int) (noteParams[(size_t) slot]->load() + 0.5f));
}

int CyclicalProcessor::slotForNote (int key) const
{
    // Two passes (plan 17): an exact Note match wins BEFORE any chop range, so a chopped
    // slot 1 does not shadow the slots whose base notes sit inside its claimed span. The
    // default layout (60/61/62/63 with slot 1 chopped) stays fully playable; only keys with
    // no exact match fall to the chop ranges.
    for (int slot = 0; slot < numSlots; ++slot)
        if (key == slotNote (slot))
            return slot;

    for (int slot = 0; slot < numSlots; ++slot)
    {
        if (! slotChopped (slot))
            continue;

        // A chopped slot claims base .. base + slices - 1 (slice k on base + k).
        const int base = slotNote (slot);
        if (key > base && key < base + slotSlices (slot))
            return slot;
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
// Loop transformation tools (plan 16 Phase 3). Every tool builds a fresh buffer and
// installs it through the retirement queue (a playing voice keeps its old buffer); the
// loop reference is unchanged, so a session reload re-reads the original file.

void CyclicalProcessor::installTransformedLoop (int slot, const juce::AudioBuffer<float>& transformed)
{
    const LoadedLoop* current = slotStates[(size_t) slot].loop.current();

    if (current == nullptr)
        return;

    auto loop = makeLoadedLoop (juce::AudioBuffer<float> (transformed),
                                current->sourceSampleRate, current->sourcePath);

    slotStates[(size_t) slot].loop.install (std::move (loop));
    pendingTriggers[(size_t) slot] = {};

    // Plan 17 Phase 3: a transform is heard immediately â€” the audio thread restarts the
    // playing voice on the transformed loop with the articulation it started with
    // (otherwise the old source plays until the next trigger, which the owner read as
    // "no effect"). Gate slots wait for the next press (a restart with no held key would
    // loop forever under Gate).
    if (slotStates[(size_t) slot].voices[0].isActive()
        && juce::jlimit (0, 2, (int) (triggerParams[(size_t) slot]->load() + 0.5f)) != 1)
        retransformPending[(size_t) slot].store (true, std::memory_order_release);
}

void CyclicalProcessor::loopToolReverse (int slot)
{
    const LoadedLoop* current = slotStates[(size_t) slot].loop.current();

    if (current == nullptr || current->buffer.getNumSamples() <= 1)
        return;

    juce::AudioBuffer<float> transformed (current->buffer);
    const int numFrames = transformed.getNumSamples();

    // Whole loop unless the slot is chopped with a selected source slice.
    int spanStart = 0;
    int spanLength = numFrames;

    if (slotChopped (slot) && selectedSlices[(size_t) slot] >= 0)
    {
        const auto frames = CoreDSP::Loop::regionFrames (numFrames,
            startParams[(size_t) slot]->load(), lengthParams[(size_t) slot]->load());
        const auto bounds = activeChopBounds (slot);
        const int slices = slotSlices (slot);
        const int k = juce::jlimit (0, slices - 1, selectedSlices[(size_t) slot]);

        const double startShare = k == 0 ? 0.0 : (double) bounds[(size_t) (k - 1)];
        const double endShare = k == slices ? 1.0 : (double) bounds[(size_t) std::min (k, slices - 1)];

        spanStart = frames[0] + (int) std::lround (startShare * (double) frames[1]);
        spanLength = juce::jlimit (2, frames[1],
            (int) std::lround ((endShare - startShare) * (double) frames[1]));
    }

    for (int ch = 0; ch < transformed.getNumChannels(); ++ch)
    {
        auto* data = transformed.getWritePointer (ch);
        const int half = spanLength / 2;

        for (int i = 0; i < half; ++i)
            std::swap (data[spanStart + i], data[spanStart + spanLength - 1 - i]);
    }

    installTransformedLoop (slot, transformed);
}

void CyclicalProcessor::loopToolNormalise (int slot)
{
    const LoadedLoop* current = slotStates[(size_t) slot].loop.current();

    if (current == nullptr)
        return;

    const auto& source = current->buffer;
    const int numFrames = source.getNumSamples();
    float peak = 0.0f;

    for (int ch = 0; ch < source.getNumChannels(); ++ch)
        for (int n = 0; n < numFrames; ++n)
            peak = juce::jmax (peak, std::abs (source.getSample (ch, n)));

    if (peak < 1.0e-6f)
        return;   // silence stays silence

    juce::AudioBuffer<float> transformed (source);
    transformed.applyGain (juce::Decibels::decibelsToGain (-0.1f) / peak);   // peak at -0.1 dBFS

    installTransformedLoop (slot, transformed);
}

void CyclicalProcessor::loopToolTrim (int slot)
{
    const LoadedLoop* current = slotStates[(size_t) slot].loop.current();

    if (current == nullptr)
        return;

    const auto& source = current->buffer;
    const int numFrames = source.getNumSamples();
    constexpr float threshold = 1.0e-4f;

    int first = numFrames;
    int last = -1;

    for (int n = 0; n < numFrames && first == numFrames; ++n)
        for (int ch = 0; ch < source.getNumChannels(); ++ch)
            if (std::abs (source.getSample (ch, n)) > threshold) { first = n; break; }

    for (int n = numFrames - 1; n >= 0 && last < 0; --n)
        for (int ch = 0; ch < source.getNumChannels(); ++ch)
            if (std::abs (source.getSample (ch, n)) > threshold) { last = n; break; }

    if (last < 0)
        return;   // silence: nothing to align to

    auto* startParameter = apvts.getParameter (slotStartIds[(size_t) slot]);
    auto* lengthParameter = apvts.getParameter (slotLengthIds[(size_t) slot]);

    const double startPercent = (double) first / (double) numFrames * 100.0;
    const double lengthPercent = juce::jlimit (5.0, 100.0,
        (double) (last + 1 - first) / (double) numFrames * 100.0);

    startParameter->setValueNotifyingHost (startParameter->convertTo0to1 ((float) startPercent));
    lengthParameter->setValueNotifyingHost (lengthParameter->convertTo0to1 ((float) lengthPercent));
}

void CyclicalProcessor::selectChopSlice (int slot, int sourceSlice)
{
    selectedSlices[(size_t) slot] = selectedSlices[(size_t) slot] == sourceSlice
        ? -1 : juce::jlimit (0, maxSlices, sourceSlice);
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
    selectedSlices[(size_t) slot] = -1;   // and the tool selection with it
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

    // Plan 17 Phase 3: the parameters just changed, so a running loop would outlive its
    // own settings â€” the next block silences every voice and clears the toggle state.
    stopAllVoicesPending.store (true, std::memory_order_release);

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

        // Plan 17 Phase 3: the parameters just changed â€” a running loop would outlive its
        // own settings (the owner's "never stops, even if I change preset"). The next
        // block silences every voice and clears the toggle state.
        stopAllVoicesPending.store (true, std::memory_order_release);

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
