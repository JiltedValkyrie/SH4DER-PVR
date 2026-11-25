# demo/psm: GPU Gems chapters that apply to SH-4 per-vertex shadows

This is a reading list for the thesis (`THESIS_SH4_SHADOWS.md`) and for Senior's equations. Research only; nothing here was built.

**Where the text came from.**

- **No local copy** of GPU Gems was found under `D:\Workspace-Claude`. A search to depth 6 found no files named "gems".
- The repo only cites chapter 14's NVIDIA PDF (`demo/psm/CREDITS.md`, `README.md`, `include/sh4der_psm.h`).
- Every chapter below was read on NVIDIA's free online edition, https://developer.nvidia.com/gpugems/ , on 2026-11-25.

**Page numbers.** The online edition has no printed page numbers, so the "Where" column gives chapter and section numbers. The printed pages could not be checked.

**Equations.** The online edition shows most equations as images. Equations quoted from code or text on the page are marked *page*. Those written from the chapter's own notation are marked *transcribed*: check them against the page image before citing.

## Chapters

"Lacks" = what the chapter assumes that the SH-4 / PVR2 does not have: per-pixel shaders, depth textures, float targets, occlusion queries, a stencil buffer.

| Book, ch. | Title, authors | Where | What to take | What we lack |
|---|---|---|---|---|
| Gems 1, ch. 13 | Generating Soft Shadows Using Occlusion Interval Maps (William Donnelly, Joe Demers) | §13.2–13.4 | **The closest match to cached per-vertex terms.** For a static scene and a light on a fixed path (the sun), store per point the "rise" and "fall" times when it is lit; the soft shadow is a box-filtered overlap of those intervals with the light's width, computed with "just min, max, and subtraction" (§13.4). Per vertex instead of per texel works the same. | Per-pixel lookup and 4-channel textures. Ours: per-vertex tables in RAM, evaluated on the SH-4 or baked into vertex colour. Static scenes only, as the chapter says (§13.5). |
| Gems 1, ch. 17 | Ambient Occlusion (Matt Pharr, Simon Green) | §17.2 | Precomputed accessibility (fraction of rays unoccluded, `numUnoccluded / numRays`) and the bent normal (the average unoccluded direction), **per vertex** in its first form. Fits a load-time or offline ray cast against the town grid. | The hardware-accelerated variant (§17.3) uses depth maps rendered on the GPU. |
| Gems 2, ch. 14 | Dynamic Ambient Occlusion and Indirect Lighting (Michael Bunnell) | §14.1–14.2 | Surfaces as **discs (elements), one per vertex**; occlusion summed disc to disc. A second pass corrects double darkening. Analytic, no rays: a fit for a cached SH-4 pass over few elements and for analytic proxies. | Runs in fragment programs over all element pairs (O(n²) with a hierarchy). On the SH-4 only for a small n or with a cache. |
| Gems 3, ch. 12 | High-Quality Ambient Occlusion (Jared Hoberock, Yuntao Jia) | §12.2–12.3 | The known artefacts of ch. 14 above (disc-shaped, "pinching") and their smoothing. Read before using Bunnell's discs. | Per-pixel evaluation. |
| Gems 1, ch. 14 | Perspective Shadow Maps: Care and Feeding (Simon Kozlov) | §14.2–14.3 | **Already used** in `sh4der_psm.h` (virtual camera, light camera, biasing). | Depth texture and per-pixel compare. Ours: route B per vertex. |
| Gems 3, ch. 10 | Parallel-Split Shadow Maps on Programmable GPUs (Fan Zhang, Hanqiu Sun, Oskari Nyman) | §10.2.1 | The **practical split scheme**: split depths a blend of logarithmic and uniform. Applies directly to choosing map slices, or cache levels, by view depth. | Several depth maps and per-pixel selection. |
| Gems 1, ch. 11 | Shadow Map Antialiasing (Michael Bunnell, Fabio Pellacini) | §11.2–11.5 | Percentage-closer filtering: filter the compare **results**, not the depths. Our 2 x 2 PCF in the receiver-plane compare is this. | Per-pixel PCF on depth textures. |
| Gems 3, ch. 8 | Summed-Area Variance Shadow Maps (Andrew Lauritzen) | §8.4, §8.4.3 | VSM: store depth and depth², bound the lit fraction with Chebyshev, and filter the map like a texture. Light bleeding (§8.4.3) is its known flaw. For a per-vertex SH-4 compare it gives a soft edge from **one** lookup of a pre-blurred moment map. | Float render targets and filtering. Ours would be a moment map in RAM; 16-bit precision needs care (§8.4.4). |
| Gems 2, ch. 17 | Efficient Soft-Edged Shadows Using Pixel Shader Branching (Yury Uralsky) | §17.2.2 | Take a few samples first; if they all agree, skip the rest (most pixels are fully lit or fully shadowed). Per vertex: test few, refine only the vertices near a boundary. This is the thesis's "edge sharpening" budget idea. | Pixel shader branching. |
| Gems 1, ch. 12 | Omnidirectional Shadow Mapping (Philipp S. Gerasimov) | §12.2 | A point light's map as a cube; distance stored as a value (§12.2.3, §12.2.5). Only for lamps. | Cube textures, float formats. |
| Gems 1, ch. 9 | Efficient Shadow Volume Rendering (Morgan McGuire) | §9.1.1, §9.3 | Volume construction, caps, z-pass / z-fail. **Related work only**: the owner excluded volumes. | Stencil buffer. |
| Gems 3, ch. 11 | Efficient and Robust Shadow Volumes… (Martin Stich, Carsten Wächter, Alexander Keller) | §11.3.1 | Shadow volumes for non-closed, low-quality meshes. Related work only. | Geometry shaders, stencil. |
| Gems 1, ch. 15 | Managing Visibility for Per-Pixel Lighting (John O'Rorke) | §15.3–15.4 | The visible, lights, illumination and **shadow sets**: which casters can shadow what is seen. Applies directly to culling which vertices and casters a frame's refresh touches. | Nothing essential: the method is CPU-side. |
| Gems 1, ch. 29 / Gems 2, ch. 6 | Efficient Occlusion Culling (Dean Sekulic); Hardware Occlusion Queries Made Useful (Michael Wimmer, Jiří Bittner) | §29.1–29.5; §6.4–6.5 | **Temporal coherence**: reuse last frame's visibility and test only what may have changed. This is the logic of amortised refresh. | Hardware occlusion queries. Ours: a CPU grid or PVS. |
| Gems 3, ch. 9 | Interactive Cinematic Relighting with Global Illumination (Fabio Pellacini, Miloš Hašan, Kavita Bala) | §9.2–9.7 | Precompute the costly part (gather samples, transfer) once; relight cheaply when only the lights change. Cached shadow terms are the same split: geometry fixed, sun moving. | GPU sparse matrix and wavelet passes. |
| Gems 2, ch. 2 | Terrain Rendering Using GPU-Based Geometry Clipmaps (Arul Asirvatham, Hugues Hoppe) | §2.1, §2.4 | Nested grids around the viewer, updated incrementally as it moves: a pattern for a per-vertex shadow cache that is refreshed in rings. | Vertex textures. |

No chapter in the three books was found on per-vertex shadow caching, ray casting through a grid for shadows, or analytic sphere or capsule occlusion as such. Ch. 14 of Gems 2 (discs) is the nearest analytic proxy. This was checked against all three tables of contents, not by reading every chapter.

## Equations

1. **Occlusion interval, soft (Gems 1, ch. 13, §13.4).**
   - *Page*: the box filter is W_dt(t) = 1/dt for −dt/2 < t < dt/2, and 0 otherwise.
   - *Transcribed* from §13.4's text ("min, max, and subtraction"): for one rise R and fall F, the lit fraction at time t is
     V(t) = max(0, min(F, t + dt/2) − max(R, t − dt/2)) / dt.
   - Several intervals add.
2. **Disc-to-disc occlusion (Gems 2, ch. 14, §14.2, *page*, from the chapter's code):**
   occlusion = (1 − 1/sqrt(A/r² + 1)) · sat(n_e · v) · sat(4 · n_r · v)
   - A is the emitter's area divided by π.
   - r² is the squared distance.
   - v is the unit vector between the elements.
3. **Chebyshev bound, VSM (Gems 3, ch. 8, §8.4, *page*, from the chapter's code):**
   p_max(t) = σ² / (σ² + (t − μ)²), valid for t > μ.
   - μ = E[d], σ² = E[d²] − μ².
4. **Practical split scheme (Gems 3, ch. 10, §10.2.1, *transcribed*; the page shows it as an image):**
   C_i = λ·C_i^log + (1 − λ)·C_i^uni
   - C_i^log = n·(f/n)^(i/m)
   - C_i^uni = n + (f − n)·i/m
   - n and f are the near and far depths, m the number of splits, λ the blend weight.
5. **Accessibility (Gems 1, ch. 17, §17.2, *page*):**
   accessibility = numUnoccluded / numRays
   - The bent normal is the average of the unoccluded ray directions.

## Not read

- The printed books and their page numbers.
- The equation images, which were not viewed: items 1 (soft form) and 4 are transcribed.
- Chapters outside the lighting, shadow and visibility parts, apart from Gems 2 ch. 2.
