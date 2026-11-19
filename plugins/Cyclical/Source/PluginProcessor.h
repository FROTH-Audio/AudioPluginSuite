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
    static constexpr int numControls = 35;
    static constexpr int firstSlotNote = 60;      // C4..D#4 = slots 1..4

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

    static constexpr const char* pitchShiftId = "pitchShift";
    static constexpr const char* crossfadeLengthId = "crossfadeLength";

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
    };
    std::array<PendingTrigger, numSlots> pendingTriggers {};

    /** Starts (or restarts) a slot's voice at a block-relative sample. */
    void startSlotVoice (int slot, int channel, float velocity, int sampleInBlock);
    void releaseSlotVoice (int slot);

    /** Reads one slot's parameters into a Region, against the loaded loop's length. */
    CoreDSP::Loop::Region regionFor (int slot, int numFrames) const;

    void flushPendingTriggers();

    // The rate is the one prepareToPlay was given: getSampleRate() is 0 in a bare test harness.
    double preparedSampleRate = 44100.0;

    // The beat clock: host PPQ while the host plays, otherwise accrued at the host tempo
    // (120 BPM with no playhead). It only accumulates, so loops never jump on relocate.
    double beatPosition = 0.0;
    bool wasRunning = false;

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
    std::atomic<float>* pitchShiftParam = nullptr;
    std::atomic<float>* crossfadeLengthParam = nullptr;

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
