# Factory bank — optional embedded presets

This directory holds the **optional** embedded factory bank for the AudioSuite preset system.
The preset bar, the `.ausp` file I/O and the menu model are public and live in
`modules/suite_presets` (`include/`, `Source/`, `tests/`); see `AGENTS.md` §5.5. Only the bank is
gated: it is compiled in under `SUITE_PRESET_BANK`, and without that flag `Source/PresetBank.cpp`
degrades to an empty bank, so the same sources build with or without it.

## Layout

```
factory_bank/
  include/PresetBank.h          PresetBank (getFactoryPresets, parseDelta) and PresetPrograms
  Source/PresetBank.cpp         Bank parse (empty bank under SUITE_PRESET_BANK=0) and program list
  bank/AudioSuitePresets.xml    The embedded bank payload: 80 deltas (26 SD / 26 DS / 28 MTC)
  bank/<plugin>/<slug>.json     One delta JSON per factory preset (authoring source)
  tests/PresetTestUtils.h       Shared runBank*/runApply*/runProgramList/runMenuModel helpers
  tests/<Name>PresetTests.cpp   Per-plugin bank tests, registered by suite_add_plugin
```

## How it builds

`juce_add_binary_data(SuitePresetBankData)` is the one real target here. It is added **before**
`modules/suite_presets` in the root `CMakeLists.txt`, because the always-on `SuitePresetsFileTests`
executable links it when the bank is on.

`suite_presets_add_to_target(<target>)` in `modules/suite_presets/SuitePresets.cmake` compiles
`PresetBank.cpp` (together with the public `PresetBar.cpp` and `PresetFile.cpp`) into each consumer:

- `SUITE_PRESET_BANK=OFF`: compiles with `SUITE_PRESET_BANK=0`; the bank accessors see an empty
  list, `PresetBar::getNumFactoryPresets()` returns 0, and each plugin reports one default program.
- `SUITE_PRESET_BANK=ON`: defines `SUITE_PRESET_BANK=1` and links `SuitePresetBankData`.

There is no static library — like `suite_ui` and `suite_modulation`, the sources compile directly
into each consumer to avoid duplicate-symbol link errors with JUCE. The data target namespace is
`SuitePresetBankData` (not `BinaryData`) so it does not collide with each plugin's
`<Name>Assets` binary-data target.

## Delivery model (owner decision 2026-11-09, hybrid)

- The preset system (browser, file I/O) is **free** and ships in every build.
- The **embedded bank is the free subset**: a small selection of factory presets compiled into the
  binaries so every user sees the feature work. The subset selection is still an owner decision.
- The **full set is sold as paid `.ausp` data files**: users drop them into
  `Documents/AudioSuite/<plugin>/Presets` and the public bar picks them up. Paid content is data,
  needs no build, and does not change the bank order.

## Factory bank content

The bank is the authoring source of truth for all 80 presets, whether or not each one ships in the
free subset. Each preset is a **delta** from the default state: only the parameters it changes are
listed, and `applyPresetState()` resets absent IDs to their defaults at load time. This keeps the
embedded asset small and survives a parameter being added later.

The authoring source is `tools/preset_data.py` (JSON deltas, 26 SD / 26 DS / 28 MTC). Regenerate
with:

```
python tools/build_bank.py
python tools/audit_bank.py
```

`audit_bank.py` enforces: unique names per plugin, ≥3 categories, every value inside the
parameter's `NormalisableRange`, no preset equal to the default state, Spectral Drift `freeze` only
in `Freeze`, Dynamic Saturation `drive` 24 + `fold` 50 only in `Extreme`, Micro-Timing Chaos host
`sync` off and `bpm` 120 in every factory preset, and Micro-Timing Chaos base patterns that fire at
least one step.

## Host program list (`SUITE_PRESET_BANK_AS_PROGRAMS`)

Behind a separate, revertable flag that requires the bank (the root `CMakeLists.txt` refuses it
otherwise): program 0 is "Init" (every parameter at its default), programs 1..N are the factory
presets in bank order. `setCurrentProgram(i)` applies preset i; `getCurrentProgram()` tracks the
last applied index (-1 after any state change that isn't a factory program). Bank order is
append-only once hosts cache indices, so the flag is separate from `SUITE_PRESET_BANK`.

## Licence

GPL v3, same as the rest of the suite (code). The sold presets are separately licensed data — see
`docs/plans/03_presets.md` §11 and the licence decision of 2026-11-08 (commercial JUCE licence plus
relicensing; terms, cost and copyright holders still open). Note the repository-level caveat in
`AGENTS.md` §10: closed-source distribution would need a commercial JUCE licence.
