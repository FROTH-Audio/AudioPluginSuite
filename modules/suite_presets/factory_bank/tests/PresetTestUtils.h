#pragma once

// Shared helpers and the tests that are identical for every plugin. Each test executable compiles
// one plugin's sources, so the helpers are templates over the concrete processor type.

#undef NDEBUG
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <set>
#include <vector>

#include "SuitePresets.h"
#include "PresetBank.h"
#include "SuiteState.h"

namespace PresetTest
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    inline void pass (const char* name)
    {
        std::printf ("[PASS] %s\n", name);
    }

    /** The factory presets that belong to one plugin, in bank order. */
    inline std::vector<SuitePresets::PresetInfo> presetsFor (const juce::String& pluginId)
    {
        std::vector<SuitePresets::PresetInfo> result;

        for (const auto& preset : SuitePresets::PresetBank::getFactoryPresets())
            if (preset.plugin == pluginId)
                result.push_back (preset);

        return result;
    }

    /** Reads a PARAM value from a preset tree. Returns NaN when the id is absent. */
    inline float treeValue (const juce::ValueTree& tree, const juce::String& id)
    {
        for (int i = 0; i < tree.getNumChildren(); ++i)
        {
            const auto child = tree.getChild (i);

            if (child.getProperty ("id").toString() != id)
                continue;

            const auto value = child.getProperty ("value");
            return value.isString() ? value.toString().getFloatValue() : (float) (double) value;
        }

        return std::numeric_limits<float>::quiet_NaN();
    }

    /** True when every parameter equals its default, within a small tolerance. */
    inline bool isDefaultState (juce::AudioProcessor& processor)
    {
        for (auto* parameter : processor.getParameters())
            if (std::abs (parameter->getValue() - parameter->getDefaultValue()) > 1.0e-6f)
                return false;

        return true;
    }

    inline float defaultOf (juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
    {
        auto* parameter = apvts.getParameter (id);
        assert (parameter != nullptr);
        return parameter->convertFrom0to1 (parameter->getDefaultValue());
    }

    /** Checks every id the preset names exists, and its value is inside the live parameter's range. */
    inline void checkPresetWithinRanges (juce::AudioProcessorValueTreeState& apvts,
                                         const SuitePresets::PresetInfo& preset)
    {
        for (int i = 0; i < preset.state.getNumChildren(); ++i)
        {
            const auto id = preset.state.getChild (i).getProperty ("id").toString();
            auto* parameter = apvts.getParameter (id);

            assert (parameter != nullptr);

            const auto range = parameter->getNormalisableRange();
            const float value = treeValue (preset.state, id);
            const float tolerance = 1.0e-3f * juce::jmax (1.0f, std::abs (value));

            assert (value >= range.start - tolerance);
            assert (value <= range.end + tolerance);
        }
    }

    /** Applies a preset, then checks every id it names reads back at the stored value. */
    inline void checkPresetApplied (juce::AudioProcessorValueTreeState& apvts,
                                    const SuitePresets::PresetInfo& preset)
    {
        for (int i = 0; i < preset.state.getNumChildren(); ++i)
        {
            const auto id = preset.state.getChild (i).getProperty ("id").toString();
            const float expected = treeValue (preset.state, id);
            const float actual = apvts.getRawParameterValue (id)->load();
            const float tolerance = 1.0e-3f * juce::jmax (1.0f, std::abs (expected));

            if (std::abs (actual - expected) > tolerance)
                std::fprintf (stderr, "checkPresetApplied mismatch: %s expected %.6f actual %.6f (%s)\n",
                              id.toRawUTF8(), expected, actual,
                              dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id)) != nullptr
                                  ? dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id))->getCurrentChoiceName().toRawUTF8()
                                  : "(not a choice)");

            assert (std::abs (actual - expected) <= tolerance);
        }
    }

    /** Number of pixels with any opacity, used to prove an editor actually painted. */
    inline int countPaintedPixels (const juce::Image& image, int step = 4)
    {
        const juce::Image::BitmapData data (image, juce::Image::BitmapData::readOnly);
        int painted = 0;

        for (int y = 0; y < image.getHeight(); y += step)
            for (int x = 0; x < image.getWidth(); x += step)
                if (data.getPixelColour (x, y).getAlpha() != 0)
                    ++painted;

        return painted;
    }

    /** A scratch folder under build/, so tests never write outside the project. */
    inline juce::File scratchFolder (const juce::String& name)
    {
        auto folder = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                          .getParentDirectory()
                          .getChildFile ("preset-test-" + name);

        folder.deleteRecursively();
        folder.createDirectory();
        return folder;
    }

    inline void addSignal (juce::AudioBuffer<float>& buffer, int64_t firstSample)
    {
        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            const double t = (double) (firstSample + n) / sampleRate;
            const float value = 0.18f * (float) (std::sin (2.0 * juce::MathConstants<double>::pi * 137.0 * t)
                                                 + std::sin (2.0 * juce::MathConstants<double>::pi * 311.0 * t)
                                                 + std::sin (2.0 * juce::MathConstants<double>::pi * 977.0 * t)
                                                 + std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * t));

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.setSample (channel, n, value);
        }
    }

    //==============================================================================
    // Tests that every plugin runs.

    template <typename Processor>
    void runBankShape (const juce::String& pluginId, std::size_t minPresets = 25)
    {
        const auto presets = presetsFor (pluginId);

        assert (presets.size() >= minPresets);

        std::set<juce::String> names;
        std::set<juce::String> categories;

        for (const auto& preset : presets)
        {
            assert (preset.plugin == pluginId);
            assert (! preset.name.isEmpty());
            assert (! preset.category.isEmpty());
            assert (! preset.description.isEmpty());
            assert (preset.state.isValid());
            assert (preset.state.hasType ("PARAMETERS"));
            assert (names.insert (preset.name).second);
            categories.insert (preset.category);
        }

        assert (categories.size() >= 3);

        pass ("bankShape");
    }

    template <typename Processor>
    void runBankRanges (Processor& processor)
    {
        const auto presets = presetsFor (processor.getPresetPluginId());
        assert (! presets.empty());

        for (const auto& preset : presets)
            checkPresetWithinRanges (processor.apvts, preset);

        pass ("bankRanges");
    }

    template <typename Processor>
    void runBankNotDefault (Processor& processor)
    {
        const auto presets = presetsFor (processor.getPresetPluginId());
        assert (! presets.empty());

        for (const auto& preset : presets)
        {
            processor.applyPresetState (preset.state);
            assert (! isDefaultState (processor));
        }

        pass ("bankNotDefault");
    }

    template <typename Processor>
    void runApplyMatchesFile (Processor& processor)
    {
        const auto presets = presetsFor (processor.getPresetPluginId());
        assert (! presets.empty());

        for (const auto& preset : presets)
        {
            processor.applyPresetState (preset.state);
            checkPresetApplied (processor.apvts, preset);
        }

        pass ("applyMatchesFile");
    }

    template <typename Processor>
    void runApplyResetsMissing (Processor& processor)
    {
        const auto presets = presetsFor (processor.getPresetPluginId());
        assert (! presets.empty());

        const auto& preset = presets.front();
        assert (preset.state.getNumChildren() > 0);

        const auto id = preset.state.getChild (0).getProperty ("id").toString();
        processor.applyPresetState (preset.state);
        assert (std::abs (processor.apvts.getRawParameterValue (id)->load()
                          - treeValue (preset.state, id)) < 1.0e-3f);

        // An empty <PARAMETERS> tree means "every parameter at its default".
        processor.applyPresetState (juce::ValueTree ("PARAMETERS"));

        assert (std::abs (processor.apvts.getRawParameterValue (id)->load()
                          - defaultOf (processor.apvts, id)) < 1.0e-3f);

        pass ("applyResetsMissing");
    }

    template <typename Processor>
    void runApplyTwiceIdempotent (Processor& processor)
    {
        const auto presets = presetsFor (processor.getPresetPluginId());
        assert (! presets.empty());

        processor.applyPresetState (presets.front().state);
        const auto once = processor.apvts.copyState();

        processor.applyPresetState (presets.front().state);
        const auto twice = processor.apvts.copyState();

        assert (once.isEquivalentTo (twice));

        pass ("applyTwiceIdempotent");
    }

    template <typename Processor>
    void runAudioSanity (Processor& processor)
    {
        const auto presets = presetsFor (processor.getPresetPluginId());
        assert (! presets.empty());

        juce::AudioBuffer<float> buffer (2, blockSize);
        int64_t sample = 0;

        for (const auto& preset : presets)
        {
            processor.applyPresetState (preset.state);
            processor.prepareToPlay (sampleRate, blockSize);
            processor.reset();

            const int blocks = (int) (2.0 * sampleRate / blockSize);

            for (int block = 0; block < blocks; ++block)
            {
                buffer.clear();
                addSignal (buffer, sample);

                juce::MidiBuffer midi;
                processor.processBlock (buffer, midi);

                assert (allFinite (buffer));
                assert (peak (buffer) <= 4.0f);

                sample += blockSize;
            }
        }

        processor.releaseResources();
        pass ("audioSanity");
    }

    template <typename Processor>
    void runEditorWithBar (Processor& processor)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        assert (editor != nullptr);
        assert (editor->getWidth() > 0 && editor->getHeight() > 0);

        juce::MessageManager::getInstance()->runDispatchLoopUntil (150);

        juce::Image image (juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
        {
            juce::Graphics g (image);
            editor->paintEntireComponent (g, true);
        }

        assert (countPaintedPixels (image) > 0);

        processor.editorBeingDeleted (editor.get());
        editor.reset();

        pass ("editorWithBar");
    }

    /** True if a menu or any of its submenus holds an item with this id. */
    inline bool menuContainsItemId (const juce::PopupMenu& menu, int itemId)
    {
        for (juce::PopupMenu::MenuItemIterator it (menu, true); it.next();)
            if (it.getItem().itemID == itemId)
                return true;

        return false;
    }

    /** Finds the preset bar inside an editor: the bar is a child component of the editor. */
    inline SuitePresets::PresetBar* findBar (juce::AudioProcessorEditor* editor)
    {
        for (auto* child : editor->getChildren())
            if (auto* bar = dynamic_cast<SuitePresets::PresetBar*> (child))
                return bar;

        return nullptr;
    }

    /** Plan 09 1.4: the preset the bar applied survives the editor being destroyed and
        recreated (the reported "shows INIT after reopen" bug). The dirty mark survives
        too: a restored bar's clean baseline is the state the preset produces, not
        whatever the session happens to hold. An applied empty tree (the bar's INIT)
        resets the identity, and a user-file identity that no longer resolves falls back
        to Init without touching the audio state. */
    template <typename Processor>
    void runSelectionSurvivesReopen (Processor& processor)
    {
        const auto presets = presetsFor (processor.getPresetPluginId());
        assert (! presets.empty());

        // Select the first factory preset through the bar, then throw the editor away.
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
            assert (editor != nullptr);
            auto* bar = findBar (editor.get());
            assert (bar != nullptr);
            assert (bar->getNumFactoryPresets() == (int) presets.size());

            bar->selectPreset (0);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            assert (bar->getCurrentPresetName() == presets.front().name);
            assert (! bar->isPresetDirty());

            // A tweak after the preset: the name keeps the preset, the dirty mark shows.
            if (auto* last = processor.getParameters().getLast())
                last->setValueNotifyingHost (last->getValue() < 0.5f ? 0.87f : 0.13f);

            processor.editorBeingDeleted (editor.get());
            editor.reset();
        }

        // The reopened bar remembers the preset and the dirty mark (plan 09 1.4).
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
            assert (editor != nullptr);
            auto* bar = findBar (editor.get());
            assert (bar != nullptr);
            assert (bar->getCurrentPresetName() == presets.front().name);

            juce::MessageManager::getInstance()->runDispatchLoopUntil (150);   // the 10 Hz dirty tick
            assert (bar->isPresetDirty());

            processor.editorBeingDeleted (editor.get());
            editor.reset();
        }

        // Init: an applied empty tree resets the identity (the bar's INIT path).
        processor.applyPresetState (juce::ValueTree ("PARAMETERS"));
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
            auto* bar = findBar (editor.get());
            assert (bar != nullptr);
            assert (bar->getCurrentPresetName() == "Init");
            processor.editorBeingDeleted (editor.get());
            editor.reset();
        }

        // A user-file identity that no longer resolves falls back to Init.
        SuitePresets::PresetSelection missing;
        missing.kind = SuitePresets::PresetSelection::Kind::userFile;
        missing.userFilePath = "Z:/does/not/exist.ausp";
        processor.setPresetSelection (missing);

        {
            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
            auto* bar = findBar (editor.get());
            assert (bar != nullptr);
            assert (bar->getCurrentPresetName() == "Init");
            processor.editorBeingDeleted (editor.get());
            editor.reset();
        }

        pass ("selectionSurvivesReopen");
    }

#if SUITE_PRESET_BANK_AS_PROGRAMS
    template <typename Processor>
    void runProgramList (Processor& processor)
    {
        const auto pluginId = processor.getPresetPluginId();
        const auto presets = presetsFor (pluginId);
        assert (! presets.empty());

        // Index 0 is Init, then the plugin's factory presets in bank order.
        assert (processor.getNumPrograms() == (int) presets.size() + 1);
        assert (processor.getProgramName (0) == "Init");
        assert (processor.getProgramName (1) == presets.front().name);
        assert (processor.getProgramName ((int) presets.size()) == presets.back().name);
        assert (processor.getProgramName (-1).isEmpty());
        assert (processor.getProgramName ((int) presets.size() + 1).isEmpty());

        processor.setCurrentProgram (1);
        assert (processor.getCurrentProgram() == 1);
        checkPresetApplied (processor.apvts, presets.front());

        processor.setCurrentProgram (0);
        assert (processor.getCurrentProgram() == 0);
        assert (isDefaultState (processor));

        processor.setCurrentProgram (-1);
        processor.setCurrentProgram ((int) presets.size() + 5);
        assert (processor.getCurrentProgram() == 0);

        // A preset applied from the bar or a file is no longer a factory program.
        processor.applyPresetState (presets.front().state);
        assert (processor.getCurrentProgram() == -1);

        // The program state information maps to the session state bytes.
        processor.setCurrentProgram (2);
        juce::MemoryBlock programState;
        processor.getCurrentProgramStateInformation (programState);
        assert (programState.getSize() > 0);

        processor.setCurrentProgram (0);
        processor.setCurrentProgramStateInformation (programState.getData(), (int) programState.getSize());
        checkPresetApplied (processor.apvts, presets[1]);

        // Plan 09 1.4: the host program and the bar's selection describe the same event,
        // and the identity rides in the program state bytes (D3).
        {
            const auto selection = processor.getPresetSelection();
            assert (selection.kind == SuitePresets::PresetSelection::Kind::factory);
            assert (selection.factoryIndex == 1);
        }

        processor.setCurrentProgram (1);
        {
            const auto selection = processor.getPresetSelection();
            assert (selection.kind == SuitePresets::PresetSelection::Kind::factory);
            assert (selection.factoryIndex == 0);
        }

        processor.setCurrentProgram (0);
        assert (processor.getPresetSelection().kind == SuitePresets::PresetSelection::Kind::none);

        processor.setCurrentProgram (2);
        juce::MemoryBlock selectionState;
        processor.getCurrentProgramStateInformation (selectionState);

        {
            Processor other;
            other.setStateInformation (selectionState.getData(), (int) selectionState.getSize());

            const auto selection = other.getPresetSelection();
            assert (selection.kind == SuitePresets::PresetSelection::Kind::factory);
            assert (selection.factoryIndex == 1);
        }

        pass ("programList");
    }
#endif

    inline void runMenuModel (const juce::String& pluginId)
    {
        const auto factory = presetsFor (pluginId);
        assert (! factory.empty());

        SuitePresets::PresetInfo user;
        user.name = "My User Preset";
        user.category = "User";
        user.description = "Written by the test";
        user.plugin = pluginId;
        user.isFactory = false;

        const std::vector<SuitePresets::PresetInfo> userPresets { user };
        const auto model = SuitePresets::PresetMenuModel::build (factory, userPresets, 0);

        assert (model.items.size() == factory.size() + 1);
        assert (model.items.front().isCurrent);
        assert (model.items.front().isFactory);
        assert (model.items.back().name == "My User Preset");
        assert (! model.items.back().isFactory);
        assert (model.items.back().flatIndex == (int) factory.size());

        const auto menu = model.createMenu();
        assert (menuContainsItemId (menu, 1));                                  // first factory preset
        assert (menuContainsItemId (menu, (int) factory.size() + 1));           // the user preset

        pass ("menuModel");
    }

    //==============================================================================
    // Small signal helpers, kept here so the per-plugin files stay short.

    inline bool allFinite (const juce::AudioBuffer<float>& buffer)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int n = 0; n < buffer.getNumSamples(); ++n)
                if (! std::isfinite (buffer.getSample (channel, n)))
                    return false;

        return true;
    }

    inline float peak (const juce::AudioBuffer<float>& buffer)
    {
        float result = 0.0f;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            result = juce::jmax (result, buffer.getMagnitude (channel, 0, buffer.getNumSamples()));

        return result;
    }
}
