#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

#include "CoreDSP.h"
#include "SuiteModulationRouter.h"

/**
    Dynamic Saturation Companion.

    A saturation stage whose drive follows the signal envelope: the louder the input,
    the harder the stage is pushed, scaled by Sensitivity. The shaping runs at 2x
    oversampling to reduce aliasing. Its latency is reported to the host.
*/
class DynamicSaturationProcessor : public juce::AudioProcessor
{
public:
    // Core controls
    static constexpr const char* driveId = "drive";
    static constexpr const char* sensitivityId = "sensitivity";
    static constexpr const char* modeId = "mode";
    static constexpr const char* mixId = "mix";
    static constexpr const char* outputLevelId = "outputLevel";

    // Extended controls
    static constexpr const char* attackId = "attack";
    static constexpr const char* releaseId = "release";
    static constexpr const char* detectHpfId = "detectHpf";
    static constexpr const char* stereoLinkId = "stereoLink";
    static constexpr const char* biasId = "bias";
    static constexpr const char* foldId = "fold";
    static constexpr const char* toneId = "tone";
    static constexpr const char* curveId = "curve";

    // LFO 1 and LFO 2. The IDs are fixed by suite_modulation, so every plugin saves them the same way.
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

    // Modulation routing: six slots, each with a source, a target, a signed amount and a delay
    static constexpr const char* modSourceIds[6] = { "mod1Source", "mod2Source", "mod3Source", "mod4Source", "mod5Source", "mod6Source" };
    static constexpr const char* modTargetIds[6] = { "mod1Target", "mod2Target", "mod3Target", "mod4Target", "mod5Target", "mod6Target" };
    static constexpr const char* modAmountIds[6] = { "mod1Amount", "mod2Amount", "mod3Amount", "mod4Amount", "mod5Amount", "mod6Amount" };
    static constexpr const char* modDelayIds[6] = { "mod1Delay", "mod2Delay", "mod3Delay", "mod4Delay", "mod5Delay", "mod6Delay" };

    static constexpr int numControls = 47;
    static constexpr int lfoCount = 2;
    static constexpr int slotCount = 6;

    /**
        The parameters that an LFO can move. The order is part of the saved session: append new
        targets at the end, and never reorder or remove existing ones. Names come from modTargetNames().
    */
    enum class ModTarget
    {
        None = 0,
        Drive,
        Sensitivity,
        Mix,
        OutputLevel,
        Attack,
        Release,
        DetectHpf,
        StereoLink,
        Bias,
        Fold,
        Tone,
        Curve,
        count
    };

    static juce::StringArray modTargetNames();

    DynamicSaturationProcessor();
    ~DynamicSaturationProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Dynamic Saturation Companion"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    /** Linked detector level (linear), published for the editor's meter. */
    float getEnvelopeLevel() const noexcept { return envelopeLevel.load (std::memory_order_relaxed); }

    /** Latency added by the oversampler, in samples. */
    int getOversamplingLatency() const noexcept { return latencySamples; }

    /** The shared LFO and routing module. */
    SuiteModulation::Router& getRouter() noexcept { return modulation; }

    /** The value a control shows right now: its base value moved by the published routing offset. */
    float getModulatedValue (ModTarget target) const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    static constexpr int maxChannels = 2;

    juce::dsp::Oversampling<float> oversampling { maxChannels, 1,
                                                  juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
                                                  false, true };
    int latencySamples = 0;

    CoreDSP::Saturation::EnvelopeFollower linkedEnvelope;
    std::array<CoreDSP::Saturation::EnvelopeFollower, maxChannels> channelEnvelope;
    std::array<CoreDSP::Saturation::OnePoleFilter, maxChannels> detector;
    std::array<CoreDSP::Saturation::ToneControl, maxChannels> toneControls;

    juce::SmoothedValue<float> driveGain;
    juce::SmoothedValue<float> mixAmount;
    juce::SmoothedValue<float> outputGain;

    // Per-block scratch space, sized in prepareToPlay so the audio thread does not allocate.
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> preGainBuffer;
    std::vector<float> mixCurve;
    std::vector<float> outputCurve;

    // Dry path delayed by the oversampler latency, so the mix stays aligned.
    std::array<std::vector<float>, maxChannels> dryDelay;
    int dryDelayPosition = 0;

    std::atomic<float>* drive = nullptr;
    std::atomic<float>* sensitivity = nullptr;
    std::atomic<float>* mode = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* outputLevel = nullptr;
    std::atomic<float>* attack = nullptr;
    std::atomic<float>* release = nullptr;
    std::atomic<float>* detectHpf = nullptr;
    std::atomic<float>* stereoLink = nullptr;
    std::atomic<float>* bias = nullptr;
    std::atomic<float>* fold = nullptr;
    std::atomic<float>* tone = nullptr;
    std::atomic<float>* curve = nullptr;

    std::atomic<float> envelopeLevel { 0.0f };

    // Declared after apvts, so the router can read the parameters it belongs to.
    SuiteModulation::Router modulation;

    // The block clock the LFOs run on. An effect has no sequencer, so the beat is counted here.
    // The rate is the one prepareToPlay was given: getSampleRate() is 0 in a bare test harness.
    double preparedSampleRate = 44100.0;
    std::int64_t streamSample = 0;
    double beatClock = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynamicSaturationProcessor)
};
