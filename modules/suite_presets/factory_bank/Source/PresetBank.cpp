#include "PresetBank.h"

#include <juce_core/juce_core.h>

#if SUITE_PRESET_BANK
 #include <SuitePresetBankData.h>
#endif

namespace SuitePresets
{
    namespace
    {
        juce::ValueTree treeFromXmlElement (const juce::XmlElement& element)
        {
            // Use createCopy() to ensure we have a complete deep copy of the element
            // including all children. This fixes preset loading for Sgraffito where
            // ValueTree::fromXml was failing with child elements.
            auto copy = element.createCopy();
            auto tree = juce::ValueTree::fromXml (copy);
            return tree.isValid() ? tree : juce::ValueTree();
        }

        std::vector<PresetInfo> readBank (const juce::String& xmlText, std::vector<PresetInfo>& bank)
        {
            if (auto xml = juce::XmlDocument::parse (xmlText))
                for (auto* child = xml->getFirstChildElement(); child != nullptr; child = child->getNextElement())
                {
                    if (! child->hasTagName ("AudioSuitePreset"))
                        continue;

                    PresetInfo preset;
                    preset.plugin = child->getStringAttribute ("plugin");
                    preset.isFactory = true;

                    if (const auto* meta = child->getChildByName ("Meta"))
                    {
                        preset.name = meta->getStringAttribute ("name");
                        preset.category = meta->getStringAttribute ("category");
                        preset.description = meta->getStringAttribute ("description");
                    }

                    if (const auto* parameters = child->getChildByName ("PARAMETERS"))
                        preset.state = treeFromXmlElement (*parameters);

                    if (preset.plugin.isEmpty() || preset.name.isEmpty() || ! preset.state.isValid())
                        continue;

                    bank.push_back (std::move (preset));
                }

            return std::move (bank);
        }

        std::vector<PresetInfo> embeddedBank()
        {
#if SUITE_PRESET_BANK
            const auto xmlText = juce::String::fromUTF8 (SuitePresetBankData::AudioSuitePresets_xml,
                                                         SuitePresetBankData::AudioSuitePresets_xmlSize);

            std::vector<PresetInfo> bank;
            return readBank (xmlText, bank);
#else
            return {};
#endif
        }
    }

    const std::vector<PresetInfo>& PresetBank::getFactoryPresets()
    {
        static const auto bank = embeddedBank();
        return bank;
    }

    int PresetBank::getNumFactoryPresets()
    {
        return (int) getFactoryPresets().size();
    }

    const PresetInfo* PresetBank::getFactoryPreset (int index)
    {
        const auto& bank = getFactoryPresets();

        if (index < 0 || index >= (int) bank.size())
            return nullptr;

        return &bank[(size_t) index];
    }

    juce::ValueTree PresetBank::parseDelta (const juce::String& json, const juce::ValueTree& defaultState)
    {
        auto result = defaultState.createCopy();

        if (! result.isValid())
            result = juce::ValueTree ("PARAMETERS");

        const auto parsed = juce::JSON::parse (json);
        const auto* object = parsed.getDynamicObject();

        if (object == nullptr)
            return result;

        const auto& properties = object->getProperties();

        for (int i = 0; i < properties.size(); ++i)
        {
            const auto id = properties.getName (i).toString();

            if (id.startsWith ("_"))
                continue;

            const auto value = properties.getValueAt (i);

            if (! (value.isInt() || value.isInt64() || value.isDouble() || value.isBool()))
                continue;

            // Only existing IDs are touched: a typo in a delta is a bank error, not a new parameter.
            for (int childIndex = 0; childIndex < result.getNumChildren(); ++childIndex)
            {
                auto child = result.getChild (childIndex);

                if (child.getProperty ("id").toString() == id)
                {
                    child.setProperty ("value", (double) value, nullptr);
                    break;
                }
            }
        }

        return result;
    }

    namespace PresetPrograms
    {
        namespace
        {
            std::vector<const PresetInfo*> presetsForPlugin (const juce::String& pluginId)
            {
                std::vector<const PresetInfo*> result;

                for (const auto& preset : PresetBank::getFactoryPresets())
                    if (preset.plugin == pluginId)
                        result.push_back (&preset);

                return result;
            }
        }

        int getNumPrograms (const juce::String& pluginId)
        {
            return (int) presetsForPlugin (pluginId).size() + 1;
        }

        juce::String getProgramName (const juce::String& pluginId, int index)
        {
            if (index == 0)
                return "Init";

            const auto presets = presetsForPlugin (pluginId);

            if (index < 1 || index > (int) presets.size())
                return {};

            return presets[(size_t) (index - 1)]->name;
        }

        bool applyProgram (StateHolder& holder, int index)
        {
            if (index == 0)
            {
                holder.applyPresetState (juce::ValueTree ("PARAMETERS"));
                return true;
            }

            const auto presets = presetsForPlugin (holder.getPresetPluginId());

            if (index < 1 || index > (int) presets.size())
                return false;

            holder.applyPresetState (presets[(size_t) (index - 1)]->state);
            return true;
        }
    }
}
