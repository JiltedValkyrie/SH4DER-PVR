---
title: "Heat haze hexagons [hexheat]: a honeycomb of small hexagons that shimmers over a hot far view"
tags: [effect, hexheat, heat-haze, render-target, screen-to-world]
updated: 2026-11-26
sources: [include/sh4der_hexheat.h, test/hexheat_test.c, README.md "The hexheat effect", README.md "Hexheat tuning", demo/pl4nar/src/heat_hex.h, demo/pl4nar/src/demo.c, demo/pl4nar/host/heat_hex_test.c, demo/pl4nar/README.md "The hexagons"]
---
# Heat haze hexagons [hexheat]: a honeycomb of small hexagons that shimmers over a hot far view

**What you will learn:** how the hexagon heat haze is built (a honeycomb fixed on the screen, a smooth wave that nudges each cell, and a ray to the ground for each cell), where its picture comes from, and what it costs.
**For:** a programmer who wants heat shimmer over a hot far view on the PVR, with no copy of the screen.

> **In plain words.** Hot air over a far road makes the far picture wobble. Here the far part of the picture is cut into a honeycomb of small six-sided tiles. Every tile keeps its own place and is only nudged a little, at most 2 pixels, by a slow, smooth wave that drifts up through the honeycomb. Tiles next to each other move almost the same way, so the picture shimmers. No tile pops in, jumps or flickers. The design and its maths are the owner's.

## Try this first

For heat shimmer over a far view by day, when you already draw into a render target every frame.

1. Keep the header's start values; they are the owner's. `SH4DER_HEXHEAT_S` = 8.0 (a hexagon 13.9 pixels wide and 16 high), `SH4DER_HEXHEAT_LAYERS` = 3, `SH4DER_HEXHEAT_WAVE_HEX` = 7.0 and `SH4DER_HEXHEAT_WAVE_HEX_UP` = 7.0 (the longest wave is 7 hexagon widths), `SH4DER_HEXHEAT_RISE_HEX` = 1.5 and `SH4DER_HEXHEAT_WIND` = 0.35 (README "Hexheat tuning"; include/sh4der_hexheat.h:79-94). ADVISED, NOT RUN.
2. The size of the nudge: `SH4DER_HEXHEAT_PEAK` = 0.25, a quarter of s, which is 2 pixels. `SH4DER_HEXHEAT_OFFSET_MAX` = 0.32 holds every nudge under a third of s, so two neighbours never overlap (include/sh4der_hexheat.h:103, 111). ADVISED. On the host the street scene's largest nudge is 1.86 pixels (test/hexheat_test.c).
3. Where the haze stands: `SH4DER_HEXHEAT_LAYER_FRAC` = 0.125 (the haze layer is one eighth of the screen's height) and `SH4DER_HEXHEAT_D0` = 30.0 m (include/sh4der_hexheat.h:97, 100). The street demo stands the haze 70 m ahead (`HEAT_DIST`) and keeps everything nearer than 64 m out of the band (`HEXH_NEAR_BACK` = 6 m, demo/pl4nar/src/heat_hex.h:20). ADVISED.
4. The band: `SH4DER_HEXHEAT_BAND_W` = 256 by `SH4DER_HEXHEAT_BAND_H` = 128 texels, one texel a screen pixel, in a render target you already fill (include/sh4der_hexheat.h:116-117). Sample it filtered (bilinear). ADVISED.
5. In a driving game: `SH4DER_HEXHEAT_DRIVE` = 0.5 lets the pattern stream toward the camera as the car speeds up (include/sh4der_hexheat.h:109). Read [Stripes, and the speed where they stand still](#stripes-and-the-speed-where-they-stand-still) first. ADVISED.

On a console: NOT RUN. Nothing on this page has run on a Dreamcast. Every count is from the host, and a host count is not a console time.

Header: [sh4der_hexheat.h](../../include/sh4der_hexheat.h). Steps in full: [Call order](#call-order). Other heat and screen effects: [What do you want?](../start-here/what-do-you-want.md) and [Camera and screen](../start-here/genre-camera-and-screen.md).

## What it does

The header is maths only. It gives the honeycomb, the ray to the ground, the wave and the mesh. The pass that fills the band and the draw are yours. The street demo shows both (`demo/pl4nar/src/demo.c`: `hex_band_pass`, `hex_draw`).

Header only. The header's maturity line reads "PROTOTYPE v0.1" (`include/sh4der_hexheat.h:22`): nothing of it has run on a console. This lattice is the second design. The first was a few floating hexagons that popped on and off, each with a shift rolled ten times a second. The owner judged it wrong on 2026-11-26, and the lattice replaced it.

### The honeycomb stays put

The screen is covered by a honeycomb (a lattice) of pointy-top hexagons. The size s is the distance from a hexagon's middle to a corner, in pixels. Every cell has two whole numbers (q, r), and its home on the screen is:

```
x0 = s * sqrt(3) * (q + r / 2)
y0 = 1.5 * s * r
```

A home never changes (`sh4der_hexheat_home`). Each frame a hexagon is drawn at home + offset(t). The offset is worked out fresh from the time. It is never added to where the hexagon was the frame before, and no random number is rolled a frame. The only "random" numbers are the layers' phases: four numbers in a table, chosen once.

Each hexagon carries the piece of the far picture that lies under its home. With an offset of 0 the hexagons put the picture back where it was, so only the offsets show.

### Screen to world: the pick a strategy game does

A screen effect lives on the screen. But heat belongs to the world: it is strongest low over the road and far away. A point on the screen does not know how far away it is or how high. So every cell asks the world: a ray goes from the camera through the cell's home and is met with the ground.

A real-time strategy game does the same sum to find the spot on the ground under the mouse pointer. Here it is `sh4der_hexheat_screen_to_ground` (screen to world on the ground plane). It gives the world point and its distance D from the camera. A ray that is level or points up never lands: the function returns 0 and writes nothing.

Most cells of a far view lie above the road's far line. They look at far houses and sky, so their rays land very far away or never. For those cells the point is taken where the ray crosses an upright sheet that stands across the road at the haze's distance, and h is the ray's height over the ground there (`sh4der_hexheat_screen_to_haze`). In the street scene all 131 drawn cells are on the sheet: from a camera 3.3 m up, the road between the 64 m cut and the sheet at 70 m is about 2 pixels high (host count, test/hexheat_test.c).

On the host the pick gives a known road point back within 0.00013 m at 20 to 150 m, and screen to world to screen comes back within 0.00002 pixels (test/hexheat_test.c, the plain C path).

### The wave (the owner's formulas)

At the world point, with X across the view and Y up, the heat is a stack of N waves:

```
theta_i = k_i (X - U t) + m_i (Y - W t) + phi_i
T'      = sum of a_i sin(theta_i)
k_(i+1) = 2 k_i     m_(i+1) = 2 m_i     a_(i+1) = a_i / 2
```

The waves move up at the rise speed W and sideways at the wind speed U. Each layer is twice as fine and half as strong as the one before. The shimmer is the slope (the gradient) of that stack:

```
dX = -A * sum of a_i k_i cos(theta_i)
dY = -A * sum of a_i m_i cos(theta_i)
```

A says how strong the haze is at that point (the envelope):

```
A = A0 * exp(-h / H) * (1 - exp(-D / D0))
```

h is the height over the road and D the distance from the camera. H is how thick the haze layer is, and D0 is the distance where the haze becomes plain. So the shimmer is strongest low and far, and fades with height. The final place is home + (dx, dy). A hexagon also swells and shrinks a little, like a weak lens:

```
scale = 1 + c * A * sum of a_i (k_i^2 + m_i^2) sin(theta_i)
```

The owner's rules for the numbers, which the start values follow: the largest offset stays under s / 3; the longest wave is 6 to 8 hexagon widths; the rise is 1 to 2 hexagon widths a second; the wind is 0.2 to 0.5 of the rise; the haze layer is 10 to 15 % of the screen's height; the peak offset is 0.25 s. The header tunes A0 for each view, so that the largest offset is `SH4DER_HEXHEAT_PEAK` x s at the foot of the sheet (the road's far line).

What that is in the street scene (arithmetic from the start values and that view's 6.75 pixels a metre at the sheet): a hexagon is 2.05 m wide out there, the longest wave is 14.4 m, the rise is 3.1 m a second, the wind 1.1 m a second and the haze layer 8.9 m thick.

### Back to pixels

The world offset becomes pixels as a small bend of the line of sight: the offset over the point's depth, times the focal length (the screen's scale: how many pixels a slope of 1 covers; 461 in the street scene). A nearer cell's offset is larger by focal / D, and no offset passes `SH4DER_HEXHEAT_OFFSET_MAX` x s.

A hexagon keeps one size on the screen. Hexagons sized by focal / D would each want a different size, and different sizes on homes that never move cannot tile: they would overlap or leave gaps. In the street scene every drawn cell is 64 to 69 m deep, so their sizes in the world differ by under a tenth (arithmetic).

### Where the picture comes from

The picture under the hexagons is not read back from the screen. It is drawn a second time, in the same frame:

1. A render pass that already runs (the street demo: its reflection pass) also draws the far end of the world into a 256 x 128 region of its own target, the band. It uses the screen's camera, so one texel is one screen pixel.
2. The band holds only what is farther than a near distance. The street demo cuts its triangles to the band and to that distance on the SH-4 (`gfx_window` in its gfx layer). The road alone is not cut by distance, so a hexagon that sits over the cut's line still carries road.
3. On the screen's translucent list each hexagon is a strip of 6 vertices. It takes its texels from the band at its home and is drawn at home + offset: no blend, filtered (bilinear), depth tested at the near cut's depth, no depth write. What is nearer covers the hexagon, so a hexagon never shows a piece of a near thing.

No pass is added, no wait, no copy and no tile cut. Nothing is a frame late.

### Turns and driving

`sh4der_hexheat_follow` puts what the camera did since the frame before into the wave's scroll:

- **A turn.** The far picture slides sideways on the screen, and the pattern slides with it. On the host a turn that moves the far picture 9.49 pixels changes a far point's offset by 0.0009 pixels (test/hexheat_test.c).
- **Driving forward.** The pattern streams toward the camera, by `SH4DER_HEXHEAT_DRIVE` of the way driven. 1 would leave the wave standing in the world; the start value is 0.5.

The wave is a pure function of the home, the camera, the time and those two sums. It keeps no other state: 8 bytes.

### Stripes, and the speed where they stand still

Read this before you judge the look. It follows from the formulas above, and the host test pins it.

- **The wave is stripes.** k and m double together, so every layer runs along one line. The field is stripes across that line, and every hexagon slides back and forth along it: with the start values the diagonal, up to the right and down to the left. Hexagons along one stripe move together. `SH4DER_HEXHEAT_WAVE_HEX_UP` turns the line; it does not end the stripes.
- **The stripes drift.** They cross the view at the rise plus the wind's share: 28 pixels a second with the start values (arithmetic).
- **A car's speed works against the rise.** Driving streams the pattern down the screen, toward the camera. A slow car sees the stripes climb and a fast car sees them run down. At one speed they stand still, and every hexagon holds its offset: 8.3 m a second (30 km/h) in the street scene's view with the start values. On the host the largest change a frame is 0.0000 pixels at that speed, against 0.126 at rest and 0.126 at twice that speed (test/hexheat_test.c).

A layer with a direction of its own would end both the stripes and the standstill. That is not in the owner's formulas, so it is not in the header. It is listed under [Open](#open).

## Call order

The full steps are in `README.md` "The hexheat effect". A frame:

1. `sh4der_hexheat_view_look` (eye, target, up, the field of view, the screen), then `sh4der_hexheat_view_ground` (the ground's plane, the haze sheet's distance, the near cut). A 0 from either: draw no hexagons this frame.
2. `sh4der_hexheat_follow` with the `g` and `eye` of the view before. Not on the first frame.
3. `sh4der_hexheat_band` with the sheet's foot on the screen gives the band's place. Draw the far world into the band in a pass that already runs.
4. `sh4der_hexheat_mesh` gives 6 vertices a hexagon in strip order. Draw them on the translucent list, after the world.

## Cost

Nothing here is measured on a console. COUNTED means a count from a host run of the street demo's own code.

| Item | Cost | Status |
|---|---|---|
| VRAM | the band: 256 x 128 x 2 B = 64 KB of texels. The street demo's targets grow by more than this, because a side must be a power of two: street 64 to 128 KB, map 128 to 256 KB, hills 0 (a spare region at 256 x 128) | as-stated (exact bytes) |
| RAM | 8 B state and a 192 B view; your vertex array (the street demo: 200 hexagons x 6 vertices x 16 B = 19.2 KB); the demo's glue 36 B | as-stated |
| SH-4, the lattice | about 180 cells walked a frame, a ray each; a drawn cell costs two exp, a square root and 3 sine-cosine pairs | counts; time NOT MEASURED |
| Hexagons drawn | 131 a frame in each of the street, map and hills scenes; 4 triangles each, 524 a frame | COUNTED on the host, 2026-11-26 |
| TA | as strips, 786 vertices: 24.6 KB with the header. The street demo sends 4 separate triangles a hexagon (12 vertices), 1,572 vertices: 49.1 KB | arithmetic from the count |
| Fill | 131 hexagons of 166 square pixels: about 22,000 pixels of the translucent list, textured, no blend (7 % of 640 x 480) | arithmetic; PVR time NOT MEASURED |
| Triangles sent again into the band (600 frames, 256 x 128, day, driving) | street mean 599, most 762; map mean 396, most 430; hills mean 385, most 416 | COUNTED on the host, 2026-11-26 |
| The whole reflection pass with the band, same run | street mean 1,151 (532 with no band); map 818 (268); hills 808 (268). More than the band alone: while a band is drawn the mirror's own triangles are cut to its region on the SH-4, and a cut triangle becomes several | COUNTED on the host, 2026-11-26 |
| Console time | not measured | OPEN |

## Recommended limits

All ADVISED: no number here is measured.

- `SH4DER_HEXHEAT_OFFSET_MAX` 0.32 must stay under a third of s, and `SH4DER_HEXHEAT_PEAK` at or under it: the header refuses to compile otherwise (include/sh4der_hexheat.h:121-122). To prove the look: the street scene's hexagon stage on a console, on a television.
- `SH4DER_HEXHEAT_CELLS_MAX` 200 hexagons a frame; the band holds about 180 homes at s = 8 (include/sh4der_hexheat.h:115).
- `SH4DER_HEXHEAT_LAYERS` 3 or 4 (1 to 4 compile). Each layer is one more sine-cosine pair a drawn cell.
- Keep near things out of the band, and never draw the hexagons nearer than the band's cut. Test: the street scene with traffic crossing the far end.

## Tuning

See `README.md` "Hexheat tuning". Turn the knobs in this order: `SH4DER_HEXHEAT_PEAK` (how far a tile moves), then `SH4DER_HEXHEAT_RISE_HEX` and `SH4DER_HEXHEAT_WIND` (how fast the wave drifts), then `SH4DER_HEXHEAT_WAVE_HEX` (how broad the wave is), then `SH4DER_HEXHEAT_LAYER_FRAC` (how high the haze stands). For a car, `SH4DER_HEXHEAT_DRIVE`.

## Pitfalls

- **Point sampling looks like cracked glass.** With a point-sampled band a hexagon's picture moves by whole pixels and its edges show. Sample filtered. The street demo's `-DPL4NAR_HEAT_HEX_POINT` builds the point-sampled form to compare (host pictures only).
- **A soft patch.** A filtered sample moved by a part of a pixel blurs a little. So the far picture under the honeycomb is slightly softer than the sharp picture round it, all the time (host pictures; not seen on a console).
- **Faint edges on a smooth sky.** Where the far picture is a smooth gradient, the hexagons' edges can show faintly (host pictures; not seen on a console).
- **A 2D vertex in the window.** A 2D vertex (w = 1) drawn through the demo's window gets the depth key 1 and covers everything. Draw a background quad before the window is set.
- **One target, two pictures.** The band and the mirror share a target. Cut the mirror to its own region too, so nothing of it spills into the band.
- **The tile cut.** The hills' tile cut must keep the band's tiles. The street demo widens the cut while a band is drawn.
- **No room, no hexagons.** At a size where no 256 x 128 region is free (the hills at 128 x 64 and 512 x 256), the demo draws no hexagons.
- **The fast exp.** On the Dreamcast path the envelope uses SH4ZAM's fast `exp`, good to a few per cent. On the host an offset differs from a double-precision check by at most 0.029 pixels (test/hexheat_test.c, the SH4ZAM path). A test of your own needs that much room.

## Open

- **A console run.** The time of the band and of the hexagons; the look on a television. Test: the street scene's hexagon stage (`XS_HEX`) against its neighbour stage.
- **Is a filtered sample the same picture?** With an offset of 0, does a bilinear sample at one texel a pixel give the picture back bit for bit on the PVR? Test: the hexagon stage with `SH4DER_HEXHEAT_PEAK` near 0, compared with the stage before it.
- **The stripes and the standstill.** Whether the look wants a direction for each layer (see above). The owner's call; the formulas would change.
- **The still-camera rule.** The street demo shows no heat while its camera moves, so in a normal drive the streaming is never on screen. Only the autorun's hexagon stage, which turns that rule off, shows it.
- **Haze nearer than the band.** The envelope gives haze from about 30 m (`SH4DER_HEXHEAT_D0`), but the band holds only what is farther than 64 m, so nothing nearer shimmers. A nearer cut would put near things (traffic, lamp posts) in the band.

## Related

- [Heat haze reference](../research/heat-haze-reference.md): the video the first design was measured from
- [Planar reflection](pl4nar.md): the reflection pass whose target holds the band
- [Screen warp](warp.md): heat as a mesh over a copy of the frame
- [Camera and screen](../start-here/genre-camera-and-screen.md), [Budgets](../limits/budgets.md)
- [License and credit](../start-here/license-and-credit.md)
