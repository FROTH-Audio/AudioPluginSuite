#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

namespace SuitePresets
{
    /** The state that preset operations act on. Each processor implements this once. */
    class StateHolder
    {
    public:
        virtual ~StateHolder() = default;
        
        /** Plugin identifier, e.g. "SpectralDrift", "DynamicSaturation", "MicroTimingChaos" */
        virtual juce::String getPresetPluginId() const = 0;
        
        /** The APVTS tree that preset operations modify */
        virtual juce::AudioProcessorValueTreeState& getPresetStateTree() = 0;
        
        /** Apply a preset's parameter values. Only on the message thread. */
        virtual void applyPresetState(const juce::ValueTree&) = 0;
    };

    /** True only in a Pro build. Lets tests and editors ask without #if. */
    inline bool isAvailable() noexcept { return true; }

    /**
        Preset header bar. In a public build it is invisible (height 0),
        so editor layout is unchanged.
    */
    class PresetBar : public juce::Component
    {
    public:
        PresetBar(StateHolder&, juce::Colour accent);
        ~PresetBar() override;

        int getPreferredHeight() const noexcept;  // 0 public, 30 Pro
        void paint(juce::Graphics&) override;
        void resized() override;

        /** Called on the message thread after a preset has been applied. */
        std::function<void()> onPresetLoaded;

        /** Message thread. Used by the host-program bridge and tests. */
        void selectPreset(int factoryIndex);
        int getNumFactoryPresets() const noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };

}
