#!/usr/bin/env python3
"""SH4DER-PVR: the town's sky band, from an equirectangular HDR, as a C header.

demo/psm/tools/bake_sky.py
Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

    python -I bake_sky.py <sky.hdr | -> <out.h> [--preview out.png] [--json report.json]

The input is Poly Haven's kloofendal_48d_partly_cloudy_puresky (2K, CC0; demo/psm/fetch_table.json, demo/psm/CREDITS.md),
fetched by demo/psm/tools/fetch.py into demo/psm/assets/_dl. "-" (no fetch) bakes a plain gradient stand-in, so the build
never needs the download.

What it writes (demo/psm/psm_sky.h reads it):
  psm_sky_px       PSM_SKY_W x PSM_SKY_H RGB565, rows top (PSM_SKY_EL1 degrees) to bottom (PSM_SKY_EL0), u round 360 degrees
                   by compass bearing, row major (the program twiddles it at load). Tone-mapped (1 - exp(-k x), then gamma
                   2.2), an ordered 4 x 4 dither on the 565 cut. The top rows fade into the low-passed top row, so the cap
                   above (plain vertex colour) meets the band without a line.
  psm_sky_top      PSM_SKY_BINS colours round the band's top row, low-passed: the cap's ring.
  psm_sky_horizon  PSM_SKY_BINS colours of the el 0 row, low-passed: the fog colour and the hills' colour read these.
  psm_sky_zenith   the cap's apex.
  PSM_SKY_SUN_U    the texture's u of the HDR's own sun (its brightest texel): the program turns u so that side faces the sun.
Nothing is vendored: the header is build output.
"""
import json
import math
import sys

W, H = 512, 256          # the band texture: 512 round 360 degrees, 256 from el 40 down to el -4
EL0, EL1 = -4.0, 40.0
BINS = 64                # the low-passed rings (cap, horizon)
FADE_ROWS = 24           # the top rows that fade into the low-passed top row
BAYER = (0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5)


def read_hdr(path, max_el=-90.0):
    """Radiance RGBE (new-style RLE scanlines) to rows of linear floats, top row first; rows below max_el degrees are not
    decoded. Returns (width, height, rows) where rows[y] is a flat list r, g, b, r, g, b ..."""
    with open(path, "rb") as f:
        data = f.read()
    pos = 0
    while True:                                             # the header, to the blank line
        end = data.index(b"\n", pos)
        line = data[pos:end]
        pos = end + 1
        if not line.strip():
            break
        if line.startswith(b"FORMAT=") and b"32-bit_rle_rgbe" not in line:
            raise ValueError("not RGBE")
    end = data.index(b"\n", pos)
    res = data[pos:end].split()
    pos = end + 1
    if res[0] != b"-Y" or res[2] != b"+X":
        raise ValueError("orientation %r not supported" % res)
    h, w = int(res[1]), int(res[3])
    last = min(h, int(math.ceil((90.0 - max_el) / 180.0 * h)) + 2)
    rows = []
    for _ in range(last):
        if data[pos] != 2 or data[pos + 1] != 2 or (data[pos + 2] << 8 | data[pos + 3]) != w:
            raise ValueError("flat (old-style) scanlines not supported")
        pos += 4
        ch = []
        for _c in range(4):
            out = bytearray()
            while len(out) < w:
                n = data[pos]
                pos += 1
                if n > 128:
                    out += bytes([data[pos]]) * (n - 128)
                    pos += 1
                else:
                    out += data[pos:pos + n]
                    pos += n
            ch.append(out)
        r, g, b, e = ch
        row = [0.0] * (3 * w)
        for x in range(w):
            if e[x]:
                s = math.ldexp(1.0, e[x] - 136)
                row[3 * x] = r[x] * s
                row[3 * x + 1] = g[x] * s
                row[3 * x + 2] = b[x] * s
        rows.append(row)
    return w, h, rows


def stand_in():
    """No HDR: a gradient (pale horizon, blue above), the field's look, so the build runs without the fetch."""
    w, h = 2048, 1024
    rows = []
    for y in range(int(h * (90.0 - EL0) / 180.0) + 2):
        el = 90.0 - (y + 0.5) * 180.0 / h
        t = min(1.0, max(0.0, el / 50.0)) ** 0.6
        c = (0.95 + (0.30 - 0.95) * t, 1.00 + (0.50 - 1.00) * t, 1.05 + (0.95 - 1.05) * t)
        rows.append([c[i % 3] for i in range(3 * w)])
    return w, h, rows


def sample(rows, w, h, u, el):
    """Bilinear in el, a 4 texel box in u (the 2K source is 4 times the band's width). u wraps."""
    fy = (90.0 - el) / 180.0 * h - 0.5
    y0 = max(0, min(len(rows) - 2, int(math.floor(fy))))
    ty = min(1.0, max(0.0, fy - y0))
    cx = u * w
    out = [0.0, 0.0, 0.0]
    for k in range(4):
        x = int(math.floor(cx - 1.5 + k)) % w
        for c in range(3):
            out[c] += (rows[y0][3 * x + c] * (1.0 - ty) + rows[y0 + 1][3 * x + c] * ty) * 0.25
    return out


def ring(rows, w, h, el_lo, el_hi):
    """BINS colours round the band between two elevations, each a bin's average, then smoothed over its neighbours."""
    y0 = int((90.0 - el_hi) / 180.0 * h)
    y1 = max(y0 + 1, int((90.0 - el_lo) / 180.0 * h))
    acc = []
    for k in range(BINS):
        x0, x1 = k * w // BINS, (k + 1) * w // BINS
        s = [0.0, 0.0, 0.0]
        n = 0
        for y in range(y0, min(y1, len(rows))):
            row = rows[y]
            for x in range(x0, x1):
                s[0] += row[3 * x]; s[1] += row[3 * x + 1]; s[2] += row[3 * x + 2]
                n += 1
        acc.append([v / n for v in s])
    return [[(acc[(k - 1) % BINS][c] + 2.0 * acc[k][c] + acc[(k + 1) % BINS][c]) * 0.25 for c in range(3)] for k in range(BINS)]


def tone(c, k):
    return [min(1.0, max(0.0, 1.0 - math.exp(-k * v))) ** (1.0 / 2.2) for v in c]


def lum(c):
    return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]


def pack565(c, x, y):
    d = (BAYER[(y & 3) * 4 + (x & 3)] + 0.5) / 16.0
    r = min(31, int(c[0] * 31.0 + d))
    g = min(63, int(c[1] * 63.0 + d))
    b = min(31, int(c[2] * 31.0 + d))
    return (r << 11) | (g << 5) | b


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    src, out = argv[0], argv[1]
    opt = {argv[i]: argv[i + 1] for i in range(2, len(argv) - 1, 2)}
    have = src != "-"
    w, h, rows = read_hdr(src, EL0 - 1.0) if have else stand_in()

    # the HDR's sun: the brightest texel above the band's bottom
    best, sun_x, sun_y = -1.0, 0, 0
    for y, row in enumerate(rows):
        for x in range(w):
            v = row[3 * x] + row[3 * x + 1] + row[3 * x + 2]
            if v > best:
                best, sun_x, sun_y = v, x, y
    sun_u = (sun_x + 0.5) / w
    sun_el = 90.0 - (sun_y + 0.5) * 180.0 / h

    # the exposure: the band's 98th percentile luminance (the clouds' lit sides) to 0.875 on screen. Picked by eye on the
    # 565 preview: brighter washes the blue out, and the horizon row then sets a fog near TOWN_LOOK.md's (153, 156, 146)
    band = []
    for j in range(H):
        el = EL1 - (j + 0.5) * (EL1 - EL0) / H
        for i in range(0, W, 4):
            band.append(lum(sample(rows, w, h, (i + 0.5) / W, el)))
    band.sort()
    p98 = band[int(0.98 * (len(band) - 1))]
    k = -math.log(1.0 - 0.875 ** 2.2) / max(p98, 1e-6)
    if "--exposure" in opt:
        k *= float(opt["--exposure"])

    top = [tone(c, k) for c in ring(rows, w, h, EL1 - 2.0, EL1 + 2.0)]
    horizon = [tone(c, k) for c in ring(rows, w, h, -1.0, 1.0)]
    zy = max(1, int(10.0 / 180.0 * h))
    zs = [0.0, 0.0, 0.0]
    for y in range(zy):
        for x in range(w):
            for c in range(3):
                zs[c] += rows[y][3 * x + c]
    zenith = tone([v / (zy * w) for v in zs], k)

    px = []
    for j in range(H):
        el = EL1 - (j + 0.5) * (EL1 - EL0) / H
        f = max(0.0, 1.0 - j / float(FADE_ROWS))           # 1 at the top row, 0 from FADE_ROWS down
        f = f * f * (3.0 - 2.0 * f)
        for i in range(W):
            u = (i + 0.5) / W
            c = tone(sample(rows, w, h, u, el), k)
            if f > 0.0:
                b = u * BINS - 0.5
                b0 = int(math.floor(b))
                t = b - b0
                lp = [top[b0 % BINS][q] * (1.0 - t) + top[(b0 + 1) % BINS][q] * t for q in range(3)]
                c = [c[q] + (lp[q] - c[q]) * f for q in range(3)]
            px.append(pack565(c, i, j))

    if "--preview" in opt:                                  # the texture as the PVR holds it, a binary PPM
        with open(opt["--preview"], "wb") as f:
            f.write(b"P6 %d %d 255\n" % (W, H))
            f.write(bytes(v for p in px for v in (((p >> 11) & 31) * 255 // 31, ((p >> 5) & 63) * 255 // 63, (p & 31) * 255 // 31)))

    def rgb(c):
        return "{ %.4ff, %.4ff, %.4ff }" % tuple(c)

    with open(out, "w", newline="\n") as f:
        f.write("/* SH4DER-PVR: the town's sky band (demo/psm/tools/bake_sky.py, build output, never committed).\n")
        if have:
            f.write("   From Poly Haven kloofendal_48d_partly_cloudy_puresky (2K), Greg Zaal (original), Jarod Guest (sky edits), CC0.\n")
        else:
            f.write("   A gradient stand-in: the HDR was not fetched (demo/psm/tools/fetch.py).\n")
        f.write("   Tone-mapped 1 - exp(-%.4f x), gamma 2.2; RGB565 with a 4 x 4 ordered dither. */\n" % k)
        f.write("#ifndef PSM_SKY_TEX_H\n#define PSM_SKY_TEX_H\n\n")
        f.write("#define PSM_SKY_HAVE %d\n#define PSM_SKY_W %du\n#define PSM_SKY_H %du\n#define PSM_SKY_BINS %du\n" % (1 if have else 0, W, H, BINS))
        f.write("#define PSM_SKY_EL0 (%.1ff)\n#define PSM_SKY_EL1 (%.1ff)\n" % (EL0, EL1))
        f.write("#define PSM_SKY_SUN_U %.5ff     /* the HDR's sun at el %.1f */\n\n" % (sun_u, sun_el))
        f.write("static const float psm_sky_top[PSM_SKY_BINS][3] = {\n")
        f.write(",\n".join("    " + rgb(c) for c in top) + "\n};\n")
        f.write("static const float psm_sky_horizon[PSM_SKY_BINS][3] = {\n")
        f.write(",\n".join("    " + rgb(c) for c in horizon) + "\n};\n")
        f.write("static const float psm_sky_zenith[3] = %s;\n\n" % rgb(zenith))
        f.write("static const uint16_t psm_sky_px[PSM_SKY_W * PSM_SKY_H] __attribute__((aligned(32))) = {\n")
        for j in range(0, len(px), 16):
            f.write("    " + ", ".join("0x%04x" % p for p in px[j:j + 16]) + ",\n")
        f.write("};\n\n#endif /* PSM_SKY_TEX_H */\n")

    rep = {"have": have, "exposure_k": k, "p98": p98, "sun_u": sun_u, "sun_el": sun_el, "zenith": zenith,
           "horizon_mean": [sum(c[q] for c in horizon) / BINS for q in range(3)], "bytes": W * H * 2}
    if "--json" in opt:
        with open(opt["--json"], "w") as f:
            json.dump(rep, f, indent=1)
    print("bake_sky: %s, sun at u %.3f el %.1f, exposure k %.3f, zenith %s, horizon mean %s, %u KB" % (
        "HDR" if have else "stand-in", sun_u, sun_el, k, ["%.2f" % v for v in zenith],
        ["%.2f" % v for v in rep["horizon_mean"]], W * H * 2 // 1024))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
