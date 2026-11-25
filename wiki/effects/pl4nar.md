---
title: pl4nar (planar reflection)
tags: [effect, pl4nar, reflection]
updated: 2026-11-25
sources: [include/sh4der_pl4nar.h, src/sh4der_pl4nar.c, README.md "The pl4nar effect", AGENTS.md "Call order: pl4nar", demo/pl4nar/AGENT_WORKFLOW.md]
---
# pl4nar: real-time planar reflection

**What you will learn:** how the world is mirrored into a small render target and looked up per vertex, what it costs in video memory, and which costs are still unmeasured.
**For:** a programmer adding reflections to car paint, glass, chrome, a wet road, water or a polished floor.

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

Times (README:869-873). **Measured on a console so far: the street demo's lookups and targets only** (pl4m c5, suite s7, one console, one run):

| Item | What is known | Status |
|---|---|---|
| Lookups, street demo | 0.92 to 0.97 ms a frame (PVR-160) | MEASURED, 1 console, 1 run |
| Targets | 32 KB at 128 × 64, 128 KB at 256 × 128, 512 KB at 512 × 256 (1,024 wide), no NOVRAM in any stage (PVR-161) | MEASURED, 1 console, 1 run |
| Mirror pass, SH-4, street demo | about 41 ms a frame: reflection on against off, frame times MEASURED (PVR-160); the target size changes nothing | DERIVED |
| Mirror pass and screen PVR, street demo | 3.55 to 9.43 ms in the page's columns, from a bug in our sampling (PVR-162) | ESTIMATE |
| Lookup, body | 0.43 ms for 1,623 vertices, one run of an older version | REPORTED (emulator, not evidence). This version is estimated 15-20 % less. |
| Lookup, flat | under the body's | ESTIMATE |
| Mirror pass, CPU | the world pass × the mirrored share | ESTIMATE |
| Mirror pass, PVR | 32 tiles at 256 × 128, 128 tiles at 512 × 256. "15 ms at 512 × 256" in the first client was emulator queue time, not pass time. | OPEN |
| Layer, PVR | under 0.5 ms for a car body | ESTIMATE |

## Recommended limits

- One plane per target. Two planes need two targets and two passes (README:859).
- Start the target at 256 × 128, and go down before anything else (`AGENTS.md`). The mirror pass is the largest cost. This is ADVISED; see [recommended limits](../limits/recommended-limits.md).
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

- [pool](pool.md), [spark](spark.md) (halos in the mirror pass), [guard](guard.md)
- [Video ledger](../hardware/ledger-video.md), [budgets](../limits/budgets.md)
