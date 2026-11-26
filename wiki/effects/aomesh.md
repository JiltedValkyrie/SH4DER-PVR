---
title: "aomesh (baked occlusion over a triangle mesh)"
tags: [effect, ao, bake, mesh, load]
updated: 2026-11-26
sources: [include/sh4der_aomesh.h, test/aomesh_test.c, demo/psm/psm_ao.h, demo/psm/test/psm_ao_test.c, README.md "The aomesh effect", include/sh4der_aobake.h]
---
# aomesh: baked occlusion over a triangle mesh

**What you will learn:** how one ambient occlusion byte a vertex is baked at load by casting 12 rays through a cell grid of a triangle mesh, how the optional ground grid works, what the bake costs (counted on the host; the console is NOT MEASURED), and how it differs from the other occlusion tools.
**For:** a programmer with static scenery built as triangles (a walled street) who wants a contact shadow in the vertex colour, paid once at load.

> **In plain words.** This is the corner-shading bake for scenery made of triangles, such as a walled street. For each vertex it casts 12 rays over the half-sphere above it and writes one brightness byte, so corners and the foot of a wall come out darker. A byte grid over the ground does the same for a floor that moves with the camera. It runs once at load, and moving things are left out so they keep their full light.

## Try this first

What it is for: contact shading on static triangle scenery, baked once at load, plus a ground grid.

1. Fill a `sh4der_aomesh_tris_t` (positions, index list, triangle count), call `sh4der_aomesh_need`, and allocate the buffers it names (`include/sh4der_aomesh.h:76`, `:146`). No value to set.
2. `SH4DER_AOMESH_RAYS` = 12 and `SH4DER_AOMESH_REACH` = 1.6 m (`include/sh4der_aomesh.h:62-63`; "Start with" at `:12`; `README.md:747-748`, ADVISED).
3. `SH4DER_AOMESH_CELL` = 2.0 m (`include/sh4der_aomesh.h:65`; `README.md:750`), kept at least the reach (the page's Recommended limits), and `SH4DER_AOMESH_PUSH` = 0.03 m (`include/sh4der_aomesh.h:64`; `README.md:749`), ADVISED.
4. Ground grid: `SH4DER_AOMESH_GCELL` = 0.5 m and `SH4DER_AOMESH_MARGIN` = 8.0 m (`include/sh4der_aomesh.h:66-67`; `README.md:751-752`, ADVISED). Pass NULL for the rectangle when you want the vertex bytes alone (`README.md:741`).
5. Call `sh4der_aomesh_bake` at load with a tick every 256 vertices and every 8 ground rows (`README.md:741`, ADVISED), then read the ground each frame with `sh4der_aomesh_ground` (`include/sh4der_aomesh.h:356`).

On a console: NOT RUN. The page's status: "Load-time bake on a console | NOT MEASURED".

Header: [include/sh4der_aomesh.h](../../include/sh4der_aomesh.h). Call order: [below](#call-order).

## What it does

For each vertex, `sh4der_aomesh_bake` casts 12 cosine-weighted rays over the vertex's hemisphere (a golden-ratio spiral, the same set every bake). Each ray is tested against the triangles of the 2 m cells its box touches; the nearest hit within the 1.6 m reach closes the ray by `1 - t / reach`. The byte is `255 * (1 - closed / rays)`: 255 open, 0 closed. The ray starts 0.03 m off the surface. A vertex with a zero normal is open (255) and casts no rays. The plane y = 0 occludes the vertices (the floor the mesh stands on); it never occludes the ground grid.

The optional ground grid is one byte a 0.5 m cell over a rectangle and an 8 m margin, normal up, for a ground that moves with the camera (it has no vertices of its own). `sh4der_aomesh_ground` reads a cell and returns 255 outside the grid.

It multiplies the ambient light only. Nothing is drawn by the header and nothing runs a frame.

Header only, all `static inline`. No malloc, no globals: the caller owns the mesh, the normals, the scratch, the bytes and the grid.

## Call order

1. At load: fill a `sh4der_aomesh_tris_t`: the positions (stride in floats, at least 3), the index list (`uint16_t [3]` a triangle), the triangle count.
2. `sh4der_aomesh_need(&mesh, ground_or_NULL, &need)`: the entries each buffer needs (box: float[6] a triangle; cell: uint32 `cw * ch + 1`; list: uint16 a reference; grid: bytes). Allocate them.
3. `sh4der_aomesh_bake(&mesh, normals, normal stride, vertex count, ground, &need, &work, vert_out, &grid, &stats, tick)`. It returns 1 when it baked. It returns 0 and writes nothing when a buffer is too small, the mesh is empty or unusable (more than 65,536 triangles, a stride that is not a multiple of 4, a cell grid over 65,536 cells), or a pointer is NULL. The scratch (box, cell, list) is the caller's to free after; the grid's bytes are kept.
4. Multiply the ambient light by `vert_out[i]`, and by `sh4der_aomesh_ground(&grid, x, z)` for the ground.

The town and square demo does this through `demo/psm/psm_ao.h`, an adapter that keeps the buffers, maps the scene's arrays, and hands the bytes to `psm_colour`.

## Cost

| Item | Cost | Status |
|---|---|---|
| Rays, the walled square's static mesh (8,697 vertices, 5,320 triangles, 70 x 103 ground grid) | 190,884 rays, 1,594,130 ray-triangle tests (8.4 a ray), 134,879 reciprocals (the edge tests are on the numerators: a reciprocal only for a triangle that passed them) | exact, counted on the host (`demo/psm/test/psm_ao_test.c`) |
| Host time of that bake | 66 to 78 ms on the development PC | MEASURED on the host only; not a console figure |
| Load-time bake on a console | NOT MEASURED | the AO row in the walled square shows it when the program runs |
| Per ray and per vertex on the SH-4 | ESTIMATE: 12 rays times the triangles a ray's cells hold | not measured |
| Run time | one table read a vertex (and a ground cell when the ground moved) | by construction |
| RAM | one byte a vertex, one byte a ground cell; scratch during the bake: 24 B a triangle, 4 B a cell, 2 B a reference | exact, by the code |
| VRAM | 0 | |

The bake's bytes are pinned for the town's kit mesh (8,697 vertices, 5,320 triangles) on the plain C path: the FNV-1a checksum in `demo/psm/test/psm_ao_test.c`, which prints "pin NOT RUN" on any other mesh. SH4ZAM software path against plain C on the host: 6 of 15,907 bytes differ, the largest by 21 (MEASURED on the host, 2026-11-26; the console's FSRRA path NOT RUN, so the console's bytes are neither). Cause OPEN. Before the move and before the edge change it was 6 and 20; the edge tests on the numerators (2026-11-26) closed two of those bytes and two others flipped, so the cause is upstream of the edge tests: the two paths' ray directions differ by an ulp (vertex 3808, ray 0: 1 ulp in z, det 12 ulp apart). The clearest case is vertex 6360: the edge change moved it on the SH4ZAM path only (6 to 27) and left it at 6 on plain C, so the same test on inputs an ulp apart decides one ray two ways. What it means for a user (ADVISED, host only): the bytes are not the same across maths paths and will not be on the console either; expect a few vertices a whole ray apart (about 21 of 255), which is not visible. The edge change moved 3 plain C bytes (the largest by 21), and the pin was taken again on purpose.

## Recommended limits

- 12 rays a vertex (`SH4DER_AOMESH_RAYS`). The bake is linear in the ray count.
- Bake at load only. A moving occluder needs a new bake; pass the static part only (the walled square's people and hens stay open, 255).
- Keep the cell at least the reach (`SH4DER_AOMESH_CELL` >= `SH4DER_AOMESH_REACH`): the header checks it at compile time where the compiler allows, and the bake refuses otherwise.
- Read the byte as a multiply on the ambient light, not as physical light.

## Tuning

See `README.md` "Aomesh tuning": `SH4DER_AOMESH_RAYS`, `_REACH`, `_PUSH`, `_CELL`, `_GCELL`, `_MARGIN`, `_GMAX`.

## Which occlusion tool

| Tool | What it is | Where it runs |
|---|---|---|
| [ao](ao.md) (`sh4der_ao.h`) | crease darkening from the angle between faces, and a screen cavity cue | console (no rays, no mesh) |
| [aobake](aobake.md) (`sh4der_aobake.h`) | rays at exact spheres, boxes and a plane (64 Hammersley rays), once at load | console |
| aomesh (this page) | rays at a triangle mesh through a cell grid, 12 rays, and a ground grid | console |
| [ao-bake](ao-bake.md) (`tools/ao_bake_core.h`) | the same kind of bake over any triangle list, on the host, with an inside fill | host build tool only |

## Pitfalls

- The plane y = 0 occludes the vertices but not the ground grid. A ground that is not at y = 0 needs its own geometry in the mesh.
- A zero normal is open, not an error: check that the normals were made.
- A NULL rectangle, or one with x1 <= x0 or z1 <= z0, or one too big for `SH4DER_AOMESH_GMAX`, gives no grid: `sh4der_aomesh_ground` then returns 255 everywhere.
- The indices and positions are not checked. An index past the vertex count reads past the array.

## Open

- The bake's time and its look on a console are not measured. PROTOTYPE v0.1 until a console run times it.
- The mesh-build part (turning a model into the cell grid and the normals for the caller) is parked until a second client exists; the bake takes the mesh as it is.
- The ray set is the golden-ratio spiral of `demo/psm/psm_ao.h` (kept for its bytes); it is not the Hammersley set of `sh4der_aobake.h`, and the two are not merged.

## Related

- [aobake](aobake.md), [ao](ao.md), [ao-bake](ao-bake.md), [limits: recommended](../limits/recommended-limits.md), [frame budget](../limits/frame-budget.md)
