#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "SuiteLookAndFeel.h"
#include "SuiteWidgets.h"

class MultibandSaturationEditor : public juce::AudioProcessorEditor
{
public:
    explicit MultibandSaturationEditor (MultibandSaturationProcessor&);
    ~MultibandSaturationEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MultibandSaturationProcessor& processor;

    // Declared first so it is destroyed last, after every component that uses it.
    // Provisional accent (owner may pick a permanent one before release).
    SuiteUI::SuiteLookAndFeel lookAndFeel { juce::Colour (0xffFF6B6B) };

    SuiteUI::LabelledSlider crossoverCtrl { "CROSSOVER", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider driveLowCtrl   { "DRIVE LOW",  juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider driveHighCtrl  { "DRIVE HIGH", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider toneLowCtrl    { "TONE LOW",   juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider toneHighCtrl   { "TONE HIGH",  juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider sensitivityCtrl{ "SENSITIVITY", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider attackCtrl     { "ATTACK",     juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider releaseCtrl    { "RELEASE",    juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider mixCtrl        { "MIX",        juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider outputCtrl     { "OUTPUT",     juce::Slider::RotaryVerticalDrag };

    juce::ComboBox modeCombo;

    juce::AudioProcessorValueTreeState::SliderAttachment crossoverAttachment { processor.apvts, MultibandSaturationProcessor::crossoverId,   crossoverCtrl.slider };
    juce::AudioProcessorValueTreeState::SliderAttachment driveLowAttachment   { processor.apvts, MultibandSaturationProcessor::driveLowId,    driveLowCtrl.slider };
    juce::AudioProcessorValueTreeState::SliderAttachment driveHighAttachment  { processor.apvts, MultibandSaturationProcessor::driveHighId,   driveHighCtrl.slider };
    juce::AudioProcessorValueTreeState::SliderAttachment toneLowAttachment    { processor.apvts, MultibandSaturationProcessor::toneLowId,     toneLowCtrl.slider };
    juce::AudioProcessorValueTreeState::SliderAttachment toneHighAttachment   { processor.apvts, MultibandSaturationProcessor::toneHighId,    toneHighCtrl.slider };
    juce::AudioProcessorValueTreeState::SliderAttachment sensitivityAttachment{ processor.apvts, MultibandSaturationProcessor::sensitivityId, sensitivityCtrl.slider };
    juce::AudioProcessorValueTreeState::SliderAttachment attackAttachment     { processor.apvts, MultibandSaturationProcessor::attackId,      attackCtrl.slider };
    juce::AudioProcessorValueTreeState::SliderAttachment releaseAttachment    { processor.apvts, MultibandSaturationProcessor::releaseId,     releaseCtrl.slider };
    juce::AudioProcessorValueTreeState::SliderAttachment mixAttachment        { processor.apvts, MultibandSaturationProcessor::mixId,         mixCtrl.slider };
    juce::AudioProcessorValueTreeState::SliderAttachment outputAttachment     { processor.apvts, MultibandSaturationProcessor::outputLevelId, outputCtrl.slider };

    // Built in the constructor after the combo's items exist, so its first update selects
    // the parameter's current choice (an in-class attachment would attach to an empty combo).
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
};
