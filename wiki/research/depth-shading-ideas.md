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

- **Today:** translucent list, vertex alpha, punch-through (alpha ≥ reference, PVR-060), the 1555 mask.
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

### Craziest, worth one spike each

| # | Idea | One line | Needs from depth | PVR mechanism | Camera depth map? | Cost (from ledger) | Status |
|---|---|---|---|---|---|---|---|
| C1 | **Depth pulse (scanner sweep)** | A bright band sweeps out from the camera across the scene | scene depth per pixel | One full-screen quad, camera depth map as bump texture; Q = the pulse's depth, stepped each frame. The bump window is the band. One quad has one Q, an exact fit. It measures depth from the camera's plane, not distance from a point. | yes | Map pass, plus one full-screen additive layer: 3.1 ms at 640 × 480 (PVR-002). | ESTIMATE |
| C2 | **Depth-edge outlines** | Lines wherever depth jumps between neighbours | neighbour depths | Either the c1 blend chain on two shifted copies of the camera depth map (about 11 layers at 0.77 ms, PVR-004, PVR-040), or a bump pass with shifted coordinates against a banded Q. | yes | Chain: about 8.5 ms at 320 × 240 (ESTIMATE). | **OPEN**: neighbour compares |
| C3 | **Screen-space contact darkening** | Corners and contacts darken softly | neighbour depths | Several shifted compares, averaged, like the shelved soft shadows (T1). | yes | One mask pass measured 6.1-10.2 ms (PVR-090, PVR-091); several are too dear unless tile lists cut it. | OPEN; likely too costly |
| C4 | **Distance desaturation** | Far things lose colour and contrast | scene depth per pixel | A camera depth map compare against a "far" Q makes a 1555 mask. A grey layer is drawn through it. A cyclic window, so pick the range with care. | yes | Map pass, plus mask, plus one layer (PVR-004). | ESTIMATE |
| C5 | **Fur shells** | Fur from stacked cut-out copies of the mesh | none | N shells pushed out along the normals on the SH-4, drawn punch-through with a noise texture's alpha against the reference (PVR-060). | no | N × the mesh's vertices (BUS-003), N × its fill. | ESTIMATE; per-shell cut-off OPEN (one reference register) |
| C6 | **Dithered fade by distance** | Objects dissolve by a screen pattern instead of blending | own depth only (the SH-4 knows it) | A Bayer pattern texture's alpha against the punch-through reference. One reference for the whole list, so a per-object fade needs the pattern's alpha scaled by vertex alpha, which c3b 5b did not show working (PVR-061). | no | Punch-through fill (PVR-006 line). | **OPEN** (Q16) |

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
- Codrops, "WebGPU Scanning Effect with Depth Maps", 2025, tympanus.net/codrops/2025/03/31/webgpu-scanning-effect-with-depth-maps/ (depth pulse; seen in search).

Several popular tutorials on these effects are written for named commercial engines. Under the wiki's rule against engine names, they are not listed here; lead has the list.

## Related

- [Bump compare chain](../shadows/bump-compare-chain.md)
- [Depth fog ideas](depth-fog-ideas.md)
- [PVR ledger](../hardware/ledger-pvr.md)
- [Open questions](../open-questions.md), T2
