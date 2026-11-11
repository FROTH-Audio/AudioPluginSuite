#include "SuiteLookAndFeel.h"

namespace SuiteUI
{
    namespace
    {
        constexpr int numLeds = 21;
    }

    SuiteLookAndFeel::SuiteLookAndFeel (juce::Colour accentColour)
        : accent (accentColour)
    {
        setColour (juce::Slider::textBoxTextColourId, Colours::text);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::thumbColourId, accent);
        setColour (juce::Label::textColourId, Colours::textDim);
        setColour (juce::ToggleButton::textColourId, Colours::text);
    }

    void SuiteLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPos, float startAngle, float endAngle,
                                             juce::Slider&)
    {
        const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (3.0f);
        const float diameter = juce::jmin (area.getWidth(), area.getHeight());
        const auto centre = area.getCentre();
        const float ledRadius = diameter * 0.5f;
        const float radius = diameter * 0.5f;

        // Ring-to-arc and arc-to-body gaps shrink with the 60 px knob, with fixed floors
        // so the drawing stays legible at the suite size (plan 05).
        const float arcGap = juce::jmax (4.0f, radius * 0.15f);
        const float bodyGap = juce::jmax (3.0f, radius * 0.10f);

        const float angle = startAngle + sliderPos * (endAngle - startAngle);
        const auto pointOn = [&] (float r, float a)
        {
            return centre + juce::Point<float> (std::sin (a), -std::cos (a)) * r;
        };

        // LED ring: lit up to the current value, dim beyond it.
        for (int i = 0; i < numLeds; ++i)
        {
            const float t = (float) i / (float) (numLeds - 1);
            const auto p = pointOn (ledRadius - 2.0f, startAngle + t * (endAngle - startAngle));

            g.setColour (t <= sliderPos + 1.0e-4f ? accent : Colours::track);
            g.fillEllipse (p.x - 1.6f, p.y - 1.6f, 3.2f, 3.2f);
        }

        // Track and value arc, with a soft glow under the value arc.
        const float arcRadius = ledRadius - arcGap;
        juce::Path trackArc, valueArc;
        trackArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
        valueArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);

        g.setColour (Colours::track);
        g.strokePath (trackArc, juce::PathStrokeType (3.0f));

        g.setColour (accent.withAlpha (0.25f));
        g.strokePath (valueArc, juce::PathStrokeType (juce::jmax (6.0f, arcGap * 1.8f),
                                                      juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (accent);
        g.strokePath (valueArc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Knob body and pointer.
        const float bodyRadius = arcRadius - bodyGap;
        juce::ColourGradient body (Colours::panelLight, centre.x, centre.y - bodyRadius,
                                   Colours::panel, centre.x, centre.y + bodyRadius, false);
        g.setGradientFill (body);
        g.fillEllipse (centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);

        g.setColour (Colours::panelEdge);
        g.drawEllipse (centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f, 1.0f);

        juce::Path pointer;
        pointer.startNewSubPath (pointOn (bodyRadius * 0.3f, angle));
        pointer.lineTo (pointOn (bodyRadius * 0.85f, angle));

        g.setColour (accent);
        g.strokePath (pointer, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void SuiteLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPos, float minSliderPos, float maxSliderPos,
                                             juce::Slider::SliderStyle style, juce::Slider& slider)
    {
        if (style != juce::Slider::LinearHorizontal)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos,
                                              maxSliderPos, style, slider);
            return;
        }

        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
        const float centreY = bounds.getCentreY();
        const juce::Rectangle<float> trackArea (bounds.getX() + 6.0f, centreY - 2.0f, bounds.getWidth() - 12.0f, 4.0f);

        g.setColour (Colours::track);
        g.fillRoundedRectangle (trackArea, 2.0f);

        g.setColour (accent);
        g.fillRoundedRectangle (trackArea.withRight (sliderPos), 2.0f);

        g.setColour (Colours::text);
        g.fillEllipse (sliderPos - 5.0f, centreY - 5.0f, 10.0f, 10.0f);
    }

    void SuiteLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                             bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced (2.0f);
        const float radius = juce::jmin (SuiteUI::controlRadius, bounds.getHeight() * 0.5f);
        const bool on = button.getToggleState();

        // On: accent border, tinted fill, accent text. Off: hairline border, dim text.
        // Colour never carries the state alone (plan 05 R6): the dot is filled when on,
        // hollow when off.
        g.setColour (on ? accent.withAlpha (0.18f) : Colours::panel);
        g.fillRoundedRectangle (bounds, radius);

        g.setColour (on ? accent : Colours::panelEdge);
        g.drawRoundedRectangle (bounds, radius, on ? 1.5f : 1.0f);

        const float dotRadius = juce::jmin (4.0f, bounds.getHeight() * 0.14f);
        const auto dotCentre = juce::Point<float> (bounds.getX() + bounds.getHeight() * 0.5f,
                                                   bounds.getCentreY());

        if (bounds.getWidth() > 110.0f)
        {
            if (on)
                g.fillEllipse (dotCentre.x - dotRadius, dotCentre.y - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
            else
                g.drawEllipse (dotCentre.x - dotRadius, dotCentre.y - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f, 1.2f);
        }

        const auto textBounds = bounds.getWidth() > 110.0f
                                    ? bounds.withTrimmedLeft (bounds.getHeight() * 1.1f)
                                    : bounds;
        const float brightness = shouldDrawButtonAsDown ? 0.85f : (shouldDrawButtonAsHighlighted ? 1.0f : 0.92f);
        g.setColour ((on ? accent : Colours::textDim).withMultipliedBrightness (brightness));
        g.setFont (SuiteUI::uiFont (14.0f, true));
        g.drawText (button.getButtonText(), textBounds, juce::Justification::centred);
    }
}
