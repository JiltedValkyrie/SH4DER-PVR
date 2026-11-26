#!/usr/bin/env python3
"""The wet road's asphalt grit, generated: a grey n x n map that tiles on both axes, our own (an integer hash and plain
arithmetic, nothing fetched, no picture read).

  python tools/grit_gen.py --self-test
  python tools/grit_gen.py --preview <folder>

  asphalt_grit(n=128, seed=GRIT_SEED) -> bytes, n * n grey values, row by row (0 black, 255 white)
  grit_rgba(grey, tint, lo, hi)       -> bytes, RGBA: the tint with the grey as alpha (a straight ramp lo .. hi; the
                                         caller's own curve replaces it)

Cost: built on the host. On the console nothing changes: the texture is the one tex_grit is now, 128 x 128 ARGB4444
with mips, 43,696 bytes of video memory (64 x 64: 10,928 bytes). Not measured on a console.

What it draws, dark to bright:
  the binder      a dark base, mottled by tiling value noise
  the aggregate   a grain a texel, with sparse single bright chips
  the speckles    a soft glow 0.8 to 4.4 texels in radius at 128 with one to eight bright dots in it; they gather
                  where a slow noise is high
  the patches     a few large pale areas with a ragged edge, 9 to 19 texels in radius at 128
Every feature is placed in units of the map's side and every distance wraps, so the map has no seam and a 64 x 64 map
is the same layout at half the detail. No exp, sin or sqrt: the bytes are the same on every machine.
Standard library only. Host tool: nothing here runs on the Dreamcast.
"""
import os
import struct
import sys
import zlib

GRIT_SEED = 2026
GRIT_CRC = {128: 0x87DB146D, 64: 0x40D7C737}       # zlib.crc32 of asphalt_grit(n): the self-test fails when the picture changes

BASE = 0.15                      # the binder's level
MOTTLE = ((64, 0.07), (16, 0.06), (4, 0.03))   # value noise: cells across the map, depth
GRAIN = 0.15                     # the grain a texel, peak to peak
CHIP_ODDS = 0.006                # texels that are a bright chip
CHIP_LO, CHIP_HI = 0.22, 0.55
SPECK_GRID = 16                  # speckles: one at most a cell of this grid
SPECK_ODDS_LO, SPECK_ODDS_HI = 0.10, 0.75      # a cell's odds, by the slow noise
SPECK_R_LO, SPECK_R_HI = 0.006, 0.034          # radius, in map sides
SPECK_A_LO, SPECK_A_HI = 0.08, 0.20            # the soft glow under a speckle
SPECK_DOTS = 8                   # bright dots in a speckle, at most (more in a wide one)
DOT_R_LO, DOT_R_HI = 0.005, 0.012
DOT_A_LO, DOT_A_HI = 0.30, 0.75
PATCH_GRID = 3                   # patches: one at most a cell of this grid
PATCH_ODDS = 0.45
PATCH_R_LO, PATCH_R_HI = 0.07, 0.15
PATCH_A_LO, PATCH_A_HI = 0.10, 0.20


def _h(x, y, k):
    """Integer hash of a cell and a layer number -> 0 .. 1 (below 1)."""
    v = (x * 0x27D4EB2F + y * 0x165667B1 + k * 0x9E3779B9) & 0xFFFFFFFF
    v ^= v >> 16
    v = (v * 0x7FEB352D) & 0xFFFFFFFF
    v ^= v >> 15
    v = (v * 0x846CA68B) & 0xFFFFFFFF
    v ^= v >> 16
    return (v >> 8) / 16777216.0


def _vnoise(u, v, cells, k):
    """Value noise over the unit square, `cells` across, wraps."""
    fx, fy = u * cells, v * cells
    x0, y0 = int(fx), int(fy)
    tx, ty = fx - x0, fy - y0
    tx, ty = tx * tx * (3.0 - 2.0 * tx), ty * ty * (3.0 - 2.0 * ty)
    x0 %= cells
    y0 %= cells
    x1, y1 = (x0 + 1) % cells, (y0 + 1) % cells
    a = _h(x0, y0, k) * (1.0 - tx) + _h(x1, y0, k) * tx
    b = _h(x0, y1, k) * (1.0 - tx) + _h(x1, y1, k) * tx
    return a * (1.0 - ty) + b * ty


def _wrap(d):
    """A difference of two unit coordinates, the short way round."""
    return d - 1.0 if d > 0.5 else d + 1.0 if d < -0.5 else d


def _spots(seed):
    """The speckles (glows and dots alike) and the patches: (u, v, radius, level) each, in map sides."""
    speck, patch = [], []
    for j in range(SPECK_GRID):
        for i in range(SPECK_GRID):
            slow = _vnoise((i + 0.5) / SPECK_GRID, (j + 0.5) / SPECK_GRID, 4, seed + 10)
            if _h(i, j, seed + 11) >= SPECK_ODDS_LO + (SPECK_ODDS_HI - SPECK_ODDS_LO) * slow * slow:
                continue
            s = _h(i, j, seed + 14)
            su, sv = (i + _h(i, j, seed + 12)) / SPECK_GRID, (j + _h(i, j, seed + 13)) / SPECK_GRID
            r = SPECK_R_LO + (SPECK_R_HI - SPECK_R_LO) * s * s                     # most are small
            speck.append((su, sv, r, SPECK_A_LO + (SPECK_A_HI - SPECK_A_LO) * _h(i, j, seed + 15)))
            for d in range(1 + int(SPECK_DOTS * s)):                               # its dots, inside the radius
                c = i + j * SPECK_GRID + d * SPECK_GRID * SPECK_GRID
                speck.append(((su + (_h(c, 1, seed + 16) - 0.5) * 1.4 * r) % 1.0, (sv + (_h(c, 2, seed + 16) - 0.5) * 1.4 * r) % 1.0,
                              DOT_R_LO + (DOT_R_HI - DOT_R_LO) * _h(c, 3, seed + 16),
                              DOT_A_LO + (DOT_A_HI - DOT_A_LO) * _h(c, 4, seed + 16)))
    for j in range(PATCH_GRID):
        for i in range(PATCH_GRID):
            if _h(i, j, seed + 21) >= PATCH_ODDS:
                continue
            patch.append(((i + _h(i, j, seed + 22)) / PATCH_GRID, (j + _h(i, j, seed + 23)) / PATCH_GRID,
                          PATCH_R_LO + (PATCH_R_HI - PATCH_R_LO) * _h(i, j, seed + 24),
                          PATCH_A_LO + (PATCH_A_HI - PATCH_A_LO) * _h(i, j, seed + 25)))
    return speck, patch


def _field(n, seed, fine=True):
    """The map as floats, row by row. fine = 0 leaves the grain and the chips out (the self-test's seam check)."""
    speck, patch = _spots(seed)
    out = []
    for y in range(n):
        v = (y + 0.5) / n
        near_s = [s for s in speck if abs(_wrap(v - s[1])) < s[2]]
        near_p = [p for p in patch if abs(_wrap(v - p[1])) < p[2] * 1.3]
        for x in range(n):
            u = (x + 0.5) / n
            m64 = _vnoise(u, v, 64, seed + 1)
            m16 = _vnoise(u, v, 16, seed + 2)
            c = BASE + MOTTLE[0][1] * (m64 - 0.5) + MOTTLE[1][1] * (m16 - 0.5) \
                + MOTTLE[2][1] * (_vnoise(u, v, 4, seed + 3) - 0.5)
            for pu, pv, r, a in near_p:                 # a patch: the radius moves with the noise, so the edge is ragged
                du, dv = _wrap(u - pu), _wrap(v - pv)
                rr = r * (0.7 + 0.6 * m16)
                t = 1.0 - (du * du + dv * dv) / (rr * rr)
                if t > 0.0:
                    c += a * t * (2.0 - t) * (0.3 + 1.0 * m64)
            for su, sv, r, a in near_s:                 # a speckle: soft, broken up by the fine noise
                du, dv = _wrap(u - su), _wrap(v - sv)
                t = 1.0 - (du * du + dv * dv) / (r * r)
                if t > 0.0:
                    c += a * t * t * (0.55 + 0.45 * m64)
            if fine:
                c += GRAIN * (_h(x, y, seed + 4 + n) - 0.5)
                if _h(x, y, seed + 5 + n) < CHIP_ODDS:
                    c += CHIP_LO + (CHIP_HI - CHIP_LO) * _h(x, y, seed + 6 + n)
            out.append(c)
    return out


def asphalt_grit(n=128, seed=GRIT_SEED):
    """The asphalt grit: n * n grey bytes, row by row. It tiles on both axes. n a power of two, 64 or more."""
    assert n >= 64 and n & (n - 1) == 0
    return bytes(max(0, min(255, int(c * 255.0 + 0.5))) for c in _field(n, seed))


def grit_rgba(grey, tint=(0, 0, 0), lo=0, hi=255):
    """RGBA bytes: the tint with the grey as alpha, a straight ramp from lo (clear) to hi (opaque)."""
    out = bytearray()
    for g in grey:
        a = max(0, min(255, (g - lo) * 255 // (hi - lo)))
        out += bytes((tint[0], tint[1], tint[2], a))
    return bytes(out)


# ------------------------------------------------------------------------------------------------ preview

def write_png(path, w, h, grey):
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    raw = b"".join(b"\0" + grey[y * w:(y + 1) * w] for y in range(h))
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 0, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def scaled(grey, n, k, tiles=1):
    """The map `tiles` times each way, every texel k x k pixels."""
    out = bytearray()
    for y in range(n * tiles):
        row = b"".join(bytes([grey[(y % n) * n + x % n]]) * k for x in range(n * tiles))
        out += row * k
    return bytes(out), n * tiles * k


def preview(folder):
    os.makedirs(folder, exist_ok=True)
    g128, g64 = asphalt_grit(128), asphalt_grit(64)
    a4 = bytes((g >> 4) * 17 for g in g128)             # the 16 levels ARGB4444's alpha keeps
    for name, grey, n, k, tiles in (("grit_128_x4.png", g128, 128, 4, 1), ("grit_128_tiled_2x2_x2.png", g128, 128, 2, 2),
                                    ("grit_128_alpha4_x4.png", a4, 128, 4, 1), ("grit_64_x8.png", g64, 64, 8, 1)):
        px, side = scaled(grey, n, k, tiles)
        path = os.path.join(folder, name)
        if os.path.exists(path):                        # never write over a picture
            print("kept", path)
            continue
        write_png(path, side, side, px)
        print("wrote", path)


# ------------------------------------------------------------------------------------------------ self-test

def stats(grey, n):
    mean = sum(grey) / len(grey)
    dev = (sum((g - mean) ** 2 for g in grey) / len(grey)) ** 0.5
    return mean, dev, sum(1 for g in grey if g >= 110) * 100.0 / len(grey), len(set(g >> 4 for g in grey))


def _steps(f, n):
    """Mean and largest step between neighbour columns and rows inside the map, and the same across the two edges."""
    inner, edge = [], []
    for y in range(n):
        for x in range(n):
            a = f[y * n + x]
            (edge if x == n - 1 else inner).append(abs(f[y * n + (x + 1) % n] - a))
            (edge if y == n - 1 else inner).append(abs(f[((y + 1) % n) * n + x] - a))
    return sum(inner) / len(inner), max(inner), sum(edge) / len(edge), max(edge)


def self_test():
    for n in (128, 64):
        g = asphalt_grit(n)
        assert len(g) == n * n
        assert g == asphalt_grit(n), "not deterministic"
        assert g != asphalt_grit(n, GRIT_SEED + 1), "the seed does nothing"
        crc = zlib.crc32(g) & 0xFFFFFFFF
        assert crc == GRIT_CRC[n], "asphalt_grit(%d) changed: crc 0x%08X, GRIT_CRC has 0x%08X" % (n, crc, GRIT_CRC[n])
        # no seam: the step across an edge is no larger than the steps inside, with the grain and without it
        im, ix, em, ex = _steps([float(v) for v in g], n)
        assert em <= im * 1.15 and ex <= ix, ("seam", n, im, ix, em, ex)
        im, ix, em, ex = _steps(_field(n, GRIT_SEED, fine=False), n)
        assert em <= im * 1.15 and ex <= ix, ("seam in the smooth part", n, im, ix, em, ex)
        mean, dev, bright, levels = stats(g, n)
        assert 30 <= mean <= 60 and dev >= 12 and 1.0 <= bright <= 12.0 and levels >= 10, (n, mean, dev, bright, levels)
        assert min(g) >= 0 and max(g) >= 180, (n, min(g), max(g))
        print("grit %3d: crc 0x%08X, mean %.1f, deviation %.1f, bright (>= 110) %.2f %%, 4-bit levels %d, edge step %.2f"
              " (inside %.2f)" % (n, crc, mean, dev, bright, levels, em * 255.0, im * 255.0))
    rgba = grit_rgba(bytes((0, 64, 128, 255)), (200, 190, 180), 64, 192)
    assert rgba[3::4] == bytes((0, 0, 127, 255)) and rgba[:3] == bytes((200, 190, 180))
    print("grit_gen self-test ok")


if __name__ == "__main__":
    if "--self-test" in sys.argv:
        self_test()
    elif "--preview" in sys.argv:
        preview(sys.argv[sys.argv.index("--preview") + 1])
    else:
        sys.exit(__doc__)
