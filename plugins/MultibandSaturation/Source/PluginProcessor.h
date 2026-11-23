#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <vector>

#include "MultibandEngine.h"
#include "SaturationEngine.h"
#include "SuiteModulationRouter.h"
#include "SuiteState.h"
#include "SuitePresets.h"   // StateHolder interface for presets

/**
    Multiband Saturation (plan 06 idea 2.4, first pass).

    A 2-way Linkwitz-Riley split (CoreDSP::Multiband::TwoBandSplit): the low band and the high
    band each get their own drive and tone, using the shared saturation shapers and envelope
    followers (CoreDSP::Saturation). The bands recombine through the crossover's all-pass sum,
    the result is clamped to the rails, and a dry/wet mix (no added latency, so the dry path
    needs no delay) and an output level finish the stage.

    First pass: base-rate processing (no oversampling — the oversampling-and-crossover
    interaction is the plan's open risk, and 0 samples of latency are reported).
*/
class MultibandSaturationProcessor : public juce::AudioProcessor, public SuitePresets::StateHolder
{
public:
    // Parameter IDs are saved in sessions. Never rename or reuse one after a release.
    static constexpr const char* modeId         = "mode";
    static constexpr const char* crossoverId    = "crossover";
    static constexpr const char* driveLowId     = "driveLow";
    static constexpr const char* driveHighId    = "driveHigh";
    static constexpr const char* sensitivityId  = "sensitivity";
    static constexpr const char* attackId       = "attack";
    static constexpr const char* releaseId      = "release";
    static constexpr const char* toneLowId      = "toneLow";
    static constexpr const char* toneHighId     = "toneHigh";
    static constexpr const char* mixId          = "mix";
    static constexpr const char* outputLevelId  = "outputLevel";

    // LFO 1 and LFO 2. The IDs are fixed by suite_modulation, so every plugin saves them the same way.
    static constexpr const char* lfo1RateId      = "lfo1Rate";
    static constexpr const char* lfo1ShapeId     = "lfo1Shape";
    static constexpr const char* lfo1SyncId      = "lfo1Sync";
    static constexpr const char* lfo1DivisionId  = "lfo1Division";
    static constexpr const char* lfo1OffsetId    = "lfo1Offset";
    static constexpr const char* lfo2RateId      = "lfo2Rate";
    static constexpr const char* lfo2ShapeId     = "lfo2Shape";
    static constexpr const char* lfo2SyncId      = "lfo2Sync";
    static constexpr const char* lfo2DivisionId  = "lfo2Division";
    static constexpr const char* lfo2OffsetId    = "lfo2Offset";

    // Modulation routing: six slots, each with a source, a target, a signed amount and a delay.
    static constexpr const char* modSourceIds[6] = { "mod1Source", "mod2Source", "mod3Source", "mod4Source", "mod5Source", "mod6Source" };
    static constexpr const char* modTargetIds[6] = { "mod1Target", "mod2Target", "mod3Target", "mod4Target", "mod5Target", "mod6Target" };
    static constexpr const char* modAmountIds[6] = { "mod1Amount", "mod2Amount", "mod3Amount", "mod4Amount", "mod5Amount", "mod6Amount" };
    static constexpr const char* modDelayIds[6]  = { "mod1Delay",  "mod2Delay",  "mod3Delay",  "mod4Delay",  "mod5Delay",  "mod6Delay" };

    static constexpr int numControls = 57;   // 45 + 12 plan-17 Phase 7 LFO transforms

    /** The parameters that an LFO can move. The order is part of the saved session: append new
        targets at the end, and never reorder or remove existing ones. Names come from modTargetNames(). */
    enum class ModTarget
    {
        None = 0,
        Crossover,
        DriveLow,
        DriveHigh,
        Sensitivity,
        Attack,
        Release,
        ToneLow,
        ToneHigh,
        Mix,
        Output,
        count
    };

    static juce::StringArray modTargetNames();

    /** The shared LFO and routing module. */
    SuiteModulation::Router& getRouter() noexcept { return modulation; }

    /** The value a control shows right now: its base value moved by the published routing offset. */
    float getModulatedValue (ModTarget target) const;

    juce::AudioProcessorValueTreeState apvts;

    /** The visual snapshot (plan 11 D7): each band's envelope (the larger of the two
        channels). Audio thread, no locks. Plan 15: the per-channel peaks went with the
        editor's stereo meters. */
    struct BandSnapshot
    {
        std::atomic<float> envelope[2] {};   // [band]
    };

    const BandSnapshot& getBandSnapshot() const noexcept { return bandSnapshot; }

    // Bump stateVersion when the saved state changes, and add a migration from the old version.
    int stateVersion = 1;
    std::vector<SuiteState::Migration> migrations;

    MultibandSaturationProcessor();
    ~MultibandSaturationProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MultiSat"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
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
    juce::String getPresetPluginId() const override { return "MultibandSaturation"; }
    int getPresetStateVersion() const override { return stateVersion; }
    juce::AudioProcessorValueTreeState& getPresetStateTree() override { return apvts; }
    void applyPresetState (const juce::ValueTree&) override;

    // The preset identity (plan 09 1.4). Message thread. Persisted as the presetSelection state property, so it
    // survives the editor being reopened and rides in saved sessions.
    SuitePresets::PresetSelection getPresetSelection() const override { return lastPresetSelection; }
    void setPresetSelection (const SuitePresets::PresetSelection&) override;
    static constexpr const char* presetSelectionProperty { "presetSelection" };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    double preparedSampleRate = 44100.0;

    // One split per channel; two bands, each with its own follower and tone control per channel.
    std::array<CoreDSP::Multiband::TwoBandSplit, 2> splits;
    std::array<CoreDSP::Saturation::EnvelopeFollower, 4> envelopes;   // [band][channel]
    std::array<CoreDSP::Saturation::ToneControl, 4> tones;           // [band][channel]

    // Smoothed per block, so automation never zips (the Dynamic Saturation rule).
    juce::SmoothedValue<float> driveLowSmoother, driveHighSmoother;
    juce::SmoothedValue<float> toneLowSmoother, toneHighSmoother;
    juce::SmoothedValue<float> mixSmoother, outputGainSmoother;

    // Plan 13: the crossover frequency is smoothed before the LR4 coefficients are computed,
    // so modulating it per block does not click.
    juce::SmoothedValue<float> crossoverSmoother;

    std::atomic<float>* mode = nullptr;
    std::atomic<float>* crossover = nullptr;
    std::atomic<float>* driveLow = nullptr;
    std::atomic<float>* driveHigh = nullptr;
    std::atomic<float>* sensitivity = nullptr;
    std::atomic<float>* attack = nullptr;
    std::atomic<float>* release = nullptr;
    std::atomic<float>* toneLow = nullptr;
    std::atomic<float>* toneHigh = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* outputLevel = nullptr;

    // The visual snapshot (plan 11 D7). Audio thread, once per block, no locks.
    BandSnapshot bandSnapshot;

    // Declared after apvts, so the router can read the parameters it belongs to.
    SuiteModulation::Router modulation;

    // The block clock the LFOs run on. An effect has no sequencer, so the beat is counted here.
    std::int64_t streamSample = 0;
    double beatClock = 0.0;

    // Host program list (embedded bank + SUITE_PRESET_BANK_AS_PROGRAMS). -1 means a state that is not a
    // factory program, for example a user preset.
    std::atomic<int> currentProgramIndex { 0 };

    // The preset identity (plan 09 1.4). Message thread only.
    SuitePresets::PresetSelection lastPresetSelection;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultibandSaturationProcessor)
};