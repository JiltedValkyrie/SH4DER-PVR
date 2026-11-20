#!/usr/bin/env python3
"""SH4DER-PVR: smoothed reflection normals for sh4der_pl4nar_shade.

A reflection reads best off a smooth normal: on a low polygon body the face normals make every flat face a flat
mirror, and the picture jumps from face to face. This tool gives every corner of every triangle the area-weighted
mean of the face normals that meet at its position and lie within an angle of its own face (60 degrees by default):
a curved panel bends the picture, a hard edge stays an edge. Light the surface with its own normals as before; use
these for the lookup alone.

    python tools/pl4nar_normals.py IN.obj OUT.obj [--angle 60] [--weld 0.0001]
    python tools/pl4nar_normals.py --self-test

IN.obj: v and f lines (polygons are fanned; its vt and vn are not read). OUT.obj: the same positions in the same
order, one vn a corner, f lines v//vn in the source order, so corner k of triangle t is the same corner in both
files. A degenerate triangle keeps its place with a normal of 0 1 0. An existing OUT.obj is refused.
No numpy. Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.
"""
import argparse
import math
import os
import sys


def load_obj(path):
    """Positions and triangles (0-based position indices), polygons fanned."""
    verts, tris = [], []
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            t = line.split()
            if not t:
                continue
            if t[0] == "v":
                verts.append((float(t[1]), float(t[2]), float(t[3])))
            elif t[0] == "f":
                c = []
                for x in t[1:]:
                    i = int(x.split("/")[0])
                    c.append(i - 1 if i > 0 else len(verts) + i)
                for k in range(1, len(c) - 1):
                    tris.append((c[0], c[k], c[k + 1]))
    return verts, tris


def face(a, b, c):
    """Area and unit normal of a triangle; (0, None) when it has no area."""
    ux, uy, uz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    vx, vy, vz = c[0] - a[0], c[1] - a[1], c[2] - a[2]
    nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
    ln = math.sqrt(nx * nx + ny * ny + nz * nz)
    if ln < 1e-12:
        return 0.0, None
    return 0.5 * ln, (nx / ln, ny / ln, nz / ln)


def smooth(verts, tris, angle_deg=60.0, weld=1e-4):
    """One normal a corner: [(n0, n1, n2), ...] in triangle order."""
    cos_min = math.cos(math.radians(angle_deg))
    inv = 1.0 / weld if weld > 0 else 0.0

    def key(p):
        return tuple(round(x * inv) for x in p) if inv else p

    faces = [face(verts[a], verts[b], verts[c]) for a, b, c in tris]
    ring = {}
    for (area, n), tri in zip(faces, tris):
        if n is None:
            continue
        for i in tri:
            ring.setdefault(key(verts[i]), []).append((area, n))
    out = []
    for (_area, fn), tri in zip(faces, tris):
        corner = []
        for i in tri:
            if fn is None:
                corner.append((0.0, 1.0, 0.0))
                continue
            sx = sy = sz = 0.0
            for area, m in ring[key(verts[i])]:
                if m[0] * fn[0] + m[1] * fn[1] + m[2] * fn[2] > cos_min:
                    sx += area * m[0]
                    sy += area * m[1]
                    sz += area * m[2]
            ln = math.sqrt(sx * sx + sy * sy + sz * sz)
            corner.append((sx / ln, sy / ln, sz / ln) if ln > 1e-12 else fn)
        out.append(tuple(corner))
    return out


def write_obj(path, verts, tris, normals):
    if os.path.exists(path):
        raise SystemExit("refuse existing output: %s" % os.path.basename(path))
    with open(path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("# smoothed reflection normals: one vn a corner\n")
        for v in verts:
            fh.write("v %.6f %.6f %.6f\n" % v)
        for tri in normals:
            for n in tri:
                fh.write("vn %.5f %.5f %.5f\n" % n)
        for t, tri in enumerate(tris):
            fh.write("f %d//%d %d//%d %d//%d\n" % (tri[0] + 1, 3 * t + 1, tri[1] + 1, 3 * t + 2, tri[2] + 1, 3 * t + 3))


def self_test():
    # a roof of two faces 20 degrees apart (shared edge), and a wall at 90 degrees to the first
    r = math.radians(20.0)
    verts = [(0, 0, 0), (0, 0, 1), (1, 0, 0), (-math.cos(r), -math.sin(r), 0), (-math.cos(r), -math.sin(r), 1),
             (0, -1, 0), (0, -1, 1)]
    tris = [(0, 1, 2), (0, 3, 1), (3, 4, 1), (0, 1, 5), (1, 6, 5), (0, 0, 1)]
    n = smooth(verts, tris)
    assert len(n) == 6 and all(len(t) == 3 for t in n)
    for tri in n:
        for c in tri:
            assert abs(c[0] * c[0] + c[1] * c[1] + c[2] * c[2] - 1.0) < 1e-9
    flat = face(verts[0], verts[1], verts[2])[1]
    assert flat[1] > 0.999                                      # the first face looks up
    assert n[0][2] == flat                                      # its far corner meets no other face
    shared = n[0][0]                                            # on the roof's edge: between the two roof faces
    assert shared[1] < 0.9999 and shared[1] > math.cos(r) - 1e-9 and abs(shared[2]) < 1e-9
    wall = n[3][0]                                              # the wall keeps its own normal there: a hard edge
    assert abs(abs(wall[0]) - 1.0) < 1e-9
    assert n[5] == ((0.0, 1.0, 0.0),) * 3                       # no area: a placeholder, the place kept
    hard = smooth(verts, tris, angle_deg=10.0)
    assert hard[0][0] == flat                                   # 10 degrees: the roof's edge is hard too
    print("pl4nar_normals: PASS unit length, a smooth edge, a hard edge, the angle, a triangle with no area")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("src", nargs="?")
    ap.add_argument("out", nargs="?")
    ap.add_argument("--angle", type=float, default=60.0, help="faces closer than this share a normal, degrees")
    ap.add_argument("--weld", type=float, default=1e-4, help="positions closer than this are one point, model units")
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args()
    if a.self_test:
        self_test()
        return 0
    if not a.src or not a.out:
        ap.error("IN.obj and OUT.obj")
    if not 0.0 < a.angle < 180.0:
        ap.error("--angle inside 0 .. 180")
    verts, tris = load_obj(a.src)
    if not tris:
        raise SystemExit("no triangles in the source")
    write_obj(a.out, verts, tris, smooth(verts, tris, a.angle, a.weld))
    print("PL4NAR_NORMALS_OK tris %d corners %d angle %g" % (len(tris), 3 * len(tris), a.angle))
    return 0


if __name__ == "__main__":
    sys.exit(main())
