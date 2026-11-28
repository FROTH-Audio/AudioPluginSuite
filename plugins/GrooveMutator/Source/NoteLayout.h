#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <vector>

#include "PluginProcessor.h"

/**
    NoteLayout — named lane-to-note maps for Groove Mutator's output (the note-layout
    template engine).

    A layout maps the eight percussion lanes to the MIDI notes they send. Applying a
    layout writes the eight trackNNote parameters: the GRID note dropdowns follow through
    their attachments, the host sees normal parameter changes, and sessions save the
    result. Built-in layouts are compiled in; user layouts live as XML files (one per
    layout) under userDocuments/AudioSuite/GrooveMutator/NoteLayouts/.

    The active layout's name is derived, not stored: the UI compares the current eight
    note values against the known layouts and shows "Custom" when nothing matches. So no
    parameter or state-format change comes with this engine.
*/
namespace NoteLayout
{
    constexpr int numLanes = GrooveMutatorProcessor::numTracks;
    constexpr int minNote = 24;      // the trackNNote parameter range
    constexpr int maxNote = 96;
    constexpr int formatVersion = 1;

    struct Layout
    {
        juce::String name;
        std::array<int, numLanes> notes {};
        bool builtIn = false;
        juce::File file;              // user layouts only
    };

    /** The compiled-in layouts, in display order. */
    std::vector<Layout> builtInLayouts();

    /** Every valid layout file in the folder, sorted by name; invalid files are skipped. */
    std::vector<Layout> scanUserLayouts (const juce::File& folder);

    /** The user-layout folder (created on first save). Tests redirect it through
        folderOverride() so they never touch the real Documents folder. */
    juce::File defaultFolder();
    juce::File& folderOverride();

    /** Reads a layout file. Rejects wrong roots/versions, missing or duplicate lanes and
        out-of-range notes; `out` is left untouched on failure. */
    juce::Result parse (const juce::File& file, Layout& out);

    /** Writes a layout file (creating the folder when needed). */
    juce::Result write (const juce::File& file, const Layout& layout);

    /** Applies the layout: sets the eight note parameters and notifies the host. */
    void applyToParameters (juce::AudioProcessorValueTreeState& apvts, const Layout& layout);

    /** The eight lanes' current note values, read from the parameters themselves — NOT
        from getRawParameterValue's atomics, which lag one listener-notification behind
        while the APVTS's adapter (registered first, called last) has not updated them
        yet. */
    std::array<int, numLanes> currentNotes (const juce::AudioProcessorValueTreeState& apvts);

    /** The layout whose notes match the apvts's current values, or nullptr (Custom). */
    const Layout* matchParameters (const juce::AudioProcessorValueTreeState& apvts,
                                   const std::vector<Layout>& layouts);
}  // namespace NoteLayout
