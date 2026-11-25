---
title: GPU Gems depth techniques on the PVR
tags: [research, pvr, depth, gpu-gems]
updated: 2026-11-25
sources: [demo/psm/GPU_GEMS_NOTES.md, GPU Gems 1-3 (NVIDIA online editions), wiki/hardware/ledger-pvr.md, wiki/research/post-pvr-tricks.md]
---
# GPU Gems depth techniques on the PVR

**What you will learn:** every chapter of the three GPU Gems books that uses depth, or that a depth-aware trick could imitate, sorted by what it does; for each, what it needs, which PVR or SH-4 piece could carry it, what that costs by our measured ledger rows, and the host test that would check it.
**For:** the owner ("the GPU bible"), and anyone choosing the next spike. Research, not a recipe.

**Status: research ideas only.** Nothing here was built or run. Every mapping is **ESTIMATE** or **OPEN**; costs come from MEASURED ledger rows; chapter figures are the chapter's own, on the chapter's hardware, and are not evidence for ours.

**How the chapters were read.** NVIDIA's free online editions, 2026-11-25. 33 chapters were read for this page (Gems 1: 9, Gems 2: 13, Gems 3: 11); the 17 shadow, occlusion and visibility chapters already in `demo/psm/GPU_GEMS_NOTES.md` were not read again; 81 others were judged from the tables of contents only. The fetch tool returns a summary of a page, not its text: every mechanism below is our own paraphrase, and any chapter number must be checked on the page before it goes into a ledger. Chapters whose title or body names a commercial game, engine or studio are cited by book and chapter only (rule 2); lead has the names.

**The pieces and their costs** (CMP, RAMP, BUMP, TILE, FOG, PT, TR, DA, MV, RTT, READ, VTX, SH4) are defined once, with their ledger rows, in [post-PVR tricks](post-pvr-tricks.md#the-pvr-pieces-and-what-each-costs).

## What the PVR already does that these chapters work hard for

Three things the chapters build in shaders are fixed hardware here:

- **Order-independent transparency.** Depth peeling (Gems 2 ch. 13, 15; Gems 3 ch. 17) exists to sort translucent layers per pixel. The PVR's translucent list can sort per pixel by itself ("autosort", [glossary](../glossary.md)). Its cost shows when many layers pile into one tile: 256 sorted translucent triangles cost 82 us spread over the screen, 6,385 us in one tile (PVR-009, MEASURED).
- **Hidden-surface removal before shading.** Gems 1 ch. 28's main advice is a depth-only pre-pass so hidden pixels are not shaded. The PVR resolves visibility per tile before it textures: hidden opaque layers cost nothing measurable (PVR-005, MEASURED, 0 ± 2 us). A depth pre-pass buys nothing here (ESTIMATE).
- **Per-pixel depth fog.** The fog table (PVR-049) is a free per-pixel function of 1/w.

## By category

### Soft particles and off-screen particles

| Chapter | Needs | PVR route | Cost (ledger) | Host test | Status |
|---|---|---|---|---|---|
| Gems 3 ch. 23, "High-Speed, Off-Screen Particles" (Cantlay) | A reduced copy of scene depth; particles drawn small, tested against it, faded by the depth gap; composited up | **The small target, without the depth test:** particles into a 320 × 240 target, drawn back over the frame. A translucent layer costs 0.77 ms at 320 × 240 against 3.1 ms at 640 × 480 (PVR-004, PVR-002), so fill drops about 4 times; the target's 3.2 ms floor must first be cut by a tile list (PVR-092). The depth fade needs the camera map ([D1](depth-shading-ideas.md)); without it, a per-vertex fade by height above the floor stands in | Particle fill ÷ 4, plus one full-screen layer, plus the target pass | Composite error at particle edges against a full-size host render | ESTIMATE |

### Outlines and edges

| Chapter | Needs | PVR route | Cost | Host test | Status |
|---|---|---|---|---|---|
| Gems 2 ch. 15, "Blueprint Rendering and Sketchy Drawings" (Nienhaus, Döllner) | Depth and normal edges; depth peeling for hidden edges | Outlines by inverted hull ([D6](depth-shading-ideas.md)); hidden edges as a second draw with the depth mode reversed ([D3](depth-shading-ideas.md)) | Doubles the outline mesh (VTX) | Edge set against a host image-space detector | ESTIMATE |
| Gems 3 ch. 16, vegetation animation and shading (Sousa) | Per-vertex bending; edge smoothing of leaf cards from a depth target | Bending on the SH-4 per vertex; leaf cards by punch-through (PT) with a per-vertex edge fade | Card fill (PT 3.3-4.3 ms a full-screen layer) | Bend deformation against the chapter's two-step form | ESTIMATE |

### Depth of field

| Chapter | Needs | PVR route | Cost | Host test | Status |
|---|---|---|---|---|---|
| Gems 1 ch. 23, "Depth of Field: A Survey of Techniques" (Demers) | Five methods: ray-traced, accumulation, layered, forward-mapped, reverse-mapped | **Layered** fits: sharp and blurred copies mixed per object or per depth band, as `sh4der_focus.h` does by shape. **Accumulation** is shifted copies added (TR); 8-bit colour bands quickly | `sh4der_focus.h`: a 128 × 64 blurred texture through a vertex-alpha mask; a 320 × 240 layer 0.77 ms (PVR-004) | Blend weights per band against a host reference blur | ESTIMATE |
| Gems 3 ch. 28, "Practical Post-Process Depth of Field" (Hammon) | Depth to blur radius, three blur sizes, a near-field fix | Real depth through the camera map and the bump window ([D5](depth-shading-ideas.md)); else per object | Camera map pass (3.3 ms whole list, 375 us cut) plus one layer | Radius curve against the chapter's piecewise form | ESTIMATE; camera map OPEN |

### Light shafts, volumes and the sky

| Chapter | Needs | PVR route | Cost | Host test | Status |
|---|---|---|---|---|---|
| Gems 3 ch. 13, "Volumetric Light Scattering as a Post-Process" (Mitchell) | Samples toward the sun's screen point; an occluder mask | The chapter's own fixed-function fallback: scaled copies of the frame about the sun, added; `sh4der_glow.h` already does god rays this way. The mask: occluders drawn black, or destination alpha (DA) | Each copy a layer: 0.77 ms at 320 × 240 (PVR-004); console cost of `sh4der_glow.h` OPEN (Q39) | Ray streak shape against the chapter's per-pixel sum on a host frame | ESTIMATE |
| Gems 1 ch. 39, "Volume Rendering Techniques" (Ikits, Kniss, Lefohn, Hansen) | Slices facing the eye, a transfer function, depth culling by geometry | Slices as translucent quads (TR), the fog table or vertex colour as the transfer; `sh4der_mist.h` is this family | 0.77 ms a 320 × 240 slice, 3.1 ms full screen | Slice count against visible steps | ESTIMATE |
| Gems 2 ch. 16, "Accurate Atmospheric Scattering" (O'Neil) | Scattering in closed form per vertex; phase per pixel | Per vertex on the SH-4 for the sky dome (a few hundred vertices), haze by the fog table | Chapter: about 60 evaluations a vertex; on the SH-4 a few hundred vertices only (ESTIMATE) | Sky colour against the chapter's reference values | ESTIMATE |

### Fur, grass and hair

| Chapter | Needs | PVR route | Cost | Host test | Status |
|---|---|---|---|---|---|
| Gems 1 ch. 7, "Rendering Countless Blades of Waving Grass" (Pelzer) | Three crossed alpha quads a patch, sorted, tips moved by wind | Already fixed-function: crossed quads by punch-through (no sorting needed), tips moved on the SH-4 | 12 vertices a patch (VTX, 142 ns each), PT fill | Wind motion against the chapter's trig | ESTIMATE |
| Gems 2 ch. 23, hair animation and rendering (Nguyen, Donnelly) | Hair as lines; opacity shadow maps in 16 slices | Strips with an alpha ramp; the opacity map has no PVR form. Shells are the other route ([post-PVR tricks](post-pvr-tricks.md) N5) | — | — | ESTIMATE: not a fit |

No chapter in the three books uses shells and fins; the shell method is from Lengyel et al. 2001 ([post-PVR tricks](post-pvr-tricks.md) N5).

### Relief, parallax and displacement

| Chapter | Needs | PVR route | Cost | Host test | Status |
|---|---|---|---|---|---|
| Gems 2 ch. 8, "Per-Pixel Displacement Mapping with Distance Functions" (Donnelly) | A ray march through a 3-D distance map per pixel | None per pixel (no dependent reads). Layered relief by shells and one height texture ([M3](depth-shading-ideas.md)) | N layers of PT fill | As M3 | per pixel not possible |
| Gems 3 ch. 18, "Relaxed Cone Stepping for Relief Mapping" (Policarpo, Oliveira) | Cone-limited steps and a binary search per pixel; writes depth | As above | — | — | per pixel not possible |
| Gems 2 ch. 7, adaptive tessellation with displacement (Bunnell) | Screen-space flatness per edge, subdivision by render passes | The SH-4 chooses a level per patch and displaces vertices; the flatness test is cheap per edge | VTX for the extra vertices | Crack-free tessellation on the host | ESTIMATE |

### Impostors

| Chapter | Needs | PVR route | Cost | Host test | Status |
|---|---|---|---|---|---|
| Gems 3 ch. 21, "True Impostors" (Risser) | A ray march through a four-layer height texture per pixel | An impostor atlas: a camera-facing quad showing one of a few pre-rendered views, chosen by angle; punch-through edges | 4 vertices an object; PT fill | View-switch error against the model from 16 views | ESTIMATE |

### Deferred shading

| Chapter | Needs | PVR route | Cost | Host test | Status |
|---|---|---|---|---|---|
| Gems 2 ch. 9, deferred shading in a shipped game (Shishkovtsov) | Multiple render targets, a readable depth, stencil per light | None. What survives: lights as additive passes over only the pixels they reach (a light volume's screen area), and the PVR's free hidden-surface removal | Each light pass its screen area of TR fill | — | ESTIMATE: no |
| Gems 3 ch. 19, deferred shading in a shipped online game (Koonce) | As above, with light accumulation targets | As above | — | — | ESTIMATE: no |

### Water

| Chapter | Needs | PVR route | Cost | Host test | Status |
|---|---|---|---|---|---|
| Gems 2 ch. 18, water by vertex-texture displacement (Kryachko) | Height maps read in the vertex stage; foam by vertex height | Displacement on the SH-4 per vertex: `sh4der_water.h` already does directional waves and crest transmission; foam by crest height, not depth | As the ocean demo (Q38) | Already host tested (`make test-water`) | in the library |
| Gems 2 ch. 19, "Generic Refraction Simulation" (Sousa) | The frame as a texture, read at a normal-map offset; a mask in alpha | **Fits in spirit:** the frame as a texture (RTT), the refracting mesh drawn with per-vertex UV offsets (`sh4der_warp.h`, `sh4der_uvwarp.h`), the mask in destination alpha (DA) | One frame-texture pass (PVR-004 floor, or a cut list) plus the mesh | Offset field against the chapter's per-pixel offset on a host frame | ESTIMATE |
| Gems 1 ch. 2, "Rendering Water Caustics" (Guardado, Sánchez-Crespo) | Rays from the floor up through the waves to the sun | A caustic brightness per floor vertex on the SH-4 (refract once, look up a sun disc), added (TR) | Per floor vertex: one refraction and one table read (ESTIMATE) | Caustic pattern against the chapter's analytic form | ESTIMATE |

None of the water chapters reads scene depth: no depth foam, no depth absorption. Those stay ideas [D2](depth-shading-ideas.md) and [F6](depth-fog-ideas.md).

### Transparency, peeling and tracing

| Chapter | Needs | PVR route | Cost | Host test | Status |
|---|---|---|---|---|---|
| Gems 3 ch. 17, multiple reflections and refractions (Umenhoffer, Patow, Szirmay-Kalos) | Layered distance cube maps, a ray march per pixel | None per pixel. A pre-rendered environment for reflection; per-pixel sorted translucency is the PVR's autosort | PVR-009 | — | ESTIMATE: no |
| Gems 2 ch. 13, a production renderer on the GPU (Lefrançois) | Shader graphs flattened; depth peeling for transparency | Autosort for the transparency | PVR-009 | — | ESTIMATE |

### Pipeline, motion and coverage

| Chapter | Needs | PVR route | Cost | Host test | Status |
|---|---|---|---|---|---|
| Gems 1 ch. 28, "Graphics Pipeline Performance" (Cebenoyan) | Depth pre-pass, front-to-back order | **Not needed:** the PVR removes hidden pixels per tile (PVR-005). The advice that does carry over: the vertex send (BUS-001 against BUS-003) and texture format (PVR-006) | — | — | FACT (PVR-005) for the hidden-surface part |
| Gems 3 ch. 27, "Motion Blur as a Post-Processing Effect" (Rosado) | Depth, inverse matrices, a velocity per pixel | Object-level blur by earlier copies at falling alpha; full-frame blur through the front-buffer copy (`sh4der_trail.h`) | Trail's two copies about 19 ms PVR (Q40, ESTIMATE) | Blur length against the chapter's velocity form | ESTIMATE |
| Gems 2 ch. 42, "Conservative Rasterization" (Hasselgren, Akenine-Möller, Ohlsson) | Triangles grown by half a pixel so every touched pixel is covered | **Already used:** c4 grows its map faces 0.75 texel so no map pixel holds the far scene at an edge (rim 0, PVR-099); band cutters must also avoid slivers (PVR-082) | — | `test/c4_model_test.c` already counts rim | MEASURED (rim 0) |

### Subsurface and translucency

| Chapter | Needs | PVR route | Cost | Host test | Status |
|---|---|---|---|---|---|
| Gems 1 ch. 16, "Real-Time Approximations to Subsurface Scattering" (Green) | Wrap lighting; thickness from a light depth map; texture-space blur | Wrap per vertex (`sh4der_sss.h`). **New: thickness from route B's RAM shadow map.** Route B already holds a 16-bit depth map from the light in RAM and compares per receiver vertex (`sh4der_psm_raster.h`). The difference between a vertex's own light depth and the map's nearest depth is the path the light crosses: `sh4der_sss.h`'s `path`, per vertex, with no bake | One extra subtraction per receiver vertex on top of route B's compare (ESTIMATE) | Thickness from the map against a host ray cast through a sphere and a slab | ESTIMATE |
| Gems 3 ch. 14, "Advanced Techniques for Realistic Real-Time Skin Rendering" (d'Eon, Luebke; the last tenth of the page was not read) | Texture-space diffusion by six Gaussians; translucent shadow maps for thin parts | As above for thickness; diffusion has no PVR form | — | — | ESTIMATE |

### Other chapters that use depth

| Chapter | Needs | PVR route | Status |
|---|---|---|---|
| Gems 1 ch. 41, "Real-Time Stereograms" (Policarpo) | The depth buffer copied each frame; strips shifted by depth | **Crazy:** a low-resolution camera depth code (route AB: PVR-drawn, PVR-DMA at 36-38 MB/s, BUS-051) read on the SH-4, a random-dot strip pattern built per row. A 160 × 120 16-bit map is 38 KB, about 1 ms of PVR-DMA (arithmetic) | ESTIMATE |
| Gems 2 ch. 38, global illumination by rasterization (Hachisuka) | Depth peeling along many directions | Offline only: a host bake into vertex colours | ESTIMATE |
| Gems 2 ch. 39, progressive refinement radiosity (Coombe, Harris) | Item buffers, form factors | Offline only: a host bake | ESTIMATE |
| Gems 3 ch. 34, signed distance fields by scan conversion (Erleben, Dohlmann) | The depth test as a "keep the smallest" selector | The same trick is what c4's map pass does: the PVR's depth test keeps the nearest code per pixel (PVR-095). Any per-pixel minimum or maximum of a value written as depth can use it | MEASURED (c4 map) |
| Gems 1 ch. 21, "Real-Time Glow" (James, O'Rorke) | A glow mask, a reduced target, a separable blur, an add; a fixed-function path with weights in vertex colour | The fixed-function path is `sh4der_glow.h`'s family | in the library |

Two read chapters use no depth and add nothing here: Gems 1 ch. 35, baking detail from a dense model onto a light one (an offline bake), and Gems 2 ch. 41, deferred filtering of volume data.

## Ideas this page adds

| # | Idea | From | Status |
|---|---|---|---|
| J1 | **Thickness from route B's RAM shadow map** for transmission: per vertex, no bake, moving light | Gems 1 ch. 16; Gems 3 ch. 14 | ESTIMATE |
| J2 | **Particles into a small target** to cut their fill about 4 times, once the target floor is cut by a tile list | Gems 3 ch. 23 | ESTIMATE |
| J3 | **Refraction from the frame texture** with per-vertex offsets and a destination-alpha mask | Gems 2 ch. 19 | ESTIMATE |
| J4 | **The depth test as a per-pixel min / max selector** for any value written as depth | Gems 3 ch. 34; c4's map | MEASURED as used in c4; other uses ESTIMATE |
| J5 | **Caustics per floor vertex** | Gems 1 ch. 2 | ESTIMATE |
| J6 | **A stereogram from a read-back camera depth code** | Gems 1 ch. 41 | ESTIMATE; crazy |

## Not found in the three books

Our reader found no chapter on: screen-space occlusion from depth beyond those in `GPU_GEMS_NOTES.md`; shells and fins; water that reads scene depth (foam, absorption); particle collisions against the depth buffer; a fog table indexed by depth. Judged from the tables of contents and the chapters read, not by reading every chapter.

## Sources

NVIDIA, *GPU Gems* (2004), *GPU Gems 2* (2005), *GPU Gems 3* (2007), free online editions at developer.nvidia.com/gpugems/ (read 2026-11-25 as summaries; chapters listed by book and number above). Chapter URLs are in the reader's notes; four of them carry a commercial game or renderer name in their address and are given to lead, not here.

## Related

- `demo/psm/GPU_GEMS_NOTES.md` (the shadow and visibility chapters)
- [Tricks from after and around the PVR](post-pvr-tricks.md)
- [Depth shading ideas](depth-shading-ideas.md)
- [Depth fog ideas](depth-fog-ideas.md)
- [PVR ledger](../hardware/ledger-pvr.md)
