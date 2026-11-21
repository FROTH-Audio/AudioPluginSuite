# Plan 15: DAW-test findings 3 — StereoTool compact top visual, MultiSat frequency grid, MidiTool slow rates

Written for: the coding agents who carry out the phases. Read `AGENTS.md` first. Source of
record: `owner_findings.md` (round 3, dictated 2026-11-21).

Status: implemented and verified 2026-11-22 (Phases 1-4; clean Release build 0 errors, 46
warnings all pre-existing C4996; CTest 28/28 with the bank on; 200 presets audited; renders
under `build/renders/plan15-*`).

---

## 1. Findings and decisions

| Plugin | Finding | Decision |
|---|---|---|
| StereoTool | "Visualizer: move to window top; set default height to minimum height; remove the input and output meters for this plugin" | Row order becomes preview / LEVEL+IMAGE / CHARACTER. The `SuiteUI::StereoLevelMeter` pair goes away here (the widget stays in `suite_ui` for future plugins; the placement rule is unchanged). The default window height equals the minimum resize height — the preview keeps a fixed sensible minimum (100 px) and grows when the window is enlarged |
| MultiSat | "Remove the input and output meters; draw a frequency grid in the visualizer/preview" | Same meter removal. The frequency grid is vertical dim lines through the band area at the existing log tick frequencies (80/100/200/500/1k/2k/5k/8k), drawn under the bars; the Hz axis and the crossover marker stay. The dB scale stays as a narrow left tick strip |
| MidiTool | "Step Rate entries from 1/32 to 4 Bars" | **Append-only**: `1/2`, `1 Bar`, `2 Bars`, `4 Bars` after `1/32D` (12 → 16 entries). Saved indices and factory presets stay valid, so **no state-version bump and no migration**. `CoreDSP::Chords::arpStepBeats` gains 2, 4, 8 and 16 beats |

The T/D variants for 1/2 are not added (the owner asked for the range 1/32 … 4 Bars; `1/2`
fills the 1/4 → 1 Bar gap). If the owner wants `1/2T` / `1/2D`, they append after `1/2`
(append-only still holds).

## 2. Phases

### Phase 4 — Per-mode control affinity everywhere
Files: the DS, StereoTool and MidiEffect editors, their tests. Sgraffito (`updateModeAffinity`),
Cyclical (`updateAffinity`), MTC (Tempo while synced) and the shared `ModulationPanel`
(rate/division per sync) already implement the rule; MultiSat has no mode-only control.

The suite rule (plan 10 G1): a control that only applies in one mode is **dimmed, never
hidden** (`setEnabled`), and the state follows the parameter live (the editors' 60 Hz timers
re-evaluate, like Cyclical's `updateAffinity`).

1. **DynamicSaturation**: `bias` is Tube-only — dimmed while the mode is not Tube.
2. **StereoTool**: `sideCrossover` dimmed while Side Filter is Off; transient **Attack,
   Release and Target** dimmed while Amount is 0 (the follower is bypassed entirely).
3. **MidiEffect**: in Chord mode the arp-only controls dim — **Step Rate, Host Sync, Tempo,
   Gate Length, Velocity Curve, Velocity Direction, Octave Spread** (Chord is event-for-event;
   it has no clock and no per-step shape).
4. Tests per plugin: the mode switch changes the enabled state of exactly the affected
   controls (and none of the others).

### Phase 1 — StereoTool
Files: `plugins/StereoTool/Source/PluginEditor.{h,cpp}`, `PluginProcessor.{h,cpp}`,
`tests/PluginTests.cpp`, manual + renders.

1. `layoutSoundPage`: preview first (full width), then LEVEL+IMAGE, then CHARACTER. The
   preview's height is `area - two groups - gaps`, floored at 100 px.
2. `editorHeight` 700 → **658** (the layout sums to the window exactly: 100 px preview floor +
   two 220 px groups + the header, footer and gaps); `setResizeLimits` minimum height = 658
   (default = minimum). Width stays 720.
3. `MidSideMeter`: drop `inputMeter` / `outputMeter`, `stereoMeterColumn`, the `setLevels`
   calls and the column trims in `paint` / `resized`.
4. Processor: drop `MeterSnapshot::inputPeakL/R` / `outputPeakL/R` and their computation
   (they fed only the meters). Mid/side levels and the follower envelope stay.
5. Tests: the layout test asserts the visualizer sits above the LEVEL group and that **no**
   `StereoLevelMeter` exists in the editor; the per-channel peak asserts go. R9 renders before
   and after under `build/renders/plan15-stereotool/`.

### Phase 2 — MultiSat
Files: `plugins/MultibandSaturation/Source/PluginEditor.{h,cpp}`,
`PluginProcessor.{h,cpp}`, `tests/PluginTests.cpp`, manual + renders.

1. `DualBandVisualizer`: drop both meters and the meter columns; the dB tick strip stays at
   the left edge. `resized()` has nothing left to do and is removed.
1b. The **SOUND / MOD switch moves into the header row**, left of the preset bar (owner,
   2026-11-21) — the same R8 placement StereoTool already has. The 28 px row it occupied
   below the header goes back to the pages.
2. The paint draws the **frequency grid**: dim vertical lines through the bars area at
   80/100/200/500/1k/2k/5k/8k (the same log mapping the Hz axis uses), **and horizontal
   level lines at 0/−6/−12/−24/−60 dB** (owner follow-up: freq/level, lines both ways),
   drawn before the bars so the bars sit on top; the left dB dashes align with the level
   lines. Crossover marker and `<hz> Hz` text unchanged.
3. Processor: drop the four peak fields and their per-channel accumulation.
4. Tests: no `StereoLevelMeter` in the editor; the per-channel peak asserts (L-only sine)
   are replaced by a grid-paint check. R9 renders.

### Phase 3 — MidiTool step rates
Files: `modules/core_dsp/include/ChordEngine.h`, `plugins/MidiEffect/Source/PluginProcessor.{h,cpp}`,
`PluginEditor.cpp`, `tests/PluginTests.cpp`, `modules/core_dsp/tests/DSPTests.cpp`,
`tools/preset_data.py`.

1. `arpStepBeats`: 16 entries; the four new beat values 2.0 / 4.0 / 8.0 / 16.0; clamp 0..15.
2. Processor choice list and editor dropdown options gain `1/2`, `1 Bar`, `2 Bars`, `4 Bars`.
3. `preset_data.py` schema: `stepRate` max 11 → 15. No factory preset uses the new indices;
   nothing regenerates except by `build_bank.py` (a rerun is a no-op on content, but run it
   and the audit anyway).
4. Tests: a DSP assert for the new values; a plugin test that index 15 yields a 16-beat step
   (the step clock fires one note per 4 bars). `testStepRateMigration` untouched (append-only).

### Phase 4 — Documentation and verification
1. Renders for both changed editors; manual screenshots regenerated; crops re-cut where the
   layout changed; manual prose updated (StereoTool rows and meters, MultiSat grid and meters).
2. `AGENTS.md` §4.5 / §4.7 (meter removal, grid; heights), §7 plan-15 decisions (meters dropped
   on these two; the suite meter rule stays for future plugins), README, CHANGELOG,
   `owner_findings.md` round 3, `docs/plans/README.md`.
3. Verification: clean Release build (0 errors, warning-kind baseline), CTest 28/28 with the
   bank on, `build_bank.py` + `audit_bank.py` (200 presets), `git diff --stat` /
   `git status --short`. Knob drawing untouched.

## 3. Risks and open points

- StereoTool at 640 px height: the MOD page's preview takes `jmax (150, 30 %)`; if the
  modulation panel's preferred height no longer fits, the MOD preview minimum drops to keep
  the panel whole — checked against `getPreferredHeight()` in implementation.
- MultiSat visualiser without meters: the band bars keep the full container height; the grid
  must not collide with the LOW/HIGH labels (grid drawn behind the bars).
- Step Rate 4 Bars with Gate 100 % holds a note for four bars — intentional; the gate
  machinery already schedules multi-block note-offs.
- Not checked here: macOS / AU, DAW loading, the owner-side lockup regeneration.
