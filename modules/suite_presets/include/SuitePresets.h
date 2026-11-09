#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace SuitePresets
{
    /** Implemented by each processor. The one place preset state enters and leaves a plugin. */
    class StateHolder
    {
    public:
        virtual ~StateHolder() = default;
        virtual juce::String getPresetPluginId() const = 0;                 // "SpectralDrift", ...
        virtual juce::AudioProcessorValueTreeState& getPresetStateTree() = 0;
        virtual void applyPresetState (const juce::ValueTree&) = 0;         // message thread only
    };

    /** True only in a Pro build. Lets tests and editors ask without #if. */
    inline bool isAvailable() noexcept { return true; }

    /**
        Preset header bar. In a public build it is invisible and reports height 0,
        so editor layout is unchanged.
    */
    class PresetBar : public juce::Component
    {
    public:
        PresetBar (StateHolder& holder, juce::Colour accent);
        ~PresetBar() override;

        int getPreferredHeight() const noexcept;      // 0 public, 30 Pro
        void paint (juce::Graphics&) override;
        void resized() override;

        /** Called on the message thread after a preset has been applied. */
        std::function<void()> onPresetLoaded;

        /** Message thread. Used by the host-program bridge and tests. */
        void selectPreset (int factoryIndex);
        int  getNumFactoryPresets() const noexcept { return 0; }  // Pro returns real count

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
}