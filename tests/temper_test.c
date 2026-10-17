/* SPDX-License-Identifier: GPL-3.0-only */
/* GHOULBOX: TEMP (P_TEMP, SCL > CHORD's KNOB 3), the track's temperament from its ROOT (dsp.c temper). Every note a
 * voice plays, as voice.c turns it into a phase increment, against the temperament worked out here from its ratios
 * (not from the firmware's table): PYTH a chain of pure fifths (3:2), MEAN quarter-comma meantone (fifths of 5^(1/4)),
 * JUST 5-limit with the minor seventh 16:9; each from ROOT, the wolf between the sharp fourth and the flat second.
 * EQUAL leaves every voice as before (pitch16 = note x 16, no fine: the goldens hold the sound). Also the stored
 * layout: Felucca 1.5's seven values reserved before TEMP, with their defaults. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <math.h>

static int bad;
static void check(const char *what, int ok)
{
    printf("temper: %-86s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

/* fifths from ROOT for each semitone above it: the flat side down to the flat second, the sharp side up to the
 * sharp fourth */
static const int FIFTHS[12] = {0, -5, 2, -3, 4, -1, 6, 1, -4, 3, -2, 5};
static const double JUST[12][2] = {{1, 1}, {16, 15}, {9, 8}, {6, 5}, {5, 4}, {4, 3}, {45, 32}, {3, 2}, {8, 5},
                                   {5, 3}, {16, 9}, {15, 8}};

/* the cents above ROOT of degree d in temperament k (1 PYTH, 2 MEAN, 3 JUST; 0 EQUAL) */
static double ideal(uint32_t k, uint32_t d)
{
    double c;
    if (k == 3u)
        return 1200.0 * log2(JUST[d][0] / JUST[d][1]);
    if (k == 0u)
        return 100.0 * d;
    c = FIFTHS[d] * (k == 1u ? 1200.0 * log2(1.5) : 1200.0 * log2(5.0) / 4.0);
    return c - 1200.0 * floor(c / 1200.0);
}

/* the voice's pitch as voice.c renders it (no TUNE, bend or LFO): cents away from equal temperament's note */
static double voice_cents(const voice_t *v)
{
    uint32_t inc = pitch_inc((uint32_t)clamp(v->pitch16, 0, 2047));
    int32_t fine = v->fine + v->tfine;
    if (fine)
        inc += (uint32_t)((int32_t)(inc >> 12) * fine);
    return 1200.0 * log2((double)inc / pitch_inc((uint32_t)v->note * 16u));
}

static voice_t *sounding(track_t *t, uint32_t note)
{
    for (uint32_t i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate && t->v[i].note == note)
            return &t->v[i];
    return 0;
}

static void silence(track_t *t)
{
    memset(t->v, 0, sizeof t->v);
    t->nmono = 0;
    t->mono_note = 0;
}

int main(void)
{
    static const char *const NAMES[] = {"EQUAL", "PYTH", "MEAN", "JUST"};
    track_t *t = &trk[0];
    uint32_t k, root, n, ok;
    double worst = 0;
    host_tracks_init();
    host_preset(t, 0, 0);                               /* ANALOG */

    check("layout: P_TEMP after Felucca 1.5's seven (96..102), then RING (2.3), P_E0 105, P_COUNT 113",
          P_TEMP == 103 && P_RING == 104 && P_E0 == 105 && P_COUNT == 113);
    check("  the seven reserved at Felucca 1.5.1's defaults (INSRT 0, A 64, B 96, C 96, MIX 127, TYPE 0, ESYNC 0)",
          TP[96].def == 0 && TP[97].def == 64 && TP[98].def == 96 && TP[99].def == 96 && TP[100].def == 127 &&
          TP[101].def == 0 && TP[102].def == 0);
    for (k = 0, ok = TP[P_TEMP].max == 3 && TP[P_TEMP].def == 0 && str_eq(TP[P_TEMP].label, "TEMP"); k < 4u; k++)
        ok &= str_eq(TP[P_TEMP].names[k], NAMES[k]);
    check("TEMP: EQUAL (default, stored 0), PYTH, MEAN, JUST", ok);

    t->p[P_VOICE] = V_POLY;
    for (k = 0; k < 4u; k++) {
        double w = 0;
        t->p[P_TEMP] = (int16_t)k;
        for (root = 0, ok = 1; root < 12u; root++) {
            t->p[P_ROOT] = (int16_t)root;
            for (n = 36; n < 96; n++) {
                voice_t *v;
                double want, got;
                silence(t);
                trk_note_on(t, n, 100);
                v = sounding(t, n);
                if (!v) { ok = 0; continue; }
                want = ideal(k, (n + 12u - root) % 12u) - 100.0 * ((n + 12u - root) % 12u);
                got = voice_cents(v);
                w = fabs(got - want) > w ? fabs(got - want) : w;
                if (k == 0u)
                    ok &= v->pitch16 == (int32_t)n * 16 && v->tfine == 0 && v->fine == 0;
            }
        }
        silence(t);
        worst = w > worst ? w : worst;
        printf("temper:   %-5s every ROOT, notes 36..95: within %.3f ct of its ratios\n", NAMES[k], w);
        check(k == 0u ? "EQUAL: every voice exactly as before (note x 16, no fine)" : "  within 0.6 ct", ok && w < 0.6);
    }

    /* the intervals that make each one: from D (ROOT 2), D4 = 62 */
    {
        double c[4][12];
        t->p[P_ROOT] = 2;
        for (k = 1; k < 4u; k++) {
            t->p[P_TEMP] = (int16_t)k;
            for (n = 0; n < 12u; n++) {
                silence(t);
                trk_note_on(t, 62u + n, 100);
                c[k][n] = n * 100.0 + voice_cents(sounding(t, 62u + n));
            }
        }
        silence(t);
        check("PYTH: D-A a pure fifth (702 ct)", fabs(c[1][7] - 701.955) < 0.6);
        check("MEAN: D-F# a pure major third (386.3 ct, 5:4)", fabs(c[2][4] - 386.314) < 0.6);
        check("JUST: i (D F A) a pure minor triad, 6:5 and 3:2", fabs(c[3][3] - 315.641) < 0.6 && fabs(c[3][7] - 701.955) < 0.6);
        check("JUST: bVII's fifth (C-G: 16:9 to 4:3 + octave) pure, not the 9:5 wolf",
              fabs(c[3][5] + 1200.0 - c[3][10] - 701.955) < 0.6);
        check("JUST: bVI (Bb D F) a pure major triad", fabs(c[3][3] + 1200.0 - c[3][8] - 701.955) < 0.6 &&
              fabs(1200.0 - c[3][8] - 386.314) < 0.6);
    }

    /* MONO / LEGATO / UNISON: the new note's offset; UNISON keeps its detune beside it */
    t->p[P_TEMP] = 3;
    t->p[P_ROOT] = 0;
    t->p[P_VOICE] = V_LEGATO;
    silence(t);
    trk_note_on(t, 60, 100);
    trk_note_on(t, 64, 100);                            /* legato to E: JUST's 5:4, -13.7 ct */
    {
        voice_t *v = sounding(t, 64);
        check("LEGATO: the slid-to note tempered (E over C: 5:4)", v && fabs(voice_cents(v) + 13.686) < 0.6);
    }
    trk_note_off(t, 64);
    trk_note_off(t, 60);
    silence(t);
    t->p[P_VOICE] = V_UNISON;
    t->p[P_DETUNE] = 40;
    trk_note_on(t, 64, 100);
    for (n = 0, ok = 0; n < NVOICE; n++)
        if (t->v[n].active)
            ok += t->v[n].tfine == t->v[0].tfine && t->v[n].pitch16 == t->v[0].pitch16;
    check("UNISON: every voice the same tempered note, the detune (fine) its own",
          ok == trk_nunison(t) && t->v[0].fine != t->v[trk_nunison(t) - 1u].fine);
    silence(t);

    printf("%s\n", bad ? "TEMPER TEST FAILED" : "temper test passed");
    return bad != 0;
}
