#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

#include "PluginProcessor.h"
#include "SuiteHeaderMenu.h"
#include "SuiteHelp.h"
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

    /** The chord/arp visual (plan 11 4a, D6): an 8-step strip with the clock's current
        step outlined and sounding steps filled, and — plan 13 — the suite's playable
        keyboard strip below it, so the plugin can be played from its own window. Reads
        the processor's published snapshot on the 60 Hz timer; no locks. A click on the
        strip replays the current chord through the processor's UI-note queue. */
    class ChordGridVisualizer : public juce::Component
    {
    public:
        explicit ChordGridVisualizer (MidiEffectProcessor& p)
            : processor (p)
        {
            addAndMakeVisible (keyboard);
        }

        void paint (juce::Graphics&) override;
        void resized() override;

        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;

        /** The editor sets these; a chord click plays the key through the UI-note queue. */
        std::function<void (int key)> onChordDown;
        std::function<void (int key)> onChordUp;

        /** The chord click's key while the mouse holds it (-1 when none). The editor's
            ending-path flushes read it. */
        int heldChordKey = -1;

        /** The playable keyboard (plan 13), a child of the visual band. */
        SuiteUI::PlayableKeyboardStrip keyboard;

    private:
        MidiEffectProcessor& processor;
    };

    /** Test seam (plan 15): the live affinity state of the arp-only cells. */
    bool isArpRowEnabled() const noexcept { return rateCell.isEnabled(); }

private:
    /** Every UI note the editor started gets its note-off: mouse ending paths handle the
        immediate cases; focus loss and editor destruction flush the rest (plan 13). */
    void releaseUiHeld();

    void focusLost (FocusChangeType) override;

    MidiEffectProcessor& processor;

    // Declared first so it is destroyed last, after every component that uses it.
    SuiteUI::SuiteLookAndFeel lookAndFeel { juce::Colour (0xff4ECDC4) };
    SuiteUI::HelpTracker helpTracker;
    SuiteUI::HelpLine helpLine;

    // SHAPE group: the key, the chord and the output level. The owner's plan-12 rule:
    // Scale, Pattern and Vel Curve join Chord as dropdowns.
    SuiteUI::LabelledSlider rootCtrl     { "ROOT",     juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider octaveCtrl   { "OCTAVE",   juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider velocityCtrl { "VELOCITY", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider spreadCtrl   { "SPREAD",   juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledDropdown scaleCell { "SCALE" };
    SuiteUI::LabelledDropdown chordCell { "CHORD" };
    SuiteUI::LabelledDropdown arpCell   { "PATTERN" };

    // ARP group: the step clock and the per-step velocity shape. Step Rate joins the
    // dropdowns; Mode, Sync and Vel Dir stay pills (two options each).
    SuiteUI::LabelledSlider tempoCtrl { "TEMPO", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider gateCtrl  { "GATE",  juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSelector modeCell  { "MODE", juce::Colour (0xff4ECDC4) };
    SuiteUI::LabelledDropdown rateCell  { "STEP RATE" };
    SuiteUI::LabelledSelector syncCell  { "SYNC", juce::Colour (0xff4ECDC4) };
    SuiteUI::LabelledDropdown curveCell { "VEL CURVE" };
    SuiteUI::LabelledSelector dirCell   { "VEL DIR", juce::Colour (0xff4ECDC4) };

    // The pill selectors' parameter attachments (declared after the cells, so they are
    // destroyed first — plan 11 D1, wired through SuiteUI::attachSelector / attachDropdown).
    std::unique_ptr<juce::ParameterAttachment> scaleAttachment;
    std::unique_ptr<juce::ParameterAttachment> arpAttachment;
    std::unique_ptr<juce::ParameterAttachment> modeAttachment;
    std::unique_ptr<juce::ParameterAttachment> rateAttachment;
    std::unique_ptr<juce::ParameterAttachment> syncAttachment;
    std::unique_ptr<juce::ParameterAttachment> curveCellAttachment;
    std::unique_ptr<juce::ParameterAttachment> dirCellAttachment;
    std::unique_ptr<juce::ParameterAttachment> chordAttachment;

    // Built in the constructor once the widgets exist, so their first update selects
    // the parameter's current choice.
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rootAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> octaveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> velocityAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> spreadAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tempoAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gateAttachment;

    juce::Rectangle<int> arpGroupBounds, shapeGroupBounds, visualBounds;

    // Visual + header (plan 11 4a/R13).
    ChordGridVisualizer visualizer { processor };
    std::unique_ptr<SuitePresets::PresetBar> presetBar;
    SuiteUI::HeaderMenuButton headerMenu { juce::Colour (0xff4ECDC4) };

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiEffectEditor)
};
