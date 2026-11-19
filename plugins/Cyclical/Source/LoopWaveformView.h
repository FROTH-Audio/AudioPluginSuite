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

    /** The active voice's playhead over the fresh span, 0..1 (inactive: no line). */
    void setPlayhead (float normalised, bool active);

    /** True when the slot's saved loop could not be resolved anywhere. */
    void setMissing (bool missing);

    /** Called when the LOAD button or the missing overlay is clicked. */
    std::function<void()> onLoadClicked;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void rebuildEnvelope();

    juce::Colour accent;
    const LoadedLoop* cachedLoop = nullptr;
    std::vector<float> minima, maxima;

    float regionStart = 0.0f, regionLength = 100.0f;
    float playhead = 0.0f;
    bool playheadActive = false;
    bool missing = false;

    juce::Rectangle<int> plotBounds;
    juce::TextButton loadButton { "LOAD" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopWaveformView)
};
