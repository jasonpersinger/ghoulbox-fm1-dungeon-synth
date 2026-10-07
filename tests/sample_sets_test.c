/* SPDX-License-Identifier: GPL-3.0-only */
/* GHOULBOX 1.1: the CC0 sets PSALTERY, RENORGAN, RECORDER, FOLKHARP (SET 8..11): each zone sounds, the looped ones
 * hold without a click at the seam, the harp decays to silence, the four fit their flash budget */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok) { printf("sets: %-70s %s\n", what, ok ? "ok" : "FAIL"); bad += !ok; }

/* SET value v, note n held `hold` s then released, `tail` s more: the output (mono, int32) */
static int32_t *render(uint32_t v, uint32_t n, double hold, double tail, uint32_t *len)
{
    uint32_t t, i, frames = (uint32_t)((hold + tail) * FS), off = (uint32_t)(hold * FS);
    int32_t o[2 * CTL], *x = calloc(frames + CTL, sizeof *x);
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    host_preset(&trk[0], ENGI_SAMPLE, 0);
    trk[0].p[P_E0] = (int16_t)v;
    trk[0].p[P_REV] = 0; trk[0].p[P_DLY] = 0; trk[0].p[P_CHOR] = 0;
    trk_note_on(&trk[0], n, 100);
    for (t = 0; t < frames; t += CTL) {
        if (t >= off && t < off + CTL) trk_note_off(&trk[0], n);
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) x[t + i] = o[2 * i];
    }
    *len = frames;
    return x;
}
static double rms(const int32_t *x, uint32_t a, uint32_t b)
{
    double s = 0;
    for (uint32_t i = a; i < b; i++) s += (double)x[i] * x[i];
    return sqrt(s / (b - a));
}
/* the largest sample-to-sample step over [a, b) against the typical one: a loop click is an outlier */
static double click_ratio(const int32_t *x, uint32_t a, uint32_t b)
{
    double mx = 0, s = 0;
    for (uint32_t i = a + 1; i < b; i++) {
        double d = fabs((double)x[i] - x[i - 1]);
        if (d > mx) mx = d;
        s += d * d;
    }
    return mx / sqrt(s / (b - a - 1) + 1e-9);
}

int main(void)
{
    static const struct { uint32_t v; const char *name; int looped; } SET[] = {
        {8, "PSALTERY", 1}, {9, "RENORGAN", 1}, {10, "RECORDER", 1}, {11, "FOLKHARP", 0}};
    uint32_t k, z, len, bytes = 0;
    for (k = 0; k < 4u; k++) {
        uint32_t s = SET[k].v < SMP_NALL ? smp_set_at(SET[k].v) : SMP_NONE;   /* (a value past the list: no such set) */
        const smp_set_t *set = &SMP_SETS[s == SMP_NONE ? 0u : s];
        int ok = s != SMP_NONE && str_eq(SMP_ALL_NAMES[SET[k].v], SET[k].name) && set->nz >= 3u;
        if (!ok) { check(SET[k].name, 0); continue; }
        for (z = 0; ok && z < set->nz; z++) {               /* every zone sounds at its root */
            const smp_zone_t *zn = &SMP_ZONES[set->z0 + z];
            int32_t *x = render(SET[k].v, (uint32_t)(zn->root16 / 16), 1.0, 0.5, &len);
            ok &= rms(x, FS / 10, FS / 2) > 300.0;
            bytes += (zn->n + 1u) / 2u;
            free(x);
        }
        check(SET[k].name, ok);
        if (SET[k].looped) {                                 /* each zone held 5 s: past several loop seams, no click */
            int okz = 1;
            for (z = 0; z < set->nz; z++) {
                int32_t *x = render(SET[k].v, (uint32_t)(SMP_ZONES[set->z0 + z].root16 / 16), 5.0, 0.2, &len);
                double r = click_ratio(x, FS, 5 * FS);
                printf("sets:   %s zone %u click ratio %.1f\n", SET[k].name, z, r);
                okz &= r < 12.0 && rms(x, 4 * FS, 5 * FS) > 300.0;
                free(x);
            }
            check("  every zone held 5 s: no click at the loop seam (ratio < 12)", okz);
        } else {                                             /* plucked: rings, then silence */
            int32_t *x = render(SET[k].v, 62, 3.0, 0.5, &len);
            check("  plucked: decays to silence", rms(x, FS / 20, FS / 5) > 300.0 && rms(x, len - FS / 5, len) < 30.0);
            free(x);
        }
    }
    printf("sets: the four use %u B of flash\n", bytes);
    check("the four fit their budget (<= 130 KB)", bytes <= 130u * 1024u);
    check("1.0's values unchanged: PIANO 0, FLUTE 2, SAX 3, USR1 5",
          str_eq(SMP_ALL_NAMES[0], "PIANO") && str_eq(SMP_ALL_NAMES[2], "FLUTE") && str_eq(SMP_ALL_NAMES[3], "SAX") &&
          str_eq(SMP_ALL_NAMES[5], "USR1") && smp_set_at(2) == 2u);
    puts(bad ? "SAMPLE SETS TEST FAILED" : "sample sets passed");
    return bad != 0;
}
