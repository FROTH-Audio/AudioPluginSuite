# Multiband Saturation — User Manual

<img src="../../docs/manuals/multibandsaturation-default.png" alt="Multiband Saturation" width="507">

Multiband Saturation splits the signal with a Linkwitz-Riley crossover and saturates the
two bands independently: the low band and the high band each get their own drive, their own
envelope and their own tone, then recombine — the bands sum back to the input's level
through the crossover's all-pass phase, so nothing dips or bumps at the split point.

## The controls

| Control | What it does |
|---|---|
| **Saturation Mode** | The transfer curve, shared by both bands: Tube (asymmetric, with bias), Tape (symmetric soft clip) or Diode (exponential with soft knees). |
| **Crossover** | The split point, 80 Hz – 8 kHz on a log scale (1 kHz at the centre of the travel). |
| **Drive Low / Drive High** | Pre-gain into the shaper, per band. |
| **Envelope Sensitivity** | How strongly each band's envelope pushes its own drive harder: Dynamic Drive = Drive × (1 + Sensitivity × Envelope). |
| **Attack / Release** | Both bands' envelope times. |
| **Tone Low / Tone High** | Tilt around 1 kHz, per band. |
| **Mix** | Dry/wet balance. |
| **Output Level** | The final level. |

## Tips

- **Tame the low end, excite the top.** Drive Low 0–3 dB with Drive High 12–24 dB keeps the
  bass clean while the cymbals and vocals sizzle.
- **The envelopes are per band.** A bass note pumps the low band's drive; a vocal snare
  pushes the high band — with Sensitivity at 0 the drives stay static.
- **The band sum is clamped to the rails**, so even both drives at maximum cannot exceed
  full scale.
- **Mix 0 is the untouched dry input** (once the 50 ms smoother has settled), so you can
  A/B the stage against the bypassed signal.

## Practical notes

- The first pass runs at base rate, so no latency is reported; oversampling (and its
  interaction with the crossovers) is the planned follow-up.
- Modulation routing (the suite's LFOs and slots) is planned for a follow-up pass; the
  plugin already links the shared module.
- There are no factory presets for this plugin yet (bank builds skip it until they are
  authored).
