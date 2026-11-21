# Plans: order and status

Written for: the owner (what to do next) and the coding agents who carry out each plan. Read `AGENTS.md` first.

The number in each file name is the execution order. **Finished plans are deleted.** Their outcome lives in `AGENTS.md`, `CHANGELOG.md` and the code, so the deletion loses no working record. The plans are tracked in git, so a deleted file can still be recovered from history if it is ever needed.

Deleted on 2026-11-19 (all implemented and verified): 00 (framework readiness, R1-R3), 03 and 03.1-03.4 (presets), 04 and 04.1-04.2 (Sgraffito), 05 and 05.1 (suite UI rework), 07 (sample-linked presets), 08 (header bar, settings and about), 09 (code quality), 10 (Sgraffito owner findings), 11 (MidiEffect/StereoTool/Multiband improvements), 6.1 and 6.1a (Cyclical and the MidiEffect/StereoTool enhancements).

Status below is as of 2026-11-21.

## Order

| # | File | Status | Why here | Gate before the next step |
|---|---|---|---|---|
| 15 | [15_daw-test-findings-3.md](15_daw-test-findings-3.md) | Implemented and verified 2026-11-22 (all four phases). 28/28 CTest with the bank on, 200 presets audited | Third DAW-test round: StereoTool visual to top + meters out + default height = minimum, MultiSat meters out + frequency grid + header page switch, MidiTool Step Rates up to 4 Bars (append-only), per-mode affinity everywhere | Owner: review the plan-15 renders and the manual screenshots |
| 14 | [14_cyclical-overhaul.md](14_cyclical-overhaul.md) | Proposed 2026-11-20. Not started. Split from plan 13. All owner gates decided: Slice Length knob (G-A), Speed inactive under sync (G-B), window made slightly larger (G-C), granular in slices allowed (G-D) | Cyclical concept overhaul: Toggle trigger mode, per-slot notes, BPM sync with Beats, chop mode with Slice Length, SLOTS layout and window size, tangerine accent, 10 presets | Owner: approve the window size after the Phase 4 renders; regenerate the Cyclical lockup |
| 13 | [13_daw-test-findings-2.md](13_daw-test-findings-2.md) | Implemented 2026-11-21 (all phases). 28/28 CTest with the bank on, 200 presets audited. Two post-plan findings fixed outside it: MTC TRANSPORT knob distance, Pendulum LFO bug (AGENTS §7). Cyclical moved to plan 14. StereoTool 720 px width approved by the owner 2026-11-21 | The non-Cyclical second DAW-test findings: MultiSat visualiser scales and stereo meters, modulation and presets for SD/DS/MultiSat/StereoTool, StereoTool layout, MidiTool playable keys | Owner: review the regenerated manual screenshots (the last open gate; then delete the plan) |
| 12 | [12_daw-test-findings.md](12_daw-test-findings.md) | Implemented 2026-11-20. All nine phases in; 28/28 CTest with the bank on; the Step Rate migration (v2) taken per the plan's default | The owner's DAW-test findings, all verified against the code: Sgraffito's end-of-note click and dead Level knob, StereoTool Input/Output and help, DS help, MTC SEQUENCE layout and step-grid help, Multiband's rename/layout/frames, MidiTool as an instrument with dropdowns and twelve step rates, and one name for the suite's output stage | Owner gates open: regenerate the MultiSat lockup (owner-local tool); review the regenerated manual screenshots; confirm the Step Rate migration (overridable to append-only) |
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
