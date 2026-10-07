/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* MENU's settings for the editor (EDITOR_PROTOCOL.md "MENU settings"): MENU_DESC describes one, MENU_SET sets one;
 * INFO 4E 01 count. The editor builds its settings from these; the values, names and the apply path are the menu's
 * own (menu_items.c), so the two cannot differ. CALIBRATION and ABOUT are not offered (no value). 1.0.5: each reply
 * ends with the row's MENU tab (MI_TAB: its index and name), so the editor can group the settings as the device does.
 * ED_MENU: {row, id} in the menu's order. An id keeps its meaning for good: a new setting takes the next free id
 * (append-only), wherever its row goes; ids 0..126 (127: none). */
enum { ED_MENU_DESC = 72, ED_MENU_SET };
enum { EDM_ENUM, EDM_INT };                            /* kind: names follow; or a unit (none yet) */
static const uint8_t ED_MENU[][2] = {
    {MI_COLOR, 0}, {MI_STYLE, 1}, {MI_LARGE, 2}, {MI_ANIM, 3}, {MI_LEDS, 4}, {MI_HOLD, 5}, {MI_ACCEL, 6},
    {MI_LATCH, 7}, {MI_BPMLOCK, 8}, {MI_LOWCUT, 9}, {MI_USB, 10}, {MI_SERIAL, 11},
};
#define ED_MENU_N NELEM(ED_MENU)

/* rc as UI_SET: 0 applied and saved, 3 applied in RAM only (no flash, a failed write), 4 queued until STOP */
static uint32_t ed_menu_save(void)
{
    settings_save();
#if FELUCCA_FLASH
    if (!flash_ok || persist_pending == 2u) return 3;
    if (persist_pending == 1u) return 4;
    {
        persist_t p = persist_saved;
        settings_export(&p);
        return memcmp(&p, &persist_saved, sizeof p) ? 3u : 0u;
    }
#else
    return 3;
#endif
}

static int ed_menu_handle(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    uint32_t i, row, k;
    int32_t v;
    if (cmd == ED_MENU_DESC) {                         /* index -> index, id, kind, value, min, max (v14), name, names,
                                                          * (1.0.5) its tab: index, name */
        ed_b(a[0]);
        if (a[0] >= ED_MENU_N) {
            ed_b(127);                                 /* no such item: the list ends */
            return 1;
        }
        row = ED_MENU[a[0]][0];
        k = menu_n(row);
        ed_b(ED_MENU[a[0]][1]);
        ed_b(EDM_ENUM);
        ed_v((int32_t)menu_get(row));
        ed_v(0);
        ed_v((int32_t)k - 1);
        ed_str(MI_NAME[row], 12);
        for (i = 0; i < k; i++)
            ed_str(menu_vname(row, i), 12);
        ed_b(MI_TAB[row]);                             /* (after the names: an older editor stops before it) */
        ed_str(MTAB_NAME[MI_TAB[row]], 12);
        return 1;
    }
    if (cmd != ED_MENU_SET)
        return 0;
    (void)n;                                           /* id, value v14 -> rc, id, value v14 (the device's, clamped) */
    v = ed_rv(a + 1);
    for (i = 0; i < ED_MENU_N && ED_MENU[i][1] != a[0]; i++)
        ;
    if (i == ED_MENU_N) {
        ed_b(1); ed_b(a[0]); ed_v(v);
        return 1;
    }
    row = ED_MENU[i][0];
    menu_put(row, (uint32_t)clamp(v, 0, (int32_t)menu_n(row) - 1));
    if (row == MI_SERIAL)
        menu_serial_at = fm1_ms | 1u;                  /* (applied 200 ms after this reply: usb_serial_apply) */
    ui.force = 1;                                      /* (the MENU page, if shown, redraws) */
    ed_b(ed_menu_save());
    ed_b(a[0]);
    ed_v((int32_t)menu_get(row));
    return 1;
}
