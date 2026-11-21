# Plan 13: DAW-test findings 2 — visualisers, modulation, playable keys

Written for: the coding agents who carry out the phases. Read `AGENTS.md` first. Plan 12
(the first DAW-test pass) is the baseline for the code this plan changes. Source of record:
`owner_findings.md` at the repository root.

Status: implemented 2026-11-21 (all phases, Phases 0-8; verified: clean Release build 0 errors,
28/28 CTest with the bank on, 200 presets audited). Two findings arrived after this plan was
written and were fixed outside it: the MTC TRANSPORT knob distance and the Pendulum LFO DSP bug
(see AGENTS §7, plan 13 decisions). Owner gates open: the StereoTool width and the regenerated
manual screenshots. The Cyclical overhaul
was split into **plan 14** on 2026-11-20 and is no longer in this plan.

Naming: the owner's name for the MIDI plugin is **MidiTool** (folder `plugins/MidiEffect`). In all
prose this plan says MidiTool. Folder, CMake target, bundle ID and plugin code stay as they are,
because renaming an ID or folder is a separate decision (AGENTS §4).

---

## 0. Verified current state (2026-11-20)

### 0.1 MultiSat (`plugins/MultibandSaturation`)

| Finding | Verified state |
|---|---|
| "The 2 meters for Low and High and the crossover preview should use the whole height of their container. Add some kind of Scale to the previews." | `DualBandVisualizer::paint` (`PluginEditor.cpp:199-255`): the band bars sit in `area.removeFromTop (area.getHeight() * 0.62f).reduced (14, 8)`, so the bottom 38 % is empty. The crossover marker is a log-position line (`log (hz/80) / log (8000/80)`) with a `<hz> Hz` text. **No axis, no ticks, no scale is drawn** |
| "Input meter left, output right, both stereo" | Both `SuiteUI::LevelMeter`s are at the **right** edge (`resized()` 194-196). `LevelMeter` is a single mono bar, −60…0 dB (`SuiteWidgets.h:80-92`), fed from `inputPeak` / `outputPeak`, the block max over both channels (`PluginProcessor.cpp:190-192`). `BandSnapshot` has no per-channel fields (`PluginProcessor.h:47-51`) |
| "Add Modulation/LFO" | CMake passes `MODULATION` (`CMakeLists.txt:13`), but no `Router` / `SuiteModulation` use exists in the plugin. 11 parameters (`numControls = 11`, `PluginProcessor.h:41`); the Router's 34 are not in the layout (AGENTS §4.5) |
| "At least 10 more presets that use modulation" | 15 presets (`preset_data.py:384-400`), none uses modulation. The SCHEMA has **no `_routing(...)`** (comment at 174-175). Categories: Mix bus, Bass, Bright, Drums, Vocal, Character, Subtle, Lo-Fi, Extreme, Utility |

Tests today: 10 functions, `numControls = 11` asserted against the header constant
(`tests/PluginTests.cpp:25`). Window 760×700, `visualHeight = 242` (`PluginEditor.cpp:23`,
grep-verified 2026-11-20; the 200 target from plan 12 is superseded by this value).

### 0.2 Spectral Drift

26 presets (`preset_data.py:234-261`). **8 already use modulation**, always the same shape:
`lfo1Rate` + `mod1Target = 12` (Drift) + `mod1Amount = 100`. No LFO2, no shapes, no
sync/division/offset, no `modNDelay`, no second routing. Categories (11): Ambient, Freeze, Bright,
Subtle, Wide, Rhythmic, Character, Mix, Dark, Lo-Fi, Utility.

### 0.3 Stereo Tool (`plugins/StereoTool`)

| Finding | Verified state |
|---|---|
| "IMAGE + LEVEL in one row, window width smaller; CHARACTER and the Input Bars/FOLLOWER preview fill the whole width" | Window 880×700 (`PluginEditor.cpp:11-23`). The LEVEL group (`levelGroupWidth = 220`) sits at the **right end of the visualiser band** (`resized()` 181-192); the visualiser takes the remaining 130 px; CHARACTER and IMAGE are full-width bands below. The MidSideMeter draws mid/side bars in its left 64 px and the follower trace in the rest (`PluginEditor.cpp:249-323`) |
| "Add Modulation/LFO + 10 presets" | CMake passes `MODULATION` (`:13`); no Router use (only `MANUAL.md:82` promises it "for a follow-up pass"). 13 parameters; the test hardcodes `checkParameterIds (…, 13)`, with **no `numControls` constant**. 15 presets (`preset_data.py:365-381`), no `_routing(...)` in the SCHEMA (157-173) |

Also verified: the processor publishes `MeterSnapshot.inputPeak` / `outputPeak`
(`PluginProcessor.h:68-75`, computed at `PluginProcessor.cpp:217-231, ~301`), but the editor never
draws them. That is dead data today.

Smoothing, grep-verified 2026-11-20: only the two level stages are smoothed
(`inputGainSmoothed`, `outputGainSmoothed`, 20 ms, `PluginProcessor.cpp:122-126, 205-206`). Mix
and the Character drive are **not** smoothed.

### 0.4 Dynamic Saturation

26 presets (`preset_data.py:262-289`). **Zero** presets set any `lfo*`/`mod*` key. The SCHEMA
already ends with `**_routing(12)` (`:72`), so no tooling change is needed beyond counts.
Categories (12): Mix bus, Subtle, Vocal, Character, Bass, Drums, Parallel, Bright, Ambient,
Extreme, Lo-Fi, Utility.

### 0.5 MidiTool (folder `plugins/MidiEffect`)

| Finding | Verified state |
|---|---|
| "Keyboard/Chords should be playable/clickable in any way" | `ChordGridVisualizer` (`PluginEditor.h:30-39`, paint `PluginEditor.cpp:236-370`) is **pure paint**: an 8-step arp strip above a drawn mini keyboard (MIDI 48–72, 25 keys). Nothing is clickable. The processor has **no UI-note path**: MIDI arrives only from the host's `MidiBuffer` (`PluginProcessor.cpp:174`) |

The suite already has the pattern to copy: Sgraffito's `PlayableKeyboardStrip` (MIDI 24–96) →
`pushUiNote` SPSC ring (32 slots, `uiNoteWrite`/`uiNoteRead` atomics) drained at the top of
`processBlock` (`plugins/Sgraffito/Source/PluginProcessor.h:363-384`, wired at
`PluginEditor.cpp:275-277`). MidiTool's per-key `ArpVoice` state (`std::array<ArpVoice, 128>`,
`PluginProcessor.h:147-157`) is keyed by MIDI note, so a UI-injected note runs the existing
chord/arp paths unchanged.

### 0.6 Preset tooling (`tools/preset_data.py`, `tools/audit_bank.py`)

- `_routing(target_max)` (lines 19-36) generates the Router parameter coverage. It is spread into
  SpectralDrift (56), DynamicSaturation (72), MicroTimingChaos (95) and Sgraffito (135) only.
  **StereoTool and MultibandSaturation have none.**
- Latent bug: `_routing` generates `lfo{N}Shape` with max **6** (7 shapes, 0-indexed), but
  Pendulum is index **7** (appended 2026-11). A preset using Pendulum would fail the audit. No
  current preset sets a shape, so it is latent.
- `EXPECTED_COUNTS` (`audit_bank.py:35-36`): StereoTool 15, MultibandSaturation 15;
  `EXPECTED_TOTAL = 160`. All grow in this plan.
- `check_schema_covers_live` (406-438) detects Router use via the string
  `"Router::addParameters"` (`plugin_uses_router`, 383-392) and requires the fixed
  `ROUTER_PARAMETER_IDS` list in the schema. Wiring the Router without extending the schema
  **fails the audit**, which is the intended guard.

---

## 1. Decisions (owner, 2026-11-20)

| # | Topic | Decision |
|---|---|---|
| — | Preset totals | Suite bank 160 → **200** after this plan: SD +10, DS +10, MultiSat +10, StereoTool +10. Plan 14 adds Cyclical to reach 210 |
| — | StereoTool width | **~720 px**, final value confirmed against renders and approved by the owner (window size is per plugin, AGENTS R4) |
| 8 | MultiSat layout | **Two pages: SOUND and MOD**, following the SD/DS page switch (R8). Replaces the "or its own band" wording |
| 9 | Note-off | **Every UI note has a guaranteed note-off** on every ending path (Phase 7 item 4). The rule is stated once here and applies to MidiTool |
| 12 | Documentation | **MidiTool** replaces MidiEffect in all prose. Folder, IDs and bundle stay |
| 14 | Knobs and meters | **Knobs are untouchable.** Nothing here changes `LabelledSlider`, the knob size or the knob drawing. **Stereo Level Meter rule:** input meter on the left, output meter on the right, both use the whole height of their container. It applies to every stereo meter in the suite |
| — | Split | Cyclical overhaul moved to plan 14 |

---

## 2. Phases

### Phase 0 — Baseline (before any change)

Clean Release build, record the warning count, and save the before renders of the editors whose
layout or widgets this plan changes (MultiSat, StereoTool, MidiTool; SD and DS change presets
only, so they need no renders). Every later phase compares
against this. Plan 12 gives three different counts (31, 46, "~31"); the measured count replaces
them everywhere, AGENTS §3 and §15 included.

### Phase 1 — Presets: SD +10, DS +10, and the `_routing` shape fix

Files: `tools/preset_data.py`, `tools/audit_bank.py`, the generated bank
(`modules/suite_presets/factory_bank/bank/`), `AGENTS.md` §1/§5.5 counts, `CHANGELOG.md`.

1. `_routing()`: `lfo{N}Shape` max 6 → **7** (Pendulum becomes preset-usable).
2. **SpectralDrift +10** presets (26 → 36), existing categories only. Exercise what the current 8
   do not: LFO2 routings, triangle/saw/square/sample-and-hold/pendulum shapes, synced LFOs
   (`lfoNSync` + `lfoNDivision`), negative amounts, bipolar targets (Spectral Shift ±32, Spectral
   Tilt ±2), `modNDelay` on some, and two-routing combinations (e.g. LFO1 → Drift + LFO2 → Spectral
   Gate).
3. **DynamicSaturation +10** presets (26 → 36), existing categories only. Build around Drive,
   Sensitivity, Mix, Output, Attack, Release, Detect HPF, Tube Bias, Fold, Tone and Curve.
4. `audit_bank.py`: `EXPECTED_COUNTS` SD 26 → 36, DS 26 → 36; `EXPECTED_TOTAL` 160 → **180**.
5. Run `tools/build_bank.py` then `tools/audit_bank.py`; the `*_PresetTests` executables
   (`bankNotDefault`, in-range values) must pass.
6. Presets are appended, so under `SUITE_PRESET_BANK_AS_PROGRAMS` the new programs land after the
   existing ones (bank order is append-only once hosts cache indices).

### Phase 2 — SuiteUI: stereo level meter

Files: `modules/suite_ui/include/SuiteWidgets.h`, `modules/suite_ui/Source/SuiteWidgets.cpp`,
`modules/suite_ui/tests/SuiteUITests.cpp`.

1. **`SuiteUI::StereoLevelMeter`**: one bar per channel (L and R), the same −60…0 dB law as
   `LevelMeter`, `setLevels (float left, float right)`. It is a new widget with its own look; it
   does not restyle `LevelMeter` or any knob. Alternatively, a `stereo` mode on `LevelMeter`.
   Either way there is **one** shared widget, no per-editor copies.
2. Tests: idle paints no fill; L-only level moves the left bar and not the right; both bars
   saturate at 0 dB; pixels change when the levels change.
3. **Placement rule (owner, 2026-11-20).** Wherever the suite shows a stereo meter, the **input
   meter is on the left and the output meter on the right**, and both **use the whole height of
   their container**: no fixed height, no reserved strip. The widget takes its bounds from the
   container. Tests: the input meter's x is left of the output meter's x, and both heights equal
   the container height.
4. **Knobs are untouchable.** Nothing in this plan edits `LabelledSlider`, `knobDiameter` or the
   knob drawing. The existing size test must not change.

### Phase 3 — MultiSat visualiser: full height, scales, stereo meters

Files: `plugins/MultibandSaturation/Source/PluginProcessor.{h,cpp}`, `PluginEditor.{h,cpp}`,
`tests/PluginTests.cpp`. Renders under `build/renders/`.

1. `BandSnapshot` gains `inputPeakL/R` and `outputPeakL/R` (block max per channel). The mono
   `inputPeak` / `outputPeak` fields are replaced. The meter feed in `paint` (253-254) follows.
2. `DualBandVisualizer::resized()`: the input meter column moves to the **left** edge
   (`StereoLevelMeter`, ~18-20 px wide); the output column stays at the **right** edge. Both span
   the full height of the visualiser (Phase 2 item 3).
3. The LOW/HIGH band bars and the crossover preview span the **full container height**. The
   LOW/HIGH labels overlay the bottom of their bars instead of sitting in a reserved strip.
4. Scales (10 pt dim text like the existing labels):
   - **Hz axis** under the crossover preview: log ticks at 80 / 100 / 200 / 500 / 1k / 2k / 5k /
     8k, using the existing `log (hz/80) / log (8000/80)` mapping. The crossover marker and
     `<hz> Hz` text stay.
   - **dB axis** beside the meter columns: ticks at 0 / −6 / −12 / −24 / −60, matching the −60…0 dB
     law.
5. Tests: `testVisualSnapshot` extended (an L-only sine moves `inputPeakL`, not `inputPeakR`); an
   editor test asserts the meter x positions (input left, output right) and that scale text is
   painted. Renders before and after (R7/R9).

### Phase 4 — MultiSat modulation, two pages, and 10 presets

Files: `plugins/MultibandSaturation/Source/PluginProcessor.{h,cpp}`, `PluginEditor.{h,cpp}`,
`tests/PluginTests.cpp`, `tools/preset_data.py`, `tools/audit_bank.py`.

1. Target table (`TargetInfo`, append-only, 10 targets): Crossover, Drive Low, Drive High,
   Sensitivity, Attack, Release, Tone Low, Tone High, Mix, Output. `mode` is excluded (a choice,
   as on DS). Modulation works in normalised space, so the log-ranged Crossover needs nothing
   special (AGENTS §7).
2. `Router::addParameters (params, names)` appended after the 11 existing → **numControls 11 → 45**.
   Build the layout the shared way (vector, then `{ params.begin(), params.end() }`, AGENTS §11).
   Construct the router after `apvts`; `prepare` / `computeOffsets` / `publish` / `advanceBlock`
   as SD/DS do; read each routed control through `modulate`.
3. **Smoothing audit (as Phase 6 does for StereoTool).** AGENTS §4.5: drive low/high, tone
   low/high, mix and output are already smoothed 50 ms (safe); sensitivity, attack and release
   are consumed per block (safe). **Crossover is the exception: it is updated per block, and
   modulating the LR4 split point per block produces stepped coefficient changes that can
   click.** Smooth the crossover frequency (in Hz) before the coefficients are computed, and
   test the audible result.
4. Editor: **two pages, SOUND and MOD** (owner decision 8), following the SD/DS page switch (R8).
   SOUND holds the current controls and the visualiser; MOD holds `ModulationPanel`, with the
   visualiser above it as on SD/DS. `setPage` only toggles visibility, so switching pages never
   changes a parameter. `ControlDisplay` on the routed knobs; help texts on the new controls (R14a).
5. Defaults stay bit-identical: every routing amount is 0, so the base values are returned exactly.
   Add the no-modulation regression test the SD/DS tests use.
6. Tests: `checkParameterIds` 11 → 45, `checkRange` for the new IDs, state round trip with routing
   moved, routed controls move the audio, synced LFO follows the tempo clock, the page switch
   changes no parameter, and the MOD page paints the panel.
7. Presets: **+10** using modulation, existing categories. Schema gains `**_routing(10)`.
   `audit_bank.py`: MultibandSaturation 15 → 25, `EXPECTED_TOTAL` 180 → **190**.

### Phase 5 — StereoTool layout rework

Files: `plugins/StereoTool/Source/PluginEditor.{h,cpp}`, `tests/PluginTests.cpp`,
`docs/manuals/stereotool-*.png`, `MANUAL.md`. Renders under `build/renders/`.

1. New arrangement (owner's sketch):
   - Row 1: **LEVEL** (left, INPUT and OUTPUT knobs) and **IMAGE** (the rest of the width; its five
     controls reflow into the wider group).
   - Row 2: the **mid/side bars and FOLLOWER preview across the full window width**.
   - Row 3: **CHARACTER** across the full width.
2. `editorWidth` 880 → **~720**, height stays 700. Confirm the final width against renders (four
   knob cells per row at 40 px knobs must keep room). The owner approves the measured size.
3. Draw the processor's `inputPeak` / `outputPeak` (dead data today) as a **stereo meter pair
   following the Phase 2 placement rule**: input meter on the left edge of the full-width
   preview, output meter on the right edge, both the full height of the preview. The mid/side
   bars and the follower trace sit between them.
4. `testLevelGroupAndHelp` and the layout assertions updated. Before and after renders; manual
   screenshots regenerated for this plugin (R11).

### Phase 6 — StereoTool modulation and 10 presets

Files: `plugins/StereoTool/Source/PluginProcessor.{h,cpp}`, `PluginEditor.{h,cpp}`,
`tests/PluginTests.cpp`, `tools/preset_data.py`, `tools/audit_bank.py`.

1. Target table (10 targets, append-only): Width, Drive, Mix, Transient Attack, Transient Release,
   Transient Amount, Mid Focus, Side Crossover, Input, Output. `character` and `transientTarget`
   are excluded (choices). **Side Crossover** only has an effect while Side Filter is not Off
   (AGENTS §4.7): it stays in the table, the manual says so, and a test asserts that modulating
   it with the filter Off changes nothing.
2. `Router::addParameters` appended → **numControls 13 → 47**. Add a `numControls` constant to the
   header (the test currently hardcodes 13). Same router wiring as Phase 4.
3. **Smoothing audit:** only Input and Output are smoothed today. Before wiring, each routed target
   is classified as (a) already smoothed, (b) consumed per block (safe), or (c) consumed per
   sample and needing smoothing. Mix and Drive are (c) and get a 20 ms `SmoothedValue`, as the SD
   procedure did (AGENTS §7). The identity early-return still requires `inputLevel == 0 &&
   outputLevel == 0`; routed amounts default to 0, so the defaults stay bit-exact. Test: the
   defaults are bit-identical to the Phase 5 build. For Drive there is a second option: the
   transient follower already modulates `driveDb` per sample (`PluginProcessor.cpp:284-291`), so
   an LFO-modulated drive can reuse that per-sample path instead of a smoother. Either is
   acceptable; the classification decides.
4. Tests mirroring Phase 4. The manual's "planned for a follow-up pass" line (`MANUAL.md:82`) is
   removed, because it shipped.
5. Presets: **+10** using modulation, existing categories. Schema gains `**_routing(10)`.
   `audit_bank.py`: StereoTool 15 → 25, `EXPECTED_TOTAL` 190 → **200**.

### Phase 7 — MidiTool: playable keyboard, chords and the note-off guarantee

Files: `modules/suite_ui/include/SuiteWidgets.h` (+ Source), `SuiteUITests.cpp`,
`plugins/MidiEffect/Source/PluginProcessor.{h,cpp}`, `PluginEditor.{h,cpp}`,
`tests/PluginTests.cpp`, `MANUAL.md`. Sgraffito needs no code change: the widget is already
shared.

1. **The widget already lives in the shared module.** `SuiteUI::PlayableKeyboardStrip`
   (`modules/suite_ui/include/SuiteWidgets.h:224`, `Source/SuiteWidgets.cpp:554-697`) already
   exposes `onNoteOn` / `onNoteOff` as `std::function`s, and Sgraffito already uses it. There is
   no lift and no rename: Sgraffito's tests reference the type
   (`findChildOfTypeDeep<SuiteUI::PlayableKeyboardStrip>`), so the name stays. What is missing is
   **hardening for the note-off guarantee**: the widget implements only `mouseDown`, `mouseDrag`
   and `mouseUp` — there is no `mouseExit`, no held-note tracking and no destructor flush, so
   three of the guarantee's ending paths have no mechanism in either consumer. Extend the widget:
   held-note state, a release on `mouseExit`, and a release when the widget is destroyed (or an
   accessor the editor flushes before destruction). Each with a SuiteUITests case. Sgraffito's
   behaviour is unchanged (its mouse-up path already releases).
2. MidiTool processor: add the Sgraffito-style **SPSC UI-note ring**. `pushUiNote (int key, bool
   on, float velocity)` is drained at the top of `processBlock` and merged into the MIDI path, so
   UI notes run the same chord and arp logic as host notes.
3. Editor: replace the drawn mini keyboard with `SuiteUI::PlayableKeyboardStrip`; clicking a key
   sends the note.
   The arp strip's step cells (or the chord cell) become clickable: a click triggers the current
   chord through the same queue.
4. **Note-off guarantee (owner decision 9).** Every UI note-on has a matching note-off, and every
   ending path sends it:
   - mouse-up on the key, and a drag that leaves a key (the previous key's note is released before
     the next one starts);
   - the mouse leaving the window while a key is held;
   - the editor closed or destroyed while a key is held;
   - focus lost while a key is held;
   - transport stop, and processor reset or `releaseResources`.

   Implementation: the UI keeps a set of held UI notes. Every ending path pushes the matching
   `on = false` events, and the processor also flushes all UI-held notes on transport stop and
   reset (a "release all UI notes" message in the ring). Host notes are not touched. The processor
   therefore keeps the set of notes it started from the UI ring, and the flush releases exactly
   that set. The chord and
   arp path already truncates its own notes (AGENTS §5), so the guarantee covers UI notes only.
5. Tests: processor — `pushUiNote` through `processBlock` produces the same output events as the
   equivalent host MIDI. Editor — mouse-down and mouse-up on a key inject notes (the 15-argument
   `MouseEvent` form). **One test per ending path** (mouse-up, drag off the key, mouse leaving the
   window, editor destroyed, focus lost, transport stop, reset). Each asserts that note-ons equal
   note-offs and that no UI note is sounding afterwards. A randomised click sequence with a fixed
   seed checks the same balance. SuiteUITests covers the new widget paths (held-note release on
   `mouseExit`, release on destruction), and Sgraffito's existing keyboard tests pass unchanged
   (no behavioural change there).
6. Manual: MidiTool keys and chord cells play through the plugin's own chord and arp logic, and any
   held key is released on every ending path.

### Phase 8 — Documentation, renders, verification

Files: `AGENTS.md`, `README.md`, `CHANGELOG.md`, `docs/manuals/*.png`, `plugins/*/MANUAL.md`,
`docs/plans/README.md`.

1. Manual screenshots regenerated for every touched editor (R9/R11):
   `SUITE_RENDER_DIR=<repo>/docs/manuals`. MultiSat (scales, meters, SOUND and MOD pages),
   StereoTool (new layout, the stereo meter pair, MOD page), MidiTool (playable keyboard). Cut
   detail images with
   `tools/manuals/crops.py` where they help; re-run `tools/manuals/scale_images.py` if the set
   changes.
2. Manual prose: MultiSat (two pages, meter layout), StereoTool (modulation, layout, the Side
   Crossover note), MidiTool (clickable keys and chords, the note-off rule, the instrument type).
3. `AGENTS.md`: §1 table (MultiSat and StereoTool preset counts, MidiTool name), §4.5 (MultiSat 45
   parameters, target table, two pages), §4.7 (StereoTool 47, target table), §5.5 (preset counts,
   `EXPECTED_TOTAL` 200), §2 layout (`StereoLevelMeter`, `PlayableKeyboard`), §6 test counts, §7
   decisions (knobs untouchable, meter placement rule, note-off guarantee, MidiTool naming), §9 the
   modulation procedure applied twice more.
4. `README.md`: parameter tables for MultiSat and StereoTool; preset count lines.
5. `CHANGELOG.md`: one entry per finding, in the owner's words.
6. Verification: clean Release build (0 errors; the warning count from Phase 0 is the baseline);
   `ctest` 28/28 with `SUITE_PRESET_BANK=ON` (entry count unchanged); `python tools/build_bank.py`
   and `python tools/audit_bank.py` (200 presets); before and after renders under
   `build/renders/`; `git diff --stat` and `git status --short` (AGENTS §15). `git diff` shows no
   change to `LabelledSlider`, `knobDiameter` or knob drawing.

---

## 3. Open points and risks

| Item | Note |
|---|---|
| StereoTool width 720 | Row 1 carries LEVEL (2 knobs) and IMAGE (5 controls). Verify against renders before fixing the constant; 760 (MultiSat's width) is the fallback |
| MultiSat SOUND page | The visualiser at its new height and the five-knob SHAPER row must fit 760×700 on the SOUND page. Verify in Phase 3 renders |
| StereoTool smoothing | Mix and Drive are not smoothed today (§0.3). Phase 6 item 3 classifies every routed target before wiring it |
| MultiSat crossover | Per-block LR4 coefficient steps click when modulated (Phase 4 item 3). Smooth the cutoff in Hz before the coefficients are computed |
| Speed and sync | Not in this plan (plan 14) |
| UI note-off | Phase 7 item 4 is a guarantee, so every path needs its own test. A missing path is a stuck note, which is a release blocker |
| MidiTool standalone | The standalone build has no MIDI input device guarantee; the clickable keyboard is also the way to play it standalone, which is the point of the feature |
| PlayableKeyboard hardening | The widget gains held-note tracking, a `mouseExit` release and a destructor flush (Phase 7 item 1). Sgraffito's behaviour stays unchanged; its keyboard tests are the regression |
| Knobs untouchable | Any phase that seems to need a knob change must stop and ask. Knob size and drawing are frozen (R1, owner decision 14) |
| `_routing` shape fix | Bumping the schema max to 7 is a pure relaxation; no generated file changes until a preset uses Pendulum |
| Preset program list | +40 programs across the four plugins under `SUITE_PRESET_BANK_AS_PROGRAMS`; all appended, order stable |
| Renders on CI | Editor tests need a desktop session (AGENTS §10). Report session-related failures; do not "fix" them |
| macOS / AU / DAWs | Not verifiable here; report as unchecked |

---

## 4. Verification checklist

1. Phase 0 baseline (clean build, warning count, before renders), then a clean Release build at
   the end: 0 errors, warning count compared with the baseline.
2. `ctest --test-dir build -C Release --output-on-failure`: 28/28 with the bank on. A changed entry
   count is a changed test set; report it.
3. `python tools/build_bank.py` then `python tools/audit_bank.py`: 180 → 190 → 200 across the phases.
4. Before and after renders for every editor whose layout, colour, scales or wording changed, under
   `build/renders/` (AGENTS §15.7).
5. `git grep` after the naming change: no "MidiEffect" in user-facing prose (folder paths are the
   only permitted occurrences). After Phase 7: the keyboard widget lives once in `suite_ui` and no
   editor has its own keyboard implementation.
6. Knob check: `git diff` shows no change to `LabelledSlider`, `knobDiameter` or knob drawing.
7. `git status --short`: nothing untracked except the deliberate new plan file.
8. Not checked here: macOS/AU, standalone, DAW loading, the owner-side lockup regeneration.

---

## 5. Owner gates

| Gate | When |
|---|---|
| Approve the StereoTool final width after the Phase 5 renders | Phase 5 |
| Review the regenerated manual screenshots | Phase 8 |
