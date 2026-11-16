#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_core/juce_core.h>
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

    // Row builder struct definitions - at namespace scope for visibility
    struct TooltipsRowBuilder { juce::Label* label; juce::ToggleButton* toggle; juce::Label* valueBox; juce::Rectangle<int> area; };
    struct TempoRowBuilder { juce::Label* label; juce::Slider* slider; juce::Rectangle<int> area; };
    struct PresetFolderRowBuilder { juce::Label* label; juce::Label* text; juce::TextButton* browse; juce::Rectangle<int> area; };
    struct SampleFolderRowBuilder { juce::Label* label; juce::Label* text; juce::TextButton* browse; juce::Rectangle<int> area; };

    /**
        A settings dialog with four rows: tooltips toggle, fallback tempo slider, preset folder
        browse, and Sgraffito's sample folder. The store is owned by this dialog so changes auto-save.
        Non-Sgraffito callers pass nullptr for the sample-folder row; it stays hidden then.
    */
    class SettingsDialog : public juce::Component
    {
    public:
        explicit SettingsDialog (Store& storeToOwn, juce::Component* centreAround);
        ~SettingsDialog();

        void paint (juce::Graphics&) override;

    private:
        void updateTooltipsRow();
        void updateTempoRow();
        void updatePresetFolderRow();
        void updateSampleFolderRow();

        void applyTooltipsSetting();
        void applyTempoSetting();
        void applyPresetFolderSetting();
        void applySampleFolderSetting();

        void browsePresetFolder();
        void browseSampleFolder();
        void buttonPressed (juce::Button* buttonThatWasPressed);

        void buildTooltipsRow();
        void buildTempoRow();
        void buildPresetFolderRow();
        void buildSampleFolderRow();

        Store& store;
        bool showTooltips;
        double fallbackTempo;
        juce::String presetPath;
        juce::String samplePath;
        bool shouldBuildSampleFolderRow = true;

        juce::TooltipWindow tooltipWindow;

    };

    /** Opens the Settings dialog as a modal DialogWindow centred around centreAround. */
    void showSettingsDialog (Store& store, juce::Component* centreAround);

    // Forward declarations for static row builder instances
    extern TooltipsRowBuilder tooltipsRow;
    extern TempoRowBuilder tempoRow;
    extern PresetFolderRowBuilder presetFolderRow;
    extern SampleFolderRowBuilder sampleFolderRow;

}  // namespace SuiteSettings
