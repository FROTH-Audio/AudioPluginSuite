#pragma once

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>

/**
    2-way Linkwitz-Riley split for the Multiband Saturation plugin (plan 06 idea 2.4).

    One LR4 (24 dB/oct) filter produces both bands of one channel. The low and high outputs
    sum back to an all-pass version of the input: the magnitude is the input's magnitude at
    every frequency, so recombining the bands never dips or bumps at the crossover (the phase
    near the crossover is rotated, which is the documented LR behaviour — not phase-linear).
*/
namespace CoreDSP::Multiband
{
    class TwoBandSplit
    {
    public:
        void prepare (double sampleRate, float crossoverHz)
        {
            filter.prepare (juce::dsp::ProcessSpec { sampleRate, 1, 2 });
            filter.setCutoffFrequency (juce::jlimit (20.0f, 20000.0f, crossoverHz));
            filter.reset();
        }

        void setCrossoverHz (float crossoverHz)
        {
            filter.setCutoffFrequency (juce::jlimit (20.0f, 20000.0f, crossoverHz));
        }

        void reset() { filter.reset(); }

        /** Splits one sample of one channel (0 = left, 1 = right). Low + high carries the
            input's magnitude through the crossover's all-pass phase. */
        void process (int channel, float sample, float& low, float& high) noexcept
        {
            filter.processSample (channel, sample, low, high);
        }

    private:
        juce::dsp::LinkwitzRileyFilter<float> filter;
    };
}
