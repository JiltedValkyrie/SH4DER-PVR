---
title: Ray uses beyond shadows
tags: [research, sh4, rays, sound, aica, gameplay]
updated: 2026-11-26
sources: [wiki/hardware/ledger-sh4.md, wiki/hardware/ledger-aica.md, wiki/research/depth-shading-ideas.md, wiki/research/sound-chip-precompute.md, demo/psm/bench/ray_kernels.h]
---
# Ray uses beyond shadows

**What you will learn:** what else the town's grid rays can do besides sun shadows: sound, AI sight, culling, picking, projectiles, ground probes, wind and occlusion. For each use: how many rays a frame it needs, what that costs by our measured ray and ray-triangle rows, what the sound chip side needs, and the test that would prove it.
**For:** anyone planning the game side of a scene that already has the height grid, and the owner choosing the next ray spike. Research, not a recipe.

**Status: ideas only.** Nothing on this page was built or run as a use. Every cost is **DERIVED** (arithmetic on MEASURED rows) and every use is **ESTIMATE** or **OPEN** until it has its own console row.

## The measured pieces

| Row | What it measured (one console, one run, 2026-11-25) | Used here as |
|---|---|---|
| [SH4-052](../hardware/ledger-sh4.md#ray-bench-run-one-thesis-p1-p2-p9) | A ray through the 64 × 64 byte grid, in the cache: about 22 cycles a cell (C_step 22.07 to 22.21) and 71 to 74 to set up a ray. Whole rays: n 1 = 94, n 8 = 248, n 24 = 602, n 48 = 1,133, n 64 = 1,486 cycles | **ray(n) ≈ 74 + 22.2 n cycles**; the whole-ray figures where n matches |
| [SH4-053](../hardware/ledger-sh4.md#ray-bench-run-one-thesis-p1-p2-p9) | A slice of 1,024 receivers batched: 540 to 563 cycles a ray; a cold grid adds 46 to 47 us a slice; mixing the rays into the vertex transform adds 89 to 117 cycles a ray | The cost of a sun ray from a real receiver; the cold-start and mixing penalties |
| [SH4-055](../hardware/ledger-sh4.md#ray-bench-run-one-thesis-p1-p2-p9) | Receiver against character proxy: 137.87 cycles a pair with early outs | Tests against movers, which the grid does not hold |
| [SH4-059](../hardware/ledger-sh4.md#ray-bench-repeat-and-clock-repeat-of-run-one-the-clock-the-ray-triangle-test) | Ray bench, repeat and clock [ray2]'s repeat of the grid walk: eight more fits, C_step 22.09 to 22.20, C_setup 70.99 to 76.04 | The same line holds over two runs |
| [SH4-066](../hardware/ledger-sh4.md#ray-bench-repeat-and-clock-repeat-of-run-one-the-clock-the-ray-triangle-test) | A ray against a triangle, no divide, 96 triangles in the cache: 33.16 cycles a test prepared for one direction, 62.49 any direction, with early outs | Group D |

The grid ([`ray_kernels.h:8-13`](../../demo/psm/bench/ray_kernels.h)): 64 × 64 cells of 1.5 m (96 m a side), one byte of caster height a cell in steps of 28 m / 255 (`TOWN_RAY_H`, the tower's roof, [`town_ray_data.h:19`](../../demo/psm/town_ray_data.h)). A rising ray stops when it clears 28 m or leaves the grid. The cell test is conservative: a cell blocks if its height is above the ray's height where the ray enters it. At 200 MHz, 1 ms is 200,000 cycles.

**What the grid cannot do**, for every use below:

- It is a height field: no overhangs, no ceilings, no rooms, no doors. A use that needs inside spaces needs another structure.
- It holds static casters only. People and hens are not in it; test them with the proxy pairs (SH4-055).
- At 1.5 m a cell, a post or a thin wall is either a whole cell or absent.
- The conservative test errs toward "blocked": right for shadows, a little pessimistic for sight and sound.
- All costs are for a warm grid (4 KB in the operand cache). A frame's first ray after other work pays a cold start: at most about 47 us, by SH4-053.

## Group A: the measured kernel as it is

These rays rise from a point in a fixed direction until they clear the tallest caster, exactly as the bench's sun rays do. Only the direction changes, so the cost per ray is the measured one. Each is still ESTIMATE until timed in its own loop.

| # | Use | What it gives | Rays a frame | Cells a ray | Cycles a frame (DERIVED) | ms | Sound chip side | Test that would prove it | Status |
|---|---|---|---|---|---|---|---|---|---|
| RU1 | **Sky openness at the listener** (a reverb probe) | How enclosed the listener is: a street between tall houses against an open square. Drives the reverb's wet level and its length | 8 (8 azimuths at 20° elevation), every 4th frame | ≤ 51 (a 28 m climb at 20° is 77 m) | 8 × (74 + 51 × 22.2) = 9,648; 2,412 a frame over 4 frames | 0.048 (0.012 a frame) | One or two writes a change: the DSP's send level or a reverb coefficient. A reverb program on the DSP and the silent DSP blur (S1) cannot both hold the DSP; whether a muted DSP still runs is [Q59](../open-questions.md) | Host: open fraction over a walk through the town against a picture of the grid. Console: the 8 rays timed in a game frame | ESTIMATE |
| RU2 | **Wind shelter map**, baked at load | A byte a cell: how sheltered it is from one wind direction. Drives motes, mist drift, cloth and grass sway, and wind noise level | 0 a frame; 4,096 at load (one a cell) | ≤ 64 (at 10° the ray leaves a row of the grid before it clears 28 m) | 4,096 × 1,486 = 6.09 M once a wind direction | 30 at load; 243 for 8 directions | Wind sound level from the listener's cell: one level write when it changes | Host: the map against the grid; the bake timed once on a console | ESTIMATE |
| RU3 | Ambient occlusion per vertex | [X5](depth-shading-ideas.md#ideas-from-console-results-measured-edges-and-punch-through-route-c4b-ray-bench-run-one-ray1-town-2026-11-25) | 8 a vertex | 6 | 1,650 a vertex | 8.2 us a vertex | — | X5's test | ESTIMATE |
| RU4 | Contact darkening for movers | [X6](depth-shading-ideas.md#ideas-from-console-results-measured-edges-and-punch-through-route-c4b-ray-bench-run-one-ray1-town-2026-11-25) | one a vertex near a mover | ≤ 4 | 160 a vertex; 500 vertices 80,000 | 0.4 | — | X6's test | ESTIMATE |
| RU5 | Sun shafts in mist | [X8](depth-shading-ideas.md#ideas-from-console-results-measured-edges-and-punch-through-route-c4b-ray-bench-run-one-ray1-town-2026-11-25) | 972 | as the sun's | 972 × 554 = 0.54 M | 2.7 | — | X8's test | ESTIMATE |
| RU6 | **Sun visibility of a sound source or a mover** | Whether a person stands in sun or shade: a gameplay flag (stealth, heat), a cue for a sound (birdsong in the sun, a drip in the shade) | one a mover; 12 movers | as the sun's | 12 × 563 = 6,756 | 0.034 | A level change for a source when its flag flips | Host: flags against the shadow picture; console: the 12 rays timed | ESTIMATE |

## Group B: the same walk to a target point

These rays run from one point to another (an eye to a target, a source to a listener) and stop at the target instead of at the tallest caster. The inner step is the same compare and step, so ESTIMATE the same 22 cycles a cell; the setup and the end test differ, so the kernel needs a variant, its own host test against a reference, and one console row. n is the cells between the two points: 64 cells is 96 m along a row, the grid's width.

| # | Use | What it gives | Rays a frame | Cells a ray | Cycles a frame (DERIVED) | ms | Sound chip side | Test that would prove it | Status |
|---|---|---|---|---|---|---|---|---|---|
| RU7 | **Occluded sound** (expands [X7](depth-shading-ideas.md#ideas-from-console-results-measured-edges-and-punch-through-route-c4b-ray-bench-run-one-ray1-town-2026-11-25)) | A source behind a building is quieter and duller. Three rays a source (the source to each ear and the head) give a soft edge as it passes a corner | 16 sources × 3 = 48, at 15 Hz (every 4th frame) | ≤ 64 | 48 × 1,486 = 71,328 worst; 17,832 a frame over 4 frames | 0.36 worst; 0.09 a frame | A level and a filter cutoff a source when the result changes; the pan comes from the direction alone and needs no ray. Two to three register writes a source, 4 bytes at a time ([AICA-011](../hardware/ledger-aica.md)), a G2 FIFO wait after every 8 ([AICA-012](../hardware/ledger-aica.md)). The voice registers for pan, level, filter and send are not yet in the ledger, and the cost of one write is not measured ([Q65](../open-questions.md)) | Host: levels along a walk past a corner; console: the rays and the register writes timed in a game frame, and listened to | ESTIMATE |
| RU8 | **AI line of sight** | Can a guard see the player; can a hen see the fox. Height of the eye and of the target set the ray's slope | 8 watchers × 1, every frame | ≤ 64 | 8 × 1,486 = 11,888 | 0.06 | — | Host: sight against a picture of the grid; console: timed | ESTIMATE |
| RU9 | **Camera collision and picking** | A third-person camera pulled in when a wall is between it and the player; what the pointer or the aim is on | 1 to 2 | ≤ 64 | ≤ 2,972 | 0.015 | — | Host: the hit cell against a reference | ESTIMATE |
| RU10 | **Projectile paths** | A bullet, arrow or thrown stone tested against buildings over the distance it moved this frame; hitscan as one long ray | 32 projectiles × 1 of ≤ 4 cells; or 4 hitscans of ≤ 64 | ≤ 4 (or ≤ 64) | 32 × (74 + 4 × 22.2) = 5,210; hitscan 4 × 1,486 = 5,944 | 0.026; 0.030 | A hit sound: one key-on with level and pan from RU7's figures | Host: hits against a reference stepper | ESTIMATE |
| RU11 | **Visibility culling against buildings** | An object hidden behind a building is not sent to the TA. A ray from the eye to the top of the object's bounds; blocked means skip | 64 objects | ≤ 64 | 64 × 1,486 = 95,104 worst | 0.48 worst | — | Host: count of objects culled and of TA vertices saved over a walk. It pays only when the culled objects cost more than 0.48 ms to send; the town sends 27,000 TA vertices a frame on route A ([PVR-108](../hardware/ledger-pvr.md#town-scene-hud-photos)) | ESTIMATE |

## Group C: a single cell, no walk

| # | Use | What it gives | Reads a frame | Cycles a frame (DERIVED) | ms | Sound chip side | Test that would prove it | Status |
|---|---|---|---|---|---|---|---|---|
| RU12 | **Footstep and ground probes** | The ground height under a foot (and, with a second byte grid, the material: cobbles, grass, snow). Footstep sound choice, foot placement, a dust puff | 12 movers × 2 feet = 24 | 24 × 94 = 2,256, an upper bound (a whole 1-cell ray, SH4-052) | 0.011 | A key-on a step: the sample by material, level and pan from RU7 | Host: heights under a walk against the grid; the material grid is a new table, not measured | ESTIMATE |

## Group D: rays against triangles (Ray bench, repeat and clock's test)

Ray bench, repeat and clock timed a shadow ray against triangles with no divide ([SH4-066](../hardware/ledger-sh4.md#ray-bench-repeat-and-clock-repeat-of-run-one-the-clock-the-ray-triangle-test)): **33.16 cycles a test** when every ray shares one direction and each triangle is prepared once for it (64 bytes a triangle), **62.49** for any direction, both with early outs and 96 triangles in the cache. Triangles hold what the grid cannot: overhangs, arches, inside spaces, movers' own shapes. A ray's cost is its tests × the cycles a test. How many triangles a ray must test depends on bins that are not built or measured; the town's host count is 18 tests for a model vertex's ray (TOWN-060), and the bench's own scene 89.2. All rows ESTIMATE.

| # | Use | What it gives | Rays a frame | Tests a ray | Cycles a frame (DERIVED) | ms | Sound chip side | Test that would prove it | Status |
|---|---|---|---|---|---|---|---|---|---|
| RU13 | **Picking and aim against a mover's mesh** | The exact part hit (head, arm, a hen's wing), after the grid (RU9) and a bounding sphere say the ray is near | 1 to 2 | ≤ 100 (a whole small mesh) | 2 × 100 × 62.49 = 12,498 | 0.062 | A hit sound by part | Host: the part hit against a reference; console timed | ESTIMATE |
| RU14 | **Bullets against movers' meshes** | Hits on people and hens, which the grid does not hold; the grid (RU10) handles buildings | 32 near a mover | 18 | 32 × 18 × 62.49 = 35,994 | 0.18 | As RU10 | Host: hits against a reference stepper | ESTIMATE |
| RU15 | **Sound occlusion with triangles** | RU7 inside spaces, under arches and overhangs, through a doorway: what the height grid gets wrong | 16 sources × 3 at 15 Hz | 18 | 48 × 18 × 62.49 = 53,991; 13,498 a frame over 4 frames | 0.27; 0.067 a frame | As RU7 | As RU7, in a scene with an arch or a room | ESTIMATE |
| RU16 | **Rain and snow cover** (prepared form) | Which ground points lie under eaves, awnings and arches: dry patches in rain, bare ground in snow, no splashes there. Rain falls in one direction, so every ray shares it, as the sun's do. Feeds the deform grid (`sh4der_deform.h`: snow only where it falls), a wet-ground darkening, and splash spawns (sparks, motes) | 0 a frame; 6,305 ground points (TOWN-066) at load or when the wind turns the rain | 18 | 6,305 × 18 × 33.16 = 3.76 M once | 18.8 once | Rain on a roof against rain on the ground: two loops, a level each by cover under the listener | Host: the cover map against a picture of the town; the bake timed once on a console | ESTIMATE |
| RU17 | **Static lamp occlusion** | A street lamp's pool (`sh4der_pool.h`) cut by a wall or a cart: rays from ground points to the lamp. Every ray has its own direction, so any-direction tests; a static lamp and static casters bake once | 0 a frame; 200 ground points a lamp at load | 18 | 200 × 18 × 62.49 = 225k a lamp | 1.1 a lamp, once | — | Host picture; one lamp's bake timed | ESTIMATE |

**What triangles do not pay for.** Contact darkening round a mover from its own triangles: 18 tests × 33.16 = 597 cycles a receiver, against 137.80 for a receiver-proxy pair ([SH4-064](../hardware/ledger-sh4.md#ray-bench-repeat-and-clock-repeat-of-run-one-the-clock-the-ray-triangle-test)). Keep the proxies for movers (X6, [X10](depth-shading-ideas.md#ideas-from-console-results-measured-edges-and-punch-through-route-c4b-ray-bench-run-one-ray1-town-2026-11-25)); use triangles for static shapes the grid cannot hold.

**The bake.** The sun bake (TOWN-063) moves from an assumed 50 cycles a test to 33.16 measured with the prepared form: about 28 ms a sun with the proxies' test count, 96 ms with the screen mesh's (host counts; `RAY2_RESULTS_junior.md:116-133`). Rain has its own direction, so RU16 prepares the triangles again: the unit of work is one prepare a direction, shared by every use with that direction.

**Caveat.** 33 and 62 cycles are for 96 triangles in the cache. The town's 1,162 triangles prepared are 74 KB, more than the 16 KB operand cache: bin them and keep a ray's bins together, or measure the misses (TOWN-085).

## A frame of game rays, added up

One frame of a game that runs RU1, RU7, RU8, RU9, RU10 (projectiles), RU6 and RU12 together, all from the rows above: 2,412 + 17,832 + 11,888 + 2,972 + 5,210 + 6,756 + 2,256 = **49,326 cycles, about 0.25 ms** (DERIVED, ESTIMATE). Add a cold start of at most about 0.05 ms (SH4-053). That is 0.44 % of the town's 55.6 ms frame at 18 fps ([PVR-108](../hardware/ledger-pvr.md#town-scene-hud-photos)) and 1.5 % of a 16.7 ms frame at 60 fps. RU11's culling adds up to 0.48 ms; RU3 to RU5 are the shading uses and have their own budgets.

Where to run them: batched in one block, not mixed into the vertex transform (mixing cost 89 to 164 cycles a ray over two runs, SH4-053, SH4-062; [X9](depth-shading-ideas.md#ideas-from-console-results-measured-edges-and-punch-through-route-c4b-ray-bench-run-one-ray1-town-2026-11-25)). If the render-done wait can be cut ([X4](depth-shading-ideas.md#ideas-from-console-results-measured-edges-and-punch-through-route-c4b-ray-bench-run-one-ray1-town-2026-11-25), [Q53](../open-questions.md)), they fit in that gap.

## The sound chip side

- **What needs no ray.** A source's pan and its distance level come from its position against the listener: arithmetic on the SH-4, no grid.
- **What a ray adds.** Occlusion (RU7) lowers a source's level and its filter cutoff; the room probe (RU1) sets the reverb's wet level.
- **How it gets there.** Every change is a register write from the SH-4 across G2, 4 bytes at a time (AICA-011), with a FIFO wait after 8 writes (AICA-012). G2 DMA is 5.0 to 5.7 MB/s ([AICA-020, AICA-021](../hardware/ledger-aica.md)), but a few register writes are not a DMA job. Write only on a change, not every frame.
- **An idea, ESTIMATE:** the SH-4 writes each source's target level and cutoff to sound RAM once an update; the ARM ramps the voice registers toward them on its own timer ([AICA-027](../hardware/ledger-aica.md); [S2](sound-chip-precompute.md#ideas)). That smooths the steps (no zipper noise) and keeps the per-tick register writes off G2. It rests on the ARM's speed ([Q28](../open-questions.md), [Q30](../open-questions.md)), not measured.
- **The DSP.** A reverb program is the DSP's normal job. The silent blur ideas (S1, S5) would need the DSP too; one program at a time. A voice's output into the DSP costs no CPU ([AICA-030](../hardware/ledger-aica.md)).
- None of S1 to S9 ([sound chip](sound-chip-precompute.md#ideas)) runs rays on the sound chip: the rays stay on the SH-4, where the grid is.

## Open

- The point-to-point kernel (group B): write it beside the sun kernel in [`ray_kernels.h`](../../demo/psm/bench/ray_kernels.h), test it against a C reference on the host, time it on a console.
- The voice registers for pan, level, filter and send as FACT rows from the AICA manual; the cost of one SH-4 register write across G2 ([Q65](../open-questions.md)).
- Whether a game frame keeps the grid warm: time RU1 to RU12 together after a frame's other work.
- A material grid for RU12: a second byte a cell, or the top bits of the height byte (which costs height range).

## Related

- [Depth shading ideas: ideas from console results](depth-shading-ideas.md#ideas-from-console-results-measured-edges-and-punch-through-route-c4b-ray-bench-run-one-ray1-town-2026-11-25) (X4 to X10)
- [The sound chip as a precompute unit](sound-chip-precompute.md)
- [SH-4 ledger: ray bench ray1](../hardware/ledger-sh4.md#ray-bench-run-one-thesis-p1-p2-p9)
- [AICA ledger](../hardware/ledger-aica.md)
- [The shadow thesis](../shadows/thesis.md)
