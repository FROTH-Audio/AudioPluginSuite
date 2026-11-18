#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <vector>
#include <array>
#include <cstdint>

#include "SuiteState.h"

/**
    MIDI Chord/Arp/Scale Tool - re-harmonises incoming MIDI notes.

    Processes incoming MIDI notes and generates:
    - Chords from a single note
    - Arpeggios with custom patterns
    - Scale quantisation

    Parameters are handled via the shared suite_modulation router.
    UI reuses suite_ui widgets (pop-up scales, knob-based pattern editor).
*/
class MidiEffectProcessor : public juce::AudioProcessor
{
public:
    // Parameter IDs are saved in sessions. Never rename or reuse one after a release.
    static constexpr const char* rootId   = "root";
    static constexpr const char* scaleId  = "scale";
    static constexpr const char* chordId  = "chord";
    static constexpr const char* arpId    = "arp";
    static constexpr const char* velocityId = "velocity";

    MidiEffectProcessor();
    ~MidiEffectProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::MidiBuffer& midiMessagesIn, juce::AudioBuffer<float>* /*buffer*/) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MidiEffect"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Scale degrees available
    enum class ScaleDegree { Major, Minor, Dorian, Phrygian, Lydian, Mixolydian, Aeolian, Whole tone, Chromatic };
    enum class ChordQuality { Major, Minor, Dim, Aug, Sus2, Sus4, Minor7, Major7 };
    enum class ArpPattern { Up, Down, Alternate, Random };

    // Get the current scale degree
    ScaleDegree getScale() const { return scale; }
    // Get the current chord quality
    ChordQuality getChord() const { return chord; }
    // Get the current arpeggio pattern
    ArpPattern getArp() const { return arp; }
    // Get the root note (MIDI number)
    int getRoot() const { return root; }
    // Get the current velocity
    int getVelocity() const { return velocity; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Current settings
    ScaleDegree scale {ScaleDegree::Minor};
    ChordQuality chord {ChordQuality::Major};
    ArpPattern arp {ArpPattern::Up};
    int root {48};      // MIDI note number, default C4
    int velocity {100}; // 0-127

    // Parameter pointers
    std::atomic<int>* rootParam{nullptr};
    std::atomic<int>* scaleParam{nullptr};
    std::atomic<int>* chordParam{nullptr};
    std::atomic<int>* arpParam{nullptr};
    std::atomic<int>* velocityParam{nullptr};
};