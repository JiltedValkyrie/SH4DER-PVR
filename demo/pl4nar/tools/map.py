#!/usr/bin/env python3
"""The new map: a closed loop of street with kit buildings, overhead structures, lamps and parked traffic, as C data.

    python tools/map.py [--blender <exe>] [--out build/gen] [--top <picture.png>]

Writes build/gen/map_gen.c and map_gen.h (the models it needs and the layout), and with --top a top view of the
layout. Reads the fetched packs (tools/fetch.py: Modular Buildings, City Kit Commercial, City Kit Suburban, Car Kit),
build/gen/props.obj (tools/props.py), build/gen/structures.obj (tools/structures.py) and the cut models in
build/lod/ (tools/car_lod.py; the kit buildings are cut here with --blender when their cut is not there yet).

The loop: a rectangle of centre line LOOP_X x LOOP_Z metres with corners of radius LOOP_R, run counter-clockwise seen
from above (x right, z up), starting at the south end of the east side going north. Distance along it is s. The
road band is |offset| <= BAND (asphalt to 8, sidewalk to 12), all of it the plane y = 0: the planar reflection
mirrors about that plane, so no ramp and no hill; every structure is above the band, never on it. Traffic keeps to
the right: lanes at +2 and +6 (to the right of the direction of travel) run with s, -2 and -6 against it. Cars in one
lane keep one speed, so they never meet and nothing tests for a collision.

Our own work (the layout, the code); the kit models are CC0 (CREDITS.md). Host only: nothing here runs on the
Dreamcast. Copyright (c) 2026 Jilted Valkyrie. See ../../LICENSE.
"""
import argparse
import math
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bake  # noqa: E402
import structures  # noqa: E402
from PIL import Image  # noqa: E402

ROOT = bake.ROOT
A, GEN, LOD = bake.A, bake.GEN, bake.LOD
STRUCT_OBJ = [os.path.join(GEN, "structures.obj")]   # --structures

LOOP_X, LOOP_Z, LOOP_R = 200.0, 360.0, 34.0
BAND = 12.0                 # the road texture's half width: asphalt 8, sidewalk 4
SETBACK = 18.0              # a building's front from the centre line (piers stand at 15)
STEP = 6.0                  # the road mesh's step along a straight; corners in ARC_N pieces
ARC_N = 12
ROAD_TILE = 24.0            # src/demo.c: the road texture is 24 m long
LAMP_EVERY = 32.0
LAMP_OFF = 8.9              # src/demo.c: ROAD_HALF + 0.9
LAMP_ARM = 2.6
PARK = (-40.0, 40.0)        # the infield's open middle, z

# the kits: (pack folder, colour map, metres a kit unit, triangle budget, models)
KITS = {
    "tower": ("kenney_modular-buildings", 7.0, 300, ["building-sample-tower-%s" % c for c in "abcd"]),
    "house": ("kenney_modular-buildings", 7.0, 300, ["building-sample-house-%s" % c for c in "abc"]),
    "subur": ("kenney_city-kit-suburban", 11.0, 400, ["building-type-%s" % c for c in "hikr"]),
    "low":   ("kenney_city-kit-commercial", 24.0, 200, ["low-detail-building-%s" % c for c in "abcdegjklm"] + ["low-detail-building-wide-a", "low-detail-building-wide-b"]),
}
TINTS = [0xd8cbb0, 0xb8bcc4, 0xa8c0d0, 0xd0b8a0, 0xc4c8b4, 0x9ca4b0]     # the white low-detail set, tinted a building

# along the loop (s in metres from the start): the structures. The loop is LOOP_X + LOOP_Z sides; see path().
STRUCTS = [("overpass", 120.0), ("gantry", 200.0), ("gantry", 250.0)]          # on the east side (s from its corner end)
FOOTBRIDGE_AT = "north-mid"
PASSAGE = ("west", 110.0, 10)
# the lanes (src/map.c): lateral offset from the centre line (+ = the right of the travel direction (tz, -tx), the
# screen's left in the demo's frame, so forward traffic keeps to the negative side) and speed in m/s along +s (negative:
# oncoming). Four cars a lane, spread round the loop.
LANES = [(-2.0, 12.5), (-6.0, 9.5), (2.0, -11.5), (6.0, -14.0)]
CARS_A_LANE = 4                                                  # side, s from the side's start, bays of 8 m


# ------------------------------------------------------------------------------------------------ the path

def corners():
    hx, hz = LOOP_X / 2, LOOP_Z / 2
    return [(hx - LOOP_R, -hz + LOOP_R, -90.0), (hx - LOOP_R, hz - LOOP_R, 0.0), (-hx + LOOP_R, hz - LOOP_R, 90.0), (-hx + LOOP_R, -hz + LOOP_R, 180.0)]


def path():
    """The centre line as [(s, x, z, tx, tz, side name or None)], closed (the last point is the first, at s = length)."""
    hx, hz = LOOP_X / 2, LOOP_Z / 2
    sides = [("east", (hx, -hz + LOOP_R), (hx, hz - LOOP_R)), ("north", (hx - LOOP_R, hz), (-hx + LOOP_R, hz)),
             ("west", (-hx, hz - LOOP_R), (-hx, -hz + LOOP_R)), ("south", (-hx + LOOP_R, -hz), (hx - LOOP_R, -hz))]
    pts, s = [], 0.0
    cs = corners()
    for k, (name, a, b) in enumerate(sides):
        L = math.hypot(b[0] - a[0], b[1] - a[1])
        tx, tz = (b[0] - a[0]) / L, (b[1] - a[1]) / L
        n = max(1, int(round(L / STEP)))
        for i in range(n):
            pts.append((s + L * i / n, a[0] + (b[0] - a[0]) * i / n, a[1] + (b[1] - a[1]) * i / n, tx, tz, name))
        s += L
        cx, cz, a0 = cs[(k + 1) % 4]
        for i in range(ARC_N):
            t = math.radians(a0 + 90.0 * i / ARC_N)
            pts.append((s + LOOP_R * math.pi / 2 * i / ARC_N, cx + LOOP_R * math.cos(t), cz + LOOP_R * math.sin(t), -math.sin(t), math.cos(t), None))
        s += LOOP_R * math.pi / 2
    pts.append((s,) + pts[0][1:])
    return pts, s


def side_frame(name):
    """A straight side: its start point, unit direction, length, and its right (the outside of the loop)."""
    hx, hz = LOOP_X / 2, LOOP_Z / 2
    f = {"east": ((hx, -hz + LOOP_R), (0, 1)), "north": ((hx - LOOP_R, hz), (-1, 0)),
         "west": ((-hx, hz - LOOP_R), (0, -1)), "south": ((-hx + LOOP_R, -hz), (1, 0))}[name]
    (x, z), (tx, tz) = f
    L = (LOOP_Z if name in ("east", "west") else LOOP_X) - 2 * LOOP_R
    return x, z, tx, tz, L, (tz, -tx)


def yaw_of(dx, dz):
    """The yaw (src/demo.c draw_model: shz_xmtrx_rotate_y) that turns local +z to the direction (dx, dz)."""
    return math.atan2(dx, dz)


def rot(yaw, x, z):
    c, s = math.cos(yaw), math.sin(yaw)
    return x * c + z * s, -x * s + z * c


# ------------------------------------------------------------------------------------------------ models

def dedupe(src, dst):
    """The kit OBJs hold every face twice, the same corners in a second group of the same name (checked on the
    Modular Buildings, City Kit Commercial and City Kit Suburban files used here): keep each face once, one group."""
    if os.path.exists(dst):
        return
    seen, out, name = set(), [], None
    for line in open(src, encoding="utf-8"):
        t = line.split()
        if t and t[0] in ("g", "o"):
            if name is None:
                name = t[1] if len(t) > 1 else "model"
                out.append("g %s\n" % name)
            continue
        if t and t[0] == "f":
            key = tuple(sorted(int(x.split("/")[0]) for x in t[1:]))
            if key in seen:
                continue
            seen.add(key)
        out.append(line)
    with open(dst, "w", encoding="utf-8", newline="\n") as fh:
        fh.writelines(out)


def cut(blender, src, dst, budget):
    if os.path.exists(dst):
        return
    if not blender:
        raise SystemExit("%s is not cut yet: give --blender <exe>" % os.path.relpath(dst, ROOT))
    r = subprocess.run([blender, "-b", "--factory-startup", "--python", os.path.join(ROOT, "tools", "car_lod.py"), "--", src, dst,
                        "--body", str(budget), "--wheel", str(budget)], cwd=ROOT, capture_output=True, text=True)
    if r.returncode != 0 or "CAR_LOD_OK" not in r.stdout:
        raise SystemExit("cut failed: %s\n%s" % (src, r.stdout[-2000:] + r.stderr[-2000:]))


class Models:
    """Every model the map uses once, by name: its baked form, its size, its C name."""
    def __init__(self, blender):
        self.out, self.rep, self.by = [], [], {}
        lod, once = os.path.join(ROOT, "build", "lod_map_d"), os.path.join(ROOT, "build", "map_once")
        os.makedirs(lod, exist_ok=True)
        os.makedirs(once, exist_ok=True)
        self.kit = {}
        for kind, (folder, scale, budget, names) in KITS.items():
            pal = Image.open(os.path.join(A, folder, "colormap.png")).convert("RGB")
            for n in names:
                dst = os.path.join(lod, n + ".obj")
                dedupe(os.path.join(A, folder, n + ".obj"), os.path.join(once, n + ".obj"))
                cut(blender, os.path.join(once, n + ".obj"), dst, budget)
                self.add("mdl_map_" + n.replace("-", "_"), bake.bake_model(dst, pal, scale))
                self.kit.setdefault(kind, []).append("mdl_map_" + n.replace("-", "_"))
        st = STRUCT_OBJ[0]
        for p in ("overpass", "footbridge", "gantry", "passage"):
            self.add("mdl_map_" + p, bake.bake_class_model(st, 1.0, groups=[p + "-"]))
        self.ext = ["mdl_lamp", "mdl_pole", "mdl_signal", "mdl_cone", "mdl_dumpster", "mdl_taxi_body", "mdl_taxi_wheels",
                    "mdl_police_body", "mdl_police_wheels", "mdl_van_body", "mdl_van_wheels"]     # baked by tools/bake.py

    def add(self, cname, m):
        verts, idx, lo, hi = m
        self.by[cname] = (lo, hi)
        self.rep.append((cname, bake.emit_model(self.out, cname, m)))

    def table(self):
        return list(self.by) + self.ext


# ------------------------------------------------------------------------------------------------ the layout

def layout(models):
    inst, lamps, strips, cars = [], [], [], []
    names = models.table()
    idx = {n: k for k, n in enumerate(names)}
    lohi = dict(models.by)
    rng = [12345]

    def rnd(n):
        rng[0] = (rng[0] * 1103515245 + 12345) & 0x7fffffff
        return (rng[0] >> 8) % n

    def place(cname, x, z, yaw, tint=0xffffff, wheels=None):
        inst.append((idx[cname], idx[wheels] if wheels else 0xffff, x, z, yaw, tint))

    # structures on the loop
    sx, sz, tx, tz, L, (rx, rz) = side_frame("east")
    for piece, at in STRUCTS:
        place("mdl_map_" + piece, sx + tx * at, sz + tz * at, yaw_of(tx, tz))
    busy = [("east", at, 18.0 if p == "overpass" else 3.0) for p, at in STRUCTS]
    x, z, tx, tz, L, _r = side_frame("north")
    place("mdl_map_footbridge", x + tx * L / 2, z + tz * L / 2, yaw_of(tx, tz))
    busy.append(("north", L / 2, 3.0))
    side, s0, bays = PASSAGE
    x, z, tx, tz, L, _r = side_frame(side)
    for b in range(bays):
        place("mdl_map_passage", x + tx * (s0 + 8.0 * b), z + tz * (s0 + 8.0 * b), yaw_of(tx, tz))
    busy.append((side, s0 + 4.0 * bays, 4.0 * bays + 2.0))

    def is_busy(name, s, pad):
        return any(n == name and abs(s - c) < h + pad for n, c, h in busy)

    # buildings: a row each side of each straight, fronts SETBACK from the centre line, facing the road
    plan = {("east", 1): "tower", ("north", 1): "tower", ("west", 1): "subur", ("south", 1): "house",
            ("east", -1): "low", ("north", -1): "low", ("west", -1): "low", ("south", -1): "low"}
    for (name, out), kind in plan.items():
        x0, z0, tx, tz, L, (rx, rz) = side_frame(name)
        nx, nz = rx * out, rz * out                       # from the road toward the row
        s = 6.0
        while s < L - 6.0:
            cname = models.kit[kind][rnd(len(models.kit[kind]))]
            lo, hi = lohi[cname]
            w, d = hi[0] - lo[0], hi[2] - lo[2]           # along the road (local x), into the block (local z)
            px, pz = x0 + tx * (s + w / 2), z0 + tz * (s + w / 2)
            if out < 0 and PARK[0] < pz < PARK[1] and name in ("east", "west"):
                s += w + 3.0
                continue
            if s + w > L - 6.0:
                break
            yaw = yaw_of(-nx, -nz)                        # local +z (the front) toward the road
            cx, cz = rot(yaw, (lo[0] + hi[0]) / 2, (lo[2] + hi[2]) / 2)
            back = SETBACK + d / 2
            place(cname, px + nx * back - cx, pz + nz * back - cz, yaw, TINTS[rnd(len(TINTS))] if kind == "low" else 0xffffff)
            s += w + 2.0 + rnd(3)
    # lamps, both sides, every LAMP_EVERY metres of each straight, none in a structure
    for name in ("east", "north", "west", "south"):
        x0, z0, tx, tz, L, (rx, rz) = side_frame(name)
        k = 0
        s = 10.0
        while s < L - 4.0:
            for out in (1, -1):
                if is_busy(name, s, 4.0):
                    continue
                px, pz = x0 + tx * s + rx * out * LAMP_OFF, z0 + tz * s + rz * out * LAMP_OFF
                yaw = yaw_of(rx * out, rz * out)          # local -z (the arm) toward the road: local +z away from it
                place("mdl_lamp", px, pz, yaw)
                hx, hz = rot(yaw, 0.0, -LAMP_ARM)
                lamps.append((px + hx, pz + hz))
            s += LAMP_EVERY
            k += 1
    # the lit strips' middles (for their glow at night): from the structures' own groups
    for i in inst:
        n = names[i[0]]
        if n in ("mdl_map_overpass", "mdl_map_footbridge", "mdl_map_passage", "mdl_map_gantry"):
            for x0, x1, z0, z1, ly in structures.STRIPS[n[len("mdl_map_"):]]:
                wx, wz = rot(i[4], (x0 + x1) / 2, (z0 + z1) / 2)
                strips.append((i[2] + wx, ly, i[3] + wz))
    # the self-driving traffic (src/map.c moves it): body, wheels, lane, start s. Not in map_inst.
    _pts, total = path()
    for li, (lat, _v) in enumerate(LANES):
        for k in range(CARS_A_LANE):
            body = ["taxi", "police", "van"][(k + li) % 3]
            cars.append((idx["mdl_%s_body" % body], idx["mdl_%s_wheels" % body], li, (k * total / CARS_A_LANE + li * 23.0) % total))
    return inst, lamps, strips, cars, names


def road_mesh():
    """Pairs of vertices (left, right of the travel direction) at each path point: x, z, u, v."""
    pts, total = path()
    rows = []
    for s, x, z, tx, tz, _n in pts:
        rx, rz = tz, -tx
        rows.append((x - rx * BAND, z - rz * BAND, 1.0, s / ROAD_TILE))
        rows.append((x + rx * BAND, z + rz * BAND, 0.0, s / ROAD_TILE))
    return rows, total


# ------------------------------------------------------------------------------------------------ output

def shots(total):
    """Poses for the pictures: x, z, heading (src/demo.c s_car.h: the car's +z turned by it) on the road."""
    out = {}
    sx, sz, tx, tz, L, (rx, rz) = side_frame("east")
    at = STRUCTS[0][1] - 3.0
    out["overpass"] = (sx + tx * at + rx * 2.0, sz + tz * at + rz * 2.0, yaw_of(tx, tz))
    side, s0, bays = PASSAGE
    x, z, tx, tz, L, (rx, rz) = side_frame(side)
    at = s0 + 8.0 * bays * 0.55
    out["passage"] = (x + tx * at + rx * 2.0, z + tz * at + rz * 2.0, yaw_of(tx, tz))
    x, z, tx, tz, L, (rx, rz) = side_frame("south")
    at = L * 0.4
    out["open"] = (x + tx * at + rx * 2.0, z + tz * at + rz * 2.0, yaw_of(tx, tz))
    return out


def s_lat(x, z):
    """(s, lat) on the loop of a world point: the nearest centre line segment, lat along (tz, -tx)."""
    pts, _total = path()
    best = None
    for a, b in zip(pts, pts[1:]):
        dx, dz = b[1] - a[1], b[2] - a[2]
        L2 = dx * dx + dz * dz
        t = max(0.0, min(1.0, ((x - a[1]) * dx + (z - a[2]) * dz) / L2))
        px, pz = a[1] + dx * t, a[2] + dz * t
        d2 = (x - px) ** 2 + (z - pz) ** 2
        if best is None or d2 < best[0]:
            L = math.sqrt(L2)
            best = (d2, a[0] + (b[0] - a[0]) * t, (x - px) * dz / L - (z - pz) * dx / L)
    return best[1], best[2]


def write_c(out_dir, models, inst, lamps, strips, names, rows, total, poses, cars):
    c = ["/* Generated by tools/map.py. Do not edit. */", '#include "assets.h"', '#include "map_gen.h"', ""]
    c += models.out
    c.append("const mdl_t *const map_models[MAP_MODELS] = { %s };" % ", ".join("&" + n for n in names))
    c.append("const map_inst_t map_inst[MAP_INST] = {")
    for m, w, x, z, yaw, tint in inst:
        c.append("  { %du, %du, %.3ff, %.3ff, %.5ff, 0x%06xu }," % (m, w, x, z, yaw, tint))
    c.append("};")
    c.append("const float map_road[MAP_ROAD_V][4] = {")
    for r in rows:
        c.append("  { %.3ff, %.3ff, %.1ff, %.4ff }," % r)
    c.append("};")
    c.append("const float map_lamp[MAP_LAMPS][2] = {" + ",".join("{%.3ff,%.3ff}" % l for l in lamps) + "};")
    c.append("const float map_strip[MAP_STRIPS][3] = {" + ",".join("{%.3ff,%.3ff,%.3ff}" % s for s in strips) + "};")
    pts, _total = path()
    c.append("const float map_path[MAP_PATH_N][4] = {")
    for q in pts:
        c.append("  { %.3ff, %.3ff, %.3ff, %.5ff }," % (q[0], q[1], q[2], yaw_of(q[3], q[4])))
    c.append("};")
    c.append("const float map_lane[MAP_LANES][2] = {" + ",".join("{%.2ff,%.2ff}" % l for l in LANES) + "};")
    c.append("const map_car_t map_car[MAP_CARS] = {" + ",".join("{%du,%du,%du,%.3ff}" % q for q in cars) + "};")
    with open(os.path.join(out_dir, "map_gen.c"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(c) + "\n")
    h = ["/* Generated by tools/map.py. Do not edit. */", "#ifndef MAP_GEN_H", "#define MAP_GEN_H", "#include <stdint.h>",
         '#include "assets.h"',
         "typedef struct map_inst { uint16_t model, wheels; float x, z, yaw; uint32_t tint; } map_inst_t;   /* wheels 0xffff: none */",
         "typedef struct map_car { uint16_t body, wheels, lane; float s0; } map_car_t;   /* map_models indices, map_lane index, start s */",
         "#define MAP_PATH_N %du" % len(path()[0]), "#define MAP_MAX_NV %du" % max(nv for _n, (nv, _t) in models.rep), "#define MAP_LANES %du" % len(LANES), "#define MAP_CARS %du" % len(cars),
         "#define MAP_MODELS %du" % len(names), "#define MAP_INST %du" % len(inst), "#define MAP_ROAD_V %du" % len(rows),
         "#define MAP_LAMPS %du" % len(lamps), "#define MAP_STRIPS %du" % len(strips), "#define MAP_LOOP_LEN %.3ff" % total,
         "#define MAP_LOOP_X %.1ff" % LOOP_X, "#define MAP_LOOP_Z %.1ff" % LOOP_Z, "#define MAP_BAND %.1ff" % BAND]
    for k, (x, z, hd) in poses.items():
        h.append("#define MAP_SHOT_%s %.3ff, %.3ff, %.5ff" % (k.upper(), x, z, hd))
        h.append("#define MAP_SHOT_%s_SL %.3ff, %.3ff   /* s, lat */" % ((k.upper(),) + s_lat(x, z)))
    h += ["extern const mdl_t *const map_models[MAP_MODELS];", "extern const map_inst_t map_inst[MAP_INST];",
          "extern const float map_road[MAP_ROAD_V][4];   /* pairs: left, right of the travel direction; x, z, u, v */",
          "extern const float map_lamp[MAP_LAMPS][2];    /* the lamp heads, x, z (y LAMP_HEAD_Y) */",
          "extern const float map_strip[MAP_STRIPS][3];  /* the lit strips' middles */",
          "extern const float map_path[MAP_PATH_N][4];   /* the centre line, closed (the last point is the first at s = MAP_LOOP_LEN): s, x, z, yaw */",
          "extern const float map_lane[MAP_LANES][2];    /* lat, speed along +s (m/s; negative: oncoming) */",
          "extern const map_car_t map_car[MAP_CARS];", "#endif"]
    with open(os.path.join(out_dir, "map_gen.h"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(h) + "\n")


def top_view(path_png, models, inst, lamps, strips, names, rows, poses):
    """The layout from above, from the same data: road, footprints (bounding boxes turned), structures, lamps, cars."""
    from PIL import ImageDraw
    S, W, H = 2.0, 330.0, 490.0
    img = Image.new("RGB", (int(W * S), int(H * S)), (30, 34, 30))
    d = ImageDraw.Draw(img)
    P = lambda x, z: ((x + W / 2) * S, (H / 2 - z) * S)          # noqa: E731
    d.rectangle([P(-40, PARK[1]), P(40, PARK[0])], fill=(52, 92, 58))
    for k in range(0, len(rows) - 2, 2):
        q = [rows[k], rows[k + 1], rows[k + 3], rows[k + 2]]
        d.polygon([P(r[0], r[1]) for r in q], fill=(70, 70, 68))
    lohi = dict(models.by)
    cols = {"tower": (196, 150, 110), "house": (176, 140, 100), "subur": (120, 170, 130), "low": (150, 160, 180)}
    kind_of = {c: k for k, v in models.kit.items() for c in v}
    for m, w, x, z, yaw, tint in inst:
        n = names[m]
        if n in lohi:
            lo, hi = lohi[n]
            pts = [rot(yaw, a, b) for a, b in ((lo[0], lo[2]), (hi[0], lo[2]), (hi[0], hi[2]), (lo[0], hi[2]))]
            col = cols.get(kind_of.get(n), (255, 150, 60) if "overpass" in n or "footbridge" in n else (120, 200, 255) if "gantry" in n else (255, 230, 90))
            d.polygon([P(x + a, z + b) for a, b in pts], fill=col if n in kind_of else None, outline=(20, 20, 20) if n in kind_of else col)
        elif "body" in n:
            fx, fz = rot(yaw, 0.0, 2.2)
            d.line([P(x - fx, z - fz), P(x + fx, z + fz)], fill=(230, 60, 50), width=4)
    for x, z in lamps:
        d.ellipse([P(x - 1.2, z + 1.2), P(x + 1.2, z - 1.2)], fill=(255, 200, 110))
    for x, _y, z in strips:
        d.ellipse([P(x - 0.8, z + 0.8), P(x + 0.8, z - 0.8)], fill=(230, 240, 255))
    for k, (x, z, hd) in poses.items():
        fx, fz = rot(hd, 0.0, 6.0)
        d.line([P(x, z), P(x + fx, z + fz)], fill=(255, 255, 0), width=3)
        d.text(P(x + 5, z), "shot: " + k, fill=(255, 255, 0))
    d.text((8, 8), "new map: loop %d x %d m, R %d, road band %d m, %d m round; %d instances, %d lamps, %d lit strips"
           % (LOOP_X, LOOP_Z, LOOP_R, 2 * BAND, path()[1], len(inst), len(lamps), len(strips)), fill=(255, 255, 255))
    img.save(path_png)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--blender", default=os.environ.get("BLENDER", ""))
    ap.add_argument("--out", default=GEN)
    ap.add_argument("--top", default="")
    ap.add_argument("--structures", default=os.path.join(GEN, "structures.obj"))
    a = ap.parse_args()
    STRUCT_OBJ[0] = a.structures
    for f in (a.structures, os.path.join(GEN, "props.obj")):
        if not os.path.exists(f):
            raise SystemExit("no %s: run tools/%s first" % (f, "props.py" if f.endswith("props.obj") else "structures.py"))
    models = Models(a.blender)
    inst, lamps, strips, cars, names = layout(models)
    rows, total = road_mesh()
    poses = shots(total)
    os.makedirs(a.out, exist_ok=True)
    write_c(a.out, models, inst, lamps, strips, names, rows, total, poses, cars)
    if a.top:
        top_view(a.top, models, inst, lamps, strips, names, rows, poses)
    tri = sum(nt for _n, (_nv, nt) in models.rep)
    print("MAP_OK loop %.0f m, %d models (%d tris baked here), %d instances, %d lamps, %d strips, %d road vertices, %d cars"
          % (total, len(names), tri, len(inst), len(lamps), len(strips), len(rows), len(cars)))
    for n, (nv, nt) in models.rep:
        print("  %-36s %5d verts %5d tris" % (n, nv, nt))


if __name__ == "__main__":
    main()
