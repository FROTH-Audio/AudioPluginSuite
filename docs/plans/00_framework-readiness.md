# Assessment: is the suite ready to be a framework?

Written for: the owner, and a coding agent that will carry out the refactors. Read `AGENTS.md` first.

Status: assessment, written before any refactor. Updated 2026-11-08: R1 (state versioning) and R2 (plugin recipe) are done. R3 is complete with a minor owner-decided baseline for MicroTimingChaos's PREVIEW tab first-render effect. The gap table in section 2 describes the state before refactors: those gaps are now closed.

---

## 1. Verdict

**The engine and modulation layers are framework-ready. The plugin layer is not.**

- Shared DSP, the modulation router and the UI widgets are already written so that a new plugin can use them, and no plugin-specific code was found in the shared modules.
- Adding a fourth plugin today means copying one of the three plugins by hand: about 20 lines of `juce_add_plugin` metadata, a test executable of about 30 lines with its own source list, and a processor and editor that repeat the same state and timer code. The process is documented in `AGENTS.md` section 9, but it is manual.
- There is no state version, so the first change that needs a migration will have to invent one. Presets (`docs/plans/03_presets.md`) depend on that.

Estimated cost of the gaps: a new plugin takes a day or two of copying and checking. After refactors R1 to R3 below, the scaffolding part should be a few files and a target table.

---

## 2. What was checked

| Area | Finding | Verdict |
|---|---|---|
| DSP (`modules/core_dsp`) | Header-only engines with tests (29 tests). Only plugin-name hits are comments (`SpectralEngine.h`), not coupling. | Ready |
| Modulation (`modules/suite_modulation`) | Router is index-based with a `TargetInfo` table per plugin. Used by MicroTimingChaos, Spectral Drift and Dynamic Saturation (`AGENTS.md` sections 4.1 to 4.4). | Ready |
| UI widgets (`modules/suite_ui`) | One palette (`SuiteUI::Colours`), one LookAndFeel, shared widgets. Used by all three editors. | Ready, with the look-and-feel gap below |
| Build (CMake) | Each plugin repeats a `juce_add_plugin` call and a test executable. The shared modules have a `*_add_to_target` function, but there is no plugin-level function. | **Gap (R2)** |
| State | `getStateInformation` and `setStateInformation` are identical in all three processors. There is no version field. `setStateInformation` replaces the whole state and does not check or migrate it. | **Gap (R1)** |
| Processor scaffolding | Each processor builds its own parameter layout and caches its `std::atomic<float>*` by hand. There is no shared base. | **Gap (R1)** |
| Editor scaffolding | All three editors start a 60 Hz timer and call `setLookAndFeel` with the same pattern. MicroTimingChaos has its own `LookAndFeel` class (`MicroTimingChaosLookAndFeel.h`) alongside `SuiteUI::SuiteLookAndFeel`, which SpectralDrift and DynamicSaturation use. | **Gap (R3)** |
| Versioning | `juce_add_plugin` has no `VERSION` in any plugin. The project version (1.0.0) is the only version. | Minor gap (R2) |
| Plugin identity | `PLUGIN_CODE` and `BUNDLE_ID` are typed by hand. Nothing checks that codes are unique. | Minor gap (R2) |
| Tests | Shared helpers in `tests/common`, plus `tests/common/ModulationTestUtils.h` for the routing tests. | Ready |
| Presets | Proposed, not built (`docs/plans/03_presets.md`). Needs R1. | Not started |
| Build time | Shared modules are compiled into each consumer, not linked as libraries (`AGENTS.md` section 8). This is a deliberate choice that avoids duplicate JUCE symbols. Build time grows with each plugin. | Accepted trade-off, no change |

Also checked and not a gap: parameter IDs and plugin codes are permanent (`AGENTS.md` section 7), so refactors must not rename them.

---

## 3. What to do first

1. **R2 (build recipe).** Lowest risk, and the biggest saving per new plugin. It changes no sound.
2. **R1 (state and processor core).** Needed before presets and before any change that needs a migration. Capture state fixtures before changing anything.
3. **R3 (editor and look and feel).** Visual change risk. Use the byte-identical render check from the suite modulation refactor.

R1 and R2 are independent and can be done in either order. R3 does not depend on either.

---

## 4. Detailed plans

- R1: plugin core and state versioning. **Done.** The module is `modules/suite_state`, and the plan has been removed as finished.
- R2: plugin recipe in CMake and a skeleton for new plugins. **Done.** The function is `modules/suite_plugin/SuitePlugin.cmake`, and the generator is `tools/new_plugin/new_plugin.py`. The plan has been removed as finished.
- [R3: editor base and one look and feel](02_framework-refactor-3-editor-ui.md)

Each plan has its own steps, checks and "done when" line. None of them should change a parameter ID, a range, a default, or a saved session.

---

## 5. Items the owner must decide

- Set a plugin `VERSION` (R2)? A DAW may show it, and some hosts cache plugin versions. Default: set it to the project version.
- Keep MicroTimingChaos's own look and feel class, or fold it into `SuiteUI` with a per-plugin accent (R3)? Default: fold it in, keep the MTC-only colours in a subclass.
- Is a version field in the saved state acceptable (R1)? It adds one attribute to the XML root. Existing sessions have no version and will be read as version 1. Default: yes.

## Notes for the next agent

- R1 and R2 are done. Do not redo them. Read `modules/suite_state` and `AGENTS.md` section 9 for what they do.
- The verdict in section 1 and the table in section 2 describe the code before R1 and R2. Trust the code over this file.
- The next refactor is R3, in `02_framework-refactor-3-editor-ui.md`.
