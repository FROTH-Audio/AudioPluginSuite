#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "ChordEngine.h"
#include "SamplerMaps.h"

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

    // Arpeggio pattern (CoreDSP::Chords::ArpPattern order).
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { arpId, 1 }, "Arp Pattern",
        juce::StringArray { "Up", "Down", "Alternate", "Random" },
        0));

    // Octave transposition of the chord.
    params.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { octaveId, 1 }, "Octave", -2, 2, 0));

    // Velocity shaping, as a percentage of the incoming note's velocity.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { velocityId, 1 }, "Velocity",
        juce::NormalisableRange<float> (0.0f, 150.0f, 1.0f), 100.0f, "%"));

    return { params.begin(), params.end() };
}

void MidiEffectProcessor::prepareToPlay (double, int)
{
    // A stopped transport or a state load can leave chords sounding; nothing to allocate.
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

    // Block-level settings.
    const int rootKey    = (int) (rootParam->load() + 0.5f);
    const int scaleIndex = juce::jlimit (0, 5, (int) scaleParam->load());
    const auto quality   = (CoreDSP::Chords::Quality) juce::jlimit (0, (int) CoreDSP::Chords::Quality::count - 1,
                                                                   (int) chordParam->load());
    const auto arp       = (CoreDSP::Chords::ArpPattern) juce::jlimit (0, 3, (int) arpParam->load());
    const int octaveShift = 12 * juce::jlimit (-2, 2, (int) (octaveParam->load() + 0.0f));
    const float velocityScale = juce::jlimit (0.0f, 150.0f, velocityParam->load()) / 100.0f;

    // Outgoing events, then swap the buffer in one go.
    juce::MidiBuffer out;

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
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MidiEffectProcessor();
}
