---
title: Research notes
tags: [research]
updated: 2026-11-25
sources: [demo/psm/*.md]
---
# Research notes

**What you will learn:** what each research note in `demo/psm/` is for and what it settled, so you can find the right one.
**For:** anyone continuing the research, and agents that need the full write-up behind a ledger row.

The notes stay where they are; this page indexes them. Measured numbers from them are copied into the [hardware ledger](../hardware/README.md).

| Note | Date | What it settled |
|---|---|---|
| `demo/psm/bench/NOTES.md` | 2026-11-25 | Chip benches h1 and h2: cache, transforms, copies, TA send, uploads, PVR fill, DMA, G2. h2 corrects h1's buffer placing. |
| `demo/psm/compare/NOTES.md` | 2026-11-25 | Spikes c1, c3b, c4 and c4b on a console: the c4 design, its two runs (live view and tile stage; all six result pages) and what the console said against the model; c4b's measured window edges, placing fix, frame waits and punch-through mask route (`:413-463`). |
| `demo/psm/bench/RAY1_RESULTS_cortana.md`, `RAY1_RESULTS_junior.md` | 2026-11-25 | The ray bench ray1 read from its photos: the thesis's P1 overturned, P2 mixed, P9 stands; the transform with 1/w; OC-RAM; the clock lines flagged. Ledger rows SH4-051 to SH4-055. |
| `demo/psm/bench/RAY2_RESULTS_cortana.md`, `RAY2_RESULTS_junior.md` | 2026-11-25 | The ray bench ray2 read from its photos: ray1 repeated (P1 line stands, P2 cold start 47 us again, P9 137.80, h3, OC-RAM verdict), the KOS ns clock's 2.5 ms reload step confirmed, the ray-triangle test (33.16 prepared, 62.49 any direction). Ledger rows SH4-056 to SH4-066. |
| `demo/psm/ROUTE_AB_NOTES.md` | 2026-11-24 | Route AB feasibility: PVR-DMA download, exact codes, held pixels, dither. |
| `demo/psm/PVR_FEATURES.md` | — | Index of per-pixel PVR operations, ranked compare ideas, Sega's bug list, KOS issues. |
| `demo/psm/POLLY2_NOTES.md` | 2026-11-25 | Rules read from skmp's PowerVR2 model and console tests (cited, nothing copied); c4 checked against them. |
| `demo/psm/ARM7_TOOLCHAIN_NOTES.md` | 2026-11-25 | The sound CPU is ARMv3; a modern GCC cannot target it; how KOS builds for it. |
| `demo/psm/OFFLOAD_PLAN.md` | 2026-11-25 | Which idle chips could take work. Its sound RAM rates are Sega's and about twice too hopeful (AICA-024). |
| `demo/psm/SHZ_TRIG_PROBE.md` | 2026-11-25 | Host accuracy of SH4ZAM's trig; written for an upstream report. |
| `demo/psm/THESIS_SH4_SHADOWS.md` | 2026-11-25 draft | Cached per-vertex sun visibility. See [the thesis](../shadows/thesis.md). |
| `demo/psm/GPU_GEMS_NOTES.md` | 2026-11-25 | Reading list of 17 GPU Gems chapters and what each would need here. |
| `demo/psm/PRIOR_ART.md` | — | What earlier consoles and games are documented to have done for shadows. Safe wording for claims. |
| `demo/psm/LATEGEN_NOTES.md` | — | What makes a late-generation look on this class of hardware: baked vertex light, table fog, detail textures. |
| `demo/psm/TOWN_STORY.md`, `TOWN_MOCKUP.md`, `TOWN_LOOK.md` | — | The town scene: setting, layout, budgets, mockup findings, look targets. See [scenes](../limits/scenes.md). |
| `demo/psm/DC_MOCKUP.md` | — | The field scene's disc layout, cost label and controls. |
| `demo/psm/CREDITS.md` | — | Credits for CC0 art and the technique sources. |
| `demo/psm/RECIPES_NOTES.md` | 2026-11-25 | Pass recipes for crude programmable shading from the PVR's fixed units: settings, SH-4 maths, evidence and a host test for each. Builds on the two idea pages below. Nothing built or run. |
| `demo/psm/BUMP_REVIEW_NOTES.md` | 2026-11-25 | Could `sh4der_bump.h` use c3b and c4's measured level table? Not directly: since v0.14 that header does not use the bump unit. |
| `demo/psm/town_ray_data.h` (made by `demo/psm/tools/town_ray_data.c`) | 2026-11-25 | Generated data for the thesis's ray bench (P1, P2, P9): the static town's 64 × 64 height grid of 1.5 m cells, block maxima, overhangs, and the twelve movers with their shadow runs at 14:00 and 16:00. Host tool; changes nothing in the town. |

Pages written here (research ideas, nothing built):

| Page | Date | What it covers |
|---|---|---|
| [Depth shading ideas](depth-shading-ideas.md) | 2026-11-25 | Material inputs mapped onto the PVR; where a PVR depth compare could fill a gap; most-useful and one-spike ideas. All ESTIMATE or OPEN. |
| [The sound chip as a precompute unit](sound-chip-precompute.md) | 2026-11-25 | The AICA manual read against the offload plan: the ARM's timers, the DSP's rules, the voices; nine ideas with verdicts. The manual confirms the plan: only rare, small work pays across G2. ESTIMATE / OPEN. |
| [Tricks from after and around the PVR](post-pvr-tricks.md) | 2026-11-25 | 21 GPU techniques of 2003-2010 and 18 fixed-function console tricks, each mapped to the PVR with ledger costs and a host test; the PVR pieces cost key; most useful and craziest. ESTIMATE / OPEN. |
| [Ray uses beyond shadows](ray-uses.md) | 2026-11-25 | What the town's grid rays can do besides sun shadows: occluded sound, a reverb probe, AI sight, camera collision and picking, projectiles, culling, ground probes, wind shelter. Rays a frame, cycles from SH4-052, SH4-053 and SH4-055, the sound chip side, a test each; a game frame of them adds up to about 0.25 ms. DERIVED / ESTIMATE. |
| [GPU Gems depth techniques](gems-depth-techniques.md) | 2026-11-25 | 33 chapters of the three GPU Gems books that use depth, by category, each mapped to the PVR pieces with ledger costs and a host test; what the PVR already does in hardware (sorted translucency, hidden-surface removal, table fog); six ideas J1-J6. ESTIMATE / OPEN. |
| [Depth fog and glow ideas](depth-fog-ideas.md) | 2026-11-25 | Film and TV depth fog and light; what table fog already does; fog between c4's bands, and its cost against the mask. All ESTIMATE or OPEN. |

Outside sources: [skmp's repositories](outside-sources.md).
