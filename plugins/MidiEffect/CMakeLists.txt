# MIDI Chord/Arp/Scale Tool - built with suite_add_plugin (modules/suite_plugin/SuitePlugin.cmake)

suite_add_plugin(
    NAME            MidiEffect
    PRODUCT_NAME    "MIDI Chord Arp Scale Tool"
    DESCRIPTION     "Instrument that quantises incoming notes to a scale and builds chords and arpeggios from them."
    MANUFACTURER    Asui
    PLUGIN_CODE     Marc
    IS_SYNTH
    NEEDS_MIDI_INPUT
    NEEDS_MIDI_OUTPUT
    SOURCES         Source/PluginProcessor.cpp Source/PluginProcessor.h
                    Source/PluginEditor.cpp Source/PluginEditor.h
    TEST_SOURCES    tests/PluginTests.cpp
    LOCKUP          ${CMAKE_SOURCE_DIR}/modules/suite_ui/brand/lockup_midi_effect.png
    MODULATION
    SETTINGS
)
