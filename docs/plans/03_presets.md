# Plan: Preset system for the Audio Plugin Suite

Status: superseded by plan `03.3_free_presets_init_only.md` (2026-11-10). The Preset bar, `.ausp` I/O,
user presets and menu are public and always compiled. The factory bank is **always embedded** and carries
exactly one `INIT` preset per plugin (3 total, INIT-only). No Pro overlay, no private repository,
no paid gating remains.

Scope: all three plugins (Spectral Drift Shifter, Dynamic Saturation Companion, Micro-Timing Chaos).

> **Note:** This file kept the old Pro-tier design for historical reference; the authoritative
> behaviour is in plan `03.3_free_presets_init_only.md` and the current code.

## Decision record 2026-11-09 (review outcome)

- **§1.1 Owner decision: the preset bar is public for all users; only the presets are sold.**
  The seam, `PresetBar`, `.ausp` I/O, user presets and menu move into `modules/suite_presets`
  (public, always compiled). `SUITE_PRO_PRESETS` is renamed `SUITE_PRESET_BANK` and gates only the
  **embedded factory bank** and, through it, the host program list. Private-overlay repo and
  `SUITE_PRO_DIR` are dropped.
- **Delivery model: hybrid.** A small free factory subset stays embedded (so every user sees the
  feature work), and the full set ships as paid `.ausp` data files (data product, separate licence).
  Paid content needs no build. Free-subset selection is an owner sign-off item.
- **§5 accepted:** bank keeps the delta authoring (no full `.ausp` materialisation from deltas).
- **`lerpCurve` verdict:** free for user-saved presets and sessions; explicit attribute for the bank —
  correct as built. One fix adopted: MTC must reset the curve to its default when an applied preset has
  no `lerpCurve` attribute.
- **§1.2 opinion: keep `getPresetStateVersion()`.** The file writer needs the version; alternatives
  (hardcode, registry) are worse.
- **§1.3 keep it simple:** current in-tree wiring stays.
- **§3.2 scale note:** the ⅔ design scale applies to the whole MTC editor in every format including
  Standalone, so the bar teachings small there too. Adopted: MTC bar back to the planned 360×34 at
  design scale.
- **Gaps adopted into scope:** right-click context menu on the preset name (Save As / Rename /
  Delete / Reveal; user presets only), a bank CI job, and editor renders under `build/` as pixel
  evidence for layout claims.

Grounded in the current code: all three processors use `AudioProcessorValueTreeState` and a plain
`copyState()/replaceState()` pair, Micro-Timing Chaos already keeps non-parameter state (`lerpCurve`)
inside `apvts.state`, and each editor draws a header band that has room on the right. No preset code
exists anywhere yet.

---

## 0. Decisions

| Question | Decision |
|---|---|
| Pro gating | None: the preset bar ships in every build; the factory bank is always embedded (INIT-only). |
| Preset scope | Full plugin state: every APVTS parameter **plus** non-parameter state (MTC `lerpCurve`). |
| Factory content | One `INIT` preset per plugin (3 total). |
| Host program list | Always compiled; program 0 = Init, program 1 = the plugin's `INIT` preset (`getNumPrograms() == 2`). |

Two constraints the design works around:

- **No new APVTS parameters.** The preset system adds none, so no saved session changes meaning and
  `checkParameterIds` is only extended by the routing parameters that already exist. Current counts
  (AGENTS.md section 4): Spectral Drift 48 (14 sound + 34 routing), Dynamic Saturation 47 (13 + 34),
  Micro-Timing Chaos 54. Preset files may only reference IDs that exist in these lists.
- **The public build must be visually and functionally identical to today.** The seam returns
  height 0 and draws nothing when Pro is off.

---

## 1. Architecture

### 1.1 Repo split

**Public repo (`AudioPluginSuite`) gets only the seam:**

```
modules/suite_presets/
  include/SuitePresets.h          # PresetBar (pimpl), StateHolder, isAvailable()
  Source/SuitePresetsStub.cpp     # public build: invisible, zero height
  SuitePresets.cmake              # suite_presets_add_to_target(<target>)
```

**Private overlay (`AudioPluginSuite-Pro`, not on GitHub):**

```
presets/
  include/SuitePresetsPro.h       # internal: bank, file I/O, menu model
  Source/PresetBar.cpp            # real implementation of SuitePresets::PresetBar
  Source/PresetBank.cpp           # embedded factory bank parsing
  Source/PresetFile.cpp           # .ausp read/write, user folder, sanitising
  Source/PresetMenu.cpp           # popup menu, save dialog, dirty marker
  bank/<plugin>/*.json            # authored deltas from default (source of truth, section 5)
  bank/AudioSuitePresets.xml      # generated single-file bank (do not edit by hand)
  tools/build_bank.py             # deltas -> full .ausp -> bank XML
  tools/audit_bank.py             # range/duplicate/id/default checks
  tests/PresetTests.cpp
  CMakeLists.txt
```

The public header is the contract. The Pro `.cpp` defines the same class symbols; CMake compiles
**either** the stub **or** the Pro sources, never both, so there is no ODR ambiguity.

### 1.2 The public seam API

`modules/suite_presets/include/SuitePresets.h`:

```cpp
namespace SuitePresets
{
    /** Implemented by each processor. The one place preset state enters and leaves a plugin. */
    class StateHolder
    {
    public:
        virtual ~StateHolder() = default;
        virtual juce::String getPresetPluginId() const = 0;                 // "SpectralDrift", ...
        virtual juce::AudioProcessorValueTreeState& getPresetStateTree() = 0;
        virtual void applyPresetState (const juce::ValueTree&) = 0;         // message thread only
    };

    /** True only in a Pro build. Lets tests and editors ask without #if. */
    bool isAvailable() noexcept;

    /**
        Preset header bar. In a public build it is invisible and reports height 0,
        so editor layout is unchanged.
    */
    class PresetBar : public juce::Component
    {
    public:
        PresetBar (StateHolder&, juce::Colour accent);
        ~PresetBar() override;

        int getPreferredHeight() const noexcept;      // 0 public, 30 Pro
        void paint (juce::Graphics&) override;
        void resized() override;

        /** Called on the message thread after a preset has been applied. */
        std::function<void()> onPresetLoaded;

        /** Message thread. Used by the host-program bridge and tests. */
        void selectPreset (int factoryIndex);
        int  getNumFactoryPresets() const noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
}
```

`applyPresetState` is the important piece: it makes the bar plugin-agnostic and guarantees MTC's
curve is reloaded, because each processor implements it once and `setStateInformation` delegates to
it.

### 1.3 CMake wiring

Root `CMakeLists.txt`, next to the existing options:

```cmake
option(SUITE_PRO_PRESETS "Build the Pro preset browser and factory bank" OFF)
set(SUITE_PRO_DIR "" CACHE PATH "Path to the private Pro overlay (contains presets/)")
include(${CMAKE_SOURCE_DIR}/modules/suite_presets/SuitePresets.cmake)
```

`SuitePresets.cmake`:

```cmake
function(suite_presets_add_to_target target)
  # CMAKE_CURRENT_LIST_DIR inside a function is the CALLER's directory; use the function list dir.
  if(SUITE_PRO_PRESETS)
    if(NOT EXISTS "${SUITE_PRO_DIR}/presets/CMakeLists.txt")
      message(FATAL_ERROR "SUITE_PRO_PRESETS=ON but SUITE_PRO_DIR is not set")
    endif()
    target_sources(${target} PRIVATE ${SUITE_PRO_PRESETS_SOURCES})
    target_include_directories(${target} PRIVATE ${SUITE_PRO_DIR}/presets/include)
    target_compile_definitions(${target} PRIVATE SUITE_PRO_PRESETS=1)
    target_link_libraries(${target} PRIVATE SuiteProPresetBank)
  else()
    target_sources(${target} PRIVATE ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Source/SuitePresetsStub.cpp)
    target_compile_definitions(${target} PRIVATE SUITE_PRO_PRESETS=0)
  endif()
  target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/include)
endfunction()
```

The overlay's `CMakeLists.txt` (added once, before the plugin subdirectories) defines
`juce_add_binary_data(SuiteProPresetBank SOURCES bank/AudioSuitePresets.xml)` and populates
`SUITE_PRO_PRESETS_SOURCES`.

Every plugin and its test executable calls `suite_presets_add_to_target(...)` right after
`suite_ui_add_to_target(...)`, in all six places (SpectralDrift, DynamicSaturation, MicroTimingChaos
+ their test targets).

> Gotcha to record in AGENTS.md: `CMAKE_CURRENT_LIST_DIR` inside a function resolves to the caller's
> listfile. `CMAKE_CURRENT_FUNCTION_LIST_DIR` (CMake >= 3.17) is the correct one.

---

## 2. Preset file format

Extension `.ausp`, UTF-8 XML, human-diffable, no compression:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<AudioSuitePreset formatVersion="1" plugin="MicroTimingChaos" pluginVersion="1.0.0">
  <Meta name="Slow Orbit" category="Ambient" author="AudioSuite"
        description="Wide, slow drift with a lazy gate."/>
  <Parameters ...>          <!-- exact output of apvts.copyState().createXml() -->
    ...
    <PARAM id="chaos" value="0.62"/>
    ...
  </Parameters>
</AudioSuitePreset>
```

Rules:

- `<Parameters>` is stored **verbatim**, so `ValueTree::fromXml(...)` feeds `replaceState` directly.
  `lerpCurve` lives inside that tree, so it round-trips for free.
- `plugin` must match `getPresetPluginId()`; a mismatch is refused with a readable message (no silent
  half-load).
- **Two version fields, one rule.** `formatVersion` on `<AudioSuitePreset>` is the preset file format.
  The `<Parameters>` element also carries `suiteStateVersion`, the same attribute the session state uses
  (`modules/suite_state/include/SuiteState.h`, R1 is done). A preset or session with a
  `suiteStateVersion` newer than the plugin is refused with a readable message and nothing is applied.
  A missing attribute means version 1. Migrations are the R1 `SuiteState::Migration` list, so presets and
  sessions use one code path.
- `formatVersion` is additive-only. Unknown elements are ignored, unknown parameter IDs are ignored.
- **Determinism on load:** after the state is replaced, every parameter whose ID is absent from the
  incoming tree is reset to its default (`parameter->setValueNotifyingHost(parameter->getDefaultValue())`).
  Otherwise a 1.0 preset loaded into 1.1 leaves new controls at whatever the previous preset left
  behind. This is a `resetMissing` option on R1's `SuiteState::read`. Presets and INIT set it. Session
  loading keeps its current behaviour, which is the same in practice because a restored instance starts
  at defaults.
- No host-specific data, no absolute paths, no sample rate, no automation.

**Factory bank:** one embedded file `AudioSuitePresets.xml` (`<Bank version="1">` + 80
`<AudioSuitePreset>` children), built by `tools/build_bank.py` and embedded with
`juce_add_binary_data`. Roughly 200 KB. Advantages over loose files: works in the Standalone,
nothing to install, one versioned asset, and the bank order can be made append-only for the Phase 7
program list.

**User presets:** `File::getSpecialLocation(userDocumentsDirectory) / "AudioSuite" / <pluginName> /
"Presets"`, created on first save. `*.ausp`. Factory presets are read-only and are *not* copied to
disk.

---

## 3. Per-plugin code changes (public repo)

### 3.1 Processor: one state entry point

In all three processors (`plugins/*/Source/PluginProcessor.h/.cpp`):

```cpp
class XProcessor : public juce::AudioProcessor,
                   public SuitePresets::StateHolder
{
public:
    juce::String getPresetPluginId() const override;                 // "SpectralDrift" etc.
    juce::AudioProcessorValueTreeState& getPresetStateTree() override { return apvts; }
    void applyPresetState (const juce::ValueTree&) override;         // replaceState + per-plugin refresh
    ...
};
```

`setStateInformation` becomes a call to R1's `SuiteState::read (data, size, apvts, stateVersion,
migrations, afterLoad)`, and `applyPresetState` is the same path with `resetMissing = true`. The
version check, the migrations and the refusal rule live in R1 and are not repeated here. This is a
pure refactor for SD and DS. For MTC, `loadLerpCurveFromState()` moves into `afterLoad`, so the
session path and the preset path share it.

**Ordering.** R1 is done, so this prerequisite is met. Without the version field in place, the
preset refusal rule in section 2 has nothing to check.

MTC also gets a note panic:

```cpp
std::atomic<bool> panicRequested { false };   // set by applyPresetState
```

At the top of `processBlock`, if set: `stopAllNotes()`, clear `pending`, clear `sounding`, reset
`nextStep`/playhead, clear the flag. Reason: a preset can flip `sync`, `rate` or `length` mid-
playback; queued note-offs survive but the grid is re-based, which is the classic stuck-note path in
this codebase.

### 3.2 Editors: reserve the bar, nothing else

Each editor gains one member and three lines:

```cpp
SuitePresets::PresetBar presetBar { processor, accentColour };
...
addAndMakeVisible (presetBar);
presetBar.onPresetLoaded = [this] { repaint(); };   // MTC: also curveEditor.repaint()
```

Layout (all three, inside the existing header band):

```cpp
const int barHeight = presetBar.getPreferredHeight();          // 0 in public builds
if (barHeight > 0)
    presetBar.setBounds (headerArea.removeFromRight (presetBarWidth).withHeight (barHeight)
                            .withY (headerArea.getCentreY() - barHeight / 2));
```

Because the height is 0 and the component is invisible, the public build's `resized()` result is
bit-identical to today. No `#if` in plugin code.

**Space budgets**

Sizes below are from the code as of 2026-11-08 (`PluginEditor.cpp` in each plugin). The earlier figures
(780x740, 880x620) predate the MOD page, which already added the tab row.

| Plugin | Today | Header | Change |
|---|---|---|---|
| Spectral Drift | `setSize` 780x776; `setResizeLimits` min 640x656 | 40 px (`headerHeight`) | Bar 300x30 right-aligned in the header. Move the FFT info line to the bottom strip at 11 pt dim, left-aligned. Height +0 (the bar fits the 40 px header). |
| Dynamic Saturation | `setSize` 880x660; `setResizeLimits` min 720x656 | 40 px (`headerHeight`) | Same bar. Info line to the bottom strip. Height +0. |
| Micro-Timing Chaos | design 1695 x 1400 (`editorHeight`), shown at 2/3 scale | 60 px (`titleBandHeight`), right side empty | No size change. Bar 360x34 at design scale, right-aligned in the title band. `setScaleFactor` multiplies by 2/3, so 34 px renders as about 23 px. Use 15 pt fonts. |

Because the bar fits the existing header, the resize minimums do not need to grow. If the owner wants
a taller bar than 40 px, raise `setResizeLimits` by the same amount for SD and DS, and re-render the
editor baseline.

---

## 4. Preset bar behaviour (Pro)

Visual: `[ < ] [ Preset name v ] [ > ] [ INIT ] [ SAVE ]`, drawn with `SuiteUI::Colours` and the
plugin accent. Flat rounded rect, 1 px `track` border, matching the group panels.

- **Name button** opens a `PopupMenu`: `Factory` submenu grouped by category, `User` submenu grouped
  by category, separator, `Init`, `Show preset folder`. Current preset ticked.
- **< / >** step within the currently filtered group (factory or user, current category), wrapping.
- **INIT** applies all defaults through the same `applyPresetState` path (a synthetic "Init" preset),
  and marks the bar dirty.
- **SAVE** opens an async `AlertWindow` (`MessageManager::callAsync`, never modal-blocking): Name
  field, editable Category combo, OK/Cancel. Overwrite asks for confirmation. Name is sanitised for
  the filesystem (`\ / : * ? " < > |`, control chars, leading/trailing dots and spaces) before
  becoming the filename; the display name stays as typed.
- **Right-click** on the name: `Save As...`, `Rename...`, `Delete`, `Reveal in Explorer/Finder` -
  user presets only, factory entries greyed.
- **Dirty marker:** `Name *` when the live state differs from the loaded snapshot. Implement with an
  `apvts` `Listener` that sets a flag; the bar compares a cheap FNV hash of `(id, value)` pairs on a
  10 Hz `Timer` rather than on every callback.
- **Errors** (unreadable file, bad plugin id, unwritable folder) surface as a `NativeMessageBox`
  with the file path, and the previous preset stays active.

Threading: everything is message-thread. The bar holds no audio-thread state, adds no locks, and does
no file I/O on the audio thread. `replaceState` notifies host automation per parameter - standard
behaviour for a preset load, and documented as such.

---

## 5. Factory presets (3 INIT)

The factory bank carries exactly one `INIT` preset per plugin (name `INIT`, category `Utility`,
description `The default state.`, delta = `{}` — every parameter at its default). It is exempt from
the not-default audit rule.

| Plugin | Preset | Category | Delta |
|---|---|---|---|
| Spectral Drift Shifter | INIT | Utility | none (default state) |
| Dynamic Saturation Companion | INIT | Utility | none (default state) |
| Micro-Timing Chaos Sequencer | INIT | Utility | none (default state) |

Hard rules enforced by `audit_bank.py`:

- Unique names per plugin, non-empty category and description, >= 1 category per plugin.
- Every value inside the parameter's `NormalisableRange`; every ID exists.
- No preset equals the default state — the INIT preset is explicitly exempt.
- Micro-Timing Chaos: host `sync` off in every factory preset, `bpm` left at 120. LFO sync
  (`lfo1Sync`, `lfo2Sync`) may be on, because it locks to the internal beat clock, not the host.
- Each preset differs from the default state in at least one parameter — satisfied by presets other
  than INIT; INIT is exempt.

The authoring source is `tools/preset_data.py` (JSON deltas, one INIT per plugin). Regenerate with:

```
python tools/build_bank.py
python tools/audit_bank.py
```

---

## 6. Public-repo test additions

Following the house rules (`#undef NDEBUG`, `assert`, `checkEditor`, Release-safe):

In each `plugins/<Name>/tests/PluginTests.cpp`:

1. `presetSeamUnavailable` - `assert (! SuitePresets::isAvailable())` and `assert (barHeight == 0)`
   in the public build (guard with `#if ! SUITE_PRO_PRESETS` so the same file compiles in the Pro
   repo).
2. `presetStateRoundTrip` - `auto tree = processor.getPresetStateTree().copyState();
   processor.applyPresetState (tree);` then assert every parameter value is unchanged. This is the
   regression guard for the section 3.1 refactor.
3. `setStateInformation` still restores `lerpCurve` (MTC) - extend the existing curve state test to go
   through `applyPresetState`.
4. `checkEditor` still paints with the bar present.

Non-negotiable regression: the embedded **MTC event-log table** (no-modulation baseline) must match
exactly. The refactor must not touch `processBlock` ordering except the panic check at the top, which
is a no-op unless the flag is set.

Test counts change by two new functions per plugin (`presetSeamUnavailable` and `presetStateRoundTrip`).
The third item extends an existing test and adds no function. Current counts (AGENTS.md section 6):
Spectral Drift 19, Dynamic Saturation 17, Micro-Timing Chaos 34. Expected after this plan: 21,
19 and 36. Update AGENTS.md section 6 and the README.

CTest entries: 14 — CoreDSPTests, SuiteState_Tests, SuitePresetsFile_Tests, SuiteModulation_Tests,
SuiteUI_Tests, the three plugin test executables, the three `*_PresetTests` executables, and the
three VST3 host-load tests. The factory bank (INIT-only) is always embedded, so there is no
separate public/bank split.

---

## 7. Pro-repo tests

`presets/tests/PresetTests.cpp`, one CTest entry per plugin (`SpectralDrift_PresetTests`, ...):

| Test | Assertion |
|---|---|
| `bankShape` | >= 25 presets, unique names, >= 3 categories, non-empty description, `plugin` matches |
| `bankRanges` | every parameter ID exists; every value within `NormalisableRange`; missing IDs flagged |
| `bankNotDefault` | no preset equals the default state |
| `applyMatchesFile` | for every preset: apply, then every parameter equals the file value |
| `applyResetsMissing` | apply a doctored preset missing one ID -> that parameter is back at its default |
| `applyTwiceIdempotent` | second apply changes nothing |
| `mtcCurveRestored` | a preset with a non-default `lerpCurve` restores it into `getLerpCurve()` |
| `fileRoundTrip` | write -> read -> apply in a temp dir; byte-stable metadata |
| `fileSanitising` | `a/b:c*?` -> legal filename; display name preserved |
| `audioSanity` (SD, DS) | 2 s pink noise + 1 kHz sine through every preset: all finite, peak <= 4.0, no NaN |
| `midiSanity` (MTC) | 4 bars internal clock per preset: every note-on has a note-off, offsets <= reported latency, no NaN |
| `editorWithBar` | `checkEditor` with Pro on; paint into a scoped `Graphics` (JUCE 9 gotcha) and assert non-zero pixels |
| `menuModel` | the menu model lists factory + user, grouped, with the current preset ticked |

CI: the preset suite runs in every build (`SUITE_BUILD_TESTS=ON`) as one `*_PresetTests`
executable per plugin. `python tools/audit_bank.py` runs in CI as part of the `preset-bank` job
in `.github/workflows/build.yml`.

---

## 8. Host program list (always compiled)

Program 0 is "Init" (every parameter at its default); programs 1..N are the factory presets in
bank order (`N == 1` here: the one `INIT` preset per plugin). `setCurrentProgram(i)` applies
preset i; `getCurrentProgram()` tracks the last applied index (-1 after any state change that
isn't a factory program). Bank order is append-only once hosts cache indices.

---

## 9. Risks and how the plan handles them

| Risk | Mitigation |
|---|---|
| **Preset change clicks (parameters jump)** | Accepted for v1 and documented. A 20 ms parameter ramp is a later, separate change; DS already smooths drive/mix/output. |
| MTC stuck notes on preset load | `panicRequested` flag, consumed at block start. Tested by `midiSanity`. |
| Host automation records a preset load | Standard behaviour; documented. Alternative (`replaceState` without notifications) breaks host UI sync, so not used. |
| Old preset missing new parameters | Explicit reset-to-default for absent IDs; tested by `applyResetsMissing`. |
| OneDrive-redirected Documents / unwritable folder | `Result`-based API, user-facing error, factory bank unaffected (embedded). |
| Bank grows and breaks host program indices | Append-only rule; the free bank is INIT-only today. |

---

## 10. Execution order

| Phase | Scope | Done when |
|---|---|---|
| 0 | Confirm GPL/pro distribution stance; decide where this plan lives | Decision recorded |
| 0a | R1 (done): state fixtures, `modules/suite_state`, processors use it | R1 done, fixtures still load |
| 1 | `StateHolder` + `applyPresetState` in all three processors; `setStateInformation` delegates to `SuiteState::read`; MTC panic flag | Full CTest green, MTC event log unchanged |
| 2 | `modules/suite_presets` seam + stub; editors reserve the bar | Full CTest green |
| 3 | Factory bank embedded (INIT-only), program list always compiled | Preset tests register unconditionally, CTest green |
| 4 | `.ausp` format, user save/load, menu, dirty marker, file tests | Round-trip + sanitising tests pass |
| 5 | Bank generator + audit + audio/MIDI sanity | Bank tests pass, all presets audible and in range |
| 6 | Docs: AGENTS.md sections 2/4.5/6/7/10/11/12, README one-liner, CHANGELOG | Docs match code |

Verification at every phase (full path, per the owner rules):

```powershell
$CM = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $CM -S . -B build -A x64 -DSUITE_BUILD_PLUGINS=ON
& $CM --build build --config Release --parallel 8
& "$(Split-Path $CM)\ctest.exe" --test-dir build -C Release --output-on-failure
```

---

## 11. Licence decision

GPLv3 distribution is settled (see AGENTS.md §10). The preset system itself adds
no parameters and ships in every build; the factory bank is INIT-only (free). No private overlay,
no Pro repo, no paid gating remains in the public repository.

---

## Revision notes (2026-11-08)

What changed, and why. Each item was checked against AGENTS.md or the pinned JUCE 9.0.5 source.

1. **Parameter counts.** Section 0 said 14 / 13 / 54. The current counts are 48 (Spectral Drift), 47 (Dynamic Saturation) and 54 (MTC), because the routing parameters were added. Corrected in section 0.
2. **Test counts.** Section 6 said 13 / 10 / 34 to 16 / 13 / 37. The current counts are 19 / 17 / 34 (AGENTS.md section 6), and the plan adds two new functions per plugin, so the expected counts are 21 / 19 / 36.
3. **Editor sizes and minimums.** Section 3.2 used sizes from before the MOD page. The code now gives SD 780 x 776 (minimum 640 x 656), DS 880 x 660 (minimum 720 x 656), and MTC 1695 x 1400 design size at 2/3 scale. The header bar fits the existing 40 px header, so the minimums do not need to grow. The MTC design height in AGENTS.md section 10 says 1380; the code says 1400. Flagged for the owner to reconcile.
4. **Factory presets.** Every value in the factory bank was checked by hand against the ranges in
   AGENTS.md section 4 and the target lists in section 4.4. All are in range. The `bankNotDefault`
   test enforces the not-default rule once it exists; the INIT preset is explicitly exempt. The
   `audit_bank.py` tool does the check automatically now.
5. **Version rule.** Section 2 and section 3.1 now use the state version from `modules/suite_state` (R1, done), with the same refusal rule and the same migration path for presets and sessions. A `resetMissing` option handles absent parameters for presets and INIT.
6. **Authoring source.** Section 1.1 said the `.ausp` files were the source of truth, but section 5 says the JSON deltas are. The repo layout now says JSON deltas.
7. **Program virtuals.** Phase 7 asked for these to be verified. All six are present in JUCE 9.0.5 (`juce_AudioProcessor.h`, lines 1131 to 1218).
8. **Execution order.** Added R1 phases 0 to 2 as a prerequisite, because the version rule depends on them.
9. **Public-repo risk.** Added a risk row and a warning at the top: this file describes the paid tier and must not go into a public commit. Not moved, because the location is the owner's decision.

Not changed: the GPL question in section 11 is still open, and it can change whether the plan goes ahead at all.

## Notes for the next agent

- The editor sizes, line numbers and test counts in section 3 and the revision notes were measured on 2026-11-08. Re-measure before you use them.
- Check `checkParameterIds` and `numControls` before and after each phase. The preset system must add no APVTS parameter.
- Section 11 lists the licence work that must be finished before a paid release.
