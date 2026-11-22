#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>

#include "PluginProcessor.h"
#include "SuiteHeaderMenu.h"
#include "SuiteHelp.h"
#include "SuiteLookAndFeel.h"
#include "SuiteModulationControls.h"
#include "SuiteModulationPanel.h"
#include "SuiteSelectorAttachment.h"
#include "SuiteWidgets.h"

class StereoToolEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    explicit StereoToolEditor (StereoToolProcessor&);
    ~StereoToolEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Switches between the sound controls (0) and the modulation page (1). The tests use it too. */
    void setPage (int page);
    int getPage() const noexcept { return currentPage; }

    /** Test seam (plan 15): the live affinity state of the mode-dependent cells. */
    bool isCrossoverEnabled() const noexcept { return crossoverCtrl.isEnabled(); }
    bool isTransientRowEnabled() const noexcept { return attackCtrl.isEnabled(); }

private:
    /** The stereo visual (plan 11 4c, reworked by plan 13; plan 15 removed the stereo meter
        pair on the owner's request): mid/side level bars and a scrolling trace of the
        transient follower's envelope. Reads the processor's published snapshot on the 60 Hz
        timer; the trace's history lives here on the message thread. No locks. */
    class MidSideMeter : public juce::Component
    {
    public:
        explicit MidSideMeter (StereoToolProcessor& p);

        /** One timer tick: append the current envelope to the trace. */
        void tick();

        void paint (juce::Graphics&) override;

    private:
        StereoToolProcessor& processor;
        std::array<float, 256> envelopeHistory {};
        int historyHead = 0;
    };

    StereoToolProcessor& processor;

    // Declared first so it is destroyed last, after every component that uses it.
    SuiteUI::SuiteLookAndFeel lookAndFeel { juce::Colour (0xffB18CFF) };

    // CHARACTER group: the shaper and the transient follower. The character list uses a
    // dropdown because its option text is long (owner finding, plan 13).
    SuiteUI::LabelledSlider driveCtrl   { "DRIVE",   juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider attackCtrl  { "ATTACK",  juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider releaseCtrl { "RELEASE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider amountCtrl  { "AMOUNT",  juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledDropdown characterCell { "CHARACTER" };
    SuiteUI::LabelledSelector targetCell    { "TARGET", juce::Colour (0xffB18CFF) };

    // IMAGE group: the stereo image controls. The side filter uses a dropdown for the same
    // reason (owner finding, plan 13).
    SuiteUI::LabelledSlider widthCtrl    { "WIDTH",    juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider mixCtrl      { "MIX",      juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider midFocusCtrl { "MID FOCUS", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider crossoverCtrl { "CROSSOVER", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledDropdown filterCell { "SIDE FILTER" };

    // LEVEL group (plan 12): beside the visualiser, so the window stays 880x700.
    SuiteUI::LabelledSlider inputCtrl  { "INPUT",  juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider outputCtrl { "OUTPUT", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider panCtrl    { "PAN",    juce::Slider::RotaryVerticalDrag };

    // The choice widgets' parameter attachments (declared after the cells, so they are
    // destroyed first — wired through SuiteUI::attachSelector / attachDropdown).
    std::unique_ptr<juce::ParameterAttachment> characterAttachment;
    std::unique_ptr<juce::ParameterAttachment> targetAttachment;
    std::unique_ptr<juce::ParameterAttachment> filterAttachment;

    // Built in the constructor after the combo's items exist, so their first update selects
    // the parameter's current choice (an in-class attachment would attach to an empty combo).
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> widthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> midFocusAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> crossoverAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> panAttachment;

    juce::Rectangle<int> characterGroupBounds, imageGroupBounds, levelGroupBounds, visualBounds;
    juce::Rectangle<int> modulationGroupBounds;

    // Visual + header (plan 11 4c/R13).
    MidSideMeter visualizer { processor };
    std::unique_ptr<SuitePresets::PresetBar> presetBar;
    SuiteUI::HeaderMenuButton headerMenu { juce::Colour (0xffB18CFF) };

    // Page switch and modulation panel (plan 13). The preview shows on both pages, because it
    // carries the input/output meters.
    SuiteUI::SegmentedSelector pageSelector { juce::Colour (0xffB18CFF) };
    SuiteModulation::ModulationPanel modulation { processor.getRouter(), modulationStyle() };
    std::unique_ptr<SuiteModulation::ControlDisplay> controls;

    int currentPage = 0;

    /** The LFO colours of the modulation panel. */
    static SuiteModulation::ModulationPanel::Style modulationStyle();

    void layoutSoundPage (juce::Rectangle<int> area);
    void layoutModPage (juce::Rectangle<int> area);

    // The shared help line (plan 12).
    SuiteUI::HelpTracker helpTracker;
    SuiteUI::HelpLine helpLine;

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StereoToolEditor)
};
