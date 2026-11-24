#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <vector>

namespace SuiteUI
{
    /**
        Log-frequency spectrum view with two overlaid traces: the input (dim) and the output (bright).
        Feed it linear magnitudes from the FFT; it converts them to dB for display.
    */
    class SpectrumDisplay : public juce::Component
    {
    public:
        SpectrumDisplay (juce::Colour inputColour, juce::Colour outputColour);

        /**
            numBins: number of magnitude values per trace (frameSize / 2 + 1).
            binWidthHz: sampleRate / frameSize.
            magnitudeScale: multiplier that brings a full-scale sine near 0 dB.
        */
        void setSpectra (const float* inputMagnitudes, const float* outputMagnitudes,
                         int numBins, double binWidthHz, float magnitudeScale);

        /** When false, the display says it is waiting for audio instead of showing a flat trace. */
        void setSignalPresent (bool present);

        /** Text shown in place of the curve while there is no signal to draw. */
        void setWaitingMessage (const juce::String& message);

        void paint (juce::Graphics&) override;

    private:
        juce::Path makePath (const std::vector<float>& decibels, juce::Rectangle<float> area) const;

        juce::Colour inputColour, outputColour;
        std::vector<float> inputDb, outputDb;
        double binWidthHz = 1.0;
        bool signalPresent = false;
        juce::String waitingMessage = "Waiting for audio. Play or route a signal into the plugin.";
    };

    /** Phase-space plot: a fading trail of normalised (x, y) points in [-1, 1]. */
    class TrailPlot : public juce::Component
    {
    public:
        explicit TrailPlot (juce::Colour colour);

        /** Points are ordered oldest first, newest last. */
        void setTrail (std::vector<juce::Point<float>> points);

        void paint (juce::Graphics&) override;

    private:
        juce::Colour colour;
        std::vector<juce::Point<float>> trail;
    };

    /** Draws a transfer function over [-1, 1] and marks the current operating point. */
    class CurveView : public juce::Component
    {
    public:
        explicit CurveView (juce::Colour colour);

        /** curve maps input to output, both in [-1, 1]. markerInput is the current input level. */
        void setCurve (std::function<float (float)> curve, float markerInput);

        void paint (juce::Graphics&) override;

    private:
        juce::Colour colour;
        std::function<float (float)> curve;
        float markerInput = 0.0f;
    };

    /** Vertical meter for a linear level. Maps -60 dB .. 0 dB onto the full height. */
    class LevelMeter : public juce::Component
    {
    public:
        explicit LevelMeter (juce::Colour colour);

        void setLevel (float linearLevel);

        void paint (juce::Graphics&) override;

    private:
        juce::Colour colour;
        float level = 0.0f;
    };

    /** Stereo pair of vertical meters. Left channel on the left, right channel on the right;
        both use the full height of the container (plan 13). Maps -60 dB .. 0 dB. */
    class StereoLevelMeter : public juce::Component
    {
    public:
        StereoLevelMeter (juce::Colour leftColour, juce::Colour rightColour);

        void setLevels (float leftLinearLevel, float rightLinearLevel);

        void paint (juce::Graphics&) override;

    private:
        juce::Colour leftColour, rightColour;
        float leftLevel = 0.0f;
        float rightLevel = 0.0f;
    };

    /** A row of mutually exclusive segments, used as a switch for choice parameters. */
    class SegmentedSelector : public juce::Component
    {
    public:
        explicit SegmentedSelector (juce::Colour accentColour);

        void setOptions (const juce::StringArray& labels);

        /** Sets the label font height in pixels. The default, 13, is used by the other plugins. */
        void setFontHeight (float height);

        /** Updates the display without calling onChange. */
        void setSelectedIndex (int index);
        int getSelectedIndex() const noexcept { return selected; }

        /** Called on user clicks only. */
        std::function<void (int)> onChange;

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        juce::Colour accent;
        juce::StringArray options;
        int selected = 0;
        float fontHeight = 13.0f;
    };

    /** A caption above a SegmentedSelector pill (plan 11 D1): the suite's switch for a choice
        parameter with six or fewer options. The editors attach it to a choice parameter
        through SuiteUI::attachSelector (SuiteSelectorAttachment.h), which keeps this module
        free of the juce_audio_processors dependency. */
    class LabelledSelector : public juce::Component
    {
    public:
        LabelledSelector (const juce::String& caption, juce::Colour accentColour);

        juce::Label label;
        SegmentedSelector selector;

        void setOptions (const juce::StringArray& labels);

        /** Manual update without notifying (an attached parameter will override it). */
        void setSelectedIndex (int index) { selector.setSelectedIndex (index); }

        int getSelectedIndex() const noexcept { return selector.getSelectedIndex(); }

        void resized() override;
    };

    /** A caption above a juce::ComboBox: the suite's dropdown for a choice parameter with more
        than six options, or for long option text (plan 11 D1, plan 12). Editors attach it to a
        choice parameter through SuiteUI::attachDropdown (SuiteSelectorAttachment.h), which
        keeps this module free of the juce_audio_processors dependency. */
    class LabelledDropdown : public juce::Component
    {
    public:
        explicit LabelledDropdown (const juce::String& caption);

        juce::Label label;
        juce::ComboBox box;

        /** Fills the box with items numbered 1..n (the ComboBox convention). */
        void setOptions (const juce::StringArray& items);

        /** Zero-based selection; -1 when nothing is selected. Never notifies. */
        void setSelectedIndex (int index);
        int getSelectedIndex() const noexcept;

        void resized() override;
    };

    /** A slider with a caption above it. Rotary sliders get their value box below the knob. */
    class LabelledSlider : public juce::Component
    {
    public:
        LabelledSlider (const juce::String& caption, juce::Slider::SliderStyle style);

        juce::Slider slider;
        juce::Label label;

        void resized() override;
    };

    /** The one-line help strip at the bottom of every editor (plan 12). showFor() always
        writes something - the hovered component's help text, or the idle text - so the line
        can never keep a stale string from an earlier hover. That stale value was the owner's
        "same value from the first hover" bug in the per-plugin copies this replaces. */
    class HelpLine : public juce::Component
    {
    public:
        HelpLine();

        void setIdleText (const juce::String& text);
        juce::String getIdleText() const { return idleText; }

        /** Evaluated on every showFor() call; a false result shows the idle text. Editors
            use it for the shared settings' tooltips toggle. */
        void setEnablePredicate (std::function<bool()> predicate);

        /** Shows the help text of `component`; a null pointer, a component without help
            text, or a disabled predicate shows the idle text. */
        void showFor (const juce::Component* component);

        /** Convenience overload for the editors' mouse listeners. */
        void showFor (const juce::MouseEvent& event) { showFor (event.eventComponent); }

        /** Shows a fixed string (status messages and tests). */
        void setText (const juce::String& text);
        juce::String getText() const { return text; }

        void paint (juce::Graphics&) override;

    private:
        juce::String idleText;
        juce::String text;
        std::function<bool()> enablePredicate;
        bool showingIdle = true;
    };

    /** The suite's group frame: translucent panel fill, hairline border, 8 px radius and the
        group title in the accent colour at the top-left (plan 12). Editors call this instead
        of drawing the chrome by hand, so every group looks the same. */
    void drawGroupFrame (juce::Graphics&, juce::Rectangle<int> bounds, const juce::String& title,
                         juce::Colour accent);

    /** A playable piano keyboard strip, MIDI 24-96. Mouse down, drag (glissando) and up
        produce note callbacks; the editor wires them to the processor. Keys light while
        the mouse holds them and for the notes the host feeds through setActiveNotes
        (the voices the processor reports). The root key gets a marker.

        The note-off guarantee (plan 13): every note the strip starts is released on every
        ending path — mouse-up, a drag off the key, the pointer leaving the strip, and the
        strip being destroyed. Editors that own one also flush it on focus loss and before
        destruction through releaseHeldNotes(). */
    class PlayableKeyboardStrip : public juce::Component
    {
    public:
        PlayableKeyboardStrip();
        ~PlayableKeyboardStrip() override;

        /** The editor sets these; a null callback is allowed and ignored. */
        std::function<void (int note, float velocity)> onNoteOn;
        std::function<void (int note)> onNoteOff;

        /** Voice activity from the processor, indexed by MIDI note (0-127). */
        void setActiveNotes (const bool* notes128);

        /** Marks the root key. */
        void setRootKey (int note);

        /** Releases every note the mouse currently holds, with note-off callbacks. Idempotent. */
        void releaseHeldNotes();

        /** True while the pointer holds at least one key (tests). */
        bool isAnyNoteHeld() const noexcept;

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;

    private:
        int noteAt (juce::Point<float> position) const;

        std::array<bool, 128> active {};
        std::array<bool, 128> pressed {};
        int rootKey = 60;
        int dragNote = -1;
    };
}
