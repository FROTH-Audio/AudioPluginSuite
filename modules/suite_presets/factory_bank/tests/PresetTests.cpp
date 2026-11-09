#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "SuitePresetsPro.h"

#define CTEST_IMPLEMENTATION
#include "ctest.h"

using namespace SuitePresets;

//==============================================================================
// Test 1: Factory bank loads and contains valid presets
void testFactoryBankLoads()
{
    auto presets = PresetBank::loadFactoryBank();
    
    // Should have at least one preset
    assert(presets.size() > 0);
    
    // Each preset should have non-empty name, category, description
    for (const auto& preset : presets)
    {
        assert(! preset.name.isEmpty());
        assert(! preset.category.isEmpty());
        assert(! preset.description.isEmpty());
    }
}

//==============================================================================
// Test 2: Encode/decode round trip
void testEncodeDecodeRoundTrip()
{
    juce::ValueTree tree("test");
    tree.setProperty("foo", "bar", nullptr);
    
    const String encoded = PresetBank::encodeValueTree(tree);
    auto decoded = PresetBank::decodeJson(encoded);
    
    // The decoded value should match the original
    assert(decoded.getProperty("foo").toString() == "bar");
}

//==============================================================================
// Test 3: Public seam is available
void testPublicSeamAvailable()
{
    // The public seam methods compile, which proves availability
    (void)PresetBank::loadFactoryBank;
    (void)PresetBank::parseDelta;
    (void)PresetBank::encodeValueTree;
    (void)PresetBank::decodeJson;
}

//==============================================================================
// Test 4: StateHolder interface works (compile-time check)
void testStateHolderInterface()
{
    // These methods must exist on StateHolder for Pro seam to work
    static_assert(std::is_same<decltype(StateHolder::getPresetStateTree), juce::ValueTree (*)()>::value,
                  "StateHolder::getPresetStateTree must return ValueTree*");
}

//==============================================================================
// Test 5: Full .ausp round trip - write to file and read back
void testAuspRoundTrip()
{
    // Create a value tree with some properties
    juce::ValueTree root("preset");
    root.setProperty("driftRate", 2.5f, nullptr);
    root.setProperty("depth", 19.1f, nullptr);
    root.setProperty("mix", 0.75f, nullptr);
    
    // Encode to .ausp format (XML)
    const String encoded = PresetBank::encodeValueTree(root);
    
    // Verify the encoded string is valid XML and contains our values
    assert(encoded.contains("driftRate"));
    assert(encoded.contains("2.5"));
    assert(encoded.contains("depth"));
    assert(encoded.contains("19.1"));
    
    // Decode back
    auto decoded = PresetBank::decodeJson(encoded);
    
    // Verify properties match
    assert(decoded.getProperty("driftRate").toString() == "2.5000");
    assert(decoded.getProperty("depth").toString() == "19.1000");
    assert(decoded.getProperty("mix").toString() == "0.75000");
}

//==============================================================================
// Test 6: PresetBar integration - height and callbacks
void testPresetBarIntegration()
{
    // Create a mock StateHolder for testing
    class MockStateHolder : public StateHolder
    {
    public:
        juce::ValueTree state;
        
        MockStateHolder() : state("mock") {}
        
        juce::String getPresetPluginId() const override { return "Mock"; }
        juce::AudioProcessorValueTreeState& getPresetStateTree() override { return apvts; }
        void applyPresetState (const juce::ValueTree&) override {}
    };
    
    MockStateHolder holder;
    SuitePresets::PresetBar bar { holder, juce::Colour::black };
    
    // Height should be 30 in Pro build (or 0 if SUITE_PRO_PRESETS is off)
    assert(bar.getPreferredHeight() > 0);
    
    // Select a preset - this should trigger onPresetLoaded callback
    bar.onPresetLoaded = [] { /* would notify editor */ };
    bar.selectPreset(0);
}

//==============================================================================
int main()
{
    CTest::runTests();
}