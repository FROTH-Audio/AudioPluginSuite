#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

#include "PluginProcessor.h"
#include "SuiteHeaderMenu.h"
#include "SuiteLookAndFeel.h"
#include "SuiteSelectorAttachment.h"
#include "SuiteWidgets.h"
#include "SuitePresets.h"

class MultibandSaturationEditor : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    explicit MultibandSaturationEditor (MultibandSaturationProcessor&);
    ~MultibandSaturationEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MultibandSaturationProcessor& processor;

    /** The two-band visual (plan 11 4b): a bar per band (the block's envelope maximum)
        with the crossover frequency marked between them, and input/output meters on the
        right. Reads the processor's published snapshot on the 60 Hz timer; no locks. */
    class DualBandVisualizer : public juce::Component
    {
    public:
        explicit DualBandVisualizer (MultibandSaturationProcessor& p);

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        MultibandSaturationProcessor& processor;
        SuiteUI::LevelMeter inputMeter  { SuiteUI::Colours::textDim };
        SuiteUI::LevelMeter outputMeter { SuiteUI::Colours::text };
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

    SuiteUI::LabelledSelector modeCell { "MODE", juce::Colour (0xffFF6B6B) };

    // The pill selector's parameter attachment (declared after the cell, so it is
    // destroyed first — plan 11 D1, wired through SuiteUI::attachSelector).
    std::unique_ptr<juce::ParameterAttachment> modeAttachment;

    juce::Rectangle<int> splitGroupBounds, shaperGroupBounds, visualBounds;

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

    // Visual + header (plan 11 4b/R13).
    DualBandVisualizer visualizer { processor };

    void timerCallback() override { visualizer.repaint(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultibandSaturationEditor)
};
