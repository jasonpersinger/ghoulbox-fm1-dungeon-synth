/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GHOULBOX (on Felucca by Leo Kuroshita, Hügelton Instruments) */
/* GHOULBOX 2.4: HOME > SCENES (after DREAM). Its first five white keys (F3 G3 A3 B3 C4) switch the music to DUNGEON,
 * CRYPT, TAVERN, CHAPEL or BLANK while it plays: a fade out, the scene loaded (ui.c gb_scene_load: sounds, patterns,
 * tempo, HALL, TAPE), a fade in; the transport as it was. The keys there never sound */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int32_t peak, bad_sample;
static void render(uint32_t n)
{
    int32_t o[2 * CTL];
    uint32_t i, k;
    while (n) {
        k = n < CTL ? n : CTL;
        events_block(k);
        mix_block(o, k);
        for (i = 0; i < 2u * k; i++) {
            int32_t a = o[i] < 0 ? -o[i] : o[i];
            if (a > peak) peak = a;
            if (a > 32767 * 256) bad_sample = 1;         /* (far past full scale: something broke) */
        }
        n -= k;
    }
}
static void run_ms(uint32_t ms)
{
    uint32_t i;
    for (i = 0; i < ms / 16u; i++) {
        frame();
        render(FS * 16u / 1000u);
    }
}
static void scene_key(uint32_t k) { key_down(k); frame(); key_up(k); frame(); }
static const char *preset_name(uint32_t i) { return ENGINES[trk[i].engine % NENGINES]->presets[trk[i].preset].name; }

int main(void)
{
    int bad = 0, ok;
    uint32_t i, n;

    ui_power_on();                                       /* DUNGEON at power-on */
    go_title("SCENES");
    bad += check("HOME > SCENES: the page is there, after DREAM", str_eq(cur_page()->title, "SCENES") && !ui.home);
    seq_start();
    run_ms(500);
    n = gates();
    scene_key(white(1));                                   /* G3: CRYPT */
    bad += check("SCENES: a key does not sound", gates() == n);
    run_ms(4000);
    ok = !strcmp(preset_name(0), "LOW DRONE") && song.g[G_BPM] == 60 && song.playing && fade_level() == FADE_FULL;
    bad += check("SCENES: G3 switches to CRYPT (its sounds, tempo), still playing, full level after the fade", ok);
    scene_key(white(7));                                   /* F4: not a scene */
    run_ms(2000);
    bad += check("SCENES: a key past the five changes nothing", !strcmp(preset_name(0), "LOW DRONE") &&
                 song.g[G_BPM] == 60);

    /* two presses during one switch: the last one wins */
    scene_key(white(2));                                   /* TAVERN .. */
    run_ms(300);
    scene_key(white(3));                                   /* .. then CHAPEL at once */
    run_ms(5000);
    bad += check("SCENES: pressed again during a switch: the last one plays (CHAPEL)", song.g[G_BPM] == 66 &&
                 fade_level() == FADE_FULL);

    /* twenty switches in a row while playing: no broken samples, the level back at the end */
    bad_sample = 0;
    for (i = 0; i < 20u; i++) {
        scene_key(white(i % 5u));
        run_ms(700);
    }
    run_ms(5000);
    bad += check("SCENES: 20 switches while playing: no broken sample, full level after", !bad_sample &&
                 fade_level() == FADE_FULL && song.playing);

    puts(bad ? "SCENE TEST FAILED" : "scene tests passed");
    return bad != 0;
}
