// Loads a built VST3 bundle through JUCE's own plugin host, the same way a DAW does:
// scan, instantiate, open the editor, prepare, and process audio and MIDI.
//
// Usage: PluginHostTests <path-to-.vst3-bundle>
// Exit code 0 means every step succeeded.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdio>

namespace
{
    int fail (const char* step, const juce::String& detail = {})
    {
        std::printf ("[FAIL] %s%s%s\n", step, detail.isEmpty() ? "" : ": ", detail.toRawUTF8());
        return 1;
    }
}

int main (int argc, char** argv)
{
    if (argc < 2)
        return fail ("usage", "PluginHostTests <bundle>");

    const juce::File bundle { juce::String (argv[1]) };

    if (! bundle.exists())
        return fail ("bundle", "does not exist: " + bundle.getFullPathName());

    // The editor needs the GUI toolkit initialised, as it is inside a DAW.
    juce::ScopedJuceInitialiser_GUI gui;

    juce::AudioPluginFormatManager formats;
    // JUCE 9 removed AudioPluginFormatManager::addDefaultFormats(). The host only needs VST3.
    formats.addFormat (std::make_unique<juce::VST3PluginFormat>());

    juce::OwnedArray<juce::PluginDescription> types;

    for (int i = 0; i < formats.getNumFormats(); ++i)
    {
        auto* format = formats.getFormat (i);

        if (format->getName() == "VST3")
            format->findAllTypesForFile (types, bundle.getFullPathName());
    }

    if (types.isEmpty())
        return fail ("scan", "no plugin types found in bundle");

    const auto& description = *types.getFirst();
    std::printf ("[INFO] %s | category=%s | audio in=%d out=%d | instrument=%d\n",
                 description.name.toRawUTF8(), description.category.toRawUTF8(),
                 description.numInputChannels, description.numOutputChannels,
                 description.isInstrument ? 1 : 0);

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;
    juce::String error;

    auto instance = formats.createPluginInstance (description, sampleRate, blockSize, error);

    if (instance == nullptr)
        return fail ("instantiate", error);

    std::printf ("[INFO] buses: audio in=%d out=%d | accepts MIDI=%d | produces MIDI=%d\n",
                 instance->getTotalNumInputChannels(), instance->getTotalNumOutputChannels(),
                 instance->acceptsMidi() ? 1 : 0, instance->producesMidi() ? 1 : 0);

    if (instance->hasEditor() && juce::String (argv[argc - 1]) != "noeditor")
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (instance->createEditorAndMakeActive());

        if (editor == nullptr)
            return fail ("editor", "createEditorAndMakeActive returned nullptr");

        std::printf ("[INFO] editor created: %d x %d\n", editor->getWidth(), editor->getHeight());
    }

    instance->prepareToPlay (sampleRate, blockSize);

    // The host hands a buffer that matches the OUTPUT buses (an instrument has no inputs):
    // a 1-channel buffer against a 2-output plugin renders nothing the host can read.
    juce::AudioBuffer<float> audio (juce::jmax (instance->getTotalNumOutputChannels(),
                                                instance->getTotalNumInputChannels()), blockSize);
    audio.clear();

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    midi.addEvent (juce::MidiMessage::noteOff (1, 60), 256);

    float peak = 0.0f;

    for (int block = 0; block < 8; ++block)
    {
        if (block == 0)
            instance->processBlock (audio, midi);
        else
        {
            juce::MidiBuffer empty;
            instance->processBlock (audio, empty);
        }

        peak = juce::jmax (peak, audio.getMagnitude (0, 0, blockSize));
    }

    // Plan 14: Cyclical is an instrument that plays sound on MIDI 60 with nothing else
    // set up (the embedded loops load in the constructor) - a DAW does exactly what this
    // test does, so silence here means the plugin cannot make sound.
    if (description.name == "Cyclical")
    {
        std::printf ("[INFO] Cyclical peak after a note-on: %.4f\n", (double) peak);

        if (peak <= 0.01f)
        {
            // Dump the state: the loop references ride as <PARAMETERS> attributes, so a
            // missing loop property shows here.
            juce::MemoryBlock state;
            instance->getStateInformation (state);

            std::unique_ptr<juce::XmlElement> xml = juce::XmlDocument::parse (
                juce::String::createStringFromData (state.getData(), (int) state.getSize()));

            auto* parameters = xml != nullptr ? xml->getChildByName ("PARAMETERS") : nullptr;

            if (parameters != nullptr)
            {
                std::printf ("[INFO] state version=%s loop1=%s slot1Note=%s\n",
                             parameters->getStringAttribute ("suiteStateVersion", "missing").toRawUTF8(),
                             parameters->getStringAttribute ("loop1Path", "missing").toRawUTF8(),
                             parameters->getStringAttribute ("slot1Note", "missing").toRawUTF8());

                for (int i = 0; i < parameters->getNumChildElements(); ++i)
                {
                    auto* child = parameters->getChildElement (i);
                    const juce::String id = child != nullptr ? child->getStringAttribute ("id") : juce::String();

                    if (id == "slot1Note" || id == "slot1Beats" || id == "tempo" || id == "sync"
                        || id == "quantize" || id == "triggerMode")
                        std::printf ("[INFO] param %s = %s\n", id.toRawUTF8(),
                                     child != nullptr ? child->getStringAttribute ("value").toRawUTF8() : "?");
                }
            }
            else
            {
                const juce::String text = juce::String::createStringFromData (state.getData(), juce::jmin (200, (int) state.getSize()));
                std::printf ("[INFO] state head: %s\n", text.toRawUTF8());
            }

            return fail ("cyclical audio", "no sound after a note-on");
        }
    }

    instance->releaseResources();

    std::printf ("[PASS] %s loads, opens its editor, prepares and processes\n", description.name.toRawUTF8());
    return 0;
}
