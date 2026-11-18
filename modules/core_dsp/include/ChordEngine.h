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
#include <cstdint>

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

    /** Arpeggio patterns, in the plugin's choice order (append-only once released). */
    enum class ArpPattern { up = 0, down = 1, alternate = 2, random = 3 };

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
        for (int i = count; i < 4; ++i)
            out[i] = -1;   // unused slots are marked so callers do not read garbage
    }
}
