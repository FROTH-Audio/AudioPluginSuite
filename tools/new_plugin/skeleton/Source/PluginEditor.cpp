#include "PluginEditor.h"

namespace
{
    constexpr int editorWidth = 420;
    constexpr int editorHeight = 260;
    constexpr int headerHeight = 48;
}

__NAME__Editor::__NAME__Editor (__NAME__Processor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      gainAttachment (p.apvts, __NAME__Processor::gainId, gainSlider.slider)
{
    setLookAndFeel (&lookAndFeel);
    setSize (editorWidth, editorHeight);

    addAndMakeVisible (gainSlider);
}

__NAME__Editor::~__NAME__Editor()
{
    setLookAndFeel (nullptr);
}

void __NAME__Editor::paint (juce::Graphics& g)
{
    g.fillAll (SuiteUI::Colours::background);

    g.setColour (SuiteUI::Colours::text);
    g.setFont (juce::Font (juce::FontOptions (18.0f)));
    g.drawText ("__PRODUCT__", getLocalBounds().removeFromTop (headerHeight), juce::Justification::centred);
}

void __NAME__Editor::resized()
{
    gainSlider.setBounds (getLocalBounds().withTrimmedTop (headerHeight).reduced (24));
}
