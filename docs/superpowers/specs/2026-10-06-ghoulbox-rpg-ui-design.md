# GHOULBOX RPG UI — design

Date: 2026-10-06. Status: approved in conversation; this document is for review before planning.

## Intent

GHOULBOX (the dungeon-synth fork of Felucca on the M-VAVE FM-1) still looks like Felucca with new colours.
The owner wants the screen to feel **dark, whimsical and fantasy**, specifically like **old 16-bit RPG
menus**. Success: someone who knows Felucca would not mistake the GHOULBOX screen for it, every screen stays
readable on the 240×240 display, and nothing about the music or the stored data changes.

Choices made with mockups (brainstorm companion, `.superpowers/brainstorm/`):

| Question | Choice |
|---|---|
| Mood | Old RPG menus |
| Window style | "Castlevania gothic": stone-textured windows, theme-colour double border, accent corner studs |
| Font | VT323 (the "Dragon Quest" mockup's font) |
| Centre stage | Two flickering wall torches flanking it |
| Selection in lists / menus | A sword cursor; the selected row in the accent colour (blood red in CRYPT) |
| Approach | Replace Felucca's look outright (no CLASSIC/RPG switch) |

## Scope

In: the firmware's on-screen look (fonts, frames, selection, stage decoration), the version bump to 0.3,
and installing it without a stale browser cache. Out: sounds, the editor protocol, stored formats, the web
pages (already branded), new screens or features, the FX layer's behaviour.

## Design

### 1. Type
- `tools/gen_aa_font.py` gets a preset `ghoulbox`: faces S = VT323 16 px, M = VT323 20 px, L = VT323 32 px,
  the same glyph ranges as `inter-tight`. Rendered without smoothing (alpha 0 or 15), so pixels stay crisp.
- `tools/build.py` uses it in place of `inter-tight`. `assets/fonts/VT323-Regular.ttf` and
  `LICENSES/OFL-VT323.txt` are added; `LICENSING.md` gets a row. Inter Tight stays in the tree, unused
  (its licence row says so).
- Silkscreen 8 px is not used: about 5 px tall on the device, below Felucca's smallest text.
- Any label that no longer fits is found by the layout lint (section 5) and fixed at its call site
  (shorter wording, or the existing ellipsis), not by shrinking the face.

### 2. Stone windows
- New `cv_window(x, y, w, h, under)` in `gfx.c`: a fixed stone dither of SURF and a darker blend (a pure
  function of the pixel position, so redraws match and nothing flickers), a 1 px dark outer edge, a double
  border in THEME, 3×3 corner studs in ACCENT; `under` is what lies outside the frame. Colours come only
  from palette tokens, so all 9 palettes work; MONO's tokens are grey, so it stays grey (the MONO check
  in `ui_render` must keep passing).
- Used for the large frames: the header bar, the four knob cards, the stage, the footer, the menu / ABOUT
  page, dialogs and list panels. Small elements (keycap hints, chips, badges) keep `cv_rrect`, at radius 0
  ("plates").
- The ~54 `cv_rrect` call sites are reviewed one by one; only the frames above switch.

### 3. Sword cursor
- A 14×7 sprite (pommel, guard, blade) in the palette's tokens, drawn left of the selected row in lists,
  menus and dialogs; that row's text in ACCENT. Replaces the filled selection bar (the T_SEL fills) there.
- HOME: the selected ("hot") knob card shows the sword at its label instead of today's highlight.
- Rows shift right by the sword's width only where they had no left margin for it.

### 4. Torches
- Two 12×30 torch sprites (handle, cup, a three-colour flame) inside the stage's left and right edges.
- The flame cycles 4 frames at ~8 per second from the UI's frame counter; only the two torch rectangles
  redraw for the animation, not the stage.
- Pages whose stage content needs the full width (the step grid, the piano roll, the mixer, the drum grid,
  and any page the lint finds too tight) skip the torches; a per-page flag says which.

### 5. Version, install, verification
- `GHOULBOX_VERSION` becomes "0.3"; the ABOUT screen shows it, so an install can be confirmed.
- `tools/ghoulbox_serve.py` builds the installer site with a per-version package name and serves it
  with `Cache-Control: no-store` (the 0.2 install showed no skull because Chrome reused a cached package).
- Before flashing: `tests/run_tests.sh` all green, including `ui_render`'s lint (112 screens × 9 palettes:
  no clipping, no overlap, MONO grey) and its draw-cost figures not worse than 1.5× today's; the image
  within flash (≈ 88 KB headroom today); CRYPT renders of HOME, PRESETS, a menu, a dialog and ABOUT
  shown to the owner for approval.

## Risks

- **Text width.** VT323 is narrower per glyph but taller than Inter Tight at the same role; some columns may
  clip. The lint finds every case; fixes are per call site.
- **Draw cost.** The dither fill costs more per pixel than a flat fill. The UI draws in the main loop, not
  the audio interrupt, so the risk is a less smooth display, not audio. Measured by `ui_render`.
- **No way back on the device.** Approach A removes Felucca's look from GHOULBOX; reinstalling Felucca
  restores it.
