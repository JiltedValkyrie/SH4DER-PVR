#!/usr/bin/env python3
"""Bake the demo's assets into C: build/gen/assets_gen.c (models, textures, font) from the CC0 packs in assets/.

  python tools/bake.py [--out build/gen]

Models: Kenney OBJ (one palette texture) -> vertices with the palette colour baked in (no texture at run time), a class per
vertex (0 trim, 1 paint, 2 glass, 3 tyre, 4 lamp) and indexed triangles. Car models are turned so the headlights face +Z.
Textures: RGB565, twiddled, with mip levels (PVR layout: level 1x1 at byte 6), from the ambientCG colour maps plus drawn
windows / lane lines. Font: ARGB1555, not twiddled, 8 x 16 cells from PIL's built-in bitmap font.
Host maths only (build machine): nothing here runs on the Dreamcast.
"""
import argparse
import colorsys
import math
import os
import random
import struct
import sys

from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_coupe  # noqa: E402  (tools/check_coupe.py: the hero car has no hole)
import grit_gen  # noqa: E402  (tools/grit_gen.py: the asphalt's grey)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
A = os.path.join(ROOT, "assets")            # fetched packs (tools/fetch.py), not in the repository
GEN = os.path.join(ROOT, "build", "gen")    # tools/coupe.py writes the hero car here, tools/props.py the street furniture
LOD = os.path.join(ROOT, "build", "lod")    # tools/car_lod.py writes the cut traffic cars and the cone here
CAR_SCALE = 1.6            # Kenney car kit unit -> metres
PAINT = (150, 14, 22)      # the player's car: deep red (dark paint shows a reflection best)
CLS_TRIM, CLS_PAINT, CLS_GLASS, CLS_TYRE, CLS_LAMP = range(5)
SMOOTH_COS = 0.5           # cos 60 degrees: faces closer than this share a reflection normal


# ------------------------------------------------------------------------------------------------ models

def load_obj(path):
    V, VT, VN, F, g = [], [], [], [], ""
    for line in open(path, encoding="utf-8"):
        t = line.split()
        if not t:
            continue
        if t[0] == "v":
            V.append(tuple(float(x) for x in t[1:4]))
        elif t[0] == "vt":
            VT.append((float(t[1]), float(t[2])))
        elif t[0] == "vn":
            VN.append(tuple(float(x) for x in t[1:4]))
        elif t[0] in ("g", "o"):
            g = t[1] if len(t) > 1 else ""
        elif t[0] == "f":
            c = [tuple(int(i) - 1 if i else -1 for i in (x.split("/") + ["", ""])[:3]) for x in t[1:]]
            for k in range(1, len(c) - 1):
                F.append((g, c[0], c[k], c[k + 1]))
    return V, VT, VN, F


def hue_dist(a, b):
    d = abs(a - b)
    return min(d, 1.0 - d)


def classify(rgb, group, paint_hue, y_rel=1.0):
    """Kenney car palette: windows are light blue, the paint is the dominant saturated hue, other saturated colours are lamps."""
    r, g, b = rgb
    h, s, v = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
    if group.startswith("wheel"):
        return CLS_TYRE
    if 0.5 <= h <= 0.68 and s < 0.42 and v > 0.7 and b >= r + 10 and y_rel > 0.45:      # windows sit in the upper half
        return CLS_GLASS
    if s > 0.35 and v > 0.3:
        return CLS_PAINT if paint_hue is None or hue_dist(h, paint_hue) < 0.05 else CLS_LAMP
    if min(r, g, b) > 185:
        return CLS_PAINT if paint_hue is None else CLS_LAMP        # white: the paint of a white car, else a lamp / plate
    return CLS_TRIM


def tri_area_normal(a, b, c):
    e1 = [b[i] - a[i] for i in range(3)]
    e2 = [c[i] - a[i] for i in range(3)]
    n = [e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]]
    ln = (n[0] * n[0] + n[1] * n[1] + n[2] * n[2]) ** 0.5
    return 0.5 * ln, ([x / ln for x in n] if ln > 1e-12 else [0.0, 1.0, 0.0])


def bake_model(path, pal, scale, car=False, groups=None, skip_groups=None, recolour=None):
    """-> (verts [(x, y, z, nx, ny, nz, rgb, cls)], indices, lo, hi). groups: keep only groups starting with one of these."""
    V, VT, VN, F = load_obj(path)
    W, H = pal.size
    def colour3(a, b, c):                             # the palette colour at the middle of the triangle's UVs
        u = (VT[a[1]][0] + VT[b[1]][0] + VT[c[1]][0]) / 3.0
        v = (VT[a[1]][1] + VT[b[1]][1] + VT[c[1]][1]) / 3.0
        return pal.getpixel((min(W - 1, int(u * W)), min(H - 1, int((1.0 - v) * H))))
    front, paint_hue, ref_lum = 1.0, None, None
    y_lo, y_hi = min(v[1] for v in V), max(v[1] for v in V)
    smooth = {}                                       # (rounded position) -> [(area, face normal)]
    if car:
        hues, zs_front, lum = {}, [], []
        for g, a, b, c in F:
            if g.startswith("wheel"):
                continue
            px = colour3(a, b, c)
            area, _n = tri_area_normal(V[a[0]], V[b[0]], V[c[0]])
            h, s_, v_ = colorsys.rgb_to_hsv(px[0] / 255.0, px[1] / 255.0, px[2] / 255.0)
            if s_ > 0.35 and v_ > 0.3:
                hues[round(h, 2)] = hues.get(round(h, 2), 0.0) + area
        if hues:                                      # the paint: the saturated hue with the most area (none = a white / grey car)
            near = lambda k: sum(a_ for h_, a_ in hues.items() if hue_dist(h_, k) < 0.05)   # noqa: E731
            best = max(hues, key=near)
            if near(best) > 0.8:
                paint_hue = best
        for g, a, b, c in F:
            px = colour3(a, b, c)
            cls = classify(px, g, paint_hue, ((V[a[0]][1] + V[b[0]][1] + V[c[0]][1]) / 3.0 - y_lo) / (y_hi - y_lo))
            h, s_, v_ = colorsys.rgb_to_hsv(px[0] / 255.0, px[1] / 255.0, px[2] / 255.0)
            if cls == CLS_LAMP and 0.08 <= h <= 0.20 and s_ > 0.35:      # yellow / amber lamps are at the front
                zs_front.append((V[a[0]][2] + V[b[0]][2] + V[c[0]][2]) / 3.0)
            if cls == CLS_PAINT:
                lum.append(0.3 * px[0] + 0.59 * px[1] + 0.11 * px[2])
            if cls in (CLS_PAINT, CLS_GLASS):
                area, n = tri_area_normal(V[a[0]], V[b[0]], V[c[0]])
                for cnr in (a, b, c):
                    smooth.setdefault(tuple(round(x, 4) for x in V[cnr[0]]), []).append((area, n))
        if zs_front and sum(zs_front) / len(zs_front) < 0:
            front = -1.0
        ref_lum = sum(lum) / len(lum) if lum else 128.0
        print("  %s: paint hue %s, front %+d, yellow lamp tris %d" % (os.path.basename(path), paint_hue, int(front), len(zs_front)))
    verts, index, idx = [], {}, []
    for g, a, b, c in F:
        if groups and not any(g.startswith(x) for x in groups):
            continue
        if skip_groups and any(g.startswith(x) for x in skip_groups):
            continue
        px = colour3(a, b, c)
        cls = classify(px, g, paint_hue, ((V[a[0]][1] + V[b[0]][1] + V[c[0]][1]) / 3.0 - y_lo) / (y_hi - y_lo)) if car else CLS_TRIM
        _area, fn = tri_area_normal(V[a[0]], V[b[0]], V[c[0]])
        if recolour and cls == CLS_PAINT:
            k = (0.3 * px[0] + 0.59 * px[1] + 0.11 * px[2]) / ref_lum
            px = tuple(max(0, min(255, int(ch * k + 0.5))) for ch in recolour)
        rgb = (px[0] << 16) | (px[1] << 8) | px[2]
        tri = []
        for cnr in (a, b, c):
            p, n = V[cnr[0]], VN[cnr[2]] if cnr[2] >= 0 else (0.0, 1.0, 0.0)
            key = (cnr[0], cnr[2], rgb, cls)
            if key not in index:
                index[key] = len(verts)
                # the normal the reflection uses: area-weighted mean of the face normals round this position that lie
                # within SMOOTH_COS of this face (a curved body bends the picture; a hard edge stays an edge)
                sn = [0.0, 0.0, 0.0]
                for area, m in smooth.get(tuple(round(x, 4) for x in p), []):
                    if m[0] * fn[0] + m[1] * fn[1] + m[2] * fn[2] > SMOOTH_COS:
                        sn = [sn[i] + area * m[i] for i in range(3)]
                ln = (sn[0] * sn[0] + sn[1] * sn[1] + sn[2] * sn[2]) ** 0.5
                sn = [x / ln for x in sn] if ln > 1e-9 else list(n)
                # front = -1 turns the model half a turn about Y (x and z negated): still a rotation, winding kept
                verts.append((p[0] * scale * front, p[1] * scale, p[2] * scale * front, n[0] * front, n[1], n[2] * front, rgb, cls,
                              sn[0] * front, sn[1], sn[2] * front))
            tri.append(index[key])
        idx += tri
    if len(verts) > 65535:
        raise SystemExit("%s: %d vertices" % (path, len(verts)))
    check_one_class(path, verts, idx)
    lo = [min(v[i] for v in verts) for i in range(3)]
    hi = [max(v[i] for v in verts) for i in range(3)]
    return verts, idx, lo, hi


CLASS_NAMES = {"trim": CLS_TRIM, "paint": CLS_PAINT, "glass": CLS_GLASS, "tyre": CLS_TYRE, "lamp": CLS_LAMP}


def check_one_class(path, verts, idx):
    """The reflection layer picks a triangle by class and sends all three corners: every triangle has one class."""
    for k in range(0, len(idx), 3):
        if len({verts[i][7] for i in idx[k:k + 3]}) != 1:
            raise SystemExit("%s: triangle %d mixes classes" % (os.path.basename(path), k // 3))


def bake_class_model(path, scale, groups=None, skip_groups=None):
    """A class OBJ (tools/coupe.py): every group is <part>__<class>__<rrggbb>, positions only. Flat face normals for
    the light, smoothed ones (paint and glass faces within SMOOTH_COS) for the reflection. Same result as bake_model."""
    V, _VT, _VN, F = load_obj(path)
    smooth, faces = {}, []
    for g, a, b, c in F:
        _part, cname, hexrgb = g.split("__")
        cls, rgb = CLASS_NAMES[cname], int(hexrgb, 16)
        area, fn = tri_area_normal(V[a[0]], V[b[0]], V[c[0]])
        faces.append((g, (a[0], b[0], c[0]), cls, rgb, fn))
        if cls in (CLS_PAINT, CLS_GLASS):
            for i in (a[0], b[0], c[0]):
                smooth.setdefault(tuple(round(x, 4) for x in V[i]), []).append((area, fn))
    verts, index, idx = [], {}, []
    for g, tri, cls, rgb, fn in faces:
        if groups and not any(g.startswith(x) for x in groups):
            continue
        if skip_groups and any(g.startswith(x) for x in skip_groups):
            continue
        for i in tri:
            key = (i, tuple(round(x, 3) for x in fn), rgb, cls)
            if key not in index:
                index[key] = len(verts)
                p, sn = V[i], [0.0, 0.0, 0.0]
                for area, m in smooth.get(tuple(round(x, 4) for x in p), []):
                    if m[0] * fn[0] + m[1] * fn[1] + m[2] * fn[2] > SMOOTH_COS:
                        sn = [sn[k] + area * m[k] for k in range(3)]
                ln = (sn[0] * sn[0] + sn[1] * sn[1] + sn[2] * sn[2]) ** 0.5
                sn = [x / ln for x in sn] if ln > 1e-9 else list(fn)
                verts.append((p[0] * scale, p[1] * scale, p[2] * scale, fn[0], fn[1], fn[2], rgb, cls, sn[0], sn[1], sn[2]))
            idx.append(index[key])
    check_one_class(path, verts, idx)
    lo = [min(v[i] for v in verts) for i in range(3)]
    hi = [max(v[i] for v in verts) for i in range(3)]
    return verts, idx, lo, hi


def emit_model(out, name, m, smooth_out=False):
    verts, idx, lo, hi = m
    r2 = max(v[0] * v[0] + v[1] * v[1] + v[2] * v[2] for v in verts)
    out.append("static const mdl_vert_t %s_v[%d] __attribute__((aligned(32))) = {" % (name, len(verts)))
    for v in verts:
        out.append("  {{%.5ff,%.5ff,%.5ff},{%.4ff,%.4ff,%.4ff},0x%06xu,%du}," % v[:8])
    out.append("};")
    if smooth_out:
        out.append("const float %s_sn[%d][3] __attribute__((aligned(32))) = {" % (name, len(verts)))
        for v in verts:
            out.append("  {%.4ff,%.4ff,%.4ff}," % v[8:11])
        out.append("};")
    out.append("static const uint16_t %s_i[%d] __attribute__((aligned(32))) = {" % (name, len(idx)))
    for k in range(0, len(idx), 24):
        out.append("  " + ",".join(str(i) for i in idx[k:k + 24]) + ",")
    out.append("};")
    out.append("const mdl_t %s = { %s_v, %s_i, %du, %du, %.4ff, {%.4ff,%.4ff,%.4ff}, {%.4ff,%.4ff,%.4ff} };"
               % (name, name, name, len(verts), len(idx), r2 ** 0.5, lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]))
    return len(verts), len(idx) // 3


# ------------------------------------------------------------------------------------------------ textures

def twid(x, y, bits):
    o = 0
    for i in range(bits):
        o |= ((y >> i) & 1) << (2 * i)
        o |= ((x >> i) & 1) << (2 * i + 1)
    return o


def rgb565(px):
    return ((px[0] >> 3) << 11) | ((px[1] >> 2) << 5) | (px[2] >> 3)


MIP_WORDS = [0x3, 0x4, 0x8, 0x18, 0x58, 0x158, 0x558, 0x1558, 0x5558, 0x15558]   # start of each level, 16-bit words


def mip_twiddled(im):
    """Square RGB image -> list of 16-bit words: PVR twiddled mipmap chain, RGB565."""
    n = im.size[0]
    assert im.size[0] == im.size[1] and n & (n - 1) == 0
    top = n.bit_length() - 1
    words = [0] * (MIP_WORDS[top] + n * n)
    for lvl in range(top + 1):
        s = 1 << lvl
        m = im if s == n else im.resize((s, s), Image.BOX)
        px = m.load()
        base = MIP_WORDS[lvl]
        for y in range(s):
            for x in range(s):
                words[base + twid(x, y, lvl)] = rgb565(px[x, y])
    return words


def argb4444(px):
    return ((px[3] >> 4) << 12) | ((px[0] >> 4) << 8) | ((px[1] >> 4) << 4) | (px[2] >> 4)


def mip_twiddled4444(im):
    """Square RGBA image -> list of 16-bit words: PVR twiddled mipmap chain, ARGB4444 (the levels as mip_twiddled)."""
    n = im.size[0]
    assert im.size[0] == im.size[1] and n & (n - 1) == 0
    top = n.bit_length() - 1
    words = [0] * (MIP_WORDS[top] + n * n)
    for lvl in range(top + 1):
        s = 1 << lvl
        m = im if s == n else im.resize((s, s), Image.BOX)
        px = m.load()
        base = MIP_WORDS[lvl]
        for y in range(s):
            for x in range(s):
                words[base + twid(x, y, lvl)] = argb4444(px[x, y])
    return words


GRIT_VARIANT = os.environ.get("PL4NAR_GRIT", "A")   # the alpha under review: A mid-range and smooth, B holes and the rest
                                                      # near opaque (owner's pick pending: grit_review)


def grit_tex(road, variant=None, n=128, seed=4444):
    """The wet road's layer over the reflection, RGBA n x n, our own (an integer hash, nothing fetched): one road surface,
    tex_road's colour (`road`, 256 x 256 over 24 m: a texel here is two of its pixels across, one along, so u across the
    road's 24 m and v along 12 m, one dash period) lightened and darkened by tools/grit_gen.py's grey, with our cracks
    and patches of newer asphalt. Alpha is how much of the road shows over the reflection, from grit_gen's hole field:
    A levels 7 .. 11 of 15 by the field (about 0.6 on average), a few holes at 0; B about a fifth holes, the rest about
    0.87. Paint, kerbs and cracks stay at 15 in both. The vertex alpha lowers the whole layer with the rain."""
    variant = variant or GRIT_VARIANT
    rnd = random.Random(seed)
    m_u, m_v = 24.0 / n, 12.0 / n                     # metres a texel across, along
    im = Image.new("RGBA", (n, n))
    px = im.load()
    rp = road.convert("RGB").load()

    def h(x, y, k):
        v = (x * 0x9E3779B1 ^ (y + 0x7F4A7C15) * 0x85EBCA77 ^ k * 0x2C1B3C6D) & 0xFFFFFFFF
        v ^= v >> 15; v = (v * 0x2C1B3C6D) & 0xFFFFFFFF; v ^= v >> 12
        return (v & 0xFFFF) / 65535.0

    grey = grit_gen.asphalt_grit(n)
    mean = sum(grey) / float(len(grey))
    holes = grit_gen.asphalt_grit_holes(n)
    rank = {v: i / 255.0 for i, v in enumerate(sorted(set(holes)))}   # the field's values spread 0 .. 1 (by value)
    order = sorted(holes)
    cut = order[int(len(order) * (0.03 if variant == "A" else 0.20))]   # under this: a hole
    keep = set()                                      # texels that stay at 15: paint, kerbs, cracks
    for y in range(n):
        for x in range(n):
            c0, c1 = rp[2 * x, y], rp[2 * x + 1, y]
            r, g, b = [(c0[i] + c1[i]) // 2 for i in range(3)]
            if max(r, g, b) >= 140 or r - b > 60:      # paint and kerb stones (the asphalt and sidewalk sit under 140)
                keep.add((x, y))
                px[x, y] = (r, g, b, 255)
                continue
            gg = (grey[y * n + (2 * x) % n] + grey[y * n + (2 * x + 1) % n]) * 0.5   # the grit: grit_gen's grey
            k = max(0.6, min(1.5, 1.0 + 1.2 * (gg - mean) / 255.0))
            f = holes[y * n + (2 * x) % n]            # the left half: no mean, so a hole stays a hole
            if f < cut:
                lvl = 0
            elif variant == "A":
                lvl = int(6.5 + 5.0 * rank[f] ** 0.6)
            else:
                lvl = 12 + int(h(x, y, 11) * 2.999)
            px[x, y] = (min(255, int(r * k)), min(255, int(g * k)), min(255, int(b * k)), lvl * 17)
    for _k in range(3):                               # patches of newer asphalt, a sealed seam round each: darker
        w, hh = rnd.randint(10, 22), rnd.randint(14, 34)
        x0, y0 = rnd.randint(int(4.6 / m_u), int(19.4 / m_u) - w), rnd.randint(0, n - 1)
        for y in range(hh):
            for x in range(w):
                yy = (y0 + y) % n
                if (x0 + x, yy) in keep:
                    continue
                seam = x == 0 or y == 0 or x == w - 1 or y == hh - 1
                r, g, b, a = px[x0 + x, yy]
                px[x0 + x, yy] = (40, 40, 43, 255) if seam else (r * 3 // 4, g * 3 // 4, b * 3 // 4, a)
    for _k in range(7):                               # cracks: a random walk, a texel wide, dark, they hold the eye
        x, y = rnd.uniform(4.5 / m_u, 19.5 / m_u), rnd.uniform(0, n)
        ang = rnd.uniform(0, 6.283)
        for _s in range(rnd.randint(25, 70)):
            ix, iy = int(x) % n, int(y) % n
            if 4.0 <= ix * m_u < 20.0 and (ix, iy) not in keep:
                px[ix, iy] = (30, 30, 32, 255)
            ang += rnd.uniform(-0.6, 0.6)
            x += math.cos(ang) * 0.8; y += math.sin(ang) * 1.2
    return im


def emit_words(out, name, words):
    out.append("const uint16_t %s[%d] __attribute__((aligned(32))) = {" % (name, len(words)))
    for k in range(0, len(words), 16):
        out.append("  " + ",".join("0x%04x" % w for w in words[k:k + 16]) + ",")
    out.append("};")


def emit_bytes(out, name, data):
    out.append("const uint8_t %s[%d] __attribute__((aligned(32))) = {" % (name, len(data)))
    for k in range(0, len(data), 32):
        out.append("  " + ",".join("%d" % b for b in data[k:k + 32]) + ",")
    out.append("};")


LOW = 64                   # the low look: every world texture 64 x 64, 16 colours
LOW_SAT = 0.72             # saturation kept (about the luma): a late 1990s console's washed street
LOW_GAIN = 0.94


def low_texture(out, name, im):
    """-> a 64 x 64 preview. Emits tex_p_<name> (4 bits a texel, twiddled, even texel in the low half) and pal_<name> (RGB565)."""
    small = im.convert("RGB").resize((LOW, LOW), Image.LANCZOS)
    px = small.load()
    for y in range(LOW):                              # the grade first, so the 16 colours are spent on what is shown
        for x in range(LOW):
            r, g, b = px[x, y]
            l = 0.3 * r + 0.59 * g + 0.11 * b
            px[x, y] = tuple(max(0, min(255, int((l + (c - l) * LOW_SAT) * LOW_GAIN + 0.5))) for c in (r, g, b))
    q = small.quantize(16, method=Image.MEDIANCUT, dither=Image.FLOYDSTEINBERG)
    pal = q.getpalette()[:48]
    pal += [0] * (48 - len(pal))
    qx = q.load()
    data = [0] * (LOW * LOW // 2)
    bits = LOW.bit_length() - 1
    for y in range(LOW):
        for x in range(LOW):
            t = twid(x, y, bits)
            data[t >> 1] |= (qx[x, y] & 15) << ((t & 1) * 4)
    emit_bytes(out, "tex_p_" + name, data)
    out.append("const uint16_t pal_%s[16] = { %s };" % (name, ",".join("0x%04x" % rgb565(tuple(pal[3 * i:3 * i + 3])) for i in range(16))))
    return q.convert("RGB")


def tone(im, mul, add=(0, 0, 0)):
    r, g, b = im.split()
    ch = [c.point(lambda v, m=m, a=a: max(0, min(255, int(v * m + a)))) for c, m, a in zip((r, g, b), mul, add)]
    return Image.merge("RGB", ch)


def facade(base, style, seed):
    """128 x 128: 4 bays x 4 floors on a wall material."""
    rnd = random.Random(seed)
    im = base.copy()
    d = ImageDraw.Draw(im)
    wall = im.resize((1, 1), Image.BOX).getpixel((0, 0))
    lite = tuple(min(255, int(c * 1.22 + 8)) for c in wall)
    dark = tuple(int(c * 0.55) for c in wall)
    for fy in range(4):
        y = fy * 32
        if style == "ribbon":                         # office: one glass band a floor
            d.rectangle([0, y + 8, 127, y + 22], fill=(34, 46, 62))
            for x in range(0, 128, 16):
                d.line([x, y + 8, x, y + 22], fill=(70, 82, 96))
            d.line([0, y + 8, 127, y + 8], fill=(96, 118, 140))
            d.line([0, y + 9, 127, y + 9], fill=(62, 82, 106))
            d.line([0, y + 23, 127, y + 23], fill=dark)
            d.line([0, y + 7, 127, y + 7], fill=lite)
            continue
        for bx in range(4):
            x = bx * 32
            x0, x1, y0, y1 = (x + 8, x + 23, y + 6, y + 25) if style == "tall" else (x + 6, x + 25, y + 8, y + 23)
            g = rnd.randint(-10, 10)
            glass = (36 + g, 48 + g, 64 + g)
            if rnd.random() < 0.08:
                glass = (212, 178, 104)               # a lit room
            d.rectangle([x0 - 1, y0 - 1, x1 + 1, y1 + 1], fill=lite)
            d.rectangle([x0, y0, x1, y1], fill=glass)
            d.line([x0, y0, x1, y0], fill=tuple(min(255, c + 46) for c in glass))      # sky on the top edge of the glass
            d.line([x0, y0 + 1, x1, y0 + 1], fill=tuple(min(255, c + 22) for c in glass))
            mid = (x0 + x1) // 2
            d.line([mid, y0, mid, y1], fill=lite)
            if style == "tall":
                d.line([x0, (y0 + y1) // 2, x1, (y0 + y1) // 2], fill=lite)
            d.rectangle([x0 - 2, y1 + 2, x1 + 2, y1 + 3], fill=dark)                    # sill and its shadow
        d.line([0, y + 31, 127, y + 31], fill=tuple(int(c * 0.8) for c in wall))       # floor line
    return im


def road_tex(asphalt, concrete):
    """256 x 256 = 24 m x 24 m: sidewalk 4 m, kerb, 16 m of asphalt (two lanes each way), kerb, sidewalk 4 m; v runs along the road."""
    n, m = 256, 24.0
    px_m = n / m
    im = Image.new("RGB", (n, n))
    im.paste(tone(asphalt.resize((n, n), Image.LANCZOS), (0.78, 0.78, 0.80)), (0, 0))
    side = tone(concrete.resize((n, n), Image.LANCZOS), (0.66, 0.66, 0.66))
    sw = int(4.0 * px_m)
    im.paste(side.crop((0, 0, sw, n)), (0, 0))
    im.paste(side.crop((n - sw, 0, n, n)), (n - sw, 0))
    d = ImageDraw.Draw(im)
    for x in (sw - 3, n - sw):                         # kerb stones
        d.rectangle([x, 0, x + 2, n - 1], fill=(150, 150, 146))
    d.line([sw, 0, sw, n - 1], fill=(40, 40, 40))
    d.line([n - sw - 1, 0, n - sw - 1, n - 1], fill=(40, 40, 40))
    for y in range(0, n, int(2.0 * px_m)):             # sidewalk joints
        d.line([0, y, sw - 4, y], fill=(96, 96, 94))
        d.line([n - sw + 3, y, n - 1, y], fill=(96, 96, 94))
    c = n // 2
    d.rectangle([c - 3, 0, c - 2, n - 1], fill=(196, 160, 40))    # double yellow
    d.rectangle([c + 1, 0, c + 2, n - 1], fill=(196, 160, 40))
    dash, period = int(3.0 * px_m), int(12.0 * px_m)
    for lane in (-4.0, 4.0):                           # dashed white lane lines
        x = int(c + lane * px_m)
        for y in range(0, n, period):
            d.rectangle([x - 1, y, x, y + dash], fill=(200, 200, 196))
    for x in (sw + int(0.5 * px_m), n - sw - int(0.5 * px_m) - 2):   # solid edge lines
        d.rectangle([x, 0, x + 1, n - 1], fill=(176, 176, 172))
    return im


def font_words():
    """128 x 128 ARGB1555, row order: ASCII 32 .. 127 in 8 x 16 cells, 16 a row."""
    f = ImageFont.load_default()
    im = Image.new("L", (128, 128), 0)
    d = ImageDraw.Draw(im)
    for ch in range(32, 127):
        cx, cy = ((ch - 32) % 16) * 8, ((ch - 32) // 16) * 16
        d.text((cx + 1, cy + 2), chr(ch), font=f, fill=255)
    d.rectangle([120, 80, 127, 95], fill=255)          # cell 127: a solid block (bars)
    px = im.load()
    return [0xffff if px[x, y] > 100 else 0x0000 for y in range(128) for x in range(128)]


# ------------------------------------------------------------------------------------------------ main

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(ROOT, "build", "gen"))
    ap.add_argument("--coupe", default=os.path.join(GEN, "coupe.obj"))
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    out = ["/* Generated by tools/bake.py from the CC0 packs in assets/ (see CREDITS.md). Do not edit. */",
           '#include "assets.h"', ""]
    rep = []
    car_pal = Image.open(os.path.join(A, "kenney_car-kit", "colormap.png")).convert("RGB")
    cars = os.path.join(A, "kenney_car-kit")
    if check_coupe.main_path(a.coupe) != 0:              # a crack shows the road through the car: refuse it
        raise SystemExit("%s has holes (tools/check_coupe.py): make it again with tools/coupe.py" % os.path.basename(a.coupe))
    rep.append(("mdl_player_body", emit_model(out, "mdl_player_body", bake_class_model(a.coupe, 1.0, skip_groups=["wheel"]), smooth_out=True)))
    rep.append(("mdl_player_wheels", emit_model(out, "mdl_player_wheels", bake_class_model(a.coupe, 1.0, groups=["wheel"]))))
    for n in ("taxi", "police", "van"):
        rep.append(("mdl_%s_body" % n, emit_model(out, "mdl_%s_body" % n, bake_model(os.path.join(LOD, n + ".obj"), car_pal, CAR_SCALE, car=True, skip_groups=["wheel"]))))
        rep.append(("mdl_%s_wheels" % n, emit_model(out, "mdl_%s_wheels" % n, bake_model(os.path.join(LOD, n + ".obj"), car_pal, CAR_SCALE, car=True, groups=["wheel"]))))
    for n in ("lamp", "pole", "signal", "dumpster"):          # own work (tools/props.py), metres
        rep.append(("mdl_" + n, emit_model(out, "mdl_" + n, bake_class_model(os.path.join(GEN, "props.obj"), 1.0, groups=[n + "-"]))))
    rep.append(("mdl_cone", emit_model(out, "mdl_cone", bake_model(os.path.join(LOD, "cone.obj"), car_pal, CAR_SCALE))))
    amb = os.path.join(A, "ambientcg")
    def colour_map(name):                           # the pack's colour map, whatever resolution tag its file name carries
        hits = sorted(f for f in os.listdir(amb) if f.startswith(name) and f.endswith("Color.jpg"))
        if not hits:
            raise SystemExit("no %s*Color.jpg in assets/ambientcg: run tools/fetch.py" % name)
        return Image.open(os.path.join(amb, hits[0])).convert("RGB")
    brick, conc, plas, asph = colour_map("Bricks075A"), colour_map("Concrete034"), colour_map("PaintedPlaster017"), colour_map("Asphalt025A")
    sq = lambda im: im.crop((0, 0, min(im.size), min(im.size))).resize((128, 128), Image.LANCZOS)   # noqa: E731
    walls = [(tone(sq(brick), (0.92, 0.62, 0.50)), "tall", 11),          # red brick
             (tone(sq(plas), (0.88, 0.82, 0.66)), "wide", 23),           # cream plaster
             (tone(sq(conc), (0.60, 0.62, 0.66)), "ribbon", 37),         # grey office
             (tone(sq(brick), (0.52, 0.46, 0.42)), "wide", 53)]          # dark brick
    prev = Image.new("RGB", (128 * 4 + 256, 256))
    lows = []
    for k, (base, style, seed) in enumerate(walls):
        im = facade(base, style, seed)
        prev.paste(im, (k * 128, 0))
        emit_words(out, "tex_facade%d" % k, mip_twiddled(im))
        lows.append(low_texture(out, "facade%d" % k, im))
    road = road_tex(asph, conc)
    prev.paste(road, (512, 0))
    emit_words(out, "tex_road", mip_twiddled(road))
    lows.append(low_texture(out, "road", road))
    lowprev = Image.new("RGB", (LOW * 5, LOW))
    for k, im in enumerate(lows):
        lowprev.paste(im, (k * LOW, 0))
    lowprev.resize((LOW * 5 * 4, LOW * 4), Image.NEAREST).save(os.path.join(a.out, "textures_low_x4.png"))
    glow = []                                          # 32 x 32 RGB565, row order: a round glow with a hot core (added on screen)
    for y in range(32):
        for x in range(32):
            r = min(1.0, (((x - 15.5) / 15.5) ** 2 + ((y - 15.5) / 15.5) ** 2) ** 0.5)
            k = int(255 * min(1.0, (1.0 - r) ** 2 * 1.1 + max(0.0, 1.0 - r * 4.0) * 0.6))
            glow.append(rgb565((k, k, k)))
    emit_words(out, "tex_glow", glow)
    grit = grit_tex(road)                              # the wet road's layer (ARGB4444, twiddled mips: 43,696 bytes)
    emit_words(out, "tex_grit", mip_twiddled4444(grit))
    grit.resize((512, 512), Image.NEAREST).save(os.path.join(a.out, "grit_x4.png"))
    emit_words(out, "tex_font", font_words())
    prev.save(os.path.join(a.out, "textures_preview.png"))
    with open(os.path.join(a.out, "assets_gen.c"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(out) + "\n")
    # the work arrays of the demo are sized by these: no model can outgrow them
    with open(os.path.join(a.out, "assets_gen.h"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write("/* Generated by tools/bake.py. Do not edit. */\n#ifndef ASSETS_GEN_H\n#define ASSETS_GEN_H\n")
        fh.write("#define MDL_PLAYER_BODY_NV %d\n#define MDL_MAX_NV %d\n#endif\n" % (dict(rep)["mdl_player_body"][0], max(nv for _n, (nv, _t) in rep)))
    for name, (nv, nt) in rep:
        print("%-20s %5d verts %5d tris" % (name, nv, nt))


if __name__ == "__main__":
    main()
