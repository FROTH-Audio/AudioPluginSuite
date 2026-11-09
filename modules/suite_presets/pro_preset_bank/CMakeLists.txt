# Private overlay: Pro preset bank and file I/O

set(SUITE_PRO_PRESETS_SOURCES
    Source/PresetBank.cpp
    Source/PresetBar.cpp
)

if(NOT SUITE_PRO_PRESETS)
    message(STATUS "SUITE_PRO_PRESETS is OFF - using public seam only")
else()
    add_library(SuiteProPresetBank STATIC ${SUITE_PRO_PRESETS_SOURCES})
    target_include_directories(SuiteProPresetBank PUBLIC include)
    target_include_directories(SuiteProPresetBank PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
    # Also expose the parent suite_presets' include directory for SuitePresets.h
    target_include_directories(SuiteProPresetBank PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../include)
    target_link_libraries(SuiteProPresetBank PUBLIC juce_audio_processors juce_gui_basics)
endif()

# Add the pro_preset_bank module to the suite_presets module so it gets included when using suite_presets_add_to_target
target_include_directories(suite_presets INTERFACE ${CMAKE_CURRENT_LIST_DIR}/../include)

# Register tests when SUITE_BUILD_TESTS is on (handled by root CMakeLists.txt)
if(SUITE_BUILD_TESTS AND SUITE_PRO_PRESETS)
    add_executable(PresetTests tests/PresetTests.cpp)
    target_link_libraries(PresetTests PRIVATE SuiteProPresetBank juce_audio_processors juce_gui_basics)
endif()