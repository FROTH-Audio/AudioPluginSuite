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
    SETTINGS
    ASSETS          Resources/samples/grain_4k.wav
                    Resources/samples/presets/glass_dust_4k.wav
                    Resources/samples/presets/deep_cloud_4k.wav
                    Resources/samples/presets/memory_ghosts_4k.wav
                    Resources/samples/presets/tape_break_4k.wav
                    Resources/samples/presets/cloud_rain_4k.wav
                    Resources/samples/presets/scrubbed_memory_4k.wav
                    Resources/samples/presets/chopped_sticks_4k.wav
                    Resources/samples/presets/swell_pad_4k.wav
                    Resources/samples/presets/cloud_dust_4k.wav
                    Resources/samples/presets/wobble_memory_4k.wav
                    Resources/samples/presets/ghost_delay_4k.wav
                    Resources/samples/presets/swelling_cloud_4k.wav
                    Resources/samples/presets/choppy_scratch_4k.wav
                    Resources/samples/presets/drifted_memory_4k.wav
                    Resources/samples/presets/init_cloud_4k.wav
                    Resources/samples/presets/velvet_swing_4k.wav
                    Resources/samples/presets/ghostly_echo_4k.wav
                    Resources/samples/presets/held_breath_4k.wav
                    Resources/samples/presets/fine_grained_drift_4k.wav
                    Resources/samples/presets/slow_attack_tape_4k.wav
)
