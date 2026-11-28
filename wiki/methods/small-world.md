---
title: "Small world [world bound]: draw only what is inside the walls"
tags: [method, psm, ground, camera, prototype]
updated: 2026-11-28
sources: [demo/psm/psm_scene.h (PSM_WORLD_BOUND, psm_bound_rows_, psm_bound_cells_), demo/psm/tools/bake_square.py (--closed), demo/psm/psm_town.h (PSM_TOWN_CLOSED), demo/psm/psm_dc.c (PSM_WORLD_DEFAULT), demo/psm/psm_viewcost.h (VC_BOUND), demo/psm/test/psm_bound_test.c, demo/psm/turncost/build.py (--bound), Makefile (SQUARE=closed), README.md "The shadow demo's small world (the closed square)"]
---
# Small world [world bound]: draw only what is inside the walls

**What you will learn:** why a walled place should not draw a world beyond its walls, how the shadow demo cuts its ground, its shadow map and its mesh down to the square, what that saves, and what is still unproven.
**For:** a programmer whose level is a closed place (a courtyard, an arena, a room, a walled town) and whose ground or terrain reaches far past what the player can see.

> **In plain words.** The shadow demo's square has walls 6 m high, and the camera is 1.7 m off the ground. Nobody standing inside can see ground outside the walls. The old demo still laid out ground to 92 m in every direction, and fitted its shadow map to everything the camera could see out to 70 m. The small world shuts the square's two gaps with gates and stops the ground at the walls. The shadow map then covers only the square, so the same map gives finer shadows. A little fog softens the far walls. Inside the walls the picture is the same.

## Try this first

1. Build the closed square: `make ... SQUARE=closed`, or `python -I demo/psm/turncost/build.py --tag <new tag> --ship --keep --bound --elf-only` for the view cost test. NOT RUN on a console.
2. Turn the cut on: `PSM_WORLD_DEFAULT` = 0 in `demo/psm/psm_dc.c` is the default (off). Give `-DPSM_WORLD_DEFAULT=1u` (the bit `PSM_WORLD_BOUND` = 1) to start with it on. In the view cost test it is stage option `VC_BOUND` = 512. NOT RUN.
3. Keep the fog that comes with it: `PSM_BOUND_DIST_I` = 10, so DRAW DISTANCE is `psm_dist_m[10]` = 75 m, with the fog rising from 32 m (0.43 of it). ESTIMATE: chosen so that the farthest thing, the keep's top, is 64 m from the walk and stays in view.
4. Compare it with the open square on the same disc: `turncost-t5-keep` (open) against `turncost-t6-bound` (closed, cut). NOT RUN.

## What it looks like

- The north gate and the south street are shut by wooden gates 4.5 m high. There is no view down the street.
- A light fog on the far walls and on the keep. Nearby things have none.
- Sharper ground shadows. The map covers about 28 x 31 m instead of the view's ground.
- The ground, its baked occlusion and its colours inside the walls are unchanged.

## What it does

**The closed bake** (`bake_square.py --closed`, `SQUARE=closed`):

- **Gates.** A door is set in each gap's reveal. It is also a shadow caster, so the sun does not shine through a shut gate.
- **Routes.** The walk ends inside the south wall (story z -15.5). The porter comes in at the north gate and leaves at the south gate (`psm_town.h` under `PSM_TOWN_CLOSED`).
- **Faces nobody sees** are left out of the drawn mesh:
  - Tops above the eye on solid pieces (walls, towers, gate towers, buttresses, the colonnade slab): 398 triangles. Their own body hides them from below.
  - Faces of solid pieces that turn away from every eye position in the walk: 358 triangles.
  - Parts outside the walls that sit under every line of sight over them: 46 triangles. The test uses the lowest wall top, 4.5 m.
  - The first two rules apply only to faces whose outward side was given in the bake. The PVR's culling is off in this demo, so the order of a triangle's corners says nothing about which side is seen.
- **Casters.** All of them stay. The keep and the corner towers shade the square from outside it.
- **The header** says what can be seen: `PSM_TOWN_BOUND_X0` .. `PSM_TOWN_BOUND_ZS1`, the walls' outer faces.

**The cut** (`PSM_WORLD_BOUND`, `psm_frame_t.world_opt`; only on a scene with `bound_ok`):

- **Rings.** The ground is rings round the camera. They now end where the window's farthest ray leaves the bound, instead of at `PSM_ROW_END` (92 m). The farthest point is at one of the window's two edge rays, or at a corner of the bound between them.
- **Cells.** A cell that cannot touch the bound is never placed or sent. A ground point that no kept cell uses is not placed, gets no route A map coordinates, and is not compared against the route B map.
- **Near the camera** the 0.25 m rings are kept as they were. It is the same lattice, so a route B shadow edge does not crawl.
- **Shadow map.** The uniform map is fitted to the bound's eight corners over the route's slab (`PSM_TOWN_SLAB_A` = 0.5 m, `PSM_TOWN_SLAB_B` = 13 m), not to the view out to `PSM_FAR` (70 m). It does not move while the camera turns. The perspective (warp) map keeps the view's fit, because its points must be in the view.

Version: PROTOTYPE, host tested (`demo/psm/test/psm_bound_test.c`, in `make test`).

## Cost

Host counts (DERIVED: `psm_bound_test`, 2,016 camera poses over the walk, 3 pitches and 12 headings, routes A and B). No console times.

| Item | Open square | Closed, cut | Status |
|---|---|---|---|
| Drawn mesh, static triangles / vertices | 6,097 / 8,011 | 5,301 / 6,800 | DERIVED (bake counts) |
| With the 12 movers: model vertices / triangles | 9,051 / 6,617 | 7,840 / 5,821 | DERIVED |
| Caster triangles | 1,686 | 1,764 (the two gates) | DERIVED |
| Ground rings a frame | 97 | 46.1 mean, 67 at most | DERIVED |
| Ground points placed a frame | 5,400 mean | 1,943 mean, 3,608 at most | DERIVED |
| Ground cells sent a frame (2 triangles each) | 3,948 mean | 1,185 mean, 2,610 at most | DERIVED |
| Shadow texel on the ground ahead | 1 | 0.46 mean, 0.57 at most, never larger | DERIVED |
| Fog | none | the PVR's fog table: no cost a vertex | FACT (the PVR's table fog) |
| Frame time | | | NOT RUN |

The test also checks that every ground cell the full window sends that can touch the square is sent by the cut one, with the same screen positions, and that no new cell appears.

## Recommended limits

- Cut a closed place's ground at its walls. Status ADVISED: no console run yet. To prove it, run `turncost-t5-keep` against `turncost-t6-bound` and compare each stage's frame time and placement time.
- Fit the shadow map to the ground that can be seen, not to the view's far plane. Status ADVISED, with the same test.

## Pitfalls

- **Only for a closed place.** Any gap in the walls shows ground outside the bound, which is no longer drawn. Without `bound_ok`, the scene ignores `PSM_WORLD_BOUND`.
- **Fog distance.** Keep DRAW DISTANCE past the farthest thing anyone can see. A shorter one cuts the keep off.
- **The warp map** keeps the view's fit. The cut still shortens its ground.
- **A changed walk.** Change `WALK_CLOSED` in the bake and `PSM_TOWN_ZS0` in `psm_town.h` together. The bake's faces-nobody-sees test uses the walk.

## Two more switches for the same frame (step 4)

These are not part of the cut. They are in the same builds because they work on the same placement code.

- **Still camera, seen marks kept** (`PSM_PLACE_VIS` = 16, a `place_opt` bit). When the camera has not moved, the list of model pieces the view sees cannot change. So the placement keeps that list and tests only the moving pieces. It does this only when the piece count and the moving lists are the same as at the last full test. Host check: `demo/psm/test/psm_keep_test.c`, 0 of 240 steps differ from the full test, and the list was kept on 120. View cost stage option `VC_VIS` = 1024 (`build.py --vis`). NOT RUN on a console.
- **Cold code** (`PSM_COLD` = 0, a build flag; `build.py --cold`). The code that runs only at load or when the text changes is marked cold. The compiler then puts it away from the frame's code. The SH-4's instruction cache is 8 KB and direct mapped, so a big function sitting between two hot ones pushes them onto the same cache lines. This changes only where the code sits, not what it does. NOT RUN.
- The A/B: `turncost-t6-bound` against `turncost-t7-gv` (the cut, the ground option 128 and `--vis`) and `turncost-t7-gvc` (the same and `--cold`). NOT RUN.

## Open

- The frame and placement times on a console (A/B above): NOT RUN.
- Whether the far walls' fog looks right on a television. The owner's eye decides.

## Related

- [Off-screen skip [viewcull]](../effects/viewcull.md), [Sun shadow map [psm]](../effects/psm.md), [Tools and safety guide](../start-here/genre-tools-and-safety.md), [What do you want to make?](../start-here/what-do-you-want.md)
