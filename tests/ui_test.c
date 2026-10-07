/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of the UI (firmware/src/ui*.c with the sound sources of hostsim.c;
 * the display and the HAL are stubs, the buttons and knobs are driven through them):
 *   SOUNDS     a sound load (PRESETS knob / page, TOOLS INIT, user preset) changes the sound only: the
 *              steps, LEN DIV SWING GATE, ARP, SCL, the SLICER and the mix stay the track's. Power-on:
 *              every sequencer empty.
 *   PATTERNS   SEQ > PATTERNS: KNOB 1 PAT (factory patterns, then user presets that hold one), OCT+
 *              LOAD; over the user's steps the REPLACE? dialog (OCT- / OCT+); the PRESETS hint.
 *   UNDO       a load keeps a copy of the track; SAVE held swaps back what the loads changed (again:
 *              redo); browsing keeps the copy from before the first load; power-on and projects: none.
 *   REC        a tap on every screen arms the selected track and starts PLAY, without navigation.
 *              Long REC never clears. On STEP, armed and playing: keys do not write the
 *              cursor step. With the ARP on, the arp's notes are recorded, not the keys.
 *   MIDI IN    GLO > SYSTEM ROUT: CH1-4 (channels 5..16 ignored) / SEL, note-offs follow their note-ons.
 *   SAVE       refused while playing ("STOP TO SAVE"); a used slot asks OVERWRITE? (OCT+ / OCT-).
 *   ACTIONS    PATTERNS, USER, PROJECT, TOOLS: the knobs pick, OCT+ does it, OCT- cancels or goes
 *              HOME, on release, never both together; the OCT LEDs (OCT- lit, OCT+ blinks).
 *   TRACKS     the PRESETS knob does nothing there; KNOB 1 is MUTE.
 *   GRID       SEQ > STEP on a DRUM track: white keys toggle the selected lane's steps of the page, black
 *              keys 1..8 select the lane (and only they play), 9 held = ACC, 10 / 11 the page; KNOB 1..4 STEP
 *              LANE HIT ACC; the key LEDs (hits, the playhead inverted, the lane, ACC, the page keys); a step's
 *              GM notes shown on their lanes and made the lane's own by an edit; live recording into the grid
 *              (lane keys on the grid, GM keys elsewhere; other GM drums as notes; no TIE holds); 12 BEAT from
 *              PATTERNS fills the grid (undo); sound loads keep it, and other engines play it as GM notes.
 *   ROLL       the rolling digits of the card values and the header BPM: strips only, the last frame static,
 *              direction, retarget, the snaps (fast turn, names, shape, page, track, palette, force).
 *   SOAK       #63: 150000 random frames (turns, pages, transport, REC / motion, wrapping clocks): no roll
 *              outlives ROLL_FRAMES, the cards always equal a forced redraw once the rolls are over.
 *   USB SERIAL #67: MENU > USB SERIAL ON (default, the descriptors as before) / OFF (audio + MIDI only, class 0):
 *              the menu, the settings, the descriptor at boot (built with the console, as the firmware: FELUCCA_CDC).
 * Built and run by tests/run_tests.sh. */
#ifndef FELUCCA_CDC
#define FELUCCA_CDC 1                                    /* (as src/felucca.c: the console built in, MENU > USB SERIAL) */
#endif
#include <stddef.h>
#include <stdint.h>
static uint32_t host_slots[3u * 0x14000u / 4u];          /* USR1..3 (zero: empty), as the flash at 0xA0000 */
#define SMP_USER_XIP(k) ((const uint8_t *)host_slots + (k) * SMP_USER_SIZE)
#define main hostsim_main
#include "hostsim.c"
#undef main

/* ------------------------------------------------------------ HAL stubs --- */
#define FM1_NCOL 11u
static const int8_t FM1_KEYMAP[6][FM1_NCOL];
static uint8_t fm1_led[FM1_NCOL], fm1_led_dim[FM1_NCOL];
static uint32_t host_dim_lo;                      /* hal/fm1_input.h: the glow MENU > LEDS asked for (1: DIM LO) */
static void fm1_led_dim_level(uint32_t lo) { host_dim_lo = lo; }
#define FM1_TICKS_PER_US 1u
static uint32_t host_ticks, host_pressed, host_notes;
static int32_t host_enc[7];
static uint32_t fm1_ticks(void) { return host_ticks; }
static uint32_t fm1_input_edges(int x) { uint32_t p = host_pressed; (void)x; host_pressed = 0; return p; }
static uint32_t fm1_input_note_edges(void) { uint32_t n = host_notes; host_notes = 0; return n; }
static int32_t fm1_enc_take(uint32_t e) { int32_t s = host_enc[e % 7u]; host_enc[e % 7u] = 0; return s; }
static void fm1_wdt_feed(void) {}
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
static uint16_t host_screen[240 * 240];
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
#ifdef UI_FILL_HOOK
    UI_FILL_HOOK(x, y, w, h);                     /* (tests/ui_render.c: what a fill covers is gone) */
#endif
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            host_screen[(y + j) * 240u + x + i] = (uint16_t)((c >> 8) | (c << 8));
}
static void lcd_sync(void) {}
#define SCOPE_N 512u                              /* audio.c: the HOME oscilloscope */
static int16_t scope_buf[SCOPE_N];
static uint32_t scope_w;
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    uint32_t i, j;
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++) host_screen[(y + j) * 240u + x + i] = p[j * w + i];
}
static struct { uint32_t stage; } felucca_dbg;
#define FELUCCA_FLASH 0
#ifndef FELUCCA_VERSION
#define FELUCCA_VERSION "TEST"
#endif
#include "../firmware/src/gfx.c"
#include "../firmware/src/panel.c"
#include "../firmware/src/ui.c"
#include "../firmware/src/icons.c"
#include "../firmware/src/ui_graph.c"
#include "../firmware/src/ui_draw.c"
#include "../firmware/src/ui_menu.c"
#include "../firmware/src/ui_input.c"
#include "../firmware/src/ui_layer.c"
#include "../firmware/src/upreset.c"
#include "../firmware/src/project.c"

static int check(const char *what, int ok)
{
    printf("ui: %-74s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

/* main.c felucca_init */
static void ui_power_on(void)
{
    uint32_t i;
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    memset(&chain, 0, sizeof chain);
    chain_defaults(&chain_config);
    memset(&ui, 0, sizeof ui);
    memset(&nm, 0, sizeof nm);                    /* NAME closed, no project name */
    proj_name[0] = 0;
    proj_cur = PROJ_NO_SLOT;
    memset(&favorites, 0, sizeof favorites);
    memset(&undo, 0, sizeof undo);
    memset(pat_last, 0, sizeof pat_last);
    memset(proj_slot, 0, sizeof proj_slot);
    memset(up_bank, 0, sizeof up_bank);
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    undo_depth++;
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        track_defaults(t);
        set_engine_of(t, TRK_DEF[i][0]);
        apply_preset_to(t, TRK_DEF[i][1]);
        t->engine = t->eng_req;
        track_defaults_steps(t);
        if (TRK_DEF[i][2])
            load_pat16(t, PATTERNS[TRK_DEF[i][2] - 1u].note, PATTERNS[TRK_DEF[i][2] - 1u].flags);
        pat_sig[i] = steps_sig(t);
        pat_last[i] = TRK_DEF[i][2];
    }
    undo_depth--;
    song.sel = 0;
    ui.home = 1;
    ui.force = 1;
    panel = PANEL_DEFAULT;
    settings.palette = UI_GREY_INDEX;
    palette_set(UI_GREY_INDEX);
    transport_req = 0;
    panic_req = 0;
    mi_r = mi_w = midi_in_overflow = 0;
    memset(midi_sel_on, 0, sizeof midi_sel_on);
    memset(midi_ch, 0, sizeof midi_ch);
    memset(midi_owners, 0, sizeof midi_owners);
    memset(midi_bend_q8, 0, sizeof midi_bend_q8);
    memset(midi_bend_target, 0, sizeof midi_bend_target);
    memset(kb_chn, 0, sizeof kb_chn);                   /* chord.c: no key, MIDI chord or last chord */
    memset(mchord, 0, sizeof mchord);
    memset(chord_last, 0, sizeof chord_last);
    perf_held = perf_act = kb_layer = perf_mask = 0;      /* the FX layer: nothing held */
    perf_kill = 0;
    perf_mask = 1u << panel.btn[B_FX];                  /* (ui_input sets them every frame) */
    kb_mask = layer_bits();
    perf_solo = 0;
    memset(&lys, 0, sizeof lys);
    perf_k[0] = perf_k[1] = perf_k[2] = perf_k[3] = 0;
    settings_hold = HOLD_DEF;
    fm1_in.buttons = fm1_in.notes = 0;
    memset(&motion, 0, sizeof motion);                  /* (main.c: no motion at power-on) */
    memset(motion_active, 0, sizeof motion_active);
    motion_base_valid = motion_full = 0;
}
/* card slot drawn with MOTION's mark (ui_draw.c card_mot: its icon, or a long label in the theme colour) */
static int card_motion(uint32_t slot) { return (card_mot >> slot) & 1u; }

/* one UI frame (input, then the drawing on the stub display) */
static void frame(void)
{
    host_ticks += 16000u;
    fm1_ms += 16u;
    ui_input();
    ui_draw();
}
static void press(uint32_t label)                 /* a tap: down, a frame, up, a frame */
{
    fm1_in.buttons |= 1u << panel.btn[label];
    host_pressed |= 1u << panel.btn[label];
    frame();
    fm1_in.buttons &= ~(1u << panel.btn[label]);
    frame();
}
static void frames(uint32_t ms) { uint32_t i; for (i = 0; i < ms / 16u; i++) frame(); }
static void hold(uint32_t label)                  /* held 0.8 s, then let go */
{
    uint32_t k;
    fm1_in.buttons |= 1u << panel.btn[label];
    host_pressed |= 1u << panel.btn[label];
    for (k = 0; k < 52u; k++)
        frame();
    fm1_in.buttons &= ~(1u << panel.btn[label]);
    frame();
}
static void turn(uint32_t role, int32_t s)
{
    host_enc[panel.enc[role]] += s * panel.dir[role];
    frame();
    host_ticks += 200000u;                        /* (no knob acceleration between the turns) */
}
static void stop_transport(void) { transport_req = 0; song.playing = 0; }
static int16_t stored_param(uint32_t slot, uint32_t track, uint32_t id)
{
    project_t p;
    return proj_import(&p, &proj_slot[slot], sizeof proj_slot[slot]) ? p.t[track].p[id] : -32768;
}
static uint8_t stored_note(uint32_t slot, uint32_t track, uint32_t step)
{
    project_t p;
    return proj_import(&p, &proj_slot[slot], sizeof proj_slot[slot]) ? p.t[track].step[step].note[0] : 0;
}
static int msg_is(const char *s) { return ui.msg_t && str_eq(ui.msg, s); }
static void my_steps(track_t *t)                  /* "recorded" steps */
{
    uint32_t i;
    track_defaults_steps(t);
    for (i = 0; i < 32u; i += 3u) {
        t->step[i].note[0] = (uint8_t)(40 + i);
        t->step[i].n = 1;
        t->step[i].time = ST_NOTE;
        t->step[i].vel = 99;
    }
    t->p[P_SLEN] = 32;
}

static void go_page(uint32_t graph)               /* the page that draws graph */
{
    uint32_t i;
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].graph == graph)
            break;
    ui.home = 0;
    ui.page = (uint8_t)i;
    page_entered();
}
static int steps_are(const track_t *t, uint32_t n)  /* steps 1..16 = PATTERNS[n], 17..64 empty, LEN 16 */
{
    uint32_t i;
    for (i = 0; i < NSTEP; i++) {
        uint8_t want = i < 16u ? PATTERNS[n].note[i] : 0u;
        if (t->step[i].note[0] != want || t->step[i].n != (want ? 1u : 0u))
            return 0;
    }
    return t->p[P_SLEN] == 16;
}
/* the sound of a track: engine, preset and the parameters that are not the track's own (param_kept) */
static int same_sound(const track_t *a, const track_t *b)
{
    uint32_t i;
    for (i = 0; i < P_COUNT; i++)
        if (!param_kept(i) && a->p[i] != b->p[i])
            return 0;
    return a->eng_req == b->eng_req && a->preset == b->preset;
}

/* the track's E values are DRUM's default kit (core.h DRUM_KIT_E: what a SAMPLE PERC sound loads as) */
static int perc_kit(const track_t *t)
{
    static const int16_t KIT[8] = DRUM_KIT_E;
    return !memcmp(&t->p[P_E0], KIT, sizeof KIT);
}
/* the GM drums a DRUM track plays: a kick, a snare and a closed hat each strike their lane and sound */
static int gm_hits_play(track_t *t)
{
    static const uint8_t N[3] = {36, 38, 42}, L[3] = {DV_KICK, DV_SNARE, DV_HATC};
    int32_t o[2u * CTL];
    uint32_t k, i, b, ok = 1, m = t->p[P_MUTE], mq = song.master_q12;
    t->p[P_MUTE] = 0;
    song.master_q12 = 4096;                       /* (the MASTER knob: main.c; 0 in this test) */
    for (b = 0; b < 256u && (t->xf_on || t->eng_req != t->engine); b++)   /* the engine switch done (its fade) */
        mix_block(o, CTL);
    for (k = 0; k < 3u; k++) {
        uint64_t e = 0;
        int lane = 0;
        trk_note_on(t, N[k], 100);
        for (b = 0; b < 16u; b++) {
            memset(o, 0, sizeof o);
            mix_block(o, CTL);
            for (i = 0; i < 2u * CTL; i++)
                e += (uint64_t)(o[i] < 0 ? -o[i] : o[i]);
            for (i = 0; i < NVOICE; i++)
                lane |= t->v[i].active && t->v[i].note == N[k] && (uint32_t)t->v[i].s[0] == L[k];
        }
        trk_note_off(t, N[k]);
        ok &= ENGINES[t->engine] == &ENG_DRUM && drum_lane(N[k]) == L[k] && lane && e > 0;
    }
    t->p[P_MUTE] = (int16_t)m;
    song.master_q12 = mq;
    return (int)ok;
}

/* sounds never touch the steps; the arp, the scale, the SLICER and the pattern parameters are the track's */
static void go_title(const char *title);              /* (below; GHOULBOX's torch check uses it) */
static int test_sound_loads(void)
{
    int bad = 0;
    uint32_t i, empty = 1;
    track_t *t = &trk[0], before;
    ui_power_on();
    for (i = 0; i < NTRK; i++)
        empty &= (uint32_t)seq_is_empty(&trk[i]);
    bad += check("power-on: the four sounds, every sequencer empty, no undo copy", empty && undo.trk == 0 && undo_depth == 0 &&
                 trk[3].eng_req == ENGI_DRUM && trk[3].preset == 0u && trk[0].preset == TRK_DEF[0][1]);
    my_steps(t);
    t->p[P_E0 + 1] = 77;                          /* a sound edit */
    t->p[P_AMODE] = 2;                            /* and the track's own settings */
    t->p[P_AOCT] = 3;
    t->p[P_SCALE] = 2;
    t->p[P_TRANS] = 5;
    t->p[P_SLCR] = SL_GATE;
    t->p[P_SLPAT] = 4;
    t->p[P_SDIV] = 1;
    t->p[P_LEVEL] = 90;
    before = *t;
    turn(EN_PRESET, 1);                           /* HOME: the next preset */
    bad += check("PRESETS on HOME loads the sound: steps, LEN, DIV untouched", t->preset == (uint8_t)((before.preset + 1u) % ENGINES[0]->npresets) &&
                 !memcmp(t->step, before.step, sizeof t->step) && t->p[P_SLEN] == 32 && t->p[P_SDIV] == 1);
    bad += check("..ARP, SCL, the SLICER and the mix stay the track's", t->p[P_AMODE] == 2 && t->p[P_AOCT] == 3 && t->p[P_SCALE] == 2 &&
                 t->p[P_TRANS] == 5 && t->p[P_SLCR] == SL_GATE && t->p[P_SLPAT] == 4 && t->p[P_LEVEL] == 90);
    bad += check("..and no warning about the sequence", !msg_is("T1 SEQ REPLACED"));
    for (i = 0; i < ENGINES[0]->npresets; i++)
        if (str_eq(ENGINES[0]->presets[i].name, "RAVE"))
            break;
    t->p[P_AMODE] = 0;
    t->preset = (uint8_t)(i - 1u);
    turn(EN_PRESET, 1);
    bad += check("RAVE (an ARP preset) leaves the arp off; it suggests pattern 13 ARP", t->preset == i && t->p[P_AMODE] == 0 &&
                 preset_pat_hint() == 12 && ui.ppick == 12 && str_eq(PATTERNS[12].name, "ARP"));
    t->p[P_AMODE] = 2;
    before = *t;
    for (i = t->preset; i < ENGINES[0]->npresets; i++)   /* several loads, into the next engine */
        turn(EN_PRESET, 1);
    bad += check("browsing on, into another engine: the steps still untouched", t->eng_req != before.eng_req &&
                 !memcmp(t->step, before.step, sizeof t->step) && t->p[P_AMODE] == 2);
    bad += check("browsing keeps the copy from before the first load", undo.trk == 1u && undo.what == UNDO_SOUND &&
                 undo.p[P_E0 + 1] == before.p[P_E0 + 1] && undo.eng == before.eng_req && undo.preset == before.preset);
    t->step[0].note[0] = 99;                      /* recorded after the loads */
    hold(B_SAVE);
    bad += check("SAVE held: the sound back (UNDO/REDO T1); steps recorded since stay", same_sound(t, &before) &&
                 t->step[0].note[0] == 99 && msg_is("UNDO/REDO T1"));
    bad += check("SAVE held does not open the SAVE pages", ui.home);
    hold(B_SAVE);
    bad += check("SAVE held again: the loads again (redo)", t->eng_req != before.eng_req && t->step[0].note[0] == 99);
    hold(B_SAVE);
    t->p[P_LEVEL] = 50;                           /* a mix change after the undo */
    turn(EN_PRESET, 1);                           /* a load after an undo copies the track as it is now */
    hold(B_SAVE);
    bad += check("a load after the undo: its own copy; the mix (LEVEL) is never swapped", same_sound(t, &before) && t->p[P_LEVEL] == 50);
    press(B_SAVE);
    bad += check("SAVE tap opens the SAVE pages (on release)", !ui.home && cur_page()->fam == FAM_SAVE);
    /* TOOLS INIT, user preset load: the sound only, undoable; one copy for all tracks */
    ui_power_on();
    my_steps(&trk[1]);
    trk[1].p[P_SLCR] = SL_STUT;
    before = trk[1];
    track_select(1);
    set_engine(trk[1].eng_req);                   /* TOOLS INIT */
    bad += check("TOOLS INIT: the sound only (steps and SLICER kept), a copy of T2", undo.trk == 2u &&
                 !memcmp(trk[1].step, before.step, sizeof before.step) && trk[1].p[P_SLCR] == SL_STUT);
    track_select(0);
    my_steps(&trk[0]);
    trk[0].p[P_SDIV] = 2;
    up_store(3, "MINE");
    bad += check("a user preset keeps its pattern in the record (format unchanged)", up_has_pat(3) && up_rec(3)->note[0] == 40);
    trk[0].p[P_SDIV] = 0;
    track_defaults_steps(&trk[0]);
    trk[0].step[5].note[0] = 33;
    trk[0].step[5].n = 1;
    before = trk[0];
    up_load(3);
    bad += check("a user preset load: the sound only (steps, DIV kept); the copy is now T1's", trk[0].user == 4u && undo.trk == 1u &&
                 !memcmp(trk[0].step, before.step, sizeof before.step) && trk[0].p[P_SDIV] == 0);
    undo_swap();
    bad += check("undo of a user preset load", trk[0].user == 0 && !memcmp(trk[0].step, before.step, sizeof before.step));
    /* project load: no copy, and the old one is gone */
    project_save(1);
    up_load(3);
    project_load(1);
    bad += check("a project load drops the copy and takes none", undo.trk == 0 && undo_depth == 0);
    ui_power_on();
    bad += check("SAMPLE factory browsing has four melodic presets and no PERC",
                 ENGINES[4]->npresets == 4u && str_eq(ENGINES[4]->presets[0].name, "PIANO") &&
                 str_eq(ENGINES[4]->presets[3].name, "SAX"));
    {   /* TRANH (SET 1, preset 1) is gone: both are PIANO aliases, kept for old data, never offered */
        const param_desc_t *sd = &ENGINES[4]->edit[0];
        uint32_t all, pos, k, e, pos0, shown = 0;
        set_engine_of(TSEL, 4);
        pos0 = preset_all_pos(&all);
        TSEL->preset = 1;
        TSEL->p[P_E0] = 1;
        pos = preset_all_pos(&all);
        for (k = 0; k < all; k++)
            if (preset_all_at(k, &e) == 4u)
                shown++;
        e = preset_all_at(pos0 + 1u, &k);
        apply_preset_to(TSEL, 1);
        bad += check("SAMPLE: old SET 1 / preset 1 (TRANH) play PIANO; browsing and knobs skip them",
                     SMP_SETS[1].z0 == SMP_SETS[0].z0 && SMP_SETS[1].nz == SMP_SETS[0].nz && pos == pos0 &&
                     shown == 3u && e == 4u && k == 2u && TSEL->preset == 0u && TSEL->p[P_E0] == 0 &&
                     str_eq(sd->names[1], "PIANO") && enum_step(sd, 0, 1) == 2 && enum_step(sd, 2, 1) == 0 &&
                     enum_step(sd, 1, 2) == 2 && enum_orig(sd, 1) == 0 && enum_orig(sd, 5) == 5 &&
                     enum_orig(&ENGINES[8]->edit[0], 1) == 0);
        bad += check("SAMPLE / GRAIN SET 4 (PERC, retired): a PIANO alias, knobs skip it, USR1..3 stay 5..7",
                     str_eq(sd->names[SMP_SET_PERC], "PIANO") && enum_orig(sd, SMP_SET_PERC) == 0 &&
                     enum_step(sd, 3, 4) == 5 && enum_step(sd, 5, 4) == 3 && str_eq(sd->names[5], "USR1") &&
                     str_eq(sd->names[7], "USR3") && sd->max == 7 &&
                     enum_orig(&ENGINES[8]->edit[0], SMP_SET_PERC) == 0 && SMP_SETS[SMP_SET_PERC].z0 == SMP_SETS[0].z0);
    }
    host_legacy_sample_perc(t);                /* SAMPLE PERC (SET 4, retired after 1.0.2): every load gives DRUM */
    t->preset = 4;                             /* a project written before the factory removal */
    my_steps(t);
    t->p[P_LEVEL] = 71;
    t->p[P_REV] = 33;
    before = *t;
    project_save(0);
    apply_preset_to(t, 0);
    track_defaults_steps(t);
    project_load(0);
    bad += check("saved SAMPLE PERC project: a DRUM track (its kit), the rest of the sound and every step kept",
                 t->eng_req == ENGI_DRUM && t->preset == 0u && perc_kit(t) &&
                 !memcmp(t->p, before.p, P_E0 * sizeof t->p[0]) && !memcmp(t->step, before.step, sizeof t->step));
    bad += check("  its GM hits play as drums (kick, snare, closed hat on their lanes)", gm_hits_play(t));
    {   /* a user preset of SAMPLE PERC (UP_PUT from an older editor, or a bank read): DRUM, its kit */
        up_rec_t r;
        int16_t keep[P_COUNT];
        uint32_t k;
        memset(&r, 0, sizeof r);
        r.used = UP_USED; r.ver = UP_VER; r.engine = ENGI_SAMPLE; r.np = P_COUNT;
        memcpy(r.name, "OLD PERC", 8);
        host_legacy_sample_perc(t);
        for (k = 0; k < P_COUNT; k++)
            up_set_value(&r, k, t->p[k]);
        up_migrate(&r);
        *up_rec(7) = r;
        set_engine_of(t, 0);
        memcpy(keep, t->p, sizeof keep);
        song.sel = (uint8_t)(t - trk);
        up_load(7);
        bad += check("SAMPLE PERC user preset: loads as a DRUM track (its kit)", t->eng_req == ENGI_DRUM && perc_kit(t) &&
                     t->p[P_LEVEL] == keep[P_LEVEL]);
        bad += check("  its GM hits play as drums", gm_hits_play(t));
        memset(up_rec(7), 0, sizeof r);
    }
    set_engine_of(t, 0);
    apply_preset_to(t, SMP_SET_PERC);             /* (engine 0: an ANALOG preset 4, not PERC) */
    bad += check("factory preset 4 of another engine stays that engine's", t->eng_req == 0u && t->preset == 4u);
    set_engine_of(t, ENGI_SAMPLE);
    apply_preset_to(t, SMP_SET_PERC);             /* SAMPLE preset 4 (once PERC): the editor's PRESET, a favourite */
    bad += check("SAMPLE preset 4 (once PERC): DRUM's kit", t->eng_req == ENGI_DRUM && t->preset == 0u && perc_kit(t) &&
                 gm_hits_play(t));
    ui_power_on();
    host_legacy_sample_perc(&trk[3]);
    my_steps(&trk[3]);
    trk[3].p[P_SDIV] = 3;
    before = trk[3];
    project_save(1);
    project_v6_t legacy = {0};
    project_t decoded;
    proj_import(&decoded, &proj_slot[1], sizeof proj_slot[1]);
    legacy.magic = PROJ_MAGIC_V6; legacy.size = sizeof legacy;
    legacy.parts = 0; legacy.phys = PROJ_PHYS;
    memcpy(legacy.g, decoded.g, sizeof legacy.g);
    for (uint32_t ti = 0; ti < NTRK; ti++) {
        memcpy(legacy.t[ti].p, decoded.t[ti].p, 61u * sizeof(int16_t));
        memcpy(legacy.t[ti].p + 61, decoded.t[ti].p + P_E0, 8u * sizeof(int16_t));
        legacy.t[ti].engine = decoded.t[ti].engine; legacy.t[ti].preset = decoded.t[ti].preset;
        for (uint32_t st = 0; st < NSTEP; st++) memcpy(&legacy.t[ti].step[st], &decoded.t[ti].step[st], sizeof(step10_t));
    }
    legacy.t[3].engine = 0;
    legacy.g[G_TAPE] = 71; legacy.g[G_CRSH] = 43;          /* (ids 25, 26: the drum part's level / send then) */
    legacy.g[G_RTYPE] = 10;                               /* (id 24 was the GM drum part's MIDI channel: 10) */
    legacy.sum = proj_hash(&legacy, offsetof(project_v6_t, sum));
    memcpy(&proj_slot[1], &legacy, sizeof legacy);
    project_load(1);
    bad += check("old GM projects (the drum part) load as DRUM's kit, their mix and steps kept",
                 trk[3].eng_req == ENGI_DRUM && trk[3].preset == 0u && perc_kit(&trk[3]) &&
                 trk[3].p[P_LEVEL] == 71 && trk[3].p[P_REV] == 43 && trk[3].p[P_SDIV] == 3 &&
                 !memcmp(trk[3].step, before.step, sizeof trk[3].step));
    bad += check("  their GM hits play as drums", gm_hits_play(&trk[3]));
    bad += check("  its old drum channel (10, in id 24) loads as REVERB TYPE ROOM", song.g[G_RTYPE] == 0);
    bad += check("  its old drum level / send (ids 25, 26) load as TAPE / CRSH off", song.g[G_TAPE] == 0 && song.g[G_CRSH] == 0);
    {   /* GHOULBOX: a list's selected row: the sword and accent text, no THEME bar */
        uint32_t theme = 0, accent = 0, blade = 0, i;
        cv_begin(240, 20, T_SURF);
        list_row(2, 1, "ANLG", T_MID, "CRYPT PAD", T_TEXT, 230);
        for (i = 0; i < 240u * 20u; i++) {
            uint16_t c = swap16(cv_px[i]);
            theme += c == T_THEME;
            accent += c == T_ACCENT;
        }
        for (i = 6; i < 20u; i++)
            blade += swap16(cv_px[(2u + 8u) * 240u + i]) == T_TEXT;   /* the blade's row */
        bad += check("a selected list row: the sword, accent text, no THEME bar", theme < 40u && accent > 40u && blade >= 6u);
    }
    {   /* GHOULBOX: torches on HOME's stage; their flame moves; a list page has none */
        static uint16_t f0[12 * 30], f2[12 * 30];
        uint32_t i, diff = 0, ink = 0;
        cv_begin(12, 30, T_SURF);
        torch_draw(0, 0, 0);
        memcpy(f0, cv_px, sizeof f0);
        cv_begin(12, 30, T_SURF);
        torch_draw(0, 0, 2);
        memcpy(f2, cv_px, sizeof f2);
        for (i = 0; i < 12u * 30u; i++) {
            diff += f0[i] != f2[i];
            ink += swap16(f0[i]) == T_ACCENT;
        }
        ui_power_on();
        ui.force = 1; ui_draw();
        bad += check("torches: the flame flickers (frames differ), it burns (accent), HOME shows them",
                     diff > 4u && ink > 10u && ui.torches == 1u && PANEL_X0 >= 24);
        go_title("PRESETS");
        ui.force = 1; ui_draw();
        bad += check("  a list page: no torches, the full width", ui.torches == 0u && PANEL_X0 == 10);
    }
    {   /* GHOULBOX: on every page with torches, each torch tile holds the torch on stone and nothing else (the
         * flicker repaints only the tile: a curve through it would be cut) */
        static const char *const PG[4] = {"", "ENV", "LFO", "FX"};
        uint32_t k, x, y, n = 0, pages = 0;
        track_t keep = *TSEL;
        for (k = 0; k < 4u; k++) {
            ui_power_on();
            TSEL->p[P_ATK] = 0;                          /* (the steepest attack, the shortest release: the */
            TSEL->p[P_REL] = 0;                          /* ENV curve's ends nearest the torches) */
            TSEL->p[P_SUS] = 127;
            if (k) go_title(PG[k]);
            ui.force = 1; ui_draw();
            if (!ui.torches) continue;
            pages++;
            for (uint32_t side = 0; side < 2u; side++) {
                int32_t tx = side ? TORCH_RX : TORCH_LX;
                cv_begin(12, 30, T_SURF);
                cv_stone(0, 0, 12, 30, tx, TORCH_Y);
                torch_draw(0, 0, side ? ui.torch_f ^ 2u : ui.torch_f);
                for (y = 0; y < 30u; y++)
                    for (x = 0; x < 12u; x++)
                        n += host_screen[(Y_GRAPH + TORCH_Y + y) * 240u + (uint32_t)tx + x] != cv_px[y * 12u + x];
            }
        }
        *TSEL = keep;
        bad += check("torch tiles: only the torch on stone on HOME, ENV, LFO, FX (no graph drawn into them)", pages == 4u && n == 0u);
    }
    {   /* GHOULBOX: inside a stone window nothing is drawn in INK (it was text on the old THEME bar): the PRESETS
         * row's suggested pattern, the EDIT layer's favourite star */
        uint32_t i, x, y, ink_list = 0, ink_star = 0, eng, pre;
        ui_power_on();
        go_title("PRESETS");
        ui.force = 1; ui_draw();
        for (y = Y_GRAPH + 3u; y < (uint32_t)(Y_GRAPH + H_GRAPH - 3); y++)
            for (x = 6; x < 234u; x++)
                ink_list += swap16(host_screen[y * 240u + x]) == T_INK;
        eng = TSEL->eng_req % NENGINES;
        pre = TSEL->preset;
        favorite_set(eng, pre, 1);
        cv_begin(240, 20, T_SURF);
        engine_sound_row(2);
        for (i = 0; i < 240u * 20u; i++)
            ink_star += swap16(cv_px[i]) == T_INK;
        favorite_set(eng, pre, 0);
        bad += check("no INK on stone: the PRESETS hint, the EDIT layer's star", ink_list == 0u && ink_star == 0u);
    }
    printf("ui: undo copy %u bytes\n", (unsigned)sizeof undo);
    return bad;
}

/* SEQ > PATTERNS: K1 PAT, OCT+ LOAD; the dialog over the user's steps; undo */
static int test_patterns(void)
{
    int bad = 0;
    track_t *t = &trk[0], before;
    ui_power_on();
    go_page(GR_PATS);
    bad += check("SEQ > PHRASES is a SEQ page", cur_page()->fam == FAM_SEQ && str_eq(cur_page()->title, "PHRASES") &&
                 song.seq_mode);
    before = *t;
    turn(EN_K2, 1);
    bad += check("KNOB 2 loads nothing (the knobs only pick)", seq_is_empty(t) && ui.confirm == CF_NONE);
    press(B_OCTUP);                               /* LOAD 01 ACID into the empty sequencer */
    bad += check("LOAD into an empty sequencer: at once, LOADED 01 ACID", ui.confirm == CF_NONE && steps_are(t, 0) &&
                 msg_is("LOADED 01 ACID"));
    bad += check("..the sound and the track's settings untouched", same_sound(t, &before) && t->p[P_SDIV] == before.p[P_SDIV] &&
                 t->p[P_AMODE] == before.p[P_AMODE]);
    turn(EN_K1, 2);
    press(B_OCTUP);
    bad += check("a loaded pattern, untouched: the next one without asking (03 MELODY)", ui.confirm == CF_NONE && steps_are(t, 2) &&
                 msg_is("LOADED 03 MELODY"));
    hold(B_SAVE);
    bad += check("SAVE held: back past both loads (empty again), the sound untouched", seq_is_empty(t) && same_sound(t, &before) &&
                 t->p[P_SLEN] == before.p[P_SLEN]);
    hold(B_SAVE);                                 /* redo */
    t->step[3].note[0] = 70;                      /* an edit: the user's steps now */
    t->step[3].n = 1;
    t->step[3].time = ST_NOTE;
    press(B_OCTUP);
    bad += check("edited steps: the REPLACE T1 SEQUENCE? dialog", ui.confirm == CF_LOAD_PAT && ui.confirm_trk == 0u &&
                 t->step[3].note[0] == 70);
    press(B_OCTDN);
    bad += check("OCT- keeps them", ui.confirm == CF_NONE && t->step[3].note[0] == 70);
    my_steps(t);
    before = *t;
    turn(EN_K1, 9);                               /* 12 BEAT */
    press(B_OCTUP);
    press(B_OCTUP);
    bad += check("OCT+ loads (12 BEAT), then HOLD SAVE: UNDO", ui.confirm == CF_NONE && steps_are(t, 11) &&
                 msg_is("LOADED 12 BEAT") && str_eq(ui.msg2, "[SAVE] HOLD TO UNDO"));
    hold(B_SAVE);
    bad += check("SAVE held: the user's steps and LEN back", !memcmp(t->step, before.step, sizeof t->step) && t->p[P_SLEN] == 32);
    /* a user preset's pattern: listed after the factory ones as U06, with its LEN DIV SWING GATE */
    t->p[P_SDIV] = 2;
    t->p[P_SSWING] = 40;
    up_store(5, "UPAT");
    t->p[P_SDIV] = 0;
    t->p[P_SSWING] = 0;
    up_store(6, "NOPAT");
    memset(up_rec(6)->note, 0, sizeof up_rec(6)->note);   /* .. a record with no pattern: not listed */
    bad += check("user presets with a pattern are listed after the factory ones", pat_count() == NPATTERNS + 1u);
    track_defaults_steps(t);
    turn(EN_K1, 20);                              /* (to the end of the list) */
    press(B_OCTUP);
    bad += check("LOAD U06: its 16 steps, LEN 16 (at most), DIV and SWING", msg_is("LOADED U06 UPAT") && t->step[0].note[0] == 40 &&
                 t->step[3].note[0] == 43 && !t->step[30].n && t->p[P_SLEN] == 16 && t->p[P_SDIV] == 2 && t->p[P_SSWING] == 40);
    /* a project's steps count as the user's */
    project_save(2);
    project_load(2);
    press(B_OCTUP);
    bad += check("over a project's steps: the dialog", ui.confirm == CF_LOAD_PAT);
    press(B_OCTDN);
    /* the hint: PRESETS page K3 PAT, and a load moves the pick there */
    go_page(GR_BROWSE);
    turn(EN_K1, 1);
    bad += check("PRESETS: a factory preset's suggested pattern becomes the pick", preset_pat_hint() >= 0 &&
                 ui.ppick == (uint8_t)preset_pat_hint());
    return bad;
}

static int test_rec(void)
{
    static const uint8_t FAMS[3] = {FAM_TRK, FAM_SEQ, FAM_ARP};
    int bad = 0;
    uint32_t k;
    for (k = 0; k < 3u; k++) {
        ui_power_on();
        track_select(2);
        open_family(FAMS[k]);
        press(B_REC);
        bad += check(k == 0 ? "REC on TRACKS: arms the selected track, PLAY starts" :
                     k == 1 ? "REC on SEQ: arms the selected track, PLAY starts" : "REC on ARP: arms the selected track, PLAY starts",
                     song.rec == 4u && transport_req == 1u);
        song.playing = 1;
        transport_req = 0;
        press(B_REC);
        bad += check("REC again disarms (the transport runs on)", song.rec == 0u && transport_req == 0u);
        hold(B_REC);
        bad += check("REC held remains record transport and never clears a pattern", ui.confirm == CF_NONE && song.rec == 4u);
    }
    ui_power_on();
    press(B_REC);
    bad += check("REC on HOME records without changing the screen", ui.home && song.rec == 1u && transport_req == 1u);
    /* STEP page: armed + playing -> the keys record live only */
    ui_power_on();
    open_family(FAM_SEQ);
    bad += check("SEQ opens STEP", cur_page()->scope == SC_STEP);
    press(B_REC);
    song.playing = 1;
    transport_req = 0;
    fm1_in.notes = host_notes = 1u << 7;
    frame();
    fm1_in.notes = 0;
    frame();
    bad += check("STEP, armed and playing: a key does not write the cursor step", !trk[0].step[0].n && ui.cursor == 0);
    song.rec = 0;
    fm1_in.notes = host_notes = 1u << 7;
    frame();
    fm1_in.notes = 0;
    frame();
    bad += check("STEP, not armed: the key writes the cursor step (and moves on)", trk[0].step[0].n == 1 && ui.cursor == 1);
    /* ARP: the arp's notes are recorded, not the key */
    ui_power_on();
    {
        track_t *t = &trk[0];
        uint32_t i, n = 0, notes = 0;
        t->engine = t->eng_req;
        t->p[P_AMODE] = 1;
        t->p[P_AOCT] = 2;
        t->p[P_APROB] = 127;
        song.rec = 1;
        song.playing = 1;
        input_on(t, 60, 100);
        for (i = 0; i < NSTEP; i++)
            n += t->step[i].n;
        bad += check("ARP on: the key held is not recorded", n == 0);
        arp_tick(t, 1);
        for (i = 0; i < NSTEP; i++)
            if (t->step[i].n)
                notes = t->step[i].note[0];
        bad += check("ARP on: the note the arp plays is recorded", notes == 60u);
        t->p[P_AMODE] = 0;
        input_off(t, 60);
    }
    return bad;
}

static uint32_t gated_notes(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++) n += t->v[i].gate != 0;
    return n;
}

static int test_midi(void)
{
    int bad = 0;
    track_t *a, *b;
    ui_power_on();
    song.sel = 2;
    midi_hint = 0;
    midi_event(0x90, 0, 60, 100); midi_event(0x90, 9, 61, 100);
    bad += check("ROUT CH1-4 (default): channel 1 -> part 1, channel 10 ignored",
                 song.g[G_ROUTE] == 0 && midi_sel_on[0][60] == 1u && !midi_sel_on[9][61] && !gated_notes(&trk[2]));
    bad += check("a note into another track tells the UI", midi_hint == 1u);
    frame(); bad += check("MIDI IN -> T1 shown", msg_is("MIDI IN -> T1"));
    song.g[G_ROUTE] = 1;
    midi_event(0x80, 0, 60, 0); midi_event(0x90, 0, 62, 100);
    bad += check("ROUT changes: note-off releases original owner, new note uses selection",
                 !gated_notes(&trk[0]) && midi_sel_on[0][62] == 3u && midi_track(3) == &trk[2]);
    song.sel = 1; midi_event(0x80, 0, 62, 0);
    bad += check("note-off after selection follows the note-on's track", !midi_sel_on[0][62]);
    midi_event(0x80, 9, 61, 0);
    ui_power_on();
    song.g[G_ROUTE] = 1;
    midi_event(0x90, 0, 60, 100);
    song.sel = 1;
    midi_event(0x90, 0, 60, 100);
    bad += check("same MIDI channel/pitch on a new selected track releases its old owner",
                 !gated_notes(&trk[0]) && gated_notes(&trk[1]) && midi_sel_on[0][60] == 2u);
    midi_event(0x80, 0, 60, 0);
    midi_event(0x80, 0, 60, 0);
    bad += check("both note-offs after a MIDI ownership change leave neither track held",
                 !gated_notes(&trk[0]) && !gated_notes(&trk[1]) && !midi_sel_on[0][60]);
    ui_power_on();
    song.sel = 2; song.g[G_ROUTE] = 1;
    midi_event(0x90, 0, 61, 100);
    song.g[G_ROUTE] = 0;
    midi_event(0x90, 0, 61, 100);
    midi_event(0x80, 0, 61, 0);
    bad += check("same channel/pitch across a ROUT change cannot leave its previous part held",
                 !gated_notes(&trk[2]) && !gated_notes(&trk[0]) && !midi_sel_on[0][61]);
    {
        uint32_t hold;
        for (hold = 0; hold < 2u; hold++) {
            ui_power_on();
            song.g[G_ROUTE] = 1;
            trk[0].p[P_AMODE] = 1; trk[0].p[P_AHOLD] = (int16_t)hold;
            midi_event(0x90, 0, 60, 100);
            song.sel = 1;
            midi_event(0x90, 0, 60, 100);
            bad += check("MIDI ownership transfer releases old ARP keys and respects explicit HOLD",
                         !trk[0].arp_phys && trk[0].nheld == hold && midi_sel_on[0][60] == 2u);
        }
    }
    ui_power_on();
    trk[1].p[P_AMODE] = trk[1].p[P_AHOLD] = 1;
    midi_event(0x90, 0, 60, 100);
    midi_event(0x90, 1, 64, 100);
    song.playing = 1;
    {
        uint32_t i, held = 0;
        for (i = 0; i < MQ; i++) midi_in_event(0x643C9909u);
        midi_in_event(0x003C8908u);                      /* note-off lost to a full ring */
        events_block(CTL);
        for (i = 0; i < NTRK * NVOICE; i++) held += trk[i / NVOICE].v[i % NVOICE].gate;
        bad += check("MIDI overflow releases held notes and latched ARP, keeps transport running",
                     !held && !trk[1].nheld && !trk[1].arp_phys && !midi_sel_on[0][60] &&
                     !midi_sel_on[1][64] && !midi_in_overflow && mi_r == mi_w && song.playing);
        midi_in_event(0x643D9009u);
        events_block(CTL);
        bad += check("MIDI accepts a fresh note after overflow recovery", midi_sel_on[0][61] == 1u && trk[0].v[0].gate);
    }
    {
        const page_t *pg = &PAGES[page_first(FAM_GLO) + 1u];
        int16_t *vp;
        const param_desc_t *d = page_desc(pg, 2, &vp);
        char v[8];
        const char *u;
        param_format(d, 1, v, &u);
        bad += check("GLO > SYSTEM K3: ROUT CH1-4 / SEL", str_eq(pg->title, "SYSTEM") && vp == &song.g[G_ROUTE] && str_eq(v, "SEL"));
    }
    return bad;
}

static int test_save(void)
{
    int bad = 0;
    uint32_t i;
    ui_power_on();
    trk[0].p[P_E0] = 5;
    press(B_SAVE);
    bad += check("SAVE opens USER with SAVE selected, without writing a slot",
                 !ui.home && cur_page()->graph == GR_USER && ui.act == 4u && !up_used(0));
    press(B_OCTUP);
    bad += check("SAVE then OCT+ opens NAME (the automatic name), writes nothing yet",
                 name_on() && nm.kind == NK_USER_SAVE && str_eq(nm.s, nm.ph) && nm.cur == nm.len && !up_used(0));
    press(B_OCTUP);
    {
        char nmb[16], au[16];
        up_name(0, nmb);
        up_auto_name(au, trk[0].eng_req, 0);
        bad += check("..OCT+ again stores the edited sound with that name (SAVE, OCT+, OCT+)",
                     !name_on() && up_used(0) && up_value(up_rec(0), P_E0) == 5 && ui.act == 0u && str_eq(nmb, au));
    }
    press(B_SAVE);
    bad += check("SAVE again still cycles to PROJECT", cur_page()->graph == GR_SLOTS);
    go_home();
    press(B_SAVE);
    bad += check("SAVE from HOME returns directly to USER, even after visiting PROJECT",
                 cur_page()->graph == GR_USER && ui.act == 4u);
    press(B_OCTUP);
    bad += check("direct SAVE still asks before overwriting an occupied slot",
                 ui.confirm == CF_OVR_USER && up_value(up_rec(0), P_E0) == 5);
    press(B_OCTDN);
    ui_power_on();
    song.playing = 1;
    project_save(0);
    bad += check("project save while playing: refused, STOP TO SAVE", !project_used(0) && msg_is("STOP TO SAVE"));
    bad += check("user preset save while playing: refused", up_store(0, "X") == 2 && !up_used(0));
    song.playing = 0;
    transport_req = 1;
    project_save(0);
    bad += check("a pending PLAY cannot save a project", !project_used(0) && msg_is("STOP TO SAVE"));
    bad += check("a pending PLAY cannot save a user preset", up_store(0, "X") == 2 && !up_used(0));
    transport_req = 0;
    chain.armed = 1;
    bad += check("a pending SONG cannot save a user preset", up_store(0, "X") == 2 && !up_used(0));
    chain.armed = 0;
    song.playing = 1;
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].graph == GR_SLOTS)
            break;
    ui.home = 0;
    ui.page = (uint8_t)i;
    page_entered();
    ui.msg_t = 0;
    turn(EN_K4, 1);                               /* SAVE picked */
    bad += check("PROJECT KNOB 4 picks SAVE, saves nothing", ui.act == 4u && !project_used(0) && !ui.msg_t);
    press(B_OCTUP);
    bad += check("OCT+ while playing: STOP TO SAVE, no NAME", msg_is("STOP TO SAVE") && !project_used(0) &&
                 song.g[G_SAVE] == 0 && !name_on());
    stop_transport();
    turn(EN_K4, 1);
    press(B_OCTUP);
    bad += check("stopped, an empty slot: OCT+ opens NAME (no name yet: PROJECT A)", name_on() && nm.kind == NK_PROJ_SAVE &&
                 nm.len == 0u && str_eq(nm.ph, "PROJECT A") && !project_used(0));
    press(B_OCTUP);
    bad += check("..OCT+ saves it unnamed, SAVE dropped", !name_on() && project_used(0) && msg_is("SAVED (RAM)") &&
                 ui.act == 0u);
    turn(EN_K4, 1);
    press(B_OCTUP);
    bad += check("a used slot: the OVERWRITE? dialog", ui.confirm == CF_OVR_PROJ && ui.confirm_trk == 0);
    trk[0].p[P_E0] = 5;
    press(B_OCTDN);
    bad += check("OCT- keeps the slot", ui.confirm == CF_NONE && stored_param(0, 0, P_E0) != 5);
    turn(EN_K4, 1);
    press(B_OCTUP);
    press(B_OCTUP);
    bad += check("OCT+ (YES): NAME, the slot not written yet", name_on() && stored_param(0, 0, P_E0) != 5);
    press(B_OCTUP);
    bad += check("OCT+ overwrites it", ui.confirm == CF_NONE && !name_on() && stored_param(0, 0, P_E0) == 5);
    trk[0].p[P_LEVEL] = 11;
    turn(EN_K3, 1);
    press(B_OCTUP);
    bad += check("KNOB 3 LOAD, OCT+: the project back; LOAD stays picked", trk[0].p[P_LEVEL] != 11 &&
                 trk[0].p[P_LEVEL] == stored_param(0, 0, P_LEVEL) && msg_is("LOADED") && ui.act == 3u);
    /* user presets */
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].graph == GR_USER)
            break;
    ui.page = (uint8_t)i;
    page_entered();
    up_store(4, "OLD");
    ui.uslot = 4;
    turn(EN_K4, 1);
    press(B_OCTUP);
    bad += check("USER SAVE over a used slot: the OVERWRITE? dialog", ui.confirm == CF_OVR_USER && ui.confirm_trk == 4u);
    press(B_OCTUP);
    bad += check("YES: NAME with the sound's name (not the old slot's)", name_on() && !str_eq(nm.s, "OLD"));
    press(B_OCTUP);
    {
        char nm[16];
        up_name(4, nm);
        bad += check("OCT+ stores the sound there", ui.confirm == CF_NONE && !str_eq(nm, "OLD"));
    }
    song.playing = 1;
    ui.msg_t = 0;
    turn(EN_K3, 1);                               /* ERASE */
    press(B_OCTUP);
    bad += check("USER ERASE while playing: STOP TO SAVE", msg_is("STOP TO SAVE") && up_used(4));
    stop_transport();
    return bad;
}

static void go_title(const char *title)
{
    uint32_t i;
    for (i = 0; i < NPAGES && !str_eq(PAGES[i].title, title); i++)
        ;
    ui.home = 0;
    ui.page = (uint8_t)i;
    page_entered();
}
static uint32_t oct_leds_seen(int all)            /* over 1 s: the OCT LED bits lit at some point (all: throughout) */
{
    uint32_t k, any = 0, every = 3u;
    for (k = 0; k < 64u; k++) {
        frame();
        any |= oct_leds();
        every &= oct_leds();
    }
    return all ? every : any;
}

/* the action pages: the knobs pick, OCT+ does it, OCT- cancels / goes HOME; the LEDs; chords */
static int test_actions(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    ui_power_on();
    press(B_OCTUP);
    bad += check("HOME: OCT+ shifts the octave (lit)", song.octave == 1 && oct_leds() == 2u);
    go_page(GR_PATS);
    bad += check("PATTERNS: OCT+ blinks (a pattern to load), OCT- lit", oct_leds_seen(0) == 3u && oct_leds_seen(1) == 1u);
    press(B_OCTUP);
    bad += check("..OCT+ loads it, the octave stays", steps_are(t, 0) && song.octave == 1 && !ui.home);
    bad += check("..the same pattern again would change nothing: OCT+ dark", oct_leds_seen(0) == 1u);
    turn(EN_K1, 1);
    bad += check("..another one picked: OCT+ blinks again", oct_leds_seen(0) == 3u);
    {
        char a[16], b[16];
        foot_hint(a, b);
        bad += check("..the footer: OCT+ LOAD / OCT- BACK", str_eq(a, "OCT+ LOAD") && str_eq(b, "OCT- BACK"));
    }
    fm1_in.buttons |= 1u << panel.btn[B_OCTUP];   /* both together (the UPDATE MODE hold): nothing */
    host_pressed |= 1u << panel.btn[B_OCTUP];
    frame();
    fm1_in.buttons |= 1u << panel.btn[B_OCTDN];
    host_pressed |= 1u << panel.btn[B_OCTDN];
    frame();
    fm1_in.buttons &= ~(1u << panel.btn[B_OCTUP]);
    frame();
    fm1_in.buttons &= ~(1u << panel.btn[B_OCTDN]);
    frame();
    bad += check("OCT- + OCT+ together: no load, no HOME, no octave change", steps_are(t, 0) && !ui.home && song.octave == 1);
    go_home();
    fm1_in.buttons |= 1u << panel.btn[B_OCTUP];   /* pressed on HOME, let go on PATTERNS: no load */
    host_pressed |= 1u << panel.btn[B_OCTUP];
    frame();
    go_page(GR_PATS);
    fm1_in.buttons &= ~(1u << panel.btn[B_OCTUP]);
    frame();
    bad += check("a press that began elsewhere does not load", steps_are(t, 0) && song.octave == 2);
    press(B_OCTDN);
    bad += check("PATTERNS: OCT- goes HOME (the octave stays)", ui.home && song.octave == 2);
    /* TOOLS: CLRSQ and INIT are picked, not done, by their knobs */
    my_steps(t);
    go_title("TOOLS");
    bad += check("TOOLS: nothing picked, OCT+ dark", act_cols() == 15u && act_col() == 0u && oct_leds_seen(0) == 1u);
    turn(EN_K1, 3);
    bad += check("KNOB 1 picks CLEAR, the steps stay; OCT+ blinks", ui.act == 1u && !seq_is_empty(t) && oct_leds_seen(0) == 3u);
    {
        char a[16], b[16];
        foot_hint(a, b);
        bad += check("..the footer: OCT+ CLEAR / OCT- CANCEL", str_eq(a, "OCT+ CLEAR") && str_eq(b, "OCT- CANCEL"));
    }
    turn(EN_K1, -1);
    bad += check("KNOB 1 left drops it", ui.act == 0u);
    turn(EN_K1, 1);
    press(B_OCTDN);
    bad += check("OCT- cancels the pick (still on TOOLS)", ui.act == 0u && !ui.home && !seq_is_empty(t));
    turn(EN_K1, 1);
    press(B_OCTUP);
    bad += check("TOOLS CLEAR first asks confirmation", ui.confirm == CF_CLEAR_SEQ && !seq_is_empty(t));
    press(B_OCTUP);
    bad += check("OCT+ clears the pattern, the pick dropped", seq_is_empty(t) && msg_is("PATTERN CLEARED") && ui.act == 0u);
    turn(EN_K1, 1);
    bad += check("nothing to clear: OCT+ dark", oct_leds_seen(0) == 1u);
    turn(EN_K2, 1);
    t->p[P_E0] = (int16_t)(t->p[P_E0] + 1);
    press(B_OCTUP);
    bad += check("TOOLS INIT first asks confirmation", ui.confirm == CF_INIT_SOUND);
    press(B_OCTUP);
    bad += check("KNOB 2 INIT, OCT+: SOUND INIT", msg_is("SOUND INIT") && ui.act == 0u);
    press(B_OCTDN);
    bad += check("OCT- with nothing picked: HOME", ui.home);
    /* the dialogs and the menu */
    open_family(FAM_SEQ);
    my_steps(t);
    confirm_open(CF_CLEAR_SEQ, song.sel);
    frame();
    bad += check("a dialog: OCT+ blinks, OCT- lit", ui.confirm == CF_CLEAR_SEQ && oct_leds_seen(0) == 3u && oct_leds_seen(1) == 1u);
    fm1_in.buttons |= 1u << panel.btn[B_OCTUP];
    host_pressed |= 1u << panel.btn[B_OCTUP];
    frame();
    bad += check("..it answers on release, not on the press", ui.confirm == CF_CLEAR_SEQ && !seq_is_empty(t));
    fm1_in.buttons &= ~(1u << panel.btn[B_OCTUP]);
    frame();
    bad += check("..OCT+ let go: cleared", ui.confirm == CF_NONE && seq_is_empty(t));
    bad += check("back on SEQ: the octave LEDs again", oct_leds() == 2u);
    return bad;
}

static int test_tracks(void)
{
    int bad = 0;
    track_t before;
    ui_power_on();
    open_family(FAM_TRK);
    before = trk[0];
    turn(EN_PRESET, 1);
    bad += check("TRACKS: the PRESETS knob does nothing", trk[0].preset == before.preset && !memcmp(trk[0].p, before.p, sizeof before.p));
    turn(EN_K4, 1);
    bad += check("MIXER KNOB 4 right: MUTE ON (the selected track stays)", trk[0].p[P_MUTE] == 1 && song.sel == 0);
    turn(EN_K1, -3);
    bad += check("KNOB 1 changes LEVEL only (MUTE stays)", trk[0].p[P_MUTE] == 1 && trk[0].p[P_LEVEL] == before.p[P_LEVEL] - 3);
    turn(EN_K2, 2);
    bad += check("KNOB 2 changes PAN", trk[0].p[P_PAN] == before.p[P_PAN] + 2);
    turn(EN_K4, -1);
    bad += check("MIXER KNOB 4 left: MUTE OFF", trk[0].p[P_MUTE] == 0);
    turn(EN_ALGO, 1);
    bad += check("ALGORITHM selects the track", song.sel == 1);
    return bad;
}

/* key k of colour black (1) / white (0) at place p (seq.c key_place) */
static uint32_t key_at(int black, uint32_t p)
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (key_black(k) == black && key_place(k) == p)
            return k;
    return 0;
}
static void tap_key(uint32_t k)                   /* a key down for a frame, then up */
{
    fm1_in.notes |= 1u << k;
    host_notes |= 1u << k;
    frame();
    fm1_in.notes &= ~(1u << k);
    frame();
}
static uint32_t lane_steps(const track_t *t, uint32_t l)   /* bit i: lane l strikes at step i (0..31) */
{
    uint32_t i, m = 0;
    for (i = 0; i < 32u; i++)
        m |= ((step_lanes(&t->step[i]) >> l) & 1u) << i;
    return m;
}

static int test_grid(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    uint32_t i, k, leds, ok;
    ui_power_on();
    set_engine_of(t, ENGI_DRUM);
    t->engine = t->eng_req;
    open_family(FAM_SEQ);
    frame();
    bad += check("SEQ > STEP on a DRUM track is the grid (keys to the UI), GRID in the footer", grid_on() && song.grid == 1u &&
                 cur_page()->scope == SC_STEP);
    bad += check("the lanes: KICK SNARE CLAP HATCL HATOP TOM RIM BELL on GM 36 38 39 42 46 45 37 56",
                 str_eq(drum_lane_name(t, 0), "KICK") && str_eq(drum_lane_name(t, 7), "BELL") && DRUM_LANE_NOTE[3] == 42 &&
                 DRUM_LANE_NOTE[6] == 37 && drum_lane(41) == DV_TOM && drum_lane(49) == DV_BELL && drum_lane(35) == DV_KICK);
    tap_key(key_at(1, 1));
    bad += check("black key 2: lane 2 (SNARE) selected", ui.lane == 1u);
    tap_key(key_at(0, 4));
    bad += check("white key 5: SNARE on step 5, the cursor there", t->step[4].hit == 1u << DV_SNARE && t->step[4].time == ST_NOTE &&
                 !t->step[4].n && ui.cursor == 4u);
    leds = grid_leds();
    bad += check("LEDs: white key 5 lit, black key 2 lit, ACC and the page keys dark (LEN 16)",
                 (leds >> key_at(0, 4) & 1u) && (leds >> key_at(1, 1) & 1u) && !(leds >> key_at(0, 3) & 1u) &&
                 !(leds >> key_at(1, 0) & 1u) && !(leds >> key_at(1, GK_ACC) & 1u) && !(leds >> key_at(1, GK_PGUP) & 1u));
    song.playing = 1;
    t->seq_idx = 4;
    leds = grid_leds();
    ok = !(leds >> key_at(0, 4) & 1u);
    t->seq_idx = 6;
    leds = grid_leds();
    ok &= (leds >> key_at(0, 6) & 1u) && !(leds >> key_at(0, 4) & 1u) == 0u;
    song.playing = 0;
    bad += check("LEDs: the step playing inverted (a hit goes dark, an empty step lights)", ok);
    tap_key(key_at(0, 4));
    bad += check("white key 5 again: off, an empty step (REST)", !t->step[4].hit && t->step[4].time == ST_REST);
    /* ACC held: the white keys set accents, their LEDs show them */
    fm1_in.notes |= 1u << key_at(1, GK_ACC);
    frame();
    tap_key(key_at(0, 0));
    fm1_in.notes |= 1u << key_at(1, GK_ACC);
    leds = grid_leds();
    ok = t->step[0].hit == 1u << DV_SNARE && t->step[0].acc == 1u << DV_SNARE && (leds >> key_at(0, 0) & 1u) &&
         (leds >> key_at(1, GK_ACC) & 1u);
    tap_key(key_at(0, 0));
    fm1_in.notes |= 1u << key_at(1, GK_ACC);
    leds = grid_leds();
    ok &= t->step[0].hit == 1u << DV_SNARE && !t->step[0].acc && !(leds >> key_at(0, 0) & 1u);
    fm1_in.notes = 0;
    frame();
    leds = grid_leds();
    ok &= (leds >> key_at(0, 0) & 1u) != 0u;
    bad += check("ACC held: a tap adds the hit accented, again drops the accent only; the LEDs show accents", ok);
    /* the knobs: STEP LANE HIT ACC */
    turn(EN_K1, 2);
    turn(EN_K2, 1);
    turn(EN_K3, 1);
    ok = ui.cursor == 2u && ui.lane == 2u && t->step[2].hit == 1u << DV_CLAP;
    turn(EN_K4, 1);
    ok &= t->step[2].acc == 1u << DV_CLAP;
    turn(EN_K3, -1);
    ok &= !t->step[2].hit && !t->step[2].acc && t->step[2].time == ST_REST;
    turn(EN_K4, 1);
    ok &= t->step[2].hit == 1u << DV_CLAP && t->step[2].acc == 1u << DV_CLAP;
    bad += check("KNOB 1 STEP, 2 LANE, 3 HIT on / off, 4 ACC (an accent adds the hit)", ok);
    press(B_EDIT);
    bad += check("EDIT: the cursor step cleared, on to the next", t->step[2].time == ST_REST && !t->step[2].hit && ui.cursor == 3u &&
                 grid_on());
    /* pages: LEN 32, the page keys */
    t->p[P_SLEN] = 32;
    leds = grid_leds();
    tap_key(key_at(1, GK_PGUP));
    ok = ui.bank == 1u && ui.cursor == 19u && (leds >> key_at(1, GK_PGUP) & 1u) && (leds >> key_at(1, GK_PGDN) & 1u);
    tap_key(key_at(0, 15));
    ok &= t->step[31].hit == 1u << DV_CLAP;
    tap_key(key_at(1, GK_PGDN));
    ok &= ui.bank == 0u && ui.cursor == 15u;
    t->p[P_SLEN] = 12;
    frame();
    tap_key(key_at(0, 13));
    ok &= !t->step[13].hit && !t->step[13].n;
    bad += check("pages: black keys 11 / 10 up / down (the cursor along); past LEN a key does nothing", ok);
    t->p[P_SLEN] = 16;
    /* a step's GM notes on their lanes */
    t->step[8] = (step_t){{41, 49, 36, 0}, 3, ST_NOTE, SF_ACCENT, 96, 0, 0};
    ok = step_lanes(&t->step[8]) == ((1u << DV_TOM) | (1u << DV_BELL) | (1u << DV_KICK)) &&
         step_accents(&t->step[8]) == step_lanes(&t->step[8]);
    grid_hit(t, 8, DV_TOM, 0);
    ok &= t->step[8].n == 1 && t->step[8].note[0] == 49 && t->step[8].hit == 1u << DV_KICK && (t->step[8].flags & SF_ACCENT);
    grid_acc(t, 8, DV_KICK, 0);
    ok &= !t->step[8].acc && !(t->step[8].flags & SF_ACCENT) && t->step[8].n == 1 && (step_lanes(&t->step[8]) >> DV_BELL & 1u);
    grid_acc(t, 8, DV_BELL, 1);
    ok &= !t->step[8].n && t->step[8].hit == ((1u << DV_KICK) | (1u << DV_BELL)) && t->step[8].acc == 1u << DV_BELL;
    bad += check("GM notes on the grid: a low tom / crash on their lanes; an edit makes the lane its hit", ok);
    /* the keys in the audio ISR: on the grid only the lane keys play */
    kb_prev = 0;
    fm1_in.notes = (1u << key_at(0, 3)) | (1u << key_at(1, 3)) | (1u << key_at(1, GK_ACC));
    keyboard_block();
    ok = kb_note[key_at(0, 3)] == KB_SILENT && kb_note[key_at(1, 3)] == 42u && kb_note[key_at(1, GK_ACC)] == KB_SILENT;
    fm1_in.notes = 0;
    keyboard_block();
    bad += check("grid keys in the ISR: white keys and ACC silent, black key 4 plays HAT CL (42)", ok);
    /* live recording on the grid: armed and playing, a lane key records its hit at the play head */
    track_defaults_steps(t);
    song.rec = 1;
    song.playing = 1;
    t->seq_idx = 5;
    t->seq_pos = 0;
    fm1_in.notes = 1u << key_at(1, 4);
    keyboard_block();
    fm1_in.notes = 0;
    keyboard_block();
    ok = t->step[5].hit == 1u << DV_HATO && !t->step[5].n && t->rskip_n == 0u;
    t->seq_pos = step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), 5) - 10u;   /* late in step 6: into step 7 */
    fm1_in.notes = 1u << key_at(1, 0);
    keyboard_block();
    ok &= t->step[6].hit == 1u << DV_KICK && t->rskip_n == 1u && t->rskip[0] == 36u;
    rec_hold(t, 7, 16);
    fm1_in.notes = 0;
    keyboard_block();
    ok &= t->step[7].time == ST_REST && !t->rh_n;
    bad += check("REC on the grid: lane keys record hits, quantised (the next step late), no TIE holds", ok);
    /* live recording elsewhere (HOME): the GM keys; a lane's note a hit, another GM drum a note */
    go_home();
    frame();
    t->seq_idx = 9;
    t->seq_pos = 0;
    fm1_in.notes = (1u << 7) | (1u << 13) | (1u << 6);   /* C 36 KICK, F# 42 HAT CL, E 35 (a kick 2 st down) */
    keyboard_block();
    fm1_in.notes = 0;
    keyboard_block();
    ok = !song.grid && t->step[9].hit == ((1u << DV_KICK) | (1u << DV_HATC)) && t->step[9].n == 1 && t->step[9].note[0] == 35 &&
         step_lanes(&t->step[9]) == ((1u << DV_KICK) | (1u << DV_HATC));
    song.rec = 0;
    song.playing = 0;
    bad += check("REC on HOME: GM keys quantised into the grid (35 a note on the KICK lane)", ok);
    /* SEQ > PATTERNS: 12 BEAT into the DRUM track fills the grid */
    track_defaults_steps(t);
    pat_sig[0] = steps_sig(t);
    go_page(GR_PATS);
    ui.ppick = 11;
    press(B_OCTUP);
    ok = msg_is("LOADED 12 BEAT") && t->p[P_SLEN] == 16 && lane_steps(t, DV_KICK) == ((1u << 0) | (1u << 6) | (1u << 8) | (1u << 11)) &&
         lane_steps(t, DV_SNARE) == ((1u << 4) | (1u << 12)) && lane_steps(t, DV_HATO) == 1u << 14 &&
         lane_steps(t, DV_HATC) == 0xA6AEu && t->step[0].acc == 1u << DV_KICK && t->step[4].acc == 1u << DV_SNARE &&
         t->step[1].acc == 0u && t->step[16].time == ST_REST;
    for (i = 0; i < 16u; i++)
        ok &= t->step[i].n == 0u && !(t->step[i].flags & SF_ACCENT);
    bad += check("PATTERNS 12 BEAT into a DRUM track: the grid (hits, the accents on 1 5 9 13)", ok);
    open_family(FAM_SEQ);
    ui.lane = DV_KICK;
    cursor_set(0);
    leds = grid_leds();
    ok = 1;
    for (i = 0; i < 16u; i++)
        ok &= (leds >> key_at(0, i) & 1u) == ((0x0941u >> i) & 1u);
    bad += check("..its KICK lane on the white key LEDs (1 7 9 12)", ok);
    {   /* the sound changes, the grid stays and plays as GM notes on another engine */
        step_t keep[NSTEP];
        memcpy(keep, t->step, sizeof keep);
        go_home();
        turn(EN_PRESET, 1);
        set_engine_of(t, 0);
        t->engine = t->eng_req;
        ok = !memcmp(keep, t->step, sizeof keep) && !grid_on();
        t->seq_hold = 0;
        t->seq_n = 0;
        seq_step(t, &t->step[0], div_samples((uint32_t)t->p[P_SDIV]), 0);
        ok &= t->seq_n == 1u && t->seq_notes[0] == 36u;
        seq_release(t);
        bad += check("a sound load keeps the grid; on ANALOG its hits play as their GM notes", ok);
        hold(B_SAVE);                             /* undo the sound loads: DRUM again */
        set_engine_of(t, ENGI_DRUM);
        t->engine = t->eng_req;
    }
    my_steps(t);
    go_page(GR_PATS);
    ui.ppick = 11;
    press(B_OCTUP);
    press(B_OCTUP);
    hold(B_SAVE);
    bad += check("BEAT over the user's steps: the dialog, then SAVE held brings them back", t->step[0].note[0] == 40 &&
                 !t->step[0].hit && t->p[P_SLEN] == 32);
    /* the menu: the keys play again */
    go_page(GR_ROLL);
    frame();
    k = song.grid;
    hold(B_HOME);
    frame();
    bad += check("HOME held (the menu): the keys are no longer the grid's", k == 1u && ui.menu && !song.grid);
    menu_close();
    return bad;
}

static int test_screen(void)
{
    int bad = 0;
    ui_power_on();
    ui_say("STOP TO SAVE", "");
    ui.force = 1;
    ui_draw();
    bad += check("a message keeps the header's transport and BPM (drawn right of x 80)", ui.msg_t != 0);
    ui.uboot = 3;
    ui.menu = 1;
    ui.force = 1;
    ui_draw();
    bad += check("UPDATE MODE countdown drawn over the menu", ui.force == 0);
    ui.uboot = 0;
    ui.menu = 0;
    return bad;
}

static int test_favorites(void)
{
    int bad = 0;
    uint32_t total, pos;
    track_t before;
    ui_power_on();
    my_steps(TSEL);
    TSEL->p[P_AMODE] = 2;
    go_page(GR_BROWSE);
    before = *TSEL;
    turn(EN_K3, 1);
    turn(EN_K4, 1);
    pos = preset_pos(&total);
    bad += check("FAV and LIST mark/filter the current sound without loading or touching its steps",
                 preset_favorite() && favorites.filter && total == 1 && pos == 0 &&
                 !memcmp(TSEL, &before, sizeof before));
    bad += check("one favorite occupies one display row, without repeated copies",
                 preset_visible(pos, total, 0) == 0 && preset_visible(pos, total, 1) == total);
    select_engine(ENGI_DRUM);
    before = *TSEL;
    turn(EN_K3, 1);
    bad += check("a DRUM sound can be a favorite on any track", preset_favorite() &&
                 favorite_has(ENGI_DRUM, 0) && !memcmp(TSEL, &before, sizeof before));
    turn(EN_PRESET, 1);
    bad += check("filtered browsing crosses DRUM and synth sounds while retaining the track's pattern and ARP",
                 TSEL->eng_req == 0 && preset_favorite() && !memcmp(TSEL->step, before.step, sizeof before.step) &&
                 TSEL->p[P_AMODE] == 2);
    turn(EN_K3, -1);
    pos = preset_pos(&total);
    bad += check("unmarking the current sound leaves it loaded even when outside the filtered list",
                 !preset_favorite() && total == 1 && pos == total && TSEL->eng_req == 0);
    turn(EN_PRESET, -1);
    bad += check("browsing from a nonfavorite selects the last favorite", TSEL->eng_req == ENGI_DRUM);
    turn(EN_K3, -1);
    before = *TSEL;
    turn(EN_PRESET, 1);
    bad += check("empty favorites explains LIST ALL and retains the current sound and steps",
                 msg_is("NO FAVORITES") && !memcmp(TSEL, &before, sizeof before));
    turn(EN_K4, -1);
    preset_pos(&total);
    bad += check("LIST ALL restores the full browser without an implicit load", !favorites.filter && total > 1 &&
                 !memcmp(TSEL, &before, sizeof before));
    up_store(31, "Favorite");
    up_load(31);
    turn(EN_K3, 1);
    bad += check("user slot 32 has its own favorite reference", favorite_has(NENGINES, 31) && preset_favorite());
    up_store(31, "Renamed");
    bad += check("overwriting or renaming a user slot retains its star", favorite_has(NENGINES, 31));
    up_put(31, 0);
    bad += check("successful erasure removes the user slot star and source label",
                 !favorite_has(NENGINES, 31) && !up_used(31) && !TSEL->user);
    up_store(31, "New sound");
    bad += check("reusing the erased slot does not restore its old star", !favorite_has(NENGINES, 31));
    return bad;
}

static int test_display_preferences(void)
{
    int bad = 0;
    track_t sounds[NTRK];
    uint16_t before[240 * 240];
    ui_power_on();
    memcpy(sounds, trk, sizeof sounds);
    hold(B_HOME);
    ui.menu_sel = MI_COLOR;
    ui.force = 1; ui_draw();
    memcpy(before, host_screen, sizeof before);
    settings.palette = 5; palette_set(5);
    press(B_OCTUP);
    bad += check("COLOR OCT+ previews the next palette (PAPER, light) without closing the menu",
                 settings.palette == 6 && T_BG == UI_PALETTES[6].bg && ux.light && ui.menu == 1 && !song.octave &&
                 memcmp(before, host_screen, sizeof before) && !memcmp(sounds, trk, sizeof sounds));
    turn(EN_K1, 1);
    bad += check("COLOR KNOB 1 steps on to HI-CON, CRYPT (GHOULBOX, id 8), NIGHT, MONO (black and white), then wraps to GREY",
                 settings.palette == 7 && (turn(EN_K1, 1), settings.palette == UI_CRYPT_INDEX) &&
                 str_eq(UI_PALETTES[UI_CRYPT_INDEX].name, "CRYPT") && (turn(EN_K1, 1), settings.palette == 9u) &&
                 T_BG == 0u && str_eq(UI_PALETTES[9].name, "NIGHT") && (turn(EN_K1, 1), settings.palette == UI_BW_INDEX) &&
                 str_eq(UI_PALETTES[UI_BW_INDEX].name, "MONO") && T_BG == 0u && T_TEXT == 0xFFFFu &&
                 (turn(EN_K1, 1), settings.palette == UI_GREY_INDEX) && str_eq(UI_PALETTES[UI_GREY_INDEX].name, "GREY"));
    settings.lowcut = 2;
    ui.menu_sel = MI_LOWCUT;
    press(B_OCTUP);
    bad += check("the expanded menu retains all three SPEAKER modes", settings.lowcut == 0 && !fx_lowcut);
    {   /* #42: SPEAKER EQ FLAT (the old OFF, stored 0) LOWCUT BASS+; KNOB 1 steps them, the audio follows at once */
        int ok = str_eq(MI_NAME[MI_LOWCUT], "SPEAKER EQ") && str_eq(SPK_EQ[0], "FLAT") && str_eq(SPK_EQ[1], "LOWCUT") &&
                 str_eq(SPK_EQ[2], "BASS+");
        turn(EN_K1, 1); ok &= settings.lowcut == 1u && fx_lowcut == 1u;
        turn(EN_K1, 1); ok &= settings.lowcut == 2u && fx_lowcut == 2u;
        turn(EN_K1, 1); ok &= settings.lowcut == 2u;                  /* (KNOB 1 stops at the ends) */
        turn(EN_K1, -1); turn(EN_K1, -1); ok &= settings.lowcut == 0u && !fx_lowcut;
        bad += check("#42 SPEAKER EQ: FLAT (old OFF) / LOWCUT / BASS+ on KNOB 1, fx_lowcut follows", ok);
    }
    press(B_OCTDN);
    bad += check("OCT- leaves display preferences without changing musical state",
                 !ui.menu && settings.palette == UI_GREY_INDEX && !memcmp(sounds, trk, sizeof sounds));
    return bad;
}

static int test_information(void)
{
    uint16_t header[240 * H_HEAD], footer[240 * 19], battery[240 * H_HEAD];
    track_t sounds[NTRK];
    int bad = 0, quiet = 1;
    uint32_t x, y;
    ui_power_on();
    memcpy(sounds, trk, sizeof sounds);
    hold(B_HOME);
    ui.menu_sel = MI_ABOUT;
    press(B_OCTUP);
    ui_draw();
    bad += check("ABOUT opens one document with scrollable license text and credits", ui.menu == 2 && !ui.menu_scroll && menu_scroll_max() > MENU_DOC_H);
    for (y = 146; y < 220; y++)
        for (x = 158; x < 232; x++)
            if (x < 166 || x >= 224 || y < 154 || y >= 212)
                quiet &= host_screen[y * 240 + x] == swap16(UI_QR_LIGHT);
    bad += check("ABOUT QR keeps its complete white quiet zone above the fixed footer", quiet);
    memcpy(header, host_screen, sizeof header);
    memcpy(footer, host_screen + 221 * 240, sizeof footer);
    turn(EN_PRESET, 3);
    ui_draw();
    bad += check("PRESETS scrolls ABOUT with its header and controls fixed",
                 ui.menu == 2 && ui.menu_scroll > 0 && !memcmp(header, host_screen, sizeof header) &&
                 !memcmp(footer, host_screen + 221 * 240, sizeof footer));
    turn(EN_PRESET, 12);
    ui_draw();
    bad += check("PRESETS continues from ABOUT into CREDITS without changing pages",
                 ui.menu == 2 && ui.menu_scroll > 216 && !memcmp(header, host_screen, sizeof header) &&
                 !memcmp(footer, host_screen + 221 * 240, sizeof footer));
    uint16_t position = ui.menu_scroll;
    press(B_OCTUP);
    bad += check("OCT+ does not switch pages or alter the octave in the document",
                 ui.menu == 2 && ui.menu_scroll == position && song.octave == 0);
    turn(EN_PRESET, 10000);
    turn(EN_PRESET, 10000);
    ui_draw();
    bad += check("the continuous document stops at its final credit",
                 ui.menu == 2 && ui.menu_scroll == menu_scroll_max());
    turn(EN_PRESET, -10000);
    bad += check("scrolling back through CREDITS returns to ABOUT's top", ui.menu == 2 && !ui.menu_scroll);
    press(B_OCTDN);
    bad += check("OCT- returns to the menu without changing sounds or patterns",
                 ui.menu == 1 && !memcmp(sounds, trk, sizeof sounds));
    press(B_OCTUP);
    bad += check("reopening ABOUT starts the continuous document at the top", ui.menu == 2 && !ui.menu_scroll);
    menu_close();
    ui.msg_t = 0;
    usb.config = 1; usb.suspended = 0;
    song.batt_raw = 530;
    ui.force = 1; draw_head(); ui.force = 0;
    memcpy(battery, host_screen, sizeof battery);
    uint32_t signature = ui.head_sig;
    for (x = 0; x < 4; x++) { fm1_ms += 600; draw_head(); }
    bad += check("USB power shows a steady bolt inside the battery without animation",
                 batt_shown() == 4 && ui.head_sig == signature && !memcmp(battery, host_screen, sizeof battery) &&
                 host_screen[6 * 240 + 217] != 0 && host_screen[9 * 240 + 225] != 0);
    usb.suspended = 1;
    draw_head();
    bad += check("USB suspension restores the battery outline and measured level",
                 batt_shown() == 0 && ui.head_sig != signature && host_screen[6 * 240 + 217] != 0);
    usb.config = 0; usb.suspended = 0;
    song.batt_raw = 531; int levels = batt_shown() == 1;
    song.batt_raw = 561; levels &= batt_shown() == 2;
    song.batt_raw = 591; levels &= batt_shown() == 3;
    bad += check("battery operation retains all three ADC level thresholds", levels);
    return bad;
}

/* GREY is grayscale on every screen: every page of every engine, HOME, the menu, ABOUT, the dialogs (RGB565 gray:
 * R = B, G = 2 R). MONO (black and white, blended per channel) is neutral on every screen: R = B, G within 2 of 2 R
 * (the 5-6-5 rounding of its edges) */
static int screen_gray(void)
{
    uint32_t i;
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = swap16(host_screen[i]);
        if ((c >> 11) != (c & 31u) || ((c >> 5) & 63u) != (c >> 11) * 2u)
            return 0;
    }
    return 1;
}
static int screen_neutral(void)
{
    uint32_t i;
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = swap16(host_screen[i]);
        int32_t d = (int32_t)((c >> 5) & 63u) - (int32_t)(c >> 11) * 2;
        if ((c >> 11) != (c & 31u) || d < -2 || d > 2)
            return 0;
    }
    return 1;
}
static int mono_screens(uint32_t pal, int (*chk)(void), uint32_t *n)
{
    int ok = 1;
    uint32_t e, i;
    for (e = 0; e < NENGINES; e++) {
        if (!eng_ok(e))
            continue;                               /* (DIGITAL without FELUCCA_FM4: never a track's) */
        for (i = 0; i < NPAGES; i++) {
            ui_power_on(); set_engine_of(TSEL, e); song.playing = 1; song.rec = 1;
            ui.home = 0; ui.page = (uint8_t)i; page_entered();
            if (!page_visible(i)) continue;
            settings.palette = pal; palette_set(pal);
            memset(host_screen, 0, sizeof host_screen);
            ui.force = 1; ui_draw();
            ok &= chk();
            (*n)++;
        }
        ui_power_on(); set_engine_of(TSEL, e); go_home();
        settings.palette = pal; palette_set(pal);
        ui.force = 1; ui_draw(); ok &= chk();
    }
    for (i = 0; i < 4u; i++) {
        ui_power_on(); song.rec = 3;
        settings.palette = pal; palette_set(pal);
        if (i < 2u) { ui.menu = (uint8_t)(i + 1u); ui.menu_scroll = 300; }
        else if (i == 2u) ui.confirm = CF_OVR_PROJ;
        else ui.uboot = 2;
        ui.force = 1; ui_draw(); ok &= chk();
    }
    return ok;
}
static int test_mono_screens(void)
{
    int bad = 0, ok;
    uint32_t e, n = 0, nb = 0;
    ok = mono_screens(UI_GREY_INDEX, screen_gray, &n);
    bad += check("GREY: every pixel of every page, HOME, menu, ABOUT, dialog and countdown is gray", ok && n > 200u);
    ok = mono_screens(UI_BW_INDEX, screen_neutral, &nb);
    bad += check("MONO (black and white): every pixel of every page, HOME, menu, ABOUT, dialog and countdown is neutral",
                 ok && nb == n);
    ui_power_on();
    for (e = 0; e < NPALETTES; e++) {               /* the other palettes are not gray: the check sees colour */
        ui_power_on(); palette_set(e); ui.force = 1; ui_draw();
        ok = screen_gray();
        if (e == UI_GREY_INDEX ? !ok : ok) bad += check("palette colour shows on HOME", 0);
    }
    palette_set(UI_GREY_INDEX);
    return bad;
}

/* ROLL: the rolling digits of the four cards and the header BPM (ui_draw.c roll_*). The GLOBAL page: BPM (also
 * in the header), SWG, CLK (a name), TUNE (signed). A roll frame differs from the static render only inside the
 * value strip, its last frame is the static render; unchanged digits stay; the direction follows the sign; a
 * change mid-roll retargets; a fast turn, another shape, a page / track / palette change and ui.force snap. */
static uint16_t roll_shot[240 * 240];
static void roll_settle(void)                     /* GLOBAL, nothing rolling, no knob hot */
{
    uint32_t k;
    go_title("GLOBAL");
    ui.hot_t = 0; ui.bpm_t = 0;
    for (k = 0; k < 12u; k++) frame();
}
/* host_screen against roll_shot: the pixels that differ inside the box (in = 1) or outside it */
static uint32_t roll_diff(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int in)
{
    uint32_t n = 0;
    int32_t x, y;
    for (y = 0; y < 240; y++)
        for (x = 0; x < 240; x++) {
            int inside = x >= x0 && x < x1 && y >= y0 && y < y1;
            int torch = y >= Y_GRAPH + TORCH_Y && y < Y_GRAPH + TORCH_Y + 30 &&     /* (GHOULBOX: the torches flicker */
                        ((x >= TORCH_LX && x < TORCH_LX + 12) || (x >= TORCH_RX && x < TORCH_RX + 12));   /* on their own) */
            if (!torch && inside == in && host_screen[y * 240 + x] != roll_shot[y * 240 + x]) n++;
        }
    return n;
}
/* the mean row (x16) of the ink (not the card surface) in columns x0..x1-1, rows y0..y1-1 of a screen */
static int32_t roll_ink_row(const uint16_t *s, int32_t x0, int32_t x1, int32_t y0, int32_t y1)
{
    int32_t x, y, n = 0, sum = 0;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++)
            if (s[y * 240 + x] != swap16(T_SURF)) { n++; sum += y * 16; }
    return n ? sum / n : -1;
}
static int roll_static_now(void)                  /* the screen is the static render (a forced redraw changes nothing) */
{
    memcpy(roll_shot, host_screen, sizeof roll_shot);
    ui.force = 1;
    ui_draw();
    return roll_diff(0, 0, 0, 0, 0) == 0;
}
#define STRIP(c) CARD_X(c), Y_LABEL + ROLL_Y, CARD_X(c) + COL_W, Y_LABEL + ROLL_Y + ROLL_H
static int test_roll(void)
{
    int bad = 0, ok;
    uint32_t k, n, e, id;
    int32_t up, down, st;
    static const struct { const char *a, *b; int d; } DIRS[] = {
        {"120", "121", 1}, {"121", "120", -1}, {"129", "130", 1}, {"-12", "-13", -1}, {"-13", "-12", 1}, {"+5", "+6", 1},
        {"1.25", "1.30", 1}, {"0.9", "0.8", -1}, {"9", "10", 0}, {"0", "-1", 0}, {"+5", "-5", 0}, {"C3", "C4", 0},
        {"1/4", "1/8", 0}, {"OFF", "ON", 0}, {"2X", "4X", 0}, {"120", "120", 0}, {"123456", "123457", 1},
        {"1234567", "1234568", 0}, {"12%", "13%", 0}};

    ok = 1;
    for (k = 0; k < sizeof DIRS / sizeof DIRS[0]; k++)
        if (roll_dir(DIRS[k].a, DIRS[k].b) != DIRS[k].d) { printf("  roll_dir %s -> %s\n", DIRS[k].a, DIRS[k].b); ok = 0; }
    bad += check("roll: numbers of one shape roll by the sign of the change; names, length, sign changes snap", ok);
    ok = 1;                                         /* every page of every engine: names never roll, numbers do */
    n = 0;
    for (e = 0; e < NENGINES; e++)
        for (id = 0; id < NPAGES && eng_ok(e); id++) {
            uint32_t c;
            ui_power_on(); set_engine_of(TSEL, e);
            ui.home = 0; ui.page = (uint8_t)id; page_entered();
            if (!page_visible(id) || PAGES[id].graph == GR_MOD) continue;
            ui.force = 1; ui_draw();
            for (c = 0; c < 4u; c++) {
                int16_t *vp;
                const param_desc_t *d = PAGES[id].scope == SC_GLOBAL || PAGES[id].scope == SC_TRACK ||
                                        PAGES[id].scope == SC_ENGINE ? page_desc(cur_page(), c, &vp) : 0;
                int32_t v, v0;
                if (!d || !d->label || d->label[0] == '-' || ((act_cols() >> c) & 1u)) continue;
                v0 = *vp;
                for (v = d->min; v < d->max && v - d->min < 40; v++) {
                    *vp = (int16_t)v; ui.frame += ROLL_SNAP; draw_columns();
                    *vp = (int16_t)(v + 1); ui.frame += ROLL_SNAP; draw_columns();
                    if (ui.roll[c].from[0] && (d->fmt == F_ENUM || d->fmt == F_NOTE || d->fmt == F_ONOFF)) {
                        printf("  %s/%s %s: %s rolls\n", ENGINES[e]->name, PAGES[id].title, d->label, ui.roll[c].from);
                        ok = 0;
                    }
                    n += ui.roll[c].from[0] != 0;
                }
                *vp = (int16_t)v0;
            }
        }
    ok &= n > 500u;
    bad += check("roll: every page and engine: a name (enum, even ALG 1..8, note, ON/OFF) snaps, numbers roll", ok);

    /* TUNE 0 -> 1 (card 4): a roll of ROLL_FRAMES frames, the last one the static render */
    ui_power_on(); roll_settle();
    turn(EN_K4, 1);
    ok = song.g[G_TUNE] == 1 && str_eq(ui.roll[3].from, "0") && ui.roll[3].dir == 1;
    for (n = 1; ui.roll[3].from[0] && n < 20u; n++) frame();
    ok &= n == ROLL_FRAMES;
    bad += check("roll: a card's value rolls for 9 frames (TUNE 0 -> 1), the value itself at once", ok);
    bad += check("roll: the roll's last frame is pixel-identical to the static render", roll_static_now());

    /* mid-roll: only the value strip differs from the static render */
    ui_power_on(); roll_settle();
    turn(EN_K4, 1); frame(); frame(); frame();
    memcpy(roll_shot, host_screen, sizeof roll_shot);
    ui.force = 1; ui_draw();                       /* (snaps: the static render) */
    ok = roll_diff(STRIP(3), 0) == 0 && roll_diff(STRIP(3), 1) > 20u && !ui.roll[3].from[0];
    bad += check("roll: a mid-roll frame differs from the static render inside the value strip only", ok);

    /* BPM 120 -> 121 (SELECT): the header rolls, the digits 1 and 2 stay; the BPM card too (no unit:
     * the label says BPM: set in M, it rolls like any number) */
    ui_power_on(); roll_settle();
    turn(EN_SELECT, 1);
    ok = song.g[G_BPM] == 121 && str_eq(ui.roll[ROLL_BPM].from, "120") && ui.roll[ROLL_BPM].dir == 1 &&
         str_eq(ui.roll[0].from, "120") && ui.roll[0].dir == 1;
    frame(); frame();
    memcpy(roll_shot, host_screen, sizeof roll_shot);
    ui.force = 1; ui_draw();
    n = roll_diff(BPM_X, 0, BPM_X + BPM_W, H_HEAD, 1);
    ok &= n > 20u && roll_diff(BPM_X, 0, BPM_X + BPM_W, H_HEAD, 0) == roll_diff(STRIP(0), 1);   /* (+ the card's strip) */
    ok &= roll_diff(BPM_X, 0, BPM_X + text_w(&AF_M, "12"), H_HEAD, 1) == 0;
    ok &= roll_diff(CARD_X(0), Y_LABEL + ROLL_Y, CARD_X(0) + 5 + text_w(&AF_M, "12"), Y_LABEL + ROLL_Y + ROLL_H, 1) == 0;
    /* TUNE 10 -> 11 on a card: the 1 stays */
    ui_power_on(); song.g[G_TUNE] = 10; roll_settle();
    turn(EN_K4, 1); frame(); frame();
    memcpy(roll_shot, host_screen, sizeof roll_shot);
    ui.force = 1; ui_draw();
    ok &= song.g[G_TUNE] == 11 && roll_diff(STRIP(3), 1) > 20u && roll_diff(STRIP(3), 0) == 0;
    ok &= roll_diff(CARD_X(3), Y_LABEL + ROLL_Y, CARD_X(3) + 5 + text_w(&AF_M, "1"), Y_LABEL + ROLL_Y + ROLL_H, 1) == 0;
    bad += check("roll: header BPM 120 -> 121 and a card 10 -> 11: only the strip changes, unchanged digits stay put", ok);

    /* the direction: 5 -> 6 and -5 -> -6 at frame 4: the new digit comes from below (up) / above (down) */
    ui_power_on(); song.g[G_TUNE] = 5; roll_settle();
    st = roll_ink_row(host_screen, CARD_X(3) + 5, CARD_X(3) + 16, Y_LABEL + ROLL_Y, Y_LABEL + ROLL_Y + ROLL_H);
    turn(EN_K4, 1); frame(); frame(); frame();
    up = roll_ink_row(host_screen, CARD_X(3) + 5, CARD_X(3) + 16, Y_LABEL + ROLL_Y, Y_LABEL + ROLL_Y + ROLL_H);
    ok = ui.roll[3].dir == 1;
    ui_power_on(); song.g[G_TUNE] = 7; roll_settle();
    turn(EN_K4, -1); frame(); frame(); frame();
    down = roll_ink_row(host_screen, CARD_X(3) + 5, CARD_X(3) + 16, Y_LABEL + ROLL_Y, Y_LABEL + ROLL_Y + ROLL_H);
    ok &= ui.roll[3].dir == -1 && st > 0 && up > st + 16 && down < st - 16;
    ui_power_on(); song.g[G_TUNE] = -5; roll_settle();
    turn(EN_K4, -1);
    ok &= song.g[G_TUNE] == -6 && str_eq(ui.roll[3].from, "-5") && ui.roll[3].dir == -1;
    bad += check("roll: an increase rolls up, a decrease down (-5 -> -6 is down)", ok);

    /* retarget: a change mid-roll restarts from the value it was going to; a fast turn snaps */
    ui_power_on(); song.g[G_TUNE] = 1; roll_settle();
    turn(EN_K4, 1); frame(); frame(); frame();
    turn(EN_K4, 1);
    ok = song.g[G_TUNE] == 3 && str_eq(ui.roll[3].from, "2") && (uint8_t)(ui.frame - ui.roll[3].t0) == 0;
    for (n = 1; ui.roll[3].from[0] && n < 20u; n++) frame();
    ok &= n == ROLL_FRAMES && roll_static_now();
    bad += check("roll: a change mid-roll retargets (from the last value, restarted) and ends static", ok);
    ui_power_on(); song.g[G_TUNE] = 1; roll_settle();
    turn(EN_K4, 1); turn(EN_K4, 1);
    ok = song.g[G_TUNE] == 3 && !ui.roll[3].from[0] && roll_static_now();
    bad += check("roll: a change within 40 ms of the last one snaps (a fast turn)", ok);

    /* snaps: another shape, a name, the page, the track, the palette, ui.force */
    ui_power_on(); roll_settle();
    turn(EN_K4, -1);
    ok = song.g[G_TUNE] == -1 && !ui.roll[3].from[0] && roll_static_now();
    for (k = 0; k < 5u; k++) frame();
    turn(EN_K3, 1);
    ok &= song.g[G_CLOCK] != 0 && !ui.roll[2].from[0] && roll_static_now();
    bad += check("roll: a sign / length change (0 -> -1) and a name (CLK) snap", ok);
    ok = 1;
    for (k = 0; k < 5u; k++) {
        ui_power_on(); roll_settle();
        turn(EN_K4, 1);
        ok &= ui.roll[3].from[0] != 0;
        if (k == 0) go_title("SYSTEM");
        else if (k == 1) turn(EN_ALGO, 1);
        else if (k == 2) { settings.palette = 1; palette_set(1); }          /* (no ui.force) */
        else if (k == 3) set_engine_of(TSEL, eng_step(TSEL->eng_req, 1));   /* (no ui.force) */
        else ui.force = 1;
        frame();
        ok &= !ui.roll[3].from[0] && !ui.roll[ROLL_BPM].from[0];
    }
    ui_power_on(); roll_settle();
    turn(EN_SELECT, 1);
    ok &= ui.roll[ROLL_BPM].from[0] != 0;
    ui.force = 1; frame();
    ok &= !ui.roll[ROLL_BPM].from[0];
    bad += check("roll: a page, track, palette or engine change and ui.force snap", ok);
    palette_set(UI_GREY_INDEX);

    /* MONO: every roll frame is gray */
    ui_power_on(); roll_settle();
    ok = 1;
    turn(EN_SELECT, 9);
    turn(EN_K4, -1);
    for (k = 0; k < ROLL_FRAMES; k++) { ok &= screen_gray(); frame(); }
    bad += check("roll: MONO roll frames are gray", ok);
    ui_power_on();
    return bad;
}

/* #63 ("parameters moving on their own, fixed by a page change"): a soak of the cards and the rolling digits.
 * ~40 min of UI frames: random turns of KNOB 1..4 and SELECT (bursts back and forth), page / HOME / track
 * changes, the transport and REC (motion recorded and played back), LFO rates, uneven frame times, ui.frame
 * and fm1_ms wrapping. After every frame no roll is older than ROLL_FRAMES; at random quiet moments (no
 * roll, nothing hot) the cards are pixel-identical to a forced redraw (what a page change would show). */
static int test_soak_cards(void)
{
    uint32_t seed = 63u, f, quiet = 0, old = 0, stale = 0, cmp = 0, c;
    static uint16_t shot[240 * CARD_H];
#define SOAK_RND(n) ((seed = seed * 1103515245u + 12345u) >> 8) % (n)
    ui_power_on();
    ui.frame = 0xFFFFFF00u;
    fm1_ms = 0xFFFFFFFFu - 60000u;
    for (f = 0; f < 150000u; f++) {
        uint32_t r = SOAK_RND(1000u);
        if (r < 25u) {                                  /* a knob, 1..3 detents either way */
            uint32_t role = SOAK_RND(5u) ? EN_K1 + SOAK_RND(4u) : EN_SELECT;
            int32_t s = (int32_t)SOAK_RND(3u) + 1;
            host_enc[panel.enc[role]] += (SOAK_RND(2u) ? s : -s) * panel.dir[role];
            quiet = 0;
        } else if (r < 29u) {                           /* a page */
            uint32_t i = SOAK_RND(NPAGES);
            if (page_visible(i)) { ui.home = 0; ui.page = (uint8_t)i; page_entered(); }
            quiet = 0;
        } else if (r < 30u) { ui.home = 1; page_entered(); quiet = 0; }
        else if (r < 31u) transport_req = song.playing ? 2u : 1u;
        else if (r == 31u && SOAK_RND(3u) == 0u) rec_tap();
        else if (r == 32u) { host_enc[panel.enc[EN_ALGO]] += SOAK_RND(2u) ? 1 : -1; quiet = 0; }
        else if (r == 33u) trk[SOAK_RND(NTRK)].p[P_LRATE] = (int16_t)SOAK_RND(128u);
        fm1_ms += SOAK_RND(50u) ? 16u : 1u + SOAK_RND(200u);
        host_ticks += 16000u + SOAK_RND(3u) * 4000u;
        ui_input();
        if (song.playing || transport_req) events_block(FS * 16u / 1000u);
        ui_draw();
        quiet++;
        if (ui.menu || ui.layer || ui.confirm || name_on() || ui.uboot)
            continue;                                   /* (the cards are not drawn) */
        for (c = 0; c < 5u; c++)
            old += ui.roll[c].from[0] && (uint8_t)(ui.frame - ui.roll[c].t0) >= ROLL_FRAMES;
        if (quiet > 45u && !ui.hot_t && !ui.bpm_t && !ui.msg_t && SOAK_RND(8u) == 0u) {
            for (c = 0; c < 5u && !ui.roll[c].from[0]; c++) {}
            if (c < 5u)
                continue;
            memcpy(shot, &host_screen[Y_LABEL * 240], sizeof shot);
            ui.force = 1;
            ui_draw();
            cmp++;
            stale += memcmp(shot, &host_screen[Y_LABEL * 240], sizeof shot) != 0;
        }
    }
#undef SOAK_RND
    if (old || stale || cmp < 500u) printf("  soak: %u old rolls, %u stale cards in %u checks\n", old, stale, cmp);
    stop_transport();
    memset(&motion, 0, sizeof motion);                  /* (what it recorded: the tests below start without) */
    memset(motion_active, 0, sizeof motion_active);
    motion_base_valid = motion_full = 0;
    ui_power_on();
    return check("#63 soak: 150000 random frames, wrapping clocks, motion: no roll lasts, cards = a forced redraw",
                 !old && !stale && cmp >= 500u);
}

static void screen_save(const char *dir, const char *name)
{
    char path[512];
    uint32_t i;
    FILE *f;
    snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
    f = fopen(path, "wb");
    if (!f) return;
    ui.msg_t = 0; ui.hot_t = 0; ui.force = 1;
    ui_draw();
    fprintf(f, "P6\n240 240\n255\n");
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = swap16(host_screen[i]);           /* canvas stores panel-order RGB565 */
        uint8_t b[3] = {(uint8_t)((c >> 11) * 255u / 31u), (uint8_t)(((c >> 5) & 63u) * 255u / 63u), (uint8_t)((c & 31u) * 255u / 31u)};
        fwrite(b, 1, 3, f);
    }
    fclose(f);
}
static void chain_screens(const char *dir)
{
    uint32_t k;
    ui_power_on();
    go_page(GR_SONG);
    screen_save(dir, "song-empty");
    for (k = 0; k < 2u; k++) { my_steps(&trk[0]); project_save(k); }
    chain_config.count = 3;
    chain_config.row[0] = (chain_row_t){0, 2};
    chain_config.row[1] = (chain_row_t){1, 4};
    chain_config.row[2] = (chain_row_t){0, 1};
    ui.song_row = 1;
    screen_save(dir, "song-ready");
    chain_prepare(); events_block(32);
    screen_save(dir, "song-playing");
    seq_stop();
}

static int test_chain(void)
{
    uint32_t i, k, period, last, n = 32u;
    int bad = 0, ok;
    step_t before[NTRK][NSTEP];
    int16_t timing[NTRK][4], sounds[NTRK][P_COUNT];
    ui_power_on();
    bad += check("SONG empty: PLAY does not start", chain_prepare() == 1 && !transport_req);
    for (k = 0; k < 2u; k++) {
        for (i = 0; i < NTRK; i++) {
            track_defaults_steps(&trk[i]);
            trk[i].p[P_SLEN] = (int16_t)(2u + k);
            trk[i].step[0] = (step_t){{(uint8_t)(60u + 5u * k), 0, 0, 0}, 1, ST_NOTE, 0, 96, 0, 0};
        }
        project_save(k);
    }
    for (i = 0; i < NTRK; i++) {
        my_steps(&trk[i]);
        trk[i].p[P_SLEN] = 9;
        memcpy(before[i], trk[i].step, sizeof before[i]);
        memcpy(timing[i], &trk[i].p[P_SLEN], sizeof timing[i]);
        memcpy(sounds[i], trk[i].p, sizeof sounds[i]);
    }
    chain_config.count = 2;
    chain_config.row[0] = (chain_row_t){0, 2};
    chain_config.row[1] = (chain_row_t){3, 1};
    bad += check("SONG missing source: refuses before changing any track", chain_prepare() == 6 && !chain.armed &&
        !memcmp(trk[0].step, before[0], sizeof before[0]));
    chain_config.row[1].slot = 1;
    project_save(2);
    chain_defaults(&chain_config);
    project_load(2);
    bad += check("SONG rows saved and loaded with their project", chain_config.count == 2 &&
        chain_config.row[0].repeat == 2 && chain_config.row[1].slot == 1);
    song.rec = 3;
    bad += check("SONG prepares while stopped, no starts over a pending start", chain_prepare() == 0 && chain_prepare() == 2);
    events_block(n);
    bad += check("SONG starts all tracks at source step 0, recording paused", chain.running && song.playing &&
        !song.rec && trk[0].seq_idx == 0 && seq_steps(&trk[0])[0].note[0] == 60 && trk[0].seq_notes[0] == 60);
    period = div_samples((uint32_t)trk[0].p[P_SDIV]);
    events_block(period);
    events_block(period);
    bad += check("SONG first row repeats without a gap", chain.row == 0 && chain.remaining == 1 && trk[0].seq_idx == 0);
    events_block(period);
    events_block(period);
    ok = chain.row == 1 && chain.remaining == 1;
    for (i = 0; i < NTRK; i++) {
        ok &= trk[i].seq_idx == 0 && trk[i].p[P_SLEN] == 3 && seq_steps(&trk[i])[0].note[0] == 65;
        for (k = 0; k < P_COUNT; k++)
            if (k < P_SLEN || k > P_SGATE) ok &= trk[i].p[k] == sounds[i][k];
    }
    bad += check("SONG row change: four tracks together, sounds unchanged", ok);
    open_family(FAM_SEQ);
    for (k = 0; k < NPAGES && cur_page()->scope != SC_STEP; k++) open_family(FAM_SEQ);
    turn(EN_K2, 1); press(B_EDIT); hold(B_REC); hold(B_SAVE);
    bad += check("SONG playing: step edits, clears, recording and undo are blocked", !memcmp(trk[0].step, before[0], sizeof before[0]) && !ui.confirm);
    events_block(period);
    events_block(period);
    events_block(period);
    ok = !song.playing && !chain.running && song.rec == 3;
    for (i = 0; i < NTRK; i++) ok &= !trk[i].seq_n && !memcmp(trk[i].step, before[i], sizeof before[i]) &&
        !memcmp(&trk[i].p[P_SLEN], timing[i], sizeof timing[i]);
    bad += check("SONG end: stops and restores editable patterns, timing and record arms", ok);
    bad += check("SONG source projects stay unchanged", project_used(0) && project_used(1) && stored_note(0, 0, 0) == 60);
    chain_config.row[0].repeat = 1;
    chain_prepare(); events_block(n);
    trk[0].seq_idx = 1;
    last = step_samples(&trk[0], div_samples((uint32_t)trk[0].p[P_SDIV]), 1);
    trk[0].seq_pos = last - n + 19u;
    events_block(n);
    ok = chain.row == 1;
    for (i = 0; i < NTRK; i++) ok &= trk[i].seq_pos == 19u && trk[i].seq_idx == 0;
    bad += check("SONG transition preserves fractional block time on all tracks", ok);
    transport_req = 2; events_block(n);
    bad += check("SONG manual STOP restores the previous pattern", !chain.running && !song.playing && !memcmp(trk[0].step, before[0], sizeof before[0]));
    chain_prepare(); events_block(n);
    project_load(0);
    bad += check("PROJECT load during SONG stops it before loading new timing", !chain.running && !song.playing &&
        trk[0].p[P_SLEN] == 2 && trk[0].step[0].note[0] == 60 && !chain_config.count);
    ui_power_on();
    open_family(FAM_SEQ);
    for (k = 0; k < NPAGES && cur_page()->graph != GR_SONG; k++) open_family(FAM_SEQ);
    press(B_PLAY);
    bad += check("SONG page empty PLAY explains how to start", msg_is("ADD A SONG ROW") && !transport_req);
    turn(EN_K2, 1);
    bad += check("SONG SLOT knob adds the first row with one repeat", chain_config.count == 1 && !chain_config.row[0].slot && chain_config.row[0].repeat == 1);
    turn(EN_K1, 1); turn(EN_K2, 1); turn(EN_K2, 1); turn(EN_K3, 2);
    bad += check("SONG row, slot and repeat knobs build a chain", chain_config.count == 2 && ui.song_row == 1 &&
        chain_config.row[1].slot == 1 && chain_config.row[1].repeat == 3);
    turn(EN_K4, 30);
    project_save(0);
    chain_config.count = 1;
    chain_config.row[0] = (chain_row_t){0, 1};
    fm1_in.buttons |= (1u << panel.btn[B_OCTUP]) | (1u << panel.btn[B_OCTDN]);
    host_pressed |= fm1_in.buttons; frame();
    fm1_in.buttons = 0; frame();
    bad += check("SONG OCT chord does not start playback or leave the page", !transport_req && !ui.home);
    press(B_OCTUP);
    bad += check("SONG OCT+ on release starts, pending start blocks SAVE", chain.armed && transport_req == 1);
    project_save(3);
    bad += check("SONG pending start cannot write a project", msg_is("STOP TO SAVE") && !project_used(3));
    press(B_PLAY); events_block(32);
    bad += check("SONG PLAY cancels a pending start", !chain.running && !chain.armed && !song.playing);
    uint32_t old_count = chain_config.count;
    turn(EN_K4, 30);
    bad += check("SONG unused K4 cannot accidentally add or clear rows", chain_config.count == old_count);
    for (k = chain_config.count; k < CHAIN_ROWS; k++) { turn(EN_K1, 1); turn(EN_K2, 1); }
    bad += check("SONG append row stays bounded at 16", chain_config.count == CHAIN_ROWS && chain_valid(&chain_config));
    go_title("TOOLS"); turn(EN_K4, 1); press(B_OCTUP);
    bad += check("CLEAR SONG requires explicit confirmation", ui.confirm == CF_CLEAR_SONG && chain_config.count == CHAIN_ROWS);
    press(B_OCTDN);
    bad += check("cancel preserves the full song", chain_config.count == CHAIN_ROWS);
    turn(EN_K4, 1); press(B_OCTUP); press(B_OCTUP);
    bad += check("confirmed CLEAR SONG keeps musical steps", !chain_config.count && !ui.song_row);
    return bad;
}

/* Product controls are exercised through the actual button/encoder handlers
 * and canvas, including all pages, both fonts and every shipped palette. */
static int test_product_ux(void)
{
    int bad = 0, ok = 1;
    uint32_t i, p, b;
    for (i = 0; i < NPAGES; i++) {
        ui_power_on(); set_engine_of(TSEL, 1);         /* (DIGITAL; without FELUCCA_FM4 FM6: no OP pages) */
        ui.home = 0; ui.page = (uint8_t)i; page_entered();
        if (!page_visible(i))
            continue;
        press(B_REC);
        if (PAGES[i].graph == GR_SONG)
            ok &= song.rec == 0 && !transport_req && msg_is("[SEQ] TO RECORD");
        else ok &= song.rec == 1 && transport_req == 1 && ui.page == i && !ui.home;
    }
    bad += check("REC stays on every page, SONG stopped asks for pattern recording", ok);
    ui_power_on(); hold(B_HOME); press(B_REC);
    bad += check("REC in the MENU does nothing (the menu stays, no arm, no transport start)",
                 ui.menu == 1 && song.rec == 0 && !transport_req);
    ui.menu_sel = MI_ABOUT; press(B_OCTUP); press(B_REC);
    bad += check("REC in ABOUT does nothing (the document and its scroll stay)",
                 ui.menu == 2 && !ui.menu_scroll && song.rec == 0 && !transport_req);
    ui_power_on(); press(B_GLO);
    bad += check("GLO directly opens the four-channel MIXER", cur_page()->graph == GR_TRK);
    int16_t len = TSEL->p[P_SLEN], send = TSEL->p[P_REV];
    turn(EN_K3, 1);
    bad += check("MIXER K3 edits reverb, never sequence length", TSEL->p[P_SLEN] == len && TSEL->p[P_REV] == send + 1);
    frames(320);                                       /* (taps further apart than a double tap: #83) */
    press(B_GLO); frames(320); ok = cur_page()->fam == FAM_GLO && str_eq(cur_page()->title, "GLOBAL");
    press(B_GLO); frames(320); ok &= str_eq(cur_page()->title, "SYSTEM");
    press(B_GLO); ok &= cur_page()->graph == GR_TRK;
    bad += check("GLO cycles MIXER > GLOBAL > SYSTEM > MIXER", ok);
    go_home(); hold(B_SEQ);
    bad += check("long SEQ goes directly to SONG with no tap on release", cur_page()->graph == GR_SONG);
    song.rec = 1; chain.armed = 1; press(B_REC);
    bad += check("REC cannot write borrowed patterns while SONG is armed", song.rec == 1 && msg_is("STOP TO RECORD"));
    ui_power_on(); open_family(FAM_EDIT);
    ok = 1;
    for (i = 0; i < 8u; i++) { open_family(FAM_EDIT); ok &= page_visible(ui.page) && !(cur_page()->id[0] >= P_FM1_ATK && cur_page()->id[0] <= P_FM4_LEVEL); }
    bad += check("operator envelope pages are hidden on non-DIGITAL instruments", ok);
#if FELUCCA_FM4
    set_engine_of(TSEL, 1); open_family(FAM_EDIT); ok = 0;
    for (i = 0; i < 12u; i++) { if (cur_page()->id[0] == P_FM1_ATK) ok = 1; open_family(FAM_EDIT); }
    bad += check("DIGITAL exposes four envelopes and independent operator levels", ok);
    go_title("OP1 ENV"); turn(EN_K1, 4);
    bad += check("operator envelope K1 edits only OP1 attack", TSEL->p[P_FM1_ATK] == 4 && !TSEL->p[P_FM2_ATK]);
    track_select(2); frame();
    bad += check("changing to a non-FM track leaves the stale operator page", page_visible(ui.page) && cur_page()->id[0] == P_E0);
#else
    set_engine_of(TSEL, ENGI_DIGITAL); open_family(FAM_EDIT); ok = TSEL->eng_req == ENGI_FM6;
    for (i = 0; i < 12u; i++) { ok &= !(cur_page()->id[0] >= P_FM1_ATK && cur_page()->id[0] <= P_FM4_LEVEL); open_family(FAM_EDIT); }
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].fam == FAM_EDIT && PAGES[i].id[0] >= P_FM1_ATK && PAGES[i].id[0] <= P_FM4_LEVEL)
            ok &= !page_visible(i);
    bad += check("DIGITAL retired: engine 1 asked for loads FM6; the OP ENV / OP LEVEL pages never show", ok);
#endif
    ui_power_on(); go_page(GR_CHANCE); turn(EN_K2, -35);
    bad += check("CHANCE is per step and starts at backward-compatible 100 percent", step_chance(&TSEL->step[0]) == 65 && step_chance(&TSEL->step[1]) == 100);
    turn(EN_K2, -1000); ok = step_chance(&TSEL->step[0]) == 0;
    turn(EN_K2, 1000); ok &= step_chance(&TSEL->step[0]) == 100;
    bad += check("CHANCE controls clamp to 0..100 without changing adjacent steps", ok);
    ui_power_on(); song.rec = 1; seq_start(); go_home();
    int16_t baseline = TSEL->p[P_E0]; turn(EN_K1, 1);
    bad += check("REC plus a sound knob records motion through the UI", motion_count(TSEL) == 1 && motion_enabled(TSEL));
    press(B_PLAY); events_block(32);
    bad += check("stopping returns the original sound after a recorded knob gesture", TSEL->p[P_E0] == baseline);
    go_page(GR_MOTION); turn(EN_K1, -1);
    bad += check("MOTION playback OFF preserves its stored events", !motion_enabled(TSEL) && motion_count(TSEL) == 1);
    turn(EN_K4, 1); press(B_OCTUP);
    bad += check("MOTION clear always asks confirmation", ui.confirm == CF_CLEAR_MOTION && motion_count(TSEL) == 1);
    press(B_OCTDN); turn(EN_K4, 1); press(B_OCTUP); press(B_OCTUP);
    bad += check("confirmed MOTION clear removes events and SAVE hold restores them", motion_count(TSEL) == 0);
    hold(B_SAVE);
    bad += check("motion-clear UNDO restores the recorded events", motion_count(TSEL) == 1);
    {   /* #63: a card shows MOTION's icon while the track's motion changes its parameter */
        uint32_t id = ENGINES[TSEL->eng_req]->knob[0];
        ui_power_on(); go_home(); frame();
        ok = !card_motion(0) && !card_motion(1);
        song.rec = 1; seq_start(); go_home(); turn(EN_K1, 1); frame();
        ok &= card_motion(0) && !card_motion(1) && !card_motion(2) && !card_motion(3);
        bad += check("MOTION: a recorded knob marks its HOME card (and no other)", ok && motion_count(TSEL) == 1);
        song.rec = 0; press(B_PLAY); events_block(32); frame();
        ok = card_motion(0);
        go_page(GR_MOTION); turn(EN_K1, -1); go_home(); frame();
        ok &= !card_motion(0) && motion_count(TSEL) == 1;
        bad += check("MOTION: PLAY OFF takes the icon off the card, the events stay", ok);
        go_page(GR_MOTION); turn(EN_K1, 1); go_home(); frame();
        ok = card_motion(0);
        track_select(1); frame();
        ok &= !card_motion(0);                           /* (another track: its own motion, none) */
        track_select(0); frame();
        ok &= card_motion(0);
        bad += check("MOTION: PLAY ON again shows it; only on the track that has the motion", ok);
        go_page(GR_TRK); frame();
        ok = !card_motion(0) && !card_motion(1) && !card_motion(2);
        motion_set_event(TSEL, 3, P_PAN, 20); frame();
        ok &= card_motion(1) && !card_motion(0) && !card_motion(2) && !card_motion(3);
        bad += check("MOTION: a mix parameter's event marks its MIXER card (PAN)", ok);
        go_page(GR_MOTION); turn(EN_K4, 1); press(B_OCTUP); press(B_OCTUP);
        ok = motion_count(TSEL) == 0;
        go_home(); frame();
        ok &= !card_motion(0) && !card_motion(1);
        go_page(GR_TRK); frame();
        ok &= !card_motion(1);
        bad += check("MOTION: CLEAR takes the icon off every card", ok && id < P_COUNT);
    }
    ui_power_on(); ui_leds();
    memset(led_pos, 0xFF, sizeof led_pos);
    led_pos[panel.btn[B_PLAY]] = 1; led_pos[panel.btn[B_REC]] = 2;
    song.playing = song.rec = 1; ok = 1;
    for (i = 0; i < 120u; i++) { fm1_ms += 17; ui_leds(); ok &= (fm1_led[0] & 6u) == 4u && (fm1_led[8] & 2u); }
    bad += check("PLAY's green and REC LEDs stay lit across time and audio block phase", ok);
    led_pos_init();
    ok = 1;
    for (p = 0; p < NPALETTES; p++) for (b = 0; b < 1u; b++) {
        uint16_t graph[240 * H_GRAPH], columns[240 * (Y_SEP_END - Y_LABEL)];
        ui_power_on(); palette_set(p); settings.zoom = 1;
        open_family(FAM_ENV); frame();
        memcpy(graph, host_screen + Y_GRAPH * 240, sizeof graph);
        memcpy(columns, host_screen + Y_LABEL * 240, sizeof columns);
        ui.hot_col = 1; ui.hot_t = 40; ui.force = 1; ui_draw();
        ok &= !memcmp(graph, host_screen + Y_GRAPH * 240, sizeof graph);
        int changed = 0;
        for (uint32_t y = 0; y < Y_SEP_END - Y_LABEL; y++) for (uint32_t x = 0; x < 240; x++) {
            uint16_t old = columns[y * 240 + x], now = host_screen[(Y_LABEL + y) * 240 + x];
            if (x >= 64 && x < 119) changed |= old != now;
            else ok &= old == now;
        }
        ok &= changed;
    }
    bad += check("all palettes: active column is subtle, stable-size, no zoom over graph", ok);
    ui_power_on(); set_engine_of(TSEL, 4); open_family(FAM_EDIT);
    memset(&sample_wave, 0, sizeof sample_wave); last_note = 60;
    voice_t voices[NVOICE]; memcpy(voices, TSEL->v, sizeof voices);
    ok = 1;
    for (i = 0; i < 2000u && !sample_wave.ready; i++) {
        uint32_t old = sample_wave.pos; sample_wave_tick(TSEL);
        ok &= sample_wave.pos - old <= 512u;
    }
    ok &= sample_wave.ready && !memcmp(voices, TSEL->v, sizeof voices);
    int16_t lo[SAMPLE_WAVE_COLS] = {0}, hi[SAMPLE_WAVE_COLS] = {0};
    voice_t probe = {0}; const smp_zone_t *zone = sample_wave.zone;
    if (zone) for (i = 0; i < zone->n; i++) {
        int32_t v = sample_next(zone, &probe, 0); uint32_t col = i * SAMPLE_WAVE_COLS / zone->n;
        if (v < lo[col]) lo[col] = (int16_t)v;
        if (v > hi[col]) hi[col] = (int16_t)v;
    }
    ok &= !memcmp(lo, sample_wave.lo, sizeof lo) && !memcmp(hi, sample_wave.hi, sizeof hi);
    bad += check("sample waveform is bounded, matches audio IMA decode and leaves voices intact", ok);
    ui_power_on(); return bad;
}

static int test_panel(void)
{
    int bad = 0;
    panel = PANEL_DEFAULT;
    panel.btn[B_PLAY] = panel.btn[B_REC];
    panel_init();
    bad += check("duplicate button mappings recover to the default panel", !memcmp(&panel, &PANEL_DEFAULT, sizeof panel));
    panel.enc[EN_K1] = panel.enc[EN_K2];
    panel_init();
    bad += check("duplicate knob mappings recover to the default panel", !memcmp(&panel, &PANEL_DEFAULT, sizeof panel));
    panel.btn[B_PLAY] = 255;
    panel_init();
    bad += check("out-of-range panel data recovers without an invalid shift", !memcmp(&panel, &PANEL_DEFAULT, sizeof panel));
    panel.dir[EN_K1] = -1;
    panel_init();
    bad += check("a valid reversed knob mapping is retained", panel.dir[EN_K1] == -1);
    return bad;
}

/* ------------------------------------------------- the FX hold layer --- */
static void btn_down(uint32_t label) { fm1_in.buttons |= 1u << panel.btn[label]; host_pressed |= 1u << panel.btn[label]; }
static void btn_up(uint32_t label) { fm1_in.buttons &= ~(1u << panel.btn[label]); }
static void key_down(uint32_t k) { fm1_in.notes |= 1u << k; host_notes |= 1u << k; keyboard_block(); }
static void key_up(uint32_t k) { fm1_in.notes &= ~(1u << k); keyboard_block(); }
static uint32_t gates(void)                        /* voices of every track with their key down */
{
    uint32_t p, i, n = 0;
    for (p = 0; p < NTRK; p++)
        for (i = 0; i < NVOICE; i++)
            n += trk[p].v[i].gate != 0;
    return n;
}
static uint32_t white(uint32_t w) { return key_at(0, w); }

static int test_layer(void)
{
    int bad = 0, ok, flash;
    uint32_t i, mo, k;
    /* tap: on release, the page; held briefly: no map ever */
    ui_power_on();
    btn_down(B_FX); frame();
    ok = ui.home;
    frames(64); btn_up(B_FX); frame();
    bad += check("FX tap: nothing on press, the FX page when let go", ok && !ui.home && cur_page()->fam == FAM_FX);
    frames(320);                                       /* (not a double tap: #83) */
    press(B_FX);
    bad += check("  a second tap: the next page of the family (SLICER)", str_eq(cur_page()->title, "SLICER"));
    ui_power_on();
    btn_down(B_FX);
    for (flash = 0, i = 0; i < 22u; i++) { frame(); flash |= ui.layer; }      /* 0.35 s */
    btn_up(B_FX); frame();
    bad += check("  held 0.35 s (HOLD 0.4): still a tap, the map never shown", !flash && !ui.home);
    /* peek: past HOLD alone, the map; letting go does nothing */
    ui_power_on();
    btn_down(B_FX); frames(368);
    ok = !ui.layer;
    frames(64);
    ok &= ui.layer == LAYER_FX;
    btn_up(B_FX); frame();
    bad += check("FX held past 0.4 s: the map (not before); let go: no page, the map gone",
                 ok && ui.home && !ui.layer);
    /* the HOLD setting: 0.6 s */
    ui_power_on();
    hold(B_HOME); ui.menu_sel = MI_HOLD; turn(EN_K1, 1); turn(EN_K1, 1); ok = settings_hold == 3u; hold(B_HOME);
    btn_down(B_FX); frames(512); flash = ui.layer; btn_up(B_FX); frame();
    bad += check("menu HOLD 0.6 s (KNOB 1): FX held 0.5 s is a tap, no map", ok && !flash && !ui.home && !ui.menu);
    {
        persist_t p = {0};
        p.magic = PERSIST_MAGIC; p.panel = panel;
        settings_export(&p); settings_hold = HOLD_DEF;
        bad += check("  HOLD is saved with the settings and read back", settings_import(&p, sizeof p) && settings_hold == 3u);
    }
    /* the LEDS setting: KNOB 1 right INV, left DIM; OCT+ toggles; saved in the retired zoom field */
    ui_power_on();
    hold(B_HOME); ui.menu_sel = MI_LEDS;
    ok = settings_leds == LEDS_DIM;
    turn(EN_K1, 1); ok &= settings_leds == LEDS_INV;
    turn(EN_K1, 1); ok &= settings_leds == LEDS_INV;
    turn(EN_K1, -1); ok &= settings_leds == LEDS_DIM;
    turn(EN_K1, -1); ok &= settings_leds == LEDS_DIM_LO;
    turn(EN_K1, -1); ok &= settings_leds == LEDS_OFF;
    turn(EN_K1, -1); ok &= settings_leds == LEDS_OFF;
    press(B_OCTUP); ok &= settings_leds == LEDS_DIM_LO && ui.menu == 1u;
    press(B_OCTUP); ok &= settings_leds == LEDS_DIM;
    press(B_OCTUP); ok &= settings_leds == LEDS_INV;
    press(B_OCTUP); ok &= settings_leds == LEDS_OFF && ui.menu == 1u;
    ok &= !strcmp(LEDS_NAME[LEDS_OFF], "OFF") && !strcmp(LEDS_NAME[LEDS_DIM_LO], "DIM LO") &&
          !strcmp(LEDS_NAME[LEDS_DIM], "DIM HI") && !strcmp(LEDS_NAME[LEDS_INV], "INV");
    hold(B_HOME);
    bad += check("menu LEDS: DIM HI by default; KNOB 1 OFF < DIM LO < DIM HI < INV (stops at the ends), OCT+ cycles",
                 ok && !ui.menu);
    {
        persist_t p = {0};
        uint32_t m;
        p.magic = PERSIST_MAGIC; p.panel = panel;
        ok = 1;
        settings.zoom = 0;                              /* (DIM HI keeps an older record's 0 / 1 as saved) */
        for (m = 0; m < LEDS_COUNT; m++) {
            settings_leds = (uint8_t)m;
            settings_export(&p); settings_leds = LEDS_DIM;
            ok &= settings_import(&p, sizeof p) && settings_leds == m && leds_stored_ok(p.zoom) &&
                  p.zoom == (m == LEDS_DIM ? 0u : LEDS_TAG | m);
            settings_export(&p);                    /* (idempotent: saved again, the same field) */
            ok &= p.zoom == (m == LEDS_DIM ? 0u : LEDS_TAG | m);
        }
        settings_leds = LEDS_DIM;
        settings_export(&p);
        bad += check("  LEDS is saved with the settings and read back (all four); DIM HI saves the field as before (0)",
                     ok && p.zoom == 0u && settings_import(&p, sizeof p) && settings_leds == LEDS_DIM);
    }
    /* combo: a key with FX: at once, silent, no MIDI, no recording, no step */
    ui_power_on();
    usb.config = 1; mo = mo_w;
    go_page(GR_ROLL); my_steps(TSEL); song.rec = 1; song.playing = 1;
    {
        step_t before[NSTEP];
        uint32_t cur = ui.cursor;
        memcpy(before, TSEL->step, sizeof before);
        btn_down(B_FX); key_down(white(2)); frame();
        ok = ui.layer == LAYER_FX && !gates() && mo_w == mo && (kb_layer >> white(2)) & 1u &&
             (perf_held & PF_BIT(PF_R32));                    /* white key 3 (A3): REPEAT 1/32 */
        for (i = 0; i < 40u; i++) events_block(CTL);
        ok &= !memcmp(before, TSEL->step, sizeof before) && ui.cursor == cur;
        bad += check("FX + a key: the map at once; no voice, no MIDI, no recording, no step written", ok);
        key_up(white(2)); frame();
        bad += check("  the key let go: its effect off, no note-off sent", !perf_held && !kb_layer && mo_w == mo);
        btn_up(B_FX); frame();
        bad += check("  FX let go after a combo: no tap, the map gone", !ui.layer && cur_page()->graph == GR_ROLL);
    }
    song.playing = 0; song.rec = 0;
    /* FX let go first: the effect stays with its key, the map too; a key pressed now is a note */
    ui_power_on();
    btn_down(B_FX); key_down(white(4)); frame();          /* white key 5 (C4): LPF */
    btn_up(B_FX); frame();
    ok = (perf_held & PF_BIT(PF_LPF)) && ui.layer == LAYER_FX;
    key_down(white(0)); frame();
    ok &= gates() > 0 && !((kb_layer >> white(0)) & 1u);
    key_up(white(0)); key_up(white(4)); frame();
    bad += check("FX let go first: the key holds its effect and the map; a new key is a note", ok && !ui.layer && !perf_held);
    /* OCT UP (G4) and OCT DN (A4), the 9th and 10th white keys: held effects, silent; KNOB 4 the shimmer while
     * held, back to 0 with FX; B4 on does nothing */
    ui_power_on();
    usb.config = 1; mo = mo_w;
    btn_down(B_FX); key_down(white(8)); frame();
    ok = ui.layer == LAYER_FX && !gates() && mo_w == mo && perf_held == PF_BIT(PF_OUP);
    perf_begin(CTL);                                    /* (an audio block: OCT UP runs, KNOB 4 is SHIMR) */
    turn(EN_K4, 40);
    ok &= perf_k[3] == 40;
    key_up(white(8)); key_down(white(9)); frame();
    ok &= perf_held == PF_BIT(PF_ODN) && !gates() && mo_w == mo;
    key_up(white(9)); key_down(white(10)); frame();
    ok &= !perf_held && (kb_layer >> white(10)) & 1u && !gates();
    key_up(white(10)); btn_up(B_FX); frame();
    bad += check("FX + G4 / A4: OCT UP / OCT DN held, silent, no MIDI; KNOB 4 turns; B4 nothing; FX let go: K4 back to 0",
                 ok && !perf_held && !ui.layer && !perf_k[3] && mo_w == mo);
    /* REVERB > TYPE (FX family, global): ROOM / SPRING on KNOB 1, kept by a project */
    ui_power_on();
    go_title("REVERB");
    ok = cur_page()->fam == FAM_FX && cur_page()->id[0] == G_RTYPE && cur_page()->id[1] == G_RSIZE &&
         cur_page()->id[2] == G_RDAMP && song.g[G_RTYPE] == 0;
    turn(EN_K1, 1);
    ok &= song.g[G_RTYPE] == 1 && str_eq(GP[G_RTYPE].names[song.g[G_RTYPE]], "SPRING");
    turn(EN_K1, 5);
    ok &= song.g[G_RTYPE] == 2 && str_eq(GP[G_RTYPE].names[song.g[G_RTYPE]], "HALL");   /* (the last: it stops there) */
    turn(EN_K1, -1);
    ok &= song.g[G_RTYPE] == 1;
    song.playing = 0;
    project_save(2);
    turn(EN_K1, -1);
    ok &= song.g[G_RTYPE] == 0;
    project_load(2);
    bad += check("REVERB page: TYPE ROOM -> SPRING -> HALL on KNOB 1 (SIZE, DAMP beside it); a project keeps SPRING",
                 ok && song.g[G_RTYPE] == 1);
    go_title("CHORUS");
    bad += check("  CHORUS page: CRT CDP", cur_page()->fam == FAM_FX && cur_page()->id[0] == G_CRATE &&
                 cur_page()->id[1] == G_CDEPTH && cur_page()->id[2] == 0xFFu);
    /* chording: a key held before FX stays a note, its note-off arrives */
    ui_power_on();
    mo = mo_w;
    key_down(white(4)); frame(); btn_down(B_FX); frame(); key_up(white(4)); frame(); btn_up(B_FX); frame();
    for (i = 0; i < 4u; i++) events_block(CTL);
    bad += check("a key held before FX stays a note: note-on, note-off, nothing left held", mo_w == mo + 2u && !gates() &&
                 !kb_layer);
    /* knob macros: not the page's, not recorded, back to 0 when FX is let go */
    ui_power_on();
    go_title("ENV"); song.rec = 1; song.playing = 1;
    {
        int16_t atk = TSEL->p[P_ATK];
        uint32_t ev = motion.count;
        btn_down(B_FX); frame();
        turn(EN_K1, -12); turn(EN_K2, 30);
        ok = perf_k[0] == -12 && perf_k[1] == 30 && TSEL->p[P_ATK] == atk && motion.count == ev && ui.layer == LAYER_FX;
        turn(EN_PRESET, 3);
        ok &= user_of(TSEL) == UP_SLOTS && TSEL->preset == trk[0].preset;
        btn_up(B_FX); frame();
        bad += check("FX + KNOB 1 / 2: the macros, not ATK, not recorded; FX let go: back to 0, no tap",
                     ok && !perf_k[0] && !perf_k[1] && str_eq(cur_page()->title, "ENV"));
    }
    song.rec = 0; song.playing = 0;
    /* buttons in the layer: PLAY works, SAVE, HOME, a page button are swallowed */
    ui_power_on();
    btn_down(B_FX); frame();
    press(B_PLAY);
    ok = transport_req == 1u && ui.layer == LAYER_FX;
    transport_req = 0;
    press(B_SAVE); ok &= ui.home;
    hold(B_SAVE); ok &= !msg_is("UNDO/REDO T1") && ui.home;
    press(B_ENV); ok &= ui.home;
    hold(B_HOME); ok &= !ui.menu && ui.home;
    btn_up(B_FX); frame();
    bad += check("in the layer: PLAY plays; SAVE (tap and hold), ENV, HOME (hold) do nothing", ok && ui.home && !ui.menu);
    btn_down(B_FX); frame(); btn_down(B_ENV); frame(); btn_up(B_FX); frame(); btn_up(B_ENV); frame();
    bad += check("  a page button pressed with FX, let go after it: still nothing", ui.home);
    /* release-act: every page button acts when let go */
    {
        static const uint8_t PB[6] = {B_ENV, B_LFO, B_SCL, B_ARP, B_GLO, B_EDIT};
        static const uint8_t PF[6] = {FAM_ENV, FAM_LFO, FAM_SCL, FAM_ARP, FAM_GLO, FAM_EDIT};
        ok = 1;
        for (i = 0; i < 6u; i++) {
            ui_power_on();
            btn_down(PB[i]); frame();
            ok &= ui.home;
            btn_up(PB[i]); frame();
            ok &= !ui.home && (PB[i] == B_GLO ? cur_page()->graph == GR_TRK : cur_page()->fam == PF[i]);
            if (PB[i] == B_SCL || PB[i] == B_GLO || PB[i] == B_EDIT)
                continue;                             /* (a layer of their own: held long is a peek) */
            ui_power_on();
            hold(PB[i]);                              /* (no layer of their own: a long press is a tap too) */
            ok &= !ui.home && cur_page()->fam == PF[i];
        }
        bad += check("ENV LFO SCL ARP GLO EDIT act when let go, not on press (ENV LFO ARP held long: the same)", ok);
    }
    ui_power_on();
    go_page(GR_ROLL); my_steps(TSEL); ui.cursor = 0;
    btn_down(B_EDIT); frame();
    ok = step_on(&TSEL->step[0]) && ui.cursor == 0;
    btn_up(B_EDIT); frame();
    bad += check("EDIT on STEP clears the step when let go (not on press)", ok && !step_on(&TSEL->step[0]) && ui.cursor == 1);
    ui_power_on(); hold(B_HOME);
    ok = ui.menu == 1; hold(B_HOME); ok &= !ui.menu;
    go_home(); hold(B_SEQ); ok &= cur_page()->graph == GR_SONG;
    bad += check("HOME (menu) and SEQ (SONG) holds of 0.7 s unchanged", ok);
    /* no layer in the menu or a dialog: FX + a key is a note */
    ui_power_on(); hold(B_HOME);
    btn_down(B_FX); frame(); key_down(white(3)); frame();
    ok = !ui.layer && !perf_mask && gates() > 0 && !kb_layer;
    key_up(white(3)); btn_up(B_FX); frame();
    bad += check("in the menu: no layer, FX + a key plays the key; FX let go: no page", ok && ui.menu == 1);
    ui_power_on(); ui.confirm = CF_CLEAR_SEQ;
    btn_down(B_FX); frames(512);
    ok = !ui.layer;
    btn_up(B_FX); frame();
    bad += check("in a dialog: FX held shows no map and taps nothing", ok && ui.confirm == CF_CLEAR_SEQ);
    /* UBOOT: the countdown closes the map and stops every effect; OCT- + OCT+ are still seen */
    ui_power_on();
    btn_down(B_FX); key_down(white(5)); frame();
    btn_down(B_OCTDN); btn_down(B_OCTUP); frame();
    ok = ui.layer == LAYER_FX && (fm1_in.buttons & 3u << panel.btn[B_OCTDN]) != 0u;
    ui.uboot = 3; frame();                          /* (main.c: OCT- + OCT+ held 2 s) */
    ok &= !ui.layer && perf_kill && !perf_mask && perf_begin(CTL) >= 0;
    ok &= (fm1_in.buttons & ((1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]))) ==
          ((1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]));
    ui.uboot = 0;
    btn_up(B_OCTDN); btn_up(B_OCTUP); key_up(white(5)); btn_up(B_FX); frame();
    bad += check("UBOOT countdown over the layer: the map closes, effects off; OCT- + OCT+ untouched", ok && !perf_kill && !kb_layer);
    /* the LEDs of the map */
    ui_power_on();
    btn_down(B_FX); key_down(white(1)); frame();
    {
        uint32_t a, b2;
        song.g[G_BPM] = 120;
        fm1_ms = 0; a = layer_leds();
        fm1_ms = 250; b2 = layer_leds();
        k = white(1);
        ok = ((a >> k) & 1u) && ((b2 >> k) & 1u);                    /* held: lit */
        for (i = 0; i < 10u; i++)                                     /* the other effects (F3 .. A4): blink */
            ok &= i == 1u || ((a ^ b2) >> white(i)) & 1u;
        for (i = 10; i < 16u; i++)                                    /* B4 .. G5: no effect, dark */
            ok &= !((a | b2) >> white(i) & 1u);
        ok &= ((a ^ b2) >> key_at(1, 0)) & 1u && !((a | b2) >> key_at(1, 4) & 1u);   /* a mute blinks, a spare black dark */
        song.g[G_BPM] = 72;
        fm1_ms = 0; a = layer_leds();
        fm1_ms = 250; b2 = layer_leds();
        ok &= !((a | b2) >> white(0) & 1u) && !((a | b2) >> white(3) & 1u);   /* 1/8, REVERSE: too long at 72 */
        ok &= ((a ^ b2) >> white(2)) & 1u;                            /* 1/32 still blinks */
        song.g[G_BPM] = 120;
    }
    key_up(white(1)); btn_up(B_FX); frame();
    bad += check("map LEDs: held lit, the 10 effects blink, B4 .. G5 and a too-long REPEAT dark", ok);
    usb.config = 0;
    return bad;
}

/* ------------------------------------------------------------ NAME --- */
static void nm_tap(uint32_t k)                    /* a key tapped in NAME: down a frame, up a frame (32 ms) */
{
    fm1_in.notes |= 1u << k; host_notes |= 1u << k; frame();
    fm1_in.notes &= ~(1u << k); frame();
}
static uint32_t nm_black_key(uint32_t f, uint32_t octave)   /* the black key of function f (NB_*: F# G# A# C# D#), octave 0 / 1 */
{
    return key_at(1, f + 5u * octave);
}
static int test_name(void)
{
    int bad = 0, ok;
    uint32_t i, mo;
    char b[16];
    ui_power_on();
    go_page(GR_USER);
    ui.uslot = 2;
    press(B_OCTUP);                               /* (SAVE picked on entry) */
    up_auto_name(b, TSEL->eng_req, 2);
    bad += check("NAME: USER SAVE into an empty slot opens NAME, prefilled with the automatic name",
                 name_on() && nm.kind == NK_USER_SAVE && nm.slot == 2u && str_eq(nm.s, b) && nm.cur == nm.len);
    for (i = 0; i < 12u; i++) nm_tap(nm_black_key(NB_DEL, 0));
    bad += check("  DELETE removes the character before the cursor, down to empty", nm.len == 0u && nm.cur == 0u && !nm.s[0]);
    /* multi-tap: the same key cycles, another key commits, 0.8 s commits */
    nm_tap(white(0));
    ok = nm.len == 1u && nm.s[0] == 'A' && nm.key == 1u && nm.cur == 0u;
    nm_tap(white(0));
    ok &= nm.s[0] == 'B' && nm.len == 1u;
    nm_tap(white(0));
    bad += check("  a white key types its first letter; tapped again within 0.8 s: the next one, cycling (A B A)",
                 ok && nm.s[0] == 'A' && nm.len == 1u && nm.cur == 0u);
    nm_tap(white(1));
    bad += check("  another key keeps the letter and types its own (A, C)", nm.len == 2u && str_eq(nm.s, "AC") && nm.cur == 1u &&
                 nm.key == 2u);
    frames(800);
    bad += check("  0.8 s without a tap keeps it: the cursor moves on", nm.key == 0u && nm.cur == 2u && str_eq(nm.s, "AC"));
    nm_tap(white(1));
    frames(400);
    nm_tap(white(1));
    bad += check("  a second tap within 0.8 s (0.43 s later) still cycles (C -> D)", str_eq(nm.s, "ACD") && nm.key == 2u);
    frames(800);
    nm_tap(white(4)); nm_tap(white(4)); nm_tap(white(4));   /* IJK: K */
    nm_tap(nm_black_key(NB_SPACE, 1));                       /* SPACE (the upper octave's G#) keeps K first */
    bad += check("  a three-letter key (IJK) cycles to K; SPACE keeps it and adds a space", str_eq(nm.s, "ACDK ") && nm.cur == 5u &&
                 !nm.key);
    nm_tap(nm_black_key(NB_DEL, 1));
    nm_tap(nm_black_key(NB_LEFT, 0));
    nm_tap(nm_black_key(NB_LEFT, 0));
    nm_tap(white(2));                             /* E inserted before D */
    frames(800);
    bad += check("  DELETE, cursor left twice, a letter inserted at the cursor", str_eq(nm.s, "ACEDK") && nm.cur == 3u);
    nm_tap(nm_black_key(NB_RIGHT, 1));
    nm_tap(nm_black_key(NB_RIGHT, 0));
    nm_tap(nm_black_key(NB_RIGHT, 0));
    bad += check("  cursor right stops at the end", nm.cur == 5u);
    /* 123 */
    nm_tap(nm_black_key(NB_MODE, 0));
    nm_tap(white(1)); nm_tap(white(9)); nm_tap(white(15));
    bad += check("  D# switches to 123: one tap, one character (2, 0, +), no cycling", nm.num && str_eq(nm.s, "ACEDK20+") &&
                 nm.cur == 8u && !nm.key);
    nm_tap(nm_black_key(NB_MODE, 1));
    nm_tap(white(15)); nm_tap(white(15)); nm_tap(white(15));
    frames(800);
    bad += check("  back to ABC: the last key (0-.) gives 0, then -, then .", !nm.num && str_eq(nm.s, "ACEDK20+.") && nm.cur == 9u);
    /* the knobs */
    turn(EN_K1, -2);
    turn(EN_K2, 1);
    bad += check("  KNOB 1 moves the cursor, KNOB 2 changes the character there (+ -> SPACE, wrapping)",
                 nm.cur == 7u && str_eq(nm.s, "ACEDK20 ."));
    turn(EN_K2, 5);
    bad += check("  KNOB 2 on: SPACE + 5 = E", nm.s[7] == 'E');
    turn(EN_K1, 9);
    turn(EN_K2, 1);
    bad += check("  KNOB 1 to the end, KNOB 2 adds a character there (A)", str_eq(nm.s, "ACEDK20E.A") && nm.cur == 9u && nm.len == 10u);
    turn(EN_K2, -2);
    bad += check("  .. and turns it back past SPACE to the last symbol (+)", nm.s[9] == '+');
    /* full */
    nm_tap(nm_black_key(NB_RIGHT, 0));
    nm_tap(white(7)); frames(800); nm_tap(white(7)); frames(800);
    ui.msg_t = 0;
    nm_tap(white(7));
    bad += check("  12 characters at most: NAME FULL, nothing typed", nm.len == 12u && msg_is("NAME FULL") && str_eq(nm.s, "ACEDK20E.+PP"));
    /* held DELETE repeats */
    fm1_in.notes |= 1u << nm_black_key(NB_DEL, 0); host_notes |= 1u << nm_black_key(NB_DEL, 0);
    frames(16 * 40);
    fm1_in.notes &= ~(1u << nm_black_key(NB_DEL, 0)); frame();
    bad += check("  DELETE held 0.64 s: one, then it repeats after 0.45 s, every 0.09 s (3 in all)", nm.len == 9u);
    /* the keys never sound, record or send MIDI */
    usb.config = 1; mo = mo_w; song.rec = 1;
    {
        step_t before[NSTEP];
        memcpy(before, TSEL->step, sizeof before);
        key_down(white(3)); key_down(nm_black_key(NB_SPACE, 0)); frame();
        for (i = 0; i < 8u; i++) events_block(CTL);
        ok = !gates() && mo_w == mo && !memcmp(before, TSEL->step, sizeof before) && song.grid == 2u;
        key_up(white(3)); key_up(nm_black_key(NB_SPACE, 0)); frame();
        bad += check("  keys in NAME: no voice, no MIDI, nothing recorded (seq.c: song.grid 2)", ok && mo_w == mo);
    }
    song.rec = 0; usb.config = 0;
    {   /* LEDs: all 27 keys do something; the cycling key blinks */
        uint32_t a, c, t0;
        nm_tap(nm_black_key(NB_DEL, 0)); nm_tap(nm_black_key(NB_DEL, 0));
        nm_tap(white(5));
        t0 = (fm1_ms / 500u + 1u) * 500u;
        fm1_ms = t0; a = name_leds(); fm1_ms = t0 + 250u; c = name_leds();
        nm.t = fm1_ms;                            /* (the letter still cycling) */
        bad += check("  LEDs: every key lit, the cycling key (LM) blinks", ((a | c) == 0x7FFFFFFu) && ((a ^ c) == (1u << white(5))));
    }
    /* cancel */
    {
        uint32_t page = ui.page;
        press(B_OCTDN);
        bad += check("  OCT- cancels: nothing written, back on the USER page", !name_on() && !up_used(2) && ui.page == page && !ui.home);
    }
    /* typing a name and saving it */
    ui.act = 4;
    press(B_OCTUP);
    for (i = 0; i < 12u; i++) nm_tap(nm_black_key(NB_DEL, 0));
    nm_tap(white(1)); nm_tap(white(0)); nm_tap(white(0)); nm_tap(nm_black_key(NB_RIGHT, 0));    /* C B */
    nm_tap(nm_black_key(NB_SPACE, 0)); nm_tap(nm_black_key(NB_MODE, 0)); nm_tap(white(1));        /* " 2" */
    nm_tap(nm_black_key(NB_SPACE, 0));                                                           /* a trailing space */
    song.playing = 1;
    press(B_OCTUP);
    bad += check("  OCT+ while playing: STOP TO SAVE, NAME stays", name_on() && msg_is("STOP TO SAVE") && !up_used(2));
    stop_transport();
    press(B_OCTUP);
    up_name(2, b);
    bad += check("  OCT+ stopped: saved as typed, the trailing space dropped (CB 2)", !name_on() && up_used(2) && str_eq(b, "CB 2") &&
                 msg_is("SAVED (RAM)"));
    /* SAVE held does nothing in NAME; the FX layer neither */
    ui.act = 4; press(B_OCTUP); press(B_OCTUP);   /* (U03 used: OVERWRITE?, YES) */
    bad += check("  overwrite: the dialog, then NAME", name_on() && nm.kind == NK_USER_SAVE);
    ui.msg_t = 0;
    hold(B_SAVE);
    btn_down(B_FX); frames(600); ok = !ui.layer; btn_up(B_FX); frame();
    bad += check("  SAVE held, FX held: nothing in NAME (no undo, no map, no page)", name_on() && ok && cur_page()->graph == GR_USER &&
                 !ui.msg_t);
    press(B_OCTDN);
    /* rename: EDIT on USER */
    ui.uslot = 5;
    press(B_EDIT);
    bad += check("  EDIT on an empty slot: EMPTY SLOT", !name_on() && msg_is("EMPTY SLOT") && cur_page()->graph == GR_USER);
    ui.uslot = 2;
    up_load(2);                                   /* (the sound now comes from U03) */
    trk[0].p[P_E0] = 33;                          /* an edit: a rename must not store it */
    press(B_EDIT);
    bad += check("  EDIT on a used slot: NAME (rename) with its name", name_on() && nm.kind == NK_USER_RENAME && str_eq(nm.s, "CB 2"));
    nm_tap(nm_black_key(NB_DEL, 0));
    nm_tap(white(10)); nm_tap(white(10));          /* VW: W */
    press(B_OCTUP);
    up_name(2, b);
    bad += check("  OCT+: renamed (CB W), the sound stored there unchanged", !name_on() && str_eq(b, "CB W") &&
                 up_value(up_rec(2), P_E0) != 33 && msg_is("RENAMED (RAM)"));
    press(B_EDIT);
    for (i = 0; i < 12u; i++) nm_tap(nm_black_key(NB_DEL, 0));
    press(B_OCTUP);
    {
        char au[16];
        up_name(2, b);
        up_auto_name(au, up_rec(2)->engine, 2);
        bad += check("  a name deleted to nothing: the automatic one", str_eq(b, au));
    }
    /* the sound's own name is the prefill */
    trk[0].user = 3;
    ui.uslot = 7; ui.act = 4;
    press(B_OCTUP);
    up_name(2, b);
    bad += check("  SAVE of a sound that came from U03: prefilled with U03's name", name_on() && str_eq(nm.s, b));
    press(B_OCTDN);
    /* projects */
    go_page(GR_SLOTS);
    song.g[G_SLOT] = 2;
    turn(EN_K4, 1);
    press(B_OCTUP);
    for (i = 0; i < 4u; i++) { nm_tap(white(7)); nm_tap(white(6)); frames(800); }   /* P N P N .. */
    press(B_OCTUP);
    {
        char pn[16];
        int named = project_name(1, pn);
        bad += check("PROJECT SAVE: NAME, typed, saved: the slot has the name, the project too", named && str_eq(pn, "PNPNPNPN") &&
                     str_eq(proj_name, "PNPNPNPN"));
    }
    proj_name[0] = 0;
    project_load(1);
    bad += check("  loading it brings its name back (the next save's prefill)", str_eq(proj_name, "PNPNPNPN"));
    trk[0].p[P_LEVEL] = 7;
    press(B_EDIT);
    for (i = 0; i < 4u; i++) nm_tap(nm_black_key(NB_DEL, 1));
    press(B_OCTUP);
    {
        char pn[16];
        project_name(1, pn);
        bad += check("  EDIT renames a project (PNPN); its music unchanged", str_eq(pn, "PNPN") && stored_param(1, 0, P_LEVEL) != 7 &&
                     msg_is("RENAMED (RAM)"));
    }
    song.g[G_SLOT] = 4;
    press(B_EDIT);
    bad += check("  EDIT on an empty project slot: EMPTY SLOT", !name_on() && msg_is("EMPTY SLOT"));
    song.g[G_SLOT] = 2;
    press(B_EDIT);
    hold(B_HOME);
    bad += check("  HOME held in NAME: the menu, NAME closed", ui.menu && !name_on());
    hold(B_HOME);
    return bad;
}

/* the EDIT cycle (no ENGINE page: engines are the EDIT layer's) and its memory */
static int engine_cycle(const char *const *want, uint32_t n)   /* EDIT tapped from HOME, then n - 1 times more */
{
    uint32_t i;
    int ok = 1;
    frames(320);
    go_home(); frame();
    for (i = 0; i < n; i++) {
        press(B_EDIT);
        frames(320);                                  /* (taps further apart than a double tap: #83 locks) */
        ok &= str_eq(cur_page()->title, want[i]);
    }
    return ok;
}
static int test_edit_cycle(void)
{
    static const char *const CYC_A[] = {"EDIT 1", "EDIT 2", "VOICE", "VOICE 2", "EDIT 1"};
    static const char *const CYC_D[] = {"EDIT 1", "EDIT 2", "OP1 ENV", "OP2 ENV", "OP3 ENV", "OP4 ENV",
                                        "OP LEVEL", "VOICE", "VOICE 2", "EDIT 1"};
    int bad = 0, ok;
    uint32_t i;
    ui_power_on();
    btn_down(B_EDIT); frame();
    ok = ui.home;
    btn_up(B_EDIT); frame();
    bad += check("EDIT opens EDIT 1, when let go (not on press)", ok && str_eq(cur_page()->title, "EDIT 1"));
    for (i = 0; i < NPAGES; i++)
        ok &= !str_eq(PAGES[i].title, "ENGINE");
    bad += check("no ENGINE page (engines are the EDIT layer's)", ok);
    set_engine_of(TSEL, 0);
    bad += check("EDIT cycle (ANALOG): EDIT 1 EDIT 2 VOICE VOICE 2 EDIT 1", engine_cycle(CYC_A, NELEM(CYC_A)));
#if FELUCCA_FM4
    set_engine_of(TSEL, 1);
    bad += check("EDIT cycle (DIGITAL): EDIT 1 EDIT 2 OP1..OP4 ENV OP LEVEL VOICE VOICE 2 EDIT 1",
                 engine_cycle(CYC_D, NELEM(CYC_D)));
#else
    set_engine_of(TSEL, ENGI_DIGITAL);
    bad += check("EDIT cycle (engine 1 asked for: FM6, DIGITAL retired): EDIT 1 EDIT 2 VOICE VOICE 2 EDIT 1",
                 TSEL->eng_req == ENGI_FM6 && engine_cycle(CYC_A, NELEM(CYC_A)));
    (void)CYC_D;
#endif
    go_title("OP2 ENV"); ui.fam_last[FAM_EDIT] = ui.page;
    set_engine_of(TSEL, 0); frame();
    bad += check("an OP page of a track no longer DIGITAL falls back to EDIT 1", str_eq(cur_page()->title, "EDIT 1"));
    go_title("EDIT 2"); ui.fam_last[FAM_EDIT] = ui.page;
    go_home(); press(B_EDIT);
    bad += check("EDIT remembers its last page (as every family)", str_eq(cur_page()->title, "EDIT 2"));
    return bad;
}

#if FELUCCA_SLICE
/* SLICES (EDIT family, a SLICE track: ui_slice.c; the MAN slices of eng_slice.c, ported from hugelton/Felucca#27 by
 * andreahaku): in the EDIT cycle after EDIT 2 on SLICE only; BREAK shows its slices, edits need a user slot; the first
 * edit takes the slices shown (8 equal) as MAN and sets DIV MAN; KNOB 1 the marker (then END), KNOB 2 moves it,
 * KNOB 3 / 4 pick SPLIT / JOIN and OCT+ does it (it stays picked), OCT- drops the pick, then goes HOME; a key picks
 * its slice and the keys of the selected slice are lit; the slot is marked for the store; another engine: EDIT 1 */
static void host_slot_make(uint32_t k)                /* USR k + 1: 2 s at 22.05 kHz, a noise burst every 0.25 s */
{
    smp_user_hdr_t *h = (smp_user_hdr_t *)((uint8_t *)host_slots + k * SMP_USER_SIZE);
    uint8_t *d = (uint8_t *)h + SMP_USER_DATA;
    uint32_t n = 44100u, i, r = 12345u;
    memset(h, 0, SMP_USER_SIZE);
    h->magic = SMP_USER_MAGIC;
    h->version = 1;
    h->nz = 1;
    memcpy(h->name, "TEST", 4);
    h->data_len = n / 2u;
    for (i = 0; i < n / 2u; i++) {                   /* (IMA codes: a burst of random ones, then +-1/8 steps) */
        r = r * 1103515245u + 12345u;
        d[i] = i % 2756u < 400u ? (uint8_t)(r >> 16) : 0x80u;
    }
    h->zone[0].n = n;
    h->zone[0].le = n - 1u;
    h->zone[0].rate = 32768u;                        /* 22050 / 44100, Q16 */
    h->zone[0].root16 = 60 * 16;
    h->zone[0].hi = 127;
}
static int test_slices(void)
{
    static const char *const CYC_S[] = {"EDIT 1", "EDIT 2", "SLICES", "VOICE", "VOICE 2", "EDIT 1"};
    int bad = 0, ok;
    uint32_t n, j, p0, k, note;
    ui_power_on();
    set_engine_of(TSEL, 13u);                        /* SLICE CHOP: BREAK, 16 */
    bad += check("SLICES: SLICE's EDIT cycle EDIT 1 EDIT 2 SLICES VOICE VOICE 2 EDIT 1", engine_cycle(CYC_S, NELEM(CYC_S)));
    go_page(GR_SLICES); frame();
    n = slice_count();
    p0 = TSEL->p[P_E1];
    turn(EN_K1, 2); turn(EN_K2, 3);
    ok = n == 16u && slice_sel() == 2u && TSEL->p[P_E1] == p0 && msg_is("SRC USR1-3 TO EDIT") && !slice_act_ready(2);
    bad += check("SLICES on BREAK: its 16 slices shown, KNOB 1 picks; edits need USR1-3 (DIV unchanged)", ok);
    TSEL->p[P_E0] = 2;                               /* SRC USR2, empty: BREAK's slices, the slot named */
    frame();
    turn(EN_K2, 1);
    ok = slice_count() == 16u && TSEL->p[P_E1] == p0 && msg_is("USR2 EMPTY");
    TSEL->p[P_E0] = 0;
    bad += check("SLICES on an empty USR2: \"USR2 EMPTY\" (DIV unchanged)", ok);

    host_slot_make(0);
    smp_user_scan(0);
    TSEL->p[P_E0] = 1;                               /* SRC USR1, DIV 8 */
    TSEL->p[P_E1] = 1;
    frame();
    n = slice_count();
    p0 = slice_mark(2);
    turn(EN_K2, 5);
    ok = n == 8u && TSEL->p[P_E1] == SLC_DIV_MAN && msg_is("DIV MAN") && slice_count() == 8u && slice_sel() == 2u &&
         slice_mark(2) > p0 && slc_man_of(slc_get(1)) && (slc_man_save & 1u);
    bad += check("SLICES on USR1: the first KNOB 2 turn takes the 8 slices as MAN (DIV MAN), moves slice 3's start", ok);
    turn(EN_K3, 1);
    ok = ui.act == 3u && act_ready();
    press(B_OCTUP);
    ok &= slice_count() == 9u && slice_sel() == 3u && ui.act == 3u;
    press(B_OCTUP);
    ok &= slice_count() == 10u && slice_sel() == 4u;
    bad += check("SLICES: KNOB 3 picks SPLIT, OCT+ splits the slice (again: it stays picked)", ok);
    turn(EN_K4, 1);
    ok = ui.act == 4u && act_ready();
    press(B_OCTUP);
    ok &= slice_count() == 9u && slice_sel() == 3u;
    turn(EN_K1, -20);
    ok &= slice_sel() == 0u && !act_ready();
    press(B_OCTUP);
    ok &= slice_count() == 9u && msg_is("FIRST SLICE");
    bad += check("SLICES: KNOB 4 picks JOIN, OCT+ joins to the slice before; not the first slice", ok);
    turn(EN_K1, 20);
    p0 = slice_mark(slice_count());
    turn(EN_K2, -4);
    ok = slice_sel() == slice_count() && slice_mark(slice_count()) < p0 && !act_ready();
    bad += check("SLICES: after the last slice the END marker: KNOB 2 trims the tail", ok);
    {   /* DIV set back to a grid: the first touch shows the slot's MAN slices (DIV MAN), it does not replace them */
        slc_man_t keep = *slc_man_of(slc_get(1));
        TSEL->p[P_E1] = 2;                           /* DIV 16 */
        frame();
        turn(EN_K1, -1); turn(EN_K2, 1);
        ok = TSEL->p[P_E1] == SLC_DIV_MAN && msg_is("DIV MAN") && slc_man_of(slc_get(1))->n == keep.n &&
             !memcmp(slc_man_of(slc_get(1))->pos, keep.pos, keep.n * sizeof keep.pos[0]);
        turn(EN_K1, -20); turn(EN_K2, 1);            /* then KNOB 2 edits that table */
        ok &= slc_man_of(slc_get(1))->n == keep.n && memcmp(slc_man_of(slc_get(1))->pos, keep.pos, keep.n * sizeof keep.pos[0]);
        bad += check("SLICES: DIV 16 over a slot's MAN slices: KNOB 2 goes back to them (DIV MAN), keeps them", ok);
    }

    note = 5u;                                       /* (the 6th key: SLICE maps every key) */
    key_down(note); frame(); key_up(note); frame();
    j = slc_note_slice(TSEL->p, kb_map(TSEL, note), slice_count());
    ok = slice_sel() == j && ((slice_leds() >> note) & 1u);
    for (k = 0; k < 27u; k++)
        ok &= ((slice_leds() >> k) & 1u) == (slc_note_slice(TSEL->p, kb_map(TSEL, k), slice_count()) == j);
    bad += check("SLICES: a key picks the slice it plays; the keys of the selected slice are lit", ok);
    btn_down(B_OCTDN); frame(); btn_up(B_OCTDN); frame();
    ok = ui.act == 0u && !ui.home;
    btn_down(B_OCTDN); frame(); btn_up(B_OCTDN); frame();
    ok &= ui.home;
    bad += check("SLICES: OCT- drops the pick, then goes HOME", ok);
    go_page(GR_SLICES); frame();
    set_engine_of(TSEL, 0); frame();
    bad += check("SLICES: on another engine the page is not there (EDIT 1)", str_eq(cur_page()->title, "EDIT 1"));
    {   /* the engine changed (the editor's SET between two ui_input passes) while the page is still the current one:
         * its knobs and actions do nothing to the track or the slot */
        track_t before;
        slc_man_t keep;
        set_engine_of(TSEL, 13u);
        TSEL->p[P_E0] = 1;
        TSEL->p[P_E1] = SLC_DIV_MAN;
        go_page(GR_SLICES); frame();
        keep = *slc_man_of(slc_get(1));
        set_engine(3u);                              /* LOFI, as ed_service's SET G_ENGSEL */
        TSEL->p[P_E0] = 1;
        before = *TSEL;
        slc_man_save = 0;
        host_enc[panel.enc[EN_K2]] += 2 * panel.dir[EN_K2];
        host_ticks += 16000u; fm1_ms += 16u;
        ui_input();                                  /* (no ui_draw yet) */
        ok = cur_page()->graph == GR_SLICES && !slice_page_ok() && !act_cols();
        ui.act = 3u;
        act_do();
        ui.act = 0;
        ok &= !memcmp(&before, TSEL, sizeof before) && !slc_man_save && slc_man_of(slc_get(1))->n == keep.n &&
              !memcmp(slc_man_of(slc_get(1))->pos, keep.pos, keep.n * sizeof keep.pos[0]);
        bad += check("SLICES: left by an engine change not yet drawn: KNOB 2 and OCT+ change nothing", ok);
        frame();
    }
    {   /* another sample of the same length uploaded into the slot: the waveform is decoded again */
        int8_t lo0[SP_COLS], hi0[SP_COLS];
        uint8_t *d = (uint8_t *)host_slots + SMP_USER_DATA;
        smp_user_hdr_t *h = (smp_user_hdr_t *)host_slots;
        set_engine_of(TSEL, 13u);
        TSEL->p[P_E0] = 1;
        TSEL->p[P_E1] = 2;
        go_page(GR_SLICES); ui.force = 1; frame();
        memcpy(lo0, sp.lo, sizeof lo0);
        memcpy(hi0, sp.hi, sizeof hi0);
        for (k = 0; k < h->data_len; k++)
            d[k] = 0x80u;                            /* silence, the same length */
        h->crc ^= 1u;
        smp_user_scan(0);                            /* (the upload's END) */
        ui.force = 1; frame();
        ok = memcmp(lo0, sp.lo, sizeof lo0) || memcmp(hi0, sp.hi, sizeof hi0);
        for (k = 0; k < SP_COLS; k++)
            ok &= sp.lo[k] >= -1 && sp.hi[k] <= 1;   /* (near silence) */
        bad += check("SLICES: a re-upload of the same length redraws the waveform", ok);
        set_engine_of(TSEL, 0); frame();
    }
    memset(host_slots, 0, sizeof host_slots);        /* (the other tests: empty slots) */
    smp_user_scan(0);
    slc_man_save = 0;
    return bad;
}
#endif

/* ------------------------------------------------ the GLO SCL EDIT layers --- */
static uint32_t black(uint32_t b) { return key_at(1, b); }
static void lay_combo(uint32_t btn, uint32_t k) { btn_down(btn); key_down(k); frame(); }
static void oct_back(void) { btn_down(B_OCTDN); frame(); btn_up(B_OCTDN); frame(); }
static uint32_t leds_at(uint32_t ms) { fm1_ms = ms; return layer_leds(); }

static int test_quick_layers(void)
{
    static const uint8_t LB[3] = {B_GLO, B_SCL, B_EDIT};
    static const uint8_t LL[3] = {LAYER_GLO, LAYER_SCL, LAYER_EDIT};
    static const char *const HINT[3] = {"HOLD [GLO] QUICK", "HOLD [SCL] QUICK", "HOLD [EDIT] QUICK"};
    int bad = 0, ok, flash, hint;
    uint32_t i, k, a, b2, mo;
    track_t before;
    /* tap / peek / combo, the hint once */
    ok = 1; hint = 1;
    for (i = 0; i < 3u; i++) {
        ui_power_on();
        btn_down(LB[i]);
        for (flash = 0, k = 0; k < 20u; k++) { frame(); flash |= ui.layer; }      /* 0.32 s */
        btn_up(LB[i]); frame();
        ok &= !flash && !ui.home && (LB[i] == B_GLO ? cur_page()->graph == GR_TRK :
                                     LB[i] == B_SCL ? cur_page()->fam == FAM_SCL : str_eq(cur_page()->title, "EDIT 1"));
        hint &= msg_is(HINT[i]);
        go_home(); frame();
        btn_down(LB[i]); frames(368);
        ok &= !ui.layer;
        frames(64);
        ok &= ui.layer == LL[i] && ((layer_seen >> LL[i]) & 1u);
        btn_up(LB[i]); frame();
        ok &= ui.home && !ui.layer;
        ui.msg_t = 0;
        press(LB[i]);
        hint &= !msg_is(HINT[i]);
        go_home(); frame();
        btn_down(LB[i]); key_down(white(9)); frame();
        ok &= ui.layer == LL[i] && !gates();
        key_up(white(9)); btn_up(LB[i]); frame();
        ok &= ui.home && !ui.layer;
    }
    bad += check("GLO SCL EDIT: a tap opens the page, held past HOLD the map (no page), a key: the map at once", ok);
    bad += check("  after a tap \"HOLD [BTN] QUICK\" until the layer has been opened once", hint);
    {
        persist_t p = {0};
        p.magic = PERSIST_MAGIC; p.panel = panel;
        layer_seen = 0x1Cu;
        settings_export(&p); layer_seen = 0;
        bad += check("  the seen bits are kept with the settings (a spare favorites byte)",
                     settings_import(&p, sizeof p) && layer_seen == 0x1Cu);
    }

    /* GLO: mutes latch (lit = sounding), SOLO while held, UNMUTE ALL, TAP, levels, OCT- */
    ui_power_on();
    usb.config = 1; mo = mo_w;
    lay_combo(B_GLO, black(1));
    ok = trk[1].p[P_MUTE] == 1 && ui.layer == LAYER_GLO && !gates() && mo_w == mo;
    a = leds_at(0); b2 = leds_at(250);
    ok &= ((a & b2) >> black(0)) & 1u && !(((a | b2) >> black(1)) & 1u);           /* T1 sounding lit, T2 muted dark */
    ok &= ((a ^ b2) >> white(0)) & 1u && ((a ^ b2) >> white(7)) & 1u && !(((a | b2) >> white(5)) & 1u);
    key_up(black(1)); btn_up(B_GLO); frame();
    bad += check("GLO + black key 2: T2 MUTE latched (SET), silent, no MIDI; LEDs: sounding lit, muted dark", ok &&
                 trk[1].p[P_MUTE] == 1 && !ui.layer);
    lay_combo(B_GLO, black(1)); key_up(black(1)); frame();
    key_down(white(4)); key_up(white(4)); frame();
    lay_combo(B_GLO, black(2)); key_up(black(2)); frame();
    ok = trk[1].p[P_MUTE] == 0 && trk[2].p[P_MUTE] == 1;
    key_down(white(4)); frame(); key_up(white(4)); frame();
    btn_up(B_GLO); frame();
    bad += check("  again: unmuted; C4: UNMUTE ALL", ok && !trk[2].p[P_MUTE] && !trk[1].p[P_MUTE]);
    lay_combo(B_GLO, white(2));
    perf_begin(CTL);
    ok = perf_solo == 4u && ((perf_act >> PF_M1) & 15u) == 0xBu;   /* T3 solo: T1 T2 T4 muted */
    btn_up(B_GLO); frame();
    perf_begin(CTL);
    ok &= perf_solo == 4u && ui.layer == LAYER_GLO;                 /* GLO let go first: the solo stays with its key */
    key_up(white(2)); frame();
    perf_begin(CTL);
    bad += check("GLO + A3 held: SOLO T3 (the others muted), lasts while the key is held, not P_MUTE",
                 ok && !perf_solo && !((perf_act >> PF_M1) & 15u) && !trk[0].p[P_MUTE] && !ui.layer);
    song.g[G_BPM] = 100;
    btn_down(B_GLO); frame();
    for (i = 0; i < 3u; i++) { key_down(white(7)); frame(); key_up(white(7)); frames(480); }
    ok = song.g[G_BPM] == 120;
    btn_up(B_GLO); frame();
    song.g[G_CLOCK] = 1;
    lay_combo(B_GLO, white(7)); key_up(white(7)); frame();
    ok &= msg_is("TAP: CLK IS EXT") && song.g[G_BPM] == 120;
    btn_up(B_GLO); frame(); song.g[G_CLOCK] = 0;
    bad += check("GLO + F4 x3 at 0.5 s: TAP 120 BPM; with CLK EXT: dimmed, it says why", ok);
    {
        int16_t l2 = trk[2].p[P_LEVEL], atk = TSEL->p[P_ATK];
        go_title("ENV");
        btn_down(B_GLO); frame();
        turn(EN_K3, -10);
        ok = trk[2].p[P_LEVEL] == l2 - 10 && TSEL->p[P_ATK] == atk && ui.layer == LAYER_GLO;
        key_down(black(0)); frame(); key_up(black(0)); frame();
        ok &= trk[0].p[P_MUTE] == 1 && song.octave == 0;
        oct_back();
        ok &= trk[2].p[P_LEVEL] == l2 && !trk[0].p[P_MUTE] && song.octave == 0 && ui.layer == LAYER_GLO;
        btn_up(B_GLO); frame();
        bad += check("GLO KNOB 3: T3 LEVEL from any page; OCT-: mutes and levels as it opened, no octave", ok &&
                     str_eq(cur_page()->title, "ENV"));
    }
    song.playing = 1; transport_req = 0;
    btn_down(B_GLO); frame(); press(B_PLAY);
    ok = transport_req == 3u;
    events_block(CTL);
    ok &= song.playing && transport_req == 0u;
    btn_up(B_GLO); frame();
    stop_transport();
    btn_down(B_GLO); frame(); press(B_PLAY);
    ok &= transport_req == 1u;
    btn_up(B_GLO); frame(); transport_req = 0;
    bad += check("GLO + PLAY: RESTART playing (from the top, not stopped); stopped: PLAY", ok);
    usb.config = 0;

    /* SCL: a key is ROOT, KNOB 2 the scale, the LEDs, OCT- */
    ui_power_on();
    TSEL->p[P_ROOT] = 0; TSEL->p[P_SCALE] = 1;                     /* C MAJ */
    lay_combo(B_SCL, black(4));                                    /* D#4 */
    ok = TSEL->p[P_ROOT] == 3 && ui.layer == LAYER_SCL && !gates();
    key_up(black(4)); frame();
    key_down(white(1)); key_up(white(1)); frame();                  /* G3 */
    ok &= TSEL->p[P_ROOT] == 7;
    turn(EN_K2, 1);
    ok &= TSEL->p[P_SCALE] == 2;
    a = leds_at(0); b2 = leds_at(250);
    ok &= ((a & b2) >> white(1)) & 1u && ((a & b2) >> white(8)) & 1u;   /* G3 G4: the root, lit */
    ok &= ((a ^ b2) >> white(2)) & 1u && ((a ^ b2) >> black(4)) & 1u;   /* A, A# (G minor): blink */
    ok &= !(((a | b2) >> black(0)) & 1u);                               /* F#: not in G minor, dark */
    btn_up(B_SCL); frame();
    bad += check("SCL + D#4, then G3: ROOT D#, G (latched); KNOB 2: SCL; LEDs: root lit, the scale blinks", ok &&
                 TSEL->p[P_ROOT] == 7 && TSEL->p[P_SCALE] == 2 && ui.home);
    btn_down(B_SCL); frame(); key_down(white(4)); key_up(white(4)); frame(); turn(EN_K2, 3);
    ok = TSEL->p[P_ROOT] == 0 && TSEL->p[P_SCALE] == 5;
    oct_back();
    btn_up(B_SCL); frame();
    bad += check("  OCT- in SCL: ROOT and SCL as the layer opened", ok && TSEL->p[P_ROOT] == 7 && TSEL->p[P_SCALE] == 2);

    /* EDIT: the engines from F3 (NENGINES of them), sound loads with UNDO, INIT with the dialog */
    ui_power_on();
    set_engine_of(TSEL, 0); go_home(); frame();
    my_steps(TSEL); TSEL->p[P_SLCR] = SL_STUT; song.playing = 1;
    before = *TSEL;
    sync_reload = 0;
    lay_combo(B_EDIT, white(eng_rank(2)));             /* (the keys: the engines one can pick, engines.c eng_vis) */
    ok = TSEL->eng_req == 2u && TSEL->preset == 0u && ui.layer == LAYER_EDIT && sync_reload && !gates();
    key_up(white(eng_rank(2))); frame();
    key_down(white(1)); key_up(white(1)); frame();
    ok &= TSEL->eng_req == ENGI_FM6;                    /* (G3: FM6, second in ENGINE_ORDER) */
    key_down(white(NENG_SHOWN - 1u)); key_up(white(NENG_SHOWN - 1u)); frame();
    ok &= TSEL->eng_req == ENGI_DRUM;                   /* (the last key: DRUM) */
    ok &= !memcmp(TSEL->step, before.step, sizeof before.step) && TSEL->p[P_SLEN] == before.p[P_SLEN] &&
          TSEL->p[P_SLCR] == SL_STUT;
    btn_up(B_EDIT); frame();
    bad += check("EDIT + white key n: the n-th engine shown (FM6 2nd, DRUM last), while playing; steps, LEN, SLICER stay", ok);
    hold(B_SAVE);
    bad += check("  SAVE held: UNDO back to before the layer's loads, the steps untouched",
                 TSEL->eng_req == 0u && TSEL->preset == before.preset && !memcmp(TSEL->step, before.step, sizeof before.step));
    a = 0;
    ok = 1;
    btn_down(B_EDIT); frames(480);
    for (i = 0; i < LY_INIT; i++) {
        key_down(white(i)); key_up(white(i)); frame();
        a += i < NENG_SHOWN ? TSEL->eng_req == eng_vis(i) : 0u;
        ok &= eng_ok(TSEL->eng_req);
    }
    btn_up(B_EDIT); frame();
    bad += check("  every engine one can pick has its white key from F3 (NENG_SHOWN, not a fixed count; never DIGITAL)",
                 ok && a == NENG_SHOWN);
    set_engine_of(TSEL, 0);
    lay_combo(B_EDIT, white(eng_rank(3))); key_up(white(eng_rank(3))); frame();
    turn(EN_K2, 1);
    ok = TSEL->eng_req == 3u && TSEL->preset == 1u;
    turn(EN_K3, 1);
    ok &= preset_favorite();
    oct_back();
    ok &= TSEL->eng_req == 0u && !memcmp(TSEL->step, before.step, sizeof before.step);
    btn_up(B_EDIT); frame();
    bad += check("  KNOB 2: the engine's next sound, KNOB 3 FAV; OCT-: the sound as the layer opened", ok && ui.home);
    btn_down(B_EDIT); frame(); turn(EN_K1, 1);
    ok = TSEL->eng_req == eng_step(0, 1);
    btn_up(B_EDIT); frame();
    bad += check("  KNOB 1: the next engine", ok);
    song.playing = 0;
    lay_combo(B_EDIT, white(LY_INIT));
    ok = ui.confirm == CF_INIT_SOUND;
    frame();
    ok &= !ui.layer;
    key_up(white(LY_INIT)); btn_up(B_EDIT); frame();
    ok &= ui.home && ui.confirm == CF_INIT_SOUND;
    TSEL->p[P_E0] = (int16_t)(TSEL->p[P_E0] + 5);
    press(B_OCTUP);
    bad += check("EDIT + the key after the engines: INITIALIZE SOUND? dialog (closes the layer, no tap); OCT+ inits", ok && !ui.confirm &&
                 msg_is("SOUND INIT") && TSEL->p[P_E0] == ENGINES[eng_step(0, 1)]->presets[0].e[0]);

    /* no layer in the menu or a dialog: GLO + a key plays */
    ui_power_on(); hold(B_HOME);
    btn_down(B_GLO); frame(); key_down(white(3)); frame();
    ok = !ui.layer && gates() > 0 && !kb_layer;
    key_up(white(3)); btn_up(B_GLO); frame();
    bad += check("in the menu: no GLO layer, GLO + a key plays it; GLO let go: no page", ok && ui.menu == 1);
    ui_power_on(); ui.confirm = CF_CLEAR_SEQ;
    btn_down(B_SCL); frames(512);
    ok = !ui.layer;
    btn_up(B_SCL); frame();
    bad += check("in a dialog: SCL held shows no map and taps nothing", ok && ui.confirm == CF_CLEAR_SEQ);
    /* one layer at a time: a second layer button is ignored, its keys stay notes after */
    ui_power_on();
    btn_down(B_GLO); frame(); btn_down(B_EDIT); frame(); key_down(white(1)); frame();
    ok = ui.layer == LAYER_GLO && TSEL->eng_req == trk[0].eng_req && perf_solo == 2u;
    key_up(white(1)); btn_up(B_GLO); frame();
    key_down(white(1)); frame();
    ok &= gates() > 0 && !ui.layer;
    key_up(white(1)); btn_up(B_EDIT); frame();
    bad += check("GLO then EDIT: GLO's layer; EDIT ignored (no layer, no page, its keys notes)", ok && ui.home);
    return bad;
}

/* bug fixes (UI): one regression each */
static int test_bughunt_ui(void)
{
    int bad = 0, ok;
    /* 1: REC in NAME, the menu or a dialog: nothing (PLAY cannot stop the transport there) */
    ui_power_on();
    go_page(GR_USER); ui.uslot = 3;
    press(B_OCTUP);                                     /* NAME opens */
    press(B_REC);
    bad += check("REC in NAME does nothing (no arm, no transport start); NAME stays",
                 name_on() && !song.rec && !transport_req && !song.playing);
    ui_power_on(); hold(B_HOME); press(B_REC); press(B_PLAY);
    bad += check("REC in the menu does nothing (then PLAY: nothing either)", ui.menu && !song.rec && !transport_req);
    ui_power_on();
    up_ui(2, 5);
    go_page(GR_USER); ui.uslot = 5; press(B_OCTUP);
    press(B_REC);
    ok = ui.confirm == CF_OVR_USER && !song.rec && !transport_req;
    press(B_OCTUP);
    bad += check("REC in the OVERWRITE? dialog does nothing; OCT+ then opens NAME", ok && name_on());
    {   /* 2: UNDO of a sound load on an FM6 track: the track's own (edited) patch, not PTCH's factory one */
        uint8_t mine[FP_SIZE + 1u], next[FP_SIZE + 1u];
        ui_power_on();
        track_select(1); frame();
        undo_depth++; set_engine_of(TSEL, ENGI_FM6); undo_depth--;   /* (no undo copy of this) */
        frame();
        memcpy(mine, fm6_patch[1], sizeof mine);
        mine[0] ^= 0x15; mine[5] ^= 0x22; mine[40] ^= 0x07;   /* an edited patch (the editor, a project) */
        fm6_set_patch(1, mine);
        memcpy(mine, fm6_patch[1], FP_SIZE);
        turn(EN_PRESET, 1);                             /* browse one sound */
        memcpy(next, fm6_patch[1], FP_SIZE);
        hold(B_SAVE);                                   /* UNDO */
        frame();
        ok = TSEL->eng_req == ENGI_FM6 && !memcmp(mine, fm6_patch[1], FP_SIZE);
        hold(B_SAVE);                                   /* REDO */
        frame();
        bad += check("UNDO of a sound load on FM6: the track's edited patch back; REDO: the load's",
                     ok && memcmp(mine, next, FP_SIZE) && !memcmp(next, fm6_patch[1], FP_SIZE));
        hold(B_SAVE); frame();                          /* (the edited patch again) */
        btn_down(B_EDIT); frames(500);                  /* EDIT layer: another engine, then OCT- */
        turn(EN_K1, 1);
        ok = ui.layer == LAYER_EDIT && TSEL->eng_req != ENGI_FM6;
        oct_back();
        btn_up(B_EDIT); frame();
        bad += check("  the EDIT layer's OCT- after an engine pick: FM6 with the track's edited patch",
                     ok && TSEL->eng_req == ENGI_FM6 && !memcmp(mine, fm6_patch[1], FP_SIZE));
    }
    {   /* 2b: a patch the editor sends between two preset loads is a new starting point: UNDO brings it back */
        uint8_t ed[FP_SIZE + 1u];
        ui_power_on();
        track_select(1); frame();
        undo_depth++; set_engine_of(TSEL, ENGI_FM6); undo_depth--;
        frame();
        turn(EN_PRESET, 1); frame();                    /* load 1 */
        memcpy(ed, fm6_patch[1], sizeof ed);
        ed[0] ^= 0x15; ed[40] ^= 0x07;
        fm6_set_patch(1, ed);                           /* the editor's FM6_PUT */
        memcpy(ed, fm6_patch[1], FP_SIZE);
        turn(EN_PRESET, 1); frame();                    /* load 2 */
        hold(B_SAVE); frame();                          /* UNDO */
        bad += check("UNDO after load, editor patch, load: the editor's patch back",
                     !memcmp(ed, fm6_patch[1], FP_SIZE));
    }
    {   /* the ARP button flashes on the beat while an ARP plays (the bar's first beat longer); dark otherwise */
        uint32_t q, b, r[5];
        ui_power_on(); frame();
        b = beat_samples();
        r[0] = arp_led();                               /* no ARP playing */
        trk[2].p[P_AMODE] = 1; trk[2].nheld = 1; trk[2].held[0] = 60;
        beat_n = 1; beat_pos = 0; r[1] = arp_led();
        beat_pos = b / 3u; r[2] = arp_led();
        beat_n = 0; r[3] = arp_led();
        beat_pos = b * 3u / 4u; r[4] = arp_led();
        bad += check("ARP LED: none without an ARP; flashes on the beat, the bar's first beat longer",
                     r[0] == 2u && r[1] == 1u && r[2] == 0u && r[3] == 1u && r[4] == 0u);
        for (q = 0; q < 4u * b / 128u + 4u; q++) events_block(128);
        bad += check("  the beat counter runs (a bar of blocks wraps the beat of the bar)", beat_n < 4u && beat_pos < b);
        trk[2].p[P_AMODE] = 0; trk[2].nheld = 0;
    }
    /* 3: USER ERASE asks first (ERASE U02?): OCT- keeps the preset, OCT+ erases it */
    ui_power_on();
    go_page(GR_USER); ui.uslot = 1;
    up_ui(2, 1);
    turn(EN_K1 + 2, 1);                                 /* KNOB 3: ERASE picked */
    press(B_OCTUP);
    ok = ui.confirm == CF_ERASE_USER && ui.confirm_trk == 1u && up_used(1);
    {
        char a[24], b[24];
        confirm_text(a, b);
        ok &= str_eq(a, "ERASE U02?") && b[0];
    }
    press(B_OCTDN);
    ok &= !ui.confirm && up_used(1);
    turn(EN_K1 + 2, 1); press(B_OCTUP); press(B_OCTUP);
    bad += check("USER ERASE: the ERASE U02? dialog; OCT- keeps the preset, OCT+ erases it",
                 ok && !ui.confirm && !up_used(1));
    turn(EN_K1 + 2, 1); press(B_OCTUP);
    bad += check("  an empty slot: no dialog, EMPTY SLOT", !ui.confirm && msg_is("EMPTY SLOT"));
    {   /* 4: CHANCE is not STEP: no grid, no key entry, no EDIT clear (its knobs only) */
        uint32_t before;
        step_t s0;
        ui_power_on();
        track_select(3); frame();                       /* T4 DRUM */
        go_page(GR_CHANCE); frame();
        before = lane_steps(TSEL, ui.lane);
        tap_key(white(2));
        bad += check("CHANCE on a DRUM track: no grid (title, LEDs), a white key toggles no hit",
                     drum_track(TSEL) && !grid_on() && !keys_mode() && lane_steps(TSEL, ui.lane) == before);
        ui_power_on(); go_page(GR_CHANCE); frame();
        my_steps(TSEL);
        s0 = TSEL->step[0];
        tap_key(white(5)); frame();
        ok = !memcmp(&s0, &TSEL->step[0], sizeof s0) && ui.cursor == 0u;
        press(B_EDIT);
        ok &= !memcmp(&s0, &TSEL->step[0], sizeof s0) && !msg_is("STEP CLEARED");
        ui_power_on(); go_page(GR_CHANCE); frame();
        s0 = TSEL->step[0];
        turn(EN_K2, -10);                               /* (100 % by default) */
        bad += check("CHANCE on a melodic track: a key writes no step, EDIT clears none; KNOB 2 the chance",
                     ok && step_chance(&TSEL->step[0]) != step_chance(&s0));
        ui_power_on(); go_page(GR_ROLL); frame();
        tap_key(white(5)); frame();
        bad += check("  STEP still: a key writes the cursor step", step_on(&TSEL->step[0]) || TSEL->step[0].n);
    }
    {   /* 5: ALGORITHM does nothing while a layer's button is held (as PRESETS); after it, the track again */
        int16_t r0;
        ui_power_on();
        r0 = trk[0].p[P_ROOT];
        btn_down(B_SCL); frames(500);
        turn(EN_ALGO, 1);
        ok = ui.layer == LAYER_SCL && song.sel == 0u;
        key_down(white(1)); frame(); key_up(white(1)); frame();   /* ROOT on T1 */
        ok &= trk[0].p[P_ROOT] != r0;
        oct_back();
        ok &= trk[0].p[P_ROOT] == r0;
        btn_up(B_SCL); frame();
        btn_down(B_EDIT); turn(EN_ALGO, 1);             /* armed, not open yet: ignored too */
        ok &= song.sel == 0u;
        btn_up(B_EDIT); frame(); go_home(); frame();
        turn(EN_ALGO, 1);
        bad += check("ALGORITHM ignored with a layer's button held (OCT- puts back all); then T2",
                     ok && song.sel == 1u);
    }
    /* 6: TOOLS: each column its own ready check; nothing to do says so (not STOP TO EDIT) */
    ui_power_on();
    go_title("TOOLS"); frame();
    chain_defaults(&chain_config);                      /* no song rows */
    turn(EN_K1 + 2, 1);                                 /* DELETE ROW */
    press(B_OCTUP);
    ok = !ui.confirm && msg_is("NOTHING TO DELETE") && text_w(&AF_S, ui.msg) <= 236 - 106;   /* (fits the header) */
    turn(EN_K1 + 3, 1);                                 /* CLEAR SONG */
    press(B_OCTUP);
    ok &= !ui.confirm && msg_is("NOTHING TO CLEAR");
    track_defaults_steps(TSEL);
    chain_config.count = 1; chain_config.row[0].slot = 0; chain_config.row[0].repeat = 1;
    turn(EN_K1, 1);                                     /* CLEAR PAT on an empty track, the song with a row */
    ok &= !act_ready();
    press(B_OCTUP);
    ok &= !ui.confirm && msg_is("NOTHING TO CLEAR");
    turn(EN_K1 + 3, 1);
    ok &= act_ready();
    press(B_OCTUP);
    bad += check("TOOLS: DELETE ROW / CLEAR SONG / CLEAR PAT with nothing there: NOTHING TO ..; a song: its dialog",
                 ok && ui.confirm == CF_CLEAR_SONG);
    /* 7: REC works in the layers, as PLAY: it arms the track (PLAY starts); the layer stays, no page */
    ok = 1;
    {
        static const uint8_t LB[4] = {B_FX, B_GLO, B_SCL, B_EDIT};
        uint32_t i;
        for (i = 0; i < 4u; i++) {
            ui_power_on();
            btn_down(LB[i]); frame();
            press(B_REC);
            ok &= song.rec == 1u && transport_req == 1u && ui.layer != 0u;
            btn_up(LB[i]); frame();
            ok &= ui.home;
        }
    }
    bad += check("REC in every layer: arms the track and starts PLAY; the layer stays, no page", ok);
    /* 8: the OCT- LED lit in the SET layers (OCT- = UNDO), OCT+ dark; FX (HOLD): the octave as usual */
    ok = 1;
    {
        static const uint8_t LB[3] = {B_GLO, B_SCL, B_EDIT};
        uint32_t i;
        for (i = 0; i < 3u; i++) {
            ui_power_on();
            song.octave = 1;                            /* (outside: OCT+ lit) */
            btn_down(LB[i]); frames(500);
            ok &= layer_set_open() && oct_leds_seen(1) == 1u && oct_leds_seen(0) == 1u;
            btn_up(LB[i]); frame();
            ok &= oct_leds() == 2u;
        }
        ui_power_on();
        btn_down(B_FX); frames(500);
        ok &= ui.layer == LAYER_FX && oct_leds() == 0u;
        btn_up(B_FX); frame();
    }
    bad += check("SET layers: the OCT- LED lit (UNDO), OCT+ dark; after: the octave; FX: the octave", ok);
    /* 9: no "HOLD [EDIT] QUICK" over NAME (EDIT tapped on USER / PROJECT renames) */
    ui_power_on();
    layer_seen = 0;
    up_ui(2, 0);
    go_page(GR_USER); ui.uslot = 0; frame();
    ui.msg_t = 0;
    press(B_EDIT);
    ok = name_on() && !msg_is("HOLD [EDIT] QUICK");
    ui_power_on();
    layer_seen = 0;
    press(B_EDIT);
    bad += check("EDIT tap = RENAME on USER: no layer hint over NAME (elsewhere the hint as before)",
                 ok && msg_is("HOLD [EDIT] QUICK"));
    return bad;
}

/* the chord keys on the device (chord.c): SCL tapped again is the CHORD page, KNOB 1 CHRD, KNOB 2 VOIC, the graph
 * names the last chord (MONO gray); a sound load keeps them; the SCL layer's KNOB 3 / 4 are CHRD / VOIC, OCT-
 * puts them back */
static int test_chord_page(void)
{
    int bad = 0, ok;
    track_t *t;
    ui_power_on();
    t = TSEL;
    t->p[P_VOICE] = V_POLY;
    go_home(); frame();
    press(B_SCL); frames(400);
    ok = str_eq(cur_page()->title, "SCL");
    press(B_SCL); frames(400);
    ok &= str_eq(cur_page()->title, "CHORD") && cur_page()->graph == GR_CHORD && cur_page()->fam == FAM_SCL;
    bad += check("SCL tapped again: the CHORD page (CHRD VOIC, the chord graph)", ok && cur_page()->id[0] == P_CHRD &&
                 cur_page()->id[1] == P_VOIC);
    memset(host_screen, 0, sizeof host_screen); ui.force = 1; ui_draw();
    bad += check("  CHRD OFF: the page says what to do, MONO gray", screen_gray());
    turn(EN_K1, 1); frame();
    turn(EN_K2, 1); frame();
    bad += check("  KNOB 1: CHRD DIA3, KNOB 2: VOIC OPEN", t->p[P_CHRD] == CH_DIA3 && t->p[P_VOIC] == VC_OPEN);
    key_down(7); frame();                                          /* C4 in C (SCALE CHR: the major of C) */
    ok = gates() == 3u && chord_last[song.sel].root == 60;
    memset(host_screen, 0, sizeof host_screen); ui.force = 1; ui_draw();
    ok &= screen_gray();
    key_up(7); frame();
    bad += check("  a key plays the chord, the graph shows it (C), MONO gray", ok && !gates());
    t->p[P_CHRD] = CH_MIN7; t->p[P_VOIC] = VC_BASS;
    apply_preset_to(t, 1);
    bad += check("  a sound load keeps CHRD and VOIC (SCL settings)", t->p[P_CHRD] == CH_MIN7 && t->p[P_VOIC] == VC_BASS);
    set_engine_of(t, ENGI_DRUM); t->engine = t->eng_req; frame();
    memset(host_screen, 0, sizeof host_screen); ui.force = 1; ui_draw();
    bad += check("  a kit (DRUM): the page says so, MONO gray", chord_kit(t) && screen_gray());
    ui_power_on();
    t = TSEL;
    t->p[P_CHRD] = CH_OFF; t->p[P_VOIC] = VC_CLOSE;
    go_home(); frame();
    btn_down(B_SCL); frames(800);
    ok = ui.layer == LAYER_SCL;
    turn(EN_K3, 2); frame();
    turn(EN_K4, 1); frame();
    ok &= t->p[P_CHRD] == CH_DIA7 && t->p[P_VOIC] == VC_OPEN && t->p[P_QUANT] == 0 && t->p[P_TRANS] == 0;
    turn(EN_K1, 2); frame();
    ok &= t->p[P_ROOT] == 2;
    oct_back();
    btn_up(B_SCL); frame();
    bad += check("SCL layer: KNOB 1 ROOT, 3 CHRD, 4 VOIC (QNT TRN untouched); OCT- puts them back", ok &&
                 t->p[P_CHRD] == CH_OFF && t->p[P_VOIC] == VC_CLOSE && t->p[P_ROOT] == 0 && ui.home);
    btn_down(B_SCL); frames(800);
    memset(host_screen, 0, sizeof host_screen); ui.force = 1; ui_draw();
    ok = screen_gray();
    btn_up(B_SCL); frame();
    bad += check("  its map and cards MONO gray", ok);
    return bad;
}

/* the STEP page's piano roll (ui_graph.c graph_roll), read back from the screen: per column the rows whose centre
 * pixel (the bar's middle, x + 6) is a bar colour, against the notes the step sounds (its own; a TIE: the note
 * before's; a REST: none) */
static int pr_is_bar(uint32_t x, uint32_t y)
{
    uint16_t c = swap16(host_screen[y * 240u + x]);
    return c == T_THEME || c == T_ACCENT || c == T_TEXT;
}
static int pr_columns_match(const track_t *t, uint32_t *nbars)
{
    uint32_t i, r, j, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u;
    int ok = 1;
    for (i = 0; i < 16u && base + i < len; i++) {
        uint32_t want = 0, got = 0, s = pr_src(t, base + i, len);
        for (j = 0; s < NSTEP && j < 4u + NLANE; j++) {
            int32_t n = pr_note(&t->step[s], j), rr = (int32_t)proll.lo + PR_ROWS - 1 - n;
            if (n >= 0 && rr >= 0 && rr < PR_ROWS) want |= 1u << rr;
        }
        for (r = 0; r < PR_ROWS; r++)
            if (pr_is_bar((uint32_t)(PR_X0 + (int32_t)i * PR_CW + 6), Y_GRAPH + PR_Y0 + r * PR_RH + 2u)) got |= 1u << r;
        if (got != want) {
            printf("ui:   roll column %u: rows %05x drawn, %05x wanted\n", base + i, got, want);
            ok = 0;
        }
        for (r = 0; r < PR_ROWS; r++) *nbars += (got >> r) & 1u;
    }
    return ok;
}
static int test_piano_roll(void)
{
    int bad = 0, ok;
    uint32_t i, nb = 0, lo0, steps = 0;
    track_t *t;
    ui_power_on();
    t = TSEL;
    load_pat16(t, PATTERNS[0].note, PATTERNS[0].flags);               /* ACID: A2..A3, ties, accents, slides */
    t->p[P_ROOT] = 9; t->p[P_SCALE] = 2;
    go_page(GR_ROLL); ui.cursor = 0; ui.bank = 0;
    memset(host_screen, 0, sizeof host_screen); ui.force = 1; ui_draw();
    ok = pr_columns_match(t, &nb);
    bad += check("piano roll: ACID's bars drawn at their notes' rows, ties carried, rests empty", ok && nb >= 10u);
    bad += check("  the view holds the page's notes (A2..A3 in 21 rows), MONO gray", proll.lo <= 45 && proll.lo + PR_ROWS - 1 >= 57 &&
                 screen_gray());
    t->p[P_VOICE] = V_POLY;                                           /* chords: up to 4 bars in a column */
    for (i = 0; i < 16u; i += 4u) {
        step_t *s = &t->step[i];
        s->time = ST_NOTE; s->n = 4; s->note[0] = 57; s->note[1] = 60; s->note[2] = 64; s->note[3] = 67;
        t->step[i + 1u].time = ST_TIE;
    }
    nb = 0;
    memset(host_screen, 0, sizeof host_screen); ui.force = 1; ui_draw();
    bad += check("  4-note chords stacked, their ties carry all four", pr_columns_match(t, &nb));
    for (i = 0; i < NSTEP; i++) {                                     /* LEN 32, page 2 */
        step_clear(&t->step[i]);
        if (i >= 16u && i % 2u == 0u) { t->step[i].time = ST_NOTE; t->step[i].n = 1; t->step[i].note[0] = (uint8_t)(72 + i % 7u); }
    }
    t->p[P_SLEN] = 32; cursor_set(18);
    nb = 0;
    memset(host_screen, 0, sizeof host_screen); ui.force = 1; ui_draw();
    bad += check("  LEN 32, the cursor on page 2: its 16 steps drawn", ui.bank == 1u && pr_columns_match(t, &nb) && nb == 8u);
    lo0 = proll.lo;                                                   /* two octaves up: the view follows in steps */
    for (i = 16; i < 32u; i++) if (t->step[i].n) t->step[i].note[0] += 24;
    for (i = 0; i < 20u && proll.lo != lo0 + 24u; i++) { frame(); steps++; }
    bad += check("  notes moved two octaves: the view follows over frames (not at once), then rests",
                 proll.lo == lo0 + 24u && steps >= 3u && steps < 20u);
    {
        uint32_t sig = ui.graph_sig;
        frame(); frame();
        bad += check("  nothing changed: the graph is not drawn again", ui.graph_sig == sig);
    }
    nb = 0;
    bad += check("  after the follow the bars match again", pr_columns_match(t, &nb));
    key_down(7); frames(200);                                         /* a key held: its row lit on the strip */
    {
        int32_t r = (int32_t)proll.lo + PR_ROWS - 1 - (int32_t)kb_chord[7][0];
        uint16_t c = r >= 0 && r < PR_ROWS ? swap16(host_screen[(Y_GRAPH + PR_Y0 + (uint32_t)r * PR_RH + 1u) * 240u + PR_KX + 7]) : 0;
        bad += check("  a key held: its row on the keyboard strip is the accent", kb_chn[7] && c == T_ACCENT);
    }
    key_up(7); frames(100);
    return bad;
}

/* bug fixes, second round (UI) */
static uint32_t accent_in(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    uint32_t n = 0;
    int32_t x, y;
    uint16_t a = (uint16_t)((T_ACCENT >> 8) | (T_ACCENT << 8));
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++)
            n += host_screen[y * 240 + x] == a;
    return n;
}
static int test_bughunt_ui2(void)
{
    int bad = 0;
    uint32_t i;
    {   /* 1. a layer button pressed under a dialog and still held after it closes: dead, its keys stay notes */
        static const uint32_t BTN[2] = {B_FX, B_GLO};
        for (i = 0; i < 2u; i++) {
            uint32_t k = white(0), ok;
            ui_power_on(); TSEL->p[P_VOICE] = V_POLY;
            go_home(); frame();
            confirm_open(CF_CLEAR_SEQ, 0); frame();
            btn_down(BTN[i]); frame(); frames(100);
            btn_down(B_OCTDN); frame(); btn_up(B_OCTDN); frame();   /* the dialog closes, the button still held */
            frames(800);
            ok = !ui.confirm && (ui.ly_t0 & LY_DEAD) && !ui.layer && !kb_mask && !perf_mask;
            key_down(k); frame();
            ok = ok && kb_note[k] != KB_SILENT && gates() == 1u && !((kb_layer >> k) & 1u) && !perf_held && !ui.layer;
            key_up(k); frame();
            btn_up(BTN[i]); frame();
            bad += check(i ? "dead GLO held from a dialog: no map, the key plays a note"
                           : "dead FX held from a dialog: no map, the key plays a note, no effect", ok);
        }
    }
    {   /* 2. STEP entry with CHRD on writes what the key sounds, as live recording does */
        static const uint32_t KEY[2] = {7u, 8u};                      /* C4, C#4 (out of C major) */
        uint32_t v, j, ok = 1;
        for (v = 0; v < 2u; v++)
            for (j = 0; j < 2u; j++) {
                track_t *t;
                step_t e, r;
                ui_power_on(); t = TSEL; t->p[P_VOICE] = v ? V_MONO : V_POLY; t->p[P_CHRD] = CH_DIA3; t->p[P_SCALE] = 1;
                track_defaults_steps(t); go_page(GR_ROLL); cursor_set(0); frame();
                key_down(KEY[j]); frame();
                e = t->step[0];
                key_up(KEY[j]); frame();
                ok &= e.n == (v ? 1u : 3u) && e.note[0] == 60u && (v || (e.note[1] == 64u && e.note[2] == 67u));
                ui_power_on(); t = TSEL; t->p[P_VOICE] = v ? V_MONO : V_POLY; t->p[P_CHRD] = CH_DIA3; t->p[P_SCALE] = 1;
                track_defaults_steps(t); go_home(); frame();
                song.rec = 1; song.playing = 1; t->seq_idx = 3; t->seq_pos = 0;
                fm1_in.notes = 1u << KEY[j]; keyboard_block();
                r = t->step[3];
                fm1_in.notes = 0; keyboard_block();
                song.rec = 0; song.playing = 0;
                ok &= r.n == e.n && !memcmp(r.note, e.note, e.n);
            }
        bad += check("STEP entry with CHRD: POLY the chord, MONO its root, as live recording writes", ok);
    }
    {   /* 3. MIXER: the knob just turned is the one drawn in the accent (K1 LEVEL, K2 PAN, K3 REV, K4 the MUTE badge) */
        uint32_t k, a[4][4], ok = 1;
        for (k = 0; k < 4u; k++) {
            int32_t X = CARD_X(0), Y = Y_GRAPH;
            ui_power_on();
            settings.palette = 1; palette_set(1);
            go_page(GR_TRK); frame(); frame();
            turn(EN_K1 + k, 1);                                       /* (K4 right: MUTE on, its badge shown) */
            ui.force = 1; frame();
            a[k][0] = accent_in(X + 4, Y + 37, X + 38, Y + 71);       /* LEVEL */
            a[k][1] = accent_in(X + 3, Y + 84, X + 28, Y + 115);      /* PAN */
            a[k][2] = accent_in(X + 30, Y + 84, X + 55, Y + 115);     /* REV */
            a[k][3] = accent_in(X + 22, Y + 4, X + 56, Y + 20);       /* the MUTE badge */
        }
        for (k = 0; k < 4u; k++) {
            uint32_t j;
            for (j = 0; j < 4u; j++)
                ok &= j == k ? a[k][j] > 0u : a[k][j] == 0u;
        }
        bad += check("MIXER: K1..K4 light LEVEL / PAN / REV / the MUTE badge, nothing else", ok);
    }
    {   /* 4. renaming the slot the music came from renames the music: the next SAVE prefills the new name */
        char pn[16];
        uint32_t ok;
        ui_power_on();
        go_page(GR_SLOTS); song.g[G_SLOT] = 2;
        project_save_as(1, "OLD");
        project_save_as(2, "OTHER");
        project_load(1); frame();
        ok = str_eq(proj_name, "OLD");
        project_rename(2, "ELSE");
        ok &= str_eq(proj_name, "OLD");                               /* another slot: the music's name stays */
        press(B_EDIT);                                                /* rename B on the slot list */
        while (nm.len) { nm_do(NB_DEL); }
        nm_white(13); frames(900);
        str_cpy(nm.s, "NEW", sizeof nm.s); nm.len = nm.cur = 3; nm.key = 0;
        press(B_OCTUP);
        project_name(1, pn);
        ok &= str_eq(pn, "NEW") && str_eq(proj_name, "NEW");
        turn(EN_K4, 1); press(B_OCTUP);                               /* SAVE to B: OVERWRITE? */
        press(B_OCTUP);                                               /* YES: NAME */
        ok &= name_on() && str_eq(nm.s, "NEW");
        press(B_OCTDN);
        bad += check("rename of the loaded slot: the music's name follows, SAVE prefills it", ok);
    }
    {   /* 5. NAME: PLAY stops a transport started meanwhile (so the name can be saved), never starts it; REC ignored */
        uint32_t ok;
        ui_power_on(); go_page(GR_USER); ui.uslot = 3; press(B_OCTUP);
        ok = name_on();
        press(B_PLAY); frame();
        ok &= !transport_req && !song.playing && name_on();          /* stopped: PLAY starts nothing */
        press(B_REC); frame();
        ok &= !song.rec && !transport_req && name_on();
        song.playing = 1;                                             /* an external MIDI Start / the editor */
        press(B_OCTUP);
        ok &= name_on() && msg_is("STOP TO SAVE");
        press(B_REC); frame();
        ok &= !song.rec && !transport_req;
        press(B_PLAY); frame();
        ok &= transport_req == 2u && name_on();                       /* PLAY stops it, NAME stays */
        stop_transport();
        press(B_OCTUP);
        ok &= !name_on() && up_used(3);
        bad += check("NAME: PLAY stops a running transport (never starts), REC ignored, then OCT+ saves", ok);
    }
    {   /* 6. the piano roll's lowest C name ("C-1") stays inside the panel: nothing drawn left of x 3 */
        uint32_t p, x, y, out = 0;
        uint16_t bg;
        for (p = 0; p < NPALETTES; p++) {
            track_t *t;
            uint8_t lo = 0, hi = 127;
            ui_power_on(); t = TSEL; track_defaults_steps(t); t->p[P_SLEN] = 16;
            settings.palette = (uint8_t)p; palette_set(p);
            t->step[0].time = t->step[5].time = ST_NOTE; t->step[0].n = t->step[5].n = 1;
            t->step[0].note[0] = lo; t->step[5].note[0] = hi;
            go_page(GR_ROLL); cursor_set(0); ui.force = 1; frame(); frame();
            bg = (uint16_t)((T_BG >> 8) | (T_BG << 8));
            for (y = Y_GRAPH; y < Y_GRAPH + H_GRAPH; y++)
                for (x = 0; x < 3u; x++)
                    out += host_screen[y * 240u + x] != bg;
        }
        bad += check("piano roll: the C-1 label stays inside the panel (every palette)", !out);
    }
    {   /* 7. EDIT: INIT is drawn in the cell after the engines, and that cell's white key is the one that inits */
        uint32_t p, ok = LY_INIT == NENG_SHOWN && LY_INIT < 16u;      /* (layer_edit: INIT in cell NENG_SHOWN) */
        for (p = NENG_SHOWN; p < 16u; p++) {
            ui_power_on(); go_home(); frame();
            btn_down(B_EDIT); key_down(white(p)); frame(); frames(50);
            ok &= (ui.confirm == CF_INIT_SOUND) == (p == LY_INIT);
            key_up(white(p)); btn_up(B_EDIT); frame();
        }
        bad += check("EDIT: INIT's map cell (after the engines) and its key agree", ok);
    }
    return bad;
}

#ifndef UI_TEST_NO_MAIN
/* L is a sparse face (tools/gen_aa_font.py L_CHARS): every string drawn in it has all its glyphs */
static int test_large_face(void)
{
    static const char *const FIXED[] = {"FELUCCA", "0123456789"};   /* main.c, ui_menu.c; ui_draw.c draw_uboot */
    uint32_t i, missing = 0;
    const char *s;
    for (i = 0; i < NB + NE + 2u; i++)                              /* ui_input.c setup_show: the control names */
        for (s = i < NB ? B_NAME[i] : i < NB + NE ? E_NAME[i - NB] : FIXED[i - NB - NE]; *s; s++)
            missing += glyph_at(&AF_L, (uint8_t)*s) < 0;
    return check("the L face holds every glyph of the strings drawn in it", !missing);
}

#if FELUCCA_FM4
/* The DIGITAL algorithm charts (ui_graph.c FM_CELL / FM_MOD) against the DSP: src/eng_digital.c's switch (alg)
 * in digital_render_legacy and digital_render_custom, written out here as "source>destination" routes and the
 * operators mixed to the output. Op 4's feedback is on every algorithm (drawn always). Also: the carriers sit
 * on the bottom row and only they, a modulator one row above what it modulates, no two operators in a cell,
 * every operator heard, and the eight charts distinct. */
static int test_fm_charts(void)
{
    static const char *const DSP[8][2] = {
        {"4>3 3>2 2>1", "1"},          /* o3 = op(o4), o2 = op(o3), s = op(o2) */
        {"3>2 4>2 2>1", "1"},          /* o2 = op((o3 + o4) / 2), s = op(o2) */
        {"3>2 2>1 4>1", "1"},          /* o2 = op(o3), s = op((o2 + o4) / 2) */
        {"4>3 2>1 3>1", "1"},          /* o3 = op(o4), s = op((o2 + o3) / 2) */
        {"4>3 2>1", "13"},             /* o3 = op(o4), o1 = op(o2), s = (o1 + o3) / 2 */
        {"4>1 4>2 4>3", "123"},        /* o1, o2, o3 = op(o4), s = (o1 + o2 + o3) / 3 */
        {"4>3", "123"},                /* o3 = op(o4), s = (o1 + o2 + o3) / 3 */
        {"", "1234"}};                 /* s = (o1 + o2 + o3 + o4) / 4 */
    uint32_t a, b, k, j, routes = 1, layout = 1, distinct = 1;
    for (a = 0; a < 8u; a++) {
        uint32_t want = 0, car = 0, ok = 1;
        const char *s;
        for (s = DSP[a][0]; *s; s++)
            if (*s == '>') want |= 1u << (4u * (uint32_t)(s[1] - '1') + (uint32_t)(s[-1] - '1'));
        for (s = DSP[a][1]; *s; s++) car |= 1u << (*s - '1');
        if (FM_MOD[a] != want) { printf("ui: algorithm %u: routes %04x, the DSP %04x\n", a + 1u, FM_MOD[a], want); routes = 0; }
        for (k = 0; k < 4u; k++) {
            uint32_t row = FM_CELL[a][k] >> 4, outs = 0;
            if ((row == 0u) != ((car >> k) & 1u)) ok = 0;
            for (j = 0; j < 4u; j++) {
                if (j != k && FM_CELL[a][j] == FM_CELL[a][k]) ok = 0;
                if (FM_MOD[a] >> (4u * j + k) & 1u) { outs++; if (row != (FM_CELL[a][j] >> 4) + 1u) ok = 0; }
            }
            if (!outs && !((car >> k) & 1u)) ok = 0;
        }
        if (!ok) { printf("ui: algorithm %u: the chart's layout is off\n", a + 1u); layout = 0; }
        for (b = 0; b < a; b++)
            if (FM_MOD[a] == FM_MOD[b] && !memcmp(FM_CELL[a], FM_CELL[b], 4)) distinct = 0;
    }
    return check("DIGITAL charts: the 8 algorithms' routes and carriers equal eng_digital.c's", routes) +
           check("DIGITAL charts: carriers on the bottom row, modulators a row up, one per cell, distinct", layout && distinct);
}
#endif

/* The FM6 algorithm charts (ui_graph.c graph_fm6: FM6_CELL, its routes fm6_routes and feedback operator
 * fm6_fb_op read from fm6_core.c FM6_ALG) against the 32 algorithms of 6-operator FM written out here as
 * "source>destination" routes, the carriers and the operator with feedback (fm6_core.c's: on algorithms 4 and
 * 6 the loop is the sixth operator's own, as msfa plays it). Also against the core itself: the carriers
 * fm6_carriers (fm6_car_ops), the feedback flags. The layout: the carriers on the bottom row and only they, a modulator one
 * row above everything it modulates, one operator per cell, columns 0..5, at most 4 rows. (The charts as drawn:
 * tests/ui_render.c's lint, fm6_lint.) */
static int test_fm6_charts(void)
{
    static const struct { const char *r, *car; char fb; } ALGS[32] = {
        {"2>1 4>3 5>4 6>5", "13", '6'}, {"2>1 4>3 5>4 6>5", "13", '2'}, {"2>1 3>2 5>4 6>5", "14", '6'},
        {"2>1 3>2 5>4 6>5", "14", '6'}, {"2>1 4>3 6>5", "135", '6'}, {"2>1 4>3 6>5", "135", '6'},
        {"2>1 4>3 5>3 6>5", "13", '6'}, {"2>1 4>3 5>3 6>5", "13", '4'}, {"2>1 4>3 5>3 6>5", "13", '2'},
        {"2>1 3>2 5>4 6>4", "14", '3'}, {"2>1 3>2 5>4 6>4", "14", '6'}, {"2>1 4>3 5>3 6>3", "13", '2'},
        {"2>1 4>3 5>3 6>3", "13", '6'}, {"2>1 4>3 5>4 6>4", "13", '6'}, {"2>1 4>3 5>4 6>4", "13", '2'},
        {"2>1 3>1 5>1 4>3 6>5", "1", '6'}, {"2>1 3>1 5>1 4>3 6>5", "1", '2'}, {"2>1 3>1 4>1 5>4 6>5", "1", '3'},
        {"2>1 3>2 6>4 6>5", "145", '6'}, {"3>1 3>2 5>4 6>4", "124", '3'}, {"3>1 3>2 6>4 6>5", "1245", '3'},
        {"2>1 6>3 6>4 6>5", "1345", '6'}, {"3>2 6>4 6>5", "1245", '6'}, {"6>3 6>4 6>5", "12345", '6'},
        {"6>4 6>5", "12345", '6'}, {"3>2 5>4 6>4", "124", '6'}, {"3>2 5>4 6>4", "124", '3'},
        {"2>1 4>3 5>4", "136", '5'}, {"4>3 6>5", "1235", '6'}, {"4>3 5>4", "1236", '5'},
        {"6>5", "12345", '6'}, {"", "123456", '6'}};
    uint32_t a, k, j, routes = 1, layout = 1, core = 1;
    for (a = 0; a < 32u; a++) {
        uint8_t m[6], want[6] = {0, 0, 0, 0, 0, 0};
        uint32_t car = 0, ok = 1, fbs = 0;
        const char *s;
        for (s = ALGS[a].r; *s; s++)
            if (*s == '>') want[s[1] - '1'] |= (uint8_t)(1u << (s[-1] - '1'));
        for (s = ALGS[a].car; *s; s++) car |= 1u << (*s - '1');
        fm6_routes(a, m);
        if (memcmp(m, want, 6) || fm6_fb_op(a) != (uint32_t)(ALGS[a].fb - '1')) {
            printf("ui: FM6 algorithm %u: the chart's routes or feedback differ from the written ones\n", a + 1u);
            routes = 0;
        }
        for (k = 0; k < 6u; k++)                                    /* the core: who writes the output, who has FB */
            fbs += (FM6_ALG[a][k] & 0xC0u) == 0xC0u;
        if (fm6_car_ops(a) != car || fbs != 1u) {
            printf("ui: FM6 algorithm %u: fm6_core.c's carriers / feedback differ\n", a + 1u);
            core = 0;
        }
        for (k = 0; k < 6u; k++) {
            uint32_t row = FM6_CELL[a][k] >> 4, outs = 0;
            if ((row == 0u) != ((car >> k) & 1u) || (FM6_CELL[a][k] & 0x0Fu) > 5u || row > 3u) ok = 0;
            for (j = 0; j < 6u; j++) {
                if (j != k && FM6_CELL[a][j] == FM6_CELL[a][k]) ok = 0;
                if (m[j] >> k & 1u) { outs++; if (FM6_CELL[a][k] >> 4 != (FM6_CELL[a][j] >> 4) + 1u) ok = 0; }
            }
            if (!outs && !((car >> k) & 1u)) ok = 0;               /* every operator heard */
        }
        if (!ok) { printf("ui: FM6 algorithm %u: the chart's layout is off\n", a + 1u); layout = 0; }
    }
    return check("FM6 charts: the 32 algorithms' routes, carriers and feedback operator equal fm6_core.c's", routes && core) +
           check("FM6 charts: carriers on the bottom row, modulators a row up, one per cell, at most 4 rows", layout);
}

#if !FELUCCA_FM4
/* DIGITAL retired (src/fm4_convert.c): engine 1 is on no track and in no list; its sounds arrive as FM6 with a patch
 * of their own: a user preset of it, its preset numbers (set_engine_of / apply_preset_to), a track that got engine 1
 * any other way (the main loop's net); the favourites of its presets move to FM6's; the power-on pad is FM6 PAD */
static int test_fm4_retired(void)
{
    int bad = 0, ok = 1;
    uint32_t i, k, e, total, seen = 0, all = ((1u << NENGINES) - 1u) & ~(1u << ENGI_DIGITAL);
    int16_t p[P_COUNT];
    uint8_t v[FP_SIZE + 1u];
    ui_power_on();
    bad += check("power-on: track 2 is FM6 PAD (TRK_DEF, was DIGITAL PAD)", trk[1].eng_req == ENGI_FM6 &&
                 str_eq(ENGINES[ENGI_FM6]->presets[trk[1].preset].name, "PAD"));
    preset_all_pos(&total);
    for (i = 0; i < total; i++) {
        e = preset_all_at(i, &k);
        if (e < NENGINES)
            seen |= 1u << e;
    }
    bad += check("PRESETS: the list holds every engine's presets but DIGITAL's", seen == all && NENG_SHOWN == NENGINES - 1u);
    go_page(GR_BROWSE);
    set_engine_of(TSEL, 0);
    for (i = 0, seen = 0; i < NENG_SHOWN; i++) {
        turn(EN_K2, 1);
        seen |= 1u << TSEL->eng_req;
    }
    bad += check("PRESETS KNOB 2: the engines in order, DIGITAL skipped, back to the first",
                 seen == all && TSEL->eng_req == 0u && eng_step(0, 1) == ENGI_FM6 && eng_step(ENGI_FM6, 1) == 2u &&
                 eng_step(ENGI_FM6, -1) == 0u && eng_step(0, -1) == ENGI_DRUM);
    {   /* the display order (engines.c ENGINE_ORDER): every engine one can pick once; the PRESETS list follows it */
        static const char *const ORDER[] = {"ANALOG", "FM6", "PHASE", "LOFI", "SAMPLE", "VOICE", "TRIO", "WHEEL", "GRAIN",
                                            "PHYS", "GURDY", "NOISE", "SLICE", "DRUM"};
        uint32_t last = 0xFFu, r = 0, n = 0;
        ok = NENG_SHOWN == NELEM(ORDER);
        for (i = 0; ok && i < NENG_SHOWN; i++)
            ok &= str_eq(ENGINES[eng_vis(i)]->name, ORDER[i]) && eng_rank(eng_vis(i)) == i && eng_ok(eng_vis(i));
        preset_all_pos(&total);
        for (i = 0; i < total; i++) {                  /* the list's engines, each once, in that order */
            e = preset_all_at(i, &k);
            if (e >= NENGINES || e == last)
                continue;
            ok &= n < NENG_SHOWN && e == eng_vis(n);
            n++;
            last = e;
            r++;
        }
        bad += check("engines shown ANALOG FM6 PHASE ... NOISE SLICE DRUM (ENGINE_ORDER); PRESETS lists them so",
                     ok && r == NENG_SHOWN);
    }
    /* a user preset stored with engine 1: kept as it is, it loads as FM6 with the converted patch */
    for (i = 0; i < P_COUNT; i++)
        p[i] = param_desc_of(ENGI_DIGITAL, i)->def;
    fm4_preset_values(p, 5);                           /* DIGITAL PAD */
    {
        up_rec_t r;
        memset(&r, 0, sizeof r);
        r.used = UP_USED;
        r.ver = UP_VER;
        r.engine = ENGI_DIGITAL;
        r.np = P_COUNT;
        memcpy(r.name, "OLD PAD", 7);
        for (i = 0; i < P_COUNT; i++)
            up_set_value(&r, i, p[i]);
        up_put(7, &r);
    }
    fm4_convert(p, v);
    up_load(7);
    bad += check("a DIGITAL user preset loads as FM6: the converted patch its own (SLOT OWN), preset = FM6 PAD",
                 TSEL->eng_req == ENGI_FM6 && TSEL->preset == 4u && TSEL->p[P_E7] == FM6_OWN && !TSEL->p[P_E0] &&
                 !memcmp(fm6_patch[song.sel], v, FP_SIZE) && fm6_slot[song.sel] == FM6_OWN && TSEL->user == 8u);
    frame();
    bad += check("  .. and the main loop keeps that patch (not a factory one)", !memcmp(fm6_patch[song.sel], v, FP_SIZE));
    eng_list_pos(&total);
    bad += check("  the record stays DIGITAL in the bank, listed with FM6's sounds (EDIT KNOB 2)",
                 up_rec(7)->engine == ENGI_DIGITAL && up_engine(7) == ENGI_FM6 &&
                 total == ENGINES[ENGI_FM6]->npresets + 1u);
    up_store(8, "AGAIN");
    {
        uint8_t pk[FM6_PACKED], w[FP_SIZE + 1u];
        bad += check("  saved again: an FM6 user preset with SLOT OWN and the converted patch beside it",
                     up_rec(8)->engine == ENGI_FM6 && up_value(up_rec(8), P_E7) == FM6_OWN && !upf_get(8, pk) &&
                     (fm6_unpack(pk, w), !memcmp(w, v, FP_SIZE)));
        fm6_load_slot(song.sel, 2);                    /* another patch on the track, then the preset again */
        up_load(8);
        frame();
        bad += check("  .. which loads back: the same patch, SLOT OWN", !memcmp(fm6_patch[song.sel], v, FP_SIZE) &&
                     TSEL->p[P_E7] == FM6_OWN && fm6_slot[song.sel] == FM6_OWN);
    }
    /* preset numbers of engine 1 */
    set_engine_of(TSEL, ENGI_DIGITAL);
    bad += check("engine 1 asked for: DIGITAL E.PIANO converted (FM6, PTCH TINE EP, the patch named E.PIANO)",
                 TSEL->eng_req == ENGI_FM6 && TSEL->preset == 0u && !memcmp(fm6_patch[song.sel] + FP_NAME, "E.PIANO", 7));
    TSEL->eng_req = ENGI_DIGITAL;                      /* (the ISR's view: no other path does this) */
    apply_preset_to(TSEL, 1);
    bad += check("a DIGITAL preset number (apply_preset_to): its sound converted (BELL)",
                 TSEL->eng_req == ENGI_FM6 && TSEL->preset == 1u && !memcmp(fm6_patch[song.sel] + FP_NAME, "BELL", 4));
    for (i = 0; i < P_COUNT; i++)
        trk[2].p[i] = param_desc_of(ENGI_DIGITAL, i)->def;
    fm4_preset_values(trk[2].p, 2);
    trk[2].eng_req = ENGI_DIGITAL;
    frame();
    bad += check("a track left on engine 1 any other way: the main loop converts it (BASS -> FM BASS)",
                 trk[2].eng_req == ENGI_FM6 && trk[2].preset == 2u);
    {   /* favourites of DIGITAL presets (settings PER4) -> the FM6 presets that cover them */
        persist_t pe;
        memset(&pe, 0, sizeof pe);
        pe.magic = PERSIST_MAGIC;
        pe.panel = PANEL_DEFAULT;
        pe.favorites.factory[ENGI_DIGITAL][0] = 1u << 5 | 1u << 7;   /* PAD, FUNK KEY */
        settings_import(&pe, (int)sizeof pe);
        bad += check("favourites: DIGITAL PAD / FUNK KEY -> FM6 PAD / PLUCK, engine 1's cleared",
                     favorite_has(ENGI_FM6, 4) && favorite_has(ENGI_FM6, 7) && !favorites.factory[ENGI_DIGITAL][0]);
        memset(&favorites, 0, sizeof favorites);
    }
    return bad;
}
#endif

/* #38: the keys of the notes the selected track's sequencer and ARP sound light while it plays */
static int test_play_leds(void)
{
    int bad = 0, ok;
    track_t *t = &trk[0];
    uint32_t i;
    ui_power_on();
    song.sel = 0;
    t->step[0] = (step_t){.note = {60, 64}, .n = 2, .time = ST_NOTE};
    t->step[1] = (step_t){.note = {100}, .n = 1, .time = ST_NOTE};
    trk[1].step[0] = (step_t){.note = {67}, .n = 1, .time = ST_NOTE};
    t->seq_active = trk[1].seq_active = 1;
    bad += check("stopped, nothing held: no key lit", key_leds() == 0u && play_leds() == 0u);
    seq_start();
    events_block(CTL);
    bad += check("playing: C4 and E4 of step 1 light keys 8 and 12 (from F3), track 2's G4 does not",
                 t->seq_n == 2u && trk[1].seq_n == 1u && key_leds() == (1u << 7 | 1u << 11));
    song.sel = 1;
    bad += check("the selected track's notes: track 2's G4 on key 15", play_leds() == 1u << 14);
    song.sel = 0;
    fm1_in.notes = 1u << 0;
    bad += check("a key held still lights with them", key_leds() == (1u << 0 | 1u << 7 | 1u << 11));
    fm1_in.notes = 0;
    song.octave = -1;
    ok = play_leds() == (1u << 19 | 1u << 23);
    song.octave = 1;
    ok &= play_leds() == 0u;                           /* (C4 and E4 below the keys an octave up) */
    song.octave = 0;
    t->p[P_TRANS] = 2;
    ok &= play_leds() == (1u << 5 | 1u << 9);
    t->p[P_TRANS] = 0;
    bad += check("the keys follow the octave and TRN; notes off the keyboard are not shown", ok);
    t->p[P_QUANT] = 1;                                 /* SNAP in C major: C#4 rounds down to C4 too */
    t->p[P_SCALE] = 1;
    t->p[P_ROOT] = 0;
    ok = kb_map(t, 8) == 60u && play_leds() == (1u << 7 | 1u << 11);
    t->p[P_QUANT] = t->p[P_SCALE] = 0;
    bad += check("QNT SNAP: only the key of the note itself lights, not the keys rounding onto it", ok);
    t->arp_note = 65;
    bad += check("the ARP's note lights its key too", play_leds() == (1u << 7 | 1u << 11 | 1u << 12));
    t->arp_note = 0;
    events_block(div_samples((uint32_t)t->p[P_SDIV]) + CTL);
    bad += check("a note above the keyboard (G#7) lights nothing", t->seq_n == 1u && t->seq_notes[0] == 100u &&
                 play_leds() == 0u);
    transport_req = 2;
    events_block(CTL);
    bad += check("stopped: the notes end, their keys go dark", !song.playing && key_leds() == 0u);
    /* the layer, the grid and NAME keep their keys */
    seq_start();
    events_block(CTL);
    ui.layer = 1;
    ok = key_leds() == layer_leds();
    ui.layer = 0;
    set_engine_of(t, ENGI_DRUM);
    t->engine = t->eng_req;
    t->step[0] = (step_t){.hit = 1u << DV_KICK, .time = ST_NOTE};
    transport_req = 2; events_block(CTL);
    seq_start(); events_block(CTL);
    i = play_leds();
    ok &= t->seq_n == 1u && i != 0u && kb_map(t, (uint32_t)__builtin_ctz(i)) == DRUM_LANE_NOTE[DV_KICK];
    open_family(FAM_SEQ);
    frame();
    ok &= grid_on() && key_leds() == grid_leds();
    bad += check("the layer's map and the DRUM grid keep their keys; elsewhere a kit's hits light the keys that play them", ok);
    transport_req = 2; events_block(CTL);
    return bad;
}

/* Discussion #81: the notes MIDI IN holds on the selected track light their keys as the sequencer's do: USB and TRS,
 * routed by ROUT; at the octave now, the lowest key; off the keyboard nothing; the pedal holds them; the layer's map
 * and the DRUM grid keep their keys */
static void usb_note(uint32_t ch, uint32_t note, uint32_t vel)   /* a USB-MIDI packet, as usb.c queues it */
{
    uint32_t st = (vel ? 0x90u : 0x80u) | ch;
    midi_in_event((st >> 4) | st << 8 | note << 16 | vel << 24);
    events_block(CTL);
}
static void trs_note(uint32_t a, uint32_t b, uint32_t c)        /* TRS MIDI IN: midi_uart.c's parser */
{
    um_byte(a); um_byte(b); um_byte(c);
    events_block(CTL);
}
static int test_midi_leds(void)
{
    int bad = 0, ok;
    ui_power_on();
    memset(&um, 0, sizeof um);
    song.sel = 0;
    usb_note(0, 60, 100);                              /* channel 1 -> T1 (ROUT CH1-4) */
    bad += check("#81 USB MIDI C4 into the selected track lights key 8 (C4)", midi_sel_on[0][60] == 1u &&
                 key_leds() == 1u << 7 && play_leds() == 1u << 7);
    usb_note(1, 67, 100);                              /* channel 2 -> T2: not the selected track */
    ok = key_leds() == 1u << 7;
    song.sel = 1;
    ok &= key_leds() == 1u << 14;
    song.sel = 0;
    bad += check("  another track's MIDI notes light nothing; selected, its G4 lights key 15", ok);
    usb_note(1, 67, 0);
    usb_note(0, 60, 0);
    bad += check("  note-off: dark", key_leds() == 0u && !midi_owners[0]);
    trs_note(0x90, 64, 90);                           /* TRS: E4 on channel 1 */
    ok = key_leds() == 1u << 11;
    trs_note(0x80, 64, 0);
    bad += check("  TRS MIDI IN: E4 lights key 12 while held", ok && key_leds() == 0u);
    song.g[G_ROUTE] = 1;                               /* ROUT SEL: every channel to the selected track */
    song.sel = 2;
    usb_note(9, 62, 100);
    ok = midi_sel_on[9][62] == 3u && key_leds() == 1u << 9;
    usb_note(9, 62, 0);
    song.g[G_ROUTE] = 0;
    song.sel = 0;
    bad += check("  ROUT SEL: channel 10 into the selected track lights its key", ok && key_leds() == 0u);
    usb_note(0, 60, 100);
    song.octave = -1;
    ok = key_leds() == 1u << 19;
    song.octave = 1;
    ok &= key_leds() == 0u;                            /* (C4 below the keys an octave up) */
    song.octave = 0;
    usb_note(0, 60, 0);
    usb_note(0, 100, 100);                             /* G#7: above the keyboard */
    usb_note(0, 40, 100);                              /* E2: below it */
    ok &= key_leds() == 0u && midi_owners[0] == 2u;
    usb_note(0, 100, 0);
    usb_note(0, 40, 0);
    bad += check("  the keys follow the octave; notes off the keyboard are not shown", ok);
    trk[0].p[P_QUANT] = 1;                             /* SNAP in C major: C#4 rounds down to C4 too */
    trk[0].p[P_SCALE] = 1;
    trk[0].p[P_ROOT] = 0;
    usb_note(0, 60, 100);
    ok = kb_map(&trk[0], 8) == 60u && key_leds() == 1u << 7;
    usb_note(0, 60, 0);
    trk[0].p[P_QUANT] = trk[0].p[P_SCALE] = 0;
    bad += check("  QNT SNAP: only the lowest key giving the note lights", ok && key_leds() == 0u);
    midi_event(0xB0, 0, 64, 127);                      /* the pedal down: a released note still sounds */
    usb_note(0, 65, 100);
    usb_note(0, 65, 0);
    ok = key_leds() == 1u << 12;
    midi_event(0xB0, 0, 64, 0);
    bad += check("  a note the pedal holds stays lit until the pedal is up", ok && key_leds() == 0u);
    fm1_in.notes = 1u << 0;                            /* with a key held and the sequencer */
    usb_note(0, 60, 100);
    ok = key_leds() == (1u << 0 | 1u << 7);
    fm1_in.notes = 0;
    trk[0].step[0] = (step_t){.note = {64}, .n = 1, .time = ST_NOTE};
    trk[0].seq_active = 1;
    seq_start();
    events_block(CTL);
    ok &= key_leds() == (1u << 7 | 1u << 11);
    bad += check("  with a key held and the sequencer's notes: all of them lit", ok);
    ui.layer = LAYER_SCL;
    ok = key_leds() == layer_leds();
    ui.layer = 0;
    open_family(FAM_SEQ);                              /* STEP on a melodic track: the keys as elsewhere */
    frame();
    ok &= !grid_on() && (key_leds() & (1u << 7));
    set_engine_of(&trk[0], ENGI_DRUM);
    trk[0].engine = trk[0].eng_req;
    frame();
    ok &= grid_on() && key_leds() == grid_leds();
    bad += check("  the layer's map and the DRUM grid keep their keys over the MIDI notes", ok);
    transport_req = 2;
    events_block(CTL);
    usb_note(0, 60, 0);
    return bad;
}

/* #35: every idle button and key glows dim, the active ones are lit; a map of the keys' own stays lit / dark */
static int test_idle_glow(void)
{
    int bad = 0, ok;
    ui_power_on();
    ui_leds();
    memset(led_pos, 0xFF, sizeof led_pos);
    led_pos[panel.btn[B_HOME]] = 0u << 3 | 1u;
    led_pos[panel.btn[B_ENV]] = 0u << 3 | 2u;
    led_pos[panel.btn[B_PLAY]] = 0u << 3 | 3u;
    led_pos[14u + 0u] = 1u << 3 | 1u;              /* keys 1 and 2 */
    led_pos[14u + 1u] = 1u << 3 | 2u;
    go_home();
    ui_leds();
    bad += check("HOME: its button lit, ENV and PLAY (stopped) glow, the keys glow, none lit",
                 fm1_led[0] == 2u && fm1_led_dim[0] == 14u && fm1_led[1] == 0u && fm1_led_dim[1] == 6u);
    fm1_in.notes = 1u;
    song.playing = 1;
    ui_leds();
    ok = fm1_led[0] == 2u && (fm1_led_dim[0] & 8u) == 0u && fm1_led[8] == 2u && fm1_led[1] == 2u && fm1_led_dim[1] == 6u;
    fm1_in.notes = 0;
    song.playing = 0;
    bad += check("a key held lit over the glow; PLAY running: its green, its own LED dark", ok);
    ui.layer = 1;
    ui_leds();
    ok = fm1_led_dim[1] == 0u && fm1_led_dim[0] == 14u;
    ui.layer = 0;
    ui_leds();
    ok &= fm1_led_dim[1] == 6u;
    bad += check("a layer's map: the keys lit or dark (no glow), the buttons still glow", ok);
    /* MENU > LEDS INV: the active ones dark, the idle ones lit (stock), no glow; a blink lit / dark; a map of the
     * keys' own as in DIM */
    settings_leds = LEDS_INV;
    ui_leds();
    ok = fm1_led[0] == 12u && fm1_led_dim[0] == 0u && fm1_led[1] == 6u && fm1_led_dim[1] == 0u;   /* HOME dark */
    fm1_in.notes = 1u;
    song.playing = 1;
    ui_leds();
    ok &= fm1_led[0] == 4u && fm1_led_dim[0] == 0u && fm1_led[1] == 4u && fm1_led_dim[1] == 0u && fm1_led[8] == 2u;   /* PLAY, key 1 dark; green */
    fm1_in.notes = 0;
    song.playing = 0;
    bad += check("LEDS INV: the page's button, PLAY running and a key held go dark, the rest lit, no glow", ok);
    ui.layer = 1;
    led_pos[panel.btn[layer_btn()]] = 0u << 3 | 4u;   /* the layer's button */
    {
        uint32_t seen = 0, t;
        for (t = 0; t < 4u; t++) {                  /* the layer's button blinks: lit, then dark, never dim */
            fm1_ms = t * 250u;
            ui_leds();
            seen |= ((fm1_led[0] >> 4) & 1u ? 1u : 2u) | ((fm1_led_dim[0] >> 4) & 1u ? 4u : 0u);
        }
        ok = seen == 3u && fm1_led_dim[1] == 0u && fm1_led[1] == (uint8_t)((layer_leds() & 3u) << 1);
    }
    ui.layer = 0;
    fm1_ms = 0;
    bad += check("LEDS INV: a blink alternates lit / dark; a layer's map stays lit / dark as in DIM (not inverted)", ok);
    settings_leds = LEDS_DIM;
    led_pos_init();
    return bad;
}

/* "On the step sequencer all the LEDs go out": SEQ > STEP, the piano roll and the DRUM grid, stopped and playing,
 * in every LEDS mode, with the LEDs where the hardware has them (hal/fm1_input.h FM1_KEYMAP). The buttons keep
 * their glow and their active states; on the piano roll the keys glow under the held and playing ones; on the
 * grid its map is lit and its keys that do something glow (DIM LO / HI). Before: keys_own() took the grid's keys
 * out of the glow, so in DIM 26 of 27 keys of an empty 16-step grid went fully dark */
static int test_step_leds(void)
{
    static const int8_t MAP[6][FM1_NCOL] = {     /* (hal/fm1_input.h FM1_KEYMAP; the host stub is zero) */
        {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
        {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},
        {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
        { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
        {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    };
    static const uint8_t MODES[LEDS_COUNT] = {LEDS_OFF, LEDS_DIM_LO, LEDS_DIM, LEDS_INV};
    int bad = 0;
    uint32_t id, p, r, mi, drum, run, k;
    ui_power_on();
    ui_leds();
    for (id = 0; id < 41u; id++) {
        led_pos[id] = 0xFF;
        for (p = 0; p < FM1_NCOL; p++)
            for (r = 1; r < 5u; r++)
                if (MAP[r][p] == (int8_t)id)
                    led_pos[id] = (uint8_t)(p << 3 | r);
    }
    for (drum = 0; drum < 2u; drum++)
        for (run = 0; run < 2u; run++)
            for (mi = 0; mi < LEDS_COUNT; mi++) {
                uint32_t m = MODES[mi], glow = m == LEDS_DIM || m == LEDS_DIM_LO, keys_lit = 0, keys_dim = 0, btn_bad = 0;
                uint32_t want_dim = 0, kc, keys;
                char what[128];
                ui_power_on();
                song.sel = 0;
                if (drum) {
                    set_engine_of(TSEL, ENGI_DRUM);
                    TSEL->engine = TSEL->eng_req;
                    TSEL->p[P_SLEN] = 16;
                    TSEL->step[2] = (step_t){.hit = 1u << DV_KICK, .time = ST_NOTE};   /* one hit, on step 3 */
                } else {
                    TSEL->step[0] = (step_t){.note = {60}, .n = 1, .time = ST_NOTE};   /* C4 on step 1 */
                }
                TSEL->seq_active = 1;
                settings_leds = (uint8_t)m;
                open_family(FAM_SEQ);
                frame();
                for (k = 0; k < NPAGES && cur_page()->graph != GR_ROLL; k++) {
                    open_family(FAM_SEQ);
                    frame();
                }
                if (run) {
                    seq_start();
                    events_block(CTL);
                }
                ui_leds();
                for (k = 0; k < NB; k++) {                  /* a button: lit if active, else glow / dark / lit */
                    uint8_t q = led_pos[panel.btn[k]];
                    uint32_t lit = (fm1_led[q >> 3] >> (q & 7u)) & 1u, dim = (fm1_led_dim[q >> 3] >> (q & 7u)) & 1u;
                    int active = k == B_SEQ || (k == B_PLAY && run);
                    btn_bad += m == LEDS_INV ? lit == (uint32_t)active || dim :
                               active ? (k == B_PLAY ? lit || dim : !lit) : lit || dim != glow;
                }
                keys = key_leds();
                for (k = 0; k < 27u; k++) {
                    uint8_t q = led_pos[14u + k];
                    keys_lit += (fm1_led[q >> 3] >> (q & 7u)) & 1u;
                    keys_dim += ((fm1_led_dim[q >> 3] & ~fm1_led[q >> 3]) >> (q & 7u)) & 1u;
                }
                kc = (uint32_t)__builtin_popcount(keys);
                if (glow)
                    want_dim = (uint32_t)__builtin_popcount((drum ? grid_glow() : 0x7FFFFFFu) & ~keys);
                snprintf(what, sizeof what, "STEP %s, %s, LEDS %s: buttons and keys as the mode says (%u lit, %u glow)",
                         drum ? "DRUM grid" : "piano roll", run ? "playing" : "stopped", LEDS_NAME[m],
                         (unsigned)keys_lit, (unsigned)keys_dim);
                bad += check(what, !btn_bad && host_dim_lo == (m == LEDS_DIM_LO) &&
                                       grid_on() == (int)drum && (!run || kc) &&
                                       keys_dim == want_dim && keys_lit == (m == LEDS_INV && !drum ? 27u - kc : kc) &&
                                       (!glow || keys_lit + keys_dim >= 25u) &&
                                       ((fm1_led[LED_PLAY_GREEN >> 3] >> (LED_PLAY_GREEN & 7u)) & 1u) == run);
                if (run) {
                    transport_req = 2;
                    events_block(CTL);
                }
            }
    settings_leds = LEDS_DIM;
    led_pos_init();
    ui_power_on();
    return bad;
}

/* #37: QNT SEQ on the SCL page: the keys as SNAP, the sequence snapped as it plays, saved with the project */
static int test_seq_quant(void)
{
    int bad = 0, ok;
    track_t *t = &trk[0];
    char val[16];
    const char *unit = 0;
    ui_power_on();
    song.sel = 0;
    open_family(FAM_SCL);
    frame();
    ok = cur_page()->id[2] == P_QUANT && t->p[P_QUANT] == 0;
    turn(EN_K3, 10);
    param_format(&TP[P_QUANT], t->p[P_QUANT], val, &unit);
    bad += check("SCL KNOB 3 QNT: OFF SNAP WHITE SEQ, SEQ last (default OFF)", ok && t->p[P_QUANT] == 3 &&
                 str_eq(val, "SEQ"));
    t->p[P_SCALE] = 1;
    t->p[P_ROOT] = 0;
    ok = kb_map(t, 8) == 60u && kb_map(t, 7) == 60u;   /* C#4 key -> C4, as SNAP */
    t->step[0] = (step_t){.note = {61, 66}, .n = 2, .time = ST_NOTE};
    t->seq_active = 1;
    seq_start();
    events_block(CTL);
    ok &= t->seq_n == 2u && t->seq_notes[0] == 60u && t->seq_notes[1] == 65u && t->step[0].note[0] == 61u &&
          t->step[0].note[1] == 66u;
    transport_req = 2;
    events_block(CTL);
    bad += check("QNT SEQ: the keys snap as SNAP; C#4 F#4 of a step play C4 F4 in C major, the step keeps C#4 F#4", ok);
    project_save(1);
    t->p[P_QUANT] = 0;
    project_load(1);
    bad += check("QNT SEQ saved and loaded with the project", trk[0].p[P_QUANT] == 3);
    return bad;
}

/* #39: KNOB 1..4 with a layer's button held are the layer's only, also as it is let go: a knob turned in the frame
 * the button is let go, while its map still shows (an effect key held after FX), or just after: the page under the
 * layer (ARP: ARP MODE on) or HOME kept its values. GLO / SCL / EDIT change only what their own knobs edit */
static int16_t lk_p[NTRK][P_COUNT], lk_g[G_COUNT];
static void lk_snap(void)
{
    uint32_t i;
    for (i = 0; i < NTRK; i++)
        memcpy(lk_p[i], trk[i].p, sizeof lk_p[i]);
    memcpy(lk_g, song.g, sizeof lk_g);
}
static int lk_same(uint32_t l)                      /* nothing changed but the layer's own (l: the layer) */
{
    uint32_t i, j;
    for (i = 0; i < NTRK; i++)
        for (j = 0; j < P_COUNT; j++) {
            int own = (l == LAYER_GLO && j == P_LEVEL) ||
                      (l == LAYER_SCL && i == 0u && (j == P_ROOT || j == P_SCALE || j == P_CHRD || j == P_VOIC)) ||
                      (l == LAYER_EDIT && i == 0u && (j < P_AMODE || j > P_AORDER));   /* (a load keeps the ARP) */
            if (lk_p[i][j] != trk[i].p[j] && !own)
                return 0;
        }
    for (j = 0; j < G_COUNT; j++)
        if (lk_g[j] != song.g[j] && l != LAYER_EDIT)
            return 0;
    return 1;
}
static void lk_turn(int32_t s)                       /* KNOB 1..4 together, no frame */
{
    uint32_t k;
    for (k = 0; k < 4u; k++)
        host_enc[panel.enc[EN_K1 + k]] += s * panel.dir[EN_K1 + k];
}
static int test_layer_knob_leak(void)
{
    static const uint8_t LB[4] = {B_FX, B_GLO, B_SCL, B_EDIT};
    static const uint8_t LL[4] = {LAYER_FX, LAYER_GLO, LAYER_SCL, LAYER_EDIT};
    uint32_t b, sc, pg, fails = 0, arp_fx = 0;
    int bad = 0, ok;
    for (b = 0; b < 4u; b++)
        for (sc = 0; sc < 4u; sc++)
            for (pg = 0; pg <= NPAGES; pg++) {          /* every page, HOME last */
                ui_power_on();
                if (pg < NPAGES) { ui.home = 0; ui.page = (uint8_t)pg; page_entered(); }
                frames(100);
                lk_snap();
                btn_down(LB[b]);
                if (sc == 3u) key_down(white(1));
                frame();
                lk_turn(5); frame();
                if (sc == 0u) { lk_turn(2); btn_up(LB[b]); frame(); }          /* turning as it is let go */
                if (sc == 1u) { btn_up(LB[b]); frame(); lk_turn(2); frame(); } /* .. the next frame */
                if (sc == 2u) { btn_up(LB[b]); frames(160); lk_turn(2); frame(); }   /* .. 160 ms after */
                if (sc == 3u) { btn_up(LB[b]); frame(); lk_turn(3); frame(); key_up(white(1)); frame(); }   /* the map */
                frames(400);
                ok = lk_same(LL[b]) && !perf_k[0] && !perf_k[1] && !perf_k[2] && !perf_k[3];
                if (!ok) {
                    fails++;
                    if (fails < 6u)
                        printf("ui:   leak: %s scenario %u page %s\n", B_NAME[LB[b]], sc,
                               pg < NPAGES ? PAGES[pg].title : "HOME");
                }
                if (b == 0u && pg < NPAGES && PAGES[pg].graph == GR_ARP && trk[0].p[P_AMODE] == lk_p[0][P_AMODE])
                    arp_fx++;
            }
    bad += check("#39 FX / GLO / SCL / EDIT + KNOB 1..4, let go while turning: no page under the layer changes",
                 !fails && arp_fx == 4u);
    /* the quiet time is short: KNOB 1 edits the page again 0.3 s after FX */
    ui_power_on();
    go_page(GR_ARP);
    {
        int16_t m = TSEL->p[P_AMODE];
        btn_down(B_FX); frame(); turn(EN_K1, 4); btn_up(B_FX); frame();
        frames(320);
        turn(EN_K1, 1);
        ok = TSEL->p[P_AMODE] == m + 1 && !perf_k[0] && str_eq(cur_page()->title, "ARP");
    }
    bad += check("  0.3 s after FX is let go KNOB 1 is ARP MODE again; a knob turned with FX opened no FX page", ok);
    /* a tap of FX with a knob turned in that frame: a combo, no FX page */
    ui_power_on();
    go_page(GR_ARP);
    btn_down(B_FX); frame(); lk_turn(1); btn_up(B_FX); frame(); frames(100);
    bad += check("  FX tapped while a knob turns: no FX page, no ARP change", str_eq(cur_page()->title, "ARP") &&
                 TSEL->p[P_AMODE] == 0);
    return bad;
}

/* Discussion #83: the layer lock. A double tap of FX / GLO / SCL / EDIT (within LY_DTAP_MS, both let go before HOLD)
 * opens the map with no button held, over the page the first tap left (put back); the keys and KNOB 1..4 act as held;
 * a tap closes it (no page), so does another page button (which opens its page), HOME, the menu, a dialog; OCT- puts
 * back as in the layer; a single tap, a peek, a combo and two slow taps are as before; the header shows the lock */
static int test_layer_lock(void)
{
    static const uint8_t LB[4] = {B_FX, B_GLO, B_SCL, B_EDIT};
    static const uint8_t LL[4] = {LAYER_FX, LAYER_GLO, LAYER_SCL, LAYER_EDIT};
    int bad = 0, ok, okk;
    uint32_t i, pg;
    ok = 1; okk = 1;
    for (i = 0; i < 4u; i++) {                          /* double tap on ENV: locked, ENV under it */
        ui_power_on();
        go_title("ENV"); frame();
        pg = ui.page;
        press(LB[i]);
        ok &= !ui.lock && !ui.layer;                    /* the first tap: its page at once */
        frames(64);
        press(LB[i]);
        frames(500);                                    /* (no button held, long past HOLD) */
        ok &= ui.lock == LL[i] && ui.layer == LL[i] && layer_locked() && !ui.home && ui.page == pg &&
              !fm1_in.buttons && ((layer_seen >> LL[i]) & 1u) && !msg_is("HOLD [GLO] QUICK");
        draw_head();
        okk &= layer_locked();
        press(LB[i]);                                   /* a tap: closed, no page */
        frames(16);
        okk &= !ui.lock && !ui.layer && !ui.home && ui.page == pg;
    }
    bad += check("#83 FX GLO SCL EDIT double tapped: the map stays open with no button held, over the page as it was", ok);
    bad += check("  a tap of the button closes it, no page opens", okk);
    /* the keys and KNOB 1..4 act as held */
    ui_power_on();
    go_title("ENV"); frame();
    press(B_GLO); press(B_GLO); frames(100);
    usb.config = 1;
    {
        uint32_t mo = mo_w;
        int16_t l0 = trk[0].p[P_LEVEL];
        key_down(black(1)); frame(); key_up(black(1)); frame();
        ok = trk[1].p[P_MUTE] == 1 && !gates() && mo_w == mo;
        turn(EN_K1, 3);
        ok &= trk[0].p[P_LEVEL] == l0 + 3 && str_eq(cur_page()->title, "ENV");
        press(B_OCTDN);                                 /* OCT-: the mix put back, still locked */
        ok &= !trk[1].p[P_MUTE] && trk[0].p[P_LEVEL] == l0 && msg_is("MIX PUT BACK") && ui.lock == LAYER_GLO;
    }
    bad += check("  GLO locked: a black key mutes (silent, no MIDI), KNOB 1 T1 LEVEL, OCT- puts back, still locked", ok);
    ui_power_on();
    go_title("ENV"); frame();
    press(B_FX); press(B_FX); frames(100);
    key_down(white(4)); frame();                        /* C4: LPF while held */
    ok = (perf_held & PF_BIT(PF_LPF)) && !gates();
    key_up(white(4)); frame();
    ok &= !(perf_held & PF_BIT(PF_LPF));
    turn(EN_K2, 20);
    ok &= perf_k[1] == 20;
    press(B_FX); frames(16);
    ok &= !ui.lock && !perf_k[1];                      /* (closed: the macros snap back, as FX let go) */
    bad += check("  FX locked: a key's effect while held, KNOB 2 CRUSH; closed: the macros snap back", ok);
    ui_power_on();
    go_title("ENV"); frame();
    press(B_SCL); press(B_SCL); frames(100);
    key_down(white(1)); frame(); key_up(white(1)); frame();   /* G3: ROOT G */
    ok = TSEL->p[P_ROOT] == 7 && !gates();
    turn(EN_K2, 1);
    ok &= TSEL->p[P_SCALE] == 1 && str_eq(cur_page()->title, "ENV");
    bad += check("  SCL locked: a key sets ROOT, KNOB 2 SCL, the page stays ENV", ok);
    ui_power_on();
    go_title("ENV"); frame();
    press(B_EDIT); press(B_EDIT); frames(100);
    turn(EN_K1, 1);
    ok = TSEL->eng_req != trk[1].eng_req || TSEL->eng_req != TRK_DEF[0][0];
    ok &= str_eq(cur_page()->title, "ENV") && ui.lock == LAYER_EDIT;
    key_down(white(LY_INIT)); frame(); key_up(white(LY_INIT)); frame();   /* INIT: the dialog closes it */
    ok &= ui.confirm == CF_INIT_SOUND && !ui.lock && !ui.layer;
    press(B_OCTDN);
    bad += check("  EDIT locked: KNOB 1 the engine; INIT's dialog closes the lock", ok);
    /* closing: another page button opens its page, HOME goes home, another layer's tap its page, the menu */
    ui_power_on();
    go_title("ENV"); frame();
    press(B_GLO); press(B_GLO); frames(100);
    press(B_LFO);
    ok = !ui.lock && !ui.layer && str_eq(cur_page()->title, "LFO");
    press(B_SCL); press(B_SCL); frames(100);
    ok &= ui.lock == LAYER_SCL && str_eq(cur_page()->title, "LFO");
    press(B_HOME); frames(16);
    ok &= !ui.lock && !ui.layer && ui.home;
    press(B_GLO); press(B_GLO); frames(100);
    press(B_SCL); frames(400);
    ok &= !ui.lock && !ui.layer && cur_page()->fam == FAM_SCL;
    press(B_FX); press(B_FX); frames(100);
    hold(B_HOME);
    ok &= ui.menu && !ui.lock && !ui.layer;
    hold(B_HOME);
    bad += check("  LFO, HOME, another layer's button, the menu close it (and act as always)", ok);
    ui_power_on();
    go_title("ENV"); frame();
    press(B_GLO); press(B_GLO); frames(100);
    btn_down(B_GLO); frames(500);                       /* its button held: the map stays (a peek), let go: closed */
    ok = !ui.lock && ui.layer == LAYER_GLO;
    btn_up(B_GLO); frame();
    ok &= !ui.layer && str_eq(cur_page()->title, "ENV");
    frames(400);
    {
        int16_t a = TSEL->p[P_ATK];
        turn(EN_K1, 1);
        ok &= TSEL->p[P_ATK] == a + 1;                  /* (the page's knobs again after the quiet time) */
    }
    bad += check("  its button held after the lock: a peek, let go closes it; KNOB 1 is ENV's again", ok);
    /* unchanged: a single tap, two slow taps, a tap and a peek, a tap and a combo, PLAY / REC in the lock */
    ui_power_on();
    go_page(GR_TRK); frame();
    press(B_GLO); frames(400);
    ok = str_eq(cur_page()->title, "GLOBAL") && !ui.lock;
    press(B_GLO); frames(400);
    ok &= str_eq(cur_page()->title, "SYSTEM") && !ui.lock && !ui.layer;
    press(B_GLO);
    btn_down(B_GLO); frames(500);
    ok &= ui.layer == LAYER_GLO && !ui.lock;
    btn_up(B_GLO); frame();
    ok &= !ui.layer && !ui.lock && cur_page()->graph == GR_TRK;
    press(B_SCL);
    lay_combo(B_SCL, white(2));
    key_up(white(2)); btn_up(B_SCL); frame(); frames(400);
    ok &= !ui.layer && !ui.lock && TSEL->p[P_ROOT] == 9 && cur_page()->fam == FAM_SCL;
    bad += check("  unchanged: a tap opens the page, slow taps cycle, a tap then a peek or a combo: no lock", ok);
    ui_power_on();
    go_page(GR_TRK); frame();
    press(B_GLO); frames(16);
    press(B_LFO); frames(16);
    press(B_GLO); frames(400);
    ok = !ui.lock && cur_page()->graph == GR_TRK;       /* (another button between the taps: two taps) */
    ui_power_on();
    go_title("ENV"); frame();
    press(B_FX); press(B_FX); frames(100);
    press(B_PLAY);
    ok &= song.playing || transport_req;
    ok &= ui.lock == LAYER_FX;
    stop_transport();
    bad += check("  another button between the taps: no lock; PLAY in a locked layer plays, the lock stays", ok);
    {   /* EDIT on STEP clears a step: two quick taps clear two (no lock) */
        track_t *t;
        ui_power_on();
        t = TSEL;
        my_steps(t);
        go_page(GR_ROLL); frame();
        ui.cursor = 0;
        press(B_EDIT); press(B_EDIT); frames(100);
        bad += check("  EDIT on STEP (it clears the step): two quick taps clear two steps, no lock",
                     !ui.lock && !ui.layer && ui.cursor == 2u && t->step[0].time == ST_REST);
    }
    return bad;
}

/* #40: KNOB 4's DEPTH rises to the right (it fell); MENU > FX LATCH ON: a key with FX turns its effect on, again off,
 * nothing held needed; the macros stay after FX; FX + OCT- all off; the menu turns them off; FX's LED shows them;
 * OFF (the default, and every older setting): as before */
static int test_fx_latch(void)
{
    int bad = 0, ok;
    uint32_t lpf = white(4), m1 = black(0);
    ui_power_on();
    btn_down(B_FX); frame();
    turn(EN_K4, 5);  ok = perf_k[3] == 0;                         /* DEPTH 100 %: right stays at 100 */
    turn(EN_K4, -30); ok &= perf_k[3] == 30;                      /* left: 70 % */
    turn(EN_K4, 10); ok &= perf_k[3] == 20;                       /* right: 80 % */
    key_down(white(8)); frame(); perf_begin(CTL);                 /* OCT UP: KNOB 4 is SHIMR, right is more */
    turn(EN_K4, 10); ok &= perf_k[3] == 30;
    key_up(white(8)); frame(); btn_up(B_FX); frame();
    bad += check("#40 FX + KNOB 4: DEPTH rises to the right, falls to the left; SHIMR (OCT UP) rises to the right",
                 ok && !perf_k[3]);
    /* OFF (the default; a byte older settings leave 0) */
    ui_power_on();
    ok = !(fx_latch & 1u);
    btn_down(B_FX); key_down(lpf); frame(); key_up(lpf); frame(); btn_up(B_FX); frame();
    ok &= !perf_latched && !perf_held;
    bad += check("  FX LATCH OFF by default: a key's effect ends when it is let go", ok);
    /* ON */
    ui_power_on();
    fx_latch = 1;
    frame();                                                      /* (ui_input hands the setting to the ISR) */
    btn_down(B_FX); key_down(lpf); frame(); key_up(lpf); frame();
    turn(EN_K2, 30);
    ok = perf_latched == PF_BIT(PF_LPF) && !perf_held && ui.layer == LAYER_FX && str_eq(layer_head(), "[FX] LATCH") &&
         (layer_leds() >> lpf) & 1u;
    btn_up(B_FX); frame();
    ok &= perf_latched == PF_BIT(PF_LPF) && perf_k[1] == 30 && !ui.layer;
    ok &= perf_begin(CTL) && (perf_act & PF_BIT(PF_LPF));                 /* it runs with nothing held */
    bad += check("  FX LATCH ON: FX + C4 latches LPF, CRUSH stays at 30 after FX is let go", ok);
    ui_leds();
    memset(led_pos, 0xFF, sizeof led_pos);
    led_pos[panel.btn[B_FX]] = 0u << 3 | 1u;
    go_home(); ui_leds();
    ok = (fm1_led[0] & 2u) != 0u;
    btn_down(B_FX); key_down(m1); frame(); key_up(m1); frame();   /* a mute latches too */
    ok &= perf_latched == (PF_BIT(PF_LPF) | PF_BIT(PF_M1));
    key_down(lpf); frame(); key_up(lpf); frame();                 /* C4 again: LPF off */
    ok &= perf_latched == PF_BIT(PF_M1);
    btn_up(B_FX); frame();
    bad += check("  the FX LED lit while something is on; a mute key latches; C4 again: LPF off", ok);
    btn_down(B_FX); frame(); turn(EN_K1, -40);
    ok = song.octave == 0 && oct_leds() == 1u;
    btn_down(B_OCTDN); frame(); btn_up(B_OCTDN); frame();
    ok &= !perf_latched && !perf_k[0] && !perf_k[1] && song.octave == 0 && msg_is("FX ALL OFF");
    btn_up(B_FX); frame();
    bad += check("  FX + OCT-: every latched effect and macro off (no octave)", ok);
    btn_down(B_FX); key_down(lpf); frame(); key_up(lpf); frame(); btn_up(B_FX); frame();
    hold(B_HOME);
    ok = ui.menu && !perf_latched;
    ui.menu_sel = MI_LATCH;
    press(B_OCTUP); ok &= !(fx_latch & 1u) && ui.menu;
    turn(EN_K1, 1); ok &= (fx_latch & 1u) != 0u;
    turn(EN_K1, -1); ok &= !(fx_latch & 1u);
    press(B_OCTDN);
    bad += check("  the menu turns the latched effects off; MENU > FX LATCH: OCT+ toggles, KNOB 1 right ON / left OFF",
                 ok && !ui.menu);
    {   /* kept with the settings, in the favorites record (an older one: OFF) */
        persist_t p;
        fx_latch = 1;
        settings_export(&p);
        fx_latch = 0;
        ok = settings_import(&p, (int)sizeof p) == 1 && (fx_latch & 1u);
        memset(&p.favorites, 0, sizeof p.favorites);
        p.magic = 0x50455233u;                                    /* PER3: no favorites */
        ok &= settings_import(&p, (int)(sizeof p - sizeof p.favorites)) == 2 && !(fx_latch & 1u);
        bad += check("  FX LATCH saved with the settings; settings from before it: OFF", ok);
    }
    fx_latch = 0;
    return bad;
}

/* #48: the note divisions step by their length, longest first (triplets between their neighbours), on every list
 * that has them (SEQ DIV, ARP RATE, the delay's TIME, the SLICER's RATE); the stored values and their sound stay */
static int test_div_order(void)
{
    int bad = 0, ok = 1;
    uint32_t id, c, k, found = 0;
    static const param_desc_t *const LISTS[] = {&TP[P_SDIV], &TP[P_ARATE], &GP[G_DTIME], &TP[P_SLRATE]};
    ui_power_on();
    for (k = 0; k < NELEM(LISTS); k++) {           /* left end -> right end: each one shorter than the one before */
        const param_desc_t *d = LISTS[k];
        int32_t v = d->def, n, seen = 0;
        uint32_t prev = 0xFFFFFFFFu, len;
        for (n = 0; n < 40; n++) v = param_turn(d, v, -1);
        for (n = 0; n <= d->max - d->min; n++) {
            len = d->names == N_SLDIV ? 1000000u / SL_DEN[v] : div_samples((uint32_t)v);
            if (!(len < prev && enum_rank(d, v) == d->min + n && !((seen >> v) & 1))) {
                printf("  %s: %s (value %d, place %d, length %u) after %u\n", d->label, d->names[v], (int)v,
                       (int)enum_rank(d, v), len, prev);
                ok = 0;
            }
            seen |= 1 << v;
            prev = len;
            v = param_turn(d, v, 1);
        }
        ok &= seen == (1 << (d->max - d->min + 1)) - 1;
    }
    ok &= str_eq(N_DIV[2], "1/16") && str_eq(N_DIV[4], "8T") && str_eq(N_DIV[9], "4BAR") && TP[P_SDIV].def == 2;
    ok &= param_turn(&TP[P_SDIV], 2, 1) == 5 && param_turn(&TP[P_SDIV], 2, -1) == 4 &&   /* 1/16: right 16T, left 8T */
          param_turn(&TP[P_SDIV], 3, 1) == 3 && param_turn(&TP[P_SDIV], 9, -1) == 9 &&   /* the ends hold */
          param_turn(&TP[P_SDIV], 2, -6) == 8 && param_turn(&TP[P_SDIV], 2, 40) == 3;    /* 6 left: 2BAR; far: clamped */
    bad += check("#48 DIV / RATE / TIME / SLICER RATE step longest -> shortest, every value once, the ends hold", ok);
    ok = 1;                                         /* the real knob on the pages that show them, the gauge follows */
    for (id = 0; id < NPAGES; id++)
        for (c = 0; c < 4u; c++) {
            int16_t *vp;
            const param_desc_t *d;
            ui_power_on();
            ui.home = 0; ui.page = (uint8_t)id; page_entered();
            if (!page_visible(id) || !(d = page_desc(cur_page(), c, &vp)) || !vp || !enum_order(d))
                continue;
            found++;
            *vp = d->names == N_DIV ? 2 : 1;        /* 1/16 */
            turn(EN_K1 + c, 1);
            ok &= *vp == (d->names == N_DIV ? 5 : 4);  /* 16T */
            turn(EN_K1 + c, -2);
            ok &= *vp == (d->names == N_DIV ? 4 : 3);  /* 8T */
        }
    bad += check("#48 the pages' knobs step the divisions by length (SEQ, ARP, DLY, SLICER)", ok && found >= 4u);
    return bad;
}

/* #52 MENU > KNOB ACCEL (OFF by default, #23): ON, a sustained fast turn of a wide value goes x2..x4; a slow turn, the
 * start of a turn, a lone quick detent and a reversal are one step per detent; the sign is always the detents' */
static uint32_t acc_ms(uint32_t role, const uint16_t *gaps, uint32_t n, int32_t dir, int32_t range, int32_t *out)
{
    uint32_t k, sum = 0;
    for (k = 0; k < n; k++) {
        fm1_ms += gaps[k];
        out[k] = accel(role, dir, range);
        sum += (uint32_t)(out[k] < 0 ? -out[k] : out[k]);
    }
    return sum;
}
static int test_knob_accel(void)
{
    int bad = 0, ok;
    uint32_t k, seed = 12345u;
    int32_t o[16];
    static const uint16_t SLOW[8] = {500, 60, 60, 45, 41, 60, 80, 60}, MID[8] = {500, 30, 30, 30, 26, 30, 30, 30},
                          F20[8] = {500, 20, 20, 20, 20, 20, 20, 20}, F10[8] = {500, 10, 10, 10, 10, 10, 10, 10},
                          BOUNCE[8] = {500, 2, 300, 1, 300, 3, 300, 2};
    ui_power_on();
    ok = acc_ms(EN_K1, F10, 8, 1, 127, o) == 8u;                  /* OFF (the default) */
    ui_prefs = PREF_ACCEL;
    memset(ui.enc_t, 0, sizeof ui.enc_t);
    ok &= acc_ms(EN_K1, SLOW, 8, 1, 127, o) == 8u && acc_ms(EN_K2, MID, 8, -1, 127, o) == 8u && o[7] == -1;
    ok &= acc_ms(EN_K3, BOUNCE, 8, 1, 127, o) == 8u;
    bad += check("#52 KNOB ACCEL: OFF by default; ON, slow (>= 26 ms a detent) and lone quick detents: 1 step each", ok);
    fm1_ms += 1000;
    acc_ms(EN_K1, F20, 8, 1, 127, o);
    ok = o[0] == 1 && o[1] == 1 && o[2] == 2 && o[7] == 2;          /* 20 ms a detent: x2 from the third */
    acc_ms(EN_K1, F10, 8, 1, 127, o);                              /* (a pause of 500 ms first: a new turn) */
    ok &= o[0] == 1 && o[1] == 1 && o[2] == 4 && o[7] == 4;         /* 10 ms: x4 */
    fm1_ms += 15; ok &= accel(EN_K1, 1, 127) == 3;                 /* 15 ms: x3 */
    fm1_ms += 10; ok &= accel(EN_K1, -1, 127) == -1;               /* reversed: one step, the right sign */
    fm1_ms += 10; ok &= accel(EN_K1, -1, 127) == -1;               /* (the turn starts again) */
    fm1_ms += 10; ok &= accel(EN_K1, -1, 127) == -4;
    fm1_ms += 30; ok &= accel(EN_K1, -3, 127) == -12;              /* 3 detents in one read (10 ms each): x4 */
    bad += check("#52 KNOB ACCEL ON: 20 ms a detent x2, 15 ms x3, 10 ms x4 from a turn's third detent; reversal x1", ok);
    fm1_ms += 1000; acc_ms(EN_K4, F10, 8, 1, 32, o);
    ok = o[7] == 1;                                                /* a narrow range (<= 32): never */
    fm1_ms += 1000; acc_ms(EN_K4, F10, 8, 1, 0, o);
    ok &= o[7] == 1;                                               /* a list of names (range 0 at the callers) */
    for (k = 0; k < 20000u && ok; k++) {                           /* random turns: the sign is the detents', at */
        int32_t s, r, gap;                                         /* most x4, a slow detent exactly one step */
        seed = seed * 1103515245u + 12345u;
        s = (int32_t)((seed >> 16) % 7u) - 3;
        gap = (int32_t)((seed >> 8) % 90u);
        fm1_ms += (uint32_t)gap;
        r = accel(EN_K1 + (seed & 3u), s, 127);
        ok &= s ? (r > 0) == (s > 0) && r * s <= 4 * s * s && r * s >= s * s : r == 0;
        if (gap >= 40 * (s < 0 ? -s : s) && r != s) ok = 0;
    }
    bad += check("#52 KNOB ACCEL: never on narrow values or lists; 20000 random reads keep the sign, x1..x4, slow = x1", ok);
    {   /* the real knob: LEVEL on MIXER, a detent a frame (16 ms): 1 + 1 + 3 x 6 = 20 for 8 detents; OFF: 8 */
        int16_t v0;
        ui_power_on(); ui_prefs = PREF_ACCEL;
        go_title("MIXER");
        TSEL->p[P_LEVEL] = 40; v0 = TSEL->p[P_LEVEL];
        fm1_ms += 1000;
        for (k = 0; k < 8u; k++) turn(EN_K1, 1);
        ok = TSEL->p[P_LEVEL] - v0 == 20;
        ui_prefs = 0; TSEL->p[P_LEVEL] = 40; fm1_ms += 1000;
        for (k = 0; k < 8u; k++) turn(EN_K1, 1);
        ok &= TSEL->p[P_LEVEL] == 48;
        bad += check("#52 KNOB ACCEL on MIXER LEVEL: a fast turn of 8 detents moves 20 (OFF: 8)", ok);
    }
    {   /* saved with the settings; settings from before it: OFF */
        persist_t p;
        ui_prefs = PREF_ACCEL;
        settings_export(&p);
        ui_prefs = 0;
        ok = settings_import(&p, (int)sizeof p) == 1 && ui_prefs == PREF_ACCEL;
        memset(&p.favorites, 0, sizeof p.favorites);
        p.magic = 0x50455233u;
        ok &= settings_import(&p, (int)(sizeof p - sizeof p.favorites)) == 2 && !ui_prefs;
        bad += check("#52 KNOB ACCEL saved with the settings; settings from before it: OFF", ok);
    }
    ui_prefs = 0;
    return bad;
}

/* #51 ("a melody appears when I change presets"): browsing every sound in the list (every engine's presets, then user
 * presets, one of them holding a pattern), on HOME and the PRESETS page, stopped and playing, on an empty track: no
 * step is written, the ARP stays off, nothing is held, the transport is not started, no message about a pattern. (In
 * 0.9 a preset carried a 16-step pattern and replaced the steps; since 1.0 only SEQ > PHRASES loads one.) */
static int test_browse_no_pattern(void)
{
    int bad = 0, ok = 1;
    uint32_t pass, n, total, loads = 0, pat_hints = 0;
    track_t *t;
    for (pass = 0; pass < 4u; pass++) {
        ui_power_on();
        t = TSEL;
        my_steps(t); t->p[P_SDIV] = 2;
        up_store(3, "MINE");                          /* a user preset that holds a pattern */
        up_store(5, "PLAIN");
        track_defaults_steps(t);
        ok &= seq_is_empty(t) && up_has_pat(3);
        if (pass & 1u) song.playing = 1;              /* playing: the other tracks run, this one is empty */
        if (pass & 2u) go_page(GR_BROWSE);
        preset_pos(&total);
        for (n = 0; n < total + 2u; n++) {
            turn(EN_PRESET, 1);
            loads++;
            pat_hints += preset_pat_hint() >= 0;
            if (!seq_is_empty(t) || t->p[P_AMODE] || t->nheld || (!(pass & 1u) && (song.playing || transport_req))) {
                printf("  pass %u, sound %u (%s %u): steps %d ARP %d held %u playing %d\n", pass, n,
                       ENGINES[t->eng_req % NENGINES]->name, t->preset, !seq_is_empty(t), t->p[P_AMODE], t->nheld,
                       song.playing);
                ok = 0;
                break;
            }
        }
    }
    if (!ok || loads < 200u || pat_hints <= 10u) printf("  ok %d loads %u hints %u\n", ok, loads, pat_hints);
    bad += check("#51 every sound browsed (HOME, PRESETS; stopped, playing): no step, ARP off, nothing held, no start",
                 ok && loads >= 200u && pat_hints > 10u);
    return bad;
}

/* MENU > USB LEVEL: MASTER (the default, every older setting) / FIXED; the audio follows at once (audio_test: the levels) */
static int test_usb_level(void)
{
    int bad = 0, ok;
    ui_power_on();
    frame();
    ok = !fx_usb_fixed && str_eq(MI_NAME[MI_USB], "USB LEVEL") && str_eq(menu_flag(MI_USB)->name[0], "MASTER");
    hold(B_HOME);
    ui.menu_sel = MI_USB;
    ui.force = 1; ui_draw();
    turn(EN_K1, 1); ok &= (ui_prefs & PREF_USB_FIXED) && fx_usb_fixed;      /* right: FIXED */
    turn(EN_K1, -1); ok &= !(ui_prefs & PREF_USB_FIXED) && !fx_usb_fixed;   /* left: MASTER */
    press(B_OCTUP); ok &= (ui_prefs & PREF_USB_FIXED) && fx_usb_fixed && ui.menu;
    press(B_OCTDN);
    ok &= !ui.menu && fx_usb_fixed;
    {
        persist_t p;
        settings_export(&p);
        ui_prefs = 0;
        ok &= settings_import(&p, (int)sizeof p) == 1 && ui_prefs == PREF_USB_FIXED;
        memset(&p.favorites, 0, sizeof p.favorites);
        p.magic = 0x50455233u;
        ok &= settings_import(&p, (int)(sizeof p - sizeof p.favorites)) == 2 && !ui_prefs;
        frame();
        ok &= !fx_usb_fixed;
    }
    bad += check("MENU > USB LEVEL: MASTER by default, KNOB 1 right FIXED / left MASTER, OCT+ toggles, saved, the audio follows", ok);
    ui_prefs = 0;
    frame();
    return bad;
}

/* #67 MENU > USB SERIAL: ON by default (and in every older setting): the console presented, the descriptors as
 * before. OFF: audio + MIDI only, device class 0, bcdUSB 1.10 (CFG_DESC_PLAIN: a FELUCCA_CDC=0 build's bytes), applied
 * when the menu closes (one re-enumeration); saved; at boot the saved choice before USB starts (never on then off) */
static int dev_class(void)                              /* the device class GET_DESCRIPTOR sends now (-1: none) */
{
    const uint8_t *d;
    uint16_t n;
    return get_desc(0x0100, &d, &n) && n == 18u ? d[4] : -1;
}
static int dev_is(const uint8_t *want)                  /* the device descriptor sent now = want */
{
    const uint8_t *d;
    uint16_t n;
    return get_desc(0x0100, &d, &n) && n == 18u && !memcmp(d, want, 18);
}
static int plain_desc(void)                             /* device + configuration = the console-less bytes */
{
    const uint8_t *d, *c;
    uint16_t n, m;
    return get_desc(0x0100, &d, &n) && get_desc(0x0200, &c, &m) && n == 18u && !memcmp(d, DEV_DESC_PLAIN, 18) &&
           m == sizeof CFG_DESC_PLAIN && !memcmp(c, CFG_DESC_PLAIN, m);
}
static int test_usb_serial(void)
{
    int bad = 0, ok;
    ui_power_on();
    usb.up = 0;                                          /* (no SIE here: usb_cdc_switch only flips the descriptors) */
    frame();
    ok = usb_cdc_on && dev_class() == 0xEF && !(ui_prefs & PREF_SERIAL_OFF) && str_eq(MI_NAME[MI_SERIAL], "USB SERIAL") &&
         str_eq(menu_flag(MI_SERIAL)->name[0], "ON") && str_eq(menu_flag(MI_SERIAL)->name[1], "OFF");
    ok &= dev_is(DEV_DESC);
    bad += check("#67 MENU > USB SERIAL: ON by default, the console presented (misc / IAD, as before)", ok);
    hold(B_HOME);
    ui.menu_sel = MI_SERIAL;
    ui.force = 1; ui_draw();
    turn(EN_K1, 1);
    ok = (ui_prefs & PREF_SERIAL_OFF) && usb_cdc_on;    /* right: OFF; applied when the menu closes */
    turn(EN_K1, -1); ok &= !(ui_prefs & PREF_SERIAL_OFF);
    press(B_OCTUP); ok &= (ui_prefs & PREF_SERIAL_OFF) && ui.menu && usb_cdc_on;
    press(B_OCTDN);
    ok &= ui.menu == 0 && usb_cdc_on;                 /* (OCT- acts as it is let go: closed this frame) */
    frame();
    ok &= !ui.menu && !usb_cdc_on && !usb.config && plain_desc() && dev_class() == 0;
    bad += check("  KNOB 1 right OFF / left ON, OCT+ toggles; the menu closed: audio + MIDI only, class 0", ok);
    {
        persist_t p;
        settings_export(&p);
        ui_prefs = 0;
        ok = settings_import(&p, (int)sizeof p) == 1 && ui_prefs == PREF_SERIAL_OFF;
        ok &= settings_import(&p, (int)sizeof p) == 1 && ui_prefs == PREF_SERIAL_OFF;     /* (again: the same) */
        /* boot (main.c): the settings, usb_serial_apply, then usb_start: the console-less descriptors from the start */
        usb_cdc_on = 1;
        usb.up = 0;
        usb_serial_apply();
        ok &= !usb_cdc_on && !usb.up && plain_desc();
        memset(&p.favorites, 0, sizeof p.favorites);   /* settings from before it (PER3): ON */
        p.magic = 0x50455233u;
        ok &= settings_import(&p, (int)(sizeof p - sizeof p.favorites)) == 2 && !(ui_prefs & PREF_SERIAL_OFF);
        usb_serial_apply();
        ok &= usb_cdc_on && dev_is(DEV_DESC);
    }
    bad += check("  saved with the settings (older ones: ON); at boot the device enumerates with the saved choice", ok);
    {   /* OFF: the soft key and the update frames still come in (USB-MIDI SysEx, EP1) */
        static const uint8_t KEY[8] = {0x04, 0xF0, 0x22, 0x24, 0x07, 0x35, 0x7D, 0xF7};
        ui_prefs = PREF_SERIAL_OFF;
        frame();
        usb.uboot_req = 0;
        ok = !usb_cdc_on && ep1_take(KEY, sizeof KEY) && usb.uboot_req;
        usb.uboot_req = 0;
        bad += check("  OFF: the soft key (SysEx on the MIDI endpoint) still asks for UPDATE MODE", ok);
    }
    ui_prefs = 0;
    frame();
    return bad;
}

/* #58 MENU > BPM LOCK: OFF by default (and in every older setting): SELECT sets the tempo on every page. ON: SELECT
 * leaves it on the pages ("BPM LOCKED" once per turn burst); GLO > GLOBAL's BPM knob, GLO + SELECT (a combo: the map
 * opens, no GLO page; OCT- puts it back) and GLO's F4 TAP still set it; the menu never takes SELECT as the tempo */
static int test_bpm_lock(void)
{
    int bad = 0, ok;
    int16_t b0;
    uint32_t i, page;
    ui_power_on();
    ui_prefs = 0;
    frame();
    b0 = song.g[G_BPM];
    turn(EN_SELECT, 2);
    ok = song.g[G_BPM] == b0 + 2 && !msg_is("BPM LOCKED") && str_eq(menu_flag(MI_BPMLOCK)->name[0], "OFF");
    bad += check("#58 BPM LOCK OFF (the default): SELECT sets the tempo as before", ok);

    hold(B_HOME);
    ui.menu_sel = MI_BPMLOCK;
    ui.force = 1; ui_draw();
    b0 = song.g[G_BPM];
    turn(EN_SELECT, 3);
    ok = str_eq(MI_NAME[MI_BPMLOCK], "BPM LOCK") && ui.menu == 1u && song.g[G_BPM] == b0;   /* (the menu: no tempo) */
    turn(EN_K1, 1); ok &= (ui_prefs & PREF_BPM_LOCK) != 0u;          /* right: ON */
    turn(EN_K1, -1); ok &= !(ui_prefs & PREF_BPM_LOCK);              /* left: OFF */
    press(B_OCTUP); ok &= (ui_prefs & PREF_BPM_LOCK) && ui.menu;      /* OCT+ toggles */
    ok &= ui_prefs == PREF_BPM_LOCK;                                  /* (no other setting moved) */
    press(B_OCTDN);
    bad += check("#58 MENU > BPM LOCK: KNOB 1 right ON / left OFF, OCT+ toggles; SELECT in the menu is no tempo", ok && !ui.menu);

    /* ON: no tempo from SELECT on HOME and the pages; "BPM LOCKED" once per burst */
    ok = 1;
    for (i = 0; i < 3u; i++) {
        if (i == 1u) go_title("ENV");
        if (i == 2u) go_title("STEP");
        frame();
        ui.msg_t = 0; fm1_ms += 1200u;
        b0 = song.g[G_BPM];
        turn(EN_SELECT, 1);
        ok &= song.g[G_BPM] == b0 && msg_is("BPM LOCKED") && !ui.bpm_t;
        ui.msg_t = 0;
        turn(EN_SELECT, -4);                                          /* (the same burst: said once) */
        ok &= song.g[G_BPM] == b0 && !ui.msg_t;
    }
    fm1_ms += 1200u;
    turn(EN_SELECT, 1);
    ok &= msg_is("BPM LOCKED");                                       /* (a new burst: said again) */
    ok &= text_w(&AF_S, "BPM LOCKED") <= 236 - 106;                   /* (fits the header's message zone) */
    bad += check("#58 BPM LOCK ON: SELECT leaves the tempo on HOME and the pages, \"BPM LOCKED\" once per turn burst", ok);

    /* ON: GLO > GLOBAL's BPM knob */
    go_title("GLOBAL"); frame();
    b0 = song.g[G_BPM];
    turn(EN_K1, 1);
    ok = str_eq(cur_page()->title, "GLOBAL") && cur_page()->id[0] == G_BPM && song.g[G_BPM] == b0 + 1;
    bad += check("#58 BPM LOCK ON: GLO > GLOBAL KNOB 1 still sets the tempo", ok);

    /* ON: GLO + SELECT (a quick turn: a combo, no GLO page), OCT- puts it back, F4 TAP */
    go_title("ENV"); frame();
    page = ui.page;
    b0 = song.g[G_BPM];
    ui.msg_t = 0;
    btn_down(B_GLO); frame();
    turn(EN_SELECT, 3);
    ok = song.g[G_BPM] == b0 + 3 && ui.bpm_t && !msg_is("BPM LOCKED") && ui.layer == LAYER_GLO;
    btn_up(B_GLO); frame();
    ok &= ui.page == page && !ui.home;                                /* (no tap: still ENV) */
    btn_down(B_GLO); frame();
    turn(EN_SELECT, -5);
    ok &= song.g[G_BPM] == b0 - 2;
    oct_back();
    ok &= song.g[G_BPM] == b0 + 3;                                    /* (OCT-: the tempo as this layer opened) */
    btn_up(B_GLO); frame();
    bad += check("#58 BPM LOCK ON: GLO + SELECT sets the tempo (a combo: the map, no GLO page; OCT- puts it back)",
                 ok && ui.page == page);
    song.g[G_BPM] = 100;
    btn_down(B_GLO); frame();
    for (i = 0; i < 3u; i++) { key_down(white(7)); frame(); key_up(white(7)); frames(480); }
    ok = song.g[G_BPM] == 120;
    btn_up(B_GLO); frame();
    bad += check("#58 BPM LOCK ON: GLO + F4 TAP still sets the tempo", ok);

    {   /* saved with the settings; settings from before it: OFF (and so again: idempotent) */
        persist_t p, q;
        settings_export(&p);
        ui_prefs = 0;
        ok = settings_import(&p, (int)sizeof p) == 1 && ui_prefs == PREF_BPM_LOCK;
        q = p;
        ok &= settings_import(&q, (int)sizeof q) == 1 && ui_prefs == PREF_BPM_LOCK && !memcmp(&p, &q, sizeof p);
        memset(&p.favorites, 0, sizeof p.favorites);
        p.magic = 0x50455233u;                                        /* (PER3: before the favorites record) */
        ok &= settings_import(&p, (int)(sizeof p - sizeof p.favorites)) == 2 && !ui_prefs;
        q = p;
        ok &= settings_import(&q, (int)sizeof q) == 1 && !ui_prefs;
        frame();
        b0 = song.g[G_BPM];
        go_home(); frame();
        turn(EN_SELECT, 1);
        ok &= song.g[G_BPM] == b0 + 1;
        bad += check("#58 BPM LOCK saved with the settings; settings from before it: OFF (idempotent)", ok);
    }
    ui_prefs = 0;
    ui_power_on(); frame();
    return bad;
}

/* #15 / Discussion #80 MENU > LARGE: OFF by default (and in every older setting); KNOB 1 right ON / left OFF, OCT+
 * toggles, nothing else moves. ON: HOME and the value pages draw tall cards (a K1..K4 pill over each knob's column,
 * the value in L) and the strip under them; the list / graph pages and the layers keep the layout (their labels in
 * M); the footer stays; a knob turned redraws its own card only; saved with the settings, older settings OFF
 * (idempotent) */
static uint16_t lg_px(int32_t x, int32_t y) { return swap16(host_screen[y * 240 + x]); }
static int lg_stone(uint16_t c) { return c == T_SURF || c == T_STONE; }   /* GHOULBOX: a window's stone (SURF + specks) */
static int lg_value_rows(uint32_t c)                    /* rows of card c's value band holding the value's colour */
{
    int32_t x, y, n = 0;
    for (y = Y_LABEL + 40; y < Y_LABEL + 76; y++)
        for (x = CARD_X(c) + 3; x < CARD_X(c) + CARD_W - 3; x++)   /* (GHOULBOX: inside the window's THEME border) */
            if (lg_px(x, y) == T_THEME) { n++; break; }
    return n;
}
static int test_large(void)
{
    int bad = 0, ok;
    uint32_t c;
    static uint16_t off[240 * 240];
    ui_power_on();
    song.playing = 0;
    frame();
    ok = !(ui_prefs & PREF_LARGE) && large_kind() == LK_OFF && card_h() == CARD_H && graph_y() == Y_GRAPH;
    ok &= MI_LARGE == MI_STYLE + 1u && str_eq(MI_NAME[MI_LARGE], "LARGE") && menu_flag(MI_LARGE) &&
          str_eq(menu_flag(MI_LARGE)->name[0], "OFF") && str_eq(menu_flag(MI_LARGE)->name[1], "ON");
    ui.force = 1; frame();
    memcpy(off, host_screen, sizeof off);
    hold(B_HOME); ui.menu_sel = MI_LARGE;
    ok &= ui.menu == 1u;
    turn(EN_K1, 1); ok &= (ui_prefs & PREF_LARGE) != 0u;          /* right: ON */
    turn(EN_K1, -1); ok &= !(ui_prefs & PREF_LARGE);              /* left: OFF */
    press(B_OCTUP); ok &= (ui_prefs & PREF_LARGE) && ui.menu;     /* OCT+ toggles */
    ok &= ui_prefs == PREF_LARGE && ui_style == ST_FLAT;          /* (no other setting moved) */
    press(B_OCTDN);
    bad += check("#15 MENU > LARGE: OFF by default, under STYLE; KNOB 1 right ON / left OFF, OCT+ toggles", ok && !ui.menu);

    /* HOME: tall cards (rows 28..132) and the strip (136..198); the footer as it was */
    frame();
    ok = ui.home && large_kind() == LK_TALL && card_h() == LG_CARD_H && graph_y() == LG_Y_GRAPH && strip_kind() == SK_SCOPE;
    ok &= lg_stone(lg_px(30, Y_SEP_END + 1)) && off[(Y_SEP_END + 1) * 240 + 30] == swap16(T_BG);   /* a card where the gap was */
    ok &= lg_px(30, LG_Y_GRAPH - 2) == T_BG && lg_stone(swap16(off[(LG_Y_GRAPH - 2) * 240 + 30]));   /* a gap in the panel */
    ok &= lg_stone(lg_px(30, LG_Y_GRAPH + 1));                    /* (GHOULBOX: the window's inner STONE line) */
    for (c = 0; c < 4u; c++)                                      /* each column's K pill, over its knob */
        ok &= lg_px(CARD_X(c) + 8, Y_LABEL + LG_MY + LG_MH / 2) == T_KEY;
    ok &= !memcmp(off + Y_FOOT * 240, host_screen + Y_FOOT * 240, 240u * H_FOOT * sizeof *off);
    bad += check("#15 LARGE ON: HOME's cards tall (a K1..K4 pill over each column), the scope a strip, the footer kept", ok);

    /* the value in L: GLOBAL's BPM 124 stands about twice as tall as in M */
    go_title("GLOBAL"); frame();
    ok = large_kind() == LK_TALL && strip_kind() == SK_TITLE && cur_page()->id[0] == G_BPM && lg_value_rows(0) >= AF_L_CAP_H - 3;
    ui_prefs = 0; ui.force = 1; frame();
    ok &= lg_value_rows(0) <= AF_M_CAP_H + 2;
    ui_prefs = PREF_LARGE; ui.force = 1; frame();
    bad += check("#15 LARGE: a card value in L (GLOBAL's BPM 120: about 21 rows of figures, M's 11)", ok);

    /* a knob turned: its own card redrawn, the other three not */
    go_title("ENV"); frame(); frames(600);
    memcpy(off, host_screen, sizeof off);
    turn(EN_K2, 3);
    {
        int32_t x, y, diff[4] = {0, 0, 0, 0};
        for (y = Y_LABEL; y < Y_LABEL + LG_CARD_H; y++)
            for (c = 0; c < 4u; c++)
                for (x = CARD_X(c); x < CARD_X(c) + CARD_W; x++)
                    diff[c] += off[y * 240 + x] != host_screen[y * 240 + x];
        ok = large_kind() == LK_TALL && strip_kind() == SK_ADSR && diff[1] && !diff[0] && !diff[2] && !diff[3];
    }
    bad += check("#15 LARGE: ENV tall with its ADSR in the strip; KNOB 2 turned redraws card 2 only", ok);

    /* per page type: lists and rolls keep the layout, the layers too; MIXER's tracks in the strip */
    go_page(GR_BROWSE); frame();
    ok = large_kind() == LK_LABEL && card_h() == CARD_H && graph_y() == Y_GRAPH && graph_h() == H_GRAPH;
    go_page(GR_ROLL); frame();
    ok &= large_kind() == LK_LABEL;
    go_page(GR_TRK); frame();
    ok &= large_kind() == LK_TALL && strip_kind() == SK_TRK && lg_px(CARD_X(0) + 8, LG_Y_GRAPH + 56) == T_SURF;
    go_title("ENV"); frame();
    btn_down(B_GLO); frames(800);
    ok &= ui.layer && large_kind() == LK_LABEL && card_h() == CARD_H;
    btn_up(B_GLO); frame(); frame();
    ok &= !ui.layer && large_kind() == LK_TALL && lg_px(30, LG_Y_GRAPH - 2) == T_BG;
    bad += check("#15 LARGE: PRESETS and the roll keep the layout (labels in M), the layers too; MIXER tall, its tracks in the strip", ok);

    {   /* saved with the settings; settings from before it: OFF (and so again: idempotent) */
        persist_t p, q;
        settings_export(&p);
        ui_prefs = 0;
        ok = settings_import(&p, (int)sizeof p) == 1 && ui_prefs == PREF_LARGE;
        q = p;
        ok &= settings_import(&q, (int)sizeof q) == 1 && ui_prefs == PREF_LARGE && !memcmp(&p, &q, sizeof p);
        memset(&p.favorites, 0, sizeof p.favorites);
        p.magic = 0x50455233u;                                    /* (PER3: before the favorites record) */
        ok &= settings_import(&p, (int)(sizeof p - sizeof p.favorites)) == 2 && !ui_prefs;
        q = p;
        ok &= settings_import(&q, (int)sizeof q) == 1 && !ui_prefs;
        ui.force = 1; frame();
        ok &= large_kind() == LK_OFF && lg_px(30, Y_SEP_END + 1) == T_BG;
        bad += check("#15 LARGE saved with the settings; settings from before it: OFF (idempotent)", ok);
    }
    ui_prefs = 0;
    ui_power_on(); frame();
    return bad;
}

/* MENU: the list scrolls (the selected row in the middle, every row reachable, the rows shown in order, the scroll
 * bar); #46 MENU > ANIM: ON by default (and in every older setting), OFF snaps the rolling digits and the piano roll */
static int test_menu_prefs(void)
{
    int bad = 0, ok = 1;
    uint32_t k, seen = 0;
    ui_power_on();
    ui_prefs = 0;
    hold(B_HOME);
    ui.menu_sel = 0;
    for (k = 0; k < 2u * MI_COUNT; k++) {          /* PRESETS right: every row once, wrapping; the selected one shown */
        uint32_t t = menu_top();
        seen |= 1u << ui.menu_sel;
        ok &= ui.menu_sel >= t && ui.menu_sel < t + MENU_VIS && t + MENU_VIS <= (MI_COUNT > MENU_VIS ? MI_COUNT : MENU_VIS);
        ui.force = 1; ui_draw();
        turn(EN_PRESET, 1);
    }
    ok &= seen == (1u << MI_COUNT) - 1u && ui.menu_sel == 0u && menu_top() == 0u;
    turn(EN_PRESET, -1);
    ok &= ui.menu_sel == MI_COUNT - 1u && menu_top() == (MI_COUNT > MENU_VIS ? MI_COUNT - MENU_VIS : 0u);
    bad += check("MENU: PRESETS reaches every row (wrapping), the selected row always shown, the list scrolls", ok);
    {   /* scrolled to the end: the last row drawn at the bottom slot, the first one gone */
        uint16_t endshot[240 * 240];
        ui.force = 1; ui_draw();
        memcpy(endshot, host_screen, sizeof endshot);
        ui.menu_sel = 0; ui.force = 1; ui_draw();
        ok = MI_COUNT <= MENU_VIS || memcmp(endshot, host_screen, sizeof endshot);
        bad += check("MENU: scrolled down, the screen shows other rows than at the top", ok);
    }
    /* ANIM */
    ui.menu_sel = MI_ANIM;
    ok = str_eq(MI_NAME[MI_ANIM], "ANIM") && !(ui_prefs & PREF_ANIM_OFF);
    turn(EN_K1, 1); ok &= (ui_prefs & PREF_ANIM_OFF) != 0;        /* right: OFF */
    turn(EN_K1, -1); ok &= !(ui_prefs & PREF_ANIM_OFF);           /* left: ON (the default) */
    press(B_OCTUP); ok &= (ui_prefs & PREF_ANIM_OFF) && ui.menu;  /* OCT+ toggles */
    ok &= !(ui_prefs & ~(uint32_t)PREF_ANIM_OFF);                 /* (no other setting moved) */
    press(B_OCTDN);
    bad += check("#46 MENU > ANIM: ON by default, KNOB 1 right OFF / left ON, OCT+ toggles", ok && !ui.menu);
    ui_prefs = 0;
    ui_power_on(); roll_settle();
    turn(EN_K4, 1);
    ok = song.g[G_TUNE] == 1 && ui.roll[3].from[0];              /* ON: TUNE 0 -> 1 rolls */
    ui_power_on(); ui_prefs = PREF_ANIM_OFF; roll_settle();      /* (ui_power_on clears the favorites record) */
    turn(EN_K4, 1);
    ok &= song.g[G_TUNE] == 1 && !ui.roll[3].from[0] && roll_static_now();   /* OFF: the value at once, static */
    turn(EN_SELECT, 1);
    ok &= !ui.roll[ROLL_BPM].from[0];
    bad += check("#46 ANIM OFF: the card values and the header BPM snap (no rolling digits), the frame is static", ok);
    {   /* the piano roll's view: ON glides toward the notes over frames, OFF jumps there at once */
        track_t *t = TSEL;
        uint32_t lo_on;
        ui_power_on();
        for (k = 0; k < NSTEP; k++) step_clear(&t->step[k]);
        t->step[0].n = 1; t->step[0].note[0] = 30; t->step[0].time = ST_NOTE;
        ui.force = 0;
        proll.init = 0; pr_follow(t); ui.frame++;
        t->step[0].note[0] = 90; pr_follow(t); lo_on = proll.lo;
        ui_prefs = PREF_ANIM_OFF;
        proll.init = 0; t->step[0].note[0] = 30; pr_follow(t); ui.frame++;
        t->step[0].note[0] = 90; pr_follow(t);
        ok = proll.moving == 0 && proll.lo > lo_on && lo_on != 0u;
        if (!ok) printf("  piano roll view: ON %u, OFF %u (moving %u)\n", lo_on, proll.lo, proll.moving);
        bad += check("#46 ANIM OFF: the piano roll's view jumps to the notes (ON: it glides)", ok);
    }
    {   /* saved with the settings; settings from before it: ON */
        persist_t p;
        ui_prefs = PREF_ANIM_OFF;
        settings_export(&p);
        ui_prefs = 0;
        ok = settings_import(&p, (int)sizeof p) == 1 && (ui_prefs & PREF_ANIM_OFF);
        memset(&p.favorites, 0, sizeof p.favorites);
        p.magic = 0x50455233u;
        ok &= settings_import(&p, (int)(sizeof p - sizeof p.favorites)) == 2 && !ui_prefs;
        bad += check("#46 ANIM saved with the settings; settings from before it: ON", ok);
    }
    ui_prefs = 0;
    return bad;
}

/* MENU > STYLE (ui_menu.c, gfx.c ST_*): FLAT by default; KNOB 1 right LINE, left FLAT (stops at the ends), OCT+
 * toggles; drawn so from the next frame (ux.style, the tokens: LINE has no SURF, a rule under the header); saved in a
 * spare byte of the settings and read back; settings from before it, and an unknown value, read as FLAT; 2, the
 * retired PIXEL (1.0.1), as LINE (again on a second import: idempotent) */
static int test_style(void)
{
    int bad = 0, ok;
    const uint32_t rule = 26u * 240u + 120u;           /* (the rule under the header: ui_draw.c draw_rules) */
    ui_power_on();
    frame();
    ok = ui_style == ST_FLAT && ux.style == ST_FLAT && ux.surf != ux.bg;
    ui.force = 1; frame();
    ok &= host_screen[rule] == swap16(T_BG);
    hold(B_HOME); ui.menu_sel = MI_STYLE;
    ok &= ui.menu == 1u && str_eq(MI_NAME[MI_STYLE], "STYLE");
    turn(EN_K1, 1); ok &= ui_style == ST_LINE && ux.style == ST_LINE && ux.surf == ux.bg;
    turn(EN_K1, 1); ok &= ui_style == ST_LINE;
    turn(EN_K1, -1); ok &= ui_style == ST_FLAT;
    turn(EN_K1, -1); ok &= ui_style == ST_FLAT;
    press(B_OCTUP); ok &= ui_style == ST_LINE && ui.menu == 1u;
    press(B_OCTUP); ok &= ui_style == ST_FLAT && ux.style == ST_FLAT && ux.surf != ux.bg;
    press(B_OCTUP); ok &= ui_style == ST_LINE;
    ok &= !(ui_prefs & 0xFFu) && settings_leds == LEDS_DIM;   /* (no other setting moved) */
    hold(B_HOME);
    ok &= !ui.menu && ux.style == ST_LINE && host_screen[rule] == swap16(T_RULE);
    bad += check("MENU > STYLE: FLAT by default, KNOB 1 FLAT / LINE, OCT+ toggles, LINE drawn (rules, no SURF)", ok);
    {
        persist_t p, q;
        settings_export(&p);
        ui_style = ST_FLAT; frame();
        ok = settings_import(&p, (int)sizeof p) == 1 && ui_style == ST_LINE;
        frame();
        ok &= ux.style == ST_LINE;
        p.favorites.factory[15][29] = 7;                /* unknown: FLAT, and so again */
        ok &= settings_import(&p, (int)sizeof p) == 1 && ui_style == ST_FLAT && !p.favorites.factory[15][29];
        q = p;
        ok &= settings_import(&q, (int)sizeof q) == 1 && ui_style == ST_FLAT && !memcmp(&p, &q, sizeof p);
        p.favorites.factory[15][29] = 2;                /* the retired PIXEL: LINE, and so again */
        ok &= settings_import(&p, (int)sizeof p) == 1 && ui_style == ST_LINE && p.favorites.factory[15][29] == ST_LINE;
        q = p;
        ok &= settings_import(&q, (int)sizeof q) == 1 && ui_style == ST_LINE && !memcmp(&p, &q, sizeof p);
        frame();
        ok &= ux.style == ST_LINE;
        ui_style = 2; frame();                          /* (a live PIXEL byte, a restore: LINE) */
        ok &= ui_style == ST_LINE && ux.style == ST_LINE;
        ui_style = ST_LINE;
        settings_export(&p);
        memset(&p.favorites, 0, sizeof p.favorites);
        p.magic = 0x50455233u;                          /* settings from before STYLE (PER3) */
        ok &= settings_import(&p, (int)(sizeof p - sizeof p.favorites)) == 2 && ui_style == ST_FLAT;
        frame();
        ok &= ux.style == ST_FLAT && ux.surf != ux.bg;
        bad += check("  STYLE saved with the settings and read back; older settings and unknown values: FLAT, PIXEL: LINE (idempotent)", ok);
    }
    ui_power_on(); frame();
    return bad;
}

/* the header's elements share one visual centre line (H_HEAD / 2, ui_draw.c draw_head): each element's ink box,
 * measured in the render (every pixel off BG; the elements are the runs of inked columns, split at 2 empty columns
 * and at the zone edges), has its centre (top + bottom + 1) / 2 within 1 px of row 12, in seven states: stopped,
 * playing, a song playing, a message, OCT shifted, USB connected, a message with a keycap; FLAT and LINE, every palette */
static int head_centres(const char *what, int print)
{
    static const int16_t EDGE[] = {28, 54, 72, 106, 116, 170, 189, 214};
    uint16_t bg = swap16(T_BG);
    int32_t x = 0, ok = 1;
    while (x < 240) {
        int32_t x0, x1, top = 99, bot = -1, gap = 0, k, y, ink;
        for (; x < 240; x++) {                        /* the next inked column */
            for (y = 0, ink = 0; y < H_HEAD; y++) ink |= host_screen[(Y_HEAD + y) * 240 + x] != bg;
            if (ink) break;
        }
        if (x >= 240) break;
        x0 = x1 = x;
        for (; x < 240 && gap < 2; x++) {
            for (k = 0; k < 8 && x != x0; k++) if (x == EDGE[k]) break;
            if (k < 8 && x != x0) break;              /* a zone edge ends the element */
            for (y = 0, ink = 0; y < H_HEAD; y++)
                if (host_screen[(Y_HEAD + y) * 240 + x] != bg) {
                    ink = 1;
                    if (y < top) top = y;
                    if (y > bot) bot = y;
                }
            gap = ink ? 0 : gap + 1;
            if (ink) x1 = x;
        }
        x = x1 + 1;
        if (print) printf("    head %-8s x %3d..%3d ink rows %2d..%2d centre %4.1f (%+.1f)\n", what, x0, x1, top, bot,
                          (top + bot + 1) / 2.0, (top + bot + 1) / 2.0 - H_HEAD / 2);
        ok &= top + bot + 1 >= H_HEAD - 2 && top + bot + 1 <= H_HEAD + 2;
    }
    return ok;
}
static int test_head_centres(void)
{
    static const char *const N[7] = {"stopped", "playing", "song", "message", "OCT", "USB", "keycap"};
    int bad = 0, ok = 1;
    uint32_t st, p, s;
    for (st = ST_FLAT; st <= ST_LINE; st++)
        for (p = 0; p < NPALETTES; p++)
            for (s = 0; s < 7u; s++) {
                ui_power_on();
                ui_style = (uint8_t)st; style_apply();
                settings.palette = p; palette_set(p);
                song.playing = s == 1u || s == 2u; song.rec = s == 1u ? 1u : 0u;
                if (s == 2u) { chain.running = 1; chain.row = 2; }
                if (s == 3u) { str_cpy(ui.msg, "SAVED 03 BASS", sizeof ui.msg); ui.msg_t = 100; }
                if (s == 4u) song.octave = 2;
                if (s == 5u) { usb.config = 1; usb.suspended = 0; }
                if (s == 6u) { str_cpy(ui.msg, "[OCT+] SAVE", sizeof ui.msg); ui.msg_t = 100; }   /* a keycap message */
                memset(host_screen, 0, sizeof host_screen);
                ui.force = 1; draw_head(); ui.force = 0;
                ok &= head_centres(N[s], !st && !p);
                chain.running = 0; ui.msg_t = 0; song.octave = 0; usb.config = 0; song.playing = song.rec = 0;
            }
    ui_style = ST_FLAT; style_apply();
    bad += check("header: every element's ink centred on row 12 (+-1 px), seven states, FLAT and LINE, every palette", ok);
    ui_power_on();
    return bad;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    int bad = 0;
    bad += test_large_face();
    bad += test_sound_loads();
    bad += test_patterns();
    bad += test_rec();
    bad += test_midi();
    bad += test_save();
    bad += test_actions();
    bad += test_tracks();
    bad += test_grid();
    bad += test_screen();
    bad += test_favorites();
    bad += test_display_preferences();
    bad += test_information();
    bad += test_chain();
    bad += test_product_ux();
    bad += test_mono_screens();
    bad += test_roll();
    bad += test_soak_cards();
    bad += test_panel();
    bad += test_layer();
    bad += test_layer_knob_leak();
    bad += test_fx_latch();
    bad += test_layer_lock();
    bad += test_menu_prefs();
    bad += test_style();
    bad += test_head_centres();
    bad += test_div_order();
    bad += test_knob_accel();
    bad += test_usb_level();
    bad += test_usb_serial();
    bad += test_bpm_lock();
    bad += test_large();
    bad += test_browse_no_pattern();
    bad += test_name();
    bad += test_edit_cycle();
#if FELUCCA_SLICE
    bad += test_slices();
#endif
    bad += test_quick_layers();
    bad += test_chord_page();
    bad += test_bughunt_ui();
    bad += test_bughunt_ui2();
    bad += test_piano_roll();
    bad += test_play_leds();
    bad += test_midi_leds();
    bad += test_seq_quant();
    bad += test_idle_glow();
    bad += test_step_leds();
    bad += test_fm6_charts();
#if FELUCCA_FM4
    bad += test_fm_charts();                        /* (DIGITAL's charts: built with FELUCCA_FM4=1 only) */
#else
    bad += test_fm4_retired();
#endif

    if (getenv("UI_SCREEN_DIR")) chain_screens(getenv("UI_SCREEN_DIR"));
    printf(bad ? "ui: %d FAILED\n" : "ui: all passed\n", bad);
    return bad ? 1 : 0;
}
#endif
