#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "SequencerEngine.h"

/**
    Sample mapping for Sgraffito: key-to-region maps, scale lock and shuffle permutations.

    Everything here is deterministic and stateless, so maps can be tested in isolation.
    The plugin supplies the playback position, pitch and grain logic.
*/
namespace CoreDSP::Sampler
{
    constexpr int maxKeys = 128;     // full MIDI range
    constexpr int maxRegions = 32;   // maximum slice count for chop mode

    /** A region of the sample, as a fraction of the sample length [0, 1]. */
    struct Region
    {
        double start = 0.0;   // start position, 0 = beginning of sample
        double length = 1.0;  // length, 1 = whole sample
    };

    /** Linear key map: low keys start early in the sample, high keys start late.

        `span` is the share of the sample the whole keyboard covers (0 to 1).
        `length` is each key's region length (0 to 1). Root key plays at its natural
        pitch; pitch shifting is handled separately by the plugin.
    */
    inline Region keyRegion (int key, int rootKey, double span, double length) noexcept
    {
        const double position = (double) key / 127.0;   // key 0 → 0, key 127 → 1
        const double maxStart = 1.0 - std::clamp (length, 0.0, 1.0);
        const double start = std::clamp (position * std::clamp (span, 0.0, 1.0), 0.0, maxStart);
        const double end = std::clamp (start + std::clamp (length, 0.0, 1.0), 0.0, 1.0);
        return { start, end - start };
    }

    /** Chop map: divide the sample into `count` equal slices. Returns start and length for slice index. */
    inline Region chopRegion (int sliceIndex, int count) noexcept
    {
        const int n = std::clamp (count, 1, maxRegions);
        const double slice = 1.0 / (double) n;
        const double start = slice * std::clamp (sliceIndex, 0, n - 1);
        return { start, slice };
    }

    /** Scale definitions (same six scales as CoreDSP::Sequencer). */
    struct ScaleDefinition
    {
        int size;
        std::array<int, 12> intervals;
    };

    inline const ScaleDefinition& scaleDefinition (int scaleIndex) noexcept
    {
        static constexpr ScaleDefinition scales[6] = {
            { 12, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },   // chromatic
            { 7,  { 0, 2, 4, 5, 7, 9, 11, 0, 0, 0, 0, 0 } },    // major
            { 7,  { 0, 2, 3, 5, 7, 8, 10, 0, 0, 0, 0, 0 } },    // natural minor
            { 5,  { 0, 3, 5, 7, 10, 0, 0, 0, 0, 0, 0, 0 } },    // minor pentatonic
            { 7,  { 0, 2, 3, 5, 7, 9, 10, 0, 0, 0, 0, 0 } },    // dorian
            { 6,  { 0, 2, 4, 6, 8, 10, 0, 0, 0, 0, 0, 0 } }     // whole tone
        };
        return scales[std::clamp (scaleIndex, 0, 5)];
    }

    /** Number of distinct pitch degrees across the octave range. */
    inline int degreeCount (int scaleIndex, int octaves) noexcept
    {
        return scaleDefinition (scaleIndex).size * std::clamp (octaves, 1, 3);
    }

    /** MIDI note for a degree index counted up from rootKey in the given scale. */
    inline int degreeToNote (int rootKey, int scaleIndex, int degree) noexcept
    {
        const auto& scale = scaleDefinition (scaleIndex);
        const int octave = degree / scale.size;
        const int note = rootKey + scale.intervals[(size_t) (degree % scale.size)] + 12 * octave;
        return std::clamp (note, 0, 127);
    }

    /** Quantize a MIDI key to the nearest note in the given scale, relative to rootKey.

        Searches scale degrees across several octaves around rootKey and returns the
        closest matching MIDI note (0-127). When the scale is chromatic, the key is
        returned unchanged.
    */
    inline int quantizeToScale (int key, int rootKey, int scaleIndex) noexcept
    {
        const auto& scale = scaleDefinition (scaleIndex);
        const int size = scale.size;

        // Chromatic scale: every note is in the scale.
        if (size == 12)
            return std::clamp (key, 0, 127);

        int bestNote = rootKey;
        int bestDiff = std::abs (key - bestNote);

        // Search degrees across 4 octaves below to 3 octaves above rootKey.
        for (int octave = -4; octave <= 3; ++octave)
        {
            for (int d = 0; d < size; ++d)
            {
                const int note = rootKey + scale.intervals[(size_t) d] + 12 * octave;
                if (note < 0 || note > 127)
                    continue;
                const int diff = std::abs (key - note);
                if (diff < bestDiff)
                {
                    bestDiff = diff;
                    bestNote = note;
                }
            }
        }

        return bestNote;
    }

    /** Seeded permutation of region indices: same seed gives same map, every region used once. */
    inline std::array<int, maxRegions> shuffleRegions (std::uint32_t seed, int count) noexcept
    {
        std::array<int, maxRegions> regions {};

        const int n = std::clamp (count, 1, maxRegions);
        for (int i = 0; i < n; ++i)
            regions[(size_t) i] = i;

        for (int i = n - 1; i > 0; --i)
        {
            const std::uint32_t j = static_cast<std::uint32_t> (
                CoreDSP::Sequencer::hashUnit (seed, (std::uint32_t) i) * (float) n);
            std::swap (regions[(size_t) i], regions[(size_t) j]);
        }

        return regions;
    }
}
