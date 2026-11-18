#pragma once

/**
    Chord and arpeggio maths shared by the suite (plan 06 idea 2.11, MIDI Chord/Arp/Scale Tool).

    Chord tones are semitone intervals from the chord root. Scale quantisation is not re-implemented
    here: CoreDSP::SamplerMaps::quantizeToScale does it, with the same six scales the suite's other
    MIDI plugins use (plan risk note: keep the scale engine simple, reuse what exists).

    Pure functions, no JUCE dependency, so they test the same on every platform.
*/
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "SequencerEngine.h"

namespace CoreDSP::Chords
{
    /** Chord qualities, in the plugin's choice order (append-only once released). */
    enum class Quality
    {
        major = 0,
        minor = 1,
        dim = 2,
        aug = 3,
        sus2 = 4,
        sus4 = 5,
        minor7 = 6,
        major7 = 7,
        count
    };

    /** Semitone intervals of one quality, from the chord root. */
    inline std::array<int, 4> intervals (Quality quality) noexcept
    {
        switch (quality)
        {
            case Quality::major:  return { 0, 4, 7, 12 };   // triad; the 4th slot is the octave
            case Quality::minor:  return { 0, 3, 7, 12 };
            case Quality::dim:    return { 0, 3, 6, 12 };
            case Quality::aug:    return { 0, 4, 8, 12 };
            case Quality::sus2:   return { 0, 2, 7, 12 };
            case Quality::sus4:   return { 0, 5, 7, 12 };
            case Quality::minor7: return { 0, 3, 7, 10 };   // seventh replaces the octave
            case Quality::major7: return { 0, 4, 7, 11 };
            case Quality::count:  break;
        }
        return { 0, 4, 7, 12 };
    }

    /** Number of notes a quality produces (triads 3, sevenths 4). */
    inline int noteCount (Quality quality) noexcept
    {
        return quality == Quality::minor7 || quality == Quality::major7 ? 4 : 3;
    }

    /** The MIDI notes of a chord on chordRoot, clamped to 0-127. Output order is always
        ascending pitch; the arp pattern reorders a copy of this. */
    inline void chordNotes (int chordRoot, Quality quality, int (&out)[4]) noexcept
    {
        const auto iv = intervals (quality);
        for (int i = 0; i < 4; ++i)
            out[i] = std::clamp (chordRoot + iv[(size_t) i], 0, 127);
    }

    /** Arpeggio patterns, in the plugin's choice order (append-only once released).
        Scattered (plan 6.1) is a second deterministic shuffle on its own hash stream. */
    enum class ArpPattern { up = 0, down = 1, alternate = 2, random = 3, scattered = 4 };

    /** Reorders count chord notes into the pattern. in/out hold ascending notes; the random
        order is a deterministic function of (seed, count), so the same chord always gets the
        same sequence. */
    inline void orderArp (ArpPattern pattern, int count, const int (&ascending)[4],
                          int (&out)[4], std::uint32_t seed) noexcept
    {
        count = std::clamp (count, 0, 4);
        for (int i = 0; i < count; ++i)
            out[i] = ascending[i];

        if (pattern == ArpPattern::down)
            for (int i = 0; i < count; ++i)
                out[i] = ascending[(size_t) (count - 1 - i)];
        else if (pattern == ArpPattern::alternate)
            for (int i = 0; i < count; ++i)
                out[i] = ascending[(size_t) ((i % 2 == 0) ? i / 2 : count - 1 - i / 2)];
        else if (pattern == ArpPattern::random)
        {
            // Deterministic Fisher-Yates with an xorshift step per swap.
            auto next = [seed] (int i) mutable
            {
                auto s = seed + (std::uint32_t) i * 0x9E3779B9u;
                s ^= s << 13; s ^= s >> 17; s ^= s << 5;
                return s;
            };
            for (int i = count - 1; i > 0; --i)
            {
                const int j = (int) (next (i) % (std::uint32_t) (i + 1));
                std::swap (out[i], out[j]);
            }
        }
        else if (pattern == ArpPattern::scattered)
        {
            // Scattered (plan 6.1): a deterministic shuffle on its own hash stream, so the
            // same chord always gets the same scattered order — a different one from
            // Random's stream, which keeps both choices meaningful.
            auto next = [seed] (int i) mutable
            {
                auto s = seed * 0x85EBCA6Bu + (std::uint32_t) i * 0xC2B2AE3Du + 0x165667B1u;
                s ^= s << 13; s ^= s >> 17; s ^= s << 5;
                return s;
            };
            for (int i = count - 1; i > 0; --i)
            {
                const int j = (int) (next (i) % (std::uint32_t) (i + 1));
                std::swap (out[i], out[j]);
            }
        }
        for (int i = count; i < 4; ++i)
            out[i] = -1;   // unused slots are marked so callers do not read garbage
    }

    //---- Step-clock arpeggiator helpers (plan 6.1) ----

    /** Step length in beats for the arpeggiator's Step Rate choice:
        0 = 1/8, 1 = 1/8T, 2 = 1/16, 3 = 1/16T, 4 = 1/32 (T is a triplet, 2/3 of the straight value). */
    inline double arpStepBeats (int rateIndex) noexcept
    {
        static constexpr double beats[5] = { 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
        return beats[(std::size_t) std::clamp (rateIndex, 0, 4)];
    }

    /** The octave lift, in semitones, of one step in the 8-step sequence: each pass through
        the chord tones (a cycle of `count` steps) moves by round(spread * cycle) octaves.
        spread +1 crawls up one octave per pass, -1 crawls down, 0 stays put. */
    inline int arpOctaveLift (float spread, int cycle) noexcept
    {
        return 12 * (int) std::lround ((double) std::clamp (spread, -1.0f, 1.0f) * (double) cycle);
    }

    /** The velocity-curve value of one arp step (plan 6.1). Flat is 1; Linear is
        lerp(0.2, 1, s); Exponential is 0.2 + 0.8 s^2, where s is the direction-shaped
        position 0..1 across the steps: Ascending runs from the 0.2 end up to 1, Descending
        from 1 down to 0.2. Random uses arpRandomVelocity instead. */
    inline float arpCurveValue (int curveIndex, bool ascending, int step, int numSteps) noexcept
    {
        const float t = numSteps > 1 ? (float) step / (float) (numSteps - 1) : 0.0f;
        const float s = ascending ? t : 1.0f - t;

        switch (std::clamp (curveIndex, 0, 3))
        {
            case 1:  return 0.2f + 0.8f * s;          // linear
            case 2:  return 0.2f + 0.8f * s * s;      // exponential
            default: return 1.0f;                     // flat
        }
    }

    /** Random velocity: a deterministic hash of (note, step, cycle) mapped into [0.2, 1].
        No seed parameter: the same chord on the same step of the same cycle always gets
        the same value, so the shape is testable and it never changes between runs. */
    inline float arpRandomVelocity (int note, int step, int cycle) noexcept
    {
        const float unit = Sequencer::hashUnit ((std::uint32_t) (note * 40503 + step * 7),
                                                (std::uint32_t) (cycle + 1));
        return 0.2f + 0.8f * unit;
    }
}
