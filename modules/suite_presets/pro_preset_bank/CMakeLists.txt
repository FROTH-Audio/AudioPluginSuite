# Private overlay: Pro preset bank, bar and file I/O.
#
# This is not a library target. `suite_presets_add_to_target()` compiles the sources below into
# each consumer, the same rule as suite_ui and suite_modulation, because a static library that
# links JUCE modules produces duplicate symbols. Only the embedded factory bank is a real target.

set(SUITE_PRO_PRESETS_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/Source/PresetBank.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/PresetFile.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/PresetBar.cpp
)

# Plugins are added after this directory, so they inherit the list from the root scope.
set(SUITE_PRO_PRESETS_SOURCES "${SUITE_PRO_PRESETS_SOURCES}" PARENT_SCOPE)

# The one embedded asset. The generated header is SuiteProPresetBankData.h, and the resource
# names are BinaryData::AudioSuitePresets_xml / AudioSuitePresets_xmlSize.
juce_add_binary_data(SuiteProPresetBankData
    SOURCES ${CMAKE_CURRENT_LIST_DIR}/bank/AudioSuitePresets.xml
    HEADER_NAME SuiteProPresetBankData.h
    NAMESPACE SuiteProPresetBankData)

# The per-plugin preset tests are registered by suite_add_plugin(). This one covers the file
# format, sanitising and the menu model, which do not need a plugin instance.
if(SUITE_BUILD_TESTS)
    add_executable(SuitePresetsPro_Tests
        ${CMAKE_CURRENT_LIST_DIR}/tests/PresetFileTests.cpp
        ${SUITE_PRO_PRESETS_SOURCES})

    target_include_directories(SuitePresetsPro_Tests PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_CURRENT_LIST_DIR}/../include
        ${CMAKE_SOURCE_DIR}/tests/common)

    target_compile_definitions(SuitePresetsPro_Tests PRIVATE SUITE_PRO_PRESETS=1)
    target_link_libraries(SuitePresetsPro_Tests PRIVATE
        SuiteProPresetBankData
        SuiteState
        juce::juce_audio_processors
        juce::juce_gui_basics)

    add_test(NAME SuitePresetsPro_Tests COMMAND SuitePresetsPro_Tests)
endif()
