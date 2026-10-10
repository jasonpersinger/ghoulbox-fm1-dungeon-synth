/* SPDX-License-Identifier: GPL-3.0-only */
/* GHOULBOX: a short preview of every factory preset for the sound page (tools/make_sound_page.py): one slow
 * D-minor phrase picked by the kind of sound (chords for pads, choirs, organs and strings; a lament for melodic
 * voices; broken chords for plucked ones; a low line for basses; a held note for ambience; TOMBBEAT for drums),
 * once at the power-on pace (72 BPM, 1/8 steps), then the tail, with the power-on room (HALL, TAPE 40).
 *   build/host/preset_preview OUTDIR   -> OUTDIR/<engine>_<nn>.wav, and an index on stdout (engine, nn, name, phrase)
 * Not a test. */
#define main hostsim_main
#include "hostsim.c"
#undef main

#define T_ 255                                     /* a tie */
enum { PH_LEAD, PH_CHORD, PH_PLUCK, PH_BASS, PH_HOLD, PH_DRUM };
static const char *const PH_NAME[] = {"a lament", "slow chords", "broken chords", "a low line", "a held note",
                                      "TOMBBEAT"};
static const struct { const char *name; uint8_t ph; } KIND[] = {
    {"SOFT PAD", PH_CHORD}, {"PWM STR", PH_CHORD}, {"STRINGS", PH_CHORD}, {"CRYPT PAD", PH_CHORD},
    {"FROST STR", PH_CHORD}, {"STRING", PH_CHORD}, {"CASIO CHOIR", PH_CHORD}, {"CHOIR AAH", PH_CHORD},
    {"MONKS", PH_CHORD}, {"CRYPT CHOIR", PH_CHORD}, {"WRAITH", PH_CHORD}, {"CHIP CHOIR", PH_CHORD},
    {"FANTASY PAD", PH_CHORD}, {"STRING MACH", PH_CHORD}, {"FULL ORGAN", PH_CHORD}, {"CRYPT ORGAN", PH_CHORD},
    {"CATHEDRAL", PH_CHORD}, {"HARMONIUM", PH_CHORD}, {"CLOUD PAD", PH_CHORD}, {"FROZEN", PH_CHORD},
    {"PAD", PH_CHORD}, {"GLASS CHOIR", PH_CHORD}, {"DARK STRINGS", PH_CHORD}, {"PIPE ORGAN", PH_CHORD},
    {"REN ORGAN", PH_CHORD},
    {"FOLK HARP", PH_PLUCK}, {"LUTE", PH_PLUCK}, {"HARPSICHORD", PH_PLUCK}, {"DUNGEON HARP", PH_PLUCK},
    {"VIRGINAL", PH_PLUCK}, {"BELL", PH_PLUCK},
    {"SUB BASS", PH_BASS}, {"DIRGE BASS", PH_BASS}, {"TUBA", PH_BASS}, {"LOW DRONE", PH_BASS},
    {"WIND", PH_HOLD}, {"CAVE WIND", PH_HOLD}, {"TORCH", PH_HOLD}, {"GURDY DRONE", PH_HOLD},
};
/* 16 steps of up to 3 notes; {T_} holds the step before, {0} rests */
static const uint8_t PHRASE[5][16][3] = {
    [PH_LEAD] = {{62}, {T_}, {65}, {T_}, {64}, {T_}, {62}, {T_}, {61}, {T_}, {62}, {T_}, {T_}, {T_}, {T_}, {T_}},
    [PH_CHORD] = {{50, 53, 57}, {T_}, {T_}, {T_}, {50, 53, 58}, {T_}, {T_}, {T_},
                  {50, 55, 58}, {T_}, {T_}, {T_}, {49, 52, 57}, {T_}, {T_}, {T_}},
    [PH_PLUCK] = {{50}, {53}, {57}, {62}, {46}, {50}, {53}, {58}, {43}, {46}, {50}, {55}, {45}, {49}, {52}, {57}},
    [PH_BASS] = {{38}, {T_}, {T_}, {T_}, {T_}, {T_}, {34}, {T_}, {T_}, {T_}, {T_}, {33}, {T_}, {T_}, {T_}, {T_}},
    [PH_HOLD] = {{50}, {T_}, {T_}, {T_}, {T_}, {T_}, {T_}, {T_}, {T_}, {T_}, {T_}, {T_}, {T_}, {T_}, {T_}, {T_}},
};

static uint32_t phrase_of(uint32_t e, const preset_t *p)
{
    uint32_t i;
    if (e == ENGI_DRUM)
        return PH_DRUM;
    for (i = 0; i < sizeof KIND / sizeof KIND[0]; i++)
        if (!strcmp(KIND[i].name, p->name))
            return KIND[i].ph;
    return PH_LEAD;
}

static void render(const char *dir, uint32_t e, uint32_t k)
{
    const preset_t *p = &ENGINES[e]->presets[k];
    uint32_t ph = phrase_of(e, p), t, i, n;
    uint32_t play = (uint32_t)(16 * 30.0 / 72 * FS), frames = play + (uint32_t)(2.5 * FS);   /* 16 eighths, tail */
    int32_t o[2 * CTL];
    char path[512];
    FILE *f;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    song.g[G_BPM] = 72;
    song.g[G_RTYPE] = 2;
    song.g[G_TAPE] = 40;
    rev_clear();
    hall_clear();
    fx.rtype = 2;
    host_preset(&trk[0], e, k);
    for (t = 0; t < 16u; t++) {
        if (ph == PH_DRUM) {                       /* as ui.c load_pat16 */
            uint8_t nt = PATTERNS[21].note[t], fl = PATTERNS[21].flags[t];
            put_step(&trk[0], t, nt ? 1u : 0u, &nt, nt ? ST_NOTE : ST_REST, nt ? fl & 3u : 0u);
            continue;
        }
        for (n = 0; n < 3u && PHRASE[ph][t][n] && PHRASE[ph][t][n] != T_; n++)
            ;
        if (PHRASE[ph][t][0] == T_)
            put_step(&trk[0], t, 0, PHRASE[ph][t], ST_TIE, 0);
        else
            put_step(&trk[0], t, n, PHRASE[ph][t], n ? ST_NOTE : ST_REST, 0);
        trk[0].step[t].vel = n ? 96 : 0;
    }
    trk[0].p[P_SLEN] = 16;
    trk[0].p[P_SDIV] = 1;                          /* 1/8, as the power-on scene */
    snprintf(path, sizeof path, "%s/%s_%02u.wav", dir, ENGINES[e]->name, k);
    if (!(f = fopen(path, "wb"))) {
        perror(path);
        return;
    }
    wav_hdr(f, frames);
    transport_req = 1;
    for (t = 0; t < frames; t += CTL) {
        if (t >= play && t < play + CTL)
            transport_req = 2;                     /* once through, then the tail */
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            wav_put(f, o[2 * i], o[2 * i + 1]);
    }
    fclose(f);
    printf("%s\t%u\t%s\t%s\n", ENGINES[e]->name, k, p->name, PH_NAME[ph]);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "build/preset_preview";
    uint32_t e, k;
    for (e = 0; e < NENGINES; e++) {
        if (!eng_ok(e))                            /* (DIGITAL: reserved; SLICE without FELUCCA_SLICE) */
            continue;
        for (k = 0; k < ENGINES[e]->npresets; k++)
            render(dir, e, k);
    }
    return 0;
}
