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

    // Look and feel with suite accent colour
    SuiteUI::SuiteLookAndFeel lookAndFeel { juce::Colour (0xff9aa0a6) };

    // Width control: -100 to +100 % (0 = correlated stereo)
    SuiteUI::LabelledWidthCtrl widthCtrl { "WIDTH", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider mixCtrl { "MIX", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider driveCtrl { "DRIVE", juce::Slider::RotaryVerticalDrag };

    // Parameters attachments
    juce::AudioProcessorValueTreeState::SliderAttachment widthAttach { processor.apvts, StereoToolProcessor::widthId, widthCtrl.slider };
    juce::AudioProcessorValueTreeState::SliderAttachment mixAttach  { processor.apvts, StereoToolProcessor::mixId,   mixCtrl.slider };
    juce::AudioProcessorValueTreeState::SliderAttachment driveAttach{ processor.apvts, StereoToolProcessor::driveId, driveCtrl.slider };
};