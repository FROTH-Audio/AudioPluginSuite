"""Audit the factory bank against the preset rules in `AGENTS.md` section 5.5 and
`modules/suite_presets/factory_bank/README.md`.

Checks the authoring data in `tools/preset_data.py` and, when they exist, the generated files
under `modules/suite_presets/factory_bank/bank/`. Exits non-zero on the first rule that is
broken, so it can be used in CI.

Rules:

- 26 / 26 / 28 / 20 / 15 / 15 / 15 presets (160 total, plan 11).
- Unique names per plugin, at least 3 categories per plugin, non-empty category and description.
- Every ID exists, every value is inside its `NormalisableRange`, values have the right type.
- No preset equals the default state.
- Spectral Drift: `freeze` only in the `Freeze` category.
- Dynamic Saturation: no `drive` 24 + `fold` 50 outside the `Extreme` category.
- Micro-Timing Chaos: host `sync` off in every factory preset and `bpm` left at 120, and the base
  pattern must fire at least one step (a low density with a short repeating pattern can be silent).
- No factory preset carries a file-path `samplePath`; only `embedded:` URIs are allowed
  (plan 07 Q3, revised 2026-11-16 in phase 4: the bank may now ship embedded sample
  variants, but never a machine-specific path) - checked in the authoring data, the
  generated XML attributes and the JSON files.
- Generated XML and JSON match the authoring data (metadata and every delta value).
"""

import argparse
import json
import math
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

from preset_data import PRESETS, SCHEMA

EXPECTED_COUNTS = {"SpectralDrift": 26, "DynamicSaturation": 26, "MicroTimingChaos": 28, "Sgraffito": 20,
                   "MidiEffect": 15, "StereoTool": 15, "MultibandSaturation": 15, "Cyclical": 15}
EXPECTED_TOTAL = 160

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_BANK = REPO_ROOT / "modules" / "suite_presets" / "factory_bank" / "bank"


class Audit:
    def __init__(self):
        self.errors = []

    def check(self, condition, message):
        if not condition:
            self.errors.append(message)

    def report(self):
        if not self.errors:
            print("Bank audit passed.")
            return True

        print(f"Bank audit failed with {len(self.errors)} problem(s):")
        for error in self.errors:
            print(f"  - {error}")
        return False


def as_number(spec, value):
    kind = spec["type"]
    if kind in ("int", "choice", "bool"):
        return int(round(float(value)))
    return float(value)


def mtc_hash_unit(seed, index):
    """Mirror of CoreDSP::Sequencer::hashUnit, so the audit sees the same pattern as the plugin."""
    x = (seed * 0x9E3779B9 + index * 0x85EBCA6B + 0x27D4EB2F) & 0xFFFFFFFF
    x ^= x >> 16
    x = (x * 0x7FEB352D) & 0xFFFFFFFF
    x ^= x >> 15
    x = (x * 0x846CA68B) & 0xFFFFFFFF
    x ^= x >> 16
    return (x >> 8) / 16777216.0


def mtc_density_curve(density):
    """Mirror of CoreDSP::Sequencer::densityCurve, for a density in [0, 1]."""
    return math.log1p(density * 0.99) / math.log1p(0.99) if density > 0.0 else 0.0


def mtc_preset_fires(params):
    """True when the base pattern (no modulation) fires at least one step.

    The pattern repeats every `length` steps, so a density below the lowest hash for that seed and
    length is silent forever, which would make the factory preset a dead entry in the menu.
    """
    seed = int(params.get("seed", SCHEMA["MicroTimingChaos"]["seed"]["default"]))
    length = int(params.get("length", SCHEMA["MicroTimingChaos"]["length"]["default"]))
    density = float(params.get("density", SCHEMA["MicroTimingChaos"]["density"]["default"]))
    threshold = mtc_density_curve(density / 100.0)

    return any(mtc_hash_unit(seed, index) < threshold for index in range(length))


def check_authoring(audit):
    total = 0

    for plugin, schema in SCHEMA.items():
        presets = PRESETS.get(plugin, [])
        total += len(presets)

        audit.check(len(presets) == EXPECTED_COUNTS.get(plugin, len(presets)),
                    f"{plugin}: expected {EXPECTED_COUNTS.get(plugin)} presets, found {len(presets)}")

        names = set()
        categories = set()

        for preset in presets:
            label = f"{plugin} / {preset.get('name', '<unnamed>')}"

            name = preset.get("name", "").strip()
            category = preset.get("category", "").strip()
            description = preset.get("description", "").strip()

            audit.check(bool(name), f"{label}: empty name")
            audit.check(bool(category), f"{label}: empty category")
            audit.check(bool(description), f"{label}: empty description")

            if name:
                audit.check(name not in names, f"{label}: duplicate preset name")
                names.add(name)
            if category:
                categories.add(category)

            params = preset.get("params", {})
            audit.check(bool(params), f"{label}: empty delta (equals the default state)")

            if "lerpCurve" in preset:
                audit.check(plugin == "MicroTimingChaos",
                            f"{label}: lerpCurve is only valid for MicroTimingChaos")
                points = preset["lerpCurve"].split(";")
                audit.check(len(points) == 5, f"{label}: lerpCurve needs 5 points")
                audit.check(points[0].startswith("0.0000,"), f"{label}: lerpCurve must start at x=0")
                audit.check(points[-1].startswith("1.0000,"), f"{label}: lerpCurve must end at x=1")

            # Plan 07 Q3, revised in phase 4: factory presets may reference embedded sample
            # variants, but never a file path. `samplePath` is a non-parameter key (a
            # property on <PARAMETERS>).
            sample_path = preset.get("samplePath")
            if sample_path is not None:
                audit.check(str(sample_path).startswith("embedded:"),
                            f"{label}: samplePath must be an embedded: URI, never a file "
                            f"path (plan 07 Q3 revised)")

            # Plan 6.1: Cyclical's four slot loops ride as state properties; only
            # embedded: URIs are allowed in the bank.
            loop_paths = preset.get("loopPaths")
            if loop_paths is not None:
                audit.check(len(loop_paths) == 4,
                            f"{label}: loopPaths needs exactly four entries")
                for index, path in enumerate(loop_paths):
                    audit.check(str(path).startswith("embedded:"),
                                f"{label}: loop{index + 1}Path must be an embedded: URI, "
                                f"never a file path (plan 6.1)")

            for pid, value in params.items():
                spec = schema.get(pid)
                audit.check(spec is not None, f"{label}: unknown parameter id {pid!r}")
                if spec is None:
                    continue

                number = as_number(spec, value)
                audit.check(spec["min"] <= number <= spec["max"],
                            f"{label}: {pid}={value} outside [{spec['min']}, {spec['max']}]")

                if spec["type"] in ("int", "choice", "bool"):
                    audit.check(float(number) == float(value),
                                f"{label}: {pid}={value} is not a whole number")

            if plugin == "SpectralDrift" and params.get("freeze") in (1, True):
                audit.check(category == "Freeze",
                            f"{label}: freeze is only allowed in the Freeze category")

            if plugin == "DynamicSaturation":
                drive = params.get("drive", schema["drive"]["default"])
                fold = params.get("fold", schema["fold"]["default"])
                if float(drive) == 24.0 and float(fold) == 50.0:
                    audit.check(category == "Extreme",
                                f"{label}: drive 24 + fold 50 is only allowed in Extreme")

            if plugin == "MicroTimingChaos":
                audit.check(int(params.get("sync", 0)) == 0, f"{label}: host sync must be off")
                audit.check(float(params.get("bpm", 120.0)) == 120.0,
                            f"{label}: bpm must stay at 120")
                audit.check(mtc_preset_fires(params),
                            f"{label}: the base pattern fires no steps (raise density or change the seed)")

        audit.check(len(categories) >= 3,
                    f"{plugin}: needs at least 3 categories, found {len(categories)}")

    audit.check(total == EXPECTED_TOTAL, f"expected {EXPECTED_TOTAL} presets, found {total}")


def load_generated_xml(bank_dir):
    xml_path = Path(bank_dir) / "AudioSuitePresets.xml"
    if not xml_path.is_file():
        return None

    return ET.parse(xml_path).getroot()


def check_generated(audit, bank_dir):
    root = load_generated_xml(bank_dir)

    if root is None:
        print(f"Note: {Path(bank_dir) / 'AudioSuitePresets.xml'} not found, skipping the generated-file check.")
        return

    audit.check(root.tag == "Bank", f"bank XML root is <{root.tag}>, expected <Bank>")

    generated = {}

    for element in root.findall("AudioSuitePreset"):
        plugin = element.get("plugin", "")
        meta = element.find("Meta")
        audit.check(meta is not None, f"bank XML: {plugin} preset has no <Meta>")
        if meta is None:
            continue

        name = meta.get("name", "")
        parameters = element.find("PARAMETERS")
        audit.check(parameters is not None, f"bank XML: {plugin} / {name} has no <PARAMETERS>")
        if parameters is None:
            continue

        params = {child.get("id"): child.get("value") for child in parameters.findall("PARAM")}
        generated.setdefault(plugin, {})[name] = {
            "category": meta.get("category", ""),
            "description": meta.get("description", ""),
            "params": params,
            "lerpCurve": parameters.get("lerpCurve"),
            "samplePath": parameters.get("samplePath"),
            "loopPaths": [parameters.get(f"loop{index}Path") for index in (1, 2, 3, 4)],
        }

    for plugin, presets in PRESETS.items():
        plugin_generated = generated.get(plugin, {})
        audit.check(len(plugin_generated) == len(presets),
                    f"bank XML: {plugin} has {len(plugin_generated)} presets, expected {len(presets)}")

        for preset in presets:
            name = preset["name"]
            entry = plugin_generated.get(name)
            audit.check(entry is not None, f"bank XML: {plugin} / {name} is missing")
            if entry is None:
                continue

            audit.check(entry["category"] == preset["category"],
                        f"bank XML: {plugin} / {name} category differs")
            audit.check(entry["description"] == preset["description"],
                        f"bank XML: {plugin} / {name} description differs")

            expected_ids = set(preset["params"])
            audit.check(set(entry["params"]) == expected_ids,
                        f"bank XML: {plugin} / {name} parameter set differs")

            audit.check(entry["lerpCurve"] == preset.get("lerpCurve"),
                        f"bank XML: {plugin} / {name} lerpCurve differs")

            # Plan 6.1: Cyclical's four loop paths must match the authoring data. Presets
            # without loop references carry no loop attributes at all.
            if "loopPaths" in preset:
                generated_paths = [path if path is not None else "" for path in entry["loopPaths"]]
                audit.check([str(path) for path in preset["loopPaths"]] == generated_paths,
                            f"bank XML: {plugin} / {name} loop paths differ")

            # Plan 07 Q3, revised in phase 4: the generated XML may carry an embedded:
            # URI, never a file path.
            generated_path = entry["samplePath"]
            audit.check(generated_path is None or str(generated_path).startswith("embedded:"),
                        f"bank XML: {plugin} / {name} carries a file-path samplePath "
                        f"(plan 07 Q3 revised)")

            for pid, value in preset["params"].items():
                spec = SCHEMA[plugin][pid]
                if spec["type"] in ("int", "choice", "bool"):
                    expected = str(int(round(float(value))))
                else:
                    expected = format(float(value), ".10g")

                audit.check(entry["params"].get(pid) == expected,
                            f"bank XML: {plugin} / {name} {pid} is {entry['params'].get(pid)!r}, expected {expected!r}")

        # JSON parity: one file per preset, metadata and delta identical to the authoring data.
        plugin_dir = Path(bank_dir) / plugin
        for preset in presets:
            slug = "".join(c if c.isalnum() else "_" for c in preset["name"]).strip("_").lower()
            json_path = plugin_dir / f"{slug}.json"
            audit.check(json_path.is_file(), f"bank JSON: {plugin} / {preset['name']} is missing ({json_path.name})")
            if not json_path.is_file():
                continue

            data = json.loads(json_path.read_text(encoding="utf-8"))
            audit.check(data.get("_name") == preset["name"], f"bank JSON: {plugin} / {preset['name']} name differs")
            audit.check(data.get("_category") == preset["category"],
                        f"bank JSON: {plugin} / {preset['name']} category differs")
            audit.check(data.get("_description") == preset["description"],
                        f"bank JSON: {plugin} / {preset['name']} description differs")

            delta = {key: value for key, value in data.items() if not key.startswith("_")}
            audit.check(set(delta) == set(preset["params"]),
                        f"bank JSON: {plugin} / {preset['name']} parameter set differs")
            audit.check(data.get("_lerpCurve") == preset.get("lerpCurve"),
                        f"bank JSON: {plugin} / {preset['name']} lerpCurve differs")

            # Plan 6.1: the four loop paths must match the authoring data.
            if "loopPaths" in preset:
                json_paths = data.get("_loopPaths", [])
                audit.check([str(path) for path in json_paths]
                                == [str(path) for path in preset["loopPaths"]],
                            f"bank JSON: {plugin} / {preset['name']} loop paths differ")

            # Plan 07 Q3, revised in phase 4: the JSON may carry an embedded: URI,
            # never a file path.
            audit.check(data.get("_samplePath") is None
                        or str(data.get("_samplePath")).startswith("embedded:"),
                        f"bank JSON: {plugin} / {preset['name']} carries a file-path "
                        f"_samplePath (plan 07 Q3 revised)")

            for pid, value in preset["params"].items():
                audit.check(float(delta.get(pid, float("nan"))) == float(value),
                            f"bank JSON: {plugin} / {preset['name']} {pid} differs")


def live_parameter_ids(plugin):
    """The live parameter IDs a plugin exposes, parsed from its sources (plan 05.2 F1.1).

    The IDs are collected two ways and unioned: the `const char*` constants in the processor
    header, and every `getRawParameterValue (...)` argument in the processor source. Array
    arguments (`modSourceIds[i]`) contribute all of the array's elements, which is right for
    a coverage check because every element is a real parameter. This is a parsing
    approximation, not a C++ parser: a parameter that is neither a named constant nor cached
    would be missed, so a miss here must be checked by hand before trusting the verdict.
    """
    root = Path(__file__).resolve().parent.parent
    plugin_dir = root / "plugins" / plugin

    header = (plugin_dir / "Source" / "PluginProcessor.h")
    source = (plugin_dir / "Source" / "PluginProcessor.cpp")

    constants = {}
    if header.is_file():
        text = header.read_text(encoding="utf-8", errors="replace")

        for match in re.finditer(r'const char\*\s*(\w+)\s*=\s*"([^"]+)"', text):
            constants[match.group(1)] = match.group(2)

        for match in re.finditer(r'const char\*\s*(\w+)\s*\[\s*\d+\s*\]\s*=\s*\{([^}]*)\}', text):
            elements = re.findall(r'"([^"]+)"', match.group(2))
            for index, element in enumerate(elements):
                constants[f"{match.group(1)}[{index}]"] = element
                constants[match.group(1)] = constants.get(match.group(1), element)

    ids = set()

    if source.is_file():
        text = source.read_text(encoding="utf-8", errors="replace")

        for match in re.finditer(r'getRawParameterValue\s*\(([^)]+)\)', text):
            argument = match.group(1).strip()

            if "[" in argument:
                array_name = argument.split("[")[0].strip()
                ids.update(v for k, v in constants.items() if k == array_name or k.startswith(array_name + "["))
            elif argument in constants:
                ids.add(constants[argument])

    return ids


ROUTER_PARAMETER_IDS = (
    [f"lfo{index}{suffix}" for index in (1, 2)
     for suffix in ("Rate", "Shape", "Sync", "Division", "Offset")]
    + [f"mod{slot}{suffix}" for slot in range(1, 7)
       for suffix in ("Source", "Target", "Amount", "Delay")]
)


def plugin_uses_router(plugin):
    """True when the plugin adds the shared LFO/routing parameters (Router::addParameters)."""
    plugin_dir = Path(__file__).resolve().parent.parent / "plugins" / plugin

    for name in ("Source/PluginProcessor.cpp", "Source/PluginProcessor.h"):
        path = plugin_dir / name
        if path.is_file() and "Router::addParameters" in path.read_text(encoding="utf-8", errors="replace"):
            return True

    return False


def live_num_controls(plugin):
    """The plugin's numControls constant, or None when the header does not declare one."""
    header = Path(__file__).resolve().parent.parent / "plugins" / plugin / "Source" / "PluginProcessor.h"

    if not header.is_file():
        return None

    match = re.search(r'numControls\s*=\s*(\d+)', header.read_text(encoding="utf-8", errors="replace"))
    return int(match.group(1)) if match else None


def check_schema_covers_live(audit):
    """Plan 05.2 F1.1: the authoring schema must know every parameter the plugins expose.

    A parameter that is live but missing from `SCHEMA` silently drops out of preset
    authoring (resetMissing keeps presets loadable, so nothing complains). Catches the
    drift that produced finding F1.4 (DS `pan` implemented, schema without it)."""
    for plugin, schema in SCHEMA.items():
        live = live_parameter_ids(plugin)

        # The shared LFO/routing parameters are added by SuiteModulation::Router and read
        # inside the module, not the plugin's own sources; they are live when the plugin
        # adds them (plan 05.2 F1.1).
        if plugin_uses_router(plugin):
            live = live | set(ROUTER_PARAMETER_IDS)

        schema_ids = set(schema.keys())

        if not live:
            audit.check(False, f"{plugin}: no live parameter IDs could be parsed; the coverage check needs a manual look")
            continue

        missing = sorted(live - schema_ids)
        unknown = sorted(schema_ids - live)

        audit.check(not missing,
                    f"{plugin}: live parameter(s) missing from the authoring schema: {', '.join(missing)}")
        audit.check(not unknown,
                    f"{plugin}: schema key(s) with no live parameter: {', '.join(unknown)}")

        num_controls = live_num_controls(plugin)
        if num_controls is not None:
            audit.check(len(live) == num_controls,
                        f"{plugin}: parsed {len(live)} live parameter IDs, but numControls says {num_controls}")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bank", default=str(DEFAULT_BANK),
                        help=f"generated bank directory (default: {DEFAULT_BANK})")
    args = parser.parse_args(argv)

    audit = Audit()
    check_authoring(audit)
    check_generated(audit, args.bank)
    check_schema_covers_live(audit)

    return 0 if audit.report() else 1


if __name__ == "__main__":
    sys.exit(main())
