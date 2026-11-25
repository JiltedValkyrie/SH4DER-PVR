"""The hero car, built from numbers: a 1970s two-door coupe in a PlayStation-era triangle budget.

    python tools/coupe.py OUT.obj        (an existing OUT.obj is refused)

Long hood, short deck, a boxy hardtop roof with a fastback rear window and solid sail panels, a wide centre stripe,
chrome bumpers, round lamps as squares. No stock mesh: stations along the length (half width, sill, shoulder, deck
height) are lofted into side, shoulder and top strips; the greenhouse is four stations more. Our own work, CC0.

Units: metres. +z is the front, +y up, the wheels stand on y = 0. Every OBJ group is named <part>__<class>__<rrggbb>:
the bake (tools/bake.py, class OBJ path) takes the class (paint, glass, trim, tyre, lamp) and the colour from the name,
so the reflection layer has its paint and glass. Chrome is class paint with a grey colour: it takes the additive layer.
Budget: body <= 350 triangles, the car <= 500 (printed, and checked).
"""
import math
import os
import sys

PAINT = "960e16"       # deep red
STRIPE = "e8e4d8"      # off-white
GLASS = "1c2430"
CHROME = "aab0ba"
BLACK = "18181c"
TYRE = "141416"
LAMP_F = "f4e8a0"
LAMP_R = "c81818"

# z, half width, sill y, deck y (hood / roof base / boot), from the tail to the nose
STATIONS = [(-2.38, 0.80, 0.40, 0.80), (-2.20, 0.88, 0.30, 0.86), (-1.55, 0.92, 0.27, 0.90), (-0.60, 0.93, 0.27, 0.92),
            (0.55, 0.93, 0.27, 0.93), (1.30, 0.92, 0.27, 0.90), (2.05, 0.89, 0.30, 0.85), (2.36, 0.82, 0.40, 0.78)]
SHOULDER_DROP = 0.13   # the shoulder line sits this far under the deck
SHOULDER_IN = 0.09     # and the deck's edge this far inside the side
STRIPE_HALF = 0.22
# the greenhouse: z, half width, y  (windscreen base, roof front, roof rear, rear window base)
ROOF = [(0.55, 0.80, 0.93), (0.02, 0.64, 1.31), (-0.85, 0.64, 1.30), (-1.72, 0.76, 0.905)]
WHEEL_Z = (1.45, -1.40)
WHEEL_R, WHEEL_W, WHEEL_X = 0.33, 0.22, 0.96      # radius, width, the outer face's |x|
ARCH_R = 0.42


class Mesh:
    def __init__(self):
        self.v, self.groups = [], {}

    def add(self, group, pts, hint):
        """A convex polygon as a fan, wound so its normal agrees with hint."""
        a, b, c = pts[0], pts[1], pts[2]
        e1 = [b[i] - a[i] for i in range(3)]
        e2 = [c[i] - a[i] for i in range(3)]
        n = (e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0])
        if n[0] * hint[0] + n[1] * hint[1] + n[2] * hint[2] < 0:
            pts = pts[::-1]
        base = len(self.v)
        self.v += [tuple(p) for p in pts]
        for k in range(1, len(pts) - 1):
            self.groups.setdefault(group, []).append((base, base + k, base + k + 1))

    def tris(self, prefix=None, skip=None):
        return sum(len(t) for g, t in self.groups.items()
                   if (prefix is None or g.startswith(prefix)) and (skip is None or not g.startswith(skip)))

    def write(self, path):
        if os.path.exists(path):
            raise SystemExit("refuse existing output: %s" % os.path.basename(path))
        with open(path, "w", encoding="utf-8", newline="\n") as fh:
            fh.write("# a 1970s coupe from tools/coupe.py: groups are <part>__<class>__<rrggbb>. CC0.\n")
            for p in self.v:
                fh.write("v %.4f %.4f %.4f\n" % p)
            for g in sorted(self.groups):
                fh.write("g %s\n" % g)
                for t in self.groups[g]:
                    fh.write("f %d %d %d\n" % (t[0] + 1, t[1] + 1, t[2] + 1))


def body(m):
    paint, stripe = "body__paint__" + PAINT, "stripe__paint__" + STRIPE
    for (z0, w0, s0, d0), (z1, w1, s1, d1) in zip(STATIONS, STATIONS[1:]):
        for sx in (-1.0, 1.0):
            # the side, sill to shoulder; the shoulder, up and in to the deck's edge
            m.add(paint, [(sx * w0, s0, z0), (sx * w1, s1, z1), (sx * w1, d1 - SHOULDER_DROP, z1), (sx * w0, d0 - SHOULDER_DROP, z0)], (sx, 0, 0))
            m.add(paint, [(sx * w0, d0 - SHOULDER_DROP, z0), (sx * w1, d1 - SHOULDER_DROP, z1), (sx * (w1 - SHOULDER_IN), d1, z1),
                          (sx * (w0 - SHOULDER_IN), d0, z0)], (sx, 1, 0))
            # the deck beside the stripe
            m.add(paint, [(sx * (w0 - SHOULDER_IN), d0, z0), (sx * (w1 - SHOULDER_IN), d1, z1), (sx * STRIPE_HALF, d1 + 0.012, z1),
                          (sx * STRIPE_HALF, d0 + 0.012, z0)], (0, 1, 0))
        m.add(stripe, [(-STRIPE_HALF, d0 + 0.012, z0), (STRIPE_HALF, d0 + 0.012, z0), (STRIPE_HALF, d1 + 0.012, z1), (-STRIPE_HALF, d1 + 0.012, z1)], (0, 1, 0))
    for (z, w, s, d), nz, part, lamp in ((STATIONS[0], -1.0, "tail", LAMP_R), (STATIONS[-1], 1.0, "nose", LAMP_F)):
        top = d - SHOULDER_DROP
        # the end panel: a dark grille or tail panel between the lamps, the lamps at the corners, paint above
        m.add("%s__paint__%s" % (part, PAINT), [(-w, top, z), (w, top, z), (w - SHOULDER_IN, d, z), (-(w - SHOULDER_IN), d, z)], (0, 0, nz))
        m.add("%s-panel__trim__%s" % (part, BLACK), [(-w + 0.30, s + 0.06, z), (w - 0.30, s + 0.06, z), (w - 0.30, top, z), (-w + 0.30, top, z)], (0, 0, nz))
        for sx in (-1.0, 1.0):
            m.add("%s-lamp__lamp__%s" % (part, lamp), [(sx * (w - 0.30), s + 0.06, z), (sx * w, s + 0.06, z), (sx * w, top, z), (sx * (w - 0.30), top, z)], (0, 0, nz))
        m.add("%s-valance__paint__%s" % (part, PAINT), [(-w, s, z), (w, s, z), (w, s + 0.06, z), (-w, s + 0.06, z)], (0, 0, nz))
        # the chrome bumper: a bar proud of the panel (front, top, bottom, two ends)
        zb, y0, y1, wb = z + nz * 0.09, s + 0.05, s + 0.17, w + 0.03
        ch = "%s-bumper__paint__%s" % (part, CHROME)
        m.add(ch, [(-wb, y0, zb), (wb, y0, zb), (wb, y1, zb), (-wb, y1, zb)], (0, 0, nz))
        m.add(ch, [(-wb, y1, zb), (wb, y1, zb), (wb, y1, z), (-wb, y1, z)], (0, 1, 0))
        m.add(ch, [(-wb, y0, zb), (wb, y0, zb), (wb, y0, z), (-wb, y0, z)], (0, -1, 0))
        for sx in (-1.0, 1.0):
            m.add(ch, [(sx * wb, y0, zb), (sx * wb, y1, zb), (sx * wb, y1, z - nz * 0.25), (sx * wb, y0, z - nz * 0.25)], (sx, 0, 0))


def greenhouse(m):
    paint, stripe, glass = "roof__paint__" + PAINT, "stripe__paint__" + STRIPE, "glass__glass__" + GLASS
    (z0, w0, y0), (z1, w1, y1), (z2, w2, y2), (z3, w3, y3) = ROOF
    m.add(glass, [(-w0, y0, z0), (w0, y0, z0), (w1, y1, z1), (-w1, y1, z1)], (0, 1, 1))            # the windscreen
    m.add(glass, [(-w2, y2, z2), (w2, y2, z2), (w3, y3, z3), (-w3, y3, z3)], (0, 1, -1))           # the fastback rear window
    for sx in (-1.0, 1.0):
        m.add(paint, [(sx * w1, y1, z1), (sx * w2, y2, z2), (sx * STRIPE_HALF, y2 + 0.008, z2), (sx * STRIPE_HALF, y1 + 0.008, z1)], (0, 1, 0))
        # the side: the door glass under the roof, a solid sail panel behind it
        zq = z2 + 0.12                                                                               # where the door glass ends
        yq = y0 + (y3 - y0) * (zq - z0) / (z3 - z0)                                                  # the belt line there
        wq = w0 + (w3 - w0) * (zq - z0) / (z3 - z0)
        m.add(glass, [(sx * w0, y0, z0), (sx * w1, y1, z1), (sx * w2, y2, zq), (sx * wq, yq, zq)], (sx, 0.4, 0))
        m.add(paint, [(sx * wq, yq, zq), (sx * w2, y2, zq), (sx * w2, y2, z2), (sx * w3, y3, z3)], (sx, 0.4, 0))
    m.add(stripe, [(-STRIPE_HALF, y1 + 0.008, z1), (STRIPE_HALF, y1 + 0.008, z1), (STRIPE_HALF, y2 + 0.008, z2), (-STRIPE_HALF, y2 + 0.008, z2)], (0, 1, 0))


def wheels(m):
    n = 8
    ring = [(math.cos(2 * math.pi * (k + 0.5) / n), math.sin(2 * math.pi * (k + 0.5) / n)) for k in range(n)]
    for zi, zc in enumerate(WHEEL_Z):
        for sx, side in ((-1.0, "l"), (1.0, "r")):
            name = "wheel-%s%s" % ("fb"[zi], side)
            xo, xi = sx * WHEEL_X, sx * (WHEEL_X - WHEEL_W)
            # the dark arch on the body's side: a half octagon just proud of the panel (no hole is cut)
            arch = [(sx * 0.935, WHEEL_R + ARCH_R * math.sin(a), zc + ARCH_R * math.cos(a)) for a in [math.pi * k / 4 for k in range(5)]]
            arch = [(x, max(y, 0.27), z) for x, y, z in arch]
            m.add("arch-%s%s__trim__%s" % ("fb"[zi], side, BLACK), arch, (sx, 0, 0))
            for k in range(n):                                                                       # the tread
                (c0, s0), (c1, s1) = ring[k], ring[(k + 1) % n]
                m.add("%s__tyre__%s" % (name, TYRE), [(xo, WHEEL_R + WHEEL_R * s0, zc + WHEEL_R * c0), (xo, WHEEL_R + WHEEL_R * s1, zc + WHEEL_R * c1),
                                                        (xi, WHEEL_R + WHEEL_R * s1, zc + WHEEL_R * c1), (xi, WHEEL_R + WHEEL_R * s0, zc + WHEEL_R * c0)],
                      (0, 0.5 * (s0 + s1), 0.5 * (c0 + c1)))
            m.add("%s__tyre__%s" % (name, TYRE), [(xo, WHEEL_R + WHEEL_R * s, zc + WHEEL_R * c) for c, s in ring], (sx, 0, 0))            # the side wall
            m.add("%s-cap__trim__%s" % (name, CHROME), [(xo + sx * 0.012, WHEEL_R + 0.6 * WHEEL_R * s, zc + 0.6 * WHEEL_R * c) for c, s in ring], (sx, 0, 0))


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    m = Mesh()
    body(m)
    greenhouse(m)
    wheels(m)
    nb, nw = m.tris(skip="wheel"), m.tris(prefix="wheel")
    if nb > 350 or nb + nw > 500:
        raise SystemExit("over the budget: body %d (350), car %d (500)" % (nb, nb + nw))
    m.write(sys.argv[1])
    print("COUPE_OK body %d wheels %d car %d tris" % (nb, nw, nb + nw))


if __name__ == "__main__":
    main()
