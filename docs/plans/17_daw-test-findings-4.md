# Plan 17: DAW-test findings 4 — Bouncing Ball, slot voice shaping, Cyclical key-map fix

Written for: the coding agents who carry out the phases. Read `AGENTS.md` first. Source of
record: `owner_findings.md` (round 4, 2026-11-22).

Naming: MidiTool = `plugins/MidiEffect` (AGENTS §4).

Status: proposed 2026-11-22. Phase 1 (the Cyclical key-map bug) is fixed and verified; the
rest awaits the owner's answers to §1.

---

## 0. Verified state

| Finding | Verified state |
|---|---|
| "Assigned slot-note does not work; each played/selected note plays slot 1 only" | **Bug, fixed 2026-11-22 (Phase 1)**: the single-pass key map let a chopped slot 1 (base 60, up to 16 slices → claims 60..75) shadow slots 2–4, whose exact notes (61/62/63) sit inside the range. The two-pass map now resolves exact Note matches first and chop ranges after |
| "Reverse, Start/Length seems to have no effect on Loop" | **Phase 2**: a playing voice captures its source and region at start, so REV/Start/Length apply from the next trigger — the owner expects them live. Fix: the transformation tools retrigger the slot's playing voice so REV is immediately audible; Start/Length follow at the next trigger (documented), or live (owner decides) |
| "Add pan knob and LFO-target" (StereoTool) | **Phase 3**: StereoTool has no pan; add a Pan knob (−100…+100 %, constant-power like Cyclical/MultiSat) after Output, and Pan as an 11th modulation target (append-only) |
| "Loop should play as long as note is triggered" | Gate mode plays exactly note-on..note-off today (tested). Needs the owner's clarification: which trigger mode was active, and what behaviour was expected |
| "Add Filter per slot" / "Add Attack/Release per slot" | Cyclical has no per-slot filter or envelope today; the LoopEngine voice renders the region raw with the velocity gain |
| "properly enable/disable the per-mode knob/button states" (Cyclical, MidiTool) | The plan-15 affinity shipped: Cyclical dims Speed (grain/sync/chop), Beats (chop), Slices/Slice Length (chop); MidiTool dims the seven arp-only controls in Chord mode. What still looks wrong needs the owner's specifics |
| LFO: "Replace Pendulum by Bouncing Ball" | Pendulum is shape index 7, appended 2026-11. No public release, so replacing the meaning of index 7 is safe (no migration) |
| LFO: "more possibilities to transform Waveform" | The shared LFO has 8 shapes (sine, triangle, saw, square, S&H, random, noise, pendulum) and an offset parameter. The transform set needs the owner's definition |
| StereoTool / MultiSat: "more interesting/unique/fancy" | Open design asks |

## 1. Open questions (owner)

**Answered 2026-11-22:**
- StereoTool interest features: **mono-maker + crossover, multiband width, M/S tone
  shaping, image rotation** (all four).
- MultiSat additions: **per-band mix, thermal drift, per-band fold** (no third band).
- LFO transforms: **invert/rectify, quantise + smooth, pulse width + skew** (no drive/warp).
- The Cyclical "loop never stops" was diagnosed from the report: a running voice survives
  mode/preset changes (Phase 2).

**Still open:**
1. **Cyclical Start/Length**: apply from the next trigger (documented), or retrigger on knob
   release (live)?

## 2. Phases

### Phase 1 — Cyclical key map (done 2026-11-22)
Two-pass `slotForNote`: exact Note matches first (a chopped slot 1 no longer shadows slots
2–4), chop ranges after (keys with no exact match). Regression test
`testChopRangeDoesNotShadowExactNotes`; the chop tests use range-only keys (65/67, or base
72); the Dr Rex Grid preset now uses disjoint ranges (slot 1 16@36, slot 2 8@64). The
manual's overlap rule changes: "an exact Note always wins; a chop range claims the keys no
other slot claims — a chop range reaching across another slot's base note does not shadow
it".

### Phase 2 — Bouncing Ball replaces Pendulum
Files: `modules/core_dsp/include/ModulationEngine.h`, DSP tests, the LFO scope drawing
(auto), docs.

Status: implemented and verified 2026-11-22 (CTest 28/28).

Shape index 7 is now the bouncing ball: `|cos(2π·4·p)| × (1−p) × 2 − 1` — the ball drops
from full height (+1), each of the four bounces lower than the last, and rests at the
bottom (−1) at the cycle's end, then re-drops every cycle at full amplitude. The shape
name in every plugin's LFO choice list is "Bouncing Ball" (`SuiteModulationRouter` /
`SuiteModulationPanel`). No migration (no release). The DSP tests assert the key
positions, the strictly descending hump peaks, and the cycle repetition.

### Phase 3 — Cyclical: tools apply audibly, mode/preset changes silence
Files: `plugins/Cyclical/Source/PluginProcessor.cpp`, tests.

Status: implemented and verified 2026-11-22 (CTest 28/28).

1. **A trigger-mode change and a preset/state load stop all voices and clear the toggle
   state** — the owner's "never stops, even if I change mode or preset": a running voice
   survived both (nothing stopped it but the transport in Toggle mode), so a loop started
   under Toggle kept looping after the mode read Gate, and every Gate press retriggered on
   top of it. The audio thread watches the trigger mode per block; a preset or state load
   raises an atomic command (`stopAllVoicesPending`).
2. **REV/NORM restart the slot's playing voice**: `installTransformedLoop` raises a per-slot
   atomic (`retransformPending`); the audio thread restarts the voice with the articulation
   it started with (channel, velocity, slice — recorded per slot in `startSlotVoice`).
   Gate slots wait for the next press (a restart with no held key would loop forever under
   Gate). TRIM touches no buffer, so it needs no retrigger.
3. **Start/Length**: the region is captured at voice start (stable playback); the knob
   applies from the next trigger. Still open: document only, or retrigger on knob release.
4. Tests (`testModeAndPresetSilenceAndAudibleTransform`): a toggle loop stops when the mode
   changes to Gate; a preset load (INIT) silences a running Latch loop; REV while looping
   installs the reversed buffer and keeps the voice sounding.

### Phase 4 — StereoTool pan knob and LFO target
Files: `plugins/StereoTool/*`, tests, presets schema.

Status: implemented and verified 2026-11-22 (CTest 28/28).

1. A **Pan** knob (−100…+100 %, default 0) appended after Output → numControls 47 → 48,
   no saved index moves. The law is a **balance with exact unity at centre** (an
   equal-power law would sit at −3 dB on both channels and break the plugin's
   bit-transparent identity contract): turning right attenuates the left channel by
   `cos(|pan|·45°)`, the kept channel stays at unity. 20 ms smoothing like the levels;
   the identity early-return requires Pan = 0.
2. **Pan is the 11th modulation target** (`ModTarget::Pan`, append-only);
   `ModTargetNames` gained "Pan"; the MOD page's ControlDisplay covers it.
3. The Pan knob sits in the LEVEL group's second row (beneath Input/Output).
4. Tests: centre is bit-transparent (compared sample-exact against the identity path),
   pan right attenuates the left by cos(45°) with the kept channel at unity, mirrored
   left. Range and ID checks updated.

### Phase 5 — Cyclical per-slot voice shaping + the GLOBAL mixer
Files: `plugins/Cyclical/*`, LoopEngine (if the DSP belongs there), tests, presets.
1. **Filter per slot**: a per-slot LP/HP filter (one cutoff knob + a type selector, or a
   cutoff + resonance pair — owner decides the knob budget) applied to the slot's output.
2. **Attack/Release per slot**: a per-slot envelope over the loop's gain (attack on
   trigger, release on stop), replacing the shared hard velocity gain for the slot.
3. **Level per slot + the GLOBAL mixer (owner addition, 2026-11-22)**: a per-slot Level
   (−24…+12 dB, default 0 dB, smoothed 20 ms) between the slot's voices and the master
   stage, with a **MIXER group on the GLOBAL page** — the four slot levels beside the
   master Output/Pan, so the balance is set in one place (the Level knobs do not take
   space on the SLOTS cards).
4. **Attack/Release preview (owner addition, 2026-11-22)**: the SLOTS card shows the
   envelope's shape — an attack/release trapezoid over the waveform view, the way
   Sgraffito's SOUND page lays the envelope preview of the displayed key's region over
   its waveform (plan 10 phase 3, decision G3). The preview follows the slot's Attack and
   Release knobs live, and the trigger moment marks where the attack starts.
5. **Knob size exception (owner addition, 2026-11-22; refined same day)**: the suite's
   40 px knob rule (R1) is relaxed **for the Cyclical SLOTS page only** — the per-slot
   knobs may render at **50 % size (20 px)** if that fits the added filter/A-R/level
   controls without growing the window, or the card's controls may split onto **a second
   SLOTS sub-page**. Pick whichever renders cleaner; the suite-wide 40 px rule stays
   untouched everywhere else, and any size change lives in the Cyclical page's own layout
   (the shared `LabelledSlider` is only resized from the page, never re-drawn). If the
   smaller knobs win, **the size applies to the whole SLOTS page** — every knob on the
   page, not a mixed 20/40 px layout.
6. `numControls` 57 → +3 or +4 per slot (filter, attack, release, level); appended so no
   index moves. The SLOTS cards gain the filter/A-R knobs; the levels live in the GLOBAL
   mixer.

### Phase 6 — Affinity audit round 2
Files: the Cyclical and MidiTool editors, tests.
1. MidiTool: Tempo also dims while Host Sync is on (inert); audit every control against the
   mode matrix.
2. Cyclical: audit each knob's enabled state per Chop × Sync combination again against the
   new features (Phase 5's knobs join the matrix).

### Phase 7 — LFO waveform transforms
Files: `modules/core_dsp/include/ModulationEngine.h` (the shape maths), the shared LFO panel
(a small transform strip), DSP tests, docs. The owner picked:
1. **Invert / rectify** — polarity toggles next to the Offset (upper-half / lower-half
   rectification); two bools per LFO, appended.
2. **Quantise + smooth** — quantise the shape to N steps per cycle (a stepper from any
   waveform) and/or round sharp corners; one knob (steps, Off at 0) + one smooth knob.
3. **Pulse width + skew** — width for the square/pulse reading of any shape and a skew that
   leans the shape left/right; one knob each.
All appended after the existing per-LFO parameters so no index moves. The scope's drawing
follows the transformed shape automatically (it draws the output).

### Phase 8 — StereoTool interest features
Files: `plugins/StereoTool/*`, `CoreDSP::Stereo`, tests, presets. The owner picked all four:
1. **Mono-maker + crossover** — everything below an adjustable crossover collapses to mono
   (the classic bass-mono tool); one knob.
2. **Multiband width** — a three-way split with per-band width (tight lows, wide highs).
3. **M/S tone shaping** — separate tilt/bell colour for the mid and the side channel.
4. **Image rotation** — a dedicated slow LFO that swings the image left-right, with its own
   visual.
Big architectural round: the width path gains the multiband split; the window/affinity and
the owner's render approval gate the layout.

Status: parameters landed 2026-11-22 (numControls 48 → 56); **the DSP was missing** — the
eight parameters existed but nothing read them in `processBlock`, and the routing table
lacked Pan + the eight targets. **Implemented and verified 2026-11-23 (plan 18 Phase 3,
CTest 28/28)**: mono-maker = a true side high-pass (0 = Off; the original 200 Hz default
would have broken the bit-transparent identity contract), multiband width = a two-filter
side split (200 Hz / 2 kHz), M/S tone = the shared `ToneControl` per channel, image
rotation = an orthonormal L/R plane rotation with an accumulated phase. Five new audio
regression tests; the window is now 720×858 (render awaiting the owner's R4 approval).

### Phase 9 — MultiSat interest features + 10 presets
Files: `plugins/MultibandSaturation/*`, tests, presets. The owner picked:
1. **Per-band mix** — blend each band's saturation separately (parallel per band).
2. **Thermal drift** — the drive reacts slowly to the signal (a soft attack on the
   saturation itself) plus a subtle wow on the crossover.
3. **Per-band fold** — the Dynamic Saturation wavefolder per band instead of shared.
4. **+10 presets** focused on the new controls (the owner's round-4 general finding also
   asks for +10 each on StereoTool and Spectral Drift, focused on their modulation
   routing — those two are Phase 11).

### Phase 10 — Documentation, renders, verification
Manuals, README, AGENTS, CHANGELOG, renders, clean build, 28/28 CTest, bank audit.
Runs before Phase 11 if the preset round lands last, or alongside the phase that changed the
docs (whichever executes, the other's doc updates fold into it).

### Phase 11 — +10 presets each for StereoTool and Spectral Drift
Files: `tools/preset_data.py`, `tools/audit_bank.py`, the generated bank, the preset tests.
The owner's round-4 general finding: **10 more presets each, focused on the new controls**
(for StereoTool and Spectral Drift; MultiSat's join Phase 9). Append-only; the bank grows
210 → 240 if all three land here.

### Phase 12 — Housekeeping: plans and files
Files: `docs/plans/*`, `docs/plans/README.md`, stray files.
1. **Delete the finished plans**: 12, 13, 14, 15 and 16 are all implemented and verified,
   and their owner gates are satisfied by the subsequent DAW-testing (window approved,
   lockups regenerated, screenshots reviewed). Their outcome lives in AGENTS.md,
   CHANGELOG.md and the code (AGENTS §14: finished plans are deleted, not archived). The
   rows leave `docs/plans/README.md`.
2. **Keep**: 17 (current), 01 (the macOS check), 02 (one owner decision), 05.2/05.3/05.4
   (the audit follow-ups), 06 (the idea catalogue), 6.1_review (reference).
3. **File sweep**: no stray one-shot scripts or temp files outside `build/` (the session's
   helper scripts were deleted as they were used; verify with `git status --short` and a
   scan of `tools/`), the CRLF warnings on the generated bank JSONs noted as
   build_bank.py's writer, not a defect.
4. Re-verify: CTest 28/28 after the deletions (plans are not in the build, but the sweep
   confirms nothing load-bearing was removed).

## 3. Risks and open points

| Item | Note |
|---|---|
| Shape index 7 replacement | Safe without a migration while no release exists; if a release happens first, the plan changes to append |
| Per-slot filter knob budget | The owner relaxed R1 for the Cyclical SLOTS page only: 50 % knobs (20 px) or a sub-page for the slot controls — whichever renders cleaner; if the smaller knobs win, the whole SLOTS page uses them (no mixed sizes); the suite-wide 40 px rule stays untouched everywhere else |
| Attack/Release vs the existing tails | The LoopEngine's release fade (5 ms) interacts with the per-slot release envelope — the longer one wins |
| The two-pass key map | Exact notes always win; a chop slice whose key is another slot's base note is unreachable — the user moves the chopped slot's Note (documented) |
| macOS / AU / DAWs | Not verifiable here |
