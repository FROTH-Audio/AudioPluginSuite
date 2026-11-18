# Stereo Tool Effect - built with suite_add_plugin (modules/suite_plugin/SuitePlugin.cmake)

suite_add_plugin(
    NAME            StereoTool
    PRODUCT_NAME    "Stereo Tool Effect"
    DESCRIPTION     "Mid/side stereo tool: width, correlated/uncorrelated balance and tanh saturation."
    MANUFACTURER    Asui
    PLUGIN_CODE     Ster
    SOURCES         Source/PluginProcessor.cpp Source/PluginProcessor.h
                    Source/PluginEditor.cpp Source/PluginEditor.h
    TEST_SOURCES    tests/PluginTests.cpp
    MODULATION
)
