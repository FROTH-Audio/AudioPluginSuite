# suite_add_plugin(...): builds one plugin, its test executable and its test registration.
#
# Example (plugins/SpectralDrift/CMakeLists.txt):
#   suite_add_plugin(
#       NAME            SpectralDrift
#       PRODUCT_NAME    "Spectral Drift Shifter"
#       MANUFACTURER    Asui
#       PLUGIN_CODE     Sdft
#       SOURCES         Source/PluginProcessor.cpp Source/PluginProcessor.h
#                       Source/PluginEditor.cpp Source/PluginEditor.h
#       TEST_SOURCES    tests/PluginTests.cpp
#       LOCKUP          ${CMAKE_SOURCE_DIR}/modules/suite_ui/brand/lockup_spectral_drift.png
#       MODULATION
#   )
#
# Required:
#   NAME          Short name, for example SpectralDrift. The plugin target is <NAME>, the tests are <NAME>Tests.
#   PRODUCT_NAME  Name shown by hosts.
#   MANUFACTURER  Four-character manufacturer code (Asui).
#   PLUGIN_CODE   Four-character plugin code. Must be unique across the suite; a duplicate stops configuration.
#   SOURCES       Processor, editor and other plugin files. Every .cpp here is also compiled into the test executable.
#   TEST_SOURCES  Test files, for example tests/PluginTests.cpp.
#
# Optional:
#   BUNDLE_ID     Default: com.audiosuite.<lowercased NAME>.
#   COMPANY_NAME  Default: FROTH Audio. The manufacturer name hosts show. The bundle ID keeps com.audiosuite.
#   VERSION       Default: ${PROJECT_VERSION}.
#   DESCRIPTION   Description written into the bundle. Default: PRODUCT_NAME.
#   LOCKUP        Brand lockup PNG, embedded as binary data.
#   ASSETS        Other files embedded in the same binary data target as LOCKUP.
#   MODULATION    Adds suite_modulation (LFOs and routing). Off by default.
#   SETTINGS      Adds suite_settings (the plugin settings store). Off by default.
#   IS_SYNTH        Marks the plugin as a synth/instrument (VST3 instrument category).
#   IS_MIDI_EFFECT  Marks the plugin as a MIDI effect (AU MIDI processor type).
#   NEEDS_MIDI_INPUT  Declares MIDI input capability.
#   NEEDS_MIDI_OUTPUT Declares MIDI output capability.
#
# Needs suite_ui_add_to_target (modules/suite_ui/SuiteUI.cmake) and, with MODULATION, suite_modulation_add_to_target.
# Variables set inside the function do not leak out: everything the plugin needs is a target property.

set_property(GLOBAL PROPERTY SUITE_PLUGIN_CODES "")

function(suite_add_plugin)
    set(options MODULATION SETTINGS IS_SYNTH IS_MIDI_EFFECT NEEDS_MIDI_INPUT NEEDS_MIDI_OUTPUT)
    set(oneValue NAME PRODUCT_NAME MANUFACTURER PLUGIN_CODE BUNDLE_ID COMPANY_NAME VERSION DESCRIPTION LOCKUP)
    set(multiValue SOURCES TEST_SOURCES ASSETS)
    cmake_parse_arguments(SAP "${options}" "${oneValue}" "${multiValue}" ${ARGN})

    if(SAP_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "suite_add_plugin: unknown arguments: ${SAP_UNPARSED_ARGUMENTS}")
    endif()
    foreach(required NAME PRODUCT_NAME MANUFACTURER PLUGIN_CODE SOURCES TEST_SOURCES)
        if(NOT SAP_${required})
            message(FATAL_ERROR "suite_add_plugin(${SAP_NAME}): ${required} is required")
        endif()
    endforeach()

    #---- Plugin code: four characters, unique across the suite ----
    string(LENGTH "${SAP_PLUGIN_CODE}" codeLength)
    if(NOT codeLength EQUAL 4)
        message(FATAL_ERROR "suite_add_plugin(${SAP_NAME}): PLUGIN_CODE must be exactly 4 characters, got '${SAP_PLUGIN_CODE}'")
    endif()
    get_property(usedCodes GLOBAL PROPERTY SUITE_PLUGIN_CODES)
    if(SAP_PLUGIN_CODE IN_LIST usedCodes)
        message(FATAL_ERROR "suite_add_plugin(${SAP_NAME}): plugin code '${SAP_PLUGIN_CODE}' is already used by another plugin")
    endif()
    set_property(GLOBAL APPEND PROPERTY SUITE_PLUGIN_CODES "${SAP_PLUGIN_CODE}")

    #---- Defaults ----
    string(TOLOWER "${SAP_NAME}" lowerName)
    if(NOT SAP_BUNDLE_ID)
        set(SAP_BUNDLE_ID "com.audiosuite.${lowerName}")
    endif()
    if(NOT SAP_COMPANY_NAME)
        set(SAP_COMPANY_NAME "FROTH Audio")
    endif()
    if(NOT SAP_VERSION)
        set(SAP_VERSION "${PROJECT_VERSION}")
    endif()
    #---- JUCE plugin type flags (independent booleans) ----
    if(SAP_IS_SYNTH)
        set(isSynth TRUE)
    else()
        set(isSynth FALSE)
    endif()
    if(SAP_IS_MIDI_EFFECT)
        set(isMidiEffect TRUE)
    else()
        set(isMidiEffect FALSE)
    endif()
    if(SAP_NEEDS_MIDI_INPUT)
        set(needsMidiInput TRUE)
    else()
        set(needsMidiInput FALSE)
    endif()
    if(SAP_NEEDS_MIDI_OUTPUT)
        set(needsMidiOutput TRUE)
    else()
        set(needsMidiOutput FALSE)
    endif()

    #---- Plugin target ----
    juce_add_plugin(${SAP_NAME}
        COMPANY_NAME "${SAP_COMPANY_NAME}"
        BUNDLE_ID ${SAP_BUNDLE_ID}
        PLUGIN_MANUFACTURER_CODE ${SAP_MANUFACTURER}
        PLUGIN_CODE ${SAP_PLUGIN_CODE}
        PLUGIN_NAME "${SAP_PRODUCT_NAME}"
        PRODUCT_NAME "${SAP_PRODUCT_NAME}"
        VERSION ${SAP_VERSION}
        FORMATS ${SUITE_PLUGIN_FORMATS}
        IS_SYNTH ${isSynth}
        IS_MIDI_EFFECT ${isMidiEffect}
        NEEDS_MIDI_INPUT ${needsMidiInput}
        NEEDS_MIDI_OUTPUT ${needsMidiOutput}
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
    )

    if(SAP_DESCRIPTION)
        set_target_properties(${SAP_NAME} PROPERTIES JUCE_DESCRIPTION "${SAP_DESCRIPTION}")
    endif()

    target_sources(${SAP_NAME} PRIVATE ${SAP_SOURCES})
    target_include_directories(${SAP_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
    )

    suite_ui_add_to_target(${SAP_NAME})
    if(SAP_MODULATION)
        suite_modulation_add_to_target(${SAP_NAME})
    endif()
    suite_presets_add_to_target(${SAP_NAME})  # StateHolder interface for presets
    if(SAP_SETTINGS)
        suite_settings_add_to_target(${SAP_NAME})
    endif()

    # Identity defines for the About dialog and the settings UI. The plugin target also receives
    # JucePlugin_* definitions from juce_add_plugin, but the two test executables compile the
    # same plugin sources and do not (JUCE 9 writes no JucePluginDefines.h here), so every
    # target needs its own copy.
    set(suiteIdentityDefines
        SUITE_PLUGIN_VERSION="${SAP_VERSION}"
        SUITE_PLUGIN_NAME="${SAP_PRODUCT_NAME}"
        SUITE_PLUGIN_CODE="${SAP_PLUGIN_CODE}"
        SUITE_PLUGIN_ID="${SAP_NAME}")
    target_compile_definitions(${SAP_NAME} PRIVATE ${suiteIdentityDefines})

    #---- Binary data: the lockup and any other assets ----
    set(assets ${SAP_ASSETS})
    if(SAP_LOCKUP)
        list(APPEND assets ${SAP_LOCKUP})
    endif()

    set(links CoreDSP)
    if(assets)
        juce_add_binary_data(${SAP_NAME}Assets SOURCES ${assets})
        list(APPEND links ${SAP_NAME}Assets)
    endif()
    list(APPEND links
        SuiteState
        juce::juce_audio_formats
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_dsp
    )
    target_link_libraries(${SAP_NAME} PRIVATE ${links})

    #---- Test executable ----
    # It compiles the processor and editor itself, not the plugin target. The tests rely on this.
    if(SUITE_BUILD_TESTS)
        set(testSources ${SAP_TEST_SOURCES})
        foreach(src IN LISTS SAP_SOURCES)
            if("${src}" MATCHES "\\.cpp$")
                list(APPEND testSources ${src})
            endif()
        endforeach()

        add_executable(${SAP_NAME}Tests ${testSources})

        # Plugin tests: the real processor and editor sources, run headless with the GUI toolkit initialised.
        target_include_directories(${SAP_NAME}Tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/Source
            ${CMAKE_SOURCE_DIR}/tests/common
        )

        suite_ui_add_to_target(${SAP_NAME}Tests)
        if(SAP_MODULATION)
            suite_modulation_add_to_target(${SAP_NAME}Tests)
        endif()
        suite_presets_add_to_target(${SAP_NAME}Tests)
        if(SAP_SETTINGS)
            suite_settings_add_to_target(${SAP_NAME}Tests)
        endif()
        target_compile_definitions(${SAP_NAME}Tests PRIVATE ${suiteIdentityDefines})

        target_link_libraries(${SAP_NAME}Tests PRIVATE ${links})

        # The editor tests pump the message loop so timers fire.
        target_compile_definitions(${SAP_NAME}Tests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)

        add_test(NAME ${SAP_NAME}_PluginTests COMMAND ${SAP_NAME}Tests)
    endif()

    #---- Pro preset test executable ----
    # Only in a bank build. It compiles the plugin sources, the factory bank and one test file, so the
    # bank is checked against the live APVTS ranges of this plugin.
    if(SUITE_BUILD_TESTS AND SUITE_PRESET_BANK)
        set(presetTestSources ${CMAKE_SOURCE_DIR}/modules/suite_presets/factory_bank/tests/${SAP_NAME}PresetTests.cpp)
        foreach(src IN LISTS SAP_SOURCES)
            if("${src}" MATCHES "\\.cpp$")
                list(APPEND presetTestSources ${src})
            endif()
        endforeach()

        add_executable(${SAP_NAME}PresetTests ${presetTestSources})

        target_include_directories(${SAP_NAME}PresetTests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/Source
            ${CMAKE_SOURCE_DIR}/tests/common
        )

        suite_ui_add_to_target(${SAP_NAME}PresetTests)
        if(SAP_MODULATION)
            suite_modulation_add_to_target(${SAP_NAME}PresetTests)
        endif()
        suite_presets_add_to_target(${SAP_NAME}PresetTests)
        if(SAP_SETTINGS)
            suite_settings_add_to_target(${SAP_NAME}PresetTests)
        endif()
        target_compile_definitions(${SAP_NAME}PresetTests PRIVATE ${suiteIdentityDefines})

        target_link_libraries(${SAP_NAME}PresetTests PRIVATE ${links})
        target_compile_definitions(${SAP_NAME}PresetTests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)

        add_test(NAME ${SAP_NAME}_PresetTests COMMAND ${SAP_NAME}PresetTests)
    endif()
endfunction()
