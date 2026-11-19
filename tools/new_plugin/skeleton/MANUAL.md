# __PRODUCT__ — User Manual

> **Stub.** Rule R11 in `AGENTS.md` §7: a plugin is not "done" until this manual is written and
> every page and mode has a full-page screenshot committed under `docs/manuals/`. Replace the
> placeholder text below; keep the image names and the two-thirds display width.

The plugin code is `__CODE__`. The bundle id is `com.audiosuite.__LOWERCASENAME__`.

## The interface at a glance

<img src="../../docs/manuals/__LOWERCASENAME__-slots.png" alt="__PRODUCT__ SLOTS page" width="653">

One paragraph per page: what the user sees, what each group does, and how the page switch (R8,
top right of the header) gets there. The preset bar (R12) and the hamburger menu (R13) sit in the
header; say so once, then point at the sections below.

## Page: SLOTS

<img src="../../docs/manuals/__LOWERCASENAME__-slots.png" alt="SLOTS page" width="653">

Walk through every control, left to right, top to bottom: caption, value range, what changes in
the sound, and the one mistake users make. Mention the help line in the footer (`Hover a control
to see what it does.`).

<img src="../../docs/manuals/__LOWERCASENAME__-detail-<name>.png" alt="Detail crop" width="573">

Detail crops are cut from the full-page render with `tools/manuals/crops.py`; use them where a
control's effect needs a close-up.

## Page: GLOBAL

<img src="../../docs/manuals/__LOWERCASENAME__-global.png" alt="GLOBAL page" width="653">

Same walkthrough for the second page. If a control only works in one mode, say which mode and
that it is dimmed (never hidden) elsewhere — the affinity rule, plan 10 G1.

## Settings and About

The hamburger button at the far right of the header opens one menu (plan 08):

- **Settings…** — tooltips, the no-playhead fallback tempo, and this plugin's user preset folder.
- **About…** — brand lockup, product name and version, FROTH Audio, the plugin code, the JUCE
  version and the GPL v3 note.

## Factory presets

List the categories and one sentence per preset, or a table: name, category, what it shows.
The bank order is append-only, so the table order matches `tools/preset_data.py`.

## MIDI / host notes

Fixed note map (if any), latency in samples and milliseconds, tail length, and anything the host
must do (arm the track, hold a note, enable the sidechain bus).

---

## Screenshots (mandatory — R11)

Full-page images, one per page **and per mode**, named `<plugin>-<page>.png`; detail crops named
`<plugin>-detail-<name>.png`. All of them are committed under `docs/manuals/`.

1. Run the plugin tests with the render output pointed at the manual folder:

   ```powershell
   $env:SUITE_RENDER_DIR = "C:\Users\tfroe\Documents\PROJECTS\AudioPluginSuite\docs\manuals"
   .\build\plugins\__NAME__\Release\__NAME__Tests.exe
   ```

   The test writes every view and fails if two renders of the same view differ.

2. Cut detail images with `tools/manuals/crops.py`.

3. Insert the README screenshot with `tools/manuals/scale_images.py` (it converts the `<img>`
   tags to about two-thirds of the native width).

4. Re-run the renders whenever the UI changes, and update the manual in the same change.
