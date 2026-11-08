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
#   COMPANY_NAME  Default: AudioSuite.
#   VERSION       Default: ${PROJECT_VERSION}.
#   LOCKUP        Brand lockup PNG, embedded as binary data.
#   ASSETS        Other files embedded in the same binary data target as LOCKUP.
#   MODULATION    Adds suite_modulation (LFOs and routing). Off by default.
#   MIDI_EFFECT   Makes a MIDI effect: IS_SYNTH, IS_MIDI_EFFECT, NEEDS_MIDI_INPUT and NEEDS_MIDI_OUTPUT are set.
#
# Needs suite_ui_add_to_target (modules/suite_ui/SuiteUI.cmake) and, with MODULATION, suite_modulation_add_to_target.
# Variables set inside the function do not leak out: everything the plugin needs is a target property.

set_property(GLOBAL PROPERTY SUITE_PLUGIN_CODES "")

function(suite_add_plugin)
    set(options MODULATION MIDI_EFFECT)
    set(oneValue NAME PRODUCT_NAME MANUFACTURER PLUGIN_CODE BUNDLE_ID COMPANY_NAME VERSION LOCKUP)
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
        set(SAP_COMPANY_NAME "AudioSuite")
    endif()
    if(NOT SAP_VERSION)
        set(SAP_VERSION "${PROJECT_VERSION}")
    endif()
    if(SAP_MIDI_EFFECT)
        set(midiFlag TRUE)
    else()
        set(midiFlag FALSE)
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
        IS_SYNTH ${midiFlag}
        IS_MIDI_EFFECT ${midiFlag}
        NEEDS_MIDI_INPUT ${midiFlag}
        NEEDS_MIDI_OUTPUT ${midiFlag}
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
    )

    target_sources(${SAP_NAME} PRIVATE ${SAP_SOURCES})
    suite_presets_add_to_target(${SAP_NAME})
    target_include_directories(${SAP_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
    )

    suite_ui_add_to_target(${SAP_NAME})
    if(SAP_MODULATION)
        suite_modulation_add_to_target(${SAP_NAME})
    endif()

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
        suite_presets_add_to_target(${SAP_NAME}Tests)

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

        target_link_libraries(${SAP_NAME}Tests PRIVATE ${links})

        # The editor tests pump the message loop so timers fire.
        target_compile_definitions(${SAP_NAME}Tests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)

        add_test(NAME ${SAP_NAME}_PluginTests COMMAND ${SAP_NAME}Tests)
    endif()
endfunction()
