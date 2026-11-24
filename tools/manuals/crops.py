"""User-manual detail crops (owner request 2026-11-18).

Each plugin's user manual (plugins/<Name>/MANUAL.md) embeds full-page screenshots
(docs/manuals/<plugin>-<view>.png, written by the plugin tests under SUITE_RENDER_DIR)
and cropped detail images where they help. This script cuts the details from the full
renders, with every crop box measured from the shipped editor layout.

Run after regenerating the renders:   python tools/manuals/crops.py
Inputs are read from docs/manuals, details are written back into docs/manuals.
"""

import os

from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
MANUALS = os.path.join(ROOT, "docs", "manuals")

# (source render, (left, top, right, bottom), output file)
CROPS = [
    ("sgraffito-sound.png", (10, 88, 870, 350), "sgraffito-detail-waveform.png"),
    ("sgraffito-sound.png", (10, 56, 870, 92), "sgraffito-detail-mode-strip.png"),
    ("sgraffito-sound.png", (10, 598, 870, 662), "sgraffito-detail-keyboard.png"),
    ("sgraffito-texture.png", (14, 58, 866, 190), "sgraffito-detail-tape-memory.png"),
    ("sgraffito-texture.png", (14, 328, 866, 462), "sgraffito-detail-ghost-cloud.png"),
    ("sgraffito-texture.png", (14, 468, 866, 596), "sgraffito-detail-swell-surface.png"),
    ("sgraffito-mod.png", (14, 60, 836, 472), "sgraffito-detail-modulation-panel.png"),
    # Plan 12: the SEQUENCE page is two columns now — PATTERN left, TRANSPORT top right,
    # MELODY right mid — and the step grid spans the full width below the block.
    ("microtimingchaos-sequence.png", (10, 436, 970, 672), "microtimingchaos-detail-step-grid.png"),
    ("microtimingchaos-sequence.png", (536, 62, 972, 302), "microtimingchaos-detail-transport.png"),
    ("microtimingchaos-sequence.png", (10, 62, 534, 434), "microtimingchaos-detail-pattern.png"),
    ("microtimingchaos-sequence.png", (536, 304, 972, 434), "microtimingchaos-detail-melody.png"),
    ("microtimingchaos-drift-curve.png", (14, 62, 575, 660), "microtimingchaos-detail-lerp-curve.png"),
    ("microtimingchaos-drift-curve.png", (585, 560, 970, 660), "microtimingchaos-detail-chaos.png"),
    ("spectraldrift-sound.png", (16, 95, 704, 345), "spectraldrift-detail-spectrum.png"),
    ("dynamicsaturation-sound.png", (20, 92, 700, 332), "dynamicsaturation-detail-transfer-curve.png"),
    ("dynamicsaturation-sound.png", (135, 326, 645, 364), "dynamicsaturation-detail-mode-pills.png"),
    # Plan 17 Phase 8 + plan 18 Phase 3: the MASTER group (renamed from LEVEL) carries
    # Input/Output/Pan/Phase; the IMAGE group grew to four knob rows, so the window is
    # 858 px tall and the MASTER frame reaches further down.
    ("stereotool-default.png", (10, 172, 215, 572), "stereotool-detail-level.png"),
    ("multibandsaturation-default.png", (10, 304, 750, 662), "multibandsaturation-detail-groups.png"),
]


def main():
    missing = []
    for source, box, output in CROPS:
        src = os.path.join(MANUALS, source)
        if not os.path.exists(src):
            missing.append(source)
            continue
        img = Image.open(src).convert("RGB")
        img.crop(box).save(os.path.join(MANUALS, output))
        print(f"written {output}  ({box[2] - box[0]}x{box[3] - box[1]} from {source})")

    if missing:
        raise SystemExit("missing renders (run the plugin tests with SUITE_RENDER_DIR): "
                         + ", ".join(missing))


if __name__ == "__main__":
    main()
