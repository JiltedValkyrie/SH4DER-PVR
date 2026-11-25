---
title: Index
tags: [index]
updated: 2026-11-25
sources: [every page's front matter]
---
# Index

**What you will learn:** every page in one line, for search and for agents.
**For:** agents, and anyone looking for a page by name or tag.

Generated from each page's front matter. Format: `path | title | tags | updated | summary`. `llms.txt` is the short guide for agents.

## top level

- [README.md](README.md) | SH4DER-PVR wiki | start | 2026-11-25 | what this wiki holds, how to read a page, and where to start.
- [STRUCTURE.md](STRUCTURE.md) | Wiki structure | meta | 2026-11-25 | how the wiki is laid out and kept: the tree, the page template, the ledger row, the status legend, the rules.
- [glossary.md](glossary.md) | Glossary | glossary | 2026-11-25 | what the words on these pages mean.
- [open-questions.md](open-questions.md) | Open questions | open | 2026-11-25 | everything this project does not know yet, in the order we would settle it, and the test that would settle each. IDs AICA-003 to VID-011 (22 rows).

## effects

- [effects/README.md](effects/README.md) | Effects index | effect, index | 2026-11-25 | every effect in the library in one line, how far along it is, and whether any of its cost has been measured on a real console.
- [effects/ao.md](effects/ao.md) | ao (small-scale occlusion) | effect, ao | 2026-11-25 | the two kinds of darkening the ao header gives, and when each is computed.
- [effects/blade.md](effects/blade.md) | blade | effect, blade | 2026-11-25 | what the blade effect draws, how to call it and what it costs.
- [effects/bloom.md](effects/bloom.md) | bloom | effect, bloom | 2026-11-25 | how to get a soft glow round a blade or a lamp without reading the frame back, and what it costs.
- [effects/bolt.md](effects/bolt.md) | bolt (branching strands) | effect, bolt, particles | 2026-11-25 | how the bolt draws lightning that changes a few times a second, and how much it sends to the PVR.
- [effects/bump.md](effects/bump.md) | bump (normal map, highlight power 2) | effect, bump | 2026-11-25 | how a normal map is lit with plain blend passes instead of the PVR's bump unit, and what it costs in draws and video memory.
- [effects/ca.md](effects/ca.md) | ca (frame texture and chromatic aberration) | effect, ca, post | 2026-11-25 | how the world is drawn into a texture, how the colour fringe is made from it, and what it costs in video memory.
- [effects/cloudvolume.md](effects/cloudvolume.md) | cloudvolume (a cloud deck you fly through) | effect, cloudvolume | 2026-11-25 | how the SH-4 ray-marches a cloud layer into a small buffer that the PVR draws over the sky, and what that is estimated to cost.
- [effects/core.md](effects/core.md) | core (the shared parts) | effect, core | 2026-11-25 | what every other effect borrows from `sh4der_core.h`, and what it costs.
- [effects/deform.md](effects/deform.md) | deform (ground that keeps a dent) | effect, deform | 2026-11-25 | how feet and wheels press lasting dents into a ground mesh you already draw, and what it costs.
- [effects/film.md](effects/film.md) | film (old projector wear) | effect, film | 2026-11-25 | what the film effect draws and what it costs.
- [effects/focus.md](effects/focus.md) | focus (fake depth of field) | effect, focus, post | 2026-11-25 | how a sharp region is kept while the rest of the frame softens, without a depth read.
- [effects/frame.md](effects/frame.md) | frame (the frame read) | effect, frame, post | 2026-11-25 | how the library reads the finished frame back, and why each frame carries a number.
- [effects/glow.md](effects/glow.md) | glow (screen bloom) | effect, glow, post | 2026-11-25 | how the screen bloom is built from a small map of the frame, and how to stop it feeding itself.
- [effects/guard.md](effects/guard.md) | guard (real hardware checks) | effect, guard, hardware | 2026-11-25 | the two checks every hand-made polygon should pass, and the three faults that stop a real console but not an emulator.
- [effects/holo.md](effects/holo.md) | holo | effect, holo | 2026-11-25 | how the holo effect makes an inner light breathe with a translucent body, and what it costs.
- [effects/lens.md](effects/lens.md) | lens | effect, lens, post | 2026-11-25 | what the lens overlays are, and the one piece of video memory they use.
- [effects/look.md](effects/look.md) | look | effect, look, post | 2026-11-25 | how the look effect adapts exposure, grades the picture and adds fog glare, and what each part costs.
- [effects/mist.md](effects/mist.md) | mist (a bank of slices) | effect, mist | 2026-11-25 | how a fog bank that things walk into is drawn as upright translucent sheets, and why fill is its whole cost.
- [effects/motes.md](effects/motes.md) | motes (drifting specks) | effect, motes, particles | 2026-11-25 | how the motes store nothing per speck, and how many they draw.
- [effects/pl4nar.md](effects/pl4nar.md) | pl4nar (planar reflection) | effect, pl4nar, reflection | 2026-11-25 | how the world is mirrored into a small render target and looked up per vertex, what it costs in video memory, and which costs are still unmeasured.
- [effects/pool.md](effects/pool.md) | pool (light pools on a plane) | effect, pool | 2026-11-25 | how a lamp's pool of light or a blob shadow is drawn as a fading fan, and why fill is its cost.
- [effects/psm.md](effects/psm.md) | psm (shadow map placement and the two map routes) | effect, psm, shadows | 2026-11-25 | what the three psm headers do, which route draws the map, and what the town demo measured on a console.
- [effects/rise.md](effects/rise.md) | rise (lights that lift and become stars) | effect, rise | 2026-11-25 | what the rise effect draws and what it costs.
- [effects/sh4dow-dc.md](effects/sh4dow-dc.md) | sh4dow-dc (soft character shadows) | effect, shadows, dependency | 2026-11-25 | what SH4DOW-DC does, what its published numbers are worth, and which of its limits are proven.
- [effects/shafts.md](effects/shafts.md) | shafts (shadow shafts) | effect, shafts, post | 2026-11-25 | how dark shapes in the frame cast darker copies away from a light, and what that costs per frame.
- [effects/spark.md](effects/spark.md) | spark (pooled additive quads) | effect, spark, particles | 2026-11-25 | how the fixed pool of 24 sparks works, what it costs, and how it protects the quads that matter.
- [effects/sss.md](effects/sss.md) | sss (back-lit transmission) | effect, sss | 2026-11-25 | what the sss call gives you, what it does not, and where its starting values come from.
- [effects/taa.md](effects/taa.md) | taa (temporal AA maths) | effect, taa, post | 2026-11-25 | what the TAA header computes, where the history comes from, and how far to push its weight.
- [effects/torch.md](effects/torch.md) | torch (flashlight) | effect, torch, post | 2026-11-25 | how the torch fakes a flashlight with no light source, and the trap with AA 2X.
- [effects/trail.md](effects/trail.md) | trail | effect, trail, post | 2026-11-25 | how motion blur and the TAA blend draw the frame before over this one, and what that needs from the screen setup.
- [effects/uvwarp.md](effects/uvwarp.md) | uvwarp (per-vertex texture offset) | effect, uvwarp | 2026-11-25 | how a small table pushes each vertex's texture coordinate, and how to keep it precise over time.
- [effects/visor.md](effects/visor.md) | visor | effect, visor, post | 2026-11-25 | how the night, amber and thermal visor looks are built from tables, and what they cost.
- [effects/warp.md](effects/warp.md) | warp | effect, warp, post | 2026-11-25 | how the warp bends the frame through a mesh, and what it measured on a console.
- [effects/water.md](effects/water.md) | water | effect, water | 2026-11-25 | what the water header computes per vertex, and where its numbers come from.

## errata

- [errata/README.md](errata/README.md) | Errata | errata, pvr, sh4 | 2026-11-25 | where Sega's documents, KallistiOS, outside sources and our own earlier notes disagree with each other or with the console, and which rule we follow.

## hardware

- [hardware/README.md](hardware/README.md) | The hardware ledger | hardware, ledger | 2026-11-25 | where each hardware fact is kept, how to read a ledger row, and how to add one after a console run.
- [hardware/ledger-aica.md](hardware/ledger-aica.md) | Ledger: AICA sound chip and its ARM7DI | hardware, ledger, aica | 2026-11-25 | what is known about the sound chip's processor and memory, for anyone who wants to run their own code on it or use its RAM. IDs AICA-001 to AICA-031 (28 rows).
- [hardware/ledger-bus-dma.md](hardware/ledger-bus-dma.md) | Ledger: buses, DMA and the TA send | hardware, ledger, bus, dma | 2026-11-25 | how fast data moves between main RAM, video memory, the tile accelerator and sound RAM, and how much of the SH-4 each route leaves free. IDs BUS-001 to BUS-051 (31 rows).
- [hardware/ledger-pvr.md](hardware/ledger-pvr.md) | Ledger: PowerVR2 (PVR) | hardware, ledger, pvr | 2026-11-25 | what the graphics chip costs to fill, which per-pixel tricks work on a real console, and where Sega's documents and the console disagree. IDs PVR-001 to PVR-101 (60 rows).
- [hardware/ledger-sh4.md](hardware/ledger-sh4.md) | Ledger: SH-4 processor | hardware, ledger, sh4 | 2026-11-25 | what our console runs measured about the SH-4's cache, transforms and copies, and which earlier numbers were withdrawn. IDs SH4-001 to SH4-046 (34 rows).
- [hardware/ledger-video.md](hardware/ledger-video.md) | Ledger: video memory and the frame | hardware, ledger, video | 2026-11-25 | how much video memory a frame and its targets take, and what reading the frame back costs. IDs VID-001 to VID-013 (13 rows).

## limits

- [limits/budgets.md](limits/budgets.md) | Frame budgets | limits, budget | 2026-11-25 | the measured unit costs to build a frame budget from, and how to add them up without fooling yourself. IDs AICA-021 to SH4-030 (13 rows).
- [limits/recommended-limits.md](limits/recommended-limits.md) | Recommended limits | limits | 2026-11-25 | the limits we recommend when you use this library on a real Dreamcast, and how sure we are of each. IDs PVR-002 to VID-010 (14 rows).
- [limits/scenes.md](limits/scenes.md) | Test scenes and what they measured | limits, scenes, shadows | 2026-11-25 | which demo scenes exist, what each is for, and every console number read from them so far.

## methods

- [methods/benchmarking.md](methods/benchmarking.md) | Benchmarking on a real console | method, bench | 2026-11-25 | how this project measures, what counts as evidence, and the traps that have already given wrong numbers.
- [methods/fault-catcher.md](methods/fault-catcher.md) | The fault catcher | method, debug | 2026-11-25 | what the fault catcher shows when a console crashes or hangs, how to read its screen, and what it cannot catch.
- [methods/host-test-first.md](methods/host-test-first.md) | Host test first | method, test | 2026-11-25 | how code is checked on a PC before it reaches a console, and what a host test can and cannot prove.
- [methods/sh4zam-rule.md](methods/sh4zam-rule.md) | The SH4ZAM rule | method, sh4 | 2026-11-25 | why all maths goes through SH4ZAM, what to do when SH4ZAM has no call for it, and what counts as "slow".

## research

- [research/README.md](research/README.md) | Research notes | research | 2026-11-25 | what each research note in `demo/psm/` is for and what it settled, so you can find the right one.
- [research/depth-fog-ideas.md](research/depth-fog-ideas.md) | Depth fog and glow ideas | research, pvr, fog, depth | 2026-11-25 | which film and TV fog and light effects depend on depth, what the PVR's own table fog already gives, and whether fog could fill the space between c4's depth bands more cheaply than the shadow mask. IDs PVR-002 to PVR-100 (6 rows).
- [research/depth-shading-ideas.md](research/depth-shading-ideas.md) | Depth shading ideas: material inputs on the PVR | research, pvr, shading, depth | 2026-11-25 | how the inputs of a modern material map onto what the PVR already does, where a depth compare on the PVR could fill a gap, and which ideas are worth a spike. IDs PVR-028 to PVR-044 (2 rows).
- [research/outside-sources.md](research/outside-sources.md) | Outside sources (skmp's repositories) | research, license, pvr | 2026-11-25 | what the three outside repositories in `third_party/skmp` are, what we may do with them, and how their results are tagged here.
- [research/sound-chip-precompute.md](research/sound-chip-precompute.md) | The sound chip as a precompute unit | research, aica, arm7, dsp, offload | 2026-11-25 | what the sound chip's three engines (the ARM7, the DSP and the 64 voices) can and cannot compute for shading and shadows, beyond what `demo/psm/OFFLOAD_PLAN.md` already says, and the console test for each idea.

## shadows

- [shadows/README.md](shadows/README.md) | Shadows on the Dreamcast | shadows | 2026-11-25 | the shadow methods this project has, which are ready, which are research, and where each one is written up.
- [shadows/bump-compare-chain.md](shadows/bump-compare-chain.md) | The bump compare chain (spikes c1 to c4) | shadows, pvr, bump | 2026-11-25 | how we tried to make the PVR itself compare a shadow map against each pixel, what each console spike showed, and where the chain stands.
- [shadows/psm-routes.md](shadows/psm-routes.md) | PSM routes A, B and AB | shadows, psm | 2026-11-25 | the three ways this project makes and uses a shadow map, what each costs on a console, and what is still unknown.
- [shadows/thesis.md](shadows/thesis.md) | The thesis: cached per-vertex sun visibility | shadows, research | 2026-11-25 | the idea in `demo/psm/THESIS_SH4_SHADOWS.md`, its predictions, and which have evidence.

## start-here

- [start-here/call-orders.md](start-here/call-orders.md) | Call orders | start, effect | 2026-11-25 | where to find the exact order of calls for each effect, and the shape all of them share.
- [start-here/license-and-credit.md](start-here/license-and-credit.md) | License and credit | start, license | 2026-11-25 | what you must do when you use SH4DER-PVR, and what you must not do.
- [start-here/setup.md](start-here/setup.md) | Setup | start, build, test | 2026-11-25 | how to get SH4ZAM, run the host tests and build for the Dreamcast.
- [start-here/what-it-is.md](start-here/what-it-is.md) | What SH4DER-PVR is | start, overview | 2026-11-25 | what the library does, what it needs, and where each part lives.

## images

Diagrams, with the caption beside each file and the pages that show it.

- [images/bump-texture-space.svg](images/bump-texture-space.svg) | The bump comparator in texture space (recipes R7 and R8, not built): when M is a texture in the object's own u, v (a height or a noise) and Q is one value for the whole object (a water level, a dissolve time), the bump unit's window passes exactly the pixels with |M - Q| <= w, with no bands, no camera depth map and no screen coordinates; the mechanism is MEASURED (PVR-045, PVR-065), the window edges must come from boot measurements (PVR-096, Q56), and the recipes are ESTIMATE. | shown on: glossary.md, research/depth-shading-ideas.md, shadows/bump-compare-chain.md
- [images/c4-chain.svg](images/c4-chain.svg) | The c4 chain: a map drawn from the sun (3.2-3.3 ms), a mask pass where the bump unit compares each receiver band against the map and the 1555 write keeps one bit (4.8-10.2 ms by band width), and one screen layer that darkens by the mask (6.5 ms, the difference of two measured screen passes); one console, 2026-11-25, ledger PVR-028, 045, 090, 091, 094-101. | shown on: shadows/README.md, shadows/bump-compare-chain.md
- [images/cache-placing-trap.svg](images/cache-placing-trap.svg) | The placing trap: the SH-4 operand cache is 16 KB, direct-mapped, indexed by address bits 13..5, so two arrays a multiple of 16 KB apart put line i of each in the same entry and every access misses; measured on one console, mat_transform 13.16 against 38.67 cycles a vertex (1 MB apart), pvr_project 21.15 against 132.5 (256 KB apart in c4), memcpy 398.0 against 46.6 MB/s; the fix is one pool with the second array 8 KB off modulo 16 KB (SH4-002, 003, 020-023, 030-032, 046). | shown on: hardware/ledger-sh4.md, methods/benchmarking.md, methods/sh4zam-rule.md
- [images/cel-ramp-recipe.svg](images/cel-ramp-recipe.svg) | Recipe R1, not built: the SH-4 puts clamp(N.L, 0, 1) in each vertex's u, the PVR interpolates it, and a point-sampled 8 x 1 ramp turns it into hard tone bands per pixel in one opaque pass (a textured model adds one multiply pass at equal depth); it rests on the 10-bit texture-coordinate path and the hole-free equal-depth pass (MEASURED PVR-023, PVR-097), and the recipe itself is ESTIMATE. | shown on: research/depth-shading-ideas.md
- [images/frame-wait.svg](images/frame-wait.svg) | The c4 frame waits: the live view's frame is 39.9 or 49.9 ms while the measured SH-4 and PVR work in it adds to only 21.7, 27.1 and 19.3 ms (MEASURED PVR-090, 091, 101, PVR-094, one console, 2026-11-25); the rest is waiting, and why the frame takes one of two values 10 ms apart is OPEN (Q53). | shown on: limits/budgets.md, open-questions.md, shadows/bump-compare-chain.md
- [images/pvr-pass-recipe.svg](images/pvr-pass-recipe.svg) | A PVR pass recipe: the order a pixel goes through (depth, texel or bump level, combine, offset colour, clamp, fog, blend, write-out; the punch-through test in its own list) and a card with every field of one pass, filled in for the c4 mask pass's bump draw, each entry with its status (FACT from Sega's manual or our code, MEASURED from our console). | shown on: glossary.md, research/depth-shading-ideas.md, shadows/bump-compare-chain.md
- [images/static-sun-split.svg](images/static-sun-split.svg) | Idea T4, not built: with a fixed sun, draw the static casters' map once and bake static-on-static shadow at load, then each frame draw only a small mover map cut to its tiles and run three compares (static on mover, mover on static, mover on mover) into one mask; how the bits join, and the break-even number of movers, are OPEN, and the PVR's depth buffer cannot be preloaded from a cached map (FACT PVR-083). | shown on: open-questions.md, shadows/bump-compare-chain.md
- [images/tile-list-cut.svg](images/tile-list-cut.svg) | The tile-list cut: rendering a target through only the tiles it covers took the 128 x 130 map pass from 3,312 to 375 us (20 tiles) and the 320 x 240 mask pass from 6,130 to 3,760 us (80 tiles) with the same picture (MEASURED PVR-092, one console, 2026-11-25); the savings of 2.9 ms and 2.4 ms are arithmetic (ESTIMATE PVR-093). | shown on: hardware/ledger-pvr.md, limits/budgets.md, shadows/bump-compare-chain.md
- [images/town-band-counts.svg](images/town-band-counts.svg) | Band counts under c4's rules: the whole static town needs 44,577 / 26,969 / 18,097 band vertices at bands 1 / 2 / 4 codes wide, the boot view 11,146 / 7,090 / 5,174, and the movers' patches only 864 / 460 / 260, against 2,464 / 1,288 / 692 in c4's measured scene; the town counts and every cost on top are host ESTIMATEs (TOWN-051 to 055), which is why static shadow goes to a bake and c4 runs for movers only. | shown on: limits/scenes.md, open-questions.md, shadows/bump-compare-chain.md
