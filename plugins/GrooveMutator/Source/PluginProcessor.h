#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

#include "CoreDSP.h"
#include "RhythmEngine.h"
#include "SuiteModulationRouter.h"
#include "SuiteState.h"
#include "SuitePresets.h"  // StateHolder interface for presets

/**
    Groove Mutator (plan 19).

    A generative rhythm-MIDI instrument: eight percussion lanes, each running a Euclid,
    hashed or painted-step pattern in its own grid, mutated on a musical tick by a
    deterministic operator set, with fills on the cycle's tail. One trigger key starts the
    groove (Latch or Gate); the lanes emit notes on one MIDI channel. Audio passes through
    unchanged.
*/
class GrooveMutatorProcessor : public juce::AudioProcessor, public SuitePresets::StateHolder
{
public:
    // Master. Parameter IDs are saved in sessions — never rename or reuse one after a release.
    static constexpr const char* syncId            = "sync";
    static constexpr const char* tempoId           = "tempo";
    static constexpr const char* triggerModeId     = "triggerMode";
    static constexpr const char* triggerNoteId     = "triggerNote";
    static constexpr const char* swingId           = "swing";
    static constexpr const char* humaniseTimeId    = "humaniseTime";
    static constexpr const char* humaniseVelocityId = "humaniseVelocity";
    static constexpr const char* velCurveId        = "velCurve";
    static constexpr const char* polyRatioId       = "polyRatio";
    static constexpr const char* chaosId           = "chaos";
    static constexpr const char* evolveRateId      = "evolveRate";
    static constexpr const char* mutateScaleId     = "mutateScale";
    static constexpr const char* freezeId          = "freeze";
    static constexpr const char* fillProbId        = "fillProb";
    static constexpr const char* fillStyleId       = "fillStyle";
    static constexpr const char* fillBarsId        = "fillBars";
    static constexpr const char* seedId            = "seed";

    // Per track ×8. The IDs are arrays with a literal size — the form the audit's
    // coverage check parses (the Cyclical precedent).
    static constexpr const char* trackOnIds[8]       = { "track1On", "track2On", "track3On", "track4On", "track5On", "track6On", "track7On", "track8On" };
    static constexpr const char* trackPatternIds[8]  = { "track1Pattern", "track2Pattern", "track3Pattern", "track4Pattern", "track5Pattern", "track6Pattern", "track7Pattern", "track8Pattern" };
    static constexpr const char* trackStepsIds[8]    = { "track1Steps", "track2Steps", "track3Steps", "track4Steps", "track5Steps", "track6Steps", "track7Steps", "track8Steps" };
    static constexpr const char* trackPulsesIds[8]   = { "track1Pulses", "track2Pulses", "track3Pulses", "track4Pulses", "track5Pulses", "track6Pulses", "track7Pulses", "track8Pulses" };
    static constexpr const char* trackRotationIds[8] = { "track1Rotation", "track2Rotation", "track3Rotation", "track4Rotation", "track5Rotation", "track6Rotation", "track7Rotation", "track8Rotation" };
    static constexpr const char* trackDivisionIds[8] = { "track1Division", "track2Division", "track3Division", "track4Division", "track5Division", "track6Division", "track7Division", "track8Division" };
    static constexpr const char* trackNoteIds[8]     = { "track1Note", "track2Note", "track3Note", "track4Note", "track5Note", "track6Note", "track7Note", "track8Note" };
    static constexpr const char* trackVelIds[8]      = { "track1Vel", "track2Vel", "track3Vel", "track4Vel", "track5Vel", "track6Vel", "track7Vel", "track8Vel" };
    static constexpr const char* trackAccentIds[8]   = { "track1Accent", "track2Accent", "track3Accent", "track4Accent", "track5Accent", "track6Accent", "track7Accent", "track8Accent" };
    static constexpr const char* trackFlamIds[8]     = { "track1Flam", "track2Flam", "track3Flam", "track4Flam", "track5Flam", "track6Flam", "track7Flam", "track8Flam" };
    static constexpr const char* trackGhostIds[8]    = { "track1Ghost", "track2Ghost", "track3Ghost", "track4Ghost", "track5Ghost", "track6Ghost", "track7Ghost", "track8Ghost" };
    static constexpr const char* trackGainIds[8]     = { "track1Gain", "track2Gain", "track3Gain", "track4Gain", "track5Gain", "track6Gain", "track7Gain", "track8Gain" };

    static constexpr int numTracks = 8;

    // The GM drum map the defaults walk: 36 kick, 38 snare, 42 chh, 46 ohh, 41 low tom,
    // 45 mid tom, 39 clap, 75 claves.
    static constexpr int defaultTrackNotes[8] = { 36, 38, 42, 46, 41, 45, 39, 75 };

    // LFO 1 and LFO 2 + the routing slots. The IDs are fixed by suite_modulation.
    static constexpr const char* lfo1RateId      = "lfo1Rate";
    static constexpr const char* lfo1ShapeId     = "lfo1Shape";
    static constexpr const char* lfo1SyncId      = "lfo1Sync";
    static constexpr const char* lfo1DivisionId  = "lfo1Division";
    static constexpr const char* lfo1OffsetId    = "lfo1Offset";
    static constexpr const char* lfo1InvertId    = "lfo1Invert";
    static constexpr const char* lfo1RectifyId   = "lfo1Rectify";
    static constexpr const char* lfo1QuantiseId  = "lfo1Quantise";
    static constexpr const char* lfo1SmoothId    = "lfo1Smooth";
    static constexpr const char* lfo1PulseWidthId = "lfo1PulseWidth";
    static constexpr const char* lfo1SkewId      = "lfo1Skew";
    static constexpr const char* lfo2RateId      = "lfo2Rate";
    static constexpr const char* lfo2ShapeId     = "lfo2Shape";
    static constexpr const char* lfo2SyncId      = "lfo2Sync";
    static constexpr const char* lfo2DivisionId  = "lfo2Division";
    static constexpr const char* lfo2OffsetId    = "lfo2Offset";
    static constexpr const char* lfo2InvertId    = "lfo2Invert";
    static constexpr const char* lfo2RectifyId   = "lfo2Rectify";
    static constexpr const char* lfo2QuantiseId  = "lfo2Quantise";
    static constexpr const char* lfo2SmoothId    = "lfo2Smooth";
    static constexpr const char* lfo2PulseWidthId = "lfo2PulseWidth";
    static constexpr const char* lfo2SkewId      = "lfo2Skew";
    static constexpr const char* modSourceIds[6] = { "mod1Source", "mod2Source", "mod3Source", "mod4Source", "mod5Source", "mod6Source" };
    static constexpr const char* modTargetIds[6] = { "mod1Target", "mod2Target", "mod3Target", "mod4Target", "mod5Target", "mod6Target" };
    static constexpr const char* modAmountIds[6] = { "mod1Amount", "mod2Amount", "mod3Amount", "mod4Amount", "mod5Amount", "mod6Amount" };
    static constexpr const char* modDelayIds[6]  = { "mod1Delay",  "mod2Delay",  "mod3Delay",  "mod4Delay",  "mod5Delay",  "mod6Delay" };

    static constexpr int numControls = 159;   // master 17 + 8 tracks × 12 + router 46 (recounted at build time)

    /** The parameters an LFO can move, in ModTarget order (append-only; the choice index
        is the position + 1). Per the plan: the six master musical inputs plus the eight
        per-track velocities. Recorded here before code. */
    enum class ModTarget
    {
        None = 0,
        Chaos,
        MutationScale,
        FillProbability,
        Swing,
        HumaniseTime,
        HumaniseVelocity,
        TrackVel1,
        TrackVel2,
        TrackVel3,
        TrackVel4,
        TrackVel5,
        TrackVel6,
        TrackVel7,
        TrackVel8,
        count
    };

    static juce::StringArray modTargetNames();

    /** The shared LFO and routing module. */
    SuiteModulation::Router& getRouter() noexcept { return modulation; }

    /** The value a control shows right now: its base value moved by the published offset. */
    float getModulatedValue (ModTarget target) const;

    /** The per-track velocity multiplier the curve, accent, ghost and gain produce for one
        step — exposed for the LaneView's velocity shading. */
    float trackVelocityFor (int track, int patternIndex) const;

    juce::AudioProcessorValueTreeState apvts;

    /** The lanes' live masks for the editor's LaneView (plan 19 Phase 3). */
    std::uint32_t laneMask (int track) const noexcept;

    /** The lane's painted Steps mask (the editor's draw mode; pattern kind 2). */
    std::uint32_t getLaneStepsMask (int track) const noexcept;

    /** Paints or erases one step of a lane's Steps pattern (message thread). Persisted as
        the stepsMasks state property (the lerpCurve pattern) and re-derived like any
        pattern-defining value. */
    void setLaneStep (int track, int step, bool on);

    /** Whether a step carries a flam (the schedule's hash rule, for the view). */
    bool laneFlamsAt (int track, int patternIndex) const;

    /** Whether a step is an accent hit (the schedule's hash rule, for the view). */
    bool laneAccentsAt (int track, int patternIndex) const;

    /** Whether a step is a ghost hit (the schedule's hash rule, exclusive with accent). */
    bool laneGhostsAt (int track, int patternIndex) const;

    /** The attractor trail for the MUTATE page's phase-space plot (the MTC pattern). */
    std::vector<juce::Point<float>> getTrail() const;

    /** The per-track playhead step (−1 when the groove is not running). */
    std::atomic<int> playheadStep[numTracks] {};

    // Bump stateVersion when the saved state changes, and add a migration from the old version.
    int stateVersion = 1;
    std::vector<SuiteState::Migration> migrations;

    GrooveMutatorProcessor();
    ~GrooveMutatorProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Groove Mutator"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override;
    void getCurrentProgramStateInformation (juce::MemoryBlock&) override;
    void setCurrentProgramStateInformation (const void*, int) override;
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Preset system: StateHolder interface
    juce::String getPresetPluginId() const override { return "GrooveMutator"; }
    int getPresetStateVersion() const override { return stateVersion; }
    juce::AudioProcessorValueTreeState& getPresetStateTree() override { return apvts; }
    void applyPresetState (const juce::ValueTree&) override;

    // The preset identity (plan 09 1.4). Message thread. Persisted as the presetSelection
    // state property, so it survives the editor being reopened and rides in saved sessions.
    SuitePresets::PresetSelection getPresetSelection() const override { return lastPresetSelection; }
    void setPresetSelection (const SuitePresets::PresetSelection&) override;
    static constexpr const char* presetSelectionProperty { "presetSelection" };

    // The painted Steps masks ride as one state property (the lerpCurve pattern).
    static constexpr const char* stepsMasksProperty { "stepsMasks" };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    /** The attractor follows the musical time (the MTC pattern) and feeds the trail. */
    void advanceAttractorTo (double beat) noexcept;
    void publishTrailPoint() noexcept;

    /** The painted Steps masks as the "stepsMasks" property (8 hex tokens). */
    void loadStepsMasksFromState();
    void storeStepsMasksToState();

    /** The lane's step length in beats: division × polyrhythm. */
    double laneStepBeats (int track) const noexcept;

    /** Re-derives a lane's base and live masks when its pattern-defining values changed. */
    void rederiveLaneIfNeeded (int track);

    /** The mutation tick: one crossing of the evolve-rate boundary mutates every on lane
        (gated by Chaos, skipped by Freeze). Deterministic from (seed, tick, lane). */
    void runMutationTick (double beat);

    /** The musical groove: schedules every lane's steps that fall inside the block. */
    void scheduleGroove (double blockStartBeat, double blockEndBeat, double samplesPerBeat);

    void scheduleLaneStep (int track, std::int64_t stepIndex, double stepBeat,
                           double blockStartBeat, double samplesPerBeat);
    void queueGrooveNote (int track, int note, int velocity, std::int64_t onTarget, std::int64_t lengthSamples);
    void stopAllNotes();

    using Queue = std::multimap<std::int64_t, juce::MidiMessage>;

    double currentBpm (const juce::Optional<juce::AudioPlayHead::PositionInfo>& position, float freeTempo) const;

    double sampleRate = 44100.0;
    std::int64_t streamSample = 0;
    std::int64_t latencySamples = 0;

    // Transport and the master clock.
    double beatPosition = 0.0;
    bool resyncNeeded = true;
    bool wasRunning = false;          // the transport was running
    bool grooveWasActive = false;     // the trigger was active
    bool triggerActive = false;

    // Panic flag from a preset/state load (the MTC rule): the next block stops everything.
    std::atomic<bool> panicRequested { false };

    // The trigger-mode change is an ending path: the cache detects the change per block.
    int lastTriggerMode = -1;

    // Per-lane engine state.
    std::array<CoreDSP::Rhythm::LaneParams, numTracks> laneParams;
    std::array<CoreDSP::Rhythm::LaneState, numTracks> laneStates;
    std::array<std::int64_t, numTracks> laneNextStep {};
    std::array<std::int64_t, numTracks> laneFillCycle { -1, -1, -1, -1, -1, -1, -1, -1 };
    std::array<std::uint32_t, numTracks> laneFillAdditions {};
    std::array<std::uint32_t, numTracks> laneBaseSignature {};

    // The painted Steps masks (the editor's draw mode). Atomics so the paint lands
    // between blocks; the audio thread reads them in rederiveLaneIfNeeded.
    std::array<std::atomic<std::uint32_t>, numTracks> laneStepsMask {};

    // The attractor and its published trail (the MTC pattern, plan 19 Phase 3).
    static constexpr int trailLength = 256;
    struct TrailSnapshot
    {
        std::array<std::atomic<float>, trailLength> x {};
        std::array<std::atomic<float>, trailLength> y {};
        std::atomic<int> head { 0 };
    };
    CoreDSP::Chaos::LorenzAttractor attractor;
    double attractorBeat = 0.0;
    TrailSnapshot trail;

    // The mutation tick bookkeeping.
    std::int64_t lastMutationTick = -1;

    // Event queue. Keys are absolute sample times. pendingOffs remembers each note's
    // queued note-off, so a retriggered note can cut off its earlier copy first.
    Queue pending;
    std::map<int, Queue::iterator> pendingOffs;
    std::map<int, std::int64_t> lastNoteOn;
    std::set<int> sounding;

    std::atomic<float>* sync = nullptr;
    std::atomic<float>* tempo = nullptr;
    std::atomic<float>* triggerMode = nullptr;
    std::atomic<float>* triggerNote = nullptr;
    std::atomic<float>* swing = nullptr;
    std::atomic<float>* humaniseTime = nullptr;
    std::atomic<float>* humaniseVelocity = nullptr;
    std::atomic<float>* velCurve = nullptr;
    std::atomic<float>* polyRatio = nullptr;
    std::atomic<float>* chaos = nullptr;
    std::atomic<float>* evolveRate = nullptr;
    std::atomic<float>* mutateScale = nullptr;
    std::atomic<float>* freeze = nullptr;
    std::atomic<float>* fillProb = nullptr;
    std::atomic<float>* fillStyle = nullptr;
    std::atomic<float>* fillBars = nullptr;
    std::atomic<float>* seed = nullptr;

    std::array<std::atomic<float>*, numTracks> onParams {};
    std::array<std::atomic<float>*, numTracks> patternParams {};
    std::array<std::atomic<float>*, numTracks> stepsParams {};
    std::array<std::atomic<float>*, numTracks> pulsesParams {};
    std::array<std::atomic<float>*, numTracks> rotationParams {};
    std::array<std::atomic<float>*, numTracks> divisionParams {};
    std::array<std::atomic<float>*, numTracks> noteParams {};
    std::array<std::atomic<float>*, numTracks> velParams {};
    std::array<std::atomic<float>*, numTracks> accentParams {};
    std::array<std::atomic<float>*, numTracks> flamParams {};
    std::array<std::atomic<float>*, numTracks> ghostParams {};
    std::array<std::atomic<float>*, numTracks> gainParams {};

    // Declared after apvts, so the router can read the parameters it belongs to.
    SuiteModulation::Router modulation;

    // The host program list (embedded bank + SUITE_PRESET_BANK_AS_PROGRAMS).
    std::atomic<int> currentProgramIndex { 0 };

    // The preset identity (plan 09 1.4). Message thread only.
    SuitePresets::PresetSelection lastPresetSelection;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrooveMutatorProcessor)
};
