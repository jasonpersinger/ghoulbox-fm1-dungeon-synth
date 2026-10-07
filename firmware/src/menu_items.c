/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* MENU's rows and their settings: what ui_menu.c draws and steps, and what the editor reads and sets (editor_menu.c
 * MENU_DESC / MENU_SET). One apply path for both: menu_put. Grouped: the screen (COLOR, STYLE, LARGE, ANIM, LEDS), the
 * controls (HOLD: the layer threshold, KNOB ACCEL, FX LATCH, BPM LOCK), the sound (SPEAKER EQ: FLAT LOWCUT BASS+,
 * USB LEVEL), USB SERIAL, then CALIBRATION (the setup screen: HARDWARE CALIBRATION) and ABOUT, the two rows with no
 * value (MI_VALUES: the rows before them hold one). 1.0.5: in four tabs (MI_TAB). */
enum { MI_COLOR, MI_STYLE, MI_LARGE, MI_ANIM, MI_LEDS, MI_HOLD, MI_ACCEL, MI_LATCH, MI_BPMLOCK, MI_LOWCUT, MI_USB, MI_SERIAL, MI_PANEL, MI_ABOUT, MI_COUNT };
#define MI_VALUES MI_PANEL
static const char *const MI_NAME[MI_COUNT] = {"COLOR", "STYLE", "LARGE", "ANIM", "LEDS", "HOLD", "KNOB ACCEL", "FX LATCH", "BPM LOCK",
                                              "SPEAKER EQ", "USB LEVEL", "USB SERIAL", "CALIBRATION", "ABOUT"};
/* 1.0.5: the MENU's tabs (ui_menu.c: ALGORITHM steps between them, PRESETS among one tab's rows; the editor gets a
 * row's tab after its MENU_DESC reply). A tab's rows follow each other in MI order (tests/ui_test.c checks it); at
 * most MTAB_ROWS each (the page does not scroll: ui_menu.c fits them). A new row joins a tab here, a new tab is
 * appended (its index is what the editor is told) */
enum { MTAB_DISPLAY, MTAB_CONTROL, MTAB_AUDIO, MTAB_SYSTEM, MTAB_COUNT };
#define MTAB_ROWS 6u
static const char *const MTAB_NAME[MTAB_COUNT] = {"DISPLAY", "CONTROL", "AUDIO", "SYSTEM"};
static const uint8_t MI_TAB[MI_COUNT] = {
    MTAB_DISPLAY, MTAB_DISPLAY, MTAB_DISPLAY, MTAB_DISPLAY, MTAB_DISPLAY,   /* COLOR STYLE LARGE ANIM LEDS */
    MTAB_CONTROL, MTAB_CONTROL, MTAB_CONTROL, MTAB_CONTROL,                 /* HOLD KNOB ACCEL FX LATCH BPM LOCK */
    MTAB_AUDIO, MTAB_AUDIO,                                                 /* SPEAKER EQ, USB LEVEL */
    MTAB_SYSTEM, MTAB_SYSTEM, MTAB_SYSTEM,                                  /* USB SERIAL, CALIBRATION, ABOUT */
};
typedef char mtab_fits_ui[sizeof ui.menu_row >= MTAB_COUNT ? 1 : -1];   /* (ui.c: the row last picked per tab) */
static uint32_t mtab_first(uint32_t t)                 /* a tab's first row */
{
    uint32_t i = 0;
    while (i < MI_COUNT && MI_TAB[i] != t)
        i++;
    return i;
}
static uint32_t mtab_rows(uint32_t t)                  /* .. and how many it has */
{
    uint32_t i, n = 0;
    for (i = 0; i < MI_COUNT; i++)
        n += MI_TAB[i] == t;
    return n;
}
/* STYLE (ui_style, gfx.c ST_*): FLAT the filled cards; LINE black areas divided by 1 px rules (#50, #57: the 0.9 look)
 * (1.0.2: PIXEL retired, a saved PIXEL reads as LINE) */
static const char *const STYLE_N[2] = {"FLAT", "LINE"};
/* the two-valued rows: a bit of ui_prefs and its names (bit clear = the default, bit set). A step up (a knob right,
 * OCT+) = ON on the ON / OFF rows (the switch's knob to the right; ANIM, USB SERIAL: their bit clears), else the
 * second name (STYLE LINE, USB LEVEL FIXED); a step down the other (menu_step) */
typedef struct { uint8_t row, bit; const char *name[2]; } menu_flag_t;
static const menu_flag_t MENU_FLAGS[] = {
    {MI_LARGE, PREF_LARGE, {"OFF", "ON"}},            /* #15 / Discussion #80: ON, big knob labels and values (ui.c large_kind) */
    {MI_ANIM, PREF_ANIM_OFF, {"ON", "OFF"}},          /* #46: OFF, values snap (ui_draw.c roll_note, ui_graph.c pr_follow) */
    {MI_ACCEL, PREF_ACCEL, {"OFF", "ON"}},            /* #52: ui_input.c accel */
    {MI_LATCH, PREF_LATCH, {"OFF", "ON"}},
    {MI_USB, PREF_USB_FIXED, {"MASTER", "FIXED"}},     /* fx.c fx_usb_fixed: FIXED, USB at the full level */
    {MI_BPMLOCK, PREF_BPM_LOCK, {"OFF", "ON"}},        /* #58: ON, SELECT sets the tempo with GLO held only (ui_input.c) */
    {MI_SERIAL, PREF_SERIAL_OFF, {"ON", "OFF"}},       /* #67: OFF, no serial console (usb_serial_apply) */
};
/* SPEAKER EQ (settings.lowcut, fx.c fx_lowcut): an EQ on the master for the small speaker, not a speaker switch
 * (#42: "OFF" read as the speaker off). FLAT is the old OFF (0, stored as before). The built-in speaker cannot be
 * turned off from the firmware (no amp enable or mute line is known); the EQ reaches
 * the headphone / line out and USB audio too (one DAC, uac_tap reads the master) */
static const char *const SPK_EQ[3] = {"FLAT", "LOWCUT", "BASS+"};
static const char *const HOLD_N[4] = {"0.3 s", "0.4 s", "0.5 s", "0.6 s"};   /* (the editor's names; the menu draws its own) */

/* MENU > USB SERIAL (#67). The serial console is a developer tool (README: FELUCCA_CDC). ON (the default) presents it,
 * the descriptors byte for byte as before; OFF re-enumerates as audio + MIDI only, device class 0 (the bytes of a
 * FELUCCA_CDC=0 build): macOS 13-15 then attach their USB audio driver (with the console Apple's CDC composite driver
 * takes the device and the audio input never appears). USB-MIDI stays, and with it the editor, the update installer
 * and the soft key (SysEx on EP1). Applied at boot before USB starts (main.c: enumerated with it from the start, never
 * on then off) and while the menu is closed (ui_input: one re-enumeration when it closes, not one per KNOB 1 step).
 * A change from the editor (MENU_SET) waits 200 ms (menu_serial_at) so its reply leaves before the device drops off
 * the bus: the whole device re-enumerates, USB-MIDI and audio too */
static uint32_t menu_serial_at;                        /* fm1_ms | 1 of the editor's change; 0 none */
static void usb_serial_apply(void)
{
#if FELUCCA_CDC
    if (menu_serial_at && fm1_ms - menu_serial_at < 200u)
        return;
    menu_serial_at = 0;
    usb_cdc_switch(FELUCCA_CDC_DEFAULT && !(ui_prefs & PREF_SERIAL_OFF));   /* (a FELUCCA_CDC_DEFAULT=0 build: off) */
#endif
}
static const menu_flag_t *menu_flag(uint32_t row)
{
    uint32_t i;
    for (i = 0; i < NELEM(MENU_FLAGS); i++)
        if (MENU_FLAGS[i].row == row)
            return &MENU_FLAGS[i];
    return 0;
}

/* a row's value as 0..menu_n(row) - 1 in the order the menu steps it (LEDS: OFF DIM LO DIM HI INV, LEDS_MENU) */
static uint32_t menu_n(uint32_t row)
{
    return row == MI_COLOR ? NPALETTES : row == MI_LOWCUT ? 3u : row == MI_HOLD ? 4u : row == MI_LEDS ? LEDS_COUNT : 2u;
}
static uint32_t menu_get(uint32_t row)
{
    const menu_flag_t *f = menu_flag(row);
    uint32_t i = 0;
    if (f)
        return (ui_prefs & f->bit) != 0;
    switch (row) {
    case MI_COLOR: return settings.palette % NPALETTES;
    case MI_STYLE: return ui_style == ST_LINE;
    case MI_LOWCUT: return settings.lowcut % 3u;
    case MI_HOLD: return settings_hold % 4u;
    case MI_LEDS:
        while (i + 1u < LEDS_COUNT && LEDS_MENU[i] != settings_leds)
            i++;
        return i;
    }
    return 0;
}
static const char *menu_vname(uint32_t row, uint32_t v)
{
    const menu_flag_t *f = menu_flag(row);
    if (f)
        return f->name[v & 1u];
    switch (row) {
    case MI_COLOR: return UI_PALETTES[v % NPALETTES].name;
    case MI_STYLE: return STYLE_N[v & 1u];
    case MI_LOWCUT: return SPK_EQ[v % 3u];
    case MI_HOLD: return HOLD_N[v & 3u];
    case MI_LEDS: return LEDS_NAME[LEDS_MENU[v % LEDS_COUNT]];
    }
    return "";
}
/* set a row (v below menu_n(row)) and do what the menu does on a change: the palette, the EQ and USB LEVEL at once;
 * STYLE (ui_draw.c style_apply), LARGE, ANIM, LEDS, HOLD, the flags are read where they are used, every frame;
 * USB SERIAL when the menu has closed (usb_serial_apply). Saved by the caller (menu_close, the editor) */
static void menu_put(uint32_t row, uint32_t v)
{
    const menu_flag_t *f = menu_flag(row);
    if (f) {
        ui_prefs = (uint8_t)(v ? ui_prefs | f->bit : ui_prefs & ~f->bit);
        fx_usb_fixed = (ui_prefs & PREF_USB_FIXED) != 0u;   /* (at once; every frame too: ui_input) */
        return;
    }
    switch (row) {
    case MI_COLOR:
        settings.palette = v;
        palette_set(v);                                /* (the menu signature redraws) */
        ui.force = 1;
        break;
    case MI_STYLE: ui_style = (uint8_t)(v ? ST_LINE : ST_FLAT); break;   /* (drawn so from the next frame) */
    case MI_LOWCUT: settings.lowcut = v; fx_lowcut = (uint8_t)v; break;
    case MI_HOLD: settings_hold = (uint8_t)v; break;
    case MI_LEDS: settings_leds = LEDS_MENU[v]; break;
    }
}
/* the menu's step, any of KNOB 1..4 or OCT+ (s > 0) / OCT- (s < 0): the next / previous value, stopping at the ends;
 * COLOR wraps round its palettes. On an ON / OFF row up is ON (MENU_FLAGS) */
static uint32_t menu_step(uint32_t row, int32_t s)
{
    const menu_flag_t *f = menu_flag(row);
    uint32_t v = menu_get(row), n = menu_n(row);
    if (f && str_eq(f->name[0], "ON"))                 /* (ANIM, USB SERIAL: ON is their value 0) */
        s = -s;
    if (s > 0)
        return v + 1u < n ? v + 1u : row == MI_COLOR ? 0u : v;
    if (s < 0)
        return v ? v - 1u : row == MI_COLOR ? n - 1u : 0u;
    return v;
}
