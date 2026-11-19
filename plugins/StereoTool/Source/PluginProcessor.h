#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <atomic>
#include <vector>

#include "SaturationEngine.h"   // CoreDSP::EnvelopeFollower, the transient detector
#include "SuitePresets.h"   // StateHolder interface for presets
#include "SuiteState.h"

/**
    Stereo Tool Effect (plan 06 idea 2.9, extended by plan 6.1).

    Pipeline per block:
    1. Mid/side split (CoreDSP::Stereo::encode).
    2. Saturation on mid and side with the selected character. Tube is the original tanh
       drive (transparent at Drive 0 dB: negative Drive is a clean gain reduction, positive
       Drive interpolates towards the saturator); Tape, Diode, Console and Clip share the
       same blend structure with a different curve (plan 6.1).
    3. Optional side filter (plan 6.1): a state-variable filter on the side component only,
       so the lows can be kept mono while the highs stay wide. Off is a true bypass.
    4. Mid focus (plan 6.1): midGain = 1 + f, sideGain = 1 - f, so positive values pull the
       centre forward and negative values widen the image.
    5. Width: the side component is scaled by CoreDSP::Stereo::sideGain (width/100), so a
       width of 0 leaves the image unchanged, -100 folds to mono, +100 doubles the side.
    6. Optional transient follower (plan 6.1): a peak envelope over the linked input moves
       the width and/or the drive on transients; Amount 0 bypasses it entirely.
    7. Mid/side decode back to left/right.
    8. Dry/wet mix against the unprocessed input.

    An identity early-return keeps the plugin bit-exact at the defaults (character Tube,
    transient amount 0, mid focus 0, side filter Off, drive 0, width 0, mix 1), where the
    wet path is mathematically the input. No latency is added, so getLatencySamples stays 0.
*/
class StereoToolProcessor : public juce::AudioProcessor, public SuitePresets::StateHolder
{
public:
    // Parameter IDs are saved in sessions. Never rename or reuse one after a release.
    static constexpr const char* widthId = "width";
    static constexpr const char* driveId = "drive";
    static constexpr const char* mixId   = "mix";

    // Plan 6.1, appended after the originals so no saved parameter index moves.
    static constexpr const char* characterId        = "character";
    static constexpr const char* transientAttackId  = "transientAttack";
    static constexpr const char* transientReleaseId = "transientRelease";
    static constexpr const char* transientAmountId  = "transientAmount";
    static constexpr const char* transientTargetId  = "transientTarget";
    static constexpr const char* midFocusId         = "midFocus";
    static constexpr const char* sideFilterId       = "sideFilter";
    static constexpr const char* sideCrossoverId    = "sideCrossover";

    /** How hard a transient can push the drive, in dB, at amount 1 and a full envelope. */
    static constexpr float maxTransientDriveBoostDb = 12.0f;

    juce::AudioProcessorValueTreeState apvts;

    /** The visual snapshot (plan 11 D7): the mid/side RMS of the last block, the transient
        follower's envelope (0 while the amount is 0) and the input/output peaks. Audio
        thread, no locks. */
    struct MeterSnapshot
    {
        std::atomic<float> midLevel {};
        std::atomic<float> sideLevel {};
        std::atomic<float> followerEnvelope {};
        std::atomic<float> inputPeak {};
        std::atomic<float> outputPeak {};
    };

    const MeterSnapshot& getMeterSnapshot() const noexcept { return meterSnapshot; }

    // Bump stateVersion when the saved state changes, and add a migration from the old version.
    // Plan 6.1 is additive only: no ID moved, no range changed meaning, so the version stays 1
    // and an old state loads with the new parameters at their defaults.
    int stateVersion = 1;
    std::vector<SuiteState::Migration> migrations;

    StereoToolProcessor();
    ~StereoToolProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Stereo Tool Effect"; }
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

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Preset system: StateHolder interface
    juce::String getPresetPluginId() const override { return "StereoTool"; }
    int getPresetStateVersion() const override { return stateVersion; }
    juce::AudioProcessorValueTreeState& getPresetStateTree() override { return apvts; }
    void applyPresetState (const juce::ValueTree&) override;

    // The preset identity (plan 09 1.4): which factory preset or user file the bar last
    // applied. Message thread. Persisted as the presetSelection state property, so it
    // survives the editor being reopened and rides in saved sessions.
    SuitePresets::PresetSelection getPresetSelection() const override { return lastPresetSelection; }
    void setPresetSelection (const SuitePresets::PresetSelection&) override;
    static constexpr const char* presetSelectionProperty { "presetSelection" };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Scratch buffer for the dry signal, sized in prepareToPlay. The audio thread never allocates.
    juce::AudioBuffer<float> dryBuffer;

    // The transient follower (plan 6.1): a peak envelope over the linked input.
    CoreDSP::Saturation::EnvelopeFollower transientEnv;

    // The rate is the one prepareToPlay was given: getSampleRate() is 0 in a bare test harness.
    double preparedSampleRate = 44100.0;

    // The side filter (plan 6.1): a state-variable filter on the side component only.
    juce::dsp::StateVariableTPTFilter<float> sideFilter;

    std::atomic<float>* widthParam = nullptr;
    std::atomic<float>* driveParam = nullptr;
    std::atomic<float>* mixParam   = nullptr;
    std::atomic<float>* characterParam        = nullptr;
    std::atomic<float>* transientAttackParam  = nullptr;
    std::atomic<float>* transientReleaseParam = nullptr;
    std::atomic<float>* transientAmountParam  = nullptr;
    std::atomic<float>* transientTargetParam  = nullptr;
    std::atomic<float>* midFocusParam         = nullptr;
    std::atomic<float>* sideFilterParam       = nullptr;
    std::atomic<float>* sideCrossoverParam    = nullptr;

    // The visual snapshot (plan 11 D7). Audio thread, once per block, no locks.
    MeterSnapshot meterSnapshot;

    // Host program list (embedded bank + SUITE_PRESET_BANK_AS_PROGRAMS). -1 means a state that is not a
    // factory program, for example a user preset.
    std::atomic<int> currentProgramIndex { 0 };

    // The preset identity (plan 09 1.4). Message thread only.
    SuitePresets::PresetSelection lastPresetSelection;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StereoToolProcessor)
};
