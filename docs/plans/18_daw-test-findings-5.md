# Plan 18: DAW-test findings 5 — transform UI, Cyclical tabs and audible regions, StereoTool IMAGE

Written for: the coding agents who carry out the phases. Read `AGENTS.md` first. Source of
record: `owner_findings.md` (round 5, 2026-11-23). Supersedes the open phases of plan 17
where the owner changed direction; plan 17 phases 1–4, 7 (DSP), 8 (params) and 9 (params)
are implemented and verified (CTest 28/28 on 2026-11-23).

Naming: MidiTool = `plugins/MidiEffect`, MultiSat = `plugins/MultibandSaturation`.

Status: proposed 2026-11-23. Every claim in §0 was checked against the code on 2026-11-23.

---

## 0. Verified state (checked against the code)

| Finding | Verified state |
|---|---|
| LFO transforms: "Can't see any changes" | **UI gap, confirmed**: the 12 transform parameters exist (Invert, Rectify, Quantise, Smooth, Pulse Width, Skew × 2 LFOs; `SuiteModulationRouter.cpp` lines 99–119), the router applies them (`lfoOutputAt`, lines 345–354), and the DSP + router tests pass. But **no editor or panel exposes them** — `SuiteModulationPanel` has zero references. The owner cannot turn them, so nothing can be seen or heard |
| Cyclical: "Attack/Release per slot currently (not visible)" | **Layout bug, confirmed**: the Phase-9 creative controls (filter, attack, release, level) exist as parameters and widgets, and `SlotCard::resized()` lays out four extra rows (PluginEditor.cpp lines 142–156) — but `cardContentHeight` (line 19) still sums only the two old knob rows + the pill row, so the creative rows render below the card's bottom and are clipped |
| Cyclical: "Start/Length/SliceLength/CHOP Slices/Beats have no effect" | Partially by design: the region is captured at voice start (plan 17 Phase 3), so Start/Length apply from the next trigger — the owner expects them live. Beats (sync rate), Slices/SliceLength (chop boundaries) may hold real DSP bugs under Host Sync — probe first (Phase 5) |
| Cyclical: "Loops must retrigger synchronically with DAW if HostSync active" | Quantise exists (plan 14) but its Host-Sync alignment needs a probe + fix + test (Phase 5) |
| MultiSat: "Crossover Hz marker doesn't sync when modulated" | **Bug, confirmed**: the preview draws the marker from the **raw** parameter (`PluginEditor.cpp` line 363 reads `getRawParameterValue (crossoverId)`), never the router's modulated value — although Crossover is modulation target 1 |
| StereoTool: "IMAGE group no effect at all" | Wiring **verified present** in `processBlock` (width → side gain, midFocus → mid/side gains, sideFilter → SVF on the side). At the defaults (Width 0 %, Mid Focus 0, Side Filter Off) nothing can happen **by design** (the identity contract). Needs the Phase-3 probe to find what the owner moved and why it stayed silent |
| Presets: "+10 each for DS, Sgraffito and MTC, focus on the new LFO waveforms" | New list (round 5). Supersedes plan 17 Phase 11 (which named StereoTool and Spectral Drift) — the owner's latest text wins; flagged in §1 |

## 1. Open questions (owner)

1. **Cyclical per-slot filter**: the Phase-9 filter is a type selector (Off / High Pass /
   Low Pass / Band Pass). The owner asks for **freq + reso knobs** — proposal: keep the
   type selector and add Cutoff (20 Hz–20 kHz, log) + Resonance (0.5–10) per slot; or
   LP-only cutoff/reso. Default: selector + 2 knobs.
2. **Cyclical without GLOBAL**: master Output/Pan/Pitch/Crossfade/Tempo/Sync/Quantize/Swing
   are single parameters. Proposal: every slot tab carries the slot's own controls **plus
   linked copies** of Quantize/Swing/Tempo/Sync/Pitch/Crossfade (four widgets bound to one
   parameter each, kept in step), and a slim master strip (Output/Pan) at the bottom of
   every tab. Owner approves the arrangement (R4: window size is owner-gated anyway).
3. **StereoTool Phase**: invert **both channels**, or the classic **left channel only**?
   Default: both (master polarity), simplest contract.
4. Carried from plan 17: **Cyclical Start/Length live or next-trigger** — the round-5
   finding ("should modify how the loop is played") reads as *live*; default: retrigger the
   slot's voice on knob release (the plan-16 retransform pattern).

## 2. Phases

### Phase 1 — Transform controls in the shared LFO strips (the visibility fix)
Files: `modules/suite_modulation` (panel + scope), the six editors' MOD pages, tests,
renders.
1. Each LFO strip in `SuiteModulationPanel` gains a transform row: **Invert** and
   **Rectify** as pills (R3/R6: state dot), **Quantise** (0–16, off at 0), **Smooth**
   (0–1), **Pulse** (0–1), **Skew** (−1…+1) as suite knobs. All are plain APVTS parameters
   — `SliderAttachment`/`ButtonAttachment` only, no new parameters.
2. `ModulationPanel::getPreferredHeight()` grows by the row; every MOD page (SD, DS, MTC,
   Sgraffito, StereoTool, MultiSat) grows with it. **R4 gate: the owner approves each
   window height from the renders before the phase closes** (R1 stays untouched — standard
   40 px `LabelledSlider`s).
3. The LfoScope already draws the transformed output (publish stores `lfoOutputAt`) —
   verify, and the scope then shows invert/rectify/quantise/smooth/pulse/skew live.
4. Tests: the panel's preferred-height rule (children inside it), a parameter-existence
   check, and per-plugin editor renders (R9) with one transform set audibly different.

### Phase 2 — MultiSat: the crossover marker follows the modulation
Files: `plugins/MultibandSaturation/*`, tests.
1. The processor exposes the **modulated** crossover (the router's published value for
   `Target::Crossover`) through a small public accessor; the editor's paint reads it.
2. Test: with a routing aimed at Crossover, the accessor differs from the raw parameter;
   the marker position (log mapping, line 363–364) is checked against the accessor.

### Phase 3 — StereoTool: IMAGE probe, Phase control, MASTER rename
Files: `plugins/StereoTool/*`, tests, presets.
1. **Phase 0 probe first**: scripted sweeps of Width ±100 %, Mid Focus ±100, Side Filter
   HP + Side Crossover moves, compared against the committed output. If the knobs are
   silent in a DAW but audible in the probe, the finding becomes a defaults/affinity
   documentation fix; if silent in the probe too, fix the wiring and add a regression test
   per control.
2. **Phase toggle** (owner answer §1.3) appended after Pan → numControls 68 → 69, no
   saved index moves, default off keeps the identity early-return exact. 20 ms smoothing
   is not needed for a polarity flip, but the block-rate switch must be click-safe:
   apply per block with the identity short-circuit.
3. The **LEVEL group is renamed MASTER** (caption only; R14d naming pass). The Phase
   toggle joins it.
4. Tests: the Phase toggle inverts the samples exactly (± sign), centre identity holds,
   affinity unchanged; the ID/count/range checks updated.

### Phase 4 — Cyclical: tab-per-slot rework
Files: `plugins/Cyclical/*` (editor, processor layout), tests, presets, manuals, renders.
1. The SLOTS page becomes **four slot tabs** (SLOT 1–4); the GLOBAL page is removed.
   Every slot tab carries that slot's full set: Start, Length, Speed, Velocity, Note,
   Beats, Chop, Slices, Slice Length, Direction, Ping Rand, Grain **+ Filter type, Cutoff,
   Resonance + Attack, Release, Level** (the Phase-9 controls, now visible).
2. The global parameters (Quantize, Swing, Tempo, Sync, Pitch, Crossfade) appear as
   **linked copies** on every tab (four widgets, one parameter, kept in step through
   attachments); the master Output/Pan sit in a slim strip at the bottom of every tab
   (owner answer §1.2 decides the final arrangement).
3. **Filter params**: per the §1.1 answer, +2 per slot (cutoff/reso) appended → the
   Cyclical count grows; `audit_bank.py`'s authoring schema picks the new IDs up from the
   sources. The existing Phase-9 filter type selector keeps its ID.
4. The card-height bug dies with the rework: each tab lays out its own rows with real
   bounds; the envelope preview (plan 17 Phase 5 item 4) lays over the waveform.
5. Affinity matrix re-cut per tab (Chop × Sync × Filter-type × Grain), one test per tab
   (the plan-15 pattern).
6. **R4 gate: owner approves the new window size from the renders.** Bank + preset tests
   re-run; the Ableton rescan note applies again (parameter count changes).

### Phase 5 — Cyclical DSP: regions, chop and sync that answer
Files: `plugins/Cyclical/Source/PluginProcessor.cpp`, `CoreDSP::Loop` if the DSP belongs
there, tests.
1. **Phase 0 probes**: render a probe buffer per parameter while sweeping Start, Length,
   Slices, Slice Length, Beats (sync on and off) and compare against the expected
   region/rate maths; write the findings into this plan before fixing.
2. **Live Start/Length** (§1.4 default): a knob release retriggers the slot's voice with
   the new region (the plan-16 `retransformPending` pattern; Gate slots wait for the next
   press). Beats/Slices/SliceLength follow live the same way.
3. **Host-Sync retrigger alignment**: a trigger arriving while Host Sync is on starts on
   the quantise grid at the host beat clock, not mid-block; probe and fix, with a test
   that feeds a moving host position and asserts the scheduled start sample.
4. Regression tests per parameter: Start/Length move the region audibly; Slices/Slice
   Length change the chop boundaries (shared `chopBoundary`); Beats changes a synced
   slot's rate exactly.

### Phase 6 — Cyclical modulation page
Files: `plugins/Cyclical/CMakeLists.txt` (MODULATION), processor, editor, tests, presets.
1. Pass `MODULATION` to `suite_add_plugin`; the Router joins with a **per-slot target
   table** (append-only): Filter Cutoff, Resonance, Attack, Release, Level, Speed,
   Velocity, Start, Length (and beyond, owner's pick), the 2 LFOs and 6 routing slots.
2. A MOD page joins the tab bar (page 5) with the shared panel + ControlDisplay; the
   Phase-1 transform row arrives with it automatically.
3. numControls grows by the router's parameters (34 + the 12 transforms) + the chosen
   targets; the bank schema covers them through the authoring pass.

### Phase 7 — +10 presets each for DS, Sgraffito and MTC
Files: `tools/preset_data.py`, `tools/build_bank.py`, `tools/audit_bank.py`, the preset
tests. **Round-5 list**: DynamicSaturation, Sgraffito, MicroTimingChaos — focused on the
new LFO waveforms (Bouncing Ball) and the transforms (invert/rectify/quantise/smooth/
pulse/skew through the routing). Append-only; bank 210 → 240. The plan-17 Phase-11 list
(StereoTool + SD) is superseded — §1 flags the change for the owner's confirmation.

### Phase 8 — Documentation, renders, verification
Manuals (R11), README parameter tables (the new counts: SD 60, DS 59, MTC 66, Sgraffito
83, StereoTool 68+1, MultiSat 57, Cyclical per Phase 4/6), AGENTS §4/§7, CHANGELOG, the
R9 renders for every changed editor, clean build, 28/28 CTest, bank audit.

### Phase 9 — Housekeeping
Delete the finished plans (12–16 rows out of the README index), update 17's status, the
file sweep (`git status --short`; the session's `run_ctest.bat` / `build_incremental.bat`
helper scripts are the owner's call — keep or delete), re-verify CTest 28/28.

## 3. Risks and open points

| Item | Note |
|---|---|
| Six window heights grow (Phase 1) | R4 owner render approval per plugin; the transform row is the last addition to the strips |
| Cyclical rework is the biggest phase | Do Phase 5's probes before Phase 4's layout lands, so the tab design bakes in the live-behaviour semantics |
| Parameter counts change again | DAW rescan (Ableton cache, AGENTS §10) after Phases 3, 4, 6 |
| Phase toggle vs the identity contract | Off = bit-transparent, tested against the identity path |
| Linked widgets (one parameter, four tabs) | Attachment bookkeeping must not fight the host automation; test automation through one copy reflects in the others |
| macOS / AU / DAWs | Not verifiable here |
