#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

#include "PluginProcessor.h"
#include "SuiteLookAndFeel.h"
#include "SuiteModulationControls.h"
#include "SuiteModulationPanel.h"
#include "SuiteWidgets.h"

class DynamicSaturationEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit DynamicSaturationEditor (DynamicSaturationProcessor&);
    ~DynamicSaturationEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Switches between the sound controls (0) and the modulation page (1). The tests use it too. */
    void setPage (int page);
    int getPage() const noexcept { return page; }

private:
    void timerCallback() override;

    /** The LFO colours of the modulation panel. */
    static SuiteModulation::ModulationPanel::Style modulationStyle();

    /** Forwards mouse moves from every child to the editor, so the help line follows the pointer. */
    struct HelpTracker : public juce::MouseListener
    {
        std::function<void (const juce::MouseEvent&)> onMove;
        void mouseMove (const juce::MouseEvent& e) override { if (onMove) onMove (e); }
    };

    void showHelp (const juce::MouseEvent&);
    void layoutSoundPage (juce::Rectangle<int> area);
    void layoutModPage (juce::Rectangle<int> area);

    DynamicSaturationProcessor& processor;

    // Declared first so it is destroyed last, after every component that uses it.
    SuiteUI::SuiteLookAndFeel lookAndFeel { juce::Colour (0xffff9f1c) };
    HelpTracker helpTracker;

    juce::Label helpLabel;

    // The visualiser shows on both pages, so modulation is visible while routing.
    SuiteUI::CurveView transferCurve { juce::Colour (0xffff9f1c) };
    SuiteUI::LevelMeter inputMeter { juce::Colour (0xffff9f1c) };

    // Sound page: mode, then two rows of knobs
    SuiteUI::SegmentedSelector modeSelector { juce::Colour (0xffff9f1c) };

    SuiteUI::LabelledSlider driveSlider { "DRIVE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider sensitivitySlider { "SENSITIVITY", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider curveSlider { "CURVE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider attackSlider { "ATTACK", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider releaseSlider { "RELEASE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider mixSlider { "MIX", juce::Slider::RotaryVerticalDrag };

    SuiteUI::LabelledSlider detectHpfSlider { "DETECT HPF", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider stereoLinkSlider { "STEREO LINK", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider biasSlider { "TUBE BIAS", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider foldSlider { "FOLD", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider toneSlider { "TONE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider outputSlider { "OUTPUT", juce::Slider::RotaryVerticalDrag };

    // Modulation page: the page switch, then the shared LFO and routing panel
    SuiteUI::SegmentedSelector pageSelector { juce::Colour (0xffff9f1c) };
    SuiteModulation::ModulationPanel modulation { processor.getRouter(), modulationStyle() };
    std::unique_ptr<SuiteModulation::ControlDisplay> controls;  // built in the constructor, once the sliders exist

    // Preset system seam
    std::unique_ptr<SuitePresets::PresetBar> presetBar;

    juce::Rectangle<int> modulationGroupBounds;
    int page = 0;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::unique_ptr<juce::ParameterAttachment> modeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynamicSaturationEditor)
};
