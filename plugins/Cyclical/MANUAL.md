# Cyclical — User Manual

The plugin code is `Cycl`. The bundle id is `com.audiosuite.cyclical`.

## The interface at a glance

<img src="../../docs/manuals/cyclical-slots.png" alt="Cyclical SLOTS page" width="653">

Cyclical is a four-slot loop instrument. Four loaded audio loops play with a region, a speed,
a direction (forward, reverse or ping-pong), an optional granular mode and velocity scaling —
locked to a musical grid with a quantised start and swing. There is no recording and no audio
input: the plugin is output-only. Each slot is triggered by its own **Note** key (by default
C4, C#4, D4, D#4 = MIDI 60–63); a chopped slot adds one key per slice above its base note.
The preset bar (R12) and the hamburger menu (R13) sit in the header; the page switch (R8,
top right) moves between the SLOTS and GLOBAL pages. The accent is tangerine.

## Page: SLOTS

<img src="../../docs/manuals/cyclical-slots.png" alt="SLOTS page" width="653">

Four cards, one per slot. Each card shows the slot's base key in its title (it follows the
Note knob) and holds two knob rows and a pill row:

| Control | Range | What it does |
|---|---|---|
| **Waveform** | — | The loaded loop with its region band, the chop lines (Chop mode) and the playhead. **LOAD** (top-right) opens a WAV/AIFF browser; a missing loop draws "Loop missing - click to locate" over the still-playing audio, and clicking it opens the same browser. |
| **Start** | 0 – 100 % | Where the loop starts, as a share of its length (capped at 95 %). |
| **Length** | 5 – 100 % | How much of the loop plays. Effective length = min(Length, 100 − Start). |
| **Speed** | 0.25 – 4.0 × | Playback speed (a tape varispeed: pitch moves with it). Dimmed while Grain is on, and while Host Sync is on — unless the slot is chopped (a chopped slot ignores sync). |
| **Velocity** | 30 – 150 % | Scales the note velocity of the slot's keys. |
| **Note** | 24 – 96 | The MIDI key that triggers the slot. Defaults 60–63 (the old fixed map). Two slots sharing a note: the lowest slot wins. |
| **Beats** | 1 – 16 | The musical length of the whole loaded loop, in beats. Used while Host Sync is on; **dimmed in Chop mode** (sync is disabled there). |
| **Slices** | 2 – 16 | How many slices Chop divides the region into. Only live in Chop mode. |
| **Slice Length** | −100 – +100 % | Skews the slice boundaries: positive lengthens the late slices, negative the early ones. 0 % is the equal split. **This is not Length**: Length trims the loop region that all slices live in; Slice Length only redistributes the boundaries inside it. |
| **FWD / REV / PP** | — | Direction. Ping-pong turns at both bounds; **Ping Rand** (below) randomises the turn points per trigger. |
| **PING RAND** | on / off | Random turn points on each trigger. The rate is untouched, so the pitch never drifts. Only live in Ping-Pong — dimmed otherwise, never hidden. |
| **GRAIN** | on / off | Granular playback: one 80 ms grain at a time (a stutter, not a cloud). Speed is dimmed while it is on. Allowed inside a chopped slice — very short slices sound grainy. |
| **CHOP** | on / off | Chop mode: the slot's region splits into **Slices** parts, and key Note + k plays slice k (Dr. Rex style). **Host Sync is disabled for a chopped slot** — its Speed knob stays live. |

The seam of the region crossfades so loops don't click at the wrap; **Crossfade** (GLOBAL)
sets the blend length for all four slots. A retrigger cuts the sounding loop into a 2 ms tail
and starts fresh, so a slot never doubles up.

The footer line shows the help text of the control under the pointer.

## The key map

| Key | What it plays |
|---|---|
| slot's **Note** | The slot's region (or slice 0 when chopped). |
| Note + 1 … Note + slices − 1 | Slices 1 … of a chopped slot. Out-of-range keys do nothing. |

Overlaps between slots are accepted (a lower slot wins), so the manual's default layout —
60, 61, 62, 63 — is overlap-free, but nothing stops you mapping a 16-slice chop over another
slot's key.

## Page: GLOBAL

<img src="../../docs/manuals/cyclical-global.png" alt="GLOBAL page" width="653">

| Control | Range | What it does |
|---|---|---|
| **Output** | −24 – +12 dB | Output level of the whole instrument. |
| **Pan** | −100 – +100 % | Constant-power pan: +100 % pans right, −100 % pans left; the sum of the two channels' power stays put. |
| **Pitch** | −12 – +12 | Detunes all four slots together (speed and pitch move, like tape). |
| **Crossfade** | 0 – 500 ms | The loop-seam blend, shared by all slots. |
| **Swing** | −20 – +20 % | Shifts odd grid points. Positive is late. |
| **Quantize** | Off / 1/16 / 1/8 / 1/4 / 1/2 / Bar | A trigger starts on the next grid point. Off starts immediately. |
| **Trigger** | Latch / Gate / Toggle | Latch: a note runs until retriggered. Gate: the loop plays only while the key is held. **Toggle**: the first press starts the slot looping (scheduled on the grid when Quantize is on), the second press stops it immediately — a note-off never stops it, and a stopped transport stops the toggled slots. |
| **Tempo** | 40 – 240 BPM | Runs the beat clock while Host Sync is off. |
| **Host Sync** | on / off | On: the clock and the synced loops follow the host's position and tempo. Chopped slots ignore it. |
| **Beat readout** | — | The beat clock's position, for reference. |

## BPM sync and Beats

With **Host Sync on**, every unchopped slot plays its whole loop in exactly **Beats** beats:
the rate is derived from the loop's own duration, so the Speed knob is inactive (dimmed) and
the loop is in musical time. **Start and Length choose which part plays; they do not change
the rate.** With sync off, Speed works exactly as before and the **Tempo** parameter paces
the clock (the host's tempo is authoritative while synced). A change to Tempo or Beats takes
effect from the next trigger.

## Chop mode

**Chop** splits the slot's region into **Slices** equal parts (or skewed by **Slice
Length**), and key Note + k plays slice k — a Dr. Rex-style break. The slice boundaries are
pinned to the region, so the slices always cover it exactly. Sync is disabled for a chopped
slot (its Speed knob stays live and its Beats value is unused); granular is allowed inside a
slice, and very short slices sound grainy. The waveform draws the slice boundaries while
chop is on.

## Missing loops

Each slot's loop reference (a file path or an `embedded:` library URI) travels with the
session and with presets. When a carried path cannot be found, the slot keeps playing its
current loop and shows the "Loop missing - click to locate" overlay; clicking it opens a
browser pointed at the user loop folder. The same file name is also searched in
`Documents/AudioSuite/Cyclical/Samples` before the missing flag goes up.

## Settings and About

The hamburger button at the far right of the header opens one menu (plan 08):

- **Settings…** — tooltips on/off, the no-playhead fallback tempo, and the user preset folder.
- **About…** — brand lockup, product name and version, FROTH Audio, the plugin code, the JUCE
  version and the GPL v3 note.

## Factory presets

Twenty-five factory presets ship with the plugin in bank builds; each carries the four
library loops it was made with, so selecting a preset loads the sounds too. Use the preset
bar in the header to browse, save and load. The fifteen plan-6.1 presets come first
(Four on the Floor … Gate Cutter); the ten plan-14 presets use the new concept:

| Preset | Category | What it shows |
|---|---|---|
| Dr Rex Grid | Electronic | Two chopped slots play slice-per-key on a 16th grid. |
| Late Slices | Experimental | A chopped pad whose late slices stretch out (+60 % skew). |
| Early Slices | Experimental | The opposite skew: the early slices take the space. |
| Slice Skew Pair | Texture | Two chopped pads skewed in opposite directions. |
| Chopped Break | Bass | A double-speed chopped bass riff, key-per-slice. |
| Synced Bars | Groove | Every loop locks to the host: whole loop in Beats beats. |
| Half-Time Sync | Groove | Synced loops stretched over eight beats each. |
| Toggle Looper | Ambient | Toggle mode: one press starts the pad, the next stops it. |
| Spread Keys | Electronic | The four slots sit across the keyboard, one chop included. |
| Tempo Runner | Electronic | Free-running 150 BPM drives the quantised chop. |

## MIDI / host notes

- **MIDI map:** each slot claims its **Note** key (defaults: C4 (60) → slot 1 … D#4 (63) →
  slot 4). A chopped slot claims the range above its base note. Other notes are ignored.
  A note-on with velocity 0 is treated as a note-off.
- **Latency:** 0 samples.
- **Tail length:** 2 s, so a released or ping-pong tail is not cut by the host.
- The host does not need to arm anything: the plugin is output-only. Quantised starts need
  the transport playing (or they fire on the internal clock when Quantize is Off).
