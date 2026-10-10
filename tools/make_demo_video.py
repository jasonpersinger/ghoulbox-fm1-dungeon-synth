#!/usr/bin/env python3
"""GHOULBOX demo video: build/host/demo_piece DIR/piece.wav DIR/frames.rgb, then  python3 tools/make_demo_video.py DIR
-> DIR/ghoulbox-demo.mp4 (1920x1080: the device screen, 4x, beside the section titles from piece.wav.cues)."""
import subprocess, os
import sys
D = sys.argv[1] if len(sys.argv) > 1 else "."
F = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "fonts") + "/"
os.chdir(D)
pir, vt = F + "PirataOne-Regular.ttf", F + "VT323-Regular.ttf"
cues = [l.rstrip("\n").split("\t") for l in open("piece.wav.cues")]
mood = {"I": "D PHRYGIAN  ·  54 BPM  ·  DARK RITUAL", "II": "D MINOR  ·  66 BPM  ·  CLASSIC DUNGEON",
        "III": "D DORIAN  ·  100 BPM  ·  TAVERN", "IV": "D MINOR  ·  60 BPM  ·  DUNGEON"}
names = {"I": "The Crypt", "II": "The Tower", "III": "The Tavern", "IV": "Return to the Crypt"}
def esc(s): return s.replace("\\", "\\\\").replace(":", "\\:").replace("'", "’")
f = ["[0:v]scale=960:960:flags=neighbor,pad=1920:1080:80:60:color=0x0d0b08[v0]"]
d = [f"drawtext=fontfile={pir}:text='GHOULBOX':fontsize=110:fontcolor=0xee8a3c:x=1110:y=90",
     f"drawtext=fontfile={vt}:text='DUNGEON-SYNTH FIRMWARE FOR THE M-VAVE FM-1':fontsize=34:fontcolor=0xa4967e:x=1114:y=230"]
for k in range(4):
    t0, title, sounds, bpm = cues[k]; t1 = cues[k+1][0]
    num = title.split()[0]
    en = f"enable='between(t,{t0},{t1})'"; al = f"alpha='min(1,(t-{t0})/1.2)'"
    d.append(f"drawtext=fontfile={vt}:text='{num}':fontsize=44:fontcolor=0xa4967e:x=1114:y=380:{en}:{al}")
    d.append(f"drawtext=fontfile={pir}:text='{esc(names[num])}':fontsize=84:fontcolor=0xd9584a:x=1110:y=430:{en}:{al}")
    d.append(f"drawtext=fontfile={vt}:text='{esc(mood[num])}':fontsize=36:fontcolor=0xe4d8bf:x=1114:y=560:{en}:{al}")
    for j, s in enumerate(sounds.split(" · ")):
        d.append(f"drawtext=fontfile={vt}:text='T{j+1}  {esc(s)}':fontsize=40:fontcolor=0xee8a3c:x=1114:y={650+j*56}:{en}:{al}")
d.append(f"drawtext=fontfile={vt}:text='github.com/jasonpersinger/ghoulbox-fm1-dungeon-synth':fontsize=26:fontcolor=0x6b6052:x=1114:y=995")
fc = f[0] + ";[v0]" + ",".join(d) + "[v];[1:a]loudnorm=I=-16:TP=-1.5:LRA=11,aresample=48000[a]"
subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", "240x240", "-r", "30", "-i", "frames.rgb",
       "-i", "piece.wav", "-filter_complex", fc, "-map", "[v]", "-map", "[a]", "-c:v", "libx264", "-crf", "20", "-preset", "medium",
       "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "192k", "-movflags", "+faststart", "-shortest", "ghoulbox-demo.mp4"], check=True)
