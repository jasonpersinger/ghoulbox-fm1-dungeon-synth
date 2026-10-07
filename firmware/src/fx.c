/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Effects: per-track DIST insert, then sends into three
 * shared buses (chorus, tempo delay, reverb). Mono buses, stereo dry mix. */
#define DLY_LEN 65536u           /* 1.49 s: 1/4 at 40 BPM fits */
#define CHO_LEN 2048u
static int16_t dly_buf[DLY_LEN] __attribute__((section(".pool")));
static int16_t cho_buf[CHO_LEN] __attribute__((section(".pool")));
static const uint16_t REV_COMB[4] = {1116, 1188, 1277, 1356};
static const uint16_t REV_AP[2] = {556, 441};
static int16_t rev_comb[1116 + 1188 + 1277 + 1356] __attribute__((section(".pool")));
static union {                          /* ROOM's allpasses; SPRING's allpass chain (int32: no clamps) */
    int16_t ap[556 + 441];
    int32_t sp[(556 + 441) / 2];
} rev_u __attribute__((section(".pool")));
#define rev_ap (rev_u.ap)
static struct {
    uint32_t dly_w, cho_w, cho_ph;
    int32_t dly_lp;
    uint16_t comb_i[4], ap_i[2];
    int32_t comb_lp[4];
    uint8_t rtype;                       /* the reverb model running (G_RTYPE: 0 ROOM, 1 SPRING) */
    uint16_t sp_w;                       /* SPRING: the loop's write index (SP_MASK) */
    int32_t sp_lp, sp_hp, sp_he, sp_size;   /* .. its loop low-pass, low cut (and its remainder), the loop
                                             * length (Q8, glides) */
    uint32_t sp_ph;                      /* .. the output tap's wobble */
    uint16_t hl_p;                       /* HALL: the ring's pointer (counts down: hl_at) */
    int32_t hl_lpl, hl_lpr;              /* .. the two halves' damping low-passes */
    uint32_t hl_ph;                      /* .. the tank allpasses' slow modulation */
} fx;

/* HALL (G_RTYPE 2, GHOULBOX): J. Dattorro's figure-eight tank ("Effect Design, Part 1", JAES 1997): a pre-delay
 * (longer with SIZE: a bigger room answers later), four input allpasses (diffusion), then two halves that feed
 * each other: a modulated allpass (the slow wobble keeps a long tail from ringing metallic), a delay, the
 * damping low-pass (DAMP), the decay gain (SIZE), a second allpass and delay. Lines 1.33 x Dattorro's (his
 * 29.8 kHz lengths x 1.48 at 44.1 kHz would not fit). Seven taps, his left output: the bus is mono.
 * No RAM of its own: the back half of the delay line, dly_buf[DLY_LEN / 2 ..]; while HALL runs, the delay
 * keeps the front half (1/4 at 80 BPM fits, 0.74 s). Every multiply in the loop rounds towards 0 (hl_mul):
 * floors would feed the tank an offset for ever.
 * All thirteen lines share one ring of 32768 and one pointer that counts down a sample at a time (as the
 * Lexicon and FV-1 reverbs do): line k writes at hl_p + HL_B[k] and reads d samples back at hl_p + HL_B[k] + d,
 * both masked: no index per line, no wrap test. A line's region is its longest delay + 1 (its write slot). */
enum { H_PRE, H_I1, H_I2, H_I3, H_I4, H_AL, H_D1L, H_APL, H_D2L, H_AR, H_D1R, H_APR, H_D2R, H_N };
static const uint16_t HL_D[H_N] = {2792, 189, 142, 504, 368, 917, 5922, 2394, 4948, 1231, 5609, 3532, 4207};
#define HL_B_PRE 0u                      /* the lines' bases in the ring (the sums of the regions before) */
#define HL_B_I1 2793u
#define HL_B_I2 2983u
#define HL_B_I3 3126u
#define HL_B_I4 3631u
#define HL_B_AL 4000u
#define HL_B_D1L 4918u
#define HL_B_APL 10841u
#define HL_B_D2L 13236u
#define HL_B_AR 18185u
#define HL_B_D1R 19417u
#define HL_B_APR 25027u
#define HL_B_D2R 28560u
#define HL_MASK (DLY_LEN / 2u - 1u)
#define HL_AL 894u                       /* the modulated allpasses' centre delays, +-HL_EXC (their lines: +23) */
#define HL_AR 1208u
#define HL_EXC 21
#define hl_buf (dly_buf + DLY_LEN / 2u)
_Static_assert(HL_B_D2R + 4207u + 1u == DLY_LEN / 2u, "HALL's lines fill the back half of dly_buf");

/* SPRING (G_RTYPE 1): one spring of a spring tank, mono like the other buses, in the ROOM's own buffers (no
 * RAM of its own): the input and the loop's return -> a low cut (~110 Hz: a spring carries little bass) ->
 * SP_N stretched first-order allpasses, (a + z^-4) / (1 + a z^-4) (after Valimaki, Parker and Abel: below
 * fs / 8 = 5.5 kHz the group delay rises with frequency, the chirp; each pass round the loop adds more of
 * it: the "boing", the drips) -> the loop's delay line (rev_comb, SP_LEN) -> back through a one-pole
 * low-pass (DAMP) and the decay gain (SIZE). The output: the spring's far end, half way along the loop
 * (the first sound 15 .. 30 ms after the send: the tank's own pre-delay; a slow wobble of a sample or two
 * on it), plus a second, quieter pickup at three quarters (a shorter spring beside it: denser). SIZE sets
 * the loop's length (30 .. 60 ms) and its decay; DAMP the loop's low-pass. The allpasses' states: 4
 * samples each (int32), in rev_ap's memory. Changing the model fades the old one's block out and clears both buffers. */
#define SP_LEN 4096u                     /* the loop's line in rev_comb (4937 samples) */
#define SP_MASK (SP_LEN - 1u)
#define SP_N 10u                         /* allpass stages */
#define SP_A 2867                        /* their coefficient, Q12 (0.7: Q12 keeps (x - o) * a in 32 bits up to
                                          * |x - o| < 749000, far past any peak the chain reaches) */
_Static_assert(sizeof rev_comb / 2u >= SP_LEN && sizeof rev_u.sp / 4u >= 4u * (SP_N + 1u), "SPRING in ROOM's buffers");

/* DIST: low cut -> drive (1x..8x, exponential) -> asymmetric soft clip
 * (a little bias = even harmonics) -> tone low-pass that closes with drive ->
 * make-up gain (straight into tanh over the full band, it would sound like a
 * broken digital fuzz). State per part (track_t dist_*). */
static void track_dist(track_t *t, int32_t *b, uint32_t n)
{
    int32_t d = t->p[P_DIST], i, g, k, mk, bias = 2400, b0;
    if (!d)
        return;                                         /* states kept: switching on does not click */
    g = 4096 + d * d * 2;                                /* Q12: 1x .. ~9x, gentle at first */
    k = 32000 - d * 95;                                  /* tone: transparent at low drive .. ~3 kHz, Q15 */
    mk = 30000 - d * 120;                                /* make-up */
    b0 = softclip(bias);
    for (i = 0; i < (int32_t)n; i++) {
        int32_t x = b[i], y;
        t->dist_hp += (x - t->dist_hp + 64) >> 7;           /* ~55 Hz low cut: keep the bass out of the clipper */
        x = clamp(x - t->dist_hp, -230000, 230000);         /* (x >> 2) * g fits 32 bits; the clip is flat out there */
        y = softclip((((x >> 2) * g) >> 10) + bias) - b0;   /* >> 2 first: no overflow for loud poly */
        t->dist_lp1 += mulq15(y - t->dist_lp1, k);         /* two poles: tames the fizz */
        t->dist_lp2 += mulq15(t->dist_lp1 - t->dist_lp2, k);
        b[i] = mulq15(t->dist_lp2, mk);
    }
}

/* master: peak limiter in front of the soft clipper. Fast attack (~0.1 ms),
 * ~150 ms release, threshold where tanh is still nearly linear, so chords
 * get quieter instead of crushed. */
#define LIM_T 18000
static int32_t lim_env = LIM_T;
/* MENU > USB LEVEL FIXED (for #42: record over USB with the speaker turned down): the mix goes to master_out at the
 * full MASTER level, USB audio takes that (audio.c uac_tap), and only then does MASTER scale what the DAC gets
 * (usb_fixed_dac). MASTER (0, the default): MASTER before master_out, as always (USB follows the knob) */
static volatile uint8_t fx_usb_fixed;
#define MASTER_FULL 4096                /* main.c: the MASTER knob's top, Q12 */
static __attribute__((noinline)) void usb_fixed_dac(int32_t *out, uint32_t n)   /* audio ISR, after uac_tap */
{
    uint32_t i;
    int32_t m = (int32_t)song.master_q12;
    for (i = 0; i < 2u * n; i++)
        out[i] = (out[i] * m) >> 12;                /* (|out| <= 32767 after the soft clip: fits) */
}
static volatile uint8_t fx_lowcut;     /* settings: 1 LOWCUT 12 dB/oct ~110 Hz, 2 BASS+ (the small speaker):
                                        * 12 dB/oct ~220 Hz plus the harmonics of the bass (spk_bass) */
static int32_t lc_l1, lc_l2, lc_r1, lc_r2, dc_l, dc_r, dce_l, dce_r;

/* DC blocker (~2 Hz), always on: a leaky integrator of the input (Q6 state) subtracted from it.
 * The >> 12 step keeps its remainder (error feedback, 0..4095) and adds it to the next one, so no
 * part of the step is lost: the state follows the input exactly, down to 0 after the sound stops.
 * (A rounded step of (x - dc) / 4096 would stop moving at |x - dc| < 2048 and leave an offset of up
 * to +-31 at the output after silence.) */
static inline int32_t dc_block(int32_t x, int32_t *dc, int32_t *err)
{
    int32_t e = (x << 6) - *dc + *err, d = e >> 12;
    *err = e - (d << 12);
    *dc += d;
    return x - ((*dc + 32) >> 6);
}

static int32_t lce[4];
static inline int32_t lowcut1(int32_t x, int32_t *lc, int32_t *err, uint32_t sh)   /* x minus its one-pole low-pass */
{
    int32_t e = x - *lc + *err, d = e >> sh;
    *err = e - (d << sh);
    *lc += d;
    return x - *lc;
}

/* BASS+: what the speaker cannot play, heard through its harmonics. The bass below ~150 Hz is clipped at its
 * own envelope (a level-following trapezoid: odd harmonics), then band-passed ~220 Hz..1 kHz and added.
 * The low-pass has 4 poles (#42: with 2, the trapezoid rebuilt the 300 .. 600 Hz of the mix itself, late,
 * and cancelled up to 6 dB of it; now under 0.5 dB) */
static int32_t sb_lp1, sb_lp2, sb_lp3, sb_lp4, sb_env, sb_h1, sb_h2, sb_hl;
static inline int32_t spk_bass(int32_t m)
{
    int32_t a, t, u;
    sb_lp1 += ((m - sb_lp1) * 692) >> 15;
    sb_lp2 += ((sb_lp1 - sb_lp2) * 692) >> 15;
    sb_lp3 += ((sb_lp2 - sb_lp3) * 692) >> 15;
    sb_lp4 += ((sb_lp3 - sb_lp4) * 692) >> 15;
    a = sb_lp4 < 0 ? -sb_lp4 : sb_lp4;
    if (a > sb_env)
        sb_env += (a - sb_env) >> 2;
    else if (sb_env > 0)
        sb_env -= (sb_env >> 11) + 1;
    t = clamp(sb_lp4 * 8, -sb_env, sb_env);
    sb_h1 += (t - sb_h1) >> 5;
    u = t - sb_h1;
    sb_h2 += (u - sb_h2) >> 5;
    u -= sb_h2;
    sb_hl += (u - sb_hl) >> 3;
    return sb_hl * 3;
}

static inline void master_out(int32_t *l, int32_t *r)
{
    int32_t al, ar, a;
    *l = dc_block(*l, &dc_l, &dce_l);
    *r = dc_block(*r, &dc_r, &dce_r);
    if (fx_lowcut) {                  /* two one-pole high-passes, error feedback as dc_block (the */
        uint32_t sh = fx_lowcut == 2u ? 5u : 6u;    /* rounded step stopped at |x - lc| < 32: an offset) */
        int32_t b = fx_lowcut == 2u ? spk_bass((*l + *r) >> 1) : 0;
        *l = lowcut1(*l, &lc_l1, &lce[0], sh);
        *l = lowcut1(*l, &lc_l2, &lce[1], sh) + b;
        *r = lowcut1(*r, &lc_r1, &lce[2], sh);
        *r = lowcut1(*r, &lc_r2, &lce[3], sh) + b;
    }
    al = *l < 0 ? -*l : *l;
    ar = *r < 0 ? -*r : *r;
    a = al > ar ? al : ar;
    if (a > lim_env)
        lim_env += (a - lim_env) >> 2;
    else if (lim_env > LIM_T)
        lim_env -= ((lim_env - LIM_T) >> 12) + 1;
    if (lim_env > LIM_T) {
        int32_t g = (int32_t)(((uint32_t)LIM_T << 15) / (uint32_t)lim_env);   /* < 32768 */
        *l = ((*l >> 4) * g) >> 11;                      /* >> 4 first: |l| may be far above Q15 */
        *r = ((*r >> 4) * g) >> 11;
    }
    *l = softclip(*l);
    *r = softclip(*r);
}

/* length of one division (N_DIV order) in samples at the song tempo */
static const uint8_t DIV_DEN[6] = {1, 2, 4, 8, 3, 6};    /* original IDs stay fixed; slow rates append */
static uint32_t midi_beat_samples;                    /* zero until an external clock has a measured tempo */
static uint32_t beat_samples(void)
{
    return song.g[G_CLOCK] && midi_beat_samples ? midi_beat_samples : (uint32_t)FS * 60u / (uint32_t)song.g[G_BPM];
}
static uint32_t div_samples(uint32_t div)
{
    uint32_t quarter = beat_samples();
    return div < 6u ? quarter / DIV_DEN[div] : div < 10u ? quarter << (div - 5u) : quarter / DIV_DEN[div % 6u];
}

#include "perform.c"                                 /* the FX hold layer's effects (the master) */
#include "tape.c"                                    /* GHOULBOX: TAPE / CRSH on the mix, before MASTER */

static uint32_t delay_samples(void)
{
    uint32_t s = div_samples((uint32_t)song.g[G_DTIME]), len = fx.rtype == 2u ? DLY_LEN / 2u : DLY_LEN;
    return s < 16u ? 16u : s >= len ? len - 1u : s;
}

/* ROOM (G_RTYPE 0): 4 damped combs + 2 allpasses (Freeverb-like, mono), added to out */
static __attribute__((noinline)) void rev_room(const int32_t *rev_in, int32_t *out, uint32_t n)
{
    uint32_t i, k;
    int32_t size = 25000 + song.g[G_RSIZE] * 50, damp = 32767 - song.g[G_RDAMP] * 200;
    for (i = 0; i < n; i++) {
        int32_t a = 0;
        int16_t *c = rev_comb;
        int32_t in = mulq15(rev_in[i], 2580);           /* 1/8 at -4 dB: level as before the allpass fix */
        for (k = 0; k < 4u; k++) {
            int32_t o = c[fx.comb_i[k]];
            fx.comb_lp[k] = o + mulq15(fx.comb_lp[k] - o, 32767 - damp);
            c[fx.comb_i[k]] = (int16_t)clamp(in + mulq15(fx.comb_lp[k], size), -32768, 32767);
            if (++fx.comb_i[k] >= REV_COMB[k])
                fx.comb_i[k] = 0;
            a += o;
            c += REV_COMB[k];
        }
        c = rev_ap;
        for (k = 0; k < 2u; k++) {
            int32_t o = c[fx.ap_i[k]];
            int32_t v = a + (o >> 1);
            c[fx.ap_i[k]] = (int16_t)clamp(v, -32768, 32767);
            a = o - a;                                  /* Freeverb: out = buf - in (o - v is a notch comb) */
            if (++fx.ap_i[k] >= REV_AP[k])
                fx.ap_i[k] = 0;
            c += REV_AP[k];
        }
        out[i] += a;
    }
}

/* SPRING (see the top), added to out */
static __attribute__((noinline)) void rev_spring(const int32_t *rev_in, int32_t *out, uint32_t n)
{
    uint32_t i, k, s = (uint32_t)song.g[G_RSIZE];
    int32_t g = 19661 + (int32_t)s * 85;                /* the loop's gain: 0.6 .. 0.93 */
    int32_t kl = 26000 - song.g[G_RDAMP] * 160;         /* its low-pass: ~9 kHz .. ~1.3 kHz */
    int32_t len = (int32_t)(1323u + ((s * 1323u) >> 7)) << 8, L, L2, L3, f, w;
    int16_t *ln = rev_comb;
    int32_t *ap = rev_u.sp;
    if (!fx.sp_size)
        fx.sp_size = len;
    fx.sp_size += clamp(len - fx.sp_size, -256, 256);   /* SIZE glides (a sample a block at most) */
    L = fx.sp_size >> 8;
    fx.sp_ph += 2u * LFO_INC[24];                       /* the wobble: a slow sine, 1.5 samples deep */
    w = (fx.sp_size >> 1) + ((osc_sine(fx.sp_ph) * 3) >> 8);   /* the far end, Q8 */
    L2 = w >> 8;
    f = w & 255;
    L3 = (L * 3) >> 2;
    for (i = 0; i < n; i++) {
        uint32_t wp = fx.sp_w, j = (wp & 3u) * (SP_N + 1u);
        int32_t x = mulq15(rev_in[i], 2580), r = ln[(wp - (uint32_t)L) & SP_MASK], p, o;
        int32_t t0 = ln[(wp - (uint32_t)L2) & SP_MASK], t1 = ln[(wp - (uint32_t)L2 - 1u) & SP_MASK];
        fx.sp_lp += mulq15(r - fx.sp_lp, kl);
        o = fx.sp_lp * g;
        x += (o + ((o >> 31) & 32767)) >> 15;           /* towards 0: a loop of floors would hold an offset */
        o = x - fx.sp_hp + fx.sp_he;                    /* the low cut, its step's remainder kept (as */
        fx.sp_he = o & 63;                              /* dc_block): no dead band to hold an offset in the loop */
        fx.sp_hp += o >> 6;
        x -= fx.sp_hp;
        p = ap[j];                                      /* the chain: ap[j + k], stage k's output 4 samples ago */
        ap[j] = x;
        for (k = 1; k <= SP_N; k++) {                   /* (lossless: bounded by the loop's input, no clamp) */
            int32_t v = (x - ap[j + k]) * SP_A;         /* towards 0, as the loop's gain: floors would feed */
            o = ap[j + k];                              /* the loop a little offset and noise for ever */
            x = ((v + ((v >> 31) & 4095)) >> 12) + p;
            p = o;
            ap[j + k] = x;
        }
        ln[wp & SP_MASK] = (int16_t)clamp(x, -32768, 32767);
        fx.sp_w = (uint16_t)(wp + 1u);
        out[i] += (t0 + (((t1 - t0) * f) >> 8)) * 4 + ln[(wp - (uint32_t)L3) & SP_MASK] * 2;
    }
}

/* HALL (see the top) */
static inline int32_t hl_mul(int32_t a, int32_t g)      /* a * g / 32768 towards 0 (|a| <= 32768) */
{
    int32_t v = a * g;
    return (v + ((v >> 31) & 32767)) >> 15;
}
#define hl_at(base, d) hl_buf[(p + (base) + (d)) & HL_MASK]        /* d samples back (1 .. HL_D) */
#define hl_put(base, v) (hl_buf[(p + (base)) & HL_MASK] = (int16_t)clamp((v), -32768, 32767))
static inline int32_t hl_ap(uint32_t p, uint32_t base, uint32_t len, int32_t x, int32_t g)   /* allpass, g Q15 */
{
    int32_t o = hl_at(base, len), w = clamp(x - hl_mul(o, g), -32768, 32767);
    hl_buf[(p + base) & HL_MASK] = (int16_t)w;
    return o + hl_mul(w, g);
}
static inline int32_t hl_apmod(uint32_t p, uint32_t base, int32_t x, int32_t g, int32_t dq8)   /* .. at a moving delay (Q8) */
{
    uint32_t d = (uint32_t)dq8 >> 8;
    int32_t o0 = hl_at(base, d), o1 = hl_at(base, d + 1u), o = o0 + (((o1 - o0) * (dq8 & 255)) >> 8);
    int32_t w = clamp(x - hl_mul(o, g), -32768, 32767);
    hl_buf[(p + base) & HL_MASK] = (int16_t)w;
    return o + hl_mul(w, g);
}

/* HALL's decay gain (Q15) for SIZE 0..127, applied twice in each half of the tank (a half's loop: ~0.32 s), so
 * the low notes' RT60 (where the damping does not reach) = 0.48 s / -log10(g). SIZE sets that RT60 on an
 * exponential curve, 1.5 s at 0 to 12 s at 127 (x 2 every 42 steps: even to the ear); DAMP shortens the highs.
 * HL_G: g at SIZE 0, 8 .. 120, 127 (g = 10^(-0.4815 / RT60)), linear between. */
static const uint16_t HL_G[17] = {15648, 17134, 18554, 19896, 21153, 22320, 23398, 24385, 25286, 26104, 26843,
                                  27508, 28106, 28640, 29118, 29543, 29876};
static int32_t hall_decay(int32_t size)
{
    uint32_t k = (uint32_t)size >> 3, f = (uint32_t)size & 7u;
    return HL_G[k] + (((int32_t)HL_G[k + 1u] - (int32_t)HL_G[k]) * (int32_t)f >> 3);
}

static __attribute__((noinline)) void rev_hall(const int32_t *rev_in, int32_t *out, uint32_t n)
{
    uint32_t i, s = (uint32_t)song.g[G_RSIZE], p = fx.hl_p;
    uint32_t pd = 1u + ((s * (HL_D[H_PRE] - 1u)) >> 7);   /* pre-delay: 0 .. 63 ms */
    int32_t g = hall_decay((int32_t)s), kd = 32767 - song.g[G_RDAMP] * 220;   /* damping: open .. ~1 kHz */
    int32_t m = (osc_sine(fx.hl_ph) * HL_EXC) >> 7, m2 = (osc_sine(fx.hl_ph + 0x40000000u) * HL_EXC) >> 7;
    int32_t dl = (int32_t)(HL_AL << 8) + m, dr = (int32_t)(HL_AR << 8) + m2, lpl = fx.hl_lpl, lpr = fx.hl_lpr;
    fx.hl_ph += LFO_INC[20];                            /* ~0.5 Hz, stepped per block */
    for (i = 0; i < n; i++, p--) {
        int32_t x = clamp(mulq15(rev_in[i], 2580), -32768, 32767), el, er, a, o;
        a = hl_at(HL_B_PRE, pd);
        hl_put(HL_B_PRE, x);
        a = hl_ap(p, HL_B_I1, 189u, a, 24576);          /* input diffusion 0.75, 0.75, 0.625, 0.625 */
        a = hl_ap(p, HL_B_I2, 142u, a, 24576);
        a = hl_ap(p, HL_B_I3, 504u, a, 20480);
        a = hl_ap(p, HL_B_I4, 368u, a, 20480);
        el = hl_at(HL_B_D2L, 4948u);                    /* the halves' ends, before either writes */
        er = hl_at(HL_B_D2R, 4207u);
        /* left half, fed by the right: decay diffusion 1 (-0.7, moving), delay, damping, decay, diffusion 2 (0.5) */
        o = hl_at(HL_B_D1L, 5922u);
        hl_put(HL_B_D1L, hl_apmod(p, HL_B_AL, clamp(a + hl_mul(er, g), -32768, 32767), -22938, dl));
        lpl = hl_mul(o, kd) + hl_mul(lpl, 32768 - kd);   /* (each term towards 0: no dead band to stick in) */
        hl_put(HL_B_D2L, hl_ap(p, HL_B_APL, 2394u, hl_mul(lpl, g), 16384));
        /* right half, fed by the left */
        o = hl_at(HL_B_D1R, 5609u);
        hl_put(HL_B_D1R, hl_apmod(p, HL_B_AR, clamp(a + hl_mul(el, g), -32768, 32767), -22938, dr));
        lpr = hl_mul(o, kd) + hl_mul(lpr, 32768 - kd);   /* (each term towards 0: no dead band to stick in) */
        hl_put(HL_B_D2R, hl_ap(p, HL_B_APR, 3532u, hl_mul(lpr, g), 16384));
        /* Dattorro's left output taps (x 1.33): the bus is mono; x 4: ROOM's level */
        o = hl_at(HL_B_D1R, 354u) + hl_at(HL_B_D1R, 3955u) - hl_at(HL_B_APR, 2544u) + hl_at(HL_B_D2R, 2655u)
          - hl_at(HL_B_D1L, 2647u) - hl_at(HL_B_APL, 249u) - hl_at(HL_B_D2L, 1418u);
        out[i] += o << 2;
    }
    fx.hl_p = (uint16_t)p;
    fx.hl_lpl = lpl;
    fx.hl_lpr = lpr;
}

/* HALL's half of the delay line and its states to silence */
static void hall_clear(void)
{
    uint32_t i, *p = (uint32_t *)(void *)hl_buf;
    for (i = 0; i < DLY_LEN / 4u; i++)
        p[i] = 0;
    fx.hl_lpl = fx.hl_lpr = 0;
}

/* the reverb's buffers and states to silence (the model changed) */
static void rev_clear(void)
{
    uint32_t i;
    for (i = 0; i < sizeof rev_comb / 2u; i++)
        rev_comb[i] = 0;
    for (i = 0; i < sizeof rev_u.ap / 2u; i++)          /* (int16: 997 of them, an odd count the int32 view misses one of) */
        rev_u.ap[i] = 0;
    for (i = 0; i < 4u; i++)
        fx.comb_lp[i] = 0;
    fx.sp_lp = fx.sp_hp = fx.sp_he = 0;
}

static void rev_run(uint32_t model, const int32_t *rev_in, int32_t *out, uint32_t n)   /* G_RTYPE's model */
{
    if (model == 2u)
        rev_hall(rev_in, out, n);
    else if (model)
        rev_spring(rev_in, out, n);
    else
        rev_room(rev_in, out, n);
}

static int32_t part_buf[CTL];                            /* a part's block (mix_part); the fade of a model change */

/* process the three buses for one block; sends in, wet stereo-equal out */
static void fx_buses(const int32_t *cho_in, const int32_t *dly_in, const int32_t *rev_in, int32_t *wet,
                     uint32_t n)
{
    uint32_t i, dl = delay_samples(), dm = fx.rtype == 2u ? DLY_LEN / 2u - 1u : DLY_LEN - 1u;   /* HALL: front half */
    int32_t fb = song.g[G_DFDBK] * 230, col = 2000 + song.g[G_DCOLOR] * 240;
    int32_t dmix = song.g[G_DMIX] * 258;
    int32_t cdepth = song.g[G_CDEPTH] * 6, rt;
    uint32_t cinc = LFO_INC[song.g[G_CRATE] & 127] / CTL;
    for (i = 0; i < n; i++) {
        int32_t y = 0, x, r;
        /* chorus: modulated short delay, 5..15 ms */
        cho_buf[fx.cho_w & (CHO_LEN - 1u)] = (int16_t)clamp(cho_in[i] >> 1, -32768, 32767);
        fx.cho_ph += cinc;
        r = (400 << 8) + ((osc_sine(fx.cho_ph) + 32768) * cdepth >> 8);   /* Q8 delay: read between samples */
        {
            uint32_t ri = (uint32_t)r >> 8;
            int32_t f = r & 255, c0 = cho_buf[(fx.cho_w - ri) & (CHO_LEN - 1u)];
            int32_t c1 = cho_buf[(fx.cho_w - ri - 1u) & (CHO_LEN - 1u)];
            y += (c0 + (((c1 - c0) * f) >> 8)) << 1;
        }
        fx.cho_w++;
        /* delay with a low-passed feedback */
        x = dly_buf[(fx.dly_w - dl) & dm];
        fx.dly_lp += mulq15(x - fx.dly_lp, col);
        dly_buf[fx.dly_w & dm] =
            (int16_t)clamp((dly_in[i] >> 1) + mulq15(fx.dly_lp, fb), -32768, 32767);
        fx.dly_w++;
        y += mulq15(x << 1, dmix);
        wet[i] = y;
    }
    rt = song.g[G_RTYPE] >= 0 && song.g[G_RTYPE] <= 2 ? song.g[G_RTYPE] : 0;
    if (rt != fx.rtype) {                               /* the model changed: the old one's block fades out, */
        int32_t *t = part_buf, g = 65536, d = 65536 / (int32_t)n;   /* its buffers are cleared, the new */
        for (i = 0; i < n; i++)                                     /* one starts from silence */
            t[i] = 0;
        rev_run(fx.rtype, rev_in, t, n);
        for (i = 0; i < n; i++, g -= d)
            wet[i] += mulq16(t[i], (uint32_t)g);
        rev_clear();
        if (fx.rtype == 2u || rt == 2)                  /* HALL's half: the delay's own again, or HALL's from */
            hall_clear();                               /* silence (no old tail or old echoes replayed) */
        fx.rtype = (uint8_t)rt;
        return;
    }
    rev_run((uint32_t)rt, rev_in, wet, n);
}

/* one block of the whole mix (shared with hostsim.c): events -> each part (with its modulation matrix)
 * -> dist -> SLICER -> level / pan / sends -> buses -> master; out: stereo Q15 */
static void events_block(uint32_t n);                    /* seq.c */
static int32_t send_c[CTL], send_d[CTL], send_r[CTL], wet[CTL], mix_l[CTL], mix_r[CTL];

/* one synth part into the dry mix and the sends; a part with no voice sounding costs
 * the LFO tick and a cleared buffer only (after the DIST tail has run out) */
static void mix_part(track_t *t, uint32_t n)
{
    int32_t *b = part_buf;
    uint32_t i;
    mod_begin(t);                                       /* the matrix's per-block values into t->p (mod.c) */
    if (track_render(t, b, n))
        t->tail = 16;                                   /* blocks of DIST state to run out after the last voice */
    else if ((!t->tail || !t->p[P_DIST] || !--t->tail) && !slicer_busy(t)) {
        slicer_track(t, 0, n);                          /* (the SLICER's step clock runs on) */
        if (mod.on)
            mod_end(t);
        return;
    }
    {
        int32_t lvl = LEVEL_Q12[t->p[P_LEVEL] & 127], pan = t->p[P_PAN];
        int32_t gl = 4096 - (pan > 0 ? pan * 64 : 0), gr = 4096 + (pan < 0 ? pan * 64 : 0);
        int32_t c = t->p[P_CHOR] * 258, d = t->p[P_DLY] * 258, r = t->p[P_REV] * 258, pk = t->peak;
        int32_t xmax = c > d ? c : d;
        xmax = 0x7FFFFFFF / ((xmax > r ? xmax : r) | 1);   /* sends: loud chords at a high LEVEL */
        track_dist(t, b, n);
        slicer_track(t, b, n);                          /* slicer.c: before the level, pan and sends */
        if ((pf.mute >> (t - trk)) & 1u)
            perf_mute((uint32_t)(t - trk), b, n);       /* perform.c: a black key in the FX layer */
        for (i = 0; i < n; i++) {
            int32_t x = ((b[i] >> 2) * lvl) >> 10, a = x < 0 ? -x : x;   /* pre-shift: 8 loud voices */
            int32_t xs = clamp(x, -xmax, xmax);         /* sends: mulq15 would overflow */
            if (a > pk)
                pk = a;
            if (c)
                send_c[i] += mulq15(xs, c);
            if (d)
                send_d[i] += mulq15(xs, d);
            if (r)
                send_r[i] += mulq15(xs, r);
            mix_l[i] += (x * gl) >> 12;
            mix_r[i] += (x * gr) >> 12;
        }
        t->peak = pk;
    }
    if (mod.on)
        mod_end(t);                                     /* the stored values back */
}

/* the master with the FX layer's effects between its level and master_out (perform.c) */
static __attribute__((noinline)) void perf_master(int32_t *out, uint32_t n)
{
    uint32_t i;
    int32_t mg = fx_usb_fixed ? MASTER_FULL : (int32_t)song.master_q12;   /* (USB LEVEL FIXED: MASTER after) */
    for (i = 0; i < n; i++) {
        mix_l[i] += wet[i];
        mix_r[i] += wet[i];
    }
    tape_block(mix_l, mix_r, n);                        /* GHOULBOX: TAPE / CRSH, before MASTER */
    for (i = 0; i < n; i++) {
        mix_l[i] = ((mix_l[i] >> 2) * mg) >> 10;
        mix_r[i] = ((mix_r[i] >> 2) * mg) >> 10;
    }
    perf_block(mix_l, mix_r, n);
    for (i = 0; i < n; i++) {
        int32_t l = mix_l[i], r = mix_r[i];
        master_out(&l, &r);
        out[2u * i] = l;
        out[2u * i + 1u] = r;
    }
}

static void mix_block(int32_t *out, uint32_t n)
{
    uint32_t i;
    int perf;
    int32_t mg = fx_usb_fixed ? MASTER_FULL : (int32_t)song.master_q12;   /* (USB LEVEL FIXED: MASTER after) */
    for (i = 0; i < n; i++)
        send_c[i] = send_d[i] = send_r[i] = mix_l[i] = mix_r[i] = 0;
    events_block(n);
    perf = perf_begin(n);                               /* the FX hold layer at work (perform.c) */
    for (i = 0; i < NPART; i++)
        mix_part(&trk[i], n);
    if (perf)
        perf_pre(mix_l, mix_r, send_d, send_r, n);
    fx_buses(send_c, send_d, send_r, wet, n);
    if (perf) {
        perf_master(out, n);
        return;
    }
    for (i = 0; i < n; i++) {
        mix_l[i] += wet[i];
        mix_r[i] += wet[i];
    }
    tape_block(mix_l, mix_r, n);                        /* GHOULBOX: TAPE / CRSH, before MASTER */
    for (i = 0; i < n; i++) {
        int32_t l = ((mix_l[i] >> 2) * mg) >> 10;
        int32_t r = ((mix_r[i] >> 2) * mg) >> 10;
        master_out(&l, &r);
        out[2u * i] = l;
        out[2u * i + 1u] = r;
    }
}
