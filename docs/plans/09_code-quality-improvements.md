# Plan 09: Code Quality Improvements

**Status:** Implemented 2026-11-16 (all items). Verification: clean Release build 0 errors, 0 new warnings (one unused-variable warning from the 1.3 rewrite was found and fixed); CTest 16/16 bank build, 16/16 bank+`SUITE_PRESET_BANK_AS_PROGRAMS` (the extended `runProgramList` selection asserts ran), 12/12 default build — entry counts unchanged. Renders for 1.4 under `build/renders/preset-bar-reopen/after/` (11 views, all four editors, each rendered twice in-process byte-identically). Every claim in the original draft was checked against the code before this revision; the incorrect or harmful items are dropped and recorded in Appendix B with reasons. **D1 decided 2026-11-16: pending notes steal. D3 decided 2026-11-16: preset identity persists in saved sessions** (no migration — plugins unreleased). **D2 decided 2026-11-16: the guard-constant naming folds into 1.3 as a rider; the note-ring drop counter is dropped** (Appendix B) — there is no Phase 2.
**Priority:** Critical → High → Medium → Low (per item below)
**Target:** Sgraffito (1.1–1.3); the shared preset system plus all four plugins (1.4)
**Dependencies:** None. Plan 07's async loader is the code being hardened; plan 05.4 owns the surface-effect controls this plan deliberately does not touch.
**Estimated Effort:** 12–18 hours (1.4 added ~4 h; the former Phase 2 folded away per D2, its one surviving rider rides with 1.3)

---

## Executive Summary

The original draft filed nine issues. Verification against the code (Appendix A) found:

- **One genuine memory-safety window** — the single-slot sample retirement can destroy a buffer a running audio block is still reading. The original draft's proposed fix (a safety flag) would not have fixed it; this plan replaces it with multi-generation retirement.
- **One valuable consolidation** — three note-on paths allocate voices two different ways, and the hand-rolled path already shipped one bug and still diverges in two ways. This is the largest item.
- **One hardening one-liner** — the async-load generation stamp uses two atomic operations where one suffices.
- **One reported DAW bug, investigated and confirmed (2026-11-16)** — the preset bar shows "Init" after the plugin window is closed and reopened, because the selection lives in the editor-owned bar instead of the processor. Per D3, the identity also persists in saved sessions as a state property.
- **One hygiene rider** — the Cloud-grain guard constants are named inside 1.3 (D2); the note-ring drop counter was dropped. The rest of the original draft's items are dropped, deferred or rejected (Appendix B).

Line numbers below are as of 2026-11-16 and will drift; grep for the anchors.

---

## 1. Fixes

### 1.1 Multi-generation sample retirement (use-after-free window)

**Severity:** Critical
**Location:** `plugins/Sgraffito/Source/PluginProcessor.h:304` (`retiredSample`), `PluginProcessor.cpp:223` (`finishSampleLoad`), `PluginProcessor.cpp:1189` (`clearSample`)

**Problem (verified).** The plan 07 retire pattern protects exactly **one** generation: `retiredSample = std::move (loadedSample)` keeps the previous buffer alive, but the *next* install overwrites the slot, destroying the buffer before it. An audio block loads `sampleBuffer` once and holds the pointer for the whole block, while installs run on the message thread concurrently with the audio thread. If two installs (or an install plus a `clearSample`) land while one block is running — rapid preset or sample switching can do this — the second install frees a buffer the running block is still reading.

**Fix.** Replace the single slot with a capped retirement queue, and route both retire sites through it:

```cpp
// PluginProcessor.h
static constexpr int retiredSampleGenerations = 2;
std::deque<std::unique_ptr<LoadedSample>> retiredSamples;

// PluginProcessor.cpp
void SgraffitoProcessor::retireCurrentSample()
{
    if (loadedSample == nullptr)
        return;

    retiredSamples.push_back (std::move (loadedSample));

    // A block holds at most one pointer, read at its start. After two more installs,
    // no block can still reference the oldest entry (blocks are milliseconds; installs
    // are message-thread serialised), so the front is safe to destroy.
    while ((int) retiredSamples.size() > retiredSampleGenerations)
        retiredSamples.pop_front();
}
```

- `finishSampleLoad` and `clearSample` both call `retireCurrentSample()` instead of the single move.
- The deque grows only on the message thread; the audio thread never touches it. Allocation on push_back is acceptable there.
- Keep the destructor's implicit teardown (the pool is already destroyed before any handoff could run — the `LoadLifetime` token covers that).

**Tests** (added to `SgraffitoTests`, no new executable):
- Rapid-swap stress: alternate async loads and `processBlock` calls, pumping the message loop so installs land between and *during* block sequences; many cycles, no crash.
- Lifetime test: after two consecutive installs, drive blocks that started before the first install; exercises the exact window the fix closes.

### 1.2 Generation stamp without the second atomic read

**Severity:** Low (hardening, no observable bug today)
**Location:** `plugins/Sgraffito/Source/PluginProcessor.cpp:209–215` (`loadSampleAsync`)

**Problem (verified).** `loadSampleAsync` does `fetch_add(1)` and then a separate `load()` to stamp the job. All callers are message-thread (editor `PluginEditor.cpp:314`, state application `PluginProcessor.cpp:1239/1252`), so nothing interleaves today — but the two-step stamp is a theoretical window and is unnecessary.

**Fix.** Capture the generation the request itself created:

```cpp
const int generation = loadGeneration.fetch_add (1, std::memory_order_relaxed) + 1;
pendingSampleLoads.fetch_add (1, std::memory_order_relaxed);
sampleLoadPool.addJob (new SampleLoadJob (file, generation, targetRate, loadLifetime, this), true);
```

The latest-wins check in the handoff (`PluginProcessor.cpp:132`) is untouched. No behavior change; the existing plan 07 async tests (latest-wins race, destruction with a load pending) must still pass unchanged.

### 1.3 Unify the three note-on voice-allocation paths

**Severity:** High — includes an **owner decision (D1)** before any code change
**Location:** `plugins/Sgraffito/Source/PluginProcessor.cpp` — direct path `handleNoteOn` (~267–369), ghost fire path (~806–827), pending fire path (~830–897)

**Problem (verified).** The pending-fire path hand-rolls what `VoicePool::allocateVoice` does, and diverges in three ways:

1. **No stealing.** It scans for an inactive voice only and silently drops the note when all 32 are busy. The direct and ghost paths go through `allocateVoice`, whose `findVoiceToAllocate` steals (tape-stopping → oldest release → oldest active).
2. **Cloud mode loses its lanes.** `allocateVoice` allocates `cloudLanesPerNote` voices for mode 2; the pending path allocates one.
3. **Duplicated setup.** Region math (~864–885 duplicates `makeRegion`), pitch, velocity curve and position init are copied from the direct path. This duplication already shipped the "always sounds the same" position bug (plan 05.1).

The ghost fire path is the model: it calls `allocateVoice (key, velocity, span, length, mode, sliceCount, scheduledPos, key)` and then applies only the extras.

**Fix.**
- The pending path calls `voicePool.allocateVoice (...)` with `(scheduledPos, key)`, same as the ghost path.
- Extract the post-allocation setup shared by all three paths (rate from pitch fine, velocity curve, position from `regionStart`) into one private helper; `handleNoteOn`, the ghost fire and the pending fire all use it. `allocateVoice` already value-initialises the voice (`Voice{}`), which subsumes the pending path's manual `envEnd`/`envEndSet` resets (~856–857).

**Rider (D2, decided 2026-11-16).** In the same `processBlock`, name the Cloud-grain guard literals (~910–912) as file-local constants — `minGrainSizeSamples`, `minGrainIntervalSamples`, and the grain-spread clamp. No behaviour change, no DSP-header edits, and no separate build-and-test cycle: the function is already being changed here.

**Owner decision D1 — stealing vs drop for pending notes. DECIDED 2026-11-16: Option A, pending notes steal.** Pending notes allocate through `allocateVoice` exactly like the direct and ghost paths — no free-voice probe, no drop path. A pending note fired when all 32 voices are busy steals per the established order (tape-stopping → oldest release → oldest active).

**Baseline A/B (AGENTS.md §11 worktree procedure).** The no-delay path (`delayMs == 0`, which never enters `pendingNotes`) must stay **bit-identical** — assert it. The delayed path is *expected* to change in the all-voices-busy and Cloud cases; record the probe diff as the change record, not as a regression.

**Tests** (in `SgraffitoTests`):
- A pending note with all 32 voices busy steals a voice (tape-stopping → oldest release → oldest active, per D1).
- A delayed Cloud note allocates `cloudLanesPerNote` voices.
- A delayed note's pitch, region and position match the same note played undelayed.

### 1.4 Preset selection must survive editor recreation ("INIT" after reopen)

**Severity:** High — user-visible bug in every plugin, bank build
**Reported:** 2026-11-16 by the owner: open any plugin in a DAW, select a preset, close the window, reopen — the bar shows "INIT", not the selected preset.
**Location:** `modules/suite_presets/Source/PresetBar.cpp:621–625` (`currentFlat`/`currentName`/`currentCategory`), constructor at `:156–157`; all four editors own a `presetBar`.

**Root cause (verified).** The selection lives in `PresetBar::Impl`, which the editor owns and destroys with the window. A new bar's members default to `currentFlat = -1`, `currentName = "Init"`, and the constructor snapshots only the state *hash* (`snapshotHash`) — so the dirty flag behaves correctly after reopen — but nothing recovers the selection: no comparison of the current state against the bank, and no query to the processor, which is the part that actually survives window close. Every new bar therefore starts at "Init" whatever the state is.

**Fix.** Move the selection identity into the processor; the bar records it and restores it.

1. **New value type + `StateHolder` seam** (`SuitePresets.h`), with *default* implementations so no existing implementer breaks:
   ```cpp
   struct PresetSelection
   {
       enum class Kind { none, factory, userFile };
       Kind kind = Kind::none;          // none = Init / not a preset
       int factoryIndex = -1;           // stable: bank order is append-only
       juce::String userFilePath;       // user presets: identity by file path, not list
                                       // position (the scanned user list is re-ordered by
                                       // file changes between window opens)
   };

   // StateHolder (default bodies keep test stubs and future implementers compiling):
   virtual PresetSelection getPresetSelection() const { return {}; }
   virtual void setPresetSelection (const PresetSelection&) {}
   ```
2. **Bar records and restores** (`PresetBar.cpp`): `applyFlat`/`applyInit` call `holder.setPresetSelection(...)` after applying. The constructor asks `getPresetSelection()` and maps it: `factory` → the plugin's factory slice index; `userFile` → match by path in the freshly rescanned user list; `none` → Init. A recorded user file that no longer exists falls back to Init (the audio state is untouched — correct, it is whatever the session left).
3. **Processor storage + session persistence (D3, decided 2026-11-16: persist)**: each processor holds a `PresetSelection lastPresetSelection` and implements the two virtuals. `setPresetSelection` writes the identity onto `apvts.state` as a single property at change time — the `lerpCurve` / `samplePath` precedent — encoded as e.g. `presetSelection = "factory:12"` or `"user:<path>"`, with absent (or `"none"`) meaning Init. It therefore rides in every saved session and host program dump without touching the parameter list or `numControls`. `setStateInformation` restores `lastPresetSelection` from the property after the state read; a session without the property (every session saved before this change, and INIT) yields `none` → "Init". `applyPresetState`'s state replacement wipes the property, which *is* the reset: the bar or the program bridge re-sets it immediately after applying. Under `SUITE_PRESET_BANK_AS_PROGRAMS`, `setCurrentProgram` sets the selection too; keep `currentProgramIndex` and the selection updated at the same call sites and document that they describe the same event.
   - **No `suiteStateVersion` bump, no migration** (owner decision, 2026-11-16): the plugins are unreleased, so there are no public sessions to migrate, and the property is purely additive — its absence in an old state simply means Init, which is the behaviour a fresh plugin already has. AGENTS.md §9's rule is thereby satisfied by additivity rather than by a migration.
4. **Public read-back for tests and multi-editor sync**: `juce::String PresetBar::getCurrentPresetName() const`. The 10 Hz `timerCallback` additionally compares the holder's selection with the local one and refreshes the name on a difference — this keeps two simultaneously open editors of one processor consistent, at no extra cost, in the timer's existing rhythm.
5. **Default build (`SUITE_PRESET_BANK=OFF`) unchanged**: the factory list is empty, the selection stays `none`, no visible or behavioural difference.

**Renders.** The bar is header UI, so per AGENTS.md §15.7 store before/after renders of one editor per plugin under `build/renders/preset-bar-reopen/` — no paint or layout change is expected, but the reopened-editor state is new territory.

**Tests** (in the plugin `*_PresetTests`, bank build; no new executables):
- Select factory preset *i*, destroy the editor, recreate it → `getCurrentPresetName()` equals the preset's name; repeat for the Init path (shows "Init").
- Tweak a parameter after selecting, reopen → name restored **and** the dirty `*` appears (pins the hash-restore interplay).
- User preset round trip in a temp preset folder: save, apply, reopen → name restored by path; delete the file, reopen → falls back to "Init".
- Session round trip (D3): select a factory preset, `getStateInformation` into a fresh processor instance, `setStateInformation`, create the editor → the bar shows the preset's name. A state without the property (hand-made or pre-change fixture, e.g. `build/fixtures/plan07/`) → "Init".
- Host-program bridge (flag build): `setCurrentProgram` reflects in the bar after an editor recreation, and `getCurrentProgramStateInformation` carries the identity.

**Scope note (Sgraffito).** Restoring the *name* never re-applies state, so the sample-linked preset machinery is not touched: whatever `samplePath` the session carried stays as-is. With D3, a Sgraffito session now carries two additive properties — `samplePath` (the audio content, plan 07) and `presetSelection` (the bar's identity, this plan) — which never interact: the sample resolves from its own property, the name from its own.


---

## 2. Owner decisions

| # | Question | Options | Outcome |
|---|---|---|---|
| D1 | Pending notes when all 32 voices are busy | A: steal (consistent) / B: drop (current, silent) | **Decided 2026-11-16: A — steal** |
| D2 | Former Phase 2 scope (guard constants; ring drop counter) | Both / fold 2.1 into 1.3 only / neither | **Decided 2026-11-16: fold the guard-constant naming into 1.3 as a rider; drop the ring drop counter** (Appendix B). No standalone phase remains |
| D3 | Should the preset identity persist in *saved sessions* too, or only in memory (window close/reopen)? | A: in-memory only / B: persist as a state property | **Decided 2026-11-16: B — persist.** No migration and no `suiteStateVersion` bump: the plugins are unreleased (no public sessions), and the property is additive — an old state without it reads as Init |

---

## 3. Testing strategy

- **Unit (SgraffitoTests):** retirement generations; pending-path stealing/Cloud/pitch-region tests (1.3); existing plan 07 async tests must pass unchanged (1.2).
- **Unit (plugin `*_PresetTests`, bank build):** preset-selection reopen tests (1.4) — factory, Init, dirty-after-reopen, user-file path match and fallback, host-program bridge.
- **Integration:** rapid sample-swap stress (async loads + `processBlock` interleaved, message-loop pump pattern from the plan 07 tests); preset switching during playback.
- **Regression:** bit-identity probe for the no-delay note path (1.3); full CTest before and after — **11 entries default build / 15 with the bank on; the count must not change** (all new tests join existing executables, no new ones).
- **Renders:** 1.4 touches the header bar, so before/after renders of one editor per plugin under `build/renders/preset-bar-reopen/` (AGENTS.md §15.7). Items 1.1–1.3 touch no UI; no renders required.

## 4. Risk assessment

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| 1.3 changes delayed-note audio | Certain (intended) | Medium | D1 decided up front; bit-identity assert on the no-delay path; worktree baseline probe |
| 1.4 `StateHolder` interface change ripples to all plugins | Certain | Low | New virtuals carry default bodies, so no existing implementer breaks; tests join existing executables |
| 1.4 state property escapes the intended format (D3) | Low | Low | One property, `lerpCurve`/`samplePath` precedent, additive by design; session round-trip test pins encode/decode; no parameter or `numControls` change |
| 1.1 retirement queue keeps memory alive longer | Certain | Low | Cap is 2 generations; measured in sample-buffer sizes, bounded |
| Fixes introduce new races | Low | High | All install-path changes stay on the message thread; existing async race tests re-run |
| Test coverage gaps | Medium | Medium | Each fix lands with its test in the same change |

## 5. Success criteria

- [ ] 1.1: rapid-swap stress test passes; both retire sites route through the queue
- [ ] 1.2: one-line change in, plan 07 async tests unchanged and green
- [ ] 1.3: one allocation path for all note-ons; no-delay path bit-identical; D1 behavior tested
- [ ] 1.4: preset selection survives editor recreation in all four plugins (factory, Init, dirty, user file, host program) **and persists in saved sessions (D3)**; default build unchanged; old states without the property read as Init
- [ ] Full CTest green with unchanged entry count (11 default / 15 bank)
- [ ] Renders for 1.4 stored under `build/renders/preset-bar-reopen/`
- [ ] AGENTS.md §14 (Sgraffito notes) and CHANGELOG.md updated
- [ ] Post-change checks in AGENTS.md §15 run and reported

---

## 6. Notes for coding agents

**Stay inside the project.** Build output in `build/`, logs in `build/logs/`, baselines in `build/baselines/`.

**Use absolute paths.** The shell's working directory persists and can change.

**Baseline the audio path.** For 1.3, use the worktree procedure in AGENTS.md §11 (`-DFETCHCONTENT_SOURCE_DIR_JUCE=<repo>/build/_deps/juce-src`, no re-download).

**Test incrementally.** Full CTest after each item, before starting the next.

**Record what you verified.** Line numbers in this plan are from 2026-11-16; re-check each anchor against the code before acting (docs/plans/README.md rule).

---

## Appendix A: Verification record (2026-11-16)

Every original claim was checked against the code before this revision:

- **1.1 "iterator invalidation"** — the loop at `PluginProcessor.cpp:792` is `for (size_t i = 0; i < pendingNotes.size(); )` with erase + `continue` (no increment; `size()` re-checked). Correct as written; no iterator held across an erase. The proposed rewrite allocates a vector on the audio thread — rejected.
- **1.2 "TOCTOU race"** — all `loadSampleAsync` callers are message-thread (`PluginEditor.cpp:314`; `PluginProcessor.cpp:1239`, `1252`). The latest-wins check at `PluginProcessor.cpp:132` makes "both jobs install" impossible; the proposed `compare_exchange` inverts latest-wins into first-wins. Reduced to the hardening one-liner (→ 1.2 above).
- **1.3 "use-after-free"** — the concern is real but the window is the *second* install during one running block (single retire slot). The proposed `sampleIsSafeToUse` flag guards nothing: the swap can occur between the audio thread's flag check and its pointer load, and the proposed `processBlock` guard is empty. Replaced with the retirement queue (→ 1.1 above).
- **1.4 "unclamped modulation"** — `Router::modulate` clamps every target to its normalised range (`SuiteModulationRouter.cpp:203`; AGENTS.md §4.4). The proposed hardcoded 5000 ms clamp could silently disagree with the parameter's real range. Dropped.
- **2.1 "deadlock"** — the UI ring (`PluginProcessor.h:353–363`) is lock-free SPSC; deadlock is impossible; the drop is commented, designed behavior. The drop counter it suggested was later dropped too (D2, Appendix B).
- **2.3 "PresetFile error enum"** — the string errors carry context (paths, versions) and nothing branches on error kind. Dropped.
- **3.1 "duplicate note handling"** — confirmed and worse than filed: the pending path also skips Cloud's multi-lane allocation and never steals. Promoted to a fix item (→ 1.3 above).
- **3.2 "surface effects state"** — `dust/crackle/wow/flutter` are not wired to audio yet (AGENTS.md §14); plan 05.4 owns their semantics. Deferred to 05.4.
- **3.3 "map-based parameter caching"** — `unordered_map` lookups put string hashing on the audio thread; direct `std::atomic<float>*` members are the correct real-time pattern. Rejected.
- **3.4 "voice pool priority queues"** — `maxVoices` is a fixed 32; the scan is ≤ ~96 member comparisons per note-on; three ordered structures to keep in sync cost more bug surface than they save. Rejected.
- **Preset bar "INIT" after reopen (reported by the owner, 2026-11-16)** — investigated the same day; confirmed. The selection (`currentFlat`/`currentName`, `PresetBar.cpp:621–622`) is editor-owned state; the bar constructor (`:156–157`) snapshots only the state hash, so the dirty flag survives reopen correctly but the name always resets to "Init". No processor-side record of the last applied preset exists outside the `SUITE_PRESET_BANK_AS_PROGRAMS` gate, and that one is factory-only and compile-gated. Became item 1.4.

## Appendix B: Dropped from the original draft

| Original item | Disposition | Reason |
|---|---|---|
| 1.1 vector-erase rewrite | Dropped | Current loop is correct; the fix allocates on the audio thread |
| 1.2 as filed (compare_exchange) | Replaced | No concurrent callers; the fix inverts latest-wins semantics |
| 1.3 as filed (safety flag) | Replaced | The flag guards nothing; the real fix is multi-generation retirement |
| 1.4 modulation clamp | Dropped | `Router::modulate` already clamps to the parameter range |
| 2.1 as titled (deadlock) | Reduced, then folded away | No locks exist; the counter idea it left behind was dropped by D2 |
| 2.3 error enums | Dropped | No consumer branches on error kind; strings carry needed context |
| 3.2 surface state | Deferred | Plan 05.4 owns the unwired surface controls |
| 3.3 parameter map cache | Rejected | Real-time regression (string hashing on the audio thread) |
| 3.4 voice pool queues | Rejected | Premature at n = 32; adds sync bug surface |
| 2.2 UI-ring drop counter | Dropped (D2, 2026-11-16) | No observed problem: the ring drains every `processBlock`, and filling 32 slots between two blocks would take inhuman playing; a counter also has no UI surface in the field. The original draft's `std::cerr` warning was worse still — a plugin should not write to stderr |
