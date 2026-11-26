#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

#include "PluginProcessor.h"
#include "SuiteHeaderMenu.h"
#include "SuiteHelp.h"
#include "SuiteLookAndFeel.h"
#include "SuiteModulationControls.h"
#include "SuiteModulationPanel.h"
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

    /** Switches between the sound controls (0) and the modulation page (1). The tests use it too. */
    void setPage (int page);
    int getPage() const noexcept { return currentPage; }
    bool areInterestControlsVisible() const noexcept
    {
        return foldLowCtrl.isVisible() && foldHighCtrl.isVisible() && thermalCtrl.isVisible()
            && mixLowCtrl.isVisible() && mixHighCtrl.isVisible();
    }

    /** The layout's bands, for the tests (plan 12). Valid after resized() ran. */
    juce::Rectangle<int> visualRect() const noexcept { return visualBounds; }
    juce::Rectangle<int> splitRect() const noexcept { return splitGroupBounds; }
    juce::Component* visualComponent() { return &visualizer; }

private:
    void timerCallback() override;

    /** The LFO colours of the modulation panel. */
    static SuiteModulation::ModulationPanel::Style modulationStyle();

    void layoutSoundPage (juce::Rectangle<int> area);
    void layoutModPage (juce::Rectangle<int> area);

    MultibandSaturationProcessor& processor;

    /** The two-band visual (plan 11 4b; plan 15 removed the stereo meter pair on the
        owner's request and added the frequency grid): a bar per band (the block's envelope
        maximum) over a log frequency grid, with the crossover frequency marked between
        them. Reads the processor's published snapshot on the 60 Hz timer; no locks. */
    class DualBandVisualizer : public juce::Component
    {
    public:
        explicit DualBandVisualizer (MultibandSaturationProcessor& p);

        void paint (juce::Graphics&) override;

    private:
        MultibandSaturationProcessor& processor;
    };

    // Declared first so it is destroyed last, after every component that uses it.
    SuiteUI::SuiteLookAndFeel lookAndFeel { juce::Colour (0xffFF6B6B) };

    // Preset bar and menu
    std::unique_ptr<SuitePresets::PresetBar> presetBar;
    SuiteUI::HeaderMenuButton headerMenu { juce::Colour (0xffFF6B6B) };

    // The shared help line (plan 12).
    SuiteUI::HelpTracker helpTracker;
    SuiteUI::HelpLine helpLine;

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
    // Plan 17 Phase 9: the interest knobs.
    SuiteUI::LabelledSlider foldLowCtrl    { "FOLD LOW",   juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider foldHighCtrl   { "FOLD HIGH",  juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider thermalCtrl    { "THERMAL",    juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider mixLowCtrl     { "MIX LOW",    juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider mixHighCtrl    { "MIX HIGH",   juce::Slider::RotaryVerticalDrag };

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
    // Plan 17 Phase 9.
    juce::AudioProcessorValueTreeState::SliderAttachment mixLowAttachment     { processor.apvts, MultibandSaturationProcessor::mixLowId,      mixLowCtrl.slider };
    juce::AudioProcessorValueTreeState::SliderAttachment mixHighAttachment    { processor.apvts, MultibandSaturationProcessor::mixHighId,     mixHighCtrl.slider };
    juce::AudioProcessorValueTreeState::SliderAttachment thermalAttachment    { processor.apvts, MultibandSaturationProcessor::thermalDriftId, thermalCtrl.slider };
    juce::AudioProcessorValueTreeState::SliderAttachment foldLowAttachment    { processor.apvts, MultibandSaturationProcessor::foldLowId,     foldLowCtrl.slider };
    juce::AudioProcessorValueTreeState::SliderAttachment foldHighAttachment   { processor.apvts, MultibandSaturationProcessor::foldHighId,    foldHighCtrl.slider };

    // Page switch and modulation panel (plan 13). The visualiser shows on both pages.
    SuiteUI::SegmentedSelector pageSelector { juce::Colour (0xffFF6B6B) };
    SuiteModulation::ModulationPanel modulation { processor.getRouter(), modulationStyle() };
    std::unique_ptr<SuiteModulation::ControlDisplay> controls;

    // Visual + header (plan 11 4b/R13).
    DualBandVisualizer visualizer { processor };

    int currentPage = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultibandSaturationEditor)
};
