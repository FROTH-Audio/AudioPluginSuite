# Concept and build plan: Sgraffito, a memory sampler

Written for: the owner, for the concept and the creative decisions, and a coding agent that will build it later from section 6 onward. Read `AGENTS.md` first, and `docs/plans/00_framework-readiness.md` for the recipe this plugin should use.

Status: Phase 1 (engine) built in `core_dsp`, with tests passing. The plugin itself is not started. Every feature below is a proposal and can be cut without changing the rest.

Name: **Sgraffito**, chosen by the owner. The technique of scratching through a surface to reveal the layer beneath, which is what the memory feature does. Searches found no audio product with that name, but that was not a trademark clearance. Pentimento (dBdone) and Vellum (CarbonDSP) are taken, and Underpaint sits too close to 8Dio's Soundpaint. Plugin code `Sgrf` still needs a uniqueness check.

---

## 1. The idea in one paragraph

A **palimpsest** is a manuscript that was scraped and written over, with the older text still faintly visible under the new. **Sgraffito** is a playable sampler that treats each note as a new layer over the sample rather than a fresh start. The keyboard is a timeline through the sample, the sampler remembers where it has been played, and every key has its own character that slowly drifts over time. Playing a sample is not a single-shot trigger; it's a conversation with a recording that changes as you play it.

Target sound: the places the four genres meet. Cinematic swells, slow-jazz laid-back timing, ambient drift, and chill-hop chopping, all from one sample.

---

## 2. What makes it different

Most samplers map keys to pitch or to slices, one sample, one key, one fixed result. Sgraffito adds four behaviours no single common sampler offers together:

1. **The keyboard is a scrubber.** Each key chooses a *region* of the sample (its position and length), and pitch is a separate control. Playing a melody moves through the recording the way a tape operator would.
2. **Memory.** Each note writes a small trace of where it played. The next time that region is played, it comes back slightly changed (a filter tilt, a level, a tiny pitch offset, a grain of noise). Replaying a phrase makes the sampler sound older, not identical.
3. **Organic drift.** Every key has its own detune and timing that wander over time, driven by the chaotic Lorenz attractor the suite already uses in Micro-Timing Chaos (`core_dsp/ChaosEngine.h`). No LFO sounds like this.
4. **Genre gestures as first-class modes.** Tape stop, reverse swell, ghost notes, laid-back timing, chop maps, grain clouds and vinyl dust are built in and controllable from the MOD page, not hidden in effects.

---

## 3. The sound, mode by mode

The plugin has four playback modes. The mode is one choice; the rest of the controls work in all of them.

### 3.1 Scrub (keyboard as timeline)
Each key plays a region of the sample. The region for a key is set by a **key map**, which by default runs left to right across the keyboard. Low keys start early in the sample, high keys start late. Pitch is independent of region, so the same region can be played at several pitches.

- Use: vocal chops, speech, a field recording you want to "play" as an instrument.
- Key controls: `Region Span` (how much of the sample the whole keyboard covers), `Region Length` (how long each key's region is), `Root` (the key whose pitch is unshifted).

### 3.2 Chop (chill-hop breaks)
The sample is cut into slices. Each slice goes to one key, starting at a root key. This is the classic drum-break and loop-chop workflow, plus the extras that make it feel hand-played:

- **Choke groups.** Keys in the same group cut each other off, like a hi-hat and an open hi-hat.
- **Swing and humanise.** A swing amount delays every second slice. Humanise adds a small random timing spread per note. Both are per-slice-count, so they apply to the groove, not to a single note.
- **Ghost slices.** A second, quieter copy of a slice can sound at low velocity, so soft playing adds the second-layer hits that a drummer's ghost notes give.

### 3.3 Cloud (ambient grains)
Each held note becomes a cloud of short grains read from its region. Grain count, grain size, spread (stereo and pitch) and position jitter are controls. Sustain pedal (or a hold setting) keeps a cloud alive after the key is released, so chords build into a pad.

- Use: pads, textures, frozen moments of a sample.
- Each grain is pitched to the key's interval, so a three-note chord makes a three-pitch cloud from one region.

### 3.4 Swell (cinematic)
A note can start from the sample's reverse. The plugin plays the region backwards and crossfades into the forward sample at the end of the swell, so the attack rises into the hit. Swell time is set in milliseconds, and the crossfade length is its own control.

- Use: risers, reversed hits, orchestral entrances.

Modes can be combined per key through a **layer** setting (Scrub + Cloud, for example), but the first version ships with one mode per instance, so the layering question can wait.

---

## 4. The creative features (the list you asked for)

Each feature below has a one-line sound description, the reason it fits this plugin, and the simplest first version.

### 4.1 Memory (the layer beneath)
**Sound.** Replaying a region gives back a slightly different version of it: a little darker, a little lower in level, a grain of noise. The longer it's played, the more the sample carries its history.

**How.** Each region keeps a *trace*: a small record of the last gain, tilt and noise amount applied to it. A note reads the trace, plays with it, then writes a new trace that moves a fraction toward the current note. `Memory Amount` sets how far each note moves the trace; `Memory Decay` sets how fast the trace fades back to clean when nothing is played.

**First version.** Gain and tone only. Traces live in the plugin's state, so a session remembers its history. A `Reset Memory` button clears it.

**Risk.** Traces grow without bound if the decay is zero. Clamp every trace value to a fixed range (for example gain −12 to 0 dB, tone −1 to +1).

### 4.2 Drift (the Lorenz keyboard)
**Sound.** Every key slowly moves its pitch by a few cents and its timing by a few milliseconds, in patterns that never repeat exactly. Two keys played together sound like two performers who are almost, but not quite, in tune.

**How.** One `LorenzAttractor` from `core_dsp/ChaosEngine.h` runs per instance. Each key reads its own offset (three values of the attractor, or one value per key at a different point in the attractor's path). `Drift Amount` sets the depth, `Drift Speed` the rate.

**First version.** Pitch drift only, one shared attractor, with the depth capped at ±15 cents.

### 4.3 Tape stop
**Sound.** On note release, the pitch falls and the level fades like a tape machine being stopped. The time is set in milliseconds, and the curve (linear, exponential, or the classic slow-then-fast) is a choice.

**How.** Note-off does not end the voice. It starts a ramp that lowers the playback rate toward zero over `Tape Stop Time`, then fades. Voices in a choke group are cut instantly, so the choke still works.

**Risk.** A stopped voice still uses a voice slot. Cap the number of simultaneous tape stops (the voice pool is 32 by default, see section 6).

### 4.4 Ghost notes
**Sound.** Soft playing triggers a second, quieter, slightly delayed copy of the note. Hard playing triggers only the main note. This is the most common way a jazz drummer or a pianist adds texture under the melody.

**How.** A velocity threshold (default 40 %). Below it, a ghost copy plays at `Ghost Level` (default −12 dB) after `Ghost Delay` (default 15 ms). Ghosts reuse the note's region and pitch and do not write memory.

### 4.5 Laid-back timing
**Sound.** Every note is played a few milliseconds behind the grid, more on the offbeats, so the sample feels like it's being played by a person, not quantised. Slow-jazz and chill-hop both lean on this.

**How.** A `Laid Back` amount in milliseconds, applied with a swing shape to the note start (not the release). It works with an external clock: when a MIDI generator such as Micro-Timing Chaos sends notes, the delay is applied after the MIDI arrives, so the feel is stable.

### 4.6 Scale lock
**Sound.** Chromatic playing is pulled to the nearest note of a chosen scale, with a `Glide` to the correction, which gives the bent-note feel of an old tape or a slide.

**How.** A table of scale degrees (the same scales as Micro-Timing Chaos: Chromatic, Major, Minor, Minor Pentatonic, Dorian, Whole Tone), applied to the key's pitch before the drift. Off by default.

### 4.7 Vinyl and wow
**Sound.** A low, constant hiss, occasional crackle, and slow wow (a few cents, a few times per second) and flutter (faster, smaller). The sound of a record that has been played many times.

**How.** Dust is a noise source with a crackle density control. Wow and flutter are two slow LFOs from `core_dsp/ModulationEngine.h`, applied to pitch. All three are off by default and live on one page.

### 4.8 Chance map (reshuffle)
**Sound.** A `Shuffle` button moves each key to a different region of the sample while keeping the overall shape of the map. The melody you played still sounds like a melody, but its sample material is new.

**How.** A seeded permutation of the key-to-region table, using the hashed approach of the sequencer (`CoreDSP::Sequencer`). The seed is a state value, so a shuffled map is saved with the session. Map shuffles are undoable in the sense that the previous seed is kept in the state.

### 4.9 Reverse swell
Covered in section 3.4. The extra feature here is that swells remember their own crossfade, so a fast phrase of swells does not click.

---

## 5. Genre recipes

Presets are a later feature (`docs/plans/03_presets.md`). These are the starting points the design should be checked against.

**Cinematic.** Swell mode, a long swell time, Memory on with a slow decay, Drift at a low amount. Playing a rising scale over a long sample gives the slow approach to a climax. The Memory trace makes the second and third passes darker, which reads as tension building.

**Slow jazz.** Scrub mode with a short Region Length, Laid Back around 12 ms, Ghost notes on (a 40 % threshold), Scale lock on Dorian or Minor, Tape Stop on release at about 400 ms. The result is a sample played like a piano with a human feel.

**Ambient.** Cloud mode, a sustain pedal held down, a large Grain Size with wide Spread, Drift at a medium amount, Memory at a long decay. Held chords become a slowly moving pad whose texture changes with every chord.

**Chill-hop.** Chop mode with 16 slices, Choke groups on the hats, Swing at about 58 %, Humanise at a few ms, Vinyl and Wow on at low levels. A MIDI pattern from a sequencer plays the break with a loose groove.

---

## 6. Build plan

### 6.1 Identity and format

- Product name: "Sgraffito". Plugin code: `Sgrf`. Manufacturer `Asui`, bundle ID `com.audiosuite.sgraffito`. Confirm `Sgrf` is unused. `tools/new_plugin/new_plugin.py` refuses a code that is already in use, and `suite_add_plugin()` fails at configure time on a duplicate.
- Instrument: `IS_SYNTH TRUE`, `NEEDS_MIDI_INPUT TRUE`, `NEEDS_MIDI_OUTPUT FALSE`, `IS_MIDI_EFFECT FALSE`. Stereo output. The VST3 category will be an instrument (the same reasoning as the Micro-Timing Chaos change in section 8).
- Formats: VST3, Standalone, and AU on macOS, as the other plugins.

### 6.2 Architecture

```
modules/core_dsp/include/
  SamplerEngine.h        Voice, grain scheduler, region map, tape-stop ramp, memory trace (header-only, no JUCE GUI)
  SamplerMaps.h          key map, scale lock, shuffle permutation (pure functions)
modules/suite_sampler/   (only if the sample loader needs its own files; otherwise keep in the plugin)
plugins/Sgraffito/
  Source/PluginProcessor.{h,cpp}   APVTS, MIDI handling, sample storage, voice pool, state
  Source/PluginEditor.{h,cpp}      Waveform, key map strip, mode selector, MOD page
  Source/SampleLoader.{h,cpp}      Reads WAV, AIFF or FLAC, writes FLAC for embedding, on the message thread; hands over a finished buffer
  tests/PluginTests.cpp
```

Keep the DSP header-only in `core_dsp` (as `ChaosEngine.h` and `SpectralEngine.h` do), so it can be tested without a plugin and reused. The processor handles MIDI and the voice pool. The editor never touches the audio thread.

### 6.3 Sample loading and threading

- Load with `juce::AudioFormatManager` (`juce_audio_formats`). In the pinned JUCE 9.0.5, WAV, AIFF and FLAC are all readable. FLAC is enabled by default (`JUCE_USE_FLAC 1`), and `FlacAudioFormat` has a writer (`createWriterFor`), so the plugin can both read and write it. Accept WAV, AIFF and FLAC on import.
- Embedded samples are stored as FLAC, not WAV. Lossless, so no quality loss, and typically a fraction of the WAV size. Imported samples are converted to FLAC once, at load time, on the message thread.
- Convert the sample to the host's sample rate at load time, using JUCE's interpolators (`LagrangeInterpolator`, `CatmullRomInterpolator` and `WindowedSincInterpolator` in `juce_audio_basics`). Do not resample on the audio thread.
- The loaded sample is a `juce::AudioBuffer<float>` held in a reference-counted pointer. Loading builds a new buffer on the message thread and swaps it in with one atomic pointer. The audio thread reads the pointer once per block and never locks.
**Storage policy (decided, pending the owner's confirmation in section 7):**

- **Embed by default.** A sample whose FLAC-compressed size is at or below the embed limit is stored in the plugin state as FLAC bytes (base64 in the XML, or as a binary child element). The session is then self-contained and plays on another machine. The embed limit is a visible setting, default 20 MB, and it is stored in the state so a session keeps the limit it was saved with.
- **Path above the limit.** A sample over the limit is stored as its absolute file path plus a relative path from the project folder when one can be found. The UI shows a warning that the sample is not embedded and will not travel with the session.
- **Missing files.** If a stored path is missing when the session loads, the plugin keeps every setting, shows "sample not found" with a relink button, and plays silence for the affected notes. It does not fail silently and does not reset the other parameters.
- **Duplication cost.** Each instance holds its own copy of an embedded sample in memory and in its host session. Show the embedded size next to the sample name, so a user can see when a session is getting large.
- **Encode once, copy on save.** The FLAC bytes are encoded once, when the sample is loaded, on the message thread, and cached next to the audio buffer. `getStateInformation` only copies the cached bytes. It never encodes, so a host calling it at an awkward moment cannot stall on compression. The same goes for decoding on reload: it happens on the message thread, and the audio thread only sees the finished buffer through the atomic pointer.

### 6.4 Voices and polyphony

- A pool of 32 voices by default. A voice holds a read position in the sample, a rate, a gain, an envelope and a mode.
- Stealing: when all voices are busy, the oldest voice in release is taken first, then the oldest voice overall. Tape-stop voices are stolen first, since they are already fading.
- Choke groups are checked at note-on. A choke cuts the group's other voices immediately, with a 2 ms fade to avoid clicks.

### 6.5 Parameters (first draft, to be checked against the existing IDs)

These are starting points. Parameter IDs are permanent once released (AGENTS.md section 7), so list them in the processor header before writing the layout.

| Group | IDs (draft) | Range | Default |
|---|---|---|---|
| Mode | `mode` | Scrub, Chop, Cloud, Swell | Scrub |
| Region | `regionSpan`, `regionLength`, `rootKey` | 0 to 100 %, 1 to 100 %, 24 to 96 | 100 %, 25 %, 60 |
| Pitch | `pitchFine` (cents), `scaleLock`, `scale`, `glide` | −100 to +100, off/on, six scales, 0 to 500 ms | 0, off, Minor, 50 ms |
| Level | `level` (dB), `velocityCurve` | −60 to +6, 0.5 to 2 | 0 dB, 1 |
| Envelope | `attack`, `release`, `tapeStopTime`, `tapeStopCurve` | 1 to 500 ms, 10 to 5000 ms, 0 to 5000 ms, 3 choices | 5, 200, 400 ms, exponential |
| Memory | `memoryAmount`, `memoryDecay`, `resetMemory` (action) | 0 to 100 %, 0 to 60 s | 20 %, 10 s |
| Drift | `driftAmount`, `driftSpeed` | 0 to 100 %, 0.01 to 2 Hz | 0 %, 0.1 Hz |
| Timing | `layBack` (ms), `swing`, `humanise` (ms) | 0 to 30, 50 to 75 %, 0 to 15 | 0, 50 %, 0 |
| Ghosts | `ghostOn`, `ghostThreshold`, `ghostLevel`, `ghostDelay` | off/on, 1 to 100 %, −40 to 0 dB, 0 to 50 ms | off, 40 %, −12 dB, 15 ms |
| Chop | `sliceCount`, `chokeGroups` (per key) | 2 to 32, on/off | 16 |
| Cloud | `grainSize`, `grainDensity`, `grainSpread`, `sustainHold` | 5 to 300 ms, 1 to 60 per second, 0 to 100 %, off/on | 60 ms, 12/s, 40 %, off |
| Swell | `swellTime`, `swellCrossfade` | 50 to 4000 ms, 5 to 200 ms | 800 ms, 40 ms |
| Vinyl | `dust`, `crackle`, `wow`, `flutter` | 0 to 100 % each | 0 |
| Map | `shuffleSeed` (state, not automated) | 0 to 9999 | 1 |

Modulation (section 6.8) adds the routing parameters, as in the other plugins.

### 6.6 Phases

Each phase ends with a passing CTest run. The first three phases need no GUI.

**Phase 0: prerequisites.** R1 (state versioning) is done in `modules/suite_state`. R2 is done: `suite_add_plugin()` and `tools/new_plugin/new_plugin.py`. The sampler should be the first plugin written with the recipe, not copied from an old one. Also settle the Micro-Timing Chaos MIDI declaration (section 8), since the sampler will be fed by it.

**Phase 1: engine, no plugin. DONE.** `SamplerEngine.h` and `SamplerMaps.h` in `core_dsp`, with tests in `DSPTests.cpp` (seven sampler test functions; all CoreDSP and CTest entries pass):
- region mapping: the lowest key starts at 0, the highest at the end of the span, and the root key is unshifted;
- chop map: the number of slices and their boundaries;
- shuffle: the same seed gives the same map, and every region is used once;
- tape stop: the rate reaches zero at the set time and the output is silent after the fade;
- memory trace: a repeated note moves the trace toward its target and stays inside its clamp;
- ghost threshold: a velocity just below the threshold triggers a ghost, and one just above does not;
- scale lock: every output note is a member of the scale.

**Phase 2: plugin shell.** The processor with the parameter layout, the voice pool, MIDI note-on and note-off, and state round trip (the R1 helper). No GUI beyond a placeholder. Tests: parameter IDs and ranges, a note produces sound, a note-off decays to silence, and a state round trip keeps every value.

**Phase 3: sample loading.** `SampleLoader`, the atomic buffer swap, FLAC embedding, the path fallback and the missing-file state. Tests: a loaded WAV plays the correct region for a given key, an embedded sample reloads to the same audio (compare sample by sample after decoding), a sample over the limit is stored as a path, a missing path keeps all other parameters and plays silence for the sample, and the embed limit saved in the state is the one used on reload.

**Phase 4: modes.** Scrub, Chop, Cloud and Swell, one at a time, each with a test that the mode's defining property holds (for example, Cloud's grain count matches the density control).

**Phase 5: creative features.** Memory, drift, tape stop, ghosts, laid-back timing, scale lock, vinyl and wow, and the reshuffle, in that order. Each is tested in isolation and has its own default off.

**Phase 6: the editor.** Waveform with region markers and slice lines, a key-map strip under the waveform, a mode selector (`SuiteUI::SegmentedSelector`), and the MOD page (`SuiteModulation::ModulationPanel`). Use the suite's palette and brand lockup. The window size is set when the layout is final. Render-check it against a baseline, as the other editors do.

**Phase 7: modulation and documentation.** Add `SuiteModulation::Router` with a target table of the controls that suit modulation (level, region span, pitch fine, drift amount, memory amount, grain size, grain density). Exclude the mode and the sample itself. Update AGENTS.md, the README, the CHANGELOG and a plugin README.

### 6.7 Testing

- DSP tests in `core_dsp`, as in phase 1. These are the fastest and catch most mistakes.
- Plugin tests: the parameter checks used by the other plugins (`checkParameterIds`, `checkRange`), and the state round trip with every parameter changed.
- Audible-effect tests: a note with each feature on changes the output (memory, drift, tape stop, ghosts). A note with every feature off matches a reference render (the byte-identical check used for MicroTimingChaos's probe).
- Host load test: the VST3 host test loads the built bundle, as for the other plugins.
- Performance check: 32 voices at 48 kHz on the owner's machine, Release, under 25 % of one core. Record the number in the plugin's README.

### 6.8 Modulation

Use `SuiteModulation::Router` with the same pattern as the other plugins (`AGENTS.md` section 9, "Adding modulation to a plugin"). A few controls are excluded on purpose:

- `mode`, a choice, cannot be modulated smoothly.
- `shuffleSeed`, `resetMemory`, `sliceCount`, and the choke groups change the structure, not the sound, so they are excluded.
- `scale` is excluded for the same reason as Micro-Timing Chaos's Scale: it changes the pitch set abruptly.

---

## 7. Open decisions for the owner

1. **Sample storage (recommendation accepted in principle; confirm).** Embed as FLAC up to a 20 MB visible limit, store a path above it, and show "sample not found" with a relink button when a path is missing (section 6.3). Confirm the default limit and the behaviour.
2. **Sample sources.** Factory samples need a licence that allows redistribution. Only record your own material, or use a CC0 source with its licence noted in the README.
3. **Modes in one instance.** Ship one mode per instance (default), or allow layers from the start? Layers double the CPU cost and add a second design question.
4. **Memory on by default?** The Sgraffito character is strongest with memory on, but it changes what a new user hears first. Default: off, with a one-click "Sgraffito" preset.
5. **Name.** "Sgraffito" is the chosen name. Run a trademark clearance before any public release.
6. **Scope of the first release.** Modes 3.1 to 3.4 and features 4.1 to 4.4 are a first release. Sections 4.5 to 4.8 can follow.

---

## 8. Related: Micro-Timing Chaos as a MIDI instrument

**Change made in this session:** `plugins/MicroTimingChaos/CMakeLists.txt` now sets `IS_SYNTH TRUE` and keeps `IS_MIDI_EFFECT TRUE`.

- In the pinned JUCE 9.0.5 source, the VST3 category depends only on `IS_SYNTH`. With `IS_SYNTH FALSE`, the plugin was registered as an audio effect (`kFx`). With `TRUE`, it is registered as an instrument (`kInstrumentSynth`).
- `IS_MIDI_EFFECT` maps to the AU MIDI processor type (`AU_MAIN_TYPE kAudioUnitType_MIDIProcessor`). It doesn't set a VST3 category.
- The stereo output bus is kept, as before. Instruments normally have audio output, and removing it would bring back the DAW scanner risk recorded in `AGENTS.md` section 7.

**Verification status.** The change is not yet verified in the working tree. At the time of writing, the working tree does not build because of uncommitted edits to `modules/core_dsp/include/ModulationTiming.h` that include a non-existent header (`"mod_target_types.h"` with escaped quotes). Those edits are not part of this change and were not touched. A clean copy of the last commit with only this change is being built and tested separately. See the status note in the final report.

**Risk to check in a DAW.** Changing the VST3 category can make a DAW show the plugin under instruments, and may need a rescan. Saved sessions keep their parameter values.

---

## 9. Creative prompts for later

Ideas kept out of the first release, to be picked up once the core is working:

- **Two-sample crossfade**: a key morphs between two samples over its velocity.
- **Self-resampling**: record the output back into the sample during play, so the instrument's history becomes new material. This is the logical end of the memory idea.
- **Sample-triggered movement**: the amplitude of the input signal (in a sidechain) controls the drift, so the sampler breathes with a source.
- **Sequencer-driven reshuffle**: every bar, the map reshuffles on a MIDI-clock tick. Pairs well with Micro-Timing Chaos.
- **Export of traces**: save a region's memory trace as a curve, to apply to other samples.

## Notes for the next agent

- Create the plugin with `python tools/new_plugin/new_plugin.py Sgraffito "Sgraffito" Sgrf`, then edit it. Do not copy an old plugin by hand.
- Keep the DSP in `core_dsp` and the plugin thin, as this plan says. The engine has tests; add tests wherever the logic is.
- Do not use the name publicly until `05_trademark-search-list.md` is done.
