#!/usr/bin/env python3
"""Creates a new plugin folder from tools/new_plugin/skeleton.

Usage:
    python tools/new_plugin/new_plugin.py <Name> "<Product name>" <Code>

    Name          CamelCase identifier, for example SpectralDrift. Creates plugins/<Name>/, with the
                  targets <Name> and <Name>Tests.
    Product name  Name shown by hosts, for example "Spectral Drift Shifter".
    Code          Four letters or digits, unique among all plugins. Plugin codes are saved in sessions and
                  must never change after a release, so pick it carefully.

What it does:
    - Refuses if plugins/<Name>/ exists, or if the plugin code is already used by a plugin in plugins/*/CMakeLists.txt.
    - Copies the skeleton into plugins/<Name>/ and replaces __NAME__, __PRODUCT__ and __CODE__.
    - Writes nothing outside plugins/<Name>/. It reads the skeleton and the plugins' CMakeLists.txt files.

After it runs, add one line to the root CMakeLists.txt, next to the other plugins:
    add_subdirectory(plugins/<Name>)
"""

import pathlib
import re
import shutil
import sys

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parent.parent
SKELETON = HERE / "skeleton"
PLUGINS = REPO / "plugins"

NAME_RE = re.compile(r"^[A-Z][A-Za-z0-9]*$")
CODE_RE = re.compile(r"^[A-Za-z0-9]{4}$")
# Matches PLUGIN_CODE followed by the code, in both the juce_add_plugin and suite_add_plugin forms.
USED_CODE_RE = re.compile(r"PLUGIN_CODE\s+([A-Za-z0-9]{4})\b")


def fail(message):
    print(f"error: {message}", file=sys.stderr)
    sys.exit(1)


def codes_in_use():
    used = {}
    for cmake in sorted(PLUGINS.glob("*/CMakeLists.txt")):
        for code in USED_CODE_RE.findall(cmake.read_text(encoding="utf-8")):
            used.setdefault(code, cmake.parent.name)
    return used


def main(argv):
    if len(argv) != 3:
        print(__doc__)
        sys.exit(2)

    name, product, code = argv

    if not NAME_RE.match(name):
        fail(f"Name must be CamelCase letters and digits, starting with a capital: '{name}'")
    if not product.strip() or '"' in product:
        fail("Product name must not be empty and must not contain double quotes")
    if not CODE_RE.match(code):
        fail(f"Plugin code must be exactly four letters or digits: '{code}'")
    if not any(c.isupper() for c in code):
        fail(f"Plugin code must contain at least one uppercase letter (README, Adding a plugin): '{code}'")

    target = PLUGINS / name
    if target.exists():
        fail(f"plugins/{name} already exists")

    used = codes_in_use()
    if code in used:
        fail(f"plugin code '{code}' is already used by plugins/{used[code]}")

    if not SKELETON.is_dir():
        fail(f"skeleton folder not found: {SKELETON}")

    replacements = {"__NAME__": name, "__PRODUCT__": product, "__CODE__": code}
    created = []
    try:
        for source in sorted(SKELETON.rglob("*")):
            if source.is_dir():
                continue
            relative = source.relative_to(SKELETON)
            destination = target / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            text = source.read_text(encoding="utf-8")
            for placeholder, value in replacements.items():
                text = text.replace(placeholder, value)
            destination.write_text(text, encoding="utf-8", newline="\n")
            created.append(destination)
    except Exception:
        # Do not leave a half-made plugin behind.
        shutil.rmtree(target, ignore_errors=True)
        raise

    print(f"Created plugins/{name} ({len(created)} files):")
    for path in created:
        print(f"    {path.relative_to(REPO).as_posix()}")
    print()
    print("Next: add this line to the root CMakeLists.txt, next to the other plugins:")
    print(f"    add_subdirectory(plugins/{name})")


if __name__ == "__main__":
    main(sys.argv[1:])
