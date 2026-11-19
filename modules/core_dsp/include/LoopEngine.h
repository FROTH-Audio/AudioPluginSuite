#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "SequencerEngine.h"

/**
    Loop playback maths for the Cyclical four-slot loop instrument (plan 6.1).

    Pure playback rules, no JUCE GUI, no allocation on the audio thread. A Voice captures its
    Source (pointers, counts and rate) at start() and holds it until it goes inactive, so a
    mid-voice loop swap stays valid through the processor's retirement queue. The host rate is
    read per block through prepare(), so a rate change mid-note keeps playing at the right
    speed with no reload.

    Rules the implementation and tests share:

    - Rate = region.speed x pitchRatio x sourceSampleRate / hostSampleRate. In granular mode
      speed is ignored and the rate is pitchRatio x sourceSampleRate / hostSampleRate.
    - Forward (crossfade X = min(crossfadeFrames, lengthFrames / 2)): the fresh span is
      [start + X, end); the last X frames blend the tail with the head read at
      start + (p - (end - X)), weight t = (p - (end - X)) / X rising 0 to 1, and playback
      resumes at start + X. [start, start + X) is a pre-roll heard only inside the blend.
      At X = 0 this is the plain hard wrap end -> start, bit-exact.
    - Reverse is exactly the forward rule in the mirrored coordinate q = (end - 1) - p: the
      traversal runs the same X-crossfaded cycle, and every read is mirrored into the source.
      In q-space the traversal and the blend are one shared implementation (traverse() below);
      only the read differs.
    - Ping-pong flips direction at both bounds with no wrap, so no seam blend is applied.
      The reflection does not repeat the boundary sample: p = 2*bound - p (then continue).
    - Crossfade law: linear. For correlated material (a well-made loop's seam) the blend sums
      to unity, so there is no level dip; for uncorrelated material it dips to -3 dB at the
      midpoint. Deliberate: equal-power would boost correlated content by +3 dB, which is
      worse for loops.
    - Ping random: drawn once per trigger from hashUnit(triggerSeed, 0/1), it insets the two
      turn-around bounds by a, b in [0, 0.25) x lengthFrames. The rate is untouched, so the
      cycle time changes while the pitch stays put. With pingRandom off, a = b = 0.
    - Granular: kGrainMs grains, one at a time per voice, half-sine windowed. The first grain
      starts at start (a deterministic, audible attack); later grains start from
      hashUnit(triggerSeed, grainIndex) inside the region and wrap within it, reading forward
      at the pitch-shifted rate. speed, direction and ping random are ignored in granular
      mode; start/length define the region.
    - Gain: noteVelocity x slotVelocity, applied at start() with a 5 ms attack ramp, so a
      velocity change is heard from the next trigger, not mid-note. release() fades 5 ms,
      cut() fades 2 ms, then the voice goes inactive.
    - No reads outside the region or the buffer: the caller clamps the region; the engine
      clamps again defensively.
    - Mono sources read channel 0 and feed every output channel (the Sgraffito rule; never
      index the source by the output channel number).
*/
namespace CoreDSP::Loop
{
    enum class Direction { forward, reverse, pingPong };

    /** Grain length in milliseconds (plan 6.1: 12.5 grains/s is a stutter, not a cloud). */
    inline constexpr float kGrainMs = 80.0f;

    /** Attack and fade times, converted at the host rate. */
    inline constexpr double kAttackMs = 5.0;
    inline constexpr double kReleaseMs = 5.0;
    inline constexpr double kCutMs = 2.0;

    /** A non-owning view of the loaded loop. The processor owns the buffer. */
    struct Source
    {
        const float* const* channels = nullptr;   // one read pointer per source channel
        int    numChannels = 0;
        int    numFrames   = 0;
        double sourceSampleRate = 0.0;
    };

    /** The per-slot playback settings, already resolved from the parameters. */
    struct Region
    {
        int    startFrame   = 0;
        int    lengthFrames = 1;                  // effective length, already clamped by the caller
        double speed = 1.0;                       // 0.25 .. 4.0
        Direction direction = Direction::forward;
        bool   granular = false;
        bool   pingRandom = false;                // inset the ping-pong turn bounds per trigger
        int    crossfadeFrames = 0;               // 0 .. 500 ms, converted at the host rate
    };

    /** Resolves Start/Length percents to frames inside a loaded loop (plan 6.1). Start is
        capped at 95 %, the effective length is min(Length, 100 - Start), and the pair is
        clamped into the buffer. The editor's waveform view calls the same helper, so the
        region band and the engine agree (the keyRegion/chopRegion precedent). */
    inline std::array<int, 2> regionFrames (int numFrames, float startPercent, float lengthPercent) noexcept
    {
        numFrames = std::max (1, numFrames);

        const float start = std::clamp (startPercent, 0.0f, 95.0f);
        const float length = std::clamp (std::min (lengthPercent, 100.0f - start), 5.0f, 100.0f);

        const int startFrame = std::min ((int) std::lround (start * 0.01f * (float) numFrames), numFrames - 1);
        const int lengthFrames = std::min (std::max ((int) std::lround (length * 0.01f * (float) numFrames), 1),
                                           numFrames - startFrame);

        return { startFrame, lengthFrames };
    }

    /** One playing loop: position, direction, the seam crossfade and the envelope. */
    class Voice
    {
    public:
        /** Sets the host rate the voice renders at. Called per block, so a host rate change
            mid-note keeps the speed right with no reload. Does not clear any state. */
        void prepare (double hostSampleRate) noexcept
        {
            if (hostSampleRate > 0.0)
                hostSampleRateHz = hostSampleRate;
        }

        /** Starts playback. gain is the note velocity x the slot velocity; triggerSeed
            drives the ping-random turn points and the granular grain order. */
        void start (const Source& newSource, const Region& newRegion, double pitchRatio,
                    double newGain, std::uint32_t newTriggerSeed) noexcept
        {
            capturedSource = newSource;
            region = newRegion;
            pitchRatio = std::clamp (pitchRatio, 0.03125, 32.0);
            gain = std::max (0.0, newGain);
            triggerSeed = newTriggerSeed;

            // Defensive clamp: the caller resolved the region, but never read outside.
            region.startFrame = std::clamp (region.startFrame, 0, std::max (0, capturedSource.numFrames - 1));
            region.lengthFrames = std::max (1, std::min (region.lengthFrames, capturedSource.numFrames - region.startFrame));

            crossfade = std::min (std::max (region.crossfadeFrames, 0), region.lengthFrames / 2);
            attackSamples = std::max (1, (int) std::lround (kAttackMs * 0.001 * hostSampleRateHz));
            releaseSamples = std::max (1, (int) std::lround (kReleaseMs * 0.001 * hostSampleRateHz));
            cutSamples = std::max (1, (int) std::lround (kCutMs * 0.001 * hostSampleRateHz));
            grainSamples = std::max (1, (int) std::lround (kGrainMs * 0.001 * hostSampleRateHz));

            active = true;
            fadingSamples = 0;
            fadeDuration = 1;
            envRamp = 0;
            grainIndex = 0;
            grainPosition = 0;

            // Turn bounds for ping-pong, drawn once per trigger (0 when pingRandom is off).
            // The rate is untouched, so the cycle time changes while the pitch stays put.
            turnA = 0.0;
            turnB = 0.0;

            if (region.direction == Direction::pingPong && region.pingRandom)
            {
                // Whole-frame insets, so the reflection keeps the traversal integral and
                // the turn is a clean palindrome at rate 1.
                const double length = (double) region.lengthFrames;
                turnA = std::floor ((double) Sequencer::hashUnit (triggerSeed, 0) * 0.25 * length);
                turnB = std::floor ((double) Sequencer::hashUnit (triggerSeed, 1) * 0.25 * length);
            }

            if (region.granular)
            {
                // One grain at a time. The first grain starts at the region's start.
                position = (double) region.startFrame;
                grainPosition = 0;
                return;
            }

            if (region.direction == Direction::pingPong)
            {
                position = (double) region.startFrame + turnA;
                direction = 1;
                return;
            }

            // Forward and reverse share the q-space traversal; both fresh spans start X
            // frames in (the mirrored span in reverse), so the pre-roll is heard only
            // inside the seam blend.
            position = (double) crossfade;
        }

        /** Gate note-off: a 5 ms fade, then inactive. Latch mode ignores it. */
        void release() noexcept
        {
            if (active && fadingSamples == 0)
            {
                fadingSamples = releaseSamples;
                fadeDuration = releaseSamples;
            }
        }

        /** Retrigger: a 2 ms fade, then inactive. */
        void cut() noexcept
        {
            if (active && fadingSamples == 0)
            {
                fadingSamples = cutSamples;
                fadeDuration = cutSamples;
            }
        }

        void reset() noexcept
        {
            active = false;
            fadingSamples = 0;
            fadeDuration = 1;
            position = 0.0;
        }

        bool isActive() const noexcept { return active; }

        const Source* source() const noexcept { return &capturedSource; }

        /** 0..1 over the fresh span, for the UI. */
        double playheadNormalised() const noexcept
        {
            const double freshStart = freshSpanStart();
            const double freshLength = std::max (1.0, freshSpanEnd() - freshStart);
            return std::clamp ((position - freshStart) / freshLength, 0.0, 1.0);
        }

        /** Renders into the output buffer, mixing (adding) so several voices share it. */
        void render (juce::AudioBuffer<float>& out, int startSample, int numSamples) noexcept
        {
            if (! active)
                return;

            const int renderSamples = std::min (numSamples, out.getNumSamples() - startSample);
            if (renderSamples <= 0 || out.getNumChannels() == 0)
                return;

            // The rate is recomputed per render from the current host rate.
            const double speed = region.granular ? 1.0 : region.speed;
            rate = std::clamp (speed * pitchRatio * capturedSource.sourceSampleRate / hostSampleRateHz,
                               1.0e-6, 64.0);

            for (int n = 0; n < renderSamples; ++n)
            {
                // Envelope: a 5 ms attack on start, then 1; a linear fade on release/cut.
                double level = gain;

                if (envRamp < attackSamples)
                    level *= (double) envRamp / (double) attackSamples;

                if (fadingSamples > 0)
                    level *= (double) fadingSamples / (double) fadeDuration;

                // The traversal advances every output sample, muted or not, so the loop
                // keeps time through the attack and the fades.
                if (region.granular)
                {
                    if (level > 0.0)
                    {
                        const double phase = (double) grainPosition / (double) grainSamples;
                        const double window = 0.5 * (1.0 - std::cos (2.0 * juce::MathConstants<double>::pi * phase));
                        const double sample = readRegion (position) * window * level;

                        for (int ch = 0; ch < out.getNumChannels(); ++ch)
                            out.addSample (ch, startSample + n, (float) sample);
                    }

                    advanceGrain();
                }
                else
                {
                    if (level > 0.0)
                    {
                        const double sample = readTraversal (position) * level;

                        for (int ch = 0; ch < out.getNumChannels(); ++ch)
                            out.addSample (ch, startSample + n, (float) sample);
                    }

                    traverse (rate);
                }

                if (envRamp < attackSamples)
                    ++envRamp;

                if (fadingSamples > 0 && --fadingSamples == 0)
                {
                    active = false;
                    return;
                }
            }
        }

    private:
        //---- Loop traversal (forward, reverse, ping-pong) ----

        /** Reads the traversal coordinate: mirrored for reverse, direct otherwise. The seam
            blend (forward and reverse only) mixes the traversal head with its partner. */
        double readTraversal (double q) const noexcept
        {
            const double main = readAt (q);

            if (inSeamBlend())
            {
                const double partner = readAt (q - (double) (region.lengthFrames - crossfade));
                const double t = (double) crossfade > 0.0
                    ? std::clamp ((q - (double) (region.lengthFrames - crossfade)) / (double) crossfade, 0.0, 1.0)
                    : 1.0;
                return main * (1.0 - t) + partner * t;
            }

            return main;
        }

        bool inSeamBlend() const noexcept
        {
            return region.direction != Direction::pingPong && ! region.granular
                && position >= (double) (region.lengthFrames - crossfade);
        }

        /** Advances the traversal by one step of the rate, wrapping or reflecting. */
        void traverse (double step) noexcept
        {
            if (region.direction == Direction::pingPong)
            {
                const double lower = (double) region.startFrame + turnA;
                const double upper = (double) (region.startFrame + region.lengthFrames - 1) - turnB;

                position += (double) direction * step;

                // Reflect at both bounds without repeating the boundary sample.
                if (direction > 0 && position > upper)
                {
                    position = 2.0 * upper - position;
                    direction = -1;
                }
                else if (direction < 0 && position < lower)
                {
                    position = 2.0 * lower - position;
                    direction = 1;
                }

                position = std::clamp (position, lower, upper);
                return;
            }

            // Forward and reverse share this q-space cycle over [0, length):
            // fresh span [X, length), seam blend over the last X frames, resume at X.
            position += step;

            if (position >= (double) region.lengthFrames)
                position -= (double) (region.lengthFrames - crossfade);
        }

        /** One frame of the source at a traversal coordinate, linearly interpolated.
            Ping-pong runs in real frame coordinates; reverse mirrors into the source:
            r[q] = source[(end - 1) - q]. */
        double readAt (double q) const noexcept
        {
            if (region.direction == Direction::pingPong)
            {
                const int base = (int) std::floor (q);
                const double a = readFrame (base);
                const double b = readFrame (base + 1);
                return a + (b - a) * (q - (double) base);
            }

            q = std::clamp (q, 0.0, std::nextafter ((double) region.lengthFrames, 0.0));

            const int base = (int) std::floor (q);
            const double frac = q - (double) base;

            if (region.direction == Direction::reverse)
            {
                const int end = region.startFrame + region.lengthFrames;
                const double a = readFrame ((end - 1) - base);
                const double b = readFrame ((end - 1) - base - 1);
                return a + (b - a) * frac;
            }

            const double a = readFrame (region.startFrame + base);
            const double b = readFrame (region.startFrame + base + 1);
            return a + (b - a) * frac;
        }

        /** One frame of the source, clamped into the region and the buffer. Mono sources
            read channel 0; the caller copies the read to every output channel. */
        double readFrame (int frame) const noexcept
        {
            frame = std::clamp (frame, region.startFrame,
                                region.startFrame + region.lengthFrames - 1);
            frame = std::clamp (frame, 0, std::max (0, capturedSource.numFrames - 1));
            return (double) capturedSource.channels[(size_t) 0][frame];
        }

        //---- Granular ----

        /** Advances the grain machine by one output sample: wraps to the next grain when
            the 80 ms window has passed, reading forward at the pitch-shifted rate (speed
            is ignored) and wrapping within the region. */
        void advanceGrain() noexcept
        {
            if (grainPosition >= grainSamples)
                startNextGrain();

            position += pitchRatio * capturedSource.sourceSampleRate / hostSampleRateHz;

            if (position >= (double) (region.startFrame + region.lengthFrames))
                position -= (double) region.lengthFrames;

            ++grainPosition;
        }

        /** Reads inside the region with linear interpolation, clamped into it. */
        double readRegion (double p) const noexcept
        {
            const double relative = std::clamp (p - (double) region.startFrame, 0.0,
                                                (double) region.lengthFrames - 1.0e-9);
            const int base = (int) std::floor (relative);
            const double frac = relative - (double) base;

            const double a = readFrame (region.startFrame + base);
            const double b = readFrame (region.startFrame + base + 1);
            return a + (b - a) * frac;
        }

        /** Later grains start from hashUnit(triggerSeed, grainIndex) inside the region. */
        void startNextGrain() noexcept
        {
            ++grainIndex;
            grainPosition = 0;

            const double unit = (double) Sequencer::hashUnit (triggerSeed, (std::uint32_t) grainIndex);
            position = (double) region.startFrame + unit * (double) (region.lengthFrames - 1);
        }

        //---- Playhead ----

        // The playhead coordinate matches `position`: q-space for forward and reverse
        // (the traversal cycle), real frame coordinates for ping-pong and granular.
        double freshSpanStart() const noexcept
        {
            if (region.granular)
                return (double) region.startFrame;

            if (region.direction == Direction::pingPong)
                return (double) region.startFrame + turnA;

            return (double) crossfade;
        }

        double freshSpanEnd() const noexcept
        {
            if (region.granular)
                return (double) (region.startFrame + region.lengthFrames);

            if (region.direction == Direction::pingPong)
                return (double) (region.startFrame + region.lengthFrames - 1) - turnB;

            return (double) region.lengthFrames;
        }

        // Captured at start(), held until the voice goes inactive (the lifetime rule).
        // Named capturedSource: the accessor is source().
        Source capturedSource;
        Region region;

        double hostSampleRateHz = 48000.0;
        double pitchRatio = 1.0;
        double gain = 1.0;
        std::uint32_t triggerSeed = 0;

        // The traversal coordinate. Forward and reverse run it over [0, length) in q-space
        // (reverse mirrors its reads); ping-pong runs it in real frame coordinates.
        double position = 0.0;
        double rate = 1.0;
        int direction = 1;

        int crossfade = 0;

        // Granular state.
        int grainIndex = 0;
        int grainPosition = 0;
        int grainSamples = 1;

        // Ping-pong turn insets, drawn once per trigger (0 when pingRandom is off).
        double turnA = 0.0;
        double turnB = 0.0;

        // Envelope state.
        int attackSamples = 1;
        int releaseSamples = 1;
        int cutSamples = 1;
        int fadeDuration = 1;
        int envRamp = 0;
        int fadingSamples = 0;
        bool active = false;
    };
}
