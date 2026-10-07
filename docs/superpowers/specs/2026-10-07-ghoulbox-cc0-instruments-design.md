# GHOULBOX 1.1: sampled instruments — design

Date: 2026-10-07. Status: approved 2026-10-07 (folk harp chosen).

## Intent

GHOULBOX 1.0's sounds are all synthesized. 1.1 adds real recordings of four dungeon instruments, from public-domain
(CC0) libraries, so the FM-1 can play a bowed psaltery, a Renaissance organ, a recorder and a plucked instrument
(a folk harp) as themselves. A bagpipe, which neither library has, comes as a synthesized preset. Success:
each instrument sounds like itself on the device, sustained ones hold without clicks, and nothing a 1.0 owner saved
plays differently.

## Choices made

| Question | Choice |
|---|---|
| Instruments | Bowed psaltery, Renaissance organ, recorder, folk harp |
| Retired samples (piano, flute, sax) | Kept: 1.0 projects and user presets that use them play as before |
| Bagpipe | A synthesized preset (no CC0 bagpipe exists in either library) |

## Sources (CC0 1.0, Versilian Studios)

| Instrument | Library path (sgossner/VCSL) | Kind |
|---|---|---|
| Bowed psaltery | `Chordophones/Zithers/Psaltery, Bowed and Plucked/LongBow/` | looped sustain |
| Renaissance organ | `Aerophones/Edge-blown Aerophones/Renaissance Organ/` (one stop, chosen by ear) | looped sustain |
| Recorder | `Aerophones/Edge-blown Aerophones/Baroque Tenor Recorder/` (a sustained articulation if present, else the longest) | looped sustain |
| Folk harp | `Chordophones/Composite Chordophones/Folk Harp/` | one-shot |

Each set: 3–4 zones (pitches) across the keyboard, the way the current piano / flute / sax are built, through the
existing `tools/gen_samples.py` path (22.05 kHz, IMA ADPCM, crossfaded loops for "sus"). Provenance lines go into
`assets/samples-cc0/ATTRIBUTION.txt`, as for the current sets.

## Flash budget

About 170 KB is free in the app slot with the current samples kept, roughly 15 s of sample time. Target: ≤ 130 KB
for all four sets, leaving ≥ 40 KB for future Felucca merges. Zone lengths are trimmed to fit (sustains need only a
short steady segment to loop; one-shots are cut where they fade below audibility). `tools/build.py` already fails a
build that exceeds the slot; a test reports each set's size.

## Numbering (the compatibility rule)

The SAMPLE engine's SET values are stored in projects and user presets: built-in sets 0–4, then the user sample slots
USR1–3 at 5–7. New sets are **appended after the user slots (8–11)**, so no stored number changes meaning. The SET
knob shows them in a sensible order (the new instruments first) through a display order, as ENGINE_ORDER does for
engines; stored values stay as they are. GRAIN, which plays the same sets, gains the four as sources too.

## Presets and menus

- Four SAMPLE presets, one per instrument, appended after the retired ones (stored preset numbers keep meaning), with
  the HALL-friendly sends of the other GHOULBOX presets.
- The SAMPLE engine returns to the engine picker (it now has kept presets); piano, flute and sax stay retired.
- A synthesized **BAGPIPE** preset: a reedy chanter over a drone (engine chosen by ear: GURDY's drones, or FM6 reed).
- The web editor mirrors the new sets, names and presets (its mock-vs-firmware test enforces it).

## Verification

- A test per set: audible at every zone, sustained notes held 5 s have no loop click (no sample-to-sample jump above
  a threshold at the loop seam), one-shot decays end at silence.
- Compatibility: a project / user preset stored with SET 0–7 plays the same set as on 1.0 (USR1–3 unchanged).
- Size report per set; the build stays within the slot with ≥ 40 KB spare.
- Goldens updated only for the new presets; every existing render unchanged.
- Auditions rendered (`tests/preset_preview.c`) for the owner to approve before flashing; then an install on the device.

## Decided

- **Folk harp** over a harpsichord: the more medieval of the two (the synthesized VIRGINAL / HARPSICHORD stay).

## Out of scope

Choir samples (neither library has one), removing the retired samples, user-sample changes, 1.2 items.
