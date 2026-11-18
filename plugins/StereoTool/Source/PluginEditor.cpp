#include "PluginEditor.h"

namespace
{
    // Plan 05 R4: the default window stays inside the suite budget (1000x700). The knob
    // size comes from the SuiteUI tokens (R1); never set slider bounds that exceed them.
    constexpr int editorWidth = 720;
    constexpr int editorHeight = 560;
    constexpr int headerHeight = 52;
    constexpr int footerHeight = 30;
}

StereoToolEditor::StereoToolEditor (StereoToolProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p)
{
    setLookAndFeel (&processor.lookAndFeel);
    setSize (editorWidth, editorHeight);
    setResizable (true, true);
    setResizeLimits (editorWidth - 60, editorHeight - 60, 1200, 1200);

    addAndMakeVisible (widthCtrl);
    addAndMakeVisible (mixCtrl);
    addAndMakeVisible (driveCtrl);
}

StereoToolEditor::~StereoToolEditor()
{
    setLookAndFeel (nullptr);
}

void StereoToolEditor::paint (juce::Graphics& g)
{
    g.fillAll (SuiteUI::Colours::background);

    // Header: product name
    g.setColour (lookAndFeel.getAccent());
    g.setFont (SuiteUI::uiFont (15.0f, true));
    g.drawText ("Stereo Tool Effect", getLocalBounds().removeFromTop (headerHeight).reduced (16, 0),
                juce::Justification::centredLeft);

    // Footer help text
    g.setColour (SuiteUI::Colours::textDim);
    g.setFont (SuiteUI::uiFont (11.0f));
    g.drawText ("Drag knobs to modulate. Width: -100...+100 % (0=correlated).",
                getLocalBounds().removeFromBottom (footerHeight).reduced (16, 0),
                juce::Justification::centredLeft);
}

void StereoToolEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (headerHeight);
    area.removeFromBottom (footerHeight);

    // Layout: three knobs vertically with some spacing
    auto knobHeight = (area.getHeight() - 20) / 3;
    widthCtrl.setBounds (area.getX(), area.getY(), area.getWidth(), knobHeight);
    area.removeFromTop (knobHeight + 5);
    mixCtrl.setBounds (area.getX(), area.getY(), area.getWidth(), knobHeight);
    area.removeFromTop (knobHeight + 5);
    driveCtrl.setBounds (area.getX(), area.getY(), area.getWidth(), knobHeight);
}