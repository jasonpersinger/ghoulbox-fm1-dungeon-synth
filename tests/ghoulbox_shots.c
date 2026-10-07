/* SPDX-License-Identifier: GPL-3.0-only */
/* GHOULBOX: screenshots for the README / a release, in CRYPT, of what a GHOULBOX owner sees: the boot splash, then the
 * power-on scene (main.c felucca_init: GB_SCENE, 1/8 steps, 72 BPM, HALL, TAPE 40) on HOME, PRESETS, the step roll and
 * the MENU. Drawn by the firmware's own UI code (tests/ui_render.c's host setup).
 *   build/host/ghoulbox_shots OUTDIR   -> OUTDIR/ppm/CRYPT_<name>.ppm
 * Not a test. */
#define UI_RENDER_NO_MAIN
#include "ui_render.c"

static void gb_scene(void)
{
    uint32_t i;
    ui_power_on();
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        set_engine_of(t, GB_SCENE[i][0]);
        apply_preset_to(t, GB_SCENE[i][1]);
        t->engine = t->eng_req;
        load_pat16(t, PATTERNS[GB_SCENE[i][2] - 1u].note, PATTERNS[GB_SCENE[i][2] - 1u].flags);
        t->p[P_SDIV] = 1;
        t->seq_idx = 5;
    }
    song.g[G_BPM] = 72;
    song.g[G_RTYPE] = 2;
    song.g[G_TAPE] = 40;
    song.sel = 0;
    song.batt_raw = 600;
    usb.config = 0;
    song.playing = 1;
    for (i = 0; i < SCOPE_N; i++)                  /* a bowed tone: a soft saw with its octave */
        scope_buf[i] = (int16_t)((fmod(i / 96.0, 1.0) * 2.0 - 1.0) * 7000.0 + sin(i * 0.1309) * 3000.0);
    scope_w = 0;
}

static void shot(const char *dir, const char *name)
{
    uint32_t k;
    pal(UI_CRYPT_INDEX);                           /* (ui_power_on put the test's palette back) */
    ui.force = 1;
    for (k = 0; k < 8u; k++) { ui.hot_t = 0; ui.bpm_t = 0; ui_draw(); }
    write_ppm(dir, "CRYPT", name);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "build/gb_shots";
    char cmd[600];
    snprintf(cmd, sizeof cmd, "mkdir -p '%s/ppm'", dir);
    if (system(cmd))
        return 1;
    rep = fopen("/dev/null", "w");                 /* (ui_render's lint hooks report here) */
    pal(UI_CRYPT_INDEX);
    lcd_fill(0, 0, 240, 240, T_BG);                /* the boot splash, as main.c fm1_main draws it */
    draw_art_box(34, GB_SKULL_W, GB_SKULL_H, GB_SKULL, T_TEXT);
    draw_art_box(100, GB_LOGO_W, GB_LOGO_H, GB_LOGO, T_THEME);
    draw_text_box(0, 166, 240, &AF_S, "DUNGEON SYNTH", T_MID, 1);
    write_ppm(dir, "CRYPT", "boot");
    gb_scene(); ui.home = 1; shot(dir, "home");
    gb_scene(); go_page(GR_BROWSE); shot(dir, "presets");
    gb_scene(); go_page(GR_ROLL); ui.cursor = 4; shot(dir, "roll");
    gb_scene(); ui.menu = 1; ui.menu_sel = 0; shot(dir, "menu");
    return 0;
}
