#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "SuiteLookAndFeel.h"

class SgraffitoEditor : public juce::AudioProcessorEditor
{
public:
    explicit SgraffitoEditor (SgraffitoProcessor&);
    ~SgraffitoEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    SgraffitoProcessor& processor;

    // Declared first so it is destroyed last, after every component that uses it.
    SuiteUI::SuiteLookAndFeel lookAndFeel { juce::Colour (0xff7f8c9a) };
};
