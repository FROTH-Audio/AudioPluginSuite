#include "MicroTimingChaosLookAndFeel.h"

#include "BinaryData.h"

namespace MicroTimingChaos
{
    namespace
    {
        juce::Image loadImageResource (const char* name)
        {
            int size = 0;
            if (const char* data = BinaryData::getNamedResource (name, size))
                return juce::ImageFileFormat::loadFrom (data, (size_t) size);

            return {};
        }

        /** The 4K textures are much larger than they are drawn, so they are scaled down once when loaded. */
        juce::Image loadTexture (const char* name, int maxWidth)
        {
            auto image = loadImageResource (name);

            if (image.isValid() && image.getWidth() > maxWidth)
                image = image.rescaled (maxWidth, juce::roundToInt ((double) image.getHeight() * maxWidth / image.getWidth()));

            return image;
        }

        std::unique_ptr<juce::Drawable> loadIcon (const char* name)
        {
            int size = 0;
            if (const char* data = BinaryData::getNamedResource (name, size))
                return juce::Drawable::createFromSVGString (juce::String::fromUTF8 (data, size));

            return {};
        }

        const juce::Image& backgroundImage()
        {
            static const juce::Image image = loadTexture ("sequencer_bg_4k_png", 1024);
            return image;
        }

        const juce::Image& gridImage()
        {
            static const juce::Image image = loadTexture ("sequencer_grid_4k_png", 1024);
            return image;
        }

        const juce::Image& syncImage()
        {
            static const juce::Image image = loadTexture ("bpm_sync_4k_png", 512);
            return image;
        }

        const juce::Drawable* syncIcon (bool active)
        {
            static const auto activeIcon = loadIcon ("bpm_sync_active_svg");
            static const auto inactiveIcon = loadIcon ("bpm_sync_inactive_svg");
            return (active ? activeIcon : inactiveIcon).get();
        }
    }

    LookAndFeel::LookAndFeel (juce::Colour accentColour)
        : SuiteLookAndFeel (accentColour), accent (accentColour)
    {
        setColour (juce::Label::textColourId, SuiteUI::Colours::text);
        setColour (juce::Slider::textBoxTextColourId, SuiteUI::Colours::text);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::textColourId, SuiteUI::Colours::text);
        setColour (juce::PopupMenu::backgroundColourId, SuiteUI::Colours::panel);
        setColour (juce::PopupMenu::textColourId, SuiteUI::Colours::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.25f));
        setColour (juce::PopupMenu::highlightedTextColourId, SuiteUI::Colours::text);

        // The shared look sets these for its own sliders and toggles. Put back the LookAndFeel_V4 values, which
        // the modulation sliders and the toggles here were drawn with before the shared base class.
        const juce::LookAndFeel_V4 defaults;
        setColour (juce::Slider::thumbColourId, defaults.findColour (juce::Slider::thumbColourId));
        setColour (juce::ToggleButton::textColourId, defaults.findColour (juce::ToggleButton::textColourId));
    }

    void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float minSliderPos, float maxSliderPos,
                                        juce::Slider::SliderStyle style, juce::Slider& slider)
    {
        // The modulation rows use linear sliders. Keep the LookAndFeel_V4 drawing they had before.
        juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
    }

    void LookAndFeel::paint (juce::Graphics& g, juce::Component& component)
    {
        const auto area = component.getLocalBounds().toFloat();

        g.fillAll (SuiteUI::Colours::background);

        const auto& background = backgroundImage();
        if (background.isValid())
            g.drawImage (background, area);

        const auto& grid = gridImage();
        if (grid.isValid())
            g.drawImage (grid, area);
    }

    void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
    {
        // Plan 20: the icon, texture and ON/OFF state belong to the BPM Sync button alone.
        // The shared modulation panel's toggles (INVERT, RECT) are plain text pills, so they
        // use the suite's drawing.
        if (button.getButtonText() != "BPM SYNC")
        {
            SuiteLookAndFeel::drawToggleButton (g, button, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
            return;
        }

        auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
        const bool active = button.getToggleState();
        const auto colour = active ? accent : SuiteUI::Colours::textDim;

        g.setColour (shouldDrawButtonAsDown ? SuiteUI::Colours::panelLight : SuiteUI::Colours::panel);
        g.fillRoundedRectangle (bounds, 6.0f);

        if (active)
        {
            // Texture overlay, clipped to the button
            const auto& texture = syncImage();
            if (texture.isValid())
            {
                juce::Graphics::ScopedSaveState state (g);
                g.reduceClipRegion (bounds.toNearestInt());
                g.setOpacity (0.35f);
                g.drawImage (texture, bounds);
            }
        }

        g.setColour (colour.withAlpha (active || shouldDrawButtonAsHighlighted ? 0.9f : 0.45f));
        g.drawRoundedRectangle (bounds, 6.0f, active ? 1.5f : 1.0f);

        // Icon on the left, label and state on the right
        const auto iconArea = bounds.removeFromLeft (bounds.getHeight()).reduced (bounds.getHeight() * 0.2f);
        if (const auto* icon = syncIcon (active))
            icon->drawWithin (g, iconArea, juce::RectanglePlacement::centred, 1.0f);

        auto textArea = bounds.reduced (6.0f, 0.0f);

        g.setColour (active ? SuiteUI::Colours::text : SuiteUI::Colours::textDim);
        g.setFont (SuiteUI::uiFont (14.0f, true));
        g.drawText (button.getButtonText(), textArea.withTrimmedRight (34.0f), juce::Justification::centredLeft);

        g.setColour (colour);
        g.setFont (SuiteUI::uiFont (13.0f, true));
        g.drawText (active ? "ON" : "OFF", textArea.removeFromRight (30.0f), juce::Justification::centredRight);
    }

    void LookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                    int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box)
    {
        const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (1.0f);

        g.setColour (SuiteUI::Colours::panel);
        g.fillRoundedRectangle (bounds, 5.0f);

        g.setColour (box.hasKeyboardFocus (true) ? accent : SuiteUI::Colours::track);
        g.drawRoundedRectangle (bounds, 5.0f, 1.0f);

        const auto arrowArea = juce::Rectangle<float> ((float) buttonX, (float) buttonY, (float) buttonW, (float) buttonH).reduced (buttonW * 0.32f, buttonH * 0.38f);
        juce::Path arrow;
        arrow.addTriangle (arrowArea.getX(), arrowArea.getY(),
                           arrowArea.getRight(), arrowArea.getY(),
                           arrowArea.getCentreX(), arrowArea.getBottom());

        g.setColour (isButtonDown ? accent : SuiteUI::Colours::textDim);
        g.fillPath (arrow);
    }

    void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
    {
        label.setBounds (box.getLocalBounds().reduced (10, 1).withTrimmedRight (14));
        label.setFont (getComboBoxFont (box));
    }

    juce::Font LookAndFeel::getLabelFont (juce::Label&)
    {
        return SuiteUI::uiFont (14.0f);
    }

    juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox&)
    {
        return SuiteUI::uiFont (14.0f);
    }

    juce::Font LookAndFeel::getPopupMenuFont()
    {
        return SuiteUI::uiFont (14.0f);
    }
}
