# Plan 21: Groove Mutator — review findings and fix plan

Written for: the coding agents who carry out the phases. Read `AGENTS.md` first.

Source: a full implementation review of `plugins/GrooveMutator` on 2026-11-25 (processor,
editor, LaneView, `CoreDSP::Rhythm`, tests). The review ran **against the uncommitted
working tree**, which carries an in-flight change: `IS_MIDI_EFFECT` → `IS_SYNTH` in
CMake, a new `play` parameter (master 17 → 18, `numControls` 160) and a re-layout of the
GRID/LANES/MUTATE pages (window 1000 → 1200 wide) recorded in the untracked
`groovemutator_owner_findings.md` (root). Every file/line reference below is against that
tree and must be re-checked before acting (plans move; so does the code).

Status: **Phases 0–4 executed and Phase 3 confirmed by the owner 2026-11-25**, verified
31/31 CTest and 19/19 GrooveMutatorTests; renders under `build/renders/plan21-fixed/` and
`plan21-c3/`. C3 is
resolved the owner's way: Steps lanes are exempt from mutation (the painted pattern is the
audible truth), fills stay live on them, and the LaneView draws a Steps lane's fill additions
as hollow dots (`laneFillMask`, the same message-thread read precedent as `laneMask`). Two
plan-19 bugs fell out of the C3 test and are fixed: the Fill Length choice index was passed
as a bar count ("1 Bar" = the default — disabled fills entirely; now
`CoreDSP::Rhythm::fillBarCount`, 1 << index), and **eleven 0–100 % parameter reads clamped
to [0, 1] before the /100**, so Accent/Ghost/Flam/Chaos/Mutation Scale/Fill Probability/
Humanise Velocity were all 0–1 % knobs (CHANGELOG has the list). Phase 3 is the owner's DAW
confirmation (the `IS_SYNTH` declaration loads and the stereo pass-through holds; recorded
in AGENTS §1). Still open: Phase 5 (MANUAL.md + its committed screenshots, and the root
findings doc; the AGENTS §1/§4.9/§6 rows are updated).

---

## 0. Findings summary (the review's evidence)

### A. Blocking

- **A1 — `play` parameter is half-wired and contains UB.** *(Resolved by the owner's
  2026-11-25 decision in Phase 1: the parameter is removed.)*
  `PluginProcessor.cpp:236-237`:
  ```cpp
  // play = apvts.getRawParameterValue (playId);  // TODO: fix type error
  play = static_cast<std::atomic<bool>*>(apvts.getRawParameterValue (playId));
  ```
  `getRawParameterValue` returns `std::atomic<float>*`; casting to `std::atomic<bool>*` is
  undefined behaviour (different size and representation). Worse, **nothing reads `play`**:
  `processBlock` computes `running = transportRunning && triggerActive` and never consults
  it. The editor's PLAY toggle (`PluginEditor.cpp:199`, a `ButtonAttachment` on `playId`)
  does nothing audible, and its comment ("shows whether the host is running the plugin")
  is wrong — it is an ordinary automatable parameter, not a transport read-out.

- **A2 — GRID page layout overflows.** `PluginEditor.cpp:386-424`. `gridGroupHeight`
  budgets `rulerHeight + 8 × knobRowHeight + gaps` (~700 px), but `resized()` gives the
  **top half** of the group to `rulerBounds` (which only paints a 14 px-scale ruler — the
  `rulerHeight` constant is never used in the layout) and squeezes all 8 lane rows
  (8 × 76 + 7 × 6 ≈ 650 px) into the remaining ~330 px. Rows ~4–7 are clamped to zero
  height and pile at the bottom edge. This is the plan-20 lesson repeated: "a strip's
  internal overflow shows only in renders". It also contradicts the owner findings doc,
  which says the **step grid occupies the upper half** — in the code the strips are laid
  into the control rows and the upper half is empty space with ruler lines.

- **A3 — `IS_SYNTH` may kill the documented stereo pass-through.** The CMake change
  (`IS_MIDI_EFFECT` → `IS_SYNTH`, flags one per line — the §11 misparse rule is followed)
  is presumably the MTC lesson applied (DAW scanners drop MIDI-effect classes). But a
  synth declares an output-only audio bus in most hosts: the pass-through invariant
  (`testPassThrough`, and AGENTS §4.9 "stereo pass-through") may not survive in a real
  DAW because hosts do not feed audio into a synth's input. Needs a decision plus a DAW
  check, not just the JUCE host test.

### B. Docs and tree hygiene (owner rules)

- **B1 — AGENTS.md §4.9 stale**: says 159 parameters (master 17), `IS_MIDI_EFFECT`,
  window 1000×960, 18 test functions. The tree now has 160 (master 18 with `play`),
  `IS_SYNTH`, window 1200×~818. Section 4.9's parameter table has no `play` row.
- **B2 — MANUAL.md + R11 renders stale**: the pages were re-laid out but the manual and
  its committed screenshots under `docs/manuals/groovemutator-*.png` were not regenerated
  (R9/R11: a UI change ships with before/after renders and updated screenshots).
- **B3 — `groovemutator_owner_findings.md`** sits untracked at the repo root. Per the
  plans README: a done finding's outcome belongs in AGENTS.md/CHANGELOG, not a loose doc.
- **B4 — `build_test/`** untracked at the root and not covered by `.gitignore`
  (`build/` and `cmake-build-*/` only). Build output belongs in `build/`.

### C. Minor

- **C1 — Encoding damage (AGENTS §15 check 3)**: `PluginProcessor.cpp` comments carry
  mojibake (`â€”`, `Ã—`, `Â§` — lines 17, 275, 337, 488, 771, 808, 835) and
  `tests/PluginTests.cpp` doubly-encoded ones (lines 213, 359). PowerShell-edit
  corruption; repair with the Edit tool only, keep UTF-8 + line endings intact.
- **C2 — Dead / misleading knob sizing**: the `reducedKnobDiameter` member
  (`PluginEditor.h:162`) is never used. The local `knobDiameter` constant
  (`PluginEditor.cpp:18`) only sizes *cells* — the knobs still render at the suite's
  40 px inside 64 px cells. The owner findings doc's "knobs reduced 30 % (40 → 28 px)"
  is not what the code does. Note R1: knob changes go through the owner; if the 28 px
  knobs are really wanted they need the explicit owner exemption (the plan-20 MTC
  30 px precedent), implemented where the knob is actually drawn.
- **C3 — Steps-mode display gap**: `runMutationTick` mutates **all** lanes' live masks,
  including Steps lanes, but LaneView shows the painted mask in Steps mode — audible
  mutations and fill additions are invisible there. Either exclude Steps lanes from
  mutation/fill (musical decision) or show the live mask.
- **C4 — RT-safety note**: note queuing `emplace`s into a `std::multimap` on the audio
  thread (node allocation). Bounded in practice and consistent with the MTC precedent;
  record as accepted, or swap for a pool/linear queue if the owner wants strict
  allocation-free audio code.
- **C5 — Test count drift**: AGENTS §6 says GrooveMutatorTests has 18 test functions;
  the file defines 17 `void test…` functions. Verify nothing was lost in the working-tree
  edits, then align the docs.

### What the review found solid (do not churn these)

`CoreDSP::Rhythm` is clean (pure, deterministic, clamped, documented); the
Euclid/mutation/fill maths are correct; the trigger lifecycle follows every plan-19
ending path (mode change, transport stop, `prepareToPlay`, panic, preset load); the
note-off guarantee and retrigger-cut logic are right; state/preset plumbing follows the
suite patterns; affinity and the per-path tests exist.

---

## Phase 0 — Baseline and evidence (no behaviour change)

1. Clean Release build + full CTest of the current tree (default **and** bank builds).
   Record pass/fail and the entry counts (§15 check 1–2).
2. Render the editor per R9 (`SUITE_RENDER_DIR` under `build/renders/`) and **confirm A2
   visually** — the GRID page render should show rows 4–7 collapsed/overlapping. Keep the
   before renders for the §15 check 7 comparison.

Gate: the A2 overflow is confirmed from the render (or refuted — then rewrite Phase 2).

## Phase 1 — DECIDED 2026-11-25: the run sources are the host transport or the trigger

The owner's answer: **the plugin is a sequencer that syncs to the host transport/clock, or
is started by the trigger key.** There is no third "PLAY" source. Consequences:

3. **Remove the `play` parameter entirely** — `playId` leaves the header, the layout, the
   editor's `playIndicator` button + `ButtonAttachment`, and the A1 cast goes with it.
   `numControls` returns to 159 (master 17). No state-version bump is needed: the plugin is
   unreleased (the plan-19 rule), and a saved session carrying `play` simply drops it via
   `resetMissing`.
   - Optionally keep a *read-only* running indicator in the editor: the processor publishes
     a `grooveRunning` atomic (set in `processBlock`, the plan-19 `playheadStep` pattern)
     and the 60 Hz timer drives a non-interactive lamp. Not a toggle, not a parameter.
4. **Change the running condition to match the owner's model** (`processBlock`):
   ```cpp
   const bool running = followHost ? transportRunning : triggerActive;
   ```
   - Host Sync **on**: the groove runs and stops with the host transport, on the host's
     PPQ + tempo — no trigger key needed.
   - Host Sync **off**: the trigger key (Latch/Gate, internal Tempo clock) starts and stops
     the groove — as today.
   Sub-question to confirm with the owner at execution time (one sentence, not a gate):
   when synced, should the trigger key still gate the groove, or is it ignored (and then
   should it pass through to the output like any other note instead of being consumed)?
5. **Update the tests to the new model**: with sync on + a running fake playhead and *no*
   trigger note, the groove runs (`testHostSyncAlignment` likely simplifies); with sync off
   and no trigger, silence holds (`testDefaultSilence`); one test per ending path with the
   transport stop now stopping a synced groove that never saw a trigger. The trigger-mode
   ending path stays for the free-running case.

Gate: none — the decision is recorded. Phase 2 may start.

## Phase 2 — Fix the GRID page layout (A2)

4. Re-layout the STEP GRID group to the owner findings' description: the lane strips in
   the upper half (or rows across the full inner height — whichever the Phase 0 render
   plus the owner's note support), the ruler as a real `rulerHeight` strip above them,
   the note/velocity/level/on/division controls in the rows.
5. Derive `editorHeight` from the row sum so a mismatch is impossible (the plan-20 rule:
   a block's height equals the sum of its rows). Remove the unused `rulerBounds`-as-half
   arrangement.
6. Re-render every page before/after (R9) and byte-compare the unaffected pages (LANES,
   MUTATE, GROOVE, MOD must be unchanged); the owner approves the measured window size
   (R4).

## Phase 3 — Plugin identity and pass-through (A3) — **CONFIRMED by the owner 2026-11-25**

7. `IS_SYNTH` is confirmed: the DAW check passed (the plugin loads and the stereo
   pass-through holds with the synth declaration). Recorded in AGENTS §1's plugin table.
8. The JUCE host test `GrooveMutator_VST3_HostLoad` and `testPassThrough` both pass against
   the synth build. Note for the Ableton rescan rule: the parameter count is unchanged by
   this plan (159), so no forced rescan is needed — a fresh instance is enough to pick up
   the clamp/behaviour fixes.

## Phase 4 — Hygiene

9. Repair the C1 mojibake comments with the Edit tool; `git diff --stat` afterwards must
   list only those lines (§15 check 4).
10. Resolve C2: delete `reducedKnobDiameter`, or — only with the owner's explicit R1
    exemption — implement the 28 px knob where it is drawn. Rename the local
    `knobDiameter` constant to what it actually is (`controlCellWidth` semantics) or make
    it do what its name promises.
11. Resolve C3 with the owner in the same pass (Steps lanes mutated or not; view follows
    the decision).
12. C4: record as accepted, or plan a queue swap if the owner wants it.

## Phase 5 — Docs and tree

13. Update AGENTS.md §4.9 (159 parameters — master 17, no `play` row; the new running
    condition: synced = the transport runs the groove, free = the trigger; `IS_SYNTH`,
    window size, test-function count) and §6's test list. The Ableton-rescan note applies
    to the parameter-count change (160 in the working tree → 159 after this plan).
14. Regenerate the manual screenshots (`SUITE_RENDER_DIR` → `docs/manuals/`, crops per
    R11) and re-walk MANUAL.md's layout text.
15. Absorb `groovemutator_owner_findings.md`'s outcome into AGENTS/CHANGELOG and delete
    the file (the plans-README rule); note the change in CHANGELOG.md. `build_test/` is
    deleted or folded into `build/` — show the owner before deleting anything untracked.
16. Update this file's Status line, run the full §15 checklist, then delete the plan and
    remove its row from `docs/plans/README.md` (the finished-plan rule).

## Verification checklist (applies to every phase)

- Clean build: 0 errors, no new warnings (the C4996 baseline ~31 from the shared router).
- Full CTest, default and bank; report the entry counts.
- Before/after renders of every Groove Mutator page; unaffected pages byte-identical.
- `git diff --stat` shape check; `git status --short` shows only deliberate files.
- Not runnable here (report, never claim): macOS/AU, the real-DAW pass-through and
  rescan behaviour, and the lockup generator (owner's local tool).
