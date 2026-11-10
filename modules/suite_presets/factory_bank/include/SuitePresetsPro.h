#pragma once

#include "SuitePresets.h"

#include <vector>

namespace SuitePresets
{
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

    /** The embedded factory bank, parsed once from AudioSuitePresets.xml. */
    class PresetBank
    {
    public:
        static const std::vector<PresetInfo>& getFactoryPresets();
        static int getNumFactoryPresets();
        static const PresetInfo* getFactoryPreset (int index);

        /**
            Merges a JSON object { "paramId": value, ... } onto the default state. Keys beginning
            with "_" are metadata (_name, _category, _description) and are not parameters. Used by
            tools/build_bank.py and by the Pro tests.
        */
        static juce::ValueTree parseDelta (const juce::String& json, const juce::ValueTree& defaultState);
    };

    namespace PresetFile
    {
        struct Result
        {
            bool ok = false;
            juce::String error;
            PresetInfo preset;        // read only
        };

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

    /**
        Host program list over the factory bank. Index 0 is Init (every parameter at its default);
        1 to N are the plugin's factory presets in bank order. The order is append-only, because
        hosts cache the program index in sessions. Pro builds only.
    */
    namespace PresetPrograms
    {
        int getNumPrograms (const juce::String& pluginId);              // N + 1, never 0
        juce::String getProgramName (const juce::String& pluginId, int index);
        bool applyProgram (StateHolder& holder, int index);             // false for an invalid index
    }
}
