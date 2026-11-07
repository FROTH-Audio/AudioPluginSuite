#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

#include "SpectralEngine.h"
#include "SuiteModulationRouter.h"

/**
    Spectral Drift Shifter.

    FFT overlap-add processing with an LFO that drifts the phase of every bin, plus spectral
    shaping (shift, tilt, gate). The channels run on LFOs offset by the Stereo Phase control,
    which decorrelates them and widens the image.
*/
class SpectralDriftProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numBins = CoreDSP::Spectral::numBins;

    /** Spectra published for the editor. Written on the audio thread, read on the message thread. */
    struct SpectrumSnapshot
    {
        std::array<std::atomic<float>, numBins> input {};
        std::array<std::atomic<float>, numBins> output {};
        std::atomic<std::uint64_t> frames { 0 };
    };

    // Drift and mix
    static constexpr const char* driftRateId = "driftRate";
    static constexpr const char* depthId = "depth";
    static constexpr const char* spreadId = "spread";
    static constexpr const char* stereoPhaseId = "stereoPhase";
    static constexpr const char* lfoShapeId = "lfoShape";

    // Spectral shaping
    static constexpr const char* shiftId = "shift";
    static constexpr const char* tiltId = "tilt";
    static constexpr const char* gateId = "gate";

    // Feedback and freeze
    static constexpr const char* feedbackId = "feedback";
    static constexpr const char* mixId = "mix";
    static constexpr const char* freezeId = "freeze";
    static constexpr const char* feedbackDampingId = "feedbackDamping";
    static constexpr const char* driftStartId = "driftStart";
    static constexpr const char* outputGainId = "outputGain";

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

    static constexpr int numControls = 48;

    /** Largest feedback the plugin allows. The engine itself can go to 0.95. */
    static constexpr float maxFeedbackPercent = 12.0f;

    /**
        The parameters that an LFO can move. The order is part of the saved session: append new
        targets at the end, and never reorder or remove existing ones. Names come from modTargetNames().
    */
    enum class ModTarget
    {
        None = 0,
        DriftRate,
        Depth,
        BinSpread,
        StereoPhase,
        Shift,
        Tilt,
        Gate,
        Feedback,
        FeedbackDamping,
        DriftStart,
        OutputGain,
        Mix,
        count
    };

    static juce::StringArray modTargetNames();

    /** The shared LFO and routing module. */
    SuiteModulation::Router& getRouter() noexcept { return modulation; }

    /** The value a control shows right now: its base value moved by the published routing offset. */
    float getModulatedValue (ModTarget target) const;

    juce::AudioProcessorValueTreeState apvts;

    SpectralDriftProcessor();
    ~SpectralDriftProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Spectral Drift Shifter"; }
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

    const SpectrumSnapshot& getSpectrumSnapshot() const noexcept { return snapshot; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    /** Reads one block's settings, applying per-target modulation. mix/feedback/damping are left at
        their raw values here and are smoothed per-sample in processBlock. */
    void readSettings (CoreDSP::Spectral::DriftEngine::Settings& settings,
                       const SuiteModulation::Router::Offsets& offsets) const noexcept;

    /** The host tempo if it reports one, otherwise a fixed 120 BPM. Drives the synced LFOs. */
    double currentTempo() const;

    // The rate is the one prepareToPlay was given: getSampleRate() is 0 in a bare test harness.
    double preparedSampleRate = 44100.0;

    std::array<CoreDSP::Spectral::DriftEngine, 2> engines;

    std::atomic<float>* driftRate = nullptr;
    std::atomic<float>* depth = nullptr;
    std::atomic<float>* spread = nullptr;
    std::atomic<float>* stereoPhase = nullptr;
    std::atomic<float>* lfoShape = nullptr;
    std::atomic<float>* shift = nullptr;
    std::atomic<float>* tilt = nullptr;
    std::atomic<float>* gate = nullptr;
    std::atomic<float>* feedback = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* freeze = nullptr;
    std::atomic<float>* feedbackDamping = nullptr;
    std::atomic<float>* driftStart = nullptr;
    std::atomic<float>* outputGain = nullptr;

    SpectrumSnapshot snapshot;

    // Declared after apvts, so the router can read the parameters it belongs to.
    SuiteModulation::Router modulation;

    // The block clock the LFOs run on. An effect has no sequencer, so the beat is counted here.
    std::int64_t streamSample = 0;
    double beatClock = 0.0;

    // Smoothed so modulating these per block never zips or clicks.
    juce::SmoothedValue<float> mixSmoother;
    juce::SmoothedValue<float> feedbackSmoother;
    juce::SmoothedValue<float> dampingSmoother;
    juce::SmoothedValue<float> outputGainSmoother;

    // One block of the smoothed values, filled once per block so both channels read the same ramp.
    // Sized in prepareToPlay and grown in processBlock if a host sends a bigger block.
    std::vector<float> mixCurve, feedbackCurve, dampingCurve, gainCurve;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectralDriftProcessor)
};
