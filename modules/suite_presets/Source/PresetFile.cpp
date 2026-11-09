#include "SuitePresets.h"

#include "SuiteState.h"

#include <algorithm>

namespace SuitePresets::PresetFile
{
    juce::File getUserPresetFolder (const juce::String& pluginId)
    {
        // Created lazily: scanning an unused plugin must not write into the user's Documents folder.
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile ("AudioSuite")
            .getChildFile (pluginId)
            .getChildFile ("Presets");
    }

    juce::String sanitiseFileName (const juce::String& displayName)
    {
        juce::String result;

        for (auto character : displayName.trim())
        {
            if (character == '\\' || character == '/' || character == ':' || character == '*'
                || character == '?' || character == '"' || character == '<' || character == '>'
                || character == '|' || character < 32)
                result << '_';
            else
                result << character;
        }

        result = result.trimCharactersAtStart (" .");
        result = result.trimCharactersAtEnd (" .");

        if (result.isEmpty())
            result = "Preset";

        if (result.length() > 80)
            result = result.substring (0, 80).trimCharactersAtEnd (" .");

        return result;
    }

    Result write (const juce::File& file, const PresetInfo& preset, const juce::ValueTree& state,
                  int stateVersion, const juce::String& pluginVersion)
    {
        Result result;

        if (! state.isValid() || ! state.hasType ("PARAMETERS"))
        {
            result.error = "The preset state is empty.";
            return result;
        }

        auto parent = file.getParentDirectory();

        if (! parent.isDirectory() && ! parent.createDirectory())
        {
            result.error = "Cannot create the preset folder: " + parent.getFullPathName();
            return result;
        }

        juce::XmlElement root ("AudioSuitePreset");
        root.setAttribute ("formatVersion", 1);
        root.setAttribute ("plugin", preset.plugin);
        root.setAttribute ("pluginVersion", pluginVersion);

        auto* meta = root.createNewChildElement ("Meta");
        meta->setAttribute ("name", preset.name);
        meta->setAttribute ("category", preset.category);
        meta->setAttribute ("author", "FROTH Audio");
        meta->setAttribute ("description", preset.description);

        auto parameters = state.createCopy();
        parameters.setProperty (SuiteState::versionProperty, stateVersion, nullptr);

        if (auto xml = parameters.createXml())
            root.addChildElement (xml.release());

        const auto text = juce::String ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n") + root.toString();

        if (! file.replaceWithText (text))
        {
            result.error = "Cannot write the preset file: " + file.getFullPathName();
            return result;
        }

        result.ok = true;
        result.preset = preset;
        result.preset.file = file;
        return result;
    }

    Result read (const juce::File& file, const juce::String& expectedPluginId, int currentStateVersion)
    {
        Result result;

        const auto xml = juce::XmlDocument::parse (file);

        if (xml == nullptr)
        {
            result.error = "Not a readable preset file: " + file.getFullPathName();
            return result;
        }

        if (! xml->hasTagName ("AudioSuitePreset"))
        {
            result.error = "Not an AudioSuite preset: " + file.getFullPathName();
            return result;
        }

        const int formatVersion = xml->getIntAttribute ("formatVersion", 1);

        if (formatVersion > 1)
        {
            result.error = "This preset uses a newer file format (" + juce::String (formatVersion) + ").";
            return result;
        }

        const auto plugin = xml->getStringAttribute ("plugin");

        if (plugin != expectedPluginId)
        {
            result.error = "This preset belongs to \"" + plugin + "\", not \"" + expectedPluginId + "\".";
            return result;
        }

        const auto* parameters = xml->getChildByName ("PARAMETERS");

        if (parameters == nullptr)
        {
            result.error = "The preset has no parameter state.";
            return result;
        }

        auto state = juce::ValueTree::fromXml (*parameters);

        if (! state.isValid() || ! state.hasType ("PARAMETERS"))
        {
            result.error = "The preset parameter state is invalid.";
            return result;
        }

        const int storedVersion = (int) state.getProperty (SuiteState::versionProperty, 1);

        if (currentStateVersion >= 0 && storedVersion > currentStateVersion)
        {
            result.error = "This preset was saved by a newer version of the plugin.";
            return result;
        }

        result.ok = true;
        result.preset.plugin = plugin;
        result.preset.isFactory = false;
        result.preset.file = file;
        result.preset.state = state;

        if (const auto* meta = xml->getChildByName ("Meta"))
        {
            result.preset.name = meta->getStringAttribute ("name");
            result.preset.category = meta->getStringAttribute ("category");
            result.preset.description = meta->getStringAttribute ("description");
        }

        if (result.preset.name.isEmpty())
            result.preset.name = file.getFileNameWithoutExtension();

        if (result.preset.category.isEmpty())
            result.preset.category = "User";

        return result;
    }

    std::vector<PresetInfo> scanUserPresets (const juce::String& pluginId)
    {
        std::vector<PresetInfo> presets;
        const auto folder = getUserPresetFolder (pluginId);

        if (! folder.isDirectory())
            return presets;

        for (const auto& entry : juce::RangedDirectoryIterator (folder, false, "*.ausp",
                                                               juce::File::findFiles))
        {
            auto readResult = read (entry.getFile(), pluginId);

            if (readResult.ok)
                presets.push_back (std::move (readResult.preset));
        }

        std::sort (presets.begin(), presets.end(), [] (const auto& a, const auto& b)
        {
            return a.name.compareIgnoreCase (b.name) < 0;
        });

        return presets;
    }
}

namespace SuitePresets
{
    PresetMenuModel PresetMenuModel::build (const std::vector<PresetInfo>& factory,
                                            const std::vector<PresetInfo>& user,
                                            int currentFlatIndex)
    {
        PresetMenuModel model;
        model.currentFlatIndex = currentFlatIndex;

        auto add = [&model, currentFlatIndex] (const std::vector<PresetInfo>& list, bool isFactory, int offset)
        {
            for (int i = 0; i < (int) list.size(); ++i)
            {
                const auto flatIndex = offset + i;
                model.items.push_back ({ list[(size_t) i].name, list[(size_t) i].category, flatIndex,
                                         isFactory, flatIndex == currentFlatIndex });
            }
        };

        add (factory, true, 0);
        add (user, false, (int) factory.size());
        return model;
    }

    juce::PopupMenu PresetMenuModel::createMenu() const
    {
        juce::PopupMenu menu;
        juce::PopupMenu factoryMenu;
        juce::PopupMenu userMenu;

        auto addGrouped = [] (juce::PopupMenu& parent, const std::vector<Item>& items, bool isFactory)
        {
            juce::StringArray categories;

            for (const auto& item : items)
                if (item.isFactory == isFactory && ! categories.contains (item.category))
                    categories.add (item.category);

            for (const auto& category : categories)
            {
                juce::PopupMenu categoryMenu;

                for (const auto& item : items)
                    if (item.isFactory == isFactory && item.category == category)
                        categoryMenu.addItem (item.flatIndex + 1, item.name, true, item.isCurrent);

                parent.addSubMenu (category, categoryMenu);
            }
        };

        addGrouped (factoryMenu, items, true);
        addGrouped (userMenu, items, false);

        if (factoryMenu.containsAnyActiveItems())
            menu.addSubMenu ("Factory", factoryMenu);

        if (userMenu.containsAnyActiveItems())
            menu.addSubMenu ("User", userMenu);

        return menu;
    }
}
