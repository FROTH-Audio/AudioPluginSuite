# Plan 08 — Subtle preset bar, plugin settings, About dialog

Status: **Started 2026-11-16. Phase 0 (verification + baseline renders) is next.** No code written yet.

Owner brief (verbatim): "#1 Re-Work the Preset-Bar design. Make it more subtle. #2 Make the
Plugin-Settings accessible. #3 Add a 'About' Dialog to every plugin".

---

## 1. Goal

Three independent deliverables that share one surface (the plugin header):

1. **Ghost preset bar** — the five header buttons (‹ name ›, INIT, SAVE) lose their accent
   fill; they become outlined ghost buttons with dim text and accent on hover. The bar stays
   in place, same height, same behaviour, same menus.
2. **Plugin settings** — a gear button in the header opens a small dialog with four settings:
   tooltips on/off, no-playhead fallback tempo, user preset folder (per plugin), and Sgraffito's
   sample search folder. Nothing here is an APVTS parameter; nothing joins saved sessions.
3. **About dialog** — every plugin gets an About entry (in the same gear menu) showing the
   brand lockup, product name + version, FROTH Audio, plugin code, JUCE version, and the
   GPL v3 note.

## 2. Owner decisions (2026-11-16)

- **Q1 — preset-bar subtlety**: **Ghost buttons.** Keep all five buttons; remove the fill,
  draw a hairline outline, dim text, accent on hover/press. Not text-only, not name-only.
- **Q2 — settings scope**: **Core four.** Tooltips on/off, fallback tempo, user preset
  folder, Sgraffito sample search folder. No window-scale setting.
- **Q3 — trigger placement**: **One gear menu.** A single header button opens a popup with
  "Settings…" and "About…". No separate About button.
- **Q4 — gear position** (proposed default, confirm at implementation): the gear sits at the
  far right of the header, right of the preset bar, in all four editors. Sgraffito keeps
  LOAD SOUND left of the bar; the order there becomes `LOAD SOUND | preset bar | gear`.
- **Q5 — settings storage** (proposed default, confirm at implementation): one shared
  `juce::PropertiesFile` at `%APPDATA%/FROTH Audio/SuiteSettings.xml` (platform equivalent),
  keys namespaced per plugin (`presets/<pluginId>/folder`). One file, shared by all plugins,
  read and written on the message thread only.

## 3. Phases

### Phase 0 — verify claims, baseline renders (no functional change)
- Confirm from `build/_deps/juce-src` (grep, never guess): `TextButton::paintButton` /
  `LookAndFeel_V4::drawButtonBackground` outline behaviour (decides whether `GhostButton`
  must override `paintButton` or can just set colours); `TooltipWindow` API (whether
  `getTipFor` can be gated or the window is created/destroyed on toggle).
- Confirm whether the plugin targets actually get `JucePlugin_VersionString` in this JUCE 9
  setup (AGENTS §14 says no `JucePluginDefines.h`). If absent, phase 3 adds our own
  `SUITE_PLUGIN_VERSION` / `SUITE_PLUGIN_CODE` compile definitions in `SuitePlugin.cmake`.
- Confirm `suite_presets` stays independent of `suite_ui` (PresetBar includes only
  `SuitePresets.h` + `PresetBank.h`). The ghost restyle must keep it that way — no SuiteUI
  dependency added to suite_presets.
- Record the exact call sites: `getUserPresetFolder` (PresetFile.cpp write, PresetBar save +
  menu + scan), `hostTempoOrDefault` (SD 276, DS 323, Sgraffito 300/634 — the fallback
  parameter already exists), `sampleSearchFolder` (Sgraffito header member, used at
  PluginProcessor.cpp 1247).
- **Baseline renders** under `build/renders/plan08-before/` (rule R9): one page per editor
  plus Sgraffito's three pages, with the header in view.

### Phase 1 — `modules/suite_settings` (store + API + tests)
New module, same compile-into-consumer pattern as `suite_ui`/`suite_modulation` (AGENTS §8 —
no static library, it links JUCE). Contents:

- `include/SuiteSettings.h` + `Source/SuiteSettings.cpp`:
  - `load(File)`, `save()`, `getBool/getDouble/getString(key, default)`, `set(...)`.
  - Message-thread only; audio consumers receive values pushed into atomics at call sites.
  - Public file-location override (like Sgraffito's `sampleSearchFolder`) so **tests never
    touch the real user folder**. A missing or corrupt file falls back to defaults.
  - Keys: `ui/tooltips` (default on), `tempo/fallbackBpm` (default 120, clamped 20–300),
    `presets/<pluginId>/folder` (empty = default), `sgraffito/sampleFolder` (empty = default).
- `suite_settings_add_to_target(<target>)` in `SuiteSettings.cmake`; root `CMakeLists.txt`
  includes it before the plugin subdirectories; `suite_add_plugin` gains a `SETTINGS` flag
  that adds it to the plugin **and its test executable**.
- Tests: `modules/suite_settings/tests/SuiteSettingsTests.cpp` (own executable, registered
  like suite_modulation's, only with `SUITE_BUILD_TESTS` on). Round trip, defaults on
  missing file, clamp, per-plugin keys, corrupt file ignored.

### Phase 2 — ghost preset bar (task #1, suite_presets only)
- Private `GhostButton : juce::TextButton` in `PresetBar.cpp`: transparent fill, hairline
  rounded outline (accent ~25% alpha, radius 6 — the suite's `controlRadius` value, local
  constant to keep suite_presets SuiteUI-free), text at reduced alpha, accent border +
  full-brightness text on hover/press. The preset-name button keeps slightly stronger
  presence; ‹ › and INIT/SAVE stay dimmer.
- Bar height, buttons, menus, tooltips, INIT semantics: all unchanged. Colours move from
  `Colours::white` to dimmed white.
- **After renders** under `build/renders/plan08-after/` compared against the phase-0
  baseline; both sets stay under `build/`.
- The existing `editorWithBar` preset tests must pass unchanged (component tree is the same).

### Phase 3 — gear button + About dialog (tasks #2 trigger + #3, suite_ui + SuitePlugin.cmake)
- `SuiteUI::HeaderMenuButton` (suite_ui): ~24 px gear drawn with a `juce::Path` (no new
  asset), hover accent, opens a `PopupMenu` with "Settings…" and "About…"; public
  `onSettings` / `onAbout` callbacks. Follows R1–R10 (this is not a knob or a pill; it is a
  header icon button — record it as a small addition to the AGENTS §7 rule set).
- `SuiteUI::showAboutDialog(...)`: a modal `DialogWindow` with the per-plugin lockup image,
  product name, version (from the phase-0-confirmed version macro), "FROTH Audio", plugin
  code, JUCE version (`JUCE_VERSION`), and the GPL v3 note. Non-resizable, ~380×220, slate
  palette, Bahnschrift typography (R5).
- `SuitePlugin.cmake`: add `SUITE_PLUGIN_VERSION="${SAP_VERSION}"` and
  `SUITE_PLUGIN_CODE="${PLUGIN_CODE}"` compile definitions to plugin + test targets (only if
  phase 0 confirmed `JucePlugin_VersionString` is missing).
- All four editors place the gear in the header (Q4 position), sized to the header band,
  never overlapping the preset bar or (Sgraffito) the LOAD SOUND button.

### Phase 4 — settings dialog + plumbing (task #2)
- `SuiteSettings::SettingsDialog` (suite_settings, so it can own the store): rows for
  - tooltips on/off (a R6-style switch with state dot),
  - fallback tempo (number box, 20–300, host-sync behaviour documented in the row caption),
  - user preset folder (path + Browse… + Reset to default) — the Browse `FileChooser` is a
    dialog member, the Sgraffito LOAD SOUND pattern (deleting the chooser aborts the dialog),
  - Sgraffito's sample search folder (same row kind; only passed by Sgraffito).
- Changes apply live and save; a "Reset" per row restores the default.
- Plumbing:
  - `PresetFile::getUserPresetFolder` consults the override key once per call (all PresetBar
    call sites go through it — one change point).
  - SD/DS/Sgraffito pass the stored fallback tempo to the existing
    `hostTempoOrDefault(playHead, fallback)` parameter.
  - Sgraffito sets `sampleSearchFolder` from the setting.
  - Each editor creates one `juce::TooltipWindow` gated by `ui/tooltips` (phase 0 decides
    create/destroy vs. gate).
- Nothing joins APVTS or saved sessions (the lerpCurve precedent — settings are not
  parameters). `numControls` and `checkParameterIds` are unchanged.

### Phase 5 — docs + verification
- Docs: plan index row in `docs/plans/README.md`; AGENTS.md §7 (new header-button rule +
  ghost-bar styling), §5.5 (bar styling note), §14; user README (settings + About);
  `CHANGELOG.md`.
- Full post-change checks (AGENTS §15): clean Release build, warning count vs. the 9
  pre-existing `-Winconsistent-missing-override` sites, full CTest, before/after renders on
  record. Report anything not verified (macOS/AU, DAW).

## 4. Tests

- `SuiteSettingsTests` (new executable): store round trip, defaults, clamp, corrupt file,
  redirect-to-temp override. **CTest entry count changes: 11 → 12 default, 15 → 16 with the
  bank on. Report it.**
- `SuiteUITests` additions: `HeaderMenuButton` paints + its menu contains both items
  (`MenuItemIterator(..., true)` — `containsCommandItem` does not recurse); `AboutPanel`
  paints lockup and texts into an image (scope the `Graphics` before reading pixels — AGENTS
  §11).
- Plugin tests: gear button present in the header component tree; About dialog opens, paints
  and closes (the `JUCE_MODAL_LOOPS_PERMITTED` pump pattern already in use); settings dialog
  opens, toggles a setting, value persists in the redirected store.
- Preset tests: `editorWithBar` unchanged (same tree); ghost style is a paint-only change.
- Editor renders: full per-page set before/after (R9); no parameter changes, so
  `checkParameterIds` counts are untouched.

## 5. Pitfalls

- **Never link suite_settings as a library** (§8 duplicate symbols). Same for anything new.
- **Tests must redirect the settings file**; a test that writes `%APPDATA%` is a bug.
- **suite_presets must not gain a suite_ui dependency** for the ghost style; use local
  constants.
- JUCE 9: `Graphics` must be out of scope before reading image pixels; `getenv`-style
  deprecations show up in a clean build only.
- The settings dialog's `FileChooser` must be a member (stack-local choosers never show).
- `hostTempoOrDefault`'s fallback only matters when no host tempo is reported; while Host
  Sync is on with a tempo, the setting is inert — document that in the row caption.
- Default build (`SUITE_PRESET_BANK=OFF`) has an empty factory list: the gear, settings and
  About must work there too, and the ghost bar must not regress the default build's layout
  (the stub bar reports height 0 in builds without presets — keep that).
- Do not stop or start CMake processes the IDE may own; drive long builds in resumable
  chunks (the 120 s shell cap).
