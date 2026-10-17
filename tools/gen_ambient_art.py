#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# GHOULBOX ambience videos (tests/ambient_render.c): one small pixel-art icon per theme, drawn from a pixel map
# (mirrored about its middle column, then the few pixels that break the symmetry), shown at whole-number scale in a
# tile beside the FM-1's own screen, in the lesson videos' look (tools/make_lesson_video.py).
#   gen_ambient_art.py THEME OUT.png [SCREEN.rgb]   THEME 1..5; SCREEN: a 240x240 RGB24 frame (ambient_render's)
#   gen_ambient_art.py THEME OUT.png --thumb        its YouTube thumbnail (1280x720), as the lessons' (the emulator's
#                                                    wall and torch from build/emu/art: web/emu/build.sh)
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[1]
NAMES = ["Dungeon", "Castle in the Sky", "Underwater Temple", "Town Square", "Conquest"]
THUMB_LINES = [["Dungeon"], ["Castle in", "the Sky"], ["Underwater", "Temple"], ["Town", "Square"], ["Conquest"]]
BG, BODY, EDGE, INK, DIM, EMBER = "#0d0b08", "#26211b", "#3d352b", "#e4d8bf", "#8a7d68", "#ee8a3c"   # the lessons'
SCALE = 14
TILE, GAP, TILE_Y = 524, 72, 120                        # the icon's tile; the screen's bezel the same size
TILE_X = (1920 - 2 * TILE - GAP) // 2
SCREEN_AT = (TILE_X + TILE + GAP + 22, TILE_Y + 22)       # the screen's 480 x 480 (make_ambient_video.py)

PAL = {
    "K": (18, 14, 11),                                   # outline
    "Y": (255, 232, 150), "O": (243, 154, 60), "R": (200, 70, 42),            # flame
    "I": (138, 138, 148), "i": (85, 85, 94),             # iron
    "W": (138, 90, 52), "w": (94, 60, 34),               # wood
    "P": (122, 66, 104), "T": (176, 168, 200), "t": (128, 120, 156), "X": (255, 216, 140),   # castle
    "U": (244, 238, 240), "u": (196, 184, 200),          # cloud
    "S": (176, 192, 186), "s": (124, 142, 138), "d": (74, 92, 92),            # stone
    "L": (76, 160, 80), "l": (44, 108, 60),              # weed
    "B": (76, 140, 208), "C": (168, 220, 244),           # water
    "G": (240, 192, 64), "g": (176, 124, 32), "H": (255, 240, 160),           # gold
    "E": (210, 60, 60), "F": (70, 110, 220),             # gems
}

# half maps: each row is the left half and the middle column; the right half mirrors it
TORCH = ["...........K", "..........KY", "..........KY", ".........KYY", ".........KYY", "........KOYY",
         "........KOYY", ".......KOOYY", ".......KROOY", ".......KROOO", "........KROO", "........KRRO",
         ".......KKKKK", ".......KIIII", ".......KiIII", "........KiII", ".........KWW", ".........KWW",
         ".......Kiiii", ".........KWW", ".........KWW", ".........KWW", "..........Kw", "...........K"]
CASTLE = ["...........K", "..........KP", "..........KP", ".........KPP", ".........KPP", "........KPPP",
          "........KKKK", "........KTTT", "........KTTX", ".....K..KTTX", "....KPK.KTTT", "....KPK.KTTT",
          "...KPPPKKTTT", "...KKKKKKTTT", "...KTTTKKTTT", "...KTXTKTTTT", "...KtTTKTTTT", "...KtTTTTTKK",
          "...KtTTTTKKK", ".KKKtTTTTKKK", "KUUKKKKKKKKK", "KUUUUUUUUUUU", "KuUUUUUUUUUU", ".Kuuuuuuuuuu",
          "..KKKKKKKKKK"]
COLUMN = ["............", "....KKKKKKKK", "....KSSSSSSS", "....KKKKKKKK", ".....KsSSSSS", "......KKKKKK",
          "......KSsSsS", "......KSsSsS", "......KSsSsS", "......KSsSsS", "......KSsSsS", "......KSsSsS",
          "......KSsSsS", "......KSsSsS", "......KSsSsS", "......KSsSsS", "......KSsSsS", ".....KKKKKKK",
          "....KSSSSSSS", "...KKKKKKKKK", "..Kddddddddd", "..KKKKKKKKKK"]
FOUNTAIN = [".........C.C", "........C..C", ".......C...C", "......C....C", ".....C....KI", "....C.KKKKKK",
            "....C.KBBBBB", ".......KSSSS", "....C....KSS", "....C....KSS", ".........KSS", "..KKKKKKKKKK",
            "..KBBBBBBBBB", ".KSSSSSSSSSS", ".KSSSSSSSSSS", ".KSSSSSSSSSS", "..KKKKKKKKKK"]
CROWN = ["...K.......K", "..KHK.....KH", "..KGK.....KG", "..KGGK...KGG", "..KGGGK.KGGG", "..KGGGGKGGGG",
         "..KGGGGGGGGG", "..KKKKKKKKKK", "..KGGGGGGGEE", "..KGGFGGGGEE", "..KGGGGGGGEE", "..KKKKKKKKKK",
         "..Kggggggggg", "..KKKKKKKKKK"]
# (x, y, colour): the pixels that are not mirrored
EXTRA = {
    1: [],
    2: [],
    3: [(3, 19, "L"), (2, 18, "L"), (3, 17, "l"), (4, 16, "L"), (3, 15, "L"), (2, 14, "l"),   # weed
        (19, 19, "L"), (20, 18, "l"), (19, 17, "L"),
        (19, 3, "C"), (18, 4, "C"), (20, 4, "C"), (19, 5, "C"), (21, 0, "C"), (17, 7, "C")],   # bubbles
    4: [],
    5: [(3, 3, "H"), (3, 4, "H"), (3, 5, "H"), (4, 5, "H"), (4, 8, "H"), (4, 9, "H")],     # the gold's shine
}
ICONS = [TORCH, CASTLE, COLUMN, FOUNTAIN, CROWN]


def icon(n):
    rows = [r + r[:-1][::-1] for r in ICONS[n - 1]]
    w = len(rows[0])
    assert all(len(r) == w for r in rows), n
    im = Image.new("RGBA", (w, len(rows)), (0, 0, 0, 0))
    px = im.load()
    for y, r in enumerate(rows):
        for x, c in enumerate(r):
            if c != ".":
                px[x, y] = PAL[c] + (255,)
    for x, y, c in EXTRA[n]:
        px[x, y] = PAL[c] + (255,)
    return im


def compose(n, screen):
    """the 1920x1080 frame, as the lesson videos: the icon in a tile and the FM-1's screen at 2x in its bezel, side
    by side in the middle; the kicker, the name and a line below"""
    out = Image.new("RGB", (1920, 1080), BG)
    d = ImageDraw.Draw(out)
    x0, y0, tile = TILE_X, TILE_Y, TILE
    d.rounded_rectangle((x0, y0, x0 + tile, y0 + tile), 20, fill=BODY, outline=EDGE, width=3)
    ic = icon(n)
    ic = ic.resize((ic.width * SCALE, ic.height * SCALE), Image.NEAREST)
    out.paste(ic, (x0 + (tile - ic.width) // 2, y0 + (tile - ic.height) // 2), ic)
    sx, sy = SCREEN_AT
    d.rounded_rectangle((sx - 22, sy - 22, sx + 502, sy + 502), 20, fill="#050403", outline=EDGE, width=3)
    if screen is not None:
        out.paste(screen.resize((480, 480), Image.NEAREST), (sx, sy))
    vt = str(ROOT / "assets/fonts/VT323-Regular.ttf")
    d.text((960, 726), "GHOULBOX DREAM  ·  AMBIENT", font=ImageFont.truetype(vt, 34), fill=DIM, anchor="mt")
    d.text((960, 768), NAMES[n - 1], font=ImageFont.truetype(str(ROOT / "assets/fonts/PirataOne-Regular.ttf"), 92),
           fill=EMBER, anchor="mt")
    d.text((960, 892), "Composed by GHOULBOX DREAM on the FM-1's own firmware", font=ImageFont.truetype(vt, 50), fill=INK,
           anchor="mt")
    return out


def thumb(n):
    """the YouTube thumbnail, as the lessons': the wall in the torch's glow, the icon in a dark tile on the left, the
    kicker, the name in two lines, GHOULBOX and a torch"""
    w, h = 1280, 720
    a = ROOT / "build/emu/art"
    wall = Image.open(a / "wall.png").convert("RGB")
    im = Image.new("RGB", (w, h))
    for y in range(0, h, wall.height):
        for x in range(0, w, wall.width):
            im.paste(wall, (x, y))
    g = Image.new("L", (w, h), 0)
    ImageDraw.Draw(g).ellipse((700, -100, 1500, 760), fill=255)
    g = g.filter(ImageFilter.GaussianBlur(170))
    im = Image.composite(Image.new("RGB", (w, h), (8, 6, 4)), im, Image.eval(g, lambda v: 200 - v * 80 // 255))
    im = Image.composite(Image.blend(im, Image.new("RGB", (w, h), (255, 140, 50)), 0.18), im, g)
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((48, 100, 568, 620), 36, fill="#0a0907", outline="#2e2820", width=6)
    ic = icon(n)
    k = min(440 // ic.width, 440 // ic.height)
    ic = ic.resize((ic.width * k, ic.height * k), Image.NEAREST)
    im.paste(ic, (308 - ic.width // 2, 360 - ic.height // 2), ic)

    def inter(sz):
        fo = ImageFont.truetype(str(ROOT / "assets/fonts/InterTight[wght].ttf"), sz)
        fo.set_variation_by_axes([800])
        return fo

    def text(xy, s, fo, fill, sh):
        d.text((xy[0] + sh, xy[1] + sh), s, font=fo, fill="#000000")
        d.text(xy, s, font=fo, fill=fill)

    pir = ImageFont.truetype(str(ROOT / "assets/fonts/PirataOne-Regular.ttf"), 150)
    lines = THUMB_LINES[n - 1]
    for s in lines:
        assert d.textlength(s, font=pir) < w - 608 - 30, s
    text((612, 92), "90 MIN AMBIENT", inter(54), "#ee8a3c", 4)
    for i, s in enumerate(lines):
        text((608, 160 + 160 * i + (80 if len(lines) == 1 else 0)), s, pir, "#efe4cc", 7)
    text((612, 570), "GHOULBOX", inter(34), "#ee8a3c", 3)
    text((612, 610), "DUNGEON SYNTH ON THE FM-1", inter(34), "#efe4cc", 3)
    t = Image.open(a / "torch.png").convert("RGBA").crop((0, 0, 12, 30)).resize((60, 150), Image.NEAREST)
    im.paste(t, (w - 40 - 60, h - 30 - 150), t)
    return im


def main():
    n = int(sys.argv[1])
    if "--thumb" in sys.argv:
        thumb(n).save(sys.argv[2])
        return
    screen = None
    if len(sys.argv) > 3:
        screen = Image.frombytes("RGB", (240, 240), Path(sys.argv[3]).read_bytes()[-240 * 240 * 3:])
    compose(n, screen).save(sys.argv[2])


if __name__ == "__main__":
    main()
