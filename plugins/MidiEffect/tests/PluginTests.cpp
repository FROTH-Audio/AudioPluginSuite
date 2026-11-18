// Plugin-level tests for MIDI Chord/Arp/Scale Tool. Compiles the real processor and editor sources.

#include "PluginTestUtils.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    using namespace PluginTest;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    using Processor = MidiEffectProcessor;

    /** Collects the note numbers of the note-on (or note-off) messages in a buffer. */
    std::vector<int> collectNotes (const juce::MidiBuffer& buffer, bool noteOns)
    {
        std::vector<int> notes;
        for (const auto metadata : buffer)
        {
            const auto& message = metadata.getMessage();
            if (noteOns ? message.isNoteOn() : message.isNoteOff())
                notes.push_back (message.getNoteNumber());
        }
        return notes;
    }

    void containsAll (const std::vector<int>& notes, std::initializer_list<int> expected)
    {
        assert ((int) notes.size() == (int) expected.size());
        for (int note : expected)
            assert (std::find (notes.begin(), notes.end(), note) != notes.end());
    }

    void testParameters()
    {
        Processor processor;

        checkParameterIds (processor, processor.apvts,
            { Processor::rootId, Processor::scaleId, Processor::chordId,
              Processor::arpId, Processor::octaveId, Processor::velocityId }, 6);
        checkRange (processor.apvts, Processor::rootId,     24.0f, 84.0f, 60.0f);
        checkRange (processor.apvts, Processor::scaleId,    0.0f, 5.0f, 2.0f);
        checkRange (processor.apvts, Processor::chordId,    0.0f, 7.0f, 0.0f);
        checkRange (processor.apvts, Processor::arpId,      0.0f, 3.0f, 0.0f);
        checkRange (processor.apvts, Processor::octaveId,  -2.0f, 2.0f, 0.0f);
        checkRange (processor.apvts, Processor::velocityId, 0.0f, 150.0f, 100.0f);
        pass ("testParameters");
    }

    void testStateRoundTrip()
    {
        Processor processor;

        checkStateRoundTrip (processor, processor.apvts, Processor::rootId,     48.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::scaleId,    4.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::chordId,    6.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::arpId,      2.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::octaveId,  -1.0f);
        checkStateRoundTrip (processor, processor.apvts, Processor::velocityId, 75.0f);
        pass ("testStateRoundTrip");
    }

    /** Plan 06 idea 2.11 first step: a note routed through the chord generator produces the
        expected output note IDs, and the note-off releases the whole chord. */
    void testChordNoteIds()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        // Defaults: root 60 (C4), Minor scale, Major chord. Note 60 is in the scale.
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        midi.addEvent (juce::MidiMessage::noteOff (1, 60), 100);

        juce::AudioBuffer<float> audio (2, blockSize);
        audio.clear();
        processor.processBlock (audio, midi);

        containsAll (collectNotes (midi, true), { 60, 64, 67 });
        containsAll (collectNotes (midi, false), { 60, 64, 67 });
        pass ("testChordNoteIds");
    }

    /** The note is quantised to the scale before the chord is built on it. */
    void testScaleQuantisationMovesTheChordRoot()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        // Scale Major (1): note 61 ties to the root side, so the chord lands on 60.
        setParameter (processor.apvts, Processor::scaleId, 1.0f);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 61, 0.8f), 0);
        juce::AudioBuffer<float> audio (2, blockSize);
        audio.clear();
        processor.processBlock (audio, midi);
        containsAll (collectNotes (midi, true), { 60, 64, 67 });

        // Note 66 (F#) is out of the scale and sits nearer F (65) than G (67).
        midi.clear();
        midi.addEvent (juce::MidiMessage::noteOn (1, 66, 0.8f), 0);
        processor.processBlock (audio, midi);
        containsAll (collectNotes (midi, true), { 65, 69, 72 });

        // Release the held chord from the first part before the next check on the same key.
        midi.clear();
        midi.addEvent (juce::MidiMessage::noteOff (1, 61), 0);
        processor.processBlock (audio, midi);

        // Chromatic scale: no quantisation, the chord root is the note itself.
        setParameter (processor.apvts, Processor::scaleId, 0.0f);
        midi.clear();
        midi.addEvent (juce::MidiMessage::noteOn (1, 61, 0.8f), 0);
        processor.processBlock (audio, midi);
        containsAll (collectNotes (midi, true), { 61, 65, 68 });
        pass ("testScaleQuantisationMovesTheChordRoot");
    }

    /** Octave shifts the chord; velocity scales the note-on velocity (0-1 floats). */
    void testOctaveAndVelocity()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        setParameter (processor.apvts, Processor::octaveId, -1.0f);
        setParameter (processor.apvts, Processor::velocityId, 50.0f);   // half velocity

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        juce::AudioBuffer<float> audio (2, blockSize);
        audio.clear();
        processor.processBlock (audio, midi);

        containsAll (collectNotes (midi, true), { 48, 52, 55 });
        for (const auto metadata : midi)
            if (metadata.getMessage().isNoteOn())
            {
                // Velocities are 7-bit inside MidiMessage, so allow the quantisation step.
                assert (std::abs (metadata.getMessage().getFloatVelocity() - 0.4f) < 0.02f);
            }
        pass ("testOctaveAndVelocity");
    }

    /** A retrigger before note-off releases the previous chord first, so nothing hangs. */
    void testRetriggerReleasesPreviousChord()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 10);

        juce::AudioBuffer<float> audio (2, blockSize);
        audio.clear();
        processor.processBlock (audio, midi);

        // The second note-on emitted the first chord's note-offs, then its own note-ons.
        assert ((int) collectNotes (midi, false).size() == 3);
        assert ((int) collectNotes (midi, true).size() == 6);
        pass ("testRetriggerReleasesPreviousChord");
    }

    /** Audio passes through untouched (the stereo pass-through rule for MIDI effects). */
    void testAudioPassThrough()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();
        addSine (buffer, 0, sampleRate, 440.0, 0.5f, 0);
        addSine (buffer, 1, sampleRate, 880.0, 0.25f, 0);
        juce::AudioBuffer<float> expected (buffer);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        processor.processBlock (buffer, midi);

        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < blockSize; ++n)
                assert (buffer.getSample (ch, n) == expected.getSample (ch, n));
        pass ("testAudioPassThrough");
    }

    void testEditor()
    {
        Processor processor;
        checkEditor (processor);
        pass ("testEditor");
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::puts ("Running MIDI Chord/Arp/Scale Tool plugin tests...");

    testParameters();
    testStateRoundTrip();
    testChordNoteIds();
    testScaleQuantisationMovesTheChordRoot();
    testOctaveAndVelocity();
    testRetriggerReleasesPreviousChord();
    testAudioPassThrough();
    testEditor();

    std::puts ("All MIDI Chord/Arp/Scale Tool plugin tests passed.");
    return 0;
}
