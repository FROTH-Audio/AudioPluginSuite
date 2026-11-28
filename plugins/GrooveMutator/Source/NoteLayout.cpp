#include "NoteLayout.h"

#include <algorithm>

namespace NoteLayout
{
    namespace
    {
        // The compiled-in table (built-ins never share eight notes — the exact-match
        // rule would make a duplicate unreachable). Names are user-visible and become
        // the SAVE AS prefill, so keep them legal file names (no slashes) and append
        // only. GM Standard's notes are the plugin's defaults
        // (GrooveMutatorProcessor::defaultTrackNotes); Toontrack EZ/Superior, Addictive
        // Drums, Steven Slate Drums, BFD and NI Studio Drummer default to the same map,
        // which the help text and manual document instead of duplicating here.
        // Ableton Drum Rack's default pads run C1 (36) upward, chromatic — the same map
        // the generic sequential-pad entry carried.
        struct BuiltIn
        {
            const char* name;
            std::array<int, numLanes> notes;
        };

        const BuiltIn builtInTable[] = {
            { "GM Standard",     { 36, 38, 42, 46, 41, 45, 39, 75 } },
            { "GM Toms 43-47",   { 36, 38, 42, 46, 43, 47, 39, 75 } },
            { "GM Percussion",   { 36, 38, 42, 46, 41, 45, 54, 56 } },
            { "Ableton Drum Rack (Default)", { 36, 37, 38, 39, 40, 41, 42, 43 } },
            { "MPC A01-A08",     { 37, 38, 39, 40, 41, 42, 43, 44 } },
        };
    }

    std::vector<Layout> builtInLayouts()
    {
        std::vector<Layout> layouts;
        for (const auto& entry : builtInTable)
        {
            Layout layout;
            layout.name = entry.name;
            layout.notes = entry.notes;
            layout.builtIn = true;
            layouts.push_back (layout);
        }
        return layouts;
    }

    juce::File& folderOverride()
    {
        static juce::File overridden;
        return overridden;
    }

    juce::File defaultFolder()
    {
        if (folderOverride() != juce::File())
            return folderOverride();

        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile ("AudioSuite")
            .getChildFile ("GrooveMutator")
            .getChildFile ("NoteLayouts");
    }

    juce::Result parse (const juce::File& file, Layout& out)
    {
        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr)
            return juce::Result::fail ("Not valid XML: " + file.getFullPathName());

        if (! xml->hasTagName ("AudioSuiteNoteLayout"))
            return juce::Result::fail ("Not an AudioSuite note layout: " + file.getFullPathName());

        if (xml->getIntAttribute ("formatVersion", 0) != formatVersion)
            return juce::Result::fail ("Unsupported note-layout version: " + file.getFullPathName());

        const juce::String name = xml->getStringAttribute ("name").trim();
        if (name.isEmpty())
            return juce::Result::fail ("Note layout has no name: " + file.getFullPathName());

        Layout layout;
        layout.name = name;
        layout.file = file;
        layout.builtIn = false;

        int seen = 0;
        for (const auto* child : xml->getChildWithTagNameIterator ("Note"))
        {
            const int lane = child->getIntAttribute ("lane", 0);
            if (lane < 1 || lane > numLanes)
                return juce::Result::fail ("Note lane out of range: " + file.getFullPathName());

            const int index = lane - 1;
            if (layout.notes[(size_t) index] != 0)
                return juce::Result::fail ("Duplicate note lane: " + file.getFullPathName());

            const int value = child->getIntAttribute ("value", 0);
            if (value < minNote || value > maxNote)
                return juce::Result::fail ("Note out of range: " + file.getFullPathName());

            layout.notes[(size_t) index] = value;
            ++seen;
        }

        if (seen != numLanes)
            return juce::Result::fail ("Note layout must define eight notes: " + file.getFullPathName());

        out = layout;
        return juce::Result::ok();
    }

    juce::Result write (const juce::File& file, const Layout& layout)
    {
        if (layout.name.trim().isEmpty())
            return juce::Result::fail ("Note layout has no name.");

        for (const int note : layout.notes)
            if (note < minNote || note > maxNote)
                return juce::Result::fail ("Note out of range: " + juce::String (note));

        juce::XmlElement root ("AudioSuiteNoteLayout");
        root.setAttribute ("formatVersion", formatVersion);
        root.setAttribute ("name", layout.name.trim());

        for (int lane = 1; lane <= numLanes; ++lane)
        {
            auto* note = root.createNewChildElement ("Note");
            note->setAttribute ("lane", lane);
            note->setAttribute ("value", layout.notes[(size_t) (lane - 1)]);
        }

        if (! file.getParentDirectory().createDirectory())
            return juce::Result::fail ("Could not create the note-layout folder: "
                                          + file.getParentDirectory().getFullPathName());

        if (! root.writeTo (file))
            return juce::Result::fail ("Could not write: " + file.getFullPathName());

        return juce::Result::ok();
    }

    std::vector<Layout> scanUserLayouts (const juce::File& folder)
    {
        std::vector<Layout> layouts;
        juce::RangedDirectoryIterator iterator (folder, false, "*.xml", juce::File::findFiles);
        for (const auto& entry : iterator)
        {
            Layout layout;
            if (parse (entry.getFile(), layout).wasOk())
                layouts.push_back (layout);
        }

        std::sort (layouts.begin(), layouts.end(),
                   [] (const Layout& a, const Layout& b)
                   { return a.name.compareIgnoreCase (b.name) < 0; });
        return layouts;
    }

    void applyToParameters (juce::AudioProcessorValueTreeState& apvts, const Layout& layout)
    {
        for (int lane = 0; lane < numLanes; ++lane)
        {
            auto* parameter = apvts.getParameter (GrooveMutatorProcessor::trackNoteIds[lane]);
            if (parameter == nullptr)
                continue;
            const int note = juce::jlimit (minNote, maxNote, layout.notes[(size_t) lane]);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) note));
        }
    }

    std::array<int, numLanes> currentNotes (const juce::AudioProcessorValueTreeState& apvts)
    {
        std::array<int, numLanes> notes {};
        for (int lane = 0; lane < numLanes; ++lane)
            if (auto* parameter = apvts.getParameter (GrooveMutatorProcessor::trackNoteIds[lane]))
                notes[(size_t) lane] = juce::jlimit (minNote, maxNote,
                    juce::roundToInt (parameter->convertFrom0to1 (parameter->getValue())));
        return notes;
    }

    const Layout* matchParameters (const juce::AudioProcessorValueTreeState& apvts,
                                   const std::vector<Layout>& layouts)
    {
        const auto current = currentNotes (apvts);

        for (const auto& layout : layouts)
            if (layout.notes == current)
                return &layout;

        return nullptr;
    }
}  // namespace NoteLayout
