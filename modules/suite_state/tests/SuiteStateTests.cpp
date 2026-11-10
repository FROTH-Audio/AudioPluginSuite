// Unit tests for SuiteState. NDEBUG is undefined before <cassert> on purpose, as in the other test files.

#undef NDEBUG
#include <cassert>

#include <cstdio>
#include <string>

#include "SuiteState.h"

namespace
{
    // A minimal processor, so an APVTS can be built. It has no audio behaviour.
    struct StubProcessor : public juce::AudioProcessor
    {
        const juce::String getName() const override { return "Stub"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };

    juce::AudioProcessorValueTreeState::ParameterLayout makeLayout()
    {
        return { std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "gain", 1 }, "Gain",
                                                              juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f),
                 std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "enabled", 1 }, "Enabled", false) };
    }

    // One processor and its APVTS, kept together so the APVTS never outlives its processor.
    struct Rig
    {
        Rig() : apvts (processor, nullptr, "PARAMETERS", makeLayout()) {}

        float gain() const { return apvts.getRawParameterValue ("gain")->load(); }
        bool enabled() const { return apvts.getRawParameterValue ("enabled")->load() > 0.5f; }
        void setGain (float normalised) { apvts.getParameter ("gain")->setValueNotifyingHost (normalised); }
        void setEnabled (bool on) { apvts.getParameter ("enabled")->setValueNotifyingHost (on ? 1.0f : 0.0f); }

        StubProcessor processor;
        juce::AudioProcessorValueTreeState apvts;
    };

    // A blob written the way the plugins wrote state before this module: no version attribute.
    juce::MemoryBlock unversionedBlob (juce::AudioProcessorValueTreeState& apvts)
    {
        const auto xml = apvts.copyState().createXml();
        return SuiteState::detail::encodeXml (*xml);
    }

    void testRoundTrip()
    {
        Rig source;
        source.setGain (0.8f);
        source.setEnabled (true);

        const auto blob = SuiteState::write (source.apvts, 1);

        Rig target;
        const auto result = SuiteState::read (blob.getData(), (int) blob.getSize(), target.apvts, 1, {});
        assert (result == SuiteState::LoadResult::loaded);
        assert (std::abs (target.gain() - 0.8f) < 1e-6f);
        assert (target.enabled());

        std::puts ("[PASS] testRoundTrip");
    }

    void testExtraHookIsWritten()
    {
        Rig source;
        const auto blob = SuiteState::write (source.apvts, 1, [] (juce::ValueTree& state) {
            state.setProperty ("marker", "lerp", nullptr);
        });

        const auto xml = SuiteState::detail::decodeXml (blob.getData(), (int) blob.getSize());
        assert (xml != nullptr);
        const auto tree = juce::ValueTree::fromXml (*xml);
        assert (tree.getProperty ("marker").toString() == "lerp");

        std::puts ("[PASS] testExtraHookIsWritten");
    }

    void testUnversionedIsVersionOne()
    {
        Rig source;
        source.setGain (0.3f);
        const auto blob = unversionedBlob (source.apvts);

        Rig target;
        const auto result = SuiteState::read (blob.getData(), (int) blob.getSize(), target.apvts, 1, {});
        assert (result == SuiteState::LoadResult::loaded);
        assert (std::abs (target.gain() - 0.3f) < 1e-6f);

        std::puts ("[PASS] testUnversionedIsVersionOne");
    }

    void testNewerStateIsRefused()
    {
        Rig source;
        source.setGain (0.8f);
        const auto blob = SuiteState::write (source.apvts, 2);

        Rig target;
        target.setGain (0.2f);
        const auto result = SuiteState::read (blob.getData(), (int) blob.getSize(), target.apvts, 1, {});
        assert (result == SuiteState::LoadResult::newerThanPlugin);
        assert (std::abs (target.gain() - 0.2f) < 1e-6f);

        std::puts ("[PASS] testNewerStateIsRefused");
    }

    void testWrongRootIsRefused()
    {
        juce::XmlElement other ("NOT_PARAMETERS");
        other.setAttribute ("gain", 0.9);
        const auto blob = SuiteState::detail::encodeXml (other);

        Rig target;
        target.setGain (0.2f);
        const auto result = SuiteState::read (blob.getData(), (int) blob.getSize(), target.apvts, 1, {});
        assert (result == SuiteState::LoadResult::wrongRoot);
        assert (std::abs (target.gain() - 0.2f) < 1e-6f);

        std::puts ("[PASS] testWrongRootIsRefused");
    }

    void testGarbageIsRefused()
    {
        const char garbage[] = "this is not a saved state";

        Rig target;
        assert (SuiteState::read (garbage, (int) sizeof (garbage), target.apvts, 1, {}) == SuiteState::LoadResult::notXml);
        assert (SuiteState::read (garbage, 0, target.apvts, 1, {}) == SuiteState::LoadResult::notXml);

        std::puts ("[PASS] testGarbageIsRefused");
    }

    void testMigrationRunsOnce()
    {
        Rig source;
        source.setGain (0.8f);
        const auto blob = unversionedBlob (source.apvts);

        // Version 1 to 2: move the gain to 0.25, and count the runs.
        int runs = 0;
        const std::vector<SuiteState::Migration> migrations {
            { 1, [&runs] (juce::ValueTree& state) {
                ++runs;
                for (int i = 0; i < state.getNumChildren(); ++i)
                    if (state.getChild (i).getProperty ("id").toString() == "gain")
                        state.getChild (i).setProperty ("value", 0.25f, nullptr);
            } }
        };

        Rig target;
        const auto result = SuiteState::read (blob.getData(), (int) blob.getSize(), target.apvts, 2, migrations);
        assert (result == SuiteState::LoadResult::loaded);
        assert (runs == 1);
        assert (std::abs (target.gain() - 0.25f) < 1e-6f);
        assert ((int) target.apvts.state.getProperty (SuiteState::versionProperty) == 2);

        std::puts ("[PASS] testMigrationRunsOnce");
    }

    void testAfterLoadRunsOnlyOnSuccess()
    {
        Rig source;
        const auto good = SuiteState::write (source.apvts, 1);
        const auto newer = SuiteState::write (source.apvts, 2);

        int calls = 0;
        const auto hook = [&calls] (const juce::ValueTree&) { ++calls; };

        Rig target;
        assert (SuiteState::read (good.getData(), (int) good.getSize(), target.apvts, 1, {}, hook) == SuiteState::LoadResult::loaded);
        assert (calls == 1);

        assert (SuiteState::read (newer.getData(), (int) newer.getSize(), target.apvts, 1, {}, hook) == SuiteState::LoadResult::newerThanPlugin);
        assert (calls == 1);

        std::puts ("[PASS] testAfterLoadRunsOnlyOnSuccess");
    }

    void testResetMissing()
    {
        // A blob that only knows the gain, written by a plugin that did not have the enabled switch yet.
        juce::ValueTree tree ("PARAMETERS");
        juce::ValueTree gainNode ("PARAM");
        gainNode.setProperty ("id", "gain", nullptr);
        gainNode.setProperty ("value", 0.4, nullptr);
        tree.appendChild (gainNode, nullptr);
        const auto xml = tree.createXml();
        const auto blob = SuiteState::detail::encodeXml (*xml);

        // Measured with JUCE 9.0.5: replaceState already sets a parameter that the blob does not contain back to
        // its default, so the switch is off with or without resetMissing. The option keeps that guarantee explicit
        // and does not depend on JUCE's behaviour. The plan's assumption that the old value survives was wrong.
        Rig kept;
        kept.setEnabled (true);
        assert (SuiteState::read (blob.getData(), (int) blob.getSize(), kept.apvts, 1, {}) == SuiteState::LoadResult::loaded);
        assert (std::abs (kept.gain() - 0.4f) < 1e-6f);
        assert (! kept.enabled());

        Rig reset;
        reset.setEnabled (true);
        assert (SuiteState::read (blob.getData(), (int) blob.getSize(), reset.apvts, 1, {}, {}, true) == SuiteState::LoadResult::loaded);
        assert (std::abs (reset.gain() - 0.4f) < 1e-6f);
        assert (! reset.enabled());

        std::puts ("[PASS] testResetMissing");
    }
}

int main()
{
    std::puts ("Running SuiteState unit tests...");

    testRoundTrip();
    testExtraHookIsWritten();
    testUnversionedIsVersionOne();
    testNewerStateIsRefused();
    testWrongRootIsRefused();
    testGarbageIsRefused();
    testMigrationRunsOnce();
    testAfterLoadRunsOnlyOnSuccess();
    testResetMissing();

    std::puts ("All SuiteState unit tests passed.");
    return 0;
}
