/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GHOULBOX (on Felucca by Leo Kuroshita, Hügelton Instruments) */
/* DREAM (GHOULBOX 2.3: HOME > DREAM, dream.c) through the UI's own code (the UI test's harness):
 * 1. the page (after KEYS on HOME's taps) and its four controls, saved with the project (FUN10's tail; none: defaults).
 * 2. the parts by sound: the power-on scenes' tracks.
 * 3. a piece, for every scene, MOOD and DENS 1 / 4 / 8: 64 steps at 1/8, ROOT and SCL set, every note in the key and in
 *    range, a tie only after a note, the drone one note held, the pads triads, the drums hits, a GURDY's drones on the key.
 * 4. DENS: more notes at 8 than at 1.
 * 5. the keys on DREAM: silent; over steps the dialog first (its key), then a key composes at once.
 * 6. EVOL: a change at the end of a pass (every pass at DRFT 8), always in key, the drone and the motif's span kept; none
 *    with EVOL OFF, nor stopped. After a load, the key and the chords are found back from the tracks. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int note_ok(uint32_t n)
{
    return n >= 24u && n <= 100u && ((SCALE_MASK[dream.scl] >> ((n + 120u - dream.root) % 12u)) & 1u);
}

/* the piece now on the tracks against what 3. asks; why not on stdout */
static int piece_ok(const char *what)
{
    uint32_t i, j, k;
    for (i = 0; i < NTRK; i++) {
        const track_t *t = &trk[i];
        uint32_t r = dream.role[i], drum = t->eng_req % NENGINES == ENGI_DRUM;
        int prev = r == DR_PAD && dream.dup[i];            /* (a late pad's first steps tie over from the last chord) */
        if (t->p[P_SLEN] != 64 || t->p[P_SDIV] != 1 || t->p[P_ROOT] != dream.root || t->p[P_SCALE] != dream.scl ||
            t->p[P_CHRD] || t->p[P_AMODE]) {
            printf("    %s: T%u's LEN DIV ROOT SCL CHRD ARP\n", what, i + 1u);
            return 0;
        }
        if (t->eng_req % NENGINES == ENGI_GURDY && t->p[P_E0] != dream.root) {
            printf("    %s: T%u's DRN\n", what, i + 1u);
            return 0;
        }
        for (j = 0; j < 64u; j++) {
            const step_t *s = &t->step[j];
            if (s->time == ST_TIE && !prev) {
                printf("    %s: T%u step %u ties onto nothing\n", what, i + 1u, j + 1u);
                return 0;
            }
            if (s->time == ST_NOTE)
                prev = 1;
            else if (s->time == ST_REST)
                prev = 0;
            if (s->time != ST_NOTE)
                continue;
            if (drum ? s->n || !s->hit : !s->n || s->hit) {
                printf("    %s: T%u step %u: notes %u hits %02x\n", what, i + 1u, j + 1u, s->n, s->hit);
                return 0;
            }
            for (k = 0; k < s->n; k++)
                if (!note_ok(s->note[k])) {
                    printf("    %s: T%u step %u note %u out of key or range\n", what, i + 1u, j + 1u, s->note[k]);
                    return 0;
                }
            if ((r == DR_PAD && s->n != 3u) || (r == DR_DRONE && j) || (r != DR_PAD && r != DR_DRONE && s->n > 1u)) {
                printf("    %s: T%u (role %u) step %u holds %u notes\n", what, i + 1u, r, j + 1u, s->n);
                return 0;
            }
        }
    }
    return 1;
}

static uint32_t notes_of(const track_t *t)
{
    uint32_t j, n = 0;
    for (j = 0; j < 64u; j++)
        n += t->step[j].time == ST_NOTE;
    return n;
}

static void scene(uint32_t sc)
{
    ui_power_on();
    gb_scene_load(sc);
    song.playing = 0;
    transport_req = 0;
}

int main(void)
{
    int bad = 0, ok;
    uint32_t i, k, sc, m, d;
    static const uint8_t ROLES[4][4] = {
        {DR_MELODY, DR_PAD, DR_PAD, DR_DRONE},         /* DUNGEON: HURDY GURDY, CRYPT PAD, MONKS, DRONE WHEEL */
        {DR_DRONE, DR_PAD, DR_DRONE, DR_PERC},         /* CRYPT: LOW DRONE, CRYPT CHOIR, CAVE WIND, TOMB DRUMS */
        {DR_MELODY, DR_ARP, DR_ARP, DR_PERC},          /* TAVERN: DANCE GURDY, DUNGEON HARP, LUTE, CRYPT KIT */
        {DR_PAD, DR_PAD, DR_MELODY, DR_HIGH},          /* CHAPEL: CATHEDRAL, MONKS, TENOR RECORD, VIELLE (2nd melody) */
    };

    /* 1 */
    ui_power_on();
    ui.home = 1;
    for (k = 0; k < 4u; k++)
        home_tap();
    ok = !ui.home && str_eq(cur_page()->title, "DREAM") && cur_page()->fam == FAM_TRK && cur_page()->id[0] == G_DREAM &&
         cur_page()->id[3] == G_DMOOD && str_eq(GP[G_DREAM].label, "EVOL") && str_eq(GP[G_DDENS].label, "DENS") &&
         str_eq(GP[G_DDRFT].label, "DRFT") && str_eq(GP[G_DMOOD].label, "MOOD") && GP[G_DMOOD].max == 5 &&
         GP[G_DREAM].def == 0 && GP[G_DDENS].def == 4 && GP[G_DDRFT].def == 6 && G_COUNT == 37;
    bad += check("DREAM: HOME's fourth tap (after MIXER CLOCK KEYS), EVOL DENS DRFT MOOD", ok);
    {
        project_t q;
        project_store_t st;
        song.g[G_DREAM] = 1; song.g[G_DDENS] = 8; song.g[G_DDRFT] = 1; song.g[G_DMOOD] = 5;
        project_capture(&q);
        ok = proj_pack(&st, &q) == 1 && proj_import(&q, &st, (int)sizeof st) && q.g[G_DREAM] == 1 && q.g[G_DDENS] == 8 &&
             q.g[G_DDRFT] == 1 && q.g[G_DMOOD] == 5;
        st.raw[PROJ_DREAM_OFF] = st.raw[PROJ_DREAM_OFF + 1u] = 0;   /* a project saved before 2.3: no DREAM word */
        k = proj_hash(st.raw, PROJ_STORE_SIZE - 4u);
        memcpy(st.raw + PROJ_STORE_SIZE - 4u, &k, 4);
        ok &= proj_import(&q, &st, (int)sizeof st) && q.g[G_DREAM] == 0 && q.g[G_DDENS] == 4 && q.g[G_DDRFT] == 6 &&
              q.g[G_DMOOD] == 0;
        bad += check("  saved with the project; a project from before 2.3: EVOL OFF, DENS 4, DRFT 6, MIN", ok);
    }

    /* 2 */
    for (ok = 1, sc = 0; sc < 4u; sc++) {
        scene(sc);
        dr_roles();
        for (i = 0; i < NTRK; i++)
            if (dream.role[i] != ROLES[sc][i]) {
                printf("    scene %s T%u: role %u, not %u\n", GB_SCENES[sc].name, i + 1u, dream.role[i], ROLES[sc][i]);
                ok = 0;
            }
    }
    bad += check("the parts by sound: drone, pad (a second one later, higher), melody (a second one high), arp, drums", ok);

    /* 3 */
    for (ok = 1, sc = 0; sc < 4u && ok; sc++)
        for (m = 0; m < 6u && ok; m++)
            for (d = 1; d <= 8u && ok; d += d == 1u ? 3u : 4u)
                for (k = 0; k < 3u && ok; k++) {
                    char w[48];
                    scene(sc);
                    rng_state = 0x9E3779B9u * (sc * 97u + m * 13u + d * 5u + k + 1u);
                    song.g[G_DMOOD] = (int16_t)m;
                    song.g[G_DDENS] = (int16_t)d;
                    dream_compose((sc * 5u + m * 7u + k) % 12u);
                    snprintf(w, sizeof w, "%s %s DENS %u #%u", GB_SCENES[sc].name, N_DMOOD[m], d, k);
                    ok = dream.valid && transport_req == 1u && piece_ok(w);
                }
    bad += check("pieces (4 scenes x 6 moods x DENS 1 4 8 x 3): 64 steps 1/8, in key and range, ties on notes", ok);

    /* 4 */
    {
        uint32_t lo = 0, hi = 0;
        for (k = 0; k < 8u; k++) {
            scene(2);
            song.g[G_DDENS] = 1; dream_compose(2); lo += notes_of(&trk[1]) + notes_of(&trk[0]);
            scene(2);
            song.g[G_DDENS] = 8; dream_compose(2); hi += notes_of(&trk[1]) + notes_of(&trk[0]);
        }
        bad += check("DENS: the arp and the melody play more at 8 than at 1", hi > lo * 3u / 2u);
        printf("        (TAVERN's harp and gurdy: %u notes at DENS 1, %u at 8, over 8 pieces)\n", lo, hi);
    }

    /* 5 */
    {
        uint32_t key = 9;                              /* D4 (key 0 is F3) */
        scene(0);
        dream.asked = 0;
        ui.home = 1;
        for (k = 0; k < 4u; k++)
            home_tap();
        frame();
        ok = song.grid == 4u;
        fm1_in.notes |= 1u << key;
        host_notes |= 1u << key;
        frame();
        events_block(CTL);
        for (i = 0; i < NTRK; i++)
            for (k = 0; k < NVOICE; k++)
                ok &= !(trk[i].v[k].active && trk[i].v[k].gate);
        ok &= ui.confirm == CF_DREAM && ui.confirm_trk == 2u;
        fm1_in.notes &= ~(1u << key);
        frame();
        press(B_OCTUP);
        ok &= !ui.confirm && dream.valid && dream.asked && dream.root == 2u && trk[0].p[P_ROOT] == 2;
        tap_key(key + 2u);                             /* E4: at once, no dialog */
        ok &= !ui.confirm && dream.root == 4u && trk[2].p[P_ROOT] == 4;
        bad += check("the keys on DREAM: silent, the dialog over steps (its key), then a key composes at once", ok);
    }

    /* 6 */
    {
        static step_t before[NTRK][NSTEP];
        uint32_t changes = 0, pass, motif_kept = 1, drone_kept = 1, inkey = 1;
        int16_t mute[NTRK];
        scene(0);
        rng_state = 12345;
        dream_compose(2);
        song.g[G_DREAM] = 1;
        song.g[G_DDRFT] = 8;                           /* every pass */
        song.playing = 1;
        for (pass = 0; pass < 60u; pass++) {
            for (i = 0; i < NTRK; i++) {
                memcpy(before[i], trk[i].step, sizeof before[i]);
                mute[i] = trk[i].p[P_MUTE];
            }
            trk[0].seq_idx = 40;
            dream_tick();
            trk[0].seq_idx = 0;
            dream_tick();
            for (k = 0, i = 0; i < NTRK; i++)
                k |= memcmp(before[i], trk[i].step, sizeof before[i]) || mute[i] != trk[i].p[P_MUTE];
            changes += k;
            for (i = 0; i < NTRK; i++) {
                motif_kept &= dream.role[i] != DR_MELODY || !memcmp(before[i], trk[i].step, 16u * sizeof(step_t));
                drone_kept &= dream.role[i] != DR_DRONE || !memcmp(before[i], trk[i].step, sizeof before[i]);
            }
            inkey &= piece_ok("EVOL");
        }
        bad += check("EVOL at DRFT 8: a change at every pass's end, in key; the drone and the motif's span kept",
                     changes >= 55u && motif_kept && drone_kept && inkey);
        printf("        (%u of 60 passes changed something)\n", changes);
        for (i = 0; i < NTRK; i++)
            memcpy(before[i], trk[i].step, sizeof before[i]);
        song.g[G_DDRFT] = 6;                           /* every 3 passes */
        for (changes = 0, pass = 0; pass < 30u; pass++) {
            trk[0].seq_idx = 40; dream_tick(); trk[0].seq_idx = 0; dream_tick();
            changes += memcmp(before[0], trk[0].step, sizeof before[0]) != 0;
        }
        song.g[G_DREAM] = 0;
        for (i = 0; i < NTRK; i++)
            memcpy(before[i], trk[i].step, sizeof before[i]);
        for (pass = 0; pass < 30u; pass++) {
            trk[0].seq_idx = 40; dream_tick(); trk[0].seq_idx = 0; dream_tick();
        }
        for (ok = 1, i = 0; i < NTRK; i++)
            ok &= !memcmp(before[i], trk[i].step, sizeof before[i]);
        song.g[G_DREAM] = 1;
        song.playing = 0;
        for (pass = 0; pass < 30u; pass++) {
            trk[0].seq_idx = 40; dream_tick(); trk[0].seq_idx = 0; dream_tick();
        }
        for (i = 0; i < NTRK; i++)
            ok &= !memcmp(before[i], trk[i].step, sizeof before[i]);
        bad += check("  none with EVOL OFF, nor with the transport stopped", ok);
    }
    {
        uint8_t deg[4];
        scene(1);
        rng_state = 777;
        dream_compose(5);
        memcpy(deg, dream.deg, 4);
        dream.valid = 0;
        dream.root = dream.scl = 0;
        memset(dream.deg, 9, 4);
        dr_rebuild();
        bad += check("after a load: the key, the scale and the chords found back from the tracks",
                     dream.valid && dream.root == 5u && dream.scl == DR_MOOD_SCL[song.g[G_DMOOD]] && !memcmp(deg, dream.deg, 4));
    }
    {   /* EVOL over patterns of any scale (after a load, the tracks' own SCL: 5 .. 12 tones): no hang (the alarm), in key */
        uint32_t scl, pass;
        alarm(60);
        for (ok = 1, scl = 0; scl < sizeof SCALE_MASK / sizeof SCALE_MASK[0]; scl++) {
            scene(0);
            for (i = 0; i < NTRK; i++)
                trk[i].p[P_SCALE] = (int16_t)scl;
            dream.valid = 0;
            song.g[G_DREAM] = 1;
            song.g[G_DDRFT] = 8;
            song.playing = 1;
            rng_state = scl ? 31u * scl + 7u : 0u;         /* (0: xorshift stuck at 0, every pick the same: no loop) */
            {
                static step_t was[NTRK][NSTEP];
                for (i = 0; i < NTRK; i++)
                    memcpy(was[i], trk[i].step, sizeof was[i]);
                for (pass = 0; pass < 40u; pass++) {
                    trk[0].seq_idx = 40; dream_tick(); trk[0].seq_idx = 0; dream_tick();
                }
                for (i = 0; i < NTRK; i++)                 /* what EVOL wrote (the scene's own steps: its key) */
                    for (k = 0; k < 64u; k++)
                        for (m = 0; memcmp(&was[i][k], &trk[i].step[k], sizeof(step_t)) &&
                                    trk[i].step[k].time == ST_NOTE && m < trk[i].step[k].n; m++)
                            if (!note_ok(trk[i].step[k].note[m])) {
                                printf("    SCL %u T%u step %u: %u\n", scl, i + 1u, k + 1u, trk[i].step[k].note[m]);
                                ok = 0;
                            }
            }
            song.playing = 0;
        }
        alarm(0);
        bad += check("EVOL over any SCL (CHR, the pentatonics, the 8-tone ..) and a stuck random: no hang, in that scale", ok);
    }
    printf(bad ? "dream: %d FAILED\n" : "dream: all ok\n", bad);
    return bad != 0;
}
