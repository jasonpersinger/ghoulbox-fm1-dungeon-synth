/* SPDX-License-Identifier: GPL-3.0-only */
/* GHOULBOX: a short dungeon-synth piece through the firmware's own audio code (hostsim.c), for the release post and the sound page
 *   (tools/make_sound_page.py).  build/host/demo_clip OUT.wav
 * D minor, 108 BPM: a HURDY GURDY tune over its drone, a FRENCH HORN line under it, DARK STRINGS chords, MONKS on the
 * roots; HALL, TAPE and CRSH. */
#define main hostsim_main
#include "hostsim.c"
#undef main

#define TIE 255u

static uint32_t preset_of(uint32_t e, const char *name)
{
    uint32_t i;
    for (i = 0; i < ENGINES[e]->npresets; i++)
        if (!strcmp(ENGINES[e]->presets[i].name, name))
            return i;
    fprintf(stderr, "no preset %s\n", name);
    exit(1);
}

/* track t: engine e's preset, DIV div (params.c N_DIV), the steps (count 0 rest, TIE hold), its reverb send */
static void part(track_t *t, uint32_t e, const char *name, uint32_t div, uint32_t len, uint32_t rev,
                 const uint8_t (*notes)[3], const uint8_t *count)
{
    uint32_t i;
    host_preset(t, e, preset_of(e, name));
    t->p[P_SDIV] = (int16_t)div;
    t->p[P_SLEN] = (int16_t)len;
    t->p[P_SGATE] = 127;
    t->p[P_REV] = (int16_t)rev;
    for (i = 0; i < len; i++) {
        if (count[i] == TIE)
            put_step(t, i, 0, notes[i], ST_TIE, 0);
        else
            put_step(t, i, count[i], notes[i], count[i] ? ST_NOTE : ST_REST, 0);
    }
}

int main(int argc, char **argv)
{
    /* a bar each: Dm, Bb, Gm, A, then Dm held (DIV 1/1) */
    static const uint8_t CH[6][3] = {{50, 53, 57}, {46, 50, 53}, {43, 46, 50}, {45, 49, 52}, {50, 53, 57}, {0}};
    static const uint8_t CHN[6] = {3, 3, 3, 3, 3, TIE};
    /* the hurdy-gurdy's tune in eighths (its own drone under it): D . F . A G F E | D . F . Bb . A G |
     * G . F . D . Bb . | E . C# . A . C# . | D ... */
    static const uint8_t GU[40][3] = {{62}, {0}, {65}, {0}, {69}, {67}, {65}, {64}, {62}, {0}, {65}, {0}, {70}, {0}, {69}, {67},
                                      {67}, {0}, {65}, {0}, {62}, {0}, {58}, {0}, {64}, {0}, {61}, {0}, {57}, {0}, {61}, {0},
                                      {62}, {0}, {0}, {0}, {0}, {0}, {0}, {0}};
    static const uint8_t GUN[40] = {1, TIE, 1, TIE, 1, 1, 1, 1, 1, TIE, 1, TIE, 1, TIE, 1, 1, 1, TIE, 1, TIE, 1, TIE, 1, TIE,
                                    1, TIE, 1, TIE, 1, TIE, 1, TIE, 1, TIE, TIE, TIE, TIE, TIE, TIE, TIE};
    /* the horn below it in halves: A . | Bb . | G Bb | A G | A . */
    static const uint8_t HO[10][3] = {{57}, {0}, {58}, {0}, {55}, {58}, {57}, {55}, {57}, {0}};
    static const uint8_t HON[10] = {1, TIE, 1, TIE, 1, 1, 1, 1, 1, TIE};
    static const uint8_t MO[6][3] = {{50}, {46}, {43}, {45}, {50}, {0}}, MON[6] = {1, 1, 1, 1, 1, TIE};
    const char *path = argc > 1 ? argv[1] : "crypt_clip.wav";
    uint32_t t, i, frames = (uint32_t)(13.5 * FS), stop = (uint32_t)(11.2 * FS);
    int32_t o[2 * CTL];
    FILE *f;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    song.g[G_BPM] = 108;
    song.g[G_RTYPE] = 2;
    fx.rtype = 2;
    song.g[G_RSIZE] = 105;
    song.g[G_RDAMP] = 70;
    song.g[G_TAPE] = 70;                               /* more grit: the cassette, */
    song.g[G_CRSH] = 35;                               /* and the cheap sampler */
    rev_clear();
    hall_clear();
    part(&trk[0], ENGI_FM6, "DARK STRINGS", 7, 6, 100, CH, CHN);
    part(&trk[1], ENGI_FM6, "FRENCH HORN", 6, 10, 85, HO, HON);
    part(&trk[2], 5, "MONKS", 7, 6, 105, MO, MON);
    part(&trk[3], ENGI_GURDY, "HURDY GURDY", 1, 40, 65, GU, GUN);
    trk[0].p[P_LEVEL] = 92;
    trk[1].p[P_LEVEL] = 84;
    trk[2].p[P_LEVEL] = 80;
    trk[3].p[P_LEVEL] = 112;
    if (!(f = fopen(path, "wb")))
        return 1;
    wav_hdr(f, frames);
    transport_req = 1;
    for (t = 0; t < frames; t += CTL) {
        if (t >= stop && t < stop + CTL)
            transport_req = 2;                     /* the last chord rings, then the hall's tail */
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            wav_put(f, o[2 * i], o[2 * i + 1]);
    }
    fclose(f);
    printf("%s\n", path);
    return 0;
}
