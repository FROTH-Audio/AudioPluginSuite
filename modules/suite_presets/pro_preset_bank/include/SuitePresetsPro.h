#pragma once

#include "SuitePresets.h"
#include <memory>  // for std::unique_ptr
#include <vector>   // for std::vector

namespace SuitePresets {

    /** Pro-only bank: parses JSON deltas and materialises full .ausp presets. */
    
    struct PresetInfo
    {
        juce::String name;
        juce::String category;
        juce::String description;
        std::unique_ptr<juce::ValueTree> state;  // full preset, ready to apply
    };

    class PresetBank
    {
    public:
        // Factory bank (embedded in binary, loaded at runtime)
        static std::vector<SuitePresets::PresetInfo> loadFactoryBank() noexcept;

        // Delta parsing: takes a JSON delta string and merges it with the default state
        static juce::ValueTree parseDelta(const juce::String& json, const juce::ValueTree& defaultState);

        // Value tree serialization helpers (for .ausp file I/O)
        static juce::String encodeValueTree(const juce::ValueTree& tree);
        static juce::ValueTree decodeJson(const juce::String& json);

    private:
        // Internal helper to merge a JSON object with a ValueTree
        static void mergeJsonObject(const juce::XmlElement* element, juce::ValueTree& tree);
    };

}