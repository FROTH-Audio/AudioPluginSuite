# __PRODUCT__ - built with suite_add_plugin (modules/suite_plugin/SuitePlugin.cmake)

suite_add_plugin(
    NAME            __NAME__
    PRODUCT_NAME    "__PRODUCT__"
    MANUFACTURER    Asui
    PLUGIN_CODE     __CODE__
    SOURCES         Source/PluginProcessor.cpp Source/PluginProcessor.h
                    Source/PluginEditor.cpp Source/PluginEditor.h
    TEST_SOURCES    tests/PluginTests.cpp
    # R13: the gear menu's Settings... entry needs the shared settings store.
    SETTINGS
    # LOCKUP        ${CMAKE_SOURCE_DIR}/modules/suite_ui/brand/lockup_<lowercase name>.png
    # LOCKUP is per-plugin authoring: add the row to tools/brand/lockups.py, run it, then
    # uncomment the line above (the About dialog shows the lockup when one is embedded).
)
