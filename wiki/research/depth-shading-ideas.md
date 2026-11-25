---
title: "Depth shading ideas: material inputs on the PVR"
tags: [research, pvr, shading, depth]
updated: 2026-11-25
sources: [wiki/hardware/ledger-pvr.md, demo/psm/compare/NOTES.md, public shading tutorials listed under Sources]
---
# Depth shading ideas: material inputs on the PVR

**What you will learn:** how the inputs of a modern material map onto what the PVR already does, where a depth compare on the PVR could fill a gap, and which ideas are worth a spike.
**For:** the owner and anyone planning the next shading spikes. Research, not a recipe.

**Status: research ideas only.** Nothing on this page has been built or run. Every mapping is **ESTIMATE** or **OPEN**. Costs are built from MEASURED ledger rows, but no idea itself is measured.

## Read this first: what c4 compares, and what it does not

The [bump compare chain](../shadows/bump-compare-chain.md) (c4) compares two numbers per pixel:

- **M**: a depth code read from a texture, per pixel. In c4 that texture is the **light's** depth map.
- **Q**: **one** depth code **per polygon** (PVR-045). It is not interpolated. That is why c4 cuts each receiver into **bands** of equal code.

So c4 is **not** a per-pixel camera depth test. Every idea below that needs the scene's depth "behind this pixel" needs two more things:

1. **A camera-view depth map.** c4's MAP pass gets a per-pixel depth code by a texture lookup on 1/z (`compare/NOTES.md:189-193`), so the same pass drawn from the camera should give one. It has not been tried (OPEN). The nearest measured cost:
   - c4's map pass: 3.2-3.3 ms through the whole tile list, 375 us cut to its 20 tiles (PVR-090, PVR-091, PVR-092);
   - a 320 × 240 pass cut to its 80 tiles: 3.76 ms (PVR-092);
   - plus the vertices sent again, at about 142 ns each in store-queue runs (BUS-003).
2. **Screen-space texture coordinates.** The PVR interpolates texture coordinates with perspective, using each vertex's 1/w. A screen-space lookup is exact only when a polygon's vertices share one depth: true for a camera-facing quad, false for a long slanted face. Slanted faces must be cut small, as route A's ground is ([PSM routes](../shadows/psm-routes.md)). OPEN on a console.

**Precision.** The bump unit works on 8-bit codes and is cyclic, 256 codes a turn (`compare/NOTES.md:169`). A depth window must sit inside one turn.

**Neighbour compares are OPEN.** In one bump pass a pixel reads one texel (M) at its own texture coordinate and compares it with the polygon's Q. Shifting the texture coordinates reads a neighbour's M, but compares it with the same polygon-wide Q, not with this pixel's own depth. Whether the bump unit can compare a pixel against its neighbour is OPEN. The blend chain of spike c1 can subtract two shifted copies per pixel, but it took about 11 layers (PVR-040, PVR-004).

**A compare that needs no bands.** When M is a texture in the object's own u, v (a height or a noise) and Q is one value for the whole object (a water level, a dissolve time), the bump window passes exactly the pixels near Q, with no bands and no camera depth map (`demo/psm/RECIPES_NOTES.md` R7, R8; ESTIMATE):

![The bump comparator in texture space (recipes R7 and R8, not built): when M is a texture in the object's own u, v (a height or a noise) and Q is one value for the whole object (a water level, a dissolve time), the bump unit's window passes exactly the pixels with |M - Q| <= w, with no bands, no camera depth map and no screen coordinates; the mechanism is MEASURED (PVR-045, PVR-065), the window edges must come from boot measurements (PVR-096, Q56), and the recipes are ESTIMATE.](../images/bump-texture-space.svg)


## The PVR's building blocks

| Block | What it gives | Evidence |
|---|---|---|
| Texture × vertex colour | Base colour per pixel, modulated by a per-vertex colour | standard PVR (FACT) |
| Offset colour | A per-vertex colour added after the texture (specular add) | glossary; used by `sh4der_sss.h` |
| Bump unit | Level = K1 + K2·sin S + K3·cos S·cos(R − Q) per pixel; as a compare, 128 + 127·sin(M − Q) | PVR-045, PVR-047 |
| 1555 write threshold | Alpha ≥ threshold becomes one bit: a binary mask | PVR-028 |
| Depth bands | A face cut along lines of equal code, one Q a band | c4, PVR-090 / PVR-091 vertex counts |
| PVR-drawn depth map | 8-bit codes per pixel from a pass through a ramp | c3b, c4 (PVR-092) |
| Table fog | Per-pixel fog by 1/w, 128 entries, one colour; costs the SH-4 nothing until changed | PVR-049; `PVR_FEATURES.md:54` |
| Translucent list | Blended layers; 3.1 ms a full-screen layer at 640 × 480, 0.77 ms at 320 × 240 | PVR-002, PVR-004 |
| Depth compare modes | 8 modes per polygon, including "greater" and "less" | glossary; PVR-050 |
| Modifier volumes | Exact inside/outside per pixel; no cost seen in one test | PVR-044 |
| Frame read | The SH-4 reads the drawn frame back | SH4-040, SH4-041, BUS-021 |
| SH-4 per-vertex maths | Any function of position, normal, light, eye, written into colours | [SH4ZAM rule](../methods/sh4zam-rule.md); SH4-020 |

The order these blocks act on a pixel, and one pass written out field by field (the c4 mask pass's bump draw):

![A PVR pass recipe: the order a pixel goes through (depth, texel or bump level, combine, offset colour, clamp, fog, blend, write-out; the punch-through test in its own list) and a card with every field of one pass, filled in for the c4 mask pass's bump draw, each entry with its status (FACT from Sega's manual or our code, MEASURED from our console).](../images/pvr-pass-recipe.svg)

## By material input

Each row: what the PVR does today, what this library already does per vertex (headers named, nothing copied), and where a PVR depth compare could add something.

### Base colour

- **Today:** texture × vertex colour. Per-vertex light or tint from the SH-4.
- **Library:** `sh4der_holo.h`, `sh4der_blade.h` (vertex colour tables); `sh4der_look.h` (grade layers).
- **Depth fills:** distance tint is already the table fog (below). **Distance desaturation** (far things grey) is not: table fog blends toward one colour, it does not take colour away. Idea D3 below. ESTIMATE.

### Metallic and specular

- **Today:** offset colour as a specular add; environment textures.
- **Library:** `sh4der_pl4nar.h` (planar reflection with Fresnel alpha), `sh4der_water.h` (Fresnel, reflected direction).
- **Depth fills:** nothing obvious. Reflections are a view problem, not a depth-compare one. OPEN whether a reflection layer needs a contact fade where it meets the ground (idea D1 would do it).

### Roughness

- **Today:** no per-pixel roughness. Highlight width per vertex on the SH-4; blur through mipmaps (PVR-008).
- **Library:** `sh4der_focus.h` (a blurred copy drawn through a vertex-alpha mask: disc, band or rect).
- **Depth fills:** **depth-of-field by real depth.** The focus mask is drawn by shape today. A camera depth map with Q set to the focus depth gives a per-pixel "near the focus plane" window from the bump level (idea D5). ESTIMATE.

### Normal

- **Today:** the bump unit (a hardware normal map, its light set per polygon); per-vertex normals for SH-4 lighting.
- **Library:** `sh4der_bump.h` (normal map by blend passes, no bump unit), `sh4der_water.h` (ripple normals), `sh4der_sss.h` (normals for transmission).
- **Depth fills:** none needed. Note that the bump unit cannot be a normal map and a depth compare in the same pass.

### Emissive

- **Today:** additive passes, offset colour, vertex colour.
- **Library:** `sh4der_blade.h`, `sh4der_bloom.h`, `sh4der_glow.h`, `sh4der_spark.h`, `sh4der_bolt.h`, `sh4der_rise.h`.
- **Depth fills:** **soft halos.** A lamp's halo quad cut off by a wall shows a hard line; a depth fade (idea D1) removes it. ESTIMATE.

### Opacity

- **Today:** translucent list, vertex alpha, punch-through (texture alpha × vertex alpha ≥ reference, PVR-060, PVR-064), the 1555 mask.
- **Library:** `sh4der_warp.h` (ordered dither as a screen door), `sh4der_mist.h` (fog sheets that fade near the eye).
- **Depth fills:** the biggest gap. Depth fade, soft particles, contact edges and foam all need "how far is the scene behind me". Ideas D1, D2.

### World position offset

- **Today:** the SH-4 moves vertices before sending them.
- **Library:** `sh4der_water.h` (waves), `sh4der_deform.h` (dents), `sh4der_uvwarp.h` (texture offset per vertex).
- **Depth fills:** none needed.

### Ambient occlusion

- **Today:** baked vertex colour; blob shadows.
- **Library:** `sh4der_ao.h` (crease bake, and a screen cavity cue from the 80 × 60 read), `sh4der_pool.h` (blob shadow).
- **Depth fills:** screen-space occlusion needs neighbour depths: OPEN (see above). Contact darkening under a moving object is cheaper as a blob than as a compare. Idea C3. ESTIMATE.

### Fog and depth colour

- **Today:** **table fog already does per-pixel fog by depth.** Do not rebuild it. Linear, exponential or any curve fits its 128 entries (PVR-049; `PVR_FEATURES.md:54`).
- **Library:** `sh4der_look.h` (fog glare), `sh4der_torch.h` (fog table bent to black), `sh4der_mist.h` (fog with a shape).
- **Depth fills:** **height fog** is not in the table (it is by depth only). It can be done per vertex on the SH-4, or with mist sheets. A depth "pulse" sweeping out from the camera is idea C1.

## Material functions by slot: per vertex or per pixel

Modern material editors give each input slot a small set of ready functions. This table lists the common ones, what this library already does for each, and **where it happens**:

- **per pixel** means a PVR unit does it for every pixel: texture lookup, texture × vertex colour, offset add, bump unit, fog table, blend, punch-through, 1555 threshold;
- **per vertex** means the SH-4 computes it and the PVR interpolates the result across the triangle (Gouraud). A per-vertex term is only as sharp as the mesh.

A function's maths is per vertex here unless a PVR unit can carry it per pixel. Headers are named; nothing is copied from them.

| Slot | Function | What it does | Library today | Where | Route to per pixel on the PVR | Status |
|---|---|---|---|---|---|---|
| Specular, opacity | **Fresnel** (Schlick) | More reflection at grazing angles: F0 + (1 − F0)(1 − N·V)^5 | `sh4der_water.h` (`sh4der_water_fresnel`), `sh4der_pl4nar.h` (layer alpha) | per vertex | M1: N·V per vertex as a texture coordinate, a ramp texture holding the curve. The curve is then applied per pixel to an interpolated N·V | ESTIMATE |
| Emissive | **Rim** | A glow on the silhouette: (1 − N·V)^p | none as such; `sh4der_sss.h` lights the side away from the light, not the rim | — | M1 (ramp of the rim curve); or M2 (the bump unit as per-pixel N·V) | ESTIMATE |
| Opacity | **Dither fade** (screen door) | Pixels cut by a threshold pattern instead of blended | `sh4der_warp.h` (`sh4der_warp_dither_threshold`, ordered dither for pixelate) | per pixel (pattern) | C6: a pattern's alpha × vertex alpha against the punch-through reference (PVR-064) | ESTIMATE |
| Subsurface | **Thickness / transmission** | Light through thin parts, tinted by absorption over a path | `sh4der_sss.h` (baked thickness, wrap, distortion), `sh4der_water.h` (crest) | per vertex | M4: thickness per pixel from two depth maps (front and back faces) | **OPEN**; crazy |
| Diffuse | **Wrap lighting** | N·L shifted so light wraps past the terminator | `sh4der_sss.h` (`wrap` in the back term) | per vertex | the bump unit's own K terms set an ambient floor per polygon (PVR-047, REPORTED) | ESTIMATE |
| Specular | **Half-vector highlight** | (N·H)^n | `sh4der_bump.h` (power 2 by grey add passes, H per cell) | per pixel (power 2), per cell (H) | as the library does it | ESTIMATE (the client's passes; console checks Q42) |
| Normal | **Normal map** | Per-pixel normal for light | `sh4der_bump.h` (blend passes); the bump unit (hardware) | per pixel | already per pixel | FACT (unit), ESTIMATE (passes) |
| Normal, UV | **Parallax** | Texture shifted by view angle × height, so flat surfaces look deep | `sh4der_uvwarp.h` (a UV offset per vertex from a table; not view-driven) | per vertex | per pixel parallax needs a texture read that depends on another texture: the PVR has none. M3 gives depth by layers instead | per vertex: ESTIMATE; per pixel: not possible |
| Opacity, WPO | **Shells** (fur, grass, moss) | N copies of the mesh pushed out along the normals, each cut by a noise alpha | `sh4der_holo.h` names a translucent shell, but not stacked shells | — | C5 (punch-through, per-shell cut-off by vertex alpha) | ESTIMATE |
| Base colour | **Detail texture** | A small tiling texture multiplied over the base up close | none; `LATEGEN_NOTES.md` lists it | per pixel (second pass) | a multiply pass (DESTCOLOR blend), faded out by vertex alpha with distance | ESTIMATE |
| Specular | **Matcap / sphere map** | A pre-lit sphere picture looked up by the view-space normal | none | — | per vertex: the SH-4 writes the view-space normal's x, y as u, v; the PVR interpolates and looks up per pixel | ESTIMATE |
| AO | **Baked occlusion** | Darkening in creases | `sh4der_ao.h` (crease bake), `sh4der_pool.h` (blob) | per vertex | lightmap pass (DESTCOLOR) for per pixel | ESTIMATE |
| Opacity | **Depth fade** | Fade where the surface meets geometry | none | — | D1 (camera depth map) | ESTIMATE; needs the camera map |
| Fog | **Distance fog** | Blend toward a colour by depth | the PVR's fog table; `sh4der_look.h`, `sh4der_torch.h` bend it | per pixel | already per pixel | FACT |
| Fog | **Height fog** | Density falls with height | `sh4der_mist.h` (sheets) | per vertex or sheets | none in the table (depth only) | ESTIMATE |

### The new ideas from this table

| # | Idea | PVR mechanism | Cost (from ledger) | Status |
|---|---|---|---|---|
| M1 | **Per-pixel curve of a per-vertex term** (Fresnel, rim, cel, any 1-D curve) | The SH-4 writes the term (N·V, N·L) into u per vertex. The PVR interpolates u per pixel; a 1-D ramp texture, point sampled or bilinear, holds the curve. Drawn as a second pass (multiply or add), or as the only texture on an untextured object. This is D4 generalised. Interpolating the term and then applying the curve is closer to per-pixel maths than interpolating the curve's output, but the term itself is still linear across the triangle | One more pass of the object (BUS-003 for the vertices, PVR-006 for the texture) | ESTIMATE |
| M2 | **Bump-unit rim** | The bump unit gives a per-pixel level from a texel's normal and one light direction per polygon (PVR-045, PVR-047). Set that direction to the view direction and the level follows N·V per pixel; write it to alpha and blend a rim colour by inverse alpha. The view direction is one per polygon, so large polygons need cutting, as c4 cuts bands. Linear in N·V: no power curve | One bump pass over the object | ESTIMATE; OPEN whether the bump level reaches alpha usefully outside a 1555 target |
| M3 | **Relief by shells with one height texture** | Draw N layers of the surface, layer k lifted by k/N of the relief depth (or for a floor, sunk), all with the same texture whose alpha is the height. Each layer's vertex alpha is set so that texture × vertex passes the punch-through reference only where the height reaches that layer (PVR-064). One texture, one reference, N layers. Parallax then comes from real geometry, not a lookup | N × the surface's vertices (BUS-003), N × its punch-through fill | ESTIMATE; rests on one measured line (PVR-064); the rounding OPEN (Q16) |
| M4 | **Per-pixel thickness** | Draw the back faces' depth into one map and use it as a bump texture against the front faces' Q in bands, as c4 does from the light. The level is a thickness window, fed to the transmission colour | Two map passes (PVR-100: 3.3 ms each through the whole list) plus a banded pass (PVR-100: 5.0-9.1 ms) | **OPEN**; far too dear today |

Sources for these functions: Schlick 1994 (Fresnel, below); Kaneko et al. 2001 and Welsh 2004 (parallax, below); Lengyel et al. 2001 (shells, below).

## The ideas

"Needs camera depth map" means the camera-view pass of the first section, which has not been tried.

### Most useful

| # | Idea | One line | Needs from depth | PVR mechanism | Camera depth map? | Cost (from ledger) | Status |
|---|---|---|---|---|---|---|---|
| D1 | **Soft particles, depth fade, soft halos** | Sprites and halos fade where they meet geometry instead of cutting a hard line | scene depth behind the pixel, and the sprite's own depth | Camera depth map as the bump texture; Q = the sprite's depth. A camera-facing quad has one depth, so one Q fits it exactly. The bump level gives a smooth ramp near M = Q. Pass 1 writes the level to destination alpha (c1 showed a compare in destination alpha, PVR-040); pass 2 draws the sprite with a destination-alpha blend. | yes | Map pass ESTIMATE 0.4-3.8 ms cut to tiles (PVR-092). Each sprite drawn twice; translucent fill as PVR-009. | The destination-alpha step and the camera depth code MEASURED (d1, suite s7: PVR-163, PVR-164; one run); the fade OPEN (not run: PVR-165, Q90) |
| D2 | **Contact edges and shoreline foam** | A glow or foam line where a shield, water or force field meets geometry | as D1 | As D1, but threshold the level with the 1555 bit (PVR-028), or draw a narrow window, to get a line, not a fade. Water is a slanted face, so it needs bands of equal camera depth (as c4 does from the light). | yes | Map pass as D1. Banding adds vertices: c4's live view needed 1,288-2,192 (PVR-090, PVR-091). | ESTIMATE |
| D3 | **X-ray silhouettes** | A hidden character shows as a flat silhouette through walls | the scene's depth against the object's, per pixel | **No depth map needed.** Draw the object a second time, flat colour, with the depth compare mode set to pass only where the stored depth is nearer (the PVR has 8 modes per polygon), and no depth write. Or use a modifier volume (PVR-044). | no | One extra draw of the object's vertices (BUS-003) plus its fill. | ESTIMATE; mode behaviour in the translucent list OPEN |
| D4 | **Cel bands per pixel** | Light falls off in hard steps, not a smooth gradient | none | The SH-4 writes N·L per vertex as a texture coordinate. The PVR interpolates it per pixel; a small ramp texture, point sampled, turns it into hard bands. Draw the ramp as a second, multiply pass over the base texture. | no | One more pass of the object; texture cost as PVR-006 (point-sampled twiddled, the cheapest line). | ESTIMATE |
| D5 | **Depth-of-field by real depth** | Only what is near the focus distance stays sharp | scene depth per pixel | Camera depth map on one full-screen quad; Q = focus depth; the bump window makes the sharp band. Feed it to `sh4der_focus.h` instead of its shape mask. One quad has one Q, so no bands. | yes | Map pass, plus a full-screen bump layer at 320 × 240: 0.77 ms (PVR-004). | ESTIMATE |
| D6 | **Inverted hull outlines** | A thick dark border round a character | none | The SH-4 pushes a copy of the mesh out along its normals; draw it flat with the culling reversed. | no | Doubles the outline mesh's vertices (BUS-003) and adds a little fill. | ESTIMATE |


D4 as a pass recipe (`demo/psm/RECIPES_NOTES.md` R1):

![Recipe R1, not built: the SH-4 puts clamp(N.L, 0, 1) in each vertex's u, the PVR interpolates it, and a point-sampled 8 x 1 ramp turns it into hard tone bands per pixel in one opaque pass (a textured model adds one multiply pass at equal depth); it rests on the 10-bit texture-coordinate path and the hole-free equal-depth pass (MEASURED PVR-023, PVR-097), and the recipe itself is ESTIMATE.](../images/cel-ramp-recipe.svg)

### Craziest, worth one spike each

| # | Idea | One line | Needs from depth | PVR mechanism | Camera depth map? | Cost (from ledger) | Status |
|---|---|---|---|---|---|---|---|
| C1 | **Depth pulse (scanner sweep)** | A bright band sweeps out from the camera across the scene | scene depth per pixel | One full-screen quad, camera depth map as bump texture; Q = the pulse's depth, stepped each frame. The bump window is the band. One quad has one Q, an exact fit. It measures depth from the camera's plane, not distance from a point. | yes | Map pass, plus one full-screen additive layer: 3.1 ms at 640 × 480 (PVR-002). | ESTIMATE |
| C2 | **Depth-edge outlines** | Lines wherever depth jumps between neighbours | neighbour depths | Either the c1 blend chain on two shifted copies of the camera depth map (about 11 layers at 0.77 ms, PVR-004, PVR-040), or a bump pass with shifted coordinates against a banded Q. | yes | Chain: about 8.5 ms at 320 × 240 (ESTIMATE). | **OPEN**: neighbour compares |
| C3 | **Screen-space contact darkening** | Corners and contacts darken softly | neighbour depths | Several shifted compares, averaged, like the shelved soft shadows (T1). | yes | One mask pass measured 6.1-10.2 ms (PVR-090, PVR-091); several are too dear unless tile lists cut it. | OPEN; likely too costly |
| C4 | **Distance desaturation** | Far things lose colour and contrast | scene depth per pixel | A camera depth map compare against a "far" Q makes a 1555 mask. A grey layer is drawn through it. A cyclic window, so pick the range with care. | yes | Map pass, plus mask, plus one layer (PVR-004). | ESTIMATE |
| C5 | **Fur shells** | Fur from stacked cut-out copies of the mesh | none | N shells pushed out along the normals on the SH-4, drawn punch-through with a noise texture's alpha against the reference (PVR-060). One reference register, but the tested alpha is texture × vertex (PVR-064), so each shell's vertex alpha can set its own cut-off. | no | N × the mesh's vertices (BUS-003), N × its fill. | ESTIMATE (the per-shell cut-off rests on one measured line, PVR-064) |
| C6 | **Dithered fade by distance** | Objects dissolve by a screen pattern instead of blending | own depth only (the SH-4 knows it) | A Bayer pattern texture's alpha against the punch-through reference. One reference for the whole list, so a per-object fade needs the pattern's alpha scaled by vertex alpha. c4 test 6 showed that it is: vertex alpha 128 cut a ramp to its top sixteenth (PVR-064). c3b 5b's "every pixel" was the ONE/ZERO blend (PVR-061). | no | Punch-through fill (PVR-006 line). | ESTIMATE (the mechanism measured in one line; the fade not built; the rounding OPEN, Q16) |

## Ideas from console results (c4b, ray1, town; 2026-11-25)

Each console result is checked for uses beyond the question it answered. Every idea below is **ESTIMATE** or **OPEN** until it has its own console run. Each names the MEASURED rows it rests on; a cost is arithmetic on those rows.

| # | Idea | What it is for | Rests on (MEASURED) | Mechanism and cost (arithmetic) | Test | Status |
|---|---|---|---|---|---|---|
| X1 | **Dissolve or burn edge in one punch-through pass** | Objects dissolving, burning or being revealed; a water line on a wall; a damage reveal. No target and no screen layer | PVR-105 (punch-through route gives the 1555 mask's pixels exactly), PVR-065 (the bump level as the tested alpha), PVR-102 (window edges known) | A noise or height texture in the object's own u, v as M; Q one value for the object, stepped each frame (recipes R7, R8). The bump level goes straight to the punch-through test in the frame. c4b's punch-through mask pass cost 7.2-7.9 ms for a 320 × 240 target; an object pays only for its own area | c4b test 9's harness with a noise texture and Q stepped: count pixels against the host model on the console's table (`dtab`) | ESTIMATE |
| X2 | **Calibrated bump-level curves** for rim, cel and N·V shading ([M2](#the-new-ideas-from-this-table), [D4](#most-useful)) | Any effect that reads the bump level as a brightness | PVR-102, PVR-103 (with the console's own table the model agrees within 40 px a scene) | Measure the level table at boot, as c4b does, and build ramps and Q from it, not from the cosine | Draw M2's rim; compare it with the host model on `dtab` | ESTIMATE |
| X3 | **A stencil from punch-through and destination alpha** | A second effect only where the first one drew: a glow only on a dissolve edge, a tint only inside an x-ray silhouette, a decal only on a marked area | PVR-105 (every punch-through pixel read ffff in a 1555 target), PVR-040 (blends read destination alpha) | Pass 1, punch-through, writes alpha 1 where it draws. Pass 2 blends with DESTALPHA, so it changes only those pixels. The PVR's nearest thing to a stencil buffer. A candidate for joining compares (Q58) | Whether the punch-through write sets destination alpha in the frame's own pack mode, not only in a 1555 target, is OPEN | OPEN |
| X4 | **SH-4 work in the render-done gap** | AI, physics, audio preparation, a shadow slice, without lengthening the frame | PVR-104 (the render-done waits took 36 of a 39 ms frame, running 2.9-9.1 ms past the renders), PVR-011 (a 100 Hz wake, FACT from the KOS source) | The tick is the cause (PVR-117): polled, c4g's frame fell 16.6 ms, from 39.9 to 23.3. A frame that polls can spend what it freed on a 1,024-ray shadow slice (2.8 ms, SH4-053) and the proxy pass (0.3 ms, SH4-055) with room left | The same frame with a slice added | ESTIMATE |
| X5 | **Ambient occlusion per vertex from short grid rays** | Soft darkening in corners, under eaves, at wall feet; replaces [C3](#craziest-worth-one-spike-each)'s several mask passes | SH4-052 (22 cycles a cell, 71-74 a ray) | 8 directions × 6 cells: 8 × (74 + 22 × 6) ≈ 1,650 cycles, about 8.2 us a vertex. 1,000 ground vertices ≈ 8 ms: bake at load, or redo in slices as the thesis does for the sun | Host count of rays and cells; one console slice timed against the arithmetic | ESTIMATE |
| X6 | **Contact shadows for movers on the grid** | A dark patch where a person or hen meets the ground, from the grid, not a blob | SH4-052 | Rays of at most 4 cells from the vertices round each mover: 74 + 4 × 22 ≈ 160 cycles a vertex; 500 vertices ≈ 80,000 cycles ≈ 0.4 ms | As X5 | ESTIMATE |
| X7 | **Line of sight for gameplay and sound** | AI sight lines; sound occlusion (a source behind a building is muffled: lower a voice's level or filter on the sound chip) | SH4-052 | One 64-cell ray ≈ 74 + 22 × 64 ≈ 1,480 cycles ≈ 7.4 us; 100 checks a frame ≈ 0.74 ms | A ray count per frame in a game loop, timed; costed use by use in [ray uses](ray-uses.md) | ESTIMATE |
| X8 | **Sun shafts in a mist bank** | Light shafts through fog where the sun passes between buildings, dark where they block it | SH4-052, SH4-053 (a batched ray 540-554 cycles in the town) | Per vertex of the mist slices (`sh4der_mist.h`), a grid ray toward the sun gives visibility, multiplied into vertex alpha. 12 slices × 81 vertices = 972 rays ≈ 972 × 554 ≈ 0.54 M cycles ≈ 2.7 ms: slice it over frames or use fewer vertices | Host picture first; then the slice timed | ESTIMATE |
| X9 | **Batch grid and table reads apart from the vertex transform** | Every per-vertex table read: the deform dent grid, uvwarp tables, X5 to X8 | SH4-053 (interleaving cost 89-117 cycles a ray, 16-21 %) | Run the reads in one batch before or after the transform, not mixed into it. Measured for grid rays only | Time the deform grid read both ways | ESTIMATE |
| X10 | **Character proxies as local occluders** | Soft darkening round people on nearby ground and walls; a lamp's pool (`sh4der_pool.h`) dimmed when a person stands in it | SH4-055 (137.87 cycles a receiver-proxy pair with early outs) | The P9 pair loop with an occlusion term in place of a shadow test: 454 pairs ≈ 0.3 ms | Host picture; the pair loop timed with the new term | ESTIMATE |
| X11 | **A sun projector fitted by the PSM** | Cloud shadows crossing the town, a leaf or window pattern from the sun, with even detail near and far | PVR-113 (PSM map: 3.3 / 2.2 / 1.8 / 1.6 texels a pixel at 4 / 10 / 25 / 60 m), PVR-112 (uniform: 29.1 / 8.2 / 2.2 / 0.6) | `sh4der_psm.h`'s warped matrix and `sh4der_psm_pvr_uv` per vertex project a pattern texture; one multiply layer over the receivers (3.1 ms full screen, PVR-002) | The per-vertex coordinate error (`sh4der_psm_pvr_uv_error`) on the town's ground cells is OPEN | ESTIMATE |
| X12 | **A placing pool for every effect buffer** | Any effect with a source and a destination array: ocean, pl4nar lookup, sss, mist | SH4-048 (the projection 2.2 times faster once its arrays were placed apart), SH4-031, SH4-032 | One pool with offsets that are never a multiple of 16 KB apart, checked at build time as c4b does | Time one effect before and after | ESTIMATE |

From ray2 (2026-11-25):

| # | Idea | What it is for | Rests on (MEASURED) | Mechanism and cost (arithmetic) | Test | Status |
|---|---|---|---|---|---|---|
| X13 | **One prepared direction for every parallel-ray effect** | The sun bake, rain and snow cover (dry under eaves, bare under arches), a fixed key light's shadows on static props, wind shelter against triangles | SH4-066 (33.16 cycles a test prepared for one direction, against 62.49 for any) | Every ray sharing a direction gets the prepared form: work out the direction's part once a triangle (64 bytes), keep it while the direction holds. Rain cover over the town's 6,305 ground points: about 18.8 ms once ([ray uses](ray-uses.md) RU16) | Host cover map; one bake timed | ESTIMATE |
| X14 | **Pace animation and sound by the cycle counter or the vblank, not the KOS ns/us clock** | Smooth panning, audio scheduling, replays, input timing | SH4-057 (the ns clock jumps 2,502,321 ns at each reload, about once a second) | The demos take each frame's dt from `timer_us_gettime64` (`demo/main.c:916-920`, `demo/cloudvolume.c:685-688`). If that clock shares the step (ESTIMATE: it reads the same timer), one frame a second gets a dt 2.5 ms long, a 15 % longer step at 60 fps: a faint judder once a second | Log dt each frame over 10 s on a console; look for a +2.5 ms frame once a second | OPEN |
| X15 | **Triangles only where the grid fails** | Picking a mover's part, bullets on movers, sound through arches and rooms, a lamp's pool cut by a wall | SH4-066, SH4-064 | Grid first (22 cycles a cell), triangles after (33 to 62 a test), proxies for movers (138 a pair, cheaper than their triangles). [ray uses](ray-uses.md) RU13 to RU17 | As each RU row | ESTIMATE |

From suite s1 (2026-11-25):

| # | Idea | What it is for | Rests on (MEASURED) | Mechanism and cost (arithmetic) | Test | Status |
|---|---|---|---|---|---|---|
| X16 | **Poll every KallistiOS wait that a frame stands on** (narrowed by suite s3: **only where the wait is on every frame**) | The render-done wait, and any other wait that sleeps on an interrupt (a DMA end, a sound upload): a sleeping wait pays up to one 10 ms tick | PVR-117 (the render-done wait woke 2.9 to 9.9 ms late; polled, 7 to 8 us) | Poll the event in our own code. Other KallistiOS waits that sleep the same way are an ESTIMATE until each is timed; nothing in KallistiOS is changed **Suite s3 (PVR-136):** in the town, where the wait runs about one frame in six, a spin made route A 3 ms slower and route B no faster; it only halved the worst waits. See X27. | Time a DMA-end wait both ways, as c4g test 8b did | ESTIMATE |
| X17 | **A free CPU-load meter** | A profiling HUD line: how busy the SH-4 is each frame, with no extra timer | SH4-067 (the cycle counter stops while the CPU sleeps; polled it agrees with the ns clock to 2 us) | SH-4 load = cycles counted / (ns elapsed × 200) over a frame that sleeps in its waits. 3,400 / 39,909 us in c4g: the SH-4 was 8.5 % busy | Print it beside the town HUD's SH-4 ms | ESTIMATE |
| X18 | **The static-on-static bake as a general baker** | Per-vertex ambient occlusion, lamp lightmaps for static lamps (ray uses RU17), rain and snow cover (RU16), sky openness per vertex | SH4-070 (152 ms a bake; it runs and is right on a console), SH4-066 (33 to 62 cycles a test in the cache) | The same machinery with other directions: one bake a direction. At SH4-070's rate an 8-direction AO bake is about 1.2 s: at load, or sliced at 2 ms over about 600 frames. With the prepared test it may fall about 5 times (bake2) | bake2 against bake1; one AO bake timed | ESTIMATE |
| X19 | **An overflow governor, and the HUD first** | Any scene near the TA's limits: a game that must never lose its HUD or a frame's end | PVR-121 and PVR-149 (the PARAM events count every vertex buffer overflow; the OPB count is not one, Q81; the estimate reads 21 to 26 % low), PVR-122 (the HUD, sent last, is what gets cut), errata K7 | Count the PVR's error events each frame; when one fires, shed far parts on the next frame (a governor driven by the hardware, not an estimate). Send the HUD and other must-show parts first, or in a list that is sent before the busy one | hud1 (text first) at the start view: done, the HUD stays whole (PVR-150); a governor build at the same view | ESTIMATE (the governor); the HUD first MEASURED (PVR-150) |
| X20 | **Data out of the console by photo** | Bench results; a fault page as a code (registers, the stage, the stack); a replay seed or a save code a player can photograph; a profiling dump from a console with no serial port | VID-014 (all 16 codes decoded from photos; 84 bytes a code at 8 px), VID-015 (the CRC line works) | A fault page with one version 5 code (84 bytes) holds the PC, the stage, 16 registers and a CRC; four codes at 4 px carry 336 bytes with a second photo | A fault page with a code, decoded from a photo | ESTIMATE |
| X21 | **Even dissolves from the fitted punch-through rule** | X1's dissolve and burn edges, a dither fade by vertex alpha | PVR-120 (a fade by vertex alpha at a fixed reference is steep low and flat above 128; the rule fits all 38 rows, ESTIMATE) | Build the fade table from the rule so equal steps of the effect give equal steps of drawn cells, rather than stepping vertex alpha | Q75, then a fade drawn and counted | ESTIMATE |
| X22 | **Contact shadows with no bias** | Shadows that meet the caster's foot: no gap under a person, a crate or a post | PVR-119 (b0 0 gave 0 acne at two suns with the measured edge) | Set b0 to 0 where the measured edge is used: the false-shadow edge pixels it adds are under 0.1 % of the mask | A third sun (Q57); a picture of a foot at b0 0 and 3 | ESTIMATE |

From suite s3 (2026-11-25):

| # | Idea | What it is for | Rests on (MEASURED) | Mechanism and cost (arithmetic) | Test | Status |
|---|---|---|---|---|---|---|
| X23 | **For a union of shadows, do not use the depth trick; try modifier volumes** | Any "darken once however many casters cover it" pass: sky AO a direction, several movers' blob shadows merging, one shadow a light from many casters | PVR-127 (a depth union by "greater" with write failed: 477 to 2,541 wrong texels); PVR-044 (one modifier volume exact, 0 wrong) | The c4 chain does not use a depth union (it compares through the bump unit), so it is not touched. A modifier volume marks "inside" once whatever the overlap; whether two overlapping closed volumes still mark once is OPEN on this console. It gives one darkening for all directions together, so it cannot replace K faint directions; it fits the sun shadow and merged blobs | Two overlapping closed volumes over a flat target; count pixels changed once and twice | OPEN |
| X24 | **One side as an overlap counter** | A map of "how many things cover this texel": crowd density, the depth of overlapping foliage or smoke sheets, how many lamps reach a spot, a penumbra hint where shadows pile up | PVR-128 (one side darkens once a caster, twice where two overlap); PVR-129 (the blend is exact at 8 bits a channel, cut to 565 once) | Draw each closed shape one side, at a fixed faint alpha, into a small target; the level is the count. At alpha 1/2 the levels are 255, 127, 63, …: about 5 distinct counts in 565 green; a gentler alpha gives more counts. Cost as a sky direction a shape set: about 0.75 ms PVR and 34 KB at 1,164 triangles (PVR-130, DERIVED) | ao0's C at K 1 with 1 to 6 overlapping casters; read the levels | ESTIMATE |
| X25 | **Send one side of every closed caster** | The sun's map pass too, not only AO: halves its TA bytes and part of its PVR time | PVR-130 (one side 753 against 1,213 us PVR and 34 against 68 KB a direction); SH4-072 (the SH-4 saves only the send, 70 %) | The sun's 1,164 casters are 68 KB of the map pass's buffer (AO1-04: 63 KB in the town); one side makes that about 34 KB and saves some of the 0.6 ms the casters add (PVR-131). Needs Q78 (closed, wound one way) | The town's sun pass both ways, map texels compared, PVR time | ESTIMATE |
| X26 | **A tile an object: the owner's AO atlas** | Sky AO on the ground for the town without method 2's 10 ms a frame (PVR-135), at finer texels | PVR-130, SH4-071 (a direction of 1,164 casters: 1.82 ms SH-4, 1.21 ms PVR, 522 ns a vertex sent); PVR-129 (blend exact at 8 bits); PVR-004 and PVR-092 (a target pass's floor, and its cut to the covered tiles: 3,312 → 375 us) | **Layout.** One square tile an object in one atlas target: 64 px (8 KB at 16 bits) or 128 px (32 KB). The tile covers the object's footprint plus the AO reach (3.5 m out from a 6 m wall at 60°, AO_METHOD.md). A 16 m tile at 128 px is 12.5 cm a texel, three times finer than the shared 256 × 256 map over 96 m (37.5 cm). Sixteen 64 px tiles fill a 256 × 256 atlas (128 KB); sixteen 128 px tiles need 512 × 512, which a KallistiOS target cannot be (PVR-026: rows end at 479), so 128 px tiles go in two atlases or a 512 × 256 one. **Static objects, once at load:** each object's casters drawn K directions into its own tile, the method 2 draw with the tile's own slide matrices. For the whole town that is K × (1.82 + 1.21) ms: about 24 ms at K 8, once (DERIVED). **Movers, a frame:** each mover's tile cleared and drawn again, so a turn costs nothing extra. SH-4 about 522 ns × 3 vertices a triangle × its triangles × K: a 50-triangle mover at K 4 is about 0.31 ms, twelve of them 3.8 ms (DERIVED); one side halves the sending (SH4-072). The PVR pays a target pass each frame: its floor is about 3.2 ms (PVR-004) unless the pass is cut to the movers' tiles (X29), about 0.4 ms for 20 tiles by PVR-092. **The lookup:** each object's ground patch (the ground cells under its tile) gets u, v = (x − tile x0) / size, (z − tile z0) / size, mapped into the tile's sub-rect: two multiply-adds a ground vertex on the SH-4. With a uniform (top-down) map that is affine, so the PVR's per-vertex interpolation is exact (`sh4der_psm_pvr_uv_error` is 0 for a uniform map). The patch is drawn as a multiply layer; where two patches overlap, two multiplies give the product, which is right for AO. **The perspective map is the harder case:** the lookup is not affine there, the error grows with the cell size, and the ground near the camera needs fine cells (`sh4der_psm_pvr.h`). It is also the slower map: route B 43.9 against 40.2 ms of SH-4 (PVR-112, PVR-113), route AB 87.9 against 87.3 (PVR-114, PVR-115), route A the same to the HUD's 0.1 ms (PVR-108 to PVR-111); the owner's own benchmarks find perspective slower than uniform too (REPORTED, 2026-11-25). So the atlas is for the uniform map, and the perspective case is not worth chasing. **Saving against method 2 at K 4 in the town** (+9.8 ms SH-4, +1.4 ms PVR, +226 KB): statics go to 0 a frame; what is left is the movers' redraw plus the patches. **Console numbers (ao4-d, PVR-141):** the movers' half ran as 3+M: four 32-pixel tiles in a 128 × 128 target, send 336 us, quads +324 us of screen PVR, but the target pass's own 3,268 us made the frame +3.5 ms. Without that pass (X30) the movers' half is about 0.7 ms (DERIVED). | One static object and one mover both ways (atlas against method 2): the ground texels compared and the frame time; then the town with 12 movers at K 4 | ESTIMATE (every cost DERIVED from MEASURED unit rows; nothing built) |
| X27 | **A wait as a work slot, not a spin** | Every KallistiOS wait that is rare in a frame: render done, DMA end, a sound upload | PVR-136 (spinning made route A 3 ms slower; the wait runs one frame in six); SH4-075 (route A's map part +2.8 ms while spinning); PVR-117 (sleeping costs up to 10 ms) | Neither sleep nor spin: do the next piece of SH-4 work (the next frame's transform, a bake slice, X20's page) and test the event flag between pieces, with no timer read in the loop. Costs a flag read a piece | a3e with the render-done flag tested between map-part chunks; frame time against both a3e runs | ESTIMATE |
| X28 | **Stack faint layers in one render** | Light pools, glow rings, soft shadow layers, mist slices: many faint layers lose nothing to 565 until the render is written | PVR-129 (each value is the top bits of an 8-bit result, cut once; no banding a pass) | Put the layers in one render, not one render each: one 565 cut in place of one a layer. Where the layers must span renders, the cut adds up. Fits `sh4der_pool.h`, `sh4der_mist.h`, the glow rings. How many layers keep 8 bits is X24's question too | 16 layers at alpha 1/16 in one render against two renders of 8; read the steps | ESTIMATE |
| X29 | **Draw only the tiles an object covers** (the second reading of the owner's question, kept apart) | Any target pass with a few small things in it: the movers' tiles of X26's atlas each frame, a target with one caster, an inset | PVR-092 (a target rendered only through its covered tiles: 3,312 → 375 us for 20 tiles; 6,130 → 3,760 us for 80); [CORE 17](pvr-bugs-as-features.md) (user tile clips safe on 2.42, PVR-123) | Two ways: a tile list cut to the covered tiles (built for PVR-092), or a user tile clip a polygon header, which the PVR honours by skipping tiles outside it. The clip changes one strip early (CORE 18): a dummy polygon with screen area goes before each switch, since `sh4der_tri_area_ok` drops one without | X26's movers' pass with and without the cut; PVR time by `pvr_get_stats` | ESTIMATE |

From ao4-d (suite s6b, 2026-11-25):

| # | Idea | What it is for | Rests on (MEASURED) | Mechanism and cost (arithmetic) | Test | Status |
|---|---|---|---|---|---|---|
| X30 | **One target pass for every small extra** | The hens' AO tiles, a hero's shadow tile, a portrait inset, a mirror's small picture: anything that would get a render target of its own | PVR-140 (a 128 × 130 target pass costs 3,268 us for 576 vertices: the floor, PVR-004); PVR-139 and PVR-141 (each extra pass adds its whole time to the frame) | Draw the extras into spare rows of a target already rendered: the 256 × 256 shadow target as 256 × 290 (a KallistiOS target may go to row 479, PVR-026) holds eight 32-pixel tiles in its bottom 32 rows. Saves about 3.3 ms a frame a pass removed; the tiles cost only their vertices (336 us for four hens) | Q82 | ESTIMATE |
| X31 | **The baked AO byte as sky visibility for more than light** | The same byte a vertex dims what a crease hides: specular and sss gain, fog glare, rain wetness (dry under a ledge), the sound's open-sky share at a listener | SH4-077 (32 to 44 ns a vertex at draw); SH4-076 (the bake at load, 1.85 to 2.41 us a ray) | One multiply a term a vertex where the byte is already loaded; no new bake. With RU-style rays (ray-uses) the byte also tells an emitter's openness for reverb | A COLOUR scene with the byte on the specular too; a look pair | ESTIMATE |
| X32 | **Lightmap quads to buy back TA** | Scenes whose TA vertex buffer overflows (the town: PARAM, PVR-121, PVR-149): flat ground and walls as one textured quad each instead of a vertex grid | PVR-142 (a half: 4,268 against 6,680 vertices, 83 against 147 KB estimated, 64 KB of textures for a room) | The town's ground is a grid for vertex light and AO; as lightmap tiles it would drop most of its vertices, at the price of video memory and a static sun term in the map. Pairs with the vertex budget (PVR-121) | The town's ground as lightmap quads: TA events and frame | ESTIMATE |
| X33 | **Clay as a look, and AO where the ambient rules** | (a) an authoring look (CLAY or AO ONLY) to judge AO before colour; (b) a clay-figure art style in which method 1 is free and seen; (c) AO put on the sun's term too inside shadow and indoors, where COLOUR hides it | PVR-145 (the owner saw AO in CLAY only: ambient 0.72 / sun 0.30 against 0.50 / 0.50); PVR-138 (method 1 adds nothing to the frame) | Arithmetic on `ao4_light`: an AO of 0.5 on a lit face takes CLAY from 1.02 to 0.66 (−35 %) and COLOUR from 1.00 to 0.75 (−25 %); on one grey the change is the only signal, with a colour a shape the hue edges hide it (ESTIMATE). EXAGGERATE doubles it | Q86 | ESTIMATE |
| X34 | **The wall-foot fringe as a contact line** (one man's bug) | If the fringe is the bilinear leak, it is a free dark line where a wall meets the ground: contact darkening that method 2 alone does not give | PVR-145 (seen, REPORTED; the target is read bilinear, FACT) | Keep the leak where it reads as contact, cut it where it reads as a mistake by a one-texel inset; costs nothing either way | Q83, then a look pair with and without the inset | OPEN |

From suite s6b, the rest (2026-11-25):

| # | Idea | What it is for | Rests on (MEASURED) | Mechanism and cost (arithmetic) | Test | Status |
|---|---|---|---|---|---|---|
| X35 | **A wait-timeout count as a health line** | Any build that might crawl instead of hang: a result page, a debug HUD, a test that must say why it was slow | PVR-151 (hud1c-1: 0.6 to 1.6 frames a second, no fault page, no other sign) | KallistiOS's `pvr_wait_ready` and `pvr_wait_render_done` return −1 after 100 ms. Count the −1s a frame and print them beside the frame number: one counter, no hook. A build that crawls then names its stall in one photo | hud1c-1 with the count (Q87) | ESTIMATE |
| X36 | **KallistiOS's "used" reaching the whole buffer as the overflow tell** | X19's governor without hooking the error events (which hud1c shows can change how a build behaves) | PVR-149 (PARAM counted on every frame where "used" is 1,024 KB; none at 600 KB) | Read `pvr_get_stats` after each frame; when the largest used equals the buffer, shed on the next frame. Costs one stats read a frame | a governor on route A driven by "used", against one driven by the PARAM count | ESTIMATE |
| X37 | **Senses and queries on a tick, staggered** | Any per-agent query that need not run every step: hens' sweeps, sound occlusion rays, a mover's light or AO probe, a path re-plan | SH4-080 (the grid sweep on a 100 ms tick: 4 hens 49,810 → 5,392 cycles a step, 16 hens 221,541 → 36,328; worst 37,283 and 112,992) | Run each agent's query once every 6 steps and give each agent its own phase, so the worst step carries one agent in six: the worst falls toward the mean (DERIVED). At 200 MHz, 36,328 cycles is about 0.18 ms (DERIVED) | the hens with staggered phases: the worst step against the unstaggered tick | ESTIMATE |

From suite s7 (2026-11-25):

| # | Idea | What it is for | Rests on (MEASURED) | Mechanism and cost (arithmetic) | Test | Status |
|---|---|---|---|---|---|---|
| X38 | **Cut every small target pass to its tiles** | The hens' AO tiles, a hero's shadow tile, a portrait or radar inset, the mirror target's top rows, a light cookie: any render target whose drawn part is smaller than its tile table | PVR-156 (target M: 3,256 us for 300 tiles, 344 us cut to 4 × 5, the same texel sum) | The PVR walks every tile in the table; a cut table walks only the tiles drawn. ao4's 3.3 ms "floor" (PVR-140) becomes about 17 us a tile here (344 / 20, DERIVED). Revises the remedy in X30: a pass of its own may be cheap once cut | the cut on pl4nar's mirror target and on a 64 × 64 inset; the rewrite cost (ao4 p3, not shown) | ESTIMATE |
| X39 | **Destination alpha as a free per-pixel mask from the bump unit** | Beyond D1: a dissolve, a soft clip edge, a rim mask, a light cookie's falloff, a scan line or ripple band, any one-channel ramp the next draw multiplies by | PVR-163 (an alpha-only add of the level works with MODULATE and MODULATEALPHA; a negative level clips to 0; the half wave is clean) | Clear alpha, one bump layer into alpha, then the effect through DESTALPHA: two layers of fill, no texture of its own beyond the bump map. The level is near, not exactly, a cosine (172 against 180 at one code): shape it by Q and K, not by formula | one dissolve quad through a noise bump map | ESTIMATE |
| X40 | **Lamps by vertex count, the room lit by the lamp** | Street lamps in pl4nar, torches, a car's interior light, a fire: any point light the SH-4 runs per vertex | SH4-083 (1,166 ns a vertex; 3,883 vertices took 4.5 ms and the frame missed 60 Hz); PVR-158 (the look the owner called "REALLY good") | Budget about 860 lit vertices a millisecond (DERIVED). Light only the vertices within the lamp's reach and give the rest the dimmed ambient, which ao-lamp already computes (dimming 1.3 ms) | a lamp with a reach cut, counting lit vertices | ESTIMATE |
| X41 | **The camera depth map as an occlusion probe** | Is the sun or a lamp behind a wall (lens flare, glare, a halo's fade), is a hen seen by the player, a sound's occlusion: a few depth codes read on the SH-4 | PVR-164 (the codes read back off by 0 on a flat and a tilted quad); BUS-053 and PVR-163 (PVR-DMA reads video memory right) | Read a handful of pixels of the depth target after its render: a few words by the uncached read, no full download. Costs the map pass (PVR-092: cut to tiles, X38) and microseconds of reads | read the sun's pixel in d1's map with the sun behind a wall and in the open | ESTIMATE |

**Results with no use found elsewhere** (dead ends kept on record):

- OC-RAM for a grid (SH4-054): slower once vertices run beside it, and warm it was 0.9-1.1 % slower even alone. It only removes a 46 us cold start.
- The dce batch transform (SH4-050, SH4-065): slower than `mat_transform` for every mesh, in both runs.
- ray1's clock lines (SH4-051): a bench artefact, explained by ray2 (SH4-056, SH4-057); the explanation itself gave X14.
- OC-RAM in ray2 (SH4-063): a cold grid costs 0 there, but nothing else uses that yet.
- a3f's cache pads (SH4-069): no net gain; the lesson is that an 8 KB move can make another pair clash, so a pad needs its own before-and-after.
- c4b's Q forms (SH4-068): the same cost; nothing to reuse.
- The depth union for AO (PVR-127): fails, and with one side it saves no PVR time (PVR-130); kept only as a question (Q76) and as the reason for X23.
- c4g and r3-d run 2 (PVR-133, PVR-134, SH4-074): repeats, nothing new to reuse; they harden PVR-117, PVR-118, PVR-120 and SH4-048 to two runs.
- hud1b (TOWN-125): bisected in s6b (PVR-151); the use is X35.
- ao1c (PVR-148): a repeat of ao1; it hardens PVR-135 to three runs and shows that a segment's largest frame does not repeat (a limits row).
- hero1 h1e, hens-eyes hen-d and ao4-h's target passes (PVR-153, PVR-159, SH4-084, SH4-085): repeats, nothing new to reuse; they harden PVR-140, PVR-146, PVR-147, SH4-079 and SH4-080 to two runs.
- d1 T3's "+0 us" (PVR-165): two identical stages compared; not a result.
- c4b's slower band cut (SH4-049): no lesson until its cause is known (Q64).

## What to test first

1. **The camera-view depth map:** c4's MAP pass drawn from the camera into a 320 × 240 target, through a cut tile list. Read back its codes as s3 did (PVR-023). This unlocks D1, D2, D5, C1, C2 and C4. **Done in part (d1 T2, s7):** a ramp looked up by camera depth reads back off by 0 on 160 of 160 px (PVR-164).
2. **D1 on one camera-facing quad:** the bump level into destination alpha, then the sprite through it. Photograph both passes. **The first pass works (d1 T1, s7, PVR-163); the fade itself is not yet run (PVR-165, Q90).**
3. **D3:** an object drawn again with depth mode "less" behind a wall. No map needed, so it is the cheapest test.

## Sources

Read for the techniques; explained here in our own words; nothing copied.

- Ronja Böhringer, "Outlines via Postprocessing", ronja-tutorials.com/post/019-postprocessing-outlines/ (depth and normal edges).
- Ronja Böhringer, "Hull Outlines", ronja-tutorials.com/post/020-hull-outline/ (inverted hull).
- Ronja Böhringer, "Dithering", ronja-tutorials.com/post/042-dithering/ (Bayer screen-door, distance fade).
- Alexander Ameye, "Edge Detection Outlines", ameye.dev/notes/edge-detection-outlines/.
- Iain Cantlay, "High-Speed, Off-Screen Particles", *GPU Gems 3*, ch. 23, NVIDIA, 2007 (soft particles).
- NVIDIA, "Soft Particles" whitepaper, 2007, developer.download.nvidia.com/whitepapers/2007/SDK10/SoftParticles_hi.pdf (seen in search, not opened).
- Lengyel, Praun, Finkelstein, Hoppe, "Real-Time Fur over Arbitrary Surfaces", I3D 2001, hhoppe.com/proj/fur/ (shells and fins).
- Bavoil, Sainz, Dimitrov, "Image-Space Horizon-Based Ambient Occlusion", SIGGRAPH 2008 talk (screen-space occlusion).
- Philippe Decaudin, "Cartoon-Looking Rendering of 3D Scenes", INRIA RR-2919, 1996 (cel shading; seen in search).
- Christophe Schlick, "An Inexpensive BRDF Model for Physically-based Rendering", *Computer Graphics Forum* 13(3), 1994 (Fresnel; via its summary, the paper was not reachable).
- Tomomichi Kaneko et al., "Detailed Shape Representation with Parallax Mapping", ICAT 2001, Tokyo, pp. 205-208 (parallax by a texture-coordinate shift per pixel; seen in search, with a course-site copy at page.mi.fu-berlin.de).
- Terry Welsh, "Parallax Mapping with Offset Limiting: A Per-Pixel Approximation of Uneven Surfaces", Infiscape, revision 0.3, 2004 (the offset capped at the height to stop swimming at grazing angles; seen in search; later slides date it 2003).
- Codrops, "WebGPU Scanning Effect with Depth Maps", 2025, tympanus.net/codrops/2025/03/31/webgpu-scanning-effect-with-depth-maps/ (depth pulse; seen in search).

Several popular tutorials on these effects are written for named commercial engines. Under the wiki's rule against engine names, they are not listed here; lead has the list.

## Related

- [Bump compare chain](../shadows/bump-compare-chain.md)
- [Depth fog ideas](depth-fog-ideas.md)
- [PVR ledger](../hardware/ledger-pvr.md)
- [Open questions](../open-questions.md), T2
