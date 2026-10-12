#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Make the site (GitHub Pages):

  CNAME                       GHOULBOX: the site's domain (GitHub Pages' custom domain; a publish must keep it)
  index.html                  redirect to the emulator (GHOULBOX; the installer when there is none)
  firmware/felucca-VER.fwsc   the package (+ LICENSE, LICENSING.md, LICENSES/: the package holds
                              JieLi SDK files under Apache-2.0, see LICENSING.md)
  webapp/installer/index.html index_pkg.html, self-contained (fm1pkg.js, fm1ota.js, metadata inlined)
  webapp/editor/index.html    editor.html (+ fukiai.ttf, FUKIAI-LICENSE.txt)
  webapp/art/                 GHOULBOX: the dungeon's pixel art the installer shows (tools/gen_emu_art.py, from build/gen)
  firmwares/                  GHOULBOX: the FM-1 firmwares directory (web/firmwares: index.html, firmwares.json, the
                              Firmware Hub catalog snapshot, audio/ for the samples their authors send)
  webapp/emu/                 GHOULBOX: the browser emulator, build/emu as web/emu/build.sh made it (when it is there)
  src/                        not touched

  web/make_site.py build/felucca-X.Y.fwsc X.Y OUT_DIR

The package must be one made by tools/fm1pkg_make.py (Felucca's own loader, no vendor files).
Its identity (FM-1_9xx) is read from the package; the device must report it after
the install.
"""
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
DOMAIN = "ghoulbox.messy.quest"                     # GHOULBOX: GitHub Pages' custom domain (the CNAME file)
BLOCKS, BLK, KEEP = 20, 0x30, 0x2F


def strip_module(src):
    src = re.sub(r"^export\s+", "", src, flags=re.M)
    return re.sub(r"^import .*?;\n", "", src, flags=re.M)


def product_of(raw):
    """the package identity: one marker byte after each of the first 20 blocks (fm1pkg.js productOf)"""
    return "".join(chr((m - i - 1) & 0xFF) for i in range(BLOCKS) if (m := raw[i * BLK + KEEP]) != 0x7D)


def main(pkg, version, out):
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_9\d\d", product):
        raise SystemExit(f"{pkg}: identity {product!r} is not a Felucca package (FM-1_9xx)")
    if b"FELUCCA-LOADER-1" not in raw:              # Felucca loader marker: never publish a package with vendor files
        raise SystemExit(f"{pkg}: no Felucca loader in it; the site ships only fm1pkg_make.py packages "
                         "(a package patched from an official one carries vendor files)")
    html = (HERE / "index_pkg.html").read_text(encoding="utf-8")
    lib = strip_module((HERE / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1ota.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1backup.js").read_text(encoding="utf-8"))
    name = f"felucca-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
    meta = json.dumps({"version": version, "product": product, "pkg": "../../firmware/" + name})
    for mark in ("/*LIB*/", "/*META*/"):
        if html.count(mark) != 1:
            raise SystemExit(f"index_pkg.html must contain {mark} once; update make_site.py")
    html = html.replace("/*LIB*/", lib).replace("/*META*/", meta)
    inst, ed, fw = out / "webapp" / "installer", out / "webapp" / "editor", out / "firmware"
    for d in (inst, ed, fw):
        d.mkdir(parents=True, exist_ok=True)
    for old in fw.glob("felucca-*.fwsc"):          # one package: the current one
        old.unlink()
    (inst / "index.html").write_text(html, encoding="utf-8")
    shutil.copy(pkg, fw / name)
    lic = HERE.parent / "LICENSES"                  # the package holds JieLi SDK files (Apache-2.0): their
    (fw / "LICENSES").mkdir(exist_ok=True)          # licence travels next to it, with Felucca's own
    names = sorted(f.name for f in lic.glob("*.txt"))
    for n in names:
        shutil.copy(lic / n, fw / "LICENSES" / n)
    (fw / "LICENSES" / "index.html").write_text(    # the installer links this folder: Pages lists no folders
        '<!doctype html><meta charset="utf-8"><title>Felucca licences</title><h1>Licence texts</h1><ul>'
        + "".join(f'<li><a href="{n}">{n}</a></li>' for n in names)
        + '</ul><p><a href="../LICENSING.md">LICENSING.md</a> · <a href="../LICENSE">LICENSE (GPL-3.0)</a></p>\n',
        encoding="utf-8")
    for doc in ("LICENSE", "LICENSING.md"):
        shutil.copy(HERE.parent / doc, fw / doc)
    shutil.copy(HERE / "editor.html", ed / "index.html")
    root = HERE.parent                              # GHOULBOX: the art, and the emulator when it was built
    subprocess.run([sys.executable, str(root / "tools/gen_emu_art.py"), str(root / "build/gen"), str(out / "webapp/art")],
                   check=True)
    fw_dir = out / "firmwares"                      # GHOULBOX: the FM-1 firmwares directory
    shutil.rmtree(fw_dir, ignore_errors=True)
    shutil.copytree(HERE / "firmwares", fw_dir, ignore=shutil.ignore_patterns("audio-README.txt"))
    (fw_dir / "audio").mkdir(exist_ok=True)
    shutil.copy(HERE / "firmwares" / "audio-README.txt", fw_dir / "audio" / "README.txt")
    emu = root / "build/emu"
    if (emu / "felucca.wasm").exists():
        shutil.rmtree(out / "webapp/emu", ignore_errors=True)
        shutil.copytree(emu, out / "webapp/emu", ignore=shutil.ignore_patterns("native*"))
    else:
        print("site: no build/emu (web/emu/build.sh): webapp/emu left as it was")
    for f in ("fukiai.ttf", "FUKIAI-LICENSE.txt", "fm1backup.js"):
        if (HERE / f).exists():
            shutil.copy(HERE / f, ed / f)
    (out / "CNAME").write_text(DOMAIN, encoding="utf-8")   # (as GitHub writes it: no newline)
    home = "webapp/emu/" if (out / "webapp/emu/index.html").exists() else "webapp/installer/"   # GHOULBOX: the emulator
    (out / "index.html").write_text(                                                          # first, when it is there
        '<!doctype html><meta charset="utf-8"><title>GHOULBOX</title>'
        f'<meta http-equiv="refresh" content="0; url={home}">'
        f'<a href="{home}">GHOULBOX</a>\n', encoding="utf-8")
    print(f"site: {out}: webapp/installer ({len(html)} B), webapp/editor, firmware/{name} ({len(raw)} B, {product})")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4])
