#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""GHOULBOX: a CC0 set whose folder is missing stops tools/gen_samples.py, naming it. Without that, the sets after it
would move down a place and stored SET values would play another instrument (1.1's review).
  python3 tests/gen_samples_missing_test.py"""
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as t:
    t = Path(t)
    (t / "tools").mkdir()
    for f in ("gen_samples.py", "sampleio.py", "gen_waves.py"):
        shutil.copy(ROOT / "tools" / f, t / "tools" / f)
    shutil.copytree(ROOT / "assets" / "samples-cc0", t / "assets" / "samples-cc0",
                    ignore=lambda d, names: ["RENORGAN"] if d.endswith("samples-cc0") else [])
    r = subprocess.run([sys.executable, str(t / "tools" / "gen_samples.py"), str(t / "out.h")],
                       capture_output=True, text=True)
    ok = r.returncode != 0 and "RENORGAN" in (r.stdout + r.stderr) and not (t / "out.h").exists()
    print(f"gen_samples: a missing CC0 set (RENORGAN) stops the build, named {'ok' if ok else 'FAIL'}")
    if not ok:
        print((r.stdout + r.stderr)[-600:])
    sys.exit(0 if ok else 1)
