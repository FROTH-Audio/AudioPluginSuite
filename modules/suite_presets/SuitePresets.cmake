# Module: suite_presets
# Provides StateHolder interface and PresetBar for preset management

function(suite_presets_add_to_target target)
    if(SUITE_PRO_PRESETS)
        if(NOT EXISTS "${SUITE_PRO_DIR}/presets/CMakeLists.txt")
            message(FATAL_ERROR "SUITE_PRO_PRESETS=ON but SUITE_PRO_DIR is not set")
        endif()
        target_sources(${target} PRIVATE ${SUITE_PRO_PRESETS_SOURCES})
        target_include_directories(${target} PRIVATE ${SUITE_PRO_DIR}/presets/include)
        target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
        target_compile_definitions(${target} PRIVATE SUITE_PRO_PRESETS=1)
        target_link_libraries(${target} PRIVATE SuiteProPresetBank)
    else()
        target_sources(${target} PRIVATE ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Source/SuitePresetsStub.cpp)
        target_compile_definitions(${target} PRIVATE SUITE_PRO_PRESETS=0)
    endif()
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/include)
    
    # Also add the pro_preset_bank's parent include directory so SuitePresetsPro.h is found
    if(SUITE_PRO_PRESETS)
        target_include_directories(${target} PUBLIC ${SUITE_PRO_DIR}/include)
    endif()
endfunction()
