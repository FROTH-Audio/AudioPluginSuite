# Factory bank — embedded presets (free, INIT-only)

This directory holds the **embedded factory bank** for the AudioSuite preset system.
The preset bar, the `.ausp` file I/O and the menu model are public and live in
`modules/suite_presets` (`include/`, `Source/`, `tests/`); see `AGENTS.md` §5.5.
The bank itself is always compiled in (plan 03.3): it carries exactly one `INIT`
preset per plugin — the default state — and nothing else. No Pro overlay, no paid tier.

## Layout

```
factory_bank/
  include/PresetBank.h          PresetBank (getFactoryPresets, parseDelta) and PresetPrograms
  Source/PresetBank.cpp         Bank parse (INIT-only bank) and program list
  bank/AudioSuitePresets.xml    The embedded bank payload: 3 INIT deltas (1 per plugin)
  bank/<plugin>/<slug>.json     One delta JSON per factory preset (authoring source)
  tests/PresetTestUtils.h       Shared runBank*/runApply*/runProgramList/runMenuModel helpers
  tests/<Name>PresetTests.cpp   Per-plugin bank tests, registered by suite_add_plugin
```

## How it builds

`juce_add_binary_data(SuitePresetBankData)` is the one real target here. It is added
unconditionally in the root `CMakeLists.txt`, because the tests link it in every build.

`suite_presets_add_to_target(<target>)` in `modules/suite_presets/SuitePresets.cmake` compiles
`PresetBank.cpp` (together with the public `PresetBar.cpp` and `PresetFile.cpp`) into each
consumer, always defining `SUITE_PRESET_BANK=1` and linking `SuitePresetBankData`.

There is no static library — like `suite_ui` and `suite_modulation`, the sources compile directly
into each consumer to avoid duplicate-symbol link errors with JUCE. The data target namespace is
`SuitePresetBankData` (not `BinaryData`) so it does not collide with each plugin's
`<Name>Assets` binary-data target.

## Factory bank content

The bank carries one `INIT` preset per plugin (name `INIT`, category `Utility`, description
`The default state.`), with an empty delta — every parameter stays at its default. It is exempt
from the not-default audit rule.

The authoring source is `tools/preset_data.py` (JSON deltas, one INIT per plugin). Regenerate
with:

```
python tools/build_bank.py
python tools/audit_bank.py
```

`audit_bank.py` enforces: unique names per plugin, at least one category per plugin, every value
inside the parameter's `NormalisableRange`, Micro-Timing Chaos host `sync` off and `bpm` 120 in
every factory preset, and Micro-Timing Chaos base patterns that fire at least one step.

## Host program list

Program 0 is "Init" (every parameter at its default) and program 1 is the plugin's `INIT`
preset; `getNumPrograms() == 2`. The list is always compiled. `setCurrentProgram(i)` applies
preset i; `getCurrentProgram()` tracks the last applied index (-1 after any state change that
isn't a factory program). Bank order is append-only once hosts cache indices.

## Licence

GPL v3, same as the rest of the suite (code). The licence stays GPL (2026-11-12); the factory
presets are now publicly available.
