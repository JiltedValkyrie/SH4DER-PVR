"""Hand cut of one model's parts to a lower triangle ratio (runs INSIDE Blender). Generic form of the Halo DC port's blender_decimate_prop.py.

Blender MCP / Scripting tab:
    SOURCE_OBJ = r"<path to the model's OBJ>"        # exported from the game's mesh (the source engine reader writes this; TODO(SOURCE) below)
    OUT_OBJ    = r"<path to write the cut OBJ>"
    RATIOS     = {"body": 0.5, "helmet": 0.6}          # part name -> share of its triangles to keep; a part not listed stays whole
    MODE       = "preview"                              # "preview": cut and report, nothing written; "export": also write OUT_OBJ
    exec(open(r"<path>/tools/lod/blender_cut_parts.py", encoding="utf-8").read())

Optional globals: SEAL = {"ratio": 0.2, "maxerr": 0.05, "uv_tol": 0.03, "holes": {...}} runs the sealed cut (blender_sealed_qem.py) on the
cut parts instead of Blender's Decimate modifier. Use it for hard-surface parts with seams: every part welded into one mesh, so a cut opens
no crack at a part border. WELD_PARTS (default False) welds each part by position before the Decimate modifier.

Report: per part the source and cut triangles and vertices, and a UV error (sampled points on the cut surface against the source surface's UV at the
nearest point; the share over 0.02 of the texture, and the 95th percentile). The UV error is a hint only: a stacked or hologram surface scores high.
The decision is made on the textured copy beside the original in the viewport (the cut collection sits beside the source along Y).

Why a hand cut and not one button: a welded decimation of a chart-based mesh can open black cracks on a smooth hull, and welding charts by position
can smear texture on chart-dense interiors. Each cut is checked against the source by eye before it is written. The result is written to OUT_OBJ
with the cut parts' own corner UVs and normals.

TODO(SOURCE): SOURCE_OBJ comes from the source engine static / skeletal mesh reader (not written yet). Its part names, the scale and the up axis must match
the engine's import; the OBJ axes below assume forward -Z, up +Y, as the Halo DC port exported them.
"""
import os
import random
import sys
from pathlib import Path

import bmesh
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.interpolate import poly_3d_calc

HERE = Path(globals().get("__file__") or os.environ.get("LOD_TOOLS_DIR") or sys.exit("set LOD_TOOLS_DIR to tools/lod when exec() passes no __file__")).resolve().parent
SOURCE = Path(globals().get("SOURCE_OBJ") or sys.exit("set SOURCE_OBJ"))
OUT = Path(globals().get("OUT_OBJ") or SOURCE.with_name(SOURCE.stem + "_cut.obj"))
MODE = globals().get("MODE", "preview")
RATIOS = dict(globals().get("RATIOS") or {})
SEAL = globals().get("SEAL")
WELD = 1e-5
WELD_PARTS = bool(globals().get("WELD_PARTS", False))
SAMPLES = 2500
COLL = "lod_cut"
SRC_COLL = "lod_source"


def drop(name):
    old = bpy.data.collections.get(name)
    if old:
        for ob in list(old.objects):
            me = ob.data
            bpy.data.objects.remove(ob, do_unlink=True)
            if me and me.users == 0:
                bpy.data.meshes.remove(me)
        bpy.data.collections.remove(old)


def evaluated(ob):
    bpy.context.view_layer.update()
    ev = ob.evaluated_get(bpy.context.evaluated_depsgraph_get())
    me = ev.to_mesh()
    me.calc_loop_triangles()
    return ev, me


def uv_error(orig, cut, seed=1):
    """{"bad": share of samples whose UV is off by more than 0.02, "p95": 95th percentile UV distance} of the cut against the original."""
    random.seed(seed)
    orig.calc_loop_triangles()
    cut.calc_loop_triangles()
    ouv = orig.uv_layers.active.data
    verts, tris, tuv = [], [], []
    for lt in orig.loop_triangles:   # a triangle soup: the BVH hit names one triangle whose corner UVs interpolate exactly
        i = len(verts)
        verts += [orig.vertices[v].co.copy() for v in lt.vertices]
        tris.append((i, i + 1, i + 2))
        tuv.append([ouv[k].uv.copy() for k in lt.loops])
    bvh = BVHTree.FromPolygons(verts, tris)
    cuv = cut.uv_layers.active.data
    cts = list(cut.loop_triangles)
    errs = []
    for lt in random.choices(cts, weights=[t.area for t in cts], k=SAMPLES):
        a, b, c = [cut.vertices[v].co for v in lt.vertices]
        u, v = random.random(), random.random()
        if u + v > 1:
            u, v = 1 - u, 1 - v
        uvc = [cuv[k].uv for k in lt.loops]
        uv_cut = uvc[0] + (uvc[1] - uvc[0]) * u + (uvc[2] - uvc[0]) * v
        loc, _, idx, _ = bvh.find_nearest(a + (b - a) * u + (c - a) * v)
        if idx is None:
            continue
        t = tris[idx]
        w = poly_3d_calc([verts[t[0]], verts[t[1]], verts[t[2]]], loc)
        uv_o = tuv[idx][0] * w[0] + tuv[idx][1] * w[1] + tuv[idx][2] * w[2]
        d = uv_cut - uv_o
        errs.append(Vector(((d.x + .5) % 1 - .5, (d.y + .5) % 1 - .5)).length)
    errs.sort()
    return {"bad": round(sum(1 for e in errs if e > 0.02) / max(1, len(errs)), 3),
            "p95": round(errs[int(len(errs) * .95)], 4) if errs else 0.0}


drop(COLL)
drop(SRC_COLL)
src = bpy.data.collections.new(SRC_COLL)
cut = bpy.data.collections.new(COLL)
bpy.context.scene.collection.children.link(src)
bpy.context.scene.collection.children.link(cut)
before = set(bpy.data.objects)
bpy.ops.object.select_all(action="DESELECT")
# TODO(SOURCE): forward / up axes and the split rule must match the source engine export; these are the Halo DC port's values.
bpy.ops.wm.obj_import(filepath=str(SOURCE), forward_axis="NEGATIVE_Z", up_axis="Y", use_split_objects=True)
new = [o for o in bpy.data.objects if o not in before and o.type == "MESH"]
for o in new:
    for c in list(o.users_collection):
        c.objects.unlink(o)
    src.objects.link(o)
width = max(o.dimensions.y for o in new) * 1.2
cuts = {}
for o in new:
    key = o.name.split(".")[0]
    cp = o.copy()
    cp.data = o.data.copy()
    cp.name = key
    cut.objects.link(cp)
    cp.location.y += width
    cuts[key] = cp
    ratio = RATIOS.get(key, 1.0)
    if ratio < 1.0 and SEAL is None:
        if WELD_PARTS:
            bm = bmesh.new()
            bm.from_mesh(cp.data)
            bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=WELD)
            bm.to_mesh(cp.data)
            bm.free()
        m = cp.modifiers.new("Dec", "DECIMATE")
        m.decimate_type = "COLLAPSE"
        m.ratio = ratio
        m.use_collapse_triangulate = True

seal_report = None
if SEAL is not None:
    sys.path.insert(0, str(HERE))
    import importlib
    import blender_sealed_qem as sq
    importlib.reload(sq)   # Blender keeps modules between runs
    order = sorted(cuts)
    objs = [cuts[k] for k in order]
    P, faces0, fuv0, fpart, fnrm0 = sq.build(objs, SEAL.get("weld", 1e-4))
    before_open = sq.open_edge_length(P, faces0, [True] * len(faces0))
    cutp = [k in RATIOS for k in order]
    faces, fuv, fnrm, alive, before_tris, after_tris, stats, loop = sq.sealed_cut(P, faces0, fuv0, fpart, fnrm0, cutp, SEAL)
    sq.write_back(objs, P, faces, fuv, fpart, fnrm, alive, [i for i, k in enumerate(order) if k in RATIOS])
    seal_report = {"cut_parts_before": before_tris, "cut_parts_after": after_tris, "stats": dict(stats),
                   "open_edges_before": round(before_open, 4), "open_edges_after": round(sq.open_edge_length(P, faces, alive), 4)}
    seal_report.update(loop)

rows, t0, t1 = [], 0, 0
for o in sorted(new, key=lambda o: o.name):
    key = o.name.split(".")[0]
    ea, ma = evaluated(o)
    eb, mb = evaluated(cuts[key])
    row = {"part": key, "ratio": RATIOS.get(key, 1.0), "source_tris": len(ma.loop_triangles), "cut_tris": len(mb.loop_triangles),
           "source_verts": len(ma.vertices), "cut_verts": len(mb.vertices)}
    if key in RATIOS:
        row["uv"] = uv_error(ma, mb)
    rows.append(row)
    t0 += row["source_tris"]
    t1 += row["cut_tris"]
    ea.to_mesh_clear()
    eb.to_mesh_clear()

if MODE == "export":
    OUT.parent.mkdir(parents=True, exist_ok=True)
    if SEAL is not None:
        # the source .mtl is not copied: put a copy of it beside OUT_OBJ (its mtllib line names it)
        sq.write_obj(OUT, "lod sealed cut (tools/lod/blender_cut_parts.py)", SOURCE.stem + ".mtl", P, faces, fuv, fpart, fnrm, alive,
                     [(i, k, objs[i].data.materials[0].name.split(".")[0] if objs[i].data.materials else "default")
                      for i, k in enumerate(order) if k in RATIOS])
    else:
        bpy.ops.object.select_all(action="DESELECT")
        for o in cut.objects:
            o.location.y -= width
            o.select_set(True)
        bpy.ops.wm.obj_export(filepath=str(OUT), forward_axis="NEGATIVE_Z", up_axis="Y", export_selected_objects=True, export_uv=True,
                              export_normals=False, export_materials=True, apply_modifiers=True)
        for o in cut.objects:
            o.location.y += width

result = {"source": SOURCE.name, "mode": MODE, "source_tris": t0, "cut_tris": t1, "parts": rows,
          "written": str(OUT) if MODE == "export" else None, "sealed": seal_report}
print(result)
