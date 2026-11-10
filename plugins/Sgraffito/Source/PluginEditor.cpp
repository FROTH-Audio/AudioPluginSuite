#include "PluginEditor.h"

namespace
{
    constexpr int editorWidth = 420;
    constexpr int editorHeight = 260;
    constexpr int headerHeight = 48;
}

SgraffitoEditor::SgraffitoEditor (SgraffitoProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p)
{
    setLookAndFeel (&lookAndFeel);
    setSize (editorWidth, editorHeight);
}

SgraffitoEditor::~SgraffitoEditor()
{
    setLookAndFeel (nullptr);
}

void SgraffitoEditor::paint (juce::Graphics& g)
{
    g.fillAll (SuiteUI::Colours::background);

    g.setColour (SuiteUI::Colours::text);
    g.setFont (juce::Font (juce::FontOptions (18.0f)));
    g.drawText ("Sgraffito", getLocalBounds().removeFromTop (headerHeight), juce::Justification::centred);

    g.setFont (juce::Font (juce::FontOptions (14.0f)));
    g.drawText ("Sample: not loaded",
                getLocalBounds().withTrimmedTop (headerHeight).reduced (24),
                juce::Justification::centred);
}

void SgraffitoEditor::resized()
{
}
