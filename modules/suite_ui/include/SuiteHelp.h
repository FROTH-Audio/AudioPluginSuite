#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace SuiteUI
{
    /** Forwards mouse moves from every child to the owner component, so a help line can
        follow the pointer. The owner adds it with addMouseListener (&tracker, true) and
        sets `onMove` (plan 10 phase 5: the fourth editor to use this pattern - the first
        three carry local copies, which can migrate here later). */
    struct HelpTracker : public juce::MouseListener
    {
        std::function<void (const juce::MouseEvent&)> onMove;
        void mouseMove (const juce::MouseEvent& e) override { if (onMove) onMove (e); }
        void mouseEnter (const juce::MouseEvent& e) override { if (onMove) onMove (e); }
    };

    /** Writes one help text onto a labelled knob's slider and caption (and the wrapper),
        so a help line finds it whichever component the pointer is over. */
    inline void describe (LabelledSlider& knob, const juce::String& text)
    {
        knob.setHelpText (text);
        knob.slider.setHelpText (text);
        knob.label.setHelpText (text);
    }

    /** Writes one help text onto a plain control (switch, selector, button). */
    inline void describe (juce::Component& control, const juce::String& text)
    {
        control.setHelpText (text);
    }
}
