# Plan R3: editor base and one look and feel

Written for: a coding agent carrying out this refactor in the repository. Read `AGENTS.md` (sections 6, 7, 8, 9 and 15) and `docs/plans/00_framework-readiness.md` first. Do not commit. Do not push.

Status (2026-11-08): phases 0 to 4 are implemented and measured. Five of the six baseline views are byte-identical after the change. The sixth, the MicroTimingChaos PREVIEW tab, has one noisy first render (section 8). Owner decision needed on that view before this plan is closed.

---

## 0. Goal

1. One look and feel base for the suite. MicroTimingChaos's own class now derives from `SuiteUI::SuiteLookAndFeel`.
2. The shared refresh rate is named once, not copied in each editor.
3. No visible change to any editor. Checked by a byte-identical render of each editor before and after.

Non-goals:

- No change to the palette values. The palette consolidation is done, and the palette lives in `SuiteUI::Colours`.
- No change to the layout or the window sizes.
- No new widgets.

---

## 1. Current state (verified 2026-11-08 against HEAD)

- `SuiteUI::SuiteLookAndFeel` (`modules/suite_ui/include/SuiteLookAndFeel.h`) derives from `LookAndFeel_V4`. It overrides `drawRotarySlider` (LED ring), `drawLinearSlider` (horizontal track with dot), and `drawToggleButton` (pill with glow). It sets a few colours in its constructor. It does not override any font.
- `MicroTimingChaos::LookAndFeel` (`plugins/MicroTimingChaos/MicroTimingChaosLookAndFeel.h`) derived from `LookAndFeel_V4` at HEAD. It overrides `drawRotarySlider`, `drawToggleButton`, `drawComboBox`, `positionComboBoxText`, and the fonts `getLabelFont`, `getComboBoxFont` and `getPopupMenuFont` (14 pt). It loads textures and SVG icons from `BinaryData`.
- SpectralDrift and DynamicSaturation use `SuiteUI::SuiteLookAndFeel`.
- All three editors call `startTimerHz (60)` in their constructors.
- The MicroTimingChaos modulation rows are `SuiteModulationPanel` sliders with `juce::Slider::LinearHorizontal` (`modules/suite_modulation/Source/SuiteModulationPanel.cpp`, lines 70 and 158 to 159). They draw with the look and feel of the editor they are in.

---

## 2. The design

1. **`SuiteUI::kRefreshHz = 60`** in `SuiteLookAndFeel.h`. All three editors use it.
2. **`MicroTimingChaos::LookAndFeel` derives from `SuiteUI::SuiteLookAndFeel`.** It keeps its own colours, textures, and every drawing that differs from the shared class.
3. **Move a drawing into the shared class only if it is identical in both plugins** that would use it. Do not change either look to make them match.
4. **Editor setup stays in each editor.** A shared base class for the editors was considered and rejected: the editors differ in layout and pages, and a base class would add a virtual call for three lines of setup.

---

## 3. Phases

### Phase 0: baseline renders (required)

1. Each plugin's test executable writes a PNG of each editor view when `SUITE_RENDER_DIR` is set. The views are SpectralDrift SOUND and MOD, DynamicSaturation SOUND and MOD, and MicroTimingChaos CURVE and PREVIEW. The helper is `PluginTest::writeDeterministicPng` in `tests/common/PluginTestUtils.h`, and the tests are `testRenderBaseline` in each `PluginTests.cpp`.
2. Render the baseline from the code before the change. Copy the working tree to `build/baselines/ui/before-src`, restore the four look and feel files there from HEAD, and build the test executables in `build/baselines/ui/before-build`.
3. Store the PNGs under `build/` (`build/baselines/ui/before-renders-N`). Nothing goes outside the project.

**Done when:** six PNGs exist, and each view renders to identical bytes in the same process. **Result:** met. The test asserts the first condition for every view. Across processes, see section 8.

### Phase 1: find out how the two look and feels differ

Verified against HEAD. Each row lists the drawing, the shared version, and the decision.

| Drawing | MicroTimingChaos | Shared (`SuiteLookAndFeel`) | Result |
|---|---|---|---|
| Rotary knob | Gradient body, thick track, pointer line | LED ring (21 LEDs) and body | Different shape. MTC keeps its override |
| Toggle button (BPM Sync) | Icon, texture overlay when active, ON/OFF text | Pill with glow | Different. MTC keeps its override |
| Linear slider | Has none of its own, but its modulation rows use `LinearHorizontal` sliders | Horizontal track with dot | Would change MTC's modulation rows. MTC keeps the `LookAndFeel_V4` drawing through an override |
| Combo box | `drawComboBox`, `positionComboBoxText` | Not overridden | MTC only. Keeps its override |
| Label, combo and popup fonts | 14 pt | Not overridden (`LookAndFeel_V4` default) | Different. MTC keeps its overrides |
| Constructor colours | Sets its own text colours | Sets slider thumb and toggle text colours | MTC restores the `LookAndFeel_V4` values for the thumb and toggle text, so its sliders and toggles are unchanged |

**Identical drawings: none.** The earlier list in this file said the fonts were identical. That was wrong: the comparison was against a shared class that had no font override at HEAD.

### Phase 2: name the refresh rate and make MTC's LookAndFeel a subclass

1. Done. `SuiteUI::kRefreshHz` is defined in `SuiteLookAndFeel.h`, and the three editors call `startTimerHz (SuiteUI::kRefreshHz)`.
2. Done. `MicroTimingChaos::LookAndFeel` derives from `SuiteUI::SuiteLookAndFeel`. It keeps every override from the table above, and adds `drawLinearSlider` that calls the `LookAndFeel_V4` version.
3. Done. The constructor still takes the accent colour, and `MicroTimingChaos::Palette::velocity` still reaches the classes that use it.

**Done when:** the MTC editor renders byte-identically to its phase 0 PNG, and MTC's tests pass. **Result:** the CURVE view is byte-identical. The PREVIEW view matches the baseline's later runs (section 8). All 10 CTest entries pass.

### Phase 3: move identical drawings into the shared class (only if phase 1 allows)

1. Nothing moved. Phase 1 found no drawing that is identical in both plugins.
2. Because nothing moved, SpectralDrift and DynamicSaturation are unaffected by this phase. Their renders are byte-identical to the baseline.

**Done when:** all renders match. **Result:** met for SpectralDrift and DynamicSaturation (four views byte-identical), and for MicroTimingChaos CURVE. PREVIEW: section 8.

### Phase 4: documentation

- `AGENTS.md` section 2 updated: the MTC look and feel line, and `kRefreshHz`.
- `AGENTS.md` section 6 updated: the `SUITE_RENDER_DIR` render tests.
- `CHANGELOG.md`: a "no visible change" entry for the MTC look and feel.
- `docs/plans/01_juce9-upgrade-opportunities.md` and `docs/plans/README.md` corrected to match this file.
- `AGENTS.md` section 8 is unchanged. The public names used by consumers did not change, apart from the new constant.

---

## 4. Verification

- The baseline views are the test. A byte-identical match is the pass condition, with the exception in section 8.
- Full CTest on Windows: 10 of 10 pass in a clean build with no errors and no warnings.
- A byte match in a headless render does not prove the live window looks the same. Look at one screenshot in the Standalone app before this plan is closed. This has not been done.

## 5. Risks

- A look that is inherited changes the plugin that inherits it. The renders above would show it. They do not, for the views measured.
- A headless render may not match a live window, especially for fonts and `setScaleFactor`. The standalone check in section 4 covers this.
- The MTC look and feel loads its textures once in static images. The subclass adds no new loads, so start-up time is unchanged.

## 6. Owner decision (default used unless changed)

- Fold MTC's shared drawing into `SuiteUI`, keeping its MTC-only colours in a subclass? **Used: subclass only.** No drawing was identical, so nothing was folded.

## 7. Rollback

Nothing is committed. Revert the changed look and feel and editor files. The baseline PNGs in `build/baselines/ui/before-renders-1` show the window as it was before the change.

---

## 8. Results and the open decision

**Measured renders** (`build/baselines/ui/`):

- `before-renders-1`, `-2`, `-3`: the baseline, from `before-src`.
- `after-renders-1`, `-2`: after the change.

| View | Baseline runs 2 and 3 vs after | Notes |
|---|---|---|
| SpectralDrift SOUND | identical | |
| SpectralDrift MOD | identical | |
| DynamicSaturation SOUND | identical | |
| DynamicSaturation MOD | identical | |
| MicroTimingChaos CURVE | identical | Includes the modulation sliders, so the linear-slider override works |
| MicroTimingChaos PREVIEW | identical to runs 2 and 3 | Baseline run 1 differs. See below |

**The PREVIEW exception.** The first baseline run differs from runs 2 and 3 and from both after runs by three pixels, at (346,165), (273,167) and (93,243). These are glyph edges of the legend and the VELOCITY label, with a maximum channel difference of 34. The same three pixels differ in the same way between the after run and the first baseline run. So the change does not move any pixel. The cause is not identified. It looks like a first-render effect in a fresh process, in text rasterisation, and not in the look and feel.

**Decision needed from the owner.** Choose one:

1. Accept the PREVIEW view as the baseline of runs 2 and 3, and document the first-run effect. This is the current recommendation.
2. Keep the byte-identical gate and find the cause first. This may take time, and the cause may be in JUCE.
3. Allow a pixel tolerance for this view only.

Until the owner chooses, this plan is open for the PREVIEW view. The other five views pass the gate.

---

## Notes for the next agent

- Prove determinism first. This plan's renders were repeated in separate processes, and that found the PREVIEW effect. Repeat the renders across processes, not only within one.
- Read `SuiteLookAndFeel.h` and `MicroTimingChaosLookAndFeel.h` side by side before you decide anything. The table in section 3, phase 1 is the verified list. Check it against the code before you change it.
- Check a phase's claim against the code before you mark it done. Earlier work marked phases 2 and 3 complete while the code still had the old class and a font move that was not identical.
- Write renders under `build/`, never into the repo or `%TEMP%`.
- Scope the `juce::Graphics` before you read an image (`AGENTS.md` section 11).
