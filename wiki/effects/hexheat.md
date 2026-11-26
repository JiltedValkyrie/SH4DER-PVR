---
title: "Heat haze hexagons [hexheat]: a few floating hexagons that show the picture behind them"
tags: [effect, hexheat, heat-haze, render-target]
updated: 2026-11-26
sources: [include/sh4der_hexheat.h, test/hexheat_test.c, README.md effect table, demo/pl4nar/src/heat_hex.h, demo/pl4nar/src/demo.c]
---
# Heat haze hexagons [hexheat]: a few floating hexagons that show the picture behind them

**What you will learn:** what the hexheat effect draws, where its picture comes from, and what it costs.
**For:** a programmer who wants a heat haze over a hot far view on the PVR, with no copy of the screen.

## What it does

A few small hexagons float over a hot far view: a road's far end, a runway, a desert horizon. Each one shows the picture
behind it 1 to 5 pixels from where it belongs. That shift is rolled again ten times a second. The hexagons rise slowly, lean,
live about 1.7 s and pop on and off. There is no smooth wave, no blend and no tint (README effect table). The sizes, shifts,
rise, lean and life come from the [heat haze reference note](../research/heat-haze-reference.md).

The header is maths only: the lives, the shift, the band's place and the mesh. The pass that fills the band and the draw are
the client's. The street demo shows both (`demo/pl4nar/src/demo.c`: `hex_band_pass`, `hex_draw`).

Header only. The header's version line reads "PROTOTYPE v0.1" (`include/sh4der_hexheat.h:15`).

## Where the picture comes from

The picture under the hexagons is not read back from the screen. It is drawn a second time, in the same frame:

1. A render pass that already runs (the street demo: its reflection pass) also draws the far end of the world into a
   256 x 128 region of its own target. It uses the screen's camera, so one texel is one screen pixel.
2. The band holds only what is farther than a near distance. The street demo cuts its triangles to the region and to that
   distance on the SH-4 (`gfx_window` in its gfx layer). It does not use the user clip, because the band is opaque-list
   geometry.
3. On the screen's translucent list, each hexagon is a strip of 6 vertices. It samples the band at its own place plus its
   shift: point sampled, blend one / zero, depth tested at the far view's depth, no depth write. What is nearer covers the
   hexagon, so a hexagon never shows a piece of a near thing.

No pass is added, no wait, no copy and no tile cut. Nothing is a frame late.

## Cost

| Item | Cost | Status |
|---|---|---|
| VRAM | the band: 256 x 128 x 2 B = 64 KB of texels. The street demo's targets grow by more than this, because a side must be a power of two: street 64 to 128 KB, map 128 to 256 KB, hills 0 (a spare region at 256 x 128) | as-stated (exact bytes) |
| RAM | 4 B state; the demo's glue 20 B | as-stated |
| SH-4 | 48 slots hashed a frame; the far world sent a second time into the band | counted on the host, below |
| Triangles sent again (host, 600 frames, 256 x 128, day, driving) | street mean 595, most 755 (the mirror alone: 532); map mean 381, most 412 (399); hills mean 369, most 408 (402) | COUNTED on the host, 2026-11-26 |
| TA | about 17 hexagons, 4 triangles each, a frame | counted on the host |
| Console time | not measured | OPEN |

The band's triangles roughly double what the reflection pass sends. Nothing of this has run on a console.

## Pitfalls

- A 2D vertex (w = 1) drawn through the demo's window gets the depth key 1 and covers everything. Draw a background quad
  before the window is set.
- The band and the mirror share a target. Cut the mirror to its own region too, so nothing of it spills into the band.
- The hills' tile cut must keep the band's tiles. The street demo widens the cut while a band is drawn.
- At a size where no 256 x 128 region is free (the hills at 128 x 64 and 512 x 256), the demo draws no hexagons.

## Related

- [Heat haze reference](../research/heat-haze-reference.md): the measured look the numbers come from
- [Planar reflection](pl4nar.md): the reflection pass whose target holds the band
- [Budgets](../limits/budgets.md)
