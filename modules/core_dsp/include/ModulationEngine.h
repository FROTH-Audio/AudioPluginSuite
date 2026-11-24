#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "SequencerEngine.h"

/**
 * Low-frequency oscillators and modulation maths for the Micro-Timing Chaos plugin.
 *
 * The LFO input is the number of cycles elapsed, as an absolute value: 2.25 is a quarter of the
 * way through the third cycle. Keeping the count unwrapped lets sample and hold tell which cycle
 * it is in, and lets the processor change the rate without the phase jumping.
 */
namespace CoreDSP::Modulation
{
    enum class LfoShape
    {
        sine = 0,
        triangle = 1,
        saw = 2,
        square = 3,
        sampleAndHold = 4,
        random = 5,
        noise = 6,
        bouncingBall = 7,
        count
    };

    /** An independent random level in [-1, 1] for one grid point. Every grid point, in every cycle, gets its own level. */
    inline float gridLevel (std::uint32_t seed, double gridPoint) noexcept
    {
        const auto index = static_cast<std::uint32_t> (static_cast<std::int64_t> (gridPoint));
        return 2.0f * CoreDSP::Sequencer::hashUnit (seed, index) - 1.0f;
    }

    /**
        Per-LFO waveform transforms (plan 17 Phase 7). Every field defaults to identity, so a
        waveform read without transforms is bit-exact.
    */
    struct LfoTransforms
    {
        bool invert = false;         // polarity toggle: the waveform is negated
        bool rectify = false;        // fold into the upper half: |value|; with invert on, the lower half
        float quantiseSteps = 0.0f; // 0 = off; above 0 the value is quantised to that many levels over [-1, 1]
        float smooth = 0.0f;         // 0 = off; above 0 sharp corners are rounded (strength, 0 to 1)
        float pulseWidth = 0.0f;     // 0 = off; above 0 the shape is read as a pulse, high while phase < width
        float skew = 0.0f;           // phase lean in cycles, 0 = off
    };

    /**
     * The raw waveform value in [-1, 1] for a shape at a cycle position.
     */
    inline float shapeValue (LfoShape shape, double cycles, std::uint32_t seed) noexcept
    {
        const double cycleFloor = std::floor (cycles);
        const double p = cycles - cycleFloor;

        switch (shape)
        {
            case LfoShape::sine:
                return (float) std::sin (2.0 * 3.14159265358979323846 * p);

            case LfoShape::triangle:
                return (float) (1.0 - 4.0 * std::abs (p - 0.5));

            case LfoShape::saw:
                return (float) (2.0 * p - 1.0);

            case LfoShape::square:
                return p < 0.5 ? 1.0f : -1.0f;

            case LfoShape::sampleAndHold:
            {
                const auto index = static_cast<std::uint32_t> (static_cast<std::int64_t> (cycleFloor));
                return 2.0f * CoreDSP::Sequencer::hashUnit (seed, index) - 1.0f;
            }

            case LfoShape::random:
            {
                const double position = cycles * 8.0;
                const double cell = std::floor (position);
                const float t = (float) (position - cell);
                const float a = gridLevel (seed, cell);
                const float b = gridLevel (seed, cell + 1.0);
                return a + (b - a) * t;
            }

            case LfoShape::noise:
                return gridLevel (seed + 0x9E37u, std::floor (cycles * 32.0));

            case LfoShape::bouncingBall:
            {
                // A bouncing ball: four diminishing bounces across the cycle — the ball drops
                // from full height (+1), each bounce lower than the last, and rests at the
                // bottom (-1) at the cycle's end — then re-drops every cycle, so it keeps
                // modulating at full amplitude. The earlier shape at this index was the
                // pendulum swing (1.5s - 0.5s³), which the owner replaced.
                const double bounces = std::cos (2.0 * 3.14159265358979323846 * 4.0 * p);
                return (float) ((1.0 - p) * std::abs (bounces) * 2.0 - 1.0);
            }

            case LfoShape::count:
                break;
        }

        return 0.0f;
    }

    /**
     * Output in [-1, 1] for a number of elapsed cycles, after the per-LFO transforms and before the
     * routing offset is added. The plain shape is the transforms-disabled case, so the existing call
     * sites and tests keep their meaning.
     */
    inline float lfoValue (LfoShape shape, double cycles, std::uint32_t seed, const LfoTransforms& transforms) noexcept
    {
        const double cycleFloor = std::floor (cycles);
        double p = cycles - cycleFloor;

        if (transforms.skew != 0.0f)
            p = std::fmod (p + static_cast<double> (transforms.skew), 1.0);

        float value = shapeValue (shape, cycleFloor + p, seed);

        if (transforms.rectify)
            value = std::abs (value);
        if (transforms.invert)
            value = -value;

        if (transforms.pulseWidth > 0.0f)
            value = (p < static_cast<double> (transforms.pulseWidth)) ? value : 0.0f;

        if (transforms.smooth > 0.0f)
        {
            // Round sharp corners: blend the value at p with the linear interpolation between the
            // values at p - window and p + window, weighted by the smoothing strength.
            constexpr double cornerWindow = 0.25;
            const double pLow = std::max (0.0, p - cornerWindow);
            const double pHigh = std::min (1.0, p + cornerWindow);
            const float valueLow = shapeValue (shape, cycleFloor + pLow, seed);
            const float valueHigh = shapeValue (shape, cycleFloor + pHigh, seed);
            const float interpolated = valueLow + (valueHigh - valueLow) * static_cast<float> ((p - pLow) / (pHigh - pLow));
            value = static_cast<float> (transforms.smooth) * interpolated + (1.0f - static_cast<float> (transforms.smooth)) * value;
        }

        if (transforms.quantiseSteps > 0.0f)
        {
            int levels = static_cast<int> (std::round (transforms.quantiseSteps));
            levels = std::max (2, std::min (16, levels));
            const float index = std::round ((value + 1.0f) / 2.0f * static_cast<float> (levels - 1));
            const int snapped = static_cast<int> (std::clamp (index, 0.0f, static_cast<float> (levels - 1)));
            value = -1.0f + 2.0f * static_cast<float> (snapped) / static_cast<float> (levels - 1);
        }

        return value;
    }

    inline float lfoValue (LfoShape shape, double cycles, std::uint32_t seed) noexcept
    {
        return lfoValue (shape, cycles, seed, LfoTransforms ());
    }

    /**
     * Adds a modulation offset in normalised space (0 to 1 across the parameter's range) and clamps the result.
     */
    inline float modulatedNormalised (float normalised, float offsetSum) noexcept
    {
        return std::clamp (normalised + offsetSum, 0.0f, 1.0f);
    }

    /**
     * Adds an offset to a waveform value (no clamping).
     */
    inline float withOffset (float wave, float offset) noexcept
    {
        return wave + offset;
    }
}