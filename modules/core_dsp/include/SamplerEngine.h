#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

#include "ChaosEngine.h"
#include "SamplerMaps.h"

/**
    Sampler engine helpers for Sgraffito: memory traces, tape-stop ramps and voices.

    Everything here is deterministic and allocation-free after construction. Region boundaries are
    normalised (0 to 1); playback positions are absolute frames in the loaded audio buffer.
*/
namespace CoreDSP::Sampler
{
    constexpr int maxVoices = 32;
    constexpr int cloudLanesPerNote = 4;

    /** Per-region memory trace. Values are bounded so repeated playback cannot grow without limit. */
    struct RegionTrace
    {
        float gainOffset = 0.0f;    // -12 to 0 dB
        float toneOffset = 0.0f;    // -1 to 1
        float noiseOffset = 0.0f;   // 0 to 1
    };

    /** Move a trace toward the current note, then apply a direct decay fraction in [0, 1]. */
    inline RegionTrace updateTrace (RegionTrace trace, float currentGainDb,
                                    float currentTone, float amount, float decay) noexcept
    {
        const float t = std::clamp (amount, 0.0f, 1.0f);
        const float d = std::clamp (decay, 0.0f, 1.0f);

        trace.gainOffset = std::clamp (trace.gainOffset * (1.0f - d) + currentGainDb * t,
                                       -12.0f, 0.0f);
        trace.toneOffset = std::clamp (trace.toneOffset * (1.0f - d) + currentTone * t,
                                       -1.0f, 1.0f);
        trace.noiseOffset = std::clamp (trace.noiseOffset * (1.0f - d)
                                        + std::abs (currentTone) * t, 0.0f, 1.0f);
        return trace;
    }

    /** Decay a trace toward clean after elapsedSeconds. A decay time of zero disables time decay. */
    inline RegionTrace decayTrace (RegionTrace trace, double elapsedSeconds, double decaySeconds) noexcept
    {
        if (elapsedSeconds <= 0.0 || decaySeconds <= 0.0)
            return trace;

        const double decay = 1.0 - std::exp (-elapsedSeconds / decaySeconds);
        return updateTrace (trace, 0.0f, 0.0f, 0.0f, static_cast<float> (decay));
    }

    /** Tape-stop ramp: playback rate and level fall toward zero over a set time. */
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
            if (remainingMs <= 0.0 || sampleRate <= 0.0)
                return 0.0f;

            remainingMs -= 1000.0 / sampleRate;

            if (remainingMs <= 0.0)
            {
                remainingMs = 0.0;
                return 0.0f;
            }

            const double progress = std::clamp (1.0 - remainingMs / startMs, 0.0, 1.0);
            double factor = 1.0;

            switch (curve)
            {
                case Curve::linear:       factor = 1.0 - progress; break;
                case Curve::exponential:  factor = std::exp (-3.0 * progress); break;
                case Curve::slowThenFast: factor = (1.0 - progress) * (1.0 - progress); break;
            }

            return static_cast<float> (std::clamp (factor, 0.0, 1.0));
        }

        bool active() const noexcept { return remainingMs > 0.0; }

    private:
        double remainingMs = 0.0;
        double startMs = 0.0;
        Curve curve = Curve::exponential;
    };

    /** Per-voice playback state. Cloud notes occupy four lanes from this same fixed pool. */
    struct Voice
    {
        bool active = false;
        int key = 60;                       // quantised key, used for pitch
        int inputKey = 60;                  // original key, used for region and note-off matching
        int mode = 0;
        int traceIndex = 0;
        float velocity = 1.0f;

        double regionStart = 0.0;           // normalised sample position
        double regionEnd = 1.0;             // normalised sample position
        double position = 0.0;              // absolute buffer frame
        double forwardPosition = 0.0;       // Swell's forward crossfade reader
        bool positionInitialised = false;

        double rate = 1.0;                  // target playback rate before tape stop
        double currentRate = 1.0;           // smoothed toward rate for glide
        float gain = 1.0f;                  // memory-trace gain
        float tone = 0.0f;                  // memory-trace tone; negative is darker
        std::array<float, 2> toneState {};

        double envStart = -1.0;
        double envEnd = -1.0;
        bool envEndSet = false;            // true when envEnd has been set to a valid release position
        bool envEndUpdatedInBlock = false; // true when envEnd has been updated in this block to prevent multiple updates
        double releaseStart = -1.0;
        float envValue = 0.0f;
        float releaseLevel = 1.0f;
        bool noteHeld = true;
        bool tapeStopping = false;
        TapeStopRamp tapeStop;

        std::uint32_t noteId = 0;           // shared by all four lanes of a cloud note
        std::uint32_t startOrder = 0;       // monotonic, used by oldest-voice stealing

        int cloudLane = -1;
        std::uint32_t grainIndex = 0;
        bool grainActive = false;
        double grainPosition = 0.0;
        double grainStartSample = 0.0;
        double grainDurationSamples = 1.0;
        double nextGrainSample = 0.0;

        // Sustain hold: latch release when note is held (plan 05.4 Phase 3)
        bool sustainActive = false;         // voice should continue after note-off
        int sustainKey = 60;                // key that will release this voice
        double sustainTimeout = 0.0;        // time in seconds before timeout

        // Surface effects state: per-sample modulation of playback rate and level (plan 05.4 Phase 4)
        struct SurfaceState
        {
            double phaseDust = 0.0;          // slow low-frequency modulation (0.1-6 Hz)
            double phaseCrackle = 0.0;       // fast high-frequency texture (50-210 Hz)
            double phaseWow = 0.0;           // medium LFO-like rate modulation (0.5-4 Hz)
            double phaseFlutter = 0.0;       // faster LFO-like rate modulation (3-11 Hz)
            std::uint64_t sampleCounter = 0;  // for deterministic seeding
        } surfaceState;

        /** Envelope value at an absolute sample index. Release starts from releaseLevel without a jump. */
        float envelope (double sampleIndex, double attackMs, double releaseMs, double sampleRate) const noexcept
        {
            if (envStart < 0.0 || sampleIndex < envStart || sampleRate <= 0.0)
                return 0.0f;

            if (envEnd >= 0.0 && sampleIndex >= envEnd)
            {
                const double releaseSamples = std::max (1.0, releaseMs * sampleRate / 1000.0);
                const float progress = static_cast<float> ((sampleIndex - envEnd) / releaseSamples);
                return releaseLevel * std::max (0.0f, 1.0f - progress);
            }

            const double attackSamples = std::max (1.0, attackMs * sampleRate / 1000.0);
            const float progress = static_cast<float> ((sampleIndex - envStart) / attackSamples);
            return std::clamp (progress, 0.0f, 1.0f);
        }
    };

    /** Fixed 32-voice pool. Stealing order: tape stop, oldest release, then oldest active voice. */
    class VoicePool
    {
    public:
        Voice voices[maxVoices];

        VoicePool() = default;

        void reset() noexcept
        {
            for (auto& voice : voices)
                voice = Voice {};

            nextNoteId = 1;
            nextStartOrder = 1;
        }

        /** Allocate one voice, or four lanes in Cloud mode. Span and length are percentages. */
        int allocateVoice (int key, float velocity, double regionSpan, double regionLength,
                           int mode, int sliceCount, double startSample = 0.0, int regionKey = -1) noexcept
        {
            const int originalKey = regionKey >= 0 ? regionKey : key;
            const auto region = makeRegion (originalKey, regionSpan, regionLength, mode, sliceCount);
            const int count = mode == 2 ? cloudLanesPerNote : 1;
            const std::uint32_t noteId = nextNoteId++;
            int firstAllocated = -1;

            for (int lane = 0; lane < count; ++lane)
            {
                const int index = findVoiceToAllocate();

                if (index < 0)
                    break;

                auto& voice = voices[index];
                voice = Voice {};
                voice.active = true;
                voice.key = std::clamp (key, 0, 127);
                voice.inputKey = std::clamp (originalKey, 0, 127);
                voice.mode = std::clamp (mode, 0, 3);
                voice.velocity = std::clamp (velocity, 0.0f, 1.0f);
                voice.regionStart = region.start;
                voice.regionEnd = region.start + region.length;
                voice.envStart = startSample;
                voice.nextGrainSample = startSample;
                voice.noteId = noteId;
                voice.startOrder = nextStartOrder++;
                voice.cloudLane = mode == 2 ? lane : -1;
                
                // Initialize surface state for this voice
                voice.surfaceState.sampleCounter = (std::uint64_t) noteId * cloudLanesPerNote + lane;
                voice.surfaceState.phaseDust = 0.0;
                voice.surfaceState.phaseCrackle = 0.0;
                voice.surfaceState.phaseWow = 0.0;
                voice.surfaceState.phaseFlutter = 0.0;

                if (firstAllocated < 0)
                    firstAllocated = index;
            }

            return firstAllocated;
        }

        /** Start release for one voice. Normal releases remain active until their envelope finishes. */
        void releaseVoice (int index, double sampleIndex, double tapeStopTimeMs, int tapeStopCurve) noexcept
        {
            if (index < 0 || index >= maxVoices)
                return;

            auto& voice = voices[index];

            if (! voice.active)
                return;

            voice.noteHeld = false;
            voice.releaseStart = sampleIndex;

            if (tapeStopTimeMs > 0.0)
            {
                voice.tapeStopping = true;
                const auto curve = static_cast<TapeStopRamp::Curve> (
                    std::clamp (tapeStopCurve, 0, static_cast<int> (TapeStopRamp::Curve::slowThenFast)));
                voice.tapeStop.start (tapeStopTimeMs, curve);
            }
            else if (voice.envEnd < 0.0)
            {
                voice.releaseLevel = voice.envValue;
                voice.envEnd = sampleIndex;
            }
        }

        /** Release every lane/voice matching the original, unquantised input key. */
        void releaseKey (int inputKey, double sampleIndex, double tapeStopTimeMs, int tapeStopCurve) noexcept
        {
            for (int i = 0; i < maxVoices; ++i)
                if (voices[i].active && voices[i].inputKey == inputKey)
                    releaseVoice (i, sampleIndex, tapeStopTimeMs, tapeStopCurve);
        }

        int countActive() const noexcept
        {
            int count = 0;
            for (int idx = 0; idx < maxVoices; ++idx)
                if (voices[idx].active)
                    ++count;

            return count;
        }

    private:
        std::uint32_t nextNoteId = 1;
        std::uint32_t nextStartOrder = 1;

        static Region makeRegion (int key, double spanPercent, double lengthPercent,
                                  int mode, int sliceCount) noexcept
        {
            if (mode == 1)
            {
                const int count = std::clamp (sliceCount, 2, maxRegions);
                const int slice = std::clamp (key * count / maxKeys, 0, count - 1);
                return chopRegion (slice, count);
            }

            return keyRegion (key, 60, spanPercent / 100.0, lengthPercent / 100.0);
        }

        int findVoiceToAllocate() noexcept
        {
            for (int i = 0; i < maxVoices; ++i)
                if (! voices[i].active)
                {
                    // Reset sustain state for reuse
                    voices[i].sustainActive = false;
                    voices[i].sustainKey = 60;
                    voices[i].sustainTimeout = 0.0;

                    return i;
                }

            int candidate = -1;
            std::uint32_t oldest = std::numeric_limits<std::uint32_t>::max();

            for (int i = 0; i < maxVoices; ++i)
                if (voices[i].tapeStopping && voices[i].startOrder < oldest)
                {
                    candidate = i;
                    oldest = voices[i].startOrder;
                }

            if (candidate >= 0)
                return candidate;

            double oldestRelease = std::numeric_limits<double>::infinity();

            for (int i = 0; i < maxVoices; ++i)
                if (voices[i].releaseStart >= 0.0 && voices[i].releaseStart < oldestRelease)
                {
                    candidate = i;
                    oldestRelease = voices[i].releaseStart;
                }

            if (candidate >= 0)
                return candidate;

            oldest = std::numeric_limits<std::uint32_t>::max();

            for (int i = 0; i < maxVoices; ++i)
                if (voices[i].startOrder < oldest)
                {
                    candidate = i;
                    oldest = voices[i].startOrder;
                }

            return candidate;
        }
    };

    /** Linear interpolation with safe endpoints and no out-of-bounds reads. */
    inline float readLinear (const float* samples, int numSamples, double position) noexcept
    {
        if (samples == nullptr || numSamples <= 0 || position < 0.0 || position > numSamples - 1)
            return 0.0f;

        const int index = static_cast<int> (position);
        const int next = std::min (index + 1, numSamples - 1);
        const float fraction = static_cast<float> (position - index);
        return samples[index] + fraction * (samples[next] - samples[index]);
    }

    inline double wrapPosition (double position, double start, double end) noexcept
    {
        const double length = end - start;

        if (length <= 0.0)
            return start;

        while (position >= end)
            position -= length;

        while (position < start)
            position += length;

        return position;
    }

    /** Deterministic cloud-grain position. Spread zero still gives four distinct lane centres. */
    inline double cloudGrainPosition (std::uint32_t noteId, std::uint32_t grainIndex, int lane,
                                      double regionStart, double regionEnd, double spread,
                                      int numSamples) noexcept
    {
        const double centre = (static_cast<double> (lane) + 0.5) / cloudLanesPerNote;
        const double jitter = CoreDSP::Sequencer::hashUnit (noteId, grainIndex * 2u + 1u) - 0.5;
        const double normalised = std::clamp (centre + jitter * std::clamp (spread, 0.0, 1.0), 0.0, 1.0);
        return (regionStart + (regionEnd - regionStart) * normalised) * numSamples;
    }

    inline double cloudLaneDetune (int lane) noexcept
    {
        return 1.0 + (static_cast<double> (lane) - 1.5) * 0.003;
    }

    /** Ghost note description retained for CoreDSP users; the processor owns its fixed pending queue. */
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
