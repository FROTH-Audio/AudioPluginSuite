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

class SpectralDriftEditor : public juce::AudioProcessorEditor,
                            private juce::Timer
{
public:
    explicit SpectralDriftEditor (SpectralDriftProcessor&);
    ~SpectralDriftEditor() override;

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

    SpectralDriftProcessor& processor;

    // Declared first so it is destroyed last, after every component that uses it.
    SuiteUI::SuiteLookAndFeel lookAndFeel { juce::Colour (0xff2de2ff) };
    HelpTracker helpTracker;

    juce::Label helpLabel;

    // The visualiser shows on both pages, so modulation is visible while routing.
    SuiteUI::SpectrumDisplay spectrum { juce::Colour (0xffa9b7c9), juce::Colour (0xff2de2ff) };

    // Sound page: two rows of knobs plus the LFO shape switch.
    SuiteUI::LabelledSlider rateSlider { "DRIFT RATE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider depthSlider { "DRIFT DEPTH", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider spreadSlider { "BIN SPREAD", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider stereoSlider { "STEREO PHASE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider shiftSlider { "SHIFT", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider tiltSlider { "TILT", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider gateSlider { "GATE", juce::Slider::RotaryVerticalDrag };
    juce::Label shapeCaption;
    SuiteUI::SegmentedSelector shapeSelector { juce::Colour (0xff2de2ff) };
    SuiteUI::LabelledSlider feedbackSlider { "FEEDBACK", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider dampingSlider { "FB DAMPING", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider driftStartSlider { "DRIFT START", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider outputSlider { "OUTPUT GAIN", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider mixSlider { "MIX", juce::Slider::RotaryVerticalDrag };
    juce::ToggleButton freezeButton { "FREEZE" };

    // Modulation page: the page switch, then the shared LFO and routing panel.
    SuiteUI::SegmentedSelector pageSelector { juce::Colour (0xff2de2ff) };
    SuiteModulation::ModulationPanel modulation { processor.getRouter(), modulationStyle() };
    std::unique_ptr<SuiteModulation::ControlDisplay> controls;  // built in the constructor, once the sliders exist

    // Preset system seam
    std::unique_ptr<SuitePresets::PresetBar> presetBar;

    juce::Rectangle<int> modulationGroupBounds;
    int page = 0;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;
    std::unique_ptr<juce::ParameterAttachment> shapeAttachment;

    std::vector<float> inputMagnitudes;
    std::vector<float> outputMagnitudes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectralDriftEditor)
};
