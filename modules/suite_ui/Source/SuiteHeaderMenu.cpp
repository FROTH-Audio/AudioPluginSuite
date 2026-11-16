#include "SuiteHeaderMenu.h"
#include "SuiteLookAndFeel.h"

namespace SuiteUI
{
    namespace
    {
        /** The gear icon: a ring body plus eight teeth, drawn from a Path so no binary asset
            is needed. */
        void drawGear (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
        {
            const auto centre = area.getCentre();
            const float size = juce::jmin (area.getWidth(), area.getHeight());
            const float rBody = size * 0.32f;
            const float rHole = size * 0.13f;
            constexpr float twoPi = juce::MathConstants<float>::twoPi;

            juce::Path gear;
            gear.addPieSegment (centre.x - rBody, centre.y - rBody, rBody * 2.0f, rBody * 2.0f,
                                0.0f, twoPi, rHole / rBody);

            for (int i = 0; i < 8; ++i)
            {
                juce::Path tooth;
                tooth.addRoundedRectangle (-size * 0.055f, -size * 0.46f,
                                           size * 0.11f, size * 0.16f, 2.0f, 2.0f);
                tooth.applyTransform (juce::AffineTransform::rotation (twoPi * (float) i / 8.0f)
                                          .translated (centre.x, centre.y));
                gear.addPath (tooth);
            }

            g.setColour (colour);
            g.fillPath (gear);
        }
    }

    HeaderMenuButton::HeaderMenuButton (juce::Colour accentColour)
        : accent (accentColour)
    {
        setWantsKeyboardFocus (false);
        setRepaintsOnMouseActivity (true);
        setHelpText ("Settings and about");
    }

    void HeaderMenuButton::paint (juce::Graphics& g)
    {
        auto area = getLocalBounds().toFloat();

        if (isMouseOver() || isMouseButtonDown())
        {
            g.setColour (accent.withAlpha (0.10f));
            g.fillEllipse (area);
        }

        drawGear (g, area.reduced (3.0f),
                  (isMouseOver() || isMouseButtonDown()) ? Colours::text : Colours::textDim);
    }

    void HeaderMenuButton::mouseDown (const juce::MouseEvent&)
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Settings...", true, false);
        menu.addItem (2, "About...", true, false);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [this] (int result)
                            {
                                if (result == 1 && onSettings)
                                    onSettings();

                                if (result == 2 && onAbout)
                                    onAbout();
                            });
    }

    AboutPanel::AboutPanel (const juce::Image& lockupImage, const juce::String& productName,
                            const juce::String& versionString, const juce::String& pluginCode,
                            const juce::String& pluginId)
        : lockup (lockupImage), name (productName), version (versionString),
          code (pluginCode), id (pluginId)
    {
        setSize (380, 210);
    }

    void AboutPanel::paint (juce::Graphics& g)
    {
        g.fillAll (Colours::panel);
        g.setColour (Colours::panelEdge);
        g.drawRect (getLocalBounds().toFloat(), 1.0f);

        auto bounds = getLocalBounds().reduced (16);

        // The lockup keeps its proportions and is reduced to fit the top-left slot, like the
        // editor headers draw it (SuiteBrand.h).
        if (lockup.isValid())
        {
            const auto lockupArea = bounds.removeFromTop (44);
            g.drawImageWithin (lockup, lockupArea.getX(), lockupArea.getY(),
                               lockupArea.getWidth() / 2, lockupArea.getHeight(),
                               juce::RectanglePlacement (juce::RectanglePlacement::xLeft
                                                         | juce::RectanglePlacement::yMid
                                                         | juce::RectanglePlacement::onlyReduceInSize),
                               false);
        }

        bounds.removeFromTop (6);
        g.setColour (Colours::text);
        g.setFont (uiFont (20.0f, true));
        g.drawText (name, bounds.removeFromTop (28), juce::Justification::centredLeft);

        g.setFont (uiFont (13.0f));
        g.setColour (Colours::textDim);
        g.drawText (version + "  -  FROTH Audio", bounds.removeFromTop (20),
                    juce::Justification::centredLeft);

        bounds.removeFromTop (4);
        g.drawText ("Plugin code " + code + "  -  " + id, bounds.removeFromTop (20),
                    juce::Justification::centredLeft);

        const juce::String juceVersion { "Built with JUCE " };
        g.drawText (juceVersion + juce::String (JUCE_MAJOR_VERSION) + "."
                        + juce::String (JUCE_MINOR_VERSION) + "."
                        + juce::String (JUCE_BUILDNUMBER),
                    bounds.removeFromTop (20), juce::Justification::centredLeft);

        // The whole suite is GPL v3 (LICENSE); the plugin repeats it here because the About
        // dialog is the one place every user looks.
        g.setColour (Colours::textDim);
        g.drawText ("Licensed under the GNU GPL v3. This plugin comes with no warranty.",
                    getLocalBounds().removeFromBottom (28).reduced (16, 0),
                    juce::Justification::centredLeft);
    }

    void showAboutDialog (const juce::Image& lockupImage, const juce::String& productName,
                          const juce::String& version, const juce::String& pluginCode,
                          const juce::String& pluginId, juce::Component* centreAround)
    {
        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned (new AboutPanel (lockupImage, productName, version,
                                                  pluginCode, pluginId));
        options.dialogTitle = "About";
        options.componentToCentreAround = centreAround;
        options.dialogBackgroundColour = Colours::panel;
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = false;
        options.resizable = false;
        options.launchAsync();
    }
}  // namespace SuiteUI
