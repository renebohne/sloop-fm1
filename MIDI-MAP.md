# MIDI program change and control change on the FM-1

SLOOP takes MIDI from two places at once: the **MIDI IN jack** (3.5 mm TRS, on the FM-1) and **USB**
from a computer, a phone or a USB MIDI host box. Either can carry program change and control change.
A USB keyboard plugged straight into the FM-1 cannot work: both are USB devices and a USB link needs a
host.

This file is the reference for the two messages that reach the instrument's sound. Notes, MIDI clock
and everything else are in [SLOOP.md](SLOOP.md#midi-keyboards).

| | Program change (PC) | Control change (CC) |
| --- | --- | --- |
| What it does | loads a preset or a drum kit | moves a parameter |
| Number is | a position in a list | a fixed parameter, see the table below |
| Value is | the list position, 0–127 | the parameter's value, 0–127 |
| Received | yes | yes |
| Sent back | no | no |

Both are **receive only**: the FM-1 acts on what arrives and never sends either message itself. Your
controller keeps the faders and displays it shows; the FM-1 does not have to be the master of the
mapping.

## Which track a message reaches

The **MIDI channel picks the track**, the same way notes do, so one map covers all four parts:

| MIDI channel | Plays |
| --- | --- |
| 1, 2, 3 | synth tracks 1, 2, 3 |
| 10 | the drum track (GLO → DRUMS → **CH** changes this channel) |
| 4–16 | the selected track: set the controller to channel 4 and it follows whatever page the FM-1 is on |

So a CC on channel 2 moves a knob of track 2 and nothing else. A message on a channel that owns no
part (4–16 with no part selected to follow) moves the selected track, as a note there would.

Note-off tracking does not apply to either message: they carry no note, so they always act on the
track the channel plays at that moment.

## Program change

A PC is what the **PRESETS** knob does, from the controller.

On a **synth track**:

| Program number | Loads |
| --- | --- |
| 0 … NBANK−1 | the factory presets, in the order of the PRESETS page. The entry may belong to another engine; the track switches engine to match. |
| NBANK … NBANK+31 | the 32 user preset slots, by slot number (not by rank). An empty slot is left alone, as the USER page does. |
| NBANK+32 … 127 | nothing, ignored |

On the **drum track**:

| Program number | Loads |
| --- | --- |
| 0 … kits−1 | the drum kits |
| kits … 127 | nothing, ignored |

A PC only changes the sound. It leaves the pattern, the track's level, pan and mute and the mix alone,
exactly as turning the knob does. Out of range is ignored rather than clamped, so a controller sweeping
0–127 does what the knob would and no more.

On this branch NBANK is 76 and there are 37 drum kits, so a synth track takes 0–75 for the factory
presets, 76–107 for the user slots, and the drum channel 0–36 for the kits. Those counts follow the
firmware; the USER page on the device shows the real ones.

## Control change

A CC reaches a **fixed parameter**: the same number always means the same thing, whatever page the
FM-1 is showing. It cannot follow the page, because the page is the device's own state and is never
sent out — a map keyed on "whatever KNOB 1 shows" would point somewhere else as soon as you tapped a
button. The pages below are only how the families are grouped.

| CC | Moves | Page it is on | CC | Moves | Page it is on |
| --- | --- | --- | --- | --- | --- |
| 7 | LVL | TRACKS (KNOB 2) | 10 | PAN | TRACKS (KNOB 4) / VOICE 2 |
| 16 | HOME knob 1 | HOME | 17 | HOME knob 2 | HOME |
| 18 | HOME knob 3 | HOME | 19 | HOME knob 4 | HOME |
| 20 | ATK | ENV | 21 | DEC | ENV |
| 22 | SUS | ENV | 23 | REL | ENV |
| 24 | FLT | ENV DEST | 25 | PIT | ENV DEST |
| 26 | SHP | ENV DEST | | | |
| 27 | RATE | LFO | 28 | WAVE | LFO |
| 29 | PHS | LFO | 30 | FADE | LFO |
| 31 | PIT | LFO DEST | 32 | FLT | LFO DEST |
| 33 | SHP | LFO DEST | 34 | AMP | LFO DEST |
| 35 | DST | FX | 36 | CHO | FX |
| 37 | DLY | FX | 38 | REV | FX |
| 39 | SLCR | SLICER | 40 | PAT | SLICER |
| 41 | RATE | SLICER | 42 | DEPTH | SLICER |
| 43 … 50 | the engine's EDIT 1 and EDIT 2 knobs 1–8 | EDIT 1, EDIT 2 | | | |
| 51 | VCE | VOICE | 52 | GLD | VOICE |
| 53 | GLMOD | VOICE | 54 | PRIO | VOICE |
| 55 | ALLOC | VOICE 2 | 56 | DTUNE | VOICE 2 |
| 57 | PAN | VOICE 2 | 58 | MUTE | VOICE 2 |

45 CCs in all. The labels are the three-character ones the device prints on its screen, so the table
matches what is in front of you while playing. Where a knob is on more than one page — PAN, for
instance — either CC reaches it.

CC 0–6, 8, 9, 11–15 and 59–127 do nothing. They are the standard numbers for things this instrument
has no parameter for (breath, foot, expression, modulation, pitch bend) or are simply unassigned.

### The four HOME knobs follow the engine

CC 16–19 are not four fixed parameters: each engine decides which four its HOME page shows, so they
always move the knobs you can see on HOME for the engine in use.

| Engine | CC 16 | CC 17 | CC 18 | CC 19 |
| --- | --- | --- | --- | --- |
| ANALOG | CUT | RES | ATK | REL |
| DIGITAL | IDX | MDEC | FB | REL |
| DRAWBAR | REG | PERC | DRV | ROTR |
| FLOYD | LVL2 | DEC2 | LVL3 | REL |
| FORMANT | VOWL | BUZZ | BRTH | TALK |
| GRAIN | POS | SIZE | DENS | SPRD |
| LOFI | WAVE | DUTY | CRSH | REL |
| PHASE | DCW | ENV | DTN | REL |
| SAMPLE | SET | CUT | ATK | REL |
| SLICE | SRC | DIV | PTCH | DCAY |
| TRIO | CUT | RES | PW | INT2 |

The four EDIT knobs (CC 43–50) follow the engine the same way: they are its own EDIT 1 and EDIT 2
knobs, in order.

### The drum track takes fewer

On the drum channel only these do anything:

| CC | Moves |
| --- | --- |
| 7 | the drum track's LEVEL (GLO → DRUMS → LVL, the same knob as TRACKS → KNOB 2) |
| 10 | PAN |
| 16–19 | LEVEL, REVERB, PAN, SLICE LEN — the drum track's own HOME page |
| 39–42 | the SLICER |

Everything else is ignored there, because a synth parameter has no drum track meaning. That includes
CC 43, which on a synth track is the engine's EDIT 1 knob 1: on the drum track that same slot is the
kit, and a PC already changes the kit.

### Values

A CC value is **absolute**, scaled over the parameter's own range:

| Value | Result |
| --- | --- |
| 0 | the parameter's minimum |
| 127 | its maximum |
| 64 | about the middle of its range |

So a fader's travel matches the knob's, and a parameter with a fixed list of positions (WAVE, a knob
that only has six settings) lands on one of them — no rounding rules to remember, and the ends are
exact. A bipolar knob such as PAN goes from its −64 end to its +63 end.

A controller that sends its values as 14-bit pairs (pitch bend style) moves nothing: the FM-1 takes
plain 7-bit values, one per message.

### What a CC does not do

- **It does not touch the pattern.** Notes and steps are steps and song state, not a continuous sound,
  and a fader sweep over LEN, DIV, SWING or GATE is not a performance gesture.
- **It does not change the engine, the preset or the kit.** Use a program change for that.
- **It does not move the GLO parameters.** Those are settings of the whole machine, not of one track:
  master level, tempo, sync, the drum channel, the audio settings. They belong on the device.
- **It does not move the scale or the arpeggiator.** Same reasoning as the pattern: steps, not a
  sound.

## After a message arrives

The screen redraws, so the knob you moved shows its new position on whichever page is up. The web
editor is told too and re-reads the track, so its faders follow the controller instead of drifting
away from it.

If the controller and the FM-1 disagree about a value, the last message wins — the FM-1 has no separate
knowledge of where a fader is.

## Practical notes

- Mapping on the controller is usually quicker than on the FM-1: put the four fader rows of a standard
  8-knob controller on channels 1, 2, 3 and 10 and the whole instrument is under the hands.
- On the drum channel, CC 64 and CC 66 — what a sustain pedal usually sends — are unassigned. They are
  safe to use as a latch, a hold or a mute from a controller's footswitches without touching the sound.
- Bank and program-change buttons on a controller usually send PC 0–15 with a bank select (CC 0 and
  CC 32). On a synth track those are the first sixteen factory presets, which is rarely what the
  button means — check that your controller is sending bare program numbers before blaming the FM-1.

## Reference

| | |
| --- | --- |
| Source | `firmware/src/ccmap.c` (the CC map), `firmware/src/ui.c` (`midi_pc`), `firmware/src/seq.c` (both handlers, and the channel routing) |
| Tests | `tests/ui_pages_test.c`, `midi_pc_tests()` and `cc_tests()` |
| User manual | [SLOOP.md](SLOOP.md#midi-keyboards) |
| Editor protocol | [web/EDITOR_PROTOCOL.md](web/EDITOR_PROTOCOL.md) — the SysEx side, a different path from these two messages |