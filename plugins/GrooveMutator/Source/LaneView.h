#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

/**
    LaneView — one percussion lane's step strip (plan 19 Phase 3, a plugin-local widget
    because GrooveMutator is its only consumer).

    Draws the lane's pattern over up to CoreDSP::Rhythm::maxSteps cells: a full dot is a
    hit, a bright dot an accent, a ring a ghost, a double dot a flam (all read from the
    same hash rules the scheduler uses). The playhead column marks the step the lane is
    on right now.

    Draw mode: clicking a cell paints or erases the step while the lane's pattern is
    Steps (kind 2); in Euclid and Hash modes the strip is read-only (the pattern comes
    from the maths, not from a mask).
*/
class LaneView : public juce::Component
{
public:
    LaneView (GrooveMutatorProcessor& processor, int track, juce::Colour accentColour);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    GrooveMutatorProcessor& processor;
    const int track;
    juce::Colour accent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LaneView)
};
