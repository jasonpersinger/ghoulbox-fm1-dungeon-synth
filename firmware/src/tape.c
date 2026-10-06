/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments; GHOULBOX changes 2026 */
/* TAPE and CRSH (G_TAPE, G_CRSH, GHOULBOX): the whole mix (dry + buses) through a cheap sampler and a worn
 * cassette, before MASTER (the level into the tape sets how hard it saturates, as a bounce would; MASTER is
 * the monitor). Both 0: nothing runs, the mix is bit for bit as without them.
 * CRSH (the keyboard): fewer bits (16 .. 7) and a sample-and-hold at 1/1 .. 1/6 of 44.1 kHz, no filter
 *   before it: the aliasing is the sound of an SK-1.
 * TAPE (the cassette, one "age" knob): wow (0.5 Hz, its depth wandering over seconds) and flutter (7 Hz) as a
 *   moving read of a short line (up to ~0.9 % and ~0.15 % of pitch at 127; the depth grows with TAPE squared:
 *   the first half of the knob is subtle), a tanh saturation (drive 1x .. 1.75x, made up: quiet parts pass at
 *   unity, the peaks round off), a 12 dB/oct high cut
 *   (~14 kHz .. ~3.5 kHz) and hiss (to ~-55 dB). The line's centre is fixed (TP_C): only the depth moves with
 *   the knob, the pitch does not jump; switching TAPE on starts the line from silence (~3 ms). */
#define TP_LEN 512u                                     /* the wow line, stereo, int32: 4 KiB */
#define TP_MASK (TP_LEN - 1u)
#define TP_C (140 << 8)                                 /* its centre delay, Q8 samples (~3.2 ms) */
#define TP_WOW_INC 1558426u                             /* phase steps per block (FS / CTL a second): 0.5 Hz */
#define TP_FLUT_INC 21818000u                           /* 7 Hz */
static int32_t tp_buf[2][TP_LEN] __attribute__((section(".pool")));
static struct {
    uint32_t w, ph_wow, ph_flut, hold_n;
    int32_t d_prev, drift, drift_to, rng, hiss_lp[2], lp1[2], lp2[2], hold[2];
    uint8_t on;
} tp;

static void tape_block(int32_t *l, int32_t *r, uint32_t n)
{
    int32_t t = song.g[G_TAPE], c = song.g[G_CRSH], *ch[2] = {l, r};
    uint32_t i, k;
    if (!t && !c) {
        tp.on = 0;
        return;
    }
    if (!tp.on) {                                       /* from silence: no stale line, no old state */
        memset(tp_buf, 0, sizeof tp_buf);
        memset(&tp, 0, sizeof tp);
        tp.rng = 0x2545F491;
        tp.d_prev = TP_C;
        tp.on = 1;
    }
    if (c) {                                            /* CRSH: hold every h-th sample, drop `sh` bits */
        uint32_t h = 1u + (((uint32_t)c * 6u) >> 7), sh = ((uint32_t)c * 10u) >> 7;
        for (i = 0; i < n; i++) {
            if (tp.hold_n == 0u)
                for (k = 0; k < 2u; k++) {
                    int32_t x = ch[k][i];               /* towards 0 (a floor would bias the mix negative) */
                    tp.hold[k] = x < 0 ? -((-x) >> sh << sh) : x >> sh << sh;
                }
            if (++tp.hold_n >= h)
                tp.hold_n = 0;
            l[i] = tp.hold[0];
            r[i] = tp.hold[1];
        }
    }
    if (t) {
        /* wow + flutter: the delay at this block's end (Q8), reached linearly over the block */
        int32_t dw = t * t * 2, df = t * 3, drv = 4096 + t * 24, mk = (4096 << 12) / drv, kl = 28200 - t * 121;
        int32_t hz = (t * t) >> 6, d1, dd, d;
        if ((tp.ph_wow >> 29) != ((tp.ph_wow + TP_WOW_INC) >> 29))   /* the wow's depth wanders: a new aim 4 times */
            tp.drift_to = (int32_t)(noise32(&tp.rng) >> 18) - 8192; /* a cycle, +-25 %, reached over ~1.5 s (a */
        tp.drift += (tp.drift_to - tp.drift) >> 11;                 /* faster step would be a pitch glitch) */
        tp.ph_wow += TP_WOW_INC;
        tp.ph_flut += TP_FLUT_INC;
        d1 = TP_C + mulq15(osc_sine(tp.ph_wow), (dw * (32768 + tp.drift)) >> 15)
                  + mulq15(osc_sine(tp.ph_flut), df);
        dd = (d1 - tp.d_prev) / (int32_t)n;
        d = tp.d_prev;
        for (i = 0; i < n; i++) {
            uint32_t di, nz = noise32(&tp.rng);         /* (16 bits of hiss for each channel) */
            int32_t f;
            d += dd;
            di = (uint32_t)d >> 8;
            f = d & 255;
            for (k = 0; k < 2u; k++) {
                int32_t *b = tp_buf[k], s0, s1, y;
                b[tp.w & TP_MASK] = ch[k][i];
                s0 = b[(tp.w - di) & TP_MASK];
                s1 = b[(tp.w - di - 1u) & TP_MASK];
                y = s0 + (((s1 - s0) >> 1) * f >> 7);   /* (>> 1 first: |s| may reach 2^21) */
                y = ((softclip(((clamp(y, -(1 << 21), 1 << 21) >> 1) * drv) >> 12) * mk) >> 12) << 1;   /* the saturation */
                tp.lp1[k] += mulq15(y - tp.lp1[k], kl);     /* the high cut, two poles */
                tp.lp2[k] += mulq15(tp.lp1[k] - tp.lp2[k], kl);
                tp.hiss_lp[k] += ((((int32_t)((nz >> (16u * k)) & 0xFFFFu) - 32768) * hz >> 17) - tp.hiss_lp[k]) >> 1;
                ch[k][i] = tp.lp2[k] + tp.hiss_lp[k];
            }
            tp.w++;
        }
        tp.d_prev = d1;
    }
}
