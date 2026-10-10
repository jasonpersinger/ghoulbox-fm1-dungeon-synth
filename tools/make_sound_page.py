#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""GHOULBOX: the public "hear every sound" page, from the firmware's own audio code.

  python3 tools/make_sound_page.py SITE_DIR      -> SITE_DIR/sounds/ (index.html, audio/, fonts/)

Every sound the PRESETS list offers (build/host/desc.json: ORDER, minus HIDDEN), rendered by tests/preset_preview.c
on its own suggested pattern; the demo piece from tests/demo_clip.c. Needs gcc, lame and ffmpeg, and build/gen
(./build.sh) and build/host/desc.json (tests/run_tests.sh). Run it after web/make_site.py, into the same SITE_DIR.
"""
import csv
import html
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CC = ["gcc", "-O2", "-w", f"-I{ROOT}/build/gen", f"-I{ROOT}/firmware/src", f"-I{ROOT}/tests"]
WHAT = {"ANALOG": "virtual analog", "FM6": "6-operator FM", "PHASE": "phase distortion", "LOFI": "chiptune",
        "SAMPLE": "real recordings (CC0)", "VOICE": "sung vowels", "TRIO": "three oscillators",
        "WHEEL": "tonewheel organ", "GRAIN": "granular", "PHYS": "physical models", "GURDY": "hurdy-gurdy",
        "NOISE": "noise and ambience", "DRUM": "drum kits"}


def ghoulbox_names():
    """GHOULBOX's own presets: tools/ghoulbox_presets.py, FM6's F9.., SAMPLE's CC0 sets"""
    names = set(re.findall(r'\("([A-Z0-9 ]+)", \[', (ROOT / "tools/ghoulbox_presets.py").read_text()))
    return names | {"FRENCH HORN", "VIRGINAL", "GLASS CHOIR", "DARK STRINGS", "LOFI FLUTE", "PIPE ORGAN", "TUBA",
                    "BOWED PSALT", "REN ORGAN", "TENOR RECORD", "FOLK HARP", "CRYPT KIT", "TOMB DRUMS"}


def main(site):
    out = Path(site).resolve() / "sounds"
    if out.exists():
        shutil.rmtree(out)
    (out / "audio").mkdir(parents=True)
    (out / "fonts").mkdir()
    d = json.loads((ROOT / "build/host/desc.json").read_text())
    hidden = {}
    for e, n in d["HIDDEN"]:
        hidden.setdefault(e, set()).add(n)
    gb = ghoulbox_names()
    with tempfile.TemporaryDirectory() as t:
        t = Path(t)
        subprocess.run(CC + ["-o", t / "preset_preview", ROOT / "tests/preset_preview.c", "-lm"], check=True)
        idx = subprocess.run([t / "preset_preview", t], check=True, capture_output=True, text=True).stdout
        subprocess.run(CC + ["-o", t / "demo_clip", ROOT / "tests/demo_clip.c", "-lm"], check=True)
        subprocess.run([t / "demo_clip", t / "demo.wav"], check=True, capture_output=True)
        subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i", t / "demo.wav", "-af",
                        "loudnorm=I=-14:TP=-1:LRA=11,afade=t=out:st=13.0:d=0.5", t / "demo_n.wav"], check=True)
        subprocess.run(["lame", "--quiet", "-b", "160", t / "demo_n.wav", out / "audio/demo.mp3"], check=True)
        by_engine = {}
        for e, k, name, pat in csv.reader(idx.splitlines(), delimiter="\t"):
            if e not in d["ORDER"] or name in hidden.get(e, set()):
                continue
            if any(x["name"] == name for x in by_engine.get(e, [])):
                continue                                    # (an alias: the same sound)
            fid = f"{e}_{int(k):02d}"
            subprocess.run(["lame", "--quiet", "-b", "80", "-m", "j", t / f"{fid}.wav", out / f"audio/{fid}.mp3"],
                           check=True)
            by_engine.setdefault(e, []).append({"id": fid, "name": name, "pat": pat, "gb": name in gb})
    for f in ("PirataOne-Regular.ttf", "VT323-Regular.ttf"):
        shutil.copy(ROOT / "assets/fonts" / f, out / "fonts" / f)
    total = sum(len(v) for v in by_engine.values())
    shelves = []
    for e in d["ORDER"]:
        if e not in by_engine:
            continue
        cards = "".join(
            f'<li class="card" data-src="audio/{s["id"]}.mp3"><button type="button" class="play" aria-label="Play '
            f'{html.escape(s["name"])}"></button><span class="nm">{html.escape(s["name"])}</span>'
            f'<span class="meta">plays {html.escape(s["pat"])}{" · <b>GHOULBOX</b>" if s["gb"] else ""}</span>'
            f'<i class="bar"></i></li>' for s in by_engine[e])
        shelves.append(f'<section><h2><span class="eng">{e}</span> <span class="what">{WHAT.get(e, "")}</span>'
                       f'<span class="n">{len(by_engine[e])}</span></h2><ul class="grid">{cards}</ul></section>')
    page = (ROOT / "tools/sound_page.html").read_text()
    page = page.replace("{{TOTAL}}", str(total)).replace("{{ENGINES}}", str(len(by_engine)))
    page = page.replace("{{SHELVES}}", "\n".join(shelves))
    (out / "index.html").write_text(page)
    print(f"sound page: {total} sounds in {len(by_engine)} engines -> {out}")
    return total


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
