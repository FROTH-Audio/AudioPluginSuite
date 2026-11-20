#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <vector>

#include "SuiteState.h"
#include "SuitePresets.h"

#include "LoadedSample.h"

#include <CoreDSP.h>

#include "SuiteModulationRouter.h"

/**
    Sgraffito - a memory sampler.

    Treats each note as a new layer over the sample rather than a fresh start.
    The keyboard is a timeline through the sample, and every key has its own
    character that slowly drifts over time.
*/
class SgraffitoProcessor : public juce::AudioProcessor,
                           public SuitePresets::StateHolder
{
public:
    //==============================================================================
    // Parameter IDs. Saved in sessions: never rename or reuse after a release.
    static constexpr const char* modeId = "mode";
    static constexpr const char* regionSpanId = "regionSpan";
    static constexpr const char* regionLengthId = "regionLength";
    static constexpr const char* rootKeyId = "rootKey";
    static constexpr const char* pitchFineId = "pitchFine";
    static constexpr const char* scaleLockId = "scaleLock";
    static constexpr const char* scaleId = "scale";
    static constexpr const char* glideId = "glide";
    static constexpr const char* levelId = "level";
    static constexpr const char* velocityCurveId = "velocityCurve";
    static constexpr const char* attackId = "attack";
    static constexpr const char* releaseId = "release";
    static constexpr const char* tapeStopTimeId = "tapeStopTime";
    static constexpr const char* tapeStopCurveId = "tapeStopCurve";
    static constexpr const char* memoryAmountId = "memoryAmount";
    static constexpr const char* memoryDecayId = "memoryDecay";
    static constexpr const char* driftAmountId = "driftAmount";
    static constexpr const char* driftSpeedId = "driftSpeed";
    static constexpr const char* layBackId = "layBack";
    static constexpr const char* swingId = "swing";
    static constexpr const char* humaniseId = "humanise";
    static constexpr const char* ghostOnId = "ghostOn";
    static constexpr const char* ghostThresholdId = "ghostThreshold";
    static constexpr const char* ghostLevelId = "ghostLevel";
    static constexpr const char* ghostDelayId = "ghostDelay";
    static constexpr const char* sliceCountId = "sliceCount";
    static constexpr const char* grainSizeId = "grainSize";
    static constexpr const char* grainDensityId = "grainDensity";
    static constexpr const char* grainSpreadId = "grainSpread";
    static constexpr const char* sustainHoldId = "sustainHold";
    static constexpr const char* swellTimeId = "swellTime";
    static constexpr const char* swellCrossfadeId = "swellCrossfade";
    static constexpr const char* dustId = "dust";
    static constexpr const char* crackleId = "crackle";
    static constexpr const char* wowId = "wow";
    static constexpr const char* flutterId = "flutter";
    static constexpr const char* shuffleSeedId = "shuffleSeed";

    // LFO 1 and LFO 2
    static constexpr const char* lfo1RateId = "lfo1Rate";
    static constexpr const char* lfo1ShapeId = "lfo1Shape";
    static constexpr const char* lfo1SyncId = "lfo1Sync";
    static constexpr const char* lfo1DivisionId = "lfo1Division";
    static constexpr const char* lfo1OffsetId = "lfo1Offset";
    static constexpr const char* lfo2RateId = "lfo2Rate";
    static constexpr const char* lfo2ShapeId = "lfo2Shape";
    static constexpr const char* lfo2SyncId = "lfo2Sync";
    static constexpr const char* lfo2DivisionId = "lfo2Division";
    static constexpr const char* lfo2OffsetId = "lfo2Offset";

    // Modulation routing: six slots, each with a source, a target, an amount and a delay
    static constexpr const char* modSourceIds[6] = { "mod1Source", "mod2Source", "mod3Source", "mod4Source", "mod5Source", "mod6Source" };
    static constexpr const char* modTargetIds[6] = { "mod1Target", "mod2Target", "mod3Target", "mod4Target", "mod5Target", "mod6Target" };
    static constexpr const char* modAmountIds[6] = { "mod1Amount", "mod2Amount", "mod3Amount", "mod4Amount", "mod5Amount", "mod6Amount" };
    static constexpr const char* modDelayIds[6] = { "mod1Delay", "mod2Delay", "mod3Delay", "mod4Delay", "mod5Delay", "mod6Delay" };

    static constexpr int numControls = 71;
    static constexpr int lfoCount = 2;
    static constexpr int slotCount = 6;

    /** The parameters that an LFO can move. The order is part of the saved session: append new
        targets at the end, and never reorder or remove existing ones. Names come from modTargetNames(). */
    enum class ModTarget
    {
        None = 0,
        RegionSpan,
        RegionLength,
        RootKey,
        PitchFine,
        Glide,
        Level,
        Attack,
        Release,
        MemoryAmount,
        DriftAmount,
        DriftSpeed,
        LayBack,
        Swing,
        Humanise,
        SustainHold,
        GhostThreshold,
        GhostLevel,
        GrainSize,
        GrainDensity,
        GrainSpread,
        SwellTime,
        Dust,
        Crackle,
        Wow,
        Flutter,
        ShuffleSeed,
    };

    /** Modulation routing targets - declared here so they're available in the constructor initializer. */
    static constexpr SuiteModulation::TargetInfo modulationTargets[] = {
        { "regionSpan",     "Region Span" },
        { "regionLength",   "Region Length" },
        { "rootKey",        "Root Key",   true },
        { "pitchFine",      "Pitch Fine" },
        { "glide",          "Glide" },
        { "level",          "Output" },
        { "attack",         "Attack" },
        { "release",        "Release" },
        { "memoryAmount",   "Memory Amount" },
        { "driftAmount",    "Drift Amount" },
        { "driftSpeed",     "Drift Speed" },
        { "layBack",        "Lay Back" },
        { "swing",          "Swing" },
        { "humanise",       "Humanise" },
        { "sustainHold",    "Sustain Hold", true },
        { "ghostThreshold", "Ghost Threshold" },
        { "ghostLevel",     "Ghost Level" },
        { "grainSize",      "Grain Size" },
        { "grainDensity",   "Grain Density" },
        { "grainSpread",    "Grain Spread" },
        { "swellTime",      "Swell Time" },
        { "dust",           "Dust" },
        { "crackle",        "Crackle" },
        { "wow",            "Wow" },
        { "flutter",        "Flutter" },
        { "shuffleSeed",    "Shuffle Seed", true },
    };

    static constexpr int numModulationTargets = sizeof(modulationTargets) / sizeof(SuiteModulation::TargetInfo);
    static constexpr int ModTargetCount = numModulationTargets;

    static juce::StringArray modTargetNames();

    //==============================================================================
    SgraffitoProcessor();
    ~SgraffitoProcessor() override;

    //==============================================================================
    // StateHolder
    juce::String getPresetPluginId() const override { return "Sgraffito"; }
    int getPresetStateVersion() const override { return stateVersion; }
    juce::AudioProcessorValueTreeState& getPresetStateTree() override { return apvts; }
    void applyPresetState (const juce::ValueTree&) override;

    // The preset identity (plan 09 1.4): which factory preset or user file the bar last
    // applied. Message thread. Written through setPresetSelection, restored by
    // setStateInformation, reset by applyPresetState (the caller re-states it right after).
    SuitePresets::PresetSelection getPresetSelection() const override { return lastPresetSelection; }
    void setPresetSelection (const SuitePresets::PresetSelection&) override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Sgraffito"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}
    void getCurrentProgramStateInformation (juce::MemoryBlock&) override;
    void setCurrentProgramStateInformation (const void*, int) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Voice access for the editor and modulation
    // const juce::AudioBuffer<float>* getSampleBuffer() const noexcept;  // inline below
    
    int getNumActiveVoices() const noexcept { return voicePool.countActive(); }
    
    // Sample loading
    void loadSample (const juce::File& file);
    /** Loads a file sample on a background thread and installs it on the message thread
        (plan 07 phase 4). The current sample keeps playing until the load completes;
        the latest request wins. Requires a message thread for the handoff. */
    void loadSampleAsync (const juce::File& file);
    void loadEmbeddedSample (const juce::String& embeddedPath);  // e.g., "embedded:grain_4k.wav"
    void clearSample();

    /** True while a background sample load has not yet been installed (plan 07 phase 4).
        Tests pump the message loop until this clears. */
    bool hasPendingSampleLoad() const noexcept { return pendingSampleLoads.load() > 0; }

    juce::String getSamplePath() const noexcept { return sampleFile.getFullPathName(); }
    bool hasSample() const noexcept { return sampleBuffer.load() != nullptr; }

    // Sample-linked presets and sessions (plan 07): the loaded sample travels as the
    // samplePath property on <PARAMETERS>, the same non-parameter route as the
    // Micro-Timing lerp curve. It is not an APVTS parameter.
    static constexpr const char* samplePathProperty { "samplePath" };
    static constexpr const char* embeddedSampleUri { "embedded:grain_4k.wav" };

    // The preset identity travels as the presetSelection property on <PARAMETERS>
    // (plan 09 1.4, owner decision D3): "factory:<index>" / "user:<path>" / absent = Init.
    // Additive like samplePath: a session saved before plan 09 simply reads as Init.
    static constexpr const char* presetSelectionProperty { "presetSelection" };

    /** Writes the current sample reference into a state tree (file path or embedded: URI).
        A tree without a sample speaks about no sample: applying it leaves the sample alone. */
    void storeSampleToState (juce::ValueTree& state) const;

    /** Applies the sample rule from an incoming preset or session tree (message thread).
        An empty tree is INIT (bar and host program 0): the sample resets to the embedded
        default like every other parameter. A tree with samplePath loads it; a missing
        file keeps the current sample playing and raises sampleMissing. */
    void applySampleFromState (const juce::ValueTree& tree);

    /** True when the last preset/session asked for a sample that could not be found. */
    bool isSampleMissing() const noexcept { return sampleMissing.load (std::memory_order_acquire); }
    juce::String getMissingSamplePath() const { return missingSamplePath; }

    /** The folder searched when a preset or session names a sample that does not exist
        at its stored path (plan 07 2.2, owner decision Q5). */
    static juce::File userSampleFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                  .getChildFile ("AudioSuite").getChildFile ("Sgraffito").getChildFile ("Samples");
    }

    /** Where the fallback search looks. Empty (the normal state) = userSampleFolder();
        tests set it so they do not write into the user's Documents folder. */
    juce::File sampleSearchFolder;

    /** The raw value of a parameter, for the editor's visual widgets (waveform, key map). */
    float getParamValue (const char* id) const
    {
        if (auto* parameter = apvts.getRawParameterValue (id))
            return parameter->load();
        return 0.0f;
    }

    /** The rate the plugin was prepared at, for the editor's envelope preview. */
    double getPreparedSampleRate() const noexcept { return preparedSampleRate; }

    /** The LFOs and routing, for the editor's panel and control display. */
    SuiteModulation::Router& getRouter() noexcept { return modulation; }

    /** The value a control shows right now: its base value moved by the offset currently published for its target. */
    float getModulatedValue (ModTarget target) const { return modulation.getModulatedValue ((int) target); }

    // APVTS is public so the editor and tests can build attachments.
    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "PARAMETERS", createParameterLayout() };

    // The two LFOs and the six routing slots, shared with the other plugins (modules/suite_modulation).
    SuiteModulation::Router modulation;

    // Voice pool (public for tests).
    CoreDSP::Sampler::VoicePool voicePool;

    // Lorenz attractor for chaos drift.
    CoreDSP::Chaos::LorenzAttractor lorenz;

    friend class SgraffitoEditor;  // Allow editor to access private members

    // Ghost note state: ghost notes are quieter, delayed copies of notes played below
    // the ghost threshold.
    struct GhostNote
    {
        bool active = false;
        int key = 60;
        float velocity = 1.0f;      // ghost velocity (amount * original)
        double position = 0.0;      // read position in the sample
        double rate = 1.0f;         // playback rate
        double delayRemaining = 0.0; // samples remaining before ghost starts
        double regionStart = 0.0;   // region start (fraction of sample length)
        double regionEnd = 0.0;     // region end (fraction of sample length)
    };

    // Ghost notes waiting to trigger.
    static constexpr int maxGhostVoices = 8;
    std::array<GhostNote, maxGhostVoices> ghostNotes;

    // The loaded sample (public for tests).
    std::unique_ptr<LoadedSample> loadedSample;

    // Retirement queue: capped at 2 generations to prevent a running audio block from
    // being starved of memory. The deque grows only on the message thread (install path),
    // so allocation is acceptable there. The audio thread never touches it.
    static constexpr int retiredSampleGenerations = 2;
    std::deque<std::unique_ptr<LoadedSample>> retiredSamples;

    void retireCurrentSample();

    // The loaded sample buffer (atomic swap for thread safety).
    std::atomic<const juce::AudioBuffer<float>*> sampleBuffer { nullptr };

    // The original file path (empty for embedded samples).
    juce::File sampleFile;

    // Missing-sample report for the UI (plan 07 phase 2 draws the overlay). Both are
    // written on the message thread only.
    std::atomic<bool> sampleMissing { false };
    juce::String missingSamplePath;

    // Host program list (embedded bank + SUITE_PRESET_BANK_AS_PROGRAMS). -1 means a state
    // that is not a factory program, for example a user preset.
    std::atomic<int> currentProgramIndex { 0 };

    // Pending note-ons for lay back delay, swing and humanise (plan 05.4). The queue keeps
    // notes that are not due yet; each entry carries the absolute scheduled position and the
    // delay that was computed at note-on time, in samples.
    struct PendingNote
    {
        int key = 60;
        float velocity = 1.0f;
        double samplePosition = 0.0;      // onset, in stream samples
        double delaySamples = 0.0;        // start delay past the onset, in samples
        double scheduledPos = 0.0;        // absolute onset + delay at note-on time, in samples
        bool ghost = false;               // a ghost copy: quieter, delayed; allocates a real voice at fire time
    };
    std::vector<PendingNote> pendingNotes;

    // Swing and humanise state (plan 05.4). Swing alternates straight/swung on repeated
    // note-ons of the same key; humanise seeds a deterministic per-note start offset.
    std::array<std::uint8_t, 128> swingState {};
    std::uint32_t humaniseSeed = 0x51A5u;

    //==============================================================================
    // Notes played from the editor's keyboard strip. A single-producer/single-consumer
    // ring: the message thread writes, the audio thread drains at the top of processBlock
    // and runs the notes through the normal note handling. No locks, no allocations.
    struct UiNote
    {
        int key = 60;
        float velocity = 1.0f;
        bool on = true;
    };

    static constexpr int uiNoteCapacity = 32;

    void pushUiNote (int key, bool on, float velocity = 0.8f)
    {
        const int write = uiNoteWrite.load (std::memory_order_relaxed);
        const int next = (write + 1) % uiNoteCapacity;

        if (next == uiNoteRead.load (std::memory_order_acquire))
            return;   // full: drop the oldest would need a second index; a full ring drops this note

        uiNoteRing[(size_t) write] = { key, velocity, on };
        uiNoteWrite.store (next, std::memory_order_release);
    }

    UiNote uiNoteRing[(size_t) uiNoteCapacity];
    std::atomic<int> uiNoteRead { 0 };
    std::atomic<int> uiNoteWrite { 0 };

    // Per-sample-rate derived values.
    double preparedSampleRate = 44100.0;

    // The output gain (plan 12): the former dead Level knob, now the final gain stage,
    // smoothed 20 ms so automation does not zip. 0 dB is exactly 1.0, so the default is
    // bit-identical to the version without the knob.
    juce::SmoothedValue<float> outputGainSmoothed;

    // Free-LFO clock: stream position in samples and a tempo-locked beat clock (see SuiteModulation).
    std::int64_t streamSample = 0;
    double beatClock = 0.0;

private:
    /** The current sample buffer (atomic swap for thread safety). */
    const juce::AudioBuffer<float>* getSampleBuffer() const noexcept
    {
        return sampleBuffer.load(std::memory_order_acquire);
    }
    
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Applies an offset to a parameter's base value, in the parameter's own normalised range. Exact when the offset is zero. */
    float modulatedValue (ModTarget target, float base, const SuiteModulation::Router::Offsets& offsets) const
    {
        return modulation.modulate ((int)target, base, offsets);
    }

    void handleNoteOn (int key, float velocity, int samplePosition, float rootKeyMod, float layBackMod, float spanMod, float lengthMod, float humaniseMod, juce::AudioParameterFloat* sustainHoldParam, float releaseMs);
    void handleNoteOff (int key, int samplePosition, bool sustainOn, float releaseMs, double blockStartSample, juce::AudioParameterFloat* tapeStopTimeParam, juce::AudioParameterFloat* tapeStopCurveParam);

    /** Queue a ghost copy of a note (plan 05.4 Phase 2); one-shot, one slot per ghost. */
    void queueGhost (int key, float ghostVelocity, double samplePosition, double ghostDelaySamples, double mainDelaySamples);

    /** Post-allocation setup shared by the direct, ghost and pending note paths (plan 09 1.3):
        pitch from the key and PITCH FINE, the velocity curve, and the start position from
        the region the pool assigned. Everything else comes from VoicePool::allocateVoice. */
    void setupAllocatedVoice (int idx, int key, float velocity, float rootKeyMod);

    void handleNoteOn(int keyIn, float velocity, int samplePosition, float rootKeyMod, float layBackMod, float spanMod, float lengthMod, float humaniseMod, std::atomic<float> *sustainHoldParam, float releaseMs);

    // Parameter pointers, cached for the audio thread.
    std::atomic<float>* modeParam = nullptr;
    std::atomic<float>* regionSpanParam = nullptr;
    std::atomic<float>* regionLengthParam = nullptr;
    std::atomic<float>* rootKeyParam = nullptr;
    std::atomic<float>* pitchFineParam = nullptr;
    std::atomic<float>* scaleLockParam = nullptr;
    std::atomic<float>* scaleParam = nullptr;
    std::atomic<float>* glideParam = nullptr;
    std::atomic<float>* levelParam = nullptr;
    std::atomic<float>* velocityCurveParam = nullptr;
    std::atomic<float>* attackParam = nullptr;
    std::atomic<float>* releaseParam = nullptr;
    std::atomic<float>* tapeStopTimeParam = nullptr;
    std::atomic<float>* tapeStopCurveParam = nullptr;
    std::atomic<float>* memoryAmountParam = nullptr;
    std::atomic<float>* memoryDecayParam = nullptr;
    std::atomic<float>* driftAmountParam = nullptr;
    std::atomic<float>* driftSpeedParam = nullptr;
    std::atomic<float>* layBackParam = nullptr;
    std::atomic<float>* swingParam = nullptr;
    std::atomic<float>* humaniseParam = nullptr;
    std::atomic<float>* ghostOnParam = nullptr;
    std::atomic<float>* ghostThresholdParam = nullptr;
    std::atomic<float>* ghostLevelParam = nullptr;
    std::atomic<float>* ghostDelayParam = nullptr;
    std::atomic<float>* sliceCountParam = nullptr;
    std::atomic<float>* grainSizeParam = nullptr;
    std::atomic<float>* grainDensityParam = nullptr;
    std::atomic<float>* grainSpreadParam = nullptr;
    std::atomic<float>* sustainHoldParam = nullptr;
    std::atomic<float>* swellTimeParam = nullptr;
    std::atomic<float>* swellCrossfadeParam = nullptr;
    std::atomic<float>* dustParam = nullptr;
    std::atomic<float>* crackleParam = nullptr;
    std::atomic<float>* wowParam = nullptr;
    std::atomic<float>* flutterParam = nullptr;
    std::atomic<float>* shuffleSeedParam = nullptr;

    // LFO and routing parameter pointers, cached for the audio thread.
    std::atomic<float>* lfo1RateParam = nullptr;
    std::atomic<float>* lfo1ShapeParam = nullptr;
    std::atomic<float>* lfo1SyncParam = nullptr;
    std::atomic<float>* lfo1DivisionParam = nullptr;
    std::atomic<float>* lfo1OffsetParam = nullptr;
    std::atomic<float>* lfo2RateParam = nullptr;
    std::atomic<float>* lfo2ShapeParam = nullptr;
    std::atomic<float>* lfo2SyncParam = nullptr;
    std::atomic<float>* lfo2DivisionParam = nullptr;
    std::atomic<float>* lfo2OffsetParam = nullptr;
    std::atomic<float>* modSourceParam[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    std::atomic<float>* modTargetParam[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    std::atomic<float>* modAmountParam[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    std::atomic<float>* modDelayParam[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

    int stateVersion = 1;
    std::vector<SuiteState::Migration> migrations;

    // The preset identity (plan 09 1.4). Message thread only.
    SuitePresets::PresetSelection lastPresetSelection;

    //==============================================================================
    // Background sample loads (plan 07 phase 4). The read + resample runs on a pooled
    // thread; the install (retire pattern, atomic swap, property write) runs on the
    // message thread. `loadGeneration` makes the latest request win: each load request
    // bumps it and an install that arrives out of order is discarded. `loadLifetime`
    // lets a queued handoff notice that the processor is gone; it is set to dead first
    // thing in the destructor, before any member is destroyed.
    struct LoadLifetime
    {
        std::atomic<bool> alive { true };
    };

    std::shared_ptr<LoadLifetime> loadLifetime { std::make_shared<LoadLifetime>() };
    std::atomic<int> loadGeneration { 0 };
    std::atomic<int> pendingSampleLoads { 0 };

    /** Shared install path: retire the old sample, swap the atomics and sync the
        samplePath property. Message thread only. */
    void finishSampleLoad (juce::AudioBuffer<float>&& buffer, const juce::String& sourcePath,
                           double sourceSampleRate);

    class SampleLoadJob;   // defined in the .cpp

    // Declared last: destroyed first, and the destructor waits for running jobs, so a
    // job never outlives the members it would hand off to.
    juce::ThreadPool sampleLoadPool;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SgraffitoProcessor)
};
