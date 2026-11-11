# Sgraffito - built with suite_add_plugin (modules/suite_plugin/SuitePlugin.cmake)

suite_add_plugin(
    NAME            Sgraffito
    PRODUCT_NAME    "Sgraffito"
    MANUFACTURER    Asui
    PLUGIN_CODE     Sgrf
    SOURCES         Source/PluginProcessor.cpp Source/PluginProcessor.h
                    Source/PluginEditor.cpp Source/PluginEditor.h
    TEST_SOURCES    tests/PluginTests.cpp
    # The brand lockup (FROTH Audio wordmark, mark and the product name) is embedded, so the header loads it from inside the plugin.
    LOCKUP          ${CMAKE_SOURCE_DIR}/modules/suite_ui/brand/lockup_sgraffito.png
    IS_SYNTH NEEDS_MIDI_INPUT MODULATION
    ASSETS          Resources/samples/grain_4k.wav
                    Resources/samples/cloud_4k.wav
)
