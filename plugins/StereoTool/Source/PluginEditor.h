#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>

#include "PluginProcessor.h"
#include "SuiteHeaderMenu.h"
#include "SuiteLookAndFeel.h"
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

private:
    /** A caption above a combo box, in one grid cell. Kept for choices the editor may
        still show as a list (plan 11 D1 keeps dropdowns only above six options). */
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

    /** The stereo visual (plan 11 4c): mid/side level bars with a width indicator above a
        scrolling trace of the transient follower's envelope. Reads the processor's
        published snapshot on the 60 Hz timer; the trace's history lives here on the
        message thread. No locks. */
    class MidSideMeter : public juce::Component
    {
    public:
        explicit MidSideMeter (StereoToolProcessor& p) : processor (p) {}

        /** One timer tick: append the current envelope to the trace. */
        void tick() { envelopeHistory[(size_t) historyHead] = processor.getMeterSnapshot().followerEnvelope.load (std::memory_order_relaxed); historyHead = (historyHead + 1) % (int) envelopeHistory.size(); repaint(); }

        void paint (juce::Graphics&) override;

    private:
        StereoToolProcessor& processor;
        std::array<float, 256> envelopeHistory {};
        int historyHead = 0;
    };

    StereoToolProcessor& processor;

    // Declared first so it is destroyed last, after every component that uses it.
    SuiteUI::SuiteLookAndFeel lookAndFeel { juce::Colour (0xffB18CFF) };

    // CHARACTER group: the shaper and the transient follower.
    SuiteUI::LabelledSlider driveCtrl   { "DRIVE",   juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider attackCtrl  { "ATTACK",  juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider releaseCtrl { "RELEASE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider amountCtrl  { "AMOUNT",  juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSelector characterCell { "CHARACTER", juce::Colour (0xffB18CFF) };
    SuiteUI::LabelledSelector targetCell    { "TARGET", juce::Colour (0xffB18CFF) };

    // IMAGE group: the stereo image controls.
    SuiteUI::LabelledSlider widthCtrl    { "WIDTH",    juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider mixCtrl      { "MIX",      juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider midFocusCtrl { "MID FOCUS", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider crossoverCtrl { "CROSSOVER", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSelector filterCell { "SIDE FILTER", juce::Colour (0xffB18CFF) };

    // The pill selectors' parameter attachments (declared after the cells, so they are
    // destroyed first — plan 11 D1, wired through SuiteUI::attachSelector).
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

    juce::Rectangle<int> characterGroupBounds, imageGroupBounds, visualBounds;

    // Visual + header (plan 11 4c/R13).
    MidSideMeter visualizer { processor };
    std::unique_ptr<SuitePresets::PresetBar> presetBar;
    SuiteUI::HeaderMenuButton headerMenu { juce::Colour (0xffB18CFF) };

    void timerCallback() override { visualizer.tick(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StereoToolEditor)
};
