#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <vector>
#include <array>

#include "SuiteState.h"

/**
    Stereo Tool Effect - mid/side encoding, width control, tanh saturation.

    Processing pipeline:
    1. Split to mid/side (MS matrix)
    2. Apply tanh saturation per channel
    3. Width control (correlated/uncorrelated balance)
    4. Recompose to stereo
    5. Dry/wet mix with latency-aligned dry signal
*/
class StereoToolProcessor : public juce::AudioProcessor
{
public:
    // Parameter IDs are saved in sessions. Never rename or reuse one after a release.
    static constexpr const char* widthId = "width";
    static constexpr const char* mixId   = "mix";
    static constexpr const char* driveId = "drive";

    StereoToolProcessor();
    ~StereoToolProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "StereoTool"; }
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

    // Width in % (-100 to +100, where 0 = correlated stereo)
    // +100 = fully uncorrelated, -100 = mono (both channels identical)
    float width{0.0f};
    float mix{1.0f};      // dry/wet mix in % (0-100)
    float drive{0.0f};    // saturation drive in dB

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Mid/side processing buffers
    juce::AudioBuffer<float> msBuffer; // temp buffer for MS processing
    float lastLeft{0}, lastRight{0}; // for latency alignment

    // Parameter pointers
    std::atomic<float>* widthParam{nullptr};
    std::atomic<float>* mixParam{nullptr};
    std::atomic<float>* driveParam{nullptr};
};
