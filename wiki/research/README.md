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
| `demo/psm/compare/NOTES.md` | 2026-11-25 | Spikes c1, c3b and c4 on a console: the c4 design, its two runs (live view and tile stage; all six result pages) and what the console said against the model. |
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

Pages written here (research ideas, nothing built):

| Page | Date | What it covers |
|---|---|---|
| [Depth shading ideas](depth-shading-ideas.md) | 2026-11-25 | Material inputs mapped onto the PVR; where a PVR depth compare could fill a gap; most-useful and one-spike ideas. All ESTIMATE or OPEN. |
| [Depth fog and glow ideas](depth-fog-ideas.md) | 2026-11-25 | Film and TV depth fog and light; what table fog already does; fog between c4's bands, and its cost against the mask. All ESTIMATE or OPEN. |

Outside sources: [skmp's repositories](outside-sources.md).
