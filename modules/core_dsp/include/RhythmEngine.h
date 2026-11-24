#pragma once

#include <juce_core/juce_core.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "SequencerEngine.h"

/**
    CoreDSP::Rhythm — the Groove Mutator's pattern maths (plan 19).

    Everything is pure and deterministic: the euclid masks, the mutation operators and the
    fill additions derive from (seed, musical position) alone, so the DSP tests can assert
    exact sequences with no GUI and no host. The plugin supplies the parameters and the
    musical time; the engine owns the bitmask maths.

    The mask convention: bit k = step k fires, bit 0 = step 0, at most `maxSteps` bits.
    The mutation tick rewrites the lane's *live* mask copy — the pattern the parameters
    define is the base, and a Seed change / preset load / Freeze-off re-derives it.
*/
namespace CoreDSP::Rhythm
{
    constexpr int maxSteps = 32;
    constexpr int patternKinds = 3;      // 0 Euclid, 1 Hash, 2 Steps
    constexpr int numDivisions = 8;      // 1/32, 1/16T, 1/16, 1/8T, 1/8, 1/4, 1/2, 1 Bar
    constexpr int numPolyRatios = 6;     // 1:1, 3:4, 5:4, 7:8, 4:3, 8:7
    constexpr int numFillStyles = 4;     // Tom Roll, Snare Build, Hat Drop, Chaos Burst

    /** One lane's parameter slice (the plugin converts its APVTS values into this). */
    struct LaneParams
    {
        bool on = true;
        int patternKind = 0;             // 0 Euclid, 1 Hash, 2 Steps
        int steps = 16;
        int pulses = 4;
        int rotation = 0;
        std::uint32_t stepsMask = 0;     // the Steps kind: the user's painted steps
        float divisionBeats = 0.25;      // the lane's own step length in beats
        int note = 36;
        float velocity = 1.0f;           // 0.3 to 1.5 (the % range / 100)
        float accent = 0.3f;             // 0 to 1
        float flam = 0.0f;               // 0 to 1
        float ghost = 0.1f;              // 0 to 1
        float gain = 1.0f;               // the Level stage, 0.06 to 4 (dB /100 folded in)
    };

    /** One lane's live state. The mask is the engine's copy — the parameters are the base. */
    struct LaneState;

    std::uint32_t basePattern (const LaneParams& params, std::uint32_t seed) noexcept;
    std::uint32_t euclidPattern (int pulses, int steps, int rotation) noexcept;

    struct LaneState
    {
        std::uint32_t baseMask = 0;      // re-derived from the parameters
        std::uint32_t mask = 0;          // the live, mutated mask
        std::uint64_t mutateTick = 0;    // the musical mutation tick
        int lastStepFired = -1;

        /** Re-derives the base from the parameters and resets the live copy to it. */
        void rederive (const LaneParams& params, std::uint32_t seed)
        {
            baseMask = basePattern (params, seed);
            mask = baseMask;
        }
    };

    /** The lane's base mask from its parameters: Euclid, Hash or the painted Steps. */
    inline std::uint32_t basePattern (const LaneParams& params, std::uint32_t seed) noexcept
    {
        const int steps = std::clamp (params.steps, 1, maxSteps);
        switch (params.patternKind)
        {
            case 1:  // Hash: the MTC hashed pattern, density = the pulse share.
            {
                const float density = steps > 0
                                          ? (float) std::clamp (params.pulses, 0, steps) / (float) steps
                                          : 0.0f;
                std::uint32_t mask = 0;
                for (int i = 0; i < steps; ++i)
                    if (Sequencer::stepFires (seed, i, Sequencer::densityCurve (density)))
                        mask |= (1u << i);
                return mask;
            }
            case 2:  // Steps: the user's painted steps, clamped to the current count.
            {
                const std::uint32_t full = steps >= 32 ? 0xFFFFFFFFu : (1u << steps) - 1u;
                return params.stepsMask & full;
            }
            default:  // Euclid
                return euclidPattern (params.pulses, steps, params.rotation);
        }
    }

    /**
        Bjorklund's algorithm as a bitmask: bit k = step k fires.

        E(0, n) is empty, E(n, n) is full, and the distribution is the classic Euclidean
        rhythm (E(3, 8) = x..x..x.). `pulses` clamps to [0, steps]; `rotation` wraps, so
        the pattern starts `rotation` steps into the base cycle.
    */
    inline std::uint32_t euclidPattern (int pulses, int steps, int rotation) noexcept
    {
        const int n = std::clamp (steps, 1, maxSteps);
        const int k = std::clamp (pulses, 0, n);
        if (k == 0)
            return 0;

        std::uint32_t base = 0;
        for (int i = 0; i < n; ++i)
            if ((i * k) % n < k)
                base |= (1u << i);

        if (k == n)
            return base;   // full: rotation cannot move anything

        const int r = ((rotation % n) + n) % n;
        if (r == 0)
            return base;

        std::uint32_t rotated = 0;
        for (int i = 0; i < n; ++i)
            if ((base >> ((i + r) % n)) & 1u)
                rotated |= (1u << i);
        return rotated;
    }

    /** Step length in beats for a division choice: 0 = 1/32, 1 = 1/16T, 2 = 1/16,
        3 = 1/8T, 4 = 1/8, 5 = 1/4, 6 = 1/2, 7 = 1 Bar. */
    inline double divisionBeats (int choiceIndex) noexcept
    {
        static constexpr double beats[numDivisions] = { 0.125, 1.0 / 6.0, 0.25, 0.375, 0.5, 1.0, 2.0, 4.0 };
        return beats[std::clamp (choiceIndex, 0, numDivisions - 1)];
    }

    /** The polyrhythm's scale factor: the choice's numerator over denominator. Metric
        modulation — every lane's division scales by this. */
    inline double polyScale (int choiceIndex) noexcept
    {
        static constexpr double ratios[numPolyRatios][2] = {
            { 1.0, 1.0 }, { 3.0, 4.0 }, { 5.0, 4.0 }, { 7.0, 8.0 }, { 4.0, 3.0 }, { 8.0, 7.0 }
        };
        const auto& r = ratios[std::clamp (choiceIndex, 0, numPolyRatios - 1)];
        return r[0] / r[1];
    }

    /** Deterministic hash of (seed, index) mapped to [0, 1) — the Sequencer's hash. */
    inline float hashUnit (std::uint32_t seed, std::uint32_t index) noexcept
    {
        return Sequencer::hashUnit (seed, index);
    }

    /** The mutation hash: deterministic from (seed, tick, lane). */
    inline std::uint32_t mutationHash (std::uint32_t seed, std::uint64_t tick, int lane) noexcept
    {
        return (std::uint32_t) (seed * 0x9E3779B9u + tick * 0x85EBCA6Bu + (std::uint32_t) lane * 0x27D4EB2Fu);
    }

    /**
        The mutation operators, tiered by `scale` (0 to 1):

        scale 0      — unchanged (the pattern holds).
        tier 0 drift — one pulse moves one step.
        tier 1 swap  — two steps exchange their states.
        tier 2 split — a pulse splits into two, or two merge into one.
        tier 3 flip  — a fired step clears and an unfired one fires.
        tier 4 morph — the density moves by one pulse.

        `hash` picks the operator (within the tier bound) and the steps it touches. The
        pulse count can never leave [0, steps], and every operator is deterministic from
        the inputs alone.
    */
    inline std::uint32_t mutatePattern (std::uint32_t mask, int steps, float scale, std::uint32_t hash) noexcept
    {
        const int n = std::clamp (steps, 1, maxSteps);
        const std::uint32_t full = n >= 32 ? 0xFFFFFFFFu : (1u << n) - 1u;
        mask &= full;

        const float s = std::clamp (scale, 0.0f, 1.0f);
        if (s <= 0.0f)
            return mask;   // scale 0: the pattern holds

        const int maxTier = std::clamp ((int) (s * 5.0f), 1, 5) - 1;   // 0..4
        const int op = (int) (hash % (std::uint32_t) (maxTier + 1));

        auto pulses = [mask] () noexcept
        {
            int count = 0;
            for (int i = 0; i < maxSteps; ++i)
                if ((mask >> i) & 1u)
                    ++count;
            return count;
        };

        // Pick two distinct step indices from the hash (deterministic).
        const int a = (int) (Sequencer::hashUnit (hash, 0u) * (float) n) % n;
        const int b = (a + 1 + (int) (Sequencer::hashUnit (hash, 1u) * (float) (n - 1))) % n;

        switch (op)
        {
            case 0:  // drift: one pulse moves one step (toward a, clamped inside).
            {
                const int dir = ((hash >> 8) & 1u) != 0u ? 1 : -1;
                for (int i = 0; i < n; ++i)
                {
                    const int from = dir > 0 ? i : n - 1 - i;   // scan outward from the chosen end
                    if ((mask >> from) & 1u)
                    {
                        const int to = std::clamp (from + dir, 0, n - 1);
                        if (((mask >> to) & 1u) == 0u)
                            return (mask & ~(1u << from)) | (1u << to);
                        return mask;   // the neighbour is busy: nothing to drift into
                    }
                }
                return mask;   // nothing fired: nothing to drift
            }
            case 1:  // swap: two steps exchange their states.
            {
                const bool aFired = ((mask >> a) & 1u) != 0u;
                const bool bFired = ((mask >> b) & 1u) != 0u;
                if (aFired == bFired)
                    return mask;
                return (mask ^ (1u << a)) ^ (1u << b);
            }
            case 2:  // split/merge: split one pulse into two, or merge two into one.
            {
                if (pulses() < n && ((mask >> a) & 1u) != 0u && ((mask >> b) & 1u) == 0u)
                    return mask | (1u << b);       // split: the neighbour fires too
                if (pulses() > 0 && ((mask >> a) & 1u) != 0u && ((mask >> b) & 1u) != 0u && a != b)
                    return mask & ~(1u << b);      // merge: one of the two clears
                if (pulses() > 0 && ((mask >> a) & 1u) != 0u)
                    return mask & ~(1u << a);      // merge fallback
                if (pulses() < n && ((mask >> a) & 1u) == 0u)
                    return mask | (1u << a);       // split fallback
                return mask;
            }
            case 3:  // flip: a fired step clears and an unfired one fires.
            {
                if (pulses() == 0 || pulses() == n)
                    return mask;   // nothing to trade
                int from = -1, to = -1;
                for (int i = 0; i < n; ++i)
                    if (((mask >> ((a + i) % n)) & 1u) != 0u) { from = (a + i) % n; break; }
                for (int i = 0; i < n; ++i)
                    if (((mask >> ((b + i) % n)) & 1u) == 0u) { to = (b + i) % n; break; }
                if (from < 0 || to < 0 || from == to)
                    return mask;
                return (mask & ~(1u << from)) | (1u << to);
            }
            default:  // morph: the density moves by one pulse (the hash decides the direction).
            {
                const bool grow = ((hash >> 16) & 1u) != 0u;
                if (grow && pulses() < n)
                {
                    for (int i = 0; i < n; ++i)
                        if (((mask >> ((a + i) % n)) & 1u) == 0u)
                            return mask | (1u << ((a + i) % n));
                    return mask;
                }
                if (pulses() > 0)
                {
                    for (int i = 0; i < n; ++i)
                        if (((mask >> ((a + i) % n)) & 1u) != 0u)
                            return mask & ~(1u << ((a + i) % n));
                }
                return mask;
            }
        }
    }

    /**
        The fill additions for one fill window: every candidate step that is currently
        UNFIRED may fire, so the fill never collides with an existing hit's note-off (the
        plan-19 rule). Deterministic from the hash; `prob` 0 adds nothing. The style picks
        the candidates: 0 Tom Roll (every other step), 1 Snare Build (the second half,
        denser toward the end), 2 Hat Drop (every fourth step), 3 Chaos Burst (hashed).
    */
    inline std::uint32_t fillMask (std::uint32_t liveMask, int steps, int fillStyle, float prob,
                                   std::uint32_t hash) noexcept
    {
        const int n = std::clamp (steps, 1, maxSteps);
        const float p = std::clamp (prob, 0.0f, 1.0f);
        if (p <= 0.0f)
            return 0;

        std::uint32_t additions = 0;
        for (int i = 0; i < n; ++i)
        {
            if (((liveMask >> i) & 1u) != 0u)
                continue;   // an existing hit: never touched

            bool candidate = false;
            switch (std::clamp (fillStyle, 0, numFillStyles - 1))
            {
                case 0: candidate = (i % 2) == 0; break;                       // Tom Roll
                case 1: candidate = i >= n / 2 && (i % 2) == 0; break;         // Snare Build
                case 2: candidate = (i % 4) == 0; break;                       // Hat Drop
                default: candidate = Sequencer::hashUnit (hash, (std::uint32_t) i) < 0.5f; break;
            }

            if (candidate && Sequencer::hashUnit (hash ^ 0x5A5A5A5Au, (std::uint32_t) i) < p)
                additions |= (1u << i);
        }
        return additions;
    }

    /** True if the step at `stepIndex` (its index within the lane's cycle) lies in the
        fill window: the last `fillBars` four-beat bars of the cycle. A cycle shorter
        than the window is entirely window. */
    inline bool inFillWindow (int stepIndex, int steps, double stepBeats, int fillBars,
                              double barBeats = 4.0) noexcept
    {
        if (fillBars <= 0)
            return false;
        const double cycleBeats = (double) std::clamp (steps, 1, maxSteps) * std::max (stepBeats, 1.0e-9);
        const double windowBeats = (double) fillBars * std::max (barBeats, 1.0e-9);
        const double position = (double) std::clamp (stepIndex, 0, maxSteps - 1) * std::max (stepBeats, 1.0e-9);
        return position >= cycleBeats - windowBeats;
    }
}  // namespace CoreDSP::Rhythm
