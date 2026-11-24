#!/usr/bin/env python3
"""Match Light film: builds the title card of the disc from a picture.

    python demo/matchlight/tools/title.py --image CARD.png [--out DIR]

What it writes into --out (default build/matchlight/disc):

  title.bin   the card, shown full screen before the first shot: a 640 x 480 picture at full size, not compressed.
              The picture's colour is stored as ARGB1555 (5 bits a channel, ordered dither, every texel opaque) cut
              into two texture tiles the Dreamcast accepts, 512 x 512 and 128 x 512 (the card's columns 0 .. 511 and
              512 .. 639; rows 480 .. 511 of both are unused and black): 640 KB of video memory.
              The picture's own translucency is kept apart from the colour, as BANDS: runs of rows with one alpha
              value. The program draws each band as a rectangle with that alpha, so the card is laid over the film's
              first frame like a sheet of slightly clear film and all 256 alpha levels survive (a 16-bit texture has
              one alpha bit, or four at the cost of colour).
              Layout, little-endian: "TTL1", u16 width (640), u16 height (480), u16 bands, u16 0; per band u16 first
              row, u16 row after the last, u8 alpha, u8 0, u16 0; then the 512 x 512 tile's rows and the 128 x 512
              tile's rows, two bytes a texel.

The tool expects the alpha to be the same along a row (a card made as a full-frame layer at some opacity, with or
without darker bars): it takes each row's mean alpha and stops if a row's alpha varies by more than ALPHA_SPREAD.
The picture is a build input and is never part of the repository. Needs Python 3.8 or later with Pillow 9.2 or later.
Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.
"""
import argparse
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEMO = os.path.dirname(HERE)
ROOT = os.path.dirname(os.path.dirname(DEMO))

CARD_W, CARD_H = 640, 480         # the card is the whole frame, pixels
TILE_A_W, TILE_B_W = 512, 128     # widths of the two texture tiles (their sum is the card's width)
TILE_H = 512                      # height of both tiles, texels (the card uses the first 480 rows)
MAX_BANDS = 64                    # the most alpha bands the program reads (TITLE_BANDS_MAX in matchlight.c)
ALPHA_SPREAD = 2                  # a row's alpha may vary by this much (of 255) and still count as one value
# 4 x 4 ordered dither thresholds, 0 .. 15 (Bayer): spreads the step from 8 to 5 bits so a soft grey does not band
BAYER = ((0, 8, 2, 10), (12, 4, 14, 6), (3, 11, 1, 9), (15, 7, 13, 5))


def to5(v, x, y):
    """An 8-bit channel value as 5 bits, dithered by the texel's place."""
    scaled = v * 31.0 / 255.0
    low = int(scaled)
    return min(31, low + (1 if (scaled - low) * 16.0 > BAYER[y & 3][x & 3] else 0))


def main():
    ap = argparse.ArgumentParser(description="Builds the Match Light film's title card.")
    ap.add_argument("--image", required=True, help="the card: a 640 x 480 picture (any format Pillow reads)")
    ap.add_argument("--out", default=os.path.join(ROOT, "build", "matchlight", "disc"))
    a = ap.parse_args()

    from PIL import Image
    im = Image.open(a.image).convert("RGBA")
    if im.size != (CARD_W, CARD_H):
        raise SystemExit("title: the picture is %d x %d; the card must be %d x %d" % (im.size[0], im.size[1], CARD_W, CARD_H))
    px = im.load()

    # one alpha value per row, then runs of equal rows
    rows = []
    for y in range(CARD_H):
        alphas = [px[x, y][3] for x in range(CARD_W)]
        if max(alphas) - min(alphas) > ALPHA_SPREAD:
            raise SystemExit("title: row %d has alpha %d .. %d; this tool takes a card whose alpha is the same along a row" % (y, min(alphas), max(alphas)))
        rows.append(int(round(sum(alphas) / float(CARD_W))))
    bands = []
    for y, alpha in enumerate(rows):
        if bands and bands[-1][2] == alpha:
            bands[-1][1] = y + 1
        else:
            bands.append([y, y + 1, alpha])
    if len(bands) > MAX_BANDS:
        raise SystemExit("title: the picture has %d alpha bands; the program reads %d at the most (TITLE_BANDS_MAX)" % (len(bands), MAX_BANDS))

    def tile(x0, width):
        """One tile's texels as ARGB1555 words, row by row: the card's columns x0 .. x0 + width - 1, then black rows."""
        out = []
        for y in range(TILE_H):
            for x in range(x0, x0 + width):
                if y < CARD_H:
                    r, g, b, _ = px[x, y]
                    out.append(0x8000 | (to5(r, x, y) << 10) | (to5(g, x, y) << 5) | to5(b, x, y))
                else:
                    out.append(0x8000)
        return struct.pack("<%dH" % len(out), *out)

    os.makedirs(a.out, exist_ok=True)
    path = os.path.join(a.out, "title.bin")
    with open(path, "wb") as f:
        f.write(b"TTL1" + struct.pack("<HHHH", CARD_W, CARD_H, len(bands), 0))
        for y0, y1, alpha in bands:
            f.write(struct.pack("<HHBBH", y0, y1, alpha, 0, 0))
        f.write(tile(0, TILE_A_W))
        f.write(tile(TILE_A_W, TILE_B_W))
    print("title.bin  %d B: %d x %d card, %d alpha bands (alpha %d .. %d of 255), two ARGB1555 tiles = %d KB of video memory" % (
        os.path.getsize(path), CARD_W, CARD_H, len(bands), min(rows), max(rows), (TILE_A_W + TILE_B_W) * TILE_H * 2 // 1024))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, KeyError, ValueError, IndexError, struct.error) as e:     # a missing or unreadable picture: one line, no trace
        raise SystemExit("%s: %s: %s" % (os.path.basename(__file__), type(e).__name__, e))
