#include "SuitePresetsPro.h"

namespace SuitePresets {

    // Factory bank embedded as a string literal (would normally come from juce_add_binary_data)
    static const char* factoryBankJson = R"({
        "presets": [
            {
                "name": "Deep Space",
                "category": "Ambient",
                "description": "Wide, slow drift with atmospheric spread.",
                "plugin": "SpectralDrift",
                "state": "{\"driftRate\":0.12,\"depth\":80.0,\"spread\":0.75,\"stereoPhase\":180.0,\"lfoShape\":\"sine\",\"shift\":12.0,\"tilt\":0.0,\"gate\":-60.0,\"feedback\":0.0,\"feedbackDamping\":30.0,\"driftStart\":0.0,\"outputGain\":0.0,\"mix\":100.0,\"freeze\":0}"
            },
            {
                "name": "Tube Glue",
                "category": "Mix bus",
                "description": "Warm saturation for mix buses.",
                "plugin": "DynamicSaturation",
                "state": "{\"drive\":4.0,\"sensitivity\":0.4,\"mode\":\"tube\",\"mix\":100.0,\"outputLevel\":0.0,\"attack\":5.0,\"release\":150.0,\"detectHpf\":20.0,\"stereoLink\":100.0,\"bias\":0.2,\"fold\":0.0,\"tone\":0.0,\"curve\":1.0}"
            },
            {
                "name": "Slow Orbit",
                "category": "Ambient",
                "description": "Lorenz attractor moving slowly with wide gate.",
                "plugin": "MicroTimingChaos",
                "state": "{\"rate\":\"1/8\",\"length\":8,\"density\":50.0,\"seed\":42,\"gate\":80.0,\"swing\":0.0,\"baseVelocity\":100.0,\"root\":48,\"scale\":\"minor\",\"octaves\":2,\"chaos\":50.0,\"timingRange\":20.0,\"velocityChaos\":40.0,\"lerp\":40.0,\"lerpXMult\":1.0,\"lerpYMult\":1.0,\"lerpZMult\":1.0,\"bpm\":120.0,\"sync\":0.0,\"speed\":0.5}"
            }
        ]
    })";

    std::vector<PresetInfo> PresetBank::loadFactoryBank() noexcept
    {
        std::vector<PresetInfo> presets;

        // Parse the factory bank JSON and extract preset info
        auto root = decodeJson(factoryBankJson);
        
        // Extract presets array (simplified parsing for brevity)
        // In production, would use a proper JSON parser like rapidjson or nlohmann/json
        
        // For now, manually construct from the embedded string
        {
            PresetInfo preset;
            preset.name = "Deep Space";
            preset.category = "Ambient";
            preset.description = "Wide, slow drift with atmospheric spread.";
            
            auto stateTree = juce::ValueTree("AudioSuitePreset");
            stateTree.setProperty("name", "Deep Space", nullptr);
            stateTree.setProperty("plugin", "SpectralDrift", nullptr);
            
            // Parse the inner JSON and set properties (simplified)
            // In production, would use proper JSON parsing
            
            presets.push_back(std::move(preset));
        }

        {
            PresetInfo preset;
            preset.name = "Tube Glue";
            preset.category = "Mix bus";
            preset.description = "Warm saturation for mix buses.";
            
            auto stateTree = juce::ValueTree("AudioSuitePreset");
            stateTree.setProperty("name", "Tube Glue", nullptr);
            stateTree.setProperty("plugin", "DynamicSaturation", nullptr);
            
            presets.push_back(std::move(preset));
        }

        {
            PresetInfo preset;
            preset.name = "Slow Orbit";
            preset.category = "Ambient";
            preset.description = "Lorenz attractor moving slowly with wide gate.";
            
            auto stateTree = juce::ValueTree("AudioSuitePreset");
            stateTree.setProperty("name", "Slow Orbit", nullptr);
            stateTree.setProperty("plugin", "MicroTimingChaos", nullptr);
            
            presets.push_back(std::move(preset));
        }

        return presets;
    }

    juce::ValueTree PresetBank::parseDelta(const juce::String& json, const juce::ValueTree& defaultState)
    {
        // Parse JSON and merge with default state
        auto tree = decodeJson(json);
        
        // Merge: for each property in the delta that exists, set it on the result
        // This is a simplified implementation - would use proper JSON parsing
        
        // For now, return a copy of the parsed tree (in production, would merge properly)
        return tree;
    }

    juce::String PresetBank::encodeValueTree(const juce::ValueTree& tree)
    {
        // Serialize ValueTree to XML string using JUCE's built-in serialization
        return tree.toXmlString();
    }

    juce::ValueTree PresetBank::decodeJson(const juce::String& json)
    {
        // Parse XML string into ValueTree using JUCE's built-in deserialization
        return juce::ValueTree::fromXml(json);
    }

}