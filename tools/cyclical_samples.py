"""Generates the Cyclical embedded loop library (plan 6.1).

One deterministic recipe per name, seeded by the name, written as 48 kHz mono 16-bit WAV,
EXACTLY 2.000 s long (96000 frames = one bar at 120 BPM). The exact bar length matters: the
double-speed lap test and the one-bar quantise presets assume it, and the engine keeps the
source rate rather than resampling. Sine-based recipes use whole numbers of cycles per bar
so the loops are seamless before the engine's seam crossfade even runs.

Run from the repository root:   python tools/cyclical_samples.py
"""

import math
import struct
import wave
from pathlib import Path

RATE = 48000
BAR_FRAMES = 96000            # exactly 2.000 s: one bar at 120 BPM
OUT_DIR = Path(__file__).resolve().parent.parent / "plugins" / "Cyclical" / "Resources" / "samples"

NAMES = ["kick", "hats", "snare", "perc", "bass", "chord", "arp",
         "texture", "drone", "bells", "vox", "noise"]


def hash_seed(name):
    seed = 0
    for character in name:
        seed = (seed * 31 + ord(character)) & 0xFFFFFFFF
    return seed


def unit(seed, index):
    """Mirror of CoreDSP::Sequencer::hashUnit, so the recipes are deterministic."""
    x = (seed * 0x9E3779B9 + index * 0x85EBCA6B + 0x27D4EB2F) & 0xFFFFFFFF
    x ^= x >> 16
    x = (x * 0x7FEB352D) & 0xFFFFFFFF
    x ^= x >> 15
    x = (x * 0x846CA68B) & 0xFFFFFFFF
    x ^= x >> 16
    return (x >> 8) / 16777216.0


def decay_envelope(n, length, power=4.0):
    if n >= length:
        return 0.0
    return math.exp(-power * n / length)


def sine(n, hz):
    return math.sin(2.0 * math.pi * hz * n / RATE)


def write_loop(name, frames):
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    path = OUT_DIR / f"{name}_4k.wav"

    with wave.open(str(path), "wb") as stream:
        stream.setnchannels(1)
        stream.setsampwidth(2)
        stream.setframerate(RATE)

        data = b"".join(struct.pack("<h", max(-32767, min(32767, int(sample * 32767.0))))
                        for sample in frames)
        stream.writeframes(data)

    print(f"  {name}_4k.wav ({len(frames)} frames)")


def recipe_kick(seed):
    frames = []
    for n in range(BAR_FRAMES):
        # One kick on the downbeat: a pitch-dropping sine with a fast decay.
        drop = 120.0 * math.exp(-n / 2400.0) + 40.0
        envelope = decay_envelope(n, RATE // 2, 6.0)
        click = 0.3 * decay_envelope(n, 240, 12.0) * math.sin(2.0 * math.pi * 900.0 * n / RATE)
        frames.append(0.9 * envelope * sine(n, drop) + click)
    return frames


def recipe_hats(seed):
    frames = []
    state = seed & 0xFFFF
    noise = [0.0] * BAR_FRAMES

    for n in range(BAR_FRAMES):
        state = (state * 1103515245 + 12345) & 0x7FFFFFFF
        noise[n] = (state / 0x3FFFFFFF - 1.0)

    # One-pole high pass, then 8th-note bursts.
    previous_in = 0.0
    previous_out = 0.0
    for n in range(BAR_FRAMES):
        alpha = 0.55
        high = alpha * (previous_out + noise[n] - previous_in)
        previous_in = noise[n]
        previous_out = high

        step = n % (RATE // 8)   # 8th notes at 120 BPM
        envelope = decay_envelope(step, RATE // 16, 9.0)
        frames.append(0.5 * high * envelope)
    return frames


def recipe_snare(seed):
    frames = []
    state = (seed * 2654435761) & 0x7FFFFFFF
    noise = [0.0] * BAR_FRAMES
    for n in range(BAR_FRAMES):
        state = (state * 1103515245 + 12345) & 0x7FFFFFFF
        noise[n] = state / 0x3FFFFFFF - 1.0

    for n in range(BAR_FRAMES):
        # Snare on beats 2 and 4.
        beat = (n % (RATE // 2)) if (n // (RATE // 2)) % 2 == 1 else -1
        if beat >= 0:
            envelope = decay_envelope(beat, RATE // 4, 8.0)
            body = 0.4 * envelope * sine(n, 190.0)
            rattle = 0.5 * envelope * noise[n]
        else:
            body = rattle = 0.0
        frames.append(body + rattle)
    return frames


def recipe_perc(seed):
    frames = []
    # A 3-against-4 click pattern with seeded tuning.
    for n in range(BAR_FRAMES):
        sample = 0.0
        for k, division in enumerate((3, 4)):
            step = n % (RATE // division)
            start = (n // (RATE // division)) % 2 == k % 2
            if start:
                envelope = decay_envelope(step, RATE // 24, 10.0)
                hz = 400.0 + 200.0 * unit(seed, k)
                sample += 0.45 * envelope * sine(n, hz)
        frames.append(sample)
    return frames


def recipe_bass(seed):
    frames = []
    # Four ascending notes, one per beat, saw-ish (a few sine harmonics).
    degrees = [0, 3, 5, 7]
    root = 55.0   # A1
    for n in range(BAR_FRAMES):
        beat = n // (RATE // 2)
        note = root * (2.0 ** (degrees[beat % 4] / 12.0))
        fade = min(1.0, (n % (RATE // 2)) / 200.0)
        sample = 0.0
        for harmonic, weight in ((1, 1.0), (2, 0.4), (3, 0.25)):
            if (note * harmonic) < RATE / 2:
                sample += weight * sine(n, note * harmonic)
        frames.append(0.55 * fade * sample)
    return frames


def recipe_chord(seed):
    frames = []
    # A whole-bar A minor pad: A3, C4, E4, each an integer number of cycles per bar.
    for n in range(BAR_FRAMES):
        sample = 0.0
        for hz in (220.0, 261.63, 329.63):
            cycles = round(hz * 2.0)          # whole cycles per 2 s bar
            sample += math.sin(2.0 * math.pi * cycles * n / BAR_FRAMES)
        swell = 0.5 + 0.5 * math.sin(2.0 * math.pi * n / BAR_FRAMES)
        frames.append(0.3 * swell * sample)
    return frames


def recipe_arp(seed):
    frames = []
    # An 8-step arpeggio, one note per 8th, whole cycles per step so it loops clean.
    intervals = [0, 7, 12, 7, 3, 10, 15, 10]
    for n in range(BAR_FRAMES):
        step = n // (RATE // 8)
        cycles = 2 * (1 + intervals[step % 8] // 4)
        sample = math.sin(2.0 * math.pi * cycles * (n % (RATE // 8)) / (RATE // 8))
        gate = min(1.0, 4.0 * (1.0 - (n % (RATE // 8)) / (RATE // 8)))
        frames.append(0.5 * gate * sample)
    return frames


def recipe_texture(seed):
    frames = []
    state = (seed ^ 0x2545F491) & 0x7FFFFFFF
    previous = 0.0
    for n in range(BAR_FRAMES):
        state = (state * 1103515245 + 12345) & 0x7FFFFFFF
        noise = state / 0x3FFFFFFF - 1.0
        # A slow low-pass breath over filtered noise.
        alpha = 0.02 + 0.06 * (0.5 + 0.5 * math.sin(2.0 * math.pi * 2 * n / BAR_FRAMES))
        previous += alpha * (noise - previous)
        frames.append(0.6 * previous)
    return frames


def recipe_drone(seed):
    frames = []
    # Two low sines an octave apart, whole cycles per bar, beating slowly.
    for n in range(BAR_FRAMES):
        a = math.sin(2.0 * math.pi * 55 * n / BAR_FRAMES)
        b = 0.6 * math.sin(2.0 * math.pi * 110 * n / BAR_FRAMES)
        shimmer = 0.15 * math.sin(2.0 * math.pi * 2 * n / BAR_FRAMES)
        frames.append(0.5 * (a + b + shimmer))
    return frames


def recipe_bells(seed):
    frames = []
    # Three bell hits (FM-ish: carrier + a decaying modulator), whole cycles per bar.
    for n in range(BAR_FRAMES):
        sample = 0.0
        for k, start in enumerate((0, RATE // 4, RATE // 2)):
            local = n - start
            if local >= 0:
                envelope = decay_envelope(local, RATE // 3, 5.0)
                carrier = 6 * (2 + k)
                modulator = 2.0 * math.sin(2.0 * math.pi * carrier * 3 * local / RATE)
                sample += 0.35 * envelope * math.sin(2.0 * math.pi * carrier * local / RATE + modulator)
        frames.append(sample)
    return frames


def recipe_vox(seed):
    frames = []
    # A formant chop: a pulse train through two resonant-ish band emphases.
    for n in range(BAR_FRAMES):
        step = n % (RATE // 4)
        pulse = 1.0 if step < 240 else 0.0
        sample = pulse * (sine(n, 220.0) + 0.6 * sine(n, 660.0) + 0.3 * sine(n, 1100.0))
        chop = decay_envelope(step, RATE // 8, 6.0)
        frames.append(0.55 * chop * sample)
    return frames


def recipe_noise(seed):
    frames = []
    state = (seed + 0x9E3779B9) & 0x7FFFFFFF
    previous = 0.0
    for n in range(BAR_FRAMES):
        state = (state * 1103515245 + 12345) & 0x7FFFFFFF
        noise = state / 0x3FFFFFFF - 1.0
        # A bandpass sweep: low-pass, then subtract a slower low-pass (the band).
        alpha = 0.01 + 0.2 * (0.5 + 0.5 * math.sin(2.0 * math.pi * n / BAR_FRAMES))
        previous += alpha * (noise - previous)
        slower = 0.9 * previous
        frames.append(0.6 * (previous - slower))
    return frames


RECIPES = {
    "kick": recipe_kick,
    "hats": recipe_hats,
    "snare": recipe_snare,
    "perc": recipe_perc,
    "bass": recipe_bass,
    "chord": recipe_chord,
    "arp": recipe_arp,
    "texture": recipe_texture,
    "drone": recipe_drone,
    "bells": recipe_bells,
    "vox": recipe_vox,
    "noise": recipe_noise,
}


def main():
    print(f"Writing {len(NAMES)} loops to {OUT_DIR}")
    for name in NAMES:
        frames = RECIPES[name](hash_seed(name))
        assert len(frames) == BAR_FRAMES
        peak = max(abs(sample) for sample in frames)
        assert peak < 1.0, f"{name} clips ({peak})"
        write_loop(name, frames)
    print("Done.")


if __name__ == "__main__":
    main()
