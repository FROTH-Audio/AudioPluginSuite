#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <array>
#include <vector>

#include "SuiteState.h"

/**
    MIDI Chord/Arp/Scale Tool (plan 06 idea 2.11).

    Audio passes through untouched (the stereo-bus rule that keeps DAW scanners happy, same as
    MicroTimingChaos). Incoming MIDI is re-harmonised:

    1. Each note-on is quantised to the selected scale (CoreDSP::SamplerMaps::quantizeToScale,
       with the plugin's Root as the key centre — the same six scales the rest of the suite uses).
    2. A chord is built on the quantised note (CoreDSP::Chords), shifted by the Octave parameter.
    3. The chord notes are ordered by the Arp pattern and emitted at the note-on's sample
       position, with velocity scaled by the Velocity parameter.
    4. The original note's note-off releases the whole chord, so nothing sticks.

    The Arp pattern currently orders the chord at note-on; a step-clock arpeggiator (which would
    spread the notes over time) is the planned follow-up, so no latency is reported.
*/
class MidiEffectProcessor : public juce::AudioProcessor
{
public:
    // Parameter IDs are saved in sessions. Never rename or reuse one after a release.
    static constexpr const char* rootId     = "root";
    static constexpr const char* scaleId    = "scale";
    static constexpr const char* chordId    = "chord";
    static constexpr const char* arpId      = "arp";
    static constexpr const char* octaveId   = "octave";
    static constexpr const char* velocityId = "velocity";

    juce::AudioProcessorValueTreeState apvts;

    // Bump stateVersion when the saved state changes, and add a migration from the old version.
    int stateVersion = 1;
    std::vector<SuiteState::Migration> migrations;

    MidiEffectProcessor();
    ~MidiEffectProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MIDI Chord/Arp/Scale Tool"; }
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

    /** Note name with octave, the suite's "C4 (60)" parameter-text format. */
    static juce::String noteName (int midiNote);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // The chord currently sounding for each incoming key, so its note-off releases the lot.
    struct HeldChord
    {
        int count = 0;
        std::array<int, 4> notes {};
    };
    std::array<HeldChord, 128> heldChords {};

    std::atomic<float>* rootParam     = nullptr;
    std::atomic<float>* scaleParam    = nullptr;
    std::atomic<float>* chordParam    = nullptr;
    std::atomic<float>* arpParam      = nullptr;
    std::atomic<float>* octaveParam   = nullptr;
    std::atomic<float>* velocityParam = nullptr;
};
