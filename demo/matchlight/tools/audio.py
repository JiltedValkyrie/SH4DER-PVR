#!/usr/bin/env python3
"""Match Light film: builds the sound files of the disc. Nothing it reads or writes is in the repository.

    python demo/matchlight/tools/audio.py [--manifest audio_public.json] [--assets DIR] [--library DIR] [--voice WAV]
                                          [--out DIR] [--fetch] [--pin] [--ffmpeg EXE]

What it writes into --out (default build/matchlight/disc, the folder the image step puts on the disc):

  sfx/<name>.wav   one file per sound effect of the manifest (the names are ml_sound_names in matchlight_scene.h),
                   converted with ffmpeg to mono AICA ADPCM (4 bits a sample), or 16-bit PCM for a sound marked
                   "codec": "pcm16" (the two loops: an ADPCM loop clicks at its seam on the AICA).
  fx.wav           the sounds marked "stream": true, laid into one 22,050 Hz 16-bit mono track of the film's length at
                   the times the tables ml_cues and ml_holds of matchlight_scene.h give them (a held sound is faded
                   out over ML_HOLD_FADE_S when its animation is done). The program plays it as a third disc stream.
  voice.wav        the narrator, from --voice (any file ffmpeg reads), as 22,050 Hz 16-bit mono. Left out when no
                   --voice is given: the film then plays without a voice.
  audio_used.txt   which source file each sound came from, and the bytes of every file written.

The manifest (JSON): "sources" lists downloads (url, file, sha256, author, licence, page, optional unpack folder);
"sounds" maps each sound name to {"path": file under --assets (or under --library when "root" is "library"),
"rate": Hz, "codec": "adpcm" | "pcm16", optional "start" and "dur" in seconds, optional "af": an ffmpeg audio filter,
optional "stream": true for a sound longer than an AICA sound effect may be}. Optional "bed": one sound laid under the
whole film as a loop, in fx.wav: {"path" (and "root") as a sound, "rms_db": its level in dB of full scale (rms),
"fade_in" and "fade_out": seconds at the film's start and end, "seam": seconds of crossfade where the loop joins}.
--fetch downloads the sources into <assets>/files, checks every sha256 and unpacks archives into <assets>/unpack/<name>.
A download is written under a ".part" name and takes its real name only when its sha256 is the manifest's; --pin
accepts a new source's sha256 as it is and prints it, to put into the manifest.
The film's song (music.wav) and the narrator's cut voice track are laid by tools/narration.py, not here.

The public manifest (audio_public.json) uses CC0 sources only. A private manifest may point at a sound library that
is licensed to one person: such a build is for that person's own console and must not be published.

Sound effects on the AICA must be 65,534 samples or shorter (KallistiOS snd_sfx); the tool refuses a longer one.
Needs Python 3.8 or later (standard library only) and ffmpeg 4.4 or later (made and checked with 8.0.1). Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.
"""
import argparse
import hashlib
import json
import math
import os
import re
import shutil
import struct
import subprocess
import sys
import urllib.request
import wave
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
DEMO = os.path.dirname(HERE)
ROOT = os.path.dirname(os.path.dirname(DEMO))

STREAM_RATE = 22050          # Hz: the rate of the disc streams (16-bit mono PCM)
SFX_MAX_SAMPLES = 65534      # the longest sound effect KallistiOS snd_sfx accepts, samples

def shot_times(scene_path):
    """{shot name: (start, end)} in seconds, read from the shot table of matchlight_scene.h."""
    with open(scene_path, encoding="utf-8") as f:
        text = f.read()
    shots = {m.group(1): (float(m.group(2)), float(m.group(3)))
             for m in re.finditer(r'\{\s*"(M\w+)",\s*([0-9.]+)f,\s*([0-9.]+)f,', text)}
    if not shots:
        raise SystemExit("audio: no shot table found in " + scene_path)
    return shots


def write_wav(path, samples_or_bytes, rate):
    """Writes a plain 44-byte-header WAV: 16-bit, mono. Takes a list of sample values or raw little-endian bytes."""
    data = samples_or_bytes if isinstance(samples_or_bytes, (bytes, bytearray)) else struct.pack("<%dh" % len(samples_or_bytes), *samples_or_bytes)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(data)
    return len(data)


def sha256_of(path):
    """sha256 of a file, hex."""
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def run_ffmpeg(ffmpeg, argv):
    """Runs ffmpeg quietly; stops the build with its message when it fails."""
    r = subprocess.run([ffmpeg, "-hide_banner", "-loglevel", "error", "-y"] + argv, capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit("audio: ffmpeg failed: " + r.stderr.strip())


def fetch(manifest, assets, pin):
    """Downloads every source of the manifest that is not there yet, checks its sha256, unpacks archives."""
    files = os.path.join(assets, "files")
    os.makedirs(files, exist_ok=True)
    for src in manifest.get("sources", []):
        path = os.path.join(files, src["file"])
        part = path + ".part"                              # a download in progress, or one whose sha256 was wrong
        fresh = not os.path.exists(path)
        if fresh:
            print("audio: fetch", src["url"])
            with urllib.request.urlopen(src["url"]) as r, open(part, "wb") as f:
                shutil.copyfileobj(r, f)
        got = sha256_of(part if fresh else path)
        if src.get("sha256") != got:
            if not pin:
                raise SystemExit("audio: %s has sha256 %s, the manifest says %s%s" % (
                    src["file"], got, src.get("sha256"), " (left as %s)" % part if fresh else ""))
            print("audio: pin %s sha256 %s" % (src["file"], got))
        if fresh:
            os.replace(part, path)                         # only a checked file gets the real name
        if src.get("unpack"):
            dest = os.path.join(assets, "unpack", src["unpack"])
            if os.path.isdir(dest):
                continue
            os.makedirs(dest)
            if path.lower().endswith(".zip"):
                with zipfile.ZipFile(path) as z:
                    z.extractall(dest)
            else:                                          # .7z: the standard library cannot; a tar that reads 7z can
                tool = shutil.which("7z") or shutil.which("7za") or shutil.which("bsdtar") or shutil.which("tar")
                argv = [tool, "x", "-o" + dest, path] if tool and os.path.basename(tool).lower().startswith("7z") else [tool, "-xf", path, "-C", dest]
                if not tool or subprocess.run(argv, capture_output=True).returncode != 0:
                    raise SystemExit("audio: cannot unpack %s (needs 7z or a tar that reads 7z)" % src["file"])


def fx_placements(scene_path):
    """Where each sound is heard in the film, read from ml_cues and ml_holds of matchlight_scene.h (rows of plain numbers):
    {sound name: [(film start s, volume 0..255, hold length s or None)]}. A hold is faded out after its length."""
    with open(scene_path, encoding="utf-8") as f:
        text = f.read()
    shots = shot_times(scene_path)
    num = r"([0-9.]+)f"
    places = {}
    for m in re.finditer(r"\{\s*ML_SHOT_(\w+),\s*%s,\s*ML_SND_(\w+),\s*(\d+),\s*\d+\s*\}" % num, text):          # ml_cues
        places.setdefault(m.group(3).lower(), []).append((shots[m.group(1).replace("B", "b")][0] + float(m.group(2)), int(m.group(4)), None))
    for m in re.finditer(r"\{\s*ML_SHOT_(\w+),\s*%s,\s*%s,\s*ML_SND_(\w+),\s*(\d+),\s*\d+\s*\}" % (num, num), text):   # ml_holds
        start = shots[m.group(1).replace("B", "b")][0] + float(m.group(2))
        places.setdefault(m.group(4).lower(), []).append((start, int(m.group(5)), float(m.group(3)) - float(m.group(2))))
    fade = re.search(r"#define\s+ML_HOLD_FADE_S\s+([0-9.]+)f", text)
    if not places or not fade:
        raise SystemExit("audio: no cue or hold table found in " + scene_path)
    return places, float(fade.group(1)), max(end for _, end in shots.values())


def lay_fx(clips, places, fade_s, film_s):
    """The long sound effects' stream: film_s seconds at STREAM_RATE with every clip (name -> list of 16-bit samples) added
    at each of its places, at volume / 255 of its own level; a held clip is cut at its hold length and faded over fade_s.
    Returns (samples, what was laid)."""
    track = [0.0] * int(round(film_s * STREAM_RATE))
    laid = []
    for name, clip in sorted(clips.items()):
        for start, vol, hold in places.get(name, []):
            n = len(clip) if hold is None else min(len(clip), int((hold + fade_s) * STREAM_RATE))
            f0 = None if hold is None else int(hold * STREAM_RATE)          # the sample at which the fade begins
            at = int(round(start * STREAM_RATE))
            gain = vol / 255.0
            for i in range(min(n, len(track) - at)):
                g = gain if f0 is None or i < f0 else gain * max(0.0, 1.0 - (i - f0) / (fade_s * STREAM_RATE))
                track[at + i] += clip[i] * g
            laid.append("fx.wav  %-8s at %6.2f s for %.2f s, volume %d%s" % (name, start, n / STREAM_RATE, vol,
                                                                           "" if hold is None else ", faded out over the last %.3f s" % fade_s))
    return [int(round(max(-32767.0, min(32767.0, v)))) for v in track], laid


def lay_bed(track, clip, rms_db, fade_in_s, fade_out_s, seam_s):
    """Adds the sound bed to the long sound effects' track (16-bit samples at STREAM_RATE, the film's length): clip is
    looped over the whole track, its end crossfaded into its start over seam_s so the join does not click, scaled so its
    rms is rms_db dB of full scale, faded in over fade_in_s and out over fade_out_s. Returns (samples, one report line)."""
    seam = max(1, min(int(seam_s * STREAM_RATE), len(clip) // 4))
    unit_n = len(clip) - seam                                  # the loop's length: the clip less the overlapped join
    unit = [clip[i] if i >= seam else clip[i] * (i / seam) + clip[unit_n + i] * (1.0 - i / seam) for i in range(unit_n)]
    rms = math.sqrt(sum(v * v for v in unit) / unit_n)
    if rms <= 0.0:
        raise SystemExit("audio: the sound bed is silent")
    gain = 32768.0 * 10.0 ** (rms_db / 20.0) / rms
    n, n_in, n_out = len(track), int(fade_in_s * STREAM_RATE), int(fade_out_s * STREAM_RATE)
    out = []
    for i in range(n):
        g = gain * min(1.0, i / n_in if n_in else 1.0, (n - 1 - i) / n_out if n_out else 1.0)
        out.append(int(round(max(-32767.0, min(32767.0, track[i] + unit[i % unit_n] * g)))))
    return out, "fx.wav  bed      a loop of %.2f s under the whole film at %.1f dB rms, in over %.1f s, out over %.1f s" % (
        unit_n / STREAM_RATE, rms_db, fade_in_s, fade_out_s)


def main():
    ap = argparse.ArgumentParser(description="Builds the Match Light film's sound files.")
    ap.add_argument("--manifest", default=os.path.join(DEMO, "audio_public.json"))
    ap.add_argument("--assets", default=os.path.join(DEMO, "assets"), help="where fetched sources live (not in the repository)")
    ap.add_argument("--library", default=None, help="root of a private sound library, for sounds with \"root\": \"library\"")
    ap.add_argument("--voice", default=None, help="the narrator's recording")
    ap.add_argument("--out", default=os.path.join(ROOT, "build", "matchlight", "disc"))
    ap.add_argument("--fetch", action="store_true", help="download and check the manifest's sources first")
    ap.add_argument("--pin", action="store_true", help="with --fetch: accept and print a source's sha256 instead of refusing a mismatch")
    ap.add_argument("--ffmpeg", default=shutil.which("ffmpeg") or "ffmpeg")
    a = ap.parse_args()

    with open(a.manifest, encoding="utf-8") as f:
        manifest = json.load(f)
    if a.fetch:
        fetch(manifest, a.assets, a.pin)
    os.makedirs(os.path.join(a.out, "sfx"), exist_ok=True)
    used = []
    aica = 0
    fx_clips = {}                                          # the sounds marked "stream": name -> samples at STREAM_RATE

    # the sound effects
    for name, snd in sorted(manifest["sounds"].items()):
        root = a.library if snd.get("root") == "library" else a.assets
        if not root:
            raise SystemExit("audio: sound %s needs --library" % name)
        src = os.path.join(root, snd["path"])
        if not os.path.exists(src):
            raise SystemExit("audio: sound %s: no file %s (run with --fetch?)" % (name, src))
        dst = os.path.join(a.out, "sfx", name + ".wav")
        argv = []
        if snd.get("stream"):                              # too long for an AICA sound effect: into fx.wav instead
            cut = (["-ss", str(snd["start"])] if "start" in snd else []) + (["-t", str(snd["dur"])] if "dur" in snd else [])
            raw = subprocess.run([a.ffmpeg, "-hide_banner", "-loglevel", "error"] + cut + ["-i", src, "-vn", "-ac", "1", "-ar", str(STREAM_RATE)] +
                                 (["-af", snd["af"]] if snd.get("af") else []) + ["-f", "s16le", "-"], capture_output=True)
            if raw.returncode != 0 or not raw.stdout:
                raise SystemExit("audio: cannot read sound %s from %s" % (name, src))
            fx_clips[name] = struct.unpack("<%dh" % (len(raw.stdout) // 2), raw.stdout)
            used.append("fx.wav  %-8s %.2f s  <- %s" % (name, len(fx_clips[name]) / STREAM_RATE, snd["path"]))
            continue
        if "start" in snd:
            argv += ["-ss", str(snd["start"])]
        if "dur" in snd:
            argv += ["-t", str(snd["dur"])]
        argv += ["-i", src, "-vn", "-map_metadata", "-1", "-bitexact", "-ac", "1", "-ar", str(snd["rate"])]
        if snd.get("af"):
            argv += ["-af", snd["af"]]
        pcm = snd.get("codec", "adpcm") == "pcm16"
        argv += ["-c:a", "pcm_s16le" if pcm else "adpcm_yamaha", dst]
        run_ffmpeg(a.ffmpeg, argv)
        # the length in samples, from a second decode to raw 16-bit (2 bytes a sample)
        raw = subprocess.run([a.ffmpeg, "-hide_banner", "-loglevel", "error", "-i", dst, "-f", "s16le", "-"], capture_output=True).stdout
        samples = len(raw) // 2
        if samples > SFX_MAX_SAMPLES:
            raise SystemExit("audio: sound %s is %d samples; the AICA sound effect limit is %d (shorten \"dur\" or lower \"rate\")" % (name, samples, SFX_MAX_SAMPLES))
        resident = samples * 2 if pcm else (samples + 1) // 2      # bytes it takes in sound RAM
        aica += resident
        used.append("sfx/%s.wav  %6d B in sound RAM  %5d Hz %-5s %.2f s  <- %s" % (name, resident, snd["rate"], "pcm16" if pcm else "adpcm",
                                                                                  samples / snd["rate"], snd["path"]))

    # the long sound effects' stream
    bed = manifest.get("bed")
    if fx_clips or bed:
        places, fade_s, film_s = fx_placements(os.path.join(DEMO, "matchlight_scene.h"))
        for name in fx_clips:
            if name not in places:
                raise SystemExit("audio: sound %s is marked \"stream\" but no cue or hold of matchlight_scene.h plays it" % name)
        track, laid = lay_fx(fx_clips, places, fade_s, film_s)
        if bed:
            root = a.library if bed.get("root") == "library" else a.assets
            if not root:
                raise SystemExit("audio: the sound bed needs --library")
            raw = subprocess.run([a.ffmpeg, "-hide_banner", "-loglevel", "error", "-i", os.path.join(root, bed["path"]), "-vn", "-ac", "1",
                                  "-ar", str(STREAM_RATE), "-f", "s16le", "-"], capture_output=True)
            if raw.returncode != 0 or len(raw.stdout) < 8:
                raise SystemExit("audio: cannot read the sound bed from %s" % bed["path"])
            track, line = lay_bed(track, struct.unpack("<%dh" % (len(raw.stdout) // 2), raw.stdout), bed["rms_db"], bed.get("fade_in", 0.0),
                                  bed.get("fade_out", 0.0), bed.get("seam", 0.25))
            laid.append(line)
        n = write_wav(os.path.join(a.out, "fx.wav"), track, STREAM_RATE)
        used += laid
        used.append("fx.wav  %d B on disc (streamed)" % (n + 44))
    else:
        used.append("fx.wav  NOT written: the manifest marks no sound \"stream\" and has no \"bed\"")

    # the voice, when given
    voice = os.path.join(a.out, "voice.wav")
    if a.voice:
        raw = subprocess.run([a.ffmpeg, "-hide_banner", "-loglevel", "error", "-i", a.voice, "-vn", "-ac", "1", "-ar", str(STREAM_RATE), "-f", "s16le", "-"],
                             capture_output=True)
        if raw.returncode != 0 or not raw.stdout:
            raise SystemExit("audio: cannot read the voice file " + a.voice)
        n = write_wav(voice, raw.stdout, STREAM_RATE)
        used.append("voice.wav  %d B on disc (streamed)  <- %s" % (n + 44, a.voice))
    elif os.path.exists(voice):
        used.append("voice.wav  kept from an earlier run (no --voice given)")
    else:
        used.append("voice.wav  NOT written: no --voice given, the film plays without a narrator")

    used.append("sound RAM for the effects: %d B (%.1f KB of the AICA's 2,048 KB)" % (aica, aica / 1024.0))
    with open(os.path.join(a.out, "audio_used.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(used) + "\n")
    print("\n".join(used))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, KeyError, ValueError, IndexError, struct.error) as e:     # a missing file, a malformed manifest or table: one line, no trace
        raise SystemExit("%s: %s: %s" % (os.path.basename(__file__), type(e).__name__, e))
