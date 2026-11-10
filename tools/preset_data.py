"""Authoring data for the AudioSuite factory bank.

`SCHEMA` lists every parameter the three plugins expose, with its range and default, so
`build_bank.py` and `audit_bank.py` can validate a preset without loading the plugin.
`PRESETS` holds the deltas from the default state. Both are the source of truth; the JSON files
under `bank/<plugin>/` and the combined `bank/AudioSuitePresets.xml` are generated from them.

Keep the schema in step with `createParameterLayout()` in each processor and with AGENTS.md
section 4. The C++ test `bankRanges` checks the same rules against the live APVTS, so a drift here
is caught at build time in a Pro build.
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
        "driftRate": {"min": 0.1, "max": 10.0, "default": 1.0, "type": "float"},
        "depth": {"min": 0.0, "max": 100.0, "default": 19.1, "type": "float"},
        "spread": {"min": 0.0, "max": 1.0, "default": 0.05, "type": "float"},
        "stereoPhase": {"min": 0.0, "max": 180.0, "default": 90.0, "type": "float"},
        "lfoShape": {"min": 0, "max": 3, "default": 0, "type": "choice"},
        "shift": {"min": -32.0, "max": 32.0, "default": 0.0, "type": "float"},
        "tilt": {"min": -2.0, "max": 2.0, "default": 0.0, "type": "float"},
        "gate": {"min": -90.0, "max": 0.0, "default": -90.0, "type": "float"},
        "feedback": {"min": 0.0, "max": 12.0, "default": 0.0, "type": "float"},
        "mix": {"min": 0.0, "max": 100.0, "default": 100.0, "type": "float"},
        "freeze": {"min": 0, "max": 1, "default": 0, "type": "bool"},
        "feedbackDamping": {"min": 0.0, "max": 100.0, "default": 30.0, "type": "float"},
        "driftStart": {"min": 0.0, "max": 100.0, "default": 0.0, "type": "float"},
        "outputGain": {"min": -12.0, "max": 12.0, "default": 0.0, "type": "float"},
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
}


def _preset(name, category, description, lerp_curve=None, **params):
    preset = {"name": name, "category": category, "description": description, "params": params}

    # Micro-Timing Chaos only: the Lerp curve is a state property, not a parameter.
    if lerp_curve is not None:
        preset["lerpCurve"] = lerp_curve

    return preset


PRESETS = {
    "SpectralDrift": [
        _preset("INIT", "Utility", "The default state."),
    ],
    "DynamicSaturation": [
        _preset("INIT", "Utility", "The default state."),
    ],
    "MicroTimingChaos": [
        _preset("INIT", "Utility", "The default state."),
    ],
}
