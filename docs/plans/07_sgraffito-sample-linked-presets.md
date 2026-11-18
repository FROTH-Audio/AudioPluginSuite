# Plan 07: Sample-Linked Presets (Sgraffito)

Proposed 2026-11-15. Owner decisions recorded the same day (section 7).
**Phase 0 done 2026-11-15** — findings in section 8, with corrections to sections 2-3.
**Phase 1 done 2026-11-15** — processor changes + 7 new tests, all uncommitted. Results in section 9.
**Phase 2 done 2026-11-15** — fallback search, missing-sample overlay, 5 new tests, all uncommitted. Results in section 10.
**Phase 3 done 2026-11-15** — bank audit rule + the host program virtuals Sgraffito was missing. Results in section 11.
**Phase 4 done 2026-11-16** (owner approved: async load + embedded presets) — results in section 12.
Remaining: none.

## Goal

Presets (and host sessions) carry a reference to the sample they were made with, so
changing presets changes the sample the way every other parameter does. This resolves
the owner's report: "I see always the same sample when I change preset."

## The design, in one paragraph

Copy the `lerpCurve` precedent exactly. A preset's non-parameter extras travel as
**properties/attributes on the `<PARAMETERS>` tree**, not as APVTS parameters and
not as a preset-format bump. Sgraffito stores a `samplePath` property on
`apvts.state`; `PresetFile::save` writes it as an attribute, `PresetFile::read`
passes it through, and `applyPresetState` acts on it after `SuiteState::read`.
Presets without the attribute (all 100 factory presets today) leave the sample
untouched — delta semantics, same as parameters. No `formatVersion` bump, no
`StateHolder` interface change, no parameter-ID churn.

## 1. What to look for first (investigation checklist)

Read these before writing code; each item names the file and what to confirm.

1. **The lerpCurve precedent** — `plugins/MicroTimingChaos/Source/PluginProcessor.cpp`
   - `loadLerpCurveFromState()` (line 163) reads `apvts.state.getProperty("lerpCurve")`.
   - Called in the constructor (line 72), after `setStateInformation` and inside
     `applyPresetState` (lines 688, 702).
   - `storeLerpCurveToState()` (line 192) sets the property before `SuiteState::write`
     (line 680 area). Confirm the exact call sites and ordering.
2. **SuiteState encode/decode of non-parameter properties**
   - `modules/suite_state/` — confirm `SuiteState::write(apvts, stateVersion)` serialises
     the whole `apvts.state` XML (so a set property travels), and that `SuiteState::read`
     replaces the state wholesale. Critical consequence: any property set on the *old*
     state vanishes at read time — MTC re-applies it after; Sgraffito must too.
   - Confirm `resetMissing=true` (preset path) does not strip non-parameter properties
     unexpectedly, and that the `suiteStateVersion` check is unaffected by extra
     attributes.
3. **PresetFile** — `modules/suite_presets/Source/PresetFile.cpp`
   - Save: `root.setAttribute("formatVersion", 1)` (line 64) — attributes are written
     from the state tree; check whether it copies arbitrary attributes or only known
     ones. If only known ones, add `samplePath`.
   - Read: refuses `formatVersion > 1` (line 114). Keeping the additive route (version
     stays 1) avoids making old builds refuse new presets.
4. **applyPresetState today** — `plugins/Sgraffito/Source/PluginProcessor.cpp` line ~873.
   It encodes the tree and calls `SuiteState::read(..., resetMissing=true)`. The sample
   hook goes immediately after the read, exactly like MTC's `loadLerpCurveFromState()`.
5. **Who calls applyPresetState and from where** — `PresetBar::selectPreset`, the host
   program bridge (`PresetPrograms`), and the preset menu. All message thread. The
   sample load must therefore live in the **processor**, never the editor: host program
   changes and program-list scans run without an editor.
6. **Sample plumbing as it stands** — `loadSample` / `loadEmbeddedSample` /
   `clearSample` / `retiredSample` / `getSamplePath` / `sampleFile` (PluginProcessor,
   lines ~880–1030). The retire pattern and the atomic `sampleBuffer` swap are already
   correct; the plan builds on them, not around them.
7. **Bank tooling** — `tools/preset_data.py` (authoring), `tools/build_bank.py`
   (generation), `tools/audit_bank.py` (rules). The bank XML is generated; never edit
   `bank/AudioSuitePresets.xml` by hand.
8. **Sample folder** — user presets go to `userDocuments/AudioSuite/<plugin>/Presets`.
   The default sample folder is `userDocuments/AudioSuite/Sgraffito/Samples` (owner
   decision Q5).

## 2. Behaviour specification (owner decisions recorded 2026-11-15)

### 2.1 The attribute

- Property name: `samplePath` on `<PARAMETERS>` (XML attribute), holding either
  - an absolute path with forward slashes (`C:/Users/.../hit.wav` — JUCE `File` accepts
    them on Windows), or
  - an `embedded:` URI (`embedded:grain_4k.wav`) resolved through `BinaryData`, so a
    preset can reference the shipped sample without a machine path.
- Absent attribute = "preset does not speak about samples" → **sample untouched**
  (delta semantics, matching parameters).
- **INIT resets the sample to the embedded factory default** (`embedded:grain_4k.wav`),
  like every other parameter resets (owner decision Q4).

### 2.2 Resolution order when the path does not exist as stored

1. The absolute path, as stored.
2. Same file name inside the preset's own folder — *only if* the apply path can see it
   (see 2.4 pitfall; if not reachable, drop this step and go to 3).
3. Same file name in `userDocuments/AudioSuite/Sgraffito/Samples`.
4. `embedded:` fallback is **not** automatic — a missing sample is reported, never
   silently substituted (the user must see what happened).

### 2.3 Missing sample (owner decision Q2)

- Keep the currently loaded sample playing (never silence, never a random substitute).
- Set a `sampleMissing` flag (plain member + atomic bool for the UI) and a
  `missingSamplePath` string; the waveform view draws a "Sample missing — click to
  locate" overlay (accent border, no colour-only signal, per suite rule R6).
- Clicking the overlay opens the existing `browseForSample()` flow; loading from it
  clears the flag and updates the state property.
- The flag is **not** persisted; on a fresh session with a missing path the overlay
  reappears.

### 2.4 Saving

- `PresetFile::save` writes the current `getSamplePath()` as `samplePath` when a sample
  is loaded; writes nothing when none.
- User presets only. **Factory presets stay sample-free in phase 1** (owner decision
  Q3: no `embedded:` references in the bank yet).

### 2.5 Host session state (owner decision Q1: yes)

- `storeSampleToState()` before `SuiteState::write`; after `SuiteState::read`, load the
  stored path the same way presets do. Additive property → **no `suiteStateVersion`
  bump needed** (absent property = old behaviour), but a fixture test is still added so
  a later bump has an anchor. Fixtures are not in the repo; the test skips without
  `SUITE_FIXTURE_DIR` as the others do.

### 2.6 Threading and IO

- Loads stay on the message thread (as `LOAD SOUND` does today). The retire pattern
  (`retiredSample`) makes the audio side safe.
- Known cost: loading a long sample stalls the UI for the read + resample. Acceptable
  for phase 1; note it. An async load is a phase-4 option, not a prerequisite.

## 3. Phases

### Phase 0 — investigation
Work through section 1 and record findings here or in the report before any code
change. Plans have been wrong before; check every claim against the code.

### Phase 1 — preset attribute plumbing (core)
Files: `plugins/Sgraffito/Source/PluginProcessor.{h,cpp}`,
`modules/suite_presets/Source/PresetFile.cpp` (attribute copy on save/read),
`plugins/Sgraffito/tests/PluginTests.cpp`.
- `storeSampleToState()` / `applySampleFromState(const juce::ValueTree&)`.
- `applyPresetState`: after the `SuiteState::read`, apply the sample rule (2.1/2.3).
- INIT: reset to `embedded:grain_4k.wav` (Q4).
- PresetFile save/read: round-trip the attribute; format stays 1.
- Additive only — no interface or parameter changes.

### Phase 2 — session state + missing-sample UX
- Session property round trip (2.5).
- `sampleMissing` flag + waveform overlay + click-to-locate (2.3).
- User sample folder constant + fallback search (2.2).

### Phase 3 — bank/tooling guard rails
- `tools/audit_bank.py`: new rule — factory presets must not carry absolute paths;
  `embedded:` URIs rejected until Q3 is revisited.
- `preset_data.py` schema note: `samplePath` is a non-parameter key; the audit's
  "covers every parameter" check must not treat it as one.

### Phase 4 — done 2026-11-16 (owner picked the first two items)
- Async/background sample load (message-thread handoff at the end). **Done** — §12.
- Factory presets with `embedded:` sample variants (revisits Q3). **Done** — Q3
  revised, see §7 and §12.
- Relative-path sharing (preset file + sample in one folder) — needs a way to pass the
  preset file location into apply; today `StateHolder::applyPresetState` receives only
  the ValueTree (see pitfall P2). Would require an interface extension — **not done**
  (the plan says avoid unless sharing folders is a real use case).

## 4. Tests (each maps to a phase)

In `plugins/Sgraffito/tests/PluginTests.cpp` (and `SuitePresets` file tests where the
seam is shared):

1. **Attribute round trip** (Phase 1): save a preset with a temp WAV loaded → read it
   back → `samplePath` attribute present and equal; `formatVersion` still 1.
2. **Apply loads the sample** (Phase 1): preset with `samplePath` pointing at a temp
   WAV → `sampleBuffer` non-null, `getSamplePath()` updated, and audio runs
   (`processBlock` with a note produces output from the new file's content).
3. **Delta semantics** (Phase 1): preset **without** the attribute leaves the current
   sample and `getSamplePath()` untouched.
4. **embedded: URI** (Phase 1): attribute `embedded:grain_4k.wav` loads through
   BinaryData (same path `loadEmbeddedSample` uses).
5. **INIT resets the sample** (Phase 1): applying INIT reloads the embedded default
   (Q4).
6. **Missing file** (Phase 2): path to a non-existent file → sample unchanged,
   `sampleMissing` true, no crash; then a preset change to a valid path clears it.
7. **Rapid preset switching under audio** (Phase 2): alternate two presets (one valid,
   one missing) while `processBlock` runs from the message loop — no use-after-free
   (exercises `retiredSample` under the new trigger path).
8. **Session round trip** (Phase 2): `loadSample` → `getStateInformation` → fresh
   processor → `setStateInformation` → same sample reloaded; the newer-state refusal
   test is unchanged.
9. **Old preset compatibility** (Phase 2): a hand-written format-1 preset with no
   attribute (copy of a bank entry) loads exactly as before.
10. **Editor paint** (Phase 2): missing-sample overlay renders; preset-bar
    `onPresetLoaded` repaint path still fires (existing test extended).
11. **Host program list** (Phase 3): with `SUITE_PRESET_BANK_AS_PROGRAMS`, program
    change on sample-linked presets does not crash in a host context (extend
    `runProgramList` in the preset tests).
12. **Bank audit** (Phase 3): no absolute paths in factory presets (new audit rule,
    tested by running the audit in CI as today).
13. **Fixture anchor** (Phase 2): a session-state fixture with `samplePath` recorded
    for a future `suiteStateVersion` bump; skips without `SUITE_FIXTURE_DIR`.

Existing suites must stay green: 11 CTest entries default build, 15 with the bank on.
A changed entry count is a changed test set — report it.

## 5. Pitfalls (each one has bitten this codebase or this plan already)

- **P1 — State read replaces the tree.** `SuiteState::read` swaps `apvts.state`, so a
  property set before the read is gone afterwards. Apply the sample *after* the read,
  from the *incoming* tree (never from the old state). This is exactly how MTC survives
  with `lerpCurve`.
- **P2 — applyPresetState does not know the preset file.** Relative-path resolution
  cannot work without the preset's location; `PresetInfo.file` exists on the read side
  but is not passed through `StateHolder`. Store absolute paths (recommended) or accept
  an interface change (phase 4 only).
- **P3 — Host context.** Program changes and program-list scans happen without an
  editor and sometimes without a message loop running in tests. The load must be in the
  processor, must not throw, and a missing file must be a flag, not an error dialog.
- **P4 — `resetMissing=true`.** The preset path resets absent parameters to defaults;
  make sure the *sample* is not treated as a parameter and silently reset. The
  attribute-presence test (test 3) is the guard.
- **P5 — Audio-thread pointer lifetime.** Every new trigger for `loadSample` must keep
  the retire pattern (`retiredSample`), never a bare `loadedSample.reset()`. The
  atomic `sampleBuffer` swap order (install new before dropping old) is already right.
- **P6 — Generated bank files.** `bank/AudioSuitePresets.xml` is generated by
  `tools/build_bank.py`; edit `preset_data.py` only, then regenerate and re-audit.
- **P7 — The audit's parameter-coverage rule.** `audit_bank.py` requires the schema to
  cover every live parameter ID; a new non-parameter key must be whitelisted or the
  audit breaks the bank build.
- **P8 — XML attribute content.** Paths with spaces, non-ASCII characters and
  backslashes must survive XML escaping; use forward slashes and JUCE `File` (it
  normalises). Test one path with a space.
- **P9 — FileChooser lifetime.** Any new browse flow (click-to-locate overlay) must
  keep the `FileChooser` alive across the dialog — member chooser, like the fixed
  `browseForSample()`. A stack-local chooser aborts the dialog silently.
- **P10 — Preset format refusal.** `PresetFile::read` rejects `formatVersion > 1`
  wholesale. Keeping the additive route (version stays 1) is why old builds open new
  presets. Do not bump casually.
- **P11 — Parameter identity.** `samplePath` is a **property**, never an APVTS
  parameter: `numControls`, parameter IDs and `checkParameterIds` counts must not move.
- **P12 — GPL audio.** Any factory-embedded sample beyond `grain_4k.wav` needs
  GPL-compatible rights; licence stays GPL (owner decision 2026-11-12).
- **P13 — Encoding on edit.** Use the edit tool, never `Set-Content`; re-read files
  before editing (the owner and IDE touch them between reads).
- **P14 — Message-thread IO stalls.** Large samples freeze the UI during load; document
  it in phase 1, fix optionally in phase 4. Consider a size sanity check (e.g. refuse >
  ~1 GB with an overlay message) so a mistake does not look like a hang.

## 6. Verification and docs

- Full clean Release build + CTest: 11 entries default, 15 with the bank; 0 new
  warnings in touched files (clean-build warning count compared to the ~90 baseline).
- Renders (suite rule R9): before/after PNGs of the Sgraffito SOUND page — normal and
  missing-sample states — under `build/renders/`, compared.
- Docs: `plugins/Sgraffito/README.md`, `AGENTS.md` §4 (Sgraffito parameters/state) and
  §14 (sample loading entry extended), `CHANGELOG.md`.
- Report what was not verified: macOS/AU, real DAW behaviour (file paths inside DAW
  sandboxes), host program-change behaviour in a third-party host.

## 7. Owner decisions (recorded 2026-11-15)

- **Q1** — Session state carries the sample path too? **Yes** (phase 2).
- **Q2** — Missing sample behaviour: **keep current sample + overlay**.
- **Q3** — Factory presets reference samples? **Not yet**; the bank stays sample-free.
  **Revised 2026-11-16 (phase 4, owner decision)**: the bank may carry `embedded:` URIs
  only — five Sgraffito presets ship the embedded cloud variant — and the audit rejects
  any file path.
- **Q4** — INIT semantics: **reset to the embedded default** (`embedded:grain_4k.wav`).
- **Q5** — Sample folder: `userDocuments/AudioSuite/Sgraffito/Samples` **confirmed**.

---

## 8. Phase 0 findings (2026-11-15)

Every claim in section 1 was checked against the code. Two design corrections fall
out; both make the plan smaller than written.

### 8.1 Confirmed as written

- **SuiteState carries properties both ways.** `write(apvts, version, extra)` edits the
  copied state tree before serialisation (`SuiteState.h` 97-108); `read(...)` does
  `apvts.replaceState(tree)` wholesale and runs `afterLoad` once, on success, with the
  new state (119-157). `resetMissing` loops only over **parameters** (147-151) — it
  cannot strip properties. The version check reads only `versionProperty` (132-135), so
  extra attributes are ignored.
- **PresetFile needs no changes at all** (correction to Phase 1's file list): save does
  `state.createCopy()` — every property on the tree rides along (`PresetFile.cpp`
  74-78); read rebuilds properties from XML attributes via `ValueTree::fromXml` (136).
  `formatVersion > 1` is refused wholesale (114); the plugin state version is checked
  separately (144-150).
- **The bar's SAVE path passes `getPresetStateTree().copyState()`**
  (`PresetBar.cpp` 539-541), so a property kept in sync on `apvts.state` is saved
  automatically. This is the MTC pattern: `storeLerpCurveToState()` runs after every
  curve edit and reset (`PluginProcessor.cpp` 124, 136), keeping `apvts.state` current.
- **applyPresetState call sites, all message thread:** `PresetBar::applyFlat` (162),
  the menu (286), `selectPreset` (620-623), and `PresetPrograms::applyProgram`
  (`PresetBank.cpp` 173-186). The bank's `treeFromXmlElement` (13-28) preserves
  attributes and children. The bank XML already carries `lerpCurve` as a
  `<PARAMETERS>` attribute (bank line 585).
- **Sgraffito is at `stateVersion = 1` with no migrations** (`PluginProcessor.h` 403).
  An additive property needs no bump and no migration.
- **Sample plumbing** is as section 1.6 says: `loadSample` / `loadEmbeddedSample` /
  `clearSample` with the `retiredSample` retire pattern and the atomic `sampleBuffer`
  swap; `getSamplePath()` returns `sampleFile`.
- **Audit tooling:** `audit_bank.py` whitelists `lerpCurve` as a plugin-specific
  special key (126-132, 209, 233-234, 265-266). A `samplePath` authoring key would need
  the same treatment — but per Q3 the bank stays sample-free, so Phase 3 only adds a
  rule that factory presets must **not** carry `samplePath`.

### 8.2 Corrections to the plan

- **C1 — INIT detection rule (Q4).** INIT (`PresetBar.cpp` 179) and host program 0
  (`PresetBank.cpp` 177) both apply a hand-made **empty** `ValueTree("PARAMETERS")`.
  Every real preset (user save, bank entry) carries parameter child elements, so the
  processor can distinguish: **empty tree (no children, no properties) = reset the
  sample to `embedded:grain_4k.wav`; otherwise apply `samplePath` if present, else
  leave the sample**. Note MTC's INIT *keeps* the curve (absent property →
  `loadLerpCurveFromState` no-ops); Sgraffito's rule is deliberately different and
  lives in Sgraffito's own `applyPresetState`. No shared-module change.
- **C2 — The sample property must be kept in sync at change time.** Like MTC, Sgraffito
  writes `samplePath` onto `apvts.state` inside `loadSample` / `loadEmbeddedSample` /
  `clearSample` (and the editor's browse callback picks it up through those). Then
  `getStateInformation` adds the property via the `SuiteState::write` `extra` callback
  (belt and braces, as MTC does), `setStateInformation` and `applyPresetState` reload
  through the `afterLoad` callback. The planned "PresetFile.cpp attribute copy on
  save/read" work item is dropped — nothing to change there.
- **C3 — Dirty tracking reacts to the sample.** `PresetBar` hashes
  `getPresetStateTree()` for its dirty/SAVE indicator (`PresetBar.cpp` 115, 167, 184,
  244, 455). With `samplePath` on the state, loading a sample marks the session dirty —
  the SAVE button lights up after a sample change. That is the wanted behaviour
  (the sample is part of the preset); record it in the README so it does not read as a
  bug.

### 8.3 Phase 1 shape after the corrections

All shared modules stay untouched. The whole change lives in
`plugins/Sgraffito/Source/PluginProcessor.{h,cpp}` plus tests:

1. `applySampleFromState (const juce::ValueTree& presetTree)`:
   empty tree → `loadEmbeddedSample("embedded:grain_4k.wav")`; else read
   `samplePath` from the tree — `embedded:` URI → `loadEmbeddedSample`, else resolve
   (2.2 order) → `loadSample`; missing → keep sample, set `sampleMissing` +
   `missingSamplePath`.
2. `storeSampleToState (juce::ValueTree& state)`: writes the current path
   (`sampleFile` or `embedded:` URI).
3. `loadSample` / `loadEmbeddedSample` / `clearSample` keep `apvts.state`'s
   `samplePath` in sync (C2).
4. `applyPresetState`: pass an `afterLoad` callback (P1 warning applies — read the
   path from the **incoming tree**, not the replaced state, for the empty-tree rule).
5. `getStateInformation`: `extra` callback; `setStateInformation`: `afterLoad`.
6. Phase 1 tests: 1-5 of section 4 (attribute round trip, apply loads, delta
   semantics, embedded URI, INIT reset).

---

## 9. Phase 1 results (2026-11-15)

All done inside `plugins/Sgraffito/` as section 8.3 predicted. Nothing outside the
plugin changed.

**Processor** (`PluginProcessor.{h,cpp}`):
- `samplePathProperty` ("samplePath") and `embeddedSampleUri`
  ("embedded:grain_4k.wav") constants; `prepareToPlay` now uses the constant.
- `loadSample` / `loadEmbeddedSample` set the property from
  `loadedSample->sourcePath` and clear `sampleMissing`; `clearSample` removes the
  property (it keeps its old behaviour; no callers yet).
- `storeSampleToState()` / `applySampleFromState()` implement the C1 rule: empty
  tree → embedded reset; `embedded:` URI → `loadEmbeddedSample`; existing file →
  `loadSample`; missing → keep the current sample, set `sampleMissing` +
  `missingSamplePath`; no property → no-op.
- `applyPresetState` passes an `afterLoad` callback capturing the **incoming tree**
  (P1/C1: an empty INIT tree gains a version property during read, so the empty
  check must run on the caller's tree); `getStateInformation` uses the
  `SuiteState::write` `extra` callback; `setStateInformation` uses `afterLoad` on
  `apvts.state`.
- Members: `std::atomic<bool> sampleMissing`, `juce::String missingSamplePath`.
  No parameter, no `numControls` change, no state-version bump.

**Tests** (`PluginTests.cpp`), all passing: `testPresetSampleLinkRoundTrip`,
`testPresetSampleLinkLoads` (apply + audible output), `testPresetWithoutSampleLeavesSample`,
`testPresetEmbeddedSampleUri`, `testInitResetsSample`,
`testPresetMissingSampleKeepsCurrent`, `testSessionRestoresSample`. The plan's test 4
item list came out as 7 functions because the session round trip (plan test 8) was
cheap to add in the same pass. Tests write WAVs into
`%TEMP%/SgraffitoPlan07/` with the JUCE 9 `AudioFormatWriterOptions` writer API.

**Verification:** clean Release build 0 errors, 90 warnings (exactly the
pre-existing baseline, nothing new); full CTest 15/15 with the bank on, entry count
unchanged — `Sgraffito_PresetTests` (program 0 = INIT → embedded reload) green.
Not verified: macOS/AU, a real DAW, editor renders (Phase 2 owns the overlay and
its renders). JUCE 9 notes hit while writing the tests: `File::createDirectory`
(not `createDirectoryIfMissing`), `ValueTree::appendChild` takes 2 arguments, and
`AudioFormat::createWriterFor` takes `AudioFormatWriterOptions` (the old
seven-argument form is deprecated).

---

## 10. Phase 2 results (2026-11-15)

**Fallback search (2.2).** `SgraffitoProcessor::userSampleFolder()` static returns
`userDocuments/AudioSuite/Sgraffito/Samples` (Q5). `applySampleFromState` now tries the
stored path, then the same **file name** inside the search folder; loading the fallback
updates the saved `samplePath`, so the session remembers the resolution. No automatic
`embedded:` substitution (2.2 step 4). Tests cannot write into the user's Documents
folder, so the processor has a public `sampleSearchFolder` member (empty = the real
folder) that the fallback test sets to a temp folder.

**Missing-sample overlay (2.3).** `SgraffitoWaveformView` draws it on top of the still
playing waveform: dim scrim, 2 px accent border, "Sample missing - click to locate"
(13 pt bold) and "<file name> - not found" (10 pt dim) — text plus border, not a
colour-only signal (R6). A click runs `onLocateClicked`, wired to the editor's
`browseForSample()` (the FileChooser stays alive as an editor member, P9). The cursor
becomes a pointing hand over the flagged view. The overlay repaints through the
editor's existing 60 Hz timer.

**Tests** (`PluginTests.cpp`), 5 new, all passing:
`testSampleFolderFallback` (fallback load + remembered path + stored-path priority +
no name match → missing report), `testRapidPresetSwitchingUnderAudio` (24 alternating
valid/missing applies with audio between each; exercises `retiredSample`), 
`testOldPresetCompatibility` (hand-written format-1 `.ausp` without the attribute),
`testMissingSampleOverlayPaints` (pixel diff normal vs flagged = 50776 sampled pixels),
and `testLoadsStateFixture`. `SgraffitoTests` also gained a fixture-recording mode:
`SgraffitoTests.exe <dir>` writes `<dir>/sgraffito_v1.xml` (state as
`getStateInformation` writes it) + `sgraffito_v1.values.txt` (one
"parameterID normalisedValue" line per parameter, the `StateFixtures` format), so the
samplePath anchor for a future `suiteStateVersion` bump exists. One was recorded into
`build/fixtures/plan07/` and the fixture test was run against it: 71 parameters checked,
the recorded path applied (it still exists on this machine, so the loaded branch held).
The test skips without `SUITE_FIXTURE_DIR`, as the other plugins' fixtures do.

**Renders (R9):** `build/renders/plan07-phase2/` holds `sgraffito-sound.png`,
`sgraffito-texture.png`, `sgraffito-mod.png` (normal states, deterministic — each is
rendered twice inside the test and compared) and the new `sgraffito-missing.png`
(overlay visible over the still playing waveform). The normal-state renders differ
byte-wise from the 2026-11-12 `ui-rework2/after` baseline for pre-existing, explainable
reasons (the preset bar and LOAD SOUND button in the header, the `C3 (60)` root-key
formatting, the Minor default scale, the removed help line — all committed work after
that baseline was taken, not Phase 2). Phase 2 adds no visible change while the flag is
off: the overlay drawing is gated on `isSampleMissing()`, and the only other editor
change is the click/cursor wiring.

**Verification:** clean Release build 0 errors, 90 warnings (the pre-existing
baseline, nothing new); full CTest 15/15 with the bank on, entry count unchanged.
Not verified: macOS/AU, a real DAW (path handling inside DAW sandboxes), host
program-change behaviour in a third-party host (Phase 3 owns `runProgramList`).

## 11. Phase 3 results (2026-11-15)

**Audit rule (test 12).** `tools/audit_bank.py` now rejects `samplePath` in three
places: the authoring data (`preset_data.py` deltas — a key on the preset or inside
`params`), the generated bank XML (the `<PARAMETERS>` attribute) and the per-preset
JSON (`_samplePath`). Rule line added to the module docstring; the failure message
cites plan 07 Q3. A dry-run with an injected `samplePath` made the audit fail with
"samplePath is not allowed in factory presets (plan 07 Q3: the bank stays
sample-free)"; the real bank passes (exit 0). `preset_data.py`'s docstring notes
that `samplePath` must never become a SCHEMA key — the schema-coverage check would
flag it as having no live parameter. CI does not run the audit ("as today", test 12);
no workflow change.

**Host program list (test 11) — a real gap found and fixed.** Sgraffito was the one
plugin without the `SUITE_PRESET_BANK_AS_PROGRAMS` program virtuals: the header still
had the inline `getNumPrograms() { return 1; }` stubs, and the flag build's shared
`runProgramList` asserted at `PresetTestUtils.h:347`. Fixed by mirroring
SpectralDrift exactly: out-of-line `getNumPrograms`/`getCurrentProgram`/
`setCurrentProgram`/`getProgramName`/`getCurrentProgramStateInformation`/
`setCurrentProgramStateInformation` in `PluginProcessor.cpp` under
`#if SUITE_PRESET_BANK_AS_PROGRAMS` (with a guarded `#include "PresetBank.h"`),
`changeProgramName` stays empty inline, a `currentProgramIndex` atomic member reset
to −1 in `applyPresetState` (on `LoadResult::loaded`) and in
`setCurrentProgramStateInformation`. The plugin tests also pass under the flag.

**SgraffitoPresetTests program-list extension.** Under the flag, after
`runProgramList`: `prepareToPlay`, then a note-on `processBlock` per program change
across all programs (all finite), then back to INIT asserting the embedded URI is
re-applied (`hasSample`, `samplePathProperty == embeddedSampleUri`,
`!isSampleMissing`). The extension lives in the per-plugin test file, not in the
shared `PresetTestUtils.h`, because Sgraffito is the only sample-linked plugin.

**Verification.** Flag build (`BANK=ON`, `PROGRAMS=ON`): `SgraffitoPresetTests` green
including `programList`; `SgraffitoTests` green. Owner's configuration restored
(`BANK=ON`, `PROGRAMS=OFF`): clean Release build 0 errors / 90 warnings (baseline),
full CTest 15/15, `python tools/audit_bank.py` passes (exit 0). Bank+programs still
shows 15 CTest entries; the program virtuals are exercised inside the preset tests.
Not verified: macOS/AU, a real DAW, third-party host program changes (the shared
`runProgramList` is the closest stand-in).

## 12. Phase 4 results (2026-11-16)

Owner picked two of the three optional items: the async sample load and the
`embedded:` factory presets. Relative-path sharing stays undone (it would need the
`StateHolder` interface extension the plan says to avoid).

**Async file loads (plan item 1).** `loadSample`/`loadEmbeddedSample` stay synchronous
(embedded loads are small and in memory; tests stay deterministic). New
`loadSampleAsync` runs the read + resample of a **file** sample on a `ThreadPool`
member and installs through a message-thread handoff. The safety net:

- A `LoadLifetime` token (shared_ptr with an atomic `alive` flag) is set dead first
  thing in the destructor, so a queued handoff lambda does nothing after destruction.
  The token copy lives in the lambda, so the check stays valid.
- A `loadGeneration` counter makes the **latest request win**: every sample request
  bumps it (`applySampleFromState` entry, `loadSampleAsync`, sync `loadSample`,
  `clearSample`), and an install that arrives out of order is discarded.
- `pendingSampleLoads` counts requests; the public `hasPendingSampleLoad()` clears
  when every handoff has been processed. Tests pump the message loop until it does.
- The job (`SampleLoadJob`, nested, defined in the cpp) touches no processor member on
  the worker thread: it captures the request by value. `readAndResample` is now the
  shared reader for the sync loaders, the job and (via it) the async path.
- `finishSampleLoad` is the one install path: retire pattern, atomic swap,
  `sampleFile`, missing-flag clear and the `samplePath` property sync. Message thread
  only.
- `sampleLoadPool` is declared last in the header, so it is destroyed first and its
  destructor waits; the processor destructor also calls `removeAllJobs (true, 5000)`.

Behaviour change: applying a preset or session whose `samplePath` is a **file** no
longer loads synchronously — the current sample keeps playing until the handoff
installs (the existence/fallback/missing resolution is still synchronous, so a missing
sample is reported immediately as before). Embedded URIs and INIT stay synchronous.

**Embedded factory presets (plan item 2, Q3 revised).** Five Sgraffito factory
presets now carry `samplePath="embedded:cloud_4k.wav"` (the second embedded asset,
present since the plugin was built): Glass Dust, Deep Cloud, Cloud Rain, Cloud Dust,
Swelling Cloud. `_preset` gained `sample_path=`; `build_bank.py` writes the
`<PARAMETERS>` attribute and the `_samplePath` JSON key; the attribute rides into
`PresetInfo.state` through the existing `ValueTree::fromXml` path — no processor or
seam change. The audit rule is revised: `embedded:` allowed, any file path rejected
(authoring, XML and JSON). `SgraffitoPresetTests` gained `embeddedSamples`: every
`embedded:` URI in the bank must actually load (hasSample, sourcePath matches, not
missing) — this is the typo guard, because `loadEmbeddedSample` silently keeps the
current sample on an unknown resource name.

**New tests** (`SgraffitoTests`): `waitForSampleLoads` pump helper;
`testAsyncLoadLatestWins` (two raced applies, the second wins);
`testAsyncLoadSurvivesDestruction` (destroy with a load pending, drain the handoff).
The phase 1-2 preset tests that assert a loaded file sample now pump first;
`testPresetSampleLinkLoads` additionally asserts `hasPendingSampleLoad()` right after
the apply, which pins the async behaviour. 31 test functions total (was 29).

**Verification.** Flag build (`PROGRAMS=ON`): `SgraffitoPresetTests` green including
`programList` with the embedded presets under audio. Owner's configuration restored
(`PROGRAMS=OFF`). `python tools/audit_bank.py` passes; a file-path probe fails it with
"samplePath must be an embedded: URI, never a file path". Clean build + full CTest
results in the final report. Not verified: macOS/AU, a real DAW.
