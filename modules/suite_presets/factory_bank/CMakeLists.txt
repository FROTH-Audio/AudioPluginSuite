# The embedded factory bank: one data target with bank/AudioSuitePresets.xml (the free, INIT-only
# factory bank since plan 03.3). The sources are compiled into each consumer by
# suite_presets_add_to_target().
juce_add_binary_data(SuitePresetBankData
    SOURCES ${CMAKE_CURRENT_LIST_DIR}/bank/AudioSuitePresets.xml
    HEADER_NAME SuitePresetBankData.h
    NAMESPACE SuitePresetBankData)
