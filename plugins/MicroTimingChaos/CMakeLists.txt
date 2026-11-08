# Micro-Timing Chaos - built with suite_add_plugin (modules/suite_plugin/SuitePlugin.cmake)

suite_add_plugin(
    NAME            MicroTimingChaos
    PRODUCT_NAME    "Micro-Timing Chaos"
    MANUFACTURER    Asui
    PLUGIN_CODE     Mtch
    MIDI_EFFECT
    SOURCES         Source/PluginProcessor.cpp Source/PluginProcessor.h
                    Source/PluginEditor.cpp Source/PluginEditor.h
                    Source/LerpCurveEditor.cpp Source/LerpCurveEditor.h
                    Source/LerpPreview.cpp Source/LerpPreview.h
                    Source/MicroTimingChaosLookAndFeel.cpp
    TEST_SOURCES    tests/PluginTests.cpp
    # Icons and textures are embedded in the binary, so they load from inside the plugin bundle.
    ASSETS          Resources/icons/bpm_sync_active.svg
                    Resources/icons/bpm_sync_inactive.svg
                    Resources/textures/bpm_sync_4k.png
                    Resources/textures/sequencer_bg_4k.png
                    Resources/textures/sequencer_grid_4k.png
    # The brand lockup (FROTH Audio wordmark, mark and the product name) is embedded, so the header loads it from inside the plugin.
    LOCKUP          ${CMAKE_SOURCE_DIR}/modules/suite_ui/brand/lockup_micro_timing_chaos.png
    MODULATION
)
