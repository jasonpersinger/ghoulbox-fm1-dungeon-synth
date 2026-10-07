# GHOULBOX: FM-1 Dungeon Synth

![GHOULBOX 0.3: boot, HOME, PRESETS, ENV](docs/ghoulbox-0.3.png)

**GHOULBOX** is dungeon-synth firmware for the **M-VAVE FM-1**: a fork of
[Felucca](https://github.com/hugelton/Felucca) by Hügelton Instruments, tuned for dark ambient,
medieval and fantasy synth music, from Old Tower and Hole Dweller to Mortiis-era dungeon synth.
Everything Felucca does still works; GHOULBOX adds:

- **HALL reverb**: a third REVERB TYPE (FX > REVERB, KNOB 1) beside ROOM and SPRING, a Dattorro-style
  tank. SIZE runs from a 1.5 s stone chamber to a 12 s cathedral, and DAMP darkens the tail. It borrows
  the delay's buffer, so it needs no extra RAM; with HALL selected, the delay reaches 0.74 s.
- **TAPE and CRSH** (FX > TAPE page): the whole mix through a worn cassette (wow, flutter, saturation,
  treble loss, hiss) and a cheap sampler (fewer bits, lower rate). Both off by default.
- **GURDY**, a hurdy-gurdy engine (after PHYS). It plays one note at a time, like the real instrument:
  a bowed melody string, a drone and a fifth on a note you choose (DRN), and the trompette's buzzing
  bridge struck in time with the tempo (BUZZ, COUP).
- **29 dungeon presets** across 9 engines: crypt pads, string machines, monk and crypt choirs, cathedral
  and reed organs, lute, harpsichord, harp, tower flutes and recorders, war and mournful horns, cave wind,
  torch crackle, and five hurdy-gurdies. They come after each engine's own sounds.
- **An old-RPG screen**: crisp VT323 pixel type, stone windows, a sword cursor, flickering wall torches,
  and the CRYPT palette (stone, bone, torch amber, blood red). Your saved palette stays; pick CRYPT in
  HOME (hold) > COLOR.

Version 0.3 (shown on HOME held > ABOUT). It is a hobby project tested on one FM-1:
**install at your own risk.**

## Install

GHOULBOX installs exactly as Felucca does, from Felucca's web installer running on your own computer:

```
./build.sh                         # see BUILDING.md: JieLi toolchain + AC79 SDK
python3 tools/ghoulbox_serve.py    # http://localhost:8000/webapp/installer/ in Chrome or Edge
```

The installer saves a full backup of your music, sounds and settings before it writes anything.
To go back, reinstall Felucca from [its installer](https://hugelton.github.io/Felucca/) and restore the
backup in the editor; its "Return to official V15" returns to M-VAVE's firmware.

## Sound auditions

`tests/dungeon_demo.c` renders a passage and every GHOULBOX preset through the firmware's own audio
code, so you can hear a change before flashing:

```
gcc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/dungeon_demo tests/dungeon_demo.c -lm
build/host/dungeon_demo build/dungeon_demo     # dungeon_*.wav and presets/NN_NAME.wav
```

## Credits and licence

GHOULBOX changes © 2026 Jason Persinger ([@jasonpersinger](https://github.com/jasonpersinger)),
GPL-3.0-only, like Felucca. It is built entirely on **Felucca** by Leo Kuroshita / Hügelton Instruments
(GPL-3.0-only); please support the original ([GitHub Sponsors](https://github.com/sponsors/hugelton),
[itch.io](https://hugelton.itch.io/felucca)). GHOULBOX adds the VT323 font (Peter Hull, SIL OFL 1.1) and
the Pirata One font (Rodrigo Fuenzalida, Nicolas Massi; SIL OFL 1.1); see [LICENSING.md](LICENSING.md).
M-VAVE and FM-1 are trademarks of their owners; GHOULBOX is not affiliated with or endorsed by them or by
Hügelton Instruments.

Felucca's own README follows.

---

# Felucca

[![License: GPL-3.0-only](https://img.shields.io/badge/license-GPL--3.0--only-blue.svg)](LICENSE)
[![Sponsor](https://img.shields.io/badge/Sponsor-ea4aaa?logo=githubsponsors&logoColor=white)](https://github.com/sponsors/hugelton)

![Felucca 1.0](docs/felucca-1.0.png)

**TL;DR:** Felucca 1.0.3 — Big New Features, field testing. Connect your FM-1 to a computer by USB,
open the [web installer](https://hugelton.github.io/Felucca/) in Chrome or Edge, and press Install;
no extra hardware is needed. Installing is at your own risk: M-VAVE's updater or the installer's
**Return to official V15** takes you back.

Multi-engine synthesizer firmware for the M-VAVE FM-1. Please report what you find in
[Issues](https://github.com/hugelton/Felucca/issues).

- Install: [web installer](https://hugelton.github.io/Felucca/) (Chrome or Edge, USB), or `tools/fm1_install.py` from a terminal
- Editor: [web editor](https://hugelton.github.io/Felucca/webapp/editor/); its development has moved to
  [Felucca-WebApp](https://github.com/hugelton/Felucca-WebApp)
- Build: [BUILDING.md](BUILDING.md)

<a href="https://hugelton.itch.io/felucca"><img src="https://static.itch.io/images/badge-color.svg" alt="Available on itch.io" width="74"></a>

## Features

- **Thirteen engines** (below), each with its own factory presets
- **Four tracks**, one synth part each with its own engine and sound (drums are the DRUM engine);
  8 voices shared between them. ALGORITHM selects the track on every page
- **Sequencer:** 64 steps per track with chords, ties, accent, slide and per-step chance; a piano
  roll of the steps; a drum grid (white keys = steps, black keys = lanes); motion recording of knob
  moves (a motion icon marks the cards it drives); live loop recording with overdub; divisions listed
  by length, 4 bars to 1/32; loading a sound never touches your patterns
- **Songs:** chain patterns A–D
- **Chord keys:** one finger plays an in-key chord (triads or sevenths of the scale, or fixed chord
  shapes), with voicings; on the keys, MIDI in, recording and the arpeggiator
- **Arpeggiator** with REPEAT and a beat LED, 16 scales with a white-key mode, glide,
  MONO / LEGATO / UNISON
- **Modulation matrix:** 4 slots per track, MIDI controllers as sources
- **Effects:** distortion and the SLICER per track; chorus, delay and reverb sends (the reverb as
  ROOM or SPRING); master limiter
- **FX layer:** hold FX for repeat, reverse, filter sweeps, tape stop, freeze and a harmonizer
  (OCT UP / OCT DN with shimmer), and mutes on the black keys; MENU > FX LATCH makes them toggle, so
  nothing has to stay held
- **Quick layers:** hold FX, GLO, SCL or EDIT for shortcuts on the keys and knobs, or double-tap it to
  keep the layer open; one-step undo (SAVE held); REC on every page; OCT+ confirms, OCT- goes back
- **Presets:** factory presets, 32 user preset slots and 4 projects, named on the device (an FM6
  sound keeps its own patch in both); projects from every earlier version load
- **Screen:** flat UI with Inter Tight and Fukiai icons, FLAT or LINE style, 10 palettes including
  grayscale, black and white, high contrast and NIGHT; tracks numbered 1–4 on small cushions;
  MENU > LARGE for tall knob cards with larger labels and values
- **LEDs:** idle buttons and keys glow dim so the panel can be found in the dark (MENU > LEDS: OFF,
  DIM LO, DIM HI or INV, the official firmware's look); PLAY turns green while playing; the keys show
  the notes the sequencer and MIDI IN play
- **USB:** class-compliant MIDI in and out, and a 44.1 kHz stereo audio input ("Felucca") that
  records the master output on the computer, no driver needed (at a fixed level with MENU > USB LEVEL FIXED;
  on macOS 13–15, set MENU > USB SERIAL to OFF so the audio input appears)
- **MIDI:** USB and TRS MIDI in; with ROUT CH1-4, channels 1–4 play tracks 1–4 and 5–16 are ignored
  (ROUT SEL: every channel plays the selected track), and the keys send on the track's channel; pitch
  bend, sustain, panic; clock from internal, USB or TRS
- **Web:** editor for every parameter (with a 6-operator FM patch editor), step grid, mixer,
  preset library, sample upload and recording with trim; full backup and restore; return to the
  official firmware

## Controls

![FM-1 controls with Felucca](docs/controls.jpg)

- Tap a page button for its page, again for the next; HOME returns home
- Hold FX, GLO, SCL or EDIT for its quick layer; hold SAVE to undo, HOME for the menu, SEQ for the song
- Double-tap FX, GLO, SCL or EDIT to keep its layer open without holding; tap it again to close
- On action pages, in dialogs and the menu, OCT+ does it and OCT− goes back
- Save a sound: stop, tap SAVE, pick a slot with KNOB 1, then OCT+ and OCT+ again (name it with the keys)

## Menu

Hold **HOME** for the menu. PRESETS moves through the rows, KNOB 1 or OCT+ changes the value, hold HOME
again to leave. Every new setting defaults to the earlier behaviour.

- **COLOR:** the palette. GREY (grayscale, called MONO up to 1.0.1), GREEN, AMBER, ICE, VIOLET, ROSE,
  PAPER (light), HI-CON (high contrast), NIGHT (the 0.9 look: true black, green-tinted text) and MONO
  (black and white)
- **STYLE:** FLAT (filled cards) or LINE (areas divided by thin lines)
- **LARGE:** OFF or ON: on HOME and the value pages the four knob cards grow tall, marked K1–K4, with
  larger labels and values about twice the size; the graph below becomes a strip. Lists, the piano roll,
  the drum grid and the quick layers keep their layout with larger labels
- **ANIM:** ON or OFF; OFF shows every change at once, without rolling digits or a gliding piano roll
- **LEDS:** OFF (no glow), DIM LO, DIM HI (default) or INV (the idle LEDs lit, the active ones dark)
- **HOLD:** how long FX, GLO, SCL or EDIT is held before its map shows
- **KNOB ACCEL:** OFF (one step per click) or ON: a fast, steady turn of a wide value moves 2 to 4 steps
  per click; lists never jump
- **FX LATCH:** ON, FX + an effect key turns the effect on until pressed again, the knob macros stay
  where you leave them, and FX + OCT− turns everything off
- **BPM LOCK:** ON, SELECT no longer changes the tempo; hold GLO and turn SELECT, tap F4 in the GLO
  layer or use GLO > GLOBAL instead
- **SPEAKER EQ:** FLAT, LOWCUT or BASS+, a tone setting for the built-in speaker (not a switch). It also
  shapes the headphone out and USB audio, so keep it on FLAT when recording. The firmware cannot turn
  the speaker off, but headphones in the jack do
- **USB LEVEL:** MASTER (the MASTER knob sets the USB audio level too) or FIXED (USB always at full
  level, MASTER sets only the speaker and headphones)
- **USB SERIAL:** ON or OFF, applied when the menu closes (the FM-1 reconnects). OFF leaves out the
  serial console, a developer tool, so the FM-1 is a plain audio + MIDI device; this lets macOS 13–15
  see its USB audio input. MIDI, the editor and the installer work either way
- **HARDWARE CALIBRATION** and **ABOUT**

## Engines

In the order the device lists them:

- **ANALOG**: virtual analog; two oscillators, noise, drive, resonant low-pass filter
- **FM6**: classic 6-operator FM (Dexed-based): 32 algorithms, a full patch per track edited in the
  web editor (which imports .syx files), macros on the device, an algorithm chart on screen; SLOT
  picks a factory patch (F1–F8) or the track's own (OWN)
- **PHASE**: phase distortion (ported from CrispyZebra)
- **LOFI**: chiptune; pulse, triangle, saw, noise and a 4-bit wave RAM, stepped envelope, sweep, arpeggio
- **SAMPLE**: multisampled instruments and 3 user sample slots
- **VOICE**: formant oscillator, sung vowels
- **TRIO**: 3 oscillators with ring modulation and sync, multimode filter
- **WHEEL**: tonewheel-style organ; drawbar registrations, percussion, key click, drive, rotary speaker
- **GRAIN**: granular textures from the built-in samples or a user slot
- **PHYS**: physical models: modal resonators, strings, struck membranes, sympathetic strings
- **NOISE**: noise from analog to digital: colours, crackle, shift-register and metallic tones
- **SLICE**: a drum break or your own sample cut into slices, one per key; set the slices by hand
  on the SLICES page
- **DRUM**: an 8-lane kit of Felucca's own drum voices on the General MIDI key map

The DIGITAL engine of 0.9 has been replaced by FM6: projects and presets with DIGITAL sounds load
as FM6 sounds converted from them. The SAMPLE engine's PERC kit was removed in 1.0.2: sounds and
projects that used it load as the DRUM engine's kit, on the same key map. FM6's patch bank (the B
slots) was removed in 1.0.3: user presets keep their own FM6 patch, and presets that used a B slot get
that patch on the first start of 1.0.3.

**SLICER** (FX page, every track): a tempo-synced 16-step gate or stutter, with 16 patterns.

## Scale keyboard

On the **SCL** page, set **QNT** to WHITE to play the selected scale using only the
white keys (SNAP keeps every key and rounds it down to the scale). C4 plays **ROOT**; consecutive white keys play consecutive scale notes
above and below it. Black keys are silent, including during live recording and
step entry. **TRN** transposes the resulting notes; the octave buttons shift them
by full octaves. Set QNT to OFF for the normal chromatic keyboard. QNT SEQ snaps the keys like SNAP and
also maps the sequenced notes onto the current ROOT / SCALE as they play (the steps are not changed; drum
kits are never quantized).

Available scales: chromatic (CHR), major (MAJ), natural minor (MIN), Dorian (DOR),
Mixolydian (MIX), major pentatonic (PEN), minor pentatonic (MPEN), harmonic minor
(HARM), Phrygian (PHRY), Lydian (LYD), Locrian (LOC), ascending melodic minor (MEL),
minor blues (BLUES), whole tone (WHOLE), half-whole diminished (DIMHW), and
whole-half diminished (DIMWH). Scales with other than seven notes continue across
the white keys without repeating notes; their roots need not fall on every C key.
Drum kits and incoming MIDI keep their own note mapping.

Press **SCL** again for the **CHORD** page: CHRD picks the chord keys (OFF, the scale's triads or
sevenths, or a fixed shape) and VOIC the voicing.

## Layout

| Path | What |
| --- | --- |
| `firmware/` | firmware sources: `src/` app, `hal/` hardware layer, `loader/` update loader |
| `tools/` | build script, generators, package maker, installer and sample uploader |
| `assets/` | UI font, icon names, CC0 instrument samples |
| `web/` | the web installer and the editor the site serves (the new editor: [Felucca-WebApp](https://github.com/hugelton/Felucca-WebApp)) |
| `tests/` | tests that run on the build machine |
| `LICENSES/` | licence texts of the bundled font, icons, ported DSP and SDK files |

## If the FM-1 does not start

If an update is interrupted and the FM-1 stays black, check whether a computer sees it as a USB device named
**WL80UBOOT** (or a USB mass-storage device with ID 4C4A:8057). That is the chip's built-in boot mode, and
the FM-1 can be brought back:

- First try another USB data cable, and close every other app that uses MIDI, then run the web installer again.
- If it stays in boot mode, [FM-1 Transporter](https://github.com/kurogedelic/FM-1-transporter) reads and
  writes the FM-1's flash from a Mac through a Seeed XIAO RP2040 (three wires to the FM-1's USB lines). Back up
  the flash first, then write the official firmware (M-VAVE's FM-1.fwsc).
- Questions: [Issues](https://github.com/hugelton/Felucca/issues).

## Support

If Felucca is useful to you, [sponsoring on GitHub](https://github.com/sponsors/hugelton) or a donation
on [itch.io](https://hugelton.itch.io/felucca) helps keep its development going.

Issues are for reproducible bugs (one per issue). Ideas and requests go to
[Discussions](https://github.com/hugelton/Felucca/discussions), and feature requests posted as issues will be
moved there. Pull requests are welcome: see [CONTRIBUTING.md](CONTRIBUTING.md).

## Credits

- **[Hügelton Instruments](https://hugelton.com)** (Leo Kuroshita, [@kurogedelic](https://github.com/kurogedelic)):
  Felucca itself; the PHASE engine's waveforms (a C port of the oscillator of
  [CrispyZebra](https://github.com/hugelton/CrispyZebra), GPL-3.0); the DRUM voices; the Hügelton Sample
  Pack (the drum samples, GPL-3.0-only, not CC0); the [Fukiai](https://github.com/hugelton/Fukiai) icon
  font ([MIT](LICENSES/MIT-Fukiai.txt))
- Font: [Inter Tight](https://github.com/rsms/inter-tight) by The Inter Project Authors, [SIL OFL 1.1](LICENSES/OFL-InterTight.txt)
- Samples: [Versilian Studios](https://versilian-studios.com/) [VSCO-2 Community Edition](https://github.com/sgossner/VSCO-2-CE) and [VCSL](https://github.com/sgossner/VCSL), CC0 1.0 ([attribution](assets/samples-cc0/ATTRIBUTION.txt))
- VOICE engine: after [klattsch](https://github.com/tgies/klattsch) by Tony Gies (MIT); formant data from Klatt (1980) and Hillenbrand et al. (1995)
- PHYS engine: models ported from [DaisySP](https://github.com/electro-smith/DaisySP) by Electrosmith and Emilie Gillet ([MIT](LICENSES/MIT-DaisySP.txt)) and from Emilie Gillet's [eurorack](https://github.com/pichenettes/eurorack) code ([MIT](LICENSES/MIT-Rings.txt))
- FM6 engine: msfa from [Dexed](https://github.com/asb2m10/dexed) by Google Inc. and Pascal Gauthier ([Apache-2.0](LICENSES/Apache-2.0-msfa.txt))
- Package format and boot files: [JieLi AC79 SDK](https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK) ([Apache-2.0](LICENSES/Apache-2.0.txt); three of its files are in every package, none in this tree)
- Contributions: [keremimo](https://github.com/keremimo) (white-key scales, #2), [ChanceTheMaker](https://github.com/ChanceTheMaker)
  (TRS MIDI, bend, sustain and clock, palettes, favourites, editor display settings: #8, #10, #11, #12),
  [andreahaku](https://github.com/andreahaku) (sample recording and trim, #29; SLICE manual slices and tests, #27, #22)

## Licence

Free software: [GPL-3.0-only](LICENSE), the Hügelton Sample Pack included. The bundled font and the
ported DSP keep their own licences ([LICENSES/](LICENSES/)); details in [LICENSING.md](LICENSING.md).

M-VAVE and FM-1 are trademarks of their respective owners. Felucca is not affiliated with or endorsed by them.

Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
