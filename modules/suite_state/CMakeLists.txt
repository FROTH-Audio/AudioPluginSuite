# SuiteState: versioned saved state. Header-only, so it follows the CoreDSP pattern.
add_library(SuiteState INTERFACE)
target_include_directories(SuiteState INTERFACE include)
target_link_libraries(SuiteState INTERFACE juce::juce_audio_processors)

if(SUITE_BUILD_TESTS)
    # Run with: ctest --test-dir build -C Release --output-on-failure
    add_executable(SuiteStateTests tests/SuiteStateTests.cpp)
    target_link_libraries(SuiteStateTests PRIVATE SuiteState)
    add_test(NAME SuiteState_Tests COMMAND SuiteStateTests)
endif()
