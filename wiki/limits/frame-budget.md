---
title: "Frame budget: practices that keep the frame rate"
tags: [limits, budget, practice]
updated: 2026-11-25
sources: [wiki/hardware/ledger-pvr.md, wiki/hardware/ledger-sh4.md, wiki/hardware/ledger-bus-dma.md, wiki/hardware/ledger-video.md, wiki/limits/budgets.md]
---
# Frame budget: practices that keep the frame rate

**What you will learn:** how to tell whether the SH-4 or the PVR is holding a frame back, what each measured feature costs, and the practices that follow, each with the test that measures it on your own console.
**For:** a programmer who adds an effect and wants to keep 60 Hz (or 30), and anyone reading a result page.

Every number here comes from the [hardware ledger](../hardware/README.md), one console (Holly 2.42, PVR-123), 2026-11-25 unless a row says otherwise. A practice is **PROVEN** only where a MEASURED number backs it; otherwise it is **ADVISED**, and names the test that would prove it. The unit costs themselves are in [budgets](budgets.md); this page is what to do with them.

## The advised default look: baked AO plus PVR-drawn shadow tiles

The owner's call (2026-11-25): **baked AO a vertex, plus shadows the PVR draws into small tiles (route A), is the cleanest and fastest look.** It is the advised default. Its parts are measured; "fastest" as a whole is not yet.

**The recipe:**

1. **AO baked at load, one byte a vertex, applied on the SH-4** (ao4's method 1). Not the PVR AO map (method 2).
2. **Shadows drawn by the PVR into a tile a caster** in one small render target, then laid on the ground as multiply quads. Route A, the PVR draws the map; not route B, the SH-4's map in RAM.
3. **Tile size by caster:** 128 × 128 for a car or a person; 64 or 32 for a small one. **Only the nearest few casters get a tile.** Each fades out by distance (full strength to about 40 m, none by about 50 m; chosen numbers), and its tile is released only at fade 0 (section 3).
4. **Cut the target's pass to its own tiles.** This is the cost lever.

| Part | What is known | Status | Ledger |
|---|---|---|---|
| Baked AO at draw | +222 / +280 us of SH-4 a frame for 2,142 / 2,544 vertices; frame +0.2 ms (method 1) against +7.5 to +10.0 ms for the PVR AO map (method 2) in the same scenes | MEASURED (two runs of the build, one console) | PVR-176, PVR-152, PVR-135 |
| Baked AO at load | 253 ms (2,142 vertices) and 392 ms (2,544), 64 rays a vertex | MEASURED | PVR-176, SH4-076 |
| Shadow tiles, small casters | ao4's room C: a 32 px tile a hen (4 cm a texel) in a 128 × 128 target. The target's send 356 us (576 vertices), its pass 3,260 us of PVR uncut; the multiply quads +306 us for 14,996 pixels. Frame: lightmap with tiles 23.3 ms, without 20.3, PVR AO map instead 29.5 | MEASURED (two runs, one console) | PVR-152, PVR-153, PVR-154, PVR-176 |
| The cut | The same target's pass, cut to its 20 tiles: 3,256 → 344 us, the same texel sum | MEASURED (one run) | PVR-156 |
| Nearest casters only | The street demo gives tiles to traffic within 50 m of the car (`demo/pl4nar/src/demo.c:39`); at most 4 tiles were seen in a stage | MEASURED (the count); the 50 m is a design choice | PVR-161, PVR-175 |
| 128 × 128 for cars and people, 64 or 32 for small casters | Only the 32 px hen tile is measured; the larger sizes and the number of tiles a frame are not | ADVISED | — |
| Route A against route B, the town | One view each: route A 18 fps (SH-4 13.6 ms, PVR 15.9), route B 14 fps (SH-4 40.2, PVR 7.1); a3e's means still: route A 57.1 ms, route B 76.8. Route A was the faster in the town, which uses a whole map, not tiles | MEASURED (one run each) | PVR-108, PVR-112, PVR-136 |
| "Fastest" for the whole recipe | Not measured as one scene | ADVISED | — |

**The test that would prove "fastest":** one scene and one camera path, run on a console with no key pressed, comparing:

- (a) this recipe, with the tile cut on and off;
- (b) the same scene on route B;
- (c) baked AO with the PVR AO map in place of the tiles.

Read the frame, SH-4 and PVR columns of each, with the tile sizes 32, 64 and 128 and 2, 4 and 8 casters given tiles. The ao4 program already holds (a) without the cut and (c); the cut stages and route B need adding.

## 1. The budget: two bounds, not one

At 60 Hz a frame is 16.7 ms; at 30 Hz, 33.3 ms. The SH-4 (transform, light, send) and the PVR (render) are **separate bounds**:

- In a program that overlaps them (the SH-4 builds frame N+1 while the PVR renders frame N), the frame costs about the **larger** of the two.
- In a program that runs them one after the other, the frame costs about their **sum**. The corridor test says so on its page ("Frames are serial: SH-4 sends, then waits for the render", PVR-182).
- On top of either come the waits. KallistiOS's sleeping render-done wait returned up to 10 ms after the render; polled, 7 to 8 us, and a frame fell from 39.9 to 23.3 ms (PVR-117). In the town, polling did not help (the wait ran on one frame in six, PVR-136).

**What each of our programs is bound by:**

| Program | Frame | SH-4 | PVR | Bound | Ledger |
|---|---|---|---|---|---|
| Street demo c7, reflection on | 25.25 ms | 21.25 | 7.45 screen + 3.23 mirror | SH-4 | PVR-174 |
| Street demo c5 | 110.17 ms | 109.97 | not measured right | SH-4 | PVR-160, PVR-162 |
| Corridor, every filter | 16.63 to 16.66 ms | 2.46 to 3.20 | 5.39 to 9.44 | neither: 60 Hz held | PVR-180 |
| Hero, all passes | 18.76 ms | 7.37 | 10.56 | the two together (17.9 ms summed): fits a serial frame, ESTIMATE | PVR-178 |
| Hero, opaque + shadow | 16.50 ms | 5.73 | 9.54 | neither: 60 Hz held; the PVR is the larger | PVR-178 |
| d1-b with the fade | 20.32 ms | — | 5.05 screen + 4.92 map | misses 60 Hz after +3.7 ms | PVR-172 |
| Town, route A, one view | about 55 ms (18 fps) | 13.3 to 13.6 | 15.9 | both near 16.7, and waits on top | [scenes](scenes.md) |

**How to read a page.** Take the frame (fr), the SH-4 time and the PVR time of the same stage:

1. Frame at 16.6 or 16.7 ms: 60 Hz held. The headroom is 16.7 minus the larger (or, serial, the sum).
2. SH-4 within a few ms of the frame: **SH-4 bound**. Cut vertex work first (section 3). In the street demo's columns "SH4" is the CPU less the PVR waits.
3. PVR (screen plus any target passes) within a few ms of the frame: **PVR bound**. Cut passes and fill first (section 3).
4. Frame near SH-4 + PVR: the program serialises them; overlap them, or cut either.
5. Frame well above both and above their sum: **waits**. Look at how you wait for the render (PVR-117).

## 2. What features cost (measured)

| Feature | Cost | Where | Status | Ledger |
|---|---|---|---|---|
| A render target pass, any size | about 3.2 to 3.3 ms of PVR: 3,213 us at 320 × 240 empty, 3,268 at 128 × 130, 3,256 for a 128 × 128 target; mip-a's 320 × 240 passes 3.3 to 3.9 ms | PVR | MEASURED | PVR-004, PVR-140, PVR-156, PVR-170 |
| The same pass cut to its own tiles | 3,312 → 375 us (c4's map), 3,256 → 344 us (ao4's target M), the same picture | PVR | MEASURED, two programs | PVR-092, PVR-156 |
| The street demo's mirror pass | about 3.2 ms at 128 × 64, 256 × 128 and 512 × 256 alike: the floor, not the pixels (cause ESTIMATE) | PVR | MEASURED (keys pressed in two of the three stages) | PVR-175 |
| The D1 depth fade (a camera depth map at 320 × 240) | +3.69 ms of frame: the map pass 4.9 ms of PVR and 3.2 ms of SH-4 send; the fade's own quads +22 us | both | MEASURED, one run | PVR-172 |
| Trilinear (two passes, the same vertices) | +3.27 ms PVR and +0.74 ms SH-4 over bilinear at 640 × 480; +0.48 ms PVR at 320 × 240 | both | stage means MEASURED; the differences DERIVED | PVR-180, PVR-170 |
| Bilinear over nearest | +0.78 ms PVR at 640 × 480 | PVR | stage means MEASURED; the difference DERIVED | PVR-180 |
| Mip mapping off | +1.43 ms on a 320 × 240 bilinear pass; a 1024² texture shrunk 16 times with no mips: 23.5 ms (21 times 1:1) | PVR | MEASURED | PVR-170, PVR-008 |
| A mip bias toward a sharper level | +1.0 to +1.5 ms PVR at 640 × 480 (why: OPEN) | PVR | stage means MEASURED; the difference DERIVED | PVR-180 |
| A planar reflection lookup, a vertex | 0.92 to 0.97 ms in c5 for the car body, whose looked-up count is 320 from the code, not from the console: about 2.9 to 3.0 us a vertex (DERIVED); c7 0.59 to 0.65 ms (about 2.0 us if still 320; the count not checked in c7) | SH-4 | the frame's lookup time MEASURED; per vertex DERIVED | PVR-160, PVR-174 |
| A baked AO byte a vertex | +222 / +280 us a frame for 2,142 / 2,544 vertices in ao4-i (two runs of this build); +69 / +113 us in an earlier build (cause OPEN, Q91) | SH-4 | MEASURED | PVR-176, SH4-082, SH4-077 |
| Textured multiply quads on screen (the hens' contact shadows) | +306 us for 14,996 pixels, about 20 ns a pixel (DERIVED); 26 ns in an earlier run | PVR | MEASURED | PVR-176, PVR-154, PVR-141 |
| A full-screen translucent layer, 640 × 480 | +3.1 ms each (1, 2, 4, 8 layers: 6.3, 9.4, 15.6, 28.1 ms) | PVR | MEASURED | PVR-002 |
| Translucent triangles stacked in one tile | 256 sorted: 6.4 ms in one tile against 82 us spread | PVR | MEASURED | PVR-009 |
| The hero's shadow pass | +2.08 ms SH-4, +5.05 ms PVR, +2,594 vertices | both | MEASURED, three runs | PVR-178 |
| A skinned hero's skin | 0.81 ms for 1,465 vertices (about 553 ns a vertex, DERIVED) | SH-4 | MEASURED, two runs | SH4-079, SH4-084 |
| Vertex transform load, the street map | about 54,000 vertex transforms a frame (screen 36,500, mirror 17,400) against about 9,000 on the old street: SH-4 bound at 110 ms; about 2 us a transformed vertex on the whole path (ESTIMATE), where `mat_transform` alone is 13 to 43 cycles (0.07 to 0.21 us) | SH-4 | frames MEASURED; counts FACT (host); per vertex ESTIMATE | PVR-160, SH4-086, SH4-020, SH4-050 |
| The same street after box detail levels past 50 m (15 m in the mirror) and shorter draw distances | 110 → 25 ms a frame, about 4.4 times; several changes landed at once, so the share of each is not measured | SH-4 | frames MEASURED; the attribution ESTIMATE | PVR-160, PVR-174; commit 14fd7b0 |
| A vertex sent to the TA | 260 ns by `pvr_prim`, 142 ns in a store-queue run | SH-4 | MEASURED | BUS-001, BUS-003 |
| Video memory → RAM by PVR-DMA | 37.6 to 38.1 MB/s ended by its interrupt (512 KB in 13.7 ms); 36.1 MB/s in another program; 25.4 MB/s polled in s7 (why: OPEN, Q92) | bus | MEASURED | BUS-021, BUS-050, BUS-053 |

## 3. The practices that follow

| Practice | Why (number) | Status | What to measure on your own console | Our test that measures it |
|---|---|---|---|---|
| **Cut every render target pass to its own tiles.** | A target pass costs the screen's whole tile table, about 3.2 ms, whatever its size; cut, 344 to 375 us with the same picture | PROVEN for two passes (PVR-092, PVR-156); ADVISED for the mirror, the depth map and the shadow target (X43, Q97) | The pass's PVR time with and without the cut, and the texel sum of both | ao4 (target M's last two stages), c4 (the map pass) |
| **Count your target passes before anything else.** | Each is about 3.2 ms of a 16.7 ms frame, uncut; the D1 fade's depth map alone is 4.9 ms | PROVEN (PVR-004, PVR-172) | `pvr_get_stats` render time a pass | d1 (T3), ao4, pl4m (PVRm) |
| **Keep per-vertex SH-4 work to near geometry.** Box or drop detail past a distance, shorten the mirror's draw distance. | 54,000 transforms a frame held the street demo at 110 ms; with detail levels and shorter distances, 25 ms | PROVEN that the frame fell (PVR-160, PVR-174); ADVISED which change did it | The SH-4 column against a detail-distance stage, one change at a time | pl4m (the lod30 stages; run them with no key pressed) |
| **Limit per-vertex lookups and bytes to the vertices that need them.** Look up only the reflecting body, not the world. | About 2.0 to 3.0 us a looked-up vertex (DERIVED from MEASURED lookup times and a vertex count read from the code, PVR-160, PVR-174); the AO byte 32 to 110 ns a vertex (SH4-077, SH4-082) | ADVISED: the per-vertex costs are DERIVED, not MEASURED a vertex | us a vertex at your own count | pl4m (look column), ao4 (place / light off / AO byte) |
| **Mipmap every minified texture.** | Mip off +1.43 ms on one 320 × 240 pass; no mips at 16:1 is 21 times the 1:1 cost | PROVEN (PVR-170, PVR-008) | The same pass with mips on and off | mip-a (stage 9), corr-a2 |
| **Use trilinear only where the steps show.** It is two passes. | +3.27 ms PVR and +0.74 ms SH-4 at 640 × 480 (DERIVED from MEASURED stage means) | PROVEN (PVR-180's stage means); the blend's look OPEN (PVR-182) | Trilinear against bilinear, the same view | corr-a2 (stages 2, 3, 7), mip-a (T2) |
| **Treat a sharper mip bias as a cost.** | +1.0 to +1.5 ms PVR at 640 × 480 (DERIVED from MEASURED stage means) | PROVEN (PVR-180's stage means); why OPEN | Each bias code's PVR time | corr-a2 (stages 4 to 6) |
| **Prefer 128 textures where 256 adds nothing.** | Bilinear on 128 textures 5.39 ms against 6.17 on 256 | ADVISED: the 128 stages had a key pressed (PVR-180) | The same stage at both sizes, no key | corr-a2 (stages 10, 11) |
| **Count your full-screen translucent layers.** | +3.1 ms each at 640 × 480 | PROVEN (PVR-002) | Render time with 0, 1, 2 layers | bench h1, spike c1 |
| **Spread translucent triangles; never stack them in a tile.** | 256 sorted in one tile 6.4 ms, spread 82 us | PROVEN (PVR-009) | The worst tile's count (the page's bT column) and the render time | bench h1; corr-a2 (bT) |
| **Keep filtered opaque polygons a tile low when a texture is non-twiddled or a bump map.** Sega lists a hang at the 8th filtered opaque polygon a tile (16th translucent), counted per tile. | REPORTED only (PVR-071, errata E3); we do not test hangs on the console | ADVISED: at most 7 a tile with bilinear on a non-twiddled or bump texture; point sampling ran with no hang (PVR-070) | None: a hang test is not run on the console | — |
| **Twiddled, mipmapped textures: no such cap seen.** | Up to 60 filtered opaque polygon boxes on one 32 × 32 bin, bilinear and trilinear, 361 frames a stage, no hang; mip-a at 6 a tile row, no hang | MEASURED, one run (PVR-181, PVR-170); the counts are box upper bounds | The page's bO column (most boxes on a bin) and the waits given up | corr-a2, mip-a |
| **Poll the render-done wait where you wait on every frame.** | Sleeping: up to 10 ms late a pass; polled: 7 to 8 us, frame 39.9 → 23.3 ms; no gain where the wait ran on one frame in six | PROVEN both ways (PVR-117, PVR-136) | The frame with the wait sleeping and polled | c4g (test 8); the town (a3e-poll) |
| **Send in store-queue runs.** | 142 ns a vertex against 260 by `pvr_prim` | PROVEN (BUS-001, BUS-003) | ns a vertex sent | bench h1, h2 |
| **Keep the TA's vertex buffer under full.** | The town's start view filled the 1,024 KB buffer on every frame and lost the end of the list (PARAM on 150 of 150 frames) | PROVEN that it fills (PVR-149, VID-011); the 90 % shed point is a design choice, ADVISED | The page's TA KB and peak, and PARAM events | the town (hud1c), every suite page (TA columns) |
| **Send what must be seen early, or give it its own room.** A full vertex buffer drops the end of the list without a message: the HUD and any inset drawn last are simply not on screen. | vc-a's ground-view stages read the 1,024 KB buffer 100 % full, and for 20 s neither the HUD nor the map inset was on screen; both showed again with the camera on the sky (PVR-185) | ADVISED: the buffer reading and the missing overlays are MEASURED (one run); that the full buffer drops them is an ESTIMATE from PVR-149 | Whether the HUD shows with the buffer full, with the HUD sent first and sent last | the town (vc-b1, vc-b2: 1,536 KB buffer, PARAM events a stage) |
| **Add up video memory before you add a target.** | 8 MB in all; 4,554 KB free after `pvr_init` with two 1,024 KB vertex buffers, 4,010 KB after the town's load; the street demo 3,666 KB free with a 128 KB target. A target is pitch × height × 2 B (the street demo's is twice the mirror's width: 128 KB at 256 × 128) | PROVEN (VID-012, PVR-161, PVR-175) | `pvr_mem_available()` after load, each target size | pl4m (VRAM column), the town's boot line |
| **Give every shadow map a maximum distance, and fade the shadow out before it.** Never an endless-range map. About 50 m, full strength to 40 m, both tunable. With a tile a caster: fade the caster's strength, and release the tile only at fade 0. With a grid round the camera: feather the map's edge. | The town's HUD ratio is the ground a map texel covers over the ground a pixel covers. At 60 m it is 0.5 to 1.6: the far ground gets texel detail at about a pixel. At 4 m it is 3.3 to 29.1: one texel smears over many pixels, on the near ground where shadows are looked at. Far casters still cost their send and their place in the map. A shorter range puts the map's texels nearer (ESTIMATE for a uniform fit) | ADVISED: the owner's design call. **40 m and 50 m are chosen, not measured.** The ratios are MEASURED (HUD, one run each: PVR-108 to PVR-113) | The same view with the range at 50 m and unlimited: the frame, the map pass's PVR time, the TA vertices sent to it, the HUD ratios at 4 and 10 m; and a drive past the fade edge for a pop | the town (route A, the TEXEL/PX line); the street demo (tiles within 50 m, `demo/pl4nar/src/demo.c:39`) |
| **Read back by PVR-DMA, ended by its interrupt.** | 37.6 to 38.1 MB/s; polled it ran at 25.4 MB/s in one program (Q92) | PROVEN (BUS-021, BUS-053) | MB/s at your size | pvr-dma (pd), h2 |

## 4. A short checklist

1. Read the frame, SH-4 and PVR of every stage (section 1) before you tune anything.
2. Count target passes: each is about 3.2 ms until cut. Give every shadow map a maximum distance with a fade.
3. Count vertices transformed a frame; cut far ones first.
4. Mipmap; use trilinear and a sharper bias only where they show.
5. Count full-screen layers and the worst tile's translucent triangles.
6. Watch the TA's peak and video memory free.
7. Measure on the console: an emulator number is not evidence.

## 5. The town: where the frame goes, and the fixes ranked

The town is the slowest scene we have. This section is Senior's costing (2026-11-25), from the town's autorun pages and code reading. **Every gain below is an ESTIMATE** until vc-a runs: a town image with timers only, built and not yet run. The measured rows each gain leans on are named beside it, in their own column.

### The measured base

Still camera at the boot view, mean ms, one console, one run each (TOWN-119; TOWN-099 and PVR-136 agree within about 1 ms). The TOWN rows are in `demo/psm/TOWN_NOTES.md`.

| Route | Frame | SH-4, the HUD's parts | PVR | TA vertices | Not in any timed part (DERIVED) |
|---|---|---|---|---|---|
| A (the PVR draws the map) | 57.1 | 13.0: build 2.8, map 6.3, uv 4.0 | 3.8 on these pages (see Q98) | 30,613 | 44.1 |
| B (the SH-4 draws the map) | 76.8 | 43.3: build 2.9, clear 0.7, casters 15.4, compare 24.3 | 8.8 | 18,832 | 33.5 |

- **Turning the camera** takes the build part from 2.8 to 21.8 ms on both routes (TOWN-099, TOWN-119): MEASURED.
- **Route B's caster raster and compare** are 39.7 of its 43.3 ms of SH-4, and change little with the view (TOWN-099, TOWN-119): MEASURED.
- **The untimed share** (frame less the HUD's SH-4 parts) is 33 to 44 ms of the frame: DERIVED. Waits, the colour pass, the send count and the two list sends are not in the HUD's parts.

### The fixes, ranked

| Rank | Fix | What it removes | Gain: ESTIMATE | Measured basis | Test that would prove it |
|---|---|---|---|---|---|
| 1 | A static sun's shadows in a bake, and a shadow method only for what moves. Route B: compare only where a mover's shadow can fall. Route A: a PVR tile for each mover in place of the whole map and the whole ground layer | B: the caster raster and nearly all of the compare. A: the map pass's static casters and the second ground draw | B: 24 to 28 ms of the 43.3 ms SH-4. A: about 6 ms SH-4, about 3 ms PVR, 8,656 vertices and the layer's fill | The bake took the casters part 15.3 → 2.1 ms but kept the whole compare, 24.1 → 29.5 ms (TOWN-102, TOWN-103); the map pass is 3,877 us of PVR (PVR-131) | A town image with the bake and mover-only shadows, against the same script |
| 2 | Cheaper placement while the camera moves: area tests from the stored 1 / w (no reciprocals), one finite check a vertex instead of nine a triangle, placement by object against the frustum | Part of the +19 ms every moving frame pays, both routes | 8 to 12 ms on moving frames | Build 2.8 → 21.8 ms turning (TOWN-099, TOWN-119); the transform alone 13.16 cycles a vertex (SH4-020) | vc-a's turn stages, then the same stages after the change |
| 3 | Measure the frame outside the HUD's parts first: the colour pass, the send count, the two list sends, each wait | Unknown | Unknown: it is 33 to 44 ms of the frame | DERIVED from TOWN-119 | vc-a (built) |
| 4 | Cull model triangles by the screen's sides; no compare for ground vertices in no visible cell | Work in front of the eye but off the screen | Little at the boot view; large looking at a wall, the sky or away | 8,815 of 9,737 model vertices are sent at the boot view (TOWN-048: a host count, ESTIMATE for the share); the rest is code reading | vc-a's sky stages against the boot view, then the same after |
| 5 | The compare kernel: batch transform for the models, a reciprocal for the divide | About half of a compare | About 10 ms on route B if the whole compare stays; little after fix 1 | 24.3 ms over about 9,500 points, about 500 cycles a point (DERIVED; the point count is a host figure) | A bench page of the kernel alone |
| 6 | Tile cut round route A's map pass | The whole-table walk of a render target pass | About 3 ms of PVR a frame, if the frame is PVR bound | 3,256 → 344 us in ao4 (PVR-156); the map pass 3,877 us (PVR-131) | d1-c stage 6 (the same cut on another target), then the town |
| 7 | No shadow work past a range (full strength to 40 m, none past 50 m: the rule in section 3) | Compare or tile work on far receivers | 3 to 5 ms of route B's ground compare; on route A, only the far layer's fill | At 60 m a map texel covers 0.5 to 1.6 pixels' ground (PVR-108 to PVR-113) | vc-a gives the base; the range needs its own image |

**Order of work** (set by lead): fix 3 first (measure), then the tile header on the host, then a decision on fixes 1 and 2 from the run. Fixes 4 to 7 wait.

### Practices the code reading suggests (ADVISED, none measured yet)

- Cull by the screen's sides before lighting and sending, not by the near plane only.
- Keep per-triangle tests free of reciprocals when 1 / w is already stored.
- Do not walk the send twice a frame to count it.
- A static sun's shadows belong in a bake; a map a frame is for movers only.

## Open

- **The town's untimed 33 to 44 ms** (section 5): vc-a will time the parts the HUD does not.
- Route A's PVR time: 15.9 ms on the HUD photos, 3.8 ms on the autorun pages (Q98).
- No run has held the camera on the sky (Q99).
- Whether the mirror pass walks the screen's tile table (Q97), and what the tile cut saves there (X43).
- Why a sharper mip bias costs +1 ms (PVR-180).
- The share of each street-demo change in the 110 → 25 ms fall (one change at a time, no key pressed).
- Whether the corridor's 128 textures are cheaper with no key pressed (PVR-180).
- Why the AO byte cost tripled between two builds (Q91).

## Related

- [Budgets](budgets.md): the unit costs
- [Recommended limits](recommended-limits.md)
- [Header RECOMMENDED block](header-recommended.md): the short form of these practices at the top of each header
- [Scenes](scenes.md)
- [Maturity](../status/maturity.md)
- [Benchmarking](../methods/benchmarking.md)
