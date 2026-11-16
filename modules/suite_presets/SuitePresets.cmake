# Module: suite_presets (public seam)
#
# StateHolder is the only interface a plugin implements. PresetBar, PresetFile and PresetBank
# are compiled into every consumer; SUITE_PRESET_BANK gates whether the embedded bank data
# target is linked and SUITE_PRESET_BANK=1 is defined.
#
# suite_presets_add_to_target(<target>) is defined here and must be called by every consumer. It is
# not a library target: like suite_ui and suite_modulation, the sources are compiled into each
# consumer, because a static library that links JUCE modules produces duplicate symbols.
#
# PresetBank.cpp is always compiled: with SUITE_PRESET_BANK=0 it degrades to an empty bank, so the
# bar, the file I/O and the tests build unchanged without it.

function(suite_presets_add_to_target target)
    # PresetFile reads the user-preset-folder override from the shared settings store (plan 08),
    # so the settings module rides along automatically. It defines suite_settings_add_to_target,
    # which the root CMakeLists includes before any consumer.
    suite_settings_add_to_target(${target})

    target_sources(${target} PRIVATE
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Source/PresetBar.cpp
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Source/PresetFile.cpp
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/factory_bank/Source/PresetBank.cpp)

    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/include
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/factory_bank/include)

    if(SUITE_PRESET_BANK)
        if(NOT TARGET SuitePresetBankData)
            message(FATAL_ERROR "SUITE_PRESET_BANK=ON but the factory bank was not configured before ${target}")
        endif()

        target_compile_definitions(${target} PRIVATE SUITE_PRESET_BANK=1)
        target_link_libraries(${target} PRIVATE SuitePresetBankData)
    else()
        target_compile_definitions(${target} PRIVATE SUITE_PRESET_BANK=0)
    endif()

    # Host programs are index-cached by hosts, so they stay behind their own flag and the bank order
    # is append-only once it is on. Only meaningful with a bank; the root CMakeLists refuses the
    # flag without one.
    if(SUITE_PRESET_BANK_AS_PROGRAMS)
        target_compile_definitions(${target} PRIVATE SUITE_PRESET_BANK_AS_PROGRAMS=1)
    else()
        target_compile_definitions(${target} PRIVATE SUITE_PRESET_BANK_AS_PROGRAMS=0)
    endif()
endfunction()
