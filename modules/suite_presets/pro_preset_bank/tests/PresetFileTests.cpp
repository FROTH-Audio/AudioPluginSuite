// Pro preset tests for the file format and the bank parser. No plugin instance is needed here;
// the per-plugin executables check the bank against the live APVTS.

#include "PresetTestUtils.h"

#include <cstring>

namespace
{
    using namespace PresetTest;
    using SuitePresets::PresetInfo;
    namespace PresetFile = SuitePresets::PresetFile;

    juce::ValueTree makeState (const juce::String& plugin = "SpectralDrift")
    {
        juce::ValueTree state ("PARAMETERS");

        auto add = [&state] (const char* id, double value)
        {
            juce::ValueTree param ("PARAM");
            param.setProperty ("id", id, nullptr);
            param.setProperty ("value", value, nullptr);
            state.appendChild (param, nullptr);
        };

        add ("depth", 42.0);
        add ("shift", -7.0);

        if (plugin == "MicroTimingChaos")
            state.setProperty ("lerpCurve", "0.0000,0.0000;0.2500,0.2000;0.5000,0.5000;0.7500,0.8000;1.0000,1.0000",
                               nullptr);

        return state;
    }

    PresetInfo makePreset (const juce::String& name, const juce::String& plugin = "SpectralDrift")
    {
        PresetInfo preset;
        preset.name = name;
        preset.category = "Test";
        preset.description = "Written by the file test";
        preset.plugin = plugin;
        preset.state = makeState (plugin);
        preset.isFactory = false;
        return preset;
    }

    void testSanitiseFileName()
    {
        const auto cleaned = PresetFile::sanitiseFileName ("a/b:c*?");
        assert (! cleaned.isEmpty());
        assert (cleaned.indexOfAnyOf ("\\/:*?\"<>|") < 0);
        assert (PresetFile::sanitiseFileName ("  .  ") == "Preset");
        assert (PresetFile::sanitiseFileName ("Normal Name") == "Normal Name");

        pass ("fileSanitising");
    }

    void testFileRoundTrip()
    {
        const auto folder = scratchFolder ("round-trip");
        const auto file = folder.getChildFile ("round_trip.ausp");

        auto preset = makePreset ("Round Trip");
        preset.state.setProperty (SuiteState::versionProperty, 1, nullptr);

        const auto written = PresetFile::write (file, preset, preset.state, 1);
        assert (written.ok);
        assert (file.existsAsFile());

        const auto read = PresetFile::read (file, "SpectralDrift", 1);
        assert (read.ok);
        assert (read.preset.name == "Round Trip");
        assert (read.preset.category == "Test");
        assert (read.preset.description == "Written by the file test");
        assert (read.preset.plugin == "SpectralDrift");
        assert (! read.preset.isFactory);
        assert (read.preset.state.hasType ("PARAMETERS"));
        assert (std::abs (treeValue (read.preset.state, "depth") - 42.0f) < 1.0e-4f);
        assert (std::abs (treeValue (read.preset.state, "shift") - -7.0f) < 1.0e-4f);
        assert ((int) read.preset.state.getProperty (SuiteState::versionProperty, 0) == 1);

        folder.deleteRecursively();
        pass ("fileRoundTrip");
    }

    void testFileRefusals()
    {
        const auto folder = scratchFolder ("refusals");

        // Wrong plugin id.
        {
            const auto file = folder.getChildFile ("wrong_plugin.ausp");
            const auto written = PresetFile::write (file, makePreset ("Wrong"), makeState(), 1);
            assert (written.ok);

            const auto read = PresetFile::read (file, "DynamicSaturation", 1);
            assert (! read.ok);
            assert (read.error.containsIgnoreCase ("belongs"));
        }

        // Newer state version than the plugin.
        {
            const auto file = folder.getChildFile ("newer.ausp");
            const auto written = PresetFile::write (file, makePreset ("Newer"), makeState(), 99);
            assert (written.ok);

            const auto read = PresetFile::read (file, "SpectralDrift", 1);
            assert (! read.ok);
            assert (read.error.containsIgnoreCase ("newer"));
        }

        // Not XML at all.
        {
            const auto file = folder.getChildFile ("garbage.ausp");
            file.replaceWithText ("this is not xml");

            const auto read = PresetFile::read (file, "SpectralDrift", 1);
            assert (! read.ok);
            assert (read.error.isNotEmpty());
        }

        // Not an AudioSuite preset.
        {
            const auto file = folder.getChildFile ("other.ausp");
            file.replaceWithText ("<?xml version=\"1.0\"?><Other/>");

            const auto read = PresetFile::read (file, "SpectralDrift", 1);
            assert (! read.ok);
            assert (read.error.containsIgnoreCase ("AudioSuite"));
        }

        folder.deleteRecursively();
        pass ("fileRefusals");
    }

    void testParseDelta()
    {
        const auto defaults = makeState();

        const auto merged = SuitePresets::PresetBank::parseDelta (
            "{\"_name\":\"Ignored\",\"depth\":80,\"unknown\":1,\"shift\":-2}", defaults);

        assert (merged.hasType ("PARAMETERS"));
        assert (std::abs (treeValue (merged, "depth") - 80.0f) < 1.0e-4f);
        assert (std::abs (treeValue (merged, "shift") - -2.0f) < 1.0e-4f);
        assert (! merged.hasProperty ("_name"));

        // An unknown id is ignored rather than added.
        for (int i = 0; i < merged.getNumChildren(); ++i)
            assert (merged.getChild (i).getProperty ("id").toString() != "unknown");

        pass ("parseDelta");
    }

    void testPresetFolderPath()
    {
        const auto folder = PresetFile::getUserPresetFolder ("SpectralDrift");
        assert (folder.getFileName() == "Presets");
        assert (folder.getParentDirectory().getFileName() == "SpectralDrift");
        assert (folder.getParentDirectory().getParentDirectory().getFileName() == "AudioSuite");

        pass ("presetFolderPath");
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::puts ("Running Pro preset file tests...");

    testSanitiseFileName();
    testFileRoundTrip();
    testFileRefusals();
    testParseDelta();
    testPresetFolderPath();

    std::puts ("All Pro preset file tests passed.");
    return 0;
}
