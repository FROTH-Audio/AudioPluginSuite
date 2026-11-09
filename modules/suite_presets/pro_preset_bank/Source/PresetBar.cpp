#include "SuitePresetsPro.h"

namespace SuitePresets {

struct Impl { };

PresetBar::PresetBar(StateHolder& holder, juce::Colour accent)
    : Component("presetBar")
    , _holder(&holder)
    , _accent(accent)
    , impl(std::make_unique<Impl>())
    {
        onPresetLoaded = nullptr;  // Set by editor
        
        setWantsKeyboardFocus(false);
    }

    PresetBar::~PresetBar() = default;

    int PresetBar::getPreferredHeight() const noexcept
    {
        return 30;  // Pro builds have a visible bar
    }

    void PresetBar::paint(juce::Graphics& g)
    {
        using namespace juce;
        
        if (! isVisible())
            return;
        
        auto colour = _accent.withAlpha(0.8f);
        
        // Draw background
        g.setColour(colour);
        g.fillRect(getLocalBounds().withTrimmedTop(1));
        
        // Draw border
        g.setColour(Colours::grey);
        g.drawRect(getLocalBounds(), 1);
        
        // Draw preset name (would show current preset in Pro)
        g.setColour(Colours::white);
        const String name = "Preset";
        auto bounds = getLocalBounds().withTrimmedLeft(8).withTrimmedRight(8).withTrimmedTop(4).withTrimmedBottom(4);
        juce::Font font(10, Font::plain);
        // Get string width using GlyphArrangement's simpler API
        const int width = (int)juce::GlyphArrangement::getStringWidth(font, name);
        const int x = bounds.getCentreX() - width / 2;
        g.drawText(name, Rectangle<int>(x, bounds.getY(), width, bounds.getHeight()), Justification::centredTop);
    }

    void PresetBar::resized()
    {
        // Layout the controls (would show buttons in Pro)
    }

    void PresetBar::selectPreset(int factoryIndex)
    {
        // Apply preset from factory bank
        if (_holder != nullptr)
            _holder->applyPresetState(_holder->getPresetStateTree().copyState());
    }

}