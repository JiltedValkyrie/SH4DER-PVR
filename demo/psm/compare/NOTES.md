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
| (was OPEN here: "pages 0 to 5 have no photo".) Closed the same day by a second run: the section "Hardware result c4: the result pages" below. | - | - | - |
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

## Hardware result c4: the result pages (second run, all six pages)

The same image, the same console, 2026-11-25, a second run. Photos in `logs/benchmarks/c4_c4/`: p1 `PXL_20261125_082205102`,
p2 `..082208017`, p3 `..082215888`, p4 `..082218488`, p5 `..082221222`, p6 `..082337928` (taken after the live view,
so it carries the LIVE line). Five more photos of that folder (`..082228971` to `..082329488`) are not transcribed
here. The pages are numbered 1 to 6 and the tests 0 to 7: page 1 opens with test 0, so nothing is missing. One
line a fact; the host model's numbers are not in this table (they are in "What the host model says" above and in
the comparison below it).

The scene and bias tests of this image use c4's rules as built: b0 1.5 in the scenes, and a bias of b0 + half a
band + HALF a texel of slope. A count there is a number of pixels of the 320 x 240 mask (its border row and the
sky left out).

| Claim | Status | Photo | Test |
|---|---|---|---|
| The lookup by depth works: a ramp texture read at u = 1 / (2 z) gives each pixel the code of its own depth. With the plain 1,024-texel ramp 2,303 of 16,384 pixels are wrong, none by more than 1 code; with 8 sub-steps through v, 348, none by more than 1 code. No pixel holds anything but a code. | MEASURED 2026-11-25, 1 console, 1 run | p1 | c4 test 0, the two ramp lines |
| A map of whole codes (row y holds 2y on the left, 2y + 1 on the right) reads back with 0 of 16,384 pixels wrong. | MEASURED 2026-11-25, 1 console, 1 run | p1 | c4 test 0, "whole codes" |
| A PVR-DMA fetch of the 32,768-byte map equals 256 words read straight from video memory. | MEASURED 2026-11-25, 1 console, 1 run | p1 | c4 test 0, "fetch" |
| The bump compare into a 1555 target, every code against 160 Q values (40,960 pairs), disagrees with the cosine level table at: window 16 (threshold 10) 784 pairs, 320 of them within 2 levels of the threshold; window 32 (threshold 38) 443, 320 within 2 levels; window 64 (threshold 128) 240, all 240 within 2 levels. | MEASURED 2026-11-25, 1 console, 1 run | p1, p2 | c4 test 1, the three w lines |
| A second pass at equal depth with the test "equal or nearer" draws every pixel: holes 0 in the window test and 0 inside faces and 0 at outlines in both scenes. | MEASURED 2026-11-25, 1 console, 1 run | p1, p2, p3 | c4 tests 1 and 4, "holes" |
| Ground alone at sun 150 / 26.5, window 32, c4's bias rule (b0 + half a band + half a texel of slope); shadow pixels of 34,928, all of them acne. Bands 1 code wide, b0 0 / 0.5 / 1 / 1.5 / 2 / 3: 31,493 / 16,867 / 16,867 / 2,774 / 2,774 / 0. Bands 4 wide: 15,959 / 1,485 / 1,485 / 1,364 / 1,364 / 0. | MEASURED 2026-11-25, 1 console, 1 run | p2 | c4 test 3 |
| Scene at sun 150 / 26, window 32, bands 2 wide, grow 0.75, b0 1.5: the console's map equals the model's in 16,115 of 16,384 pixels, is 1 code off in 269, more in 0. | MEASURED 2026-11-25, 1 console, 1 run | p2 | c4 test 4, "map vs model" |
| The same scene, 322 bands, 1,288 vertices. Mask against the model run on the console's own map: 2,832 pixels differ inside a face, 193 at an outline. Mask against a ray to the sun: 36,418 lit pixels and 1,378 shadowed; of the lit, 2,687 are shown shadowed by their own face (acne), 971 by a nearer face (edge), 0 by a face behind (rim); of the shadowed, 2 are shown lit inside the bias and 0 elsewhere. | MEASURED 2026-11-25, 1 console, 1 run | p2 | c4 test 4, first scene |
| Scene at sun 100 / 13, the same settings: map equal in 16,226, 1 code off in 158, more in 0; 337 bands, 1,348 vertices; mask against the model 2,564 inside a face, 137 at an outline; against the ray 34,889 lit and 2,907 shadowed; acne 2,502, edge 1,906, rim 0; lit in error 0 inside the bias, 1 elsewhere. | MEASURED 2026-11-25, 1 console, 1 run | p2, p3 | c4 test 4, second scene |
| Cost at sun 150 / 26, bands 1 / 2 / 4 codes wide: 616 / 322 / 173 bands, 2,464 / 1,288 / 692 vertices; light fit 54 / 53 / 53 us; band cut 2,483 / 1,332 / 744 us; sending the mask's two passes 820 / 440 / 251 us; the map's 68 vertices 13 to 15 us. | MEASURED 2026-11-25, 1 console, 1 run | p3, p4 | c4 test 5, the fit, cut and send lines |
| PVR render time at the same three widths: map pass 3,312 / 3,311 / 3,311 us; mask pass 9,054 / 6,132 / 5,015 us. Screen pass: the scene alone 3,161 us, with the shadow layer 9,646 us. | MEASURED 2026-11-25, 1 console, 1 run | p3, p4 | c4 test 5, the PVR lines |
| `shz_dce_pvr_project` in this spike: 132.5 / 132.3 / 115.7 cycles a vertex against 138.8 / 138.3 / 137.5 for its C form, the bytes the same. | MEASURED 2026-11-25, 1 console, 1 run | p3, p4 | c4 test 5, "project" |
| In the c4 program the projection's source array is at 8c205220 and its output at 8c1c5220: exactly 256 KB apart, a multiple of the 16 KB operand cache, so every source line and its output line share a cache entry. | FACT: the symbol table of `build/psm_cmp/psm-c4-c4.elf` (`sh-elf-nm`) | - | bench h2 timed the same routine on a clean placing: 21.15 cycles in the cache, 58.09 out of it |
| Punch-through, reference 128, a 4444 texture whose alpha is a ramp, 32,768 pixels: KallistiOS's own header draws 16,384 (the half with alpha at or over the reference); with depth write off 16,384; with depth "always" 16,384. | MEASURED 2026-11-25, 1 console, 1 run | p4 | c4 test 6, first three lines |
| The same with blend ONE / ZERO draws all 32,768: the pixels that fail the test are written too. | MEASURED 2026-11-25, 1 console, 1 run | p5 | c4 test 6, "blend ONE/ZERO" |
| The same with vertex alpha 128 draws 2,048: only the top sixteenth of the ramp. The tested alpha is the texture's times the vertex's. | MEASURED 2026-11-25, 1 console, 1 run | p5 | c4 test 6, "vertex alpha 128" |
| The bump level can be the alpha the punch-through test reads: 10,200 pixels drawn of 20,480 at reference 128, where the cosine level table gives 10,320; 120 pixels differ. | MEASURED 2026-11-25, 1 console, 1 run | p5 | c4 test 6, "bump level as the tested alpha" |
| PVR-DMA of 128 KB from video memory: 4,009 us with its end polled each 1 ms, 3,526 us polled each 250 us, 3,630 us by interrupt. Of 512 KB: 14,028 us, 13,852 us, and by interrupt 14,516 us (14,510 in the first run). Every copy right. A polled time is rounded up to the next poll. | MEASURED 2026-11-25, 1 console, 2 runs for the last figure, 1 run for the others | p5, p6; first run `c4e/..074541020` | c4 test 7 |
| Live frame at sun 182 / 26, b0 8.0, bands 4 wide, layer on: 676 vertices; SH-4 fit + cut + faces 811 us, projection 422 us; PVR map 3,301 us, mask 4,792 us, screen 9,951 us; frame 39,929 us. | MEASURED 2026-11-25, 1 console, 1 run | p6 | c4 page 6, LIVE line |
| The frame takes 39.9 ms at 676 and at 1,288 vertices and 49.9 ms at 2,192, while the work in it (the measured SH-4 and PVR lines added) is 19.3, 21.7 and 27.1 ms. Why the frame is one of two values 10.0 ms apart is not known. | OPEN | p6; `c4e/..074645426` | c4b: count the pictures (vblanks) in each frame and time each wait apart |

### What was predicted, and what the console said

"Model" is the host model of this file (ESTIMATE); "third party" is the reading of a third party's hardware rules in
`../POLLY2_NOTES.md` (REPORTED); "thesis" is `../THESIS_SH4_SHADOWS.md`.

| Prediction | From | Console | Verdict |
|---|---|---|---|
| The lookup by depth works; the plain ramp is one code off in about a quarter of the pixels near the light | model | works; 2,303 of 16,384 wrong with the plain ramp (14 % of the whole map), 348 with 8 sub-steps, never by more than 1 code | right in kind; the 8 sub-steps are worth having |
| The ramp's perspective error is minor | third party | worst 1 code | right |
| The target holds only codes | model | 0 pixels not a code | right |
| A second pass at equal depth leaves no holes | third party; "not verified" here | holes 0 everywhere | right |
| The bump level follows the cosine table, so the window's edges are where Q puts them | model | 784 / 443 / 240 of 40,960 pairs disagree at window 16 / 32 / 64: 4.9 / 2.8 / 1.5 codes a Q value | WRONG |
| The real table makes the windows a few codes narrower: 1 to 3 at window 32, 5 at window 16 | third party | 2.8 and 4.9 codes a Q value (the page counts disagreements; it does not say on which side) | right in size |
| b0 1.5 gives no acne | model | ground alone: 2,774 pixels of acne at 1.5 (1,364 with bands 4 wide), 0 at 3.0; scenes at 1.5: 2,687 and 2,502 | WRONG |
| Rim 0 with the faces grown 0.75 texel | model | 0 and 0 | right |
| Edge (false shadow from a nearer face): 634 at 150 / 26.5, 1,716 at 100 / 13 | model | 971 and 1,906 | low by 337 and 190 pixels |
| Real shadow shown lit: 4 inside the bias and 0 elsewhere; 0 and 5 | model | 2 and 0; 0 and 1 | right in size |
| The map is the model's map | model | 98.4 % and 99.0 % of the pixels equal, the rest 1 code off | right within the ramp's 1 code |
| The projection costs 19.5 cycles a vertex against 39 for its C form | pipeline model | 132.5 against 138.8 in this program | WRONG for this program: its two arrays are 256 KB apart and thrash the cache (the FACT row). On a clean placing bench h2 measured 21.15 against 43.19 |
| An alpha under the reference fails, at or over it passes | third party | 16,384 of 32,768 drawn | right |
| A failing pixel is still written when the blend is ONE / ZERO | third party | 32,768 drawn | right: this is the one setting that made c3b's punch-through "draw everything" |
| Inside the punch-through list the depth mode and the depth write bit do nothing | third party | 16,384 with either changed | not contradicted (the count cannot tell "ignored" from "harmless here") |
| The punch-through test does not threshold vertex alpha (P7) | thesis | 2,048 drawn with vertex alpha 128 | WRONG: P7 named this run as what would overturn it, and it did. The tested alpha is texture x vertex |
| Texel 255 x vertex 255 gives 254 (read here as a plain product shifted by 8) | third party | the top step of the ramp (alpha nibble 15) x vertex 128 passes a reference of 128; the step under it does not | does not fit as read: (255 x 128) >> 8 is 127 and would have drawn 0, if nibble 15 becomes 255. Either the product is not a plain shift or the reading of the rule is wrong; not settled by one line |
| Whether the bump level is tested by punch-through | open on page 6 | 10,200 drawn against the table's 10,320 | yes; and it differs from the cosine table at the same rate as the 1555 route at threshold 128 (120 of 20,480 against 240 of 40,960) |
| PVR-DMA is held down by polling its status (bench h1) | reading of h1, withdrawn by h2 | 512 KB: 14,028 us polled each 1 ms, 13,852 each 250 us, 14,516 by interrupt | withdrawn stands: the interrupt route was the slowest of the three here |

### Readings

- **The acne has one cause, and it is measured twice.** The console's map is the model's map to within one code,
  and no pass leaves a hole; but the console's compare differs from the cosine table in 443 of 40,960 pairs at
  window 32, and in the scenes the mask differs from the model in 2,832 and 2,564 pixels inside faces, about the
  number that are acne (2,687 and 2,502). The level table is what is wrong in the model. The chip is not wrong; the
  cosine is not its table.
- **What the acne counts are.** Pixels of the 320 x 240 mask that a ray says are lit, that the mask shows shadowed,
  and whose map texel holds their own face: self-shadow. 7.4 % and 7.2 % of the lit pixels, at b0 1.5, bands 2 wide.
- **b0 3.0.** The ground alone shows 0 acne at b0 3.0 with bands 1 and 4 wide at sun 150 / 26.5 under c4's rule. c4b's
  rule adds a whole texel of slope where c4 added half, so on that ground at that sun c4b's default 3.0 has more
  bias than what measured 0. That is support for the default at one sun, on the ground, and no more: bands 2 wide
  were not in the test, no wall was, no other sun was, and in the live view the owner went to 5.25 at sun 91 / 34
  and to 8.0 at sun 182 / 26. b0 0.5 and 1 give the same count, and 1.5 and 2: Q is a whole code, so b0 acts in
  whole codes.
- **The cure stays the one planned.** Measure the window's two edges for the threshold in use at boot, from strips
  of known codes (test 1's own picture), and make Q from those, not from the cosine. A larger b0 hides the error at
  the price of shadow that starts further from its caster.
- **The projection's 132 cycles are this program's doing.** `bsrc` and `bscr` are static arrays of 256 KB one after
  the other. The same routine measured 21.15 cycles in bench h2. c4b must place them so that a source line and its
  output line do not share a cache entry, and the routine's comment should say so. Until then the projection (857 us
  at 1,288 vertices in the live view) costs 2.3 times what bench h2 measured for arrays that are not in the cache
  (58.09 cycles a vertex) and 6.3 times what it measured for arrays that are (21.15).
- **Punch-through is usable as the threshold.** It tests texture alpha x vertex alpha (KallistiOS's header for that
  list asks for the product: its texture mode there is "modulate alpha") and it tests the bump level;
  c3b's failure was the ONE / ZERO blend, which writes the failing pixels as well. A mask made by punch-through
  with KallistiOS's blend would need no 1555 threshold. Not built.
- **The frame time.** 39.9, 39.9 and 49.9 ms for 19.3, 21.7 and 27.1 ms of work: the frame is set by waiting, not
  by work. Each of the three passes waits for the PVR twice (`pvr_wait_ready`) and for its render. The two values
  are 10.0 ms apart and both 70 us short of a multiple of 10 ms; three pictures at 60 a second are 50.05 ms and two
  are 33.4, so the lower value is not a whole number of pictures. A wait that wakes on a 10 ms clock would give
  these numbers (4 and 5 periods; the mask pass crosses 10 ms between 1,288 and 2,192 vertices: 6.1 and 10.2 ms).
  That is a guess from three numbers. OPEN, with its test in the table.
