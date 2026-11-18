#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "ChordEngine.h"
#include "SamplerMaps.h"

#if SUITE_PRESET_BANK_AS_PROGRAMS
 #include "PresetBank.h"
#endif

#include <algorithm>

MidiEffectProcessor::MidiEffectProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    rootParam     = apvts.getRawParameterValue (rootId);
    scaleParam    = apvts.getRawParameterValue (scaleId);
    chordParam    = apvts.getRawParameterValue (chordId);
    arpParam      = apvts.getRawParameterValue (arpId);
    octaveParam   = apvts.getRawParameterValue (octaveId);
    velocityParam = apvts.getRawParameterValue (velocityId);
    arpModeParam           = apvts.getRawParameterValue (arpModeId);
    stepRateParam          = apvts.getRawParameterValue (stepRateId);
    hostSyncParam          = apvts.getRawParameterValue (hostSyncId);
    tempoParam             = apvts.getRawParameterValue (tempoId);
    gateLengthParam        = apvts.getRawParameterValue (gateLengthId);
    velocityCurveParam     = apvts.getRawParameterValue (velocityCurveId);
    velocityDirectionParam = apvts.getRawParameterValue (velocityDirectionId);
    octaveSpreadParam      = apvts.getRawParameterValue (octaveSpreadId);
}

juce::String MidiEffectProcessor::noteName (int midiNote)
{
    static constexpr const char* names[12] = { "C", "C#", "D", "D#", "E", "F",
                                               "F#", "G", "G#", "A", "A#", "B" };
    midiNote = juce::jlimit (0, 127, midiNote);
    return names[midiNote % 12] + juce::String (midiNote / 12 - 1) + " (" + juce::String (midiNote) + ")";
}

juce::AudioProcessorValueTreeState::ParameterLayout MidiEffectProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Key centre for the scale quantisation, MIDI 24-84, shown as "C4 (60)".
    const auto rootToString = [] (float value, int)
    {
        return MidiEffectProcessor::noteName ((int) (value + 0.5f));
    };
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { rootId, 1 }, "Root Note", juce::NormalisableRange<float> (24.0f, 84.0f, 1.0f), 60.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (rootToString)));

    // Scale: the same six scales the rest of the suite uses (SamplerMaps order).
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { scaleId, 1 }, "Scale",
        juce::StringArray { "Chromatic", "Major", "Minor", "Minor Pentatonic", "Dorian", "Whole Tone" },
        2));   // Minor

    // Chord quality (CoreDSP::Chords::Quality order).
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { chordId, 1 }, "Chord Quality",
        juce::StringArray { "Major", "Minor", "Dim", "Aug", "Sus2", "Sus4", "Minor 7", "Major 7" },
        0));

    // Arpeggio pattern (CoreDSP::Chords::ArpPattern order). Scattered was appended in
    // plan 6.1, so saved indices 0-3 keep their meaning.
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { arpId, 1 }, "Arp Pattern",
        juce::StringArray { "Up", "Down", "Alternate", "Random", "Scattered" },
        0));

    // Octave transposition of the chord.
    params.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { octaveId, 1 }, "Octave", -2, 2, 0));

    // Velocity shaping, as a percentage of the incoming note's velocity.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { velocityId, 1 }, "Velocity",
        juce::NormalisableRange<float> (0.0f, 150.0f, 1.0f), 100.0f, "%"));

    //---- Plan 6.1: the arpeggiator, appended so no saved parameter index moves. The mode
    // defaults to Chord, which is the pre-plan-6.1 behaviour, so saved sessions and the
    // default output are unchanged.
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { arpModeId, 1 }, "Mode", juce::StringArray { "Chord", "Arp" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { stepRateId, 1 }, "Step Rate",
        juce::StringArray { "1/8", "1/8T", "1/16", "1/16T", "1/32" }, 2));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { hostSyncId, 1 }, "Host Sync", juce::StringArray { "Off", "On" }, 0));

    // Used while Host Sync is off, and disabled in meaning while it is on (the host tempo
    // is authoritative then, as in MicroTimingChaos).
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { tempoId, 1 }, "Tempo",
        juce::NormalisableRange<float> (40.0f, 240.0f, 0.1f), 120.0f, " BPM"));

    // Note length as a share of one step.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { gateLengthId, 1 }, "Gate Length",
        juce::NormalisableRange<float> (10.0f, 100.0f, 0.1f), 60.0f, "%"));

    // Per-step velocity shape in Arp mode. Flat keeps every step at the incoming velocity.
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { velocityCurveId, 1 }, "Velocity Curve",
        juce::StringArray { "Flat", "Linear", "Exponential", "Random" }, 0));

    // Applies to the Linear and Exponential curves.
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { velocityDirectionId, 1 }, "Velocity Direction",
        juce::StringArray { "Ascending", "Descending" }, 1));

    // Lifts each pass through the chord tones of the 8-step sequence by up to an octave.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { octaveSpreadId, 1 }, "Octave Spread",
        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f));

    return { params.begin(), params.end() };
}

void MidiEffectProcessor::prepareToPlay (double sampleRate, int)
{
    preparedSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    // Latency (plan 6.1): a fixed 50 ms, matching MicroTimingChaos. The value never changes
    // with rate or gate, so host delay compensation stays stable.
    setLatencySamples ((int) std::lround (latencyMs * preparedSampleRate / 1000.0));

    // A stopped transport or a state load can leave chord-mode chords sounding, so those are
    // not touched here. The arpeggiator's clock and its scheduled gate-offs, however, belong
    // to the running stream and start clean.
    arpVoices = {};
    pendingNoteOffs.clear();
    streamSample = 0;
    beatPosition = 0.0;
    wasRunning = false;
}

bool MidiEffectProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MidiEffectProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // Audio passes through untouched.
    (void) buffer;

    juce::MidiBuffer out;

    // Arp mode (plan 6.1): the step-clock path.
    if (arpModeParam->load() > 0.5f)
    {
        processArpBlock (midi, out, buffer.getNumSamples());
        midi.swapWith (out);
        return;
    }

    //---- Chord mode: the pre-plan-6.1 path, verbatim (compatibility requirement). ----

    // Block-level settings.
    const int rootKey    = (int) (rootParam->load() + 0.5f);
    const int scaleIndex = juce::jlimit (0, 5, (int) scaleParam->load());
    const auto quality   = (CoreDSP::Chords::Quality) juce::jlimit (0, (int) CoreDSP::Chords::Quality::count - 1,
                                                                   (int) chordParam->load());
    const auto arp       = (CoreDSP::Chords::ArpPattern) juce::jlimit (0, 4, (int) arpParam->load());
    const int octaveShift = 12 * juce::jlimit (-2, 2, (int) (octaveParam->load() + 0.0f));
    const float velocityScale = juce::jlimit (0.0f, 150.0f, velocityParam->load()) / 100.0f;

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        const int position = (int) metadata.samplePosition;

        if (message.isNoteOn())
        {
            const int key = message.getNoteNumber();

            // A retrigger before the previous note-off releases the previous chord first,
            // so a fast retrigger never hangs notes (the MicroTimingChaos rule).
            if (heldChords[(size_t) key].count > 0)
            {
                for (int i = 0; i < heldChords[(size_t) key].count; ++i)
                    out.addEvent (juce::MidiMessage::noteOff (message.getChannel(),
                                    heldChords[(size_t) key].notes[(size_t) i]), position);
                heldChords[(size_t) key] = {};
            }

            const int chordRoot = juce::jlimit (0, 127,
                CoreDSP::Sampler::quantizeToScale (key, rootKey, scaleIndex) + octaveShift);

            int ascending[4];
            CoreDSP::Chords::chordNotes (chordRoot, quality, ascending);
            const int count = CoreDSP::Chords::noteCount (quality);

            int ordered[4];
            CoreDSP::Chords::orderArp (arp, count, ascending, ordered, (std::uint32_t) key);

            auto& held = heldChords[(size_t) key];
            held.count = count;
            for (int i = 0; i < count; ++i)
            {
                const int note = ordered[i];
                held.notes[(size_t) i] = note;
                // MidiMessage velocities run 0-1 via getFloatVelocity; the Velocity
                // parameter scales that (getVelocity() is the raw 0-127 byte).
                const float velocity = juce::jlimit (0.0f, 1.0f, message.getFloatVelocity() * velocityScale);
                out.addEvent (juce::MidiMessage::noteOn (message.getChannel(), note, velocity), position);
            }
        }
        else if (message.isNoteOff())
        {
            const int key = message.getNoteNumber();
            if (heldChords[(size_t) key].count > 0)
            {
                for (int i = 0; i < heldChords[(size_t) key].count; ++i)
                    out.addEvent (juce::MidiMessage::noteOff (message.getChannel(),
                                    heldChords[(size_t) key].notes[(size_t) i]), position);
                heldChords[(size_t) key] = {};
            }
            // A note-off for a key the plugin never chorded is dropped: the original note
            // was quantised away, so there is nothing sounding to release.
        }
        else
        {
            // Everything else (clock, controllers, pitch bend) passes through.
            out.addEvent (message, position);
        }
    }

    midi.swapWith (out);
}

//==============================================================================
// Arp mode (plan 6.1): a step clock over the held keys' 8-step sequences.

void MidiEffectProcessor::processArpBlock (juce::MidiBuffer& midi, juce::MidiBuffer& out, int numSamples)
{
    const int rootKey    = (int) (rootParam->load() + 0.5f);
    const int scaleIndex = juce::jlimit (0, 5, (int) scaleParam->load());
    const auto quality   = (CoreDSP::Chords::Quality) juce::jlimit (0, (int) CoreDSP::Chords::Quality::count - 1,
                                                                   (int) chordParam->load());
    const auto arp       = (CoreDSP::Chords::ArpPattern) juce::jlimit (0, 4, (int) arpParam->load());
    const int octaveShift = 12 * juce::jlimit (-2, 2, (int) (octaveParam->load() + 0.0f));
    const float velocityScale = juce::jlimit (0.0f, 150.0f, velocityParam->load()) / 100.0f;

    const int rateIndex = juce::jlimit (0, 4, (int) (stepRateParam->load() + 0.5f));
    const double stepLength = CoreDSP::Chords::arpStepBeats (rateIndex);
    const float gatePercent = juce::jlimit (10.0f, 100.0f, gateLengthParam->load());
    const int curveIndex = juce::jlimit (0, 3, (int) (velocityCurveParam->load() + 0.5f));
    const bool ascending = (int) (velocityDirectionParam->load() + 0.5f) == 0;
    const float spread = juce::jlimit (-1.0f, 1.0f, octaveSpreadParam->load());

    const auto* playHead = getPlayHead();
    const auto position = playHead != nullptr ? playHead->getPosition()
                                              : juce::Optional<juce::AudioPlayHead::PositionInfo>();
    const bool followHost = hostSyncParam->load() > 0.5f && position.hasValue();
    const bool running = ! followHost || position->getIsPlaying();

    // Tempo: the host's when synced and reported, otherwise the Tempo parameter.
    double bpm = juce::jlimit (40.0, 240.0, (double) tempoParam->load());
    if (followHost)
        if (const auto hostBpm = position->getBpm(); hostBpm.hasValue() && *hostBpm > 1.0)
            bpm = juce::jlimit (20.0, 300.0, (double) *hostBpm);

    const double samplesPerBeat = 60.0 / bpm * preparedSampleRate;

    // Follow the host position first, so the block's beat span is the right one.
    if (followHost)
    {
        const auto ppq = position->getPpqPosition();

        if (ppq.hasValue() && (! wasRunning || std::abs (*ppq - beatPosition) > 1.0e-3))
            beatPosition = *ppq;
    }

    if (! running)
    {
        // A stopped transport releases everything the arp is holding, as in MicroTimingChaos.
        if (wasRunning)
            releaseAllArpKeys (out, 0);

        wasRunning = false;

        // Controllers and clocks still pass through while stopped.
        for (const auto metadata : midi)
            if (const auto message = metadata.getMessage(); ! message.isNoteOn() && ! message.isNoteOff())
                out.addEvent (message, (int) metadata.samplePosition);

        return;
    }

    wasRunning = true;

    const double blockStartBeat = beatPosition;
    const double blockEndBeat = blockStartBeat + numSamples / samplesPerBeat;
    const std::int64_t blockEndSample = streamSample + numSamples;

    // The gate-off drain, run at the top of the block and again after new gate-offs may
    // have been scheduled inside it.
    const auto drainDueNoteOffs = [&]()
    {
        for (auto it = pendingNoteOffs.begin(); it != pendingNoteOffs.end() && it->first < blockEndSample;)
        {
            const int offset = (int) std::max<std::int64_t> (0, it->first - streamSample);
            out.addEvent (it->second.second, offset);

            auto& voice = arpVoices[(size_t) it->second.first];
            voice.sounding.erase (std::remove (voice.sounding.begin(), voice.sounding.end(),
                                               it->second.second.getNoteNumber()),
                                  voice.sounding.end());

            it = pendingNoteOffs.erase (it);
        }
    };

    // 1. Gate note-offs due in this block fire at their scheduled sample.
    drainDueNoteOffs();

    // 2. Incoming MIDI. A note-on builds the key's sequence and starts it at step 0; a
    //    note-off truncates the key immediately.
    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        const int sample = (int) metadata.samplePosition;

        if (message.isNoteOn())
        {
            const int key = message.getNoteNumber();

            // A retrigger before the previous note-off cuts the previous chord's notes
            // first (the existing rule).
            if (arpVoices[(size_t) key].active)
                releaseArpKey (key, out, sample);

            const int chordRoot = juce::jlimit (0, 127,
                CoreDSP::Sampler::quantizeToScale (key, rootKey, scaleIndex) + octaveShift);

            int ascendingNotes[4];
            CoreDSP::Chords::chordNotes (chordRoot, quality, ascendingNotes);
            const int count = CoreDSP::Chords::noteCount (quality);

            int ordered[4];
            CoreDSP::Chords::orderArp (arp, count, ascendingNotes, ordered, (std::uint32_t) key);

            auto& voice = arpVoices[(size_t) key];
            voice.active = true;
            voice.channel = message.getChannel();
            voice.baseVelocity = message.getFloatVelocity();
            voice.startBeat = blockStartBeat + (double) sample / samplesPerBeat;
            voice.nextStep = 0;
            voice.sounding.clear();

            for (int s = 0; s < arpSteps; ++s)
            {
                const int lift = CoreDSP::Chords::arpOctaveLift (spread, s / count);
                voice.sequence[(size_t) s] = juce::jlimit (0, 127, ordered[(size_t) (s % count)] + lift);
            }
        }
        else if (message.isNoteOff())
        {
            const int key = message.getNoteNumber();
            if (arpVoices[(size_t) key].active)
                releaseArpKey (key, out, sample);
            // A note-off for a key the arp never started is dropped, as in Chord mode.
        }
        else
        {
            // Everything else (clock, controllers, pitch bend) passes through.
            out.addEvent (message, sample);
        }
    }

    // 3. Steps: every active key fires the steps of its sequence that fall in the block.
    //    Everything is derived from the beat grid (start beat + step index x step length),
    //    never accumulated, so a tempo change or a host relocate never drifts.
    const std::int64_t gateSamples = std::max<std::int64_t> (1,
        (std::int64_t) std::lround (gatePercent / 100.0f * stepLength * samplesPerBeat));

    for (int key = 0; key < 128; ++key)
    {
        auto& voice = arpVoices[(size_t) key];
        if (! voice.active)
            continue;

        // A relocated host position can leave many steps behind; skip them in one jump
        // instead of walking each one.
        if (voice.startBeat + (double) voice.nextStep * stepLength < blockStartBeat - 1.0e-9)
            voice.nextStep = (std::int64_t) std::ceil ((blockStartBeat - voice.startBeat) / stepLength - 1.0e-9);

        while (voice.startBeat + (double) voice.nextStep * stepLength < blockEndBeat)
        {
            const double stepBeat = voice.startBeat + (double) voice.nextStep * stepLength;

            if (stepBeat >= blockStartBeat - 1.0e-9)
            {
                const int stepInSequence = (int) (voice.nextStep % (std::int64_t) arpSteps);
                const int cycle = (int) (voice.nextStep / (std::int64_t) arpSteps);
                const int note = voice.sequence[(size_t) stepInSequence];
                int offset = (int) std::lround ((stepBeat - blockStartBeat) * samplesPerBeat);
                offset = juce::jlimit (0, juce::jmax (0, numSamples - 1), offset);

                // A retrigger of a note that is still sounding cuts the earlier copy first
                // (the MicroTimingChaos rule), so a step never hangs a note.
                if (std::find (voice.sounding.begin(), voice.sounding.end(), note) != voice.sounding.end())
                    out.addEvent (juce::MidiMessage::noteOff (voice.channel, note), offset);

                // finalVelocity = incoming velocity x Velocity x curve(step). Flat keeps the
                // velocity uniform; Random hashes (note, step, cycle) deterministically.
                float curveValue = 1.0f;

                if (curveIndex == 3)
                    curveValue = CoreDSP::Chords::arpRandomVelocity (note, stepInSequence, cycle);
                else if (curveIndex > 0)
                    curveValue = CoreDSP::Chords::arpCurveValue (curveIndex, ascending, stepInSequence, arpSteps);

                const float velocity = juce::jlimit (0.0f, 1.0f, voice.baseVelocity * velocityScale * curveValue);

                out.addEvent (juce::MidiMessage::noteOn (voice.channel, note, velocity), offset);
                voice.sounding.push_back (note);

                // The gate note-off lands note-on + gate; it may fall into a later block.
                pendingNoteOffs.emplace ((std::int64_t) streamSample + offset + gateSamples,
                                         std::make_pair (key,
                                             juce::MidiMessage::noteOff (voice.channel, note)));
            }

            ++voice.nextStep;
        }
    }

    // 4. Any gate-off the steps above scheduled inside this block fires now.
    drainDueNoteOffs();

    beatPosition = blockEndBeat;
    streamSample = blockEndSample;
}

void MidiEffectProcessor::releaseArpKey (int key, juce::MidiBuffer& out, int sample)
{
    auto& voice = arpVoices[(size_t) key];
    if (! voice.active)
        return;

    // Truncate: every sounding note of the key stops now, and its scheduled gate-offs
    // are removed so they do not fire afterwards.
    for (int note : voice.sounding)
        out.addEvent (juce::MidiMessage::noteOff (voice.channel, note), sample);

    for (auto it = pendingNoteOffs.begin(); it != pendingNoteOffs.end();)
    {
        if (it->second.first == key)
            it = pendingNoteOffs.erase (it);
        else
            ++it;
    }

    voice.sounding.clear();
    voice.active = false;
}

void MidiEffectProcessor::releaseAllArpKeys (juce::MidiBuffer& out, int sample)
{
    for (int key = 0; key < 128; ++key)
        releaseArpKey (key, out, sample);
}

juce::AudioProcessorEditor* MidiEffectProcessor::createEditor()
{
    return new MidiEffectEditor (*this);
}

void MidiEffectProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    destData = SuiteState::write (apvts, stateVersion);
}

void MidiEffectProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    SuiteState::read (data, sizeInBytes, apvts, stateVersion, migrations);

    // The preset identity rides as a property (plan 09 1.4, D3): restore the bar's
    // selection from the replaced state. A session without the property — every
    // session saved before plan 09 — reads as Init.
    lastPresetSelection = SuitePresets::decodePresetSelection (
        apvts.state.getProperty (presetSelectionProperty).toString());
}

void MidiEffectProcessor::setPresetSelection (const SuitePresets::PresetSelection& selection)
{
    lastPresetSelection = selection;

    // The lerpCurve / samplePath pattern (plan 09 1.4): keep the property in sync at
    // change time, so the identity rides in every saved session and host program dump.
    if (selection.kind == SuitePresets::PresetSelection::Kind::none)
        apvts.state.removeProperty (presetSelectionProperty, nullptr);
    else
        apvts.state.setProperty (presetSelectionProperty,
                                 SuitePresets::encodePresetSelection (selection), nullptr);
}

void MidiEffectProcessor::applyPresetState (const juce::ValueTree& tree)
{
    // Presets use resetMissing=true so absent parameters are set to default. The tree is
    // encoded into the binary state format first, so it goes through the same read path
    // as a saved session: version check, migrations, replaceState, resetMissing.
    const juce::MemoryBlock block = SuiteState::detail::encodeXml (*tree.createXml());
    const auto result = SuiteState::read (block.getData(), (int) block.getSize(), apvts, stateVersion,
                                          migrations, {}, true);

    // Applying any tree resets the preset identity (plan 09 1.4): the caller — the bar
    // or the host-program bridge — re-states it right after, when it knows the preset.
    if (result == SuiteState::LoadResult::loaded)
        lastPresetSelection = {};

#if SUITE_PRESET_BANK_AS_PROGRAMS
    // A preset applied from the bar or a file is no longer one of the host programs.
    if (result == SuiteState::LoadResult::loaded)
        currentProgramIndex.store (-1, std::memory_order_relaxed);
#else
    juce::ignoreUnused (result);
#endif
}

//==============================================================================
// Host program list. Program 0 is Init; 1 to N are the factory presets in bank order. The list only
// exists in a Pro build with SUITE_PRESET_BANK_AS_PROGRAMS on; otherwise the plugin reports one
// program, as it did before the preset system.

int MidiEffectProcessor::getNumPrograms()
{
#if SUITE_PRESET_BANK_AS_PROGRAMS
    return SuitePresets::PresetPrograms::getNumPrograms (getPresetPluginId());
#else
    return 1;
#endif
}

int MidiEffectProcessor::getCurrentProgram()
{
#if SUITE_PRESET_BANK_AS_PROGRAMS
    return currentProgramIndex.load (std::memory_order_relaxed);
#else
    return 0;
#endif
}

void MidiEffectProcessor::setCurrentProgram (int index)
{
#if SUITE_PRESET_BANK_AS_PROGRAMS
    if (SuitePresets::PresetPrograms::applyProgram (*this, index))
    {
        currentProgramIndex.store (index, std::memory_order_relaxed);

        // The host program and the bar's selection describe the same event (plan 09 1.4):
        // program 0 is Init, program N is factory preset N-1 in bank order.
        SuitePresets::PresetSelection selection;

        if (index > 0)
        {
            selection.kind = SuitePresets::PresetSelection::Kind::factory;
            selection.factoryIndex = index - 1;
        }

        setPresetSelection (selection);
    }
#else
    juce::ignoreUnused (index);
#endif
}

const juce::String MidiEffectProcessor::getProgramName (int index)
{
#if SUITE_PRESET_BANK_AS_PROGRAMS
    return SuitePresets::PresetPrograms::getProgramName (getPresetPluginId(), index);
#else
    juce::ignoreUnused (index);
    return {};
#endif
}

void MidiEffectProcessor::changeProgramName (int, const juce::String&)
{
    // The factory programs are read-only.
}

void MidiEffectProcessor::getCurrentProgramStateInformation (juce::MemoryBlock& destData)
{
    getStateInformation (destData);
}

void MidiEffectProcessor::setCurrentProgramStateInformation (const void* data, int sizeInBytes)
{
    setStateInformation (data, sizeInBytes);

#if SUITE_PRESET_BANK_AS_PROGRAMS
    currentProgramIndex.store (-1, std::memory_order_relaxed);
#endif
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MidiEffectProcessor();
}
