---
title: Tricks from after and around the PVR
tags: [research, pvr, shading, history]
updated: 2026-11-26
sources: [wiki/hardware/ledger-pvr.md, wiki/hardware/ledger-sh4.md, src/sh4der_trail.c, demo/psm/POLLY2_NOTES.md, public papers and manuals listed under Sources]
---
# Tricks from after and around the PVR

**What you will learn:** which GPU techniques of about 2003-2010, and which tricks that earlier fixed-function consoles used to get masks and depth out of their hardware, could be done on the PVR and the SH-4 in spirit; what each would cost by our measured ledger rows; which are the most useful and which the craziest.
**For:** the owner and anyone choosing the next spike. Research, not a recipe.

**Status: research ideas only.** Nothing here was built or run. Every mapping to the PVR is **ESTIMATE** or **OPEN**; costs are built from MEASURED ledger rows. Sources were read for the mechanism and explained in our own words; nothing is copied. F = fetched and read; S = seen in search results only.

## The PVR pieces, and what each costs

Every mapping on this page and on [GPU Gems depth techniques](gems-depth-techniques.md) names one or more of these. MEASURED on one console, 2026-11-25, unless the row says otherwise.

| Key | Piece | What it gives | Measured cost | Ledger |
|---|---|---|---|---|
| CMP | Full compare chain [c4] compare | A depth code from a map, per pixel, against one Q a polygon; bands on slanted receivers | Map pass 3.3 ms through the whole tile list, 375 us cut to 20 tiles; mask pass 5.0-9.1 ms by band width, 3.76 ms cut to 80 tiles | PVR-092, PVR-100 |
| RAMP | Ramp texture | A per-pixel function of an interpolated coordinate: a depth code by 1/(2z), or any 1-D curve | Within 1 code with 8 sub-steps; one textured pass | PVR-095, PVR-006 |
| BUMP | Bump unit | A per-pixel level from a texel and one light (or Q) a polygon; at window 32 the console passes differences 99 to 157, measured at boot | As the mask line above | PVR-045, PVR-096, PVR-102 |
| TILE | Tile-list cut | A target rendered through only the tiles it covers | Map 3,312 → 375 us; mask 6,130 → 3,760 us | PVR-092, PVR-093 |
| FOG | Table fog | Per-pixel blend toward one colour by 1/w, 128 entries | Nothing on the SH-4 until the table changes | PVR-049 |
| PT | Punch-through | Texture alpha × vertex alpha ≥ one reference: a per-pixel cut; with the bump level as the alpha it gives the same mask as the 1555 route | 3.3-4.3 ms a full-screen 4444 layer; as a mask route 14-18 % dearer than 1555 (7.2-7.9 ms for a 320 × 240 mask) | PVR-060, PVR-064, PVR-006, PVR-105 |
| TR | Translucent layer | A blended layer | 3.1 ms full screen at 640 × 480; 0.77 ms at 320 × 240 on a 3.2 ms target floor | PVR-002, PVR-004 |
| DA | Destination alpha | The frame keeps an alpha that later blends read (DESTALPHA, INVDESTALPHA) | A compare in destination alpha worked (Compare by blending [c1]); the library's trail pass uses it as a column mask | PVR-040; `src/sh4der_trail.c:127-151` |
| MV | Modifier volume | Exact inside/outside per pixel, in hardware | Cost did not show in one test | PVR-044 |
| RTT | Render to texture | A pass into a 16-bit target, textured in the next pass | 3.2 ms floor a pass through the whole list | PVR-004, PVR-020, PVR-021 |
| READ | Frame read | The SH-4 reads pixels back | PVR-DMA 36-38 MB/s; cached 38.7 MB/s; uncached 7.5 MB/s | BUS-021, BUS-051, SH4-040, SH4-041 |
| VTX | Vertices | Every extra mesh copy | 142 ns a vertex in store-queue runs; 260 ns one at a time | BUS-003, BUS-001 |
| SH4 | SH-4 per-vertex maths | Any function written into colour, alpha or UV | FIPR 38.1 cycles a point, FTRV 16.6, in the cache; a ray through a 64 × 64 height grid 22 cycles a cell + 71-74 a ray; a proxy test 138 cycles a pair | SH4-017, SH4-020, SH4-052, SH4-055 |

**What the PVR cannot do** (FACT unless marked): read the depth buffer (it stays on chip per tile, PVR-083); offset a texture coordinate per pixel from another texture (no dependent read: the bump unit gives a level, not an offset); use a rendered colour as a texture coordinate (so no colour lookup table on the frame); 3-D textures; float targets.

## Corrections to the sources' "fits the PVR" remarks

Our reader judged the fits from a short brief. Five of its remarks are wrong against our own ledger, and are corrected here:

| Idea | Reader said | Our ledger says |
|---|---|---|
| Mask bit, stencil-less volumes (Part 2) | The PVR has no destination alpha to read | It has one: blends read and write the frame's alpha (PVR-040), and the library already masks a pass with it (`sh4der_trail.c:127-151`) |
| Stencil-less shadow volumes | Would need a per-pixel count the PVR lacks | The PVR's modifier volumes do the inside test in hardware, exactly in one test (PVR-044) |
| Colour grading by a lookup table | A 2-D strip of slices works as a post pass | No: the PVR cannot use a rendered colour as a texture coordinate. Grades are blend layers (`sh4der_look.h`) or palette rewrites of paletted art |
| Fog table "129 entries, OPEN" | A conflict with our 128 | Not a conflict: the hardware has 128 entries of two bytes, each holding a value and its neighbour for interpolation (Sega §3.4.8.1, FACT; `POLLY2_NOTES.md` (3), REPORTED). KallistiOS takes 129 points to fill those 128 pairs (ESTIMATE from the two) |
| In-place drawing on aliased buffers | Out of reach | The same buffer cannot be read and written in one pass, but a target rendered in one pass is a texture in the next (PVR-020): the two-pass form works |

## Part 1: GPU techniques of about 2003-2010

"Route" names the pieces above. "Host test" is what would check the maths before any console run.

| # | Technique (source) | Mechanism, in short | Route on our hardware | Cost (ledger) | Host test | Useful / crazy | Status |
|---|---|---|---|---|---|---|---|
| N1 | Spherical-harmonic irradiance (Ramamoorthi, Hanrahan 2001, F) | Diffuse light from a whole sky is a quadratic in the normal: nine numbers an environment | SH4: per vertex, three FIPRs a colour channel, into vertex colour | About 9 FIPR a vertex: ~340 cycles if each costs SH4-017's 38.1 (ESTIMATE; pipelined FIPRs should cost less) | Against a brute-force hemisphere sum of a sky picture: the paper's ~1 % mean error | high / low | ESTIMATE |
| N2 | Precomputed radiance transfer (Sloan, Kautz, Snyder 2002, F abstract) | Per vertex, a baked vector that turns a low-frequency light into outgoing light, self-shadowing included; at run time one dot product | SH4: 9 or 16 floats a vertex, FIPR in fours, static geometry | 3-4 FIPR a vertex a channel; RAM 36-64 bytes a vertex (ESTIMATE) | Bake on the host; compare with a ray-traced reference for one sun | med / low | ESTIMATE |
| N3 | Ambient occlusion fields (Kontkanen, Laine 2005, F) | Around each occluder, a stored field of how much it blocks; receivers sample nearby fields | SH4: a small radial table per object, read per ground vertex | A few table reads a vertex (ESTIMATE) | Against a ray-cast occlusion of a box on a plane | med / low | ESTIMATE |
| N4 | Ambient aperture lighting (Oat 2006, F outline; Oat, Sander 2007, S) | Visible sky as a spherical cap per vertex; light overlap as cap overlap | SH4: one cap per vertex from a bake, one dot per light | A dot and a table a vertex (ESTIMATE) | Against the ray-cast visible fraction | med / low | ESTIMATE |
| N5 | Fur by shells and fins (Lengyel et al. 2001, F) | Stacked copies pushed out along the normal, each cut by a hair texture; fins at silhouettes | PT, VTX: each shell's cut-off set by its vertex alpha (PVR-064). Same family as [C5 and M3](depth-shading-ideas.md) | N × the mesh's vertices at 142 ns, N × its punch-through fill (3.3-4.3 ms a full-screen layer) | Count punch-through pixels a shell under the product rule against the intended height slices | high / low | ESTIMATE; the product's rounding OPEN (Q16) |
| N6 | Stochastic (screen-door) transparency (Enderton et al. 2010, F) | A transparent surface covers a random share of samples equal to its alpha: no sorting | PT: a dither texture's alpha × vertex alpha against one reference (PVR-064), one layer, no sort. [C6](depth-shading-ideas.md) | One punch-through layer of the object | Coverage of a 4 × 4 pattern per vertex alpha step against the intended alpha | high / med | ESTIMATE |
| N7 | Billboard clouds (Décoret et al. 2003, F) | A model replaced by a few textured planes with alpha, chosen to stay within an error | PT, VTX: a handful of quads a tree or a far prop | Tiny vertex count; the cost is overdraw in a tile (PVR-009: 256 translucent triangles in one tile 6.4 ms) | Silhouette difference against the model from 16 views | med / low | ESTIMATE |
| N8 | View-aligned volume slices (Engel, Kraus, Ertl 2001, F) | A volume as a stack of slices facing the eye, blended | TR, FOG: 4-8 slices, the fog table as a free per-pixel falloff; `sh4der_mist.h` is this family | 0.77 ms a 320 × 240 slice layer, 3.1 ms at 640 × 480 | Slice count against step visibility in a host render | med / med | ESTIMATE |
| N9 | Soft particles (Lorach 2007, F) | A sprite fades where the scene behind it is close | CMP from the camera ([D1](depth-shading-ideas.md)); or a fixed-plane fade per vertex with no map | The camera map pass (3.3 ms whole list) plus the sprite twice | Fade curve against the depth gap in the Full compare chain host model | med / low | ESTIMATE; needs the camera map |
| N10 | Cascaded shadow maps (Dimitrov 2007, F) | Several shadow maps by view-depth slice | Route B's RAM map per slice, or route A silhouettes per slice; the split rule is already in `GPU_GEMS_NOTES.md` | One map pass per slice (PVR-092: 375 us a small map cut to its tiles) | The split depths against the texel footprint in `test/psm_test.c`'s terms | med / low | ESTIMATE |
| N11 | Reverse reprojection cache (Nehab et al. 2007, F) | Reuse last frame's shading where the surface point is still the same | SH4: per-vertex lit colours cached and refreshed in bands, the thesis's cached sun visibility in another form | Saves the per-vertex lighting on unchanged vertices (ESTIMATE) | Error of the stale colour against a fresh one over a camera path | med / med | ESTIMATE |
| N12 | Depth and normal outlines (Saito, Takahashi 1990, F) | Edges from jumps in a depth and normal image | Mesh outlines (inverted hull, [D6](depth-shading-ideas.md)) instead; the image form needs neighbour compares (OPEN) | Doubles the outline mesh's vertices | Edge set against a host image-space detector | med / low | ESTIMATE |
| N13 | Parallax occlusion and relief mapping (Tatarchuk 2006, F partial; Policarpo et al. 2005, S) | March the view ray through a height map per pixel | Not per pixel (no dependent reads). Shells with one height texture ([M3](depth-shading-ideas.md)) give real parallax by layers | N layers of fill | As N5 | med / low | per pixel not possible; layered ESTIMATE |
| N14 | Bokeh sprites (Gems 3 ch. 28 summary, S) | Bright out-of-focus points drawn as aperture-shaped sprites | READ for the bright list (an 80 × 60 grid exists in `sh4der_frame.h`), additive sprites (`sh4der_spark.h`) | Sprite fill; the frame read 1.2 ms at 80 × 60 (VID-005, REPORTED) | Bright-point selection on a host frame | low / med | ESTIMATE |
| N15 | Morphological antialiasing (Reshetov 2009, S) | Find edge shapes in the finished image and blend along them | READ plus SH-4 pattern search | A full-frame read and search is far over budget (ESTIMATE) | — | low / low | ESTIMATE: no |
| N16 | Variance and exponential shadow maps (Donnelly, Lauritzen 2006, S; Annen et al. 2008, F) | Store moments or an exponential so the map can be filtered | Route B in RAM could hold a moment map in 16 bits; the whitepaper says 16 bits are too few for moments | — | Light-bleed and precision on the host model at 16 bits | low / high | ESTIMATE: poor |
| N17 | Light pre-pass and inferred lighting (Engel 2008, F; Kircher, Lawrance 2009, F) | Light into a buffer first, then shade the scene from it | Needs readable multiple targets: no. The idea that survives is a small lit field in a texture, drawn back (RTT) | — | — | low / med | ESTIMATE: no |
| N18 | Screen-space reflections (McGuire, Mara 2014, F) | March a reflected ray across the depth image | No depth read. Planar reflections are already `sh4der_pl4nar.h` | — | — | low / med | ESTIMATE: no |
| N19 | Light propagation volumes (Kaplanyan, Dachsbacher 2010, F via a blog) | Light spread cell to cell through a coarse 3-D grid | SH4: a coarse grid of SH coefficients updated slowly, read per vertex | Very large (ESTIMATE) | — | low / high | ESTIMATE |
| N20 | Reflective shadow maps (Dachsbacher, Stamminger 2005, S) | The light's view as a set of small secondary lights for one bounce | Route B's RAM map with a colour per texel; a few samples per vertex | Large (ESTIMATE) | One-bounce colour against a host path tracer on a box room | med / high | ESTIMATE |
| N21 | Fresnel, wrap, subsurface layers (Schlick 1994, S; Hanrahan, Krueger 1993, F) | Rim reflection and soft terminator terms | SH4 per vertex (`sh4der_water.h`, `sh4der_sss.h`); per pixel by a ramp ([M1](depth-shading-ideas.md)) | One FIPR a vertex | Already host tested in the library | high / low | in the library; M1 ESTIMATE |

Not ported (need what the PVR lacks, and no spirit survives): horizon-based screen-space occlusion from depth (needs neighbour depths, OPEN); 3-D colour lookup tables (no dependent reads).

## Part 2: fixed-function tricks of about 1995-2005

| # | Trick (hardware, source) | Mechanism, in short | Our nearest | Status |
|---|---|---|---|---|
| H1 | **Mask bit** (a 1994 console's GPU; nocash PSX-SPX reference, F, REPORTED) | Each 16-bit pixel's top bit can be set on write; a mode then refuses to overwrite pixels whose bit is set | **Destination alpha.** A blend with DESTALPHA / INVDESTALPHA lands only where the frame's alpha says (the library's trail pass, `sh4der_trail.c:127-151`). A candidate for **Q58**: a shadow pass that never clears a bit an earlier one set | ESTIMATE; for Q58, OPEN whether the bump level can drive it |
| H2 | Four semi-transparency formulas a primitive (same console, F) | Average, add, subtract, add a quarter | The PVR's blend factors express average and add directly; subtract has no factor (ESTIMATE) | ESTIMATE |
| H3 | **Shadow by halving marked pixels** (a 1994 console's sprite processor; Sega's manual, F, FACT for that chip) | A "shadow" draw halves the colour of pixels already drawn whose top bit is set | Modifier volume shading (MV), or a DESTCOLOR multiply layer over a destination-alpha mask (DA) | ESTIMATE |
| H4 | Alpha compare with a random threshold (a 1996 console's display processor; its programming manual, F, FACT; the manual disagrees with itself on modes, DISPUTED) | Pixels dropped against a fixed or a noise threshold | Punch-through with a noise texture × vertex alpha (PVR-064): the threshold is fixed, the noise is in the texture | ESTIMATE |
| H5 | Stenciled shadow volumes, made robust (Everitt, Kilgard 2002, F) | Count volume faces per pixel; the far plane moved to infinity | The PVR's modifier volumes do the inside test in hardware (PVR-044); the owner excluded volumes as the shadow method here | FACT (PVR-044); route not chosen |
| H6 | **Cheap-shadow modifier volumes** (KallistiOS docs, F, REPORTED) | Inside a volume, the final colour is multiplied by one scale a frame | MV: exact in our test, cost not seen (PVR-044). Town scale is spike Town-scale test [c2], built, not run (Q19) | MEASURED (one test); scale OPEN |
| H7 | Projected textures and planar shadows (Segal et al. 1992, S) | A light's picture projected onto receivers | Route A's silhouette map; blob shadows in `sh4der_pool.h` | in the library |
| H8 | Accumulation buffer (Haeberli, Akeley 1990, F) | Average of jittered passes: antialiasing, soft shadow, blur | TR sums into a 16-bit target, 2-4 passes; the shelved soft shadows (T1) are this, and too dear at 6.1-10.2 ms a mask pass | ESTIMATE |
| H9 | Environment-mapped bump (a 1999 vendor article, F, REPORTED) | A per-pixel offset from a bump map shifts a second lookup | Not per pixel on the PVR; per vertex as `sh4der_warp.h` and `sh4der_uvwarp.h` | per pixel not possible |
| H10 | Dot-product bump by combiners (Kilgard 2000, F partial) | Per-pixel N·L in the texture stage | The PVR's bump unit does this in hardware (PVR-045) | FACT (unit) |
| H11 | Depth as a texture (ARB_depth_texture, 2002, F, FACT) | The depth buffer copied to a readable texture | No depth read; Full compare chain's ramp pass makes a depth code instead (PVR-095) | MEASURED (the ramp form) |
| H12 | Fog table as a depth ramp (KallistiOS docs, F) | A free per-pixel function of depth | FOG: one colour, any curve (PVR-049); `sh4der_torch.h` bends it | FACT |
| H13 | **Palette cycling** (Wikipedia, "Color cycling", F) | Rotate palette entries; the image seems to move at no pixel cost | Paletted textures, one palette value a scene (PVR-043): water, fire, signs for nothing per pixel. Palette textures are always twiddled (PVR-072), so not a rendered target | ESTIMATE |
| H14 | Render to texture and frame copy (KallistiOS docs, F partial) | Draw into a texture, draw it back | RTT: the base of glow, warp, trail, ca, focus in this library | in the library |
| H15 | Scaled-copy blur (a 2003 talk, S) | Several passes of four offset bilinear samples | RTT + TR: `sh4der_glow.h` and `sh4der_warp.h` do scaled copies | in the library |
| H16 | In-place drawing on aliased buffers (a 2000 console's GS reference, F, REPORTED) | Read the frame as a texture while writing it | Two passes through RTT (PVR-020) | ESTIMATE |
| H17 | 4 × 4 ordered dither before 15-bit output (a 1994 console, F) | Banding turned to fine noise | The PVR's own dither switch; it must be off for depth codes in a target (PVR-022) | MEASURED (PVR-022) |
| H18 | Coverage bits in alpha (a 1996 console's manual, F, FACT) | A pixel's partial coverage kept in spare alpha bits | ARGB4444 or 1555 targets keep 4 or 1 alpha bits (PVR-028, PVR-029) | MEASURED (formats) |

## Most useful (ESTIMATE, our ranking)

1. **N1, spherical-harmonic sky light per vertex.** Nine numbers a sky, FIPR per vertex, no texture, no pass. A host test settles the error before any console run.
2. **N6, screen-door transparency by punch-through.** Order-free translucency in one layer; rests on PVR-064.
3. **N5 / M3, shells for fur, grass, moss and relief,** each shell's cut-off set by its vertex alpha.
4. **H6, modifier-volume shadows,** exact on the PVR, cost not seen in one test; the town-scale test is spike Town-scale test (Q19).
5. **N2, precomputed radiance transfer** for a static set: per-vertex vectors, FIPR, soft self-shadow in a sky light.
6. **H1, the destination-alpha mask** as the join for several shadow compares (Q58); the library already uses the mechanism in `sh4der_trail.c`.

## Craziest (ESTIMATE)

1. **H1 for Q58:** a console-era protect bit, rebuilt from destination alpha, as the way three Full compare chain compares join into one mask.
2. **N20, one bounce of colour from route B's RAM map**, per vertex.
3. **N19, a slow light grid** of SH coefficients on the SH-4.
4. **H13, palette cycling on paletted ground art** for animated water and lamps, one palette value a scene.
5. **N16, moments in a 16-bit RAM map**, to see how bad the light bleed is.
6. **H3, shadow by halving marked pixels:** a DESTCOLOR multiply landing only where destination alpha marks, the sprite-processor shadow in PVR terms.

## Sources

Explained here in our own words; nothing copied. Where a source is a talk or document about a named commercial product, it is not listed: lead has those.

- Ravi Ramamoorthi, Pat Hanrahan, "An Efficient Representation for Irradiance Environment Maps", SIGGRAPH 2001, graphics.stanford.edu/papers/envmap/ (F).
- Peter-Pike Sloan, Jan Kautz, John Snyder, "Precomputed Radiance Transfer for Real-Time Rendering in Dynamic, Low-Frequency Lighting Environments", SIGGRAPH 2002 (F, abstract).
- Janne Kontkanen, Samuli Laine, "Ambient Occlusion Fields", I3D 2005 (F).
- Chris Oat, "Ambient Aperture Lighting", SIGGRAPH 2006 course (F, outline); Chris Oat, Pedro Sander, I3D 2007 (S).
- Jerome Lengyel, Emil Praun, Adam Finkelstein, Hugues Hoppe, "Real-Time Fur over Arbitrary Surfaces", I3D 2001, hhoppe.com/proj/fur/ (F).
- Eric Enderton, Erik Sintorn, Peter Shirley, David Luebke, "Stochastic Transparency", I3D 2010 (F, abstract).
- Xavier Décoret, Frédo Durand, François Sillion, Julie Dorsey, "Billboard Clouds for Extreme Model Simplification", ACM TOG 22(3), 2003 (F).
- Klaus Engel, Martin Kraus, Thomas Ertl, "High-Quality Pre-Integrated Volume Rendering Using Hardware-Accelerated Pixel Shading", Graphics Hardware 2001 (F).
- Tristan Lorach, "Soft Particles", NVIDIA whitepaper, 2007 (F).
- Rouslan Dimitrov, "Cascaded Shadow Maps", NVIDIA whitepaper, 2007 (F).
- Diego Nehab et al., "Accelerating Real-Time Shading with Reverse Reprojection Caching", Graphics Hardware 2007 (F).
- Takafumi Saito, Tokiichiro Takahashi, "Comprehensible Rendering of 3-D Shapes", SIGGRAPH 1990 (F, abstract).
- Natalya Tatarchuk, "Practical Parallax Occlusion Mapping", SIGGRAPH 2006 course (F, partial); Fabio Policarpo, Manuel Oliveira, João Comba, "Real-Time Relief Mapping on Arbitrary Polygonal Surfaces", I3D 2005 (S).
- Alexander Reshetov, "Morphological Antialiasing", HPG 2009 (S).
- Kevin Myers, "Variance Shadow Mapping", NVIDIA whitepaper, 2007 (F); William Donnelly, Andrew Lauritzen, I3D 2006 (S); Thomas Annen et al., "Exponential Shadow Maps", GI 2008 (F, abstract).
- Wolfgang Engel, "Light Pre-Pass Renderer", blog, 2008 (F); Scott Kircher, Alan Lawrance, "Inferred Lighting", SIGGRAPH 2009 Sandbox (F, abstract).
- Morgan McGuire, Michael Mara, "Efficient GPU Screen-Space Ray Tracing", JCGT 3(4), 2014 (F, abstract).
- Anton Kaplanyan, Carsten Dachsbacher, "Cascaded Light Propagation Volumes", I3D 2010 (F, via a blog summary).
- Carsten Dachsbacher, Marc Stamminger, "Reflective Shadow Maps", I3D 2005 (S).
- Pat Hanrahan, Wolfgang Krueger, "Reflection from Layered Surfaces due to Subsurface Scattering", SIGGRAPH 1993 (F, abstract); Schlick 1994 (S; see [depth shading ideas](depth-shading-ideas.md)).
- Martin Korth, "nocash PSX-SPX" GPU reference, a community reverse-engineering document (F, REPORTED).
- Sega, VDP1 User's Manual, ch. 6 "Color calculation", via a fan-hosted copy of the Saturn hardware manual (F, FACT for that chip).
- Nintendo / SGI, Ultra64 Programming Manual, ch. 15 "Color Blend Hardware" (F, FACT for that chip).
- Cass Everitt, Mark Kilgard, "Practical and Robust Stenciled Shadow Volumes for Hardware-Accelerated Rendering", NVIDIA 2002, arXiv cs/0301002 (F).
- KallistiOS API documentation: `pvr_global` (shadow scale), `pvr_fog`, `pvr_txr_mgmt` (F).
- Mark Segal et al., "Fast Shadows and Lighting Effects Using Texture Mapping", SIGGRAPH 1992 (S).
- Paul Haeberli, Kurt Akeley, "The Accumulation Buffer", SIGGRAPH 1990 (F, abstract page).
- A 1999 graphics vendor article on environment-mapped bump mapping (F, vendor claims not checked).
- Mark Kilgard, "A Practical and Robust Bump-mapping Technique for Today's GPUs", GDC 2000 (F, partial).
- Khronos, GL_ARB_depth_texture specification, 2002 (F).
- Wikipedia, "Color cycling" (F).
- "ps2tek" GS effects reference, a community document (F, REPORTED).

## Related

- [GPU Gems depth techniques](gems-depth-techniques.md)
- [Depth shading ideas](depth-shading-ideas.md)
- [Depth fog ideas](depth-fog-ideas.md)
- [PVR ledger](../hardware/ledger-pvr.md)
- [Open questions](../open-questions.md)
