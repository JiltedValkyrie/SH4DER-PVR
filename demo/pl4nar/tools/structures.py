"""The new map's overhead structures, built from numbers: an overpass, a footbridge, a sign gantry, a covered passage.

    python tools/structures.py OUT.obj        (an existing OUT.obj is refused)

Boxes only (tools/props.py's box, tools/coupe.py's Mesh), our own work, CC0. One OBJ, every group named
<piece>-<part>__<class>__<rrggbb> (the bake's class OBJ path); tools/map.py picks a piece by its prefix and places it.
Units: metres, +y up. Local frame of every piece: +z along the road (the direction of travel), x across it, the origin
on the road's centre line at y = 0. The road band (asphalt and sidewalks) is |x| <= 12: nothing stands inside it, so
the driving surface stays the plane y = 0 that the reflection mirrors about. Everything here is above it.
The lit strips are class lamp: the draw gives them their own colour, unshaded (a light, day and night), and the
mirror pass sees them from below, so they show on the car's roof and hood.
  overpass    a deck 14 m wide across the road, its underside at 7 m, parapets, four piers at |x| 15, six strips under and a lit edge
  footbridge  a deck 3 m wide at 6 m, rails, two piers at |x| 14, one strip under
  gantry      two posts at |x| 13, a beam at 6 m, a sign facing the traffic (local -z), lamp bars
  passage     one 8 m bay of a covered passage: a roof at 6.5 m over |x| <= 13.5, columns at |x| 12.6 (z 0), four strips
              along the ceiling. tools/map.py lays bays end to end.
Budget: each piece <= 120 triangles (printed, and checked).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coupe import Mesh  # noqa: E402
from props import box   # noqa: E402

CONCRETE = "8c8a86"
DARK = "34363a"
STEEL = "5a5e64"
RAIL = "6c7076"
SIGN = "1f6a3c"
STRIP = "f4f8ff"       # cool white: the lit strips
EDGE = "fff0d0"        # warm white: the lit deck edge and the gantry bars


# Every lit strip facing down, per piece: (x0, x1, z0, z1, y), local frame. tools/map.py takes the glow middles from it.
STRIPS = {
    "overpass": [(-11.0, 11.0, z - 0.5, z + 0.5, 6.98) for z in (-5.5, -3.3, -1.1, 1.1, 3.3, 5.5)],
    "footbridge": [(-10.0, 10.0, -0.5, 0.5, 5.98)],
    "gantry": [(-12.0, 12.0, -0.25, 0.25, 6.58)],
    "passage": [(x - 0.5, x + 0.5, 0.4, 7.6, 6.48) for x in (-7.5, -2.5, 2.5, 7.5)],
}


def strip(m, group, x0, x1, z0, z1, y):
    """A lit strip: one quad facing down at height y."""
    m.add(group, [(x0, y, z0), (x1, y, z0), (x1, y, z1), (x0, y, z1)], (0, -1, 0))


def strips(m, piece, rgb=STRIP):
    for x0, x1, z0, z1, y in STRIPS[piece]:
        strip(m, piece + "-strip__lamp__" + rgb, x0, x1, z0, z1, y)


def edge(m, group, x0, x1, y0, y1, z, nz):
    """A lit band on a vertical face at z, facing nz (+1 or -1): a deck's lit edge, a lamp bar's face."""
    if nz > 0:
        m.add(group, [(x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z)], (0, 0, 1))
    else:
        m.add(group, [(x1, y0, z), (x0, y0, z), (x0, y1, z), (x1, y1, z)], (0, 0, -1))


def overpass(m):
    box(m, "overpass-deck__trim__" + CONCRETE, (-24.0, 7.0, -7.0), (24.0, 8.2, 7.0), bottom=True)
    for z in (-7.0, 6.6):
        box(m, "overpass-parapet__trim__" + CONCRETE, (-24.0, 8.2, z), (24.0, 9.1, z + 0.4))
    for x in (-15.0, 15.0):
        for z in (-4.5, 4.5):
            box(m, "overpass-pier__trim__" + DARK, (x - 0.6, 0.0, z - 0.6), (x + 0.6, 7.0, z + 0.6))
    strips(m, "overpass")
    for z, nz in ((-7.02, -1), (7.02, 1)):                             # the deck's lit edge, both faces, under the parapets
        edge(m, "overpass-edge__lamp__" + EDGE, -24.0, 24.0, 7.0, 7.35, z, nz)


def footbridge(m):
    box(m, "footbridge-deck__trim__" + CONCRETE, (-16.0, 6.0, -1.5), (16.0, 6.5, 1.5), bottom=True)
    for z in (-1.5, 1.4):
        box(m, "footbridge-rail__trim__" + RAIL, (-16.0, 6.5, z), (16.0, 7.6, z + 0.1))
    for x in (-14.0, 14.0):
        box(m, "footbridge-pier__trim__" + DARK, (x - 0.4, 0.0, -0.4), (x + 0.4, 6.0, 0.4))
    strips(m, "footbridge")


def gantry(m):
    for x in (-13.0, 13.0):
        box(m, "gantry-post__trim__" + STEEL, (x - 0.25, 0.0, -0.25), (x + 0.25, 7.2, 0.25))
    box(m, "gantry-beam__trim__" + STEEL, (-13.25, 6.6, -0.3), (13.25, 7.2, 0.3), bottom=True)
    for x0, x1 in ((-9.0, -2.0), (2.0, 9.0)):                          # two signs, one over each pair of lanes
        box(m, "gantry-sign__trim__" + SIGN, (x0, 6.0, -0.45), (x1, 8.4, -0.3), bottom=True)
    strips(m, "gantry", EDGE)                                          # the lamp bar under the beam
    edge(m, "gantry-bar__lamp__" + EDGE, -12.0, 12.0, 6.6, 6.85, 0.32, 1)   # its face toward the cars that have passed
    for x0, x1 in ((-9.0, -2.0), (2.0, 9.0)):                          # a bar over each sign, toward the traffic
        edge(m, "gantry-bar__lamp__" + EDGE, x0, x1, 8.4, 8.6, -0.47, -1)


def passage(m):
    box(m, "passage-roof__trim__" + CONCRETE, (-13.5, 6.5, 0.0), (13.5, 7.1, 8.0), bottom=True)
    for x in (-12.6, 12.6):
        box(m, "passage-column__trim__" + DARK, (x - 0.35, 0.0, -0.35), (x + 0.35, 6.5, 0.35))
    strips(m, "passage")


PIECES = ("overpass", "footbridge", "gantry", "passage")


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    m = Mesh()
    for f in (overpass, footbridge, gantry, passage):
        f(m)
    rows = [(p, m.tris(prefix=p + "-")) for p in PIECES]
    over = [p for p, n in rows if n > 120]
    if over:
        raise SystemExit("over the budget (120): %s" % ", ".join(over))
    m.write(sys.argv[1])
    print("STRUCTURES_OK " + " ".join("%s %d" % r for r in rows) + " tris")


if __name__ == "__main__":
    main()
