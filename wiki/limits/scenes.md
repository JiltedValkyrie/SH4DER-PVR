---
title: Test scenes and what they measured
tags: [limits, scenes, shadows]
updated: 2026-11-25
sources: [demo/psm/TOWN_STORY.md, demo/psm/TOWN_MOCKUP.md, demo/psm/DC_MOCKUP.md, demo/psm/psm_scene.h, demo/OCEAN.md]
---
# Test scenes and what they measured

**What you will learn:** which demo scenes exist, what each is for, and every console number read from them so far.
**For:** anyone comparing their own scene's cost with ours, and anyone running a demo disc.

## The town (shadow demo)

An invented market square in an English town of about 1420, at 52.2° N, on a September afternoon (`demo/psm/TOWN_STORY.md`). The art is a CC0 modular village kit with 22 flat colours and no textures; the sky is a CC0 HDR (`demo/psm/CREDITS.md`).

- **Layout:** an open square 24 m across, a hall to the north, a tavern to the west, four stalls, a well, a market cross. A church tower 6 × 6 × 24 m stands to the south-west. The ground is a 1.5 m cobble grid (`TOWN_MOCKUP.md:64-79`).
- **People:** up to 8 people and 4 hens, as grey placeholder blocks.
- **Budget (targets, not measured):** about 13,900 triangles for the whole scene; 8,000-12,000 on screen; 1,500-2,500 shadow casters (`TOWN_STORY.md:190-221`).
- **Controls:** three HUD tabs (SHADOWS, SUN & TIME, VIEW), six presets, routes A, B and AB, map sizes 64 to 512, draw distance 5 m to infinite (`demo/psm/psm_scene.h:107-124, 585-598`).

### Console numbers

Eight HUD photos, one console, 2026-11-25: `logs/benchmarks/older/PXL_20261125_070114152.jpg` to `..070131888.jpg`. All show the same view: camera x 0.7, z −3.5, heading 50°, pitch −17°, 14:18 at ×1, sun 219°/25°, map 256 × 256, draw distance INF, all casters, 8 people and 4 hens. No build tag is visible on screen.

| Route | Map | SH-4 ms | PVR ms | TA vertices | fps | Triangles drawn |
|---|---|---|---|---|---|---|
| A (PVR draws the map) | uniform | 13.3-13.6 | 15.9 | 27,031-27,075 | 18 | 1,051-1,055 of 1,162 |
| A | PSM | 13.5-13.6 | 15.9 | 27,023-27,075 | 18 | 960-961 of 1,162 |
| B (SH-4 depth) | uniform | 40.2 | 7.1 | 15,142 | 14 | 1,069 of 1,162 |
| B | PSM | 43.9 | 7.1 | 15,142 | 13 | 869 of 1,162 |
| AB stub (SH-4 fill, 1 frame late) | uniform | 87.3 | 7.2 | 15,142 | 8 | 1,058 of 1,162 |
| AB stub | PSM | 87.9 | 7.0 | 15,142 | 8 | 840 of 1,162 |

All MEASURED as the HUD printed them, one run.

What the table shows:

- **Route A's PVR time (15.9 ms) is about 8.8 ms above route B's (7.1 ms).** That is the map pass plus the full-screen shadow layer. The layer alone is about 3.1 ms (PVR-002); the rest is OPEN.
- **Route B's SH-4 time (40-44 ms) does not fit 30 fps** in this view.
- **The AB stub is not route AB.** It fills the map on the SH-4 in place of the PVR, so its 87 ms says nothing about the real route.
- **Route A runs at 18 fps although neither side is near 55 ms.** Where the frame goes is OPEN.

![Where the 16.7 ms goes in the town (route A, one view, one console): the SH-4 takes 13.3 to 13.6 ms and the PVR 15.9 ms (MEASURED, HUD), both under 16.7 ms, yet the frame runs at 18 fps, about 55.6 ms; the PVR splits into the scene 7.1 ms (route B, MEASURED), a map pass about 3.2 ms and a shadow layer about 3.1 ms (ESTIMATE) and 2.5 ms OPEN; the SH-4's transform and send are about 5.6 to 6.7 ms (ESTIMATE from unit costs) and the rest is not broken down; no G2 work is in the frame; the wait that makes the frame long is OPEN (Q53, P10).](../images/frame-budget-town.svg)

Shadow texels a pixel at 4 / 10 / 25 / 60 m, from the same photos:

- route A uniform: 24.8 / 7.0 / 1.8 / 0.5; route A PSM: 13.6 / 5.2 / 2.4 / 1.5;
- route B uniform: 29.1 / 8.2 / 2.2 / 0.6; route B PSM: 3.3 / 2.2 / 1.8 / 1.6.

The PSM spreads the map's texels much more evenly with distance.

The TA line of the same photos is explained in [VID-011](../hardware/ledger-video.md).

**Earlier figure, MEASURED by relay (photo name OPEN):** build a3, another view (camera x −1.7, z −11.8, heading 337°), route A, map 256: 28,699 TA vertices, SH-4 22.6 ms, PVR 16.0 ms, 25 fps (`demo/psm/TOWN_NOTES.md` TOWN-001; `THESIS_SH4_SHADOWS.md:45-65`). The owner's HUD photo was relayed without a file name (errata O5).


**Band counts for c4 on the town (ESTIMATE, host; `demo/psm/TOWN_NOTES.md` TOWN-051 to TOWN-055).** No console has run c4 on the town. A host counter says the whole static town would need 44,577 / 26,969 / 18,097 band vertices at bands 1 / 2 / 4 codes wide, the boot view 11,146 / 7,090 / 5,174, and only the movers' ground patches 864 / 460 / 260, against 2,464 / 1,288 / 692 in c4's measured scene (PVR-100). Scaled by c4's measured mask passes, the visible static bands alone would take 12 to 33 ms (TOWN-053, ESTIMATE). The notes' recommendation: bake static on static, and run c4 for the movers only (TOWN-055; the [T4 idea](../shadows/bump-compare-chain.md#idea-a-cached-static-map-plus-small-mover-maps-t4-estimate--open)).

![Band counts under c4's rules: the whole static town needs 44,577 / 26,969 / 18,097 band vertices at bands 1 / 2 / 4 codes wide, the boot view 11,146 / 7,090 / 5,174, and the movers' patches only 864 / 460 / 260, against 2,464 / 1,288 / 692 in c4's measured scene; the town counts and every cost on top are host ESTIMATEs (TOWN-051 to 055), which is why static shadow goes to a bake and c4 runs for movers only.](../images/town-band-counts.svg)

![The static bake split (ESTIMATE, host, TOWN-060 to 067): static casters on static receivers are baked once a sun at load (a byte a vertex for the models, a 256 x 256 ground lightmap; about 169,000 ray-triangle tests, about 0.04 s at an assumed 50 cycles a test; 8,697 B + 64 KB, sizes FACT from the counts) and cost nothing a frame, while the movers' shadows go through c4 on 12 ground patches (mask pass about 1.1 to 4.1 ms); the share of model vertices in shadow moves with where the ray starts (14:00: 41.6 % from 0.05 m out against 28.5 % from 0.3 m + 0.5 m, screen mesh as casters), which is OPEN.](../images/static-bake-split.svg)

Other town findings are kept in `demo/psm/TOWN_NOTES.md` in ledger form: video memory free after load (VID-012), one crash with no message while turning (TOWN-004), and a swimming shadow map at 64 × 64 (TOWN-006).

## The field (first shadow scene)

A graded polar ground grid, ten poles 4 to 44 m ahead, a 1 m checker ground and two CC0 "Spot" cows cut to about 400 triangles each (`demo/psm/DC_MOCKUP.md:119-126`; `psm_scene.h:1-35`). Eye 1.7 m, 60° vertical field of view. Used for host pictures (`make psm-demo`). No console numbers are filed for it.

## The ocean (water demo)

A sunset ocean, its own program (`make ocean`). Rows switch swell, ripple, transmission, reflection, sh4dow-dc and bloom live. The water mesh runs from 400 to 23,040 triangles (`demo/OCEAN.md`).

- **Status:** it runs on a real Dreamcast (`demo/OCEAN.md:5`, 2026-11-18). A stable 33.3 ms has **not** been measured.
- **QUALITY preset:** every row on, 23,040 water triangles (11,737 vertices).
- **PERFORMANT preset:** sh4dow-dc off, 2,016 triangles (1,073 vertices). Bloom is the first row to turn off if a frame is still over 33.3 ms.
- **TA data per frame:** ESTIMATE only (`OCEAN.md:180-188`), from 15 KB plus 77 KB of sky at 390 triangles to 885 KB plus 77 KB at 23,040.
- **Scene update cost:** host plain-C ratios only, not console times.

## The street (planar reflection demo)

`demo/pl4nar`: a car on a street with mirror pass, reflection layer, light pools and halos. Its cost table (`demo/pl4nar/AGENT_WORKFLOW.md:60-71`) has no console numbers yet.

## Related

- [Budgets](budgets.md)
- [Recommended limits](recommended-limits.md)
- [PSM routes](../shadows/psm-routes.md)
