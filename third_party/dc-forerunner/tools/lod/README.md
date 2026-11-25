# LOD tools

Reduced-detail (LOD) tools from the Halo Dreamcast port (`dc-halo`), made game-agnostic. Each file says where it came from.
The inputs are OBJ files and plain Python lists. Every place that needs your source engine's mesh format is marked `TODO(SOURCE)`: write that reader for your game first.

All files compile (`python -m py_compile`). The pure-Python and numpy files run their self tests (see each file's docstring). The Blender scripts (`blender_*.py`) are syntax-checked only: they need Blender's `bpy` and `mathutils`, and have not been run in Blender in this repository.

## Files

| File | What it does | Input | Output | Origin (Halo DC port) |
|---|---|---|---|---|
| `qem_decimate.py` | Quadric-error edge-collapse decimation. Keeps UV seams, locks open edges and part borders (near cuts), refuses folds, optional max deviation. | `vertices` as `(group, (x,y,z), (u,v), ...)`, `triangles` as index triples, `target` | surviving triangle index triples | the halo-64 N64 decimator, algorithm and code CC0 (halo-64, per the Halo DC port's lighting provenance note). Keep that credit. |
| `blender_sealed_qem.py` | Sealed cut: all parts welded into one mesh, so a cut opens no crack at a part border; UV kept on its chart; holes check. Functions: `build`, `decimate`, `sealed_cut`, `snap_corners`, `write_obj`, `write_back`, `holes`. | Blender objects (or any welded mesh via `build`) and a spec dict | cut faces, corner UVs and normals, OBJ | Halo DC port's sealed cut (2026-10). MIT (the Halo port's LICENSE). |
| `blender_cut_parts.py` | Hand cut of named parts to a ratio, in Blender: preview beside the source, UV error per part, export. Sealed cut or Blender's Decimate modifier. | `SOURCE_OBJ`, `RATIOS` (part to ratio), optional `SEAL`, `MODE` | cut OBJ (export) and a per-part report (triangles, vertices, UV error) | Generic form of the Halo DC port's prop cut. Halo paths, model table and export folders removed. |
| `impostor_views.py` | Impostor math: the 58-view ring (side 16, rings of 12 and 8, poles), z-buffered orthographic cell render with 1-bit alpha, yaw-averaged sun shading, colour bleed, premultiplied box shrink. Pure numpy. | a triangle soup and a texel callback | RGBA cells | Halo DC port's impostor bake and far-prop shrink. Model loader, texture decode and Halo blob writer removed. |
| `cross_quads.py` | Far cross-quad card: card height and width from the tree's drawn vertices, two crossed quads (4 triangles), 128 x 128 atlas (front left, side right). | drawn vertex positions, and two rendered views | 8 vertices, 4 triangles, UVs, the atlas RGBA | Halo DC port's far cross-quad cards (2026-10, 2026-10-30 VRAM). The Halo blob writer is not ported. |
| `lod_census.py` | Census of every model x map x tier (`<root>/<map>/<model>/<tier>.obj`), with the rules flagged: missing far, far over 45 %, far not lighter than near, a non-near tier over the 600-triangle bar. Writes stdout and optional CSV. | folder tree of OBJ files, optional placements CSV (`map,model,count`) | table and flags; exit 1 with `--strict` | Written for this starter from the Halo DC port's census rules (issue #376). |

## Halo-only, not copied, and why

| Halo file | Why not copied | The idea that carries over |
|---|---|---|
| Halo BSP decimation | Halo BSP chunks: the metal-chunk name pattern and the chunk rules | the sealed cut family (`blender_sealed_qem.py`) |
| Halo character cut | Halo's LOD slots (super low, low, medium, high) per geometry and permutation | the cut with a vertex group (`vgroup`): a vertex collapses only inside its own bone, so the skin holds |
| Halo vehicle cut | Halo vehicle slot table | one hand cut per vehicle, signed off in renders; no machine decimation of vehicles |
| Halo super far characters | Halo's `models_x.pak` row format and skin sidecar | the super far body: one hand-made body of at most 110 triangles, skinned to the model's own bones, for characters under 24 px |
| Halo lily pads, shadow proxy | Halo bitmap pak; Halo bone names | a flat object as one quad over its own top-down texture; a 78-vertex shadow proxy |
| Halo runtime distance rule and card placement | the Halo engine (`level.c`, `impostor.c`) | the rule: a character drops to its far mesh past one set distance, a card is pulled toward the eye |
| Halo cinematic cards | Halo cinematic clips and actors | a bounded animated card per cinematic pose |
| Halo tag-to-model converter | Halo tag format | the tier writer (near, mid, far, xfar, sxfar) is the layout `lod_census.py` reads |

## Rules Halo learned (the Dreamcast port, 2026-10 to 2026-11)

These are the Halo port's own rules. Your game's numbers (its own screen budget and frame rate) are not measured yet: check each rule against your build before using a number here.

1. **Screen budget.** The Dreamcast needed about 6,000 polygons on screen for 30 fps (the Halo port's owner, 2026-11-10). Judge each model against that: one 1,000-triangle character is a sixth of the scene. Never call a model of about 1,000 triangles "light", near or far.
2. **The bar for a character.** About 600 triangles per character tier (the Halo port's bar). `lod_census.py --bar` sets it.
3. **Every model needs a real far cut.** Far is at most 45 % of the near, in triangles and in vertices, where the cut can reach it. Far equal to near is a gap, not a cut. Far heavier than near is a bug (Halo's own far slot was heavier than its near on two models).
4. **Census every character on every map and every tier.** Near tiers, far, the far-far card or cut, and the super far body. The Halo port found far cuts existed only on two maps, and the rule became "no per-map gaps". Run `lod_census.py --placements` on the placed pairs.
5. **Fade only on appearance.** Enemies may fade in when they first appear (campaign only). No mesh-swap fades between tiers.
6. **Hand cuts, checked by eye.** Each cut is compared with the source in a side-by-side render before it is written. The UV error is a hint only; the textured copy beside the original decides. The Halo port banned machine decimation of vehicles (2026-10-21); props and first-person models kept their machine cut (2026-10-26).
7. **No cracks.** Weld by position within a group before a cut, lock open edges and part borders on near cuts, and keep UV seams. Far copies may unlock the borders when they are small on screen.
8. **Cards are a look and a cost decision.** A cross-quad or impostor card is a departure from the engine's mesh, taken for speed or VRAM. Use it only for far trees, plants and rocks, and record it as a departure with its reason.
9. **Skin holds.** A vertex collapses only inside its own bone group, so joints bend as before.
10. **Record each cut.** The cut's triangles and vertices before and after, the far share, and the look check go in the project tracker row for that model, with the commit.

## Halo issues and commits behind these rules

- #265: super far characters (`sxfar_chars.py`): `c1733c1e`, `731de0c7` (runtime), `6e40d011` (Flood forms).
- #376 (F-LOD-ALL-TIERS): the census and the cuts per model: `8f6a301c` (census closed), `16a7efd6` (far cuts on the maps that had none), `ae9cbc67`, `0b4353a0`, `2f0d405e`, `a2ef8674`, `cc2afe42`, `40b67a5c`, `455cb8fb`, `dbadb3d2`, `4154535b`.
- #332 (LOD seen and SXFAR log): `183bd26d` (how to read a LOD boot log), `d105a38c` (side-pak CRC guard).
- #139: storage far cut (F-CSTORAGE-FAR), Parity PASS.
- #304 and #287: far cuts on one more map (F-FLOOD-FAR-304).

## TODO(SOURCE)

1. **Mesh reader.** Write your source engine's static and skeletal mesh export to OBJ: part names, scale, up axis, winding, normals and the skin (bone name and weight per vertex). `blender_cut_parts.py` assumes forward -Z, up +Y.
2. **Engine winding rule.** `blender_sealed_qem.py` (`game_soup`, `holes`) and `cross_quads.py` need your engine's front-face rule.
3. **Game vertex count.** `lod_census.py` counts positions. Replace with your engine's vertex rule (position, UV and normal per corner) and add the column.
4. **Skin for `vgroup`.** The cut's bone rule needs your skin weights (`blender_sealed_qem.decimate`, `vgroup`).
5. **Card format.** The card blob, its placement and its fade are your engine's; `cross_quads.py` gives the geometry and the atlas only.
6. **Blender run.** Run `blender_cut_parts.py` on one model in Blender before trusting it; record the output in the project's tracker.
7. **Numbers.** Replace the Halo numbers above (6,000, 600, 45 %) with your own after a measurement on the Dreamcast.
