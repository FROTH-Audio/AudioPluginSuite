# suite_presets_add_to_target(<target>): adds the preset seam sources and include dir.
# The public build is a stub with height 0; Pro builds add the full implementation.
set(SUITE_PRESETS_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(suite_presets_add_to_target target)
    # Always add the header
    target_include_directories(${target} PRIVATE ${SUITE_PRESETS_DIR}/include)

    # Stub sources are always compiled (public seam)
    target_sources(${target} PRIVATE ${SUITE_PRESETS_DIR}/Source/SuitePresetsStub.cpp)

    # Pro build flag is not needed for the stub; it's always available.
endfunction()
