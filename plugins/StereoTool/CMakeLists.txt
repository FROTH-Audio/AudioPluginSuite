# Stereo Tool Effect - mid/side encoding, width control, shared DSP

suite_add_plugin(
    NAME            StereoTool
    PRODUCT_NAME    "Stereo Tool Effect"
    MANUFACTURER    Asui
    PLUGIN_CODE     STS_
    SOURCES         Source/PluginProcessor.cpp Source/PluginProcessor.h
                    Source/PluginEditor.cpp Source/PluginEditor.h
    TEST_SOURCES    tests/PluginTests.cpp
    MODULATION      TRUE
    LOCKUP          StereoTool
)
