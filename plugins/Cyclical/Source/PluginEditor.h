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

private:
    /** One slot's card: title, waveform, four knobs and the mode switches. */
    struct SlotCard : juce::Component
    {
        SlotCard (int slotIndex, juce::Colour accentColour);

        // The colour first: the members below are initialised from it.
        juce::Colour accentColour;
        int slot;
        juce::Label title;
        LoopWaveformView waveform;
        SuiteUI::LabelledSlider start { "START", juce::Slider::RotaryVerticalDrag };
        SuiteUI::LabelledSlider length { "LENGTH", juce::Slider::RotaryVerticalDrag };
        SuiteUI::LabelledSlider speed { "SPEED", juce::Slider::RotaryVerticalDrag };
        SuiteUI::LabelledSlider velocity { "VELOCITY", juce::Slider::RotaryVerticalDrag };
        SuiteUI::SegmentedSelector direction { accentColour };
        juce::ToggleButton pingRand { "PING RAND" };
        juce::ToggleButton granular { "GRAIN" };

        void resized() override;
    };

    void timerCallback() override;
    void browseForLoop (int slot);
    void updateAffinity();

    CyclicalProcessor& processor;

    // Declared first so it is destroyed last, after every component that uses it.
    // Owner decision 2026-11-18: #8FA3B8.
    SuiteUI::SuiteLookAndFeel lookAndFeel { juce::Colour (0xff8fa3b8) };

    SuiteUI::HelpTracker helpTracker;
    juce::Label helpLabel;

    // R12: the factory preset bar, flat in the header.
    std::unique_ptr<SuitePresets::PresetBar> presetBar;

    // R13: one gear menu at the far right of the header, right of the preset bar.
    SuiteUI::HeaderMenuButton headerMenu { juce::Colour (0xff8fa3b8) };

    // R8: the page switch at the top right of the header.
    SuiteUI::SegmentedSelector pageSelector { juce::Colour (0xff8fa3b8) };
    int page = 0;

    std::array<std::unique_ptr<SlotCard>, CyclicalProcessor::numSlots> cards;

    // GLOBAL page: the shared knobs and the transport selectors.
    SuiteUI::LabelledSlider masterCtrl { "MASTER", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider panCtrl { "PAN", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider pitchCtrl { "PITCH", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider crossfadeCtrl { "CROSSFADE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider swingCtrl { "SWING", juce::Slider::RotaryVerticalDrag };
    SuiteUI::SegmentedSelector quantizeSelector { juce::Colour (0xff8fa3b8) };
    SuiteUI::SegmentedSelector triggerSelector { juce::Colour (0xff8fa3b8) };
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
