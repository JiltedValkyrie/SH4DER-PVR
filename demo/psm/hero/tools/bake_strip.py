#!/usr/bin/env python3
"""SH4DER-PVR: hero1, the space stretch: a straight road between hangars, baked to a vertex-colour header.
Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

    python demo/psm/hero/tools/bake_strip.py <out dir> [--zip <spacekit_2.0.zip>]     writes <out dir>/hero_strip_data.h
    python demo/psm/hero/tools/bake_strip.py --self-test

The art is Space Kit (2.0) by Kenney (www.kenney.nl), CC0 1.0. It is read from the zip as it was downloaded (sha256
checked, nothing of it is kept in this tree); the header is a build product. The ground is this tool's own quads in the
kit's rock colour; the road's tiles and every building are the kit's.

The layout (LAYOUT below, metres, the kit's unit x SCALE): the road runs along z from -ROAD to ROAD at x = 0; the hero
runs it between -RUN and RUN. Buildings stand either side, none nearer the road's middle than CLEAR, and one closes
each end. The light is baked: colour = the material's x (AMB + SUNK x max(0, n . SUN)); the program lights the hero
with the same numbers, which this header carries. No normals are shipped.

An object is a run of vertices and triangles with a bounding sphere, so the program can leave out what is not in
view; the ground's objects are marked: they read the hero's shadow.
"""
import argparse
import hashlib
import math
import os
import sys
import zipfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..", ".."))
ZIP = os.path.join(ROOT, "demo", "pl4nar", "assets", "_dl", "spacekit_2.0.zip")
ZIP_SHA256 = "d5d7cdf2635ed5a43a9187deaf409b6f47484e402321128341d3c3698e9ef4d9"
OBJ_DIR = "Models/OBJ format/"

SCALE = 4.0
ROAD = 28.0            # the kit's road tiles, z -ROAD .. ROAD
RUN = 18.0             # the hero's run, z -RUN .. RUN
CLEAR = 3.8            # no building vertex nearer x = 0 than this between the ends
END = 30.0             # past |z| = END the ends' buildings may cross the road
GROUND_X = 16.0
GROUND_Z = 44.0
CHUNK = 8.0            # a ground object's length along z
SUN = (-0.3995, 0.7990, 0.4494)   # toward the sun, unit: the hero's shadow falls to +x, -z
AMB = 0.42
SUNK = 0.60
ROCK = (0.9098039, 0.5176471, 0.3882353)   # the kit's "rock" (terrain.mtl), for this tool's ground quads

# name, x, z, quarter turns about y
LAYOUT = (
    ("hangar_roundA", -10.6, -21.0, 0),
    ("hangar_smallA", -8.0, -7.0, 0),
    ("hangar_largeB", -8.4, 6.0, 2),
    ("rocket_baseA", -7.6, 21.0, 0),
    ("hangar_smallB", 8.0, -22.0, 2),
    ("machine_generatorLarge", 6.8, -11.0, 1),
    ("craft_cargoA", 7.2, 1.0, 0),
    ("hangar_largeA", 8.0, 17.0, 0),
    ("hangar_roundB", 0.0, -37.0, 1),
    ("hangar_roundGlass", 0.0, 37.0, 1),
)


def read_obj(z, name):
    """A kit piece: [(material colour, [(position, normal) x 3])], in the kit's units."""
    kd, cur = {}, None
    for ln in z.read(OBJ_DIR + name + ".mtl").decode("utf-8").splitlines():
        p = ln.split()
        if len(p) >= 2 and p[0] == "newmtl":
            cur = p[1]
        elif len(p) >= 4 and p[0] == "Kd" and cur:
            kd[cur] = tuple(float(c) for c in p[1:4])
    v, vn, tris, mat = [], [], [], None
    for ln in z.read(OBJ_DIR + name + ".obj").decode("utf-8").splitlines():
        p = ln.split()
        if not p:
            continue
        if p[0] == "v":
            v.append(tuple(float(c) for c in p[1:4]))
        elif p[0] == "vn":
            vn.append(tuple(float(c) for c in p[1:4]))
        elif p[0] == "usemtl":
            mat = p[1]
        elif p[0] == "f":
            c = []
            for w in p[1:]:
                q = w.split("/")
                c.append((v[int(q[0]) - 1], vn[int(q[2]) - 1]))
            for k in range(1, len(c) - 1):
                tris.append((kd[mat], (c[0], c[k], c[k + 1])))
    return tris


def turn(p, q):
    """A point or a direction turned q quarter turns about y."""
    x, y, z = p
    for _ in range(q % 4):
        x, z = z, -x
    return (x, y, z)


def lit(col, n):
    d = max(0.0, n[0] * SUN[0] + n[1] * SUN[1] + n[2] * SUN[2])
    k = AMB + SUNK * d
    c = [min(255, int(min(1.0, ch * k) * 255.0 + 0.5)) for ch in col]
    return 0xFF000000 | (c[0] << 16) | (c[1] << 8) | c[2]


class Obj:
    def __init__(self, name, floor):
        self.name, self.floor, self.v, self.c, self.t, self.key = name, floor, [], [], [], {}

    def vert(self, p, argb):
        k = (round(p[0], 4), round(p[1], 4), round(p[2], 4), argb)
        if k not in self.key:
            self.key[k] = len(self.v)
            self.v.append(p)
            self.c.append(argb)
        return self.key[k]

    def tri(self, pts, col, n):
        argb = lit(col, n)
        i = [self.vert(p, argb) for p in pts]
        if i[0] != i[1] and i[1] != i[2] and i[0] != i[2]:
            self.t.append(tuple(i))

    def sphere(self):
        mn = [min(p[k] for p in self.v) for k in range(3)]
        mx = [max(p[k] for p in self.v) for k in range(3)]
        c = [(mn[k] + mx[k]) * 0.5 for k in range(3)]
        return c, max(math.dist(c, p) for p in self.v)


def quad(o, x0, z0, x1, z1):
    """A ground quad at y 0, counter-clockwise seen from above (as the kit's faces are from outside)."""
    up = (0.0, 1.0, 0.0)
    a, b, c, d = (x0, 0.0, z0), (x0, 0.0, z1), (x1, 0.0, z1), (x1, 0.0, z0)
    o.tri((a, b, c), ROCK, up)
    o.tri((a, c, d), ROCK, up)


def build(z):
    objs = []
    road = read_obj(z, "terrain_roadStraight")
    n = int(round(2.0 * GROUND_Z / CHUNK))
    for k in range(n):
        z0 = -GROUND_Z + CHUNK * k
        o = Obj("ground%u" % k, 1)
        quad(o, -GROUND_X, z0, -SCALE * 0.5, z0 + CHUNK)
        quad(o, SCALE * 0.5, z0, GROUND_X, z0 + CHUNK)
        zt = z0
        while zt < z0 + CHUNK - 1e-6:
            if -ROAD - 1e-6 <= zt and zt + SCALE <= ROAD + 1e-6:
                for col, pts in road:
                    o.tri([(p[0] * SCALE, 0.0, p[2] * SCALE + zt + SCALE * 0.5) for p, _ in pts], col, pts[0][1])
            else:
                quad(o, -SCALE * 0.5, zt, SCALE * 0.5, zt + SCALE)
            zt += SCALE
        objs.append(o)
    for name, x, zc, q in LAYOUT:
        o = Obj(name, 0)
        for col, pts in read_obj(z, name):
            w = []
            for p, _ in pts:
                t = turn(p, q)
                w.append((t[0] * SCALE + x, t[1] * SCALE, t[2] * SCALE + zc))
            o.tri(w, col, turn(pts[0][1], q))
        objs.append(o)
    return objs


def check(objs):
    for o in objs:
        if o.floor:
            if any(abs(p[1]) > 1e-6 for p in o.v):
                raise SystemExit("%s: the ground is not flat" % o.name)
            continue
        for p in o.v:
            if abs(p[2]) < END and abs(p[0]) < CLEAR:
                raise SystemExit("%s: a vertex at x %.2f z %.2f is inside the street (CLEAR %.1f)" % (o.name, p[0], p[2], CLEAR))
        if len(o.v) > 65535:
            raise SystemExit("%s: too many vertices" % o.name)
    if sum(len(o.v) for o in objs) > 65535:
        raise SystemExit("over 65,535 vertices")


def render(objs):
    nv, nt = sum(len(o.v) for o in objs), sum(len(o.t) for o in objs)
    o_ = ["/* SH4DER-PVR: hero1, the space stretch. WRITTEN BY demo/psm/hero/tools/bake_strip.py: do not edit; a build product.",
          "   The buildings and the road's tiles: Space Kit (2.0) by Kenney (www.kenney.nl), CC0 1.0.",
          "   Read with ../hero_strip.h (strip_obj_t). Metres, y up, the road along z. */",
          "#ifndef HERO_STRIP_DATA_H", "#define HERO_STRIP_DATA_H", "",
          "#define STRIP_V %uu" % nv, "#define STRIP_T %uu" % nt, "#define STRIP_OBJS %uu" % len(objs),
          "#define STRIP_RUN %.1ff               /* the hero runs z -RUN .. RUN at x 0 */" % RUN,
          "#define STRIP_CLEAR %.1ff             /* no building nearer x 0 than this while |z| < STRIP_END */" % CLEAR,
          "#define STRIP_END %.1ff" % END,
          "#define STRIP_GROUND_X %.1ff          /* the ground: |x| <= this, |z| <= STRIP_GROUND_Z */" % GROUND_X,
          "#define STRIP_GROUND_Z %.1ff" % GROUND_Z,
          "#define STRIP_AMB %.2ff" % AMB, "#define STRIP_SUNK %.2ff" % SUNK,
          "static const float strip_sun[3] = { %.4ff, %.4ff, %.4ff };   /* toward the sun, unit: the light baked below */" % SUN, "",
          "static const float strip_v[STRIP_V][3] = {"]
    for o in objs:
        o_ += ["    { %.4ff, %.4ff, %.4ff }," % p for p in o.v]
    o_ += ["};", "static const uint32_t strip_c[STRIP_V] = {"]
    for o in objs:
        for k in range(0, len(o.c), 8):
            o_.append("    " + " ".join("0x%08xu," % c for c in o.c[k:k + 8]))
    o_ += ["};", "static const uint16_t strip_t[STRIP_T][3] = {   /* an object's own vertices: add its v0 */"]
    for o in objs:
        for k in range(0, len(o.t), 6):
            o_.append("    " + " ".join("{ %u, %u, %u }," % t for t in o.t[k:k + 6]))
    o_ += ["};", "static const strip_obj_t strip_obj[STRIP_OBJS] = {   /* v0, vn, t0, tn, floor, centre, radius */"]
    v0 = t0 = 0
    for o in objs:
        c, r = o.sphere()
        o_.append("    { %u, %u, %u, %u, %u, { %.3ff, %.3ff, %.3ff }, %.3ff },   /* %s */" % (v0, len(o.v), t0, len(o.t), o.floor, c[0], c[1], c[2], r + 0.001, o.name))
        v0 += len(o.v)
        t0 += len(o.t)
    o_ += ["};", "", "#endif /* HERO_STRIP_DATA_H */", ""]
    return "\n".join(o_), nv, nt


def self_test():
    assert turn((1.0, 2.0, 0.0), 1) == (0.0, 2.0, -1.0) and turn((1.0, 2.0, 3.0), 4) == (1.0, 2.0, 3.0)
    assert turn(turn((1.0, 0.0, 2.0), 1), 3) == (1.0, 0.0, 2.0)
    assert abs(math.sqrt(sum(c * c for c in SUN)) - 1.0) < 1e-3 and SUN[1] > 0.5
    assert lit((1.0, 1.0, 1.0), (0.0, -1.0, 0.0)) == 0xFF000000 | (int(AMB * 255.0 + 0.5) * 0x010101)
    assert lit((2.0, 0.0, 0.5), SUN) & 0x00FF0000 == 0x00FF0000
    o = Obj("t", 1)
    quad(o, -1.0, 0.0, 1.0, 2.0)
    assert len(o.v) == 4 and len(o.t) == 2
    a, b, c = (o.v[i] for i in o.t[0])
    e1, e2 = [b[k] - a[k] for k in range(3)], [c[k] - a[k] for k in range(3)]
    assert e1[2] * e2[0] - e1[0] * e2[2] > 0.0, "the ground's triangles face up"
    c_, r = o.sphere()
    assert abs(c_[2] - 1.0) < 1e-6 and abs(r - math.sqrt(2.0)) < 1e-6
    bad = Obj("b", 0)
    bad.tri(((0.0, 0.0, 0.0), (1.0, 1.0, 0.0), (0.0, 1.0, 0.0)), ROCK, (0.0, 0.0, 1.0))
    try:
        check([bad])
    except SystemExit:
        pass
    else:
        raise AssertionError("a building in the street was not refused")
    assert RUN + 10.0 <= ROAD and ROAD <= GROUND_Z and CLEAR < GROUND_X
    print("bake_strip.py self-test: ok")
    return 0


def main():
    ap = argparse.ArgumentParser(description="Bake hero1's space stretch from Space Kit (2.0).")
    ap.add_argument("out", nargs="?")
    ap.add_argument("--zip", default=ZIP)
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args()
    if a.self_test:
        return self_test()
    if not a.out:
        raise SystemExit(__doc__)
    if not os.path.isfile(a.zip):
        raise SystemExit("no %s: Space Kit (2.0), kenney.nl/assets/space-kit (CC0); pass --zip" % a.zip)
    with open(a.zip, "rb") as f:
        got = hashlib.sha256(f.read()).hexdigest()
    if got != ZIP_SHA256:
        raise SystemExit("%s is not the Space Kit (2.0) this layout was made for (sha256 %s)" % (a.zip, got))
    with zipfile.ZipFile(a.zip) as z:
        objs = build(z)
    check(objs)
    text, nv, nt = render(objs)
    os.makedirs(a.out, exist_ok=True)
    out = os.path.join(a.out, "hero_strip_data.h")
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print("wrote %s: %u objects, %u vertices, %u triangles (%u of them ground)" % (out, len(objs), nv, nt, sum(len(o.t) for o in objs if o.floor)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
