#!/usr/bin/env python3
"""EXAMPLE asset step. Turns a title still and a title track into Dreamcast formats in build/disc.

This file is an example, not a finished reader. The part that gets files out of YOUR source (disc image, archive,
package, container) is yours to write, one per source format. Its only job: leave two files in SOURCE_EXTRACT:
  title.png   a 1024 x 512 still (any PNG the source gives you, scaled by the reader if needed)
  title.wav   the title track, stereo, any rate (converted to 48 kHz here)
Nothing from the source is committed; SOURCE_EXTRACT is outside the repo.

Outputs (what src/main.c opens):
  TITLE.DT   1024 x 512 VQ RGB565 still, made by the KOS pvrtex tool
  TITLE.ADP  the title track as AICA ADPCM, stereo interleaved, headerless, 48 kHz, made by KOS wav2adpcm
"""
import os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cfg as C

cfg = C.load()
EXTRACT = C.need(cfg, "SOURCE_EXTRACT")
KOS = C.need(cfg, "KOS_BASE")
OUT = os.path.join(C.ROOT, "build", "disc")
TMP = os.path.join(C.ROOT, "build", "tmp")
LOG = os.path.join(C.ROOT, "build", "assets.log")
TITLE_PNG = os.path.join(EXTRACT, "title.png")
TITLE_WAV = os.path.join(EXTRACT, "title.wav")
RATE = 48000
os.makedirs(OUT, exist_ok=True); os.makedirs(TMP, exist_ok=True)

def run(args):
    with open(LOG, "ab") as log:
        r = subprocess.run(args, stdout=log, stderr=log, creationflags=0x08000000 if os.name == "nt" else 0)
    if r.returncode:
        raise SystemExit("step failed (%d): %s (see build/assets.log)" % (r.returncode, args[0]))

def need_file(p):
    if not os.path.exists(p):
        raise SystemExit("missing %s: your source reader must write it (see the docstring)" % p)
    return p

def title_music():
    pcm = os.path.join(TMP, "title_pcm.wav")
    run(["ffmpeg", "-hide_banner", "-y", "-i", need_file(TITLE_WAV), "-ar", str(RATE), "-ac", "2", "-c:a", "pcm_s16le", pcm])
    run([os.path.join(KOS, "utils", "wav2adpcm", "wav2adpcm.exe"), "-n", "-i", "-t", pcm, os.path.join(OUT, "TITLE.ADP")])

def title_still():
    run([os.path.join(KOS, "utils", "pvrtex", "pvrtex.exe"), "-i", need_file(TITLE_PNG), "-o", os.path.join(OUT, "TITLE.DT"),
         "-f", "rgb565", "-c", "256", "-d"])

if __name__ == "__main__":
    title_music()
    title_still()
    open(os.path.join(OUT, "TITLE.TXT"), "w").write("rate=%d\n" % RATE)
    print("assets ready:", ", ".join("%s %d KB" % (f, os.path.getsize(os.path.join(OUT, f)) // 1024) for f in sorted(os.listdir(OUT))))
