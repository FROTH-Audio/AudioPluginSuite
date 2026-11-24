"""Scales every manual image to ~2/3 of its native size and adds one screenshot per
plugin section to the README (owner request 2026-11-18).

GitHub-flavoured Markdown ignores ![]() size syntax, so the manuals use <img> tags with
an explicit width. The width is read from the image itself and rounded to two thirds.
Idempotent: an <img> line whose width already matches is left alone.
"""

import os
import re

from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
MANUALS = os.path.join(ROOT, "docs", "manuals")
PLUGINS = ["SpectralDrift", "DynamicSaturation", "MicroTimingChaos", "Sgraffito",
           "StereoTool", "MidiEffect", "MultibandSaturation"]

# The signature screenshot of each plugin section in the README.
README_SHOTS = {
    "### Spectral Drift Shifter": "docs/manuals/spectraldrift-sound.png",
    "### Dynamic Saturation Companion": "docs/manuals/dynamicsaturation-sound.png",
    "### Micro-Timing Chaos": "docs/manuals/microtimingchaos-sequence.png",
    "### Sgraffito Memory Sampler": "docs/manuals/sgraffito-sound.png",
    "### Stereo Tool Effect": "docs/manuals/stereotool-default.png",
    "### MIDI Chord Arp Scale Tool": "docs/manuals/midieffect-default.png",
    "### Multiband Saturation": "docs/manuals/multibandsaturation-default.png",
}


def scaled_width(path, fraction = 2.0 / 3.0):
    width = Image.open(path).size[0]
    return max(1, int(round(width * fraction)))


def scale_manual_images(manual_path):
    """Rewrites ![alt](relative path) lines into <img> tags at two-thirds width."""
    lines = open(manual_path, encoding="utf-8").read().splitlines(keepends=True)
    out = []
    changed = 0
    for line in lines:
        match = re.match(r'^!\[(.*?)\]\(((?:\.\./)+docs/manuals/[^)]+)\)\s*$', line.rstrip("\n"))
        if match:
            alt, path = match.group(1), match.group(2)
            image = os.path.normpath(os.path.join(os.path.dirname(manual_path), path))
            width = scaled_width(image)
            out.append(f'<img src="{path}" alt="{alt}" width="{width}">\n')
            changed += 1
        else:
            out.append(line)
    open(manual_path, "w", encoding="utf-8", newline="").writelines(out)
    return changed


def insert_readme_screenshots(readme_path):
    src = open(readme_path, encoding="utf-8", newline="").read()
    inserted = 0
    for heading, image in README_SHOTS.items():
        if image in src:
            continue
        idx = src.find(heading)
        assert idx >= 0, heading
        line_end = src.find("\n", idx) + 1
        width = scaled_width(os.path.join(ROOT, image))
        tag = f'\n<img src="{image}" alt="{heading[4:]} screenshot" width="{width}">\n'
        src = src[:line_end] + tag + src[line_end:]
        inserted += 1
    open(readme_path, "w", encoding="utf-8", newline="").write(src)
    return inserted


def main():
    total = 0
    for plugin in PLUGINS:
        manual = os.path.join(ROOT, "plugins", plugin, "MANUAL.md")
        n = scale_manual_images(manual)
        print(f"{plugin}: {n} images scaled to 2/3")
        total += n
    print("README screenshots inserted:", insert_readme_screenshots(os.path.join(ROOT, "README.md")))


if __name__ == "__main__":
    main()
