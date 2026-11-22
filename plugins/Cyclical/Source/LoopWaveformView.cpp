#include "LoopWaveformView.h"

#include "SuiteLookAndFeel.h"

LoopWaveformView::LoopWaveformView (juce::Colour accentColour)
    : accent (accentColour)
{
    loadButton.setTriggeredOnMouseDown (true);
    loadButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    loadButton.setColour (juce::TextButton::textColourOffId, SuiteUI::Colours::textDim);
    addAndMakeVisible (loadButton);

    loadButton.onClick = [this]
    {
        if (onLoadClicked)
            onLoadClicked();
    };

    autoButton.setTriggeredOnMouseDown (true);
    autoButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    autoButton.setColour (juce::TextButton::textColourOffId, SuiteUI::Colours::textDim);
    addAndMakeVisible (autoButton);

    autoButton.onClick = [this]
    {
        if (onAutoClicked)
            onAutoClicked();
    };
}

void LoopWaveformView::setAutoEnabled (bool enabled)
{
    autoButton.setEnabled (enabled);
}

void LoopWaveformView::setLoop (const LoadedLoop* loop)
{
    if (loop == cachedLoop)
        return;

    cachedLoop = loop;
    rebuildEnvelope();
    repaint();
}

void LoopWaveformView::rebuildEnvelope()
{
    minima.clear();
    maxima.clear();

    if (cachedLoop == nullptr || cachedLoop->buffer.getNumSamples() == 0)
        return;

    const auto& buffer = cachedLoop->buffer;
    const int numFrames = buffer.getNumSamples();
    const int columns = std::max (1, getWidth());

    minima.resize ((size_t) columns);
    maxima.resize ((size_t) columns);

    for (int column = 0; column < columns; ++column)
    {
        const int first = (int) ((std::int64_t) column * numFrames / columns);
        const int last = std::min (numFrames,
            (int) ((std::int64_t) (column + 1) * numFrames / columns));

        float lowest = 0.0f, highest = 0.0f;
        for (int frame = first; frame < last; ++frame)
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                const float sample = buffer.getSample (ch, frame);
                lowest = std::min (lowest, sample);
                highest = std::max (highest, sample);
            }
        }

        minima[(size_t) column] = lowest;
        maxima[(size_t) column] = highest;
    }
}

void LoopWaveformView::setRegion (float startPercent, float lengthPercent)
{
    regionStart = startPercent;
    regionLength = lengthPercent;
    repaint();
}

void LoopWaveformView::setChopLines (const std::vector<float>& boundaryShares)
{
    chopLineShares = boundaryShares;
    repaint();
}

void LoopWaveformView::setSliceOrder (const std::vector<int>& order, int slices)
{
    sliceOrder = order;
    orderSlices = slices;
    repaint();
}

void LoopWaveformView::setPlayhead (float normalised, bool active)
{
    playhead = normalised;
    playheadActive = active;
    repaint();
}

void LoopWaveformView::setMissing (bool isMissing)
{
    if (missing != isMissing)
    {
        missing = isMissing;
        repaint();
    }
}

void LoopWaveformView::resized()
{
    plotBounds = getLocalBounds();
    loadButton.setBounds (getLocalBounds().removeFromTop (22).removeFromRight (56).reduced (3, 1));
    autoButton.setBounds (getLocalBounds().removeFromTop (22).removeFromRight (112)
                              .removeFromRight (56).reduced (3, 1));
    rebuildEnvelope();
}

void LoopWaveformView::mouseDown (const juce::MouseEvent& event)
{
    // The arrangement strip's cells (plan 16 Phase 2): a click cycles that key slice's
    // number. Live only when the strip is shown (chop mode).
    if (orderSlices > 1)
    {
        const int index = orderCellAt (event.getPosition());

        if (index >= 0 && onOrderCellClicked)
        {
            onOrderCellClicked (index);
            return;
        }
    }

    // The missing overlay covers the whole view; a click on it (and not on the LOAD
    // button, which handles itself) opens the locate flow.
    if (missing && onLoadClicked && ! loadButton.getBounds().contains (event.getPosition()))
        onLoadClicked();
}

int LoopWaveformView::orderCellAt (juce::Point<int> position) const
{
    // The strip spans the region band's bottom (the same geometry paint uses).
    auto plot = getLocalBounds().toFloat().reduced (4.0f);
    const float startFraction = regionStart / 100.0f;
    const float lengthFraction = std::max (0.02f, regionLength / 100.0f);
    const auto regionArea = plot.withLeft (plot.getX() + startFraction * plot.getWidth())
                                 .withRight (plot.getX() + startFraction * plot.getWidth()
                                                 + lengthFraction * plot.getWidth());

    const auto strip = juce::Rectangle<float> (regionArea.getX(), regionArea.getBottom() - 16.0f,
                                               regionArea.getWidth(), 16.0f).reduced (2.0f, 1.0f);

    if (! strip.contains (position.toFloat()) || orderSlices <= 1)
        return -1;

    const int index = (int) ((float) (position.x - strip.getX()) / strip.getWidth() * (float) orderSlices);
    return juce::jlimit (0, orderSlices - 1, index);
}

void LoopWaveformView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (SuiteUI::Colours::panel.withAlpha (0.55f));
    g.fillRoundedRectangle (bounds, 6.0f);

    auto plot = bounds.reduced (4.0f);

    // The region band: inside Start..Start+Length is the played span.
    const float startFraction = regionStart / 100.0f;
    const float lengthFraction = std::max (0.02f, regionLength / 100.0f);
    const auto regionArea = plot.withLeft (plot.getX() + startFraction * plot.getWidth())
                                 .withRight (plot.getX() + startFraction * plot.getWidth()
                                                 + lengthFraction * plot.getWidth());

    g.setColour (SuiteUI::Colours::panelLight.withAlpha (0.6f));
    g.fillRoundedRectangle (regionArea, 4.0f);

    // The chop lines (plan 14 Phase 5): the slice boundaries as shares of the region,
    // the same values the processor plays, drawn as short vertical ticks.
    g.setColour (accent.withAlpha (0.8f));
    for (float share : chopLineShares)
    {
        const float x = regionArea.getX() + juce::jlimit (0.0f, 1.0f, share) * regionArea.getWidth();
        g.drawVerticalLine ((int) x, regionArea.getY(), regionArea.getBottom());
    }

    // The waveform envelope (cached per loop).
    if (! minima.empty() && maxima.size() == minima.size())
    {
        juce::Path top, bottom;
        const float midY = plot.getCentreY();
        const float halfHeight = plot.getHeight() * 0.45f;
        const int columns = (int) minima.size();

        for (int column = 0; column < columns; ++column)
        {
            const float x = plot.getX() + (float) column / (float) columns * plot.getWidth();
            const float high = midY - maxima[(size_t) column] * halfHeight;
            const float low = midY - minima[(size_t) column] * halfHeight;

            if (column == 0)
            {
                top.startNewSubPath (x, high);
                bottom.startNewSubPath (x, low);
            }
            else
            {
                top.lineTo (x, high);
                bottom.lineTo (x, low);
            }
        }

        g.setColour (SuiteUI::Colours::track);
        g.strokePath (top, juce::PathStrokeType (1.0f));
        g.strokePath (bottom, juce::PathStrokeType (1.0f));
    }

    // The arrangement strip (plan 16 Phase 2): one cell per key slice at the bottom of the
    // region band, showing the source slice it plays (identity reads 0 1 2 ...). Rule R6:
    // the number is the state, so the text carries it, not colour alone.
    if (orderSlices > 1)
    {
        const auto strip = juce::Rectangle<float> (regionArea.getX(), regionArea.getBottom() - 16.0f,
                                                   regionArea.getWidth(), 16.0f).reduced (2.0f, 1.0f);
        const float cellWidth = strip.getWidth() / (float) orderSlices;

        for (int i = 0; i < orderSlices; ++i)
        {
            auto cell = juce::Rectangle<float> (strip.getX() + cellWidth * (float) i, strip.getY(),
                                                cellWidth, strip.getHeight()).reduced (1.0f, 1.0f);

            g.setColour (SuiteUI::Colours::panel);
            g.fillRect (cell);
            g.setColour (SuiteUI::Colours::track);
            g.drawRect (cell, 1.0f);

            const int source = i < (int) sliceOrder.size() ? sliceOrder[(size_t) i] : i;
            g.setColour (source == i ? SuiteUI::Colours::textDim : accent);
            g.setFont (SuiteUI::uiFont (11.0f));
            g.drawText (juce::String (source), cell, juce::Justification::centred);
        }
    }

    // The playhead over the fresh span.
    if (playheadActive)
    {
        const float x = plot.getX() + playhead * plot.getWidth();
        g.setColour (accent);
        g.drawLine (x, plot.getY(), x, plot.getBottom(), 1.5f);
    }

    // The missing overlay (rule R6: accent border and text, not colour alone).
    if (missing)
    {
        g.setColour (accent);
        g.drawRoundedRectangle (bounds.reduced (1.0f), 6.0f, 1.5f);

        g.setColour (SuiteUI::Colours::text);
        g.setFont (SuiteUI::uiFont (13.0f, true));
        g.drawText ("Loop missing - click to locate", bounds, juce::Justification::centred);
    }
}
