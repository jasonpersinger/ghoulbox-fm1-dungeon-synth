/* SPDX-License-Identifier: GPL-3.0-only */
/* GHOULBOX audition: a slow D minor passage (Dm - Bb - Gm - A, 60 BPM) through the whole mix, as the device
 * renders it (hostsim.c): a string pad, a choir, a flute melody and a low drone, all deep in the reverb.
 *   build/host/dungeon_demo OUTDIR      -> OUTDIR/dungeon_tape.wav (HALL + TAPE 75 + CRSH 40),
 *                                          OUTDIR/dungeon_hall.wav (clean), OUTDIR/dungeon_room.wav
 *                                          OUTDIR/dungeon_ghoulbox.wav (the passage on GHOULBOX's own sounds),
 *                                          OUTDIR/presets/NN_NAME.wav (each GHOULBOX preset alone, HALL + TAPE)
 * Not a test: an ear check of the sounds while the dungeon presets are made. */
#define main hostsim_main
#include "hostsim.c"
#undef main

#define TIE 255u                                       /* in the note tables below: hold the previous note */

static void part(track_t *t, uint32_t eng, uint32_t preset, uint32_t div, uint32_t len, uint32_t rev,
                 const uint8_t (*notes)[3], const uint8_t *count)
{
    uint32_t i;
    host_preset(t, eng, preset);
    t->p[P_SDIV] = (int16_t)div;
    t->p[P_SLEN] = (int16_t)len;
    t->p[P_SGATE] = 127;                               /* legato: each note held to the next */
    t->p[P_REV] = (int16_t)rev;
    for (i = 0; i < len; i++) {
        if (count[i] == TIE)
            put_step(t, i, 0, notes[i], ST_TIE, 0);
        else
            put_step(t, i, count[i], notes[i], count[i] ? ST_NOTE : ST_REST, 0);
    }
}

/* a factory preset of engine e by name (255: none) */
static uint32_t preset_of(uint32_t e, const char *name)
{
    uint32_t i;
    for (i = 0; i < ENGINES[e]->npresets; i++)
        if (!strcmp(ENGINES[e]->presets[i].name, name))
            return i;
    fprintf(stderr, "dungeon demo: no preset %s\n", name);
    exit(1);
}

/* the four parts' sounds: engine, preset (by name) */
typedef struct { uint32_t e; const char *name; } snd_t;
static const snd_t STOCK[4] = {{0, "STRINGS"}, {5, "CHOIR AAH"}, {4, "FLUTE"}, {7, "FULL ORGAN"}};
static const snd_t GHOUL[4] = {{0, "CRYPT PAD"}, {5, "MONKS"}, {7, "TOWER FLUTE"}, {9, "GURDY DRONE"}};

static void render(const char *dir, const char *name, int rtype, int tape, int crsh, const snd_t *snd)
{
    /* the pad: one chord a bar (DIV 1/1) */
    static const uint8_t PAD[4][3] = {{50, 53, 57}, {46, 50, 53}, {43, 46, 50}, {45, 49, 52}}, PADN[4] = {3, 3, 3, 3};
    /* the choir: a long note a bar, above the pad */
    static const uint8_t CHO[4][3] = {{62}, {62}, {58}, {61}}, CHON[4] = {1, 1, 1, 1};
    /* the flute: quarter notes */
    static const uint8_t FLU[16][3] = {{69}, {0}, {67}, {65}, {62}, {0}, {65}, {67}, {70}, {0}, {69}, {67}, {69}, {0}, {0}, {0}};
    static const uint8_t FLUN[16] = {1, TIE, 1, 1, 1, TIE, 1, 1, 1, TIE, 1, 1, 1, TIE, TIE, 0};
    /* the drone: low D for the whole 4 bars (DIV 4BAR) */
    static const uint8_t DRN[1][3] = {{38}}, DRNN[1] = {1};
    char path[512];
    FILE *f;
    uint32_t t, i, frames = 40u * FS;
    int32_t o[2 * CTL];
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    rev_clear();
    hall_clear();
    song.g[G_BPM] = 60;
    song.g[G_RTYPE] = (int16_t)rtype;
    fx.rtype = (uint8_t)rtype;
    song.g[G_RSIZE] = 110;
    song.g[G_RDAMP] = 70;                              /* dark stone, not a bright plate */
    song.g[G_TAPE] = (int16_t)tape;                    /* the cassette and the cheap sampler (tape.c) */
    song.g[G_CRSH] = (int16_t)crsh;
    part(&trk[0], snd[0].e, preset_of(snd[0].e, snd[0].name), 7, 4, 100, PAD, PADN);   /* pad: a chord a bar */
    part(&trk[1], snd[1].e, preset_of(snd[1].e, snd[1].name), 7, 4, 110, CHO, CHON);   /* choir */
    part(&trk[2], snd[2].e, preset_of(snd[2].e, snd[2].name), 0, 16, 90, FLU, FLUN);   /* the melody */
    part(&trk[3], snd[3].e, preset_of(snd[3].e, snd[3].name), 9, 1, 70, DRN, DRNN);    /* the drone, low */
    trk[3].p[P_LEVEL] = 80;
    snprintf(path, sizeof path, "%s/%s.wav", dir, name);
    if (!(f = fopen(path, "wb"))) {
        perror(path);
        return;
    }
    wav_hdr(f, frames);
    transport_req = 1;
    for (t = 0; t < frames; t += CTL) {
        if (t == 32u * FS)
            transport_req = 2;                         /* two passes, then the tail alone */
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            wav_put(f, o[2 * i], o[2 * i + 1]);
    }
    fclose(f);
    printf("dungeon demo: %s\n", path);
}

/* one preset alone: a phrase for its kind, 10 s (8 of notes, the tail), HALL SIZE 100, TAPE 60 */
enum { K_CHORDS, K_MELODY, K_ARP, K_BASS, K_HOLD };
static void audition(const char *dir, uint32_t n, uint32_t e, const char *name, uint32_t kind)
{
    static const uint8_t CH[4][3] = {{50, 53, 57}, {46, 50, 53}, {43, 46, 50}, {45, 49, 52}}, CHN[4] = {3, 3, 3, 3};
    static const uint8_t ME[16][3] = {{69}, {0}, {67}, {65}, {62}, {0}, {65}, {67}, {70}, {0}, {69}, {67}, {69}, {0}, {0}, {0}};
    static const uint8_t MEN[16] = {1, TIE, 1, 1, 1, TIE, 1, 1, 1, TIE, 1, 1, 1, TIE, TIE, 0};
    static const uint8_t AR[16][3] = {{50}, {57}, {62}, {65}, {69}, {65}, {62}, {57}, {46}, {53}, {58}, {62}, {65}, {62}, {58}, {53}};
    static const uint8_t ARN[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    static const uint8_t BA[8][3] = {{38}, {0}, {38}, {41}, {34}, {0}, {33}, {0}}, BAN[8] = {1, TIE, 1, 1, 1, TIE, 1, TIE};
    static const uint8_t HO[1][3] = {{50, 57}}, HON[1] = {2};
    char path[512];
    FILE *f;
    uint32_t t, i, frames = 10u * FS;
    int32_t o[2 * CTL];
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    rev_clear();
    hall_clear();
    song.g[G_BPM] = 60;
    song.g[G_RTYPE] = 2;
    fx.rtype = 2;
    song.g[G_RSIZE] = 100;
    song.g[G_RDAMP] = 70;
    song.g[G_TAPE] = 60;
    song.g[G_CRSH] = 0;
    switch (kind) {
    case K_CHORDS: part(&trk[0], e, preset_of(e, name), 6, 4, 100, CH, CHN); break;            /* a chord every 2 s */
    case K_MELODY: part(&trk[0], e, preset_of(e, name), 1, 16, 90, ME, MEN); break;            /* 1/8 */
    case K_ARP: part(&trk[0], e, preset_of(e, name), 1, 16, 80, AR, ARN); break;
    case K_BASS: part(&trk[0], e, preset_of(e, name), 0, 8, 60, BA, BAN); break;               /* 1/4 */
    default: part(&trk[0], e, preset_of(e, name), 8, 1, 90, HO, HON); break;                  /* 2BAR: held */
    }
    trk[0].p[P_REV] = (int16_t)ENGINES[e]->presets[preset_of(e, name)].fx[3] - 1;            /* the preset's own send */
    snprintf(path, sizeof path, "%s/presets/%02u_%s.wav", dir, n, name);
    for (i = 0; path[i]; i++)
        if (path[i] == ' ')
            path[i] = '_';
    if (!(f = fopen(path, "wb"))) {
        perror(path);
        return;
    }
    wav_hdr(f, frames);
    transport_req = 1;
    for (t = 0; t < frames; t += CTL) {
        if (t == 8u * FS)
            transport_req = 2;
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            wav_put(f, o[2 * i], o[2 * i + 1]);
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    static const struct { uint32_t e; const char *name; uint32_t kind; } GALLERY[] = {
        {0, "CRYPT PAD", K_CHORDS}, {0, "FROST STR", K_CHORDS}, {0, "WAR HORN", K_MELODY}, {0, "DIRGE BASS", K_BASS},
        {5, "MONKS", K_CHORDS}, {5, "CRYPT CHOIR", K_CHORDS}, {5, "WRAITH", K_HOLD},
        {7, "CRYPT ORGAN", K_CHORDS}, {7, "CATHEDRAL", K_CHORDS}, {7, "HARMONIUM", K_CHORDS}, {7, "TOWER FLUTE", K_MELODY},
        {9, "LUTE", K_ARP}, {9, "HARPSICHORD", K_ARP}, {9, "DUNGEON HARP", K_ARP}, {9, "GURDY DRONE", K_HOLD},
        {6, "FANTASY PAD", K_CHORDS}, {6, "MOURN HORN", K_MELODY}, {6, "STRING MACH", K_CHORDS},
        {3, "TOWER LEAD", K_MELODY}, {3, "CASIO CHOIR", K_CHORDS}, {3, "RECORDER", K_MELODY},
        {2, "GRIM BRASS", K_MELODY}, {11, "CAVE WIND", K_HOLD}, {11, "TORCH", K_HOLD},
    };
    const char *dir = argc > 1 ? argv[1] : "build/dungeon_demo";
    char sub[512];
    uint32_t k;
    render(dir, "dungeon_ghoulbox", 2, 75, 40, GHOUL);
    render(dir, "dungeon_tape", 2, 75, 40, STOCK);
    render(dir, "dungeon_hall", 2, 0, 0, STOCK);
    render(dir, "dungeon_room", 0, 0, 0, STOCK);
    snprintf(sub, sizeof sub, "mkdir -p '%s/presets'", dir);
    if (system(sub))
        return 1;
    for (k = 0; k < sizeof GALLERY / sizeof GALLERY[0]; k++)
        audition(dir, k + 1u, GALLERY[k].e, GALLERY[k].name, GALLERY[k].kind);
    printf("dungeon demo: %u presets in %s/presets\n", k, dir);
    return 0;
}
