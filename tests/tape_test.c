/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments; GHOULBOX changes 2026 */
/* TAPE / CRSH (src/tape.c, GHOULBOX) on the host, through hostsim.c:
 * 1. off (both 0): the mix untouched, sample for sample.
 * 2. wow and flutter: the period of a 1 kHz sine wanders 0.3 .. 1.5 % at TAPE 127, less (but some) at 64.
 * 3. the high cut: 10 kHz at least 12 dB below 500 Hz at TAPE 127; within 3 dB at TAPE 1.
 * 4. hiss: silence in -> -75 .. -45 dB RMS out at TAPE 127 (Q15 full scale).
 * 5. bounded: a 2^21 square (far over any mix) -> at most 2^17 out.
 * 6. CRSH 127: held 6 samples, 512-steps (7 bits of 16).
 * 7. switching TAPE on in the middle of a loud sine: no burst over the input's peak.
 * 8. cost (instructions per sample; the Linux / macOS counter). */
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
    printf("tape: %-96s %s\n", what, ok ? "ok" : "FAIL");
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

#define N (8u * FS / CTL * CTL)
static int32_t L[N], R[N];
static void sine(double hz, double amp)
{
    uint32_t i;
    for (i = 0; i < N; i++)
        L[i] = R[i] = (int32_t)(amp * sin(2 * M_PI * hz * i / FS));
}
static void run(int32_t tape, int32_t crsh)
{
    uint32_t b;
    tp.on = 0;
    song.g[G_TAPE] = (int16_t)tape;
    song.g[G_CRSH] = (int16_t)crsh;
    for (b = 0; b < N; b += CTL)
        tape_block(L + b, R + b, CTL);
}
static double rms_db(const int32_t *x, uint32_t a, uint32_t n)
{
    double s = 0;
    uint32_t i;
    for (i = a; i < a + n; i++)
        s += (double)x[i] * x[i];
    return 10 * log10(s / n / (32768.0 * 32768.0) + 1e-30);
}
static double wander(void)                              /* the periods' largest relative deviation, after 1 s */
{
    double last = -1, lo = 1e9, hi = 0;
    uint32_t i;
    for (i = FS; i < N; i++)
        if (L[i - 1] < 0 && L[i] >= 0) {
            double t = i - 1 + (double)-L[i - 1] / (L[i] - L[i - 1]);
            if (last >= 0) {
                lo = t - last < lo ? t - last : lo;
                hi = t - last > hi ? t - last : hi;
            }
            last = t;
        }
    return (hi - lo) / ((hi + lo) / 2) / 2 * 100;
}

int main(void)
{
    char what[200];
    uint32_t i;
    host_tracks_init();
    {   /* 1 */
        uint32_t diff = 0;
        sine(440, 20000);
        run(0, 0);
        for (i = 0; i < N; i++)
            diff += L[i] != (int32_t)(20000 * sin(2 * M_PI * 440 * i / FS));
        check("off (TAPE 0, CRSH 0): the mix untouched", !diff && !tp.on);
    }
    {   /* 2 */
        double w127, w64;
        sine(1000, 16000);
        run(127, 0);
        w127 = wander();
        sine(1000, 16000);
        run(64, 0);
        w64 = wander();
        snprintf(what, sizeof what, "wow / flutter: a 1 kHz period wanders %.2f %% at TAPE 127, %.2f %% at 64", w127, w64);
        check(what, w127 >= 0.3 && w127 <= 1.5 && w64 < w127 && w64 > 0.03);
    }
    {   /* 3 */
        double lo, hi, lo1, hi1;
        sine(500, 8000); run(127, 0); lo = rms_db(L, FS, 4u * FS);
        sine(10000, 8000); run(127, 0); hi = rms_db(L, FS, 4u * FS);
        sine(500, 8000); run(1, 0); lo1 = rms_db(L, FS, 4u * FS);
        sine(10000, 8000); run(1, 0); hi1 = rms_db(L, FS, 4u * FS);
        snprintf(what, sizeof what, "high cut: 10 kHz %.1f dB under 500 Hz at TAPE 127, %.1f dB at TAPE 1", lo - hi, lo1 - hi1);
        check(what, lo - hi >= 12.0 && lo1 - hi1 <= 3.0);
    }
    {   /* 4 */
        double h;
        memset(L, 0, sizeof L); memset(R, 0, sizeof R);
        run(127, 0);
        h = rms_db(L, FS, 4u * FS);
        snprintf(what, sizeof what, "hiss: silence in, %.1f dB RMS out at TAPE 127", h);
        check(what, h >= -75.0 && h <= -45.0);
    }
    {   /* 5 */
        int32_t pk = 0;
        for (i = 0; i < N; i++)
            L[i] = R[i] = (i / 50u) & 1u ? 1 << 21 : -(1 << 21);
        run(127, 0);
        for (i = 0; i < N; i++)
            pk = abs(L[i]) > pk ? abs(L[i]) : pk;
        snprintf(what, sizeof what, "bounded: a 2^21 square in, peak %d out (limit 2^17)", pk);
        check(what, pk <= 1 << 17);
    }
    {   /* 6 */
        uint32_t held = 1, steps = 1;
        for (i = 0; i < N; i++)
            L[i] = R[i] = (int32_t)(i * 1000u) - 300000;   /* (steep: each hold a new step) */
        run(0, 127);
        for (i = 1; i < 600u; i++) {
            held &= (L[i] == L[i - 1]) == (i % 6u != 0u);
            steps &= L[i] % 512 == 0;
        }
        check("CRSH 127: each value held 6 samples, on 512-steps (7 bits)", held && steps);
    }
    {   /* 7 */
        int32_t pk = 0;
        uint32_t b;
        sine(220, 30000);
        tp.on = 0;
        song.g[G_CRSH] = 0;
        for (b = 0; b < N; b += CTL) {
            song.g[G_TAPE] = b < N / 2u ? 0 : 90;
            tape_block(L + b, R + b, CTL);
            if (b >= N / 2u - 4u * CTL)
                for (i = b; i < b + CTL; i++)
                    pk = abs(L[i]) > pk ? abs(L[i]) : pk;
        }
        snprintf(what, sizeof what, "TAPE switched on under a 30000 sine: peak %d (the input's 30000)", pk);
        check(what, pk <= 31000);
    }
    {   /* 8 */
        uint64_t i0;
        double c;
        sine(440, 20000);
        tp.on = 0;
        i0 = instr_now();
        run(90, 60);
        c = i0 ? (double)(instr_now() - i0) / N : 0;
        if (c) {
            snprintf(what, sizeof what, "cost: TAPE 90 + CRSH 60, %.0f instructions per stereo sample (limit 250: once per mix, under one ANALOG voice on the device)", c);
            check(what, c <= 250);
        } else
            printf("tape: cost: no instruction counter on this host\n");
    }
    printf("%s\n", bad ? "TAPE TEST FAILED" : "tape test passed");
    return bad != 0;
}
