/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Firmware save flows against NOR flash: deferred settings, retries, and failed-save rollback. */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int32_t fm1_enc_take(uint32_t e) { (void)e; return 0; }
static uint32_t irq_start_race, irq_races;
static void fm1_irq_off(void)
{
    if (irq_start_race) {                         /* audio serviced PLAY immediately before main masks it */
        irq_start_race = 0;
        events_block(CTL);
        irq_races++;
    }
}
static void fm1_irq_on(void) {}
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{ (void)x; (void)y; (void)w; (void)h; (void)p; }
#define FELUCCA_FLASH 1
#include "../firmware/src/gfx.c"
#include "../firmware/src/panel.c"
#include "../firmware/src/ui.c"
static void panel_setup(void) {}

static uint8_t nor[0x100000], flash_ok = 1;
static int erase_error, fail_after = -1;
static uint32_t erases;
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off)
{
    erases++;
    if (erase_error) return -8;
    memset(nor + off, 0xFF, 4096);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if (fail_after == 0) return -9;
    if (fail_after > 0) fail_after--;
    for (i = 0; i < n; i++) nor[off + i] &= s[i];
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0; }
static void fl_plain_window_init(void) {}
#define FL_FAR(fn) (fn)
#include "../firmware/src/storage.c"
#include "../firmware/src/upreset.c"
#include "../firmware/src/project.c"

enum { ED_UI_STATE = 34, ED_UI_SET, ED_UI_PALETTES, ED_FAV_GET, ED_FAV_SET };
static void ed_b(uint32_t n) { (void)n; }
static void ed_v(int32_t n) { (void)n; }
static int32_t ed_rv(const uint8_t *a) { return (int32_t)(a[0] | (uint32_t)a[1] << 7) - 8192; }
static void ed_str(const char *s, uint32_t n) { (void)s; (void)n; }
#include "../firmware/src/editor_preferences.c"

static int check(const char *what, int ok)
{
    printf("persist: %-66s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

static void reset(void)
{
    memset(nor, 0xFF, sizeof nor);
    memset(&song, 0, sizeof song);
    memset(trk, 0, sizeof trk);
    memset(&chain, 0, sizeof chain);
    chain_defaults(&chain_config);
    memset(proj_slot, 0, sizeof proj_slot);
    memset(up_bank, 0, sizeof up_bank);
    memset(&persist_saved, 0, sizeof persist_saved);
    memset(&settings, 0, sizeof settings);
    memset(&ui, 0, sizeof ui);
    memset(&favorites, 0, sizeof favorites);
    panel = PANEL_DEFAULT;
    host_tracks_init();
    fm1_ms = 0;
    transport_req = 0;
    persist_pending = 0;
    persist_retry_ms = 0;
    flash_ok = 1;
    erase_error = 0;
    fail_after = -1;
    up_gen = erases = 0;
    irq_start_race = irq_races = 0;
}

/* ---- 1.0.3: the retired FM6 bank's patches move into the user presets on the first boot (up_fm6.c) ---- */
static uint8_t mig_pre[0x100000];
static fm6_bank_t mig_bank;

static void mig_rec(up_bank_t *b, uint32_t i, uint32_t engine, int16_t slot, char tag)
{
    up_rec_t *r = &b->r[i];
    uint32_t k;
    memset(r, 0, sizeof *r);
    r->used = UP_USED; r->ver = UP_VER; r->engine = (uint8_t)engine; r->np = P_COUNT;
    r->name[0] = tag;
    for (k = 0; k < P_COUNT; k++) up_set_value(r, k, param_desc_of(engine, k)->def);
    up_set_value(r, P_E7, slot);
    up_set_value(r, P_E0, (int16_t)(tag & 7));          /* (each record another sound) */
}

/* a 1.0.2 flash: user presets on B3 (slot 1), F2 (2), B5 empty (3), DRUM (4), B1 (18); the bank written twice */
static void mig_setup(void)
{
    static up_bank_t b[2];
    memset(nor, 0xFF, sizeof nor);
    memset(b, 0, sizeof b);
    for (uint32_t i = 0; i < 2u; i++) { b[i].magic = UP_BANK_MAGIC; b[i].rsize = sizeof(up_rec_t); b[i].nslot = UP_PER_BANK; }
    mig_rec(&b[0], 0, ENGI_FM6, FM6_NFACTORY + 2, 'A');
    mig_rec(&b[0], 1, ENGI_FM6, 1, 'B');
    mig_rec(&b[0], 2, ENGI_FM6, FM6_NFACTORY + 4, 'C');
    mig_rec(&b[0], 3, ENGI_DRUM, FM6_NFACTORY + 2, 'D');
    mig_rec(&b[1], 1, ENGI_FM6, FM6_NFACTORY + 0, 'E');
    st_save(OBJ_UPRESET0, &b[0], sizeof b[0]);
    st_save(OBJ_UPRESET0 + 1, &b[1], sizeof b[1]);
    memset(&mig_bank, 0, sizeof mig_bank);
    mig_bank.magic = FM6_BANK_MAGIC; mig_bank.ver = 1; mig_bank.nslot = FM6_BANK_N; mig_bank.used = 1u | 1u << 2;
    memcpy(mig_bank.v[2], FM6_FACTORY[4], FM6_PACKED); memcpy(mig_bank.v[2] + 118, "OLD BANK 1", 10);
    st_save(OBJ_FM6BANK, &mig_bank, sizeof mig_bank);   /* A: an older bank */
    memcpy(mig_bank.v[0], FM6_FACTORY[6], FM6_PACKED); memcpy(mig_bank.v[0] + 118, "NEW B1    ", 10);
    memcpy(mig_bank.v[2] + 118, "NEW B3    ", 10);
    st_save(OBJ_FM6BANK, &mig_bank, sizeof mig_bank);   /* B: the newest */
}

static void mig_boot(void)                               /* a power-on: RAM lost, the stores from flash */
{
    memset(up_bank, 0, sizeof up_bank);
    memset(&upf, 0, sizeof upf);
    up_boot();
}

/* the presets play what 1.0.2 played: slot 1 the newest B3, 18 B1, 2 F2, 3 init (an empty B5), DRUM none */
static int mig_ok(void)
{
    uint8_t pk[FM6_PACKED];
    return !upf_get(0, pk) && !memcmp(pk + 118, "NEW B3    ", 10) && !upf_get(17, pk) &&
           !memcmp(pk + 118, "NEW B1    ", 10) && upf_get(1, pk) && upf_get(2, pk) && upf_get(3, pk);
}

static int mig_test(void)
{
    int bad = 0, ok = 1;
    uint32_t cut, before, cuts = 0;
    uint8_t pk[FM6_PACKED];
    upf_t got;
    reset();
    mig_setup();
    memcpy(mig_pre, nor, sizeof nor);
    mig_boot();
    bad += check("FM6 bank -> user presets: B slots get the bank's newest patches", mig_ok());
    bad += check("  .. written into A (the bank's older copy); the bank's newest copy (B) still whole",
                 st_load(OBJ_UPFM6, &got, sizeof got) == (int)sizeof got && !memcmp(&got, &upf, sizeof got) &&
                     ((st_hdr_t *)(nor + 0x9F000))->type == OBJ_UPFM6 && st_load(OBJ_FM6BANK, &mig_bank, sizeof mig_bank) > 0 &&
                     !memcmp(mig_bank.v[0] + 118, "NEW B1", 6));
    up_load(0);
    bad += check("  .. UP_LOAD 1 plays the bank's B3, SLOT OWN", !memcmp(fm6_patch[song.sel] + FP_NAME, "NEW B3    ", 10) &&
                 TSEL->p[P_E7] == FM6_OWN);
    up_load(1);
    bad += check("  .. UP_LOAD 2 (F2) plays F2, SLOT F2", !memcmp(fm6_patch[song.sel] + FP_NAME, FM6_FACTORY[1] + 118, 10) &&
                 TSEL->p[P_E7] == 1);
    up_load(2);
    bad += check("  .. UP_LOAD 3 (an empty B5) plays the init voice, as before",
                 !memcmp(fm6_patch[song.sel] + FP_NAME, "INIT VOICE", 10) && TSEL->p[P_E7] == FM6_OWN);
    before = erases;
    mig_boot();
    bad += check("  .. the next boot: done (no write), the same patches", mig_ok() && erases == before);
    /* a power cut at every write step of the migration: the next boot ends with the same result */
    for (cut = 0; cut < 64u && ok; cut++) {
        memcpy(nor, mig_pre, sizeof nor);
        fail_after = (int)cut;
        mig_boot();                                      /* (writes stop at the cut: the rest fails) */
        if (fail_after > 0) break;                       /* (this boot finished before the cut) */
        fail_after = -1;
        cuts++;
        mig_boot();
        ok &= mig_ok() && st_load(OBJ_UPFM6, &got, sizeof got) == (int)sizeof got;
    }
    memcpy(nor, mig_pre, sizeof nor);
    erase_error = 1;
    mig_boot();                                          /* a cut before the erase */
    erase_error = 0;
    mig_boot();
    ok &= mig_ok();
    bad += check("a power cut at any step of the migration loses nothing (the next boot redoes it)", ok && cuts >= 10u);
    /* after it: a preset save writes the other sector (the bank's last copy); a cut there keeps the first copy */
    memcpy(nor, mig_pre, sizeof nor);
    fail_after = -1;
    mig_boot();
    trk[song.sel].eng_req = ENGI_FM6;
    fm6_load_slot(song.sel, 3);
    fail_after = (int)((sizeof(up_bank_t) + 255u) / 256u + 1u + 3u);   /* the preset bank's write passes, the patches' fails */
    up_store(20, "CUT");
    fail_after = -1;
    mig_boot();
    bad += check("a cut while a preset's patch is written: the others kept, that preset without one",
                 mig_ok() && upf_get(20, pk));
    up_store(20, "SAVED");
    mig_boot();
    bad += check("saved again: its patch, and the bank's sectors now both hold the patches object",
                 mig_ok() && !upf_get(20, pk) && !memcmp(pk + 118, FM6_FACTORY[3] + 118, 10) &&
                     ((st_hdr_t *)(nor + 0x9F000))->type == OBJ_UPFM6 && ((st_hdr_t *)(nor + 0xFE000))->type == OBJ_UPFM6);
    memset(nor, 0xFF, sizeof nor);                        /* a device that never had a bank: nothing written */
    before = erases;
    mig_boot();
    bad += check("no bank: nothing to move, nothing written", erases == before && upf_valid(&upf));
    return bad;
}

int main(void)
{
    int bad = 0, ok;
    uint32_t before;
    persist_t p;
    project_store_t old, got;
    project_v5_t v5;
    up_bank_t bank;
    up_rec_t r;
    reset();
    settings_init();
    {
        const uint8_t amber[] = {0, 2};
        song.playing = 1;
        bad += check("editor display change while playing is applied and reports deferred saving",
                      ed_ui_set(amber, sizeof amber) == 4 && settings.palette == 2u && persist_pending == 1 && !erases);
        song.playing = 0;
        settings_poll();
        bad += check("STOP commits the deferred palette setting",
                      !persist_pending && erases == 1 && persist_saved.palette == palette_to_stored(2));
        before = erases;
        bad += check("repeating an unchanged editor preference succeeds without erasing",
                      ed_ui_set(amber, sizeof amber) == 0 && erases == before);
        erase_error = 1;
        const uint8_t mono[] = {0, 0};
        bad += check("editor reports failed saving while preserving the previous flash setting",
                      ed_ui_set(mono, sizeof mono) == 3 && settings.palette == 0u && persist_pending == 2 &&
                      persist_saved.palette == palette_to_stored(2));
        erase_error = 0;
        fm1_ms += 1000;
        settings_poll();
        bad += check("failed editor preference retries through the existing persistence path",
                      !persist_pending && persist_saved.palette == palette_to_stored(0));
        const uint8_t bold[] = {1, 1};
        bad += check("the retired font weight is refused without a write", ed_ui_set(bold, sizeof bold) == 2);
    }
    reset();
    settings.palette = 3;
    settings.lowcut = 2;
    settings.zoom = 1;
    persist_saved.bold = 1;                       /* (an earlier firmware's font weight: kept as saved) */
    favorites.factory[8][0] = 1;
    favorites.user = 1u << 31;
    song.playing = 1;
    settings_save();
    bad += check("playing: settings queued, no erase, runtime kept", persist_pending && !erases &&
                  settings.palette == 3u && settings.lowcut == 2u && settings.zoom == 1u);
    song.playing = 0;
    transport_req = 1;
    settings_poll();
    bad += check("pending PLAY: settings still queued, no erase", persist_pending && !erases);
    transport_req = 0;
    chain.armed = 1;
    settings_poll();
    bad += check("pending SONG: settings still queued, no erase", persist_pending && !erases);
    chain.armed = 0;
    settings.palette = 2;
    settings_poll();
    bad += check("STOP: latest settings saved and request cleared", !persist_pending && erases == 1u &&
                  st_load(OBJ_SETTINGS, &p, sizeof p) == (int)sizeof p && p.palette == palette_to_stored(2) &&
                  p.lowcut == 2u && p.zoom == 1u && p.bold == 1u &&
                  p.favorites.factory[8][0] == 1 && p.favorites.user == (1u << 31) &&
                  !memcmp(&p.panel, &panel, sizeof panel));
    settings_save();
    bad += check("unchanged settings do not erase", !persist_pending && erases == 1u);
    settings.palette = 1;
    erase_error = 1;
    fm1_ms = 0xFFFFFF00u;
    settings_save();
    before = erases;
    settings_poll();
    fm1_ms += 999u;
    settings_poll();
    bad += check("failed settings retry waits through clock wrap", persist_pending && erases == before &&
                  persist_saved.palette == palette_to_stored(2) && settings.palette == 1u);
    erase_error = 0;
    fm1_ms++;
    settings_poll();
    bad += check("settings retry succeeds after 1000 ms", !persist_pending && erases == before + 1u &&
                  st_load(OBJ_SETTINGS, &p, sizeof p) == (int)sizeof p && p.palette == palette_to_stored(1));

    reset();
    trk[0].step[0] = (step_t){{60}, 1, ST_NOTE, 0, 96, 0, 0};
    project_save(0);
    old = proj_slot[0];
    trk[0].step[0].note[0] = 72;
    fail_after = 1;
    project_save(0);
    fail_after = -1;
    bad += check("failed project save keeps RAM and old flash", !strcmp(ui.msg, "SAVE ERROR") &&
                  !memcmp(&proj_slot[0], &old, sizeof old) && st_load(OBJ_PROJECT0, &got, sizeof got) == (int)sizeof got &&
                  !memcmp(&got, &old, sizeof got));
    project_save(0);
    bad += check("successful project save publishes new RAM", !strcmp(ui.msg, "SAVED") &&
                  proj_import(&proj_scratch, &proj_slot[0], sizeof proj_slot[0]) &&
                  proj_scratch.t[0].step[0].note[0] == 72u && proj_ok(&proj_scratch));
    before = erases;
    transport_req = 1;
    project_save(1);
    bad += check("pending PLAY: project save leaves slot and flash", !project_used(1) && erases == before &&
                  !strcmp(ui.msg, "STOP TO SAVE"));
    transport_req = 0;
    erase_error = 1;
    project_save(2);
    bad += check("failed first project save leaves slot empty", !project_used(2));
    erase_error = 0;
    {   /* names: a project's through flash; a failed rename keeps the old one in RAM and flash */
        char n[16];
        project_store_t keep;
        step_t st0 = trk[0].step[0];
        project_save_as(0, "LOFI JAM");
        memset(proj_slot[0].raw, 0, 8);                       /* (the RAM copy lost: read it back from flash) */
        proj_fetch(0);
        bad += check("a project's name goes to flash and comes back", project_name(0, n) && !strcmp(n, "LOFI JAM") &&
                      !strcmp(proj_name, "LOFI JAM"));
        keep = proj_slot[0];
        fail_after = 1;
        bad += check("failed project rename: SAVE ERROR, the slot and flash as they were",
                      project_rename(0, "DUB") == 2 && !strcmp(ui.msg, "SAVE ERROR") && !memcmp(&proj_slot[0], &keep, sizeof keep) &&
                      st_load(OBJ_PROJECT0, &got, sizeof got) == (int)sizeof got && !memcmp(&got, &keep, sizeof got));
        fail_after = -1;
        before = erases;
        transport_req = 1;
        bad += check("pending PLAY: no rename, no erase", project_rename(0, "DUB") == 1 && erases == before &&
                      !strcmp(ui.msg, "STOP TO SAVE"));
        transport_req = 0;
        bad += check("project rename: flash has the new name, the music as it was", project_rename(0, "DUB") == 0 &&
                      !strcmp(ui.msg, "RENAMED") && (memset(proj_slot[0].raw, 0, 8), proj_fetch(0), project_name(0, n)) &&
                      !strcmp(n, "DUB") && proj_scratch.t[0].step[0].note[0] == st0.note[0]);
    }
    {   /* FUN7 itself refuses a step outside its fields: a damaged slot is empty, the tracks stay */
        step_t keep = trk[0].step[0];
        old = proj_slot[0];
        proj_slot[0].raw[68 + P_COUNT + 2 + 4] = 7;               /* track 1 step 1: n = 7 */
        bad += check("damaged FUN7 slot (hash ok, n > 4) is refused", (memcpy(proj_slot[0].raw + PROJ_STORE_SIZE - 4,
                      &(uint32_t){proj_hash(proj_slot[0].raw, PROJ_STORE_SIZE - 4)}, 4), !project_used(0)));
        st_save(OBJ_PROJECT0, &proj_slot[0], sizeof proj_slot[0]);
        ui.msg[0] = 0;
        project_load(0);
        bad += check("loading it says EMPTY SLOT and keeps the steps", !strcmp(ui.msg, "EMPTY SLOT") &&
                      !memcmp(&trk[0].step[0], &keep, sizeof keep));
        proj_slot[0] = old;
        st_save(OBJ_PROJECT0, &proj_slot[0], sizeof proj_slot[0]);
    }
    {   /* an older format (FUN5) with values FUN7 cannot pack still loads, bounded */
        uint32_t i;
        memset(&v5, 0, sizeof v5);
        v5.magic = PROJ_MAGIC_V5;
        v5.size = sizeof v5;
        for (i = 0; i < G_COUNT; i++)
            v5.g[i] = GP[i].def;
        for (i = 0; i < NTRK; i++) {
            v5.t[i].engine = trk[i].engine;
            memcpy(v5.t[i].p, trk[i].p, sizeof v5.t[i].p);
        }
        v5.t[0].p[P_LEVEL] = 900;                                  /* out of range */
        v5.t[0].step[0] = (step10_t){{255, 128, 160, 200}, 255, 255, 0, 96, 255, 5};
        v5.sum = proj_hash(&v5, sizeof v5 - 4u);
        st_save(OBJ_PROJECT0 + 3u, &v5, sizeof v5);
        memset(proj_slot[3].raw, 0, 4);
        project_load(3);
        bad += check("FUN5 load bounds steps and masks lane accents", trk[0].step[0].n == 4u &&
                      trk[0].step[0].time == ST_REST && trk[0].step[0].note[0] == 127u &&
                      !trk[0].step[0].note[1] && trk[0].step[0].acc == 5u && project_used(3));
        bad += check("FUN5 load clamps a parameter into its range", trk[0].p[P_LEVEL] == TP[P_LEVEL].max);
        for (i = 0; i < P_COUNT; i++) {
            uint32_t e;
            for (e = 0; e < NENGINES; e++)
                if (param_desc_of(e, i)->min < -64 || param_desc_of(e, i)->max > 127) {
                    printf("param %u engine %u: range %d..%d does not fit FUN7's byte\n", i, e,
                           param_desc_of(e, i)->min, param_desc_of(e, i)->max);
                    bad++;
                }
        }
    }
    chain_config.count = 1;
    chain_config.row[0] = (chain_row_t){3, 1};
    bad += check("SONG sources use the same step bounds", chain_prepare() == 0 &&
                  !memcmp(&chain.source[3].step[0][0], &trk[0].step[0], sizeof(step_t)));
    seq_stop();
    transport_req = 0;
    memset(&r, 0, sizeof r);
    r.used = UP_USED;
    r.ver = UP_VER;
    r.np = P_COUNT;
    memcpy(r.name, "First", 6);
    erase_error = 1;
    bank = up_bank[1];
    ok = up_put(17, &r) == 2;
    erase_error = 0;
    bad += check("failed first preset save restores empty bank", ok && !memcmp(&up_bank[1], &bank, sizeof bank));
    bad += check("preset initial save", up_put(3, &r) == 0 && up_used(3));
    bank = up_bank[0];
    before = up_gen;
    r.p[0] = 77;
    fail_after = 1;
    ok = up_put(3, &r) == 2;
    fail_after = -1;
    bad += check("failed preset save rolls back bank and generation", ok && up_gen == before &&
                  !memcmp(&up_bank[0], &bank, sizeof bank));
    favorite_set(NENGINES, 3, 1);
    fail_after = 1;
    ok = up_put(3, 0) == 2;
    fail_after = -1;
    bad += check("failed preset erasure retains both the sound and its favorite", ok && up_used(3) &&
                  favorite_has(NENGINES, 3) && !memcmp(&up_bank[0], &bank, sizeof bank));
    memset(&up_bank[0], 0, sizeof up_bank[0]);
    up_boot();
    bad += check("failed preset save keeps old flash", !memcmp(&up_bank[0], &bank, sizeof bank));
    trk[0].user = 4;
    erase_error = 1;
    ok = up_put(3, 0) == 2;
    erase_error = 0;
    bad += check("failed preset erase keeps record and loaded label", ok && trk[0].user == 4u && up_gen == before &&
                  !memcmp(&up_bank[0], &bank, sizeof bank));
    bad += check("successful preset erase clears loaded label", up_put(3, 0) == 0 && !up_used(3) && !trk[0].user);
    before = erases;
    transport_req = 1;
    bad += check("pending PLAY: preset save blocked", up_put(3, &r) == 2 && erases == before && !up_used(3));
    transport_req = 0;
    chain.armed = 1;
    bad += check("pending SONG: preset save blocked", up_put(3, &r) == 2 && erases == before && !up_used(3));
    chain.armed = 0;
    bad += check("preset invalid slot blocked before access", up_put(UP_SLOTS, &r) == 1 && erases == before);
    song.playing = 0; transport_req = 1; irq_start_race = 1;
    bad += check("PLAY consumed before the save snapshot still blocks a project write",
                  project_save(1) != 0 && irq_races == 1u && song.playing && !transport_req && erases == before);
    song.playing = 0; transport_req = 1; irq_start_race = 1;
    bad += check("PLAY consumed before the preset snapshot still blocks a bank write",
                  up_put(3, &r) == 2 && irq_races == 2u && song.playing && !transport_req && erases == before);
    song.playing = 0; transport_req = 1; irq_start_race = 1;
    settings.palette = 4;
    settings_save();
    bad += check("PLAY consumed before the settings snapshot defers the write",
                  irq_races == 3u && song.playing && !transport_req && persist_pending && erases == before);
    bad += mig_test();
    printf("%s\n", bad ? "PERSISTENCE TEST FAILED" : "persistence test passed");
    return bad != 0;
}
