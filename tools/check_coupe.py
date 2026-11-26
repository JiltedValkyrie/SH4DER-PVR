"""The hero car has no hole: a class OBJ drawn with the back faces culled shows no back face from above.

    python tools/check_coupe.py COUPE.obj      prints COUPE_CLOSED, or each hole and exit code 1
    python tools/check_coupe.py --self-test    a closed box passes, a box with a crack fails

The demo culls back faces (gfx.c, by the signed screen area), so a crack between two panels shows the road through
the car. The test draws the car with a depth buffer from 25 directions above the ground (8 azimuths x 10, 25 and 45
degrees, and straight down), in parallel projection at 1 cm a pixel. A pixel whose nearest surface faces away from the
eye, lies above HOLE_Y and has a nearer front face within ENCLOSE pixels on both sides (left and right, or above and
below) is a hole: a gap the panels close round. A thin lip seen edge on from behind (a wheel arch, a bumper's end) has
open ground on one side, so a silhouette is never counted. A ray from above falls as it goes, so it reaches the inside
of the shell only through a gap in the panels above that point: the open floor of the shell is never counted. Wheels
and trim count as surfaces. Standard library only; about 4 s on a desktop. Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE. Began in demo/pl4nar/tools (2026-11-26: copied here; the old
path is a forward).
"""
import math
import sys

PX = 0.01          # metres a pixel
HOLE_Y = 0.45      # back faces under this are the shell's floor seen through its own open bottom
MIN_PX = 2         # a view with fewer hole pixels than this is not counted (an edge the raster hits on a pixel centre)
ENCLOSE = 6        # pixels: how far to look for the panels round a gap
NEARER = 0.01      # metres: a panel round a gap is this much nearer the eye than what shows through it


def load_obj(path):
    v, f = [], []
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            s = line.split()
            if not s:
                continue
            if s[0] == "v":
                v.append(tuple(float(x) for x in s[1:4]))
            elif s[0] == "f":
                idx = [int(x.split("/")[0]) - 1 for x in s[1:]]
                for k in range(1, len(idx) - 1):
                    f.append((idx[0], idx[k], idx[k + 1]))
    return v, f


def views():
    out = [(0.0, 90.0)]
    for el in (10.0, 25.0, 45.0):
        out += [(az, el) for az in range(0, 360, 45)]
    return out


def holes_in_view(v, f, az, el):
    """The hole pixels seen looking down along (az, el): a list of (x, y, z) hit points."""
    a, e = math.radians(az), math.radians(el)
    d = (-math.cos(e) * math.sin(a), -math.sin(e), -math.cos(e) * math.cos(a))      # the eye looks along d
    r = (math.cos(a), 0.0, -math.sin(a))                                            # screen right
    u = (d[1] * r[2] - d[2] * r[1], d[2] * r[0] - d[0] * r[2], d[0] * r[1] - d[1] * r[0])   # screen up
    dot = lambda p, q: p[0] * q[0] + p[1] * q[1] + p[2] * q[2]                       # noqa: E731
    sx = [dot(p, r) / PX for p in v]
    sy = [dot(p, u) / PX for p in v]
    sz = [dot(p, d) for p in v]                                                      # depth along the view
    x0, y0 = int(math.floor(min(sx))) - 1, int(math.floor(min(sy))) - 1
    w, h = int(math.ceil(max(sx))) - x0 + 2, int(math.ceil(max(sy))) - y0 + 2
    zb, back, hit = [1e30] * (w * h), [False] * (w * h), [None] * (w * h)
    for t in f:
        p0, p1, p2 = (v[i] for i in t)
        n = ((p1[1] - p0[1]) * (p2[2] - p0[2]) - (p1[2] - p0[2]) * (p2[1] - p0[1]),
             (p1[2] - p0[2]) * (p2[0] - p0[0]) - (p1[0] - p0[0]) * (p2[2] - p0[2]),
             (p1[0] - p0[0]) * (p2[1] - p0[1]) - (p1[1] - p0[1]) * (p2[0] - p0[0]))
        nd = dot(n, d)
        if abs(nd) < 1e-12:
            continue                                                                 # edge on: covers nothing
        is_back = nd > 0.0
        ax, ay, bx, by, cx, cy = sx[t[0]] - x0, sy[t[0]] - y0, sx[t[1]] - x0, sy[t[1]] - y0, sx[t[2]] - x0, sy[t[2]] - y0
        area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax)
        if abs(area) < 1e-12:
            continue
        inv = 1.0 / area
        za, zb_, zc = sz[t[0]], sz[t[1]], sz[t[2]]
        for py in range(max(0, int(min(ay, by, cy))), min(h, int(max(ay, by, cy)) + 2)):
            cyp = py + 0.5
            for px in range(max(0, int(min(ax, bx, cx))), min(w, int(max(ax, bx, cx)) + 2)):
                cxp = px + 0.5
                w0 = ((bx - cxp) * (cy - cyp) - (by - cyp) * (cx - cxp)) * inv
                w1 = ((cx - cxp) * (ay - cyp) - (cy - cyp) * (ax - cxp)) * inv
                w2 = 1.0 - w0 - w1
                if w0 < 0.0 or w1 < 0.0 or w2 < 0.0:
                    continue
                z = w0 * za + w1 * zb_ + w2 * zc
                k = py * w + px
                if z < zb[k]:
                    zb[k], back[k] = z, is_back
                    hit[k] = tuple(w0 * p0[i] + w1 * p1[i] + w2 * p2[i] for i in range(3))
    def front_nearer(px, py, sx_, sy_, z):
        for s in range(1, ENCLOSE + 1):
            x, y = px + sx_ * s, py + sy_ * s
            if not (0 <= x < w and 0 <= y < h):
                return False
            k = y * w + x
            if hit[k] is not None and not back[k] and zb[k] < z - NEARER:
                return True
        return False

    out = []
    for k in range(w * h):
        if not back[k] or hit[k][1] <= HOLE_Y:
            continue
        px, py, z = k % w, k // w, zb[k]
        if (front_nearer(px, py, -1, 0, z) and front_nearer(px, py, 1, 0, z)) or (front_nearer(px, py, 0, -1, z) and front_nearer(px, py, 0, 1, z)):
            out.append(hit[k])
    return out


def check(v, f, quiet=False):
    """The views with holes: a list of (az, el, count, a sample point)."""
    bad = []
    for az, el in views():
        hs = holes_in_view(v, f, az, el)
        if len(hs) >= MIN_PX:
            mid = hs[len(hs) // 2]
            bad.append((az, el, len(hs), mid))
            if not quiet:
                print("hole: view az %3d el %2d: %d px, e.g. at x %.3f y %.3f z %.3f" % (az, el, len(hs), mid[0], mid[1], mid[2]))
    return bad


def box(open_crack):
    """A 1 x 1 x 1 box from y 0.5, every face outward; open_crack lifts the far half of the top by 3 cm (a step with no
    riser, the fault this test is for)."""
    lo, hi = (-0.5, 0.5, -0.5), (0.5, 1.5, 0.5)
    c = [(x, y, z) for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])]
    v = list(c)
    f = []
    quads = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (0, 2, 6, 4), (1, 5, 7, 3)]        # -x, +x, floor, -z, +z: the top below
    for q in quads:
        f += [(q[0], q[1], q[2]), (q[0], q[2], q[3])]
    lift = 0.03 if open_crack else 0.0
    base = len(v)
    v += [(-0.5, 1.5, 0.5), (0.5, 1.5, 0.5), (0.5, 1.5, 0.0), (-0.5, 1.5, 0.0),
          (-0.5, 1.5 + lift, 0.0), (0.5, 1.5 + lift, 0.0), (0.5, 1.5 + lift, -0.5), (-0.5, 1.5 + lift, -0.5)]
    f += [(base, base + 1, base + 2), (base, base + 2, base + 3), (base + 4, base + 5, base + 6), (base + 4, base + 6, base + 7)]
    return v, f


def fix_winding(v, f):
    """The box's faces wound outward (the box is convex round (0, 1, 0))."""
    out = []
    for t in f:
        p = [v[i] for i in t]
        n = ((p[1][1] - p[0][1]) * (p[2][2] - p[0][2]) - (p[1][2] - p[0][2]) * (p[2][1] - p[0][1]),
             (p[1][2] - p[0][2]) * (p[2][0] - p[0][0]) - (p[1][0] - p[0][0]) * (p[2][2] - p[0][2]),
             (p[1][0] - p[0][0]) * (p[2][1] - p[0][1]) - (p[1][1] - p[0][1]) * (p[2][0] - p[0][0]))
        c = [sum(q[i] for q in p) / 3.0 - (0.0, 1.0, 0.0)[i] for i in range(3)]
        out.append(t if n[0] * c[0] + n[1] * c[1] + n[2] * c[2] > 0 else (t[0], t[2], t[1]))
    return out


def self_test():
    v, f = box(False)
    ok_closed = not check(v, fix_winding(v, f), quiet=True)
    v, f = box(True)
    ok_crack = bool(check(v, fix_winding(v, f), quiet=True))
    print("SELF_TEST closed %s, crack found %s" % ("ok" if ok_closed else "FAIL", "ok" if ok_crack else "FAIL"))
    return 0 if ok_closed and ok_crack else 1


def main_path(path):
    """0 and COUPE_CLOSED when the car at path has no hole, else 1 and each hole (tools/bake.py calls this)."""
    v, f = load_obj(path)
    bad = check(v, f)
    if bad:
        print("COUPE_HOLES %d views" % len(bad))
        return 1
    print("COUPE_CLOSED %d views, %d triangles" % (len(views()), len(f)))
    return 0


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    if sys.argv[1] == "--self-test":
        return self_test()
    return main_path(sys.argv[1])


if __name__ == "__main__":
    sys.exit(main())
