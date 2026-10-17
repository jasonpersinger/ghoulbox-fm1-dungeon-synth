#!/usr/bin/env python3
"""GHOULBOX lesson video: build/host/lesson_render N DIR/lesson.wav DIR/frames.rgb, then
  python3 tools/make_lesson_video.py DIR N "Title" ["KICKER"]  -> DIR/ghoulbox-lesson-N.mp4
(KICKER: the small line above the step, "LESSON N" when not given: "GHOULBOX 2.3" for a what's-new video)
1920x1080: an FM-1 panel drawn with the device's live screen in it, a sword pointing at the control each action
uses (from lesson.wav.acts), and below it the step and the one thing to do."""
import math, os, subprocess, sys
from PIL import Image, ImageDraw, ImageFont

D, N, TITLE = sys.argv[1], sys.argv[2], sys.argv[3]
KICK = sys.argv[4] if len(sys.argv) > 4 else f"LESSON {N}"
FONTS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "fonts")
def font(name, size): return ImageFont.truetype(os.path.join(FONTS, name), size)
VT, PIR = "VT323-Regular.ttf", "PirataOne-Regular.ttf"
BG, BODY, EDGE, INK, DIM, EMBER, BLOOD, BONE = ("#0d0b08", "#26211b", "#3d352b", "#e4d8bf", "#8a7d68", "#ee8a3c",
                                                 "#d9584a", "#cfc3a8")
W, H = 1920, 1080
SX, SY = 630, 40                                       # the screen, 2x: 480 x 480

# the controls: name -> (x, y, kind, w, h); knobs are circles of radius w
KNOBS = {"MASTER": (330, 140), "SELECT": (500, 140), "PRESETS": (330, 330), "ALGORITHM": (500, 330),
         "KNOB1": (1250, 130), "KNOB2": (1380, 130), "KNOB3": (1510, 130), "KNOB4": (1640, 130)}
ROW1, ROW2 = ["FX", "SCL", "ENV", "LFO", "EDIT", "GLO"], ["HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC"]
BUTTONS = {n: (1210 + i * 88, 300) for i, n in enumerate(ROW1)}
BUTTONS.update({n: (1210 + i * 88, 390) for i, n in enumerate(ROW2)})
BUTTONS.update({"OCT-": (345, 470), "OCT+": (485, 470)})
BW, BH = 74, 58
KR = 50

WHITES = [k for k in range(27) if (5 + k) % 12 not in (1, 3, 6, 8, 10)]   # 27 keys from F
KW = 1440 / len(WHITES)
def key_box(k):
    """a key's rectangle: white keys along the bottom row, black ones in their own row above"""
    if k in WHITES:
        x = 240 + WHITES.index(k) * KW
        return (x + 6, 650, x + KW - 6, 770)
    x = 240 + WHITES.index(k - 1) * KW + KW / 2
    return (x + 10, 560, x + KW - 10, 636)

def keys(c):
    """the keys a control names: KEY7, GLO+KEY1, or a chord KEY7+11+14"""
    return [int(k) for k in c.split("KEY")[1].split("+")]

def centre(c):
    if "KEY" in c:                                     # (a chord, KEYa+b+c: its middle key)
        x0, y0, x1, y1 = key_box(keys(c)[len(keys(c)) // 2])
        return (x0 + x1) / 2, (y0 + y1) / 2
    if c in KNOBS: return KNOBS[c]
    if c in BUTTONS: x, y = BUTTONS[c]; return x + BW / 2, y + BH / 2
    return SX + 480, SY + 200                          # the screen itself ("look", "listen", power)

def panel():
    im = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((170, 10, 1750, 790), 34, fill=BODY, outline=EDGE, width=4)
    lab = font(VT, 30)
    for n, (x, y) in KNOBS.items():
        d.text((x, y - KR - 12), "KNOB " + n[-1] if n.startswith("KNOB") else n, font=lab, fill=DIM, anchor="ms")
        d.ellipse((x - KR, y - KR, x + KR, y + KR), fill="#171410", outline=EDGE, width=4)
    for n, (x, y) in BUTTONS.items():
        d.rounded_rectangle((x, y, x + BW, y + BH), 12, fill="#1b1814", outline=EDGE, width=3)
        d.text((x + BW / 2, y + BH / 2 + 2), n, font=font(VT, 30 if len(n) < 5 else 26), fill=BONE, anchor="mm")
    d.rounded_rectangle((SX - 22, SY - 22, SX + 502, SY + 502), 20, fill="#050403", outline=EDGE, width=3)
    # the keys: 27 from F, black keys in their own row above the white ones
    for k in range(27):
        d.rounded_rectangle(key_box(k), 14 if k in WHITES else 12, fill="#3a3129" if k in WHITES else "#141110",
                            outline=EDGE, width=2)
    return im

def sword(d, tip, ang):
    """a sword whose point is at tip, pointing along ang (radians)"""
    ux, uy = math.cos(ang), math.sin(ang)
    px, py = -uy, ux
    def at(back, side): return (tip[0] - ux * back + px * side, tip[1] - uy * back + py * side)
    d.polygon([at(0, 0), at(30, 11), at(150, 11), at(150, -11), at(30, -11)], fill=INK, outline=BG)
    d.line([at(8, 0), at(146, 0)], fill="#9c917c", width=3)
    d.polygon([at(150, 38), at(166, 38), at(166, -38), at(150, -38)], fill=EMBER, outline=BG)
    d.polygon([at(166, 8), at(214, 8), at(214, -8), at(166, -8)], fill="#6b4a2b", outline=BG)
    c = at(226, 0)
    d.ellipse((c[0] - 14, c[1] - 14, c[0] + 14, c[1] + 14), fill=EMBER, outline=BG)

def aim(c):
    """where the sword's point goes for control c, and the angle it comes in at"""
    x, y = centre(c)
    if c not in KNOBS and c not in BUTTONS and "KEY" not in c:   # the screen: from the right, between the knobs and buttons
        return (SX + 492, SY + 200), math.radians(180)
    if "KEY" in c:                                     # a key: the point at its top
        ks = keys(c)
        x0, y0, x1, y1 = key_box(ks[len(ks) // 2])
        return ((x0 + x1) / 2 + 8, y0 + 10), math.radians(115)
    off = KR + 6 if c in KNOBS else 30
    if y < 250:                                        # the top row: from below right, so the hilt stays in frame
        return (x + off * 0.70, y + off * 0.70), math.radians(-135)
    return (x + off * 0.70, y - off * 0.70), math.radians(135)

os.chdir(D)
acts = []
for l in open("lesson.wav.acts"):
    t0, t1, ctl, kind, step, title, say = l.rstrip("\n").split("\t")
    acts.append((float(t0), float(t1), ctl, kind, int(step), title, say))
nsteps = max(a[4] for a in acts)
dur = float(subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0",
                            "lesson.wav"], capture_output=True, text=True).stdout)
base = panel()
screen = open("frames.rgb", "rb")
f_small, f_step, f_say = font(VT, 34), font(PIR, 58), font(VT, 64)
enc = subprocess.Popen(["ffmpeg", "-v", "error", "-y", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{W}x{H}",
                        "-r", "30", "-i", "-", "-i", "lesson.wav", "-filter_complex",
                        "[1:a]loudnorm=I=-16:TP=-1.5:LRA=11,aresample=48000[a]", "-map", "0:v", "-map", "[a]",
                        "-c:v", "libx264", "-crf", "20", "-preset", "medium", "-pix_fmt", "yuv420p", "-c:a", "aac",
                        "-b:a", "192k", "-movflags", "+faststart", "-shortest", f"ghoulbox-lesson-{N}.mp4"],
                       stdin=subprocess.PIPE)
spin = {k: -2.4 for k in KNOBS}                        # each knob's marker angle
k, cur = 0, 0
left = []                                              # keys a "press" holds down until a "letgo" (the left hand)
while True:
    raw = screen.read(240 * 240 * 3)
    if len(raw) < 240 * 240 * 3:
        break
    t = k / 30.0
    k += 1
    while cur + 1 < len(acts) and t >= acts[cur + 1][0]:
        cur += 1
    a = acts[cur] if t >= acts[0][0] else None
    im = base.copy()
    im.paste(Image.frombytes("RGB", (240, 240), raw).resize((480, 480), Image.NEAREST), (SX, SY))
    d = ImageDraw.Draw(im)
    busy = a and a[0] + 0.6 <= t < a[1] - (0.05 if a[3] in ("key", "press") else 0.4) and a[3] not in ("say", "letgo")   # (a key: held to its end)
    if a and a[3].startswith("turn") and busy and a[2] in KNOBS:   # the way it really turned
        spin[a[2]] += 0.12 if a[3] == "turn+" else -0.12
    for n, (x, y) in KNOBS.items():                    # the markers
        g = spin[n]
        d.line([(x + math.cos(g) * 14, y + math.sin(g) * 14), (x + math.cos(g) * (KR - 8), y + math.sin(g) * (KR - 8))],
               fill=BONE, width=6)
    if a and a[3] == "press" and t >= a[0] + 0.6:
        left = keys(a[2])
    if a and a[3] == "letgo":
        left = []
    for kk in left:                                    # the left hand, still down
        d.rounded_rectangle(key_box(kk), 14, fill=EMBER)
    if a and "KEY" in a[2]:                            # a key pressed (with GLO held for a mute)
        down = busy and (a[3] in ("key", "press") or a[0] + 1.5 <= t < a[0] + 1.75)
        for kk in keys(a[2]):
            box = key_box(kk)
            d.rounded_rectangle((box[0] - 5, box[1] - 5, box[2] + 5, box[3] + 5), 16, outline=EMBER if busy else BLOOD,
                                width=6)
            if down:
                d.rounded_rectangle(box, 14, fill=EMBER)
        if a[2].startswith("GLO+"):
            bx, by = BUTTONS["GLO"]
            d.rounded_rectangle((bx, by, bx + BW, by + BH), 12, fill=EMBER if busy else "#1b1814")
            d.rounded_rectangle((bx - 6, by - 6, bx + BW + 6, by + BH + 6), 16, outline=EMBER, width=6)
            d.text((bx + BW / 2, by + BH / 2 + 2), "GLO", font=font(VT, 30), fill=BG if busy else BONE, anchor="mm")
    if a and a[2] in KNOBS or a and a[2] in BUTTONS:   # the control in use, lit
        x, y = centre(a[2])
        r = KR + 10 if a[2] in KNOBS else 50
        glow = EMBER if busy else BLOOD
        if a[2] in KNOBS:
            d.ellipse((x - r, y - r, x + r, y + r), outline=glow, width=6)
        else:
            bx, by = BUTTONS[a[2]]
            pressed = busy and a[3] in ("tap", "hold")
            d.rounded_rectangle((bx - 6, by - 6, bx + BW + 6, by + BH + 6), 16, outline=glow, width=6)
            if pressed:
                d.rounded_rectangle((bx, by, bx + BW, by + BH), 12, fill=EMBER)
                d.text((bx + BW / 2, by + BH / 2 + 2), a[2], font=font(VT, 30 if len(a[2]) < 5 else 26), fill=BG,
                       anchor="mm")
    if a:                                              # the sword: glides to the control, then thrusts as the hand works
        tip, ang = aim(a[2])
        if cur > 0 and t < a[0] + 0.45:
            p, _ = aim(acts[cur - 1][2])
            e = (t - a[0]) / 0.45
            e = e * e * (3 - 2 * e)
            tip = (p[0] + (tip[0] - p[0]) * e, p[1] + (tip[1] - p[1]) * e)
        bob = 10 * math.sin(t * 2 * math.pi * 1.4) if busy else 4 * math.sin(t * 2 * math.pi * 0.5)
        tip = (tip[0] - math.cos(ang) * (bob + 12), tip[1] - math.sin(ang) * (bob + 12))
        sword(d, tip, ang)
        d.text((180, 830), f"{KICK}  ·  {TITLE.upper()}", font=f_small, fill=DIM)
        d.text((180, 862), f"{a[4]} / {nsteps}   {a[5]}", font=f_step, fill=EMBER)
        d.text((180, 950), ("Press the keys: " if "+" in a[2] else "Press the key: ") + a[6] if a[3] in ("key", "press") else a[6], font=f_say, fill=INK)
    else:
        d.text((180, 830), KICK, font=f_small, fill=DIM)
        d.text((180, 862), TITLE, font=f_step, fill=BLOOD)
    enc.stdin.write(im.tobytes())
enc.stdin.close()
enc.wait()
