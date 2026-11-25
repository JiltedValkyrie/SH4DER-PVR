#!/usr/bin/env python3
"""SH4DER-PVR: a room of OBJ files (or the placeholder room) as C arrays.  Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

    python demo/psm/ao/room_import.py [--room <dir with room_manifest.json> | --placeholder] --out <dir> [--grid 0.25]

Writes <out>/room_mesh.h, the geometry that demo/psm/ao/room_data.h includes. The AO (a byte a vertex, a lightmap a
flat surface) is baked from it by demo/psm/ao/tools/room_bake.c into <out>/room_ao.h. Both are build products: they are
not kept in the repository. It fetches nothing and writes nothing outside --out.

THE ROOM'S FILES (--room). room_manifest.json is a list of objects, or {"objects": [...], "eye": [x, y, z],
"target": [x, y, z]}. An object:
    "name"     a short name
    "file"     an OBJ file beside the manifest: triangles or polygons (cut into fans), v and vn lines
    "surface"  "floor", "wall" or null. A surface is ONE flat rectangle (given as a quad or two triangles). It is cut
               here into a grid of --grid metres so a colour a vertex has something to hold, and it gets a lightmap
               (floor 128 x 128, a wall 128 along its longer side by 64). A wall also becomes a caster: a slab 0.2 m
               thick behind it, closed, so the floor at its foot can be darkened by the PVR map method.
    "open"     true: not a closed surface wound one way; the PVR map method must draw both its sides
    "caster"   false: it casts nothing (default true; a surface's own quad never casts)
    "tint"     0 .. 3, a colour for the COLOUR look
Metres, y up, the floor's top at y 0, everything within 8 m of the middle.

--placeholder builds the stand-in room in this file from boxes and blobs: a floor of 6 x 6 m, two walls 3 m high with a
skirting each, a sofa, a table, two ottomans, a lamp and two plants. It goes through the same code as a room of files.

room_mesh.h holds: the vertices on screen (place, normal, tint), the triangles, the objects (their ranges and flags),
the casters (place and triangles, by object), the surfaces (their frames, grids and lightmap sizes, and the 4 corners
of each for the few-vertex drawing with a lightmap), the occluders for the bake (the objects' triangles and each
surface as two), and the camera's start. demo/psm/ao/room_data.h documents each name.

A thing that lies flat on the floor and casts nothing (a rug, a border: no "surface", "caster" false, nowhere over
FLAT_MAX metres high) is NOT drawn: it would cover the floor's AO, the lightmap's and the PVR map's alike, with a few
vertices of its own. It is left out of the mesh and of the bake, and the floor's vertices under it take its tint
(not under one marked "optional": true, which is only left out). --room may name the folder of the manifest or the
folder above its export/ folder. The manifest's "camera" ("eye", "target") is the camera's start.

Trim on a wall (an object that casts nothing, is no surface and does not lie flat on the floor: rails, panel
mouldings, a skirting) is LIT WITH THE WALL'S NORMAL: each of its vertices within TRIM_NEAR metres in front of a wall's
rectangle takes that wall's normal. A rail's top and sides are a pixel or less on the screen from across the room;
lit by their own normals they are brighter or darker than the wall and show as dashed lines (seen on a console,
2026-11-25). With the wall's normal they have the wall's shade and the moulding shows by its baked AO alone. The
places and the triangles are not changed.

--self-test builds the placeholder into a temporary folder and checks its counts.
"""
import argparse
import json
import math
import os
import sys
import tempfile

WALL_SLAB = 0.2
FLAT_MAX = 0.05
TRIM_NEAR = 0.08


def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def mul(a, s): return (a[0] * s, a[1] * s, a[2] * s)
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def length(a): return math.sqrt(dot(a, a))


def norm(a):
    l = length(a)
    return mul(a, 1.0 / l) if l > 1e-12 else (0.0, 1.0, 0.0)


class Mesh:
    """Vertices (place, normal) and triangles of one object."""

    def __init__(self):
        self.v, self.n, self.t = [], [], []

    def vert(self, p, n):
        self.v.append(tuple(p)); self.n.append(tuple(n))
        return len(self.v) - 1

    def box(self, lo, hi):
        """Six faces wound outward, flat normals (24 vertices, 12 triangles)."""
        c = [(hi[0] if i & 1 else lo[0], hi[1] if i & 2 else lo[1], hi[2] if i & 4 else lo[2]) for i in range(8)]
        faces = [((0, 4, 6, 2), (-1, 0, 0)), ((1, 3, 7, 5), (1, 0, 0)), ((0, 1, 5, 4), (0, -1, 0)),
                 ((2, 6, 7, 3), (0, 1, 0)), ((0, 2, 3, 1), (0, 0, -1)), ((4, 5, 7, 6), (0, 0, 1))]
        for q, n in faces:
            i = [self.vert(c[k], n) for k in q]
            self.t += [(i[0], i[1], i[2]), (i[0], i[2], i[3])]

    def blob(self, c, r, seg=10, rings=8, squash=1.0):
        """A sphere (squashed in y), smooth normals, wound outward; the poles as single triangles."""
        v0 = len(self.v)
        for j in range(rings + 1):
            ph = math.pi * j / rings
            for i in range(seg + 1):
                th = 2.0 * math.pi * (i % seg) / seg
                n = (math.sin(ph) * math.cos(th), math.cos(ph), math.sin(ph) * math.sin(th))
                self.vert((c[0] + r * n[0], c[1] + r * squash * n[1], c[2] + r * n[2]), n)
        at = lambda j, i: v0 + j * (seg + 1) + i
        for i in range(seg):
            self.t.append((at(0, i), at(1, i + 1), at(1, i)))
            self.t.append((at(rings, i), at(rings - 1, i), at(rings - 1, i + 1)))
        for j in range(1, rings - 1):
            for i in range(seg):
                self.t.append((at(j, i), at(j, i + 1), at(j + 1, i)))
                self.t.append((at(j, i + 1), at(j + 1, i + 1), at(j + 1, i)))

    def quad(self, a, b, c, d, n):
        i = [self.vert(p, n) for p in (a, b, c, d)]
        self.t += [(i[0], i[1], i[2]), (i[0], i[2], i[3])]


def load_obj(path):
    m = Mesh()
    vs, vns, seen = [], [], {}
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            w = line.split()
            if not w:
                continue
            if w[0] == "v":
                vs.append(tuple(float(x) for x in w[1:4]))
            elif w[0] == "vn":
                vns.append(norm(tuple(float(x) for x in w[1:4])))
            elif w[0] == "f":
                corner = []
                for c in w[1:]:
                    p = c.split("/")
                    vi = int(p[0]); vi = vi - 1 if vi > 0 else len(vs) + vi
                    ni = None
                    if len(p) > 2 and p[2]:
                        ni = int(p[2]); ni = ni - 1 if ni > 0 else len(vns) + ni
                    corner.append((vi, ni))
                if len(corner) < 3:
                    continue
                flat = norm(cross(sub(vs[corner[1][0]], vs[corner[0][0]]), sub(vs[corner[2][0]], vs[corner[0][0]])))
                idx = []
                for vi, ni in corner:
                    n = vns[ni] if ni is not None else flat
                    key = (vi, ni if ni is not None else ("flat", flat))
                    if key not in seen:
                        seen[key] = m.vert(vs[vi], n)
                    idx.append(seen[key])
                for k in range(1, len(idx) - 1):
                    m.t.append((idx[0], idx[k], idx[k + 1]))
    if not m.t:
        raise SystemExit("no faces in %s" % path)
    return m


def surface_frame(mesh, name):
    """The rectangle of a flat surface: origin, the two sides (whole lengths), the normal."""
    pts = []
    for p in mesh.v:
        if not any(length(sub(p, q)) < 1e-4 for q in pts):
            pts.append(p)
    if len(pts) != 4:
        raise SystemExit("surface %s: %d corners, a rectangle has 4 (give it as one quad; it is cut into a grid here)" % (name, len(pts)))
    n = norm((sum(x[0] for x in mesh.n), sum(x[1] for x in mesh.n), sum(x[2] for x in mesh.n)))
    o = min(pts, key=lambda p: (round(p[1], 4), round(p[0], 4), round(p[2], 4)))       # the lowest corner, then least x, z
    others = sorted((q for q in pts if q is not o), key=lambda q: length(sub(q, o)))
    du, dv = sub(others[0], o), sub(others[1], o)
    if abs(dot(norm(du), norm(dv))) > 1e-2 or length(sub(add(add(o, du), dv), others[2])) > 1e-2:
        raise SystemExit("surface %s is not a rectangle" % name)
    if abs(dot(norm(cross(du, dv)), n)) < 0.99:
        raise SystemExit("surface %s: its normals do not agree with its plane" % name)
    if dot(cross(du, dv), n) < 0.0:
        du, dv = dv, du
    return o, du, dv, n


def placeholder():
    objs = []

    def obj(name, mesh, surface=None, is_open=False, caster=True, tint=0):
        objs.append({"name": name, "mesh": mesh, "surface": surface, "open": is_open, "caster": caster, "tint": tint})

    m = Mesh(); m.quad((-3, 0, -3), (-3, 0, 3), (3, 0, 3), (3, 0, -3), (0, 1, 0)); obj("floor", m, "floor", tint=0)
    m = Mesh(); m.quad((-3, 0, -3), (-3, 3, -3), (-3, 3, 3), (-3, 0, 3), (1, 0, 0)); obj("wall_left", m, "wall", tint=3)
    m = Mesh(); m.quad((-3, 0, 3), (-3, 3, 3), (3, 3, 3), (3, 0, 3), (0, 0, -1)); obj("wall_right", m, "wall", tint=3)
    m = Mesh(); m.box((-2.99, 0.0, -3.0), (-2.95, 0.12, 2.95)); obj("trim_left", m, tint=3)
    m = Mesh(); m.box((-2.95, 0.0, 2.95), (3.0, 0.12, 2.99)); obj("trim_right", m, tint=3)
    m = Mesh()                                                    # the sofa against the right wall
    m.box((-1.9, 0.12, 1.95), (0.5, 0.48, 2.85)); m.box((-1.9, 0.48, 2.6), (0.5, 1.0, 2.9))
    m.box((-2.15, 0.12, 1.95), (-1.9, 0.72, 2.9)); m.box((0.5, 0.12, 1.95), (0.75, 0.72, 2.9))
    obj("sofa", m, tint=2)
    m = Mesh()                                                    # the table
    m.box((-1.3, 0.38, 0.5), (-0.1, 0.45, 1.2))
    for x, z in ((-1.25, 0.55), (-0.2, 0.55), (-1.25, 1.1), (-0.2, 1.1)):
        m.box((x, 0.0, z), (x + 0.07, 0.38, z + 0.07))
    obj("table", m, tint=1)
    m = Mesh(); m.box((0.6, 0.0, 0.55), (1.2, 0.4, 1.15)); obj("ottoman_a", m, tint=2)
    m = Mesh(); m.box((-2.5, 0.0, 0.2), (-1.9, 0.4, 0.8)); obj("ottoman_b", m, tint=2)
    m = Mesh()                                                    # the lamp in the corner
    m.box((-2.6, 0.0, 2.4), (-2.3, 0.04, 2.7)); m.box((-2.47, 0.04, 2.53), (-2.43, 1.55, 2.57)); m.box((-2.65, 1.55, 2.35), (-2.25, 1.9, 2.75))
    obj("lamp", m, tint=1)
    for k, (x, z) in enumerate(((1.9, 2.4), (-2.5, -2.2))):       # the plants
        m = Mesh()
        m.box((x - 0.2, 0.0, z - 0.2), (x + 0.2, 0.4, z + 0.2))
        m.blob((x, 0.95, z), 0.5, 10, 8, 1.15)
        obj("plant_%s" % "ab"[k], m, tint=0)
    return objs, (6.5, 5.2, -6.5), (-0.6, 0.8, 0.6)


def from_files(room):
    if not os.path.exists(os.path.join(room, "room_manifest.json")) and os.path.exists(os.path.join(room, "export", "room_manifest.json")):
        room = os.path.join(room, "export")
    path = os.path.join(room, "room_manifest.json")
    with open(path, encoding="utf-8") as f:
        man = json.load(f)
    eye, target = (6.5, 5.2, -6.5), (-0.6, 0.8, 0.6)
    if isinstance(man, dict):
        cam = man.get("camera", man)
        eye = tuple(float(x) for x in cam.get("eye", eye)); target = tuple(float(x) for x in cam.get("target", target))
        man = man.get("objects", [])
    objs = []
    for e in man:
        surf = e.get("surface") or None
        if surf not in (None, "floor", "wall"):
            raise SystemExit("%s: surface must be floor, wall or null" % e.get("name"))
        mesh = load_obj(os.path.join(room, e["file"]))
        caster = bool(e.get("caster", True))
        objs.append({"name": e["name"], "mesh": mesh, "surface": surf, "open": bool(e.get("open", False)), "caster": caster,
                     "tint": int(e.get("tint", 0)) & 3, "optional": bool(e.get("optional", False)),
                     "flat": surf is None and not caster and max(p[1] for p in mesh.v) <= FLAT_MAX})
    if not objs:
        raise SystemExit("no objects in %s" % path)
    return objs, eye, target


def build(objs, grid):
    V, N, TINT, T, OBJ, CV, CT, SURF, OT = [], [], [], [], [], [], [], [], []
    OTC = build.closed = []                                       # an occluder triangle: 1 when its object is a closed solid
    for o in objs:
        m = o["mesh"]
        if o.get("flat"):
            continue
        rec = {"name": o["name"], "v0": len(V), "t0": len(T), "cv0": len(CV), "ct0": len(CT), "surf": -1, "flags": 0}
        if o["surface"]:
            org, du, dv, n = surface_frame(m, o["name"])
            nu, nv = max(1, int(math.ceil(length(du) / grid - 1e-6))), max(1, int(math.ceil(length(dv) / grid - 1e-6)))
            if o["surface"] == "floor":
                lw, lh = 128, 128
            else:
                lw, lh = (128, 64) if length(du) >= length(dv) else (64, 128)
            v0 = len(V)
            for j in range(nv + 1):
                for i in range(nu + 1):
                    V.append(add(org, add(mul(du, i / nu), mul(dv, j / nv)))); N.append(n); TINT.append(o["tint"])
            for j in range(nv):
                for i in range(nu):
                    a, b, c, d = v0 + j * (nu + 1) + i, v0 + j * (nu + 1) + i + 1, v0 + (j + 1) * (nu + 1) + i + 1, v0 + (j + 1) * (nu + 1) + i
                    T += [(a, b, c), (a, c, d)]
            rec["surf"] = len(SURF)
            SURF.append({"obj": len(OBJ), "kind": 1 if o["surface"] == "floor" else 2, "w": lw, "h": lh, "o": org, "du": du, "dv": dv, "n": n,
                         "nu": nu, "nv": nv, "v0": v0})
            c4 = [org, add(org, du), add(add(org, du), dv), add(org, dv)]
            OT += [(c4[0], c4[1], c4[2]), (c4[0], c4[2], c4[3])]
            OTC += [0, 0]
            if o["surface"] == "wall" and o["caster"]:           # a closed slab behind the wall, wound outward
                back = mul(n, -WALL_SLAB)
                c8 = c4 + [add(p, back) for p in c4]
                k0 = len(CV)
                CV += c8
                # front (toward the room, normal n), back, and the four sides; corners 0..3 run round n counter-clockwise
                for q in ((0, 1, 2, 3), (7, 6, 5, 4), (4, 5, 1, 0), (5, 6, 2, 1), (6, 7, 3, 2), (7, 4, 0, 3)):
                    CT += [(k0 + q[0], k0 + q[1], k0 + q[2]), (k0 + q[0], k0 + q[2], k0 + q[3])]
                rec["flags"] |= 2
        else:
            v0 = len(V)
            V += m.v; N += m.n; TINT += [o["tint"]] * len(m.v)
            T += [(v0 + a, v0 + b, v0 + c) for a, b, c in m.t]
            OT += [(m.v[a], m.v[b], m.v[c]) for a, b, c in m.t]
            OTC += [0 if o["open"] else 1] * len(m.t)
            if o["caster"]:
                k0 = len(CV)
                CV += m.v
                CT += [(k0 + a, k0 + b, k0 + c) for a, b, c in m.t]
                rec["flags"] |= 2
            if o["open"]:
                rec["flags"] |= 1
        rec["vn"] = len(V) - rec["v0"]; rec["tn"] = len(T) - rec["t0"]; rec["cvn"] = len(CV) - rec["cv0"]; rec["ctn"] = len(CT) - rec["ct0"]
        OBJ.append(rec)
    build.trim = 0
    for o, rec in zip([o for o in objs if not o.get("flat")], OBJ):   # trim on a wall: the wall's normal
        if o["surface"] or o["caster"]:
            continue
        for i in range(rec["v0"], rec["v0"] + rec["vn"]):
            best = None
            for s in SURF:
                if s["kind"] != 2:
                    continue
                d = sub(V[i], s["o"])
                h = dot(d, s["n"])
                a, b = dot(d, s["du"]) / dot(s["du"], s["du"]), dot(d, s["dv"]) / dot(s["dv"], s["dv"])
                if -0.02 <= h <= TRIM_NEAR and -0.01 <= a <= 1.01 and -0.01 <= b <= 1.01 and (best is None or h < best[0]):
                    best = (h, s["n"])
            if best:
                N[i] = best[1]; build.trim += 1
    for o in objs:                                                # what lay flat on the floor: its tint on the floor under it
        if o.get("flat") and not o.get("optional"):
            xs = [p[0] for p in o["mesh"].v]; zs = [p[2] for p in o["mesh"].v]
            for s in SURF:
                if s["kind"] == 1:
                    for i in range(s["v0"], s["v0"] + (s["nu"] + 1) * (s["nv"] + 1)):
                        if min(xs) - 1e-3 <= V[i][0] <= max(xs) + 1e-3 and min(zs) - 1e-3 <= V[i][2] <= max(zs) + 1e-3:
                            TINT[i] = o["tint"]
    if len(V) > 65535 or len(CV) > 65535:
        raise SystemExit("over 65,535 vertices: the indices are 16 bits")
    return V, N, TINT, T, OBJ, CV, CT, SURF, OT


def f3(p):
    return "{ %.6ff, %.6ff, %.6ff }" % (p[0], p[1], p[2])


def write(out, source, built, eye, target, grid):
    V, N, TINT, T, OBJ, CV, CT, SURF, OT = built
    os.makedirs(out, exist_ok=True)
    path = os.path.join(out, "room_mesh.h")
    L = []
    L.append("/* SH4DER-PVR: the room's geometry. WRITTEN BY demo/psm/ao/room_import.py from %s; do not edit, do not commit.\n   Included by demo/psm/ao/room_data.h, which says what each name is. */" % source)
    L.append("#define ROOM_V %uu\n#define ROOM_T %uu\n#define ROOM_OBJS %uu\n#define ROOM_CV %uu\n#define ROOM_CT %uu\n#define ROOM_SURFS %uu\n#define ROOM_OT %uu"
             .replace("%u", "%d") % (len(V), len(T), len(OBJ), len(CV), len(CT), len(SURF), len(OT)))
    L.append("#define ROOM_GRID %.4ff" % grid)
    L.append("static const float room_eye[3] = %s, room_target[3] = %s;" % (f3(eye), f3(target)))
    L.append("static const float room_cam_angle = %.6ff, room_cam_radius = %.6ff;   /* the eye round the target: radians from +x toward +z, metres on the ground */"
             % (math.atan2(eye[2] - target[2], eye[0] - target[0]), math.hypot(eye[0] - target[0], eye[2] - target[2])))
    L.append("static const float room_v[ROOM_V][3] = {\n" + ",\n".join("    " + f3(p) for p in V) + "\n};")
    L.append("static const float room_n[ROOM_V][3] = {\n" + ",\n".join("    " + f3(p) for p in N) + "\n};")
    L.append("static const uint8_t room_tint[ROOM_V] = {\n    " + ", ".join(str(x) for x in TINT) + "\n};")
    L.append("static const uint16_t room_t[ROOM_T][3] = {\n" + ",\n".join("    { %d, %d, %d }" % t for t in T) + "\n};")
    L.append("static const float room_cv[ROOM_CV][3] = {\n" + ",\n".join("    " + f3(p) for p in CV) + "\n};")
    L.append("static const uint16_t room_ct[ROOM_CT][3] = {\n" + ",\n".join("    { %d, %d, %d }" % t for t in CT) + "\n};")
    L.append("static const room_obj_t room_obj[ROOM_OBJS] = {\n" + ",\n".join(
        '    { "%s", %d, %d, %d, %d, %d, %d, %d, %d, %d, %d }' % (o["name"][:15], o["v0"], o["vn"], o["t0"], o["tn"], o["cv0"], o["cvn"], o["ct0"], o["ctn"], o["surf"], o["flags"])
        for o in OBJ) + "\n};")
    L.append("static const room_surf_t room_surf[ROOM_SURFS] = {\n" + ",\n".join(
        "    { %d, %d, %d, %d, %d, %d, %d, %s, %s, %s, %s }" % (s["obj"], s["kind"], s["w"], s["h"], s["nu"], s["nv"], s["v0"], f3(s["o"]), f3(s["du"]), f3(s["dv"]), f3(s["n"]))
        for s in SURF) + "\n};")
    L.append("static const float room_ot[ROOM_OT][3][3] = {\n" + ",\n".join("    { %s, %s, %s }" % (f3(a), f3(b), f3(c)) for a, b, c in OT) + "\n};")
    L.append("static const uint8_t room_ot_closed[ROOM_OT] = {\n    " + ", ".join(str(x) for x in build.closed) + "\n};")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(L) + "\n")
    return path


def main():
    ap = argparse.ArgumentParser(description="A room of OBJ files, or the placeholder room, as C arrays.")
    ap.add_argument("--room")
    ap.add_argument("--placeholder", action="store_true")
    ap.add_argument("--out")
    ap.add_argument("--grid", type=float, default=0.25)
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args()
    if a.self_test:
        objs, eye, target = placeholder()
        built = build(objs, 0.25)
        V, N, TINT, T, OBJ, CV, CT, SURF, OT = built
        assert len(SURF) == 3 and SURF[0]["nu"] == 24 and SURF[0]["nv"] == 24 and (SURF[0]["w"], SURF[0]["h"]) == (128, 128)
        assert SURF[1]["nu"] * SURF[1]["nv"] == 24 * 12 and sorted((SURF[1]["w"], SURF[1]["h"])) == [64, 128]
        assert len(V) == len(N) == len(TINT) and all(abs(length(n) - 1.0) < 1e-4 for n in N)
        assert all(max(t) < len(V) for t in T) and all(max(t) < len(CV) for t in CT)
        assert OBJ[1]["ctn"] == 12 and OBJ[0]["ctn"] == 0 and len(OT) == len(T) - sum(2 * s["nu"] * s["nv"] for s in SURF) + 6
        with tempfile.TemporaryDirectory() as d:
            p = write(d, "the self-test", built, eye, target, 0.25)
            assert os.path.getsize(p) > 10000
        print("room_import self-test: ok (%d vertices, %d triangles, %d caster triangles)" % (len(V), len(T), len(CT)))
        return 0
    if not a.out or bool(a.room) == bool(a.placeholder):
        ap.error("give --out and one of --room or --placeholder")
    objs, eye, target = placeholder() if a.placeholder else from_files(a.room)
    built = build(objs, a.grid)
    path = write(a.out, "the placeholder room" if a.placeholder else a.room, built, eye, target, a.grid)
    V, N, TINT, T, OBJ, CV, CT, SURF, OT = built
    print("%s: %d objects, %d vertices, %d triangles on screen; %d caster triangles; %d surfaces (lightmaps %s); %d occluder triangles" % (
        path, len(OBJ), len(V), len(T), len(CT), len(SURF), ", ".join("%dx%d" % (s["w"], s["h"]) for s in SURF), len(OT)))
    print("  %d trim vertices lit with their wall's normal" % build.trim)
    for o in objs:
        if o.get("flat"):
            print("  %-14s left out: flat on the floor, casts nothing%s" % (o["name"], "" if o.get("optional") else "; its tint on the floor under it"))
    for o in OBJ:
        print("  %-14s v %5d  t %5d  caster t %4d  %s%s" % (o["name"], o["vn"], o["tn"], o["ctn"], "surface " if o["surf"] >= 0 else "", "open" if o["flags"] & 1 else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
