#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

#include "PluginProcessor.h"
#include "SuiteLookAndFeel.h"
#include "SuiteWidgets.h"
#include "SuiteHeaderMenu.h"

class MultibandSaturationEditor : public juce::AudioProcessorEditor
{
public:
    explicit MultibandSaturationEditor (MultibandSaturationProcessor&);
    ~MultibandSaturationEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MultibandSaturationProcessor& processor;

    /** A caption above a combo box, in one grid cell. */
    class ComboCell : public juce::Component
    {
    public:
        explicit ComboCell (const char* captionText)
        {
            label.setText (captionText, juce::dontSendNotification);
            label.setFont (SuiteUI::uiFont (13.0f, true));
            label.setColour (juce::Label::textColourId, SuiteUI::Colours::textDim);
            label.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (label);
            box.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (box);
        }

        juce::ComboBox box;
        juce::Label label;

        void resized() override
        {
            auto area = getLocalBounds();
            label.setBounds (area.removeFromTop (16));
            area.removeFromTop (2);
            box.setBounds (area.removeFromBottom (28).reduced (8, 0));
        }
    };

    // Declared first so it is destroyed last, after every component that uses it.
    SuiteUI::SuiteLookAndFeel lookAndFeel { juce::Colour (0xffFF6B6B) };

    // Preset bar and gear menu
    std::unique_ptr<SuitePresets::PresetBar> presetBar;
    SuiteUI::HeaderMenuButton gearButton { juce::Colour (0xffFF6B6B) };

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

    ComboCell modeCell { "MODE" };

    juce::Rectangle<int> splitGroupBounds, shaperGroupBounds;

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