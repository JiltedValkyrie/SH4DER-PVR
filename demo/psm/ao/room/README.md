# Living room for the AO scene

A small clay living room for the ambient occlusion tests: a floor, two walls with panel trim, a sofa, a coffee table with vases, two ottomans, a tripod lamp, two plants, a rug and a framed panel.

**Licence.** Own work, modelled from numbers by `tools/build_room.py`. No third-party asset, texture or scan is used. Copyright (c) 2026 Jilted Valkyrie; it is covered by this repository's `LICENSE`.

**Status.** Host work only. Nothing here has run on a console. The triangle counts are exact. No cost on the Dreamcast is claimed.

## The look

The pictures and `dc_room.blend` are not in the repository: `tools/build_room.py` makes them again (see "Making it again"). One reference picture, the ambient occlusion alone, is kept in `wiki/images/`.

| File | What |
|---|---|
| `work/08_clay_ao.png` | The clay render: one grey diffuse material, a white sky and a soft sun (Cycles, 160 samples). |
| `work/09_ao_only.png` | The ambient occlusion alone (Cycles AO node, 1 m distance). This is the target for the AO work. |
| `work/01_blockout.png` to `work/07_plants.png` | The steps: blockout, shell, sofa, table and vases, ottomans, lamp, plants. |

## Triangles

Counted after triangulating, by `tools/check_export.py` from the exported files.

| Object | Triangles | Vertices | Closed | Caster | Movable | Stands on |
|---|---|---|---|---|---|---|
| `floor` | 2 | 4 | one quad | no | no | — |
| `wall_left` | 2 | 4 | one quad | no | no | floor |
| `wall_right` | 2 | 4 | one quad | no | no | floor |
| `room_trim` | 232 | 128 | yes | no | no | walls |
| `floor_border` (optional) | 32 | 16 | yes | no | no | floor |
| `rug` | 12 | 8 | yes | no | no | floor |
| `wall_frame` | 44 | 24 | yes | yes | no | wall_left |
| `sofa` | 280 | 172 | yes | yes | yes | floor |
| `coffee_table` | 72 | 48 | yes | yes | yes | floor |
| `vases` | 196 | 106 | yes | yes | yes | coffee_table |
| `ottoman_a` | 76 | 48 | yes | yes | yes | floor |
| `ottoman_b` | 76 | 48 | yes | yes | yes | floor |
| `lamp` | 112 | 68 | yes | yes | yes | floor |
| `plant_a` | 80 | 80 | **no** (leaf cards) | yes | yes | floor |
| `plant_b` | 64 | 64 | **no** (leaf cards) | yes | yes | floor |
| **Total** | **1,282** | | | **1,000 in casters** | | |

- The budget was 3,000 triangles in all. The floor and walls are not tessellated here; the importer does that.
- **Not watertight:** `plant_a` and `plant_b`. Their pots (and `plant_a`'s trunk) are closed, but each leaf is a one-sided card of two triangles (24 and 20 triangles of leaves). Draw both sides of these two objects.
- `floor`, `wall_left` and `wall_right` are single quads facing into the room. The manifest marks them open, with `surface` set.
- Every other object is made of closed shells. An object can hold several shells that pass through each other (a sofa's cushions, a table's legs).

## The export (`export/`)

- One OBJ per object, one `o` group each, plus `room_all.obj` with every object as its own group.
- Triangles only, with normals (`vn`), no UVs. Flat normals on boxes; smooth on the turned shapes (vases, pots, lamp shade and hub).
- **Space:** metres, Y up, right-handed, the floor's top at y = 0, the room from -3 to 3 in x and z. Blender's (x, y, z) is (x, z, -y) in the files.
- **Winding:** counter-clockwise seen from outside.
- **Positions are in room space.** Each object's base point is `origin` in the manifest, with `turn_y_degrees`, so a mover can be put back about its own base.
- **No shared planes between objects.** A thing that touches a neighbour sinks 1 cm into it: furniture goes to y = -0.01, trim goes 1 cm into the wall, the vases 1 cm into the table top.
- `room_manifest.json` gives, per object: `name`, `file`, `tris`, `verts`, `surface` (`floor`, `wall` or null), `open`, `caster`, `tint` (0 shell, 1 upholstery and rug, 2 wood and metal, 3 plants and ceramics), `movable`, `ground_contact`, `rests_on`, `optional`, `origin`, `turn_y_degrees`, `bbox_min`, `bbox_max`, `volume_m3`. It also gives the camera of the renders (`eye`, `target`, orthographic, 9.2 m across).
- `floor_border` is the tile border as a thin raised band (6 mm). It is marked optional: it lies over the floor's own surface, so drop it if the floor is lit from a map.

**What the importer does with it** (`../room_import.py`; these files are not changed by it):

- It leaves out `rug` and `floor_border`: both are flat and cast nothing.
- It lights `room_trim` with its wall's normal. The trim's thin faces, a pixel wide on the console, showed as dashed lines with their own normals.

## Making it again

From a clone, with Blender 5.2 LTS open and its MCP add-on listening on 127.0.0.1:9876, this one command makes `dc_room.blend`, the pictures in `work/` and the files in `export/`:

```
python -I demo/psm/ao/room/tools/blender_run.py demo/psm/ao/room/tools/build_room.py --pre "STEP=8"
python -I demo/psm/ao/room/tools/check_export.py
```

- `STEP=1` to `STEP=7` build the room up to that step and render it to `work/`. `STEP=8` renders the two final pictures, writes `export/` and saves `dc_room.blend`.
- The script works in its own scene, `dc_room`, and removes only that scene's objects before it builds.
- `check_export.py` reads the files back with no Blender. It checks the counts, the bounding boxes, that closed objects are closed and wound outward, and that every normal is on its triangle's side. It must print `CHECK_OK`.

## Files

| File | What |
|---|---|
| `dc_room.blend` | The scene as saved by step 8 (118 KB). Made by the script; Git ignores it. |
| `tools/build_room.py` | The whole model, the renders and the export. |
| `tools/blender_run.py` | Sends a Python file to the open Blender. Protocol only; nothing of the add-on is copied. |
| `tools/check_export.py` | The host check of the export. |
| `export/` | The OBJ files and the manifest. |
| `work/` | The step renders. Made by the script; Git ignores them. |
