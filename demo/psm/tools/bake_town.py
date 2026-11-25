#!/usr/bin/env python3
"""Build the town square of demo/psm/TOWN_STORY.md into a C header for the Dreamcast program and the host test.

  python -I demo/psm/tools/bake_town.py <modular_village_collection.zip | -> <out.h> [--lod <dir>] [--json <report.json>]

The kit is Modular Village (Keith at Fertile Soil Productions, CC0, fetch_table.json), read from the zip; nothing is
extracted. The kit's pieces as they are come to about 64,000 triangles in the mock-up's placement (TOWN_MOCKUP.md), so:

- the walls, roofs, windows, doors, chimneys, the tower, the churchyard wall, the cross, the stall posts and counters are
  built here in the kit's colours: a wall face is one grid of quads (1.5 m columns, a row a block, the top block in
  four rows: the eave band route B needs), a window or a door a proud frame with its glass or planks (12 triangles),
  an open crate four sides and its produce (the kit's crate cuts clean only at 324 triangles);
- the props the story names (the arch, the well, the barrels, the ladder, the crates, the canopies, the pen, the lamps,
  the sign) are the kit's pieces, cut by the quadric decimator of third_party/dc-forerunner/tools/lod/qem_decimate.py
  (imported from there, not copied; --lod names another copy) at the smallest cut that keeps every colour and opens no
  crack. Without the zip each of them is a box of its size in its main colour, so the build and the host test run
  without the art (PSM_TOWN_KIT 0).

Out, in the program's world frame (metres, x east, y up, z SOUTH: story z north is -z here, as psm_scene.h has the camera
look along -z at yaw 0, which is north):
- the screen mesh as strips (a strip's triangle j is its vertices j, j + 1, j + 2), a palette index and a normal a vertex;
- the casters: low boxes and prisms, an object each (a house, the tower, a barrel), for the map; the screen mesh is
  never drawn into it;
- the movers' templates (the placeholder person, the hen), local to their feet, facing -z;
- the people's accent colours, in the order of psm_town.h's cast.
Coordinates and placements follow the mock-up's town_scene.py (story metres, the sun-line frame of the south-west fill).
"""
import json
import math
import os
import sys
import zipfile

U = 1.5                                    # metres a kit unit; a storey is 2 units
HERE = os.path.dirname(os.path.abspath(__file__))
LOD_DEFAULT = os.path.join(HERE, "..", "..", "..", "third_party", "dc-forerunner", "tools", "lod")

# ---- colours ---------------------------------------------------------------------------------------------------
# the kit's .mtl colours (read from the zip when there is one; these are its values, for the stand-in build)
KD = {
    "Stucco": (0.9373, 0.8941, 0.8196), "Wood_Light": (0.7373, 0.6314, 0.4627), "Wood_Medium": (0.6275, 0.5255, 0.3961),
    "Wood_Dark": (0.5176, 0.4196, 0.3333), "Wood_Darker": (0.3843, 0.3137, 0.2510), "Red_Roof": (0.5765, 0.2667, 0.1922),
    "Dark_Red_Roof": (0.4235, 0.1804, 0.1176), "Stone_Brit_Gray_Dark": (0.4039, 0.3529, 0.3137),
    "Stone_Brit_Gray": (0.4863, 0.4235, 0.3725), "Stone_Brit_Gray_Light": (0.5725, 0.4902, 0.4275),
    "Stone_Bri_Gray_Lighter": (0.6627, 0.5961, 0.5451), "Burlap": (0.7255, 0.6431, 0.5216), "Window": (0.6588, 0.7059, 0.7765),
    "Iron_Grey": (0.3020, 0.2863, 0.2745), "Canvas": (0.8039, 0.7412, 0.5961), "Candlelight": (1.0, 0.8824, 0.5255),
}
# the designer's palette over the kit's (TOWN_MOCKUP.md "The look")
STUCCO = (0.78, 0.75, 0.68)
MID_TIMBER = (0.44, 0.365, 0.29)
POSTS = {"Canopy_Beam", "Canopy_Beam_Short", "Wood_Post_Small", "Wood_Post_Large"}
ACC = [(0.66, 0.40, 0.34), (0.46, 0.53, 0.66), (0.52, 0.60, 0.42), (0.86, 0.81, 0.70)]      # muted dyes
GLASS = (0.22, 0.24, 0.27)                 # a window seen from outside in daylight: dark
PEOPLE_ACC = [(0.24, 0.36, 0.60), (0.30, 0.48, 0.24), (0.34, 0.26, 0.20), (0.62, 0.20, 0.16),     # psm_town.h's cast, in order
              (0.62, 0.20, 0.16), (0.30, 0.48, 0.24), (0.42, 0.33, 0.24), (0.42, 0.33, 0.24)]
PERSON_GREY = (0.70, 0.70, 0.72)
HEN_WHITE, HEN_COMB, HEN_LEG = (0.86, 0.84, 0.80), (0.70, 0.18, 0.14), (0.80, 0.66, 0.34)
ACCENT = 255                               # a template vertex coloured by its mover's accent

# the kit pieces used, the cut's target, and for the stand-in: the piece's box (local, kit units) and its main colour
KIT = {
    "Stone_Arch_Outer": (150, (-0.5, -0.05, -0.65, 0.65, 3.0, 1.65), "Stone_Brit_Gray_Light"),
    "Prop_Well_1": (250, (-0.75, 0.0, -0.54, 0.61, 1.51, 0.54), "Stone_Brit_Gray"),
    "Prop_Barrel_2": (32, (-0.24, -0.01, -0.25, 0.24, 0.58, 0.25), "Wood_Medium"),
    "Prop_Ladder_1": (80, (-0.53, -0.02, -0.34, 0.58, 3.91, 0.34), "Wood_Medium"),
    "Canopy_Top": (0, (-0.5, 1.1, -0.5, 0.5, 1.21, 0.5), "Canvas"),
    "Wood_Post_Small": (0, (-0.59, 0.0, 0.41, -0.41, 0.55, 0.59), "Wood_Darker"),
    "Wood_Rope_Straight": (12, (-0.5, -0.14, 0.4, 0.5, 0.51, 0.6), "Burlap"),
    "Wall_Prop_Lamp": (60, (-0.5, 0.42, -0.12, 0.32, 1.12, 0.12), "Iron_Grey"),
    "Wall_Prop_Sign_1": (40, (-0.5, 0.23, -0.1, 1.05, 1.18, 0.1), "Wood_Dark"),
}

FACE = {"E": (1.0, 0.0), "W": (-1.0, 0.0), "N": (0.0, -1.0), "S": (0.0, 1.0)}   # the world frame (z south)


def R(x, y, zs):
    """story (x, y, z north) -> world (x, y, z south)"""
    return (x, y, -zs)


def add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def mul(a, s): return (a[0] * s, a[1] * s, a[2] * s)
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def unit(a):
    n = math.sqrt(dot(a, a))
    return mul(a, 1.0 / n) if n > 1e-12 else None


def flat(d):
    """a facing (letter or (dx, dz)) as a world vector on the ground"""
    dx, dz = FACE[d] if isinstance(d, str) else d
    return (dx, 0.0, dz)


UP = (0.0, 1.0, 0.0)


# ---- meshes --------------------------------------------------------------------------------------------------
class Palette:
    def __init__(self):
        self.cols, self.index = [], {}

    def __call__(self, c):
        k = tuple(round(x, 4) for x in c)
        if k not in self.index:
            self.index[k] = len(self.cols)
            self.cols.append(k)
        return self.index[k]


class Screen:
    """triangles with a normal and a palette colour a vertex; vertices shared where position, normal and colour agree"""

    def __init__(self, pal):
        self.pal, self.key, self.p, self.n, self.c, self.t = pal, {}, [], [], [], []

    def vert(self, p, n, c):
        k = (round(p[0], 4), round(p[1], 4), round(p[2], 4), round(n[0], 2), round(n[1], 2), round(n[2], 2), c)
        if k not in self.key:
            self.key[k] = len(self.p)
            self.p.append(p); self.n.append(n); self.c.append(c)
        return self.key[k]

    def tri(self, a, b, c, col, item, n=None):
        fn = unit(cross(sub(b, a), sub(c, a)))
        if fn is None:
            return
        n = unit(n) if n is not None else fn
        ci = ACCENT if col == ACCENT else self.pal(col)
        self.t.append((self.vert(a, n, ci), self.vert(b, n, ci), self.vert(c, n, ci), item))

    def quad(self, a, b, c, d, col, item, n=None):
        self.tri(a, b, c, col, item, n)
        self.tri(a, c, d, col, item, n)


class Casters:
    """the map's casters: an object a building or a prop, positions only"""

    def __init__(self):
        self.objs = []

    def begin(self, name, item):
        self.objs.append({"name": name, "item": item, "p": [], "key": {}, "t": []})

    def vert(self, p):
        o = self.objs[-1]
        k = tuple(round(x, 4) for x in p)
        if k not in o["key"]:
            o["key"][k] = len(o["p"])
            o["p"].append(p)
        return o["key"][k]

    def tri(self, a, b, c):
        if unit(cross(sub(b, a), sub(c, a))) is None:
            return
        self.objs[-1]["t"].append((self.vert(a), self.vert(b), self.vert(c)))

    def quad(self, a, b, c, d):
        self.tri(a, b, c)
        self.tri(a, c, d)


def box_pts(c, size, yaw=0.0, ax=None):
    """a box's corners: c the centre of its base, size (sx, sy, sz), turned by yaw (or along the axes ax = (u, v) on the ground)"""
    hx, hy, hz = size[0] / 2, size[1], size[2] / 2
    if ax is None:
        cs, sn = math.cos(yaw), math.sin(yaw)
        ax = ((cs, 0.0, sn), (-sn, 0.0, cs))
    u, v = ax

    def P(x, y, z): return add(add(c, mul(u, x)), add(mul(UP, y), mul(v, z)))

    return [P(-hx, 0, -hz), P(hx, 0, -hz), P(hx, 0, hz), P(-hx, 0, hz), P(-hx, hy, -hz), P(hx, hy, -hz), P(hx, hy, hz), P(-hx, hy, hz)], u, v


BOX_FACES = ((0, 1, 5, 4, "-v"), (1, 2, 6, 5, "+u"), (2, 3, 7, 6, "+v"), (3, 0, 4, 7, "-u"), (4, 5, 6, 7, "+y"), (0, 3, 2, 1, "-y"))


def box(scr, c, size, col, item, yaw=0.0, ax=None, bottom=False, top=True, sides=True):
    p, u, v = box_pts(c, size, yaw, ax)
    nrm = {"-v": mul(v, -1), "+u": u, "+v": v, "-u": mul(u, -1), "+y": UP, "-y": mul(UP, -1)}
    for a, b, cc, d, f in BOX_FACES:
        if (f == "-y" and not bottom) or (f == "+y" and not top) or (f[1] in "uv" and not sides):
            continue
        scr.quad(p[a], p[b], p[cc], p[d], col, item, nrm[f])


def cbox(cst, c, size, yaw=0.0, ax=None, top=True):
    p, _, _ = box_pts(c, size, yaw, ax)
    for a, b, cc, d, f in BOX_FACES:
        if f == "-y" or (f == "+y" and not top):
            continue
        cst.quad(p[a], p[b], p[cc], p[d])


def prism(scr, c, r, h, n, col, item, top=True, cst=None):
    ring = [(c[0] + r * math.cos(2 * math.pi * i / n), c[2] + r * math.sin(2 * math.pi * i / n)) for i in range(n)]
    for i in range(n):
        a, b = ring[i], ring[(i + 1) % n]
        q = ((a[0], c[1], a[1]), (b[0], c[1], b[1]), (b[0], c[1] + h, b[1]), (a[0], c[1] + h, a[1]))
        mid = unit(((a[0] + b[0]) / 2 - c[0], 0.0, (a[1] + b[1]) / 2 - c[2]))
        if scr is not None:
            scr.quad(q[0], q[1], q[2], q[3], col, item, mid)
            if top:
                scr.tri((c[0], c[1] + h, c[2]), q[3], q[2], col, item, UP)
        if cst is not None:
            cst.quad(*q)


def grid(scr, o, u, v, us, vs, n, col, item, skip=None):
    """quads over o + u a + v b, a in us, b in vs (u, v unit), normal n"""
    for i in range(len(us) - 1):
        for j in range(len(vs) - 1):
            if skip is not None and skip(i, j):
                continue
            def P(a, b): return add(o, add(mul(u, a), mul(v, b)))
            scr.quad(P(us[i], vs[j]), P(us[i + 1], vs[j]), P(us[i + 1], vs[j + 1]), P(us[i], vs[j + 1]), col, item, n)


# ---- the kit's pieces ------------------------------------------------------------------------------------------
class Kit:
    def __init__(self, zip_path, lod_dir):
        self.z = zipfile.ZipFile(zip_path) if zip_path and zip_path != "-" and os.path.exists(zip_path) else None
        self.cache, self.report = {}, {}
        self.decimate = None
        if self.z is not None:
            cur = None
            for line in self.z.read("Modular Village/Materials_Modular_Village.mtl").decode().splitlines():
                p = line.split()
                if p and p[0] == "newmtl":
                    cur = p[1]
                elif p and p[0] == "Kd":
                    KD[cur] = tuple(float(x) for x in p[1:4])
            sys.path.insert(0, os.path.abspath(lod_dir))
            sys.dont_write_bytecode = True  # leave no __pycache__ in third_party (read only here)
            from qem_decimate import decimate  # noqa: E402  (dc-forerunner's, CC0 algorithm, used where it is)
            self.decimate = decimate

    def _load(self, name):
        pos, tri, cur = [], [], None
        for line in self.z.read("Modular Village/%s.obj" % name).decode("utf-8", "replace").splitlines():
            p = line.split()
            if not p:
                continue
            if p[0] == "v":
                pos.append(tuple(float(x) for x in p[1:4]))
            elif p[0] == "usemtl":
                cur = p[1]
            elif p[0] == "f":
                idx = [int(s.split("/")[0]) for s in p[1:]]
                idx = [i - 1 if i > 0 else len(pos) + i for i in idx]
                for k in range(1, len(idx) - 1):
                    tri.append((idx[0], idx[k], idx[k + 1], cur))
        return pos, tri

    @staticmethod
    def _open_edges(pos, tris):
        use = {}
        for t in tris:
            for a, b in ((t[0], t[1]), (t[1], t[2]), (t[2], t[0])):
                pa, pb = tuple(round(c, 5) for c in pos[a]), tuple(round(c, 5) for c in pos[b])
                k = (pa, pb) if pa < pb else (pb, pa)
                use[k] = use.get(k, 0) + 1
        return sum(1 for n in use.values() if n == 1)

    def piece(self, name):
        """(positions, [(a, b, c, material)]) local, kit units: the smallest clean cut, or the piece as it is"""
        if name in self.cache:
            return self.cache[name]
        target, bb, main = KIT[name]
        if self.z is None:                                   # the stand-in: the piece's box
            lo, hi = bb[:3], bb[3:]
            pos = [(x, y, z) for y in (lo[1], hi[1]) for z in (lo[2], hi[2]) for x in (lo[0], hi[0])]
            quads = ((0, 1, 5, 4), (1, 3, 7, 5), (3, 2, 6, 7), (2, 0, 4, 6), (4, 5, 7, 6), (0, 2, 3, 1))
            tris = []
            for a, b, c, d in quads:                         # wound outward
                tris += [(a, d, c, main), (a, c, b, main)]
            self.cache[name] = (pos, tris)
            self.report[name] = {"kit": 0, "tris": len(tris)}
            return self.cache[name]
        pos, tri = self._load(name)
        mats = sorted({t[3] for t in tri})
        best, how = tri, "as is"
        if target and len(tri) > target:
            verts = []
            key = {}
            tris = []
            for a, b, c, m in tri:
                t = []
                for i in (a, b, c):
                    k = (i, m)
                    if k not in key:
                        key[k] = len(verts)
                        verts.append((0, pos[i], (mats.index(m) * 10.0, 0.0)))
                    t.append(key[k])
                tris.append(tuple(t))
            vpos = [v[1] for v in verts]
            open0 = self._open_edges(vpos, tris)
            for md in (0.2, 0.1, 0.05, 0.02):                # the smallest first: the first clean one wins
                kept = self.decimate(verts, tris, target, cross_groups=False, max_deviation=md, lock_open=True)
                got = [(a, b, c, mats[int(verts[a][2][0] / 10.0 + 0.5)]) for a, b, c in kept]
                if sorted({t[3] for t in got}) == mats and self._open_edges(vpos, kept) <= open0:
                    best, how, pos = got, "cut, max deviation %.2f u" % md, vpos
                    break
        self.cache[name] = (pos, best)
        self.report[name] = {"kit": 1, "source_tris": len(tri), "tris": len(best), "how": how}
        return self.cache[name]


# ---- the town ----------------------------------------------------------------------------------------------------
class Town:
    def __init__(self, kit):
        self.kit = kit
        self.pal = Palette()
        self.scr = Screen(self.pal)
        self.cst = Casters()
        self.named = {}

    def col(self, m):
        return STUCCO if m == "Stucco" else KD[m]

    def place(self, name, pos, facing, item, s=U, center=False, recolor=None):
        """a kit piece: its local +X turned to `facing`, scaled by s, origin at pos (world), as the mock-up's place()"""
        v, t = self.kit.piece(name)
        ox = oz = 0.0
        if center:
            ox = 0.5 * (min(a[0] for a in v) + max(a[0] for a in v))
            oz = 0.5 * (min(a[2] for a in v) + max(a[2] for a in v))
        dx, dz = FACE[facing] if isinstance(facing, str) else facing
        w = []
        for lx, ly, lz in v:
            lx -= ox
            lz -= oz
            w.append((pos[0] + s * (lx * dx - lz * dz), pos[1] + s * ly, pos[2] + s * (lx * dz + lz * dx)))
        for a, b, c, m in t:
            col = recolor or self.col(m)
            if not recolor and m == "Wood_Darker" and name not in POSTS:
                col = MID_TIMBER
            self.scr.tri(w[a], w[b], w[c], col, item)

    def frame_piece(self, F, sc, y0, w, h, proud, col_frame, col_in, item, inset=0.12):
        """a window or a door on the wall F(s, y, r): a frame w x h proud of the wall, and its glass or planks on it"""
        s0, s1, y1 = sc - w / 2, sc + w / 2, y0 + h
        r = -proud
        d = sub(F(0, 0, -1), F(0, 0, 0))                     # the wall's outward normal
        al = sub(F(1, 0, 0), F(0, 0, 0))
        self.scr.quad(F(s0, y0, r), F(s1, y0, r), F(s1, y1, r), F(s0, y1, r), col_frame, item, d)
        self.scr.quad(F(s0, y0, 0), F(s0, y0, r), F(s0, y1, r), F(s0, y1, 0), col_frame, item, mul(al, -1))
        self.scr.quad(F(s1, y0, r), F(s1, y0, 0), F(s1, y1, 0), F(s1, y1, r), col_frame, item, al)
        self.scr.quad(F(s0, y1, r), F(s1, y1, r), F(s1, y1, 0), F(s0, y1, 0), col_frame, item, UP)
        self.scr.quad(F(s0, y0, 0), F(s1, y0, 0), F(s1, y0, r), F(s0, y0, r), col_frame, item, mul(UP, -1))
        g = r - 0.012
        self.scr.quad(F(s0 + inset, y0 + inset, g), F(s1 - inset, y0 + inset, g), F(s1 - inset, y1 - inset, g),
                      F(s0 + inset, y1 - inset, g), col_in, item, d)

    def terrace(self, name, x0, z0, along, facing, n, storeys, item, doors=(), arch=None, wide_door=None, chimney=None,
                ends=(True, True), hide=(), ornate=None):
        """n tiles along the facade, `storeys` high, 4 tiles (6 m) deep, a ridged roof along the facade: the front face, the
        end walls drawn (`ends`: the end at s = 0, the end at s = n U), the front slope; the back face and the back slope only
        in the map. `hide`: front tiles inside a neighbour, not drawn."""
        d, al = flat(facing), flat(along)
        base = R(x0, 0.0, z0)
        H = 2 * storeys
        He, D = H * U, 4 * U
        rise, run, over = 2.0 * 1.2 * U * 0.83, D / 2, 0.47
        k = rise / run

        def F(s, y, r=0.0):                                 # s along the facade, y up, r metres back from it
            return add(add(base, mul(al, s)), add(mul(UP, y), mul(d, -r)))

        arch_t = () if arch is None else (arch, arch + 1)
        wide_t = () if wide_door is None else (wide_door, wide_door + 1)
        ss = [i * U for i in range(n + 1)]
        ys = [b * U for b in range(H)] + [(H - 1) * U + 0.375 * q for q in (1, 2, 3, 4)]
        wall = self.col("Stucco")

        def skip(i, j):
            return i in hide or (i in arch_t and ys[j + 1] <= 3 * U + 1e-6)

        o, u, v = F(0, 0), al, UP
        grid(self.scr, o, u, v, ss, ys, d, wall, item, skip)
        # windows (the mock-up's rule: odd tiles, from the second block pair up, not in the top block), doors
        for i in range(n):
            if i in hide:
                continue
            h = 2
            while h + 1 < H:
                if i % 2 == 1 and not (i in arch_t and h < 3):
                    self.frame_piece(F, (i + 0.5) * U, h * U + 0.65, 0.8, 1.7, 0.08, MID_TIMBER, GLASS, item)
                h += 2
            if i in doors:
                self.frame_piece(F, (i + 0.5) * U, 0.0, 1.05, 2.3, 0.06, MID_TIMBER, KD["Wood_Dark"], "doors", inset=0.1)
        if wide_door is not None:
            self.frame_piece(F, (wide_door + 1) * U, 0.0, 2.4, 2.7, 0.08, MID_TIMBER, ornate or KD["Wood_Dark"], "doors", inset=0.12)
        # the arch: the kit's piece on the centre line, the passage's walls and ceiling behind it
        if arch is not None:
            lz = (-d[2], d[0])
            tgt = arch if (lz[0] * al[0] + lz[1] * al[2]) < 0 else arch + 1
            at = F((tgt + 0.5) * U, 0.0, 0.5 * U)
            self.place("Stone_Arch_Outer", at, (d[0], d[2]), item)
            st = self.col("Stone_Brit_Gray")
            rs = [U * q for q in (1, 2, 3, 4)]
            hs = [0.0, U, 2 * U, 3 * U]
            s0, s1 = arch * U, (arch + 2) * U
            for j in range(3):
                for q in range(3):
                    self.scr.quad(F(s0, hs[j], rs[q]), F(s0, hs[j], rs[q + 1]), F(s0, hs[j + 1], rs[q + 1]), F(s0, hs[j + 1], rs[q]), st, item, al)
                    self.scr.quad(F(s1, hs[j], rs[q + 1]), F(s1, hs[j], rs[q]), F(s1, hs[j + 1], rs[q]), F(s1, hs[j + 1], rs[q + 1]), st, item, mul(al, -1))
            for q in range(3):
                self.scr.quad(F(s0, 3 * U, rs[q]), F(s0, 3 * U, rs[q + 1]), F(s1, 3 * U, rs[q + 1]), F(s1, 3 * U, rs[q]),
                              self.col("Stone_Brit_Gray_Dark"), item, mul(UP, -1))
        # the end walls, with their gables
        rr = [0.0, U, 2 * U, 3 * U, 4 * U]
        yb = [b * U for b in range(H + 1)]
        for e, (on, s, sg) in enumerate(((ends[0], 0.0, -1.0), (ends[1], n * U, 1.0))):
            if not on:
                continue
            nn = mul(al, sg)
            grid(self.scr, F(s, 0, 0), mul(d, -1), UP, rr, yb, nn, wall, item)
            self.scr.tri(F(s, He, 0), F(s, He, D), F(s, He + rise, run), wall, item, nn)
        # the front slope: from the eave (over the wall by `over`) to the ridge, 2 rows, a cell a tile
        roof = self.col("Red_Roof")
        nr = unit(add(UP, mul(d, k)))
        e0 = (-over, He - over * k)
        for i in range(n):
            pts = [(e0[0] + (run - e0[0]) * t, e0[1] + (He + rise - e0[1]) * t) for t in (0.0, 0.5, 1.0)]
            for q in range(2):
                (ra, ya), (rb, yb2) = pts[q], pts[q + 1]
                self.scr.quad(F(ss[i], ya, ra), F(ss[i + 1], ya, ra), F(ss[i + 1], yb2, rb), F(ss[i], yb2, rb), roof, item, nr)
        # the chimney: a stone stack behind the ridge
        cb = None
        if chimney is not None:
            cb = F((chimney + 0.5) * U, He + 1.0, 2.5 * U)
            box(self.scr, cb, (0.75, rise + 0.4, 0.75), self.col("Stone_Brit_Gray"), item, ax=(al, mul(d, -1)))
            box(self.scr, add(cb, (0.0, rise + 0.4, 0.0)), (0.9, 0.15, 0.9), self.col("Stone_Brit_Gray_Dark"), item, ax=(al, mul(d, -1)))
        # the map: the house as a box, the roof as a prism over it (eaves front and back), the chimney
        self.cst.begin(name, item)
        cbox(self.cst, F(n * U / 2, 0, D / 2), (n * U, He, D), ax=(al, mul(d, -1)), top=False)
        ef, eb = (-over, He - over * k), (D + over, He - over * k)
        for s_a, s_b in ((0.0, n * U),):
            self.cst.quad(F(s_a, ef[1], ef[0]), F(s_b, ef[1], ef[0]), F(s_b, He + rise, run), F(s_a, He + rise, run))
            self.cst.quad(F(s_b, eb[1], eb[0]), F(s_a, eb[1], eb[0]), F(s_a, He + rise, run), F(s_b, He + rise, run))
            self.cst.tri(F(s_a, He, 0), F(s_a, He, D), F(s_a, He + rise, run))
            self.cst.tri(F(s_b, He, 0), F(s_b, He, D), F(s_b, He + rise, run))
        if cb is not None:
            cbox(self.cst, cb, (0.75, rise + 0.55, 0.75), ax=(al, mul(d, -1)))
        return F

    def build(self):
        H_ = "houses and hall"
        # North: NW houses (2), the guildhall (4, the arch on the centre line), the NE house (3). Facades at z 12, facing south.
        self.terrace("NW houses", -18.0, 12.0, "E", "S", 8, 2, H_, doors=(5,), ends=(False, False), hide=(0, 1, 2, 3))
        self.terrace("guildhall", -6.0, 12.0, "E", "S", 8, 4, H_, arch=3, chimney=0)
        F_ne = self.terrace("NE house", 6.0, 12.0, "E", "S", 8, 3, H_, doors=(3,), chimney=6, ends=(False, False), hide=(4, 5, 6, 7))
        # West: SW houses (2), the tavern (3), the corner house (2). Facades at x -12, facing east; tiles run north.
        self.terrace("SW ring houses", -12.0, -12.0, "N", "E", 7, 2, H_, doors=(2, 5), ends=(True, False))
        self.terrace("tavern", -12.0, -1.5, "N", "E", 6, 3, H_, wide_door=2, chimney=4, ornate=KD["Wood_Dark"])
        self.terrace("corner house", -12.0, 7.5, "N", "E", 3, 2, H_, ends=(False, False))
        # East: behind the market, facades at x 12 facing west; tiles run north.
        self.terrace("E houses south", 12.0, -12.0, "N", "W", 8, 2, H_, doors=(1, 6), ends=(True, False))
        self.terrace("E houses north", 12.0, 0.0, "N", "W", 8, 3, H_, doors=(2, 6), ends=(True, False))

        # Tavern dressing: the ale-stake (the kit's sign bracket, a green bush), two lantern brackets.
        self.place("Wall_Prop_Sign_1", R(-12.0, 3.6, 4.5), "E", "tavern sign")
        box(self.scr, R(-10.6, 3.2, 4.5), (0.6, 0.7, 0.6), (0.33, 0.47, 0.22), "tavern sign", bottom=True)
        self.cst.begin("tavern sign", "tavern sign")
        cbox(self.cst, R(-11.15, 3.6, 4.5), (1.7, 0.25, 0.1))
        cbox(self.cst, R(-10.6, 3.2, 4.5), (0.6, 0.7, 0.6))
        for zl in (0.2, 6.6):
            self.place("Wall_Prop_Lamp", R(-12.0, 2.4, zl), "E", "lantern brackets")
        self.cst.begin("lantern brackets", "lantern brackets")
        for zl in (0.2, 6.6):
            cbox(self.cst, R(-11.8, 2.95, zl), (0.5, 0.75, 0.3))
        # The NE house: the ladder against the lit south face (the rungs' stripes are route B's self-shadow test).
        self.place("Prop_Ladder_1", R(8.0, 0, 11.2), "S", "ladder", center=True)
        self.cst.begin("ladder", "ladder")

        def LP(t, x):                                        # along the ladder: its foot (story z 10.45) to its top on the wall
            return R(x, 5.85 * t, 10.45 + 1.52 * t)
        for xr in (7.55, 8.45):
            self.cst.quad(LP(0, xr - 0.03), LP(0, xr + 0.03), LP(1, xr + 0.03), LP(1, xr - 0.03))
        for q in range(12):
            t = (0.3 + 0.45 * q) / 5.85
            self.cst.quad(LP(t - 0.004, 7.55), LP(t - 0.004, 8.45), LP(t + 0.004, 8.45), LP(t + 0.004, 7.55))
        _ = F_ne

        # The church tower, south-west beyond the open corner (built: faces in 3 m cells, belfry openings, a pyramid roof).
        tw = R(-27.0, 0, -31.0)
        stone = tuple(min(1.0, 1.18 * x) for x in KD["Stone_Bri_Gray_Lighter"])
        for kf in range(4):
            a = kf * math.pi / 2
            nrm = (math.cos(a), 0.0, math.sin(a))
            tan = (-math.sin(a), 0.0, math.cos(a))
            o = add(add(tw, mul(nrm, 3.0)), mul(tan, -3.0))
            grid(self.scr, o, tan, UP, [0.0, 3.0, 6.0], [3.0 * q for q in range(9)], nrm, stone, "tower")
            c0 = add(tw, mul(nrm, 3.01))
            self.scr.quad(add(add(c0, mul(tan, -0.9)), (0, 19.0, 0)), add(add(c0, mul(tan, 0.9)), (0, 19.0, 0)),
                          add(add(c0, mul(tan, 0.9)), (0, 22.0, 0)), add(add(c0, mul(tan, -0.9)), (0, 22.0, 0)),
                          KD["Stone_Brit_Gray_Dark"], "tower", nrm)
        apex = (tw[0], 28.0, tw[2])
        pc = [(tw[0] - 3.2, 24.0, tw[2] - 3.2), (tw[0] + 3.2, 24.0, tw[2] - 3.2), (tw[0] + 3.2, 24.0, tw[2] + 3.2), (tw[0] - 3.2, 24.0, tw[2] + 3.2)]
        for kf in range(4):
            self.scr.tri(pc[(kf + 1) % 4], pc[kf], apex, KD["Dark_Red_Roof"], "tower")
        self.cst.begin("tower", "tower")
        self.named["tower"] = len(self.cst.objs) - 1
        cbox(self.cst, tw, (6.0, 24.0, 6.0), top=False)
        for kf in range(4):
            self.cst.tri(pc[(kf + 1) % 4], pc[kf], apex)

        # The south-west fill in the sun-line frame: t from the tower along bearing 56, n to its right (bearing 326).
        def SL(t, n, y=0.0):
            return R(-27.0 + 0.829 * t - 0.559 * n, y, -31.0 + 0.559 * t + 0.829 * n)
        T_HAT, N_HAT = (0.829, -0.559), (-0.559, -0.829)    # world frame
        nax = (flat(N_HAT), flat((-N_HAT[1], N_HAT[0])))     # a box's x along n, its z across
        wall_s = tuple(min(1.0, 1.51 * x) for x in KD["Stone_Brit_Gray"])
        wall_c = tuple(min(1.0, 1.15 * x) for x in wall_s)
        self.cst.begin("churchyard wall", "churchyard wall")
        for n0, n1 in ((-12.0, -1.5), (1.5, 4.5)):
            c = SL(11.0, (n0 + n1) / 2)
            box(self.scr, c, (n1 - n0, 1.4, 0.5), wall_s, "churchyard wall", ax=nax)
            box(self.scr, (c[0], 1.4, c[2]), (n1 - n0 + 0.04, 0.1, 0.56), wall_c, "churchyard wall", ax=nax)
            cbox(self.cst, c, (n1 - n0, 1.5, 0.5), ax=nax)
        for nn in (-1.5, 1.5):                               # gate posts, eight-sided, on a square base
            p = SL(11.0, nn)
            box(self.scr, p, (0.7, 0.3, 0.7), wall_s, "churchyard wall", ax=nax)
            prism(self.scr, (p[0], 0.3, p[2]), 0.3, 1.7, 8, wall_c, "churchyard wall", cst=self.cst)
        # Houses 1 and 2: 5 tiles along t, 4 deep (n 6 to 12), the facade on the n = 6 side; house 1 three storeys. The two
        # meet at t = 2: only house 1's (taller) end wall is drawn there, so no two faces share that plane.
        face_sl = (-N_HAT[0], -N_HAT[1])
        p1, p2 = SL(-5.5, 6.0), SL(2.0, 6.0)
        self.terrace("house 1", p1[0], -p1[2], T_HAT, face_sl, 5, 3, "SW houses", chimney=1)
        F2 = self.terrace("house 2", p2[0], -p2[2], T_HAT, face_sl, 5, 2, "SW houses", chimney=3, ends=(False, True))
        # house 2's door on the gable that faces the square (t = 9.5): the end wall's own frame (across it the house's depth,
        # out of it along t), the door at the middle of the depth
        self.frame_piece(lambda s, y, r: F2(5 * U - r, y, s), 3.0, 0.0, 1.05, 2.3, 0.06, MID_TIMBER, KD["Wood_Dark"], "doors", inset=0.1)

        # ---- the open square ----
        self.place("Prop_Well_1", R(-2.0, 0, -7.0), "E", "well", center=True)
        self.cst.begin("well", "well")
        wc = R(-2.0, 0, -7.0)
        prism(None, wc, 0.85, 0.8, 8, None, None, cst=self.cst)
        for dx in (-0.85, 0.75):
            cbox(self.cst, (wc[0] + dx, 0.8, wc[2]), (0.12, 1.2, 0.12))
        rp = [(wc[0] - 1.1, 1.9, wc[2] - 0.85), (wc[0] + 0.95, 1.9, wc[2] - 0.85), (wc[0] + 0.95, 1.9, wc[2] + 0.85), (wc[0] - 1.1, 1.9, wc[2] + 0.85)]
        rg = [(wc[0] - 1.1, 2.3, wc[2]), (wc[0] + 0.95, 2.3, wc[2])]
        self.cst.quad(rp[0], rp[1], rg[1], rg[0])
        self.cst.quad(rp[3], rg[0], rg[1], rp[2])
        # Market cross at (4, 0): a step, an eight-sided shaft 5 m, a head with arms.
        c = R(4.0, 0, 0.0)
        box(self.scr, c, (2.0, 0.35, 2.0), KD["Stone_Brit_Gray_Light"], "market cross")
        box(self.scr, (c[0], 0.35, c[2]), (1.3, 0.3, 1.3), KD["Stone_Brit_Gray"], "market cross")
        prism(self.scr, (c[0], 0.65, c[2]), 0.2, 4.35, 8, KD["Stone_Bri_Gray_Lighter"], "market cross")
        box(self.scr, (c[0], 4.4, c[2]), (0.9, 0.18, 0.18), KD["Stone_Bri_Gray_Lighter"], "market cross", bottom=True)
        box(self.scr, (c[0], 5.0, c[2]), (0.22, 0.45, 0.22), KD["Stone_Bri_Gray_Lighter"], "market cross")
        self.cst.begin("market cross", "market cross")
        cbox(self.cst, c, (2.0, 0.65, 2.0))
        cbox(self.cst, (c[0], 0.65, c[2]), (0.36, 4.8, 0.36))
        cbox(self.cst, (c[0], 4.4, c[2]), (0.9, 0.18, 0.18))
        # Pennant lines A (z -6, 4.2 m) and B (z -1, 4.6 m): 32 pennants a line drawn; the rope in the map only.
        self.cst.begin("pennant lines", "pennant lines")
        for zl, yl in ((-6.0, 4.2), (-1.0, 4.6)):
            a, b = R(-12.0, yl - 0.02, zl), R(12.0, yl - 0.02, zl)
            self.cst.quad(a, b, add(b, (0, 0.04, 0)), add(a, (0, 0.04, 0)))
            for q in range(32):
                x = -11.625 + 0.75 * q
                tri = (R(x - 0.2, yl, zl), R(x + 0.2, yl, zl), R(x, yl - 0.5, zl))
                self.scr.tri(tri[0], tri[1], tri[2], ACC[q % 4], "pennant lines", (0.0, 0.0, 1.0))
                self.cst.tri(*tri)
        # Market stalls (east side), opening west: four posts, a 2 x 2 canopy (the kit's), a counter.
        for si, zc in enumerate((6.0, 2.0, -2.0, -6.0)):
            self.cst.begin("stall %d" % (si + 1), "stalls")
            for px in (9.15, 11.85):
                for pz in (zc - 1.4, zc + 1.4):
                    box(self.scr, R(px, 0, pz), (0.14, 2.3, 0.14), KD["Wood_Darker"], "stalls", top=False)
                    cbox(self.cst, R(px, 0, pz), (0.14, 2.3, 0.14), top=False)
            for tx in (9.75, 11.25):
                for tz in (zc - 0.75, zc + 0.75):
                    self.place("Canopy_Top", R(tx, 2.25 - 1.21 * U + 0.02, tz), "W", "stalls", center=True)
            self.cst.quad(R(9.0, 2.27, zc - 1.5), R(12.0, 2.27, zc - 1.5), R(12.0, 2.27, zc + 1.5), R(9.0, 2.27, zc + 1.5))
            box(self.scr, R(9.85, 0, zc), (0.8, 0.95, 2.6), KD["Wood_Medium"], "counters")
            cbox(self.cst, R(9.85, 0, zc), (0.8, 0.95, 2.6))
        # stall 1, the clothier: three hung cloth strips at the canopy's front
        self.cst.begin("hung cloth", "hung cloth")
        for q, dz in enumerate((-0.9, 0.0, 0.9)):
            qd = (R(9.2, 2.2, 6.0 + dz - 0.3), R(9.2, 2.2, 6.0 + dz + 0.3), R(9.2, 1.1, 6.0 + dz + 0.3), R(9.2, 1.1, 6.0 + dz - 0.3))
            self.scr.quad(*qd, ACC[q], "hung cloth", (-1.0, 0.0, 0.0))
            self.cst.quad(*qd)
        # stall 2, the poulterer: the hen pen in front, (7.75 .. 9.25) x (-1 .. 2): four posts, six ropes
        self.cst.begin("hen pen", "hen pen")
        for px, pz in ((7.75, -1.0), (9.25, -1.0), (7.75, 2.0), (9.25, 2.0)):
            self.place("Wood_Post_Small", R(px, 0, pz), "E", "hen pen", center=True)
            cbox(self.cst, R(px, 0, pz), (0.27, 0.83, 0.27), top=False)
        for (ax_, az), (bx, bz) in (((7.75, -1.0), (7.75, 0.5)), ((7.75, 0.5), (7.75, 2.0)), ((9.25, -1.0), (9.25, 0.5)), ((9.25, 0.5), (9.25, 2.0)),
                                    ((7.75, -1.0), (9.25, -1.0)), ((7.75, 2.0), (9.25, 2.0))):
            mx, mz = (ax_ + bx) / 2, (az + bz) / 2
            self.place("Wood_Rope_Straight", R(mx, 0.14 * U, mz), "E" if az == bz else "N", "hen pen", center=True)
            a, b = R(ax_, 0.66, az), R(bx, 0.66, bz)
            self.cst.quad(a, b, add(b, (0, 0.06, 0)), add(a, (0, 0.06, 0)))
        # stall 3, produce: three open crates (half scale) with produce tops, the porter's sacks
        self.cst.begin("crates", "crates")
        for q, dz in enumerate((-0.9, 0.0, 0.9)):
            box(self.scr, R(9.0, 0, -2.0 + dz), (0.55, 0.5, 0.55), KD["Wood_Medium"], "crates", top=False)   # open: 4 sides
            self.scr.quad(R(8.75, 0.45, -2.0 + dz - 0.25), R(9.25, 0.45, -2.0 + dz - 0.25), R(9.25, 0.45, -2.0 + dz + 0.25),
                          R(8.75, 0.45, -2.0 + dz + 0.25), [(0.62, 0.20, 0.16), (0.36, 0.52, 0.22), (0.62, 0.20, 0.16)][q], "crates", UP)
            cbox(self.cst, R(9.0, 0, -2.0 + dz), (0.55, 0.55, 0.55))
        self.cst.begin("sacks", "sacks")
        for sx, sz, sy in ((11.0, -3.3, 0.0), (11.5, -3.3, 0.0), (11.25, -3.3, 0.38)):
            box(self.scr, R(sx, sy, sz), (0.45, 0.38, 0.6), KD["Burlap"], "sacks")
            cbox(self.cst, R(sx, sy, sz), (0.45, 0.38, 0.6))
        # stall 4, the cooper: eight barrels
        self.cst.begin("barrels", "barrels")
        for bx, bz in ((8.6, -5.0), (8.6, -5.8), (8.6, -6.6), (9.3, -7.2), (10.6, -7.1), (11.3, -7.1), (10.6, -4.9), (11.3, -4.9)):
            self.place("Prop_Barrel_2", R(bx, 0, bz), "E", "barrels", center=True)
            prism(None, R(bx, 0, bz), 0.36, 0.87, 6, None, None, cst=self.cst)


def movers(pal):
    """the movers' templates, local to their feet, facing -z: (screen, casters) for the person and the hen"""
    out = []
    p = Screen(pal)
    pc = Casters()
    pc.begin("person", "people")
    for c, size, col in (((0, 0, 0), (0.46, 1.05, 0.30), PERSON_GREY), ((0, 1.05, 0), (0.46, 0.40, 0.30), ACCENT),
                         ((0, 1.45, 0), (0.24, 0.25, 0.24), PERSON_GREY)):
        box(p, c, size, col, "people")
        cbox(pc, c, size)
    out.append((p, pc.objs[0]))
    h = Screen(pal)
    hc = Casters()
    hc.begin("hen", "hens")
    for c, size, col, cast in (((0, 0.10, 0.02), (0.22, 0.20, 0.34), HEN_WHITE, 1), ((0, 0.24, -0.17), (0.11, 0.13, 0.11), HEN_WHITE, 1),
                               ((0, 0.37, -0.17), (0.03, 0.05, 0.07), HEN_COMB, 0), ((0, 0.28, -0.24), (0.04, 0.03, 0.05), HEN_LEG, 0),
                               ((0, 0.22, 0.18), (0.16, 0.15, 0.08), HEN_WHITE, 0), ((-0.05, 0.0, 0.02), (0.03, 0.10, 0.03), HEN_LEG, 0),
                               ((0.05, 0.0, 0.02), (0.03, 0.10, 0.03), HEN_LEG, 0)):
        box(h, c, size, col, "hens")
        if cast:
            cbox(hc, c, size)
    out.append((h, hc.objs[0]))
    return out


# ---- strips --------------------------------------------------------------------------------------------------------
def stripify(tris):
    """greedy strips: triangle j of a strip is its vertices j, j + 1, j + 2 (the PVR's culling is off, so winding is free)"""
    edges = {}
    for i, t in enumerate(tris):
        for a, b in ((t[0], t[1]), (t[1], t[2]), (t[2], t[0])):
            edges.setdefault((min(a, b), max(a, b)), []).append(i)
    used = [False] * len(tris)

    def grow(s, mark):
        taken = []
        while True:
            p, q = s[-2], s[-1]
            nxt = None
            for j in edges.get((min(p, q), max(p, q)), ()):
                if not used[j] and j not in mark:
                    nxt = j
                    break
            if nxt is None:
                return taken
            r = [x for x in tris[nxt][:3] if x != p and x != q][0]
            s.append(r)
            mark.add(nxt)
            taken.append(nxt)

    strips = []
    for i, t in enumerate(tris):
        if used[i]:
            continue
        best = None
        for rot in ((t[0], t[1], t[2]), (t[1], t[2], t[0]), (t[2], t[0], t[1])):
            s = list(rot)
            mark = {i}
            taken = grow(s, mark)
            if best is None or len(taken) > len(best[1]):
                best = (s, taken)
        used[i] = True
        for j in best[1]:
            used[j] = True
        strips.append(best[0])
    return strips


# ---- out -------------------------------------------------------------------------------------------------------------
def c_floats(rows, fmt="%.4ff"):
    return ",\n".join("    { " + ", ".join(fmt % x for x in r) + " }" for r in rows)


def c_ints(vals, per=24):
    return ",\n".join("    " + ", ".join(str(v) for v in vals[i:i + per]) for i in range(0, len(vals), per))


def write_screen(f, prefix, scr, strips):
    sv = [v for s in strips for v in s]
    f.write("#define %s_V %du\n#define %s_SV %du\n#define %s_S %du\n#define %s_T %du\n"
            % (prefix.upper(), len(scr.p), prefix.upper(), len(sv), prefix.upper(), len(strips), prefix.upper(), len(sv) - 2 * len(strips)))
    f.write("static const float %s_v[%s_V][3] = {\n%s\n};\n" % (prefix, prefix.upper(), c_floats(scr.p)))
    f.write("static const signed char %s_n[%s_V][3] = {\n%s\n};\n" % (prefix, prefix.upper(),
            c_floats([[int(round(x * 127.0)) for x in n] for n in scr.n], "%d")))
    f.write("static const unsigned char %s_c[%s_V] = {\n%s\n};\n" % (prefix, prefix.upper(), c_ints(scr.c)))
    f.write("static const unsigned short %s_sv[%s_SV] = {\n%s\n};\n" % (prefix, prefix.upper(), c_ints(sv)))
    f.write("static const unsigned short %s_sl[%s_S] = {\n%s\n};\n" % (prefix, prefix.upper(), c_ints([len(s) for s in strips])))


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    lod = argv[argv.index("--lod") + 1] if "--lod" in argv else LOD_DEFAULT
    rep_path = argv[argv.index("--json") + 1] if "--json" in argv else None
    kit = Kit(argv[1], lod)
    town = Town(kit)
    town.build()
    mv = movers(town.pal)
    if len(town.pal.cols) > 250 or len(town.scr.p) > 65535:
        print("bake_town: too many colours or vertices")
        return 1
    strips = stripify(town.scr.t)
    acc = [town.pal(c) for c in PEOPLE_ACC]
    # the items, for the report: screen triangles and casters
    items = {}
    for t in town.scr.t:
        items.setdefault(t[3], [0, 0])[0] += 1
    for o in town.cst.objs:
        items.setdefault(o["item"], [0, 0])[1] += len(o["t"])
    cv, ct, co = [], [], []
    for o in town.cst.objs:
        base, t0 = len(cv), len(ct)
        cv += o["p"]
        ct += [(a + base, b + base, c + base) for a, b, c in o["t"]]
        co.append((base, len(o["p"]), t0, len(o["t"])))
    sv_n = sum(len(s) for s in strips)
    rep = {
        "kit": 1 if kit.z is not None else 0,
        "screen": {"vertices": len(town.scr.p), "triangles": len(town.scr.t), "strips": len(strips), "strip_vertices": sv_n},
        "casters": {"vertices": len(cv), "triangles": len(ct), "objects": len(co)},
        "items": {k: {"screen": v[0], "map": v[1]} for k, v in sorted(items.items())},
        "pieces": kit.report,
        "movers": {"person": {"screen": len(mv[0][0].t), "map": len(mv[0][1]["t"])}, "hen": {"screen": len(mv[1][0].t), "map": len(mv[1][1]["t"])}},
        "palette": len(town.pal.cols),
    }
    with open(argv[2], "w", encoding="utf-8", newline="\n") as f:
        f.write("/* Generated by demo/psm/tools/bake_town.py. Not in the repository: the build folder.\n")
        f.write("   Kit: Modular Village by Keith at Fertile Soil Productions (CC0), %s.\n" % ("read from the zip" if kit.z is not None else "NOT USED: stand-in boxes"))
        f.write("   Props cut by qem_decimate.py (halo-64 code, CC0) from third_party/dc-forerunner/tools/lod.\n")
        for k, v in sorted(items.items()):
            f.write("   %-22s screen %5d  map %4d\n" % (k, v[0], v[1]))
        f.write("*/\n#define PSM_TOWN_KIT %d\n" % rep["kit"])
        f.write("#define PSM_TOWN_PAL %du\nstatic const float psm_town_pal[PSM_TOWN_PAL][3] = {\n%s\n};\n" % (len(town.pal.cols), c_floats(town.pal.cols)))
        f.write("static const unsigned char psm_town_acc[8] = { %s };   /* the cast's accents, psm_town.h's order */\n" % ", ".join(map(str, acc)))
        write_screen(f, "psm_town", town.scr, strips)
        f.write("#define PSM_TOWN_CV %du\n#define PSM_TOWN_CT %du\n#define PSM_TOWN_CO %du\n#define PSM_TOWN_CO_TOWER %du\n"
                % (len(cv), len(ct), len(co), town.named["tower"]))
        f.write("static const float psm_town_cv[PSM_TOWN_CV][3] = {\n%s\n};\n" % c_floats(cv))
        f.write("static const unsigned short psm_town_ct[PSM_TOWN_CT][3] = {\n%s\n};\n" % c_floats(ct, "%d"))
        f.write("static const unsigned short psm_town_co[PSM_TOWN_CO][4] = {   /* first vertex, vertices, first triangle, triangles */\n%s\n};\n"
                % c_floats(co, "%d"))
        for name, (s, c) in zip(("psm_town_person", "psm_town_hen"), mv):
            write_screen(f, name, s, stripify(s.t))
            f.write("#define %s_CV %du\n#define %s_CT %du\n" % (name.upper(), len(c["p"]), name.upper(), len(c["t"])))
            f.write("static const float %s_cv[%s_CV][3] = {\n%s\n};\n" % (name, name.upper(), c_floats(c["p"])))
            f.write("static const unsigned short %s_ct[%s_CT][3] = {\n%s\n};\n" % (name, name.upper(), c_floats(c["t"], "%d")))
    if rep_path:
        with open(rep_path, "w", encoding="utf-8") as f:
            json.dump(rep, f, indent=1)
    print("bake_town: kit %s; screen %d triangles, %d vertices, %d strips (%d strip vertices); map %d triangles in %d objects; %d colours"
          % ("yes" if rep["kit"] else "NO (stand-ins)", len(town.scr.t), len(town.scr.p), len(strips), sv_n, len(ct), len(co), len(town.pal.cols)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
