#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

/**
    LaneView — one percussion lane's step strip (plan 19 Phase 3, a plugin-local widget
    because GrooveMutator is its only consumer).

    Draws the lane's pattern over up to CoreDSP::Rhythm::maxSteps cells: a filled box is a
    hit, a brighter box an accent, a ring a ghost, a hollow ring a fill addition and a hole
    a flam (all read from the same hash rules the scheduler uses). The playhead column marks
    the step the lane is on right now.

    Draw mode: clicking a cell paints or erases the step while the lane's pattern is
    Steps (kind 2); in Euclid and Hash modes the strip is read-only (the pattern comes
    from the maths, not from a mask).

    Preview strips (owner findings round 4, finding 2): constructed with previewStrip =
    true they are the same painter as a non-editable miniature — no pointer cursor, no
    mouse events, no "draw" affordance. The LANES page stacks eight of them as a compact
    read-only STEP GRID preview.
*/
class LaneView : public juce::Component
{
public:
    LaneView (GrooveMutatorProcessor& processor, int track, juce::Colour accentColour,
              bool previewStrip = false);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    GrooveMutatorProcessor& processor;
    const int track;
    juce::Colour accent;
    const bool preview;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LaneView)
};
