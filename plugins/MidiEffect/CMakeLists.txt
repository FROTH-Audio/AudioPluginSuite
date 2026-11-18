# MIDI Chord/Arp/Scale Tool - built with suite_add_plugin (modules/suite_plugin/SuitePlugin.cmake)

suite_add_plugin(
    NAME            MidiEffect
    PRODUCT_NAME    "MIDI Chord Arp Scale Tool"
    DESCRIPTION     "MIDI effect that quantises incoming notes to a scale and builds chords from them."
    MANUFACTURER    Asui
    PLUGIN_CODE     Marc
    IS_SYNTH        IS_MIDI_EFFECT  NEEDS_MIDI_INPUT
    SOURCES         Source/PluginProcessor.cpp Source/PluginProcessor.h
                    Source/PluginEditor.cpp Source/PluginEditor.h
    TEST_SOURCES    tests/PluginTests.cpp
    LOCKUP          ${CMAKE_SOURCE_DIR}/modules/suite_ui/brand/lockup_midi_effect.png
    MODULATION
)
