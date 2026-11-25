#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace SuiteUI
{
    inline constexpr const char* kRepositoryUrl = "https://github.com/FROTH-Audio/AudioPluginSuite";

    /**
        A small hamburger icon button for the plugin header. Left click opens a popup menu with
        "Settings..." and "About..."; the callbacks run on the message thread. The icon is
        drawn with a Path - there is no new binary asset (plan 08 Q3).

        R1-R10 note: this is a header icon button, not a knob and not a pill; it draws itself
        with the shared palette colours and hovers to the text colour.
    */
    class HeaderMenuButton : public juce::Component
    {
    public:
        explicit HeaderMenuButton (juce::Colour accentColour);

        std::function<void()> onSettings;
        std::function<void()> onAbout;

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        juce::Colour accent;
    };

    /**
        The content of the About dialog: brand lockup, product name and version, FROTH Audio,
        the plugin id and code, the JUCE version and the GPL v3 note. Every string comes in as
        a constructor argument, so suite_ui stays free of plugin defines (the editors pass
        SUITE_PLUGIN_NAME and friends, plan 08 phase 0).
    */
    class AboutPanel : public juce::Component
    {
    public:
        AboutPanel (const juce::Image& lockupImage, const juce::String& productName,
                    const juce::String& version, const juce::String& pluginCode,
                    const juce::String& pluginId);

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        juce::Image lockup;
        juce::String name;
        juce::String version;
        juce::String code;
        juce::String id;
        juce::HyperlinkButton repoLink;
    };

    /** Opens the About dialog as a modal DialogWindow centred around centreAround. */
    void showAboutDialog (const juce::Image& lockupImage, const juce::String& productName,
                          const juce::String& version, const juce::String& pluginCode,
                          const juce::String& pluginId, juce::Component* centreAround);
}  // namespace SuiteUI
