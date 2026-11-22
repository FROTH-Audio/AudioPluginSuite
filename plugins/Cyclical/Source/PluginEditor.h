#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>
#include <vector>

#include "LoopWaveformView.h"
#include "PluginProcessor.h"
#include "SuiteHeaderMenu.h"
#include "SuiteWidgets.h"
#include "SuiteHelp.h"
#include "SuiteLookAndFeel.h"
#include "SuitePresets.h"

class CyclicalEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit CyclicalEditor (CyclicalProcessor&);
    ~CyclicalEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** The page switch: 0 = SLOTS, 1 = GLOBAL (tests use it too). */
    void setPage (int page);
    int getPage() const noexcept { return page; }

    /** The plan 10 G1 affinity rule, checked disabled-not-invisible by the tests. */
    bool isSpeedEnabled (int slot) const { return cards[(size_t) slot]->speed.isEnabled(); }
    bool isPingRandomEnabled (int slot) const { return cards[(size_t) slot]->pingRand.isEnabled(); }
    bool isSpeedVisible (int slot) const { return cards[(size_t) slot]->isVisible() && cards[(size_t) slot]->speed.isVisible(); }

    /** Test seams for the plan 14 affinity rules (plan 10 G1, checked by the tests). */
    bool isBeatsEnabled (int slot) const { return cards[(size_t) slot]->beats.isEnabled(); }
    bool isSlicesEnabled (int slot) const { return cards[(size_t) slot]->slices.isEnabled(); }
    bool isSliceLengthEnabled (int slot) const { return cards[(size_t) slot]->sliceLength.isEnabled(); }

private:
    /** One slot's card: title, waveform, two knob rows and the mode switches (plan 14:
        the second row carries Note, Beats, Slices and Slice Length; Chop joins the pills). */
    struct SlotCard : juce::Component
    {
        SlotCard (int slotIndex, juce::Colour accentColour);

        /** The title shows the slot's trigger note (plan 14); the timer keeps it in step. */
        void setTriggerNote (int note);

        // The colour first: the members below are initialised from it.
        juce::Colour accentColour;
        int slot;
        int titleNote = -1;
        juce::Label title;
        LoopWaveformView waveform;
        SuiteUI::LabelledSlider start { "START", juce::Slider::RotaryVerticalDrag };
        SuiteUI::LabelledSlider length { "LENGTH", juce::Slider::RotaryVerticalDrag };
        SuiteUI::LabelledSlider speed { "SPEED", juce::Slider::RotaryVerticalDrag };
        SuiteUI::LabelledSlider velocity { "VELOCITY", juce::Slider::RotaryVerticalDrag };
        SuiteUI::LabelledSlider note { "NOTE", juce::Slider::RotaryVerticalDrag };
        SuiteUI::LabelledSlider beats { "BEATS", juce::Slider::RotaryVerticalDrag };
        SuiteUI::LabelledSlider slices { "SLICES", juce::Slider::RotaryVerticalDrag };
        SuiteUI::LabelledSlider sliceLength { "SLICE LENGTH", juce::Slider::RotaryVerticalDrag };
        SuiteUI::SegmentedSelector direction { accentColour };
        juce::ToggleButton pingRand { "PING RAND" };
        juce::ToggleButton granular { "GRAIN" };
        juce::ToggleButton chop { "CHOP" };

        void resized() override;
    };

    void timerCallback() override;
    void browseForLoop (int slot);
    void updateAffinity();

    CyclicalProcessor& processor;

    // Declared first so it is destroyed last, after every component that uses it.
    // Owner decision 2026-11-18: #FF7A45.
    SuiteUI::SuiteLookAndFeel lookAndFeel { juce::Colour (0xffFF7A45) };

    SuiteUI::HelpTracker helpTracker;
    SuiteUI::HelpLine helpLine;

    // R12: the factory preset bar, flat in the header.
    std::unique_ptr<SuitePresets::PresetBar> presetBar;

    // R13: one menu at the far right of the header, right of the preset bar.
    SuiteUI::HeaderMenuButton headerMenu { juce::Colour (0xffFF7A45) };

    // R8: the page switch at the top right of the header.
    SuiteUI::SegmentedSelector pageSelector { juce::Colour (0xffFF7A45) };
    int page = 0;

    std::array<std::unique_ptr<SlotCard>, CyclicalProcessor::numSlots> cards;

    // GLOBAL page: the shared knobs and the transport selectors.
    SuiteUI::LabelledSlider masterCtrl { "OUTPUT", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider panCtrl { "PAN", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider pitchCtrl { "PITCH", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider crossfadeCtrl { "CROSSFADE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider swingCtrl { "SWING", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider tempoCtrl { "TEMPO", juce::Slider::RotaryVerticalDrag };
    juce::ToggleButton syncToggle { "HOST SYNC" };
    SuiteUI::SegmentedSelector quantizeSelector { juce::Colour (0xffFF7A45) };
    SuiteUI::SegmentedSelector triggerSelector { juce::Colour (0xffFF7A45) };
    juce::Label quantizeCaption, triggerCaption;
    juce::Label beatReadout;

    juce::Rectangle<int> globalGroupBounds;

    // Attachments, built in the constructor once the widgets exist.
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::ParameterAttachment>> choiceAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;

    // The loop browser, kept alive as a member (deleting the chooser aborts the dialog).
    std::unique_ptr<juce::FileChooser> loopChooser;
    int chooserSlot = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CyclicalEditor)
};
