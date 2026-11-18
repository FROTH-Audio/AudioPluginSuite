#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "SuiteLookAndFeel.h"
#include "SuiteWidgets.h"

class StereoToolEditor : public juce::AudioProcessorEditor
{
public:
    explicit StereoToolEditor (StereoToolProcessor&);
    ~StereoToolEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    StereoToolProcessor& processor;

    // Declared first so it is destroyed last, after every component that uses it.
    // Provisional accent (owner may pick a permanent one before release).
    SuiteUI::SuiteLookAndFeel lookAndFeel { juce::Colour (0xffB18CFF) };

    SuiteUI::LabelledSlider widthCtrl { "WIDTH", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider driveCtrl { "DRIVE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider mixCtrl   { "MIX",   juce::Slider::RotaryVerticalDrag };

    juce::AudioProcessorValueTreeState::SliderAttachment widthAttachment { processor.apvts, StereoToolProcessor::widthId, widthCtrl.slider };
    juce::AudioProcessorValueTreeState::SliderAttachment driveAttachment { processor.apvts, StereoToolProcessor::driveId, driveCtrl.slider };
    juce::AudioProcessorValueTreeState::SliderAttachment mixAttachment   { processor.apvts, StereoToolProcessor::mixId,   mixCtrl.slider };
};
