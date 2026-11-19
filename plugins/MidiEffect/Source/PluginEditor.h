#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

#include "PluginProcessor.h"
#include "SuiteHeaderMenu.h"
#include "SuiteLookAndFeel.h"
#include "SuiteSelectorAttachment.h"
#include "SuiteWidgets.h"

class MidiEffectEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    explicit MidiEffectEditor (MidiEffectProcessor&);
    ~MidiEffectEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    /** A caption above a combo box, in one grid cell. Kept for CHORD, the one choice
        with more than six options (plan 11 rule #3). */
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

    /** The chord/arp visual (plan 11 4a, D6): an 8-step strip with the clock's current
        step outlined and sounding steps filled, above a two-octave mini keyboard with
        the last key's notes marked. Reads the processor's published snapshot on the
        60 Hz timer; no locks. */
    class ChordGridVisualizer : public juce::Component
    {
    public:
        explicit ChordGridVisualizer (MidiEffectProcessor& p) : processor (p) {}

        void paint (juce::Graphics&) override;

    private:
        MidiEffectProcessor& processor;
    };

    MidiEffectProcessor& processor;

    // Declared first so it is destroyed last, after every component that uses it.
    SuiteUI::SuiteLookAndFeel lookAndFeel { juce::Colour (0xff4ECDC4) };

    // SHAPE group: the key, the chord and the output level.
    SuiteUI::LabelledSlider rootCtrl     { "ROOT",     juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider octaveCtrl   { "OCTAVE",   juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider velocityCtrl { "VELOCITY", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider spreadCtrl   { "SPREAD",   juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSelector scaleCell { "SCALE", juce::Colour (0xff4ECDC4) };
    SuiteUI::LabelledSelector arpCell   { "PATTERN", juce::Colour (0xff4ECDC4) };
    ComboCell chordCell { "CHORD" };   // eight options: stays a dropdown (plan 11 rule #3)

    // ARP group: the step clock and the per-step velocity shape.
    SuiteUI::LabelledSlider tempoCtrl { "TEMPO", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider gateCtrl  { "GATE",  juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSelector modeCell  { "MODE", juce::Colour (0xff4ECDC4) };
    SuiteUI::LabelledSelector rateCell  { "STEP RATE", juce::Colour (0xff4ECDC4) };
    SuiteUI::LabelledSelector syncCell  { "SYNC", juce::Colour (0xff4ECDC4) };
    SuiteUI::LabelledSelector curveCell { "VEL CURVE", juce::Colour (0xff4ECDC4) };
    SuiteUI::LabelledSelector dirCell   { "VEL DIR", juce::Colour (0xff4ECDC4) };

    // The pill selectors' parameter attachments (declared after the cells, so they are
    // destroyed first — plan 11 D1, wired through SuiteUI::attachSelector).
    std::unique_ptr<juce::ParameterAttachment> scaleAttachment;
    std::unique_ptr<juce::ParameterAttachment> arpAttachment;
    std::unique_ptr<juce::ParameterAttachment> modeAttachment;
    std::unique_ptr<juce::ParameterAttachment> rateAttachment;
    std::unique_ptr<juce::ParameterAttachment> syncAttachment;
    std::unique_ptr<juce::ParameterAttachment> curveCellAttachment;
    std::unique_ptr<juce::ParameterAttachment> dirCellAttachment;

    // Built in the constructor after the combo's items exist, so its first update selects
    // the parameter's current choice (an in-class attachment would attach to an empty combo).
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rootAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> octaveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> velocityAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> spreadAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tempoAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> chordAttachment;

    juce::Rectangle<int> arpGroupBounds, shapeGroupBounds, visualBounds;

    // Visual + header (plan 11 4a/R13).
    ChordGridVisualizer visualizer { processor };
    std::unique_ptr<SuitePresets::PresetBar> presetBar;
    SuiteUI::HeaderMenuButton gearButton { juce::Colour (0xff4ECDC4) };

    void timerCallback() override { visualizer.repaint(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiEffectEditor)
};
