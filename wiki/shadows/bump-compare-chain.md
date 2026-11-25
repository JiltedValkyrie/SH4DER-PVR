---
title: The bump compare chain (spikes c1 to c4b)
tags: [shadows, pvr, bump]
updated: 2026-11-25
sources: [demo/psm/compare/NOTES.md, demo/psm/POLLY2_NOTES.md, demo/psm/PVR_FEATURES.md]
---
# The bump compare chain (spikes c1 to c4b)

**What you will learn:** how we tried to make the PVR itself compare a shadow map against each pixel, what each console spike showed, and where the chain stands.
**For:** anyone continuing the per-pixel shadow research, or using the PVR's blend, bump or fog units in an unusual way.

## The problem

Route B's compare runs on the SH-4, per vertex (see [PSM routes](psm-routes.md)). A per-pixel compare would give sharp edges on coarse geometry. The PVR has no depth compare against a texture. So we looked for PVR units that can **subtract** two numbers per pixel and **threshold** the result: the blend unit, the [bump unit](../glossary.md), the [fog table](../glossary.md), the 1555 write-out and [punch-through](../glossary.md). `demo/psm/PVR_FEATURES.md` ranks the candidates.

## c1: compare by blending (MEASURED, one console)

Saturating adds, an invert and doublings in a mask target (`compare/NOTES.md:15-90`):

- The compare works: 254 of 256 rows found, one clean edge a row. But every edge is **one level off** (PVR-040). The doublings lose a difference of exactly 1 (PVR-041).
- It needs about 11 translucent layers: at 320 × 240, about 0.77 ms each (PVR-004).
- A PVR-drawn 565 map gives only about 6 bits (PVR-042).
- Punch-through as the threshold drew every pixel. c4 found the cause: the blend was ONE/ZERO, which writes failing pixels too (PVR-061).
- A palette as the threshold is one value a scene (PVR-043).
- A modifier volume was exact and its cost did not show (PVR-044).

## c3b: the bump unit as a subtract (MEASURED, one console)

(`compare/NOTES.md:92-178`)

- The bump unit's level follows 128 + 127·sin(M − Q). M is the map's code in the texture; Q is one value a polygon (PVR-045).
- Equal depths are undecided: a bias of at least one code is needed (PVR-046).
- A render target worked as a bump texture, non-twiddled, point sampled (PVR-070). Sega's errata says this hangs; the outside rule says only with bilinear filtering (errata E3).
- The 1555 write threshold is exact, so the level can be cut to a mask bit at write (PVR-028).
- The fog table gives a depth code per pixel, about one 6-bit step low (PVR-049).

**Result:** two layers where c1 needed eleven, with 8-bit map depth. The catch: Q is per polygon, so a receiver must be cut into **bands** of equal depth code.

## Outside rules

skmp's PowerVR2 model gives the bump unit's exact form, the punch-through rule, the fog index and the blend rounding (REPORTED, their console; `demo/psm/POLLY2_NOTES.md`). Six of seven c3b levels match it. The +1 of c1 is predicted by its blend rule. See [outside sources](../research/outside-sources.md). No table of theirs is copied.

## c4: the whole chain (MEASURED, one console, two runs)

![The c4 chain: a map drawn from the sun (3.2-3.3 ms), a mask pass where the bump unit compares each receiver band against the map and the 1555 write keeps one bit (4.8-10.2 ms by band width), and one screen layer that darkens by the mask (6.5 ms, the difference of two measured screen passes); one console, 2026-11-25, ledger PVR-028, 045, 090, 091, 094-101.](../images/c4-chain.svg)

Program `demo/psm/compare/c4_spike.c`; image `demo/psm/out/cdi/psm-c4-c4.cdi`; design in `compare/NOTES.md:180-244`:

1. The PVR draws the map through a depth-lookup ramp into a 128 × 128 target.
2. A 320 × 240 mask pass uses the map as a bump texture. Receiver faces are cut into bands of equal depth; bias b0 + half a band + half a map texel of slope, b0 1.5; the 1555 threshold makes the mask bit.
3. A screen layer applies the mask.

**Status:** run twice on one console, 2026-11-25. First run: the live view and the tile stage (`logs/benchmarks/c4e/`, write-up `compare/NOTES.md:246-284`). Second run: all six result pages, tests 0 to 7 (`logs/benchmarks/c4_c4/`, write-up `compare/NOTES.md:286-381`). The pages are numbered 1 to 6; page 1 opens with test 0, so nothing is missing.

**What works (MEASURED, one run each):**

- The lookup by depth: each map pixel gets its own depth code, never more than 1 code off; 8 sub-steps cut the wrong pixels from 2,303 to 348 of 16,384 (PVR-095).
- A second pass at equal depth with "equal or nearer" leaves 0 holes (PVR-097).
- The console's map equals the host model's to within 1 code (98.4 % and 99.0 % of pixels equal, PVR-099).
- Rim 0 with the faces grown 0.75 texel (PVR-099).

**What the model got wrong (MEASURED):**

- **The bump level is not the cosine table** the Q values were made from: 784 / 443 / 240 of 40,960 pairs disagree at window 16 / 32 / 64 (PVR-096). That is the acne: 2,687 and 2,502 pixels at b0 1.5 in the two scenes, where the model said 0 (PVR-099).
- **Bias:** on the ground alone at one sun, b0 3 gives 0 acne with bands 1 and 4 wide; 1.5 gives 2,774 and 1,364. b0 acts in whole codes (PVR-098). Bands 2, walls and other suns are untested; the owner used 5.25 and 8.0 in the live view (Q57).
- Edge (false shadow from a nearer face): 971 and 1,906 pixels against the model's 634 and 1,716.

The cure, measuring each window's two edges at boot and making Q from them, was run in c4b and worked (PVR-102, PVR-103; next section). A larger b0 hides the error at the price of shadow that starts further from its caster.

**Costs by band width** (sun 150°/26°, MEASURED; PVR-100, SH4-045):

| Bands | Vertices | SH-4 fit | SH-4 cut | SH-4 send (mask) | PVR map | PVR mask |
|---|---|---|---|---|---|---|
| 1 code wide (616) | 2,464 | 54 us | 2,483 us | 820 us | 3,312 us | 9,054 us |
| 2 wide (322) | 1,288 | 53 us | 1,332 us | 440 us | 3,311 us | 6,132 us |
| 4 wide (173) | 692 | 53 us | 744 us | 251 us | 3,311 us | 5,015 us |

Screen pass: 3,161 us for the scene alone, 9,646 us with the shadow layer.

The projection's c4 cycle counts (132.5 against 138.8 a vertex) are a cache-placing case, its two arrays exactly 256 KB apart; they are not the routine's cost (SH4-046; the routine is SH4-023).

**Live view** (MEASURED, one run each):

| View | Vertices | SH-4 (fit, cut, faces + project) | PVR map | PVR mask | PVR screen | Frame |
|---|---|---|---|---|---|---|
| Sun 150°/26°, b0 1.75, bands 2 wide | 1,288 | 1,404 + 857 us | 3,314 us | 6,137 us | 9,941 us | 39,933 us |
| Sun 91°/34°, b0 5.25, bands 1 wide | 2,192 | 2,272 + 1,459 us | 3,236 us | 10,171 us | 9,947 us | 49,929 us |
| Sun 182°/26°, b0 8.0, bands 4 wide | 676 | 811 + 422 us | 3,301 us | 4,792 us | 9,951 us | 39,929 us |

- **The frame is set by waiting, not by work:** 39.9, 39.9 and 49.9 ms for 19.3, 21.7 and 27.1 ms of work. Why the frame is one of two values 10.0 ms apart is OPEN (PVR-094, Q53).

![The frame waits, c4 and c4b (one console, 2026-11-25): in c4 the frame is 39.9 or 49.9 ms while the SH-4 and PVR work in it adds to 19.3 to 27.1 ms (MEASURED PVR-090, 091, 101, 094); in c4b, over 60 frames, the vblank counts 2 or 3 a frame, a 60 Hz vblank, the frames are 29.9 or 39.9 ms (mean 39.2, not whole vblanks), and the three render-done waits take 36.2 of the 39.2 ms, running 2.9 to 9.1 ms past their renders, while the ready waits take 1 to 40 us and polling them changed nothing (MEASURED); a wait that wakes on a 10 ms clock fits (INFERENCE), the cause is OPEN (Q53, P10), and the next test is a render done polled on render_busy.](../images/frame-wait.svg)

- **Tile stage:** rendering only the tiles a target covers took the map from 3,312 to 375 us, and the mask from 6,130 to 3,760 us, with the picture unchanged (PVR-092). That points at the old 3.2 ms target floor (PVR-004).

![The tile-list cut: rendering a target through only the tiles it covers took the 128 x 130 map pass from 3,312 to 375 us (20 tiles) and the 320 x 240 mask pass from 6,130 to 3,760 us (80 tiles) with the same picture (MEASURED PVR-092, one console, 2026-11-25); the savings of 2.9 ms and 2.4 ms are arithmetic (ESTIMATE PVR-093).](../images/tile-list-cut.svg)

**Worked example: one pass, field by field.** The recipe card below fills in every setting of the c4 mask pass's bump draw, in the order a pixel meets them.

![A PVR pass recipe: the order a pixel goes through (depth, texel or bump level, combine, offset colour, clamp, fog, blend, write-out; the punch-through test in its own list) and a card with every field of one pass, filled in for the c4 mask pass's bump draw, each entry with its status (FACT from Sega's manual or our code, MEASURED from our console).](../images/pvr-pass-recipe.svg)

**Punch-through** (c4 test 6, MEASURED, one run): KallistiOS's header thresholds at alpha ≥ reference; ONE/ZERO writes every pixel (c3b's failure); the tested alpha is texture × vertex (PVR-060, PVR-061, PVR-064). A bump polygon in the punch-through list is thresholded on its level: 10,200 of 20,480 drawn against the cosine table's 10,320 (PVR-065). So a mask could be made with no 1555 target. Not built (ESTIMATE).

PVR-DMA: 36.1 to 37.8 MB/s at 128 KB and 512 KB (BUS-050, BUS-051).

Before the console run, the design was checked against skmp's rules (`POLLY2_NOTES.md`, "Spike c4 checked"). Two risks from that check:

- Their level windows are 1-3 codes narrower at window 32. The console agrees in size (PVR-096).
- A band with three equal corners can fill whole tile rows (PVR-082).

## c4b: measured edges, placing fixed, frame waits, punch-through route (MEASURED, one console, one run)

Image `demo/psm/out/cdi/psm-c4-c4b.cdi`, run by the owner on 2026-11-25. Photos `logs/benchmarks/c4b/PXL_20261125_134720968.jpg` to `135001261` (25: pages p1 to p11, three live pictures, and the same run paged through again). Write-up `demo/psm/compare/NOTES.md:413-463`. What changed from c4: Q made from window edges measured at boot; the projection's arrays placed apart; a frame-wait test; the punch-through mask route behind a switch.

**What it settled:**

- **The window's real edges.** At window 32 no pixel read shadow for differences 99 to 157; the cosine gives 97 to 159. The console is 2 codes narrower at each end (PVR-102).
- **Q from the measured edge ends the acne.** On the ground alone: 0 acne at every b0 from 0 to 3, bands 1 and 4 wide, where the cosine edge needed b0 1.5 (PVR-103). In both scenes at c4b's default b0 3.0, acne is 0 with either edge. The price shows there: real shadow shown lit rises from 6 to 30 and from 16 to 47 pixels, about what the host model gave (17, 39). Edge falls from 546 to 495 and from 1,254 to 1,222.
- **The model now agrees with the console** to within 40 pixels a scene, once it uses the console's own level table (PVR-103). In c4 it was 2,832 / 193 and 2,564 / 137: the disagreement was the cosine table.
- **The placing fix worked.** The projection now takes 60.5 to 61.7 cycles a vertex, against 115.7 to 132.5 in c4: 2.2 times faster. That is bench h2's out-of-cache figure (SH4-048).
- **The frame is the render-done wait.** About 36 of a 39 ms frame. The ready waits are tens of microseconds, and polling them changed nothing. The vblank is 60 Hz; frames are 29.9 or 39.9 ms, not whole vblanks. The waits run past the renders by 2.9 to 9.1 ms, which fits KallistiOS's 100 Hz wake (PVR-011). That is not proven until a polled render-done is timed (PVR-104, Q53).
- **Punch-through is a working mask route.** It gives the same pixels as the 1555 route at two suns, 0 different. It costs 14 to 18 % more PVR time in the mask pass, and needs a green depth pass to tell its pixels apart (PVR-105). Joining several compares with it is untested (Q58).
- c4's PVR times, test 0, test 1's counts and test 6 repeat to within 2 % or to the pixel (PVR-106). PVR-DMA: 37.0 to 38.9 MB/s (BUS-052).

**New and not explained:** the band cut is 22 to 28 % slower than in c4 (3,183 / 1,691 / 908 against 2,483 / 1,332 / 744 us), where by design only the Q set-up changed (SH4-049, Q64).

**Costs by band width in c4b** (sun 150°/26°; PVR-106, SH4-048, SH4-049):

| Bands | Vertices | SH-4 fit | SH-4 cut | SH-4 send (mask) | Project (cycles a vertex) | PVR map | PVR mask |
|---|---|---|---|---|---|---|---|
| 1 code wide (616) | 2,464 | 54 us | 3,183 us | 846 us | 60.8 | 3,292 us | 9,228 us |
| 2 wide (322) | 1,288 | 54 us | 1,691 us | 512 us | 61.7 | 3,287 us | 6,225 us |
| 4 wide (173) | 692 | 55 us | 908 us | 273 us | 60.5 | 3,292 us | 5,124 us |

Live view (PVR-107): sun 302°/42°, b0 5.25, bands 1 wide, measured edge: 3,008 vertices; SH-4 3,957 + 887 us; PVR map 3,248, mask 9,260, screen 9,943 us; frame 49,911 us.

**Next:** lower b0 in the scenes with the measured edge (Q57); a polled render-done (Q53); the cut's slowdown (Q64); two compares joined into one mask (Q58).

## Idea: a cached static map plus small mover maps (T4, ESTIMATE / OPEN)

The owner's idea, not built. Everything here is ESTIMATE or OPEN.

![Idea T4, not built: with a fixed sun, draw the static casters' map once and bake static-on-static shadow at load, then each frame draw only a small mover map cut to its tiles and run three compares (static on mover, mover on static, mover on mover) into one mask; how the bits join, and the break-even number of movers, are OPEN, and the PVR's depth buffer cannot be preloaded from a cached map (FACT PVR-083).](../images/static-sun-split.svg)

With a sun that does not move:

- **Static casters:** draw their sun map once and keep it. Bake static-on-static shadows at load into vertex colours or a lightmap.
- **Movers:** each frame, give the movers their own small map, cut to the tiles they cover. Measured tile cuts: the map pass in 375 us with 20 tiles against 3.3 ms whole; the mask pass in 3.76 ms with 80 tiles against 6.13 ms (PVR-092).
- **A pixel is in shadow where either map says so.**

The four cases:

| Caster on receiver | How |
|---|---|
| static on static | Baked at load. |
| static on mover | The mover's bands cut each frame, compared against the cached static map. |
| mover on static | The static receivers' bands cut once at load, compared against the mover map each frame. |
| mover on mover, and self | The mover map against the mover's own bands, each frame. |

The catches:

- **The PVR's depth buffer cannot be preloaded from a cached map** (it stays on chip per tile, PVR-083). So the static casters cannot simply be merged into one map with the movers without drawing them again. Two maps mean two compares.
- **Each extra pass adds waits.** The c4 frame is 40 ms for 19-22 ms of measured work (PVR-094).
- **The break-even number of movers is unknown.**
- **Joining the compares into one mask is OPEN (Q58).** Three compares must write into one mask without a later one clearing a shadow bit an earlier one set. Today's bands write with blend ONE/ZERO, which overwrites. A write that can only set the bit is needed; the punch-through route (PVR-065) is a candidate: a failing pixel is not written with KallistiOS's blend (PVR-061), so a pass drawing "shadow" only where the test passes would leave earlier bits alone (ESTIMATE; which way round the level must point for that is part of Q58). A second candidate: a destination-alpha mask, which the library's trail pass already uses to choose the pixels a pass lands on (`src/sh4der_trail.c:127-151`; [post-PVR tricks](../research/post-pvr-tricks.md) H1). ESTIMATE.


Why the static part should be baked: band counts on the town (ESTIMATE, host, `demo/psm/TOWN_NOTES.md` TOWN-051 to TOWN-055; see [scenes](../limits/scenes.md#the-town-shadow-demo)):

![Band counts under c4's rules: the whole static town needs 44,577 / 26,969 / 18,097 band vertices at bands 1 / 2 / 4 codes wide, the boot view 11,146 / 7,090 / 5,174, and the movers' patches only 864 / 460 / 260, against 2,464 / 1,288 / 692 in c4's measured scene; the town counts and every cost on top are host ESTIMATEs (TOWN-051 to 055), which is why static shadow goes to a bake and c4 runs for movers only.](../images/town-band-counts.svg)

![The static bake split (ESTIMATE, host, TOWN-060 to 067): static casters on static receivers are baked once a sun at load (a byte a vertex for the models, a 256 x 256 ground lightmap; about 169,000 ray-triangle tests, about 0.04 s at an assumed 50 cycles a test; 8,697 B + 64 KB, sizes FACT from the counts) and cost nothing a frame, while the movers' shadows go through c4 on 12 ground patches (mask pass about 1.1 to 4.1 ms); the share of model vertices in shadow moves with where the ray starts (14:00: 41.6 % from 0.05 m out against 28.5 % from 0.3 m + 0.5 m, screen mesh as casters), which is OPEN.](../images/static-bake-split.svg)

**Test that would settle it:** now that c4b works (PVR-103), and after the polled wait test (Q53), time one static map plus one mover map against the single full map, by `pvr_get_stats` and the frame time.

### c4 live-view pictures

Besides the three text photos, ten more photos, `logs/benchmarks/c4e/PXL_20261125_070703610.jpg` to `..071118010.jpg`, one console, 2026-11-25. They show a test scene (a cube, a box, a pole, a small cube, a sky, a ground plane) with an inset at top left. **No text or numbers are on screen**, and no build tag. What the photos show, as seen:

- Some frames have clean ground shadows and lit faces.
- Others show jagged, notched or stair-step shadow edges, dotted marks along cube edges, and dark patches on faces.
- The inset changes between a diamond and a rectangle with dark blobs or wedges.

Which c4 page or setting each photo shows is not recorded, so these photos are **not ledger evidence**. They are OPEN until the run is written up with its page and settings.

## Open

The same comparator in texture space, where Q is one value for the whole object and no bands are needed (`demo/psm/RECIPES_NOTES.md` R7, R8; ESTIMATE):

![The bump comparator in texture space (recipes R7 and R8, not built): when M is a texture in the object's own u, v (a height or a noise) and Q is one value for the whole object (a water level, a dissolve time), the bump unit's window passes exactly the pixels with |M - Q| <= w, with no bands, no camera depth map and no screen coordinates; the mechanism is MEASURED (PVR-045, PVR-065), the window edges must come from boot measurements (PVR-096, Q56), and the recipes are ESTIMATE.](../images/bump-texture-space.svg)


- The bias with the measured edge: how low b0 can go in the scenes, at more suns (Q57).
- The frame: is KallistiOS's 100 Hz wake the whole render-done wait? (PVR-104, Q53).
- Why c4b's band cut is 22 to 28 % slower than c4's (SH4-049, Q64).
- Tile lists cut to a target in the library's own passes (Q54).
- A cached static map plus mover maps (T4), and how its compares join into one mask (Q58; the punch-through route works for one mask, PVR-105).
- Punch-through: blended with alpha 0 or not drawn; the texture × vertex rounding; the depth mode with an occluder (Q16, Q21).
- PVR-046: why equal depths are undecided.
- c3b's level 28 where the rules say 32 (`POLLY2_NOTES.md:276`).
- c2 (modifier volumes at town scale): built, not run.

## Related

- [PVR ledger](../hardware/ledger-pvr.md)
- [Errata](../errata/README.md)
- [PSM routes](psm-routes.md)
