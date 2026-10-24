/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GHOULBOX (on Felucca by Leo Kuroshita, Hügelton Instruments) */
/* GHOULBOX 2.4: the master fade (fx.c fade_to / fade_level) and MENU > SYSTEM > SLEEP (sleep.c). The minute and the
 * fade are shortened here (SLEEP_MS_PER_MIN, SLEEP_FADE_MS); the firmware's are 60 s and 2 minutes */
#define UI_TEST_NO_MAIN 1
#define SLEEP_MS_PER_MIN 100u
#define SLEEP_FADE_MS 1000u
#include "ui_test.c"

static int32_t peak;                                     /* the loudest output sample since peak_reset */
static void render(uint32_t n)
{
    int32_t o[2 * CTL];
    uint32_t i, k;
    while (n) {
        k = n < CTL ? n : CTL;
        mix_block(o, k);
        for (i = 0; i < 2u * k; i++)
            if (o[i] > peak || -o[i] > peak)
                peak = o[i] < 0 ? -o[i] : o[i];
        n -= k;
    }
}
static void run_ms(uint32_t ms)                          /* the UI a frame at a time, the audio between */
{
    uint32_t i;
    for (i = 0; i < ms / 16u; i++) {
        frame();
        render(FS * 16u / 1000u);
    }
}

int main(void)
{
    int bad = 0, ok;
    uint32_t i, last, mono = 1;

    ui_power_on();
    fade_to(0u, 100u);
    last = fade_level();
    for (i = 0; i < 22u; i++) {                          /* 22 x 5 ms: 100 ms and a block's leeway */
        render(FS * 5u / 1000u);
        mono &= fade_level() <= last;
        last = fade_level();
    }
    bad += check("fade: fade_to(0, 100 ms) falls steadily and reaches 0 in 100 ms", mono && fade_level() == 0u);
    fade_to(FADE_FULL, 10u);
    render(FS * 20u / 1000u);
    bad += check("fade: fade_to(full, 10 ms) comes back to full", fade_level() == FADE_FULL);

    /* SLEEP 30 MIN (30 x 100 ms here): the music fades over SLEEP_FADE_MS, the sequencer stops, silence after */
    ui_power_on();
    ui_sleep = 1;                                        /* 30 MIN */
    scrn.idle = fm1_ms;
    seq_start();
    trk_note_on(&trk[1], 50, 100);                       /* a note held through it all (a drone: never let go) */
    run_ms(2800);
    ok = fade_level() == FADE_FULL && song.playing;
    bad += check("sleep: before its time, full level and the music playing", ok);
    run_ms(200 + SLEEP_FADE_MS + 200);
    ok = fade_level() == 0u && !song.playing;
    peak = 0;
    run_ms(200);
    bad += check("sleep: after its time and the fade, level 0, the sequencer stopped, silence (a held note too)",
                 ok && peak == 0);

    /* any panel input wakes it: the level back, the music as it was left (stopped) */
    turn(EN_K1, 1);
    run_ms(200);
    bad += check("sleep: a knob turned wakes it: full level again", fade_level() == FADE_FULL);

    /* a value from another firmware in its byte: OFF */
    ui_power_on();
    ui_sleep = 7;
    scrn.idle = fm1_ms;
    seq_start();
    run_ms(10000);
    bad += check("sleep: an unknown value (7) is OFF: never fades", fade_level() == FADE_FULL && song.playing);
    ui_sleep = 0;

    puts(bad ? "SLEEP TEST FAILED" : "sleep tests passed");
    return bad != 0;
}
