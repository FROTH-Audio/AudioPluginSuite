# Plan 12: DAW-test findings — fixes, rename and suite consistency

Written for: the coding agents who carry out the phases. Read `AGENTS.md` first; plan 05.1 pass 2
(Sgraffito voice fixes) and plan 11 (MidiEffect/StereoTool/Multiband) are the baselines for the
code this plan changes. Source of record: `owner_findings.md` at the repository root.

Status: proposed 2026-11-20. Not started. Owner decisions of 2026-11-20 are in §1.

Purpose: turn the owner's DAW-test notes into verified defects with fixes, plus one deliberate
consistency pass over the suite (one name for the final gain stage, one group-frame drawing, one
help-line mechanism, one dropdown widget). Every finding below was checked against the code on
2026-11-20; the line numbers are the evidence, not the plan's assumptions.

---

## 0. Verified current state (2026-11-20)

### 0.1 Sgraffito

| Finding | Verified cause |
|---|---|
| "Crackles at the end of the note with some presets (e.g. Deep Cloud, Swell Pad)" | Scrub/Chop/Swell one-shots stop with `voice.active = false` the instant the read position crosses the region boundary, at whatever level the sample happens to be there (`PluginProcessor.cpp:1074-1081`). No fade, no envelope release. Swell is the same test on `position <= regionStartF`. A second, rarer source: a stolen voice is overwritten with `voice = Voice {}` (`SamplerEngine.h`, `allocateVoice`), which cuts a sounding voice mid-sample. Cloud mode itself is windowed per grain (half-sine, `PluginProcessor.cpp:1043`) and is not a click source |
| Same | "Deep Cloud" ships **without** `mode=2`, so its three grain settings are inert and it plays Scrub (`tools/preset_data.py:317`). The Scrub region-end cut is exactly the audible click. "Swell Pad" is `mode=3` and hits the region-start cut (`:323`) |
| "Knob Level does not work" | `levelParam` is cached (`PluginProcessor.cpp:45`) and never read anywhere. The parameter exists (`:499`, −60 … +6 dB, default 0), the knob is bound and visible on the SOUND page, and the summed voice output never multiplies it (`:1064`) |

### 0.2 StereoTool

| Finding | Verified cause |
|---|---|
| "Should have an Input/Output Level knob/control" | The plugin has 11 parameters and no gain stage at either end (`PluginProcessor.cpp:60-90`). The identity early-return is `satAmount == 0 && cleanGain == 1 && sideGain == 1 && characterIndex == 0 && transientAmount == 0 && midFocus == 0 && sideFilterIndex == 0` (`:184-186`) |
| "Tooltip not working properly, shows always the same value from the first hover" | The editor has **no** help mechanism at all: no help label, no `HelpTracker`, no `setHelpText`/`setTooltip` on any control (repo-wide grep for `tooltip|help` in `plugins/StereoTool/Source` returns nothing) |

### 0.3 Dynamic Saturation

| Finding | Verified cause |
|---|---|
| "Tooltip not working (same as StereoTool)" | A `helpLabel` exists but is visible only on the MOD page (`PluginEditor.cpp:203`), no SOUND-page control carries a help text, and `showHelp` ignores an empty string (`:174-181`), so the label keeps whatever it last showed. SpectralDrift (`:173`) and MTC (`:431`) use the same stale rule; only Sgraffito clears back to an idle text (`:368-375`) |

### 0.4 MicroTimingChaos

| Finding | Verified cause |
|---|---|
| "Tooltip should explain the Sequencer View" | `StepGridView` is a plain `juce::Component` with no help text (`PluginEditor.h:21-38`, `PluginEditor.cpp:92`, bound at `:582`) |
| "Transport to right top, Melody right mid, Pattern on the left" | SEQUENCE page is three full-width bands: PATTERN (6 × 3 grid, full width), then MELODY + TRANSPORT side by side, then SEQUENCER (`PluginEditor.cpp:566-616`) |

### 0.5 Multiband Saturation

| Finding | Verified cause |
|---|---|
| "Rename to MultiSat (everything)" | `PRODUCT_NAME "Multiband Saturation"` (`plugins/MultibandSaturation/CMakeLists.txt:5`), which feeds `SUITE_PLUGIN_NAME` (About dialog, lockup text) and the host-visible name |
| "Preview at least 50 % higher" | `visualHeight = 130` (`PluginEditor.cpp:19`) |
| "Make the whole window width smaller" | `editorWidth = 980` (`:11`) for 11 controls laid out 2 + 4 + 5 across two groups |
| "Where are the group backgrounds/frames?" | `paint` draws only the two group **titles** (`:109-115`); no `fillRoundedRectangle`/`drawRoundedRectangle` for `splitGroupBounds`/`shaperGroupBounds`. StereoTool (`:117-123`) and MidiEffect (`:236-242`) have the same omission. Cyclical (`:365-386`), MTC (`:112-114`) and the SD/DS MODULATION group do draw frames |

### 0.6 MIDI Chord Arp Scale Tool

| Finding | Verified cause |
|---|---|
| "VST Instrument was the better choice, switch it back" | `IS_MIDI_EFFECT NEEDS_MIDI_INPUT` (`plugins/MidiEffect/CMakeLists.txt:9`); the built `Defs.txt` confirms `JucePlugin_IsSynth=0, JucePlugin_IsMidiEffect=1`, `Vst3Category="Fx"`. Buses declare a stereo input (`PluginProcessor.cpp:14-16`) and the audio is passed through (`processBlock`, `(void) buffer`) |
| "Pattern, Scale, Vel Curve should be dropdowns" | They are `LabelledSelector` pills (`PluginEditor.h:78-79, 88`), five, six and four options |
| "Step Rate should have a lot more options, also a dropdown" | Five choices (`PluginProcessor.cpp:91`), and `CoreDSP::Chords::arpStepBeats` hard-codes the same five (`ChordEngine.h`) |

### 0.7 Suite-wide

| Finding | Verified cause |
|---|---|
| "Master/Output Level/Level all do the same, should share one name" | SpectralDrift "Output Gain" (`:120`), DynamicSaturation "Output Level" (`:77`), Multiband "Output Level" (`:70`), Cyclical "Master Volume" (`:123`), Sgraffito "Level" (`:499`), StereoTool none. Captions differ too: OUTPUT GAIN / OUTPUT / MASTER / LEVEL |
| "Use suite design for dropdowns" | `SuiteLookAndFeel` overrides only `drawRotarySlider`, `drawLinearSlider` and `drawToggleButton`; there is no `drawComboBox`, so every dropdown is raw `LookAndFeel_V4`. Three local `ComboCell` copies exist (MidiEffect `PluginEditor.h:27`, StereoTool `PluginEditor.h:28`) and MTC uses a bare `juce::ComboBox` with a separate caption label (`PluginEditor.h:105-106`) |
| No working tooltip anywhere | The only `juce::TooltipWindow` in the repository is inside `SuiteSettings::SettingsDialog` (`SuiteSettings.h:112`). Plugin editors create none, so `setTooltip` calls in the preset bar and the settings dialog are the only ones that can ever appear |

---

## 1. Decisions (owner, 2026-11-20)

| Question | Decision |
|---|---|
| MultiSat rename scope | **Display name only.** `PRODUCT_NAME` becomes `"MultiSat"`, which also changes `SUITE_PLUGIN_NAME` (About dialog) and the host-visible name. Folder, CMake target, test executable names, bundle ID `com.audiosuite.multibandsaturation` and preset ID `"MultibandSaturation"` stay. The lockup PNG must be regenerated with the new text by the owner (same file name, `tools/brand/lockups.py`, an owner-local tool) |
| Final gain stage name | **"Output" everywhere.** Parameter display names become "Output", captions "OUTPUT". Parameter **IDs do not change** (`outputGainId`, `outputLevelId`, `masterVolumeId`, `levelId`), so no state migration. The SpectralDrift modulation target label "Output Gain" becomes "Output"; Sgraffito's target label "Level" becomes "Output". Sgraffito keeps its −60 … +6 dB range |
| MidiTool type | **Instrument, output only.** `IS_SYNTH TRUE`, `NEEDS_MIDI_INPUT TRUE`, `NEEDS_MIDI_OUTPUT TRUE`, `IS_MIDI_EFFECT` removed, buses = stereo output only, audio buffer cleared instead of passed through, VST3 category becomes Instrument. Each flag on its own line (AGENTS §11: a bare multi-flag line misparses) |
| Dropdown rule | **Only the named controls** become dropdowns: MidiEffect Pattern, Scale, Vel Curve, Step Rate (CHORD already is one). All other choice widgets keep their current pill/toggle. The shared dropdown widget still lands in SuiteUI, and MTC's raw combo box and the two local `ComboCell` copies migrate to it so there is one implementation |
| Tooltip mechanism | **Help line only.** One shared in-editor help line in every plugin, with an idle text and a clearing rule (hovering a control without help text restores the idle text). No floating `TooltipWindow` in plugin editors |
| Sgraffito | **Wire Level as the output gain** (renamed "Output"). **Leave Deep Cloud's authoring as is** (it stays Scrub); the click fix must therefore cover the Scrub/Chop/Swell region-boundary cut |

---

## 2. Phases

### Phase 1 — SuiteUI foundations

Files: `modules/suite_ui/include/SuiteHelp.h`, `modules/suite_ui/include/SuiteWidgets.h`,
`modules/suite_ui/Source/SuiteWidgets.cpp`, `modules/suite_ui/include/SuiteSelectorAttachment.h`,
`modules/suite_ui/Source/SuiteLookAndFeel.cpp`, `modules/suite_ui/tests/SuiteUITests.cpp`.

1. **`SuiteUI::HelpLine`** (a `juce::Component` in `SuiteWidgets.h`): a one-line label with
   `setIdleText (const juce::String&)`, `setText (...)`, and the rule that makes the owner's bug
   impossible — `showFor (const juce::Component* c)` always writes either the component's help
   text or the idle text, never "keep the last one". It draws in the 26 px band the editors
   already reserve for their footer (`helpHeight` in SD/DS/MTC, `footerHeight` in StereoTool,
   MidiEffect, Multiband, Cyclical, the status bar in Sgraffito), so no layout height changes.
2. **`SuiteUI::HelpTracker`** stays as is; add a small helper
   `attachHelpLine (juce::Component& owner, HelpLine&, HelpTracker&, std::function<bool()> enabled)`
   in `SuiteHelp.h` so the four hand-written `showHelp` copies disappear. `enabled` is the editor's
   `SuiteSettings::shared().getBool (SuiteSettings::tooltipsKey, true)` (SuiteUI must not include
   SuiteSettings, so the predicate is injected).
3. **`describe` overloads** for `LabelledSelector` (wrapper + caption + selector) and the new
   `LabelledDropdown` (wrapper + caption + box). Without them the deepest child under the pointer
   has no help text and the help line stays idle. `describe (LabelledSlider&)` already covers all
   three children.
4. **`SuiteUI::drawGroupFrame (juce::Graphics&, juce::Rectangle<int> bounds, const juce::String& title, juce::Colour accent)`**:
   one implementation of the group look — `panel.withAlpha (0.55f)` fill, `panelEdge` 1 px stroke,
   8 px radius, 15 pt bold accent title at the top-left. Every editor that has groups calls it;
   editors stop drawing titles by hand.
5. **`SuiteUI::LabelledDropdown`**: caption above, `juce::ComboBox` below, same footprint as
   `LabelledSelector`. The attachment helper (`attachDropdown`, mirroring `attachSelector`) goes in
   `SuiteSelectorAttachment.h` so SuiteUI stays free of `juce_audio_processors`.
6. **Combo styling in `SuiteLookAndFeel`**: `drawComboBox`, `positionComboBoxText`,
   `getComboBoxFont`, `drawPopupMenuBackground`, `drawPopupMenuItem`, `getPopupMenuFont` — the
   suite's panel/track/text colours, `controlRadius`, 36 px control height to match the pills.
7. Tests in `SuiteUITests`: the help line clears to idle text for a component without help text
   (the regression for the owner's report); the frame paints non-background pixels inside its
   bounds; the dropdown paints its caption and box and reports the selected item.

### Phase 2 — One help line in every editor, with text on every control

Files: all eight `plugins/*/Source/PluginEditor.{h,cpp}`, `plugins/*/Source/PluginProcessor.h`
(where a control's target name changes), `modules/suite_modulation/Source/SuiteModulationPanel.cpp`
(unchanged, already sets help text).

1. Replace the four local help implementations (SD `PluginEditor.h:37`/`.cpp:173`, DS `:37`/`:174`,
   MTC `PluginEditor.h:65`/`.cpp:431`, Sgraffito `:239`/`:368`) with `HelpLine` + `HelpTracker` +
   `attachHelpLine`. Keep each plugin's current static footer sentence as the idle text
   ("FFT 2048 / 75 % overlap …", "2x oversampled …", the MTC/DS help sentence, Sgraffito's
   `idleHelpText`, the ARP/SHAPE sentence in MidiEffect, the two-band sentence in Multiband, the
   mid/side sentence in StereoTool, Cyclical's footer text).
2. DS: the help line becomes visible on **both** pages (today `helpLabel` is page-1 only), and
   every SOUND-page control gets `describe()` text (mode, drive, sensitivity, curve, attack,
   release, mix, detect HPF, stereo link, bias, fold, tone, output) plus the transfer curve and
   the two meters.
3. StereoTool, MidiEffect, Multiband, Cyclical: add the help line (they already reserve the band)
   and help text on **every** control, selector, dropdown, visualiser and the preset bar.
4. MTC: `grid.setHelpText ("The sequencer view: one column per step. The dot shows the timing
   offset of the last pass, the bar the gate length; the outlined column is the current step.")`
   — the owner's explicit request. Also the attractor, readout, page selector and lerp view.
5. Sgraffito: reuse `HelpLine`; keep the `ui/tooltips` toggle behaviour, now shared by every
   plugin (the toggle currently affects Sgraffito only).
6. Tests (per plugin, in the existing editor tests): hovering a control with help text shows that
   text; hovering a blank area (or a component without help text) shows the idle text again —
   the stale-value regression. Sgraffito's existing `showHelp` test (`PluginTests.cpp:488`) is
   adapted to the shared component.

### Phase 3 — Sgraffito: the end-of-note click and the Output knob

Files: `plugins/Sgraffito/Source/PluginProcessor.{h,cpp}`,
`modules/core_dsp/include/SamplerEngine.h`, `plugins/Sgraffito/Source/PluginEditor.{h,cpp}`,
`plugins/Sgraffito/tests/PluginTests.cpp`.

1. **Region-boundary declick.** A one-shot that reaches its region boundary (forward for
   Scrub/Chop, backwards for Swell) enters a short fade instead of setting `active = false`
   (`PluginProcessor.cpp:1074-1081`). Add `int declickRemaining` to `Voice`; on crossing, set it
   to `declickSamples` (5 ms at the prepared sample rate, ≥ 1) and clamp the read position inside
   the region; multiply `level` by `declickRemaining / declickSamples`, decrement per sample, and
   deactivate at 0. Cloud mode is untouched (its grains are already windowed).
2. **Voice steal.** `VoicePool::allocateVoice` currently does `voice = Voice {}` on the stolen
   slot. Choose the quietest eligible voice (`envValue × gain × velocity`) inside the existing
   steal order (`findVoiceToAllocate`): tape-stop voices first, then the oldest release, then the
   quietest active voice. No new pool, no new thread interaction.
3. **Output gain.** Apply `levelParam` to the summed voice output once per sample through a
   `juce::SmoothedValue<float>` (20 ms) so automation does not zip; `0 dB` is exactly 1.0, so the
   default stays bit-identical. Rename the parameter to "Output" (`levelId` unchanged) and the
   caption to `OUTPUT` (`PluginEditor.h:292`); rename the modulation target label
   (`PluginProcessor.h`, `modulationTargets`, `{ "level", "Level" }` → `"Output"`).
4. Tests:
   - `testRegionEndIsDeclicked`: render a held note whose region ends mid-waveform (Deep Cloud's
     values: Scrub, `rootKey 84`, `attack 120`, `release 1500`) and assert the maximum
     sample-to-sample delta in the last 10 ms before silence is below a small threshold; the same
     for Swell Pad's values (Swell). The test must fail on the current code.
   - `testOutputGainScalesTheSum`: −6 dB halves the peak (within tolerance), +6 dB doubles it;
     default 0 dB leaves the output unchanged.
   - `testStolenVoicePrefersTheQuietest`: fill the pool with one loud and one quiet voice, request
     a new one, assert the quiet voice's slot is reused.

### Phase 4 — StereoTool: Input and Output

Files: `plugins/StereoTool/Source/PluginProcessor.{h,cpp}`, `PluginEditor.{h,cpp}`,
`tests/PluginTests.cpp`.

1. Two parameters, appended after `sideCrossover` so no saved index moves:
   `inputLevel` ("Input", −24 … +24 dB, default 0) and `outputLevel` ("Output", −24 … +24 dB,
   default 0). Both smoothed 20 ms.
2. Signal order: input gain → detector/character chain → mix → output gain. The identity
   early-return gains `inputGain == 1.0f && outputGain == 1.0f`, so the defaults stay bit-exact.
3. Layout: a new one-row `LEVEL` group occupying the right end of the visualiser band (two knobs
   side by side, `knobRowHeight` tall, the visualiser shrinks horizontally), so the window stays
   880 × 700 and no height is spent. `drawGroupFrame` from Phase 1 draws it.
4. Tests: `checkParameterIds` 11 → 13, `checkRange` for both; identity regression at defaults;
   −6 dB input halves the peak and −6 dB output halves it; the editor paints the new group.

### Phase 5 — MicroTimingChaos: SEQUENCE page layout

Files: `plugins/MicroTimingChaos/Source/PluginEditor.{h,cpp}`, `tests/PluginTests.cpp`.

1. Replace the three full-width bands with a two-column top block (`resized`, `:566-616`):
   - left column (~55 %): `PATTERN` with the six knobs in **2 columns × 3 rows**;
   - right column: `TRANSPORT` on top (tempo + speed knobs, rate pills, BPM Sync, host readout)
     and `MELODY` below it (root, octaves, scale — the raw `juce::ComboBox` becomes a
     `LabelledDropdown`);
   - `SEQUENCER` spans the full width below the block and keeps the remaining height.
   All three placements are exactly what the owner asked for. The `groups` vector keeps the
   titles in the new order so `paint` and `setPage` need no other change.
2. `grid.setHelpText (...)` (Phase 2 item 4) plus help text on the transport readout.
3. Tests: an editor test asserting `patternGroup` is left of `transportGroup` and `melodyGroup`,
   `transportGroup` is above `melodyGroup`, and the grid sits below all three (expose the group
   bounds through a small const accessor for the test); renders of the SEQUENCE page.

### Phase 6 — Multiband: "MultiSat" and the layout

Files: `plugins/MultibandSaturation/CMakeLists.txt`, `Source/PluginEditor.{h,cpp}`,
`Source/PluginProcessor.{h,cpp}`, `tests/PluginTests.cpp`, `MANUAL.md`,
`modules/suite_ui/brand/lockup_multiband_saturation.png` (owner-regenerated),
`README.md`, `AGENTS.md`, `CHANGELOG.md`, `docs/manuals/multibandsaturation-*.png`.

1. `PRODUCT_NAME "MultiSat"`; description stays meaningful ("Two-band …"). About and the host
   name follow through `SUITE_PLUGIN_NAME`. `getPresetPluginId()` stays `"MultibandSaturation"`,
   the folder and target stay, the bundle ID stays. Grep the repository for "Multiband Saturation"
   afterwards (AGENTS §15.9) and update only the user-facing occurrences.
2. Width 980 → **760** (`editorWidth`), `setResizeLimits` accordingly; keep 700 height.
3. `SPLIT` becomes a one-row group (crossover + mode, `groupHeight` for one row); `SHAPER` keeps
   two rows. The freed height goes to the visualiser: `visualHeight` 130 → **200** (≥ 50 %
   higher). The visualiser's internal bar layout scales with the bounds already.
4. `drawGroupFrame` for SPLIT and SHAPER (fixes the missing frames); help line in the footer band.
5. Tests: an editor test asserts `visualBounds.getHeight() >= 195` and `getWidth() < 900`;
   renders of the (now single-page) editor.

### Phase 7 — MidiTool: instrument, dropdowns, Step Rate

Files: `plugins/MidiEffect/CMakeLists.txt`, `Source/PluginProcessor.{h,cpp}`,
`Source/PluginEditor.{h,cpp}`, `tests/PluginTests.cpp`, `modules/core_dsp/include/ChordEngine.h`,
`modules/core_dsp/tests/DSPTests.cpp`, `MANUAL.md`, `README.md`.

1. **Instrument.** CMake: `IS_SYNTH TRUE`, `NEEDS_MIDI_INPUT TRUE`, `NEEDS_MIDI_OUTPUT TRUE`, each
   on its own line, `IS_MIDI_EFFECT` removed. Buses become output-only
   (`BusesProperties().withOutput (...)`); `processBlock` calls `buffer.clear()` instead of
   `(void) buffer`. Verify `Defs.txt` shows `JucePlugin_IsSynth=1`, `JucePlugin_IsMidiEffect=0`,
   `Vst3Category="Instrument|Synth"`.
2. **Step Rate.** Extend `CoreDSP::Chords::arpStepBeats` and the parameter's choice list to twelve
   values: `1/4, 1/4T, 1/4D, 1/8, 1/8T, 1/8D, 1/16, 1/16T, 1/16D, 1/32, 1/32T, 1/32D` (beats:
   1, 2/3, 1.5, 1/2, 1/3, 0.75, 1/4, 1/6, 3/8, 1/8, 1/12, 3/16), default `1/16`. The clean order
   needs a **state migration**: bump `stateVersion` to 2 and map the five old indices
   (`0→1/8, 1→1/8T, 2→1/16, 3→1/16T, 4→1/32`) to their new positions, with a fixture test
   (AGENTS §9, "Changing a saved parameter or state layout"). Nothing is released, but the
   migration is cheap and keeps the rule honest.
3. **Dropdowns.** Pattern, Scale, Vel Curve and Step Rate use `SuiteUI::LabelledDropdown`; CHORD
   moves from `ComboCell` to it too; `ComboCell` is deleted. MODE, SYNC and VEL DIR stay pills.
4. Tests: `arpStepBeats` for all twelve indices; the migration (a v1 state with `stepRate` 2
   loads as `1/16`); the editor shows five dropdowns (Pattern, Scale, Vel Curve, Step Rate, Chord)
   and three pills (Mode, Sync, Vel Dir); `checkParameterIds` count unchanged (14); renders of the
   editor.

### Phase 8 — One name for the final gain stage

Files: `plugins/SpectralDrift/Source/PluginProcessor.cpp` (+ header target table),
`plugins/DynamicSaturation/Source/PluginProcessor.cpp`,
`plugins/MultibandSaturation/Source/PluginProcessor.cpp`,
`plugins/Cyclical/Source/PluginProcessor.cpp`, `plugins/Cyclical/Source/PluginEditor.h`,
`plugins/Sgraffito/Source/PluginProcessor.cpp` (target table), the matching
`tests/PluginTests.cpp` where a name is asserted.

1. Display name "Output" in all five plugins; captions `OUTPUT` (SpectralDrift `OUTPUT GAIN` →
   `OUTPUT`, Cyclical `MASTER` → `OUTPUT`, Sgraffito `LEVEL` → `OUTPUT`; DS and Multiband are
   already `OUTPUT`). Parameter IDs unchanged.
2. Modulation target labels: SpectralDrift `"Output Gain"` → `"Output"` (index unchanged),
   Sgraffito `"Level"` → `"Output"` (index unchanged). Cyclical and StereoTool have no routing.
3. Check every place a name is duplicated: `createParameterLayout` display name, editor caption,
   target table, help text, `MANUAL.md`, `README.md`, AGENTS §4, the factory preset descriptions
   (they use IDs, so values do not change; only prose may mention "master volume").
4. Tests: no test asserts the display name of these parameters today; add one per plugin
   (`apvts.getParameter (id)->getName (100) == "Output"`) so the sweep cannot regress.

### Phase 9 — Documentation, renders, verification

Files: `AGENTS.md`, `README.md`, `CHANGELOG.md`, `docs/manuals/*.png`,
`plugins/*/MANUAL.md`, `docs/plans/README.md`.

1. Regenerate every affected manual screenshot (R9/R11) with
   `SUITE_RENDER_DIR=<repo>/docs/manuals`: Sgraffito (Output caption, help line), StereoTool
   (LEVEL group, help line), DS (help line on both pages, Output caption), SD (help line, Output
   caption), MTC (new SEQUENCE layout, help line), Multiband (name, width, taller visual, frames,
   help line), MidiEffect (dropdowns, help line), Cyclical (help line, Output caption). Cut new
   detail images with `tools/manuals/crops.py` where the change needs one (MTC SEQUENCE groups,
   StereoTool LEVEL, Multiband frames) and re-run `tools/manuals/scale_images.py` if the set
   changes.
2. Manual prose: MTC SEQUENCE page walkthrough (new group order), Multiband page (name, layout),
   StereoTool (Input/Output), MidiEffect (instrument, dropdowns, twelve step rates, the type
   change's consequence: no audio passes through), Sgraffito (Output).
3. `AGENTS.md`: §1 table (Multiband name note), §4 parameter tables (StereoTool 11 → 13,
   MidiEffect Step Rate list, the Output names), §4.6 Step Rate values, §5.1/§5.2/§5.3 where
   behaviour changed (Sgraffito declick + Output, MidiEffect instrument), §7 the new rules
   (help line, group frame, dropdown widget, the "Output" name), §9 the two new procedures,
   §11 the gotchas if new ones appear, §13 Sgraffito notes.
4. `CHANGELOG.md`: one entry per finding, in the owner's words.
5. Full verification: clean Release build (0 errors, compare the warning count against the
   baseline of 31 pre-existing C4996), `ctest` 28/28 with `SUITE_PRESET_BANK=ON`,
   `python tools/audit_bank.py`, and `git diff --stat` sanity (AGENTS §15).

---

## 3. Open points and risks

| Item | Note |
|---|---|
| MultiSat lockup | The embedded PNG still reads "Multiband Saturation" until the owner runs the lockup generator. Gate: the owner regenerates `lockup_multiband_saturation.png` before the rename is called done. The About dialog reads `SUITE_PLUGIN_NAME`, so it updates with the build |
| Multiband width 760 | SHAPER's five-knob row needs ~5 × 140 px of inner width. Verify at 760 before fixing the constant; if it does not fit, use 800 (still "smaller") and record why |
| MTC PATTERN in two columns | Six knobs in 2 × 3 rows inside ~55 % of 980 px = ~250 px per knob cell, well above the 40 px knob. The step grid loses width if the block grows; check the grid's minimum height after the change |
| Step Rate migration | Twelve values in a clean order need `stateVersion` 2 and a fixture. Alternative: append the seven new values after the old five (no migration, ugly dropdown order). The plan takes the migration; the owner may override |
| MidiTool as instrument | The standalone build no longer accepts audio input, and no audio passes through. The plugin tests that call `processBlock` with a buffer must still pass (the buffer is cleared, not read). The VST3 host test still instantiates and opens the editor |
| StereoTool identity | `inputGain`/`outputGain` must be exactly 1.0 at 0 dB (use `Decibels::decibelsToGain (0.0f) == 1.0f`, which it is) and the smoothed value must start at the target, or the first block breaks the bit-exact default |
| Sgraffito declick | 5 ms at 44.1 kHz is 220 samples; a very short region (5 % of a short sample) could be shorter than the fade. Clamp the fade to the remaining region length |
| Deep Cloud | Stays Scrub by owner decision. If the owner later wants it to sound like a cloud, that is a preset-value change (bank order unchanged) |
| Renders on CI | Editor tests need a desktop session; a headless run can fail for reasons that are not code bugs (AGENTS §10). Report it, do not "fix" it |

---

## 4. Verification checklist (per phase and at the end)

1. `--target clean`, full Release build, 0 errors, warning count compared with the 31 baseline.
2. `ctest --test-dir build -C Release --output-on-failure`: 28/28 with the bank on. A changed
   entry count is a changed test set — report it.
3. `python tools/audit_bank.py` (160 presets, unchanged).
4. Before/after renders for every editor whose layout, colour or wording changed, stored under
   `build/renders/` (AGENTS §15.7). No render means no claim of "no visible change".
5. `git grep` for the old display names ("Output Gain", "Master Volume", "Multiband Saturation"
   in user-facing text) and for `ComboCell`.
6. `git status --short`: nothing untracked except the deliberate new plan file.
7. Report what was not checked: macOS/AU, the standalone instrument build, DAW loading.

---

## 5. Owner gates

| Gate | When |
|---|---|
| Regenerate the MultiSat lockup (`tools/brand/lockups.py`, owner-local) | Before Phase 6 is called done |
| Confirm the Multiband width if 760 does not fit the five-knob row | Phase 6 |
| Confirm the Step Rate migration (v2) versus append-only order | Phase 7 start |
| Review the regenerated manual screenshots | Phase 9 |
