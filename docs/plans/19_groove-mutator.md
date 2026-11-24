# Plan 19: Groove Mutator — implementation plan

Written for: the coding agents who carry out the phases. Read `AGENTS.md` first.
Concept approved by the owner 2026-11-23. Source: `19_groove-mutator-concept.md` (§1–6
there define the idea; this file is the build order). Naming: MidiTool =
`plugins/MidiEffect`.

Status: approved concept; nothing implemented. Execution starts after plan 20 closes (the plan-18
Cyclical phases 4–6 landed, and the modulation panel's transform row is fixed in plan 20 Phase 1),
so the shared pattern maths and the modulation panel's transforms are landed.

---

## 0. Product definition (fixed points)

| Item | Decision |
|---|---|
| Name / folder / code | Groove Mutator / `plugins/GrooveMutator` / plugin code `GRVM` (unique-code check runs at configure) |
| Kind | MIDI effect: output-only buses, notes out, audio buffer cleared (the MidiEffect precedent), stereo pass-through bus declared |
| Formats | VST3 + AU + Standalone via `suite_add_plugin` |
| Manufacturer / IDs | FROTH Audio, `com.audiosuite.groovemutator`, preset ID `GrooveMutator` |
| Trigger | One trigger key (default 36/C2), Latch or Gate mode (the Cyclical `triggerMode` pattern) |
| Clock | Host Sync on = host PPQ + tempo (the MTC law, `SuiteModulation::hostTempoOrDefault` fallback); off = the Tempo parameter |
| Latency | Fixed reported latency for the timing-drift budget (the MTC 50 ms pattern, `maxTimingRangeMs`), never changing with a knob |
| Parameters | 159 (master 17 + 8 tracks × 12 + the shared modulation router’s 46); per-track array IDs (`track1Steps` …, the Cyclical/audit form), **append-only from day one**; state version 1 |

## 1. Parameter layout (createParameterLayout order — never reorder after first commit)

Master block (appended in this order, then the eight tracks, then the router):

| ID | Name | Range | Default | Notes |
|---|---|---|---|---|
| `sync` | Host Sync | off/on | on | |
| `tempo` | Tempo | 40–240 BPM | 120 | Used while Host Sync is off |
| `triggerMode` | Trigger Mode | Latch, Gate | Latch | |
| `triggerNote` | Trigger Key | 24–96 | 36 | `C2 (36)` text (the Root-Key pattern) |
| `swing` | Swing | −50…+50 % | 0 % | Bipolar, the MTC law |
| `humaniseTime` | Humanise Time | 0–20 ms | 2 ms | ± timing jitter |
| `humaniseVelocity` | Humanise Velocity | 0–100 % | 10 % | |
| `velCurve` | Velocity Curve | Flat, Linear, Exponential, Random | Flat | The MidiTool pattern |
| `polyRatio` | Polyrhythm | 1:1, 3:4, 5:4, 7:8, 4:3, 8:7 | 1:1 | Scales every lane's division (metric modulation) |
| `chaos` | Chaos | 0–100 % | 25 % | The mutation input (drives the lerp-curve equivalent) |
| `evolveRate` | Evolve Rate | per Beat, per Bar, 2 Bars, 4 Bars, 8 Bars | per Bar | Mutation tick in musical time |
| `mutateScale` | Mutation Scale | 0–100 % | 50 % | Picks the mutation-operator tier |
| `freeze` | Freeze | bool | off | Locks the current pattern (automatable) |
| `fillProb` | Fill Probability | 0–100 % | 15 % | Ramp on the last bar of the cycle |
| `fillStyle` | Fill Style | Tom Roll, Snare Build, Hat Drop, Chaos Burst | Tom Roll | |
| `fillBars` | Fill Length | 1, 2, 4 Bars | 1 Bar | The last-N-bars window |
| `seed` | Seed | 0–999 | 42 | Determinism anchor (the MTC seed) |

Per track ×8 (`track1…track8`, array IDs in the Cyclical form, defaults below are GM drum
map: 36 kick, 38 snare, 42 chh, 46 ohh, 41 low tom, 45 mid tom, 39 clap, 75 cb/claves):

| ID | Name | Range | Default (track 1; N = 1…8 offsets where noted) |
|---|---|---|---|
| `trackNOn` | On | bool | on for 1–4, off for 5–8 |
| `trackNPattern` | Pattern | Euclid, Hash, Steps | Euclid |
| `trackNSteps` | Steps | 1–32 | 16 |
| `trackNPulses` | Pulses | 0–32 | 4 (track 1), varied 2–7 for the defaults so the INIT groove is musical |
| `trackNRotation` | Rotation | 0–32 | 0 |
| `trackNDivision` | Division | 1/32, 1/16T, 1/16, 1/8T, 1/8, 1/4, 1/2, 1 Bar | 1/16 |
| `trackNNote` | Note | 24–96 | 36 + (track−1) mapped over the GM set above |
| `trackNVel` | Velocity | 30–150 % | 100 % |
| `trackNAccent` | Accent | 0–100 % | 30 % |
| `trackNFlam` | Flam | 0–100 % | 0 % |
| `trackNGhost` | Ghost | 0–100 % | 10 % |
| `trackNGain` | Level | −24…+12 dB | 0 dB | Sum stage before the (virtual) balance — tracks only emit notes; the level scales the note velocity contribution |

Then `SuiteModulation::Router::addParameters` (34 + the 12 plan-17 transforms, appended
last, exactly as every other modulation plugin).

## 2. CoreDSP::Rhythm (one new header-only module, `modules/core_dsp/include/RhythmEngine.h`)

```cpp
namespace CoreDSP::Rhythm {
    /** Bjorklund's algorithm as a bitmask: bit k = step k fires. E(0,n) = 0. */
    std::uint32_t euclidPattern (int pulses, int steps, int rotation);   // steps ≤ 32

    struct LaneParams { bool on; int patternKind; int steps, pulses, rotation;
                        float divisionBeats; int note; float velocity,
                              accent, flam, ghost, gain; };

    struct LaneState { std::uint32_t mask; int lastStepFired; /* … */ };

    /** The mutation operators, tiered by mutateScale: 0 drift, 1 swap, 2 split/merge
        pulses, 3 accent flip, 4 density morph. Applied on a musical tick from the
        attractor's hash, deterministic from (seed, tick, lane). */
    std::uint32_t mutatePattern (std::uint32_t mask, int steps, float scale,
                                 std::uint32_t hash);

    /** One scheduling pass over one block; fills and humanise ride on Sequencer helpers. */
}
```

Rules:
- All functions pure and deterministic from `(seed, musical position)` — the DSP tests
  assert exact sequences (no GUI, no host).
- The mutation tick rewrites the lane's mask copy, never the parameter state: the pattern
  the parameter defines is the *base*, the engine keeps the *live* mutated mask, and a
  Seed change / preset load / Freeze-off re-derives it.
- Fills ride on top of the base+mutated pattern for the last `fillBars` bars of the cycle.

## 3. Phases

### Phase 0 — Skeleton (R2, about an hour)
1. `python tools/new_plugin/new_plugin.py GrooveMutator "Groove Mutator" GRVM`.
2. Root `CMakeLists.txt`: `add_subdirectory(plugins/GrooveMutator)` after the other
   plugins (the script prints the line).
3. `tests/host/CMakeLists.txt`: `suite_add_host_test(GrooveMutator)`.
4. The CMake call: `suite_add_plugin(..., NEEDS_MIDI_INPUT, NEEDS_MIDI_OUTPUT,
   IS_MIDI_EFFECT, MODULATION, SETTINGS)` — **one flag per line** (AGENTS §11, the
   bare-keyword misparse).
5. Lockup: add the row to the owner's `tools/brand/lockups.py` table (owner-local tool —
   ask the owner to run it; `LOCKUP` argument points at
   `modules/suite_ui/brand/lockup_groovemutator.png`).
6. Verify: configure + build the empty skeleton, CTest gains 3 entries
   (PluginTests, PresetTests — after Phase 5, VST3_HostLoad).

### Phase 1 — CoreDSP::Rhythm + DSP tests
Files: `modules/core_dsp/include/RhythmEngine.h`, `modules/core_dsp/tests/DSPTests.cpp`.
1. Euclid: exact E(3,8) = x..x..x., E(5,16), E(2,16), E(7,12) asserted bit for bit;
   rotation cycles; E(0,n) empty; E(n,n) full.
2. Mutation tiers: at scale 0 the mask is unchanged; tier bounds (a mutation can never
   push pulses outside [0, steps]); determinism from (seed, tick, lane).
3. Lane scheduling: a lane with division D fires at the (hash-ordered) euclid steps over
   its own beat-clock phase; accent/flam/ghost probabilities are seed-deterministic.
4. Fill: the last bar's added hits never collide with an existing hit's note-off.

### Phase 2 — Processor: engine, sync, MIDI out
Files: `plugins/GrooveMutator/Source/PluginProcessor.{h,cpp}`.
1. Layout per §1; `numControls` = master(17) + 8×12 + router(46) = 159… **recount at
   build time and record the real number in AGENTS §4** (the count is whatever the layout
   creates; the test asserts it).
2. The transport follows the MTC block start: host PPQ resync (jump > 1e-3 beats resets
   the step grid), the free tempo fallback, a stopped transport releases every sounding
   note and clears the held-trigger state.
3. Scheduling: per block, while a step boundary lies inside the block, queue note-on/off
   with `max(streamSample, gridSample + swing + humanise + latency)` (the MTC queue
   rules); flams schedule a second note-on +6…+12 ms after the first; ghost notes are
   velocity, not extra hits.
4. **The note-off guarantee (plan-13 rule, one test per path)**: mouse/transport stop,
   trigger release, mode change, preset load, `setStateInformation`, reset,
   `releaseResources`, editor destroyed — every sounding note receives its note-off.
5. Latency fixed at 50 ms equivalent (`setLatencySamples` in prepareToPlay).
6. Stereo pass-through: audio copied untouched (the MTC invariant, tested).

### Phase 3 — Editor: GRID / MUTATE / GROOVE pages
Files: `plugins/GrooveMutator/Source/PluginEditor.{h,cpp}` + a `LaneView` widget in the
plugin (not SuiteUI — one consumer).
1. Header: lockup, preset bar (R12), page switch (R8), hamburger (R13); footer HelpLine
   (R14a) with `SuiteUI::describe` on every control.
2. GRID: eight `LaneView` rows over a shared step ruler — the mutated live mask (dots:
   full = hit, ring = ghost, double = flam, bright = accent), the lane strip's knobs per
   row (On pill, Division dropdown (8 options), Note, Velocity, Level), and draw mode for
   Steps lanes. The playhead follows the host beat.
3. MUTATE: chaos group (Chaos, Evolve Rate, Mutation Scale, Freeze pill, Seed), the fill
   group (Probability, Style dropdown, Length), and the attractor trail plot (the MTC
   Lorenz visual, shared style, fed by the published trail).
4. GROOVE: swing, humanise pair, velocity curve, polyrhythm dropdown, trigger mode +
   trigger key.
5. Affinity (G1): Steps-pattern lanes dim Pulses/Rotation; Euclid lanes dim nothing;
   Tempo dims while Host Sync is on; per-mode re-evaluated in the 60 Hz timer, one test.
6. R9 renders: every page, twice per process, byte-compared; the window size is measured
   and **owner-approved from the renders before Phase 4 closes**.

### Phase 4 — Modulation
1. The Router joins with the target table (append-only): Chaos, Evolve Rate, Mutation
   Scale, Fill Probability, Swing, Humanise Time, Humanise Velocity, Poly… + per-track
   Velocity, Accent, Ghost (pick ~12, the rest of the table is recorded in this plan
   before code). Excluded: divisions, notes, pattern kinds (structural).
2. MOD page with the shared panel — the plan-18 transform row arrives automatically.

### Phase 5 — Presets (R12)
1. ≥ 15 factory presets in `tools/preset_data.py`, ≥ 3 categories; every value inside the
   parameter's `NormalisableRange`; no preset equal to the default state; the trigger key
   and lanes so each preset is playable from one held note.
2. `build_bank.py` + `audit_bank.py` (the authoring schema picks the new IDs up from the
   sources — the per-track array form is what the coverage check parses).
3. `GrooveMutator_PresetTests` lands via the skeleton.

### Phase 6 — Plugin tests + host test
`plugins/GrooveMutator/tests/PluginTests.cpp`: parameters/count/ranges, state round trip
with every parameter moved, the refusal + fixture tests, note-off per ending path,
host-sync grid alignment (a moving host position, the scheduled start sample asserted),
latency unchanged, pass-through untouched, the editor pages paint, affinity, and the
program-list virtuals under the programs flag. `PluginHostTests` loads the VST3.

### Phase 7 — Docs, bank, housekeeping
MANUAL.md (R11, full-page renders committed under `docs/manuals/`), README row + parameter
table, AGENTS §1/§4/§7/§13, CHANGELOG, `git status --short` sweep. CTest expectation: 28 →
31 entries (PluginTests, PresetTests, VST3_HostLoad added).

## 4. Risks

| Item | Note |
|---|---|
| Parameter count (159) | Per-track array IDs keep the bank/test maintenance sane; append-only from the first commit; Ableton rescan after every count change |
| Polyrhythm phase maths | Each lane runs its own beat-clock phase (the MTC synced-LFO generalisation) — its own test first |
| Mutation determinism | The live mutated mask must survive state round trips identically (seed + tick rebuild) |
| ~100 parameters on the GRID page | The lane strip must stay readable — the R1 40 px rule holds; if the page cannot fit, a second page is the R4 answer, not smaller knobs |
| MIDI output in DAWs | The MTC scanner caveat applies (output-only + pass-through bus); the host test covers load, a DAW check is the owner's |
| macOS / AU / Standalone | CI builds; manual verification stays with the owner |
