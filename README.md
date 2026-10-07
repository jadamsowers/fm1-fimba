# FiMba-1 for the M-VAVE FM-1

FiMba-1 turns the M-VAVE FM-1 into a thumb piano. (The name is kalimba with the FM-1 hidden in it,
the way FoMni hides FM in Omnichord.) The 16 white keys are the tines, laid out the way a
real kalimba's are: the longest (lowest) tine in the middle, with the scale alternating outward left,
right, left, right. The 11 black keys do something else: they play thumb-roll chords, a chromatic
second row, or performance moves (palm mute, covering the sound hole, freeze, octave shift,
materials).

Each tine is physically modelled. It is a clamped-free metal bar with inharmonic bending modes,
a twin of the first mode that beats slowly, and a pluck that a soft thumb or a hard nail excites
differently. Six materials change the modes, how they decay and how bright they are. The tines sit
on a body (board, box or gourd) whose sound hole you can cover for the kalimba "wah", with mbira
buzzers if you want them. After that comes a granular cloud you can freeze, a ping-pong delay and
a plate reverb.

It is built on FoMni's platform layer (and through it X0X's and Felucca's): the same boot, safe mode,
USB, storage and installer.

## Playing

| | |
|---|---|
| White keys | The tines. **Layout** (Keys page) sets their order: **Tine** is the kalimba's V (C4 in the middle, D4 right of it, E4 left, up to D6 on the right edge), **Linear** goes up the scale from left to right, and **Split** puts the same eight degrees in each hand with the left an octave down. |
| Black keys | Set by **Black** (Keys page). See below. |
| PLAY | Freezes the grains, so the cloud keeps playing what's in its buffer. Press again for live. |
| REC | Palm mute: damps every tine. |
| ARP | **Release**: *Ring* (a tine rings on after you let go, as on a real kalimba) or *Damp* (letting go puts a finger on it). |
| OCT− / OCT+ | Octave. |
| SELECT | Material. |
| ALGORITHM | Scale. |
| PRESETS | Key. |
| KNOB 1–4 | The four values on the screen. |
| HOME ENV FX LFO SEL SEQ GLO | The pages (EDIT steps through them). |
| SAVE | Saves. It also saves by itself a few seconds after a change, once everything is quiet. |
| OCT− + OCT+ held 5 s | Update mode (Felucca's). Held at power-on: hardware calibration. |

The keys aren't velocity-sensitive (the FM-1 reports on and off only), so keys pluck at a fixed
strength. **Hardness** sets how hard that is, and over MIDI velocity does.

### The black keys

- **Chords**: each key rolls a chord across the tines the way a thumb slides over them: root,
  third, fifth and the octave, **Strum ms** apart. The first seven keys are the triads on the
  scale's seven degrees (C Dm Em F G Am Bdim in C major). The last four are sevenths on I, IV, V
  and vi (CM7 FM7 G7 Am7). Chords follow the scale, so Minor, Dorian or Mbira give their own.
- **Sharps**: a chromatic kalimba's second row. Each black key plays the tine of the white key to
  its left, a semitone up.
- **Perform**:

  | F♯3 | G♯3 | A♯3 | C♯4 | D♯4 | F♯4 … F♯5 (six keys) |
  |---|---|---|---|---|---|
  | Mute (all tines) | Hole (cover the sound hole while held) | Freeze | Octave down while held | Octave up while held | Steel, Brass, Bronze, Aluminium, Bamboo, Glass |

### The pages

- **HOME, Tine**: Material, Hardness (thumb ↔ nail), Decay, Tone (how fast the upper modes die).
- **ENV, Body**: Body (None, Board, Box, Gourd), Buzz (mbira buzzers), Wah (how much of the sound
  hole is covered), Wah rate (*Hand* means no LFO, so the hole is moved by the knob, the mod wheel,
  pressure or the Hole key; otherwise a hand fluttering over it, 0.1 to 8 Hz). The hole only exists
  on Box and Gourd. The bodies, all at about the same loudness:
  - *None*: the bare tine.
  - *Board*: a plank, as on a board kalimba. Thin and bright, with a woody knock.
  - *Box*: a hollow box with a sound hole. Warm, with a bloom around 200 Hz and softened highs.
  - *Gourd*: a calabash resonator, as under an mbira. Boomy, hollow and dark.
- **FX, Space**: Reverb, Size, Delay (echo level), Time (40–740 ms, ping-pong).
- **LFO, Grain**: Grains (level), Grain ms (20–500), Density (grains/s), Pitch (−12, −7, 0, +7,
  +12, +19, Shimmer = unison/fifth/octave at random, Reverse).
- **SEL, Keys**: Layout, Scale, Key, Black.
- **SEQ, More**: Feedback (delay), Spray (grain position, timing, detune and pan scatter), Strum ms,
  Release.
- **GLO, Setup**: Tune (±50 cents), MIDI ch (Omni or 1–16), MIDI out, Width. The header shows the
  audio CPU load and any dropouts, so you can check the load on the hardware.

### Materials

| | Modes (× the fundamental) | Character |
|---|---|---|
| Steel | 1, 5.9, 16.2, 31.5 | The classic: long sustain, a clear sixth-partial "ping", a slow beat |
| Brass | 1, 5.6, 15.3, 29.8 | Softer metal, lower partials, warmer and shorter |
| Bronze | 1, 2.76, 5.40, 8.93 | Played as a bar free at both ends: bell-like |
| Aluminium | 1, 6.27, 17.55, 34.4 | The ideal cantilever: bright and tinny, quicker decay |
| Bamboo | 1, 4.6, 11.8, 22.3 | High internal loss: a short, woody knock (array mbira) |
| Glass | 1, 3.01, 6.24, 10.4 | Barely any damping of the upper modes: crystalline |

A uniform clamped-free bar has its modes at 1, 6.27, 17.55 and 34.39 (Euler–Bernoulli). A real tine
is bent up and thinner at the tip, which pulls them down, more so in soft brass. Higher tines decay
faster, upper modes faster still (more so with Tone turned down). Re-plucking a ringing tine reuses
its vibration, partly damped by the thumb, just as a real tine does.

### Scales

Major, Minor, Penta, Penta m, Dorian, Mixolydian, Lydian, Harmonic minor, Hirajoshi, Blues, Mbira
and Chromatic. Mbira is an approximation of a Shona *Nyamaropa* tuning (close to an equal seven-step
octave, with a near-pure fifth). No two real mbira are tuned alike. The lowest tine sits in F♯3–F4
for any key, so a kalimba in C starts on C4 like the common 17-key one. OCT−/OCT+ move it.

### MIDI

MIDI comes in over USB and the TRS jack, on the channel set on Setup (Omni by default).

- **Notes** pluck the tine of that pitch (12-TET, with Tune), and velocity is how hard. **Note off**
  damps when Release is Damp, and **CC64** (sustain) holds the damp until the pedal lifts.
- **CC1** (mod wheel) and **channel pressure**: a finger over the sound hole (the wah).
- **CC72** Decay, **CC73** Hardness, **CC74** Tone, **CC91** Reverb, **CC93** Grains, **CC94** Delay.
- **Program change**: material (0–5, wrapping).
- **Pitch bend**: ±2 semitones, on every ringing tine.
- **CC120 / CC123**: damp everything.
- **Out** (on by default): the keys as notes, on the Setup channel (1 when Omni).

## Building and testing

```
tests/run_tests.sh      # the instrument, the platform pieces, the whole app in a simulator
./build.sh              # build/fimba.fwsc (needs JieLi's toolchain and the AC79 SDK: BUILDING.md)
python3 tools/fm1_install.py build/fimba.fwsc     # install it over USB
```

**In a browser, without the FM-1:** `web/emu/build.sh` (needs Emscripten) builds the emulator, the
firmware's UI and sound engine compiled to WebAssembly. `build/emu/FiMba-1.html` is all of it in one
file: double-click it, or `open build/emu/FiMba-1.html`, and it runs from disk with no web server.
Chrome or Edge is best (Web MIDI works there); the mouse, touch and computer keyboard work anywhere.

`build/host/kalimba_host SCRIPT OUTDIR` runs the whole app on a computer from a script
(`tests/scenarios/*.kal`): screenshots, audio and the MIDI it sends come out. The engine test writes
one WAV per material to `build/host/materials/`.

**Status:** the firmware package has not been built or run on an FM-1 yet. JieLi's toolchain and SDK
couldn't be downloaded where this was written. Everything above is tested in the host simulator, and
the device sources type-check, but the first `./build.sh` and the first boot on hardware are still
to come. The engine's worst case (12 tines, gourd, buzz, dense grains, delay, reverb) costs about
1.15× FoMni's busiest case on a Mac. Check the CPU figure on the Setup page; if it's tight,
`KM_NVOICE` in `firmware/src/dsp/kalimba.h` is the knob. If it ever fails to start twice, it comes up
in safe mode with USB on, so you can reinstall.

## Credits

The platform (hardware layer, USB, update loader, storage, installer), the app's frame and the plate
reverb are [FoMni](https://github.com/charlesvestal/fm1-fomni)'s by Charles Vestal, which builds on
[X0X](https://github.com/charlesvestal/fm1-x0x) and [Felucca](https://github.com/hugelton/Felucca) by
Leo Kuroshita (Hügelton Instruments). GPL-3.0-only; see [LICENSING.md](LICENSING.md).

FiMba-1 isn't affiliated with or endorsed by M-VAVE, Hügelton Instruments or the authors of FoMni.
