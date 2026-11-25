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

All PROVEN limits rest on **one console, one run**. A limit that is arithmetic on a measured cost says so.

## Hard rules (seen on a real console)

These are faults the console showed and an emulator hid. Break them and the console draws garbage or stops.

- Test every triangle with `sh4der_tri_area_ok` before you send it; a triangle with no screen area draws bright lines (PVR-080).
- Test every polygon header with `sh4der_poly_hdr_ok`; a header never compiled ends the list (PVR-081).
- Keep every 4-byte access aligned; colour writers need a stride that is a multiple of 4 (VID-009).

## Sending and copying

| Limit | Kind | Backing |
|---|---|---|
| Send long meshes in store-queue runs, not `pvr_prim` per vertex: 142 against 260 ns a vertex. **But measure your own mesh:** the warp's mesh ran 6 % slower by runs (973 against 914 ns), for a reason not yet known, so the warp now sends its mesh by `pvr_prim` and its pixelate pass by runs. | PROVEN (TA test, warp pixelate pass); the exception is OPEN | BUS-001, BUS-003, BUS-004 |
| Never place a copy's or a transform's source and destination a multiple of 16 KB apart. It cost up to 8.5 times; c4b's projection ran 2.2 times faster once its arrays were placed apart. | PROVEN | SH4-031, SH4-032, SH4-046, SH4-048 |
| Copy RAM to RAM with `shz_sq_memcpy32` or `sq_cpy` (150 MB/s), or `shz_memcpy` (132), not `memcpy` (63). | PROVEN | SH4-030 |
| Use `mat_transform`, not a C loop of single SH4ZAM calls (13.16 against 28.67 cycles a vertex). Our `shz_dce_xmtrx_project_point3_batch` is 5.8 % slower than `mat_transform` in the cache and 7.3 % out of it: it has no measured gain. | PROVEN (one run each) | SH4-020, SH4-022, SH4-050 |
| Do not put a lookup grid in OC-RAM in a frame that also transforms vertices: the RAM cache halves, the vertices cost 6.8 % more, and the interleaved slice was slower overall. | PROVEN (one run) | SH4-054, SH4-012 |
| A burst DMAC copy holds the SH-4 for the whole copy. For a background copy use cycle steal (84 % of the SH-4 left). | PROVEN | BUS-031, BUS-032 |

## Reading back

| Limit | Kind | Backing |
|---|---|---|
| Never read video memory pixel by pixel through the uncached address in a hot loop (3.7 MB/s). Use PVR-DMA (38 MB/s), or the cached address with `dcache_inval_range` first (38.7 MB/s). | PROVEN | SH4-040, SH4-041, BUS-021 |
| Invalidate before every cached read of video memory; without it 238 of 256 lines read stale. Never write through that address. | PROVEN | SH4-006 |
| Copy a map to RAM before random reads: 20,000 scattered reads took 12.9 ms from video memory, 2.8 ms from RAM. | PROVEN | SH4-042 |
| Do not move per-frame data through sound RAM: 5.0-5.7 MB/s, so 32 KB costs 6.5 ms. Sound RAM suits data made once or rarely. | PROVEN (the rates; 6.5 ms is arithmetic on them, and one 32 KB copy to texture memory measured 6,511 us) | AICA-020, AICA-021, AICA-023 |

## Render targets

| Limit | Kind | Backing |
|---|---|---|
| Render a target 2 rows taller than the map you need; the last 32 bytes arrive one render late. | PROVEN | PVR-021 |
| Turn dither off for a target that holds codes, and back on after. | PROVEN | PVR-022 |
| Keep a target at most 480 rows under KOS. | PROVEN | PVR-026 |
| Keep a 16-bit depth code under 16,384 (14 bits) for exact codes. | PROVEN | PVR-024 |
| Budget a render target pass at least 3.2 ms of PVR time, whatever its size, when it renders through the screen's whole tile list. | PROVEN (320 × 240 in c1; 128 × 128 map in c4: 3,312 us, and 3,311 to 3,314 us in four more c4 lines) | PVR-004, PVR-092, PVR-100 |
| Render a small target through a tile list cut to the tiles it covers: the 128 × 128 map fell from 3,312 to 375 us, the 320 × 240 mask from 6,130 to 3,760 us, with the same picture. | PROVEN (one run); not yet in the library's own passes (Q54) | PVR-092 |
| Start pl4nar's target at 256 × 128 and go down before anything else. | ADVISED | Test: pl4nar cost table at 128 × 64, 256 × 128, 512 × 256 (`demo/pl4nar/AGENT_WORKFLOW.md:60-71`) |

## Fill

| Limit | Kind | Backing |
|---|---|---|
| Count 3.1 ms of PVR time for each full-screen translucent layer at 640 × 480. At 30 fps that is about 9 layers before anything else is drawn (arithmetic). | PROVEN (cost); the count is ESTIMATE | PVR-002 |
| Do not let translucent triangles pile up in one tile: 256 in one tile took 6.4 ms, 82 us spread out. Spread particles; cap overlapping quads. | PROVEN | PVR-009 |
| Use mipmaps on any texture drawn smaller than its size; without them a 16-times shrink cost 21 times as much. | PROVEN | PVR-008 |
| Limit light pools and halos by distance; their cost is translucent fill. | ADVISED | Test: town or street scene, pools on and off, PVR render time by `pvr_get_stats` |

## Shadows

| Limit | Kind | Backing |
|---|---|---|
| c4 chain: make Q from the window edges measured at boot, not from a cosine table. With them the ground showed 0 acne at every b0 from 0 to 3; with the cosine it needed b0 1.5. | PROVEN (ground alone, one sun, one run) | PVR-102, PVR-103 |
| c4 chain: keep b0 at 3.0 with the measured edge until lower values are tested in a scene; both scenes showed 0 acne there. | ADVISED | Test: Q57 (b0 0 to 1.5 with the measured edge, three suns) |
| c4 chain mask: use the 1555 route; punch-through gives the same pixels but costs 14 to 18 % more PVR time. Use punch-through only where its alpha test is needed (for example to join compares, Q58). | PROVEN (cost, two suns, one run); the join ADVISED | PVR-105 |
| Budget a ray through a height grid at 22 cycles a cell plus 74 a ray, not the thesis's 12 to 16 plus 30. | PROVEN (one run; the two constants are the page's fit to MEASURED cycles a ray) | SH4-052 |
| **Do not cast shadows to infinity.** In the town at draw distance INF with all casters, route A ran at 18 fps and route B at 13-14. Limit casters to the shadow map's square, and set a shadow draw distance. | PROVEN that INF with all casters misses 30 fps in that view (one run, HUD photos; PVR-108 to PVR-113); the remedy is ADVISED | Town photos (see [scenes](scenes.md)). Test: sweep the demo's CASTERS and DRAW DISTANCE rows and log SH-4, PVR and fps. |
| Route B (SH-4 depth map) cost 40-44 ms of SH-4 time in the town view: do not use it for a whole town at 30 fps. | PROVEN (one view, one run) | PVR-112, PVR-113 ([scenes](scenes.md)) |
| Shadow map at most 256 × 256 for route AB: 512 × 512 needs 13.7 ms of download alone by its interrupt (14.8 ms polled) and does not render past row 479. | PROVEN | PVR-026, BUS-021 |
| **sh4dow-dc: shade only the first N characters within R metres of the camera.** N and R are not yet known. | **OPEN** | See the bench request below. |
| Drop sh4dow-dc's foot cones past 20 units of camera distance (they fade from 18). | ADVISED (the library's default, not measured) | `sh4dow-dc/README.md:207` |

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
- [Scenes](scenes.md)
- [Open questions](../open-questions.md)
