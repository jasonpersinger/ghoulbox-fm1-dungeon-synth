/* SPDX-License-Identifier: GPL-3.0-only */
/* GHOULBOX ambience: long DREAM pieces for YouTube, played by the firmware itself (audio and screen, as
 * tests/lesson_render.c). A theme is four sounds and DREAM's settings; DREAM composes in the theme's first key and,
 * with EVOL ON, keeps changing the piece; every MOVE_S seconds a new key is dreamed (a new movement), as a player
 * would press another key on the DREAM page.
 *   build/host/ambient_render THEME SECONDS OUT.wav [FRAMES.rgb]   (THEME 1..; FRAMES: 240x240 RGB24, 30 fps; "-"
 *   for stdout, to pipe into ffmpeg). Not a test. */
#define LESSON_RENDER_NO_MAIN
#include "lesson_render.c"

#define MOVE_S 1080u                                   /* 18 minutes a movement */

typedef struct { const char *eng, *snd; uint8_t lvl, rev; } part_t;
typedef struct {
    const char *name;
    uint32_t seed;
    uint8_t bpm, mood, dens, drft, tape, amb, crkl, rsize;   /* mood: N_DMOOD (MIN DOR PHRY HARM MAJ MIX) */
    uint8_t roots[5];                                  /* the movements' keys (0 = C), in turn */
    part_t part[NTRK];
    uint8_t div;                                       /* the steps' DIV after each dream (N_DIV; 0: DREAM's 1/8) */
} theme_t;

static const theme_t THEMES[] = {
    {"Dungeon", 0x6d756e64u, 60, 2, 3, 5, 45, 34, 50, 110, {2, 9, 4, 7, 0},
     {{"GURDY", "DRONE WHEEL", 86, 70}, {"ANALOG", "CRYPT PAD", 84, 90},
      {"GURDY", "HURDY GURDY", 88, 90}, {"DRUM", "TOMB DRUMS", 86, 70}}},
    {"Castle in the Sky", 0x736b7931u, 66, 5, 3, 5, 25, 0, 0, 115, {7, 0, 2, 5, 9},
     {{"FM6", "GLASS CHOIR", 94, 100}, {"PHYS", "DUNGEON HARP", 96, 90},
      {"WHEEL", "TOWER FLUTE", 92, 100}, {"GRAIN", "CLOUD PAD", 90, 110}}},
    {"Underwater Temple", 0x74656d70u, 56, 1, 2, 6, 60, 22, 0, 127, {5, 0, 10, 3, 8},
     {{"GRAIN", "FROZEN", 98, 110}, {"FM6", "BELL", 86, 120},
      {"WHEEL", "SOFT FLUTE", 94, 110}, {"VOICE", "LOW DRONE", 100, 80}}},
    {"Town Square", 0x62617431u, 120, 3, 8, 8, 25, 0, 0, 70, {4, 11, 2, 9, 6},
     {{"ANALOG", "WAR HORN", 98, 40}, {"PHASE", "GRIM BRASS", 94, 40},
      {"ANALOG", "DIRGE BASS", 100, 20}, {"DRUM", "CRYPT KIT", 104, 35}}, 2},
    {"Conquest", 0x76696374u, 96, 4, 6, 5, 25, 0, 0, 85, {0, 7, 5, 10, 2},
     {{"FM6", "FRENCH HORN", 104, 55}, {"WHEEL", "FULL ORGAN", 92, 60},
      {"FM6", "TUBA", 100, 35}, {"DRUM", "CRYPT KIT", 100, 45}}},
};
#define NTHEMES (sizeof THEMES / sizeof THEMES[0])

/* a sound by its engine's and its preset's names, as PRESETS lists it (apply_preset_to takes the list's index) */
static int load_sound(track_t *t, const char *eng, const char *snd)
{
    uint32_t e, j;
    for (e = 0; e < NENGINES; e++) {
        const engine_t *en = ENGINES[e];
        if (!en || !str_eq(en->name, eng))
            continue;
        for (j = 0; j < en->npresets; j++)
            if (str_eq(en->presets[preset_orig(en, j)].name, snd)) {
                track_defaults(t);
                set_engine_of(t, e);
                apply_preset_to(t, j);
                t->engine = t->eng_req;               /* (as gb_scene_load: no note can come before) */
                eng_mem_claim(t);
                return 1;
            }
    }
    fprintf(stderr, "ambient: no sound %s %s\n", eng, snd);
    return 0;
}

int main(int argc, char **argv)
{
    uint32_t n = argc > 1 ? (uint32_t)atoi(argv[1]) : 1u, i, m, total;
    float sec = argc > 2 ? (float)atof(argv[2]) : 180.0f, done;
    const char *path = argc > 3 ? argv[3] : "ambient.wav";
    const theme_t *T;
    if (n < 1u || n > NTHEMES)
        return 2;
    T = &THEMES[n - 1u];
    vid = argc > 4 ? (strcmp(argv[4], "-") ? fopen(argv[4], "wb") : stdout) : 0;
    rep = fopen("/dev/null", "w");
    ui_power_on();
    pal(UI_CRYPT_INDEX);
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    song.batt_raw = 600;
    usb.config = 0;
    gb_scene_load(4);                                  /* BLANK: the globals' defaults, no notes */
    song.master_q12 = 2048;                            /* (as felucca_init) */
    undo_depth++;
    for (i = 0; i < NTRK; i++) {
        if (!load_sound(&trk[i], T->part[i].eng, T->part[i].snd))
            return 3;
        trk[i].p[P_LEVEL] = T->part[i].lvl;
        trk[i].p[P_REV] = T->part[i].rev;
    }
    undo_depth--;
    song.g[G_BPM] = T->bpm;
    song.g[G_RTYPE] = 2;                               /* HALL */
    song.g[G_RSIZE] = T->rsize;
    song.g[G_TAPE] = T->tape;
    song.g[G_AMB] = T->amb;
    song.g[G_CRKL] = T->crkl;
    song.g[G_DREAM] = 1;                               /* EVOL ON */
    song.g[G_DDENS] = T->dens;
    song.g[G_DDRFT] = T->drft;
    song.g[G_DMOOD] = T->mood;
    rng_state = T->seed;
    rev_clear();
    hall_clear();
    if (!(wav = fopen(path, "wb")))
        return 1;
    wav_hdr(wav, 0);
    run(0.5f);
    for (m = 0, done = 0.0f; done < sec; m++) {        /* a movement: a key dreamed, then played */
        float len = sec - done < (float)MOVE_S ? sec - done : (float)MOVE_S;
        dream_go(T->roots[m % sizeof T->roots]);
        for (i = 0; T->div && i < NTRK; i++)
            trk[i].p[P_SDIV] = T->div;
        if (!m)                                        /* what DREAM made of each sound */
            for (i = 0; i < NTRK; i++)
                fprintf(stderr, "  T%u %-12s %s\n", i + 1u, T->part[i].snd,
                        (const char *const[]){"drone", "bass", "pad", "arp", "melody", "high", "drums"}[dream.role[i]]);
        run(len);
        done += len;
    }
    total = now;
    fseek(wav, 0, SEEK_SET);
    wav_hdr(wav, total);
    fclose(wav);
    if (vid && vid != stdout)
        fclose(vid);
    fprintf(stderr, "%s: %s, %.1f s, %u movement(s)\n", path, T->name, total / (double)FS, m);
    return 0;
}
