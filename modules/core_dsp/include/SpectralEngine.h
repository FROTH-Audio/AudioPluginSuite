#pragma once

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>

/**
    FFT overlap-add engine with LFO-driven spectral phase drift and spectral shaping.

    Signal path per channel:
      input -> Hann analysis -> FFT (2048) -> [freeze? hold : analyse magnitudes and phases]
            -> spectral shaping of magnitudes (shift, tilt, gate)
            -> per-bin phase drift from an LFO -> Hermitian IFFT -> Hann synthesis -> overlap-add
            -> feedback and dry/wet mix

    The engine is a streaming processor: samples go in one at a time and come out
    getLatencySamples() later. Drift changes phases only, so the audible effect comes from
    phase relationships between overlapping frames. Shift, tilt and gate change magnitudes.
*/
namespace CoreDSP::Spectral
{
    constexpr int frameOrder = 11;
    constexpr int frameSize = 1 << frameOrder;     // 2048 samples
    constexpr int hopSize = frameSize / 4;         // 75 % overlap
    constexpr int numBins = frameSize / 2 + 1;     // DC .. Nyquist

    constexpr float defaultDriftDepth = 0.6f;      // radians of phase swing at the LFO peak
    constexpr float defaultBinSpread = 0.05f;      // radians between the LFO phases of adjacent bins
    constexpr float gateOffDb = -90.0f;            // at or below this the gate is open for every bin

    namespace detail
    {
        constexpr float twoPi = 6.28318530717958647692f;

        inline float wrapPhase (float phase) noexcept
        {
            phase = std::fmod (phase, twoPi);
            return phase < 0.0f ? phase + twoPi : phase;
        }
    }

    /** Shape of the drift LFO. All shapes lie in [-1, 1]. */
    enum class LfoShape
    {
        sine = 0,
        triangle,
        saw,
        square
    };

    /** Evaluates the LFO at a phase in radians. The phase may be outside [0, 2pi). */
    inline float lfoValue (LfoShape shape, float phase) noexcept
    {
        const float p = detail::wrapPhase (phase);

        switch (shape)
        {
            case LfoShape::sine:     return std::sin (p);
            case LfoShape::triangle: return 1.0f - 4.0f * std::abs (p / detail::twoPi - 0.5f);
            case LfoShape::saw:      return 2.0f * p / detail::twoPi - 1.0f;
            case LfoShape::square:   return p < juce::MathConstants<float>::pi ? 1.0f : -1.0f;
        }

        return 0.0f;
    }

    class DriftEngine
    {
    public:
        struct Settings
        {
            float driftDepth = defaultDriftDepth;      // radians
            float binSpread = defaultBinSpread;        // hop-steps of drift lag between neighbouring bins
            float stereoPhase = 0.0f;                  // radians; as extra hop lag for this engine (right channel)
            float driftValue = 0.0f;                   // -1 .. 1: the drift source for this frame (from the routed LFO)
            float shiftBins = 0.0f;                    // -16 .. +16 bins, positive moves content up
            float tilt = 0.0f;                         // -1 .. +1, negative darkens, positive brightens
            float gateDb = gateOffDb;                  // bins quieter than this (relative to full scale) are removed
            float feedback = 0.0f;                     // 0 .. 0.95 (the plugin exposes a much smaller range)
            float feedbackDamping = 0.0f;              // 0 (bright feedback) .. 1 (strongly darkened feedback)
            float driftStart = 0.0f;                   // 0 .. 1: the lowest fraction of bins that is left undrifted
            float mix = 1.0f;                          // 0 (dry) .. 1 (wet)
            bool frozen = false;                       // hold the captured spectrum
        };

        DriftEngine();

        /** Sets the sample rate and clears all state. */
        void prepare (double newSampleRate) noexcept;

        /** Updates the processing settings. Safe to call once per block. */
        void setSettings (const Settings& newSettings) noexcept { settings = newSettings; }

        /** Per-sample values that change at block rate and need smoothing (mix, feedback, damping). */
        struct PerSampleSettings
        {
            float mix = 1.0f;               // 0 (dry) .. 1 (wet)
            float feedback = 0.0f;          // 0 .. 0.95 (the plugin exposes a much smaller range)
            float feedbackDamping = 0.0f;   // 0 (bright feedback) .. 1 (strongly darkened feedback)
        };

        /** Processes one sample and returns the sample that entered getLatencySamples() ago. */
        float processSample (float input) noexcept;

        /** Processes one sample with per-sample mix, feedback and feedback damping. The smoother
            applies these once per block; everything else (drift value, depth, shift, ...) comes from
            `settings`, which is set once per block. */
        float processSample (float input, const PerSampleSettings& p) noexcept;

        /** The current settings. The tests feed the drift value through them per sample. */
        const Settings& settingsForTest() const noexcept { return settings; }

        /** Linear magnitudes of the most recent input frame (not updated while frozen). */
        const std::array<float, numBins>& getInputMagnitudes() const noexcept { return inputMagnitudes; }

        /** Linear magnitudes of the most recent output frame, measured on the overlap-added signal. */
        const std::array<float, numBins>& getOutputMagnitudes() const noexcept { return outputMagnitudes; }

        /** Number of frames processed so far. The UI uses it to tell that audio has arrived. */
        std::uint64_t getFrameCount() const noexcept { return frameCount; }

        static constexpr int getLatencySamples() noexcept { return frameSize; }

    private:
        void processFrame() noexcept;
        void shapeMagnitudes() noexcept;

        float feedbackState = 0.0f;
        double sampleRate = 44100.0;
        Settings settings;
        std::uint64_t frameCount = 0;

        // The drift values of the past frames: bin k reads the value from k * binSpread
        // frames ago, which is how the drift smears across the bins.
        std::array<float, numBins> driftHistory {};
        int driftHistoryPos = 0;
        bool driftHistoryFilled = false;

        juce::dsp::FFT fft { frameOrder };
        juce::dsp::WindowingFunction<float> window { frameSize, juce::dsp::WindowingFunction<float>::hann, false };

        // Inverse of the sum of squared windows for each hop phase, so overlap-add is unity gain.
        std::array<float, hopSize> invOverlapGain {};

        // Circular buffers indexed by absolute sample number modulo frameSize.
        // Slot ringPos always holds the sample that is frameSize samples old.
        std::array<float, frameSize> inputRing {};
        std::array<float, frameSize> dryRing {};
        std::array<float, frameSize> wetRing {};
        std::array<float, frameSize> outputAccumulator {};

        std::array<float, frameSize> frameBuffer {};

        // stereoPhase maps to at most this many frames of extra drift lag for this engine.
        static constexpr int stereoLagFrames = 16;
        std::array<std::complex<float>, frameSize> fftInput {};
        std::array<std::complex<float>, frameSize> fftOutput {};
        std::array<std::complex<float>, frameSize> spectrum {};

        std::array<float, numBins> magnitudes {};
        std::array<float, numBins> shapedMagnitudes {};
        std::array<float, numBins> phases {};
        std::array<float, numBins> inputMagnitudes {};
        std::array<float, numBins> outputMagnitudes {};

        int ringPos = 0;
        int hopIndex = 0;
    };

    inline DriftEngine::DriftEngine()
    {
        // Each output sample is covered by frameSize / hopSize windows. Measure the sum of squared
        // windows directly, so the gain is exact for any window shape.
        std::array<float, frameSize> probe;
        probe.fill (1.0f);
        window.multiplyWithWindowingTable (probe.data(), frameSize);

        for (int r = 0; r < hopSize; ++r)
        {
            float sum = 0.0f;

            for (int j = r; j < frameSize; j += hopSize)
                sum += probe[(size_t) j] * probe[(size_t) j];

            invOverlapGain[(size_t) r] = 1.0f / sum;
        }
    }

    inline void DriftEngine::prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        feedbackState = 0.0f;
        ringPos = 0;
        hopIndex = 0;
        frameCount = 0;

        driftHistory.fill (0.0f);
        driftHistoryPos = 0;
        driftHistoryFilled = false;

        inputRing.fill (0.0f);
        dryRing.fill (0.0f);
        wetRing.fill (0.0f);
        outputAccumulator.fill (0.0f);
        magnitudes.fill (0.0f);
        shapedMagnitudes.fill (0.0f);
        phases.fill (0.0f);
        inputMagnitudes.fill (0.0f);
        outputMagnitudes.fill (0.0f);
    }

    inline float DriftEngine::processSample (float input) noexcept
    {
        return processSample (input, { settings.mix, settings.feedback, settings.feedbackDamping });
    }

    inline float DriftEngine::processSample (float input, const PerSampleSettings& p) noexcept
    {
        // The slot at ringPos holds index n - frameSize. Every frame that covers that index has already
        // been added, so the sample is final and can be emitted now.
        const float wet = outputAccumulator[(size_t) ringPos] * invOverlapGain[(size_t) hopIndex];
        outputAccumulator[(size_t) ringPos] = 0.0f;
        wetRing[(size_t) ringPos] = wet;

        const float dry = dryRing[(size_t) ringPos];
        dryRing[(size_t) ringPos] = input;

        // Feedback is taken from the bounded wet signal so the loop cannot run away. Damping is a one-pole
        // low-pass on the loop: at 0 it passes the signal unchanged, and higher values darken each repeat.
        const float dampedCoefficient = 1.0f - 0.9f * p.feedbackDamping;
        feedbackState += dampedCoefficient * (std::tanh (wet) - feedbackState);
        inputRing[(size_t) ringPos] = input + p.feedback * feedbackState;

        ringPos = (ringPos + 1) % frameSize;
        hopIndex = (hopIndex + 1) % hopSize;

        if (hopIndex == 0)
            processFrame();

        return dry + p.mix * (wet - dry);
    }

    inline void DriftEngine::shapeMagnitudes() noexcept
    {
        const bool tiltActive = std::abs (settings.tilt) > 1.0e-6f;
        const bool gateActive = settings.gateDb > gateOffDb;

        // Gate threshold relative to a full-scale sine, whose Hann-windowed peak is frameSize / 4.
        const float gateLinear = gateActive ? std::pow (10.0f, settings.gateDb / 20.0f) * (frameSize / 4.0f) : 0.0f;

        for (int k = 0; k < numBins; ++k)
        {
            // Shifting by s bins reads the magnitude at k - s, interpolating between bins.
            const float source = (float) k - settings.shiftBins;
            float value = 0.0f;

            if (source >= 0.0f && source <= (float) (numBins - 1))
            {
                const int j0 = (int) source;
                const float frac = source - (float) j0;
                const float m0 = magnitudes[(size_t) j0];
                const float m1 = j0 + 1 < numBins ? magnitudes[(size_t) (j0 + 1)] : 0.0f;
                value = (1.0f - frac) * m0 + frac * m1;
            }

            if (tiltActive && k > 0)
            {
                // Power law around bin 32 (about 700 Hz at 48 kHz), clamped so extreme tilts stay usable.
                const float gain = std::pow ((float) k / 32.0f, settings.tilt);
                value *= juce::jlimit (0.0625f, 8.0f, gain);
            }

            if (gateActive && value < gateLinear)
                value = 0.0f;

            shapedMagnitudes[(size_t) k] = value;
        }
    }

    inline void DriftEngine::processFrame() noexcept
    {
        // The oldest sample in the input ring starts the frame.
        const int start = ringPos;

        if (! settings.frozen)
        {
            for (int i = 0; i < frameSize; ++i)
                frameBuffer[(size_t) i] = inputRing[(size_t) ((start + i) % frameSize)];

            window.multiplyWithWindowingTable (frameBuffer.data(), frameSize);

            for (int i = 0; i < frameSize; ++i)
                fftInput[(size_t) i] = { frameBuffer[(size_t) i], 0.0f };

            fft.perform (fftInput.data(), fftOutput.data(), false);

            for (int k = 0; k < numBins; ++k)
            {
                magnitudes[(size_t) k] = std::abs (fftOutput[(size_t) k]);
                phases[(size_t) k] = std::arg (fftOutput[(size_t) k]);
                inputMagnitudes[(size_t) k] = magnitudes[(size_t) k];
            }
        }
        else
        {
            // Frozen: keep the captured magnitudes and advance each bin at its own frequency.
            constexpr float frozenPhaseStep = detail::twoPi * (float) hopSize / (float) frameSize;

            for (int k = 0; k < numBins; ++k)
                phases[(size_t) k] = detail::wrapPhase (phases[(size_t) k] + frozenPhaseStep * (float) k);
        }

        shapeMagnitudes();

        // The drift per bin comes from the routed LFO's value, read through a per-bin lag:
        // bin k reads the value from round(k * binSpread) frames ago, which is how the drift
        // smears across the bins. stereoPhase adds an extra lag for this engine instance, which
        // is how the right channel lags the left (plan 05.1 pass 2: the drift has no internal
        // oscillator any more; it is driven from the outside).
        const int stereoLag = (int) std::round (settings.stereoPhase / juce::MathConstants<float>::pi
                                                * (float) stereoLagFrames);

        if (! driftHistoryFilled)
        {
            // The first frame seeds the whole history with the current value, so every bin
            // starts drifting at once instead of waiting for its lag to fill.
            driftHistory.fill (settings.driftValue);
            driftHistoryFilled = true;
        }
        else
        {
            driftHistory[(size_t) driftHistoryPos] = settings.driftValue;
        }

        const int newest = driftHistoryPos;
        driftHistoryPos = (driftHistoryPos + 1) % numBins;

        // Bins below the drift start stay exactly as analysed.
        const float driftStartBin = std::clamp (settings.driftStart, 0.0f, 1.0f) * (float) numBins;

        for (int k = 0; k < numBins; ++k)
        {
            const float drift = (float) k < driftStartBin
                                ? 0.0f
                                : [&]
            {
                const int lag = juce::jlimit (0, numBins - 1,
                                              (int) std::round ((float) k * settings.binSpread) + stereoLag);
                const float value = lag == 0 ? settings.driftValue
                                             : driftHistory[(size_t) ((newest - lag + numBins) % numBins)];
                return settings.driftDepth * value;
            }();

            spectrum[(size_t) k] = std::polar (shapedMagnitudes[(size_t) k], phases[(size_t) k] + drift);
        }

        // A real signal needs real DC and Nyquist bins, and conjugate symmetry everywhere else.
        spectrum[0].imag (0.0f);
        spectrum[frameSize / 2].imag (0.0f);

        for (int k = 1; k < frameSize / 2; ++k)
            spectrum[(size_t) (frameSize - k)] = std::conj (spectrum[(size_t) k]);

        fft.perform (spectrum.data(), fftInput.data(), true);

        for (int i = 0; i < frameSize; ++i)
            frameBuffer[(size_t) i] = fftInput[(size_t) i].real();

        window.multiplyWithWindowingTable (frameBuffer.data(), frameSize);

        for (int i = 0; i < frameSize; ++i)
            outputAccumulator[(size_t) ((start + i) % frameSize)] += frameBuffer[(size_t) i];

        // Measure the output spectrum on the samples that are already final (one frame old).
        for (int i = 0; i < frameSize; ++i)
            frameBuffer[(size_t) i] = wetRing[(size_t) ((start + i) % frameSize)];

        window.multiplyWithWindowingTable (frameBuffer.data(), frameSize);

        for (int i = 0; i < frameSize; ++i)
            fftInput[(size_t) i] = { frameBuffer[(size_t) i], 0.0f };

        fft.perform (fftInput.data(), fftOutput.data(), false);

        for (int k = 0; k < numBins; ++k)
            outputMagnitudes[(size_t) k] = std::abs (fftOutput[(size_t) k]);

        ++frameCount;
    }
}
