#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <array>
#include <map>
#include <vector>

#include "SuitePresets.h"   // StateHolder interface for presets
#include "SuiteState.h"

/**
    MIDI Chord/Arp/Scale Tool (plan 06 idea 2.11, extended by plan 6.1).

    Audio passes through untouched (the stereo-bus rule that keeps DAW scanners happy, same as
    MicroTimingChaos). Incoming MIDI is re-harmonised:

    1. Each note-on is quantised to the selected scale (CoreDSP::SamplerMaps::quantizeToScale,
       with the plugin's Root as the key centre — the same six scales the rest of the suite uses).
    2. A chord is built on the quantised note (CoreDSP::Chords), shifted by the Octave parameter.
    3. The chord notes are ordered by the Arp pattern.

    Two modes (plan 6.1):
    - **Chord** (the default) is the instant-chord path the plugin has always had: every note of
      the ordered chord fires at the note-on's sample position, with velocity scaled by Velocity.
      A note-off releases the whole chord. This path must stay event-for-event identical to the
      version before plan 6.1.
    - **Arp** runs a step clock: the ordered chord notes cycle over a fixed 8-step sequence, one
      note per step, at the Step Rate. The clock follows the host position (Host Sync on) or the
      Tempo parameter (off); steps are always derived from the beat grid, never accumulated.
      Octave Spread lifts each pass through the chord tones by up to an octave, and the velocity
      of each step is shaped by the Velocity Curve. A note-off truncates the held notes.
*/
class MidiEffectProcessor : public juce::AudioProcessor, public SuitePresets::StateHolder
{
public:
    // Parameter IDs are saved in sessions. Never rename or reuse one after a release.
    static constexpr const char* rootId     = "root";
    static constexpr const char* scaleId    = "scale";
    static constexpr const char* chordId    = "chord";
    static constexpr const char* arpId      = "arp";
    static constexpr const char* octaveId   = "octave";
    static constexpr const char* velocityId = "velocity";

    // Plan 6.1, appended after the originals so no saved parameter index moves.
    static constexpr const char* arpModeId            = "arpMode";
    static constexpr const char* stepRateId           = "stepRate";
    static constexpr const char* hostSyncId           = "hostSync";
    static constexpr const char* tempoId              = "tempo";
    static constexpr const char* gateLengthId         = "gateLength";
    static constexpr const char* velocityCurveId      = "velocityCurve";
    static constexpr const char* velocityDirectionId  = "velocityDirection";
    static constexpr const char* octaveSpreadId       = "octaveSpread";

    /** The fixed length of the arpeggiator's pattern (the owner's decision, so there is no
        step-count parameter). The pattern cycles while the key is held. */
    static constexpr int arpSteps = 8;

    /** The visual snapshot (plan 11 D6/D7): the most recently triggered held key's notes,
        published once per block on the audio thread and read on the message thread. No
        locks. notes[i] < 0 marks an empty step; sounding[i] says that step's note is
        currently on; step is the index of the step the clock last fired (-1 none); others
        counts further keys whose notes are still sounding (drawn as markers only). */
    struct ArpSnapshot
    {
        std::atomic<int> key { -1 };
        std::atomic<int> step { 0 };
        std::atomic<int> notes[arpSteps] {};
        std::atomic<bool> sounding[arpSteps] {};
        std::atomic<int> others { 0 };
    };

    const ArpSnapshot& getArpSnapshot() const noexcept { return arpSnapshot; }

    /** Latency reported to the host (plan 6.1): fixed at 50 ms, matching MicroTimingChaos, so
        it never changes with rate or gate. No event is scheduled before its grid time, so the
        value is conservative; the chord path emits at the note-on's position, as it always has. */
    static constexpr double latencyMs = 50.0;

    juce::AudioProcessorValueTreeState apvts;

    // Bump stateVersion when the saved state changes, and add a migration from the old version.
    // Plan 6.1 is additive only: no ID moved, no range changed meaning, so the version stays 1
    // and an old state loads with the new parameters at their defaults.
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

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override;
    void getCurrentProgramStateInformation (juce::MemoryBlock&) override;
    void setCurrentProgramStateInformation (const void*, int) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Preset system: StateHolder interface
    juce::String getPresetPluginId() const override { return "MidiEffect"; }
    int getPresetStateVersion() const override { return stateVersion; }
    juce::AudioProcessorValueTreeState& getPresetStateTree() override { return apvts; }
    void applyPresetState (const juce::ValueTree&) override;

    // The preset identity (plan 09 1.4): which factory preset or user file the bar last
    // applied. Message thread. Persisted as the presetSelection state property, so it
    // survives the editor being reopened and rides in saved sessions.
    SuitePresets::PresetSelection getPresetSelection() const override { return lastPresetSelection; }
    void setPresetSelection (const SuitePresets::PresetSelection&) override;
    static constexpr const char* presetSelectionProperty { "presetSelection" };

    /** Note name with octave, the suite's "C4 (60)" parameter-text format. */
    static juce::String noteName (int midiNote);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // The chord currently sounding for each incoming key, so its note-off releases the lot.
    // Chord mode only: the Arp mode keeps its own state below.
    struct HeldChord
    {
        int count = 0;
        std::array<int, 4> notes {};
    };
    std::array<HeldChord, 128> heldChords {};

    // The arpeggiator's state per key (plan 6.1). A held key fires the next step of its
    // 8-step sequence every time the step clock ticks; the sounding notes are released
    // by their gate or when the key ends.
    struct ArpVoice
    {
        bool active = false;
        int channel = 1;
        float baseVelocity = 0.8f;          // the note-on's velocity, before the curve
        double startBeat = 0.0;             // beat position of the note-on
        std::int64_t nextStep = 0;          // absolute step index (cycling modulo arpSteps)
        std::array<int, arpSteps> sequence {};   // MIDI notes with the spread already applied
        std::vector<int> sounding;          // notes currently on for this key
    };
    std::array<ArpVoice, 128> arpVoices {};

    // Gate note-offs scheduled beyond the current block, keyed by absolute sample. The
    // mapped value carries the owning key, so a key release can remove its gate-offs.
    std::multimap<std::int64_t, std::pair<int, juce::MidiMessage>> pendingNoteOffs;

    // The step clock. In Host Sync mode the position follows the host's PPQ; otherwise it
    // advances by the sample count at the Tempo parameter's rate.
    double preparedSampleRate = 44100.0;
    double beatPosition = 0.0;
    std::int64_t streamSample = 0;
    bool wasRunning = false;

    // Host program list (embedded bank + SUITE_PRESET_BANK_AS_PROGRAMS). -1 means a state that is not a
    // factory program, for example a user preset.
    std::atomic<int> currentProgramIndex { 0 };

    // The preset identity (plan 09 1.4). Message thread only.
    SuitePresets::PresetSelection lastPresetSelection;

    std::atomic<float>* rootParam     = nullptr;
    std::atomic<float>* scaleParam    = nullptr;
    std::atomic<float>* chordParam    = nullptr;
    std::atomic<float>* arpParam      = nullptr;
    std::atomic<float>* octaveParam   = nullptr;
    std::atomic<float>* velocityParam = nullptr;
    std::atomic<float>* arpModeParam           = nullptr;
    std::atomic<float>* stepRateParam          = nullptr;
    std::atomic<float>* hostSyncParam          = nullptr;
    std::atomic<float>* tempoParam             = nullptr;
    std::atomic<float>* gateLengthParam        = nullptr;
    std::atomic<float>* velocityCurveParam     = nullptr;
    std::atomic<float>* velocityDirectionParam = nullptr;
    std::atomic<float>* octaveSpreadParam      = nullptr;

    // The Arp-mode clock, note lifecycle and gate scheduling, split out of processBlock so
    // the Chord path stays verbatim.
    void processArpBlock (juce::MidiBuffer& midi, juce::MidiBuffer& out, int numSamples);
    void releaseArpKey (int key, juce::MidiBuffer& out, int sample);
    void releaseAllArpKeys (juce::MidiBuffer& out, int sample);

    // The visual snapshot (plan 11 D7). Audio thread, once per block, no locks.
    int lastTriggeredKey = -1;
    ArpSnapshot arpSnapshot;
    void publishSnapshot();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiEffectProcessor)
};
