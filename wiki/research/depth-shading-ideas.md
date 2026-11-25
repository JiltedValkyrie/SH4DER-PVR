---
title: Depth shading ideas: material inputs on the PVR
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
| D1 | **Soft particles, depth fade, soft halos** | Sprites and halos fade where they meet geometry instead of cutting a hard line | scene depth behind the pixel, and the sprite's own depth | Camera depth map as the bump texture; Q = the sprite's depth. A camera-facing quad has one depth, so one Q fits it exactly. The bump level gives a smooth ramp near M = Q. Pass 1 writes the level to destination alpha (c1 showed a compare in destination alpha, PVR-040); pass 2 draws the sprite with a destination-alpha blend. | yes | Map pass ESTIMATE 0.4-3.8 ms cut to tiles (PVR-092). Each sprite drawn twice; translucent fill as PVR-009. | ESTIMATE; the destination-alpha step is OPEN |
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

## What to test first

1. **The camera-view depth map:** c4's MAP pass drawn from the camera into a 320 × 240 target, through a cut tile list. Read back its codes as s3 did (PVR-023). This unlocks D1, D2, D5, C1, C2 and C4.
2. **D1 on one camera-facing quad:** the bump level into destination alpha, then the sprite through it. Photograph both passes.
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
