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

## Hardware result c3b

One run on one console, read from photos of the result pages (`logs/benchmarks/PXL_20261125_054724266.jpg`,
`..054728965.jpg`, `..054736434.jpg`, `..054747805.jpg`). Every number below is from those photos. Nothing was
repeated. The program is `sub_spike.c`, the models are in `sub_model.h` (models, not measurements).

### What the pages said

| Line | Console | Model |
|---|---|---|
| 1a RGB, row 128, x 32 96 124 128 132 160 224 | 255 255 255 255 255 255 255 | none |
| 1a A (the same through alpha) | 125 125 125 125 125 125 125 | a fall from about 226 to about 48 |
| 1a Q by vertex | err -101..-101, found 127, rag 100, btw 0 | err +0..+0 |
| 1b RGB | 255 x 7 | none |
| 1b A | 223 207 142 109 109 28 48 | 226 209 140 116 116 30 47 |
| 1b Q in 32 strips | err -5..+3, found 253, rag 0, btw 0 | err -4..+3 |
| 2 thr 1 | err +0..+0, found 255, rag 0 | +0 |
| 2 thr 2 | err +1..+1, found 254, rag 0 | +1 |
| 2 thr 128 | err +127..+127, found 128, rag 0 | +127 |
| 2 register while set | 00800003 | pack mode 3, threshold 0x80 |
| 2 4444, row 0 | alpha over 0 from x = 16; 15 at x = 255; pixel ffff | 16; 15 |
| 5a 4444, ref 64 / 128 / 192 | 49152 / 32768 / 16384 drawn | the same for "alpha >= reference" |
| 5a reference register | reads c0 | the last value written (192) |
| 5b product, ref 64 | 65536 drawn; differs 39022 39192 39300 39108 | 26514 26344 26236 26428 drawn |
| 5c bump, ref 128, 32 strips | 65536 drawn; no edge found | err -4..+3 |
| 6a ramp table, x 1 3 7 15 31 63 127 255 | 12 52 89 125 158 190 223 255 | 16 56 92 126 159 191 223 255 |
| 6b top octave, x 0 64 128 192 255 | 0 60 125 190 251; steps back 0; most off the line 5 | 0 65 130 195 255 |
| 1c raster texture (after START) | err -5..+3, found 253, rag 0, btw 0 | err -4..+3 |
| 1d codes the PVR drew | 0 of 65536 wrong | 0 |
| 1d PVR target as the bump texture | err -5..+3, found 253, rag 0, btw 0 | err -4..+3 |

The "A" row for 1b is the model for the strip each sample falls in (Q = 36, 100, 124, 132, 132, 164, 228 against
M = 128), worked out here from `sub_bump_level`; the read-back has 6 bits, so a value stands for itself and the
three above it.

### What that settles

- **The bump unit is a subtract.** With Q one value a polygon the level follows 128 + 127 sin(M - Q) (1b: five of
  seven samples inside one 6-bit step of the model; the other two are one strip, below) and the binary edge is the model's on every row but those where
  M equals Q. The level is in alpha; the colour is white.
- **Q is one value a polygon, not interpolated.** 1a gave the same level across the whole quad though its left
  vertices carried Q = 0 and its right ones Q = 255. 125 at M = 128 is the level for Q = 255: both triangles of the
  strip took the Q of their last vertex.
- **Equal depths are undecided.** The model's +3 rows and the console's extra -5 rows are the same rows, those
  where M = Q for one strip: on some the strip read lit, on others shadow. A difference of one code either way read
  correctly. So the compare is exact to +-1 code and a bias of at least one code is needed.
- **One strip is low.** At M - Q = -4 (both samples in that strip) the level read 109 (108 to 111) where the model has 116. The sign is right;
  the size on the negative side is not the model's. Not explained; the PVR's own sine is not known.
- **A bump texture need not be twiddled, and a render target can be one.** 1c and 1d gave the same numbers as 1b,
  and the PVR drew all 65,536 codes of the target exactly. Sega's errata list says the CORE hangs on a bump texture
  that is not twiddled; on this console, in this test, it did not. One run: not a proof that it never does.
- **The frame buffer write does the threshold.** With the target written as ARGB1555 the alpha bit is "accumulated
  alpha >= threshold", exact at 1, 2 and 128. The register is not touched by KallistiOS between the lists and the
  render. The ARGB4444 target keeps the top four bits of alpha.
- **c1's +1 was the doublings.** The threshold at 1 puts the edge at +0 with the same add and invert layers, so a
  difference of exactly 1 is there in the alpha and is lost only when it is doubled.
- **Punch-through: the rule is alpha >= reference** (5a), and the reference register holds what is written.
- **Punch-through did not act on vertex alpha times texture alpha, nor on the bump level** (5b, 5c: every pixel
  drawn). 5a's header was KallistiOS's default for the list; 5b's and 5c's had the depth test at "always", no depth
  write and the blend at one / zero, and 5b's texture was paletted. Which of those stops the test is not known.
- **The fog table gives a depth code per pixel.** 6a: the table is addressed by 16 entries an octave of scaled
  1 / w, the far end first, as documented. 6b: across one octave the level rises with no step back, one level a
  pixel.
  - 6a is one 6-bit step low at the four dark samples and equal at the four bright ones. A multiply that takes 1.0
    as 255 / 256 does that (16 becomes 15, 56 becomes 55, 92 becomes 91, 126 becomes 125). Fits; not proven.
  - 6b is one step low everywhere, 251 at the end where the model has 255. KallistiOS packs the fog scale with 7
    bits of mantissa, cut, so the 255.99 asked for is 255.0 in the register; with that scale and the same lost
    level the model gives 62, 126, 189 and 251 at x = 64, 128, 192, 255: the console's 6-bit values at all four.
    Fits; not proven.

### What it means

- A shadow compare with no blend chain: the map as a bump texture (8 bits, drawn by the PVR), the receiver's depth
  as Q, the result in alpha, the threshold at the target write. Two layers where c1 needed eleven, and the map has 8
  bits where c1's had 6.
- The price is Q per polygon: a receiver polygon has one depth. It must be small along the light, or cut into bands
  of equal depth.
- The compare is cyclic (256 codes a turn). K1, K3 and the threshold choose how much of the turn reads lit.

### Not measured

- The bump level with K1 and K3 other than 128 and 127, and S other than 0.
- The bump level written straight into a 1555 target (test 1 read it through a multiply; test 2 used paletted
  alpha).
- Punch-through with the bump level under KallistiOS's default header.
- The fog level routed into the low byte of a target.
- Any time. A second run.

## Spike c4: the whole chain (built, not run)

`c4_spike.c`, `c4_model.h`, `test/c4_model_test.c`; image `demo/psm/out/cdi/psm-c4-c4.cdi`. Nothing below is a
console result: it is the design and what the host MODEL says. The console's pages replace it when they exist.

### The chain

- MAP: faces toward the sun, drawn from the sun into a 128 x 128 RGB565 target through a ramp texture; a pixel's
  16 bits are its depth code (0 .. 255, high byte 0). The PVR's depth test keeps the nearest. Faces are grown 0.75
  map pixel in their own plane.
- The ramp. The PVR divides the interpolated u z by the interpolated z. With u = 1 / (2 z) at every vertex, u z is a
  constant, so each pixel gets 1 / (2 z) of its own depth: a lookup by depth, which a ramp texture turns into the
  code (z = 1 - (code + 0.5) / 512). 1024 texels give one texel a code at the near end; a second ramp uses v =
  1024 u to split each texel of u into 8 (8 rows). The MODEL of the plain ramp: about a quarter of the pixels near
  the light can be one code off. Test 0 measures both.
- MASK: 320 x 240 target. An opaque pass for depth, a clear (alpha 0), then the same polygons with the map as a
  bump texture (not twiddled, point sampled), depth test "equal or nearer", no depth write, blend one / zero. The
  target is written as ARGB1555 with the alpha threshold: the alpha bit is the shadow.
- BANDS: Q is one value a polygon (c3b), so each face is cut along lines of equal depth code into bands 1, 2 or 4
  codes wide. Two bands share a corner's bits (the cut is worked out from the face's own corners).
- Window: Q = R + (w - b) + 128; lit while M - R is in (-b, 2 w - b). Threshold for w = 16 / 32 / 64: 10 / 38 / 128.
- BIAS: b = b0 + half a band + half a map texel's depth on that face, in codes. b0 = 1.5.
- SCREEN: one translucent layer textured with the mask (bilinear), black, alpha = bit x strength.

### What the host model says (plain C and SH4ZAM's software back end give the same numbers)

320 x 240 mask, bias rule as above, against a ray to the sun. "acne" must be 0 and is.

| Sun az / el | Window | Grow | Bands 1 / 2 / 4 wide: corners | acne | edge | rim | lit in bias | other | wrap |
|---|---|---|---|---|---|---|---|---|---|
| 150 / 26.5 | 32 | 0 | 2428 / 1252 / 656 | 0 | 76 | 25 | 144 | 0 | 0 |
| 150 / 26.5 | 32 | 0.75 | same | 0 | 707 / 634 / 572 | 0 | 4 / 4 / 13 | 0 | 0 |
| 210 / 45 | 32 | 0.75 | 2866 / 1460 / 768 | 0 | 527 / 446 / 405 | 0 | 0 / 0 / 10 | 0 | 0 |
| 100 / 13 | 32 | 0 | 2572 / 1312 / 692 | 0 | 111 | 654 | 176 | 0 | 0 |
| 100 / 13 | 32 | 0.75 | same | 0 | 1785 / 1716 / 1594 | 0 | 0 | 5 | 4 |
| 100 / 13 | 64 | 0.75 | same | 0 | 1717 / 1650 / 1531 | 0 | 0 | 746 / 726 / 689 | 865 |

- Growing the faces removes the rim (a map pixel whose centre is off a face's edge holds what is far behind it, and
  the cyclic compare reads that as shadow) and costs a wider shadow edge.
- A window of 64 at a low sun wraps: casters more than 256 - 2 w + b codes nearer read lit. 32 does not here.

### What each page line settles

- 0 RAMP: whether the lookup by depth works at all, how many pixels are a code off with each ramp, and whether the
  target holds only codes.
- 1 WINDOW: the bump level against the level table at the three thresholds, straight into a 1555 target (c3b read it
  through a multiply); "holes" = pixels the second pass did not draw at EQUAL depth.
- 3 BIAS: the ground alone, so every shadow pixel is acne: the least b0 with 0 is the bias this console needs.
- 4 SCENE: the map against the model's; the mask against the model run on the console's own map (inside faces and
  at outlines apart); the mask against the ray (acne, edge, rim, in bias, other).
- 5 COST: band cut, projection (asm loop against its C form, cycles a vertex, same bytes or not), sends; PVR render
  time of the map, the mask and the screen with and without the layer.
- 6 PUNCH: which single header setting made c3b's punch-through draw everything; whether the bump level is tested.
- 7 DMA: PVR-DMA 128 KB and 512 KB with the end polled each 1 ms, each 250 us, and taken by interrupt.
- LIVE (START): d-pad the sun, A band width, X / Y bias, B layer; costs are added to the pages on leaving.
- TILES (Y, last): render time of the map and mask passes with a tile list that ends after 20 and 80 tiles, the
  pictures compared, every table word restored and read back.

### Not verified

- Nothing ran on a console. Every PVR behaviour the chain rests on beyond c3b's lines is untested: the lookup by
  depth, "equal or nearer" on a second pass, a 128 x 128 target as a bump texture (c3b: 256 x 256), a 320-wide pass
  into a 512-wide texture, the 1555 texture's alpha bit through a bilinear filter.
- The asm projection's cycle count is an estimate until page 5 prints (19.5 E a vertex against 39).
- The interrupt form of test 7 uses event code 0x000b (the PVR-DMA end bit); KallistiOS has no name for it.

## Hardware result c4: the live view, pages 6 and 7 (pages 0 to 5 not photographed)

Image `demo/psm/out/cdi/psm-c4-c4.cdi`, one console, 2026-11-25. Photos in `logs/benchmarks/c4e/` (`PXL_20261125_070703610`
to `..074650392`; a name below is its last digits). The owner went to the live view first; of the result pages only
the last two were photographed. One line a fact, in the form of `wiki/STRUCTURE.md`; a row marked "seen" is what a
photo of the live view shows, with no number behind it. The ESTIMATE rows are the host model's and say what c4b
changes; no c4b image exists.

| Claim | Status | Photo | Test |
|---|---|---|---|
| The whole chain draws: the map from the sun, the bump compare into a 1555 mask, the mask as a shadow layer on the screen, with the sun moved live. | MEASURED 2026-11-25, 1 console (seen on the photo, not counted) | `..070848079`, `..071055705` | c4 live view |
| At b0 = 1.5 the lit ground shows a regular grid of shadow dots (acne), and dots along outlines. | MEASURED 2026-11-25, 1 console (seen on the photo, not counted) | `..070703610` | c4 live view, default settings; page 3 would give the least b0 with none |
| With b0 raised in the live view the ground is clean at sun 150 / 26. | MEASURED 2026-11-25, 1 console (seen on the photo, not counted) | `..070848079` | c4 live view, X / Y |
| With the sun square to the boxes, thin false shadow lines run from their bases across the ground. | MEASURED 2026-11-25, 1 console (seen on the photo, not counted) | `..070855125`, `..071118010` | c4 live view |
| A wall that the sun only grazes shows blocks of false shadow (a comb down the box's side, a smear under the slab's top). | MEASURED 2026-11-25, 1 console (seen on the photo, not counted) | `..071110100` | c4 live view |
| Shadow edges are stair steps the size of a map texel; at a low sun they are several screen pixels long on the ground. | MEASURED 2026-11-25, 1 console (seen on the photo, not counted) | `..070905851`, `..070955284` | c4 live view |
| Live frame at sun 150 / 26, b0 1.75, bands 2 codes wide, layer on: 1,288 vertices; SH-4 fit + cut + faces 1,404 us, projection 857 us; PVR map 3,314 us, mask 6,137 us, screen 9,941 us; frame 39,933 us (it waits for the picture's turn). | MEASURED 2026-11-25, 1 console, 1 run | `..074645426` | c4 page 6, first LIVE line |
| Live frame at sun 91 / 34, b0 5.25, bands 1 code wide, layer on: 2,192 vertices; SH-4 fit + cut + faces 2,272 us, projection 1,459 us; PVR map 3,236 us, mask 10,171 us, screen 9,947 us; frame 49,929 us. | MEASURED 2026-11-25, 1 console, 1 run | `..074645426` | c4 page 6, second LIVE line |
| PVR-DMA of 512 KB ended by its interrupt: 14,510 us, 36.1 MB/s, the copy right. (Bench h2 on the same console: 13,742 us.) | MEASURED 2026-11-25, 1 console, 1 run | `..074541020` | c4 test 7, last line |
| The tile list can be ended early for a small target: the map pass renders in 375 us with a 20-tile list against 3,312 us with the whole list (3,311 again after), the picture the same all three times. | MEASURED 2026-11-25, 1 console, 1 run | `..074650392` | c4 TILES map line |
| The mask pass: 3,760 us with an 80-tile list against 6,130 us whole (6,140 again), the picture the same all three times. | MEASURED 2026-11-25, 1 console, 1 run | `..074650392` | c4 TILES mask line |
| The two tile tables (at 0f8448 and 4f8448, 1,806 words each) read back as saved after the test: 0 words differ. | MEASURED 2026-11-25, 1 console, 1 run | `..074650392` | c4 TILES restore line |
| Pages 0 to 5 of c4 (ramp, window, bias, scene counts, cost, punch-through) have no photo yet. | OPEN | - | the c4 image's pages 0 to 5, photographed |
| c4b rules (bias = b0 + half a band + a WHOLE texel of slope, b0 3.0; faces whose bias leaves under 4 codes of window are not compared: drawn lit, kept in the map ungrown): 0 acne on a face's own pixels and 0 on a face behind over 360 suns x 3 elevations, where c4's rules gave 170 and 44 at the worst sun. | ESTIMATE: host model `c4_model.h`, 320 x 240 mask against a ray to the sun | - | `test/c4_model_test.c` `kinds()`; on the console: c4b page 3 and page 4 |
| The same rules, false shadow from a nearer face at five suns (150, 177, 174, 93, 96 deg at 26.5): 513, 220, 161, 277, 435 pixels, where c4's gave 634, 925, 733, 1,623, 1,247. | ESTIMATE: host model `c4_model.h`, 320 x 240 mask against a ray to the sun | - | `test/c4_model_test.c` `chain()` |
| Their price: real shadow missed 13, 11, 1, 21, 22 pixels (c4: 4, 2, 1, 367, 129), and at suns 93 and 96 about 864 and 868 pixels of shadow cast ON a grazed wall are not drawn. | ESTIMATE: host model `c4_model.h`, 320 x 240 mask against a ray to the sun | - | the same |
| A band cutter that lets a band level fall on a face corner emits triangles of three equal corners (105 over 360 suns x 3 band widths, because the light fit puts the ground's near corner exactly 7.5 codes in); snapping levels within 1e-3 code to the corner gives 0. | ESTIMATE: host model `c4_model.h`, 320 x 240 mask against a ray to the sun | - | `test/c4_model_test.c` `bands()`, the equal-corner check |
| The dot grid is the bump level table of the chip differing from the cosine the Q values were made from (windows 1 to 3 codes narrower at w 32 by a third party's reading of the hardware). | REPORTED: `../POLLY2_NOTES.md` "Spike c4 checked against skmp's rules" (cite only) | - | c4b: measure the window's own edges at boot from strips of known codes, and set Q from them |

What the cost lines say, as readings: the screen pass is 9.9 ms with the layer whatever the sun; the mask pass grows
with the band count (6.1 ms at 1,288 vertices, 10.2 ms at 2,192); the SH-4's whole share is 2.3 ms and 3.7 ms. The
frame times (39.9 and 49.9 ms) are longer than the work because the frame waits for the picture's turn (the page's
own words): 49.9 ms is three pictures at 60 a second. Why the other is 39.9 and neither 33.4 nor 50.0 is not known;
a mean over frames of two and of three pictures would give it. The work, PVR and SH-4 together, is 21.7 ms and
27.1 ms (sums of the measured lines above).

The tile lines are the first sign that a small target need not pay for the whole screen's tile list: the 128 x 128
map is 16 tiles and rendered in 375 us with a list of 20. The mask (320 x 240, 80 tiles of 32 x 32 hold 320 x 256)
did not gain as much: 3,760 us. Nothing was changed for good: the tables read back as saved.
