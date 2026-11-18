# Plan 10: Sgraffito — the owner's eight findings

Written for: the coding agents who carry out the phases, and for the owner who raised the
findings on 2026-11-17 (after the T1/T2 crash-and-audio fixes landed). Read `AGENTS.md`
first; section 14 is the Sgraffito baseline, and the T2 fixes (per-sample voice state,
memory floor, tape-stop level, surface factors) are in the code and in `CHANGELOG.md`.

Status: **implemented 2026-11-18** (phases 1-8 + 9; bank build CTest 16/16, audit passed, renders
under `build/renders/mode-affinity/`, `build/renders/envelope/` and `build/renders/mod-page/`). Corrections of record
made during execution:
- The phase 5b legibility fix also gained explicit BROWSE button colours and captions — the
  rewritten dialog had `TextButton`s with **no text** (`juce::TextButton presetFolderBrowse;`
  default-constructs empty), which is why the owner's screenshot showed unreadable pills even
  beyond the host-L&F issue. The recorded PNG initially looked unchanged across builds because
  a JUCE `FileOutputStream` **appends** to an existing file: the test now deletes the target
  first and asserts the reloaded PNG matches (a JUCE `FileOutputStream` never truncates).
- Phase 7's size estimate in G7 was wrong arithmetic: 3 s at 48 kHz/16-bit mono is ~288 kB,
  not ~115 kB, so the 20 samples total ~5 MB, not ~2 MB. Format (48 kHz mono 16-bit) and
  duration (1.5-3 s) were the approved constraints and are kept; the owner can shorten the
  set later by trimming the recipes in `tools/sgraffito_samples.py` and re-running it.

Purpose: eight findings from playing the plugin. Each phase below states the verified
current state (file and line references checked 2026-11-17), the design, the exact steps,
and the regression test that fails before and passes after. Anyone should be able to
execute a phase from this document alone; the checks in `AGENTS.md` section 15 apply to
every phase.

---

## 0. The eight findings and where they land

| # | Finding | Phase | Size |
|---|---|---|---|
| 1 | Controls with no role in the current mode should be disabled or hidden | Phase 2 | small |
| 2 | LOAD SOUND sits right-middle; belongs right-top | Phase 1 | one-liner |
| 3 | Attack and Release mostly have no effect; envelope display? | Phase 3 | medium (DSP) |
| 4 | Rename "Region Span" to "Region Start"? | Phase 4 (with #6) | **Decided: no rename**; Root Key becomes the pitch centre |
| 5 | Status bar below the keyboard showing per-control help | Phase 5 | medium (UI) |
| 6 | Explain the Root Key ↔ Region Span relation | Phase 4 | docs + decision |
| 7 | Sample does not switch back when preset and sample change | Phase 6 | decision + small |
| 8 | One generated sample per preset; preset overhaul | Phase 7 | large |
| 9 | Settings dialog shows but is barely readable (screenshot 2026-11-17 22:51) | Phase 5b | small |
| 10 | DS MOD page: curve preview behind the modulation grid (owner report 2026-11-18) | Phase 9 | small |

Execution order: 1 → 2 → 3 → 4 → 5 → 5b → 6 → 7 → 8. Rationale: the one-liner first, then the
editor-mode work, then the DSP, then Root Key's semantics (the help texts of phase 5 are
written against the final behaviour), then the sample pipeline last because it regenerates
the factory bank that every earlier phase's tests run against.

---

## 1. Verified current state (2026-11-17)

### 1.1 Region maths (#4, #6)

`modules/core_dsp/include/SamplerMaps.h:34-41`:

```cpp
inline Region keyRegion (int key, int rootKey, double span, double length) noexcept
{
    const double position = (double) key / 127.0;   // key 0 → 0, key 127 → 1
    const double maxStart = 1.0 - std::clamp (length, 0.0, 1.0);
    const double start = std::clamp (position * std::clamp (span, 0.0, 1.0), 0.0, maxStart);
    const double end = std::clamp (start + std::clamp (length, 0.0, 1.0), 0.0, 1.0);
    return { start, end - start };
}
```

- The region start of a played key is `(key / 127) × span`, clamped so the region fits.
  **"Region Span" is not a start position** — it scales how far across the sample the
  keyboard reaches. Low keys read near the sample start, high keys approach
  `span × sample length`.
- **`rootKey` is a dead argument**: `makeRegion` (`modules/core_dsp/include/SamplerEngine.h:303-314`)
  calls `keyRegion (key, 60, ...)` with a hardcoded 60, and the body never reads it.
  The Root Key knob does **not** move any region.
- Root Key today does exactly two things: it is the centre of Scale Lock
  (`quantizeToScale (key, rootKey, scale)`, `PluginProcessor.cpp:303-306`) and it is the key
  the waveform preview displays (`computeRegion`, `PluginEditor.h:160-178`, uses
  `rootKeyParam` as the *key*). It does **not** change pitch either:
  `setupAllocatedVoice` (`PluginProcessor.cpp:274-277`) computes
  `semitones = (key − 60) + pitchFine/100` against a fixed middle C, not against Root Key.
- The editor's region preview shows the **root key's** region; the played note always uses
  its own key's region. Both paths mirror the same maths and both carry a
  "keep this in step" comment.

### 1.2 Envelope (#3)

- **Attack is dead on direct notes.** `handleNoteOn`'s direct path
  (`PluginProcessor.cpp:356-360`) calls `voicePool.allocateVoice (key, velocity, spanMod,
  lengthMod, mode, sliceCount)` without `startSample`, so `Voice::envStart` defaults to
  `0.0` (`SamplerEngine.h:225`). The envelope (`SamplerEngine.h:164-179`) computes
  `progress = (sampleIndex − envStart) / attackSamples`, and `sampleIndex` is the absolute
  stream position (tens of thousands), so the attack is always "finished": `envValue` is 1
  from the first sample. Only the pending/ghost paths set `envStart` correctly
  (`PluginProcessor.cpp:855-859`).
- **Release is dead at the defaults.** `releaseVoice` (`SamplerEngine.h:245-271`): when
  `tapeStopTimeMs > 0` (the default is 400 ms, `PluginProcessor.cpp:506-508`) the voice
  takes the tape-stop branch and `envEnd` is never set, so the envelope's release curve is
  never used — the tail is the tape-stop ramp alone. The Release knob (200–5000 ms) only
  shapes the tail when Tape Stop Time is 0.
- One-shot voices end at the region end without a note-off; no release tail exists for
  them (correct sampler behaviour, but the owner should know it is why short regions
  "ignore" Release).
- The waveform view (`PluginEditor.h:26-188`) draws min/max bars per pixel column in a loop
  (`:82-107`); there is no envelope overlay today. The region overlay and Chop lines are at
  `:56-80`; the "Region 7% - 57%" caption at `:109-114`.

### 1.3 Editor: button, mode, pages, help (#1, #2, #5)

- LOAD SOUND is a child of `waveformView` (`PluginEditor.cpp:59`) but is positioned with
  `waveformView.getBounds()` (`PluginEditor.cpp:459-462`) — **editor** coordinates applied
  to a **waveform-local** `setBounds`. The waveform's top edge sits ≈104 px down in editor
  coordinates, so the button lands vertically centred on the waveform's right edge — exactly
  the owner's screenshot. The fix is `waveformView.getLocalBounds()`.
- There is **no per-mode visibility or enabling anywhere** (grep: no `setEnabled`, no
  mode-dependent `setVisible`). Only the waveform's Chop lines and region maths react to
  the mode. The mode reaches the editor through a `ParameterAttachment`
  (`PluginEditor.cpp:191-195`); `modeSelector.onChange` (`:194`) only pushes the value back.
- TEXTURE-page grouping (`PluginEditor.cpp:487-562`): Slice Count sits in the CLOUD group
  (`:547`) although it belongs to Chop. Grain size/density/spread (`:548-550`) are
  Cloud-only; Swell time/crossfade (`:556-557`) are Swell-only (and still unwired to the
  audio — plan 05.4); tape stop time/curve are release features that run in every mode.
- No `TooltipWindow` exists anywhere in the repo. The three other editors show help with a
  `HelpTracker` `MouseListener` + `helpLabel` reading `getHelpText()` (e.g.
  `SpectralDrift/PluginEditor.h:37`, `PluginEditor.cpp:55-60`, `:173-182`, `:300`;
  MTC adds a `describe()` helper that sets the same text on knob, caption and slider,
  `MicroTimingChaos/Source/PluginEditor.cpp:65-77`, `:310-314`). Sgraffito had a help line
  once (commit `32064fc`), removed in `56a5af5`; it sets no help texts today.
  `SuiteSettings` carries the `ui/tooltips` toggle (plan 08) that nothing reads yet.
- Layout: `resized()` (`PluginEditor.cpp:412-443`) takes the header, then
  `keyboard.setBounds (area.removeFromBottom (keyboardHeight))` (`:434`) with a `groupGap`
  (`:435`). A status bar goes at the very bottom, below the keyboard.

### 1.4 Presets and samples (#7, #8)- 20 Sgraffito factory presets are authored in `tools/preset_data.py:241-262`; exactly five
  carry `sample_path="embedded:cloud_4k.wav"` (Glass Dust, Deep Cloud, Cloud Rain,
  Cloud Dust, Swelling Cloud). The other 15 carry no sample reference at all.
- `applySampleFromState` (`PluginProcessor.cpp:1270-1322`) implements delta semantics: a
  preset without the `samplePath` property **leaves the current sample alone**. That is why
  finding #7 happens: after loading a user sample, re-selecting one of the 15 presets never
  switches back. INIT and host program 0 reset to `embedded:grain_4k.wav`.
- The bank is generated by `tools/build_bank.py` (JSON `:64-89`, XML `:96-116`; `samplePath`
  becomes the `<PARAMETERS ... samplePath="...">` attribute at `:106`). The audit
  (`tools/audit_bank.py:138-145`, `:250-255`, `:289-294`) rejects any `samplePath` that is
  not an `embedded:` URI. `SgraffitoPresetTests.cpp:27-57` applies every URI in the bank and
  asserts it loads; it **hard-codes `embeddedCount == 5`**.
- Embedded samples live in `plugins/Sgraffito/Resources/samples/` (grain_4k.wav,
  cloud_4k.wav), listed in `plugins/Sgraffito/CMakeLists.txt:15-16` and embedded through
  `juce_add_binary_data` (`SuitePlugin.cmake:150-160`). Resource names keep only
  alphanumerics (`.` → `_`; `loadEmbeddedSample`, `PluginProcessor.cpp:1211-1224`) and an
  unknown name **silently keeps the current sample** — the test above is the typo guard.
- `tools/create_samples.py` is a stale FLAC-era artefact (it writes FLAC, which
  `registerBasicFormats()` cannot read); the new generator (phase 7) writes WAV directly.

### 1.5 Settings dialog legibility (#9, screenshot 2026-11-17 22:51)

The owner's screenshot shows the Settings window open with its geometry intact — the
fallback-tempo slider, its text box, and the two dark Browse pills are all in the right
places (the T1 fix's layout) — but **no text is visible anywhere**: no captions, no toggle,
no button labels, no values. Only drawn geometry shows. Working hypothesis: the dialog
inherits a LookAndFeel whose text colours are light (the suite's dark-palette text on the
dialog's plain white `paint()`), so light-on-white text vanishes; the dark button fill
suggests the suite look and feel reaches the dialog (top-level windows use the process's
default look and feel — the DAW's, not the editor's component-scoped one). **Verify before
coding**: the screenshot may predate the T1 rebuild; reproduce against the current build
first (a DAW run, or a test that paints the dialog under the default look and feel), and
confirm which look and feel and text colours the dialog actually resolves.

---

## 2. Phase 0 — owner gates: decided 2026-11-18

Every gate below was put to the owner with a recommendation; the decision column is the
outcome of record. No further approval is needed to execute the phases.

| Gate | Decision (2026-11-18) |
|---|---|
| G1 (#1) | **Disabled** (dimmed, `setEnabled(false)`) — never hidden; the layout stays stable |
| G2 (#3a) | **Release always shapes the envelope tail; the tape-stop ramp multiplies on top; the voice ends when both are finished** |
| G3 (#3b) | **Static envelope preview** for the displayed (root) key's region, drawn over the waveform |
| G4 (#4/#6) | **No rename** — the caption and parameter stay "Region Span". Root Key becomes the **pitch centre** (`rate = 2^((key − rootKey)/12)`), the dead `rootKey` argument is removed from `keyRegion`, and the relation is documented (help text + README). The "Region Start" idea is rejected with reasons on record in section 6 |
| G5 (#5) | **The status bar honours the settings' `ui/tooltips` toggle** |
| G6 (#7) | **Delta semantics stay** (a preset without a sample reference leaves the sample); finding #7 is solved by phase 7 giving every factory preset its own sample |
| G7 (#8) | **20 generated samples** (one per preset), 48 kHz mono 16-bit, 1.5–3 s each (≈ 2 MB embedded total); **retire `cloud_4k.wav`**, keep `grain_4k.wav` as the INIT default |

---

## 3. Phase 1 — LOAD SOUND back to the top-right (#2)

Files: `plugins/Sgraffito/Source/PluginEditor.cpp` (one line), `PluginEditor.h` (comment).

1. In `layoutSoundPage()` (`PluginEditor.cpp:459-462`) replace
   `waveformView.getBounds()` with `waveformView.getLocalBounds()` — the button is a child
   of `waveformView`, so its bounds are waveform-local. The intended placement
   (top-right strip inside the waveform, pill height 36, width 130) then lands where the
   2026-11-16 owner request put it.
2. Update the stale comment at `PluginEditor.h:276` ("top right of header" → "top-right
   inside the waveform preview"), and the AGENTS §14 sentence only if its wording claims
   something else after the change.
3. Regression test (new `testLoadButtonSitsTopRight` in `plugins/Sgraffito/tests/PluginTests.cpp`,
   registered after `testPageVisibility`): build the editor (the `checkEditor` helper
   pattern), then assert, in `waveformView` coordinates:
   `loadSampleButton.getY() <= 8` and
   `loadSampleButton.getRight() >= waveformView.getWidth() - 8`.
   (The old code fails this: y ≈ 109.)

---

## 4. Phase 2 — mode affinity: disable what the mode cannot use (#1)

Files: `plugins/Sgraffito/Source/PluginEditor.{h,cpp}`.

Design (per G1): a control whose parameter has no effect in the selected mode is
**disabled** — dimmed by the look and feel, still visible, never hidden. Common controls
stay enabled in every mode.

1. Add a private `updateModeAffinity()` to `SgraffitoEditor`. It reads the current mode
   (the mode parameter's value through `modeAttachment`'s parameter or a cached
   `currentMode` int set by both attachment callbacks) and calls `setEnabled` on exactly
   the affinity set. Keep one static table at the top of the .cpp:

   ```cpp
   // Controls that only work in one mode (phase 2, plan 10). Everything else on the
   // TEXTURE page is common: memory, drift, lay-back/swing/humanise, ghosts, tape stop,
   // shuffle seed and the surface controls run on every mode's voices.
   struct ModeAffinity { SuiteUI::LabelledSlider* slider; int mode; };
   ```

   Members and their only-working mode:
   - `sliceCountSlider` → Chop (1) — also move it out of the CLOUD group into a sensible
     spot on the TEXTURE page (see step 4).
   - `grainSizeSlider`, `grainDensitySlider`, `grainSpreadSlider` → Cloud (2).
   - `swellTimeSlider`, `swellCrossfadeSlider` → Swell (3). Note in the help text (phase 5)
     that these two shape the swell envelope only after plan 05.4's swell phase wires them.
   - Tape stop time/curve stay enabled in all modes (they trigger on note-off in every
     mode — verified: `releaseKey` matches lanes by `inputKey`).
2. Call `updateModeAffinity()` from three places: the parameter→UI lambda of
   `modeAttachment` (`PluginEditor.cpp:193`), `modeSelector.onChange` (`:194`), and once
   after `sendInitialUpdate()` (`:195`) so the initial state is right.
3. Disabled knobs must not look broken: `SuiteUI::SuiteLookAndFeel` draws disabled labels
   and knobs through the standard JUCE `isEnabled()` paths (verify in the render, step 5;
   if the disabled knob is illegible, dim the caption via `Label`'s disabled colour rather
   than adding new drawing).
4. Regroup on the TEXTURE page: move Slice Count from the CLOUD group into the row layout
   (the TEXTURE groups are built in `layoutTexturePage()`, `PluginEditor.cpp:487-562`;
   slice count belongs visually with the tape/scratch row, next to tape stop). This is a
   `groups` entry change (`PluginEditor.h:207-212`) plus the row assignment — no parameter
   order change.
5. Tests (extend `testPageVisibility` or add `testModeAffinity` in
   `plugins/Sgraffito/tests/PluginTests.cpp`):
   - mode 0 (Scrub): `sliceCount…/grain…/swell…` sliders `! isEnabled()`.
   - mode 1: `sliceCountSlider.isEnabled()`, the grain and swell sliders not.
   - mode 2: the three grain sliders enabled, slice count and swell not.
   - mode 3: the two swell sliders enabled, the rest not.
   - The mode change must not touch any parameter value (same assertion style as the
     plan 05 page-switch test: render/compare before and after a mode change).
6. Renders (rule R9): with `SUITE_RENDER_DIR` set, write the TEXTURE page per mode
   (`texture_scrub.png`, `texture_chop.png`, `texture_cloud.png`, `texture_swell.png`)
   under `build/renders/mode-affinity/`, before and after.

---

## 5. Phase 3 — Attack and Release: make them real, then show them (#3)

Files: `modules/core_dsp/include/SamplerEngine.h`, `plugins/Sgraffito/Source/PluginProcessor.cpp`,
`plugins/Sgraffito/Source/PluginEditor.h` (waveform view), tests.

### 5.1 Attack on direct notes

1. In `handleNoteOn`'s direct path (`PluginProcessor.cpp:356-360`) pass the absolute start
   sample: `voicePool.allocateVoice (key, velocity, spanMod, lengthMod, mode,
   sliceCount, (double) streamSample + (double) samplePosition)`. The ghost/pending paths
   already set `envStart` after allocation — leave them.
2. `Voice::envelope` needs no change: with a correct `envStart`, the attack ramp runs.
3. Nothing else reads `envStart == 0` for meaning (the Cloud lane trigger uses
   `nextGrainSample`, which `allocateVoice` also seeds from `startSample` — passing the
   absolute start keeps "fire immediately" behaviour).

### 5.2 Release under tape stop (per G2)

1. In `releaseVoice` (`SamplerEngine.h:245-271`) set the envelope end **always**, keeping
   the "don't overwrite an existing release" guard:

   ```cpp
   if (voice.envEnd < 0.0)
   {
       voice.releaseLevel = voice.envValue;
       voice.envEnd = sampleIndex;
   }
   if (tapeStopTimeMs > 0.0)
   { /* tape-stop start, as today */ }
   ```

2. In the processor's per-sample deactivation (the `envValue <= 0.0f && envEnd >= 0.0`
   branch from the T2 fix) require **both** to be finished:

   ```cpp
   const bool rampBusy = voice.tapeStopping && voice.tapeStop.active();
   if (envValue <= 0.0f && voice.envEnd >= 0.0 && ! rampBusy)
       voice.active = false;
   ```

   Net tail shape: the envelope falls over Release ms while the tape ramp multiplies its
   own curve; the voice stops when both are done. The Release knob is audible at the
   defaults; Tape Stop Time keeps its character.
3. Check the interplay with `testTapeStopReleaseFreesVoice` (T2): with the default 400 ms
   ramp and 200 ms release the voice still frees inside 0.64 s — the test stays as is.

### 5.3 Envelope display in the waveform (per G3)

1. In `SgraffitoWaveformView` (`PluginEditor.h:26-188`) add a static envelope overlay drawn
   **inside the region overlay** after the min/max bars: a filled accent path at low alpha
   (≈ 0.20) tracing attack → sustain → release over the region:
   - x axis: region start → region end (the same `computeRegion()` used for the overlay).
   - The preview key is the root key, so the preview rate is
     `2^((rootKey − 60)/12)` today, `2^0 = 1` after G4c makes Root Key the pitch centre —
     convert milliseconds to region fractions with
     `ms × sampleRate / 1000 / (regionLengthFrames / rate)`, clamped to [0, 1].
   - Shape: linear rise over `attack`, flat sustain, linear fall over `release` from the
     sustain level. (The envelope is `Voice::envelope`'s shape: linear attack, clamp, linear
     release from `releaseLevel`.)
2. The view needs attack and release values: read them from the processor's APVTS through
   two new public getters or pass the two `std::atomic<float>*` in (the editor already
   holds `processor` — simplest is reading
   `processor.apvts.getRawParameterValue (attackId)->load()` at paint time, message thread
   only, same as the existing region params).
3. Keep the overlay behind the Chop lines and the region caption; nothing is interactive.

### 5.4 Tests (all fail before, pass after)

1. `testAttackShapesDirectNotes`: render a direct note-on (no lay-back/swing/humanise) with
   attack 1 ms and attack 400 ms (separate processors, same 1 s sine sample, region 100 %);
   assert the mean absolute output over the first 4800 samples (100 ms) is clearly lower
   with the long attack (e.g. `longRms < 0.5 × shortRms`).
2. `testReleaseShapesTapeStopTail`: default Tape Stop Time 400 ms; note-on, note-off after
   10 blocks; render until silent; measure the tail length (samples above 0.01 after the
   note-off). Assert the Release 2000 ms tail is at least 1.5× the Release 200 ms tail
   (before the fix both are identical — the ramp ignores Release).
3. `testEnvelopeOverlayPaints`: editor render with `SUITE_RENDER_DIR` — the SOUND page
   renders with the overlay; `countChangedPixels` between the overlay on/off states (or
   simply assert the render differs from the phase-1 baseline only inside the region band).
   Store before/after renders under `build/renders/envelope/`.

---

## 6. Phase 4 — Root Key semantics and the Region Span explanation (#4, #6)

Files: `modules/core_dsp/include/SamplerMaps.h`, `SamplerEngine.h` (mirror only),
`plugins/Sgraffito/Source/PluginProcessor.{h,cpp}`, `PluginEditor.{h,cpp}`, docs.

Decision of record (G4): **no rename**. The caption and the parameter name stay
"Region Span"; the confusion is removed by making Root Key do something honest, deleting
the dead code, and documenting the relation.

### 6.1 The explanation (becomes help text and README text)

The relation the owner asked about, stated truthfully for today's code:

- **Region Span** scales how far across the sample the keyboard's regions reach: region
  start = `(key / 127) × span`. At 100 %, key 127's region ends at the sample's right edge;
  at 37 % (the screenshot), the whole keyboard lives in the left 37 % of the sample. It is
  a spread, not a start — every key has its own start.
- **Region Length** is each key's region width (25 % default = a quarter of the sample).
- **Root Key** today does not move the region and does not transpose (pitch is against a
  fixed middle C). It is the Scale Lock centre and the key the waveform preview shows.

After 6.3, the sentence gains: "Root Key transposes: this note plays the sample at its
natural pitch; notes above it play faster, notes below slower."

### 6.2 The name stays (decision of record)

The opinion against "Region Start" is on record: the value does not set a start, so the
rename would trade one confusion for another. What remains in this phase instead:

1. No editor caption change (`PluginEditor.h:236` keeps `"REGION SPAN"`).
2. No parameter name change (`PluginProcessor.cpp:462` keeps `"Region Span"`); the
   parameter ID never changes either way.
3. The explanation of 6.1 lands as help text (phase 5's table already uses the final
   caption) and in the README + AGENTS §14 (phase 8).

### 6.3 Root Key as the pitch centre (the fix)

1. `setupAllocatedVoice` (`PluginProcessor.cpp:274-277`):
   `semitones = (double) (key − rootKey) + pitchFine / 100.0`, where `rootKey` is the
   modulated Root Key value (rounded). With the default 60 the pitch is unchanged, so
   presets and sessions sound the same unless they set Root Key ≠ 60.
2. Audit check: `tools/audit_bank.py` already validates ranges; note in the plan's report
   if any Sgraffito factory preset sets `rootKey` ≠ 60 — those presets change pitch after
   this change (list them for the owner in the phase report before regenerating the bank).
3. Tests:
   - `testRootKeyTransposes`: key 60 with Root Key 72 plays an octave lower than with
     Root Key 60 (zero-crossing count halves, the `testKeyChangesPitch` pattern).
   - Existing `testKeyChangesPitch` stays valid (Root Key default 60 in that render).
   - Region test unchanged: `testRegionMapping` still asserts key-based starts.

### 6.4 Dead code removal (the cleanup)

1. Drop the unused `rootKey` parameter from `keyRegion`'s signature
   (`SamplerMaps.h:34`): `keyRegion (int key, double span, double length)`.
2. Update the caller `makeRegion` (`SamplerEngine.h:313`) to
   `keyRegion (key, spanPercent / 100.0, lengthPercent / 100.0)` (the hardcoded-60
   argument disappears with the parameter).
3. Update the editor mirror's comment (`PluginEditor.h:153-160`) if it names the root
   argument; the mirror itself does not call the engine.
4. Grep for other callers before editing (`DSPTests.cpp` may exercise `keyRegion`)
   and fix them. CoreDSP is header-only, so no link impact.

---

## 7. Phase 5 — status bar below the keyboard (#5)

Files: `modules/suite_ui/` (new shared helper), `plugins/Sgraffito/Source/PluginEditor.{h,cpp}`.

Design (per G5): a one-line status bar at the very bottom (below the keyboard) showing the
help text of the control under the pointer, driven by the `HelpTracker` pattern the other
three editors already use — extracted into `suite_ui` so this is the fourth and last copy.

1. New shared helper `SuiteUI::HelpTracker` in `modules/suite_ui/include/SuiteHelp.h`
   (+ `Source/SuiteHelp.cpp` only if a .cpp is needed; the HelpTracker copies are small
   enough for a header-only struct): a `juce::MouseListener` that on mouse-move (and
   mouse-enter) writes `component->getHelpText()` into a target `juce::Label`, with an
   idle text for "no help under the pointer". Wire it with
   `addMouseListener (&helpTracker, true)` on the editor, exactly as
   `SpectralDrift/PluginEditor.cpp:55-60` does. The other three editors keep their local
   copies for now (a one-line note in the plan record; migrating them is a separate pass).
2. Sgraffito editor layout (`resized()`, `PluginEditor.cpp:412-443`): after the page
   layout, take the bottom in this order —
   `statusLabel.setBounds (area.removeFromBottom (statusHeight))` **first** (the very
   bottom), then the existing keyboard removal above it. `statusHeight = 22`; the keyboard
   keeps `keyboardHeight = 48`. No window-size change (the page content above shrinks by
   30 px total; verify the TEXTURE page's five rows still fit at 880×700 — the R4 budget
   allows this because the keyboard strip currently touches the window edge).
3. The label: 11 pt `textDim`, left-aligned with the outer margin, `Justification::centredLeft`.
4. Write the help texts: add a private `describe (juce::Component&, const juce::String&)`
   helper (the MTC pattern, `MicroTimingChaos/Source/PluginEditor.cpp:65-77`) that sets the
   same text on the knob, its caption label and the value box, then call it for every
   control. The texts (final captions after phase 4):

   | Control | Help text |
   |---|---|
   | Mode | "Playback mode: Scrub plays the region forward, Chop slices it, Cloud granulates it, Swell reads it backwards." |
   | Region Span | "How far across the sample the keyboard's regions reach. Region start = key position × this value." |
   | Region Length | "Width of each key's region, as a share of the sample." |
   | Root Key | "Pitch centre and Scale Lock centre: this note plays the sample at its natural pitch. Does not move the region." |
   | Pitch Fine | "Fine tuning of every note, in cents." |
   | Scale Lock / Scale | "Quantise incoming notes to the selected scale, centred on Root Key." |
   | Glide | "Reserved for glide between successive notes (not wired yet)." — only if it stays unwired; else the real behaviour |
   | Level | "Output level of the whole plugin." |
   | Velocity Curve | "How note velocity maps to loudness: 1.0 is linear, higher emphasises soft notes." |
   | Attack | "Rise time of every note's volume envelope." |
   | Release | "Fall time of the volume envelope after a note-off. Tape Stop (TEXTURE page) fades on top of this." |
   | Tape Stop Time / Curve | "On note-off, the level falls to zero over this time, shaped by the curve. Runs in every mode." |
   | Memory Amount | "How far each note fades toward this floor over the Memory Decay time. 0 % keeps notes at full level." |
   | Memory Decay | "Time constant of the memory fade, in seconds." |
   | Drift Amount / Speed | "Reserved for the Lorenz chaos drift (not wired yet)." |
   | Lay Back | "Delays every note by this many milliseconds." |
   | Swing | "Repeated notes of one key alternate straight and swung; the swung copy is delayed up to half a beat." |
   | Humanise | "Randomised per-note timing offset up to this many milliseconds." |
   | Ghost On / Threshold / Level / Delay | "Ghost notes: a quiet delayed copy fires when a note plays below the threshold." |
   | Slice Count | "Chop mode: number of equal slices across the sample." |
   | Grain Size / Density / Spread | "Cloud mode: grain length, grains per second, and how far apart the four grain lanes start." |
   | Sustain Hold | "Note-offs latch; the next note of the same key releases." |
   | Swell Time / Crossfade | "Swell mode: the reverse swell envelope and its wrap crossfade (shaping wired by plan 05.4)." |
   | Dust / Crackle / Wow / Flutter | "Vinyl surface: read jitter, random level steps, slow pitch wobble, fast level wobble." |
   | Shuffle Seed | "Reshuffles the humanise pattern." |
   | LOAD SOUND | "Load a WAV or AIFF sample from disk." |

   Unwired controls say so in their text (honesty rule; update them when plan 05.4 lands).
5. The status bar honours the settings toggle (G5): read
   `SuiteSettings::shared().getBool (SuiteSettings::tooltipsKey, true)` in the editor
   constructor (message thread); when off, the HelpTracker writes only the idle text.
   The `suite_settings` module is already compiled into Sgraffito.
6. Tests: `testStatusBarShowsHelp` — construct the editor, pump the message loop
   (`checkEditor` pattern), assert the label text is the idle string; synthesise a mouse
   move over a knob (the 15-argument `MouseEvent`, as `SuiteUITests` does) and assert the
   label shows that knob's text; assert the label sits below the keyboard
   (`statusLabel.getY() > keyboard.getY()`).

---

## 7b. Phase 5b — Settings dialog legibility (#9, owner finding 2026-11-18)

Files: `modules/suite_settings/Source/SuiteSettings.cpp` (and the header if colours need
members). The T1 fix made the dialog crash-free and correctly laid out, but the owner's
screenshot (section 1.5) shows it launched in the DAW with no readable text.

Design: make the dialog self-sufficient in colour — never rely on whichever look and feel
a host hands a top-level window. Two moves, ordered:

1. **Reproduce and diagnose first** (house rule: check the claim against the current
   build): run the current plugin in the DAW, open Settings, confirm the screenshot's
   symptom on the T1 build. Then identify the colour path — most likely the dialog's
   plain-white `paint()` under a look and feel whose label text colour is light. A quick
   probe: log `LookAndFeel::getDefaultLookAndFeel().getClass()`-level info (or simply set
   explicit colours and see the symptom vanish).
2. **Give the dialog the suite look, explicitly** (matches the About dialog's styling and
   makes it host-independent):
   - A `SuiteUI::SuiteLookAndFeel lookAndFeel { accent }` member on `SettingsDialog`
     (component-scoped `setLookAndFeel` in the constructor, `setLookAndFeel (nullptr)` in
     the destructor — declared **after** the components it serves so destruction order is
     safe, per the editor's pattern).
   - `paint()` fills `SuiteUI::Colours::panel` and draws the border in `track`, matching
     the suite's dark dialog look.
   - Every text element gets explicit colours: captions and value texts
     `setColour (juce::Label::textColourId, SuiteUI::Colours::text)`, the disabled/idle
     folder text in `textDim`; the toggle, slider and Browse buttons inherit the suite
     look and feel's drawing (same as the editors, so they read correctly on the dark
     panel).
   - The window background: pass `DialogWindow::LaunchOptions::dialogBackgroundColour`
     (`SuiteUI::Colours::panel`) in `showSettingsDialog` so the title bar area matches.
3. **Regression test** (`SuiteSettingsTests`, test 10): paint the dialog into an image and
   assert readable contrast — e.g. render twice, once with the caption text colour forced
   and once without... simplest deterministic check: count distinct sampled pixel colours
   in the caption row is > 1 (text pixels exist against the panel background), and a
   second render after `setText` changes differs (text is actually painted, not clipped).
   Store a render (`settings-dialog.png`) when `SUITE_RENDER_DIR` is set, and record the
   before/after pair under `build/renders/settings-dialog/` (the symptom is host-L&F
   dependent, so the test's value is the explicit-colour guarantee, not the DAW repro).
4. Note for the record: the owner's screenshot may predate the T1 rebuild — if the
   current build already shows readable text in the DAW, close this phase with the
   explicit-colour hardening alone (it is still correct: no dialog should depend on the
   host's look and feel).

---

## 10. Phase 9 — DS MOD page: curve preview above the modulation grid (#10, owner report 2026-11-18)

Files: `plugins/DynamicSaturation/Source/PluginEditor.cpp`,
`modules/suite_modulation/{include/SuiteModulationPanel.h,Source/SuiteModulationPanel.cpp}`.

The DS MOD page gave `transferCurve` the whole page area and then laid the modulation
group over the same rectangle, so the curve was drawn behind the grid (the before render
shows only its diagonal peeking out under the routing rows). The fix mirrors
SpectralDrift's MOD page split: the curve gets the strip above, the group gets its
natural height below (anchored just above the help line).

- `ModulationPanel::getPreferredHeight()` exposes the panel's natural height (LFO block +
  gap + heading + six slot rows = 318 px at the current constants), so editors stop
  hardcoding module-internal numbers when they stack content around the group.
- `layoutModPage` takes the group (title band + 4 + preferred + 12) from the bottom, the
  curve fills what remains above, and the meters keep their right-hand gutter.
- Tests: `testPanelPreferredHeightFitsItsRows` (SuiteModulationTests, test 10) sizes the
  panel to its preferred height and asserts every child stays inside it. Renders:
  `build/renders/mod-page/ds-mod-before.png` / `ds-mod-after.png` (rule R9).

---

## 8. Phase 6 — the sample switch-back rule (#7)

Files: docs + the preset bar's tooltip; no code change under G6.

1. The rule of record (plan 07 Q4, unchanged): a preset that carries a `samplePath`
   switches the sample; a preset without one leaves the current sample; INIT resets to the
   embedded default. Finding #7 exists because 15 of the 20 factory presets carry no
   reference — phase 7 fixes that by giving every preset its own sample, after which
   "select preset → its sample loads" always holds.
2. Write the rule where the owner will meet it:
   - The preset bar's context help text (one string, via the phase-5 help mechanism):
     "Applying a preset loads the sample it was made with. Presets without a sample keep
     the current one; INIT restores the factory sample."
   - README (Sgraffito section) and AGENTS §14: same sentence.
3. Verify the edge cases in a test (`testPresetSampleSwitchBack`, extending
   `SgraffitoTests`): preset A (embedded URI) → load a user WAV → apply preset A again →
   `loadedSample->sourcePath` is the URI again; apply preset B (its own URI) → sourcePath
   is B's; apply INIT → `embedded:grain_4k.wav`. This passes once phase 7 lands and is
   written now against the five cloud presets (A = Glass Dust, B = Deep Cloud) so the
   *behaviour* is pinned before the bank regenerates.

---

## 9. Phase 7 — one generated sample per preset, presets overhauled (#8)

Files: new `tools/sgraffito_samples.py`; `plugins/Sgraffito/Resources/samples/`;
`plugins/Sgraffito/CMakeLists.txt`; `tools/preset_data.py`; `tools/build_bank.py`
(re-run only); `tools/audit_bank.py` (rule stays); `SgraffitoPresetTests.cpp` (count).

### 9.1 The generator

1. New `tools/sgraffito_samples.py` (precedents: `plugins/MicroTimingChaos/generate_texture.py`
   for plugin-local generators; `tools/create_samples.py` for the numpy/wave pattern — but
   write **WAV directly**, no ffmpeg, because `registerBasicFormats()` reads no FLAC).
   Deterministic: every sample seeded from the preset name hash, so regeneration is
   reproducible and diffs are reviewable.
2. Output: one 48 kHz, mono, 16-bit WAV per Sgraffito factory preset into
   `plugins/Sgraffito/Resources/samples/presets/`, named `<preset-slug>_4k.wav`
   (slug = lowercase, hyphens → nothing; the BinaryData name rule then gives e.g.
   `glass_dust_4k_wav`). Length 1.5–3 s (G7 size budget ≈ 2 MB total embedded).
3. Each sample is designed to showcase its preset (the owner reviews direction list in
   G7; the table below is the proposal, one row per preset — the preset list is
   `preset_data.py:241-262`):

   | Preset | Sample direction |
   |---|---|
   | Glass Dust | bright inharmonic bell cluster, long decay |
   | Deep Cloud | dark sustained drone with slow beating |
   | Memory Ghosts | decaying piano-like tones with gaps |
   | Tape Break | saturated drum-ish transient loop |
   | Cloud Rain | sparse droplet plinks over a soft bed |
   | Scrubbed Memory | noisy vinyl phrase with a modulation wobble |
   | Chopped Sticks | dry percussive wood hits, even spacing |
   | Swell Pad | slow-attack evolving pad (long attack baked in) |
   | Cloud Dust | airy noise bed with tonal shimmer |
   | Wobble Memory | pitch-wobbled tone (wow character) |
   | Ghost Delay | short motif with echo-like repeats |
   | Swelling Cloud | crescendo texture (reverse-friendly) |
   | Choppy Scratch | rhythmic chopped phrase, strong transients |
   | Drifted Memory | detuned two-oscillator drift tone |
   | Init Cloud | neutral reference tone (the utility preset) |
   | Velvet Swing | warm rounded groove tones |
   | Ghostly Echo | sparse hits with long tail |
   | Held Breath | breathy filtered noise with slow envelope |
   | Fine Grained Drift | fine granular shimmer texture |
   | Slow Attack Tape | tape-saturated slow-rise tone |

4. The script prints the file list and total size; the phase report records both.

### 9.2 Embedding and bank

1. `plugins/Sgraffito/CMakeLists.txt`: replace `cloud_4k.wav` in ASSETS with the 20
   per-preset files (keep `grain_4k.wav` — it is `embeddedSampleUri`, the INIT default,
   `PluginProcessor.h:232`; **that constant never changes**). Delete
   `Resources/samples/cloud_4k.wav` after the presets stop referencing it (G7).
2. `tools/preset_data.py`: every Sgraffito `_preset(...)` gains
   `sample_path="embedded:<slug>_4k.wav"`. The audit rule (embedded-only) stays as is.
3. `tools/build_bank.py`: no code change; re-run to regenerate the bank XML + JSON.
   `STATE_VERSIONS` unchanged (Sgraffito is version 1).
4. `SgraffitoPresetTests.cpp:27-57`: the `embeddedCount == 5` assertion becomes
   `== 20`, and the comment line updates ("the cloud presets actually shipped" →
   "every preset ships its own sample"). The existing per-URI load assertions then cover
   all 20 samples — this is the typo guard that makes an unknown resource name impossible
   to ship.
5. Preset overhaul (the "showcase" half of #8): revisit each of the 20 presets' parameter
   deltas so the preset demonstrates the controls that are **live today** (T2 made memory,
   surface, tape-stop release real; plan 05.4's ghost/sustain/swell shaping is still
   pending, so presets must not lean on those). Per preset:
   - Re-check the delta against the parameter table (AGENTS §4 Sgraffito) and the preset's
     sample direction above (e.g. Swell Pad: long attack, region high on the sample;
     Tape Break: tape stop time short, crackle moderate; Cloud Rain: small grain size,
     high density, spread wide).
   - Keep names, categories and bank order unchanged (append-only rule).
   - Values must stay inside the parameter ranges (the audit enforces this; run it).
   - Presets that set `rootKey` ≠ 60: after phase 4's pitch change these transpose —
     verify each in the audit report and adjust the authoring if the sound direction
     requires the old pitch.
6. Full pipeline run and checks (AGENTS §15):
   `python tools/build_bank.py` → `python tools/audit_bank.py` → clean Release build →
   full CTest (16 entries; the four `*_PresetTests` executables now carry 20 embedded
   samples each for Sgraffito).

---

## 10. Phase 8 — documentation close-out

1. AGENTS.md §14: the Sgraffito paragraph gains the mode-affinity note, the Root Key
   pitch-centre semantics (name unchanged), the status bar (with the `SuiteUI::HelpTracker`
   module note), and the 20-sample bank (the preset minimum note already says 20).
2. `CHANGELOG.md` under Unreleased: one entry per owner-visible change.
3. `docs/plans/README.md`: add the row for plan 10 (status "Started <date>; Phase 0 next"
   until the gates pass).
4. Section 15 checks in full: clean build, 0 new warnings, full CTest (report the entry
   count — it must stay 16; only test *functions* grow), renders under `build/renders/`
   for every UI phase, `git status --short` clean of surprises.

---

## 11. What could bite (from this repo's history)

- **The Region Span name stays** (decision G4): no caption or parameter-name change; only
  Root Key's pitch semantics and the dead `keyRegion` argument change. Sessions keep
  working either way — the parameter ID is untouched.
- **`keyRegion`'s signature change** touches CoreDSP (header-only): grep for callers
  (`SamplerEngine.h`, `DSPTests.cpp`, the editor mirror comment) before editing.
- **`SgraffitoPresetTests` hard-codes 5** embedded samples — phase 7 must update it or the
  bank build fails.
- **The audit rejects file paths** in `samplePath` — the generator writes files into
  `Resources/`, but the bank only ever references `embedded:` URIs. Do not "help" by
  writing absolute paths into presets.
- **Editor tests need a desktop session**; headless runs can fail for non-code reasons
  (AGENTS §14). Renders are mandatory before claiming "no visible change" (R9).
- **`build_bank.py` + audit are the single source of truth** for the bank; never edit
  `bank/` files by hand.
- The status bar's settings toggle reads the **shared store** (`SuiteSettings::shared()`),
  message thread only — do not call it from `processBlock`.
