# Module: suite_presets (public seam)
#
# StateHolder is the only interface a plugin implements. PresetBar is declared here and implemented
# either by Source/SuitePresetsStub.cpp (public build) or by the private overlay (Pro build).
#
# suite_presets_add_to_target(<target>) is defined here and must be called by every consumer. It is
# not a library target: like suite_ui and suite_modulation, the sources are compiled into each
# consumer, because a static library that links JUCE modules produces duplicate symbols.

function(suite_presets_add_to_target target)
    if(SUITE_PRO_PRESETS)
        if(NOT TARGET SuiteProPresetBankData)
            message(FATAL_ERROR "SUITE_PRO_PRESETS=ON but the Pro overlay was not added before ${target}")
        endif()

        target_sources(${target} PRIVATE ${SUITE_PRO_PRESETS_SOURCES})
        target_include_directories(${target} PRIVATE ${SUITE_PRO_DIR}/include)
        target_compile_definitions(${target} PRIVATE SUITE_PRO_PRESETS=1)

        # Host program list is a separate, revertable step: bank indices are cached by hosts, so it
        # stays behind its own flag and the bank order is append-only once it is on.
        if(SUITE_PRO_PRESETS_AS_PROGRAMS)
            target_compile_definitions(${target} PRIVATE SUITE_PRO_PRESETS_AS_PROGRAMS=1)
        else()
            target_compile_definitions(${target} PRIVATE SUITE_PRO_PRESETS_AS_PROGRAMS=0)
        endif()

        target_link_libraries(${target} PRIVATE SuiteProPresetBankData)
    else()
        if(SUITE_PRO_PRESETS_AS_PROGRAMS)
            message(FATAL_ERROR "SUITE_PRO_PRESETS_AS_PROGRAMS=ON needs SUITE_PRO_PRESETS=ON")
        endif()

        target_sources(${target} PRIVATE ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Source/SuitePresetsStub.cpp)
        target_compile_definitions(${target} PRIVATE SUITE_PRO_PRESETS=0 SUITE_PRO_PRESETS_AS_PROGRAMS=0)
    endif()

    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/include)
endfunction()
