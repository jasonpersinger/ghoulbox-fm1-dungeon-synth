/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GHOULBOX (on Felucca by Leo Kuroshita, Hügelton Instruments) */
/* GHOULBOX 2.4: MENU > SYSTEM > SLEEP, OFF / 30 / 60 / 90 MIN. After that long with no input on the panel (what SCREEN
 * OFF counts: ui.c scrn.idle; a MIDI note in counts too: midi_control.c midi_note_ms), the music fades out over two minutes (fx.c fade_to), the sequencer stops and the
 * output stays silent, a held drone too, until the panel is touched: the level comes back, the music as it was left.
 * For a game night, study or sleep. Kept in a byte no engine uses, saved with the settings; any other value is OFF */
#define ui_sleep (favorites.factory[15][13])
#ifndef SLEEP_MS_PER_MIN
#define SLEEP_MS_PER_MIN 60000u                         /* (tests/sleep_test.c shortens the minute and the fade) */
#endif
#ifndef SLEEP_FADE_MS
#define SLEEP_FADE_MS 120000u
#endif
#define SLEEP_N 4u
static const uint8_t SLEEP_MIN[SLEEP_N] = {0, 30, 60, 90};
static const char *const SLEEP_NAME[SLEEP_N] = {"OFF", "30 MIN", "60 MIN", "90 MIN"};
enum { SL_AWAKE, SL_FADING, SL_ASLEEP };
static uint8_t sleep_st;
static uint32_t sleep_get(void) { return ui_sleep < SLEEP_N ? ui_sleep : 0u; }   /* the MENU's value: 0 OFF .. 3 */

static void sleep_wake(void)
{
    sleep_st = SL_AWAKE;
    fade_to(FADE_FULL, 50u);
}

/* ui_input, every pass (after scr_input has seen the panel) */
static void sleep_tick(void)
{
    uint32_t lim = SLEEP_MIN[sleep_get()] * SLEEP_MS_PER_MIN, last = scrn.idle, m = midi_note_ms;
    int idle;
    if (m && (int32_t)(m - last) > 0)                   /* (a MIDI note in after the panel's last touch: playing) */
        last = m;
    idle = lim && (int32_t)(fm1_ms - last) >= (int32_t)lim;   /* (signed: robust to a wrap) */
    switch (sleep_st) {
    case SL_AWAKE:
        if (idle) {
            fade_to(0u, SLEEP_FADE_MS);
            sleep_st = SL_FADING;
        }
        break;
    case SL_FADING:
        if (!idle)
            sleep_wake();
        else if (!fade_level()) {
            if (song.playing)
                seq_stop();
            sleep_st = SL_ASLEEP;
        }
        break;
    default:
        if (!idle)
            sleep_wake();
        break;
    }
}
