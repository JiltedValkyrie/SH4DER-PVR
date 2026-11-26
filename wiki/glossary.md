---
title: Glossary
tags: [glossary]
updated: 2026-11-26
sources: [demo/psm/PVR_FEATURES.md, demo/psm/POLLY2_NOTES.md, AGENTS.md]
---
# Glossary

**What you will learn:** what the words on these pages mean.
**For:** everyone, and especially readers new to the Dreamcast.

Short definitions in our own words. Where a term has a ledger entry, the [hardware ledger](hardware/README.md) holds the facts about it.

## The machine

- **Dreamcast**: Sega's 1998 console. Its parts:
  - an SH-4 processor;
  - a PowerVR2 graphics chip, inside the Holly chip;
  - an AICA sound chip;
  - 16 MB of main RAM, 8 MB of video RAM and 2 MB of sound RAM.
- **SH-4**: the main processor, a Hitachi SH7750-series chip at 200 MHz with a fast single-precision FPU. It has:
  - an 8 KB instruction cache and a 16 KB operand (data) cache, both direct-mapped (Sega, *System Architecture*, Table 2-4);
  - two 32-byte store queues.
- **Holly**: Sega's chip that holds the PowerVR2 core, the tile accelerator and the system bus logic.
- **PVR, PowerVR2, CLX2**: the graphics core. It renders the screen in 32 × 32-pixel **tiles**, keeping each tile's depth and colour on chip.
- **TA (tile accelerator)**: the part of the PVR that takes polygons from the SH-4 and sorts them into per-tile lists in video memory.
- **ISP**: the PVR stage that decides which polygon is visible at each pixel (depth test, coverage).
- **TSP**: the PVR stage that shades a visible pixel: texture, colour, fog, blending.
- **AICA**: the sound chip (made by Yamaha for Sega). It contains an ARM7DI processor, 64 voices and a DSP. See [AICA ledger](hardware/ledger-aica.md).
- **ARM7DI**: the sound chip's processor. It is architecture ARMv3: no Thumb, no 16-bit load or store, no 64-bit multiply (see the ledger).
- **G2**: the bus that joins Holly to the sound chip and expansion devices. It is slow, and shared.
- **KallistiOS (KOS)**: the open-source Dreamcast operating system and library this project builds on.

## Drawing

- **Display list**: the PVR draws polygons by list:
  - **opaque** (OP): hidden pixels cost nothing;
  - **punch-through** (PT): on/off transparency by an alpha test;
  - **translucent** (TR): blended, sorted per pixel ("autosort") or drawn in the order sent ("presort").
- **1/w**: the PVR's depth value. A larger 1/w is nearer. KOS calls it `z` in a vertex.
- **Depth compare mode**: one of 8 tests (never, less, equal, less or equal, greater, not equal, greater or equal, always). "Greater or equal" means "equal or nearer".
- **Blend**: how a new colour joins the old one. The result is source × factor + destination × factor, clamped at 255.
- **Offset colour**: a second vertex colour, added after the texture. The bump unit uses it as its light (K1, K2, K3, Q).
- **Bump unit**: the PVR's normal-map hardware. It gives each pixel a brightness from the texel's two angles (S, R) and four per-polygon numbers.
  - K1: the base level.
  - K2, K3: gains.
  - Q: an angle.

Where the offset colour, the bump unit and the punch-through test sit in a pixel's path, with one pass filled in field by field:

![A PVR pass recipe: the order a pixel goes through (depth, texel or bump level, combine, offset colour, clamp, fog, blend, write-out; the punch-through test in its own list) and a card with every field of one pass, filled in for the Full compare chain [c4] mask pass's bump draw, each entry with its status (FACT from Sega's manual or our code, MEASURED from our console).](images/pvr-pass-recipe.svg)

The bump unit used as a comparator in an object's own texture space:

![The bump comparator in texture space (recipes R7 and R8, not built): when M is a texture in the object's own u, v (a height or a noise) and Q is one value for the whole object (a water level, a dissolve time), the bump unit's window passes exactly the pixels with |M - Q| <= w, with no bands, no camera depth map and no screen coordinates; the mechanism is MEASURED (PVR-045, PVR-065), the window edges must come from boot measurements (PVR-096, Q56), and the recipes are ESTIMATE.](images/bump-texture-space.svg)


  We use it as a per-pixel compare: see [bump compare chain](shadows/bump-compare-chain.md).
- **Fog table**: 128 entries the PVR looks up by each pixel's 1/w. It gives a per-pixel depth code without reading depth back.
- **Render to texture (RTT)**: drawing into video memory that is then read as a texture. 16-bit formats only (565, 1555, 4444, 0555).
- **Pack mode, 565 / 1555 / 4444**: the pixel format written out. In 1555 the top bit is "alpha ≥ a threshold" (FB_W_CTRL).
- **Twiddled**: the PVR's preferred texture order, with the bits of x and y interleaved. "Raster" or "non-twiddled" is row by row.
- **Store queue (SQ)**: two 32-byte write buffers on the SH-4. One burst sends 32 bytes, which is one PVR vertex, to the TA or video memory.
- **DMA**: copies done by a controller, not the CPU:
  - **PVR-DMA** moves between main RAM and video memory, both ways.
  - **ch2 DMA** feeds the TA.
  - **G2 DMA** reaches sound RAM.
- **Cache aliasing**: on a direct-mapped cache, two addresses a multiple of the cache size apart share one line and evict each other. See [SH-4 ledger](hardware/ledger-sh4.md).

## Shadows

- **Shadow map**: a depth picture drawn from the light. A point is in shadow when something in the map is nearer the light than the point.
- **PSM (perspective shadow map)**: a shadow map fitted in the camera's view, so near objects get more map texels.
- **Route A**: the PVR draws the map (a silhouette in a render target); the SH-4 only makes lookup coordinates. No self-shadowing. See [PSM routes](shadows/psm-routes.md).
- **Route B**: the SH-4 draws a 16-bit depth map in RAM and compares it per receiver vertex. See [PSM routes](shadows/psm-routes.md).
- **Route AB**: the PVR draws the map, it is copied back by DMA, and the SH-4 compares. See [PSM routes](shadows/psm-routes.md).
- **Caster / receiver**: what throws a shadow, and what it falls on.
- **Acne**: false self-shadowing on a lit surface, from too little depth precision. A **bias** pushes the compare away from it.
- **Band**: in the bump compare, a strip of a receiver polygon with one depth code, because Q is one value per polygon.

## Our evidence words

- MEASURED, FACT, REPORTED, ESTIMATE, DISPUTED and OPEN: see [STRUCTURE.md](STRUCTURE.md), "Status legend".
- PROVEN and ADVISED: limits backed by a MEASURED number, or not yet. See [Recommended limits](limits/recommended-limits.md).
