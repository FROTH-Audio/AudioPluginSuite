#pragma once

/**
    Mid/side stereo maths shared by the suite (plan 06 idea 2.9, Stereo Tool Effect).

    The encode/decode pair is exactly lossless: decode(encode(l, r)) == (l, r) for every finite
    input pair. The width control scales the side component only, so a width of 0 is a no-op and
    the range is guarded so the decode can never fold energy (plan risk: width beyond the guard).

    Pure functions, no JUCE dependency, so they test the same on every platform.
*/
#include <algorithm>
#include <cmath>

namespace CoreDSP::Stereo
{
    /** Mid/side encode: mid = (l + r) / 2, side = (l - r) / 2. */
    inline void encode (float left, float right, float& mid, float& side) noexcept
    {
        mid  = 0.5f * (left + right);
        side = 0.5f * (left - right);
    }

    /** Mid/side decode: left = mid + side, right = mid - side. Inverts encode exactly. */
    inline void decode (float mid, float side, float& left, float& right) noexcept
    {
        left  = mid + side;
        right = mid - side;
    }

    /** The side multiplier for a width in [-1, 1]: -1 folds to mono, 0 leaves the image
        unchanged, +1 doubles the side energy. Anything outside [-1, 1] is clamped, so the
        decode never folds (a side gain below 0 inverts the image, below -1 it would fold). */
    inline float sideGain (float width) noexcept
    {
        return 1.0f + std::clamp (width, -1.0f, 1.0f);
    }

    /** Drive-style saturation: interpolates from the clean signal towards tanh (gain * x).
        amount 0 (and any negative amount) is the identity, so Drive 0 dB is transparent;
        amount 1 is the full tanh drive, where loud parts sit near the rails. The gain is the
        linear pre-gain (decibelsToGain of the Drive parameter), guarded for tiny gains. */
    inline float saturate (float x, float amount01, float gain) noexcept
    {
        if (amount01 <= 0.0f)
            return x;
        const float a = std::clamp (amount01, 0.0f, 1.0f);
        if (gain <= 1.0e-6f)
            return x;
        return x + a * (std::tanh (gain * x) - x);
    }
}
