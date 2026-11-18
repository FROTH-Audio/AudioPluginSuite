# MIDI Chord/Arp/Scale Tool - re-harmonises incoming MIDI notes

suite_add_plugin(
    NAME            MidiEffect
    PRODUCT_NAME    "MIDI Chord/Arp/Scale Tool"
    DESCRIPTION     "MIDI effect that generates chords, arpeggios and scales from incoming notes."
    MANUFACTURER    Asui
    PLUGIN_CODE     MCAT
    IS_SYNTH        TRUE
    IS_MIDI_EFFECT  TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    SOURCES         Source/PluginProcessor.cpp Source/PluginProcessor.h
                    Source/PluginEditor.cpp Source/PluginEditor.h
    TEST_SOURCES    tests/PluginTests.cpp
    MODULATION
    SETTINGS
)
