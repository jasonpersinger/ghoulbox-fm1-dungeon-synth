/* SPDX-License-Identifier: GPL-3.0-only */
/* GHOULBOX 1.4's saved flash on this firmware (tests/fixtures/ghoulbox-1.4.fix, written by GHOULBOX 1.4's own code:
 * tests/gen_ghoulbox14_fixture.c): the power-on boot reads it, then a project load and a user preset load.
 *   the settings: SCENE CHAPEL, USB SERIAL OFF, GURDY's starred sounds (favorites.factory[14]), CRYPT; the bytes
 *     Felucca 1.1.5 .. 1.4.1 put its own settings in (SCREEN OFF, CLICK / COUNT-IN / SCALE LEDS) read as their
 *     defaults, not as GHOULBOX's SCENE and marks; RESTORE LAST OFF (GHOULBOX 1.5: once, the scenes play)
 *   the project (FUN8): every track's engine, sound and its 91 values (the ENGINE values E1..E8 at P_E0.. now),
 *     HALL, TAPE, CRSH and the tempo, FM6's SLOT F12 (a GHOULBOX patch) and its patch, Felucca's new values at
 *     their defaults
 *   the user preset: GURDY with its values */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int32_t fm1_enc_take(uint32_t e) { (void)e; return 0; }
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
static void lcd_sync(void) {}
static void lcd_power(uint32_t s) { (void)s; }
static void lcd_wake_now(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{ (void)x; (void)y; (void)w; (void)h; (void)p; }
#define FELUCCA_FLASH 1
#include "../firmware/src/gfx.c"
#include "../firmware/src/panel.c"
#include "../firmware/src/ui.c"
#include "../firmware/src/menu_items.c"
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
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}
#define FL_FAR(fn) (fn)
#define FL_JEDEC_OK(id) ((id) == 0x856014u)                          /* as hal/fm1_flash_map.h (FM-1) */
#include "../firmware/src/storage.c"
#include "../firmware/src/upreset.c"
#include "../firmware/src/project.c"

#define NP14 91u                                        /* GHOULBOX 1.4's P_COUNT; its E1..E8 at 83.. */
#define PE0_14 83u
static struct { int16_t eng, preset, p[NP14]; } t14[NTRK];
static int16_t g14[27];

static int check(const char *what, int ok)
{
    printf("ghoulbox 1.4: %-86s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

static int load_fixture(const char *path)
{
    FILE *f = fopen(path, "rb");
    char magic[8];
    uint32_t off;
    if (!f || fread(magic, 1, 8, f) != 8 || memcmp(magic, "GB14FIX1", 8))
        return 0;
    for (uint32_t i = 0; i < NTRK; i++)
        if (fread(&t14[i], 2, 2 + NP14, f) != 2 + NP14)
            return 0;
    if (fread(g14, 2, 27, f) != 27)
        return 0;
    memset(nor, 0xFF, sizeof nor);
    while (fread(&off, 4, 1, f) == 1)
        if (off > sizeof nor - 4096u || fread(nor + off, 1, 4096, f) != 4096)
            return 0;
    fclose(f);
    return 1;
}

/* track i's values as GHOULBOX 1.4 saved them, in this firmware's ids: the shared ones as they were, E1..E8 */
static int same_values(const int16_t *p, uint32_t i)
{
    for (uint32_t k = 0; k < NP14; k++) {
        uint32_t id = k < PE0_14 ? k : P_E0 + (k - PE0_14);
        if (p[id] != t14[i].p[k]) {
            printf("  track %u value %u (now %u): %d, saved %d\n", i + 1u, k, id, p[id], t14[i].p[k]);
            return 0;
        }
    }
    return 1;
}

int main(int argc, char **argv)
{
    int bad = 0, ok = 1;
    uint32_t i;
    if (!load_fixture(argc > 1 ? argv[1] : "tests/fixtures/ghoulbox-1.4.fix")) {
        printf("ghoulbox 1.4: no fixture\n");
        return 1;
    }
    chain_defaults(&chain_config);
    host_tracks_init();
    panel = PANEL_DEFAULT;
    fm6_init();
    {   /* project.c persist_boot, but the user sample slots' scan (XIP flash: none on the host) */
        persist_t p;
        int n = st_load(OBJ_SETTINGS, &p, sizeof p);
        if (settings_import(&p, n))
            persist_saved = p;
        else
            settings_fresh();
        for (i = 0; i < 4u; i++)
            if (!proj_import(&proj_scratch, &proj_slot[i], sizeof(project_store_t)))
                proj_fetch(i);
        up_boot();
    }
    settings_init();

    bad += check("settings: SCENE CHAPEL (favorites.factory[15][27])", ui_scene == 3u && str_eq(GB_SCENES[ui_scene].name, "CHAPEL"));
    bad += check("settings: USB SERIAL OFF as saved", (ui_prefs & PREF_SERIAL_OFF) != 0u);
    bad += check("settings: GURDY's starred sounds (favorites.factory[14]) kept", favorites.factory[ENGI_GURDY][0] == 0x05u);
    bad += check("settings: the CRYPT palette", str_eq(UI_PALETTES[settings.palette].name, "CRYPT"));
    bad += check("settings: SCREEN OFF NEVER (not SCENE's byte read as SCREEN OFF)", scr_get() == 0u && ui_scr == 0u);
    bad += check("settings: CLICK OFF, CLICK LEVEL MID, COUNT-IN OFF, SCALE LEDS OFF (not GHOULBOX's marks)",
                 rp_get(0) == 0u && rp_get(1) == 1u && rp_get(2) == 0u && ui_rec_prefs == 0u);   /* (its byte: none of the marks) */
    bad += check("settings: RESTORE LAST ON (OFF in 1.5, ON again once in 2.1), the three marks set",
                 !(ui_prefs & PREF_RESTORE_OFF) && (favorites.factory[15][28] & 7u) == 7u);

    bad += check("project A: its slot is used", project_used(0));
    project_load(0);
    for (i = 0; i < NTRK; i++)
        ok &= trk[i].eng_req == (uint32_t)t14[i].eng && trk[i].preset == (uint32_t)t14[i].preset;
    bad += check("project A: every track's engine and sound (GURDY, FM6, SAMPLE BOWED PSALT, ..)",
                 ok && trk[0].eng_req == ENGI_GURDY && trk[1].eng_req == ENGI_FM6 && trk[2].eng_req == ENGI_SAMPLE);
    for (i = 0, ok = 1; i < NTRK; i++)
        ok &= same_values(trk[i].p, i);
    bad += check("project A: all 91 saved values of every track (E1..E8 at their new ids)", ok);
    bad += check("project A: GURDY's WHL 99, WOBL 77", trk[0].p[P_E3] == 99 && trk[0].p[P_E7] == 77);
    for (i = 0, ok = 1; i < 27u; i++)
        ok &= song.g[i] == g14[i];
    bad += check("project A: every global as saved: HALL, TAPE 40, CRSH 35, 66 BPM", ok && song.g[G_RTYPE] == 2 &&
                 str_eq(GP[G_RTYPE].names[2], "HALL") && song.g[G_TAPE] == 40 && song.g[G_CRSH] == 35 && song.g[G_BPM] == 66);
    {
        uint8_t f12[FP_SIZE + 1u];
        fm6_unpack(FM6_FACTORY[11], f12);
        bad += check("project A: FM6 SLOT F12, GHOULBOX's patch (FR HORN .. TUBA after Felucca's 8)",
                     trk[1].p[P_E7] == 11 && FM6_NFACTORY == 15u && !strcmp(N_FM6_PATCH[11], "F12") &&
                     !memcmp(fm6_patch[1], f12, FP_SIZE));
    }
    for (i = 0, ok = 1; i < NTRK; i++)
        ok &= trk[i].p[P_LN0] == TP[P_LN0].def && trk[i].p[P_SPRD] == TP[P_SPRD].def && trk[i].p[P_SQNT] == TP[P_SQNT].def;
    bad += check("project A: Felucca's new values at their defaults (lane levels, SPREAD, QUANTIZE)", ok);

    bad += check("user preset 1: used, GURDY", up_used(0) && up_rec(0)->engine == ENGI_GURDY);
    song.sel = 3;
    up_load(0);
    bad += check("user preset 1: loads GURDY with WHL 99, WOBL 77", trk[3].eng_req == ENGI_GURDY &&
                 trk[3].p[P_E3] == 99 && trk[3].p[P_E7] == 77);

    printf("%s\n", bad ? "GHOULBOX 1.4 COMPATIBILITY TEST FAILED" : "GHOULBOX 1.4 compatibility test passed");
    return bad != 0;
}
