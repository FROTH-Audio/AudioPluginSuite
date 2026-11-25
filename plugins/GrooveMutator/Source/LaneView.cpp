#include "LaneView.h"

#include "SuiteLookAndFeel.h"

namespace
{
    juce::Colour dimmed (juce::Colour c)
    {
        return c.withAlpha (0.45f);
    }
}

LaneView::LaneView (GrooveMutatorProcessor& p, int trackIndex, juce::Colour accentColour)
    : processor (p),
      track (trackIndex),
      accent (accentColour)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setOpaque (false);
}

void LaneView::paint (juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();

    g.setColour (SuiteUI::Colours::panel.withAlpha (0.55f));
    g.fillRoundedRectangle (full, 6.0f);
    g.setColour (SuiteUI::Colours::track);
    g.drawRoundedRectangle (full, 6.0f, 1.0f);

    const int steps = juce::jlimit (1, CoreDSP::Rhythm::maxSteps,
                                    juce::roundToInt (processor.apvts.getRawParameterValue (
                                        GrooveMutatorProcessor::trackStepsIds[(size_t) track])->load()));
    const int patternKind = juce::jlimit (0, CoreDSP::Rhythm::patternKinds - 1,
                                          juce::roundToInt (processor.apvts.getRawParameterValue (
                                              GrooveMutatorProcessor::trackPatternIds[(size_t) track])->load()));
    const int seedValue = juce::roundToInt (processor.apvts.getRawParameterValue (
        GrooveMutatorProcessor::seedId)->load());
    const bool stepsMode = patternKind == 2;

    // In Steps mode the painted (editable) mask is the truth; otherwise the mutated
    // live mask the scheduler fires from. The fill additions are audible but not
    // painted (plan 21, C3): the strip draws them as hollow dots so it tells the truth.
    const std::uint32_t painted = processor.getLaneStepsMask (track);
    const std::uint32_t mask = stepsMode ? painted : processor.laneMask (track);
    const std::uint32_t fills = stepsMode ? processor.laneFillMask (track) & ~painted : 0u;

    const auto strip = full.reduced (6.0f, (float) juce::jlimit (4, (int) full.getHeight() / 3, 6));
    const float cellWidth = strip.getWidth() / (float) CoreDSP::Rhythm::maxSteps;
    const int playhead = processor.playheadStep[track].load (std::memory_order_relaxed);

    for (int step = 0; step < CoreDSP::Rhythm::maxSteps; ++step)
    {
        auto cell = strip.withX (strip.getX() + step * cellWidth).withWidth (cellWidth);
        auto dot = cell.withSizeKeepingCentre (std::min (cellWidth - 3.0f, 11.0f),
                                               std::min (cell.getHeight() - 3.0f, 11.0f));
        const bool inside = step < steps;
        const bool fires = inside && ((mask >> step) & 1u) != 0u;

        // The playhead column: a faint wash behind the cell the lane is sitting on.
        if (playhead == step)
        {
            g.setColour (accent.withAlpha (0.13f));
            g.fillRect (cell);
        }

        // The steps grid: every 4th cell gets a dim divider, so 1/16 groups read.
        if (step % 4 == 0 && step > 0)
        {
            g.setColour (SuiteUI::Colours::track.withAlpha (0.7f));
            g.drawVerticalLine ((int) cell.getX(), strip.getY(), strip.getBottom());
        }

        if (! inside)
        {
            // Beyond the lane's Steps count: dim the cell so the count reads.
            g.setColour (SuiteUI::Colours::track.withAlpha (0.35f));
            g.drawVerticalLine ((int) (cell.getX() + cellWidth), strip.getY(), strip.getBottom());
            continue;
        }

        if (! fires)
        {
            g.setColour (SuiteUI::Colours::track.withAlpha (stepsMode ? 0.55f : 0.3f));
            g.fillEllipse (dot.reduced (2.0f));
            continue;
        }

        // A fired step: full = hit, bright = accent, ring = ghost, double = flam. A fill
        // addition is a hollow dot in the full accent (the ghost's ring is dimmed, so the
        // two read apart), with the flam's inner ring when the hash says flam.
        const bool isFill = ((fills >> step) & 1u) != 0u;
        const bool isAccent = processor.laneAccentsAt (track, step);
        const bool isGhost = ! isAccent && processor.laneGhostsAt (track, step);
        const bool isFlam = processor.laneFlamsAt (track, step);

        if (isFill)
        {
            g.setColour (accent);
            g.drawEllipse (dot, 1.4f);
        }
        else if (isGhost)
        {
            g.setColour (dimmed (accent));
            g.drawEllipse (dot, 1.4f);
        }
        else
        {
            g.setColour (isAccent ? accent.brighter (0.45f) : accent);
            g.fillEllipse (dot);
        }

        if (isFlam)
        {
            auto flamDot = dot.reduced (4.5f);
            g.setColour (isGhost ? dimmed (accent).brighter (0.15f)
                                 : (isFill ? accent : accent.brighter (0.25f)));
            g.drawEllipse (flamDot, 1.2f);
        }
    }

    // The Steps-mode affordance: the strip answers the draw-mode rule (plan 19 Phase 3).
    if (stepsMode)
    {
        g.setColour (SuiteUI::Colours::textDim);
        g.setFont (SuiteUI::uiFont (9.0f));
        g.drawText ("draw", full.withTrimmedRight (8.0f).removeFromBottom (12.0f),
                    juce::Justification::centredRight);
    }
}

void LaneView::mouseDown (const juce::MouseEvent& event)
{
    const int steps = juce::jlimit (1, CoreDSP::Rhythm::maxSteps,
                                    juce::roundToInt (processor.apvts.getRawParameterValue (
                                        GrooveMutatorProcessor::trackStepsIds[(size_t) track])->load()));
    const int patternKind = juce::jlimit (0, CoreDSP::Rhythm::patternKinds - 1,
                                          juce::roundToInt (processor.apvts.getRawParameterValue (
                                              GrooveMutatorProcessor::trackPatternIds[(size_t) track])->load()));

    // Draw mode exists only for Steps lanes (plan 19 Phase 3).
    if (patternKind != 2)
        return;

    const auto strip = getLocalBounds().toFloat().reduced (6.0f,
        (float) juce::jlimit (4, getLocalBounds().getHeight() / 3, 6));
    const float cellWidth = strip.getWidth() / (float) CoreDSP::Rhythm::maxSteps;
    const int step = juce::jlimit (0, CoreDSP::Rhythm::maxSteps - 1,
                                   (int) ((event.position.x - strip.getX()) / cellWidth));

    if (step >= steps)
        return;

    const bool on = ((processor.getLaneStepsMask (track) >> step) & 1u) == 0u;
    processor.setLaneStep (track, step, on);
    repaint();
}
