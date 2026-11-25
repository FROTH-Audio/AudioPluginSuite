# Groove Mutator - built with suite_add_plugin (modules/suite_plugin/SuitePlugin.cmake)
# Plan 19: a MIDI effect (output-only buses, notes out, the MidiEffect precedent) with the
# shared LFOs and routing. Each flag on its own line: the bare-keyword misparse (AGENTS §11).

suite_add_plugin(
    NAME            GrooveMutator
    PRODUCT_NAME    "Groove Mutator"
    MANUFACTURER    Asui
    PLUGIN_CODE     GRVM
    IS_SYNTH
    NEEDS_MIDI_INPUT
    NEEDS_MIDI_OUTPUT
    SOURCES         Source/PluginProcessor.cpp Source/PluginProcessor.h
                    Source/PluginEditor.cpp Source/PluginEditor.h
                    Source/LaneView.cpp Source/LaneView.h
    TEST_SOURCES    tests/PluginTests.cpp
    MODULATION
    # R13: the menu's Settings... entry needs the shared settings store.
    SETTINGS
    LOCKUP        ${CMAKE_SOURCE_DIR}/modules/suite_ui/brand/lockup_groovemutator.png
)
