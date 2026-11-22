#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include "LoopSlot.h"
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
    static constexpr int numControls = 57;   // plan 14: +4 Note, +4 Beats, +Tempo, +Sync, +4 Chop/Slices/Slice Length
    static constexpr int maxSlices = 16;
    static constexpr int firstSlotNote = 60;      // the default base: C4..D#4 = slots 1..4

    // Parameter IDs are saved in sessions. Never rename or reuse one after a release.
    static constexpr const char* masterVolumeId = "masterVolume";
    static constexpr const char* panId = "pan";
    static constexpr const char* quantizeId = "quantize";
    static constexpr const char* swingId = "swing";
    static constexpr const char* triggerModeId = "triggerMode";

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

    static constexpr const char* pitchShiftId = "pitchShift";
    static constexpr const char* crossfadeLengthId = "crossfadeLength";

    // Plan 14: the musical clock. Sync on follows the host position and tempo; sync off
    // runs on the Tempo parameter (the MTC pattern).
    static constexpr const char* tempoId = "tempo";
    static constexpr const char* syncId = "sync";

    // The loop references are state properties, not parameters (the lerpCurve/samplePath
    // route). Brace-initialised, so the audit's parameter-ID regex does not see them.
    static constexpr const char* loop1PathProperty { "loop1Path" };
    static constexpr const char* loop2PathProperty { "loop2Path" };
    static constexpr const char* loop3PathProperty { "loop3Path" };
    static constexpr const char* loop4PathProperty { "loop4Path" };
    static constexpr const char* embeddedUriPrefix { "embedded:" };

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
    double beatPositionUi() const noexcept { return beatPositionUiValue.load (std::memory_order_relaxed); }

    /** The note a slot triggers on. Chop mode extends the claimed range upward (Phase 3):
        key slotNote + k triggers slice k. */
    int slotNote (int slot) const;

    /** The musical length of the slot's whole loaded loop, in beats (plan 14). */
    int slotBeats (int slot) const;

    /** True when the slot is chopped: its keys then cover slotNote .. slotNote + slices − 1. */
    bool slotChopped (int slot) const;
    int slotSlices (int slot) const;

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

    // The beat clock: host PPQ while the host plays, otherwise accrued at the host tempo
    // (120 BPM with no playhead). It only accumulates, so loops never jump on relocate.
    double beatPosition = 0.0;
    bool wasRunning = false;

    // The effective tempo of the current block (plan 14): regionFor reads it for the
    // synced loop rate. Audio thread only.
    double blockBpm = 120.0;

    // The trigger seed: deterministic per instance and reproducible in a session.
    std::atomic<std::uint32_t> triggerCounter { 0 };

    // Master stage, smoothed 20 ms as per-sample curves (the Spectral Drift rule).
    juce::SmoothedValue<float> masterSmoother;
    juce::SmoothedValue<float> panSmoother;
    std::vector<float> gainCurveL, gainCurveR;

    // UI snapshots.
    std::array<std::atomic<float>, numSlots> playheads {};
    std::array<std::atomic<bool>, numSlots> activeFlags {};
    std::array<std::atomic<bool>, numSlots> loopMissing {};
    std::array<juce::String, numSlots> missingPaths;
    std::atomic<double> beatPositionUiValue { 0.0 };

    // The preset identity (plan 09 1.4). Message thread only.
    SuitePresets::PresetSelection lastPresetSelection;

    // Host program list (embedded bank + SUITE_PRESET_BANK_AS_PROGRAMS). -1 means a state
    // that is not a factory program, for example a user preset.
    std::atomic<int> currentProgramIndex { 0 };

    // Parameters, cached as atomics (the audio thread never locks).
    std::atomic<float>* masterVolumeParam = nullptr;
    std::atomic<float>* panParam = nullptr;
    std::atomic<float>* quantizeParam = nullptr;
    std::atomic<float>* swingParam = nullptr;
    std::atomic<float>* triggerModeParam = nullptr;
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
    std::atomic<float>* pitchShiftParam = nullptr;
    std::atomic<float>* crossfadeLengthParam = nullptr;
    std::atomic<float>* tempoParam = nullptr;
    std::atomic<float>* syncParam = nullptr;

    // The background loop loader. Declared last, so it is destroyed first and waits for
    // its jobs before the members it touches go away.
    juce::ThreadPool sampleLoadPool { 1 };
    std::atomic<int> pendingLoopLoads { 0 };

    // One lifetime token per in-flight load: a queued handoff is a no-op after destruction
    // (the plan 07 phase 4 pattern). Killed first thing in the destructor.
    std::array<std::shared_ptr<std::atomic<bool>>, numSlots> loadAliveTokens;
    std::array<std::shared_ptr<juce::Atomic<int>>, numSlots> loadGenerators;

    void finishLoopLoad (int slot, std::unique_ptr<LoadedLoop> loop, const juce::String& pathToSave);
    void setLoopPathProperty (int slot, const juce::String& path);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CyclicalProcessor)
};
