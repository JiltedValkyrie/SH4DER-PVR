#!/usr/bin/env python3
"""Match Light film: builds the end credits of the disc, silent-film intertitle cards.

    python demo/matchlight/tools/credits.py --title-font TTF --body-font TTF [--licence NAME=FILE ...]
                                            [--cards credits.json] [--out DIR]

What it writes into --out (default build/matchlight/disc):

  credits.bin   every card of credits.json, laid out: each letter's place on the screen, and one picture of all the
                glyphs the cards use (two typefaces at four sizes; only the letters that occur).
                Layout, little-endian: "CRD1", u16 cards, u16 glyphs, u16 picture width, u16 picture height,
                u32 bytes of the card records; per glyph u8 x, y, w, h (where it lies in the picture); per card
                u16 letters, u16 rule y (the screen row of its one accent rule), then per letter u16 x, u16 y (screen
                pixels of its top left), u8 glyph number, u8 ink (255 = full ink, less = dimmer); then the picture's
                rows as ARGB4444 (white, the glyph's coverage in the alpha), two bytes a texel.
  LICENSES/     with --licence NAME=FILE (repeatable): each FILE copied as LICENSES/NAME (the notices the credits name:
                the fonts' licence texts, the libraries' licence texts).

The cards (the look spec of the film's designer): a 640 x 360 picture in the middle of the 640 x 480 frame; everything
centred on x = 320; a label in capitals, letter-spaced, at 70 % ink; one short accent rule under it; then either a
large name and smaller lines under it (layout "name"), up to three body lines (layout "source"), or up to six body
lines in two columns, the left one filled first (layout "columns": a list of short entries on one card). Positions below are
baselines in the 640 x 360 picture, as the spec gives them; the tool adds the picture's top row.

The fonts are build inputs, not part of the repository (audio_public.json "sources", CREDITS.md). credits.bin does not
carry a font's name. Needs Python 3.8 or later with Pillow 9.2 or later (made and checked with 12.3).
Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.
"""
import argparse
import json
import os
import re
import shutil
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEMO = os.path.dirname(HERE)
ROOT = os.path.dirname(os.path.dirname(DEMO))

SCREEN_W = 640            # frame width, pixels
BAND_TOP = 60             # first screen row of the 640 x 360 picture: (480 - 360) / 2
CENTRE_X = 320            # every line is centred here
SAFE_X = (48, 592)        # no letter outside these columns
SAFE_Y = (82, 278)        # nor outside these rows of the 360-row picture
ATLAS_W, ATLAS_H = 256, 256   # the glyph picture, texels (128 KB of video memory as ARGB4444)
MAX_RECORD_BYTES = 8192   # the card records together must fit this (CREDIT_DATA_BYTES in matchlight.c)
MAX_LETTERS = 128         # the most letters one card may have (the program's limit)
MAX_BODY_CHARS = 36       # the most characters of one body line (the spec's cap)
BODY_WEIGHT = 400         # the body face is a variable font: its Regular weight

# (face, size in pixels, letter-spacing in em, ink 0..255) of each kind of line
LABEL = ("title", 16, 0.12, 178)      # 70 % ink
NAME = ("title", 36, 0.0, 255)
SUB = ("body", 20, 0.0, 204)          # 80 % ink
BODY = ("body", 18, 0.0, 255)

# baselines per layout, rows of the 360-row picture: label, accent rule, then the lines
LAYOUT_NAME_2 = {"label": 156, "rule": 166, "name": 204, "lines": []}            # a label and a name
LAYOUT_NAME_3 = {"label": 138, "rule": 148, "name": 184, "lines": [214, 240]}    # and one or two lines under it
LAYOUT_SOURCE = {"label": 136, "rule": 146, "lines": [180, 204, 228]}            # a label and up to three body lines
LAYOUT_COLUMNS = {"label": 136, "rule": 146, "lines": [180, 204, 228] * 2}       # a label and two columns of up to three body lines
COLUMN_X = (184, 456)     # the centre of the left and of the right column: the middles of the safe area's two halves
COLUMN_HALF_W = 132       # a column's line may reach this far to either side of its centre (8 clear pixels between the columns)


class Atlas:
    """Packs each glyph that is asked for once, in rows, into one ARGB4444 picture."""

    def __init__(self, fonts):
        self.fonts = fonts            # {(face, size): ImageFont}
        self.cells = {}               # {(face, size, char): (index, w, h, left, top)}
        self.table = []               # per glyph (x, y, w, h)
        self.texels = [0] * (ATLAS_W * ATLAS_H)
        self.pen_x, self.pen_y, self.row_h = 1, 1, 0

    def glyph(self, face, size, ch):
        """(index, w, h, left, top) of a glyph: left / top are where its ink starts from the pen on the baseline.
        None for a character with no ink (a space)."""
        key = (face, size, ch)
        if key in self.cells:
            return self.cells[key]
        font = self.fonts[(face, size)]
        mask = font.getmask(ch)
        box = mask.getbbox()
        if box is None:
            self.cells[key] = None
            return None
        x0, y0, x1, y1 = box
        w, h = x1 - x0, y1 - y0
        left, top, _, _ = font.getbbox(ch, anchor="ls")     # the ink's box from the pen on the baseline
        if self.pen_x + w + 1 > ATLAS_W:
            self.pen_x, self.pen_y, self.row_h = 1, self.pen_y + self.row_h + 1, 0
        if self.pen_y + h + 1 > ATLAS_H:
            raise SystemExit("credits: the glyphs do not fit the %d x %d picture" % (ATLAS_W, ATLAS_H))
        if len(self.table) >= 255:
            raise SystemExit("credits: more than 255 different glyphs")
        for y in range(h):
            for x in range(w):
                cover = mask.getpixel((x0 + x, y0 + y)) >> 4                      # 0 .. 15
                self.texels[(self.pen_y + y) * ATLAS_W + self.pen_x + x] = (cover << 12) | 0x0FFF
        self.cells[key] = (len(self.table), w, h, left, top)
        self.table.append((self.pen_x, self.pen_y, w, h))
        self.pen_x += w + 1                                                       # a clear texel between neighbours
        self.row_h = max(self.row_h, h)
        return self.cells[key]


def set_line(atlas, kind, text, baseline, out, centre=CENTRE_X, half_w=None):
    """Lays one line centred on column `centre`: appends (x, y, glyph, ink) per inked letter to out. baseline: row of the
    360-row picture. half_w: how far the line may reach to either side of its centre (None: to the safe area's edges)."""
    left_edge = SAFE_X[0] if half_w is None else centre - half_w
    right_edge = SAFE_X[1] if half_w is None else centre + half_w
    face, size, spacing, ink = kind
    font = atlas.fonts[(face, size)]
    gap = spacing * size
    # each letter's pen: the width of the text before it (so the font's kerning pairs count) plus the letter-spacing
    pens = [font.getlength(text[:i]) + gap * i for i in range(len(text))]
    width = font.getlength(text) + gap * max(0, len(text) - 1)
    x_start = centre - 0.5 * width
    if x_start < left_edge or x_start + width > right_edge:
        raise SystemExit("credits: the line does not fit the safe area (%d px wide): %s" % (round(width), text))
    for ch, p in zip(text, pens):
        g = atlas.glyph(face, size, ch)
        if g is None:
            continue
        index, w, h, left, top = g
        y = baseline + top
        if y < SAFE_Y[0] or y + h > SAFE_Y[1]:
            raise SystemExit("credits: a letter of '%s' leaves the safe rows" % text)
        out.append((int(round(x_start + p)) + left, BAND_TOP + y, index, ink))


def main():
    ap = argparse.ArgumentParser(description="Builds the Match Light film's end credits.")
    ap.add_argument("--title-font", required=True, help="the face of labels and names (a TrueType file)")
    ap.add_argument("--body-font", required=True, help="the face of the smaller lines (a TrueType file)")
    ap.add_argument("--licence", action="append", default=[], metavar="NAME=FILE", help="copy FILE onto the disc as LICENSES/NAME")
    ap.add_argument("--cards", default=os.path.join(DEMO, "credits.json"))
    ap.add_argument("--out", default=os.path.join(ROOT, "build", "matchlight", "disc"))
    a = ap.parse_args()

    from PIL import ImageFont
    fonts = {}
    for face, size, _, _ in (LABEL, NAME, SUB, BODY):
        font = ImageFont.truetype(a.title_font if face == "title" else a.body_font, size)
        if face == "body":
            try:
                font.set_variation_by_axes([BODY_WEIGHT])       # a variable font opens at its lightest weight
            except OSError:
                pass                                            # a static font: it is the weight it is
        fonts[(face, size)] = font
    with open(a.cards, encoding="utf-8") as f:
        cards = json.load(f)["cards"]
    atlas = Atlas(fonts)
    records, report = [], []
    for n, card in enumerate(cards, 1):
        letters = []
        lines = card.get("lines", [])
        if card["layout"] == "name":
            lay = LAYOUT_NAME_3 if lines else LAYOUT_NAME_2
            set_line(atlas, NAME, card["name"], lay["name"], letters)
            kind = SUB
        elif card["layout"] in ("source", "columns"):
            lay, kind = (LAYOUT_SOURCE if card["layout"] == "source" else LAYOUT_COLUMNS), BODY
            for text in lines:
                if len(text) > MAX_BODY_CHARS:
                    raise SystemExit("credits: card %d: a body line has %d characters, the cap is %d: %s" % (n, len(text), MAX_BODY_CHARS, text))
        else:
            raise SystemExit("credits: card %d: unknown layout %s" % (n, card["layout"]))
        columns = card["layout"] == "columns"
        if len(lines) > len(lay["lines"]):
            raise SystemExit("credits: card %d has %d lines, its layout holds %d" % (n, len(lines), len(lay["lines"])))
        set_line(atlas, LABEL, card["label"].upper(), lay["label"], letters)
        left_n = (len(lines) + 1) // 2                           # two columns: the left one takes the odd line
        for k, (text, baseline) in enumerate(zip(lines, lay["lines"])):
            if columns:
                row = k if k < left_n else k - left_n            # both columns start at the first row
                set_line(atlas, kind, text, lay["lines"][row], letters, COLUMN_X[0 if k < left_n else 1], COLUMN_HALF_W)
            else:
                set_line(atlas, kind, text, baseline, letters)
        if len(letters) > MAX_LETTERS:
            raise SystemExit("credits: card %d has %d letters, the limit is %d" % (n, len(letters), MAX_LETTERS))
        rec = struct.pack("<HH", len(letters), BAND_TOP + lay["rule"])
        for x, y, g, ink in letters:
            rec += struct.pack("<HHBB", x, y, g, ink)
        records.append(rec)
        report.append("card %d  %-12s %3d letters" % (n, card["label"], len(letters)))

    with open(os.path.join(DEMO, "matchlight_scene.h"), encoding="utf-8") as f:
        want = re.search(r"#define\s+ML_CREDIT_CARDS\s+(\d+)u", f.read())
    if not want:
        raise SystemExit("credits: ML_CREDIT_CARDS not found in matchlight_scene.h")
    if len(records) != int(want.group(1)):
        raise SystemExit("credits: %d cards; the program shows exactly %d (ML_CREDIT_CARDS in matchlight_scene.h)" % (len(records), int(want.group(1))))
    os.makedirs(a.out, exist_ok=True)
    body = b"".join(records)
    if len(body) > MAX_RECORD_BYTES:
        raise SystemExit("credits: the card records are %d B; the program takes %d B at the most (CREDIT_DATA_BYTES)" % (len(body), MAX_RECORD_BYTES))
    path = os.path.join(a.out, "credits.bin")
    with open(path, "wb") as f:
        f.write(b"CRD1" + struct.pack("<HHHHI", len(records), len(atlas.table), ATLAS_W, ATLAS_H, len(body)))
        for x, y, w, h in atlas.table:
            f.write(struct.pack("<BBBB", x, y, w, h))
        f.write(body)
        f.write(struct.pack("<%dH" % len(atlas.texels), *atlas.texels))
    report.append("credits.bin  %d B: %d cards, %d glyphs in a %d x %d ARGB4444 picture = %d KB of video memory, card records %d B" % (
        os.path.getsize(path), len(records), len(atlas.table), ATLAS_W, ATLAS_H, ATLAS_W * ATLAS_H * 2 // 1024, len(body)))
    for item in a.licence:
        name, _, src = item.partition("=")
        if not name or not src:
            raise SystemExit("credits: --licence wants NAME=FILE, got " + item)
        os.makedirs(os.path.join(a.out, "LICENSES"), exist_ok=True)
        shutil.copyfile(src, os.path.join(a.out, "LICENSES", name))
        report.append("LICENSES/%s  <- %s" % (name, os.path.basename(src)))
    print("\n".join(report))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, KeyError, ValueError, IndexError, struct.error) as e:     # a missing file, a malformed manifest or table: one line, no trace
        raise SystemExit("%s: %s: %s" % (os.path.basename(__file__), type(e).__name__, e))
