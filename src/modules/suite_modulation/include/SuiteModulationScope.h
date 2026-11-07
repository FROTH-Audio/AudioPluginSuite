/**
 * The LFO scope, used in the editor for visualising LFO cycles
 */

namespace SuiteModulation
{

/**
 * A visualiser that draws one LFO cycle.
 * Each cycle is 100 samples long, sampled at 4096 Hz (100 / 4096 ~ 24ms),
 * which is the maximum period the scope can display as a single cycle.
 */
class LfoScope
{
public:
    /**
     * Build a scope for the given LFO.
     */
    LfoScope(LFO source) : m_source(source) {}

    /**
     * Returns the colour of the LFO.
     */
    juce::Colour getColour() const noexcept
    {
        if (m_source == LFO::L1)
            return juce::Colours::cyan;
        return juce::Colours::yellow;
    }

    /**
     * Returns the LFO's shape name.
     */
    juce::String getName() const noexcept
    {
        if (m_source == LFO::L1)
            return "LFO 1";
        return "LFO 2";
    }
};