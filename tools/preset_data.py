"""Authoring data for the AudioSuite factory bank.

`SCHEMA` lists every parameter the three plugins expose, with its range and default, so
`build_bank.py` and `audit_bank.py` can validate a preset without loading the plugin.
`PRESETS` holds the deltas from the default state. Both are the source of truth; the JSON files
under `bank/<plugin>/` and the combined `bank/AudioSuitePresets.xml` are generated from them.

Keep the schema in step with `createParameterLayout()` in each processor and with AGENTS.md
section 4. The C++ test `bankRanges` checks the same rules against the live APVTS, so a drift here
is caught at build time in a bank build.

`samplePath` is **not** a parameter and must never be a SCHEMA key (plan 07 phase 3): it travels
as a `<PARAMETERS>` attribute. Since phase 4 the bank may carry `embedded:` URIs (revised Q3) —
set `sample_path=` on a Sgraffito `_preset`; a file path is rejected by the audit. The
schema-coverage check would flag a SCHEMA entry as having no live parameter.
"""


def _routing(target_max):
    """LFO and modulation-routing parameters shared by all three plugins."""
    params = {}

    for index in (1, 2):
        params[f"lfo{index}Rate"] = {"min": 0.02, "max": 20.0, "default": 0.25, "type": "float"}
        params[f"lfo{index}Shape"] = {"min": 0, "max": 6, "default": 0, "type": "choice"}
        params[f"lfo{index}Sync"] = {"min": 0, "max": 1, "default": 0, "type": "bool"}
        params[f"lfo{index}Division"] = {"min": 0, "max": 6, "default": 4, "type": "choice"}
        params[f"lfo{index}Offset"] = {"min": -100.0, "max": 100.0, "default": 0.0, "type": "float"}

    for slot in range(1, 7):
        params[f"mod{slot}Source"] = {"min": 0, "max": 1, "default": 0, "type": "choice"}
        params[f"mod{slot}Target"] = {"min": 0, "max": target_max, "default": 0, "type": "choice"}
        params[f"mod{slot}Amount"] = {"min": -100.0, "max": 100.0, "default": 0.0, "type": "float"}
        params[f"mod{slot}Delay"] = {"min": 0.0, "max": 10.0, "default": 0.0, "type": "float"}

    return params


SCHEMA = {
    "SpectralDrift": {
        "depth": {"min": 0.0, "max": 100.0, "default": 19.1, "type": "float"},
        "spread": {"min": 0.0, "max": 1.0, "default": 0.05, "type": "float"},
        "stereoPhase": {"min": 0.0, "max": 180.0, "default": 90.0, "type": "float"},
        "shift": {"min": -32.0, "max": 32.0, "default": 0.0, "type": "float"},
        "tilt": {"min": -2.0, "max": 2.0, "default": 0.0, "type": "float"},
        "gate": {"min": -90.0, "max": 0.0, "default": -90.0, "type": "float"},
        "feedback": {"min": 0.0, "max": 12.0, "default": 0.0, "type": "float"},
        "mix": {"min": 0.0, "max": 100.0, "default": 100.0, "type": "float"},
        "freeze": {"min": 0, "max": 1, "default": 0, "type": "bool"},
        "feedbackDamping": {"min": 0.0, "max": 100.0, "default": 30.0, "type": "float"},
        "driftStart": {"min": 0.0, "max": 100.0, "default": 0.0, "type": "float"},
        "outputGain": {"min": -12.0, "max": 12.0, "default": 0.0, "type": "float"},
        # Plan 06 idea 2.5: the sidechain ducking pair (appended after the routing).
        "sidechainAmount": {"min": 0.0, "max": 100.0, "default": 0.0, "type": "float"},
        "sidechainTarget": {"min": 0, "max": 1, "default": 0, "type": "choice"},
        **_routing(12),
    },
    "DynamicSaturation": {
        "drive": {"min": 0.0, "max": 24.0, "default": 6.0, "type": "float"},
        "sensitivity": {"min": 0.0, "max": 1.0, "default": 0.5, "type": "float"},
        "mode": {"min": 0, "max": 2, "default": 0, "type": "choice"},
        "mix": {"min": 0.0, "max": 100.0, "default": 100.0, "type": "float"},
        "outputLevel": {"min": -12.0, "max": 6.0, "default": 0.0, "type": "float"},
        "attack": {"min": 1.0, "max": 100.0, "default": 5.0, "type": "float"},
        "release": {"min": 50.0, "max": 2000.0, "default": 150.0, "type": "float"},
        "detectHpf": {"min": 20.0, "max": 800.0, "default": 20.0, "type": "float"},
        "stereoLink": {"min": 0.0, "max": 100.0, "default": 100.0, "type": "float"},
        "bias": {"min": 0.0, "max": 0.25, "default": 0.2, "type": "float"},
        "fold": {"min": 0.0, "max": 50.0, "default": 0.0, "type": "float"},
        "tone": {"min": -1.0, "max": 1.0, "default": 0.0, "type": "float"},
        "curve": {"min": 0.5, "max": 3.0, "default": 1.0, "type": "float"},
        **_routing(12),
    },
    "MicroTimingChaos": {
        "rate": {"min": 0, "max": 3, "default": 2, "type": "choice"},
        "length": {"min": 2, "max": 16, "default": 8, "type": "int"},
        "density": {"min": 0.0, "max": 100.0, "default": 70.0, "type": "float"},
        "seed": {"min": 0, "max": 999, "default": 42, "type": "int"},
        "gate": {"min": 10.0, "max": 100.0, "default": 60.0, "type": "float"},
        "swing": {"min": -20.0, "max": 20.0, "default": 0.0, "type": "float"},
        "baseVelocity": {"min": 30, "max": 127, "default": 100, "type": "int"},
        "root": {"min": 24, "max": 72, "default": 48, "type": "int"},
        "scale": {"min": 0, "max": 5, "default": 2, "type": "choice"},
        "octaves": {"min": 1, "max": 3, "default": 2, "type": "int"},
        "chaos": {"min": 0.0, "max": 100.0, "default": 50.0, "type": "float"},
        "timingRange": {"min": 0.0, "max": 50.0, "default": 20.0, "type": "float"},
        "velocityChaos": {"min": 0.0, "max": 100.0, "default": 40.0, "type": "float"},
        "lerp": {"min": 0.0, "max": 100.0, "default": 50.0, "type": "float"},
        "lerpXMult": {"min": 0.0, "max": 2.0, "default": 1.0, "type": "float"},
        "lerpYMult": {"min": 0.0, "max": 2.0, "default": 1.0, "type": "float"},
        "lerpZMult": {"min": 0.0, "max": 2.0, "default": 1.0, "type": "float"},
        "bpm": {"min": 40.0, "max": 240.0, "default": 120.0, "type": "float"},
        "sync": {"min": 0, "max": 1, "default": 0, "type": "bool"},
        "speed": {"min": 0.5, "max": 2.0, "default": 1.0, "type": "float"},
        **_routing(14),
    },
    "Sgraffito": {
        "mode": {"min": 0, "max": 3, "default": 0, "type": "choice"},
        "regionSpan": {"min": 0.0, "max": 100.0, "default": 100.0, "type": "float"},
        "regionLength": {"min": 1.0, "max": 100.0, "default": 25.0, "type": "float"},
        "rootKey": {"min": 24, "max": 96, "default": 60, "type": "int"},
        "pitchFine": {"min": -100.0, "max": 100.0, "default": 0.0, "type": "float"},
        "scaleLock": {"min": 0, "max": 1, "default": 0, "type": "bool"},
        "scale": {"min": 0, "max": 5, "default": 2, "type": "choice"},
        "glide": {"min": 0.0, "max": 500.0, "default": 50.0, "type": "float"},
        "level": {"min": -60.0, "max": 6.0, "default": 0.0, "type": "float"},
        "velocityCurve": {"min": 0.5, "max": 2.0, "default": 1.0, "type": "float"},
        "attack": {"min": 1.0, "max": 500.0, "default": 5.0, "type": "float"},
        "release": {"min": 10.0, "max": 5000.0, "default": 200.0, "type": "float"},
        "tapeStopTime": {"min": 0.0, "max": 5000.0, "default": 400.0, "type": "float"},
        "tapeStopCurve": {"min": 0, "max": 2, "default": 0, "type": "choice"},
        "memoryAmount": {"min": 0.0, "max": 100.0, "default": 20.0, "type": "float"},
        "memoryDecay": {"min": 0.0, "max": 60.0, "default": 10.0, "type": "float"},
        "driftAmount": {"min": 0.0, "max": 100.0, "default": 0.0, "type": "float"},
        "driftSpeed": {"min": 0.01, "max": 2.0, "default": 0.1, "type": "float"},
        "layBack": {"min": 0.0, "max": 30.0, "default": 0.0, "type": "float"},
        "swing": {"min": 50.0, "max": 75.0, "default": 50.0, "type": "float"},
        "humanise": {"min": 0.0, "max": 15.0, "default": 0.0, "type": "float"},
        "ghostOn": {"min": 0, "max": 1, "default": 0, "type": "bool"},
        "ghostThreshold": {"min": 1.0, "max": 100.0, "default": 40.0, "type": "float"},
        "ghostLevel": {"min": -40.0, "max": 0.0, "default": -12.0, "type": "float"},
        "ghostDelay": {"min": 0.0, "max": 50.0, "default": 15.0, "type": "float"},
        "sliceCount": {"min": 2, "max": 32, "default": 16, "type": "int"},
        "grainSize": {"min": 5.0, "max": 300.0, "default": 60.0, "type": "float"},
        "grainDensity": {"min": 1.0, "max": 60.0, "default": 12.0, "type": "float"},
        "grainSpread": {"min": 0.0, "max": 100.0, "default": 40.0, "type": "float"},
        "sustainHold": {"min": 0, "max": 1, "default": 0, "type": "bool"},
        "swellTime": {"min": 50.0, "max": 4000.0, "default": 800.0, "type": "float"},
        "swellCrossfade": {"min": 5.0, "max": 200.0, "default": 40.0, "type": "float"},
        "dust": {"min": 0.0, "max": 100.0, "default": 0.0, "type": "float"},
        "crackle": {"min": 0.0, "max": 100.0, "default": 0.0, "type": "float"},
        "wow": {"min": 0.0, "max": 100.0, "default": 0.0, "type": "float"},
        "flutter": {"min": 0.0, "max": 100.0, "default": 0.0, "type": "float"},
        "shuffleSeed": {"min": 0, "max": 9999, "default": 1, "type": "int"},
        **_routing(24),
    },
    # Plan 6.1: the two first-pass plugins joined the bank, 15 presets each.
    "MidiEffect": {
        "root": {"min": 24, "max": 84, "default": 60, "type": "int"},
        "scale": {"min": 0, "max": 5, "default": 2, "type": "choice"},
        "chord": {"min": 0, "max": 7, "default": 0, "type": "choice"},
        "arp": {"min": 0, "max": 4, "default": 0, "type": "choice"},
        "octave": {"min": -2, "max": 2, "default": 0, "type": "int"},
        "velocity": {"min": 0.0, "max": 150.0, "default": 100.0, "type": "float"},
        "arpMode": {"min": 0, "max": 1, "default": 0, "type": "choice"},
        # Plan 12: twelve step rates in the clean order (0=1/4 ... 11=1/32D). The old
        # authoring indices (0=1/8, 1=1/8T, 2=1/16, 3=1/16T, 4=1/32) were remapped, so
        # each preset still means the note value it was written with.
        "stepRate": {"min": 0, "max": 11, "default": 6, "type": "choice"},
        "hostSync": {"min": 0, "max": 1, "default": 0, "type": "choice"},
        "tempo": {"min": 40.0, "max": 240.0, "default": 120.0, "type": "float"},
        "gateLength": {"min": 10.0, "max": 100.0, "default": 60.0, "type": "float"},
        "velocityCurve": {"min": 0, "max": 3, "default": 0, "type": "choice"},
        "velocityDirection": {"min": 0, "max": 1, "default": 1, "type": "choice"},
        "octaveSpread": {"min": -1.0, "max": 1.0, "default": 0.0, "type": "float"},
    },
    "StereoTool": {
        "width": {"min": -100.0, "max": 100.0, "default": 0.0, "type": "float"},
        "drive": {"min": -24.0, "max": 24.0, "default": 0.0, "type": "float"},
        "mix": {"min": 0.0, "max": 100.0, "default": 100.0, "type": "float"},
        "character": {"min": 0, "max": 4, "default": 0, "type": "choice"},
        "transientAttack": {"min": 1.0, "max": 200.0, "default": 5.0, "type": "float"},
        "transientRelease": {"min": 20.0, "max": 2000.0, "default": 150.0, "type": "float"},
        "transientAmount": {"min": 0.0, "max": 100.0, "default": 0.0, "type": "float"},
        "transientTarget": {"min": 0, "max": 2, "default": 2, "type": "choice"},
        "midFocus": {"min": -100.0, "max": 100.0, "default": 0.0, "type": "float"},
        "sideFilter": {"min": 0, "max": 3, "default": 0, "type": "choice"},
        "sideCrossover": {"min": 20.0, "max": 8000.0, "default": 200.0, "type": "float"},
        # Plan 12: the level stage. The recorded decision holds: levels/gains are never
        # preset-authored, so the schema only covers them for the audit.
        "inputLevel": {"min": -24.0, "max": 24.0, "default": 0.0, "type": "float"},
        "outputLevel": {"min": -24.0, "max": 24.0, "default": 0.0, "type": "float"},
    },
    # Plan 11: Multiband Saturation, the two-band split (11 parameters; the shared Router's
    # parameters are not in the layout yet, so no routing entries).
    "MultibandSaturation": {
        "mode": {"min": 0, "max": 2, "default": 0, "type": "choice"},
        "crossover": {"min": 80.0, "max": 8000.0, "default": 500.0, "type": "float"},
        "driveLow": {"min": 0.0, "max": 24.0, "default": 6.0, "type": "float"},
        "driveHigh": {"min": 0.0, "max": 24.0, "default": 6.0, "type": "float"},
        "sensitivity": {"min": 0.0, "max": 1.0, "default": 0.5, "type": "float"},
        "attack": {"min": 1.0, "max": 100.0, "default": 5.0, "type": "float"},
        "release": {"min": 50.0, "max": 2000.0, "default": 150.0, "type": "float"},
        "toneLow": {"min": -1.0, "max": 1.0, "default": 0.0, "type": "float"},
        "toneHigh": {"min": -1.0, "max": 1.0, "default": 0.0, "type": "float"},
        "mix": {"min": 0.0, "max": 100.0, "default": 100.0, "type": "float"},
        "outputLevel": {"min": -12.0, "max": 6.0, "default": 0.0, "type": "float"},
    },
    # Plan 6.1: Cyclical, the four-slot loop instrument (35 parameters).
    "Cyclical": {
        "masterVolume": {"min": -24.0, "max": 12.0, "default": 0.0, "type": "float"},
        "pan": {"min": -100.0, "max": 100.0, "default": 0.0, "type": "float"},
        "quantize": {"min": 0, "max": 5, "default": 0, "type": "choice"},
        "swing": {"min": -20.0, "max": 20.0, "default": 0.0, "type": "float"},
        "triggerMode": {"min": 0, "max": 1, "default": 0, "type": "choice"},
        **{f"slot{slot}{field}": spec
           for slot in (1, 2, 3, 4)
           for field, spec in {
               "Start": {"min": 0.0, "max": 100.0, "default": 0.0, "type": "float"},
               "Length": {"min": 5.0, "max": 100.0, "default": 100.0, "type": "float"},
               "Speed": {"min": 0.25, "max": 4.0, "default": 1.0, "type": "float"},
               "Direction": {"min": 0, "max": 2, "default": 0, "type": "choice"},
               "PingRandom": {"min": 0, "max": 1, "default": 0, "type": "bool"},
               "Granular": {"min": 0, "max": 1, "default": 0, "type": "bool"},
               "Velocity": {"min": 30.0, "max": 150.0, "default": 100.0, "type": "float"},
           }.items()},
        "pitchShift": {"min": -12, "max": 12, "default": 0, "type": "int"},
        "crossfadeLength": {"min": 0.0, "max": 500.0, "default": 5.0, "type": "float"},
    },
}


def _preset(name, category, description, lerp_curve=None, sample_path=None, loop_paths=None, **params):
    preset = {"name": name, "category": category, "description": description, "params": params}

    # Micro-Timing Chaos only: the Lerp curve is a state property, not a parameter.
    if lerp_curve is not None:
        preset["lerpCurve"] = lerp_curve

    # Sgraffito only (plan 07 phase 4): a sample reference. Only `embedded:` URIs are
    # allowed in the bank (revised Q3); the audit rejects anything else.
    if sample_path is not None:
        preset["samplePath"] = sample_path

    # Cyclical only (plan 6.1): the four slot loops. Only `embedded:` URIs are allowed;
    # the audit rejects anything else.
    if loop_paths is not None:
        preset["loopPaths"] = loop_paths

    return preset


PRESETS = {
    "SpectralDrift": [
        _preset("Glass Cathedral", "Ambient", "Slow, wide drift with a bright upper shift.", lfo1Rate=0.12, mod1Target=12, mod1Amount=100.0, spread=0.75, stereoPhase=180.0, shift=12.0, gate=-60.0),
        _preset("Slow Melt", "Ambient", "Deep, slow movement with a short feedback tail.", lfo1Rate=0.15, mod1Target=12, mod1Amount=100.0, depth=80.0, feedback=8.0, feedbackDamping=60.0),
        _preset("Bin Wash", "Ambient", "Wide bin spread and a dark tilt.", spread=0.9, tilt=-1.0, mix=80.0),
        _preset("Deep Space", "Ambient", "The slowest drift, full depth, wide stereo.", lfo1Rate=0.1, mod1Target=12, mod1Amount=100.0, depth=100.0, stereoPhase=180.0, feedback=6.0),
        _preset("Frozen Choir", "Freeze", "Holds a soft spectrum with a gentle gate.", freeze=1, depth=30.0, gate=-50.0),
        _preset("Frozen Static", "Freeze", "Holds the spectrum with no drift.", freeze=1, depth=0.0, gate=-30.0),
        _preset("Metallic Sheen", "Bright", "Bright tilt with a small upward shift.", tilt=1.5, shift=7.0, spread=0.1),
        _preset("Air Shifter", "Bright", "Large upward shift, partly wet.", shift=24.0, mix=60.0, tilt=0.8),
        _preset("Top Sparkle", "Bright", "Bright tilt with a high gate.", gate=-70.0, tilt=1.0),
        _preset("Whisper Drift", "Subtle", "A very small, slow drift.", depth=8.0, lfo1Rate=0.3, mod1Target=12, mod1Amount=100.0, stereoPhase=20.0),
        _preset("Mono Anchor", "Subtle", "Almost no stereo spread.", stereoPhase=0.0, spread=0.01),
        _preset("Wide Bloom", "Wide", "Wide stereo with a moderate depth.", stereoPhase=180.0, spread=0.9, depth=40.0),
        _preset("Phase Tunnel", "Wide", "Full depth and a long feedback path.", depth=100.0, spread=0.6, feedback=10.0),
        _preset("Fast Steps", "Rhythmic", "Fast drift at 2 Hz.", lfo1Rate=2.0, mod1Target=12, mod1Amount=100.0, depth=40.0),
        _preset("Wide Sweep", "Rhythmic", "Slow sweep across the bins.", lfo1Rate=0.5, mod1Target=12, mod1Amount=100.0, spread=0.3),
        _preset("Deep Tide", "Rhythmic", "Slow drift with a deep sweep.", lfo1Rate=0.25, mod1Target=12, mod1Amount=100.0, depth=60.0),
        _preset("Comb Weaver", "Character", "Maximum feedback, lightly damped.", feedback=12.0, feedbackDamping=15.0),
        _preset("Damped Echo", "Character", "Feedback with a heavy low-pass.", feedback=10.0, feedbackDamping=90.0),
        _preset("Vocal Ghost", "Character", "A small shift and tilt, partly wet.", shift=3.0, tilt=0.3, depth=25.0),
        _preset("Low Anchor", "Mix", "Only the upper part of the spectrum drifts.", driftStart=60.0, shift=-12.0),
        _preset("Dark Matter", "Dark", "Dark tilt with a low gate.", tilt=-2.0, gate=-80.0),
        _preset("Sub Rumble", "Dark", "Large downward shift, dark tilt.", shift=-32.0, tilt=-1.5),
        _preset("Broken Radio", "Lo-Fi", "Gated, undamped feedback.", gate=-40.0, feedback=12.0, feedbackDamping=0.0),
        _preset("Fast Chatter", "Lo-Fi", "Fast drift with a wide spread.", lfo1Rate=8.0, mod1Target=12, mod1Amount=100.0, spread=0.9, shift=5.0),
        _preset("Null Test", "Utility", "No drift, no shaping: a transparent reference.", depth=0.0),
        _preset("Init Bright", "Utility", "Near-default with a little brightness.", tilt=0.5),
    ],
    "DynamicSaturation": [
        _preset("Tube Glue", "Mix bus", "Gentle tube glue for a mix bus.", drive=4.0, sensitivity=0.4),
        _preset("Tape Warmth", "Mix bus", "Soft tape saturation with a warm top.", mode=1, drive=3.0, tone=-0.3),
        _preset("Diode Edge", "Mix bus", "A little diode edge on the mix.", mode=2, drive=5.0, tone=0.2),
        _preset("Mixbus Glue II", "Mix bus", "Slower envelope, stronger drive.", drive=3.0, sensitivity=0.6, curve=1.5, mode=1),
        _preset("Gentle Touch", "Subtle", "Barely there, parallel.", drive=1.0, sensitivity=0.2, mix=40.0),
        _preset("Clean Lift", "Subtle", "Clean tape lift at full mix.", drive=2.0, mode=1),
        _preset("Vocal Leveller", "Vocal", "Vocal-friendly detector and envelope.", attack=8.0, release=120.0, detectHpf=90.0, curve=1.2),
        _preset("Mid Forward", "Character", "Forward mids with a touch of folding.", tone=0.5, drive=5.0, fold=5.0),
        _preset("Bias Blues", "Character", "Asymmetric tube bias with more drive.", bias=0.25, drive=9.0, tone=-0.2),
        _preset("Tape Stop", "Character", "Heavily saturated, dark tape.", mode=1, drive=14.0, tone=-1.0),
        _preset("Bass Growl", "Bass", "Low detector and a dark tilt.", drive=10.0, detectHpf=30.0, tone=-0.5),
        _preset("Sub Tightener", "Bass", "Fast envelope for low-end control.", detectHpf=40.0, attack=2.0, release=80.0),
        _preset("Drum Smash", "Drums", "Fast, heavy drum saturation.", drive=18.0, attack=1.0, release=60.0, mix=70.0),
        _preset("Snare Bite", "Drums", "Bias and fold for snare attack.", drive=12.0, bias=0.1, fold=10.0, tone=0.4),
        _preset("Kick Punch", "Drums", "Short envelope, strong curve.", drive=8.0, attack=1.0, release=50.0, curve=2.0),
        _preset("Parallel Crunch", "Parallel", "Heavy diode crunch in parallel.", mix=35.0, drive=20.0, mode=2),
        _preset("Air Exciter", "Bright", "High-passed brightness at half mix.", drive=4.0, tone=0.8, detectHpf=200.0, mix=50.0),
        _preset("Slow Bloom", "Ambient", "Very slow attack and release.", attack=100.0, release=2000.0, drive=12.0),
        _preset("Release Tail", "Ambient", "Long release, partly wet.", release=1200.0, mix=60.0),
        _preset("Full Send", "Extreme", "Maximum drive and bias.", drive=24.0, fold=40.0, bias=0.25),
        _preset("Fold Chaos", "Extreme", "Maximum folding into a diode stage.", fold=50.0, drive=16.0, mode=2),
        _preset("Lo-Fi Grit", "Lo-Fi", "Folded and darkened.", fold=25.0, drive=15.0, tone=-0.6),
        _preset("Detector Focus", "Utility", "High detector, unlinked.", detectHpf=300.0, stereoLink=0.0),
        _preset("Stereo Split", "Utility", "Unlinked envelopes with light drive.", stereoLink=0.0, drive=7.0),
        _preset("Transient Keeper", "Utility", "Slow attack, fast release.", attack=30.0, release=50.0, sensitivity=0.1),
        _preset("Null Reference", "Utility", "Fully dry: a null reference.", drive=0.0, mix=0.0),
    ],
    "MicroTimingChaos": [
        _preset("Slow Orbit", "Ambient", "Slow, sparse orbit.", speed=0.5, lerp=40.0, density=50.0),
        _preset("Deep Space", "Ambient", "Low root, three octaves, long gate.", root=36, octaves=3, gate=80.0),
        _preset("Long Gate Pad", "Ambient", "Long gates, sparse and soft.", gate=100.0, density=40.0, baseVelocity=70),
        _preset("Dust Motes", "Sparse", "Very sparse with the widest timing range.", density=20.0, timingRange=50.0, baseVelocity=30, seed=5),
        _preset("Ghost Notes", "Sparse", "Sparse, quiet and very uneven.", density=15.0, baseVelocity=40, velocityChaos=80.0, seed=1),
        _preset("Broken Clock", "Glitch", "Fast, dense and heavily swung.", swing=20.0, rate=3, density=90.0),
        _preset("Stutter Steps", "Glitch", "Short gates at 1/32.", rate=3, gate=15.0, timingRange=10.0),
        _preset("Off Grid", "Glitch", "Maximum swing and timing range.", swing=20.0, timingRange=50.0),
        _preset("Machine Pulse", "Techno", "Steady, loud and dense.", density=80.0, baseVelocity=110),
        _preset("Warehouse", "Techno", "Low pentatonic with more chaos.", root=36, scale=3, chaos=70.0),
        _preset("Acid Ladder", "Techno", "One octave, short gates.", octaves=1, gate=30.0),
        _preset("Tight Techno", "Techno", "Very short gates and tight timing.", gate=25.0, timingRange=5.0),
        _preset("Polyrhythm", "Rhythmic", "A 13-step pattern.", length=13),
        _preset("Triplet Drift", "Rhythmic", "12 steps with a negative swing.", length=12, swing=-10.0),
        _preset("Half Time", "Rhythmic", "Eighth notes.", rate=1),
        _preset("Double Time", "Rhythmic", "1/32 notes with short gates.", rate=3, gate=40.0),
        _preset("Dorian Walk", "Melodic", "Dorian melody around F#3.", scale=4, root=50),
        _preset("Pentatonic Bells", "Melodic", "High pentatonic with long gates.", scale=3, root=60, gate=90.0),
        _preset("Whole Tone Fall", "Melodic", "Whole-tone melody.", scale=5, root=55),
        _preset("Glass Rain", "Melodic", "High whole-tone notes with short gates.", scale=5, root=60, gate=40.0),
        _preset("Chromatic Chaos", "Experimental", "Every semitone, three octaves, full chaos.", scale=0, octaves=3, chaos=100.0),
        _preset("Lorenz Storm", "Experimental", "Full chaos and every drift multiplier.",
                lerp_curve="0.0000,0.0000;0.2500,0.2000;0.5000,0.5000;0.7500,0.8000;1.0000,1.0000",
                chaos=100.0, lerp=100.0, lerpXMult=2.0, lerpYMult=2.0, lerpZMult=2.0),
        _preset("LFO Wobble", "Experimental", "Synced LFO moves the chaos amount.", lfo1Sync=1, mod1Target=1, mod1Amount=60.0),
        _preset("Random Walk", "Experimental", "Random LFO moves the seed after 2 s.", lfo1Shape=5, mod1Target=14, mod1Amount=40.0, mod1Delay=2.0),
        _preset("S&H Pitch", "Experimental", "Sample and hold moves the chaos.", lfo2Shape=4, mod1Source=1, mod1Target=1, mod1Amount=50.0),
        _preset("Frozen Pattern", "Utility", "The pattern without any drift.", lerp=0.0),
        _preset("Seed 42 Dense", "Utility", "The default seed, much denser.", density=90.0),
        _preset("Tempo Rider", "Utility", "Triangle LFO moves the tempo.", lfo2Shape=1, mod1Source=1, mod1Target=13, mod1Amount=30.0),
    ],
    "Sgraffito": [
        _preset("Glass Dust", "Ambient", "Slow, sparse cloud with long decay.", sample_path="embedded:glass_dust_4k.wav", grainSize=90.0, grainDensity=8.0, grainSpread=55.0, memoryDecay=8.0, release=800.0),
        _preset("Deep Cloud", "Ambient", "Low pentatonic, dense and slow.", sample_path="embedded:deep_cloud_4k.wav", rootKey=84, scale=4, grainSize=140.0, grainDensity=16.0, grainSpread=30.0, attack=120.0, release=1500.0),
        _preset("Memory Ghosts", "Sparse", "Ghost notes over a quiet memory trace.", sample_path="embedded:memory_ghosts_4k.wav", mode=1, sliceCount=4, ghostOn=1, ghostThreshold=90.0, memoryAmount=35.0, release=900.0),
        _preset("Tape Break", "Glitch", "Chopped tape stop with dust and crackle.", sample_path="embedded:tape_break_4k.wav", mode=1, sliceCount=8, tapeStopTime=300.0, dust=5.0, crackle=8.0, memoryAmount=45.0, memoryDecay=6.0),
        _preset("Cloud Rain", "Melodic", "High pentatonic with long gates and sparse grains.", sample_path="embedded:cloud_rain_4k.wav", scale=3, grainSize=120.0, grainDensity=4.0, grainSpread=60.0, release=2000.0),
        _preset("Scrubbed Memory", "Scratch", "Quick region scrub with humanised timing.", sample_path="embedded:scrubbed_memory_4k.wav", mode=0, regionSpan=80.0, humanise=15.0, layBack=12.0, dust=6.0),
        _preset("Chopped Sticks", "Rhythmic", "Slice a sample into many short notes.", sample_path="embedded:chopped_sticks_4k.wav", mode=1, sliceCount=8, release=120.0, velocityCurve=1.3),
        _preset("Swell Pad", "Pads", "Long soft swells with surface noise.", sample_path="embedded:swell_pad_4k.wav", mode=3, attack=250.0, release=3000.0, dust=15.0, memoryAmount=30.0, memoryDecay=20.0),
        _preset("Cloud Dust", "Textural", "Dust and flutter over a sparse cloud.", sample_path="embedded:cloud_dust_4k.wav", grainSize=80.0, grainDensity=6.0, grainSpread=40.0, dust=10.0, flutter=2.0, crackle=4.0),
        _preset("Wobble Memory", "Experimental", "LFO 1 bends the pitch centre over a wobbled sample.", sample_path="embedded:wobble_memory_4k.wav", lfo1Sync=1, lfo1Division=3, mod1Target=3, mod1Amount=30.0, wow=20.0),
        _preset("Ghost Delay", "Echo", "Ghost notes repeating quietly under the memory decay.", sample_path="embedded:ghost_delay_4k.wav", ghostOn=1, ghostThreshold=75.0, ghostDelay=45.0, memoryAmount=40.0, memoryDecay=8.0),
        _preset("Swelling Cloud", "Evolution", "Starts small, swells to a dense cloud.", sample_path="embedded:swelling_cloud_4k.wav", grainSize=45.0, grainDensity=20.0, grainSpread=70.0, attack=200.0, release=1500.0),
        _preset("Choppy Scratch", "Scratch", "Fast chops with pitch fine control.", sample_path="embedded:choppy_scratch_4k.wav", mode=1, sliceCount=16, pitchFine=40.0, release=100.0, humanise=8.0),
        _preset("Drifted Memory", "Ambient", "A memory trace that fades slowly across the sample.", sample_path="embedded:drifted_memory_4k.wav", regionSpan=70.0, regionLength=60.0, memoryAmount=60.0, memoryDecay=25.0, wow=10.0),
        _preset("Init Cloud", "Utility", "A clean reference: 60% span, 40% length.", sample_path="embedded:init_cloud_4k.wav", regionSpan=60.0, regionLength=40.0),
        _preset("Velvet Swing", "Rhythmic", "Swung repeats with a soft attack and release.", sample_path="embedded:velvet_swing_4k.wav", swing=65.0, attack=80.0, release=400.0, layBack=8.0, memoryAmount=25.0),
        _preset("Ghostly Echo", "Echo", "Ghost notes repeating quietly under the memory decay.", sample_path="embedded:ghostly_echo_4k.wav", ghostOn=1, ghostThreshold=70.0, ghostLevel=-15.0, ghostDelay=30.0, attack=40.0, release=300.0, crackle=5.0),
        _preset("Held Breath", "Pads", "Latched voices holding through the memory decay.", sample_path="embedded:held_breath_4k.wav", sustainHold=1, memoryAmount=40.0, memoryDecay=30.0, attack=150.0, release=2500.0),
        _preset("Fine Grained Drift", "Experimental", "Fine pitch and velocity shaping over a drifting memory.", sample_path="embedded:fine_grained_drift_4k.wav", pitchFine=40.0, velocityCurve=1.5, memoryAmount=35.0, memoryDecay=15.0, attack=60.0, release=600.0, flutter=5.0),
        _preset("Slow Attack Tape", "Glitch", "Tape stop that eases in with a long attack and release.", sample_path="embedded:slow_attack_tape_4k.wav", mode=1, sliceCount=12, tapeStopTime=250.0, tapeStopCurve=1, attack=200.0, release=1200.0, dust=5.0, wow=8.0),
    ],
    # Plan 6.1: the chord/arp engine. Every preset runs arpMode=Arp (1), so none equals
    # the default (Chord) state. Bank order is append-only.
    "MidiEffect": [
        _preset("Classic 8-Step", "Arp", "Even 16ths at the default gate and velocity.", arpMode=1),
        _preset("Descending Runner", "Arp", "Fast descending 8ths with a falling spread.", arpMode=1, stepRate=3, gateLength=50.0, velocityCurve=1, octaveSpread=-0.5),
        _preset("Scatter Chaos", "Experimental", "Scattered 32nds with random velocity.", arpMode=1, arp=4, stepRate=9, gateLength=70.0, velocityCurve=3),
        _preset("Bass Octave Pump", "Bass", "A short octave crawl one octave down.", arpMode=1, stepRate=3, gateLength=80.0, octaveSpread=0.5, octave=-1),
        _preset("Laid-Back Keys", "R&B", "Soft decaying 16ths, slightly hot.", arpMode=1, gateLength=55.0, velocityCurve=2, velocity=120.0),
        _preset("Triplet Cascade", "Funk", "Rising 8th-note triplets.", arpMode=1, stepRate=4, gateLength=65.0, velocityCurve=1, velocityDirection=0, octaveSpread=0.2),
        _preset("Minor Drift", "Ambient", "Sparse minor 16ths, spread down.", arpMode=1, gateLength=40.0, velocityCurve=2, octaveSpread=-0.3),
        _preset("Dotted Drive", "Techno", "Fast 32nds, spread up.", arpMode=1, stepRate=9, gateLength=75.0, octaveSpread=0.4),
        _preset("Piano Lift", "Classical", "Rising linear velocity over 16ths.", arpMode=1, velocityCurve=1, velocityDirection=0, octaveSpread=0.2),
        _preset("Guitar Roll", "Rock", "Rolling 16ths under a major 7.", arpMode=1, chord=7, gateLength=50.0, velocityCurve=1, octaveSpread=-0.2),
        _preset("Deep Space", "Ambient", "Slow random 32nds, wide and high.", arpMode=1, stepRate=9, gateLength=45.0, velocityCurve=3, octaveSpread=0.6, octave=1),
        _preset("Pop Stab", "Pop", "Punchy 8ths with hot velocity.", arpMode=1, stepRate=3, gateLength=70.0, velocityCurve=2, velocity=130.0),
        _preset("Jazz Comp", "Jazz", "Ascending 8ths through a minor 7.", arpMode=1, stepRate=3, chord=6, velocityCurve=1, velocityDirection=0, octaveSpread=-0.4),
        _preset("Machine Gun", "Electronic", "Ultra-fast 32nds at a full gate.", arpMode=1, stepRate=9, gateLength=80.0, octaveSpread=0.5),
        _preset("Slow Bloom", "Ambient", "Wide, slow, accented from the bottom.", arpMode=1, gateLength=90.0, velocityCurve=2, velocityDirection=0, octaveSpread=0.5, velocity=80.0),
    ],
    # Plan 6.1: the character stereo processor. Every preset changes at least one
    # parameter from the default. Bank order is append-only.
    "StereoTool": [
        _preset("Tube Warmth", "Vintage", "Warm tube drive.", drive=6.0),
        _preset("Tape Glue", "Classic", "Smooth tape-style glue.", character=1, drive=9.0),
        _preset("Hard Clip Drive", "Rock", "Aggressive hard clipping.", character=4, drive=18.0),
        _preset("Console Clean", "Pop", "Gentle console colour.", character=3, drive=3.0),
        _preset("Diode Bright", "Electronic", "Bright asymmetric edge.", character=2, drive=12.0),
        _preset("Dynamic Width", "Groove", "Width blooms on transients.", drive=3.0, transientAmount=60.0, transientTarget=0, transientAttack=10.0, transientRelease=400.0),
        _preset("Mono Center", "Focus", "Mid-forward, tight centre.", drive=6.0, midFocus=50.0),
        _preset("Wide Highs", "Air", "Mono lows, wide highs.", character=1, drive=6.0, midFocus=-30.0, sideFilter=1, sideCrossover=4000.0),
        _preset("Vintage Punch", "Rock", "Drive pumps on the hits.", character=4, drive=15.0, transientAmount=40.0, transientTarget=1, transientAttack=5.0, transientRelease=80.0),
        _preset("Smooth Stereo", "Pop", "Soft, slow dynamic movement.", character=3, drive=4.0, transientAmount=20.0, transientAttack=80.0, transientRelease=300.0),
        _preset("Aggressive Width", "Electronic", "Hard width throw on transients.", drive=10.0, width=40.0, transientAmount=100.0, transientTarget=0, transientAttack=5.0, transientRelease=800.0),
        _preset("Balanced Vintage", "Classic", "Rounded vintage tone with a touch of centre.", character=1, drive=8.0, midFocus=10.0),
        _preset("Bright Air", "Electronic", "Airy top, mono bottom.", character=2, drive=10.0, sideFilter=1, sideCrossover=6000.0),
        _preset("Narrow Focus", "Vocals", "Tight vocal centre.", character=3, drive=5.0, midFocus=80.0),
        _preset("Full Spectrum", "All-genre", "Wide but controlled over tape drive.", character=1, drive=7.0, transientAmount=30.0, transientAttack=30.0, transientRelease=250.0, sideFilter=1, sideCrossover=200.0),
    ],
    # Plan 11: Multiband Saturation, the two-band split. Every preset changes at least one
    # parameter from the default. Bank order is append-only.
    "MultibandSaturation": [
        _preset("Band Glue", "Mix bus", "Gentle tube glue on both bands.", driveLow=3.0, driveHigh=3.0, sensitivity=0.4),
        _preset("Low Weight", "Bass", "Driven low band, clean highs.", driveLow=12.0, driveHigh=2.0, toneLow=-0.3),
        _preset("Air Bands", "Bright", "Tape texture with a lifted top band.", mode=1, driveHigh=8.0, toneHigh=0.7, crossover=1500.0),
        _preset("Split Crunch", "Drums", "Hard drive on both bands, fast envelope.", driveLow=18.0, driveHigh=16.0, attack=2.0, release=80.0),
        _preset("Vocal Leveller", "Vocal", "Envelope control per band, gentle drive.", driveLow=4.0, driveHigh=4.0, sensitivity=0.7, attack=8.0, release=120.0),
        _preset("Tape Split", "Character", "Warm tape character over both bands.", mode=1, driveLow=7.0, driveHigh=5.0, toneHigh=-0.2),
        _preset("Diode Bite", "Character", "Aggressive diode edge in each band.", mode=2, driveLow=14.0, driveHigh=10.0),
        _preset("Sub Keeper", "Subtle", "Only the low band works; the highs pass untouched.", driveLow=9.0, driveHigh=0.0, crossover=200.0),
        _preset("High Sparkle", "Bright", "A bright, driven high band over a calm low.", driveHigh=10.0, toneHigh=0.9, crossover=2500.0),
        _preset("Dark Console", "Lo-Fi", "Dark tilt on both bands.", driveLow=8.0, toneLow=-0.5, toneHigh=-0.7),
        _preset("Parallel Bands", "Subtle", "Heavy drive mixed back, both bands.", mix=40.0, driveLow=12.0, driveHigh=12.0),
        _preset("Bass Punch", "Bass", "A fast envelope tightens the low band.", driveLow=15.0, sensitivity=0.8, attack=1.0, release=60.0, crossover=300.0),
        _preset("Gentle Two", "Mix bus", "Barely there, both bands.", driveLow=2.0, driveHigh=2.0, sensitivity=0.2),
        _preset("Extreme Split", "Extreme", "Maximum drive on both bands, diode mode.", mode=2, driveLow=24.0, driveHigh=24.0, sensitivity=1.0),
        _preset("Null Split", "Utility", "Fully dry: a transparent reference.", mix=0.0),
    ],
    # Plan 6.1: the four-slot loop instrument. Every preset carries four embedded: loop
    # paths (the slot loading rides as state properties) and changes at least one
    # parameter. Bank order is append-only.
    "Cyclical": [
        _preset("Four on the Floor", "Groove", "Kick, hats, bass and chord locked to 16ths.",
                loop_paths=["embedded:kick_4k.wav", "embedded:hats_4k.wav", "embedded:bass_4k.wav", "embedded:chord_4k.wav"],
                quantize=1, slot3Speed=2.0),
        _preset("Backbeat Stack", "Groove", "Snare, hats and bass interlock with swing.",
                loop_paths=["embedded:snare_4k.wav", "embedded:hats_4k.wav", "embedded:bass_4k.wav", "embedded:perc_4k.wav"],
                quantize=1, swing=12.0),
        _preset("Half-Time Drift", "Groove", "Halved speeds and a reversed chord swell.",
                loop_paths=["embedded:kick_4k.wav", "embedded:snare_4k.wav", "embedded:chord_4k.wav", "embedded:texture_4k.wav"],
                slot3Direction=1, slot3Speed=0.5, slot4Speed=0.5),
        _preset("Sub Anchor", "Bass", "A forward bass riff over a reversed kick.",
                loop_paths=["embedded:bass_4k.wav", "embedded:kick_4k.wav", "embedded:perc_4k.wav", "embedded:noise_4k.wav"],
                slot2Direction=1, slot2Speed=0.5, pitchShift=-12),
        _preset("Rubber Bassline", "Bass", "Double-speed bass with granular sparkle.",
                loop_paths=["embedded:bass_4k.wav", "embedded:hats_4k.wav", "embedded:arp_4k.wav", "embedded:noise_4k.wav"],
                slot1Speed=2.0, slot3Speed=0.5, slot4Granular=1),
        _preset("Glass Loop", "Texture", "Bells and pads drifting at uneven speeds.",
                loop_paths=["embedded:bells_4k.wav", "embedded:texture_4k.wav", "embedded:drone_4k.wav", "embedded:noise_4k.wav"],
                slot2Speed=0.75, slot3Speed=0.5, crossfadeLength=50.0),
        _preset("Tape Halo", "Texture", "A reversed pad over a slow forward texture.",
                loop_paths=["embedded:texture_4k.wav", "embedded:drone_4k.wav", "embedded:bells_4k.wav", "embedded:noise_4k.wav"],
                slot2Direction=1, slot1Speed=0.5, crossfadeLength=200.0),
        _preset("Grain Field", "Experimental", "All four slots granular, two random ping-pong.",
                loop_paths=["embedded:chord_4k.wav", "embedded:vox_4k.wav", "embedded:perc_4k.wav", "embedded:noise_4k.wav"],
                slot1Granular=1, slot2Granular=1, slot3Granular=1, slot4Granular=1,
                slot2Direction=2, slot2PingRandom=1, slot4Direction=2, slot4PingRandom=1),
        _preset("Broken Clock", "Experimental", "Ping-pong slots with randomised turn points.",
                loop_paths=["embedded:kick_4k.wav", "embedded:snare_4k.wav", "embedded:arp_4k.wav", "embedded:vox_4k.wav"],
                slot1Direction=2, slot2Direction=2, slot3Direction=2, slot4Direction=2,
                slot1PingRandom=1, slot3PingRandom=1, quantize=2, swing=-10.0),
        _preset("Reverse Universe", "Experimental", "Three reversed loops behind a forward kick.",
                loop_paths=["embedded:kick_4k.wav", "embedded:chord_4k.wav", "embedded:vox_4k.wav", "embedded:texture_4k.wav"],
                slot2Direction=1, slot3Direction=1, slot4Direction=1, crossfadeLength=120.0, slot4Speed=0.5),
        _preset("Stutter Grid", "Electronic", "Sprinting hats over a 16th grid.",
                loop_paths=["embedded:kick_4k.wav", "embedded:hats_4k.wav", "embedded:snare_4k.wav", "embedded:perc_4k.wav"],
                quantize=1, slot1Speed=2.0, slot2Speed=4.0),
        _preset("Warehouse", "Electronic", "Wide pan, long crossfade, driving kick.",
                loop_paths=["embedded:kick_4k.wav", "embedded:bass_4k.wav", "embedded:chord_4k.wav", "embedded:drone_4k.wav"],
                pan=30.0, slot1Speed=2.0, slot3Speed=0.5, crossfadeLength=30.0),
        _preset("Slow Motion", "Ambient", "Quarter-speed pads and bells.",
                loop_paths=["embedded:texture_4k.wav", "embedded:bells_4k.wav", "embedded:drone_4k.wav", "embedded:noise_4k.wav"],
                slot1Speed=0.5, slot2Speed=0.5, slot3Speed=0.25, crossfadeLength=300.0, masterVolume=-6.0),
        _preset("Night Drive", "Ambient", "A granular texture under a straight pulse.",
                loop_paths=["embedded:kick_4k.wav", "embedded:texture_4k.wav", "embedded:drone_4k.wav", "embedded:vox_4k.wav"],
                slot2Granular=1, slot4Direction=1, slot4Speed=0.5, crossfadeLength=200.0),
        _preset("Gate Cutter", "Groove", "Gate mode chops the held notes.",
                loop_paths=["embedded:kick_4k.wav", "embedded:hats_4k.wav", "embedded:chord_4k.wav", "embedded:vox_4k.wav"],
                triggerMode=1, quantize=2, swing=18.0, slot4Granular=1),
    ],
}
