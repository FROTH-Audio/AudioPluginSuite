#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace SuiteUI
{
    /** Editor refresh rate in Hz. Every plugin editor's timer uses it, so the three stay in step. */
    inline constexpr int kRefreshHz = 60;

    /** Palette shared by every plugin in the suite. Each plugin supplies only its own accent colour. */
    namespace Colours
    {
        inline const juce::Colour background { 0xff0b0e13 };
        inline const juce::Colour panel { 0xff151a22 };
        inline const juce::Colour panelLight { 0xff1f2631 };
        inline const juce::Colour panelEdge { 0xff2a3341 };
        inline const juce::Colour track { 0xff2a3240 };
        inline const juce::Colour text { 0xffdde4ee };
        inline const juce::Colour textDim { 0xff7d8899 };
    }

    /**
        Dark, flat look with neon accents: arc knobs with an LED ring, glow toggles and
        thin linear sliders. Every plugin editor sets this as its look and feel.

        Pattern: instantiate with your accent colour, then call setLookAndFeel(&laf) in
        createEditor(), and nullptr on shutdown. The same laf can be reused across editors.

        Example:
            SuiteUI::SuiteLookAndFeel laf { juce::Colour (0xff2de2ff) }; // cyan accent
            setLookAndFeel (&laf);
    */
    class SuiteLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        explicit SuiteLookAndFeel (juce::Colour accentColour);

        juce::Colour getAccent() const noexcept { return accent; }

        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPosProportional, float rotaryStartAngle,
                               float rotaryEndAngle, juce::Slider&) override;

        void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPos, float minSliderPos, float maxSliderPos,
                               juce::Slider::SliderStyle, juce::Slider&) override;

        void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    private:
        juce::Colour accent;
    };

    /** Factory helper: creates a look-and-feel with the given accent colour.
        This is not a factory in the traditional sense — it simply constructs and returns
        a SuiteLookAndFeel. It exists to make the pattern obvious in new code.

        Usage:
            auto laf = createSuiteLookAndFeel (juce::Colour (0xffff9f1c)); // amber accent
            setLookAndFeel (&laf);
    */
    inline std::unique_ptr<SuiteUI::SuiteLookAndFeel> createSuiteLookAndFeel (juce::Colour accentColour)
    {
        return std::make_unique<SuiteUI::SuiteLookAndFeel> (accentColour);
    }
}
