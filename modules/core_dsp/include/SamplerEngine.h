#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "ChaosEngine.h"
#include "SamplerMaps.h"

/**
    Sampler engine helpers for Sgraffito: memory traces, tape-stop ramps, voices.

    Everything here is deterministic and testable in isolation. The plugin owns the
    audio buffer, the voice pool lifetime and the per-block processing loop.
*/
namespace CoreDSP::Sampler
{
    constexpr int maxVoices = 32;

    /** Per-region memory trace: how a region was played the last time it sounded.

        gainOffset is in dB (-12 to 0), toneOffset and noiseOffset are normalised
        (-1 to 1, 0 to 1). Traces live in plugin state so a session remembers its
        history.
    */
    struct RegionTrace
    {
        float gainOffset = 0.0f;    // -12 to 0 dB
        float toneOffset = 0.0f;    // -1 to 1
        float noiseOffset = 0.0f;   // 0 to 1
    };

    /** Move a trace a fraction toward the current note's values, then apply decay.

        `amount` (0 to 1) sets how far each note moves the trace; `decay` (0 to 1)
        sets how fast the trace fades toward clean when nothing is played. All values
        are clamped to fixed ranges so traces cannot grow without bound.
    */
    inline RegionTrace updateTrace (RegionTrace trace, float currentGainDb,
                                    float currentTone, float amount, float decay) noexcept
    {
        const float t = std::clamp (amount, 0.0f, 1.0f);
        const float d = std::clamp (decay, 0.0f, 1.0f);

        trace.gainOffset = std::clamp (trace.gainOffset * (1.0f - d) + currentGainDb * t,
                                       -12.0f, 0.0f);
        trace.toneOffset = std::clamp (trace.toneOffset * (1.0f - d) + currentTone * t,
                                       -1.0f, 1.0f);
        trace.noiseOffset = std::clamp (trace.noiseOffset * (1.0f - d) + currentTone * t,
                                        0.0f, 1.0f);

        return trace;
    }

    /** Tape-stop ramp: playback rate falls toward zero over a set time.

        Call tick() once per sample while stopping; it returns the rate in [0, 1].
        active() is false once the ramp has finished.
    */
    class TapeStopRamp
    {
    public:
        enum class Curve { linear, exponential, slowThenFast };

        void start (double timeMs, Curve curve) noexcept
        {
            startMs = std::max (timeMs, 0.0);
            remainingMs = startMs;
            this->curve = curve;
        }

        float tick (double sampleRate) noexcept
        {
            if (remainingMs <= 0.0)
                return 0.0f;

            remainingMs -= 1000.0 / sampleRate;
            const double progress = remainingMs <= 0.0 ? 1.0
                                                       : (1.0 - remainingMs / startMs);
            const double clamped = std::clamp (progress, 0.0, 1.0);

            double factor;
            switch (curve)
            {
                case Curve::linear:      factor = 1.0 - clamped; break;
                case Curve::exponential: factor = std::exp (-3.0 * clamped); break;
                case Curve::slowThenFast: factor = std::pow (1.0 - clamped, 2.0); break;
            }

            return static_cast<float> (std::clamp (factor, 0.0, 1.0));
        }

        bool active() const noexcept { return remainingMs > 0.0; }

    private:
        double remainingMs = 0.0;
        double startMs = 0.0;
        Curve curve = Curve::exponential;
    };

    /** Per-voice playback state. */
    struct Voice
    {
        bool active = false;
        int key = 60;
        float velocity = 1.0f;
        double position = 0.0;      // read position inside the region (samples)
        double rate = 1.0;          // playback rate (1.0 = normal speed)
        float gain = 1.0f;          // current gain, includes memory trace
        double regionStart = 0.0;   // region start in samples
        double regionEnd = 0.0;     // region end in samples
        double releaseStart = 0.0;  // sample index when note-off happened
        bool tapeStopping = false;  // voice is in a tape-stop fade-out
        TapeStopRamp tapeStop;

        // Envelope state
        double envStart = 0.0;      // sample index when note-on happened
        double envEnd = 0.0;        // sample index when note-off happened (release start)
        float envValue = 0.0f;      // current envelope value [0, 1]
        bool envSustain = false;    // true if sustain pedal is held

        /** Envelope value at the given sample index. */
        float envelope (double sampleIndex, double attackMs, double releaseMs, double sampleRate) const noexcept;
    };

    inline float Voice::envelope (double sampleIndex, double attackMs, double releaseMs, double sampleRate) const noexcept
    {
        if (envStart < 0.0)
            return 0.0f;

        const double attackSamples = attackMs * sampleRate / 1000.0;
        const double releaseSamples = releaseMs * sampleRate / 1000.0;

        if (sampleIndex <= envStart)
            return 0.0f;

        const double age = sampleIndex - envStart;

        if (envEnd > 0.0 && sampleIndex >= envEnd)
        {
            // Release phase
            const double releaseAge = sampleIndex - envEnd;
            const float releaseProgress = static_cast<float> (releaseAge / releaseSamples);
            return envValue * std::max (0.0f, 1.0f - releaseProgress);
        }

        // Attack phase
        const float attackProgress = static_cast<float> (age / attackSamples);
        return envValue * std::min (1.0f, attackProgress);
    };

    /** Voice pool: 32 voices. Stealing takes the oldest voice in release first,
        then the oldest voice overall. Tape-stopping voices are stolen first since
        they are already fading.
    */
    class VoicePool
    {
    public:
        Voice voices[maxVoices];

        VoicePool() = default;

        /** Mark a voice as playing a note. Returns the voice index.

            For Cloud mode (mode == 2), spawns multiple grains spread across the
            region with slight detune, creating a granular cloud texture.
        */
        int allocateVoice (int key, float velocity, double regionSpan, double regionLength, int mode, int sliceCount) noexcept
        {
            // Compute region based on mode (shared by all grains in a cloud).
            const double keyPos = (double) key / 127.0;
            double regionStart = 0.0, regionEnd = 1.0;

            if (mode == 1)  // Chop mode: slice-based
            {
                const int sliceCountClamped = std::clamp (sliceCount, 2, maxRegions);
                const int sliceIndex = std::clamp ((int) (keyPos * sliceCountClamped), 0, sliceCountClamped - 1);
                const double sliceLen = 1.0 / sliceCountClamped;
                regionStart = sliceIndex * sliceLen;
                regionEnd = (sliceIndex + 1) * sliceLen;
            }
            else  // Scrub, Cloud, Swell: linear mapping
            {
                const double maxStart = 1.0 - std::clamp (regionLength, 0.0, 1.0);
                const double start = std::clamp (keyPos * std::clamp (regionSpan, 0.0, 1.0), 0.0, maxStart);
                const double end = std::clamp (start + std::clamp (regionLength, 0.0, 1.0), 0.0, 1.0);
                regionStart = start;
                regionEnd = end;
            }

            // Number of grains for Cloud mode; single voice otherwise.
            const int numGrains = (mode == 2) ? 4 : 1;
            const double cloudSpread = 0.4;   // fraction of region over which grains spread
            const double cloudDetune = 0.003; // rate detune per grain step (fractional)

            auto initVoice = [&] (int idx, int g) noexcept
            {
                voices[idx] = Voice {};
                voices[idx].active = true;
                voices[idx].key = key;
                voices[idx].velocity = velocity;
                voices[idx].envValue = 1.0f;  // Start at full envelope value
                voices[idx].regionStart = regionStart;
                voices[idx].regionEnd = regionEnd;

                // Stagger position within region for Cloud mode.
                double posOffset = 0.0;
                if (mode == 2 && numGrains > 1)
                    posOffset = ((double) g / (double) (numGrains - 1) - 0.5) * cloudSpread * (regionEnd - regionStart);

                voices[idx].position = regionStart + posOffset;
                voices[idx].envStart = -1.0;  // not started yet

                // Slight detune per grain for Cloud mode.
                if (mode == 2 && numGrains > 1)
                    voices[idx].rate = 1.0 + ((double) g - (double) (numGrains - 1) / 2.0) * cloudDetune;
            };

            int firstAllocated = -1;

            for (int g = 0; g < numGrains; ++g)
            {
                int idx = -1;

                // Find a free voice.
                for (int i = 0; i < maxVoices; ++i)
                {
                    if (! voices[i].active)
                    {
                        idx = i;
                        break;
                    }
                }

                // Steal: tape-stopping voices first, then oldest in release, then oldest overall.
                if (idx < 0)
                {
                    double oldestRelease = -1.0;
                    for (int i = 0; i < maxVoices; ++i)
                    {
                        if (! voices[i].active)
                            continue;

                        if (voices[i].tapeStopping)
                        {
                            idx = i;
                            break;
                        }

                        if (voices[i].releaseStart > oldestRelease)
                        {
                            oldestRelease = voices[i].releaseStart;
                            idx = i;
                        }
                    }
                }

                if (idx < 0)
                    break;  // no voices available

                initVoice (idx, g);

                if (firstAllocated < 0)
                    firstAllocated = idx;
            }

            return firstAllocated;
        }

        /** Mark a voice as released (note-off). */
        void releaseVoice (int index, double sampleIndex, double tapeStopTimeMs, int tapeStopCurve) noexcept
        {
            if (index < 0 || index >= maxVoices || ! voices[index].active)
                return;

            if (tapeStopTimeMs > 0.0)
            {
                voices[index].tapeStopping = true;
                voices[index].tapeStop.start (tapeStopTimeMs, static_cast<TapeStopRamp::Curve> (tapeStopCurve));
            }
            else
            {
                voices[index].active = false;
                voices[index].releaseStart = sampleIndex;
                voices[index].envEnd = sampleIndex;
            }
        }

        int countActive() const noexcept
        {
            int count = 0;
            for (int i = 0; i < maxVoices; ++i)
                if (voices[i].active)
                    ++count;
            return count;
        }
    };

    /** Ghost note: a quieter, delayed copy of a note. */
    struct Ghost
    {
        bool active = false;
        int key = 60;
        float velocity = 1.0f;
        double position = 0.0;
        double rate = 1.0;
        float gain = 1.0f;
        double delayRemainingMs = 0.0;
    };
}
