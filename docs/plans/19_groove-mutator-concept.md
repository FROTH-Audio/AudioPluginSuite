# Plan 19: Groove Mutator — a generative rhythm-MIDI instrument (concept)

Written for: the owner (approve/steer the concept) and the coding agents who will build it.
Read `AGENTS.md` first. Status: **concept, 2026-11-23 — nothing implemented.** The owner's
ask: "a creative new plugin that creates rhythm-midi output to creatively play and
sequence drums/percussion/rhythm lines."

Naming: MidiTool = `plugins/MidiEffect` (AGENTS §4).

---

## 1. The idea in one sentence

**Groove Mutator** is a MIDI effect (output-only, like the Micro-Timing Chaos Sequencer)
that generates and endlessly mutates **drum/percussion rhythm patterns** — eight
independent percussion tracks driven by euclidean grids, hashed patterns and Lorenz-chaos
evolution — so one finger on one held note plays a living, never-repeating-yet-musical
groove line that you can freeze, mutate and take over.

## 2. Why it fits the suite

| Suite asset | How the Mutator reuses it |
|---|---|
| `CoreDSP::Chaos::LorenzAttractor` (RK4) | The mutation driver: X/Y/Z drift controls which steps mutate, how velocities drift, and when fills erupt — the same "same seed, same chaos" determinism as MTC |
| `CoreDSP::Sequencer` (hashed patterns, swing, gate, velocity, step rates) | The pattern and timing maths; swing/gate/humanise helpers shared, not copied |
| `SuiteModulation::Router` | Two LFOs + six routing slots onto the Mutator's own parameters (chaos, density, fill probability, per-track accent) — R8/R14 pages as in every plugin |
| `SuitePresets` (R12) | ≥ 15 factory presets, each a complete groove: "909 Acid", "Broken Drill", " Afro Cog", "Glacier Trap", "Euclid Wave" … |
| R1–R14 UI contract | Group frames, HelpLine, LabelledSlider knobs, pills, hamburger + SETTINGS, preset bar, per-page renders |
| The MTC latency/notes-out pattern | Fixed reported latency for timing drift, note-off guarantees on every ending path (the plan-13 MidiTool note-off rule, one test per path) |

New DSP (one new CoreDSP header, `CoreDSP::Rhythm`): a **Bjorklund euclidean engine** and
a **GrooveEngine** track mixer. Both pure, header-only, deterministic from a seed — the
DSP tests can assert exact step sequences.

## 3. The rhythm brain

### 3.1 Eight percussion tracks
Each track is one drum voice lane and owns:

| Control | Range / default | What it does |
|---|---|---|
| On | bool, on | The lane fires at all |
| Pattern | Euclid / Hash / Steps | Euclid grid, the Sequencer's hashed pattern, or a hand-drawn 16-step grid |
| Steps | 1–32, default 16 | Pattern length in steps |
| Pulses | 0–steps | Euclid only: E(pulses, steps), rotation-adjustable |
| Rotation | 0–steps | Rotates the euclid pattern (the classic "xoxo → oxox" move) |
| Division | 1/32…1 bar | The lane's own step rate — **polyrhythm per track** (a 5-step lane against a 16-step lane) |
| Note | 24–96 | The MIDI note out (GM drum map defaults: 36 kick, 38 snare, 42 closed hat…) |
| Velocity | 30–150 % | Base velocity before accents/humanise |
| Accent | 0–100 % | Every pattern's accents hit harder; probability of accent steps |
| Flam | 0–100 % | Probability of a 1/64 flam double-hit on a step |
| Ghost | 0–100 % | Probability of ghost-note velocity dips |

≈ 12 parameters × 8 tracks + the master block (below) → about 96 + 12 parameters,
per-slot-array IDs in the Cyclical/audit form (`track1Steps` …), append-only forever.

### 3.2 The mutation engine (the "Mutator" part)
- **Chaos amount (0–100 %)**: how far the Lorenz attractor is allowed to rewrite the
  pattern. At 0 the groove is a locked, repeatable pattern (seeded). At 100 % every bar is
  rewritten: steps migrate, accents flip, ghost notes bloom and die.
- **Evolve rate**: mutate every beat / every bar / every 2/4/8 bars; the attractor advances
  per musical time exactly like MTC's per-step Lorenz advance, so the same seed + tempo
  gives the same evolution at any sample rate.
- **Mutation scale**: from "swap two steps" to "redraw the whole grid" — the chaos value
  selects the mutation operator tier (drift, swap, split/merge pulses, accent flip,
  density morph).
- **Freeze button**: locks the current pattern for as long as held (performance control);
  a parameter (`freeze`) so it is automatable and presettable.
- **Fill engine**: on the last bar of the cycle, fill probability ramps; the fill style
  (tom roll, snare build, hat drop, chaos burst) is a choice. Fill is an LFO/routing
  target so it can be triggered externally.

### 3.3 The groove feel
- **Swing** −50…+50 % (bipolar, the MTC law) + **Humanise** timing/velocity (±ms, ±velocity,
  probability) — both reuse `CoreDSP::Sequencer` maths.
- **Velocity curve** Flat/Linear/Exponential/Random per the MidiTool pattern.
- **Polyrhythm master ratio**: an optional global "scale all lane divisions" knob
  (1:1, 3:4, 5:4, 7:8 …) for metric modulation without touching eight knobs.

### 3.4 Playing it
- **Host Sync** (default on): lanes align to the DAW grid; lanes with different divisions
  stay phase-locked to the bar. Host Sync off runs on the Tempo parameter (the MTC pattern).
- **Trigger key** (one note, default C2/B0 range): note-on starts the engine, note-off is
  the Freeze-off/choke boundary; in **Latch** mode one press runs until a second press.
  The trigger key is a parameter (the Cyclical `slotNNote` pattern).
- **Output**: MIDI notes only, output-only buses, audio buffer cleared (the MidiEffect
  precedent). Stuck notes are impossible: every note-on gets its note-off on every ending
  path — transport stop, mode change, preset load, editor destroyed, reset.

## 4. The interface (one owner-gated sketch)

Window ≈ 1000 × 720, tangerine-family accent to be chosen by the owner (each plugin owns
its accent). Pages:

- **GRID** — the eight lanes as horizontal step rows (accent/ghost/flam coloured dots over
  a shared step grid), the playhead, the lane strip (On, Division, Note, Velocity) per row,
  and the hand-draw mode for Steps-pattern lanes.
- **MUTATE** — the chaos group (amount, evolve rate, scale, freeze), the fill group
  (probability, style, last-bar length), the Lorenz trail plot (the MTC attractor visual,
  shared style) and the seed control.
- **GROOVE** — swing, humanise (timing/velocity/amount), velocity curve, the polyrhythm
  master ratio, and the trigger key.
- **MOD** — the shared router panel (with the plan-18 transform row).

## 5. Build recipe (R2: about one day of scaffolding + DSP + tests)

1. `python tools/new_plugin/new_plugin.py GrooveMutator "Groove Mutator" GRVM` (the code is
   checked unique by `suite_add_plugin`; the skeleton carries R11/R12/R13).
2. `suite_add_plugin(..., NEEDS_MIDI_INPUT, NEEDS_MIDI_OUTPUT, IS_MIDI_EFFECT, MODULATION,
   SETTINGS)` + `add_subdirectory` + `suite_add_host_test` (AGENTS §9 "Adding a plugin").
3. `CoreDSP::Rhythm` header (euclid + groove) + `RhythmTests` in `DSPTests.cpp`: exact
   E(3,8), E(5,16), rotation, determinism from seed, mutation-tier bounds.
4. Plugin tests: parameters/count/ranges, state round trip (fixtures), the note-off
   guarantee per ending path, host-sync grid alignment, latency (fixed, the MTC rule),
   editor renders per page, the preset bar in the tree.
5. ≥ 15 factory presets authored in `tools/preset_data.py`; bank audit; the manual with
   renders; README/AGENTS/CHANGELOG.

## 6. Risks / open points

| Item | Note |
|---|---|
| Parameter count (~108) | Larger than MTC (66) — per-track array IDs keep the bank schema and tests manageable; append-only from day one |
| Polyrhythm vs the shared step grid | Each lane's division runs on its own beat-clock phase — the alignment maths needs its own tests (the MTC synced-LFO pattern generalises) |
| Mutation determinism vs "never repeats" | Chaos above 0 with S&H-style seeded draws gives drift; the owner decides how much non-repeating is wanted at the top end |
| MIDI output in the DAW | Same scanner caveats as MTC (output-only buses, pass-through bus declared); the VST3 host test covers load |
| macOS / AU / Standalone / DAW | Not verifiable here — CI covers Windows/macOS builds |
| Name/code | "Groove Mutator" / `GRVM` — owner may rename; the folder/target/preset ID follow `new_plugin.py` |
