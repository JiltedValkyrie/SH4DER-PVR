# demo/psm: pass recipes for crude programmable shading on the PVR

Design note, 2026-11-25. **Nothing here is built and nothing has run.** The note lists multipass recipes made from the PVR's fixed units. Each recipe gives:

- the PVR settings it would need;
- the SH-4 maths (SH4ZAM calls only);
- the evidence it rests on;
- a host test that would check its maths before any console run.

It builds on `wiki/research/depth-shading-ideas.md` (ideas D1 to D6, C1 to C6) and `wiki/research/depth-fog-ideas.md` (F1 to F12). What it adds to them is marked **new**. The wiki pages are not edited.

Status marks are the ledger's:

- **MEASURED**: one console, one run, 2026-11-25, unless the row says otherwise.
- **FACT**: Sega's document, or a file in this repo.
- **REPORTED**: skmp's PowerVR2 model or KallistiOS issues.
- **ESTIMATE**: our arithmetic, its inputs named.
- **OPEN**: not known.

Ledger IDs (PVR-, SH4-, BUS-) are from `wiki/hardware/`. Nothing is called proven.

## 1. The units, as a programmer sees them

| Unit | As an operation | What we know | Limits |
|---|---|---|---|
| Texture x vertex colour, point sampled | `out = T[u] x c`: a per-pixel table lookup by an interpolated coordinate | a texel reaches a 16-bit target exactly; 10-bit coordinates exact (PVR-023, MEASURED) | one texture a polygon; perspective-correct interpolation, so a screen-space lookup is exact only on a polygon of one depth (OPEN on a console) |
| Offset colour (`gen.specular = 1`) | `out += o` after the texture | FACT (KallistiOS field; used by `sh4der_sss.h`) | per vertex, Gouraud |
| Bump unit as a comparator | `level = K1 + K3 cos(2π (M − Q) / 256)`, M the texel's byte, Q one byte a polygon in the offset colour's low byte, the level in alpha, the colour white | subtract works, Q not interpolated, ±1 code at M = Q (PVR-045, PVR-046, MEASURED); the curve is not the cosine table: 443 of 40,960 pairs differ at window 32 (PVR-096, MEASURED) | cyclic, 256 codes a turn; Q per polygon; offset word `K1 << 24 \| K2 << 16 \| K3 << 8 \| Q` (FILE `compare/c4_spike.c` macro `KQ`, K2 = 0) |
| 1555 write threshold | `bit = alpha ≥ thr` at write-out, one register a render | exact at 1, 2, 128 (PVR-028, MEASURED); set after the lists, put back after the render (FILE `c4_spike.c` `mask_pass`: `PVR_FB_CFG_2`, threshold bits 23–16, pack mode 3) | a render target only, not the 565 screen; one threshold a render |
| Punch-through test | draw if `texture alpha x vertex alpha ≥ ref` | rule and product MEASURED (PVR-060, PVR-064); the bump level can be the tested alpha (PVR-065, MEASURED) | one reference a render (`PVR_PT_ALPHA_REF`); never blend ONE / ZERO, it writes the failing pixels (PVR-061); the product's rounding OPEN; depth mode in the list REPORTED ignored (PVR-062) |
| Table fog | `out = lerp(colour, fog colour, t[i(1/w)])`, 128 entries, 16 an octave of scaled 1/w, far end first, interpolated between entries | per-pixel depth code MEASURED (PVR-049, c3b 6b: one level a pixel across an octave, no step back) | one table and one colour a frame; table write-only (keep a RAM copy) |
| Depth compare modes | 8 modes per polygon; a second pass at equal depth with "equal or nearer" leaves 0 holes | PVR-097, MEASURED | auto-sorted translucent depth is GreaterEq whatever is asked (FACT, Sega §3.7.9.1) |

Costs to build on (MEASURED):

| What | Cost | Ledger |
|---|---|---|
| A full-screen translucent layer, 640 x 480 | 3,112 us | PVR-002 |
| A full-screen punch-through layer, 4444 raster, nearest | 4,303 us first, 3,294 us each further | PVR-006 |
| Vertices into the TA, one store-queue run | 142 ns each | BUS-003 |
| A 128 x 128 target pass, whole tile list / cut to its tiles | 3,312 / 375 us | PVR-092 |

**The finding this note adds (new).** The bump comparator is exact only where Q is truly one value for the whole polygon. With the light's depth map (c4) that is false, so receivers are cut into bands: c4 needed 692 to 2,464 band vertices (PVR-100). But when M is a texture in the object's own texture space (a height, a noise, a mask) and Q is a single value for the whole object (a water level, a dissolve time), the compare is exact per pixel:

- no bands are needed;
- no camera depth map is needed;
- no screen-space coordinates are needed.

Recipes 7 and 8 use this. We know of no earlier use of the bump unit this way; we have not searched widely.

## 2. Recipes, from most useful to craziest

### R1. Ramp cel shading (wiki D4)

Hard light bands per pixel on a smooth mesh.

- **SH-4, once per object a frame:**
  - the light direction into object space: `shz_mat4x4_inverse_rigid` on the object's matrix, then `shz_mat4x4_transform_vec3_transpose`;
  - normalise with `shz_vec3_normalize`.
- **SH-4, per vertex:**
  - u = `shz_clampf(shz_vec3_dot(n, l), 0, 1)`; v = 0.5.
  - Optional wrap term: u = `shz_clampf(shz_lerpf(0.5, 1, d), 0, 1)` with d = the dot.
- **Pass, single for an untextured model:**
  - `PVR_LIST_OP_POLY`;
  - ramp texture 8 x 1 RGB565 (the smallest texture side is 8, FACT), `PVR_FILTER_NEAREST`, `PVR_UVCLAMP_U`, `txr.env = PVR_TXRENV_MODULATE`;
  - vertex colour = the material colour.
- **Second pass, for a textured model:**
  - draw the base first;
  - then the ramp on `PVR_LIST_TR_POLY` with `blend.src = PVR_BLEND_DESTCOLOR`, `blend.dst = PVR_BLEND_ZERO` (frame x ramp), `depth.comparison = PVR_DEPTHCMP_GEQUAL`, `depth.write = PVR_DEPTHWRITE_DISABLE`, `gen.fog_type = PVR_FOG_DISABLE`.
- **Rests on:**
  - the texture-coordinate path, exact to 10 bits (PVR-023, MEASURED);
  - a second pass at equal depth leaving no holes (PVR-097, MEASURED).
- **The band edge:** it falls where the interpolated u crosses a texel boundary. Its placement error is the thesis's §5.8 bound, at most half an edge for hard values (ESTIMATE).
- **Cost:**
  - one dot a vertex (h2: a FIPR row 38.10 cycles a point in the cache, SH4-017, MEASURED for that loop; ours is ESTIMATE);
  - the second pass adds the object's vertices again at 142 ns each (BUS-003) and its fill.
- **Host test:** `test/recipe_cel_test.c`.
  1. On a 32 x 32 sphere, u per vertex on the SH4ZAM path and on plain C agree within 1 / 1024.
  2. u is monotonic in N·L.
  3. Every vertex with N·L ≤ 0 gets the ramp's first texel.
- **Status:** ESTIMATE. **The most useful recipe and the cheapest to try.**

### R2. Inverted hull outline (wiki D6)

A dark border of near-constant screen width.

- **SH-4, per vertex:**
  - p' = p + n x (k x w), with w the vertex's clip-space w (`shz_xmtrx_transform_vec4`) and k = border pixels / (focal length in pixels), so the border keeps its width in pixels.
  - Then project as usual (`shz_invf_fsrra` on w).
- **Pass:**
  - `PVR_LIST_OP_POLY`, untextured (`pvr_poly_cxt_col`);
  - culling reversed against the model's (`gen.culling` CW against CCW; check the names against the KallistiOS header, this repo has used only `PVR_CULLING_NONE`);
  - vertex colour = the border colour.
- **Rests on:** standard TA behaviour, nothing new.
- **Cost:** the outline mesh's vertices again (BUS-003) and its thin fill.
- **Risk:** sliver triangles at the silhouette. Guard them with `sh4der_tri_area_ok` (PVR-080: a zero-area triangle draws stray lines on a console).
- **Host test:** `test/recipe_hull_test.c`.
  1. The screen distance between p and p' is k x focal within 1 % across depths 1 to 100.
  2. SH4ZAM and plain C agree.
- **Status:** ESTIMATE.

### R3. Dither fade and dissolve by punch-through (wiki C6, made exact here)

Objects that fade in, or fade out near the camera, with no translucent sort.

- **The test** (MEASURED rule, PVR-060, PVR-064): a pixel is drawn if t x a ≥ ref, with t the texel's alpha and a the vertex alpha.
- **The texture:** ARGB4444 with a 4 x 4 Bayer index k = 1 .. 15 in the alpha nibble; index 0 is never used, so a pixel can always be drawn.
- **Reading t:** if a nibble k reads as 17 k (INF: the expansion is not measured), the pixel is drawn when 17 k x a / 255 ≥ ref.
- **Choosing ref:** with ref = 16:
  - k ≥ 240 / a;
  - a = 255 draws all 15 cells;
  - a = 16 draws only k = 15.
- **SH-4, per object:** to show a fraction f of the cells (the cells k_min .. 15), k_min = 16 − round(15 f) and a = the smallest whole number ≥ 240 / k_min (`shz_divf`, then `shz_floorf` plus one where it is not whole), once an object, not a vertex. A fade by distance: f = `shz_smoothstepf(near, far, d)` per object, or per vertex for a long object.
- **Pass:**
  - `PVR_LIST_PT_POLY`;
  - `pvr_poly_cxt_txr` with `PVR_TXRFMT_ARGB4444`, `PVR_FILTER_NEAREST`, `txr.env = PVR_TXRENV_MODULATEALPHA` (KallistiOS's default for the list asks for the product, `compare/NOTES.md` c4);
  - **KallistiOS's blend for the list, not ONE / ZERO** (PVR-061);
  - `PVR_SET(PVR_PT_ALPHA_REF, 16)` before and after the lists, as `c4_spike.c` `punch_pass` does.
- **The screen pattern:** the Bayer texture must map to screen pixels, so give the vertices u, v = screen x / 4, y / 4 with `PVR_UVCLAMP_NONE` and a repeating texture. That is exact on a polygon of one depth only; on a slanted face the pattern swims (INF). For a dissolve, use a noise texture in the object's own u, v instead: the noise then stays on the surface, which is what a dissolve wants.
- **Cost:** punch-through fill (PVR-006), no sort.
- **Risk:** how t x a rounds is OPEN (PVR-064: the top nibble x 128 passed a reference of 128, which a plain (255 x 128) >> 8 would not). The step from a to cells may be off by one cell.
- **Host test:** `test/recipe_dither_test.c`.
  1. For a = 0 .. 255, count the cells drawn under two rounding rules: the product shifted right by 8, and the product rounded to nearest.
  2. Check the count is monotonic in a under both.
  3. Print both counts as the model for the console page.
- **Console check:** a sweep of a in steps of 8 against one texel value, at reference 16 and 128. That is PVR-064's own open test, and it settles the rounding.
- **Status:** mechanism MEASURED (one line each); the fade ESTIMATE.

### R4. Depth pulse and depth contours by the fog table (new; wiki C1 did it with a camera depth map)

A bright band sweeps out from the camera (a scanner), or contour lines by depth. **No camera depth map, no extra pass, no extra vertex.**

- **The table:**
  - The table indexes scaled 1/w: s = F / w, with F the fog scale (`pvr_fog_far_depth`).
  - The entry is about 16 x log2(s) (FILE: the model in `compare/sub_spike.c` `fog`, which c3b 6a matched within one 6-bit step, PVR-049).
  - Each table word holds an entry and the next one, for interpolation: `(t[i] << 8) | t[i + 1]` (FILE `sub_spike.c` `fog_load`).
- **SH-4, per frame:**
  - the pulse's entry i_p = 16 x `shz_log2f`(F / d_p);
  - t[i] = A x `shz_fmaxf`(0, 1 − |i − i_p| / h) for i = 0 .. 127, with h the half-width in entries;
  - for contours, t[i] = A when `shz_fractf`(i / P) < 0.2 (an ESTIMATE of a good duty).
  - Write the 128 words, keep a RAM copy (registers do not read back, FACT, Sega's errata).
- **Pass:** none of its own. Set `gen.fog_type = PVR_FOG_TABLE` on the polygons the pulse should touch, and `pvr_fog_table_color(1, r, g, b)` for the pulse colour.
- **Resolution:**
  - an entry spans 2^(1/16), that is 4.4 % of the depth;
  - with interpolation between two entries, the narrowest clean band is about 2 entries, about 9 % of its depth (ESTIMATE, DERIVED from 16 entries an octave);
  - a pulse at 20 m is about 1.8 m wide.
- **Cost:**
  - 128 register writes a frame (time not measured, OPEN);
  - the PVR's fog is per pixel at no measured extra cost (ESTIMATE: no line in h1 timed fog on and off).
- **Limits:**
  - one table and one colour a frame, so the pulse shares the table with scene fog (`sh4der_look.h`, `sh4der_torch.h` also bend it);
  - combine as t = max(fog, pulse), and accept one colour;
  - it measures distance along the view axis (1/w), not from a point;
  - our console read the top fog level one 6-bit step low (errata K5).
- **Host test:** `test/recipe_fogpulse_test.c`.
  1. For d_p from 2 to 200 m, the table's peak sits at the entry whose depth range holds d_p.
  2. The band's width in metres follows 2 entries x 4.4 % within 10 %.
  3. Packing then unpacking the 128 words gives the table back.
- **Status:** ESTIMATE. The cheapest crazy idea here: one table a frame.

### R5. Soft particles against known planes, per vertex (new; wiki D1 needs a camera depth map)

Halos, smoke and sparks fade where they meet the ground or a wall, with no map.

- **Idea:** most hard cut lines are against a plane the SH-4 already knows: the ground, a water surface, a wall of a pool (`sh4der_pool.h` planes).
- **SH-4, per sprite corner:**
  - signed distance s = `shz_vec3_dot(p, n)` − d to each nearby plane;
  - alpha x= `shz_smoothstepf(0, r, s)`, with r the sprite's radius.
  - Cut the quad 2 x 2 (9 vertices) so the fade is not a single ramp across it.
- **Pass:** the sprite's own (`sh4der_spark.h`, `sh4der_bloom.h`), vertex alpha only.
- **Cost:**
  - one dot and one smoothstep a corner a plane (ESTIMATE);
  - 9 vertices instead of 4 a sprite (BUS-003).
- **Limit:** only the planes the SH-4 knows. A crate or a character still cuts the sprite hard. The per-pixel version (wiki D1) is still OPEN on the camera depth map.
- **Host test:** `test/recipe_softplane_test.c`.
  1. Alpha is 0 for corners under the plane and 1 at r above it, monotonic in s.
  2. SH4ZAM and plain C agree.
- **Status:** ESTIMATE. Useful now, with no unknowns on the PVR side.

### R6. X-ray silhouette (wiki D3, with its open point narrowed)

A hidden character shows as a flat colour through walls.

- **Why the translucent list will not do:**
  - Sega gives auto-sorted translucent depth as always GreaterEq (FACT, §3.7.9.1), so a "draw only where hidden" mode cannot be asked for there.
  - Presort mode might honour it (`pvr_set_presort_mode`, a KallistiOS from after April 2026, REPORTED); OPEN.
- **The opaque list instead (new):**
  - Draw the character as usual in the opaque list.
  - At the end of the opaque list, draw it again: untextured, the silhouette colour, `depth.comparison = PVR_DEPTHCMP_LESS` (strictly farther than what is stored, so its own visible pixels at equal depth do not pass), `depth.write = PVR_DEPTHWRITE_DISABLE`.
  - This assumes the opaque list is processed in the order sent within a tile, so the second draw meets the finished world's depth there (INF: no source read, not tested).
- **SH-4:** nothing new; send the same vertices again.
- **Cost:** the character's vertices again (BUS-003), opaque fill only where hidden (PVR-005 says hidden opaque layers cost nothing that shows, MEASURED).
- **Risk:** whether this KallistiOS lets an opaque polygon with depth write off and LESS through as asked (OPEN).
- **Console check:** a cube behind a wall; count the silhouette pixels against the host's count; then the same with GEQUAL to see the cube's visible part only.
- **Host test:** `test/recipe_xray_test.c`. A software z-buffer of the test scene (the depth key as `sh4der_psm_raster.h` uses it) predicts the silhouette's pixel count, the console page's model.
- **Status:** ESTIMATE; the list-order point OPEN.

### R7. Waterline, snow line and flood by the bump comparator in texture space (new)

A foam line where terrain meets water, a snow line on a slope, rising water; per pixel, with Q for the whole terrain.

- **The map:** the terrain's height as an 8-bit bump texture in the terrain's own u, v (the M byte, built as c4's ramp is; twiddled is fine, PVR-070).
- **The compare:** the bump level peaks where the height equals the water level Q, so a window round Q is the shoreline.
- **SH-4, per frame:**
  - Q = the water level in height codes (one value for every polygon);
  - the offset colour `KQ(Q)` = K1 << 24 | K3 << 8 | Q on every vertex;
  - the punch-through reference from the window's measured edge (below).
- **Pass:**
  - `PVR_LIST_PT_POLY`, `pvr_poly_cxt_txr` with `PVR_TXRFMT_BUMP`, `PVR_FILTER_NEAREST`, `gen.specular = 1`, KallistiOS's blend for the list;
  - `PVR_SET(PVR_PT_ALPHA_REF, thr)`.
  - The bump level is the tested alpha (PVR-065, MEASURED); the pixels drawn are white (c3b: "the colour is white").
  - Drawn coplanar over the terrain after it.
- **Why it is exact (ESTIMATE):** Q is constant across the whole terrain and M is in texture space, so there are no bands and no screen-space lookup.
- **Window edges:** the window's edges are not the cosine table's (PVR-096). Take thr from edges measured at boot on strips of known codes, the c4b plan (Q56), not from `c4_threshold`.
- **Limits:**
  - 8-bit heights;
  - the compare is cyclic, so a height near 0 and a level near 255 meet (keep the terrain's codes inside 16 .. 239);
  - the line's width is in height codes, so it is thin on steep ground and wide on flat;
  - the coplanar punch-through pass depends on the depth mode in that list (PVR-062, OPEN).
- **Cost:** the terrain's vertices again (BUS-003) and punch-through fill.
- **Host test:** `test/recipe_waterline_test.c`.
  1. On a synthetic height map and the measured level table (once it exists; the cosine table until then), the pixels inside the window equal the pixels with |h − Q| ≤ w.
  2. Codes near the wrap are refused by the builder.
- **Status:** ESTIMATE; mechanism MEASURED in c4 test 6 (PVR-065).

### R8. Dissolve with a burning edge (new; R3 plus R7)

An object dissolves through a noise pattern, with a white-hot rim at the edge.

- **Body:** R3's punch-through on the object's own ARGB4444 texture with noise in the alpha nibble, vertex alpha = the dissolve's progress a (PVR-064: the vertex alpha counts).
- **Rim:**
  - the same noise as a bump texture (M), Q = the noise level at the current edge, a narrow window;
  - punch-through on the bump level, drawn after the body, coplanar;
  - white (the bump unit's colour).
- **SH-4, per object:** Q = the noise code where t x a = ref, for R3's rule: Q = 255 x ref / a, through `shz_divf` once an object.
- **Limits:**
  - the rim and the body use two different rules (a product against a reference, and a cyclic window), so their edges meet only if the rounding of the product is known (PVR-064, OPEN);
  - the noise must stay inside half a turn (128 codes) so the window is one-sided.
- **Cost:** two punch-through passes of the object.
- **Host test:** `test/recipe_dissolve_test.c`. For a in 16 .. 255, the rim's pixels border the body's cut-out pixels on the model, under both rounding rules of R3.
- **Status:** ESTIMATE.

### R9. Contact edges and soft particles per pixel (wiki D1, D2)

The general case: anything meeting any geometry. Needs the camera depth map (wiki "What to test first", item 1): c4's map pass drawn from the camera into a 320 x 240 target cut to its tiles (PVR-092), the sprite with screen-space u, v and Q = its own depth.

- A camera-facing sprite has one depth, so one Q fits it exactly; a slanted water face needs bands, as c4's receivers do.
- **Settings:**
  - as c4's mask pass (`c4_spike.c` `h_bump`): `PVR_LIST_TR_POLY`, `PVR_TXRFMT_BUMP | PVR_TXRFMT_NONTWIDDLED`, `gen.specular = 1`, `txr.env = PVR_TXRENV_REPLACE`, `PVR_UVCLAMP_UV`, blend ONE / ZERO into a target, `PVR_DEPTHCMP_GEQUAL`, no depth write;
  - then a 1555 threshold for a line (D2) or the level as a fade (D1).
- **Cost:**
  - map pass 375 us cut to tiles, 3.3 ms whole (PVR-092, MEASURED for c4's 128 x 128 map; a 320 x 240 camera map is not measured);
  - plus a mask pass (5.0 to 9.1 ms in c4, PVR-100).
- **Status:** OPEN. The most general recipe, and the dearest; R5 covers the common case for no PVR cost.

### R10. Fur shells (wiki C5)

- **SH-4, per shell s of N:**
  - p_s = p + n x (H s / N);
  - vertex alpha a_s = 255 x ref / n_s, with n_s rising from ref to 255 across the shells, so fewer strands pass farther out (by the rule t x a ≥ ref, PVR-064).
  - One `shz_divf` a shell, not a vertex.
- **Pass:** N punch-through passes of the mesh, noise in the alpha nibble (R3's settings), u, v scaled up so a strand is about a texel.
- **Cost:** N x the mesh's vertices (BUS-003: 8 shells of 500 vertices is 0.57 ms of TA time, ESTIMATE) and N x its fill.
- **Host test:** `test/recipe_fur_test.c`. The share of strands passing at each shell falls monotonically with s, under both rounding rules.
- **Status:** ESTIMATE.

### R11. Depth-edge outlines and screen contact darkening (wiki C2, C3)

- Both need a pixel compared with its neighbour's depth.
- The bump unit compares a texel with a polygon's Q, not with the pixel's own depth, so a neighbour compare is OPEN.
- The blend chain of c1 can do it in about 11 layers (PVR-040).
- **Not recommended** until the camera depth map exists and a neighbour compare is found. R2 gives outlines now.

## 3. What to test first

| Order | Test | What it settles | Needs |
|---|---|---|---|
| 1 | R1 cel ramp on one sphere, one and two passes | the cheapest look win; the multiply pass at equal depth | nothing new |
| 2 | Punch-through vertex-alpha sweep (R3's console check) | the product's rounding (PVR-064); R3, R8, R10 at once | one page in a c4-style image |
| 3 | R4 fog pulse on the c4 scene | the table's resolution in metres; the cost of 128 writes | the fog table code of `sub_spike.c` |
| 4 | R6 x-ray in the opaque list | whether LESS with no depth write is honoured in order | one cube and one wall |
| 5 | R7 waterline on a synthetic height map | the texture-space compare; the coplanar punch-through depth mode | c4b's measured window edges (Q56) |

The tests named `test/recipe_*.c` are not written. Each would be built like the existing `test/*_test.c`: plain C and the SH4ZAM path, with SH4ZAM's software back end on the host (`make test`, `make test-shz`). The recipes' maths would then live in one header (for example `sh4der_recipes.h`) so the client, the tests and a console page share the same bytes.

## Sources

- `demo/psm/compare/NOTES.md` (c1, c3b, c4), `compare/c4_spike.c`, `compare/c4_model.h`, `compare/sub_spike.c`: settings and models (FILE).
- `wiki/hardware/ledger-pvr.md`, `ledger-sh4.md`, `ledger-bus-dma.md`: the IDs cited.
- `wiki/research/depth-shading-ideas.md`, `depth-fog-ideas.md`: the ideas this note extends.
- `demo/psm/POLLY2_NOTES.md`: skmp's model, REPORTED (credit skmp; nothing copied).
- `demo/psm/THESIS_SH4_SHADOWS.md` §5.8: the edge error of a threshold on an interpolated value.
