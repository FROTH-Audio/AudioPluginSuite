#include "SuitePresets.h"

namespace SuitePresets
{
    PresetBar::PresetBar (StateHolder& /*holder*/, juce::Colour /*accent*/) {}
    PresetBar::~PresetBar() = default;

    int PresetBar::getPreferredHeight() const noexcept { return 0; }
    void PresetBar::paint (juce::Graphics&) {}
    void PresetBar::resized() {}
}