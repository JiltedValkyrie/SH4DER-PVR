---
title: "Genre guide: shadows and depth"
tags: [guide, genre, shadows, effect]
updated: 2026-11-26
sources: [wiki/STRUCTURE.md, wiki/effects/names.md, wiki/start-here/what-do-you-want.md, wiki/start-here/call-orders.md, wiki/glossary.md, wiki/limits/header-recommended.md, wiki/limits/recommended-limits.md, wiki/status/maturity.md (rows found by search), wiki/effects/psm.md, wiki/effects/ao.md, wiki/effects/aobake.md, wiki/effects/aosky.md, wiki/effects/sh4dow-dc.md, wiki/shadows/psm-routes.md, README.md (the psm, ao, aobake and aosky effect sections; the SH4DOW-DC paragraph), include/sh4der_psm.h, include/sh4der_psm_raster.h, include/sh4der_psm_pvr.h, include/sh4der_ao.h, include/sh4der_aobake.h, include/sh4der_aosky.h, README.md 'The sh4dow effect', include/sh4dow_dc.h, AGENTS.md]
---
# Genre guide: shadows and depth

**What you will learn:** which shadow or depth effect fits your picture, what each one costs on the Dreamcast, and which of its numbers are measured.
**For:** a programmer on a Dreamcast project who wants sun or lamp shadows, darker corners, or soft shadows on a character.

A shadow map is a picture drawn from the light. A point is in shadow when something sits between it and the light. The graphics chip (the PVR, see the [glossary](../glossary.md)) cannot compare depths one pixel at a time, so this library draws the map in two ways and places it with one set of maths. Two effects darken corners with no map at all. Fake focus blur [focus] and light through thin parts [sss] are on the other genre guides. The [names list](../effects/names.md) links all six.

## What you can make

- A sun's shadow of cars, characters and props on a road or a floor.
- A shadow that falls on a model itself, for a few low-detail casters.
- Dark creases where walls meet floors, baked at load so the frame pays nothing for them.
- A faint contact shadow under props on flat ground, and soft shadows on a character's limbs.

Most numbers here come from one console, and several from one view. The walled-square scene and the town scene use the psm headers, and scenes A and B use aobake. The [demo guide](demo-guide.md) says what each scene shows.

## The effects

Four headers (psm, psm_raster, psm_pvr and ao) have no RECOMMENDED block yet ([template](../limits/header-recommended.md)). Their settings are header or README defaults, so they are ADVISED unless a line says otherwise.

### Sun shadow map [psm]

**What it looks like:** a sun's or lamp's shadow on ground seen at a low angle, sharper near the camera for the same map size.
**Use it when:** a directional light, such as a sun or a moon, lights ground that the camera sees from low down.
**Where it is:** [header](../../include/sh4der_psm.h) and [effect page](../effects/psm.md). This is the placement. The drawing routes are the next two effects.
**Starting settings:**
- `SH4DER_PSM_BIAS_A` 0.02f (metres) and `SH4DER_PSM_BIAS_B` 1.5f (texels), from include/sh4der_psm.h:91-92. ADVISED: no console number backs them.
- `warp_near` 4 m, the argument to `sh4der_psm_build_warp`, raised above the render's 0.3 m near plane (README, "The psm effect"). ADVISED.
- Not an infinite draw distance: give the shadow a range and keep casters inside the map's square. An infinite range with all casters missed 30 fps in the town scene (PVR-108 to PVR-113). PROVEN that it misses; the remedy is ADVISED ([recommended limits](../limits/recommended-limits.md)).

### Shadow map on the SH-4 [psm_raster]

**What it looks like:** a shadow that falls on the model itself as well as on the ground, with one light value a vertex.
**Use it when:** a few low-detail casters must shade themselves over ground with dense vertices (include/sh4der_psm_raster.h:45-46).
**Where it is:** [header](../../include/sh4der_psm_raster.h). It has no effect page of its own, so see the [PSM routes page](../shadows/psm-routes.md) and the [psm effect page](../effects/psm.md).
**Starting settings:**
- A 128 x 128 map, 32 KB of RAM (include/sh4der_psm_raster.h:34). ADVISED: not run at this size. The town scene's route B used 256 x 256.
- `SH4DER_PSM_RASTER_TEXEL_MAX` 5.0f (include/sh4der_psm_raster.h:71): the largest texel size the bias follows, in metres. ADVISED.
- `SH4DER_PSM_RASTER_STEP` 0 (include/sh4der_psm_raster.h:81): the default row fill. ADVISED. The SH4ZAM form, 1, is not faster on covered texels (README, psm tuning table).

### Shadow map drawn by the PVR [psm_pvr]

**What it looks like:** a flat dark shadow laid on the ground as a multiply. The casters do not shadow themselves.
**Use it when:** the casters are separate from the receivers, such as cars on a road (include/sh4der_psm_pvr.h:23-24).
**Where it is:** [header](../../include/sh4der_psm_pvr.h), the [PSM routes page](../shadows/psm-routes.md) and the [psm effect page](../effects/psm.md). The target pass and the layer are not in the library yet. The town scene has its own.
**Starting settings:**
- `SH4DER_PSM_PVR_MAX_TRIS` 10 (include/sh4der_psm_pvr.h:52): a cap on triangles per world triangle. ADVISED.
- `SH4DER_PSM_PVR_UV_GRID` 5 (include/sh4der_psm_pvr.h:53): the error probe's grid. On the host, a 1 m ground cell is off by 1.1 texels at 6 m and 0.07 at 20 m; a 4 m cell by 57 and 1.5 (README, "The psm effect"). ESTIMATE, from the host.
- A 256 x 256 target, 128 KB of video memory, as the town scene used it. ADVISED.

### Corner darkening [ao]

**What it looks like:** creases and corners are a little darker, so a model reads as solid.
**Use it when:** you bake a crease into vertex colour at build time, or want a cavity cue on the finished frame, one frame behind.
**Where it is:** [header](../../include/sh4der_ao.h) and [effect page](../effects/ao.md).
**Starting settings:** NOT RUN on a console. No program uses this header yet (maturity: PROTOTYPE v0.1). The values are ADVISED defaults.
- `SH4DER_AO_CREASE_K` 0.5f (include/sh4der_ao.h:54): a full concave crease keeps half its colour.
- `SH4DER_AO_SCR_K` 0.5f and `SH4DER_AO_SCR_ENGAGE` 0.2f (include/sh4der_ao.h:58-59): the cue's depth, and the luma floor below which nothing darkens.
- `SH4DER_AO_SCR_GRID_X` 80 and `SH4DER_AO_SCR_GRID_Y` 60 (include/sh4der_ao.h:56-57): the grid the look effect reads.

### Baked corner shading [aobake]

**What it looks like:** a soft dark band where a wall meets a floor, and under a box, in the scenery's own vertex colour.
**Use it when:** the geometry is static. The bake runs once at load, so a moving object needs a new bake and is not a fit.
**Where it is:** [header](../../include/sh4der_aobake.h), [effect page](../effects/aobake.md) and the host test `test/aobake_test.c`.
**Starting settings:**
- `SH4DER_AOBAKE_RAYS` 64 (include/sh4der_aobake.h:64): rays a point. The bake cost is linear in it. ADVISED for this copy.
- `SH4DER_AOBAKE_REACH` 3.0f (include/sh4der_aobake.h:65): metres a ray looks for an occluder. ADVISED.
- `SH4DER_AOBAKE_PUSH` 0.02f (include/sh4der_aobake.h:66): metres a ray starts off its surface. ADVISED.

### Sky shading [aosky]

**What it looks like:** a faint dark patch on flat ground under props, drawn as extra copies of the sun's shadow.
**Use it when:** the sun's shadow already goes into a target and the receiver is flat ground. It does not darken walls or moving things.
**Where it is:** [header](../../include/sh4der_aosky.h) and [effect page](../effects/aosky.md).
**Starting settings:**
- `SH4DER_AOSKY_DIRS_MAX` 8 (include/sh4der_aosky.h:85) is the cap. Start at 4 directions with `sh4der_aosky_header(h, 0)` (plain blending): PROVEN for that start (PVR-138 to PVR-186, one console). This copy since the move: Run on a console by the owner in Room shading, four ways (her test; no measured numbers recorded yet).
- `SH4DER_AOSKY_FLOOR` 0.5f (include/sh4der_aosky.h:87): PROVEN for the same start.
- One side of closed casters: ADVISED. The header's block counts it in its PROVEN start, but the [recommended limits](../limits/recommended-limits.md) hold it at ADVISED until open question Q78 closes. This page follows the limits page.

### Soft character shadows [sh4dow]

**What it looks like:** a character's limbs cast soft dark columns along the light, and two small cones of shadow sit under its feet.
**Use it when:** a few characters near the camera need soft shadows. The cost grows with each character and column.
**Where it is:** in this library: `include/sh4dow_dc.h`, `src/sh4dow_dc.c` and `src/sh4dow_foot.c`, with the heightfield sun in `include/sh4dow_heightfield.h`. Effect page: [sh4dow](../effects/sh4dow.md). Credit and use rules: [licence and credit](license-and-credit.md).
**Starting settings:** Run on a console by the owner in Demo disc (her test; no measured numbers recorded yet). The numbers below are the original author's (REPORTED).
- `SH4DOW_COLUMN_MAX` 32 (README.md, 'The sh4dow effect', line 1755): the columns one frame's state holds, about 3 KB. ADVISED, a cap in the code.
- `sh4dow_foot_range_fade` is 1 up to 18 and 0 at 20 (README.md, line 1794). These are the original game's units, where 1 unit is 3.048 m (README.md, line 1753). ADVISED: the library's default, not measured.
- The count of shadowed characters: none is recommended. The original game shaded its six nearest, with 14 limbs each (README.md, 'The sh4dow effect', line 1755). That is one game's choice. The bench that would set the count is OPEN.

## What it costs

| Effect | Video memory | Triangles or vertices | SH-4 time | Status |
|---|---|---|---|---|
| Sun shadow map [psm] | none (maths) | none of its own | none of its own | The 80 B map description is RAM: ESTIMATE, from a host `sizeof`. Cost on a console OPEN. ALPHA v0.5 |
| Shadow map on the SH-4 [psm_raster] | none; 32 KB of RAM at 128 x 128 (FACT, arithmetic) | 15,142 TA vertices, route B, town scene (MEASURED, one console, 2026-11-25) | 40.2 ms uniform, 43.9 ms PSM (MEASURED, HUD). The route's own share is OPEN | PROVEN: too costly for a whole town at 30 fps (one view). ALPHA v0.5 |
| Shadow map drawn by the PVR [psm_pvr] | 128 KB, a 256 x 256 target (FACT, arithmetic) | 27,023 to 27,075 TA vertices, route A, town scene (MEASURED) | 13.3 to 13.6 ms (MEASURED, HUD). The 18 fps is below these times: OPEN | ALPHA v0.5 |
| Corner darkening [ao] | not stated | the crease bake costs nothing at run time (as-stated by the header) | not measured. OPEN: time the crease bake and the screen cue on a console | NOT RUN on a console. PROTOTYPE v0.1 |
| Baked corner shading [aobake] | 0 (by the code) | 2,142 vertices in scene A, 2,544 in scene B, 64 rays each (exact counts) | load: 252 to 254 ms (A) and 391 to 393 ms (B) (MEASURED, one console, SH4-076, SH4-082). The frame moved by under 0.6 ms (PROVEN, PVR-135, PVR-138, SH4-073, SH4-076, SH4-077) | ALPHA v0.5. This copy since the move: Run on a console by the owner in Room shading, four ways (her test; no measured numbers recorded yet). Placing cost OPEN (Q91) |
| Sky shading [aosky] | 0 more (it uses the sun's target) | 1,164 caster triangles, one side (exact count) | 522 ns a vertex (MEASURED, one scene, SH4-071). Town scene at 4 directions: frame +10.0 ms (MEASURED, PVR-135) | PROVEN for the start. This copy since the move: Run on a console by the owner in Room shading, four ways (her test; no measured numbers recorded yet) |
| Soft character shadows [sh4dow] | nothing extra (as-stated) | 6 characters, 84 columns, 5,406 vertices (REPORTED) | 14.70 ms before the column lists, 4.5 ms after (REPORTED, one run, no date) | Run on a console by the owner in Demo disc (her test; no measured numbers recorded yet) |

Video memory is the graphics chip's own memory for pictures and textures. A triangle or vertex is one corner the chip is sent each frame, and the count is the work it does.

PVR time, where it matters: psm_pvr is 15.9 ms in the town scene (MEASURED, HUD). aosky is about 451 us a direction in scene A (DERIVED from PVR-140), and 753 us a direction for one side of 1,164 triangles (DERIVED from PVR-130). The scenes are described on the [scenes page](../limits/scenes.md).

## Try this first

**Recipe: baked corner shading [aobake].** It runs once at load, so the frame pays little for it.

1. Include `sh4der_aobake.h` from the library's `include` folder. Its maths uses SH4ZAM on the Dreamcast, or plain C with `SH4DER_CORE_NO_SH4ZAM` on a host build. Keep the [credit notice](license-and-credit.md).
2. At load, fill a `sh4der_aobake_occl_t` with your boxes and spheres. Set `ground` to 1 and `ground_y` to the floor's height (include/sh4der_aobake.h:73-79).
3. Call `sh4der_aobake_rays(dirs, 64)` into a `float dirs[64][3]` array you own. Then call `sh4der_aobake_bytes` over your static vertices and their unit normals, with a tick function for a watchdog. Scene A took 252 to 254 ms on one console (MEASURED, SH4-076). This copy has not run since the move, so time your own scene.
4. Keep the bytes. Each frame, multiply the ambient light at each vertex by `sh4der_aobake_open(byte, 0)`, a 0 to 1 share.
5. The wall-and-floor corners, and the ground under a box, should read darker than open floor. A vertex sunk inside a box reads black. In direct sun with colour the effect is hard to see (ADVISED, one run, [recommended limits](../limits/recommended-limits.md)).
6. With the bytes in use, the frame should move by under 0.6 ms (PROVEN). A slower frame suggests the bake ran inside the frame, not at load.

The steps are in the header's top comment (include/sh4der_aobake.h:34-39). AGENTS.md has call orders for other effects, not this one. The [call orders](call-orders.md) page explains the shape they share.

## Related

- [What do you want to make?](what-do-you-want.md)
- [PSM routes](../shadows/psm-routes.md), for the two drawing routes and their costs
- [effect index](../effects/README.md)
- [recommended limits](../limits/recommended-limits.md)
- [glossary](../glossary.md)
- [licence and credit](license-and-credit.md)
- [demo guide](demo-guide.md)
