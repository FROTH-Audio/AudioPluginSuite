# Multiband Saturation — User Manual

<img src="../../docs/manuals/multibandsaturation-default.png" alt="Multiband Saturation" width="587">

Multiband Saturation splits the input into two bands with a Linkwitz-Riley crossover and
shapes each band with its own drive and tone, using the same saturation shapers as
Dynamic Saturation Companion. The bands recombine through the crossover's all-pass sum,
are clamped to the rails, and finish through a dry/wet mix and an output level.

## The controls

The editor has two groups. **SPLIT** is the crossover and the shared saturation mode;
**SHAPER** is the per-band shaping. The panel between the header and the groups is the
dual-band visual: a bar per band (the block's envelope maximum), the crossover frequency
marked between them on a log axis, and input/output meters on the right.

### SPLIT

| Control | What it does |
|---|---|
| **Crossover** | The Linkwitz-Riley split point, 80 Hz – 8 kHz (log scale, 1 kHz at centre). |
| **Mode** | The saturation curve, shared by both bands: Tube, Tape or Diode. |

### SHAPER

| Control | What it does |
|---|---|
| **Drive Low** | Pre-gain of the low band's shaper, 0–24 dB. |
| **Drive High** | Pre-gain of the high band's shaper, 0–24 dB. |
| **Tone Low** | Tilt around 1 kHz on the shaped low band (−1 darkens, +1 brightens). |
| **Tone High** | Tilt around 1 kHz on the shaped high band. |
| **Sensitivity** | How far the envelopes push the drives. |
| **Attack** | Envelope rise, both bands, 1–100 ms. |
| **Release** | Envelope fall, both bands, 50–2000 ms. |
| **Mix** | Dry/wet balance. |
| **Output** | Output level after the mix, −12 to +6 dB. |

Each band gets the Dynamic Drive rule: its drive is multiplied by
`1 + Sensitivity × Envelope`, so loud parts of that band are pushed harder than quiet
ones — the two bands respond independently.

## Reading the visual

- The **LOW** and **HIGH** bars show each band's envelope: how hard that band's detector
  is working right now.
- The **vertical marker** between the bars is the crossover, placed on the same log axis
  the Crossover knob uses (80 Hz – 8 kHz), with its frequency printed above.
- The two meters on the right are the input and output peaks of the last block.

## Tips

- **Selective bass weight.** Drive the low band (12–18 dB), leave the high band near 0,
  and keep the crossover low (150–250 Hz): only the low end gets the harmonics.
- **Air without mud.** Split high (2–3 kHz), drive the high band, and brighten it with
  Tone High — the low band stays untouched.
- **Glue.** Both bands at 2–3 dB with low sensitivity is a gentle full-band saturation.
- **Parallel crunch.** Heavy drive on both bands with Mix around 40 % keeps the
  transients dry.
- The band sum is clamped to the rails before the mix, so the output never explodes even
  at maximum drive on both bands.

## Practical notes

- No latency is added (base-rate processing; oversampling is a planned follow-up).
- 15 factory presets ship with the plugin in bank builds; use the preset bar in the header
  to browse, save and load.
- The hamburger menu in the header (right of the preset bar) opens Settings… (tooltips toggle,
  fallback tempo, preset folder) and About….
- Mode is a pill switch (plan 11 rule: a dropdown only for longer lists).
- Modulation routing (the suite's LFOs and slots) is planned for a follow-up pass; the
  plugin already links the shared module.
