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

    // Declared first so it is destroyed last, after every component that uses it.
    // Provisional accent (owner may pick a permanent one before release).
    SuiteUI::SuiteLookAndFeel lookAndFeel { juce::Colour (0xff4ECDC4) };

    SuiteUI::LabelledSlider rootCtrl     { "ROOT",     juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider octaveCtrl   { "OCTAVE",   juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider velocityCtrl { "VELOCITY", juce::Slider::RotaryVerticalDrag };

    juce::ComboBox scaleCombo;
    juce::ComboBox chordCombo;
    juce::ComboBox arpCombo;

    juce::AudioProcessorValueTreeState::SliderAttachment rootAttachment     { processor.apvts, MidiEffectProcessor::rootId,     rootCtrl.slider };
    juce::AudioProcessorValueTreeState::SliderAttachment octaveAttachment   { processor.apvts, MidiEffectProcessor::octaveId,   octaveCtrl.slider };
    juce::AudioProcessorValueTreeState::SliderAttachment velocityAttachment { processor.apvts, MidiEffectProcessor::velocityId, velocityCtrl.slider };
    juce::AudioProcessorValueTreeState::ComboBoxAttachment scaleAttachment  { processor.apvts, MidiEffectProcessor::scaleId,    scaleCombo };
    juce::AudioProcessorValueTreeState::ComboBoxAttachment chordAttachment  { processor.apvts, MidiEffectProcessor::chordId,    chordCombo };
    juce::AudioProcessorValueTreeState::ComboBoxAttachment arpAttachment    { processor.apvts, MidiEffectProcessor::arpId,      arpCombo };
};
