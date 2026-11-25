#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>
#include <vector>

#include "LaneView.h"
#include "PluginProcessor.h"
#include "SuiteHeaderMenu.h"
#include "SuiteHelp.h"
#include "SuiteLookAndFeel.h"
#include "SuiteModulationControls.h"
#include "SuiteModulationPanel.h"
#include "SuiteSelectorAttachment.h"
#include "SuiteSettings.h"
#include "SuiteWidgets.h"
#include "SuitePresets.h"

class GrooveMutatorEditor : public juce::AudioProcessorEditor,
                            private juce::Timer
{
public:
    explicit GrooveMutatorEditor (GrooveMutatorProcessor&);

    ~GrooveMutatorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** The pages: GRID (0), LANES (1), MUTATE (2), GROOVE (3), MOD (4). The tests use it. */
    void setPage (int page);
    int getPage() const noexcept { return currentPage; }

    // The affinity seams (the tests read them; the timer re-evaluates them per tick).
    bool isPulsesEnabled (int track) const noexcept { return lanes[(size_t) track]->pulsesCtrl.isEnabled(); }
    bool isRotationEnabled (int track) const noexcept { return lanes[(size_t) track]->rotationCtrl.isEnabled(); }
    bool isTempoEnabled() const noexcept { return tempoCtrl.isEnabled(); }

private:
    void timerCallback() override;

    /** The affinity rule (plan 19 Phase 3, decision G1): Steps lanes dim Pulses and
        Rotation, Hash lanes dim Rotation (the maths ignores it), Euclid dims nothing;
        Tempo dims while Host Sync is on. Re-evaluated in the 60 Hz timer. */
    void updateAffinity();

    static SuiteModulation::ModulationPanel::Style modulationStyle();

    GrooveMutatorProcessor& processor;

    // Declared first so it is destroyed last, after every component that uses it.
    SuiteUI::SuiteLookAndFeel lookAndFeel { juce::Colour (0xffdcf014) };

    // R12: the factory preset bar, flat in the header; R13: the menu at its right.
    std::unique_ptr<SuitePresets::PresetBar> presetBar;
    SuiteUI::HeaderMenuButton headerMenu { juce::Colour (0xffdcf014) };

    // R14a: the shared help line in the footer band.
    SuiteUI::HelpTracker helpTracker;
    SuiteUI::HelpLine helpLine;

    /** One lane's widgets: the GRID strip (dots, note, velocity, level, on, division) and
        the LANES knobs (pattern, steps, pulses, rotation, accent, flam, ghost). The editor
        lays the members out per page; the attachments ride with the widgets. */
    struct LaneCard
    {
        LaneCard (GrooveMutatorProcessor& p, int trackIndex, juce::Colour accentColour);

        const int track;
        LaneView view;
        juce::Label name;
        SuiteUI::LabelledSlider noteCtrl { "NOTE", juce::Slider::RotaryVerticalDrag };
        SuiteUI::LabelledSlider velCtrl { "VELOCITY", juce::Slider::RotaryVerticalDrag };
        SuiteUI::LabelledSlider levelCtrl { "LEVEL", juce::Slider::RotaryVerticalDrag };
        juce::ToggleButton onToggle { "ON" };
        SuiteUI::LabelledDropdown divisionCtrl { "DIVISION" };

        SuiteUI::LabelledSelector patternCtrl { "PATTERN", juce::Colour (0xffdcf014) };
        SuiteUI::LabelledSlider stepsCtrl { "STEPS", juce::Slider::RotaryVerticalDrag };
        SuiteUI::LabelledSlider pulsesCtrl { "PULSES", juce::Slider::RotaryVerticalDrag };
        SuiteUI::LabelledSlider rotationCtrl { "ROTATION", juce::Slider::RotaryVerticalDrag };
        SuiteUI::LabelledSlider accentCtrl { "ACCENT", juce::Slider::RotaryVerticalDrag };
        SuiteUI::LabelledSlider flamCtrl { "FLAM", juce::Slider::RotaryVerticalDrag };
        SuiteUI::LabelledSlider ghostCtrl { "GHOST", juce::Slider::RotaryVerticalDrag };

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> noteAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> velAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
        std::unique_ptr<juce::ParameterAttachment> divisionAttachment;
        std::unique_ptr<juce::ParameterAttachment> patternAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> stepsAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pulsesAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rotationAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> accentAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> flamAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ghostAttachment;
    };

    std::array<std::unique_ptr<LaneCard>, GrooveMutatorProcessor::numTracks> lanes;

    // MUTATE page: the attractor trail (the MTC visual) and the chaos/fill group.
    SuiteUI::TrailPlot trailPlot { juce::Colour (0xffdcf014) };
    SuiteUI::LabelledSlider chaosCtrl { "CHAOS", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider mutateScaleCtrl { "MUTATE SCALE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider seedCtrl { "SEED", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSelector evolveRateCtrl { "EVOLVE RATE", juce::Colour (0xffdcf014) };
    juce::ToggleButton freezeToggle { "FREEZE" };
    SuiteUI::LabelledSlider fillProbCtrl { "FILL PROB", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSelector fillStyleCtrl { "FILL STYLE", juce::Colour (0xffdcf014) };
    SuiteUI::LabelledSelector fillBarsCtrl { "FILL LENGTH", juce::Colour (0xffdcf014) };

    // GROOVE page: the feel and the clock/trigger.
    SuiteUI::LabelledSlider swingCtrl { "SWING", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider humaniseTimeCtrl { "HUM TIME", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider humaniseVelCtrl { "HUM VEL", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSelector velCurveCtrl { "VEL CURVE", juce::Colour (0xffdcf014) };
    SuiteUI::LabelledSelector polyRatioCtrl { "POLY", juce::Colour (0xffdcf014) };
    juce::ToggleButton syncToggle { "SYNC" };
    SuiteUI::LabelledSlider tempoCtrl { "TEMPO", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSelector triggerModeCtrl { "TRIGGER", juce::Colour (0xffdcf014) };
    SuiteUI::LabelledSlider triggerKeyCtrl { "KEY", juce::Slider::RotaryVerticalDrag };

    // Attachments for the non-lane widgets.
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> chaosAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mutateScaleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> seedAttachment;
    std::unique_ptr<juce::ParameterAttachment> evolveRateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fillProbAttachment;
    std::unique_ptr<juce::ParameterAttachment> fillStyleAttachment;
    std::unique_ptr<juce::ParameterAttachment> fillBarsAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> swingAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> humaniseTimeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> humaniseVelAttachment;
    std::unique_ptr<juce::ParameterAttachment> velCurveAttachment;
    std::unique_ptr<juce::ParameterAttachment> polyRatioAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tempoAttachment;
    std::unique_ptr<juce::ParameterAttachment> triggerModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> triggerKeyAttachment;

    // The modulation page (plan 19 Phase 4): the shared panel and the live displays.
    SuiteUI::SegmentedSelector pageSelector { juce::Colour (0xffdcf014) };
    SuiteModulation::ModulationPanel modulationPanel { processor.getRouter(), modulationStyle() };
    std::unique_ptr<SuiteModulation::ControlDisplay> controls;

    // The group frames and the ruler strip, painted per page (R14b).
    juce::Rectangle<int> gridBounds, rulerBounds;
    std::array<juce::Rectangle<int>, GrooveMutatorProcessor::numTracks> laneBounds;   // LANES cards
    juce::Rectangle<int> trailBounds, chaosBounds, fillBounds;
    juce::Rectangle<int> grooveBounds, clockBounds, modBounds;

    int currentPage = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrooveMutatorEditor)
};
