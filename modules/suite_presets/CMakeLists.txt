# The preset bar and the .ausp file I/O ship in every build, and the free factory bank (INIT-only
# since plan 03.3) is always embedded, so the per-plugin bank tests are registered unconditionally.
# They check the file format, sanitising, the delta parser, the menu and the embedded INIT bank.
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
