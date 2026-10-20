/* SPDX-License-Identifier: GPL-3.0-only */
/* GHOULBOX 1.4's flash, for tests/ghoulbox_compat_test.c: a project, a user preset and the settings as GHOULBOX 1.4
 * saved them (its own code), written to tests/fixtures/ghoulbox-1.4.fix. Built against the ghoulbox-v1.4 sources,
 * not this tree's (the point is 1.4's bytes):
 *   git worktree add /tmp/gb14 ghoulbox-v1.4 && (cd /tmp/gb14 && ./build.sh)
 *   cc -O1 -w -I/tmp/gb14/build/gen -I/tmp/gb14/firmware/src -I/tmp/gb14/tests -o gen tests/gen_ghoulbox14_fixture.c -lm
 *   ./gen tests/fixtures/ghoulbox-1.4.fix
 * The file: "GB14FIX1"; the four tracks as saved (engine, preset, then P_COUNT = 91 int16 parameters each); the 27
 * globals (int16); then the flash: (offset u32, 4096 bytes) for each sector of the projects, the user presets and
 * the settings that is not blank, to the end of the file. */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int32_t fm1_enc_take(uint32_t e) { (void)e; return 0; }
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{ (void)x; (void)y; (void)w; (void)h; (void)p; }
#define FELUCCA_FLASH 1
#include "gfx.c"
#include "panel.c"
#include "ui.c"
#include "menu_items.c"
static void panel_setup(void) {}

static uint8_t nor[0x100000], flash_ok = 1;
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    for (uint32_t i = 0; i < n; i++) nor[off + i] &= s[i];
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0; }
static void fl_plain_window_init(void) {}
#define FL_FAR(fn) (fn)
#define FL_JEDEC_OK(id) ((id) == 0x856014u)                          /* as hal/fm1_flash_map.h (FM-1) */
#include "storage.c"
#include "upreset.c"
#include "project.c"

static uint32_t preset_named(const engine_t *e, const char *name)   /* an engine's factory sound by name */
{
    for (uint32_t k = 0; k < e->npresets; k++)
        if (str_eq(e->presets[k].name, name))
            return k;
    fprintf(stderr, "no %s on %s\n", name, e->name);
    exit(1);
}

int main(int argc, char **argv)
{
    FILE *f;
    uint32_t i, k;
    _Static_assert(P_COUNT == 91u && G_COUNT == 27u && ENGI_GURDY == 14u, "GHOULBOX 1.4's counts");
    if (argc != 2) { fprintf(stderr, "usage: gen OUT.fix\n"); return 2; }
    memset(nor, 0xFF, sizeof nor);
    chain_defaults(&chain_config);
    host_tracks_init();
    panel = PANEL_DEFAULT;
    fm6_init();
    /* the music: CHAPEL, then a GURDY, an FM6 on a GHOULBOX patch and a recording, every GHOULBOX global set */
    gb_scene_load(3);
    set_engine_of(&trk[0], ENGI_GURDY);
    apply_preset_to(&trk[0], preset_named(&ENG_GURDY, "DRONE WHEEL"));
    trk[0].engine = trk[0].eng_req;
    trk[0].p[P_E3] = 99;                                 /* WHL */
    trk[0].p[P_E7] = 77;                                 /* WOBL */
    set_engine_of(&trk[1], ENGI_FM6);
    apply_preset_to(&trk[1], 0);
    trk[1].engine = trk[1].eng_req;
    trk[1].p[P_E7] = 11;                                 /* SLOT F12: GHOULBOX's (F9..F15) */
    fm6_poll();
    set_engine_of(&trk[2], ENGI_SAMPLE);
    apply_preset_to(&trk[2], preset_named(&ENG_SAMPLE, "BOWED PSALT"));
    trk[2].engine = trk[2].eng_req;
    song.g[G_RTYPE] = 2;                                 /* HALL */
    song.g[G_TAPE] = 40;
    song.g[G_CRSH] = 35;
    song.g[G_BPM] = 66;
    if (project_save(0)) { fprintf(stderr, "project_save failed\n"); return 1; }
    song.sel = 0;
    if (up_store(0, "GB GURDY")) { fprintf(stderr, "up_store failed\n"); return 1; }
    /* the settings: SCENE CHAPEL, USB SERIAL OFF (marked), GURDY's first and third sound starred, CRYPT */
    favorites.factory[15][27] = 3;
    favorites.factory[15][30] |= PREF_SERIAL_OFF;
    favorites.factory[15][28] |= 1u;
    favorites.factory[ENGI_GURDY][0] = 0x05;
    settings.palette = 8;
    if (!str_eq(UI_PALETTES[settings.palette].name, "CRYPT")) { fprintf(stderr, "palette 8 is not CRYPT\n"); return 1; }
    settings_save();
    if (persist_pending) { fprintf(stderr, "settings not written\n"); return 1; }
    if (!(f = fopen(argv[1], "wb"))) return 1;
    fwrite("GB14FIX1", 1, 8, f);
    for (i = 0; i < NTRK; i++) {
        int16_t h[2] = {(int16_t)trk[i].eng_req, (int16_t)trk[i].preset};
        fwrite(h, 2, 2, f);
        fwrite(trk[i].p, 2, P_COUNT, f);
    }
    fwrite(song.g, 2, G_COUNT, f);
    for (k = 0; k < 0x100000u; k += 4096u) {
        int blank = 1;
        if (!((k >= 0x97000u && k < 0xA0000u) || (k >= 0xDC000u && k < 0xE0000u) || (k >= 0xFC000u && k < 0xFF000u)))
            continue;
        for (i = 0; i < 4096u && blank; i++) blank = nor[k + i] == 0xFFu;
        if (!blank) {
            fwrite(&k, 4, 1, f);
            fwrite(nor + k, 1, 4096, f);
            printf("sector %#x\n", k);
        }
    }
    fclose(f);
    printf("GHOULBOX 1.4 fixture: %s\n", argv[1]);
    return 0;
}
