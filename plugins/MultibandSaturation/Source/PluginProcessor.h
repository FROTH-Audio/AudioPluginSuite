#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <vector>

#include "MultibandEngine.h"
#include "SaturationEngine.h"
#include "SuiteState.h"

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
class MultibandSaturationProcessor : public juce::AudioProcessor
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

    static constexpr int numControls = 11;

    juce::AudioProcessorValueTreeState apvts;

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

    const juce::String getName() const override { return "Multiband Saturation"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

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
};
