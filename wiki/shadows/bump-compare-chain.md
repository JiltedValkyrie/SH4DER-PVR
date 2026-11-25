---
title: The bump compare chain (spikes c1 to c4)
tags: [shadows, pvr, bump]
updated: 2026-11-25
sources: [demo/psm/compare/NOTES.md, demo/psm/POLLY2_NOTES.md, demo/psm/PVR_FEATURES.md]
---
# The bump compare chain (spikes c1 to c4)

**What you will learn:** how we tried to make the PVR itself compare a shadow map against each pixel, what each console spike showed, and where the chain stands.
**For:** anyone continuing the per-pixel shadow research, or using the PVR's blend, bump or fog units in an unusual way.

## The problem

Route B's compare runs on the SH-4, per vertex (see [PSM routes](psm-routes.md)). A per-pixel compare would give sharp edges on coarse geometry. The PVR has no depth compare against a texture. So we looked for PVR units that can **subtract** two numbers per pixel and **threshold** the result: the blend unit, the [bump unit](../glossary.md), the [fog table](../glossary.md), the 1555 write-out and [punch-through](../glossary.md). `demo/psm/PVR_FEATURES.md` ranks the candidates.

## c1: compare by blending (MEASURED, one console)

Saturating adds, an invert and doublings in a mask target (`compare/NOTES.md:15-90`):

- The compare works: 254 of 256 rows found, one clean edge a row. But every edge is **one level off** (PVR-040). The doublings lose a difference of exactly 1 (PVR-041).
- It needs about 11 translucent layers: at 320 × 240, about 0.77 ms each (PVR-004).
- A PVR-drawn 565 map gives only about 6 bits (PVR-042).
- Punch-through as the threshold drew every pixel (explained later: PVR-061).
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

## c4: the whole chain (pages 6 and 7 run)

Program `demo/psm/compare/c4_spike.c`; image `demo/psm/out/cdi/psm-c4-c4.cdi`; design in `compare/NOTES.md:180-246`:

1. The PVR draws the map through a depth-lookup ramp into a 128 × 128 target.
2. A 320 × 240 mask pass uses the map as a bump texture. Receiver faces are cut into bands of equal depth; bias 1.5 codes; the 1555 threshold makes the mask bit.
3. A screen layer applies the mask.

**Status:** pages 6 and 7 have run on one console (2026-11-25, three photos, `logs/benchmarks/c4e/PXL_20261125_074541020.jpg`, `074645426`, `074650392`). Pages 0 to 5 have no photo with text, so the chain's correctness on the console is still unwritten. The host model gave acne 0 in all six cases (ESTIMATE).

What the console showed (MEASURED, one run):

| View | Vertices | SH-4 (fit, cut, faces + project) | PVR map | PVR mask | PVR screen | Frame |
|---|---|---|---|---|---|---|
| Sun 150°/26°, b0 × 100 = 175, bands 2 wide | 1,288 | 1,404 + 857 us | 3,314 us | 6,137 us | 9,941 us | 39,933 us |
| Sun 91°/34°, b0 × 100 = 525, bands 1 wide | 2,192 | 2,272 + 1,459 us | 3,236 us | 10,171 us | 9,947 us | 49,929 us |

- **The PVR side of the chain is about 19-23 ms**; the map is the cheapest part. The mask grows with band count.
- **The frame is 18.3 and 22.8 ms above the listed parts.** The page says the frame "waits for the picture's turn". The rest is OPEN (PVR-094).
- **Tile stage:** rendering only the tiles a target covers took the map from 3,312 to 375 us, and the mask from 6,130 to 3,760 us, with the picture unchanged (PVR-092). That points at the old 3.2 ms target floor (PVR-004).
- PVR-DMA of 512 KB by its interrupt: 14,510 us, 36.1 MB/s (BUS-050).

Before the console run, the design was: Checked against skmp's rules and found consistent (`POLLY2_NOTES.md`, "Spike c4 checked"). Two risks from that check:

- Their level windows are 1-3 codes narrower at window 32.
- A band with three equal corners can fill whole tile rows (PVR-082).

### c4 live-view pictures

Besides the three text photos, ten more photos, `logs/benchmarks/c4e/PXL_20261125_070703610.jpg` to `..071118010.jpg`, one console, 2026-11-25. They show a test scene (a cube, a box, a pole, a small cube, a sky, a ground plane) with an inset at top left. **No text or numbers are on screen**, and no build tag. What the photos show, as seen:

- Some frames have clean ground shadows and lit faces.
- Others show jagged, notched or stair-step shadow edges, dotted marks along cube edges, and dark patches on faces.
- The inset changes between a diamond and a rectangle with dark blobs or wedges.

Which c4 page or setting each photo shows is not recorded, so these photos are **not ledger evidence**. They are OPEN until the run is written up with its page and settings.

## Open

- c4 pages 0 to 5 on a console, written up page by page (Q7).
- The c4 frame gap (PVR-094, Q53).
- Tile lists cut to a target in the library's own passes (Q54).
- PVR-061: rerun c3b test 5b with SRC_ALPHA / INV_SRC_ALPHA.
- PVR-046: why equal depths are undecided.
- c3b's level 28 where the rules say 32 (`POLLY2_NOTES.md:276`).
- c2 (modifier volumes at town scale): built, not run.

## Related

- [PVR ledger](../hardware/ledger-pvr.md)
- [Errata](../errata/README.md)
- [PSM routes](psm-routes.md)
