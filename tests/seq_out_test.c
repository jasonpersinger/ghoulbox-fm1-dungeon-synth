/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GHOULBOX (on Felucca by Leo Kuroshita, Hügelton Instruments) */
/* GHOULBOX 2.4: MENU > MIDI > SEQ OUT. ON: what the sequencer plays (DREAM's parts too, being steps) goes out over
 * USB MIDI, each track on its own channel (seq.c trk_midi_ch), drum hits as their lanes' notes; STOP sends the
 * note-offs still owed. OFF (the default): the sequencer sends nothing, as before */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static uint32_t sent_on[16][128], sent_off[16][128], sent_n;
static void collect(uint32_t from)                       /* the packets queued since from */
{
    uint32_t i, p, st, ch;
    memset(sent_on, 0, sizeof sent_on);
    memset(sent_off, 0, sizeof sent_off);
    sent_n = 0;
    for (i = from; i != mo_w; i++) {
        p = midi_out_q[i % MQ];
        st = (p >> 8) & 0xF0u;
        ch = (p >> 8) & 15u;
        if (st == 0x90u && (p >> 24))
            sent_on[ch][(p >> 16) & 127u]++;
        else if (st == 0x80u || st == 0x90u)
            sent_off[ch][(p >> 16) & 127u]++;
        sent_n++;
    }
}
static void setup(void)                                  /* track 1: C4 on step 1 of 4; track 2: DRUM, a KICK on step 1 */
{
    uint32_t i;
    ui_power_on();
    host_tracks_init();
    host_preset(&trk[1], ENGI_DRUM, 0);
    for (i = 0; i < NTRK; i++) {
        memset(trk[i].step, 0, sizeof trk[i].step);
        trk[i].p[P_SLEN] = 4;
    }
    trk[0].step[0].n = 1;
    trk[0].step[0].note[0] = 60;
    trk[0].step[0].vel = 100;
    trk[1].step[0].hit = 1;                              /* KICK: note 36 */
    trk[1].step[0].vel = 100;
    song.g[G_ROUTE] = 0;                                 /* CH1-4: track i on channel i */
    usb.config = 1;                                      /* (a computer connected: MIDI OUT queues) */
}
static void play_ms(uint32_t ms) { uint32_t n = FS / 1000u * ms; while (n >= CTL) { events_block(CTL); n -= CTL; } }

int main(void)
{
    int bad = 0, ok;
    uint32_t mo;

    setup();
    ui_prefs2 &= (uint8_t)~(PREF_SEQ_OUT >> 16);
    frame();                                             /* (ui_input takes the setting) */
    mo = mo_w;
    seq_start();
    play_ms(2000);
    seq_stop();
    play_ms(50);
    collect(mo);
    bad += check("SEQ OUT OFF (the default): the sequencer sends no MIDI", sent_n == 0u);

    setup();
    ui_prefs2 |= (uint8_t)(PREF_SEQ_OUT >> 16);
    frame();
    mo = mo_w;
    seq_start();
    play_ms(2000);
    collect(mo);
    ok = sent_on[0][60] >= 1u && sent_off[0][60] >= 1u && sent_on[1][36] >= 1u;
    bad += check("SEQ OUT ON: track 1's C4 on channel 1, track 2's KICK as note 36 on channel 2, with note-offs", ok);
    ok = sent_on[2][60] == 0u && sent_on[0][36] == 0u;
    bad += check("SEQ OUT ON: nothing on the other channels or notes", ok);

    /* STOP with a note still held (GATE long): its note-off goes out */
    setup();
    ui_prefs2 |= (uint8_t)(PREF_SEQ_OUT >> 16);
    trk[0].p[P_SGATE] = 127;
    trk[0].step[1].time = ST_TIE;                        /* held into step 2 */
    frame();
    mo = mo_w;
    seq_start();
    play_ms(60);
    seq_stop();
    play_ms(50);
    collect(mo);
    bad += check("SEQ OUT ON: STOP sends the note-off still owed (every note-on has its note-off)",
                 sent_on[0][60] >= 1u && sent_off[0][60] == sent_on[0][60]);
    ui_prefs2 &= (uint8_t)~(PREF_SEQ_OUT >> 16);

    puts(bad ? "SEQ OUT TEST FAILED" : "seq out tests passed");
    return bad != 0;
}
