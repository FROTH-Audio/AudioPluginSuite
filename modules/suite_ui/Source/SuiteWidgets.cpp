#include "SuiteWidgets.h"
#include "SuiteLookAndFeel.h"

#include <cmath>

namespace SuiteUI
{
    namespace
    {
        constexpr float minDb = -96.0f;
        constexpr float meterFloorDb = -60.0f;

        float toDb (float magnitude, float scale) noexcept
        {
            return 20.0f * std::log10 (juce::jmax (magnitude * scale, 1.0e-6f));
        }

        float unitInterval (float value, float low, float high) noexcept
        {
            return juce::jlimit (0.0f, 1.0f, (value - low) / (high - low));
        }

        // Maps a point in [-1, 1] x [-1, 1] (y pointing up) into the plot area.
        juce::Point<float> plotPoint (juce::Rectangle<float> area, float x, float y) noexcept
        {
            return { area.getX() + (x * 0.5f + 0.5f) * area.getWidth(),
                     area.getY() + (0.5f - y * 0.5f) * area.getHeight() };
        }
    }

    //==============================================================================
    SpectrumDisplay::SpectrumDisplay (juce::Colour in, juce::Colour out)
        : inputColour (in), outputColour (out)
    {
    }

    void SpectrumDisplay::setSpectra (const float* inputMagnitudes, const float* outputMagnitudes,
                                      int numBins, double binWidthHz_, float magnitudeScale)
    {
        binWidthHz = binWidthHz_;
        inputDb.resize ((size_t) numBins);
        outputDb.resize ((size_t) numBins);

        for (int k = 0; k < numBins; ++k)
        {
            inputDb[(size_t) k] = toDb (inputMagnitudes[k], magnitudeScale);
            outputDb[(size_t) k] = toDb (outputMagnitudes[k], magnitudeScale);
        }

        repaint();
    }

    juce::Path SpectrumDisplay::makePath (const std::vector<float>& decibels, juce::Rectangle<float> area) const
    {
        juce::Path path;
        const float minFreq = 20.0f;
        const float maxFreq = 20000.0f;
        bool started = false;

        for (size_t k = 1; k < decibels.size(); ++k)
        {
            const float freq = (float) (k * binWidthHz);

            if (freq < minFreq || freq > maxFreq)
                continue;

            const float x = area.getX() + std::log (freq / minFreq) / std::log (maxFreq / minFreq) * area.getWidth();
            const float y = area.getBottom() - unitInterval (decibels[k], minDb, 0.0f) * area.getHeight();

            // Path::isEmpty() stays true until a line exists, so it cannot tell whether a sub-path has started.
            if (! started)
            {
                path.startNewSubPath (x, y);
                started = true;
            }
            else
            {
                path.lineTo (x, y);
            }
        }

        return path;
    }

    void SpectrumDisplay::paint (juce::Graphics& g)
    {
        auto area = getLocalBounds().toFloat().reduced (4.0f);

        g.setColour (Colours::panel);
        g.fillRoundedRectangle (area, 6.0f);

        // Horizontal dB gridlines.
        g.setColour (Colours::track);
        for (float db = -24.0f; db > minDb; db -= 24.0f)
            g.drawHorizontalLine ((int) (area.getBottom() - unitInterval (db, minDb, 0.0f) * area.getHeight()),
                                  area.getX(), area.getRight());

        // Frequency gridlines and labels: 100 Hz, 1 kHz and 10 kHz.
        const float minFreq = 20.0f;
        const float maxFreq = 20000.0f;
        g.setFont (juce::Font (juce::FontOptions (10.0f)));

        for (const float freq : { 100.0f, 1000.0f, 10000.0f })
        {
            const float x = area.getX() + std::log (freq / minFreq) / std::log (maxFreq / minFreq) * area.getWidth();
            g.setColour (Colours::track);
            g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
            g.setColour (Colours::textDim);
            g.drawText (freq >= 1000.0f ? juce::String ((int) (freq / 1000.0f)) + "k" : juce::String ((int) freq),
                        juce::Rectangle<float> (x + 2.0f, area.getBottom() - 14.0f, 40.0f, 12.0f),
                        juce::Justification::left);
        }

        if (! signalPresent || inputDb.empty())
        {
            g.setColour (Colours::textDim);
            g.setFont (juce::Font (juce::FontOptions (13.0f)));
            g.drawText (waitingMessage, area, juce::Justification::centred);
            return;
        }

        g.setColour (inputColour.withAlpha (0.55f));
        g.strokePath (makePath (inputDb, area), juce::PathStrokeType (1.2f));

        const auto outputPath = makePath (outputDb, area);
        g.setColour (outputColour.withAlpha (0.25f));
        g.strokePath (outputPath, juce::PathStrokeType (5.0f));
        g.setColour (outputColour);
        g.strokePath (outputPath, juce::PathStrokeType (1.8f));
    }

    void SpectrumDisplay::setWaitingMessage (const juce::String& message)
    {
        if (waitingMessage != message)
        {
            waitingMessage = message;
            repaint();
        }
    }

    void SpectrumDisplay::setSignalPresent (bool present)
    {
        if (signalPresent != present)
        {
            signalPresent = present;
            repaint();
        }
    }

    //==============================================================================
    TrailPlot::TrailPlot (juce::Colour c)
        : colour (c)
    {
    }

    void TrailPlot::setTrail (std::vector<juce::Point<float>> points)
    {
        trail = std::move (points);
        repaint();
    }

    void TrailPlot::paint (juce::Graphics& g)
    {
        const auto area = getLocalBounds().toFloat().reduced (4.0f);

        g.setColour (Colours::panel);
        g.fillRoundedRectangle (area, 6.0f);

        g.setColour (Colours::track);
        g.drawLine (area.getX(), area.getCentreY(), area.getRight(), area.getCentreY());
        g.drawLine (area.getCentreX(), area.getY(), area.getCentreX(), area.getBottom());

        if (trail.size() < 2)
            return;

        // Older segments fade out; the newest point gets a bright marker.
        const auto count = trail.size();

        for (size_t i = 1; i < count; ++i)
        {
            const float age = (float) i / (float) count;
            g.setColour (colour.withAlpha (age * 0.9f));
            g.drawLine (juce::Line<float> (plotPoint (area, trail[i - 1].x, trail[i - 1].y),
                                           plotPoint (area, trail[i].x, trail[i].y)), 1.5f);
        }

        const auto head = plotPoint (area, trail.back().x, trail.back().y);
        g.setColour (colour);
        g.fillEllipse (head.x - 3.5f, head.y - 3.5f, 7.0f, 7.0f);
    }

    //==============================================================================
    CurveView::CurveView (juce::Colour c)
        : colour (c)
    {
    }

    void CurveView::setCurve (std::function<float (float)> newCurve, float marker)
    {
        curve = std::move (newCurve);
        markerInput = marker;
        repaint();
    }

    void CurveView::paint (juce::Graphics& g)
    {
        const auto area = getLocalBounds().toFloat().reduced (4.0f);

        g.setColour (Colours::panel);
        g.fillRoundedRectangle (area, 6.0f);

        // Unity-gain reference line.
        g.setColour (Colours::track);
        g.drawLine (juce::Line<float> (plotPoint (area, -1.0f, -1.0f), plotPoint (area, 1.0f, 1.0f)), 1.0f);

        if (! curve)
            return;

        constexpr int steps = 128;
        juce::Path path;

        for (int i = 0; i <= steps; ++i)
        {
            const float x = -1.0f + 2.0f * (float) i / (float) steps;
            const auto p = plotPoint (area, x, juce::jlimit (-1.0f, 1.0f, curve (x)));

            if (i == 0)
                path.startNewSubPath (p);
            else
                path.lineTo (p);
        }

        g.setColour (colour.withAlpha (0.25f));
        g.strokePath (path, juce::PathStrokeType (6.0f));
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (2.0f));

        const float markerX = juce::jlimit (-1.0f, 1.0f, markerInput);
        const auto marker = plotPoint (area, markerX, juce::jlimit (-1.0f, 1.0f, curve (markerX)));
        g.setColour (colour);
        g.fillEllipse (marker.x - 4.0f, marker.y - 4.0f, 8.0f, 8.0f);
    }

    //==============================================================================
    LevelMeter::LevelMeter (juce::Colour c)
        : colour (c)
    {
    }

    void LevelMeter::setLevel (float linearLevel)
    {
        level = linearLevel;
        repaint();
    }

    void LevelMeter::paint (juce::Graphics& g)
    {
        const auto area = getLocalBounds().toFloat().reduced (2.0f);
        const float db = 20.0f * std::log10 (juce::jmax (level, 1.0e-6f));
        const float fill = unitInterval (db, meterFloorDb, 0.0f);

        g.setColour (Colours::track);
        g.fillRoundedRectangle (area, 3.0f);

        auto filled = area.withTop (area.getBottom() - fill * area.getHeight());
        g.setColour (colour);
        g.fillRoundedRectangle (filled, 3.0f);
    }

    //==============================================================================
    SegmentedSelector::SegmentedSelector (juce::Colour accentColour)
        : accent (accentColour)
    {
    }

    void SegmentedSelector::setOptions (const juce::StringArray& labels)
    {
        options = labels;
        repaint();
    }

    void SegmentedSelector::setSelectedIndex (int index)
    {
        selected = juce::jlimit (0, juce::jmax (0, options.size() - 1), index);
        repaint();
    }

    void SegmentedSelector::setFontHeight (float height)
    {
        fontHeight = height;
        repaint();
    }

    void SegmentedSelector::paint (juce::Graphics& g)
    {
        const auto area = getLocalBounds().toFloat().reduced (1.0f);
        const float segmentWidth = area.getWidth() / (float) juce::jmax (1, options.size());

        g.setColour (Colours::panel);
        g.fillRoundedRectangle (area, SuiteUI::controlRadius);
        g.setColour (Colours::panelEdge);
        g.drawRoundedRectangle (area, SuiteUI::controlRadius, 1.0f);

        for (int i = 0; i < options.size(); ++i)
        {
            const juce::Rectangle<float> segment (area.getX() + segmentWidth * (float) i, area.getY(),
                                                  segmentWidth, area.getHeight());
            const bool isSelected = i == selected;

            // The selected segment is a tinted fill with accent text; no glow (plan 05 R3).
            if (isSelected)
            {
                g.setColour (accent.withAlpha (0.20f));
                g.fillRoundedRectangle (segment.reduced (2.0f),
                                        juce::jmax (2.0f, SuiteUI::controlRadius - 2.0f));
            }

            g.setColour (isSelected ? accent : Colours::textDim);
            g.setFont (SuiteUI::uiFont (fontHeight, isSelected));
            g.drawText (options[i], segment, juce::Justification::centred);
        }
    }

    void SegmentedSelector::mouseDown (const juce::MouseEvent& event)
    {
        if (options.isEmpty())
            return;

        const float segmentWidth = (float) getWidth() / (float) options.size();
        const int index = juce::jlimit (0, options.size() - 1, (int) (event.position.x / segmentWidth));

        if (index != selected)
        {
            setSelectedIndex (index);

            if (onChange)
                onChange (index);
        }
    }

    //==============================================================================
    LabelledSlider::LabelledSlider (const juce::String& caption, juce::Slider::SliderStyle style)
    {
        const bool rotary = style == juce::Slider::RotaryVerticalDrag
                         || style == juce::Slider::RotaryHorizontalVerticalDrag
                         || style == juce::Slider::Rotary;

        slider.setSliderStyle (style);
        slider.setTextBoxStyle (rotary ? juce::Slider::TextBoxBelow : juce::Slider::TextBoxRight,
                                false, SuiteUI::valueBoxWidth, SuiteUI::valueBoxHeight);

        label.setText (caption, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (SuiteUI::uiFont (13.0f, true));
        label.setColour (juce::Label::textColourId, SuiteUI::Colours::textDim);

        addAndMakeVisible (label);
        addAndMakeVisible (slider);
    }

    void LabelledSlider::resized()
    {
        auto area = getLocalBounds();
        label.setBounds (area.removeFromTop (SuiteUI::captionHeight));

        if (! slider.isRotary())
        {
            slider.setBounds (area);
            return;
        }

        // One knob size for the whole suite (plan 05 R1): the slider is centred and sized
        // so drawRotarySlider receives exactly knobDiameter. The drawable rect is the
        // slider bounds minus the value box (18 px below), reduced 3 px per side in the
        // look and feel; 64x64 bounds produce a 40 px knob.
        constexpr int sliderBounds = SuiteUI::knobDiameter + 6 + SuiteUI::valueBoxHeight;
        const int h = juce::jmin (sliderBounds, area.getHeight());
        const int w = juce::jmin (sliderBounds, juce::jmax (SuiteUI::valueBoxWidth, area.getWidth()));
        slider.setBounds (area.withSizeKeepingCentre (w, h));
    }

    //==============================================================================
    namespace
    {
        constexpr int keyboardFirstKey = 24;
        constexpr int keyboardNumKeys = 73;   // MIDI 24-96

        bool isBlackMidiKey (int note) noexcept
        {
            const int pc = ((note % 12) + 12) % 12;
            return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        }

        int whiteKeyCount() noexcept
        {
            int whites = 0;
            for (int i = 0; i < keyboardNumKeys; ++i)
                if (! isBlackMidiKey (keyboardFirstKey + i))
                    ++whites;
            return whites;
        }

        juce::Colour accentOf (juce::Component& component)
        {
            if (auto* look = dynamic_cast<SuiteLookAndFeel*> (&component.getLookAndFeel()))
                return look->getAccent();
            return SuiteUI::Colours::text;
        }
    }

    PlayableKeyboardStrip::PlayableKeyboardStrip()
    {
        setWantsKeyboardFocus (false);
    }

    void PlayableKeyboardStrip::setActiveNotes (const bool* notes128)
    {
        for (int i = 0; i < 128; ++i)
            active[(size_t) i] = notes128 != nullptr && notes128[i];

        repaint();
    }

    void PlayableKeyboardStrip::setRootKey (int note)
    {
        rootKey = juce::jlimit (keyboardFirstKey, keyboardFirstKey + keyboardNumKeys - 1, note);
        repaint();
    }

    void PlayableKeyboardStrip::paint (juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        const float keyW = bounds.getWidth() / (float) keyboardNumKeys;
        const auto litColour = accentOf (*this);

        g.setColour (SuiteUI::Colours::panel.withAlpha (0.55f));
        g.fillRoundedRectangle (bounds, SuiteUI::controlRadius);
        g.setColour (SuiteUI::Colours::track);
        g.drawRoundedRectangle (bounds, SuiteUI::controlRadius, 1.0f);

        const float blackHeight = bounds.getHeight() * 0.62f;

        for (int pass = 0; pass < 2; ++pass) // white keys first, then black keys on top
        {
            for (int i = 0; i < keyboardNumKeys; ++i)
            {
                const int note = keyboardFirstKey + i;
                const bool black = isBlackMidiKey (note);

                if (black != (pass == 1))
                    continue;

                const float h = black ? blackHeight : bounds.getHeight();
                auto keyRect = juce::Rectangle<float> (bounds.getX() + i * keyW, bounds.getY(), keyW, h);
                keyRect.reduce (0.5f, 0.0f);

                const bool lit = active[(size_t) note] || pressed[(size_t) note];

                if (black)
                {
                    g.setColour (lit ? litColour : SuiteUI::Colours::track.withAlpha (0.85f));
                    g.fillRect (keyRect);
                }
                else
                {
                    g.setColour (lit ? litColour.withAlpha (0.8f) : SuiteUI::Colours::panelLight.withAlpha (0.55f));
                    g.fillRect (keyRect);
                }

                if (note == rootKey)
                {
                    g.setColour (lit ? SuiteUI::Colours::background : litColour);
                    g.fillRect (keyRect.removeFromBottom (3.0f).reduced (0.0f, 0.5f));
                }
            }
        }
    }

    int PlayableKeyboardStrip::noteAt (juce::Point<float> position) const
    {
        auto bounds = getLocalBounds().toFloat();
        if (! bounds.contains (position))
            return -1;

        const float keyW = bounds.getWidth() / (float) keyboardNumKeys;
        const float blackHeight = bounds.getHeight() * 0.62f;
        const int guess = keyboardFirstKey + (int) (position.x / keyW);

        // Black keys sit on top in the upper region; check them around the guessed key.
        if (position.y < blackHeight)
            for (int offset = -1; offset <= 1; ++offset)
            {
                const int note = guess + offset;
                if (note < keyboardFirstKey || note >= keyboardFirstKey + keyboardNumKeys)
                    continue;

                if (isBlackMidiKey (note))
                {
                    const float centreX = bounds.getX() + (float) (note - keyboardFirstKey + 1) * keyW;
                    if (std::abs (position.x - centreX) <= keyW * 0.3f)
                        return note;
                }
            }

        // The white key under the pointer: white keys share the width equally.
        const int whiteIdx = juce::jlimit (0, whiteKeyCount() - 1,
                                           (int) ((position.x - bounds.getX()) / (bounds.getWidth() / (float) whiteKeyCount())));

        int note = keyboardFirstKey;
        int seen = 0;
        for (; note < keyboardFirstKey + keyboardNumKeys; ++note)
            if (! isBlackMidiKey (note))
                if (seen++ == whiteIdx)
                    break;

        return juce::jlimit (keyboardFirstKey, keyboardFirstKey + keyboardNumKeys - 1, note);
    }

    void PlayableKeyboardStrip::mouseDown (const juce::MouseEvent& e)
    {
        const int note = noteAt (e.position);
        if (note < 0)
            return;

        dragNote = note;
        pressed[(size_t) note] = true;
        repaint();

        if (onNoteOn)
            onNoteOn (note, 0.8f);
    }

    void PlayableKeyboardStrip::mouseDrag (const juce::MouseEvent& e)
    {
        const int note = noteAt (e.position);
        if (note < 0 || note == dragNote)
            return;

        if (dragNote >= 0)
        {
            pressed[(size_t) dragNote] = false;
            if (onNoteOff)
                onNoteOff (dragNote);
        }

        dragNote = note;
        pressed[(size_t) note] = true;
        repaint();

        if (onNoteOn)
            onNoteOn (note, 0.8f);
    }

    void PlayableKeyboardStrip::mouseUp (const juce::MouseEvent& e)
    {
        const int note = dragNote >= 0 ? dragNote : noteAt (e.position);
        dragNote = -1;

        if (note >= 0 && pressed[(size_t) note])
        {
            pressed[(size_t) note] = false;
            repaint();

            if (onNoteOff)
                onNoteOff (note);
        }
    }
}
