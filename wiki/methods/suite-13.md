---
title: "Suite 13: what each program tests and what to read"
tags: [method, suite, run, ledger]
updated: 2026-11-25
sources: [demo/psm/suite/suite_13.json, demo/pl4nar/src/main.c, demo/psm/hero/hero_strip.h, demo/psm/psm_ao.h, demo/psm/suite/suite_menu.c]
---
# Suite 13: what each program tests and what to read

**What you will learn:** what each program on the suite 13 disc should show, what to film or photograph, which lines on its pages matter, and which ledger row each page goes to.
**For:** the owner, running the disc; and Scholar, filing the run.

This page follows the committed list (`demo/psm/suite/suite_13.json`, the car ELFs tagged tech-map-12, tech-hills-13, wet-14, drive-map-15, drive-hills-16, live-wet-17). It replaces [suite 12](suite-12.md), which never ran. **Nothing on it has run on a console yet:** every claim below is what the code should do (ESTIMATE), and every part is PROTOTYPE until a run.

**On the disc:** the heat haze's version 2 and a fly-by stage in each day autorun (car-tech stage 2, car-hills stage 4). The heat is drawn only while the camera stands still: in the fly-by it comes up 0.5 s into a static kerb shot, is faded out by 5.7 s, and the camera moves only after that (host: `demo/pl4nar/host/snap_test.c`). The wet road has no fly-by and no heat.

## Before you start

- One video of the whole run is enough. Each page has a CRC line at its foot that proves it was read right.
- **Do not press keys during an autorun.** A key press marks the stage TOUCHED, and a touched stage is not compared.
- A page that says **"THE RUN DID NOT FINISH"** is still worth filming.
- To go back to the menu from result pages: hold A + B + X + Y.
- RUN ALL no longer stops in the square: the square is out of it. Start it from the list.
- **Hiding the HUD:** B + Y in the car programs (START brings it back), Y + A in the square.
- If the serial cable is on, keep the log: the menu writes its redraw times there and nowhere else.

## The programs

| Menu title | What it should show | What to do | What matters on the pages | Ledger row (proposed) |
|---|---|---|---|---|
| **Car in a city by day** (car-tech) | Reflection and the car's mesh shadow in every stage; corner shading (AO); no jump of the car or camera | Let it run (8 stages, about 56 s), film every page | Stage table: 1 day 256 × 128, 2 the fly-by (heat on STRONG while the camera stands), 3 no AO, 4 to 6 night at 256, 128 and 512, 7 orbit, 8 buildings boxed past 30 m. Stage 1 against 3 is the AO's cost; stage 2 against 1 holds the heat's cost, mixed with the fly-by's different view. In the video: does the heat ever show while the camera moves? "shadow tiles a stage, most seen" | PVR-212 |
| **Car on hills** (car-hills) | The mesh shadow on sloped ground in every stage; reflection | Let it run (9 stages, about 63 s), film every page | Stages 1 to 3: the sizes; 4: the fly-by (heat while the camera stands); 5: night; 6 to 8: t8 uncut, t8 cut, t4 cut; 9: the car stands while traffic crosses the fade band. The "tile cut" line (passes, refused, FAILED, tables): the cut now refuses unless both tables are found, and this is its first console run | PVR-213 |
| **Car on a wet night road** (car-wet) | Rain, the soft reflection pool round the car (no water line anywhere), grit showing through the wet, light pools | Let it run (5 stages, about 35 s), film every page; one still of the road in the wet stage | The `look` column: "wet 128x64" against "dry 128x64" is the pool and the grit's SH-4 time. Stages 4 and 5: the target at 256 × 128 and 64 × 32. No stage turns the layer or the grit off, so neither cost is isolated | PVR-214 |
| **Drive a car in a city / on hills / a wet night road** (drive-map, drv-hills, drv-wet) | You drive; lights stay on the car on slopes; Y rain on the wet road | Drive; a short clip of each. Try B + Y and START | No pages: the look only. Does the car or camera snap? (Q107.) The heat starts OFF; X steps it, with a label giving its copy time: a photo of that label is a number. It is drawn only while the camera is still (else the HUD says "heat: waiting for a still camera"); B + X starts the fly-by on the map and the hills | PVR-215 |
| **Space soldier on a street** (hero1) | The running figure, his shadow, hangars, a long street, three hens with blob shadows | Let it run (5 stages, about 35 s), film every page | The stage table (fr, SH4, PVR, TA). Stage 2's parts (anim, skin, sends, the set). "set: N triangles sent, N cut at the near plane, N of N objects left out". In the video: he and his shadow are never hidden | PVR-216 |
| **Walled town square** (square) | Kit-piece walls darkened at the foot by AO baked at load, sun shadows, hens | Start it from the list. Walk it; photograph the stalls, the fountain, the gate banners; HUD photos (written every 20 frames). Turn the AO row off and on at one view, a photo each. Y + A hides the HUD | No pages. The AO row (the bake's time at load); the HUD's TA line (est, used, max against the 1,536 KB buffer), SH-4, PVR, TA vertices, FPS. Any freeze: the video time of the last new picture | PVR-217 |
| **Room shading, four ways** (ao4) | Corner shading: none, baked, ground, texture | Let it run (110 s), film every page | A repeat (PVR-176, PVR-205) | the next free PVR row |
| **Lamp with a lit shade** (ao-lamp) | Light through a thin shade, a glow halo | Let it run; film the pages | A repeat (PVR-158) | the next free PVR row |
| **Glow that meets a wall** (d1) | A glow that fades where it meets a wall | Film it all, the pages after | A repeat (PVR-204, PVR-205) | the next free PVR row |
| **Hens that see the room** (hens-eyes) | Two ways to steer hens, and what each costs | Let it run; film every page | A repeat (SH4-080, SH4-085) | SH4-087 |
| **Copy video memory to RAM** (pvr-dma) | A fast copy by the graphics chip, checked | Film the page and the graph | MB/s, every copy right: a repeat (BUS-053) | BUS-054 |
| **The menu itself** | The skinned menu, plain titles | Film the menu as you move through it | Only changed lines redrawn: no full-screen flash on a key press. The serial log's lines "suite: redraw lines y0 .. y1, N us" give the time | VID-017 |

## The rows, ready for the run

Each row is filed only from the video and the pages, with the video time (mm:ss.s) as the photo name. The IDs are proposed; if another row takes one first, the next free one is used. Suite 12's proposed IDs were never used, so they are reused here.

| Row | Claim to fill | Status it can reach | Its numbers come from |
|---|---|---|---|
| PVR-212 | car-tech: frame, SH4, look, PVRs, PVRm, TA, tiles seen in each of 8 stages; the AO's cost (stage 1 against 3); the fly-by stage 2 with the heat up (first console run of heat version 2 and of the still-camera gate); against the map's earlier runs (PVR-174, PVR-175) | MEASURED (one run); differences DERIVED | the stage table pages |
| PVR-213 | car-hills: the sizes with the mesh shadow; the fly-by stage 4 with the heat up; the tile cut with the two-table refusal (first console run); the fade band untouched | MEASURED (one run); differences DERIVED | the stage table; the tile cut lines |
| PVR-214 | car-wet: the pool and grit's SH-4 (look, wet against dry); the target sizes; the pool's edge and the grit by eye | MEASURED stage means; differences DERIVED; the look by eye; the grit's own cost OPEN | car-wet's table; a still |
| PVR-215 | The live drives: the look; no snap seen (Q107); the heat label's copy time if photographed | MEASURED (the look, or a label) | clips |
| PVR-216 | The space-street hero with hens: each stage, stage 2's parts, the set's triangles and cuts; against hero1 h1f (PVR-178) | MEASURED (one run) | hero1's pages |
| PVR-217 | The walled square: the AO bake's time, the look with AO on and off, the HUD's TA, SH-4, PVR and FPS at the photographed views | MEASURED (HUD photos, one run) | HUD and AO row photos |
| VID-017 | The menu's redraw: changed lines only, and their time | MEASURED from the serial log; the look from the video | serial log; video |
| SH4-087, BUS-054 | hens-eyes and pvr-dma repeats | MEASURED (one run) | their pages |

## Related

- [Suite 12](suite-12.md): superseded before it ran
- [Frame budget, section 6](../limits/frame-budget.md#6-the-car-demo-practices-so-far): the car practices these runs would prove
- [Maturity: not run yet](../status/maturity.md#not-run-yet-s11-s12-and-s13)
- [Open questions](../open-questions.md): Q103 to Q108
- [Heat haze reference](../research/heat-haze-reference.md)
