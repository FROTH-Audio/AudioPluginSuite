#include "SuitePresets.h"

namespace SuitePresets
{
    struct PresetBar::Impl {};

    PresetBar::PresetBar (StateHolder&, juce::Colour) {}
    PresetBar::~PresetBar() = default;

    // Public build: all PresetBar operations are no-ops, height 0
    int PresetBar::getPreferredHeight() const noexcept { return 0; }

    void PresetBar::paint(juce::Graphics&) {}
    void PresetBar::resized() {}
    void PresetBar::selectPreset(int) {}
    int PresetBar::getNumFactoryPresets() const noexcept { return 0; }

}
