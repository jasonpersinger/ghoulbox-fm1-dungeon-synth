/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* web/emu/felucca_web.c built natively (cc, as the host tests), playing emu_test.mjs's heavy song with the same
 * gestures and the same render calls: its samples, float32 L R interleaved, to OUT. emu_test.mjs compares the
 * WebAssembly build's against them (the browser plays what the host build plays).
 *   cc -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o build/emu/native_check web/emu/native_check.c -lm
 *   build/emu/native_check OUT.f32 */
#include "felucca_web.c"

static FILE *out;
static int rec;
static void render(double ms)                     /* as emu_test.mjs: 128-frame calls, the last one shorter */
{
    uint32_t n = (uint32_t)(ms * 44.1 + 0.5), k, i;
    for (k = 0; k < n; k += 128u) {
        uint32_t m = n - k < 128u ? n - k : 128u;
        web_render(m);
        for (i = 0; rec && i < m; i++) {
            float s[2] = {out_l[i], out_r[i]};
            fwrite(s, sizeof s, 1, out);
        }
    }
}

int main(int argc, char **argv)
{
    if (argc < 2 || !(out = fopen(argv[1], "wb")))
        return 2;
    web_nor_erase();
    web_boot();
    render(500);
    web_test_heavy();
    web_buttons(1u << B_PLAY);
    render(60);
    web_buttons(0);
    render(60);
    rec = 1;
    render(2000);
    fclose(out);
    return 0;
}
