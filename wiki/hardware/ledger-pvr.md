---
title: Ledger: PowerVR2 (PVR)
tags: [hardware, ledger, pvr]
updated: 2026-11-25
sources: [demo/psm/bench/NOTES.md, demo/psm/compare/NOTES.md, demo/psm/ROUTE_AB_NOTES.md, demo/psm/PVR_FEATURES.md, demo/psm/POLLY2_NOTES.md]
---
# Ledger: PowerVR2 (PVR)

**What you will learn:** what the graphics chip costs to fill, which per-pixel tricks work on a real console, and where Sega's documents and the console disagree.
**For:** a programmer planning layers, textures and render targets, and anyone building a per-pixel compare on the PVR.

Each row is one fact; the status legend is in [STRUCTURE.md](../STRUCTURE.md). Terms such as [punch-through](../glossary.md), [bump unit](../glossary.md) and [fog table](../glossary.md) are in the glossary.

**Where the numbers come from.** All our runs are one console, one run, read from photos.

| Run | Date | Photos | Write-up |
|---|---|---|---|
| h1 (chip bench) | 2026-11-25 | `logs/benchmarks/PXL_20261125_054414414.jpg` … `054608113` | `demo/psm/bench/NOTES.md` h1 |
| c1 (compare spike) | 2026-11-25 | `logs/PXL_20261125_041608930.jpg`, `041618154` | `demo/psm/compare/NOTES.md:15-90` |
| c3b (bump subtract spike) | 2026-11-25 | `logs/benchmarks/PXL_20261125_054724266.jpg` … `054747805` | `demo/psm/compare/NOTES.md:92-178` |
| c4 (whole chain), pages 6 and 7 | 2026-11-25 | `logs/benchmarks/c4e/PXL_20261125_074541020.jpg`, `074645426`, `074650392` | this page, PVR-090 to PVR-094 (no write-up in `compare/NOTES.md` yet) |
| s2, s3 (route AB spikes) | 2026-11-24 | `logs/PXL_20261125_0237*.jpg`, `0310*.jpg` | `demo/psm/ROUTE_AB_NOTES.md:194-433` |

Rows tagged REPORTED from skmp's repositories are their console's results, cited from our reading in `demo/psm/POLLY2_NOTES.md`; see [outside sources](../research/outside-sources.md). None of their tables is copied here.

## Fill and render time

| ID | Claim | Status | Evidence | Source | Test | Used by |
|---|---|---|---|---|---|---|
| PVR-001 | An empty 640 × 480 frame renders in 3,150 us. The background is a layer like any other. | MEASURED | h1 | bench/NOTES.md:41 | bench h1 | budgets |
| PVR-002 | Each full-screen translucent layer at 640 × 480 adds about 3,112 us; blend and additive cost the same. | MEASURED | h1; c1 (0, 1, 2, 4, 8 layers: 3,151 / 6,266 / 9,379 / 15,605 / 28,056 us) | bench/NOTES.md:42-56; compare/NOTES.md:28 | bench h1, spike c1 | [budgets](../limits/budgets.md) |
| PVR-003 | That is about 99 million blended pixels a second. | ESTIMATE (arithmetic on PVR-002) | — | compare/NOTES.md:65-66 | — | budgets |
| PVR-004 | A 320 × 240 render target: 3,213 us empty, then about 771 us a translucent layer. A target pass costs the same floor (about 3.15 ms) whatever its size. The likely cause is that KOS renders a target through the screen's whole tile set: see PVR-092. | MEASURED (times); cause ESTIMATE (supported by PVR-092, not proven) | c1; c4 tile stage | compare/NOTES.md:29, 67-72; ROUTE_AB_NOTES.md:407 | a target pass through a tile list sized to the target | render targets |
| PVR-005 | Hidden opaque layers cost nothing that shows (0 ± 2 us). | MEASURED | h1 | bench/NOTES.md:42-56, 121-126 | bench h1 | — |
| PVR-006 | First textured layer added, us: opaque 565 twiddled nearest 1,115; twiddled bilinear 2,081; raster nearest 1,984; paletted 8-bit nearest 3,034; 4444 raster bilinear 3,120. Translucent 565 twiddled bilinear 3,989 a layer; 4444 raster bilinear 6,249-6,428; punch-through 4444 raster nearest 4,303 then 3,294. | MEASURED | h1 | bench/NOTES.md:42-56 | bench h1 | texture choice |
| PVR-007 | Why the same 565 bilinear texture costs 2,081 us on an opaque screen but about 877 us over the base of a translucent layer; format and layout are confounded in the 4444 lines. | OPEN | — | bench/NOTES.md:127-131 | separate runs per format and layout | — |
| PVR-008 | A 1024 × 1024 twiddled texture with no mipmaps: 1,087 us at 1 texel a pixel, 23,474 us at 16 texels a pixel (21 times), 3 us at 1/16. Raster: 1,522 / 3,339 / 4. | MEASURED | h1 | bench/NOTES.md:60-61, 132-135 | bench h1 | **use mipmaps when minifying** |
| PVR-009 | 4,080 opaque triangles: 588 us spread over the screen, 1,256 us in one tile. 256 sorted translucent triangles: 82 us spread, 6,385 us in one tile. | MEASURED | h1 | bench/NOTES.md:62-63, 136-137 | bench h1 | particle and overlay limits |
| PVR-010 | The YUV converter: 96 KB of 4:2:0 in, 128 KB texture out, in 519 us (189 MB/s in). | MEASURED | h1 | bench/NOTES.md:87, 157-159 | bench h1 | video playback |
| PVR-011 | KOS's "render done" wait is quantised by the 100 Hz scheduler tick, so a timer round a wait reads 6.8 or 16.8 ms. The true render time is `pvr_get_stats`. | FACT (KOS source) + MEASURED (s3) | s3 | ROUTE_AB_NOTES.md:373-379 | — | [benchmarking](../methods/benchmarking.md) |
| PVR-012 | Translucent sorting off against on. | OPEN | never measured | bench/NOTES.md "Not measured" | — | — |

## Render to texture

| ID | Claim | Status | Evidence | Source | Test | Used by |
|---|---|---|---|---|---|---|
| PVR-020 | A 128 × 128 RGB565 render target renders right (9,216 of 9,216 quad pixels) except 16 pixels. | MEASURED | s2 | ROUTE_AB_NOTES.md:204-208 | spike s2 | route AB, pl4nar, ca |
| PVR-021 | **The last 32 bytes of a render are held back** and written at the next render: always 16 pixels on the last row the pass renders, in its last tile. Fix: render 2 rows taller than the map (130 for 128; 512 bytes of video memory). 129 rows untried. | MEASURED | s3 | ROUTE_AB_NOTES.md:319-329, 386 | spike s3 | every render target |
| PVR-022 | Dither can be off for the target pass alone (FB_CFG_2 0x00000001, then 0x00000009). With dither on, a depth code in the target is destroyed (16-bit: 10,161 pixels wrong, worst 2,081 codes). | MEASURED | s2, s3 | ROUTE_AB_NOTES.md:224-229, 355-359 | spikes s2, s3 | depth codes in a target |
| PVR-023 | A texel reaches a 16-bit target exactly through REPLACE with dither off. 10-bit ramp: exact flat, within 1 code tilted. | MEASURED | s2, s3 | ROUTE_AB_NOTES.md:213-218, 335-337 | — | route AB |
| PVR-024 | 16-bit ramp: codes up to 16,495 exact; from 32,768 every pixel is one code under. A 14-bit code (under 16,384) is always exact. The pattern is not explained. | MEASURED; cause OPEN | s3 | ROUTE_AB_NOTES.md:344-353, 388-392 | — | route AB |
| PVR-025 | The best sample point is the pixel centre (offset 0, 0) in every test. | MEASURED | s2 | ROUTE_AB_NOTES.md:221 | — | — |
| PVR-026 | KOS's target pass draws only to row 479, so a 512 × 512 target is out (and its download alone is 14.8 ms). The TA can address 2,048 × 480. | MEASURED + REPORTED (a KOS issue thread) | s3 | ROUTE_AB_NOTES.md:409; PVR_FEATURES.md:143 | — | map size |
| PVR-027 | A render target takes the main frame's pack mode (a KOS issue: RGB0888 breaks it). Render to texture is 16-bit only. | FACT (Sega §3.4.10) + REPORTED (KOS #1506) | — | PVR_FEATURES.md:62-63, 142, 162 | — | — |
| PVR-028 | ARGB1555 target: the alpha bit is "alpha ≥ the FB_W_CTRL threshold", exact at thresholds 1, 2 and 128. The threshold register read back 00800003. | MEASURED + FACT (Sega) + REPORTED (skmp) | c3b line 2 | compare/NOTES.md:108-111, 143-145 | spike c3b | bump compare chain |
| PVR-029 | ARGB4444 target keeps the top 4 bits of alpha. | MEASURED | c3b | compare/NOTES.md:112, 145 | — | — |

## Per-pixel compare tricks

These rows feed the [bump compare chain](../shadows/bump-compare-chain.md).

| ID | Claim | Status | Evidence | Source | Test | Used by |
|---|---|---|---|---|---|---|
| PVR-040 | A compare by saturating adds in a mask target works, 254 of 256 rows found, but every edge sits **one level off (+1)**. Compare in frame destination alpha gives the same. | MEASURED | c1 | compare/NOTES.md:24-27, 41-50 | spike c1 | — |
| PVR-041 | The +1 comes from the doubling layers: a difference of exactly 1 is lost when doubled. skmp's blend rule (each product shifted right 8 on its own; factor 255 counts as 256) predicts it. | MEASURED (c3b threshold 1 gives +0) + REPORTED (rule) | c3b | compare/NOTES.md:142-143, 158; POLLY2_NOTES.md (8) | — | — |
| PVR-042 | A map the PVR drew in 565 gives about 6 bits (error −2 to +4 levels of 256). | MEASURED | c1 | compare/NOTES.md:25, 51-53 | — | — |
| PVR-043 | A palette entry is one value a scene: written between two draws, both show the last value. | MEASURED | c1 | compare/NOTES.md:32, 58-59 | — | — |
| PVR-044 | A modifier volume is exact (11,008 pixels in shadow, 0 wrong) and its cost did not show (3,151 us with and without). | MEASURED | c1 | compare/NOTES.md:33-34, 60-61 | spike c2 at town scale (built, not run) | — |
| PVR-045 | **The bump unit works as a subtract.** Its level follows 128 + 127·sin(M − Q), where Q is one value a polygon, not interpolated. | MEASURED | c3b | compare/NOTES.md:102-107, 129-135 | spike c3b | bump compare chain |
| PVR-046 | Equal depths (M = Q) are undecided: a bias of at least one code is needed. | MEASURED; cause OPEN | c3b | compare/NOTES.md:136-141; POLLY2_NOTES.md (1) | — | bump compare chain |
| PVR-047 | The bump unit's level curve is a measured table, not a sine; products drop their low 8 bits; the light comes from the last vertex's offset colour as sent. | REPORTED (skmp, their console) | — | POLLY2_NOTES.md (1) | — | bump compare chain |
| PVR-048 | A bump polygon's light persists across tiles and renders until changed; a bump map without the offset bit is lit by the old one. Always set the offset bit. | REPORTED (skmp) | — | POLLY2_NOTES.md (1) | — | bump compare chain |
| PVR-049 | The fog table gives a depth code per pixel, 16 entries an octave, far end first; our reading ran about one 6-bit step low. | MEASURED (c3b) + REPORTED (skmp index rule) | c3b lines 6a, 6b | compare/NOTES.md:117-118, 152-160 | — | depth codes |
| PVR-050 | Stored depth is 1/w cut to 19 significant bits. Modifier volumes always compare "strictly greater". | REPORTED (skmp) | — | POLLY2_NOTES.md (4) | — | — |

## Punch-through

| ID | Claim | Status | Evidence | Source | Test | Used by |
|---|---|---|---|---|---|---|
| PVR-060 | The rule is alpha ≥ reference: references 64, 128 and 192 drew 49,152, 32,768 and 16,384 pixels. | MEASURED (c3b 5a) + REPORTED (skmp) | c3b | compare/NOTES.md:113, 148; POLLY2_NOTES.md (2) | — | motes, alpha cut-outs |
| PVR-061 | A failing pixel is still blended, with alpha 0. With blend ONE/ZERO it is written anyway, which explains c1 and c3b's "every pixel drawn" (5b, 5c). | REPORTED (skmp) + ESTIMATE (our explanation) | — | POLLY2_NOTES.md (2); compare/NOTES.md:115-116 | rerun 5b with SRC_ALPHA / INV_SRC_ALPHA | — |
| PVR-062 | In the punch-through list the depth mode and depth-write-disable do nothing. | REPORTED (skmp) | — | POLLY2_NOTES.md (2) | — | — |
| PVR-063 | The reference register read back c0 after 192 was written. | MEASURED | c3b 5a | compare/NOTES.md:114 | — | do not trust read-back |

## Textures

| ID | Claim | Status | Evidence | Source | Test | Used by |
|---|---|---|---|---|---|---|
| PVR-070 | A non-twiddled render target used as a bump texture with point sampling did not hang, and gave the same result as twiddled. | MEASURED (one run) | c3b 1c, 1d | compare/NOTES.md:119-121, 140-143 | — | bump compare chain |
| PVR-071 | The hang Sega lists (CORE 26) comes only with bilinear filtering, counted per tile (16th quad translucent, 8th opaque or punch-through). | REPORTED (skmp) | — | POLLY2_NOTES.md (6) | — | see [errata](../errata/README.md) |
| PVR-072 | Palette textures are always twiddled; 1,024 entries; one palette format a frame. | FACT (Sega §3.6) | — | PVR_FEATURES.md:66 | — | — |
| PVR-073 | Keep each texture inside a 2,048-byte page. | FACT (Sega §3.6.5) | — | PVR_FEATURES.md:195 | untested | — |
| PVR-074 | KOS sets HALF_OFFSET = 7: pixel centres for coverage, half-texel offset for bilinear only; point sampling is floor(u × size). | FACT (KOS) + REPORTED (effect, skmp) | — | POLLY2_NOTES.md c4 (a) | — | — |

## Coverage and geometry

| ID | Claim | Status | Evidence | Source | Test | Used by |
|---|---|---|---|---|---|---|
| PVR-080 | A triangle with no screen area drew short bright lines on a real console (an emulator drew nothing). | FACT (seen on a console) | — | README.md:1304 | — | [guard](../effects/guard.md) |
| PVR-081 | A polygon header never compiled (command word 0) ends the list; a real console can stop. | FACT (seen on a console) | — | README.md:1305 | — | [guard](../effects/guard.md) |
| PVR-082 | A pixel is drawn when its centre is inside all edges; a sliver can draw nothing; a triangle with three equal corners can fill whole tile rows. | REPORTED (skmp) | — | POLLY2_NOTES.md c4 (e) | — | band cutters |
| PVR-083 | The Z buffer stays on chip per tile; no register writes it to memory. | FACT (no such register in Sega §8.4.2) | — | PVR_FEATURES.md:44, 95 | — | why shadow maps are hard |

## Spike c4 on a console (pages 6 and 7)

The whole [bump compare chain](../shadows/bump-compare-chain.md) on one console, 2026-11-25, one run, read from three photos (see the table at the top). The pages number themselves "p6/6" and then "p7/7". The tile stage was added as a last page, so the page count changed mid-run. Pages 0 to 5 were not photographed with text.

| ID | Claim | Status | Evidence | Source | Test | Used by |
|---|---|---|---|---|---|---|
| PVR-090 | Live view, sun 150°/26°, bias b0 × 100 = 175, bands 2 codes wide, layer on: 1,288 vertices. SH-4 fit + cut + faces 1,404 us, project 857 us. PVR map 3,314 us, mask 6,137 us, screen 9,941 us. Frame 39,933 us. | MEASURED | c4, photo `074645426` | — | c4 live view | [bump compare chain](../shadows/bump-compare-chain.md) |
| PVR-091 | Live view, sun 91°/34°, b0 × 100 = 525, bands 1 code wide, layer on: 2,192 vertices. SH-4 2,272 us, project 1,459 us. PVR map 3,236 us, mask 10,171 us, screen 9,947 us. Frame 49,929 us. | MEASURED | c4, photo `074645426` | — | c4 live view | same |
| PVR-092 | **Tile stage:** the map's whole list rendered in 3,312 us (3,311 again); only the 20 tiles it covers, 375 us. The mask's whole list 6,130 us (6,140 again); its 80 tiles, 3,760 us. The picture was the same all three times, and restoring the 2 saved tile tables (1,806 words each) left 0 words different. | MEASURED | c4, photo `074650392` | — | c4 tile stage | render targets, PVR-004 |
| PVR-093 | So a render target through a tile list cut to the tiles it covers saves 2.9 ms on the 128 × 128 map (89 %) and 2.4 ms on the mask (39 %). | ESTIMATE (arithmetic on PVR-092) | — | — | — | route A, route AB, pl4nar |
| PVR-094 | The frame is 18.3 ms (PVR-090) and 22.8 ms (PVR-091) above the sum of its listed parts. The page says the frame "waits for the picture's turn"; where the rest goes is not known. | MEASURED (frame and parts); the gap OPEN | c4, photo `074645426` | — | log `pvr_get_stats` and each wait apart in the live view | budgets |

c4's PVR-DMA line is BUS-050. The punch-through tests are not on these pages, so PVR-061 and PVR-062 stay as they were.

## Open

Also in [open-questions.md](../open-questions.md): PVR-004 (target floor; PVR-092 points at the tile walk), PVR-094 (c4 frame gap), PVR-007, PVR-012, PVR-024 (one-under pattern), PVR-046 (equal depths), PVR-061 retest, the PVR render time of a caster pass, every c4 page (built, not run).
