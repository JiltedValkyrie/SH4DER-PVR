#!/usr/bin/env python3
"""Join demo/psm/out/{uniform,psm}.ppm and the two maps into one labelled PNG.

usage: sheet.py [out_dir] [scale]   writes out_dir/sheet.png
"""
import os
import sys

from PIL import Image, ImageDraw


def main(argv):
    out = argv[0] if argv else os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "out")
    scale = int(argv[1]) if len(argv) > 1 else 2
    uni = Image.open(os.path.join(out, "uniform.ppm"))
    psm = Image.open(os.path.join(out, "psm.ppm"))
    muni = Image.open(os.path.join(out, "map_uniform.ppm"))
    mpsm = Image.open(os.path.join(out, "map_psm.ppm"))
    w, h = uni.size
    mh = h
    mw = max(1, int(muni.size[0] * mh / muni.size[1]))
    sheet = Image.new("RGB", (2 * (w + mw) + 3 * 4, h + 16), (20, 20, 24))
    d = ImageDraw.Draw(sheet)
    x = 4
    for label, pic, mp in (("UNIFORM", uni, muni), ("PSM", psm, mpsm)):
        sheet.paste(pic, (x, 14))
        d.text((x, 2), label, fill=(255, 255, 255))
        sheet.paste(mp.resize((mw, mh), Image.NEAREST), (x + w + 4, 14))
        d.text((x + w + 4, 2), "map", fill=(180, 180, 180))
        x += w + mw + 8
    sheet = sheet.resize((sheet.size[0] * scale, sheet.size[1] * scale), Image.NEAREST)
    path = os.path.join(out, "sheet.png")
    sheet.save(path)
    print(path)


if __name__ == "__main__":
    main(sys.argv[1:])
