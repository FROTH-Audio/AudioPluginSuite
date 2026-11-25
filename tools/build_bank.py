"""Generate the factory bank from `tools/preset_data.py`.

Writes, for every preset:

    modules/suite_presets/factory_bank/bank/<plugin>/<slug>.json
    modules/suite_presets/factory_bank/bank/AudioSuitePresets.xml

Each preset is stored as a delta from the default state, both in the JSON files and in the
`<PARAMETERS>` element of the bank XML. `applyPresetState()` resets every absent ID to its
default, so a delta materialises the full state at load time. That is the one place this
deviates from the full-explicit-state design recorded in `AGENTS.md` section 5.5; deltas keep the
embedded asset small and survive a parameter being added.

Run from the repository root:

    python tools/build_bank.py
    python tools/audit_bank.py
"""

import argparse
import json
import re
import sys
from pathlib import Path
from xml.sax.saxutils import quoteattr

from preset_data import PRESETS, SCHEMA

PLUGIN_VERSION = "1.0.0"
STATE_VERSION = 1
# Spectral Drift is at state version 2 since plan 05.1 pass 2 (the Drift Rate parameter was
# removed and the Drift routing target appended); its presets are authored against the new
# routing order. MIDI Chord Arp Scale Tool is at version 2 since plan 12 (Step Rate grew
# from five to twelve values in a clean order, so the authoring indices are the new ones).
# The other plugins are still at version 1.
STATE_VERSIONS = {"SpectralDrift": 2, "MidiEffect": 2}


def state_version_for(plugin):
    return STATE_VERSIONS.get(plugin, STATE_VERSION)
AUTHOR = "FROTH Audio"

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_OUT = REPO_ROOT / "modules" / "suite_presets" / "factory_bank" / "bank"


def slugify(name):
    slug = re.sub(r"[^A-Za-z0-9]+", "_", name).strip("_").lower()
    return slug or "preset"


def format_value(spec, value):
    kind = spec["type"]

    if kind in ("int", "choice", "bool"):
        return str(int(round(float(value))))

    return format(float(value), ".10g")


def parameter_order(plugin):
    """Schema insertion order, so the generated files are deterministic."""
    return list(SCHEMA[plugin].keys())


def preset_json(plugin, preset):
    data = {
        "_name": preset["name"],
        "_category": preset["category"],
        "_description": preset["description"],
        "_plugin": plugin,
        "_pluginVersion": PLUGIN_VERSION,
    }

    if "lerpCurve" in preset:
        data["_lerpCurve"] = preset["lerpCurve"]

    if "samplePath" in preset:
        data["_samplePath"] = preset["samplePath"]

    # Cyclical (plan 6.1): the four slot loops ride as state properties.
    if "loopPaths" in preset:
        data["_loopPaths"] = preset["loopPaths"]

    # Keep the schema order in the JSON too, so a diff shows a new parameter in one place.
    params = preset["params"]
    for pid in parameter_order(plugin):
        if pid in params:
            data[pid] = params[pid]

    unknown = sorted(set(params) - set(SCHEMA[plugin]))
    if unknown:
        raise SystemExit(f"{plugin} / {preset['name']}: unknown parameter(s) {', '.join(unknown)}")

    return data


def xml_escape_attribute(value):
    return quoteattr(str(value))


def preset_xml(plugin, preset, indent="  "):
    lines = []
    lines.append(f"{indent}<AudioSuitePreset formatVersion=\"1\" plugin={xml_escape_attribute(plugin)}"
                 f" pluginVersion={xml_escape_attribute(PLUGIN_VERSION)}>")
    lines.append(f"{indent}  <Meta name={xml_escape_attribute(preset['name'])}"
                 f" category={xml_escape_attribute(preset['category'])}"
                 f" author={xml_escape_attribute(AUTHOR)}"
                 f" description={xml_escape_attribute(preset['description'])}/>")
    lines.append(f"{indent}  <PARAMETERS suiteStateVersion=\"{state_version_for(plugin)}\""
                 + (f" lerpCurve={xml_escape_attribute(preset['lerpCurve'])}" if "lerpCurve" in preset else "")
                 + (f" samplePath={xml_escape_attribute(preset['samplePath'])}" if "samplePath" in preset else "")
                 + "".join(f" loop{index + 1}Path={xml_escape_attribute(path)}"
                           for index, path in enumerate(preset.get("loopPaths", [])))
                 + ">")

    for pid in parameter_order(plugin):
        if pid in preset["params"]:
            value = format_value(SCHEMA[plugin][pid], preset["params"][pid])
            lines.append(f"{indent}    <PARAM id={xml_escape_attribute(pid)} value={xml_escape_attribute(value)}/>")

    lines.append(f"{indent}  </PARAMETERS>")
    lines.append(f"{indent}</AudioSuitePreset>")
    return "\n".join(lines)


def build(out_dir):
    out_dir = Path(out_dir)

    xml_lines = [
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>",
        "<Bank version=\"1\" generator=\"tools/build_bank.py\">",
    ]

    total = 0

    for plugin in SCHEMA:
        plugin_dir = out_dir / plugin
        plugin_dir.mkdir(parents=True, exist_ok=True)

        seen_slugs = set()

        for preset in PRESETS.get(plugin, []):
            data = preset_json(plugin, preset)

            slug = slugify(preset["name"])
            if slug in seen_slugs:
                raise SystemExit(f"{plugin}: duplicate file slug {slug!r}")
            seen_slugs.add(slug)

            (plugin_dir / f"{slug}.json").write_text(
                json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8",
                newline="\n")

            xml_lines.append(preset_xml(plugin, preset))
            total += 1

    xml_lines.append("</Bank>")
    xml_lines.append("")

    (out_dir / "AudioSuitePresets.xml").write_text("\n".join(xml_lines), encoding="utf-8",
                                                   newline="\n")

    print(f"Wrote {total} presets to {out_dir}")
    for plugin in SCHEMA:
        print(f"  {plugin}: {len(PRESETS.get(plugin, []))}")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", default=str(DEFAULT_OUT),
                        help=f"output directory (default: {DEFAULT_OUT})")
    args = parser.parse_args(argv)

    build(args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
