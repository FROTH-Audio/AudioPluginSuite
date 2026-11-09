# SuiteState: versioned saved state. Header-only public API, with implementation in Source/.
add_library(SuiteState INTERFACE)
target_include_directories(SuiteState INTERFACE include)
target_link_libraries(SuiteState INTERFACE juce::juce_audio_processors)

if(SUITE_BUILD_TESTS)
    target_sources(SuiteState PRIVATE Source/SuiteState.cpp)
    add_executable(SuiteStateTests tests/SuiteStateTests.cpp)
    target_link_libraries(SuiteStateTests PRIVATE SuiteState)
    add_test(NAME SuiteState_Tests COMMAND SuiteStateTests)
endif()
