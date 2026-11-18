#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <vector>

#include "SuiteState.h"

/**
    Stereo Tool Effect (plan 06 idea 2.9).

    Pipeline per block:
    1. Mid/side split (CoreDSP::Stereo::encode).
    2. Tanh saturation on mid and side (transparent at Drive 0 dB: negative Drive is a clean
       gain reduction, positive Drive interpolates towards the saturator).
    3. Width: the side component is scaled by CoreDSP::Stereo::sideGain (width/100), so a
       width of 0 leaves the image unchanged, -100 folds to mono, +100 doubles the side.
    4. Mid/side decode back to left/right.
    5. Dry/wet mix against the unprocessed input.

    No latency is added, so getLatencySamples stays 0.
*/
class StereoToolProcessor : public juce::AudioProcessor
{
public:
    // Parameter IDs are saved in sessions. Never rename or reuse one after a release.
    static constexpr const char* widthId = "width";
    static constexpr const char* driveId = "drive";
    static constexpr const char* mixId   = "mix";

    juce::AudioProcessorValueTreeState apvts;

    // Bump stateVersion when the saved state changes, and add a migration from the old version.
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

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Scratch buffer for the dry signal, sized in prepareToPlay. The audio thread never allocates.
    juce::AudioBuffer<float> dryBuffer;

    std::atomic<float>* widthParam = nullptr;
    std::atomic<float>* driveParam = nullptr;
    std::atomic<float>* mixParam   = nullptr;
};
