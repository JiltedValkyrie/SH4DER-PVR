#!/usr/bin/env python3
"""SH4DER-PVR: check the living room's exported OBJ files against their manifest. Host only, no Blender.

demo/psm/ao/room/tools/check_export.py
Copyright (c) 2026 Jilted Valkyrie. See ../../../../../LICENSE.

    python -I demo/psm/ao/room/tools/check_export.py [export folder]

For every object of room_manifest.json it reads the OBJ again and checks: triangles only; the triangle count; one `o`
group; the bounding box; everything inside -8 .. 8 in x and z; closed and wound one way (every directed edge once and
its reverse once) unless the manifest says open; a positive volume when closed (the faces look outward); every
exported normal on its triangle's own side. Then the same counts for room_all.obj. Exit 1 on any failure.
"""
import json
import os
import sys


def read_obj(path):
    groups, v, vn, tris = [], [], [], []
    with open(path, encoding="ascii") as f:
        for line in f:
            p = line.split()
            if not p:
                continue
            if p[0] == "o":
                groups.append(p[1])
            elif p[0] == "v":
                v.append(tuple(float(x) for x in p[1:4]))
            elif p[0] == "vn":
                vn.append(tuple(float(x) for x in p[1:4]))
            elif p[0] == "f":
                if len(p) != 4:
                    raise ValueError("%s: a face with %d corners" % (path, len(p) - 1))
                tris.append(tuple((int(c.split("/")[0]) - 1, int(c.split("/")[2]) - 1) for c in p[1:]))
    return groups, v, vn, tris


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def main(argv):
    folder = argv[1] if len(argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "export")
    with open(os.path.join(folder, "room_manifest.json"), encoding="utf-8") as f:
        man = json.load(f)
    bad, total, casters = 0, 0, 0

    def fail(msg):
        nonlocal bad
        bad += 1
        print("  FAIL " + msg)

    for o in man["objects"]:
        groups, v, vn, tris = read_obj(os.path.join(folder, o["file"]))
        total += len(tris)
        casters += len(tris) if o["caster"] else 0
        print("%-13s %5d tris  %s" % (o["name"], len(tris), "open" if o["open"] else "closed"))
        if groups != [o["name"]]:
            fail("groups %r" % groups)
        if len(tris) != o["tris"] or len(v) != o["verts"]:
            fail("counts %d / %d against the manifest's %d / %d" % (len(tris), len(v), o["tris"], o["verts"]))
        if len(v) >= 65536:
            fail("%d vertices" % len(v))
        lo = [min(p[k] for p in v) for k in range(3)]
        hi = [max(p[k] for p in v) for k in range(3)]
        if any(abs(lo[k] - o["bbox_min"][k]) > 1e-3 or abs(hi[k] - o["bbox_max"][k]) > 1e-3 for k in range(3)):
            fail("bounding box %r %r" % (lo, hi))
        if min(lo[0], lo[2]) < -8.0 or max(hi[0], hi[2]) > 8.0:
            fail("outside -8 .. 8")
        edges, vol, wrong = {}, 0.0, 0
        for t in tris:
            a, b, c = (v[i] for i, _ in t)
            n = cross(sub(b, a), sub(c, a))
            vol += dot(a, cross(b, c)) / 6.0
            wrong += sum(1 for _, ni in t if dot(n, vn[ni]) <= 0.0)
            for i in range(3):
                e = (t[i][0], t[(i + 1) % 3][0])
                edges[e] = edges.get(e, 0) + 1
        closed = all(k == 1 and edges.get((e[1], e[0]), 0) == 1 for e, k in edges.items())
        if wrong:
            fail("%d normals on the wrong side of their triangle" % wrong)
        if closed == o["open"]:
            fail("closed is %s but the manifest says open %s" % (closed, o["open"]))
        if closed and vol <= 0.0:
            fail("volume %.6f: wound inward" % vol)
    groups, v, vn, tris = read_obj(os.path.join(folder, man["combined"]))
    if len(tris) != total or groups != [o["name"] for o in man["objects"]]:
        fail("room_all.obj: %d triangles, groups %r" % (len(tris), groups))
    if total != man["triangles_total"] or casters != man["triangles_casters"]:
        fail("totals %d / %d against the manifest's %d / %d" % (total, casters, man["triangles_total"], man["triangles_casters"]))
    print("TOTAL %d triangles, casters %d; room_all.obj %d triangles" % (total, casters, len(tris)))
    print("CHECK_OK" if not bad else "%d FAILED" % bad)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
