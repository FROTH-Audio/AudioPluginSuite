#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>
#include <vector>

#include "LoopWaveformView.h"
#include "PluginProcessor.h"
#include "SuiteHeaderMenu.h"
#include "SuiteModulationControls.h"
#include "SuiteModulationPanel.h"
#include "SuiteSelectorAttachment.h"
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

    /** The page switch: 0..3 = SLOT 1..4, 4 = MOD (plan 18 Phase 6; tests use it too). */
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
    /** Plan 18 §1.4 (owner answer): the region knobs apply live. The listener captures
        the value at drag start and retriggers the slot's voice on release when the knob
        actually moved (a no-op drag does not restart the loop). */
    struct RegionRetriggerListener : juce::Slider::Listener
    {
        RegionRetriggerListener (CyclicalEditor& e, int s) : editor (e), slot (s) {}

        void sliderValueChanged (juce::Slider*) override {}   // only the drag boundaries matter

        void sliderDragStarted (juce::Slider* slider) override
        {
            valueAtDragStart = slider->getValue();
        }

        void sliderDragEnded (juce::Slider* slider) override
        {
            if (slider->getValue() != valueAtDragStart)
                editor.processor.requestRegionRetrigger (slot);
        }

        CyclicalEditor& editor;
        int slot = 0;
        double valueAtDragStart = 0.0;
    };
    /** One slot's full page (plan 18 Phase 4): title, waveform, the playback rows, the
        creative controls (filter + cutoff/reso, attack/release, level) and the slot's own
        quantise/swing/trigger/pitch/crossfade/tempo/sync/pan — the former GLOBAL controls,
        working independently per slot. */
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
        // Plan 17 Phase 2 + plan 18 §1.1: the creative controls with cutoff and resonance.
        // Plan 20 Phase 2: the filter type is a LabelledDropdown (caption above, pill below)
        // instead of a full-cell ComboBox.
        SuiteUI::LabelledDropdown filter { "FILTER TYPE" };
        SuiteUI::LabelledSlider cutoff { "CUTOFF", juce::Slider::RotaryVerticalDrag };
        SuiteUI::LabelledSlider reso { "RESO", juce::Slider::RotaryVerticalDrag };
        SuiteUI::LabelledSlider attack { "ATTACK", juce::Slider::RotaryVerticalDrag };
        SuiteUI::LabelledSlider release { "RELEASE", juce::Slider::RotaryVerticalDrag };
        // Plan 20 (owner ask): the slot's Level and Pan moved to the editor-level mix row,
        // so they sit beside their meters on every slot page.
        // Plan 18 Phase 4: the slot's own transport and mix controls (the former GLOBALs).
        SuiteUI::LabelledSlider pitchCtrl { "PITCH", juce::Slider::RotaryVerticalDrag };
        SuiteUI::LabelledSlider swingCtrl { "SWING", juce::Slider::RotaryVerticalDrag };
        SuiteUI::LabelledSlider tempoCtrl { "TEMPO", juce::Slider::RotaryVerticalDrag };
        SuiteUI::LabelledSlider crossfadeCtrl { "CROSSFADE", juce::Slider::RotaryVerticalDrag };
        juce::ToggleButton syncToggle { "HOST SYNC" };
        juce::Label quantizeCaption, triggerCaption;
        SuiteUI::SegmentedSelector quantizeSelector { accentColour };
        SuiteUI::SegmentedSelector triggerSelector { accentColour };
        SuiteUI::SegmentedSelector direction { accentColour };
        juce::ToggleButton pingRand { "PING RAND" };
        juce::ToggleButton granular { "GRAIN" };
        juce::ToggleButton chop { "CHOP" };

        // Plan 20 Phase 2: framed groups drawn by drawGroupFrame, laid out below.
        struct Group { const char* title; juce::Rectangle<int> bounds; int page; };
        std::vector<Group> groups;

        /** The area inside a group frame, below its title band. */
        juce::Rectangle<int> innerOf (juce::Rectangle<int> group)
        {
            return group.withTrimmedTop (SuiteUI::groupTitleHeight + 4).reduced (10, 4);
        }

        void resized() override;
        void paint (juce::Graphics&) override;

        std::unique_ptr<juce::ParameterAttachment> filterAttachment;
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

    // Plan 20 (owner ask): the mix row under every slot page — each slot's Level and Pan
    // beside its own little level meter, and the master Output. The four slots' Level and
    // Pan live here (they left the slot cards), so the MOD-page display and the bindings
    // point at these widgets.
    std::array<SuiteUI::LabelledSlider, CyclicalProcessor::numSlots> slotLevels {
        SuiteUI::LabelledSlider { "LEVEL 1", juce::Slider::RotaryVerticalDrag },
        SuiteUI::LabelledSlider { "LEVEL 2", juce::Slider::RotaryVerticalDrag },
        SuiteUI::LabelledSlider { "LEVEL 3", juce::Slider::RotaryVerticalDrag },
        SuiteUI::LabelledSlider { "LEVEL 4", juce::Slider::RotaryVerticalDrag },
    };
    std::array<SuiteUI::LabelledSlider, CyclicalProcessor::numSlots> slotPans {
        SuiteUI::LabelledSlider { "PAN 1", juce::Slider::RotaryVerticalDrag },
        SuiteUI::LabelledSlider { "PAN 2", juce::Slider::RotaryVerticalDrag },
        SuiteUI::LabelledSlider { "PAN 3", juce::Slider::RotaryVerticalDrag },
        SuiteUI::LabelledSlider { "PAN 4", juce::Slider::RotaryVerticalDrag },
    };
    std::array<SuiteUI::LevelMeter, CyclicalProcessor::numSlots> slotMeters {
        SuiteUI::LevelMeter { juce::Colour (0xffFF7A45) },
        SuiteUI::LevelMeter { juce::Colour (0xffFF7A45) },
        SuiteUI::LevelMeter { juce::Colour (0xffFF7A45) },
        SuiteUI::LevelMeter { juce::Colour (0xffFF7A45) },
    };
    // Smoothed meter display: the per-block peak decays between timer ticks.
    std::array<float, CyclicalProcessor::numSlots> meterLevels { 0.0f, 0.0f, 0.0f, 0.0f };

    // The one global: master Output, shown on every slot page (plan 18 Phase 4).
    SuiteUI::LabelledSlider masterCtrl { "OUTPUT", juce::Slider::RotaryVerticalDrag };

    // The mix row's one group frame (MASTER), drawn in paint(). The four slots' Level,
    // meter and Pan share it with the Output.
    juce::Rectangle<int> mixGroupBounds;

    // Plan 18 Phase 6: the MOD page — the shared panel and the control display.
    SuiteModulation::ModulationPanel modulation { processor.getRouter(), modulationStyle() };
    std::unique_ptr<SuiteModulation::ControlDisplay> controls;
    juce::Rectangle<int> modulationGroupBounds;

    /** The LFO colours of the modulation panel (tangerine, the plugin's accent). */
    SuiteModulation::ModulationPanel::Style modulationStyle();

    // Attachments, built in the constructor once the widgets exist.
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::ParameterAttachment>> choiceAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;

    // The loop browser, kept alive as a member (deleting the chooser aborts the dialog).
    std::unique_ptr<juce::FileChooser> loopChooser;
    int chooserSlot = 0;

    // Plan 18 §1.4: one listener per region knob (start/length/slices/slice length/beats
    // x four slots); the listener retriggers the slot's voice when the drag changed the value.
    std::vector<std::unique_ptr<RegionRetriggerListener>> regionRetriggerListeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CyclicalEditor)
};
