#!/usr/bin/env python3
"""Build the walled square (the town's replacement) into the same C header bake_town.py writes, psm_town_mesh.h.

  python -I demo/psm/tools/bake_square.py <kenney_fantasy-town-kit_2.0.zip | -> <out.h> [--json <report.json>] [--closed]

--closed (the small world, the turn plan's world step): the square shut all round. The north gate and the south street's
gap get gates (a door set in the reveal, cast in the map too), the south walls lose their outer faces, and the faces no eye
in the walk can see are left out of the screen mesh: tops above the eye on the solid pieces, the solid pieces' faces that
turn away from the whole walk, and whatever outside the walls sits under every line of sight over them. The map's casters
are all kept (the keep and the towers shade the square). The header then also has PSM_TOWN_CLOSED 1 and the ground that
can be seen, PSM_TOWN_BOUND_X0 .. ZS1 (story metres: the walls' outer faces), and psm_town.h takes its closed routes.

A walled square open to the sky: stone walls with merlons round the town's walk, gate towers, a colonnade, a dais,
corner towers, the keep tower where the church tower stood, and in the open the town's own fixed points kept where the
cast and the hens expect them (a fountain where the well stood at story (-2, -7), a stone obelisk where the cross stood
at (4, 0), the four stalls on the east side, the hen pen, the ladder on the north wall). The people and the hens are
bake_town.py's movers, imported, so psm_town.h's walks, hens, presets and sun need no change.

The walls are baked here from quads (they hold the budget). The pieces (pillars, the fountain's basin, the stalls'
awnings, banners, lanterns, fences, the dais's stairs, trees) are the Fantasy Town Kit (2.0)
by Kenney (www.kenney.nl), CC0 1.0, read from its zip (fetch_table.json, a download by hand; never unpacked), coloured
from its colormap at each face's UV, its tones folded onto the square's few, and cut by qem_decimate.py
(third_party/dc-forerunner, used where it is). The map casts boxes for them. With no zip each piece is its box
(PSM_TOWN_KIT 0). Nothing from the kit is in the repository.

Out: bake_town.py's interface exactly (screen strips, palette, accents, casters, movers), plus
  #define PSM_TOWN_NAME "walled square"   the SCENE row's name.
The bake fails when a route of psm_town.h's cast, or the hens' pen, runs through a solid footprint here.
"""
import json
import math
import os
import re
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.dont_write_bytecode = True
import bake_town as bt  # noqa: E402  (the shared mesh helpers and the movers: imported, not copied)

R, add, mul, UP = bt.R, bt.add, bt.mul, bt.UP

# ---- the look ---------------------------------------------------------------------------------------------------
STONE = [(0.56, 0.55, 0.52), (0.50, 0.49, 0.47), (0.61, 0.59, 0.55)]   # three block tones, picked by a hash a cell
STONE_DK = (0.42, 0.41, 0.40)              # plinths, the keep, the obelisk's shaft
STONE_TOP = (0.66, 0.64, 0.60)             # tops in the sun
WOOD, WOOD_DK, WOOD_LT = (0.47, 0.35, 0.23), (0.30, 0.22, 0.15), (0.60, 0.45, 0.30)
IRON = (0.24, 0.24, 0.26)
FLAME = (1.0, 0.76, 0.36)
DARK = (0.10, 0.10, 0.11)                  # a passage's far end
WATER = (0.22, 0.33, 0.40)                 # the fountain's water
BANNER = [bt.ACC[0], bt.ACC[1]]
CLOTH = bt.ACC

WH, WT = 6.0, 1.0                          # wall height and thickness, metres
U = 1.5                                    # a facade cell
MER_W, MER_H = 0.75, 0.8                   # a merlon: width, height (one a cell)

# the courtyard: inner faces at these story lines
XW, XE, ZS, ZN = -12.0, 13.5, -16.5, 12.0
GATE_N = (-1.5, 1.5)                       # the north gate, story x
GATE_S = (-3.0, 6.0)                       # the south gap (the street out)
CLOSED = False                             # --closed: main() sets it before the build

# --closed: the eye (psm_scene.h PSM_EYE_H) and the closed walk (psm_town.h's PSM_TOWN_CLOSED walk area), story metres
EYE_H = 1.7
WALK_CLOSED = (-11.4, -15.5, 7.2, 11.4)    # x0, z0, x1, z1
WALL_LOW = 4.5                             # the lowest wall top (the broken stretch, the gates' lintels): a sight line must clear it
SOLID = ("wall", "towers", "keep", "gate", "buttresses", "colonnade")   # items made of closed solids (an item's name contains one)


# ---- the kit: Kenney's Fantasy Town Kit 2.0 (CC0), read from its zip, never unpacked ------------------------------
FT_DIR = "Models/OBJ format/"
LOD_DIR = os.path.join(HERE, "..", "..", "..", "third_party", "dc-forerunner", "tools", "lod")
FT_DEV = (0.08, 0.04, 0.02)   # a cut's largest error, metres as drawn: the first that keeps the piece closed wins
FT_BB = {   # the pieces' local bounds (x0, y0, z0, x1, y1, z1), kit units: the stand-in boxes when the zip is missing
    "pillar-stone": (-0.08, 0.0, -0.08, 0.08, 1.0, 0.08),
    "fountain-round": (-1.0, 0.0, -1.0, 1.0, 0.28, 1.0),
    "stall-red": (-0.5, 0.0, -0.5, 0.5, 1.24, 0.5),
    "stall-green": (-0.5, 0.0, -0.5, 0.5, 1.24, 0.5),
    "banner-red": (0.38, 0.16, -0.25, 0.42, 1.0, 0.25),
    "banner-green": (0.38, 0.16, -0.25, 0.42, 1.0, 0.25),
    "lantern": (-0.11, 0.0, -0.11, 0.11, 1.56, 0.11),
    "fence": (0.42, 0.0, -0.5, 0.5, 0.38, 0.5),
    "stairs-wide-stone": (-0.52, 0.0, -0.5, 0.5, 1.0, 0.5),
    "tree-high-round": (-0.51, 0.0, -0.51, 0.51, 2.75, 0.51),
}


def png_rgba(data):
    """an 8-bit RGBA or RGB PNG (no interlace) as rows of (r, g, b) 0 .. 1: zlib and the five row filters, no PIL"""
    import struct
    import zlib
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    pos, idat, w = 8, b"", 0
    while pos < len(data):
        ln, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + ln]
        if kind == b"IHDR":
            w, h, depth, ctype, _, _, lace = struct.unpack(">IIBBBBB", body)
            assert depth == 8 and ctype in (2, 6) and lace == 0, "png: 8-bit RGB or RGBA only"
            bpp = 4 if ctype == 6 else 3
        elif kind == b"IDAT":
            idat += body
        pos += 12 + ln
    raw, stride, rows, prev = zlib.decompress(idat), w * bpp, [], bytearray(w * bpp)
    for y in range(h):
        f, line = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b, c = prev[i], prev[i - bpp] if i >= bpp else 0
            if f == 1:
                line[i] = (line[i] + a) & 255
            elif f == 2:
                line[i] = (line[i] + b) & 255
            elif f == 3:
                line[i] = (line[i] + ((a + b) >> 1)) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append([(line[x * bpp] / 255.0, line[x * bpp + 1] / 255.0, line[x * bpp + 2] / 255.0) for x in range(w)])
        prev = line
    return rows


class FTKit:
    """a piece: its faces' colours from the colormap at the face's mean UV (the kit's swatches are flat), merged into a
    few tones a piece, then cut by qem_decimate.py (dc-forerunner, used where it is) with the tones as groups"""

    def __init__(self, zip_path):
        import zipfile
        self.z = zipfile.ZipFile(zip_path) if zip_path else None
        self.cache, self.report, self.decimate, self.cmap = {}, {}, None, None
        if self.z is not None:
            self.cmap = png_rgba(self.z.read(FT_DIR + "Textures/colormap.png"))
            sys.path.insert(0, os.path.abspath(LOD_DIR))
            from qem_decimate import decimate  # noqa: E402
            self.decimate = decimate

    def _tex(self, u, v):
        rows = self.cmap
        h, w = len(rows), len(rows[0])
        x = min(w - 1, max(0, int((u % 1.0) * w)))
        y = min(h - 1, max(0, int((1.0 - (v % 1.0)) * h)))
        return rows[y][x]

    def _load(self, name):
        pos, uv, tri, mat, kd = [], [], [], None, {}
        cur = None
        for line in self.z.read(FT_DIR + name + ".mtl").decode("utf-8", "replace").splitlines():
            p = line.split()
            if p and p[0] == "newmtl":
                cur = p[1]
            elif p and p[0] == "Kd":
                kd[cur] = tuple(float(x) for x in p[1:4])
        for line in self.z.read(FT_DIR + name + ".obj").decode("utf-8", "replace").splitlines():
            p = line.split()
            if not p:
                continue
            if p[0] == "v":
                pos.append(tuple(float(x) for x in p[1:4]))
            elif p[0] == "vt":
                uv.append((float(p[1]), float(p[2])))
            elif p[0] == "usemtl":
                mat = p[1]
            elif p[0] == "f":
                ix = [s.split("/") for s in p[1:]]
                vi = [int(s[0]) - 1 for s in ix]
                ti = [int(s[1]) - 1 if len(s) > 1 and s[1] else -1 for s in ix]
                for k in range(1, len(vi) - 1):
                    tri.append(((vi[0], vi[k], vi[k + 1]), (ti[0], ti[k], ti[k + 1]), mat))
        out = []
        for (a, b, c), ts, m in tri:
            if m == "colormap" and min(ts) >= 0:
                u = sum(uv[t][0] for t in ts) / 3.0
                v = sum(uv[t][1] for t in ts) / 3.0
                col = self._tex(u, v)
            else:
                col = kd.get(m, (0.5, 0.5, 0.5))
            out.append((a, b, c, col))
        return pos, out

    @staticmethod
    def _tones(tris, merge=0.06):
        """merge colours closer than `merge` (largest channel) into one, the most used first"""
        count = {}
        for t in tris:
            count[t[3]] = count.get(t[3], 0) + 1
        keep = []
        for c in sorted(count, key=lambda c: -count[c]):
            if not any(max(abs(c[i] - k[i]) for i in range(3)) < merge for k in keep):
                keep.append(c)

        def near(c):
            return min(keep, key=lambda k: sum((c[i] - k[i]) ** 2 for i in range(3)))
        return [(a, b, c, near(col)) for a, b, c, col in tris]

    def piece(self, name, target, ymin=None, scale=1.0, dev=FT_DEV, sheet=False):
        """(positions, [(a, b, c, colour)]) local, kit units; ymin keeps the faces wholly above it (a stall's awning).
        scale: the largest the piece is drawn at, so a cut's error is held in metres (FT_DEV).
        With no zip: the piece's box from FT_BB, stone."""
        key = (name, target, ymin, round(scale, 2), dev, sheet)
        if key in self.cache:
            return self.cache[key]
        if self.z is None:
            bb = list(FT_BB[name])
            if ymin is not None:
                bb[1] = max(bb[1], ymin)
            lo, hi = bb[:3], bb[3:]
            pos = [(x, y, z) for y in (lo[1], hi[1]) for z in (lo[2], hi[2]) for x in (lo[0], hi[0])]
            quads = ((0, 1, 5, 4), (1, 3, 7, 5), (3, 2, 6, 7), (2, 0, 4, 6), (4, 5, 7, 6), (0, 2, 3, 1))
            tris = []
            for a, b, c, d in quads:
                tris += [(a, d, c, STONE[0]), (a, c, b, STONE[0])]
            self.cache[key] = (pos, tris)
            self.report[name] = {"kit": 0, "tris": len(tris)}
            return self.cache[key]
        pos, tri = self._load(name)
        if ymin is not None:
            tri = [t for t in tri if min(pos[i][1] for i in t[:3]) >= ymin]
        tri = self._tones(tri)
        tones = sorted({t[3] for t in tri})
        # sheet: a two-sided sheet (a banner's cloth: each face and its twin wound the other way), which the cut
        # folds away, kept out of the cut as it is (an awning cuts cleanly: not asked there)
        def pk(i):
            return tuple(round(c, 4) for c in pos[i])
        faces = {(pk(a), pk(b), pk(c)) for a, b, c, _ in tri}
        sheet = [] if not sheet else [t for t in tri if (pk(t[0]), pk(t[2]), pk(t[1])) in faces or (pk(t[1]), pk(t[0]), pk(t[2])) in faces
                 or (pk(t[2]), pk(t[1]), pk(t[0])) in faces]
        n_src, tri, pos0 = len(tri), [t for t in tri if t not in sheet], pos
        best, how = tri, "as is"
        if target and len(tri) > target:
            verts, vkey, tris = [], {}, []
            for a, b, c, col in tri:
                g = tones.index(col)
                t = []
                for i in (a, b, c):
                    if (i, g) not in vkey:
                        vkey[(i, g)] = len(verts)
                        verts.append((0, pos[i], (g * 10.0, 0.0)))
                    t.append(vkey[(i, g)])
                tris.append(tuple(t))
            vpos = [v[1] for v in verts]
            open0 = bt.Kit._open_edges(vpos, tris)
            for md in [d / scale for d in dev]:          # the smallest first: the first clean one wins
                kept = self.decimate(verts, tris, target, cross_groups=False, max_deviation=md, lock_open=True)
                got = [(a, b, c, tones[int(verts[a][2][0] / 10.0 + 0.5)]) for a, b, c in kept]
                if bt.Kit._open_edges(vpos, kept) <= open0:
                    best, how, pos = got, "cut, max deviation %.2f u" % md, vpos
                    break
        if sheet:                                            # the sheet back, on its own vertices
            pos, best = list(pos), list(best)
            for a, b, c, col in sheet:
                best.append((len(pos), len(pos) + 1, len(pos) + 2, col))
                pos += [pos0[a], pos0[b], pos0[c]]
        self.cache[key] = (pos, best)
        self.report[name + ("" if ymin is None else " (y > %g)" % ymin)] = {"kit": 1, "scale": round(scale, 2), "source_tris": n_src,
                                                                              "sheet_tris": len(sheet), "tris": len(best), "how": how, "tones": len(tones)}
        return self.cache[key]


def h2(a, b):
    """a small integer hash, 0 .. 1"""
    v = (a * 73856093) ^ (b * 19349663) ^ 0x5bd1e995
    v = (v ^ (v >> 13)) * 0x5bd1e995 & 0xffffffff
    return ((v ^ (v >> 15)) & 0xffff) / 65535.0


class Square:
    def __init__(self, kit):
        self.kit = kit
        self.pal = bt.Palette()
        self.scr = bt.Screen(self.pal)
        self.scr.given = []               # a triangle a flag: its normal was given (a face's outward side), not its winding's
        tri = self.scr.tri

        def given_tri(a, b, c, col, item, n=None):
            before = len(self.scr.t)
            tri(a, b, c, col, item, n)
            if len(self.scr.t) > before:
                self.scr.given.append(n is not None)
        self.scr.tri = given_tri
        self.cst = bt.Casters()
        self.named = {}
        self.solid = []                   # story footprints (x0, z0, x1, z1) the walkers must keep off

    def foot(self, x0, z0, x1, z1):
        self.solid.append((min(x0, x1), min(z0, z1), max(x0, x1), max(z0, z1)))

    @staticmethod
    def kit_col(c):
        """the kit's tones onto the square's: its blue-grey stone to the warm stone by brightness, its browns to the
        woods, the rest (cloth, leaves, water) kept; a few colours for the whole kit"""
        r, g, b = c
        lum = 0.3 * r + 0.59 * g + 0.11 * b
        if b > r + 0.04 and max(c) - min(c) < 0.3 and b < 0.97:          # stone
            want = 0.40 + (lum - 0.40) * 0.6
            return min(STONE + [STONE_DK, STONE_TOP], key=lambda k: abs(0.3 * k[0] + 0.59 * k[1] + 0.11 * k[2] - want))
        if r > g > b and r - b > 0.2 and r - g < 0.35:                    # wood
            return min((WOOD_DK, WOOD, WOOD_LT), key=lambda k: abs(0.3 * k[0] + 0.59 * k[1] + 0.11 * k[2] - lum * 0.75))
        return tuple(round(x * 8.0) / 8.0 for x in c)                     # the rest, on a coarse grid (fewer colours)

    def kp(self, name, target, at, fwd, scale, item, y=0.0, ymin=None, centre=True, dev=FT_DEV, sheet=False):
        """a kit piece: local +X along the story direction fwd, scaled (sx, sy, sz), its xz centre (or origin) at story
        `at`, base at y. A turn, never a mirror (the story to world flip is undone here). Returns its story xz bounds."""
        v, t = self.kit.piece(name, target, ymin, max(scale), dev, sheet)
        ox = oz = 0.0
        if centre:
            ox = 0.5 * (min(a[0] for a in v) + max(a[0] for a in v))
            oz = 0.5 * (min(a[2] for a in v) + max(a[2] for a in v))
        fw = bt.unit((fwd[0], 0.0, -fwd[1]))
        g = bt.cross(fw, UP)
        W = R(at[0], y, at[1])
        w = [add(W, add(mul(fw, (lx - ox) * scale[0]), add((0.0, ly * scale[1], 0.0), mul(g, (lz - oz) * scale[2])))) for lx, ly, lz in v]
        for a, b, c, col in t:
            self.scr.tri(w[a], w[b], w[c], self.kit_col(col), item)
        xs, zs = [p[0] for p in w], [-p[2] for p in w]
        return (min(xs), min(zs), max(xs), max(zs))

    # ---- a wall: its inner facade on a story line, cells of 1.5 m, merlons on top --------------------------------
    def wall(self, name, a, b, facing, h=WH, gaps=(), outer=False, merlons=True, broken=None, map_merlons=False):
        """from story point a to b (one axis), its facade facing `facing` (into the square). gaps: (s0, s1, top) spans
        along it left open below `top` (a door or a gate). broken: (s0, s1, h1) where the wall stands lower."""
        ax, az = a
        bx, bz = b
        L = math.hypot(bx - ax, bz - az)
        ux, uz = (bx - ax) / L, (bz - az) / L
        f = bt.flat(facing)                               # world normal of the facade
        fs = (f[0], -f[2])                                # the same in story (x, z north)
        n = int(round(L / U))

        def P(s, y, r=0.0):                               # along s, up y, r metres back into the wall
            return R(ax + ux * s - fs[0] * r, y, az + uz * s - fs[1] * r)

        def top_at(s):
            if broken and broken[0] <= s < broken[1]:
                return broken[2]
            return h

        rows = [0.0, 1.5, 3.0, 4.5, 6.0]
        for i in range(n):
            s0, s1 = i * U, (i + 1) * U
            ht = top_at(s0 + 0.01)
            ys = [y for y in rows if y < ht] + [ht]
            for j in range(len(ys) - 1):
                y0, y1 = ys[j], ys[j + 1]
                if any(g[0] <= s0 + 0.01 and s1 - 0.01 <= g[1] and y1 <= g[2] + 0.01 for g in gaps):
                    continue
                col = STONE[int(h2(zlib.crc32(name.encode()) & 0xffff, i * 8 + j) * 2.999)]
                self.scr.quad(P(s0, y0), P(s1, y0), P(s1, y1), P(s0, y1), col, name, f)
                if outer:
                    self.scr.quad(P(s1, y0, WT), P(s0, y0, WT), P(s0, y1, WT), P(s1, y1, WT), col, name, mul(f, -1))
            self.scr.quad(P(s0, ht), P(s1, ht), P(s1, ht, WT), P(s0, ht, WT), STONE_TOP, name, UP)   # the walk on top
            if merlons and ht >= h - 0.01:                    # one merlon a cell, on the inner half of the top
                m0, m1 = s0 + 0.375, s0 + 0.375 + MER_W
                q = [P(m0, ht), P(m1, ht), P(m1, ht + MER_H), P(m0, ht + MER_H)]
                bq = [P(m0, ht, 0.5), P(m1, ht, 0.5), P(m1, ht + MER_H, 0.5), P(m0, ht + MER_H, 0.5)]
                self.scr.quad(q[0], q[1], q[2], q[3], STONE[1], name, f)
                self.scr.quad(q[3], q[2], bq[2], bq[3], STONE_TOP, name, UP)
                self.scr.quad(q[0], q[3], bq[3], bq[0], STONE[1], name)
                self.scr.quad(q[1], bq[1], bq[2], q[2], STONE[1], name)
        # the gaps' reveals (the wall's thickness) and their lintels
        for g in gaps:
            for s in (g[0], g[1]):
                self.scr.quad(P(s, 0.0), P(s, 0.0, WT), P(s, min(g[2], top_at(s - 0.01)), WT), P(s, min(g[2], top_at(s - 0.01))), STONE[1], name)
            if g[2] < h - 0.01:
                self.scr.quad(P(g[0], g[2]), P(g[1], g[2]), P(g[1], g[2], WT), P(g[0], g[2], WT), STONE_DK, name, mul(UP, -1))
        # the map: the wall as boxes between the gaps, a lintel over a gap, the merlons as one low box a cell
        self.cst.begin(name, "walls")
        cuts = sorted([0.0, L] + [x for g in gaps for x in (g[0], g[1])])
        for k in range(0, len(cuts) - 1, 2):
            s0, s1 = cuts[k], cuts[k + 1]
            if s1 - s0 < 0.05:
                continue
            c = P(0.5 * (s0 + s1), 0.0, 0.5 * WT)
            bt.cbox(self.cst, c, (s1 - s0, top_at(0.5 * (s0 + s1)), WT), ax=(bt.flat((ux, -uz)), mul(f, -1)))
            x0, z0, x1, z1 = ax + ux * s0, az + uz * s0, ax + ux * s1, az + uz * s1
            self.foot(x0 - fs[0] * WT, z0 - fs[1] * WT, x1, z1)
        for g in gaps:
            if g[2] < h - 0.01:
                c = P(0.5 * (g[0] + g[1]), g[2], 0.5 * WT)
                bt.cbox(self.cst, c, (g[1] - g[0], h - g[2], WT), ax=(bt.flat((ux, -uz)), mul(f, -1)))
        if merlons and map_merlons:                       # in the map only where the sun puts them on the square
            for i in range(n):
                if top_at(i * U + 0.01) >= h - 0.01:
                    c = P(i * U + 0.75, h, 0.25)
                    bt.cbox(self.cst, c, (MER_W, MER_H, 0.5), ax=(bt.flat((ux, -uz)), mul(f, -1)), top=False)
        return P

    def tower(self, name, cx, cz, side, h, cells, item="towers", map_merlons=False):
        """a square stone tower at story (cx, cz), merlons two a side"""
        c = R(cx, 0.0, cz)
        hs = side / 2
        for kf in range(4):
            a = kf * math.pi / 2
            nrm = (math.cos(a), 0.0, math.sin(a))
            tan = (-math.sin(a), 0.0, math.cos(a))
            o = add(add(c, mul(nrm, hs)), mul(tan, -hs))
            us = [side * q / cells[0] for q in range(cells[0] + 1)]
            vs = [h * q / cells[1] for q in range(cells[1] + 1)]
            bt.grid(self.scr, o, tan, UP, us, vs, nrm, STONE[kf % 3] if item == "towers" else STONE_DK, item)
            for q in (0.25, 0.75):                         # two merlons a side
                mc = add(add(c, mul(nrm, hs - 0.3)), mul(tan, side * (q - 0.5)))
                bt.box(self.scr, (mc[0], h, mc[2]), (0.8, 0.9, 0.6), STONE[1], item, ax=(tan, nrm))
        self.scr.quad(add(c, (-hs, h, -hs)), add(c, (hs, h, -hs)), add(c, (hs, h, hs)), add(c, (-hs, h, hs)), STONE_TOP, item, UP)
        self.cst.begin(name, item)
        self.named[name] = len(self.cst.objs) - 1
        bt.cbox(self.cst, c, (side, h, side))
        for kf in range(4 if map_merlons else 0):
            a = kf * math.pi / 2
            nrm = (math.cos(a), 0.0, math.sin(a))
            tan = (-math.sin(a), 0.0, math.cos(a))
            for q in (0.25, 0.75):
                mc = add(add(c, mul(nrm, hs - 0.3)), mul(tan, side * (q - 0.5)))
                bt.cbox(self.cst, (mc[0], h, mc[2]), (0.8, 0.9, 0.6), ax=(tan, nrm))
        self.foot(cx - hs, cz - hs, cx + hs, cz + hs)

    def door(self, name, P, s0, s1, top, cast=False):
        """a door in a wall's gap: planks set back in the reveal, an iron band. cast: in the map too (a gate that closes
        a gap the sun would shine through)"""
        self.scr.quad(P(s0, 0.0, 0.6), P(s1, 0.0, 0.6), P(s1, top, 0.6), P(s0, top, 0.6), WOOD_DK, "doors")
        self.scr.quad(P(s0, 1.1, 0.58), P(s1, 1.1, 0.58), P(s1, 1.25, 0.58), P(s0, 1.25, 0.58), IRON, "doors")
        if cast:
            along, back = bt.unit(bt.sub(P(1.0, 0.0), P(0.0, 0.0))), bt.unit(bt.sub(P(0.0, 0.0, 1.0), P(0.0, 0.0)))
            self.cst.begin(name, "doors")
            bt.cbox(self.cst, P(0.5 * (s0 + s1), 0.0, 0.6), (s1 - s0, top, 0.1), ax=(along, back))
            a, b = P(s0, 0.0, 0.0), P(s1, 0.0, WT)
            self.foot(a[0], -a[2], b[0], -b[2])

    def build(self):
        S = self
        # ---- the enclosure -------------------------------------------------------------------------------------
        Pn = S.wall("north wall", (XW, ZN), (XE, ZN), "S", gaps=((GATE_N[0] - XW, GATE_N[1] - XW, 4.5), (9.75 - XW, 11.25 - XW, 2.4)))
        S.door("north door", Pn, 9.75 - XW, 11.25 - XW, 2.4)
        if CLOSED:                                        # the north gate shut: the porter comes in through it
            S.door("north gate", Pn, GATE_N[0] - XW, GATE_N[1] - XW, 4.5, cast=True)
        Pw = S.wall("west wall", (XW, ZS), (XW, ZN), "E", map_merlons=True, gaps=((-9.75 - ZS, -8.25 - ZS, 2.4), (2.25 - ZS, 3.75 - ZS, 2.4)))
        S.door("west door s", Pw, -9.75 - ZS, -8.25 - ZS, 2.4)
        S.door("west door n", Pw, 2.25 - ZS, 3.75 - ZS, 2.4)
        S.wall("east wall", (XE, ZN), (XE, ZS), "W")
        S.wall("south wall w", (XW, ZS), (GATE_S[0], ZS), "N", outer=not CLOSED, broken=(0.0, 6.0, 4.5), map_merlons=True)
        S.wall("south wall e", (GATE_S[1], ZS), (XE, ZS), "N", outer=not CLOSED, map_merlons=True)
        if CLOSED:                                        # the street's gap: a wall with a gate under a lintel
            L = GATE_S[1] - GATE_S[0]
            Ps = S.wall("south gate wall", (GATE_S[0], ZS), (GATE_S[1], ZS), "N", gaps=((0.0, L, 4.5),), map_merlons=True)
            S.door("south gate", Ps, 0.0, L, 4.5, cast=True)
        # the north gate: two gate towers either side, the passage's dark far end
        for gx in (GATE_N[0] - 1.0, GATE_N[1] + 1.0):
            c = R(gx, 0.0, ZN - 0.5)
            bt.box(S.scr, c, (2.0, 7.6, 1.0), STONE[2], "gate")
            for q in (-0.5, 0.5):
                bt.box(S.scr, (c[0] + q * 1.0, 7.6, c[2]), (0.6, 0.8, 1.0), STONE[1], "gate")
            S.cst.begin("gate tower %.0f" % gx, "gate")
            bt.cbox(S.cst, c, (2.0, 7.6, 1.0))
            S.foot(gx - 1.0, ZN - 1.0, gx + 1.0, ZN + WT)
        # corner towers, outward of the corners
        for cx, cz in ((XW - 0.75, ZN + 0.75), (XE + 0.75, ZN + 0.75), (XW - 0.75, ZS - 0.75), (XE + 0.75, ZS - 0.75)):
            S.tower("corner %.0f %.0f" % (cx, cz), cx, cz, 3.5, 9.0, (2, 3))
        # the keep, where the church tower stood (the 15:30 preset's long shadow)
        S.tower("keep", -27.0, -31.0, 6.0, 27.1, (2, 9), item="keep", map_merlons=True)   # 28 m to its merlons' tops, as the church tower
        S.named["tower"] = S.named["keep"]
        # buttresses on the long facades
        for x in (-7.5, -3.75, 4.5, 7.5):
            bt.box(S.scr, R(x, 0.0, ZN - 0.25), (0.9, 5.2, 0.5), STONE_DK, "buttresses")
            S.cst.begin("buttress n %.1f" % x, "buttresses")
            bt.cbox(S.cst, R(x, 0.0, ZN - 0.25), (0.9, 5.2, 0.5))
            S.foot(x - 0.45, ZN - 0.5, x + 0.45, ZN)
        for z in (-12.0, -4.5, 7.5):
            bt.box(S.scr, R(XE - 0.25, 0.0, z), (0.5, 5.2, 0.9), STONE_DK, "buttresses")
            S.cst.begin("buttress e %.1f" % z, "buttresses")
            bt.cbox(S.cst, R(XE - 0.25, 0.0, z), (0.5, 5.2, 0.9))
            S.foot(XE - 0.5, z - 0.45, XE, z + 0.45)

        # ---- the colonnade along the north wall, west of the gate ----------------------------------------------
        S.cst.begin("colonnade", "colonnade")
        for x in (-9.3, -7.4, -5.5, -3.6):
            c = R(x, 0.0, 9.5)
            S.kp("pillar-stone", 90, (x, 9.5), (1.0, 0.0), (4.4, 4.2, 4.4), "colonnade", dev=(0.15, 0.1, 0.06))   # 0.7 m across, base to cap
            bt.prism(None, (c[0], 0.3, c[2]), 0.28, 3.6, 8, None, None, cst=S.cst)
            S.foot(x - 0.4, 9.1, x + 0.4, 9.9)
        slab = R(-6.45, 4.2, 10.6)
        bt.box(S.scr, slab, (7.0, 0.35, 2.9), STONE[1], "colonnade", bottom=True)
        bt.cbox(S.cst, slab, (7.0, 0.35, 2.9))

        # ---- the open square: the town's fixed points kept --------------------------------------------------
        # the fountain where the well stood (-2, -7): the kit's round basin, its water a disc
        wc = R(-2.0, 0.0, -7.0)
        S.kp("fountain-round", 220, (-2.0, -7.0), (1.0, 0.0), (0.9, 2.0, 0.9), "well", dev=(0.25, 0.2, 0.16))
        ring = [(wc[0] + 0.75 * math.cos(2 * math.pi * i / 10), wc[2] + 0.75 * math.sin(2 * math.pi * i / 10)) for i in range(10)]
        for i in range(10):
            a, b = ring[i], ring[(i + 1) % 10]
            S.scr.tri((wc[0], 0.42, wc[2]), (b[0], 0.42, b[1]), (a[0], 0.42, a[1]), WATER, "well", UP)
        S.cst.begin("well", "well")
        bt.prism(None, wc, 0.9, 0.56, 10, None, None, cst=S.cst)
        S.foot(-2.85, -7.85, -1.15, -6.15)
        # the obelisk where the cross stood (4, 0): two steps, a tapered shaft, a point
        c = R(4.0, 0.0, 0.0)
        bt.box(S.scr, c, (2.0, 0.35, 2.0), STONE[2], "obelisk")
        bt.box(S.scr, (c[0], 0.35, c[2]), (1.3, 0.3, 1.3), STONE[1], "obelisk")
        b0, b1, ht = 0.30, 0.20, 4.6
        lo = [(c[0] - b0, 0.65, c[2] - b0), (c[0] + b0, 0.65, c[2] - b0), (c[0] + b0, 0.65, c[2] + b0), (c[0] - b0, 0.65, c[2] + b0)]
        hi = [(c[0] - b1, 0.65 + ht, c[2] - b1), (c[0] + b1, 0.65 + ht, c[2] - b1), (c[0] + b1, 0.65 + ht, c[2] + b1), (c[0] - b1, 0.65 + ht, c[2] + b1)]
        apex = (c[0], 0.65 + ht + 0.45, c[2])
        for k in range(4):
            S.scr.quad(lo[k], lo[(k + 1) % 4], hi[(k + 1) % 4], hi[k], STONE_DK, "obelisk")
            S.scr.tri(hi[k], hi[(k + 1) % 4], apex, STONE_TOP, "obelisk")
        S.cst.begin("obelisk", "obelisk")
        bt.cbox(S.cst, c, (2.0, 0.65, 2.0))
        for k in range(4):
            S.cst.quad(lo[k], lo[(k + 1) % 4], hi[(k + 1) % 4], hi[k])
            S.cst.tri(hi[k], hi[(k + 1) % 4], apex)
        S.foot(3.0, -1.0, 5.0, 1.0)
        # the four stall frames on the east side, opening west (the cast's stops are under them)
        for si, zc in enumerate((6.0, 2.0, -2.0, -6.0)):
            S.cst.begin("stall %d" % (si + 1), "stalls")
            for px in (9.15, 11.85):
                for pz in (zc - 1.4, zc + 1.4):
                    bt.box(S.scr, R(px, 0, pz), (0.14, 2.1, 0.14), WOOD_DK, "stalls", top=False)
                    bt.cbox(S.cst, R(px, 0, pz), (0.14, 2.1, 0.14), top=False)
            # the kit's awning alone (its own counter would stand where the cast works), ridge along z, 2.0 .. 3.2 m
            S.kp("stall-red" if si % 2 == 0 else "stall-green", 130, (10.5, zc), (1.0, 0.0), (3.0, 2.6, 3.1), "stalls", ymin=0.7)
            for xa, xb in ((9.0, 10.5), (10.5, 12.0)):
                cloth = [R(xa, 2.0 if xa < 10 else 3.2, zc - 1.55), R(xb, 3.2 if xa < 10 else 2.0, zc - 1.55),
                         R(xb, 3.2 if xa < 10 else 2.0, zc + 1.55), R(xa, 2.0 if xa < 10 else 3.2, zc + 1.55)]
                S.cst.quad(*cloth)
            bt.box(S.scr, R(9.85, 0, zc), (0.8, 0.95, 2.6), WOOD, "counters")   # a box: the kit's table is 80 triangles
            bt.cbox(S.cst, R(9.85, 0, zc), (0.8, 0.95, 2.6))
        S.cst.begin("hung cloth", "hung cloth")
        for q, dz in enumerate((-0.9, 0.0, 0.9)):
            qd = (R(9.2, 2.1, 6.0 + dz - 0.3), R(9.2, 2.1, 6.0 + dz + 0.3), R(9.2, 1.1, 6.0 + dz + 0.3), R(9.2, 1.1, 6.0 + dz - 0.3))
            S.scr.quad(*qd, CLOTH[(q + 1) % 4], "hung cloth", (-1.0, 0.0, 0.0))
            S.cst.quad(*qd)
        S.cst.begin("crates", "crates")
        for dz in (-0.9, 0.0, 0.9):
            bt.box(S.scr, R(9.0, 0, -2.0 + dz), (0.55, 0.5, 0.55), WOOD, "crates")
            bt.cbox(S.cst, R(9.0, 0, -2.0 + dz), (0.55, 0.5, 0.55))
        S.cst.begin("sacks", "sacks")
        for sx, sz, sy in ((11.0, -3.3, 0.0), (11.5, -3.3, 0.0), (11.25, -3.3, 0.38)):
            bt.box(S.scr, R(sx, sy, sz), (0.45, 0.38, 0.6), (0.62, 0.55, 0.42), "sacks")
            bt.cbox(S.cst, R(sx, sy, sz), (0.45, 0.38, 0.6))
        S.cst.begin("barrels", "barrels")
        for bx, bz in ((8.6, -5.0), (8.6, -5.8), (8.6, -6.6), (9.3, -7.2), (10.6, -7.1), (11.3, -7.1), (10.6, -4.9), (11.3, -4.9)):
            bt.prism(S.scr, R(bx, 0, bz), 0.33, 0.87, 6, WOOD, "barrels", cst=S.cst)
        # the hen pen, (7.75 .. 9.25) x (-1 .. 2): four posts, rails
        S.cst.begin("hen pen", "hen pen")
        for px, pz in ((7.75, -1.0), (9.25, -1.0), (7.75, 2.0), (9.25, 2.0)):
            bt.cbox(S.cst, R(px, 0, pz), (0.16, 0.6, 0.16), top=False)
        # the kit's fence, a 1.5 m length a side cell (its local z along the side), 0.6 m high
        for (mx, mz), (dx, dz), ln in (((7.75, 0.5), (1.0, 0.0), 3.0), ((9.25, 0.5), (1.0, 0.0), 3.0),
                                       ((8.5, -1.0), (0.0, 1.0), 1.5), ((8.5, 2.0), (0.0, 1.0), 1.5)):
            S.kp("fence", 60, (mx, mz), (dx, dz), (1.5, 1.6, ln), "hen pen")
        for (ax_, az), (bx_, bz) in (((7.75, -1.0), (7.75, 2.0)), ((9.25, -1.0), (9.25, 2.0)), ((7.75, -1.0), (9.25, -1.0)), ((7.75, 2.0), (9.25, 2.0))):
            mx, mz, ln = (ax_ + bx_) / 2, (az + bz) / 2, math.hypot(bx_ - ax_, bz - az)
            size = (0.06, 0.08, ln) if ax_ == bx_ else (ln, 0.08, 0.06)
            bt.cbox(S.cst, R(mx, 0.45, mz), size)
        # the ladder on the north wall (route B's self-shadow test): rails and rungs, foot at story z 10.45
        def LP(t, x):
            return R(x, 5.85 * t, 10.45 + 1.52 * t)
        S.cst.begin("ladder", "ladder")
        for xr in (7.55, 8.45):
            for a, b in (((xr - 0.03, 0), (xr + 0.03, 0)),):
                q = (LP(0, a[0]), LP(0, b[0]), LP(1, b[0]), LP(1, a[0]))
                S.scr.quad(*q, WOOD, "ladder")
                S.cst.quad(*q)
        for k in range(12):
            t = (0.3 + 0.45 * k) / 5.85
            q = (LP(t - 0.004, 7.55), LP(t - 0.004, 8.45), LP(t + 0.004, 8.45), LP(t + 0.004, 7.55))
            S.scr.quad(*q, WOOD, "ladder")
            S.cst.quad(*q)

        # ---- the dais in the south-west corner: a platform, three steps, a brazier ------------------------------
        S.cst.begin("dais", "dais")
        bt.box(S.scr, R(-7.75, 0.0, -12.75), (4.5, 0.9, 3.5), STONE[0], "dais")
        bt.cbox(S.cst, R(-7.75, 0.0, -12.75), (4.5, 0.9, 3.5))
        S.kp("stairs-wide-stone", 70, (-7.75, -10.35), (0.0, -1.0), (1.25, 0.9, 2.4), "dais", dev=(0.12, 0.08))   # climbs south, onto the dais
        for k in range(3):
            bt.cbox(S.cst, R(-7.75, 0.0, -10.75 + 0.4 * (2 - k)), (2.4, 0.3 * (k + 1), 0.4))
        bt.prism(S.scr, R(-7.75, 0.9, -13.4), 0.35, 0.9, 6, IRON, "dais", cst=S.cst)
        S.scr.tri(R(-8.0, 1.8, -13.4), R(-7.5, 1.8, -13.4), R(-7.75, 2.4, -13.4), FLAME, "dais", (0.0, 0.0, 1.0))
        S.foot(-10.0, -14.5, -5.5, -10.0)
        # rubble under the broken stretch
        S.cst.begin("rubble", "rubble")
        for k, (rx, rz, sz) in enumerate(((-11.0, -15.6, 0.7), (-10.1, -15.9, 0.5), (-9.2, -15.5, 0.6), (-8.1, -15.8, 0.45))):
            bt.box(S.scr, R(rx, 0.0, rz), (sz, sz * 0.6, sz), STONE[k % 3], "rubble", yaw=0.4 * k)   # the kit's rock: 148 a stone
            bt.cbox(S.cst, R(rx, 0.0, rz), (sz, sz * 0.6, sz), yaw=0.4 * k)
            S.foot(rx - sz / 2, rz - sz / 2, rx + sz / 2, rz + sz / 2)

        # ---- dressing: banners and torches ----------------------------------------------------------------------
        S.cst.begin("banners", "banners")
        # the kit's banners, 1.3 x 2.2 m, hung flat on the wall from 5.2 m, facing into the square
        for k, x in enumerate((GATE_N[0] - 1.0, GATE_N[1] + 1.0)):   # on the gate towers' faces, either side of the gate
            S.kp(("banner-red", "banner-green")[k % 2], 70, (x, ZN - 1.08), (0.0, -1.0), (2.6, 2.6, 2.6), "banners", y=4.4, sheet=True)
            q = (R(x - 0.5, 7.0, ZN - 1.05), R(x + 0.5, 7.0, ZN - 1.05), R(x + 0.5, 4.8, ZN - 1.05), R(x - 0.5, 4.8, ZN - 1.05))
            S.cst.quad(*q)
        for k, z in enumerate((-1.0, 9.6)):
            S.kp(("banner-red", "banner-green")[(k + 1) % 2], 70, (XW + 0.08, z), (1.0, 0.0), (2.6, 2.6, 2.6), "banners", y=2.6, sheet=True)
            q = (R(XW + 0.05, 5.2, z - 0.5), R(XW + 0.05, 5.2, z + 0.5), R(XW + 0.05, 3.0, z + 0.5), R(XW + 0.05, 3.0, z - 0.5))
            S.cst.quad(*q)
        S.cst.begin("torches", "torches")
        torches = [((x, ZN - 0.15), (0.0, -1.0)) for x in (-10.0, -2.4, 2.4, 8.6)] + [((XW + 0.15, z), (1.0, 0.0)) for z in (-12.6, -5.0, 6.6)]
        for (tx, tz), (dx, dz) in torches:
            c = R(tx + 0.1 * dx, 2.4, tz + 0.1 * dz)
            bt.box(S.scr, c, (0.16, 0.45, 0.16), IRON, "torches")
            bt.cbox(S.cst, c, (0.16, 0.45, 0.16))
            f0 = (c[0], 2.85, c[2])
            S.scr.quad(add(f0, (-0.12, 0, 0)), add(f0, (0.12, 0, 0)), add(f0, (0.0, 0.35, 0)), add(f0, (-0.0, 0.35, 0)), FLAME, "torches", (0.0, 0.0, 1.0))
            S.scr.quad(add(f0, (0, 0, -0.12)), add(f0, (0, 0, 0.12)), add(f0, (0, 0.35, 0.0)), add(f0, (0, 0.35, -0.0)), FLAME, "torches", (1.0, 0.0, 0.0))

        # ---- the kit's free pieces: lanterns on posts, two trees (a cart was 290 triangles: left out) --------------------------
        S.cst.begin("lanterns", "lanterns")
        for lx, lz in ((-11.2, -5.5), (12.8, -9.6), (12.8, 9.8), (-4.0, 1.5)):
            S.kp("lantern", 70, (lx, lz), (1.0, 0.0), (1.6, 1.6, 1.6), "lanterns")
            bt.cbox(S.cst, R(lx, 0.0, lz), (0.2, 2.5, 0.2))
            S.foot(lx - 0.18, lz - 0.18, lx + 0.18, lz + 0.18)
        S.cst.begin("trees", "trees")
        for k, (tx, tz) in enumerate(((-10.3, -3.2), (12.0, -13.0))):
            S.kp("tree-high-round", 130, (tx, tz), (math.cos(1.1 * k), math.sin(1.1 * k)), (1.7, 1.6, 1.7), "trees", dev=(0.2, 0.15, 0.1))
            bt.cbox(S.cst, R(tx, 0.0, tz), (0.3, 1.6, 0.3), top=False)
            bt.cbox(S.cst, R(tx, 1.6, tz), (1.5, 2.8, 1.5))
            S.foot(tx - 0.2, tz - 0.2, tx + 0.2, tz + 0.2)


# ---- --closed: the faces no eye in the walk can see, left out of the screen mesh ---------------------------------------
def walk_eyes(step=1.0):
    """eye points over the closed walk, story (x, z), a grid `step` apart with the edges on it"""
    x0, z0, x1, z1 = WALK_CLOSED
    nx, nz = int(math.ceil((x1 - x0) / step)), int(math.ceil((z1 - z0) / step))
    return [(x0 + (x1 - x0) * i / nx, z0 + (z1 - z0) * j / nz) for i in range(nx + 1) for j in range(nz + 1)]


def over_walls(e, p):
    """1 when the sight line from the eye (story x, z at EYE_H) to the world point p clears the walls: p outside the inner
    faces, and the line's height where it leaves the courtyard over WALL_LOW (the lowest top; a higher wall only hides more)"""
    px, py, pz = p[0], p[1], -p[2]
    if XW <= px <= XE and ZS <= pz <= ZN:
        return True                                       # inside: not this test's
    if py <= WALL_LOW:
        return False
    dx, dz, t = px - e[0], pz - e[1], 1.0
    if dx > 1e-9: t = min(t, (XE - e[0]) / dx)
    if dx < -1e-9: t = min(t, (XW - e[0]) / dx)
    if dz > 1e-9: t = min(t, (ZN - e[1]) / dz)
    if dz < -1e-9: t = min(t, (ZS - e[1]) / dz)
    return EYE_H + (py - EYE_H) * t > WALL_LOW - 0.25      # 0.25 m spare for the eye grid's spacing


def cull_closed(scr):
    """the screen triangles kept, and how many each rule left out: tops (a solid's face up, wholly above the eye: its own
    body hides it from below), away (a solid's face that turns from every eye: its far side; the walk is convex, so its four
    corners decide), hidden (outside the walls and under every sight line over them, sampled at its corners, edge middles
    and middle from eyes a metre apart). The first two only for a face whose outward normal was given: the PVR's culling
    is off, so a face's winding says nothing about which side is seen."""
    x0, z0, x1, z1 = WALK_CLOSED
    corners = [R(x, EYE_H, z) for x in (x0, x1) for z in (z0, z1)]
    eyes = walk_eyes()
    kept, out = [], {"tops": 0, "away": 0, "hidden": 0}
    for t, given in zip(scr.t, scr.given):
        p = [scr.p[i] for i in t[:3]]
        n = scr.n[t[0]]
        solid = given and any(k in t[3] for k in SOLID)
        if solid and n[1] > 0.9 and min(q[1] for q in p) >= EYE_H + 0.1:
            out["tops"] += 1
            continue
        if solid and all(bt.dot(n, bt.sub(e, q)) < -1e-4 for e in corners for q in p):
            out["away"] += 1
            continue
        inside = [XW <= q[0] <= XE and ZS <= -q[2] <= ZN for q in p]
        if not any(inside):
            mids = [bt.mul(bt.add(p[a], p[b]), 0.5) for a, b in ((0, 1), (1, 2), (2, 0))]
            cen = bt.mul(bt.add(bt.add(p[0], p[1]), p[2]), 1.0 / 3.0)
            if not any(over_walls(e, q) for q in p + mids + [cen] for e in eyes):
                out["hidden"] += 1
                continue
        kept.append(t)
    return kept, out


def compact(scr, tris):
    """the screen mesh with only the vertices `tris` use, in their first use order"""
    remap, p, n, c, t2 = {}, [], [], [], []
    for t in tris:
        for i in t[:3]:
            if i not in remap:
                remap[i] = len(p)
                p.append(scr.p[i]); n.append(scr.n[i]); c.append(scr.c[i])
        t2.append((remap[t[0]], remap[t[1]], remap[t[2]], t[3]))
    scr.p, scr.n, scr.c, scr.t = p, n, c, t2


# ---- the cast's routes, read from psm_town.h (not copied), checked against the solid footprints -----------------------
def routes():
    src = open(os.path.join(HERE, "..", "psm_town.h"), encoding="utf-8").read()
    src = src[src.index("PSM_TOWN_CLOSED_ROUTES" if CLOSED else "PSM_TOWN_OPEN_ROUTES"):]   # the closed square's routes, or the open one's
    wp_txt = src[src.index("psm_cast_wp[] = {"):src.index("};", src.index("psm_cast_wp[] = {"))]
    wp = [(float(a), float(b)) for a, b, _ in re.findall(r"\{\s*(-?[\d.]+)f,\s*(-?[\d.]+)f,\s*(-?[\d.]+)f\s*\}", wp_txt)]
    rt_txt = src[src.index("psm_cast_route[PSM_CAST][3] = {"):src.index("};", src.index("psm_cast_route[PSM_CAST][3] = {"))]
    rts = [tuple(int(x) for x in m) for m in re.findall(r"\{\s*(\d+),\s*(\d+),\s*(\d+)\s*\}", rt_txt)]
    legs = []
    for f, n, open_ in rts:
        for k in range(n):
            if open_ and k + 1 == n:
                continue
            legs.append((wp[f + k], wp[f + (k + 1) % n]))
    return wp, legs


def seg_box_dist(a, b, bx, steps=40):
    x0, z0, x1, z1 = bx
    best = 1e9
    for i in range(steps + 1):
        t = i / steps
        px, pz = a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t
        dx = max(x0 - px, 0.0, px - x1)
        dz = max(z0 - pz, 0.0, pz - z1)
        best = min(best, math.hypot(dx, dz))
    return best


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    global CLOSED
    rep_path = argv[argv.index("--json") + 1] if "--json" in argv else None
    CLOSED = "--closed" in argv
    kit_zip = argv[1] if argv[1] != "-" and os.path.exists(argv[1]) else None
    kit = FTKit(kit_zip)
    sq = Square(kit)
    sq.build()
    culled = None
    if CLOSED:
        n_t, n_v = len(sq.scr.t), len(sq.scr.p)
        kept, culled = cull_closed(sq.scr)
        compact(sq.scr, kept)
        culled.update({"triangles_before": n_t, "vertices_before": n_v, "triangles": len(sq.scr.t), "vertices": len(sq.scr.p)})
    mv = bt.movers(sq.pal)
    acc = [sq.pal(c) for c in bt.PEOPLE_ACC]
    # the routes keep off every solid footprint (a walker's half width 0.25 m)
    bad = []
    _, legs = routes()
    for a, b in legs:
        for bx in sq.solid:
            d = seg_box_dist(a, b, bx)
            if d < 0.25:
                bad.append((a, b, bx, round(d, 2)))
    if bad:
        for a, b, bx, d in bad[:12]:
            print("bake_square: a route %s -> %s passes %.2f m from a solid %s" % (a, b, d, bx))
        return 1
    if len(sq.pal.cols) > 64 or len(sq.scr.p) > 65535:
        print("bake_square: too many colours or vertices")
        return 1
    strips = bt.stripify(sq.scr.t)
    items = {}
    for t in sq.scr.t:
        items.setdefault(t[3], [0, 0])[0] += 1
    for o in sq.cst.objs:
        items.setdefault(o["item"], [0, 0])[1] += len(o["t"])
    cv, ct, co = [], [], []
    for o in sq.cst.objs:
        base, t0 = len(cv), len(ct)
        cv += o["p"]
        ct += [(a + base, b + base, c + base) for a, b, c in o["t"]]
        co.append((base, len(o["p"]), t0, len(o["t"])))
    sv_n = sum(len(s) for s in strips)
    rep = {
        "kit": 1 if kit_zip else 0,
        "pieces": kit.report,
        "screen": {"vertices": len(sq.scr.p), "triangles": len(sq.scr.t), "strips": len(strips), "strip_vertices": sv_n},
        "casters": {"vertices": len(cv), "triangles": len(ct), "objects": len(co)},
        "items": {k: {"screen": v[0], "map": v[1]} for k, v in sorted(items.items())},
        "movers": {"person": {"vertices": len(mv[0][0].p), "screen": len(mv[0][0].t), "map": len(mv[0][1]["t"])},
                   "hen": {"vertices": len(mv[1][0].p), "screen": len(mv[1][0].t), "map": len(mv[1][1]["t"])}},
        "palette": len(sq.pal.cols),
        "route_legs_checked": len(legs),
        "closed": culled,
    }
    with open(argv[2], "w", encoding="utf-8", newline="\n") as f:
        f.write("/* Generated by demo/psm/tools/bake_square.py. Not in the repository: the build folder.\n")
        f.write("   The walled square. Pieces: Fantasy Town Kit (2.0) by Kenney (www.kenney.nl), CC0 1.0, %s.\n"
                % ("cut by qem_decimate.py (dc-forerunner)" if kit_zip else "NOT USED: stand-in boxes"))
        for k, v in sorted(kit.report.items()):
            f.write("   kit %-20s %s\n" % (k, json.dumps(v)))
        for k, v in sorted(items.items()):
            f.write("   %-22s screen %5d  map %4d\n" % (k, v[0], v[1]))
        if culled:
            f.write("   closed: screen %(triangles_before)d -> %(triangles)d triangles, %(vertices_before)d -> %(vertices)d vertices"
                    " (left out: tops %(tops)d, away %(away)d, hidden %(hidden)d)\n" % culled)
        f.write("*/\n#define PSM_TOWN_KIT %d\n#define PSM_TOWN_NAME \"walled square\"\n" % (1 if kit_zip else 0))
        if CLOSED:                                        # psm_town.h's closed routes and walk; the ground that can be seen
            f.write("#define PSM_TOWN_CLOSED 1\n#define PSM_TOWN_BOUND_X0 (%.2ff)\n#define PSM_TOWN_BOUND_X1 %.2ff\n"
                    "#define PSM_TOWN_BOUND_ZS0 (%.2ff)\n#define PSM_TOWN_BOUND_ZS1 %.2ff\n" % (XW - WT, XE + WT, ZS - WT, ZN + WT))
        f.write("#define PSM_TOWN_PAL %du\nstatic const float psm_town_pal[PSM_TOWN_PAL][3] = {\n%s\n};\n" % (len(sq.pal.cols), bt.c_floats(sq.pal.cols)))
        f.write("static const unsigned char psm_town_acc[8] = { %s };   /* the cast's accents, psm_town.h's order */\n" % ", ".join(map(str, acc)))
        bt.write_screen(f, "psm_town", sq.scr, strips)
        f.write("#define PSM_TOWN_CV %du\n#define PSM_TOWN_CT %du\n#define PSM_TOWN_CO %du\n#define PSM_TOWN_CO_TOWER %du\n"
                % (len(cv), len(ct), len(co), sq.named["tower"]))
        f.write("static const float psm_town_cv[PSM_TOWN_CV][3] = {\n%s\n};\n" % bt.c_floats(cv))
        f.write("static const unsigned short psm_town_ct[PSM_TOWN_CT][3] = {\n%s\n};\n" % bt.c_floats(ct, "%d"))
        f.write("static const unsigned short psm_town_co[PSM_TOWN_CO][4] = {   /* first vertex, vertices, first triangle, triangles */\n%s\n};\n"
                % bt.c_floats(co, "%d"))
        for name, (s, c) in zip(("psm_town_person", "psm_town_hen"), mv):
            bt.write_screen(f, name, s, bt.stripify(s.t))
            f.write("#define %s_CV %du\n#define %s_CT %du\n" % (name.upper(), len(c["p"]), name.upper(), len(c["t"])))
            f.write("static const float %s_cv[%s_CV][3] = {\n%s\n};\n" % (name, name.upper(), bt.c_floats(c["p"])))
            f.write("static const unsigned short %s_ct[%s_CT][3] = {\n%s\n};\n" % (name, name.upper(), bt.c_floats(c["t"], "%d")))
    if rep_path:
        with open(rep_path, "w", encoding="utf-8") as f:
            json.dump(rep, f, indent=1)
    print("bake_square: kit %s; screen %d triangles, %d vertices, %d strips (%d strip vertices); map %d triangles in %d objects; "
          "%d colours; %d route legs clear" % ("YES" if kit_zip else "NO (stand-ins)", len(sq.scr.t), len(sq.scr.p), len(strips), sv_n, len(ct), len(co), len(sq.pal.cols), len(legs)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
