#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <atomic>
#include <cstddef>
#include <memory>
#include <vector>

#include "SuiteState.h"
#include "SuitePresets.h"

#include "SampleLoader.h"

#include <CoreDSP.h>

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

    static constexpr int numControls = 37;

    //==============================================================================
    SgraffitoProcessor();
    ~SgraffitoProcessor() override = default;

    //==============================================================================
    // StateHolder
    juce::String getPresetPluginId() const override { return "Sgraffito"; }
    int getPresetStateVersion() const override { return stateVersion; }
    juce::AudioProcessorValueTreeState& getPresetStateTree() override { return apvts; }
    void applyPresetState (const juce::ValueTree&) override;

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

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Voice access for the editor and modulation
    int getNumActiveVoices() const noexcept { return voicePool.countActive(); }

    // The current sample buffer (atomic swap for thread safety).
    const juce::AudioBuffer<float>* getSampleBuffer() const noexcept
    {
        return sampleBuffer.load (std::memory_order_acquire);
    }

    // Sample loading
    void loadSample (const juce::File& file);
    void clearSample();

    juce::String getSamplePath() const noexcept { return sampleFile.getFullPathName(); }
    bool hasSample() const noexcept { return sampleBuffer.load() != nullptr; }
    size_t getEmbedLimitBytes() const noexcept { return embedLimitBytes; }
    void setEmbedLimitBytes (size_t bytes) noexcept { embedLimitBytes = bytes; }

    // APVTS is public so the editor and tests can build attachments.
    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "PARAMETERS", createParameterLayout() };

    // Voice pool (public for tests).
    CoreDSP::Sampler::VoicePool voicePool;

    // Lorenz attractor for chaos drift.
    CoreDSP::Chaos::LorenzAttractor lorenz;

    // The loaded sample (public for tests).
    std::unique_ptr<LoadedSample> loadedSample;

    // The loaded sample buffer (atomic swap for thread safety).
    std::atomic<const juce::AudioBuffer<float>*> sampleBuffer { nullptr };

    // The original file path (for path-based loading above the embed limit).
    juce::File sampleFile;

    // Maximum size for embedded samples (default 20 MB).
    size_t embedLimitBytes = 20ull * 1024ull * 1024ull;

    // Per-sample-rate derived values.
    double preparedSampleRate = 44100.0;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

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

    int stateVersion = 1;
    std::vector<SuiteState::Migration> migrations;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SgraffitoProcessor)
};
