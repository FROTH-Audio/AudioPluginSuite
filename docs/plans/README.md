# Plans: order and status

Written for: the owner (what to do next) and the coding agents who carry out each plan. Read `AGENTS.md` first.

The number in each file name is the execution order. **Finished plans are deleted.** Their outcome lives in `AGENTS.md`, `CHANGELOG.md` and the code, so the deletion loses no working record. The plans are tracked in git, so a deleted file can still be recovered from history if it is ever needed.

Deleted on 2026-11-19 (all implemented and verified): 00 (framework readiness, R1-R3), 03 and 03.1-03.4 (presets), 04 and 04.1-04.2 (Sgraffito), 05 and 05.1 (suite UI rework), 07 (sample-linked presets), 08 (header bar, settings and about), 09 (code quality), 10 (Sgraffito owner findings), 11 (MidiEffect/StereoTool/Multiband improvements), 6.1 and 6.1a (Cyclical and the MidiEffect/StereoTool enhancements).

Status below is as of 2026-11-24.

Deleted on 2026-11-24 (all implemented and verified; the owner gates were satisfied by the
subsequent DAW-testing): 12, 13, 14, 15 and 16 (plan 17 Phase 12's deletions, executed by
plan 18 Phase 9). Their outcome lives in `AGENTS.md`, `CHANGELOG.md` and the code.

## Order

| # | File | Status | Why here | Gate before the next step |
|---|---|---|---|---|
| 20 | [20_daw-test-findings-6.md](20_daw-test-findings-6.md) | Proposed 2026-11-24; every §0 claim verified the same day — **the transform-scope root cause is found** (the scope samples the raw shape; the plan-17 "follows automatically" note was an assumption) | Sixth DAW-test round: the transforms fix + labels + room, Cyclical slot-page groups + dropdown + taller waveform, the filter/A-R probes, the lockup accent (owner task) | None — execute Phases 0-3 in order |
| 18 | [18_daw-test-findings-5.md](18_daw-test-findings-5.md) | Phases 1-9 executed 2026-11-23/24 (CTest 28/28, the bank at 240, the audit passing). The plan closes when the owner has reviewed the manuals and renders | Fifth DAW-test round: the transform UI, Cyclical tab-per-slot rework with per-slot controls and the MOD page, live region knobs, MultiSat's modulated crossover marker, StereoTool IMAGE + Phase + MASTER, +10 presets for DS/Sgraffito/MTC, docs, housekeeping | Owner: review the regenerated manual screenshots (docs/manuals) and the Cyclical 1000×994 render |
| 17 | [17_daw-test-findings-4.md](17_daw-test-findings-4.md) | Phases 1-5, 7 and 8 implemented and verified (the rest superseded by plan 18). **Open: Phase 6** (the affinity audit round 2 — MidiTool's Tempo dimming while synced, and the Cyclical matrix against the new per-slot knobs) **and Phase 9** (MultiSat's interest features: per-band mix, thermal drift, per-band fold — never implemented) | Fourth DAW-test round: the key-map fix, Bouncing Ball, audible transforms, StereoTool pan, the per-slot filter/A-R params, the LFO transforms, the StereoTool interest features | Phases 6 and 9 execute after plan 20 closes (the Cyclical matrix needs the plan-20 grouped layout) |
| 19 | [19_groove-mutator.md](19_groove-mutator.md) | Approved concept; detailed implementation plan written 2026-11-23 ([19_groove-mutator-concept.md](19_groove-mutator-concept.md) holds the idea record) | New plugin: Groove Mutator, a generative rhythm-MIDI instrument (8 percussion lanes, euclidean + hashed patterns, Lorenz-driven mutation, fills, polyrhythm) | Build starts when the owner says go |
| 01 | [01_juce9-upgrade-opportunities.md](01_juce9-upgrade-opportunities.md) | Open: R0 (macOS and CI check) | Checks the R2 recipe and the AU build on macOS, which no earlier step has checked. | The owner runs the macOS build and the CI on a Mac |
| 02 | [02_framework-refactor-3-editor-ui.md](02_framework-refactor-3-editor-ui.md) | Implemented 2026-11-08. One owner decision open on the MicroTimingChaos PREVIEW first render | The editor base every plugin now shares. Kept for the measurement of the one noisy render view. | The owner wants one look and feel. Byte-identical renders before and after |
| 05.2 | [05.2_preset_parameter_audit.md](05.2_preset_parameter_audit.md) | Phase 0 quick wins done 2026-11-12; phases 1-5 open | Cross-cutting audit: parameters vs presets, cleanup, documentation, tests, refactorings. Findings go to 05.3. | Open: the refactor candidates (owner-gated) |
| 05.3 | [05.3_preset_parameter_audit_results.md](05.3_preset_parameter_audit_results.md) | Findings 2026-11-12; F1.3 resolved 2026-11-12 (five Sgraffito presets added) | Detailed results of plan 05.2's checks. | Open: the refactor candidates (single source of truth for the schema) |
| 05.4 | [05.4_sgraffito-silent-controls.md](05.4_sgraffito-silent-controls.md) | Proposed 2026-11-12. Phase 0 (owner gate on control semantics) next | Sgraffito's silent controls become real: swing + humanise, ghost notes, sustain hold + swell shaping, and the vinyl surface. Each lands with a regression test. | The owner approves the semantics table (Phase 0) before code changes |
| 06 | [06_juce9-plugin-ideas.md](06_juce9-plugin-ideas.md) | Owner picked the order 2026-11-18; first passes of 2.1, 2.4, 2.5, 2.9 and 2.11 done | Options for the rest. | Owner picks the next idea (2.10 is done as Cyclical; 2.3 remains) |
| 6.1_review | [6.1_review.md](6.1_review.md) | Reference, created 2026-11-19 | Review of the Cyclical creative-enhancement work: loop loading sources, playback modes, test coverage and known issues. | None |

## Rules for executing a plan

- Do the steps in order. Start a later plan only when the earlier gate is met, or the owner says to skip it.
- Do not commit or push. The owner commits between plans.
- Check the plan's claims against the code before acting on them. Plans were written on a date and the code moves.
- Record what you verified and what you could not verify, in the plan or in your report.
- Run the post-change checks in `AGENTS.md` section 15 before you mark any step done.
- Delete the plan when it is finished, and remove its row here. Do not keep a "done" plan as an archive: its outcome belongs in `AGENTS.md`, `CHANGELOG.md` and the code.
