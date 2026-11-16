# SuiteSettings: shared settings storage and Settings dialog.
#
# The module sources compile into each consumer plugin, following the same pattern as suite_ui. A static
# library would bundle JUCE modules and cause duplicate symbol errors at link time, so sources are compiled
# directly.
#
# Call suite_ui_add_to_target() first (for SuiteUI::LabelledSlider used by the dialog), then suite_settings_add_to_target().

set(SUITE_SETTINGS_ROOT "${CMAKE_CURRENT_LIST_DIR}")

function(suite_settings_add_to_target target)
    target_sources(${target} PRIVATE
        ${SUITE_SETTINGS_ROOT}/Source/SuiteSettings.cpp
    )

    target_include_directories(${target} PRIVATE ${SUITE_SETTINGS_ROOT}/include)
endfunction()
