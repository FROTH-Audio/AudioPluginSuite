#!/usr/bin/env python3
"""Generates the Sgraffito factory sample set: one WAV per factory preset.

Plan 10 phase 7 (owner finding #8, decision G7): every factory preset ships its own
embedded sample, chosen to showcase that preset's performance. Output:
plugins/Sgraffito/Resources/samples/presets/<slug>_4k.wav, 48 kHz mono 16-bit,
1.5-3 s each (~2 MB total embedded).

The samples are synthesised with the standard library only (no numpy), and every
recipe is seeded from the preset name (crc32), so regeneration is deterministic and
reviewable. `tools/build_bank.py` and `tools/audit_bank.py` are not touched by this
script; the preset authoring in preset_data.py references the generated names as
`embedded:` URIs.

Run from the repository root:  python tools/sgraffito_samples.py
"""

import math
import random
import struct
import wave
import zlib
from pathlib import Path

SAMPLE_RATE = 48000
ROOT = Path(__file__).resolve().parent.parent
OUTPUT_DIR = ROOT / "plugins" / "Sgraffito" / "Resources" / "samples" / "presets"


# ---------------------------------------------------------------------------
# Small synthesis helpers. All return float lists in [-1, 1]; every recipe is
# seeded from the preset name so the output is reproducible.


def seconds(n):
    return int(round(n * SAMPLE_RATE))


def mix(*layers):
    """Adds any number of layers; each layer is a sample list (gain 1.0) or a
    (samples, gain) pair, with lengths not required to match."""
    parts = []

    for layer in layers:
        if isinstance(layer, tuple) and not isinstance(layer[0], (int, float)):
            parts.append((layer[0], float(layer[1])))
        else:
            parts.append((layer, 1.0))

    length = max(len(s) for s, _ in parts)
    out = [0.0] * length
    for samples, gain in parts:
        for i, v in enumerate(samples):
            out[i] += v * gain
    return out


def tone(freq, dur, harmonics=((1, 1.0),), attack=0.0, decay=None, vibrato=0.0,
         vibratoHz=5.0, drift=0.0):
    """A harmonic tone with an exponential decay and optional vibrato/drift."""
    n = seconds(dur)
    out = [0.0] * n
    for multiple, amp in harmonics:
        phase = random.random() * 2.0 * math.pi
        for i in range(n):
            t = i / SAMPLE_RATE
            f = freq * multiple * (1.0 + drift * t)
            if vibrato > 0.0:
                f *= 1.0 + vibrato * math.sin(2.0 * math.pi * vibratoHz * t)
            out[i] += amp * math.sin(2.0 * math.pi * f * t + phase)

    env = [min(1.0, i / max(1, seconds(attack))) if attack > 0 else 1.0 for i in range(n)]
    if decay is not None:
        tau = seconds(decay)
        for i in range(n):
            env[i] *= math.exp(-i / tau)
    return [s * e for s, e in zip(out, env)]


def noise(dur, level=1.0, lp=0.0):
    """Random noise; lp (0..1) is a one-pole low-pass mix for a darker bed."""
    n = seconds(dur)
    out = [0.0] * n
    state = 0.0
    for i in range(n):
        v = random.uniform(-1.0, 1.0)
        state += lp * (v - state)
        out[i] = state if lp > 0.0 else v
    return [s * level for s in out]


def shape(samples, drive=1.0):
    """Soft tanh saturation, for the tape-flavoured recipes."""
    return [math.tanh(v * drive) for v in samples]


def fade(samples, fadeIn=0.0, fadeOut=0.05):
    a = seconds(fadeIn)
    o = seconds(fadeOut)
    n = len(samples)
    out = list(samples)
    for i in range(min(a, n)):
        out[i] *= i / max(1, a)
    for i in range(min(o, n)):
        out[n - 1 - i] *= i / max(1, o)
    return out


def silence(dur):
    return [0.0] * seconds(dur)


def concatenate(*parts):
    out = []
    for p in parts:
        out.extend(p)
    return out


# ---------------------------------------------------------------------------
# One recipe per preset, in the preset list's order (preset_data.py:241-262).


def glass_dust():
    """Bright inharmonic bell cluster, long decay."""
    return fade(mix(
        tone(523.25, 3.0, ((1, 1.0), (2.76, 0.5), (5.40, 0.25)), decay=1.6),
        tone(659.25, 3.0, ((1, 0.8), (2.76, 0.4)), attack=0.3, decay=1.2),
        tone(392.0, 3.0, ((1, 0.5), (2.76, 0.3)), attack=0.6, decay=2.0),
    ), fadeIn=0.05, fadeOut=0.3)


def deep_cloud():
    """Dark sustained drone with slow beating."""
    return fade(mix(
        tone(82.4, 3.0, ((1, 1.0), (2, 0.4), (3, 0.2)), attack=0.8),
        tone(82.9, 3.0, ((1, 0.9),), attack=0.8),
        tone(164.8, 3.0, ((1, 0.3),), attack=1.2),
    ), fadeIn=0.2, fadeOut=0.4)


def memory_ghosts():
    """Decaying piano-like tones with gaps, for ghost notes over a trace."""
    notes = [(261.6, 0.6), (329.6, 0.55), (392.0, 0.55), (523.25, 0.6)]
    parts = []
    for freq, dur in notes:
        parts.append(tone(freq, dur, ((1, 1.0), (2, 0.3), (4, 0.12)), decay=dur * 0.7))
        parts.append(silence(0.12))
    return fade(concatenate(*parts), fadeOut=0.2)


def tape_break():
    """Saturated transient loop, for chopped tape-stop breaks."""
    hits = []
    for freq in (110.0, 110.0, 146.8, 110.0):
        hits.append(shape(mix(
            tone(freq, 0.45, ((1, 1.0), (2, 0.6)), decay=0.18, attack=0.001),
            noise(0.45, 0.4, lp=0.3),
        ), drive=2.5))
        hits.append(silence(0.05))
    return fade(concatenate(*hits), fadeOut=0.15)


def cloud_rain():
    """Sparse droplet plinks over a soft bed."""
    drops = []
    for _ in range(6):
        freq = random.choice((1046.5, 1174.7, 1318.5, 1568.0))
        drops.append(tone(freq, 0.4, ((1, 1.0), (2.0, 0.2)), decay=0.12))
        drops.append(silence(random.uniform(0.02, 0.08)))
    bed = noise(2.4, 0.08, lp=0.8)
    return fade(mix((concatenate(*drops), 1.0), (bed, 1.0)), fadeIn=0.1, fadeOut=0.25)


def scrubbed_memory():
    """A noisy phrase with a pitch wobble, for scrubbed-scratch playback."""
    return fade(shape(mix(
        tone(220.0, 2.5, ((1, 1.0), (2, 0.5), (3, 0.25)), vibrato=0.02, vibratoHz=3.0, decay=2.2),
        noise(2.5, 0.25, lp=0.5),
    ), drive=1.4), fadeIn=0.05, fadeOut=0.25)


def chopped_sticks():
    """Dry wood hits, evenly spaced for chopping."""
    parts = []
    for _ in range(8):
        parts.append(mix(
            noise(0.22, 0.9, lp=0.15),
            tone(660.0, 0.22, ((1, 0.5),), decay=0.07),
        ))
        parts.append(silence(0.1))
    return fade(concatenate(*parts), fadeOut=0.1)


def swell_pad():
    """A slow-attack evolving pad, brightness rising."""
    n = seconds(3.0)
    out = [0.0] * n
    for k, (freq, amp, rise) in enumerate(((220.0, 0.6, 0.5), (277.2, 0.5, 1.0),
                                           (329.6, 0.5, 1.5), (440.0, 0.35, 2.0))):
        phase = k * 0.7
        for i in range(n):
            t = i / SAMPLE_RATE
            env = min(1.0, t / rise)
            out[i] += amp * env * math.sin(2.0 * math.pi * freq * t + phase)
    return fade(out, fadeOut=0.5)


def cloud_dust():
    """An airy noise bed with a tonal shimmer."""
    return fade(mix(
        noise(2.5, 0.35, lp=0.85),
        tone(2093.0, 2.5, ((1, 0.15),), attack=0.8),
        tone(2637.0, 2.5, ((1, 0.1),), attack=1.2),
    ), fadeIn=0.3, fadeOut=0.4)


def wobble_memory():
    """A pitch-wobbled tone: the wow character, audible when scrubbed."""
    return fade(tone(174.6, 2.5, ((1, 1.0), (2, 0.35), (3, 0.15)),
                     vibrato=0.04, vibratoHz=1.5, attack=0.1), fadeOut=0.3)


def ghost_delay():
    """A three-note motif with fading echo repeats."""
    motif = []
    for freq in (440.0, 523.25, 587.3):
        motif.append(tone(freq, 0.3, ((1, 1.0), (2, 0.2)), decay=0.2))
        motif.append(silence(0.1))
    motif = concatenate(*motif)
    echo1 = [v * 0.55 for v in motif]
    echo2 = [v * 0.3 for v in motif]
    return fade(mix((concatenate(motif, silence(0.9)), 1.0),
                    (concatenate(silence(0.35), echo1, silence(0.6)), 1.0),
                    (concatenate(silence(0.7), echo2, silence(0.6)), 1.0)), fadeOut=0.3)


def swelling_cloud():
    """A crescendo texture; ends loud, so Swell's reverse playback reads naturally."""
    n = seconds(3.0)
    out = [0.0] * n
    bed = noise(3.0, 1.0, lp=0.6)
    for i in range(n):
        t = i / n
        swell = t * t
        out[i] = 0.5 * bed[i] * swell + 0.35 * swell * math.sin(2.0 * math.pi * 146.8 * i / SAMPLE_RATE)
    return fade(out, fadeIn=0.1, fadeOut=0.1)


def choppy_scratch():
    """A rhythmic chopped phrase: a hard-gated tone with strong transients."""
    n = seconds(2.4)
    out = [0.0] * n
    gate_period = seconds(0.15)
    for i in range(n):
        on = (i % gate_period) < seconds(0.09)
        if on:
            click = max(0.0, 1.0 - (i % gate_period) / seconds(0.01))
            out[i] = click * 0.6 + 0.35 * math.sin(2.0 * math.pi * 196.0 * i / SAMPLE_RATE)
    return fade(out, fadeOut=0.1)


def drifted_memory():
    """Two detuned oscillators drifting against each other."""
    return fade(mix(
        tone(146.83, 3.0, ((1, 1.0), (2, 0.3)), drift=0.01),
        tone(147.5, 3.0, ((1, 1.0), (2, 0.3)), drift=-0.01),
    ), fadeIn=0.2, fadeOut=0.4)


def init_cloud():
    """A neutral reference tone for the utility preset."""
    return fade(tone(220.0, 2.0, ((1, 1.0),), attack=0.02), fadeOut=0.2)


def velvet_swing():
    """Warm, rounded groove tones."""
    parts = []
    for freq, dur in ((130.8, 0.5), (155.6, 0.35), (174.6, 0.55), (155.6, 0.4)):
        parts.append(tone(freq, dur, ((1, 1.0), (2, 0.25), (3, 0.08)), attack=0.02, decay=dur * 0.8))
        parts.append(silence(0.12))
    return fade(concatenate(*parts), fadeOut=0.2)


def ghostly_echo():
    """Sparse hits with a long noise tail."""
    hits = concatenate(
        mix(tone(329.6, 0.6, ((1, 1.0),), decay=0.3), noise(0.6, 0.15, lp=0.7)),
        silence(0.4),
        mix(tone(392.0, 0.6, ((1, 1.0),), decay=0.3), noise(0.6, 0.15, lp=0.7)),
        silence(0.4),
        noise(1.0, 0.12, lp=0.85),
    )
    return fade(hits, fadeIn=0.05, fadeOut=0.4)


def held_breath():
    """A breathy filtered noise bed with a slow envelope."""
    n = seconds(2.6)
    bed = noise(2.6, 1.0, lp=0.9)
    out = []
    for i in range(n):
        t = i / n
        env = math.sin(math.pi * min(1.0, t)) ** 2
        out.append(bed[i] * 0.8 * env + 0.15 * env * math.sin(2.0 * math.pi * 246.9 * i / SAMPLE_RATE))
    return fade(out, fadeIn=0.4, fadeOut=0.4)


def fine_grained_drift():
    """Dense high partials with slow per-partial amplitude drift."""
    n = seconds(2.6)
    out = [0.0] * n
    partials = [(987.8, 0.3), (1108.7, 0.25), (1318.5, 0.2), (1480.0, 0.15), (1760.0, 0.1)]
    for k, (freq, amp) in enumerate(partials):
        rate = 0.3 + 0.12 * k
        phase = random.random() * 2.0 * math.pi
        tone_phase = random.random() * 2.0 * math.pi
        for i in range(n):
            t = i / SAMPLE_RATE
            env = 0.5 + 0.5 * math.sin(2.0 * math.pi * rate * t + phase)
            out[i] += amp * env * math.sin(2.0 * math.pi * freq * t + tone_phase)
    return fade(out, fadeIn=0.3, fadeOut=0.4)


def slow_attack_tape():
    """A tape-saturated tone that eases in, with hiss."""
    return fade(shape(mix(
        tone(110.0, 3.0, ((1, 1.0), (2, 0.4), (3, 0.2)), attack=1.2, decay=3.0),
        noise(3.0, 0.06, lp=0.6),
    ), drive=1.6), fadeOut=0.4)


RECIPES = {
    "Glass Dust": glass_dust,
    "Deep Cloud": deep_cloud,
    "Memory Ghosts": memory_ghosts,
    "Tape Break": tape_break,
    "Cloud Rain": cloud_rain,
    "Scrubbed Memory": scrubbed_memory,
    "Chopped Sticks": chopped_sticks,
    "Swell Pad": swell_pad,
    "Cloud Dust": cloud_dust,
    "Wobble Memory": wobble_memory,
    "Ghost Delay": ghost_delay,
    "Swelling Cloud": swelling_cloud,
    "Choppy Scratch": choppy_scratch,
    "Drifted Memory": drifted_memory,
    "Init Cloud": init_cloud,
    "Velvet Swing": velvet_swing,
    "Ghostly Echo": ghostly_echo,
    "Held Breath": held_breath,
    "Fine Grained Drift": fine_grained_drift,
    "Slow Attack Tape": slow_attack_tape,
}


def slugify(name):
    """File slug: lowercase words joined by underscores (the BinaryData rule then maps
    every non-alphanumeric to '_', so 'glass_dust_4k.wav' becomes glass_dust_4k_wav)."""
    return "_".join("".join(c for c in word if c.isalnum()).lower()
                    for word in name.split() if word)


def write_wav(name, samples):
    peak = max(1e-9, max(abs(v) for v in samples))
    scale = 0.7 / peak
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    path = OUTPUT_DIR / f"{slugify(name)}_4k.wav"

    with wave.open(str(path), "wb") as handle:
        handle.setnchannels(1)
        handle.setsampwidth(2)
        handle.setframerate(SAMPLE_RATE)
        frames = bytearray()
        for v in samples:
            clamped = max(-1.0, min(1.0, v * scale))
            frames += struct.pack("<h", int(clamped * 32767.0))
        handle.writeframes(bytes(frames))

    return path


def main():
    # Deterministic per preset name across runs and processes (random module's global
    # seed is per-call below, never the process hash).
    total = 0
    print(f"Generating {len(RECIPES)} Sgraffito preset samples into {OUTPUT_DIR.relative_to(ROOT)}")
    for name, recipe in RECIPES.items():
        random.seed(zlib.crc32(name.encode("utf-8")))
        path = write_wav(name, recipe())
        size = path.stat().st_size
        total += size
        print(f"  {path.name:28s} {size / 1024:7.1f} kB")
    print(f"Total: {total / 1024:.1f} kB")


if __name__ == "__main__":
    main()
