#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "SuiteHeaderMenu.h"
#include "SuiteLookAndFeel.h"
#include "SuitePresets.h"
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
    SuiteUI::SuiteLookAndFeel lookAndFeel { juce::Colour (0xff9aa0a6) };

    // R12: the factory preset bar, flat in the header. Created before setSize(), because
    // setSize() runs resized(), which lays the bar out.
    std::unique_ptr<SuitePresets::PresetBar> presetBar;

    // R13: one menu at the far right of the header, right of the preset bar.
    SuiteUI::HeaderMenuButton headerMenu { juce::Colour (0xff9aa0a6) };

    SuiteUI::LabelledSlider gainSlider { "GAIN", juce::Slider::RotaryVerticalDrag };
    juce::AudioProcessorValueTreeState::SliderAttachment gainAttachment;
};
