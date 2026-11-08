# Dynamic Saturation Companion - built with suite_add_plugin (modules/suite_plugin/SuitePlugin.cmake)

suite_add_plugin(
    NAME            DynamicSaturation
    PRODUCT_NAME    "Dynamic Saturation Companion"
    MANUFACTURER    Asui
    PLUGIN_CODE     Dsat
    SOURCES         Source/PluginProcessor.cpp Source/PluginProcessor.h
                    Source/PluginEditor.cpp Source/PluginEditor.h
    TEST_SOURCES    tests/PluginTests.cpp
    # The brand lockup (FROTH Audio wordmark, mark and the product name) is embedded, so the header loads it from inside the plugin.
    LOCKUP          ${CMAKE_SOURCE_DIR}/modules/suite_ui/brand/lockup_dynamic_saturation.png
    MODULATION
)
