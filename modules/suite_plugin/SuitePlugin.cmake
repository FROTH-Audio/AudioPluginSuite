# suite_add_plugin CMake helper - minimal: just registers plugin code globally
# called AFTER juce_add_plugin has been called directly in the plugin CMakeLists.txt
# 
# Usage in plugin CMakeLists.txt:
#   juce_add_plugin(...)  # called directly
#   suite_add_plugin_helper(...) # called after, for modulation/UI/binary data registration

function(suite_add_plugin_helper
  NAME
  COMPANY_NAME
  BUNDLE_ID
  PLUGIN_CODE
  PLUGIN_NAME
  FORMATS
  IS_SYNTH
  IS_MIDI_EFFECT
  NEEDS_MIDI_INPUT
  NEEDS_MIDI_OUTPUT
  EDITOR_WANTS_KEYBOARD_FOCUS
  COPY_PLUGIN_AFTER_BUILD
  MODULATION
  LOCKUP
  SOURCES
  TEST_SOURCES
)

  #---- Append plugin code to global registry ----
  if(NOT DEFINED SUITE_PLUGIN_CODES)
    set(SUITE_PLUGIN_CODES "")
  endif()

  # Append this code to the global list
  set(SUITE_PLUGIN_CODES "${SUITE_PLUGIN_CODES};${PLUGIN_CODE}")

endfunction(suite_add_plugin_helper)