/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of the key / button debounce in hal/fm1_input.h (fm1__key, fm1__frame), against a model
 * of the scan and of bouncing contacts. The scan is the TIMER5 one (fm1_input_tick): one column every
 * 100 us, sampled one tick after it was latched; a frame (the debounce step) every 11 ticks.
 * A contact closes at time T, bounces (closed / open runs of 30..700 us) for up to 3 ms, stays closed,
 * and on the way up bounces again for up to 5 ms. Checked:
 *   - a clean press counts at the FM1_DEB_PRESS-th scan that sees it (<= 2.3 ms of the contact) and
 *     the press stat measures it;
 *   - 2000 bouncy presses of every note key and button: exactly one note-on (press edge) and one
 *     release each, no note ending while the key is held, none hanging 12 ms after the last bounce;
 *   - a stray closed sample (a 150 us glitch) plays nothing;
 *   - fast repeats (40 ms apart) are all heard;
 *   - the encoders still count one step per detent (their decoder is not touched);
 *   - the LED scan through fm1_input_tick: lit LEDs all of their tick, dim ones a pulse over the start of the
 *     595 shift (no wait) that settles to FM1_LED_DIM_NS (DIM HI) or FM1_LED_DIM_LO_NS (DIM LO, fm1_led_dim_level)
 *     +-30 % on every column whatever the bus speed, every
 *     frame, each only on its own column, the lines dark at every latch; the cost of a tick;
 *   - #119 the breath (fm1_led_breath), DIM HI and LO, the ticks 100 us apart, every LED's on-time a frame from the
 *     trace of the line writes against a lit LED: dark .. ~60 % of lit (LO ~30 %) and back over
 *     FM1_LED_BREATH_FRAMES, from its peak, dark at half a period; the low end a share of the pulse, over the glow whole
 *     lit frames; smooth and monotonic (32-frame means); no flicker near the peak (a dark run between two lit frames
 *     <= 9 frames, ~10 ms, where the breath is over half its peak; the runs over a quarter and at all printed); in
 *     step on two columns; the glow still its target +-30 %; a lit or dim LED never breathing, nothing of another
 *     column, the lines dark at the latch, the tick as before (the same writes) once nothing breathes.
 * The GPIO / timer helpers of the header are compiled, never called. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
static uint32_t host_now;                          /* TIMER4 ticks (24 MHz) */
#define FM1_INPUT_NOW() host_now
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#include "../firmware/hal/fm1_time.h"
#include "../firmware/hal/fm1_gpio.h"
static volatile uint32_t host_reg[16][64];        /* the GPIO registers, as memory (the LED test below) */
static uint32_t host_step = 2400u;                /* TIMER4 ticks per read of the clock (the LED test: finer) */
static uint32_t host_ticks(void) { return host_now += host_step; }
/* every write of the LED lines (fm1__led_lines), with the time it happened */
#define LED_NW 8
static uint32_t led_w[LED_NW], led_wt[LED_NW], led_nw, led_now, latch_lit, latches;
#define FM1_LED_TRACE(m) (led_now = (m), led_nw < LED_NW ? (led_wt[led_nw] = host_now, led_w[led_nw++] = (m)) : 0u)
#define FM1_SR_LATCH_TRACE() (latches++, latch_lit |= led_now)   /* the lines at the 595 latch: dark */
/* the GPIO as memory; each access costs host_bus TIMER4 ticks (the LED test: the bus time of the 595 shift) */
static uint32_t host_bus;
#undef FM1_PR
#define FM1_PR(p, r) (*(host_now += host_bus, &host_reg[(p) & 15u][((r) / 4u) & 63u]))
#define fm1_ticks host_ticks
#include "../firmware/hal/fm1_input.h"

#define TICK_US 100u
static int fails;
static int check(const char *what, int ok)
{
    printf("%-72s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok)
        fails++;
    return ok;
}

static uint32_t rng = 0x12345u;
static uint32_t rnd(uint32_t n)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng % n;
}

/* one contact: a list of times (us) at which it toggles, starting open */
#define NEDGE 1024
static struct { uint32_t t[NEDGE], n; } ct;
static int contact_at(uint32_t us)
{
    uint32_t i, s = 0;
    for (i = 0; i < ct.n && ct.t[i] <= us; i++)
        s ^= 1u;
    return (int)s;
}
static void add_edge(uint32_t us) { if (ct.n < NEDGE) ct.t[ct.n++] = us; }
/* a bouncy run starting at t (first edge = the change), the contact settles to `final` within
 * `span` us; returns the time of the last edge */
static uint32_t bounce(uint32_t t, uint32_t span, int final)
{
    uint32_t end = t + span, last = t;
    int s = final;
    add_edge(t);                                   /* the first touch / the first break */
    while (span) {
        uint32_t run = 30u + rnd(670u);
        if (t + run >= end)
            break;
        t += run;
        add_edge(t);
        s ^= 1;
        last = t;
    }
    if (s != final) {                              /* settle */
        t += 30u + rnd(200u);
        add_edge(t);
        last = t;
    }
    return last;
}

static uint32_t sim_us, sim_col;
static int key_col, key_row;
static void locate(uint32_t id)
{
    int r, p;
    for (r = 0; r < 6; r++)
        for (p = 0; p < (int)FM1_NCOL; p++)
            if (FM1_KEYMAP[r][p] == (int8_t)id) {
                key_row = r;
                key_col = p;
            }
}
/* one TIMER5 tick: as fm1_input_tick, column p's rows are read and its keys debounced, the frame
 * (the encoders) runs after the last column */
static void tick(void)
{
    uint32_t p = sim_col;
    fm1_in.raw[p] = (uint8_t)((int)p == key_col && contact_at(sim_us) ? 1u << key_row : 0u);
    sim_col = p + 1u == FM1_NCOL ? 0u : p + 1u;
    host_now = sim_us * 24u;
    fm1__keys(p);
    if (sim_col == 0u)
        fm1__frame();
    sim_us += TICK_US;
}
static uint32_t state_of(uint32_t id)
{
    return id >= 14u ? (fm1_in.notes >> (id - 14u)) & 1u : (fm1_in.buttons >> id) & 1u;
}
static void reset(void)
{
    memset((void *)&fm1_in, 0, sizeof fm1_in);
    memset((void *)&fm1_in_stat, 0, sizeof fm1_in_stat);
    memset(&ct, 0, sizeof ct);
    {
        uint32_t i;
        for (i = 0; i < FM1_NENC; i++)
            fm1_in.enc_prev[i] = fm1_in.enc_last[i] = 0xFF;
    }
}

/* run until `until` us; count rises / falls of id's state and the press edges */
static uint32_t rises, falls, edges, first_rise_us, last_fall_us, early_fall;
static uint32_t held_from, held_to;               /* the key is down (settled) in [held_from, held_to) */
static void run(uint32_t id, uint32_t until)
{
    while (sim_us < until) {
        uint32_t was = state_of(id), now;
        tick();
        now = state_of(id);
        if (now && !was) {
            rises++;
            if (!first_rise_us)
                first_rise_us = sim_us;
        }
        if (!now && was) {
            falls++;
            last_fall_us = sim_us;
            if (sim_us > held_from && sim_us < held_to)
                early_fall++;
        }
        if (id >= 14u ? fm1_in.notes_pressed : fm1_in.pressed) {
            edges++;
            fm1_in.notes_pressed = fm1_in.pressed = 0;
        }
    }
}

/* #126 a turned encoder 0: the toggle times of its A (0) and B (1) contacts; a turn of n detents (dir +1 clockwise:
 * B A B A from the detent state 0, -1: A B A B), per us a detent, its 4 transitions bunched over w (1 = even) of each,
 * each edge bouncing (runs of 30..bmax us) for up to bspan us (at most half the way to the next edge) */
#define ENE 8192
static struct { uint32_t t[ENE], n, i, s; } enl[2];
static void en_clear(void) { memset(enl, 0, sizeof enl); }
static void en_add(uint32_t l, uint32_t t) { if (enl[l].n < ENE && (!enl[l].n || t > enl[l].t[enl[l].n - 1u])) enl[l].t[enl[l].n++] = t; }
static uint32_t en_turn(uint32_t t0, int32_t n, int32_t dir, double per, double w, uint32_t bspan, uint32_t bmax)
{
    int32_t i, j;
    for (i = 0; i < n; i++)
        for (j = 0; j < 4; j++) {
            double f = 0.5 - w / 2 + (j + 0.5) * w / 4, nf = j < 3 ? 0.5 - w / 2 + (j + 1.5) * w / 4 : 1.0 + 0.5 - w / 2 + 0.5 * w / 4;
            uint32_t t = t0 + (uint32_t)(per * (i + f)), gap = (uint32_t)(per * (nf - f)), l = (uint32_t)((j & 1) ^ (dir > 0)), end;
            end = t + (bspan < gap / 2u ? bspan : gap / 2u);
            en_add(l, t);
            while (bspan && bmax) {          /* bounce: back and forth in pairs, the new level last */
                uint32_t a = t + 30u + rnd(bmax), b = a + 30u + rnd(bmax);
                if (b >= end)
                    break;
                en_add(l, a);
                en_add(l, b);
                t = b;
            }
        }
    return t0 + (uint32_t)(per * n);
}
static uint32_t en_at(uint32_t l, uint32_t us)     /* (us only rises between en_clear and the next) */
{
    while (enl[l].i < enl[l].n && enl[l].t[enl[l].i] <= us) {
        enl[l].i++;
        enl[l].s ^= 1u;
    }
    return enl[l].s;
}
/* the TIMER5 scan of encoder 0 (A on its column, B on its own) up to `until` us; the steps it counted */
static int32_t en_run(uint32_t until)
{
    const uint8_t *m = FM1_ENC[0];
    uint32_t us, col = 0;
    for (us = 0; us < until; us += TICK_US) {
        fm1_in.raw[col] = 0;
        if (col == m[0])
            fm1_in.raw[col] |= (uint8_t)(en_at(0, us) << m[1]);
        if (col == m[2])
            fm1_in.raw[col] |= (uint8_t)(en_at(1, us) << m[3]);
        col = col + 1u == FM1_NCOL ? 0u : col + 1u;
        if (!col)
            fm1__frame();
    }
    return fm1_in.enc_steps[0];
}

/* The power-on LED sweep (hal/fm1_led_anim.h, fm1_led_anim_start) through fm1_input_tick, from the trace of the line
 * writes: each LED's state each scan frame (lit: the lines left on for its column; the glow: its dim pulse alone).
 * Checked, with the idle glow (LEDS DIM HI), without (OFF), and with a key pressed half way:
 *   - it runs FM1_ANIM_FRAMES frames (~0.70 s) and its end picture, then fm1_led_anim_on() is 0 (a press: the frame after);
 *   - every LED, every frame, as its level asks (an own first order sigma-delta, lit frames /64; the glow under them
 *     or alone): exactly; over any 32 frames its lit frames within 1 of the level's sum; a dark run between lit frames
 *     at a level of 4 /64 or more <= 16 frames (~18 ms);
 *   - the picture: one peak at a time on the keys, falling away from it on both sides (a bright head, a fading tail),
 *     the head full and reaching every key left to right in place order (white and black interleaved), a tail of 4..6
 *     keys over the glow; the buttons dark until the keys are nearly done, then up to a quarter of lit and down;
 *   - the end: nothing lit, the glow on every LED (DIM) or none (OFF), breath / mid clear: the idle glow as the UI
 *     shows it; then the tick as before (the same line writes as with no sweep ever started).
 * Prints the sweep: a row every 32 frames, the 27 keys left to right and the buttons' level. */
static void anim_test(void)
{
    enum { NF = FM1_ANIM_FRAMES + 48u };
    static uint8_t on[NF][FM1_NKEY], gl[NF][FM1_NKEY], lvq[NF][FM1_NKEY];
    static int32_t fa[NF];
    uint32_t g;
    for (g = 0; g < 3u; g++) {                       /* 0 DIM (the glow), 1 OFF (no glow), 2 DIM, a key down at frame 200 */
        uint32_t litcur[FM1_NCOL] = {0}, fr = 0, t, i, c, r, bad = 0, badsd = 0, badrun = 0, frames_on = 0, end_fr = 0;
        uint32_t acc[FM1_NKEY], idle_bad = 0, tr_n = 0;
        uint32_t tr[3u * FM1_NCOL][LED_NW + 1u];
        char what[128];
        reset();
        fm1_led_dim_level(0);
        FM1_PR(FM1_PA, FM1_IN) = 0xFFFFFFFFu;   /* rows open */
        FM1_PR(FM1_PB, FM1_IN) = 0xFFFFFFFFu;   /* (one register a statement: FM1_PR advances host_now) */
        memset(fm1_led, 0x1E, sizeof fm1_led);         /* (whatever was there: the start clears it) */
        memset(fm1_led_dim, 0x1E, sizeof fm1_led_dim);
        memset(fm1_led_breath, 0x02, sizeof fm1_led_breath);
        memset(on, 0, sizeof on);
        memset(gl, 0, sizeof gl);
        memset(lvq, 0, sizeof lvq);
        host_step = 1u;
        host_bus = 1u;
        while (fm1__tick_col != 3u)                    /* (started mid-frame) */
            fm1_input_tick();
        fm1_led_anim_start(g != 1u);
        for (i = 0; i < NF; i++)
            fa[i] = -1;
        for (t = 0; fr + 1u < NF; t++) {
            uint32_t p = fm1__tick_col, n, run = fm1__an_f != 0u, f = fm1__an_f - 1u, relit, pulse;
            if (g == 2u && fr == 200u)
                fm1_in.notes = 1u << 5;               /* a key down */
            led_nw = 0;
            fm1_input_tick();
            n = fm1__tick_col;
            relit = n == 0u && run;                    /* the sweep's frame: column 0 written again */
            pulse = led_nw - relit == 4u ? led_w[1] & ~litcur[p] : 0u;   /* column p's pulse, of frame fr */
            for (r = 1; r < 5u; r++)
                if (FM1_KEYMAP[r][p] >= 0)
                    gl[fr][FM1_KEYMAP[r][p]] = (uint8_t)((pulse >> r) & 1u);
            if (n == 0u) {
                fr++;
                if (run) {
                    fa[fr] = (int32_t)f;
                    frames_on++;
                    if (!fm1_led_anim_on() && !end_fr)
                        end_fr = fr;
                }
            }
            litcur[n] = led_w[led_nw - 1u];
            for (r = 1; r < 5u; r++)
                if (FM1_KEYMAP[r][n] >= 0)
                    on[fr][FM1_KEYMAP[r][n]] = (uint8_t)((litcur[n] >> r) & 1u);
            if (end_fr && fr >= end_fr + 4u && tr_n < 3u * FM1_NCOL) {   /* idle: three frames of writes */
                tr[tr_n][0] = led_nw;
                for (i = 0; i < led_nw; i++)
                    tr[tr_n][1u + i] = led_w[i];
                tr_n++;
            }
        }
        /* the model: the same levels, the same sigma-delta, frame by frame */
        for (i = 0; i < FM1_NKEY; i++)
            acc[i] = FM1_ANIM_FULL / 2u;
        for (fr = 1; fr + 1u < NF; fr++) {
            if (fa[fr] < 0)
                continue;
            for (i = 0; i < FM1_NKEY; i++) {
                uint32_t f = (uint32_t)fa[fr], q, lit;
                if (g == 2u && fr > 200u)              /* (the key down: the end at once) */
                    f = FM1_ANIM_FRAMES;
                q = fm1_anim_level(f, i, g != 1u);
                acc[i] += q & 0x7Fu;
                lit = acc[i] >= FM1_ANIM_FULL;
                if (lit)
                    acc[i] -= FM1_ANIM_FULL;
                lvq[fr][i] = (uint8_t)q;
                bad += on[fr][i] != lit || gl[fr][i] != ((q & FM1_ANIM_GLOW) && !lit);
            }
        }
        /* over any 32 frames: lit frames against the level's sum; the dark runs at 4 /64 and up */
        for (i = 0; i < FM1_NKEY; i++) {
            uint32_t s = 0, k = 0, run = 0;
            for (fr = 1; fr + 32u < NF; fr++) {
                if (fa[fr] < 0 || fa[fr + 31u] < 0)
                    continue;
                for (k = s = 0, t = fr; t < fr + 32u; t++) {
                    s += lvq[t][i] & 0x7Fu;
                    k += on[t][i];
                }
                badsd += (k * 64u + 64u < s) || (k * 64u > s + 64u);
            }
            for (fr = 1; fr < NF; fr++) {
                if (fa[fr] < 0 || (lvq[fr][i] & 0x7Fu) < 4u || on[fr][i]) {
                    run = 0;
                    continue;
                }
                badrun += ++run > 16u;
            }
        }
        if (g == 2u) {
            snprintf(what, sizeof what, "power-on sweep: a key down at frame 200: done the next frame, the glow left");
            check(what, !bad && end_fr == 201u && frames_on == 201u);
            fm1_in.notes = 0;
            continue;
        }
        snprintf(what, sizeof what, "power-on sweep %s: %u frames (%u ms) and its end, then fm1_led_anim_on() 0",
                 g ? "OFF" : "DIM", (unsigned)(frames_on - 1u), (unsigned)((frames_on - 1u) * FM1_NCOL * TICK_US / 1000u));
        check(what, frames_on == FM1_ANIM_FRAMES + 1u && !fm1_led_anim_on());
        check("  every LED every frame as its level asks: lit frames (sigma-delta), the glow under / alone", !bad);
        check("  32 frames: lit frames within 1 of the level; at 4 /64 and up a dark run <= 16 frames", !badsd && !badrun);
        {   /* the picture (DIM / OFF alike): over the sweep's frames, the keys' levels by place */
            uint32_t peak_fr[FM1_ANIM_NKEY] = {0}, peak[FM1_ANIM_NKEY] = {0}, k, badpic = 0, tmin = 99, tmax = 0, bmax = 0, bbad = 0;
            for (fr = 1; fr < NF; fr++) {
                int32_t f = fa[fr];
                uint32_t lv[FM1_ANIM_NKEY], top = 0, j, tail = 0;
                if (f < 0 || f >= (int32_t)FM1_ANIM_FRAMES)
                    continue;
                for (k = 0; k < FM1_ANIM_NKEY; k++) {
                    lv[k] = fm1_anim_level((uint32_t)f, 14u + k, g != 1u) & 0x7Fu;
                    if (lv[k] > peak[k]) {
                        peak[k] = lv[k];
                        peak_fr[k] = (uint32_t)f;
                    }
                    if (lv[k] > lv[top])
                        top = k;
                }
                for (j = top; j > 0; j--)              /* falling away from the head on both sides */
                    badpic += lv[j - 1u] > lv[j];
                for (j = top; j + 1u < FM1_ANIM_NKEY; j++)
                    badpic += lv[j + 1u] > lv[j];
                for (j = top; j > 0 && lv[j - 1u]; j--)
                    tail++;                            /* keys over the glow behind the head */
                if (lv[top] >= 48u && top >= 6u && top + 3u < FM1_ANIM_NKEY) {   /* (the head well on the keyboard) */
                    tmin = tail < tmin ? tail : tmin;
                    tmax = tail > tmax ? tail : tmax;
                }
                {
                    uint32_t b = fm1_anim_level((uint32_t)f, 0, g != 1u) & 0x7Fu;
                    static uint32_t bprev;
                    if (f < (int32_t)FM1_ANIM_BTN0)
                        bbad += b != 0u;
                    else if (f <= (int32_t)FM1_ANIM_BTN_PK)
                        bbad += b < bprev;
                    else
                        bbad += b > bprev;
                    bprev = b;
                    bmax = b > bmax ? b : bmax;
                }
            }
            for (k = 0; k < FM1_ANIM_NKEY; k++) {
                badpic += peak[k] != FM1_ANIM_FULL;
                badpic += k && (peak_fr[k] <= peak_fr[k - 1u] || FM1_ANIM_X[k] <= FM1_ANIM_X[k - 1u]);
            }
            snprintf(what, sizeof what, "  the picture: one head, full, left to right in place order; a tail of %u..%u keys",
                     (unsigned)tmin, (unsigned)tmax);
            check(what, !badpic && tmin >= 4u && tmax <= 6u);
            snprintf(what, sizeof what, "  the buttons: dark until frame %u, up to %u /64, down to the end",
                     (unsigned)FM1_ANIM_BTN0, (unsigned)bmax);
            check(what, !bbad && bmax == 16u);
        }
        {   /* the end: the idle picture's glow (DIM) or dark (OFF) */
            uint32_t ok = 1, all = 0;
            for (c = 0; c < FM1_NCOL; c++) {
                uint32_t m = 0;
                for (r = 1; r < 5u; r++)
                    m |= FM1_KEYMAP[r][c] >= 0 ? 1u << r : 0u;
                all |= m;
                ok &= !fm1_led[c] && fm1_led_dim[c] == (g ? 0u : m) && !fm1_led_breath[c] && !fm1_led_mid[c];
            }
            for (fr = end_fr + 1u; fr < NF - 1u; fr++)
                for (i = 0; i < FM1_NKEY; i++)
                    ok &= !on[fr][i] && gl[fr][i] == (g ? 0u : 1u);
            snprintf(what, sizeof what, "  the end: nothing lit, %s; nothing breathing or mid", g ? "dark (no glow)" : "the glow on every LED");
            check(what, ok && all);
        }
        {   /* idle: the same writes as a scan that never ran the sweep (the same LEDs) */
            uint32_t j;
            uint16_t f0 = fm1__an_f;
            while (fm1__tick_col != FM1_NCOL - 1u)    /* (traced from the tick that lights column 0) */
                fm1_input_tick();
            for (t = 0; t < tr_n; t++) {
                led_nw = 0;
                fm1_input_tick();
                idle_bad += tr[t][0] != led_nw || (led_nw != 2u && led_nw != 4u);
                for (j = 0; j < led_nw && j < tr[t][0]; j++)
                    idle_bad += tr[t][1u + j] != led_w[j];
            }
            snprintf(what, sizeof what, "  then the tick as before: %u ticks, the same line writes, no extra one", (unsigned)tr_n);
            check(what, !idle_bad && tr_n == 3u * FM1_NCOL && !f0);
        }
        if (!g) {                                      /* the sweep, a row every 32 frames: the keys F3..G5 left to right */
            static const char SH[] = " .:-=+*#%@";    /* the glow '.', then lit frames /64 by eighths */
            uint32_t f;
            printf("power-on sweep (DIM): frame  ms  keys F3..G5 (black and white by place)  buttons\n");
            for (f = 0; f <= FM1_ANIM_FRAMES; f += 32u) {
                char row[FM1_ANIM_NKEY + 1u], b;
                uint32_t k, q;
                for (k = 0; k < FM1_ANIM_NKEY; k++) {
                    q = fm1_anim_level(f, 14u + k, 1);
                    row[k] = (q & 0x7Fu) ? SH[2u + ((q & 0x7Fu) - 1u) * 8u / 64u] : q ? SH[1] : SH[0];
                }
                row[FM1_ANIM_NKEY] = 0;
                q = fm1_anim_level(f, 0, 1);
                b = (q & 0x7Fu) ? SH[2u + ((q & 0x7Fu) - 1u) * 8u / 64u] : q ? SH[1] : SH[0];
                printf("  %4u %4u  |%s|  %c %2u/64\n", (unsigned)f, (unsigned)(f * FM1_NCOL * TICK_US / 1000u), row, b,
                       (unsigned)(q & 0x7Fu));
            }
        }
    }
    host_step = 2400u;
    host_bus = 0;
    memset(fm1_led, 0, sizeof fm1_led);
    memset(fm1_led_dim, 0, sizeof fm1_led_dim);
    memset(fm1_led_breath, 0, sizeof fm1_led_breath);
}

/* 1.2 (Discussion #135): the idle animation (hal/fm1_led_anim.h fm1_idle_level, hal/fm1_input.h fm1_led_idle_start)
 * through fm1_input_tick, the LEDs read after each scan frame:
 *   - endless: over a cycle and a half it keeps running (the frame wraps at FM1_IDLE_FRAMES), fm1_led_idle_on() 1;
 *   - every LED, every frame, as its level asks (lit frames by the same first order sigma-delta, the glow flag);
 *   - the picture: one soft light on the keys (at most ~35 % of lit), drifting left to right over the first half of
 *     a cycle and back over the second, reaching both ends; the keys around it falling away; the buttons together
 *     between 1/64 and 1/8 of lit; the glow under every LED with DIM, none without;
 *   - a key down ends it before the next frame is drawn (the LEDs as left), so does fm1_led_anim_stop; the power-on
 *     sweep started after it is the sweep (not idle) */
static void idle_anim_test(void)
{
    uint32_t g;
    for (g = 0; g < 2u; g++) {                         /* 0 DIM (the glow), 1 OFF */
        enum { NF = FM1_IDLE_FRAMES + FM1_IDLE_FRAMES / 2u };
        uint32_t acc[FM1_NKEY], cb[FM1_NKEY], fr = 0, i, r, c, bad = 0, badglow = 0, run_ok = 1, maxlv = 0;
        uint32_t lmin = 999, lmax = 0, bmin = 99, bmax = 0, mono = 0, prevtop = 0, reach_l = 0, reach_r = 0, badfall = 0;
        char what[128];
        reset();
        fm1_led_dim_level(0);
        FM1_PR(FM1_PA, FM1_IN) = 0xFFFFFFFFu;
        FM1_PR(FM1_PB, FM1_IN) = 0xFFFFFFFFu;   /* (one register a statement: FM1_PR advances host_now) */
        host_step = 1u;
        host_bus = 1u;
        for (i = 0; i < FM1_NKEY; i++)                 /* each LED's column and row (as fm1_led_anim_start) */
            for (r = 1; r < 5u; r++)
                for (c = 0; c < FM1_NCOL; c++)
                    if (FM1_KEYMAP[r][c] == (int8_t)i)
                        cb[i] = c << 3 | r;
        while (fm1__tick_col != 3u)
            fm1_input_tick();
        fm1_led_idle_start(g == 0u);
        for (i = 0; i < FM1_NKEY; i++)
            acc[i] = FM1_ANIM_FULL / 2u;
        while (fr < NF) {
            uint32_t f = fm1__an_f - 1u, lv[FM1_ANIM_NKEY], top = 0, k;
            do
                fm1_input_tick();
            while (fm1__tick_col != 0u);
            run_ok &= fm1_led_idle_on() && fm1_led_anim_on();
            for (i = 0; i < FM1_NKEY; i++) {           /* the model: this frame's levels, the same sigma-delta */
                uint32_t q = fm1_idle_level(f, i, g == 0u), lit, m = 1u << (cb[i] & 7u);
                acc[i] += q & 0x7Fu;
                lit = acc[i] >= FM1_ANIM_FULL;
                if (lit)
                    acc[i] -= FM1_ANIM_FULL;
                bad += ((fm1_led[cb[i] >> 3] & m) != 0u) != lit;
                bad += ((fm1_led_dim[cb[i] >> 3] & m) != 0u) != ((q & FM1_ANIM_GLOW) != 0u);
                badglow += g == 0u && !(q & FM1_ANIM_GLOW);
                if (i < FM1_ANIM_NBTN) {
                    bmin = (q & 0x7Fu) < bmin ? q & 0x7Fu : bmin;
                    bmax = (q & 0x7Fu) > bmax ? q & 0x7Fu : bmax;
                }
            }
            for (k = 0; k < FM1_ANIM_NKEY; k++) {      /* the picture on the keys, by place */
                lv[k] = fm1_idle_level(f, FM1_ANIM_NBTN + k, 1) & 0x7Fu;
                if (lv[k] > lv[top])
                    top = k;
            }
            for (k = top; k > 0; k--)
                badfall += lv[k - 1u] > lv[k];
            for (k = top; k + 1u < FM1_ANIM_NKEY; k++)
                badfall += lv[k + 1u] > lv[k];
            maxlv = lv[top] > maxlv ? lv[top] : maxlv;
            lmin = lv[top] < lmin ? lv[top] : lmin;
            lmax = lv[top] > lmax ? lv[top] : lmax;
            if (fr && (f & (FM1_IDLE_FRAMES - 1u)))   /* left to right in the first half, back in the second */
                mono += (f & (FM1_IDLE_FRAMES / 2u)) ? top > prevtop : top < prevtop;
            reach_l |= top == 0u;
            reach_r |= top == FM1_ANIM_NKEY - 1u;
            prevtop = top;
            fr++;
        }
        snprintf(what, sizeof what, "idle animation %s: endless (%u frames, %u s), every LED every frame as its level asks",
                 g ? "OFF" : "DIM", (unsigned)fr, (unsigned)(fr * FM1_NCOL * TICK_US / 1000000u));
        check(what, run_ok && !bad);
        snprintf(what, sizeof what, "  one soft light (%u..%u /64 at it), drifting F3 -> G5 and back each %u s, falling away",
                 (unsigned)lmin, (unsigned)lmax, (unsigned)(FM1_IDLE_FRAMES * FM1_NCOL * TICK_US / 1000000u));
        check(what, !mono && reach_l && reach_r && !badfall && maxlv <= 24u && lmin >= 8u);
        snprintf(what, sizeof what, "  the buttons breathe: the glow (1/64) .. %u /64 (lit frames %u..%u); %s", (unsigned)bmax, (unsigned)bmin, (unsigned)bmax,
                 g ? "no glow" : "the glow under every LED");
        check(what, bmin == 0u && bmax == 8u && !badglow);
        {   /* a key down: done before the next frame (the LEDs as left); the sweep after it is the sweep */
            uint8_t l0[FM1_NCOL], d0[FM1_NCOL];
            memcpy(l0, fm1_led, sizeof l0);
            memcpy(d0, fm1_led_dim, sizeof d0);
            fm1_in.notes = 1u << 3;
            do
                fm1_input_tick();
            while (fm1__tick_col != 0u);
            run_ok = !fm1_led_anim_on() && !fm1_led_idle_on() && !memcmp(l0, fm1_led, sizeof l0) && !memcmp(d0, fm1_led_dim, sizeof d0);
            fm1_in.notes = 0;
            fm1_led_idle_start(1);
            fm1_input_tick();
            fm1_led_anim_stop();
            run_ok &= !fm1_led_anim_on();
            fm1_led_anim_start(1);
            run_ok &= fm1_led_anim_on() && !fm1_led_idle_on();
            fm1_led_anim_stop();
            check("  a key down ends it before its next frame (the LEDs as left); fm1_led_anim_stop too; the sweep is no idle", run_ok);
        }
    }
    if (1) {                                           /* the cycle, a row every 512 frames (DIM) */
        static const char SH[] = " .:-=+*#%@";
        uint32_t f;
        printf("idle animation (DIM): frame  s  keys F3..G5  buttons\n");
        for (f = 0; f <= FM1_IDLE_FRAMES; f += 512u) {
            char row[FM1_ANIM_NKEY + 1u], b;
            uint32_t k, q;
            for (k = 0; k < FM1_ANIM_NKEY; k++) {
                q = fm1_idle_level(f, 14u + k, 1);
                row[k] = (q & 0x7Fu) ? SH[2u + ((q & 0x7Fu) - 1u) * 8u / 64u] : q ? SH[1] : SH[0];
            }
            row[FM1_ANIM_NKEY] = 0;
            q = fm1_idle_level(f, 0, 1);
            b = (q & 0x7Fu) ? SH[2u + ((q & 0x7Fu) - 1u) * 8u / 64u] : q ? SH[1] : SH[0];
            printf("  %5u %4.1f  |%s|  %c %2u/64\n", (unsigned)f, f * FM1_NCOL * TICK_US / 1e6, row, b, (unsigned)(q & 0x7Fu));
        }
    }
    host_step = 2400u;
    host_bus = 0;
    memset(fm1_led, 0, sizeof fm1_led);
    memset(fm1_led_dim, 0, sizeof fm1_led_dim);
    memset(fm1_led_breath, 0, sizeof fm1_led_breath);
}

int main(void)
{
    uint32_t id, k, lat_max = 0, lat_sum = 0, lat_n = 0, worst_rel = 0, bad = 0;
    printf("debounce: press %u frames, release %u frames, frame %u us\n",
           (unsigned)FM1_DEB_PRESS, (unsigned)FM1_DEB_RELEASE, (unsigned)(FM1_NCOL * TICK_US));

    /* a clean press of every note key and button, at every phase of the scan */
    for (id = 0; id < FM1_NKEY; id++) {
        uint32_t ph;
        locate(id);
        for (ph = 0; ph < FM1_NCOL * TICK_US; ph += 37u) {
            uint32_t t0;
            reset();
            sim_us = 0;
            sim_col = 0;
            t0 = 10000u + ph;
            add_edge(t0);
            add_edge(t0 + 100000u);
            rises = falls = edges = first_rise_us = last_fall_us = early_fall = 0;
            held_from = t0;
            held_to = t0 + 100000u;
            run(id, t0 + 130000u);
            if (rises != 1u || falls != 1u || edges != 1u)
                bad++;
            if (first_rise_us - t0 > lat_max)
                lat_max = first_rise_us - t0;
            lat_sum += first_rise_us - t0;
            lat_n++;
            if (last_fall_us - (t0 + 100000u) > worst_rel)
                worst_rel = last_fall_us - (t0 + 100000u);
        }
    }
    printf("clean press: contact -> press avg %u us, max %u us; release -> note off max %u us\n",
           (unsigned)(lat_sum / lat_n), (unsigned)lat_max, (unsigned)worst_rel);
    check("clean presses: one press and one release each, every key and button", bad == 0u);
    check("clean press: counted within 2.3 ms of the contact (2 frames)", lat_max <= 2300u);
    check("clean release: the note ends within 10 ms", worst_rel <= 10000u);
    {
        uint32_t stat = fm1_in_stat.press_n ? fm1_in_stat.press_sum / fm1_in_stat.press_n / 24u : 0u;
        printf("press stat (first closed frame -> press): avg %u us over %u\n", (unsigned)stat,
               (unsigned)fm1_in_stat.press_n);
        check("the press stat counts the note keys' presses (one frame after the first)",
              fm1_in_stat.press_n == 1u && stat >= 1000u && stat <= 1200u);
    }

    /* bouncy presses and releases */
    {
        uint32_t trials = 0, onebad = 0, early = 0, hang = 0;
        uint32_t blat_max = 0, blat_sum = 0, rel_max = 0, settle_max = 0;
        for (k = 0; k < 2000u; k++) {
            uint32_t t0, tb, hold, tr, tl;
            id = rnd(FM1_NKEY);
            locate(id);
            reset();
            sim_us = rnd(1100u);
            sim_col = rnd(FM1_NCOL);
            t0 = 5000u + rnd(1100u);
            tb = bounce(t0, rnd(3000u), 1);        /* settled closed from tb */
            hold = 15000u + rnd(300000u);
            tr = tb + hold;
            tl = bounce(tr, rnd(5000u), 0);        /* settled open from tl */
            rises = falls = edges = first_rise_us = last_fall_us = early_fall = 0;
            held_from = tb;
            held_to = tr;
            run(id, tl + 12000u);
            trials++;
            if (rises != 1u || falls != 1u || edges != 1u)
                onebad++;
            early += early_fall;
            if (state_of(id))
                hang++;
            if (first_rise_us - t0 > blat_max)
                blat_max = first_rise_us - t0;
            blat_sum += first_rise_us - t0;
            if (first_rise_us > tb && first_rise_us - tb > settle_max)
                settle_max = first_rise_us - tb;
            if (last_fall_us > tl && last_fall_us - tl > rel_max)
                rel_max = last_fall_us - tl;
        }
        printf("bouncy press (<= 3 ms of bounce): first touch -> press avg %u us, max %u us; "
               "settled -> press max %u us; last bounce -> note off max %u us\n",
               (unsigned)(blat_sum / trials), (unsigned)blat_max, (unsigned)settle_max, (unsigned)rel_max);
        check("2000 bouncy presses: exactly one note-on / press and one release each", onebad == 0u);
        check("no note ends while its key is held", early == 0u);
        check("no hanging note 12 ms after the last bounce of a release", hang == 0u);
        check("bouncy press: counted within 2.3 ms of the contact settling, or sooner", settle_max <= 2300u);
        check("bouncy release: the note ends within 10 ms of the last bounce", rel_max <= 10000u);
    }

    /* a stray closed sample: no note */
    {
        uint32_t g, any = 0;
        id = 20;
        locate(id);
        for (g = 0; g < FM1_NCOL * TICK_US; g += 13u) {
            reset();
            sim_us = 0;
            sim_col = 0;
            add_edge(5000u + g);
            add_edge(5150u + g);
            rises = falls = edges = first_rise_us = 0;
            held_from = held_to = 0;
            run(id, 40000u);
            any += rises + edges;
        }
        check("a 150 us glitch plays no note", any == 0u);
    }

    /* fast repeats */
    {
        uint32_t t;
        id = 30;
        locate(id);
        reset();
        sim_us = 0;
        sim_col = 0;
        for (t = 0; t < 10u; t++) {
            bounce(5000u + t * 40000u, 1500u, 1);
            bounce(5000u + t * 40000u + 20000u, 2000u, 0);
        }
        rises = falls = edges = first_rise_us = 0;
        held_from = held_to = 0;
        run(id, 5000u + 10u * 40000u + 20000u);
        check("10 notes 40 ms apart (20 ms held, bouncy): 10 note-ons, 10 releases",
              rises == 10u && edges == 10u && falls == 10u);
    }

    /* encoders: one clockwise detent cycle of encoder 0 = one step (decoder unchanged) */
    {
        static const uint8_t SEQ[] = {0, 1, 3, 2, 0};   /* quadrature states A<<1|B, from the rest */
        uint32_t i, f;
        const uint8_t *m = FM1_ENC[0];
        int32_t s;
        reset();
        for (i = 0; i < 5u; i++)
            for (f = 0; f < 4u; f++) {
                memset((void *)fm1_in.raw, 0, sizeof fm1_in.raw);
                fm1_in.raw[m[0]] |= (uint8_t)(((SEQ[i] >> 1) & 1u) << m[1]);
                fm1_in.raw[m[2]] |= (uint8_t)((SEQ[i] & 1u) << m[3]);
                fm1__frame();
            }
        s = fm1_in.enc_steps[0];
        printf("encoder 0: one cycle -> %d step(s)\n", (int)s);
        check("an encoder detent cycle is one step", s == 1 || s == -1);
    }
    {   /* #63: a knob left alone on its detent, one contact chattering (runs of 1..5 frames), or both contacts
         * lost together (the row they share disturbed, 1..5 frames): ~4 min each of every rest state, no step */
        uint32_t rest, mode, f, g, moved = 0;
        const uint8_t *m = FM1_ENC[0];
        for (mode = 0; mode < 3u; mode++)
            for (rest = 0; rest < 4u; rest++) {
                reset();
                for (f = 0, g = 0; f < 200000u; f++) {
                    uint32_t st = rest;
                    if (g) {
                        st ^= mode == 0u ? 2u : mode == 1u ? 1u : 3u;
                        g--;
                    } else if (rnd(300) == 0u) {
                        g = 1u + rnd(5);
                    }
                    if (f < 100u)
                        st = rest;                     /* (the power-on state is the detent) */
                    memset((void *)fm1_in.raw, 0, sizeof fm1_in.raw);
                    fm1_in.raw[m[0]] |= (uint8_t)(((st >> 1) & 1u) << m[1]);
                    fm1_in.raw[m[2]] |= (uint8_t)((st & 1u) << m[3]);
                    fm1__frame();
                    moved |= fm1_in.enc_steps[0] != 0;
                }
            }
        check("#63 a still knob: one contact chattering or both lost together never steps", !moved);
    }
    {   /* #126 turns at speed through the scan (a frame ~1.1 ms): every detent counted, 12-detent turns over every phase
         * of the scan; slow turns with bouncy contacts; fast ones each way; a reversal. Was: the decoder took a state
         * only when two frames in a row saw it, so above ~110 detents/s (even) or ~60/s (bunched) whole clicks went
         * missing (12 at 150/s -> ~8, at 200/s -> 0) */
        static const double EVEN[] = {50, 100, 150, 200, 250, 300}, BUNCH[] = {50, 100, 150};
        static const double SLOW[] = {3, 8, 20, 40};
        uint32_t r, ph, d, bad = 0, n = 0, worst = 0;
        char what[112];
        for (r = 0; r < 2u * 6u; r++)
            for (ph = 0; ph < 40u; ph++)
                for (d = 0; d < 2u; d++) {
                    int32_t dir = d ? -1 : 1, s;
                    double rate = r < 6u ? EVEN[r] : BUNCH[(r - 6u) % 3u];
                    if (r >= 6u + 3u)
                        continue;
                    reset();
                    en_clear();
                    s = en_run(en_turn(5000u + ph * 53u, 12, dir, 1e6 / rate, r < 6u ? 1.0 : 0.5, 0u, 0u) + 20000u);
                    n++;
                    if (s != 12 * dir) {
                        bad++;
                        if ((uint32_t)(s * dir < 12 ? 12 - s * dir : s * dir - 12) > worst)
                            worst = (uint32_t)(s * dir < 12 ? 12 - s * dir : s * dir - 12);
                    }
                }
        printf("encoder at speed: %u turns of 12 detents, %u miscounted (worst by %u)\n", (unsigned)n, (unsigned)bad, (unsigned)worst);
        check("#126 12-detent turns up to 300 detents/s (even) / 150/s (bunched in half a click): every detent", !bad);
        for (bad = 0, r = 0; r < 4u; r++)
            for (ph = 0; ph < 20u; ph++)
                for (d = 0; d < 2u; d++) {
                    int32_t dir = d ? -1 : 1;
                    reset();
                    en_clear();
                    bad += en_run(en_turn(5000u + ph * 71u, 6, dir, 1e6 / SLOW[r], ph & 1u ? 1.0 : 0.4, 1500u, 400u) + 20000u) != 6 * dir;
                }
        snprintf(what, sizeof what, "#126 slow turns (3..40 detents/s), contacts bouncing 1.5 ms: one step a click (%u off)", (unsigned)bad);
        check(what, !bad);
        for (bad = 0, ph = 0; ph < 40u; ph++) {
            uint32_t t;
            int32_t s1;
            reset();
            en_clear();
            t = en_turn(5000u + ph * 37u, 8, 1, 1e6 / 120.0, 0.7, 200u, 60u);   /* 8 right, 6 left at once */
            s1 = en_run(t + 2000u);
            t = en_turn(t + 2000u, 6, -1, 1e6 / 120.0, 0.7, 200u, 60u);
            reset();                                    /* (replayed from the start: the same scan) */
            enl[0].i = enl[1].i = enl[0].s = enl[1].s = 0;
            bad += s1 != 8 || en_run(t + 20000u) != 2;
        }
        check("#126 120 detents/s, bouncing 0.2 ms: 8 right then 6 left at once -> 8, then 2", !bad);
    }

    {   /* the LEDs through fm1_input_tick (the GPIO as memory): each tick writes the lines dark (the key read),
         * then, a dim-only LED on column p: lit | dim of p, the first bits of the shift, dark; the rest of the shift,
         * the latch (the lines dark), the lit LEDs of column n. Over bus speeds from 0 (the shift shorter than the
         * pulse: the rest waited) to 4 TIMER4 ticks an access (a bit ~1 us): the pulse within 30 % of
         * FM1_LED_DIM_NS on every column once settled, nothing of another column, ever */
        static const uint32_t BUS[] = {0u, 1u, 2u, 4u};
        uint32_t bi, lv;
        for (lv = 0; lv < 2u; lv++)                     /* both glows: DIM HI (FM1_LED_DIM_NS), DIM LO */
        for (bi = 0; bi < sizeof BUS / sizeof BUS[0]; bi++) {
            uint32_t t, col, prev, lit[FM1_NCOL][5], dimw[FM1_NCOL][5], frames = 400u, bad = 0, badw = 0, r;
            uint32_t pulse_min[FM1_NCOL], pulse_max = 0, pulses = 0, waits = 0, kmin = 16u, kmax = 0u, c, pmin = ~0u;
            const uint32_t NS = lv ? FM1_LED_DIM_LO_NS : FM1_LED_DIM_NS, T = (NS * FM1_TICKS_PER_US + 500u) / 1000u;
            char what[112];
            memset(lit, 0, sizeof lit);
            memset(dimw, 0, sizeof dimw);
            for (c = 0; c < FM1_NCOL; c++)
                pulse_min[c] = ~0u;
            reset();
            fm1_led_dim_level(lv);
            FM1_PR(FM1_PA, FM1_IN) = 0xFFFFFFFFu;   /* rows open */
            FM1_PR(FM1_PB, FM1_IN) = 0xFFFFFFFFu;   /* (one register a statement: FM1_PR advances host_now) */
            memset(fm1_led, 0, sizeof fm1_led);
            memset(fm1_led_dim, 0, sizeof fm1_led_dim);
            fm1_led[3] = 1u << 2;                     /* lit: column 3 row 2 */
            fm1_led_dim[3] = 1u << 2 | 1u << 4;       /* dim: column 3 rows 2 (also lit) and 4 */
            fm1_led_dim[4] = 0x1Eu;                   /* the next column: every row dim */
            fm1_led_dim[10] = 1u << 1;
            fm1_led_dim[0] = 1u << 3;
            fm1_led[7] = 1u << 3;                     /* a column with a lit LED and no dim one */
            host_step = 1u;                           /* a clock read: 1 tick (~42 ns) */
            host_bus = BUS[bi];
            latch_lit = latches = 0;
            for (t = 0; t < frames * FM1_NCOL; t++) {
                uint32_t want, d;
                prev = fm1__tick_col;
                led_nw = 0;
                fm1_input_tick();
                col = fm1__tick_col;
                want = fm1_led[col];
                d = fm1_led_dim[prev] & ~fm1_led[prev];
#if FM1_LED_DIM_DIV > 1
                if (fm1__dim_ph)                      /* (frames skipped: not the dim LEDs' turn) */
                    d = 0;
#endif
                /* the writes: 0 (the key read), [lit | dim of p, 0 (the pulse)], lit of n */
                badw += led_nw != (d ? 4u : 2u) || led_w[0] != 0u || led_w[led_nw - 1u] != want ||
                        (d && (led_w[1] != (fm1_led[prev] | d) || led_w[2] != 0u));
                if (d) {
                    uint32_t wt = led_wt[2] - led_wt[1];
                    bad += (led_w[1] & ~(uint32_t)(fm1_led[prev] | fm1_led_dim[prev])) != 0u;   /* column p's only */
                    pulses++;
                    if (t >= 20u * FM1_NCOL) {            /* settled (20 frames; 4 dim columns here) */
                        pulse_min[prev] = wt < pulse_min[prev] ? wt : pulse_min[prev];
                        pulse_max = wt > pulse_max ? wt : pulse_max;
                        pmin = wt < pmin ? wt : pmin;
                        kmin = fm1__dim_k < kmin ? fm1__dim_k : kmin;
                        kmax = fm1__dim_k > kmax ? fm1__dim_k : kmax;
                    }
                    for (r = 1; r < 5u; r++)
                        dimw[prev][r] += (d >> r) & 1u;
                }
                bad += (want & ~(uint32_t)fm1_led[col]) != 0u;   /* column n's lit only */
                for (r = 1; r < 5u; r++)
                    lit[col][r] += (led_w[led_nw - 1u] >> r) & 1u;
            }
            waits = kmax == 16u;
            for (c = 0; c < FM1_NCOL; c++)
                bad += pulse_min[c] != ~0u && (pulse_min[c] * 10u < T * 7u);
            snprintf(what, sizeof what, "LEDs, bus %u tick(s) an access: lit every frame, own column only, dark at the latch",
                     (unsigned)host_bus);
            check(what, !bad && !badw && !latch_lit && latches == frames * FM1_NCOL && lit[3][2] == frames &&
                  lit[7][3] == frames && !lit[2][2] && !lit[3][4] && !lit[4][1]);
            snprintf(what, sizeof what, "  dim %s: a pulse every frame on its own column, %u..%u ns (target %u +-30 %%)",
                     lv ? "LO" : "HI", (unsigned)(pmin * 1000u / FM1_TICKS_PER_US),
                     (unsigned)(pulse_max * 1000u / FM1_TICKS_PER_US), (unsigned)NS);
            check(what, pulses == frames / FM1_LED_DIM_DIV * 4u && dimw[3][4] == frames / FM1_LED_DIM_DIV &&
                  dimw[4][1] == frames / FM1_LED_DIM_DIV && dimw[4][4] == frames / FM1_LED_DIM_DIV &&
                  dimw[10][1] == frames / FM1_LED_DIM_DIV && dimw[0][3] == frames / FM1_LED_DIM_DIV &&
                  !dimw[3][2] && !dimw[5][1] && !dimw[9][1] && !dimw[7][3] &&
                  pmin * 10u >= T * 7u && pulse_max * 10u <= T * 13u);
            printf("LEDs %s, bus %u: the pulse spans %u..%u bits of the shift%s\n", lv ? "LO" : "HI", (unsigned)host_bus, (unsigned)kmin,
                   (unsigned)kmax, waits ? " (the shift shorter than the pulse: the rest waited)" : " (no wait)");
        }
        for (bi = 0; bi < 4u; bi++) {                   /* #119 the breath: bus 1 and 2 (a 16 and a ~12 bit pulse) x HI, LO */
            enum { BF = FM1_LED_BREATH_FRAMES, SETTLE = 20u, NF = 2u * FM1_LED_BREATH_FRAMES, WIN = 32u };
            /* the on-time of each LED a frame (TIMER4 ticks), from the trace of the line writes: before the latch they
             * drive column p (its pulse), the last one column n until the next tick's first write; the ticks 100 us apart */
            static uint32_t on[NF][FM1_NCOL][5], mean[NF / WIN];
            const uint32_t lo = bi >> 1, bus = 1u + (bi & 1u), TICK = FM1_LED_TICK_US * FM1_TICKS_PER_US;
            const uint32_t T = ((lo ? FM1_LED_DIM_LO_NS : FM1_LED_DIM_NS) * FM1_TICKS_PER_US + 500u) / 1000u;
            const uint32_t PK = lo ? FM1_LED_BREATH_PK_LO : FM1_LED_BREATH_PK;
            uint32_t t, f, c, r, i, bad = 0, badw = 0, wmin = ~0u, wmax = 0, after = 0, others = 0, step = 0, mono = 0;
            uint32_t lit = 0, peak = 0, first = 0, dark = 0, dimok = 0, run, run50 = 0, run25 = 0, runlit = 0, lastw = 0, lastt = 0;
            uint32_t tstart, frames = NF, n6 = 0;
            uint64_t litsum = 0;
            char what[160];
            reset();
            fm1_led_dim_level(lo);
            FM1_PR(FM1_PA, FM1_IN) = 0xFFFFFFFFu;
            FM1_PR(FM1_PB, FM1_IN) = 0xFFFFFFFFu;   /* (one register a statement: FM1_PR advances host_now) */
            memset(fm1_led, 0, sizeof fm1_led);
            memset(fm1_led_dim, 0, sizeof fm1_led_dim);
            memset(fm1_led_breath, 0, sizeof fm1_led_breath);
            fm1_led[3] = 1u << 2;                       /* column 3: row 2 lit (also set to breathe), row 4 breathes */
            fm1_led_dim[4] = 0x1Eu;                     /* column 4: every row dim, row 1 also set to breathe */
            fm1_led[7] = 1u << 3;                       /* column 7: row 3 lit (the reference), nothing else */
            host_step = 1u;
            host_bus = bus;
            for (t = 0; t < SETTLE * FM1_NCOL || fm1__tick_col; t++)   /* the glow settled, nothing breathing yet; */
                fm1_input_tick();                                  /* (then from column 0: frame 0 the first) */
            fm1_led_breath[3] = 1u << 2 | 1u << 4;
            fm1_led_breath[4] = 1u << 1;
            fm1_led_breath[6] = 1u << 1;                /* column 6: breathes alone (no dim, no lit) */
            memset(on, 0, sizeof on);
            latch_lit = 0;
            tstart = host_now + TICK;
            for (t = 0; t <= frames * FM1_NCOL; t++) {  /* (one tick more: the end of the last lit time) */
                uint32_t p = fm1__tick_col, n = p + 1u == FM1_NCOL ? 0u : p + 1u, pre;
                f = t / FM1_NCOL;
                if (host_now < tstart + t * TICK)
                    host_now = tstart + t * TICK;
                led_nw = 0;
                fm1_input_tick();
                if (t) {                                /* the last tick's last write lit column p until now */
                    uint32_t fl = (t - 1u) / FM1_NCOL;
                    for (r = 1; r < 5u; r++)
                        on[fl][p][r] += (lastw >> r & 1u) * (led_wt[0] - lastt);
                }
                if (t == frames * FM1_NCOL)
                    break;
                pre = fm1_led[p] | fm1_led_dim[p] | fm1_led_breath[p];
                for (i = 0; i + 1u < led_nw; i++) {     /* column p's writes (the key read, the pulse) */
                    bad += (led_w[i] & ~pre) != 0u;     /* its own LEDs only */
                    for (r = 1; r < 5u; r++)
                        on[f][p][r] += (led_w[i] >> r & 1u) * (led_wt[i + 1u] - led_wt[i]);
                }
                if (led_nw == 5u) {                     /* the pulse with the breath's write: the glow's width */
                    uint32_t pw = led_wt[3] - led_wt[1];
                    badw += led_w[0] != 0u || led_w[3] != 0u ||
                            (led_w[1] & (fm1_led[p] | (fm1_led_dim[p] & ~fm1_led[p]))) != (fm1_led[p] | (fm1_led_dim[p] & ~fm1_led[p]));
                    if (f >= 4u) {
                        wmin = pw < wmin ? pw : wmin;
                        wmax = pw > wmax ? pw : wmax;
                    }
                } else {
                    badw += led_nw != 2u || led_w[0] != 0u; /* no pulse: the key read, then column n */
                }
                /* column n: its lit ones, and its breathing ones (not dim) or none */
                badw += (led_w[led_nw - 1u] & fm1_led[n]) != fm1_led[n] ||
                        (led_w[led_nw - 1u] & ~(uint32_t)(fm1_led[n] | (fm1_led_breath[n] & ~fm1_led_dim[n]))) != 0u;
                lastw = led_w[led_nw - 1u];
                lastt = led_wt[led_nw - 1u];
            }
            for (f = 0; f < frames; f++) {
                lit = on[f][7][3];
                litsum += lit;
                /* in step (both columns 1..10): lit in the same frames, the pulses within half the glow (fm1__dim_k
                 * may differ by a bit between the two) */
                bad += (on[f][3][4] * 2u >= lit) != (on[f][6][1] * 2u >= lit) ||
                       (on[f][3][4] > on[f][6][1] ? on[f][3][4] - on[f][6][1] : on[f][6][1] - on[f][3][4]) * 2u > T;
                bad += on[f][3][2] * 10u < lit * 9u;    /* lit stays lit */
                dimok += on[f][4][1] == on[f][4][2] && on[f][4][1] * 8u < lit && on[f][4][1] > 0u;   /* dim: the glow */
                for (c = 0; c < FM1_NCOL; c++)
                    for (r = 1; r < 5u; r++)
                        others += on[f][c][r] && !((c == 3u && (r == 2u || r == 4u)) || c == 4u || (c == 6u && r == 1u) ||
                                                   (c == 7u && r == 3u));
            }
            lit = (uint32_t)(litsum / frames);          /* a lit LED's time a frame */
            for (i = 0; i < NF / WIN; i++) {            /* WIN-frame means of the breath (column 6), /1000 of lit */
                uint32_t s = 0;
                for (f = i * WIN; f < (i + 1u) * WIN; f++)
                    s += on[f][6][1];
                mean[i] = (uint32_t)((uint64_t)s * 1000u / ((uint64_t)lit * WIN));
                peak = mean[i] > peak ? mean[i] : peak;
                if (i) {
                    uint32_t dd = mean[i] > mean[i - 1u] ? mean[i] - mean[i - 1u] : mean[i - 1u] - mean[i];
                    step = dd > step ? dd : step;
                    /* frames 0..BF/2 fall, BF/2..BF rise, and again: 2 lit frames of slack (the sigma-delta) */
                    if (((i * WIN) % BF) < BF / 2u)
                        mono += mean[i] > mean[i - 1u] + 2000u / WIN && (i * WIN) % BF != 0u;
                    else
                        mono += mean[i] + 2000u / WIN < mean[i - 1u];
                }
            }
            first = mean[0];
            for (f = BF / 2u - 2u; f < BF / 2u + 2u; f++)
                dark += on[f][6][1];
            /* the flicker: the dark runs (frames not lit, on < half a lit frame) between two lit frames of the same half
             * (fall or rise), by the breath's mean where they are: over 50 % and 25 % of its peak, and at all */
            for (f = 0, i = ~0u; f < frames; f++) {
                if (on[f][6][1] * 2u < lit)
                    continue;
                n6++;
                if (i != ~0u && i / (BF / 2u) == f / (BF / 2u)) {
                    uint32_t m = mean[(i + f) / 2u / WIN];
                    run = f - i - 1u;
                    run50 = m * 2u >= peak && run > run50 ? run : run50;
                    run25 = m * 4u >= peak && run > run25 ? run : run25;
                    runlit = run > runlit ? run : runlit;
                }
                i = f;
            }
            /* then nothing breathes: the tick as before (the same writes) */
            memset(fm1_led_breath, 0, sizeof fm1_led_breath);
            for (t = 0; t < 2u * FM1_NCOL; t++) {
                uint32_t p = fm1__tick_col, n = p + 1u == FM1_NCOL ? 0u : p + 1u, d = fm1_led_dim[p] & ~fm1_led[p];
                led_nw = 0;
                fm1_input_tick();
                if (t >= FM1_NCOL)
                    after += led_nw != (d ? 4u : 2u) || led_w[led_nw - 1u] != fm1_led[n] ||
                             (d && (led_w[1] != (fm1_led[p] | d) || led_w[2] != 0u));
            }
            snprintf(what, sizeof what, "#119 breath %s, bus %u: own column, the writes, lit / dim never breathing, in step",
                     lo ? "LO" : "HI", (unsigned)bus);
            check(what, !bad && !badw && !latch_lit && !others && dimok == frames && !after && !fm1__br_on && !fm1__br_lit);
            snprintf(what, sizeof what, "  peak %u.%u %% of lit (%u..%u), from the peak (%u.%u %%), dark at half a period",
                     (unsigned)(peak / 10u), (unsigned)(peak % 10u), lo ? 25u : 50u, lo ? 35u : 70u, (unsigned)(first / 10u),
                     (unsigned)(first % 10u));
            check(what, peak >= (lo ? 250u : 500u) && peak <= (lo ? 350u : 700u) && first * 10u >= peak * 9u && !dark &&
                  peak * 256u + 7680u >= PK * 1000u && peak * 256u <= PK * 1000u + 7680u);
            snprintf(what, sizeof what, "  smooth: %u-frame means step <= %u.%u %% of lit (<= 15 %% of the peak + 2 frames), monotonic",
                     (unsigned)WIN, (unsigned)(step / 10u), (unsigned)(step % 10u));
            check(what, step * 100u <= peak * 15u + 200000u / WIN && !mono);
            snprintf(what, sizeof what, "  flicker: dark runs %u fr over 50 %% of the peak (<= 9: ~10 ms), %u over 25 %%, %u at all",
                     (unsigned)run50, (unsigned)run25, (unsigned)runlit);
            check(what, run50 <= 9u && n6 > 0u);
            snprintf(what, sizeof what, "  the glow still %u..%u ns (target %u +-30 %%)", (unsigned)(wmin * 1000u / FM1_TICKS_PER_US),
                     (unsigned)(wmax * 1000u / FM1_TICKS_PER_US), (unsigned)(T * 1000u / FM1_TICKS_PER_US));
            check(what, wmin * 10u >= T * 7u && wmax * 10u <= T * 13u);
        }
        for (bi = 0; bi < 2u; bi++) {                   /* 1.1 the mid level (fm1_led_mid, the DRUM grid's beats): HI, LO */
            enum { MF = 240u };                         /* (a multiple of 8 and 12) */
            static uint32_t on[MF][FM1_NCOL][5];
            const uint32_t lo = bi, N = lo ? FM1_LED_MID_N_LO : FM1_LED_MID_N, TICK = FM1_LED_TICK_US * FM1_TICKS_PER_US;
            uint32_t t, f, c, r, i, bad = 0, others = 0, nlit = 0, gap = 0, last = ~0u, glow = 0, lit = 0, mean, dark = 0;
            uint32_t lastw = 0, lastt = 0, tstart;
            uint64_t sum6 = 0, litsum = 0;
            char what[160];
            reset();
            fm1_led_dim_level(lo);
            FM1_PR(FM1_PA, FM1_IN) = 0xFFFFFFFFu;
            FM1_PR(FM1_PB, FM1_IN) = 0xFFFFFFFFu;   /* (one register a statement: FM1_PR advances host_now) */
            memset(fm1_led, 0, sizeof fm1_led);
            memset(fm1_led_dim, 0, sizeof fm1_led_dim);
            memset(fm1_led_breath, 0, sizeof fm1_led_breath);
            memset(fm1_led_mid, 0, sizeof fm1_led_mid);
            fm1_led[7] = 1u << 3;                       /* the reference: lit */
            fm1_led_dim[6] = 1u << 1;                   /* column 6 row 1: the mid level over the glow (as ui_leds) */
            fm1_led_mid[6] = 1u << 1;
            fm1_led_mid[5] = 1u << 2;                   /* column 5 row 2: mid alone (lit frames, dark between) */
            host_step = 1u;
            host_bus = 1u;
            for (t = 0; t < 20u * FM1_NCOL || fm1__tick_col; t++)
                fm1_input_tick();
            memset(on, 0, sizeof on);
            latch_lit = 0;
            tstart = host_now + TICK;
            for (t = 0; t <= MF * FM1_NCOL; t++) {
                uint32_t p = fm1__tick_col, pre;
                f = t / FM1_NCOL;
                if (host_now < tstart + t * TICK)
                    host_now = tstart + t * TICK;
                led_nw = 0;
                fm1_input_tick();
                if (t) {
                    uint32_t fl = (t - 1u) / FM1_NCOL;
                    for (r = 1; r < 5u; r++)
                        on[fl][p][r] += (lastw >> r & 1u) * (led_wt[0] - lastt);
                }
                if (t == MF * FM1_NCOL)
                    break;
                pre = fm1_led[p] | fm1_led_dim[p] | fm1_led_mid[p];
                for (i = 0; i + 1u < led_nw; i++) {
                    bad += (led_w[i] & ~pre) != 0u;
                    for (r = 1; r < 5u; r++)
                        on[f][p][r] += (led_w[i] >> r & 1u) * (led_wt[i + 1u] - led_wt[i]);
                }
                lastw = led_w[led_nw - 1u];
                lastt = led_wt[led_nw - 1u];
            }
            for (f = 0; f < MF; f++)
                litsum += on[f][7][3];
            lit = (uint32_t)(litsum / MF);
            for (f = 0; f < MF; f++) {
                uint32_t a = on[f][6][1] * 2u >= lit, b = on[f][5][2] * 2u >= lit;
                sum6 += on[f][6][1];
                bad += a != b;                          /* in step on both columns */
                if (a) {
                    nlit++;
                    if (last != ~0u && f - last != N) gap++;
                    last = f;
                    bad += on[f][6][1] * 10u < lit * 9u || on[f][5][2] * 10u < lit * 9u;   /* as lit */
                } else {
                    glow += on[f][6][1] > 0u && on[f][6][1] * 8u < lit;   /* the glow between */
                    dark += on[f][5][2] == 0u;                            /* mid alone: dark between */
                }
                for (c = 0; c < FM1_NCOL; c++)
                    for (r = 1; r < 5u; r++)
                        others += on[f][c][r] && !((c == 7u && r == 3u) || (c == 6u && r == 1u) || (c == 5u && r == 2u));
            }
            mean = (uint32_t)(sum6 * 1000u / ((uint64_t)lit * MF));
            snprintf(what, sizeof what, "mid level %s: lit 1 frame in %u, evenly (every %u frames, ~%u Hz), the glow between, in step",
                     lo ? "LO" : "HI", (unsigned)N, (unsigned)N, (unsigned)(1000000u / (N * FM1_NCOL * FM1_LED_TICK_US)));
            check(what, !bad && !others && !latch_lit && nlit == MF / N && !gap && glow == MF - nlit && dark == MF - nlit);
            snprintf(what, sizeof what, "  its level %u.%u %% of lit (1/%u + the glow), above the glow, below a breath's peak",
                     (unsigned)(mean / 10u), (unsigned)(mean % 10u), (unsigned)N);
            check(what, mean * N >= 1000u && mean * N <= 1000u + 1000u * N / 12u && mean * 256u < (lo ? FM1_LED_BREATH_PK_LO : FM1_LED_BREATH_PK) * 1000u);
            memset(fm1_led_mid, 0, sizeof fm1_led_mid);
        }
        anim_test();
        idle_anim_test();
        memset(fm1_led_breath, 0, sizeof fm1_led_breath);
        host_bus = 0;
        host_step = 2400u;
        fm1_led_dim_level(0);
        printf("LEDs: refresh %u Hz lit, %u Hz dim\n", (unsigned)(1000000u / (FM1_NCOL * TICK_US)),
               (unsigned)(1000000u / (FM1_NCOL * TICK_US * FM1_LED_DIM_DIV)));
        {   /* the host cost of a tick, the dim LEDs off, on, on and breathing (no bus time: the shift waits out the pulse) */
            uint32_t k, t, n = 2000000u;
            double ns[3];
            host_step = 1000u;                       /* (the clock far ahead on each read: no wait on the host) */
            for (k = 0; k < 3u; k++) {
                clock_t c0;
                memset(fm1_led_dim, k ? 0x1E : 0, sizeof fm1_led_dim);
                memset(fm1_led_breath, k == 2u ? 0x1E : 0, sizeof fm1_led_breath);
                if (k == 2u)
                    memset(fm1_led_dim, 0x0E, sizeof fm1_led_dim);   /* (row 4 breathes) */
                c0 = clock();
                for (t = 0; t < n; t++) {
                    led_nw = 0;
                    fm1_input_tick();
                }
                ns[k] = (double)(clock() - c0) * 1e9 / CLOCKS_PER_SEC / n;
            }
            host_step = 2400u;
            memset(fm1_led_breath, 0, sizeof fm1_led_breath);
            printf("LEDs: host %.1f ns per tick without dim LEDs, %.1f ns with, %.1f ns with a breath too\n", ns[0], ns[1],
                   ns[2]);
        }
    }

    if (fails) {
        printf("INPUT TEST FAILED (%d)\n", fails);
        return 1;
    }
    printf("input debounce: all ok\n");
    return 0;
}
