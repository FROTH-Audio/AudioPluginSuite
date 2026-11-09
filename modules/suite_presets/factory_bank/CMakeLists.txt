# The optional embedded factory bank: one data target with bank/AudioSuitePresets.xml. The sources
# are compiled into each consumer by suite_presets_add_to_target(). Only added when
# SUITE_PRESET_BANK is on.
juce_add_binary_data(SuitePresetBankData
    SOURCES ${CMAKE_CURRENT_LIST_DIR}/bank/AudioSuitePresets.xml
    HEADER_NAME SuitePresetBankData.h
    NAMESPACE SuitePresetBankData)
