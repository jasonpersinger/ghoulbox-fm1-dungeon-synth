/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GHOULBOX (on Felucca by Leo Kuroshita, Hügelton Instruments) */
/* RING (GHOULBOX 2.3: core.h P_RING, voice.c voice_release, eng_gurdy.c gurdy_rings): a GURDY's drones through the rests.
 * 1. the parameter (RING OFF / ON, OFF by default) and its page, a GURDY track's only.
 * 2. the sound loads: HURDY GURDY, DRONE WHEEL and BAGPIPE ring, the other GURDY sounds and other engines do not.
 * 3. OFF, or ON with the transport stopped: a note's end is its release, the voice ends (as ever).
 * 4. ON while the transport runs: the note's end leaves the drones sounding (the voice kept, its level steady over a
 *    second), the melody string faded; with no drones (DLVL, 5TH 0) silence.
 * 5. the next note takes the same voice back, its melody string back to full over the attack (no click).
 * 6. what ends the ringing: the stop, RING OFF, both into the release.
 * 7. a ringing voice is not a released one to steal (voice.c voice_victim), as a held note.
 * 8. the project keeps RING (FUN10 by count). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int32_t rbuf[CTL];
static double level(track_t *t, uint32_t blocks)      /* the part's RMS over that many blocks */
{
    double e = 0;
    for (uint32_t b = 0; b < blocks; b++) {
        track_render(t, rbuf, CTL);
        for (uint32_t i = 0; i < CTL; i++)
            e += (double)rbuf[i] * rbuf[i];
    }
    return sqrt(e / (blocks * CTL));
}
static uint32_t nactive(const track_t *t)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < NVOICE; i++)
        n += t->v[i].active && t->v[i].stage != 4u;
    return n;
}
static track_t *r_reset(uint32_t preset, int ring, int playing)
{
    track_t *t = &trk[0];
    uint32_t p, i;
    ui_power_on();
    song.playing = 0;
    for (p = 0; p < NTRK; p++) {
        trk_all_off(&trk[p]);
        for (i = 0; i < NVOICE; i++)
            trk[p].v[i].active = 0;
    }
    song.sel = 0;
    set_engine_of(t, ENGI_GURDY);
    engine_block(t);                                   /* (the engine switch, as the audio block does it) */
    apply_preset_to(t, preset);
    t->p[P_RING] = (int16_t)ring;
    song.playing = (uint8_t)playing;                   /* (no events_block: the transport's flag only, no steps) */
    return t;
}

int main(void)
{
    int bad = 0, ok;
    uint32_t i, k;
    track_t *t;
    static const char *const GURDY[6] = {"HURDY GURDY", "DRONE WHEEL", "DANCE GURDY", "TROMPETTE", "VIELLE", "BAGPIPE"};

    /* 1 */
    bad += check("RING: OFF / ON, OFF by default, after TEMP (104), P_E0 105",
                 str_eq(TP[P_RING].label, "RING") && TP[P_RING].max == 1 && TP[P_RING].def == 0 &&
                 str_eq(TP[P_RING].names[0], "OFF") && str_eq(TP[P_RING].names[1], "ON") && P_RING == 104 && P_E0 == 105);
    for (k = 0, i = 0; i < NPAGES; i++)
        if (PAGES[i].scope == SC_TRACK && PAGES[i].id[0] == P_RING)
            k = i;
    ui_power_on();
    set_engine_of(TSEL, ENGI_GURDY);
    ok = k && str_eq(PAGES[k].title, "RING") && PAGES[k].fam == FAM_EDIT && page_visible(k);
    set_engine_of(TSEL, 0);
    ok &= !page_visible(k);
    set_engine_of(TSEL, ENGI_DRUM);
    ok &= !page_visible(k);
    bad += check("  its page RING (EDIT), on a GURDY track only", ok);

    /* 2 */
    for (ok = 1, i = 0; i < 6u; i++) {
        t = r_reset(i, 0, 0);
        apply_preset_to(t, i);
        ok &= str_eq(ENG_GURDY.presets[i].name, GURDY[i]) && t->p[P_RING] == (i == 0 || i == 1 || i == 5);
    }
    t = r_reset(0, 1, 0);
    set_engine_of(t, 0);
    ok &= !t->p[P_RING];
    bad += check("sound loads: HURDY GURDY, DRONE WHEEL, BAGPIPE ring; DANCE GURDY, TROMPETTE, VIELLE, ANALOG do not", ok);

    /* 3 */
    for (ok = 1, k = 0; k < 2u; k++) {                 /* RING OFF playing; RING ON stopped */
        t = r_reset(5, (int)k, (int)!k);
        trk_note_on(t, 62, 100);
        level(t, 50);
        trk_note_off(t, 62);
        ok &= t->v[0].stage == 3u;
        level(t, 3000);
        ok &= !nactive(t);
    }
    bad += check("OFF, or ON with the transport stopped: the note's end is the release, the voice ends", ok);

    /* 4 */
    {
        double on, a, b, c;
        t = r_reset(5, 1, 1);                          /* BAGPIPE: loud drones */
        trk_note_on(t, 62, 100);
        on = level(t, 400);
        trk_note_off(t, 62);
        ok = t->v[0].active && !t->v[0].gate && t->v[0].stage == 2u;
        level(t, 400);                                 /* (the melody string's fade) */
        a = level(t, 700);
        b = level(t, 700);
        ok &= t->v[0].active && t->v[0].stage == 2u && !t->v[0].ring && a > on * 0.2 && fabs(a - b) < a * 0.1;
        bad += check("ON, playing: the drones ring on after the note (steady over a second), the voice at its sustain", ok);
        printf("        (note %.0f, ringing %.0f then %.0f RMS)\n", on, a, b);
        t = r_reset(5, 1, 1);
        t->p[P_E1] = t->p[P_E2] = 0;                   /* no drones: the melody string only */
        trk_note_on(t, 62, 100);
        on = level(t, 400);
        trk_note_off(t, 62);
        level(t, 400);
        c = level(t, 200);
        bad += check("  with no drones (DLVL, 5TH 0): the melody string fades to silence", t->v[0].active && on > 1000 && c < on * 1e-3);

        /* 5 */
        t = r_reset(5, 1, 1);
        trk_note_on(t, 62, 100);
        level(t, 100);
        trk_note_off(t, 62);
        level(t, 600);
        k = t->v[0].ring;
        trk_note_on(t, 65, 100);
        ok = k == 0 && t->v[0].gate && t->v[0].ring == 0 && t->v[0].note == 65 && nactive(t) == 1;
        level(t, 1);
        ok &= t->v[0].ring > 0 && t->v[0].ring < 255;   /* (back over the attack: no click) */
        level(t, 10);
        ok &= t->v[0].ring == 255;
        trk_note_off(t, 65);
        ok &= t->v[0].stage == 2u && !t->v[0].gate;
        bad += check("the next note takes the voice back (its melody string back over the attack), and rings again", ok);

        /* 6 */
        seq_stop();
        level(t, 2);
        ok = t->v[0].stage == 3u;
        level(t, 3000);
        ok &= !nactive(t);
        t = r_reset(0, 1, 1);
        trk_note_on(t, 62, 100);
        level(t, 50);
        trk_note_off(t, 62);
        level(t, 50);
        ok &= t->v[0].stage == 2u;
        t->p[P_RING] = 0;
        level(t, 2);
        ok &= t->v[0].stage == 3u;
        level(t, 3000);
        ok &= !nactive(t);
        bad += check("the stop, or RING OFF, ends the ringing: the release, the voice ends", ok);
        t = r_reset(0, 1, 1);                          /* a mute too: no next note comes to take the voice back */
        trk_note_on(t, 62, 100);
        level(t, 50);
        trk_note_off(t, 62);
        level(t, 50);
        ok = t->v[0].stage == 2u;
        t->p[P_MUTE] = 1;
        level(t, 2);
        ok &= t->v[0].stage == 3u;
        level(t, 3000);
        ok &= !nactive(t);
        bad += check("  and a mute (GLO): the drones do not ring on, unheard notes never come", ok);
    }

    /* 7 */
    {
        track_t *pp = 0;
        t = r_reset(0, 1, 1);
        trk_note_on(t, 62, 100);
        level(t, 20);
        trk_note_off(t, 62);
        level(t, 20);
        ok = voice_victim(&trk[1], 1, &pp) == NVOICE;   /* soft: released voices only */
        t->p[P_RING] = 0;
        level(t, 2);
        ok &= voice_victim(&trk[1], 1, &pp) == 0 && pp == t;
        bad += check("a ringing voice is not taken as a released one (once released, it is)", ok);
    }

    /* 8 */
    {
        project_t q;
        project_store_t st;
        t = r_reset(0, 1, 0);
        trk[2].p[P_RING] = 0;
        project_capture(&q);
        ok = proj_pack(&st, &q) == 1 && proj_import(&q, &st, (int)sizeof st) && q.t[0].p[P_RING] == 1 && q.t[2].p[P_RING] == 0;
        bad += check("the project keeps RING", ok);
    }
    song.playing = 0;
    printf(bad ? "ring: %d FAILED\n" : "ring: all ok\n", bad);
    return bad != 0;
}
