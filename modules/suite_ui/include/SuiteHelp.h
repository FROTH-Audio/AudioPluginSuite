#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

#include "SuiteWidgets.h"

namespace SuiteUI
{
    /** Forwards mouse moves from every child to the owner component, so a help line can
        follow the pointer. The owner adds it with addMouseListener (&tracker, true). */
    struct HelpTracker : public juce::MouseListener
    {
        std::function<void (const juce::MouseEvent&)> onMove;
        void mouseMove (const juce::MouseEvent& e) override { if (onMove) onMove (e); }
        void mouseEnter (const juce::MouseEvent& e) override { if (onMove) onMove (e); }
    };

    /** Connects a HelpTracker to a HelpLine: every mouse move over the owner or any of its
        children shows that component's help text, or the idle text (plan 12). The owner must
        keep both alive for its lifetime and removeMouseListener in its destructor. A settings
        toggle (tooltips on/off) is a HelpLine::setEnablePredicate, so every path through the
        line - including tests - honours it. */
    inline void attachHelpLine (juce::Component& owner, HelpLine& line, HelpTracker& tracker)
    {
        tracker.onMove = [&line] (const juce::MouseEvent& e) { line.showFor (e.eventComponent); };
        owner.addMouseListener (&tracker, true);
    }

    /** Writes one help text onto a labelled knob's slider and caption (and the wrapper),
        so a help line finds it whichever component the pointer is over. */
    inline void describe (LabelledSlider& knob, const juce::String& text)
    {
        knob.setHelpText (text);
        knob.slider.setHelpText (text);
        knob.label.setHelpText (text);
    }

    /** Writes one help text onto a labelled selector's caption and pill (and the wrapper). */
    inline void describe (LabelledSelector& cell, const juce::String& text)
    {
        cell.setHelpText (text);
        cell.label.setHelpText (text);
        cell.selector.setHelpText (text);
    }

    /** Writes one help text onto a labelled dropdown's caption and box (and the wrapper). */
    inline void describe (LabelledDropdown& cell, const juce::String& text)
    {
        cell.setHelpText (text);
        cell.label.setHelpText (text);
        cell.box.setHelpText (text);
    }

    /** Writes one help text onto a plain control (switch, selector, button). */
    inline void describe (juce::Component& control, const juce::String& text)
    {
        control.setHelpText (text);
    }
}
