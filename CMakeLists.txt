cmake_minimum_required(VERSION 3.22)
project(AudioPluginSuite VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Apple clang fuses a multiply and an add into one rounding by default on arm64 (-ffp-contract=on).
# JUCE's parameter snapping then leaves a residue of about 1.5e-6 where the value is exactly zero, so an
# exact-zero default such as the LFO Offset reads 1.5e-6 and the test fails. Windows does not fuse, and passes.
# Turning contraction off makes Apple arithmetic match. It must come before JUCE is fetched, so JUCE gets it too.
if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-ffp-contract=off)
endif()

option(SUITE_BUILD_PLUGINS "Build the VST3 / AU / Standalone plugins" ON)
option(SUITE_BUILD_TESTS "Build and register the unit tests" ON)

# The preset bar, .ausp preset files and user presets ship in every build, and the free factory
# bank (INIT-only since plan 03.3) is always embedded. The bank is parsed by modules/suite_presets.
include(${CMAKE_SOURCE_DIR}/modules/suite_presets/SuitePresets.cmake)

# JUCE is pinned to a release tag so that builds are reproducible.
include(FetchContent)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.5
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# Metadata written into the VST3 and AU bundles, and shown by hosts. The e-mail is left empty on purpose.
set_property (GLOBAL PROPERTY JUCE_COMPANY_WEBSITE "https://github.com/FROTH-Audio/AudioPluginSuite")
set_property (GLOBAL PROPERTY JUCE_COMPANY_COPYRIGHT "Copyright (C) 2026 FROTH Audio. Licensed under the GNU GPL v3.")

# Applied to every target, including JUCE's own modules.
add_compile_definitions(
    JUCE_VST3_CAN_REPLACE_VST2=0   # no legacy VST2 flags
    JUCE_WEB_BROWSER=0             # the suite does not use web views or curl
    JUCE_USE_CURL=0
)

# AU is macOS-only. Windows builds VST3 and Standalone.
if(APPLE)
    set(SUITE_PLUGIN_FORMATS VST3 AU Standalone)
else()
    set(SUITE_PLUGIN_FORMATS VST3 Standalone)
endif()

enable_testing()

if(SUITE_BUILD_TESTS OR SUITE_BUILD_PLUGINS)
    add_subdirectory(modules/core_dsp)
    add_subdirectory(modules/suite_state)
endif()

add_subdirectory(modules/suite_presets/factory_bank)  # embedded bank data target (INIT-only), adds first
add_subdirectory(modules/suite_presets)  # PresetBar, .ausp I/O, menu model (ships in every build)

if(SUITE_BUILD_PLUGINS)
    include(${CMAKE_SOURCE_DIR}/modules/suite_modulation/suite_modulation.cmake)
    include(${CMAKE_SOURCE_DIR}/modules/suite_ui/SuiteUI.cmake)
    include(${CMAKE_SOURCE_DIR}/modules/suite_plugin/SuitePlugin.cmake)
    add_subdirectory(modules/suite_modulation)
    add_subdirectory(modules/suite_ui)
    add_subdirectory(plugins/SpectralDrift)
    add_subdirectory(plugins/DynamicSaturation)
    add_subdirectory(plugins/MicroTimingChaos)

    if(SUITE_BUILD_TESTS)
        add_subdirectory(tests/host)
    endif()
endif()
