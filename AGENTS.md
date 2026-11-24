# AGENTS.md: Audio Plugin Suite

Knowledge base for coding agents working on this repository. Read this before changing anything. It records what is built, why it is built that way, what is verified and what is not, and the JUCE and CMake pitfalls already hit.

Keep this file in step with the code. When you change a parameter range, a default, a latency, a test count, or a file layout, update the matching section in the same change. The user-facing docs are `README.md`, `CHANGELOG.md` and each plugin's `plugins/<Name>/MANUAL.md` (MicroTimingChaos also keeps a technical `README.md`).

The original brief (`coding_agent_prompt.md`) is no longer in the repository. Section 12 lists where the built code differs from it.

---

## 0. Framework readiness summary

The suite framework is ready for new plugin development:

- **R1 State versioning**: Done via `modules/suite_state`. Sessions carry a `suiteStateVersion` attribute; older states load unchanged, newer states are refused.
- **R2 Build recipe**: Done via `suite_add_plugin()` in `modules/suite_plugin/SuitePlugin.cmake` and the generator `tools/new_plugin/new_plugin.py`. New plugins are created with one CMake function call.
- **R3 Editor UI base**: Done. MicroTimingChaos's LookAndFeel now derives from `SuiteUI::SuiteLookAndFeel`, sharing fonts, layout setup, and common widget drawing. The refresh rate is a single constant `SuiteUI::kRefreshHz = 60`. One minor decision remains: the MicroTimingChaos PREVIEW tab first-render effect (owner-decided baseline in plan 02).

New plugins cost about one day of processor/editor scaffolding work, plus DSP and tests. The shared modules (`core_dsp`, `suite_modulation`, `suite_ui`) are independent and have no plugin-specific code.

---

## 1. What this project is

Eight audio plugins written with JUCE 9.0.5 in C++17. Formats: VST3 on Windows and macOS, Audio Unit on macOS, and Standalone on both.

| Plugin | Folder | Kind | DSP core | Editor accent |
|---|---|---|---|---|
| Spectral Drift Shifter | `plugins/SpectralDrift` | audio effect | `CoreDSP::Spectral::DriftEngine` | cyan `#2DE2FF` |
| Dynamic Saturation Companion | `plugins/DynamicSaturation` | audio effect | `CoreDSP::Saturation` | amber `#FF9F1C` |
| Micro-Timing Chaos Sequencer | `plugins/MicroTimingChaos` | MIDI effect, with stereo pass-through | `CoreDSP::Chaos::LorenzAttractor`, `CoreDSP::Sequencer` | lime `#7DFF5A` |
| Sgraffito Memory Sampler | `plugins/Sgraffito` | Memory sampler (memory traces, tape-stop, granular cloud) | `CoreDSP::Sampler::DriftEngine`, `CoreDSP::Sampler::VoicePool`, `CoreDSP::Sampler::quantizeToScale` | slate `#9aa0a6` |
| Stereo Tool Effect | `plugins/StereoTool` | audio effect | `CoreDSP::Stereo` (mid/side, width, drive, saturation characters, transient follower, side filter) | violet `#B18CFF` |
| MIDI Chord Arp Scale Tool | `plugins/MidiEffect` | **instrument** (output-only buses, notes out; audio buffer cleared) | `CoreDSP::Chords` (chords, arp patterns, step-clock helpers), `CoreDSP::Sampler::quantizeToScale` | teal `#4ECDC4` |
| MultiSat | `plugins/MultibandSaturation` (host name "MultiSat"; folder/target/preset ID keep the MultibandSaturation name) | audio effect | `CoreDSP::Multiband::TwoBandSplit`, `CoreDSP::Saturation` shapers/followers | coral `#FF6B6B` |
| Cyclical | `plugins/Cyclical` | **instrument** (output-only, per-slot notes) | `CoreDSP::Loop` (LoopEngine: regions, crossfaded seams, ping-pong, grains) | tangerine `#FF7A45` (plan 14) |
- Manufacturer name `FROTH Audio` (shown by hosts; set by `COMPANY_NAME` in `modules/suite_plugin/SuitePlugin.cmake`), manufacturer code `Asui`, bundle IDs `com.audiosuite.<name>` (kept: they are part of the plugin identity, section 7).
- Plugin code `Sgrf`. Codes and parameter IDs are saved in sessions

- **Minimum 15 presets**: Each new plugin must include at least 15 factory presets bundled with the plugin binary, exportable through the preset bank build. The preset bank (`SUITE_PRESET_BANK`) generates these from the schema defined in each plugin's processor. This ensures every plugin ships with a functional default set of presets immediately upon installation. and **must never change** after a release.
- Licence: GNU GPL v3 (`LICENSE`, the unmodified GNU text). JUCE is used under its GPLv3 option; distribution stays under the GPL.
- State versioning: saved sessions carry a `suiteStateVersion` attribute. States without the attribute are version 1. Newer states than the plugin are refused and leave the current state unchanged.

- **DSP engine** (Phase 4): Lorenz chaos drift, tape stop, ghost notes, scale lock, cloud granular mode. 71 parameters total (37 original + 34 new: 2 LFOs + 6 modulation slots + 21 creative parameters).
- **Editor** (Phase 5, Sgraffito): 880×700 px window, SOUND/TEXTURE/MOD pages, 60 Hz timer, slate accent colour `#9aa0a6`.
- **Modulation** (Phase 5): `SuiteModulation::Router` shared with other plugins, 2 LFOs, 6 routing slots (source/target/amount/delay). All parameters automatable, savable, restorable.
- **Presets**: 240 factory presets (36 SD / 46 DS / 38 MTC / 30 Sgraffito / 15 MidiEffect / 25 StereoTool / 25 Multiband / 25 Cyclical, plans 6.1, 11, 13 and 14 — plan 13 added +10 modulation presets each to SD, DS, StereoTool and MultiSat, plan 14 added +10 concept presets to Cyclical, plan 18 Phase 7 added +10 transform/LFO-waveform presets each to DS, MTC and Sgraffito) behind the `SUITE_PRESET_BANK` flag (full bank accepted by owner decision, 2026-11-12; five Sgraffito presets added by plan 05.3 F1.3; the two first-pass plugins joined in plan 6.1, then Cyclical, then Multiband Saturation in plan 11). Bank + programs (`SUITE_PRESET_BANK_AS_PROGRAMS`) maps Init=program 0, factory presets=programs 1..N.
- **Test results**: 28/28 CTest entries pass with the factory bank on (last full run 2026-11-24, plan 18; the default build is 20, the bank build adds eight preset-test executables).

---

## 2. Repository layout

```
CMakeLists.txt                    Root: JUCE pin, global definitions, options, SUITE_PLUGIN_FORMATS
AGENTS.md / CLAUDE.md             This file / Claude Code specifics (points here)
README.md, CHANGELOG.md, LICENSE  User and developer docs, history, licence
docs/plans/                       Live plans only. Index and execution order: docs/plans/README.md. Finished plans are deleted (outcome lives in AGENTS.md, CHANGELOG.md and the code)
modules/
  core_dsp/                       Header-only DSP, depends on juce::juce_dsp
    include/CoreDSP.h             Umbrella header; decibelsToGain()
    include/SaturationEngine.h    Shapers, fold, OnePoleFilter, ToneControl, EnvelopeFollower
    include/ChaosEngine.h         LorenzAttractor (RK4), LorenzParams, LorenzState
    include/SpectralEngine.h      DriftEngine (overlap-add FFT); the drift is driven from the routed LFO's value
    include/SequencerEngine.h     CoreDSP::Sequencer: step rates, hashed pattern and melody, timing, swing, gate, velocity
    include/ModulationEngine.h    CoreDSP::Modulation: LFO shapes (cycles in, value in [-1, 1]) and the normalised offset clamp
    include/ModulationTiming.h    CoreDSP::Modulation: division table (beats per cycle), fractionalPart, cyclesFromSeconds
    tests/DSPTests.cpp            CoreDSPTests (47 test functions)
    CMakeLists.txt                CoreDSP INTERFACE target, CoreDSPTests
  suite_modulation/               Shared LFOs, routing slots, LFO scope, modulation panel, control display. NOT a library target, see section 8.
    include/SuiteModulationRouter.h   Router: LFO and slot parameters (addParameters), state, offsets, modulate(), publish. Also hostTempoOrDefault(), the tempo the effects' synced LFOs use
    include/SuiteModulationScope.h    LfoScope: one LFO cycle with the current position
    include/SuiteModulationPanel.h    ModulationPanel: LFO strips and the six routing rows (uses SuiteUI::LabelledSlider)
    include/SuiteModulationControls.h ControlDisplay: shows modulated values on sliders, skips the one under the mouse
    Source/*.cpp                  Implementations
    tests/SuiteModulationTests.cpp  SuiteModulationTests (10 test functions), with a stub processor
    suite_modulation.cmake        suite_modulation_add_to_target(<target>): adds sources + include dir. Needs suite_ui_add_to_target on the same target
  suite_ui/                       Shared UI. NOT a library target, see section 8
    include/SuiteLookAndFeel.h    SuiteUI::Colours: the neutral palette shared by all plugins, SuiteLookAndFeel (LookAndFeel_V4), and kRefreshHz (the 60 Hz editor timer rate)
    include/SuiteWidgets.h        SpectrumDisplay, TrailPlot, CurveView, LevelMeter, SegmentedSelector, LabelledSlider, StereoLevelMeter (plan 13: the stereo pair — input left, output right, both full height) and PlayableKeyboardStrip (hardened in plan 13: held-note tracking, a mouseExit release, releaseHeldNotes() and a destructor flush)
    include/SuiteBrand.h          drawBrandLockup(): draws a brand lockup image at the left of a header area
    brand/                        Generated artwork: brand_mark.png and lockup_<plugin>.png. The lockups are made by the owner's template generator tools/brand/lockups.py (one table row per plugin; the script is not in this repository, see section 14); do not edit the PNGs by hand
    Source/*.cpp                  Implementations
    SuiteUI.cmake                 suite_ui_add_to_target(<target>): adds sources + include dir
    tests/SuiteUITests.cpp        SuiteUITests (10 test functions)
    CMakeLists.txt                SuiteUITests only
  suite_settings/                Shared settings storage and Settings dialog. NOT a library target, see section 8.
    include/SuiteSettings.h       SuiteSettings::Store: global application settings; SuiteSettings::SettingsDialog: the modal dialog (tooltips toggle, fallback tempo, preset folder browse, plus Sgraffito's sample folder when the caller asks for it)
    Source/SuiteSettings.cpp      Dialog implementation; showSettingsDialog() static function
    tests/SuiteSettingsTests.cpp  SuiteSettingsTests (9 test functions)
    suite_settings.cmake          suite_settings_add_to_target(<target>): adds sources + include dir. The dialog uses plain JUCE widgets; no SuiteUI requirement beyond the historic call-order comment
  suite_plugin/                   Build recipe. NOT a library target, see section 9
    SuitePlugin.cmake             suite_add_plugin(...): juce_add_plugin, sources, lockup, links, <Name>Tests and add_test from one call. Checks PLUGIN_CODE is unique
plugins/<Name>/
  CMakeLists.txt                  One suite_add_plugin(...) call (see section 9)
  Source/PluginProcessor.{h,cpp}  APVTS, processing, state, createPluginFilter()
  Source/PluginEditor.{h,cpp}     Editor, 60 Hz Timer reads processor snapshots
  tests/PluginTests.cpp           Plugin tests: SpectralDrift 22, DynamicSaturation 19, MicroTimingChaos 40
plugins/MicroTimingChaos/         Additional files:
  MicroTimingChaosLookAndFeel.h   At the plugin root (not include/). MTC-only colours (axes, LFOs, preview text); the neutrals come from SuiteUI::Colours. Derives from SuiteUI::SuiteLookAndFeel for shared fonts and layout setup. It keeps its own knob, toggle, combo box, linear slider and fonts, so the window is unchanged (plan 02 results).
  Source/MicroTimingChaosLookAndFeel.cpp  Rotary knobs, BPM Sync button, combo box; textures and icons from BinaryData
  Source/LerpCurveEditor.{h,cpp}  The 5-point curve editor (Curve tab)
  Source/LerpPreview.{h,cpp}      The live drift preview (Preview tab)
  The LFO panel and scope are in modules/suite_modulation. The plugin's own copies were removed.
  Resources/icons/*.svg           BPM Sync icons, embedded in the binary
  Resources/textures/*.png        4K textures, embedded and scaled down at load
  generate_texture.py, make_png.py  Scripts that produced the textures (owner's tools)
  README.md                       User documentation for this plugin
tests/
  common/PluginTestUtils.h        Helpers: setParameter, checkRange, checkStateRoundTrip, checkEditor, addSine, ...
  host/PluginHostTests.cpp        Loads a built .vst3 through JUCE's VST3 host (scan, instantiate, editor, process)
  host/CMakeLists.txt             PluginHostTests + one ctest entry per plugin
tools/new_plugin/
  new_plugin.py                   Creates plugins/<Name>/ from the skeleton, and refuses a plugin code already in use
  skeleton/                       CMakeLists.txt, MANUAL.md, Source/Plugin{Processor,Editor}.{h,cpp}, tests/PluginTests.cpp, with __NAME__, __PRODUCT__, __CODE__ and __LOWERCASENAME__ placeholders. Carries R11 (manual stub), R12 (preset bar + StateHolder) and R13 (hamburger menu + SETTINGS), with tests for all three
.github/workflows/build.yml       CI: Windows (windows-latest) + macOS (macos-15, default Xcode). Build, ctest, upload VST3
.github/workflows/release.yml     Manual release (workflow_dispatch): build, test, package zips, gh release create
```

Build output: `build/plugins/<Name>/<Name>_artefacts/Release/{VST3,Standalone,AU}/`. Test executables: `build/modules/core_dsp/Release/CoreDSPTests.exe`, `build/tests/host/Release/PluginHostTests.exe`, and so on.

---

## 3. Build and test

Windows (PowerShell). CMake is not on the bash PATH, so use the full path:

```powershell
$CM = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $CM -S . -B build -A x64 -DSUITE_BUILD_PLUGINS=ON
& $CM --build build --config Release --parallel 8
& "$(Split-Path $CM)\ctest.exe" --test-dir build -C Release --output-on-failure
```

- Do not pass `-G`. CI relies on CMake picking the newest installed Visual Studio, and the local `build/` cache already has a generator.
- `-DSUITE_BUILD_PLUGINS=OFF` builds only CoreDSP and its tests. The SuiteUI tests are only registered when plugins are on.
- macOS CI runs `macos-15` with the runner's default Xcode, and Apple clang is built with `-ffp-contract=off` (see section 11 and [section 10](#10-open-items-and-known-risks)).
- JUCE is fetched by `FetchContent` into `build/_deps/juce-src`. That directory is a full local copy: grep it to check any API before using it.
- Build records for the pre-refactor baseline are in `build/baselines/phase0-pre-refactor/`, and the converted tree in `build/baselines/phase1-after-suite_add_plugin/`. They are generated and not committed (`build/` is ignored). To regenerate: `git archive 0d21400` into `build/baselines/src`, configure that tree with `-DFETCHCONTENT_SOURCE_DIR_JUCE=<repo>/build/_deps/juce-src` so JUCE is not fetched again, build Release, then save `Defs.txt`, `Info.txt`, `<Name>_resources.rc`, the VST3 file list and the source lists from each `.vcxproj`.
- Asset embedding: `juce_add_binary_data(MicroTimingChaosAssets ...)` builds a `BinaryData` target that the plugin and its test executable link. Add new icons or textures to its `SOURCES` list, then reference them by the generated name (`file_name_with_underscores`, for example `bpm_sync_active_svg`).

Last verified status (2026-11-21, plan 13): JUCE 9.0.5. Release build: 0 errors. Full CTest: 28/28 with the factory bank on; 20 default; `tools/audit_bank.py` passes with 200 presets.

- Default build (`SUITE_PRESET_BANK=OFF`): 20 entries - CoreDSPTests, SuiteState_Tests, SuiteModulation_Tests, SuiteUI_Tests, SuiteSettingsTests, the eight plugin test executables, and the seven VST3 host-load tests.
- Bank build (`SUITE_PRESET_BANK=ON`): **28** entries - the 20 public entries (the eight plugin test executables among them) plus **eight** factory-preset test executables (`SpectralDrift_PresetTests`, `DynamicSaturation_PresetTests`, `MicroTimingChaos_PresetTests`, `Sgraffito_PresetTests`, `MidiEffect_PresetTests`, `StereoTool_PresetTests`, `MultibandSaturation_PresetTests`, `Cyclical_PresetTests`). From the committed baseline (plan 6.1a is in; Cyclical is not) the increment is +4: `Cyclical_PluginTests`, `Cyclical_VST3_HostLoad`, `Cyclical_PresetTests` and `MultibandSaturation_PresetTests`. `suite_add_plugin` skips a plugin's preset-test target until `<Name>PresetTests.cpp` exists.
- Bank with host program list (`SUITE_PRESET_BANK_AS_PROGRAMS=ON`): the same entry count; the program-list virtuals are exercised inside the plugin preset tests via `runProgramList`.

All test executables pass:
- CoreDSPTests (47 functions)
- SuiteStateTests (9 functions)
- SuiteModulationTests (10 functions)
- SuiteSettingsTests (9 functions) - new in Plan 08 Phase 4
- SuiteUITests (15 functions)
- SpectralDriftTests (30 functions)
- DynamicSaturationTests (26 functions)
- MicroTimingChaosTests (47 functions)
- SgraffitoTests (55 functions)
- StereoToolTests (24 functions) - new 2026-11 (plan 06 idea 2.9), extended by plan 6.1, plan 11, plan 12, plan 13 and plan 15 (the modulation routing, the Side Crossover inert case, the new layout asserts and the mode affinity)
- MidiEffectTests (26 functions) - new 2026-11 (plan 06 idea 2.11), extended by plan 6.1, plan 11, plan 12, plan 13 and plan 15 (the UI-note ring, one test per ending path, the slow step rates and the mode affinity)
- MultibandSaturationTests (14 functions) - new 2026-11 (plan 06 idea 2.4), extended by plan 11, plan 12 and plan 13 (the routing, the smoothed crossover and the page switch)
- CyclicalTests (39 functions) - new 2026-11-19 (plan 6.1), extended by plan 12, plan 14 and plan 16 (Toggle trigger, per-slot notes, tempo/sync, the loop-sync rate, chop boundaries, the chop key mapping, sync-ignoring chop, granular inside a slice, the waveform chop lines, the full affinity matrix, the auto-chop analyser, the slice arrangement, and the loop transformation tools)
- [PluginName]_PresetTests for each plugin (factory bank)

Before 2026-11-08 the list was 9 entries: CoreDSPTests, SuiteUI_Tests, SuiteModulation_Tests, the three plugin test executables, and the three VST3 host-load tests. SuiteUI_Tests also passed against JUCE 8.0.6, the previous pin, which is the version CI ran (last checked 2026-11-06, not re-run since). macOS is not verified on JUCE 9.

---

## 4. Parameters

Parameter IDs are `static constexpr const char*` members of each processor. Defaults and ranges below are what `createParameterLayout()` defines. Each float uses a `NormalisableRange`, and some use a log or skew. The plugin tests check these values with `checkRange`, so a change here must change the test too.

### 4.1 Spectral Drift Shifter (60 parameters)

The 12 controls below are the original sound parameters. The next 34 are the two LFOs and six modulation
routings added by `SuiteModulation::Router` (`lfo1Rate` … `mod6Delay`), and the last two are the sidechain
ducking (plan 06 idea 2.5). All three groups are appended in that order, so the original order never
moves. At defaults every routing amount is 0 and the sidechain amount is 0, so the sound is bit-identical
to the pre-modulation version.
The drift LFO always runs its sine shape: the old `lfoShape` parameter was removed in plan 05.1 pass 2
(owner decision: no hidden parameters), so old sessions and presets that set it simply ignore that value.

| ID | Name | Range | Default | Notes |
|---|---|---|---|---|
| `depth` | Drift Depth | 0 – 100 % | 19.1 % | Scales the routed LFO's drift to radians. 19.1 % = 0.6 rad |
| `spread` | Bin Spread | 0 – 1 rad | 0.05 | Lag, in frames, between the drift of neighbouring bins |
| `stereoPhase` | Stereo Phase | 0 – 180° | 90° | Extra drift lag, in frames, for the right channel |
| `shift` | Spectral Shift | −32 – +32 bins | 0 | Magnitudes read from bin k − shift, linear interpolation |
| `tilt` | Spectral Tilt | −2 – +2 | 0 | Gain (k / 32)^tilt, clamped to [1/16, 8] |
| `gate` | Spectral Gate | −90 – 0 dB | −90 | At −90 the gate is open for every bin |
| `feedback` | Feedback | 0 – 12 % | 0 % | Processing also caps the value at `maxFeedbackPercent` = 12 %, which guards sessions saved with the old 0–20 % range. Engine accepts up to 0.95. Fed back through tanh |
| `feedbackDamping` | Feedback Damping | 0 – 100 % | 30 % | One-pole low-pass in the feedback loop. 0 % passes the loop unchanged |
| `driftStart` | Drift Start | 0 – 100 % | 0 % | Bins below this fraction of the spectrum are not drifted |
| `outputGain` | Output | −12 – +12 dB | 0 dB | Applied to the whole output after the engines; named "Output" suite-wide (plan 12) |
| `mix` | Mix | 0 – 100 % | 100 % | Dry is delayed by the frame latency |
| `freeze` | Freeze | bool | off | Holds captured magnitudes, phases still advance |
| `sidechainAmount` | Sidechain Amount | 0 – 100 % | 0 % | How hard the sidechain bus ducks the target. Appended after the routing (plan 06 idea 2.5) |
| `sidechainTarget` | Sidechain Target | Drift, Gate | Drift | What the sidechain ducks. Appended after the routing |

With plan 05.1 pass 2 the drift is driven by the routed LFOs: a new **Drift** routing target
(append-only, index 12 in the target list) receives the routed LFO's value (−1 … +1 around no
drift), which the depth, spread and stereo-phase parameters scale and spread across the bins.
With no routing assigned there is no drift. The old `driftRateHz` internal oscillator and the
`lfoShape` parameter are gone; the engine's per-bin drift reads a per-frame history of the
routed value, bin k lagging k × `binSpread` frames behind.

Sidechain ducking (plan 06 idea 2.5): a second input bus named *Sidechain*, declared disabled
by default (mono or stereo when a host enables it). A `CoreDSP::Saturation::EnvelopeFollower`
peak detector (5 ms attack, 150 ms release) over the sidechain channels ducks the target:
**Drift** multiplies `driftDepth` by (1 − duck), **Gate** sweeps `gateDb` toward 0 dB, thinning
the wet spectrum from the bottom. The duck is a per-block setting read by both channels; the
amount defaults to 0, so the defaults are bit-identical to the committed sound. When the
sidechain bus is enabled the block buffer also carries the sidechain channels behind the main
ones: the per-channel loop and the channel-clear loop are bounded by the main output bus, not
the buffer's channel count.

Latency reported: 2048 samples (`setLatencySamples`). Tail length: 0.

### 4.2 Dynamic Saturation Companion (59 parameters)

The 13 controls below are the original sound parameters. The remaining 34 are the
two LFOs and six modulation
routings added by `SuiteModulation::Router` (`lfo1Rate` … `mod6Delay`), appended last so the original order never
moves. At defaults every routing amount is 0, so the sound is bit-identical to the 13-parameter version.

| ID | Name | Range | Default | Notes |
|---|---|---|---|---|
| `drive` | Drive | 0 – 24 dB | 6 dB | Pre-gain base, smoothed 50 ms |
| `sensitivity` | Envelope Sensitivity | 0 – 1 | 0.5 | Dynamic Drive = Drive × (1 + Sens × Env) |
| `mode` | Saturation Mode | choice: Tube, Tape, Diode | Tube | |
| `mix` | Mix | 0 – 100 % | 100 % | Smoothed 50 ms |
| `outputLevel` | Output | −12 – +6 dB | 0 dB | Smoothed 50 ms; named "Output" suite-wide (plan 12) |
| `attack` | Attack | 1 – 100 ms, skew 0.35 | 5 ms | Envelope rise |
| `release` | Release | 50 – 2000 ms, skew 0.4 | 150 ms | Envelope fall |
| `detectHpf` | Detect HPF | 20 – 800 Hz, log | 20 Hz | High-pass on the detector only |
| `stereoLink` | Stereo Link | 0 – 100 % | 100 % | 100 % = linked envelope (max of channels) |
| `bias` | Tube Bias | 0 – 0.25 | 0.2 | Asymmetry of the tube curve. Tube only |
| `fold` | Fold | 0 – 50 % | 0 % | Wavefolder after shaping, 2× domain |
| `tone` | Tone | −1 – +1 | 0 | Tilt around 1 kHz after the shaper |
| `curve` | Envelope Curve | 0.5 – 3.0 | 1.0 | Envelope exponent |

Latency reported: the oversampler's integer latency, computed in `prepareToPlay`. The dry path is delayed by the same amount. Tail length: 0.

### 4.3 Micro-Timing Chaos Sequencer (66 parameters)

| ID | Name | Range | Default | Notes |
|---|---|---|---|---|
| `rate` | Rate | 1/4, 1/8, 1/16, 1/32 | 1/16 | Step length |
| `length` | Pattern Length | 2 – 16 | 8 | |
| `density` | Density | 0 – 100 % | 70 % | Share of steps that fire, curved by `log1p` |
| `seed` | Seed | 0 – 999 | 42 | Fixes the pattern and the hashed melody |
| `gate` | Gate | 10 – 100 % | 60 % | Note length as a share of the step. Lorenz Y and the pitch drift move it |
| `swing` | Swing | −20 – +20 % | 0 % | Odd steps. Negative moves them early |
| `baseVelocity` | Velocity | 30 – 127 | 100 | Before chaos |
| `root` | Root Note | 24 – 72 | 48 | MIDI note |
| `scale` | Scale | Chromatic, Major, Minor, Minor Pentatonic, Dorian, Whole Tone | Minor | |
| `octaves` | Octaves | 1 – 3 | 2 | |
| `chaos` | Chaos | 0 – 100 % | 50 % | The input to the lerp curve. At 0 nothing moves |
| `timingRange` | Timing Range | 0 – 50 ms | 20 ms | Largest timing shift at full drift. Does not change latency |
| `velocityChaos` | Velocity Chaos | 0 – 100 % | 40 % | Scaled by the Z drift |
| `lerp` | Lerp | 0 – 100 % | 50 % | Global drift amount. 0 turns all drift off |
| `lerpXMult` | Lerp X Mult | 0 – 2 | 1 | Timing drift multiplier |
| `lerpYMult` | Lerp Y Mult | 0 – 2 | 1 | Pitch and gate drift multiplier |
| `lerpZMult` | Lerp Z Mult | 0 – 2 | 1 | Velocity drift multiplier |
| `bpm` | Tempo | 40 – 240 BPM | 120 | Used when Host Sync is off, and disabled while it is on |
| `sync` | Host Sync | off / on | off | |
| `speed` | Speed | 0.5 – 2.0 | 1.0 | Attractor speed relative to the tempo |
| `lfo1Rate` / `lfo2Rate` | LFO Rate | 0.02 – 20 Hz, log | 0.25 Hz | Used when that LFO's Sync is off. Disabled in the UI while synced |
| `lfo1Shape` / `lfo2Shape` | LFO Shape | Sine, Triangle, Saw, Square, Sample & Hold, Random, Noise, Bouncing Ball | Sine | Index 0 to 7. Index 7 was the pendulum swing until plan 17 replaced it with the bouncing ball (no release yet, so no migration); it drops from full height, each of four bounces lower, and re-drops every cycle |
| `lfo1Sync` / `lfo2Sync` | LFO Sync | off / on | off | Locks the LFO to musical time |
| `lfo1Division` / `lfo2Division` | LFO Division | 4 bars, 2 bars, 1 bar, 1/2, 1/4, 1/8, 1/16 | 1/4 | Cycle length: 16, 8, 4, 2, 1, 0.5, 0.25 beats. Used while synced |
| `lfo1Offset` / `lfo2Offset` | LFO Offset | −100 – +100 % | 0 % | Added to the waveform before routing and the scope curve, then limited to −1 … +1. 0 % leaves the LFO unchanged |
| `mod1Source` … `mod6Source` | Mod Source | LFO 1, LFO 2 | LFO 1 | Which LFO drives the slot |
| `mod1Target` … `mod6Target` | Mod Target | the 14 targets in section 4.4 | None | Index 0 to 14. Append-only, see 4.4 |
| `mod1Amount` … `mod6Amount` | Mod Amount | −100 – +100 % | 0 % | Signed. 0 % leaves the slot idle |
| `mod1Delay` … `mod6Delay` | Mod Delay | 0 – 10 s, skew 0.5 | 0 s | Per routing. The slot is silent until this time has passed on the delay clock. The clock restarts when playback starts (the transition from stopped to running) and runs from `prepareToPlay` when there is no host transport |

The lerp curve is **not** a parameter. It is five points (x, y) stored as the `lerpCurve` property of the APVTS state (`"x,y;x,y;..."`, four decimals). `loadLerpCurveFromState()` reads it on construction and after `setStateInformation`. The first and last x are fixed at 0 and 1. The default is `y = log10(1 + 9x)`, sampled at x = 0, 0.25, 0.5, 0.75, 1.

Latency reported: 50 ms fixed (`maxTimingRangeMs`, 2400 samples at 48 kHz), regardless of Timing Range. Tail length: 0.

---

### 4.4 LFOs and modulation routing

Two LFOs and six routing slots. A slot adds `amount × LFO value` to its target, in the target's normalised range (0 to 1 across the parameter's range, so log and bipolar ranges behave the same). The result is clamped to [0, 1], and converted back with the parameter's own `convertFrom0to1`. With no slot aimed at a target, the base value is returned exactly, which is what keeps the committed output unchanged at the defaults.

The LFO and routing parameters are created by `SuiteModulation::Router::addParameters`, which every plugin that uses modulation calls. The target table is per plugin: the plugin passes its own `TargetInfo` list to the router, and the choice index is the position in that list plus one (0 is None). The table order is saved in sessions, so it is append-only per plugin.

Targets (`ModTarget` in `PluginProcessor.h`; index order is saved in sessions, so append only): 0 None, 1 Chaos, 2 Timing Range, 3 Velocity Chaos, 4 Lerp, 5 Lerp X Mult, 6 Lerp Y Mult, 7 Lerp Z Mult, 8 Density, 9 Gate, 10 Swing, 11 Velocity (rounded), 12 Speed, 13 Tempo (applies only while Host Sync is off), 14 Seed (changes the pattern and melody). Excluded on purpose: Rate, Length, Scale, Root, Octaves and Host Sync.

LFO phase is counted in cycles. Free LFOs accumulate `rate × samples / sampleRate` at the end of each block, so a rate change never jumps the phase, and they keep running when the transport stops. Synced LFOs use `beat / divisionBeats`, where the beat is the sequencer's beat (host or internal), so they hold still when the transport stops. Sample and hold takes its seed from `lfoSeeds` and its cycle index from the unwrapped cycle count.

LFOs are sampled at step times, not per sample, so a fast LFO can alias against the step grid (section 10).

### Modulation targets in the effects

Spectral Drift targets (12, in `ModTarget` order): 0 None, 1 Drift Depth, 2 Bin Spread, 3 Stereo Phase, 4 Spectral Shift, 5 Spectral Tilt, 6 Spectral Gate, 7 Feedback, 8 Feedback Damping, 9 Drift Start, 10 Output Gain, 11 Mix, 12 Drift. Excluded: `freeze` (a switch). State version 2 removed the old Drift Rate target (a migration remaps saved routing indices).

Mix, feedback, feedback damping and output gain are smoothed per-sample before reaching the engine, so modulating them per block never zips or clicks. Everything else is consumed once per frame (2048 samples) or per block, so block-rate modulation is safe.

Dynamic Saturation targets (12, in `ModTarget` order): 0 None, 1 Drive, 2 Sensitivity, 3 Mix, 4 Output, 5 Attack, 6 Release, 7 Detect HPF, 8 Stereo Link, 9 Tube Bias, 10 Fold, 11 Tone, 12 Curve. Excluded: `mode` (a choice).

DS already smooths drive, mix and output level for 50 ms, and its other targets are block-rate by construction, so no DSP change was needed.

Synced LFOs in both effects use a tempo-locked beat clock (`beatClock += numSamples × tempo / 60 / sampleRate`), not the host's PPQ. The tempo is the host's when the host reports one, clamped to 20–300 BPM (`SuiteModulation::hostTempoOrDefault`). With no playhead (Standalone) it is 120 BPM. The clock only accumulates, so it does not jump on loop or relocate, and it keeps moving while the host still reports a tempo with the transport stopped. The routing Delay clock runs from `prepareToPlay` and is not restarted on transport start.

### 4.5 MultiSat (57 parameters)

A two-band saturator (plan 06 idea 2.4, first pass). Host name "MultiSat" since plan 12 (display
name only: the folder, target, bundle ID and preset ID keep the MultibandSaturation name). The
11 controls below are the sound parameters; the 34 after them are the shared Router's (two LFOs
and six routing slots, `lfo1Rate` … `mod6Delay`, appended last so no saved parameter index
moves), added in plan 13. At defaults every routing amount is 0, so the sound is bit-identical
to the 11-parameter version. Since plan 13 the editor has a SOUND / MOD page switch; the
modulation panel lives on the MOD page.

| ID | Name | Range | Default | Notes |
|---|---|---|---|---|
| `mode` | Saturation Mode | choice: Tube, Tape, Diode | Tube | Shared by both bands |
| `crossover` | Crossover | 80 – 8000 Hz, log (1 kHz centre) | 500 Hz | The LR4 split point; **smoothed in Hz (20 ms) before the coefficients are computed**, so routing it never clicks |
| `driveLow` | Drive Low | 0 – 24 dB | 6 dB | Low-band pre-gain, smoothed 50 ms |
| `driveHigh` | Drive High | 0 – 24 dB | 6 dB | High-band pre-gain, smoothed 50 ms |
| `sensitivity` | Envelope Sensitivity | 0 – 1 | 0.5 | Dynamic Drive = Drive × (1 + Sens × Env), per band |
| `attack` | Attack | 1 – 100 ms, skew 0.35 | 5 ms | Both bands' envelope rise |
| `release` | Release | 50 – 2000 ms, skew 0.4 | 150 ms | Both bands' envelope fall |
| `toneLow` | Tone Low | −1 – +1 | 0 | Tilt around 1 kHz on the shaped low band (smoothed 50 ms) |
| `toneHigh` | Tone High | −1 – +1 | 0 | Tilt around 1 kHz on the shaped high band (smoothed 50 ms) |
| `mix` | Mix | 0 – 100 % | 100 % | Smoothed 50 ms |
| `outputLevel` | Output | −12 – +6 dB | 0 dB | Smoothed 50 ms; named "Output" suite-wide (plan 12) |

Modulation targets (10, in `ModTarget` order, append-only): 0 None, 1 Crossover, 2 Drive Low,
3 Drive High, 4 Sensitivity, 5 Attack, 6 Release, 7 Tone Low, 8 Tone High, 9 Mix, 10 Output.
Excluded: `mode` (a choice).

Latency reported: 0 (base-rate first pass; the plan's oversampling-and-crossover interaction risk
stays open). Tail length: 0. The band sum is clamped to ±1 before the mix and output level (the
Dynamic Saturation ceiling rule). First pass notes: the envelopes are per channel and per band
(no stereo link), the tube bias is the shared default (0.2), there is no fold, and the tone
control's pivot is the fixed 1 kHz of `CoreDSP::Saturation::ToneControl`. The visual
(plan 13; plan 15 removed the meters on the owner's request): band bars use the full container
height against a −60 … 0 dB scale, drawn over a **frequency × level grid** (vertical dim lines
at 80/100/200/500/1k/2k/5k/8k, horizontal ones at 0/−6/−12/−24/−60 dB), with dB ticks on the
left edge aligned with the level lines, the crossover marker on the log
axis and the Hz scale below. The SOUND / MOD switch sits **in the header row, left of the
preset bar** (plan 15, the StereoTool R8 placement).

### 4.6 MIDI Chord/Arp/Scale Tool (14 parameters)

The first six controls are the original chord parameters (plan 06 idea 2.11). The eight after them
are the arpeggiator added by plan 6.1, appended in that order so no saved parameter index moves.
`Scattered` was appended to `arpId` (index 4), so saved indices 0–3 keep their meaning.

| ID | Name | Range | Default | Notes |
|---|---|---|---|---|
| `root` | Root Note | 24 – 84 (int), shown `C4 (60)` | 60 | Key centre for the quantisation |
| `scale` | Scale | Chromatic, Major, Minor, Minor Pentatonic, Dorian, Whole Tone | Minor | Index 0 to 5 |
| `chord` | Chord Quality | Major, Minor, Dim, Aug, Sus2, Sus4, Minor 7, Major 7 | Major | Index 0 to 7 |
| `arp` | Arp Pattern | Up, Down, Alternate, Random, Scattered | Up | Index 0 to 4; Scattered is a second deterministic shuffle |
| `octave` | Octave | int −2 – +2 | 0 | Transposes the chord |
| `velocity` | Velocity | 0 – 150 % | 100 % | Scales the note-on velocity |
| `arpMode` | Mode | Chord, Arp | Chord | Chord is the pre-plan-6.1 path, event for event |
| `stepRate` | Step Rate | 1/4, 1/4T, 1/4D, 1/8, 1/8T, 1/8D, 1/16, 1/16T, 1/16D, 1/32, 1/32T, 1/32D, 1/2, 1 Bar, 2 Bars, 4 Bars | 1/16 | `CoreDSP::Chords::arpStepBeats`: 1, 2/3, 1.5, 0.5, 1/3, 0.75, 0.25, 1/6, 3/8, 1/8, 1/12, 3/16, 2, 4, 8, 16 beats. Twelve values since plan 12 (state version 2; a v1 index is remapped); plan 15 appended the four slow plain rates (append-only, no migration) |
| `hostSync` | Host Sync | Off, On | Off | On follows the host's PPQ position and tempo |
| `tempo` | Tempo | 40 – 240 BPM | 120 | Used while Host Sync is off; the host tempo is authoritative when synced |
| `gateLength` | Gate Length | 10 – 100 % | 60 % | Note length as a share of the step |
| `velocityCurve` | Velocity Curve | Flat, Linear, Exponential, Random | Flat | Per-step shape in Arp mode; Flat keeps velocity uniform |
| `velocityDirection` | Velocity Direction | Ascending, Descending | Descending | Applies to Linear/Exponential |
| `octaveSpread` | Octave Spread | −1 – +1 | 0 | Each pass through the chord tones of the 8-step pattern lifts by `round(spread × cycle)` octaves |

Latency reported: fixed 50 ms (`latencyMs`, 2400 samples at 48 kHz). Tail length: 0. The editor's
visual (plan 13) carries the suite's `PlayableKeyboardStrip`: clicking a key pushes the note
through an SPSC UI-note ring (`pushUiNote`), drained at the top of `processBlock` so UI notes run
the same chord/arp logic as host notes, and clicking the 8-step strip replays the current chord.
The note-off guarantee (plan 13): every UI note the processor starts from the ring is tracked in a
held set, and it is released on every ending path — mouse-up, a drag off the key, the pointer
leaving, focus lost (the editor flushes), the editor being destroyed (the strip's destructor
flush), a stopped transport (a playhead running→stopped transition flushes the held set), and
`reset()` / `releaseResources()` (which clear the bookkeeping). One test per path in
`testUiNoteEndingPaths`. Plan 15: in Chord mode the arp-only controls (Step Rate, Host Sync,
Tempo, Gate Length, Velocity Curve, Velocity Direction, Octave Spread) are dimmed, live again
in Arp mode — the chord parameters and the Pattern ordering stay live in both.

### 4.7 Stereo Tool Effect (69 parameters)

Plan 17 Phase 8 appended eight interest-feature parameters (Mono Maker Crossover 0–800 Hz,
**0 = Off**, Multiband Width Low/Mid/High ±100 %, MS Tone Mid/Side ±1, Image Rotation Rate
0.02–2 Hz, Depth 0–100 %) and plan 18 appended the Phase knob (0–360°). The DSP behind all
nine is implemented (plan 18 Phase 3): the mono-maker is a true side high-pass, the
multiband width splits the side at fixed 200 Hz/2 kHz, the M/S tone is the shared
ToneControl per channel, and the rotation is an orthonormal L/R plane rotation with an
accumulated phase. The routing table grew to 16 targets (+Pan, +the eight interest
parameters). The identity early-return requires all nine at their defaults.

The first three controls are the original stereo parameters (plan 06 idea 2.9). The eight after
them are the character processor added by plan 6.1, appended in that order so no saved parameter
index moves. Every new default is neutral, and the identity early-return now also requires
`character == Tube && transientAmount == 0 && midFocus == 0 && sideFilter == Off` (and, since
plan 12, `inputLevel == 0 && outputLevel == 0`), so the defaults stay bit-exact. The identity
test reads the **modulated** values, so a routing with a non-zero amount disables the shortcut.
The 34 after the level stage are the shared Router's (plan 13, appended last so no saved
parameter index moves); at defaults every routing amount is 0, so the sound is bit-identical.

| ID | Name | Range | Default | Notes |
|---|---|---|---|---|
| `width` | Width | −100 – +100 % | 0 % | Side gain 1 + width, clamped so the decode never folds |
| `drive` | Drive | −24 – +24 dB | 0 dB | 0 transparent; below 0 clean gain reduction; above 0 shaper drive; **smoothed in dB (20 ms) inside the sample loop** so routing never steps |
| `mix` | Mix | 0 – 100 % | 100 % | **Smoothed 20 ms** (plan 13), so routing never steps |
| `character` | Character | Tube, Tape, Diode, Console, Clip | Tube | Tube is `CoreDSP::Stereo::saturate()` exactly; the others are `saturateCharacter()` |
| `transientAttack` | Transient Attack | 1 – 200 ms, skew 0.35 | 5 ms | One follower, inert while the amount is 0 |
| `transientRelease` | Transient Release | 20 – 2000 ms, skew 0.4 | 150 ms | Same follower |
| `transientAmount` | Transient Amount | 0 – 100 % | 0 % | 0 bypasses the follower entirely |
| `transientTarget` | Transient Target | Width, Drive, Both | Both | What the follower moves |
| `midFocus` | Mid Focus | −100 – +100 % | 0 % | `midGain = 1 + f`, `sideGain = 1 − f` |
| `sideFilter` | Side Filter | Off, High Pass, Low Pass, Band Pass | Off | A `juce::dsp::StateVariableTPTFilter` on the side only; Off runs no filter node |
| `sideCrossover` | Side Crossover | 20 – 8000 Hz, log | 200 Hz | Only active while the side filter is not Off |
| `inputLevel` | Input | −24 – +24 dB | 0 dB | Plan 12: gain into the chain, smoothed 20 ms |
| `outputLevel` | Output | −24 – +24 dB | 0 dB | Plan 12: the final gain stage, after the mix, smoothed 20 ms |
| `pan` | Pan | −100 – +100 % | 0 % | Plan 17 Phase 4: a balance law with exact unity at centre (turning right attenuates the left by `cos(|pan|·45°)`, the kept channel stays at unity), smoothed 20 ms; the identity early-return requires Pan = 0, so the defaults stay bit-transparent. The 11th modulation target |

Modulation targets (11, in `ModTarget` order, append-only): 0 None, 1 Width, 2 Drive, 3 Mix,
4 Transient Attack, 5 Transient Release, 6 Transient Amount, 7 Mid Focus, 8 Side Crossover,
9 Input, 10 Output, 11 Pan. Excluded: `character` and `transientTarget` (choices). **Side Crossover
only has an effect while the Side Filter is not Off** — the test `testSideCrossoverInertWhenFilterOff`
asserts the routing is bit-identical with the filter Off.

Latency reported: 0. Tail length: 0. A transient at amount 1 and a full envelope boosts the drive
by at most 12 dB (`maxTransientDriveBoostDb`). The editor (plan 13; plan 15 on the owner's
request) is 720×658, and the default height **is** the minimum resize height: row 1 is the
full-width stereo visual — the mid/side bars (twice their old width) and the FOLLOWER trace,
**no input/output meters** — row 2 is LEVEL (two knobs) plus IMAGE, row 3 CHARACTER across the
full width. Character and Side Filter are dropdowns (`LabelledDropdown`), Target stays a pill.
A SOUND / MOD page switch (R8: top right of the header, left of the preset bar) shows the
modulation panel on the MOD page. Plan 15 affinity: Side Crossover is dimmed while the Side
Filter is Off, and the transient Attack/Release/Target are dimmed while Amount is 0.

### 4.8 Cyclical (151 parameters)

The four-slot loop instrument (plan 6.1, overhauled by plan 14, reworked by plan 18 Phase 4).
**Plan 18 removed the global Pan/Quantize/Swing/TriggerMode/Pitch/CrossfadeLength/Tempo/Sync
parameters and re-created them per slot** (`slotNQuantize` … `slotNPan`, × 4, appended so no
saved index moves — safe before a release): each slot runs its own beat clock, grid, swing,
trigger mode, pitch, crossfade, tempo, host sync and pan, and the Phase-9
filter/attack/release/level parameters are wired through a per-slot render stage (scratch →
filter → level → pan → master). The layout order is: `masterVolume` (the one global), 12
playback parameters per slot, filter + cutoff/reso + attack/release/level per slot, the 8
former globals per slot, then the shared Router (46). The per-slot A/R drive the voice's
envelope via `CoreDSP::Loop::Voice::setEnvelopeTimes` (per-slot Release default **5 ms**).
The region knobs (Start/Length/Slices/Slice Length/Beats) apply live: a knob release that
moved the value retriggers the playing voice (`requestRegionRetrigger`, the plan-16
retransform pattern; a routed Speed is heard from the trigger via `blockOffsets`). The
modulation targets are per-slot: Level 1–4, Cutoff 1–4, Speed 1–4, Pan 1–4 (16). The loop
references are **not** parameters: they ride as `loop1Path` … `loop4Path` attributes on
`<PARAMETERS>` (the samplePath pattern). The per-slot IDs are `const char* x[4]` arrays with
a literal size, which is the form `tools/audit_bank.py`'s coverage check parses.

| ID | Name | Range | Default | Notes |
|---|---|---|---|---|
| `masterVolume` | Output | −24 – +12 dB | 0 dB | Smoothed 20 ms; named "Output" suite-wide (plan 12); the ID keeps the masterVolumeId name — the one global, shown on every slot page |
| `slotNQuantize` | Quantize | Off, 1/16, 1/8, 1/4, 1/2, 1 Bar | Off | This slot's grid; the host time signature is not read |
| `slotNSwing` | Swing | −20 – +20 % | 0 % | This slot's odd grid points, `Sequencer::swingBeats` |
| `slotNTrigger` | Trigger | Latch, Gate, Toggle | Latch | Plan 14 appended Toggle (index 2): the first press starts the slot looping, the second stops it immediately; a note-off never stops it; a transport stop stops this slot when its mode is Toggle |
| `slotNStart` | Start | 0 – 100 % | 0 % | Capped at 95 %; effective length = min(Length, 100 − Start), via the shared `CoreDSP::Loop::regionFrames` |
| `slotNLength` | Length | 5 – 100 % | 100 % | Share of the loaded loop |
| `slotNSpeed` | Speed | 0.25 – 4.0 ×, skew 0.5 | 1.0 | × the sample-rate ratio; inert in granular mode, and inactive while Host Sync is on — unless the slot is chopped (a chopped slot ignores sync). The editor dims it in those states |
| `slotNDirection` | Direction | Forward, Reverse, Ping-Pong | Forward | |
| `slotNPingRandom` | Ping Rand | bool | off | Ping-pong turn points redrawn per trigger; rate unchanged |
| `slotNGranular` | Grain | bool | off | 80 ms grains, deterministic; allowed inside a chopped slice |
| `slotNVelocity` | Velocity | 30 – 150 % | 100 % | Scales the note velocity; applies from the next trigger |
| `slotNNote` | Note | int 24 – 96 | 60/61/62/63 | The slot's trigger key (plan 14). A chopped slot claims Note .. Note + Slices − 1, slice k on Note + k. Overlaps between slots are accepted; the lowest slot wins |
| `slotNBeats` | Beats | int 1 – 16 | 4 | The musical length of the whole loaded loop (plan 14). With Host Sync on the loop plays in exactly Beats beats: `region.speed = loopFrames × bpm / (beats × 60 × sourceRate)`. Start/Length choose the span; they do not change the rate. Inert (dimmed) for a chopped slot |
| `slotNChop` | Chop | bool | off | Chop mode (plan 14): the region splits into Slices, one key per slice. Sync is disabled for a chopped slot |
| `slotNSlices` | Slices | int 2 – 16 | 8 | Slice count (plan 14); dimmed while chop is off |
| `slotNSliceLength` | Slice Length | −100 – +100 % | 0 % | Skews the boundaries: `b_i = (i/S)^e` with `e = 2^(2·v)`, `v = Slice Length/100`. Positive lengthens the late slices, negative the early. Shared with the waveform view (`CyclicalProcessor::chopBoundary`) |

The plan-16 chop workflow adds three non-parameter state properties per slot (the lerpCurve pattern): `chopNBounds` (the auto-chop analyser's interior boundary shares — used when the count matches Slices, the `slotNSliceLength` power curve otherwise; cleared when Slice Length moves), `chopNOrder` (the slice arrangement — key k plays the source slice the order names; identity when absent or the count mismatches), and the in-memory tool selection (`selectedSlices`, not saved). The waveform header carries LOAD, AUTO (the transient analyser), REV, NORM and TRIM (reverse loop/selected slice, peak to −0.1 dBFS, region aligned to the non-silent content) — every tool installs a fresh buffer through the retirement queue and leaves the loop reference unchanged, so transforms are session-local. Plan 17 Phase 3: a trigger-mode change or a preset/state load silences every voice and clears the toggle state (a running loop no longer outlives its own settings), and REV/NORM restart the playing voice on the transformed buffer with the articulation it started with (Latch/Toggle; Gate waits for the next press).
| `pitchShift` | Pitch | int −12 – +12 | 0 | Simple detune (speed and pitch move together) |
| `crossfadeLength` | Crossfade | 0 – 500 ms | 5 ms | Global loop-seam blend |
| `tempo` | Tempo | 40 – 240 BPM | 120 | Runs the beat clock while Host Sync is off (plan 14) |
| `sync` | Host Sync | off / on | off | On: the clock follows the host's position and tempo; synced loops play in Beats beats. Chopped slots ignore it |

Latency reported: 0. Tail length: 2.0 s. The window is 1000×1120 (plan 20 Phase 2 grouped the
slot pages and raised the waveform to 170 px; the mix row sits inside the card frame; the
height is derived from the layout and the owner approves the measured size from the render);
the accent is tangerine `#FF7A45` and the regenerated tangerine lockup is committed.

## 5. Plugin behaviour

### 5.1 Spectral Drift engine

Per channel, `DriftEngine` runs this pipeline:

```
input → ring buffer → Hann analysis → FFT 2048 → [not frozen: store magnitudes and phases]
      → shapeMagnitudes() (shift, tilt, gate) → per-bin phase drift from the routed LFO's value
      → Hermitian IFFT → Hann synthesis → overlap-add → wet
      → feedback into input ring; dry/wet mix with the aligned dry signal
```

Frame parameters: `frameOrder = 11` (2048), `hopSize = frameSize / 4` (75 % overlap), `numBins = 1025`.

Invariants (the indexing is subtle, so do not change it casually):
- Every sample is stored at slot `n mod frameSize`. `ringPos` is `n mod frameSize`, and the slot at `ringPos` holds the sample that is `frameSize` samples old.
- At call `n`, the output emitted is the output sample for index `n − frameSize`. It is final, because the frame covering it was processed at the end of call `n − 1`.
- The overlap gain is `invOverlapGain[hopIndex]`, measured numerically from the window (`hopIndex = n mod hopSize`). It is not assumed to be constant.
- The frame at time `t` (when `hopIndex` wraps) reads `inputRing` starting at `ringPos`, which is the oldest sample `t − frameSize`.
- The output spectrum shown in the UI is measured on `wetRing`, which holds finished samples one frame behind. It is the real overlap-add result, not the synthesised frame.
- `inverse FFT` in JUCE is already divided by N. Do not normalise again.
- `frameCount` increments once per frame. The UI uses it, together with the input energy, to decide whether to show "Waiting for audio". The message itself is drawn by the shared `SuiteUI::SpectrumDisplay`.
- The engine feedback is capped at `maxFeedbackPercent` (12 %) in `readSettings`, whatever the parameter range.

Reconstruction test: with drift depth 0, no shift/tilt/gate, feedback 0 and mix 1, the output equals the input delayed by 2048 samples to within 1e-3. This is the main correctness check for the engine.

### 5.2 Dynamic Saturation processing

Three passes in `processBlock`:

1. **Base rate.** Detector high-pass per channel → linked peak → linked envelope and per-channel envelopes, mixed by Stereo Link → shaped by Curve → Dynamic Drive per sample into `preGainBuffer`. Mix and output level are smoothed per sample.
2. **2× oversampled.** `juce::dsp::Oversampling<float>` (half-band FIR, integer latency) upsamples; each oversampled sample is `fold(shape(x × preGain[n/2], mode, bias), fold)`; then downsample back into the buffer.
3. **Base rate.** `wet = clamp(±1, tone(...))`. This ceiling is deliberate: the oversampler and tone can overshoot by about 10 %, and the output test checks a hard ±2.0 after the output gain. Then `mixed = dryDelayed + mix × (wet − dryDelayed)` and `× outputGain`. `dryDelayed` is a ring of `latencySamples`.

Shapers: Tube is `tanh(x + bias)`, re-centred and normalised to [−1, 1]. Tape is a symmetric cubic soft clip, `1.5 · (y − y³/3)` on the clamped input, so the rails sit at ±1. Diode is an exponential curve with a soft knee for positive excursions and a tighter one for negative.

The editor's transfer-curve display (`SuiteUI::CurveView`) redraws with the current drive, bias and fold. Its input meter shows the input level.

### 5.3 Micro-Timing Chaos sequencer

The sequencer is driven by musical time, not by sample count. Each block:

1. **Transport.** With Host Sync on and a host playhead present, the beat position follows the host's PPQ position. A jump of more than 1e-3 beats, or a resync request, resets the step grid. With Host Sync off, the beat position advances by `numSamples / samplesPerBeat`, where `samplesPerBeat` comes from `currentBpm()`. The Tempo parameter is used unless the host reports a tempo while synced. A stopped transport releases every sounding note (`stopAllNotes`) and sets the playhead to −1.
2. **Steps.** While `nextStep × stepLength` lies inside the block, `scheduleStep()` runs for that step:
   - `advanceAttractorTo(stepBeat, speed)` advances the Lorenz attractor by `Δbeats × speed` time units, in chunks of at most `LorenzAttractor::maxAdvance`. The attractor therefore moves per step, so the same pattern gets the same chaos at any tempo. The trail is published once per step.
   - Every control is read through `modulatedValue()` with this step's offsets: Chaos, Timing Range, Speed, the Lerp amount and multipliers, Density, Gate, Swing, Velocity and Velocity Chaos. The lerp curve is then evaluated on the modulated Chaos, so a modulated Chaos moves along the curve. The drifts are published for the Preview tab, and the offsets are published for `getTargetOffset()`.
   - `stepFires(seed, patternIndex, density)` decides whether the step sounds. Silent steps stop here.
   - The pitch is `blendDegree(hashDegree, lorenzDegree, pitchDrift, count)`, mapped to the scale by `notePitch`.
   - Velocity is `velocityFor(baseVelocity, velocityChaos × velocityDrift, z)`.
   - Timing is `timingOffsetMs(timingDrift, rangeMs, x)`. Swing is added on odd steps, and gate is `gateBeats(stepLength, gate, pitchDrift, y)`.
   - `queueNoteOn` places the note-on at `max(streamSample, gridSample + swing + latency + offset)`. If an earlier copy of the same note is still pending, it is cut first, so a retrigger never hangs.
   - `queueNoteOff` is placed at note-on + gate, at least one sample later.
3. **Pass-through.** Incoming MIDI goes into the same queue at `streamSample + samplePosition + latency`. Audio is copied through untouched.
4. **Release.** Queued events due in this block are written to the MIDI buffer with `offset = max(0, time − streamSample)`, and the sounding-note set is kept up to date.

**Lerp curve (hybrid model).** Each axis has a drift amount:

```text
curve        = evaluateLerpCurve(chaos)                 (piecewise linear through the 5 points, 0 to 1)
timingDrift  = lerp × clamp(curve × lerpXMult, 0, 1)
pitchDrift   = lerp × clamp(curve × lerpYMult, 0, 1)
velocityDrift = lerp × clamp(curve × lerpZMult, 0, 1)
```

Chaos at 0 gives `curve = 0`, so nothing moves. Lerp at 0 turns all drift off. A zero multiplier switches its axis off. The three drifts are stored in atomics and read by `getDriftAmounts()`.

**Latency and reporting.** `latencySamples = maxTimingRangeMs × sampleRate / 1000`, reported with `setLatencySamples` in `prepareToPlay`. It is fixed, so a change to Timing Range never changes the reported latency.

Audio: the stereo buffer passes through unchanged (tested).

**Block start.** Before the steps, `processBlock` resyncs to the host position, computes the block-start offsets (for the free tempo), and publishes each LFO's phase and value for the UI. At the end of the block the free LFOs advance.

---

## 5.5 Presets (`SuitePresets`, optional factory bank)

The public seam is `modules/suite_presets/include/SuitePresets.h`: a `StateHolder` interface (`getPresetPluginId`, `getPresetStateVersion`, `getPresetStateTree`, `applyPresetState`) each processor implements, plus an `isAvailable()` helper and a `PresetBar` whose public (stub) form reports height 0 so the default build's layout is unchanged.

**R12 (codified 2026-11-18): every plugin ships this seam** — the bar in the editor header, `StateHolder` in the processor from the first commit, a factory bank of at least 15 presets and a `<Name>_PresetTests` entry. It used to be a convention followed by a subset of plugins, and `suite_add_plugin` skips the preset-test target when `<Name>PresetTests.cpp` is missing, so nothing complains. Plan 11 backfilled the last gap: every plugin now has the full seam (MidiEffect and StereoTool joined in plan 6.1a, MultibandSaturation in plan 11).

`applyPresetState()` runs through `SuiteState::read` with `resetMissing=true`: the preset tree (a `<PARAMETERS>` element) is encoded to binary, then `SuiteState::read` does the version check, migrations, `replaceState` and reset-to-default for absent IDs. Nothing changes if the state is `notXml`, `wrongRoot` or `newerThanPlugin`. So a preset carries only the parameters it changes — deltas that materialise into full state at load time. The one exception is Sgraffito's `samplePath` property (plan 07 phase 1): it rides along as a `<PARAMETERS>` attribute and `applySampleFromState` acts on it after the read; an empty tree (INIT and host program 0) resets the sample to the embedded default.

The factory bank (`modules/suite_presets/factory_bank/`, gated by `SUITE_PRESET_BANK`) adds:

- `PresetBank`: parses the embedded factory bank (`bank/AudioSuitePresets.xml`, one `AudioSuitePreset` per preset), provides `getFactoryPresets()` / `getNumFactoryPresets()` / `getFactoryPreset(i)`, plus `parseDelta()` which merges a JSON delta onto the default state.
- `PresetFile`: `.ausp` read/write (`<AudioSuitePreset formatVersion="1" plugin="..." pluginVersion="...">`, `<Meta>`, `<PARAMETERS suiteStateVersion="N">`), user presets stored under `userDocuments/AudioSuite/<plugin>/Presets`, sanitised file names, version check at read time.
- `PresetBar`: header-bar UI (factory presets, INIT, SAVE, menu), `selectPreset(i)`, `getNumFactoryPresets()`, `getCurrentPresetName()` / `isPresetDirty()`.
- `PresetMenuModel`: builds a `juce::PopupMenu` with Factory/User submenus grouped by category.
- `PresetPrograms` (behind the separate `SUITE_PRESET_BANK_AS_PROGRAMS` flag): host program list where program 0 is Init and programs 1..N are the factory presets in bank order. Bank order is append-only once hosts cache indices.

Factory presets are authored as JSON deltas in `tools/preset_data.py` (36 SD / 36 DS / 28 MTC / 20 Sgraffito / 15 MidiEffect / 25 StereoTool / 25 MultibandSaturation / 25 Cyclical = 210, plans 6.1, 11, 13 and 14) and generated into `modules/suite_presets/factory_bank/bank/` by `tools/build_bank.py`. `tools/audit_bank.py` enforces the rules: unique names per plugin, ≥3 categories, every value inside the parameter's `NormalisableRange`, no preset equal to the default state, SD `freeze` only in `Freeze`, DS `drive` 24+`fold` 50 only in `Extreme`, MTC host `sync` off and `bpm` 120 in every factory preset, the base pattern must fire at least one step, factory presets may carry a `samplePath` only as an `embedded:` URI, never a file path (plan 07 Q3, revised in phase 4; checked in the authoring data, the generated XML attributes and the JSON files), and — since plan 05.2 — the authoring schema must cover every parameter the plugins expose (live IDs parsed from the sources, the shared Router's parameters included). Plan 05.3 F1.3 resolved the coverage findings: parameters no factory preset moves are now a recorded decision (levels/gains are never preset-authored; the structural LFO/routing slots stay at defaults), and Sgraffito gained five new presets.

`SUITE_PRESET_BANK_AS_PROGRAMS=OFF` (default build): plugins report `getNumPrograms() == 1`, `getProgramName() == ""`, exactly as before the preset system. With both flags on, the program virtuals map to `applyPresetState` / `getStateInformation`, and `currentProgramIndex` tracks the last applied program (-1 after any preset/state change that is not a factory program).

The preset system adds no APVTS parameters; `numControls` and `checkParameterIds` are unchanged (48 / 47 / 54).

MTC's `lerpCurve` state property (not a parameter) round-trips through presets and factory bank XML via the `lerpCurve` attribute on `<PARAMETERS>`; `applyPresetState()` reloads it through `loadLerpCurveFromState()`.

**Preset selection persistence (plan 09 1.4, all four plugins).** The identity of the applied preset lives in the processor, not the editor-owned bar, so closing and reopening the plugin window keeps showing the preset's name (the bar used to reset to "Init" on every reopen). The seam is `StateHolder::getPresetSelection()` / `setPresetSelection()` over the `SuitePresets::PresetSelection` value type (`none` / `factory` with an append-only bank index / `userFile` with a file path — the scanned user list re-orders, so paths, not positions, are the identity). All four processors implement both virtuals: `setPresetSelection` writes the additive `presetSelection` property on `apvts.state` at change time (`"factory:<i>"` / `"user:<path>"`, the `lerpCurve`/`samplePath` pattern — no version bump, unreleased; a session without the property reads as Init), `setStateInformation` restores the member from the property, and `applyPresetState` resets it on `LoadResult::loaded` — the caller (bar or host-program bridge) re-states it right after. The bar records on every apply, restores in its constructor (an unresolvable user file falls back to Init without touching the audio state), and its 10 Hz timer follows selection changes made by another editor of the same processor. A restored bar's dirty baseline is the state the preset would produce (`hashOfAppliedPreset`: every parameter at the delta's value or its default, the resetMissing semantics), so a tweaked-then-reopened session still shows the `*`. Under `SUITE_PRESET_BANK_AS_PROGRAMS`, `setCurrentProgram` sets the selection (program N = factory preset N−1), keeping the host program list and the bar describing the same event.

---

## 6. Test architecture

Twenty-eight CTest entries (bank build) run twenty-two executables: `CoreDSPTests` (47 test functions), `SuiteStateTests` (9), `SuiteUI_Tests` (`SuiteUITests`, 13), `SuiteModulation_Tests` (`SuiteModulationTests`, 10), `SuiteSettingsTests` (`SuiteSettingsTests`, 9), `SpectralDriftTests` (30), `DynamicSaturationTests` (25), `MicroTimingChaosTests` (47), `SgraffitoTests` (55), `StereoToolTests` (19), `MidiEffectTests` (23), `MultibandSaturationTests` (10), `CyclicalTests` (24), and `PluginHostTests`, which has one ctest entry per plugin (7). With the factory bank on, eight preset-test executables join them (one per plugin: SpectralDrift, DynamicSaturation, MicroTimingChaos, Sgraffito, MidiEffect, StereoTool, MultibandSaturation and Cyclical). The README has the full list.

**Module tests** (`modules/suite_modulation/tests/SuiteModulationTests.cpp`): the router, with a stub processor (no plugin). They cover the routed LFO, the zero case (bit for bit), bipolar amounts, whole-number targets, Host Sync ignoring Tempo, the one-bar period, sample and hold, out-of-range indices, and the control display (values set without notifying), plus the modulation panel's preferred height (every child stays inside it; plan 10 phase 9). Not covered: the control display skipping a slider the mouse holds, since the test cannot press a mouse button. The panel and scope are checked only by the MicroTimingChaos editor test.

**Assertions.** Every test file starts with `#undef NDEBUG` before `<cassert>`. Release builds (what CI uses) would otherwise compile every `assert` away. Keep this in every new test file.

**DSP tests** (`modules/core_dsp/tests/DSPTests.cpp`): pure functions and engines, no GUI. This includes the sequencer helpers (`blendDegree`, `timingOffsetMs`, `swingBeats`, `gateBeats`, `velocityFor`).

**State tests** (in each plugin's `PluginTests.cpp`): a round trip with every parameter moved off its default, a refusal test (a state with `suiteStateVersion` 99 changes nothing), and a fixture test that loads a state saved before the `SuiteState` change and checks each recorded value. The fixture test reads `SUITE_FIXTURE_DIR`, and skips with a message when it is not set. The fixtures are not in the repository. The Micro-Timing Chaos fixture also checks the Lerp curve.

**Plugin tests** (`plugins/<Name>/tests/PluginTests.cpp`): each executable compiles the plugin's own sources directly. The Micro-Timing Chaos plugin keeps its source list in the CMake variable `MICRO_TIMING_CHAOS_SOURCES`, which the plugin and its test executable both use, so a new source file only needs adding in one place. Tests include `tests/common/PluginTestUtils.h`. They run under `juce::ScopedJuceInitialiser_GUI`, and `JUCE_MODAL_LOOPS_PERMITTED=1` is defined for them so `MessageManager::runDispatchLoopUntil` exists. `checkEditor()` runs the message loop so the editor's 60 Hz timer fires, then paints the editor into an image.

The Micro-Timing Chaos tests cover the lerp curve (defaults, point editing and ordering, state round trip) and the hybrid drift (Lerp 0 and a zero X multiplier each switch the axis off). The LFO tests cover the no-modulation regression (an embedded table of the committed event log, which must match exactly), timing modulation and its bipolar symmetry, the Amount 0 case, the synced period against the host beat, the free rate in Hz, sample and hold, every target staying in range, state round trip, latency, and the editor with modulation in use. They do not cover mouse interaction with the curve, tab switching, the BPM readout, or the LFO panel's mouse use.

**Host tests** (`tests/host/PluginHostTests.cpp`): loads the built `.vst3` bundle through `AudioPluginFormatManager` with `JUCE_PLUGINHOST_VST3=1`. This define is needed because the host is compiled out by default in JUCE 7. The test scans, instantiates, opens the editor, prepares and processes. CTest passes the bundle path as `$<TARGET_FILE_DIR:<Plugin>_VST3>/../..`, and each host test depends on its plugin's VST3 target.

**Spectral Drift and Dynamic Saturation tests** cover the parameters, state, latency, the editor (both pages paint, and each page draws content of its own, and switching pages changes no parameter), and the shared routing: default routing is bit-identical, routed controls move the audio, synced LFOs follow the tempo clock, and routed values stay in range. The SD test `testOutputGainRampMatchesChannels` checks that both channels see the same output-gain ramp.

**SuiteUI tests** (`modules/suite_ui/tests/SuiteUITests.cpp`): widget logic and painting into images, including the look and feel's drawing paths.

**Editor renders** (`testRenderBaseline` in each plugin's `PluginTests.cpp`): when `SUITE_RENDER_DIR` is set, the test writes a PNG of each editor view: SpectralDrift and DynamicSaturation SOUND and MOD, and MicroTimingChaos CURVE and PREVIEW. Each view is rendered twice in one process and the test fails if the two differ. Without the variable it prints SKIP and CTest is unchanged. Store the output under `build/`. Across processes, the MicroTimingChaos PREVIEW view once differed in 3 glyph-edge pixels on its first render; see plan 02 for the measurement.

Test helpers you will use: `setParameter(apvts, id, value)` takes denormalised values. `checkRange` checks the normalisable range and the default. `checkParameterIds` checks the count against `numControls`. `checkStateRoundTrip` saves state, changes a value and restores it. `addSine` adds to a buffer using a global sample index. `paintComponent` and `countChangedPixels` render a component and count the sampled pixels that differ between two renders (for example, a page shown against the same page with its children hidden).

Mouse events in tests use the 15-argument `juce::MouseEvent` constructor, which still compiles on JUCE 9.0.5.

---

## 7. Decisions and why

- **JUCE pinned to 9.0.5** (was 7.0.9, then 8.0.6). Major versions change APIs and `juce_add_plugin` properties. The 9.0 changes are in section 11. The macOS job has not been verified on 9.0.5. `ParameterAttachment::setValueAsCompleteGesture` takes a denormalised value.
- **`suite_ui` is a module, not in the original layout.** Three editors share the same LookAndFeel and widgets. This is an accepted deviation (section 12).
- **`CoreDSP` is an INTERFACE target linking `juce_dsp`.** The DSP core uses `juce::dsp::FFT`, so it is not JUCE-free.
- **Spectral engine latency = 2048.** The dry path is delayed by the same amount, so mix stays phase-aligned.
- **Dynamic Saturation oversamples 2×** with JUCE's half-band FIR and an integer latency.
- **Output ceiling ±1 before the output level.** Protects against oversampler and tone overshoot. Tested.
- **Micro-Timing has a stereo pass-through bus.** The earlier version declared zero audio buses. The JUCE host loads that version fine, but some DAW scanners drop VST3 audio-effect classes with no audio I/O. This is the most likely cause of the reported DAW load failure. It is a best guess: the DAW failure could not be reproduced here.
- **Micro-Timing latency fixed at 50 ms.** Notes can be early or late by up to 50 ms without the reported latency changing. The earlier plan said latency should equal Timing Range; that was not implemented (section 12).
- **Lerp curve stored in the state, not as parameters.** Five points would have added ten host-visible parameters. Stored as one state property, they save with the session and keep the parameter list stable. The end points keep x fixed, so the curve always spans Chaos 0 to 1.
- **Chaos feeds the curve; Lerp and the multipliers scale the drift.** Chaos no longer drives the melody blend or the timing directly. At the defaults, timing drift is about 37 % (curve 0.74 × Lerp 0.5), so the sequencer is quieter than the earlier versions. Velocity variation is smaller too: about ±7 % at defaults, against ±20 % before.
- **Swing is bipolar** (−20 to +20 %). The engine clamps swing to [−1, 1], so negative values move odd steps early. The earlier clamp to [0, 1] silently ignored negative swing.
- **Assets are embedded with `juce_add_binary_data`.** The `PLUGIN_RESOURCE_DIR` and `PLUGIN_RESOURCE_FILES` arguments are not JUCE 7.0.9 options and do nothing. Embedding makes the plugin load its textures and icons from inside the bundle.
- **Note-offs clamped to note-ons** in the sequencer, and a retrigger cuts the earlier copy first. Prevents stuck notes.
- **Atomics only between audio and UI.** The audio thread never locks.
- **Parameter IDs and plugin codes are permanent.** Renaming breaks saved sessions.
- **`COPY_PLUGIN_AFTER_BUILD FALSE`.** The JUCE default writes into Program Files, which fails without admin rights.
- **Modulation in normalised space.** An amount of ±100 % sweeps the full range of the target, whatever its scale.
- **Discrete targets excluded** (Rate, Length, Scale, Root, Octaves, Host Sync). They would change the length or pitch set abruptly. Seed is a target since commit 5eff266: it is read through `modulatedValue`, so a modulated seed changes the pattern and melody.
- **Tempo modulation applies only while Host Sync is off**, because the host tempo is authoritative when synced.
- **LFO and routing live in `suite_modulation`.** The `Router` uses target indices, not enums, so any plugin can share it. Each plugin supplies its own target table (`TargetInfo`), with `wholeNumber` and `ignoredWhileHostSync` flags. The plugin's `ModTarget` enum keeps its numbers, which are the saved indices. The shared module reproduces MicroTimingChaos's routing exactly. The editor render was checked byte-identical before and after the move.
- **The control display is a module class, not editor code.** `ControlDisplay` takes the router and the list of sliders, and it reads Host Sync through the router. Whole-number targets are rounded inside the router, so the display and the sequencer agree.
- **Modulation applies before the lerp curve**, so a modulated Chaos moves along the curve.
- **The routing and LFO settings are plain APVTS parameters**, so they automate, save and restore like the rest.
- **Effects use a SOUND/MOD page switch**, not a permanent panel. Both show the visualiser on both pages and the `ModulationPanel` on the MOD page. `setPage` only touches visibility, so switching pages never changes a parameter. Since plan 05 the windows are SD 720×660 and DS 780×680 (plan 05.1 added the pan control and the stereo output bar to DS). Since plan 10 phase 9 the effects stack the visualiser above the modulation group on the MOD page instead of overlapping it, and `ModulationPanel::getPreferredHeight()` exposes the group's natural height (the LFO block + routing heading + slot rows) so editors don't hardcode the module's internal constants.
- **Synced LFOs in the effects use a tempo-locked beat clock that follows the host tempo** (120 BPM when there is no playhead), not the host PPQ. The clock only accumulates, so it never jumps on loop or relocate. The routing Delay clock runs from `prepareToPlay` and is not restarted on transport start.
- **Spectral Drift smooths mix, feedback, feedback damping and output gain per sample** (20 ms). These four are consumed per sample in the engine, so block-rate modulation would zip or click; smoothing removes it. The smoothers are advanced once per block into per-sample curves (`mixCurve` and so on), and both channels read the same curves. Advancing them inside the channel loop made the right channel use the next block's ramp, which skewed the stereo image during ramps. `testOutputGainRampMatchesChannels` guards this. Everything else in SD is frame- or block-rate. DS needed no DSP change: it already smooths drive/mix/output level and its other targets are block-rate by construction.
- **The engine's `processSample` keeps its single-argument behaviour bit-identical.** A two-argument overload takes `PerSampleSettings` (mix, feedback, damping); the old form forwards the raw settings, so all existing CoreDSP tests and the reconstruction test are unchanged.
- **The LFO panel and the window height.** Since plan 05 the LFO panel lives on the MOD page of the new page switch, and the window is 980×700 at native scale — the ×2/3 `setScaleFactor` override is gone (owner decision, 2026-11-11). The editor has three pages: SEQUENCE (pattern + step grid), DRIFT (the lerp group fills the left column at full height: curve or preview spans the group width with the four lerp knobs below it; the attractor and the chaos group share the right column) and MOD (the LFO and routing panel alone, since plan 05.1 moved melody and transport to SEQUENCE). The preview's three-colour legend is gone; the lerp multiplier captions carry the axis meaning ("TIMING (X)" and so on). `setPage` toggles component visibility and respects the lerp view state, so the first DRIFT open shows the curve (plan 05.1 bug fix); `paint` skips groups on other pages.
- **Suite UI rules (plan 05, amended by plan 05.1 on 2026-11-12; R11 added 2026-11-18; R14 added 2026-11-20).** One mandatory control language, enforced in `modules/suite_ui` so editors cannot regress: **R1** every rotary knob is a `SuiteUI::LabelledSlider`; the widget sizes the knob to `SuiteUI::knobDiameter` (40 px since plan 05.1 — "2/3 again"; the LED-ring drawing itself is frozen) centred in its cell — no editor caps, scales or draws knobs itself. **R2** caption above, value box below, both centred. **R3** `ToggleButton` and `SegmentedSelector` render at `SuiteUI::pillHeight` (36 px), corner radius `SuiteUI::controlRadius` (6 px); layouts centre them, they never fill a row. **R4** an editor's window size is set per plugin by the owner; a page that cannot fit gets a second page, or the window is resized with the owner's approval. **R5** type: group titles 15 pt bold in the accent, knob captions 13 pt bold `textDim`, value text 14 pt `text`, help/footer 11 pt; one font family — `SuiteUI::uiFont` uses Bahnschrift (the brand font), with JUCE's default fallback where it is missing (macOS). **R6** switches read as state without colour alone: a filled state dot when on, hollow when off, accent border and text when on. **R7** one `groupGap` (8 px) everywhere, group title band `SuiteUI::groupTitleHeight` (20 px). **R8** page switches use the shared `SegmentedSelector` at the top right of the header. **R9** every plugin's test executable writes per-page renders when `SUITE_RENDER_DIR` is set; a UI change ships with before/after renders under `build/renders/`. **R10** future plugins inherit all of this through `tools/new_plugin/skeleton`. The factory preset bar sits flat in the header (no fill or border, plan 05.1). **R11** every plugin ships a user manual, `plugins/<Name>/MANUAL.md`: a walkthrough of every page and mode with a full-page screenshot of each (the R9 render set, stored committed under `docs/manuals/`), plus cropped detail images where they help (cut from the renders by `tools/manuals/crops.py`). **The images are mandatory, not optional**: every page and mode has its own full-page PNG named `<plugin>-<page>.png` (details `<plugin>-detail-<name>.png`), committed under `docs/manuals/`, and the manual is part of "done", not a follow-up. Manual and README images are displayed at about two-thirds of their native size through `<img>` width attributes (`tools/manuals/scale_images.py` does the conversion, and inserts the README's one screenshot per plugin). A UI change updates the manual's screenshots alongside the R9 renders; the brand lockups are the one-font family style from the owner's `tools/brand/lockups.py` (not in this repository; see its header note). **R12** every plugin ships the factory preset seam: `SuitePresets::PresetBar` in the editor header, `SuitePresets::StateHolder` implemented in the processor from the first commit (`getPresetPluginId`, `getPresetStateVersion`, `getPresetStateTree`, `applyPresetState`, plus the plan 09 `presetSelection` virtuals and the `presetSelection` state property), a factory bank of at least 15 presets, and a `<Name>_PresetTests` entry. `suite_add_plugin` always links `suite_presets`, but the bar is editor code, so a plugin that forgets it compiles and silently ships without presets. **R13** every plugin ships one hamburger menu in the header (far right, right of the preset bar) with **Settings…** and **About…**: `SuiteUI::HeaderMenuButton` plus `SETTINGS` in the `suite_add_plugin` call (which adds `suite_settings` to the plugin and its test executable), About… through `SuiteUI::showAboutDialog`, and a test that the menu button is in the header with both actions wired (the About and Settings contents paint; the dialogs themselves are async). R11/R12/R13 are carried by `tools/new_plugin/skeleton`, so a generated plugin starts with a `MANUAL.md` stub, the bar, `StateHolder` and the menu wired; the skeleton's tests check all three. Plan 11 closed the backfill: MidiEffect, StereoTool and MultibandSaturation now have the bar, presets, manual, hamburger menu and SETTINGS (each with a header-contract test), so every plugin follows R11/R12/R13. **R14 (plan 12, 2026-11-20)** — the plan-12 suite rules every editor follows: (a) **help line**: one `SuiteUI::HelpLine` per editor in the footer band, wired with `SuiteUI::attachHelpLine`, idle text = the editor's footer sentence, every control carrying help text via the `SuiteUI::describe` helpers, and `HelpLine::setEnablePredicate` reading the settings' `ui/tooltips` toggle — no per-plugin help copies and no floating `TooltipWindow` in editors; (b) **group frames**: every group is drawn by `SuiteUI::drawGroupFrame` (panel fill at 0.55 alpha, `track` 1 px stroke, 8 px radius, 15 pt bold accent title) — no editor draws group chrome by hand; (c) **dropdowns**: a choice parameter shows as a dropdown only when the owner names it; the widget is `SuiteUI::LabelledDropdown` + `SuiteUI::attachDropdown` (caption above, styled `juce::ComboBox` below), and pills (`LabelledSelector`) stay the default for two-to-six options; (d) **one name for the final gain stage**: the display name is "Output" and the caption `OUTPUT` in every plugin (IDs unchanged: `outputGainId`, `outputLevelId`, `masterVolumeId`, `levelId`).
- **Interface typography.** Group titles are 15 pt bold in the accent colour, for every group. Knob captions are 13 pt bold, and value text 14 pt (look and feel `getLabelFont`). The root note name is part of the parameter text (`C2 (48)`), so the separate note label is gone; Sgraffito's Root Key does the same (`C3 (60)`).
- **Pattern display.** The sequencer view computes the pattern with the modulated Seed, Density and Timing Range, and `CoreDSP::Sequencer::densityCurve` (shared with the processor). Keep the two in step if either changes.
- **LFO shapes.** Random has eight grid points per cycle, each with its own random level, joined by straight lines. Noise holds an uncorrelated random level on each of 32 points per cycle, with no glide. Sample and hold is one level per cycle. Neither Random nor Noise repeats from cycle to cycle. The scope draws Sample and hold, Random and Noise as a scrolling history of the output (128 timer ticks), because a single cycle of them shows nothing. The DSP tests check Random for continuity, Noise for uncorrelated neighbouring levels, and both for non-repetition. **Bouncing Ball** (index 7; the owner replaced the pendulum in plan 17) is four diminishing bounces per cycle: `|cos(2π·4·p)| × (1−p) × 2 − 1` — the ball drops from full height, each bounce lower, rests at the bottom at the cycle's end, and re-drops every cycle at full amplitude (an LFO shape must never decay with the cycle count — the earlier `exp(-|cycles|/4)` pendulum bug did exactly that). Available in every plugin that uses the shared router; the shape choice list is append-only.
- **Live control displays.** Each routable control shows `getModulatedValue()`: its base value moved by the offset published for its target. Offsets are published once per block, so displays follow LFOs in real time. Host automation shows through the base value. While the mouse is held on a control, the display is the user's value, and the modulation shows again on release. Tempo is shown unmodulated while Host Sync is on.
- **Editor sizing (plan 05, amended by plan 05.1 on 2026-11-12; SEQUENCE rearranged by plan 12).** The window is 980×700 at native scale, resizable from 940×660. The layout has three pages (see the LFO panel note above), one `groupGap` of 8 px separates every pair of groups horizontally and vertically, and sets the outer margin. On the SEQUENCE page (plan 12, the owner's arrangement) PATTERN sits on the left as a 2×3 knob grid, TRANSPORT is top right (rate switches, BPM Sync, host tempo readout) with MELODY (root, octaves, the scale dropdown) under it, and the step grid spans the full width below the block. On the DRIFT page the lerp group (58 % of the width, with its four knobs on the right) shares a row with the attractor, chaos spans the full width below. On the MOD page the modulation panel takes the whole page. Knobs are the suite's 40 px since plan 05.1; the editor holds no size constants of its own beyond the page arrangement.
- **Spectral Drift preview:** the old panel was blank because, with no audio, the traces sat at the −120 dB floor. The fix adds a "Waiting for audio" state (shown until frames and input energy arrive) and frequency labels. A regression test checks that a 1 kHz tone reaches the snapshot at bin 43.

**Plan 13 decisions (owner DAW-test findings, 2026-11-21):**
- **Knobs are untouchable.** No change may touch `LabelledSlider`, `knobDiameter` or the knob drawing (R1, owner decision 14). Any phase that seems to need a knob change stops and asks.
- **Meter placement rule.** Every stereo meter pair shows the input on the left and the output on the right, both the full height of their container (`SuiteUI::StereoLevelMeter`, built in plan 13 Phase 2). StereoTool and MultiSat follow it; future plugins inherit it.
- **Long choice lists are dropdowns.** A choice whose option text does not fit a pill (StereoTool's Character and Side Filter) is a `SuiteUI::LabelledDropdown`; pills stay the default for short lists (R14c).
- **Mid/side meter bars** in StereoTool's preview are twice their old width (the owner's "200 %" ask).
- **The note-off guarantee (MidiTool).** Every UI note has a matching note-off on every ending path (mouse-up, drag off, mouse exit, editor destroyed, focus lost, transport stop, reset / releaseResources). The shared `PlayableKeyboardStrip` gained held-note tracking, `mouseExit` release, `releaseHeldNotes()` and a destructor flush; MidiEffect's processor tracks the UI-held set and flushes exactly that set. One test per path.
- **Modulation reaches two more plugins.** MultiSat (10 targets) and StereoTool (10 targets) joined SD/DS with the same procedure; the routed Drive and Mix in StereoTool are smoothed per sample (20 ms), and MultiSat's crossover is smoothed in Hz before the LR4 coefficients, so block-rate routing never clicks. Routing amounts default to 0, so the committed sound is bit-identical.
- **MTC transport knob cells.** The Tempo and Speed knobs sit in one `knobRowHeight` cell instead of the group's full inner height, so the caption sits directly above the knob (owner finding).
- **Pendulum LFO fixed.** The owner's report was right: the shape decayed with the cycle count (`exp(-|cycles|/4)`), so it went silent and the scope froze. It is now the periodic swing `1.5·s − 0.5·s³` with `s = sin(2π·frac)` (full amplitude every cycle); see the LFO shapes bullet above.

**Plan 15 decisions (owner DAW-test findings 3, 2026-11-21):**
- **The stereo meters come off StereoTool and MultiSat** (owner decision). The `SuiteUI::StereoLevelMeter` widget and the plan-13 placement rule (input left, output right, full container height) stay in the shared module for future plugins; the per-channel peak fields went with the meters in both processors.
- **StereoTool is compact**: the mid/side + follower preview sits at the top of the window, and the default height (658) **is** the minimum resize height — the layout sums to the window exactly, and enlarging grows the preview above its 100 px floor.
- **MultiSat draws a log frequency grid** behind its band bars (the same log mapping as the Hz axis), and its SOUND/MOD switch sits in the header row, left of the preset bar (the StereoTool R8 placement).
- **MidiTool's Step Rate goes to 4 Bars**: `1/2`, `1 Bar`, `2 Bars`, `4 Bars` appended after `1/32D` (12 → 16 entries). Append-only, so a saved index keeps its meaning and there is no migration.
- **Per-mode affinity everywhere (plan 10 G1).** A control that does not apply right now is dimmed, never hidden: DS dims Tube Bias outside Tube mode; StereoTool dims Side Crossover while the Side Filter is Off and the follower's Attack/Release/Target while Amount is 0; MidiEffect dims the seven arp-only controls in Chord mode. Sgraffito, Cyclical, MTC and the shared modulation panel already did their own; MultiSat has no mode-only control. Each editor re-evaluates in its 60 Hz timer (the Cyclical `updateAffinity` pattern), with one test per plugin.

**Plan 14 decisions (Cyclical overhaul, 2026-11-22):**
- **Toggle trigger** is appended to `triggerMode` (index 2): the first press starts the slot looping (scheduled on the quantise grid), the second stops it immediately, never delayed to the grid; a note-off never stops a toggled slot; a stopped transport stops the toggled slots and clears the toggle state; a relocate clears it too.
- **Per-slot notes** (`slotNNote`, defaults 60–63 = the old fixed map): `slotForNote` reads each slot's Note; a chopped slot claims `Note .. Note + Slices − 1` (slice k on base + k). Overlaps are accepted, lowest slot wins.
- **Tempo and Host Sync** are appended after Crossfade (the MTC pattern): sync on follows the host's position and tempo, sync off runs on the Tempo parameter. **Beats** (`slotNBeats`, appended after Note) sets the musical length of the whole loaded loop; synced slots play in exactly Beats beats via `region.speed = loopFrames × bpm / (beats × 60 × sourceRate)` — Speed is inactive under sync (G-B, dimmed). Start/Length choose the span without changing the rate.
- **Chop mode** (G-A): `slotNChop` / `slotNSlices` / `slotNSliceLength` appended after Beats. Boundaries `b_i = (i/S)^(2^(2v))` are a public static (`chopBoundary`) shared with the waveform view (the Sgraffito mirror rule); slices sum exactly to the region. **Sync is disabled for a chopped slot** (owner decision) — its Speed stays live, its Beats dims. Granular inside a slice is allowed (G-D). The SLOTS cards gain a second knob row (Note, Beats, Slices, Slice Length) and Chop joins the pill row.
- **Window 1000×876** (G-C: the height is derived from the layout constants; the owner approves the measured size) and the **tangerine `#FF7A45` accent** replace the steel colour everywhere (`git grep 8fa3b8` in plugins/Cyclical is empty); the owner regenerates the lockup.
- **25 Cyclical presets** (15 + 10 concept presets using chop/sync/Toggle/notes); the bank is 210. `Cyclical_PresetTests` presses each slot's own Note key (the old fixed 60–63 no longer covers every preset).

**Plan 16 decisions (Cyclical chop workflow, 2026-11-22):**
- **The Ableton silence was Ableton's VST3 cache**, not the plugin: a full rescan fixed it (see §10). The host test now permanently asserts Cyclical makes sound after a note-on (peak 0.4275).
- **Auto-chop** (the owner picked transient slicing): a per-slot `chopNBounds` state property carries the analyser's interior boundaries (the lerpCurve pattern); the AUTO button sits in the waveform header (dimmed while chop is off); `regionFor` prefers the property when its count matches Slices, the plan-14 power curve otherwise. A Slices edit falls back via the count mismatch; a Slice Length edit clears the property through the APVTS listener (deliberately not Slices — the analyser's own Slices write must not erase its boundaries). The analyser: envelope maxima in 256-frame windows, onsets rising through 15 % of the loop peak with a 1.5× rise, merged closer than 100 ms, capped at 16 slices; it sets Slices and resets the arrangement.
- **Slice re-arranging**: a per-slot `chopNOrder` state property (a permutation of 0…slices−1); key k plays the source slice the order names. The arrangement strip lives inside the region band's bottom edge (the approved window does not change); left-click cycles a cell, cycling back to identity clears the property; a count mismatch falls back to identity; chop off ignores it; the analyser resets it.
- **Transformation tools** (the owner picked reverse loop/slice, normalise, silence trim; no export): REV/NORM/TRIM join the waveform header. Every tool builds a fresh buffer and installs it through the retirement queue (a playing voice keeps its old buffer); the loop reference is unchanged, so transforms are **session-local** (a reload re-reads the original file). REV with a **selected slice** reverses only that slice's span; the selection is right-click on a strip cell (accent-filled cell), message-thread state, reset by the analyser. NORM scales the peak to −0.1 dBFS; TRIM sets Start/Length to the non-silent content (the buffer untouched).
- **Phase 4 (playable keyboard) dropped** by the owner: the rescan fixed the DAW path, so Cyclical keeps DAW-side triggering only.
- JUCE 9 removed `MouseEvent::isPopupMenu()` (the AGENTS §11 note was for JUCE 7): use `event.mods.isPopupMenu()`.

**Plan 18 decisions (DAW-test findings 5, 2026-11-23/24):**
- **The LFO transforms are visible and turnable** (Phase 1): the shared LFO strip gains a transform row — INVERT/RECT pills and four transform controls (Quantise 0–16, Smooth, Pulse width, Skew) — with help texts; the scope draws the transformed output live. Defaults neutral, so an untouched strip changes nothing. (Revisited in plan 20: the four controls are labelled knobs — 30 px, 25 % smaller than the suite knob per the owner — panel preferred height 200 → 428.)
- **StereoTool interest features implemented** (Phase 3, completing plan 17 Phase 8): the eight parameters were dead until now. Mono-maker = a **true side high-pass** (0 = Off; the original 200 Hz default would have broken the identity contract, so the default is 0); multiband width = a two-filter side split (200 Hz / 2 kHz fixed); M/S tone = the shared `ToneControl` per channel; image rotation = an orthonormal L/R plane rotation with an accumulated phase. Five audio regression tests. The routing table gained the missing Pan + 8 targets (16 total).
- **The Phase control is a 0–360° rotation knob** (the owner's pick over a polarity toggle): two cascaded all-pass sections on the right channel, calibrated at 1 kHz, the digital coefficient `c = (1−k)/(1+k)` with `k = tan(φ/2)/tan(ω/2)`; 0° is the bit-exact identity. LEVEL renamed MASTER.
- **Cyclical's former GLOBAL controls are per-slot parameters** (Phase 4, owner decision): Quantize, Swing, Trigger, Pitch, Crossfade, Tempo, Host Sync, Pan ×4; master Output the one global; the GLOBAL page removed; four SLOT tabs + MOD. Each slot runs its own beat clock, grid and swing. The Phase-9 filter/A-R/Level params are wired through a per-slot render stage (scratch → filter → level → pan → master). numControls 57 → 151 (105 after Phase 4, +46 router). **Per-slot Release default 5 ms** (the engine's old feel), not 1000 ms.
- **A real bug found by the router wiring**: the per-slot Level's `jlimit(0, 2, …)` clamped the 0–200 % value at 2 before the /100 — level 100 % played at 2 %. Fix: `jlimit(0, 200, …)`.
- **Region knobs apply live** (Phase 5, owner answer): a knob release that moved Start/Length/Slices/Slice Length/Beats retriggers the slot's voice (the plan-16 retransform pattern, driven from the editor's drag end; a no-op drag does not restart). Gate slots wait for the next press.
- **Cyclical's modulation targets are per-slot** (Phase 6): Level 1–4, Cutoff 1–4, Speed 1–4, Pan 1–4 (16 targets). Level/Cutoff/Pan read `modulate` per block per slot; Speed reads the block offsets in `regionFor` (a routed speed is heard from the trigger). The ROUTER_PARAMETER_IDS in `audit_bank.py` gained the 12 transforms.
- **+10 presets each for DS, MTC and Sgraffito** (Phase 7), focused on Bouncing Ball and the transforms; ten new Sgraffito sample recipes; the bank is 240; the audit's expected counts updated.
- **Ableton rescan note applies again** after every parameter-count change (Cyclical 57 → 151, StereoTool 48 → 69).

**Plan 20 decisions (DAW-test findings 6, 2026-11-24):**
- **The transform-scope bug is real and fixed** (Phase 1): the scope's one-cycle curve and `curveValue` sampled the raw three-argument `lfoValue` — transforms never reached the drawn waveform (the plan-17 "the scope's drawing follows automatically" note was an assumption the code never kept). The fix is one shared path: `Router::transformedLfoValue (lfo, cycles)` builds the `LfoTransforms` from the cached parameters and applies the Offset; `lfoOutputAt` delegates to it and the scope samples it. `testTransformedLfoValueFollowsTransforms` guards it. The drawing is byte-identical with neutral transforms (the `LfoTransforms` defaults are identity).
- **The transform controls are suite 40 px knobs with captions** (owner refinement): QUANTISE / SMOOTH / PULSE / SKEW as `LabelledSlider`s in one full-width row, INVERT/RECT pills (52 × 26) centred in the row beside them; the RATE knob moved to the top row beside the waveform, so the transform row spans the whole column. JUCE lesson: `SliderAttachment` sets `slider.textFromValueFunction` from the parameter's `getText` — the slider's own `setNumDecimalPlacesToDisplay` never shows; put the formatting on the parameter (Smooth/Pulse/Skew ranges now carry a 0.1 interval, the routing Amount's pattern; Quantise keeps 0.5).
- **Second refinement round (same day)**: the four transform knobs are **25 % smaller than the suite knob (30 px)** — an owner-issued size exemption for these four controls; the RATE knob stays suite-size. The waveform-type selector moved **under the rate knob** (110 px wide, the old full-width row is gone). The transform-knob cell mirrors `LabelledSlider`'s formula with the smaller diameter (72 px), so the strip is 250 px and the panel's preferred height is **428**. Two windows were sized for the old 414 panel and clipped the sixth routing row — **Spectral Drift 750 → 820** and **MultiSat 700 → 760** (both re-rendered, owner approves the measured sizes per R4); MultiSat's test hardcoded the editor size and crashed before rendering (now asserts 760).
- **A layout bug the refinement exposed**: since the knob conversion the strip laid out 276 px inside a 240 px block — the SYNC/DIVISION row was pushed out of the strip and **silently missing from all six renders** (the panel test only checks panel-level children, so the overflow inside a strip child was invisible to it). The new 250 px block restores the row. Lesson: a strip's internal overflow shows only in renders — keep the block height equal to the sum of its rows.
- **Third refinement round (same day)**:
  - **MTC's INVERT/RECT toggles showed icons, not text** (the owner's report): MicroTimingChaos's `drawToggleButton` always drew the BPM-Sync icon, texture and ON/OFF state — on the shared panel's 52 px toggles the text was crushed. The icon variant now applies to the "BPM SYNC" button alone; everything else falls back to the suite's text drawing.
  - **Cyclical's mix row** (owner ask): the four slots' **Level and Pan moved out of the slot cards into ONE group named MASTER inside the card frame** — the four Level knobs (each beside its little meter) and Pan knobs share it with the Output at the end, so no small frames touch. The processor publishes each slot's post-level peak per block (`slotLevelPeaks`, measured in the fold loop) and the meters decay it in the timer. The ENVELOPE group keeps Attack/Release; the MOD-page display and bindings rebind to the new widgets. Window 1000×1072.
  - **PLAY MODE named and moved**: the direction/Ping-Rand/Grain/Chop group now sits **directly below the sample preview**, properly titled "PLAY MODE" (the pills used to overlap its title band, which is why the group read as unnamed), and gained the **TRIGGER selector and HOST SYNC** (owner follow-up).
  - **The selectors live with their families** (owner follow-up): QUANTIZE joined **KEY & SLICES** (quantising belongs with the slice grid); CLOCK & MIX is the four clock knobs only (Swing, Tempo, Pitch, Crossfade).
  - **Cyclical's Note knobs show the note name** (owner ask, the MTC/Sgraffito pattern): the `slotNNote` parameters gained `withStringFromValueFunction` — the value box reads `C3 (60)`.
  - **The tangerine Cyclical lockup is regenerated and committed** (owner task, closed).
- **Cyclical's slot pages are grouped** (Phase 2 + owner refinements): PLAY MODE (direction, Ping Rand, Grain, Chop, Trigger, Host Sync) sits directly below the waveform; PLAYBACK (Start/Length/Speed/Velocity) and KEY & SLICES (Note/Beats/Slices/Slice Length + QUANTIZE) span the full width; below them a two-column section — **ENVELOPE on the left** (Attack/Release stacked, spanning the section) with **FILTER** (one compact row: the `LabelledDropdown` type + Cutoff + Reso) and **CLOCK & MIX** (the four clock knobs) on the right; the MASTER mix group inside the card frame (plan 20). Waveform 110 → 170 px. The window height is derived from the layout constants (`cardContentHeight` sums the groups — a mismatch cuts the bottom off, which the first render showed).
- **The Cyclical filter and Attack/Release findings were not DSP bugs** (Phase 0 probes, now regression tests): a 10 kHz tone through slot 0 LP at 300 Hz is >20 dB down (`testProbeFilterDsp`, RMS 0.248 → 0.00024); an 800 ms attack rises ~12× slower than the 1 ms default over the first 100 ms (`testProbeAttackReleaseDsp`, RMS 0.245 → 0.021). Both filters are prepared in `prepareToPlay` and `setEnvelopeTimes` is applied at trigger; the owner's findings were UI/affinity or DAW-cache ghosts.

---

## 8. SuiteUI is compiled into each consumer

`SuiteUI` is **not** a library target. Any consumer calls `suite_ui_add_to_target(<target>)` from `modules/suite_ui/SuiteUI.cmake`, which adds the two `.cpp` files and the include directory.

Reason: a static library that links JUCE module targets bundles those module objects into the archive. Each consumer also links the JUCE modules itself, so the link fails with hundreds of duplicate symbols (`juce::Colour`, `JUCEApplicationBase`, `juce_audio_utils`, and so on). A plain `OBJECT` library had the same problem. Compiling the sources directly avoids it. Consumers still link their JUCE modules explicitly. The test executables link `CoreDSP` and `SuiteUI` but not extra JUCE modules, so they do not need them listed.

If you add a new consumer, use `suite_ui_add_to_target`. Do not add `SuiteUI` to `target_link_libraries`.

**R3 completion**: The editor UI base is fully implemented. All four plugins share the suite look and feel through `SuiteUI::SuiteLookAndFeel`. MicroTimingChaos's own LookAndFeel derives from it, keeping only MTC-specific drawings (knobs, toggles, combo box). The shared refresh rate constant `SuiteUI::kRefreshHz = 60` is used by all editors. One minor owner-decided baseline remains for MicroTimingChaos's PREVIEW tab first-render effect (see plan 02).

`suite_modulation` follows the same rule (`suite_modulation_add_to_target`, never `target_link_libraries`). Its panel uses `SuiteUI::LabelledSlider`, so a consumer calls `suite_ui_add_to_target` on the same target first. Its tests are registered in `modules/suite_modulation/CMakeLists.txt` only when `SUITE_BUILD_TESTS` is on, and the directory is added only inside the `SUITE_BUILD_PLUGINS` block.

`suite_settings` follows the same pattern (`suite_settings_add_to_target`, never `target_link_libraries`). The dialog builds on plain JUCE widgets, so it does not need SuiteUI compiled in; the cmake comments keep the old call-order advice for safety. Its tests are registered in `modules/suite_settings/CMakeLists.txt` and only built when plugins are enabled.

---

## 9. Procedure for common changes

**Adding a parameter**
1. Add the ID constant to the processor header, in the matching group.
2. Add the `AudioParameter*` to `createParameterLayout()` with its range and default. Use a `NormalisableRange` lambda for log ranges.
3. Cache its `std::atomic<float>*` in the constructor, and read it in `processBlock` or `readSettings`.
4. Bind a widget in the editor: `SliderAttachment`, `ButtonAttachment`, or a `ParameterAttachment` for choices. Add a `LabelledSlider` to the layout.
5. Update `numControls`, the plugin test's `checkParameterIds` list and its `checkRange` calls. The count must equal the number of parameters the layout creates.
6. Add DSP tests if the engine changed, and a plugin test for the audible effect.
7. Update the README parameter table, section 4 of this file, and `CHANGELOG.md`.

**Changing a parameter range or default.** Update `createParameterLayout()`, the plugin test's `checkRange`, section 4 here, and the user README. Note the change in `CHANGELOG.md`, since saved sessions can be clamped by a new range.

**Changing a saved parameter or state layout:** bump `stateVersion`, add a migration, add a fixture test. Saved sessions carry a `suiteStateVersion` attribute; states without the attribute are version 1. Newer states than the plugin are refused and leave the current state unchanged.

**Adding a factory preset** (bank build)
1. Add a `_preset(...)` entry to `tools/preset_data.py` under the right plugin, using only IDs from that plugin's schema. Values must be inside the parameter's `NormalisableRange`.
2. Run `python tools/build_bank.py` to regenerate `bank/AudioSuitePresets.xml` and the per-preset JSON files.
3. Run `python tools/audit_bank.py` — it enforces unique names, ≥3 categories per plugin, in-range values, no preset equal to the default state, and the category/format rules.
4. The `bankNotDefault` test in each plugin's `*_PresetTests` executable asserts every preset differs from the default state.

**Host program list** (`SUITE_PRESET_BANK_AS_PROGRAMS`)
- Program 0 is Init; programs 1..N are the factory presets in bank order. The order is **append-only** once hosts cache indices — add new presets at the end of each plugin's list in `tools/preset_data.py`, never reorder or remove.
- The virtuals map to `applyPresetState` / `getStateInformation`. `currentProgramIndex` tracks the last applied program (-1 after any non-factory state change).
- This flag is separate from `SUITE_PRESET_BANK` and is revertable.

**Changing a DSP engine**
- Add a test in `DSPTests.cpp` first where possible. Keep the reconstruction test passing.
- If latency changes, update `getLatencySamples`, the plugin latency tests, and the README latency table.

**Editing files.** The working tree is edited by the IDE and by the owner. Some files change between reads. Re-read a file before editing it.

**Commits.** Do not commit or push unless the owner asks. Git is initialised, and the origin remote is set.

---

**Adding a plugin**
1. Run `python tools/new_plugin/new_plugin.py <Name> "<Product name>" <Code>`. It copies `tools/new_plugin/skeleton/` into `plugins/<Name>/`, fills in the names, and refuses if the folder exists or the plugin code is in use. It writes nothing else.
2. Add `add_subdirectory(plugins/<Name>)` to the root `CMakeLists.txt`, after the other plugins. The script prints this line.
3. The plugin's CMake is one `suite_add_plugin(...)` call (`modules/suite_plugin/SuitePlugin.cmake`). It sets the plugin target, its `<Name>Tests` executable, the links, the lockup and `add_test`. Its arguments are described at the top of that file. `DESCRIPTION` sets the bundle description (default: the product name). The company website and copyright are set once, in the root `CMakeLists.txt`. Add `MODULATION` for the LFOs and routing, `SETTINGS` for the hamburger menu's Settings dialog (R13 — the skeleton already passes it), `IS_SYNTH` / `IS_MIDI_EFFECT` / `NEEDS_MIDI_INPUT` / `NEEDS_MIDI_OUTPUT` for MIDI effects and instruments, and `LOCKUP` or `ASSETS` for embedded images and files. The skeleton already carries the preset bar, `StateHolder` (R12) and the hamburger menu (R13); presets and the lockup are per-plugin authoring.
4. Add `suite_add_host_test(<Name>)` to `tests/host/CMakeLists.txt`. Add one row to the lockup table in the owner's `tools/brand/lockups.py` (file name, product name, accent) and run it to generate the lockup. The generator is the owner's local tool and is not in this repository, so ask the owner to run it. Add it to the README and the CHANGELOG.
5. The function checks that the plugin code is four characters and unique across all plugins. A duplicate stops configuration.
6. Finish the user manual, `plugins/<Name>/MANUAL.md` (rule R11 — the skeleton ships the stub): run the plugin's tests with `SUITE_RENDER_DIR=<repo>/docs/manuals` for the full-page screenshots of **every page and mode**, cut any detail images with `tools/manuals/crops.py`, and walk through the interface in the manual. The images are mandatory and committed (`<plugin>-<page>.png`, `<plugin>-detail-<name>.png`); a plugin without them is not done.
7. The skeleton's render test writes its views when `SUITE_RENDER_DIR` is set; extend it to cover every page and mode the manual shows (rule R9).

**Adding modulation to a plugin** (the procedure used for Spectral Drift and Dynamic Saturation)
1. In the plugin's CMake, pass `MODULATION` to `suite_add_plugin`. This adds `suite_ui_add_to_target` and `suite_modulation_add_to_target` to the plugin and its test executable.
2. Declare a `static constexpr SuiteModulation::TargetInfo` table of the controls the routing may move, in ModTarget order, with `wholeNumber` and `ignoredWhileHostSync` where they apply. Append only.
3. Add `SuiteModulation::Router::addParameters (params, names)` to `createParameterLayout()`, with `names` = `"None"` followed by the target names, in table order.
4. Construct the router in the processor after `apvts`, with the table and the host sync ID (or `nullptr`). Call `prepare`, `computeOffsets`, `publish` and `advanceBlock` as MicroTimingChaos does. Read each routed control through `modulate`.
5. The editor builds `SuiteModulation::ModulationPanel` and `ControlDisplay`. Add the plugin's `numControls` and the parameter tests (`checkParameterIds`, `checkRange`).
6. Record a probe of the unmodulated output before the change, and diff it after. Document the synced-LFO fallback for a plugin with no beat.

## 10. Open items and known risks

- **MicroTiming DAW load:** the reported failure is not reproduced. JUCE's VST3 host loads and opens it, both before and after the stereo bus change. The remedy applied (stereo pass-through) is a best guess. If it still fails in a DAW, ask for the DAW name, its scanner log, and the exact error.
- **Ableton's VST3 cache (2026-11-22, plan 14 review):** Cyclical appeared completely inert in Ableton ("nothing reacts at all") after its parameter count changed 35 → 57 — Ableton's plugin cache still held the old parameter list. A full rescan (Preferences → Plug-Ins → Rescan, incompatible cache off) fixed it. After any parameter-count change, a DAW rescan is part of the deployment; tell the owner before blaming the plugin.
- **Presets (Phase 7 done):** default build (`SUITE_PRESET_BANK=OFF`) is visually and behaviourally unchanged — 20 CTest entries, all green. Bank build (`SUITE_PRESET_BANK=ON`) adds 8 CTest entries, all green: one `*_PresetTests` per plugin. `SUITE_PRESET_BANK_AS_PROGRAMS` adds the host-program virtuals with no entry-count change.
- **Preset licence (plan §11):** GPL. Decided 2026-11-12: the licence stays GPL, and the factory presets are now publicly available; it stays GPL.
- **macOS / AU / Standalone / DAW verification:** not performed for presets; same caveats as before.
- **GUI tests on CI:** create editors, so CI needs a desktop session. Editor renders under `build/` are required before claiming "no visible change".
- **Bank binary data namespace:** `SuitePresetBankData`, not `BinaryData`, so it does not collide with each plugin's `<Name>Assets` binary-data target.
- **PopupMenu recursion:** `containsCommandItem` does not recurse into submenus; tests use `MenuItemIterator(..., true)` instead.


---

## 11. Gotchas (verified in this codebase)

- `juce::dsp::FFT::perform` with `inverse = true` divides by N already.
- `juce::dsp::WindowingFunction` normalises by default. Pass `normalise = false`, and measure the overlap gain numerically.
- `juce::dsp::Oversampling<float>(channels, factor, type, isMaxQuality, useIntegerLatency)`: factor 1 means 2×. `processSamplesUp` takes `AudioBlock<const float>`. `processSamplesDown` takes a non-const `AudioBlock<float>&`.
- `AudioPlayHead::getPosition()` returns `juce::Optional<PositionInfo>`, not `std::optional`.
- `ParameterAttachment` callbacks and `setValueAsCompleteGesture` both work with **denormalised** values. For a choice parameter that is the index as a float.
- `juce_add_plugin` has no `PLUGIN_RESOURCE_DIR` or `PLUGIN_RESOURCE_FILES` in JUCE 7.0.9. Those arguments are silently ignored. Embed assets with `juce_add_binary_data` and load them through `BinaryData::getNamedResource` (names like `sequencer_bg_4k_png`, `bpm_sync_active_svg`).
- `juce_add_plugin`: `PLUGIN_NAME` maps to `JUCE_PLUGIN_NAME`. `IS_MIDI_EFFECT TRUE` and `NEEDS_MIDI_INPUT/OUTPUT TRUE` make a MIDI effect.
- `juce::Vector3D` is in `juce_opengl`, which the plugins do not link. Use a small struct for three values.
- A namespace cannot be aliased with `using`. Write `namespace Palette = MicroTimingChaos::Palette;`.
- `LookAndFeel_V3` (the base of `LookAndFeel_V4`) has a private member `backgroundTexture()`. A free function with the same name, called inside a `LookAndFeel` method, resolves to the member. Name helpers differently.
- `juce::Slider` and `juce::ComboBox` are `SettableTooltipClient`s, so they take `setTooltip`. `juce::Component` takes `setHelpText` and `getHelpText`, which a mouse listener can read from `e.eventComponent`.
- `juce::Graphics::drawText` takes a `Rectangle<float>` or a `Rectangle<int>`, and `juce::String (float, int)` formats decimals.
- `juce::MouseEvent` has `isPopupMenu()` for right-click on Windows.
- `JUCE_PLUGINHOST_VST3` is 0 by default in JUCE 9, as in 7. It must be 1 to use the VST3 host in a test.
- `MessageManager::runDispatchLoopUntil` exists only when `JUCE_MODAL_LOOPS_PERMITTED` is 1.
- `juce::MouseEvent` in 7.0.9 takes 15 arguments, with no pen details. The same 15-argument form compiles in 9.0.5.
- Clang rejects a nested struct's default member initialiser used as a default argument in its enclosing class. `LorenzState` is at namespace scope for this reason.
- `juce::File bundle (juce::String (argv[1]));` is a function declaration (most vexing parse). Use braces.
- Duplicate-symbol link errors with JUCE: see section 8.
- `add_library(... STATIC)` linking JUCE modules bundles them. `OBJECT` libraries have the same problem.
- The root `CMakeLists.txt` must `include()` `SuiteUI.cmake` before any plugin subdirectory, because `suite_ui_add_to_target` is a function.
- `juce::AudioParameterFloat`'s seven-argument constructor (label, category, `stringFromValue`) gives the value box its text. Setting `slider.textFromValueFunction` after the `SliderAttachment` exists does not refresh the box, so put the formatting on the parameter.
- In JUCE 7.0.9 the text box position is `juce::Slider::TextBoxRight`. `juce::TextEntryBoxPosition::TextBoxRight` does not compile.
- **Apple clang fuses multiply-add on arm64 by default, which breaks exact zeros.** `juce::NormalisableRange::snapToLegalValue` computes `start + interval * floor(...)`. With `start = -100` and `interval = 0.1f`, a zero default snaps to exactly `0` when the multiply and add round separately (MSVC), but to `1.49e-6` when they are fused (arm64 clang). The MTC test `testOffsetControlIsUsable` failed on macOS for this reason, while Windows passed. The root `CMakeLists.txt` sets `-ffp-contract=off` for Apple clang before JUCE is fetched. Keep it, because the exact-zero asserts rely on it. Compare the same arithmetic on both platforms before blaming a test.
- A baseline for a processing change: build the committed code in a `git worktree` with `-DFETCHCONTENT_SOURCE_DIR_JUCE=<repo>/build/_deps/juce-src` (no re-download), run the same probe there, and diff against the new build.
- **A bare keyword list in `suite_add_plugin` misparses silently.** `juce_add_plugin`'s `IS_SYNTH`, `IS_MIDI_EFFECT`, `NEEDS_MIDI_INPUT` and `NEEDS_MIDI_OUTPUT` are one-value keywords (`cmake_parse_arguments`), so `IS_SYNTH IS_MIDI_EFFECT NEEDS_MIDI_INPUT` reads as IS_SYNTH="IS_MIDI_EFFECT", IS_MIDI_EFFECT="NEEDS_MIDI_INPUT", NEEDS_MIDI_INPUT="SOURCES" — all truthy, and the next keyword's value is eaten. `suite_add_plugin` converts its own flags to explicit TRUE/FALSE, but the caller must pass each flag alone, never two on one line. MIDI Chord Arp Scale Tool shipped as a synth for this reason until plan 11 (verified in `Defs.txt`: `JucePlugin_IsSynth=0, JucePlugin_IsMidiEffect=1`).
- **Choice widgets (plan 11).** A `SuiteUI::LabelledSelector` pill (caption above, `SegmentedSelector` below, the `ParameterAttachment` wiring encapsulated in the widget) for every choice parameter with six or fewer options; a dropdown (`juce::ComboBox` + `ComboBoxAttachment`) only above six or for long text. CHORD (eight options) is the one dropdown left. The per-editor `ComboCell` class survives only where a dropdown is still needed.
- `juce::String` has `indexOfChar (juce_wchar)`, not `indexOf (char)`. `indexOf` takes a String or a start index plus a String.
- `BinaryData::getNamedResource` in JUCE 9 takes `(const char* resourceNameUTF8, int& dataSizeInBytes)` and returns the raw `char*` data. The resource name keeps only alphanumerics ("grain_4k.wav" becomes "grain_4k_wav"), and the consumer must `#include "BinaryData.h"`. `registerBasicFormats()` covers WAV and AIFF only — FLAC needs a separate define, which is why the factory samples are WAV.
- `AudioFormatManager::createReaderFor` takes a `juce::File&` or an owning `std::unique_ptr<juce::InputStream>`; wrap memory data in a `juce::MemoryInputStream`.
- `AudioProcessorValueTreeState::ParameterLayout` has no public accessor for its parameters. To append shared-module parameters, build a `std::vector<std::unique_ptr<RangedAudioParameter>>` first, call the shared `addParameters`, then return `{ params.begin(), params.end() }`.
- Test failures reported as `Exit code 0xc0000409` with `Assertion failed` are a failed `checkRange` or similar assert. The message names the file and line. Run the executable directly to see the full output.
- `juce::AudioParameterFloat` with the eight-argument form (label, category, `stringFromValue`, `valueFromString`) is deprecated in JUCE 7.0.9 and 9.0.5. MSVC warns with C4996. The shared router uses it as the MicroTimingChaos layout did. Move to the `Attributes` form when the plugins are next touched.
- `juce::Component` has `setHelpText`, not `setTooltip`. The editor's help line reads the help text of the component under the pointer, so copy the exact strings when moving a panel.
- `juce::AudioParameterChoice::choices` is a public member. `getParameter (id)` returns a `juce::RangedAudioParameter*`, so `dynamic_cast` it to read the names.
- A broken test source blocks the whole build, not just its own tests. `cmake --build` stops at the first failing target, and the other executables then run from stale binaries. Before reporting a green CTest run, check that each test executable was built from the current sources.
- **JUCE 8 and later: release a `Graphics` before reading its image.** Drawing through a live `juce::Graphics` is not visible in the `juce::Image` until the `Graphics` is destroyed. Reading pixels inside its scope gives zero. JUCE 7 showed the drawing immediately, which is why the SuiteUI paint check passed on 7.0.9 and failed on CI's 8.0.6 (`Assertion failed: painted > 0`, reported as `Exit code 0xc0000409`). Scope the `Graphics` in every test helper that reads pixels.
- **JUCE 9 editor lifetime.** `AudioProcessor::createEditorIfNeeded()` is deprecated; use `createEditorAndMakeActive()`, which returns nullptr while an editor is active. Before an editor that the test owns is destroyed, call `processor.editorBeingDeleted (editor.get())`. A host's wrapper does this, and `checkEditor` in `tests/common/PluginTestUtils.h` does it too.
- **JUCE 9 removed APIs:** `AudioPluginFormatManager::addDefaultFormats()` (add `juce::VST3PluginFormat` directly); `Drawable::createFromSVG (XmlElement)` (use `createFromSVGString`); `Font::getStringWidthFloat` (use `juce::GlyphArrangement::getStringWidth (font, text)`).
- **JUCE 9 splits the audio processor code** into `juce_audio_processors_headless` (no GUI) and `juce_audio_processors`. The plugins and tests still link `juce_audio_processors`, which is correct.
- **Changing the JUCE tag does not refresh an existing checkout.** FetchContent keeps `build/_deps/juce-src` at its old version. After a version change, delete `build/_deps/juce-src` (or the whole build directory) and reconfigure, then check `JUCE_MAJOR_VERSION` in `juce_StandardHeader.h`. A stale checkout built silently against 7.0.9 when the pin said 9.0.5.

---

## 12. Deviations from the original brief

The original brief (`coding_agent_prompt.md`) is no longer in the repository. This is what the built code does differently, and why.

| Brief | Built | Why |
|---|---|---|
| Layout lists only `modules/core_dsp` | Adds `modules/suite_ui` and `tests/` | Shared UI for three editors, and integration tests |
| Tests use `assert` | `assert` with `NDEBUG` undefined, plus plugin and host tests | Release builds would otherwise drop the asserts |
| `SpectralEngine.h`, `SaturationEngine.h`, `ChaosEngine.h` in `CoreDSP` | Same, plus `CoreDSP.h` umbrella and `SequencerEngine.h` | As specified, plus the sequencer maths |
| Spectral Drift: four parameters | Fourteen. The four named ones are unchanged | User request for more FX controls |
| Dynamic Saturation: five parameters | Thirteen | User request for eight more |
| Micro-Timing: MIDI effect, no audio buses | MIDI effect with stereo pass-through | Some DAW scanners drop audio-less VST3 effects |
| Micro-Timing: latency equals Timing Range (plan) | Fixed 50 ms | Changing the reported latency with a knob would disturb host delay compensation |
| Micro-Timing: five curve points as parameters (plan) | One state property holding the points | Keeps the parameter list small and stable across sessions |
| Micro-Timing: Lerp shown in Lerp and in Transport (plan) | Lerp appears once, in the Lerp Curve group | Avoids two controls for one value |
| Micro-Timing: `LookAndFeel` in `include/` (plan) | In the plugin root, next to the other source headers | The root is on the include path for the plugin and its tests |
| Output spectrum on the display | Measured on finished output (one frame behind) | Shows the real overlap-add result |
| Micro-Timing: LFO panel in the right column, window 940 px (plan, D9) | LFO panel at the bottom of the left column, window 980 px | The right column had no room for a lerp curve |
| Micro-Timing: LFO and routing as APVTS parameters (plan) | 26 new parameters, `numControls` 46 | The plan's design; sessions save them |
| Shared LFO and routing module (refactor plan) | `ControlDisplay` takes the router and the slider list, with no APVTS argument. The router reads Host Sync itself. The panel uses `SuiteUI::LabelledSlider`, so it needs SuiteUI in the same target | The router already knows the target table and Host Sync. A second copy of the sync flag would be a second source of truth |

---

## 13. Sgraffito plugin development complete

Development of the Sgraffito memory sampler plugin is complete across all phases:

- **Phase 4 - Creative Features**: Lorenz chaos drift, tape stop, ghost notes, scale lock, cloud granular mode. 71 parameters total (37 original + 34 new: 2 LFOs + 6 modulation slots + 21 creative parameters).
- **Phase 5 - Modulation Infrastructure**: SuiteModulation::Router shared with other plugins, 2 LFOs, 6 routing slots (source/target/amount/delay). All parameters automatable, savable, restorable.
- **Editor**: 880×700 window (plan 05 budget; plan 05.1 added the MOD page and the playable keyboard), SOUND/TEXTURE/MOD pages, brand lockup in the header (`lockup_sgraffito.png`, embedded via LOCKUP; generated by the owner's lockup template generator `tools/brand/lockups.py` (not in this repository) — one table row per plugin, composited from the family template so the wordmark, "Audio" and mark are pixel-identical across the suite; the old one-off `build/brand/lockup_sgraffito.py` is superseded) and the factory preset bar (added to the component tree in every build). Knobs come from the shared `SuiteUI` tokens (40 px, plan 05.1); toggle buttons and selectors are 36 px pills. The SOUND page shows the waveform view (region overlay, Chop slice lines) and a Root Key value box that reads as a note name and number (`C3 (60)`); the TEXTURE page groups the controls into framed rows (TAPE & MEMORY, DRIFT & TIMING, GHOST, CLOUD, SWELL & SURFACE); the MOD page holds only the `ModulationPanel` (both LFOs in the slate accent). A status bar below the keyboard shows the help text of the control under the pointer (`SuiteUI::HelpTracker`, plan 10 phase 5; honours the settings' `ui/tooltips` toggle), and every control carries a help text. Controls that only work in one mode (Slice Count = Chop, the three grain knobs = Cloud, the two swell knobs = Swell) are disabled — dimmed, never hidden — in the other modes (`updateModeAffinity`, plan 10 phase 2, decision G1), and Slice Count sits in the TAPE & MEMORY row. A `PlayableKeyboardStrip` (MIDI 24–96) sits above the status bar on every page and plays through an SPSC note queue drained in `processBlock`. The waveform's region maths mirrors `CoreDSP::Sampler::keyRegion`/`chopRegion`, and the SOUND page also draws an envelope preview — the attack/sustain/release trapezoid of the displayed key's region, laid over the waveform (plan 10 phase 3, decision G3).
- **Presets**: Sgraffito ships 30 factory presets (the 15-preset minimum plus five added by plan 05.3 F1.3 and ten by plan 18 Phase 7); the suite-wide bank is 240 factory presets (36 SD / 46 DS / 38 MTC / 30 Sgraffito / 15 MidiEffect / 25 StereoTool / 25 Multiband / 25 Cyclical) behind SUITE_PRESET_BANK flag. CI and the release workflow configure with `-DSUITE_PRESET_BANK=ON`, so published binaries carry the presets. Since plan 10 phase 7 every Sgraffito preset ships its own generated sample (`embedded:<slug>_4k.wav`, generated by `tools/sgraffito_samples.py`, one recipe per preset, ~7.5 MB embedded total; the old `cloud_4k.wav` variant is retired and `grain_4k.wav` stays the INIT default), so selecting a preset always loads the sample it was made with.
- **Documentation**: AGENTS.md updated with Sgraffito plugin entry, parameter tables, and minimum 15 presets requirement.
- **Test results**: 20 CTest entries with `SUITE_PRESET_BANK=OFF`, 28 with `=ON` (both include `SuiteSettings_Tests` since plan 08). The 8 plugin test executables run in both; the bank build adds 8 preset-test executables. Sgraffito's plugin tests cover 55 functions: parameters, state, note-on/off voice handling, region mapping, Chop slices, output, the editor paint, the preset bar in the component tree, per-page knob visibility (a knob missing from a page's visibility list floats over every page), editor renders (3 pages plus the missing-sample overlay), the shared routing, the plan 05.1 sound regressions (the four modes produce different audio; the key changes the pitch), the T2 audio regressions (Scrub plays at the sample's own rate; Cloud holds its level in stereo over a second with identical L/R reads; a tape-stop release frees its voice), the plan 07 sample-linked preset/session tests (7 in phase 1, 5 in phase 2, 2 in phase 4: the async latest-wins race and destruction with a load pending), the plan 09 tests (the capped retirement queue, pending-note stealing per D1, pending Cloud lanes, pending-matches-direct, and the preset-identity session round trip), the plan 10 tests (LOAD SOUND top-right bounds; mode affinity; the status bar's help texts; Attack on direct notes; Release shaping the tail under tape stop; the envelope overlay; Root Key as pitch centre; the sample switch-back rule), and the plan 12 tests (the region-boundary declick in Scrub and Swell, the Output gain scaling, the quietest-voice steal).
- **Plan 12 owner findings**: the Level knob (now **Output**, `levelId` unchanged) is wired to the summed voice output through a 20 ms `SmoothedValue`; 0 dB is exactly 1.0, so the default is bit-identical. A one-shot that reaches its region boundary fades over its last 5 ms instead of cutting (distance-based: `declickFactor = distanceToEnd / declickFrames`, so any playback rate lands on ~zero at the cut; no Voice state needed). A voice steal takes the **quietest** active voice (`envValue × gain × velocity`) after the tape-stop and oldest-release tiers. Deep Cloud stays Scrub by owner decision, so the declick covers the Scrub cut it hits.
- **Audio playback (per-sample loop)**: `processBlock` reads and writes audio sample by sample. The voice state machine runs **once per output sample** and each voice's contribution is then read per channel — the older loop ran the whole state machine once per output *channel*, so stereo output advanced every voice twice as fast (Scrub/Chop/Swell played at double speed and one-shots ended after half the region) and Cloud's second channel read grain positions the first pass had already moved (decorrelated L/R, the reported crackle). The envelope (`Voice::envelope`) runs per sample; a released voice deactivates when its release envelope reaches zero (`envEnd >= 0.0`, which also frees a voice released at stream sample 0 — the old `> 0.0` test left silent zombies). Scrub and Chop advance `voice.position` by `rate` frames per output sample and end as one-shots at the region end; Swell starts at the region end and reads backwards; Cloud runs a per-lane grain machine (`cloudGrainPosition`, half-sine window, `grainSize`/`grainDensity`/`grainSpread`, wrap within the region) whose reads are linearly interpolated (`readLinear`). A mono sample (the embedded `grain_4k.wav` is mono) feeds every output channel — the per-channel read must use `ch % sampleChannels`, or a stereo output crashes on `getReadPointer`. Memory fades each voice's gain toward `1 − amount` over the decay time (amount 0 leaves the note alone; the old binary gate faded every voice toward true silence). The tape-stop ramp ticks **per output sample** and its level factor multiplies the voice's output (it used to tick once per block — a 400 ms ramp took minutes — and the level was ignored, so released voices piled up at full level). Dust/crackle/wow/flutter run as bounded per-block **factors** recomputed from the voice's phase state (`surfaceGainFactor`, `surfaceRateFactor`, `surfaceReadOffset`); they never compound into `voice.rate`/`voice.gain` (the old per-block multiplication grew exponentially). A lane fires one grain at a time, so density above 1/grain-size is dropped, and grains do not overlap within a lane. Plan 10 phase 3: a direct note-on allocates with the **absolute stream position** as `envStart` (the old default 0.0 put the attack ramp tens of thousands of samples behind the playhead, so ATTACK did nothing on direct notes), `releaseVoice` always sets `envEnd` (the old tape-only branch never did, so RELEASE was dead at the default Tape Stop Time), and the voice ends only when **both** the envelope and the ramp are finished (decision G2) — the tape tick must run before the envelope-zero checks, or a finished release starves the ramp (found by `testTapeStopReleaseFreesVoice`). Plan 10 phase 4: the pitch centre is the **Root Key** — `rate = 2^((key − rootKey)/12)` in `setupAllocatedVoice` (the old centre was a fixed middle C, so Root Key changed nothing), and `keyRegion` lost its never-read `rootKey` argument. The routing target table has 26 entries; `TimingRange` and `Chaos` (MicroTimingChaos leftovers) are not Sgraffito parameters and were removed before any release. Still not wired to the audio: `driftAmount`/`driftSpeed` (chaos drift), `swellTime`/`swellCrossfade` (Swell reverse playback runs, its own parameters do not yet), `glide`, and tape-stop **slowdown** (the level fade runs) — a follow-up plan owns them. Ghost notes, swing, humanise and sustain hold are wired through the pending/queue machinery.
- **Sample loading (LOAD SOUND button in the waveform preview)**: the button sits at the top-right *inside* the waveform view on the SOUND page (owner request 2026-11-16; it used to sit in the header left of the preset bar) and opens a `FileChooser` kept alive as an editor member (deleting the chooser aborts the dialog, so a stack-local one never shows it). It is a child of the waveform view, so it follows the page's visibility, and it stays clickable above the missing-sample overlay. File patterns are `*.wav;*.aiff;*.aif` only — `registerBasicFormats()` reads no FLAC. The chosen file loads asynchronously (plan 07 phase 4): the read runs on a pooled thread and the install happens on the message thread, so the UI never stalls on a big file; the current sample keeps playing until then. On a new load the previous `LoadedSample` moves into `retiredSamples`, a **capped two-generation retirement queue** (plan 09 1.1), instead of being destroyed: an audio block that started before the load may still hold the old buffer pointer, and two installs inside one running block (rapid preset/sample switching) would have freed it under the old single `retiredSample` slot. Both retire sites (`finishSampleLoad`, `clearSample`) route through `retireCurrentSample()`; the queue grows only on the message thread, and the audio thread never touches it.
- **Sample-linked presets and sessions (plan 07 phases 1-4)**: the loaded sample travels as the `samplePath` property on `<PARAMETERS>` (constants `samplePathProperty`, `embeddedSampleUri`), never as an APVTS parameter. `loadSample`/`loadEmbeddedSample` keep the property in sync on `apvts.state` at change time (the lerpCurve pattern), so the bar's SAVE path and `SuiteState::write`'s `extra` callback pick it up. `applySampleFromState` implements the rule: an **empty** tree is INIT (the bar and host program 0 apply hand-made empty trees) → reset to `embedded:grain_4k.wav`; an `embedded:` URI → `loadEmbeddedSample`; an existing file → `loadSample`; a missing path → the same file name in the fallback folder (`userSampleFolder()` = `userDocuments/AudioSuite/Sgraffito/Samples`, Q5; tests override it through the public `sampleSearchFolder` member), and if that fails too, keep the current sample and set `sampleMissing` + `missingSamplePath`; no property → no-op (delta semantics). The fallback load updates the saved path, so the session remembers the resolution; there is no automatic `embedded:` substitution. The waveform view draws a "Sample missing - click to locate" overlay (accent border + text, rule R6) over the still playing sample; clicking it runs the editor's `browseForSample()` flow. `applyPresetState`'s `afterLoad` reads the **incoming tree**, because an empty INIT tree gains a version property during read. No parameter or `numControls` change, no state-version bump; a preset change to the sample marks the session dirty (intended). Factory presets may now carry an `embedded:` URI only — never a file path (Q3, revised in phase 4; the audit rule rejects file paths in the authoring data, XML and JSON). Since plan 10 phase 7, every Sgraffito factory preset carries its own generated sample (see the Presets entry above; the five-preset `cloud_4k.wav` variant is retired), and `SgraffitoPresetTests`'s `embeddedSamples` check applies every URI in the bank (all 20) and asserts it actually loads (the guard against a resource-name typo, since `loadEmbeddedSample` silently keeps the current sample on an unknown name). Plan 07 phase 3 also added the host program virtuals Sgraffito was missing (the flag build's `runProgramList` exposed the gap): `getNumPrograms`/`getCurrentProgram`/`setCurrentProgram`/`getProgramName`/`getCurrentProgramStateInformation`/`setCurrentProgramStateInformation` are now out-of-line in `PluginProcessor.cpp` under `#if SUITE_PRESET_BANK_AS_PROGRAMS`, mirroring SpectralDrift, with a guarded `#include "PresetBank.h"` and the `currentProgramIndex` member reset to −1 in `applyPresetState` (on `LoadResult::loaded`) and in `setCurrentProgramStateInformation`. Phase 4 made **file** sample loads asynchronous: `loadSampleAsync` runs the read + resample on a `ThreadPool` member (`sampleLoadPool`, declared last so it is destroyed first and waits) and installs on the message thread via `finishSampleLoad` — the one install path (retire pattern, atomic swap, property sync), shared with the sync loaders. A `loadGeneration` counter makes the latest request win (every request bumps it, out-of-order installs are discarded), and a `LoadLifetime` token (shared_ptr + atomic alive flag, set dead first thing in the destructor) makes a queued handoff lambda a no-op after destruction. `hasPendingSampleLoad()` reports outstanding handoffs; tests pump the message loop until it clears. The existence/fallback/missing resolution stays synchronous, so a missing sample is still reported immediately; embedded URIs and INIT stay synchronous. `SgraffitoTests.exe <dir>` records a state fixture (`sgraffito_v1.xml` + `.values.txt`) for a future version bump; one exists under `build/fixtures/plan07/`.
- **Voice release and Sustain Hold routing**: a released voice now deactivates when its release envelope reaches zero (`sampleIndex >= envEnd + releaseSamples`), instead of staying active and holding its pool slot until the region end (many seconds with the default 25 % region). Tape-stop voices have no envelope end and keep their own completion path. `SustainHold` is now in `modulationTargets[]` (after `Humanise`, so table position matches enum value 15; `wholeNumber` on, because it is a switch). Every target from Ghost Threshold onward previously indexed the wrong row, so those routings moved the wrong parameter. No migration: there is no public release. `SgraffitoTests` grew to cover both.

All code changes are uncommitted and ready for owner review.

- Build: `cmake -S . -B build -A x64`, then `cmake --build build --config Release`.
- Default build: `cmake -S . -B build -A x64 -DSUITE_PRESET_BANK=OFF` → 20 CTest entries.
- Bank build: `cmake -S . -B build -A x64 -DSUITE_PRESET_BANK=ON` → 28 CTest entries.
- Bank + programs: add `-DSUITE_PRESET_BANK_AS_PROGRAMS=ON` → still 28 entries, program-list virtuals exercised inside the plugin preset tests.
- Test: `ctest --test-dir build -C Release --output-on-failure`.
- Bundles: `build/plugins/<Name>/<Name>_artefacts/Release/VST3/`.
- Release: GitHub Actions → *Release* → *Run workflow* (version without `v`, draft on by default).

---

## 14. Working notes for coding agents

Each item is a mistake or a slow path hit during the framework work (2026-11-08). Read them before you start, and add to the list when you hit something new.

**Stay inside the project.** Build output goes in `build/`, logs in `build/logs/`, baselines in `build/baselines/`. Do not write to `%TEMP%`, the home folder or a sibling folder. The owner asked for this. A baseline once sat in `Documents\PROJECTS\baselines` and had to be moved back.

**Use absolute paths.** The shell's working directory persists between calls and can change under you. One search ran in `docs/plans` and missed `AGENTS.md`.

**Shells.** `bash` in PowerShell starts WSL on this machine, not Git Bash. Use the Bash tool for `git archive`, `sed` and `grep`.

**Processes.** Do not stop CMake or any other process. The IDE may own it. If a command is refused, do not look for another route to the same effect. Report it.

**Encoding.** `Set-Content` writes ANSI and corrupts UTF-8 text. Edit with the Edit tool, or write bytes and keep the BOM and line endings. After an edit, `git diff --stat` should list only the lines you meant to change. A whole-file diff means line endings changed.

**Plans.** `docs/plans/` is tracked, so a deleted plan is recoverable from git history. Finished plans are deleted as soon as they are verified (see `docs/plans/README.md`); do not keep a done plan as an archive. Show the owner before deleting a plan that is not finished.

**Build facts.** JUCE 9 does not write `JucePluginDefines.h`. The same data is in `<Name>_artefacts/JuceLibraryCode/Release/Defs.txt` and `Info.txt`. The root project is `VERSION 1.0.0`, so the plugin version is 1.0.0 even without a `VERSION` argument.

**Deprecations.** Count them from the build log after the changed files rebuild. `juce::Font (x, flags)` is deprecated. The replacement is `juce::Font (juce::FontOptions (x, flags))`. Do not silence a warning with a pragma.

**Asserts.** `tests/common/PluginTestUtils.h` undefines `NDEBUG` before `<cassert>`, so plugin asserts also run in Release. Do not add a second check mechanism.

**Plans can be wrong.** Check each claim against the code before you build on it. Two examples from the framework work: a plan said `IS_SYNTH FALSE` for every plugin, but MicroTimingChaos is `TRUE`; and a plan said "six JUCE modules" where the plugins link five plus `SuiteState`.

**Links drift.** Grep the repo for a file name before you rely on a link. Renamed plans are listed in `docs/plans/README.md`.

**The brand generator.** `tools/brand/build.py` (mark, lockups, README banner) and `tools/brand/lockups.py` (per-plugin lockups) are the owner's local tools. `.gitignore` excludes `tools/brand/`, so the scripts are not in this repository; the artwork they produce is. Do not assume the scripts exist in a fresh clone, and do not commit them without the owner's decision. Regenerating a lockup is an owner task.

**Throwaway builds.** When a test needs a root change, such as a new plugin, build in a folder under `build/`, then remove the root line and the folder. Check `git status` at the end.

**Verification.** Run the full CTest set (20 entries default build, 28 with the bank on) before you say a change works. A change in the entry count is a change in the test set, so report it. Say which checks you could not run, such as macOS or a desktop-only editor test.

**Stop and report** when a step needs the owner: a commit, a push, a macOS machine, an external search, a licence decision, or a deletion of an untracked file.

## 15. Post-change checks

Run these after every change, before you report it as working. Each one was missed at least once in the framework work (2026-11-08), and each miss hid a real defect. Run them in order.

**1. Clean build, then count.** Run `--target clean`, then a full Release build. An incremental build compiles only the files you touched, so warnings in untouched targets stay hidden. Incremental builds reported no warnings. The clean build showed four `getenv` deprecations in test code, and two more in files that had not changed.

```powershell
$CM = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $CM --build "C:\Users\tfroe\Documents\PROJECTS\AudioPluginSuite\build" --config Release --target clean
& $CM --build "C:\Users\tfroe\Documents\PROJECTS\AudioPluginSuite\build" --config Release --parallel 8 > "C:\Users\tfroe\Documents\PROJECTS\AudioPluginSuite\build\logs\check-build.log" 2>&1
Select-String -Path "C:\Users\tfroe\Documents\PROJECTS\AudioPluginSuite\build\logs\check-build.log" -Pattern " error " | Measure-Object   # must be 0
Select-String -Path "C:\Users\tfroe\Documents\PROJECTS\AudioPluginSuite\build\logs\check-build.log" -Pattern "warning" | Measure-Object  # baseline: ~31 pre-existing C4996 deprecations from the shared router's AudioParameterFloat form; count and compare, 0 new
```

**2. CTest.** Run the full set (20 entries default build, 28 with the bank on), with `ctest.exe --test-dir build -C Release --output-on-failure`. A changed entry count is a changed test set. Say so.

**3. Look for escape damage.** A PowerShell replace once wrote the two characters backtick and `n` into C++ source in place of newlines. Search every changed file for them, and for duplicate definitions:

```powershell
Select-String -Path <changed files> -Pattern '`n' -SimpleMatch      # must print nothing
```

Then check each function you touched is defined once: `Select-String -Pattern 'Processor::<name>\('` should return one definition.

**4. Diff shape.** Run `git diff --stat`. A file listed with far more lines than you edited means line endings or encoding changed. A `CRLF will be replaced by LF` warning on a file you did not touch means the file mixes endings; leave it alone and mention it.

**5. Declarations match.** Check each `override` against a base virtual with the same signature. Check that no abstract class is used as a data member: a pure-virtual interface can only be a base, or be held by pointer or reference. An abstract class in a member declaration fails only at the build, after the edit looks finished.

**6. Includes resolve.** Every `#include` you added must be on an include path. Check that the CMake function that adds the include directory is actually called for that target. The presets header was not found because its CMake function existed but was never called.

**7. Renders.** Any change to a look and feel, a layout or a colour needs a before and after render of every affected editor, compared byte for byte, and both renders stored under `build/`. No render means no claim of "no visible change". Plan 02 requires this before its own phase 2.

**8. Plan claims.** Before you mark a plan step done, grep for the thing it says was removed or added. `MicroTimingChaosLookAndFeel.h` was still present after the plan said it was removed. A plan step is done when the code shows it, not when a status line says so.

**9. Documents.** After a rename or deletion, grep the repo for the old name, including `AGENTS.md`, `CLAUDE.md`, `README.md` and `docs/`. Plan links break silently.

**10. Edits to files.** Count matches before you replace, and stop if the count is not what you expect. Keep the BOM and line endings. Make a scripted edit idempotent: skip it if the change is already present, so a rerun after a partial failure does not double it. Do not use `Set-Content` on UTF-8 files.

**11. Untracked work.** Run `git status --short` at the end. Anything new and untracked must be deliberate: a source file to keep, a plan the owner expects, or a build output that `.gitignore` covers. Show the owner any untracked file you did not create.

**12. Report what you did not check.** List the checks you could not run here, such as macOS, the AU build, renders on a desktop session, or a DAW. Do not describe them as passing.

## Owner Rules

- **Build tooling** CMake is not on the bash PATH. Use the full path: C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe (and ctest.exe in the same folder). See section 3.

- **Files change on disk** The IDE and the owner edit files between reads. The harness reports changes; take the on-disk state as current and re-read before editing.

- **Git** Do not commit or push unless the owner asks.

- **Test expectations** Run the full CTest suite before reporting a change as working. Editor tests need a desktop session, so a headless run can fail for reasons that are not code bugs.

- **Prefer the existing helpers** tests/common/PluginTestUtils.h and the DSP tests already cover most patterns. Reuse them before writing new ones.

- **Refactoring and scalability** Always check whether there is something that could be refactored or simplified. If so, make a plan with detailed topics found. Always keep scalability in mind.
