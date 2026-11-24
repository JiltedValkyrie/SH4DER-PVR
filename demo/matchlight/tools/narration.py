#!/usr/bin/env python3
"""Match Light film: builds the narrator's voice track and the typewriter subtitles of the disc.

    python demo/matchlight/tools/narration.py --font Aldrich-Regular.ttf [--font-licence OFL.txt]
                                              [--edit voice_edit.json --takes DIR] [--music SONG] [--out DIR]
                                              [--ffmpeg EXE]

What it writes into --out (default build/matchlight/disc):

  font.bin    the subtitle type: the printable ASCII glyphs of --font at 16 pixels, as one 256 x 128 ARGB4444 picture
              (white, the glyph's coverage in the alpha) and a table of where each glyph lies in it.
              Layout: "SFN1", u16 width, u16 height, u16 glyphs, u16 0; per glyph u8 x, y, w, h; then the picture's
              rows, two bytes a texel, little-endian.
  subs.bin    the subtitles: every sentence of narration.json, wrapped to at most two centred lines on the lower
              letterbox bar, with the moment each letter appears (a typewriter that keeps step with the voice).
              Layout: "SUB1", u16 sentences, u16 0; per sentence u32 start (ms of film time), u32 end, u16 glyphs,
              u16 0; per glyph u16 x, u16 y (screen pixels of its top left), u16 delay (ms after the sentence's
              start), u8 glyph number (index into font.bin's table), u8 0.
  voice.wav   only with --edit: the narrator's track for the whole film, 22,050 Hz 16-bit mono. The edit list names,
              per shot's line, a recording under --takes, the start and end of the line in it (seconds) and the time
              of every word inside that clip. Each clip is levelled, faded at both ends and laid into its shot.

  music.wav   only with --music: the song under the whole film, 22,050 Hz 16-bit mono, the same stream format as the
              voice. It is laid here, not in audio.py, because its level follows the voice: MUSIC_DB under the
              voice's level between lines, MUSIC_SPEAK_DB while a line is spoken, and MUSIC_GAP_DB in the pauses
              of at least MUSIC_GAP_MIN_S inside the lines of MUSIC_GAP_SHOTS. It fades in over MUSIC_IN_S and
              out over the film's last MUSIC_OUT_S, then plays on under the end credits at MUSIC_CREDITS_DB
              (the credits' length is read from matchlight_scene.h) and fades to silence with the last card. A song shorter than the film is repeated with a crossfade of
              MUSIC_XFADE_S.

When does a line start? LEAD_S after its shot begins (after the fade up, or the beat that follows a strike flash), and
it must end END_GAP_S before the shot's cut: the tool stops if a line does not fit. Shot times are read from
matchlight_scene.h, so a re-timed film re-times the voice and the subtitles.
Without --edit there is no voice.wav and the subtitles are typed at DEFAULT_WPS words a second (faster where a line would not fit its shot).

The font is a build input, not part of the repository (see audio_public.json "sources" and CREDITS.md). font.bin does
not carry the font's name: an OFL font's reserved name must not be used for a changed copy.
Needs Python 3.8 or later with Pillow 9.2 or later (made and checked with 12.3), and ffmpeg 4.4 or later for the voice and the song.
Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.
"""
import argparse
import array
import json
import math
import os
import re
import shutil
import struct
import subprocess
import sys
import wave

HERE = os.path.dirname(os.path.abspath(__file__))
DEMO = os.path.dirname(HERE)
ROOT = os.path.dirname(os.path.dirname(DEMO))

RATE = 22050              # Hz of the voice track (the stream rate the program plays)
FONT_PX = 16              # subtitle type size, pixels
ATLAS_W, ATLAS_H = 256, 128   # the glyph picture, texels (a 64 KB ARGB4444 texture)
SCREEN_W = 640            # frame width, pixels
SUB_MAX_W = 560           # widest subtitle line, pixels (inside the safe area of a television)
SUB_Y_ONE = 432           # top of a single subtitle line, pixels: the middle of the lower letterbox bar (rows 420 .. 479)
SUB_Y_TWO = (423, 441)    # tops of the two lines of a wrapped sentence
SUB_HOLD_S = 1.5          # a finished sentence stays this long, seconds, unless the next one starts first
LEAD_S = 1.0              # a line starts this long after its shot begins, seconds
LEAD_BY_SHOT = {"M1": 1.5, "M7": 1.3}   # M1 fades up for a second first; M7's star crosses first
END_GAP_S = 1.0           # a line ends at least this long before its shot's cut, seconds
DEFAULT_WPS = 2.1         # words a second when there is no recording
SENTENCE_GAP_S = 0.6      # pause between sentences when there is no recording, seconds
TYPE_SHARE = 0.8          # a word's letters are typed over this share of the time the word is spoken
LEVEL_DB = -20.0          # each clip's loudness after levelling: rms of its sounding parts, dB below full scale
PEAK_DB = -1.5            # and no sample louder than this
FADE_IN_S, FADE_OUT_S = 0.01, 0.08   # the fades at a clip's ends, seconds
SOUND_RMS = 12.0          # a 10 ms stretch under this rms (of 32,768) is silence and is left out of the loudness measure
MUSIC_DB = -20.0          # the song's level between the narrator's lines: dB under the voice's level (LEVEL_DB)
MUSIC_SPEAK_DB = -23.0    # and while a line is being spoken
MUSIC_GAP_DB = -14.0      # and in the long pauses inside the lines of MUSIC_GAP_SHOTS, where the song comes forward
MUSIC_GAP_SHOTS = ("M9", "M10")
MUSIC_GAP_MIN_S = 0.8     # a pause between two words of such a line counts when it is at least this long, seconds
MUSIC_IN_S = 3.0          # the song fades in over the film's first this many seconds
MUSIC_OUT_S = 5.0         # and out over its last this many
MUSIC_CREDITS_DB = -14.0  # the song's level under the end credits, dB under the voice's level: it has the cards alone
MUSIC_CREDITS_IN_S = 0.5  # it comes up over the first card's first this many seconds
MUSIC_CREDITS_OUT_S = 2.0 # and fades to silence over the credits' last this many
MUSIC_XFADE_S = 2.0       # overlap when a song shorter than the film repeats, seconds
MUSIC_SLEW_S = 0.25       # the level moves to a new value with this time constant, seconds (no steps, no pumping)
MAX_SENTENCES = 48        # the most sentences the program reads from subs.bin (SUB_SENTENCES_MAX in matchlight.c)
MAX_SUBS_BYTES = 12288    # subs.bin must be SMALLER than this (SUBS_MAX_BYTES in matchlight.c)
MAX_GLYPHS = 64           # the most letters one sentence may have (the program's limit for one frame)


def credits_seconds(scene_path):
    """Length of the end credits in seconds, worked out from the ML_CREDIT_* constants of matchlight_scene.h."""
    with open(scene_path, encoding="utf-8") as f:
        text = f.read()
    v = {}
    for name in ("CARDS", "HOLD_S", "DISSOLVE_S", "OUT_S"):
        m = re.search(r"#define\s+ML_CREDIT_%s\s+([0-9.]+)" % name, text)
        if not m:
            raise SystemExit("narration: ML_CREDIT_%s not found in %s" % (name, scene_path))
        v[name] = float(m.group(1))
    return v["CARDS"] * v["HOLD_S"] + (v["CARDS"] - 1.0) * v["DISSOLVE_S"] + v["OUT_S"]


def shot_table(scene_path):
    """{shot name: (start, end)} in seconds, read from the shot table of matchlight_scene.h."""
    with open(scene_path, encoding="utf-8") as f:
        text = f.read()
    shots = {m.group(1): (float(m.group(2)), float(m.group(3)))
             for m in re.finditer(r'\{\s*"(M\w+)",\s*([0-9.]+)f,\s*([0-9.]+)f,', text)}
    if not shots:
        raise SystemExit("narration: no shot table found in " + scene_path)
    return shots


def build_font(ttf):
    """Rasterises ASCII 33 .. 126 of the font. Returns (glyphs, advance, atlas bytes): glyphs maps a character to
    (index, x, y, w, h, xoff, yoff) with the offsets from the pen position at the line's top; advance maps a character
    (the space too) to its pen advance in pixels."""
    from PIL import ImageFont
    font = ImageFont.truetype(ttf, FONT_PX)
    atlas = [0] * (ATLAS_W * ATLAS_H)
    glyphs, advance = {}, {" ": font.getlength(" ")}
    pen_x, pen_y, row_h = 1, 1, 0
    for code in range(33, 127):
        ch = chr(code)
        advance[ch] = font.getlength(ch)
        mask = font.getmask(ch)
        box = mask.getbbox()
        if box is None:
            continue
        x0, y0, x1, y1 = box
        w, h = x1 - x0, y1 - y0
        left, top, _, _ = font.getbbox(ch)                 # where the ink starts, from the pen at the line's top
        if pen_x + w + 1 > ATLAS_W:
            pen_x, pen_y, row_h = 1, pen_y + row_h + 1, 0
        if pen_y + h + 1 > ATLAS_H:
            raise SystemExit("narration: the glyphs do not fit the %d x %d picture" % (ATLAS_W, ATLAS_H))
        for y in range(h):
            for x in range(w):
                cover = mask.getpixel((x0 + x, y0 + y)) >> 4          # 0 .. 15
                atlas[(pen_y + y) * ATLAS_W + pen_x + x] = (cover << 12) | 0x0FFF   # ARGB4444: white, coverage as alpha
        glyphs[ch] = (len(glyphs), pen_x, pen_y, w, h, left, top)
        pen_x += w + 1                                     # a clear texel between neighbours
        row_h = max(row_h, h)
    return glyphs, advance, struct.pack("<%dH" % len(atlas), *atlas)


def line_width(text, advance):
    """Pen width of a line of text, pixels."""
    return sum(advance.get(c, advance[" "]) for c in text)


def wrap(words, advance):
    """Splits a sentence's words into one line, or two lines of nearly equal width, each within SUB_MAX_W."""
    whole = " ".join(words)
    if line_width(whole, advance) <= SUB_MAX_W:
        return [words]
    best = None
    for k in range(1, len(words)):
        a, b = line_width(" ".join(words[:k]), advance), line_width(" ".join(words[k:]), advance)
        if a <= SUB_MAX_W and b <= SUB_MAX_W and (best is None or abs(a - b) < best[0]):
            best = (abs(a - b), k)
    if best is None:
        raise SystemExit("narration: a sentence does not fit two subtitle lines: " + whole)
    return [words[:best[1]], words[best[1]:]]


def level_clip(samples):
    """Brings a clip (list of ints) to LEVEL_DB rms over its sounding parts, never past PEAK_DB, and fades its ends."""
    hop = RATE // 100
    loud = [sum(x * x for x in samples[i:i + hop]) / hop for i in range(0, max(1, len(samples) - hop), hop)]
    loud = [v for v in loud if v > SOUND_RMS * SOUND_RMS]
    if not loud:
        return samples
    rms = math.sqrt(sum(loud) / len(loud))
    gain = 32768.0 * 10.0 ** (LEVEL_DB / 20.0) / rms
    peak = max(abs(x) for x in samples)
    gain = min(gain, 32768.0 * 10.0 ** (PEAK_DB / 20.0) / max(1, peak))
    n_in, n_out = int(FADE_IN_S * RATE), int(FADE_OUT_S * RATE)
    out = []
    for i, x in enumerate(samples):
        g = gain * min(1.0, i / max(1, n_in)) * min(1.0, (len(samples) - 1 - i) / max(1, n_out))
        out.append(int(round(max(-32767.0, min(32767.0, x * g)))))
    return out


def rms_sounding(samples):
    """The rms of the sounding 10 ms stretches of a list of 16-bit samples (silence is left out), of 32,768."""
    hop = RATE // 100
    loud = [sum(x * x for x in samples[i:i + hop]) / hop for i in range(0, max(1, len(samples) - hop), hop)]
    loud = [v for v in loud if v > SOUND_RMS * SOUND_RMS]
    return math.sqrt(sum(loud) / len(loud)) if loud else 0.0


def lay_music(song, film_s, credits_s, spoken, gaps):
    """The song under the whole film and the end credits after it. song: its samples at RATE (ints). spoken:
    (start s, end s) of every narrated line; gaps: (start s, end s) of the pauses where the song comes forward.
    Returns film_s + credits_s seconds of samples."""
    n = int(round((film_s + credits_s) * RATE))
    xf = int(MUSIC_XFADE_S * RATE)
    if len(song) <= 2 * xf:
        raise SystemExit("narration: the song is shorter than two crossfades")
    bed = [float(x) for x in song[:n]]
    while len(bed) < n:                                    # repeat: the tail of the bed crosses into the song's start
        for i in range(xf):
            k = i / float(xf)
            bed[len(bed) - xf + i] = bed[len(bed) - xf + i] * (1.0 - k) + song[i] * k
        bed.extend(float(x) for x in song[xf:xf + n - len(bed)])
    rms = rms_sounding([int(x) for x in bed])
    if rms <= 0.0:
        raise SystemExit("narration: the song is silent")
    unit = 32768.0 * 10.0 ** (LEVEL_DB / 20.0) / rms       # the gain that would put the song at the voice's level
    step = RATE // 100                                     # the wanted level is worked out every 10 ms
    keep = math.exp(-(step / float(RATE)) / MUSIC_SLEW_S)
    out = array.array("h", [0]) * n
    level = 10.0 ** (MUSIC_DB / 20.0)
    for i0 in range(0, n, step):
        t = i0 / float(RATE)
        db = MUSIC_DB
        if any(s <= t < e for s, e in spoken):
            db = MUSIC_SPEAK_DB
        if any(s <= t < e for s, e in gaps):
            db = MUSIC_GAP_DB
        if t < film_s:
            fade = min(1.0, t / MUSIC_IN_S, max(0.0, (film_s - t) / MUSIC_OUT_S))
        else:                                              # under the cards: its own level, in and out
            db = MUSIC_CREDITS_DB
            fade = min(1.0, (t - film_s) / MUSIC_CREDITS_IN_S, max(0.0, (film_s + credits_s - t) / MUSIC_CREDITS_OUT_S))
        level = level * keep + 10.0 ** (db / 20.0) * (1.0 - keep)
        g = unit * level * fade
        for i in range(i0, min(n, i0 + step)):
            out[i] = int(round(max(-32767.0, min(32767.0, bed[i] * g))))
    return out


def main():
    ap = argparse.ArgumentParser(description="Builds the Match Light film's voice track and subtitles.")
    ap.add_argument("--font", required=True, help="the subtitle font (a TrueType file)")
    ap.add_argument("--font-licence", default=None, help="the font's licence text; copied onto the disc as FONTLIC.TXT")
    ap.add_argument("--edit", default=None, help="the voice edit list (JSON); without it there is no voice")
    ap.add_argument("--takes", default=None, help="folder of the recordings the edit list names")
    ap.add_argument("--music", default=None, help="a song to lay under the voice (any file ffmpeg reads); without it there is no music")
    ap.add_argument("--script", default=os.path.join(DEMO, "narration.json"))
    ap.add_argument("--out", default=os.path.join(ROOT, "build", "matchlight", "disc"))
    ap.add_argument("--ffmpeg", default=shutil.which("ffmpeg") or "ffmpeg")
    a = ap.parse_args()

    shots = shot_table(os.path.join(DEMO, "matchlight_scene.h"))
    with open(a.script, encoding="utf-8") as f:
        lines = json.load(f)["lines"]
    edit = None
    if a.edit:
        with open(a.edit, encoding="utf-8") as f:
            edit = {e["shot"]: e for e in json.load(f)}
    os.makedirs(a.out, exist_ok=True)
    glyphs, advance, atlas = build_font(a.font)
    film_s = max(end for _, end in shots.values())
    track = array.array("h", [0]) * int(round(film_s * RATE))
    sentences = []                                         # (start s, end s, [(x, y, delay s, glyph index)])
    report = []
    spoken, gaps = [], []                                  # (start s, end s) of each line, and of the pauses the song fills

    for line in lines:
        name, text = line["shot"], line["text"]
        if name not in shots:
            raise SystemExit("narration: the script names shot %s, which the shot table does not have" % name)
        t_shot, t_cut = shots[name]
        t0 = t_shot + LEAD_BY_SHOT.get(name, LEAD_S)
        words = text.split()
        if edit:
            e = edit[name]
            if len(e["words"]) != len(words):
                raise SystemExit("narration: %s has %d words in the script and %d in the edit list" % (name, len(words), len(e["words"])))
            length = e["end"] - e["start"]
            last = 0.0
            for w in e["words"]:                              # word starts in order (an aligner lets two words overlap a little), each word inside the cut
                if not (last <= w[0] <= w[1] <= length + 1.0e-6):
                    raise SystemExit("narration: %s: the word '%s' at %.3f - %.3f s is out of order or outside its %.3f s cut" % (
                        name, w[2] if len(w) > 2 else "?", w[0], w[1], length))
                last = w[0]
            times = [(t0 + w[0], t0 + w[1]) for w in e["words"]]
            raw = subprocess.run([a.ffmpeg, "-hide_banner", "-loglevel", "error", "-ss", str(e["start"]), "-t", str(length), "-i",
                                  os.path.join(a.takes or "", e["take"]), "-vn", "-ac", "1", "-ar", str(RATE), "-f", "s16le", "-"], capture_output=True)
            if raw.returncode != 0 or not raw.stdout:
                raise SystemExit("narration: cannot read %s" % e["take"])
            clip = level_clip(list(array.array("h", raw.stdout)))
            at = int(round(t0 * RATE))
            for i, x in enumerate(clip):
                if at + i < len(track):
                    track[at + i] = x
        else:
            # no recording: typed at DEFAULT_WPS, or faster when the line would not end END_GAP_S before its shot's cut
            pauses = SENTENCE_GAP_S * sum(1 for w in words[:-1] if w[-1] in ".!?")
            room = t_cut - END_GAP_S - t0 - pauses - 0.01
            if room <= 0.0:
                raise SystemExit("narration: the shot %s has no room for its line" % name)
            wps = max(DEFAULT_WPS, len(words) / room)
            times, t = [], t0
            for w in words:
                d = 1.0 / wps
                times.append((t, t + d))
                t += d + (SENTENCE_GAP_S if w[-1] in ".!?" else 0.0)
            length = times[-1][1] - t0
        if t0 + length > t_cut - END_GAP_S:
            raise SystemExit("narration: the line of %s ends %.2f s into the film; its shot is cut at %.2f s (it must end %.1f s before)" % (
                name, t0 + length, t_cut, END_GAP_S))
        spoken.append((t0, t0 + length))
        if name in MUSIC_GAP_SHOTS:
            gaps += [(times[i][1], times[i + 1][0]) for i in range(len(times) - 1) if times[i + 1][0] - times[i][1] >= MUSIC_GAP_MIN_S]
        report.append("%-4s line %6.2f - %6.2f s of the film, shot cut at %6.2f (%.2f s to spare)" % (name, t0, t0 + length, t_cut, t_cut - (t0 + length)))
        # the sentences of the line: each its own subtitle
        k = 0
        while k < len(words):
            n = k
            while n < len(words) - 1 and words[n][-1] not in ".!?":
                n += 1
            sent, stimes = words[k:n + 1], times[k:n + 1]
            rows = wrap(sent, advance)
            ys = [SUB_Y_ONE] if len(rows) == 1 else list(SUB_Y_TWO)
            placed, wi = [], 0
            for row, y in zip(rows, ys):
                pen = 0.5 * (SCREEN_W - line_width(" ".join(row), advance))
                for word in row:
                    w0, w1 = stimes[wi]
                    for ci, ch in enumerate(word):
                        if ch in glyphs:
                            g = glyphs[ch]
                            placed.append((int(round(pen)) + g[5], y + g[6], w0 + (w1 - w0) * TYPE_SHARE * ci / len(word) - stimes[0][0], g[0]))
                        pen += advance.get(ch, advance[" "])
                    pen += advance[" "]
                    wi += 1
            if len(placed) > MAX_GLYPHS:
                raise SystemExit("narration: a sentence has %d letters, the limit is %d: %s" % (len(placed), MAX_GLYPHS, " ".join(sent)))
            sentences.append([stimes[0][0], min(stimes[-1][1] + SUB_HOLD_S, t_cut - 0.3), placed])
            k = n + 1
    for i in range(len(sentences) - 1):                    # a sentence leaves as the next one starts
        sentences[i][1] = min(sentences[i][1], sentences[i + 1][0])

    if len(sentences) > MAX_SENTENCES:
        raise SystemExit("narration: %d sentences; the program reads %d at the most (SUB_SENTENCES_MAX)" % (len(sentences), MAX_SENTENCES))
    subs_bytes = 8 + sum(12 + 8 * len(s[2]) for s in sentences)
    if subs_bytes >= MAX_SUBS_BYTES:
        raise SystemExit("narration: subs.bin would be %d B; the program takes less than %d B (SUBS_MAX_BYTES)" % (subs_bytes, MAX_SUBS_BYTES))
    if a.font_licence:
        shutil.copyfile(a.font_licence, os.path.join(a.out, "FONTLIC.TXT"))
        report.append("FONTLIC.TXT  the font's licence, as its licence asks of every copy")
    order = sorted(glyphs.values())
    with open(os.path.join(a.out, "font.bin"), "wb") as f:
        f.write(b"SFN1" + struct.pack("<HHHH", ATLAS_W, ATLAS_H, len(order), 0))
        for _, x, y, w, h, _, _ in order:
            f.write(struct.pack("<BBBB", x, y, w, h))
        f.write(atlas)
    with open(os.path.join(a.out, "subs.bin"), "wb") as f:
        f.write(b"SUB1" + struct.pack("<HH", len(sentences), 0))
        for t0, t1, placed in sentences:
            f.write(struct.pack("<IIHH", int(round(t0 * 1000)), int(round(t1 * 1000)), len(placed), 0))
            for x, y, delay, g in placed:
                f.write(struct.pack("<HHHBB", x, y, max(0, min(65535, int(round(delay * 1000)))), g, 0))
    if edit:
        with wave.open(os.path.join(a.out, "voice.wav"), "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(RATE)
            w.writeframes(track.tobytes())
        report.append("voice.wav  %d B (streamed from the disc)" % (len(track) * 2 + 44))
    else:
        report.append("voice.wav  NOT written: no --edit given; subtitles typed at %.1f words a second" % DEFAULT_WPS)
    if a.music:
        raw = subprocess.run([a.ffmpeg, "-hide_banner", "-loglevel", "error", "-i", a.music, "-vn", "-ac", "1", "-ar", str(RATE),
                              "-f", "s16le", "-"], capture_output=True)
        if raw.returncode != 0 or not raw.stdout:
            raise SystemExit("narration: cannot read %s" % a.music)
        song = array.array("h", raw.stdout)
        bed = lay_music(song, film_s, credits_seconds(os.path.join(DEMO, "matchlight_scene.h")), spoken, gaps)
        with wave.open(os.path.join(a.out, "music.wav"), "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(RATE)
            w.writeframes(bed.tobytes())
        report.append("music.wav  %d B (streamed from the disc): the song is %.1f s%s; %d dB under the voice, %d dB under it in %d lines, %d dB in %d pauses" % (
            len(bed) * 2 + 44, len(song) / float(RATE), "" if len(song) >= len(bed) else ", repeated with a crossfade",
            -MUSIC_DB, -MUSIC_SPEAK_DB, len(spoken), -MUSIC_GAP_DB, len(gaps)))
    else:
        report.append("music.wav  NOT written: no --music given, the film plays without music")
    report.append("font.bin  %d B: %d glyphs at %d px, %d x %d ARGB4444 = %d KB of video memory" % (
        os.path.getsize(os.path.join(a.out, "font.bin")), len(order), FONT_PX, ATLAS_W, ATLAS_H, ATLAS_W * ATLAS_H * 2 // 1024))
    report.append("subs.bin  %d B: %d sentences, the longest %d letters" % (
        os.path.getsize(os.path.join(a.out, "subs.bin")), len(sentences), max(len(s[2]) for s in sentences)))
    print("\n".join(report))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, KeyError, ValueError, IndexError, struct.error) as e:     # a missing file, a malformed manifest or table: one line, no trace
        raise SystemExit("%s: %s: %s" % (os.path.basename(__file__), type(e).__name__, e))
