#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

#include "PluginProcessor.h"
#include "SuiteLookAndFeel.h"
#include "SuiteWidgets.h"
#include "SuiteBrand.h"

class SgraffitoEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit SgraffitoEditor (SgraffitoProcessor&);
    ~SgraffitoEditor() override;

    void paint (juce::Graphics&) override;

    /** Switches between the sound page (0) and the texture page (1). */
    void setPage (int page);
    int getPage() const noexcept { return page; }

private:
    void timerCallback() override;

    /** Forwards mouse moves from every child to the editor, so the help line follows the pointer. */
    struct HelpTracker : public juce::MouseListener
    {
        std::function<void (const juce::MouseEvent&)> onMove;
        void mouseMove (const juce::MouseEvent& e) override { if (onMove) onMove (e); }
    };

    void showHelp (const juce::MouseEvent&);
    void layoutSoundPage (juce::Rectangle<int> area);
    void layoutTexturePage (juce::Rectangle<int> area);

    SgraffitoProcessor& processor;

    // Declared first so it is destroyed last, after every component that uses it.
    SuiteUI::SuiteLookAndFeel lookAndFeel { juce::Colour (0xff9aa0a6) };
    HelpTracker helpTracker;

    juce::Label helpLabel;

    // Page switch.
    SuiteUI::SegmentedSelector pageSelector { juce::Colour (0xff9aa0a6) };

    // SOUND page controls.
    juce::Label modeCaption;
    SuiteUI::SegmentedSelector modeSelector { juce::Colour (0xff9aa0a6) };
    SuiteUI::LabelledSlider regionSpanSlider { "REGION SPAN", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider regionLengthSlider { "REGION LENGTH", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider rootKeySlider { "ROOT KEY", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider pitchFineSlider { "PITCH FINE", juce::Slider::RotaryVerticalDrag };
    juce::ToggleButton scaleLockButton { "SCALE LOCK" };
    SuiteUI::SegmentedSelector scaleSelector { juce::Colour (0xff9aa0a6) };
    SuiteUI::LabelledSlider glideSlider { "GLIDE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider levelSlider { "LEVEL", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider velocityCurveSlider { "VELOCITY CURVE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider attackSlider { "ATTACK", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider releaseSlider { "RELEASE", juce::Slider::RotaryVerticalDrag };

    // TEXTURE page controls.
    SuiteUI::LabelledSlider tapeStopTimeSlider { "TAPE STOP TIME", juce::Slider::RotaryVerticalDrag };
    SuiteUI::SegmentedSelector tapeStopCurveSelector { juce::Colour (0xff9aa0a6) };
    SuiteUI::LabelledSlider memoryAmountSlider { "MEMORY AMOUNT", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider memoryDecaySlider { "MEMORY DECAY", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider driftAmountSlider { "DRIFT AMOUNT", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider driftSpeedSlider { "DRIFT SPEED", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider layBackSlider { "LAY BACK", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider swingSlider { "SWING", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider humaniseSlider { "HUMANISE", juce::Slider::RotaryVerticalDrag };
    juce::ToggleButton ghostOnButton { "GHOST ON" };
    SuiteUI::LabelledSlider ghostThresholdSlider { "GHOST THRESHOLD", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider ghostLevelSlider { "GHOST LEVEL", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider ghostDelaySlider { "GHOST DELAY", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider sliceCountSlider { "SLICE COUNT", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider grainSizeSlider { "GRAIN SIZE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider grainDensitySlider { "GRAIN DENSITY", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider grainSpreadSlider { "GRAIN SPREAD", juce::Slider::RotaryVerticalDrag };
    juce::ToggleButton sustainHoldButton { "SUSTAIN HOLD" };
    SuiteUI::LabelledSlider swellTimeSlider { "SWELL TIME", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider swellCrossfadeSlider { "SWELL CROSSFADE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider dustSlider { "DUST", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider crackleSlider { "CRACKLE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider wowSlider { "WOW", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider flutterSlider { "FLUTTER", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider shuffleSeedSlider { "SHUFFLE SEED", juce::Slider::RotaryVerticalDrag };

    juce::Rectangle<int> textureGroupBounds;
    int page = 0;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> scaleLockAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ghostOnAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> sustainHoldAttachment;
    std::unique_ptr<juce::ParameterAttachment> modeAttachment;
    std::unique_ptr<juce::ParameterAttachment> scaleAttachment;
    std::unique_ptr<juce::ParameterAttachment> tapeStopCurveAttachment;

    void resized() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SgraffitoEditor)
};
