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
        GhostThreshold,
        GhostLevel,
        GrainSize,
        GrainDensity,
        SwellTime,
        Dust,
        Crackle,
        Wow,
        Flutter,
        ShuffleSeed,
        count
    };

    static juce::StringArray modTargetNames();

    /** Modulation routing targets - declared here so they're available in the constructor initializer. */
    static constexpr SuiteModulation::TargetInfo modulationTargets[] = {
        { "regionSpan",     "Region Span" },
        { "regionLength",   "Region Length" },
        { "rootKey",        "Root Key",   true },
        { "pitchFine",      "Pitch Fine" },
        { "glide",          "Glide" },
        { "level",          "Level" },
        { "attack",         "Attack" },
        { "release",        "Release" },
        { "memoryAmount",   "Memory Amount" },
        { "driftAmount",    "Drift Amount" },
        { "driftSpeed",     "Drift Speed" },
        { "layBack",        "Lay Back" },
        { "swing",          "Swing" },
        { "humanise",       "Humanise" },
        { "ghostThreshold", "Ghost Threshold" },
        { "ghostLevel",     "Ghost Level" },
        { "grainSize",      "Grain Size" },
        { "grainDensity",   "Grain Density" },
        { "swellTime",      "Swell Time" },
        { "dust",           "Dust" },
        { "crackle",        "Crackle" },
        { "wow",            "Wow" },
        { "flutter",        "Flutter" },
        { "shuffleSeed",    "Shuffle Seed", true },
    };

    static constexpr int numModulationTargets = sizeof(modulationTargets) / sizeof(SuiteModulation::TargetInfo);

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
    // const juce::AudioBuffer<float>* getSampleBuffer() const noexcept;  // inline below
    // int getNumActiveVoices() const noexcept;                           // inline below

    // Sample loading
    void loadSample (const juce::File& file);
    void loadEmbeddedSample (const juce::String& embeddedPath);  // e.g., "embedded:grain_4k.wav"
    void clearSample();

    juce::String getSamplePath() const noexcept { return sampleFile.getFullPathName(); }
    bool hasSample() const noexcept { return sampleBuffer.load() != nullptr; }
    size_t getEmbedLimitBytes() const noexcept { return embedLimitBytes; }
    void setEmbedLimitBytes (size_t bytes) noexcept { embedLimitBytes = bytes; }

    /** The raw value of a parameter, for the editor's visual widgets (waveform, key map). */
    float getParamValue (const char* id) const
    {
        if (auto* parameter = apvts.getRawParameterValue (id))
            return parameter->load();
        return 0.0f;
    }

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

    // The loaded sample buffer (atomic swap for thread safety).
    std::atomic<const juce::AudioBuffer<float>*> sampleBuffer { nullptr };

    // The original file path (for path-based loading above the embed limit).
    juce::File sampleFile;

    // Maximum size for embedded samples (default 20 MB).
    size_t embedLimitBytes = 20ull * 1024ull * 1024ull;

    // Pending note-ons for lay back delay.
    struct PendingNote
    {
        int key = 60;
        float velocity = 1.0f;
        double samplePosition = 0.0;
    };
    std::vector<PendingNote> pendingNotes;

    // Per-sample-rate derived values.
    double preparedSampleRate = 44100.0;

    // Free-LFO clock: stream position in samples and a tempo-locked beat clock (see SuiteModulation).
    std::int64_t streamSample = 0;
    double beatClock = 0.0;

private:
    /** The current sample buffer (atomic swap for thread safety). */
    const juce::AudioBuffer<float>* getSampleBuffer() const noexcept
    {
        return sampleBuffer.load(std::memory_order_acquire);
    }
    
    int getNumActiveVoices() const noexcept { return voicePool.countActive(); }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    /** Applies an offset to a parameter's base value, in the parameter's own normalised range. Exact when the offset is zero. */
    float modulatedValue (ModTarget target, float base, const SuiteModulation::Router::Offsets& offsets) const
    {
        return modulation.modulate ((int)target, base, offsets);
    }

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SgraffitoProcessor)
};
