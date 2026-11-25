"""The street furniture, built from numbers: a street lamp, a wooden pole with a cross arm, a traffic signal, a dumpster.

    python tools/props.py OUT.obj        (an existing OUT.obj is refused)

Boxes only (tools/coupe.py's Mesh, its fan winding by a hint), our own work, CC0. One OBJ, every group named
<prop>-<part>__<class>__<rrggbb> (the bake's class OBJ path): tools/bake.py picks a prop by its prefix (lamp, pole,
signal, dumpster). Units: metres, +y up, standing on y = 0. Each prop's facing is what src/demo.c places it with
(shz_xmtrx_rotate_y: world x = x cos a + z sin a, world z = -x sin a + z cos a):
  lamp      the head hangs LAMP_ARM out along local -z at LAMP_HEAD_Y (demo.c: hx = x - LAMP_ARM sin(yaw), y 7.45)
  pole      the cross arm runs along local z; the wires hang at local z +-1.1, y 6.35 (demo.c draws them)
  signal    the arm reaches over the road along local +x, the lamps face local -z (the traffic coming up the street)
  dumpster  1.8 x 1.15 x 1.2 m
Budget: each prop <= 60 triangles (printed, and checked); a box is 10 (no bottom) or 12.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coupe import Mesh  # noqa: E402

LAMP_HEAD_Y = 7.45     # src/demo.c LAMP_HEAD_Y
LAMP_ARM = 2.6         # src/demo.c LAMP_ARM
WIRE_Y = 6.35          # src/demo.c draw_props: the wires
WIRE_Z = 1.1

STEEL = "5a5e64"
DARK = "2a2c30"
WOOD = "4a3a2c"
SODIUM = "ffd890"
RED, AMBER, GREEN = "e02018", "f0a020", "20d060"
DUMP = "2c5a3a"


def box(m, group, lo, hi, bottom=False):
    """An axis-aligned box, faces wound outward; the bottom face only if asked (most stand on the ground)."""
    x0, y0, z0 = lo
    x1, y1, z1 = hi
    m.add(group, [(x0, y1, z0), (x1, y1, z0), (x1, y1, z1), (x0, y1, z1)], (0, 1, 0))
    if bottom:
        m.add(group, [(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)], (0, -1, 0))
    m.add(group, [(x0, y0, z0), (x0, y1, z0), (x0, y1, z1), (x0, y0, z1)], (-1, 0, 0))
    m.add(group, [(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)], (1, 0, 0))
    m.add(group, [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)], (0, 0, -1))
    m.add(group, [(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], (0, 0, 1))


def lamp(m):
    box(m, "lamp-base__trim__" + DARK, (-0.18, 0.0, -0.18), (0.18, 0.6, 0.18))
    box(m, "lamp-post__trim__" + STEEL, (-0.08, 0.6, -0.08), (0.08, LAMP_HEAD_Y + 0.25, 0.08))
    box(m, "lamp-arm__trim__" + STEEL, (-0.05, LAMP_HEAD_Y + 0.15, -LAMP_ARM - 0.1), (0.05, LAMP_HEAD_Y + 0.25, 0.05), bottom=True)
    box(m, "lamp-hood__trim__" + DARK, (-0.22, LAMP_HEAD_Y + 0.02, -LAMP_ARM - 0.4), (0.22, LAMP_HEAD_Y + 0.15, -LAMP_ARM + 0.4))
    m.add("lamp-glass__lamp__" + SODIUM, [(-0.18, LAMP_HEAD_Y, -LAMP_ARM - 0.35), (0.18, LAMP_HEAD_Y, -LAMP_ARM - 0.35),
                                         (0.18, LAMP_HEAD_Y, -LAMP_ARM + 0.35), (-0.18, LAMP_HEAD_Y, -LAMP_ARM + 0.35)], (0, -1, 0))


def pole(m):
    box(m, "pole-post__trim__" + WOOD, (-0.13, 0.0, -0.13), (0.13, 7.4, 0.13))
    box(m, "pole-arm__trim__" + WOOD, (-0.07, WIRE_Y + 0.07, -WIRE_Z - 0.25), (0.07, WIRE_Y + 0.21, WIRE_Z + 0.25), bottom=True)
    for z in (-WIRE_Z, WIRE_Z):                                       # the insulators the wires hang from
        box(m, "pole-ins__trim__" + DARK, (-0.04, WIRE_Y, z - 0.04), (0.04, WIRE_Y + 0.07, z + 0.04), bottom=True)


def signal(m):
    box(m, "signal-post__trim__" + DARK, (-0.1, 0.0, -0.1), (0.1, 5.6, 0.1))
    box(m, "signal-arm__trim__" + DARK, (0.0, 5.35, -0.06), (4.2, 5.5, 0.06), bottom=True)
    box(m, "signal-head__trim__" + DARK, (3.4, 4.15, -0.12), (3.75, 5.35, 0.12), bottom=True)
    for k, col in enumerate((RED, AMBER, GREEN)):                     # three lamps on the face toward local -z
        y = 5.05 - 0.37 * k
        m.add("signal-lamp%d__lamp__%s" % (k, col), [(3.45, y - 0.14, -0.13), (3.70, y - 0.14, -0.13), (3.70, y + 0.14, -0.13), (3.45, y + 0.14, -0.13)], (0, 0, -1))


def dumpster(m):
    box(m, "dumpster-bin__trim__" + DUMP, (-0.9, 0.12, -0.575), (0.9, 1.15, 0.575))
    box(m, "dumpster-lid__trim__" + DARK, (-0.92, 1.15, -0.6), (0.92, 1.2, 0.6))
    for x in (-0.75, 0.75):                                           # the wheels as blocks
        box(m, "dumpster-foot__trim__" + DARK, (x - 0.08, 0.0, -0.45), (x + 0.08, 0.12, 0.45))


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    m = Mesh()
    for f in (lamp, pole, signal, dumpster):
        f(m)
    rows = [(p, m.tris(prefix=p + "-")) for p in ("lamp", "pole", "signal", "dumpster")]
    over = [p for p, n in rows if n > 60]
    if over:
        raise SystemExit("over the budget (60): %s" % ", ".join(over))
    m.write(sys.argv[1])
    print("PROPS_OK " + " ".join("%s %d" % r for r in rows) + " tris")


if __name__ == "__main__":
    main()
