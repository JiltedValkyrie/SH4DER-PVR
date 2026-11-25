"""SH4DER-PVR: the living room for the AO scene: modelled, rendered and exported from inside Blender.

demo/psm/ao/room/tools/build_room.py
Copyright (c) 2026 Jilted Valkyrie. See ../../../../../LICENSE.

Run inside Blender (tools/blender_run.py build_room.py --pre "STEP=3"). Every mesh is made here from numbers: no asset
from anywhere else. The work is in the scene 'dc_room' only; nothing else in the open file is touched.

  STEP 1  blockout: floor, two walls, a box for every piece of furniture
  STEP 2  the shell's trim, the floor border, the rug, the wall frame
  STEP 3  sofa          STEP 4  coffee table and vases      STEP 5  ottomans
  STEP 6  lamp          STEP 7  plants
  STEP 8  final: the clay render with ambient occlusion, the occlusion alone, the OBJ files, the manifest, the .blend

Blender space: metres, Z up, floor top at z = 0, floor -3 .. 3 in x and y, walls at x = -3 ("left") and y = +3
("right": the back-right of the isometric view). Export space: Y up, (x, y, z) -> (x, z, -y), a rotation, so the
winding holds. Faces are counter-clockwise seen from outside. Things that touch a neighbour sink 1 cm into it, so no
two objects share a plane.
"""
import json
import math
import os
import random

import bmesh
import bpy
from mathutils import Matrix, Vector

ROOT = r"D:\Workspace-Claude\dc-homebrew\sh4der-pvr\demo\psm\ao\room"
STEP = globals().get("STEP", 8)
SCENE = "dc_room"
WALL_H = 2.8
SINK = 0.01


# ---- parts: lists of vertices and faces, wound outward -------------------------------------------------------------

class Part:
    def __init__(self, verts, faces, smooth=None, is_open=False):
        self.verts = [tuple(v) for v in verts]
        self.faces = [tuple(f) for f in faces]
        self.smooth = list(smooth) if smooth else [False] * len(self.faces)
        self.is_open = is_open

    def xf(self, m):
        self.verts = [tuple(m @ Vector(v)) for v in self.verts]
        return self

    def volume(self):
        vol = 0.0
        for f in self.faces:
            a = Vector(self.verts[f[0]])
            for i in range(1, len(f) - 1):
                vol += a.dot(Vector(self.verts[f[i]]).cross(Vector(self.verts[f[i + 1]])))
        return vol / 6.0


def T(x, y, z):
    return Matrix.Translation((x, y, z))


def R(axis, deg):
    return Matrix.Rotation(math.radians(deg), 4, axis)


def rect(cx, cy, hx, hy, z):
    return [(cx - hx, cy - hy, z), (cx + hx, cy - hy, z), (cx + hx, cy + hy, z), (cx - hx, cy + hy, z)]


def circle(r, z, n, phase=0.0):
    return [(r * math.cos(phase + 2 * math.pi * k / n), r * math.sin(phase + 2 * math.pi * k / n), z) for k in range(n)]


def ring_stack(rings, smooth=False, closed=False):
    """Loops of n points, each counter-clockwise seen from above, bottom to top. Capped at both ends, or (closed)
    the last loop joined to the first: a ring with no caps."""
    n, m = len(rings[0]), len(rings)
    verts = [p for r in rings for p in r]
    faces, sm = [], []
    if not closed:
        faces.append(tuple(reversed(range(n)))); sm.append(False)
    for i in range(m if closed else m - 1):
        lo, up = i * n, ((i + 1) % m) * n
        for j in range(n):
            faces.append((lo + j, lo + (j + 1) % n, up + (j + 1) % n, up + j)); sm.append(smooth)
    if not closed:
        faces.append(tuple(range((m - 1) * n, m * n))); sm.append(False)
    return Part(verts, faces, sm)


def box(x0, x1, y0, y1, z0, z1):
    cx, cy, hx, hy = (x0 + x1) / 2, (y0 + y1) / 2, (x1 - x0) / 2, (y1 - y0) / 2
    return ring_stack([rect(cx, cy, hx, hy, z0), rect(cx, cy, hx, hy, z1)])


def chamfer_box(cx, cy, hx, hy, z0, z1, top, c, bottom=None):
    """A box whose top edge is cut back by c over the last `top` of its height (and its bottom edge, when given)."""
    rings = []
    if bottom:
        rings.append(rect(cx, cy, hx - c, hy - c, z0)); rings.append(rect(cx, cy, hx, hy, z0 + bottom))
    else:
        rings.append(rect(cx, cy, hx, hy, z0))
    rings.append(rect(cx, cy, hx, hy, z1 - top)); rings.append(rect(cx, cy, hx - c, hy - c, z1))
    return ring_stack(rings)


def lathe(profile, n, phase=0.0):
    """profile: (radius, z) from the bottom up. Sides smooth, caps flat."""
    return ring_stack([circle(r, z, n, phase) for r, z in profile], smooth=True)


def tube(a, b, ra, rb, n):
    a, b = Vector(a), Vector(b)
    d = b - a
    p = ring_stack([circle(ra, 0.0, n, math.pi / n), circle(rb, d.length, n, math.pi / n)])
    return p.xf(Matrix.Translation(a) @ Vector((0, 0, 1)).rotation_difference(d).to_matrix().to_4x4())


def frame_ring(cx, cy, hx, hy, width, z0, z1):
    """A mitred rectangular ring in the xy plane, z0 .. z1 thick: closed, no caps."""
    return ring_stack([rect(cx, cy, hx, hy, z0), rect(cx, cy, hx, hy, z1),
                       rect(cx, cy, hx - width, hy - width, z1), rect(cx, cy, hx - width, hy - width, z0)], closed=True)


def pillow(half, thick):
    """A square throw pillow standing in the xz plane, its base at z = 0: four corners and two centres, 8 triangles."""
    c = [(-half, 0, 0), (half, 0, 0), (half, 0, 2 * half), (-half, 0, 2 * half)]
    verts = c + [(0, -thick / 2, half), (0, thick / 2, half)]
    faces = []
    for k in range(4):
        faces.append((k, (k + 1) % 4, 4))          # the -y side: counter-clockwise seen from -y
        faces.append(((k + 1) % 4, k, 5))
    return Part(verts, faces)


def leaf(base, tip, width, fold):
    """A leaf card: a diamond folded along its midrib, two triangles, one-sided (the object is marked open)."""
    base, tip = Vector(base), Vector(tip)
    d = tip - base
    length = d.length
    d.normalize()
    side = d.cross(Vector((0, 0, 1)))
    if side.length < 1e-4:
        side = Vector((1, 0, 0))
    side.normalize()
    up = side.cross(d)
    if up.z < 0:
        up, side = -up, -side
    mid = base + d * (0.45 * length)
    l, r = mid + side * width + up * fold, mid - side * width + up * fold
    return Part([base, r, tip, l], [(0, 1, 2), (0, 2, 3)], is_open=True)


# Wall space (u along the wall, v up, w out of the wall into the room) to the room; both are rotations.
WALL_LEFT = Matrix(((0, 0, 1, -3.0), (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 0, 1)))        # x = -3 + w, y = u, z = v
WALL_RIGHT = Matrix(((1, 0, 0, 0), (0, 0, -1, 3.0), (0, 1, 0, 0), (0, 0, 0, 1)))       # x = u, y = 3 - w, z = v


# ---- the objects ---------------------------------------------------------------------------------------------------

def make_floor():
    return [Part(rect(0, 0, 3, 3, 0.0), [(0, 1, 2, 3)], is_open=True)]


def make_wall(m):
    return [Part(rect(0, WALL_H / 2, 3, WALL_H / 2, 0.0), [(0, 1, 2, 3)], is_open=True).xf(m)]


def make_trim():
    parts = []
    for m, u0, rings in ((WALL_LEFT, -3.0, ((-1.8, 0.8), (1.8, 0.8))), (WALL_RIGHT, -3.0 + 0.025, ((-1.8, 0.8), (0.0, 0.8), (1.8, 0.8)))):
        parts.append(box(u0, 3.0, -SINK, 0.12, -SINK, 0.025).xf(m))                    # baseboard
        parts.append(box(u0 + (0.005 if u0 > -3.0 else 0), 3.0, 0.92, 0.98, -SINK, 0.03).xf(m))   # chair rail
        parts.append(box(u0 + (0.025 if u0 > -3.0 else 0), 3.0, WALL_H - 0.11, WALL_H, -SINK, 0.05).xf(m))   # crown
        for cu, hu in rings:                                                          # panel mouldings
            parts.append(frame_ring(cu, 1.825, hu, 0.675, 0.05, -SINK, 0.02).xf(m))
    return parts


def make_border():
    return [frame_ring(0, 0, 2.88, 2.88, 0.30, -SINK, 0.006)]


def make_rug():
    return [box(-1.9, 1.9, -1.6, 1.6, -SINK, 0.012)]


def make_frame():
    return [frame_ring(0, 1.8, 0.62, 0.48, 0.09, -SINK, 0.06).xf(WALL_LEFT),
            box(-0.54, 0.54, 1.40, 2.20, -SINK, 0.025).xf(WALL_LEFT)]


def make_sofa():
    parts = []
    for sx in (-1, 1):
        for sy in (-1, 1):
            parts.append(ring_stack([rect(sx * 1.05, sy * 0.36, 0.03, 0.03, -SINK), rect(sx * 1.05, sy * 0.36, 0.045, 0.045, 0.13)]))
    parts.append(box(-1.15, 1.15, -0.465, 0.465, 0.12, 0.32))                           # base
    for sx in (-1, 1):
        parts.append(chamfer_box(sx * 1.045, 0.0, 0.115, 0.475, 0.31, 0.65, 0.05, 0.035))   # arms
    parts.append(chamfer_box(0.0, 0.3425, 0.94, 0.1325, 0.31, 0.86, 0.06, 0.04))        # back
    for cx in (-0.62, 0.0, 0.62):
        parts.append(chamfer_box(cx, -0.1325, 0.305, 0.3525, 0.31, 0.48, 0.04, 0.03))   # seat cushions
        parts.append(chamfer_box(0, 0, 0.30, 0.09, 0.0, 0.40, 0.05, 0.03, bottom=0.05).xf(T(cx, 0.13, 0.47) @ R('X', -12)))
    for sx, rz in ((-1, 28), (1, -22)):
        parts.append(pillow(0.18, 0.13).xf(T(sx * 0.70, -0.08, 0.47) @ R('Z', rz) @ R('X', -18)))
    return parts


def make_table():
    parts = [box(-0.65, 0.65, -0.30, 0.30, 0.385, 0.42), box(-0.60, 0.60, -0.25, 0.25, 0.14, 0.17)]
    for sx in (-1, 1):
        for sy in (-1, 1):
            parts.append(box(sx * 0.58 - 0.025, sx * 0.58 + 0.025, sy * 0.23 - 0.025, sy * 0.23 + 0.025, -SINK, 0.395))
    return parts


def make_vases():
    return [lathe([(0.045, -SINK), (0.08, 0.09), (0.065, 0.19), (0.03, 0.26), (0.04, 0.30)], 8).xf(T(-0.38, 0.05, 0)),
            lathe([(0.035, -SINK), (0.035, 0.13), (0.013, 0.19), (0.013, 0.26)], 6).xf(T(0.02, -0.09, 0)),
            lathe([(0.05, -SINK), (0.10, 0.05), (0.10, 0.07)], 8).xf(T(0.36, 0.04, 0)),
            lathe([(0.04, -SINK), (0.045, 0.09), (0.03, 0.11)], 6).xf(T(-0.14, 0.12, 0))]


def make_ottoman(half):
    def build():
        parts = [chamfer_box(0, 0, half, half, 0.12, 0.42, 0.05, 0.035, bottom=0.05)]
        for sx in (-1, 1):
            for sy in (-1, 1):
                c = half - 0.075
                parts.append(ring_stack([rect(sx * c, sy * c, 0.02, 0.02, -SINK), rect(sx * c, sy * c, 0.03, 0.03, 0.14)]))
        return parts
    return build


def make_lamp():
    parts = []
    for k in range(3):
        a = math.radians(90 + 120 * k)
        parts.append(tube((0.34 * math.cos(a), 0.34 * math.sin(a), -SINK), (0.02 * math.cos(a), 0.02 * math.sin(a), 1.16), 0.018, 0.013, 4))
    parts.append(lathe([(0.04, 1.12), (0.04, 1.20)], 6))
    parts.append(tube((0, 0, 1.19), (0, 0, 1.45), 0.012, 0.012, 4))
    parts.append(lathe([(0.27, 1.36), (0.19, 1.70)], 12))
    return parts


def make_plant_a():
    rnd = random.Random(7)
    top = Vector((0.03, 0.02, 1.05))
    parts = [lathe([(0.15, -SINK), (0.20, 0.36), (0.20, 0.42)], 8), tube((0, 0, 0.40), top, 0.025, 0.015, 4)]
    for k in range(12):
        az, el = math.radians(k * 137.5), math.radians(rnd.uniform(12, 62))
        length = rnd.uniform(0.38, 0.55)
        base = top + Vector((0, 0, -rnd.uniform(0.0, 0.30)))
        d = Vector((math.cos(el) * math.cos(az), math.cos(el) * math.sin(az), math.sin(el)))
        parts.append(leaf(base, base + d * length + Vector((0, 0, -0.06)), rnd.uniform(0.10, 0.14), 0.04))
    return parts


def make_plant_b():
    rnd = random.Random(11)
    parts = [lathe([(0.13, -SINK), (0.17, 0.28), (0.17, 0.33)], 8)]
    for k in range(10):
        az, lean = math.radians(k * 137.5 + 20), math.radians(rnd.uniform(4, 24))
        r0, length = rnd.uniform(0.02, 0.09), rnd.uniform(0.45, 0.75)
        base = Vector((r0 * math.cos(az), r0 * math.sin(az), 0.31))
        d = Vector((math.sin(lean) * math.cos(az), math.sin(lean) * math.sin(az), math.cos(lean)))
        parts.append(leaf(base, base + d * length, rnd.uniform(0.045, 0.07), 0.012))
    return parts


# name, step, builder, origin (x, y, z), turn about z (degrees), blockout box (sx, sy, sz) or None, manifest fields
OBJECTS = [
    ("floor",        1, make_floor,                     (0, 0, 0),        0, None, dict(surface="floor", caster=False, tint=0, movable=False, ground=True)),
    ("wall_left",    1, lambda: make_wall(WALL_LEFT),   (0, 0, 0),        0, None, dict(surface="wall", caster=False, tint=0, movable=False, ground=True)),
    ("wall_right",   1, lambda: make_wall(WALL_RIGHT),  (0, 0, 0),        0, None, dict(surface="wall", caster=False, tint=0, movable=False, ground=True)),
    ("room_trim",    2, make_trim,                      (0, 0, 0),        0, None, dict(surface=None, caster=False, tint=0, movable=False, ground=True)),
    ("floor_border", 2, make_border,                    (0, 0, 0),        0, None, dict(surface=None, caster=False, tint=0, movable=False, ground=True, optional=True)),
    ("rug",          2, make_rug,                       (0.2, 0.55, 0),   0, None, dict(surface=None, caster=False, tint=1, movable=False, ground=True)),
    ("wall_frame",   2, make_frame,                     (-3.0, 0.0, 1.3), 0, None, dict(surface=None, caster=True, tint=2, movable=False, ground=False, rests_on="wall_left")),
    ("sofa",         3, make_sofa,                      (0.2, 2.42, 0),   0, (2.3, 0.93, 0.86), dict(surface=None, caster=True, tint=1, movable=True, ground=True)),
    ("coffee_table", 4, make_table,                     (0.2, 0.75, 0),   0, (1.3, 0.6, 0.42), dict(surface=None, caster=True, tint=2, movable=True, ground=True)),
    ("vases",        4, make_vases,                     (0.2, 0.75, 0.42), 0, None, dict(surface=None, caster=True, tint=3, movable=True, ground=False, rests_on="coffee_table")),
    ("ottoman_a",    5, make_ottoman(0.275),            (-0.50, -0.40, 0), 8, (0.55, 0.55, 0.42), dict(surface=None, caster=True, tint=1, movable=True, ground=True)),
    ("ottoman_b",    5, make_ottoman(0.255),            (0.95, -0.32, 0), -17, (0.51, 0.51, 0.42), dict(surface=None, caster=True, tint=1, movable=True, ground=True)),
    ("lamp",         6, make_lamp,                      (-2.25, 2.25, 0),  0, (0.6, 0.6, 1.7), dict(surface=None, caster=True, tint=2, movable=True, ground=True)),
    ("plant_a",      7, make_plant_a,                   (2.3, 2.35, 0),    0, (0.5, 0.5, 1.5), dict(surface=None, caster=True, tint=3, movable=True, ground=True)),
    ("plant_b",      7, make_plant_b,                   (-2.45, -0.9, 0),  0, (0.4, 0.4, 0.95), dict(surface=None, caster=True, tint=3, movable=True, ground=True)),
]

CAM_TARGET = Vector((0.0, 0.0, 1.4))
CAM_EYE = CAM_TARGET + Vector((1, -1, 1)).normalized() * 14.0
CAM_SCALE = 9.2


# ---- scene ---------------------------------------------------------------------------------------------------------

def scene_get():
    sc = bpy.data.scenes.get(SCENE) or bpy.data.scenes.new(SCENE)
    for o in list(sc.collection.all_objects):
        data = o.data
        bpy.data.objects.remove(o)
        if data is not None and data.users == 0:
            {"MESH": bpy.data.meshes, "CAMERA": bpy.data.cameras, "LIGHT": bpy.data.lights}[data.id_type].remove(data)
    return sc


def clay():
    m = bpy.data.materials.get("dc_clay") or bpy.data.materials.new("dc_clay")
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    b = nt.nodes.new("ShaderNodeBsdfDiffuse")
    b.inputs["Color"].default_value = (0.72, 0.72, 0.72, 1.0)
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(b.outputs["BSDF"], out.inputs["Surface"])
    m.diffuse_color = (0.72, 0.72, 0.72, 1.0)
    return m


def ao_only_material():
    m = bpy.data.materials.get("dc_ao_only") or bpy.data.materials.new("dc_ao_only")
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    ao = nt.nodes.new("ShaderNodeAmbientOcclusion")
    ao.inputs["Distance"].default_value = 1.0
    ao.samples = 16
    em = nt.nodes.new("ShaderNodeEmission")
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(ao.outputs["AO"], em.inputs["Color"])
    nt.links.new(em.outputs["Emission"], out.inputs["Surface"])
    return m


def add_object(sc, name, parts, origin, turn, mat):
    verts, faces, smooth = [], [], []
    for p in parts:
        base = len(verts)
        verts += p.verts
        faces += [tuple(base + i for i in f) for f in p.faces]
        smooth += p.smooth
        if not p.is_open and p.volume() <= 0:
            print("WARNING %s: a part is wound inward (volume %.6f)" % (name, p.volume()))
    world_to_local = (Matrix.Translation(origin) @ R('Z', turn)).inverted() if name in WORLD_BUILT else Matrix.Identity(4)
    verts = [tuple(world_to_local @ Vector(v)) for v in verts]
    me = bpy.data.meshes.new("dc_" + name)
    me.from_pydata(verts, [], faces)
    me.polygons.foreach_set("use_smooth", smooth)
    me.update()
    me.materials.append(mat)
    ob = bpy.data.objects.new(name, me)
    ob.location = origin
    ob.rotation_euler = (0, 0, math.radians(turn))
    sc.collection.objects.link(ob)
    return ob


WORLD_BUILT = {"wall_frame"}       # built where it hangs; the rest are built about their own base


def build(sc):
    mat = clay()
    made = {}
    for name, step, fn, origin, turn, block, meta in OBJECTS:
        if step <= STEP:
            parts = fn()
            made[name] = add_object(sc, name, parts, origin, turn, mat)
        elif block:
            made[name] = add_object(sc, name, [box(-block[0] / 2, block[0] / 2, -block[1] / 2, block[1] / 2, -SINK, block[2])], origin, turn, mat)
    return made


def stage(sc):
    cam = bpy.data.objects.new("dc_camera", bpy.data.cameras.new("dc_camera"))
    cam.data.type = 'ORTHO'
    cam.data.ortho_scale = CAM_SCALE
    cam.data.clip_end = 100.0
    cam.location = CAM_EYE
    cam.rotation_euler = (CAM_TARGET - CAM_EYE).to_track_quat('-Z', 'Y').to_euler()
    sc.collection.objects.link(cam)
    sc.camera = cam
    sun = bpy.data.objects.new("dc_sun", bpy.data.lights.new("dc_sun", 'SUN'))
    sun.data.energy = 2.0
    sun.data.angle = math.radians(12)
    sun.rotation_euler = Vector((-0.60, 0.15, -0.78)).to_track_quat('-Z', 'Y').to_euler()
    sc.collection.objects.link(sun)
    w = bpy.data.worlds.get("dc_room_world") or bpy.data.worlds.new("dc_room_world")
    w.use_nodes = True
    nt = w.node_tree
    nt.nodes.clear()
    light = nt.nodes.new("ShaderNodeBackground")          # what lights the room: an even white sky
    light.inputs["Color"].default_value = (1, 1, 1, 1)
    light.inputs["Strength"].default_value = 0.55
    back = nt.nodes.new("ShaderNodeBackground")           # what the camera sees behind the room
    back.inputs["Color"].default_value = (0.17, 0.18, 0.21, 1)
    path = nt.nodes.new("ShaderNodeLightPath")
    mix = nt.nodes.new("ShaderNodeMixShader")
    out = nt.nodes.new("ShaderNodeOutputWorld")
    nt.links.new(path.outputs["Is Camera Ray"], mix.inputs[0])
    nt.links.new(light.outputs["Background"], mix.inputs[1])
    nt.links.new(back.outputs["Background"], mix.inputs[2])
    nt.links.new(mix.outputs["Shader"], out.inputs["Surface"])
    sc.world = w
    return sun, w


def render(sc, path, samples, ao_only=False, sun=None, world=None):
    sc.render.engine = 'CYCLES'
    sc.cycles.samples = samples
    sc.cycles.use_denoising = True
    sc.render.resolution_x = sc.render.resolution_y = 800
    sc.render.resolution_percentage = 100
    sc.render.image_settings.file_format = 'PNG'
    sc.render.filepath = path
    sc.view_settings.view_transform = 'Standard'
    vl = sc.view_layers[0]
    vl.material_override = ao_only_material() if ao_only else None
    if sun:
        sun.hide_render = ao_only
    bpy.ops.render.render(write_still=True, scene=sc.name)
    vl.material_override = None
    if sun:
        sun.hide_render = False


# ---- counting and export -------------------------------------------------------------------------------------------

def yup(v):
    return (v[0], v[2], -v[1])


def tri_mesh(ob):
    """The object's triangles in room space: (positions, corner normals, triangles of corner indices)."""
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    tmp = bpy.data.meshes.new("dc_tmp")
    bm.to_mesh(tmp)
    bm.free()
    mw, nm = ob.matrix_world, ob.matrix_world.to_3x3().inverted().transposed()
    pos = [tuple(mw @ v.co) for v in tmp.vertices]
    cn = [tuple((nm @ Vector(n.vector)).normalized()) for n in tmp.corner_normals]
    tris = [(tuple(tmp.loops[i].vertex_index for i in p.loop_indices), tuple(p.loop_indices)) for p in tmp.polygons]
    bpy.data.meshes.remove(tmp)
    return pos, cn, tris


def check(pos, tris):
    """Closed and wound one way: every directed edge once, its reverse once. Returns (closed, edges at fault, volume)."""
    seen = {}
    vol = 0.0
    for (a, b, c), _ in tris:
        for e in ((a, b), (b, c), (c, a)):
            seen[e] = seen.get(e, 0) + 1
        vol += Vector(pos[a]).dot(Vector(pos[b]).cross(Vector(pos[c]))) / 6.0
    bad = sum(1 for (a, b), n in seen.items() if n != 1 or seen.get((b, a), 0) != 1)
    return bad == 0, bad, vol


def obj_text(name, pos, cn, tris, v0=0, n0=0):
    lines = ["o %s" % name]
    lines += ["v %.5f %.5f %.5f" % yup(p) for p in pos]
    index, normals = {}, []
    for n in cn:
        key = tuple(round(x, 4) for x in yup(n))
        if key not in index:
            index[key] = len(normals); normals.append(key)
    lines += ["vn %.4f %.4f %.4f" % n for n in normals]
    for vs, ls in tris:
        lines.append("f " + " ".join("%d//%d" % (v0 + v + 1, n0 + index[tuple(round(x, 4) for x in yup(cn[l]))] + 1) for v, l in zip(vs, ls)))
    return lines, len(pos), len(normals)


def report(made, export):
    rows, total, casters = [], 0, 0
    combined, v0, n0 = ["# SH4DER-PVR living room, own work. Metres, Y up, counter-clockwise outward."], 0, 0
    meta = {n: m for n, _, _, _, _, _, m in OBJECTS}
    steps = {n: s for n, s, _, _, _, _, _ in OBJECTS}
    for name, ob in made.items():
        pos, cn, tris = tri_mesh(ob)
        closed, bad, vol = check(pos, tris)
        m = meta[name]
        real = steps[name] <= STEP
        total += len(tris)
        casters += len(tris) if m["caster"] else 0
        lo = [min(yup(p)[k] for p in pos) for k in range(3)]
        hi = [max(yup(p)[k] for p in pos) for k in range(3)]
        rows.append(dict(name=name, file=name + ".obj", tris=len(tris), verts=len(pos), surface=m["surface"], open=not closed,
                         caster=m["caster"], tint=m["tint"], movable=m["movable"], ground_contact=m["ground"],
                         rests_on=m.get("rests_on", "floor" if m["ground"] and name != "floor" else None),
                         optional=bool(m.get("optional")), origin=[round(x, 4) for x in yup(ob.location)],
                         turn_y_degrees=round(math.degrees(ob.rotation_euler[2]), 2),
                         bbox_min=[round(x, 4) for x in lo], bbox_max=[round(x, 4) for x in hi],
                         volume_m3=round(vol, 5) if closed else None, blockout=not real))
        print("%-13s %5d tris %5d verts  %s%s" % (name, len(tris), len(pos), "closed, volume %.4f" % vol if closed else "OPEN (%d edges)" % bad,
                                                "" if real else "  [blockout]"))
        if export:
            lines, nv, nn = obj_text(name, pos, cn, tris)
            with open(os.path.join(ROOT, "export", name + ".obj"), "w", newline="\n") as f:
                f.write("# SH4DER-PVR living room: %s. Own work. Metres, Y up, counter-clockwise outward.\n" % name + "\n".join(lines) + "\n")
            lines, nv, nn = obj_text(name, pos, cn, tris, v0, n0)
            combined += lines
            v0 += nv; n0 += nn
    print("TOTAL %d triangles; casters %d" % (total, casters))
    if export:
        with open(os.path.join(ROOT, "export", "room_all.obj"), "w", newline="\n") as f:
            f.write("\n".join(combined) + "\n")
        manifest = dict(
            note="SH4DER-PVR living room for the AO scene. Own modelling, made by tools/build_room.py. Metres, Y up, right-handed, "
                 "floor top at y = 0, room -3 .. 3 in x and z. Triangles counter-clockwise seen from outside. Blender (x, y, z) is (x, z, -y) here.",
            units="metres", up="Y", triangles_total=total, triangles_casters=casters,
            tint_key={"0": "shell", "1": "upholstery and rug", "2": "wood and metal", "3": "plants and ceramics"},
            camera=dict(type="orthographic", eye=[round(x, 4) for x in yup(CAM_EYE)], target=[round(x, 4) for x in yup(CAM_TARGET)],
                        ortho_scale_m=CAM_SCALE),
            combined="room_all.obj", objects=rows)
        with open(os.path.join(ROOT, "export", "room_manifest.json"), "w", newline="\n") as f:
            json.dump(manifest, f, indent=1)
            f.write("\n")


NAMES = {1: "01_blockout", 2: "02_shell", 3: "03_sofa", 4: "04_table_vases", 5: "05_ottomans", 6: "06_lamp", 7: "07_plants", 8: "08_clay_ao"}

for d in ("work", "export"):
    os.makedirs(os.path.join(ROOT, d), exist_ok=True)
sc = scene_get()
made = build(sc)
sun, world = stage(sc)
bpy.context.view_layer.update()
sc.view_layers[0].update()
report(made, export=STEP >= 8)
render(sc, os.path.join(ROOT, "work", NAMES[min(STEP, 8)] + ".png"), 160 if STEP >= 8 else 48, sun=sun, world=world)
if STEP >= 8:
    render(sc, os.path.join(ROOT, "work", "09_ao_only.png"), 96, ao_only=True, sun=sun, world=world)
    bpy.ops.wm.save_as_mainfile(copy=True, filepath=os.path.join(ROOT, "dc_room.blend"))
print("step %d done" % STEP)
