#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace SuiteUI
{
    /** Editor refresh rate in Hz. Every plugin editor's timer uses it, so the three stay in step. */
    inline constexpr int kRefreshHz = 60;

    //==============================================================================
    // Mandatory suite UI tokens (plan 05). Every editor reads these; no plugin keeps
    // its own sizes. AGENTS.md section 7 documents them.

    /** The one rotary knob diameter, in px, identical in every plugin (plan 05 R1; 05.1 pass 2). */
    inline constexpr int knobDiameter = 40;

    /** Height of a row that holds one rotary knob: caption + knob + value box + padding (plan 05 R2). */
    inline constexpr int knobRowHeight = 92;

    /** Height of every toggle button and selector pill; layouts centre them in their cell (plan 05 R3). */
    inline constexpr int pillHeight = 36;

    /** Corner radius of every rounded control (plan 05 R3). */
    inline constexpr float controlRadius = 6.0f;

    /** Caption row height inside a LabelledSlider (plan 05 R5). */
    inline constexpr int captionHeight = 18;

    /** Group title band height, and the corner radius of every rounded control (plan 05 R3/R7). */
    inline constexpr int groupTitleHeight = 20;

    /** Rotary value box size, shared by every knob (plan 05 R5). */
    inline constexpr int valueBoxWidth = 84;
    inline constexpr int valueBoxHeight = 18;

    /** The suite font. Bahnschrift is the brand font (docs/brand.md); when it is missing
        (macOS), JUCE falls back to its default sans automatically. */
    inline juce::Font uiFont (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions ("Bahnschrift", height, bold ? juce::Font::bold : juce::Font::plain));
    }

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

        // Dropdowns and their popup menus use the suite's panel/track/text look (plan 12).
        void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                           int buttonX, int buttonY, int buttonW, int buttonH,
                           juce::ComboBox&) override;

        juce::Font getComboBoxFont (juce::ComboBox&) override;
        void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

        void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
        juce::Font getPopupMenuFont() override;

        void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area,
                                bool isSeparator, bool isActive, bool isHighlighted,
                                bool isTicked, bool hasSubMenu, const juce::String& text,
                                const juce::String& shortcutKeyText, const juce::Drawable* icon,
                                const juce::Colour* textColourToUse) override;

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
