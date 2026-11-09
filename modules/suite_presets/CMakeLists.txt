# The preset bar and the .ausp file I/O ship in every build: the unit-test executable here is
# registered unconditionally. It checks the file format, sanitising, the delta parser and the menu,
# which do not need a plugin instance or the embedded bank. The per-plugin bank tests are
# registered by suite_add_plugin() when SUITE_PRESET_BANK is on.
if(SUITE_BUILD_TESTS)
    add_executable(SuitePresetsFileTests tests/PresetFileTests.cpp)
    suite_presets_add_to_target(SuitePresetsFileTests)

    target_include_directories(SuitePresetsFileTests PRIVATE
        ${CMAKE_SOURCE_DIR}/tests/common
        ${CMAKE_SOURCE_DIR}/modules/suite_presets/factory_bank/tests)

    target_link_libraries(SuitePresetsFileTests PRIVATE
        SuiteState
        juce::juce_audio_processors
        juce::juce_gui_basics)

    add_test(NAME SuitePresetsFile_Tests COMMAND SuitePresetsFileTests)
endif()
