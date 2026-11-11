#include "PluginEditor.h"

namespace
{
    // Plan 05 R4: the default window stays inside the suite budget (1000x700). The knob
    // size comes from the SuiteUI tokens (R1); never set slider bounds that exceed them.
    constexpr int editorWidth = 640;
    constexpr int editorHeight = 420;
    constexpr int headerHeight = 44;
    constexpr int footerHeight = 26;
}

__NAME__Editor::__NAME__Editor (__NAME__Processor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      gainAttachment (p.apvts, __NAME__Processor::gainId, gainSlider.slider)
{
    setLookAndFeel (&lookAndFeel);
    setSize (editorWidth, editorHeight);
    setResizable (true, true);
    setResizeLimits (editorWidth - 60, editorHeight - 60, 1200, 1200);

    addAndMakeVisible (gainSlider);
}

__NAME__Editor::~__NAME__Editor()
{
    setLookAndFeel (nullptr);
}

void __NAME__Editor::paint (juce::Graphics& g)
{
    g.fillAll (SuiteUI::Colours::background);

    // Header: the brand lockup goes here once the plugin has one (LOCKUP in CMake, see
    // the other plugins and SuiteUI::drawBrandLockup). The product name uses the suite font.
    g.setColour (lookAndFeel.getAccent());
    g.setFont (SuiteUI::uiFont (15.0f, true));
    g.drawText ("__PRODUCT__", getLocalBounds().removeFromTop (headerHeight).reduced (16, 0),
                juce::Justification::centredLeft);

    g.setColour (SuiteUI::Colours::textDim);
    g.setFont (SuiteUI::uiFont (11.0f));
    g.drawText ("Hover a control to see what it does.",
                getLocalBounds().removeFromBottom (footerHeight).reduced (16, 0),
                juce::Justification::centredLeft);
}

void __NAME__Editor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (headerHeight);
    area.removeFromBottom (footerHeight);

    // One knob row at the suite size (SuiteUI::knobRowHeight); the widget caps the knob.
    gainSlider.setBounds (area.removeFromTop (SuiteUI::knobRowHeight));
}
