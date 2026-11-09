# Private overlay: Pro preset bank and file I/O
# This module is compiled only when SUITE_PRO_PRESETS=ON

option(SUITE_PRO_PRESETS "Build the Pro preset browser and factory bank" OFF)

if(SUITE_PRO_PRESETS)
    set(SUITE_PRO_DIR "" CACHE PATH "Path to the private Pro overlay")
    
    if(NOT EXISTS "${SUITE_PRO_DIR}/presets/CMakeLists.txt")
        message(FATAL_ERROR "SUITE_PRO_PRESETS=ON but SUITE_PRO_DIR is not set")
    endif()
    
    target_sources(${target} PRIVATE ${SUITE_PRO_PRESETS_SOURCES})
    target_include_directories(${target} PRIVATE ${SUITE_PRO_DIR}/presets/include)
    target_compile_definitions(${target} PRIVATE SUITE_PRO_PRESETS=1)
    target_link_libraries(${target} PRIVATE SuiteProPresetBank)
endif()

function(suite_presets_add_to_target target)
    # Include the public seam (Phase 1)
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/include)
    
    if(SUITE_PRO_PRESETS)
        # Include Pro implementation (Phase 2+)
        target_sources(${target} PRIVATE ${SUITE_PRO_PRESETS_SOURCES})
        target_include_directories(${target} PRIVATE ${SUITE_PRO_DIR}/presets/include)
        target_link_libraries(${target} PRIVATE SuiteProPresetBank)
    endif()
endfunction()

# Also handle the case where suite_presets_add_to_target is called before this function is defined
if(NOT TARGET SuiteProPresetBank AND SUITE_PRO_PRESETS)
    # Placeholder - real build would have the actual library
endif()