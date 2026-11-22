#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

#include "LoopSlot.h"

/**
    One slot's loop waveform (plan 6.1, plugin-local for Phase 1; a future refactor can fold
    it into suite_ui with Sgraffito's waveform view).

    Draws the loop's min/max envelope, the region band (resolved with the shared
    CoreDSP::Loop::regionFrames helper, so the band and the engine agree), the playhead over
    the fresh span, a LOAD button at the top-right and a "loop missing" overlay with
    click-to-locate. The envelope is cached per loaded loop, so the 60 Hz timer only redraws.
*/
class LoopWaveformView : public juce::Component
{
public:
    explicit LoopWaveformView (juce::Colour accentColour);

    /** Message thread. The loop pointer is the cache key: the envelope is recomputed only
        when the slot installs a different loop. */
    void setLoop (const LoadedLoop* loop);

    /** The region band, as the parameters resolve it (Start/Length percents). */
    void setRegion (float startPercent, float lengthPercent);

    /** The chop lines (plan 14 Phase 5): the slice boundaries as shares 0..1 of the
        region, from the same function the processor plays (so display and audio agree).
        Empty when the slot is unchopped. */
    void setChopLines (const std::vector<float>& boundaryShares);

    /** The arrangement strip (plan 16 Phase 2): when the slot is chopped, a row of step
        cells along the bottom of the region band, one per key slice, each showing the
        source slice it plays. A click cycles that cell's number. */
    void setSliceOrder (const std::vector<int>& order, int slices);

    /** The active voice's playhead over the fresh span, 0..1 (inactive: no line). */
    void setPlayhead (float normalised, bool active);

    /** True when the slot's saved loop could not be resolved anywhere. */
    void setMissing (bool missing);

    /** Called when the LOAD button or the missing overlay is clicked. */
    std::function<void()> onLoadClicked;

    /** Called when the AUTO button is clicked (plan 16: the transient auto-chop). The
        editor no-ops it while chop is off. */
    std::function<void()> onAutoClicked;

    /** The arrangement strip's click (plan 16 Phase 2): the key-slice index the owner
        clicked. The editor forwards it to the processor's cycleSliceOrder. */
    std::function<void (int index)> onOrderCellClicked;

    /** The AUTO button is dimmed while chop is off (the plan 10 G1 rule). */
    void setAutoEnabled (bool enabled);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void rebuildEnvelope();

    juce::Colour accent;
    const LoadedLoop* cachedLoop = nullptr;
    std::vector<float> minima, maxima;

    float regionStart = 0.0f, regionLength = 100.0f;
    std::vector<float> chopLineShares;   // slice boundaries as shares of the region (plan 14)
    std::vector<int> sliceOrder;         // the arrangement (plan 16); empty = identity
    int orderSlices = 0;                 // the slice count the strip was built with
    float playhead = 0.0f;
    bool playheadActive = false;
    bool missing = false;

    juce::Rectangle<int> plotBounds;
    juce::TextButton loadButton { "LOAD" };
    juce::TextButton autoButton { "AUTO" };

    /** The arrangement cell's bounds under a position (plan 16 Phase 2), or -1. */
    int orderCellAt (juce::Point<int> position) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopWaveformView)
};
