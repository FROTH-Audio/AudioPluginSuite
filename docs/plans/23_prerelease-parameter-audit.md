# Plan 23: Pre-release parameter wiring and documentation close-out

Written for: the owner and coding agents executing the follow-up. Read `AGENTS.md` first.

Source: the all-plugin parameter and documentation audit requested on 2026-11-25, plus
the preceding pre-release review and the current `.github/workflows/release.yml`.

Status: **Proposed; findings recorded, no implementation or product decisions made.** The
owner's new request reopens the Sgraffito silent-control and parameter-audit topics that
were dropped on 2026-11-24 (plans 05.2–05.4). This plan does not assume the intended DSP
behavior where it is not defined below.

Review note (2026-11-25): every checkable claim was verified against source and confirmed
(counts, target tables, README/MTC-README omissions, the `checkParameterIds` limit, the
missing Sgraffito host test, the release workflow). The review surfaced one **blocking
contradiction** (§0.3a — three of the five silent controls have non-neutral defaults) and
several gaps, folded in below and marked **[review]**.

## 0. Findings and release-readiness assessment

### 0.1 Per-plugin parameter audit

The parameter counts below include each plugin's shared modulation router where present.
The source pass found no other clearly unconsumed plugin controls; this is not a substitute
for an audible regression test of every parameter and parameter combination.

| Plugin | Parameters | Finding |
|---|---:|---|
| Spectral Drift | 60 | No unconsumed controls identified. The shared modulation reference is incomplete; see §0.2. |
| Dynamic Saturation | 59 | No unconsumed controls identified. The shared modulation reference is incomplete; see §0.2. |
| Micro-Timing Chaos | 66 | No unconsumed controls identified. Its technical README has an outdated LFO shape list. |
| Sgraffito | 83 | Five exposed controls do not currently produce their described audio effect; details below. |
| Stereo Tool | 69 | No unconsumed controls identified. README target count is wrong; see §0.2. |
| MIDI Chord/Arp Scale Tool | 14 | No unconsumed controls identified; this plugin does not use the shared modulation router. |
| MultiSat | 62 | No unconsumed controls identified. |
| Cyclical | 151 | No unconsumed controls identified. |
| Groove Mutator | 159 | No unconsumed controls identified. |

### 0.2 Confirmed documentation mismatches

- The suite README says only Spectral Drift, Dynamic Saturation and Micro-Timing Chaos
  share the modulation system. Eight plugins use it; MIDI Chord/Arp Scale Tool does not.
- **[review]** The suite README makes the same era-stale claim about the preset system
  ("Spectral Drift, Dynamic Saturation and Micro-Timing Chaos share a preset system") —
  all nine plugins ship the R12 preset seam.
- The suite README and Micro-Timing Chaos technical README omit **Bouncing Ball** from the
  LFO Shape options. The shared modulation reference also omits the 12 per-LFO transform
  parameters (Invert, Rectify, Quantise, Smooth, Pulse Width and Skew for each LFO), including
  their ranges and defaults. Plugin manuals mention the transform row, but do not consistently
  give its parameter ranges/defaults.
- Sgraffito README and manual say 24 controls are routable; its source target table contains
  26. Four of those targets correspond to controls that are not yet audible (Glide, Drift
  Amount, Drift Speed and Swell Time).
- Stereo Tool README says 16 routable controls; its target table contains 19.
- Sgraffito's Swell Time and Swell Crossfade descriptions conflict: README describes their
  behavior as implemented, the manual calls their shaping a follow-up, and editor help text
  claims the shaping is wired. Neither parameter is consumed by the audio path.
- Sgraffito's Tape Stop Time/Curve currently shape a **level fade** on note-off, not a
  playback-speed slowdown. The controls work as a level fade, but the product wording should
  not imply a full tape-speed stop. **[review]** The manual's TAPE & MEMORY paragraph already
  describes a level fall (accurate to the implementation); the README wording is the one that
  needs correcting if the level fade stays.
- **[review]** Stale Sgraffito parameter counts in the developer docs: `AGENTS.md` §13 says
  "71 parameters total" while `numControls` is 83 (71 + 12 plan-17 Phase 7 LFO transforms),
  and `SgraffitoTests.cpp`'s `checkParameterIds` comment repeats the 71. Both need the same
  correction pass as the user-facing docs.

### 0.3 Sgraffito implementation gaps

- **Glide**: parameter and editor binding exist; the processor caches the value but does not
  use it to glide between note pitches.
- **Drift Amount**: parameter and editor binding exist; no audio-path read applies it.
- **Drift Speed**: advances the Lorenz attractor, but the attractor state is not used by the
  audio path or a visible output, so the control has no user-observable effect.
- **Swell Time / Swell Crossfade**: parameter and editor bindings exist, but the processor
  does not use either to shape Swell playback.
- The editor currently presents these as usable controls; the modulation target table also
  exposes some of the inactive values as targets. Their controls can change and save values
  without changing sound.

### 0.3a Blocking: the silent controls are not neutral by default **[review]**

Phase 0.5 says "preserve ranges and defaults"; Phase 2 says wire the controls. Both cannot
hold for three of the five controls, whose defaults are already non-zero:

| Control | Range | Default | Consequence of wiring it |
|---|---|---|---|
| Glide | 0–500 ms | **50 ms** | Wired as-is, the default sound changes for every note |
| Swell Time | 50–4000 ms | **800 ms** | Wired as-is, the default Swell sound changes |
| Swell Crossfade | 5–200 ms | **40 ms** | Same |
| Drift Amount | 0–100 % | 0 % | Neutral — safe |
| Drift Speed | 0.01–2.0 | 0.1 | Inert while Amount is 0 — safe |

There is no third option: each of the three needs an explicit owner decision —
(a) introduce a neutral default (e.g. Glide 0 ms) and change the range/default together with
its `checkRange`, README, MANUAL and AGENTS entries, or (b) keep the default and accept the
changed default sound, or (c) keep the control unavailable and labelled as such. Phase 0 must
record one decision per control before Phase 1 writes its regression tests, because the
"neutral" test case differs per choice. A default/range change also touches the preset audit
(factory presets may author these values) and must not re-order or renumber anything else.

### 0.4 Release readiness

**Assessment: conditional, not a full pre-release sign-off.** The Windows release path and
the local Windows test evidence are strong, but the requested “all controls wired and
documented” criterion is not met until Sgraffito and the documentation discrepancies above
are resolved. macOS/AU and real-DAW checks remain external validation gates.

Evidence and limits:

- The preceding Windows Release clean build completed with 0 errors; 35 warnings were
  reported, with none identified in the changed UI sources. **[review]** The latest full
  bank build measured 34 warnings, all one class (C4996, the shared router's deprecated
  `AudioParameterFloat` form). Treat "one warning class, ~34" as the baseline, so a new
  warning cannot hide inside a ±1 drift.
- The full factory-bank CTest run passed **31/31**, `tools/audit_bank.py` passed, and the
  render-enabled run passed **31/31** while generating 39 editor images. Representative
  renders were inspected. These checks do not prove every parameter has an audible effect.
- All nine plugin manuals were present. The About/help and release-archive documentation
  changes from the preceding readiness task were validated.
- The current release workflow is manual-dispatch, defaults to a draft, configures the
  factory bank on, runs CTest and the bank audit on Windows and macOS, then packages the
  results. Its expected flat archive roots are:
  - Windows: nine top-level `.vst3` bundles and nine standalone `.exe` files.
  - macOS: nine top-level `.vst3` bundles, nine `.component` bundles and nine `.app` bundles.
  The archive-selection logic was checked, but a release archive was not produced as part
  of the audit.
- `tests/host/CMakeLists.txt` registers VST3 host-load tests for eight plugins; Sgraffito
  has no host-load entry. **[review]** This is diagnosed here but never scheduled — adding
  `suite_add_host_test(Sgraffito)` is now a Phase 4.1 item. It changes the CTest entry count
  (31 → 32 bank, 22 → 23 default), which §15 requires reporting, and `AGENTS.md` says
  "eight VST3 host-load tests" in several places. The local audit did not verify macOS, AU,
  standalone behavior on macOS, a fresh GitHub Actions release run, or all nine plugins in a
  real DAW.
- The plugin test helper checks the expected total count and that supplied IDs exist; it
  does not prove that the supplied ID list is the exact complete set. Passing tests should
  therefore not be treated as an exhaustive ID-coverage proof.
- The prior render review did not systematically check that screenshots containing meters
  or previews showed meaningful activity. This is a separate visual-readiness gap: a passing
  render test can still capture idle meters or empty previews.

### 0.5 Screenshot and render quality requirement **[review, scoped]**

For committed manual screenshots and R9 editor renders, a view that renders a **meter or a
signal-driven trace** should show representative, non-idle output. Populate it from a
deterministic processor input or representative preview data; do not paint a fabricated value
that is disconnected from what the plugin actually processes. Keep the capture repeatable
(the repeat-render byte-identity assert already exists in `testRenderBaseline`) and avoid
clipped meters, overlays or labels.

Scope notes:
- Some idle states are **legitimate** and must not be churned: Spectral Drift's "Waiting for
  audio" overlay, Groove Mutator's mostly-empty GRID with the transport stopped, and similar
  no-host states. The owner decides per view which ones should show signal; the goal is
  "no view looks broken or empty by accident", not "every pixel moves".
- Re-rendering a page the owner already approved re-opens the R4 approval — batch those
  renders and present them together rather than one at a time.
- The render set is also the manual screenshot set (R11): any re-render must be re-copied to
  `docs/manuals/` in the same change.

## Phase 0 — Reconfirm baseline and decide product behavior

1. Re-read the current Sgraffito processor, editor, manual, README, parameter layout, preset
   authoring data and tests before editing. The findings above are from 2026-11-25 and must
   be checked against the execution tree.
2. Record an owner decision for the intended semantics of Glide and Lorenz drift:
   - Glide: what triggers a glide, its pitch/time curve, and whether simultaneous voices
     glide independently.
   - Drift: which audible quantity the Lorenz X/Y/Z state modulates, and how Drift Amount
     and Drift Speed scale that movement.
3. **[review]** Resolve the §0.3a contradiction per control: for Glide, Swell Time and Swell
   Crossfade decide (a) a new neutral default (with the range/default, `checkRange`, README,
   MANUAL and AGENTS updates that follow), (b) an audible non-zero default, or (c) the
   control stays unavailable and labelled. Record the decision with the Phase 0 gate.
4. Confirm that Swell Time controls the reverse-swell envelope and Swell Crossfade controls
   its wrap blend, as their current names and descriptions imply.
5. Confirm whether Tape Stop should remain a level fade or include playback-rate slowdown.
   Keep the existing level-fade behavior either way; do not silently change its meaning.
6. Preserve existing parameter IDs, order, ranges and defaults unless the owner explicitly
   approves a pre-release change. If a range/default changes, update its tests, README,
   manual and AGENTS.md together. Add a state migration if the saved layout requires one.
   **[review]** Where the owner chose §0.3a option (a), the default change is that explicit
   approval — record it here so Phase 1's "neutral" test case is unambiguous.

Gate: the owner has decided the audible behavior for Glide, Drift and Tape Stop, **and the
§0.3a default question for Glide, Swell Time and Swell Crossfade**. If a behavior is
deferred, decide whether the control should be removed before release or be explicitly
marked unavailable; do not leave it appearing functional.

## Phase 1 — Protect the parameter inventory with tests

1. Review every plugin's current `checkParameterIds` call and compare its ID list against
   the complete parameter layout, including array-generated IDs and shared-router IDs.
2. Add an exact parameter-set assertion, or expand the existing helper only after confirming
   its callers intend to provide complete lists. Assert no missing, duplicate or unexpected
   IDs; retain the count check. **[review]** The cleaner direction: enumerate the APVTS
   state's own `<PARAM>` IDs and check membership both ways against the supplied list — that
   catches array-generated and shared-router IDs with no per-plugin bookkeeping, and keeps
   the existing caller lists as the source of names rather than the source of truth for
   completeness.
3. Add focused Sgraffito regression tests before implementation:
   - Glide produces the approved pitch transition and has no effect when neutral.
   - Drift Amount/Speed produce the approved audible change and remain deterministic for
     identical inputs and state.
   - Swell Time and Crossfade change Swell playback independently and do not affect modes
     where those controls are inactive.
   - Tape Stop tests assert the approved level/rate behavior and the existing completion
     path.
4. Keep default/neutral behavior regression coverage, state round trips, preset-bank audit
   coverage and parameter-range/default checks. Test an audio outcome, not only that a
   parameter value or UI widget changes.

Gate: the focused tests fail against the current inert implementation for the intended
behaviors, and the exact-ID checks cover all nine plugin layouts.

## Phase 2 — Wire Sgraffito controls

1. Implement Glide in the voice/pitch path using the Phase 0 decision. Keep the audio thread
   bounded and allocation-free; handle retriggers, overlapping notes and voice release.
2. Connect Lorenz state and Drift Amount/Speed to the agreed audible destination. Remove or
   repurpose unused Lorenz work if the chosen behavior does not need it.
3. Apply Swell Time and Swell Crossfade to the Swell mode's reverse envelope/wrap behavior.
   Preserve the mode affinity: dim the controls outside Swell, and verify the audible path
   while Swell is active.
4. Implement Tape Stop rate slowdown only if approved. If the owner retains the level fade,
   keep the DSP stable and update all visible descriptions to call it a level fade.
5. Keep parameter state, preset application and modulation consistent. Do not retain
   modulation targets that appear to work but cannot affect audio.

Gate: all five exposed controls have the approved effect, or are explicitly unavailable and
not presented as functioning; focused tests pass.

## Phase 3 — Reconcile user-facing documentation

Update the directly related files together:

1. `README.md`: enumerate all eight modulation consumers; correct the same stale sentence
   about the preset system (all nine plugins ship the R12 seam) **[review]**; add Bouncing
   Ball; document all transform parameter names, ranges and defaults; correct Sgraffito's
   24 → 26 and Stereo Tool's 16 → 19 target counts; state Sgraffito's implemented
   Glide/Drift/Swell/Tape Stop behavior accurately.
2. `plugins/MicroTimingChaos/README.md`: include Bouncing Ball and ensure the shared LFO
   options agree with the suite implementation.
3. The affected plugin `MANUAL.md` files: document the transform controls consistently,
   including ranges/defaults where the README is the central reference; correct Sgraffito
   and Stereo Tool target counts and Sgraffito mode behavior.
4. Correct Sgraffito editor help text so it agrees with the implemented behavior. If a
   control remains unavailable, its caption, help line, manual and README must all disclose
   that clearly.
5. Update `AGENTS.md` parameter and behavior notes, relevant preset coverage notes and
   `CHANGELOG.md`. **[review]** Name the two known stale counts: `AGENTS.md` §13's "71
   parameters total" for Sgraffito (it is 83) and the matching comment in
   `SgraffitoTests.cpp`'s `checkParameterIds` call (§0.2). If a UI layout or affinity
   changes, regenerate the affected manual screenshots under `docs/manuals/` and R9 renders
   under `build/`.
6. Review every committed manual screenshot and R9 render for meters and previews, per the
   scoped §0.5: only the views the owner flagged as needing signal, keeping legitimate idle
   states (Spectral Drift's "Waiting for audio", a stopped transport's empty GRID) as they
   are. Where a view needs activity, arrange deterministic real input (audio, MIDI, a loaded
   sample or other appropriate data) before capture. Update the render test/setup so it
   produces the same representative activity on repeat runs, and record this expectation in
   the R9 guidance in `AGENTS.md`. Batch re-rendered pages for one R4 approval, and re-copy
   each changed render to `docs/manuals/` in the same change.

Gate: every exposed parameter has a documented name, range/default, behavior and any
mode-dependent limitation; the suite-wide and plugin-specific descriptions agree with
source. Screenshots with meters/previews display meaningful activity.

## Phase 4 — Pre-release verification and sign-off

1. Run the clean Release build, full bank-enabled CTest suite, `tools/audit_bank.py`, exact
   parameter inventory tests and render-enabled CTest. Report exact test entry counts and
   any warnings against the existing baseline (~34, one C4996 class — see §0.4).
   **[review]** Add `suite_add_host_test(Sgraffito)` to `tests/host/CMakeLists.txt` first
   (the §0.4 gap) and report the new entry counts — 32 bank / 23 default — plus the
   "eight VST3 host-load tests" mentions in `AGENTS.md` §3/§6 that the ninth test makes stale.
2. Verify every new Sgraffito DSP test fails before the wiring change and passes afterward;
   verify neutral defaults preserve the established output.
3. Inspect renders for all affected Sgraffito pages and any changed shared modulation
   panels; regenerate committed manual screenshots where required. For every plugin page
   with a meter or preview, confirm it shows real, representative activity rather than an
   idle/empty state, and confirm repeated renders remain byte-identical.
4. Exercise the release archive packaging on Windows and macOS CI when available. Inspect
   each ZIP root for exactly the expected plugin bundles/executables and no nested raw VST3
   binary duplication.
5. Before publishing, verify a green GitHub Actions release build/test/bank audit on both
   matrix platforms, perform the available macOS/AU checks, and test the installers/bundles
   in supported DAWs. Record anything not verified rather than calling it passed.
6. Run AGENTS.md §15 post-change checks, add this plan's row to `docs/plans/README.md` when
   the owner approves it (there is none yet **[review]**), update this plan's status and the
   plans index, then delete this plan when finished; outcomes belong in AGENTS.md,
   CHANGELOG.md and code.

Release gate: no known exposed no-op controls; all documentation counts/options and
behavior match source; Windows and macOS release jobs pass; package contents are inspected;
remaining DAW/AU limitations are explicitly accepted by the owner.
