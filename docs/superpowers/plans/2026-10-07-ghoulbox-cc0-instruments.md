# GHOULBOX 1.1 Sampled Instruments Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add four CC0 sampled instruments (bowed psaltery, Renaissance organ, tenor recorder, folk harp) as SAMPLE sets and presets, plus a synthesized BAGPIPE preset, without changing what any stored 1.0 SET value plays.

**Architecture:** The new sets are appended to the sample data after the five existing (sets 5–8) but get SET values *after* the user slots (8–11): one mapping, `smp_set_at(v)`, replaces every `v < SMP_NSETS` / `v - SMP_NSETS` test. That refactor lands first, behaviour-preserving (Task 1). Then the data (Task 2), the menus (Task 3), the bagpipe (Task 4), and audition / release (Task 5).

**Tech Stack:** C (JieLi AC79 firmware, host-built tests via `tests/hostsim.c`), Python build tools (`tools/fetch_cc0.py`, `tools/gen_samples.py`), the web editor (`web/editor.html`, `web/test_web.mjs`).

**Spec:** `docs/superpowers/specs/2026-10-07-ghoulbox-cc0-instruments-design.md`

## Global Constraints

- Sources: CC0 1.0, Versilian Studios VCSL (`sgossner/VCSL`); provenance lines in `assets/samples-cc0/ATTRIBUTION.txt`.
- Stored SET / SRC values 0–7 keep their 1.0 meaning: 0 PIANO, 1 PIANO (alias), 2 FLUTE, 3 SAX, 4 PIANO (alias, once PERC), 5–7 USR1–USR3.
- New sets' SET values: 8 PSALTERY, 9 RENORGAN, 10 RECORDER, 11 FOLKHARP.
- Flash: the four sets together ≤ 130 KB (≥ 40 KB of the app slot left spare).
- Retired presets (PIANO, FLUTE, SAX) stay retired (`engines.c GB_HIDDEN`); stored numbers keep playing them.
- Goldens: only new renders added; every existing render unchanged.
- Build / test commands: `./build.sh`; `AC79_SDK=~/fw-AC79_AIoT_SDK sh tests/run_tests.sh`; host tests use `gcc -O1/-O2 -w -Ibuild/gen -Ifirmware/src` (`-O0` fails on device-only asm).
- Commits are GPG-signed (the repo's default); never `--no-gpg-sign`.

## Review Focus

1. A 1.0 project with SET = USR1 (value 5) after the update: must still play user slot 1, not a new set.
2. GRAIN with SRC on a new set: grains come from that set (GRAIN shares the mapping).
3. A sustained note held for a long time on a new looped set: no click at the loop seam.
4. The SET knob: steps over aliases, shows the new instruments first, never lands on a value outside 0–11.
5. A build without the new data (fetch not run): fails clearly rather than renumbering USR.

---

### Task 1: SET-value mapping (behaviour-preserving refactor)

**Files:**
- Modify: `firmware/src/eng_sample.c` (around `SMP_NALL`, `smp_set_missing`, `sample_note_on`)
- Modify: `firmware/src/eng_grain.c:104-138` (`gr_nz`, `gr_zone`, `gr_stamp`, `gr_find`)
- Modify: `firmware/src/ui_graph.c:475-487` (`sample_wave_zone`)
- Modify: `firmware/src/params.c:170` (`enum_orig`)
- Modify: tests using `SMP_NSETS + k` for USR: `tests/ui_test.c:3057-3097`, `tests/slice_test.c:860-861`, `tests/ui_render.c:801`
- Test: `tests/ui_test.c` (new `test_sample_values`)

**Interfaces:**
- Produces: `#define SMP_USR_V0 5u`; `#define SMP_NONE 0xFFFFFFFFu`; `static uint32_t smp_set_at(uint32_t v)` (built-in set index, or `SMP_NONE` for a user slot); `static uint32_t smp_usr_at(uint32_t v)` (user slot 0..2, only when `smp_set_at(v) == SMP_NONE`).

- [ ] **Step 1: Write the failing test** (append before `int main` in `tests/ui_test.c`, call it from `main`)

```c
/* GHOULBOX 1.1: a SET / SRC value -> what it plays. 0..4 the 1.0 sets, 5..7 USR1..3 (fixed), 8.. the sets added since */
static int test_sample_values(void)
{
    int ok = 1;
    uint32_t v;
    for (v = 0; v < SMP_NALL; v++) {
        uint32_t s = smp_set_at(v);
        if (v < 5u) ok &= s == v;
        else if (v < 8u) ok &= s == SMP_NONE && smp_usr_at(v) == v - 5u;
        else ok &= s == v - 3u && s < SMP_NSETS;
    }
    ok &= SMP_USR_V0 == 5u && SMP_NALL == SMP_NSETS + 3u && str_eq(SMP_ALL_NAMES[5], "USR1") && str_eq(SMP_ALL_NAMES[7], "USR3");
    return check("SAMPLE SET values: 0..4 the 1.0 sets, USR1..3 at 5..7, later sets after them", ok);
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `gcc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/ui_test tests/ui_test.c -lm`
Expected: compile error, `smp_set_at` / `SMP_USR_V0` undeclared.

- [ ] **Step 3: Implement the mapping** (`firmware/src/eng_sample.c`, after `#define SMP_NALL`)

```c
#define SMP_USR_V0 5u                                /* SET / SRC value of USR1: 1.0's, fixed; sets added later come after USR3 */
#define SMP_NONE 0xFFFFFFFFu
_Static_assert(SMP_NSETS >= SMP_USR_V0, "the 1.0 sets (assets/samples-cc0) are needed: USR1 stays at value 5");
/* the built-in set SET / SRC value v plays (SMP_SETS index), SMP_NONE for a user slot */
static uint32_t smp_set_at(uint32_t v)
{
    v %= SMP_NALL;
    return v < SMP_USR_V0 ? v : v < SMP_USR_V0 + SMP_USER_SLOTS ? SMP_NONE : v - SMP_USER_SLOTS;
}
static uint32_t smp_usr_at(uint32_t v) { return (v % SMP_NALL - SMP_USR_V0) % SMP_USER_SLOTS; }
```

Then replace each site:
- `smp_set_missing(si)`: `uint32_t s = smp_set_at(si); if (s != SMP_NONE) return !SMP_ZONES[SMP_SETS[s].z0].n; return !usr_nz[smp_usr_at(si)];`
- `sample_note_on`: `if ((s = smp_set_at(si)) != SMP_NONE) { const smp_set_t *set = &SMP_SETS[s]; ... } else { uint32_t k = smp_usr_at(si); ... }`
- `eng_grain.c`: `gr_nz`, `gr_zone`, `gr_stamp` test `smp_set_at(src) != SMP_NONE` and index `SMP_SETS[smp_set_at(src)]` / `usr_nz[smp_usr_at(src)]`; `gr_find`'s `zl = smp_set_at(src) != SMP_NONE ? 0 : -1`.
- `ui_graph.c sample_wave_zone`: the same pattern.
- `params.c enum_orig`: `v >= 0 && v < (int32_t)SMP_USR_V0 ? SMP_SET_ORIG[v] : v` (the aliases are among 0..4).
- Tests: `SMP_NSETS + 1` → `SMP_USR_V0 + 1`, `SMP_NSETS + 2` → `SMP_USR_V0 + 2` at each listed line.

- [ ] **Step 4: Run the suite: everything passes, nothing changed**

Run: `AC79_SDK=~/fw-AC79_AIoT_SDK sh tests/run_tests.sh`
Expected: `ALL HOST TESTS PASSED`; `regress: ... (0 changed, 0 gone)`; the new check `ok`.

- [ ] **Step 5: Commit**

```bash
git add firmware/src/eng_sample.c firmware/src/eng_grain.c firmware/src/ui_graph.c firmware/src/params.c tests/ui_test.c tests/slice_test.c tests/ui_render.c
git commit -m "SAMPLE / GRAIN: SET values through smp_set_at (USR1..3 fixed at 5..7)"
```

---

### Task 2: The four sample sets

**Files:**
- Modify: `tools/fetch_cc0.py` (`PICK`)
- Modify: `tools/gen_samples.py` (`CC0_SETS`, `SET_KEEP`, `SET_FADE`, preset values / names, `SMP_NPRESETS`, the names macros)
- Modify: `firmware/src/eng_sample.c` (`SMP_ALL_NAMES` from the new name macros)
- Create: `assets/samples-cc0/{PSALTERY,RENORGAN,RECORDER,FOLKHARP}/*.wav` (fetched), lines in `assets/samples-cc0/ATTRIBUTION.txt`
- Create: `tests/sample_sets_test.c`; add it to `tests/run_tests.sh`

**Interfaces:**
- Consumes: `smp_set_at`, `SMP_USR_V0` (Task 1).
- Produces: sets 5..8 in `SMP_SETS`, SET values 8..11 named `PSALTERY RENORGAN RECORDER FOLKHARP`; presets 5..8 `BOWED PSALT`, `REN ORGAN`, `TENOR RECORD`, `FOLK HARP` (≤ 12 chars), each with `e[0]` = its SET value; `SMP_NPRESETS` = 9; macros `SMP_SET_NAMES_V1` (sets 0..4) and `SMP_SET_NAMES_NEW` (sets 5..).

- [ ] **Step 1: Write the failing test** `tests/sample_sets_test.c`

```c
/* SPDX-License-Identifier: GPL-3.0-only */
/* GHOULBOX 1.1: the CC0 sets PSALTERY, RENORGAN, RECORDER, FOLKHARP (SET 8..11): each zone sounds, the looped ones
 * hold without a click at the seam, the harp decays to silence, the four fit their flash budget */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok) { printf("sets: %-70s %s\n", what, ok ? "ok" : "FAIL"); bad += !ok; }

/* SET value v, note n held `hold` s then released, `tail` s more: the output (mono, int32) */
static int32_t *render(uint32_t v, uint32_t n, double hold, double tail, uint32_t *len)
{
    uint32_t t, i, frames = (uint32_t)((hold + tail) * FS), off = (uint32_t)(hold * FS);
    int32_t o[2 * CTL], *x = calloc(frames + CTL, sizeof *x);
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    host_preset(&trk[0], ENGI_SAMPLE, 0);
    trk[0].p[P_E0] = (int16_t)v;
    trk[0].p[P_REV] = 0; trk[0].p[P_DLY] = 0; trk[0].p[P_CHOR] = 0;
    trk_note_on(&trk[0], n, 100);
    for (t = 0; t < frames; t += CTL) {
        if (t >= off && t < off + CTL) trk_note_off(&trk[0], n);
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) x[t + i] = o[2 * i];
    }
    *len = frames;
    return x;
}
static double rms(const int32_t *x, uint32_t a, uint32_t b)
{
    double s = 0;
    for (uint32_t i = a; i < b; i++) s += (double)x[i] * x[i];
    return sqrt(s / (b - a));
}
/* the largest sample-to-sample step over [a, b) against the typical one: a loop click is an outlier */
static double click_ratio(const int32_t *x, uint32_t a, uint32_t b)
{
    double mx = 0, s = 0;
    for (uint32_t i = a + 1; i < b; i++) {
        double d = fabs((double)x[i] - x[i - 1]);
        if (d > mx) mx = d;
        s += d * d;
    }
    return mx / sqrt(s / (b - a - 1) + 1e-9);
}

int main(void)
{
    static const struct { uint32_t v; const char *name; int looped; } SET[] = {
        {8, "PSALTERY", 1}, {9, "RENORGAN", 1}, {10, "RECORDER", 1}, {11, "FOLKHARP", 0}};
    uint32_t k, z, len, bytes = 0;
    for (k = 0; k < 4u; k++) {
        uint32_t s = smp_set_at(SET[k].v);
        const smp_set_t *set = &SMP_SETS[s % SMP_NSETS];
        int ok = s != SMP_NONE && str_eq(SMP_ALL_NAMES[SET[k].v], SET[k].name) && set->nz >= 3u;
        for (z = 0; ok && z < set->nz; z++) {               /* every zone sounds at its root */
            const smp_zone_t *zn = &SMP_ZONES[set->z0 + z];
            int32_t *x = render(SET[k].v, (uint32_t)(zn->root16 / 16), 1.0, 0.5, &len);
            ok &= rms(x, FS / 10, FS / 2) > 300.0;
            bytes += (zn->n + 1u) / 2u;
            free(x);
        }
        check(SET[k].name, ok);
        if (SET[k].looped) {                                 /* held 5 s: past several loop seams, no click */
            int32_t *x = render(SET[k].v, 62, 5.0, 0.2, &len);
            double r = click_ratio(x, FS, 5 * FS);
            printf("sets:   %s click ratio %.1f\n", SET[k].name, r);
            check("  held 5 s: no click at the loop seam (ratio < 12)", r < 12.0 && rms(x, 4 * FS, 5 * FS) > 300.0);
            free(x);
        } else {                                             /* plucked: rings, then silence */
            int32_t *x = render(SET[k].v, 62, 3.0, 0.5, &len);
            check("  plucked: decays to silence", rms(x, FS / 20, FS / 5) > 300.0 && rms(x, len - FS / 5, len) < 30.0);
            free(x);
        }
    }
    printf("sets: the four use %u B of flash\n", bytes);
    check("the four fit their budget (<= 130 KB)", bytes <= 130u * 1024u);
    check("1.0's values unchanged: PIANO 0, FLUTE 2, SAX 3, USR1 5",
          str_eq(SMP_ALL_NAMES[0], "PIANO") && str_eq(SMP_ALL_NAMES[2], "FLUTE") && str_eq(SMP_ALL_NAMES[3], "SAX") &&
          str_eq(SMP_ALL_NAMES[5], "USR1") && smp_set_at(2) == 2u);
    puts(bad ? "SAMPLE SETS TEST FAILED" : "sample sets passed");
    return bad != 0;
}
```

Notes for the implementer: `trk_note_on` / `trk_note_off` are the firmware's live-note entry points that hostsim's tests already call (`tests/hostsim.c:177-178`). `str_eq` comes from `firmware/src/libc.c` via hostsim.

- [ ] **Step 2: Run it to verify it fails**

Run: `gcc -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/sample_sets_test tests/sample_sets_test.c -lm && build/host/sample_sets_test`
Expected: `PSALTERY ... FAIL` (value 8 is past `SMP_NALL` today: no such set).

- [ ] **Step 3: Fetch the recordings** (`tools/fetch_cc0.py`, add to `PICK`)

```python
    # GHOULBOX 1.1 (VCSL, CC0)
    "PSALTERY": (VCSL, "Chordophones/Zithers/Psaltery, Bowed and Plucked/LongBow",
                 [r"_A#3_Main_LongBow", r"_E4_Main_LongBow", r"_D5_Main_LongBow"]),
    "RENORGAN": (VCSL, "Aerophones/Edge-blown Aerophones/Renaissance Organ/8'",
                 [r"_C2_rr1", r"_C3_rr1", r"_C4_rr1", r"_C5_rr1"]),
    "RECORDER": (VCSL, "Aerophones/Edge-blown Aerophones/Baroque Tenor Recorder/Sustain",
                 [r"_Sus_C3_", r"_Sus_C4_", r"_Sus_C5_"]),
    "FOLKHARP": (VCSL, "Chordophones/Composite Chordophones/Folk Harp",
                 [r"_C2_v2_RR1", r"_C3_v2_RR1", r"_C4_v2_RR1", r"_C5_v2_RR1"]),
```

Run: `python3 tools/fetch_cc0.py PSALTERY RENORGAN RECORDER FOLKHARP` (check its argument handling first: `sed -n 60,120p tools/fetch_cc0.py`; if it fetches every `PICK` set, it skips files that exist).
Expected: 14 WAVs under `assets/samples-cc0/<SET>/`. Add one provenance line per file to `ATTRIBUTION.txt` in its existing format (`SET/NN_file.wav  <-  sgossner/VCSL: path`).

- [ ] **Step 4: Generate the sets** (`tools/gen_samples.py`)

```python
CC0_SETS = [("PIANO", "oneshot"), ("PIANO", "alias"), ("FLUTE", "sus"), ("SAX", "sus"), ("PIANO", "alias"),
            # GHOULBOX 1.1: appended (data sets 5..8); their SET values follow USR1..3 (8..11: eng_sample.c smp_set_at)
            ("PSALTERY", "sus"), ("RENORGAN", "sus"), ("RECORDER", "sus"), ("FOLKHARP", "oneshot")]
USR_V0 = 5                                    # eng_sample.c SMP_USR_V0: the SET value of USR1
PRESET_NAME = {"PSALTERY": "BOWED PSALT", "RENORGAN": "REN ORGAN", "RECORDER": "TENOR RECORD", "FOLKHARP": "FOLK HARP"}
SET_KEEP.update({"PSALTERY": 0.80, "RENORGAN": 0.70, "RECORDER": 0.60, "FOLKHARP": 1.30})
SET_FADE.update({"FOLKHARP": 0.50})
```

In `header()`:
- the preset rows: `si = self.alias.get(i, i)`; `val = si if si < USR_V0 else si + 3` (3 = the user slots); the row's `e[0]` is `val`; its name `PRESET_NAME.get(name, name)`; the GHOULBOX rows' sends `FX(0, 20, 15, 95)`.
- `SMP_NPRESETS` = the number of rows (9); preset 4 (PERC's alias) stays hidden by `SMP_SET_ORIG` (`ui.c preset_orig`).
- the names: `#define SMP_SET_NAMES_V1` (sets 0..4) and `#define SMP_SET_NAMES_NEW` (sets 5..; empty when none); keep `SMP_SET_NAMES_INIT` (all, data order) for anything else that reads it.

In `firmware/src/eng_sample.c`: `static const char *const SMP_ALL_NAMES[SMP_NALL] = {SMP_SET_NAMES_V1, "USR1", "USR2", "USR3", SMP_SET_NAMES_NEW};`

Trim `SET_KEEP` (the seconds kept per zone) until the test's flash line is ≤ 130 KB.

- [ ] **Step 5: Run the test to verify it passes**

Run: `./build.sh && gcc -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/sample_sets_test tests/sample_sets_test.c -lm && build/host/sample_sets_test`
Expected: every line `ok`, `sample sets passed`, the four's bytes ≤ 133120. If a click ratio fails, lengthen that set's crossfade or move its loop start later in `cc0_entries` (per set) and re-run; check by ear in Task 5.

- [ ] **Step 6: Add the test to the suite and run everything**

In `tests/run_tests.sh`, beside the other host tests:
```sh
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/sample_sets_test" tests/sample_sets_test.c -lm
    run "SAMPLE: the GHOULBOX CC0 sets (zones, loops, decay, flash)" "$OUT/sample_sets_test"
```
Run: `AC79_SDK=~/fw-AC79_AIoT_SDK sh tests/run_tests.sh`
Expected: all pass except tests that count SAMPLE's presets or list its names (fixed in Task 3); regress shows new renders only (`GOLDEN_UPDATE=1` adds them in Task 3).

- [ ] **Step 7: Commit**

```bash
git add tools/fetch_cc0.py tools/gen_samples.py firmware/src/eng_sample.c assets/samples-cc0 tests/sample_sets_test.c tests/run_tests.sh
git commit -m "SAMPLE: bowed psaltery, Renaissance organ, tenor recorder, folk harp (CC0, SET 8..11)"
```

---

### Task 3: Menus, editor mirror, goldens

**Files:**
- Modify: `firmware/src/engines.c` (`ENGINE_ORDER`: SAMPLE back), `firmware/src/core.h` (`NENG_SHOWN`: `- 2u`)
- Modify: `firmware/src/params.c` (`enum_order`: the SET / SRC display order)
- Modify: `web/editor.html` (SAMPLE / GRAIN SET and SRC names, SAMPLE presets, `ENGINE_ORDER`, `ENGINE_HIDDEN`), `web/test_web.mjs`
- Modify: tests counting engines or SAMPLE presets: `tests/ui_test.c` (`test_hidden_presets`, the ORDER list, #124), `tests/golden.txt`

- [ ] **Step 1: Write the failing tests** (in `tests/ui_test.c test_hidden_presets`, replacing the SAMPLE-hidden expectations)

```c
    ok = NENG_SHOWN == 12u + FELUCCA_FM4;
    for (i = 0, k = 0; i < NENG_SHOWN; i++) k |= eng_vis(i) == ENGI_SAMPLE;
    bad += check("1.1: SAMPLE offered again (its CC0 presets), DRUM and SLICE not", ok && k);
    select_engine(ENGI_SAMPLE);
    bad += check("1.1: picking SAMPLE loads BOWED PSALT (its first kept preset)",
                 str_eq(ENGINES[ENGI_SAMPLE]->presets[TSEL->preset].name, "BOWED PSALT") && TSEL->p[P_E0] == 8);
    {
        const param_desc_t *sd = &ENGINES[ENGI_SAMPLE]->edit[0];
        int32_t v = 8, seen[6], n;
        for (n = 0; n < 6; n++) { v = param_turn(sd, v, 1); seen[n] = v; }   /* (params.c: one detent, in the shown order) */
        bad += check("1.1: the SET knob from PSALTERY: RENORGAN RECORDER FOLKHARP USR1 USR2 USR3",
                     seen[0] == 9 && seen[1] == 10 && seen[2] == 11 && seen[3] == 5 && seen[4] == 6 && seen[5] == 7);
    }
```

`param_turn(d, v, steps)` (`firmware/src/params.c:210`) is the knob's step through `enum_order`. Update the 50-presets count to 54 (`50u + 4u`), the ORDER list (`"ANALOG", "FM6", "PHASE", "LOFI", "SAMPLE", "VOICE", ...`), and #124's SAMPLE block (KNOB 2 now steps the four, never the alias).

- [ ] **Step 2: Run to verify they fail**

Run: `gcc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/ui_test tests/ui_test.c -lm && build/host/ui_test | grep -E "1.1:|FAIL"`
Expected: the `1.1:` checks FAIL.

- [ ] **Step 3: Implement**

- `engines.c ENGINE_ORDER`: `2, 3, 4, 5, 6, 7, 8, 9,  /* PHASE LOFI SAMPLE VOICE TRIO WHEEL GRAIN PHYS */`, comment: SLICE and DRUM not offered.
- `core.h`: `#define NENG_SHOWN (NENGINES - !FELUCCA_FM4 - 2u)` with the comment updated (not DRUM, SLICE).
- `params.c enum_order`: for `d->names == SMP_ALL_NAMES` return `SMP_SET_ORDER`, generated by `tools/gen_samples.py` as the SET values new sets first, then USR1..3, then the 1.0 sets: `{8, 9, 10, 11, 5, 6, 7, 0, 2, 3, 1, 4}` (every value once: `enum_rank` needs them all; the aliases 1 and 4 last).
- `params.c param_turn`: its ordered branch today lands on any value in the order, aliases included. Skip them: step the rank by `steps`' sign until `enum_orig(d, value) == value` (stop at the ends, as now). Add to Step 1's test: from SAX (3) one step on lands on SAX again (the aliases after it are skipped, the end holds) and never on 1 or 4:

```c
        bad += check("1.1: the SET knob never stops on an alias (1, 4)", param_turn(sd, 3, 1) == 3 && param_turn(sd, 3, 2) == 3);
```
- `web/editor.html`: SAMPLE and GRAIN `E("SET"/"SRC", [...])` in value order `["PIANO","PIANO","FLUTE","SAX","PIANO","USR1","USR2","USR3","PSALTERY","RENORGAN","RECORDER","FOLKHARP"]`; the four SAMPLE presets `P("BOWED PSALT", [8, 0, 0, 1, 127, 0, 0, 0], [a, d, s, r], 0, 0)` etc. with the generated envelopes (read `build/gen/felucca_samples.h`); `ENGINE_ORDER` with `"SAMPLE"` after `"LOFI"`; `ENGINE_HIDDEN = ["SLICE", "DRUM"]`. Update `web/test_web.mjs`'s engine list (`"ANALOG,FM6,PHASE,LOFI,SAMPLE,VOICE,..."`, indices) and the retired-engines check.

- [ ] **Step 4: Run, update goldens for the new presets only**

Run: `AC79_SDK=~/fw-AC79_AIoT_SDK sh tests/run_tests.sh` → the only regress notice is `N renders not in tests/golden.txt`; then `GOLDEN_UPDATE=1 build/host/regress tests/golden.txt tests/cpu_baseline.txt` and `git diff tests/golden.txt` shows **only added lines**.
Expected after: `ALL HOST TESTS PASSED`, `0 lint findings`, `0 off by 1 px`.

- [ ] **Step 5: Commit**

```bash
git add firmware/src/engines.c firmware/src/core.h firmware/src/params.c web/editor.html web/test_web.mjs tests/ui_test.c tests/golden.txt
git commit -m "SAMPLE back in the menus with its CC0 presets; SET knob shows the new sets first"
```

---

### Task 4: BAGPIPE preset

**Files:**
- Modify: `tools/ghoulbox_presets.py` (a GURDY preset), regenerated `firmware/src/eng_gurdy.c` and `web/editor.html` blocks
- Modify: `tests/ui_test.c` (the preset counts: 54 → 55)

- [ ] **Step 1: Write the failing test** (in `test_hidden_presets`)

```c
    for (k = 0; k < ENGINES[ENGI_GURDY]->npresets && !str_eq(ENGINES[ENGI_GURDY]->presets[k].name, "BAGPIPE"); k++)
        ;
    bad += check("1.1: GURDY has BAGPIPE, browsable", k < ENGINES[ENGI_GURDY]->npresets && !preset_hidden(ENGINES[ENGI_GURDY], k));
```

- [ ] **Step 2: Verify it fails** (`build/host/ui_test | grep BAGPIPE` → FAIL)

- [ ] **Step 3: Add the preset** in `tools/ghoulbox_presets.py`'s `"GURDY"` list, after `VIELLE`, starting from `HURDY GURDY`'s values with the buzz off and the drone up (a chanter over a constant drone), then tune by ear in Task 5:

```python
        ("BAGPIPE", [2, 0, 90, 110, 0, 0, 100, 60], [4, 64, 127, 30], 0, 1, (12, 0, 0, 80), 15),
```

Run `python3 tools/ghoulbox_presets.py` (regenerates the engine's and the editor's preset blocks), then the test → `ok`; update the preset count expectation to 55.

- [ ] **Step 4: Commit**

```bash
git add tools/ghoulbox_presets.py firmware/src/eng_gurdy.c web/editor.html tests/ui_test.c
git commit -m "GURDY: BAGPIPE, a chanter over its drone"
```

---

### Task 5: Audition, version, release

**Files:**
- Modify: `tests/dungeon_demo.c` (GALLERY: the five), `firmware/src/core.h` (`GHOULBOX_VERSION "1.1"`), `README.md`, `ROADMAP.md`

- [ ] **Step 1: Render the auditions**

Add `{ENGI_SAMPLE, "BOWED PSALT", K_CHORDS}, {ENGI_SAMPLE, "REN ORGAN", K_CHORDS}, {ENGI_SAMPLE, "TENOR RECORD", K_MELODY}, {ENGI_SAMPLE, "FOLK HARP", K_ARP}, {ENGI_GURDY, "BAGPIPE", K_MELODY}` to `GALLERY`; build and run `build/host/dungeon_demo build/dungeon_demo`; join the five WAVs into one file for the owner.
Expected: five audible files. **Stop for the owner's approval of the sounds.** Rework trims, loops, envelopes or the bagpipe's values on their notes; re-run Tasks 2–4 tests after each change.

- [ ] **Step 2: Version and docs** — `GHOULBOX_VERSION "1.1"`; README: the four sampled instruments and BAGPIPE in the feature list, counts (55 sounds, 12 engines); ROADMAP: move the 1.1 items to done.

- [ ] **Step 3: Full verification**

Run: `./build.sh && AC79_SDK=~/fw-AC79_AIoT_SDK sh tests/run_tests.sh`
Expected: build ok, image within the slot; `ALL HOST TESTS PASSED`.

- [ ] **Step 4: Commit, install on the device** (`python3 tools/ghoulbox_serve.py`): the owner checks ABOUT 1.1, the four instruments, a 1.0 project with a user sample (USR1) still playing it.

- [ ] **Step 5: Release** (after the owner's go): push `ghoulbox`; rebuild the Pages site (`python3 web/make_site.py build/felucca.fwsc gb1.1 <dir>`, commit on `gh-pages`, push); signed tag `ghoulbox-v1.1` (check `git ls-remote --tags origin` first); `gh release create ghoulbox-v1.1` with notes and `ghoulbox-1.1.fwsc`; verify the hosted package hash equals the build's.
