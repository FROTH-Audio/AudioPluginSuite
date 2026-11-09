# Pro presets — AudioSuite Preset System

This directory holds the private Pro overlay for the AudioSuite preset system. It is gated behind the `SUITE_PRO_PRESETS` CMake option and is **not** compiled into public builds.

## Layout

```
pro_preset_bank/
  include/SuitePresetsPro.h     PresetBank, PresetMenuModel, PresetPrograms API
  Source/PresetBank.cpp         Factory-bank parsing, PresetPrograms host-program list
  Source/PresetBar.cpp          Header bar UI: factory presets, INIT, SAVE, menu
  Source/PresetFile.cpp         .ausp read/write, user folder, sanitising
  bank/AudioSuitePresets.xml    Embedded factory bank: 80 presets (26 SD / 26 DS / 28 MTC)
  bank/<plugin>/<slug>.json     One delta JSON per factory preset (authoring source)
  tests/                        Per-plugin preset tests + PresetFile tests
```

## How it builds

`suite_presets_add_to_target(<target>)` in `modules/suite_presets/SuitePresets.cmake`:

- Public build (`SUITE_PRO_PRESETS=OFF`): compiles `Source/SuitePresetsStub.cpp` into each consumer. `PresetBar` reports height 0, `isAvailable()` is false, `getNumPrograms()` returns 1 — the editor layout is byte-identical to before presets.
- Pro build (`SUITE_PRO_PRESETS=ON`): compiles `Source/PresetBank.cpp`, `PresetBar.cpp`, `PresetFile.cpp` into each consumer and links the embedded `SuiteProPresetBankData` binary-data target (`bank/AudioSuitePresets.xml`).

There is no static library — like `suite_ui` and `suite_modulation`, the Pro sources compile directly into each consumer to avoid duplicate-symbol link errors with JUCE.

## Preset file format (`.ausp`)

UTF-8 XML, no compression:

```xml
<AudioSuitePreset formatVersion="1" plugin="MicroTimingChaos" pluginVersion="1.0.0">
  <Meta name="Slow Orbit" category="Ambient" author="FROTH Audio"
        description="Wide, slow drift with a lazy gate."/>
  <PARAMETERS suiteStateVersion="1">
    <PARAM id="chaos" value="0.62"/>
    ...
  </PARAMETERS>
</AudioSuitePreset>
```

- `<PARAMETERS>` is stored verbatim, so `ValueTree::fromXml` feeds `replaceState` directly.
- `plugin` must match `getPresetPluginId()`; a mismatch is refused.
- Two version fields: `formatVersion` (the preset file format) and `suiteStateVersion` on `<PARAMETERS>` (the same attribute sessions use). A preset saved with a newer plugin version is refused.

User presets are written to `Documents/AudioSuite/<plugin>/Presets` (Windows) or the platform equivalent, with sanitised file names. The folder is created lazily — scanning an unused plugin never writes into Documents.

## Factory bank

80 factory presets are embedded as one `bank/AudioSuitePresets.xml`. Each preset is a **delta** from the default state: only the parameters it changes are listed, and `applyPresetState()` resets absent IDs to their defaults at load time. This keeps the embedded asset small and survives a parameter being added later.

The authoring source is `tools/preset_data.py` (JSON deltas, 26 SD / 26 DS / 28 MTC). Regenerate with:

```
python tools/build_bank.py
python tools/audit_bank.py
```

`audit_bank.py` enforces: unique names per plugin, ≥3 categories, every value inside the parameter's `NormalisableRange`, no preset equal to the default state, Spectral Drift `freeze` only in `Freeze`, Dynamic Saturation `drive` 24 + `fold` 50 only in `Extreme`, Micro-Timing Chaos host `sync` off and `bpm` 120 in every factory preset, and Micro-Timing Chaos base patterns that fire at least one step.

## Host program list (`SUITE_PRO_PRESETS_AS_PROGRAMS`)

Behind a separate, revertable flag: program 0 is "Init" (every parameter at its default), programs 1..N are the factory presets in bank order. `setCurrentProgram(i)` applies preset i; `getCurrentProgram()` tracks the last applied index (-1 after any state change that isn't a factory program). Bank order is append-only once hosts cache indices, so this flag is separate from `SUITE_PRO_PRESETS`.

## Licence

GPL v3, same as the rest of the suite. Linking GPL v3 code with non-GPL code and distributing the binary requires the whole to be GPL — see `docs/plans/03_presets.md` §11 (owner decision pending).
