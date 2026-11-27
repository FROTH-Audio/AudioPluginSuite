# Plan 24: Groove Mutator — GRID lane names and PATTERN/STEPS on GRID

Written for: the coding agents who carry out the phases. Read `AGENTS.md` first.

Source: owner findings (2026-11-27) on the Groove Mutator editor. Two layout changes to the
GRID page, plus a sizing rebalance between the GRID and LANES pages.

Status: **Executed 2026-11-28** (Phases 1–4; 21/21 GrooveMutatorTests pass, renders under
`build/renders/plan24-after/`). One deviation from the written plan, forced by the first
render: §4.2's two-row variant was replaced by its own Option B (three lane-group rows) —
the three-segment PATTERN selector is illegible when sharing a row with a knob at the
131 px group width. Window 938 → 1102 (inside the 1200 resize limit). Check every
file/line reference against the current tree before acting (the code moves).

---

## 0. Findings summary

1. **Lane name above the ON button on GRID.** On the GRID page each lane row carries the note
   dropdown, the step strip, and (on the right) the DIVISION selector and the ON toggle, but no
   lane label. The LANES page shows "LANE n" in the card title band, and the note name sits to
   its right. The GRID page should gain the same "LANE n" identity above the ON toggle, so the
   operator can tell which row is which without switching pages. (The owner's note says
   "Lane 1…9"; the plugin has eight tracks — `GrooveMutatorProcessor::numTracks == 8` — so the
   labels run 1…8.)

2. **PATTERN and STEPS move from LANES to GRID.** The PATTERN selector and the STEPS knob
   currently live only on the LANES page (one column per lane, six knobs below the selector).
   The owner wants them on the GRID page's per-lane control group at the bottom, so the most
   frequently edited pattern controls are reachable without leaving GRID. The LANES page keeps
   the remaining articulation knobs (Pulses, Rotation, Accent, Flam, Ghost).

3. **Sizing rebalance.** GRID-page knobs may be made smaller and LANES-page knobs larger, so
   the moved controls fit the existing GRID lane-control group and the LANES page uses its
   vertical space for the remaining knobs. **Knob-size fact (verified in the code):** the
   drawn knob diameter is a function of the cell height, not of the local
   `knobDiameter = SuiteUI::knobDiameter * 0.7f` constant — that constant (and the
   `knobRowHeight` derived from it) only sizes *cells* (`LabelledSlider::resized`,
   `SuiteWidgets.cpp:400-419`: the slider gets `min(knobDiameter + 6 + valueBoxHeight, cell
   height − captionHeight)` centred, and the drawn knob is that rect minus 3 px per side).
   Today GRID and LANES cells are both `knobRowHeight` (76 px), so **both pages render the
   same ≈34 px knob**. A full 40 px suite knob needs a cell of `captionHeight (18) +
   knobDiameter (40) + 6 + valueBoxHeight (18) = 82` px or more. This is the plan-21 C2
   finding, unresolved in the tree (the local constant is still named `knobDiameter`).

---

## 1. Current layout (what the code does today)

Relevant files: `plugins/GrooveMutator/Source/PluginEditor.h` (LaneCard members, layout
constants) and `plugins/GrooveMutator/Source/PluginEditor.cpp` (`setPage`, `paint`, `resized`,
`timerCallback`).

### 1.1 GRID page (page 0)

- `gridBounds` = STEP GRID group, `gridGroupHeight` ≈ 704 px: ruler strip + 8 lane rows
  (`knobRowHeight` ≈ 76 px cells; the drawn knobs are ≈34 px, see §0.3) + gaps.
- Each lane row: `noteDropdown` (left, `laneNameWidth` 82 px) + `laneViewInset` + `view`
  (step strip) + `onToggle` (52 px cell, 36 px pill centred) + `divisionCtrl` (64 px cell).
- Below `gridBounds`: one framed group per lane (`laneBounds[track]`), each holding two knobs
  in one row — `velCtrl` (VELOCITY) and `levelCtrl` (LEVEL). This is the "lane control group"
  the owner refers to. **These groups already draw a "LANE n" title** (`paint()`, page 0
  branch calls `drawGroupFrame` per `laneBounds[track]`), so the Phase 1 label's job is to tie
  each step-grid row to its already-titled group, not to introduce the lane name.

### 1.2 LANES page (page 1)

- Eight framed columns (`laneBounds[track]`), full page height, each ≈139 px wide
  ((1168 − 7 × 8) / 8) — the same width the GRID lane groups get.
- Title band: "LANE n" (drawn by `paint`) + the note name in `card.name` (right side).
- `patternCtrl` (PATTERN) at the top in a `selectorCellHeight` (62 px) cell, then six knobs
  in `knobRowHeight` (76 px) rows: Steps, Pulses, Rotation, Accent, Flam, Ghost.

### 1.3 Visibility (`setPage`)

- GRID: `view`, `noteDropdown`, `velCtrl`, `levelCtrl`, `onToggle`, `divisionCtrl` visible.
- LANES: `name`, `patternCtrl`, `stepsCtrl`, `pulsesCtrl`, `rotationCtrl`, `accentCtrl`,
  `flamCtrl`, `ghostCtrl` visible.
- `name` is visible only on LANES today.

---

## 2. Phase 0 — Baseline and evidence (no behaviour change)

1. Clean Release build + full CTest, default and bank builds. Record pass/fail and entry
   counts (AGENTS §15 checks 1–2).
2. Render every Groove Mutator page with `SUITE_RENDER_DIR` set (GRID, LANES, MUTATE, MOD)
   into `build/renders/plan24-before/`. Keep them for the before/after byte comparison.
3. Confirm the GRID lane-control group currently holds only VELOCITY and LEVEL, that `name`
   is not visible on GRID, and that the GRID/LANES knobs render at the same ≈34 px (the
   §0.3 cell-height fact). Record the measured cell sizes.

Gate: renders captured and the two findings reproduced visually.

---

## 3. Phase 1 — Lane name above the ON button on GRID

### 3.1 Component

Add a `juce::Label laneNameLabel` to `LaneCard` (do **not** reuse the existing `name` label
with page-aware placement). The existing `name` is the note-name label on LANES; it is
updated in `timerCallback` from `trackNoteIds`. For GRID we want a static "LANE n" caption,
not the note name, so a separate label avoids overloading `name`'s semantics and its timer
update.

- In `PluginEditor.h`, `LaneCard`: add `juce::Label laneNameLabel;` (caption "LANE n").
- In the `LaneCard` constructor: set text "LANE " + `track + 1`, justification centred,
  font `SuiteUI::uiFont(11.0f, true)`, colour `SuiteUI::Colours::textDim`, mouse clicks off.
- In the editor constructor's component tree loop: `addAndMakeVisible(card.laneNameLabel)`.

### 3.2 Visibility

In `setPage`: `card->laneNameLabel.setVisible(currentPage == 0)`.

### 3.3 Layout

In `resized()` GRID branch, the lane row currently is:

```
noteDropdown | view | onToggle | divisionCtrl
```

Place `laneNameLabel` above `onToggle`. Because `onToggle` is a 52 px pill centred in a 52 px
cell, the label needs a small band above it. Two options:

- **Option A (preferred, matches the owner's "above the ON button"):** carve a
  `SuiteUI::captionHeight` (18 px) label band at the top of the `onToggle` cell and centre
  the pill in the remaining height, i.e. `laneNameLabel.setBounds(cell.removeFromTop(SuiteUI::captionHeight))`
  then `onToggle.setBounds(cell.withSizeKeepingCentre(...))`. Use `captionHeight`, not an
  arbitrary height, so the label's baseline matches the DIVISION caption beside it; note the
  ON pill then sits `captionHeight − 14` px lower in its cell than the DIVISION dropdown's
  pill — check the render and, if it reads as misaligned, bottom-align both pills instead.
- **Option B:** put the label in its own column to the left of `onToggle`, spanning the row
  height. Rejected: it steals strip width and the owner asked for "above".

Use Option A. The ruler/row heights are unchanged, so no window resize is required for this
phase alone; the label simply occupies space already inside the row.

### 3.4 Affinity / tests

- `testEditorPages` asserts `isNoteDropdownVisible(track) == (page == 0)`. Add an analogous
  `isLaneNameVisible(track)` seam only if a test needs it; otherwise keep the change internal.
  The render test (R9) covers the visual.
- No parameter change → no state-version bump, no preset change.

---

## 4. Phase 2 — PATTERN and STEPS on the GRID lane-control group

### 4.1 Visibility

In `setPage`, move the two controls' visibility from LANES-only to GRID:

- `card->patternCtrl.setVisible(currentPage == 0)` (was `currentPage == 1`)
- `card->stepsCtrl.setVisible(currentPage == 0)` (was `currentPage == 1`)
- LANES keeps `pulsesCtrl`, `rotationCtrl`, `accentCtrl`, `flamCtrl`, `ghostCtrl`.

So after this phase, GRID shows `patternCtrl` + `stepsCtrl` in each lane-control group, and
LANES shows the five articulation knobs only. `patternCtrl`/`stepsCtrl` attachments are
unchanged (they are already created in the `LaneCard` constructor and added to the tree in
the editor constructor), so no wiring changes.

### 4.2 Layout — GRID lane-control group

The group currently holds one row: `velCtrl` + `levelCtrl`. With PATTERN (a pill selector) and
STEPS (a knob) added, the group needs more rows. Two layouts:

- **Option A (preferred):** two rows — row 1 = VELOCITY + LEVEL (as today), row 2 = PATTERN
  (pill selector) + STEPS (knob). This keeps the knob row height and only grows the group
  height by one row.
- **Option B:** three rows — VELOCITY, LEVEL, PATTERN+STEPS. Rejected: too tall, the group
  would dominate the bottom of the window.

Use Option A. Consequences:

- Code sites: `resized()` page-0 branch, inside the per-track lane-group loop (after the
  `velCtrl`/`levelCtrl` row); `resized()` page-1 branch loses
  `card.patternCtrl.setBounds (inner.removeFromTop (selectorCellHeight)...)`, and
  `selectorCellHeight` (PluginEditor.cpp:27) becomes unused — reuse it for the GRID PATTERN
  cell or delete it.
- `laneControlGroupHeight` grows from `groupTitleHeight + 4 + knobRowHeight + 8` to
  `groupTitleHeight + 4 + 2 * knobRowHeight + rowGap + 8` (108 → 190).
- `editorHeight` is derived from the sum (938 → ≈1020, still inside the 1200
  `setResizeLimits` maximum); the owner approves the measured size (R4).
- Width is a non-risk: each GRID lane group is ≈139 px, the same width the PATTERN pill
  already gets on LANES, so the 3-option "Euclid/Hash/Steps" selector fits unchanged.
- Row 2 layout: PATTERN gets `row.removeFromLeft(row.getWidth() - knobCellWidth)` and STEPS
  gets the remainder (the plan's earlier "full width pill" wording was wrong — the pill
  shares the row with the STEPS knob).

### 4.3 Layout — LANES page

With PATTERN and STEPS gone, the LANES column has five knobs (Pulses, Rotation, Accent, Flam,
Ghost) instead of six, and more vertical room per knob. Three adjustments:

- `rows` changes from 6 to 5 in the page-1 `knobRows[]` array.
- The freed `selectorCellHeight` band joins the knob rows' spare-height pool.
- Knobs may be **larger**: the owner explicitly allows this. Because the drawn knob size
  follows the cell height (§0.3), "larger" means **taller LANES knob cells**, a new
  LANES-only row constant (e.g. 92 px — the pre-0.7 suite cell — or ≥ 82 px for a full
  40 px knob), *not* a change to `LabelledSlider` or any drawing code (R1 is untouched).
  Five 92 px rows plus gaps fit the column's current height budget (the six 76 px rows
  already consumed ~540 px of the ~800 px column).

The owner's wording ("You may make … LANES-Page Knobs larger") is permission, not a
requirement; the smaller change (keep the current 76 px cells) is acceptable. Record the
chosen constant in this phase before implementing.

### 4.4 GRID knobs smaller

GRID's drawn knobs are ≈34 px today as a side effect of the 76 px cells (§0.3), even though
the local constant's name suggests a 28 px knob. The owner's "may make GRID knobs smaller"
can therefore mean two different things, and the owner picks one:

- **(a) Accept the status quo** — the GRID knobs already render smaller than the suite's
  40 px; do nothing. Cheapest, and consistent with how the plugin has shipped.
- **(b) Shrink the GRID row cells** (e.g. `knobRowHeight` 76 → 70 px) so the drawn knob
  drops to ≈31 px. This touches the shared `knobRowHeight` constant, which also sizes the
  GRID lane rows and the MUTATE group rows — if MUTATE must stay byte-identical (§7),
  introduce a GRID-rows-only constant instead of changing the shared one.

Either way this resolves plan-21 C2's "dead / misleading knob sizing" for GrooveMutator:
rename the local `knobDiameter` constant to what it sizes (e.g. `gridCellDiameter`), per
plan-21 Phase 4 step 10. The PATTERN pill uses `SuiteUI::pillHeight` (36 px) unchanged.

**Owner decision needed (R1 conformance):** every GrooveMutator knob renders < 40 px today.
Strict R1 would make them all 40 px (82 px cells); the owner's findings instead keep GRID
small. Record the owner's answer here before implementing §4.3/§4.4.

### 4.5 Affinity

`updateAffinity` dims `pulsesCtrl` and `rotationCtrl` based on pattern kind, and runs in both
`setPage` and the 60 Hz timer, so it re-evaluates wherever the controls live. After the move,
`pulsesCtrl`/`rotationCtrl` stay on LANES and nothing else is affinity-gated, so
`updateAffinity` itself needs no change. `testAffinity`'s `gm.setPage(1)` / `gm.setPage(0)`
dance keeps working (it exercises the Pulses/Rotation seams, which remain on LANES). One
UX note to carry into the manual: with PATTERN on GRID, a Steps lane's dimmed Pulses/Rotation
knobs are only visible on LANES — if the owner wants the dimming visible on GRID too, that
is a follow-up, not part of this plan.

### 4.6 Tests

- `testEditorPages`: the visibility assertion
  `isNoteDropdownVisible(track) == (page == 0)` stays. The test does not assert PATTERN/STEPS
  visibility today, so no change is strictly required, but add an assertion that
  `patternCtrl`/`stepsCtrl` are visible on GRID and not on LANES to lock in the new contract
  (mirroring the note-dropdown check). This requires exposing `isPatternVisible`/`isStepsVisible`
  seams, or checking via `getChildComponent`/component lookup. Prefer adding two small seams
  to `GrooveMutatorEditor` for testability, consistent with `isNoteDropdownVisible`.
- `testAffinity`: re-run; no expected change.
- Render test (R9): GRID and LANES pages re-render; MUTATE and MOD must be byte-identical.

---

## 5. Phase 3 — Sizing and window

1. Update `laneControlGroupHeight` and `editorHeight` (the derived sum, Phase 2).
2. Keep the shared `knobRowHeight` for the GRID lane rows and the MUTATE groups; the LANES
   page gets its own row constant (§4.3) so MUTATE and MOD renders stay byte-identical.
3. Update `setResizeLimits` only if the new height exceeds the current 1200 maximum (it does
   not at the plan's numbers: ≈1020).
4. Re-render all four pages; byte-compare MUTATE and MOD against the Phase 0 renders (they
   must be identical).
5. Owner approves the measured window size (R4).

---

## 6. Phase 4 — Tests and docs

1. Update `testEditorPages` visibility assertions (Phase 4.6).
2. Update `testAffinity` if needed (it should not change).
3. Update `MANUAL.md`: the GRID page now shows the lane name above ON and PATTERN+STEPS in
   the lane-control group; the LANES page shows five knobs. Regenerate screenshots
   (`SUITE_RENDER_DIR` → `docs/manuals/`, crops per R11).
4. Update AGENTS.md §4.9 **unconditionally** — the plan-22 layout description ("the pattern
   selector plus six pattern/articulation knobs" on LANES; the GRID row contents) changes
   regardless of the final window size — plus the window size once measured.
5. Update the `LaneCard` doc comment in `PluginEditor.h` (lines 66-68: pattern/steps are now
   GRID controls, not "the LANES knobs").
6. Update this plan's Status line, run the full §15 checklist, then delete the plan and
   remove its row from `docs/plans/README.md`.

---

## 7. Verification checklist

- Clean build: 0 errors, no new warnings (C4996 baseline ~31 from the shared router).
- Full CTest, default and bank; report entry counts.
- Before/after renders of every Groove Mutator page; MUTATE and MOD byte-identical;
  GRID and LANES visibly changed per the findings.
- `git diff --stat` shape check; `git status --short` shows only deliberate files.
- Not runnable here (report, never claim): macOS/AU, real-DAW behaviour.

---

## 8. Risks and open questions

- **R1 (knob sizes, needs an owner answer before §4.3/§4.4):** every GrooveMutator knob
  renders ≈34 px today (cell-height side effect, §0.3), below the suite's 40 px. The
  findings keep GRID small, so the owner is implicitly re-approving sub-R1 sizes for this
  plugin (the plan-21 C2 loose end). Options: accept ≈34 px everywhere (status quo), GRID
  smaller + LANES at the full 40 px (82 px cells), or full R1 conformance (all 40 px). No
  drawing-code change is involved in any option — only cell heights and the local constant's
  name.
- **R2 (group height):** growing `laneControlGroupHeight` by one row increases the window
  height (≈938 → ≈1020, inside the 1200 resize maximum); the owner must approve the
  measured size (R4).
- **R3 (test seams):** adding `isPatternVisible`/`isStepsVisible` to `GrooveMutatorEditor`
  widens the test surface; keep them `const noexcept` and read-only, matching
  `isNoteDropdownVisible`.
- **R4 (label reuse):** reusing the existing `name` label instead of adding `laneNameLabel`
  would couple GRID's static "LANE n" to the timer's note-name update. A separate label is
  safer and costs one extra widget per lane.
- **R5 (pill alignment):** the ON pill shifts down by the label band inside its cell while
  DIVISION keeps its full-height layout; check the Phase 1 render and bottom-align both
  pills if the offset reads as sloppy (§3.3).
