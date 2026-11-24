# Multiband Saturation - built with suite_add_plugin (modules/suite_plugin/SuitePlugin.cmake)

suite_add_plugin(
    NAME            MultibandSaturation
    PRODUCT_NAME    "MultiSat"
    DESCRIPTION     "Two-band Linkwitz-Riley split, each band with its own drive and tone."
    MANUFACTURER    Asui
    PLUGIN_CODE     Msat
    SOURCES         Source/PluginProcessor.cpp Source/PluginProcessor.h
                    Source/PluginEditor.cpp Source/PluginEditor.h
    TEST_SOURCES    tests/PluginTests.cpp
    LOCKUP          ${CMAKE_SOURCE_DIR}/modules/suite_ui/brand/lockup_multiband_saturation.png
    MODULATION
    SETTINGS
)
