# 11_MidiEffect_StereoTool_Multiband_Improvements.md

**Status: implemented 2026-11-19.** All phases done. Bank build CTest 28/28, `audit_bank.py` passes with 160 presets, renders under `build/renders/plan11-p45/` and committed under `docs/manuals/`. Open items: macOS/AU builds, a DAW load check, `SUITE_PRESET_BANK_AS_PROGRAMS=ON` run, desktop-session editor tests — as everywhere.

---

## Objective
Close the owner's five UI/preset requirements for MidiEffect, MultibandSaturation and StereoTool, on top of the already-landed plan 6.1a work.

## Requirements (owner brief)
- **#1** all three have a preset-bar **and** a settings/about menu (gear button).
- **#2** at least 15 factory presets each.
- **#3** same button/switch concept as MTC/SD/DS — dropdown only if more than 6 entries or long text.
- **#4** each has a visualization.
- **#5** controls combined in groups (like the other plugins).

## Verified status (code-checked 2026-11-19)

| Plugin | Preset Bar | Gear Menu | ≥15 Presets | Suite choice widgets (#3) | Visualization (#4) | Groups (#5) |
|---|---|---|---|---|---|---|
| **MidiEffect** | ✅ | ❌ missing | ✅ 15 | ❌ 8 ComboBoxes, 7 of them ≤6 entries | ❌ | ✅ SHAPE / ARP |
| **StereoTool** | ✅ | ❌ missing | ✅ 15 | ❌ 3 ComboBoxes, all ≤6 entries | ❌ | ✅ CHARACTER / IMAGE |
| **MultibandSaturation** | ❌ missing | ❌ missing | ❌ 0 | ❌ 1 ComboBox (MODE, 3 entries) | ❌ | ❌ flat list, no groups |

Facts behind the table (verified in sources):
- **Gear menu**: none of the three passes `SETTINGS` to `suite_add_plugin()`; no `HeaderMenuButton` / `showAboutDialog` / `showSettingsDialog` in any of the three editors. Only SD / DS / MTC / Sgraffito / Cyclical have it. `MODULATION` is already in MultibandSaturation's CMake; only `SETTINGS` is missing.
- **Choice widgets**: MidiEffect uses 8 plain `juce::ComboBox`es (SCALE 6, CHORD 8, PATTERN 5, STEP RATE 5, VEL CURVE 4, MODE 2, SYNC 2, VEL DIR 2). Only CHORD qualifies for a dropdown under rule #3. StereoTool uses 3 (CHARACTER 5, TARGET 3, SIDE FILTER 4); MultibandSaturation uses 1 (MODE 3). None use `SuiteUI::SegmentedSelector`.
- **MultibandSaturation**: no `SuitePresets` include, no `StateHolder`, no `presetBar`, no preset key in `tools/preset_data.py`, no `MultibandSaturation_PresetTests.cpp` (the preset-test target only auto-appears when that file exists). `numControls` = 11.
- **Current CTest counts**: bank build = **27 entries** (verified with `ctest -N`); default build = 20 per AGENTS. Preset bank total = **145** (`audit_bank.py` `EXPECTED_TOTAL = 145`; 26 SD / 26 DS / 28 MTC / 20 Sgraffito / 15 MidiEffect / 15 StereoTool / 15 Cyclical; **MultibandSaturation 0**). `audit_bank.py`'s docstring still says "130 total" — stale, fix alongside.
- **Group styling (#5)**: MidiEffect and StereoTool already draw real groups, not tabs — single-page editors with titles in the accent at 15 pt bold ("ARP"/"SHAPE", "CHARACTER"/"IMAGE"), `groupGap = 8`, `SuiteUI::groupTitleHeight` (verified in both `paint()`/`resized()`). Requirement #5 is fully met for these two; only MultibandSaturation needs grouping.
- **Processor publish state (Phase 4 prerequisite)**: **none of the three processors publishes UI state today.** MidiEffect has no snapshot and no UI atomics beyond `currentProgramIndex`; its chord/arp data (`heldChords`, `arpVoices` with `sequence[8]`, `sounding`, `nextStep`) is private, non-atomic, audio-thread-owned. StereoTool's transient follower is private and unpublished. MultibandSaturation publishes nothing. Phase 4 therefore starts with new per-block publish plumbing in each processor (the MTC trail/offsets pattern: plain struct copied into an atomic snapshot per block, read on the 60 Hz timer, no locks).
- **`SuiteUI::SegmentedSelector`** exists (`SuiteWidgets.h`: `setOptions` / `setSelectedIndex` / `onChange`, accent colour) but has **no APVTS attachment** — DS/Sgraffito wire it through `juce::ParameterAttachment` with the denormalised choice index (AGENTS gotcha). The new `LabelledSelector` must encapsulate that wiring.

## Decisions carried over from plan 6.1a (already implemented, uncommitted)
- MidiEffect Chord mode kept event-for-event identical; Arp mode uses a beat-grid step clock (host PPQ when synced, Tempo parameter otherwise), fixed 8-step cycle.
- Octave Spread implemented as per-cycle octave crawl, lifting by `round(spread × cycle)` octaves each pass through the chord tones.
- Transient→Drive boost capped at +12 dB max.
- Console character uses 75 % of the tube bend for a gentler tanh blend.
- Latency reported as 50 ms but events emitted without delay (chord path event-identical, arp events never earlier than grid time).
- State version not bumped (additive only; new parameters default to neutral at load time).
- Scattered appended to `arpId` (index 4); saved indices 0–3 unchanged.

## New decisions for this plan
- **D1** — Choice-widget conversion is a code phase, not an audit: a shared `SuiteUI::LabelledSelector` (caption above, pill selector below, R2/R3 sizing: 36 px pill, radius 6) goes into `modules/suite_ui`, replacing the per-plugin `ComboCell` copies in MidiEffect and StereoTool. CHORD (8 entries) keeps a dropdown; everything else becomes a `SegmentedSelector`-style pill cell.
- **D2** — MultibandSaturation presets are deltas like every other plugin's (JSON in `tools/preset_data.py`, `build_bank.py`, `audit_bank.py`); empty-tree INIT = reset to defaults (no sample semantics — that text in the earlier draft was Sgraffito-specific and wrong here).
- **D3** — MultibandSaturation ships the full plan-09 preset seam: `getPresetSelection` / `setPresetSelection` + the `presetSelection` state property, mirroring MidiEffect/StereoTool (verified present there).
- **D4** — No new parameters anywhere in this plan; `numControls` stays 14 / 11 / 11 and no state-version bump.
- **D5** — Gear menus use the R13 pattern: `SETTINGS` in `suite_add_plugin`, `SuiteUI::HeaderMenuButton` in the header (far right, right of the preset bar), About… via `SuiteUI::showAboutDialog`, Settings… via `SuiteSettings::showSettingsDialog`, plus a test (gear found in header, both actions wired, dialogs paint).
- **D6** — The MidiEffect visualizer shows the **most recently triggered held key** (its `ArpVoice::sequence` and `sounding` set); notes sounding from *other* keys are drawn as small markers only. The processor publishes a small atomic snapshot per block (last key, sequence[8], sounding flags, current step index); no locks.
- **D7** — Phase 4 begins with a **publish-plumbing step per plugin** (snapshot struct + per-block publish + a test that the snapshot changes when the engine runs) before any widget is painted. The MTC trail-and-offsets pattern is the template; all three processors currently publish nothing.

---

# Work plan

## Phase 3 — MultibandSaturation: full preset seam + gear + grouped layout (largest work item)

1. **CMake** (`plugins/MultibandSaturation/CMakeLists.txt`): add `SETTINGS` to the `suite_add_plugin` call (`MODULATION` is already there).
2. **Processor** (`PluginProcessor.{h,cpp}`):
   - Derive from `SuitePresets::StateHolder`; implement `getPresetPluginId` (`"MultibandSaturation"`), `getPresetStateVersion`, `getPresetStateTree`, `applyPresetState` (empty tree → reset to defaults; `LoadResult::loaded` resets the program index), plus the plan-09 `getPresetSelection` / `setPresetSelection` virtuals and the additive `presetSelection` state property (D2, D3).
   - Program-list virtuals (`getNumPrograms` … `setCurrentProgramStateInformation`) under `#if SUITE_PRESET_BANK_AS_PROGRAMS`, mirroring SpectralDrift.
3. **Editor** (`PluginEditor.{h,cpp}`):
   - `SuitePresets::PresetBar` in the header (flat, no fill, plan 05.1 style).
   - `SuiteUI::HeaderMenuButton` gear (D5).
   - **Grouped layout** (requirement #5): two groups in the plugin's coral accent — SPLIT (crossover) and SHAPER (per-band drive/tone/sensitivity/attack/release, mix, output) — or a SOUND/MOD page split if the owner prefers page parity with SD/DS. Group titles 15 pt bold accent, `groupGap` 8 px, R4 window ≤1000×700.
4. **Presets** (requirement #2):
   - 15 `_preset(...)` entries under a new `"MultibandSaturation"` key in `tools/preset_data.py` (≥3 categories, in-range values, none equal to default).
   - Add `"MultibandSaturation": 15` to `audit_bank.py`'s `EXPECTED_PRESETS_PER_PLUGIN` and fix the stale "130 total" docstring to 145.
   - Create `plugins/MultibandSaturation/tests/…` preset tests executable (`MultibandSaturation_PresetTests.cpp`) following MidiEffect/StereoTool's; `suite_add_plugin` picks it up automatically.
5. **Tests**: preset bar in the component tree; state round trip; gear-menu test (D5); `checkRange`/`checkParameterIds` unchanged (D4).
6. Update plugin test count expectations in AGENTS §3 (`MultibandSaturationTests` grows beyond 7 functions).

## Phase 4 — Visualizations (requirement #4; one per plugin, independent)

**Step 0, per plugin (D7 — publish plumbing before any widget):**
1. Design a small snapshot struct per plugin (plain values, no pointers into audio buffers).
2. The processor fills it once per `processBlock` (last, after the DSP work) into atomic members — the MTC trail/offsets pattern. No locks, no allocation on the audio thread.
3. Plugin test: run a block with input and assert the snapshot changes; run it idle and assert it stays stable. This is the spike that proves the visualizer can read without rework.

| Plugin | Component | Published data (new) | Notes |
|---|---|---|---|
| **MidiEffect** | `ChordGridVisualizer`: scale keyboard grid with chord tones highlighted; active notes as rectangles with velocity height. Second small strip: 8-step arp pattern with current-step highlight | Last key, `sequence[8]`, sounding flags, current step index (D6) | 60 Hz timer reads the snapshot; MIDI effect, no audio to meter |
| **MultibandSaturation** | `DualBandVisualizer`: two overlapping bar graphs (low/high band drive/envelope), crossover frequency marker between them | Per-band envelope values, crossover Hz (parameter is already readable via its atomic) | Reuse `SuiteUI::LevelMeter` where it fits |
| **StereoTool** | `MidSideMeter`: mid/side level meters + width indicator; `TransientCurveDisplay`: live follower envelope with attack/release shape | Mid/side RMS levels, follower envelope value | Follower state exists but is private and unpublished — publishing is new code |

All visualizers: child of the editor, updated from processor snapshots on the `SuiteUI::kRefreshHz` timer, atomics only (AGENTS §7). Each gets a paint test (two renders in one process must match, `testRenderBaseline` pattern) and R9 renders under `build/renders/`.

## Phase 5 — Choice-widget conversion (requirement #3; code, not an audit)

1. **SuiteUI**: add `SuiteUI::LabelledSelector` (caption 13 pt bold `textDim` above, `SegmentedSelector` 36 px pill below, value text 14 pt) in `modules/suite_ui`; extend `SuiteUITests` for its painting/layout. It encapsulates the `ParameterAttachment` wiring (denormalised choice index, per the AGENTS gotcha) against a named parameter ID, since `SegmentedSelector` itself has no APVTS attachment.
2. **State-safety note (verified)**: converting a widget from `ComboBoxAttachment` to `ParameterAttachment` does not touch saved state — both write the same parameter by its `static constexpr` ID string (`scaleId`, `chordId`, …, all public members of each processor), and the choice index lives on the parameter, not the widget. The only rule is the existing one: choice lists stay append-only so saved indices keep their meaning.
2. **MidiEffect**: convert 7 of 8 cells — SCALE, PATTERN, STEP RATE, VEL CURVE, MODE, SYNC, VEL DIR → `LabelledSelector`. **CHORD stays a dropdown** (8 entries, >6). Delete the local `ComboCell` class.
3. **StereoTool**: convert CHARACTER, TARGET, SIDE FILTER → `LabelledSelector`. Delete the local `ComboCell` class.
4. **MultibandSaturation**: convert MODE → `LabelledSelector` (as part of the Phase 3 editor rework if that lands first).
5. Scope check: SCALE has exactly 6 entries (rule says dropdown only if *more* than 6), so it converts to a pill; if the owner later adds a 7th scale, it may move back to a dropdown — append-only choice lists either way (saved indices must not move).
6. Before/after renders per plugin (R7): every affected editor rendered before and after, compared, stored under `build/renders/`.

## Phase 6 — Gear menus for MidiEffect + StereoTool (requirement #1 remainder)

1. **CMake**: add `SETTINGS` to both `suite_add_plugin` calls.
2. **Editors**: `SuiteUI::HeaderMenuButton` (accent: teal `#4ECDC4` / violet `#B18CFF`) far right of the header, right of the preset bar; About… via `SuiteUI::showAboutDialog`, Settings… via `SuiteSettings::showSettingsDialog` (no sample-folder row — that is Sgraffito-only).
3. **Tests**: gear found in header with both actions wired; About and Settings contents paint (the Cyclical test pattern, R13).

## Phase 7 — Verification, renders, documentation

1. **Clean build + counts** (AGENTS §15.1): `--target clean`, full Release build, 0 errors, warning count no worse than the ~90 baseline.
2. **CTest**: bank build = 27 entries (+1 new `MultibandSaturation_PresetTests` → **28**; report the change), default build = 20. Run full suite with `--output-on-failure`.
3. **Renders** (R7/R9): before/after PNGs of every changed editor view under `build/renders/`; two renders per view in one process must match.
4. **Manuals** (R11): update `plugins/<Name>/MANUAL.md` walkthroughs; regenerate committed `docs/manuals/<plugin>-*.png` full-page renders (+ `tools/manuals/crops.py` details, `scale_images.py` conversion). MultibandSaturation gets its first manual.
5. **audit_bank**: run `python tools/build_bank.py` then `python tools/audit_bank.py` — 160 total presets expected after Phase 3.
6. **Docs sync**: README (preset table, plugin list), CHANGELOG, AGENTS §3/§4/§7 (R13 gap closed for all three; MultibandSaturation no longer "the one plugin left without the seam"; test-function counts; preset totals 145 → 160).
7. **Untracked check**: `git status --short` at the end; show the owner anything unexpected.
8. Not verified here (report honestly): macOS/AU builds, DAW load, `SUITE_PRESET_BANK_AS_PROGRAMS=ON` run, desktop-session editor tests.

---

## Effort estimate

| Phase | Hours | Notes |
|---|---|---|
| 3. MultibandSaturation seam + presets + groups + gear | 8–10 | Largest: editor rework + 15 presets + preset tests |
| 4-step 0. Publish plumbing (3 processors) | 3–4 | Snapshot structs + per-block publish + change tests (D7) |
| 4a. MidiEffect visualizer | 3–5 | Chord grid + arp steps, on top of step 0 |
| 4b. MultibandSaturation visualizer | 3–5 | Dual band bars, on top of step 0 |
| 4c. StereoTool visualizer | 3–5 | Meters + transient curve, on top of step 0 |
| 5. Choice-widget conversion | 4–6 | Shared widget + 11 cells across 3 plugins + renders |
| 6. Gear menus MidiEffect/StereoTool | 2–3 | Mechanical, R13 pattern |
| 7. Verification/docs | 3–4 | Renders, manuals, audit, README/AGENTS |
| **Total** | **~29–42 hours** | Phases 4a/4b/4c, 5 and 6 are independent of 3 and of each other |

## Key files

| File | Change |
|---|---|
| `plugins/MultibandSaturation/CMakeLists.txt` | add `SETTINGS` |
| `plugins/MultibandSaturation/Source/PluginProcessor.{h,cpp}` | StateHolder, presetSelection, program virtuals |
| `plugins/MultibandSaturation/Source/PluginEditor.{h,cpp}` | PresetBar, gear, groups, `DualBandVisualizer`, `LabelledSelector` |
| `plugins/MultibandSaturation/tests/` | extended PluginTests + new PresetTests |
| `plugins/MidiEffect/CMakeLists.txt`, `Source/PluginEditor.{h,cpp}` | `SETTINGS`, gear, `LabelledSelector` ×7, `ChordGridVisualizer` |
| `plugins/StereoTool/CMakeLists.txt`, `Source/PluginEditor.{h,cpp}` | `SETTINGS`, gear, `LabelledSelector` ×3, `MidSideMeter`/`TransientCurveDisplay` |
| `modules/suite_ui/include/SuiteWidgets.h` + Source | `LabelledSelector` |
| `modules/suite_ui/tests/SuiteUITests.cpp` | `LabelledSelector` paint/layout tests |
| `tools/preset_data.py` | 15 MultibandSaturation presets |
| `tools/audit_bank.py` | Multiband entry + docstring 130 → (160 after Phase 3) |
| `tools/brand/lockups.py` | verify rows exist for all three (they do; no action expected) |
| `README.md`, `CHANGELOG.md`, `AGENTS.md`, `plugins/*/MANUAL.md`, `docs/manuals/*` | docs sync |

## Execution order

1. **Phase 3** (MultibandSaturation full seam) — sets the pattern, closes requirement #1/#2/#5 for the last plugin.
2. **Phase 6** (gear menus, mechanical) — can run in parallel with Phase 3.
3. **Phase 4a + 4c** (MidiEffect + StereoTool visualizers, parallel).
4. **Phase 4b** (MultibandSaturation visualizer, after its Phase 3 editor exists).
5. **Phase 5** (choice-widget conversion) — after the visualizers so renders are taken once.
6. **Phase 7** (verification/docs) — last, before handing back to the owner.

## Final state (owner brief scorecard)

| Plugin | #1 Preset bar + gear | #2 ≥15 presets | #3 Suite widgets | #4 Visualization | #5 Groups |
|---|---|---|---|---|---|
| **MidiEffect** | ✅ (gear added) | ✅ 15 (unchanged) | ✅ 7 pills, CHORD dropdown | ✅ Chord grid + arp strip | ✅ (unchanged) |
| **MultibandSaturation** | ✅ (both added) | ✅ 15 (new) | ✅ 1 pill | ✅ Dual-band bars | ✅ (new groups) |
| **StereoTool** | ✅ (gear added) | ✅ 15 (unchanged) | ✅ 3 pills | ✅ Meters + transient curve | ✅ (unchanged) |

Bank after this plan: 160 presets (145 + 15 MultibandSaturation). All three follow R1–R13.
