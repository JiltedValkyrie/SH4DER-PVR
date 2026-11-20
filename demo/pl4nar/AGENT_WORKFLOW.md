# Agent workflow: reflections and night lights in a port

For an agent adding `sh4der_pl4nar` (planar reflection), `sh4der_pool` (light pools, blob shadows) and lamp halos (`sh4der_spark_quad`) to an engine it did not write: typically a port of an older console game, where meshes carry positions, texture coordinates and baked colours and nothing else. Each step ends in something you can look at or count. Do not start a step before the one above it passes. The reference client is `src/demo.c` in this folder; the calls are in `../../AGENTS.md` ("Call order: pl4nar", "Call order: pool").

## 0. Before any code: find the inputs, write them down

| Input | Where to find it | Check |
| --- | --- | --- |
| The mirror plane | the road, floor or water level: unit normal toward the camera's side, and d with dot(n, x) = d, in world units after any fixed-point conversion | a point you know is on the ground gives dot(n, x) - d = 0 |
| The main camera | where the engine builds its view: eye, target, up, vertical field of view in radians, near depth. Take them there; do not rebuild them from a matrix | the engine's own picture, drawn through `sh4der_pl4nar_viewproj_rows` with plane NULL, is the same picture |
| Reflective vertices | world positions as `float[3]` at a stride that is a multiple of 4, 4-byte aligned. Convert fixed point once at load | the count; `(stride & 3) == 0` |
| Normals | none in such engines: bake them for the reflective meshes alone (`../../tools/pl4nar_normals.py`, or its rule in the loader), rotate them with the object | every normal has length 1; a flat roof's normal points up |
| Which triangles reflect | by material, palette entry or texture page: paint, glass, chrome | every triangle has ONE class (make the tool refuse a mixed one) |
| A cheap world draw | the engine's own draw at its lowest detail, a shorter far distance, no small props, no HUD, culling reversed | its triangle count, printed |
| Lamps | positions of street lamps and of the car's lamps, a day / night value 0 .. 1 | the count within your light distance |

Write the triangle and vertex counts down now. They are your budget and your report.

## 1. The mirror target, seen

`sh4der_pl4nar_target_init`, `sh4der_pl4nar_view_build`, `sh4der_pl4nar_pass` with your cheap world draw. Show the target as a quad in a corner of the screen.

- PASS: the world upside down, the horizon near the top, the sky at the bottom. It follows the camera.
- Upright world: the plane was not given (NULL), or its normal points away from the camera.
- Empty target: the cull is the wrong way round (a mirrored world has reversed winding), or the draw closed no list.
- A 0 from the init: no video memory. A 0 from the build: the view has no direction. In both cases draw no layer; never use a view that was refused.

## 2. The layer, pinned

One mesh. `sh4der_pl4nar_shade_n`, then the same triangles again on the translucent list with `sh4der_pl4nar_layer_header`, vertex alpha forced to 255, blend 0.

- PASS: the picture sits still on the surface while the camera turns, and slides over it as the object moves.
- It swims with the camera: the positions are not world positions, or the normals are not rotated with the object.
- Stray triangles to nowhere: a vertex of the layer was never written this frame (step 0, "one class"), or a work array is smaller than the mesh. Size arrays from the mesh; refuse a mesh that is larger.
- Wrong picture after the lookups: they leave the reflection view in XMTRX. Load your own matrix again before the next transform.

## 3. Fresnel and materials

`sh4der_pl4nar_argb` for the vertex colour; a row a material from `sh4der_pl4nar_preset`, tuned in your own table: `f0` first, then `gain`, then `ground_keep`.

- PASS: faces seen edge on reflect strongly, faces seen head on barely; paint keeps its colour (additive), glass is replaced by its reflection (blended).
- Physical F0 looks weak on flat-shaded art: 0.14 for paint and 0.24 for glass read right in this demo.

## 4. Flat surfaces, if you have them

`sh4der_pl4nar_flat_n` for water, a wet road, a floor; `sh4der_pl4nar_tri_split` on each triangle's three depths, cut and ask again (stop at a depth of 4).

- PASS: the reflection does not swim inside a large polygon when the camera pitches.
- This path has a host test and no picture yet: judge it on screen and say so in your report.

## 5. Night: pools, halos, lamps in the mirror

In the translucent list after the ground: `sh4der_pool_disc` under each lamp within your light distance, `sh4der_pool_wedge` ahead of each headlight (two wedges read softer: a narrow long beam, a wide short spill), a dark disc under each car through a blended header. A halo is `sh4der_spark_quad` at the lamp with a small glow texture, additive. For the lamps to pass over the car, draw the same halos in the mirror pass, on the target's translucent list.

- PASS: by day nothing is drawn (ramp 0); at night the pools lie on the road without flicker (lift them 2 to 4 cm), and a lamp's glow crosses the glass and the paint as the car passes under it.
- Dim on paint: raise the layer's `gain` at night (2.0 here) before you raise `f0`.
- Every lost ray lights up at once under a lamp: lost rays fade to one texel of the target (the sky overhead). It is a limit of the method, not a bug in your code.

## 6. Count and report

Measure with the engine's own timers, at 128 x 64, 256 x 128 and 512 x 256 targets, effect off and on:

| Number | How |
| --- | --- |
| Target bytes | exact: pitch x h x 2 (`target.bytes`) |
| Lookup ms, vertex count | a timer round `sh4der_pl4nar_shade_n` alone |
| Mirror pass CPU ms, triangle count | list begin to list finish of your draw alone |
| PVR render time, and your two waits for the PVR | `pvr_get_stats`; the waits timed apart. A timer round the whole pass reads the PVR's queue, not the pass |
| Frames a second off / on, video memory free after load | as the engine reports them |

Mark each number "measured on a console", "measured on an emulator" or "estimate". Never mix them in one row. An emulator's render time is not a console's.

## Rules that hold at every step

- All maths through SH4ZAM (directly, or through the `sh4der_*_` helpers of `sh4der_core.h`). A hand-written product gets a comment saying no SH4ZAM call exists for it.
- Build the view with `sh4der_pl4nar_view_build` (four rows, one load). Do not build it with a chain of matrix calls.
- Every 4-byte load on an address that is a multiple of 4. A console stops where an emulator carries on.
- Every polygon header compiled before its first vertex (`sh4der_poly_hdr_ok`); no triangle without screen area (`sh4der_tri_area_ok`).
- Nothing that uses the store queues between a list's begin and its finish if you submit by direct rendering.
- The target is allocated once, not every frame. Free it only after the PVR is done with it (`sh4der_pl4nar_target_free` waits).
- Checks before you say done: `make test`, `make test-shz SH4ZAM=...`, `python tools/pl4nar_normals.py --self-test`, `python demo/pl4nar/build.py --dry-run`, then a boot. Say plainly which of them you did not run.
