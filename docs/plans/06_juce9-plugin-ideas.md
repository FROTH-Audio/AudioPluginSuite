# Ideas: new plugins and framework features, using JUCE 9

Written for: the owner, as a menu of options to choose from. Ideas only, nothing here is decided or scheduled.

Status: proposal. Module names were checked in the local JUCE 9.0.5 source (`cmake-build-juce9/_deps/juce-src/modules`). Effort is a rough guess for one developer who knows this codebase.

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

### 2.2 OSC modulation bus (cross-plugin and external control)

**What.** The routing module can receive modulation values from an external source over OSC (Open Sound Control), for example from a DAW script, a phone app, Max or a hardware controller. One plugin can also send its LFO values out, so a Micro-Timing Chaos LFO could drive a Spectral Drift parameter in another track.

**JUCE modules.** `juce_osc`.

**Framework fit.** Medium-high. It adds a source type to `suite_modulation`, with a network thread that writes into atomics the audio thread reads. The audio thread must never lock (AGENTS.md section 7).

**Effort.** M to L. **First step.** A send-only OSC output of the LFO value, tested with an OSC monitor, before any receive path.

**Risk.** Network input is untrusted. Bind to localhost by default and make the port a visible setting. Discuss this with the owner before a receive path is built.

### 2.3 Cabinet and IR loader (new effect)

**What.** A convolution cabinet or room simulator: load an impulse response, with the suite's visual style and the same Mix and Output controls.

**JUCE modules.** `juce_dsp` (convolution is already linked by the plugins).

**Framework fit.** High. The engine goes in `core_dsp`, and the UI reuses `suite_ui` widgets and the level meter.

**Effort.** M. **First step.** A `core_dsp` convolution wrapper with a test that a unit impulse returns the impulse response.

**Risk.** Latency is the IR length, or uses partitioned convolution. Report it through `getLatencySamples` and the README latency table (AGENTS.md section 9).

### 2.4 Multiband saturation (extends Dynamic Saturation)

**What.** A version of Dynamic Saturation with two or three bands, each with its own drive and tone. It uses the existing shapers and envelope follower, split by crossovers.

**JUCE modules.** `juce_dsp` (`LinkwitzRileyFilter` for the crossovers, in the local source).

**Framework fit.** High. The shapers and `EnvelopeFollower` already exist in `SaturationEngine.h`. Linkwitz-Riley crossovers are IIR filters, so they add no latency, but the bands are not phase-linear. A linear-phase split would add latency and would need `getLatencySamples` updated.

**Effort.** M. **First step.** A two-band split in a `core_dsp` test that sums back to the input at unity.

**Risk.** Oversampling and crossovers interact. Re-check the output ceiling (AGENTS.md section 7).

### 2.5 Sidechain-driven spectral ducking (extends Spectral Drift)

**What.** A sidechain input ducks the spectral drift or the gate, so the effect breathes with a kick or vocal.

**JUCE modules.** `juce_audio_processors` (sidechain buses). Nothing new.

**Framework fit.** Medium. It needs a second input bus in the processor and a bus declaration, which the MicroTimingChaos stereo bus change already shows (AGENTS.md section 7). The envelope follower already exists.

**Effort.** M. **First step.** Add the sidechain bus with no effect on the sound, and check the plugin host load test still passes.

**Risk.** Some hosts show sidechain buses awkwardly. Test in one DAW before advertising it.

### 2.6 Animated visualiser (UI)

**What.** Smooth transitions when the owner switches the SOUND and MOD pages, and when a preset is loaded.

**JUCE modules.** `juce_animation`.

**Framework fit.** Medium. It goes in `suite_ui` and is shared by every plugin.

**Effort.** S to M. **First step.** Animate only the page switch, then check the editor render baseline.

**Risk.** Animation changes what a headless render produces. The byte-identical render check (AGENTS.md sections 8 and 9) would need a new baseline. Do this after the renderer measurement (`docs/plans/01_juce9-upgrade-opportunities.md`, item 1).

### 2.7 Scripted modulation curves (idea, deferred)

**What.** Users write small expressions or scripts that become modulation curves, editing them in the editor.

**JUCE modules.** `juce_javascript`.

**Framework fit.** Low for now. A script engine would have to run off the audio thread, and its results would have to reach the audio thread through atomics, as the rest of the UI does.

**Effort.** L. **Recommendation.** Not now. It adds a large surface for bugs and for security review.

### 2.8 Other modules, not recommended

- `juce_video`: no use in an audio effect.
- `juce_analytics`: collects usage data. Not recommended for a privacy-minded suite, and it would need a privacy notice.
- `juce_product_unlocking`: relevant only if the suite is sold. The licence note in AGENTS.md section 10 says GPL redistribution uses the JUCE GPLv3 option. Raise it with the owner before choosing any unlock scheme.
- `juce_midi_ci`: MIDI capability negotiation, aimed at controllers and synths. No fit here.

---

## 3. Suggested order

1. **Framework groundwork** (`docs/plans/00_framework-readiness.md`): the plugin recipe and state versioning. Both are done. The open part is R3 (`02_framework-refactor-3-editor-ui.md`). Without these, each idea costs a full copy of a plugin.
2. **2.1 Pendulum modulator.** Smallest change that tests the new modulation-source path in all three plugins.
3. **2.3 Cabinet and IR loader.** The first new plugin, using the new recipe.
4. **2.2 OSC bus**, after a security decision from the owner.
5. **2.4 Multiband saturation**, as a new plugin or a Dynamic Saturation mode.
6. **2.5 and 2.6** as time allows.

## 4. Open questions for the owner

- Which plugin ideas do you want? Pick any number, or none.
- Is network control (2.2) acceptable, with a localhost-only default?
- Should a new plugin get its own product name and plugin code now, or only once it is finished? Plugin codes are permanent once released (AGENTS.md section 7).

## Notes for the next agent

- This is a menu. Do not start an idea without an owner choice.
- Ideas with network input (2.2, OSC) need the owner's security decision first, as this file says.
- Check each idea's framework dependency (`00`, `02`, `03`) before you estimate it.
