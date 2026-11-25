# PVR-side compare: notes

Can the PVR decide "is this pixel in shadow" itself? `cmp_model.h` has the idea and a plain C model of it;
`cmp_spike.c` is the test program. The routes, as named in the proposal:

| Route | Idea |
|---|---|
| C1 | compare by saturating adds, an invert and doublings, in a mask render target |
| C2 | the same in the frame's destination alpha |
| C3 | the punch-through alpha test on texture alpha x vertex alpha |
| D | a palette change as the threshold |
| E | modifier volumes (the PVR's own inside / outside test) |
| F | an ID map and an equality test built from C1's steps |

## Hardware result c1

One run on one console, read from two photos of the result pages (`PXL_20261125_041608930.jpg`,
`PXL_20261125_041618154.jpg`). Every number below is from those photos. Nothing here was repeated, so there is no
spread to give.

### What the pages said

| Line | Console | Model (host, not a measurement) |
|---|---|---|
| 1a 8-bit map | err +1..+1, found 254, rag 0, btw 0 | err 0..0, found 255 (three rules); err +2, btw 254 (`ab>>8`) |
| 1b 565 map | err -2..+4, found 252, rag 0, btw 0 | err -3..+3, found 252 |
| 1c gouraud R | err +1..+1, found 254, rag 0, btw 0 | not modelled |
| 2 dst alpha | err +1..+1, found 254, rag 0, btw 0 | as 1a |
| 3 screen 640 x 480 | 0: 3151, 1: 6266, 2: 9379, 4: 15605, 8: 28056 us | none |
| 3 target 320 x 240 | 0: 3213, 1: 3931, 2: 4712, 4: 6267, 8: 9383 us | none |
| 4 punch, ref 64 | 65536 px drawn; differs 39022 / 39192 / 39300 / 3910(cut) | 26514 / 26344 / 26236 / 26428 px drawn |
| 5 palette | A = 001f, B = 001f | none |
| 6 volume | 11008 px in shadow, 0 differ, box x 85-170, y 64-191 | 11008 px, x 85-170, y 64-191 |
| 6 render | 3151 us with the volume, 3151 us without | none |

The last number of line 4 is cut by the page's 50 columns. With every pixel drawn it has to be 65536 - 26428 =
39108; the photo shows its first four digits.

### What that settles

- **C1: the compare works, one level off.** The boundary is one clean edge on every row, two-valued (no pixel
  between lit and shadow at the 6 bits the read-back sees), and sits a constant +1 level from the ideal: the pixel
  goes to shadow where R - M >= 2, not >= 1. The receiver's depth from Gouraud vertex colour (1c) gives the same
  result as from a ramp texture.
  - The +1 matches none of the four model rules. Two causes fit the photo and the run cannot tell them apart: a
    difference of exactly 1 is lost in the invert or never grows in the doublings (it would end at a value too small
    for the 6-bit read-back to see), or the map's texel is sampled one row off. Either way it is a constant, so a
    bias of one level removes it. Not explained further here.
- **C2: destination alpha works the same.** The frame keeps an alpha, the blend factors used read and write it, and
  the result equals C1's. The colour was untouched until the last layer.
- **F and a map the PVR draws itself: 6 bits.** With an RGB565 map the boundary error is -2..+4 levels of 256 (the
  model's -3..+3 with the same +1), and 252 rows found as modelled. A map the PVR renders carries 64 depth levels
  into this compare.
- **C3: not working in this test.** Every pixel was drawn, also where the product is 0, so the punch-through test
  did not act on texture alpha x vertex alpha with the reference that was set. Whether the reference register, the
  header or the list is the cause is not known. With the model's finding (a product of log depths is wrong by up to
  7 levels) the route is dropped rather than debugged.
- **D: one palette a scene.** A palette entry written between two draws shows its last value on both. The palette
  cannot be a per-draw threshold.
- **E: the modifier volume is exact.** 11008 pixels in shadow, none different from the model, the box where the
  model has it. The volume's cost did not show: the render time is 3151 us with and without it.

### Fill cost

- Screen, 640 x 480: (28056 - 3151) / 8 = about 3113 us a full translucent layer, 307,200 pixels: about 99 million
  blended pixels a second.
- Render target, 320 x 240: (9383 - 3213) / 8 = about 771 us a layer, 76,800 pixels: the same rate.
- The cost is linear in the layer count at both sizes (each step fits the slope within 10 us).
- A pass with no translucent layer took 3151 us on the screen and 3213 us in the 320 x 240 target: about the same,
  though the target has a quarter of the pixels. The volume test (a 256 x 256 opaque quad) also read 3151 us. So
  about 3.15 ms looks like a floor of this setup for an opaque-only pass, not a fill cost. KallistiOS renders a
  target through the screen's tile set; whether that is the cause was not tested.

### What it means for the routes

- C1 at 320 x 240 with about 11 layers: about 3.2 + 11 x 0.77 = 11.7 ms of PVR render time a frame for the mask
  alone (from the measured slope; the 11-layer pass itself was not run). At 160 x 120 the slope gives about 5.3 ms.
  The mask then still has to be laid over the screen (one more layer over the lit area, about 3.1 ms at full
  screen).
- C1's depth is 8 bits at best, and 6 bits if the PVR draws the map.
- E has the exact answer and no cost that this test could see, at one volume. Its open questions are scale
  (volumes, triangles, bin memory), self-shadow, overlap, the camera inside a volume and the two-parameter mode:
  spike c2.

### Not measured

- The accumulation buffer's own depth: the read-back is the RGB565 frame buffer.
- Any second console, any second run.
- C1 or C2 on a real scene, the 11-layer pass as one pass, the mask laid over the screen.
- The volume at more than one, and with any triangle behind the near plane.
