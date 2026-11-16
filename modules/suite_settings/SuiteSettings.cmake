# Module: suite_settings (plugin settings store)
#
# One shared PropertiesFile (XML) holds every setting: tooltips on/off, the no-playhead
# fallback tempo, per-plugin user preset folders and Sgraffito's sample search folder.
# Nothing here is a plugin parameter and nothing joins a saved session.
#
# suite_settings_add_to_target(<target>) is defined here and must be called by every consumer.
# It is not a library target: like suite_ui, suite_modulation and suite_presets, the sources are
# compiled into each consumer, because a static library that links JUCE modules produces
# duplicate symbol errors at link time.
#
# suite_presets_add_to_target() calls this automatically (PresetFile reads the preset-folder
# override from the shared store), so a plugin only calls it directly for its own editor UI.

set(SUITE_SETTINGS_ROOT "${CMAKE_CURRENT_LIST_DIR}")

function(suite_settings_add_to_target target)
    target_sources(${target} PRIVATE
        ${SUITE_SETTINGS_ROOT}/Source/SuiteSettings.cpp
    )

    target_include_directories(${target} PRIVATE ${SUITE_SETTINGS_ROOT}/include)
endfunction()
