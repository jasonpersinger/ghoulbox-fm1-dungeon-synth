/* SPDX-License-Identifier: GPL-3.0-only */
/* The USB RESCUE (ported from zvenson/dinghy's recovery.c): this firmware's own start-up, RAM flash driver and USB
 * stack, and nothing else: no TIMER5 (its USB poll and milliseconds run here instead), no audio, no settings,
 * projects or samples read, no UI. What is left is the M-UPGRADE updater (ota.c, as in normal play) and the UBOOT
 * request, so the web installer can always put a working firmware on. Entered when two boots in a row died in their
 * first 30 s (bootguard.h), or with OCT- alone held at power-on (main.c). A reset in here: the ROM's UBOOT. Nothing is
 * written to the flash just for entering it. */
static uint32_t rescue_last, rescue_fraction, rescue_usb_last;

static void rescue_poll(void)                           /* the main loop's and ota_idle's: TIMER5's work */
{
    uint32_t now = fm1_ticks(), elapsed = now - rescue_last;
    rescue_last = now;
    fm1_ms += elapsed / (1000u * FM1_TICKS_PER_US);     /* (split before adding: safe across TIMER4's wrap) */
    rescue_fraction += elapsed % (1000u * FM1_TICKS_PER_US);
    fm1_ms += rescue_fraction / (1000u * FM1_TICKS_PER_US);
    rescue_fraction %= 1000u * FM1_TICKS_PER_US;
    fm1_wdt_feed();
    if (now - rescue_usb_last >= 500u * FM1_TICKS_PER_US) {   /* usb_poll's 2 kHz */
        rescue_usb_last = now;
        usb_poll();
    }
}

static void rescue_step(void)
{
    rescue_poll();
    usb_retry(fm1_ms);
    if (usb.uboot_req) {                                /* the host asked for UBOOT (SysEx) */
        usb_detach();
        fm1_delay_ms(30);
        bootguard_clear(&bootguard);
        fm1_enter_uboot();
        return;
    }
    ota_service();
    if (usb.ota_req) {
        usb.ota_req = 0;
        if (flash_ok)
            ota_session();                              /* the same checked updater as in normal play */
    }
}

/* OCT- alone held at power-on: 50 ms of scans (TIMER5 is not running yet), then the raw buttons */
static int rescue_key(void)
{
    uint32_t start = fm1_ticks();
    fm1_input_init();
    do {
        fm1_wdt_feed();
        fm1_input_scan();
    } while ((uint32_t)(fm1_ticks() - start) < 50u * 1000u * FM1_TICKS_PER_US);
#if BOARD_SMK37
    return (fm1_in.buttons & FM1_OCT_BOTH) == 1u << FM1_BTN_OCT_DOWN;   /* (SMK-37 Pro: OCTAVE- is button 4) */
#else
    return bootguard_manual(fm1_in.buttons);
#endif
}

static void rescue_main(void)
{
    rescue_active = 1;
    bootguard.pending = 2;                              /* a WDT reset or a crash in here: the ROM (bootguard.h) */
    rescue_last = rescue_usb_last = fm1_ticks();
    rescue_fraction = 0;
    fm1_ms = 0;
    flash_ok = FL_JEDEC_OK(FL_FAR(fl_jedec_ram)());      /* (as persist_boot: the expected 1 MiB part) */
    lcd_init();
    lcd_fill(0, 0, 240, 240, 0);                        /* fixed colours: no palette is read here */
    draw_text_line(0, 56, 240, &AF_M, "GHOULBOX", UI_CRASH_INK, 0, 1);
    draw_text_line(0, 84, 240, &AF_M, "USB RESCUE", UI_CRASH_INK, 0, 1);
    draw_text_line(0, 128, 240, &AF_S, "CONNECT USB", UI_CRASH_INK, 0, 1);
    draw_text_line(0, 148, 240, &AF_S, flash_ok ? "OPEN THE INSTALLER" : "UNKNOWN FLASH", UI_CRASH_INK, 0, 1);
    draw_text_line(0, 188, 240, &AF_S, "AUDIO OFF", UI_CRASH_INK, 0, 1);
    lcd_sync();
#if FELUCCA_CDC
    usb_cdc_switch(0);                                  /* plain USB-MIDI and audio (no console) */
#endif
    usb_start();
    for (;;)
        rescue_step();
}
