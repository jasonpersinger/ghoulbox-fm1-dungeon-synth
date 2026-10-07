# Felucca editor protocol (SysEx over USB-MIDI)

The firmware side is `firmware/src/editor.c`. Commands 1-15, track selection, dumps, step reads
and parameter writes (27, 29, 30, 31) were checked on hardware. `tests/editor_test.c` also exercises the real C
handler, malformed transfers and stop timeouts against simulated flash. User-preset
flash writes and live pushes (16-26, 32) have host coverage; that does not establish
their hardware behavior. The step chance byte, motion (64) and the full backup (65-67) have host
coverage only (`tests/editor_test.c`, `tests/backup_test.c`, `web/test_backup.mjs`).

**v3 (four tracks):** the device has four tracks, each a synth part (since 1.0; before it, track 4
was a GM drum track: see "Track 4 since 1.0" below). One
of them is *selected* (ALGORITHM on the device, or `TRACK`). Every v1 / v2 command acts on the
selected track (its parameters, engine, preset, steps, the user presets it stores or loads); `TRACK`,
`TRACK_MIX`, `TRACK_DUMP` and `TRACK_STEP` reach any track. Command numbers 1-26 are unchanged.

**v4 (any track's parameters):** `TRACK_PARAM` gets or sets a parameter of any track without changing
the selection, and the `TRACK_CHANGED` push follows level, pan and mute of the tracks that are not
selected. The v1-v3 commands are byte for byte as before; v4 is asked for with bit 1 of `WATCH`.

**v5 (the drum grid):** a step also holds lane hits and their accents (see "v5: the drum grid" below).
`STEP_GET` / `STEP_SET` / `TRACK_STEP` replies get 3 more bytes, `STEP_SET` / `TRACK_STEP` take them
optionally, and `UP_GET` / `UP_PUT` carry a user preset's 16-step grid after its pattern. Every
earlier byte is where it was; an editor that reads the first bytes of a reply keeps working.

**v7 (motion, chance, backup, 89 parameters):** a step gets a chance byte after its hits. `MOTION` (64) reads
and edits a track's recorded knob moves. `BACKUP_LIST` / `BACKUP_GET` / `BACKUP_PUT` (65-67) read and
restore the whole device. P_COUNT is 89 and P_E0 is 81: twenty FM operator parameters (ids 61..80) went in
before the engine parameters, which moved from 61..68 to 81..88. Always take P_E0 from `INFO`. INFO ends with
tagged capability blocks for these (see `INFO`).

**MENU settings (1.0.4):** `MENU_DESC` (72) and `MENU_SET` (73) read and set the device's MENU settings, described
by the device (see "MENU settings" at the end); INFO advertises them with `4E 01 count`. 1.0.5: a `MENU_DESC` reply
ends with the item's tab on the device (index and name), after the bytes 1.0.4 sent.

**Ratchet (1.0.5, INFO `52 01 04`):** a step gets a ratchet byte (its hits, 1..4) after its chance: one more byte at
the end of the step replies, and an optional last byte of a step write (see "Ratchet" below). Nothing else changed.

**Chord keys (91 parameters, 1.0):** two track parameters, `CHRD` (81) and `VOIC` (82), went in before the
engine parameters, which moved from 81..88 to 83..90: P_COUNT 91, P_E0 83. No command changed; an editor that
takes P_COUNT and P_E0 from `INFO` keeps working (see "The chord keys" below).

## Framing

A request is `F0 7D 46 4C <cmd> <args...> F7`:

- `7D` is the non-commercial SysEx ID.
- `46 4C` is "FL".

Every valid request gets exactly one reply, with the same header and the same `<cmd>`.
Unknown commands and invalid fixed argument lengths get no reply; argument errors
follow the command-specific rules below.
Fixed-size commands require exactly the lengths in the tables; step writes accept the
8-byte legacy step, the 11-byte grid step, the 12-byte step with its chance (a chance above 100 gets
no reply) or the 13-byte step with its ratchet (a ratchet outside 1..4 gets no reply). Every data byte is 7 bit.
MIDI realtime bytes may occur inside SysEx; another status aborts the partial frame. While the editor
watches (v2, `WATCH`), the device also sends push frames (cmds 23, 24, 26) at any time.

| Item | Encoding |
| --- | --- |
| value (v14) | 2 bytes, LSB first, holding value + 8192, so the range is -8192..8191. `[lo, hi]`: value = (lo \| hi << 7) − 8192 |
| u32 (v7) | 5 bytes, 7 bits each, LSB first; the last byte is 0..15. Used by the backup commands |
| string | ASCII bytes, ended by a 0 byte |
| scope | 0 = parameter of the selected track (`P_*`, 0..P_COUNT−1); 1 = global parameter (`G_*`, 0..G_COUNT−1) |
| track | 0..3: tracks 1..4 (synth parts) |
| engine byte | 0..NENGINES−1 (firmware before 1.0: NENGINES = its drum track, no engine). The numbers are fixed, new engines are appended: 0 ANALOG, 1 reserved (DIGITAL before 1.0: see below), 2 PHASE, 3 LOFI, 4 SAMPLE, 5 VOICE, 6 TRIO, 7 WHEEL, 8 GRAIN, 9 PHYS, 10 DRUM, 11 NOISE, 12 FM6, 13 SLICE (NENGINES 14; a build with `FELUCCA_SLICE=0` has 13). The device and the editor list them in another order (ANALOG FM6 PHASE LOFI SAMPLE VOICE TRIO WHEEL GRAIN PHYS NOISE SLICE DRUM: `ENGINE_ORDER`); the numbers stay |

The engine parameters are `P_E0..P_E7`: P_COUNT−8 .. P_COUNT−1 (83..90), and `INFO` gives `P_E0`.
Their meaning, range and names depend on the current engine, so re-read `DESC` for them
after an engine change.

The modulation matrix (the MOD page, `firmware/src/mod.c`) is twelve track parameters at ids 49..60 (since
P_COUNT 69): slot k (1..4) is `SRCk`, `DSTk`, `AMTk` at ids 49 + 3 (k − 1) .. 51 + 3 (k − 1).

| id | label | values |
| --- | --- | --- |
| 49, 52, 55, 58 | SRC1..SRC4 | enum: 0 OFF, 1 LFO, 2 ENV, 3 VEL, 4 KEY, 5 RAND, 6 MODW (CC1), 7 AT (channel aftertouch), 8 EXPR (CC11) |
| 50, 53, 56, 59 | DST1..DST4 | enum (20 names): 0 OFF, 1 PITCH, 2 CUT, 3 SHP, 4 AMP (per voice); 5 PAN, 6 DIST, 7 CHO, 8 DLY, 9 REV, 10 RATE (LFO rate), 11 VIB (LFO pitch depth), 12..19 E1..E8 = `P_E0..P_E7` (per block) |
| 51, 54, 57, 60 | AMT1..AMT4 | −64..63 (fmt BIPCT) |

`DESC` names E1..E8 as such; the device shows the engine's label of that parameter instead (`DESC` of
`P_E0 + n`), and so does the editor. A slot with SRC, DST or AMT at 0 does nothing; every default is 0
(a factory preset sets all twelve to 0). The modulation never changes the stored values: `GET`,
`DUMP` and the pushes report what was set.

**DIGITAL (engine 1) was replaced by FM6 (Dexed-based)** in 1.0. The number stays reserved: `INFO` names it
"-" (a build with `FELUCCA_FM4=1` has DIGITAL back, named "DIGITAL"), `NAMES 1` lists no presets, its EDIT
descriptors are "-" with DIGITAL's ranges, and no track ever has it. Whatever brings a DIGITAL sound plays it as
FM6 with a patch converted from its values (`firmware/src/fm4_convert.c`; the patch is the track's own, `FM6_GET`
reads it, a project keeps it): `SET` of `G_ENGSEL` = 1 (DIGITAL's first preset), `PRESET` 1 k (its preset k),
`UP_LOAD` of a slot stored with engine 1 (`UP_PUT` still takes engine 1: the record keeps the DIGITAL values, every
load converts them), projects with DIGITAL tracks (their motion on E1..E8 and ids 61..80 is dropped). The track
reports engine 12 (FM6) and the FM6 preset that covers the sound. The editor converts DIGITAL patches of library
files the same way (it equals the firmware's, `test_web.mjs`).

The four FM operator envelopes (the DIGITAL engine's OP1..OP4 ENV and OP LEVEL pages, `eng_digital.c`) are
twenty track parameters at ids 61..80, five per operator: operator k (1..4) has `ATK`, `DEC`, `SUS`, `REL`,
`LVL` at ids 61 + 5 (k − 1) .. 65 + 5 (k − 1). Since DIGITAL was retired they are inert: no page and no editor
layout shows them, only the conversion of a DIGITAL sound reads them (its FM6 tracks hold the defaults). The ids
and labels stay (library files key values by label).

| id | label | fmt, range, default |
| --- | --- | --- |
| 61, 66, 71, 76 | ATK | TIME, 0..127, 0 |
| 62, 67, 72, 77 | DEC | TIME, 0..127, 0 |
| 63, 68, 73, 78 | SUS | PCT, 0..127, 127 |
| 64, 69, 74, 79 | REL | TIME, 0..127, 0 |
| 65, 70, 75, 80 | LVL | PCT, 0..127, 127 |

The defaults change nothing: the sound is the same as before the parameters existed. The master `ENV`
(ids 1..4) shapes the whole voice; these shape each operator on top of it. Other engines ignore them, and the
device shows the pages for DIGITAL only. Descriptors are the same for every engine.

The chord keys (SCL > CHORD on the device, `firmware/src/chord.c`) are two track parameters at ids 81, 82:

| id | label | values |
| --- | --- | --- |
| 81 | CHRD | enum: 0 OFF, 1 DIA3, 2 DIA7 (the triad / seventh of the track's ROOT and SCALE on the key: every tone in key), 3 MAJ, 4 MIN, 5 DOM7, 6 MAJ7, 7 MIN7, 8 SUS4, 9 POW (fixed shapes) |
| 82 | VOIC | enum: 0 CLOSE, 1 OPEN (1-5-3), 2 INV1, 3 INV2, 4 +OCT (the root an octave down; a seventh drops its fifth) |

With CHRD on, a key, a MIDI note of that track and so the arp's held notes play the chord (at most 4 notes),
and live recording writes it into one step; MONO / LEGATO / UNISON play its root; a kit (DRUM, SLICE)
ignores it. Both defaults are 0: nothing changes until CHRD is set. They are the track's, like ARP and SCL: a
sound load (a factory preset, `UP_LOAD`, an audition) keeps them, and motion never records them.

Other enums that grew: `P_SDIV` (DIV, id 30) has 10 names (1/4, 1/8, 1/16, 1/32, 8T, 16T, 1/2, 1/1, 2BAR,
4BAR; the first six keep their numbers), `P_AMODE` (id 17) 7 (OFF, UP, DN, UPDN, RND, ORD, REPEAT) and the
global `G_CLOCK` (id 2, label "CLK") 3 (INT, USB, TRS). `G_MIDI` (id 12) is an enum of USB / TRS that nothing reads.

**Retired enum values (aliases).** A value that no longer exists keeps its number, so stored sounds stay valid:
`DESC` names it like the value it now plays (an alias), the device never holds it (a `SET` of it lands on that value
and the reply says so) and its knob steps over it. Of a repeated name, the original is the first value when that has
the name, else the last one with it; an editor should list only the originals and show an alias by its name when
it ever sees one. Today: SAMPLE SET and GRAIN SRC 1 and 4 (once TRANH, PERC) = 0 PIANO; DRUM KIT (engine 10, E1)
1, 2, 3 (once HAND, CYM, H+CYM; 1.0.5) = 6 (66), 5 (10), 8 (77): its names are STD 66 10 77 80 10 66 55 77, the
device's knob steps STD 80 10 66 55 77. An editor of before 1.0.5 (first of a name = original) offers 66 10 77
at 1..3 and hides 6 5 8: what it sets still plays the right kit, and the device answers with 6 5 8.

## Commands

| cmd | Request args | Reply args |
| --- | --- | --- |
| 1 INFO | — | version string, NENGINES, P_COUNT, G_COUNT, NSTEP, P_E0, then NENGINES engine-name strings, then (v3) NTRK (4), then (v6) CHAIN_ROWS (16), then the tagged blocks `55 01 uiCaps`, `4D 01 64 01`, `42 01 3`, `46 01 nfactory nbank`, `53 01 3`, (1.0.3) `50 01 3` and (1.0.4) `4E 01 count` (MENU settings) and (1.0.5) `52 01 4` (ratchet) (below); older firmware ends earlier |
| 2 GET | scope, id | scope, id, v14 |
| 3 SET | scope, id, v14 | scope, id, v14 (the value after clamping). Setting global `G_ENGSEL` (id from DESC label "ENG") changes the engine: its defaults, then its first preset (as on the device) |
| 4 DUMP | — | engine, preset, then P_COUNT × v14 (the selected track), then G_COUNT × v14 (globals) |
| 5 DESC | scope, id | scope, id, fmt, min v14, max v14, def v14, label string, unit string, then for an enum (fmt 8) one name string per value (at most 24; firmware before the matrix: at most 16) |
| 6 STEP_GET | index 0..NSTEP−1 | index, n (0..4 notes), note0..note3, time (0 NOTE, 1 TIE, 2 REST), flags (1 accent, 2 slide), vel, then (v5) hits (3 bytes, below), then (v7) chance 0..100, then (`52 01`) ratchet 1..4 |
| 7 STEP_SET | index, n, note0..3, time, flags, vel [, hits (3 bytes, v5) [, chance 0..100 (v7) [, ratchet 1..4]]] | same as STEP_GET (after the write). Without the hits the step keeps its own; without the chance or the ratchet it keeps its own. The chance can only follow the hits, the ratchet the chance |
| 8 PRESET | engine, preset | engine, preset (applies the preset's sound to the selected track and sends; the steps and the track's own parameters stay, see "Sound loads and undo"). For another track, select it with `TRACK` first |
| 9 PROJECT | op (0 load, 1 save, 2 query), slot 0..3 | op, slot, used (1/0). Save writes flash: allow ~2 s; it stops the transport first (see "Saves while playing") |
| 10 NAMES | engine | engine, count, count preset-name strings, then the two edit-page titles |
| 11 SMP_BEGIN | slot 0..2 | slot, rc (0 ok). Erases the slot's header sector: the slot is empty from now on |
| 12 SMP_WRITE | slot, offset (3 × 7 bit, LSB first), pack7 data (≤ 256 bytes) | slot, offset, rc: 0 ok, 1 arguments, 2 erase, 3 write, 4 slot in use (send SMP_BEGIN first). Offset ≥ 512 and a multiple of 256; writes go in increasing order (a write at a 4 KiB boundary erases that sector) |
| 13 SMP_END | slot, pack7 header (480 bytes) | slot, rc: 0 ok, 1 size, 2 header, 3 data CRC, 4 flash, 5 zones |
| 14 SMP_ERASE | slot | slot, rc (erases the whole slot, ~1 s) |
| 15 SMP_INFO | — | slots, slot KiB, then per slot: zone count (0 = empty), name string, data KiB |
| 16 UP_LIST | start, count (1..16) | start, count, total slots, then per slot: used (0/1), engine, name string ("" if unused) |
| 17 UP_GET | slot | slot, used, engine, name, P_COUNT × v14, 16 × (note, flags), then (v5) kind (0 a note pattern, 1 a drum grid) and for kind 1 16 × hi |
| 18 UP_PUT | slot, engine, name, P_COUNT × v14, 16 × (note, flags) [, kind 0 or 1, 16 × hi (v5)] | slot, rc (0 ok, 1 args, 2 flash). Writes flash: allow 1 s |
| 19 UP_STORE | slot, name | slot, rc. Stores the current sound: engine, parameters, the first 16 sequencer steps as the pattern (TIE steps → flag 4) |
| 20 UP_LOAD | slot | slot, rc (0 ok, 1 empty/invalid). Applies its sound (not its pattern; the steps stay) |
| 21 UP_ERASE | slot | slot, rc |
| 22 WATCH | on (0/1; v4: 3 = also `TRACK_CHANGED`) | on (0/1; v4 firmware: 3 when 3 was asked for). While on, the device pushes cmds 23, 24, 26 (and 32 with bit 1) |
| 23 CHANGED (push) | — | scope, id, v14 |
| 24 RELOAD (push) | — | engine, preset, then (v3) the selected track |
| 25 PING | — | 0 |
| 26 STEP_CHANGED (push) | — | index, then (v3) the selected track |

| cmd (v3) | Request args | Reply args |
| --- | --- | --- |
| 27 TRACK | — (query), or track (select it) | selected track, NTRK, then per track: engine byte, preset, level v14, mute (0/1), armed (0/1, live recording) |
| 28 TRACK_MIX | track (get), or track, level v14 (0..127), mute (set) | track, level v14, mute: the track's `P_LEVEL` and `P_MUTE` |
| 29 TRACK_DUMP | track | track, engine byte, preset, P_COUNT × v14 (that track's parameters; no globals) |
| 30 TRACK_STEP | track, index (get), or track, index, n, note0..3, time, flags, vel [, hits (v5) [, chance (v7) [, ratchet]]] (set) | track, index, n, note0..3, time, flags, vel, then (v5) hits, then (v7) chance, then (`52 01`) ratchet |

| cmd (v4) | Request args | Reply args |
| --- | --- | --- |
| 31 TRACK_PARAM | track, id (get), or track, id, v14 (set); id = `P_*` (0..P_COUNT−1) | track, id, v14 (the value after clamping, as `SET`). The selection does not change; no push about the editor's own write |
| 32 TRACK_CHANGED (push) | — | track, id, v14: `P_LEVEL`, `P_PAN` or `P_MUTE` of a track that is not selected changed on the device (only while `WATCH` was sent with bit 1) |

| cmd (v7) | Request args | Reply args |
| --- | --- | --- |
| 64 MOTION | track (query); track, 1, on 0/1 (play on / off); track, 2 (clear); track, 3, step, id, v14 (set an event); track, 4, step, id (delete an event) | track, rc, on (0/1), count (this track's events), max (64), then count × (step, id, v14) |
| 65 BACKUP_LIST | — | 1, rc, count (11), then per object: id, size u32, crc u32 |
| 66 BACKUP_GET | id, offset u32, count lo, count hi (≤ 256) | id, rc, offset u32, count lo, count hi, pack7 data |
| 67 BACKUP_PUT | op 0 begin: 0, id, size u32, crc u32; op 1 data: 1, id, offset u32, pack7 data; op 2 commit: 2, id; op 3 abort: 3, id | op, id, rc |

Without flash (no flash part found at boot) `SMP_BEGIN`, `SMP_WRITE`, `SMP_END` and `SMP_ERASE` get no
reply.

**pack7:** groups of up to 7 bytes, each preceded by one byte holding their top bits
(bit j = bit 7 of byte j). A mask must have at least one following data byte;
unused mask bits in the last group must be zero. Oversized or incomplete transfers
are rejected; SMP_WRITE decodes at most 256 bytes, SMP_END exactly 480 bytes.

**User sample slot** (80 KiB each, SAMPLE engine sets USR1..USR3; reference uploader
`tools/fm1_sample_upload.py`, slot builder `sampleio.user_slot`; the editor's port of it is
checked byte for byte by `web/test_web.mjs`): header at 0, ADPCM data at 512.

| Offset | Field |
| --- | --- |
| 0 | magic `"FSMP"` (u32 0x504D5346), u16 version 1, u8 zone count 1..16, u8 0 |
| 8 | name, 8 ASCII bytes (0-padded) |
| 16 | u32 data length (bytes), u32 CRC-32 (zlib) of the data, 8 bytes 0 |
| 32 | 16 zones × 28 bytes: u32 off (in the data), n (samples), loop start, loop end, rate (Hz / 44100 × 65536); i16 root × 16 (MIDI note), ADPCM predictor at the loop start; u8 step index at the loop start, lo note, hi note, looped (0/1) |

Data is IMA ADPCM, 4 bit, low nibble first, starting from predictor 0 and step index 0.
All little endian.

The slot's last 4 KiB sector (offset 0x13000) holds SLICE's slices set by hand on the device (`src/slice_store.c`:
magic `"SLM1"`, the sample's length and data CRC, the slice starts) when the data leaves it free (at most 77,312
bytes). An upload writes over it like any other data; a record that is not the slot's sample's is ignored.

`fmt` values (`firmware/src/core.h`):

| Value | Name | Value | Name | Value | Name |
| --- | --- | --- | --- | --- | --- |
| 0 | INT | 5 | CUTOFF | 10 | NOTE |
| 1 | PCT | 6 | DB | 11 | ONOFF |
| 2 | BIPCT | 7 | SEMI | 12 | OCT |
| 3 | TIME | 8 | ENUM | 13 | STEPS |
| 4 | LFOHZ | 9 | BPM | | |

The editor should show the value with the unit; formatting it exactly like the device does
is not required.

## v2: user presets

A user preset = engine (0..NENGINES−1), name (1..12 chars, ASCII 32..126; the device shows it upper
case), all P_COUNT instrument parameters (v14 each, the same order as `DUMP`), and a 16-step pattern:
16 × (note 0..127 (0 = rest), flags: 1 accent, 2 slide, 4 tie, 8 | 16 the ratchet − 1 (firmware with `52 01`;
before, 0: x1, and such firmware drops those bits when it loads the pattern)). Loading one applies the engine and
the parameters of the sound, as a factory preset (the track's own parameters, the steps and LEN stay; see
"Sound loads and undo"). The stored pattern is kept and returned by `UP_GET`; on the device SEQ > PHRASES
lists it as "U07" and loads it, with the stored LEN (at most 16), DIV, SWING and GATE. The slots are
numbered 0..31 (the device shows U01..U32).

- `UP_LIST`: count is cut at 16 and at the last slot (start ≥ 32: count 0, no entries).
- `UP_GET` of an empty slot has the same shape with used 0, engine 0, name "" and all values 0.
  Values come back in the current parameter order, inside their ranges.
- `UP_PUT`: rc 1 for a slot ≥ 32, an engine ≥ NENGINES, a name that is empty, longer than 12 or has
  bytes outside 32..126, or an incomplete or oversized frame. Values are clamped to their ranges for that
  engine. A note with flag 4 is stored as a tie (note 0); flags on a rest are dropped.
- **DRUM** (engine 10) was PHYS's MODEL 4 (DRUM) before 1.0. A record of PHYS with E1 (MODEL) = 4,
  stored then or sent by `UP_PUT` from an older editor, is the DRUM engine: the device rewrites it (engine
  10; E1..E8 {MODEL, TUNE, TONE, DECY, SNAP, ACC, KICK 0..127, PERC 0..127} become {KIT = PERC / 32, TUNE,
  TONE, DECY, SNAP, ACC, KICK 0 PUNCH / 1 ROUND (old ≥ 64), DRV 0}), and `UP_GET` / `UP_LIST` give it so.
  Projects do the same. PHYS's MODEL is 0..3 (MODAL STRNG MEMB SYMP) now.
- **SAMPLE SET 4** was PERC, the General MIDI drum kit, until 1.0.2. A record of SAMPLE (engine 4) with E1 (SET)
  = 4, stored then or sent by `UP_PUT`, is the DRUM engine with its default kit: the device rewrites it (engine 10,
  E1..E8 = DRUM KIT's {0, 64, 70, 64, 64, 100, 0, 0}, the other values and the pattern as they were). Projects
  and `PRESET` 4 / 4 do the same (a project's track keeps its steps). SET 4 itself stays (`DESC` names it "PIANO":
  an alias, as SET 1; a `SET` of 4 lands on 0), and USR1..USR3 stay 5..7; GRAIN's SRC 4 plays PIANO.
- **DRUM KIT 1..3** were HAND, CYM and H+CYM until 1.0.4 (STD with a conga / claves, a cymbal, both). Since 1.0.5
  they play the VA kits 66, 10 and 77, and a record holding one loads as that kit: `UP_GET` / `UP_LOAD` give KIT 6, 5
  or 8 (the record itself is not rewritten). Projects, motion events and `SET` do the same.
- `UP_STORE`: name "" stores with the automatic name the device uses (engine name + slot number,
  "ANALOG 07"). rc 1 for a bad slot or name.
- rc 2 = the flash write failed or there is no flash. A failed flash write keeps the previous
  record in RAM and flash, including its name. Without flash, a valid change is RAM-only until power-off.
  (`UP_PUT` / `UP_STORE` / `UP_ERASE` stop the transport first, see "Saves while playing"; a device
  that cannot stop it in 100 ms answers rc 2 and writes nothing.)
- Frames stay below 640 bytes (`UP_PUT` is 5 + 1 + 1 + 13 + 2 × P_COUNT + 32 + 1).

**On the device:** SAVE > USER page: KNOB 1 picks the slot, KNOB 2 LOAD, KNOB 3 ERASE, KNOB 4 SAVE;
OCT+ executes the selected action, OCT- goes back. SAVE over a used slot asks
"OVERWRITE U07?" with OCT+ / OCT-. SAVE uses the automatic name. The PRESETS knob and the SAVE > PRESETS browser continue past the factory presets into the used user
presets.

**Flash** (`firmware/src/upreset.c`): two storage objects (`OBJ_UPRESET0/1`, A/B sector pairs at
0xDC000..0xDFFFF), 16 records of 192 bytes each, behind a bank header (magic "UPB1", record size,
slot count; a mismatch reads as an empty bank). A record keeps its layout version (versions 1..5 are read; another: empty)
and the P_COUNT it was stored with; another count is mapped by count (last 8 values = P_E0..P_E7, the
first ones = P_LEVEL.. in order, missing ones = defaults). Versions 4 (a note pattern) and 5 (a drum grid)
store each value as one byte, value + 64, so P_COUNT can reach 127; versions 1..3 stored 16-bit values.
The wire format is unchanged: `UP_GET` / `UP_PUT` still carry v14 values, and `UP_PUT` now answers rc 1
for a value outside −64..127. P_COUNT was 53 (P_E0 45) until the SLICER
parameters (SLCR, PAT, RATE, DEPTH: ids 45..48) went in just before P_E0: P_COUNT 57, P_E0 49; then
the twelve matrix parameters (ids 49..60): P_COUNT 69, P_E0 61. An editor takes both from `INFO`;
records stored with 53 load with the SLICER off and every matrix slot off, with 57 with every slot off. The
twenty FM operator parameters (ids 61..80) went in before P_E0 after that: P_COUNT 89, P_E0 81. Records stored with 69 load with
those at their defaults, which change nothing. Then the chord keys (ids 81, 82): P_COUNT 91, P_E0 83; records
stored with 89 (or 69) load with CHRD OFF and VOIC CLOSE, their engine values at 83..90.

## v5: the drum grid

A DRUM track (engine 10) is edited on the device as a grid of 8 lanes × the steps (SEQ > STEP). The
grid lives in the steps themselves, so every engine has it:

- **Hits.** Each step has a lane mask `hit` and an accent mask `acc` (bit l = lane l, `acc` only on
  lanes that hit). The lanes and the General MIDI note each plays: 0 KICK 36, 1 SNARE 38, 2 CLAP 39,
  3 HAT CL 42, 4 HAT OP 46, 5 TOM 45, 6 RIM 37, 7 BELL 56 (a VA kit plays its own piece on each: KIT 66 a conga
  on lane 5, 77 claves on lane 6, 10 and 77 a cymbal on lane 7). A NOTE step plays its notes and then its hits, on any engine (DRUM strikes its
  lanes, a synth plays the pitches); an accented hit at velocity 127, the others at
  the step's velocity (0 = 96). A TIE or REST step plays no hits.
- **On the wire** (after `vel`): `hit & 127`, `acc & 127`, then `(hit >> 7) | (acc >> 7) << 1`. A
  `STEP_SET` / `TRACK_STEP` of 8 step bytes (an editor of before v5) leaves the step's hits as they are;
  send `0, 0, 0` to clear them.
- **Notes on lanes.** A step's notes keep playing as before; the device shows each note on the lane
  it strikes (GM 35..81, the others folded into their octave of 36..47: a low tom 41 on TOM, a crash 49 on
  BELL). When the device edits a lane of a step, notes on that lane become the lane's hit. Patterns
  loaded on the device into a DRUM track (SEQ > PHRASES, e.g. 12 BEAT), and DRUM tracks of projects saved
  before the grid, get their lanes' own notes as hits (the same notes and velocities play).
- **Sound loads never convert steps** (as before): switching a track to or from DRUM keeps its notes
  and hits.
- **User presets.** A user preset stored from a DRUM track whose first 16 steps strike a lane holds a
  16-step grid instead of a note pattern (record version 3; version 5 with byte values). `UP_GET` gives its low 7 bits in the 16
  (note, flags) pairs (hits, accents), then kind 1 and 16 bytes of bit 7s (bit 0 hits, bit 1 accents);
  a note pattern ends with kind 0. `UP_PUT` with kind 1 and the 16 bytes stores a grid (accents only on
  hits); without an extension, a note pattern as before. If an extension is sent, it must be
  exactly 17 bytes: kind 0 or 1 followed by 16 bytes (kind 0 ignores those bytes).
  Unknown kinds, truncated extensions and extra bytes are rejected without changing the slot.
  Firmware before v5 shows a grid record as empty.
- **Projects** in v5 used format 5 ("FUN5"): 10-byte steps and 40 reserved bytes per track.
  FUN6 retained those fields and added the song chain (v6 below); FUN7 is current (v7 below). Older formats
  are converted on load.

## v2: live sync

- `WATCH 1` starts the pushes. Watching ends by itself 3 s after the last request of any kind (send
  `PING` about every 1 s), on a USB reset, and when the host goes away; `WATCH 0` ends it at once.
- **CHANGED** (scope, id, v14): a parameter changed on the device (knob, menu, sequencer edit of a
  `P_*`), not by the editor's own `SET`. Coalesced: each (scope, id) at most every 20 ms, with the
  latest value.
- **RELOAD** (engine, preset): the engine, a preset, a user preset or a project was loaded; re-read
  `DESC` of the engine parameters, `DUMP` and the steps. It is also sent after loads the editor asked
  for with `PROJECT` load and `UP_LOAD`. After the editor's own `PRESET` and `SET` of G_ENGSEL it is not
  (firmware with INFO `53 01`, bit 1): the device takes the load as known (the engine, the preset and the
  parameters the load changed), since the editor re-reads `DUMP` after the reply; a device change still
  pending at that moment (a load or another track selected on the device, a knob turned) is pushed as before.
  Older firmware sends `RELOAD` after those too; an editor that skips its own echoes should do so only when
  bit 1 is clear.
- **WATCH while watching** (firmware with INFO `53 01`, bit 0): `WATCH 1` (or 3) while already watching on
  the same USB connection keeps what has not been pushed yet, so a change made just before is still pushed.
  Older firmware takes everything as known again (re-read `DUMP` after a re-`WATCH` there). `WATCH 0`, the 3 s
  timeout and a USB reset end watching; the next `WATCH` starts from the values as they are.
- **STEP_CHANGED** (index): a sequencer step changed on the device (record, clear, step edit,
  pattern load); not after the editor's own `STEP_SET`.
- Push frames have the normal header. Accept them at any time, also while waiting for a reply:
  match replies by cmd (23, 24 and 26 are never replies). The device sends at most a few per
  ~5 ms pass, and only when its USB send queue has room, so a push never delays a reply.

## v3: tracks

- **Track 4 since 1.0** is a synth part like tracks 1..3: `DUMP` / `RELOAD` / `TRACK` / `TRACK_DUMP`
  give its real engine byte (power-on: DRUM, preset DRUM KIT, the General MIDI map), and `PRESET`, `SET` of
  `G_ENGSEL`, `UP_LOAD` and `UP_STORE` work on it; its level is its `P_LEVEL`. The commands are byte for
  byte as before; only the meaning changed. Firmware before 1.0 had a GM drum track there: engine byte
  NENGINES, no presets (`UP_LOAD` / `UP_STORE` rc 1), its level the global `G_DRLVL`. Drums are now the
  DRUM engine on any part: the first C key is the kick (C2 = 36), notes are GM numbers.
- The globals `G_DRCH`, `G_DRLVL`, `G_DRREV` (ids 24..26: the old drum track's MIDI channel, level and
  reverb send) keep their ids, and `G_COUNT` stays 27. Id 24 (`G_DRCH`, never read since 1.0) is `G_RTYPE`
  since 1.0: the reverb's model, `DESC` label "TYPE", enum ROOM (0) / SPRING (1) / HALL (2, GHOULBOX), on
  the REVERB page (FX); projects of formats before FUN7 load it as ROOM. GHOULBOX: ids 25, 26 (inert in
  Felucca) are `G_TAPE` / `G_CRSH`, `DESC` labels "TAPE" / "CRSH", 0..127, default 0, on the TAPE page
  (FX); a project of firmware before 1.0 loads them 0 (after its drum part took its level and send from them).
  MIDI channel 10 is no longer special (channels 1..4 play tracks 1..4, channels 5..16 are ignored since
  1.0.3; with ROUT SEL every channel the selected track).
- Selecting a track with `TRACK` does not push `RELOAD` (the editor re-reads `DUMP`, the steps and the
  engine `DESC` itself); selecting one on the device does (`RELOAD` with the new track).
- Pushes are about the selected track only: `CHANGED` (scope 0) and `STEP_CHANGED` refer to it, and
  changes to other tracks (live recording from MIDI into another track, `TRACK_*` writes) push nothing.
- Level and mute are also `P_LEVEL` / `P_MUTE` of the selected track (`SET`); `TRACK_MIX` reaches the
  others. Presets and user presets change a part's sound but keep its `P_LEVEL`, `P_PAN`, `P_MUTE`.
- Projects (`PROJECT`) save and load all four tracks, the selection, the song chain and the motion (FUN7;
  FUN5 added the drum grid, FUN6 the chain). Formats 6..1 are converted; a format 1 project loads into track 1. A project saved before
  1.0 loads its drum track as track 4 with DRUM's kit (SAMPLE PERC until 1.0.2), its steps kept.
- Older firmware (no NTRK in `INFO`): one instrument; skip the track UI.

## v4: any track's parameters

- **Finding out:** send `WATCH 3`. v4 firmware answers 3; v3 (0.8) firmware answers 1, does not know
  cmds 31 / 32 (no reply) and never pushes `TRACK_CHANGED`. `WATCH 1` behaves exactly as in v2 / v3
  (reply 1, no `TRACK_CHANGED`). Match the `WATCH` reply by bit 0.
- `TRACK_PARAM` clamps like `SET` scope 0: to the range of that parameter; the engine parameters
  `P_E0..P_E7` to the ranges of that track's engine. A parameter
  with a fixed range (min = max) keeps its value. A track ≥ NTRK or an id ≥ P_COUNT gets no reply.
  For the selected track it is the same as `SET` scope 0.
- `TRACK_CHANGED` is never about the selected track (its changes stay `CHANGED` scope 0). Coalesced like
  `CHANGED` (each track and id at most every 20 ms, latest value), and not sent for the editor's own
  `TRACK_PARAM` / `TRACK_MIX` writes. After a selection change (`RELOAD`, or the editor's `TRACK`) the
  device takes the current values as known.

## Notes for the editor

- **One request at a time.** Wait for the reply, about 10–50 ms, before sending the next, from one queue
  for every request, keep-alive `PING` included. The device holds one incoming SysEx frame until the main
  loop has answered it; a frame that arrives before then is dropped whole (no reply). A frame is never
  taken in part: USB delivers every packet (acknowledged, retried), and a dropped frame leaves nothing.
  Any frame size up to the largest request (a 256-byte `BACKUP_PUT` / `SMP_WRITE` piece: 306 bytes on the
  wire, 640 bytes of buffer) is safe at full speed; pacing between bytes is not needed. With a second
  request in flight (pipelined, or a keep-alive from another thread) the second is lost: no reply, and a
  following `BACKUP_PUT` / `SMP_WRITE` piece is then refused (rc 1, the offset). Measured on an FM-1 with
  1.0 over USB: 256-byte pieces one at a time, the next sent from the reply callback, 560 of 560 taken
  (about 10 ms each); with the next piece sent before the reply, every second piece was dropped.
- **Following the device.** With v2 firmware, `WATCH` and `PING` (above). Older firmware pushes
  nothing (no reply to `PING`): poll `DUMP` about every 300–500 ms while the page is visible.
- **Port.** The device's MIDI port is named "Felucca" (USB 1209:0001). Updates use the same
  port with other SysEx (the `F0 22 24 35 …` keys, `00 59 …` frames); never send those
  from the editor. Since 1.0 the same USB device also has an audio input ("Felucca",
  44.1 kHz stereo; bcdDevice 3.11); the MIDI port and this protocol are unchanged, and both
  work while the computer records.
- **Global ids.** `G_ROUTE` (id 14, label "ROUT", GLO > SYSTEM) was a placeholder ("--", range 0..0);
  since 1.0 it is the MIDI IN routing: 0 "CH1-4" (channels 1..4 → tracks 1..4; what projects stored
  before. Until 1.0.2 every other channel played the selected track; since 1.0.3 channels 5..16 are
  ignored: notes, bend, CCs, aftertouch, panic and reset), 1 "SEL" (every channel → the selected track).
  Neither setting affects this SysEx protocol or MIDI clock. The
  id, `G_COUNT` (27) and the project format are unchanged; it is saved and loaded with the project like
  the other globals. The editor shows it under GLOBAL > SYSTEM.
- **Sound loads and undo.** A load that changes a track's sound (`PRESET`, `SET` of `G_ENGSEL`, `UP_LOAD`,
  and the same on the device) changes the sound only: the engine and the parameters of the sound. It
  never changes the steps, nor the track's own parameters: `P_LEVEL`, `P_PAN`, `P_MUTE` (0, 39, 40), the
  ARP pages, SCL and LEN / DIV / SWING / GATE (17..32), the SLICER (45..48). It does drop the track's recorded
  motion (the device's SAVE held brings it back). (Before 1.0 a preset also
  set the arp and replaced the steps with its pattern; a factory preset turned the SLICER off.) The byte
  layout of every command is unchanged. Patterns are loaded on the device (SEQ > PHRASES) or written by
  the editor with `STEP_SET` / `TRACK_STEP`.
  The device keeps one copy of the track from before the last load (any track); SAVE held 0.7 s on the
  device swaps back what the load changed (the sound; after a SEQ > PHRASES load, the steps and LEN / DIV
  / SWING / GATE). Loads in a row on one track with nothing changed between them keep the copy from before
  the first; `SET` (scope 0) within 1.5 s of a load on the selected track counts as part of that load, so
  an audition (`G_ENGSEL`, then the patch's values by `SET`, the track's own parameters skipped as
  `UP_LOAD` skips them) and the next one still undo to the state before the first. `PROJECT` load takes no
  copy.
- **Saves while playing.** A flash erase silences the audio and stalls the sequencer for a moment. The
  device refuses its own saves while the transport plays ("STOP TO SAVE"); `PROJECT` save, `UP_PUT`,
  `UP_STORE`, `UP_ERASE`, `BACKUP_LIST`, `BACKUP_PUT` and sample BEGIN / WRITE / END / ERASE from the editor stop the transport
  first (`BACKUP_GET` does not: it answers rc 3 while the transport runs). If it does not stop within 100 ms, no flash operation starts: user-preset commands
  return rc 2, sample commands a nonzero rc, backup commands rc 3. PROJECT retains its existing reply shape
  (op, slot, used); a failed PROJECT save gets no reply, allowing the editor to report
  a timeout instead of confirming the previous used slot. The previous slot is kept.
  A pending PLAY or SONG start also prevents a device save.
- **Published samples.** SMP_END repeated with an identical committed header succeeds without
  another write or zone scan. A different header is rejected (rc 2) until SMP_BEGIN,
  so sounding sample voices cannot see their zone table change.
- **Safety.** `PROJECT` save, sample-slot commands, `UP_PUT` / `UP_STORE` / `UP_ERASE`, and the tagged preference writes below write flash, and only in
  Felucca's own storage; never the app or the update area.


## v6: song chain

INFO appends CHAIN_ROWS (16) after NTRK. Earlier INFO bytes and command numbers
stay where they were. No trailing byte means no SONG command: hide its controls.

| cmd | Request args | Reply args |
| --- | --- | --- |
| 33 SONG | op 0 query; op 1 set, count 0..16, count × (slot 0..3, repeat 1..16); op 2 start; op 3 stop | op, rc, count, running 0/1, row 0..15, remaining repeats, count × (slot, repeat) |

A set must have exactly `2 + count × 2` argument bytes. rc 0 = accepted,
1 = invalid rows (or an empty chain on start), 2 = busy, 3..6 = source slot 0..3
is empty. Rejected sets keep every previous row. Set edits RAM; PROJECT save
persists it, PROJECT load recalls it. A stop/start is acknowledged before the
next audio block; query reports the actual running state. `row` and `remaining`
are meaningful only while running. Rows always start from 0, with track 1's
loop as the repeat and transition boundary; the last row stops.

A row takes all four tracks' steps (with their chance) and LEN / DIV / SWING / GATE from its saved
project slot, retaining the current sounds, mix, ARP, scales and effects. The
four sources are copied before starting, with no flash operation in playback.
Original editable patterns and timing are restored on stop. Sources are shared
project slots; overwriting a slot changes its uses on the next start.

STEP_GET / TRACK_STEP and WATCH report the playing source. During chain
playback, STEP_SET / TRACK_STEP writes, SET / TRACK_PARAM writes to
LEN / DIV / SWING / GATE and MOTION edits (rc 3) are ignored, returning the current value. Other
sound and mix parameters remain editable. Recording and panel pattern edits
also require STOP. Older editors keep working; older firmware gets no SONG
requests from the new editor.

Projects now write FUN7 (below), the same 3388 bytes in the same A/B sectors. FUN6 has the same size;
FUN5 (3352 bytes) and FUN1–FUN4 convert with an empty chain, without changing the sample-slot layout.
A chain row also plays the motion of its source project (events for an FM operator parameter are skipped
when the track's engine is not the saved one).

## v7: chance, motion, MIDI clock

- **Chance.** Each step has a chance, 0..100 %, as the last byte of the step reply (and the optional last byte
  of a step write). 100 is the default and what every older pattern holds; 0 means the step never plays. The
  device rolls once each time a step comes round, for all of it: its notes, its drum hits and a TIE. A failed
  roll plays nothing and releases what rang before. Chance is saved in projects (FUN7) and in song rows; a
  user preset does not store it. Changing it on the device pushes `STEP_CHANGED`.
- **Motion** is knob moves recorded per step: 64 events shared by the four tracks. An event is (track, step
  0..63, parameter id, value). While a track plays its motion, the step sets the value at the step and it
  holds until another step changes it; the loop restarts from the sound's own value, and stopping puts the
  sound's own values back. Parameters that can be recorded (`motion_param`): ids 0..16 (LEVEL, ENV, LFO),
  33..36 (DIST, CHO, DLY, REV), 38 (GLIDE), 39 (PAN), 44 (DETUNE), 61..80 (the FM operator parameters) and
  83..90 (the engine parameters; not the chord keys 81, 82). The device records them while the track is armed, playing and selected, from its knobs
  and from `SET` / `TRACK_PARAM` alike.
  - `MOTION` with the track alone is the query. `on` 0 keeps the data and stops playing it; 1 plays it. Clear
    drops the track's events and turns it off; on the device it is undone by SAVE held.
  - Set: `value` must be inside the parameter's range for that track's engine (and −64..127); a
    new event when the 64 are used gets rc 2 ("MOTION FULL" on the device), an event that already exists is
    updated. A set turns the track's motion on. A delete of an event that does not exist succeeds.
  - rc: 0 ok, 1 invalid (step ≥ 64, an id that cannot be recorded, a value outside its range), 2 full, 3 a song is playing.
    A track ≥ NTRK, `on` above 1 or any other length gets no reply.
  - Events come back in the order they are stored. The device does not push motion changes: poll `MOTION`
    (the web editor does, while the sequencer tab shows).
  - `DUMP`, `TRACK_DUMP`, a user preset store and a project save give the sound's own (base) values;
    `GET`, `SET` and the pushes give what is sounding now, which differs while a motion event is in effect.
  - A sound or pattern load, a pattern clear and INIT SOUND on the device drop the track's motion; the
    device's UNDO brings it back. `MOTION` clear does the same through the same undo copy.
- **Projects (FUN7).** 3388 bytes, little endian, in the same A/B sectors as before: `46 55 4E 37` ("FUN7"),
  size u32 (3388), the 27 globals as i16 (bytes 8..61), sel, parts, phys (62..64), P_COUNT as stored (byte 66),
  then from byte 68 for each of the 4 tracks: P_COUNT bytes (value + 64), engine, preset, 64 steps of 9 bytes
  (4 notes; n | time << 3 | flags << 5; vel; hit; acc; chance byte where 0 = 100 %, 1..100, 101 = never; firmware
  with `52 01`: the ratchet − 1 in bit 7 of vel (bit 0) and of the chance byte (bit 1), see "Ratchet");
  then the song chain, then the motion (260 bytes: count, on mask, 2 reserved, 64 × (track << 6 | step, id,
  i16 value)); the reserved tail is zero up to byte 3371; bytes 3372..3383 are the project's name (since
  1.0: ASCII 32..126, upper case, 0-padded; all zero = no name, as firmware before wrote them; a byte
  outside 32..126 reads as no name, it never refuses the project); the last 4 bytes are an FNV-1a hash of all
  before (the name included). The device names projects itself (SAVE > PROJECT, NAME); the editor's PROJECT
  save keeps the current name, and backups carry it as part of the 3388 bytes. FUN6..FUN1 load with no name
  and are bounded to today's ranges. A count smaller than P_COUNT is mapped as for user presets, and so are the
  motion events' ids: a FUN7 of 89 parameters has its engine parameters' events at 81..88, which load as 83..90
  (the ids below its P_E0 stay). 68 + 4 × (91 + 2 + 576) + chain + motion = 3040 bytes: 332 to spare.
- **Projects (FUN8, with FM6).** FUN7 laid out the same way, `46 55 4E 38` ("FUN8"), size 3584: the data up to
  byte 3040 as above (16 bytes to spare), the four tracks' FM6 patches as packed 128-byte records (see "FM6
  patches") at bytes 3056..3567, the name at 3568..3579, the hash last. FUN7 (3388) and older load with the
  init patch on every track. Backups, `PROJECT` and the editor's project files carry the 3584 bytes.
- **MIDI clock** has no SysEx. `G_CLOCK` selects the source: 0 INT, 1 USB, 2 TRS. With 1 or 2 the sequencer
  steps on that port's Clock pulses (the other port's are ignored), Start (0xFA) restarts from step 0,
  Continue (0xFB) resumes and Stop (0xFC) stops; BPM follows the incoming tempo (40..240), and 500 ms
  without a pulse stops the transport. Changing `G_CLOCK` stops it.
- Pitch bend, sustain (CC64), RPN 0 (bend range, ±0..24 semitones), CC120 / 121 / 123 are MIDI only and
  have no parameters, protocol or saved state.

## Ratchet

INFO advertises `52 01 4` after the MENU settings tag (`4E 01 count`): a step plays up to 4 times. Firmware without the tag has no
ratchet byte (read every step as 1).

- A NOTE step's ratchet 2..4 plays all of it (its notes and its drum hits) that many times, in equal parts of the
  step as swung, each part retriggered and held for GATE of the part. The chance is rolled once for the step (a
  failed roll: no part). A slide into it glides into the first part; it never slides or ties into the next step. 1
  (the default, what every older pattern holds) is the step as before. TIE and REST steps ignore it.
- `STEP_GET` / `STEP_SET` / `TRACK_STEP`: the byte after the chance, 1..4. The flags byte stays 1 accent, 2 slide in
  both directions; a write without the ratchet byte (an older editor) keeps the step's ratchet.
- On the device: SEQ > CHANCE, KNOB 3 (RATCH x1..x4) of the cursor step; the piano roll and the DRUM grid draw a
  ratcheted step in its parts. Changing it pushes `STEP_CHANGED`.
- Saved in projects (FUN7 / FUN8 layout unchanged: bit 7 of the velocity byte is the ratchet − 1's bit 0, bit 7 of
  the chance byte its bit 1; 0 in every older project, which load x1), so also in backups and song rows. Firmware
  before the ratchet refuses a project that holds one. A user preset's note pattern keeps it in its flags (8 | 16);
  a drum grid record has no room for it and loads x1.

## v7: full backup (65-67)

INFO advertises `42 01 caps`: bit 0 = `BACKUP_LIST` / `BACKUP_GET` (read), bit 1 = `BACKUP_PUT` (restore);
this firmware sends 3. Requests name objects, never flash addresses.

| id | object | size |
| --- | --- | --- |
| 0 | runtime: the music being played now, as a FUN8 project (firmware before FM6: FUN7, 3388) | 3584 |
| 1 | settings (palette, speaker, HOLD time, favorites, panel calibration, ...) | the settings record's size |
| 2..5 | PROJECT slots 1..4 (FUN8) | 3584, or 0 if empty |
| 6, 7 | user preset banks (slots 1..16, 17..32) | the bank's size, or 0 if empty |
| 8 | the FM6 patch bank of 1.0..1.0.2 (B1..B27). Since 1.0.3 always listed empty (see below) | 3472, or 0 if empty |
| 9 | the user presets' FM6 patches (1.0.3; `up_fm6.c`: per slot a tag and the packed patch) | 3728, or 0 if none |
| 32..34 | user sample slots 1..3: header (512 bytes) then ADPCM data | 512 + data length, or 0 if empty |

Reading: `BACKUP_LIST` (no arguments) stops the transport, then takes a snapshot of the runtime object and
answers `1, rc, count` (13 since 1.0.3, 12 with FM6 up to 1.0.2, 11 before) and, per object in the order above, `id, size u32, crc u32` (CRC-32, zlib). The other objects are read as
they are in RAM or flash. Then `BACKUP_GET` reads an object in pieces: `id, offset u32, count lo, count hi`
(count 1..256, LSB first 7 bit pair) answers `id, rc, offset u32, count lo, count hi` and the data as pack7. Check each object's CRC
against the list; if it differs the device changed, so start again.

Restoring: `BACKUP_PUT` takes ids 0..9 (firmware before 1.0.3: 0..8, id 9 answers rc 1 at begin and nothing is written) (the samples are written with `SMP_BEGIN` / `SMP_WRITE` / `SMP_END`,
or `SMP_ERASE` for an empty slot). Begin: `0, id, size u32, crc u32`: size is 3584 (FUN8) or 3388 (FUN7, FUN6) for id 0,
the settings record's size for id 1, 3584, 3388 or 0 (empty the slot) for ids 2..5, the bank's size or 0 for 6 and 7,
3472 or 0 for 8 (the FM6 bank: its magic, layout and every byte below 128 are checked), 3728 or 0 for 9 (its magic,
version and slot count checked; 0 clears the patches). FUN7 and FUN6 become FUN8.
An archive without id 8 (written before FM6) still restores; the web editor reads all three kinds (11, 12, 13 objects).
Since 1.0.3 an id 8 with data (an archive of 1.0..1.0.2) is not stored: like the first boot after the update, every FM6
user preset whose stored SLOT is a B slot (8..34) and has no patch yet gets that bank slot's patch, and id 9 is written.
So restore ids 6 and 7 before 8 (the web editor does); an empty id 8 is taken and ignored (rc 0). Old archives
therefore restore on both old and new firmware; a 1.0.3 archive on older firmware loses only id 9 (the web editor
skips it when the device answers rc 1). Data: `1, id, offset u32, pack7` with the next offset (they must follow each other)
and at most 256 decoded bytes (256-byte pieces are fine at full speed, one request at a time: see "Notes for
the editor"; a piece that gets no reply was not taken: abort and begin the object again). Commit: `2, id`: the device checks the length and the CRC, validates the
content, and then writes. Abort: `3, id`. Id 0 replaces the music now playing (RAM only, no flash);
ids 1..9 are written to flash (settings and presets are applied too). Nothing is written before the commit.
The web editor sends ids 2..7 and the samples first, then 1, then 0 last. A failed restore can leave
earlier objects restored; the file is still the source.

| rc | Meaning |
| --- | --- |
| 0 | ok |
| 1 | invalid: arguments, id, size, offset, count, or an id `BACKUP_PUT` does not take |
| 2 | validation failed: CRC, length or content at commit (and a runtime object that cannot be packed) |
| 3 | stop playback first (`LIST` and `PUT` stop it themselves, and answer 3 if it does not stop in 100 ms; `GET` answers 3 while it plays) |
| 4 | flash write failed |
| 5 | stale: no `LIST` yet, the USB bus was reset, 15 s with no `PUT` request, or a project save / load reused the staging RAM (for `GET`, of id 0 only). Start again with `LIST` / `BACKUP_PUT` begin |

`BACKUP_LIST` answers `1, rc, 0` when rc is not 0. `GET` and `PUT` answer id 127 when they got no arguments.
A `LIST` replaces the snapshot, and a `PUT` begin ends it: a `GET` after a begin gets rc 5.

## FM6 patches (68-71)

The FM6 engine (12) plays a 6-operator patch per track; its eight EDIT parameters are macros on top of it
(ALG 0 = the patch's algorithm, 1..32 another; FB, MLVL, MRAT, MEG, VMOD offsets; DTUN; SLOT). The patch itself
only travels through these commands. INFO advertises `46 01 nfactory nbank` after the backup tag (1.0.3:
`46 01 08 00`; 1.0..1.0.2: `46 01 08 1B`); firmware without it has no FM6 and does not answer 68..71.
After it, `53 01 caps` (live sync, v2 above): bit 0 = `WATCH` while watching keeps what is not pushed yet,
bit 1 = no `RELOAD` after the editor's own `PRESET` / `SET` of G_ENGSEL. This firmware sends 3.
Then (1.0.3) `50 01 caps`, FM6 v2: bit 0 = no patch bank (SLOT is F1..F8 = 0..7 and 8 = OWN; the bank target
answers rc 3), bit 1 = user presets carry their patch (target 3; backup id 9). This firmware sends 3. Firmware
without the tag has the bank and SLOT 0..34 (F1..F8, B1..B27): an editor should not offer the bank either way.

SLOT (P_E0 + 7), since 1.0.3: 0..7 load that factory patch into the track; 8 (OWN) is the track's own patch (what
a project, a user preset, a converted DIGITAL sound or an FM6_PUT to the track put there). Every such load sets SLOT
to F n when the patch is that factory patch unchanged, else OWN, and nothing reloads over it. Setting SLOT from OWN
to F n keeps the own patch aside; setting it back to OWN brings it back. A stored value 9..34 (a B slot of 1.0.2) is
clamped to 8.

A patch is the 128-byte packed record of the generic 6-operator voice (the 32-voice bank's record; every byte is
7-bit, so it travels as it is, no pack7). Operators come sixth first: per operator 17 bytes (R1..R4, L1..L4,
break point, left / right depth, curves `LC | RC << 2`, `RS | DET << 3`, `AMS | KVS << 2`, output level,
`MODE | FC << 1`, fine), then pitch EG rates and levels (102..109), algorithm 0..31 (110), `FB | OKS << 3`,
LFO speed, delay, PMD, AMD, `SYNC | WAVE << 1 | PMS << 4`, transpose (24 = none), the name (10 ASCII bytes).
The device stores every value clamped into its range.

| cmd | Request args | Reply args |
| --- | --- | --- |
| 68 FM6_GET | target, index | target, index, rc, then (rc 0) the 128 bytes |
| 69 FM6_PUT | target, index, the 128 bytes | target, index, rc |
| 70 FM6_LIST | — | nfactory, nbank, then per slot (factory first): used (0/1), name string ("" if empty); 1.0.3: nbank 0, the factory patches only |
| 71 FM6_ERASE | bank index | index, rc (1.0.3: 3, no bank) |

target:
- 0 a track's own patch (index 0..3: what it plays and what its project saves). A PUT is heard at once; SLOT becomes
  OWN (F n if it is that factory patch unchanged), and the device does not reload a factory patch over it.
- 1 the patch bank. 1.0..1.0.2: index 0..26 = B1..B27 in flash (A 0x9F000, B 0xFE000; backup id 8). Since 1.0.3:
  GET, PUT and ERASE answer rc 3 ("no bank") and change nothing; LIST reports nbank 0.
- 2 a factory patch (0..7, GET only).
- 3 a user preset's patch (1.0.3; index 0..31 = U01..U32). It is kept beside the record and counts only for an FM6
  record as it was when the patch was stored (a rename keeps it; an `UP_PUT` that changes the record, `UP_ERASE` or another store
  drops it). `UP_STORE` / SAVE of an FM6 track stores the track's patch with it; `UP_LOAD` plays it. GET: rc 2 when
  the slot has none (not an FM6 sound, or stored without one: it then loads SLOT's factory patch, or the init voice
  for OWN). PUT (after an `UP_PUT` of the record): rc 1 when the slot is not a used FM6 record; it writes flash (stops
  the transport, allow 1 s).

rc: 0 ok, 1 arguments (an unknown target, an index out of range, a record that is not 128 bytes), 2 empty (GET) or a
flash error / transport that did not stop (PUT), 3 no bank (1.0.3, target 1 and ERASE).

The web editor (6-OP FM tab) imports the generic SysEx files of the format: a single voice `F0 43 0n 00 01 1B`, the
155-byte unpacked voice, checksum, `F7` (163 bytes), and 32 voices `F0 43 0n 09 20 00`, 32 x 128 packed, checksum,
`F7` (4104 bytes); the checksum is the two's complement of the data's sum, 7 bits. Raw 155 / 4096-byte files are
read too. Pick a voice, edit it, send it to a track (target 0); to keep it, SAVE it as a user preset (or save the
project) on the device. It exports single voices. Its librarian reads and writes a user preset's patch (target 3)
with the record when the firmware has FM6 v2 bit 1, and keeps it in library files as `fm6` (128 numbers).

## Tagged device preferences v1

INFO appends `0x55, 1, uiCaps` after the existing `NTRK, CHAIN_ROWS` bytes.
Only this complete tagged extension enables commands 34–38. An untagged byte
from an experimental firmware is not a SONG or preference capability.
Command 33 remains SONG. Older editors can ignore the additional INFO bytes.

`uiCaps`: bit 0 palette, bit 1 font weight (retired: firmware since the 1.0 UI has one
weight and no longer sets it), bit 2 reserved for a MIDI monitor, bit 3 favorites. This build
advertises 9 (palette and favorites).

| cmd | Request args | Reply args |
| --- | --- | --- |
| 34 UI_STATE | none | caps, palette, font, monitor, filter, favoriteSig u28, bankSig u28 |
| 35 UI_SET | id, value | rc, id, value, UI_STATE payload |
| 36 UI_PALETTES | none | count, count × name string |
| 37 FAV_GET | engine, start v14, count 1..32 | rc; on success: engine, start v14, count, count × on/off |
| 38 FAV_SET | engine, preset v14, on/off | rc; when applied: engine, preset v14, on/off |

UI_SET ids: 0 palette (0..count-1, the order of UI_PALETTES: GREY GREEN AMBER ICE VIOLET ROSE PAPER
HI-CON NIGHT MONO; 1.0.2: GREY is the MONO of 1.0.1, MONO a new black and white palette), 1 font (retired: rc 2), 2 reserved, 3 preset filter (0 all, 1 favorites). Unsupported state
fields are 127.
Both u28 signatures are four least-significant-first 7-bit bytes; compare them
to refresh changed favorites and user slots. Factory references use stable
engine/preset ids; engine NENGINES denotes a user slot. An empty user slot
cannot be marked. Factory DRUM sounds use the same favorite path.

rc 0 = applied and saved; 1 = invalid arguments; 2 = unsupported feature;
3 = applied in RAM but not saved (no flash or a failed write);
4 = applied and queued to save after STOP. A failed settings write is retried
by the same persistence path as the panel. These changes never stop playback.
Unchanged writes do not erase flash. Replies echo preference ids/values and
favorite ranges so the editor rejects replies to a different request.

## MENU settings (72, 73; 1.0.4)

The device describes the settings of its MENU (HOME held) to the editor, which builds its settings from what it
is told: no list of settings is fixed in the editor. INFO advertises `4E 01 count` after the FM6 v2 tag
(`50 01 caps`): `count` items, MENU_DESC index 0..count−1. Firmware without the tag answers neither command.

| cmd | Request args | Reply args |
| --- | --- | --- |
| 72 MENU_DESC | index (0..count−1) | index, id, kind, value v14, min v14, max v14, name string, then for kind 0 (max−min+1) value-name strings, for kind 1 a unit string; then (1.0.5) tab, tab-name string. An index past the list: index, 127 (no more bytes) |
| 73 MENU_SET | id, value v14 | rc, id, value v14: the device's value after the write (clamped to min..max) |

- **index** is the position in the device's menu order (show them in this order); **id** names the setting and
  never changes its meaning: a new setting takes a new id (append-only), wherever it shows in the menu. Ids are
  0..126; 127 means "no item". Remember settings by id, not by index.
- **kind**: 0 an enum (the value is an index into the names, min..max), 1 a number (min..max, its unit after the
  name; no setting uses it yet). An editor shows a kind it does not know read-only, or not at all.
- Values are v14 like the rest of the protocol, so a setting may later go past 127; today every value is 0..9.
- **rc** as `UI_SET`: 0 applied and saved, 3 applied in RAM only (no flash or a failed write; retried by the
  settings path), 4 applied and saved after STOP; 1 an unknown id (the request's id and value are echoed,
  nothing changes). A value out of range is not refused: it is clamped and the reply says which value the device
  took. A wrong argument length gets no reply.
- A MENU_SET is applied exactly as the menu's KNOB 1 / OCT+ applies it (`src/menu_items.c` `menu_put`, shared by
  both), and saved through the same settings record (`settings_save`; unchanged values write nothing). If the
  device shows the MENU page, it redraws with the new value. There is no push: an editor that wants to follow
  changes made on the device reads MENU_DESC again (for instance when its settings page opens).
- CALIBRATION and ABOUT have no value and are not offered. COLOR (id 0) is the same setting as `UI_SET` id 0.
- **tab** (1.0.5): the MENU shows its settings in tabs (ALGORITHM steps between them on the device). After the
  names (kind 0) or the unit (kind 1) the reply carries the item's tab: its index (0.., the device's tab order,
  left to right) and its name (≤ 12 characters). An editor that wants the device's grouping shows the items by tab,
  in tab-index order, each tab's items in index order; the tab names come from the device, as the item names do.
  Firmware 1.0.4 ends the reply after the names: no tab (show the items as one list). An editor written for 1.0.4
  reads exactly the names and ignores what follows, so the tab changes nothing for it. Tab indexes are not ids: an
  item may move to another tab in a later firmware, and tabs may be added (appended); remember settings by id.
  A tab may hold items with no value that are not offered (this firmware: SYSTEM also holds CALIBRATION and ABOUT).
  For a kind it does not know, an editor cannot find the tab (it does not know that kind's bytes).

This firmware (count 12; tabs 0 DISPLAY, 1 CONTROL, 2 AUDIO, 3 SYSTEM):

| index | id | name | kind | values (min 0) | default | tab |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | 0 | COLOR | 0 | 0..9: GREY GREEN AMBER ICE VIOLET ROSE PAPER HI-CON NIGHT MONO (the order of `UI_PALETTES`) | GREY | 0 DISPLAY |
| 1 | 1 | STYLE | 0 | 0 FLAT, 1 LINE | FLAT | 0 DISPLAY |
| 2 | 2 | LARGE | 0 | 0 OFF, 1 ON | OFF | 0 DISPLAY |
| 3 | 3 | ANIM | 0 | 0 ON, 1 OFF | ON | 0 DISPLAY |
| 4 | 4 | LEDS | 0 | 0 OFF, 1 DIM LO, 2 DIM HI, 3 INV (darkest first, as the menu steps them) | DIM HI | 0 DISPLAY |
| 5 | 5 | HOLD | 0 | 0 "0.3 s", 1 "0.4 s", 2 "0.5 s", 3 "0.6 s" | 0.4 s | 1 CONTROL |
| 6 | 6 | KNOB ACCEL | 0 | 0 OFF, 1 ON | OFF | 1 CONTROL |
| 7 | 7 | FX LATCH | 0 | 0 OFF, 1 ON | OFF | 1 CONTROL |
| 8 | 8 | BPM LOCK | 0 | 0 OFF, 1 ON | OFF | 1 CONTROL |
| 9 | 9 | SPEAKER EQ | 0 | 0 FLAT, 1 LOWCUT, 2 BASS+ | FLAT | 2 AUDIO |
| 10 | 10 | USB LEVEL | 0 | 0 MASTER, 1 FIXED | MASTER | 2 AUDIO |
| 11 | 11 | USB SERIAL | 0 | 0 ON, 1 OFF | ON | 3 SYSTEM |

Example (bytes in hex): `F0 7D 46 4C 48 04 F7` asks for index 4; the reply
`F0 7D 46 4C 48 04 04 00 02 40 00 40 03 40 4C 45 44 53 00 4F 46 46 00 44 49 4D 20 4C 4F 00 44 49 4D 20 48 49 00 49 4E 56 00 00 44 49 53 50 4C 41 59 00 F7`
is index 4, id 4, kind 0, value 2 (`02 40`), min 0 (`00 40`), max 3 (`03 40`), "LEDS", then "OFF" "DIM LO"
"DIM HI" "INV", then tab 0 "DISPLAY" (1.0.4 firmware: the same reply without `00 44 49 53 50 4C 41 59 00`). `F0 7D 46 4C 49 04 03 40 F7` sets LEDS to INV and answers `F0 7D 46 4C 49 00 04 03 40 F7` (rc 0).
`F0 7D 46 4C 49 00 32 40 F7` (COLOR 50) answers `F0 7D 46 4C 49 00 00 09 40 F7` (clamped to 9, MONO).
`F0 7D 46 4C 48 0C F7` answers `F0 7D 46 4C 48 0C 7F F7` (no index 12).

**USB SERIAL (id 11).** The menu applies it when it closes; a MENU_SET applies it about 200 ms after its reply
(`usb_serial_apply`), so the reply leaves first (if the device shows the MENU at that moment: when it closes).
A change (ON <-> OFF; setting the value it already has does nothing) re-enumerates the whole device: it drops off
the bus (its D+ pull-up off) and comes back with the other descriptors, so USB-MIDI, USB audio and the serial
console all go away and come back. On the device the gap is about 25 ms plus one main-loop pass (`usb_retry`,
`usb_start`), or up to 1 s when it re-enumerated in the second before; the host then debounces the attach
(>= 100 ms), resets and enumerates the device. Measured on macOS (CoreMIDI): OFF -> ON, the reply at ~0.04 s, the
device gone at ~0.3 s, the MIDI port gone at ~0.57 s and back at ~1.15 s; ON -> OFF, the serial port goes
at ~0.6 s while the MIDI port was not seen to disappear at all (so do not wait for a disconnect event: after
a real change, wait ~1.5 s and open again). What the editor sends in that window is lost, `WATCH` ends with the
bus reset (send `INFO` and `WATCH` again after reconnecting), and a backup in progress answers rc 5 (stale).
