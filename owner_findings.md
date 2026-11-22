# Found during DAW-Test

## Issues

### Plugin MultiSat

- The 2 meters for Low and High and the "crossover preview (Hz)" should use the whole height of their container. Add some kind of Scale to the previews.
- Add Modulation/LFO and create at least 10 more presets that use modulation. Use the existing preset categories.
- Input meter should be on the left side, output on the right side. Make both stereo.

### Plugin SD

- Add at least 10 more presets that use modulation. Use the existing preset categories.

### Plugin Stereo Tool

- Set group "IMAGE" and group "LEVEL" in one row and make plugin window width smaller. Group "CHARACTER" and group "Mid/Side, FOLLOWER Preview" should fill its width to the whole window width.
- Add Modulation/LFO and create at least 10 more presets that use modulation. Use the existing preset categories.
- CHARACTER -> Character Chioces should use Conbobox since text is too long. same with IMAGE -> Filters.
- Mid/Side Meter: make width 200% each
- fit height of Preview row and CHARACTER row (no extra space for Character but for preview/meter)

### Plugin DS

- Add at least 10 more presets that use modulation. Use the existing preset categories.

### Plugin MidiTool

- Keyboard/Chords should be playable/clickable in any way.

### Plugin Cyclical

- Concept needs an complete overhaul.
- there should be a mode to set if either the sample is played during key-press trigger or start looped playing and stop an the next trigger.
- Assigned notes should be user selectable.
- Loops should be syncable (BPM Sync)
- i want to have a mode where i can chop a loop and each slice could be assigned to a note/key (pretty much like Propellerheads "Dr. Rex" in Reason)
- Plugin Size and and view of the "SLOTS" Tab is pretty good. please try to keep that.
- Select a new Accent Color

### Plugin MTC

- Group "TRANSPORT": Knobs "Tempo" and "Speed" distance too far from Label.

## LFO

- New waveform "Pendulum" does not work. visualization does not change; no Modulation at all with this waveform.

## Round 3 (dictated 2026-11-21)

- StereoTool: Visualizer: move to window top. Set default height to minimum height. Remove the input and output meters for this plugin.
- MultiSat: Remove the input and output meters for this plugin. Draw a frequency grid in the visualizer/preview. Also move the "SOUND|MOD"-Switcher left of preset-bar (same row as preset bar).
- MidiTool: Step Rate entries from 1/32 to 4 Bars.
- Every plugin: properly enable/disable the per-mode knob/button states (controls that only apply in one mode are dimmed, never hidden, as Sgraffito already does).
