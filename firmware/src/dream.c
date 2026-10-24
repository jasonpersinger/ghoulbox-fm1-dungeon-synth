/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GHOULBOX (on Felucca by Leo Kuroshita, Hügelton Instruments) */
/* DREAM (GHOULBOX 2.3, HOME > DREAM after KEYS): ambient music from the four tracks' own sounds.
 * A key on the page composes a piece in its key and MOOD's scale (after a dialog when there are steps to lose, until
 * the first): four chords, one per 16 steps of 64 at 1/8 (DR_PROG), and each track a part that suits its sound
 * (dr_role, by the preset's name, else the engine): a drone holds the root, a bass the chords' roots, a pad their
 * triads (voice-led), a plucked sound breaks them, a melodic one sings a motif and, two chords on, the same rhythm on
 * the new chord, a second melodic one a few high notes, drums a slow ritual. DENS: how much each part plays; the steps'
 * CHANCE varies them on every pass. EVOL ON: every 9 - DRFT passes of track 1 one thing changes (dream_evolve: a chord,
 * a phrase, a part played again, a part in or out). What DREAM writes is ordinary steps: edit them, save them.
 * Also sets each track's LEN 64, DIV 1/8, ROOT and SCL (the keys play along in key), CHRD and ARP off (they would stack
 * on the parts), a GURDY's drone (DRN) on the root. */

enum { DR_DRONE, DR_BASS, DR_PAD, DR_ARP, DR_MELODY, DR_HIGH, DR_PERC };
#define DR_LEN 64u
#define DR_SPAN 16u
static const uint8_t DR_MOOD_SCL[6] = {2, 3, 8, 7, 1, 4};   /* MOOD (params.c N_DMOOD) -> SCL: MIN DOR PHRY HARM MAJ MIX */
/* the progressions, in scale degrees (0: the root's chord), a chord per 16 steps */
static const uint8_t DR_PROG[8][4] = {{0, 5, 6, 0}, {0, 3, 0, 4}, {0, 6, 5, 4}, {0, 5, 3, 4},
                                      {0, 2, 5, 6}, {0, 3, 6, 0}, {0, 5, 0, 6}, {0, 3, 5, 4}};

static struct {
    uint8_t valid;                                          /* composed (or rebuilt from the tracks) this session */
    uint8_t asked;                                          /* the dialog said yes once: later keys dream at once */
    uint8_t root, scl, deg[4], role[NTRK], dup[NTRK];       /* dup: the second track of its role */
    uint8_t passes, last, restart;
} dream;

/* the piece's key, chords and parts are no longer what the tracks hold (GHOULBOX 2.3.1): a project load (project.c
 * project_restore_runtime), a sound or pattern load (ui.c load_end). EVOL's next change finds them back (dr_rebuild) */
static void dream_forget(void) { dream.valid = 0; seq_heard_forget(); }   /* (2.4: FREEZE too: new steps) */

static uint32_t dr_rnd(uint32_t n) { return n ? rng() % n : 0u; }

static int dr_has(const char *s, const char *w)            /* the word w (up to a space) in s */
{
    uint32_t i, j;
    for (i = 0; s[i]; i++) {
        for (j = 0; w[j] > ' ' && s[i + j] == w[j]; j++)
            ;
        if (w[j] <= ' ')
            return 1;
    }
    return 0;
}

/* the part a track's sound suits: by words in its factory preset's name (each word after its DR_ role; the first that
 * is there: "STEP LEAD" a melody, not EP's), else by its engine (and a user preset by its engine) */
static uint32_t dr_role(const track_t *t)
{
    static const char KW[] = "0DRONE 1BASS 1TUBA 6TOMS 6DRUM 4LEAD 4FLUTE 2PAD 2STR 2CHOIR 2ORGAN 2BOWED "
                             "3ARP 3LUTE 3VIRGINAL 3BELL 3PLUCK 3MARIMBA 3PIANO 3KEY 3EP";
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    uint32_t ei = t->eng_req % NENGINES;
    const char *w;
    if (ei == ENGI_DRUM)
        return DR_PERC;
    if (!t->user && e->npresets)
        for (w = KW; *w; w++)
            if (w == KW || w[-1] == ' ')
                if (dr_has(e->presets[t->preset % e->npresets].name, w + 1))
                    return (uint32_t)(*w - '0');
    if (e == &ENG_FORMANT || e == &ENG_WHEEL || e == &ENG_GRAIN)
        return DR_PAD;
    if (e == &ENG_NOISE)
        return DR_DRONE;
    if (e == &ENG_PHYS)
        return DR_ARP;
    return t->user && t->p[P_ATK] >= 50 ? DR_PAD : DR_MELODY;
}

/* the scale's tones above the root (semitones), how many */
static uint32_t dr_tones(uint8_t *pc)
{
    uint32_t m = SCALE_MASK[dream.scl % (sizeof SCALE_MASK / sizeof SCALE_MASK[0])], i, n = 0;
    for (i = 0; i < 12u; i++)
        if ((m >> i) & 1u)
            pc[n++] = (uint8_t)i;
    return n;
}

/* scale degree d (0: the root; any, below too) as a note, its octave from C of octave `oct` (4: C4 60) */
static uint32_t dr_note(int32_t d, int32_t oct)
{
    uint8_t pc[12];
    int32_t n = (int32_t)dr_tones(pc), o = d >= 0 ? d / n : -((n - 1 - d) / n);
    return (uint32_t)clamp(12 * (oct + 1 + o) + dream.root + pc[d - o * n], 12, 115);
}

/* the degree of note x (rounded down onto the scale) */
static int32_t dr_degree(uint32_t x)
{
    uint8_t pc[12];
    int32_t n = (int32_t)dr_tones(pc), r = ((int32_t)x - dream.root) % 12, o = ((int32_t)x - dream.root) / 12, d = n - 1;
    while (d > 0 && pc[d] > r)
        d--;
    return o * n + d;
}

static void dr_put(track_t *t, uint32_t i, const uint8_t *notes, uint32_t n, uint32_t vel, uint32_t chance)
{
    step_t *s = &t->step[i % DR_LEN];
    uint32_t k;
    step_clear(s);
    for (k = 0; k < n && k < 4u; k++)
        s->note[k] = notes[k];
    s->n = (uint8_t)n;
    s->time = ST_NOTE;
    s->vel = (uint8_t)vel;
    step_set_chance(s, chance);
}
static void dr_tie(track_t *t, uint32_t a, uint32_t b)     /* steps a .. b - 1 tie into the note before */
{
    for (; a < b && a < DR_LEN; a++) {
        step_clear(&t->step[a]);
        t->step[a].time = ST_TIE;
    }
}
static uint32_t dr_vel(void) { return 78u + dr_rnd(24); }

/* ------------------------------------------------------------------ the parts, a chord (a span of 16 steps) at once */
static void dr_drone(track_t *t, uint32_t s, uint32_t dens)
{
    uint8_t n[2] = {(uint8_t)dr_note(0, 3), (uint8_t)dr_note(4, 3)};
    if (s)
        return;                                            /* one note for the whole piece (span 0 writes it all) */
    dr_put(t, 0, n, t->eng_req % NENGINES == ENGI_GURDY || dens < 5u ? 1u : 2u, 90, 100);
    dr_tie(t, 1, DR_LEN);
}

static void dr_bass(track_t *t, uint32_t s, uint32_t dens)
{
    uint32_t a = s * DR_SPAN;
    uint8_t n = (uint8_t)dr_note(dream.deg[s], 2);
    dr_put(t, a, &n, 1, dr_vel(), 100);
    dr_tie(t, a + 1u, a + 12u);
    if (dens >= 6u) {                                      /* busier: the fifth before the next chord */
        n = (uint8_t)dr_note(dream.deg[s] + 4, 2);
        dr_put(t, a + 12u, &n, 1, dr_vel(), 70);
        dr_tie(t, a + 13u, a + 15u);
    }
}

static void dr_pad(track_t *t, uint32_t s, uint32_t dens, uint32_t dup)
{
    uint32_t a = s * DR_SPAN + (dup ? 4u : 0u), k;
    int32_t lo = dup ? 67 : 55;                            /* each tone in the octave from lo: a voicing that moves */
    uint8_t c[3];                                          /* least (the tones two chords share stay) */
    for (k = 0; k < 3u; k++) {
        int32_t x = (int32_t)dr_note(dream.deg[s] + 2 * (int32_t)k, 3);
        while (x < lo) x += 12;
        while (x >= lo + 12) x -= 12;
        c[k] = (uint8_t)x;
    }
    if (dup && s == 0u)                                    /* (the late entrance: the piece's last chord holds over) */
        dr_tie(t, 0, 4);
    dr_put(t, a, c, 3, dr_vel() - (dup ? 24u : 16u), 100);   /* (held triads: under the rest, as the scenes' pads) */
    dr_tie(t, a + 1u, s * DR_SPAN + DR_SPAN + (dup && s < 3u ? 4u : 0u));
    if (dens >= 6u && !dup) {                              /* busier: the chord again halfway, half the time */
        dr_put(t, a + 8u, c, 3, dr_vel() - 26u, 50);
        dr_tie(t, a + 9u, a + DR_SPAN);
    }
}

static void dr_arp(track_t *t, uint32_t s, uint32_t dens, uint32_t dup)
{
    uint32_t a = s * DR_SPAN, j, step = dens >= 6u ? 1u : 2u, shape = dr_rnd(3), ch;
    for (j = 0; j * step < DR_SPAN; j++) {
        uint32_t k = shape == 0u ? j % 6u : shape == 1u ? (j % 10u < 6u ? j % 10u : 10u - j % 10u) : dr_rnd(6);
        uint8_t n = (uint8_t)dr_note(dream.deg[s] + 2 * (int32_t)(k % 3u), (dup ? 3 : 4) + (int32_t)(k / 3u));   /* (a second: lower) */
        ch = j ? 25u + dens * 9u : 100u;
        dr_put(t, a + j * step, &n, 1, dr_vel() - 8u, ch > 100u ? 100u : ch);
    }
}

/* the melody of span s: the motif's rhythm (spans 0 and 2) or a new one, on the span's chord */
static uint8_t dr_motif[8], dr_mlen;                       /* the motif's onsets (steps into its span), how many */
static void dr_melody(track_t *t, uint32_t s, uint32_t dens, int new_motif)
{
    uint32_t a = s * DR_SPAN, j, n, at[8];
    uint8_t pc[12];
    int32_t d, n7 = (int32_t)dr_tones(pc), lo = dr_degree(62), cd = dream.deg[s], k;   /* (n7: the scale's tones, any) */
    if (s == 0u || s == 2u) {
        if (new_motif || !dr_mlen) {                       /* 2..4 notes, the first on a strong step */
            uint32_t want = 2u + dens / 3u, p = dr_rnd(2) * 2u;
            for (dr_mlen = 0; dr_mlen < want && p < 13u; p += 2u + 2u * dr_rnd(dens >= 5u ? 2u : 3u))
                dr_motif[dr_mlen++] = (uint8_t)p;
        }
        for (n = 0; n < dr_mlen; n++)
            at[n] = dr_motif[n];
    } else {                                               /* the answer: sparser, later */
        uint32_t want = 1u + dens / 3u, p = 2u + dr_rnd(3) * 2u;
        for (n = 0; n < want && p < 13u; p += 3u + 2u * dr_rnd(3))
            at[n++] = p;
    }
    d = cd + 2 * (int32_t)dr_rnd(3);                       /* a chord tone to begin */
    d = lo + ((d - lo) % n7 + n7) % n7;                    /* into the octave from 62 (a fold, never a loop) */
    for (j = 0; j < n; j++) {
        uint32_t end = j + 1u < n ? at[j + 1u] : (s == 3u ? 15u : 13u);
        uint8_t x;
        k = ((d - cd) % n7 + n7) % n7;                     /* (the degree above the chord's root) */
        if (j == n - 1u && s == 3u)                         /* the piece ends on the root's chord tone */
            d -= k;
        else if (j == n - 1u)                              /* the phrase rests on a chord tone (an even degree) */
            d += k == n7 - 1 ? 1 : k & 1 ? -1 : 0;
        x = (uint8_t)dr_note(d, -1);                       /* (degrees from the root of octave -1, as dr_degree) */
        dr_put(t, a + at[j], &x, 1, dr_vel(), j && dens < 4u && dr_rnd(3) == 0u ? 75u : 100u);
        dr_tie(t, a + at[j] + 1u, a + (end > at[j] + 1u ? end - (dr_rnd(3) == 0u) : at[j] + 1u));
        d += dr_rnd(5) == 0u ? (int32_t)dr_rnd(5) - 2 : dr_rnd(3) ? -1 : 1;   /* mostly down a step: a lament */
        if (d < lo) d += 2;
        if (d > lo + n7 + 2) d -= 2;
    }
}

static void dr_high(track_t *t, uint32_t s, uint32_t dens)
{
    uint32_t a = s * DR_SPAN, k = (dens + 2u) / 4u, j, p = dr_rnd(4) * 2u;
    for (j = 0; j < k && p < 14u; j++, p += 6u + dr_rnd(3) * 2u) {
        uint8_t x = (uint8_t)dr_note(dream.deg[s] + 2 * (int32_t)dr_rnd(3), 5);
        dr_put(t, a + p, &x, 1, dr_vel() - 15u, 50u + dr_rnd(30));
        dr_tie(t, a + p + 1u, a + p + 5u + dr_rnd(4));
    }
}

static void dr_perc(track_t *t, uint32_t s, uint32_t dens)
{
    uint32_t a = s * DR_SPAN, j, n = dens / 2u;
    if (t->eng_req % NENGINES == ENGI_DRUM) {
        grid_hit(t, a, s ? DV_TOM : DV_KICK, 1);
        step_set_chance(&t->step[a], s && s != 2u ? 70u : 100u);
        t->step[a].vel = (uint8_t)dr_vel();
        if (!s && dens >= 4u)
            grid_hit(t, a, DV_BELL, 1);
        for (j = 0; j < n; j++) {
            uint32_t i = a + 4u + 2u * dr_rnd(6);
            grid_hit(t, i, dr_rnd(4) ? DV_TOM : DV_RIM, 1);
            t->step[i].vel = (uint8_t)(dr_vel() - 20u);
            step_set_chance(&t->step[i], 40u + dens * 5u);
        }
        return;
    }
    {                                                      /* a pitched drum (TOMS, HAND DRUM): the root, sparse */
        uint8_t x = (uint8_t)dr_note(dream.deg[s], 3);
        dr_put(t, a, &x, 1, dr_vel(), 100);
        for (j = 0; j < n; j++)
            dr_put(t, a + 4u + 2u * dr_rnd(6), &x, 1, dr_vel() - 20u, 40u + dens * 5u);
    }
}

/* track t's part over span s, its steps there replaced */
static void dr_span(track_t *t, uint32_t s, int new_motif)
{
    uint32_t ti = trk_index(t), dens = (uint32_t)clamp(song.g[G_DDENS], 1, 8), j;
    uint32_t r = dream.role[ti];
    if (r == DR_DRONE && s)
        return;
    if (r != DR_PAD || !dream.dup[ti])                     /* (a late pad's span reaches into the next) */
        for (j = 0; j < DR_SPAN; j++)
            step_clear(&t->step[s * DR_SPAN + j]);
    switch (r) {
    case DR_DRONE: dr_drone(t, s, dens); break;
    case DR_BASS: dr_bass(t, s, dens); break;
    case DR_PAD: dr_pad(t, s, dens, dream.dup[ti]); break;
    case DR_ARP: dr_arp(t, s, dens, dream.dup[ti]); break;
    case DR_MELODY: dr_melody(t, s, dens, new_motif); break;
    case DR_HIGH: dr_high(t, s, dens); break;
    default: dr_perc(t, s, dens); break;
    }
}

static void dr_roles(void)
{
    uint32_t i, seen = 0;
    for (i = 0; i < NTRK; i++) {
        uint32_t r = dr_role(&trk[i]);
        dream.dup[i] = (uint8_t)((seen >> r) & 1u);
        if (r == DR_MELODY && dream.dup[i]) {              /* a second melodic voice: high and sparse */
            r = DR_HIGH;
            dream.dup[i] = (uint8_t)((seen >> r) & 1u);
        }
        seen |= 1u << r;
        dream.role[i] = (uint8_t)r;
    }
}

/* compose: a piece in key root (0..11 from C) */
static void dream_compose(uint32_t root)
{
    uint32_t i, s;
    seq_heard_forget();                                     /* (2.4: new steps, FREEZE waits for them to play) */
    const uint8_t *pg = DR_PROG[dr_rnd(sizeof DR_PROG / sizeof DR_PROG[0])];
    dream.root = (uint8_t)(root % 12u);
    dream.scl = DR_MOOD_SCL[clamp(song.g[G_DMOOD], 0, 5)];
    for (s = 0; s < 4u; s++)
        dream.deg[s] = pg[s];
    dr_roles();
    dr_mlen = 0;
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        motion_reset(t);
        for (s = 0; s < NSTEP; s++)
            step_clear(&t->step[s]);
        t->p[P_SLEN] = DR_LEN;
        t->p[P_SDIV] = 1;
        t->p[P_ROOT] = (int16_t)dream.root;
        t->p[P_SCALE] = (int16_t)dream.scl;
        t->p[P_CHRD] = 0;
        t->p[P_AMODE] = 0;
        if (t->eng_req % NENGINES == ENGI_GURDY)
            t->p[P_E0] = (int16_t)dream.root;              /* the drones on the key */
        for (s = 0; s < 4u; s++)
            dr_span(t, s, s == 0u);
    }
    dream.valid = 1;
    dream.passes = 0;
    if (song.playing)                                      /* every part from the top, together */
        transport_req = 3;
    else if (!chain_busy())
        transport_req = 1;
}

/* after power-on or a load: the key and the chords back from the tracks (track 1's ROOT and SCL; each span's chord: its
 * bass note's degree, else the triad the pad's notes spell, else the root's) */
static void dr_rebuild(void)
{
    uint8_t pc[12];
    uint32_t s, i, k, set, bass, n;
    dream.root = (uint8_t)(trk[0].p[P_ROOT] % 12);
    dream.scl = (uint8_t)trk[0].p[P_SCALE];
    n = dr_tones(pc);                                      /* (any scale: 5 .. 12 tones) */
    dr_roles();
    for (s = 0; s < 4u; s++) {
        for (set = 0, bass = n, i = 0; i < NTRK; i++) {
            const step_t *st = &trk[i].step[s * DR_SPAN];
            if (st->time != ST_NOTE || (dream.role[i] != DR_BASS && (dream.role[i] != DR_PAD || dream.dup[i])))
                continue;
            for (k = 0; k < st->n; k++)
                set |= 1u << (dr_degree(st->note[k]) % n);
            if (dream.role[i] == DR_BASS)
                bass = (uint32_t)dr_degree(st->note[0]) % n;
        }
        for (k = 0; bass == n && k < n; k++)
            if ((set >> k & 1u) && (set >> (k + 2u) % n & 1u) && (set >> (k + 4u) % n & 1u))
                bass = k;
        dream.deg[s] = (uint8_t)(bass < n ? bass : 0u);
    }
    dream.valid = 1;
}

/* EVOL: one change, at the end of a pass, in span s (2..4: the first, the motif's, stays): a new chord there (every part
 * but the drone again on it), or one part there played anew, or a part (not the drone, the bass or the first pad) out
 * or back in */
static void dream_evolve(void)
{
    uint32_t r = dr_rnd(4), s = 1u + dr_rnd(3), i = dr_rnd(NTRK), n, d;
    if (!dream.valid)
        dr_rebuild();
    if (!r) {
        d = 2u + dr_rnd(5);                                /* (a new one: never a loop on the random numbers) */
        if (d == dream.deg[s])
            d = d == 6u ? 2u : d + 1u;
        dream.deg[s] = (uint8_t)d;
        for (i = 0; i < NTRK; i++)
            dr_span(&trk[i], s, 0);
        return;
    }
    for (n = 0; n < NTRK; n++, i = (i + 1u) % NTRK)       /* from a track picked at random, the first that may */
        if (dream.role[i] != DR_DRONE && (r < 3u || (dream.role[i] != DR_BASS && (dream.role[i] != DR_PAD || dream.dup[i]))))
            break;
    if (n == NTRK)
        return;
    if (r < 3u)
        dr_span(&trk[i], s, 0);
    else
        trk[i].p[P_MUTE] = (int16_t)!trk[i].p[P_MUTE];
}

/* a key's piece: composed, and said ("DREAMING D DOR") */
static void dream_go(uint32_t root)
{
    char b[16];
    if (chain_busy()) {                                    /* (also after the dialog: a SONG started meanwhile) */
        ui_message("STOP THE SONG FIRST");
        return;
    }
    dream_compose(root);
    str_cpy(b, N_GURDY_DRN[root % 12u], 4);               /* (the note names: GURDY's DRN's) */
    str_cpy(b + str_len(b), " ", 2);
    str_cpy(b + str_len(b), N_DMOOD[clamp(song.g[G_DMOOD], 0, 5)], 6);
    ui_say("DREAMING ", b);
}

/* DREAM's keys (song.grid 4, silent): a press composes in its key; the first time, over steps that are there, after
 * the dialog (CF_DREAM) */
static void dream_keys(uint32_t notes)
{
    uint32_t k, i, j, any = 0;
    for (k = 0; k < 27u && !((notes >> k) & 1u); k++)
        ;
    if (k == 27u)
        return;
    for (i = 0; i < NTRK && !any; i++)
        for (j = 0; j < NSTEP && !any; j++)
            any = trk[i].step[j].n || trk[i].step[j].hit;
    if (any && !dream.asked && !chain_busy())
        confirm_open(CF_DREAM, (53u + k) % 12u);           /* (key 0 is F3) */
    else
        dream_go((53u + k) % 12u);
}

/* every pass of the UI: EVOL's clock (track 1 back at its first step: a pass), the restart a dream asked for */
static void dream_tick(void)
{
    uint32_t idx = trk[0].seq_idx, every = 9u - (uint32_t)clamp(song.g[G_DDRFT], 1, 8);
    if (!song.g[G_DREAM] || !song.playing || chain_busy()) {
        dream.last = 0;
        dream.passes = 0;
        return;
    }
    if (idx < dream.last && ++dream.passes >= every) {
        dream.passes = 0;
        dream_evolve();
    }
    dream.last = (uint8_t)idx;
}
