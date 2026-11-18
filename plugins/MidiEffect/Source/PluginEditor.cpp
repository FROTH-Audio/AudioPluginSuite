#include "PluginEditor.h"

namespace
{
    // Plan 05 R4: the default window stays inside the suite budget (1000x700). The knob
    // size comes from the SuiteUI tokens (R1); never set slider bounds that exceed them.
    constexpr int editorWidth = 800;
    constexpr int editorHeight = 600;
    constexpr int headerHeight = 52;
    constexpr int footerHeight = 30;
}

MidiEffectEditor::MidiEffectEditor (MidiEffectProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p)
{
    setLookAndFeel (&processor.lookAndFeel);
    setSize (editorWidth, editorHeight);
    setResizable (true, true);
    setResizeLimits (editorWidth - 60, editorHeight - 60, 1200, 1200);

    // Scale combo
    scaleCombo.addItem ("Major", 1);
    scaleCombo.addItem ("Minor", 2);
    scaleCombo.addItem ("Dorian", 3);
    scaleCombo.addItem ("Phrygian", 4);
    scaleCombo.addItem ("Lydian", 5);
    scaleCombo.addItem ("Mixolydian", 6);
    scaleCombo.addItem ("Aeolian", 7);
    scaleCombo.addItem ("Whole tone", 8);
    scaleCombo.addItem ("Chromatic", 9);
    scaleCombo.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (scaleCombo);

    // Chord quality combo
    chordCombo.addItem ("Major", 1);
    chordCombo.addItem ("Minor", 2);
    chordCombo.addItem ("Dim", 3);
    chordCombo.addItem ("Aug", 4);
    chordCombo.addItem ("Sus2", 5);
    chordCombo.addItem ("Sus4", 6);
    chordCombo.addItem ("Minor7", 7);
    chordCombo.addItem ("Major7", 8);
    chordCombo.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (chordCombo);

    // Arpeggio pattern combo
    arpCombo.addItem ("Up", 1);
    arpCombo.addItem ("Down", 2);
    arpCombo.addItem ("Alternate", 3);
    arpCombo.addItem ("Random", 4);
    arpCombo.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (arpCombo);

    // Root note slider (24-84 range)
    rootSlider.setRange (24, 84, 1);
    rootSlider.setValue (60);
    rootSlider.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (rootSlider);

    // Velocity combo
    velCombo.addItem ("Low", 1);
    velCombo.addItem ("Medium", 2);
    velCombo.addItem ("High", 3);
    velCombo.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (velCombo);
}

MidiEffectEditor::~MidiEffectEditor()
{
    setLookAndFeel (nullptr);
}

void MidiEffectEditor::paint (juce::Graphics& g)
{
    g.fillAll (SuiteUI::Colours::background);

    // Header
    g.setColour (lookAndFeel.getAccent());
    g.setFont (SuiteUI::uiFont (15.0f, true));
    g.drawText ("MIDI Chord/Arp/Scale Tool", getLocalBounds().removeFromTop (headerHeight).reduced (16, 0),
                juce::Justification::centredLeft);

    // Footer help
    g.setColour (SuiteUI::Colours::textDim);
    g.setFont (SuiteUI::uiFont (11.0f));
    g.drawText ("Select scale, chord, arp pattern and root note. Incoming MIDI notes will be re-harmonised.",
                getLocalBounds().removeFromBottom (footerHeight).reduced (16, 0),
                juce::Justification::centredLeft);
}

void MidiEffectEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (headerHeight);
    area.removeFromBottom (footerHeight);

    // Layout: controls vertically
    auto ctrlHeight = area.getHeight() / 4;
    scaleCombo.setBounds (area.getX(), area.getY(), area.getWidth(), ctrlHeight);
    area.removeFromTop (ctrlHeight + 2);
    chordCombo.setBounds (area.getX(), area.getY(), area.getWidth(), ctrlHeight);
    area.removeFromTop (ctrlHeight + 2);
    arpCombo.setBounds (area.getX(), area.getY(), area.getWidth(), ctrlHeight);
    area.removeFromTop (ctrlHeight + 2);
    rootSlider.setBounds (area.getX(), area.getY(), area.getWidth(), ctrlHeight);
    area.removeFromTop (ctrlHeight + 2);
    velCombo.setBounds (area.getX(), area.getY(), area.getWidth(), ctrlHeight);
}