#include "PluginProcessor.h"
#include "PluginEditor.h"

MidiEffectProcessor::MidiEffectProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput ("MIDI", juce::AudioChannelSet::mono(), true),  // MIDI input
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout MidiEffectProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Root note: MIDI note number 24-84 (C-2 to G8)
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { rootId, 1 }, "Root",
        juce::StringArray { "C-2", "C-1", "G-1", "A-1", "B-1", "C0", "C0#", "D0", "D0#", "E0", "F0", "F0#", "G0", "G0#", "A0", "A0#", "B0", "C1", "C1#", "D1", "D1#", "E1", "F1", "F1#", "G1", "G1#", "A1", "A1#", "B1", "C2", "C2#", "D2", "D2#", "E2", "F2", "F2#", "G2", "G2#", "A2", "A2#", "B2", "C3", "C3#", "D3", "D3#", "E3", "F3", "F3#", "G3", "G3#", "A3", "A3#", "B3", "C4", "C4#", "D4", "D4#", "E4", "F4", "F4#", "G4", "G4#", "A4", "A4#", "B4", "C5", "C5#", "D5", "D5#", "E5", "F5", "F5#", "G5", "G5#", "A5", "A5#", "B5", "C6", "C6#", "D6", "D6#", "E6", "F6", "F6#", "G6", "G6#", "A6", "A6#", "B6", "C7", "C7#", "D7", "D7#", "E7", "F7", "F7#", "G7", "G7#", "A7", "A7#", "B7", "C8" },
        60));

    // Scale: choice of scale
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { scaleId, 1 }, "Scale",
        juce::StringArray { "Major", "Minor", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Aeolian", "Whole tone", "Chromatic" },
        1)); // Minor

    // Chord quality
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { chordId, 1 }, "Chord quality",
        juce::StringArray { "Major", "Minor", "Dim", "Aug", "Sus2", "Sus4", "Minor7", "Major7" },
        0));

    // Arpeggio pattern
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { arpId, 1 }, "Arp pattern",
        juce::StringArray { "Up", "Down", "Alternate", "Random" },
        0));

    // Velocity
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { velocityId, 1 }, "Velocity",
        juce::StringArray { "Low", "Medium", "High" },
        1)); // Medium

    return { params.begin(), params.end() };
}

void MidiEffectProcessor::prepareToPlay (double, int)
{
}

bool MidiEffectProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // MIDI effect: needs MIDI input, stereo output is optional
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::mono();
}

void MidiEffectProcessor::processBlock (juce::MidiBuffer& midiMessagesIn, juce::AudioBuffer<float>* /*buffer*/)
{
    // Process incoming MIDI messages and generate chord/arp/scale output
    // For now, just pass through with some basic processing

    // Read current parameters
    const int root = rootParam ? juce::jlimit (24, 84, *rootParam) : 60;
    const int scaleDeg = scaleParam ? *scaleParam : 1; // index into scale enum
    const int chordQual = chordParam ? *chordParam : 0; // index into chord quality
    const int arpPat = arpParam ? *arpParam : 0; // index into arp pattern
    const int vel = velocityParam ? juce::jlimit (0, 2, *velocityParam) : 1; // index into velocity choice

    // Process all MIDI messages in the buffer
    for (const auto msg : midiMessagesIn)
    {
        // Get the note-on message
        if (msg.isNoteOn())
        {
            int noteNum = msg.getNoteNumber();
            // TODO: Implement chord generation, arpeggio, scale quantisation
            // For now, just pass the note through with velocity adjustment
            // This is a stub - full implementation would generate chord notes,
            // apply arpeggio pattern, and quantise to the selected scale
        }
    }

    // Reset the MIDI buffer - we'll output our generated notes
    midiMessagesIn.clear();
}

//==============================================================================
juce::AudioProcessorEditor* MidiEffectProcessor::createEditor()
{
    return new MidiEffectEditor (*this);
}

void MidiEffectProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    destData = SuiteState::write (apvts, 1);
}

void MidiEffectProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    SuiteState::read (data, sizeInBytes, apvts, 1, {});
}