#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""FM6 factory patches (src/eng_fm6.c) -> a C header of packed 128-byte voices.

  tools/gen_fm6_patches.py OUT.h

Felucca's own patches, made for it (no factory ROM data of any instrument): written here as readable
operator settings, packed in the generic 128-byte 6-operator voice layout (the 32-voice bank's record).
Operators are listed as OP1..OP6; the packed record starts with OP6, as the format does.
"""
import sys
from pathlib import Path


def op(r=(99, 99, 99, 99), l=(99, 99, 99, 0), ol=0, fc=1, ff=0, det=7, mode=0, kvs=0, ams=0, rs=0,
       bp=39, ld=0, rd=0, lc=0, rc=0):
    return dict(r=r, l=l, ol=ol, fc=fc, ff=ff, det=det, mode=mode, kvs=kvs, ams=ams, rs=rs, bp=bp, ld=ld, rd=rd,
                lc=lc, rc=rc)


def voice(name, alg, ops, fb=0, oks=1, pr=(99, 99, 99, 99), pl=(50, 50, 50, 50), lfs=35, lfd=0, lpmd=0, lamd=0,
          lks=1, lfw=0, lpms=3, trnsp=24):
    """the 155-byte single-voice layout (OP6 first); alg 1..32"""
    assert len(ops) == 6 and 1 <= alg <= 32 and len(name) <= 10
    v = []
    for o in reversed(ops):                        # OP6 .. OP1
        v += list(o["r"]) + list(o["l"]) + [o["bp"], o["ld"], o["rd"], o["lc"], o["rc"], o["rs"], o["ams"],
                                            o["kvs"], o["ol"], o["mode"], o["fc"], o["ff"], o["det"]]
    v += list(pr) + list(pl) + [alg - 1, fb, oks, lfs, lfd, lpmd, lamd, lks, lfw, lpms, trnsp]
    v += [ord(c) for c in name.ljust(10)]
    assert len(v) == 155
    return v


def pack(v):
    """155 -> the 128-byte record (the C side: eng_fm6.c fm6_pack)"""
    b = []
    for k in range(6):
        o = v[k * 21:k * 21 + 21]
        b += o[0:11]
        b += [(o[11] & 3) | (o[12] & 3) << 2, (o[13] & 7) | (o[20] & 15) << 3, (o[14] & 3) | (o[15] & 7) << 2,
              o[16], (o[17] & 1) | (o[18] & 31) << 1, o[19]]
    b += v[126:135]
    b += [(v[135] & 7) | (v[136] & 1) << 3]
    b += v[137:141]
    b += [(v[141] & 1) | (v[142] & 7) << 1 | (v[143] & 7) << 4, v[144]]
    b += v[145:155]
    assert len(b) == 128 and all(0 <= x < 128 for x in b)
    return b


INIT = voice("INIT VOICE", 1, [op(ol=99)] + [op() for _ in range(5)], lpms=3)

# OP1..OP6. Algorithm 5 = three pairs (1<-2, 3<-4, 5<-6 with the feedback), 32 = six carriers.
PATCHES = [
    ("TINE EP", voice("TINE EP", 5, [
        op(r=(96, 25, 25, 67), l=(99, 75, 0, 0), ol=98, kvs=2, rs=3),
        op(r=(95, 50, 35, 78), l=(99, 75, 0, 0), ol=58, kvs=7, rs=3),
        op(r=(95, 20, 20, 50), l=(99, 95, 0, 0), ol=90, kvs=2, det=8, rs=2),
        op(r=(97, 62, 40, 60), l=(99, 60, 0, 0), ol=68, fc=14, kvs=6, rs=3),
        op(r=(95, 30, 20, 60), l=(99, 90, 0, 0), ol=78, det=6, kvs=1),
        op(r=(95, 40, 30, 60), l=(99, 80, 0, 0), ol=52, kvs=3)],
        fb=3, lfs=34, lfd=33, lfw=4, lpms=2)),
    ("BELL", voice("GLASS BELL", 5, [
        op(r=(99, 40, 25, 35), l=(99, 80, 0, 0), ol=96, kvs=1),
        op(r=(99, 35, 25, 35), l=(99, 70, 0, 0), ol=78, fc=3, ff=50, kvs=3),
        op(r=(99, 45, 30, 40), l=(99, 70, 0, 0), ol=84, fc=2, ff=38, det=9, kvs=1),
        op(r=(99, 50, 30, 40), l=(99, 60, 0, 0), ol=70, fc=5, kvs=3),
        op(r=(99, 60, 40, 45), l=(99, 50, 0, 0), ol=78, fc=5, ff=40, det=5),
        op(r=(99, 60, 40, 45), l=(99, 50, 0, 0), ol=60)],
        fb=0, lpms=0)),
    ("FM BASS", voice("ROUND BASS", 5, [
        op(r=(99, 40, 30, 75), l=(99, 85, 70, 0), ol=99, kvs=2),
        op(r=(99, 60, 40, 75), l=(99, 72, 55, 0), ol=80, kvs=5),
        op(r=(99, 55, 40, 75), l=(99, 70, 50, 0), ol=80, det=8, kvs=1),
        op(r=(99, 72, 50, 75), l=(99, 65, 40, 0), ol=78, fc=2, kvs=6),
        op(ol=0),
        op(ol=0)],
        fb=0, lpms=0)),
    ("BRASS", voice("BRASS SECT", 5, [
        op(r=(65, 50, 40, 60), l=(99, 92, 90, 0), ol=98, kvs=1),
        op(r=(55, 50, 40, 60), l=(99, 88, 85, 0), ol=80, kvs=3),
        op(r=(62, 50, 40, 60), l=(99, 92, 90, 0), ol=92, det=9, kvs=1),
        op(r=(50, 50, 40, 60), l=(99, 88, 85, 0), ol=78, kvs=3),
        op(r=(62, 50, 40, 60), l=(99, 92, 90, 0), ol=88, det=5, kvs=1),
        op(r=(52, 50, 40, 60), l=(99, 85, 82, 0), ol=72, kvs=2)],
        fb=5, pr=(80, 60, 99, 60), pl=(46, 50, 50, 50), lfs=33, lfd=50, lpmd=6, lfw=4, lpms=3)),
    ("PAD", voice("SOFT PAD", 5, [
        op(r=(40, 30, 40, 40), l=(99, 95, 90, 0), ol=92, det=5),
        op(r=(35, 30, 40, 40), l=(85, 90, 80, 0), ol=60, kvs=1),
        op(r=(40, 30, 40, 40), l=(99, 95, 90, 0), ol=92, det=10),
        op(r=(30, 25, 40, 40), l=(80, 90, 85, 0), ol=55, fc=2, kvs=1),
        op(r=(38, 30, 40, 40), l=(99, 95, 90, 0), ol=80, fc=2),
        op(r=(35, 30, 40, 40), l=(85, 90, 85, 0), ol=50)],
        fb=2, lfs=30, lfd=60, lpmd=4, lfw=4, lpms=2)),
    ("MARIMBA", voice("WOOD BARS", 5, [
        op(r=(99, 52, 40, 55), l=(99, 0, 0, 0), ol=99, kvs=2, rs=2),
        op(r=(99, 75, 50, 60), l=(99, 0, 0, 0), ol=70, fc=4, kvs=5, rs=3),
        op(r=(99, 70, 50, 60), l=(99, 0, 0, 0), ol=72, fc=4, kvs=3, rs=3),
        op(r=(99, 80, 50, 60), l=(99, 0, 0, 0), ol=45, kvs=3, rs=3),
        op(ol=0),
        op(ol=0)],
        fb=0, lpms=0)),
    ("ORGAN", voice("DRAWBARS", 32, [
        op(r=(99, 99, 99, 85), l=(99, 99, 99, 0), ol=90),
        op(r=(99, 99, 99, 85), l=(99, 99, 99, 0), ol=84, fc=0),
        op(r=(99, 99, 99, 85), l=(99, 99, 99, 0), ol=86, fc=2),
        op(r=(99, 99, 99, 85), l=(99, 99, 99, 0), ol=76, fc=3),
        op(r=(99, 99, 99, 85), l=(99, 99, 99, 0), ol=78, fc=4),
        op(r=(99, 99, 99, 85), l=(99, 99, 99, 0), ol=72, fc=1, ff=50)],
        fb=0, lfs=60, lpmd=3, lfw=4, lpms=3)),
    ("PLUCK", voice("NYLON PICK", 5, [
        op(r=(99, 45, 30, 60), l=(99, 60, 0, 0), ol=98, kvs=2, rs=2),
        op(r=(99, 70, 40, 60), l=(99, 40, 0, 0), ol=76, fc=3, kvs=4, rs=2),
        op(r=(99, 60, 40, 60), l=(99, 30, 0, 0), ol=74, fc=2, det=8, kvs=2, rs=2),
        op(r=(99, 75, 40, 60), l=(99, 30, 0, 0), ol=68, kvs=3, rs=2),
        op(ol=0),
        op(ol=0)],
        fb=0, lpms=0)),
    # GHOULBOX: the dungeon patches F9..F15 (appended: F1..F8 keep their numbers; OWN moves to 15, see
    # FM6_NFACTORY_V1 in eng_fm6.c). Dark and slow, for the HALL: rates 99 fastest, levels 99 loudest
    ("FRENCH HORN", voice("FR HORN", 5, [          # three pairs, modulators low (round, not brassy), swelling in
        op(r=(48, 40, 35, 52), l=(99, 92, 88, 0), ol=97, kvs=2),
        op(r=(40, 35, 30, 50), l=(92, 80, 72, 0), ol=66, kvs=4),
        op(r=(46, 40, 35, 52), l=(99, 90, 86, 0), ol=88, det=9, kvs=2),
        op(r=(38, 35, 30, 50), l=(90, 78, 70, 0), ol=60, kvs=4),
        op(r=(44, 40, 35, 52), l=(99, 90, 86, 0), ol=78, fc=0, det=5, kvs=1),
        op(r=(36, 35, 30, 50), l=(88, 75, 68, 0), ol=58, fc=1, kvs=3)],
        fb=3, pr=(72, 60, 99, 55), pl=(46, 50, 50, 50), lfs=32, lfd=70, lpmd=5, lfw=4, lpms=3)),
    ("HARPSICHORD", voice("VIRGINAL", 5, [         # a plucked jack: no velocity, a buzzing 4' choir an octave up
        op(r=(99, 32, 24, 72), l=(99, 70, 0, 0), ol=98, rs=3),
        op(r=(99, 55, 30, 72), l=(99, 45, 0, 0), ol=78, fc=3, rs=3),
        op(r=(99, 34, 26, 72), l=(99, 66, 0, 0), ol=82, fc=2, det=9, rs=3),
        op(r=(99, 60, 35, 72), l=(99, 40, 0, 0), ol=74, fc=5, rs=3),
        op(r=(99, 40, 28, 72), l=(99, 55, 0, 0), ol=70, det=5, rs=3),
        op(r=(99, 70, 40, 72), l=(99, 30, 0, 0), ol=80, fc=4, rs=3)],
        fb=6, lpms=0)),
    ("GLASS CHOIR", voice("GLASSCHOIR", 22, [      # alg 22: 1<-2, and 6 under 3, 4, 5: a vowel body, a glass top
        op(r=(30, 30, 40, 38), l=(99, 92, 90, 0), ol=72, fc=4, ff=2),
        op(r=(30, 30, 40, 38), l=(90, 85, 80, 0), ol=48, fc=1),
        op(r=(34, 30, 40, 36), l=(99, 95, 92, 0), ol=94, det=6),
        op(r=(32, 30, 40, 36), l=(99, 95, 92, 0), ol=86, fc=2, det=9),
        op(r=(30, 30, 40, 36), l=(99, 95, 92, 0), ol=76, fc=3, det=7),
        op(r=(28, 30, 40, 36), l=(92, 88, 85, 0), ol=62, fc=1)],
        fb=4, lfs=28, lfd=55, lpmd=6, lamd=4, lfw=4, lpms=3)),
    ("DARK STRINGS", voice("DARK STRGS", 5, [      # a low section: an octave down under the unison, a bow's rasp
        op(r=(42, 30, 40, 44), l=(99, 94, 92, 0), ol=94, det=5),
        op(r=(36, 30, 40, 44), l=(88, 85, 80, 0), ol=64, kvs=2),
        op(r=(42, 30, 40, 44), l=(99, 94, 92, 0), ol=92, det=10),
        op(r=(36, 30, 40, 44), l=(88, 85, 80, 0), ol=62, kvs=2),
        op(r=(40, 30, 40, 44), l=(99, 94, 92, 0), ol=88, fc=0, det=7),
        op(r=(34, 30, 40, 44), l=(85, 82, 78, 0), ol=66, fc=1)],
        fb=5, lfs=30, lfd=65, lpmd=5, lfw=4, lpms=2)),
    ("LOFI FLUTE", voice("LOFI FLUTE", 5, [        # a soft tone, a faint octave, a breath (feedback noise) and chiff
        op(r=(70, 40, 40, 55), l=(99, 92, 90, 0), ol=98, det=8, kvs=2),
        op(r=(70, 40, 40, 55), l=(80, 70, 66, 0), ol=52),
        op(r=(66, 40, 40, 55), l=(99, 90, 88, 0), ol=62, fc=2, det=6),
        op(r=(66, 40, 40, 55), l=(70, 60, 55, 0), ol=40),
        op(r=(90, 60, 40, 60), l=(99, 55, 45, 0), ol=64),
        op(r=(90, 65, 40, 60), l=(99, 60, 50, 0), ol=84, fc=7)],
        fb=7, lfs=36, lfd=40, lpmd=4, lfw=4, lpms=3)),
    ("CRYPT ORGAN", voice("PIPE ORGAN", 32, [      # six pipes, 16' to 2', the top one reedy (feedback)
        op(r=(80, 99, 99, 60), l=(99, 99, 99, 0), ol=86, fc=0),
        op(r=(80, 99, 99, 60), l=(99, 99, 99, 0), ol=92),
        op(r=(80, 99, 99, 60), l=(99, 99, 99, 0), ol=84, fc=2, det=8),
        op(r=(80, 99, 99, 60), l=(99, 99, 99, 0), ol=74, fc=3),
        op(r=(80, 99, 99, 60), l=(99, 99, 99, 0), ol=72, fc=4, det=6),
        op(r=(78, 99, 99, 60), l=(99, 99, 99, 0), ol=70, fc=1)],
        fb=6, lfs=50, lpmd=2, lfw=4, lpms=2)),
    ("TUBA", voice("TUBA", 1, [                    # alg 1: 1<-2 the tone, 3<-4<-5<-6 a soft growl under it
        op(r=(50, 40, 35, 55), l=(99, 92, 90, 0), ol=99, kvs=2),
        op(r=(42, 38, 30, 52), l=(90, 78, 72, 0), ol=68, kvs=4),
        op(r=(48, 40, 35, 55), l=(99, 90, 88, 0), ol=80, fc=0, det=8, kvs=1),
        op(r=(40, 38, 30, 52), l=(88, 76, 70, 0), ol=58, fc=1),
        op(r=(40, 38, 30, 52), l=(85, 75, 68, 0), ol=48, fc=1),
        op(r=(40, 38, 30, 52), l=(80, 70, 64, 0), ol=40, fc=1)],
        fb=4, pr=(70, 60, 99, 55), pl=(47, 50, 50, 50), lfs=30, lfd=75, lpmd=3, lfw=4, lpms=2)),
]


def c_rows(b):
    return ",\n".join("     " + ", ".join(f"{x:3d}" for x in b[i:i + 16]) for i in range(0, len(b), 16))


def main(path):
    L = ["/* generated by tools/gen_fm6_patches.py: FM6's own factory patches, packed (128 bytes) */",
         "#pragma once", "#include <stdint.h>", f"#define FM6_NFACTORY {len(PATCHES)}",
         "static const uint8_t FM6_INIT[128] = {", c_rows(pack(INIT)), "};",
         f"static const uint8_t FM6_FACTORY[{len(PATCHES)}][128] = {{"]
    for name, v in PATCHES:
        L += [f"    {{/* {name} */", c_rows(pack(v)) + "},"]
    L += ["};"]
    Path(path).write_text("\n".join(L) + "\n")
    print(f"fm6 patches: {len(PATCHES)} -> {path}")


def js():
    """the same patches as the web editor's mock device holds them (web/editor.html FM6 INIT_PK, FACTORY_PK)"""
    a = lambda b: "[" + ",".join(str(x) for x in b) + "]"
    print("  const INIT_PK = " + a(pack(INIT)) + ";")
    print("  const FACTORY_PK = [")
    for name, v in PATCHES:
        print("    " + a(pack(v)) + ",   /* " + name + " */")
    print("  ];")


if __name__ == "__main__":
    js() if sys.argv[1] == "--js" else main(sys.argv[1])
