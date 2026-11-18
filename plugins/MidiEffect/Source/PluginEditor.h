#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "SuiteLookAndFeel.h"
#include "SuiteWidgets.h"

class MidiEffectEditor : public juce::AudioProcessorEditor
{
public:
    explicit MidiEffectEditor (MidiEffectProcessor&);
    ~MidiEffectEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MidiEffectProcessor& processor;

    // Look and feel with suite accent colour
    SuiteUI::SuiteLookAndFeel lookAndFeel { juce::Colour (0xff9aa0a6) };

    // Scale pop-up
    juce::ComboBox scaleCombo;
    juce::Label scaleLabel { "SCALE" };

    // Chord quality pop-up
    juce::ComboBox chordCombo;
    juce::Label chordLabel { "CHORD" };

    // Arpeggio pattern pop-up
    juce::ComboBox arpCombo;
    juce::Label arpLabel { "ARP" };

    // Root note display
    juce::Label rootLabel { "ROOT" };
    juce::Slider rootSlider { juce::Slider::LinearVertical };
    juce::AudioProcessorValueTreeState::SliderAttachment rootAttach { processor.apvts, MidiEffectProcessor::rootId, rootSlider.slider };

    // Velocity display
    juce::ComboBox velCombo;
    juce::Label velLabel { "VEL" };

    // Parameters attachments
    juce::AudioProcessorValueTreeState::SliderAttachment scaleAttach { processor.apvts, MidiEffectProcessor::scaleId, scaleCombo }; // needs adjustment
    juce::AudioProcessorValueTreeState::SliderAttachment chordAttach { processor.apvts, MidiEffectProcessor::chordId, chordCombo }; // needs adjustment
    juce::AudioProcessorValueTreeState::SliderAttachment arpAttach { processor.apvts, MidiEffectProcessor::arpId, arpCombo }; // needs adjustment
    juce::AudioProcessorValueTreeState::SliderAttachment velAttach { processor.apvts, MidiEffectProcessor::velocityId, velCombo }; // needs adjustment
};