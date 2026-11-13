# Sgraffito Plugin Fixes Summary

## Issues Fixed

### 1. Surface Effects Undefined Variables (PluginProcessor.cpp lines 623-654)
**Problem**: The surface effects code was using undefined variables `dustParam`, `crackleParam`, `wowParam`, `flutterParam` which were not in scope for the voice processing loop.

**Fix**: Changed all references to use `apvts.getRawParameterValue(id)->load()` directly in the correct scope.

### 2. Surface Effects Phase Never Incremented
**Problem**: The surface effects phase values (`phaseDust`, `phaseCrackle`, `phaseWow`, `phaseFlutter`) were never being incremented, so the effects were only applied once when first enabled.

**Fix**: Added proper phase advancement for each effect:
- Dust: +0.1 per block (slow LF modulation ~0.1-6 Hz)
- Crackle: +10.0 per block (fast HF texture ~50-210 Hz)  
- Wow: +0.5 per block (medium LFO-like ~0.5-4 Hz)
- Flutter: +5.0 per block (faster LFO-like ~3-11 Hz)

Each phase wraps at 1.0 to stay in the [0, 1) range.

### 3. Added Sample Loading UI (PluginEditor.cpp)
**Problem**: The `loadSample()` method existed but there was no button to trigger it.

**Fix**: 
- Added `loadSampleButton` member variable
- Initialized button in constructor with proper bounds next to preset bar
- Added `browseForSample()` method that opens a file dialog
- Updated layout functions to show/hide the button on the SOUND page

## Issues Still Needing Attention

### 1. Missing Audio Playback Code
**Critical**: The PluginProcessor.cpp file does NOT contain code that actually:
- Reads from the loaded sample buffer
- Applies voice rate, position, and gain
- Writes audio to the output buffer

The plugin uses CoreDSP::Sampler::VoicePool which tracks voices but there's no `processVoices()` or similar function that outputs audio.

**Action Required**: Need to implement the actual sample playback loop that:
1. Iterates through active voices
2. For each voice, reads samples from `getSampleBuffer()` at `voice.position` with rate `voice.rate`
3. Applies `voice.gain` (which includes memory decay and surface effects)
4. Writes to `buffer.getWritePointer(channel)`

### 2. Voice Release Logic Issue
**Problem**: When a note-off occurs for a key, ALL voices of that key have their release envelopes reset via `voicePool.releaseVoice()`. This can cause:
- Voices to be released prematurely if they're already releasing
- Audio glitches when presets with long release times are used

**Action Required**: Modify `handleNoteOff` to check if a voice is already in release before resetting its envelope. Only reset if the release hasn't started yet (`voice.envEnd < 0`).

### 3. Surface Effects Logic Issue  
**Problem**: The surface effects code checks `> 0.5f` before applying effects, but this means:
- Effects only apply when modulated value is in top half of range
- Effects should probably use the full range or a different threshold

**Action Required**: Either change threshold to `>= 0.0f` or adjust logic to properly handle bipolar modulation.

## Preset Issues

### "Velvet Swing" and Other Problematic Presets
These presets have extreme parameter values (e.g., release=400ms, swing=65%) that can cause:
1. Long release times causing voices to pile up
2. Extreme swing values affecting timing calculations
3. Potential rate instability from aggressive surface effects

**Mitigation**: 
- Add validation in preset loading to clamp extreme values
- Improve voice stealing order to prefer releasing long-releasing voices first
- Add debug logging to track when/why voices are released

## Testing Checklist

- [ ] Build the plugin with `cmake --build build --config Release`
- [ ] Run full CTest suite: `ctest --test-dir build -C Release --output-on-failure`
- [ ] Test loading a WAV file via the new button
- [ ] Test that "Velvet Swing" preset no longer causes audio to stop
- [ ] Verify surface effects are audible when enabled
- [ ] Check that phase modulation is cycling correctly

## Files Modified

1. `plugins/Sgraffito/Source/PluginProcessor.cpp` - Fixed surface effects undefined variables and added phase advancement
2. `plugins/Sgraffito/Source/PluginEditor.h` - Added loadSampleButton member
3. `plugins/Sgraffito/Source/PluginEditor.cpp` - Added button initialization, browseForSample method, and layout updates
