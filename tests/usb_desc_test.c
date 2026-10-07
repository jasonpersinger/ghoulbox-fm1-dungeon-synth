/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of the USB descriptor layouts in src/usb.c (#67), one build per variant:
 *   -DT_CDC=0/1 (FELUCCA_CDC)  -DT_UAC=0/1 (FELUCCA_UAC)  -DT_LAYOUT=0..3 (FELUCCA_USB_LAYOUT)
 *   -DT_ON=0/1 (usb_cdc_on: the console presented or not)
 * The device and configuration descriptors come from get_desc(), as GET_DESCRIPTOR sends them, and are
 * parsed as a host does: lengths and wTotalLength, bNumInterfaces, interface numbers 0..n-1 in order, the
 * endpoints of each setting and their addresses (EP1 MIDI, EP2 / EP3 CDC, EP4 audio, each once), the IADs
 * (in front of their first interface, contiguous, the function's class, no interface in two), the AC
 * header's collection (the MIDI and audio streaming interfaces), the CDC union / call management (the data
 * interface), the device class of the layout, bcdDevice. Layout 0 and the console left out must equal the
 * 1.0 descriptors byte for byte (tests/usb_desc_v10.h). Last, the device descriptor against the IOUSBHostDevice
 * personalities of macOS 27.2's AppleUSBCDC / AppleUSBAudio / AppleUSBHostCompositeDevice (the same on the
 * #67 reporter's macOS 15): which composite drivers may take the device. And the update path, with the console
 * presented or not (MENU > USB SERIAL OFF): the soft key, the M-UPGRADE command and a SysEx frame (the installer's,
 * the editor's) arrive through EP1 OUT (ep1_take, as usb_poll hands it the packet), on the MIDI interface alone. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#define HALF_FRAMES 128
#define FELUCCA_OTA 1                                    /* (the M-UPGRADE / editor SysEx frames: the update path) */
#define FELUCCA_CDC T_CDC
#define FELUCCA_UAC T_UAC
#define FELUCCA_USB_LAYOUT T_LAYOUT
#define FELUCCA_CDC_DEFAULT T_ON
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"   /* SIE register macros (never touched here) */
#include "../firmware/src/usb.c"
#include "usb_desc_v10.h"
static uint32_t ota_now_ms(void) { return 0; }
static void ota_idle(void) {}

static int fails;
static void check(const char *what, int ok)
{
    printf("%-72s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok)
        fails++;
}

static uint32_t le16(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8; }

#define CDC_SHOWN (T_CDC && T_ON)

int main(void)
{
    const uint8_t *dev, *c;
    uint16_t dev_len, n;
    uint32_t off, i, nif = 0, niad = 0, eps_n = 0;
    int lens_ok = 1, order_ok = 1, eps_ok = 1, iad_ok = 1, dup_ok = 1, ac_ok = 0, union_ok = !CDC_SHOWN;
    int cm_ok = !CDC_SHOWN, coll_ok = 0;
    int cur_if = -1, cur_neps = 0, got = 0, iad_next = -1, iad_cls = 0, iad_sub = 0;
    uint8_t if_class[16], if_sub[16], in_iad[16], seen[16], coll[4], ncoll = 0, eps[16];
    int aud_if = -1, midi_if = -1, as_if = -1, comm_if = -1, data_if = -1;
    char name[128];

    memset(if_class, 0, sizeof if_class);
    memset(if_sub, 0, sizeof if_sub);
    memset(in_iad, 0, sizeof in_iad);
    memset(seen, 0, sizeof seen);
    printf("-- USB descriptors: CDC %d (%s), UAC %d, layout %d\n", T_CDC,
           !T_CDC ? "not built" : T_ON ? "presented" : "left out", T_UAC, T_LAYOUT);
    check("GET_DESCRIPTOR device", get_desc(0x0100, &dev, &dev_len) && dev_len == 18 && dev[0] == 18 && dev[1] == 1);
    check("GET_DESCRIPTOR configuration", get_desc(0x0200, &c, &n) && c[1] == 2);

    /* ---- the device ---- */
    if (!CDC_SHOWN)
        check("no console: bcdUSB 1.10, class 00 00 00", le16(dev + 2) == 0x0110 && !dev[4] && !dev[5] && !dev[6]);
    else if (T_LAYOUT <= 1)
        check("misc / IAD: bcdUSB 2.00, class EF 02 01", le16(dev + 2) == 0x0200 && dev[4] == 0xEF && dev[5] == 2 &&
              dev[6] == 1);
    else
        check(T_LAYOUT == 2 ? "bcdUSB 2.00, class 00 00 01" : "bcdUSB 2.00, class 00 00 00",
              le16(dev + 2) == 0x0200 && !dev[4] && !dev[5] && dev[6] == (T_LAYOUT == 2));
    check("VID 1209 PID 0001, strings 1 2, one configuration",
          le16(dev + 8) == 0x1209 && le16(dev + 10) == 1 && dev[14] == 1 && dev[15] == 2 && dev[17] == 1);
    snprintf(name, sizeof name, "bcdDevice 3.%02X", 0x10 * T_UAC + (CDC_SHOWN ? 1 + 2 * T_LAYOUT : 0));
    check(name, le16(dev + 12) == 0x300u + 0x10 * T_UAC + (CDC_SHOWN ? 1 + 2 * T_LAYOUT : 0));

    /* ---- the configuration, walked as a host does ---- */
    check("wTotalLength = the bytes sent", le16(c + 2) == n);
    for (off = 0; off < n;) {
        const uint8_t *d = c + off;
        if (d[0] < 2 || off + d[0] > n) {
            lens_ok = 0;
            break;
        }
        switch (d[1]) {
        case 0x0B:                                     /* IAD: must come right before its first interface */
            niad++;
            if (d[0] != 8 || iad_next >= 0 || d[3] < 2 || d[2] + d[3] > 16)
                iad_ok = 0;
            iad_next = d[2];
            iad_cls = d[4];
            iad_sub = d[5];
            for (i = d[2]; i < (uint32_t)d[2] + d[3] && i < 16; i++) {
                if (in_iad[i])
                    iad_ok = 0;                        /* an interface in two functions */
                in_iad[i] = (uint8_t)(niad);
            }
            if (!((d[4] == 1 && d[5] == 1 && d[6] == 0) || (d[4] == 2 && d[5] == 2 && d[6] == 1)))
                iad_ok = 0;                            /* audio 01 01 00 or CDC ACM 02 02 01 */
            break;
        case 4:
            if (cur_if >= 0 && got != cur_neps)
                eps_ok = 0;
            if (d[2] >= 16) {
                order_ok = 0;
                break;
            }
            if (iad_next >= 0) {                       /* the function's class is its first interface's */
                if (d[2] != iad_next || d[3] != 0 || d[5] != iad_cls || d[6] != iad_sub)
                    iad_ok = 0;
                iad_next = -1;
            }
            if (!seen[d[2]]) {                         /* first setting of a new interface: the next number */
                if (d[2] != nif || d[3] != 0)
                    order_ok = 0;
                seen[d[2]] = 1;
                nif++;
                if_class[d[2]] = d[5];
                if_sub[d[2]] = d[6];
            } else if (d[2] != cur_if) {
                order_ok = 0;                          /* alternate settings follow their interface */
            }
            cur_if = d[2];
            cur_neps = d[4];
            got = 0;
            if (d[5] == 1 && d[6] == 1)
                aud_if = d[2];
            if (d[5] == 1 && d[6] == 3)
                midi_if = d[2];
            if (d[5] == 1 && d[6] == 2)
                as_if = d[2];
            if (d[5] == 2 && d[6] == 2)
                comm_if = d[2];
            if (d[5] == 10)
                data_if = d[2];
            break;
        case 5:
            got++;
            if (eps_n < sizeof eps)
                eps[eps_n++] = d[2];
            for (i = 0; i + 1 < eps_n; i++)
                if (eps[i] == d[2])
                    dup_ok = 0;
            if ((d[2] == 0x01 || d[2] == 0x81) && !(d[3] == 2 && cur_if == midi_if))
                eps_ok = 0;
            if (d[2] == 0x84 && !(d[3] == 5 && cur_if == as_if))
                eps_ok = 0;
            if (d[2] == 0x82 && !(d[3] == 3 && cur_if == comm_if))
                eps_ok = 0;
            if ((d[2] == 0x03 || d[2] == 0x83) && !(d[3] == 2 && cur_if == data_if))
                eps_ok = 0;
            break;
        case 0x24:
            if (cur_if == aud_if && d[2] == 1) {       /* AC header */
                ac_ok = le16(d + 3) == 0x0100 && d[0] == 8u + d[7];
                ncoll = d[7];
                for (i = 0; i < ncoll && i < sizeof coll; i++)
                    coll[i] = d[8 + i];
            }
            if (cur_if == comm_if && d[2] == 0x06)     /* union: this interface + the data one */
                union_ok = d[0] == 5 && d[3] == comm_if && d[4] == comm_if + 1;
            if (cur_if == comm_if && d[2] == 0x01)     /* call management: the data interface */
                cm_ok = d[0] == 5 && d[4] == comm_if + 1;
            break;
        }
        off += d[0];
    }
    if (cur_if >= 0 && got != cur_neps)
        eps_ok = 0;
    coll_ok = ncoll == 1 + T_UAC && coll[0] == midi_if && (!T_UAC || coll[1] == as_if);

    check("descriptor lengths add up to wTotalLength", lens_ok && off == n);
    snprintf(name, sizeof name, "bNumInterfaces %u = the interfaces, numbered 0..n-1 in order", (unsigned)c[4]);
    check(name, order_ok && c[4] == nif && nif == 2u + T_UAC + 2u * CDC_SHOWN);
    check("each setting has bNumEndpoints endpoints, each on its interface", eps_ok);
    snprintf(name, sizeof name, "endpoint addresses unique (%u: MIDI 01 81%s%s)", (unsigned)eps_n,
             CDC_SHOWN ? ", CDC 82 03 83" : "", T_UAC ? ", audio 84" : "");
    check(name, dup_ok && eps_n == 2u + 3u * CDC_SHOWN + T_UAC);
    if (CDC_SHOWN) {
        snprintf(name, sizeof name, "IADs: audio IF %d-%d, CDC IF %d-%d, before their first interface", aud_if,
                 aud_if + 1 + T_UAC, comm_if, comm_if + 1);
        check(name, iad_ok && niad == 2 && in_iad[aud_if] && in_iad[midi_if] == in_iad[aud_if] &&
                        (!T_UAC || in_iad[as_if] == in_iad[aud_if]) && in_iad[comm_if] &&
                        in_iad[data_if] == in_iad[comm_if] && in_iad[comm_if] != in_iad[aud_if]);
        check(T_LAYOUT == 1 ? "CDC first: IF 0-1, audio from IF 2" : "audio from IF 0, CDC last",
              T_LAYOUT == 1 ? comm_if == 0 && aud_if == 2 : aud_if == 0 && comm_if == 2 + T_UAC);
    } else {
        check("no IAD, audio from IF 0", niad == 0 && aud_if == 0);
    }
    check("audio function: AC, MIDI, AS in a row", midi_if == aud_if + 1 && (!T_UAC || as_if == aud_if + 2));
    check("AC header 1.00, collection = the MIDI (and audio streaming) interfaces", ac_ok && coll_ok);
    check("UAC_AS_IF (SET_INTERFACE / GET_INTERFACE) = the audio streaming interface",
          !T_UAC || (int)UAC_AS_IF == as_if);
    check("CDC union and call management name the data interface", union_ok && cm_ok &&
          (!CDC_SHOWN || (data_if == comm_if + 1 && if_class[data_if] == 10)));

    /* ---- the released bytes ---- */
    if (CDC_SHOWN && T_LAYOUT == 0)
        check("layout 0 = Felucca 1.0 byte for byte",
              T_UAC ? !memcmp(dev, V10_CDC_DEV, 18) && n == sizeof V10_CDC_CFG && !memcmp(c, V10_CDC_CFG, n)
                    : !memcmp(dev, V10_CDC_NOUAC_DEV, 18) && n == sizeof V10_CDC_NOUAC_CFG &&
                          !memcmp(c, V10_CDC_NOUAC_CFG, n));
    if (!CDC_SHOWN)
        check("without the console = a FELUCCA_CDC=0 build of 1.0 byte for byte",
              T_UAC ? !memcmp(dev, V10_NOCDC_DEV, 18) && n == sizeof V10_NOCDC_CFG && !memcmp(c, V10_NOCDC_CFG, n)
                    : !memcmp(dev, V10_MIDI_DEV, 18) && n == sizeof V10_MIDI_CFG && !memcmp(c, V10_MIDI_CFG, n));

    /* ---- macOS: the IOUSBHostDevice personalities (macOS 27.2 kext Info.plists; -1 = "*") ---- */
    {
        static const struct { const char *drv; int cls, sub, proto; } P[] = {
            {"AppleUSBCDCCompositeDevice (CDCCompositeDevice)", 2, -1, -1},
            {"AppleUSBCDCCompositeDevice (Misc)", 0xEF, 2, 1},
            {"AppleUSBCDCCompositeDevice (Vendor)", 0, 0, 0},
            {"AppleUSBAudioComposite", 0, 0, -1},
            {"AppleUSBAudioComposite (InterfaceAssociationClass)", 0xEF, 2, 1},
            {"AppleUSBHostCompositeDevice", 0, 0, -1},
            {"AppleUSBHostCompositeDevice (InterfaceAssociationClass)", 0xEF, 2, 1},
        };
        int cdc_drv = 0;
        for (i = 0; i < sizeof P / sizeof P[0]; i++) {
            int m = P[i].cls == dev[4] && (P[i].sub < 0 || P[i].sub == dev[5]) &&
                    (P[i].proto < 0 || P[i].proto == dev[6]);
            if (m) {
                printf("   macOS may attach: %s\n", P[i].drv);
                cdc_drv |= i < 3;
            }
        }
        if (CDC_SHOWN && T_LAYOUT == 2)
            check("layout 2: no AppleUSBCDC device personality matches", !cdc_drv);
    }
    /* ---- the update path is USB-MIDI SysEx on EP1: it does not need the console ---- */
    {
        static const uint8_t KEY[8] = {0x04, 0xF0, 0x22, 0x24, 0x07, 0x35, 0x7D, 0xF7};   /* F0 22 24 35 7D F7 */
        static const uint8_t UPG[8] = {0x04, 0xF0, 0x22, 0x24, 0x07, 0x35, 0x7F, 0xF7};   /* F0 22 24 35 7F F7 */
        static const uint8_t FRM[12] = {0x04, 0xF0, 0x7D, 0x01, 0x04, 0x02, 0x03, 0x04, 0x06, 0x05, 0xF7, 0};
        const uint8_t *fp;
        uint32_t fn;
        int ok = midi_if >= 0 && !usb.uboot_req && !usb.ota_req;
        ok &= ep1_take(KEY, sizeof KEY) && usb.uboot_req;
        ok &= ep1_take(UPG, sizeof UPG) && usb.ota_req;
        ok &= ep1_take(FRM, sizeof FRM) && ota_frame_get(&fp, &fn) && fn == 6u && fp[0] == 0x7D && fp[5] == 0x05;
        snprintf(name, sizeof name, "update path (SysEx on EP1: soft key, M-UPGRADE, frames), console %s",
                 CDC_SHOWN ? "presented" : "not presented");
        check(name, ok);
    }
    printf(fails ? "USB DESCRIPTOR TEST FAILED (%d)\n" : "usb_desc: all ok\n", fails);
    return fails != 0;
}
