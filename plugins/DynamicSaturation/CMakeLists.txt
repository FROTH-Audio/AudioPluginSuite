# Dynamic Saturation Companion - built with suite_add_plugin (modules/suite_plugin/SuitePlugin.cmake)

suite_add_plugin(
    NAME            DynamicSaturation
    PRODUCT_NAME    "Dynamic Saturation Companion"
    DESCRIPTION     "Envelope-followed saturation with tube, tape and diode shapers, 2x oversampled."
    MANUFACTURER    Asui
    PLUGIN_CODE     Dsat
    SOURCES         Source/PluginProcessor.cpp Source/PluginProcessor.h
                    Source/PluginEditor.cpp Source/PluginEditor.h
    TEST_SOURCES    tests/PluginTests.cpp
    # The brand lockup (FROTH Audio wordmark, mark and the product name) is embedded, so the header loads it from inside the plugin.
    LOCKUP          ${CMAKE_SOURCE_DIR}/modules/suite_ui/brand/lockup_dynamic_saturation.png
    MODULATION
)
