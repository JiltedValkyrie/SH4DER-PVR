#!/usr/bin/env python3
"""SH4DER-PVR: the suite menu's skin (demo/psm/suite/suite_skin.h), written from the UI Pack (Kenney, CC0).
Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

    python demo/psm/tools/suite_skin.py [--zip <kenney_ui-pack.zip>] [--check] [--preview <png>]
    python demo/psm/tools/suite_skin.py --self-test

The pack is not in the repository: --zip names the owner's download (by default demo/pl4nar/assets/_dl/, which Git
ignores), and its sha256 must be the one below. From it, a few panels and buttons of PNG/<colour>/Default:
  halved (the menu's text is 8 x 16: the pack's 64-pixel buttons are 32 here), darkened so white and yellow text read
  on them (the pack's panels are light; the shapes, edges and depth stay the pack's), their soft edge mixed with what
  they are drawn on (565 has no alpha), and cut to a 9-slice: the four borders and one pixel of the middle.
Each slot of suite_ui.h gets its 9-slice or its plain colour; the header says what each came from and how much RAM
it takes. Without --check it writes the header; with --check it writes nothing and fails when the header on disk is
not what it would write. --preview writes the slots side by side at 4x, to look at.

Needs Pillow. The pack's licence is CC0; the disc credits it all the same (psm_credits.h, through make_credits.py).
"""
import argparse
import hashlib
import io
import os
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
OUT = os.path.join(ROOT, "demo", "psm", "suite", "suite_skin.h")
ZIP = os.path.join(ROOT, "demo", "pl4nar", "assets", "_dl", "kenney_ui-pack.zip")
ZIP_SHA256 = "a8a14a234911eb648c062622915c93e79e94e97cb7f9f375a70f6617f1174318"   # UI Pack (2.0), 12-06-2024
CLEAR = 0xF81F

BG = (20, 28, 38)                            # the menu's background: no slot of the pack, our own colour

# slot: (pack file, darken r g b, drawn over, l r t b) -- the borders in pixels of the halved image
NINE = {
    "UI_BAND":     ("PNG/Grey/Default/button_rectangle_flat.png",       (0.15, 0.19, 0.27), "bg",   3, 3, 3, 3),
    "UI_ROW":      ("PNG/Grey/Default/button_rectangle_depth_flat.png", (0.21, 0.25, 0.32), "bg",   3, 3, 2, 4),
    "UI_ROW_PICK": ("PNG/Blue/Default/button_rectangle_depth_flat.png", (0.52, 0.52, 0.56), "bg",   3, 3, 2, 4),
    "UI_FLAG":     ("PNG/Red/Default/button_rectangle_depth_flat.png",  (0.80, 0.80, 0.80), "pick", 3, 3, 2, 4),
    "UI_CHIP_PAD": ("PNG/Grey/Default/button_square_depth_flat.png",    (0.42, 0.44, 0.48), "band", 3, 3, 2, 4),
    "UI_CHIP_A":   ("PNG/Red/Default/button_square_depth_flat.png",     (0.85, 0.85, 0.85), "band", 3, 3, 2, 4),
    "UI_CHIP_B":   ("PNG/Blue/Default/button_square_depth_flat.png",    (0.70, 0.70, 0.75), "band", 3, 3, 2, 4),
    "UI_CHIP_Y":   ("PNG/Green/Default/button_square_depth_flat.png",   (0.62, 0.62, 0.62), "band", 3, 3, 2, 4),
}
# plain slots: (pack file whose centre colour it is, darken), or None for BG
PLAIN = {
    "UI_BG":       None,
    "UI_RULE":     ("PNG/Blue/Default/button_rectangle_flat.png", (0.80, 0.80, 0.80)),
    "UI_ROW_MARK": ("PNG/Yellow/Default/button_rectangle_flat.png", (1.0, 1.0, 1.0)),
}
ORDER = ["UI_BG", "UI_BAND", "UI_RULE", "UI_ROW", "UI_ROW_PICK", "UI_ROW_MARK", "UI_FLAG", "UI_CHIP_PAD", "UI_CHIP_A",
         "UI_CHIP_B", "UI_CHIP_Y"]                                    # suite_ui.h's enum, in its order


def rgb565(c):
    v = ((c[0] >> 3) << 11) | ((c[1] >> 2) << 5) | (c[2] >> 3)
    return v ^ 0x20 if v == CLEAR else v     # a real pixel is never the clear key: one step of green off it


def back565(v):
    return (((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31)


def halve(img):
    """RGBA to half size, the colours weighed by their alpha (no dark fringe from the clear pixels)."""
    w, h = img.size
    px = img.load()
    out = []
    for y in range(h // 2):
        row = []
        for x in range(w // 2):
            acc, a = [0, 0, 0], 0
            for dy in (0, 1):
                for dx in (0, 1):
                    p = px[2 * x + dx, 2 * y + dy]
                    a += p[3]
                    for i in range(3):
                        acc[i] += p[i] * p[3]
            row.append((tuple(acc[i] // a for i in range(3)) if a else (0, 0, 0), a // 4))
        out.append(row)
    return out


def shade(grid, k, under):
    """Darken by k, mix the soft edge with `under`; alpha under 64 is left clear."""
    out = []
    for row in grid:
        o = []
        for (c, a) in row:
            if a < 64:
                o.append(None)
                continue
            d = [min(255, int(c[i] * k[i] + 0.5)) for i in range(3)]
            o.append(tuple((d[i] * a + under[i] * (255 - a) + 127) // 255 for i in range(3)))
        out.append(o)
    return out


def nine(grid, l, r, t, b):
    """The 9-slice: borders l r t b and one middle pixel, as (w, h, pixels 565, centre 565)."""
    h, w = len(grid), len(grid[0])
    cx, cy = w // 2, h // 2
    cols = list(range(l)) + [cx] + list(range(w - r, w))
    rows = list(range(t)) + [cy] + list(range(h - b, h))
    pix = [CLEAR if grid[y][x] is None else rgb565(grid[y][x]) for y in rows for x in cols]
    return len(cols), len(rows), pix, rgb565(grid[cy][cx])


def load(z, name):
    from PIL import Image
    return Image.open(io.BytesIO(z.read(name))).convert("RGBA")


def build(zpath):
    with open(zpath, "rb") as f:
        sha = hashlib.sha256(f.read()).hexdigest()
    if sha != ZIP_SHA256:
        raise SystemExit("%s: sha256 %s, not the UI Pack (2.0) this was made from (%s)" % (zpath, sha, ZIP_SHA256))
    slots = {}
    with zipfile.ZipFile(zpath) as z:
        def centre(name, k):
            g = halve(load(z, name))
            c = g[len(g) // 2][len(g[0]) // 2][0]
            return tuple(min(255, int(c[i] * k[i] + 0.5)) for i in range(3))
        under = {"bg": BG}
        for s in ("UI_BAND", "UI_ROW_PICK"):
            name, k = NINE[s][0], NINE[s][1]
            under["band" if s == "UI_BAND" else "pick"] = back565(rgb565(centre(name, k)))
        for s, p in PLAIN.items():
            slots[s] = {"flat": rgb565(BG if p is None else centre(p[0], p[1])), "src": "our own" if p is None else p[0]}
        for s, (name, k, over, l, r, t, b) in NINE.items():
            w, h, pix, flat = nine(shade(halve(load(z, name)), k, under[over]), l, r, t, b)
            slots[s] = {"flat": flat, "pix": pix, "w": w, "h": h, "l": l, "r": r, "t": t, "b": b, "src": name}
    return slots


def header(slots):
    o = ["/* Written by demo/psm/tools/suite_skin.py from the UI Pack (2.0) by Kenney (www.kenney.nl), CC0,",
         "   kenney_ui-pack.zip sha256 %s. Do not edit." % ZIP_SHA256,
         "   The pack's panels halved, darkened and cut to 9-slices (the tool's head says how); the slots of suite_ui.h. */",
         "", "#ifndef SUITE_SKIN_H", "#define SUITE_SKIN_H", "", '#include "suite_ui.h"', ""]
    total = 0
    for s in ORDER:
        d = slots[s]
        if "pix" not in d:
            continue
        total += 2 * len(d["pix"])
        o.append("static const uint16_t ui_skin_%s[%u] = {   /* %s, %u x %u */" % (s[3:].lower(), len(d["pix"]), d["src"], d["w"], d["h"]))
        for y in range(d["h"]):
            o.append("    " + ", ".join("0x%04x" % v for v in d["pix"][y * d["w"]:(y + 1) * d["w"]]) + ",")
        o.append("};")
    o.append("")
    o.append("/* %u bytes of 9-slices, RAM; no video memory. */" % total)
    o.append("static const ui_slot_t ui_skin[UI_SLOTS] = {")
    for s in ORDER:
        d = slots[s]
        if "pix" in d:
            o.append("    { 0x%04x, ui_skin_%s, %uu, %uu, %uu, %uu, %uu, %uu },   /* %s */" % (d["flat"], s[3:].lower(), d["w"], d["h"], d["l"], d["r"], d["t"], d["b"], s))
        else:
            o.append("    { 0x%04x, 0, 0u, 0u, 0u, 0u, 0u, 0u },   /* %s: %s */" % (d["flat"], s, d["src"]))
    o += ["};", "", "#endif /* SUITE_SKIN_H */", ""]
    return "\n".join(o)


def preview(slots, path):
    from PIL import Image
    im = Image.new("RGB", (len(ORDER) * 20 + 8, 28), BG)
    for i, s in enumerate(ORDER):
        d = slots[s]
        for y in range(d.get("h", 12)):
            for x in range(d.get("w", 12)):
                v = d["pix"][y * d["w"] + x] if "pix" in d else d["flat"]
                if v != CLEAR:
                    im.putpixel((4 + i * 20 + x, 4 + y), back565(v))
    im.resize((im.width * 4, im.height * 4), Image.NEAREST).save(path)


def self_test():
    assert rgb565((255, 255, 255)) == 0xFFFF and rgb565((0, 0, 0)) == 0
    assert rgb565((255, 0, 255)) != CLEAR                       # the clear key is never written for a real pixel
    g = [[((10 * x, 10 * y, 0), 255) for x in range(9)] for y in range(7)]
    g[0][0] = ((0, 0, 0), 0)
    s = shade(g, (1, 1, 1), BG)
    w, h, pix, flat = nine(s, 2, 2, 1, 2, )
    assert (w, h) == (5, 4) and pix[0] == CLEAR and flat == rgb565((40, 30, 0))
    assert pix[1] == rgb565((10, 0, 0)) and pix[2] == rgb565((40, 0, 0)) and pix[3] == rgb565((70, 0, 0))
    assert pix[3 * 5 + 4] == rgb565((80, 60, 0))                # the far corner is the source's far corner
    h2 = halve(_img([[(255, 0, 0, 255), (0, 0, 255, 0)], [(255, 0, 0, 255), (0, 0, 255, 0)]]))
    assert h2 == [[((255, 0, 0), 127)]]                         # a clear pixel's colour does not mix in
    m = shade([[((200, 200, 200), 128)]], (1, 1, 1), (0, 0, 0))
    assert m == [[(100, 100, 100)]]                             # half alpha: half way to what is under
    print("suite_skin.py self-test: ok")


def _img(rows):
    from PIL import Image
    im = Image.new("RGBA", (len(rows[0]), len(rows)))
    for y, row in enumerate(rows):
        for x, p in enumerate(row):
            im.putpixel((x, y), p)
    return im


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--zip", default=ZIP)
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--preview")
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args()
    if a.self_test:
        self_test()
        return
    slots = build(a.zip)
    text = header(slots)
    if a.preview:
        preview(slots, a.preview)
    if a.check:
        with open(OUT, encoding="utf-8") as f:
            if f.read() != text:
                raise SystemExit("suite_skin.h is not what suite_skin.py writes from the pack: run it without --check")
        print("suite_skin.h: ok")
        return
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print("wrote %s" % OUT)


if __name__ == "__main__":
    sys.exit(main())
