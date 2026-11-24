#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include "LoopSlot.h"
#include "SuiteModulationRouter.h"
#include "SuitePresets.h"
#include "SuiteState.h"

/**
    Cyclical (plan 6.1): a four-slot loop instrument.

    Four loaded audio loops play with creative playback modes: per-slot region, speed,
    direction (forward, reverse, ping-pong with a randomised turn-around), a granular mode
    and velocity scaling, locked to the host grid with a quantised start and swing. There is
    no recording and no audio input: the plugin is output-only, triggered from four fixed
    MIDI keys (60-63 = C4..D#4, one per slot).

    Loop loading follows the Sgraffito sample rules (plan 07): the loaded loops travel as
    `loop1Path` ... `loop4Path` attributes on <PARAMETERS> (never parameters), disk loads run
    on a background pool with the latest request winning, and a path that cannot be found is
    searched for in the user folder before the slot keeps playing with a missing overlay.
*/
class CyclicalProcessor : public juce::AudioProcessor,
                          public SuitePresets::StateHolder
{
public:
    static constexpr int numSlots = 4;
    static constexpr int numControls = 151;   // 105 + 46 plan-18 Phase 6 (the router's 34 + 12 transforms)

    /** The 16 per-slot modulation targets (plan 18 Phase 6, append-only): Level, Cutoff,
        Speed and Pan, one entry per slot — the families whose values act per block. */
    enum class ModTarget
    {
        None = 0,
        Level1, Level2, Level3, Level4,
        Cutoff1, Cutoff2, Cutoff3, Cutoff4,
        Speed1, Speed2, Speed3, Speed4,
        Pan1, Pan2, Pan3, Pan4,
        count
    };

    static juce::StringArray modTargetNames();

    // The shared router's parameter IDs (the SD/MTC alias pattern): the LFOs, the six
    // routing slots and the plan-17 transforms. Tests and presets reference them by name.
    static constexpr const char* lfo1RateId = "lfo1Rate";
    static constexpr const char* lfo1ShapeId = "lfo1Shape";
    static constexpr const char* lfo1SyncId = "lfo1Sync";
    static constexpr const char* lfo1DivisionId = "lfo1Division";
    static constexpr const char* lfo1OffsetId = "lfo1Offset";
    static constexpr const char* lfo1InvertId = "lfo1Invert";
    static constexpr const char* lfo1RectifyId = "lfo1Rectify";
    static constexpr const char* lfo1QuantiseId = "lfo1Quantise";
    static constexpr const char* lfo1SmoothId = "lfo1Smooth";
    static constexpr const char* lfo1PulseWidthId = "lfo1PulseWidth";
    static constexpr const char* lfo1SkewId = "lfo1Skew";
    static constexpr const char* lfo2RateId = "lfo2Rate";
    static constexpr const char* lfo2ShapeId = "lfo2Shape";
    static constexpr const char* lfo2SyncId = "lfo2Sync";
    static constexpr const char* lfo2DivisionId = "lfo2Division";
    static constexpr const char* lfo2OffsetId = "lfo2Offset";
    static constexpr const char* lfo2InvertId = "lfo2Invert";
    static constexpr const char* lfo2RectifyId = "lfo2Rectify";
    static constexpr const char* lfo2QuantiseId = "lfo2Quantise";
    static constexpr const char* lfo2SmoothId = "lfo2Smooth";
    static constexpr const char* lfo2PulseWidthId = "lfo2PulseWidth";
    static constexpr const char* lfo2SkewId = "lfo2Skew";
    static constexpr const char* modSourceIds[6] = { "mod1Source", "mod2Source", "mod3Source", "mod4Source", "mod5Source", "mod6Source" };
    static constexpr const char* modTargetIds[6] = { "mod1Target", "mod2Target", "mod3Target", "mod4Target", "mod5Target", "mod6Target" };
    static constexpr const char* modAmountIds[6] = { "mod1Amount", "mod2Amount", "mod3Amount", "mod4Amount", "mod5Amount", "mod6Amount" };
    static constexpr const char* modDelayIds[6] = { "mod1Delay", "mod2Delay", "mod3Delay", "mod4Delay", "mod5Delay", "mod6Delay" };
    static constexpr int maxSlices = 16;
    static constexpr int firstSlotNote = 60;      // the default base: C4..D#4 = slots 1..4

    // Parameter IDs are saved in sessions. Never rename or reuse one after a release.
    static constexpr const char* masterVolumeId = "masterVolume";

    // Plan 18 Phase 4 (owner decision 2026-11-23): the former GLOBAL controls are per
    // slot and work independently — each slot runs its own grid, swing, trigger mode,
    // pitch, crossfade, tempo, host sync and pan. Master Output stays the one global.
    static constexpr const char* slotQuantizeIds[4] = { "slot1Quantize", "slot2Quantize", "slot3Quantize", "slot4Quantize" };
    static constexpr const char* slotSwingIds[4] = { "slot1Swing", "slot2Swing", "slot3Swing", "slot4Swing" };
    static constexpr const char* slotTriggerIds[4] = { "slot1Trigger", "slot2Trigger", "slot3Trigger", "slot4Trigger" };
    static constexpr const char* slotPitchIds[4] = { "slot1Pitch", "slot2Pitch", "slot3Pitch", "slot4Pitch" };
    static constexpr const char* slotCrossfadeIds[4] = { "slot1Crossfade", "slot2Crossfade", "slot3Crossfade", "slot4Crossfade" };
    static constexpr const char* slotTempoIds[4] = { "slot1Tempo", "slot2Tempo", "slot3Tempo", "slot4Tempo" };
    static constexpr const char* slotSyncIds[4] = { "slot1Sync", "slot2Sync", "slot3Sync", "slot4Sync" };
    static constexpr const char* slotPanIds[4] = { "slot1Pan", "slot2Pan", "slot3Pan", "slot4Pan" };

    // Plan 18 §1.1 (owner answer: default): the filter is the type selector (the Phase-9
    // slotNFilter choice) plus cutoff and resonance per slot.
    static constexpr const char* slotCutoffIds[4] = { "slot1Cutoff", "slot2Cutoff", "slot3Cutoff", "slot4Cutoff" };
    static constexpr const char* slotResoIds[4] = { "slot1Reso", "slot2Reso", "slot3Reso", "slot4Reso" };

    // The per-slot IDs are arrays, which is the form tools/audit_bank.py's coverage check
    // parses (getRawParameterValue (slotStartIds[i]) contributes every element).
    static constexpr const char* slotStartIds[4] = { "slot1Start", "slot2Start", "slot3Start", "slot4Start" };
    static constexpr const char* slotLengthIds[4] = { "slot1Length", "slot2Length", "slot3Length", "slot4Length" };
    static constexpr const char* slotSpeedIds[4] = { "slot1Speed", "slot2Speed", "slot3Speed", "slot4Speed" };
    static constexpr const char* slotDirectionIds[4] = { "slot1Direction", "slot2Direction", "slot3Direction", "slot4Direction" };
    static constexpr const char* slotPingRandomIds[4] = { "slot1PingRandom", "slot2PingRandom", "slot3PingRandom", "slot4PingRandom" };
    static constexpr const char* slotGranularIds[4] = { "slot1Granular", "slot2Granular", "slot3Granular", "slot4Granular" };
    static constexpr const char* slotVelocityIds[4] = { "slot1Velocity", "slot2Velocity", "slot3Velocity", "slot4Velocity" };

    // Plan 14: the per-slot trigger notes (appended after Velocity). Defaults are today's
    // fixed map, so a saved session that lacks the IDs reads the same keys via resetMissing.
    static constexpr const char* slotNoteIds[4] = { "slot1Note", "slot2Note", "slot3Note", "slot4Note" };

    // Plan 14: the musical length of the whole loaded loop, in beats. With Host Sync on
    // the loop plays in exactly this many beats (Speed is inactive then).
    static constexpr const char* slotBeatsIds[4] = { "slot1Beats", "slot2Beats", "slot3Beats", "slot4Beats" };

    // Plan 14 Phase 3: the chop mode. A chopped slot splits its region into Slices, and
    // key slotNNote + k triggers slice k. Sync is disabled for a chopped slot.
    static constexpr const char* slotChopIds[4] = { "slot1Chop", "slot2Chop", "slot3Chop", "slot4Chop" };
    static constexpr const char* slotSlicesIds[4] = { "slot1Slices", "slot2Slices", "slot3Slices", "slot4Slices" };
    static constexpr const char* slotSliceLengthIds[4] = { "slot1SliceLength", "slot2SliceLength", "slot3SliceLength", "slot4SliceLength" };

    // Plan 17 Phase 2: per-slot creative controls — filter, attack, release, level.
    static constexpr const char* slotFilterIds[4] = { "slot1Filter", "slot2Filter", "slot3Filter", "slot4Filter" };
    static constexpr const char* slotAttackIds[4] = { "slot1Attack", "slot2Attack", "slot3Attack", "slot4Attack" };
    static constexpr const char* slotReleaseIds[4] = { "slot1Release", "slot2Release", "slot3Release", "slot4Release" };
    static constexpr const char* slotLevelIds[4] = { "slot1Level", "slot2Level", "slot3Level", "slot4Level" };
    std::atomic<float>* levelParams[4] = { nullptr };

    // The loop references are state properties, not parameters (the lerpCurve/samplePath
    // route). Brace-initialised, so the audit's parameter-ID regex does not see them.
    static constexpr const char* loop1PathProperty { "loop1Path" };
    static constexpr const char* loop2PathProperty { "loop2Path" };
    static constexpr const char* loop3PathProperty { "loop3Path" };
    static constexpr const char* loop4PathProperty { "loop4Path" };
    static constexpr const char* embeddedUriPrefix { "embedded:" };
    // Plan 14 post-review: the auto-chop boundaries. One comma-separated share list per
    // slot (the lerpCurve pattern), written by the transient analyser, cleared when the
    // user moves Slices or Slice Length. Brace-initialised for the same reason.
    static constexpr const char* chop1BoundsProperty { "chop1Bounds" };
    static constexpr const char* chop2BoundsProperty { "chop2Bounds" };
    static constexpr const char* chop3BoundsProperty { "chop3Bounds" };
    static constexpr const char* chop4BoundsProperty { "chop4Bounds" };

    // Plan 16 Phase 2: the slice arrangement. One comma-separated permutation per slot:
    // key k plays the slice the order names. Empty = the slices play in order.
    static constexpr const char* chop1OrderProperty { "chop1Order" };
    static constexpr const char* chop2OrderProperty { "chop2Order" };
    static constexpr const char* chop3OrderProperty { "chop3Order" };
    static constexpr const char* chop4OrderProperty { "chop4Order" };

    /** The embedded defaults the INIT state installs into slots 1-4. */
    static constexpr const char* defaultLoopNames[] = { "kick", "hats", "bass", "chord" };

    juce::AudioProcessorValueTreeState apvts;

    // Bump stateVersion when the saved state changes, and add a migration from the old version.
    int stateVersion = 1;
    std::vector<SuiteState::Migration> migrations;

    CyclicalProcessor();
    ~CyclicalProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Cyclical"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    // A released or ping-pong tail is not cut by the host.
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override;
    void getCurrentProgramStateInformation (juce::MemoryBlock&) override;
    void setCurrentProgramStateInformation (const void*, int) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- Preset seam (R12) ------------------------------------------------------------
    juce::String getPresetPluginId() const override { return "Cyclical"; }
    int getPresetStateVersion() const override { return stateVersion; }
    juce::AudioProcessorValueTreeState& getPresetStateTree() override { return apvts; }
    void applyPresetState (const juce::ValueTree&) override;

    // The preset identity (plan 09 1.4), persisted as the presetSelection state property.
    SuitePresets::PresetSelection getPresetSelection() const override { return lastPresetSelection; }
    void setPresetSelection (const SuitePresets::PresetSelection&) override;
    static constexpr const char* presetSelectionProperty { "presetSelection" };

    // ---- Loop loading (plan 07 semantics) ----------------------------------------------
    /** The loop reference of one slot, as saved: a file path or an embedded: URI. */
    juce::String loopPath (int slot) const;
    static juce::String loopPathPropertyId (int slot);

    // ---- Auto-chop (plan 16 Phase 1) ----------------------------------------------------
    /** The interior slice-boundary shares of one slot, parsed from the state property.
        Empty when the slot uses the plan-14 equal/skew split. */
    std::vector<float> chopBounds (int slot) const;

    /** Message thread. The transient analyser (the AUTO button): detects onsets on the
        slot's loaded loop region, sets Slices to the count, and writes the boundary shares
        so every slice starts on a hit. */
    void autoChopSlot (int slot);

    /** The slot's slice arrangement, parsed from the state property. Empty = identity
        (the slices play in order). */
    std::vector<int> chopOrder (int slot) const;

    /** Message thread. Cycles the arrangement's entry at key index: the step-cell click
        (plan 16 Phase 2). Only live in chop mode. */
    void cycleSliceOrder (int slot, int index);

    // ---- Loop transformation tools (plan 16 Phase 3) ------------------------------------
    /** Message thread. Reverses the loaded loop; when the slot is chopped and a source
        slice is selected in the arrangement strip, only that slice's span reverses. The
        transform installs as a fresh loop through the retirement queue (a playing voice
        keeps its old buffer), and the loop reference (path/URI) is unchanged — a session
        reload re-reads the original file. */
    void loopToolReverse (int slot);

    /** Message thread. Scales the loaded loop so its peak sits at -0.1 dBFS, so slots
        match in loudness. Silent loops are left alone. */
    void loopToolNormalise (int slot);

    /** Message thread. Aligns Start/Length to the loop's non-silent content: the first and
        last frames above a small threshold define the region. The buffer is untouched. */
    void loopToolTrim (int slot);

    /** Message thread. Selects the source slice the arrangement strip's tools act on
        (right-click a strip cell; the same index again deselects). */
    void selectChopSlice (int slot, int sourceSlice);
    int selectedChopSlice (int slot) const noexcept { return selectedSlices[(size_t) slot]; }

    /** Message thread. Writes the arrangement property (the tests use it; a preset may
        carry it too). */
    void setChopOrderProperty (int slot, const std::vector<int>& order);

    /** Message thread. Resolves the four loop references of a <PARAMETERS> tree: an empty
        tree is INIT (all slots back to their embedded defaults), an embedded: URI loads the
        resource, an existing file loads asynchronously, a missing file searches the user
        folder first and otherwise keeps playing with the missing flag up. */
    void applyLoopsFromState (const juce::ValueTree& tree);

    /** Message thread. Loads a disk file into one slot in the background; the current loop
        keeps playing until the load installs. The saved path is kept in sync. */
    void loadLoopFromFileAsync (int slot, const juce::File& file, const juce::String& pathToSave);

    /** Message thread. Loads an embedded library loop synchronously; an unknown resource
        name silently keeps the current loop. */
    void loadEmbeddedLoop (int slot, const juce::String& name, const juce::String& pathToSave);

    /** True while a background load has not installed yet (tests pump the message loop). */
    bool hasPendingLoopLoad() const noexcept { return pendingLoopLoads.load() > 0; }

    /** True when the slot's saved loop could not be resolved anywhere. */
    bool isLoopMissing (int slot) const noexcept { return loopMissing[(size_t) slot].load(); }
    juce::String missingLoopPath (int slot) const { return missingPaths[(size_t) slot]; }

    /** The folder a missing file name is searched in. Defaults to the user-documents
        location; tests override the member. */
    juce::File sampleSearchFolder;
    static juce::File defaultUserLoopFolder();

    // ---- UI snapshots ------------------------------------------------------------------
    float slotPlayhead (int slot) const noexcept { return playheads[(size_t) slot].load (std::memory_order_relaxed); }
    bool slotIsActive (int slot) const noexcept { return activeFlags[(size_t) slot].load (std::memory_order_relaxed); }
    /** Plan 20: the slot's post-level peak (linear 0..1), for the mix row's meter. */
    float slotLevelPeak (int slot) const noexcept { return slotLevelPeaks[(size_t) slot].load (std::memory_order_relaxed); }
    double beatPositionUi() const noexcept { return beatPositionUiValue.load (std::memory_order_relaxed); }

    /** The note a slot triggers on. Chop mode extends the claimed range upward (Phase 3):
        key slotNote + k triggers slice k. */
    int slotNote (int slot) const;

    /** The shared LFO and routing module (plan 18 Phase 6). */
    SuiteModulation::Router& getRouter() noexcept { return modulation; }

    /** The value a control shows right now: its base value moved by the routing offset. */
    float getModulatedValue (ModTarget target) const
    {
        return modulation.getModulatedValue ((int) target);
    }

    /** The musical length of the slot's whole loaded loop, in beats (plan 14). */
    int slotBeats (int slot) const;

    /** The interior chop boundary shares a slot plays right now: the auto-chop property
        when its count matches Slices, the plan-14 power curve otherwise. The editor's
        waveform draws exactly these. */
    std::vector<float> activeChopBounds (int slot) const;

    /** True when the slot is chopped: its keys then cover slotNote .. slotNote + slices − 1. */
    bool slotChopped (int slot) const;
    int slotSlices (int slot) const;

    /** Plan 18 §1.4 (owner answer): the region knobs apply live — a knob release
        retriggers the playing voice with the new region (the plan-16 retransform
        pattern). Gate slots wait for the next press. Message thread. */
    void requestRegionRetrigger (int slot);

    /** The chop boundary of slice i, as a share (0..1) of the slot's region: b_i = (i/S)^e
        with e = 2^(2·v) and v = Slice Length / 100. v = 0 is the equal split; positive v
        lengthens the late slices, negative the early ones. b_0 = 0, b_S = 1, and the
        boundaries are strictly increasing. Shared with the waveform view, so the display
        and the audio cannot disagree. */
    static double chopBoundary (int i, int slices, float sliceLengthPercent);

    /** Message thread. The loaded loop of one slot, for the waveform views (may be null). */
    const LoadedLoop* loopForUi (int slot) const { return slotStates[(size_t) slot].loop.current(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // One slot: the loaded loop, two voices (active + fading tail) and the block-relative
    // sample each voice starts rendering at (0 for a voice that started in an earlier block).
    struct Slot
    {
        LoopSlot loop;
        std::array<CoreDSP::Loop::Voice, 2> voices {};
        std::array<int, 2> renderFrom { 0, 0 };
    };
    std::array<Slot, numSlots> slotStates;

    /** One pending quantised trigger per slot (a bounded list, plan 6.1). */
    struct PendingTrigger
    {
        bool active = false;
        int channel = 1;
        float velocity = 0.0f;
        double beatTime = 0.0;
        int slice = 0;
    };
    std::array<PendingTrigger, numSlots> pendingTriggers {};

    /** Toggle trigger mode (plan 14): the slots a toggle press started. The press
        semantics look at this flag; transport stop and a relocate clear it. */
    std::array<bool, numSlots> toggledOn {};

    // The arrangement strip's tool selection (plan 16 Phase 3): the source slice the
    // reverse tool acts on, -1 = none (whole loop). Message thread only.
    std::array<int, numSlots> selectedSlices { -1, -1, -1, -1 };

    // The last trigger's channel, velocity and slice per slot (plan 17 Phase 3): the
    // transformation tools retrigger the playing voice with the same articulation. Audio
    // thread writes, message thread reads.
    std::array<std::atomic<int>, numSlots> lastTriggerChannel {};
    std::array<std::atomic<int>, numSlots> lastTriggerSlice {};
    std::array<std::atomic<float>, numSlots> lastTriggerVelocity {};

    // Plan 17 Phase 3: the trigger mode of the previous block (audio thread only). A mode
    // change stops every voice and clears the toggle state. Per slot since plan 18.

    // Set from the message thread (plan 17 Phase 3): the next processBlock stops every
    // voice and clears the toggle state (a preset or state load just replaced the
    // parameters, so a running loop would outlive its own settings).
    std::atomic<bool> stopAllVoicesPending { false };

    // Set from the message thread (plan 17 Phase 3): the next processBlock restarts the
    // slot's playing voice on the freshly transformed loop, with the articulation it
    // started with (Latch/Toggle only — a Gate press plays the transform).
    std::array<std::atomic<bool>, numSlots> retransformPending {};

    /** Starts (or restarts) a slot's voice at a block-relative sample. */
    void startSlotVoice (int slot, int channel, float velocity, int sampleInBlock, int slice = 0);
    void releaseSlotVoice (int slot);

    /** The slot that claims a MIDI note: the slot whose Note parameter equals the key
        (a chopped slot claims the range slotNote .. slotNote + slices − 1, slice k on
        base + k). Two slots sharing a note: the lowest index wins. -1 when none.
        Phase 3 widens this for chop mode (base note + k). */
    int slotForNote (int key) const;

    /** The slice index a key triggers on a slot: 0 for an unchopped slot (the whole
        region), key − base for a chopped one. */
    int sliceForNote (int slot, int key) const;

    /** The next quantise grid point at or after an event, with the suite's swing on odd points. */
    double nextGridBeat (double blockStartBeat, int sampleInBlock, double samplesPerBeat,
                         double grid, float swingNorm) const;

    /** Reads one slot's parameters into a Region, against the loaded loop's length. A
        chopped slot narrows it to the given slice (0 for an unchopped slot). */
    CoreDSP::Loop::Region regionFor (int slot, const CoreDSP::Loop::Source& view, int slice = 0);

    /** The beat-clock tempo of the current block: the host's when Host Sync is on and it
        reports one, the Tempo parameter otherwise. Audio thread only. */
    double effectiveBpm (const juce::Optional<juce::AudioPlayHead::PositionInfo>& position) const;

    void flushPendingTriggers();

    // The rate is the one prepareToPlay was given: getSampleRate() is 0 in a bare test harness.
    double preparedSampleRate = 44100.0;

    // The effective tempo of the current block (plan 14): the host's when it reports one,
    // 120 BPM otherwise. Synced slots read it for the loop rate; unsynced slots use their
    // own Tempo parameter. Audio thread only.
    double blockBpm = 120.0;

    // The trigger seed: deterministic per instance and reproducible in a session.
    std::atomic<std::uint32_t> triggerCounter { 0 };

    // Master stage (plan 18): the one global Output, smoothed 20 ms. Pan moved per slot.
    juce::SmoothedValue<float> masterSmoother;

    // UI snapshots.
    std::array<std::atomic<float>, numSlots> playheads {};
    std::array<std::atomic<bool>, numSlots> activeFlags {};
    std::array<std::atomic<bool>, numSlots> loopMissing {};
    // Plan 20 (owner ask): each slot's post-level peak, for the mix row's meter.
    std::array<std::atomic<float>, numSlots> slotLevelPeaks {};
    std::array<juce::String, numSlots> missingPaths;
    std::atomic<double> beatPositionUiValue { 0.0 };

    // The preset identity (plan 09 1.4). Message thread only.
    SuitePresets::PresetSelection lastPresetSelection;

    // Host program list (embedded bank + SUITE_PRESET_BANK_AS_PROGRAMS). -1 means a state
    // that is not a factory program, for example a user preset.
    std::atomic<int> currentProgramIndex { 0 };

    // Parameters, cached as atomics (the audio thread never locks).
    std::atomic<float>* masterVolumeParam = nullptr;
    std::array<std::atomic<float>*, numSlots> startParams {};
    std::array<std::atomic<float>*, numSlots> lengthParams {};
    std::array<std::atomic<float>*, numSlots> speedParams {};
    std::array<std::atomic<float>*, numSlots> directionParams {};
    std::array<std::atomic<float>*, numSlots> pingRandomParams {};
    std::array<std::atomic<float>*, numSlots> granularParams {};
    std::array<std::atomic<float>*, numSlots> velocityParams {};
    std::array<std::atomic<float>*, numSlots> noteParams {};
    std::array<std::atomic<float>*, numSlots> beatsParams {};
    std::array<std::atomic<float>*, numSlots> chopParams {};
    std::array<std::atomic<float>*, numSlots> slicesParams {};
    std::array<std::atomic<float>*, numSlots> sliceLengthParams {};

    // Plan 17 Phase 2 (creative) and plan 18 Phase 4 (the former globals, per slot).
    std::array<std::atomic<float>*, numSlots> filterParams {};
    std::array<std::atomic<float>*, numSlots> attackParams {};
    std::array<std::atomic<float>*, numSlots> releaseParams {};
    std::array<std::atomic<float>*, numSlots> quantizeParams {};
    std::array<std::atomic<float>*, numSlots> swingParams {};
    std::array<std::atomic<float>*, numSlots> triggerParams {};
    std::array<std::atomic<float>*, numSlots> pitchParams {};
    std::array<std::atomic<float>*, numSlots> crossfadeParams {};
    std::array<std::atomic<float>*, numSlots> tempoParams {};
    std::array<std::atomic<float>*, numSlots> syncParams {};
    std::array<std::atomic<float>*, numSlots> panParams {};
    std::array<std::atomic<float>*, numSlots> cutoffParams {};
    std::array<std::atomic<float>*, numSlots> resoParams {};

    // Plan 18 Phase 4: the per-slot beat clocks (the plan-14 clock, one per slot).
    std::array<double, numSlots> slotClockBeats { 0.0, 0.0, 0.0, 0.0 };
    std::array<bool, numSlots> slotWasRunning { false, false, false, false };
    std::array<int, numSlots> lastBlockTriggerMode { -1, -1, -1, -1 };

    // The per-slot render stage (plan 18 Phase 4): each slot renders into its own scratch,
    // then filter, level and pan are applied before the master sum. Audio thread only.
    std::array<juce::AudioBuffer<float>, numSlots> slotScratch;
    std::array<juce::SmoothedValue<float>, numSlots> panSmoothers;
    std::array<juce::SmoothedValue<float>, numSlots> levelSmoothers;
    std::array<juce::dsp::StateVariableTPTFilter<float>, numSlots> slotFilters;

    // The per-slot tempo of the current block (plan 18 Phase 4): the host's when the slot
    // is synced and the host reports one, the slot's Tempo parameter otherwise. regionFor
    // reads it for the synced loop rate. Audio thread only.
    std::array<double, numSlots> slotBpmNow { 120.0, 120.0, 120.0, 120.0 };

    // Plan 18 Phase 6: the shared LFO and routing module. Declared after apvts, so the
    // router can read the parameters it belongs to.
    SuiteModulation::Router modulation;

    // The offsets of the current block (plan 18 Phase 6): regionFor reads them when a
    // voice starts, so a routed Speed is heard from the trigger. Audio thread only.
    SuiteModulation::Router::Offsets blockOffsets {};

    // The background loop loader. Declared last, so it is destroyed first and waits for
    // its jobs before the members it touches go away.
    juce::ThreadPool sampleLoadPool { 1 };
    std::atomic<int> pendingLoopLoads { 0 };

    // One lifetime token per in-flight load: a queued handoff is a no-op after destruction
    // (the plan 07 phase 4 pattern). Killed first thing in the destructor.
    std::array<std::shared_ptr<std::atomic<bool>>, numSlots> loadAliveTokens;
    std::array<std::shared_ptr<juce::Atomic<int>>, numSlots> loadGenerators;

    /** Clears one slot's auto-chop boundaries when the user edits Slice Length (plan 16:
        a manual edit falls back to the equal/skew split). Message thread. */
    struct ChopBoundsClearer : juce::AudioProcessorValueTreeState::Listener
    {
        explicit ChopBoundsClearer (CyclicalProcessor& o) : owner (o) {}

        void parameterChanged (const juce::String& parameter, float) override
        {
            for (int slot = 0; slot < numSlots; ++slot)
                if (parameter == slotSliceLengthIds[(size_t) slot])
                    owner.setChopBoundsProperty (slot, {});
        }

        CyclicalProcessor& owner;
    };
    ChopBoundsClearer chopBoundsClearer { *this };

    void finishLoopLoad (int slot, std::unique_ptr<LoadedLoop> loop, const juce::String& pathToSave);
    void setLoopPathProperty (int slot, const juce::String& path);
    void setChopBoundsProperty (int slot, const std::vector<float>& shares);
    juce::String chopBoundsPropertyId (int slot) const;
    juce::String chopOrderPropertyId (int slot) const;
    void installTransformedLoop (int slot, const juce::AudioBuffer<float>& transformed);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CyclicalProcessor)
};
