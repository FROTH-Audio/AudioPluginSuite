# Module: suite_presets
#
# The preset bar, the .ausp file I/O and the menu model ship in every build, and the free factory
# bank (INIT-only since plan 03.3) is always embedded. The bank is parsed by modules/suite_presets.
#
# suite_presets_add_to_target(<target>) is defined here and must be called by every consumer. It is
# not a library target: like suite_ui and suite_modulation, the sources are compiled into each
# consumer, because a static library that links JUCE modules produces duplicate symbols.
#
# PresetBank.cpp is always compiled: the free bank is embedded, so it always parses (the INIT bank).

function(suite_presets_add_to_target target)
    target_sources(${target} PRIVATE
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Source/PresetBar.cpp
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Source/PresetFile.cpp
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/factory_bank/Source/PresetBank.cpp)

    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/include
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/factory_bank/include)

    target_compile_definitions(${target} PRIVATE SUITE_PRESET_BANK=1)
    target_link_libraries(${target} PRIVATE SuitePresetBankData)

    # Host programs are index-cached by hosts, so they are compiled; bank order is append-only once
    # the flag is on. Only meaningful with a bank; the bank is now always embedded.
    target_compile_definitions(${target} PRIVATE SUITE_PRESET_BANK_AS_PROGRAMS=1)
endfunction()
