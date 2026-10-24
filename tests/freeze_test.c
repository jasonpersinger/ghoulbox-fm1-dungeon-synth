/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GHOULBOX (on Felucca by Leo Kuroshita, Hügelton Instruments) */
/* GHOULBOX 2.4: SEQ TOOLS > F4 FREEZE (ui_tools.c TL_FREEZE). The selected track's steps 1..LEN as they last played:
 * a step whose CHANCE let it play keeps its notes at CHANCE 100, one that did not is emptied; one undo (SAVE held);
 * DREAM's EVOL off (it would change them again). Not before the track has played since its steps came */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

#define NCH 8u                                           /* steps 2..9: CHANCE 50 */
static void setup(void)
{
    uint32_t i;
    ui_power_on();
    host_tracks_init();
    song.sel = 0;
    memset(trk[0].step, 0, sizeof trk[0].step);
    trk[0].p[P_SLEN] = 10;
    for (i = 0; i < 1u + NCH; i++) {
        trk[0].step[i].n = 1;
        trk[0].step[i].note[0] = (uint8_t)(48 + i);
        trk[0].step[i].vel = 100;
        if (i)
            step_set_chance(&trk[0].step[i], 50);
    }
    trk[0].step[12].n = 1;                               /* past LEN: never touched */
    trk[0].step[12].note[0] = 70;
    step_set_chance(&trk[0].step[12], 50);
}
static void play_ms(uint32_t ms) { uint32_t n = FS / 1000u * ms; while (n >= CTL) { events_block(CTL); n -= CTL; } }

int main(void)
{
    int bad = 0, ok;
    uint32_t i, kept = 0, gone = 0;
    uint64_t heard;
    step_t before[NSTEP];

    /* not played yet: nothing to freeze */
    setup();
    memcpy(before, trk[0].step, sizeof before);
    tools_key(TL_FREEZE);
    bad += check("FREEZE before the track has played: nothing changed", !memcmp(before, trk[0].step, sizeof before));

    /* played a few passes, then frozen */
    setup();
    song.g[G_DREAM] = 1;                                 /* EVOL ON */
    seq_start();
    play_ms(8000);
    seq_stop();
    heard = seq_heard[0];
    memcpy(before, trk[0].step, sizeof before);
    tools_key(TL_FREEZE);
    ok = trk[0].step[0].n == 1u && step_chance(&trk[0].step[0]) == 100u;
    for (i = 1; i <= NCH; i++) {
        if ((heard >> i) & 1u) {
            ok &= trk[0].step[i].n == 1u && trk[0].step[i].note[0] == 48 + i && step_chance(&trk[0].step[i]) == 100u;
            kept++;
        } else {
            ok &= !trk[0].step[i].n;
            gone++;
        }
    }
    bad += check("FREEZE: the steps that played kept at CHANCE 100, the ones that did not emptied", ok && kept && gone);
    bad += check("FREEZE: steps past LEN untouched", !memcmp(&trk[0].step[12], &before[12], sizeof(step_t)));
    bad += check("FREEZE: DREAM's EVOL off", song.g[G_DREAM] == 0);
    undo_swap();
    bad += check("FREEZE: one undo (SAVE held) brings the steps back", !memcmp(before, trk[0].step, sizeof before));

    puts(bad ? "FREEZE TEST FAILED" : "freeze tests passed");
    return bad != 0;
}
