#include "SuitePresets.h"

namespace SuitePresets
{
    // The public build has no bank and no bar. Both entry points exist so callers compile and link
    // without an #if, and both do nothing.
    struct PresetBar::Impl {};

    PresetBar::PresetBar (StateHolder&, juce::Colour) : impl (std::make_unique<Impl>()) {}
    PresetBar::~PresetBar() = default;

    int PresetBar::getPreferredHeight() const noexcept { return 0; }
    void PresetBar::paint (juce::Graphics&) {}
    void PresetBar::resized() {}
    void PresetBar::selectPreset (int) {}
    int PresetBar::getNumFactoryPresets() const noexcept { return 0; }
}
