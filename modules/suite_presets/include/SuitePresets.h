#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_core/juce_core.h>
#include <memory>
#include <vector>
#include <string>

namespace SuitePresets
{
    /** A single factory or user preset: its metadata and the state delta. */
    struct PresetInfo
    {
        juce::String name;
        juce::String category;
        juce::String description;
        juce::String plugin;          // getPresetPluginId() of the owning plugin
        juce::ValueTree state;        // root <PARAMETERS>; may be empty (defaults)
        bool isFactory = false;
        juce::File file;              // user presets only; empty for factory
    };

    /** Result of a preset file read/write operation. */
    struct Result
    {
        bool ok = false;
        juce::String error;
        PresetInfo preset;            // only valid when ok && reading
    };

    /** The identity of the preset the bar last applied: which factory preset or user
        file, or none (Init). The processor owns it, so it survives the editor being
        destroyed and reopened, and it travels in saved sessions as the
        presetSelection state property (plan 09 1.4, the lerpCurve/samplePath pattern). */
    struct PresetSelection
    {
        enum class Kind { none, factory, userFile };
        Kind kind = Kind::none;          // none = Init / not a preset
        int factoryIndex = -1;           // stable: bank order is append-only
        juce::String userFilePath;       // user presets: identity by file path, not list
                                         // position (the scanned user list re-orders when
                                         // files change between window opens)

        bool operator== (const PresetSelection& other) const noexcept
        {
            return kind == other.kind
                && factoryIndex == other.factoryIndex
                && userFilePath == other.userFilePath;
        }

        bool operator!= (const PresetSelection& other) const noexcept { return ! (*this == other); }
    };

    /** Text form of a selection for the presetSelection state property:
        "factory:<index>", "user:<path>", or "none" (an absent property also means none). */
    inline juce::String encodePresetSelection (const PresetSelection& selection)
    {
        if (selection.kind == PresetSelection::Kind::factory)
            return "factory:" + juce::String (selection.factoryIndex);

        if (selection.kind == PresetSelection::Kind::userFile)
            return "user:" + selection.userFilePath;

        return "none";
    }

    /** Reads back encodePresetSelection. Anything unrecognised (including an empty
        property from a session saved before plan 09) is Init. */
    inline PresetSelection decodePresetSelection (const juce::String& text)
    {
        PresetSelection selection;

        if (text.startsWith ("factory:"))
        {
            selection.kind = PresetSelection::Kind::factory;
            selection.factoryIndex = text.substring (8).getIntValue();
        }
        else if (text.startsWith ("user:"))
        {
            selection.kind = PresetSelection::Kind::userFile;
            selection.userFilePath = text.substring (5);
        }

        return selection;
    }

    /** Implemented by each processor. The one place preset state enters and leaves a plugin. */
    class StateHolder
    {
    public:
        virtual ~StateHolder() = default;
        virtual juce::String getPresetPluginId() const = 0;                 // "SpectralDrift", ...
        virtual int getPresetStateVersion() const = 0;                      // SuiteState version
        virtual juce::AudioProcessorValueTreeState& getPresetStateTree() = 0;
        virtual void applyPresetState (const juce::ValueTree&) = 0;         // message thread only

        /** The preset the bar last applied (plan 09 1.4). Message thread. The default
            bodies keep implementers that do not persist a selection compiling and
            behaving as before: no selection, nothing recorded. */
        virtual PresetSelection getPresetSelection() const { return {}; }
        virtual void setPresetSelection (const PresetSelection&) {}
    };

    /** True when the embedded factory bank is compiled in (SUITE_PRESET_BANK). Lets tests and
        editors ask without #if. The bar itself ships in every build. */
    inline bool isAvailable() noexcept { return SUITE_PRESET_BANK != 0; }

    /**
        Preset header bar. Shows factory and user presets, INIT, SAVE, and a right-click menu.
        Ships in every build; with SUITE_PRESET_BANK=0 the factory list is empty.
    */
    class PresetBar : public juce::Component
    {
    public:
        PresetBar (StateHolder& holder, juce::Colour accent);
        ~PresetBar() override;

        int getPreferredHeight() const noexcept;      // 30
        void paint (juce::Graphics&) override;
        void resized() override;

        /** Called on the message thread after a preset has been applied. */
        std::function<void()> onPresetLoaded;

        /** Message thread. Used by the host-program bridge and tests. */
        void selectPreset (int factoryIndex);
        int  getNumFactoryPresets() const noexcept;   // real bank size (0 without the bank)

        /** The preset name the bar shows now ("Init" when nothing is selected). Used by
            tests; the bar's own restore keeps it in step with the holder (plan 09 1.4). */
        juce::String getCurrentPresetName() const;

        /** True when the state has moved from the applied preset's snapshot (the bar
            shows the name with a trailing " *"). */
        bool isPresetDirty() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };

    namespace PresetFile
    {
        /** %APPDATA%/FROTH Audio/<pluginId>/Presets on Windows, or the platform equivalent. */
        juce::File getUserPresetFolder (const juce::String& pluginId);

        /** Filesystem-safe file name stem. The display name is kept separately. */
        juce::String sanitiseFileName (const juce::String& displayName);

        /** Writes one .ausp file. state must be a <PARAMETERS> tree (apvts.copyState()). */
        Result write (const juce::File& file, const PresetInfo& preset, const juce::ValueTree& state,
                      int stateVersion, const juce::String& pluginVersion = "1.0.0");

        /** Reads one .ausp file. The plugin id must match expectedPluginId. Pass -1 to skip the
            state-version check (used when scanning the menu). */
        Result read (const juce::File& file, const juce::String& expectedPluginId,
                     int currentStateVersion = -1);

        /** Reads only the metadata, for the menu. Unreadable files are skipped. */
        std::vector<PresetInfo> scanUserPresets (const juce::String& pluginId);
    }

    /** The menu model: factory and user entries, grouped by category, with the current one ticked. */
    struct PresetMenuModel
    {
        struct Item
        {
            juce::String name;
            juce::String category;
            int flatIndex = -1;       // index in the combined factory+user list
            bool isFactory = false;
            bool isCurrent = false;
        };

        std::vector<Item> items;
        int currentFlatIndex = -1;

        static PresetMenuModel build (const std::vector<PresetInfo>& factory,
                                      const std::vector<PresetInfo>& user,
                                      int currentFlatIndex);

        /** Command ids are flatIndex + 1. Factory and user are submenus; categories are submenus. */
        juce::PopupMenu createMenu() const;
    };
}
