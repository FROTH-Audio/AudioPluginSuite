#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <vector>

#include "SuiteState.h"

/**
    __PRODUCT__.

    Starting point for a new plugin: one gain control, versioned saved state, and the suite's shared editor.
    Replace the processing and the parameters with the plugin's own.
*/
class __NAME__Processor : public juce::AudioProcessor
{
public:
    // Parameter IDs are saved in sessions. Never rename or reuse one after a release.
    static constexpr const char* gainId = "gain";

    juce::AudioProcessorValueTreeState apvts;

    // Bump stateVersion when the saved state changes, and add a migration from the old version.
    int stateVersion = 1;
    std::vector<SuiteState::Migration> migrations;

    __NAME__Processor();
    ~__NAME__Processor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "__PRODUCT__"; }
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

    std::atomic<float>* gain = nullptr;
};
