# GHOULBOX RPG UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give GHOULBOX's screen an old-RPG look — crisp VT323 type, stone windows, a sword cursor, flickering wall torches — on every screen, without changing sound, data or behaviour.

**Architecture:** All drawing goes through `firmware/src/gfx.c` primitives and three font faces generated at build time. We (1) swap the faces for crisp VT323, (2) add `cv_stone` / `cv_window` / `cv_sprite` primitives, (3) switch the large frames to windows, (4) replace list / menu selection bars with a sword, (5) add torches to the stage on pages that have room. The host renderer's layout lint (`tests/ui_render.c`) is the gate: every window registers its inner area as a lint *cell*, so any text touching a border is reported.

**Tech Stack:** C (JieLi pi32v2, built on the host with gcc for tests), Python 3 + Pillow/fontTools build generators, host test suite `tests/run_tests.sh`.

**Spec:** `docs/superpowers/specs/2026-10-06-ghoulbox-rpg-ui-design.md`

## Global Constraints

- Fonts: faces S = VT323 16 px, M = VT323 20 px, L = VT323 32 px, rendered crisp (alpha only 0 or 15), one phase.
- Colours only from palette tokens (`ux.*`); all 9 palettes work; MONO stays pure grey.
- No change to audio code, stored formats, the editor protocol or the web pages.
- `GHOULBOX_VERSION` = "0.3" in `firmware/src/core.h` for this build.
- Draw cost (ui_render's "full redraw us") at most 1.5× today's: home 77.2, presets 66.0, menu 64.8, mixer 82.2, about_credits 116.5.
- Image must fit flash (today 492232 B of ~581 KB; the font swap frees ~30 KB).
- Every commit ends with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Measured facts this plan relies on (2026-10-06 spike, reverted)

- VT323 faces S/M/L cost 10066 B (Inter Tight: ~42 KB).
- With VT323 in place, the layout lint's only real finding was the FX layer's effect cell: the 24 px icon overlaps the 1-pixel-taller name ('1/8'), on 4 screens × 9 palettes. Fix in Task 1.
- Header (24 px) and footer (38 px) are too short for a 3 px border plus their text rows; they use a 1 px border (`border = 1`). The stage, menus, ABOUT and dialogs use the full 3 px border.
- Keycaps (`cv_keycap`) are pre-rendered pill bitmaps (`gen_aa_keycaps.py`); reshaping them is out of scope here (deviation from the spec's "plates" for keycaps; the dialog's two button backgrounds do become square plates).

## Review Focus

1. **A text row inside a window touching its border** on some page reached only by an unusual state (a long user-preset name, a message in the header, the GRID footer): expect the lint to report it — the cells in Task 3 cover it on every rendered screen; Task 3 Step 6 adds a render of the header with a message.
2. **Torch flame redraw on a page without torches** (switching from HOME to PRESETS mid-animation): expect no torch pixels drawn over a list — Task 5's test checks `ui.torches == 0` stops the tick redraw.
3. **Stone pattern seams** where one window is drawn as two canvases (the menu's two passes): expect a continuous texture — Task 2's test draws a window in two halves and compares with one whole draw.
4. **MONO palette** after new tokens: expect grey — Task 2's test extends the MONO grey assertion to the new tokens.
5. **Light palette (PAPER)**: the stone must stay readable under dark text — Task 2's contrast assertions cover TEXT/THEME on STONE for every palette.

---

### Task 1: Crisp VT323 type

**Files:**
- Add: `assets/fonts/VT323-Regular.ttf`, `LICENSES/OFL-VT323.txt` (from `https://github.com/google/fonts/raw/main/ofl/vt323/VT323-Regular.ttf` and `.../ofl/vt323/OFL.txt`)
- Modify: `tools/aa_raster.py` (`raster_font`), `tools/gen_aa_font.py` (preset, `emit`, `main`), `tools/build.py:99` (preset name), `firmware/src/ui_layer.c:401` (icon row), `LICENSING.md` (a row)
- Test: `tests/theme_test.c`

**Interfaces:**
- Produces: `raster_font(..., crisp=False)`; `gen_aa_font.py --preset ghoulbox`; generated `AF_S`, `AF_M`, `AF_L` unchanged in shape (same names and types).

- [ ] **Step 1: Write the failing test** — in `tests/theme_test.c`, at the end of `main()` before its final `printf`, add:

```c
    {   /* GHOULBOX: the UI faces are crisp (VT323 drawn without smoothing): S's alpha is only 0 or 15 */
        unsigned bad_alpha = 0;
        for (unsigned i = 0; i < sizeof AF_S_DATA; i++) {
            unsigned hi = AF_S_DATA[i] >> 4, lo = AF_S_DATA[i] & 15u;
            bad_alpha += (hi != 0u && hi != 15u) + (lo != 0u && lo != 15u);
        }
        printf("faces: S alpha values other than 0 / 15: %u\n", bad_alpha);
        assert(bad_alpha == 0u);
    }
```

- [ ] **Step 2: Run it to verify it fails**

Run: `./build.sh >/dev/null && gcc -O1 -w -Ibuild/gen -Ifirmware/src -o build/host/theme_test tests/theme_test.c -lm && build/host/theme_test | tail -3`
Expected: assertion `bad_alpha == 0u` fails (Inter Tight is anti-aliased).

- [ ] **Step 3: Add the crisp option to the rasteriser** — `tools/aa_raster.py`, `raster_font`: add the parameter and threshold each phase image:

```python
def raster_font(spec, px, chars, tracking=0.0, gamma=1.0, tabular=True, kern_min=1, phases=1, crisp=False):
```
and in the phase loop, replace
```python
            nib = None if ch == " " else _phase(font_hi, ch, cw, chh, (pad + shift + p / phases) * SS, base, f, gamma)
```
with
```python
            nib = None if ch == " " else _phase(font_hi, ch, cw, chh, (pad + shift + p / phases) * SS, base, f, gamma)
            if nib is not None and crisp:              # a pixel font: whole pixels only (no smoothing)
                nib = nib.point(lambda v: 15 if v >= 8 else 0)
```
Add to the docstring: `crisp: alpha thresholded to 0 / 15 (a pixel font at its native size).`

- [ ] **Step 4: Add the preset** — `tools/gen_aa_font.py`:

In `preset()`, before `if name == "inter-tight":`
```python
    if name == "ghoulbox":                     # GHOULBOX: VT323 (SIL OFL 1.1), crisp, one phase
        v = str(FONTS.parent / "assets" / "fonts" / "VT323-Regular.ttf")
        return [("S", v, 16, (32, 126), EXTRAS, 0.0, 1),
                ("M", v, 20, (32, 126), EXTRAS, 0.0, 1),
                ("L", v, 32, (32, 32), [ord(c) for c in L_CHARS[1:]], 0.0, 1)]
```
`emit(face, tracking, gamma, kern_min)` → `emit(face, tracking, gamma, kern_min, crisp=False)`, passing `crisp=crisp` to `ar.raster_font(...)`.
In `main()`: `choices=["standin", "inter-tight", "ghoulbox"]`; after `faces = ...`: `crisp = a.preset == "ghoulbox"`; call `emit(f, a.tracking, a.gamma, a.kern_min, crisp)`; and make the header comment name the font:
```python
    credit = ("VT323 by Peter Hull, SIL OFL 1.1; see LICENSES/OFL-VT323.txt" if a.preset == "ghoulbox" else
              "Inter Tight, SIL OFL 1.1, Copyright 2022 The Inter Project Authors; see assets/fonts/OFL.txt")
    out = [f"/* generated by tools/gen_aa_font.py: 4-bit alpha glyphs ({credit}) */",
           "#pragma once", "#include <stdint.h>", ""]
```
Update the module docstring's usage line to list `ghoulbox` and add a line: `ghoulbox    assets/fonts/VT323-Regular.ttf (SIL OFL 1.1) at 16 / 20 / 32 px, crisp (GHOULBOX).`

- [ ] **Step 5: Use it in the build** — `tools/build.py`: `"--preset", "inter-tight"` → `"--preset", "ghoulbox"`.

- [ ] **Step 6: Fix the FX cell's icon** — `firmware/src/ui_layer.c` in `lcell`:
```c
        cv_icon_on(x + (lc_w - 24) / 2, y + h - 24, 24, icon, ink, fill);   /* (its ink: rows 3 .. 20 of 24; GHOULBOX:
                                                                             * 1 px lower, under VT323's taller name) */
```

- [ ] **Step 7: Licence row** — `LICENSING.md`, in the third-party table before the Pirata One row:
```
| VT323 font by Peter Hull (GHOULBOX): the UI text, rasterised into the firmware at build time (`tools/gen_aa_font.py --preset ghoulbox`; the generated tables are not offered as a font) | SIL OFL 1.1 | `assets/fonts/VT323-Regular.ttf`, `LICENSES/OFL-VT323.txt` |
```
and append to the Inter Tight row's first cell: `(GHOULBOX: in the tree, not used by the build)`.

- [ ] **Step 8: Verify** — `./build.sh` then `AC79_SDK=~/fw-AC79_AIoT_SDK sh tests/run_tests.sh`.
Expected: `faces: S alpha values other than 0 / 15: 0`; `ui_render: ... 4 lint findings` (only the self-test's 4) ; `ALL HOST TESTS PASSED`. If `ui_test` checks every L string is covered and fails, add the missing letters to `L_CHARS` in `tools/gen_aa_font.py`.

- [ ] **Step 9: Commit**
```bash
git add assets/fonts/VT323-Regular.ttf LICENSES/OFL-VT323.txt LICENSING.md tools/aa_raster.py tools/gen_aa_font.py tools/build.py firmware/src/ui_layer.c tests/theme_test.c
git commit -m "GHOULBOX UI: crisp VT323 faces (S 16, M 20, L 32 px)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Stone, window and sprite primitives

**Files:**
- Modify: `firmware/src/gfx.c` (the `ux` struct, `palette_set`, new functions after `cv_frame`)
- Test: `tests/theme_test.c`

**Interfaces:**
- Produces (all in `gfx.c`):
  - tokens `ux.stone` (`T_STONE`, a darker speck of the stone), `ux.edge` (`T_EDGE`, a window's outer line)
  - `static void cv_stone(int32_t x, int32_t y, int32_t w, int32_t h, int32_t ox, int32_t oy)` — fills the rectangle with SURF and STONE specks; the pattern is a function of `(x + i + ox, y + j + oy)` only (caller coordinates, before `cv_oy`)
  - `static void cv_window(int32_t x, int32_t y, int32_t w, int32_t h, int32_t border)` — `border` 1 or 3; draws edge, stone, THEME line(s) and ACCENT corner studs; registers `GFX_HOOK_CELL` of its inner area
  - `static void cv_sprite(int32_t x, int32_t y, const char *const *rows, uint32_t h, const uint16_t *pal)` — `'.'` skips, `'0'..'9'` index `pal`

- [ ] **Step 1: Write the failing tests** — in `tests/theme_test.c`, inside the per-palette loop after `palette_set(p);` extend the MONO check and add contrast pairs, and after the loop add the primitive tests:

```c
        if (p == UI_MONO_INDEX)
            assert(gray(T_STONE) && gray(T_EDGE));                     /* GHOULBOX's tokens too */
        assert(contrast(T_TEXT, T_STONE) >= 7.0 && contrast(T_THEME, T_STONE) >= 3.5);   /* text on the stone */
```

```c
    {   /* GHOULBOX primitives */
        static uint16_t a[60 * 40], b[60 * 40];
        palette_set(UI_CRYPT_INDEX);
        cv_begin(60, 40, T_BG);
        cv_window(0, 0, 60, 40, 3);
        memcpy(a, cv_px, sizeof a);
        cv_begin(60, 40, T_BG);
        cv_window(0, 0, 60, 40, 3);
        assert(!memcmp(a, cv_px, sizeof a));                           /* the same every time: no flicker */
        assert(swap16(cv_px[0]) == T_ACCENT && swap16(cv_px[59]) == T_ACCENT);   /* corner studs */
        assert(swap16(cv_px[20 * 60 + 0]) == T_EDGE);                  /* the outer line */
        assert(swap16(cv_px[20 * 60 + 2]) == T_THEME);                 /* the border */
        {
            unsigned specks = 0, other = 0;
            for (unsigned y = 3; y < 37; y++)
                for (unsigned x = 3; x < 57; x++) {
                    uint16_t c = swap16(cv_px[y * 60 + x]);
                    specks += c == T_STONE;
                    other += c != T_STONE && c != T_SURF;
                }
            assert(specks > 50 && specks < 600 && !other);             /* stone: SURF with specks, nothing else */
        }
        /* drawn as two canvases (the menu's passes), the texture continues across the seam */
        cv_begin(60, 40, T_BG);
        cv_stone(0, 0, 60, 40, 0, 0);
        memcpy(a, cv_px, sizeof a);
        cv_begin(60, 20, T_BG);
        cv_stone(0, 0, 60, 20, 0, 0);
        memcpy(b, cv_px, 60 * 20 * 2);
        cv_begin(60, 20, T_BG);
        cv_stone(0, 0, 60, 20, 0, 20);
        memcpy(b + 60 * 20, cv_px, 60 * 20 * 2);
        assert(!memcmp(a, b, sizeof a));
        {   /* a sprite: '.' leaves the canvas, digits pick colours */
            static const char *const R[2] = {"0.", ".1"};
            const uint16_t pal[2] = {T_THEME, T_ACCENT};
            cv_begin(2, 2, T_BG);
            cv_sprite(0, 0, R, 2u, pal);
            assert(swap16(cv_px[0]) == T_THEME && swap16(cv_px[1]) == T_BG && swap16(cv_px[3]) == T_ACCENT);
        }
        printf("primitives: window, stone (seamless), sprite ok\n");
    }
```

- [ ] **Step 2: Run to verify they fail**

Run: `gcc -O1 -w -Ibuild/gen -Ifirmware/src -o build/host/theme_test tests/theme_test.c -lm`
Expected: compile errors: `T_STONE`, `cv_window`, `cv_stone`, `cv_sprite` undeclared.

- [ ] **Step 3: Tokens** — `firmware/src/gfx.c`: in the `ux` struct, after `lane, grid;` add a field line `uint16_t stone, edge;            /* GHOULBOX: the stone's specks, a window's outer line */` (a separate declaration after the existing `uint16_t` line, so the first 16 tokens keep their order); defines after `T_GRID`:
```c
#define T_STONE ux.stone         /* GHOULBOX: the darker specks of a window's stone (SURF -> BG 45 %) */
#define T_EDGE ux.edge           /* GHOULBOX: a window's outer line (BG -> THEME 45 %) */
```
and in `palette_set`, after `ux.grid = ...;`:
```c
    ux.stone = ux_mix(p->surf, p->bg, 45);
    ux.edge = ux_mix(p->bg, p->theme, 45);
```

- [ ] **Step 4: The primitives** — `firmware/src/gfx.c`, after `cv_frame`:

```c
/* GHOULBOX: stone, SURF with STONE specks (about 1 in 8). The pattern is a pure function of the position in the
 * caller's coordinates plus (ox, oy), so a redraw matches and a window drawn as two canvases has no seam */
static void cv_stone(int32_t x, int32_t y, int32_t w, int32_t h, int32_t ox, int32_t oy)
{
    int32_t i, j;
    uint16_t s = swap16(T_SURF), k = swap16(T_STONE);
    for (j = 0; j < h; j++) {
        int32_t py = y + j + cv_oy;
        uint32_t ry = (uint32_t)(y + j + oy) * 0x7F4Au;
        uint16_t *row;
        if (py < 0 || py >= (int32_t)cv_h)
            continue;
        row = cv_px + (uint32_t)py * cv_w;
        for (i = 0; i < w; i++) {
            int32_t px = x + i;
            uint32_t r = ((uint32_t)(x + i + ox) * 0x9E37u) ^ ry;
            if ((uint32_t)px < cv_w)
                row[px] = ((r >> 7) & 7u) ? s : k;
        }
    }
    GFX_HOOK_PIXELS((uint32_t)(w * h));
}

/* GHOULBOX: an RPG window. border 3: the EDGE line, a STONE line, a THEME line, then stone; border 1: one THEME
 * line, then stone. ACCENT studs (border x border) on the corners. Its inner area is a lint cell */
static void cv_window(int32_t x, int32_t y, int32_t w, int32_t h, int32_t border)
{
    int32_t b = border == 1 ? 1 : 3;
    cv_stone(x + b, y + b, w - 2 * b, h - 2 * b, 0, 0);
    if (b == 3) {
        cv_frame(x, y, w, h, T_EDGE);
        cv_frame(x + 1, y + 1, w - 2, h - 2, T_STONE);
        cv_frame(x + 2, y + 2, w - 4, h - 4, T_THEME);
    } else {
        cv_frame(x, y, w, h, T_THEME);
    }
    cv_rect(x, y, b, b, T_ACCENT);
    cv_rect(x + w - b, y, b, b, T_ACCENT);
    cv_rect(x, y + h - b, b, b, T_ACCENT);
    cv_rect(x + w - b, y + h - b, b, b, T_ACCENT);
    GFX_HOOK_CELL(x + b, y + b, x + w - b, y + h - b);
}

/* GHOULBOX: a small picture from rows of characters: '.' leaves the canvas, '0'..'9' are pal[0..9] */
static void cv_sprite(int32_t x, int32_t y, const char *const *rows, uint32_t h, const uint16_t *pal)
{
    uint32_t j;
    for (j = 0; j < h; j++) {
        const char *r = rows[j];
        int32_t i;
        for (i = 0; r[i]; i++)
            if (r[i] >= '0' && r[i] <= '9')
                cv_rect(x + i, y + (int32_t)j, 1, 1, pal[r[i] - '0']);
    }
}
```
If `GFX_HOOK_CELL` is not defined for the firmware build, add next to the other hook defaults at the top of `gfx.c`: `#ifndef GFX_HOOK_CELL` / `#define GFX_HOOK_CELL(x0, y0, x1, y1) ((void)0)` / `#endif`. Check `cv_frame`'s signature is `(x, y, w, h, c)` and `cv_rect` honours `cv_oy` (both do in gfx.c:170-195).

- [ ] **Step 5: Run the tests** — Run: `gcc -O1 -w -Ibuild/gen -Ifirmware/src -o build/host/theme_test tests/theme_test.c -lm && build/host/theme_test | tail -4`
Expected: `primitives: window, stone (seamless), sprite ok`, no assertion. If a palette fails `contrast(T_THEME, T_STONE) >= 3.5`, lower the STONE blend to 35 % in `palette_set` and rerun.

- [ ] **Step 6: Full suite and commit** — `./build.sh && AC79_SDK=~/fw-AC79_AIoT_SDK sh tests/run_tests.sh` → `ALL HOST TESTS PASSED` (nothing draws the primitives yet).
```bash
git add firmware/src/gfx.c tests/theme_test.c
git commit -m "GHOULBOX UI: stone, window and sprite primitives; STONE and EDGE tokens

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Windows on the screens

**Files:**
- Modify: `firmware/src/ui_draw.c` (`draw_head`, `draw_column`, `draw_foot`, `draw_confirm`), `firmware/src/ui_graph.c` (`draw_graph` panel, `draw_tracks` strips at :1113), `firmware/src/ui_layer.c:547` (the layer's panel), `firmware/src/ui_menu.c` (the menu and ABOUT pages)
- Test: `tests/ui_render.c` (renders; one new scene), visual review

**Interfaces:**
- Consumes: `cv_window(x, y, w, h, border)`, `cv_stone(...)`, `T_STONE`, `T_EDGE` from Task 2.
- Produces: every large frame is a window; the stage panel is `cv_window(3, 0, 234, H_GRAPH, 3)` with `cv_bg = T_SURF` after it (graphs keep drawing on SURF-based text ramps).

- [ ] **Step 1: The stage** — `ui_graph.c` `draw_graph`: replace `cv_rrect(3, 0, 234, H_GRAPH, 5, T_SURF, T_BG);   /* the panel */` with `cv_window(3, 0, 234, H_GRAPH, 3);   /* the panel: GHOULBOX's stone window */`. Same replacement in `ui_layer.c:547`. In `draw_tracks` (`ui_graph.c:1113`) replace `cv_rrect(0, 0, CARD_W, H_GRAPH, 5, T_SURF, T_BG);` with `cv_window(0, 0, CARD_W, H_GRAPH, 3);`.

- [ ] **Step 2: The cards** — `ui_draw.c` `draw_column`: replace `cv_rrect(0, 0, COL_W, COL_H, 4, T_SURF, T_BG);` with `cv_window(0, 0, COL_W, COL_H, 3);`. In the rolling strip branch keep `cv_begin(COL_W, ROLL_H, T_SURF)` but follow it with `cv_stone(3, 0, COL_W - 6, ROLL_H, 0, ROLL_Y);` and redraw the two side borders of the strip: `cv_rect(0, 0, 1, ROLL_H, T_EDGE); cv_rect(1, 0, 1, ROLL_H, T_STONE); cv_rect(2, 0, 1, ROLL_H, T_THEME); cv_rect(COL_W - 3, 0, 1, ROLL_H, T_THEME); cv_rect(COL_W - 2, 0, 1, ROLL_H, T_STONE); cv_rect(COL_W - 1, 0, 1, ROLL_H, T_EDGE);` (the strip is rows ROLL_Y.. of the card, so the stone uses `oy = ROLL_Y` to match the full draw). The gauge track `cv_rrect(5, 38, gw, 3, 1, T_BG, T_SURF)` stays.

- [ ] **Step 3: Header and footer** — `draw_head`: after `cv_begin(240, H_HEAD, T_BG);` add `cv_window(0, 0, 240, H_HEAD, 1);` and change every `T_BG` passed as a background/under colour inside the header drawing (the `cv_icon_mid(..., T_BG)`, `cv_free_hint(..., T_BG, ...)`, `draw_rec_mark(28, T_BG)`, `draw_battery`) to `T_SURF`; in the BPM rolling strip branch use `cv_begin(BPM_W, H_HEAD, T_SURF); cv_stone(0, 1, BPM_W, H_HEAD - 2, BPM_X, 0); cv_rect(0, 0, BPM_W, 1, T_THEME); cv_rect(0, H_HEAD - 1, BPM_W, 1, T_THEME);`. `draw_foot`: after `cv_begin(240, H_FOOT, T_BG);` add `cv_window(0, 0, 240, H_FOOT, 1);` and change the `T_BG` backgrounds in the footer calls (`cv_rrect(..., T_BG)` unders, `cv_key_row(..., T_BG)`, `cv_key_hint(..., T_BG)`, `cv_icon_on(..., T_BG)`, `cv_text_fit/_r/cv_free_text(..., T_BG, ...)`) to `T_SURF`. In `draw_frame` nothing changes (it fills the gaps between strips).

- [ ] **Step 4: Dialog** — `draw_confirm`: `cv_rrect(0, 0, DLG_W, DLG_H, 8, T_SURF, T_BG);` → `cv_window(0, 0, DLG_W, DLG_H, 3);`; the two buttons `cv_rrect(10, 80, 90, 26, 6, ...)` and `cv_rrect(108, 80, 90, 26, 6, ...)` → radius `0` (square plates).

- [ ] **Step 5: Menu** — `ui_menu.c`, the settings menu (the `for (pass = 0; pass < 2u; pass++)` loop with `cv_oy = -top;`): right after `cv_oy = -top;` add `cv_window(2, H_HEAD + 2, 236, 240 - H_HEAD - 4, 3);` (screen coordinates; `cv_stone` builds the pattern from them, so the two passes join). Remove the per-row pill `cv_rrect(4, y, 232, 24, 6, bg, T_BG);` (rows now sit on the stone; Task 4 adds the selection) and set `bg = T_SURF` for every row. The ABOUT document (`ui.menu >= 2`, drawn scrolled over two canvases) keeps its plain background in this plan: a fixed window would need its text clipped at the border while scrolling (a deviation from the spec, recorded in the commit message; its wordmark already marks it).

- [ ] **Step 6: A header-message scene for the lint** — `tests/ui_render.c`: in the scene list (`S_*` enum near line 402 and `S_NAME` near 415) add `S_HEAD_MSG` / `"head_msg"`, and in the scene setup `switch` add `case S_HEAD_MSG: ui_message("LOADED CRYPT PAD"); break;` (`ui_message` posts a header message, `firmware/src/ui.c:155`). Keep `S_NAME` in the same order as the enum.

- [ ] **Step 7: Run the lint and fix what it finds** — `./build.sh && gcc -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/ui_render tests/ui_render.c -lm && mkdir -p build/ui_new/ppm build/ui_slot && build/host/ui_render build/ui_new build/ui_slot | tail -1`.
Expected at the end: `4 lint findings` (the self-test's). For each other finding ("spills out of its cell" = text touching a border): move that call's x/y inward by the overlap (the report gives both boxes) — e.g. header battery `draw_battery(214)` → `draw_battery(212)` if it reaches column 239. Rerun until clean.

- [ ] **Step 8: Draw cost** — in `build/ui_new/report.txt` check the "full redraw us" column against the Global Constraints (≤ 1.5×). If a screen exceeds it, profile: the stone is the only new per-pixel cost; make `cv_stone` skip `GFX_HOOK_PIXELS` per row only (not per pixel) — it already does one call per fill.

- [ ] **Step 9: Visual check and commit** — `python3 tests/ui_render.py build/ui_new build/ui_slot` and look at `build/ui_new/CRYPT/{home,presets,menu,confirm_project,about}.png` (all borders visible, no text on a border). Then the full suite → `ALL HOST TESTS PASSED`.
```bash
git add firmware/src/ui_draw.c firmware/src/ui_graph.c firmware/src/ui_layer.c firmware/src/ui_menu.c tests/ui_render.c
git commit -m "GHOULBOX UI: stone windows on the header, cards, stage, footer, dialogs and menu

ABOUT keeps its plain background (it scrolls; a fixed window would need clipped text).

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Sword cursor

**Files:**
- Modify: `firmware/src/gfx.c` (the sword sprite), `firmware/src/ui_graph.c` (`list_row` :871, `graph_mod` :384, `graph_song` :1192), `firmware/src/ui_menu.c` (menu rows), `firmware/src/ui_draw.c` (`draw_column` hot card)
- Test: `tests/ui_test.c`

**Interfaces:**
- Consumes: `cv_sprite` (Task 2).
- Produces: `static int32_t cv_sword(int32_t x, int32_t y)` in `gfx.c` — draws the 14×7 sword with its top-left at (x, y), returns 14.

- [ ] **Step 1: Write the failing test** — `tests/ui_test.c`, in the main test function after the palette is set up (any place after `ui_power_on()`), add:

```c
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
```

- [ ] **Step 2: Run it** — `gcc -w -Ibuild/gen -Ifirmware/src -o build/host/ui_test tests/ui_test.c -lm && build/host/ui_test | grep "selected list row"` → `FAIL` (the THEME bar is there).

- [ ] **Step 3: The sword** — `gfx.c`, after `cv_sprite`:

```c
/* GHOULBOX: the sword cursor, 14 x 7, pointing right: pommel and grip (EDGE), guard (THEME), blade (TEXT) */
static int32_t cv_sword(int32_t x, int32_t y)
{
    static const char *const R[7] = {"....2.........",
                                     "....2.........",
                                     "....2.0000000.",
                                     "11112000000000",
                                     "....2.0000000.",
                                     "....2.........",
                                     "....2........."};
    const uint16_t pal[3] = {T_TEXT, T_EDGE, T_THEME};
    cv_sprite(x, y, R, 7u, pal);
    return 14;
}
```

- [ ] **Step 4: Lists** — `ui_graph.c` `list_row`:
```c
static void list_row(int32_t y, int sel, const char *tag, uint16_t tc, const char *name, uint16_t nc, int32_t x1)
{
    if (sel)
        cv_sword(6, y + 5);                              /* GHOULBOX: the sword, the row in the accent */
    cv_text_on(22, y + 1, &AF_S, tag, sel ? T_ACCENT : tc, T_SURF);
    cv_free_text(54, y + 1, &AF_S, name, sel ? T_ACCENT : nc, T_SURF, x1 - 54);
}
```
Update the comment above it: `/* a list row (17 px): the selected one marked by the sword (GHOULBOX), its text in the accent; tag at x 22, free text from x 54 to x1 */` and the file header line 9 (`a list's selected row is a THEME bar with INK text`) → `a list's selected row has the sword and ACCENT text (GHOULBOX)`.
`graph_mod`: delete `if (sel) cv_rrect(6, y, 228, 17, 4, T_THEME, T_SURF);`, add `if (sel) cv_sword(6, y + 5);`, set `bg = T_SURF` always, `col = sel ? T_ACCENT : on ? c : T_DIM`, the slot number at x 22 with `sel ? T_ACCENT : T_MID`.
`graph_song`: delete the `if (sel) cv_rrect(...)`, add `if (sel) cv_sword(6, y + 5);`, `bg = T_SURF`, `col = sel ? T_ACCENT : T_TEXT`, `dim = T_DIM`; the "playing" arrow icon moves from x 9 to x 22 only when the row is not selected (when selected, the sword marks it).

- [ ] **Step 5: Menu rows** — `ui_menu.c`: for the selected row `if (sel) cv_sword(8, y + 9);`, the icon from x 12 → x 26, the name from x 36 → x 46, `fg = sel ? T_ACCENT : T_TEXT`, `val = T_THEME`, icon colour `sel ? T_ACCENT : T_MID`.

- [ ] **Step 6: The hot card** — `ui_draw.c` `draw_column`: when `hot` and not `strip`, draw `cv_sword(4, 7)` instead of the icon and start the label at `lx = 20`:
```c
        if (!strip && hot && label[0])
            lx = 4 + cv_sword(4, 7) + 2;                 /* GHOULBOX: the knob just turned: the sword */
        else if (!strip && FELUCCA_ICONS && icon != ICON_NONE && label[0] && text_w(&AF_S, label) <= COL_W - 2 - 19)
            lx = 5 + cv_icon_on(5, 5, 12, icon, lc, T_SURF) + 2;
```

- [ ] **Step 7: Run tests, lint, commit** — rebuild `ui_test` → the new check `ok`; run the full suite (`ALL HOST TESTS PASSED`, ui_render `4 lint findings`). Look at `build/ui_new/CRYPT/presets.png` and `menu.png`.
```bash
git add firmware/src/gfx.c firmware/src/ui_graph.c firmware/src/ui_menu.c firmware/src/ui_draw.c tests/ui_test.c
git commit -m "GHOULBOX UI: the sword cursor in lists, menus and the turned knob's card

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Torches

**Files:**
- Modify: `firmware/src/ui_graph.c` (`PANEL_X0` / `PANEL_W` become variables; torches in `draw_graph`; `torch_tick`), `firmware/src/ui_draw.c` (`ui_draw` calls the tick), `firmware/src/ui.c` (`ui.torches` field)
- Test: `tests/ui_test.c`

**Interfaces:**
- Consumes: `cv_sprite`, `cv_stone` (Task 2).
- Produces: `static void torch_draw(int32_t x, int32_t y, uint32_t f)` (12×30, frame f 0..3), `static void torch_tick(void)`, `ui.torches` (1 while the stage shows them), `TORCH_LX 8`, `TORCH_RX 220`, `TORCH_Y 10`.

- [ ] **Step 1: Write the failing test** — `tests/ui_test.c`:
```c
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
```
(If `go_title("PRESETS")` does not reach the browse list, use the page title the browse list has in `PAGES[]`: `grep -n "GR_BROWSE" firmware/src/params.c`.)

- [ ] **Step 2: Run it** — compile `ui_test` → errors: `torch_draw`, `ui.torches` undeclared.

- [ ] **Step 3: The panel's width becomes per page** — `ui_graph.c` top:
```c
static int32_t panel_x0 = 10, panel_w = 220;         /* GHOULBOX: the graphs' inner area (narrower beside torches) */
#define PANEL_X0 panel_x0                            /* the graphs' inner area: x 10..230, or 24..216 */
#define PANEL_W panel_w
```
(replacing the two constant `#define`s). Check every use compiles (`grep -n "PANEL_X0\|PANEL_W" firmware/src/*.c`); none may be used in a constant expression (array size, `static const`); if one is, give it its own literal.

- [ ] **Step 4: The torch** — `ui_graph.c`, before `draw_graph`:
```c
/* GHOULBOX: a wall torch, 12 x 30: the flame (rows 0..11, frame f 0..3: its tip sways and the core rises), the
 * cup (12..17), the handle (18..29). Colours: ACCENT outer flame, THEME middle, TEXT core, EDGE wood */
#define TORCH_LX 8
#define TORCH_RX 220
#define TORCH_Y 10
static void torch_draw(int32_t x, int32_t y, uint32_t f)
{
    static const uint8_t OUTER[12] = {2, 2, 4, 4, 6, 6, 8, 8, 10, 10, 8, 6};
    static const uint8_t MIDW[12] = {0, 0, 0, 2, 2, 4, 4, 6, 6, 6, 6, 4};
    static const uint8_t CORE[12] = {0, 0, 0, 0, 0, 2, 2, 2, 4, 4, 2, 2};
    static const int8_t SWAY[4] = {0, 1, 0, -1};
    int32_t j;
    for (j = (int32_t)(f & 1u); j < 12; j++) {        /* (odd frames: one row shorter) */
        int32_t dx = j < 5 ? SWAY[f & 3u] : 0, cx = x + 6 + dx;
        cv_rect(cx - OUTER[j] / 2, y + j, OUTER[j], 1, T_ACCENT);
        if (MIDW[j]) cv_rect(cx - MIDW[j] / 2, y + j, MIDW[j], 1, T_THEME);
        if (CORE[j] && j + (int32_t)(f & 2u) / 2 < 12) cv_rect(cx - CORE[j] / 2, y + j, CORE[j], 1, T_TEXT);
    }
    for (j = 0; j < 6; j++)                            /* the cup: narrowing */
        cv_rect(x + j / 2, y + 12 + j, 12 - (j / 2) * 2, 1, j == 0 ? T_THEME : T_EDGE);
    cv_rect(x + 4, y + 18, 4, 12, T_EDGE);             /* the handle */
    cv_rect(x + 5, y + 18, 1, 12, T_STONE);
    GFX_HOOK_TEXT(x, y + cv_oy, x + 12, y + cv_oy + 30, "torch", 4u);   /* the lint: nothing may overlap it */
}
/* the pages whose stage has room for the torches: HOME's scope, the envelope, the LFO, FX, SLICER, the engines' own */
static int torch_page(void)
{
    const page_t *pg = cur_page();
    if (ui.home)
        return 1;
    switch (pg->graph) {
    case GR_ADSR: case GR_LFO: case GR_FX: case GR_SLCR: case GR_NONE:
        return 1;
    default:
        return 0;
    }
}
static void torch_tick(void)                          /* only the two torch rectangles, every 8 UI frames */
{
    uint32_t f = (ui.frame >> 3) & 3u;
    if (!ui.torches || f == ui.torch_f)
        return;
    ui.torch_f = (uint8_t)f;
    cv_begin(12, 30, T_SURF);
    cv_stone(0, 0, 12, 30, TORCH_LX, TORCH_Y);
    torch_draw(0, 0, f);
    cv_blit(TORCH_LX, Y_GRAPH + TORCH_Y);
    cv_begin(12, 30, T_SURF);
    cv_stone(0, 0, 12, 30, TORCH_RX, TORCH_Y);
    torch_draw(0, 0, f ^ 2u);                          /* (the other one out of step) */
    cv_blit(TORCH_RX, Y_GRAPH + TORCH_Y);
}
```

- [ ] **Step 5: Use them** — `draw_graph`: before the graph is drawn (right after `cv_bg = T_SURF;`):
```c
    ui.torches = (uint8_t)torch_page();
    panel_x0 = ui.torches ? 24 : 10;
    panel_w = ui.torches ? 192 : 220;
    if (ui.torches) {
        ui.torch_f = (uint8_t)((ui.frame >> 3) & 3u);
        torch_draw(TORCH_LX, TORCH_Y, ui.torch_f);
        torch_draw(TORCH_RX, TORCH_Y, ui.torch_f ^ 2u);
    }
```
Also at the start of `draw_graph`'s `GR_TRK` early return set `ui.torches = 0;`. `ui.c`, in the `ui` struct: `uint8_t torches, torch_f;      /* GHOULBOX: the stage shows torches; their flame frame */`. `ui_draw.c` `ui_draw()`: after the call that draws the graph (`draw_graph();`) add `torch_tick();`. The scope's trigger search `SCOPE_N - 240u` stays valid (the width only shrinks).

- [ ] **Step 6: Run tests and the lint; trim the page list** — the new `ui_test` checks `ok`. Run ui_render: any finding naming `'torch'` means that page's content needs the full width — remove its `case` from `torch_page()` and rerun until `4 lint findings`.

- [ ] **Step 7: Commit**
```bash
git add firmware/src/ui_graph.c firmware/src/ui_draw.c firmware/src/ui.c tests/ui_test.c
git commit -m "GHOULBOX UI: flickering wall torches on the stage (pages with room)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: Version 0.3, cache-proof install, review renders

**Files:**
- Modify: `firmware/src/core.h` (`GHOULBOX_VERSION "0.3"`)
- Add: `tools/ghoulbox_serve.py` (already written in the working tree: builds the site under `gb<version>` and serves it on 127.0.0.1:8000 with `Cache-Control: no-store`)
- Test: full suite; renders for the owner

- [ ] **Step 1: Version** — `firmware/src/core.h`: `#define GHOULBOX_VERSION "0.3"`.
- [ ] **Step 2: Full verification** — `./build.sh` (image size printed: must be within flash) and `AC79_SDK=~/fw-AC79_AIoT_SDK sh tests/run_tests.sh` → `ALL HOST TESTS PASSED`; draw costs within 1.5× (report.txt).
- [ ] **Step 3: Owner review before flashing** — copy `build/ui_new/CRYPT/{home,presets,menu,confirm_project,about,fx}.png` into the brainstorm companion's `screen_dir` and show them; wait for approval or changes.
- [ ] **Step 4: Commit**
```bash
git add firmware/src/core.h tools/ghoulbox_serve.py
git commit -m "GHOULBOX 0.3: version; tools/ghoulbox_serve.py (per-version package, no-store)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
- [ ] **Step 5: Install handoff** — stop the old `python3 -m http.server` on :8000, run `python3 tools/ghoulbox_serve.py` in the background, and give the owner the installer URL; after install, ABOUT must read 0.3 and the boot screen show the skull.
