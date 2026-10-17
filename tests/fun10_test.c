/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* 1.2's stored-format changes (FUN10), on the real sound, sequencer, project, user preset and UI sources (with the
 * stubs of tests/ui_test.c):
 *   LFO 2      SYNC: one cycle per note value of the tempo (several values and tempi, the external clock's measured
 *              tempo), RATE ignored meanwhile; OFF: RATE's table exactly as before. TRIG: NOTE restarts at PHS on a
 *              fresh phrase (as before), FREE never (with SYNC: from PHS when PLAY starts). POL: UNI 0..+1 (the
 *              wave's range, LFO DEST AMP and the matrix's AMP as ENV takes it), BI as before.
 *   NUDGE      live recording keeps a note's time (1/16 of a step, rounded, -8..+7) in a fresh step, early notes as
 *              the next step's negative nudge; QUANTIZE ON (the default) plays every step on its start as before;
 *              OFF plays it nudged, late and early, once per pass, never twice with what live recording sounds
 *              already; a ratcheted step on its start; every nudge 0 (older patterns) plays the same either way;
 *              the controls (1.2 pages, Discussion #153): SEQ > AUTOMATION's QUANTIZE row (KNOB 4 ON / OFF) and NUDGE
 *              rows (+ ADD NUDGE +4/16, KNOB 4 -8..+7, KNOB 2 the step, EDIT 0, SAVE held undoes; "QUANTIZE IS ON"
 *              while it is), and on STEP steps held + the PRESETS knob (let go: the cursor).
 *   MOTION     128 records shared by the four tracks, all played.
 *   FORMATS    FUN10 round trip (nudges, 128 records, the four parameters), FUN9 / FUN8 / FUN7 as 1.1 / 1.0 wrote
 *              them load as before (tests/old_pack.h), a FUN9 claiming more than 64 records or a nudge bit refused, a
 *              user preset's pattern keeps no nudge (its bit 4 is the tie), user presets of 99 and 103 parameters.
 * Built and run by tests/run_tests.sh. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "old_pack.h"

/* ---------------------------------------------------------------- LFO 2 --- */
/* the LFO's phase over `blocks` blocks, in cycles (wraps + the phase) */
static double lfo_cycles(track_t *t, uint32_t blocks)
{
    uint32_t b, wraps = 0, ph0 = t->lfo_ph;
    for (b = 0; b < blocks; b++) {
        uint32_t old = t->lfo_ph;
        track_lfo_tick(t);
        wraps += t->lfo_ph < old;
    }
    return wraps + ((double)t->lfo_ph - (double)ph0) / 4294967296.0;
}

static int lfo(void)
{
    static const uint8_t VALS[] = {1, 3, 5, 6, 7, 8, 9, 10};   /* 4BAR 1/1 1/4 1/8 8T 1/16 16T 1/32 */
    static const double BEATS[11] = {0, 16, 8, 4, 2, 1, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125};
    static const int16_t BPM[] = {40, 97, 120, 173, 240};
    int bad = 0, ok = 1;
    uint32_t i, j, b;
    track_t *t;
    double worst = 0;
    ui_power_on();
    t = &trk[0];
    bad += check("LFO 2 defaults: SYNC OFF, TRIG NOTE, POL BI (as before 1.2); QUANTIZE ON",
                 !TP[P_LSYNC].def && !TP[P_LTRIG].def && !TP[P_LPOL].def && TP[P_SQNT].def == 1 && !t->p[P_LSYNC] &&
                 !t->p[P_LTRIG] && !t->p[P_LPOL] && t->p[P_SQNT] == 1);
    for (i = 0; i < 128u; i += 7u) {                 /* SYNC OFF: RATE's table, the value as before */
        uint32_t ph = 0x12345678u;
        t->p[P_LRATE] = (int16_t)i;
        t->lfo_ph = ph;
        track_lfo_tick(t);
        ok &= t->lfo_ph == ph + LFO_INC[i] && t->lfo_val == osc_sine(ph + LFO_INC[i]);
    }
    bad += check("SYNC OFF: the phase moves by RATE's LFO_INC, the value the wave's, bit for bit", ok);
    ok = 1;
    for (i = 0; i < NELEM(BPM); i++)
        for (j = 0; j < NELEM(VALS); j++) {
            double cyc = FS * 60.0 / BPM[i] * BEATS[VALS[j]], want, got;
            uint32_t blocks = (uint32_t)(cyc * 8.0 / CTL);   /* eight cycles */
            song.g[G_BPM] = BPM[i];
            t->p[P_LSYNC] = VALS[j];
            t->p[P_LRATE] = 127;                     /* (ignored) */
            t->lfo_ph = 0;
            got = lfo_cycles(t, blocks);
            want = (double)blocks * CTL / (double)div_samples(LSYNC_DIV[VALS[j]]);
            ok &= fabs(got - want) < 2e-4 && fabs(got - (double)blocks * CTL / cyc) < 0.003 * got;
            if (fabs(got - want) > worst) worst = fabs(got - want);
        }
    printf("ui:   SYNC: worst phase error against div_samples over 8 cycles %.2g cycles\n", worst);
    bad += check("SYNC 4BAR..1/32 at 40, 97, 120, 173, 240 BPM: one cycle per note value (RATE ignored)", ok);
    song.g[G_BPM] = 120;
    t->p[P_LSYNC] = 5;                               /* 1/4 */
    t->lfo_ph = 0;
    ok = fabs(lfo_cycles(t, (uint32_t)(22050u * 100u / CTL)) - 100.0 * (22050u * 100u / CTL * CTL) / 2205000.0) < 1e-4;
    bad += check("  1/4 at 120 BPM: 100 beats are 100 cycles (no drift)", ok);
    {   /* the external clock: USB, 24 pulses a quarter at 100 BPM (25 ms apart): the measured tempo sets the cycle */
        uint32_t ms = 1000u;
        song.g[G_CLOCK] = 1;
        midi_beat_samples = 0;
        memset(&midi_clock, 0, sizeof midi_clock);
        for (i = 0; i < 13u; i++, ms += 25u)
            midi_clock_pulse(ms);
        t->lfo_ph = 0;
        ok = midi_beat_samples == 26460u && fabs(lfo_cycles(t, 26460u * 4u / CTL) - (double)(26460u * 4u / CTL) * CTL / 26460.0) < 1e-4;
        for (i = 0; i < 13u; i++, ms += 30u)        /* slower: 83 BPM */
            midi_clock_pulse(ms);
        t->lfo_ph = 0;
        ok &= midi_beat_samples == 31752u && fabs(lfo_cycles(t, 31752u * 2u / CTL) - (double)(31752u * 2u / CTL) * CTL / 31752.0) < 1e-4;
        bad += check("SYNC on the external clock: 1/4 = the measured beat (100, then 83 BPM)", ok);
        song.g[G_CLOCK] = 0;
        midi_beat_samples = 0;
    }
    t->p[P_LSYNC] = 0;
    /* TRIG: NOTE restarts at PHS on a fresh phrase, FREE never */
    t->p[P_LPHASE] = 32;
    t->lfo_ph = 0x55555555u;
    trk_note_on(t, 60, 100);
    ok = t->lfo_ph == 32u << 25 && t->lfo_fade == 0;
    trk_all_off(t);
    for (b = 0; b < 2000u; b++) env_tick(t, &t->v[0]);
    for (i = 0; i < NVOICE; i++) t->v[i].gate = 0;
    t->p[P_LTRIG] = 1;
    t->lfo_ph = 0x55555555u;
    t->lfo_fade = 32767;
    trk_note_on(t, 62, 100);
    ok &= t->lfo_ph == 0x55555555u && t->lfo_fade == 0;
    bad += check("TRIG NOTE: a fresh phrase restarts the LFO at PHS; FREE: never (FADE still restarts)", ok);
    t->p[P_LSYNC] = 5;
    t->lfo_ph = 0x55555555u;
    seq_start();
    ok = t->lfo_ph == 32u << 25;
    seq_stop();
    t->p[P_LSYNC] = 0;
    t->lfo_ph = 0x55555555u;
    seq_start();
    ok &= t->lfo_ph == 0x55555555u;
    seq_stop();
    bad += check("  FREE + SYNC: from PHS when PLAY starts (in time with the bars); FREE alone: not even then", ok);
    trk_all_off(t);
    t->p[P_LTRIG] = 0;
    t->p[P_LPHASE] = 0;
    /* POL: BI -1..+1 (as before), UNI 0..+1 */
    {
        int32_t lo[2] = {99999, 99999}, hi[2] = {-99999, -99999};
        uint32_t pol;
        t->p[P_LWAVE] = 2;                           /* SAW: every value */
        t->p[P_LRATE] = 100;
        for (pol = 0; pol < 2u; pol++) {
            t->p[P_LPOL] = (int16_t)pol;
            for (b = 0; b < 20000u; b++) {
                track_lfo_tick(t);
                lo[pol] = t->lfo_val < lo[pol] ? t->lfo_val : lo[pol];
                hi[pol] = t->lfo_val > hi[pol] ? t->lfo_val : hi[pol];
            }
        }
        printf("ui:   POL BI %d..%d, UNI %d..%d\n", lo[0], hi[0], lo[1], hi[1]);
        bad += check("POL UNI: 0..+1 (0..32767), BI -1..+1 as before", lo[0] < -32000 && hi[0] > 32000 && lo[1] >= 0 &&
                     lo[1] < 300 && hi[1] <= 32767 && hi[1] > 32400);
    }
    {   /* the matrix's AMP from the LFO: BI takes it as 0..1 ((x + 1) / 2), UNI as it is (as ENV) */
        int32_t g[4];
        t->p[P_M1SRC] = MS_LFO; t->p[P_M1DST] = MD_AMP; t->p[P_M1AMT] = 63;
        t->lfo_fade = 32767;
        t->p[P_LPOL] = 0; t->lfo_val = 0; mod_begin(t); g[0] = mod.gain; mod_end(t);
        t->p[P_LPOL] = 1; t->lfo_val = 0; mod_begin(t); g[1] = mod.gain; mod_end(t);
        t->lfo_val = 16384; mod_begin(t); g[2] = mod.gain; mod_end(t);
        t->p[P_LPOL] = 0; t->lfo_val = -32768; mod_begin(t); g[3] = mod.gain; mod_end(t);
        bad += check("  the matrix's LFO > AMP: UNI 0 = BI -1 (closed), UNI 0.5 = BI 0", g[1] == g[3] && abs(g[2] - g[0]) < 64 &&
                     g[1] < 1000);
        t->p[P_M1SRC] = t->p[P_M1DST] = t->p[P_M1AMT] = 0;
        t->p[P_LPOL] = 0;
    }
    bad += check("LFO 2 is no automation (motion_param), its page SYNC TRIG POL after LFO (LFO's family)",
                 !motion_param(P_LSYNC) && !motion_param(P_LTRIG) && !motion_param(P_LPOL) && !motion_param(P_SQNT) &&
                 motion_param(P_LN7) && motion_param(P_E0) && PAGES[3].id[0] == P_LSYNC && !strcmp(PAGES[3].title, "LFO 2") &&
                 PAGES[3].fam == FAM_LFO && PAGES[2].graph == GR_LFO && !strcmp(PAGES[4].title, "LFO DEST"));
    go_page(GR_LFO);
    ui.page++;                                       /* LFO 2 */
    page_entered();
    frame();
    turn(EN_K1, 3);
    turn(EN_K3, 1);
    bad += check("  LFO 2's knobs: SYNC (4BAR 2BAR 1/1 ..), POL", t->p[P_LSYNC] == 3 && t->p[P_LPOL] == 1 &&
                 !strcmp(N_LSYNC[3], "1/1"));
    return bad;
}

/* ---------------------------------------------------------------- NUDGE --- */
static uint32_t P16;                                 /* the 1/16 step at 120 BPM */
/* track 1 alone: 16 steps of 1/16, GATE 64, no swing, POLY; QUANTIZE as given */
static track_t *seq1(uint32_t qntz)
{
    track_t *t;
    ui_power_on();
    t = &trk[0];
    track_defaults_steps(t);
    t->p[P_SLEN] = 16;
    t->p[P_SDIV] = 2;
    t->p[P_SGATE] = 64;
    t->p[P_SSWING] = 0;
    t->p[P_VOICE] = V_POLY;
    t->p[P_SQNT] = (int16_t)qntz;
    song.g[G_SWING] = 0;
    song.g[G_BPM] = 120;
    P16 = div_samples(2);
    return t;
}
static void note_step(track_t *t, uint32_t i, uint32_t note, int32_t nudge)
{
    t->step[i] = (step_t){{(uint8_t)note}, 1, ST_NOTE, 0, 100};
    step_set_nudge(&t->step[i], nudge);
}
/* note-ons over `blocks` blocks from PLAY: at[k] the sample of the block the k-th started in, note[k] its note */
static uint32_t play(track_t *t, uint32_t blocks, uint32_t *at, uint8_t *note, uint32_t nat)
{
    uint32_t b, n = 0;
    seq_start();
    for (b = 0; b < blocks; b++) {
        uint32_t v0 = vage, i;
        seq_tick(t, CTL);
        for (; v0 < vage; v0++, n++)
            if (n < nat) {
                at[n] = b * CTL;
                for (i = 0; i < NVOICE; i++)
                    if (t->v[i].age == v0 + 1u)
                        note[n] = t->v[i].note;
            }
    }
    seq_stop();
    return n;
}
/* the block step i (from PLAY, no swing) plus `off` samples falls in, as a sample */
static uint32_t blk(uint32_t i, int32_t off)
{
    uint32_t s = (uint32_t)((int32_t)(i * P16) + off);
    return (s + CTL - 1u) / CTL * CTL;
}

static int nudge(void)
{
    int bad = 0, ok;
    uint32_t at[64], at2[64], n, n2, i;
    uint8_t nt[64], nt2[64];
    track_t *t;

    /* live recording: the time of a note into a fresh step */
    t = seq1(1);
    song.sel = 0;
    song.rec = 1;
    seq_start();
    seq_tick(t, CTL);                                /* step 1 has started */
    t->seq_pos = P16 * 3u / 16u;                     /* 3/16 into step 1 */
    input_on(t, 64, 100);
    input_off(t, 64);
    ok = t->step[0].n == 1u && t->step[0].note[0] == 64u && step_nudge(&t->step[0]) == 3;
    t->seq_pos = P16 - P16 / 4u;                     /* 1/4 before step 2 */
    input_on(t, 65, 100);
    input_off(t, 65);
    ok &= t->step[1].note[0] == 65u && step_nudge(&t->step[1]) == -4;
    t->seq_pos = P16 / 16u + 40u;                    /* just past 1/16: rounds to it */
    t->seq_idx = 4;
    input_on(t, 66, 100);
    input_off(t, 66);
    ok &= t->step[4].note[0] == 66u && step_nudge(&t->step[4]) == 1;
    t->seq_idx = 4;
    t->seq_pos = P16 / 8u;                           /* a second note into step 5 (a chord): the first's time stays */
    input_on(t, 69, 100);
    input_off(t, 69);
    ok &= t->step[4].n == 2u && step_nudge(&t->step[4]) == 1;
    seq_stop();
    bad += check("REC: a note's time in its fresh step (+3/16, -4/16 into the next, rounded); a chord keeps the first's", ok);
    t = seq1(1);
    song.sel = 0; song.rec = 1;
    t->p[P_SSWING] = 100;                            /* even steps 1.4 x: a note just before the middle */
    seq_start();
    seq_tick(t, CTL);
    t->seq_pos = step_samples(t, P16, 0) / 2u - 4u;
    input_on(t, 60, 100);
    input_off(t, 60);
    seq_stop();
    bad += check("  a note 0.7 of a step late (SWING 100): +7, the most", step_nudge(&t->step[0]) == 7);
    t = seq1(1);
    song.sel = 0; song.rec = 1;
    t->p[P_AMODE] = 1;                               /* the ARP: what it plays, as it plays it */
    seq_start();
    seq_tick(t, CTL);
    t->seq_pos = P16 / 2u - 8u;
    input_on(t, 60, 100);
    arp_step(t, CTL, CTL);
    input_off(t, 60);
    seq_stop();
    bad += check("  the ARP's notes recorded with their time too", t->step[0].note[0] == 60u && step_nudge(&t->step[0]) == 8 - 1);

    /* playback: QUANTIZE ON plays every step on its start (as before); OFF plays the nudges */
    t = seq1(1);
    note_step(t, 0, 60, 0);
    note_step(t, 2, 62, 5);
    note_step(t, 5, 65, -3);
    note_step(t, 8, 68, -8);
    note_step(t, 12, 72, 7);
    n = play(t, 16u * P16 / CTL, at, nt, 64);
    ok = n == 5u && at[0] == 0 && at[1] == blk(2, 0) && at[2] == blk(5, 0) && at[3] == blk(8, 0) && at[4] == blk(12, 0);
    bad += check("QUANTIZE ON (the default): every step on its start, whatever its nudge (as before 1.2)", ok);
    t->p[P_SQNT] = 0;
    n = play(t, 16u * P16 / CTL, at, nt, 64);
    ok = n == 5u && at[0] == 0 && nt[1] == 62 && at[1] == blk(2, (int32_t)(5u * P16 / 16u)) && nt[2] == 65 &&
         at[2] == blk(5, -(int32_t)(3u * P16 / 16u)) && nt[3] == 68 && at[3] == blk(8, -(int32_t)(8u * P16 / 16u)) &&
         nt[4] == 72 && at[4] == blk(12, (int32_t)(7u * P16 / 16u));
    printf("ui:   OFF: at %u %u %u %u %u (steps at %u %u %u %u)\n", at[0], at[1], at[2], at[3], at[4], blk(2, 0),
           blk(5, 0), blk(8, 0), blk(12, 0));
    bad += check("QUANTIZE OFF: +5/16, -3/16, -8/16 (the step before), +7/16, to the block; each once a pass", ok);
    note_step(t, 0, 60, -2);                         /* step 1 early: at the end of the pass before */
    n = play(t, 32u * P16 / CTL, at, nt, 64);
    ok = n == 11u && at[0] == 0 && nt[5] == 60 && at[5] == blk(16, -(int32_t)(2u * P16 / 16u));
    bad += check("  step 1 early: on PLAY at once, then 2/16 before each pass (the third's in the second)", ok);
    t->p[P_SLEN] = 1;                                /* a one-step loop, early: once a pass */
    note_step(t, 0, 60, -4);
    n = play(t, 8u * P16 / CTL, at, nt, 64);
    bad += check("  a one-step loop with -4/16: one note a pass (8 passes: 9, the ninth's early)", n == 9u && at[1] == blk(1, -(int32_t)(P16 / 4u)));
    t->p[P_SLEN] = 16;

    /* the external clock: a tempo change rescales a latched nudged play with the step (as seq_pos) */
    t = seq1(0);
    note_step(t, 1, 61, 6);
    song.g[G_CLOCK] = 1;
    memset(&midi_clock, 0, sizeof midi_clock);
    midi_beat_samples = 0;
    seq_start();
    for (i = 0; i < 13u; i++)
        midi_clock_pulse(1000u + 25u * i);           /* 100 BPM */
    P16 = div_samples(2);
    t->seq_idx = 0;
    t->seq_pos = P16 - 10u;
    seq_tick(t, CTL);                                /* step 2 starts: its play latched at 6/16 */
    ok = t->seq_idx == 1u && t->nd_late == 6u * P16 / 16u + 1u;
    for (i = 0; i < 13u; i++)
        midi_clock_pulse(2000u + 50u * i);           /* 50 BPM: the step twice as long */
    ok &= t->nd_late > 2u * (6u * P16 / 16u) - 8u && t->nd_late < 2u * (6u * P16 / 16u) + 8u;
    seq_stop();
    song.g[G_CLOCK] = 0;
    midi_beat_samples = 0;
    bad += check("external clock: a tempo change moves a latched nudged play with the step (100 -> 50 BPM: twice as late)", ok);

    /* every nudge 0 (an older pattern): ON and OFF play the same notes at the same times */
    t = seq1(1);
    for (i = 0; i < 16u; i += 3u)
        note_step(t, i, 50u + i, 0);
    n = play(t, 32u * P16 / CTL, at, nt, 64);
    t->p[P_SQNT] = 0;
    n2 = play(t, 32u * P16 / CTL, at2, nt2, 64);
    bad += check("every nudge 0: QUANTIZE OFF plays exactly as ON", n == n2 && n == 12u && !memcmp(at, at2, n * 4u) &&
                 !memcmp(nt, nt2, n));
    /* ... with SWING and a chance roll on the way: the shared generator called in the same order */
    t = seq1(1);
    t->p[P_SSWING] = 60;
    for (i = 0; i < 16u; i++) {
        note_step(t, i, 50u + i, 0);
        step_set_chance(&t->step[i], 50);
    }
    rng_state = 1234567u;
    n = play(t, 48u * P16 / CTL, at, nt, 64);
    t->p[P_SQNT] = 0;
    rng_state = 1234567u;
    n2 = play(t, 48u * P16 / CTL, at2, nt2, 64);
    bad += check("  with SWING 60 and CHANCE 50 %: the same notes, the same times", n == n2 && n > 8u && !memcmp(at, at2, n * 4u));

    /* a ratcheted step plays on its start */
    t = seq1(0);
    note_step(t, 2, 62, 6);
    step_set_ratchet(&t->step[2], 2);
    n = play(t, 4u * P16 / CTL, at, nt, 64);
    bad += check("a RATCH x2 step with a nudge: its parts on its start (the nudge ignored)", n == 2u && at[0] == blk(2, 0));

    /* live recording into a step whose nudged play is still to come: not played twice */
    t = seq1(0);
    song.sel = 0; song.rec = 1;
    note_step(t, 0, 60, 6);
    seq_start();
    {
        uint32_t v0 = vage, b, ons;
        seq_tick(t, CTL);                            /* step 1 starts; its play latched at 6/16 */
        input_on(t, 67, 100);                        /* (at 0: into step 1, a chord with 60) */
        ons = vage - v0;
        for (b = 1; b < P16 / CTL; b++)
            seq_tick(t, CTL);
        ok = vage - v0 == 2u && ons == 1u && t->step[0].n == 2u;
        input_off(t, 67);
    }
    seq_stop();
    bad += check("REC into a step before its nudged play: 60 at its time, the live 67 not again (2 note-ons)", ok);

    /* the controls: SEQ > AUTOMATION's QUANTIZE and NUDGE rows, STEP with steps held + PRESETS */
    t = seq1(1);
    song.sel = 0;
    note_step(t, 3, 63, 0);
    {
        uint16_t rw[EV_ROWS];
        step_t ref[NSTEP];
        uint32_t n;
        go_auto_top(); frame();
        turn(EN_K1, 1);                                 /* PLAY -> QUANTIZE */
        ok = auto_cur() == EVC(EVK_QNT, 0) && t->p[P_SQNT] == 1;
        turn(EN_K4, -1);
        ok &= t->p[P_SQNT] == 0;
        turn(EN_K4, 1);
        ok &= t->p[P_SQNT] == 1;
        press(B_OCTUP); press(B_EDIT);
        ok &= t->p[P_SQNT] == 1 && msg_is("NOTHING TO DELETE") && !act_ready() && str_eq(act_name(4), "--");
        bad += check("AUTOMATION's QUANTIZE row (the second): KNOB 4 OFF / ON; OCT+ and EDIT nothing", ok);
        memcpy(ref, t->step, sizeof ref);
        go_auto_add(EV_NUDGE, 3); frame();
        n = ev_list(rw);
        press(B_OCTUP);
        ok = step_nudge(&t->step[3]) == 4 && auto_cur() == EVC(EVK_NUDGE, 3) && msg_is("QUANTIZE IS ON") &&
             ev_list(rw) == n + 1u && !memcmp(&t->step[4], &ref[4], sizeof ref[4] * (NSTEP - 4u));
        turn(EN_K4, 1); turn(EN_K4, 1); turn(EN_K4, 1); turn(EN_K4, 1);
        ok &= step_nudge(&t->step[3]) == 7 && msg_is("QUANTIZE IS ON");
        for (i = 0; i < 20u; i++) turn(EN_K4, -1);
        ok &= step_nudge(&t->step[3]) == -8;
        bad += check("AUTOMATION + ADD NUDGE: step 4 +4/16 (QUANTIZE ON: says so), KNOB 4 -8..+7", ok);
        frame();
        t->p[P_SQNT] = 0;
        ui.msg_t = 0;
        for (i = 0; i < 11u; i++) turn(EN_K4, 1);
        bad += check("  QUANTIZE OFF: no message", step_nudge(&t->step[3]) == 3 && !msg_is("QUANTIZE IS ON"));
        frames(2000);                                   /* (the next turn its own undo) */
        turn(EN_K2, 1);                                 /* to step 5: step 4 back to 0 */
        ok = step_nudge(&t->step[3]) == 0 && step_nudge(&t->step[4]) == 3 && auto_cur() == EVC(EVK_NUDGE, 4);
        hold(B_SAVE);
        ok &= step_nudge(&t->step[3]) == 3 && step_nudge(&t->step[4]) == 0;
        hold(B_SAVE);
        ok &= step_nudge(&t->step[4]) == 3;
        bad += check("  KNOB 2 moves it to the next step; SAVE held undoes the move, held again redoes it", ok);
        press(B_EDIT);
        ok = step_nudge(&t->step[4]) == 0 && msg_is("DELETED");
        bad += check("  EDIT: back to 0 (the row goes)", ok && ev_list(rw) == n);
        t->p[P_SQNT] = 1;
    }
    {   /* the DRUM grid on STEP: a step key held + the PRESETS knob; let go: the hit stays */
        uint32_t k4;
        song.sel = 3;
        t = &trk[3];
        track_defaults_steps(t);
        t->p[P_SLEN] = 16;
        t->p[P_SQNT] = 0;
        go_home();
        open_family(FAM_SEQ);
        frame();
        grid_hit(t, 4, 0, 1);
        k4 = key_at(0, 4);
        uint32_t cur;
        fm1_in.notes |= 1u << k4; host_notes |= 1u << k4; frame();
        cur = ui.cursor;
        turn(EN_PRESET, -1); turn(EN_PRESET, -1);
        ok = step_nudge(&t->step[4]) == -2 && msg_is("NUDGE -2") && ui.cursor == cur;
        fm1_in.notes &= ~(1u << k4); frame();
        ok &= (t->step[4].hit & 1u) != 0u;
        turn(EN_PRESET, 1);
        ok &= ui.cursor == cur + 1u && step_nudge(&t->step[4]) == -2;
        bad += check("STEP (the grid): a step held + PRESETS: its NUDGE (-2); let go, the hit stays; alone, the cursor", ok);
        hold(B_SAVE);
        bad += check("  SAVE held: that turn undone (one undo), the hit kept", step_nudge(&t->step[4]) == 0 &&
                     (t->step[4].hit & 1u) != 0u);
    }
    return bad;
}

/* ------------------------------------------------------------ MOTION 128 --- */
static int motion128(void)
{
    int bad = 0, ok = 1;
    uint32_t i;
    track_t *t = seq1(1);
    for (i = 0; i < MOTION_MAX - 1u; i++)            /* 127 on tracks 2..4 */
        ok &= !motion_set_event(&trk[1 + i % 3u], (i / 3u) % NSTEP, i / 3u < NSTEP ? P_REV : P_DLY, 5);
    ok &= !motion_set_event(t, 3, P_REV, 90) && motion.count == MOTION_MAX && motion_set_event(t, 4, P_REV, 1) == 2;
    seq_start();
    for (i = 0; i < 4u * P16 / CTL + 2u; i++)
        seq_tick(t, CTL);
    ok &= t->seq_idx == 4u && t->p[P_REV] == 90;
    seq_stop();
    bad += check("MOTION: 128 records shared, the 128th (stored last) plays at its step; the 129th refused", ok);
    return bad;
}

/* ------------------------------------------------------------- FORMATS --- */
static int formats(void)
{
    int bad = 0, ok;
    uint32_t i, k;
    project_t a, b;
    static project_store_t st;
    static uint8_t v9[3648], v8[3584], v7[3388];
    track_t *t = seq1(1);
    t->p[P_LSYNC] = 8; t->p[P_LTRIG] = 1; t->p[P_LPOL] = 1; t->p[P_SQNT] = 0;
    trk[2].p[P_E0 + 3] = 5;
    note_step(t, 0, 60, -8);
    note_step(t, 1, 61, 7);
    note_step(t, 2, 62, -1);
    note_step(t, 3, 63, 1);
    step_set_ratchet(&t->step[3], 4);
    step_set_chance(&t->step[3], 33);
    for (i = 0; i < MOTION_MAX; i++)
        motion_set_event(&trk[i % 4u], (i / 4u) % NSTEP, i / 4u < NSTEP ? P_REV : P_PAN, (int16_t)(i / 4u < NSTEP ? i / 4u : -(int32_t)(i % 60u)));
    motion_set_lock(&trk[2], 9, P_E0 + 3, 7);
    project_capture(&a);
    ok = motion.count == MOTION_MAX && proj_pack(&st, &a) && ((uint32_t *)st.raw)[0] == 0x46554E41u &&
         ((uint32_t *)st.raw)[1] == 3840u && st.raw[66] == P_COUNT && proj_import(&b, &st, sizeof st) && !memcmp(&a, &b, sizeof a);
    ok &= step_nudge(&b.t[0].step[0]) == -8 && step_nudge(&b.t[0].step[1]) == 7 && step_nudge(&b.t[0].step[2]) == -1 &&
          step_ratchet(&b.t[0].step[3]) == 4u && step_nudge(&b.t[0].step[3]) == 1 && step_chance(&b.t[0].step[3]) == 33u;
    ok &= b.t[0].p[P_LSYNC] == 8 && b.t[0].p[P_LTRIG] == 1 && b.t[0].p[P_LPOL] == 1 && b.t[0].p[P_SQNT] == 0;
    ok &= !(st.raw[68u + P_COUNT + 2u] & 0x80u) && (st.raw[68u + P_COUNT + 2u + 3u] & 0x80u) &&   /* -8: bit 3 only */
          (st.raw[68u + P_COUNT + 2u + 9u] & 0x80u) && !(st.raw[68u + P_COUNT + 2u + 12u] & 0x80u);   /* +7: bits 0..2 */
    bad += check("FUN10 round trip: 3840 bytes, nudges (bit 7 of the note bytes), 128 records, LFO 2, QUANTIZE", ok);
    /* a FUN10 of a 1.2 build before SPREAD (#148): 103 parameters (no P_SPRD, nor what came after it); one of 1.2 ..
     * 1.4 (GHOULBOX up to 1.4.1): 104 (no Felucca 1.5's seven, no TEMP); one of Felucca 1.5: 111 (no TEMP); one of
     * GHOULBOX 2.0 .. 2.2: 112 (no RING). Their engine values and their motion (the lock on E3) as many ids lower. They
     * load by count: SPREAD 0, the seven at Felucca 1.5's defaults, TEMP EQUAL, RING OFF, the rest as written */
    for (uint32_t f = 0; f < 4u; f++) {
        static const uint32_t FROM[4] = {P_SPRD, P_ITYPE, P_TEMP, P_RING};
        static const char *const WHAT[4] = {
            "a FUN10 of 103 parameters (1.2 before SPREAD): SPREAD 0, the seven reserved, TEMP EQUAL, engine values, motion",
            "a FUN10 of 104 parameters (1.2 .. 1.4.1): the seven reserved at their defaults, TEMP EQUAL, engine values, motion",
            "a FUN10 of 111 parameters (Felucca 1.5): TEMP EQUAL, engine values and motion in place",
            "a FUN10 of 112 parameters (GHOULBOX 2.0 .. 2.2): RING OFF, engine values and motion in place"};
        static project_store_t so;
        uint32_t from = FROM[f], pi = 68u, po = 68u, tr, m0, drop = P_E0 - from;
        memset(&so, 0, sizeof so);
        ok = proj_pack(&st, &a) && !a.t[0].p[P_SPRD] && !a.t[0].p[P_ITYPE] && !a.t[0].p[P_ESYNC] && !a.t[0].p[P_TEMP] &&
             !a.t[0].p[P_RING];
        memcpy(so.raw, st.raw, 68u);
        so.raw[66] = (uint8_t)(P_COUNT - drop);
        for (tr = 0; tr < NTRK; tr++) {
            for (i = 0; i < P_COUNT; i++, pi++)
                if (i < from || i >= P_E0)
                    so.raw[po++] = st.raw[pi];
            memcpy(so.raw + po, st.raw + pi, 2u + NSTEP * 9u);
            pi += 2u + NSTEP * 9u;
            po += 2u + NSTEP * 9u;
        }
        memcpy(so.raw + po, st.raw + pi, sizeof(chain_config_t) + sizeof(motion_store_t));
        m0 = po + sizeof(chain_config_t);
        for (i = 0; i < so.raw[m0]; i++) {
            uint8_t *id = &so.raw[m0 + 4u + 3u * i + 1u];
            if ((*id & 0x7Fu) >= P_E0)
                *id = (uint8_t)(*id - drop);            /* (the lock bit kept) */
        }
        memcpy(so.raw + PROJ_FM6_OFF, st.raw + PROJ_FM6_OFF, PROJ_STORE_SIZE - 4u - PROJ_FM6_OFF);
        i = proj_hash(so.raw, PROJ_STORE_SIZE - 4u);
        memcpy(so.raw + PROJ_STORE_SIZE - 4u, &i, 4);
        ok &= so.raw[66] == (uint8_t[]){103u, 104u, 111u, 112u}[f] && proj_import(&b, &so, sizeof so) && !memcmp(&a, &b, sizeof a) &&
              b.t[2].p[P_E0 + 3] == 5 && b.t[1].p[P_IA] == 64 && b.t[1].p[P_IMIX] == 127 && b.t[1].p[P_TEMP] == 0 &&
              !b.t[1].p[P_RING];
        bad += check(WHAT[f], ok);
    }
    {   /* the song's sections (1.2): a slot per track, "-" silent, 5 bytes a row, 84 in all, right after the steps */
        project_t c = a, d;
        uint32_t at = 68u + NTRK * (P_COUNT + 2u + NSTEP * 9u);
        c.chain.count = CHAIN_ROWS;
        for (i = 0; i < CHAIN_ROWS; i++)
            c.chain.row[i] = (chain_row_t){{(uint8_t)(i % 5u), (uint8_t)((i + 1u) % 5u), (uint8_t)((i + 2u) % 5u),
                                            (uint8_t)((i * 3u) % 5u)}, (uint8_t)(1u + i)};
        c.sum = proj_sum(&c);
        ok = proj_pack(&st, &c) && proj_import(&d, &st, sizeof st) && !memcmp(&c, &d, sizeof c) && st.raw[at] == CHAIN_ROWS &&
             st.raw[at + 4u] == 0 && st.raw[at + 5u] == 1 && st.raw[at + 6u] == 2 && st.raw[at + 7u] == 0 && st.raw[at + 8u] == 1 &&
             st.raw[at + 4u + 5u * 15u + 4u] == 16;
        c.chain.row[3].slot[2] = 5;                   /* past "-": refused (pack and load) */
        c.sum = proj_sum(&c);
        ok &= !proj_pack(&st, &c);
        bad += check("FUN10 round trip: 16 sections, a slot per track (A..D, -), the bytes in place", ok);
    }
    project_save(2);
    motion_clear(t); t->p[P_SQNT] = 1; step_set_nudge(&t->step[1], 0);
    project_load(2);
    bad += check("  saved and loaded on the device: the same", step_nudge(&t->step[1]) == 7 && t->p[P_SQNT] == 0 &&
                 motion.count == MOTION_MAX && t->p[P_LSYNC] == 8);
    st.raw[68u + 4u] |= 0x80u;                       /* a nudge bit in a FUN10 note byte: fine; in a FUN9: refused */

    /* FUN9 (1.1), FUN8 (1.0.x), FUN7: as before; the four parameters at their defaults, every nudge 0 */
    memset(&a.motion, 0, sizeof a.motion);
    for (i = 0; i < 64u; i++)
        a.motion.event[i] = (motion_event_t){(uint8_t)((i % 4u) << 6 | i / 4u), (uint8_t)(i % 5u ? P_REV : P_E0 + 3), (int8_t)(i % 5u ? i : 2)};
    a.motion.count = 64; a.motion.on = 15;
    a.motion.event[7].param = P_PAN; a.motion.event[7].value = -40;
    a.motion.event[9].param |= MOTION_LOCK;
    for (k = 0; k < NTRK; k++) {
        a.t[k].p[P_LSYNC] = a.t[k].p[P_LTRIG] = a.t[k].p[P_LPOL] = 0;
        a.t[k].p[P_SQNT] = 1;
        for (i = 0; i < NSTEP; i++)
            step_set_nudge(&a.t[k].step[i], 0);
    }
    memcpy(a.name, "OLD SONG", 9);
    chain_defaults(&a.chain);                         /* a song of rows {slot, repeat} (36 bytes then) */
    a.chain.count = 3;
    a.chain.row[0] = chain_row_of(0, 4); a.chain.row[1] = chain_row_of(3, 16); a.chain.row[2] = chain_row_of(1, 1);
    a.sum = proj_sum(&a);
    ok = old_pack(v9, &a, OLD_FUN9, 99u) == sizeof v9 && proj_import(&b, v9, sizeof v9) && !memcmp(&a, &b, sizeof a);
    bad += check("FUN9 (1.1, 64 records of 4 bytes): exactly the project (LFO 2 / QUANTIZE defaults, nudges 0)", ok);
    {
        uint8_t c9[3648];
        uint32_t sum, at = 68u + NTRK * (99u + 2u + NSTEP * 9u);
        ok = b.chain.count == 3u && !memcmp(&b.chain.row[1], &(chain_row_t){{3, 3, 3, 3}, 16}, sizeof(chain_row_t)) &&
             v9[at] == 3 && v9[at + 4u] == 0 && v9[at + 5u] == 4 && v9[at + 6u] == 3 && v9[at + 7u] == 16;
        memcpy(c9, v9, sizeof c9);
        c9[at + 6u] = 4;                              /* a row on slot 4: never in a FUN9 */
        sum = proj_hash(c9, 3644u); memcpy(c9 + 3644u, &sum, 4);
        ok &= !proj_import(&b, c9, sizeof c9);
        bad += check("  its song: rows {slot, repeat} load as sections with the four tracks on the row's slot; slot 4 refused", ok);
        proj_import(&b, v9, sizeof v9);
    }
    {
        uint8_t c9[3648];
        uint32_t sum, mo = 68u + NTRK * (99u + 2u + NSTEP * 9u) + sizeof(chain_v9_t);
        memcpy(c9, v9, sizeof c9);
        c9[mo] = 65;                                  /* 65 records: never in a FUN9 */
        sum = proj_hash(c9, 3644u); memcpy(c9 + 3644u, &sum, 4);
        ok = !proj_import(&b, c9, sizeof c9);
        memcpy(c9, v9, sizeof c9);
        c9[mo + 4u + 4u * 7u + 2u] = 0x00; c9[mo + 4u + 4u * 7u + 3u] = 0x01;   /* a value of 256 */
        sum = proj_hash(c9, 3644u); memcpy(c9 + 3644u, &sum, 4);
        ok &= !proj_import(&b, c9, sizeof c9);
        memcpy(c9, v9, sizeof c9);
        c9[68u + 99u + 2u] |= 0x80u;                  /* a note byte above 127 (a FUN10's nudge bit) */
        sum = proj_hash(c9, 3644u); memcpy(c9 + 3644u, &sum, 4);
        ok &= !proj_import(&b, c9, sizeof c9);
        bad += check("  a FUN9 claiming 65 records, a value out of -64..127 or a note byte above 127: refused", ok);
    }
    for (k = 0; k < NTRK; k++)                       /* (FUN8 / FUN7: no lane levels, no locks then) */
        for (i = P_LN0; i <= P_LN7; i++)
            a.t[k].p[i] = 127;
    a.motion.event[9].param &= 0x7Fu;
    a.sum = proj_sum(&a);
    ok = old_pack(v8, &a, OLD_FUN8, 91u) == sizeof v8 && proj_import(&b, v8, sizeof v8) && !memcmp(&a, &b, sizeof a);
    bad += check("FUN8 (1.0.x, 91 parameters): exactly the project", ok);
    for (k = 0; k < NTRK; k++)
        memcpy(a.fm6[k], FM6_INIT, FM6_PACKED);
    a.g[G_RTYPE] = 1;                                /* (a FUN7 knew ROOM and SPRING: above, the old drum channel) */
    a.sum = proj_sum(&a);
    ok = old_pack(v7, &a, OLD_FUN7, 91u) == sizeof v7 && proj_import(&b, v7, sizeof v7) && !memcmp(&a, &b, sizeof a);
    bad += check("FUN7 (91 parameters): exactly the project, the init patches", ok);
    for (i = 0; i < 2u; i++) {                        /* a retained slot of 3840 holding the older records */
        static project_store_t slot;
        memset(&slot, 0xA5, sizeof slot);
        memcpy(slot.raw, i ? v8 : v9, i ? sizeof v8 : sizeof v9);
        ok = proj_import(&b, &slot, sizeof slot) && b.motion.count == 64u;
        bad += check(i ? "  a retained (longer) slot holding a FUN8: loads" : "  a retained (longer) slot holding a FUN9: loads", ok);
    }
    project_restore_runtime(&b);
    bad += check("  restored: QUANTIZE ON, LFO 2 off, every step's nudge 0, the 64 records", trk[0].p[P_SQNT] == 1 &&
                 !trk[0].p[P_LSYNC] && !step_nudge(&trk[0].step[1]) && motion.count == 64u && motion.event[7].value == -40);

    /* user presets: a record of 103 round trips; one of 99 (1.1) gets the four at their defaults; a pattern keeps no
     * nudge (bit 4 of its flags is the tie) */
    {
        up_rec_t r;
        int16_t v[P_COUNT], def[P_COUNT];
        t = seq1(1);
        t->p[P_LSYNC] = 4; t->p[P_LPOL] = 1;
        note_step(t, 0, 60, -8);                     /* (bit 4 of the flags set) */
        note_step(t, 1, 62, 3);
        memset(&r, 0, sizeof r);
        r.used = UP_USED; r.ver = UP_VER; r.engine = 0; r.np = P_COUNT;
        for (i = 0; i < P_COUNT; i++) { up_set_value(&r, i, t->p[i]); def[i] = TP[i].def; }
        up_pat_from(&r, t->step);
        up_params(&r, v, def);
        ok = v[P_LSYNC] == 4 && v[P_LPOL] == 1 && r.flags[0] == 0 && r.note[0] == 60 && r.flags[1] == 0 && r.note[1] == 62;
        r.np = 99;                                    /* as 1.1 stored it: the engine's 8 after its 91 */
        for (i = 0; i < 8u; i++) up_set_value(&r, 91u + i, (int16_t)(10 + i));
        up_params(&r, v, def);
        ok &= v[P_LSYNC] == 0 && v[P_LPOL] == 0 && v[P_SQNT] == 1 && v[P_E0] == 10 && v[P_E7] == 17 && v[P_LN7] == t->p[P_LN7];
        r.np = 103;                                   /* a 1.2 build before SPREAD: its 95, then the engine's 8 */
        for (i = 0; i < 95u; i++) up_set_value(&r, i, t->p[i]);
        for (i = 0; i < 8u; i++) up_set_value(&r, 95u + i, (int16_t)(20 + i));
        up_params(&r, v, def);
        ok &= v[P_LSYNC] == 4 && v[P_LPOL] == 1 && v[P_SPRD] == 0 && v[P_E0] == 20 && v[P_E7] == 27;
        bad += check("user presets: 104 parameters kept; 99 (1.1): LFO 2 / QUANTIZE defaults; 103: SPREAD 0; no nudge", ok);
    }
    return bad;
}

int main(void)
{
    int bad = 0;
    bad += lfo();
    bad += nudge();
    bad += motion128();
    bad += formats();
    printf(bad ? "ui: 1.2 FORMAT (FUN10) TEST FAILED\n" : "1.2 format (FUN10) tests passed\n");
    return bad != 0;
}
