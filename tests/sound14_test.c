/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of the roadmap's sound items (Discussions #132, #104, #148), same sources as the firmware (hostsim.c):
 *   build/host/sound14_test [DEMO_DIR]      (run_tests.sh builds and runs it; demos in build/sound14_demo/)
 * 1. the matrix (mod.c): the lists only grew (every stored SRC / DST keeps its name), S&H holds one value a cycle of the
 *    track LFO (the value LFO WAVE S&H plays), steps at the cycle's start, spreads over both signs; SLEW is continuous,
 *    glides from the cycle before's value to this one's, stays inside S&H's range; DEPTH scales the LFO (its LFO DEST
 *    routings and the matrix's LFO source) as AMP does a voice, several slots multiply, none = untouched (bit for bit);
 *    RATE (the LFO's rate) with them: the modulator modulated; values past the lists do nothing.
 * 2. ANALOG WAVE SYNC / SUB (eng_analog.c): WAVE 0..4 untouched (the golden renders: regress.c), the labels (SYNC, SUB),
 *    no DC, bounded, not silent; SUB an octave below (period twice the note's), SYNC periodic at the note whatever DTN,
 *    its sweep (DTN, SHP) brightens it, no restart click (largest sample step against the plain saw's).
 * 3. SPREAD (fx.c mix_spread): SPRD 0 / MONO / LEGATO bit for bit the plain mix, a POLY note hard left / the next hard
 *    right at SPRD 127, a chord's L and R differ, the mono sum's level, UNISON's voices alternate, the SLICER's gate and
 *    the mute key close the side too, the sends stay mono; the cost per part and per voice.
 * 4. demos (DEMO_DIR): S&H on the cutoff, LFO rate / depth modulation, ANALOG SUB and SYNC, SPREAD on a pad. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif

static int bad;
static void check(const char *what, int ok)
{
    printf("sound14: %-74s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static int32_t out_buf[2 * CTL];
static uint64_t hash;
static void blocks(uint32_t n)
{
    uint32_t i, k;
    while (n--) {
        mix_block(out_buf, CTL);
        for (i = 0; i < 2u * CTL; i++)
            for (k = 0; k < 4u; k++) {
                hash ^= ((uint32_t)out_buf[i] >> (8u * k)) & 0xFFu;
                hash *= 0x100000001B3ull;
            }
    }
}

static void slot(track_t *t, uint32_t k, int32_t s, int32_t d, int32_t a)
{
    t->p[P_M1SRC + 3u * k] = (int16_t)s;
    t->p[P_M1DST + 3u * k] = (int16_t)d;
    t->p[P_M1AMT + 3u * k] = (int16_t)a;
}

static uint32_t eng_by_name(const char *n)
{
    uint32_t e;
    for (e = 0; e < NENGINES; e++)
        if (str_eq(ENGINES[e]->name, n))
            return e;
    return 0;
}
static uint32_t preset_by_name(uint32_t e, const char *n)
{
    uint32_t k;
    for (k = 0; k < ENGINES[e]->npresets; k++)
        if (str_eq(ENGINES[e]->presets[k].name, n))
            return k;
    return 0;
}

static void fresh(uint32_t e, uint32_t pi)        /* the boot state (no FX tails), track 1 = engine e preset pi */
{
    uint32_t k;
    memset(dly_buf, 0, sizeof dly_buf);
    memset(cho_buf, 0, sizeof cho_buf);
    memset(rev_comb, 0, sizeof rev_comb);
    memset(rev_ap, 0, sizeof rev_ap);
    memset(&fx, 0, sizeof fx);
    memset(&pf, 0, sizeof pf);
    for (k = 0; k < NTRK; k++)
        pf.mg[k] = 32768;
    perf_held = perf_latched = 0;
    lim_env = LIM_T;
    dc_l = dc_r = dce_l = dce_r = 0;                      /* (the master's DC blockers: no residue of a run before) */
    vage = 0;                                             /* the seeds: every run the same (ANALOG's noise by age) */
    rng_state = 0x1234567u;
    mod_seed = 0x2545F491u;
    memset(trk, 0, sizeof trk);
    memset(&mod, 0, sizeof mod);
    memset(sl, 0, sizeof sl);
    host_tracks_init();
    for (k = 0; k < NPART; k++)
        host_preset(&trk[k], k ? 0u : e, k ? 0u : pi);
    song.sel = 0;
    hash = 0xCBF29CE484222325ull;
}
static void dry(track_t *t)                       /* no sends: the dry mix only */
{
    t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
}

/* the hash of a phrase on track 1 in a fork()ed child (the seeds as they were: the same in each); setup(t) first */
static uint64_t phrase_child(uint32_t e, uint32_t pi, void (*setup)(track_t *t))
{
    int fd[2];
    uint64_t h = 0;
    pid_t pid;
    if (pipe(fd))
        return 0;
    fflush(stdout);
    if (!(pid = fork())) {
        track_t *t = &trk[0];
        fresh(e, pi);
        if (setup)
            setup(t);
        trk_note_on(t, 60, 100);
        trk_note_on(t, 64, 70);
        trk_note_on(t, 67, 120);
        blocks(FS / 2u / CTL);
        trk_note_on(t, 72, 90);
        blocks(FS / 4u / CTL);
        trk_note_off(t, 60);
        trk_note_off(t, 64);
        trk_note_off(t, 67);
        trk_note_off(t, 72);
        blocks(FS / CTL);
        h = hash;
        if (write(fd[1], &h, sizeof h) != sizeof h)
            _exit(1);
        _exit(0);
    }
    close(fd[1]);
    if (read(fd[0], &h, sizeof h) != sizeof h)
        h = 0;
    close(fd[0]);
    waitpid(pid, 0, 0);
    return h;
}

/* ------------------------------------------------------------------ 1 --- */
static void test_matrix(void)
{
    static const char *const SRC_V12[] = {"OFF", "LFO", "ENV", "VEL", "KEY", "RAND", "MODW", "AT", "EXPR"};
    static const char *const DST_V12[] = {"OFF", "PITCH", "CUT", "SHP", "AMP", "PAN", "DIST", "CHO", "DLY", "REV", "RATE",
                                          "VIB", "E1", "E2", "E3", "E4", "E5", "E6", "E7", "E8"};
    track_t *t = &trk[0];
    uint32_t i, k, steps = 0, changes_in_cycle = 0, pos = 0, neg = 0, same_as_wave = 1;
    int32_t prev_sh, max_slew_step = 0, max_sh_step = 0, prev_slew, lo = 0, hi = 0, slew_ok = 1, wrap_ok = 1;
    int ok = 1;

    for (i = 0; i < NELEM(SRC_V12); i++)
        ok &= str_eq(N_MSRC[i], SRC_V12[i]);
    for (i = 0; i < NELEM(DST_V12); i++)
        ok &= str_eq(N_MDST[i], DST_V12[i]);
    check("SRC / DST lists only grew: every stored value keeps its name (S&H SLEW, DEPTH appended)",
          ok && MS_SH == 9 && MS_SLEW == 10 && MS_N == 11 && MD_DEPTH == 20 && MD_N == 21 &&
          str_eq(N_MSRC[MS_SH], "S&H") && str_eq(N_MSRC[MS_SLEW], "SLEW") && str_eq(N_MDST[MD_DEPTH], "DEPTH") &&
          TP[P_M1SRC].max == MS_N - 1 && TP[P_M1DST].max == MD_N - 1);
    check("DST names: DEPTH is not an engine parameter (E1..E8 only)", str_eq(mod_dst_name(t, MD_DEPTH), "DEPTH") &&
          !MD_ENGINE(MD_DEPTH) && MD_ENGINE(MD_E1 + 7));

    /* S&H and SLEW over a few LFO cycles: one value per cycle, the steps at the wraps; SLEW continuous */
    fresh(0, 0);
    t->p[P_LRATE] = 90;                                   /* a few Hz */
    t->p[P_LWAVE] = 4;                                    /* S&H: the LFO's own value is the same random */
    trk_note_on(t, 60, 100);
    blocks(1);
    prev_sh = mod_sh(t);
    prev_slew = mod_tsrc(t, MS_SLEW, 0);
    for (k = 0; k < 4u * FS / CTL; k++) {
        uint32_t ph0 = t->lfo_ph;
        int32_t sh, sl;
        blocks(1);
        sh = mod_sh(t);
        sl = mod_tsrc(t, MS_SLEW, 0);
        if (t->lfo_ph < ph0) {                            /* a new cycle */
            steps++;
            wrap_ok &= t->lfo_prev == prev_sh;            /* SLEW starts from the old value */
            if (sh != prev_sh) {
                int32_t d = sh - prev_sh;
                d = d < 0 ? -d : d;
                max_sh_step = d > max_sh_step ? d : max_sh_step;
            }
        } else {
            changes_in_cycle += sh != prev_sh;
        }
        if (t->lfo_rnd)
            same_as_wave &= sh == t->lfo_val;             /* (WAVE S&H, POL BI) */
        pos += sh > 8000;
        neg += sh < -8000;
        {
            int32_t d = sl - prev_slew;
            d = d < 0 ? -d : d;
            max_slew_step = d > max_slew_step ? d : max_slew_step;
        }
        lo = sl < lo ? sl : lo;
        hi = sl > hi ? sl : hi;
        slew_ok &= sl >= -32768 && sl <= 32767;
        prev_sh = sh;
        prev_slew = sl;
    }
    check("S&H: one value per LFO cycle (none changes inside one), the LFO's S&H value", steps >= 8u &&
          !changes_in_cycle && same_as_wave);
    printf("sound14:   %u cycles in 4 s; S&H steps up to %d, SLEW moves at most %d a block (%.1f %% of S&H's)\n",
           steps, max_sh_step, max_slew_step, 100.0 * max_slew_step / (max_sh_step ? max_sh_step : 1));
    check("S&H: values on both sides (bipolar)", pos > 0u && neg > 0u);
    check("SLEW: continuous (a block's move small against S&H's steps), inside S&H's range, from the cycle before's value",
          slew_ok && wrap_ok && max_slew_step * 8 < max_sh_step && hi > 0 && lo < 0);

    /* S&H follows RATE: a faster LFO steps more often */
    {
        uint32_t s1 = 0, s2 = 0;
        fresh(0, 0);
        t->p[P_LRATE] = 70;
        for (k = 0; k < 2u * FS / CTL; k++) {
            uint32_t ph0 = t->lfo_ph;
            blocks(1);
            s1 += t->lfo_ph < ph0;
        }
        fresh(0, 0);
        t->p[P_LRATE] = 70;
        slot(t, 0, MS_MODW, MD_RATE, 63);                 /* the matrix's RATE: S&H with it */
        t->mw = 127;
        for (k = 0; k < 2u * FS / CTL; k++) {
            uint32_t ph0 = t->lfo_ph;
            blocks(1);
            s2 += t->lfo_ph < ph0;
        }
        check("S&H: the matrix's RATE (MODW -> RATE) makes it step faster", s2 > s1 * 2u && s1 > 0u);
    }

    /* DEPTH: the LFO's gain, as AMP's */
    fresh(0, 0);
    t->lfo_val = 32767;
    t->lfo_fade = 32767;
    slot(t, 0, MS_MODW, MD_DEPTH, 63);
    t->mw = 0;
    mod_begin(t);
    check("MODW -> DEPTH +63: the wheel down closes the LFO (gain 1.6 %)", mod.on && mod.ldep >= 500 && mod.ldep <= 512);
    mod_end(t);
    t->mw = 127;
    mod_begin(t);
    check("MODW -> DEPTH +63: the wheel up leaves it whole", mod.ldep >= 32700);
    mod_end(t);
    slot(t, 0, MS_MODW, MD_DEPTH, -64);
    mod_begin(t);
    check("MODW -> DEPTH -64: the wheel up closes it", mod.ldep == 0);
    mod_end(t);
    slot(t, 0, MS_MODW, MD_DEPTH, 63);
    slot(t, 1, MS_LFO, MD_CUT, 63);
    t->mw = 0;
    mod_begin(t);
    check("DEPTH scales the matrix's LFO source too (LFO -> CUT with the wheel down: ~1.6 %)",
          mod.cut > 0 && mod.cut < ((32767 * 63) >> 7) / 40);
    mod_end(t);
    slot(t, 2, MS_AT, MD_DEPTH, 63);
    t->mw = 127;
    t->at = 64;
    mod_begin(t);
    check("two DEPTH slots multiply (wheel up x AT half)", mod.ldep > 16000 && mod.ldep < 17000);
    mod_end(t);
    slot(t, 0, MS_LFO, MD_DEPTH, 63);                     /* the LFO on its own depth: the unscaled LFO */
    slot(t, 1, 0, 0, 0);
    slot(t, 2, 0, 0, 0);
    mod_begin(t);
    check("LFO -> DEPTH: takes the LFO unscaled (at its top: whole)", mod.ldep >= 32700);
    mod_end(t);
    slot(t, 0, MS_SH, MD_CUT, 40);                        /* no DEPTH slot: the LFO untouched */
    mod_begin(t);
    check("no DEPTH slot: the LFO untouched (track_render leaves it alone)", mod.ldep == 32767);
    mod_end(t);
    slot(t, 0, 11, MD_CUT, 40);                           /* past the lists: nothing */
    slot(t, 1, MS_LFO, MD_N, 40);
    mod_begin(t);
    check("a SRC / DST past the lists does nothing", mod.cut == 0 && mod.nk == 0 && mod.nv == 0);
    mod_end(t);
}

/* DEPTH in the sound: an LFO DEST PIT vibrato with MODW -> DEPTH -64 (whole with the wheel down, closed with it up) */
static void vib_plain(track_t *t)
{
    t->p[P_LRATE] = 80;
    t->p[P_LD_PIT] = 0;
}
static void vib_depth0(track_t *t)                /* the wheel up: DEPTH 0, no vibrato */
{
    t->p[P_LRATE] = 80;
    t->p[P_LD_PIT] = 40;
    slot(t, 0, MS_MODW, MD_DEPTH, -64);
    t->mw = 127;
}
static void vib_depth1(track_t *t)
{
    vib_depth0(t);
    t->mw = 0;
}
static void vib_full(track_t *t)
{
    t->p[P_LRATE] = 80;
    t->p[P_LD_PIT] = 40;
}
static void sh_on_nothing(track_t *t)             /* S&H and SLEW slots with AMT 0: nothing */
{
    slot(t, 0, MS_SH, MD_CUT, 0);
    slot(t, 1, MS_SLEW, MD_PITCH, 0);
    slot(t, 2, MS_LFO, MD_DEPTH, 0);
}
static void test_matrix_sound(void)
{
    uint64_t h0 = phrase_child(0, 0, 0), hn = phrase_child(0, 0, sh_on_nothing);
    uint64_t p0 = phrase_child(0, 0, vib_plain), d0 = phrase_child(0, 0, vib_depth0);
    uint64_t d1 = phrase_child(0, 0, vib_depth1), f1 = phrase_child(0, 0, vib_full);
    check("S&H / SLEW / DEPTH slots at AMT 0: bit for bit no matrix", h0 && h0 == hn);
    check("DEPTH closed: LFO DEST PIT plays nothing (bit for bit LFO DEST PIT 0)", p0 && p0 == d0);
    check("DEPTH open: the vibrato is there (not the plain note), as without the matrix but for the gain",
          d1 != p0 && f1 != p0);
}

/* ------------------------------------------------------------------ 2 --- */
#define NREND (FS / CTL)
static int32_t ren_l[NREND * CTL];

/* one note of track 1 (ANALOG, preset pi, WAVE w, DTN d, MIX mx) for NREND blocks; ren_l: the left channel */
static void analog_note(uint32_t pi, int32_t w, int32_t d, int32_t mx, int32_t shp_env, uint32_t note)
{
    track_t *t = &trk[0];
    uint32_t k, i;
    fresh(0, pi);
    dry(t);
    t->p[P_VOICE] = V_POLY;
    t->p[P_E0] = (int16_t)w;
    t->p[P_E1] = (int16_t)d;
    t->p[P_E2] = (int16_t)mx;
    t->p[P_E4] = 127;                                     /* open: the oscillators */
    t->p[P_E5] = 0;
    t->p[P_E3] = 0;
    t->p[P_E6] = 0;
    t->p[P_ED_FLT] = 0;
    t->p[P_ED_SHP] = (int16_t)shp_env;
    t->p[P_ATK] = 0;
    t->p[P_SUS] = 127;
    trk_note_on(t, note, 100);
    for (k = 0; k < NREND; k++) {
        blocks(1);
        for (i = 0; i < CTL; i++)
            ren_l[k * CTL + i] = out_buf[2u * i];
    }
    trk_note_off(t, note);
    blocks(FS / CTL);
}
typedef struct { double mean, rms, peak, maxstep, hf; } stats_t;
static stats_t stats(uint32_t from)
{
    stats_t s = {0};
    uint32_t i, n = NREND * CTL - from;
    for (i = from; i < NREND * CTL; i++) {
        double x = ren_l[i], d = i > from ? x - ren_l[i - 1] : 0;
        s.mean += x;
        s.rms += x * x;
        s.peak = fabs(x) > s.peak ? fabs(x) : s.peak;
        s.maxstep = fabs(d) > s.maxstep ? fabs(d) : s.maxstep;
        s.hf += d * d;
    }
    s.mean /= n;
    s.rms = sqrt(s.rms / n);
    s.hf = sqrt(s.hf / n) / (s.rms + 1e-9);               /* the first difference's RMS against the signal's: brightness */
    return s;
}
static double autocorr(uint32_t from, uint32_t lag)   /* normalised, over the steady part */
{
    double a = 0, b = 0, c = 0;
    uint32_t i;
    for (i = from; i + lag < NREND * CTL; i++) {
        a += (double)ren_l[i] * ren_l[i + lag];
        b += (double)ren_l[i] * ren_l[i];
        c += (double)ren_l[i + lag] * ren_l[i + lag];
    }
    return a / sqrt(b * c + 1e-9);
}
static uint32_t best_lag(uint32_t from, uint32_t lo, uint32_t hi)
{
    uint32_t l, best = lo;
    double bv = -2;
    for (l = lo; l <= hi; l++) {
        double v = autocorr(from, l);
        if (v > bv) {
            bv = v;
            best = l;
        }
    }
    return best;
}

static void test_analog(void)
{
    const engine_t *e = ENGINES[0];
    track_t *t = &trk[0];
    stats_t saw, sub, sync0, sync40, syncs;
    double per = FS / 220.0;                              /* A3 (57) */
    uint32_t from = FS / 10u, lag;
    char msg[160];

    check("ANALOG WAVE: SAW .. PWM where they were, SYNC 5 SUB 6 appended, default SAW",
          e->edit[0].max == 6 && e->edit[0].def == 0 && str_eq(e->edit[0].names[4], "PWM") &&
          str_eq(e->edit[0].names[5], "SYNC") && str_eq(e->edit[0].names[6], "SUB"));
    fresh(0, 0);
    t->p[P_E0] = 5;
    check("labels: SYNC shows DTN as SYNC, SUB MIX as SUB; the others as before",
          str_eq(track_desc(t, P_E1)->label, "SYNC") && str_eq(track_desc(t, P_E2)->label, "MIX") &&
          track_desc(t, P_E1)->min == 0 && track_desc(t, P_E1)->max == 127);
    t->p[P_E0] = 6;
    t->eng_req = 0;
    check("  .. SUB", str_eq(track_desc(t, P_E2)->label, "SUB") && str_eq(track_desc(t, P_E1)->label, "DTN"));
    t->p[P_E0] = 0;
    check("  .. SAW: DTN MIX", str_eq(track_desc(t, P_E1)->label, "DTN") && str_eq(track_desc(t, P_E2)->label, "MIX"));

    analog_note(0, 0, 0, 0, 0, 57);
    saw = stats(from);
    analog_note(0, 6, 0, 127, 0, 57);                     /* SUB alone */
    sub = stats(from);
    lag = best_lag(from, (uint32_t)(per * 1.5), (uint32_t)(per * 2.5));
    snprintf(msg, sizeof msg, "SUB: an octave below (period %u samples, the note's %.1f), no DC, bounded", lag, per);
    check(msg, fabs(lag - 2 * per) < 2.0 && autocorr(from, lag) > 0.95 && fabs(sub.mean) < sub.rms * 0.02 &&
          sub.peak < 32000 && sub.rms > saw.rms * 0.3);
    analog_note(0, 6, 0, 64, 0, 57);                      /* SUB half */
    {
        stats_t s = stats(from);
        check("SUB 50 %: saw + sub, bounded, no DC", fabs(s.mean) < s.rms * 0.02 && s.peak < 32000 && s.rms > 100);
    }
    analog_note(0, 5, 0, 127, 0, 57);                     /* SYNC alone, DTN 0: OSC 2 at OSC 1's pitch */
    sync0 = stats(from);
    analog_note(0, 5, 40, 127, 0, 57);                    /* SYNC alone, DTN 40: +7.5 semitones, synced */
    sync40 = stats(from);
    lag = best_lag(from, (uint32_t)(per * 0.5), (uint32_t)(per * 1.5));
    snprintf(msg, sizeof msg, "SYNC: periodic at the note whatever DTN (period %u, the note's %.1f; r %.3f)", lag, per,
             autocorr(from, lag));
    check(msg, fabs(lag - per) < 1.5 && autocorr(from, lag) > 0.97);
    snprintf(msg, sizeof msg, "SYNC: DTN 0 ~ the saw (rms %.0f vs %.0f), DTN 40 brighter (%.3f vs %.3f)", sync0.rms,
             saw.rms, sync40.hf, sync0.hf);
    check(msg, fabs(sync0.rms / saw.rms - 1) < 0.05 && sync40.hf > sync0.hf * 1.1);
    snprintf(msg, sizeof msg, "SYNC: no DC, bounded, the restart band-limited (largest step %.0f vs the saw's %.0f)",
             sync40.maxstep, saw.maxstep);
    check(msg, fabs(sync40.mean) < sync40.rms * 0.03 && sync40.peak < 32000 && sync40.maxstep < saw.maxstep * 1.3);
    analog_note(0, 5, 10, 127, 63, 57);                   /* ENV -> SHP: the sync sweep */
    syncs = stats(from);
    check("SYNC: ENV DEST SHP sweeps it (other than without), bounded", syncs.peak < 32000 &&
          fabs(syncs.hf - sync0.hf) > 0.01);
    /* the corners: low and high notes, DTN 127, drive and resonance: bounded, no DC */
    {
        static const uint8_t NOTE[3] = {24, 60, 108};
        uint32_t w, n, okc = 1;
        double worst = 0;
        for (w = 5; w <= 6; w++)
            for (n = 0; n < 3u; n++) {
                stats_t s;
                analog_note(0, (int32_t)w, 127, 100, 63, NOTE[n]);
                s = stats(from);
                okc &= s.peak < 32767 && fabs(s.mean) < s.rms * 0.05 + 30 && s.rms > 10;
                worst = fabs(s.mean) > worst ? fabs(s.mean) : worst;
            }
        snprintf(msg, sizeof msg, "SYNC / SUB at the corners (notes 24 60 108, DTN 127, SHP env): bounded, no DC (|mean| <= %.0f)",
                 worst);
        check(msg, okc);
    }
}

/* ------------------------------------------------------------------ 3 --- */
static void sp_mono127(track_t *t) { t->p[P_VOICE] = V_MONO; t->p[P_SPRD] = 127; }
static void sp_mono0(track_t *t) { t->p[P_VOICE] = V_MONO; }
static void sp_leg127(track_t *t) { t->p[P_VOICE] = V_LEGATO; t->p[P_SPRD] = 127; }
static void sp_leg0(track_t *t) { t->p[P_VOICE] = V_LEGATO; }
static void sp_poly0(track_t *t) { t->p[P_VOICE] = V_POLY; t->sp_alt = 1; }   /* (the sides change nothing at 0) */
static void sp_poly0b(track_t *t) { t->p[P_VOICE] = V_POLY; }

/* track 1 (ANALOG SOFT PAD, dry, POLY), SPRD sp, PAN pan: notes, then blocks; L and R sums of squares, the mono sum's */
typedef struct { double l, r, m, lr; int32_t rmax, lmax; } lr_t;
static lr_t spread_run(int32_t sp, int32_t pan, uint32_t mode, const uint8_t *notes, uint32_t nn, uint32_t nb)
{
    track_t *t = &trk[0];
    lr_t a = {0};
    uint32_t k, i;
    fresh(0, preset_by_name(0, "SOFT PAD"));
    dry(t);
    song.master_q12 = 1024;                               /* below the limiter: the levels as mixed */
    t->p[P_VOICE] = (int16_t)mode;
    t->p[P_SPRD] = (int16_t)sp;
    t->p[P_PAN] = (int16_t)pan;
    t->p[P_ATK] = 0;
    for (i = 0; i < nn; i++)
        trk_note_on(t, notes[i], 100);
    for (k = 0; k < nb; k++) {
        blocks(1);
        for (i = 0; i < CTL; i++) {
            double l = out_buf[2u * i], r = out_buf[2u * i + 1u];
            a.l += l * l;
            a.r += r * r;
            a.m += (l + r) * (l + r);
            a.lr += (l - r) * (l - r);
            a.lmax = abs(out_buf[2u * i]) > a.lmax ? abs(out_buf[2u * i]) : a.lmax;
            a.rmax = abs(out_buf[2u * i + 1u]) > a.rmax ? abs(out_buf[2u * i + 1u]) : a.rmax;
        }
    }
    return a;
}

static void test_spread(void)
{
    static const uint8_t ONE[1] = {60}, TWO[2] = {60, 67}, CHORD[4] = {48, 55, 60, 64};
    lr_t a, b, c;
    uint64_t h1, h2;
    char msg[200];

    check("SPRD: a track parameter 0..127, default 0 (as before), at 95 (GHOULBOX: Felucca 1.5's seven, TEMP after it)",
          TP[P_SPRD].min == 0 && TP[P_SPRD].max == 127 && TP[P_SPRD].def == 0 && P_SPRD == 95 && P_SPRD + 1 == P_ITYPE &&
          P_E0 == 105 && P_COUNT == 113 && str_eq(TP[P_SPRD].label, "SPRD") && motion_param(P_SPRD));
    h1 = phrase_child(0, 1, sp_mono0);
    h2 = phrase_child(0, 1, sp_mono127);
    check("MONO: SPRD 127 plays bit for bit as SPRD 0 (one voice: on PAN)", h1 && h1 == h2);
    h1 = phrase_child(0, 1, sp_leg0);
    h2 = phrase_child(0, 1, sp_leg127);
    check("LEGATO: SPRD 127 plays bit for bit as SPRD 0", h1 && h1 == h2);
    h1 = phrase_child(0, 1, sp_poly0);
    h2 = phrase_child(0, 1, sp_poly0b);
    check("POLY SPRD 0: the voices' sides change nothing (the plain mix, bit for bit)", h1 && h1 == h2);

    a = spread_run(0, 0, V_POLY, ONE, 1, NREND / 2);
    check("SPRD 0, PAN 0: L = R (as before)", a.lr == 0 && a.l > 0);
    b = spread_run(127, 0, V_POLY, ONE, 1, NREND / 2);
    snprintf(msg, sizeof msg, "SPRD 127: the first note hard left (R peak %d of L's %d), at its level (L %.3f of SPRD 0's)",
             b.rmax, b.lmax, sqrt(b.l / a.l));
    check(msg, b.rmax <= 2 && fabs(sqrt(b.l / a.l) - 1) < 0.01);
    b = spread_run(127, 0, V_POLY, TWO, 2, NREND / 2);
    {
        lr_t r1 = spread_run(127, 0, V_POLY, TWO + 1, 1, NREND / 2);   /* (67 alone: left, the first) */
        snprintf(msg, sizeof msg, "SPRD 127: the second note hard right (L holds the first only: %.3f)", sqrt(b.l / r1.l));
        (void)r1;
        check(msg, b.r > 0 && b.l > 0 && b.rmax > 1000 && b.lmax > 1000);
    }
    {   /* the sides: alternate per new voice; a sounding voice retriggered keeps its side (no jump) */
        track_t *t = &trk[0];
        uint32_t k, sides = 0, same = 1;
        spread_run(127, 0, V_POLY, CHORD, 4, 4);
        for (k = 0; k < NVOICE; k++)
            if (t->v[k].active)
                sides |= 1u << t->v[k].side;
        for (k = 0; k < NVOICE; k++)
            if (t->v[k].active && t->v[k].note == 60) {
                uint32_t s0 = t->v[k].side;
                trk_note_on(t, 60, 100);
                blocks(2);
                same = t->v[k].active && t->v[k].note == 60 && t->v[k].side == s0;
            }
        check("the voices of a chord on both sides; a voice retriggered while it sounds keeps its side", sides == 3u && same);
    }
    a = spread_run(0, 0, V_POLY, CHORD, 4, NREND);
    b = spread_run(64, 0, V_POLY, CHORD, 4, NREND);
    c = spread_run(127, 0, V_POLY, CHORD, 4, NREND);
    snprintf(msg, sizeof msg, "a chord: L and R differ with SPRD (side/mid %.3f at 64, %.3f at 127; 0 at 0)",
             sqrt(b.lr / b.m), sqrt(c.lr / c.m));
    check(msg, a.lr == 0 && b.lr > 0 && c.lr > b.lr);
    snprintf(msg, sizeof msg, "the mono sum: its level %.3f of SPRD 0's at 64 (pan law: 1 - 32/128 = 0.75), %.3f at 127 (0.5)",
             sqrt(b.m / a.m), sqrt(c.m / a.m));
    check(msg, fabs(sqrt(b.m / a.m) - 0.75) < 0.03 && fabs(sqrt(c.m / a.m) - 0.5) < 0.03);
    b = spread_run(64, 40, V_POLY, CHORD, 4, NREND);
    a = spread_run(0, 40, V_POLY, CHORD, 4, NREND);
    check("around PAN: PAN +40 with SPRD 64 leans right as PAN alone does", b.r > b.l && a.r > a.l);
    a = spread_run(0, 0, V_UNISON, ONE, 1, NREND);
    b = spread_run(100, 0, V_UNISON, ONE, 1, NREND);
    snprintf(msg, sizeof msg, "UNISON: the detuned voices alternate L / R (side/mid %.3f at SPRD 100, 0 at 0)", sqrt(b.lr / b.m));
    check(msg, a.lr == 0 && b.lr > b.m * 0.01);
}

/* the SLICER's gate and the mute key close the side too; the sends stay mono */
static void test_spread_fx(void)
{
    static const uint8_t CHORD[4] = {48, 55, 60, 64};
    track_t *t = &trk[0];
    uint32_t k, i, closed = 0;
    double side_closed = 0, side_open = 0;
    char msg[160];
    /* SLICER GATE, pattern 1 (x.x.), DEPTH 100 %: in the closed steps L and R both near silent */
    fresh(0, preset_by_name(0, "SOFT PAD"));
    dry(t);
    t->p[P_ATK] = 0;
    t->p[P_SPRD] = 127;
    t->p[P_SLCR] = SL_GATE;
    t->p[P_SLPAT] = 1;
    t->p[P_SLDEPTH] = 127;
    slicer_start();
    for (i = 0; i < 4u; i++)
        trk_note_on(t, CHORD[i], 100);
    blocks(FS / 4u / CTL);
    for (k = 0; k < FS / CTL; k++) {
        double s = 0;
        blocks(1);
        for (i = 0; i < CTL; i++) {
            double d = (double)out_buf[2u * i] - out_buf[2u * i + 1u];
            s += d * d;
        }
        if (sl[0].gc >= 32000 && !sl[0].bit) {
            side_closed += s;
            closed++;
        } else if (!sl[0].gc) {
            side_open += s;
        }
    }
    snprintf(msg, sizeof msg, "SLICER GATE: the side closes with the gate (L - R energy closed / open %.2e, %u blocks)",
             side_closed / (side_open + 1), closed);
    check(msg, closed > 10u && side_closed < side_open * 1e-4);
    /* the mute key (FX layer, black key 1): both sides ramp to silence. What is left after it is the master's DC
     * blockers settling: as much at SPRD 127 as at 0, and L - R no more than that (the side muted with the mid) */
    {
        double side[2][2] = {{0}}, all[2][2] = {{0}};
        uint32_t ph, r;
        for (r = 0; r < 2u; r++) {
            fresh(0, preset_by_name(0, "SOFT PAD"));
            dry(t);
            t->p[P_ATK] = 0;
            t->p[P_SPRD] = (int16_t)(r ? 127 : 0);
            for (i = 0; i < 4u; i++)
                trk_note_on(t, CHORD[i], 100);
            for (ph = 0; ph < 2u; ph++) {
                if (ph)
                    perf_held |= 1u << PF_M1;
                blocks(FS / 8u / CTL);
                for (k = 0; k < 8u; k++) {
                    blocks(1);
                    for (i = 0; i < CTL; i++) {
                        side[r][ph] += fabs((double)out_buf[2u * i] - out_buf[2u * i + 1u]);
                        all[r][ph] += fabs((double)out_buf[2u * i]) + fabs((double)out_buf[2u * i + 1u]);
                    }
                }
            }
            perf_held = 0;
        }
        snprintf(msg, sizeof msg, "the mute key: L and R both go (SPRD 127: left %.1e, L - R %.1e of before; SPRD 0: %.1e)",
                 all[1][1] / all[1][0], side[1][1] / side[1][0], all[0][1] / all[0][0]);
        check(msg, side[1][0] > 0 && all[1][1] / all[1][0] < 2 * all[0][1] / all[0][0] + 1e-3 &&
              side[1][1] <= all[1][1] && all[0][1] < all[0][0] * 0.05);
    }
    /* the sends: mono (a chord with SPRD 127 and only REV: what the wet bus gets is the same) */
    {
        uint64_t hw[2];
        uint32_t r;
        for (r = 0; r < 2u; r++) {
            fresh(0, preset_by_name(0, "SOFT PAD"));
            dry(t);
            t->p[P_REV] = 100;
            t->p[P_SPRD] = (int16_t)(r ? 127 : 0);
            for (i = 0; i < 4u; i++)
                trk_note_on(t, CHORD[i], 100);
            blocks(FS / 4u / CTL);
            uint64_t hs = 0xCBF29CE484222325ull;   /* (blocks() hashes the output into hash: its own) */
            for (k = 0; k < FS / 4u / CTL; k++) {
                blocks(1);
                for (i = 0; i < CTL; i++) {
                    hs ^= (uint32_t)send_r[i];
                    hs *= 0x100000001B3ull;
                }
            }
            hw[r] = hs;
        }
        check("the sends take the mono mix: REV's send the same at SPRD 127 as at 0", hw[0] == hw[1]);
    }
}

/* ------------------------------------------------------------------ cost --- */
static uint64_t instr_now(void)
{
#ifdef __APPLE__
    struct rusage_info_v4 ri;
    if (!proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri))
        return ri.ri_instructions;
#endif
    return 0;
}
/* instructions per sample of ANALOG SOFT PAD POLY with nv notes, SPRD sp; or matrix slots (S&H -> CUT, SLEW -> PITCH,
 * MODW -> DEPTH, ENV -> RATE) with mx */
static double cost(uint32_t nv, int32_t sp, int mx, uint32_t wave)
{
    static const uint8_t NOTES[8] = {48, 52, 55, 59, 60, 64, 67, 71};
    track_t *t = &trk[0];
    uint32_t i, rep;
    double best = 1e30;
    fresh(0, preset_by_name(0, "SOFT PAD"));
    t->p[P_VOICE] = V_POLY;
    t->p[P_SUS] = 127;
    t->p[P_SPRD] = (int16_t)sp;
    t->p[P_E0] = (int16_t)wave;
    if (mx) {
        slot(t, 0, MS_SH, MD_CUT, 30);
        slot(t, 1, MS_SLEW, MD_PITCH, 4);
        slot(t, 2, MS_MODW, MD_DEPTH, 40);
        slot(t, 3, MS_ENV, MD_RATE, 20);
        t->mw = 90;
        t->p[P_LD_FLT] = 20;
    }
    for (i = 0; i < nv; i++)
        trk_note_on(t, NOTES[i], 100);
    blocks(FS / 4u / CTL);
    for (rep = 0; rep < 3u; rep++) {
        uint64_t i0 = instr_now();
        double ipc;
        blocks(FS / 2u / CTL);
        ipc = (double)(instr_now() - i0) / (FS / 2u);
        best = ipc < best ? ipc : best;
    }
    return best;
}
static void test_cost(void)
{
    double c1, c1s, c8, c8s, m0, m1, a0, a5, a6;
    if (!instr_now()) {
        printf("sound14: cost: no instruction counter on this host (proc_pid_rusage), skipped\n");
        return;
    }
    c1 = cost(1, 0, 0, 0);
    c1s = cost(1, 64, 0, 0);
    c8 = cost(8, 0, 0, 0);
    c8s = cost(8, 64, 0, 0);
    printf("sound14: cost: SPREAD, ANALOG SOFT PAD POLY: 1 voice %.0f -> %.0f (+%.0f), 8 voices %.0f -> %.0f (+%.0f) "
           "instructions / sample: per part +%.0f, per voice %+.1f\n", c1, c1s, c1s - c1, c8, c8s, c8s - c8,
           c1s - c1, ((c8s - c8) - (c1s - c1)) / 7.0);
    check("cost: SPREAD a fixed cost per part (< 3 % of an 8-voice part), next to nothing per voice",
          (c8s - c8) < c8 * 0.03 && ((c8s - c8) - (c1s - c1)) / 7.0 < 2.0);
    m0 = cost(8, 0, 0, 0);
    m1 = cost(8, 0, 1, 0);
    printf("sound14: cost: matrix S&H -> CUT, SLEW -> PITCH, MODW -> DEPTH, ENV -> RATE on 8 voices: %.0f -> %.0f (+%.1f %%)\n",
           m0, m1, (m1 - m0) * 100 / m0);
    check("cost: the new sources / DEPTH within the matrix's allowance (+5 %)", (m1 - m0) * 100 / m0 <= 5.0);
    a0 = cost(8, 0, 0, 0);
    a5 = cost(8, 0, 0, 5);
    a6 = cost(8, 0, 0, 6);
    printf("sound14: cost: ANALOG 8 voices SAW %.0f, SYNC %.0f (%+.1f %%), SUB %.0f (%+.1f %%) instructions / sample\n", a0, a5,
           (a5 - a0) * 100 / a0, a6, (a6 - a0) * 100 / a0);
    check("cost: SYNC / SUB within 25 % of SAW (regress.c's CPU allowance)", a5 < a0 * 1.25 && a6 < a0 * 1.25);
}

/* ------------------------------------------------------------------ 4 --- */
static FILE *demo_open(const char *dir, const char *name, uint32_t frames)
{
    char path[512];
    FILE *f;
    snprintf(path, sizeof path, "%s/%s.wav", dir, name);
    f = fopen(path, "wb");
    if (f)
        wav_hdr(f, frames);
    return f;
}
static void demo_blocks(FILE *f, uint32_t n)
{
    uint32_t i;
    while (n--) {
        blocks(1);
        for (i = 0; i < CTL; i++)
            wav_put(f, out_buf[2u * i], out_buf[2u * i + 1u]);
    }
}
static void chord_on(track_t *t, const uint8_t *n, uint32_t k)
{
    while (k--)
        trk_note_on(t, n[k], 100);
}
static void chord_off(track_t *t, const uint8_t *n, uint32_t k)
{
    while (k--)
        trk_note_off(t, n[k]);
}

static int demos(const char *dir)
{
    static const uint8_t PAD[4] = {48, 55, 60, 64}, PAD2[4] = {45, 52, 57, 60};
    uint32_t n = 0, k;
    FILE *f;
    track_t *t = &trk[0];
    /* S&H -> CUT: ANALOG ACID, a held note, the cutoff jumps once per LFO cycle (1/16 at 120 BPM: SYNC) */
    fresh(0, preset_by_name(0, "SAW LEAD"));
    t->p[P_VOICE] = V_POLY;
    t->p[P_E4] = 50;
    t->p[P_E5] = 90;
    t->p[P_LSYNC] = 8;                                    /* 1/16 */
    slot(t, 0, MS_SH, MD_CUT, 63);
    if ((f = demo_open(dir, "sh_cutoff", 6u * FS / CTL * CTL))) {
        chord_on(t, PAD, 2);
        demo_blocks(f, 5u * FS / CTL);
        chord_off(t, PAD, 2);
        demo_blocks(f, FS / CTL);
        fclose(f);
        n++;
    }
    /* SLEW -> CUT, the same: the smooth random line */
    fresh(0, preset_by_name(0, "SAW LEAD"));
    t->p[P_VOICE] = V_POLY;
    t->p[P_E4] = 50;
    t->p[P_E5] = 90;
    t->p[P_LSYNC] = 6;                                    /* 1/8 */
    slot(t, 0, MS_SLEW, MD_CUT, 63);
    if ((f = demo_open(dir, "slew_cutoff", 6u * FS / CTL * CTL))) {
        chord_on(t, PAD, 2);
        demo_blocks(f, 5u * FS / CTL);
        chord_off(t, PAD, 2);
        demo_blocks(f, FS / CTL);
        fclose(f);
        n++;
    }
    /* LFO rate and depth modulated: LFO DEST PIT vibrato; ENV -> RATE speeds it up as the note decays .. no: MODW
     * rises over 4 s: MODW -> DEPTH opens the vibrato, MODW -> RATE speeds it up (a mod-wheel vibrato that quickens) */
    fresh(0, preset_by_name(0, "SINE KEY"));
    t->p[P_VOICE] = V_POLY;
    t->p[P_SUS] = 127;
    t->p[P_LRATE] = 55;
    t->p[P_LD_PIT] = 12;
    slot(t, 0, MS_MODW, MD_DEPTH, 63);
    slot(t, 1, MS_MODW, MD_RATE, 40);
    if ((f = demo_open(dir, "lfo_rate_depth", 6u * FS / CTL * CTL))) {
        trk_note_on(t, 69, 100);
        for (k = 0; k < 5u * FS / CTL; k++) {
            t->mw = (uint8_t)(k * 127u / (5u * FS / CTL));
            demo_blocks(f, 1);
        }
        trk_note_off(t, 69);
        demo_blocks(f, FS / CTL);
        fclose(f);
        n++;
    }
    /* ANALOG SUB: a bass line, SUB at 0 .. 127 over it */
    fresh(0, preset_by_name(0, "SQR BASS"));
    t->p[P_E0] = 6;
    t->p[P_E2] = 0;
    if ((f = demo_open(dir, "analog_sub", 6u * FS / CTL * CTL))) {
        static const uint8_t LINE[8] = {36, 36, 43, 36, 39, 36, 46, 43};
        for (k = 0; k < 24u; k++) {
            t->p[P_E2] = (int16_t)(k * 127u / 23u);
            trk_note_on(t, LINE[k & 7u], 100);
            demo_blocks(f, FS / 4u / CTL * 3u / 4u);
            trk_note_off(t, LINE[k & 7u]);
            demo_blocks(f, FS / 4u / CTL - FS / 4u / CTL * 3u / 4u);
        }
        demo_blocks(f, 6u * FS / CTL - 24u * (FS / 4u / CTL));
        fclose(f);
        n++;
    }
    /* ANALOG SYNC: the classic sweep, ENV DEST SHP on a lead, then DTN up by hand */
    fresh(0, preset_by_name(0, "SAW LEAD"));
    t->p[P_E0] = 5;
    t->p[P_E1] = 0;
    t->p[P_E2] = 110;
    t->p[P_E4] = 100;
    t->p[P_ED_SHP] = 63;
    t->p[P_DEC] = 90;
    t->p[P_SUS] = 40;
    if ((f = demo_open(dir, "analog_sync", 7u * FS / CTL * CTL))) {
        static const uint8_t MEL[8] = {57, 60, 64, 67, 64, 60, 62, 55};
        for (k = 0; k < 8u; k++) {
            trk_note_on(t, MEL[k], 110);
            demo_blocks(f, FS / 2u / CTL);
            trk_note_off(t, MEL[k]);
        }
        t->p[P_ED_SHP] = 0;
        trk_note_on(t, 57, 110);
        for (k = 0; k < 2u * FS / CTL; k++) {
            t->p[P_E1] = (int16_t)(k * 127u / (2u * FS / CTL));
            demo_blocks(f, 1);
        }
        trk_note_off(t, 57);
        demo_blocks(f, 7u * FS / CTL - 4u * FS / CTL - 2u * FS / CTL);
        fclose(f);
        n++;
    }
    /* SPREAD on a pad: two chords at SPRD 0, then the same at SPRD 100 */
    fresh(0, preset_by_name(0, "SOFT PAD"));
    t->p[P_REV] = 50;
    if ((f = demo_open(dir, "spread_pad", 6u * (2u * FS / CTL) * CTL))) {
        uint32_t r;
        for (r = 0; r < 2u; r++) {
            t->p[P_SPRD] = (int16_t)(r ? 100 : 0);
            chord_on(t, PAD, 4);
            demo_blocks(f, 2u * FS / CTL);
            chord_off(t, PAD, 4);
            chord_on(t, PAD2, 4);
            demo_blocks(f, 2u * FS / CTL);
            chord_off(t, PAD2, 4);
            demo_blocks(f, 2u * FS / CTL);
        }
        fclose(f);
        n++;
    }
    /* SPREAD on UNISON: a supersaw-ish lead, SPRD 0 then 127 */
    fresh(0, preset_by_name(0, "SAW LEAD"));
    t->p[P_VOICE] = V_UNISON;
    t->p[P_DETUNE] = 60;
    if ((f = demo_open(dir, "spread_unison", 6u * FS / CTL * CTL))) {
        uint32_t r;
        for (r = 0; r < 2u; r++) {
            t->p[P_SPRD] = (int16_t)(r ? 127 : 0);
            trk_note_on(t, 57, 100);
            demo_blocks(f, 2u * FS / CTL);
            trk_note_off(t, 57);
            demo_blocks(f, FS / CTL);
        }
        fclose(f);
        n++;
    }
    printf("sound14: %u demos in %s (sh_cutoff, slew_cutoff, lfo_rate_depth, analog_sub, analog_sync, spread_pad, "
           "spread_unison)\n", n, dir);
    return n == 7u;
}

int main(int argc, char **argv)
{
    test_matrix();
    test_matrix_sound();
    test_analog();
    test_spread();
    test_spread_fx();
    test_cost();
    if (argc > 1)
        check("demos written", demos(argv[1]));
    printf("sound14: %s\n", bad ? "FAILED" : "all ok");
    return bad ? 1 : 0;
}
