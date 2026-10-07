#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""The FM-1 controls cheat sheet of the README (1200 px wide, the style of the 1.0 feature sheet: dark
rounded cards, white Inter Tight text, Fukiai line icons). On top a small, dimmed photo of the panel with
each group of controls marked by a coloured band and its number; below, in two columns, one card per
group: rows of a keycap (the pill the device's key hints use, tools/gen_aa_keycaps.py) and what the
control does. Hold actions carry the clock icon.

  gen_panel_diagram.py PHOTO.jpg OUT.svg [--lang en|ja] [--ja-font NotoSansJP.ttf] [--png OUT.png] [--jpg OUT.jpg]

PHOTO is the 1750 x 1050 photo of the panel (docs/manual/fm1_photo.jpg); every control's box (CTRL) is
measured in its pixels, and each band is its controls' boxes and a small margin. Everything else is in
one table (GROUPS), English and Japanese side by side. The text is written as outlines (Inter Tight,
assets/fonts, OFL-1.1; for --lang ja the function texts in Noto Sans JP, OFL-1.1, given with --ja-font;
icons from web/fukiai.ttf, MIT), so the SVG needs no font. --png / --jpg render it with rsvg-convert.
Checked: every text, keycap and icon inside its card (every keycap's label inside its pill), nothing
overlapping, the cards apart, the bands and their numbers on the photo and apart, each band around the
centres of all its group's controls and of no other group's.
"""
import argparse
import base64
import io
import logging
import subprocess
import sys
from pathlib import Path

from fontTools import subset
from fontTools.pens.boundsPen import BoundsPen
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer

logging.getLogger("fontTools").setLevel(logging.ERROR)   # fukiai.ttf: a harmless post-table note
ROOT = Path(__file__).resolve().parents[1]
FONT = ROOT / "assets/fonts/InterTight[wght].ttf"
ICONS = ROOT / "web/fukiai.ttf"

W = 1200
BG, CARD = "#1e1e1e", "#484848"                         # sampled from the feature sheet
KEY, INK = "#d7d7d7", "#1e1e1e"                         # keycap: mix(BG, TEXT, 78 %), its label BG (gen_ui_palettes)
R = 17                                                  # card corner radius (feature sheet)
DIM = 0.62                                              # opacity of secondary text
HOLD_ICON = "symbol_clock"

# ------------------------------------------------------------------------------------------------ the table
# A group: its number on the photo (0: none, the clock), its band colour, the column, the title, a note
# beside the title, the bands (each the controls it frames, CTRL below) and the rows. A row: the keycaps
# ("+" between two: pressed together), hold (the clock before them), the text and a second, dim line.
COLORS = {1: "#f2b866", 2: "#7ec8e3", 3: "#f28b82", 4: "#8fd694", 5: "#c9a0f0"}   # 4: PLAY's green

GROUPS = [
    dict(n=1, col=0, title=("Knobs", "ノブ"), note=None,
         bands=[("SELECT", "MASTER", "ALGORITHM", "PRESETS"), ("KNOB 1", "KNOB 2", "KNOB 3", "KNOB 4")],
         rows=[
             (("SELECT",), 0, ("Tempo (BPM)", "テンポ（BPM）"),
              ("MENU > BPM LOCK keeps it fixed", "MENU > BPM LOCK で固定")),
             (("MASTER",), 0, ("Volume", "音量"), None),
             (("ALGORITHM",), 0, ("Track T1–T4, on every page", "トラック T1–T4 を選ぶ（全ページ）"), None),
             (("PRESETS",), 0, ("The track's sound", "トラックの音色"),
              ("on HOME and SAVE > PRESETS", "HOME と SAVE > PRESETS で")),
             (("KNOB 1–4",), 0, ("The four columns of the page", "ページの 4 列を編集"), None),
         ]),
    dict(n=2, col=0, title=("Page buttons", "ページボタン"), note=("again: the next page", "もう一度で次のページ"),
         bands=[("FX", "SCL", "ENV", "LFO", "EDIT", "GLO"), ("HOME", "SAVE", "ARP", "SEQ")],
         rows=[
             (("FX",), 0, ("Effects and SLICER", "エフェクト、SLICER"), None),
             (("SCL",), 0, ("Scale, chord keys", "スケール、コードキー"), None),
             (("ENV",), 0, ("Envelope", "エンベロープ"), None),
             (("LFO",), 0, ("LFO, modulation matrix", "LFO、モジュレーション"), None),
             (("EDIT",), 0, ("Engine parameters", "エンジンのパラメーター"), None),
             (("GLO",), 0, ("Mixer, global, system", "ミキサー、全体設定"), None),
             (("HOME",), 0, ("Home screen", "ホーム画面"), None),
             (("SAVE",), 0, ("Presets, user sounds, projects", "プリセット、ユーザー音色、プロジェクト"), None),
             (("ARP",), 0, ("Arpeggiator; flashes on the beat", "アルペジエーター（拍で点滅）"), None),
             (("SEQ",), 0, ("Sequencer: steps, pattern", "シーケンサー（ステップ、パターン）"), None),
         ]),
    dict(n=3, col=0, title=("Keys", "鍵盤"), note=None,
         bands=[("KEYS",)],
         rows=[
             (("KEYS",), 0, ("Play the selected track, F3–G5", "選んだトラックを演奏（F3–G5）"),
              ("STEP page: enter notes (drum tracks: a step grid)", "STEP ページ: 音を入力（ドラムはステップグリッド）")),
         ]),
    dict(n=4, col=1, title=("Transport", "再生・録音"), note=None,
         bands=[("PLAY", "REC")],
         rows=[
             (("PLAY",), 0, ("Start / stop all four tracks", "4 トラックを再生・停止"),
              ("lit green while playing", "再生中は緑に点灯")),
             (("REC",), 0, ("Arm the track to record, on every page", "選んだトラックを録音待機（全ページ）"),
              ("stopped: starts playing too", "停止中なら再生も始まる")),
         ]),
    dict(n=5, col=1, title=("Octave", "オクターブ"), note=None,
         bands=[("OCT-", "OCT+")],
         rows=[
             (("OCT−",), 0, ("Octave down", "オクターブ下げ"),
              ("action pages, dialogs, menu: back", "アクションページ・ダイアログ・メニュー: 戻る")),
             (("OCT+",), 0, ("Octave up", "オクターブ上げ"),
              ("action pages, dialogs, menu: do it", "アクションページ・ダイアログ・メニュー: 実行")),
             (("OCT−", "OCT+"), 0, ("Octave reset", "オクターブをリセット"), None),
         ]),
    dict(n=0, col=1, title=("Hold", "長押し"), note=None, bands=[],
         rows=[
             (("FX",), 1, ("Performance effects, mutes on black keys", "演奏エフェクト、黒鍵でミュート"), None),
             (("GLO",), 1, ("Mute, solo, tap tempo; KNOB 1–4 levels", "ミュート・ソロ・タップ、ノブで音量"), None),
             (("SCL",), 1, ("A key sets the root; scale, chords", "鍵盤でルート、スケール・コード"), None),
             (("EDIT",), 1, ("A white key picks the engine", "白鍵でエンジンを選ぶ"), None),
             (("SAVE",), 1, ("Undo the last load (again: redo)", "直前の読み込みを取り消し（再度でやり直し）"), None),
             (("HOME",), 1, ("Menu", "メニュー"), None),
             (("SEQ",), 1, ("Song: chain patterns", "ソング（パターンをつなぐ）"), None),
             (("GLO", "SELECT"), 1, ("Tempo, even with BPM LOCK", "テンポ（BPM LOCK 中も）"), None),
             (("GLO", "PLAY"), 1, ("Restart from the top", "頭から再スタート"), None),
         ]),
]
TITLE = ("FM-1 controls in Felucca 1.0", "Felucca 1.0 の FM-1 操作")   # the SVG's <title> only

# Every control's box on the photo (its pixels), measured on it: the knobs are the part darker than the
# panel around them (knob and shadow, the printed names excluded), the buttons and OCT-/+ their bright caps
# between the dark gaps, the keys the bed inside its dark outline. Each contains the anchor the earlier,
# leader-line diagram used for it (panel_en.svg's knobs, OCT- and keys; the buttons on the photo).
CTRL = {
    "MASTER": (124, 117, 219, 237), "SELECT": (299, 117, 394, 240),
    "PRESETS": (114, 287, 217, 410), "ALGORITHM": (286, 286, 394, 413),
    "KNOB 1": (919, 113, 1019, 238), "KNOB 2": (1108, 115, 1203, 238),
    "KNOB 3": (1294, 112, 1389, 238), "KNOB 4": (1477, 113, 1577, 236),
    "FX": (962, 319, 1037, 394), "SCL": (1066, 318, 1141, 393), "ENV": (1170, 318, 1245, 393),
    "LFO": (1274, 318, 1349, 393), "EDIT": (1378, 317, 1452, 392), "GLO": (1482, 317, 1557, 392),
    "HOME": (963, 421, 1038, 497), "SAVE": (1068, 420, 1143, 496), "ARP": (1172, 420, 1247, 496),
    "SEQ": (1276, 420, 1351, 495), "PLAY": (1380, 419, 1455, 495), "REC": (1483, 419, 1559, 495),
    "OCT-": (154, 461, 241, 509), "OCT+": (284, 461, 371, 508),
    "KEYS": (91, 585, 1652, 951),
}
BAND_M = 8                                              # a band: its controls' boxes and this margin


def band_box(names):
    bs = [CTRL[n] for n in names]
    return (min(b[0] for b in bs) - BAND_M, min(b[1] for b in bs) - BAND_M,
            max(b[2] for b in bs) + BAND_M, max(b[3] for b in bs) + BAND_M)

# --------------------------------------------------------------------------------------------- text
class Face:
    def __init__(self, font, tag):
        self.font = font
        self.tag = tag
        self.upm = font["head"].unitsPerEm
        self.cmap = font.getBestCmap()
        self.gs = font.getGlyphSet()
        self.hmtx = font["hmtx"]
        self.ids = {}

    def glyph(self, name):
        """the id of the glyph's path in <defs> (None: an empty glyph)"""
        if name not in self.ids:
            pen = SVGPathPen(self.gs)
            self.gs[name].draw(pen)
            d = pen.getCommands()
            self.ids[name] = f"{self.tag}{len(self.ids)}" if d else None
            if d:
                defs.append(f'<path id="{self.ids[name]}" d="{d}"/>')
        return self.ids[name]

    def bounds(self, name):
        pen = BoundsPen(self.gs)
        self.gs[name].draw(pen)
        return pen.bounds                       # font units, y up; None when empty

    def gname(self, c):
        if ord(c) not in self.cmap:
            raise SystemExit(f"{self.tag}: no glyph for {c!r}")
        return self.cmap[ord(c)]

    def width(self, s, size):
        return sum(self.hmtx[self.gname(c)][0] for c in s) * size / self.upm


defs = []         # glyph outlines, once each
out = []          # svg elements
boxes = []        # (x0, y0, x1, y1, label, container): texts, keycaps, icons, badges
solids = []       # cards and the photo: (x0, y0, x1, y1, name)
marks = []        # bands and badges on the photo: (x0, y0, x1, y1, name)
desc = []         # every text, for <desc>
FACES = {}


def load_faces(ja_font, ja_text):
    for w in (450, 600):
        FACES["lat", w] = Face(instancer.instantiateVariableFont(TTFont(FONT), {"wght": w}), f"t{w}_")
    if ja_font:
        f = TTFont(ja_font)
        opt = subset.Options()
        opt.layout_features = []
        opt.notdef_outline = False
        sub = subset.Subsetter(opt)
        sub.populate(unicodes=sorted({ord(c) for c in ja_text}))
        sub.subset(f)
        for w in (450, 600):
            FACES["ja", w] = Face(instancer.instantiateVariableFont(f if w == 600 else TTFont(_sub_copy(f)),
                                                                     {"wght": w}), f"j{w}_")
    FACES["icon"] = Face(TTFont(ICONS), "i")


def _sub_copy(f):
    buf = io.BytesIO()
    f.save(buf)
    buf.seek(0)
    return buf


def ink(face, items, k, ox, oy):
    """the ink box of glyphs (name, pen_x in font units) drawn at (ox, oy) with scale k, y down"""
    xs, ys = [], []
    for name, px in items:
        b = face.bounds(name)
        if b:
            xs += [ox + (px + b[0]) * k, ox + (px + b[2]) * k]
            ys += [oy - b[3] * k, oy - b[1] * k]
    return min(xs), min(ys), max(xs), max(ys)


def text(s, x, y, size, weight=450, anchor="start", op=1.0, inside=None, fam="lat", fill=None, reg=True):
    """s at baseline y; anchor start / middle / end; inside = the box it must fit in. Returns its ink box."""
    f = FACES[fam, weight]
    w = f.width(s, size)
    x0 = x - (w / 2 if anchor == "middle" else w if anchor == "end" else 0)
    k = size / f.upm
    parts, items, pen_x = [], [], 0.0
    for c in s:
        g = f.gname(c)
        gid = f.glyph(g)
        if gid:
            parts.append(f'<use href="#{gid}" x="{pen_x:.0f}"/>')
            items.append((g, pen_x))
        pen_x += f.hmtx[g][0]
    o = f' fill-opacity="{op}"' if op < 1 else ""
    fl = f' fill="{fill}"' if fill else ""
    out.append(f'<g transform="translate({x0:.2f} {y:.2f}) scale({k:.5f} {-k:.5f})"{fl}{o}>{"".join(parts)}</g>')
    ib = ink(f, items, k, x0, y)
    if reg:
        # the line box (cap height and descenders) or the ink, whichever is larger
        boxes.append((min(x0, ib[0]), min(y - size * 0.74, ib[1]), max(x0 + w, ib[2]), max(y + size * 0.22, ib[3]),
                      s, inside))
        desc.append(s)
    return ib


def icon(name, x, y, size, op=1.0, inside=None):
    """Fukiai glyph name, its em box with the top-left at (x, y)"""
    f = FACES["icon"]
    k = size / f.upm
    asc = f.font["hhea"].ascent
    o = f' fill-opacity="{op}"' if op < 1 else ""
    out.append(f'<use href="#{f.glyph(name)}" transform="translate({x:.2f} {y + asc * k:.2f}) '
               f'scale({k:.5f} {-k:.5f})"{o}/>')
    ix0, iy0, ix1, iy1 = ink(f, [(name, 0)], k, x, y + asc * k)
    boxes.append((min(x, ix0), min(y, iy0), max(x + size, ix1), max(y + size, iy1), name, inside))


def rect(x, y, w, h, r, fill="none", stroke=None, sw=1.5, fop=1.0):
    s = f' stroke="{stroke}" stroke-width="{sw}"' if stroke else ""
    o = f' fill-opacity="{fop}"' if fop < 1 else ""
    out.append(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" rx="{r}" fill="{fill}"{o}{s}/>')
    return (x, y, x + w, y + h)


# ------------------------------------------------------------------------------------------- keycaps
CAP_H, CAP_R, CAP_SIZE, CAP_PAD = 24, 7.5, 14, 6.5     # gen_aa_keycaps' 13 / 4 / 9 / 3 px, about x 1.85
bad_caps = []


def cap_ink(label):
    f = FACES["lat", 600]
    items, pen_x = [], 0.0
    for c in label:
        g = f.gname(c)
        items.append((g, pen_x))
        pen_x += f.hmtx[g][0]
    k = CAP_SIZE / f.upm
    x0, _, x1, _ = ink(f, items, k, 0, 0)
    return x0, x1


def cap_w(label):
    x0, x1 = cap_ink(label)
    return x1 - x0 + 2 * CAP_PAD


def keycap(label, x, ymid, inside):
    """the pill CAP_PAD each side of the label's ink, the caps centred on ymid; returns its right edge"""
    f = FACES["lat", 600]
    ix0, ix1 = cap_ink(label)
    w = ix1 - ix0 + 2 * CAP_PAD
    pill = rect(x, ymid - CAP_H / 2, w, CAP_H, CAP_R, fill=KEY)
    cap = f.font["OS/2"].sCapHeight * CAP_SIZE / f.upm
    lb = text(label, x + CAP_PAD - ix0, ymid + cap / 2, CAP_SIZE, 600, fill=INK, reg=False)
    if not (pill[0] + CAP_PAD - 0.5 <= lb[0] and lb[2] <= pill[2] - CAP_PAD + 0.5 and pill[1] + 3 <= lb[1]
            and lb[3] <= pill[3] - 3):
        bad_caps.append(f"label {label!r} off its pill")
    boxes.append(pill + ("cap " + label, inside))
    desc.append(label)
    return pill[2]


def lead_w(caps, hold):
    """the width of a row's lead: the clock, the keycaps and the '+' between them"""
    w = (HOLD_SZ + 8 if hold else 0) + sum(cap_w(c) for c in caps)
    return w + (len(caps) - 1) * PLUS_W


HOLD_SZ, PLUS_W = 20, 22


# --------------------------------------------------------------------------------------------- photo
CROP = (70, 60, 1680, 980)                              # the body, the key bed's bottom edge (951) included
PW = 640
PX, PY = (W - PW) / 2, 24                               # the cards' margin M; no title above
PS = PW / (CROP[2] - CROP[0])
PH = (CROP[3] - CROP[1]) * PS
PHOTO = (PX, PY, PX + PW, PY + PH)


def at(x, y):
    return PX + (x - CROP[0]) * PS, PY + (y - CROP[1]) * PS


def photo(path):
    from PIL import Image, ImageEnhance
    im = Image.open(path).convert("RGB")
    assert im.size == (1750, 1050), im.size
    im = im.crop(CROP).resize((PW * 2, round(PH * 2)), Image.LANCZOS)
    im = ImageEnhance.Brightness(ImageEnhance.Color(im).enhance(0.5)).enhance(0.55)
    buf = io.BytesIO()
    im.save(buf, "JPEG", quality=84, optimize=True)
    data = base64.b64encode(buf.getvalue()).decode()
    defs.append(f'<clipPath id="pc"><rect x="{PX}" y="{PY}" width="{PW}" height="{PH:.2f}" rx="{R}"/></clipPath>')
    out.append(f'<image x="{PX}" y="{PY}" width="{PW}" height="{PH:.2f}" clip-path="url(#pc)" '
               f'preserveAspectRatio="none" href="data:image/jpeg;base64,{data}"/>')
    solids.append(PHOTO + ("photo",))


BADGE_R = 12


def badge(n, cx, cy, reg_list, inside=None):
    color = COLORS[n]
    out.append(f'<circle cx="{cx:.2f}" cy="{cy:.2f}" r="{BADGE_R}" fill="{color}"/>')
    b = (cx - BADGE_R, cy - BADGE_R, cx + BADGE_R, cy + BADGE_R)
    t = text(str(n), cx, cy + 5.3, 15, 600, "middle", fill=INK, reg=False)
    if not (b[0] + 3 <= t[0] and t[2] <= b[2] - 3 and b[1] + 3 <= t[1] and t[3] <= b[3] - 3):
        bad_caps.append(f"badge {n}: number off its circle")
    reg_list.append(b + (f"badge {n}", inside) if reg_list is boxes else b + (f"badge {n}",))


def bands():
    for g in GROUPS:
        for i, names in enumerate(g["bands"]):
            a, b = (at(*p) for p in zip(*[iter(band_box(names))] * 2))
            rect(a[0], a[1], b[0] - a[0], b[1] - a[1], 7, fill=COLORS[g["n"]], fop=0.13,
                 stroke=COLORS[g["n"]], sw=2)
            marks.append((a[0], a[1], b[0], b[1], f"band {g['n']}.{i}"))
    for g in GROUPS:                                 # the numbers on every band's top-left corner (on the photo)
        for i, names in enumerate(g["bands"]):
            a = at(*band_box(names)[:2])
            badge(g["n"], max(a[0] + 2, PHOTO[0] + BADGE_R + 4), max(a[1] + 2, PHOTO[1] + BADGE_R + 4), marks)


# --------------------------------------------------------------------------------------------- cards
M, GAP, CGAP = 24, 16, 14
CW = (W - 2 * M - GAP) / 2
ROW, SUB = 34, 19
HEAD = 56


def card_h(g):
    return HEAD + sum(ROW + (SUB if r[3] else 0) for r in g["rows"]) + 12


def card(g, x0, y0, L):
    x1, y1 = x0 + CW, y0 + card_h(g)
    rect(x0, y0, CW, y1 - y0, R, fill=CARD)
    box = (x0, y0, x1, y1)
    solids.append(box + (g["title"][0],))
    if g["n"]:
        badge(g["n"], x0 + 20 + BADGE_R, y0 + 28, boxes, box)
    else:
        icon(HOLD_ICON, x0 + 20, y0 + 16, 24, inside=box)
    fam = "ja" if L else "lat"
    text(g["title"][L], x0 + 20 + 2 * BADGE_R + 10, y0 + 35, 20, 600, inside=box, fam=fam)
    if g["note"]:
        text(g["note"][L], x1 - 20, y0 + 34, 14, 450, "end", op=DIM, inside=box, fam=fam)
    tx = x0 + 20 + max(lead_w(r[0], r[1]) for r in g["rows"]) + 14
    y = y0 + HEAD
    for caps, hold, main, sub in g["rows"]:
        mid = y + ROW / 2 - 4
        x = x0 + 20
        if hold:
            icon(HOLD_ICON, x, mid - HOLD_SZ / 2, HOLD_SZ, op=0.85, inside=box)
            x += HOLD_SZ + 8
        for i, c in enumerate(caps):
            if i:
                text("+", x + PLUS_W / 2, mid + 5.5, 16, 600, "middle", op=0.85, inside=box)
                x += PLUS_W
            x = keycap(c, x, mid, box)
        text(main[L], tx, mid + 5.5, 16, 450, inside=box, fam=fam)
        if sub:
            text(sub[L], tx, mid + 5.5 + SUB, 13.5, 450, op=DIM, inside=box, fam=fam)
        y += ROW + (SUB if sub else 0)
    return y1


def build(photo_path, L):
    top = PY + PH + 20
    hs = [top, top]
    for g in GROUPS:
        hs[g["col"]] += card_h(g) + CGAP
    H = round(max(hs) - CGAP + M)
    out.append(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
    photo(photo_path)
    bands()
    ys = [top, top]
    for g in GROUPS:
        c = g["col"]
        ys[c] = card(g, M + c * (CW + GAP), ys[c], L) + CGAP
    return H


# --------------------------------------------------------------------------------------------- checks
def check(H):
    bad = list(bad_caps)

    def over(a, b, e=0.5):
        return a[0] < b[2] - e and b[0] < a[2] - e and a[1] < b[3] - e and b[1] < a[3] - e

    for b in boxes:
        x0, y0, x1, y1, s, c = b
        if not (0 <= x0 and x1 <= W and 0 <= y0 and y1 <= H):
            bad.append(f"off the canvas: {s!r}")
        if c and not (c[0] + 12 <= x0 and x1 <= c[2] - 12 and c[1] + 8 <= y0 and y1 <= c[3] - 6):
            bad.append(f"outside its card: {s!r} {tuple(round(v) for v in b[:4])} in {tuple(round(v) for v in c)}")
    for i, a in enumerate(boxes):
        for b in boxes[i + 1:]:
            if over(a, b):
                bad.append(f"overlap: {a[4]!r} / {b[4]!r}")
    for i, a in enumerate(solids):
        if not (0 <= a[0] and a[2] <= W and 0 <= a[1] and a[3] <= H):
            bad.append(f"card off the canvas: {a[4]}")
        for b in solids[i + 1:]:
            if over(a, b, -6):                       # at least 6 px apart
                bad.append(f"cards too close: {a[4]} / {b[4]}")
    for i, a in enumerate(marks):
        if not (PHOTO[0] + 2 <= a[0] and a[2] <= PHOTO[2] - 2 and PHOTO[1] + 2 <= a[1] and a[3] <= PHOTO[3] - 2):
            bad.append(f"off the photo: {a[4]}")
        for b in marks[i + 1:]:
            if a[4].startswith("band") and b[4].startswith("band") and over(a, b, -3):
                bad.append(f"bands touch: {a[4]} / {b[4]}")
            if a[4].startswith("badge") and b[4].startswith("badge") and over(a, b, -2):
                bad.append(f"badges touch: {a[4]} / {b[4]}")
    for g in GROUPS:                                 # each band frames its controls and no other group's
        for i, names in enumerate(g["bands"]):
            x0, y0, x1, y1 = band_box(names)
            for h in GROUPS:
                for n in (n for nn in h["bands"] for n in nn):
                    c = CTRL[n]
                    cx, cy = (c[0] + c[2]) / 2, (c[1] + c[3]) / 2
                    inside = x0 < cx < x1 and y0 < cy < y1
                    if n in names and not inside or h is not g and inside:
                        bad.append(f"band {g['n']}.{i}: {n} {'inside' if inside else 'outside'}")
    return bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("photo")
    ap.add_argument("svg")
    ap.add_argument("--lang", choices=("en", "ja"), default="en")
    ap.add_argument("--ja-font", help="Noto Sans JP (variable, wght), for --lang ja")
    ap.add_argument("--png")
    ap.add_argument("--jpg")
    a = ap.parse_args()
    L = a.lang == "ja"
    if L and not a.ja_font:
        ap.error("--lang ja needs --ja-font")
    ja_text = ""
    if L:
        ja_text = "".join(g["title"][1] + (g["note"][1] if g["note"] else "") +
                                     "".join(r[2][1] + (r[3][1] if r[3] else "") for r in g["rows"]) for g in GROUPS)
    load_faces(a.ja_font if L else None, ja_text)
    H = build(a.photo, L)
    bad = check(H)
    if bad:
        print("\n".join(bad), file=sys.stderr)
        sys.exit(1)
    d = " / ".join(desc).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
    t = TITLE[L].replace("&", "&amp;")
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" fill="#fff">'
           f'<title>{t}</title><desc>{d}</desc>\n'
           f'<defs>{"".join(defs)}</defs>\n' + "\n".join(out) + "\n</svg>\n")
    Path(a.svg).write_text(svg, encoding="utf-8")
    print(f"{a.svg}: {W} x {H}, {len(boxes)} texts / keycaps / icons, {len(solids)} cards, {len(marks)} marks "
          f"checked, {len(svg)} bytes")
    for out_path in (a.png, a.jpg):
        if not out_path:
            continue
        png = out_path + ".tmp.png"
        subprocess.run(["rsvg-convert", "-w", str(W), "-h", str(H), "-o", png, a.svg], check=True)
        from PIL import Image
        im = Image.open(png).convert("RGB")
        if out_path == a.jpg:
            im.save(out_path, "JPEG", quality=90, optimize=True, progressive=True, subsampling=0)
        else:
            im.save(out_path, "PNG", optimize=True)
        Path(png).unlink()
        print(f"{out_path}: {Path(out_path).stat().st_size} bytes")


if __name__ == "__main__":
    main()
