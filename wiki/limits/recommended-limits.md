---
title: Recommended limits
tags: [limits]
updated: 2026-11-25
sources: [wiki/hardware/, demo/psm/bench/NOTES.md, demo/psm/ROUTE_AB_NOTES.md, sh4dow-dc/README.md]
---
# Recommended limits

**What you will learn:** the limits we recommend when you use this library on a real Dreamcast, and how sure we are of each.
**For:** anyone setting up a project with SH4DER-PVR, and any agent tuning a scene.

Every limit is one of two kinds:

- **PROVEN**: a MEASURED number from our console backs it. The ledger row is named.
- **ADVISED**: it is our best judgement, but no measured number backs it yet. The test that would prove it is named.

All PROVEN limits rest on **one console**. The run count is shown in each row, not used as a gate: a limit is PROVEN when a MEASURED number backs it, and a repeat that would harden it is named where it is cheap. A limit that is arithmetic on a measured cost says so.

## Hard rules (seen on a real console)

These are faults the console showed and an emulator hid. Break them and the console draws garbage or stops.

- Test every triangle with `sh4der_tri_area_ok` before you send it; a triangle with no screen area draws bright lines (PVR-080).
- Test every polygon header with `sh4der_poly_hdr_ok`; a header never compiled ends the list (PVR-081).
- Keep every 4-byte access aligned; colour writers need a stride that is a multiple of 4 (VID-009).

## Sending and copying

| Limit | Kind | Backing |
|---|---|---|
| Send long meshes in store-queue runs, not `pvr_prim` per vertex: 142 against 260 ns a vertex. **But measure your own mesh:** the warp's mesh ran 6 % slower by runs (973 against 914 ns), for a reason not yet known, so the warp now sends its mesh by `pvr_prim` and its pixelate pass by runs. | PROVEN (TA test in h1 and h2: two runs, one console; warp pixelate pass); the exception is OPEN | BUS-001, BUS-003, BUS-004 |
| Never place a copy's or a transform's source and destination a multiple of 16 KB apart. It cost up to 8.5 times; the projection of Shadow chain with measured edges [c4b] ran 2.2 times faster once its arrays were placed apart. | PROVEN (h2, Whole shadow chain by the PVR [c4] and Shadow chain with measured edges: three runs, one console) | SH4-031, SH4-032, SH4-046, SH4-048 |
| Copy RAM to RAM with `shz_sq_memcpy32` or `sq_cpy` (150 MB/s), or `shz_memcpy` (132), not `memcpy` (63). | PROVEN (h2: one run, one console; repeat: h2's copy test in a later image) | SH4-030 |
| Use `mat_transform`, not a C loop of single SH4ZAM calls (13.16 against 28.67 cycles a vertex). Our `shz_dce_xmtrx_project_point3_batch` is 5.7 to 6.0 % slower than `mat_transform` in the cache and 5.5 to 7.3 % out of it: it has no measured gain. | PROVEN (`mat_transform` and the C loop: h2, Sun-ray walk timing bench [ray1] and Sun-ray walk timing, clock checked [ray2], three runs; the batch: Sun-ray walk timing bench and Sun-ray walk timing, clock checked, two runs; one console) | SH4-020, SH4-022, SH4-050, SH4-065 |
| Do not put a lookup grid in OC-RAM in a frame that also transforms vertices: the RAM cache halves, the vertices cost 6.8 % more, and the interleaved slice was slower overall. | PROVEN (Sun-ray walk timing bench and Sun-ray walk timing, clock checked: two runs, one console) | SH4-054, SH4-063, SH4-012 |
| A burst DMAC copy holds the SH-4 for the whole copy. For a background copy use cycle steal (84 % of the SH-4 left). | PROVEN (h2, and h1 for BUS-032: one console) | BUS-031, BUS-032 |

## Reading back

| Limit | Kind | Backing |
|---|---|---|
| Never read video memory pixel by pixel through the uncached address in a hot loop (3.7 MB/s). Use PVR-DMA (38 MB/s), or the cached address with `dcache_inval_range` first (38.7 MB/s). | PROVEN (h2: one run, one console) | SH4-040, SH4-041, BUS-021 |
| Invalidate before every cached read of video memory; without it 238 of 256 lines read stale. Never write through that address. | PROVEN (h2: one run, one console) | SH4-006 |
| Copy a map to RAM before random reads: 20,000 scattered reads took 12.9 ms from video memory, 2.8 ms from RAM. | PROVEN (spike s2: one run, one console) | SH4-042 |
| Do not move per-frame data through sound RAM: 5.0-5.7 MB/s, so 32 KB costs 6.5 ms. Sound RAM suits data made once or rarely. | PROVEN (the rates, h2: one run, one console; 6.5 ms is arithmetic on them, and one 32 KB copy to texture memory measured 6,511 us) | AICA-020, AICA-021, AICA-023 |

## Render targets

| Limit | Kind | Backing |
|---|---|---|
| Render a target 2 rows taller than the map you need; the last 32 bytes arrive one render late. | PROVEN (spike s3: one run, one console) | PVR-021 |
| Turn dither off for a target that holds codes, and back on after. | PROVEN (spikes s2 and s3: two runs, one console) | PVR-022 |
| Keep a target at most 480 rows under KOS. | PROVEN (spike s3: one run, one console) | PVR-026 |
| Keep a 16-bit depth code under 16,384 (14 bits) for exact codes. | PROVEN (spike s3: one run, one console) | PVR-024 |
| Budget a render target pass at least 3.2 ms of PVR time, whatever its size, when it renders through the screen's whole tile list. | PROVEN (320 × 240 in Shadow test by blending layers [c1]; 128 × 128 map in Whole shadow chain by the PVR: 3,312 us, and 3,311 to 3,314 us in four more Whole shadow chain by the PVR lines; one console) | PVR-004, PVR-092, PVR-100 |
| Render a small target through a tile list cut to the tiles it covers: the 128 × 128 map fell from 3,312 to 375 us, the 320 × 240 mask from 6,130 to 3,760 us, with the same picture. | PROVEN (one run, one console); not yet in the library's own passes (Q54) | PVR-092 |
| Start pl4nar's target at 256 × 128. Its size did not change the mirror pass's PVR time in the car runs (3.20 to 3.31 ms, PVR-174 and PVR-175, MEASURED, one console): measure the mirror before you cut size. | ADVISED | Test: pl4nar cost table at 128 × 64, 256 × 128, 512 × 256 with the mirror pass's PVR time, no key pressed (`demo/pl4nar/AGENT_WORKFLOW.md:60-71`) |

## Fill

| Limit | Kind | Backing |
|---|---|---|
| Count 3.1 ms of PVR time for each full-screen translucent layer at 640 × 480. At 30 fps that is about 9 layers before anything else is drawn (arithmetic). | PROVEN (cost, h1 and Shadow test by blending layers: two runs, one console); the count is ESTIMATE | PVR-002 |
| Do not let translucent triangles pile up in one tile: 256 in one tile took 6.4 ms, 82 us spread out. Spread particles; cap overlapping quads. | PROVEN (h1: one run, one console) | PVR-009 |
| Use mipmaps on any texture drawn smaller than its size; without them a 16-times shrink cost 21 times as much. | PROVEN (h1: one run, one console) | PVR-008 |
| Limit light pools and halos by distance; their cost is translucent fill. | ADVISED | Test: town or street scene, pools on and off, PVR render time by `pvr_get_stats` |

## Shadows

| Limit | Kind | Backing |
|---|---|---|
| Whole shadow chain by the PVR: make Q from the window edges measured at boot, not from a cosine table. With them the ground showed 0 acne at every b0 from 0 to 3; with the cosine it needed b0 1.5. | PROVEN (ground alone, one sun; Shadow chain with measured edges and Shadow chain with polled waits [c4g]: two runs, one console) | PVR-102, PVR-103, PVR-116 |
| Whole shadow chain by the PVR: with the measured edge set b0 between 0 and 1. Acne was 0 at every b0 from 0 to 3 in both scenes at two suns; a higher b0 only trades a few edge pixels for real shadow shown lit (under 0.1 % of the mask). | PROVEN that b0 0 to 3 gives 0 acne (two suns, one run, one console); the choice of 0 to 1 is ADVISED | PVR-119. Test: a third sun (Q57) |
| The mask of Whole shadow chain by the PVR: use the 1555 route; punch-through gives the same pixels but costs 14 to 18 % more PVR time. Use punch-through only where its alpha test is needed (for example to join compares, Q58). | PROVEN (cost, two suns; Shadow chain with measured edges and Shadow chain with polled waits: two runs, one console); the join ADVISED | PVR-105, PVR-116 |
| Budget a ray through a height grid at 22 cycles a cell plus 74 a ray (76 at worst), not the thesis's 12 to 16 plus 30. | PROVEN (Sun-ray walk timing bench and Sun-ray walk timing, clock checked: two runs, twelve fits, one console; the two constants are the page's fit to MEASURED cycles a ray) | SH4-052, SH4-059 |
| Budget a ray-triangle test at 33 cycles when every ray shares one direction (prepared once a triangle, 64 bytes) and 62 for any direction, both with early outs and with the triangles in the cache. Out of the cache, measure. | PROVEN in the cache (one run, one console, 96 triangles); a town-sized set ADVISED | SH4-066. Test: TOWN-085 |
| Time work with the cycle counter, not `timer_ns_gettime64`: the ns clock jumps 2.5 ms at each reload, about once a second, so any interval that spans one reads 2.5 ms long. But the cycle counter stops while the CPU sleeps in a KallistiOS wait: for a frame's wall time use the ns clock with the step taken out, or poll the waits. | PROVEN (the step: one run, one console, and predicted from the KallistiOS source; the sleep: one run, one console; repeat: Shadow chain with polled waits test 8 again) | SH4-057, SH4-067 |
| Poll the render-done event where a frame waits on it every frame; do not block in KallistiOS's sleeping `pvr_wait_render_done` there. It wakes on the 100 Hz tick, up to 10 ms a pass late: polled, the frame of Shadow chain with polled waits fell from 39.9 to 23.3 ms. **Where the wait is rare, do not spin:** in the town (the wait on about one frame in six) polling left route B the same and made route A 3 ms slower; it only halved the worst waits. | PROVEN for Shadow chain with polled waits (two runs, one console: PVR-117, PVR-133); PROVEN that it does not pay in the town (one run, one console: PVR-136); why route A slows is OPEN (Q79) | PVR-117, PVR-133, PVR-136, errata K8 |
| **Sky AO from the shadow map (method 2): budget a direction at 1.8 ms SH-4, 1.2 ms PVR and 68 KB of TA** for about 1,160 caster triangles, both sides. In the town K 4 added 10 ms to the frame (SH-4 +9.8 ms, 2.4 ms a direction). Draw **one side** (about 1.3 ms SH-4, 0.75 ms PVR, 34 KB a direction, DERIVED) once the casters are known closed and wound one way (Q78); the depth union buys nothing (it fails, and with one side it saves no PVR time). | PROVEN (Sky shadow overlap and cost [ao0] and Town corner shading, three ways [ao1]: one run each, one console; the one-side figures DERIVED); one side ADVISED until Q78 | PVR-127 to PVR-130, PVR-135, SH4-071 to SH4-073 |
| **Baked AO (method 1) is free at run time:** a byte a vertex changed the town's frame by under 0.6 ms (53.2 → 53.4 still) and it left the frame of Room shading, four ways [ao4] unchanged (27.9 = 27.9 ms in A); it costs 32 to 44 ns a vertex of placing. Pay for it at build or load: 64 rays a vertex took 254 ms for 2,142 vertices and 393 ms for 2,544 against exact shapes. Prefer it for anything static. | PROVEN (Town corner shading, three ways and Room shading, four ways, build d: two programs, one run each, one console) | PVR-135, PVR-138, SH4-073, SH4-076, SH4-077 |
| **Every extra render-to-texture pass costs about its whole PVR time in the frame, and at least about 3.3 ms, even at 128 × 130.** The hens' target M in Room shading, four ways, build d (576 vertices) added 3.5 ms to the frame; the split's second shadow target added 7.1 to 10.2 ms. Draw small extras (movers' AO tiles, an inset) into a target you already render, or cut the pass to its tiles (PVR-092). **The 3.3 ms is the whole tile table, not the pixels: cut to 4 × 5 tiles the same pass took 344 us (s7, PVR-156).** | PROVEN that the passes add their time (two runs, one console); the remedy ADVISED until Q82 | PVR-004, PVR-139 to PVR-141, PVR-152, PVR-153, PVR-156 |
| **AO shows where the ambient light dominates.** The owner saw it only in CLAY (ambient 0.72, sun 0.30), not in COLOUR (0.50, 0.50). Spend AO where a scene is in shade, indoors or overcast, or exaggerate it; in direct sun with colour it is not seen. | ADVISED (the owner's eyes, one run; Q86) | PVR-145 |
| **Lightmap quads for flat AO surfaces:** in room C a half drew 4,268 vertices (83 KB of TA, estimated) with lightmaps against 6,680 (147 KB) with vertex AO, for 64 KB of textures. Use them where the TA is short. | ADVISED (counts exact; the frame difference, 19.3 against 20.2 ms, one run) | PVR-138, PVR-142 |
| Run a static-on-static sun bake at load or in slices, never inside one frame: it took 152 ms on the console. | PROVEN that a bake does not fit in a frame (152 ms; one run, one console); how to slice it is ADVISED | SH4-070 |
| Do not trust the town's TA estimate as a limit: at an estimated 74 % of the buffer route A's vertex buffer overflowed (PARAM) on every frame, and on route B the estimate read 21 % under KallistiOS's count; Town HUD, no debug texture [hud1c-3] repeats it (754 KB estimated at the full 1,024 KB, PVR-149). Count the PVR's PARAM events (KallistiOS does not report them, errata K7) or watch KallistiOS's "used" reach the whole buffer; the OPB events are not a count of overflows until Q81's proof test. Send the HUD where an overflow cannot cut it: with the text first the HUD stayed whole (PVR-150). | PROVEN that the estimate is no limit (route A overflowed on every frame at an estimated 74 %; one run, one console, three binaries); counting the PARAM events is ADVISED; sending the HUD first PROVEN at one view (PVR-150, one run) | PVR-121, PVR-122, PVR-149, PVR-150 |
| **One skinned hero (1,660 triangles, 33 bones) with every pass costs 18.8 ms a frame on the clay set and 23.7 ms in a furnished room; opaque alone holds 60 Hz.** The shadow is the dearest pass (+4.80 ms PVR, +1.98 SH-4); budget it first. | PROVEN (two runs, one console; s7 repeats within 0.15 ms) | PVR-146, PVR-147, SH4-079, PVR-159, SH4-084 |
| **Run per-agent sweeps on a tick, not every step:** the hens' grid sweep on a 100 ms tick cost 5,392 cycles a step for 4 hens and 36,328 for 16, against 49,810 and 221,541 every step; a naive sweep is 6 to 36 times dearer still. | PROVEN (two runs, one console; s7 within 5 %) | SH4-080, SH4-085 |
| **Quote a segment's mean, not its largest frame.** Town corner shading, clay view [ao1c] twice: the means repeated within 0.1 ms in all six segments; the largest frame moved by up to 10.5 ms (87.3 → 97.8). | PROVEN (two runs, one console) | PVR-148 |
| **Cut a small render target's tile list to the tiles it draws.** The 128 × 128 target M in Room shading, four ways took 3,256 us of PVR through the screen's whole table (20 × 15 tiles of 32 × 32 px) and 344 us cut to its own 4 × 5 tiles (128 × 160 px), the same texels. | PROVEN for that target and that cut (one run, one console) | PVR-156 |
| **A point light run per vertex on the SH-4 costs about 1.17 us a vertex:** 3,883 vertices took 4.5 ms and pushed the room in Lamp with a lit shade [ao-lamp] from 16.2 to 20.6 ms. Light only what the lamp reaches. | PROVEN (one run, one console) | SH4-083, PVR-157 |
| **An opened, unused punch-through list costs about 45 us of PVR a frame;** a punch-through lamp shade (96 triangles) ran with no hang beside the user clip. | PROVEN (one run, one console) | PVR-157 |
| **The new street map (Street car demo [pl4m] c5) is SH-4 bound at about 110 ms with the reflection and 69 ms without; the mirror pass is about 41 ms of SH-4 and the target size does not change it.** Cut the map's vertex work before tuning the target. | PROVEN (one run, one console); the per-vertex cost ESTIMATE | PVR-160, SH4-086 |
| QR codes on a result page: 8 px a module by default, 6 px is safe; 4 px with six codes a page needs a second photo. Keep the numbers as text too. | PROVEN for 8 and 6 px (every code read; one run, one console, one television, one phone; repeat: another television); keeping the text beside is ADVISED | VID-014 |
| **Do not cast shadows to infinity.** In the town at draw distance INF with all casters, route A ran at 18 fps and route B at 13-14. Limit casters to the shadow map's square, and set a shadow draw distance. | PROVEN that INF with all casters misses 30 fps in that view (one run, one console, HUD photos; PVR-108 to PVR-113); the remedy is ADVISED | Town photos (see [scenes](scenes.md)). Test: sweep the demo's CASTERS and DRAW DISTANCE rows and log SH-4, PVR and fps. |
| Route B (SH-4 depth map) cost 40-44 ms of SH-4 time in the town view: do not use it for a whole town at 30 fps. | PROVEN (one view, one run, one console; suite s1's autorun, at another view, ran route B at 42 to 45 ms, SH4-069) | PVR-112, PVR-113 ([scenes](scenes.md)) |
| Shadow map at most 256 × 256 for route AB: 512 × 512 needs 13.7 ms of download alone by its interrupt (14.8 ms polled) and does not render past row 479. | PROVEN (s3 and h2: one run each, one console) | PVR-026, BUS-021 |
| **sh4dow-dc: shade only the first N characters within R metres of the camera.** N and R are not yet known. | **OPEN** | See the bench request below. |
| Drop sh4dow-dc's foot cones past 20 units of camera distance (they fade from 18). | ADVISED (the library's default, not measured) | `sh4dow-dc/README.md:207` |

The poll row in one picture:

![The render-done wait over one frame, sleeping against polled (Shadow chain with polled waits, its tests 8 and 8b, 60 frames each, one console, one run, 2026-11-25, PVR-117, closes Q53): with KallistiOS's sleeping pvr_wait_render_done the render-done interrupt comes at each render's end but the wait returns 4,645 / 2,929 / 9,940 us later (map, mask, screen), on the next 100 Hz tick, and the frame is 39.9 ms, 4.00 ticks of 9,983 us; with the event polled each wait returns 7 to 8 us after its interrupt and the frame is 22.4 to 22.6 ms, mean 23,264 us, 42 % shorter (MEASURED); where the SH-4 work sits between the waits is DERIVED; with both waits polled about 1 frame in 5 is still about 26.5 ms (OPEN, Q69).](../images/render-wait-poll.svg)

The picture places the SH-4 work between the waits (the 2.05, 0.94 and 0.13 ms gaps): that placing is **DERIVED**, not measured; the waits, the interrupt times and the frame lengths are MEASURED (PVR-117).

### Bench request: sh4dow-dc N casters × R metres

**Question:** how many characters (N), within what distance (R), can sh4dow-dc shade per frame within a budget of 3 ms of SH-4 time?

**What we have:**

- 6 characters, 84 columns, 5,406 vertices took 14.70 ms before column lists and 4.5 ms after, on one console. The original game's author timed it, with no date. REPORTED, not our console (`sh4dow-dc/README.md:274-284`).
- Packed lists: emulator numbers only, REPORTED, "emulator, not evidence" (`sh4dow-dc/README.md:286-297`).
- The original budget was 14 casting limbs a character and the six nearest characters a frame (`sh4dow-dc/README.md:160`). That is a choice, not a measured limit.

**Proposed test (for Junior or Senior):**

- A bench image with placeholder characters at known vertex counts: 1, 2, 4, 6, 8 and 12 characters, each with 14 columns.
- Characters placed at 5, 10, 20 and 40 m.
- Time `sh4dow_*` prepare and the vertex pass apart, with the cycle counter. Show the plain list path and the packed path.
- Print the vertex count and ms per frame for each N and R.
- One console, then a second run.

**Result:** a PROVEN row in the [SH-4 ledger](../hardware/ledger-sh4.md) and a filled row in this table.

## Effects

| Limit | Kind | Backing |
|---|---|---|
| Look: clamp the eye's gain to 0.4-2.0; RGB565 bands past that. | ADVISED (author's eye) | `README.md:1219`; test: a ramp photo at gains 1.5, 2.0, 2.5 |
| TAA: history weight never above 0.9. | ADVISED | `include/sh4der_taa.h` top comment |
| Trail and ca never at once (shared stride register). | Hard rule (one register) | VID-003 |
| AA 2X: scale screen x exactly once (torch). | Hard rule (design) | VID-010 |

## Related

- [Budgets](budgets.md)
- [Frame budget: practices](frame-budget.md)
- [Scenes](scenes.md)
- [Open questions](../open-questions.md)
