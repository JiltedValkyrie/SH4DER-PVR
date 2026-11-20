"""Blender (headless): cut a car OBJ down to a triangle budget, group by group.

    blender -b --factory-startup --python tools/car_lod.py -- IN.obj OUT.obj --body 186 --wheel 16

Every OBJ group stays its own group (the bake picks wheels by group name). A group whose name starts with "wheel"
gets --wheel triangles, any other group --body. Decimate COLLAPSE keeps the palette UVs, so the colours and the
paint / glass classes survive; the ratio is searched until the count is at or just under the budget.
An existing OUT.obj is refused. Prints CAR_LOD_OK and a row a group.
"""
import os
import sys

import bpy


def tris_of(obj, ratio):
    """Triangle count of obj with a Decimate COLLAPSE of this ratio evaluated (the modifier stays, set to it)."""
    obj.modifiers["lod"].ratio = ratio
    dg = bpy.context.evaluated_depsgraph_get()
    dg.update()
    me = obj.evaluated_get(dg).to_mesh()
    n = sum(len(p.vertices) - 2 for p in me.polygons)
    obj.evaluated_get(dg).to_mesh_clear()
    return n


def cut(obj, budget):
    tri = obj.modifiers.new("tri", "TRIANGULATE")
    tri.min_vertices = 4
    obj.modifiers.new("lod", "DECIMATE").decimate_type = "COLLAPSE"
    before = tris_of(obj, 1.0)
    if before <= budget:
        return before, before
    lo, hi = 0.0, 1.0
    for _ in range(24):                                   # the largest ratio that fits the budget
        mid = 0.5 * (lo + hi)
        if tris_of(obj, mid) <= budget:
            lo = mid
        else:
            hi = mid
    return before, tris_of(obj, lo)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:]
    src, out = argv[0], argv[1]
    body = int(argv[argv.index("--body") + 1])
    wheel = int(argv[argv.index("--wheel") + 1])
    if os.path.exists(out):
        raise SystemExit("refuse existing output: %s" % os.path.basename(out))
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.wm.obj_import(filepath=src, use_split_groups=True)
    rows, total = [], 0
    for obj in [o for o in bpy.context.scene.objects if o.type == "MESH"]:
        before, after = cut(obj, wheel if obj.name.startswith("wheel") else body)
        rows.append((obj.name, before, after))
        total += after
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.wm.obj_export(filepath=out, export_selected_objects=True, apply_modifiers=True, export_object_groups=True,
                          export_materials=False, export_triangulated_mesh=True, export_normals=True, export_uv=True)
    for name, before, after in rows:
        print("CAR_LOD_ROW %s %d -> %d" % (name, before, after))
    print("CAR_LOD_OK tris %d" % total)


main()
