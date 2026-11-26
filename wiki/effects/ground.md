---
title: "ground (paving slab texture)"
tags: [effect, texture, procedural, load, tiling]
updated: 2026-11-26
sources: [include/sh4der_ground.h, test/ground_test.c, demo/psm/psm_dc.c, README.md "The ground texture (paving slabs)"]
---
# ground (paving slab texture)

**What you will learn:** how a 64 x 64 RGB565 paving slab texture is made at load from a seed, why it tiles both ways with no chequer, and what it costs (exact in bytes, not measured on a console).
**For:** a programmer who needs a ground or floor texture without a file, and does not want a two-tone chequer.

> **In plain words.** It makes a small square of stone slabs in rows, each slab with its own width and tone, a dark joint between them, and a light edge on each slab. The same seed always gives the same picture. The picture repeats cleanly across its edges. It is made once at load from numbers, so it needs no file.

## Try this first

What it is for: the ground of a square or a street, where a plain colour looks flat and a chequer looks wrong.

1. An array of `PSM_GROUND_N * PSM_GROUND_N` (64 * 64) `uint16_t` texels at load.
2. `psm_ground_slabs(px, 158, 152, 142, 1u)`: the base colour (0 .. 255 a channel) and a seed. The walled square uses these values (`demo/psm/psm_dc.c`).
3. Upload it as an RGB565 texture, not twiddled (8 KB). Tile it; the repeat is the caller's (the square uses a 3 m repeat).

Header: [include/sh4der_ground.h](../../include/sh4der_ground.h). Old path: `demo/psm/psm_ground.h`, a forward.

## What it does

The rows are 6, of heights 12, 10, 13, 11, 8, 10 texels (they sum to 64). Each row's slabs are 10 to 20 texels wide, the last one fills the row, and every row is offset by a hash so no joint runs on. Each slab has a tone 0.90 to 1.06 of the base and a warm or cool shift. The slab's top and left edge catch the light, its right edge does not, the row's bottom is a dark joint, and a few joint texels are broken. A small grain goes over everything.

Every value is an integer hash of (row, slab) or (x, y) with a seed. No float maths: 4,096 texels once at load.

## Call order

1. At load: `psm_ground_slabs(px, base_r, base_g, base_b, seed)`.
2. Upload `px` as the texture (RGB565, 64 x 64).
3. Nothing per frame.

## Cost

| Item | Cost | Status |
|---|---|---|
| Texels made at load | 4,096, integer maths | exact, by the code |
| Video memory | 8 KB (64 x 64 x 2 bytes) | exact, by the code |
| Time at load on a console | not measured | NOT MEASURED |
| Per frame | none: an ordinary texture read | by construction |

## Recommended limits

- Make it at load into RAM; do not regenerate it each frame.
- Use it where a stone look is wanted with no normal or height map (it has neither).
- Keep the repeat wide enough that the 64-texel tile does not read as a pattern (the square's repeat is 3 m).

## Tuning

No `SH4DER_*` macros. The base colour and the seed are the arguments. `PSM_GROUND_N` (64) is the side, fixed by the row layout. `README.md` "The ground texture (paving slabs)" has the same call.

## Maturity

PROTOTYPE v0.1 (`wiki/status/maturity.md` has no row yet). Host test: `test/ground_test.c`: the same seed gives the same texels, another seed others, the x and y seams no harder than the inside, 64 tones (24 required), the red mean within 15 % of the base. It passes on the plain path and on SH4ZAM's software path, with the same texels.
