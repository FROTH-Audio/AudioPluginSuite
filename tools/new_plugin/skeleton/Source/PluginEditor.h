#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "SuiteLookAndFeel.h"
#include "SuiteWidgets.h"

class __NAME__Editor : public juce::AudioProcessorEditor
{
public:
    explicit __NAME__Editor (__NAME__Processor&);
    ~__NAME__Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    __NAME__Processor& processor;

    // Declared first so it is destroyed last, after every component that uses it.
    SuiteUI::SuiteLookAndFeel lookAndFeel { juce::Colour (0xff7f8c9a) };

    SuiteUI::LabelledSlider gainSlider { "GAIN", juce::Slider::RotaryVerticalDrag };
    juce::AudioProcessorValueTreeState::SliderAttachment gainAttachment;
};
