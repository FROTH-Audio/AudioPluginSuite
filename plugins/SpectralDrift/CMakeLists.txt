# Spectral Drift Shifter - built with suite_add_plugin (modules/suite_plugin/SuitePlugin.cmake)

suite_add_plugin(
    NAME            SpectralDrift
    PRODUCT_NAME    "Spectral Drift Shifter"
    MANUFACTURER    Asui
    PLUGIN_CODE     Sdft
    SOURCES         Source/PluginProcessor.cpp Source/PluginProcessor.h
                    Source/PluginEditor.cpp Source/PluginEditor.h
    TEST_SOURCES    tests/PluginTests.cpp
    # The brand lockup (FROTH Audio wordmark, mark and the product name) is embedded, so the header loads it from inside the plugin.
    LOCKUP          ${CMAKE_SOURCE_DIR}/modules/suite_ui/brand/lockup_spectral_drift.png
    MODULATION
)
