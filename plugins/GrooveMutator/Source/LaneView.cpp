#include "LaneView.h"

#include "SuiteLookAndFeel.h"

namespace
{
    juce::Colour dimmed (juce::Colour c)
    {
        return c.withAlpha (0.45f);
    }
}

LaneView::LaneView (GrooveMutatorProcessor& p, int trackIndex, juce::Colour accentColour,
                     bool previewStrip)
    : processor (p),
      track (trackIndex),
      accent (accentColour),
      preview (previewStrip)
{
    if (preview)
    {
        // The preview strip never answers the mouse (finding 2: non-editable).
        setInterceptsMouseClicks (false, false);
    }
    else
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
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
        // Plan 22, Finding 4: a rounded box per cell is the enabled step's shape, inset so
        // the 4-step dividers still read in the gutters.
        const auto box = cell.reduced (2.0f, 2.0f);
        constexpr float boxCorner = 3.0f;
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
            // Beyond the lane's Steps count: a dim cell body plus the boundary divider, so
            // the count reads at a glance.
            g.setColour (SuiteUI::Colours::track.withAlpha (0.16f));
            g.fillRoundedRectangle (box, boxCorner);
            g.setColour (SuiteUI::Colours::track.withAlpha (0.35f));
            g.drawVerticalLine ((int) (cell.getX() + cellWidth), strip.getY(), strip.getBottom());
            continue;
        }

        if (! fires)
        {
            // An off step is a faint box: the off/enabled silhouette is the dominant signal.
            g.setColour (SuiteUI::Colours::track.withAlpha (stepsMode ? 0.6f : 0.45f));
            g.fillRoundedRectangle (box, boxCorner);
            continue;
        }

        // A fired step: a filled box = hit, brighter = accent, a ring = ghost, a hollow
        // ring = fill addition, and the flam punches a hole (or a dot inside a ring).
        const bool isFill = ((fills >> step) & 1u) != 0u;
        const bool isAccent = processor.laneAccentsAt (track, step);
        const bool isGhost = ! isAccent && processor.laneGhostsAt (track, step);
        const bool isFlam = processor.laneFlamsAt (track, step);

        if (isFill)
        {
            g.setColour (accent);
            g.drawRoundedRectangle (box.reduced (0.8f), boxCorner, 1.6f);
        }
        else if (isGhost)
        {
            g.setColour (dimmed (accent));
            g.drawRoundedRectangle (box.reduced (0.8f), boxCorner, 1.6f);
        }
        else
        {
            g.setColour (isAccent ? accent.brighter (0.45f) : accent);
            g.fillRoundedRectangle (box, boxCorner);
        }

        if (isFlam)
        {
            const auto inner = box.reduced (4.5f);
            g.setColour ((isFill || isGhost) ? (isGhost ? dimmed (accent).brighter (0.15f) : accent)
                                             : SuiteUI::Colours::panel);
            g.fillRoundedRectangle (inner, 2.0f);
        }
    }

    // The Steps-mode affordance: the strip answers the draw-mode rule (plan 19 Phase 3).
    // The preview strips skip it (they are never editable).
    if (stepsMode && ! preview)
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
