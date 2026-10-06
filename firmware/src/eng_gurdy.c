/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GHOULBOX (on Felucca by Leo Kuroshita, Hügelton Instruments) */
/* GURDY (GHOULBOX): a hurdy-gurdy. One wheel bows every string at once, so it is monophonic (poly 1) and its
 * drones sound under every note, whatever key is played:
 *   the chanterelle  the melody: a band-limited saw (a bowed string's Helmholtz motion), darker with less
 *                    wheel pressure (WHL), with the rosin's friction noise
 *   the bourdon      a drone on DRN (C2 .. B2), DLVL loud
 *   the mouche       a drone a fifth above it, 5TH loud
 *   the trompette    the bourdon an octave up on the buzzing bridge: its "dog" rattles on the soundboard.
 *                    BUZZ: how hard; COUP: the wrist's strokes that kick it, in time (HOLD: always, or
 *                    1/4 1/8 1/8T 1/16 of the song tempo from the note-on: each stroke a burst that fades)
 *   the body         two wooden resonances (~270 Hz box, ~1.1 kHz nose), BODY of them against the dry strings
 * WOBL: the wheel's unevenness, a slow random wander of pitch and loudness.
 * State: s[0..1] the box's SVF, s[2..3] the nose's, s[4] noise, s[5] the wander, s[6] samples since the
 * note-on (the strokes), s[7] the pressure low-pass. ph[0] chanterelle, ph[1] bourdon (x2: the trompette),
 * ph[2] mouche. */
static const char *const N_GURDY_DRN[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char *const N_GURDY_COUP[] = {"HOLD", "1/4", "1/8", "1/8T", "1/16"};
static const uint8_t GURDY_COUP_DIV[5] = {0, 0, 1, 4, 2};   /* N_DIV indices (fx.c div_samples) */
static uint32_t div_samples(uint32_t div);              /* fx.c */

static void gurdy_note_on(track_t *t, voice_t *v)
{
    (void)t;
    v->s[6] = 0;                                        /* the strokes start with the note */
    if (!v->s[4])
        v->s[4] = 0x5EED1234 + (int32_t)v->age;
}

/* the SVF's band-pass (tsvf_lp's v1): the same state update, the other output */
static inline int32_t gurdy_bp(const tsvf_t *c, int32_t in, int32_t *ic1, int32_t *ic2)
{
    int32_t v3 = in - *ic2;
    int32_t v1 = (c->a1 * *ic1 + c->a2 * v3) >> 13;
    int32_t v2 = *ic2 + ((c->a2 * *ic1 + c->a3 * v3) >> 13);
    *ic1 = clamp(2 * v1 - *ic1, -150000, 150000);
    *ic2 = clamp(2 * v2 - *ic2, -150000, 150000);
    return v1;
}

static void gurdy_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    int32_t drn = 36 + clamp(p[P_E0], 0, 11), dlvl = p[P_E1] * 200, flvl = p[P_E2] * 170, whl = p[P_E3];
    int32_t buzz = p[P_E4] * 220, body = p[P_E6] * 258, wobl = p[P_E7];
    uint32_t coup = (uint32_t)clamp(p[P_E5], 0, 4), i;
    uint32_t inc = m->inc, dinc = cents_inc(drn * 16, 0, m->fine), finc = cents_inc((drn + 7) * 16, 0, m->fine);
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1], ph2 = v->ph[2];
    int32_t b1 = v->s[0], b2 = v->s[1], n1 = v->s[2], n2 = v->s[3], nst = v->s[4], wob = v->s[5], cnt = v->s[6];
    int32_t lp = v->s[7], kp = 6000 + whl * 200, fric = whl * 24, e0, e1, de, e, w;
    tsvf_t box, nose;
    /* the wheel's wander: a leaky random walk, once per block; pitch +-~12 ct and loudness +-~8 % at WOBL 127 */
    wob += (int32_t)(noise32(&nst) >> 22) - 512;
    wob -= wob >> 6;
    w = (wob * wobl) >> 10;                             /* +-~32000 at the extremes */
    inc += (uint32_t)((int32_t)(inc >> 12) * (w >> 10));
    /* the trompette's strokes: its envelope at this block's start and end, linear between */
    if (!buzz)
        e0 = e1 = 0;
    else if (!coup)
        e0 = e1 = 32767;
    else {
        uint32_t per = div_samples(GURDY_COUP_DIV[coup]), a = (uint32_t)cnt % per, fall = per * 2u / 5u + 1u;
        uint32_t kf = (32767u << 8) / fall;             /* each stroke falls over 2/5 of its division */
        e0 = a < fall ? 32767 - (int32_t)((a * kf) >> 8) : 0;
        a += n;
        e1 = a < fall ? 32767 - (int32_t)((a * kf) >> 8) : a >= per ? 32767 : 0;
    }
    de = (e1 - e0) / (int32_t)n;
    e = e0;
    tsvf_coef(&box, 44 << 8, 70);                      /* ~270 Hz (CUTOFF_HZ: 30 Hz .. 16 kHz over 0..127) */
    tsvf_coef(&nose, 73 << 8, 90);                     /* ~1.1 kHz */
    for (i = 0; i < n; i++) {
        int32_t s, x, r, tr, nz = (int32_t)(noise32(&nst) >> 17) - 16384;
        s = osc_saw(ph0, inc);                          /* the chanterelle, through the pressure low-pass */
        lp += mulq15(s - lp, kp);
        x = lp + mulq15(nz, fric);                      /* .. and the rosin */
        x += mulq15(osc_saw(ph1, dinc), dlvl) + mulq15(osc_saw(ph2, finc), flvl);   /* the drones */
        if (e > 0) {                                    /* the trompette: the dog chatters at each period's start */
            tr = osc_saw(ph1 << 1, dinc << 1);
            tr = clamp(tr * 3, -24000, 24000) + ((ph1 << 1) < 0x30000000u ? nz : 0);
            x += mulq15(mulq15(tr, e), buzz);
        }
        e += de;
        ph0 += inc;
        ph1 += dinc;
        ph2 += finc;
        x >>= 1;                                        /* (four sources: the filters' range) */
        r = gurdy_bp(&box, x, &b1, &b2) + (gurdy_bp(&nose, x, &n1, &n2) >> 1);
        x += mulq15(r - x, body);
        x = soft_knee(x, 20000);
        out[i] += voice_amp(x, m, i) << 1;
    }
    (void)cnt;
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->ph[2] = ph2;
    v->s[0] = b1;
    v->s[1] = b2;
    v->s[2] = n1;
    v->s[3] = n2;
    v->s[4] = nst;
    v->s[5] = wob;
    v->s[6] = cnt + (int32_t)n;
    v->s[7] = lp;
}

static const preset_t GURDY_PRESETS[] = {
    /* name, {DRN, DLVL, 5TH, WHL, BUZZ, COUP, BODY, WOBL}, {A D S R}, fenv, mono */
    /* GHOULBOX presets (tools/ghoulbox_presets.py) */
    {"HURDY GURDY", {2, 90, 50, 80, 70, 2, 70, 40}, {8, 64, 127, 40}, 0, 1, FX(5, 10, 0, 90), PAT(4)},
    {"DRONE WHEEL", {2, 110, 80, 60, 0, 0, 90, 60}, {40, 64, 127, 90}, 0, 1, FX(0, 20, 0, 105), PAT(5)},
    {"DANCE GURDY", {2, 80, 40, 100, 110, 3, 60, 25}, {4, 64, 127, 30}, 0, 1, FX(10, 0, 10, 75), PAT(3)},
    {"TROMPETTE", {2, 70, 30, 90, 95, 1, 65, 35}, {6, 64, 127, 50}, 0, 1, FX(8, 0, 0, 90), PAT(4)},
    {"VIELLE", {2, 0, 0, 85, 0, 0, 80, 30}, {20, 64, 120, 60}, 0, 1, FX(0, 15, 10, 95), PAT(4)},
    /* GHOULBOX end */
};

static const engine_t ENG_GURDY = {
    .name = "GURDY",
    .page_title = {"DRONE", "WHEEL"},
    .edit = {
        {"DRN", F_ENUM, 0, 11, 2, N_GURDY_DRN, 0},
        {"DLVL", F_PCT, 0, 127, 90, 0, 0},
        {"5TH", F_PCT, 0, 127, 50, 0, 0},
        {"WHL", F_PCT, 0, 127, 80, 0, 0},
        {"BUZZ", F_PCT, 0, 127, 60, 0, 0},
        {"COUP", F_ENUM, 0, 4, 2, N_GURDY_COUP, 0},
        {"BODY", F_PCT, 0, 127, 70, 0, 0},
        {"WOBL", F_PCT, 0, 127, 40, 0, 0},
    },
    .presets = GURDY_PRESETS,
    .npresets = NELEM(GURDY_PRESETS),
    .note_on = gurdy_note_on,
    .render = gurdy_render,
    .knob = {P_E4, P_E3, P_E1, P_REL},
    .poly = 1,                                          /* one wheel: one melody note */
    .keep = 0xAF,                                       /* the body, the wander and the pressure carry on */
};
