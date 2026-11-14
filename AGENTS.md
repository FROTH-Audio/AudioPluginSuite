# AGENTS.md: Audio Plugin Suite

Knowledge base for coding agents working on this repository. Read this before changing anything. It records what is built, why it is built that way, what is verified and what is not, and the JUCE and CMake pitfalls already hit.

Keep this file in step with the code. When you change a parameter range, a default, a latency, a test count, or a file layout, update the matching section in the same change. The user-facing docs are `README.md`, `CHANGELOG.md` and `plugins/MicroTimingChaos/README.md`.

The original brief (`coding_agent_prompt.md`) is no longer in the repository. Section 12 lists where the built code differs from it.

---

## 1. What this project is

Three audio plugins written with JUCE 9.0.5 in C++17. Formats: VST3 on Windows and macOS, Audio Unit on macOS, and Standalone on both.

| Plugin | Folder | Kind | DSP core | Editor accent |
|---|---|---|---|---|
| Spectral Drift Shifter | `plugins/SpectralDrift` | audio effect | `CoreDSP::Spectral::DriftEngine` | cyan `#2DE2FF` |
| Dynamic Saturation Companion | `plugins/DynamicSaturation` | audio effect | `CoreDSP::Saturation` | amber `#FF9F1C` |
| Micro-Timing Chaos Sequencer | `plugins/MicroTimingChaos` | MIDI effect, with stereo pass-through | `CoreDSP::Chaos::LorenzAttractor`, `CoreDSP::Sequencer` | lime `#7DFF5A` |
| Sgraffito Memory Sampler | `plugins/Sgraffito` | Memory sampler (memory traces, tape-stop, granular cloud) | `CoreDSP::Sampler::DriftEngine`, `CoreDSP::Sampler::VoicePool`, `CoreDSP::Sampler::quantizeToScale` | slate `#9aa0a6` |

- Manufacturer name `FROTH Audio` (shown by hosts; set by `COMPANY_NAME` in `modules/suite_plugin/SuitePlugin.cmake`), manufacturer code `Asui`, bundle IDs `com.audiosuite.<name>` (kept: they are part of the plugin identity, section 7).
- Plugin code `Sgrf`. Codes and parameter IDs are saved in sessions

- **Minimum 15 presets**: Each new plugin must include at least 15 factory presets bundled with the plugin binary, exportable through the preset bank build. The preset bank (`SUITE_PRESET_BANK`) generates these from the schema defined in each plugin's processor. This ensures every plugin ships with a functional default set of presets immediately upon installation. and **must never change** after a release.
- Licence: GNU GPL v3 (`LICENSE`, the unmodified GNU text). JUCE is used under its GPLv3 option; distribution stays under the GPL.
- State versioning: saved sessions carry a `suiteStateVersion` attribute. States without the attribute are version 1. Newer states than the plugin are refused and leave the current state unchanged.

- **DSP engine** (Phase 4): Lorenz chaos drift, tape stop, ghost notes, scale lock, cloud granular mode. 71 parameters total (37 original + 34 new: 2 LFOs + 6 modulation slots + 21 creative parameters).
- **Editor** (Phase 5, Sgraffito): 880×700 px window, SOUND/TEXTURE/MOD pages, 60 Hz timer, slate accent colour `#9aa0a6`.
- **Modulation** (Phase 5): `SuiteModulation::Router` shared with other plugins, 2 LFOs, 6 routing slots (source/target/amount/delay). All parameters automatable, savable, restorable.
- **Presets**: 100 factory presets (26 SD / 26 DS / 28 MTC / 20 Sgraffito) behind the `SUITE_PRESET_BANK` flag (full bank accepted by owner decision, 2026-11-12; five Sgraffito presets added by plan 05.3 F1.3). Bank + programs (`SUITE_PRESET_BANK_AS_PROGRAMS`) maps Init=program 0, factory presets=programs 1..N.
- **Test results**: 15/15 CTest entries pass with the factory bank on (default build: CoreDSP, SuiteState, SuiteModulation, SuiteUI, the four plugin test executables, and the three VST3 host-load tests; bank build adds the preset bank tests).

---

## 2. Repository layout

```
CMakeLists.txt                    Root: JUCE pin, global definitions, options, SUITE_PLUGIN_FORMATS
AGENTS.md / CLAUDE.md             This file / Claude Code specifics (points here)
README.md, CHANGELOG.md, LICENSE  User and developer docs, history, licence
docs/plans/                       Ordered plans 00 to 06. Index and execution order: docs/plans/README.md
modules/
  core_dsp/                       Header-only DSP, depends on juce::juce_dsp
    include/CoreDSP.h             Umbrella header; decibelsToGain()
    include/SaturationEngine.h    Shapers, fold, OnePoleFilter, ToneControl, EnvelopeFollower
    include/ChaosEngine.h         LorenzAttractor (RK4), LorenzParams, LorenzState
    include/SpectralEngine.h      DriftEngine (overlap-add FFT); the drift is driven from the routed LFO's value
    include/SequencerEngine.h     CoreDSP::Sequencer: step rates, hashed pattern and melody, timing, swing, gate, velocity
    include/ModulationEngine.h    CoreDSP::Modulation: LFO shapes (cycles in, value in [-1, 1]) and the normalised offset clamp
    include/ModulationTiming.h    CoreDSP::Modulation: division table (beats per cycle), fractionalPart, cyclesFromSeconds
    tests/DSPTests.cpp            CoreDSPTests (35 test functions)
    CMakeLists.txt                CoreDSP INTERFACE target, CoreDSPTests
  suite_modulation/               Shared LFOs, routing slots, LFO scope, modulation panel, control display. NOT a library target, see section 8.
    include/SuiteModulationRouter.h   Router: LFO and slot parameters (addParameters), state, offsets, modulate(), publish. Also hostTempoOrDefault(), the tempo the effects' synced LFOs use
    include/SuiteModulationScope.h    LfoScope: one LFO cycle with the current position
    include/SuiteModulationPanel.h    ModulationPanel: LFO strips and the six routing rows (uses SuiteUI::LabelledSlider)
    include/SuiteModulationControls.h ControlDisplay: shows modulated values on sliders, skips the one under the mouse
    Source/*.cpp                  Implementations
    tests/SuiteModulationTests.cpp  SuiteModulationTests (9 test functions), with a stub processor
    suite_modulation.cmake        suite_modulation_add_to_target(<target>): adds sources + include dir. Needs suite_ui_add_to_target on the same target
  suite_ui/                       Shared UI. NOT a library target, see section 8
    include/SuiteLookAndFeel.h    SuiteUI::Colours: the neutral palette shared by all plugins, SuiteLookAndFeel (LookAndFeel_V4), and kRefreshHz (the 60 Hz editor timer rate)
    include/SuiteWidgets.h        SpectrumDisplay, TrailPlot, CurveView, LevelMeter, SegmentedSelector, LabelledSlider
    include/SuiteBrand.h          drawBrandLockup(): draws a brand lockup image at the left of a header area
    brand/                        Generated artwork: brand_mark.png and lockup_<plugin>.png. Made by tools/brand/build.py, do not edit by hand
    Source/*.cpp                  Implementations
    SuiteUI.cmake                 suite_ui_add_to_target(<target>): adds sources + include dir
    tests/SuiteUITests.cpp        SuiteUITests (9 test functions)
    CMakeLists.txt                SuiteUITests only
  suite_plugin/                   Build recipe. NOT a library target, see section 9
    SuitePlugin.cmake             suite_add_plugin(...): juce_add_plugin, sources, lockup, links, <Name>Tests and add_test from one call. Checks PLUGIN_CODE is unique
plugins/<Name>/
  CMakeLists.txt                  One suite_add_plugin(...) call (see section 9)
  Source/PluginProcessor.{h,cpp}  APVTS, processing, state, createPluginFilter()
  Source/PluginEditor.{h,cpp}     Editor, 60 Hz Timer reads processor snapshots
  tests/PluginTests.cpp           Plugin tests: SpectralDrift 22, DynamicSaturation 19, MicroTimingChaos 40
plugins/MicroTimingChaos/         Additional files:
  MicroTimingChaosLookAndFeel.h   At the plugin root (not include/). MTC-only colours (axes, LFOs, preview text); the neutrals come from SuiteUI::Colours. Derives from SuiteUI::SuiteLookAndFeel for the shared setup. It keeps its own knob, toggle, combo box, linear slider and fonts, so the window is unchanged (plan 02 results)
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
  skeleton/                       CMakeLists.txt, Source/Plugin{Processor,Editor}.{h,cpp}, tests/PluginTests.cpp, with __NAME__, __PRODUCT__ and __CODE__ placeholders
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

Last verified status (2026-11-12, plan 05.1 Phase 5): JUCE 9.0.5. Clean Release build: 0 errors, 90 warnings (all pre-existing `-Winconsistent-missing-override` on the preset-state overrides; none new since the plan 05.1 dead-constant cleanup). All CTest entries pass (15/15 with the factory bank on). All four plugin editor renders under `build/renders/ui-rework2/` green.

- Default build (`SUITE_PRESET_BANK=OFF`): 11 entries - CoreDSPTests, SuiteState_Tests, SuiteModulation_Tests, SuiteUI_Tests, the four plugin test executables, and the three VST3 host-load tests.
- Bank build (`SUITE_PRESET_BANK=ON`): 15 entries - the 11 public entries plus `SpectralDrift_PresetTests`, `DynamicSaturation_PresetTests`, `MicroTimingChaos_PresetTests`, and `Sgraffito_PresetTests`.
- Bank with host program list (`SUITE_PRESET_BANK_AS_PROGRAMS=ON`): still 15 entries; the program-list virtuals are exercised inside the plugin preset tests via `runProgramList`.

Before 2026-11-08 the list was 9 entries: CoreDSPTests, SuiteUI_Tests, SuiteModulation_Tests, the three plugin test executables, and the three VST3 host-load tests. SuiteUI_Tests also passed against JUCE 8.0.6, the previous pin, which is the version CI ran (last checked 2026-11-06, not re-run since). macOS is not verified on JUCE 9.

---

## 4. Parameters

Parameter IDs are `static constexpr const char*` members of each processor. Defaults and ranges below are what `createParameterLayout()` defines. Each float uses a `NormalisableRange`, and some use a log or skew. The plugin tests check these values with `checkRange`, so a change here must change the test too.

### 4.1 Spectral Drift Shifter (47 parameters)

The 13 controls below are the original sound parameters. The remaining 34 are the two LFOs and six modulation
routings added by `SuiteModulation::Router` (`lfo1Rate` … `mod6Delay`), appended last so the original order never
moves. At defaults every routing amount is 0, so the sound is bit-identical to the 13-parameter version.
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
| `outputGain` | Output Gain | −12 – +12 dB | 0 dB | Applied to the whole output after the engines |
| `mix` | Mix | 0 – 100 % | 100 % | Dry is delayed by the frame latency |
| `freeze` | Freeze | bool | off | Holds captured magnitudes, phases still advance |

With plan 05.1 pass 2 the drift is driven by the routed LFOs: a new **Drift** routing target
(append-only, index 12 in the target list) receives the routed LFO's value (−1 … +1 around no
drift), which the depth, spread and stereo-phase parameters scale and spread across the bins.
With no routing assigned there is no drift. The old `driftRateHz` internal oscillator and the
`lfoShape` parameter are gone; the engine's per-bin drift reads a per-frame history of the
routed value, bin k lagging k × `binSpread` frames behind.

Latency reported: 2048 samples (`setLatencySamples`). Tail length: 0.

### 4.2 Dynamic Saturation Companion (47 parameters)

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
| `outputLevel` | Output Level | −12 – +6 dB | 0 dB | Smoothed 50 ms |
| `attack` | Attack | 1 – 100 ms, skew 0.35 | 5 ms | Envelope rise |
| `release` | Release | 50 – 2000 ms, skew 0.4 | 150 ms | Envelope fall |
| `detectHpf` | Detect HPF | 20 – 800 Hz, log | 20 Hz | High-pass on the detector only |
| `stereoLink` | Stereo Link | 0 – 100 % | 100 % | 100 % = linked envelope (max of channels) |
| `bias` | Tube Bias | 0 – 0.25 | 0.2 | Asymmetry of the tube curve. Tube only |
| `fold` | Fold | 0 – 50 % | 0 % | Wavefolder after shaping, 2× domain |
| `tone` | Tone | −1 – +1 | 0 | Tilt around 1 kHz after the shaper |
| `curve` | Envelope Curve | 0.5 – 3.0 | 1.0 | Envelope exponent |

Latency reported: the oversampler's integer latency, computed in `prepareToPlay`. The dry path is delayed by the same amount. Tail length: 0.

### 4.3 Micro-Timing Chaos Sequencer (54 parameters)

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
| `lfo1Shape` / `lfo2Shape` | LFO Shape | Sine, Triangle, Saw, Square, Sample & Hold, Random, Noise | Sine | Index 0 to 6 |
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

`applyPresetState()` runs through `SuiteState::read` with `resetMissing=true`: the preset tree (a `<PARAMETERS>` element) is encoded to binary, then `SuiteState::read` does the version check, migrations, `replaceState` and reset-to-default for absent IDs. Nothing changes if the state is `notXml`, `wrongRoot` or `newerThanPlugin`. So a preset carries only the parameters it changes — deltas that materialise into full state at load time.

The factory bank (`modules/suite_presets/factory_bank/`, gated by `SUITE_PRESET_BANK`) adds:

- `PresetBank`: parses the embedded factory bank (`bank/AudioSuitePresets.xml`, one `AudioSuitePreset` per preset), provides `getFactoryPresets()` / `getNumFactoryPresets()` / `getFactoryPreset(i)`, plus `parseDelta()` which merges a JSON delta onto the default state.
- `PresetFile`: `.ausp` read/write (`<AudioSuitePreset formatVersion="1" plugin="..." pluginVersion="...">`, `<Meta>`, `<PARAMETERS suiteStateVersion="N">`), user presets stored under `userDocuments/AudioSuite/<plugin>/Presets`, sanitised file names, version check at read time.
- `PresetBar`: header-bar UI (factory presets, INIT, SAVE, menu), `selectPreset(i)`, `getNumFactoryPresets()`.
- `PresetMenuModel`: builds a `juce::PopupMenu` with Factory/User submenus grouped by category.
- `PresetPrograms` (behind the separate `SUITE_PRESET_BANK_AS_PROGRAMS` flag): host program list where program 0 is Init and programs 1..N are the factory presets in bank order. Bank order is append-only once hosts cache indices.

Factory presets are authored as JSON deltas in `tools/preset_data.py` (26 SD / 26 DS / 28 MTC / 20 Sgraffito = 100) and generated into `modules/suite_presets/factory_bank/bank/` by `tools/build_bank.py`. `tools/audit_bank.py` enforces the rules: unique names per plugin, ≥3 categories, every value inside the parameter's `NormalisableRange`, no preset equal to the default state, SD `freeze` only in `Freeze`, DS `drive` 24+`fold` 50 only in `Extreme`, MTC host `sync` off and `bpm` 120 in every factory preset, the base pattern must fire at least one step, and — since plan 05.2 — the authoring schema must cover every parameter the plugins expose (live IDs parsed from the sources, the shared Router's parameters included). Plan 05.3 F1.3 resolved the coverage findings: parameters no factory preset moves are now a recorded decision (levels/gains are never preset-authored; the structural LFO/routing slots stay at defaults), and Sgraffito gained five new presets.

`SUITE_PRESET_BANK_AS_PROGRAMS=OFF` (default build): plugins report `getNumPrograms() == 1`, `getProgramName() == ""`, exactly as before the preset system. With both flags on, the program virtuals map to `applyPresetState` / `getStateInformation`, and `currentProgramIndex` tracks the last applied program (-1 after any preset/state change that is not a factory program).

The preset system adds no APVTS parameters; `numControls` and `checkParameterIds` are unchanged (48 / 47 / 54).

MTC's `lerpCurve` state property (not a parameter) round-trips through presets and factory bank XML via the `lerpCurve` attribute on `<PARAMETERS>`; `applyPresetState()` reloads it through `loadLerpCurveFromState()`.

---

## 6. Test architecture

Eleven CTest entries run eight executables: `CoreDSPTests` (35 test functions), `SuiteUI_Tests` (`SuiteUITests`, 9), `SuiteModulation_Tests` (`SuiteModulationTests`, 9), `SpectralDriftTests` (26), `DynamicSaturationTests` (24), `MicroTimingChaosTests` (45), `SgraffitoTests` (17), and `PluginHostTests`, which has one ctest entry per plugin (3). With the factory bank on, four preset-test executables join them (one per plugin). The README has the full list.

**Module tests** (`modules/suite_modulation/tests/SuiteModulationTests.cpp`): the router, with a stub processor (no plugin). They cover the routed LFO, the zero case (bit for bit), bipolar amounts, whole-number targets, Host Sync ignoring Tempo, the one-bar period, sample and hold, out-of-range indices, and the control display (values set without notifying). Not covered: the control display skipping a slider the mouse holds, since the test cannot press a mouse button. The panel and scope are checked only by the MicroTimingChaos editor test.

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
- **Effects use a SOUND/MOD page switch**, not a permanent panel. Both show the visualiser on both pages and the `ModulationPanel` on the MOD page. `setPage` only touches visibility, so switching pages never changes a parameter. Since plan 05 the windows are SD 720×660 and DS 780×680 (plan 05.1 added the pan control and the stereo output bar to DS).
- **Synced LFOs in the effects use a tempo-locked beat clock that follows the host tempo** (120 BPM when there is no playhead), not the host PPQ. The clock only accumulates, so it never jumps on loop or relocate. The routing Delay clock runs from `prepareToPlay` and is not restarted on transport start.
- **Spectral Drift smooths mix, feedback, feedback damping and output gain per sample** (20 ms). These four are consumed per sample in the engine, so block-rate modulation would zip or click; smoothing removes it. The smoothers are advanced once per block into per-sample curves (`mixCurve` and so on), and both channels read the same curves. Advancing them inside the channel loop made the right channel use the next block's ramp, which skewed the stereo image during ramps. `testOutputGainRampMatchesChannels` guards this. Everything else in SD is frame- or block-rate. DS needed no DSP change: it already smooths drive/mix/output level and its other targets are block-rate by construction.
- **The engine's `processSample` keeps its single-argument behaviour bit-identical.** A two-argument overload takes `PerSampleSettings` (mix, feedback, damping); the old form forwards the raw settings, so all existing CoreDSP tests and the reconstruction test are unchanged.
- **The LFO panel and the window height.** Since plan 05 the LFO panel lives on the MOD page of the new page switch, and the window is 980×700 at native scale — the ×2/3 `setScaleFactor` override is gone (owner decision, 2026-11-11). The editor has three pages: SEQUENCE (pattern + step grid), DRIFT (the lerp group fills the left column at full height: curve or preview spans the group width with the four lerp knobs below it; the attractor and the chaos group share the right column) and MOD (the LFO and routing panel alone, since plan 05.1 moved melody and transport to SEQUENCE). The preview's three-colour legend is gone; the lerp multiplier captions carry the axis meaning ("TIMING (X)" and so on). `setPage` toggles component visibility and respects the lerp view state, so the first DRIFT open shows the curve (plan 05.1 bug fix); `paint` skips groups on other pages.
- **Suite UI rules (plan 05, amended by plan 05.1 on 2026-11-12).** One mandatory control language, enforced in `modules/suite_ui` so editors cannot regress: **R1** every rotary knob is a `SuiteUI::LabelledSlider`; the widget sizes the knob to `SuiteUI::knobDiameter` (40 px since plan 05.1 — "2/3 again"; the LED-ring drawing itself is frozen) centred in its cell — no editor caps, scales or draws knobs itself. **R2** caption above, value box below, both centred. **R3** `ToggleButton` and `SegmentedSelector` render at `SuiteUI::pillHeight` (36 px), corner radius `SuiteUI::controlRadius` (6 px); layouts centre them, they never fill a row. **R4** an editor's default window is at most 1000×700; a page that cannot fit gets a second page, not a bigger window. **R5** type: group titles 15 pt bold in the accent, knob captions 13 pt bold `textDim`, value text 14 pt `text`, help/footer 11 pt; one font family — `SuiteUI::uiFont` uses Bahnschrift (the brand font), with JUCE's default fallback where it is missing (macOS). **R6** switches read as state without colour alone: a filled state dot when on, hollow when off, accent border and text when on. **R7** one `groupGap` (8 px) everywhere, group title band `SuiteUI::groupTitleHeight` (20 px). **R8** page switches use the shared `SegmentedSelector` at the top right of the header. **R9** every plugin's test executable writes per-page renders when `SUITE_RENDER_DIR` is set; a UI change ships with before/after renders under `build/renders/`. **R10** future plugins inherit all of this through `tools/new_plugin/skeleton`. The factory preset bar sits flat in the header (no fill or border, plan 05.1).
- **Interface typography.** Group titles are 15 pt bold in the accent colour, for every group. Knob captions are 13 pt bold, and value text 14 pt (look and feel `getLabelFont`). The root note name is part of the parameter text (`C2 (48)`), so the separate note label is gone; Sgraffito's Root Key does the same (`C3 (60)`).
- **Pattern display.** The sequencer view computes the pattern with the modulated Seed, Density and Timing Range, and `CoreDSP::Sequencer::densityCurve` (shared with the processor). Keep the two in step if either changes.
- **LFO shapes.** Random has eight grid points per cycle, each with its own random level, joined by straight lines. Noise holds an uncorrelated random level on each of 32 points per cycle, with no glide. Sample and hold is one level per cycle. Neither Random nor Noise repeats from cycle to cycle. The scope draws Sample and hold, Random and Noise as a scrolling history of the output (128 timer ticks), because a single cycle of them shows nothing. The DSP tests check Random for continuity, Noise for uncorrelated neighbouring levels, and both for non-repetition.
- **Live control displays.** Each routable control shows `getModulatedValue()`: its base value moved by the offset published for its target. Offsets are published once per block, so displays follow LFOs in real time. Host automation shows through the base value. While the mouse is held on a control, the display is the user's value, and the modulation shows again on release. Tempo is shown unmodulated while Host Sync is on.
- **Editor sizing (plan 05, amended by plan 05.1 on 2026-11-12).** The window is 980×700 at native scale, resizable from 940×660. The layout has three pages (see the LFO panel note above), one `groupGap` of 8 px separates every pair of groups horizontally and vertically, and sets the outer margin. On the SEQUENCE page the pattern is a 3×2 knob grid across the top, MELODY and TRANSPORT share the next row (TRANSPORT stacks its rate switches, BPM Sync and the host tempo readout), and the step grid takes the remaining height. On the DRIFT page the lerp group (58 % of the width, with its four knobs on the right) shares a row with the attractor, chaos spans the full width below. On the MOD page the modulation panel takes the whole page. Knobs are the suite's 40 px since plan 05.1; the editor holds no size constants of its own beyond the page arrangement.
- **Spectral Drift preview:** the old panel was blank because, with no audio, the traces sat at the −120 dB floor. The fix adds a "Waiting for audio" state (shown until frames and input energy arrive) and frequency labels. A regression test checks that a 1 kHz tone reaches the snapshot at bin 43.

---

## 8. SuiteUI is compiled into each consumer

`SuiteUI` is **not** a library target. Any consumer calls `suite_ui_add_to_target(<target>)` from `modules/suite_ui/SuiteUI.cmake`, which adds the two `.cpp` files and the include directory.

Reason: a static library that links JUCE module targets bundles those module objects into the archive. Each consumer also links the JUCE modules itself, so the link fails with hundreds of duplicate symbols (`juce::Colour`, `JUCEApplicationBase`, `juce_audio_utils`, and so on). A plain `OBJECT` library had the same problem. Compiling the sources directly avoids it. Consumers still link their JUCE modules explicitly. The test executables link `CoreDSP` and `SuiteUI` but not extra JUCE modules, so they do not need them listed.

If you add a new consumer, use `suite_ui_add_to_target`. Do not add `SuiteUI` to `target_link_libraries`.

`suite_modulation` follows the same rule (`suite_modulation_add_to_target`, never `target_link_libraries`). Its panel uses `SuiteUI::LabelledSlider`, so a consumer calls `suite_ui_add_to_target` on the same target first. Its tests are registered in `modules/suite_modulation/CMakeLists.txt` only when `SUITE_BUILD_TESTS` is on, and the directory is added only inside the `SUITE_BUILD_PLUGINS` block.

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
3. The plugin's CMake is one `suite_add_plugin(...)` call (`modules/suite_plugin/SuitePlugin.cmake`). It sets the plugin target, its `<Name>Tests` executable, the links, the lockup and `add_test`. Its arguments are described at the top of that file. `DESCRIPTION` sets the bundle description (default: the product name). The company website and copyright are set once, in the root `CMakeLists.txt`. Add `MODULATION` for the LFOs and routing, `IS_SYNTH` / `IS_MIDI_EFFECT` / `NEEDS_MIDI_INPUT` / `NEEDS_MIDI_OUTPUT` for MIDI effects and instruments, and `LOCKUP` or `ASSETS` for embedded images and files.
4. Add `suite_add_host_test(<Name>)` to `tests/host/CMakeLists.txt`. Add its accent colour and lockup to `tools/brand/build.py` and regenerate the artwork. Add it to the README and the CHANGELOG.
5. The function checks that the plugin code is four characters and unique across all plugins. A duplicate stops configuration.

**Adding modulation to a plugin** (the procedure used for Spectral Drift and Dynamic Saturation)
1. In the plugin's CMake, pass `MODULATION` to `suite_add_plugin`. This adds `suite_ui_add_to_target` and `suite_modulation_add_to_target` to the plugin and its test executable.
2. Declare a `static constexpr SuiteModulation::TargetInfo` table of the controls the routing may move, in ModTarget order, with `wholeNumber` and `ignoredWhileHostSync` where they apply. Append only.
3. Add `SuiteModulation::Router::addParameters (params, names)` to `createParameterLayout()`, with `names` = `"None"` followed by the target names, in table order.
4. Construct the router in the processor after `apvts`, with the table and the host sync ID (or `nullptr`). Call `prepare`, `computeOffsets`, `publish` and `advanceBlock` as MicroTimingChaos does. Read each routed control through `modulate`.
5. The editor builds `SuiteModulation::ModulationPanel` and `ControlDisplay`. Add the plugin's `numControls` and the parameter tests (`checkParameterIds`, `checkRange`).
6. Record a probe of the unmodulated output before the change, and diff it after. Document the synced-LFO fallback for a plugin with no beat.

## 10. Open items and known risks

- **MicroTiming DAW load:** the reported failure is not reproduced. JUCE's VST3 host loads and opens it, both before and after the stereo bus change. The remedy applied (stereo pass-through) is a best guess. If it still fails in a DAW, ask for the DAW name, its scanner log, and the exact error.
- **Presets (Phase 7 done):** default build (`SUITE_PRESET_BANK=OFF`) is visually and behaviourally unchanged — 10 CTest entries, all green. Bank build (`SUITE_PRESET_BANK=ON`) adds 4 CTest entries, all green: `SuitePresetsFile_Tests` plus one `*_PresetTests` per plugin. `SUITE_PRESET_BANK_AS_PROGRAMS` adds the host-program virtuals with no entry-count change.
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

## 13. Quick reference

## 14. Sgraffito Plugin Development Complete

Development of the Sgraffito memory sampler plugin is complete across all phases:

- **Phase 4 - Creative Features**: Lorenz chaos drift, tape stop, ghost notes, scale lock, cloud granular mode. 71 parameters total (37 original + 34 new: 2 LFOs + 6 modulation slots + 21 creative parameters).
- **Phase 5 - Modulation Infrastructure**: SuiteModulation::Router shared with other plugins, 2 LFOs, 6 routing slots (source/target/amount/delay). All parameters automatable, savable, restorable.
- **Editor**: 880×700 window (plan 05 budget; plan 05.1 added the MOD page and the playable keyboard), SOUND/TEXTURE/MOD pages, brand lockup in the header (`lockup_sgraffito.png`, embedded via LOCKUP; generated by `build/brand/lockup_sgraffito.py` in the style of the other lockups) and the factory preset bar (added to the component tree in every build). Knobs come from the shared `SuiteUI` tokens (40 px, plan 05.1); toggle buttons and selectors are 36 px pills. The SOUND page shows the waveform view (region overlay, Chop slice lines) and a Root Key value box that reads as a note name and number (`C3 (60)`); the TEXTURE page groups the controls into framed rows (TAPE & MEMORY, DRIFT & TIMING, GHOST, CLOUD, SWELL & SURFACE); the MOD page holds only the `ModulationPanel` (both LFOs in the slate accent). The editor has no help/status line. A `PlayableKeyboardStrip` (MIDI 24–96) sits at the very bottom of every page and plays through an SPSC note queue drained in `processBlock`. The waveform's region maths mirrors `CoreDSP::Sampler::keyRegion`/`chopRegion`.
- **Presets**: Sgraffito ships 20 factory presets (the 15-preset minimum plus five added by plan 05.3 F1.3); the suite-wide bank is 100 factory presets (26 SD / 26 DS / 28 MTC / 20 Sgraffito) behind SUITE_PRESET_BANK flag. CI and the release workflow configure with `-DSUITE_PRESET_BANK=ON`, so published binaries carry the presets.
- **Documentation**: AGENTS.md updated with Sgraffito plugin entry, parameter tables, and minimum 15 presets requirement.
- **Test results**: 11 CTest entries with `SUITE_PRESET_BANK=OFF`, 15 with `=ON`. The 4 plugin test executables run in both; the bank build adds 4 preset-test executables. Sgraffito's plugin tests cover 17 functions: parameters, state, note-on/off voice handling, region mapping, Chop slices, output, the editor paint, the preset bar in the component tree, per-page knob visibility (a knob missing from a page's visibility list floats over every page), editor renders (3 pages), the shared routing, and the plan 05.1 sound regressions (the four modes produce different audio; the key changes the pitch).
- **Audio playback (per-sample loop)**: `processBlock` reads and writes audio sample by sample. The envelope (`Voice::envelope`) runs per sample; a released voice deactivates when its release envelope reaches zero. Scrub and Chop advance `voice.position` by `rate` frames per output sample and end as one-shots at the region end; Swell starts at the region end and reads backwards; Cloud runs a per-lane grain machine (`cloudGrainPosition`, half-sine window, `grainSize`/`grainDensity`/`grainSpread`, wrap within the region). A mono sample (the embedded `grain_4k.wav` is mono) feeds every output channel — the per-channel read must use `ch % sampleChannels`, or a stereo output crashes on `getReadPointer`. The routing target table has 26 entries; `TimingRange` and `Chaos` (MicroTimingChaos leftovers) are not Sgraffito parameters and were removed before any release. Still not wired to the audio: `ghostOn/threshold/level/delay`, `dust/crackle/wow/flutter`, `swellTime/swellCrossfade` (Swell reverse playback runs, its own parameters do not yet), chaos drift, tape-stop slowdown, `swing` and `humanise` — a follow-up plan owns them.
- **Voice release and Sustain Hold routing**: a released voice now deactivates when its release envelope reaches zero (`sampleIndex >= envEnd + releaseSamples`), instead of staying active and holding its pool slot until the region end (many seconds with the default 25 % region). Tape-stop voices have no envelope end and keep their own completion path. `SustainHold` is now in `modulationTargets[]` (after `Humanise`, so table position matches enum value 15; `wholeNumber` on, because it is a switch). Every target from Ghost Threshold onward previously indexed the wrong row, so those routings moved the wrong parameter. No migration: there is no public release. `SgraffitoTests` grew to cover both.

All code changes are uncommitted and ready for owner review.

- Build: `cmake -S . -B build -A x64`, then `cmake --build build --config Release`.
- Default build: `cmake -S . -B build -A x64 -DSUITE_PRESET_BANK=OFF` → 10 CTest entries.
- Bank build: `cmake -S . -B build -A x64 -DSUITE_PRESET_BANK=ON` → 14 CTest entries.
- Bank + programs: add `-DSUITE_PRESET_BANK_AS_PROGRAMS=ON` → still 14 entries, program-list virtuals exercised inside the plugin preset tests.
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

**Untracked files.** `docs/plans/` is untracked, so git cannot restore a file deleted there. Show the owner before you delete one.

**Build facts.** JUCE 9 does not write `JucePluginDefines.h`. The same data is in `<Name>_artefacts/JuceLibraryCode/Release/Defs.txt` and `Info.txt`. The root project is `VERSION 1.0.0`, so the plugin version is 1.0.0 even without a `VERSION` argument.

**Deprecations.** Count them from the build log after the changed files rebuild. `juce::Font (x, flags)` is deprecated. The replacement is `juce::Font (juce::FontOptions (x, flags))`. Do not silence a warning with a pragma.

**Asserts.** `tests/common/PluginTestUtils.h` undefines `NDEBUG` before `<cassert>`, so plugin asserts also run in Release. Do not add a second check mechanism.

**Plans can be wrong.** Check each claim against the code before you build on it. Two examples from the framework work: a plan said `IS_SYNTH FALSE` for every plugin, but MicroTimingChaos is `TRUE`; and a plan said "six JUCE modules" where the plugins link five plus `SuiteState`.

**Links drift.** Grep the repo for a file name before you rely on a link. Renamed plans are listed in `docs/plans/README.md`.

**The brand generator.** It is `tools/brand/build.py`. `.gitignore` excludes `tools/brand/`, so the script is not in git. Do not assume it is tracked, and do not commit it without the owner's decision.

**Throwaway builds.** When a test needs a root change, such as a new plugin, build in a folder under `build/`, then remove the root line and the folder. Check `git status` at the end.

**Verification.** Run the full CTest set (11 entries default build, 15 with the bank on) before you say a change works. A change in the entry count is a change in the test set, so report it. Say which checks you could not run, such as macOS or a desktop-only editor test.

**Stop and report** when a step needs the owner: a commit, a push, a macOS machine, an external search, a licence decision, or a deletion of an untracked file.

## 15. Post-change checks

Run these after every change, before you report it as working. Each one was missed at least once in the framework work (2026-11-08), and each miss hid a real defect. Run them in order.

**1. Clean build, then count.** Run `--target clean`, then a full Release build. An incremental build compiles only the files you touched, so warnings in untouched targets stay hidden. Incremental builds reported no warnings. The clean build showed four `getenv` deprecations in test code, and two more in files that had not changed.

```powershell
$CM = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $CM --build "C:\Users\tfroe\Documents\PROJECTS\AudioPluginSuite\build" --config Release --target clean
& $CM --build "C:\Users\tfroe\Documents\PROJECTS\AudioPluginSuite\build" --config Release --parallel 8 > "C:\Users\tfroe\Documents\PROJECTS\AudioPluginSuite\build\logs\check-build.log" 2>&1
Select-String -Path "C:\Users\tfroe\Documents\PROJECTS\AudioPluginSuite\build\logs\check-build.log" -Pattern " error " | Measure-Object   # must be 0
Select-String -Path "C:\Users\tfroe\Documents\PROJECTS\AudioPluginSuite\build\logs\check-build.log" -Pattern "warning" | Measure-Object  # baseline: ~90 pre-existing -Winconsistent-missing-override warnings on the preset-state overrides; count and compare, 0 new
```

**2. CTest.** Run the full set (11 entries default build, 15 with the bank on), with `ctest.exe --test-dir build -C Release --output-on-failure`. A changed entry count is a changed test set. Say so.

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
