---
title: "Planar reflection [pl4nar]: the world mirrored in a floor, car paint or a wet road"
tags: [effect, pl4nar, reflection]
updated: 2026-11-26
sources: [include/sh4der_pl4nar.h, src/sh4der_pl4nar.c, README.md "The pl4nar effect", AGENTS.md "Call order: pl4nar", demo/pl4nar/AGENT_WORKFLOW.md]
---
# Planar reflection [pl4nar]: real-time, through a small render target

**What you will learn:** how the world is mirrored into a small render target and looked up per vertex, what it costs in video memory, and which costs are still unmeasured.
**For:** a programmer adding reflections to car paint, glass, chrome, a wet road, water or a polished floor.

> **In plain words.** The world is drawn a second time, turned upside down, into a small [render to texture](../glossary.md) picture. Shiny things, such as car paint, glass or a wet road, show that picture, so they look like a mirror of the street. The reflection is strongest where a surface is seen at a sharp angle. Use it when a shiny floor or car body must show its surroundings, and the frame has room for a second pass of the world.

## Try this first

**What it is for:** a mirror of the world in a shiny floor or car paint, made from one small picture drawn each frame.

1. **Make the target.** `sh4der_pl4nar_target_init(&target, 256, 128)` gives a 256 × 128 target, 64 KB with `SH4DER_PL4NAR_HAVE_RTT` (include/sh4der_pl4nar.h:8; ADVISED). That flag needs `pvr_scene_begin_rtt` (README:957). The sources disagree on the size. The header (line 8) and AGENTS.md say turn it down before anything else. This page's Recommended limits says the ledger (PVR-174, PVR-175) does not support that order. Not picked here: measure the mirror pass at each size.
2. **Set the field of view.** `SH4DER_PL4NAR_FOV_SCALE` = 1.55 for a body, 1 for a flat surface (include/sh4der_pl4nar.h:118; README:1033; ADVISED).
3. **Set the paint.** `SH4DER_PL4NAR_F0_PAINT` = 0.06 (include/sh4der_pl4nar.h:112; README:1027; ADVISED). The earlier client's paint read best at 0.14 (README:1020), so 0.06 is a start, not a look. No recommended value yet for a stylised paint: judge 0.06 and 0.14 on screen.
4. **Set the reach and the ground.** `SH4DER_PL4NAR_REACH` = 18.0 world units (include/sh4der_pl4nar.h:116; README:1031) and `SH4DER_PL4NAR_GROUND_KEEP` = 0.55 (include/sh4der_pl4nar.h:121; README:1036). Both ADVISED.
5. **Set the other materials.** Glass: `SH4DER_PL4NAR_F0_GLASS` = 0.10 (include/sh4der_pl4nar.h:113; README:1028; ADVISED). Paint is additive and glass is blended (README:950). The wet road's gain of 0.7 and the floor's 0.8 (README:1017-1018) have not been judged on screen (README:1009): no recommended value yet.

On a console: the page's status, "Measured on a console so far: the street demo's lookups and targets only" (Times). Its mirror pass PVR time has street demo rows too (PVR-174, PVR-175).

Read next: [the header](../../include/sh4der_pl4nar.h), [the call order](../../AGENTS.md#call-order-pl4nar).

## What it does

The world is drawn a second time, mirrored about a plane, into a small [render target](../glossary.md). A surface then looks the target up per vertex. That surface is either a body near the plane (`sh4der_pl4nar_shade`, for paint or glass) or a surface in the plane (`sh4der_pl4nar_flat`, for water or a road). The reflection is blended in as one translucent layer, with Fresnel as its vertex alpha (README:848-945).

The header is the view rows, the lookup, the materials and the split: inline and host tested. `src/sh4der_pl4nar.c` is the target, the pass and the layer header, Dreamcast only. It needs `pvr_scene_begin_rtt` to compile with `SH4DER_PL4NAR_HAVE_RTT`.

Version: v0.10, passes its maths tests (pl4nar.h:75).

## Call order

- `AGENTS.md` "Call order: pl4nar" has the seven steps.
- For an engine that has no normals, see "pl4nar in an engine that has no normals" in `AGENTS.md`, and `demo/pl4nar/AGENT_WORKFLOW.md` for the step-by-step version.
- The client code to copy from is `demo/pl4nar/src/demo.c`.

## Cost

Video memory is exact arithmetic: the target's pitch × height × 2 B (README:865).

| Target | With `HAVE_RTT` | Without (pitch 1,024) |
|---|---|---|
| 128 × 64 | 16 KB | 128 KB |
| 256 × 128 | 64 KB | 256 KB |
| 512 × 256 | 256 KB | 512 KB |

RAM: view 104 B (112 aligned), material 24 B, header 32 B. Code: 692 B with `HAVE_RTT`, 720 B without (SH-4, -O3, measured as a size).

Times (README:869-873). **Measured on a console so far: the street demo's lookups and targets only** (Street car demo [pl4m], build c5, suite 7 [s7], one console, one run):

| Item | What is known | Status |
|---|---|---|
| Lookups, street demo | 0.92 to 0.97 ms a frame for the player car's body, 320 vertices; about 2.9 to 3.0 us a vertex (DERIVED) (PVR-160) | MEASURED, 1 console, 1 run |
| Targets, street demo | 32 KB at 128 × 64, 128 KB at 256 × 128, 512 KB at 512 × 256, no NOVRAM in any stage (PVR-161, PVR-175). The demo's target is twice the mirror's width (the mirror and the shadow tiles side by side), so these are 2 × w × h × 2 B; the library's own target for a mirror alone is half that | MEASURED, 1 console, 2 runs |
| Mirror pass, SH-4, street demo | about 41 ms a frame: reflection on against off, frame times MEASURED (PVR-160); the target size changes nothing | DERIVED |
| Mirror pass and screen PVR, street demo | 3.55 to 9.43 ms in the page's columns, from a bug in our sampling (PVR-162) | ESTIMATE |
| Lookup, body | 0.43 ms for 1,623 vertices, one run of an older version | REPORTED (emulator, not evidence). This version is estimated 15-20 % less. |
| Lookup, flat | under the body's | ESTIMATE |
| Mirror pass, CPU | the world pass × the mirrored share | ESTIMATE |
| Mirror pass, PVR, street demo | 3.23, 3.31 and 3.26 ms at 256 × 128 in stages 1, 4 and 11 of PVR-174 (no key pressed); 3.20 ms at 128 × 64 in PVR-175 stage 13 (a key was pressed there, so that stage is TOUCHED). The "15 ms at 512 × 256" in the first client was emulator queue time, not pass time. | MEASURED, 1 console (PVR-174, PVR-175) |
| Layer, PVR | under 0.5 ms for a car body | ESTIMATE |

## Recommended limits

- One plane per target. Two planes need two targets and two passes (README:859).
- Start the target at 256 × 128, the header's start size. In the car runs the target's size did not change the mirror pass's PVR time (PVR-174, PVR-175, MEASURED, one console), so measure the mirror before you cut size. `AGENTS.md` says to go down in size first; the ledger does not support that order. This is ADVISED; see [recommended limits](../limits/recommended-limits.md).
- Nothing reflects itself. A body needs normals (README:859).

## Tuning

See `README.md` "Materials" (README:913-926) and "Pl4nar tuning" (README:928-944). Only paint and glass were judged, on a host picture; the other materials are untested gains (pl4nar.h:145).

## Pitfalls

From README:860-864 and `AGENTS.md`:

- Keep the row form. Building the view with a chain of matrix calls gave a wrong camera in the first client; the cause was not isolated.
- XMTRX is shared. Make no `shz_xmtrx_*` or `mat_*` call between `begin` and the last lookup. Load your own matrix again afterwards.
- FTRV loses precision in kilometre-sized worlds. Build the rows about the eye.
- Without `pvr_scene_begin_rtt`, the target is 1,024 wide: 4 times the video memory at 256 × 128.
- With direct render, make no store-queue use (`pvr_txr_load`, `sq_cpy`) between list begin and finish.
- Do not draw the layer before the opaque surface. Do not write depth from the layer. Do not allocate the target every frame.

## Open

- Every console time: lookup, mirror pass CPU, mirror pass PVR, layer, frames a second with the effect off and on. The cost table to fill is in `demo/pl4nar/AGENT_WORKFLOW.md`:60-71. Each row must be marked console, emulator or estimate, never mixed.
- The render-target path on a console.
- The cause of the matrix-chain bug (README:860).

## Related

- [Light pools](pool.md), [Sparks and tracers](spark.md) (halos in the mirror pass), [Send checks](guard.md)
- [Video ledger](../hardware/ledger-video.md), [budgets](../limits/budgets.md)
