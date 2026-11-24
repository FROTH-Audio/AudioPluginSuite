# Plans: order and status

Written for: the owner (what to do next) and the coding agents who carry out each plan. Read `AGENTS.md` first.

The number in each file name is the execution order. **Finished plans are deleted.** Their outcome lives in `AGENTS.md`, `CHANGELOG.md` and the code, so the deletion loses no working record. The plans are tracked in git, so a deleted file can still be recovered from history if it is ever needed.

Deleted on 2026-11-19 (all implemented and verified): 00 (framework readiness, R1-R3), 03 and 03.1-03.4 (presets), 04 and 04.1-04.2 (Sgraffito), 05 and 05.1 (suite UI rework), 07 (sample-linked presets), 08 (header bar, settings and about), 09 (code quality), 10 (Sgraffito owner findings), 11 (MidiEffect/StereoTool/Multiband improvements), 6.1 and 6.1a (Cyclical and the MidiEffect/StereoTool enhancements).

Status below is as of 2026-11-24.

Deleted on 2026-11-24 (all implemented and verified; the owner gates were satisfied by the
subsequent DAW-testing or the render review, and the owner approved the remaining gates in
one pass on 2026-11-24): 02 (the one owner decision on the MTC PREVIEW first render closed),
**17** (Phases 6, 9 and 11 executed — the affinity audit round 2, MultiSat's interest
features and the +20 SD/StereoTool routing presets, the bank at 270; Phases 10's docs and
12's housekeeping folded in), 12, 13, 14, 15, 16 (plan 17 Phase 12's deletions, executed by
plan 18 Phase 9), **18** (all nine phases done; the closing review satisfied by the plan-20
render rounds) and **20** (the transform-scope fix, the transform knobs and panel re-layout,
the Cyclical slot-page groups with the MASTER mix row). **Dropped on 2026-11-24 by the owner
decision (not executed, no outcome to record): 01** (the macOS/CI check — the owner takes it
offline), **05.2 and 05.3** (the parameter audit — dropped) **and 05.4** (Sgraffito's silent
controls — dropped). Any valuable observations in them stay recoverable from git history.

## Order

| # | File | Status | Why here | Gate before the next step |
|---|---|---|---|---|
| 19 | [19_groove-mutator.md](19_groove-mutator.md) | Approved concept; detailed implementation plan written 2026-11-23, refined 2026-11-24 ([19_groove-mutator-concept.md](19_groove-mutator-concept.md) holds the idea record) | New plugin: Groove Mutator, a generative rhythm-MIDI instrument (8 percussion lanes, euclidean + hashed patterns, Lorenz-driven mutation, fills, polyrhythm) | Build starts when the owner says go |
| 06 | [06_juce9-plugin-ideas.md](06_juce9-plugin-ideas.md) | Owner picked the order 2026-11-18; first passes of 2.1, 2.4, 2.5, 2.9 and 2.11 done | Options for the rest. | Owner picks the next idea (2.10 is done as Cyclical; 2.3 remains) |
| 6.1_review | [6.1_review.md](6.1_review.md) | Reference, created 2026-11-19 | Review of the Cyclical creative-enhancement work: loop loading sources, playback modes, test coverage and known issues. | None |

## Rules for executing a plan

- Do the steps in order. Start a later plan only when the earlier gate is met, or the owner says to skip it.
- Do not commit or push. The owner commits between plans.
- Check the plan's claims against the code before acting on them. Plans were written on a date and the code moves.
- Record what you verified and what you could not verify, in the plan or in your report.
- Run the post-change checks in `AGENTS.md` section 15 before you mark any step done.
- Delete the plan when it is finished, and remove its row here. Do not keep a "done" plan as an archive: its outcome belongs in `AGENTS.md`, `CHANGELOG.md` and the code.
