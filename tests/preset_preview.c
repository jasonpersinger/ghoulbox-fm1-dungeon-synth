/* SPDX-License-Identifier: GPL-3.0-only */
/* GHOULBOX: a short preview of every factory preset, as the device plays it: the preset's own suggested pattern
 * (PAT(n); MELODY when it has none) twice at 120 BPM in 1/16 steps, then the tail, with the power-on room (HALL,
 * TAPE 40). For the instrument picker page (tools/preset_page.py).
 *   build/host/preset_preview OUTDIR   -> OUTDIR/<engine>_<nn>.wav, and an index on stdout (engine, nn, name, pattern)
 * Not a test. */
#define main hostsim_main
#include "hostsim.c"
#undef main

static void render(const char *dir, uint32_t e, uint32_t k)
{
    const preset_t *p = &ENGINES[e]->presets[k];
    uint32_t pat = p->pat ? p->pat : 3u, t, i;     /* 3: MELODY */
    uint32_t frames = (uint32_t)(5.5 * FS);
    int32_t o[2 * CTL];
    char path[512];
    FILE *f;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    song.g[G_RTYPE] = 2;
    song.g[G_TAPE] = 40;
    rev_clear();
    hall_clear();
    fx.rtype = 2;
    host_preset(&trk[0], e, k);
    for (t = 0; t < 16u; t++) {                    /* as ui.c load_pat16 */
        uint8_t n = PATTERNS[pat - 1u].note[t], fl = PATTERNS[pat - 1u].flags[t];
        put_step(&trk[0], t, n ? 1u : 0u, &n, (fl & 4u) ? ST_TIE : n ? ST_NOTE : ST_REST, n ? fl & 3u : 0u);
        trk[0].step[t].vel = n ? 96 : 0;
    }
    trk[0].p[P_SLEN] = 16;
    snprintf(path, sizeof path, "%s/%s_%02u.wav", dir, ENGINES[e]->name, k);
    if (!(f = fopen(path, "wb"))) {
        perror(path);
        return;
    }
    wav_hdr(f, frames);
    transport_req = 1;
    for (t = 0; t < frames; t += CTL) {
        if (t == 4u * FS)
            transport_req = 2;                         /* two passes of 2 s, then 1.5 s of tail */
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            wav_put(f, o[2 * i], o[2 * i + 1]);
    }
    fclose(f);
    printf("%s\t%u\t%s\t%s\n", ENGINES[e]->name, k, p->name, PATTERNS[pat - 1u].name);
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
