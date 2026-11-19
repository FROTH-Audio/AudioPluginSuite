#pragma once

// Wiring for SuiteUI::LabelledSelector and APVTS choice parameters (plan 11 D1).
//
// This lives in its own header, not in SuiteWidgets, because it pulls in
// juce_audio_processors and SuiteUI must stay free of that dependency: SuiteSettingsTests
// compiles the SuiteWidgets sources but links only juce_gui_basics. Only editors (which
// link the plugin module set anyway) include this file.

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

#include "SuiteWidgets.h"

namespace SuiteUI
{
    /** Wires a LabelledSelector to a choice parameter: the denormalised value of a choice
        parameter is its 0-based index. Clicks become complete gestures; automation and
        sendInitialUpdate update the display. The caller owns the attachment — declare the
        unique_ptr as a member next to the selector cell. */
    inline void attachSelector (LabelledSelector& cell, juce::RangedAudioParameter& parameter,
                                std::unique_ptr<juce::ParameterAttachment>& storage)
    {
        storage = std::make_unique<juce::ParameterAttachment> (
            parameter,
            [&cell] (float value) { cell.setSelectedIndex (juce::roundToInt (value)); });

        // The raw pointer is safe: the editor owns both members for the editor's lifetime,
        // and the callback only runs on the message thread while the editor is alive.
        juce::ParameterAttachment* attached = storage.get();

        cell.selector.onChange = [attached, &cell] (int)
        {
            attached->setValueAsCompleteGesture ((float) cell.getSelectedIndex());
        };

        storage->sendInitialUpdate();
    }
}
