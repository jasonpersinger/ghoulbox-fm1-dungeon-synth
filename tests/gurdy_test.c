/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GHOULBOX (on Felucca by Leo Kuroshita, Hügelton Instruments) */
/* GURDY (src/eng_gurdy.c, engine 14) on the host, the voices through voice.c as on the device:
 * 1. the bourdon sits on DRN whatever key is played (D2 under C4 and under G4, as loud), and moves with DRN
 *    (C2 at DRN C, no D2); the chanterelle follows the key.
 * 2. the trompette's strokes: at COUP 1/8 (120 BPM) the buzz (> 2 kHz) is loud at each stroke's start and gone
 *    by its end; at HOLD it is steady; at BUZZ 0 there is none.
 * 3. one wheel: two keys held sound one voice.
 * 4. the corners: every EDIT at 0 and 127, notes C1 .. C7: bounded, no DC.
 * 5. cost (instructions per sample of a voice; the Linux counter). */
#define main hostsim_main
#include "hostsim.c"
#undef main
#ifdef __linux__
#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

static int bad;
static void check(const char *what, int ok)
{
    printf("gurdy: %-96s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static uint64_t instr_now(void)
{
#ifdef __linux__
    static int fd = -2;
    uint64_t v;
    if (fd == -2) {
        struct perf_event_attr a;
        memset(&a, 0, sizeof a);
        a.type = PERF_TYPE_HARDWARE;
        a.size = sizeof a;
        a.config = PERF_COUNT_HW_INSTRUCTIONS;
        a.exclude_kernel = a.exclude_hv = 1;
        fd = (int)syscall(SYS_perf_event_open, &a, 0, -1, -1, 0);
    }
    if (fd >= 0 && read(fd, &v, sizeof v) == (ssize_t)sizeof v)
        return v;
#endif
    return 0;
}

#define N (2u * FS / CTL * CTL)
static double y[N];

/* EDIT e (DRN DLVL 5TH WHL BUZZ COUP BODY WOBL), notes held, N samples after 0.2 s */
static void play(const int16_t *e, const uint8_t *notes, uint32_t nn)
{
    static int32_t b[CTL];
    uint32_t i, k;
    host_tracks_init();
    song.g[G_BPM] = 120;
    host_preset(&trk[0], ENGI_GURDY, 0);
    for (i = 0; i < 8u; i++)
        trk[0].p[P_E0 + i] = e[i];
    trk[0].p[P_ATK] = 0;
    trk[0].p[P_SUS] = 127;
    trk[0].p[P_REL] = 10;
    trk[0].p[P_VOICE] = V_POLY;
    for (i = 0; i < nn; i++)
        trk_note_on(&trk[0], notes[i], 100);
    for (i = 0; i < FS / 5u / CTL; i++)
        track_render(&trk[0], b, CTL);
    for (i = 0; i < N; i += CTL) {
        track_render(&trk[0], b, CTL);
        for (k = 0; k < CTL; k++)
            y[i + k] = b[k] / 32768.0;
    }
}
static double tone(double hz)                           /* Goertzel magnitude of y at hz, Hann window */
{
    double w = 2 * M_PI * hz / FS, c = 2 * cos(w), s0, s1 = 0, s2 = 0;
    uint32_t i;
    for (i = 0; i < N; i++) {
        s0 = y[i] * (0.5 - 0.5 * cos(2 * M_PI * i / N)) + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) / N;
}
static double midi_hz(int n) { return 440.0 * pow(2, (n - 69) / 12.0); }

int main(void)
{
    char what[200];
    uint32_t i;
    {   /* 1 */
        static const int16_t E[8] = {2, 127, 0, 60, 0, 0, 0, 0};
        static const int16_t EC[8] = {0, 127, 0, 60, 0, 0, 0, 0};
        static const uint8_t C4[1] = {60}, G4[1] = {67};
        double d1, d2, m1, m2, c2, dd;
        play(E, C4, 1); d1 = tone(midi_hz(38)); m1 = tone(midi_hz(60));
        play(E, G4, 1); d2 = tone(midi_hz(38)); m2 = tone(midi_hz(67));
        play(EC, C4, 1); c2 = tone(midi_hz(36)); dd = tone(midi_hz(38));
        snprintf(what, sizeof what, "drone: D2 under C4 %.4f, under G4 %.4f; the keys' own C4 %.4f, G4 %.4f", d1, d2, m1, m2);
        check(what, d1 > 0.01 && fabs(20 * log10(d1 / d2)) < 1.0 && m1 > 0.01 && m2 > 0.01);
        snprintf(what, sizeof what, "  DRN C: C2 %.4f, D2 %.4f (gone)", c2, dd);
        check(what, c2 > 0.01 && dd < c2 / 30);
    }
    {   /* 2 */
        static const uint8_t D4[1] = {62};
        int16_t e[8] = {2, 0, 0, 0, 127, 2, 0, 0};       /* buzz alone (no bourdon, no fifth, little melody) */
        double on, off, ratio[3];
        uint32_t per = (uint32_t)FS * 60u / 120u / 2u, k, m;
        for (m = 0; m < 3u; m++) {
            double prev = 0, hp;
            e[5] = m == 1u ? 0 : 2;                     /* 1/8, HOLD, 1/8 at BUZZ 0 */
            e[4] = m == 2u ? 0 : 127;
            play(e, D4, 1);
            on = off = 0;
            for (i = 1; i < N; i++) {                   /* > 2 kHz: a first difference, twice */
                hp = y[i] - y[i - 1];
                k = (i + FS / 5u) % per;                /* (the strokes count from the note-on, 0.2 s before y) */
                *(k < per / 4u ? &on : k > per / 2u ? &off : &prev) += (hp - prev) * (hp - prev);
                prev = hp;
            }
            ratio[m] = 10 * log10((on / (per / 4.0)) / (off / (per / 2.0 - 1) + 1e-30) + 1e-30);
        }
        snprintf(what, sizeof what, "trompette: stroke start vs end, the buzz %.1f dB at COUP 1/8, %.1f dB at HOLD, %.1f dB at BUZZ 0",
                 ratio[0], ratio[1], ratio[2]);
        check(what, ratio[0] >= 10.0 && fabs(ratio[1]) < 2.0 && fabs(ratio[2]) < 2.0);
    }
    {   /* 3 */
        static const int16_t E[8] = {2, 90, 50, 80, 60, 2, 70, 40};
        static const uint8_t TWO[2] = {60, 64};
        play(E, TWO, 2);
        snprintf(what, sizeof what, "one wheel: two keys held, %u voice(s) sounding", busy_now());
        check(what, busy_now() == 1u);
    }
    {   /* 4 */
        int32_t worst = 0;
        double dc = 0;
        uint32_t c, n;
        for (c = 0; c < 256u; c++) {
            int16_t e[8];
            uint8_t note[1];
            for (i = 0; i < 8u; i++)
                e[i] = (int16_t)((c >> i) & 1u ? (i == 0u ? 11 : i == 5u ? 4 : 127) : 0);
            for (n = 24; n <= 96; n += 36) {
                double sum = 0, pk = 0;
                note[0] = (uint8_t)n;
                play(e, note, 1);
                for (i = 0; i < N; i++) {
                    sum += y[i];
                    pk = fabs(y[i]) > pk ? fabs(y[i]) : pk;
                }
                worst = (int32_t)(pk * 32768) > worst ? (int32_t)(pk * 32768) : worst;
                dc = fabs(sum / N) > dc ? fabs(sum / N) : dc;
            }
        }
        snprintf(what, sizeof what, "corners: 256 EDIT corners x C1 / C4 / C7: peak %d, worst DC %.2f %% of full scale", worst, dc * 100);
        check(what, worst < 4 * 32768 && dc < 0.02);
    }
    {   /* 5 */
        static const int16_t E[8] = {2, 90, 50, 80, 70, 2, 70, 40};
        static const uint8_t D4[1] = {62};
        static int32_t b[CTL];
        uint64_t i0;
        play(E, D4, 1);
        i0 = instr_now();
        for (i = 0; i < 4u * FS / CTL; i++)
            track_render(&trk[0], b, CTL);
        if (i0) {
            double c = (double)(instr_now() - i0) / (4.0 * FS);
            snprintf(what, sizeof what, "cost: a buzzing voice, %.0f instructions per sample (limit 300; one voice: poly 1)", c);
            check(what, c <= 300);
        } else
            printf("gurdy: cost: no instruction counter on this host\n");
    }
    printf("%s\n", bad ? "GURDY TEST FAILED" : "gurdy test passed");
    return bad != 0;
}
