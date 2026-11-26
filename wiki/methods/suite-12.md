---
title: "Suite 12: what each program tests and what to read"
tags: [method, suite, run, ledger]
updated: 2026-11-25
sources: [demo/psm/suite/suite_12.json, demo/pl4nar/src/main.c, demo/psm/hero/hero_spike.c, demo/psm/hero/hero_strip.h, demo/psm/suite/suite_menu.c, demo/pl4nar/src/heat.h]
---
# Suite 12: what each program tests and what to read

**What you will learn:** what each program on the suite 12 disc shows, what to film or photograph, which lines on its pages matter, and which ledger row each page goes to.
**For:** the owner, running the disc; and Scholar, filing the run.

> **Superseded by [suite 13](suite-13.md) before it ran.** Kept for the record; read suite 13 instead.

The disc is `sh4der-pvr-suite12` (staging 7d40338). **Nothing on it has run on a console.** The titles below are the menu's own (`demo/psm/suite/suite_12.json`).

## Before you start

- One video of the whole run is enough. The pages are read from it; each page has a CRC line at its foot that proves it was read right.
- **Do not press keys during an autorun.** A key press marks the stage TOUCHED, and a touched stage is not compared. B with the stick changes nothing.
- A page that says **"THE RUN DID NOT FINISH"** is still worth filming: the lines above it are of the stages that ran.
- To go back to the menu from the result pages: hold A + B + X + Y.
- **RUN ALL waits in the Walled town square [square]: press A + B + X + Y to go on.** The Walled town square is live, with no pages, and leaves only on those four keys (suite 13 takes it out of RUN ALL).
- If the serial cable is on, keep the log: the menu writes its redraw times there and nowhere else.

## The programs

| Menu title | What it shows | What to do | What matters on the pages | Ledger row (proposed) |
|---|---|---|---|---|
| **Car in a city by day** [car-tech] | reflection, car shadow, corner shading, heat haze | Let it run (11 stages, about 79 s), film every page | Stage 1 against stage 11: the same view, stage 11 with the heat haze on STRONG. The line "stage 11 = stage 1 + heat STRONG: last copy N us" is the heat's own copy time. "shadow tiles a stage, most seen" | PVR-212 (stages 1 to 10), PVR-213 (the heat) |
| **Car on hills** [car-hills] | car shadow on sloped ground, reflection | Let it run (11 stages, about 79 s), film every page | Stages 1 to 3: the shadow on the ground mesh, as one flat quad, and none. Stages 8 to 10 and the "tile cut" line: passes, refused, FAILED, tables. The cut now refuses unless both tables are found; this is its first console run | PVR-214 |
| **Car on a wet night road** [car-wet] | rain, wet road, light pools, reflection, grit | Let it run (6 stages, about 44 s), film every page; one still of the road in the wet stage | The `look` column: "wet 128x64" against "dry 128x64" is the pool and the grit's SH-4 time. "off (no layer)" against "wet 128x64": the whole wet layer. From the still: does the pool fade to nothing with no water line, and does the grit show through the wet? | PVR-215 (the pool), PVR-216 (the grit and the rain levels) |
| **Drive a car in a city** [drive-map], **Drive a car on hills** [drv-hills], **Drive a wet night road** [drv-wet] | you drive | Drive; a short clip of each is enough | No pages: the look only. The heat haze (X steps OFF, LIGHT, STRONG, by day) has a label with its copy time: a photo of that label is a number | PVR-219 (the look, and the heat label if seen) |
| **Space soldier on a street** [hero1] | running figure, his shadow, hangars, long street | Let it run (5 stages, about 45 s), film every page | The stage table (fr, SH4, PVR, TA). Stage 2's parts (anim, skin, sends, the set). The lines "set: N triangles sent, N cut at the near plane, N of N objects left out". In the video: he and his shadow are never hidden | PVR-217 |
| **Walled town square** | built from kit pieces, sun shadows, hens | Walk it; photograph the stalls, the fountain, the gate banners; a few photos of the HUD (written every 20 frames). In RUN ALL it waits here: A + B + X + Y goes on | No pages. The HUD's TA line (est, used, max against the 1,536 KB buffer), SH-4, PVR, TA vertices, FPS. Any freeze: the video time of the last new picture | PVR-218 |
| **Room shading, four ways** [ao4] | corner shading: none, baked, ground, texture | Let it run (110 s), film every page | As before (PVR-176, PVR-205): a repeat | the next free PVR row |
| **Lamp with a lit shade** [ao-lamp] | light through a thin shade, glow halo | Let it run; film the pages | As before (PVR-158): a repeat | the next free PVR row |
| **Glow that meets a wall** [d1] | a glow that fades where it meets a wall | Film it all, the pages after | As before (PVR-204, PVR-205): a repeat | the next free PVR row |
| **Hens that see the room** [hens-eyes] | two ways to steer hens, and what each costs | Let it run; film every page | As before (SH4-080, SH4-085): a repeat | SH4-087 |
| **Copy video memory to RAM** [pvr-dma] | a fast copy by the graphics chip, checked | Film the page and the graph | MB/s, and every copy right: a repeat (BUS-053) | BUS-054 |
| **The menu itself** | the new skinned menu with plain titles | Film the menu as you move through it | That only changed lines are redrawn: no full-screen flash on a key press. The serial log's lines "suite: redraw lines y0 .. y1, N us" give the time | VID-017 |

## The rows, ready for the run

Each row is filed only from the video and the pages, with the video time (mm:ss.s) as the photo name. The IDs are proposed; if another row takes one first, the next free one is used. What each will hold:

| Row | Claim to fill | Status it can reach | Its numbers come from |
|---|---|---|---|
| PVR-212 | Car in a city by day stages 1 to 10: frame, SH4, look, PVRs, PVRm, TA, tiles seen; the repeat of the map's earlier runs (PVR-174, PVR-175) | MEASURED (one run) | the stage table pages |
| PVR-213 | The heat haze's cost: stage 11 less stage 1 (frame, SH4, PVRs), and the copy's own time | MEASURED stage means; the difference DERIVED; the copy time MEASURED | the stage table; the "last copy" line |
| PVR-214 | Car on hills: the three shadow modes; the tile cut with the two-table refusal (first console run) | MEASURED (one run); differences DERIVED | the stage table; the tile cut lines |
| PVR-215 | The soft pool: the look column wet against dry; the whole layer, wet against off; the look of the edge | MEASURED stage means; differences DERIVED; the edge by eye | The table of Car on a wet night road; a still |
| PVR-216 | The grit and the rain levels: wet, damp, dry; the grit's cost stays not isolated | MEASURED stage means; the grit's own cost OPEN | The table of Car on a wet night road; a still |
| PVR-217 | The space street hero: each stage, stage 2's parts, the set's triangles and cuts; against Space soldier on a street h1f (PVR-178) | MEASURED (one run) | The pages of Space soldier on a street |
| PVR-218 | The Walled town square: the HUD's TA, SH-4, PVR and FPS at the photographed views | MEASURED (HUD photos, one run) | HUD photos |
| PVR-219 | The live drives: the look; the heat label's copy time if photographed | MEASURED (the look, or a label) | clips |
| VID-017 | The menu's redraw: lines changed only, and their time | MEASURED from the serial log; the look from the video | serial log; video |
| SH4-087, BUS-054 | Repeats of Hens that see the room and Copy video memory to RAM | MEASURED (one run) | their pages |

## Related

- [Frame budget, section 6](../limits/frame-budget.md#6-the-car-demo-practices-so-far): the car practices these runs would prove
- [Maturity: not run yet](../status/maturity.md#not-run-yet-s11-s12-and-s13)
- [Open questions](../open-questions.md): Q103 to Q106
- [Benchmarking](benchmarking.md)
