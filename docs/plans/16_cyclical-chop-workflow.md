# Plan 16: Cyclical chop workflow — auto-chop, slice re-arranging, transformation tools

Written for: the coding agents who carry out the phases. Read `AGENTS.md` first. Split from the
plan 14 post-review round (2026-11-22): the owner's Ableton test found Cyclical silent and asked
for workflow tools. Plan 14's overhaul itself is implemented and verified.

Naming: MidiTool = `plugins/MidiEffect` (AGENTS §4).

Status: implemented 2026-11-22 (Phases 1-3 and 5; Phase 4 dropped by the owner — the rescan
fixed the Ableton path). 28/28 CTest; 210 presets audited; renders under
`build/renders/plan16-cyclical/`.

---

## 0. Verified state of the Ableton silence (2026-11-22)

| Check | Result |
|---|---|
| Fresh VST3 through JUCE's own host, stereo output buffer, note-on 60 | **Makes sound** (peak 0.4275). The host probe is now a permanent check (`tests/host/PluginHostTests.cpp`) |
| The same VST3, a 1-channel host buffer against the 2-output bus | Silent — the one reproducible silence, but no DAW hands a mismatched buffer |
| Build identity (`Defs.txt`) | Identical class to MidiTool/Sgraffito (`IsSynth=1`, `WantsMidiInput=1`), both of which work in the owner's Ableton |
| `resetMissing` for an old session's absent IDs | Restores defaults (Note 60–63, Latch, Quantize Off) |
| Owner's test | Different presets, INIT state, notes from a keyboard, Host Sync — **nothing reacts at all**: the playheads never move, so MIDI never reaches the processor |
| DAW | Ableton Live, stereo track |
| **Resolution (2026-11-22)** | **A full VST3 rescan fixed it**: Ableton's plugin cache held the pre-plan-14 parameter list (35 params); the fresh 57-parameter plugin loaded after the rescan. Not a plugin bug. Lesson recorded in AGENTS §10: after a parameter-count change, a DAW rescan is part of the deployment |

Strong hypothesis: this is the same Ableton-VST3-MIDI issue the suite saw with MidiTool — the
plan-13 finding "Keyboard/Chords should be playable/clickable in any way" reads as its
workaround. The processor is fine; the host is not feeding it. Owner checks (before any code
change): **Ableton's on-device bypass switch** (a cached bypass state stays silent and inert),
and a full rescan with the VST3 cache cleared. The workaround shipped in Phase 4 is the same
one MidiTool got: a playable keyboard in the editor.

---

## 1. Decisions (owner, 2026-11-22)

| Gate | Decision |
|---|---|
| Auto-chop | **Transient slicing**: an AUTO button analyses the loaded loop, detects onsets, and writes the actual slice boundaries per detected hit. Boundaries saved with the session |
| Slice re-arranging | Requested; design in Phase 2 (a per-slot slice order, Dr. Rex pattern lane) |
| Loop transformation tools | Requested; the owner defines the tool set (Phase 3 asks) |
| Window 1000×876 | **Approved** (plan 14 G-C closed); lockup regenerated; screenshots reviewed |

## 2. Phases

### Phase 1 — Auto-chop (transient slicing)
Files: `plugins/Cyclical/Source/{PluginProcessor,PluginEditor,LoopWaveformView}.*`,
`tests/PluginTests.cpp`.

1. Per-slot state property **`chopNBounds`** (the lerpCurve pattern): a comma-separated list of
   the interior slice-boundary shares (0…1), written by the analyser, cleared when the user
   moves Slices or Slice Length. Empty property = the equal/skew split of plan 14.
2. The **AUTO** button sits in each card's waveform header, next to LOAD (the pill row is
   full). Dimmed while chop is off. The analyser (message thread) detects onsets on the
   slot's region (envelope windows, a relative threshold, merged closer than ~100 ms, capped
   at 16 slices), sets Slices to the count, and writes the boundary shares.
3. `regionFor` uses the property's shares when chop is on and the count matches Slices;
   otherwise the plan-14 power curve. The waveform draws the same shares (they already flow
   through `setChopLines`).
4. Tests: the analyser on a synthetic loop (clicks at 1/3 and 2/3 → shares ≈ 0.333/0.667);
   audio starts at the property's boundary; moving Slices falls back to the power curve; the
   button is in the card; INIT/empty tree clears the properties.

### Phase 2 — Slice re-arranging (Dr. Rex pattern lane)
Files: processor, editor, tests.

Status: implemented and verified 2026-11-22 (plugin tests 37 functions; CTest 28/28).

1. Per-slot **slice order**: a state property holding a permutation of 0…slices−1 — key k
   plays the slice the order names. The re-arranging UI is a strip of small step cells along
   the bottom of each chopped slot's region band (inside the waveform, so the approved
   1000×876 window does not change): click a cell to cycle its slice number. An identity
   arrangement clears the property again.
2. `regionFor` maps through the order; the playhead already follows the fresh span (the
   source slice). The audio follows the order. A count mismatch falls back to the identity;
   chop off ignores it; the AUTO analyser resets the arrangement.
3. Tests: the order round-trips, the audio follows a rearranged order (verified against the
   ramp loop; the read sits inside the source slice, with the seam pre-roll and rendered
   sample count as the window), the mismatch fallback, cycling to identity, chop-off.

### Phase 3 — Loop transformation tools (owner defines the tool set)
Files: processor, editor, `LoopWaveformView.*`, tests.

Status: implemented and verified 2026-11-22. The owner picked **Reverse (loop / slice),
Normalise, Silence trim** (no export).

1. Three buttons join the waveform header (LOAD, AUTO, REV, NORM, TRIM — five 48 px buttons
   in the 22 px band). Each tool builds a **fresh buffer** and installs it through the
   retirement queue (a playing voice keeps its old buffer); the loop reference (path/URI) is
   unchanged, so a session reload re-reads the original file — the transforms are
   session-local.
2. **REV** reverses the whole loop; with a **selected source slice** it reverses only that
   slice's span. The selection: **right-click an arrangement-strip cell** (left-click keeps
   cycling); the selected cell carries the accent fill and its number; the same cell again
   deselects. NORM scales the peak to −0.1 dBFS (silent loops stay silent). TRIM sets
   Start/Length to the first/last frames above a small threshold — the buffer is untouched,
   the region covers the content.
3. The selection is message-thread state (not saved); the AUTO analyser resets it.
4. Tests: the reversed buffer reads backwards and the audio sits in the reversed content;
   the slice-scoped reverse touches only the selected span; the normalised peak sits at
   −0.1 dBFS; TRIM's Start/Length match the content with the buffer untouched. The JUCE 9
   right-click test is `event.mods.isPopupMenu()` (the MouseEvent::isPopupMenu helper is
   gone in JUCE 9).

### Phase 4 — Playable keyboard strip (the Ableton workaround)
**Dropped by the owner, 2026-11-22**: the rescan fixed the Ableton path, so the keyboard strip
is no longer needed. Cyclical keeps its DAW-side triggering only.

### Phase 5 — Documentation, renders, verification
Manual + README + AGENTS + CHANGELOG, renders, clean build, 28/28 CTest, bank audit
(210 presets — no new presets unless the owner asks).

## 3. Open points

| Item | Note |
|---|---|
| Ableton silence | **Resolved**: the rescan fixed it (Ableton's cache held the old parameter list); the host-test sound check stays as a regression guard |
| Transformation tools | **Shipped** (owner's pick: reverse loop/slice, normalise, trim; no export) |
| The bounds property vs presets | A preset may carry `chopNBounds` like `loopNPath`; INIT clears it |
| Auto-chop thresholds | Relative (15 % of the loop peak) and merge window (100 ms); tuned against the embedded library |
| Knobs untouchable | No knob change anywhere; the AUTO button is a pill-style button, not a knob |
| The arrangement vs the analyser | A Slices edit falls back via the count mismatch (no listener — the analyser's own Slices write must not clear its boundaries); a Slice Length edit clears through the APVTS listener |
| Transforms are session-local | The buffer is transformed in memory; the loop reference is unchanged, so a reload re-reads the original file (documented in the manual) |
| Target naming | The MSBuild targets are `<Name>Tests`, `<Name>PresetTests` (no underscore between name and Tests) — `--target Cyclical_PresetTests` matches nothing and MSB1009 is easy to miss in a filtered build log |
