#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

#include "PluginProcessor.h"
#include "SuiteLookAndFeel.h"
#include "SuiteWidgets.h"
#include "SuiteHeaderMenu.h"

class MidiEffectEditor : public juce::AudioProcessorEditor
{
public:
    explicit MidiEffectEditor (MidiEffectProcessor&);
    ~MidiEffectEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    /** A caption above a combo box, in one grid cell. */
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

    MidiEffectProcessor& processor;

    // Declared first so it is destroyed last, after every component that uses it.
    SuiteUI::SuiteLookAndFeel lookAndFeel { juce::Colour (0xff4ECDC4) };

    // SHAPE group: the key, the chord and the output level.
    SuiteUI::LabelledSlider rootCtrl     { "ROOT",     juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider octaveCtrl   { "OCTAVE",   juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider velocityCtrl { "VELOCITY", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider spreadCtrl   { "SPREAD",   juce::Slider::RotaryVerticalDrag };
    ComboCell scaleCell { "SCALE" };
    ComboCell chordCell { "CHORD" };
    ComboCell arpCell   { "PATTERN" };

    // ARP group: the step clock and the per-step velocity shape.
    SuiteUI::LabelledSlider tempoCtrl { "TEMPO", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider gateCtrl  { "GATE",  juce::Slider::RotaryVerticalDrag };
    ComboCell modeCell { "MODE" };
    ComboCell rateCell { "STEP RATE" };
    ComboCell syncCell { "SYNC" };
    ComboCell curveCell { "VEL CURVE" };
    ComboCell dirCell   { "VEL DIR" };

    // Built in the constructor after the combos' items exist, so their first update selects
    // the parameter's current choice (an in-class attachment would attach to an empty combo).
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rootAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> octaveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> velocityAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> spreadAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tempoAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> chordAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> arpAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> syncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> curveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> dirAttachment;

    juce::Rectangle<int> arpGroupBounds, shapeGroupBounds;

    // Preset bar (Pro builds: visible, public build: height 0 so layout unchanged)
    std::unique_ptr<SuitePresets::PresetBar> presetBar;

    SuiteUI::HeaderMenuButton gearButton { juce::Colour (0xff4ECDC4) };

    /** Lays one group's 4x2 grid of cells inside its frame. */
    void layoutGroup (juce::Rectangle<int> group, std::initializer_list<juce::Component*> cells);
};