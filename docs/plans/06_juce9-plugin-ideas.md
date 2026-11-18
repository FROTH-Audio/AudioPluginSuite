# Ideas: new plugins and framework features, using JUCE 9

Written for: the owner, as a menu of options to choose from. Ideas only, nothing here is decided or scheduled.

Status: proposal. Module names were checked in the local JUCE 9.0.5 source (`cmake-build-juce9/_deps/juce-src/modules`). Effort is a rough guess for one developer who knows this codebase.

**Status 2026-11-18 (after the owner picked the order):** 2.1's first step is done — the pendulum is a tested `CoreDSP::Modulation` shape and selectable in every plugin through the shared router (appended as index 7). 2.9 and 2.11 exist as first-pass plugins: the mid/side engine (`CoreDSP::Stereo`, lossless-tested), the chord engine and the scale quantisation (reusing `CoreDSP::Sampler`) live in `core_dsp`; both plugins are minimal first steps (2.9: width/drive/mix, no routing yet; 2.11: chord-on-note-on, arp ordering without a step clock). **2.5's first pass is done:** Spectral Drift has the optional Sidechain bus with a peak follower that ducks the drift or the gate (two appended parameters, transparent at the defaults, host-load tested). Brand lockups are generated and embedded (one `tools/brand/lockups.py` row each); what remains is listed in the CHANGELOG.

---

## 1. The framework idea

Today the suite is three plugins that share a DSP header library (`modules/core_dsp`), a modulation module (`modules/suite_modulation`) and a UI module (`modules/suite_ui`). Each new plugin repeats the same processor and editor scaffolding by hand (see `docs/plans/00_framework-readiness.md`).

The framework idea is to make the suite a small platform:

- **Shared engines** in `core_dsp`, reused by every plugin.
- **Shared modulation** in `suite_modulation`. A plugin only supplies a target table. New modulation *sources* (for example a physics pendulum, or an external OSC source) would plug in the same way for every plugin.
- **Shared look** in `suite_ui`, with one neutral palette and one accent colour per plugin. The editor base is now shared: MicroTimingChaos's LookAndFeel derives from `SuiteUI::SuiteLookAndFeel`, sharing fonts and layout setup, while keeping MTC-specific drawings (knobs, toggles, combo box).
- **A plugin recipe**: a CMake function and a skeleton, so a new plugin is a few files and a target table, not a copy of an old plugin.
- **Shared presets and state versioning**, so one preset format works for every plugin (`docs/plans/03_presets.md`).

The platform only pays off if new plugins are cheap to make. That is the first thing to build, before most of the ideas below.

**Current status**: R1 (state versioning) and R2 (build recipe) are fully implemented. R3 (editor UI base) is complete with a minor owner-decided baseline for MicroTimingChaos's PREVIEW tab first-render effect. The suite is ready for new plugin development.

---

## 2. New plugin ideas

Each idea lists what it is, the JUCE 9 modules it would use, the framework fit, and a first step.

### 2.1 Pendulum / physics modulator (modulation source)

**What.** A modulation source whose output comes from a simulated pendulum or bouncing ball, not a periodic shape. Its phase can be driven by the host tempo or run freely. It feeds the existing six routing slots.

**JUCE modules.** `juce_box2d` (2D rigid-body physics, in the local source). A small pendulum does not need Box2D; it can be a damped ODE in `core_dsp`, with Box2D only if bouncing and collisions are wanted.

**Framework fit.** High. It is a new LFO shape or source inside `suite_modulation` and works in all three plugins once the parameter layout includes it. It needs no change to the plugins' DSP.

**Effort.** M. **First step.** A pure function in `core_dsp` with a unit test for a known period, following the pattern of `ModulationEngine.h`.

**Risk.** A new shape adds parameters. The parameter IDs are permanent (AGENTS.md section 7), so the shape must be appended to the choice list, never inserted.

### 2.9 A Stereo Tool Effect (new effect)

**What.** A stereo audio effect that processes left and right channels together: mid/side encoding, stereo width control, correlated/uncorrelated balance, and a shared DSP block (saturation, filter, or distortion) that can be applied independently or linked. Intended as a stereo widening / balance / creative tool that slots into the same framework as the existing effects.

**JUCE modules.** `juce_dsp` (mid/side splitting, all-pass filters, buffers). No new modules needed.

**Framework fit.** High. The engine goes in `core_dsp` (a shared `StereoTool` class that handles mid/side, width, and the chosen DSP operation). The UI reuses `suite_ui` widgets (knobs for width, mix, dry/wet) and the level meter. The plugin reuses the same `suite_modulation` routing if desired, but it is optional.

**Effort.** M. **First step.** A `core_dsp` test that takes a stereo signal, splits to mid/side, applies a chosen operation (e.g. tanh saturation), and reconstructs stereo — verifying the mid/side math is lossless.

**Risk.** Mid/side decoding can fold energy if the width control is pushed beyond ±100 %. Guard the control range and report latency through `getLatencySamples`.

### 2.11 A creative MIDI Chord/Arp/Scale Tool (MIDI effect)

**What.** A MIDI effect that takes incoming MIDI notes and re-harmsonises them: generates chords from a single note, arpeggiates with custom patterns, and quantises to a scale. Features: chord root/scale selector, arpeggio pattern editor (step sequence), velocity shaping, and octave transposition. Can be synced to the host tempo or run freely.

**JUCE modules.** `juce_midi` (note processing), `juce_audio_processors` (plugin format). No new modules needed.

**Framework fit.** High. This is a MIDI-effect sibling to MicroTimingChaos: it reuses the same `suite_modulation` router for any parameters (rate, swing, velocity), its UI reuses `suite_ui` widgets (pop-up scales, knob-based pattern editor), and the plugin recipe makes it a target-table-only addition. The `ModTarget` table for this plugin would include Scale, Root, and Chord quality — all append-only.

**Effort.** M. **First step.** A `core_dsp` test (pure function, no GUI) that takes a MIDI note number and a scale degree, returns the transposed note, verifying the scale-quantisation logic. Follow with a stub processor that routes a MIDI note through the chord generator and checks the output note IDs.

**Risk.** Chord generation can produce enharmonic equivalents that confuse the host. Keep the scale engine simple (diatonic only, no modes initially) and test every key-degree pair.

### 2.5 Sidechain-driven spectral ducking (extends Spectral Drift)

**What.** A sidechain input ducks the spectral drift or the gate, so the effect breathes with a kick or vocal.

**JUCE modules.** `juce_audio_processors` (sidechain buses). Nothing new.

**Framework fit.** Medium. It needs a second input bus in the processor and a bus declaration, which the MicroTimingChaos stereo bus change already shows (AGENTS.md section 7). The envelope follower already exists.

**Effort.** M. **First step.** Add the sidechain bus with no effect on the sound, and check the plugin host load test still passes.

**Risk.** Some hosts show sidechain buses awkwardly. Test in one DAW before advertising it.

### 2.4 Multiband saturation (extends Dynamic Saturation)

**What.** A version of Dynamic Saturation with two or three bands, each with its own drive and tone. It uses the existing shapers and envelope follower, split by crossovers.

**JUCE modules.** `juce_dsp` (`LinkwitzRileyFilter` for the crossovers, in the local source).

**Framework fit.** High. The shapers and `EnvelopeFollower` already exist in `SaturationEngine.h`. Linkwitz-Riley crossovers are IIR filters, so they add no latency, but the bands are not phase-linear. A linear-phase split would add latency and would need `getLatencySamples` updated.

**Effort.** M. **First step.** A two-band split in a `core_dsp` test that sums back to the input at unity.

**Risk.** Oversampling and crossovers interact. Re-check the output ceiling (AGENTS.md section 7).

### 2.10 A creative Loop Player (new instrument / effect)

**What.** A loop player that records, triggers, and plays back audio loops with configurable start/end, speed, direction (forward/backward/alternating), and quantisation to the host tempo. Can run in effect mode (processes incoming audio) or instrument mode (plays loops polyphonically). UI: a grid of loop slots, a transport row, and a length/rate editor.

**JUCE modules.** `juce_dsp` (sample-accurate position, pitch-shifting), `juce_audio_processors` (ios/audio unit compatibility). No new modules needed.

**Framework fit.** Medium. The loop player is a new plugin that reuses `suite_ui` for its grid and controls, and its DSP can share the `core_dsp` envelope follower and saturation shapers if desired. The plugin recipe makes it a target-table-only addition.

**Effort.** L to M. **First step.** A `core_dsp` test that reads a buffer from start to end with looping, verifying that a repeated 1-bar loop at double speed plays exactly two bars before the playhead laps itself.

**Risk.** Latency from buffer allocation and from the playhead position quantisation. Report it through `getLatencySamples` and the README latency table. Polyphonic loop triggering can cause voice-stealing bugs — test with many simultaneous loops.

### 2.3 Cabinet and IR loader (new effect)

**What.** A convolution cabinet or room simulator: load an impulse response, with the suite's visual style and the same Mix and Output controls.

**JUCE modules.** `juce_dsp` (convolution is already linked by the plugins).

**Framework fit.** High. The engine goes in `core_dsp`, and the UI reuses `suite_ui` widgets and the level meter.

**Effort.** M. **First step.** A `core_dsp` convolution wrapper with a test that a unit impulse returns the impulse response.

**Risk.** Latency is the IR length, or uses partitioned convolution. Report it through `getLatencySamples` and the README latency table (AGENTS.md section 9).

---

## 3. Suggested order

1. **Framework groundwork** (`docs/plans/00_framework-readiness.md`): the plugin recipe and state versioning. Both are done. The open part is R3 (`02_framework-refactor-3-editor-ui.md`). Without these, each idea costs a full copy of a plugin.
2. **2.1 Pendulum modulator.** Smallest change that tests the new modulation-source path in all three plugins.
3. **2.9 Stereo Tool Effect.** Mid/side processing; reuses core DSP and suite_ui.
4. **2.11 MIDI Chord/Arp/Scale Tool.** MIDI re-harmoniser; chord/arpeggio/scale logic.
5. **2.5 Sidechain-driven spectral ducking**, after a DAW test.
6. **2.4 Multiband saturation**, as a new plugin or a Dynamic Saturation mode.
7. **2.10 Loop Player.** Loop recording/playback; new plugin with grid UI.
8. **2.3 Cabinet and IR loader.** The first new plugin, using the new recipe.

## 4. Open questions for the owner

- Which plugin ideas do you want? Pick any number, or none.
- Should a new plugin get its own product name and plugin code now, or only once it is finished? Plugin codes are permanent once released (AGENTS.md section 7).
- **Effect 2.9 should implement its own new mid/side logic** rather than reusing Dynamic Saturation's shapers — the mid/side math is distinctive enough to warrant a dedicated core_dsp module.
- **2.10 for now only effect mode** — instrument mode (polyphonic loop triggering) can be added later once the core effect is stable and the latency is well measured.
- **2.11 everything creative** — chord generation, arpeggio patterns, and scale quantisation are all creative by nature; no need to narrow the scope.
- **2.1 could be shared in the future** — the pendulum modulator is a generic LFO shape source; it could be shared across all plugins once the parameter layout is stable, but for now it is a physics modulator only.
- **2.5 built in** — sidechain-driven spectral ducking should be built into Spectral Drift rather than as a separate plugin — the envelope follower and sidechain bus are already part of that processor's design.

## Notes for the next agent

- This is a menu. Do not start an idea without an owner choice.
- Ideas with network input need the owner's security decision first, as this file says.
- Check each idea's framework dependency (`00`, `02`, `03`) before you estimate it.
