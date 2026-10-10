/* SPDX-License-Identifier: GPL-3.0-only */
/* GHOULBOX demo: a ~2.5 min piece in four sections, through the firmware's own audio AND screen code (tests/ui_render.c:
 * hostsim.c's mix plus the device UI), for the release post and the README.
 *   build/host/demo_piece OUT.wav [FRAMES.rgb]   -> the mix; with FRAMES.rgb also the screen, 240x240 RGB24 at 30 fps
 * Each section is four tracks of steps, as on a device; between sections the sounds, steps, tempo and effects change, as
 * a player loads them. I THE CRYPT (D Phrygian, 54), II THE TOWER (D minor, 66), III THE TAVERN (D Dorian, 100),
 * IV RETURN TO THE CRYPT (D minor, 60). Drums stay sparse and off the grid. Not a test. */
#define UI_RENDER_NO_MAIN
#include "ui_render.c"

#define T_ 255u                                        /* hold the previous note (a TIE step) */
#define R_ 0u                                          /* a rest */
#define MAXS 64u

typedef struct { uint8_t n[MAXS][4]; } steps_t;
typedef struct {
    uint32_t eng; const char *name;
    uint32_t div, len, rev, level;                     /* div: params.c N_DIV (0 1/4, 1 1/8, 6 1/2, 7 1/1) */
    const steps_t *st;
} part_t;
typedef struct {
    const char *title;
    uint32_t bpm, bars, rsize, tape, crsh;
    part_t p[NTRK];
    uint8_t show[8];                                   /* the track HOME shows, two bars each */
} section_t;

static uint32_t preset_of(uint32_t e, const char *name)
{
    uint32_t i;
    for (i = 0; i < ENGINES[e]->npresets; i++)
        if (!strcmp(ENGINES[e]->presets[i].name, name))
            return i;
    fprintf(stderr, "no preset %s\n", name);
    exit(1);
}

static void load_part(track_t *t, const part_t *p)
{
    uint32_t i, n;
    memset(t->step, 0, sizeof t->step);
    host_preset(t, p->eng, preset_of(p->eng, p->name));
    t->p[P_SDIV] = (int16_t)p->div;
    t->p[P_SLEN] = (int16_t)p->len;
    t->p[P_SGATE] = 127;                               /* legato: each note held to the next */
    t->p[P_REV] = (int16_t)p->rev;
    t->p[P_LEVEL] = (int16_t)p->level;
    t->p[P_MUTE] = 0;
    for (i = 0; i < p->len; i++) {
        const uint8_t *s = p->st->n[i];
        for (n = 0; n < 4u && s[n] && s[n] != T_; n++)
            ;
        if (s[0] == T_)
            put_step(t, i, 0, s, ST_TIE, 0);
        else
            put_step(t, i, n, s, n ? ST_NOTE : ST_REST, 0);
    }
}

/* steps from a list of (index, notes): everything else a rest */
#define AT(i, ...) [i] = {__VA_ARGS__}

/* ---------------------------------------------------------------- I THE CRYPT: D Phrygian, 54 BPM, 8 bars */
static const steps_t CR_WIND = {{{50}, {T_}, {T_}, {T_}, {T_}, {T_}, {T_}, {T_}}};                       /* 1/1 */
static const steps_t CR_DRONE = {{{38}, {T_}, {T_}, {T_}, {38}, {T_}, {T_}, {T_}}};                      /* 1/1 */
static const steps_t CR_CHOIR = {{{R_}, {R_}, {R_}, {R_}, {62}, {T_}, {63}, {T_},                        /* 1/2 */
                                  {62}, {T_}, {60}, {58}, {62}, {T_}, {T_}, {T_}}};
static const steps_t CR_DRUM = {{AT(0, 56), AT(21, 36), AT(37, 36), AT(46, 45), AT(58, 36), AT(63, 45)}};  /* 1/8 */

/* ---------------------------------------------------------------- II THE TOWER: D minor, 66 BPM, 12 bars */
#define DM {50, 53, 57}
#define BB {46, 50, 53}
#define CM {48, 52, 55}
#define GM {43, 46, 50}
#define AM {45, 49, 52}
static const steps_t TW_GURDY = {{                                                                         /* 1/4 */
    {69}, {T_}, {67}, {65},  {65}, {T_}, {62}, {T_},  {64}, {65}, {67}, {T_},  {69}, {T_}, {T_}, {T_},
    {74}, {T_}, {72}, {70},  {69}, {T_}, {65}, {T_},  {67}, {65}, {62}, {T_},  {64}, {T_}, {61}, {T_},
    {70}, {T_}, {74}, {T_},  {77}, {T_}, {74}, {72},  {70}, {72}, {74}, {T_},  {73}, {T_}, {T_}, {T_}}};
static const steps_t TW_PAD = {{DM, BB, CM, DM, DM, BB, GM, AM, GM, DM, BB, AM}};                          /* 1/1 */
static const steps_t TW_MONKS = {{{R_}, {R_}, {R_}, {R_}, {R_}, {R_}, {R_}, {R_}, {43}, {50}, {46}, {45}}};   /* 1/1 */
static const steps_t TW_DRUM = {{AT(8, 45), AT(16, 45, 36), AT(23, 45)}};                                 /* 1/2 */

/* ---------------------------------------------------------------- III THE TAVERN: D Dorian, 100 BPM, 16 bars */
static const steps_t TV_GURDY = {{                                                                         /* 1/8 */
    {62}, {T_}, {65}, {67}, {69}, {T_}, {67}, {65},   {64}, {T_}, {62}, {T_}, {60}, {T_}, {62}, {T_},
    {62}, {T_}, {65}, {67}, {69}, {T_}, {71}, {72},   {74}, {T_}, {72}, {T_}, {69}, {T_}, {T_}, {T_},
    {72}, {T_}, {71}, {69}, {67}, {T_}, {69}, {71},   {72}, {T_}, {69}, {T_}, {65}, {T_}, {67}, {T_},
    {69}, {T_}, {67}, {65}, {64}, {T_}, {60}, {T_},   {62}, {T_}, {T_}, {T_}, {R_}, {R_}, {R_}, {R_}}};
static steps_t TV_HARP;                                                                                     /* 1/8: built */
static const steps_t TV_LUTE = {{{38}, {45}, {36}, {43}, {38}, {45}, {43}, {38},                           /* 1/2 */
                                 {36}, {43}, {41}, {36}, {45}, {40}, {38}, {T_}}};
static const steps_t TV_DRUM = {{AT(0, 36), AT(11, 46), AT(16, 36), AT(27, 39), AT(32, 36), AT(43, 46),    /* 1/8 */
                                 AT(48, 36), AT(56, 36), AT(61, 39)}};

/* ---------------------------------------------------------------- IV RETURN TO THE CRYPT: D minor, 60 BPM, 8 bars */
static const steps_t RT_VIELLE = {{                                                                        /* 1/4 */
    {R_}, {R_}, {R_}, {R_},  {69}, {T_}, {67}, {65},  {65}, {T_}, {62}, {T_},  {64}, {T_}, {61}, {T_},
    {74}, {T_}, {72}, {70},  {69}, {T_}, {67}, {65},  {64}, {T_}, {T_}, {T_},  {62}, {T_}, {T_}, {T_}}};
static const steps_t RT_ORGAN = {{DM, BB, BB, AM, GM, DM, AM, DM}};                                        /* 1/1 */
static const steps_t RT_MONKS = {{{50}, {T_}, {46}, {45}, {43}, {50}, {45}, {38}}};                        /* 1/1 */
static const steps_t RT_DRUM = {{AT(13, 36), AT(43, 45), AT(56, 56, 36, 45)}};                            /* 1/8 */

static section_t SEC[4] = {
    {"I  THE CRYPT", 54, 8, 120, 70, 30, {
        {11, "CAVE WIND", 7, 8, 110, 70, &CR_WIND},
        {5, "LOW DRONE", 7, 8, 100, 84, &CR_DRONE},
        {5, "CRYPT CHOIR", 6, 16, 115, 74, &CR_CHOIR},
        {ENGI_DRUM, "TOMB DRUMS", 1, 64, 100, 92, &CR_DRUM}}, {1, 1, 2, 2, 3, 2, 0, 1}},
    {"II  THE TOWER", 66, 12, 105, 60, 20, {
        {ENGI_GURDY, "HURDY GURDY", 0, 48, 70, 96, &TW_GURDY},
        {0, "CRYPT PAD", 7, 12, 100, 72, &TW_PAD},
        {5, "MONKS", 7, 12, 110, 70, &TW_MONKS},
        {ENGI_DRUM, "TOMB DRUMS", 6, 24, 95, 88, &TW_DRUM}}, {0, 0, 1, 0, 2, 0, 0, 0}},
    {"III  THE TAVERN", 100, 16, 70, 45, 10, {
        {ENGI_GURDY, "DANCE GURDY", 1, 64, 55, 102, &TV_GURDY},
        {ENGI_PHYS, "DUNGEON HARP", 1, 64, 70, 92, &TV_HARP},
        {ENGI_PHYS, "LUTE", 6, 16, 50, 92, &TV_LUTE},
        {ENGI_DRUM, "CRYPT KIT", 1, 64, 60, 96, &TV_DRUM}}, {0, 0, 1, 1, 0, 3, 2, 0}},
    {"IV  RETURN TO THE CRYPT", 60, 8, 127, 75, 35, {
        {ENGI_GURDY, "VIELLE", 0, 32, 90, 90, &RT_VIELLE},
        {7, "CATHEDRAL", 7, 8, 105, 70, &RT_ORGAN},
        {5, "MONKS", 7, 8, 115, 72, &RT_MONKS},
        {ENGI_DRUM, "TOMB DRUMS", 1, 64, 100, 92, &RT_DRUM}}, {1, 0, 0, 2, 0, 3, 0, 0}},
};

/* the tavern harp: each bar's chord broken in eighths (root, fifth, octave, tenth, twelfth, tenth, octave, fifth) */
static void build_harp(void)
{
    static const uint8_t ROOT[8][3] = {{50, 53, 57}, {48, 52, 55}, {50, 53, 57}, {43, 47, 50},     /* Dm C Dm G */
                                       {48, 52, 55}, {41, 45, 48}, {45, 48, 52}, {50, 53, 57}};    /* C F Am Dm */
    static const uint8_t SHAPE[8][2] = {{0, 0}, {2, 0}, {0, 12}, {1, 12}, {2, 12}, {1, 12}, {0, 12}, {2, 0}};
    uint32_t b, k;
    for (b = 0; b < 8u; b++)
        for (k = 0; k < 8u; k++)
            TV_HARP.n[b * 8u + k][0] = (uint8_t)(ROOT[b][SHAPE[k][0]] + SHAPE[k][1]);
}

static void section_load(const section_t *s)
{
    uint32_t i;
    song.g[G_BPM] = (int16_t)s->bpm;
    song.g[G_RSIZE] = (int16_t)s->rsize;
    song.g[G_TAPE] = (int16_t)s->tape;
    song.g[G_CRSH] = (int16_t)s->crsh;
    for (i = 0; i < NTRK; i++)
        load_part(&trk[i], &s->p[i]);
}

static void frame_out(FILE *v)
{
    static uint8_t rgb[240u * 240u * 3u];
    uint32_t i;
    ui.force = 1;
    ui.hot_t = 0;
    ui.bpm_t = 0;
    ui_draw();
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = swap16(host_screen[i]);
        rgb[3 * i] = (uint8_t)((c >> 11) * 255u / 31u);
        rgb[3 * i + 1] = (uint8_t)(((c >> 5) & 63u) * 255u / 63u);
        rgb[3 * i + 2] = (uint8_t)((c & 31u) * 255u / 31u);
    }
    fwrite(rgb, 1, sizeof rgb, v);
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "demo_piece.wav";
    FILE *f, *v = argc > 2 ? fopen(argv[2], "wb") : 0, *cue;
    const uint32_t gap = FS / 2u, tail = 9u * FS;     /* a breath between sections; the hall's tail at the end */
    uint32_t s, i, t = 0, frames, next_frame = 0, start[5], end[4];
    int32_t o[2 * CTL];
    char cpath[600];
    rep = fopen("/dev/null", "w");                     /* (ui_render's lint hooks report here) */
    build_harp();
    ui_power_on();
    pal(UI_CRYPT_INDEX);
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    song.g[G_RTYPE] = 2;                               /* HALL */
    fx.rtype = 2;
    song.g[G_RDAMP] = 70;
    song.batt_raw = 600;
    usb.config = 0;
    ui.home = 1;
    rev_clear();
    hall_clear();
    for (s = 0, i = 0; s < 4u; s++) {                 /* the timeline: each section's first and last sample */
        start[s] = i;
        end[s] = i + (uint32_t)((uint64_t)SEC[s].bars * 4u * 60u * FS / SEC[s].bpm);
        i = end[s] + gap;
    }
    start[4] = end[3];
    frames = end[3] + tail;
    if (!(f = fopen(path, "wb")))
        return 1;
    wav_hdr(f, frames);
    snprintf(cpath, sizeof cpath, "%s.cues", path);    /* the sections' times, for the video's titles */
    if ((cue = fopen(cpath, "w"))) {
        for (s = 0; s < 4u; s++)
            fprintf(cue, "%.3f\t%s\t%s · %s · %s · %s\t%u\n", start[s] / (double)FS, SEC[s].title, SEC[s].p[0].name,
                    SEC[s].p[1].name, SEC[s].p[2].name, SEC[s].p[3].name, SEC[s].bpm);
        fprintf(cue, "%.3f\tEND\t-\t0\n", frames / (double)FS);
        fclose(cue);
    }
    for (s = 0; t < frames; t += CTL) {
        if (s < 4u && t >= start[s] && t < start[s] + CTL) {
            section_load(&SEC[s]);
            transport_req = 1;
        }
        if (s < 4u && t >= end[s] && t < end[s] + CTL) {
            transport_req = 2;                         /* the section ends: its notes ring into the hall */
            s++;
        }
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            wav_put(f, o[2 * i], o[2 * i + 1]);
            scope_buf[scope_w++ & (SCOPE_N - 1u)] = (int16_t)clamp(o[2 * i], -32768, 32767);   /* as audio.c */
        }
        if (v && t + CTL > next_frame) {               /* 30 fps: the screen as it stands after this block */
            uint32_t sec = 0, bar;
            while (sec < 3u && t >= start[sec + 1]) sec++;
            bar = (uint32_t)((uint64_t)(t > start[sec] ? t - start[sec] : 0) * SEC[sec].bpm / (4u * 60u * FS));
            song.sel = SEC[sec].show[(bar / 2u) & 7u];
            fm1_ms = (uint32_t)((uint64_t)t * 1000u / FS);
            frame_out(v);
            next_frame += FS / 30u;
        }
    }
    fclose(f);
    if (v)
        fclose(v);
    printf("%s: %.1f s\n", path, frames / (double)FS);
    return 0;
}
