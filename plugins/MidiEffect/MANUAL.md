# MIDI Chord Arp Scale Tool — User Manual

<img src="../../docs/manuals/midieffect-default.png" alt="MIDI Chord Arp Scale Tool, Chord mode" width="587">

MIDI Chord Arp Scale Tool takes incoming notes and re-harmonises them: each note is
quantised to a scale, chorded, and sent on — either all at once (Chord mode) or spread
over an 8-step clock (Arp mode). It accepts notes from the host *and from its own playable
keyboard*. The plugin is a VST Instrument (the owner switched it back from the MIDI-effect
build): it produces notes, not audio, so nothing passes through its output — put an
instrument on the track that receives its MIDI, or route the MIDI onward to another track.

## The two modes

**Chord** (the default) is the instant chord: the whole ordered chord fires at the
note-on's position, and the note-off releases it. This is the behaviour the plugin has
always had.

**Arp** runs a step clock: the ordered chord notes cycle over a fixed 8-step pattern, one
note per step, at the Step Rate. The clock follows the host position (Host Sync on) or the
Tempo parameter (Host Sync off). Steps are always derived from the beat grid, so a tempo
change or a relocation never makes the pattern drift.

## The controls

The editor has two groups. **SHAPE** is the key and the chord; **ARP** is the clock.
The panel between the header and the groups is the chord/arp visual: an 8-step strip (the
clock's current step outlined, sounding steps filled, note names inside) above a playable
keyboard — click a key and the plugin plays the note through its own chord and arp logic,
so you can try the plugin without a keyboard routed in. Notes held from other keys appear
as a small "+n more" marker. It updates while you play.

### Playing the plugin from its own window

- **Click a key** on the keyboard strip: the note runs the same scale, chord and arp path
  as a host note.
- **Click the 8-step strip**: it replays the current chord (a second click, or the pointer
  leaving the strip, ends it).
- Every note the editor starts also *ends*, on every path: mouse-up, a drag off the key,
  the pointer leaving, losing keyboard focus, a stopped transport, and the editor being
  closed. There is no stuck note.

<img src="../../docs/manuals/midieffect-arp.png" alt="MIDI Chord Arp Scale Tool, Arp mode" width="587">

### SHAPE

| Control | What it does |
|---|---|
| **Root Note** | The key centre the scale quantisation works from (shown as `C4 (60)`). |
| **Scale** | Notes are quantised to the nearest scale tone: Chromatic (no change), Major, Minor, Minor Pentatonic, Dorian or Whole Tone. |
| **Chord Quality** | The chord built on the quantised note: Major, Minor, Dim, Aug, Sus2, Sus4, Minor 7 or Major 7. |
| **Pattern** | The order the chord's notes are played in: Up, Down, Alternate (outer-in), Random (deterministic per note) or Scattered (a second deterministic shuffle). |
| **Octave** | Transposes the chord by up to two octaves. |
| **Spread** | Octave Spread: each pass through the chord tones of the 8-step pattern lifts by up to an octave — +1 crawls up, −1 crawls down, 0 stays put. |
| **Velocity** | Scales the note velocity, 0–150 %. |

### ARP

Controls that only apply in Arp mode are **dimmed while Mode is Chord** (Chord fires the
whole chord event for event: no clock, no gate, no per-step shape). The SHAPE controls and
the Pattern ordering stay live in both modes.

| Control | What it does |
|---|---|
| **Mode** | Chord or Arp, as above. |
| **Step Rate** | The step length in Arp mode, plain, triplet (T) or dotted (D): 1/4, 1/4T, 1/4D, 1/8, 1/8T, 1/8D, 1/16, 1/16T, 1/16D, 1/32, 1/32T, 1/32D, plus the slower plain rates 1/2, 1 Bar, 2 Bars and 4 Bars. |
| **Sync** | Host Sync. On: the clock follows the host's position and tempo. Off: it runs on Tempo. |
| **Tempo** | The clock's tempo while Host Sync is off, 40–240 BPM. |
| **Gate** | Note length as a share of one step, 10–100 %. |
| **Vel Curve** | The per-step velocity shape in Arp mode: Flat (uniform), Linear, Exponential or Random (a deterministic hash of note, step and cycle). |
| **Vel Dir** | Which end the Linear and Exponential curves start at: Ascending rises 0.2 → 1.0, Descending falls 1.0 → 0.2. |

## How a note flows through

1. The incoming note is quantised to the scale (Root Note is the key centre).
2. A chord is built on the quantised note, in the selected quality, shifted by Octave.
3. In Chord mode the whole ordered chord is sent at the note-on's position. In Arp mode the
   ordered notes cycle over 8 steps, each with a gate and a shaped velocity.
4. A note-off truncates: in Chord mode the whole chord releases; in Arp mode everything the
   key is holding stops and the pattern ends.

A retrigger before the note-off cuts the previous chord first, so nothing sticks — you can
hold a key and re-strike it freely.

## Practical notes

- The plugin produces no audio: its output stays silent and it exists for its MIDI output.
- The reported latency is a fixed 50 ms, whatever the rate or gate.
- 15 factory presets ship with the plugin in bank builds; use the preset bar in the header
  to browse, save and load.
- The hamburger menu in the header (right of the preset bar) opens Settings… (tooltips toggle,
  fallback tempo, preset folder) and About….
- Hovering any control shows what it does in the help line at the bottom of the window; the
  Settings dialog's tooltips toggle turns the line off.
- Scale, Pattern, Vel Curve, Step Rate and Chord Quality are dropdowns (plan 12: the owner
  named them); Mode, Sync and Vel Dir are pill switches.

## Using it in a host

The plugin is a VST Instrument (its VST3 category is Instrument | Synth). Hosts list it
with the instruments, not with MIDI effects.

1. Put MIDI Chord Arp Scale Tool on an instrument track, or on any track that can host an
   instrument.
2. Feed it MIDI: a clip on that track, a keyboard, or another track routed in.
3. Send its MIDI output where it is needed. Some hosts process the MIDI output inline on
   the same track (another instrument after it); others route it to a second track with
   **MIDI From** set to the track holding the plugin.

In Ableton Live the plugin appears under **Plug-Ins** (Live's MIDI Effects browser holds
native devices and Max for Live devices only). The standard two-track workflow applies:

1. Put the plugin on a MIDI track, from the Plug-Ins browser.
2. Feed it MIDI from a clip, a keyboard, or another track routed in.
3. Create a second MIDI track, set its **MIDI From** to the track holding the plugin, arm
   it, and put an instrument on it. The instrument plays the chorded or arp'd notes.

The notes come out of the plugin's MIDI output, not its (silent) audio output.
