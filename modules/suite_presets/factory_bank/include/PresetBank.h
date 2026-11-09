#pragma once

#include "SuitePresets.h"

#include <vector>

namespace SuitePresets
{
    /** The embedded factory bank, parsed once from AudioSuitePresets.xml.

        The bank is optional: it is compiled in only under SUITE_PRESET_BANK, and adds one CTest
        entry per plugin on top of the always-present file tests. Without it every accessor sees
        an empty bank, so the same sources build with or without it. */
    class PresetBank
    {
    public:
        static const std::vector<PresetInfo>& getFactoryPresets();
        static int getNumFactoryPresets();
        static const PresetInfo* getFactoryPreset (int index);

        /**
            Merges a JSON object { "paramId": value, ... } onto the default state. Keys beginning
            with "_" are metadata (_name, _category, _description) and are not parameters. Used by
            tools/build_bank.py and by the tests.
        */
        static juce::ValueTree parseDelta (const juce::String& json, const juce::ValueTree& defaultState);
    };

    /**
        Host program list over the factory bank. Index 0 is Init (every parameter at its default);
        1 to N are the plugin's factory presets in bank order. The order is append-only, because
        hosts cache the program index in sessions. Only meaningful when a bank is embedded
        (SUITE_PRESET_BANK); otherwise there are no programs.
    */
    namespace PresetPrograms
    {
        int getNumPrograms (const juce::String& pluginId);              // N + 1, never 0
        juce::String getProgramName (const juce::String& pluginId, int index);
        bool applyProgram (StateHolder& holder, int index);             // false for an invalid index
    }
}
