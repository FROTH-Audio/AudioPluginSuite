# Plan 05: Suite-wide UI rework — one control language, smaller everywhere

Written for: the coding agents who carry out the phases, with owner gates at Phase 0 and the
MTC decision. Read `AGENTS.md` first; this plan changes `AGENTS.md` section 7 when done.

Status: proposed 2026-11-11. Not started.

Purpose: the plugin windows are too big (worst: Micro-Timing Chaos at 1133×920 displayed,
Sgraffito at 880×820), the knobs differ in size and appearance between plugins, and the
toggle buttons and selectors grow to whatever a layout hands them (row-filling ellipses,
seen in the Sgraffito screenshots of 2026-11-11). This plan defines one mandatory set of UI
rules, enforces them in the shared `SuiteUI` module so no editor can regress, shrinks every
rotary control to 2/3 of its current size, makes switches readable at a glance, and brings
every window under one size budget. Future plugins get the rules through the skeleton.

References: `juce-plugin` (attachment order, editor lifetime, no work in paint),
`juce-framework` (UI never blocks the audio thread), `frontend-design` (one house style,
restraint, hierarchy over decoration; the brief's own house look wins over inventing a new one).

---

## 0. Decisions

| Question | Decision |
|---|---|
| Knob size | One diameter for the whole suite: `knobDiameter = round(2/3 × current maximum measured in Phase 0)`. Expected 72–80 px; the exact value is pinned in Phase 0 and recorded in `AGENTS.md` section 7 |
| Knob appearance | The shared LED-ring drawing (`SuiteLookAndFeel::drawRotarySlider`) is the suite's knob. Micro-Timing Chaos stops drawing its own |
| Enforcement point | `SuiteUI::LabelledSlider` enforces the knob size itself in `resized()`, centred in its cell. Editors stop capping knobs locally (Sgraffito's `layoutKnobCell` goes away) |
| Buttons and selectors | Fixed heights (36 px pills), never fill a row. 14 pt bold labels. The on state must read at a glance without colour alone (see R6) |
| Window budget | Every editor fits a default window of at most 1000×700 at scale 1.0. Per-plugin numbers pinned in Phase 0 |
| MTC scale factor | **Dropped (owner decision, 2026-11-11).** Phase 3 redesigns Micro-Timing Chaos natively at the R4 budget; the ×2/3 `setScaleFactor` and the 1695 px minimum width go away with it |
| Typography | `AGENTS.md` section 7's table stands (15 pt group titles in the accent, 13 pt captions, 14 pt values) and is extended by R5 below. One font family across the suite; candidate: Bahnschrift (the brand font, Windows system) with the JUCE default as the macOS fallback — owner decides in Phase 0 |
| New plugins | `tools/new_plugin/skeleton` gets the rules baked in, so a fresh editor cannot violate them |
| Not in scope | Colours, the brand lockup, the waveform/key-map widgets' internals, and the DSP. Pages may be re-flowed but no parameter moves, no parameter is added or removed |

---

## 1. Current state (measured, to re-verify in Phase 0)

| Plugin | Window | Knob today | Switches today |
|---|---|---|---|
| Spectral Drift | 780×776, SOUND/MOD pages, rows split into 4 cells | Fills the cell (`LabelledSlider::resized()` hands the slider all space below the caption); roughly 110–130 px | SegmentedSelector fills its strip |
| Dynamic Saturation | 880×660 | Same mechanism | Same |
| Micro-Timing Chaos | Design 1695×1380 shown at ×2/3 (≈1133×920), min width 1695 | Own `drawRotarySlider` in `MicroTimingChaosLookAndFeel.cpp`, bounds-scaled (reduced 5 px) | Own toggle/combo drawing |
| Sgraffito | 880×820 | Editor-local cap (`layoutKnobCell`, ~90 px) — the only plugin with a cap | 36 px pills since the 2026-11-11 rework, but by editor-local code, not a rule |

Root cause: the shared `SuiteLookAndFeel::drawRotarySlider` and `drawToggleButton` scale to
the component bounds, and `LabelledSlider` gives the slider the whole cell. Size is a
per-editor accident. The MTC look and feel diverged to keep its window unchanged during the
plan 02 refactor.

---

## 2. The mandatory rules (become `AGENTS.md` section 7)

These are written once, here, and enforced in code. `AGENTS.md` section 7 is rewritten to
contain them verbatim when the plan is done.

- **R1 — One knob.** Every rotary control is a `SuiteUI::LabelledSlider` with a rotary style.
  The knob diameter is `SuiteUI::knobDiameter`, identical in all plugins. Editors may not cap,
  scale, or draw knobs themselves.
- **R2 — One scale.** The knob, its caption and its value box form one unit; caption above,
  value box below, both centred, per `AGENTS.md` section 7 typography. No editor adds a second
  value display for the same parameter.
- **R3 — Pills have a fixed size.** `ToggleButton` and `SegmentedSelector` render at
  `SuiteUI::pillHeight` (36 px) and never fill their cell; layouts centre them. Corner radius
  6 px everywhere a control is rounded.
- **R4 — Window budget.** An editor's default window is at most 1000×700 at scale 1.0, with
  resizable limits that keep every group visible. If a page cannot fit, it gets a second page
  (the SOUND/MOD pattern), not a bigger window.
- **R5 — Type.** Group titles 15 pt bold in the plugin accent; knob captions 13 pt bold in
  `textDim`; value text 14 pt in `text`; help/footer lines 11 pt `textDim`. One font family
  suite-wide (owner picks: Bahnschrift with JUCE default fallback, or JUCE default).
- **R6 — Switches read as state.** On: accent border, accent text, accent-tinted fill, and a
  filled state dot at the left edge. Off: `panelEdge` border, `textDim` text, hollow dot.
  Colour is never the only carrier of the state (accessibility).
- **R7 — Spacing.** One `groupGap` (8 px) between groups and around controls; no per-plugin
  margins outside the shared constants.
- **R8 — Pages over size.** When a page has more controls than the budget holds, split into
  pages with the shared `SegmentedSelector` page switch at the top right of the header.
- **R9 — Renders are the contract.** Every plugin's test executable writes per-page renders
  when `SUITE_RENDER_DIR` is set (the existing `testRenderBaseline` pattern). A UI change
  ships with before/after renders under `build/renders/` and a statement of which pages were
  meant to change.
- **R10 — Future plugins inherit.** `tools/new_plugin/skeleton` produces an editor that
  already follows R1–R8; `new_plugin.py` stays the only supported way to start a plugin.

Design intent (from the frontend brief): the LED-ring knob is the suite's one signature
element — spend the boldness there. Everything else stays quiet: flat panels, hairline
`panelEdge` strokes, no gradients except the knob body that already exists. Switches gain
readability, not decoration.

---

## 3. Phases

### Phase 0 — Measure and pin (owner gate)

1. Render every page of all four plugins into `build/renders/ui-rework/before/`
   (`SUITE_RENDER_DIR` + each plugin's `testRenderBaseline`; Sgraffito's exists, SD has
   one, DS/MTC use their existing render tests).
2. From the renders and the code, record per plugin: knob diameter, caption/value font
   sizes, pill heights, window size, group gap. A table appended to this plan.
3. Compute `knobDiameter = round(2/3 × max measured)` and the per-plugin window sizes under
   the R4 budget. Pin the exact numbers.
4. Owner approves the token sheet before any code changes.

### Phase 1 — Enforce the rules in SuiteUI

Files: `modules/suite_ui/include/SuiteLookAndFeel.h`, `Source/SuiteLookAndFeel.cpp`,
`include/SuiteWidgets.h`, `Source/SuiteWidgets.cpp`, `tests/SuiteUITests.cpp`.

1. Add the constants: `knobDiameter`, `pillHeight`, `controlRadius`, `captionHeight`,
   `valueBoxWidth/Height`. No per-plugin copies remain.
2. `LabelledSlider::resized()`: caption row (`captionHeight`), then the slider centred with
   the knob at `knobDiameter` (rotary) regardless of cell size. The rotary value box keeps
   its 84×18 textbox; width capped to the knob.
3. `drawRotarySlider`: keep the LED ring, track arc, glow, body and pointer. At 2/3 size the
   proportional details need floor values so the ring stays legible — LED dot radius and the
   9 px arc inset become fixed px (exact values in Phase 0's table). No allocations, no font
   lookups per LED.
4. `drawToggleButton` and `SegmentedSelector::paint`: implement R3/R6 (state dot, fixed
   radius, on/off contrast). The pill draws from its own height, not the bounds'.
5. Update `SuiteUITests` paint checks: they assert painted pixels, so they should pass
   unchanged; add one assert per new constant if cheap.

### Phase 2 — Converge Micro-Timing Chaos

Files: `plugins/MicroTimingChaos/MicroTimingChaosLookAndFeel.h`,
`Source/MicroTimingChaosLookAndFeel.cpp`, `Source/PluginEditor.{h,cpp}`.

1. Delete its private `drawRotarySlider`, toggle and combo drawing where the shared version
   now matches; keep the BPM Sync icons, textures and fonts it genuinely needs.
2. Its editor switches to the shared `LabelledSlider` behaviour (no local knob bounds).
3. Owner decision recorded 2026-11-11: the ×2/3 `setScaleFactor` is **dropped**. Remove it
   together with the Phase 3 relayout — removing it alone would show the old 1695×1380
   design at true size, larger than today.

### Phase 3 — Relayout every editor to the budget

Files: the four `plugins/<Name>/Source/PluginEditor.{h,cpp}`.

1. Spectral Drift: re-flow SOUND and MOD pages under the R4 budget; remove the 780×776
   constants in favour of the pinned size.
2. Dynamic Saturation: same, 880×660 → pinned size.
3. Micro-Timing Chaos: re-flow the five groups (lerp, pattern, melody, chaos, transport,
   attractor, step grid, modulation) under the budget **at native scale — the ×2/3
   `setScaleFactor` override is removed here**, so the design height shrinks from 1380 to
   the pinned size and every DAW sees true-size fonts. The resizable "attractor and step
   grid stretch" behaviour stays. This is the plan's biggest single relayout; if the groups
   cannot fit one page, R8 applies (second page).
4. Sgraffito: delete the local `layoutKnobCell`/`layoutPill` caps; the widget now enforces
   them. Window to the pinned size.
5. Every editor reads only `SuiteUI` constants for sizes it does not own (waveform and
   visualiser internals stay plugin-owned).

### Phase 4 — Verify and document

1. Full clean Release build; 0 errors; warnings counted and compared (Sgraffito must stay at
   0; the pre-existing C4996 AudioParameterFloat warnings are the baseline).
2. All CTest entries green (11 with `SUITE_PRESET_BANK=OFF`, 15 with `=ON`).
3. After-renders of every page into `build/renders/ui-rework/after/`; the gallery is the
   visible proof. Byte-compare is not expected to pass for pages that were re-laid out —
   the after-renders and this plan's table are the record instead.
4. `AGENTS.md` section 7 rewritten to the R1–R10 rules with the pinned numbers; the section
   1 table's window sizes and section 14's editor notes updated; plugin READMEs and
   `CHANGELOG.md` updated; `docs/plans/README.md` marks this plan done.

---

## 4. Risks and known traps

- **Legibility at 2/3.** The LED ring's 3.2 px dots and the 9 px arc inset are tuned for
  ~120 px knobs. At 72–80 px they need fixed-px floors (Phase 1) or the ring turns to mush.
  The after-render at 100 % zoom is the acceptance check.
- **MTC render baselines.** Plan 02 recorded a 3-glyph-edge-pixel cross-process difference in
  the PREVIEW view. Expect the same flakiness; judge by the majority of two runs.
- **MTC native redesign is the deep end.** With the scale factor dropped (owner decision),
  the 1695×1380 design must become a native ≈1000×700 layout: the melody/chaos side-by-side
  row (~1700 px today) and the 980-px column widths all re-flow. Expect a page split (R8).
  The setScaleFactor override in `PluginEditor.h` also gates `setResizeLimits` — both change
  together. MTC render baselines (plan 02's 3-glyph-edge-pixel flakiness) apply.
- **Attachment order and lifetimes** (juce-plugin): attachments are created after
  `addAndMakeVisible`; `checkEditor`/`editorBeingDeleted` patterns in the tests already
  handle the JUCE 9 editor lifetime — keep them.
- **No allocation in paint.** The knob and pill drawing must not construct fonts or images
  per call (the 60 Hz timer repaints the whole editor; `SuiteUI::kRefreshHz` stays).
- **Modulation panel and control display** (`SuiteModulationPanel`, `ControlDisplay`) sit on
  `LabelledSlider`, so the R1 change flows through them; the MOD pages shrink with the knobs.
  The panel's own row heights may need one pass to fit the budget.
- **Bahnschrift is Windows-only.** If chosen, `FontOptions` with a fallback family must be
  verified on macOS in CI (plan 01's open R0 covers macOS).

---

## 5. Notes for the next agent

- Phase 0 is measurement only; do not touch code. Append the token sheet to this file and
  stop for the owner's approval.
- Do not commit or push; the owner commits between phases.
- Do not resize or restyle the waveform view, key map strip, spectrum display, trail plot or
  attractor internals in this plan — they keep their own drawing; only their containers move.
- Parameter IDs, ranges, defaults, state version and presets are untouched; the bank tests
  must not change.
- Record what you verified and what you could not (macOS, AU, a DAW) in the usual way.

---

## 6. Phase 0 token sheet (measured 2026-11-11, approved: knob 60 px, Bahnschrift)

Status: Phases 1–4 implemented the same day. The before renders are under
`build/renders/ui-rework/before/`, the after renders under `build/renders/ui-rework/after/`
(9 pages: SD sound/mod, DS sound/mod, MTC sequence/drift/mod, Sgraffito sound/texture).
All 15 CTest entries pass after the rework.

Before-renders: `build/renders/ui-rework/before/` (8 pages, all four plugins, one process).
Knob diameters computed from the deterministic drawing path
(`SuiteLookAndFeel::drawRotarySlider` reduces the slider bounds by 3 px per side; the LED
ring sits 2 px inside that circle) and cross-checked against accent-pixel clusters in the
renders. LabelledSlider chain: cell − 8 (cell padding) − 18 (caption) − 18 (value box).

### 6.1 Measured today

| Plugin | Window | Knob drawn | Knob displayed | Pill heights today | Notes |
|---|---|---|---|---|---|
| Spectral Drift | 780×776 | **58 px** (row = content/4 ≈ 108) | 58 px | Freeze 44, selectors 30 | 4 knobs per row |
| Dynamic Saturation | 880×660 | **87 px** (row = content/2 = 137) | 87 px | Mode selector 30 | 3 rows of 4; largest knob shown today |
| Micro-Timing Chaos | 1695×1380 design, ×2/3 shown ≈ 1133×920 | **110 px** (width-capped by `knobMaxWidth`) | ≈ 73 px | Own toggle/combo drawing | Scale factor ×2/3 |
| Sgraffito | 880×820 | **70 px** (editor-local cap) | 70 px | 36 (editor-local rule) | 5 knobs per row |

Caption font: `LabelledSlider` uses 12 pt bold today (AGENTS §7 says 13 pt — R5 fixes the
widget). Value box 84×18 everywhere. Group titles 15 pt accent (SD 26 px title band,
Sgraffito 20 px — inconsistent; R7 pins it).

### 6.2 Pinned tokens (proposal — owner approves)

| Token | Value | Derivation |
|---|---|---|
| `knobDiameter` | **60 px** | 2/3 × 87 (the largest knob displayed today, DS), rounded up from 58 for crisp LED geometry. Alternative if "current" meant MTC's drawn 110: 73 px — owner picks |
| `knobRowHeight` | **112 px** | caption 18 + knob 60 + value box 18 + 16 padding |
| `pillHeight` | **36 px** | Sgraffito's proven height; SD's Freeze 44 and selectors 30 converge here |
| `controlRadius` | **6 px** | one radius for every rounded control |
| `captionHeight` | **18 px**, font 13 pt bold `textDim` | R5; also fixes the 12 pt drift |
| `valueBox` | 84×18, font 14 pt `text` | unchanged |
| Group title band | **20 px**, 15 pt bold accent | SD's 26 px band converges to Sgraffito's |
| `groupGap` | **8 px** | already shared |
| Knob internals at 60 px | LED dot 3.2 px; ring→arc gap 4 px (fixed, was proportional 9); arc stroke 3 px; body outline 1 px; pointer 2.5 px | floors so the ring stays legible at 2/3 |
| Font family | Owner picks: Bahnschrift (brand font, Windows) with JUCE default fallback, or JUCE default everywhere | R5 |
| SD window | **720×660** | spectrum ~30%, 4 knob rows at 112, preset bar stays in the header |
| DS window | **780×640** | curve row 160 + 3 knob rows at 112 |
| MTC window | **980×700** (native scale, no `setScaleFactor`) | five groups re-flowed; R8 page split if the Phase 3 layout cannot fit |
| Sgraffito window | **880×640** | mode row 76 + waveform (fills) + key strip 46 + 2 knob rows + scale strip |
| Suite budget | ≤ 1000×700 default | R4 |

All four windows shrink: SD 780×776 → 720×660, DS 880×660 → 780×640,
MTC ≈1133×920 → 980×700, Sgraffito 880×820 → 880×640.
