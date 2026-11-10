#pragma once

#include "SuitePresets.h"

#include <vector>

namespace SuitePresets
{
    /** The embedded factory bank, parsed once from AudioSuitePresets.xml.

        The free factory bank is always embedded (INIT-only since plan 03.3), so every accessor sees
        it. The build embeds the bank unconditionally; the bar shows the factory presets it parses. */
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
        hosts cache the program index in sessions. The bank is always embedded now, so the list is
        always compiled; with the INIT-only free bank there is always Init + one INIT preset.
    */
    namespace PresetPrograms
    {
        int getNumPrograms (const juce::String& pluginId);              // N + 1, never 0
        juce::String getProgramName (const juce::String& pluginId, int index);
        bool applyProgram (StateHolder& holder, int index);             // false for an invalid index
    }
}
