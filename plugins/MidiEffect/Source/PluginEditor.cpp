#include "PluginEditor.h"

namespace
{
    // Plan 05 R4: the default window stays inside the suite budget (1000x700). The knob
    // size comes from the SuiteUI tokens (R1); never set slider bounds that exceed them.
    constexpr int editorWidth = 720;
    constexpr int editorHeight = 420;
    constexpr int headerHeight = 44;
    constexpr int footerHeight = 26;
    constexpr int groupGap = 8;   // the suite's one-gap rule (plan 05 R7)
}

MidiEffectEditor::MidiEffectEditor (MidiEffectProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p)
{
    setLookAndFeel (&lookAndFeel);
    setSize (editorWidth, editorHeight);
    setResizable (true, true);
    setResizeLimits (editorWidth - 60, editorHeight - 60, 1200, 1200);

    // The combo lists mirror the parameter choice lists; the attachment keeps them in sync.
    scaleCombo.addItemList ({ "Chromatic", "Major", "Minor", "Minor Pentatonic", "Dorian", "Whole Tone" }, 1);
    chordCombo.addItemList ({ "Major", "Minor", "Dim", "Aug", "Sus2", "Sus4", "Minor 7", "Major 7" }, 1);
    arpCombo.addItemList ({ "Up", "Down", "Alternate", "Random" }, 1);
    for (auto* combo : { &scaleCombo, &chordCombo, &arpCombo })
        combo->setJustificationType (juce::Justification::centred);

    addAndMakeVisible (rootCtrl);
    addAndMakeVisible (octaveCtrl);
    addAndMakeVisible (velocityCtrl);
    addAndMakeVisible (scaleCombo);
    addAndMakeVisible (chordCombo);
    addAndMakeVisible (arpCombo);
}

MidiEffectEditor::~MidiEffectEditor()
{
    setLookAndFeel (nullptr);
}

void MidiEffectEditor::paint (juce::Graphics& g)
{
    g.fillAll (SuiteUI::Colours::background);

    // Header: the brand lockup goes here once the plugin has one (LOCKUP in CMake, see
    // the other plugins and SuiteUI::drawBrandLockup). The product name uses the suite font.
    g.setColour (lookAndFeel.getAccent());
    g.setFont (SuiteUI::uiFont (15.0f, true));
    g.drawText ("MIDI Chord Arp Scale Tool", getLocalBounds().removeFromTop (headerHeight).reduced (16, 0),
                juce::Justification::centredLeft);

    g.setColour (SuiteUI::Colours::textDim);
    g.setFont (SuiteUI::uiFont (11.0f));
    g.drawText ("Incoming notes are quantised to the scale, then chorded. Note-off releases the chord.",
                getLocalBounds().removeFromBottom (footerHeight).reduced (16, 0),
                juce::Justification::centredLeft);
}

void MidiEffectEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (headerHeight);
    area.removeFromBottom (footerHeight);

    // Knob row at the suite size (SuiteUI::knobRowHeight); the widget caps the knob.
    auto knobs = area.removeFromTop (SuiteUI::knobRowHeight);
    auto knobWidth = knobs.getWidth() / 3;
    rootCtrl.setBounds (knobs.removeFromLeft (knobWidth));
    octaveCtrl.setBounds (knobs.removeFromLeft (knobWidth));
    velocityCtrl.setBounds (knobs.removeFromLeft (knobWidth));

    area.removeFromTop (groupGap);

    // One combo row under the knobs.
    auto combos = area.removeFromTop (SuiteUI::pillHeight);
    auto comboWidth = combos.getWidth() / 3;
    scaleCombo.setBounds (combos.removeFromLeft (comboWidth).reduced (4, 0));
    chordCombo.setBounds (combos.removeFromLeft (comboWidth).reduced (4, 0));
    arpCombo.setBounds (combos.removeFromLeft (comboWidth).reduced (4, 0));
}
