/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GHOULBOX (on Felucca by Leo Kuroshita, Hügelton Instruments) */
/* GHOULBOX 2.4: HOME > SCENES (DREAM tapped again). Its first five white keys, F3 G3 A3 B3 C4, switch the music to
 * DUNGEON, CRYPT, TAVERN, CHAPEL or BLANK (engines.c GB_SCENES) while it plays: the master fades out (fx.c fade_to),
 * the scene loads as at power-on but live (ui.c gb_scene_apply: each part's engine switches with its own crossfade,
 * voice.c engine_block), then fades back in; the transport keeps going, the page and the selection stay. A key pressed
 * during a switch replaces the one waiting. For a game master: the party walks into the tavern, one key. The keys
 * there never sound (seq.c: song.grid 5). The music before is replaced (RESTORE LAST's autosave keeps going) */
#define SC_OUT_MS 1500u
#define SC_IN_MS 2000u
static const uint8_t SC_KEY[GB_NSCENE] = {0, 2, 4, 6, 7};   /* key index from F3: F3 G3 A3 B3 C4 */
static uint8_t scene_req = 0xFF, scene_cur;              /* the switch waiting (0xFF none); the last one played */

static int scene_keys_on(void)
{
    return !ui.home && !ui.menu && !ui.confirm && !ui.layer && cur_page()->graph == GR_SCENES;
}
static void scene_keys(uint32_t notes)
{
    uint32_t i;
    for (i = 0; i < GB_NSCENE; i++)
        if ((notes >> SC_KEY[i]) & 1u) {
            if (chain_busy()) {
                ui_message("STOP THE SONG FIRST");
                return;
            }
            scene_req = (uint8_t)i;
            fade_to(0u, SC_OUT_MS);
            ui_say("SCENE ", GB_SCENES[i].name);
            ui.force = 1;
            return;
        }
}
/* ui_input, every pass: the switch, once the fade is down */
static void scene_tick(void)
{
    if (scene_req >= GB_NSCENE || fade_level())
        return;
    gb_scene_apply(scene_req, 1);
    scene_cur = scene_req;
    scene_req = 0xFF;
    dream_forget();                                     /* (new steps: DREAM's piece and FREEZE's passes are gone) */
    fade_to(FADE_FULL, SC_IN_MS);
}
