# __PRODUCT__ - built with suite_add_plugin (modules/suite_plugin/SuitePlugin.cmake)

suite_add_plugin(
    NAME            __NAME__
    PRODUCT_NAME    "__PRODUCT__"
    MANUFACTURER    Asui
    PLUGIN_CODE     __CODE__
    SOURCES         Source/PluginProcessor.cpp Source/PluginProcessor.h
                    Source/PluginEditor.cpp Source/PluginEditor.h
    TEST_SOURCES    tests/PluginTests.cpp
)
