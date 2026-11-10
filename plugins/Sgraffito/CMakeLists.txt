# Sgraffito - built with suite_add_plugin (modules/suite_plugin/SuitePlugin.cmake)

suite_add_plugin(
    NAME            Sgraffito
    PRODUCT_NAME    "Sgraffito"
    MANUFACTURER    Asui
    PLUGIN_CODE     Sgrf
    SOURCES         Source/PluginProcessor.cpp Source/PluginProcessor.h
                    Source/PluginEditor.cpp Source/PluginEditor.h
    TEST_SOURCES    tests/PluginTests.cpp
    IS_SYNTH NEEDS_MIDI_INPUT MODULATION
)
