/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FM6's voice bank (1.4.1, Discussion #168; src/fm6_vbank.c, the editor's FM6B_* in src/editor_fm6.c) against
 * simulated NOR flash: a bank written whole and read back, its sectors (0xE7000, staged in the autosave's older copy,
 * nothing else touched, the tail erased), a power cut after any programmed byte or during any erase (the old bank or
 * the new one, never half; the autosave kept), a wrong CRC and an autosave between requests changing nothing, SLOT
 * B1..B32 loading the exact records, bank slots without a voice skipped by the knob and refused elsewhere, a new
 * bank leaving a track's voice its own, projects and user presets keeping a bank voice whatever the bank holds later,
 * and the SLOT values stored before 1.4.1 (projects, user presets, motion) loading exactly as before. */
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
static long cut_bytes = -1, cut_erase = -1;     /* the power goes after that many programmed bytes / at that erase */
static int dead;                                 /* .. and stays off: nothing more is written */
static uint32_t progs, erases;
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off)
{
    if (dead)
        return -8;
    erases++;
    if (cut_erase >= 0 && cut_erase-- == 0) {    /* cut during the erase: half of it done */
        memset(nor + off, 0xFF, 2048);
        dead = 1;
        return -8;
    }
    memset(nor + off, 0xFF, 4096);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if ((off & 0xFFu) + n > 256u) {
        printf("page wrap at %#x\n", off);
        exit(1);
    }
    for (i = 0; i < n; i++) {
        if (dead || (cut_bytes >= 0 && cut_bytes-- == 0)) {
            dead = 1;
            return -9;
        }
        nor[off + i] &= s[i];
        progs++;
    }
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0; }
static void fl_plain_window_init(void) {}
#define FL_FAR(fn) (fn)
#define FL_JEDEC_OK(id) ((id) == 0x856014u)                          /* as hal/fm1_flash_map.h (FM-1) */
#include "../firmware/src/storage.c"
#include "../firmware/src/upreset.c"
#include "../firmware/src/project.c"

static uint8_t rep[1024];                         /* the editor's reply */
static uint32_t rep_n;
static void ed_b(uint32_t v) { if (rep_n < sizeof rep) rep[rep_n++] = (uint8_t)(v & 127u); }
static void ed_str(const char *s, uint32_t max)
{
    uint32_t i;
    for (i = 0; s && s[i] && i < max; i++)
        ed_b((uint8_t)s[i]);
    ed_b(0);
}
static int ed_flash_stop(void) { return transport_busy(); }
#include "../firmware/src/editor_fm6.c"

static int check(const char *what, int ok)
{
    printf("fm6bank: %-96s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

static uint32_t request(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    rep_n = 0;
    if (!ed_fm6_handle(cmd, a, n))
        return 0;
    return rep_n;
}

/* a bank: per voice a packed record (a factory patch renamed "<tag> k"), the used mask */
typedef struct {
    uint8_t v[FM6_NBANK][FM6_PACKED];
    uint32_t used;
    char name[11];
} bank_t;
static void make_bank(bank_t *b, char tag, uint32_t used, const char *name)
{
    uint32_t k;
    memset(b, 0, sizeof *b);
    for (k = 0; k < FM6_NBANK; k++) {
        uint8_t v[FP_SIZE + 1u];
        char nm[11];
        fm6_unpack(FM6_FACTORY[(k + (uint32_t)tag) % FM6_NFACTORY], v);
        snprintf(nm, sizeof nm, "%c VOICE %02u", tag, (unsigned)(k + 1u));
        memcpy(v + FP_NAME, nm, 10);
        v[FP_ALG] = (uint8_t)((k * 7u + (uint32_t)tag) % 32u);   /* (each voice its own bytes) */
        v[FP_OP * 2u + FP_OL] = (uint8_t)(50u + k);
        fm6_pack(v, b->v[k]);
    }
    b->used = used;
    snprintf(b->name, sizeof b->name, "%s", name);
}

/* the editor's transfer: BEGIN, a WRITE per used voice, END with the name and the CRC (crc_xor: a wrong one) -> END's rc
 * (or the first rc that was not 0) */
static uint32_t send_bank(const bank_t *b, uint32_t crc_xor)
{
    uint8_t a[1 + FM6_PACKED], e[15];
    uint32_t k, c = 0xFFFFFFFFu, rc;
    request(ED_FM6B_BEGIN, a, 0);
    if ((rc = rep[0]) != 0)
        return rc;
    for (k = 0; k < FM6_NBANK; k++) {
        if (!((b->used >> k) & 1u))
            continue;
        a[0] = (uint8_t)k;
        memcpy(a + 1, b->v[k], FM6_PACKED);
        request(ED_FM6B_WRITE, a, sizeof a);
        if ((rc = rep[1]) != 0)
            return rc;
        c = st_crc32_add(c, b->v[k], FM6_PACKED);
    }
    c = ~c ^ crc_xor;
    memset(e, ' ', 10);
    memcpy(e, b->name, strlen(b->name));
    for (k = 0; k < 5u; k++)
        e[10 + k] = (uint8_t)((c >> (7u * k)) & 127u);
    request(ED_FM6B_END, e, sizeof e);
    return rep[0];
}

/* the bank the device holds now equals b (its voices, the used mask, the name: LIST) */
static int bank_is(const bank_t *b)
{
    uint8_t pk[FM6_PACKED];
    const fvb_info_t *in = fvb_info();
    char nm[11] = {0};
    uint32_t k;
    if (!in || fm6_bank_used != b->used)
        return 0;
    memcpy(nm, in->name, 10);
    for (k = 10; k && nm[k - 1] == ' '; k--)
        nm[k - 1] = 0;
    if (strcmp(nm, b->name))
        return 0;
    for (k = 0; k < FM6_NBANK; k++) {
        int has = !fvb_read(k, pk);
        if (has != (int)((b->used >> k) & 1u) || (has && memcmp(pk, b->v[k], FM6_PACKED)))
            return 0;
    }
    return 1;
}

static void reset(void)
{
    uint32_t k;
    memset(nor, 0xFF, sizeof nor);
    memset(&song, 0, sizeof song);
    memset(trk, 0, sizeof trk);
    memset(&chain, 0, sizeof chain);
    chain_defaults(&chain_config);
    memset(proj_slot, 0, sizeof proj_slot);
    memset(up_bank, 0, sizeof up_bank);
    memset(&upf, 0, sizeof upf);
    memset(&ui, 0, sizeof ui);
    memset(&motion, 0, sizeof motion);
    memset(&fvb_tx, 0, sizeof fvb_tx);
    host_tracks_init();
    fm6_init();
    for (k = 0; k < NTRK; k++) {
        trk[k].eng_req = trk[k].engine = (uint8_t)ENGI_FM6;
        for (uint32_t i = 0; i < 8u; i++)
            trk[k].p[P_E0 + i] = ENG_FM6.edit[i].def;
        trk[k].p[P_E7] = FM6_OWN;
        fm6_slot[k] = FM6_OWN;
    }
    fm6_own_ok = 0;
    flash_ok = 1;
    dead = 0;
    cut_bytes = cut_erase = -1;
    fm1_ms = 0;
    transport_req = 0;
    up_boot();
}

static void slot_to(uint32_t tr, int32_t s)       /* SLOT set (the editor, MIDI, motion), then the main loop */
{
    trk[tr].p[P_E7] = (int16_t)s;
    fm6_poll();
}
static int patch_is(uint32_t tr, const uint8_t *pk)
{
    uint8_t got[FM6_PACKED];
    fm6_pack(fm6_patch[tr], got);
    return !memcmp(got, pk, FM6_PACKED);
}

static int store_test(void)
{
    int bad = 0, ok;
    static uint8_t snap[0x100000];
    static bank_t a, b;
    uint8_t as_data[600], got[600], arg[1 + FM6_PACKED];
    uint32_t k;
    reset();
    ok = !fm6_bank_used && !fvb_info() && fm6_bank_read;
    request(ED_FM6B_LIST, arg, 0);
    bad += check("an empty flash: no bank; LIST: 32 voices, no bank, \"\", every voice empty",
                 ok && rep_n == 2u + 1u + 32u * 2u && rep[0] == 32 && rep[1] == 0 && rep[2] == 0 && rep[3] == 0 && rep[4] == 0);
    memset(as_data, 'S', sizeof as_data);
    st_save(OBJ_AUTOSAVE, as_data, sizeof as_data);      /* an autosave (copy A) */
    memcpy(snap, nor, sizeof nor);
    make_bank(&a, 'A', 0xFFFFFFFFu, "BANK A");
    ok = send_bank(&a, 0) == 0 && rep[1] == 32 && bank_is(&a);
    bad += check("a whole bank written over the editor (BEGIN, 32 WRITEs, END): rc 0, 32 voices, read back exactly", ok);
    ok = 1;
    for (k = 0; k < 0x100000u; k++)
        if (nor[k] != snap[k] && !(k >= 0xE6000u && k < 0xE8000u))
            ok = 0;
    bad += check("  .. it wrote 0xE7000 (the bank) and 0xE6000 (the autosave's older copy, staged), nothing else", ok &&
                 ((st_hdr_t *)(nor + 0xE7000u))->type == OBJ_FM6VB && ((st_hdr_t *)(nor + 0xE6000u))->type == OBJ_FM6VB);
    ok = 1;
    for (k = 0xE7F00u; k < 0xE8000u; k++)
        ok &= nor[k] == 0xFFu;
    bad += check("  .. the sector's tail (0xE7F00..) erased: no update record can be read there", ok);
    bad += check("  .. the autosave still loads (its current copy untouched)",
                 st_load(OBJ_AUTOSAVE, got, sizeof got) == (int)sizeof as_data && !memcmp(got, as_data, sizeof as_data));
    request(ED_FM6B_LIST, arg, 0);
    ok = rep[0] == 32 && rep[1] == 1 && !memcmp(rep + 2, "BANK A", 7);
    {
        uint32_t o = 9, n = 0;
        for (k = 0; k < 32u && ok; k++) {
            char nm[16];
            snprintf(nm, sizeof nm, "A VOICE %02u", (unsigned)(k + 1u));
            ok &= rep[o] == 1 && !strcmp((const char *)rep + o + 1, nm);
            o += 2u + (uint32_t)strlen(nm);
            n++;
        }
        ok &= o == rep_n && n == 32u;
    }
    bad += check("  .. LIST: the bank's name and each voice's name", ok);
    memset(&upf, 0, sizeof upf);                         /* a power-on: the RAM lost, the stores read */
    fm6_bank_used = 0;
    up_boot();
    bad += check("  .. after a power-on: the same bank", bank_is(&a));

    /* a wrong CRC: nothing changes */
    make_bank(&b, 'B', 0x80000021u, "BANK B");
    ok = send_bank(&b, 1) == 2 && rep[1] == 32 && bank_is(&a);
    bad += check("a wrong CRC: rc 2, the bank stays", ok);
    /* requests out of order */
    arg[0] = 0;
    memcpy(arg + 1, b.v[0], FM6_PACKED);
    request(ED_FM6B_WRITE, arg, sizeof arg);
    ok = rep[0] == 0 && rep[1] == 5;
    request(ED_FM6B_BEGIN, arg, 0);
    request(ED_FM6B_WRITE, arg, sizeof arg);
    ok &= rep[1] == 0;
    request(ED_FM6B_WRITE, arg, sizeof arg);
    ok &= rep[1] == 1;
    arg[0] = 32;
    request(ED_FM6B_WRITE, arg, sizeof arg);
    ok &= rep[1] == 1;
    request(ED_FM6B_WRITE, arg, 20);
    ok &= rep[1] == 1;
    request(ED_FM6B_END, arg, 3);
    ok &= rep[0] == 1;
    ok &= bank_is(&a);
    bad += check("WRITE without BEGIN: rc 5; a voice twice, index 32, a short record, a short END: rc 1; the bank stays", ok);
    /* an autosave between two requests takes the transfer's sector: the transfer ends, nothing else lost */
    request(ED_FM6B_BEGIN, arg, 0);
    ok = rep[0] == 0;
    memset(as_data, 'T', sizeof as_data);
    st_save(OBJ_AUTOSAVE, as_data, sizeof as_data);      /* (into the copy BEGIN took: the older one) */
    arg[0] = 1;
    request(ED_FM6B_WRITE, arg, sizeof arg);
    ok &= rep[1] == 5;
    request(ED_FM6B_END, arg, 0);                         /* (a short END: 1) */
    ok &= rep[0] == 1;
    {
        uint8_t e[15] = {0};
        request(ED_FM6B_END, e, sizeof e);
        ok &= rep[0] == 5;
    }
    bad += check("an autosave between BEGIN and WRITE: WRITE / END answer 5; the bank and the new autosave kept",
                 ok && bank_is(&a) && st_load(OBJ_AUTOSAVE, got, sizeof got) == (int)sizeof as_data &&
                     !memcmp(got, as_data, sizeof as_data));
    /* the next bank: staged in the autosave's older copy again (A now), the new autosave (B) kept */
    ok = send_bank(&b, 0) == 0 && rep[1] == 3 && bank_is(&b) && st_load(OBJ_AUTOSAVE, got, sizeof got) == (int)sizeof as_data &&
         !memcmp(got, as_data, sizeof as_data) && ((st_hdr_t *)(nor + 0xE5000u))->type == OBJ_FM6VB;
    bad += check("a second bank (3 voices): staged in the other copy, the autosave kept, read back exactly", ok);
    /* an empty bank: BEGIN, END (the CRC of nothing) */
    {
        bank_t e;
        make_bank(&e, 'E', 0, "");
        ok = send_bank(&e, 0) == 0 && rep[1] == 0 && bank_is(&e) && !fm6_bank_used;
    }
    bad += check("an empty bank (no WRITE): valid, no voice", ok);
    /* no flash: refused */
    flash_ok = 0;
    request(ED_FM6B_BEGIN, arg, 0);
    ok = rep[0] == 4;
    flash_ok = 1;
    transport_req = 0;
    bad += check("no flash: BEGIN rc 4", ok);
    return bad;
}

/* a power cut after every programmed byte, and during every erase, of a bank's transfer: the old bank or the new
 * one after the next power-on, never anything else; the autosave's current copy always kept */
static int cut_test(void)
{
    int bad = 0;
    static uint8_t pre[0x3000];
    static bank_t a, b;
    uint8_t as_data[600], got[600];
    uint32_t n, old = 0, new = 0, other = 0, total, e, eold = 0, enew = 0;
    reset();
    memset(as_data, 'S', sizeof as_data);
    st_save(OBJ_AUTOSAVE, as_data, sizeof as_data);
    make_bank(&a, 'A', 0x0000FFFFu, "OLD");
    make_bank(&b, 'B', 0xFFFFFFFFu, "NEW");
    send_bank(&a, 0);
    memcpy(pre, nor + 0xE5000u, sizeof pre);              /* (only these three sectors are ever written here) */
    progs = 0;
    send_bank(&b, 0);
    total = progs;
    for (n = 0; n < total; n++) {
        memcpy(nor + 0xE5000u, pre, sizeof pre);
        memset(&fvb_tx, 0, sizeof fvb_tx);
        fvb_boot();
        dead = 0;
        cut_bytes = (long)n;
        send_bank(&b, 0);
        dead = 0;
        cut_bytes = -1;
        fvb_boot();                                       /* the next power-on */
        if (bank_is(&a))
            old++;
        else if (bank_is(&b))
            new++;
        else
            other++;
        if (st_load(OBJ_AUTOSAVE, got, sizeof got) != (int)sizeof as_data || memcmp(got, as_data, sizeof as_data))
            other++;
    }
    for (e = 0; e < 2u; e++) {                            /* BEGIN's erase, the copy's erase */
        memcpy(nor + 0xE5000u, pre, sizeof pre);
        memset(&fvb_tx, 0, sizeof fvb_tx);
        fvb_boot();
        dead = 0;
        cut_erase = (long)e;
        send_bank(&b, 0);
        dead = 0;
        cut_erase = -1;
        fvb_boot();
        eold += bank_is(&a);
        enew += bank_is(&b);
        if (st_load(OBJ_AUTOSAVE, got, sizeof got) != (int)sizeof as_data || memcmp(got, as_data, sizeof as_data))
            other++;
    }
    printf("fm6bank:   %u cut points (bytes): %u the old bank, %u the new one, %u anything else\n", (unsigned)total,
           (unsigned)old, (unsigned)new, (unsigned)other);
    bad += check("a power cut after any programmed byte of a transfer: the old bank or the new one, never half",
                 !other && old && new && old + new == total && total > 7000u);
    bad += check("  .. a cut during BEGIN's erase: the old bank; during the copy's erase: the new one (finished at boot)",
                 eold == 1u && enew == 1u && !other);
    return bad;
}

static int slot_test(void)
{
    int bad = 0, ok;
    static bank_t a, b;
    const param_desc_t *d = &ENG_FM6.edit[7];
    uint32_t k;
    reset();
    ok = param_turn(d, FM6_OWN, 1) == FM6_OWN && param_turn(d, FM6_OWN, -1) == FM6_OWN - 1 &&   /* (GHOULBOX: F15) */
         param_turn(d, FM6_OWN - 1, 1) == FM6_OWN && param_turn(d, FM6_OWN - 1, 5) == FM6_OWN;
    slot_to(0, FM6_OWN + 3);
    ok &= trk[0].p[P_E7] == FM6_OWN && fm6_slot[0] == FM6_OWN;
    bad += check("no bank: the knob stops at OWN (B1..B32 skipped); a B value from elsewhere is refused (SLOT back)", ok);
    make_bank(&a, 'A', 0xFFFFFFFFu, "FULL");
    send_bank(&a, 0);
    ok = 1;
    for (k = 0; k < FM6_NBANK; k++) {
        slot_to(1, (int32_t)(FM6_OWN + 1u + k));
        ok &= fm6_slot[1] == FM6_OWN + 1u + k && trk[1].p[P_E7] == (int16_t)(FM6_OWN + 1u + k) && patch_is(1, a.v[k]);
    }
    bad += check("SLOT B1..B32 (16..47: GHOULBOX's F1..F15, OWN before them): each loads its voice's exact record into the track", ok);
    {
        char val[8];
        const char *u;
        param_format(d, FM6_OWN + 12, val, &u);
        ok = !strcmp(val, "B12");
        param_format(d, FM6_OWN + 32, val, &u);
        ok &= !strcmp(val, "B32");
    }
    bad += check("  .. the card shows B12, B32", ok);
    /* from OWN the own patch is kept aside, back to OWN brings it back (as for F n) */
    {
        uint8_t own[FM6_PACKED];
        slot_to(2, FM6_OWN);
        fm6_load_slot(2, 3);
        fm6_adopt(2);
        trk[2].p[P_E7] = FM6_OWN;
        fm6_slot[2] = FM6_OWN;
        fm6_pack(fm6_patch[2], own);
        own[110] = 30;                                    /* (an own patch: no factory one) */
        {
            uint8_t v[FP_SIZE + 1u];
            fm6_unpack(own, v);
            fm6_set_patch(2, v);
        }
        slot_to(2, FM6_OWN + 5);
        ok = patch_is(2, a.v[4]);
        slot_to(2, FM6_OWN);
        ok &= patch_is(2, own);
    }
    bad += check("OWN -> B5 -> OWN: the own patch kept aside and back", ok);
    /* a partial bank: the knob passes the empty slots */
    slot_to(0, FM6_OWN + 5);
    make_bank(&b, 'B', 1u | 1u << 5 | 1u << 31, "SOME");
    send_bank(&b, 0);
    ok = trk[0].p[P_E7] == FM6_OWN && fm6_slot[0] == FM6_OWN && patch_is(0, a.v[4]);
    bad += check("a new bank: the track on B5 keeps that voice as its own (SLOT OWN)", ok);
    ok = param_turn(d, FM6_OWN, 1) == FM6_OWN + 1 && param_turn(d, FM6_OWN + 1, 1) == FM6_OWN + 6 &&
         param_turn(d, FM6_OWN + 6, 1) == FM6_OWN + 32 && param_turn(d, FM6_OWN + 32, 1) == FM6_OWN + 32 &&
         param_turn(d, FM6_OWN + 32, -1) == FM6_OWN + 6 && param_turn(d, FM6_OWN + 1, -1) == FM6_OWN &&
         param_turn(d, FM6_OWN, 3) == FM6_OWN + 6 && param_turn(d, FM6_OWN + 6, 9) == FM6_OWN + 32 &&
         param_turn(d, FM6_OWN + 6, 40) == FM6_OWN + 32;
    bad += check("bank voices 1, 6, 32 only: the knob goes OWN B1 B6 B32 and back, the empty slots skipped", ok);
    slot_to(0, FM6_OWN + 6);
    ok = patch_is(0, b.v[5]);
    slot_to(0, FM6_OWN + 2);                              /* B2: none there (the editor's SET, a MIDI CC) */
    ok &= trk[0].p[P_E7] == FM6_OWN + 6 && fm6_slot[0] == FM6_OWN + 6 && patch_is(0, b.v[5]);
    bad += check("SET B2 (no voice there) on B6: refused, SLOT back to B6, its voice kept", ok);
    slot_to(0, 2);
    ok = trk[0].p[P_E7] == 2 && patch_is(0, FM6_FACTORY[2]);
    bad += check("  .. F3 from there: the factory patch as always", ok);
    return bad;
}

/* projects, user presets and motion: a bank voice stays with what was saved; stored SLOT values load as before */
static int keep_test(void)
{
    int bad = 0, ok;
    static bank_t a, b;
    uint8_t pk[FM6_PACKED];
    reset();
    make_bank(&a, 'A', 0xFFFFFFFFu, "FIRST");
    make_bank(&b, 'B', 0xFFFFFFFFu, "SECOND");
    send_bank(&a, 0);
    song.sel = 0;
    slot_to(0, FM6_OWN + 5);                              /* track 1 on B5 */
    slot_to(1, FM6_OWN + 7);                              /* track 2 on B7 */
    ok = project_save(0) == 0;
    song.sel = 1;
    ok &= up_store(5, "BANK B7") == 0;
    send_bank(&b, 0);                                     /* the bank replaced */
    slot_to(0, 1);
    slot_to(1, 1);
    memset(proj_slot, 0, sizeof proj_slot);              /* (from flash) */
    project_load(0);
    ok &= patch_is(0, a.v[4]) && trk[0].p[P_E7] == FM6_OWN && patch_is(1, a.v[6]) && trk[1].p[P_E7] == FM6_OWN;
    bad += check("a project saved on B5 / B7: loads those voices (SLOT OWN) after the bank was replaced", ok);
    song.sel = 2;
    trk[2].p[P_E7] = 0;
    fm6_poll();
    up_load(5);
    ok = patch_is(2, a.v[6]) && trk[2].p[P_E7] == FM6_OWN && !upf_get(5, pk) && !memcmp(pk, a.v[6], FM6_PACKED);
    bad += check("a user preset stored on B7: loads that voice (SLOT OWN) after the bank was replaced", ok);

    /* SLOT values stored before 1.4.1, with this bank in flash: never read as its voices */
    {
        project_t *p = &proj_scratch;
        int32_t s;
        uint8_t x[FM6_PACKED];
        project_capture(p);
        memcpy(x, FM6_FACTORY[3], FM6_PACKED);
        x[110] = 17;                                      /* (an own patch) */
        ok = 1;
        for (s = FM6_OWN + 1; s <= (int32_t)(FM6_OWN + FM6_NBANK); s++) {
            p->t[0].p[P_E7] = (int16_t)s;                 /* (1.0.2's B2..B27: 9..34; any 9..40) */
            memcpy(p->fm6[0], x, FM6_PACKED);
            p->sum = proj_sum(p);
            ok &= !project_restore_runtime(p) && trk[0].p[P_E7] == FM6_OWN && fm6_slot[0] == FM6_OWN && patch_is(0, x);
            fm6_poll();
            ok &= patch_is(0, x) && trk[0].p[P_E7] == FM6_OWN;
        }
        p->t[0].p[P_E7] = 2;
        memcpy(p->fm6[0], FM6_FACTORY[2], FM6_PACKED);
        p->sum = proj_sum(p);
        ok &= !project_restore_runtime(p) && trk[0].p[P_E7] == 2 && patch_is(0, FM6_FACTORY[2]);
        bad += check("a project with a stored SLOT 9..40: its own patch, SLOT OWN (never a bank voice); F3 as before", ok);
    }
    {   /* a user preset record with a stored SLOT 13 (1.0.2's B5): with its patch, that patch; without, the init voice */
        up_rec_t *r;
        song.sel = 3;
        slot_to(3, FM6_OWN + 9);
        up_store(6, "OLD B");
        r = (up_rec_t *)up_rec(6);
        up_set_value(r, P_E7, FM6_OWN + 5);
        upf_set(6, a.v[1]);                              /* (tagged with the record as it is now) */
        slot_to(3, 0);
        up_load(6);
        ok = patch_is(3, a.v[1]) && trk[3].p[P_E7] == FM6_OWN;
        up_set_value(r, P_E0, 3);                        /* the record changed: its patch no longer counts */
        up_load(6);
        ok &= patch_is(3, FM6_INIT) && trk[3].p[P_E7] == FM6_OWN;
        bad += check("a user preset with a stored SLOT 13: its patch (OWN); with none, the init voice as before", ok);
    }
    {   /* motion records holding SLOT 9..40 (recorded on 1.0.2's B slots): they play OWN, never a bank voice */
        uint8_t x[FM6_PACKED];
        track_t *t = &trk[0];
        int32_t s;
        memcpy(x, FM6_FACTORY[1], FM6_PACKED);
        x[110] = 9;
        {
            uint8_t v[FP_SIZE + 1u];
            fm6_unpack(x, v);
            fm6_set_patch(0, v);
            fm6_adopt(0);
        }
        ok = trk[0].p[P_E7] == FM6_OWN;
        for (s = FM6_OWN + 1; s <= (int32_t)(FM6_OWN + FM6_NBANK); s++) {
            memset(&motion, 0, sizeof motion);
            motion.on = 1;
            motion.count = 1;
            motion.event[0].place = 3;                    /* track 1, step 3 */
            motion.event[0].param = P_E7;
            motion.event[0].value = (int8_t)s;
            motion_base_valid = 0;
            motion_step(t, 3, &motion, 1);
            fm6_poll();
            ok &= trk[0].p[P_E7] == FM6_OWN && patch_is(0, x);
        }
        ok &= motion_put(t, 4, P_E7, FM6_OWN + 3, 0) == 0 && motion.event[motion.count - 1u].value == FM6_OWN;
        bad += check("motion holding SLOT 9..40: plays OWN, the patch kept; a record made on B3 stores OWN", ok);
        memset(&motion, 0, sizeof motion);
    }
    return bad;
}

int main(void)
{
    int bad = 0;
    bad += store_test();
    bad += cut_test();
    bad += slot_test();
    bad += keep_test();
    printf(bad ? "fm6bank: FAILED (%d)\n" : "fm6bank: all ok\n", bad);
    return bad ? 1 : 0;
}
