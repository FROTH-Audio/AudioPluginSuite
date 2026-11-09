#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

namespace SuitePresets
{
    /** Implemented by each processor. The one place preset state enters and leaves a plugin. */
    class StateHolder
    {
    public:
        virtual ~StateHolder() = default;
        virtual juce::String getPresetPluginId() const = 0;                 // "SpectralDrift", ...
        virtual int getPresetStateVersion() const = 0;                      // SuiteState version
        virtual juce::AudioProcessorValueTreeState& getPresetStateTree() = 0;
        virtual void applyPresetState (const juce::ValueTree&) = 0;         // message thread only
    };

    /** The preset system ships in this build. The embedded factory bank is optional; ask
        PresetBank::getNumFactoryPresets() whether this build carries it. */
    inline bool isAvailable() noexcept { return true; }

    /**
        One preset, factory or user. `state` is a <PARAMETERS> tree. Absent parameters are reset to
        their defaults by applyPresetState(), so a preset may carry only the parameters it changes.
    */
    struct PresetInfo
    {
        juce::String name;
        juce::String category;
        juce::String description;
        juce::String plugin;          // getPresetPluginId() of the owning plugin
        juce::ValueTree state;        // root <PARAMETERS>
        bool isFactory = false;
        juce::File file;              // user presets only; empty for factory
    };

    namespace PresetFile
    {
        struct Result
        {
            bool ok = false;
            juce::String error;
            PresetInfo preset;        // read only
        };

        /** userDocuments/AudioSuite/<pluginId>/Presets on Windows, or the platform equivalent. */
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

    /**
        Preset header bar. Shows the factory bank (if the build embeds one), the user presets, and
        INIT / SAVE. The embedded factory bank itself is optional (SUITE_PRESET_BANK) and parsed in
        modules/suite_presets/factory_bank.
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
        int  getNumFactoryPresets() const noexcept;   // real bank size, or 0 without the bank

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
}
