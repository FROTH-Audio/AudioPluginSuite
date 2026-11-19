# Cyclical - built with suite_add_plugin (modules/suite_plugin/SuitePlugin.cmake)

suite_add_plugin(
    NAME            Cyclical
    PRODUCT_NAME    "Cyclical"
    DESCRIPTION     "Four-slot loop instrument: regions, speed, direction, ping-pong and granular playback, quantised to the host grid."
    MANUFACTURER    Asui
    PLUGIN_CODE     Cycl
    SOURCES         Source/PluginProcessor.cpp Source/PluginProcessor.h
                    Source/PluginEditor.cpp Source/PluginEditor.h
                    Source/LoopWaveformView.cpp Source/LoopWaveformView.h
                    Source/LoopSlot.h
    TEST_SOURCES    tests/PluginTests.cpp
    # R13: the hamburger menu's Settings... entry needs the shared settings store.
    SETTINGS
    IS_SYNTH        NEEDS_MIDI_INPUT
    LOCKUP          ${CMAKE_SOURCE_DIR}/modules/suite_ui/brand/lockup_cyclical.png
    ASSETS          Resources/samples/kick_4k.wav
                    Resources/samples/hats_4k.wav
                    Resources/samples/snare_4k.wav
                    Resources/samples/perc_4k.wav
                    Resources/samples/bass_4k.wav
                    Resources/samples/chord_4k.wav
                    Resources/samples/arp_4k.wav
                    Resources/samples/texture_4k.wav
                    Resources/samples/drone_4k.wav
                    Resources/samples/bells_4k.wav
                    Resources/samples/vox_4k.wav
                    Resources/samples/noise_4k.wav
)
