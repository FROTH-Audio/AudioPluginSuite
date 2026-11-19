# Micro-Timing Chaos — User Manual

<img src="../../docs/manuals/microtimingchaos-sequence.png" alt="Micro-Timing Chaos" width="653">

Micro-Timing Chaos is a MIDI step sequencer whose steps drift in time, pitch and velocity
along a Lorenz attractor, so the groove never plays the same way twice — and the same
pattern gets the same chaos at any tempo. Audio passes through unchanged, so the plugin can
sit on any track.

A technical reference (every control, the LFO and routing details, latency, building and
testing) is in [README.md](README.md).

## The three pages

The editor has three pages: **SEQUENCE** (pattern, melody, transport and the step grid),
**DRIFT** (the lerp curve and the attractor) and **MOD** (the LFO and routing panel).

## SEQUENCE page

<img src="../../docs/manuals/microtimingchaos-detail-pattern.png" alt="Pattern group" width="640">

**Pattern.** Length sets how many steps the pattern has before it repeats, Density sets the
chance that a step fires (each step has a fixed, seed-based decision, so the pattern is
stable), Seed chooses the pattern and the fixed melody, Gate sets the note length as a share
of a step, Swing delays every second step (negative values move them early) and Velocity is
the base velocity.

<img src="../../docs/manuals/microtimingchaos-detail-transport.png" alt="Transport group" width="317">

**Transport.** Tempo is used when the plugin runs freely; Host Sync follows the host's
tempo and position instead (when the transport stops, all notes end, and the host tempo is
shown under the button). Speed sets how fast the attractor moves relative to the tempo.
The rate switches choose the step length (1/4 … 1/32).

<img src="../../docs/manuals/microtimingchaos-detail-step-grid.png" alt="Step grid" width="640">

**Sequencer.** The step grid is the live picture: filled columns are the steps that fire,
the outlined column is the playhead, and each dot is a recent note — its height shows its
timing shift and its size shows its velocity.

## DRIFT page

<img src="../../docs/manuals/microtimingchaos-detail-lerp-curve.png" alt="Lerp curve" width="374">

**Lerp Curve.** The curve turns Chaos into a drift amount between 0 and 1: drag its five
points to shape it, right-click a point to reset it, double-click the graph to reset the
whole curve. The marker on the curve is where the current Chaos sits. **Lerp** sets how far
each step drifts toward the attractor, and the X / Y / Z multipliers scale the timing, the
pitch-and-gate drift and the velocity drift separately.

<img src="../../docs/manuals/microtimingchaos-detail-chaos.png" alt="Chaos group" width="257">

**Chaos.** Chaos is the input to the curve — at 0 % the groove stays fixed whatever the
curve does. Timing Range is the largest timing shift, Velocity Chaos the velocity swing.

**Attractor visualization.** The panel next to the curve draws the attractor's path; the
PREVIEW tab shows the live drift per axis and the timing, pitch and velocity of the last 32
steps:

<img src="../../docs/manuals/microtimingchaos-drift-preview.png" alt="Drift preview" width="653">

## MOD page

<img src="../../docs/manuals/microtimingchaos-mod.png" alt="MOD page" width="653">

The MOD page holds the two LFOs and the six routing slots; 14 targets are available, from
Chaos and Timing Range to Seed (which re-rolls the pattern). The shared LFOs and routing are
described in the [suite README](../README.md#modulation). A fast LFO is sampled at step
times, so it can alias against the grid.

## Practical notes

- The plugin reports a fixed 50 ms of latency: early steps are held back so they can be
  sent on time, and the host compensates.
- At the defaults the timing drift is about a third of Timing Range — raise Chaos and Lerp
  for more movement.
- The factory presets (bank build) range from subtle shuffles to full chaos; they all run
  at 120 BPM with host sync off.
