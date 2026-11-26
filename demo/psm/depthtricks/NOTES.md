# Depth tricks for the PVR chain: design notes (host only)

Senior's notes, 2026-11-25, overnight item 3. Nothing here ran on a console. Every cost is built from MEASURED ledger rows (named) and is itself an ESTIMATE; a number from the host model is marked HOST. Reading list: `demo/psm/GPU_GEMS_NOTES.md`, `wiki/research/depth-shading-ideas.md` (D, C, M items), `wiki/research/depth-fog-ideas.md` (F items).

Model test: `demo/psm/depthtricks/dt_model_test.c`. Build from the root with `gcc -std=c99 -O2 -Wall -Wextra -DSH4DER_CORE_NO_SH4ZAM -Iinclude demo/psm/depthtricks/dt_model_test.c -lm`. It passes at `-O2` and at `-O3 -ffast-math`. Log: `D:\tmp\psmbuild\logs\dt_model_20261125.log`.

## The four tools, what each one gives

| Tool | What it does in one pass | Cost row |
|---|---|---|
| c4 compare | A depth code from a ramp lookup (MAP), then a bump-unit level 128 + K·cos(M − Q) against a 1555 write threshold (MASK): one bit a pixel, "the stored code is under Q". | PVR-090, PVR-092, PVR-096, PVR-100, PVR-028 |
| bump unit alone | A per-pixel level from a stored angle and a per-polygon angle: a smooth function of (stored − polygon). | PVR-045, PVR-047, PVR-096 |
| tile cut | Draws a pass only in the tiles that hold its receivers. | PVR-092 |
| ramp texture | A per-vertex value as u into a point-sampled 1D texture: a table lookup per pixel at texture cost. | PVR-006, PVR-023 |

What decides the costs: a full-screen translucent layer at 640×480 is about 3,112 us (PVR-002). An opaque, twiddled, nearest 565 layer is 1,115 us (PVR-006). A 320×240 target is 3,213 us empty, plus about 771 us a layer (PVR-004). A vertex by store queue is 142 ns (BUS-003).

## 1. Camera depth map (first item)

What it is: a small picture of view depth, one code a pixel, that later passes read as a texture (soft particles D1, contact edges D2, fog F-items, DoF D5).

### Route A: the PVR draws it

- The scene is drawn again from the camera into a render target, with the c4 MAP ramp as the texture: u is the depth code.
- The target is 128×96 (or 160×120), 565 or 1555, and the code goes in one channel.
- What the target gets: each pixel its code within 1 code of the truth. This is the c4 test 0 result from the light, MEASURED: 2,303 of 16,384 pixels one code off with the plain ramp, 348 with 8 sub-steps.
- Cost, ESTIMATE:
  - vertices sent again: 142 ns × N (BUS-003). The live scene had 1,288 vertices (PVR-090), so about 0.18 ms of SH-4 time;
  - transform: 13.16 cycles a vertex (SH4-020), about 0.08 ms;
  - PVR time: a 128×128 map pass is 3,314 us whole and 375 us tile cut (PVR-090, PVR-092). A 128×96 target is about 3/4 of that area, so about 0.3 to 2.5 ms of PVR time depending on the cut. That is PVR time; the SH-4 is free while it runs.
- Risk: the render target pass is one more scene begin on the PVR. Its fixed cost for a small target has not been measured apart; PVR-004 gives 3,213 us empty, for 320×240 only. **OPEN:** the empty-scene cost at 128×96.

### Route B: the SH-4 rasters it

- The scene is clipped at the near plane and filled by edge functions at pixel centres; the nearest is kept.
- The code is written as 8 or 16 bits into 24 KB at 16 bpp, then uploaded as a texture.

The host model, HOST, on c4's scene and camera at 128×96:

| Measure | Result |
|---|---|
| Triangles | 52 |
| Pixels hit | 6,086 (the rest is sky) |
| Pixel writes | 7,691 (1.26 a pixel hit) |
| Holes against a ray a pixel | 0 |
| Extra pixels against a ray a pixel | 0 |
| Code linear in 1/z (affine on the screen, no divide a pixel) | 127 pixels 1 code off the true pixel's code, worst 1; depth error mean 1.1 cm, max 5.6 cm |
| Code linear in z (one reciprocal a pixel) | 0 pixels off; error mean 1.2 cm, max 2.8 cm (quantisation only) |
| Size of a code over the view's 6.4–20.6 m | 1/z: 1.7 cm near, 17.8 cm far; z: 5.6 cm everywhere |

Conclusion: the affine 1/z code is good enough for fog and soft particles, and it costs no divide. Pick the z code only if a far-field compare (DoF, fog onset) needs even steps.

Cost, ESTIMATE:

- Fill: 7,691 writes. In cache, a write is a few cycles. The map is 24 KB, bigger than the 16 KB operand cache, so the stores miss. With an SH4-047 scattered cost of 27.7 cycles as the upper bound, that is about 0.05 to 1.1 ms. A scanline fill writes in order, so it is nearer the low end.
- Setup: 52 triangles × a few hundred cycles, under 0.1 ms.
- Upload: 24 KB. At 146 MB/s by store queue that is 0.17 ms; at 335 MB/s by ch2 DMA it is 0.07 ms (BUS-010, measured at 32 KB).
- This route scales with the scene's triangle count and its depth complexity. Route A scales with vertices only.

### Which one

- Route B wins on PVR time (0) and on SH-4 time only for a small scene (tens of triangles: casters' proxies, the ground, a few boxes).
- Route A wins once the depth map must show the real meshes.
- A mixed form: draw the proxies on the CPU, and leave the detail out of the depth map.

### What the host model checks

The test checks:

- holes and extra pixels against rays (0 / 0);
- the worst code error is ≤ 1 for both encodings;
- the share of the frame hit.

Next: add a proxy-only scene, and count writes against triangle count to give the fill law for the SH-4 cost.

## 2. One-pass ramp cel shading (recipe R1)

What it is:

- Per vertex, clamp(N·L, 0, 1) goes into u (and v = 0).
- The texture is a point-sampled 8×1 (or 8×8) ramp holding 4 tone bands.
- One opaque pass gives hard band edges that follow the interpolated N·L, not the triangle edges.
- A textured model needs a second pass at equal depth. PVR-097 MEASURED 0 holes, and that pass multiplies the ramp over the base texture.

The host model, HOST, on a unit sphere under orthographic view with the light (0.48, 0.64, 0.60). It counts pixels whose band differs from the true per-pixel normal's:

| Mesh | Ramp | Ramp, u at 10 bits | Gouraud tone (band per vertex, tone interpolated) |
|---|---|---|---|
| 8×6 | 13.0 % | 13.0 % | 14.3 % off by over half a band |
| 16×12 | 3.4 % | 3.4 % | 10.7 % |
| 32×24 | 0.8 % | 0.8 % | 6.6 % |

Reading:

- The ramp converges four times per mesh doubling. Gouraud tones converge slowly and smear the edges.
- The 10-bit u path changed nothing (PVR-023 holds).
- At 16×12 a ramp mesh already beats a Gouraud mesh at 32×24.

Cost, ESTIMATE:

- SH-4: one dot product a vertex and one clamp. That is under the 13.16-cycle transform (SH4-020), and it goes into the same loop.
- PVR: an opaque textured pass at 1,115 us full screen (PVR-006), against the plain Gouraud pass it replaces. The extra cost is the texture read, a few percent of that pass. **OPEN:** opaque textured against untextured at the same fill has not been measured apart.
- A textured model adds an equal-depth translucent modulate pass, about 3,112 us full screen (PVR-002), or less in proportion to the model's share of the screen.
- Extras on the same ramp:
  - rim light: put (1 − N·V) into v of a 2D ramp, a second index at no extra pass;
  - shadow from c4: pick a darker row when the mask bit is set (a third pass, or the c4 MASK itself drawn as the darken).

What the host model checks: band error against the true normal for ramp, 10-bit ramp and Gouraud, at three mesh sizes. The ramp must beat Gouraud.

## 3. The rest of the list

Each item gives what the four tools do, the cost, and what a host model would check. All are ESTIMATE until a console run.

**D1 soft particles.** The particle fades where it nears the scene behind it: alpha = sat((Zscene − Zparticle)/k).
- On the PVR: c4 MAP gives the scene depth code as a texture (route A or B above). Then per particle, the bump unit with Q = the particle's own code gives a level that falls as the gap shrinks. This is PVR-096's window: the level is monotonic over ±32 codes. That level is the fade as a modulate over the particle.
- Cost: one extra textured layer over the particles' own area (PVR-002 share), plus the depth map.
- Host test: the fade against the exact sat() at each code gap, over the measured window table (PVR-096's edges, `c4_edges`). Count pixels off by over 1/8.

**D2 contact edges (ink where depth jumps).**
- Done with the depth map drawn twice, offset by one texel in x and in y, with a subtract blend (no subtract on the PVR). In practice: the c4 compare with Q from the neighbour's code, i.e. the MASK pass drawn with the map shifted by 1 texel.
- Cost: two MASK-like passes on the cut tiles: about 3,760 us each at 128² (PVR-092). That is too dear at full screen; run it at 320×240 or on the cut only.
- Host test: edge pixels found against a Sobel of the true depth: misses and false edges.

**D3 x-ray (shows what is behind).**
- A second pass of the hidden object with depth compare GREATER and a flat colour.
- The PVR has the compare modes in the header (no c4 needed).
- Cost: the object's fill once (PVR-002 share).
- Host test: none needed beyond the header's compare mode. **OPEN:** whether GREATER against the list's stored depth works in the translucent list on hardware.

**D5 depth of field.** Blur by distance from the focal plane.
- The existing `sh4der_focus.h` already keeps a disc or band sharp by vertex alpha. Depth adds: the c4 compare against Q_near and Q_far gives two mask bits a pixel, i.e. "in focus" as a band.
- Cost: two MASK passes (PVR-100: 5,015 to 9,054 us by band width), plus the blurred map. Dear; a 320×240 target cuts it to about a quarter.
- Host test: the band mask against |z − z_f| < w over the scene, with the measured edges.

**C1 depth pulse (a ring of light travelling out through the scene).**
- The ramp texture with u = code − t·speed: a bright texel row sweeps out.
- This is the c4 MAP pass with the texture's u offset changed each frame, a vertex-level change only.
- Cost: one translucent additive pass of the scene (or its cut tiles), about the MAP pass's 3,314 / 375 us (PVR-090 / 092) at 128², scaled to the screen.
- Host test: the ring's width in pixels against code steps, i.e. that a 1-texel ring does not break into gaps at the far end (with 1/z codes the far end steps 17.8 cm a code, HOST above).

**C4 desaturation with distance.**
- Fog that tends to grey, not to the fog colour: the fog table (PVR-049) gives per-pixel fog by depth for free in the fog unit, with a grey fog colour.
- Real desaturation needs the pixel's own luma, which the PVR cannot read back. The nearest fit is the grey fog.
- Cost: 0 extra passes (fog is in the pixel path).
- Host test: the luma and chroma error of grey fog against true desaturation on a palette of scene colours.

**M1 curves (tone curve by depth or by the light).**
- The ramp texture as a curve: a palette (PAL8) texture whose palette is the curve.
- Changing the curve is a 256-entry palette write, not a texture upload.
- Cost: as the cel pass.
- Host test: the palette quantised to 565 against the float curve, as banding counts.

**F-items (fog).**
- F1 to F4 (height fog, layered fog): the PVR fog table is per-pixel by 1/w only.
- Height needs per-vertex alpha (an additive translucent pass) or a ramp u from height: the same as the cel ramp with u = height.
- Cost: a translucent layer over the fogged area.
- Host test: per-vertex against per-pixel height fog error on a slope, against mesh size (like the cel table above).

**Gems ch. 13 occlusion intervals, VSM, PSSM.**
- VSM needs a filtered depth and depth² and a divide per pixel. No PVR path: the blend unit cannot divide, and 8-bit channels are too few for the moments. Not a fit.
- PSSM, split shadow maps: the c4 chain is one map. Splits are c4 run twice with two light fits and a tile cut each. Cost: twice MAP plus MASK on their tiles, about 2 × (375 + 3,760) us at 128² cut (PVR-092), so about 8 ms. Dear.
- Host test: texel size per pixel at each split against one map, as `test/psm_test.c` does for PSM.
- Occlusion intervals (ch. 13): a precomputed per-vertex visibility, so CPU only, a bake. It fits `sh4der_ao.h` better than the PVR chain.

## Order I would run them on a console

1. Cel ramp: one pass, nothing new in hardware. Test: band edges stable as the light turns.
2. The camera depth map, route B: CPU raster plus upload. Measure the fill, then put it on screen as a texture.
3. D1 soft particles on that map, with the bump-unit fade.
4. Route A against route B at the same size.
5. C1 depth pulse.

Everything else waits on the measured cost of a small render target (OPEN above).
