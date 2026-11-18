// Plugin-level tests for Sgraffito. Compiles the real processor and editor sources.

#include "PluginTestUtils.h"
#include "StateFixtures.h"
#include "ModulationTestUtils.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <iostream>

namespace
{
    using namespace PluginTest;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    using Processor = SgraffitoProcessor;

    void testParameters()
    {
        Processor processor;

        // 71 parameters total (as defined in numControls).
        checkParameterIds (processor, processor.apvts,
                           { Processor::modeId, Processor::levelId, Processor::driftAmountId,
                             Processor::lfo1RateId, Processor::lfo1ShapeId, Processor::lfo1SyncId,
                             Processor::lfo1DivisionId, Processor::lfo1OffsetId,
                             Processor::lfo2RateId, Processor::lfo2ShapeId, Processor::lfo2SyncId,
                             Processor::lfo2DivisionId, Processor::lfo2OffsetId,
                             Processor::modSourceIds[0], Processor::modSourceIds[5],
                             Processor::modTargetIds[0], Processor::modTargetIds[5],
                             Processor::modAmountIds[0], Processor::modAmountIds[5],
                             Processor::modDelayIds[0], Processor::modDelayIds[5] },
                           Processor::numControls);
        checkRange (processor.apvts, Processor::modeId, 0.0f, 3.0f, 0.0f);
        checkRange (processor.apvts, Processor::levelId, -60.0f, 6.0f, 0.0f);
        checkRange (processor.apvts, Processor::driftAmountId, 0.0f, 100.0f, 0.0f);
        checkRange (processor.apvts, Processor::memoryAmountId, 0.0f, 100.0f, 20.0f);
        checkRange (processor.apvts, Processor::grainSpreadId, 0.0f, 100.0f, 40.0f);

        // Root Key reads as a note name and octave, like the Micro-Timing Chaos root note.
        setParameter (processor.apvts, Processor::rootKeyId, 48.0f);
        assert (processor.apvts.getParameter (Processor::rootKeyId)->getCurrentValueAsText() == "C2 (48)");

        pass ("testParameters");
    }

    void testStateRoundTrip()
    {
        Processor processor;

        checkStateRoundTrip (processor, processor.apvts, Processor::levelId, 6.0f);
        pass ("testStateRoundTrip");
    }

    void testMidiNoteOnAllocatesVoice()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        // Set up a minimal sample buffer so MIDI processing can occur
        juce::AudioBuffer<float> dummyBuffer (2, 48000);
        processor.loadedSample = std::make_unique<LoadedSample>();
        processor.loadedSample->buffer = dummyBuffer;
        processor.loadedSample->sourceSampleRate = sampleRate;
        processor.sampleBuffer.store (&processor.loadedSample->buffer, std::memory_order_release);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);

        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();
        
        processor.processBlock (buffer, midi);

        // Debug: print voice state directly
        std::cerr << "DEBUG testMidiNoteOnAllocatesVoice: checking processor=&" << &processor 
                  << ", countActive()=" << processor.voicePool.countActive() << std::endl;
        for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i) {
            if (processor.voicePool.voices[i].active) {
                std::cerr << "DEBUG testMidiNoteOnAllocatesVoice: Voice " << i << " is active" << std::endl;
            }
        }

        assert (processor.voicePool.countActive() > 0);
        pass ("testMidiNoteOnAllocatesVoice");
    }

    void testMidiNoteOffReleasesVoice()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::MidiBuffer midiOn;
        midiOn.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);

        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();
        processor.processBlock (buffer, midiOn);
        assert (processor.voicePool.countActive() > 0);

        // Set tape stop time to 0 so note-off releases without a tape spin-down.
        setParameter (processor.apvts, Processor::tapeStopTimeId, 0.0f);

        juce::MidiBuffer midiOff;
        midiOff.addEvent (juce::MidiMessage::noteOff (1, 60, 0.8f), 0);
        processor.processBlock (buffer, midiOff);

        // A released voice keeps playing its tail until its region has played out.
        juce::MidiBuffer noMidi;

        for (int block = 0; block < 40 && processor.voicePool.countActive() > 0; ++block)
        {
            buffer.clear();
            processor.processBlock (buffer, noMidi);
        }

        assert (processor.voicePool.countActive() == 0);
        pass ("testMidiNoteOffReleasesVoice");
    }

    // Swing (plan 05.4): the first note of a key plays straight, the next one is swung and
    // starts half a beat later (swing 75 % at the fallback tempo of 120 BPM = 12000 samples).
    void testSwingDelaysRepeatedNotes()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameter (processor.apvts, Processor::swingId, 75.0f);
        setParameter (processor.apvts, Processor::humaniseId, 0.0f);
        setParameter (processor.apvts, Processor::layBackId, 0.0f);
        setParameter (processor.apvts, Processor::regionLengthId, 100.0f);

        juce::AudioBuffer<float> testBuffer (2, 48000);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 48000; ++i)
                testBuffer.setSample (ch, i,
                    0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / sampleRate));
        processor.loadedSample = std::make_unique<LoadedSample>();
        processor.loadedSample->buffer = testBuffer;
        processor.loadedSample->sourceSampleRate = sampleRate;
        processor.sampleBuffer.store (&processor.loadedSample->buffer, std::memory_order_release);

        juce::AudioBuffer<float> audio (2, blockSize);
        audio.clear();
        
        // First note of key 60: swing is 75 but the first note of a phrase is straight.
        juce::MidiBuffer midi1;
        midi1.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        processor.processBlock (audio, midi1);

        int firstVoice = -1;
        for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
            if (processor.voicePool.voices[i].active && processor.voicePool.voices[i].key == 60)
            {
                firstVoice = i;
                break;
            }
        assert (firstVoice >= 0);
        const double firstOnset = processor.voicePool.voices[firstVoice].envStart;
        assert (firstOnset <= 0.0);

        // Second note of key 60: swung, queued with the swing delay instead of playing straight.
        juce::MidiBuffer midi2;
        midi2.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        processor.processBlock (audio, midi2);
        assert (processor.pendingNotes.size () == 1);
        assert (processor.pendingNotes[0].key == 60);
        assert (std::abs (processor.pendingNotes[0].delaySamples - 12000.0) < 1.0);

        // The swung note fires, late, at the delayed position (stream 1024 + the 12000-sample
        // swing delay = 12512), and the queue drains.
        juce::MidiBuffer noMidi;
        for (int block = 0; block < 40; ++block)
        {
            audio.clear();
            processor.processBlock (audio, noMidi);
        }

        assert (processor.pendingNotes.empty ());

        bool firedWithDelay = false;
        for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
            if (processor.voicePool.voices[i].active && processor.voicePool.voices[i].key == 60
                && std::abs (processor.voicePool.voices[i].envStart - 12512.0) < 1.0)
                firedWithDelay = true;

        assert (firedWithDelay);
        pass ("testSwingDelaysRepeatedNotes");
    }

    // Humanise (plan 05.4): every note gets a deterministic start offset before the (zero)
    // lay-back, so the note is queued and starts inside the humanise window.
    void testHumaniseDelaysNoteOnset()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameter (processor.apvts, Processor::swingId, 50.0f);
        setParameter (processor.apvts, Processor::humaniseId, 10.0f);
        setParameter (processor.apvts, Processor::layBackId, 0.0f);
        setParameter (processor.apvts, Processor::regionLengthId, 100.0f);

        juce::AudioBuffer<float> testBuffer (2, 48000);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 48000; ++i)
                testBuffer.setSample (ch, i,
                    0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / sampleRate));
        processor.loadedSample = std::make_unique<LoadedSample>();
        processor.loadedSample->buffer = testBuffer;
        processor.loadedSample->sourceSampleRate = sampleRate;
        processor.sampleBuffer.store (&processor.loadedSample->buffer, std::memory_order_release);

        juce::AudioBuffer<float> audio (2, blockSize);
        audio.clear();
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        processor.processBlock (audio, midi);

        // The humanise offset (10 ms at 48 kHz, deterministic per note) makes the delay small
        // enough to sit inside one block, so the queued note fires within this same block and
        // the queue drains. The voice must still start late, at the humanise offset.
        assert (processor.pendingNotes.empty ());

        int voice = -1;
        for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
            if (processor.voicePool.voices[i].active && processor.voicePool.voices[i].key == 60)
            {
                voice = i;
                break;
            }
        assert (voice >= 0);
        const double humaniseDelaySamples = processor.voicePool.voices[voice].envStart;
        // First note of the phrase, swing straight (50 %), lay-back off: the humanise offset is
        // unit * 10 ms = 0.3363 * 10 * 48 kHz = ~161 samples. Before the fix the humanise delay
        // was ignored and this onset sat at sample 0.
        assert (humaniseDelaySamples > 0.0);
        assert (std::abs (humaniseDelaySamples - 161.42) < 1.0);
        pass ("testHumaniseDelaysNoteOnset");
    }

    // Ghost notes (plan 05.4 Phase 2): a quiet note below the threshold becomes a quieter,
    // delayed copy; the ghost allocates a real voice when its delayed copy comes due.
    void testGhostDelaysQuietNote()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameter (processor.apvts, Processor::swingId, 50.0f);
        setParameter (processor.apvts, Processor::humaniseId, 0.0f);
        setParameter (processor.apvts, Processor::layBackId, 0.0f);
        setParameter (processor.apvts, Processor::regionLengthId, 100.0f);
        setParameter (processor.apvts, Processor::ghostOnId, 1.0f);
        setParameter (processor.apvts, Processor::ghostThresholdId, 50.0f);
        setParameter (processor.apvts, Processor::ghostLevelId, -12.0f);
        setParameter (processor.apvts, Processor::ghostDelayId, 10.0f);

        juce::AudioBuffer<float> testBuffer (2, 48000);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 48000; ++i)
                testBuffer.setSample (ch, i,
                    0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / sampleRate));
        processor.loadedSample = std::make_unique<LoadedSample>();
        processor.loadedSample->buffer = testBuffer;
        processor.loadedSample->sourceSampleRate = sampleRate;
        processor.sampleBuffer.store (&processor.loadedSample->buffer, std::memory_order_release);

        juce::AudioBuffer<float> audio (2, blockSize);
        audio.clear();
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.3f), 0);   // below the 50 % threshold
        processor.processBlock (audio, midi);

        // The ghost delay (10 ms = 480 samples) is shorter than one block, so the ghost fired
        // inside this block; the main note also played, so the queue drains and two voices sound.
        assert (processor.pendingNotes.empty ());

        int activeCount = 0;
        int ghostVoice = -1;
        for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
        {
            const auto& voice = processor.voicePool.voices[i];
            if (! voice.active)
                continue;
            ++activeCount;
            if (voice.envStart > 0.0 && voice.key == 60)
                ghostVoice = i;
        }
        assert (activeCount == 2);   // the main note and its ghost
        assert (ghostVoice >= 0);
        // The ghost started ghostDelay after the note, and carries the ghost level in its velocity.
        assert (std::abs (processor.voicePool.voices[ghostVoice].envStart - 480.0) < 1.0);
        const float expectedGhostVelocity = 0.3f * std::pow (10.0, -12.0 / 20.0);
        assert (std::abs (processor.voicePool.voices[ghostVoice].velocity - expectedGhostVelocity) < 1e-3);
        pass ("testGhostDelaysQuietNote");
    }

    // Ghosts need the switch on and the note below the threshold; with the velocity at or above
    // the threshold no ghost is spawned and only the main note plays.
    void testNoGhostWithoutQuietNote()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameter (processor.apvts, Processor::swingId, 50.0f);
        setParameter (processor.apvts, Processor::humaniseId, 0.0f);
        setParameter (processor.apvts, Processor::layBackId, 0.0f);
        setParameter (processor.apvts, Processor::regionLengthId, 100.0f);
        setParameter (processor.apvts, Processor::ghostOnId, 1.0f);
        setParameter (processor.apvts, Processor::ghostThresholdId, 50.0f);
        setParameter (processor.apvts, Processor::ghostLevelId, -12.0f);
        setParameter (processor.apvts, Processor::ghostDelayId, 10.0f);

        juce::AudioBuffer<float> testBuffer (2, 48000);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 48000; ++i)
                testBuffer.setSample (ch, i,
                    0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / sampleRate));
        processor.loadedSample = std::make_unique<LoadedSample>();
        processor.loadedSample->buffer = testBuffer;
        processor.loadedSample->sourceSampleRate = sampleRate;
        processor.sampleBuffer.store (&processor.loadedSample->buffer, std::memory_order_release);

        juce::AudioBuffer<float> audio (2, blockSize);
        audio.clear();
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);   // at the threshold, not below
        processor.processBlock (audio, midi);

        assert (processor.pendingNotes.empty ());

        int activeCount = 0;
        for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
            if (processor.voicePool.voices[i].active)
                ++activeCount;

        assert (activeCount == 1);   // main note only, no ghost
        pass ("testNoGhostWithoutQuietNote");
    }

    void testEditor()
    {
        Processor processor;
        checkEditor (processor);

        // The preset bar ships in every build, so it must be in the component tree and visible.
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        assert (editor != nullptr);

        bool foundPresetBar = false;
        for (int i = 0; i < editor->getNumChildComponents(); ++i)
        {
            auto* child = editor->getChildComponent (i);

            if (dynamic_cast<SuitePresets::PresetBar*> (child) != nullptr)
            {
                foundPresetBar = true;
                assert (child->isVisible());
            }
        }

        assert (foundPresetBar);
        processor.editorBeingDeleted (editor.get());
        pass ("testEditor");
    }

    /** Counts the editor's direct LabelledSlider children that are currently visible. */
    int countVisibleKnobs (juce::Component& editor)
    {
        int count = 0;

        for (int i = 0; i < editor.getNumChildComponents(); ++i)
        {
            auto* child = editor.getChildComponent (i);

            if (child->isVisible() && dynamic_cast<SuiteUI::LabelledSlider*> (child) != nullptr)
                ++count;
        }

        return count;
    }

    /** Finds a TextButton by its caption anywhere in the child hierarchy. */
    juce::TextButton* findTextButtonDeep (juce::Component* parent, const juce::String& text)
    {
        for (int i = 0; i < parent->getNumChildComponents(); ++i)
        {
            auto* child = parent->getChildComponent (i);

            if (auto* button = dynamic_cast<juce::TextButton*> (child))
                if (button->getButtonText() == text)
                    return button;

            if (auto* deeper = findTextButtonDeep (child, text))
                return deeper;
        }

        return nullptr;
    }

    /** Finds a LabelledSlider by its caption anywhere in the child hierarchy. */
    SuiteUI::LabelledSlider* findLabelledSliderDeep (juce::Component* parent, const juce::String& caption)
    {
        for (int i = 0; i < parent->getNumChildComponents(); ++i)
        {
            auto* child = parent->getChildComponent (i);

            if (auto* labelled = dynamic_cast<SuiteUI::LabelledSlider*> (child))
                if (labelled->label.getText() == caption)
                    return labelled;

            if (auto* deeper = findLabelledSliderDeep (child, caption))
                return deeper;
        }

        return nullptr;
    }

    /** Finds a component by its component ID anywhere in the child hierarchy. */
    juce::Component* findComponentByIdDeep (juce::Component* parent, const juce::String& id)
    {
        for (int i = 0; i < parent->getNumChildComponents(); ++i)
        {
            auto* child = parent->getChildComponent (i);

            if (child->getComponentID() == id)
                return child;

            if (auto* deeper = findComponentByIdDeep (child, id))
                return deeper;
        }

        return nullptr;
    }

    template <class ComponentType>
    ComponentType* findChildOfTypeDeep (juce::Component* parent)
    {
        for (int i = 0; i < parent->getNumChildComponents(); ++i)
        {
            auto* child = parent->getChildComponent (i);

            if (auto* match = dynamic_cast<ComponentType*> (child))
                return match;

            if (auto* deeper = findChildOfTypeDeep<ComponentType> (child))
                return deeper;
        }

        return nullptr;
    }

    // Plan 10 phase 5: the status bar below the keyboard follows the pointer's help text.
    void testStatusBarShowsHelp()
    {
        Processor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        assert (editor != nullptr);
        auto* sgraffito = static_cast<SgraffitoEditor*> (editor.get());

        auto* status = findComponentByIdDeep (editor.get(), "sgraffito-status");
        assert (status != nullptr);
        auto* keyboard = findChildOfTypeDeep<SuiteUI::PlayableKeyboardStrip> (editor.get());
        assert (keyboard != nullptr);
        assert (status->getY() >= keyboard->getBottom());   // the bar sits below the keyboard

        // The idle hint shows before any pointer movement.
        auto* statusText = dynamic_cast<juce::Label*> (status);
        assert (statusText != nullptr);
        assert (statusText->getText() == "Hover a control for help.");

        // Every control carries a help text (spot checks across all three pages).
        assert (findLabelledSliderDeep (editor.get(), "LEVEL") != nullptr
                && ! findLabelledSliderDeep (editor.get(), "LEVEL")->getHelpText().isEmpty());
        assert (! findLabelledSliderDeep (editor.get(), "SLICE COUNT")->getHelpText().isEmpty());
        assert (! findLabelledSliderDeep (editor.get(), "GRAIN SIZE")->getHelpText().isEmpty());
        assert (! findTextButtonDeep (editor.get(), "LOAD SOUND")->getHelpText().isEmpty());

        // A synthesised pointer move over a knob shows that knob's text.
        auto makeEvent = [] (juce::Component& c)
        {
            const auto centre = c.getLocalBounds().getCentre().toFloat();
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), centre,
                                     juce::ModifierKeys(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                     &c, &c, juce::Time::getCurrentTime(), centre,
                                     juce::Time::getCurrentTime(), 1, false);
        };

        auto* level = findLabelledSliderDeep (editor.get(), "LEVEL");
        const auto levelText = level->getHelpText();
        sgraffito->showHelp (makeEvent (*level));
        assert (statusText->getText() == levelText);

        // A move over something without a help text returns to the idle hint.
        sgraffito->showHelp (makeEvent (*editor));
        assert (statusText->getText() == "Hover a control for help.");

        const auto renderDir = juce::SystemStats::getEnvironmentVariable ("SUITE_RENDER_DIR", {});
        if (renderDir.isNotEmpty())
            assert (PluginTest::writeDeterministicPng (*editor,
                                                       juce::File (renderDir).getChildFile ("sgraffito-status-bar.png")));

        processor.editorBeingDeleted (editor.get());
        pass ("testStatusBarShowsHelp");
    }

    // Plan 10 phase 1: LOAD SOUND sits at the waveform view's top-right. The button is a
    // child of the waveform view; the old layout used the view's editor-relative bounds,
    // which pushed the button to the waveform's vertical middle.
    void testLoadButtonSitsTopRight()
    {
        Processor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        assert (editor != nullptr);
        static_cast<SgraffitoEditor*> (editor.get())->setPage (0);

        auto* button = findTextButtonDeep (editor.get(), "LOAD SOUND");
        assert (button != nullptr);
        auto* parent = button->getParentComponent();
        assert (parent != nullptr);
        assert (parent != editor.get());   // the button lives inside the waveform view

        // Waveform-local coordinates: inside the top strip, against the right edge.
        std::printf ("load button local bounds: x %d, y %d, %dx%d (view %dx%d)\n",
                     button->getX(), button->getY(),
                     button->getWidth(), button->getHeight(),
                     parent->getWidth(), parent->getHeight());
        assert (button->getY() <= 8);
        assert (button->getRight() >= parent->getWidth() - 8);

        const auto renderDir = juce::SystemStats::getEnvironmentVariable ("SUITE_RENDER_DIR", {});
        if (renderDir.isNotEmpty())
            assert (PluginTest::writeDeterministicPng (*editor,
                                                       juce::File (renderDir).getChildFile ("sgraffito-load-button.png")));

        processor.editorBeingDeleted (editor.get());
        pass ("testLoadButtonSitsTopRight");
    }

    // Plan 10 phase 2 (decision G1): mode-only controls are disabled — dimmed, never
    // hidden — outside the mode they serve; common controls stay enabled everywhere.
    void testModeAffinity()
    {
        Processor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        assert (editor != nullptr);
        auto* sgraffito = static_cast<SgraffitoEditor*> (editor.get());

        auto enabled = [&] (const char* caption)
        {
            auto* slider = findLabelledSliderDeep (editor.get(), caption);
            assert (slider != nullptr);
            return slider->isEnabled();
        };

        struct Case { int mode; const char* on; std::initializer_list<const char*> off; };
        const Case cases[] = {
            // Scrub: level (common) stays on; every mode-only control is off.
            { 0, "LEVEL", { "SLICE COUNT", "GRAIN SIZE", "GRAIN DENSITY", "GRAIN SPREAD",
                            "SWELL TIME", "SWELL CROSSFADE" } },
            { 1, "SLICE COUNT", { "GRAIN SIZE", "GRAIN DENSITY", "GRAIN SPREAD",
                                  "SWELL TIME", "SWELL CROSSFADE" } },
            { 2, "GRAIN SIZE",  { "SLICE COUNT", "SWELL TIME", "SWELL CROSSFADE" } },
            { 3, "SWELL TIME",  { "SLICE COUNT", "GRAIN SIZE", "GRAIN DENSITY" } },
        };

        for (const auto& c : cases)
        {
            sgraffito->setPage (1);
            // The host-automation path drives the mode through the parameter.
            PluginTest::setParameter (processor.apvts, SgraffitoProcessor::modeId, (float) c.mode);
            assert (enabled (c.on));
            for (auto* caption : c.off)
                assert (! enabled (caption));
        }

        // Per-mode TEXTURE renders (suite rule R9): this change intentionally dims
        // controls, so the renders are the record of the new look. The names carry the
        // mode, so the user manual can show them without a translation table.
        const auto renderDir = juce::SystemStats::getEnvironmentVariable ("SUITE_RENDER_DIR", {});
        if (renderDir.isNotEmpty())
        {
            const char* modeNames[4] = { "scrub", "chop", "cloud", "swell" };
            sgraffito->setPage (1);
            for (int m = 0; m < 4; ++m)
            {
                PluginTest::setParameter (processor.apvts, SgraffitoProcessor::modeId, (float) m);
                assert (PluginTest::writeDeterministicPng (*editor,
                            juce::File (renderDir).getChildFile ("sgraffito-texture-mode-" + juce::String (modeNames[m]) + ".png")));
            }
        }

        processor.editorBeingDeleted (editor.get());
        pass ("testModeAffinity");
    }

    // Every knob belongs to exactly one page; an unlisted one floats over all of them.
    void testPageVisibility()
    {
        Processor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        assert (editor != nullptr);
        auto* sgraffito = static_cast<SgraffitoEditor*> (editor.get());

        sgraffito->setPage (0);
        assert (countVisibleKnobs (*editor) == 9);    // SOUND: region span, length, root, fine, glide, level, curve, attack, release

        sgraffito->setPage (1);
        assert (countVisibleKnobs (*editor) == 22);   // TEXTURE: tape/memory, drift/timing, ghost, cloud, swell & surface

        sgraffito->setPage (2);
        assert (countVisibleKnobs (*editor) == 0);    // MOD: the modulation panel's knobs live inside the panel

        processor.editorBeingDeleted (editor.get());
        pass ("testPageVisibility");
    }

    // When SUITE_RENDER_DIR is set, writes a PNG of each editor page and checks the
    // two renders of the same view match within one process.
    void testRenderBaseline()
    {
        const auto folder = juce::SystemStats::getEnvironmentVariable ("SUITE_RENDER_DIR", {});

        if (folder.isEmpty())
        {
            std::printf ("[SKIP] editor renders: set SUITE_RENDER_DIR to write them\n");
            return;
        }

        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        for (int renderPage = 0; renderPage < 3; ++renderPage)
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
            assert (editor != nullptr);
            static_cast<SgraffitoEditor*> (editor.get())->setPage (renderPage);

            const juce::File file (juce::File (folder).getChildFile (
                renderPage == 0 ? "sgraffito-sound.png"
                                : renderPage == 1 ? "sgraffito-texture.png" : "sgraffito-mod.png"));
            assert (PluginTest::writeDeterministicPng (*editor, file));
            processor.editorBeingDeleted (editor.get());
        }

        // The four playback modes, each on the SOUND page, for the user manual (plan 10-era
        // renders only cover the pages).
        for (int mode = 0; mode < 4; ++mode)
        {
            setParameter (processor.apvts, Processor::modeId, (float) mode);

            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
            assert (editor != nullptr);
            static_cast<SgraffitoEditor*> (editor.get())->setPage (0);

            const juce::File file (juce::File (folder).getChildFile (
                std::array<const char*, 4> { "sgraffito-mode-scrub.png", "sgraffito-mode-chop.png",
                                             "sgraffito-mode-cloud.png", "sgraffito-mode-swell.png" }[(size_t) mode]));
            assert (PluginTest::writeDeterministicPng (*editor, file));
            processor.editorBeingDeleted (editor.get());
        }
        setParameter (processor.apvts, Processor::modeId, 0.0f);

        pass ("testRenderBaseline");
    }

    //==============================================================================
    // LFOs and modulation routing

    /** The routable controls, in ModTarget order, with the parameter each one moves. */
    struct TargetEntry { Processor::ModTarget target; const char* id; };

    const TargetEntry targetEntries[] {
        { Processor::ModTarget::RegionSpan,     Processor::regionSpanId },
        { Processor::ModTarget::RegionLength,   Processor::regionLengthId },
        { Processor::ModTarget::RootKey,        Processor::rootKeyId },
        { Processor::ModTarget::PitchFine,      Processor::pitchFineId },
        { Processor::ModTarget::Glide,          Processor::glideId },
        { Processor::ModTarget::Level,          Processor::levelId },
        { Processor::ModTarget::Attack,         Processor::attackId },
        { Processor::ModTarget::Release,        Processor::releaseId },
        { Processor::ModTarget::MemoryAmount,   Processor::memoryAmountId },
        { Processor::ModTarget::DriftAmount,    Processor::driftAmountId },
        { Processor::ModTarget::DriftSpeed,     Processor::driftSpeedId },
        { Processor::ModTarget::LayBack,        Processor::layBackId },
        { Processor::ModTarget::Swing,          Processor::swingId },
        { Processor::ModTarget::Humanise,       Processor::humaniseId },
        { Processor::ModTarget::SustainHold,    Processor::sustainHoldId },
        { Processor::ModTarget::GhostThreshold, Processor::ghostThresholdId },
        { Processor::ModTarget::GhostLevel,     Processor::ghostLevelId },
        { Processor::ModTarget::GrainSize,      Processor::grainSizeId },
        { Processor::ModTarget::GrainDensity,   Processor::grainDensityId },
        { Processor::ModTarget::SwellTime,      Processor::swellTimeId },
        { Processor::ModTarget::Dust,           Processor::dustId },
        { Processor::ModTarget::Crackle,        Processor::crackleId },
        { Processor::ModTarget::Wow,            Processor::wowId },
        { Processor::ModTarget::Flutter,        Processor::flutterId },
        { Processor::ModTarget::ShuffleSeed,    Processor::shuffleSeedId },
    };

    /** Runs the processor with a held note over a one-second sine sample and returns the left output. */
    std::vector<float> renderNote (Processor& processor, int blocks, int key = 60)
    {
        juce::AudioBuffer<float> testBuffer (2, 48000);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 48000; ++i)
                testBuffer.setSample (ch, i,
                    0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / sampleRate));

        processor.loadedSample = std::make_unique<LoadedSample>();
        processor.loadedSample->buffer = testBuffer;
        processor.loadedSample->sourceSampleRate = sampleRate;
        processor.sampleBuffer.store (&processor.loadedSample->buffer, std::memory_order_release);

        // The whole sample is the region, so the note plays for the whole render.
        setParameter (processor.apvts, Processor::regionLengthId, 100.0f);

        processor.prepareToPlay (sampleRate, blockSize);

        std::vector<float> output;

        for (int block = 0; block < blocks; ++block)
        {
            juce::AudioBuffer<float> audio (2, blockSize);
            audio.clear();
            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, key, 0.8f), 0);

            processor.processBlock (audio, midi);

            for (int n = 0; n < blockSize; ++n)
                output.push_back (audio.getSample (0, n));
        }

        return output;
    }

    void testModulationParameters()
    {
        Processor processor;

        checkRange (processor.apvts, Processor::lfo1RateId, 0.02f, 20.0f, 0.25f);
        checkRange (processor.apvts, Processor::lfo1ShapeId, 0.0f, 7.0f, 0.0f);
        checkRange (processor.apvts, Processor::lfo1SyncId, 0.0f, 1.0f, 0.0f);
        checkRange (processor.apvts, Processor::lfo1DivisionId, 0.0f, 6.0f, 4.0f);
        checkRange (processor.apvts, Processor::lfo1OffsetId, -100.0f, 100.0f, 0.0f);
        checkRange (processor.apvts, Processor::lfo2RateId, 0.02f, 20.0f, 0.25f);

        checkRange (processor.apvts, Processor::modSourceIds[0], 0.0f, 1.0f, 0.0f);
        
        // 27 choices (None + 26 modulation targets), so range is 0-26
        checkRange (processor.apvts, Processor::modTargetIds[0], 0.0f, 26.0f, 0.0f);
        checkRange (processor.apvts, Processor::modAmountIds[0], -100.0f, 100.0f, 0.0f);
        checkRange (processor.apvts, Processor::modDelayIds[0], 0.0f, 10.0f, 0.0f);

        // The target choices must match the enum order the sessions save.
        auto* target = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (Processor::modTargetIds[0]));
        assert (target != nullptr);
        assert (target->choices == Processor::modTargetNames());
        // Choices include "None" + all ModTarget enum values
        assert (target->choices.size() == (int) Processor::ModTargetCount + 1);

        assert (processor.getRouter().getNumTargets() == (int) Processor::ModTargetCount);

        pass ("testModulationParameters");
    }

    // With every routing amount at zero the modulation changes nothing, bit for bit.
    void testDefaultRoutingIsSilent()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        for (auto* id : Processor::modAmountIds)
            assert (getParameter (processor.apvts, id) == 0.0f);

        // One block publishes the offsets the display reads.
        juce::AudioBuffer<float> audio (2, blockSize);
        audio.clear();
        juce::MidiBuffer midi;
        processor.processBlock (audio, midi);

        for (const auto& entry : targetEntries)
        {
            assert (processor.getRouter().getTargetOffset ((int) entry.target) == 0.0f);
            assert (processor.getModulatedValue (entry.target) == getParameter (processor.apvts, entry.id));
        }

        // A routing that is aimed at a control but has no amount is the same as no routing at all.
        Processor routed;
        setRoute (routed, 0, 0, (int) Processor::ModTarget::Level, 0.0f);

        const auto plainOutput = renderNote (processor, 40);
        const auto routedOutput = renderNote (routed, 40);

        assert (plainOutput == routedOutput);

        pass ("testDefaultRoutingIsSilent");
    }

    // The routing reaches the audio: a routed level changes the output, and removing the amount restores it.
    void testModulationReachesAudio()
    {
        Processor processor;
        setLfo (processor, 0, 0.2f, 3.0f, false, 4.0f, 50.0f);   // square, 0.2 Hz (1 cycle/sec), +50% offset
        
        // Create sample data for the test - use a longer buffer to ensure we capture multiple LFO cycles
        juce::AudioBuffer<float> testBuffer (2, 48000);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 48000; ++i)
                testBuffer.setSample (ch, i, 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / sampleRate));
        
        processor.loadedSample = std::make_unique<LoadedSample>();
        processor.loadedSample->buffer = testBuffer;
        processor.loadedSample->sourceSampleRate = sampleRate;
        
        setParameter (processor.apvts, Processor::regionLengthId, 50.0f);
        setParameter (processor.apvts, Processor::releaseId, 20.0f);
        processor.sampleBuffer.store (&processor.loadedSample->buffer, std::memory_order_release);
        processor.prepareToPlay (sampleRate, blockSize);
        
        // Process WITH modulation first - start with 100% routing
        setRoute (processor, 0, 0, (int) Processor::ModTarget::Level, 100.0f);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        
        float peakWithModulation = 0.0f;
        int maxVoices = 0;
        
        // Process enough blocks for the note to play and release WITH modulation
        for (int block = 0; block < 10; ++block)
        {
            juce::AudioBuffer<float> audio (2, blockSize);
            audio.clear();
            
            processor.processBlock (audio, midi);
            
            int voices = processor.getNumActiveVoices();
            maxVoices = std::max(maxVoices, voices);
            
            // Check for audio output
            float chPeak[2] = {0.0f, 0.0f};
            for (int ch = 0; ch < 2; ++ch)
            {
                for (int i = 0; i < blockSize; ++i)
                {
                    const float s = audio.getSample(ch, i);
                    chPeak[ch] = std::max(chPeak[ch], std::abs(s));
                }
            }
            
            peakWithModulation = std::max(peakWithModulation, chPeak[0]);
        }
        
        assert (maxVoices > 0);
        printf("Max voices: %d, Peak with modulation: %f\n", maxVoices, peakWithModulation);
        assert (peakWithModulation > 0.1f);
        
        // Now process WITHOUT modulation - reset routing to 0% and create a FRESH MIDI buffer
        setRoute (processor, 0, 0, (int) Processor::ModTarget::Level, 0.0f);
        
        float peakWithoutModulation = 0.0f;
        juce::MidiBuffer midi2;
        midi2.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        
        // Process the same number of blocks WITHOUT modulation
        for (int block = 0; block < 10; ++block)
        {
            juce::AudioBuffer<float> audio (2, blockSize);
            audio.clear();
            
            processor.processBlock (audio, midi2);
            
            float chPeak[2] = {0.0f, 0.0f};
            for (int ch = 0; ch < 2; ++ch)
            {
                for (int i = 0; i < blockSize; ++i)
                    chPeak[ch] = std::max(chPeak[ch], std::abs(audio.getSample(ch, i)));
            }
            
            peakWithoutModulation = std::max(peakWithoutModulation, chPeak[0]);
        }
        
        // Debug: print both peak values
        printf("Peak without modulation: %f, Diff: %f\n", peakWithoutModulation, std::abs(peakWithModulation - peakWithoutModulation));
        
        // The routing reaches the audio: with 100% routing and +50% LFO offset, the level varies.
        // Without modulation, the level should be at the base value (0 dB).
        // The peaks will differ because the LFO adds up to +6dB of gain when at maximum (+50% offset)
        // This is a factor of about 2x difference, which is expected behavior.
        assert (std::abs(peakWithModulation - peakWithoutModulation) > 0.1f);
        
        pass ("testModulationReachesAudio");
    }

    // Every routable control stays inside its own range, whatever the LFO does.
    void testModulationStaysInRange()
    {
        for (const auto& entry : targetEntries)
        {
            Processor processor;
            setLfo (processor, 0, 3.0f, 3.0f, false, 4.0f, 100.0f);   // square, 3 Hz, +100% offset to max out modulation
            setRoute (processor, 0, 0, (int) entry.target, 100.0f);
            processor.prepareToPlay (sampleRate, blockSize);

            auto* parameter = processor.apvts.getParameter (entry.id);
            assert (parameter != nullptr);
            const auto range = parameter->getNormalisableRange();

            juce::AudioBuffer<float> audio (2, blockSize);
            juce::MidiBuffer midi;
            float smallest = range.end;
            float largest = range.start;

            for (int block = 0; block < 120; ++block)
            {
                audio.clear();
                midi.clear();
                processor.processBlock (audio, midi);

                const float value = processor.getModulatedValue (entry.target);
                assert (std::isfinite (value));
                smallest = juce::jmin (smallest, value);
                largest = juce::jmax (largest, value);
            }

            assert (smallest >= range.start - 1.0e-3f);
            assert (largest <= range.end + 1.0e-3f);
        }

        pass ("testModulationStaysInRange");
    }

    void testRegionMapping()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        // Set full span: key 0 starts at 0, key 127 ends at 1.0
        setParameter (processor.apvts, Processor::regionSpanId, 100.0f);
        setParameter (processor.apvts, Processor::regionLengthId, 25.0f);

        juce::MidiBuffer midiOn;
        midiOn.addEvent (juce::MidiMessage::noteOn (1, 0, 0.8f), 0);

        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();
        processor.processBlock (buffer, midiOn);

        // Key 0 should start at region start 0
        const auto& voice0 = processor.voicePool.voices[0];
        assert (voice0.active);
        assert (std::abs (voice0.regionStart - 0.0) < 1.0);
        assert (voice0.regionEnd > voice0.regionStart);

        // Key 127 should end at region end near sample length
        juce::MidiBuffer midiOn2;
        midiOn2.addEvent (juce::MidiMessage::noteOn (1, 127, 0.8f), 0);
        processor.processBlock (buffer, midiOn2);

        // Find the voice for key 127. It may already have finished (key 127 plays at ~48x
        // rate since plan 05.1), but its region fields persist in the slot.
        bool found = false;
        for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
        {
            if (processor.voicePool.voices[i].key == 127)
            {
                // With 25% length starting at position ~0.99, end should be > start
                assert (processor.voicePool.voices[i].regionEnd > processor.voicePool.voices[i].regionStart);
                found = true;
                break;
            }
        }
        assert (found);
        pass ("testRegionMapping");
    }

    void testVoiceProducesOutput()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        // Directly populate the LoadedSample structure with a simple sine wave
        const int numSamples = 48000;
        juce::AudioBuffer<float> testBuffer (2, numSamples);
        
        for (int ch = 0; ch < 2; ++ch)
        {
            float* dst = testBuffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                dst[i] = 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f * i / static_cast<float>(numSamples));
        }
        
        // Create a LoadedSample that will live as long as the processor
        processor.loadedSample = std::make_unique<LoadedSample>();
        processor.loadedSample->buffer = testBuffer;
        processor.loadedSample->sourceSampleRate = sampleRate;
        
        // Store the pointer atomically - this is safe because loadedSample is a member of the processor
        processor.sampleBuffer.store (&processor.loadedSample->buffer, std::memory_order_release);

        juce::MidiBuffer midiOn;
        midiOn.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);

        // Process multiple blocks to allow the envelope to ramp up and produce audio
        juce::AudioBuffer<float> buffer (2, blockSize);
        float peak = 0.0f;
        
        for (int block = 0; block < 10 && peak < 0.01f; ++block)
        {
            buffer.clear();
            processor.processBlock(buffer, midiOn);
            
            // Check for audio output
            for (int ch = 0; ch < 2; ++ch)
            {
                for (int i = 0; i < blockSize; ++i)
                    peak = std::max(peak, std::abs(buffer.getSample(ch, i)));
            }
            
            // Remove MIDI after first block so subsequent blocks don't re-trigger
            midiOn.clear();
        }

        assert(peak > 0.01f);
        pass("testVoiceProducesOutput");
    }

    void testChopModeRegionMapping()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        // Set Chop mode (mode 1) with 4 slices
        setParameter (processor.apvts, Processor::modeId, 1.0f);
        setParameter (processor.apvts, Processor::sliceCountId, 4.0f);
        setParameter (processor.apvts, Processor::regionSpanId, 100.0f);

        // Key 0 should map to slice 0 (first quarter)
        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();

        juce::MidiBuffer midi0;
        midi0.addEvent (juce::MidiMessage::noteOn (1, 0, 0.8f), 0);
        processor.processBlock (buffer, midi0);
        
        // Find voice for key 0
        bool found0 = false;
        for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
        {
            if (processor.voicePool.voices[i].active && processor.voicePool.voices[i].key == 0)
            {
                // Slice 0 should start at 0.0
                assert (std::abs (processor.voicePool.voices[i].regionStart - 0.0) < 1e-6);
                // Slice 0 should end at 0.25 of sample length
                assert (std::abs (processor.voicePool.voices[i].regionEnd - 0.25) < 1e-6);
                found0 = true;
                break;
            }
        }
        assert (found0);
        pass ("testChopModeRegionMapping");
    }

    void testChopModeSliceCoverage()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        // Set Chop mode with 8 slices
        setParameter (processor.apvts, Processor::modeId, 1.0f);
        setParameter (processor.apvts, Processor::sliceCountId, 8.0f);
        setParameter (processor.apvts, Processor::regionSpanId, 100.0f);

        // Each key should map to a different slice
        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();

        double prevRegionStart = -1.0;
        bool slicesCovered = true;

        for (int key = 0; key < 128; key += 16)
        {
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, key, 0.8f), 0);
            processor.processBlock (buffer, midi);

            bool foundKey = false;
            for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
            {
                // The voice may already have finished (high keys play at high rates), but
                // its region fields persist in the slot.
                if (processor.voicePool.voices[i].key == key)
                {
                    foundKey = true;
                    if (processor.voicePool.voices[i].regionStart < prevRegionStart - 1e-6)
                    {
                        slicesCovered = false;
                    }
                    prevRegionStart = processor.voicePool.voices[i].regionStart;
                    break;
                }
            }
            assert (foundKey);
        }
        assert (slicesCovered);
        pass ("testChopModeSliceCoverage");
    }

    void testModesProduceDifferentAudio()
    {
        // The owner reported "nearly always the same" sound: the four modes must produce
        // different output (plan 05.1).
        const int key = 60;

        std::vector<float> outputs[4];

        for (int mode = 0; mode < 4; ++mode)
        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            setParameter (processor.apvts, Processor::modeId, (float) mode);

            // A plain sine, so every mode's region reads loud audio (a decaying sample
            // would make Swell's backwards read start near silence).
            juce::AudioBuffer<float> testBuffer (2, 48000);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 48000; ++i)
                    testBuffer.setSample (ch, i,
                        0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / sampleRate));

            processor.loadedSample = std::make_unique<LoadedSample>();
            processor.loadedSample->buffer = testBuffer;
            processor.loadedSample->sourceSampleRate = sampleRate;
            processor.sampleBuffer.store (&processor.loadedSample->buffer, std::memory_order_release);

            outputs[mode] = renderNote (processor, 24);

            for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
            {
                auto& v = processor.voicePool.voices[i];
                if (v.key == key)
                {
                    std::printf ("mode %d: region %.4f - %.4f, rate %.3f, pos %.1f, grainPos %.0f, envStart %.1f, gain %.4f, vel %.3f\n",
                                 mode,
                                 v.regionStart,
                                 v.regionEnd,
                                 v.rate,
                                 v.position,
                                 v.grainPosition,
                                 v.envStart,
                                 v.gain,
                                 v.velocity);
                }
            }
        }

        for (int a = 0; a < 4; ++a)
        {
            double energy = 0.0;
            for (float v : outputs[a])
                energy += std::abs (v);
            std::printf ("mode %d: total |x| = %.3f\n", a, energy);
            
            // Print first 20 samples for each mode
            std::printf ("mode %d samples 0-19:", a);
            for (int n = 0; n < 20 && n < outputs[a].size(); ++n)
                std::printf (" %.4f", outputs[a][n]);
            std::printf ("\n");
        }

        // What is actually in the processor's sample buffer?
        {
            Processor probe;
            probe.prepareToPlay (sampleRate, blockSize);
            juce::AudioBuffer<float> testBuffer (2, 48000);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 48000; ++i)
                    testBuffer.setSample (ch, i,
                        0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / sampleRate));
            probe.loadedSample = std::make_unique<LoadedSample>();
            probe.loadedSample->buffer = testBuffer;
            probe.loadedSample->sourceSampleRate = sampleRate;
            probe.sampleBuffer.store (&probe.loadedSample->buffer, std::memory_order_release);
            std::printf ("probe buffer: numSamples %d, at2000:", probe.loadedSample->buffer.getNumSamples());
            for (int n = 0; n < 6; ++n)
                std::printf (" %.4f", probe.loadedSample->buffer.getSample (0, 2000 + n));
            std::printf ("\n");
        }

        for (int a = 0; a < 4; ++a)
            for (int b = a + 1; b < 4; ++b)
            {
                double difference = 0.0;
                for (size_t n = 0; n < outputs[a].size(); ++n)
                    difference += std::abs (outputs[a][n] - outputs[b][n]);

                const double average = difference / (double) outputs[a].size();
                std::printf ("modes %d vs %d: avg diff %.6f\n", a, b, average);
                assert (average > 1.0e-3);
            }

        pass ("testModesProduceDifferentAudio");
    }

    void testKeyChangesPitch()
    {
        // The key's distance from middle C must change the playback rate (plan 05.1):
        // a note an octave up reads the sample at twice the rate.
        auto renderAtKey = [] (int key)
        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);

            juce::AudioBuffer<float> testBuffer (2, 48000);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 48000; ++i)
                    testBuffer.setSample (ch, i,
                        0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / sampleRate));

            processor.loadedSample = std::make_unique<LoadedSample>();
            processor.loadedSample->buffer = testBuffer;
            processor.loadedSample->sourceSampleRate = sampleRate;
            processor.sampleBuffer.store (&processor.loadedSample->buffer, std::memory_order_release);

            setParameter (processor.apvts, Processor::regionLengthId, 100.0f);
            return renderNote (processor, 40, key);
        };

        const auto low = renderAtKey (48);
        const auto high = renderAtKey (72);

        // The octave-up note reads the sample at twice the rate: twice the apparent
        // frequency. Count positive-going zero crossings in the first 2048 samples.
        auto crossings = [] (const std::vector<float>& output)
        {
            int count = 0;
            for (size_t n = 1; n < output.size() && n < 2048; ++n)
                if (output[n - 1] <= 0.0f && output[n] > 0.0f)
                    ++count;
            return count;
        };

        const int lowCrossings = crossings (low);
        const int highCrossings = crossings (high);
        std::printf ("pitch: crossings low %d, high %d\n", lowCrossings, highCrossings);
        assert (highCrossings > lowCrossings + 2);
        pass ("testKeyChangesPitch");
    }

    // Plan 10 phase 4 (decision G4): Root Key is the pitch centre. The root key's note
    // plays the sample at its natural pitch; key 60 with Root Key 72 plays an octave
    // lower. The old pitch centre was a fixed middle C, so the knob did nothing.
    void testRootKeyTransposes()
    {
        auto renderAtRoot = [] (float rootKey)
        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            setParameter (processor.apvts, Processor::rootKeyId, rootKey);

            juce::AudioBuffer<float> testBuffer (2, 48000);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 48000; ++i)
                    testBuffer.setSample (ch, i,
                        0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / sampleRate));

            processor.loadedSample = std::make_unique<LoadedSample>();
            processor.loadedSample->buffer = testBuffer;
            processor.loadedSample->sourceSampleRate = sampleRate;
            processor.sampleBuffer.store (&processor.loadedSample->buffer, std::memory_order_release);

            return renderNote (processor, 40, 60);
        };

        auto crossings = [] (const std::vector<float>& output)
        {
            int count = 0;
            for (size_t n = 1; n < output.size() && n < 2048; ++n)
                if (output[n - 1] <= 0.0f && output[n] > 0.0f)
                    ++count;
            return count;
        };

        const int natural = crossings (renderAtRoot (60.0f));
        const int octaveDown = crossings (renderAtRoot (72.0f));
        std::printf ("root key: natural %d crossings, root 72 %d crossings\n", natural, octaveDown);
        assert (octaveDown + 3 <= natural);   // an octave lower: about half the crossings
        pass ("testRootKeyTransposes");
    }

    // ------------------------------------------------------------------
    // T2: the Cloud-crackle-and-silence regressions.

    /** Scrub plays at the sample's own rate: the voice's state machine advanced once per
        output *channel* before, so stereo output played everything at double speed and
        ended one-shots after half the region. A whole-sample region must still be playing
        past 25 000 samples (the old code finished at 24 000). */
    void testScrubPlaysAtSampleRate()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        // renderNote loads a one-second sine sample and sets Region Length to 100 %,
        // so the whole sample is the region.
        const auto output = renderNote (processor, 50);   // 50 x 512 = 25 600 samples

        float latePeak = 0.0f;
        const size_t lateStart = output.size() - 4 * blockSize;

        for (size_t n = lateStart; n < output.size(); ++n)
            latePeak = std::max (latePeak, std::abs (output[n]));

        assert (latePeak > 0.05f);   // still sounding past 24 000 samples
        pass ("testScrubPlaysAtSampleRate");
    }

    /** Cloud holds its level over a second, and both channels read the same grains.
        The old per-channel state machine gave each side its own grain positions
        (the crackle), and the binary memory gate faded every voice to true silence. */
    void testCloudSustainsInStereo()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        setParameter (processor.apvts, Processor::modeId, 2.0f);   // Cloud

        // A quiet sine so four lanes summing cannot clip the assert range.
        juce::AudioBuffer<float> testBuffer (2, 48000);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 48000; ++i)
                testBuffer.setSample (ch, i,
                    0.25f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / sampleRate));

        processor.loadedSample = std::make_unique<LoadedSample>();
        processor.loadedSample->buffer = testBuffer;
        processor.loadedSample->sourceSampleRate = sampleRate;
        processor.sampleBuffer.store (&processor.loadedSample->buffer, std::memory_order_release);

        setParameter (processor.apvts, Processor::regionSpanId, 100.0f);
        setParameter (processor.apvts, Processor::regionLengthId, 100.0f);

        juce::AudioBuffer<float> audio (2, blockSize);

        float earlyPeak = 0.0f;
        float latePeak = 0.0f;
        float maxChannelDifference = 0.0f;

        for (int block = 0; block < 100; ++block)   // 100 x 512 = 51 200 samples, ~1.07 s
        {
            audio.clear();
            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);

            processor.processBlock (audio, midi);

            for (int n = 0; n < blockSize; ++n)
            {
                const float l = audio.getSample (0, n);
                const float r = audio.getSample (1, n);
                earlyPeak = block >= 5 && block < 15 ? std::max (earlyPeak, std::max (std::abs (l), std::abs (r))) : earlyPeak;
                latePeak = block >= 85 && block < 95 ? std::max (latePeak, std::max (std::abs (l), std::abs (r))) : latePeak;
                maxChannelDifference = std::max (maxChannelDifference, std::abs (l - r));
            }
        }

        std::printf ("cloud: early peak %.4f, late peak %.4f, max L/R diff %.2e\n",
                     earlyPeak, latePeak, maxChannelDifference);

        // Sound at the start, and still sound a second later: the default Memory Amount
        // of 20 % must not fade the note away (its floor is 80 %).
        assert (earlyPeak > 0.05f);
        assert (latePeak > 0.05f);

        // Identical stereo reads: both channels follow the same grain positions.
        assert (maxChannelDifference < 1.0e-6f);

        // The cloud note still runs: its lanes only end on release.
        assert (processor.voicePool.countActive() == CoreDSP::Sampler::cloudLanesPerNote);

        pass ("testCloudSustainsInStereo");
    }

    /** A released note's tape stop runs in real time and frees the voice. The ramp
        ticked once per block before (one sample's worth per tick), so the default
        400 ms stop took minutes and the voice piled up in the pool at full level. */
    void testTapeStopReleaseFreesVoice()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> testBuffer (2, 48000);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 48000; ++i)
                testBuffer.setSample (ch, i,
                    0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / sampleRate));

        processor.loadedSample = std::make_unique<LoadedSample>();
        processor.loadedSample->buffer = testBuffer;
        processor.loadedSample->sourceSampleRate = sampleRate;
        processor.sampleBuffer.store (&processor.loadedSample->buffer, std::memory_order_release);

        juce::AudioBuffer<float> audio (2, blockSize);
        audio.clear();

        juce::MidiBuffer midiOn;
        midiOn.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        processor.processBlock (audio, midiOn);
        assert (processor.voicePool.countActive() > 0);

        // The default Tape Stop Time is 400 ms: the released voice must be gone well
        // inside a second of blocks.
        juce::MidiBuffer midiOff;
        midiOff.addEvent (juce::MidiMessage::noteOff (1, 60, 0.8f), 0);
        processor.processBlock (audio, midiOff);

        bool freed = false;
        for (int block = 0; block < 60 && ! freed; ++block)   // 60 x 512 = 0.64 s
        {
            audio.clear();
            juce::MidiBuffer none;
            processor.processBlock (audio, none);
            freed = processor.voicePool.countActive() == 0;
        }

        assert (freed);
        pass ("testTapeStopReleaseFreesVoice");
    }

    // Plan 10 phase 3: Attack and Release are real on direct notes.

    /** A direct note-on must run its attack ramp. The old direct path allocated with
        envStart = 0, tens of thousands of samples behind the playhead, so the ramp was
        always finished and ATTACK did nothing. */
    void testAttackShapesDirectNotes()
    {
        auto renderAttack = [&] (float attackMs)
        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            setParameter (processor.apvts, Processor::attackId, attackMs);
            setParameter (processor.apvts, Processor::swingId, 50.0f);   // straight
            setParameter (processor.apvts, Processor::humaniseId, 0.0f);
            setParameter (processor.apvts, Processor::layBackId, 0.0f);
            setParameter (processor.apvts, Processor::regionSpanId, 100.0f);
            setParameter (processor.apvts, Processor::regionLengthId, 100.0f);

            juce::AudioBuffer<float> testBuffer (2, 48000);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 48000; ++i)
                    testBuffer.setSample (ch, i,
                        0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / sampleRate));

            processor.loadedSample = std::make_unique<LoadedSample>();
            processor.loadedSample->buffer = testBuffer;
            processor.loadedSample->sourceSampleRate = sampleRate;
            processor.sampleBuffer.store (&processor.loadedSample->buffer, std::memory_order_release);

            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);

            double sum = 0.0;
            for (int block = 0; block < 3; ++block)   // 1536 samples: inside a 400 ms ramp
            {
                juce::AudioBuffer<float> audio (2, blockSize);
                audio.clear();
                processor.processBlock (audio, midi);
                midi.clear();

                for (int n = 0; n < blockSize; ++n)
                    sum += std::abs (audio.getSample (0, n));
            }

            return (float) (sum / (3.0 * blockSize));
        };

        const float fastRms = renderAttack (1.0f);
        const float slowRms = renderAttack (400.0f);
        std::printf ("attack: fast %.4f, slow %.4f\n", fastRms, slowRms);
        assert (slowRms < 0.5f * fastRms);
        pass ("testAttackShapesDirectNotes");
    }

    /** Release shapes the tail even with the default 400 ms tape stop (decision G2):
        the envelope release and the ramp both run, and the voice ends when both are
        done. The old tape-only branch never set envEnd, so Release did nothing. */
    void testReleaseShapesTapeStopTail()
    {
        auto lifetimeBlocks = [&] (float releaseMs)
        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);
            setParameter (processor.apvts, Processor::releaseId, releaseMs);
            setParameter (processor.apvts, Processor::regionSpanId, 100.0f);
            setParameter (processor.apvts, Processor::regionLengthId, 100.0f);   // the one-shot region end must not cut the tail short
            // Tape Stop Time stays at its 400 ms default.

            juce::AudioBuffer<float> testBuffer (2, 48000);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 48000; ++i)
                    testBuffer.setSample (ch, i,
                        0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / sampleRate));

            processor.loadedSample = std::make_unique<LoadedSample>();
            processor.loadedSample->buffer = testBuffer;
            processor.loadedSample->sourceSampleRate = sampleRate;
            processor.sampleBuffer.store (&processor.loadedSample->buffer, std::memory_order_release);

            juce::AudioBuffer<float> audio (2, blockSize);
            audio.clear();

            juce::MidiBuffer midiOn;
            midiOn.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
            processor.processBlock (audio, midiOn);

            juce::MidiBuffer midiOff;
            midiOff.addEvent (juce::MidiMessage::noteOff (1, 60, 0.8f), 0);
            processor.processBlock (audio, midiOff);

            for (int block = 0; block < 110; ++block)   // 2000 ms is 94 blocks
            {
                audio.clear();
                juce::MidiBuffer none;
                processor.processBlock (audio, none);

                if (processor.voicePool.countActive() == 0)
                    return block + 1;
            }

            return 110;
        };

        const int shortBlocks = lifetimeBlocks (200.0f);
        const int longBlocks = lifetimeBlocks (2000.0f);
        std::printf ("release under tape stop: 200 ms -> %d blocks, 2000 ms -> %d blocks\n",
                     shortBlocks, longBlocks);
        assert (longBlocks >= shortBlocks * 2);
        pass ("testReleaseShapesTapeStopTail");
    }

    /** The waveform view's envelope preview responds to Attack and Release (decision G3). */
    void testEnvelopeOverlayPaints()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);   // loads the embedded default sample

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        assert (editor != nullptr);
        static_cast<SgraffitoEditor*> (editor.get())->setPage (0);

        setParameter (processor.apvts, Processor::attackId, 5.0f);
        setParameter (processor.apvts, Processor::releaseId, 200.0f);
        const auto shortEnvelope = PluginTest::paintComponent (*editor);

        setParameter (processor.apvts, Processor::attackId, 400.0f);
        setParameter (processor.apvts, Processor::releaseId, 4000.0f);
        const auto longEnvelope = PluginTest::paintComponent (*editor);

        const int changed = PluginTest::countChangedPixels (const_cast<juce::Image&> (shortEnvelope),
                                                            const_cast<juce::Image&> (longEnvelope));
        std::printf ("envelope overlay: %d sampled pixels changed\n", changed);
        assert (changed > 0);

        const auto renderDir = juce::SystemStats::getEnvironmentVariable ("SUITE_RENDER_DIR", {});
        if (renderDir.isNotEmpty())
            assert (PluginTest::writeDeterministicPng (*editor,
                                                       juce::File (renderDir).getChildFile ("sgraffito-envelope.png")));

        processor.editorBeingDeleted (editor.get());
        pass ("testEnvelopeOverlayPaints");
    }

    // ------------------------------------------------------------------
    // Plan 07: sample-linked presets and sessions.

    /** Writes a one-second mono WAV of a constant value into the temp folder and returns it. */
    juce::File writeTestWav (const juce::String& name, float value)
    {
        const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("SgraffitoPlan07");
        folder.createDirectory();

        const auto file = folder.getChildFile (name);
        file.deleteFile();

        juce::AudioBuffer<float> buffer (1, 48000);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            buffer.setSample (0, i, value);

        juce::WavAudioFormat format;
        const auto writerOptions = juce::AudioFormatWriterOptions {}
                                       .withSampleRate (sampleRate)
                                       .withNumChannels (1)
                                       .withBitsPerSample (16);
        std::unique_ptr<juce::OutputStream> stream (new juce::FileOutputStream (file));
        std::unique_ptr<juce::AudioFormatWriter> writer (format.createWriterFor (stream, writerOptions));
        assert (writer != nullptr);
        writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
        writer = nullptr;   // close the stream before the file is read back

        assert (file.existsAsFile());
        return file;
    }

    /** A minimal preset delta tree, the way the bank and user presets carry parameters. */
    juce::ValueTree deltaTree (const char* id, double value)
    {
        juce::ValueTree tree ("PARAMETERS");
        juce::ValueTree param ("PARAM");
        param.setProperty ("id", id, nullptr);
        param.setProperty ("value", value, nullptr);
        tree.appendChild (param, nullptr);
        return tree;
    }

    /** Runs a held note through an already-prepared processor, without touching the sample. */
    std::vector<float> renderNoteOn (Processor& processor, int blocks, int key = 60)
    {
        std::vector<float> output;

        for (int block = 0; block < blocks; ++block)
        {
            juce::AudioBuffer<float> audio (2, blockSize);
            audio.clear();
            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, key, 0.8f), 0);

            processor.processBlock (audio, midi);

            for (int n = 0; n < blockSize; ++n)
                output.push_back (audio.getSample (0, n));
        }

        return output;
    }

    /** Pumps the message loop until every background sample load has been installed or
        discarded (plan 07 phase 4). JUCE_MODAL_LOOPS_PERMITTED is 1 in this executable. */
    void waitForSampleLoads (Processor& processor, int timeoutMs = 5000)
    {
        const auto start = juce::Time::getMillisecondCounter();

        while (processor.hasPendingSampleLoad())
        {
            assert (juce::Time::getMillisecondCounter() - start <= (juce::uint32) timeoutMs);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (5);
        }

        // One more tick so the install lambda that cleared the counter is fully done.
        juce::MessageManager::getInstance()->runDispatchLoopUntil (5);
    }

    void testPresetSampleLinkRoundTrip()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        const auto wavFile = writeTestWav ("roundtrip.wav", 0.5f);
        processor.loadSample (wavFile);

        // The property is on the live state at change time (plan 07 C2).
        assert (processor.apvts.state.getProperty (Processor::samplePathProperty).toString()
                == wavFile.getFullPathName());

        SuitePresets::PresetInfo preset;
        preset.name = "Round Trip";
        preset.category = "Test";
        preset.description = "Test";
        preset.plugin = processor.getPresetPluginId();
        preset.isFactory = false;

        const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("SgraffitoPlan07")
                              .getChildFile ("roundtrip.ausp");
        file.deleteFile();

        const auto written = SuitePresets::PresetFile::write (file, preset,
                                                              processor.apvts.copyState(),
                                                              processor.getPresetStateVersion());
        assert (written.ok);

        // formatVersion stays 1: the additive property needs no format bump (plan 07 P10).
        const auto xml = juce::XmlDocument::parse (file);
        assert (xml != nullptr);
        assert (xml->getIntAttribute ("formatVersion") == 1);

        const auto read = SuitePresets::PresetFile::read (file, processor.getPresetPluginId(),
                                                          processor.getPresetStateVersion());
        assert (read.ok);
        assert (read.preset.state.getProperty (Processor::samplePathProperty).toString()
                == wavFile.getFullPathName());
        // A real preset carries parameter children, unlike the empty INIT tree.
        assert (read.preset.state.getNumChildren() > 0);

        pass ("testPresetSampleLinkRoundTrip");
    }

    void testPresetSampleLinkLoads()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        const auto wavFile = writeTestWav ("loaded.wav", 0.5f);

        auto tree = deltaTree (Processor::modeId, 2.0);
        tree.setProperty (Processor::samplePathProperty, wavFile.getFullPathName(), nullptr);
        processor.applyPresetState (tree);

        // A file sample loads in the background now (plan 07 phase 4): the handoff has
        // not run because the message loop has not been pumped.
        assert (processor.hasPendingSampleLoad());
        waitForSampleLoads (processor);

        assert (processor.hasSample());
        assert (processor.getSamplePath() == wavFile.getFullPathName());
        assert (processor.getParamValue (Processor::modeId) == 2.0f);

        const auto output = renderNoteOn (processor, 10);
        float peak = 0.0f;
        for (auto sample : output)
            if (std::abs (sample) > peak)
                peak = std::abs (sample);
        std::printf ("sample-linked preset: peak %.4f\n", peak);
        assert (peak > 0.2f);

        pass ("testPresetSampleLinkLoads");
    }

    void testPresetWithoutSampleLeavesSample()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        const auto wavFile = writeTestWav ("kept.wav", 0.5f);
        processor.loadSample (wavFile);

        // Delta semantics: a preset without the property does not speak about samples.
        processor.applyPresetState (deltaTree (Processor::modeId, 2.0));

        assert (processor.getParamValue (Processor::modeId) == 2.0f);
        assert (processor.getSamplePath() == wavFile.getFullPathName());
        assert (processor.loadedSample->sourcePath == wavFile.getFullPathName());
        assert (! processor.isSampleMissing());

        pass ("testPresetWithoutSampleLeavesSample");
    }

    void testPresetEmbeddedSampleUri()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        const auto wavFile = writeTestWav ("user.wav", 0.5f);
        processor.loadSample (wavFile);
        assert (processor.getSamplePath() == wavFile.getFullPathName());

        auto tree = deltaTree (Processor::modeId, 1.0);
        tree.setProperty (Processor::samplePathProperty, Processor::embeddedSampleUri, nullptr);
        processor.applyPresetState (tree);

        assert (processor.loadedSample->sourcePath == Processor::embeddedSampleUri);
        assert (processor.getSamplePath().isEmpty());   // embedded samples have no file path
        assert (processor.apvts.state.getProperty (Processor::samplePathProperty).toString()
                == Processor::embeddedSampleUri);
        assert (! processor.isSampleMissing());

        pass ("testPresetEmbeddedSampleUri");
    }

    void testInitResetsSample()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        const auto wavFile = writeTestWav ("before-init.wav", 0.5f);
        processor.loadSample (wavFile);
        assert (processor.getSamplePath() == wavFile.getFullPathName());

        // INIT applies a hand-made empty tree (the bar and host program 0): the sample
        // resets to the embedded default like every other parameter (plan 07 Q4).
        processor.applyPresetState (juce::ValueTree ("PARAMETERS"));

        assert (processor.loadedSample->sourcePath == Processor::embeddedSampleUri);
        assert (! processor.isSampleMissing());

        pass ("testInitResetsSample");
    }

    void testPresetMissingSampleKeepsCurrent()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        const auto wavFile = writeTestWav ("current.wav", 0.5f);
        processor.loadSample (wavFile);

        const auto missing = wavFile.getSiblingFile ("deleted.wav").getFullPathName();

        auto tree = deltaTree (Processor::modeId, 1.0);
        tree.setProperty (Processor::samplePathProperty, missing, nullptr);
        processor.applyPresetState (tree);

        // Keep the current sample playing and report the missing one (plan 07 Q2).
        assert (processor.getSamplePath() == wavFile.getFullPathName());
        assert (processor.isSampleMissing());
        assert (processor.getMissingSamplePath() == missing);
        // The parameters of the preset still applied.
        assert (processor.getParamValue (Processor::modeId) == 1.0f);

        // A later apply with a valid path clears the flag.
        auto valid = deltaTree (Processor::modeId, 2.0);
        valid.setProperty (Processor::samplePathProperty, wavFile.getFullPathName(), nullptr);
        processor.applyPresetState (valid);
        waitForSampleLoads (processor);

        assert (! processor.isSampleMissing());
        assert (processor.getSamplePath() == wavFile.getFullPathName());

        pass ("testPresetMissingSampleKeepsCurrent");
    }

    void testSessionRestoresSample()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        const auto wavFile = writeTestWav ("session.wav", 0.5f);
        processor.loadSample (wavFile);

        juce::MemoryBlock block;
        processor.getStateInformation (block);

        Processor restored;
        restored.prepareToPlay (sampleRate, blockSize);
        restored.setStateInformation (block.getData(), (int) block.getSize());
        waitForSampleLoads (restored);

        assert (restored.hasSample());
        assert (restored.getSamplePath() == wavFile.getFullPathName());
        assert (restored.loadedSample->sourcePath == wavFile.getFullPathName());
        assert (! restored.isSampleMissing());

        pass ("testSessionRestoresSample");
    }

    // ------------------------------------------------------------------
    // Plan 07 phase 2: fallback search, session UX, fixture anchor.

    /** Applies a preset whose stored path does not exist, but whose file name sits in the
        search folder: the fallback loads it and the session remembers the resolution. */
    void testSampleFolderFallback()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("SgraffitoPlan07")
                                .getChildFile ("SampleFolder");
        folder.createDirectory();

        // writeTestWav lands in the temp root; move the file into the search folder.
        const auto libraryFile = folder.getChildFile ("library-hit.wav");
        assert (writeTestWav ("library-hit.wav", 0.4f).moveFileTo (libraryFile));
        assert (libraryFile.existsAsFile());

        processor.sampleSearchFolder = folder;

        // The stored path does not exist; the same name in the search folder does.
        const auto stored = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("SgraffitoPlan07")
                                .getChildFile ("moved-library")
                                .getChildFile ("library-hit.wav");
        assert (! stored.existsAsFile());

        auto tree = deltaTree (Processor::modeId, 2.0);
        tree.setProperty (Processor::samplePathProperty, stored.getFullPathName(), nullptr);
        processor.applyPresetState (tree);
        waitForSampleLoads (processor);

        assert (! processor.isSampleMissing());
        assert (processor.getSamplePath() == libraryFile.getFullPathName());
        assert (processor.loadedSample->sourcePath == libraryFile.getFullPathName());
        // The resolved path replaces the dead one on the state, so the session remembers it.
        assert (processor.apvts.state.getProperty (Processor::samplePathProperty).toString()
                == libraryFile.getFullPathName());
        assert (processor.getParamValue (Processor::modeId) == 2.0f);

        // Stored-path priority: an existing path beats the same name in the folder.
        const auto direct = writeTestWav ("direct.wav", 0.6f);
        assert (! folder.getChildFile ("direct.wav").existsAsFile());   // keep the folders apart

        auto directTree = deltaTree (Processor::modeId, 1.0);
        directTree.setProperty (Processor::samplePathProperty, direct.getFullPathName(), nullptr);
        processor.applyPresetState (directTree);
        waitForSampleLoads (processor);

        assert (! processor.isSampleMissing());
        assert (processor.getSamplePath() == direct.getFullPathName());

        // No name match anywhere: the missing report, never a silent substitution.
        auto lostTree = deltaTree (Processor::modeId, 0.0);
        lostTree.setProperty (Processor::samplePathProperty,
                              stored.getSiblingFile ("unfindable.wav").getFullPathName(), nullptr);
        processor.applyPresetState (lostTree);

        assert (processor.isSampleMissing());
        assert (processor.getMissingSamplePath().endsWith ("unfindable.wav"));
        assert (processor.getSamplePath() == direct.getFullPathName());

        pass ("testSampleFolderFallback");
    }

    /** Alternates a valid and a missing preset while audio runs: the retire pattern must
        survive the new trigger path with no use-after-free (plan 07 test 7). */
    void testRapidPresetSwitchingUnderAudio()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        const auto wavFile = writeTestWav ("rapid.wav", 0.5f);
        const auto missing = wavFile.getSiblingFile ("vanished.wav").getFullPathName();

        auto valid = deltaTree (Processor::modeId, 2.0);
        valid.setProperty (Processor::samplePathProperty, wavFile.getFullPathName(), nullptr);

        auto broken = deltaTree (Processor::modeId, 1.0);
        broken.setProperty (Processor::samplePathProperty, missing, nullptr);

        const auto peakOf = [] (const std::vector<float>& samples)
        {
            float peak = 0.0f;
            for (float sample : samples)
                peak = juce::jmax (peak, std::abs (sample));
            return peak;
        };

        for (int round = 0; round < 24; ++round)
        {
            // The valid preset loads in the background: pump the handoff before the
            // note must sound from it (plan 07 phase 4).
            processor.applyPresetState (valid);
            waitForSampleLoads (processor);
            assert (! processor.isSampleMissing());
            assert (peakOf (renderNoteOn (processor, 3)) > 0.05f);

            processor.applyPresetState (broken);
            assert (processor.isSampleMissing());
            assert (peakOf (renderNoteOn (processor, 3)) > 0.05f);   // the previous sample keeps playing
        }

        // Back to a valid preset: the flag clears and the sample loads again.
        processor.applyPresetState (valid);
        waitForSampleLoads (processor);
        assert (! processor.isSampleMissing());
        assert (processor.getSamplePath() == wavFile.getFullPathName());

        pass ("testRapidPresetSwitchingUnderAudio");
    }

    /** A format-1 preset with no samplePath attribute (the shape every preset saved before
        plan 07 has) loads exactly as before: parameters apply, the sample is untouched. */
    void testOldPresetCompatibility()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        const auto wavFile = writeTestWav ("compat.wav", 0.5f);
        processor.loadSample (wavFile);

        const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("SgraffitoPlan07")
                              .getChildFile ("old-preset.ausp");
        file.replaceWithText (R"(<AudioSuitePreset formatVersion="1" plugin="Sgraffito" pluginVersion="1.0.0">
    <Meta name="Old" category="Legacy" description="saved before plan 07" author="FROTH Audio"/>
    <PARAMETERS suiteStateVersion="1">
        <PARAM id="mode" value="2.0"/>
    </PARAMETERS>
</AudioSuitePreset>)");

        const auto read = SuitePresets::PresetFile::read (file, processor.getPresetPluginId(),
                                                          processor.getPresetStateVersion());
        assert (read.ok);
        assert (read.preset.state.getProperty (Processor::samplePathProperty).toString().isEmpty());

        processor.applyPresetState (read.preset.state);

        assert (processor.getParamValue (Processor::modeId) == 2.0f);
        assert (processor.getSamplePath() == wavFile.getFullPathName());
        assert (processor.loadedSample->sourcePath == wavFile.getFullPathName());
        assert (! processor.isSampleMissing());

        pass ("testOldPresetCompatibility");
    }

    /** The missing-sample overlay renders on the SOUND page, and differs from the normal
        render. Writes sgraffito-missing.png when SUITE_RENDER_DIR is set (suite rule R9). */
    void testMissingSampleOverlayPaints()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        assert (editor != nullptr);
        static_cast<SgraffitoEditor*> (editor.get())->setPage (0);

        const auto normal = PluginTest::paintComponent (*editor);

        // The flag the way a missing preset path sets it.
        processor.missingSamplePath = "C:/nowhere/gone.wav";
        processor.sampleMissing.store (true, std::memory_order_release);

        const auto flagged = PluginTest::paintComponent (*editor);
        const int changed = PluginTest::countChangedPixels (const_cast<juce::Image&> (normal),
                                                            const_cast<juce::Image&> (flagged));
        std::printf ("missing overlay: %d sampled pixels changed\n", changed);
        assert (changed > 0);

        const auto renderDir = juce::SystemStats::getEnvironmentVariable ("SUITE_RENDER_DIR", {});
        if (renderDir.isNotEmpty())
            assert (PluginTest::writeDeterministicPng (*editor,
                                                       juce::File (renderDir).getChildFile ("sgraffito-missing.png")));

        processor.sampleMissing.store (false, std::memory_order_release);
        processor.editorBeingDeleted (editor.get());
        pass ("testMissingSampleOverlayPaints");
    }

    /** Plan 10 phase 6 (decision G6): the sample switch-back rule, pinned on the code
        path the bank uses. A preset that carries a sample switches back to it even after
        a user sample was loaded; a preset without one keeps the current sample (delta
        semantics); INIT resets to the embedded factory default. */
    void testPresetSampleSwitchBack()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        const auto userWav = writeTestWav ("switch-back.wav", 0.5f);

        // Preset A: an embedded sample, exactly as the factory bank carries it.
        auto presetA = deltaTree (Processor::modeId, 2.0);
        presetA.setProperty (Processor::samplePathProperty, "embedded:grain_4k.wav", nullptr);
        processor.applyPresetState (presetA);
        assert (processor.loadedSample->sourcePath == "embedded:grain_4k.wav");

        // The user loads their own sample over the preset's one.
        processor.loadSample (userWav);
        assert (processor.loadedSample->sourcePath == userWav.getFullPathName());

        // Re-selecting preset A switches the sample back.
        processor.applyPresetState (presetA);
        assert (processor.loadedSample->sourcePath == "embedded:grain_4k.wav");

        // A preset without a sample reference keeps the current sample (delta semantics).
        processor.loadSample (userWav);
        processor.applyPresetState (deltaTree (Processor::modeId, 1.0));
        assert (processor.loadedSample->sourcePath == userWav.getFullPathName());

        // INIT (the empty tree) resets to the embedded factory default.
        processor.applyPresetState (juce::ValueTree ("PARAMETERS"));
        assert (processor.loadedSample->sourcePath == Processor::embeddedSampleUri);

        pass ("testPresetSampleSwitchBack");
    }

    /** Two background loads raced: the latest request wins and the older install is
        discarded (plan 07 phase 4). */
    void testAsyncLoadLatestWins()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        const auto first = writeTestWav ("async-a.wav", 0.5f);
        const auto second = writeTestWav ("async-b.wav", 0.5f);

        auto treeA = deltaTree (Processor::modeId, 1.0);
        treeA.setProperty (Processor::samplePathProperty, first.getFullPathName(), nullptr);
        processor.applyPresetState (treeA);

        auto treeB = deltaTree (Processor::modeId, 2.0);
        treeB.setProperty (Processor::samplePathProperty, second.getFullPathName(), nullptr);
        processor.applyPresetState (treeB);

        waitForSampleLoads (processor);

        assert (! processor.isSampleMissing());
        assert (processor.getSamplePath() == second.getFullPathName());
        assert (processor.loadedSample->sourcePath == second.getFullPathName());
        // The parameters applied regardless of the sample race.
        assert (processor.getParamValue (Processor::modeId) == 2.0f);
        assert (! processor.hasPendingSampleLoad());

        pass ("testAsyncLoadLatestWins");
    }

    /** A processor destroyed while a load is in flight: the pool waits or interrupts the
        job, and a queued handoff lambda must notice the dead token (plan 07 phase 4). */
    void testAsyncLoadSurvivesDestruction()
    {
        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);

            const auto wavFile = writeTestWav ("destroyed.wav", 0.5f);
            auto tree = deltaTree (Processor::modeId, 1.0);
            tree.setProperty (Processor::samplePathProperty, wavFile.getFullPathName(), nullptr);
            processor.applyPresetState (tree);
            // The scope ends with the load still pending: no pump happened.
        }

        // Drain any queued handoff: the token must be dead, the lambda a no-op.
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

        pass ("testAsyncLoadSurvivesDestruction");
    }

    //==============================================================================
    // Plan 09: the capped sample retirement, the unified note paths, the preset identity.

    /** Plan 09 1.1: the capped retirement queue. Sync loads install through the same
        finishSampleLoad path as the async ones, so a second install while a voice from
        the previous sample is still playing leaves that buffer alive in the queue —
        the exact window where the old single retire slot freed it. The queue never
        grows past the cap. */
    void testRetirementQueue()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        const auto wavA = writeTestWav ("retire-a.wav", 0.5f);
        const auto wavB = writeTestWav ("retire-b.wav", 0.5f);
        const auto wavC = writeTestWav ("retire-c.wav", 0.5f);
        const auto wavD = writeTestWav ("retire-d.wav", 0.4f);

        juce::AudioBuffer<float> audio (2, blockSize);
        audio.clear();

        processor.loadSample (wavA);

        // A voice playing A: the block that must survive the next two installs.
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        processor.processBlock (audio, midi);
        assert (processor.voicePool.countActive() > 0);

        processor.loadSample (wavB);
        assert ((int) processor.retiredSamples.size() <= Processor::retiredSampleGenerations);

        // The exact window: a second install while the voice from A still plays. The
        // old single slot destroyed A here; the queue keeps it alive.
        processor.loadSample (wavC);
        assert ((int) processor.retiredSamples.size() == Processor::retiredSampleGenerations);
        assert (processor.retiredSamples.front()->sourcePath == wavA.getFullPathName());

        for (int block = 0; block < 8; ++block)
        {
            audio.clear();
            juce::MidiBuffer none;
            processor.processBlock (audio, none);
        }

        processor.loadSample (wavD);
        assert ((int) processor.retiredSamples.size() == Processor::retiredSampleGenerations);
        assert (processor.retiredSamples.front()->sourcePath == wavB.getFullPathName());

        for (int block = 0; block < 8; ++block)
        {
            audio.clear();
            juce::MidiBuffer none;
            processor.processBlock (audio, none);
        }

        pass ("testRetirementQueue");
    }

    /** Plan 09 1.3 / owner decision D1: a pending (delayed) note fires through
        VoicePool::allocateVoice, so with all 32 voices busy it steals the oldest active
        voice instead of being silently dropped. */
    void testPendingNoteSteals()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        // Straight timing for the fill notes, then a lay-back for the pending one.
        setParameter (processor.apvts, Processor::swingId, 50.0f);
        setParameter (processor.apvts, Processor::humaniseId, 0.0f);
        setParameter (processor.apvts, Processor::layBackId, 0.0f);

        juce::AudioBuffer<float> testBuffer (2, 48000);
        processor.loadedSample = std::make_unique<LoadedSample>();
        processor.loadedSample->buffer = testBuffer;
        processor.loadedSample->sourceSampleRate = sampleRate;
        processor.sampleBuffer.store (&processor.loadedSample->buffer, std::memory_order_release);

        juce::AudioBuffer<float> audio (2, blockSize);
        audio.clear();

        // Fill every voice with a held note of its own key.
        juce::MidiBuffer fill;
        for (int key = 36; key < 36 + CoreDSP::Sampler::maxVoices; ++key)
            fill.addEvent (juce::MidiMessage::noteOn (1, key, 0.8f), 0);

        processor.processBlock (audio, fill);
        assert (processor.voicePool.countActive() == CoreDSP::Sampler::maxVoices);

        // A delayed note: 30 ms (the parameter's maximum) is 1440 samples at 48 kHz.
        setParameter (processor.apvts, Processor::layBackId, 30.0f);

        juce::MidiBuffer delayed;
        delayed.addEvent (juce::MidiMessage::noteOn (1, 72, 0.8f), 0);
        processor.processBlock (audio, delayed);
        assert (processor.pendingNotes.size() == 1);

        // Run until it fires. Voices are one-shots that end at their region end, so the
        // scan below must happen in the same block the note fired, not after a fixed
        // number of blocks.
        int firedBlock = -1;

        for (int block = 0; block < 8 && firedBlock < 0; ++block)
        {
            audio.clear();
            juce::MidiBuffer none;
            processor.processBlock (audio, none);

            if (processor.pendingNotes.empty())
                firedBlock = block;
        }

        assert (firedBlock >= 0);

        // The pending note stole a voice: it plays, the pool stayed full, and the oldest
        // fill note (key 36) is the one that was replaced.
        bool hasPendingKey = false, hasStolenKey = false;

        for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
        {
            if (! processor.voicePool.voices[i].active)
                continue;

            if (processor.voicePool.voices[i].key == 72)
                hasPendingKey = true;

            if (processor.voicePool.voices[i].key == 36)
                hasStolenKey = true;
        }

        assert (hasPendingKey);
        assert (! hasStolenKey);
        assert (processor.voicePool.countActive() == CoreDSP::Sampler::maxVoices);

        pass ("testPendingNoteSteals");
    }

    /** Plan 09 1.3: a delayed Cloud note allocates its four lanes through the pool, like
        a directly played one (the old hand-rolled path allocated a single voice). */
    void testPendingCloudNoteLanes()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        setParameter (processor.apvts, Processor::modeId, 2.0f);   // Cloud
        setParameter (processor.apvts, Processor::swingId, 50.0f);
        setParameter (processor.apvts, Processor::humaniseId, 0.0f);
        setParameter (processor.apvts, Processor::layBackId, 30.0f);

        juce::AudioBuffer<float> testBuffer (2, 48000);
        processor.loadedSample = std::make_unique<LoadedSample>();
        processor.loadedSample->buffer = testBuffer;
        processor.loadedSample->sourceSampleRate = sampleRate;
        processor.sampleBuffer.store (&processor.loadedSample->buffer, std::memory_order_release);

        juce::AudioBuffer<float> audio (2, blockSize);
        audio.clear();

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        processor.processBlock (audio, midi);

        assert (processor.voicePool.countActive() == 0);   // not due yet: it is pending

        // Run until it fires, then scan in the same block (voices are one-shots).
        int firedBlock = -1;

        for (int block = 0; block < 8 && firedBlock < 0; ++block)
        {
            audio.clear();
            juce::MidiBuffer none;
            processor.processBlock (audio, none);

            if (processor.pendingNotes.empty())
                firedBlock = block;
        }

        assert (firedBlock >= 0);
        assert (processor.voicePool.countActive() == CoreDSP::Sampler::cloudLanesPerNote);

        int lanes[CoreDSP::Sampler::cloudLanesPerNote] = {};
        for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
        {
            const auto& voice = processor.voicePool.voices[i];
            if (voice.active)
            {
                assert (voice.key == 60);
                assert (voice.cloudLane >= 0 && voice.cloudLane < CoreDSP::Sampler::cloudLanesPerNote);
                ++lanes[voice.cloudLane];
            }
        }

        for (int lane = 0; lane < CoreDSP::Sampler::cloudLanesPerNote; ++lane)
            assert (lanes[lane] == 1);   // one voice per lane

        pass ("testPendingCloudNoteLanes");
    }

    /** Plan 09 1.3: a delayed note and the same note played direct get the same pitch,
        region and start position — all three paths share setupAllocatedVoice and
        VoicePool::allocateVoice's region maths. */
    void testPendingMatchesDirect()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        setParameter (processor.apvts, Processor::swingId, 50.0f);   // straight: zero swung delay
        setParameter (processor.apvts, Processor::humaniseId, 0.0f);
        setParameter (processor.apvts, Processor::pitchFineId, 25.0f);   // off-default, so rate is meaningful

        juce::AudioBuffer<float> testBuffer (2, 48000);
        processor.loadedSample = std::make_unique<LoadedSample>();
        processor.loadedSample->buffer = testBuffer;
        processor.loadedSample->sourceSampleRate = sampleRate;
        processor.sampleBuffer.store (&processor.loadedSample->buffer, std::memory_order_release);

        juce::AudioBuffer<float> audio (2, blockSize);
        audio.clear();

        // The direct note: fires at once. Capture its fields before it starts playing,
        // while they still hold the allocation-time values.
        setParameter (processor.apvts, Processor::layBackId, 0.0f);
        juce::MidiBuffer direct;
        direct.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        processor.processBlock (audio, direct);

        int directIdx = -1;

        for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
            if (processor.voicePool.voices[i].active && processor.voicePool.voices[i].key == 60)
            {
                directIdx = i;
                break;
            }

        assert (directIdx >= 0);

        const auto directStartOrder = processor.voicePool.voices[directIdx].startOrder;
        const double directRate = processor.voicePool.voices[directIdx].rate;
        const double directRegionStart = processor.voicePool.voices[directIdx].regionStart;
        const double directRegionEnd = processor.voicePool.voices[directIdx].regionEnd;
        const double directPosition = processor.voicePool.voices[directIdx].position;
        const float directVelocity = processor.voicePool.voices[directIdx].velocity;

        // The same note, delayed through the pending queue. Run until it fires, then
        // scan in the same block (voices are one-shots).
        setParameter (processor.apvts, Processor::layBackId, 30.0f);
        juce::MidiBuffer delayed;
        delayed.addEvent (juce::MidiMessage::noteOn (1, 60, 0.7f), 0);
        processor.processBlock (audio, delayed);

        int firedBlock = -1;

        for (int block = 0; block < 8 && firedBlock < 0; ++block)
        {
            audio.clear();
            juce::MidiBuffer none;
            processor.processBlock (audio, none);

            if (processor.pendingNotes.empty())
                firedBlock = block;
        }

        assert (firedBlock >= 0);

        // The pending note's voice: same key, newer start order.
        int pendingIdx = -1;

        for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
        {
            const auto& voice = processor.voicePool.voices[i];
            if (voice.active && voice.key == 60 && voice.startOrder > directStartOrder)
            {
                pendingIdx = i;
                break;
            }
        }

        assert (pendingIdx >= 0);

        const auto& pendingVoice = processor.voicePool.voices[pendingIdx];

        assert (std::abs (directRate - pendingVoice.rate) < 1.0e-9);
        assert (std::abs (directRegionStart - pendingVoice.regionStart) < 1.0e-9);
        assert (std::abs (directRegionEnd - pendingVoice.regionEnd) < 1.0e-9);

        // Both voices start at their region's start — the direct one captured after one
        // block of playback, the pending one after the block it fired in. The playback
        // loop runs per channel (committed behaviour: a stereo buffer advances a voice
        // once per channel and sample), so the bound is channels × blockSize × rate.
        // The plan 05.1 bug this guards against was every note reading from sample 0.
        const double blockAdvance = 2.0 * blockSize * directRate + 1.0e-9;
        assert (std::abs (directPosition - directRegionStart * 48000.0) <= blockAdvance);
        assert (std::abs (pendingVoice.position - pendingVoice.regionStart * 48000.0) <= blockAdvance);

        // The velocity curve applies to both, from each note's own velocity. MIDI
        // velocities quantise to 1/127, so the expectation reads the value back from
        // the same messages (0.8f → 102/127, 0.7f → 89/127).
        const float curve = juce::jmax (0.1f, processor.getParamValue (Processor::velocityCurveId));
        const float directMidiVel = juce::MidiMessage::noteOn (1, 60, 0.8f).getVelocity() / 127.0f;
        const float pendingMidiVel = juce::MidiMessage::noteOn (1, 60, 0.7f).getVelocity() / 127.0f;
        assert (std::abs (directVelocity - std::pow (directMidiVel, curve)) < 1.0e-4f);
        assert (std::abs (pendingVoice.velocity - std::pow (pendingMidiVel, curve)) < 1.0e-4f);

        pass ("testPendingMatchesDirect");
    }

    /** Plan 09 1.4 / D3: the preset identity persists as the presetSelection state
        property and survives a session round trip. A session without the property —
        every session saved before plan 09 — reads as Init. Exercises the StateHolder
        seam directly, so it runs without the factory bank too. */
    void testPresetSelectionSessionRoundTrip()
    {
        SuitePresets::PresetSelection factory;
        factory.kind = SuitePresets::PresetSelection::Kind::factory;
        factory.factoryIndex = 0;

        // A factory selection round-trips through the session bytes.
        juce::MemoryBlock blob;

        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);

            processor.setPresetSelection (factory);
            assert (processor.getPresetSelection() == factory);
            assert (processor.apvts.state.getProperty (Processor::presetSelectionProperty).toString()
                    == "factory:0");

            processor.getStateInformation (blob);
            assert (blob.getSize() > 0);
        }

        {
            Processor recalled;
            recalled.prepareToPlay (sampleRate, blockSize);
            recalled.setStateInformation (blob.getData(), (int) blob.getSize());
            assert (recalled.getPresetSelection() == factory);

            // Init clears it, in memory and on the state.
            recalled.setPresetSelection (SuitePresets::PresetSelection {});
            assert (! recalled.apvts.state.hasProperty (Processor::presetSelectionProperty));
        }

        // A user-file identity round-trips through the property text form (paths may
        // contain spaces and colons).
        juce::MemoryBlock userBlob;

        {
            Processor processor;
            processor.prepareToPlay (sampleRate, blockSize);

            SuitePresets::PresetSelection user;
            user.kind = SuitePresets::PresetSelection::Kind::userFile;
            user.userFilePath = "C:/Users/Some One/Presets/my preset.ausp";
            processor.setPresetSelection (user);
            assert (processor.getPresetSelection() == user);

            processor.getStateInformation (userBlob);
        }

        {
            Processor recalled;
            recalled.prepareToPlay (sampleRate, blockSize);
            recalled.setStateInformation (userBlob.getData(), (int) userBlob.getSize());

            const auto selection = recalled.getPresetSelection();
            assert (selection.kind == SuitePresets::PresetSelection::Kind::userFile);
            assert (selection.userFilePath == "C:/Users/Some One/Presets/my preset.ausp");
        }

        // A session saved without the property (before plan 09) reads as Init.
        juce::MemoryBlock oldBlob;

        {
            Processor fresh;
            fresh.prepareToPlay (sampleRate, blockSize);
            fresh.getStateInformation (oldBlob);
        }

        {
            Processor recalled;
            recalled.prepareToPlay (sampleRate, blockSize);
            recalled.setStateInformation (oldBlob.getData(), (int) oldBlob.getSize());
            assert (recalled.getPresetSelection() == SuitePresets::PresetSelection {});
        }

        pass ("testPresetSelectionSessionRoundTrip");
    }

    /** Loads the recorded state fixture (skips without SUITE_FIXTURE_DIR) and checks the
        samplePath anchor recorded in it (plan 07 test 13). */
    void testLoadsStateFixture()
    {
        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        assert (StateFixtures::loadsRecordedState (processor, "sgraffito"));
        waitForSampleLoads (processor);   // a resolved path loads in the background now

        const auto fixtureDir = juce::SystemStats::getEnvironmentVariable ("SUITE_FIXTURE_DIR", {});

        if (fixtureDir.isNotEmpty())
        {
            juce::MemoryBlock blob;
            assert (juce::File (fixtureDir).getChildFile ("sgraffito_v1.xml").loadFileAsData (blob));
            assert (blob.getSize() > 0);

            const auto xml = SuiteState::detail::decodeXml (blob.getData(), (int) blob.getSize());
            assert (xml != nullptr);

            // The rule the fixture anchors: the recorded samplePath must have been applied
            // by the state read - loaded, resolved, or reported missing (plan 07 2.3).
            const auto recorded = juce::ValueTree::fromXml (*xml)
                                      .getProperty (Processor::samplePathProperty).toString();
            const auto loaded = processor.apvts.state.getProperty (Processor::samplePathProperty).toString();

            if (recorded.isEmpty())
                assert (loaded.isEmpty() || loaded == Processor::embeddedSampleUri);
            else if (recorded.startsWith ("embedded:") || juce::File (recorded).existsAsFile())
                assert (loaded == recorded);
            else
                assert (processor.isSampleMissing() && processor.getMissingSamplePath() == recorded);
        }

        pass ("testLoadsStateFixture");
    }
}

int main (int argc, char** argv)
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    juce::ScopedJuceInitialiser_GUI gui;

    // Records a state fixture for a future suiteStateVersion bump (plan 07 test 13):
    // <dir>/sgraffito_v1.xml is the state as getStateInformation writes it, and
    // sgraffito_v1.values.txt holds one "parameterID normalisedValue" line per
    // parameter, the format StateFixtures::loadsRecordedState reads.
    if (argc >= 2)
    {
        const juce::File dir (argv[1]);
        dir.createDirectory();

        Processor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        processor.loadSample (writeTestWav ("fixture.wav", 0.5f));

        juce::MemoryBlock blob;
        processor.getStateInformation (blob);
        dir.getChildFile ("sgraffito_v1.xml").replaceWithData (blob.getData(), blob.getSize());

        juce::String values;
        for (auto* parameter : processor.getParameters())
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
                values << withId->paramID << " " << juce::String (withId->getValue(), 7) << "\n";
        dir.getChildFile ("sgraffito_v1.values.txt").replaceWithText (values);

        std::printf ("fixture written to %s\n", dir.getFullPathName().toRawUTF8());
        return 0;
    }

    std::puts ("Running Sgraffito plugin tests...");

    testParameters();
    testStateRoundTrip();
    testMidiNoteOnAllocatesVoice();
    testMidiNoteOffReleasesVoice();
    testSwingDelaysRepeatedNotes();
    testHumaniseDelaysNoteOnset();
    testGhostDelaysQuietNote();
    testNoGhostWithoutQuietNote();
    testRegionMapping();
    testChopModeRegionMapping();
    testChopModeSliceCoverage();
    testVoiceProducesOutput();
    testRenderBaseline();  // Move this up - needs GUI context
    testEditor();
    testPageVisibility();
    testLoadButtonSitsTopRight();
    testModeAffinity();
    testStatusBarShowsHelp();
    testModulationParameters();
    testDefaultRoutingIsSilent();
    testModulationReachesAudio();
    testModulationStaysInRange();
    testModesProduceDifferentAudio();
    testKeyChangesPitch();
    testRootKeyTransposes();
    testScrubPlaysAtSampleRate();
    testCloudSustainsInStereo();
    testTapeStopReleaseFreesVoice();
    testAttackShapesDirectNotes();
    testReleaseShapesTapeStopTail();
    testEnvelopeOverlayPaints();
    testPresetSampleLinkRoundTrip();
    testPresetSampleLinkLoads();
    testPresetWithoutSampleLeavesSample();
    testPresetEmbeddedSampleUri();
    testInitResetsSample();
    testPresetMissingSampleKeepsCurrent();
    testSessionRestoresSample();
    testSampleFolderFallback();
    testRapidPresetSwitchingUnderAudio();
    testOldPresetCompatibility();
    testAsyncLoadLatestWins();
    testPresetSampleSwitchBack();
    testAsyncLoadSurvivesDestruction();
    testRetirementQueue();
    testPendingNoteSteals();
    testPendingCloudNoteLanes();
    testPendingMatchesDirect();
    testPresetSelectionSessionRoundTrip();
    testMissingSampleOverlayPaints();
    testLoadsStateFixture();

    std::puts ("All Sgraffito plugin tests passed.");
    return 0;
}
