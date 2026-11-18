#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_core/juce_core.h>
#include <SuiteLookAndFeel.h>
#include <memory>
#include <string>

namespace SuiteSettings
{
    // Key names in the one shared settings file. Namespaced by hand; nothing here is a plugin
    // parameter and nothing joins a saved session (the lerpCurve precedent).
    inline constexpr const char* tooltipsKey      = "ui/tooltips";
    inline constexpr const char* fallbackTempoKey = "tempo/fallbackBpm";
    inline constexpr const char* sgraffitoSampleFolderKey = "sgraffito/sampleFolder";

    inline constexpr double defaultFallbackBpm = 120.0;
    inline constexpr double minFallbackBpm     = 20.0;
    inline constexpr double maxFallbackBpm     = 300.0;

    inline juce::String presetFolderKey (const juce::String& pluginId)
    {
        return "presets/" + pluginId + "/folder";
    }

    inline double clampFallbackBpm (double bpm) noexcept
    {
        return juce::jlimit (minFallbackBpm, maxFallbackBpm, bpm);
    }

    /**
        The plugin settings store: one shared PropertiesFile (XML) under the user's application
        data folder, shared by all four plugins. Message thread only - every call site is editor,
        bar, menu or dialog code. Audio consumers never call shared(): the editor pushes values
        into members or atomics of the processor instead (plan 08 phase 4).

        An empty folder string always means "use the built-in default".
    */
    class Store
    {
    public:
        Store();                                // the default file (shared())
        explicit Store (const juce::File&);     // a redirected file (tests, integrations)

        bool        getBool   (const juce::String& key, bool defaultValue) const;
        double      getDouble (const juce::String& key, double defaultValue) const;
        juce::String getString (const juce::String& key, const juce::String& defaultValue) const;

        void setBool   (const juce::String& key, bool value);
        void setDouble (const juce::String& key, double value);
        void setString (const juce::String& key, const juce::String& value);

        /** Removes the key, so the default applies again. */
        void reset (const juce::String& key);

        /** Writes the file. Safe to call after every set; the file is small. */
        bool save();

        juce::File getFile() const noexcept { return file; }

        /** Loads from a different file from now on (tests redirect the store this way; the
            real user's settings file must never be touched by a test). */
        void setFile (const juce::File&);

    private:
        void rebuildProperties();
        juce::PropertiesFile::Options makeOptions() const;

        juce::File file;
        std::unique_ptr<juce::PropertiesFile> props;
    };

    /**
        The process-wide store. Created on first use; keep that first use on the message thread
        (which every current call site is).
    */
    Store& shared();

    /**
        A settings dialog with three rows (tooltips toggle, fallback tempo slider, user preset
        folder browse), plus Sgraffito's sample-folder row when the caller asks for it.
        The store is the caller's (normally the shared process-wide store); changes save immediately.
        All rows are children owned by this component - there is no state shared between dialog
        instances, so the dialog can be built and destroyed any number of times per process.
    */
    class SettingsDialog : public juce::Component
    {
    public:
        SettingsDialog (Store& storeToUse, bool showSampleFolderRow);
        ~SettingsDialog() override;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        void updateFolderTexts();
        void applyTooltipsSetting();
        void browse (bool forSampleFolder);

        Store& store;
        bool showTooltips = true;
        double fallbackTempo = defaultFallbackBpm;
        juce::String presetPath;
        juce::String samplePath;

        // Declared first so it is destroyed last, after every component that uses it.
        // The suite look is component-scoped on purpose (plan 10 phase 5b): a top-level
        // dialog otherwise inherits the host's look and feel, whose text colours can be
        // invisible on this panel.
        SuiteUI::SuiteLookAndFeel lookAndFeel { juce::Colour (0xff9aa0a6) };

        juce::TooltipWindow tooltipWindow;

        juce::Label        tooltipsCaption;
        juce::ToggleButton tooltipsToggle;
        juce::Label        tooltipsValue;
        juce::Label        tempoCaption;
        juce::Slider       tempoSlider;
        juce::Label        presetFolderCaption;
        juce::Label        presetFolderText;
        juce::TextButton   presetFolderBrowse { "BROWSE" };
        juce::Label        sampleFolderCaption;
        juce::Label        sampleFolderText;
        juce::TextButton   sampleFolderBrowse { "BROWSE" };
        std::unique_ptr<juce::FileChooser> chooser;   // kept alive while the dialog is up

    };

    /** Opens the Settings dialog as a modal DialogWindow centred around centreAround.
        Only Sgraffito passes true for showSampleFolderRow. */
    void showSettingsDialog (Store& store, juce::Component* centreAround,
                             bool showSampleFolderRow = false);

}  // namespace SuiteSettings
