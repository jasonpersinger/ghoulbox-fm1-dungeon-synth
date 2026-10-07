/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* RATCH, a step's ratchet (core.h SF_RATCH, seq.c seq_step / seq_ratchet), on the real sequencer, project, user
 * preset and UI sources (with the stubs of tests/ui_test.c):
 *   PLAY       x1..x4: that many note-ons in one pass of the step, in equal parts of it, each retriggered after its
 *              gate; chords and the DRUM grid's hits repeat whole; x1 slides and ties out as before, x2..x4 never;
 *              the chance is rolled once per pass for all its parts; swing; STOP and an edit to fewer end them.
 *   PROJECTS   FUN8 round trip of x1..x4 (bit 7 of the velocity and chance bytes), an x1 project packs those bytes
 *              as before, FUN7 / FUN6 images load x1.
 *   PRESETS    a user preset's note pattern keeps it (flags 8 | 16, UP_PUT too); a drum grid record has no room: x1.
 *   UI         SEQ > CHANCE KNOB 3 (RATCH x1..x4, clamped, the cursor step; dimmed on a REST, a TIE, an empty
 *              step); the piano roll and the DRUM grid draw a ratcheted step in its parts, the roll no slide line
 *              out of one.
 * The editor protocol (STEP_SET / TRACK_STEP, INFO 52 01 04) is in tests/editor_test.c. Built and run by
 * tests/run_tests.sh. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

/* track 1 alone, a one-step loop: step 0 NOTE 60 (flags), RATCH hits, GATE 64, no swing; the transport started */
static track_t *one_step(uint32_t hits, uint32_t flags)
{
    track_t *t;
    ui_power_on();
    t = &trk[0];
    track_defaults_steps(t);
    t->p[P_SLEN] = 1;
    t->p[P_SGATE] = 64;
    t->p[P_SSWING] = 0;
    song.g[G_SWING] = 0;
    t->step[0] = (step_t){{60}, 1, ST_NOTE, (uint8_t)flags, 100};
    step_set_ratchet(&t->step[0], hits);
    seq_start();
    return t;
}
static uint32_t step_len(const track_t *t, uint32_t idx) { return step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), idx); }

/* note-ons (voice starts) over `blocks` CTL blocks; at[k]: the sample of the k-th; rel: releases seen between */
static uint32_t run(track_t *t, uint32_t blocks, uint32_t *at, uint32_t nat, uint32_t *rel)
{
    uint32_t b, n = 0, v0, was = 0;
    for (b = 0; b < blocks; b++) {
        v0 = vage;
        seq_tick(t, CTL);
        for (; v0 < vage; v0++, n++)
            if (at && n < nat)
                at[n] = b * CTL;
        if (rel && was && !t->seq_n)
            (*rel)++;
        was = t->seq_n;
    }
    return n;
}
/* blocks in `passes` passes of a one-step loop, the last block before the next pass left out */
static uint32_t passes(const track_t *t, uint32_t n) { return n * step_len(t, 0) / CTL - 1u; }

static int play(void)
{
    int bad = 0, ok = 1;
    uint32_t h, n, at[8], rel, len;
    track_t *t;
    for (h = 1; h <= 4u; h++) {
        t = one_step(h, 0);
        rel = 0;
        n = run(t, passes(t, 1), at, 8, &rel);
        len = step_len(t, 0);
        ok &= n == h && rel >= h - 1u;
        for (uint32_t k = 0; k < n && k < 8u; k++) {   /* part k at k / h of the step, to the block */
            int32_t d = (int32_t)at[k] - (int32_t)(k * (len / h));
            ok &= d > -(int32_t)CTL && d <= (int32_t)CTL;
        }
        if (n != h)
            printf("ui:   RATCH x%u: %u note-ons in one pass\n", h, n);
    }
    bad += check("RATCH x1..x4: that many note-ons in a pass, in equal parts, each ended by its gate", ok);
    t = one_step(4, 0);
    bad += check("  x4 over 8 passes: 32 note-ons", run(t, passes(t, 8), 0, 0, 0) == 32u);
    t = one_step(3, 0);
    t->p[P_VOICE] = V_POLY;
    t->step[0].n = 3; t->step[0].note[1] = 64; t->step[0].note[2] = 67;
    bad += check("  a chord of 3 at x3: 9 note-ons", run(t, passes(t, 1), 0, 0, 0) == 9u);
    t = one_step(1, SF_SLIDE);
    run(t, 4, 0, 0, 0);
    ok = t->seq_hold == 1u;
    t = one_step(2, SF_SLIDE);
    run(t, passes(t, 1), 0, 0, 0);
    bad += check("  x1 slides out as before; x2 never (its last part ends at its gate)", ok && !t->seq_hold && !t->seq_n);
    t = one_step(4, 0);
    t->p[P_SLEN] = 2;
    t->step[1] = (step_t){{0}, 0, ST_TIE};
    seq_start();
    run(t, step_len(t, 0) / CTL + step_len(t, 1) / CTL - 1u, 0, 0, 0);
    bad += check("  nor ties out: a TIE after a ratcheted step holds nothing", !t->seq_n);

    t = one_step(4, 0);                               /* the DRUM grid: hits of BD (accented) and CH */
    set_engine_of(t, ENGI_DRUM);
    t->engine = t->eng_req;
    t->step[0] = (step_t){{0}, 0, ST_NOTE, 0, 100, 1u << DV_KICK | 1u << DV_HATC, 1u << DV_KICK};
    step_set_ratchet(&t->step[0], 3);
    seq_start();
    n = run(t, passes(t, 1), 0, 0, 0);
    ok = 0;
    for (h = 0; h < NVOICE; h++)
        ok |= t->v[h].note == DRUM_LANE_NOTE[DV_KICK] && t->v[h].vel == 127u;
    bad += check("  DRUM grid: BD + CH at x3, 6 hits, BD accented", n == 6u && ok);
    step_set_chance(&t->step[0], 0);
    seq_start();
    bad += check("  chance 0: no part plays", run(t, passes(t, 2), 0, 0, 0) == 0u);

    t = one_step(4, 0);                               /* chance: one roll for all four parts */
    step_set_chance(&t->step[0], 50);
    {   /* a pass starts where the step's position wraps (seq_pos below a block) */
        uint32_t heard = 0, p = 0, odd = 0, v0;
        n = 0;
        while (p < 400u) {
            v0 = vage;
            seq_tick(t, CTL);
            if (t->seq_pos < CTL) {                  /* (this block started a pass: count the one before) */
                odd += p && n != 0u && n != 4u;
                heard += p ? n : 0u;
                p++;
                n = 0;
            }
            n += vage - v0;
        }
        bad += check("  chance 50 % at x4: each pass all 4 parts or none, about half", !odd && heard > 4u * 140u &&
                     heard < 4u * 260u);
    }

    t = one_step(2, 0);                               /* swing: a step's parts stay inside its swung length */
    t->p[P_SLEN] = 2;
    t->p[P_SSWING] = SWING_MAX;
    t->step[1] = t->step[0];
    seq_start();
    n = run(t, (step_len(t, 0) + step_len(t, 1)) / CTL - 1u, at, 8, 0);
    bad += check("  SWING 100: x2 on a long and a short step, 4 note-ons, the short step's within it",
                 n == 4u && at[1] < step_len(t, 0) && at[2] >= step_len(t, 0) - CTL &&
                 at[3] - at[2] <= step_len(t, 1) / 2u + CTL);

    t = one_step(4, 0);
    run(t, 2, 0, 0, 0);
    ok = t->rat_left == 3u;
    seq_stop();
    bad += check("  STOP: no part left", ok && !t->rat_left && !t->seq_n);
    t = one_step(4, 0);
    run(t, 2, 0, 0, 0);
    step_set_ratchet(&t->step[0], 1);
    bad += check("  an edit to x1 while it plays ends the parts", run(t, passes(t, 1) - 2u, 0, 0, 0) == 0u && !t->rat_left);
    seq_stop();
    return bad;
}

static int projects(void)
{
    int bad = 0, ok = 1;
    project_t before, after;
    project_store_t packed;
    uint32_t i, k, pos;
    track_t *t;
    ui_power_on();
    t = &trk[0];
    for (i = 0; i < 8u; i++) {
        t->step[i] = (step_t){{(uint8_t)(60 + i)}, 1, ST_NOTE, i & 1u ? SF_ACCENT : 0u, i & 2u ? 127u : 90u};
        step_set_ratchet(&t->step[i], 1u + i % 4u);
        step_set_chance(&t->step[i], i < 4u ? 100u : i == 4u ? 0u : 30u);
    }
    trk[3].step[5].hit = 0x81; trk[3].step[5].acc = 0x80; trk[3].step[5].time = ST_NOTE;
    step_set_ratchet(&trk[3].step[5], 4);
    project_capture(&before);
    ok = proj_pack(&packed, &before) && proj_import(&after, &packed, sizeof packed) && !memcmp(&before, &after, sizeof before);
    for (i = 0; ok && i < 8u; i++)
        ok &= step_ratchet(&after.t[0].step[i]) == 1u + i % 4u && step_chance(&after.t[0].step[i]) == step_chance(&before.t[0].step[i]) &&
              after.t[0].step[i].vel == before.t[0].step[i].vel;
    bad += check("FUN8: x1..x4 round trip with their velocity, accent, chance (0 and 30 %) and lane hits", ok &&
                 step_ratchet(&after.t[3].step[5]) == 4u && after.t[3].step[5].hit == 0x81);
    pos = 68u + P_COUNT + 2u + 3u * 9u;              /* step 3 of track 1: x4 -> bit 7 of its velocity and chance */
    bad += check("  x4 is bit 7 of the velocity and the chance bytes", (packed.raw[pos + 5] & 128u) && (packed.raw[pos + 8] & 128u) &&
                 (packed.raw[pos + 5] & 127u) == before.t[0].step[3].vel);
    for (k = 0; k < NTRK; k++)
        for (i = 0; i < NSTEP; i++)
            trk[k].step[i].flags &= (uint8_t)~SF_RATCH;
    project_capture(&before);
    proj_pack(&packed, &before);
    ok = 1;
    for (k = 0, pos = 68u; k < NTRK; k++, pos += P_COUNT + 2u + NSTEP * 9u)
        for (i = 0; i < NSTEP; i++)
            ok &= packed.raw[pos + P_COUNT + 2u + i * 9u + 5u] < 128u && packed.raw[pos + P_COUNT + 2u + i * 9u + 8u] < 128u;
    bad += check("  a project without ratchets: every velocity and chance byte below 128, as firmware before wrote", ok);
    {   /* a FUN7 image (its tracks without patches) as firmware before RATCH wrote it: x1 */
        project_store_t old;
        uint32_t sum, magic = PROJ_MAGIC_V7, size = PROJ_STORE_V7;
        project_t fun7;
        memcpy(&old, &packed, sizeof old);
        memcpy(old.raw, &magic, 4); memcpy(old.raw + 4, &size, 4);
        memset(old.raw + PROJ_STORE_V7 - 4u - PROJ_NAME_LEN, 0, PROJ_NAME_LEN);
        sum = proj_hash(old.raw, PROJ_STORE_V7 - 4u); memcpy(old.raw + PROJ_STORE_V7 - 4u, &sum, 4);
        ok = proj_import(&fun7, &old, PROJ_STORE_V7);
        for (k = 0; ok && k < NTRK; k++)
            for (i = 0; i < NSTEP; i++)
                ok &= step_ratchet(&fun7.t[k].step[i]) == 1u;
        bad += check("  FUN7 of before: every step x1", ok && fun7.t[0].step[1].flags == SF_ACCENT);
    }
    {   /* FUN6 (10-byte steps): only accent and slide come over, x1 */
        project_v6_t v6;
        memset(&v6, 0, sizeof v6);
        v6.magic = PROJ_MAGIC_V6; v6.size = sizeof v6; v6.parts = NPART; v6.phys = PROJ_PHYS;
        memcpy(v6.g, before.g, sizeof v6.g);
        chain_defaults(&v6.chain);
        for (k = 0; k < NTRK; k++) {
            for (i = 0; i < 69u; i++) v6.t[k].p[i] = TP[i < 61u ? i : i - 61u + P_E0].def;
            v6.t[k].step[0] = (step10_t){{60}, 1, ST_NOTE, 0xFBu, 100, 0, 0};   /* (stray high bits) */
        }
        v6.sum = proj_hash(&v6, sizeof v6 - 4u);
        bad += check("  FUN6: x1, accent and slide kept", proj_import(&after, &v6, sizeof v6) &&
                     step_ratchet(&after.t[0].step[0]) == 1u && after.t[0].step[0].flags == (SF_ACCENT | SF_SLIDE));
    }
    ui_power_on();                                   /* a real save and load */
    step_set_ratchet(&trk[1].step[7], 3);
    trk[1].step[7].time = ST_NOTE; trk[1].step[7].n = 1; trk[1].step[7].note[0] = 50;
    project_save(2);
    step_set_ratchet(&trk[1].step[7], 1);
    project_load(2);
    bad += check("  project save and load keep the ratchet", step_ratchet(&trk[1].step[7]) == 3u);
    return bad;
}

static int presets(void)
{
    int bad = 0;
    up_rec_t r;
    uint8_t a[400];
    uint32_t slot, i, k, n;
    track_t *t;
    ui_power_on();
    t = &trk[0];
    track_defaults_steps(t);
    for (i = 0; i < 16u; i++)
        t->step[i] = (step_t){{(uint8_t)(48 + i)}, 1, ST_NOTE, 0, 100};
    step_set_ratchet(&t->step[2], 3);
    t->step[5].flags = SF_ACCENT;
    step_set_ratchet(&t->step[5], 4);
    memset(&r, 0, sizeof r);
    up_pat_from(&r, t->step);
    track_defaults_steps(t);
    load_pat16(t, r.note, r.flags);
    bad += check("user preset note pattern: x3 and an accented x4 stored in its flags and loaded back",
                 step_ratchet(&t->step[2]) == 3u && step_ratchet(&t->step[5]) == 4u && t->step[5].flags & SF_ACCENT &&
                 step_ratchet(&t->step[0]) == 1u && r.flags[2] == 2u << SF_RATCH_SH);
    k = 0;                                           /* UP_PUT: slot, engine, name, values, 16 x (note, flags) */
    a[k++] = 3; a[k++] = (uint8_t)t->eng_req;
    memcpy(a + k, "RAT", 4); k += 4;
    for (i = 0; i < P_COUNT; i++) { a[k++] = (uint8_t)((t->p[i] + 8192) & 127); a[k++] = (uint8_t)((t->p[i] + 8192) >> 7); }
    for (i = 0; i < 16u; i++) { a[k++] = r.note[i]; a[k++] = r.flags[i]; }
    n = k;
    memset(&r, 0, sizeof r);
    bad += check("  UP_PUT keeps a pattern's ratchet", !up_parse(a, n, &r, &slot) && r.flags[2] == 2u << SF_RATCH_SH &&
                 r.flags[5] == (SF_ACCENT | 3u << SF_RATCH_SH));
    set_engine_of(t, ENGI_DRUM);                     /* a drum grid record: no room, x1 */
    t->engine = t->eng_req;
    track_defaults_steps(t);
    t->step[0] = (step_t){{0}, 0, ST_NOTE, 0, 100, 1u << DV_KICK};
    step_set_ratchet(&t->step[0], 2);
    up_store(4, "GRID");
    up_pat_load(t, 4);
    bad += check("  a DRUM grid record loads its hits x1", up_grid(up_rec(4)) && t->step[0].hit == 1u << DV_KICK &&
                 step_ratchet(&t->step[0]) == 1u);
    return bad;
}

/* pixels of colour c in card slot k (the cards' row) */
static uint32_t card_px(uint32_t k, uint16_t c)
{
    uint32_t n = 0;
    int32_t x, y;
    for (y = Y_LABEL; y < Y_LABEL + CARD_H; y++)
        for (x = CARD_X(k); x < CARD_X(k) + CARD_W; x++)
            n += swap16(host_screen[y * 240 + x]) == c;
    return n;
}
/* the slide line's pixels (TEXT) in the gap after step column i of the piano roll, between rows lo and hi */
static uint32_t pr_slide_px(uint32_t i, int32_t lo, int32_t hi)
{
    uint32_t n = 0;
    int32_t x, y, x0 = PR_X0 + (int32_t)i * PR_CW;
    for (y = Y_GRAPH + pr_row_y(hi); y <= Y_GRAPH + pr_row_y(lo) + PR_RH; y++)
        for (x = x0 + 11; x <= x0 + 13; x += 2)          /* (x0 + 12 the step line, x0 + 14 the next bars) */
            n += swap16(host_screen[y * 240 + x]) == T_TEXT;
    return n;
}

static int screens(void)
{
    int bad = 0, ok;
    track_t *t;
    uint32_t x, y;
    ui_power_on();
    t = TSEL;
    go_page(GR_CHANCE);
    ui.cursor = 0;
    turn(EN_K3, 2);
    ok = step_ratchet(&t->step[0]) == 3u && step_ratchet(&t->step[1]) == 1u;
    turn(EN_K3, 100);
    ok &= step_ratchet(&t->step[0]) == 4u;
    turn(EN_K3, -100);
    bad += check("SEQ > CHANCE KNOB 3: RATCH of the cursor step, x1..x4", ok && step_ratchet(&t->step[0]) == 1u &&
                 step_chance(&t->step[0]) == 100u);
    {                                                /* RATCH dimmed where it does nothing: REST, TIE, empty */
        uint32_t dim[4], thm[4], k;
        static const step_t kind[4] = {
            {{60}, 1, ST_NOTE, 0, 100}, {{0}, 0, ST_REST, 0, 0}, {{0}, 0, ST_TIE, 0, 0}, {{0}, 0, ST_NOTE, 0, 0}};
        for (k = 0; k < 4u; k++) {
            t->step[0] = kind[k];
            step_set_ratchet(&t->step[0], 2);
            memset(host_screen, 0, sizeof host_screen); ui.force = 1; ui_draw();
            dim[k] = card_px(2, T_DIM); thm[k] = card_px(2, T_THEME) + card_px(2, T_ACCENT);   /* lit: THEME, or the accent while hot
                                                                                      * (GHOULBOX: the card's border is THEME too) */
        }
        t->step[0] = kind[3]; t->step[0].hit = 1u;   /* a NOTE step of lane hits only: it plays */
        step_set_ratchet(&t->step[0], 2);
        memset(host_screen, 0, sizeof host_screen); ui.force = 1; ui_draw();
        ok = card_px(2, T_DIM) == dim[0] && card_px(2, T_THEME) + card_px(2, T_ACCENT) == thm[0];
        for (k = 1; k < 4u; k++)
            ok &= dim[k] > dim[0] && thm[k] < thm[0];
        bad += check("  RATCH shows dimmed on a REST, a TIE or an empty step (a note or a hit: not)", ok);
        t->step[0] = (step_t){{0}, 0, ST_REST, 0, 0};
    }
    ui_power_on();                                   /* the piano roll: x2 in two bars, the middle column empty */
    t = TSEL;
    track_defaults_steps(t);
    t->p[P_SLEN] = 16;
    t->step[1] = (step_t){{60}, 1, ST_NOTE, 0, 100};
    t->step[3] = t->step[1];
    step_set_ratchet(&t->step[1], 2);
    go_page(GR_ROLL); ui.cursor = 8; ui.bank = 0;
    memset(host_screen, 0, sizeof host_screen); ui.force = 1; ui_draw();
    y = Y_GRAPH + (uint32_t)pr_row_y(60) + 2u;
    x = PR_X0 + 1u * PR_CW;
    ok = pr_is_bar(x + 3u, y) && pr_is_bar(x + 9u, y) && !pr_is_bar(x + 6u, y);
    x = PR_X0 + 3u * PR_CW;
    bad += check("  the piano roll draws an x2 step in two parts (x1 whole)", ok && pr_is_bar(x + 6u, y));
    t->step[4] = (step_t){{64}, 1, ST_NOTE, 0, 100};   /* SLIDE into a note: the line, but not from x2 */
    t->step[3].flags = SF_SLIDE;
    t->step[1].flags |= SF_SLIDE;                     /* (x2 kept) */
    t->step[2] = t->step[4];
    memset(host_screen, 0, sizeof host_screen); ui.force = 1; ui_draw();
    bad += check("  a slide line from an x1 SLIDE step, none from an x2 one", pr_slide_px(3, 60, 64) > 0u &&
                 pr_slide_px(1, 60, 64) == 0u);
    t->step[1].flags &= (uint8_t)~SF_SLIDE; t->step[3].flags = 0;
    t->step[2] = t->step[4] = (step_t){{0}, 0, ST_REST, 0, 0};
    set_engine_of(t, ENGI_DRUM);                     /* the grid: x3 in three bars */
    t->engine = t->eng_req;
    t->step[1] = (step_t){{0}, 0, ST_NOTE, 0, 100, 1u};
    step_set_ratchet(&t->step[1], 3);
    ui.lane = 1;
    memset(host_screen, 0, sizeof host_screen); ui.force = 1; ui_draw();
    y = Y_GRAPH + 6u + 4u + 6u;                      /* lane 0's hits: y 12 .. 21 of the panel */
    x = 40u + 12u;
    bad += check("  the DRUM grid draws an x3 hit in three parts", pr_is_bar(x, y) && !pr_is_bar(x + 2u, y) &&
                 pr_is_bar(x + 3u, y) && !pr_is_bar(x + 5u, y) && pr_is_bar(x + 6u, y));
    return bad;
}

int main(void)
{
    int bad = play() + projects() + presets() + screens();
    printf("%s\n", bad ? "RATCHET TEST FAILED" : "ratchet tests passed");
    return bad != 0;
}
