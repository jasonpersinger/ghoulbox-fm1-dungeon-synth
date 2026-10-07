/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The firmware editor handler, USB framing and UART recovery against RAM flash.
 * Build with the generated tables and the same flags as hostsim.c. */
static unsigned char host_samples[3][0x14000];
#define SMP_USER_XIP(k) host_samples[k]
#define FELUCCA_OTA 1
#define FELUCCA_FLASH 0
#define FELUCCA_VERSION "TEST"
#define FELUCCA_CDC 1                                    /* (as the firmware: MENU > USB SERIAL switches the console) */
#define main hostsim_main
#include "hostsim.c"
#undef main

static uint32_t host_progress = 1, host_erases, host_writes;
static uint8_t host_wire[4096];
static uint32_t host_wire_n;
static void host_drain(void)
{
    while (so_r != so_w) {
        uint32_t p = sx_out_q[so_r++ % SXQ], cin = p & 15u, i;
        uint32_t n = cin == 4u || cin == 7u ? 3u : cin == 6u ? 2u : 1u;
        for (i = 0; i < n && host_wire_n < sizeof host_wire; i++)
            host_wire[host_wire_n++] = (uint8_t)(p >> (8u * (i + 1u)));
    }
}
static uint32_t ota_now_ms(void) { return fm1_ms; }
static void ota_idle(void) { host_drain(); fm1_ms++; }
static void fm1_wdt_feed(void)
{
    fm1_ms++;
    if (host_progress && transport_req == 2u) { seq_stop(); transport_req = 0; }
}
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
static int32_t fm1_enc_take(uint32_t e) { (void)e; return 0; }
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{ (void)x; (void)y; (void)w; (void)h; (void)p; }
#include "../firmware/src/gfx.c"
#include "../firmware/src/panel.c"
#include "../firmware/src/ui.c"
#include "../firmware/src/menu_items.c"
static void panel_setup(void) {}
#include "../firmware/src/upreset.c"
#include "../firmware/src/project.c"

static uint32_t flash_ok = 1;
static void audio_silence(void) {}
static void fl_inval(uint32_t off, uint32_t n) { (void)off; (void)n; }
static uint8_t *host_flash_ptr(uint32_t off) { return &host_samples[0][0] + off - SMP_USER_BASE; }
static int fl_erase4k(uint32_t off, uint32_t *took)
{
    memset(host_flash_ptr(off), 0xFF, 4096); *took = 0; host_erases++; return 0;
}
static int fl_write(uint32_t off, const void *p, uint32_t n)
{
    memcpy(host_flash_ptr(off), p, n); host_writes++; return 0;
}
static int st_read(uint32_t off, void *p, uint32_t n) { (void)off; (void)p; (void)n; return -1; }
static int st_prog(uint32_t off, const void *p, uint32_t n) { (void)off; (void)p; (void)n; return -1; }
static int st_erase(uint32_t off) { (void)off; return -1; }
#include "../firmware/src/storage.c"
#include "../firmware/src/editor.c"

static int check(const char *what, int ok)
{
    printf("editor: %-70s %s\n", what, ok ? "ok" : "FAIL");
    return !ok;
}
static void reset(void)
{
    uint32_t t, i;
    memset(&song, 0, sizeof song); memset(trk, 0, sizeof trk);
    memset(&chain, 0, sizeof chain); chain_defaults(&chain_config);
    memset(&ed_w, 0, sizeof ed_w); memset(&ui, 0, sizeof ui);
    memset(&favorites, 0, sizeof favorites); memset(&settings, 0, sizeof settings); settings_init();
    memset(proj_slot, 0, sizeof proj_slot); memset(up_bank, 0, sizeof up_bank);
    memset(&um, 0, sizeof um); memset(usr_nz, 0, sizeof usr_nz);
    host_progress = 1; host_erases = host_writes = host_wire_n = 0;
    transport_req = panic_req = 0; sx_ready = sx_collect = sx_busy = 0;
    so_r = so_w = mi_r = mi_w = 0; midi_in_overflow = 0; usb.config = 1;
    for (i = 0; i < G_COUNT; i++) song.g[i] = GP[i].def;
    for (t = 0; t < NTRK; t++) {
        track_defaults(&trk[t]); set_engine_of(&trk[t], 0); apply_preset_to(&trk[t], 0);
        trk[t].engine = trk[t].eng_req; track_defaults_steps(&trk[t]);
    }
}
static uint32_t request(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    uint32_t i;
    host_wire_n = 0; ota_frame_done();
    sysex_byte(0xF0); sysex_byte(ED_HDR0); sysex_byte(ED_HDR1); sysex_byte(ED_HDR2); sysex_byte((uint8_t)cmd);
    for (i = 0; i < n; i++) sysex_byte(a[i]);
    sysex_byte(0xF7); ed_service(); host_drain();
    return host_wire_n;
}
static uint32_t pack7(const uint8_t *p, uint32_t n, uint8_t *a)
{
    uint32_t o = 0, i;
    while (n) {
        uint32_t k = n > 7u ? 7u : n, m = o++;
        a[m] = 0;
        for (i = 0; i < k; i++, n--) { a[m] |= (*p >> 7) << i; a[o++] = *p++ & 127u; }
    }
    return o;
}

static int preferences(void)
{
    int bad = 0;
    uint8_t a[4] = {0, 7, 0, 0};
    reset();
    uint32_t n = request(ED_INFO, a, 0);
    bad += check("INFO explicitly tags display capabilities after SONG without changing command 33",
        ED_SONG == 33 && ED_UI_STATE == 34 && ED_FAV_SET == 38 &&
        host_wire[n - 28] == CHAIN_ROWS && host_wire[n - 27] == 0x55 &&
        host_wire[n - 26] == 1 && host_wire[n - 25] == 9 &&
        host_wire[n - 24] == 0x4d && host_wire[n - 23] == 1 &&
        host_wire[n - 22] == MOTION_MAX && host_wire[n - 21] == 1 &&
        host_wire[n - 20] == 0x42 && host_wire[n - 19] == 1 && host_wire[n - 18] == 3 &&
        host_wire[n - 17] == 0x46 && host_wire[n - 16] == 1 && host_wire[n - 15] == FM6_NFACTORY &&
        host_wire[n - 14] == 0 &&                        /* (no bank since 1.0.3) */
        host_wire[n - 13] == 0x53 && host_wire[n - 12] == 1 && host_wire[n - 11] == 3 &&
        host_wire[n - 10] == 0x50 && host_wire[n - 9] == 1 && host_wire[n - 8] == 3 &&   /* FM6 v2: no bank, preset patches */
        host_wire[n - 7] == 0x4E && host_wire[n - 6] == 1 && host_wire[n - 5] == 12 &&   /* MENU settings: 12 items */
        host_wire[n - 4] == 0x52 && host_wire[n - 3] == 1 && host_wire[n - 2] == 4);   /* RATCH */
    request(ED_UI_SET, a, 2);
    bad += check("UI_SET updates the actual palette and reports RAM-only saving",
        host_wire[5] == 3 && settings.palette == 7 && T_BG == UI_PALETTES[7].bg);
    a[1] = NPALETTES; request(ED_UI_SET, a, 2);
    bad += check("out-of-range palette leaves the display unchanged", host_wire[5] == 1 && settings.palette == 7);
    a[0] = 1; a[1] = 1; request(ED_UI_SET, a, 2);
    bad += check("the retired font weight is not supported (rc 2), UI_STATE says 127",
        host_wire[5] == 2 && host_wire[8] == 9 && host_wire[10] == 127);
    a[0] = 2; request(ED_UI_SET, a, 2);
    bad += check("unsupported preference is reported without applying it", host_wire[5] == 2);
    a[0] = ENGI_DRUM; a[1] = 0; a[2] = 64; a[3] = 1;
    request(ED_FAV_SET, a, 4);
    bad += check("FAV_SET marks DRUM as a normal engine", host_wire[5] == 3 && favorite_has(ENGI_DRUM, 0));
    request(ED_FAV_GET, a, 4);
    bad += check("FAV_GET reads the bounded favorite range", host_wire[5] == 0 && host_wire[10] == 1);
    a[0] = NENGINES; a[1] = 31; request(ED_FAV_SET, a, 4);
    bad += check("empty user slot cannot be starred", host_wire[5] == 1 && !favorite_has(NENGINES, 31));
    up_store(31, "Saved"); request(ED_FAV_SET, a, 4);
    bad += check("saved user slot can be starred without changing its sound", host_wire[5] == 3 && favorite_has(NENGINES, 31));
    a[3] = 32; request(ED_FAV_GET, a, 4);
    bad += check("favorite range cannot cross the end of user slots", host_wire[5] == 1);
    request(ED_FAV_SET, a, 3);
    bad += check("short favorite writes return an error without reading absent bytes", host_wire[5] == 1);
    bad += check("UI_STATE rejects unexpected request bytes", request(ED_UI_STATE, a, 1) == 0);
    a[0] = 0; request(ED_SONG, a, 1);
    bad += check("SONG still responds through its original command", host_wire_n > 6 && host_wire[4] == 33 && host_wire[5] == 0);
    return bad;
}

static int framing(void)
{
    uint32_t i, bad = 0;
    static const uint8_t rt[] = {0xF0, 0x7D, 0xF8, 0x46, 0xFE, 0x4C, ED_PING, 0xFF, 0xF7};
    static const uint8_t aborted[] = {0xF0, 0x7D, 0x46, 0x4C, ED_SET, 0, P_LEVEL, 0x90, 0, 64, 0xF7};
    reset();
    for (i = 0; i < sizeof rt; i++) sysex_byte(rt[i]);
    ed_service(); host_drain();
    bad += check("realtime bytes interleaved in SysEx do not enter the editor frame",
                 host_wire_n == 7u && host_wire[4] == ED_PING && host_wire[5] == 0);
    ota_frame_done(); host_wire_n = 0;
    for (i = 0; i < sizeof aborted; i++) sysex_byte(aborted[i]);
    ed_service(); host_drain();
    bad += check("a non-realtime status aborts SysEx without changing parameters",
                 !sx_ready && !host_wire_n && TSEL->p[P_LEVEL] == TP[P_LEVEL].def);
    bad += check("a valid request works after an aborted frame", request(ED_PING, 0, 0) == 7u);
    ota_frame_done();
    midi_in_event(0x467DF004u);                          /* F0 7D 46 */
    midi_in_event(0x00004C04u);                          /* unfinished SysEx */
    midi_in_event(0x643C9009u);
    sysex_byte(0xF7);
    bad += check("a channel event in another USB packet aborts an unfinished SysEx",
                 !sx_ready && mi_w == 1u && midi_in_q[0] == 0x643C9009u);
    midi_in_event(0x643C8009u);                          /* wrong CIN */
    midi_in_event(0x643CFF09u);                          /* wrong status */
    midi_in_event(0xFF3C9009u);                          /* not a seven-bit velocity */
    midi_in_event(0x64FF9009u);                          /* not a seven-bit note */
    bad += check("USB channel packets reject mismatched CIN and high data bits", mi_w == 1u);
    midi_in_event(0xFF05C00Cu);                          /* one-byte status: padding is ignored */
    midi_in_event(0xFF07D00Du);
    bad += check("USB program and channel pressure keep their one-byte payload", mi_w == 3u);
    mi_w = UINT32_MAX - 5u; mi_r = mi_w;
    for (i = 0; i < MQ + 8u; i++) midi_in_event(0x643C9009u);
    bad += check("USB MIDI ring stops at capacity across counter wrap", mi_w - mi_r == MQ && midi_in_overflow);
    mi_r = mi_w; midi_in_event(0x643C9009u);
    bad += check("USB rejects new messages until the audio consumer clears overflow", mi_w == mi_r);
    midi_in_overflow = 0; midi_in_event(0x643C9009u);
    bad += check("USB resumes after the audio consumer clears overflow", mi_w - mi_r == 1u);
    return bad;
}

static int uart_recovery(void)
{
    uint32_t i;
    int bad = 0;
    reset();
    um_byte(0x90); um_byte(50);
    for (i = 0; i < UM_RING; i++) um_ring[i] = 0;
    um_ring[126] = 0x90; um_ring[127] = 72; um_ring[0] = 99;
    uart_midi_take(UM_RING + 1u);
    bad += check("UART DMA overrun never combines a stale partial note with new data",
                 !mi_w && midi_in_overflow && um.drops == 2u && um.bytes == UM_RING && !um.pend);
    midi_in_overflow = 0; um_byte(0x90); um_byte(72); um_byte(99);
    bad += check("UART receives a complete fresh note after DMA recovery",
                 mi_w == 1u && midi_in_q[0] == 0x63489009u);
    reset();
    um_byte(0x90); um_byte(50);
    for (i = 0; i < UM_RING; i++) um_ring[i] = 60;
    uart_midi_take(UM_RING + 2u);
    bad += check("UART discards running-status data after lost bytes until a new status",
                 !mi_w && midi_in_overflow && um.drops == 2u && !um.st && !um.got);
    midi_in_overflow = 0;
    um_byte(0x91); um_byte(70); um_byte(100);
    bad += check("UART recovers immediately on a complete channel message", mi_w == 1u && midi_in_q[0] == 0x64469109u);
    reset(); mi_w = MQ;
    um_byte(0x90); um_byte(60); um_byte(100);
    bad += check("UART queue overflow latches the same recovery flag as USB", midi_in_overflow && um.drops == 1u && mi_w == MQ);
    mi_r = mi_w; um_byte(61); um_byte(101);
    bad += check("UART rejects a new stream until audio clears overflow", mi_w == mi_r && um.drops == 2u);
    midi_in_overflow = 0; um_byte(62); um_byte(102);
    bad += check("UART resumes valid running status after audio clears overflow",
                 mi_w - mi_r == 1u && midi_in_q[mi_r % MQ] == 0x663E9009u);
    return bad;
}

static int steps(void)
{
    uint8_t a[80] = {0, 1, 60, 0, 0, 0, ST_NOTE, SF_ACCENT, 100, 0x12, 0x02, 1};
    step_t before;
    uint32_t n, ok = 1;
    int bad = 0;
    reset();
    TSEL->step[0].hit = TSEL->step[0].acc = 0x80;
    bad += check("legacy 8-byte step writes preserve lane data",
                 request(ED_STEP_SET, a, 9) == 20u && TSEL->step[0].note[0] == 60 &&
                 TSEL->step[0].hit == 0x80 && TSEL->step[0].acc == 0x80);
    bad += check("full grid step writes preserve high lane bits and constrain accents",
                 request(ED_STEP_SET, a, 12) == 20u && TSEL->step[0].hit == 0x92 && TSEL->step[0].acc == 2);
    before = TSEL->step[0]; a[2] = 71;
    for (n = 2; n <= sizeof a; n++) {
        if (n == 9u || n == 12u || n == 13u) continue;   /* (14: a[13], the ratchet, 0 is refused) */
        ok &= !request(ED_STEP_SET, a, n) && !memcmp(&before, &TSEL->step[0], sizeof before);
    }
    bad += check("partial or oversized step payloads never mutate a valid step", ok);
    a[0] = 1; a[1] = 0; memcpy(a + 2, (const uint8_t[]){1,64,0,0,0,ST_NOTE,0,99}, 8);
    bad += check("TRACK_STEP accepts its legacy payload on an unselected track",
                 request(ED_TRACK_STEP, a, 10) == 21u && trk[1].step[0].note[0] == 64 && song.sel == 0);
    before = trk[1].step[0]; a[3] = 65;
    bad += check("TRACK_STEP rejects an incomplete grid extension",
                 !request(ED_TRACK_STEP, a, 11) && !memcmp(&before, &trk[1].step[0], sizeof before));
    /* RATCH (INFO 52 01 04): the hits 1..4 after the chance; the flags byte stays accent | slide both ways */
    memcpy(a, (const uint8_t[]){0, 1, 60, 0, 0, 0, ST_NOTE, SF_ACCENT, 100, 0, 0, 0, 80, 3}, 14);
    n = request(ED_STEP_SET, a, 14);
    bad += check("STEP_SET with the ratchet: x3 kept, replied after the chance, flags without it",
                 n == 20u && step_ratchet(&TSEL->step[0]) == 3u && step_chance(&TSEL->step[0]) == 80u &&
                 host_wire[n - 2] == 3 && host_wire[n - 3] == 80 && host_wire[12] == SF_ACCENT);
    a[7] = SF_SLIDE; n = request(ED_STEP_SET, a, 13);
    ok = n == 20u && step_ratchet(&TSEL->step[0]) == 3u && TSEL->step[0].flags == (SF_SLIDE | 2u << SF_RATCH_SH);
    n = request(ED_STEP_SET, a, 9);
    bad += check("STEP_SET without the ratchet (an older editor) keeps the step's own",
                 ok && n == 20u && step_ratchet(&TSEL->step[0]) == 3u);
    before = TSEL->step[0]; ok = 1;
    for (n = 0; n < 8u; n++) {
        a[13] = (uint8_t)(n < 4u ? 0u : 5u + n);
        ok &= !request(ED_STEP_SET, a, 14) && !memcmp(&before, &TSEL->step[0], sizeof before);
    }
    bad += check("STEP_SET refuses a ratchet outside 1..4 and leaves the step", ok);
    memcpy(a, (const uint8_t[]){1, 2, 1, 64, 0, 0, 0, ST_NOTE, 0, 99, 0, 0, 0, 100, 4}, 15);
    n = request(ED_TRACK_STEP, a, 15);
    ok = n == 21u && step_ratchet(&trk[1].step[2]) == 4u && host_wire[n - 2] == 4;
    a[14] = 1; n = request(ED_TRACK_STEP, a, 15);
    bad += check("TRACK_STEP sets the ratchet of any track (x4, then back to x1)",
                 ok && n == 21u && step_ratchet(&trk[1].step[2]) == 1u && !(trk[1].step[2].flags & SF_RATCH));
    return bad;
}

static int samples(void)
{
    uint8_t a[640] = {0}, data[257], short_group[] = {0, 0, 4, 0, 1};
    uint32_t n, i;
    int bad = 0;
    smp_user_hdr_t h;
    reset(); memset(host_samples, 0, sizeof host_samples);
    for (i = 0; i < sizeof data; i++) data[i] = (uint8_t)(i * 37u);
    song.playing = 1; host_progress = 0;
    bad += check("sample erase waits for STOP and refuses a stalled audio ISR",
                 request(ED_SMP_BEGIN, a, 1) == 8u && host_wire[6] != 0 && !host_erases && song.playing);
    host_progress = 1;
    bad += check("sample erase stops transport before touching flash",
                 request(ED_SMP_BEGIN, a, 1) == 8u && !host_wire[6] && host_erases == 1 && !song.playing);
    a[0] = 0; a[1] = 0; a[2] = 4; a[3] = 0;
    n = pack7(data, 257, a + 4) + 4u;
    bad += check("257 decoded sample bytes are rejected without a truncated write",
                 request(ED_SMP_WRITE, a, n) == 11u && host_wire[9] == 1 && !host_writes);
    bad += check("a dangling packed-data mask is rejected",
                 request(ED_SMP_WRITE, short_group, sizeof short_group) == 11u && host_wire[9] == 1 && !host_writes);
    n = pack7(data, 256, a + 4) + 4u;
    transport_req = 1;
    bad += check("sample writes cancel a pending PLAY before flash access",
                 request(ED_SMP_WRITE, a, n) == 11u && !host_wire[9] && transport_req != 1u && host_writes == 1);
    memset(&h, 0, sizeof h); h.magic = SMP_USER_MAGIC; h.version = 1; h.nz = 1;
    h.data_len = 256; h.crc = st_crc32(data, 256); h.zone[0].n = 512; h.zone[0].le = 511;
    h.zone[0].rate = 65536; h.zone[0].hi = 127;
    a[0] = 0; n = pack7((const uint8_t *)&h, sizeof h, a + 1) + 1u;
    a[n] = 0; a[n + 1u] = 0;
    bad += check("an oversized sample header is rejected without programming flash",
                 request(ED_SMP_END, a, n + 2u) == 8u && host_wire[6] == 1 && host_writes == 1);
    bad += check("complete sample header and CRC publish a valid slot",
                 request(ED_SMP_END, a, n) == 8u && !host_wire[6] && usr_nz[0] == 1 && host_writes == 2);
    bad += check("repeated END of the same published header does not rewrite live zones",
                 request(ED_SMP_END, a, n) == 8u && !host_wire[6] && host_writes == 2);
    h.zone[0].rate = 131072;
    n = pack7((const uint8_t *)&h, sizeof h, a + 1) + 1u;
    bad += check("END cannot change published sample zones without BEGIN",
                 request(ED_SMP_END, a, n) == 8u && host_wire[6] == 2 && host_writes == 2 &&
                 usr_zone[0][0].rate == 65536u);
    song.playing = 1; host_progress = 0; a[0] = 0; a[1] = 0;
    bad += check("user-preset STORE does not mutate RAM when audio cannot stop",
                 request(ED_UP_STORE, a, 2) == 8u && host_wire[6] == 2 && !up_used(0));
    bad += check("user-preset ERASE reports failure when audio cannot stop",
                 request(ED_UP_ERASE, a, 1) == 8u && host_wire[6] == 2);
    return bad;
}

static int song_protocol(void)
{
    uint8_t a[] = {1, 1, 0, 2};
    chain_config_t before;
    int bad = 0;
    reset();
    bad += check("SONG sets and queries the complete chain without changing selection",
                 request(ED_SONG, a, sizeof a) == 14u && !host_wire[6] && chain_config.count == 1 && song.sel == 0);
    before = chain_config; a[3] = 0;
    bad += check("SONG invalid repeats leave the chain unchanged",
                 request(ED_SONG, a, sizeof a) == 14u && host_wire[6] == 1 && !memcmp(&before, &chain_config, sizeof before));
    a[3] = 2; chain.armed = 1;
    bad += check("SONG cannot replace a chain while its start is pending",
                 request(ED_SONG, a, sizeof a) == 14u && host_wire[6] == 2 && !memcmp(&before, &chain_config, sizeof before));
    chain.armed = 0; a[0] = 2;
    bad += check("SONG PLAY refuses extra argument bytes", !request(ED_SONG, a, 2) && !transport_req);
    return bad;
}

static int malformed_saves(void)
{
    static const uint8_t slot[] = {1, 4}, name[] = {0, 'X', 0, 'Y'};
    const uint8_t save[] = {1, 0};
    project_store_t old;
    int bad = 0;
    reset();
    bad += check("PROJECT refuses an invalid slot instead of wrapping to slot 0",
                 !request(ED_PROJECT, slot, sizeof slot) && !project_used(0));
    bad += check("UP_STORE rejects bytes after its name terminator without saving",
                 request(ED_UP_STORE, name, sizeof name) == 8u && host_wire[6] == 1 && !up_used(0));
    bad += check("a successful PROJECT save confirms its slot", request(ED_PROJECT, save, sizeof save) == 9u && project_used(0));
    old = proj_slot[0];
    TSEL->p[P_LEVEL] = 21; song.playing = 1; host_progress = 0;
    bad += check("PROJECT never confirms an old used slot when STOP timed out",
                 !request(ED_PROJECT, save, sizeof save) && !memcmp(&old, &proj_slot[0], sizeof old));
    return bad;
}

/* FM6 patches (cmds 68..71): a track's own patch, the retired bank ("no bank"), the factory patches, the user presets'
 * patches (target 3), malformed frames */
static int fm6_patches(void)
{
    int bad = 0;
    uint8_t a[2 + FM6_PACKED], pk[FM6_PACKED];
    uint32_t n, i;
    reset();
    upf_empty();
    a[0] = ED_FM6_FACTORY; a[1] = 3;
    n = request(ED_FM6_GET, a, 2);
    bad += check("FM6_GET factory 4: the packed record", n == 5u + 3u + FM6_PACKED + 1u && host_wire[7] == 0 &&
                 !memcmp(host_wire + 8, FM6_FACTORY[3], FM6_PACKED));
    memcpy(pk, FM6_FACTORY[3], FM6_PACKED);
    memcpy(pk + 118, "MY PATCH  ", 10);
    trk[2].eng_req = ENGI_FM6;
    trk[2].p[P_E7] = 3;
    a[0] = ED_FM6_TRACK; a[1] = 2; memcpy(a + 2, pk, FM6_PACKED);
    n = request(ED_FM6_PUT, a, sizeof a);
    bad += check("FM6_PUT track 3: rc 0, the track's patch, SLOT OWN (not F4: the name differs)", n == 9u &&
                 host_wire[7] == 0 && !memcmp(fm6_patch[2] + FP_NAME, "MY PATCH  ", 10) &&
                 trk[2].p[P_E7] == FM6_OWN && fm6_slot[2] == FM6_OWN);
    fm6_poll();
    bad += check("  .. the main loop keeps it", !memcmp(fm6_patch[2] + FP_NAME, "MY PATCH  ", 10));
    a[1] = 2;
    n = request(ED_FM6_GET, a, 2);
    bad += check("FM6_GET track 3: as sent", host_wire[7] == 0 && !memcmp(host_wire + 8, pk, FM6_PACKED));
    a[0] = ED_FM6_TRACK; a[1] = 2; memcpy(a + 2, FM6_FACTORY[5], FM6_PACKED);
    request(ED_FM6_PUT, a, sizeof a);
    bad += check("FM6_PUT of a factory patch unchanged: SLOT shows it (F6)", trk[2].p[P_E7] == 5 && fm6_slot[2] == 5u);
    trk[2].p[P_E7] = FM6_OWN;                            /* (back to MY PATCH for the SLOT trip below) */
    a[0] = ED_FM6_TRACK; a[1] = 2; memcpy(a + 2, pk, FM6_PACKED);
    request(ED_FM6_PUT, a, sizeof a);
    trk[2].p[P_E7] = 1;
    fm6_poll();
    bad += check("SLOT OWN -> F2 loads the factory patch", !memcmp(fm6_patch[2] + FP_NAME, FM6_FACTORY[1] + 118, 10));
    trk[2].p[P_E7] = 6;
    fm6_poll();
    trk[2].p[P_E7] = FM6_OWN;
    fm6_poll();
    bad += check("  .. and back to OWN brings the own patch back", !memcmp(fm6_patch[2] + FP_NAME, "MY PATCH  ", 10) &&
                 fm6_slot[2] == FM6_OWN);
    trk[2].p[P_E7] = 20;                                 /* (a 1.0.2 B slot that escaped a clamp) */
    fm6_poll();
    bad += check("a SLOT past OWN is OWN: the patch stays, nothing reloads", trk[2].p[P_E7] == FM6_OWN &&
                 !memcmp(fm6_patch[2] + FP_NAME, "MY PATCH  ", 10));
    for (i = 0; i < 3u; i++) {                           /* the bank: GET, PUT, ERASE answer "no bank" (3) */
        a[0] = ED_FM6_BANK; a[1] = 4; memcpy(a + 2, pk, FM6_PACKED);
        if (i < 2u)
            request(i ? ED_FM6_PUT : ED_FM6_GET, a, i ? sizeof a : 2u);
        else {
            a[0] = 4;
            request(ED_FM6_ERASE, a, 1);
        }
        bad += check(i == 0u ? "FM6_GET of the bank: rc 3, no bank" : i == 1u ? "FM6_PUT to the bank: rc 3, no bank" :
                               "FM6_ERASE: rc 3, no bank", host_wire[i == 2u ? 6 : 7] == 3u);
    }
    n = request(ED_FM6_LIST, a, 0);
    {   /* factory 8, bank 0, then used + name per factory slot */
        uint32_t p = 7, k, ok = host_wire[5] == FM6_NFACTORY && host_wire[6] == 0;
        for (k = 0; k < FM6_NFACTORY && p < n; k++) {
            ok &= host_wire[p++] == 1u;
            while (host_wire[p]) p++;
            p++;
        }
        bad += check("FM6_LIST: 8 factory names, nbank 0", ok && k == FM6_NFACTORY && p == n - 1u);
    }
    /* target 3: an FM6 user preset's patch */
    song.sel = 2;
    up_store(5, "MINE");                                 /* track 3 (MY PATCH, OWN) -> slot 6 */
    a[0] = ED_FM6_USER; a[1] = 5;
    request(ED_FM6_GET, a, 2);
    bad += check("UP_STORE of an FM6 track: FM6_GET user 6 gives its patch", host_wire[7] == 0 &&
                 !memcmp(host_wire + 8 + 118, "MY PATCH  ", 10));
    memcpy(a + 2, pk, FM6_PACKED);
    memcpy(a + 2 + 118, "EDITOR    ", 10);
    a[2 + 14] = 120;                                     /* OP6 output level 120: stored as 99 */
    request(ED_FM6_PUT, a, sizeof a);
    i = host_wire[7];
    request(ED_FM6_GET, a, 2);
    bad += check("FM6_PUT user 6, then GET: stored in range (a level of 120 -> 99)", i == 0u && host_wire[7] == 0 &&
                 host_wire[8 + 14] == 99 && !memcmp(host_wire + 8 + 118, "EDITOR    ", 10));
    fm6_load_slot(2, 0);
    up_load(5);
    bad += check("  .. UP_LOAD 6 plays it, SLOT OWN", !memcmp(fm6_patch[2] + FP_NAME, "EDITOR    ", 10) &&
                 trk[2].p[P_E7] == FM6_OWN);
    up_rename(5, "RENAMED");
    a[0] = ED_FM6_USER; a[1] = 5;
    request(ED_FM6_GET, a, 2);
    bad += check("  .. a rename keeps it", host_wire[7] == 0 && !memcmp(host_wire + 8 + 118, "EDITOR    ", 10));
    trk[0].eng_req = ENGI_DRUM;
    song.sel = 0;
    up_store(6, "KIT");
    a[0] = ED_FM6_USER; a[1] = 6; memcpy(a + 2, pk, FM6_PACKED);
    request(ED_FM6_GET, a, 2);
    i = host_wire[7];
    request(ED_FM6_PUT, a, sizeof a);
    bad += check("a DRUM user preset has no patch: GET rc 2, PUT rc 1", i == 2u && host_wire[7] == 1u);
    a[1] = 9;                                            /* an empty slot */
    request(ED_FM6_PUT, a, sizeof a);
    i = host_wire[7];
    a[1] = UP_SLOTS;
    request(ED_FM6_GET, a, 2);
    bad += check("an empty user slot: PUT rc 1; past the slots: GET rc 1", i == 1u && host_wire[7] == 1u);
    up_put(5, 0);
    a[1] = 5;
    request(ED_FM6_GET, a, 2);
    bad += check("an erased preset's patch is gone (GET rc 2)", host_wire[7] == 2u);
    a[0] = ED_FM6_TRACK; a[1] = 4;
    request(ED_FM6_PUT, a, sizeof a);
    i = host_wire[7];
    request(ED_FM6_PUT, a, 20);
    bad += check("FM6_PUT: a fifth track or a short record: rc 1", i == 1u && host_wire[7] == 1u);
    a[0] = 4; a[1] = 0;
    request(ED_FM6_GET, a, 2);
    bad += check("FM6_GET of an unknown target: rc 1", host_wire[7] == 1u);
    return bad;
}

/* UP_PUT -> UP_GET: every value round-trips through the v4 record (a byte each), negative ones too */
static int user_preset_roundtrip(void)
{
    static uint8_t a[16 + 2u * P_COUNT + 32u];
    int16_t want[P_COUNT], got[P_COUNT];
    uint32_t k = 0, i, p, ok;
    int bad = 0;
    reset();
    a[k++] = 3; a[k++] = 0;                                /* slot U04, ANALOG */
    memcpy(a + k, "ROUND", 5); k += 5; a[k++] = 0;
    for (i = 0; i < P_COUNT; i++) {
        const param_desc_t *d = param_desc_of(0, i);
        int16_t v = d->def;
        uint32_t u;
        if (i == P_LEVEL) v = 90;
        if (i == P_PAN) v = -20;
        if (i == P_ED_FLT) v = -40;
        if (i == P_LD_FLT) v = d->min;
        if (i == P_E1) v = d->max;
        want[i] = v; u = (uint32_t)(v + 8192);
        a[k++] = u & 127u; a[k++] = (u >> 7) & 127u;
    }
    for (i = 0; i < 16u; i++) { a[k++] = i & 1u ? 0u : (uint8_t)(48u + i); a[k++] = 0; }
    request(ED_UP_PUT, a, k);
    bad += check("UP_PUT stores a v4 record (no flash here: kept in RAM)",
                 host_wire[6] != 1u && up_used(3) && up_rec(3)->ver == UP_VER);
    up_values(up_rec(3), got);
    for (i = 0, ok = 1; i < P_COUNT; i++) ok &= got[i] == want[i];
    bad += check("UP_PUT keeps every value (PAN -20, FLT -40, min and max)", ok);
    a[0] = 3;
    request(ED_UP_GET, a, 1);
    ok = host_wire[5] == 3 && host_wire[6] == 1 && host_wire[7] == 0 && !memcmp(host_wire + 8, "ROUND", 6);
    for (i = 0, p = 14; i < P_COUNT; i++, p += 2u)
        ok &= (int32_t)(host_wire[p] | host_wire[p + 1] << 7) - 8192 == want[i];
    for (i = 0; i < 16u; i++, p += 2u)
        ok &= host_wire[p] == (i & 1u ? 0u : 48u + i);
    bad += check("UP_GET returns the values and the pattern UP_PUT sent", ok);
    return bad;
}

/* the frames of cmd in host_wire: how many, and the args of the last (into *args) */
static uint32_t wire_frames(uint32_t cmd, const uint8_t **args)
{
    uint32_t i, k = 0;
    for (i = 0; i + 5u < host_wire_n; i++)
        if (host_wire[i] == 0xF0 && host_wire[i + 1] == ED_HDR0 && host_wire[i + 4] == cmd) {
            k++;
            if (args) *args = host_wire + i + 5;
        }
    return k;
}
static uint32_t sync_pass(void)                          /* one main-loop pass of the pushes */
{
    host_wire_n = 0;
    fm1_ms += 30u;
    ed_sync();
    host_drain();
    return host_wire_n;
}

/* #65: WATCH while watching keeps what is not pushed yet; the editor's own sound load is not echoed as RELOAD */
static int live_sync(void)
{
    int bad = 0;
    uint8_t a[4] = {1, 0, 0, 0};
    uint32_t i, other = 0;
    const uint8_t *x = 0;
    reset();
    usb.resets = 0;
    request(ED_WATCH, a, 1);
    bad += check("WATCH 1 starts watching", host_wire[5] == 1 && ed_w.on && !sync_pass());
    TSEL->p[P_LEVEL] = 77;                               /* a device change, not pushed yet */
    request(ED_WATCH, a, 1);
    bad += check("WATCH 1 while watching keeps a pending change: CHANGED still goes out",
                 sync_pass() && wire_frames(ED_CHANGED, &x) == 1u && x[1] == P_LEVEL && ed_rv(x + 2) == 77);
    TSEL->p[P_LEVEL] = 66;
    a[0] = 0; request(ED_WATCH, a, 1);
    a[0] = 1; request(ED_WATCH, a, 1);
    bad += check("WATCH 0 then WATCH 1 starts from the values as they are (no push)", !sync_pass());
    TSEL->p[P_LEVEL] = 55; trk[1].p[P_PAN] = 3;
    a[0] = 3; request(ED_WATCH, a, 1);
    sync_pass();
    bad += check("WATCH 3 while watching: CHANGED kept, TRACK_CHANGED from the mix as it is",
                 host_wire_n && wire_frames(ED_CHANGED, 0) == 1u && !wire_frames(ED_TRACK_CHANGED, 0) && ed_w.v4);
    usb.resets++;
    TSEL->p[P_LEVEL] = 44;
    request(ED_WATCH, a, 1);
    bad += check("WATCH after a USB reset starts over", !sync_pass());

    /* PRESET */
    TSEL->p[P_LEVEL] = 33;                               /* pending; a sound load keeps P_LEVEL */
    a[0] = 0; a[1] = 2;
    request(ED_PRESET, a, 2);
    sync_pass();
    bad += check("the editor's own PRESET: no RELOAD, no CHANGED for what it loaded, a pending CHANGED stays",
                 TSEL->preset == 2 && !wire_frames(ED_RELOAD, 0) && wire_frames(ED_CHANGED, &x) == 1u &&
                 x[1] == P_LEVEL && !sync_pass());
    for (i = 0; i < 3u; i++) {                           /* any preset of any engine */
        a[0] = (uint8_t)(i ? 12u : 4u); a[1] = (uint8_t)i;
        request(ED_PRESET, a, 2);
        other += sync_pass() != 0;
    }
    bad += check("PRESET of other engines: no push at all", !other && ed_w.eng == ed_eng(TSEL));
    a[0] = 1; a[1] = G_ENGSEL; a[2] = (8192 + 5) & 127; a[3] = (8192 + 5) >> 7;
    request(ED_SET, a, 4);
    bad += check("SET of G_ENGSEL: no RELOAD", ed_eng(TSEL) == 5u && !sync_pass());
    sync_reload = 1;                                     /* a load on the device, not pushed yet */
    a[0] = 0; a[1] = 1;
    request(ED_PRESET, a, 2);
    sync_pass();
    bad += check("a RELOAD due before the editor's PRESET still goes out", wire_frames(ED_RELOAD, 0) == 1u);
    a[0] = 0; request(ED_WATCH, a, 1);
    sync_reload = 0;
    a[0] = 0; a[1] = 3;
    request(ED_PRESET, a, 2);
    a[0] = 1; request(ED_WATCH, a, 1);
    bad += check("not watching: PRESET as before, WATCH then starts from it", !sync_pass() && TSEL->preset == 3);
    return bad;
}

/* #64: SysEx through the USB packet path (ep1_take) at full speed: a 256-byte BACKUP_PUT piece (293 pack7 bytes, 101
 * event packets, 7 USB packets) arrives whole; one request at a time always works, a frame sent before the reply
 * to the one before is dropped whole (no reply; never a partial frame) */
static uint32_t usb_frame(const uint8_t *f, uint32_t n, uint8_t *usbp)   /* F0..F7 -> USB-MIDI event packets */
{
    uint32_t i = 0, o = 0;
    while (i < n) {
        uint32_t k = n - i >= 3u ? 3u : n - i, cin = k == 3u && i + 3u < n ? 4u : k == 3u ? 7u : 4u + k;
        usbp[o++] = (uint8_t)cin;
        usbp[o++] = f[i];
        usbp[o++] = k > 1u ? f[i + 1] : 0;
        usbp[o++] = k > 2u ? f[i + 2] : 0;
        i += k;
    }
    return o;
}
static uint32_t usb_feed(const uint8_t *p, uint32_t n)   /* whole 64-byte USB packets, as the host sends them */
{
    uint32_t o, refused = 0;
    for (o = 0; o < n; o += 64u)
        while (!ep1_take(p + o, n - o > 64u ? 64u : n - o))
            refused++;                                  /* NAK: the host sends it again */
    return refused;
}
static uint32_t put_frame(uint8_t *f, uint32_t op, uint32_t off, uint32_t count)
{
    static const uint8_t zero[256];
    uint32_t n = 0, i;
    f[n++] = 0xF0; f[n++] = ED_HDR0; f[n++] = ED_HDR1; f[n++] = ED_HDR2; f[n++] = ED_BACKUP_PUT;
    f[n++] = (uint8_t)op; f[n++] = 2;
    if (op == 0u) {
        for (i = 0; i < 5u; i++) f[n++] = (uint8_t)((sizeof(project_store_t) >> (7u * i)) & (i == 4u ? 15u : 127u));
        for (i = 0; i < 5u; i++) f[n++] = 0;
    } else if (op == 1u) {
        for (i = 0; i < 5u; i++) f[n++] = (uint8_t)((off >> (7u * i)) & (i == 4u ? 15u : 127u));
        n += pack7(zero, count, f + n);
    }
    f[n++] = 0xF7;
    return n;
}
static int usb_burst(void)
{
    static uint8_t f[700], u[1000], u2[2000];
    uint32_t off, n, k, ok = 1, max = 0;
    const uint8_t *x = 0;
    int bad = 0;
    reset();
    usb.rx_pend = 0;
    n = put_frame(f, 0, 0, 0);
    usb_feed(u, usb_frame(f, n, u)); host_wire_n = 0; ed_service(); host_drain();
    bad += check("BACKUP_PUT begin through ep1_take", wire_frames(ED_BACKUP_PUT, &x) == 1u && !x[2]);
    for (off = 0; off < sizeof(project_store_t); off += 256u) {
        uint32_t c = sizeof(project_store_t) - off > 256u ? 256u : sizeof(project_store_t) - off;
        n = put_frame(f, 1, off, c);
        if (n > max) max = n;
        k = usb_frame(f, n, u);
        usb_feed(u, k);
        host_wire_n = 0; ed_service(); host_drain();
        ok &= wire_frames(ED_BACKUP_PUT, &x) == 1u && !x[2] && sx_ready == 0;
    }
    bad += check("256-byte pieces in 64-byte USB packets at full speed: every piece taken (rc 0)",
                 ok && ed_bk_pos == sizeof(project_store_t) && max == 4u + 1u + 7u + 293u + 1u && max <= sizeof sx_frame);
    n = put_frame(f, 3, 0, 0);
    usb_feed(u, usb_frame(f, n, u)); host_wire_n = 0; ed_service(); host_drain();
    bad += check("abort through ep1_take", wire_frames(ED_BACKUP_PUT, &x) == 1u && !x[2] && !ed_bk_put);

    /* two frames back to back, before the reply: the second is dropped whole, the first answered intact */
    n = put_frame(f, 0, 0, 0);
    usb_feed(u, usb_frame(f, n, u)); host_wire_n = 0; ed_service(); host_drain();
    k = usb_frame(f, put_frame(f, 1, 0, 256), u2);
    k += usb_frame(f, put_frame(f, 1, 256, 256), u2 + k);
    usb_feed(u2, k);
    host_wire_n = 0; ed_service(); ed_service(); host_drain();
    bad += check("pipelined: the first piece is taken, the second (sent before its reply) dropped whole",
                 wire_frames(ED_BACKUP_PUT, &x) == 1u && !x[2] && ed_bk_pos == 256u && !sx_ready);
    n = put_frame(f, 1, 512, 256);
    usb_feed(u, usb_frame(f, n, u)); host_wire_n = 0; ed_service(); host_drain();
    bad += check("... so the next piece is refused (rc 1, offset), as a host that waits never sees",
                 wire_frames(ED_BACKUP_PUT, &x) == 1u && x[2] == 1u && ed_bk_pos == 256u);
    request(ED_BACKUP_PUT, (const uint8_t[]){3, 2}, 2);
    return bad;
}

/* ---- MENU_DESC / MENU_SET (72, 73): the menu's settings over the editor ---- */
typedef struct { uint32_t index, id, kind, nnames, tab, rest; int32_t value, min, max; char name[16], names[12][12], tabname[16]; } menu_item_t;
static uint32_t menu_desc(uint32_t index, menu_item_t *it)   /* the reply's payload length; it parsed */
{
    uint8_t a[1] = {(uint8_t)index};
    uint32_t n = request(ED_MENU_DESC, a, 1), p = 14, k;
    memset(it, 0, sizeof *it);
    if (n < 8u || host_wire[4] != ED_MENU_DESC || host_wire[n - 1] != 0xF7) return 0;
    it->index = host_wire[5]; it->id = host_wire[6];
    if (n < 15u) return n - 6u;
    it->kind = host_wire[7];
    it->value = ed_rv(host_wire + 8); it->min = ed_rv(host_wire + 10); it->max = ed_rv(host_wire + 12);
    for (k = 0; p < n - 1u && host_wire[p] && k < 15u; ) it->name[k++] = (char)host_wire[p++];
    p++;
    while (p < n - 1u && it->nnames < 12u && (int32_t)it->nnames < it->max - it->min + 1) {   /* (kind 0: max - min + 1) */
        for (k = 0; p < n - 1u && host_wire[p] && k < 11u; ) it->names[it->nnames][k++] = (char)host_wire[p++];
        p++; it->nnames++;
    }
    it->tab = 127;                                          /* 1.0.5: then the row's tab, index and name */
    if (p < n - 1u) {
        it->tab = host_wire[p++];
        for (k = 0; p < n - 1u && host_wire[p] && k < 15u; ) it->tabname[k++] = (char)host_wire[p++];
        p++;
    }
    it->rest = p < n - 1u ? n - 1u - p : 0u;                /* (bytes left over: none) */
    return n - 6u;
}
static uint32_t menu_set(uint32_t id, int32_t v)            /* -> rc; host_wire[6], [7..8]: id, value */
{
    uint8_t a[3] = {(uint8_t)id, (uint8_t)((v + 8192) & 127), (uint8_t)(((v + 8192) >> 7) & 127)};
    uint32_t n = request(ED_MENU_SET, a, 3);
    return n == 10u && host_wire[4] == ED_MENU_SET ? host_wire[5] : 99u;
}
static int menu_protocol(void)
{
    static const char *const WANT[12][2] = {
        {"COLOR", 0}, {"STYLE", "FLAT,LINE"}, {"LARGE", "OFF,ON"}, {"ANIM", "ON,OFF"}, {"LEDS", "OFF,DIM LO,DIM HI,INV"},
        {"HOLD", "0.3 s,0.4 s,0.5 s,0.6 s"}, {"KNOB ACCEL", "OFF,ON"}, {"FX LATCH", "OFF,ON"}, {"BPM LOCK", "OFF,ON"},
        {"SPEAKER EQ", "FLAT,LOWCUT,BASS+"}, {"USB LEVEL", "MASTER,FIXED"}, {"USB SERIAL", "ON,OFF"}};
    static const int32_t DEF[12] = {-1, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0};   /* (COLOR: the default palette) */
    static const uint8_t TAB[12] = {0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 3};      /* DISPLAY CONTROL AUDIO SYSTEM */
    static const char *const TABN[4] = {"DISPLAY", "CONTROL", "AUDIO", "SYSTEM"};
    int bad = 0, ok = 1;
    uint32_t i, k, n;
    menu_item_t it;
    char joined[96];
    const char *json = getenv("MENU_JSON");
    FILE *jf = json ? fopen(json, "w") : 0;
    reset();
    if (jf) fprintf(jf, "[");
    for (i = 0; i < 12u; i++) {
        n = menu_desc(i, &it);
        joined[0] = 0;
        for (k = 0; k < it.nnames; k++) { if (k) strcat(joined, ","); strcat(joined, it.names[k]); }
        ok &= n > 10u && it.index == i && it.id == i && it.kind == 0 && it.min == 0 &&
              it.max + 1 == (int32_t)it.nnames && !strcmp(it.name, WANT[i][0]) &&
              (WANT[i][1] ? !strcmp(joined, WANT[i][1]) : it.nnames == NPALETTES) &&
              it.value == (DEF[i] < 0 ? (int32_t)settings.palette : DEF[i]) &&
              it.tab == TAB[i] && !strcmp(it.tabname, TABN[TAB[i]]) && !it.rest;
        if (i == 0)
            for (k = 0; k < NPALETTES; k++) ok &= !strcmp(it.names[k], UI_PALETTES[k].name);
        if (!ok) { printf("  MENU_DESC %u: %s [%s] value %d max %d tab %u %s\n", i, it.name, joined, it.value, it.max, it.tab, it.tabname); break; }
        if (jf) {
            fprintf(jf, "%s\n {\"id\":%u,\"kind\":%u,\"min\":%d,\"max\":%d,\"value\":%d,\"name\":\"%s\",\"names\":[", i ? "," : "",
                    it.id, it.kind, it.min, it.max, it.value, it.name);
            for (k = 0; k < it.nnames; k++) fprintf(jf, "%s\"%s\"", k ? "," : "", it.names[k]);
            fprintf(jf, "],\"tab\":%u,\"tabName\":\"%s\"}", it.tab, it.tabname);
        }
    }
    if (jf) { fprintf(jf, "]\n"); fclose(jf); }
    bad += check("MENU_DESC: 12 items in the menu's order, ids 0..11, every name and value name, the defaults", ok);
    bad += check("MENU_DESC (1.0.5): after the names each item's tab, index and name (DISPLAY CONTROL AUDIO SYSTEM)", ok);
    {   /* an older editor reads the names and stops: the tab is past them, nothing it reads moved */
        uint32_t m = menu_desc(4, &it), p = 14, q;
        for (q = 0; q < 1u + it.nnames; q++) { while (host_wire[p]) p++; p++; }   /* name, the names */
        ok = m > 10u && host_wire[p] == 0 && !strcmp((const char *)host_wire + p + 1, "DISPLAY") &&
             p + 1u + 8u == m + 6u - 1u;
        bad += check("MENU_DESC: the tab comes after every byte of the 1.0.4 reply (older editors ignore it)", ok);
    }
    ok = 1;
    for (i = 0; i < 12u; i++) {
        menu_desc(i, &it);
        ok &= strcmp(it.name, "CALIBRATION") && strcmp(it.name, "ABOUT");
    }
    n = menu_desc(12, &it);
    ok &= n == 2u && it.index == 12 && it.id == 127;
    n = menu_desc(127, &it);
    bad += check("MENU_DESC: no CALIBRATION / ABOUT; an index past the list answers index, 127 (no item)",
                 ok && n == 2u && it.index == 127 && it.id == 127);
    {
        uint8_t a[3] = {0, 0, 0};
        ok = request(ED_MENU_DESC, a, 0) == 0 && request(ED_MENU_DESC, a, 2) == 0 && request(ED_MENU_SET, a, 2) == 0 &&
             request(ED_MENU_SET, a, 1) == 0;
        bad += check("MENU_DESC / MENU_SET with the wrong length: no reply", ok);
    }

    /* MENU_SET: each setting, applied as the menu applies it; RAM only here (no flash: rc 3) */
    reset();
    ui.force = 0;
    ok = menu_set(0, 2) == 3 && host_wire[6] == 0 && ed_rv(host_wire + 7) == 2 && settings.palette == 2u &&
         T_BG == UI_PALETTES[2].bg && ui.force;
    ok &= menu_set(1, 1) == 3 && ui_style == ST_LINE;
    ok &= menu_set(2, 1) == 3 && (ui_prefs & PREF_LARGE);
    ok &= menu_set(3, 1) == 3 && (ui_prefs & PREF_ANIM_OFF);
    ok &= menu_set(4, 0) == 3 && settings_leds == LEDS_OFF && menu_set(4, 1) == 3 && settings_leds == LEDS_DIM_LO;
    ok &= menu_set(5, 3) == 3 && settings_hold == 3u;
    ok &= menu_set(6, 1) == 3 && (ui_prefs & PREF_ACCEL);
    ok &= menu_set(7, 1) == 3 && (ui_prefs & PREF_LATCH);
    ok &= menu_set(8, 1) == 3 && (ui_prefs & PREF_BPM_LOCK);
    ok &= menu_set(9, 2) == 3 && settings.lowcut == 2u && fx_lowcut == 2u;
    ok &= menu_set(10, 1) == 3 && (ui_prefs & PREF_USB_FIXED) && fx_usb_fixed;
    for (i = 0; i < 12u; i++) {                         /* MENU_DESC reads them back */
        static const int32_t SET[12] = {2, 1, 1, 1, 1, 3, 1, 1, 1, 2, 1, 0};
        menu_desc(i, &it);
        ok &= it.value == SET[i];
    }
    bad += check("MENU_SET: every setting applied as the menu does (palette, EQ, USB LEVEL at once), read back", ok);
    ok = menu_set(3, 0) == 3 && !(ui_prefs & PREF_ANIM_OFF) && (ui_prefs & PREF_LARGE) && menu_set(10, 0) == 3 &&
         !fx_usb_fixed && menu_set(1, 0) == 3 && ui_style == ST_FLAT;
    bad += check("MENU_SET: a flag back to its default leaves the other flags", ok);

    /* clamping: the reply carries the device's value */
    ok = menu_set(0, 50) == 3 && ed_rv(host_wire + 7) == (int32_t)NPALETTES - 1 && settings.palette == NPALETTES - 1u;
    ok &= menu_set(0, -5) == 3 && ed_rv(host_wire + 7) == 0 && settings.palette == 0u;
    ok &= menu_set(4, 99) == 3 && ed_rv(host_wire + 7) == 3 && settings_leds == LEDS_INV;
    ok &= menu_set(5, 8191) == 3 && ed_rv(host_wire + 7) == 3 && settings_hold == 3u;
    ok &= menu_set(2, 7) == 3 && ed_rv(host_wire + 7) == 1 && (ui_prefs & PREF_LARGE);
    bad += check("MENU_SET: out-of-range values clamped, the reply says the value the device took", ok);

    /* an id nobody has */
    {
        static uint8_t fav0[sizeof favorites], set0[sizeof settings];
        uint8_t hold0 = settings_hold, leds0 = settings_leds;
        memcpy(fav0, &favorites, sizeof favorites); memcpy(set0, &settings, sizeof settings);
        ok = menu_set(12, 1) == 1 && host_wire[6] == 12 && ed_rv(host_wire + 7) == 1;
        ok &= menu_set(126, -3) == 1 && host_wire[6] == 126 && ed_rv(host_wire + 7) == -3;
        ok &= menu_set(127, 0) == 1 && host_wire[6] == 127;
        ok &= !memcmp(fav0, &favorites, sizeof favorites) && !memcmp(set0, &settings, sizeof settings) &&
              hold0 == settings_hold && leds0 == settings_leds;
        bad += check("MENU_SET: an unknown id answers rc 1, id and value echoed, nothing changed", ok);
    }

    /* the MENU page shown: the editor's change redraws it */
    ui.menu = 1; ui.force = 0;
    menu_set(7, 0);
    bad += check("MENU_SET while the MENU page is shown: the page redraws (ui.force)", ui.force && ui.menu == 1);
    ui.menu = 0;

    /* USB SERIAL: the reply first, the re-enumeration 200 ms later (usb_serial_apply, called every frame). (usb.up
     * stays 0 here: the host has no USB registers; usb.config 0 = the device off the bus, enumerated afresh) */
    {
        uint32_t t0;
        reset();
        usb_cdc_on = 1;
        usb_serial_apply();
        ok = usb_cdc_on && usb.config;
        ok &= menu_set(11, 1) == 3 && host_wire[6] == 11 && ed_rv(host_wire + 7) == 1 && (ui_prefs & PREF_SERIAL_OFF);
        t0 = fm1_ms;
        usb_serial_apply();
        ok &= usb_cdc_on && usb.config;                 /* the reply has left; USB as it was */
        fm1_ms = t0 + 150u; usb_serial_apply();
        ok &= usb_cdc_on && usb.config;
        fm1_ms = t0 + 201u; usb_serial_apply();
        ok &= !usb_cdc_on && !usb.config;               /* off the bus: usb_retry attaches again without the console */
        bad += check("MENU_SET USB SERIAL OFF: reply first, applied ~200 ms later (the device re-enumerates)", ok);
        usb.config = 1;
        ok = menu_set(11, 0) == 3 && !(ui_prefs & PREF_SERIAL_OFF);
        t0 = fm1_ms;
        ui.menu = 1;                                    /* the MENU shown: applied when it closes, as the menu's own */
        fm1_ms = t0 + 500u;                             /* (ui_input calls usb_serial_apply only with the menu closed) */
        ok &= !usb_cdc_on && usb.config;
        ui.menu = 0; usb_serial_apply();
        ok &= usb_cdc_on && !usb.config;
        usb.config = 1;
        ok &= menu_set(11, 0) == 3;                     /* unchanged: no re-enumeration */
        fm1_ms += 300u; usb_serial_apply();
        ok &= usb_cdc_on && usb.config;
        bad += check("MENU_SET USB SERIAL ON: applied later (with the MENU open: when it closes); unchanged: no drop", ok);
    }
    reset();
    return bad;
}

/* DRUM KIT 1..3 (HAND CYM H+CYM until 1.0.4, retired): DESC names them as the kit they play (66 10 77), a SET of
 * one lands on that kit (the reply says so), a user preset sent with one loads as that kit */
static int drum_kit_retired(void)
{
    static const uint8_t MAP[4] = {0, 6, 5, 8};
    static uint8_t a[16 + 2u * P_COUNT + 32u];
    int16_t got[P_COUNT];
    uint32_t r, i, k = 0, ok = 1;
    int bad = 0;
    reset();
    a[0] = 1; a[1] = G_ENGSEL; a[2] = (8192 + ENGI_DRUM) & 127; a[3] = (8192 + ENGI_DRUM) >> 7;
    request(ED_SET, a, 4);
    a[0] = 0; a[1] = P_E0;
    request(ED_DESC, a, 2);
    {   /* scope, id, fmt, min, max, def (v14 each), "KIT", "", then the 9 names */
        const char *n = (const char *)host_wire + 5 + 3 + 6;
        static const char *const WANT[9] = {"STD", "66", "10", "77", "80", "10", "66", "55", "77"};
        n += strlen(n) + 1; n += strlen(n) + 1;
        for (i = 0; i < 9u; i++, n += strlen(n) + 1)
            ok &= !strcmp(n, WANT[i]);
    }
    bad += check("DESC of DRUM KIT: STD 66 10 77 80 10 66 55 77 (1..3 as the kits they play)", ok);
    for (r = 1, ok = ed_eng(TSEL) == ENGI_DRUM; r < 4u; r++) {
        uint32_t u = r + 8192u;
        a[0] = 0; a[1] = P_E0; a[2] = u & 127u; a[3] = u >> 7;
        request(ED_SET, a, 4);
        ok &= TSEL->p[P_E0] == MAP[r] && (int32_t)(host_wire[7] | host_wire[8] << 7) - 8192 == MAP[r];
    }
    bad += check("SET of DRUM KIT 1..3 lands on 66 10 77 (and replies it)", ok);
    a[k++] = 5; a[k++] = ENGI_DRUM;                        /* slot U06, DRUM, KIT 3 (H+CYM) */
    memcpy(a + k, "OLDKIT", 6); k += 6; a[k++] = 0;
    for (i = 0; i < P_COUNT; i++) {
        uint32_t u = (uint32_t)((i == P_E0 ? 3 : param_desc_of(ENGI_DRUM, i)->def) + 8192);
        a[k++] = u & 127u; a[k++] = (u >> 7) & 127u;
    }
    for (i = 0; i < 16u; i++) { a[k++] = 0; a[k++] = 0; }
    request(ED_UP_PUT, a, k);
    up_values(up_rec(5), got);
    bad += check("a user preset of DRUM KIT 3 (H+CYM) loads as 77", up_used(5) && got[P_E0] == 8);
    {   /* a project whose DRUM tracks hold KIT 0..3: loaded (and bounded, as from flash) as STD 66 10 77 */
        static project_t p;
        for (r = 0; r < NTRK; r++) {
            set_engine_of(&trk[r], ENGI_DRUM); apply_preset_to(&trk[r], 0);
            trk[r].engine = trk[r].eng_req;
        }
        project_capture(&p);
        for (r = 0; r < NTRK; r++) p.t[r].p[P_E0] = (int16_t)r;
        p.sum = proj_sum(&p);
        ok = !project_restore_runtime(&p);
        for (r = 0; r < NTRK; r++) ok &= trk[r].p[P_E0] == MAP[r];
        for (r = 0; r < NTRK; r++) p.t[r].p[P_E0] = (int16_t)r;
        proj_bound(&p);
        for (r = 0; r < NTRK; r++) ok &= p.t[r].p[P_E0] == MAP[r];
        bad += check("a project of DRUM KIT 1..3 loads as 66 10 77 (STD stays)", ok);
    }
    return bad;
}

int main(void)
{
    int bad = preferences() + framing() + uart_recovery() + steps() + samples() + song_protocol() + malformed_saves() +
              fm6_patches() + user_preset_roundtrip() + live_sync() + usb_burst() + menu_protocol() + drum_kit_retired();
    printf("%s\n", bad ? "EDITOR TEST FAILED" : "editor test passed");
    return bad != 0;
}
