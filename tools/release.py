#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""GHOULBOX: prepare a release, everything short of publishing it (docs/RELEASING.md).

  tools/release.py X.Y            from a clean checkout of origin/ghoulbox with GHOULBOX_VERSION "X.Y" (core.h)
  tools/release.py X.Y --dry-run  the same from any commit (an old tag, a branch): nothing is checked about git

Checks git (clean, at origin/ghoulbox, the tag ghoulbox-vX.Y not taken) and the version, then:
  ./build.sh                          the image within the slot and its spare (tools/build.py)
  tests/run_tests.sh                  every test, the device ones included (HOST_ONLY=1 is what CI runs)
  node web/test_web.mjs, test_backup.mjs
  web/emu/build.sh (EMU_PREVIEW=0)    the emulator, checked against the native build (emu_test.mjs)
  web/make_site.py, make_sound_page   the site, its package compared with the release's
into build/release-gbX.Y/: ghoulbox-X.Y.fwsc (the name the FM-1 Firmware Hub looks for), SHA256SUMS, site/.
Then prints the steps that publish it: the signed tag, the GitHub release, the site; they need the owner's key.
EMSDK: Emscripten (default ~/emsdk).
"""
import argparse
import hashlib
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REPO = "jasonpersinger/ghoulbox-fm1-dungeon-synth"
BRANCH = "ghoulbox"


def say(msg):
    print(f"release: {msg}", flush=True)


def die(msg):
    raise SystemExit(f"release: {msg}")


def run(cmd, env=None, quiet=False):
    """cmd (a list, or a string for bash) in the repo root; its output shown unless quiet (then only on failure)"""
    shell = isinstance(cmd, str)
    say("$ " + (cmd if shell else " ".join(map(str, cmd))))
    r = subprocess.run(["bash", "-c", cmd] if shell else cmd, cwd=ROOT, env={**os.environ, **(env or {})},
                       stdout=subprocess.PIPE if quiet else None, stderr=subprocess.STDOUT if quiet else None,
                       text=True)
    if r.returncode:
        if quiet:
            print(r.stdout[-4000:])
        die(f"failed ({r.returncode})")
    return r


def git(*a):
    return subprocess.run(["git", *a], cwd=ROOT, check=True, capture_output=True, text=True).stdout.strip()


def check_git(ver):
    if git("status", "--porcelain", "--untracked-files=no"):
        die("uncommitted changes (git status)")
    git("fetch", "-q", "origin", BRANCH)
    head, tip = git("rev-parse", "HEAD"), git("rev-parse", f"origin/{BRANCH}")
    if head != tip:
        die(f"HEAD {head[:7]} is not origin/{BRANCH} {tip[:7]}: release what is merged (or --dry-run)")
    if git("ls-remote", "--tags", "origin", f"refs/tags/ghoulbox-v{ver}"):
        die(f"the tag ghoulbox-v{ver} is taken on origin")
    return head


def main():
    ap = argparse.ArgumentParser(description="prepare a GHOULBOX release (docs/RELEASING.md)")
    ap.add_argument("version", help="X.Y, as GHOULBOX_VERSION in firmware/src/core.h")
    ap.add_argument("--dry-run", action="store_true", help="any commit; skip the git checks and the publish steps")
    a = ap.parse_args()
    ver = a.version
    if not re.fullmatch(r"\d+\.\d+(\.\d+)?", ver):
        die(f"{ver}: use X.Y")
    core = (ROOT / "firmware/src/core.h").read_text()
    m = re.search(r'#define GHOULBOX_VERSION "([^"]*)"', core)
    if not m or m.group(1) != ver:
        die(f"GHOULBOX_VERSION is {m.group(1) if m else '?'!r}, not {ver!r} (firmware/src/core.h)")
    head = git("rev-parse", "HEAD") if a.dry_run else check_git(ver)
    say(f"GHOULBOX {ver} from {head[:7]}" + (" (dry run)" if a.dry_run else ""))
    for tool in ("node", "ffmpeg", "lame", "cc"):
        if not shutil.which(tool):
            die(f"no {tool} on PATH")
    emsdk = Path(os.environ.get("EMSDK", Path.home() / "emsdk"))
    if not (emsdk / "emsdk_env.sh").is_file():
        die(f"no Emscripten in {emsdk} (EMSDK)")

    run(["./build.sh"])
    pkg = ROOT / "build/felucca.fwsc"
    if f" GB{ver}\0".encode() not in (ROOT / "build/felucca.bin").read_bytes():    # editor.c's identity string
        die(f"build/felucca.bin does not carry GB{ver} (the package is compressed: the image is checked)")
    run(["sh", "tests/run_tests.sh"], env={"AC79_SDK": os.environ.get("AC79_SDK", str(Path.home() / "fw-AC79_AIoT_SDK"))},
        quiet=True)
    say("tests ok")
    run(["node", "web/test_web.mjs"], quiet=True)
    run(["node", "web/test_backup.mjs"], quiet=True)
    run(f'source "{emsdk}/emsdk_env.sh" >/dev/null 2>&1 && EMU_PREVIEW=0 sh web/emu/build.sh', quiet=True)
    run("cc -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o build/emu/native_check web/emu/native_check.c -lm"
        " && build/emu/native_check build/emu/native.f32", quiet=True)
    run(["node", "web/emu/emu_test.mjs", "build/emu/felucca.wasm", "build/emu/native.f32"], quiet=True)
    if f"GHOULBOX {ver}" not in (ROOT / "build/emu/index.html").read_text():
        die(f"build/emu/index.html does not show GHOULBOX {ver}")
    say("emulator ok")

    out = ROOT / f"build/release-gb{ver}"
    shutil.rmtree(out, ignore_errors=True)
    out.mkdir(parents=True)
    rel = out / f"ghoulbox-{ver}.fwsc"
    shutil.copyfile(pkg, rel)
    site = out / "site"
    run([sys.executable, "web/make_site.py", rel, f"gb{ver}", site], quiet=True)
    run([sys.executable, "tools/make_sound_page.py", site], quiet=True)
    if (site / f"firmware/felucca-gb{ver}.fwsc").read_bytes() != rel.read_bytes():
        die("the site's package is not the release's")
    sha = hashlib.sha256(rel.read_bytes()).hexdigest()
    (out / "SHA256SUMS").write_text(f"{sha}  {rel.name}\n")
    say(f"{rel.relative_to(ROOT)}  {rel.stat().st_size} B  sha256 {sha}")
    say(f"site in {site.relative_to(ROOT)}")
    if a.dry_run:
        return
    print(f"""
Next (docs/RELEASING.md), after the FM-1 smoke test with {rel.relative_to(ROOT)}:
  git tag -s ghoulbox-v{ver} {head[:7]} -m "GHOULBOX {ver}: ..."   && git push origin ghoulbox-v{ver}
  gh release create ghoulbox-v{ver} {rel.relative_to(ROOT)} --repo {REPO} --title "GHOULBOX {ver}" --notes-file NOTES.md --latest
  git fetch origin gh-pages && git worktree add build/ghpages origin/gh-pages && cd build/ghpages && git switch -c gh-pages-pub
  rsync -a --exclude .git --exclude .github --exclude .nojekyll ../release-gb{ver}/site/ ./ && git add -A && git status --short
  git commit -m "Site: GHOULBOX {ver} (ghoulbox {head[:7]})" && git push origin gh-pages-pub:gh-pages
""")


if __name__ == "__main__":
    main()
