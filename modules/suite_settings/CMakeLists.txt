# suite_settings has no standalone library target. Its sources are added to consumers via
# suite_settings_add_to_target() in SuiteSettings.cmake.
#
# The module's own tests build only with the plugins (same gate as suite_modulation). This
# directory is added from the root CMakeLists.txt inside the SUITE_BUILD_PLUGINS block.
if(SUITE_BUILD_TESTS)
    add_executable(SuiteSettingsTests
        tests/SuiteSettingsTests.cpp
    )

    suite_ui_add_to_target(SuiteSettingsTests)
    suite_settings_add_to_target(SuiteSettingsTests)

    target_link_libraries(SuiteSettingsTests PRIVATE
        juce::juce_gui_basics
    )

    # The tests redirect the store to this folder (argv[1]); they must never touch the real
    # user settings file.
    add_test(NAME SuiteSettings_Tests COMMAND SuiteSettingsTests
             ${CMAKE_BINARY_DIR}/test-settings)
endif()
