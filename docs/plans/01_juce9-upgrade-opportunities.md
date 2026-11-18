# Plan 01: JUCE 9 upgrade, verification (R0) and what remains

Written for: the owner, and the coding agent that closes the open items. Read `AGENTS.md` first.

Status as of 2026-11-08. Windows checks are done and verified. Items that need a Mac, a CI run or a DAW are open. Execution order is in `docs/plans/README.md`.

## Phases

| Phase | Scope | Status |
|---|---|---|
| R0 | Verify the build, tests and plugin loading | Windows done. macOS and CI open (below) |
| R1 | Plugin core and state versioning | Done: `modules/suite_state` |
| R2 | Plugin build recipe | Done: `suite_add_plugin()`, `tools/new_plugin/`. Plan removed |
| R3 | Editor and look and feel | Implemented. One owner decision open on the MicroTimingChaos PREVIEW render (`02`, section 8) |
| R4 | Presets | Not started. Owner decision 2026-11-09: plan 03 must not start yet. The seam committed in 643ba78 was removed (`03`) |
| R5 | Documentation | Open. Updated at each step; final pass after R4 |

## R0: verified on Windows (2026-11-08)

- All targets build in Release: CoreDSP, SuiteUI, SuiteModulation, SuiteState, the three plugins and the host tests. No errors and no warnings.
- Standalone apps and VST3 bundles exist for all three plugins under `build/plugins/<Name>/<Name>_artefacts/Release/`.
- CTest passes 10 of 10: CoreDSPTests, SuiteState_Tests, SuiteModulation_Tests, SuiteUI_Tests, the three plugin test executables, and the three VST3 host-load tests.
- The VST3 host test loads all three bundles through JUCE's plugin host. That is not a DAW test.
- MicroTimingChaos runs in a DAW without problems (owner report, 2026-11-08). The owner did not record which DAW, the plugin category the host showed, or whether a rescan was needed. Record those if the DAW matters for a release.
- The stereo buses are in place. All three processors declare a stereo input and output. MicroTimingChaos declares a stereo pass-through bus, so DAW scanners do not drop it. This was the "broken VST3 build" item. It is done, and `CHANGELOG.md` records it.
- MIDI flags: MicroTimingChaos is a MIDI effect (`MIDI_EFFECT` in its CMake). SpectralDrift and DynamicSaturation are audio effects.

**R3 look and feel (02):** `MicroTimingChaos::LookAndFeel` now derives from `SuiteUI::SuiteLookAndFeel`, and keeps its own drawing. All three editors use `SuiteUI::kRefreshHz`. The renders are identical, except one MicroTimingChaos PREVIEW view with a first-render effect. The owner decides (`02`, section 8).
- **R4 (03):** not started. The seam committed in 643ba78 was removed from the working tree on 2026-11-09, because plan 03 must not start yet.

## R0: open (needs a Mac, a CI run or a DAW)

- **macOS build (deferred by the owner, 2026-11-09).** Not testable now. Do not work on it until the owner asks. Recorded so it is not forgotten: build all targets with Apple clang, and confirm the `AU` folder exists under `_artefacts/Release`.
- **macOS tests (deferred by the owner, 2026-11-09).** Not testable now. When it can be run: all 10 CTest entries on macOS, and the `-ffp-contract=off` flag in the root `CMakeLists.txt` must stay (without it the LFO Offset test fails, see `CHANGELOG.md`).
- **CI.** `.github/workflows/build.yml` runs on `windows-latest` and `macos-15`. It has not been run on the current branch. Pushing is the owner's call.

## Removed from R0 (not in the code, or already covered)

- **Luma-curve preview, luma view and luma viewer.** Dropped by the owner (2026-11-08). They were never started, and no luma code exists in the repository.
- **"Run the build-status test".** No such test exists. Dropped.
- **"Run all 9 CTest tests".** The count is now 10, with `SuiteState_Tests`.
- **"Add the new dynamic-saturation plugin".** Done. It is in the repo.
- **"Add R3/R4/R5 to the plan" and "Add the R4/R5 sub-plans".** R3, R4 and R5 are in this file's table and in `docs/plans/README.md`. R4 has its own file (`03_presets.md`). R5 has no separate plan. Its work is listed under R5 above.
- **"Verify the build log has no warnings" and "Rebuild after any change".** Verified for this build. Rebuilding after a change is a rule, kept in `AGENTS.md`, not a task.

## R5: documentation (open)

- `AGENTS.md` and `CHANGELOG.md` are current as of 2026-11-08, with one exception: section 3 still says "All 10 CTest entries pass". That is true. Keep it in step with any new test.
- After R3 and R4, do a final pass of `README.md`, `CHANGELOG.md` and `AGENTS.md`. Check parameter tables, test counts and the layout section.

## Notes for the next agent

- R0 needs a Mac for the AU and macOS items. An agent on Windows cannot close them, so do not mark them done from Windows.
- The Windows records in `build/baselines/phase0-pre-refactor/` are not macOS data. A macOS comparison needs its own baseline, built on the Mac from commit `0d21400`.
- For the AU check, confirm the `AU` folder exists under `_artefacts/Release` after a macOS build.
- Do not push to run CI unless the owner asks.

## R3 and R4 status

**R3 (`02_framework-refactor-3-editor-ui.md`):** Implemented on 2026-11-08. Five of the six baseline renders are byte-identical after the change. The sixth, the MicroTimingChaos PREVIEW tab, differs on the first render of a fresh process, by three glyph-edge pixels. The owner chooses how to treat that view. Until then, R3 stays open.

**R4 (`03_presets.md`):** not started. The owner decided on 2026-11-09 that plan 03 must not start yet. The seam work in commit 643ba78 (`suite_presets`, `StateHolder`, `PresetBar`, and the calls in the three processors and editors) was removed from the working tree. Commit 643ba78 is still in history; the owner decides whether to revert it.

## R0: open (needs a Mac, a CI run or a DAW)
