---
title: "Demo guide: what each program shows"
tags: [start, demos, names]
updated: 2026-11-26
sources: [demo/psm/suite/suite_13.json (title, shows, name, file, elf fields), demo/psm/suite/suite_s6b.json, demo/psm/suite/suite_s7.json, demo/psm/suite/suite_s9.json (what fields, older menu codes), demo/psm/suite/suite_11.json, demo/psm/suite/suite_12.json, wiki/effects/names.md (section 0 and the demo programs table), wiki/methods/suite-13.md, wiki/methods/suite-12.md, wiki/effects/psm.md, wiki/effects/pl4nar.md, wiki/effects/water.md, wiki/effects/ao-bake.md, wiki/effects/aosky.md, wiki/effects/aobake.md, wiki/effects/hexheat.md, wiki/status/maturity.md (ocean and console rows), wiki/hardware/ledger-pvr.md (PVR-108 to PVR-115, PVR-160 to PVR-175), demo/pl4nar/src/ file list, demo/psm/ file list]
---
# Demo guide: what each program shows

**What you will learn:** which program on the Dreamcast disc shows which effect, what each program is called in its menu, and the code name the files and the ledger use for it.
**For:** anyone who wants to run a demo, and anyone who meets a code name such as car-tech, hero1 or square on a page and wants the plain name.

A demo program is a program that runs on the Dreamcast and shows effects on the screen. Each program's menu title is its plain name. The code name is what the suite list file, the disc file names and the ledger call the same program. Each row below gives both, so a code name met on any page can be found here.

Each effect named in a row links to its effect page in the [effect index](../effects/README.md), with the module name in brackets. An effect with no page yet is named without a link.

## The suite 13 disc

The menu titles, and what each program shows, are copied from the suite 13 list (`demo/psm/suite/suite_13.json`). The code name is the `name` field of that list.

| Plain name | What you see | Effects it shows | Link to the code | Code name |
|---|---|---|---|---|
| Room shading, four ways | corner shading: none, baked, ground, texture | [Baked corner shading [aobake]](../effects/aobake.md), [Sky shading [aosky]](../effects/aosky.md) (the ground case) | [demo/psm/ao/ao4_scene.h](../../demo/psm/ao/ao4_scene.h) | ao4 |
| Lamp with a lit shade | light through a thin shade, glow halo | [Light through thin parts [sss]](../effects/sss.md), [Mesh bloom [bloom]](../effects/bloom.md) (the halo) | [demo/psm/ao/lamp_spike.c](../../demo/psm/ao/lamp_spike.c) | ao-lamp |
| Space soldier on a street | running figure, his shadow, hangars, long street | [Sun shadow map [psm]](../effects/psm.md) (his shadow) | [demo/psm/hero/hero_strip.h](../../demo/psm/hero/hero_strip.h) | hero1 |
| Glow that meets a wall | a glow that fades where it meets a wall | No effect page yet: a glow faded by the depth of the scene, see [Depth shading ideas](../research/depth-shading-ideas.md) | [demo/psm/ao/d1_spike.c](../../demo/psm/ao/d1_spike.c) | d1 |
| Hens that see the room | two ways to steer hens, and what each costs | No effect page: ray tests, see [Ray uses beyond shadows](../research/ray-uses.md) | [demo/psm/ao/hen_spike.c](../../demo/psm/ao/hen_spike.c) | hens-eyes |
| Copy video memory to RAM | a fast copy by the graphics chip, checked | PVR extras [pvrx] (no effect page yet) | [demo/psm/ao/pd_spike.c](../../demo/psm/ao/pd_spike.c) | pvr-dma |
| Car in a city by day | reflection, car shadow, corner shading, heat haze | [Planar reflection [pl4nar]](../effects/pl4nar.md); [Sun shadow map [psm]](../effects/psm.md) (the car's shadow tiles); [Bake tool [ao-bake]](../effects/ao-bake.md) (the road's corner shading, baked by the road tool); the heat haze is the street demo's own [heat.h](../../demo/pl4nar/src/heat.h), and the hexagon version [Heat haze hexagons [hexheat]](../effects/hexheat.md) is an option | [demo/pl4nar/src/demo.c](../../demo/pl4nar/src/demo.c) | car-tech |
| Car on hills | car shadow on sloped ground, reflection | [Sun shadow map [psm]](../effects/psm.md); [Planar reflection [pl4nar]](../effects/pl4nar.md) | [demo/pl4nar/src/demo.c](../../demo/pl4nar/src/demo.c) | car-hills |
| Car on a wet night road | rain, wet road, light pools, reflection, grit | [Light pools [pool]](../effects/pool.md); [Planar reflection [pl4nar]](../effects/pl4nar.md); the rain and the grit are the street demo's own (the grit is made by [grit_gen.py](../../demo/pl4nar/tools/grit_gen.py)) | [demo/pl4nar/src/demo.c](../../demo/pl4nar/src/demo.c) | car-wet |
| Drive a car in a city | you drive: reflection, shadow, low camera | [Planar reflection [pl4nar]](../effects/pl4nar.md); [Sun shadow map [psm]](../effects/psm.md) | [demo/pl4nar/src/demo.c](../../demo/pl4nar/src/demo.c) | drive-map |
| Drive a car on hills | you drive: slopes, lights that stay on the car | [Light pools [pool]](../effects/pool.md) | [demo/pl4nar/src/demo.c](../../demo/pl4nar/src/demo.c) | drv-hills |
| Drive a wet night road | you drive: rain, light pools, wet road | [Light pools [pool]](../effects/pool.md); the rain and the wet road are the street demo's own | [demo/pl4nar/src/demo.c](../../demo/pl4nar/src/demo.c) | drv-wet |
| Walled town square | AO, sun shadows, hens, Y+A hides HUD | Corner shading from the demo's own [psm_ao.h](../../demo/psm/psm_ao.h) (not the [Baked corner shading [aobake]](../effects/aobake.md) module); [Sun shadow map [psm]](../effects/psm.md) | [demo/psm/tools/bake_square.py](../../demo/psm/tools/bake_square.py) (the square's build) and [demo/psm/psm_ao.h](../../demo/psm/psm_ao.h) | square |

**Status:** nothing on suite 13 has run on a console yet. Each row says what the program should show; read [Suite 13](../methods/suite-13.md) for what to film and which ledger row each page goes to.

## Other demo programs

These programs are not in the suite 13 list. Their names and descriptions are from the [names](../effects/names.md) page, where the program has a line there.

| Plain name | What you see | Effects it shows | Link to the code | Code name |
|---|---|---|---|---|
| Street car demo | A night street drive showing planar reflection on a car body, light pools, halos and shadow tiles | [Planar reflection [pl4nar]](../effects/pl4nar.md); [Light pools [pool]](../effects/pool.md); [Sparks and tracers [spark]](../effects/spark.md) (the halos); [Sun shadow map [psm]](../effects/psm.md) (the shadow tiles) | [demo/pl4nar/src/demo.c](../../demo/pl4nar/src/demo.c); built by [demo/pl4nar/build.py](../../demo/pl4nar/build.py) | pl4nar (builds pl4m c5 and later) |
| Match Light | A scripted 120-second film of a snowy night street: a dent grid, a mist bank, rising lights, spark pools and motes | [Dented ground [deform]](../effects/deform.md); [Mist bank [mist]](../effects/mist.md); [Rising lights [rise]](../effects/rise.md); [Sparks and tracers [spark]](../effects/spark.md); [Drifting specks [motes]](../effects/motes.md) | [demo/matchlight/matchlight_scene.h](../../demo/matchlight/matchlight_scene.h) | matchlight |
| Sunset sea | The wave surface, crest transmission, sky reflection and soft sun shadow, with live rows | [Water waves [water]](../effects/water.md); [Light through thin parts [sss]](../effects/sss.md) (crest transmission) | [demo/ocean.c](../../demo/ocean.c) and [demo/ocean_scene.h](../../demo/ocean_scene.h) (see [demo/OCEAN.md](../../demo/OCEAN.md)) | ocean |
| Torch walk | A first-person walk over a dark floor: a normal map lit by the screen spotlight and a fired point light, with a cost label | [Normal map [bump]](../effects/bump.md); [Screen spotlight [torch]](../effects/torch.md) | [demo/torchwalk_ab.c](../../demo/torchwalk_ab.c) | torchwalk-ab |
| Demo disc | Six screen effects as live rows (exposure and grade, screen bloom, spotlight, dark rays, TAA, chromatic aberration) plus SH4DOW-DC | [Exposure and grade [look]](../effects/look.md); [Screen bloom [glow]](../effects/glow.md); [Screen spotlight [torch]](../effects/torch.md); [Dark shafts of light [shafts]](../effects/shafts.md); [Smooth edges over time [taa]](../effects/taa.md); [Colour fringe [ca]](../effects/ca.md); [Soft character shadows [sh4dow-dc]](../effects/sh4dow-dc.md) | [demo/main.c](../../demo/main.c) | demo (make demo) |
| Town shadow demo | The old console town of the shadow tests, with sun shadows, corner shading and hens; suite 13 replaces it with the walled town square | [Sun shadow map [psm]](../effects/psm.md): the shadow route on the SH-4 (psm_raster) and the route the PVR draws (psm_pvr), which have no pages of their own | [demo/psm/](../../demo/psm/) | town (the town-* tests in the older suites) |
| Walled square build | Not a screen program: a host script that builds the walled square's set (walls from quads, pieces from the kit) | none; it writes the set that the walled town square program uses | [demo/psm/tools/bake_square.py](../../demo/psm/tools/bake_square.py) | square (build) |
| Software shadow demo | Pictures from a host program: the same scene with a uniform map and a perspective shadow map, at one map size | [Sun shadow map [psm]](../effects/psm.md); Shadow map on the SH-4 [psm_raster] (no page of its own) | [demo/psm/psm_demo.c](../../demo/psm/psm_demo.c) | psm-demo |

**Status, from the wiki:**

- Street car demo: ran on a console in its c5 and c7 builds (PVR-160 to PVR-175 in the [PVR ledger](../hardware/ledger-pvr.md)).
- Match Light: no console row in the wiki. NOT RUN on a console as far as the wiki records.
- Sunset sea: its page reports that it runs on a Dreamcast (REPORTED, no ledger row; `demo/OCEAN.md`).
- Torch walk: no tests by design, and no console row in the wiki. NOT RUN on a console as far as the wiki records.
- Demo disc: the effect pages say these rows run on the demo disc (v0.50); there is no ledger row for them.
- Town shadow demo: ran on a console (PVR-108 to PVR-115); replaced in suite 13 by the walled town square.
- Walled square build: a host script, nothing to run on screen.
- Software shadow demo: a host program. NOT RUN on a console.

## Older suites

Suite 12 ([wiki/methods/suite-12.md](../methods/suite-12.md)) was superseded by suite 13 before it ran, and is kept for the record. Suites 1 to 11 have their lists in `demo/psm/suite/` (`suite_s1.json` to `suite_s10.json`, and `suite_11.json`). Their results are in the ledgers: [PVR ledger](../hardware/ledger-pvr.md) and [SH-4 ledger](../hardware/ledger-sh4.md). The older lists have no menu titles, so the older code names in those ledgers have plain names only on the ledger pages.

## Related

- [What do you want to make?](what-do-you-want.md)
- [Effect index](../effects/README.md)
- [Suite 13](../methods/suite-13.md)
- [Demo readability](../limits/demo-readability.md)
- [Names](../effects/names.md)
