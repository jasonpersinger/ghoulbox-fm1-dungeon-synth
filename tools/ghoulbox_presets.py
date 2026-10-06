#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# GHOULBOX: the dungeon presets, one table -> the engines' preset arrays (firmware/src/eng_*.c) and the web
# editor's mirror of them (web/editor.html). Appended after each engine's own presets (a project stores a
# preset's index: never reorder, only append), between GHOULBOX markers; run again after an edit, the block
# is replaced.
#   python3 tools/ghoulbox_presets.py
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# engine -> (its .c, its PRESETS array, the editor's engine name)
ENGINES = {
    "ANALOG": ("eng_analog.c", "ANALOG_PRESETS"),
    "VOICE": ("eng_formant.c", "FORMANT_PRESETS"),
    "WHEEL": ("eng_wheel.c", "WHEEL_PRESETS"),
    "PHYS": ("eng_phys.c", "PHYS_PRESETS"),
    "TRIO": ("eng_trio.c", "TRIO_PRESETS"),
    "LOFI": ("eng_lofi.c", "LOFI_PRESETS"),
    "PHASE": ("eng_phase.c", "PHASE_PRESETS"),
    "NOISE": ("eng_noise.c", "NOISE_PRESETS"),
    "GURDY": ("eng_gurdy.c", "GURDY_PRESETS"),
}

# name, EDIT 1..8, {A D S R}, filter env, mono, FX(dist, chorus, delay, reverb), suggested pattern
# (the reverb sends are high: GHOULBOX's HALL; TAPE / CRSH on the master do the rest of the age)
PRESETS = {
    "ANALOG": [  # WAVE (SAW SQR TRI SIN PWM) DTN MIX NOIS | CUT RES DRV KTR
        ("CRYPT PAD", [0, 14, 64, 6, 42, 8, 6, 40], [95, 90, 120, 105], 6, 0, (0, 60, 10, 100), 5),
        ("FROST STR", [4, 9, 50, 0, 58, 5, 0, 48], [70, 90, 115, 95], 4, 0, (0, 90, 10, 95), 5),
        ("WAR HORN", [0, 6, 64, 0, 30, 18, 12, 64], [45, 80, 105, 70], 40, 0, (5, 20, 15, 85), 4),
        ("DIRGE BASS", [1, 0, 0, 0, 34, 10, 20, 32], [10, 80, 110, 60], 15, 1, (10, 0, 0, 40), 8),
    ],
    "VOICE": [  # VOWL (A 0, E 32, I 64, O 95, U 127) VOWL2 TALK SHIFT | BUZZ BRTH Q RAND
        ("MONKS", [105, 120, 0, -5, 75, 18, 70, 6], [70, 90, 120, 100], 0, 0, (0, 60, 0, 110), 5),
        ("CRYPT CHOIR", [95, 0, 100, -2, 45, 30, 55, 10], [100, 90, 120, 110], 0, 0, (0, 80, 10, 115), 5),
        ("WRAITH", [127, 64, 110, 0, 20, 100, 40, 30], [110, 90, 110, 115], 0, 0, (0, 50, 30, 115), 5),
    ],
    "WHEEL": [  # REG (FLUTE MELLO HOLLW SMOOT 3BAR BLUES GOSPL ROCK TOPS CLARI REED STRNG CHAPL BRITE BASS FULL)
                # SUB BODY TOP | PERC CLICK DRV ROTR (OFF SLOW FAST)
        ("CRYPT ORGAN", [12, 2, 0, -3, 0, 5, 15, 1], [20, 64, 127, 70], 0, 0, (0, 10, 0, 100), 5),
        ("CATHEDRAL", [15, 3, 0, -2, 0, 0, 10, 0], [30, 64, 127, 90], 0, 0, (0, 0, 0, 115), 5),
        ("HARMONIUM", [10, 0, 1, -4, 0, 10, 25, 1], [15, 64, 127, 50], 0, 0, (5, 20, 0, 85), 6),
        ("TOWER FLUTE", [0, 0, -3, -6, 0, 0, 30, 1], [25, 64, 127, 60], 0, 1, (0, 30, 20, 95), 4),
    ],
    "PHYS": [  # STRNG: MODEL STRC BRIT DAMP POS ACC BOW EXC; SYMP: MODEL CHRD BRIT DAMP SYMP ACC BUZZ EXC
        ("LUTE", [1, 48, 55, 70, 30, 127, 0, 0], [0, 100, 127, 70], 0, 0, (0, 15, 20, 80), 3),
        ("HARPSICHORD", [1, 80, 110, 78, 12, 127, 0, 0], [0, 100, 127, 50], 0, 0, (0, 10, 10, 75), 3),
        ("DUNGEON HARP", [3, 4, 50, 80, 70, 127, 0, 0], [0, 100, 127, 90], 0, 0, (0, 20, 20, 95), 3),
        ("GURDY DRONE", [3, 1, 70, 95, 80, 127, 110, 0], [30, 100, 127, 90], 0, 0, (5, 10, 0, 90), 5),
    ],
    "TRIO": [  # WAVE INT2 INT3 DTN | MODE CUT RES PW
        ("FANTASY PAD", [0, 12, 7, 14, 0, 55, 20, 64], [90, 90, 115, 100], 10, 0, (0, 90, 25, 110), 5),
        ("MOURN HORN", [1, 0, -12, 5, 0, 40, 25, 50], [40, 85, 105, 70], 35, 0, (5, 20, 10, 90), 4),
        ("STRING MACH", [0, 12, 0, 18, 0, 70, 5, 64], [60, 90, 120, 95], 0, 0, (0, 110, 0, 100), 5),
    ],
    "LOFI": [  # CHIP (4BIT 4B/2 8BIT 1BIT STEP) WAVE (PLS TRI SAW NOIS WRAM) DUTY CRSH | SWP VIB ARP TONE
        ("TOWER LEAD", [0, 1, 64, 0, 0, 30, 0, 70], [5, 70, 110, 60], 0, 1, (0, 20, 35, 90), 4),
        ("CASIO CHOIR", [0, 4, 92, 0, 0, 20, 0, 80], [60, 90, 115, 90], 0, 0, (0, 70, 20, 100), 5),
        ("RECORDER", [2, 1, 64, 0, 0, 24, 0, 60], [12, 70, 115, 50], 0, 1, (0, 20, 25, 90), 4),
    ],
    "PHASE": [  # WAVE WAVE2 DCW ENV | DTN LINE SUB -
        ("GRIM BRASS", [0, 0, 25, 70, 6, 0, 30, 0], [50, 80, 100, 70], 0, 0, (0, 20, 15, 90), 4),
    ],
    "NOISE": [  # MODE (ANLG DUST LFSR META) COLR FREQ RES | TRK DENS DRFT CRSH
        ("CAVE WIND", [0, 40, 50, 90, 0, 0, 90, 0], [110, 90, 120, 120], 0, 0, (0, 40, 20, 120), 5),
        ("TORCH", [1, 30, 100, 60, 0, 60, 30, 0], [20, 90, 127, 80], 0, 0, (0, 0, 0, 70), 5),
    ],
    "GURDY": [  # DRN (C .. B) DLVL 5TH WHL | BUZZ COUP (HOLD 1/4 1/8 1/8T 1/16) BODY WOBL
        ("HURDY GURDY", [2, 90, 50, 80, 70, 2, 70, 40], [8, 64, 127, 40], 0, 1, (5, 10, 0, 90), 4),
        ("DRONE WHEEL", [2, 110, 80, 60, 0, 0, 90, 60], [40, 64, 127, 90], 0, 1, (0, 20, 0, 105), 5),
        ("DANCE GURDY", [2, 80, 40, 100, 110, 3, 60, 25], [4, 64, 127, 30], 0, 1, (10, 0, 10, 75), 3),
        ("TROMPETTE", [2, 70, 30, 90, 95, 1, 65, 35], [6, 64, 127, 50], 0, 1, (8, 0, 0, 90), 4),
        ("VIELLE", [2, 0, 0, 85, 0, 0, 80, 30], [20, 64, 120, 60], 0, 1, (0, 15, 10, 95), 4),
    ],
}

BEGIN, END = "GHOULBOX presets (tools/ghoulbox_presets.py)", "GHOULBOX end"


def c_line(p):
    name, e, env, fenv, mono, fx, pat = p
    return (f'    {{"{name}", {{{", ".join(map(str, e))}}}, {{{", ".join(map(str, env))}}}, {fenv}, {mono}, '
            f'FX({", ".join(map(str, fx))}), PAT({pat})}},')


def js_item(p):
    name, e, env, _, mono, _, pat = p
    return f'P("{name}", [{", ".join(map(str, e))}], [{", ".join(map(str, env))}], {mono}, {pat})'


def check(eng, p):
    name, e, env, fenv, mono, fx, pat = p
    assert len(name) <= 12 and name == name.upper(), (eng, name)
    assert len(e) == 8 and len(env) == 4 and len(fx) == 4, (eng, name)
    assert all(0 <= v <= 127 for v in env + list(fx)) and mono in (0, 1) and 1 <= pat <= 13, (eng, name)


def strip_block(s, open_c, close_c):
    return re.sub(rf"\s*{re.escape(open_c)} {re.escape(BEGIN)} {re.escape(close_c)}.*?{re.escape(open_c)} "
                  rf"{re.escape(END)} {re.escape(close_c)}", "", s, flags=re.S)


def write_c(eng, presets):
    fname, arr = ENGINES[eng]
    path = ROOT / "firmware/src" / fname
    s = strip_block(path.read_text(), "/*", "*/")
    i = s.index(f"static const preset_t {arr}[] = {{")
    j = s.index("\n};", i)                              # the array's end
    block = f"\n    /* {BEGIN} */\n" + "\n".join(c_line(p) for p in presets) + f"\n    /* {END} */"
    path.write_text(s[:j] + block + s[j:])


def write_js(s, eng, presets):
    s = s
    i = s.index(f'{{ name: "{eng}",')
    k = s.index("presets: [", i) + len("presets: [")
    depth, j = 1, k                                     # the array's matching ]
    while depth:
        depth += {"[": 1, "]": -1}.get(s[j], 0)
        j += 1
    j -= 1
    inner = strip_block(s[k:j], "/*", "*/").rstrip().rstrip(",")   # (the separator before an old block too)
    sep = "," if inner.strip() else ""
    block = f"{sep}\n        /* {BEGIN} */ " + ", ".join(js_item(p) for p in presets) + f" /* {END} */"
    return s[:k] + inner + block + s[j:]


def main():
    html = ROOT / "web/editor.html"
    s = html.read_text()
    for eng, presets in PRESETS.items():
        for p in presets:
            check(eng, p)
        write_c(eng, presets)
        s = write_js(s, eng, presets)
    html.write_text(s)
    print(f"ghoulbox presets: {sum(len(v) for v in PRESETS.values())} in {len(PRESETS)} engines")


if __name__ == "__main__":
    sys.exit(main())
