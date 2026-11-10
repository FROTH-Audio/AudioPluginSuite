// Pro preset tests for Micro-Timing Chaos Sequencer. Compiles the real processor and editor
// sources, then checks the factory bank, the Lerp curve and MIDI output.

#include "PluginProcessor.h"
#include "PluginTestUtils.h"
#include "PresetTestUtils.h"

#include <map>

namespace
{
    using namespace PresetTest;
    using Processor = MicroTimingChaosProcessor;

    // A host playhead the test controls, so the transport can be stopped cleanly at the end.
    struct FakePlayHead : juce::AudioPlayHead
    {
        bool playing = false;
        double ppq = 0.0;
        double bpm = 120.0;

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setIsPlaying (playing);
            info.setPpqPosition (ppq);
            info.setBpm (bpm);
            return info;
        }
    };

    // An applied preset that carries no lerpCurve resets the curve to its default.
    // The INIT preset carries no lerpCurve attribute, so applying it restores the natural curve.
    void testMtcCurveRestored()
    {
        Processor processor;

        // Move the curve away from its default.
        processor.setLerpCurvePoint (1, { 0.25f, 0.5f });
        processor.setLerpCurvePoint (3, { 0.75f, 0.5f });

        const auto before = processor.getLerpCurve();
        assert (std::abs (before[1].y - 0.5f) < 1.0e-4f);
        assert (std::abs (before[3].y - 0.5f) < 1.0e-4f);

        // The INIT preset carries no lerpCurve attribute.
        const auto presets = presetsFor ("MicroTimingChaos");
        assert (! presets.empty());
        assert (presets.front().name == "INIT");

        processor.applyPresetState (presets.front().state);
        const auto after = processor.getLerpCurve();

        // Back to the natural default curve: y = log10(1 + 9x), sampled at x = 0, 0.25, 0.5, 0.75, 1.
        for (int i = 0; i < Processor::curvePointCount; ++i)
        {
            const auto expected = Processor::naturalCurvePoint (i);
            assert (std::abs (after[(size_t) i].x - expected.x) < 1.0e-4f);
            assert (std::abs (after[(size_t) i].y - expected.y) < 1.0e-4f);
        }

        pass ("mtcCurveRestored");
    }

    // Four bars with the internal clock, then a clean stop: every note-on must get a note-off.
    void testMidiSanity()
    {
        const auto presets = presetsFor ("MicroTimingChaos");
        assert (! presets.empty());

        juce::AudioBuffer<float> audio (2, blockSize);

        for (const auto& preset : presets)
        {
            Processor processor;
            processor.applyPresetState (preset.state);

            // Host sync is off in every factory preset (the bank audit enforces it). Turn it on here
            // so the test can stop the transport and flush every sounding note.
            PluginTest::setParameter (processor.apvts, Processor::syncId, 1.0f);
            PluginTest::setParameter (processor.apvts, Processor::bpmId, 120.0f);
            processor.prepareToPlay (sampleRate, blockSize);

            FakePlayHead playhead;
            playhead.playing = true;
            processor.setPlayHead (&playhead);

            std::map<int, int> sounding;
            int notesOn = 0;
            int notesOff = 0;

            auto runBlock = [&] (bool playing)
            {
                playhead.playing = playing;

                juce::MidiBuffer midi;
                audio.clear();
                processor.processBlock (audio, midi);

                assert (allFinite (audio));

                for (const auto meta : midi)
                {
                    const auto message = meta.getMessage();

                    assert (meta.samplePosition >= 0);
                    assert (meta.samplePosition < blockSize);

                    if (message.isNoteOn())
                    {
                        ++sounding[message.getNoteNumber()];
                        ++notesOn;
                    }
                    else if (message.isNoteOff())
                    {
                        const auto found = sounding.find (message.getNoteNumber());
                        assert (found != sounding.end());
                        assert (found->second > 0);

                        if (--found->second == 0)
                            sounding.erase (found);

                        ++notesOff;
                    }
                }
            };

            // Four bars at 120 BPM (16 beats, 8 s).
            const int blocks = (int) (8.0 * sampleRate / blockSize);

            for (int block = 0; block < blocks; ++block)
            {
                runBlock (true);
                playhead.ppq += (double) blockSize / sampleRate * (playhead.bpm / 60.0);
            }

            // Stop the transport and give the queued note-offs a block or two to flush.
            for (int block = 0; block < 100; ++block)
                runBlock (false);

            processor.setPlayHead (nullptr);

            assert (notesOn > 0);
            assert (notesOn == notesOff);
            assert (sounding.empty());
            assert (processor.getLatencySamples()
                    == (int) std::lround (50.0 * sampleRate / 1000.0));
        }

        pass ("midiSanity");
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::puts ("Running Micro-Timing Chaos Pro preset tests...");

    using namespace PresetTest;

    runBankShape<Processor> ("MicroTimingChaos");

    {
        Processor processor;
        runBankRanges (processor);
        runBankNotDefault (processor);
        runApplyMatchesFile (processor);
        runApplyResetsMissing (processor);
        runApplyTwiceIdempotent (processor);
    }

    {
        Processor processor;
        runEditorWithBar (processor);
    }

    {
        Processor processor;
        PresetTest::runProgramList (processor);
    }

    runMenuModel ("MicroTimingChaos");

    testMtcCurveRestored();
    testMidiSanity();

    std::puts ("All Micro-Timing Chaos Pro preset tests passed.");
    return 0;
}
