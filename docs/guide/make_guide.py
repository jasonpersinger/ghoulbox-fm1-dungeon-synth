#!/usr/bin/env python3
"""GHOULBOX user guide: docs/guide/guide.html (+ the sound list from the firmware's own tables) -> a PDF.
  python3 docs/guide/make_guide.py OUT.pdf      (needs build/host/desc.json: tests/run_tests.sh writes it; and Chrome)"""
import html, json, re, shutil, subprocess, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
d = json.loads((ROOT / "build/host/desc.json").read_text())
gb = set()                                           # GHOULBOX's own presets: the generator's list, GURDY, FM6 F9.., SAMPLE 5..
for m in re.finditer(r'\("([A-Z0-9 ]+)", \[', (ROOT / "tools/ghoulbox_presets.py").read_text()):
    gb.add(m.group(1))
gb |= {"FRENCH HORN", "VIRGINAL", "GLASS CHOIR", "DARK STRINGS", "LOFI FLUTE", "PIPE ORGAN", "TUBA",
       "BOWED PSALT", "REN ORGAN", "TENOR RECORD", "FOLK HARP", "CRYPT KIT", "TOMB DRUMS"}
hidden = {}
for e, n in d["HIDDEN"]:
    hidden.setdefault(e, set()).add(n)
WHAT = {"ANALOG": "virtual analog", "FM6": "6-operator FM", "PHASE": "phase distortion", "LOFI": "chiptune",
        "SAMPLE": "real recordings", "VOICE": "sung vowels", "TRIO": "three oscillators", "WHEEL": "tonewheel organ",
        "GRAIN": "granular", "PHYS": "physical models", "GURDY": "hurdy-gurdy", "NOISE": "noise and ambience"}
rows, total = [], 0
for name in d["ORDER"]:
    eng = next(e for e in d["ENG"] if e["name"] == name)
    seen, cells = set(), []
    for p in eng["presets"]:
        n = p["name"]
        if n in hidden.get(name, set()) or n in seen:
            continue
        seen.add(n)
        cells.append(html.escape(n) + (' <span class="gb">GB</span>' if n in gb else ""))
    total += len(cells)
    rows.append(f'<tr><td><span class="k">{name}</span><br><span class="small">{WHAT.get(name, "")}</span></td>'
                f'<td>{" · ".join(cells)}</td></tr>')
assert total == 59, total
table = '<table><tr><th>Engine</th><th>Sounds (in PRESETS order)</th></tr>' + "".join(rows) + "</table>"
src = (HERE / "guide.html").read_text().replace("{{SOUNDS}}", table)
tmp = HERE / ".guide.build.html"
tmp.write_text(src)
chrome = shutil.which("google-chrome-stable") or shutil.which("google-chrome") or shutil.which("chromium")
out = Path(sys.argv[1] if len(sys.argv) > 1 else HERE / "GHOULBOX-User-Guide.pdf").resolve()
subprocess.run([chrome, "--headless=new", "--disable-gpu", "--no-pdf-header-footer", "--allow-file-access-from-files",
                f"--print-to-pdf={out}", tmp.as_uri()], check=True, capture_output=True)
tmp.unlink()
print(f"guide: {total} sounds -> {out}")
