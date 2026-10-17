#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# A GHOULBOX ambience video: tests/ambient_render.c plays theme N for SECONDS (its screen piped straight into a small
# video, never to disk raw), the mix faded in and out and brought to -16 LUFS, the frame from tools/gen_ambient_art.py
# with the live screen at 2x in its bezel.
#   make_ambient_video.py N SECONDS OUTDIR   -> OUTDIR/ghoulbox-ambient-N.mp4 (build/host/ambient_render built first)
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import gen_ambient_art as art  # noqa: E402

FADE_IN, FADE_OUT, LUFS = 4, 12, -16


def loudness(wav):
    """the mix's integrated loudness and true peak (ffmpeg loudnorm's first pass)"""
    r = subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-i", wav, "-af", f"loudnorm=I={LUFS}:print_format=json",
                        "-f", "null", "-"], capture_output=True, text=True, check=True)
    m = json.loads(re.findall(r"\{[^{}]*\}", r.stderr)[-1])
    return float(m["input_i"]), float(m["input_tp"])


def main():
    n, sec, out = int(sys.argv[1]), float(sys.argv[2]), Path(sys.argv[3])
    out.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=out) as tmp:
        wav, screen, bg = f"{tmp}/mix.wav", f"{tmp}/screen.mkv", f"{tmp}/bg.png"
        enc = subprocess.Popen(["ffmpeg", "-v", "error", "-y", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", "240x240",
                                "-r", "30", "-i", "-", "-c:v", "libx264", "-crf", "8", "-preset", "veryfast", screen],
                               stdin=subprocess.PIPE)
        subprocess.run([str(ROOT / "build/host/ambient_render"), str(n), str(sec), wav, "-"], stdout=enc.stdin,
                       check=True)
        enc.stdin.close()
        if enc.wait():
            sys.exit("ffmpeg (screen) failed")
        art.compose(n, None).save(bg)
        i, tp = loudness(wav)
        gain = min(LUFS - i, -1.0 - tp)                  # (never past -1 dBTP)
        x, y = art.SCREEN_AT
        dur = sec + 0.5                                  # (the renderer's half second before the first dream)
        mp4 = out / f"ghoulbox-ambient-{n}.mp4"
        subprocess.run(["ffmpeg", "-v", "error", "-stats", "-y", "-loop", "1", "-framerate", "30", "-i", bg,
                        "-i", screen, "-i", wav,
                        "-filter_complex", f"[1:v]scale=480:480:flags=neighbor[s];[0:v][s]overlay={x}:{y}:shortest=1,"
                                           f"format=yuv420p[v];[2:a]volume={gain:.2f}dB,afade=t=in:d={FADE_IN},"
                                           f"afade=t=out:st={dur - FADE_OUT:.2f}:d={FADE_OUT}[a]",
                        "-map", "[v]", "-map", "[a]", "-c:v", "libx264", "-preset", "medium", "-crf", "20",
                        "-tune", "animation", "-c:a", "aac", "-b:a", "256k", "-movflags", "+faststart", "-t", f"{dur:.2f}",
                        str(mp4)], check=True)
        print(f"{mp4}: {dur / 60:.1f} min, mix {i:.1f} LUFS -> {i + gain:.1f}, peak {tp:.1f} -> {tp + gain:.1f} dBTP")


if __name__ == "__main__":
    main()
