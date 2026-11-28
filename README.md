# SH4DER-PVR

## Start here

SH4DER-PVR is a set of graphics effects for the Sega Dreamcast: screen post-processing, per-vertex light and small particle systems, made for a console with no shaders. Each effect is a small module: you take the one you need and call a few functions once a frame. The SH-4 CPU does the thinking a shader would do, and the PVR (the graphics chip) does the drawing. The maths is plain C that is tested on a PC, and on the console it runs through SH4ZAM.

- **One effect, one header, one optional `.c`.** An effect is `include/sh4der_<name>.h` (the state, the maths and the tuning numbers, usually inline and testable on a PC) plus, only when it draws, `src/sh4der_<name>.c` (the KallistiOS draw part, compiled for the Dreamcast only). "Header only" in the guide below means there is no `.c` file at all.
- **SH4ZAM** is the SH-4 maths library (square roots, sines, cosines, vectors, matrices). It is used header-only through `sh4der_core.h`, which wraps it in small `sh4der_*_` helpers. Define `SH4DER_CORE_NO_SH4ZAM` to get the same steps in plain C for a PC build.
- **KallistiOS** is the Dreamcast SDK. Only the `.c` files talk to it (PVR lists, render targets, direct render); the headers never need it.
- **Build and test** (the comment at the top of the `Makefile` lists every target):
  - `make test` runs every test on a PC with the plain C path.
  - `make test-shz SH4ZAM=<checkout>` runs the same tests with SH4ZAM's software back-end (SH4ZAM 0.10 or later).
  - `make dc SH4ZAM=<checkout>` builds the Dreamcast library `build/libsh4der_core.a` (KallistiOS environment sourced); `make test-dc SH4ZAM=<checkout>` builds the tests as Dreamcast programs.
- Unfamiliar word? See the [PVR glossary](#pvr-glossary). First effect to try: [blade](#the-blade-effect), which has a complete example. Adding your own: [Adding an effect](#adding-an-effect).

## Guide

Every module in `include/`. Cost is RAM / VRAM / CPU as the section or header states it; "see section" means the section gives no single number.

| Name | What it does | When to use it | Cost (RAM / VRAM / CPU) | Files | Section |
| --- | --- | --- | --- | --- | --- |
| ao | Darkens creases from the angle between faces (at bake time) and adds a screen cavity cue from a small map. | Baked crevice shading for a model, or a cheap cavity cue on a finished frame. | see section | `sh4der_ao.h` (header only) | [section](#the-ao-effect) |
| aobake | Baked ambient occlusion against exact shapes: one byte a vertex from cosine rays cast at spheres, boxes and a ground plane, paid once at load. | Contact shadows baked into static geometry: room corners, the ground under props. | RAM one byte a vertex plus 768 B for 64 rays; VRAM 0; nothing a frame | `sh4der_aobake.h` (header only) | [section](#the-aobake-effect-baked-occlusion-against-exact-shapes) |
| aomesh | Baked ambient occlusion over a triangle mesh: one byte a vertex from 12 rays through a cell grid of the mesh, and a byte grid over the ground, paid once at load. | Contact shadows on static scenery built as a triangle mesh (a walled street), and the ground's bytes under a ground that moves with the camera. | RAM one byte a vertex and a byte a ground cell, plus the bake's scratch; VRAM 0; nothing a frame | `sh4der_aomesh.h` (header only) | [section](#the-aomesh-effect-baked-occlusion-over-a-triangle-mesh) |
| raytri | One ray-triangle kernel for the library's bakes and the ray bench: the numerators with the sign taken off once, a distance by the reciprocal form, a hit test with no divide, and a form prepared for one direction. | The bakes that test rays against triangles (aomesh), and any ray set over a fixed set of triangles. | RAM nothing kept (the numerators are on the stack; 64 B a prepared triangle a direction); VRAM 0; no frame cost; time NOT MEASURED | `sh4der_raytri.h` (header only) | [section](#the-raytri-effect-a-ray-against-a-triangle) |
| menu | The logic of a menu with nothing drawn: a list whose headings the cursor steps over, tabs that each keep their place, a scrolled text and its bar, a held direction that repeats, a button that is tapped or held, word wrap, and the menu's place in one 32-bit word. | A menu, a settings page or a result browser in any program; the suite disc's menu and the hold of A on a test's last page use it. | RAM 4 B a row, 20 B the state; VRAM 0; no frame cost of its own; time NOT MEASURED | `sh4der_menu.h` (header only) | [section](#the-menu-logic) |
| suite | The disc's way between programs: a menu starts a test and the test goes back to it, through KallistiOS's `arch_exec`, with a 32-byte mail written into the other program's image; the four buttons go back to the menu. | A disc of several test programs that must not be switched off between tests. | RAM the menu's binary (read at start, kept while the program runs); VRAM 0; no frame cost | `sh4der_suite.h` (header only) | [section](#the-suite-chaining-programs-on-one-disc) |
| aosky | Occlusion of the sky drawn into the sun's shadow target: K slid shadows as a faint black, one side, plain blending. | Contact darkening on flat ground under props, with no bake. | VRAM 0 more (the sun's target); RAM the caller's scratch, 9 B a caster vertex | `sh4der_aosky.h` (header only) | [section](#the-aosky-effect-occlusion-of-the-sky-drawn-into-a-shadow-target) |
| blade | Light tubes orbit an axis, a soft blocker runs through them and a pulse breathes over both, all written into vertex colours. | Energy blades, glowing rods, anything that is a bright tube. | RAM 156 B a frame + 160 B params; VRAM 0; CPU two 2D tests and one 3D test a vertex | `sh4der_blade.h` + `sh4der_blade.c` | [section](#the-blade-effect) |
| bloom | A feathered glow ribbon and tip disc round a blade (24 triangles), plus `sh4der_bloom_rings` for a round glow with its own level per ring. | A soft halo round a blade, a sun disc, a lamp or a lens-flare ghost. | RAM the caller's vertices; VRAM 0; 24 triangles a blade | `sh4der_bloom.h` + `sh4der_bloom.c` | [section](#the-bloom-effect) |
| bolt | Thin jagged branching strands that are re-rolled a few times a second while their first half stays put. | Lightning, electric arcs, spark gaps, cracks of light. | RAM about 3.3 KB state; VRAM 0; 240 vertices a frame (default look) | `sh4der_bolt.h` + `sh4der_bolt.c` | [section](#the-bolt-effect-branching-strands-that-re-roll) |
| bump | A normal map lit by a moving light, put on with ordinary blend passes (the PVR bump unit is not used). | Relief on a large flat surface (a floor, a wall) lit by a torch or other moving light. | RAM 12 KB of normals at 64 x 64; VRAM 36 KB for nine grey maps at 64 x 64; up to eight draws a lit cell | `sh4der_bump.h` (header only) | [section](#the-normal-map-effect) |
| ca | The frame texture (the world drawn into a render target) and chromatic aberration through it: red, green and blue copies at slightly different scales. | A lens colour fringe, or any effect that needs the whole frame as a texture. | VRAM 160 KB (320 x 240) or 640 KB (640 x 480); three small meshes | `sh4der_ca.h` + `sh4der_ca.c` | [section](#the-frame-texture-and-chromatic-aberration) |
| core | The shared parts: the soft feather edge, tube and blocker shapes, the breath tracker, the glint, the colour ramp and the SH4ZAM helpers. | Always included by the other modules; use it directly to build an effect of your own. | VRAM 0; RAM only the small breath tracker state; see section | `sh4der_core.h` + `sh4der_core.c` | [section](#the-core-shared-parts) |
| focus | Fake depth of field: a small blurred map of the frame drawn through a vertex-alpha mask that keeps a disc, band or rectangle sharp. | Tilt-shift looks, a sharp subject with a soft background. | RAM 1,440 B working rows; VRAM 16 KB (128 x 64 RGB565) | `sh4der_focus.h` + `sh4der_focus.c` | [section](#the-focus-effect) |
| frame | Reads the finished frame back on a grid and stamps every frame with a number, so a read knows which frame it saw. | Whenever look, glow or shafts need to measure the picture without being fooled by frame delay. | see section; VRAM a small 4 x 4 pixel stamp block | `sh4der_frame.h` + `sh4der_frame.c` | [section](#the-frame-read) |
| glow | Screen bloom from a small bright map of the frame, a wide halo, a balancing darken layer and god rays. | Bright lights bleeding into the picture, sun shafts. | RAM 17 KB staging; VRAM 17 KB (two maps) | `sh4der_glow.h` + `sh4der_glow.c` | [section](#the-glow-effect) |
| guard | Two checks for what an emulator forgives and a real console does not: a triangle with no screen area, and a polygon header that was never compiled. | Before sending any hand-made quad or fan to the PVR. | RAM 0; VRAM 0; a few multiplies a triangle | `sh4der_guard.h` (header only) | [section](#real-hardware-notes) |
| holo | A light inside a translucent body follows the body's own breathing (the slow rise and fall of the chest). | Holograms, ghosts, anything see-through with an inner glow. | RAM 272 B a frame + 44 B params; VRAM 0; one load a vertex | `sh4der_holo.h` + `sh4der_holo.c` | [section](#the-holo-effect) |
| lens | Overlays that need no frame read: a vignette mesh, mean-neutral film grain, hit direction wedges and a full-screen flash. | Camera look, damage indicators, flashes. | VRAM 8 KB (64 x 64 grain tile); vertex-colour meshes | `sh4der_lens.h` + `sh4der_lens.c` | [section](#the-lens-effect) |
| look | An eye that adapts to the frame's brightness both ways, a grade (a straight line per channel) and glare in the fog. | Auto exposure and a colour grade for the whole picture. | CPU 4,800 uncached loads + a 64-bin count a frame; VRAM 0; 0 to 6 full-screen quads | `sh4der_look.h` (header only) | [section](#the-look-effect) |
| motes | A steady field of drifting specks (embers, dust, pollen, ash) with nothing stored per speck. | Ambient floating particles round the player or a source. | RAM no state, 3,072 B collect buffer at 96 sprites; VRAM 512 B mask; at most 96 sprites a frame by default | `sh4der_motes.h` + `sh4der_motes.c` | [section](#the-motes-effect-a-steady-field-of-drifting-specks) |
| dofmip | Focus blur from distance: each polygon or object gets the texture mip bias its depth from a focus plane asks for (nothing in a sharp zone, up to 2.75 levels past it), so a camera's focus pull is one number a polygon. | A cutscene focus pull or rack focus, a soft background behind a close-up, at no pass and no video memory. | RAM 116 B a frame; VRAM 0; CPU one reciprocal and 4 compares a polygon or object (not measured) | `sh4der_dofmip.h` | [section](#the-dofmip-effect-focus-blur-by-mip-bias) |
| psm route A | A few moving shadows from one light: up to four characters drawn flat from the light by the PVR into a 128 x 128 target (the pass), the ground patch drawn again as a multiply layer that looks the target up. Each caster has a strength 0 .. 1. | Characters and props on a floor, with no depth compare on the SH-4: no self-shadow. | RAM about 5 KB; VRAM 48 KB (128 map, drawn at tile row 2) or 32 KB at row 0; CPU the casters' vertices through one matrix and a clip; PVR map pass in ra2 (one console, MEASURED): 3.2 ms uncut -> 0.2 ms cut at 128 and 0.8 ms at 256, frame 16.6 -> 20.0 ms; cause (sleeping render-done waits) DERIVED from KallistiOS source, to be tested by ra5; polled waits via pvrx and held single-table cut NOT RUN on a console | `sh4der_psm_a.h`, `sh4der_psm_a.c` | [section](#the-psm-route-a-pass-moving-shadows-the-pvr-draws) |
| deform | A depth grid for soft ground: feet and wheels press dents in, a rim stands up round them, snow keeps the dent and mud closes it; plus a short list of footprint decals. | Footprints and trails in snow, mud, sand or ash under a ground mesh you already draw. | RAM 25 KB for a 160 x 160 window at 0.25 m (one byte a cell), 644 B for 32 decal prints; VRAM 0; four byte loads a ground vertex; no triangles of its own | `sh4der_deform.h` | [section](#the-deform-effect-ground-that-keeps-a-dent) |
| mist | A fog bank with a shape: upright translucent sheets one behind the other, each with drifting noise and a top edge that fades out. | Ground fog in a street, a mist bank on water, steam, low cloud: fog that things walk into. | RAM 68 B; VRAM 8 KB for a 64 x 64 noise texture (the caller's); 4 triangles a slice, 48 at the default 12; fill is the cost (not measured) | `sh4der_mist.h` | [section](#the-mist-effect-a-bank-of-slices) |
| rise | Points of light that sit still, lift off one after another, drift up with a sway, shrink and cool, and come to rest as stars that twinkle. Nothing stored per light. | Candles becoming stars, lanterns released, fireflies leaving a bush, sparks that turn into a night sky. | RAM 84 B parameter block, 0 a light; VRAM 0 of its own; about 50 multiplies and one sine-cosine pair a light a frame; at most N quads on the translucent list, additive (counts, not measured) | `sh4der_rise.h` | header comment |
| film | The wear of old film in a projector: a brightness that swings a few per cent (never a strobe: at most 8 % peak to peak and 3 hertz), thin scratches that come and go, dust specks, and a gate weave of a pixel. Nothing stored: every value is computed from the time and a seed. | Old newsreel, silent film, flashback, found footage; with the lens vignette and grain for the whole look. | RAM 92 B parameter block; VRAM 0 of its own; one multiply rectangle, up to 3 lines and 12 small squares a frame on the translucent list; two sines a frame (counts, not measured) | `sh4der_film.h` | header comment |
| timebase | A video tape deck's line timing errors as a television shows them: for one field, how far each row of the picture sits sideways (the head switching step on the bottom rows, the drum's flutter, the top hook of a tape under the wrong tension, all seen through the set's horizontal lock loop). A port to C of the time-base model of NTSCRT by Finn McKenty (MIT; the notice is in the header). Draw the frame texture as bands with `sh4der_ca_channel_rows` and move each band by its row's shift. | A tape look for a cutscene, a security monitor, a flashback, a broken broadcast. | RAM 40 B parameter block and 4 B state; about 4.2 KB of stack during a call; VRAM 0; two sines a line and, with the line noise on, a logarithm and six hashes a line (counts, not measured) | `sh4der_timebase.h` | header comment |
| hexheat | Heat haze as a lattice of small hexagons fixed on the screen over a hot far view. Each hexagon carries the picture behind it and is moved a little (about a quarter of its size, 2 pixels at the defaults, at the haze's far line; never more than 0.32 of it, 2.56 pixels) by a smooth wave field that rises through the lattice, goes with the far picture in a turn and streams toward the camera as it drives: a shimmer, no hexagon leaves its place and nothing is rolled at random (in a turn the offsets change faster: about 1 pixel a frame at 0.5 rad/s, host). Each cell is judged at its world point, found by a ray from the camera through the cell to the ground (the step a strategy game uses to find where the mouse points). The picture is this frame's: the client draws the far world a second time into a 256 x 128 band of a render target it already fills (no copy, no frame late). | A road's or a runway's far end by day, a desert horizon, the air over a far fire. | VRAM the band, 64 KB at 16 bit, in a target already allocated; RAM 8 B state and a 192 B view; SH-4 about 160 cells walked a frame (158 in the street demo's view: a ray each, and two exp and a root for a cell that passes the fade), 131 of them drawn there (3 sine-cosine pairs each), and the far world sent again into the band; TA 131 hexagons, 786 strip vertices a frame there (counts on the host, not measured) | `sh4der_hexheat.h` | header comment |
| cloudvolume | A cloud deck you fly through: a software ray march through a cloud slab (cover map, tiling noise erosion, sun light with Beer and forward scatter) into a small 8-bit buffer that the PVR draws as one filtered quad over its own sky; real fly-through, so whiteout falls out of the march. Dreamcast demo with a sea under the clouds (three options) and on-screen controls. | A flight over and through cumulus, 1970s and 1980s cinema sky fly-through, any scene that needs a lit cloud layer. | RAM about 143 KB; VRAM 32 KB for the cloud texture at the default, 128 KB at the top resolution; one blended full-screen quad; CPU 14 to 22 ms a frame at the default (ESTIMATE, not measured on a console) | `sh4der_cloudvolume.h` | header comment, `demo/cloudvolume.c` |
| pl4nar | Real-time planar reflection: the world drawn a second time, mirrored about a plane, into a small render target and looked up per vertex. | Car paint, glass, chrome, wet roads, polished floors, water. | VRAM 64 KB (256 x 128 render target); 0.92 to 0.97 ms for the player car's 320 looked-up vertices on a console (street demo, one run, PVR-160); 0.43 ms for 1,623 vertices on an emulator, earlier version | `sh4der_pl4nar.h` + `sh4der_pl4nar.c` | [section](#the-pl4nar-effect-planar-reflection) |
| softfade | Soft edges for light: a glow, a pool or a beam fades out at a wall or a floor the SH-4 knows (planes and rectangles), with no depth read. | Lamp halos near walls, headlight beams and pools that run into a barrier. | CPU only: a dot and a smooth step a surface a vertex; VRAM 0; no PVR pass | `sh4der_softfade.h` (header only) | [section](#the-softfade-effect-soft-edges-for-light) |
| ramp | A linear 0 .. 1 ramp between two distances, saturated at both ends: one call, or prepared once for a loop. A helper: it draws nothing. | Fading an effect by distance (a shadow, a fake anisotropic filter's range). | CPU only: one reciprocal, then a multiply a point; VRAM 0 | `sh4der_ramp.h` (header only) | [section](#the-ramp-helper) |
| viewcull | A view cull before placement: is a box wholly outside the view (behind the near plane, past a side of the screen, past a draw distance)? And the same for a grid of points a 4 x 4 block of cells at a time. A helper: it draws nothing. | Ground windows, terrain patches and heightfields whose points are placed again every frame a camera turns. | CPU only: 12 FIPR a box, a pass over the grid's points for the blocks' boxes; VRAM 0 (not measured) | `sh4der_viewcull.h` (header only) | [section](#the-viewcull-helper) |
| pool | The light a lamp throws on the ground as a fan of triangles whose vertex alpha fades to nothing (also a dark blob shadow). | Street lamp pools and headlight beams on ground polygons too big to light per vertex. | RAM 16 B a vertex (176 B for a 10 triangle disc); VRAM 0 | `sh4der_pool.h` (header only) | [section](#the-pool-effect-light-pools-on-a-plane) |
| psm | Where the texels of a shadow map go: a uniform map for a directional light, and a perspective shadow map fitted in the camera's post-projective space so near ground gets more texels. | Sharper shadows near the camera for the same map size; a software renderer or a client with its own depth compare. | RAM 80 B a map description; VRAM 0 | `sh4der_psm.h` (header only) | [section](#the-psm-effect-perspective-shadow-maps) |
| fault | A report on the screen for an exception, a hang, an assert or an exit: registers, the stack's calls, the last frame's breadcrumbs. | A test image that dies with nothing on screen. | RAM the breadcrumbs and about 3 KB; VRAM 0; CPU about 20 instructions a vblank (ESTIMATE) | `sh4der_fault.h` (the Dreamcast part under `_arch_dreamcast`) | [section](#the-fault-catcher) |
| split | A screen cut in a left and a right half by a divider the triggers move, in 32-pixel tile columns: which user clip each half gets. | Side-by-side comparisons (one look per half). | Integer maths, no RAM to speak of; VRAM 0 | `sh4der_split.h` (header only) | [section](#the-split-screen-divider) |
| tilecut | A render target pass through a tile table its own size: the screen's tile tables end after the target's tiles for one pass and are put back after it. | A small target (a shadow tile, a mirror, a cloud buffer) that the PVR would otherwise walk as a whole 640 x 480 screen. | RAM 14,400 B (two tables' copies, the struct); VRAM 0 (it rewrites KallistiOS's tables in place) | `sh4der_tilecut.h` (header only) | [section](#the-tilecut-pass-a-render-target-through-its-own-tiles) |
| pvrx | A layer over KallistiOS's PVR driver: polled waits, a PVR-DMA download of video memory, TA error counts, the Holly revision and a user clip change. | When the PVR's waits, error counts, DMA or user clip are needed; not yet run on a console as this layer. | see AGENTS.md (v0.05) | `sh4der_pvrx.h` + `sh4der_pvrx.c` | see AGENTS.md Files |
| shafts | Shadow shafts: the frame's dark shapes cast a darker copy pushed away from a light. | Light shafts through a dark scene, the dark twin of the glow's god rays. | RAM 16 KB; VRAM 16 KB (128 x 64 RGB565) | `sh4der_shafts.h` + `sh4der_shafts.c` | [section](#the-shadow-shafts) |
| spark | A fixed pool of 24 short-lived additive quads that face the camera, as billboards or streaks. | Muzzle flashes, tracers, impact sparks, lamp halos. | RAM 1,256 B pool; VRAM 3 KB atlas; 96 vertices a frame at most | `sh4der_spark.h` + `sh4der_spark.c` | [section](#the-spark-effect-pooled-additive-quads) |
| sss | Back-lit transmission, a sub-surface scattering stand-in: light through a thin part toward the eye, tinted by absorption. | Ears against the sun, leaves, wax, ice, cloth, wave crests. | RAM 40 B parameter block; VRAM 0; about 12 multiplies and three exponentials a vertex | `sh4der_sss.h` (header only) | [section](#the-sss-effect-back-lit-transmission) |
| taa | Temporal anti-aliasing maths: sub-pixel jitter, a history weight that falls with camera speed, and a pan shift. | Smoothing a still picture without ghosting a moving one. | VRAM 0 for the history (it rides the front buffer); see section | `sh4der_taa.h` (header only) | [section](#the-temporal-aa-maths) |
| torch | A flashlight hotspot carved on screen: a radial mesh darkens outside the circle and boosts a hot core, and the fog table bends to black for reach. | First-person flashlights in dark levels. | VRAM 0; 288 triangles (306 vertices) a frame | `sh4der_torch.h` + `sh4der_torch.c` | [section](#the-torch-effect) |
| trail | The frame before drawn over this one through a copy of the front buffer: motion blur by speed and the temporal AA blend. | Motion blur and temporal smoothing. | VRAM 128 B mask, 0 for the history; full-screen layers | `sh4der_trail.h` + `sh4der_trail.c` | [section](#the-trail-effect) |
| uvwarp | Pushes each vertex's texture coordinate by a small two-channel table that scrolls with time. | A crawling arc, a rippling banner, a shimmering water strip: any distortion map the PVR cannot read per pixel. | RAM 2 KB per 64 x 16 table; VRAM 0; a bilinear read a vertex | `sh4der_uvwarp.h` (header only) | [section](#the-uvwarp-effect-per-vertex-texture-offset) |
| visor | Visor looks as number tables (night, amber, thermal): a tint through the look grade, grain, scanlines, a thermal ramp and a fade. | Helmet and goggle overlays. | VRAM 2 KB grain + 128 B scanlines; RAM 128 B ramp | `sh4der_visor.h` + `sh4der_visor.c` | [section](#the-visor-effect) |
| warp | The frame as a texture through a mesh: shockwave ring, heat haze column, underwater wobble, radial blur, pixelate and dither. | Shockwaves, heat haze, underwater, speed blur. | VRAM the frame texture: 160 KB at 320 x 240, 640 KB at 640 x 480 | `sh4der_warp.h` + `sh4der_warp.c` | [section](#the-warp-effect) |
| water | Directional waves with filtered ripple normals, Fresnel, a reflected direction and crest transmission through sss. | Sea and lake surfaces computed per vertex. | RAM the caller's wave table; VRAM 0; one sine and cosine pair per wave per sample | `sh4der_water.h` (header only) | [section](#the-water-effect-waves-and-fresnel) |


Graphics effects for the Sega Dreamcast: screen post-processing and per-vertex light, on a machine with no shaders.

The SH-4 (the Dreamcast's main CPU) does the thinking a shader would do: it reads the finished frame back on a small grid, measures it, builds small maps and lights vertices. The PVR (the Dreamcast's graphics chip) does the drawing it is good at: blended full-screen layers, small filtered textures, the fog table. Nothing here is brute force.

Two kinds of effect, each a module of its own (take one header or all of them):

- **Screen effects** (look, frame, glow, torch, warp, trail, taa, lens, focus, visor) are drawn over the finished frame as a few layers. Some use a little video memory: the glow's two maps are 17 KB together. An effect that uses the whole frame as a texture (warp) needs a frame texture: about 640 KB at 640 x 480, about 160 KB at 320 x 240.
- **Per-vertex effects** (blade, holo, bloom, ao) change the vertex colours you were already going to write: no extra polygons (the bloom adds 24 triangles), no video memory.

It needs [KallistiOS](https://github.com/KallistiOS/KallistiOS) (the Dreamcast's operating system, used here for the drawing) and [SH4ZAM](https://github.com/gyrovorbis/sh4zam) (a maths library for the SH-4) for the maths. [SH4ZAM-DCE](third_party/sh4zam-dce), header-only SH-4 kernels on top of SH4ZAM, is included in `third_party/sh4zam-dce` (the ocean demo uses it).

Repository: [github.com/JiltedValkyrie/SH4DER-PVR](https://github.com/JiltedValkyrie/SH4DER-PVR). In code the names keep the library's first name: the `sh4der_` prefix, `sh4der_core.h`, `SH4DER_CORE_NO_SH4ZAM`, `libsh4der_core.a`.

**SH4DOW-DC is part of this library**: the soft self-shadow (`include/sh4dow_dc.h`, `src/sh4dow_dc.c`), the foot cones (`src/sh4dow_foot.c`) and the heightfield sun (`include/sh4dow_heightfield.h`); see [The sh4dow effect](#the-sh4dow-effect-soft-character-shadows). Use its shadows next to these effects. The soft edge of every effect here is SH4DOW-DC's own column feather (`sh4dow_feather_in_`), called from `sh4dow_dc.h`, not copied. Put `include` on the include path, where `sh4dow_dc.h` lives. `SH4DER_CORE_NO_SH4ZAM` also sets `SH4DOW_DC_NO_SH4ZAM`, so both take the same path.

## Add it to a project

What you need:

- **KallistiOS** for the Dreamcast build. Only the drawing files (the `.c` files that call the PVR) use it; the headers do not.
- **SH4ZAM 0.10 or later** for the Dreamcast build. The effects use its square root, sine and cosine instructions (FSRRA and FSCA).
- **SH4DOW-DC**, in this library (`include/sh4dow_dc.h`). The effect headers include its header, so `include` must be on your include path.

Two ways in:

1. **Use the built library.** Run `make dc SH4ZAM=/path/to/sh4zam` with the KallistiOS environment set up. It writes `build/libsh4der_core.a`. Put `include` on your include path, with SH4ZAM's `include` folder too, then link `build/libsh4der_core.a`. The `make dc` rule uses SH4ZAM's include path only; link SH4ZAM in your own build, as SH4ZAM's own documentation describes.
2. **Compile the sources in your own makefile.** Take `src/sh4der_core.c` and the `.c` files of the effects you use from `src`, and compile them with the flags the Makefile uses for the Dreamcast build (its `DC_FLAGS`): `-O3 -Wall -Wextra -Iinclude -I<sh4zam>/include -ffast-math -ffp-contract=fast -mfsrra -mfsca -fno-math-errno`, with `kos-cc` as the compiler.

For a host tool, or a game without SH4ZAM, compile with `-DSH4DER_CORE_NO_SH4ZAM`. This runs the same steps as plain C, and it also sets `SH4DOW_DC_NO_SH4ZAM`. The PVR drawing code in the `.c` files sits inside `_arch_dreamcast` guards, so a host build compiles it to nothing.

Start with the blade effect: it has a complete example under [First effect](#first-effect) in its section below.

## Effects

| Effect | Header | What it does | Drawn | Version |
| --- | --- | --- | --- | --- |
| blade | `sh4der_blade.h` | Light tubes orbit an axis, a blocker runs through them, a pulse breathes over both | untextured, additive | v1.00 |
| holo | `sh4der_holo.h` | A light inside a translucent body follows the body's own breathing (the slow rise and fall of the chest) | alpha blended, texture × colour | v1.00 |
| bloom | `sh4der_bloom.h` | A soft glow round a blade: a feathered ribbon facing the camera and a feathered disc at the tip, 24 triangles | untextured, additive | v1.00 |
| look | `sh4der_look.h` | An eye (the brightness the frame has adapted to) that moves toward the frame both ways, a grade (a straight line per channel), glare in the fog | full-screen untextured layers (multiply, add, invert); the fog table (fog colour and density by depth) | v0.50 |
| frame | `sh4der_frame.h` | The frame on screen read back on a grid, a stamp on every frame so a read knows which frame it saw, the gain and glow taken off a read | the scan-out (the picture the video output sends to the screen); a 4 x 4 pixel block | v0.50 |
| taa | `sh4der_taa.h` | Temporal anti-aliasing maths: jitter, history weight, pan shift | the frame before as layers | v0.50 |
| glow | `sh4der_glow.h` | Screen bloom from a small bright map of the frame (darks die, brights stay), a wide halo, a darken layer balanced so a neutral scene keeps its mean, god rays as scaled copies about the sun | a 128 x 64 RGB565 map, additive quads | v0.50 |
| warp | `sh4der_warp.h` | The frame as a texture through a mesh: shockwave ring, heat haze column, underwater wobble; radial speed blur as scaled copies; pixelate and ordered dither | a screen grid mesh over the frame texture | v0.10 |
| trail | `sh4der_trail.h` | The frame before over this one through a copy of the front buffer, the picture already on screen (a column mask in destination alpha, two passes a copy): motion blur by speed, and the temporal AA blend | full-screen layers, a 128 B mask | v0.10 |
| lens | `sh4der_lens.h` | Overlays with no frame read: a vignette mesh, mean-neutral film grain, hit direction wedges and a full-screen flash; a grain and a vignette of the caller's own (tile size and kind, its own texels, a rectangle, an amplitude, neutral on or off, four movements; the vignette's shape, centre and curve) | vertex-colour meshes, a 64 x 64 grain tile (a caller's: 16 to 128 a side) | v0.10 |
| focus | `sh4der_focus.h` | Fake depth of field: a small blurred map of the frame drawn through a vertex-alpha mask that keeps a disc, band or rect sharp | a 128 x 64 map, one alpha mesh | v0.10 |
| visor | `sh4der_visor.h` | Visor looks as number tables (night, amber, thermal): a tint through the look grade, grain, scanlines, a luma ramp for thermal, a fade in and out | the look effect's layers, small tiles | v0.10 |
| torch | `sh4der_torch.h` | A flashlight hotspot carved on screen: a radial mesh darkens outside the circle and boosts a hot core, the fog table bends to black for reach, gated by the level's darkness with hysteresis | two radial vertex-colour meshes; the fog table | v0.50 |
| ao | `sh4der_ao.h` | Small-scale occlusion: crease darkening from the angle between faces for a bake, and a screen cavity cue from a small map | vertex colour at bake; a small multiply map | v0.10 |
| aobake | `sh4der_aobake.h` | Baked ambient occlusion against exact shapes (spheres, boxes, a ground plane): one byte a vertex from cosine rays, at load. The ao4 scenes' bake, 252 to 254 ms for scene A's 2,142 vertices and 391 to 393 ms for scene B's 2,544 on a console (SH4-076, SH4-082) | One byte a vertex, multiplied into the ambient light | v0.50 |
| aomesh | `sh4der_aomesh.h` | Baked ambient occlusion over a triangle mesh (a cell grid, 12 golden-ratio rays, a reach of 1.6 m, a byte grid over the ground), at load. The walled square's bake (`demo/psm/psm_ao.h` is an adapter over it). The bake's time is NOT MEASURED | One byte a vertex and a ground grid, multiplied into the ambient light | v0.10 |
| raytri | `sh4der_raytri.h` | A ray against a triangle, one kernel: the numerators, a distance (reciprocal form), a hit with no divide, a form prepared for one direction. Not watertight; the hit test has no far limit. Used by the aomesh bake and the ray bench. Host tested; time NOT MEASURED | Nothing | PROTOTYPE v0.1 |
| menu | `sh4der_menu.h` | A menu's decisions as plain C, nothing drawn: list with headings, tabs, scroll and bar, repeat, tap or hold, wrap, the place in one word. Used by the suite's menu (`demo/psm/suite/suite_view.h`) and `sh4der_suite.h`'s page loop. Host tested; console NOT RUN | Nothing | PROTOTYPE v0.1 |
| suite | `sh4der_suite.h` | The disc's start and return between programs (`arch_exec`, a 32-byte mail, the four buttons). Nothing drawn; the menu program is a demo (`demo/psm/suite`). | Nothing | ALPHA v0.4 |
| aosky | `sh4der_aosky.h` | Occlusion of the sky drawn into the sun's shadow target: K directions slid onto the ground as a faint black, one side, plain blending (AO method 2). The ao4 scenes ran it (PVR-138 to PVR-186); about 451 us of PVR a direction for scene A (DERIVED from PVR-140); the depth union failed on the PVR (PVR-127) | The sun's shadow target and its ground layer; no new target | v0.10 |
| sh4dow | `sh4dow_dc.h` | Soft self-shadow for skinned characters, per vertex on the CPU: a limb casts a soft column that darkens what lies in it; foot cones under each foot; a heightfield sun (experimental) | a multiply in place on the vertex colour; a quad per foot cone | - |
| ca | `sh4der_ca.h` | The frame texture (the world drawn into a 640 x 480 or 320 x 240 render target, then put on screen) and chromatic aberration through it: red, green and blue copies at slightly different scales about the centre, none in the middle, most in the corners | a stride texture (a texture the PVR reads row by row at a set stride), 640 KB or 160 KB; three small meshes | v0.50 |
| shafts | `sh4der_shafts.h` | Shadow shafts: the frame's dark shapes cast a darker copy pushed away from a light, only near the light and only as the camera turns to it | a 128 x 64 RGB565 map, one multiply mesh | v0.50 |
| water | `sh4der_water.h` | Directional waves, filtered ripple normals, Fresnel; crest transmission through sss; sunset client uses SH4DOW heightfield sun visibility | textured mesh + offset RGB; optional framebuffer bloom | v0.50 |
| sss | `sh4der_sss.h` | Back-lit transmission, a sub-surface scattering stand-in: light through a thin part toward the eye, tinted by absorption over a path-length proxy (wave crests, backlit skin and ears, leaves, wax, ice, cloth) | per-vertex RGB for the offset colour or vertex colour | v0.50 |
| guard | `sh4der_guard.h` | Two checks for what an emulator forgives and a real console does not: a triangle with no screen area, and a polygon header that was never compiled | nothing (inline tests) | v0.10 |
| bump | `sh4der_bump.h` | A normal map lit by a moving light, no bump unit: pack a normal map texel from a height map, decode it into RAM, light a vertex from it, send a polygon; the torch walk puts the highlight on with add passes of grey maps made from the normals (highlight power 2) | the normals in RAM (3 bytes a texel), nine 8-bit grey maps a surface in video memory (36 KB at 64 x 64), up to eight draws a lit cell | v0.14 |
| uvwarp | `sh4der_uvwarp.h` | A texture coordinate pushed per vertex by a small two-channel table (64 x 16 is 2 KB, from a source distortion map): weight x table(u + su t, v + sv t), bilinear, both axes wrapping, unsigned; a prepared form (`sh4der_uvwarp_prepare` once, `sh4der_uvwarp_offset_p` a vertex, or `sh4der_uvwarp_offset_pn` without the input gates for a loop that checked once) for a mesh loop. For what a material does per pixel and the PVR cannot (an arc, a banner, a water strip). | nothing (the mesh's own UV: a table and a weight a vertex) | v0.10 |
| spark | `sh4der_spark.h` | A fixed pool of 24 short-lived additive quads that face the camera, in two shapes: a billboard (a muzzle flash, an impact flash) and a streak (a tracer whose tail retreats to its hit, a spark that flies and falls), with a least width in pixels, a fade along the length, a burst helper and a pool rule (sparks die first, a flash or tracer is never lost to one). Sizes, colours, lives and atlas cells are the caller's table | one atlas texture (64 x 64 VQ: 3 KB), additive ONE / ONE on the translucent list, depth test on, write off | v0.10 |
| pl4nar | `sh4der_pl4nar.h` | Real-time planar reflection: the world drawn a second time, mirrored about a plane, into a small render target; a surface near the plane (car paint, glass, chrome) or in it (water, a wet road, a polished floor) looks the target up per vertex and blends it in, weighted by Fresnel. Materials are rows of the caller's table | a 256 x 128 RGB565 render target (64 KB); one translucent layer over the opaque surface, additive or blended, depth test on, write off | v0.10 |
| softfade | `sh4der_softfade.h` | How much of a light a point keeps near the planes and walls the caller names: a near test, a fit for a 4-vertex halo, and a per-vertex fade for a ring halo, a pool or a beam. Draws nothing | CPU only; no texture, no PVR pass | v0.10 |
| ramp | `sh4der_ramp.h` | A linear 0 .. 1 ramp between two distances, saturated at both ends: one call, or a prepared ramp for a loop (one reciprocal, then a multiply a point). A helper: draws nothing | CPU only; no tuning macros | v0.10 |
| viewcull | `sh4der_viewcull.h` | A box against the view's six planes (two FIPR a plane, no corner transforms), and a grid of points tested a block at a time so a caller skips the points and cells no block in view holds. A helper: draws nothing | CPU only; one tuning macro | PROTOTYPE v0.1 |
| fakeaf | `sh4der_fakeaf.h` | Fake anisotropic filtering: a mip bias per polygon by its slope to the eye (the one that has run on a console, PVR-180) and distance ranges with stretched copies and a ramp between them (helpers) | untextured mesh; the bias adds no pass | ALPHA v0.4 |
| dofmip | `sh4der_dofmip.h` | Focus blur from distance by texture mip bias: `sh4der_dofmip_prepare` a frame's focus (distance, sharp range, near and far strength, curve, reach), then `sh4der_dofmip_code` the PVR bias code of a polygon or object from its depth; batch by code; `sh4der_dofmip_combine` with fakeaf's floor bias | no pass, no video memory, textures need mip maps | PROTOTYPE v0.2 |
| psm route A | `sh4der_psm_a.h`, `src/sh4der_psm_a.c` | The route A pass as a library: the held fit over a square patch (`sh4der_psm_a_fit`), the accounting (4 casters, 2,200 triangles, a cap a caster), a caster's colour by strength, a world triangle into the PVR's triangles with the pass's origin in y, the receiver's map coordinates; and, Dreamcast only, the target, the pass (`_begin`, `_caster`, `_end`, `_settle`, `_release`) and the layer's header and vertex. Cuts the tile table through `sh4der_tilecut.h` (`SH4DER_PSM_A_CUT_PASS` with `sh4der_pvrx_wait_render_done_poll` + `sh4der_pvrx_scene_finish`, or `SH4DER_PSM_A_CUT_HOLD` holding the pass's own table) | one 128 x 128 RGB565 target, the casters' world positions | PROTOTYPE v0.2 (ra2 ran per-pass sleeping cut; polled and held cut NOT RUN on a console) |
| ground | `sh4der_ground.h` | The square's paving slabs: a 64 x 64 RGB565 tiling texture made at load from an integer hash (8 KB, not twiddled) | a texture on the ground mesh | PROTOTYPE v0.1 |
| pool | `sh4der_pool.h` | The light a lamp throws on the ground as a fan of triangles whose vertex alpha fades to nothing: a disc under a street lamp, a wedge ahead of a headlight, and the same disc, dark, as a blob shadow. For ground polygons too big to light per vertex | untextured vertex-alpha fans on the translucent list: additive for light, blended for shadow | v0.10 |
| psm | `sh4der_psm.h` | A world-to-map matrix for a directional light, fitted to the receivers inside the view frustum: uniform (orthographic) or warped through the camera's post-projective space (one point L' for the parallel rays, either sign of w), with a virtual near plane for the warp, the depth key (distance along the light), a world-space bias, texel and pixel footprints for the A/B, the frustum corners and a slab clip. The map is the client's: a depth compare in software, a silhouette on the PVR (no depth compare there) | none; a map the client draws and reads | v0.10 |
| fault | `sh4der_fault.h` | A report on the screen when the program raises an exception, hangs, asserts or exits: registers, the stack's calls, the frame's breadcrumbs. No console run has shown it catching a fault | Not drawn with the PVR (minifont over the frame buffer) | v0.10 |
| split | `sh4der_split.h` | Divider of a left and a right half in 32-pixel tile columns, moved by held triggers; gives each half its clip rectangle (a hidden half has none). The middle position works on a console (PVR-144, PVR-161); a moving divider has not run | User clips, one per half | v0.20 |
| tilecut | `sh4der_tilecut.h` | A render target pass through a tile table its own size: KallistiOS renders every target through the screen's 20 x 15 tile table, and this ends the table after the target's own tiles for one pass, then puts every word back. The PVR's time for a 20-tile target fell 3,312 -> 375 us (PVR-092), the frame 1,869 us in d1 (PVR-205) | KallistiOS's tile tables in video memory, rewritten between passes; no render may read them meanwhile | v0.50 |
| pvrx | `sh4der_pvrx.h` | A layer over KallistiOS's PVR driver: polled render-done waits, a PVR-DMA download of video memory, counters for TA error events, the Holly revision read, a user clip change with its dummy polygon | nothing of its own (the driver's lists); the `.c` is Dreamcast only | v0.05 |
| page | `sh4der_page.h` | Result pages for the console tests: text (64 x 22 characters, an 8 x 16 font) with a CRC line, and graphs (lines, bars, a legend) in the title-safe area, drawn after the PVR is shut down | the frame buffer, no PVR state | v0.50 |
| fxlabel | `sh4der_fxlabel.h` | The effect label line on a result page and a live view: each effect by its field name, who does the work (SH-4, PVR, SH-4/PVR) and when (Baked, Texture, Realtime) | text only (the program draws it) | v0.50 |
| qrpage | `sh4der_qrpage.h` | The QR test set: 16 codes of known payloads at 10, 8, 6 and 4 pixels a module, each on a light panel, for a photograph test of the page reader | the frame buffer, no PVR state | v0.50 |

Versions: v1.00 is used in a released game. v0.50 runs on screen on the demo disc. v0.10 passes its maths tests. `sh4der_core.h`, the shared parts, is v1.00.

All of them include `sh4der_core.h`, the shared parts:
- **The feather.** SH4DOW-DC's column feather (a soft edge: full inside, fading to nothing outside), with the core answered before any square root.
- **The shapes.** A tube (a soft column of light orbiting the axis) and a blocker (a soft ball on a helix along it).
- **The breath tracker.** Reads a pose's own breathing (how far its chest moves in an idle loop) and turns it into 0 (out) .. 1 (in).
- **The glint.** A soft flare quad at a light, with brightness set by camera depth.
- **The colour ramp.** EDGE and CORE from a texture's average colour; a vertex of level k takes mix(EDGE, CORE, k²) × k, so dim parts read saturated and peaks white-hot.
- **The SH4ZAM steps.** Distances, clamps, sines, cosines and roots, with a plain C mirror for host tools.

## The blade effect

It was made for a glowing blade on a model: the two blade parts (two additive material groups of one model) get the effect, coloured with their texture's average colour. A blade node has no animated bone of its own, so the effect runs in the blade node's space: x along the blade.

- **Axis.** The line x, at (y, z) = `axis`. Pass vertices in that space.
- **Tube.**
  - Its line is parallel to the axis, at radius `rho` and angle 2π(`turns`·u + `phase`), where u = (tick mod `period`) / `period`.
  - Light from it is 1 within `inner` of its line, then falls smoothly to 0 at `outer` (the feather). That is scaled by `weight`.
  - The brightest tube wins, as SH4DOW-DC's darkest column wins.
- **Blocker.**
  - A ball whose centre goes from `x0` to `x1` along the axis `passes` times a period, while turning `turns` times on a helix of radius `rho`.
  - It takes `depth` of the light at its core and fades out to `outer`. Blockers multiply.
- **Pulse and glow.**
  - pulse = ½(1 − cos 2π·`pulses`·u)
  - glow = `glow_base` + `glow_amp`·pulse
- **Intensity.**
  - intensity = min(1, glow·(`floor` + (1 − `floor`)·tube)·mask)·fade
  - `floor` is what a vertex gets with no tube near.
  - fade is your 0..1, for example 1 − a cloak level.
  - Ripples along the axis (off by default): with `ripple_depth` > 0 the intensity is multiplied by 1 − `ripple_depth`·½(1 − cos(2π·`ripple_freq`·x − 2π·frac(`pulses`·u))): `ripple_freq` cycles a unit along the axis, travelling one ripple a pulse.
- **Point light.** `frame->light` = (1 − `light_swing`·(1 − pulse))·fade. Use it for a point light that breathes with the glow, or set `light_swing` to 0 for a constant light.

`turns`, `passes` and `pulses` are whole numbers, so the period closes.

### First effect

```c
/* SH4DER-PVR: the blade effect

   example/blade.c
   Copyright (c) 2026 Jilted Valkyrie

   One glowing blade, one frame: the default tuning at the top of a pulse,
   three vertices and their packed colours.
*/

#include <stdio.h>
#include "sh4der_blade.h"

int main(void) {
    /* blade vertices in the blade's own space: x along the blade */
    static const float blade[3][3] = {{0.20f, 0.0f, 0.06f}, {0.20f, 0.0f, -0.05f}, {0.33f, 0.0f, 0.01f}};
    static const float cyan[3] = {0.35f, 0.82f, 1.0f};   /* the blade texture's average, brightest channel 1 */
    sh4der_blade_params_t params;
    sh4der_blade_frame_t frame;
    uint32_t argb[3];
    unsigned i;

    sh4der_blade_params_default(&params);
    /* tick 48 + this instance's phase (0 here); fade = 1 minus a cloak level, 1 for none */
    if (!sh4der_blade_prepare(&frame, &params, 48, 1.0f))
        return 1;
    sh4der_blade_argb(&frame, cyan, 0xff, blade[0], sizeof blade[0], argb, sizeof argb[0], 3);
    for (i = 0; i < 3; i++)
        printf("vertex %u: intensity %.4f argb %08lx\n", i, sh4der_blade_intensity(&frame, blade[i][0], blade[i][1], blade[i][2]),
               (unsigned long)argb[i]);
    printf("point light %.4f\n", frame.light);
    return 0;
}
```

It prints intensity 1.0000, 0.6099 and 0.4000, then point light 1.0000. At tick 48 tube A lies on the top prong.

### Calls

```c
void sh4der_blade_params_default(sh4der_blade_params_t *params);
int sh4der_blade_params_valid(const sh4der_blade_params_t *params);
int sh4der_blade_prepare(sh4der_blade_frame_t *frame, const sh4der_blade_params_t *params, uint32_t tick, float fade);
void sh4der_blade_shade(const sh4der_blade_frame_t *frame, const float *xyz, size_t xyz_stride, float *out, unsigned count);
void sh4der_blade_argb(const sh4der_blade_frame_t *frame, const float rgb[3], uint8_t a, const float *xyz, size_t xyz_stride,
                       uint32_t *out, size_t out_stride, unsigned count);
void sh4der_blade_argb_ramp(const sh4der_blade_frame_t *frame, const float edge[3], const float core[3], uint8_t a, const float *xyz,
                            size_t xyz_stride, uint32_t *out, size_t out_stride, unsigned count);
void sh4der_blade_params_white(sh4der_blade_params_t *params);
static inline float sh4der_blade_intensity(const sh4der_blade_frame_t *frame, float x, float y, float z);
```

`sh4der_blade_argb_ramp` colours each vertex on the ramp (see the shared parts), `sh4der_blade_argb` with one colour.

1. Call `sh4der_blade_prepare` once per instance per frame, with the tick plus the instance's phase. Use 57 × the object's index as the phase, so a squad's blades do not pulse together. On bad params it returns 0 and leaves a dark frame: every intensity 0, light 0.
2. Then call `sh4der_blade_intensity` per vertex, or `sh4der_blade_shade` / `sh4der_blade_argb` over a run. Strides are byte steps.
3. Draw the group's triangles additively with that colour. The library does not touch your texture or your draw.

Which parts get the effect is your choice, made once at load: for example a mask of the model's shader indices whose shader names you listed.

### Blade tuning

The default blade values, from `sh4der_blade_params_default`. Lengths are in vertex units (the source model's world units on the blade node). The period is in ticks (30 a second).

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_BLADE_PERIOD | 192 | Ticks a period: one turn of tube A, 6.4 s. The source model's glow turns at 1 radian a second, 188.5 ticks a turn |
| SH4DER_BLADE_PULSES | 2 | Glow pulses a period. They peak when tube A crosses a prong |
| SH4DER_BLADE_GLOW_BASE | 0.55 | Glow at the bottom of a pulse |
| SH4DER_BLADE_GLOW_AMP | 0.45 | Glow added at the top |
| SH4DER_BLADE_MIN_GLOW | 0.25 | Lowest glow a pulse reaches (the blade never goes fully transparent) |
| SH4DER_BLADE_FLOOR | 0.40 | Share of the glow with no tube near |
| SH4DER_BLADE_LIGHT_SWING | 0.15 | Point light dip at the bottom of a pulse (0 = constant) |
| SH4DER_BLADE_AXIS_Z | 0.008 | Axis height |
| SH4DER_BLADE_A_RHO | 0.055 | Tube A orbit radius (1 turn a period) |
| SH4DER_BLADE_A_INNER | 0.012 | Tube A core |
| SH4DER_BLADE_A_OUTER | 0.035 | Tube A edge |
| SH4DER_BLADE_B_RHO | 0.040 | Tube B orbit radius (2 turns back a period) |
| SH4DER_BLADE_B_INNER | 0.010 | Tube B core |
| SH4DER_BLADE_B_OUTER | 0.028 | Tube B edge |
| SH4DER_BLADE_B_WEIGHT | 0.6 | Tube B brightness |
| SH4DER_BLADE_B_PHASE | 0.25 | Tube B start, in turns |
| SH4DER_BLADE_K_X0 | -0.10 | Blocker pass start (behind the blade's base) |
| SH4DER_BLADE_K_X1 | 0.40 | Blocker pass end (past the tips) |
| SH4DER_BLADE_K_RHO | 0.030 | Blocker helix radius |
| SH4DER_BLADE_K_INNER | 0.020 | Blocker core |
| SH4DER_BLADE_K_OUTER | 0.055 | Blocker edge |
| SH4DER_BLADE_K_DEPTH | 0.85 | Light the blocker core takes |
| SH4DER_BLADE_K_PASSES | 2 | Blocker passes a period (and helix turns) |
| SH4DER_BLADE_FADE_X0 | -0.065 | The blade's base along the axis: intensity 0 there, so the cut of its shape shows no edge |
| SH4DER_BLADE_FADE_X1 | 0.0 | Full intensity from here (smoothstep in between) |

`sh4der_blade_params_white` is a test preset: the same tubes, blocker and pulse run white-hot, never a flat silhouette. Draw it with a white ramp.

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_BLADE_WHITE_GLOW_BASE | 0.80 | Glow at the bottom of a pulse |
| SH4DER_BLADE_WHITE_GLOW_AMP | 0.20 | Glow added at the top |
| SH4DER_BLADE_WHITE_FLOOR | 0.75 | Share of the glow with no tube near |
| SH4DER_BLADE_WHITE_K_DEPTH | -0.6 | The blocker brightens: its knot is the brightest part (a blocker's depth runs -1..1) |

**Cost.** A frame is 156 bytes and can live on the stack; params are 160 bytes, one table per effect (both sizes on the SH-4). Per vertex: two 2D distance tests, one 3D distance test, and a feather only where a vertex is inside a tube or the blocker. The draw is untextured, which costs the PVR less than the textured draw it replaces.

## The holo effect

A light inside a translucent body (a hologram's chest) that follows the body's own breathing. There is no clock and no flicker term, and it moves no vertex; the alpha it writes is constant, so the body stays filled.

- **Breath, once per frame.** The animation already breathes: in the idle loop the chest moves forward of the pelvis and back. `sh4der_breath_raw(chest, pelvis_origin, pelvis_forward)` reads that from the pose you skin with. `sh4der_breath_push` remembers the last 90 frames and returns 0 (breath out) .. 1 (breath in): (raw − middle) / span + ½, span = their range, at least 0.0013, clamped.
- **Weight, once at load.** `sh4der_holo_weights` gives each vertex a byte 0..63 from its bind position: the feather around `center`. The core rides the body, so the bind position is enough.
- **Table, once per frame.** `sh4der_holo_prepare(frame, params, breath, fade)` fills 64 colours:
  - level(w) = min(1, (`glow_base` + `glow_amp`·breath)·`body` + (`core_base` + `core_amp`·breath)·`add`·w / 63)
  - grey in r, g and b; the fade (0..1) in alpha.
- **Per vertex.** `sh4der_holo_argb(frame, weight)`: one table load.
- **Draw.** Alpha blended, the body's texture modulated by that colour. Bake the texture 1 / `body` brighter so the body away from the core shows it as painted.

The core never goes out: at breath out it is still 0.60 of its strength. So the vertex level is 0.558 (out) .. 0.620 (in) away from the core and 0.798 .. 1.000 at its centre.

**The glint.** A lens flare on the same light: a soft additive quad at the chest marker, half size 0.10 x 0.15 (radius 0.025 x scale 4, 6), brightness 0.4 within 3 units of the camera, fading to nothing at 6.5 (`sh4der_glint(depth, fade)`). Draw it 0.0075 toward the viewer, depth tested, no depth write: the world in front of it hides it per pixel. The texture and tint are your data.

### Calls

```c
void sh4der_holo_params_default(sh4der_holo_params_t *params);
int sh4der_holo_params_valid(const sh4der_holo_params_t *params);
void sh4der_holo_weights(const sh4der_holo_params_t *params, const float *xyz, size_t xyz_stride, uint8_t *out, unsigned count);
int sh4der_holo_prepare(sh4der_holo_frame_t *frame, const sh4der_holo_params_t *params, float breath, float fade);
void sh4der_holo_shade(const sh4der_holo_frame_t *frame, const uint8_t *weight, uint32_t *out, size_t out_stride, unsigned count);
static inline uint32_t sh4der_holo_argb(const sh4der_holo_frame_t *frame, uint8_t weight);
float sh4der_breath_raw(const float chest[3], const float pelvis_origin[3], const float pelvis_forward[3]);
void sh4der_breath_reset(sh4der_breath_t *breath);
float sh4der_breath_push(sh4der_breath_t *breath, float raw);
float sh4der_glint(float depth, float fade);
```

On bad params `sh4der_holo_prepare` returns 0 and leaves a clear table (every entry 0), and `sh4der_holo_weights` gives 0 everywhere. A NaN breath is the middle (0.5); a NaN raw is not remembered.

### Holo tuning

The default values, from `sh4der_holo_params_default`. Lengths are in vertex units (the bind model's world units).

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_HOLO_LEVELS | 64 | Weight steps (table entries) |
| SH4DER_HOLO_GLOW_BASE | 0.90 | Body factor at breath out |
| SH4DER_HOLO_GLOW_AMP | 0.10 | Body factor added at breath in (0.90..1.00) |
| SH4DER_HOLO_BASE | 0.60 | Core strength at breath out |
| SH4DER_HOLO_AMP | 0.40 | Core strength added at breath in (0.60..1.00) |
| SH4DER_HOLO_BODY | 0.62 | Vertex level away from the core |
| SH4DER_HOLO_ADD | 0.40 | Level the core adds at its centre |
| SH4DER_HOLO_X | -0.00642 | Core centre x (a chest marker, bind model space) |
| SH4DER_HOLO_Y | 0.0 | Core centre y |
| SH4DER_HOLO_Z | 0.13477 | Core centre z |
| SH4DER_HOLO_INNER | 0.010 | Full core within this distance |
| SH4DER_HOLO_OUTER | 0.055 | No core from this distance |

**Cost.** No allocation, no globals. A frame is 272 bytes (the table is 256 of them), params 44 bytes. The caller keeps one byte a vertex for the weights. A frame: the table (64 multiply-adds). A vertex: one load. The weights cost one distance test a vertex, once, at load.

## The bloom effect

The PVR has no post-process blur. A few untextured polygons added over the frame are its bloom, and their soft edge is SH4DOW-DC's feather, sampled at the vertices and spread by Gouraud shading.

- **The ribbon.** One strip along the blade, facing the camera about the blade's axis (its width runs along x × (eye − axis)).
  - 3 rows: the root, the blade tip, and a tail past the tip at level 0, so the ribbon ends inside the disc and not on an edge.
  - 5 columns: edge, mid, centre, mid, edge, with the feather across (0, 0.62, 1, 0.62, 0 for the default width).
  - A vertex's level is feather × fade × (glow × along × (`tube_floor` + (1 − `tube_floor`) × tube) + `boost` × knot). Here along is `root`, `tip`, then 0; tube is the blade's own tubes at that point, so the swirl brightens that side; and knot is the blocker's feather along the axis, scaled by the row's along level, so the bloom brightens where it passes (the opposite of its mask on the blade) and the root and the tail stay at 0.
- **The disc.** A feathered disc facing the camera: the centre at `peak` and 8 rim vertices at 0, none on the blade's axis (eight sides read round, no flat across the tip). At the tip it is a lens flare: its peak is multiplied by `flare_floor` + (1 − `flare_floor`) × smoothstep((cos − `flare_cos`) / (1 − `flare_cos`)), cos being the blade's direction against the way to the eye, so it is full when the tip points at the camera and subtle side-on. `sh4der_bloom_disc` is general (a centre, a radius, a peak) for any glow point; `sh4der_bloom_disc_n` takes a rim count (16 is round), and `sh4der_bloom_disc_feather` adds a soft-edge share: an inner ring at the peak out to (1 − feather) × radius, then a band down to 0 at the rim (1: soft from the centre, 0: a hard edge), 3 × rim triangles.
- **The colour.** The same ramp as the blade: mix(EDGE, CORE, min(level, 1)²) × level, each channel clamped.
- **Draw.** Draw the 24 triangles of `sh4der_bloom_tris` with the blade: added (src alpha / one), depth tested, no depth write, untextured.

### Calls

```c
void sh4der_bloom_params_default(sh4der_bloom_params_t *params);
void sh4der_bloom_params_max(sh4der_bloom_params_t *params);
int sh4der_bloom_params_valid(const sh4der_bloom_params_t *params);
void sh4der_bloom_disc(float xyz[][3], float level[], const float centre[3], float radius, const float eye[3], float peak);
void sh4der_bloom_disc_n(float xyz[][3], float level[], const float centre[3], float radius, const float eye[3], float peak, unsigned rim);
void sh4der_bloom_disc_feather(float xyz[][3], float level[], const float centre[3], float radius, float feather, const float eye[3], float peak,
                               unsigned rim);
unsigned sh4der_bloom_disc_feather_tris(unsigned rim, uint8_t tris[][3]);
void sh4der_bloom_rings(float xyz[][3], float level[], const float centre[3], const float eye[3], float peak, const float radius[],
                        const float ring_level[], unsigned rings, unsigned seg);
unsigned sh4der_bloom_rings_tris(unsigned rings, unsigned seg, uint8_t tris[][3]);
int sh4der_bloom_blade(sh4der_bloom_mesh_t *mesh, const sh4der_bloom_params_t *params, const sh4der_blade_params_t *blade,
                       const sh4der_blade_frame_t *frame, const float eye[3]);
void sh4der_bloom_argb(const sh4der_bloom_mesh_t *mesh, const float edge[3], const float core[3], uint8_t a, uint32_t *out, size_t out_stride);
```

`eye` is the camera in the blade's space. On bad params `sh4der_bloom_blade` returns 0 and leaves every level 0.

### Bloom tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_BLOOM_ROWS | 3 | Ribbon rows: root, blade tip, tail |
| SH4DER_BLOOM_COLS | 5 | Ribbon columns: edge, mid, centre, mid, edge |
| SH4DER_BLOOM_RIM | 8 | Tip disc rim vertices (8: it reads round) |
| SH4DER_BLOOM_DISC_MAX | 24 | Rim vertices `sh4der_bloom_disc_n` takes at most (a round glow: 16) |
| SH4DER_BLOOM_VERTS | 24 | Vertices a bloom |
| SH4DER_BLOOM_TRIS | 24 | Triangles a bloom (16 ribbon, 8 disc) |
| SH4DER_BLOOM_X0 | -0.065 | Ribbon root along the axis: the blade's base |
| SH4DER_BLOOM_X1 | 0.35 | The blade tip along the axis (the disc's centre) |
| SH4DER_BLOOM_TAIL | 0.10 | Ribbon tail past the tip, at level 0 |
| SH4DER_BLOOM_HALF_WIDTH | 0.0875 | Ribbon half width (2.5 × tube A's edge) |
| SH4DER_BLOOM_INNER | 0.012 | Full level within this distance of the axis |
| SH4DER_BLOOM_ROOT | 0.0 | Ribbon level at the root: 0, so the ribbon fades in from the blade's base with no edge |
| SH4DER_BLOOM_TIP | 0.90 | Ribbon level at the blade tip: the ribbon brightens toward it |
| SH4DER_BLOOM_TUBE_FLOOR | 0.5 | Share of the ribbon with no tube near |
| SH4DER_BLOOM_BOOST | 0.4 | Level the blocker's knot adds |
| SH4DER_BLOOM_BOOST_REACH | 0.10 | The knot's reach along the axis |
| SH4DER_BLOOM_DISC_RADIUS | 0.14 | Tip disc radius |
| SH4DER_BLOOM_DISC_PEAK | 0.90 | Tip disc level at its centre, the tip pointing at the eye |
| SH4DER_BLOOM_FLARE_FLOOR | 0.15 | Tip disc share side-on: a lens flare |
| SH4DER_BLOOM_FLARE_COS | 0.5 | The flare rises once the tip is within 60 degrees of the eye |

`sh4der_bloom_params_max` is the MAX WHITE test, the default with these:

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_BLOOM_MAX_TAIL | 0.14 | Tail |
| SH4DER_BLOOM_MAX_HALF_WIDTH | 0.11 | Half width |
| SH4DER_BLOOM_MAX_INNER | 0.015 | Full-level core |
| SH4DER_BLOOM_MAX_TIP | 0.80 | Level at the blade tip |
| SH4DER_BLOOM_MAX_BOOST | 0.8 | Knot level |
| SH4DER_BLOOM_MAX_BOOST_REACH | 0.12 | Knot reach |
| SH4DER_BLOOM_MAX_DISC_RADIUS | 0.24 | A huge tip disc |
| SH4DER_BLOOM_MAX_DISC_PEAK | 1.8 | Its centre level (clamped white) |
| SH4DER_BLOOM_MAX_FLARE_FLOOR | 0.5 | Its side-on share |
| SH4DER_BLOOM_RINGS_MAX | 8 | Rings sh4der_bloom_rings() takes at most |

**Cost.** 24 vertices and 24 triangles a blade (the tip disc has 8 rim vertices: it reads round). For each, one tube evaluation per ribbon vertex and one normalise for the disc. No texture, no video memory.

## The look effect

The PVR has no post-process shader. It has three things the look is made of: the CPU can read a few pixels of the frame on screen, a full-screen untextured quad can multiply, add or invert the frame per channel, and the fog colour and table are free per pixel. `sh4der_look.h` is the maths; the client reads the pixels, draws the layers and writes the fog. Header only (inline): nothing is linked until a client calls it.

- **The read.** Sample a `SH4DER_LOOK_GRID_X` x `SH4DER_LOOK_GRID_Y` grid of the frame on screen (RGB565), count each sample's luma bin (`sh4der_look_luma565`, Rec. 601 weights) and take `sh4der_look_log_average`: the dark weighs as the eye weighs it.
- **The eye.** `sh4der_look_eye_step` eases an adapted level toward the scene, to 95 % in `light_s` when it gets brighter and `dark_s` when it gets darker, and gives the gain: `transient` x (scene - eye) shows as over-bright or dark until the eye catches up, `open` x (middle grey - eye) stays once adapted (a dark place opens up a little), clamped to `gain_lo` .. `gain_hi`. The frame read already carries the gain given last frame; the step divides it out.
- **The grade.** Per channel out = clamp(A x + B), A = gain x exposure x contrast, B = lift + (1 - contrast) / 2. `sh4der_look_solve` gives at most `SH4DER_LOOK_LAYERS` layers in drawing order: the part of B below 0 first (invert, add -B / A, invert: a boost cannot clip before it), then multiply, boost (x (1 + c), up to 2), add. A layer that changes no channel by 1/255 is left out; a neutral grade is no layer.
- **The glare.** `sh4der_look_glare` leans the fog colour to white and thickens the fog where it already is, by an amount 0..1 (a bright light in view). Mix it over the client's own fog, never in place of it.
- **The steps.** First order, dt / (tau + dt). The log and exp are short series on both paths: SH4ZAM's fast forms read log(1) = 0.040 and exp(-0.012) = 0.963, a grey cast and a drift here.

### Calls

```c
unsigned sh4der_look_luma565(unsigned p);
void sh4der_look_bins(sh4der_look_bins_t *t);
float sh4der_look_log_average(const sh4der_look_bins_t *t, const uint16_t hist[SH4DER_LOOK_BINS], unsigned n);
void sh4der_look_params_default(sh4der_look_params_t *p);
void sh4der_look_eye_reset(sh4der_look_eye_t *e);
float sh4der_look_eye_step(sh4der_look_eye_t *e, const sh4der_look_params_t *p, float read_ln, float dt);
int sh4der_look_solve(const sh4der_look_grade_t *gr, sh4der_look_layer_t out[SH4DER_LOOK_LAYERS]);
uint32_t sh4der_look_argb(const sh4der_look_layer_t *l);
void sh4der_look_glare(float t, float rgb[3], float *table, unsigned n);
```

Draw the layers untextured over the whole frame, under the HUD (the on-screen display), depth always, no depth write: MULTIPLY src ZERO and the source colour in the destination slot, BOOST src destination colour and dst ONE, ADD ONE / ONE, INVERT white with src inverse destination colour and dst ZERO.

### Look tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_LOOK_GRID_X | 80 | Samples across the frame |
| SH4DER_LOOK_GRID_Y | 60 | Samples down the frame |
| SH4DER_LOOK_BINS | 64 | Luma bins of the read |
| SH4DER_LOOK_FLOOR | 0.004 | Luma added before the log: black is not minus infinity |
| SH4DER_LOOK_REF | 0.18 | Middle grey OPEN pulls an adapted eye toward |
| SH4DER_LOOK_TRANSIENT | 0.5 | Share of (scene - eye) that shows as over-bright or dark |
| SH4DER_LOOK_OPEN | 0.3 | Share of (middle grey - eye) kept once adapted |
| SH4DER_LOOK_GAIN_LO | 0.75 | Lowest gain laid over the frame |
| SH4DER_LOOK_GAIN_HI | 1.5 | Highest gain (RGB565 bands past it) |
| SH4DER_LOOK_LIGHT_S | 2.0 | Seconds to 95 % when the scene gets brighter |
| SH4DER_LOOK_DARK_S | 6.0 | Seconds to 95 % when it gets darker |
| SH4DER_LOOK_GLARE_TINT | 0.6 | Fog colour's lean to white at full glare |
| SH4DER_LOOK_GLARE_FOG | 1.5 | Fog thickening at full glare (x (1 + this)) |
| SH4DER_LOOK_LAYERS | 6 | Layers `sh4der_look_solve` gives at most |

**Cost.** The read: 4,800 uncached 16-bit loads and a 64-bin count a frame. The eye: one log and one exp series a frame. The grade: 0 to 6 full-screen quads (4 vertices each), no texture, no video memory.

## The temporal AA maths

`sh4der_taa.h`: a sub-pixel jitter of the projection each frame (Halton 2, 3, `SH4DER_TAA_PHASES` phases, centred) and the frame before laid over this one at a weight. No motion vectors on the PVR: the weight falls with the camera's speed in pixels, a pan shifts the old frame whole, a turn past `SH4DER_TAA_CUT_RAD` in one frame is a cut (no history). Header only.

### TAA tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_TAA_PHASES | 8 | Jitter phases in a cycle (Halton 2, 3) |
| SH4DER_TAA_WEIGHT_HI | 0.75 | History weight while the camera is still |
| SH4DER_TAA_WEIGHT_LO | 0.0 | History weight at SPEED_HI and faster |
| SH4DER_TAA_SPEED_LO | 1.0 | Pixels a frame up to which the weight is full |
| SH4DER_TAA_SPEED_HI | 6.0 | Pixels a frame from which the weight is LO |
| SH4DER_TAA_CUT_RAD | 0.35 | A turn of this many radians in one frame is a cut |
| SH4DER_TAA_JITTER_AMP | 1.0 | Scale on the jitter |

## The glow effect

`sh4der_glow.h`, `src/sh4der_glow.c` (the KOS PVR drawing, Dreamcast only): screen bloom from a small bright map of the frame (darks die, brights stay), a wide halo, a darken layer balanced so a neutral scene keeps its mean, god rays as scaled copies about the sun.

### Glow tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_GLOW_GRID_X | 80 | Bright-map cells across the frame |
| SH4DER_GLOW_GRID_Y | 60 | Bright-map cells down the frame |
| SH4DER_GLOW_TEX_W | 128 | Power-of-two texture side holding the map |
| SH4DER_GLOW_TEX_H | 64 | Power-of-two texture side holding the map |
| SH4DER_GLOW_SMALL_X | 20 | Shrink of the map across (cells of 4 x 4) |
| SH4DER_GLOW_SMALL_Y | 15 | Shrink of the map down |
| SH4DER_GLOW_SMALL_TEX_W | 32 | Texture side of the shrink |
| SH4DER_GLOW_SMALL_TEX_H | 16 | Texture side of the shrink |
| SH4DER_GLOW_THRESHOLD | 0.62 | Luma (0..1) where the glow starts |
| SH4DER_GLOW_POWER | 2 | k in ((luma - threshold) / (1 - threshold))^k. Whole, 1..3 |
| SH4DER_GLOW_STRENGTH | 0.45 | Glow at full bloom (one draw: 0..1) |
| SH4DER_GLOW_RADIUS | 0.5 | Share of the strength on the wide 20 x 15 halo |
| SH4DER_GLOW_DARKEN_MAX | 0.25 | Most the darken layer takes off (D cap) |
| SH4DER_GLOW_RAYS | 4 | Ray copies (3..6) |
| SH4DER_GLOW_RAY_STEP | 0.12 | Scale step per copy about the sun (copy i at 1 - i x step) |
| SH4DER_GLOW_RAY_WEIGHT | 0.35 | First copy's weight |
| SH4DER_GLOW_RAY_DECAY | 0.6 | Weight ratio from one copy to the next |
| SH4DER_GLOW_ASPECT | 1.3333333 | Screen width over height: distances are in screen heights |
| SH4DER_GLOW_RAY_REACH | 0.8 | Distance from the sun (screen heights) where a ray is gone |
| SH4DER_GLOW_MESH_X | 8 | Cells across a ray or shaft copy (the fade is per vertex) |
| SH4DER_GLOW_MESH_Y | 6 | Cells down a ray or shaft copy |
| SH4DER_GLOW_SUN_EDGE | 0.25 | Sun this far off screen (screen heights): no rays |

## The warp effect

`sh4der_warp.h`, `src/sh4der_warp.c` (the KOS PVR drawing, Dreamcast only): the frame as a texture through a mesh: shockwave ring, heat haze column, underwater wobble; radial speed blur as scaled copies; pixelate and ordered dither.

### Warp tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_WARP_FRAME_W | 320 | The frame's width, pixels (the plane) |
| SH4DER_WARP_FRAME_H | 240 | The frame's height, pixels |
| SH4DER_WARP_TEX_W | 512 | The texture's width (the plane's target) |
| SH4DER_WARP_TEX_H | 256 | The texture's height |
| SH4DER_WARP_SOURCES | 4 | Warp sources a frame holds |
| SH4DER_WARP_GRID_X | 20 | Coarse grid: cells across (16 pixels) |
| SH4DER_WARP_GRID_Y | 15 | Coarse grid: cells down |
| SH4DER_WARP_FINE_X | 40 | Fine grid: cells across (8 pixels) |
| SH4DER_WARP_FINE_Y | 30 | Fine grid: cells down |
| SH4DER_WARP_RING_SPEED | 180.0 | Shockwave ring, frame pixels a second |
| SH4DER_WARP_RING_WIDTH | 14.0 | Ring band: pixels from its middle to no push |
| SH4DER_WARP_RING_AMP | 5.0 | Ring push at full strength, pixels |
| SH4DER_WARP_HAZE_AMP | 1.5 | Haze column's sideways wave, pixels |
| SH4DER_WARP_HAZE_PERIOD | 24.0 | Haze wave length, pixels |
| SH4DER_WARP_HAZE_RISE | 40.0 | Haze wave climb, pixels a second |
| SH4DER_WARP_HAZE_EDGE | 16.0 | Haze column's soft edge, pixels |
| SH4DER_WARP_WOBBLE_AMP | 1.2 | Full-frame wobble, pixels |
| SH4DER_WARP_WOBBLE_PERIOD | 96.0 | Wobble wave length, pixels |
| SH4DER_WARP_WOBBLE_TURN | 4.0 | Seconds for the wobble to move one wave length on |
| SH4DER_WARP_BLUR_COPIES | 4 | Radial blur copies (at most) |
| SH4DER_WARP_BLUR_ZOOM | 0.06 | Outermost copy's zoom at full strength (1 + this) |
| SH4DER_WARP_PIXEL_CELL | 16 | Pixelate block, frame pixels (divides 320 and 240) |
| SH4DER_WARP_DITHER | 0.25 | Dither pattern: the darkest share at full |

## The trail effect

`sh4der_trail.h`, `src/sh4der_trail.c` (the KOS PVR drawing, Dreamcast only): the frame before over this one through a copy of the front buffer, the picture already on screen (a column mask in destination alpha, two passes a copy): motion blur by speed, and the temporal AA blend.

### Trail tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_TRAIL_COPIES | 3 | History copies along the motion, 2 or 3 (copy k at k / (COPIES - 1) of the motion) |
| SH4DER_TRAIL_WEIGHT_LO | 0.0 | History weight while still (no blur) |
| SH4DER_TRAIL_WEIGHT_HI | 0.5 | History weight at SPEED_HI and faster: the cap (the HUD's last frame ghosts at most this) |
| SH4DER_TRAIL_SPEED_LO | 1.0 | Content speed (pixels a frame) below which there is no blur |
| SH4DER_TRAIL_SPEED_HI | 8.0 | Content speed (pixels a frame) from which the weight is full |
| SH4DER_TRAIL_SHIFT_MAX | 8 | Longest copy offset, pixels (the streak's length cap) |
| SH4DER_TRAIL_CUT_RAD | 0.35 | A turn of this many radians in one frame is a cut: no history |
| SH4DER_TRAIL_MASK_BYTES | 128 | The mask's size in VRAM |

## The lens effect

`sh4der_lens.h`, `src/sh4der_lens.c` (the KOS PVR drawing, Dreamcast only): overlays with no frame read: a vignette mesh, mean-neutral film grain, hit direction wedges and a full-screen flash.

The first forms are four calls: `sh4der_lens_init()` (the 64 x 64 tile, 8 KB of video memory, and the headers), `sh4der_lens_vignette_setup(w, h)`, then a frame `sh4der_lens_vignette_draw()` and `sh4der_lens_grain(frame, w, h)`. They draw the whole screen at the default tuning. `test/lens_test.c` pins their bytes (the tile, the default vignette mesh at two screen sizes, the frame offsets, the boost, the grain quads' colours): the variations below left them as they were.

**Variations: a grain of the caller's own** (`sh4der_lens_grain_t`, 64 B; it does not need `sh4der_lens_init`):

- **Whose tile.** `sh4der_lens_grain_make(&g, tile, px, seed, kind)`: the library fills, uploads and compiles. `sh4der_lens_grain_load(&g, texels, tile, px)`: the caller's own ARGB4444 texels (dust, paper, a scanned grain); the library uploads them and works out their mean a channel. `sh4der_lens_grain_use(&g, tex, tile, px, mean)`: a texture already in video memory; nothing is allocated.
- **The tile.** A side of 16, 32, 64 or 128 texels (512 B, 2 KB, 8 KB, 32 KB); 1 or 2 screen pixels a texel; a seed; the kind: `SH4DER_LENS_GRAIN_MONO` (the first form's), `_CHROMA` (a level of its own in each of R, G, B: colour grain), `_SOFT` (the mean of two levels: on the 64 x 64 tile 63 texels at black or white against MONO's 501).
- **Where and how strong.** `sh4der_lens_grain_draw(&g, frame, x0, y0, x1, y1, z, amp, neutral)`: a rectangle, a depth key, an amplitude a draw (0 .. `SH4DER_LENS_GRAIN_AMP_MAX`). `neutral` 1 draws the boost quad, so the mean brightness holds, a channel (a coloured tile stays neutral in each); 0 leaves it out: one layer, not two, and the picture darker by amp x (1 - the tile's mean), about 3 % at 0.06.
- **Movement.** `sh4der_lens_grain_move(&g, move, step)`: `SH4DER_LENS_MOVE_JUMP` (a new offset every frame, the first form), `_HOLD` (every `step` frames: film at 30 or 20 a second on a 60 Hz screen), `_DRIFT` (`step` texels down a frame), `_STILL`.

**Variations: a vignette of the caller's own.** Start from `sh4der_lens_vig_default`, then change `strength`, `inner`, `outer`, `tint`, `spokes`, `rings`, the shape with `sh4der_lens_vig_round` (0 the screen's own ellipse .. 1 a circle), the centre with `sh4der_lens_vig_centre` (off the middle: a scope, a torch held to one side; the mesh still covers the screen) and `curve` (`SH4DER_LENS_CURVE_SMOOTH`, `_LINEAR`, `_SQUARE`). `sh4der_lens_vignette_shape(&v)` gives it to the library's draw; `sh4der_lens_vignette_draw_at(ox, oy, z)` draws it moved and at a depth key (a picture band that does not start at the screen's corner); `sh4der_lens_vignette_mesh_draw(mesh, verts, per, ox, oy, z)` draws a mesh the caller built once with `sh4der_lens_vig_mesh`, so nothing is rebuilt a frame.

**Guards.** Every lens draw checks that its polygon header was compiled and sends nothing when it was not (a draw before `sh4der_lens_init`, `sh4der_lens_init_headers` or the grain's own make): a zeroed header hangs a real console. `sh4der_lens_ready()` says whether the first form's tile is there; `sh4der_lens_init` returns 1 also when video memory was short and it is not. The new draws return the triangles they sent.

**Cost.** NOT MEASURED: the lens has no console row. Exact counts: video memory 2 B a texel (above); TA a frame: grain 4 vertices and 1 header, 8 and 2 with neutral; vignette (rings + 1) x 2 x (spokes + 1) vertices, 594 at the default 32 x 8, 350 at 24 x 6. ESTIMATE: each grain layer over the whole screen is one translucent layer, about 3.1 ms of PVR at 640 x 480 (PVR-002 is the MEASURED cost of a plain layer, not of this one); a rectangle costs its share of the screen; the tile's size, kind and movement change no fill.

```c
int      sh4der_lens_init(void);                  int  sh4der_lens_ready(void);       void sh4der_lens_init_headers(void);
int      sh4der_lens_vignette_setup(float w, float h);
int      sh4der_lens_vignette_shape(const sh4der_lens_vig_t *shape);
void     sh4der_lens_vignette_draw(void);
unsigned sh4der_lens_vignette_draw_at(float ox, float oy, float z);
unsigned sh4der_lens_vignette_mesh_draw(const sh4der_lens_vtx_t *mesh, unsigned verts, unsigned per, float ox, float oy, float z);
void     sh4der_lens_grain(uint32_t frame, float w, float h);
int      sh4der_lens_grain_make(sh4der_lens_grain_t *g, unsigned tile, unsigned px, uint32_t seed, unsigned kind);
int      sh4der_lens_grain_load(sh4der_lens_grain_t *g, const uint16_t *texels, unsigned tile, unsigned px);
int      sh4der_lens_grain_use(sh4der_lens_grain_t *g, void *tex, unsigned tile, unsigned px, const float mean[3]);
void     sh4der_lens_grain_move(sh4der_lens_grain_t *g, unsigned move, unsigned step);
unsigned sh4der_lens_grain_draw(const sh4der_lens_grain_t *g, uint32_t frame, float x0, float y0, float x1, float y1, float z, float amp, int neutral);
void     sh4der_lens_grain_free(sh4der_lens_grain_t *g);     void sh4der_lens_grain_forget(sh4der_lens_grain_t *g);
void     sh4der_lens_hits_draw(const sh4der_lens_hit_t *slots, unsigned n, float w, float h);
void     sh4der_lens_flash_draw(const sh4der_lens_flash_t *f, const float rgb[3], int additive, float w, float h);
void     sh4der_lens_forget(void);
```

The maths behind them is inline and host tested: `sh4der_lens_grain_fill`, `sh4der_lens_grain_tile_mean`, `sh4der_lens_grain_uv_ex`, `sh4der_lens_grain_colours`, `sh4der_lens_vig_round`, `sh4der_lens_vig_centre`.

### Lens tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_LENS_VIG_STRENGTH | 0.45 | Darkness at full (0..1): the multiplier falls to 1 - this |
| SH4DER_LENS_VIG_INNER | 0.60 | Metric radius where the falloff starts |
| SH4DER_LENS_VIG_OUTER | 1.10 | Metric radius at full strength (> INNER) |
| SH4DER_LENS_VIG_ROUND | 0.0 | 0: screen-shaped (follows the aspect), 1: a circle |
| SH4DER_LENS_VIG_SPOKES | 32 | Spokes round the screen (3..SH4DER_LENS_SPOKES_MAX). 16 gives 9.1 / 255 worst Gouraud error, 24 gives 4.0, 32 gives 2.3 (host sweep) |
| SH4DER_LENS_VIG_RINGS | 8 | Rings across INNER..OUTER (1..SH4DER_LENS_RINGS_MAX) |
| SH4DER_LENS_TINT_R | 1.0 | Vignette tint: the multiplier per channel at full (1 = neutral) |
| SH4DER_LENS_TINT_G | 1.0 | tint g |
| SH4DER_LENS_TINT_B | 1.0 | tint b |
| SH4DER_LENS_GRAIN_TILE | 64 | Noise tile, texels a side (power of two, 64 x 64 x 2 B = 8 KB VRAM) |
| SH4DER_LENS_GRAIN_AMP | 0.06 | Grain swing: the frame x (1 - a + a x noise), noise 0..1 (a = 0.06: about +-3 %) |
| SH4DER_LENS_GRAIN_PX | 1 | Screen pixels a texel covers (1 fine, 2 coarse) |
| SH4DER_LENS_HIT_MAX | 4 | Wedges at once |
| SH4DER_LENS_HIT_ATTACK_S | 0.05 | Seconds to full alpha |
| SH4DER_LENS_HIT_HOLD_S | 1.0 | Full alpha this long, then fades |
| SH4DER_LENS_HIT_LIFE_S | 1.5 | Gone at this age |
| SH4DER_LENS_HIT_MERGE_COS | 0.9397 | cos(20 degrees): a hit this close to a live wedge refreshes it |
| SH4DER_LENS_HIT_ALPHA | 0.8 | Alpha on the edge at full |
| SH4DER_LENS_HIT_WIDTH | 40.0 | Half width of the wedge on the edge, pixels |
| SH4DER_LENS_HIT_DEPTH | 96.0 | Depth inward, pixels (alpha 0 there) |
| SH4DER_LENS_HIT_TAPER | 0.45 | The inner edge's half width as a share of WIDTH |
| SH4DER_LENS_HIT_R | 1.0 | Wedge colour (red) |
| SH4DER_LENS_HIT_G | 0.10 | hit g |
| SH4DER_LENS_HIT_B | 0.05 | hit b |
| SH4DER_LENS_FLASH_PEAK | 0.35 | Highest level a hit reaches (0..1) |
| SH4DER_LENS_FLASH_ATTACK_S | 0.03 | Level follows the target with this time constant |
| SH4DER_LENS_FLASH_DECAY_S | 0.25 | The target falls with this time constant |
| SH4DER_LENS_FLASH_R | 0.60 | Shield pop colour (blue-white) |
| SH4DER_LENS_FLASH_G | 0.80 | flash g |
| SH4DER_LENS_FLASH_B | 1.00 | flash b |
| SH4DER_LENS_Z_VIG | 430.0 | Autosort z of each overlay (TR list). Under HUD (1000) |
| SH4DER_LENS_Z_GRAIN | 432.0 | z grain |
| SH4DER_LENS_Z_HIT | 950.0 | z hit |
| SH4DER_LENS_Z_FLASH | 960.0 | z flash |
| SH4DER_LENS_GRAIN_TILE_MIN | 16 | A caller's grain tile: the least texels a side (512 B of video memory) |
| SH4DER_LENS_GRAIN_TILE_MAX | 128 | And the most (32 KB); a power of two between the two |
| SH4DER_LENS_GRAIN_HOLD | 2 | Frames a HOLD grain keeps one offset |
| SH4DER_LENS_GRAIN_DRIFT | 1 | Texels a frame a DRIFT grain slides down |
| SH4DER_LENS_GRAIN_AMP_MAX | 0.5 | The most a grain's amplitude may be: past it the boost cannot give the mean back |
| SH4DER_LENS_SPOKES_MAX | 32 | Spokes the maths takes at most |
| SH4DER_LENS_RINGS_MAX | 16 | Rings the maths takes at most |

## The focus effect

`sh4der_focus.h`, `src/sh4der_focus.c` (the KOS PVR drawing, Dreamcast only): fake depth of field: a small blurred map of the frame drawn through a vertex-alpha mask that keeps a disc, band or rect sharp.

### Focus tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_FOCUS_SRC_W | 80 | Sample grid across (the look read's grid) |
| SH4DER_FOCUS_SRC_H | 60 | Sample grid down |
| SH4DER_FOCUS_TEX_W | 128 | Blur texture width (PVR: power of 2; the grid is the top-left) |
| SH4DER_FOCUS_TEX_H | 64 | Blur texture height (power of 2) |
| SH4DER_FOCUS_GRID_X | 16 | Mesh cells across the frame |
| SH4DER_FOCUS_GRID_Y | 12 | Mesh cells down the frame |
| SH4DER_FOCUS_SCREEN_W | 640.0 | Frame width, pixels |
| SH4DER_FOCUS_SCREEN_H | 480.0 | Frame height, pixels |
| SH4DER_FOCUS_FEATHER | 48.0 | Mask falloff, pixels |
| SH4DER_FOCUS_MAX | 1.0 | Blur at full (0..1) |
| SH4DER_FOCUS_DISC_R | 120.0 | Scope disc radius, pixels |
| SH4DER_FOCUS_BAND_H | 60.0 | Tilt-shift band half height, pixels |
| SH4DER_FOCUS_RECT_W | 100.0 | Subject rect half width, pixels |
| SH4DER_FOCUS_RECT_H | 80.0 | Subject rect half height, pixels |
| SH4DER_FOCUS_Z | 1.0 | Mesh depth (the list's sort value is the client's) |

## The visor effect

`sh4der_visor.h`, `src/sh4der_visor.c` (the KOS PVR drawing, Dreamcast only): visor looks as number tables (night, amber, thermal): a tint through the look grade, grain, scanlines, a luma ramp for thermal, a fade in and out.

### Visor tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_VISOR_FADE_IN_S | 0.25 | Seconds from off to on |
| SH4DER_VISOR_FADE_OUT_S | 0.40 | Seconds from on to off |
| SH4DER_VISOR_GRAIN_TEX | 32 | Grain tile, pixels (power of 2) |
| SH4DER_VISOR_GRAIN_SPEED_U | 0.37 | Grain scroll per frame, tile widths (U) |
| SH4DER_VISOR_GRAIN_SPEED_V | 0.61 | Grain scroll per frame, tile widths (V) |
| SH4DER_VISOR_SCAN_TEX | 8 | Scanline tile, texels (power of 2) |
| SH4DER_VISOR_SCAN_PX | 2 | Pixels per scanline texel row |
| SH4DER_VISOR_VIG_R | 280.0 | Vignette: clear disc radius, pixels |
| SH4DER_VISOR_VIG_FEATHER | 200.0 | Vignette falloff, pixels |
| SH4DER_VISOR_Z | 1.0 | Depth for the overlays (the list's sort value is the client's) |

## The torch effect

A flashlight's hotspot with no shader. The frame is the product of four factors, each drawn by the client:

`out = base x (1 + near(vertex)) x carve(r) x (1 + boost(r)) x (1 - reach(depth))`

- **The near add.** Per vertex, by distance only: a vertex within `SH4DER_TORCH_NEAR_WU` of the eye gets `near_k x k x (1 - d2 / reach^2)` added. No cone maths; the client does it where it lights its vertices.
- **The carve.** A screen-space MULTIPLY layer: 1 inside the beam's edge circle, `SH4DER_TORCH_MUL_OUT` outside it, feathered from `EDGE_IN` to `EDGE_OUT`. It darkens what the near add lit outside the spot. The beam starts at the eye, so its edge is a circle about the screen centre at radius focal x tan(edge), at every depth (`sh4der_torch_radii`).
- **The boost.** A screen-space BOOST layer over the frame's own texels: a hot core and a soft corona, up to twice at the centre. Brick stays brick.
- **The reach.** The fog table bent toward black with depth (`sh4der_torch_fog_bend`), so the lit near world fades out per pixel.

The darkness gate (`sh4der_torch_gate_step`) reads the baked luminance of the near vertices, not the frame, so the carve does not feed back on itself. It switches on below `ON_LO` and off above `OFF_HI`, and holds between them. The power ramp (`sh4der_torch_power_step`) takes the flashlight's per-tick rates.

The two meshes are strips: 9 strips of 34 vertices (5 boost bands, 4 carve bands), 306 vertices and 288 triangles a frame. The KOS drawing (`sh4der_torch_headers`, `sh4der_torch_draw`) is Dreamcast only. No texture, no video memory; the client's vertex buffer is `SH4DER_TORCH_VERTS` x 16 bytes (4.9 KB).

Under anti-aliasing 2X (KOS `fsaa_enabled`) the PVR draws twice the width and the scaler halves it, so vertex x is in the wide space. Scale x exactly once: the centre `cx`, the x pixel scale given to `sh4der_torch_radii` (so `rx`), and the width given to `sh4der_torch_umax`; y stays. If the projection's x scale already holds the factor, do not scale again: twice puts the spot on the right edge as an oval.

### Calls

```c
float sh4der_torch_tan_of_cos2(float c2);
void sh4der_torch_radii(float sx, float sy, float tan_r, float *rx, float *ry);
float sh4der_torch_boost_at(float u, float k);
float sh4der_torch_carve_at(float u, float k);
float sh4der_torch_near(float dist2, float k);
float sh4der_torch_near_at(float dist2, float k, float near_k, float inv_r2);
float sh4der_torch_fog_bend(float table, float depth, float k);
float sh4der_torch_power_step(float p, int on, float dt);
void sh4der_torch_gate_reset(sh4der_torch_gate_t *g);
float sh4der_torch_dark(float lum);
float sh4der_torch_dark_range(float lum, float lo, float hi);
float sh4der_torch_gate_step(sh4der_torch_gate_t *g, float lum, float dt);
float sh4der_torch_gate_step_scaled(sh4der_torch_gate_t *g, float lum, float dt, float scale);
float sh4der_torch_umax(float cx, float cy, float rx, float ry, float w, float h);
unsigned sh4der_torch_boost_mesh_tint(sh4der_torch_vert_t out[SH4DER_TORCH_VERTS], float cx, float cy, float rx, float ry, float power, float tint_g, float tint_b);
unsigned sh4der_torch_boost_mesh_core(sh4der_torch_vert_t out[SH4DER_TORCH_VERTS], float cx, float cy, float rx, float ry, float power, float tint_g, float tint_b, float hot_u);
unsigned sh4der_torch_carve_mesh_to(sh4der_torch_vert_t out[SH4DER_TORCH_VERTS], float cx, float cy, float rx, float ry, float k, float umax);
unsigned sh4der_torch_boost_mesh(sh4der_torch_vert_t out[SH4DER_TORCH_VERTS], float cx, float cy, float rx, float ry, float power);
unsigned sh4der_torch_carve_mesh(sh4der_torch_vert_t out[SH4DER_TORCH_VERTS], float cx, float cy, float rx, float ry, float k);
```

`sh4der_torch_near_at` takes its own strength and reach (`inv_r2` = 1 / reach^2); `sh4der_torch_near` is it with the default values. Both meshes write `SH4DER_TORCH_VERTS` vertices at most.

### Torch tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_TORCH_BOOST | 0.6 | Core boost at the centre (x (1 + this) at full power, plus HOT) |
| SH4DER_TORCH_HOT | 0.4 | Extra hot centre, linear to 0 at HOT_U |
| SH4DER_TORCH_HOT_U | 0.2 | Radius (share of the edge) the hot centre reaches |
| SH4DER_TORCH_TINT_G | 0.96 | Green share of the boost (FLASHLIGHT_TINT_G) |
| SH4DER_TORCH_TINT_B | 0.88 | Blue share of the boost (FLASHLIGHT_TINT_B) |
| SH4DER_TORCH_MUL_OUT | 0.35 | Carve level outside the edge at full darkness |
| SH4DER_TORCH_EDGE_IN | 0.9 | Carve starts (share of the edge radius) |
| SH4DER_TORCH_EDGE_OUT | 1.0 | Carve full (the edge) |
| SH4DER_TORCH_UMAX | 1.6 | Carve mesh reaches this share (screen corners at 1.25) |
| SH4DER_TORCH_NEAR_K | 0.425 | Near add at the eye, x gate x power (FLASHLIGHT_STRENGTH) |
| SH4DER_TORCH_NEAR_WU | 7.0 | Near add reach, world units |
| SH4DER_TORCH_FOG_NEAR | 2.0 | Reach fade starts at this depth, world units |
| SH4DER_TORCH_FOG_FAR | 6.0 | Reach fully black at this depth (x gate x power) |
| SH4DER_TORCH_ON_LO | 0.12 | Gate on below this baked luminance |
| SH4DER_TORCH_OFF_HI | 0.22 | Gate off above this (hysteresis: the band between holds) |
| SH4DER_TORCH_GATE_UP_S | 0.4 | Gate time constant toward on |
| SH4DER_TORCH_GATE_DOWN_S | 1.0 | Gate time constant toward off |
| SH4DER_TORCH_DARK_LO | 0.30 | Baked luminance up to which the carve is full |
| SH4DER_TORCH_DARK_HI | 0.80 | Baked luminance from which there is no carve |
| SH4DER_TORCH_RISE_S | 0.2 | Power: seconds to full (flashlight: 6 ticks at 30 Hz) |
| SH4DER_TORCH_FALL_S | 0.8 | Power: seconds to none (flashlight: 24 ticks at 30 Hz) |
| SH4DER_TORCH_SPOKES | 16 | Spokes of both meshes (the unit table in sh4der_torch.c is 16) |
| SH4DER_TORCH_RINGS | 5 | Boost bands, centre to edge |
| SH4DER_TORCH_BANDS | 4 | Carve bands, edge feather to UMAX |

## The ao effect

`sh4der_ao.h`: small-scale occlusion: crease darkening from the angle between faces for a bake, and a screen cavity cue from a small map.

### Ao tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_AO_CREASE_K | 0.5 | Vertex factor at a full concave crease is 1 - this (0.5: half the colour) |
| SH4DER_AO_CREASE_FULL | 2.0 | Sum of concave edge amounts at a vertex that counts as full (two right-angle edges) |
| SH4DER_AO_SCR_GRID_X | 80 | Screen grid across (the look read's grid) |
| SH4DER_AO_SCR_GRID_Y | 60 | Screen grid down |
| SH4DER_AO_SCR_K | 0.5 | Multiply at full cavity is 1 - this |
| SH4DER_AO_SCR_ENGAGE | 0.2 | Brighter-than-above ratio below which nothing is darkened (the luma noise floor) |
| SH4DER_AO_SCR_FULL | 0.6 | Ratio (centre - above) / centre at full cavity |
| SH4DER_AO_SCR_FLOOR | 4.0 | Luma (0..255) added to both before the ratio: black is no division by zero |

### The aobake effect (baked occlusion against exact shapes)

`include/sh4der_aobake.h`, header only; `test/aobake_test.c` tests it on the host. One byte a vertex, 255 open .. 0 closed, from cosine-weighted rays over the vertex's hemisphere against exact occluders the caller owns (spheres, boxes, a ground plane); a start inside an occluder is closed. Paid once at load (`sh4der_aobake_bytes`, with a tick every 64 vertices for a watchdog); a frame costs one multiply a vertex on the ambient light. The ao4 scenes bake through it: scene A 252 to 254 ms, scene B 391 to 393 ms (SH4-076, SH4-082, MEASURED, one console). Its rays are the same set as `tools/ao_bake_core.h`'s (checked by the test). It differs from `sh4der_ao.h` (crease and screen cavity), from the host tool below (a triangle list, an inside fill) and from `sh4der_aomesh.h` (a triangle mesh through a cell grid, 12 rays, a ground grid). Wiki: effects/aobake.md.

#### Aobake tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_AOBAKE_RAYS | 64 | Rays a vertex (a start value: each call takes its own count) |
| SH4DER_AOBAKE_REACH | 3.0 | Metres a ray looks for an occluder (a start value) |
| SH4DER_AOBAKE_PUSH | 0.02 | Metres a ray starts off its surface (a start value) |

### The aomesh effect (baked occlusion over a triangle mesh)

`include/sh4der_aomesh.h`, header only; `test/aomesh_test.c` tests it on the host. One byte a vertex, 255 open .. 0 closed, from 12 golden-ratio rays over the vertex's hemisphere, each against the triangles of the 2 m cells its box touches; a hit at t inside the 1.6 m reach closes the ray by 1 - t / reach. An optional ground grid gives one byte a 0.5 m cell over a rectangle and a margin, normal up, for a ground that moves with the camera (NULL rectangle: the vertex bytes only). The plane y = 0 occludes the vertices, not the grid. Paid once at load: `sh4der_aomesh_need` gives the sizes of the caller's buffers, `sh4der_aomesh_bake` fills them (a tick every 256 vertices and every 8 ground rows); it writes nothing and returns 0 when a buffer is too small. A frame costs one table read. It differs from `sh4der_aobake.h` (exact shapes, 64 Hammersley rays), from `sh4der_ao.h` (crease and screen cavity) and from the host tool `tools/ao_bake_core.h` (the same kind of bake, off the console). The walled square's bake, `demo/psm/psm_ao.h`, is an adapter over it. PROTOTYPE v0.1: the bake's time and the look on a console are NOT MEASURED. Wiki: effects/aomesh.md.

#### Aomesh tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_AOMESH_RAYS | 12 | Rays a vertex: a golden-ratio spiral over the hemisphere, cosine weighted |
| SH4DER_AOMESH_REACH | 1.6 | Metres a ray looks for an occluder; a hit at t closes the ray by 1 - t / reach |
| SH4DER_AOMESH_PUSH | 0.03 | Metres a ray starts off its surface |
| SH4DER_AOMESH_CELL | 2.0 | Metres: the triangles' grid; not under the reach, so a ray touches 4 cells at most |
| SH4DER_AOMESH_GCELL | 0.5 | Metres: the ground's grid of bytes |
| SH4DER_AOMESH_MARGIN | 8.0 | Metres: the ground's grid goes this far past the rectangle |
| SH4DER_AOMESH_GMAX | 65536 | The most cells of the ground's grid |

### The raytri effect (a ray against a triangle)

`include/sh4der_raytri.h`, header only; `test/raytri_test.c` tests it on the host (plain C, and SH4ZAM's software back end through `make test-shz`). It is the arithmetic of one ray and one triangle, written once: the numerators (det and the two edge tests, with the sign of det taken off by a negate), a distance by one reciprocal, and a hit test that takes no divide. The aomesh bake calls it for each ray and triangle pair; the ray bench's kernels (`demo/psm/bench/ray_kernels.h`) call it for their general and their prepared forms, and keep their reference functions as they were. Nothing is measured on a console: the time is NOT MEASURED.

The prepared form takes 1 / det through SH4ZAM's reciprocal (`sh4der_recip_pos_`, the sign taken off; a plain divide only under `SH4DER_CORE_NO_SH4ZAM`). Until 2026-11-26 it kept the bench's plain divide; it was moved on purpose, so that every reciprocal goes through SH4ZAM. On the host nothing moved: over 100,000 random triangles and 2,000,000 rays the old and new prepared fields differ by 0 ulp and 0 hits flip, on both paths (SH4ZAM's software back end gives the exact reciprocal), and the bench's dump is byte-equal. On a console FSRRA is an approximation, so the bench's bytes there may move: NOT MEASURED.

Limits: not watertight (a ray through a shared edge is taken by at least one of the two triangles on the test's sampled diagonal, and no proof covers every ray); the hit test has no far limit (its `eps` is the only test on distance); a NaN direction misses.

There are no `SH4DER_` macros here, so there is no tuning table: the band (|det| 1e-9) and the distance floor (1e-4) are the callers' long-standing constants.

### The aosky effect (occlusion of the sky drawn into a shadow target)

`include/sh4der_aosky.h`, header only; `test/aosky_test.c` tests it on the host. The casters are drawn into the sun's shadow target K more times, each slid onto the ground along one sky direction (60 degrees up, K azimuths) as a faint black, so the ground layer that carries the sun's shadow carries the occlusion too: no new target, no new layer. `sh4der_aosky_dir` sends one direction through the caller's casters, map and scratch (it leaves its matrix in XMTRX on the Dreamcast); a uniform map only. Draw one side of closed casters with plain blending: the depth union (one shadow a direction) failed on the PVR (PVR-127). The ao4 scenes ran it (PVR-138 to PVR-186, one console). Moved from `demo/psm/ao_layer.h` and `demo/psm/ao/ao4_draw.h` on 2026-11-26; the town's sender `psm_ao2_send` stays in `ao_layer.h`. Wiki: effects/aosky.md.

#### Aosky tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_AOSKY_DIRS_MAX | 8 | Most sky directions a call takes (a ring of eight; `sh4der_aosky_count` rounds k to 0, 1, 2, 4 or 8) |
| SH4DER_AOSKY_FLOOR | 0.5 | What is left of the ground's light with every direction blocked |
| SH4DER_AOSKY_Z0 | 1.25 | Direction 0's depth in the map pass (the sun's casters 1.0, the border 2.0) |
| SH4DER_AOSKY_ZSTEP | 0.0625 | A direction's step nearer |
| SH4DER_AOSKY_XY_LIMIT | 2048 | A triangle with a corner farther than this from the target is left out |
| SH4DER_AOSKY_AREA_MIN | 1.0e-3 | Target pixels squared: a smaller triangle is not sent |

`SH4DER_AOSKY_COT_EL` (cot 60 degrees) is a constant, on `tools/check_tuning.py`'s exempt list.

### The ao bake (a host tool)

`tools/ao_bake_core.h` is a build-time tool, not an effect: it bakes one ambient occlusion byte a vertex from a triangle list on the host (64 cosine rays, 2.5 m reach, the rules of `demo/psm/ao/tools/room_bake.c`). It fills points inside a closed solid from their neighbours and wraps a street that repeats every period. Plain C, libc only; nothing of it runs on the console, which keeps only the baked bytes. Client: `demo/pl4nar/tools/map_ao.c`. Test: `test/ao_bake_test.c`, run by `make test-demo` (it makes the room's data with `demo/psm/ao/build.py --room-only`). Its host time is not measured. See `wiki/effects/ao-bake.md`.

## The frame texture and chromatic aberration

`sh4der_ca.h`, `src/sh4der_ca.c`. The PVR cannot read the frame it is drawing, but it can draw a frame into a texture and then draw that texture. The world goes into a render target; the screen is that texture as one quad, or three times over a small mesh, once per colour channel, each sampled at its own scale about the centre.

Video memory, as the PVR allocates it (whole tile rows, RGB565):

| Size | One texture | Two (a temporal blend's history) |
| --- | --- | --- |
| 640 x 480 | 640 KB | 1,280 KB |
| 320 x 240 | 160 KB | 320 KB |

Call order, a frame:

```c
static sh4der_ca_gpu_t ca;                 /* zeroed */

if (!sh4der_ca_setup(&ca, half, 1))        /* once, and again when the size or count changes */
    printf("CA: needs %u KB more VRAM\n", ca.short_kb);   /* nothing is held: draw as before, the effect is off */

pvr_wait_ready();
if (sh4der_ca_begin(&ca, 0) == 0) {        /* in place of pvr_scene_begin() */
    /* draw the world; multiply its screen x and y by ca.size.scale */
    pvr_scene_finish();
    pvr_wait_ready();
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_OP_POLY);
    sh4der_ca_channel(&ca, 0, SH4DER_CA_GREEN, strength, 640.0f, 480.0f, 1.0f);   /* or sh4der_ca_quad(&ca, 0, 1.0f, 0, 0, 1.0f, 1.0f) with no aberration */
    pvr_list_finish();
    pvr_list_begin(PVR_LIST_TR_POLY);
    sh4der_ca_channel(&ca, 0, SH4DER_CA_RED, strength, 640.0f, 480.0f, 2.0f);
    sh4der_ca_channel(&ca, 0, SH4DER_CA_BLUE, strength, 640.0f, 480.0f, 3.0f);
    /* the screen effects and the HUD, sharp, then pvr_list_finish(); pvr_scene_finish(); */
}
```

- **Strength.** 1 puts red and blue `SH4DER_CA_SPLIT_PX` (3) screen pixels apart in the corner, green half way; the split grows with the square of the distance from the centre.
- **No memory.** `sh4der_ca_setup` that cannot get its textures gives back what it took, returns 0 and leaves `short_kb`: the KB that were missing. Nothing else fails; `sh4der_ca_begin` then returns -1 and the client draws to the screen as before.
- **One stride.** The PVR has one stride register for every stride texture; `sh4der_ca_setup` sets it to the frame texture's width.
- **Two scenes a frame.** The world's scene and the screen's scene: the frame's time must hold both.
- **KOS path.** `pvr_scene_begin_rtt` with a size, a `PVR_TXRFMT_X32_STRIDE` texture, the vertices by direct render (`pvr_dr_target`, `pvr_dr_commit`).

`test/ca_test.c`: the two sizes and their video memory, the shortfall in KB, no shift in the centre, the default split in the corner, green half of red, blue unmoved, every sample inside the picture, texel-centre coordinates.

### Chromatic aberration tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_CA_SPLIT_PX | 3.0 | Strength 1: red and blue this many screen pixels apart in the corner |
| SH4DER_CA_MESH_X | 8 | Cells across a channel's mesh |
| SH4DER_CA_MESH_Y | 6 | Cells down a channel's mesh |
| SH4DER_CA_ROWS_MESH_X | 2 | Cells across the banded mesh (`sh4der_ca_channel_rows`): the rows are the client's |
| SH4DER_CA_TEXTURES | 2 | Most frame textures held (two: a history for a temporal blend) |

## The shadow shafts

`sh4der_shafts.h`, `src/sh4der_shafts.c`. The glow's god rays are the frame's bright parts stretched away from the sun. These are the other half: the frame's dark parts cast a darker copy of themselves, pushed away from the light.

Call order, a frame (the read is `sh4der_frame_read`'s untouched grid):

```c
static sh4der_shafts_gpu_t shafts;
static uint16_t shade[SH4DER_GLOW_TEX_W * SH4DER_GLOW_TEX_H] __attribute__((aligned(32)));

sh4der_shafts_init(&shafts);                                             /* once: 16 KB of video memory */
sh4der_shafts_clear(shade, SH4DER_GLOW_TEX_W, SH4DER_GLOW_TEX_H);

const float toward = sh4der_shafts_toward(facing);                       /* facing: cosine between the view axis and the light */
if (light_on_screen && toward > 0.01f) {
    sh4der_shafts_map(shade, SH4DER_GLOW_TEX_W, raw_grid, su, sv, sh4der_shafts_depth(strength, toward));
    sh4der_shafts_upload(&shafts, shade);                                /* before the list that draws it */
    sh4der_shafts_draw(&shafts, su, sv, sh4der_shafts_scale(strength), 640.0f, 480.0f, z);   /* translucent list, under the look and the glow */
} else {
    sh4der_shafts_clear(shade, SH4DER_GLOW_TEX_W, SH4DER_GLOW_TEX_H);    /* once, when the light leaves the view */
}
```

- **What casts.** A cell darker than `SH4DER_SHAFTS_DARK_FROM` (0.30), fully from `DARK_FULL` (0.10) down, times how near the light it is on screen (full within 0.25 screen heights, gone at 0.9).
- **How dark.** `SH4DER_SHAFTS_DEPTH` (0.45) of the light at strength 1, never over 0.9, times the turn toward the light (none past 60 degrees off it, all within 18).
- **Eased.** Each upload moves a texel half way: the read is a frame or two old and carries its own shade.
- **Needs the frame read.** Where the read sees black (an emulator without full framebuffer emulation), the whole frame counts as dark and casts: gate it as the eye and the glow are gated.

`test/shafts_test.c`: the turn toward the light, the depth and its cap, what casts, near and far, a texel easing half way, the map from a read, the copy's scale.

### Shafts tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_SHAFTS_DARK_FROM | 0.30 | Luma (0..1) under which a cell casts |
| SH4DER_SHAFTS_DARK_FULL | 0.10 | Luma from which down a cell casts fully |
| SH4DER_SHAFTS_DEPTH | 0.45 | Light a full cast takes at strength 1 |
| SH4DER_SHAFTS_DEPTH_MAX | 0.9 | Most a cast ever takes |
| SH4DER_SHAFTS_PUSH | 0.12 | How far the copy is pushed from the light at strength 1 (scale 1 - PUSH) |
| SH4DER_SHAFTS_NEAR | 0.25 | A cell within this of the light on screen (screen heights) casts fully |
| SH4DER_SHAFTS_FAR | 0.9 | A cell from this far (screen heights) casts nothing |
| SH4DER_SHAFTS_FACE_FROM | 0.5 | Cosine between the view axis and the light where the shade starts (60 degrees off) |
| SH4DER_SHAFTS_FACE_FULL | 0.95 | Cosine from which the shade is full (18 degrees off) |

## The sss effect (back-lit transmission)

`sh4der_sss.h`, header only. Light that comes through a thin part of a surface toward the eye. An artistic proxy: a tint loses each channel by `exp(-absorption x path)` over a path-length the caller supplies, weighted by a back-light term. It is NOT a multiple-scattering solve: no diffusion profile, no blur across the surface.

`out = tint x exp(-absorption x path) x entry x coverage x back x visibility x gain`

`back = saturate((wrap - (view . light + distortion x view . normal)) / (1 + wrap))`

- **view, light, normal.** Unit; view from the surface to the eye, light from the surface to the light. With wrap 0 and distortion 0, back is `saturate(-view . light)`: full with the light straight behind, 0 from side-on to front-lit.
- **path.** The caller's thickness proxy, in the unit of 1 / absorption (a crest height map, a bake, a constant per material).
- **coverage, entry, visibility.** 0..1: how much of the surface is thin, light that enters (1 - Fresnel), and whether the light reaches it (a shadow term). Visibility 0 gives exactly 0.
- **wrap, distortion.** Wrap lights side-on too (0.3 to 1 for skin, leaves); distortion bends the through direction along the normal (0.1 to 0.5).
- **Drawn.** Nothing by itself: put the RGB in the PVR offset colour (`gen.specular = 1` on a textured header) or add it to the vertex colour.
- **Cost.** VRAM 0, AICA 0, RAM: the 40-byte parameter block. CPU per vertex: two 3-term dots (one with distortion 0), three `shz_expf`, about 12 multiplies / FMAC. SH4ZAM for all maths; `SH4DER_CORE_NO_SH4ZAM` for plain C.

### Calls

```c
void  sh4der_sss_init(sh4der_sss_t *p, const float tint[3], const float absorption[3], float gain, float wrap, float distortion);
float sh4der_sss_back(const sh4der_sss_t *p, const float view[3], const float light[3], const float normal[3]);
void  sh4der_sss_absorb(const float tint[3], const float absorption[3], float path, float out[3]);
void  sh4der_sss_rgb(const sh4der_sss_t *p, const float view[3], const float light[3], const float normal[3],
                     float path, float coverage, float entry, float visibility, float out[3]);
```

Water keeps a thin wrapper, `sh4der_water_crest_sss` (crest 0..1 as coverage, path from a thick to a thin length). `test/sss_test.c`: bounds, fall with path, blocked and front light give 0, linear weights, wrap, distortion; both maths back ends.

## The spark effect (pooled additive quads)

`sh4der_spark.h` (the pool and the quad maths, inline) and `src/sh4der_spark.c` (the KallistiOS draw). A muzzle flash, a tracer, an impact flash and its sparks are the same thing to the PVR: a few textured quads that add light for a few frames. The pool holds them, the two shapes draw them, and what each one looks like is a row of the caller's table. Nothing here knows a weapon.

- **Billboard.** A centre, a half size, a spin: a square facing the camera, turned about the view axis.
- **Streak.** A tail and a head: a quad along the line, turned about its own axis to face the camera. Its width never falls under `min_px` pixels, so a far tracer stays a line. `RETREAT` moves the tail to the head over the life (a tracer swallowed by its hit), `TAIL0` blacks the tail vertices (it fades along its length, no hard end), `GRAVITY` lets the update bend its velocity.
- **Brightness.** Colour x (1 - age / life), in the vertex colour; the blend is ONE / ONE, so a dying quad adds less and the order of the quads does not matter. A life of n frames is drawn n times.
- **The pool rule.** Full pool: a new quad takes the place of the one nearest its end among those without `KEEP`; a new `KEEP` quad may also take the oldest `KEEP` one; a plain quad is dropped (counted) when every slot is `KEEP`.
- **The atlas.** One texture of cols x rows cells, no alpha (black adds nothing). A billboard maps a whole cell; a streak maps u along its length, v across.
- **Guards.** The draw sends a quad only when both its triangles have screen area (`sh4der_tri_area_ok`) and its header was compiled (`sh4der_poly_hdr_ok`).
- **Cost.** RAM: the pool, 24 x 52 B + 8 B = 1,256 B, the caller's; one polygon header, 32 B. VRAM: the caller's atlas (64 x 64 VQ RGB565: 3 KB). Sound RAM (AICA): 0. TA a frame: one header + 4 vertices a live quad (24 quads: 96 vertices, 3,104 B). Code: the draw 2,776 B (SH-4, -O3); spawn and update are inline in the caller. SH4ZAM for all maths; `SH4DER_CORE_NO_SH4ZAM` for plain C.

### Calls

```c
void     sh4der_spark_reset(sh4der_spark_pool_t *pool);
int      sh4der_spark_billboard(sh4der_spark_pool_t *pool, const float centre[3], float half, float spin,
                                uint32_t argb, float life, unsigned cell, unsigned flags);
int      sh4der_spark_streak(sh4der_spark_pool_t *pool, const float tail[3], const float head[3], const float vel[3],
                             float width, uint32_t argb, float life, unsigned cell, unsigned flags);
unsigned sh4der_spark_burst(sh4der_spark_pool_t *pool, const float at[3], const float dir[3], unsigned n, float cone,
                            float speed, float len_min, float len_max, float width, uint32_t argb, float life, unsigned cell);
void     sh4der_spark_update(sh4der_spark_pool_t *pool, float dt, const float gravity[3]);
int      sh4der_spark_quad(const sh4der_spark_t *s, const sh4der_spark_view_t *view, sh4der_spark_vtx_t out[4]);
int      sh4der_spark_init(void *texture, int width, int height, int txr_format);           /* Dreamcast */
unsigned sh4der_spark_draw(const sh4der_spark_pool_t *pool, const sh4der_spark_view_t *view,
                           const sh4der_spark_screen_t *screen);                             /* Dreamcast, inside the TR list */
```

The screen record `sh4der_spark_screen_t` (shared by the spark, bolt and motes draws) is `cx`, `cy`, `focal_y_px` and a depth pair: the vertex depth key is `z_base + z_scale / z` (`sh4der_spark_depth`). A client whose world geometry sends `z_base + z_scale / depth` (an engine that scales the key, for instance by 10) sets the same pair so the sprites sit at the right depth and are not hidden behind near geometry. `sh4der_spark_screen_make(cx, cy, focal_y_px)` fills the plain key (z_base 0, z_scale 1); a `z_scale` of 0 or less is read as 1, so a record that was zero-filled before the pair existed draws as it did. A record built field by field on the stack must set both fields (or use `_make`): an unset field is garbage. `test/motes_test.c` checks the key.

A shot, in a client: a `KEEP` billboard at the muzzle (2 frames), a `KEEP | RETREAT | TAIL0` streak from the muzzle to the hit (4 frames), and at the hit a billboard plus `sh4der_spark_burst` along the reflected shot. `test/spark_test.c`: sizes and facing of both shapes, the spin, the pixel floor, the near cut (drawn, no corner nearer than the near plane), end on (refused by the area guard), retreat, tail fade, the linear fade and the n frame life, velocity and gravity, the pool rule, the atlas cells and inset, bad arguments, and a busy pool over 12 frames through the area guard; both maths back ends and -ffast-math.

### Spark tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_SPARK_MAX | 24 | Quads the pool holds (96 vertices a frame at most) |
| SH4DER_SPARK_MIN_PX | 0.8 | Least streak width on screen, pixels |
| SH4DER_SPARK_INSET | 0.0078125 | Cell edge kept out of the bilinear read, texture units: half a texel of a 64 texel atlas |
| SH4DER_SPARK_LIFE_MAX | 60.0 | Longest life, seconds (the age counter is 16 bit milliseconds) |

## The pl4nar effect (planar reflection)

`sh4der_pl4nar.h` (the maths, inline) and `src/sh4der_pl4nar.c` (the KallistiOS render target and layer header). The PVR renders to a texture, so a second, small picture of the world costs one more pass. Mirror the world about a plane, draw it at low detail into a small target, and that target is the reflection of everything near the plane. Nothing here knows a car or a road.

- **The mirror pass.** `sh4der_pl4nar_view_build` fills the four rows of screen x projection x view x mirror in closed form and loads them (`shz_xmtrx_load_rows_4x4`). Draw the world with them into the target: low detail, culled the other way (a mirrored world has reversed winding), nothing that lies under the plane (`sh4der_pl4nar_sphere_side`; the PVR has no clip plane), not the reflecting surface itself.
- **The lookup, a body.** `sh4der_pl4nar_shade`: reflect the view ray off the vertex, follow it `reach` world units, project that point with the mirror pass's rows. A curved body bends the picture as a mirror would, from the one target. A ray that leaves the picture slides to its edge; a ray that ends behind the camera fades to the sky texel at the bottom centre.
- **The lookup, a flat surface.** `sh4der_pl4nar_flat`: for a vertex in the plane the reflection is exact, the texel under the vertex's own position in the mirrored picture. The lookup is a screen position and the PVR maps a texture perspective-correct, so a large polygon swims: cut it while its corners' depths differ (`sh4der_pl4nar_tri_split` names the edge; over the ratio 1.25 the error is up to 5.6 % of the edge's length on screen).
- **The share.** Schlick's Fresnel for any F0 (at the water F0 it equals `sh4der_water_fresnel`): the layer's vertex alpha. Edge on reflects strongly, head on barely.
- **The layer.** The same vertices as the opaque surface, on the translucent list, depth test greater-or-equal, no depth write, nothing sorted by hand. Additive (source alpha / one) over paint, so a clear coat keeps the paint's colour; blended (source alpha / 1 - source alpha) for glass, water, a wet road.
- **Normals.** A body's lookup reads best off smoothed normals; light it with its own. `tools/pl4nar_normals.py IN.obj OUT.obj` makes them (area-weighted, faces within 60 degrees; `--self-test` checks it).
- **Guards.** `sh4der_pl4nar_layer_header` zeroes the header when there is no target: test it with `sh4der_poly_hdr_ok` before a vertex is sent, and drop the layer's triangles with no screen area as you drop the opaque ones (`sh4der_tri_area_ok`).
- **Limits.** One plane a target; two planes are two targets and two passes. A body's lookup is a `reach` approximation, not a traced ray. Nothing reflects itself. A body needs normals.
- **Keep the row form.** A view built with a chain of matrix calls (screen, perspective, look-at, apply) gave a wrong camera in the first client; the four rows filled by hand and loaded once ran right: one load, one FTRV, no question of clip convention, handedness or apply order. The cause was not isolated.
- **XMTRX is shared.** The single lookups need the view in XMTRX (`sh4der_pl4nar_begin`) and nothing else may touch it until the last of them: no `shz_xmtrx_*` call, no SH4ZAM matrix routine that uses XMTRX as scratch, no KallistiOS `mat_*` call. The `_n` calls load it themselves. Every lookup leaves the view there: load your own matrix again afterwards.
- **A large world.** FTRV's error is relative to the largest product in a row, and a row's constant cancels against row . p. Within a few hundred units of the origin that is under a tenth of a pixel; for a world of kilometres build the rows about the eye and pass positions less the eye.
- **KallistiOS.** `pvr_scene_begin_rtt` (a sized render-to-texture scene) is not in every KallistiOS. Compile `src/sh4der_pl4nar.c` with `-DSH4DER_PL4NAR_HAVE_RTT` where it is. Without it the file uses `pvr_scene_begin_txr`, whose rows are 1024 pixels apart: the target is 1024 x h (a 256 x 128 picture costs 256 KB, not 64 KB) and the layer's u is multiplied by `target.u_scale` (w / 1024).
- **Store queues.** Between a list's begin and its finish call nothing that uses the store queues (`pvr_txr_load`, `sq_cpy`) if you submit by direct rendering.
- **Cost.** VRAM: the target, pitch x h x 2 B. With `SH4DER_PL4NAR_HAVE_RTT` (pitch = w): 128 x 64 = 16 KB, 256 x 128 = 64 KB, 512 x 256 = 256 KB. Without (pitch = 1024): 128 KB, 256 KB, 512 KB (exact). RAM: a view 104 B (112 B where a row is 16 B aligned) and a material 24 B, the caller's; a polygon header a blend, 32 B. Sound RAM (AICA): 0. Code: the KallistiOS file 692 B with `SH4DER_PL4NAR_HAVE_RTT`, 720 B without (SH-4, -O3, measured); the maths is inline in the caller. Time: see the table; SH4ZAM for all maths, `SH4DER_CORE_NO_SH4ZAM` for plain C.

| Step | Work | Time |
| --- | --- | --- |
| Lookup, a body | a vertex: one normalise (FSRRA), two dots, one FTRV, one reciprocal | 320 looked-up vertices (364 in the body, the player car only): the street demo's column read 0.92 to 0.97 ms on a console, one run (PVR-160), about 2.9 to 3.0 us a vertex (DERIVED). Earlier: 0.43 ms for 1,623 vertices on an emulator (its clock is not cycle-true), first client, three dots and the water kernel |
| Lookup, a flat surface | a vertex: one normalise, one dot, one FTRV, one reciprocal | estimate: under the body's |
| Mirror pass, CPU | the world's low detail submitted a second time | estimate: your world pass x the share of it you mirror |
| Mirror pass, PVR | a second render, of the target's area: 32 tiles at 256 x 128, 128 at 512 x 256 | not measured. The PVR renders one scene at a time, so the screen's scene queues behind the mirror's and the mirror's behind the last screen's; a timer round the pass reads that queue (15 ms at 512 x 256 in the first client, on an emulator), not the pass. Read `pvr_get_stats` and time your two waits apart |
| Layer, PVR | the surface once more, blended | estimate: under 0.5 ms for a car body |

What to turn down, in this order: the target's size (256 x 128 is a quarter of the tiles of 512 x 256); the rate (`sh4der_pl4nar_due(frame, 2)` halves the mirror pass and leaves the reflection one frame late: keep the view the target was drawn with for the lookups); the triangles the mirror pass draws.

### Calls

```c
sh4der_pl4nar_material_t sh4der_pl4nar_preset(unsigned kind);
void     sh4der_pl4nar_mirror_point(const sh4der_pl4nar_plane_t *plane, const float p[3], float out[3]);
int      sh4der_pl4nar_sphere_side(const sh4der_pl4nar_plane_t *plane, const float centre[3], float radius);
int      sh4der_pl4nar_due(unsigned frame, unsigned every);
int      sh4der_pl4nar_viewproj_rows(sh4der_pl4nar_row_t row[4], const sh4der_pl4nar_plane_t *plane, const float eye[3],
                                     const float target[3], const float up[3], float fov, float znear, float w, float h);
int      sh4der_pl4nar_view_build(sh4der_pl4nar_view_t *v, const sh4der_pl4nar_plane_t *plane, const float eye[3],
                                  const float target[3], const float up[3], float fov, float znear,
                                  float tex_w, float tex_h, float fade_w);
void     sh4der_pl4nar_begin(const sh4der_pl4nar_view_t *v);
float    sh4der_pl4nar_fresnel(float ndotv, float f0);
void     sh4der_pl4nar_shade(const sh4der_pl4nar_view_t *v, const float pos[3], const float n[3],
                             const sh4der_pl4nar_material_t *m, sh4der_pl4nar_sample_t *o);
unsigned sh4der_pl4nar_shade_n(const sh4der_pl4nar_view_t *v, const float *pos, size_t pos_stride, const float *n,
                               size_t n_stride, const sh4der_pl4nar_material_t *m, sh4der_pl4nar_sample_t *out, unsigned count);
float    sh4der_pl4nar_flat(const sh4der_pl4nar_view_t *v, const float pos[3], const sh4der_pl4nar_material_t *m,
                            sh4der_pl4nar_sample_t *o);
unsigned sh4der_pl4nar_flat_n(const sh4der_pl4nar_view_t *v, const float *pos, size_t pos_stride,
                              const sh4der_pl4nar_material_t *m, sh4der_pl4nar_sample_t *out, float *w, unsigned count);
int      sh4der_pl4nar_tri_split(const float w[3], float ratio);
unsigned sh4der_pl4nar_alpha(const sh4der_pl4nar_sample_t *s, const sh4der_pl4nar_material_t *m);
uint32_t sh4der_pl4nar_argb(const sh4der_pl4nar_sample_t *s, const sh4der_pl4nar_material_t *m);
int      sh4der_pl4nar_target_init(sh4der_pl4nar_target_t *t, unsigned w, unsigned h);              /* Dreamcast */
void     sh4der_pl4nar_target_free(sh4der_pl4nar_target_t *t);                                      /* Dreamcast */
int      sh4der_pl4nar_target_begin(const sh4der_pl4nar_target_t *t);                               /* Dreamcast */
void     sh4der_pl4nar_target_end(void);                                                            /* Dreamcast */
int      sh4der_pl4nar_pass(const sh4der_pl4nar_target_t *t, const sh4der_pl4nar_view_t *view,
                            sh4der_pl4nar_draw_fn draw, void *user);                                /* Dreamcast, before the screen's scene */
int      sh4der_pl4nar_layer_header(const sh4der_pl4nar_target_t *t, void *hdr, unsigned additive); /* Dreamcast */
```

The single `sh4der_pl4nar_shade` and `sh4der_pl4nar_flat` need the view in XMTRX (`sh4der_pl4nar_begin`); `sh4der_pl4nar_shade_n` and `sh4der_pl4nar_flat_n` load it themselves. A 0 from `sh4der_pl4nar_target_begin` means no scene is open. `test/pl4nar_test.c`: the default numbers and the materials, a point mirrored about a plane, the view rows against a camera worked out by hand, the mirrored rows against the plain rows of the mirrored point, the refusals, Fresnel's end points and its equality with the water kernel, the exact planar case (a vertex in the plane looks up its own screen position for any reach, and the flat call agrees), every lookup inside 0 .. 1 and every alpha inside 0 .. 255, a lost ray's fade to the sky texel, the batch against the single call through a 32 B stride, the flat batch through a 16 B stride, the split edge and a triangle cut until done, the sphere against the plane; both maths back ends and -ffast-math.

### Materials

`sh4der_pl4nar_preset(kind)` gives a starting row. Paint and glass were judged on a host picture; the others are physical F0 values with gains nobody has judged on screen.

| Kind | F0 | Gain | Ground keep | Reach | Blend |
| --- | --- | --- | --- | --- | --- |
| SH4DER_PL4NAR_PAINT | 0.06 | 1.0 | 0.55 | 18 | additive |
| SH4DER_PL4NAR_GLASS | 0.10 | 1.0 | 0.55 | 18 | blended |
| SH4DER_PL4NAR_CHROME | 0.55 | 1.0 | 0.55 | 18 | blended |
| SH4DER_PL4NAR_WATER | 0.0204 | 1.0 | 1.0 | 0 (flat) | blended |
| SH4DER_PL4NAR_WET_ROAD | 0.0204 | 0.7 | 1.0 | 0 (flat) | blended |
| SH4DER_PL4NAR_FLOOR | 0.05 | 0.8 | 1.0 | 0 (flat) | additive |

A stylised look wants more than the physical F0: the first client's paint read best at 0.14, its glass at 0.24.

### Pl4nar tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_PL4NAR_F0_WATER | 0.0204 | The F0 inside `sh4der_water_fresnel`: air to water |
| SH4DER_PL4NAR_F0_PAINT | 0.06 | Clear-coated paint |
| SH4DER_PL4NAR_F0_GLASS | 0.10 | Window glass from outside (two faces) |
| SH4DER_PL4NAR_F0_CHROME | 0.55 | Chromium plate |
| SH4DER_PL4NAR_F0_FLOOR | 0.05 | A varnished or polished floor |
| SH4DER_PL4NAR_REACH | 18.0 | World units a body's reflected ray is followed before the lookup |
| SH4DER_PL4NAR_FADE_W | 4.5 | View depth under which a body's lookup fades to the sky texel: a quarter of the reach |
| SH4DER_PL4NAR_FOV_SCALE | 1.55 | Reflection field of view / main field of view for a body; a flat surface wants 1 |
| SH4DER_PL4NAR_SKY_U | 0.5 | The texel a lost ray fades to, u: the centre |
| SH4DER_PL4NAR_SKY_V | 0.97 | The texel a lost ray fades to, v: the bottom row, the sky over the camera |
| SH4DER_PL4NAR_GROUND_KEEP | 0.55 | Share of the layer kept where a body's reflected ray points straight down at the plane |
| SH4DER_PL4NAR_W_MIN | 0.001 | View depth at or under which a point is behind the reflection camera |
| SH4DER_PL4NAR_SPLIT_RATIO | 1.25 | Far corner depth / near corner depth above which a flat triangle is cut |

## The pool effect (light pools on a plane)

`sh4der_pool.h`, header only. A big ground polygon cannot be lit per vertex: it has no vertices where the light falls. So the light is its own small mesh lying on the ground, drawn on the translucent list after it. The PVR sorts and blends it for nothing; the cost is the fill.

- **Disc.** A centre and two axes in the plane, whose lengths are the two radii: a circle under a lamp, an ellipse under a car. The centre has the full alpha, the rim 0. n triangles, n + 1 vertices; the caller closes the rim (triangle k is 0, k + 1, (k + 1) % n + 1).
- **Wedge.** An apex, a direction and a side axis in the plane, a length and a half angle. The apex has the full alpha, the far arc 0, so the light dies with distance. n triangles, n + 2 vertices (triangle k is 0, k + 1, k + 2). Two wedges read softer than one: a narrow long beam and a wide short spill at half the alpha.
- **Reach (the fade by distance).** A lamp that comes into range must not pop in. `sh4der_pool_reach_make(&r, reach, width)` once a frame, then `sh4der_pool_reach_fade(&r, distance)` a lamp: 1 inside the reach less the width, 0 at the reach, linear between (one ramp of `sh4der_ramp.h`). Multiply the pool's alpha, and its halo's, by it. `sh4der_pool_reach_fade2` takes a squared distance and gives 0 past the reach with no square root; `sh4der_pool_reach_verts` fades a fan's vertices each by its own distance from the eye. The arithmetic is the street demo's lamp line, moved here.
- **Beam.** `sh4der_pool_beam`: a wedge with `rows` arcs along its length (1 + rows (n + 1) vertices; triangles from `sh4der_pool_beam_tris`), for a headlight seen from outside. Along the beam the alpha falls linearly to 0 as the wedge's does; with a reach each vertex is also faded by its own distance from the eye, so a far car's beam comes in from its near end. One row and no reach is the wedge. The player's own headlights need no reach: use the wedge.
- **Light or shadow.** Additive (source alpha / one) with the lamp's colour for light; blended (source alpha / 1 - source alpha) with black for a blob shadow. Depth test on, depth write off.
- **Lift.** Raise the fan a little off the ground along the plane's normal (2 to 4 cm at car scale) so the depth test does not fight the ground.
- **Guards.** The calls write world positions and colours; the caller transforms and submits, with a compiled header (`sh4der_poly_hdr_ok`) and its own area test (`sh4der_tri_area_ok`): a fan seen edge on has no area.
- **Cost.** RAM: the caller's vertex array, 16 B a vertex (a 10 triangle disc: 176 B). VRAM: 0. Sound RAM (AICA): 0. CPU: one sine and cosine pair and six fused multiply-adds a rim vertex. PVR: the fill of the fan, not measured. SH4ZAM for all maths; `SH4DER_CORE_NO_SH4ZAM` for plain C.

### Calls

```c
unsigned sh4der_pool_disc(const float centre[3], const float u[3], const float v[3], unsigned n,
                          uint32_t rgb, float alpha, sh4der_pool_vtx_t *out);
unsigned sh4der_pool_wedge(const float apex[3], const float dir[3], const float side[3], float length,
                           float half_angle, unsigned n, uint32_t rgb, float alpha, sh4der_pool_vtx_t *out);
int      sh4der_pool_reach_make(sh4der_pool_reach_t *r, float reach, float width);
float    sh4der_pool_reach_fade(const sh4der_pool_reach_t *r, float dist);
float    sh4der_pool_reach_fade2(const sh4der_pool_reach_t *r, float dist2);
unsigned sh4der_pool_reach_verts(const sh4der_pool_reach_t *r, const float eye[3], sh4der_pool_vtx_t *v, unsigned nv);
unsigned sh4der_pool_beam(const float apex[3], const float dir[3], const float side[3], float length,
                          float half_angle, unsigned n, unsigned rows, uint32_t rgb, float alpha,
                          const sh4der_pool_reach_t *reach, const float eye[3], sh4der_pool_vtx_t *out);
unsigned sh4der_pool_beam_tris(unsigned n, unsigned rows, uint16_t *idx);
```

The fade by distance and the beam are PROTOTYPE v0.1: host tested, their cost NOT MEASURED (ESTIMATE: one FSRRA reciprocal a `sh4der_pool_reach_make`; a subtract, a multiply and a saturate a `sh4der_pool_reach_fade`; one FSRRA square root a vertex with alpha in `sh4der_pool_reach_verts`; a 2 x 3 beam is 10 vertices and 10 triangles against the wedge's 4 and 2, the same fill). `sh4der_pool_beam` returns 0 with nothing written when a reach leaves no vertex with alpha. `test/pool_test.c` checks the reach fade against the street demo's old line (the same bits on the plain C path), the step, the squared form, the per-vertex form, the beam's arcs, alphas and triangles, and that one row is the wedge.

Both return the vertices written, or 0 with nothing written for a bad count, length, angle or pointer. The array a call writes must hold `SH4DER_POOL_MAX_N + 2` vertices (or n + 2 for the n you pass). `test/pool_test.c`: the disc's centre and rim (circle, ellipse, the plane, alpha, colour, counts), the wedge's apex and arc (length, the half angle at both ends), every triangle through the area guard, bad arguments; both maths back ends.

### Pool tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_POOL_MAX_N | 16 | Most triangles in one fan (the caller's array holds SH4DER_POOL_MAX_N + 2 vertices) |
| SH4DER_POOL_TAU | 6.28318530718 | A whole turn, radians |
| SH4DER_POOL_REACH | 60 | Metres within which a lamp's pool is drawn (the street demo's lamps) |
| SH4DER_POOL_FADE_W | 10 | Metres: a pool fades to nothing over the last this of the reach |
| SH4DER_POOL_BEAM_ROWS | 3 | Arcs along a beam (`sh4der_pool_beam`) |
| SH4DER_POOL_BEAM_ROWS_MAX | 4 | Most arcs along a beam |
| SH4DER_POOL_BEAM_VTX_MAX | 69 | Most vertices of a beam: 1 + SH4DER_POOL_BEAM_ROWS_MAX x (SH4DER_POOL_MAX_N + 1) |
## The softfade effect (soft edges for light)

`include/sh4der_softfade.h`, header only, maths only (no KallistiOS or PVR calls: it builds on the host); `test/softfade_test.c` tests it. A camera-facing glow that passes through a wall is cut on a hard line, and so is a pool or a beam of light where a wall stands on it. The PVR cannot read the depth behind a pixel. The SH-4 knows where the walls are, so it takes the light off the vertices near them. PROTOTYPE v0.1: host tested.

- **Surfaces.** A plane (unit normal toward the lit side and d with dot(n, x) = d; `sh4der_softfade_plane` makes one from a point and a normal), endless; and a wall (`sh4der_softfade_wall`: a plane over a rectangle, a centre, one unit axis and two half sizes), so a barrier does not fade a light far along its line. At most `SH4DER_SOFTFADE_MAX` planes and as many walls a call; more are refused (no fade).
- **Near** (`sh4der_softfade_near`). Is any surface within the light's radius and the fade distance? A 0: draw the light plain and call nothing else.
- **Fit** (`sh4der_softfade_fit`). A scale 0 .. 1 for a 4-vertex halo's radius and alpha, so the quad never reaches the surface. The glow shrinks near a wall; it does not fade into it. No extra vertices.
- **Fade** (`sh4der_softfade_point`; `sh4der_softfade_levels` for a mesh with a float level a vertex, `sh4der_softfade_argb` for one with the light in the alpha byte). The keep at a vertex: 0 on or behind a surface, 1 from `SH4DER_SOFTFADE_DIST` in front of it, a smooth step between; the surfaces multiply. It needs vertices where it acts: a halo as rings (`sh4der_bloom_rings`), a beam as `sh4der_pool_beam` with 3 or 4 rows.
- **Limits.** Planes and rectangles only: nothing against a car, a person or a curved shape (no depth is read). Per vertex: where a wall crosses a triangle, a hard edge of at most that triangle stays. A light within the fade distance in front of a wall is dimmed too.
- **Cost.** NOT MEASURED as this code. ESTIMATE: a plane a vertex is one 3-term dot, a subtract, a multiply, a saturate and a smooth step (3 multiplies); a wall is that and two more dots; `sh4der_softfade_make` one FSRRA reciprocal, once. No PVR pass, VRAM 0, sound RAM 0, RAM the caller's surfaces (16 B a plane, 60 B a wall). The nearest number, MEASURED as another program's own code on one console (SH4-083, PVR-157): a lamp's halo of 129 vertices faded against a room's planes, 205 us of SH-4 a frame with the halo's build in it.
- **The other route** is a camera depth map the PVR draws, read as a bump texture: per pixel, any shape. It ran on a console in a test program (the hard line gone, PVR-204; the map 5.1 ms of PVR, 1.4 ms cut to its tiles, PVR-203, PVR-205) and is not in the library.

### Calls

```c
int      sh4der_softfade_make(sh4der_softfade_t *f, float dist);
void     sh4der_softfade_plane(sh4der_softfade_plane_t *p, const float point[3], const float normal[3]);
int      sh4der_softfade_wall(sh4der_softfade_wall_t *w, const float centre[3], const float normal[3],
                              const float u[3], float hu, float hv);
int      sh4der_softfade_near(const sh4der_softfade_t *f, const sh4der_softfade_plane_t *planes, unsigned np,
                              const sh4der_softfade_wall_t *walls, unsigned nw, const float centre[3], float radius);
float    sh4der_softfade_fit(const sh4der_softfade_t *f, const sh4der_softfade_plane_t *planes, unsigned np,
                             const sh4der_softfade_wall_t *walls, unsigned nw, const float centre[3], float radius);
float    sh4der_softfade_point(const sh4der_softfade_t *f, const sh4der_softfade_plane_t *planes, unsigned np,
                               const sh4der_softfade_wall_t *walls, unsigned nw, const float p[3]);
unsigned sh4der_softfade_levels(const sh4der_softfade_t *f, const sh4der_softfade_plane_t *planes, unsigned np,
                                const sh4der_softfade_wall_t *walls, unsigned nw,
                                const float *xyz, unsigned stride, float *level, unsigned nv);
unsigned sh4der_softfade_argb(const sh4der_softfade_t *f, const sh4der_softfade_plane_t *planes, unsigned np,
                              const sh4der_softfade_wall_t *walls, unsigned nw,
                              const float *xyz, size_t xyz_stride, uint32_t *argb, size_t argb_stride, unsigned nv);
```

The two mesh calls return the vertices that still have light (0: do not send the mesh); bad arguments change nothing and mean no fade. `test/softfade_test.c`: a plane's keep (0 behind, 1 from the distance on, a half at half), a corner, the step for no distance, a wall's rectangle and its edge, near and fit, both mesh forms (a beam run into a wall), bad arguments, and the keep against a lamp's halo fade written by hand (within one level of 255); both maths back ends.

### Softfade tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_SOFTFADE_DIST | 0.30 | Metres in front of a surface over which the light comes back |
| SH4DER_SOFTFADE_MAX | 8 | Most planes, and most walls, one call takes |

## The ramp helper

`include/sh4der_ramp.h`, header only; `test/ramp_test.c` tests it on the host. A linear 0 .. 1 ramp between two distances, saturated at both ends. `sh4der_ramp_in(from, len, d)` is one call (one FSRRA reciprocal through SH4ZAM); for a loop, `sh4der_ramp_make(&r, from, to)` once, then `sh4der_ramp_up` / `sh4der_ramp_down` a point (a multiply and a saturate). It has no tuning macros: the two distances are the caller's. Used by `include/sh4der_fakeaf.h` and `demo/psm/psm_shtile.h` (which still includes it through the forward `demo/psm/psm_ramp.h`). Costs not measured. Moved from `demo/psm/psm_ramp.h` on 2026-11-26; wiki/effects/ramp.md.

## The viewcull helper

`include/sh4der_viewcull.h`, header only; `test/viewcull_test.c` tests it on the host. When the camera turns, a client that keeps last frame's placement has to place everything again: every point through the camera's matrix, every cell's area test. Much of a ground window or a terrain grid is then off the screen. This helper says which parts cannot be on the screen before they are placed. PROTOTYPE v0.1: host tested, NOT RUN on a console.

- **A box.** `sh4der_viewcull_prepare(&vc, rows, znear, zfar)` once a frame, from the camera's world-to-clip rows in column order (the form `sh4der_psm_cam_rows` writes; `zfar` 0 for no draw distance). Then `sh4der_viewcull_box(&vc, lo, hi)`: 0 when the box is wholly behind the near plane, past one side of the screen or past the draw distance; 1 when some of it may be seen. It takes two FIPR a plane (the box's centre and its half size against the plane), not eight corner transforms, and keeps a box within rounding of a side. It does not touch XMTRX.
- **A grid.** `sh4der_viewcull_grid(&vc, pts, rows, cols, SH4DER_VIEWCULL_BLOCK, seen)` tests a rows x cols grid of world points a block of cells at a time, each block's box made from its own points. `sh4der_viewcull_grid_blocks` gives the number of blocks, and `sh4der_viewcull_grid_index` gives the block of a cell. A block it leaves out holds no cell that could be on the screen, so its points need no transform and its cells no test. Do not read a placed value of a point that was not placed.
- **Client.** The shadow demo's ground window (`demo/psm/psm_scene.h`, `PSM_PLACE_GROUND`, the turn plan's 1b). On the host the walled square's views (the town's the same) place 0 % of the window's points at the sky pitch, 76 % at the boot view and 72 % looking down (4,078 of 5,335 and 4,381 of 6,111), with the same picture to the pixel (`demo/psm/test/psm_place_cmp.c`, 432 views, the ground option alone and with the other three; host counts, not a console time).
- **Cost.** Not measured. ESTIMATE: 12 FIPR a box; the grid's boxes are one pass over its points.

### Viewcull tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_VIEWCULL_BLOCK | 4 | Cells a side of a grid's block (4 x 4 cells, 5 x 5 points) |

wiki/effects/viewcull.md.

## The fakeaf effect (fake anisotropic filtering)

`include/sh4der_fakeaf.h`, header only; `test/fakeaf_test.c` tests it on the host. The PVR has no anisotropic filter: a floor seen at a grazing angle is blurred across the view or shimmers along it. The header has two ways to mend it: a mip bias per polygon by its slope (`faf_bias_code`), and distance ranges with stretched copies and a ramp between them (`faf_table_default`, `faf_table_from`, `faf_stretch`, `faf_piece`, `faf_alpha`, `faf_edges`, `faf_cuts`). The bias has run on a console (PVR-180). The ranges are helpers only. The old path `demo/psm/fakeaf.h` is a forward. Wiki: `wiki/effects/fakeaf.md`.

### Calls

1. Start with the bias, no pass and no fill: per polygon, `faf_bias_code(ndv, strength, least)`, where `ndv` is |normal . direction to the eye| (1 facing, 0 edge on). Batch the polygons by code, one header for each code (`FAF_BIAS_NONE` = 4 is 1.00; under it is sharper).
2. The ranges, only if the bias is not enough: `faf_table_default(t)` or `faf_table_from(t, n, focal, eye_h, texels_m, share)`, checked by `faf_table_ok`. Each range k above 0 reads a stretched copy from `faf_stretch(src, w, h, shift, dst)` (box filtered, halved along the travel direction; `faf_stretch_bytes` gives its video memory). For a piece at a distance, `faf_piece` names its opaque range and its blend range, and `faf_alpha` gives the blend's vertex alpha (255 at the zone's start, 0 at its end). A mesh cut at the zone edges takes `faf_edges` and `faf_cuts`.

### Fakeaf tuning

`faf_table_default` (invented starting values for an eye about 1.6 m above a floor; ESTIMATE, to be tuned on the screen):

| Range | From (m) | Ramp (m) | Bias code | Source shift |
| --- | --- | --- | --- | --- |
| 0 | 0 | 0 | 4 | 0 (the base texture) |
| 1 | 6 | 2 | 3 | 1 (halved once) |
| 2 | 14 | 4 | 2 | 2 |
| 3 | 30 | 8 | 2 | 3 |

`FAF_RANGES_MAX` (4) and `FAF_BIAS_NONE` (4) are not `SH4DER_*` macros, so there is no row for them in the tuning tables. `faf_bias_code`'s `strength` (0 .. 1) and `least` (1 .. 4) are the arguments; PVR-180 ran `least` 0.50 and 0.25 (the ledger's values).

### Fakeaf cost

| Item | Cost | Status |
| --- | --- | --- |
| Corridor at 640 x 480, bias by slope | PVR 7.21 and 7.20 ms (PVR-180); 7.19 and 7.24 ms (PVR-207) | MEASURED, one console a run |
| Corridor, a fixed bias of 0.50 | 7.71 ms (PVR-180); 7.68 ms (PVR-207) | MEASURED |
| Corridor, bilinear and trilinear | bilinear 6.17 ms (PVR-180), 6.14 ms (PVR-207); trilinear 9.44 ms (PVR-180) | MEASURED |
| The bias's own effect (the level the PVR picks) | not read back | OPEN |
| Ranges | helpers only | NOT MEASURED |
| Three stretched copies of a 256 x 256 RGB565 texture | 114,688 bytes (112 KB) | exact, by the code |
| Fill added by the ranges (3V, 3D) | 0.068 and 0.478 of a screen (the corridor model) | ESTIMATE, a model, not a measurement |

### Do and do not

- Do: start with the bias; give a sharper bias only to floor polygons at a grazing slope.
- Do: keep one range to about a doubling of distance. A stretched copy has no mip levels.
- Do not: count the bias as a proven sharper level on the PVR until the level is read back (OPEN).

## The dofmip effect (focus blur by mip bias)

`include/sh4der_dofmip.h`, header only; `test/dofmip_test.c` tests it on the host. A camera's depth of field as one number a polygon: the PVR adds the polygon header's mip bias (a code of 1 .. 15, a quarter level each; 4 is none) to the mip level it picks, so a code over 4 softens a texture. The header gives each polygon or object the code its distance from a focus plane asks for: none inside a sharp zone, rising on each side to its own strength in levels (a near and a far one, at most 2.75, code 15), along a ramp with its own power (`curve`, 0.1 to 50). The blur follows the change in 1 / z from the focus, as a thin lens's does. It blurs textures only: a flat colour, a silhouette and an edge stay sharp (the frame-wide blur of `sh4der_focus` or a render target is the other route). Nothing has run on a console, and the level the PVR picks for a bias code has never been read back (fakeaf's open item, PVR-180), so it is not proven to blur. Wiki: `wiki/effects/dofmip.md`.

### Calls

1. Once a frame, from the keyed tracks: `sh4der_dofmip_prepare(&d, focus, range, near_strength, far_strength, curve, reach)`. Nothing is kept between frames, so a track that jumps changes the settings in the frame it is given. A 0 return means a number could not be used and the focus blurs nothing.
2. Per polygon (its middle's depth) or per object (its centre's): `z = sh4der_dofmip_depth(eye, forward, point)`, then `code = sh4der_dofmip_code(&d, z)`. `sh4der_dofmip_levels` gives the blur in levels first, if you want to see it.
3. A floor that also takes fakeaf's sharper code: `sh4der_dofmip_combine(faf_bias_code(...), code)`.
4. Batch the polygons by `sh4der_dofmip_bucket(code)` (0 .. 11), one polygon header a bucket. The textures need mip maps.

### Dofmip tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_DOFMIP_CODE_NONE | 4 | The PVR's code for a bias of 1.00: none (the same as fakeaf's FAF_BIAS_NONE) |
| SH4DER_DOFMIP_CODE_MAX | 15 | The blurriest code the field has (3.75) |
| SH4DER_DOFMIP_CODES | 12 | The codes this header gives, 4 .. 15 |
| SH4DER_DOFMIP_LEVELS_MAX | 2.75 | Levels of blur past none at code 15 |
| SH4DER_DOFMIP_REACH | 1.0 | Share of 1 / focus, in 1 / z, that takes the blur to full (a starting value, ADVISED) |
| SH4DER_DOFMIP_CURVE | 1.0 | The ramp's power: 1.0 is straight, above 1.0 holds the blur back near the zone, under 1.0 gives it early (a starting value, ADVISED) |
| SH4DER_DOFMIP_CURVE_MIN | 0.1 | The least power taken |
| SH4DER_DOFMIP_CURVE_MAX | 50.0 | The most power taken |
| SH4DER_DOFMIP_STEPS | 11 | The code steps a side has above none: codes 5 .. 15 |
| SH4DER_DOFMIP_POW_BITS | 14 | Bits of an exponent's fraction the power takes (the power's error is under 0.0006) |
| SH4DER_DOFMIP_NEAR_MIN | 0.1 | Metres: the sharp zone's near end is never closer than this |

### Dofmip cost

Not measured on a console. ESTIMATE: a polygon or object, one reciprocal (FSRRA), a few multiplies and a search of 4 compares; prepare, once a frame, 22 steps of about 14 square roots each (none with a straight ramp); 116 B of state; no pass, no video memory. The power is the header's own (a whole part by multiplication, a fraction by square roots), because SH4ZAM's `shz_powf`, `shz_expf` and `shz_logf` are coarse near 1 (`shz_powf(0.99, 50)` gives 3.6, the true value is 0.605; host, 2026-11-27). The PVR's cost of a blurrier bias is not known (a sharper one cost +1.0 to +1.5 ms against bilinear in the corridor, PVR-180).

### Do and do not

- Do: batch by code; give the focus from the tracks once a frame; combine with fakeaf's code on a floor.
- Do not: expect a soft silhouette or a soft flat colour; call the blur proven before the level the PVR picks is read back.

## The psm route A pass (moving shadows the PVR draws)

`include/sh4der_psm_a.h`, `src/sh4der_psm_a.c`; `test/psm_a_test.c` tests the maths on the host and `test/tilecut_test.c` the tile cut (including per-table state). Up to four characters cast a shadow on a patch of ground from one light, with no depth compare on the SH-4: the PVR draws each caster from the light, flat dark on white, into a small render target (the pass), and the ground patch is drawn again with that target as its texture, multiplied over the screen (the layer). A body does not shadow itself, and one map has one light. Each caster has a strength 0 .. 1 (white at 0, the full shadow level at 1). The map is the library's uniform map fitted to the patch and held still, so a moving camera moves no texel. In the town demo this route cost 13.3 to 13.6 ms of SH-4 where the SH-4 depth map cost 40.2 to 43.9 ms (PVR-108 to PVR-113). **PROTOTYPE v0.2: ra2 ran per-pass cut with sleeping waits on one console (PVR fell from 3.2 ms to 0.2 ms at 128 and 0.8 ms at 256, same picture, frame stalled 16.6 -> 20.0 ms); polled waits via `pvrx` and held single-table cut (`SH4DER_PSM_A_CUT_HOLD`) are NOT RUN on a console yet.** Wiki: `wiki/effects/psm-route-a.md`.

### Calls

1. At load, after `pvr_init`: if polling render-done waits (`SH4DER_PSM_A_CUT_PASS` without the 20 ms tick stall, or `SH4DER_PSM_A_CUT_HOLD`), call `sh4der_pvrx_wait_hook(1)` (and again after any later `pvr_init`; call `sh4der_pvrx_wait_hook(0)` before `pvr_shutdown`, AGENTS.md "Call order: pvrx"). Then `sh4der_psm_a_cfg_default(&cfg)` and set `size`, `cut` (`SH4DER_PSM_A_CUT_NONE` = 0, `SH4DER_PSM_A_CUT_PASS` = 1, or `SH4DER_PSM_A_CUT_HOLD` = 2), and, for a screen whose tile table has rows left out, `row`, `rows` and `row0` (a 640 x 360 band showing rows 1 to 13: `row` 2, `rows` 13, `row0` 1); `sh4der_psm_a_target_init(&t, &cfg)` (0: no video memory, draw no shadow); `sh4der_psm_a_init(&a, size, row)`. After any screen scene's render, `sh4der_psm_a_find(&t)` until both tile tables are found (`t.cut.n == t.cut.need`; `sh4der_psm_a_settle` finds the pass's own table after an uncut pass). After a video mode change (the PVR shut down and started again): `sh4der_psm_a_release(&t)` and `sh4der_pvrx_wait_hook(0)` before `pvr_shutdown`, `sh4der_psm_a_target_forget(&t)` (no free: the memory is gone), then `pvr_init`, `sh4der_pvrx_wait_hook(1)` and `sh4der_psm_a_target_init` again.
2. Each frame: `sh4der_psm_a_begin_frame(&a, shadow)` (the full shadow's level a channel: `sh4der_psm_a_shadow_level(ambient, sun x cosine)`); `sh4der_psm_a_fit(&a, light, centre, half, y_lo, y_hi)` for the patch (light: from the floor toward the light).
3. `sh4der_psm_a_begin(&t, &a)`; for each caster, strongest first, `sh4der_psm_a_caster(&t, &a, &c)` (0: refused, draw a blob); `sh4der_psm_a_end(&t, &a)` (ends with `sh4der_pvrx_scene_finish`); `sh4der_psm_a_settle(&t)` (other than 0: shut the PVR down and start it again). On a frame without a pass when `SH4DER_PSM_A_CUT_HOLD` is on (or before any other scene that would use the pass's table, or before exit / `pvr_shutdown`), call `sh4der_psm_a_release(&t)` before opening the screen scene.
4. Then the screen's scene, ended with `sh4der_pvrx_scene_finish()` whenever `sh4der_pvrx_wait_hook(1)` is on (never `pvr_scene_finish()` directly). In its translucent list, after the ground, send `sh4der_psm_a_layer_header(&t)` with `pvr_prim`, then the receiver's triangles with each vertex from `sh4der_psm_a_layer_vertex(&a, world, sx, sy, sz, eol, &v)`; leave out a triangle with no vertex in `sh4der_psm_a_covers`.

### Psm route A tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_PSM_A_SIZE | 128 | The map's side in texels (32 to 1024, a multiple of 32) |
| SH4DER_PSM_A_SQUARE_M | 8.0 | Metres: the receiver patch's side for a 128 map (the caller's choice; the fit takes the half side) |
| SH4DER_PSM_A_CASTERS | 4 | Casters a pass |
| SH4DER_PSM_A_TRIS | 2200 | World triangles a pass (the port's budget: three casters of 400 and one of 1,000) |
| SH4DER_PSM_A_HOLD_COS | 0.99999 | The light is taken again when it moves about a quarter of a degree (the town demo's value, `PSM_HOLD_LIGHT_COS`) |
| SH4DER_PSM_A_AREA_MIN | 0.001 | A triangle in the map with less doubled area than this (texels squared) is not sent (the town's value) |
| SH4DER_PSM_A_CAST_Z | 1.0 | A caster's depth in the target: the white ground is 0.1, the border 2.0 (the town's values) |

### Psm route A cost

Build ra2 (one console, MEASURED): map pass PVR 3.2 ms uncut -> 0.2 ms cut at 128 and 0.8 ms at 256, same picture; frame 16.6 ms uncut -> 20.0 ms cut (worst 25 ms, pass total 19.9 ms). DERIVED from reading KallistiOS's source (to be tested by ra5): the 20.0 ms stall is `SH4DER_PSM_A_CUT_PASS` hitting two sleeping `pvr_wait_render_done` waits on 10 ms scheduler ticks (the same loss as PVR-117). Polled render-done waits (`sh4der_pvrx_wait_render_done_poll`) and held single-table cut (`SH4DER_PSM_A_CUT_HOLD`, cutting only the pass's own table and leaving it cut with no render-done wait in `begin` or `settle`): NOT RUN on a console yet. Town numbers for the route: SH-4 13.3 to 13.6 ms a frame against 40.2 to 43.9 ms for route B on the same scene (PVR-108 to PVR-113). ESTIMATE: 3 vertices (96 B) of TA data a caster triangle after clipping, at most 2,200 x 96 B = 211 KB for a full pass; video memory size x (row x 32 + size) x 2 B (48 KB at row 2, 32 KB at row 0).

### Where the pass sits

A render target scene is drawn from tile row 0. On a screen whose tile table has rows left out (a port that shows a 640 x 360 band inside the 640 x 480 signal and compacts the table to rows 1 to 13) row 0 has no entry, so the pass is drawn at tile row `cfg.row` (y = 32 x row) and the render base is set so the texture is exactly `size` x `size` from there: the render is `row x 32 + size` lines tall, the texture is `base + row x 32 x size x 2` bytes on. KallistiOS sets a render target's pixel clip to the target's own size (`pvr_misc.c`), not the screen's, so a screen clip that was narrowed does not cut the pass. The tile cut writes only the target's tiles (`sh4der_tilecut_begin_at`, `sh4der_tilecut_begin_one_at`, `sh4der_tilecut_init_rows(&c, tw, rows, row0)`: tile (x, row) is entry x x rows + (row - row0), the control words keep their own row numbers). Host tested against a table compacted the way the port compacts it; the compacted table is NOT RUN on a console.

### Do and do not

- Do: send the strongest caster first (an overlap keeps the first at equal depth); keep the receiver one piece a few centimetres above the floor; call `sh4der_psm_a_settle` before the screen's scene; when `sh4der_pvrx_wait_hook(1)` is on, finish every scene with `sh4der_pvrx_scene_finish` and unhook with `sh4der_pvrx_wait_hook(0)` before `pvr_shutdown`; when `SH4DER_PSM_A_CUT_HOLD` is on, call `sh4der_psm_a_release` before any frame without a pass, before another scene that would use the pass's table, at exit, and before `pvr_shutdown` (if forgotten, the screen renders through the cut table so only the pass's few tiles are drawn and the rest of that buffer is stale, and the library sees it only at the next `begin` in `cut_broken`; `SH4DER_PSM_A_CUT_HOLD` also needs the pass to be every second render — a third render per frame, such as a mirror pass, puts another scene through the held table every other frame).
- Do not: expect self-shadow; run it from a menu screen (RGB888P); send a caster `sh4der_psm_a_caster` refused; call `pvr_scene_finish` directly while `sh4der_pvrx_wait_hook(1)` is on; open a screen scene on a no-pass frame without calling `sh4der_psm_a_release` in held mode; use `SH4DER_PSM_A_CUT_HOLD` in a frame with three renders (such as a mirror pass plus shadow pass plus screen); call polled or held cut proven before a console run.

## The ground texture (paving slabs)

`include/sh4der_ground.h`, header only; `test/ground_test.c` tests it on the host. A 64 x 64 RGB565 texture of stone slabs laid in rows: each slab its own width and tone, a dark joint and a light edge a texel wide, tiling both ways. Made at load from an integer hash of a seed; no float maths. The square's ground in `demo/psm` uses it. Wiki: `wiki/effects/ground.md`.

### Calls

1. At load, a `uint16_t` array of `PSM_GROUND_N * PSM_GROUND_N` (64 * 64) texels, then `psm_ground_slabs(px, base_r, base_g, base_b, seed)` with the base colour 0 .. 255 a channel. The square uses 158, 152, 142 and seed 1 (`demo/psm/psm_dc.c`).
2. Upload it as an RGB565 texture (not twiddled; 8 KB). The same seed gives the same texels on every machine.

### Ground tuning

No `SH4DER_*` macros. The base colour and the seed are the arguments. `PSM_GROUND_N` (64) is the texture's side, fixed by the layout of the rows.

### Ground cost

| Item | Cost | Status |
| --- | --- | --- |
| Texels made at load | 4,096, integer maths | exact, by the code |
| Video memory | 8 KB (64 x 64 x 2 bytes) | exact, by the code |
| Time at load on a console | not measured | NOT MEASURED |
| Per frame | none: a texture read like any other | by construction |

## The page kit (result pages)

`include/sh4der_page.h`, header only. The console test programs print their numbers on pages that are photographed off a television, and this header draws those pages into the frame buffer after the PVR is shut down. Text pages hold 64 x 22 characters of an 8 x 16 font (a 3-to-2 reduction of the BIOS font) in the title-safe area, and each header ends in `CRC xxxxxxxx`, the CRC-32 of the page's content, so a page typed from a photograph can be checked with `tools/page_crc.py`. Graph pages have a title, labelled axes, up to four series as lines with point marks or as grouped bars, and a legend.

Call order: `pg_init()` once at 640 x 480 RGB565; `pg_say()` for lines; `pg_plot()`, `pg_series()`, `pg_point()`, `pg_cat()` for graphs; `pg_draw(page, title, keys)` to show a page; `pg_pages()` for the count. Include `<kos.h>` first: the header uses `bfont_draw`, `vram_s` and `vid_clear`, and `test/page_test.c` stands in for those three names on the host.

Settings are macros the client defines before the include. They are the kit's own `PG_*` names, not `SH4DER_*` tuning numbers, so they have no tuning rows: `PG_PREFIX` (the serial line's prefix), `PG_BIG` (1 for the BIOS font's own size, 42 x 14), `PG_TOP` (rows kept free under the header line: 1 when the labels take two lines, 2 for three), `PG_LINES_MAX` (900).

The names are unchanged from the demos' (`pg_*`, `PG_*`). `demo/psm/psm_page.h` is a forward at the old path.

Status: VID-015 and VID-016 in `wiki/hardware/ledger-video.md` are MEASURED, one run each on one console: the CRC lines agree, and 21 text pages and 4 graphs typed from two videos agree with their CRCs. Cost: drawn once a page, no frame cost; RAM by the layout (the glyphs 12 KB, the text lines 58 KB), not measured. The host test (`make test`, `test/page_test.c`) checks the arithmetic and the layout, not how the BIOS font photographs. Maturity ALPHA v0.5. Wiki: `wiki/effects/page.md`.

## The effect labels

`include/sh4der_fxlabel.h`, header only, plain C (`fx_page` also needs `sh4der_page.h` first). Each demo screen names the effects in use by the name the field knows them by, who does the work and when, as in `Ambient Occlusion  SH-4  Baked | Shadow Mapping  PVR  Realtime<<`. The rules are in the header: a label must be true, and "Shadow Mapping" only where a depth is compared against a map.

Calls: `fx_format` (one picture), `fx_format_split` (a screen cut in a left and a right half), `fx_format_page` and `fx_page` (a result page's label line), `fx_hide_chord` (the program's own hide button). The limits are not tuning numbers: `FX_COLS` 64 characters, `FX_LINES` 2 (3 on a page), `FX_MAX` 16 entries.

Each program's own table stays in the demo: `demo/psm/ao/ao_fx.h`, `demo/psm/hero/hero_fx.h`, `demo/pl4nar/src/pl4nar_fx.h`. Tests: `test/fxlabel_test.c` (the rules, in `make test`) and `demo/psm/test/fx_tables_test.c` (every program's table, in `make test-demo`), which fails at build time when a table does not fit.

Status: VID-016: the labels were drawn on every page of eight programs on three discs, off in every measured stage, clear of the graph titles (MEASURED, one console). Maturity ALPHA v0.4 (Scholar) / v0.5 (Parity). Wiki: `wiki/effects/fxlabel.md`. `demo/psm/psm_effects.h` is a forward at the old path.

## The QR result pages

`include/sh4der_qrpage.h`, header only. The QR test set (image psm-qr-q1) draws 16 codes of known payloads at versions 3 and 5 and 10, 8, 6 and 4 pixels a module, each on a light panel with the 4-module quiet zone the standard asks for, and puts the text beside them. Each payload is `q1 <id> <crc>|<data>`, so a decoded string checks itself.

The codes are made by Project Nayuki's QR Code generator library (C, MIT License). The library is not in this repository and this header does not vendor it: a client gets `qrcodegen.c` and `qrcodegen.h` from `demo/psm/tools/fetch.py` (`demo/psm/fetch_table.json`, sha256 checked, into `demo/psm/assets/_dl`), or any copy, and includes `qrcodegen.h` before `sh4der_page.h` and this header. This header calls only `qrcodegen_encodeBinary`, `qrcodegen_getSize` and `qrcodegen_getModule`.

Tests: `test/qrpage_test.c` runs in `make test-demo` and says NOT RUN when the encoder is not fetched; `tools/qr_check.py --matrices` then decodes its dump with a decoder that shares no code with the encoder. The image is built by `demo/psm/qr/build.py`.

Status: VID-014 in `wiki/hardware/ledger-video.md`: MEASURED, one console, one television, one phone, one run: 16 codes decoded from the photographs, each to its exact payload with its CRC right (OpenCV's detector read them). Maturity ALPHA v0.5. Wiki: `wiki/effects/qrpage.md`. `demo/psm/qr/qr_page.h` is a forward at the old path.

## The psm effect (perspective shadow maps)

`sh4der_psm.h`, header only; `demo/psm` is the software demo. The placement of a shadow map for a directional light, in two forms through one interface: a uniform (orthographic) map fitted to the receivers, and a perspective shadow map, the same fit made in the camera's post-projective space. Idea from the GPU Gems chapter "Perspective Shadow Maps: Care and Feeding": near ground is large on screen and needs more texels than far ground. The equations were checked against the chapter's printed text (NVIDIA's PDF) and are written here in this library's own code and row form. Where it differs: the fit axis is the mean direction (the chapter uses the minimal cone), and the bias is in metres (the chapter's `Ltexel` is a squared length). They are pinned in `test/psm_test.c` against the numbers its prose gives.

- **The warp.** World to post-projective space (`z' = Q (1 - Zn / z)`, `Q = Zf / (Zf - Zn)`), then a projective map through L' = (D, 0) x ViewProj, the light's place in that space, fitted off axis to a rectangle round the receivers' points. One 4 x 4 matrix: `u = x / w`, `v = y / w`.
- **Light kinds.** By the sign of L'.w: 0 is a direction (an orthographic map in the warped space), more is a sun ahead of the camera, less is a sun behind it. The map is the same projective line through L' either way. A caster's vertices can have either sign of w; a triangle with mixed signs crosses the singularity and is cut or dropped (`sh4der_psm_tri_ok`).
- **The warp's near plane** (`warp_near`, not the render's) is the cheapest fix and the one that matters: `Q` is about 1, so the first metre after a 1 m near plane takes half the depth range. Raise it to a little beyond the nearest receiver (4 m for 1.7 m eye height pitched down 7 degrees). With the render's 0.3 m the map degenerates for most suns; `test/psm_test.c` prints the cases.
- **Depth and bias.** The key is the distance along the light in metres (`sh4der_psm_depth`): linear in world position, so a perspective-correct interpolation in the map is exact and the world-space bias `a + b x texel` (`sh4der_psm_bias`, the texel's metres from `sh4der_psm_texel_m`) is the same everywhere.
- **Fit.** `sh4der_psm_frustum` (eight corners), `sh4der_psm_slab_pts` (the corners and edge crossings inside a height slab, up to 32 points), then `sh4der_psm_build_uniform` / `sh4der_psm_build_warp`. A build returns 0 for a degenerate frame (a cone of about 90 degrees, a point behind the camera): draw the uniform map instead.
- **Metrics.** `sh4der_psm_texel_m` (ground a texel covers) over `sh4der_psm_pixel_m` (ground a pixel covers): above 1 the shadow is magnified and shows blocks. `test/psm_test.c` and `demo/psm` print it near and far down the view.
- **The map in RAM.** `sh4der_psm_raster.h` fills the map and reads it on the SH-4: caster triangles drawn as 16-bit depth into a buffer the caller owns (128 x 128: 32 KB), clipped in homogeneous form on both sides of the light's singularity so nothing is dropped, and a compare per receiver vertex (`sh4der_psm_raster_lit`, four texels compared then blended) that gives 0 .. 1 for the vertex colour: the slot SH4DOW-DC's visibility and `sh4der_sss_rgb`'s `visibility` take. The PVR draws nothing extra and compares nothing. `sh4der_psm_raster_lit_plane` compares each texel read with the receiver surface's own depth at that texel's centre (a receiver-plane bias, not in the chapter): needed on a warped map, where one stored depth covers metres of ground along one axis. `test/psm_raster_test.c` checks every ground vertex of 18 scenes (four suns, uniform and warped, ground drawn or not); nothing here has run on a Dreamcast.
- **The PVR draws the map instead.** `sh4der_psm_pvr.h` is the SH-4's half of the other route, to be measured against the one above: the casters clipped into triangles for a render target (`sh4der_psm_pvr_tris`: map pixels and 1 / w, both signs of w, nothing the PVR cannot take), and each ground vertex's lookup coordinates (`sh4der_psm_pvr_uv`) for a multiply layer that reads the target per pixel. The PVR does the filling and the lookup; there is no depth compare, so no self-shadowing: casters and receivers are separate sets. A uniform map is affine in world space, so the PVR's interpolation of the vertex coordinates is exact; a perspective map is projective, so it is exact only at the vertices, and `sh4der_psm_pvr_uv_error` measures it in texels. On the host (`test/psm_pvr_test.c`, 256 map): a 4 m ground cell is off by up to 57 texels at 6 m ahead and 1.5 at 20 m, a 1 m cell by 1.1 and 0.07, a 0.25 m cell by 0.06 and 0.004: the ground wants fine cells near the camera and coarse ones far away. Against the exact compare a box's shadow differs at 0.02% to 0.3% of ground points. Host numbers only; nothing here has run on a Dreamcast.
- **Prior art, as far as we know.** We found no documented commercial Dreamcast game that used a depth shadow map (`demo/psm/PRIOR_ART.md` lists what was searched and what could not be read). Render-to-texture shadows are reported for Tony Hawk's Pro Skater 1 and 2 (an emulator team's 2018 note; how the shadow was drawn is not stated); most games used modifier volumes. We claim no first. What differs here: route B is a depth compare done on the SH-4, with self-shadowing, a moving sun and moving casters; route A is a silhouette drawn by the PVR into a render target. Real-console timings are still to be measured.
- **Not the PVR's.** The PVR has no depth texture and no per-pixel compare, so on it the map would be a caster silhouette; and it has no q texture coordinate, so a per-vertex lookup is exact only where the map is affine in world space (a uniform map). Software has neither limit.
- **Cost.** RAM 80 B for a map description (matrix, light, kind); VRAM 0. CPU: 4 x 4 products at build; a lookup is a 4 x 3 transform and a divide.

### Calls

```c
int  sh4der_psm_build_uniform(sh4der_psm_t *out, const float light[3], const float *pts, unsigned n);
int  sh4der_psm_build_warp(sh4der_psm_t *out, const sh4der_psm_cam_t *cam, float warp_near, const float light[3],
                           const float *pts, unsigned n);
int  sh4der_psm_map(const sh4der_psm_t *p, const float x[3], float *u, float *v, float *w);
```

`light` is the unit direction from the surface toward the light; `pts` are n float[3] receiver points inside the frustum. `test/psm_test.c`: the chapter's depth numbers (Q 1.0002 at 1 and 4000, 1.02 at a ratio of 50, half the cube in the first metre, the inverse projection's 0, 1/2, 1), the camera rows, the light kinds, every fitted point inside the map, two points on one light ray on one texel (both signs of w, both sides of the light), the slab clip, the footprint ratio at four sun angles, bad input; both maths back ends.

### Psm tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_PSM_MAX_PTS | 64 | Most points a fit takes (a slab clip of a frustum gives 32) |
| SH4DER_PSM_W_EPS | 0.0001 | \|L'.w\| under this share of \|L'.xyz\| makes the light a direction in the warped space |
| SH4DER_PSM_MIN_COS | 0.02 | Least cosine between the cone axis and a point's direction (about 89 degrees) |
| SH4DER_PSM_MARGIN | 1.01 | The fit's rectangle, a little larger than the points |
| SH4DER_PSM_TRI_EPS | 0.000001 | \|w\| under this is the singularity: a vertex there is not drawn |
| SH4DER_PSM_BIAS_A | 0.02 | Bias a, metres |
| SH4DER_PSM_BIAS_B | 1.5 | Bias b, texels (times the texel's size in metres) |
| SH4DER_PSM_RASTER_SIZE_MAX | 1024 | Largest map side, texels |
| SH4DER_PSM_RASTER_EDGE_EPS | 0.001 | A texel centre this far outside an edge is still covered (no cracks between a clipped triangle's parts) |
| SH4DER_PSM_RASTER_DELTA | 0.02 | Step of the texel size measure, metres |
| SH4DER_PSM_RASTER_TEXEL_MAX | 5 | Largest texel size the bias follows, metres |
| SH4DER_PSM_RASTER_SLOPE_MAX | 8 | Most depth a surface's slope may add across a texel in the plane compare, metres |
| SH4DER_PSM_RASTER_STEP | 0 | 0: the running-add row fill (every target). 1: each row through sh4zam-dce's `shz_dce_tri_row_step` (needs SH4ZAM; its SH-4 form is not bit-identical and is not faster on covered texels, see KERNEL_GUIDE.md) |
| SH4DER_PSM_RASTER_IW_MIN | 1e-18 | Smallest 1 / w that counts as a depth (the reciprocal is FSRRA of its square, which flushes to zero below about 1.1e-19) |
| SH4DER_PSM_HOLD_GROW | 1.10 | A held fit is taken this much larger than it needs when it is made again |
| SH4DER_PSM_HOLD_BAND | 1.30 | A held fit is kept until it needs more, or this many times less |
| SH4DER_PSM_HOLD_PAD | 2.0 | Texels of room each side of a held fit for the centre's snap |
| SH4DER_PSM_PVR_MAX_TRIS | 10 | Most triangles one world triangle gives (two parts of up to 5 each) |
| SH4DER_PSM_PVR_UV_GRID | 5 | The error probe: points a side of the barycentric grid |

### The shadow demo's small world (the closed square)

A walled square needs no world past its walls. The shadow demo (`demo/psm`) can be built as the **closed square**: the same square with its two gaps shut by gates. It then draws only the ground inside the walls. PROTOTYPE, host tested (`demo/psm/test/psm_bound_test.c`, in `make test`), NOT RUN on a console. Its A/B is `turncost-t5-keep` against `turncost-t6-bound`.

- **The bake.** `make ... SQUARE=closed` bakes it with `demo/psm/tools/bake_square.py --closed` into `$(BUILD)/psm_closed`.
  - Gates (a door in the reveal, also a caster) close the north gate and the south street. The walk ends inside the south wall, and the porter comes and goes by the gates (`psm_town.h` under `PSM_TOWN_CLOSED`).
  - Faces that no eye in the walk can see are left out: up faces above the eye on the solid pieces, the solid pieces' far sides, and parts outside the walls that sit under every sight line over them. The first two rules apply only to faces whose outward normal was given, because the PVR's culling is off.
  - Every caster is kept: the keep and the towers shade the square.
  - The header gets `PSM_TOWN_CLOSED` and the seen ground `PSM_TOWN_BOUND_X0` .. `PSM_TOWN_BOUND_ZS1` (story metres, the walls' outer faces).
- **The cut.** `PSM_WORLD_BOUND` (`demo/psm/psm_scene.h`, `psm_frame_t.world_opt`; only on a scene with `bound_ok`).
  - The ground rings end where the window's farthest ray leaves the bound, instead of at `PSM_ROW_END`.
  - A cell that cannot touch the bound is never placed or sent. A point no kept cell uses is not placed, lit or compared.
  - The uniform shadow map is fitted to the bound over the route's slab, instead of to the view out to `PSM_FAR`. The perspective (warp) map keeps the view's fit.
  - The program draws with a little table fog at `PSM_BOUND_DIST_I`.
- **What it saves** (DERIVED: host counts over 2,016 poses, not console times):

| Item | Open square | Closed square, cut |
|---|---|---|
| Static screen triangles / vertices | 6,097 / 8,011 | 5,301 / 6,800 |
| Ground rings a frame | 97 | 46.1 mean, 67 at most |
| Ground points placed a frame | 5,400 mean | 1,943 mean, 3,608 at most |
| Ground cells sent a frame (2 triangles each) | 3,948 mean | 1,185 mean, 2,610 at most |
| Shadow texel on the ground ahead | 1 | 0.46 mean, 0.57 at most |
| Caster triangles | 1,686 | 1,764 (the gates) |

| Name | Default | Meaning |
|---|---|---|
| `PSM_WORLD_BOUND` | 1 | The bit in `world_opt` that turns the cut on |
| `PSM_WORLD_DEFAULT` | 0 | `psm_dc.c`: the world options at load (0: off) |
| `PSM_BOUND_DIST_I` | 10 | DRAW DISTANCE with the cut: `psm_dist_m[10]` = 75 m, the fog quadratic from 0.43 of it (32 m) |
| `VC_BOUND` | 512 | The view cost test's stage option for the cut and its fog (`demo/psm/turncost/build.py --bound`) |

In the picture: the gates are shut, there is a little fog on the far walls and the keep, and the ground shadows are finer. The ground, its occlusion and its colours inside the walls are the same lattice as before.

### The shadow demo's still camera and cold code (the turn plan's step 4)

Two more switches in the shadow demo, both off by default. PROTOTYPE, host tested, NOT RUN on a console. Their A/B is `turncost-t6-bound` against `turncost-t7-gv` (the seen marks kept and the ground option) and `turncost-t7-gvc` (the same and the cold code).

- **The seen marks kept** (`PSM_PLACE_VIS`, a `place_opt` bit of `demo/psm/psm_scene.h`). The placement marks which model pieces the view sees: it clears the list and tests every piece. With the camera still, those marks cannot change, so the placement keeps them and tests only the moving pieces. It does this only when the piece count and the moving lists are the same as at the last full test. `demo/psm/test/psm_keep_test.c` checks it against the full test: 0 steps differ in 240, and the marks are kept on 120.
- **The cold code** (`PSM_COLD`, a build flag). The code that runs at load or only when the text changes is marked cold: the scenes' set-up, the HUD's lines, and in `psm_dc.c` the load, the bake, the textures, the scene switch, the text and the result pages. The compiler then puts it in `.text.unlikely`, away from the frame's code. The SH-4's 8 KB instruction cache is direct mapped, so a big function between two hot ones pushes them onto the same lines. This only moves code: it is the same code.

| Name | Default | Meaning |
|---|---|---|
| `PSM_PLACE_VIS` | 16 | The `place_opt` bit that keeps the seen marks on a still camera |
| `PSM_COLD` | 0 | 1 marks the load, text and page code cold (`demo/psm/turncost/build.py --cold`) |
| `VC_VIS` | 1024 | The view cost test's stage option for `PSM_PLACE_VIS` (`build.py --vis`) |

## The bolt effect (branching strands that re-roll)

`include/sh4der_bolt.h` (the state and the quad maths, inline, host tested) and `src/sh4der_bolt.c` (the KallistiOS draw). Thin jagged strands run from a root along a direction, each with a few forks, and the whole pattern is drawn again a few times a second while its first half stays put. Lightning, an arc, a spark gap, a discharge, cracks of light, a lit root system. The caller gives roots, directions and lengths (world units) and the colours; nothing here knows a ship or a sky.

- **Strand.** A polyline of `segs` segments from the root. Each step goes along the direction with a seeded sideways wander (`wander` of the segment length on two axes) and a length jitter. State is offsets from the root, so a moving root drags the bolt without a re-roll.
- **Forks.** `forks` a strand, each of `fork_segs` segments, leaving a main point at an angle of `fork_lo` .. `fork_hi` radians. A fork is dimmer (`fork_gain`) and has no halo.
- **Re-roll.** Every `reroll_s` (4 pictures at 30 a second) the strand is drawn again with a new gain of 1 +- `gain_jitter`. The first `keep_segs` segments and every fork that leaves them stay; the rest and the other forks are new. A long pause is one re-roll, never a burst. The seed fixes the whole sequence.
- **Width.** Constant on the screen: `width_px` pixels at any distance (`width = width_px x z / focal_px`), the halo `halo_scale` times wider at `halo_gain` of the brightness, on the main strands only.
- **Draw.** Untextured quads through `sh4der_spark_quad`, translucent list, ONE / ONE, depth test on, depth write off, no culling. Quads add, so the order does not matter. Both guards of `sh4der_guard.h`.
- **Cost (default look: 4 strands, 6 segments, 1 fork of 3, 8 visible branches).** RAM: the state, about 3.3 KB (8 strand maximum), the caller's; one polygon header, 32 B. VRAM: 0. AICA: 0. TA a frame: one header + 4 vertices a quad, 36 core quads + 24 halo quads = 120 triangles, 240 vertices, 7.7 KB. SH4ZAM for all maths; `SH4DER_CORE_NO_SH4ZAM` for plain C.

```c
void     sh4der_bolt_params_default(sh4der_bolt_params_t *p);
int      sh4der_bolt_params_valid(const sh4der_bolt_params_t *p);
int      sh4der_bolt_init(sh4der_bolt_t *b, const sh4der_bolt_params_t *p, const sh4der_bolt_anchor_t *anchors, uint32_t seed);
void     sh4der_bolt_update(sh4der_bolt_t *b, const sh4der_bolt_params_t *p, const sh4der_bolt_anchor_t *anchors, float dt);
unsigned sh4der_bolt_segments(const sh4der_bolt_params_t *p);
int      sh4der_bolt_quad(const sh4der_bolt_t *b, const sh4der_bolt_params_t *p, const sh4der_bolt_anchor_t *anchors,
                          unsigned k, int halo, const sh4der_spark_view_t *view, sh4der_spark_vtx_t out[4]);
int      sh4der_bolt_init_draw(void);                                                          /* Dreamcast */
unsigned sh4der_bolt_draw(const sh4der_bolt_t *b, const sh4der_bolt_params_t *p, const sh4der_bolt_anchor_t *anchors,
                          const sh4der_spark_view_t *view, const sh4der_spark_screen_t *screen);   /* Dreamcast, inside the TR list */
```

`test/bolt_test.c`: defaults and bad params, seed determinism, strand shape and forks, segment count and chaining, the re-roll clock and what a re-roll keeps (the kept share of the bolt, .4 - .8), the gain range, quad width in pixels at two distances, the halo, fork dimming, the near plane, and no NaN over 100 re-rolls.

### Bolt tuning

The default bolt values, from `sh4der_bolt_params_default`, and the sizes of the state.

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_BOLT_STRANDS_MAX | 8 | Strands a bolt holds |
| SH4DER_BOLT_SEGS_MAX | 12 | Segments of a main polyline |
| SH4DER_BOLT_FORKS_MAX | 3 | Forks a strand holds |
| SH4DER_BOLT_FORK_SEGS_MAX | 6 | Segments of a fork |
| SH4DER_BOLT_STRANDS | 4 | Default strands (a visible branch count of 4 - 8 with the forks) |
| SH4DER_BOLT_SEGS | 6 | Default main segments |
| SH4DER_BOLT_FORKS | 1 | Default forks a strand |
| SH4DER_BOLT_FORK_SEGS | 3 | Default fork segments |
| SH4DER_BOLT_KEEP_SEGS | 3 | Main segments a re-roll keeps (half of a strand) |
| SH4DER_BOLT_WANDER | 0.45 | Sideways step as a share of the segment length, each of the two sideways axes |
| SH4DER_BOLT_SEG_JITTER | 0.25 | Segment length varies by +- this share |
| SH4DER_BOLT_FORK_LEN | 0.35 | Fork length as a share of the strand length |
| SH4DER_BOLT_FORK_LO | 0.50 | Least fork angle from the strand, radians (about 29 degrees) |
| SH4DER_BOLT_FORK_HI | 0.90 | Largest fork angle, radians (about 52 degrees) |
| SH4DER_BOLT_WIDTH_PX | 2.0 | Strand width on the screen, pixels |
| SH4DER_BOLT_HALO_SCALE | 3.0 | Halo width as a multiple of the strand width |
| SH4DER_BOLT_HALO_GAIN | 0.35 | Halo brightness as a share of the strand's |
| SH4DER_BOLT_FORK_GAIN | 0.70 | Fork brightness as a share of the main strand's |
| SH4DER_BOLT_REROLL_S | 0.1333 | Seconds between re-rolls: 4 pictures at 30 a second |
| SH4DER_BOLT_GAIN_JITTER | 0.25 | Brightness of a re-roll is 1 +- this |
| SH4DER_BOLT_CORE_ARGB | 0xFFFFEBF0 | Peak colour of the core strand (255, 235, 240) |
| SH4DER_BOLT_HALO_ARGB | 0xFFFF968C | Peak colour of the halo (255, 150, 140) |

## The motes effect (a steady field of drifting specks)

`include/sh4der_motes.h` (the field maths and the sprite quads, inline, host tested) and `src/sh4der_motes.c` (the KallistiOS draw). Embers, dust, pollen, ash, snow flecks: small square sprites born near an anchor in the world, drifting and falling, fading as they age and born again, drawn as hard 1-bit dots. Nothing is stored per speck: every value comes from (slot number, time), so the field costs no memory and never needs an update. The caller gives emitters (a position and a seed) and one params row; nothing here knows a game.

- **Stateless rule.** An emitter owns `ceil(rate x life_hi)` slots (210 at 30 a second and 7 s). Slot i has its own life between `life_lo` and `life_hi` and its own phase; at time t it is `age` seconds into generation `gen`. Every pick (start box, velocity, acceleration, size) is an integer hash of (slot, generation, salt), so a slot starts somewhere new each loop and the field does not visibly repeat. Position is start + v x age + a x age^2 / 2.
- **Curves.** Alpha rises 0 -> 1 over `alpha_in` of the life; the mask opens as alpha passes `clip` (the dot grows from nothing, no pop, no blending); colour holds to `col_hold`, falls to the fade colour at `col_end`; a speck dimmer than `cull_k` is not drawn (40 % of the quads on the default look).
- **On screen.** Side = size x focal / depth pixels, held between `min_px` and `max_px`; under `min_px` the sprite is `min_px` wide with brightness x (true / min_px)^2 (the same light in fewer pixels); over `big_px` it is dimmed to `big_k`. An emitter past `lod_cut` is not drawn. The `max_draw` brightest (brightness x screen area) are kept; `max_draw` defaults to 96 and may be 1 .. 128 (`SH4DER_MOTES_DRAW_MAX`).
- **Draw.** Punch-through list over one 16 x 16 round mask (`sh4der_motes_mask_make`, 512 B VRAM, no filtering, depth write on), or untextured in the opaque list for sprites of `flat_px` or less (0 B). Both guards of `sh4der_guard.h`.
- **Cost (default look, three emitters, two in range).** RAM: no state; the caller's collect buffer is 32 B x `max_draw` (3,072 B at 96); params 136 B; one emitter 16 B; two polygon headers 64 B. VRAM: 512 B (mask, optional). AICA: 0. TA a frame: one header + 4 vertices a sprite, at most 96 sprites: 12.3 KB. CPU: one hash set and one position per live slot, SH4ZAM for all maths.

```c
void     sh4der_motes_params_default(sh4der_motes_params_t *p);
int      sh4der_motes_params_valid(const sh4der_motes_params_t *p);
unsigned sh4der_motes_slots(const sh4der_motes_params_t *p);
int      sh4der_motes_slot(const sh4der_motes_params_t *p, const sh4der_motes_emitter_t *e, unsigned slot, float t, sh4der_motes_ember_t *out);
unsigned sh4der_motes_collect(const sh4der_motes_params_t *p, const sh4der_motes_emitter_t *em, unsigned n_em, float t,
                              const sh4der_spark_view_t *view, const sh4der_spark_screen_t *screen, sh4der_motes_draw_t out[SH4DER_MOTES_DRAW_MAX]);
int      sh4der_motes_quad(const sh4der_motes_draw_t *d, sh4der_spark_vtx_t out[4]);
void     sh4der_motes_mask_make(uint16_t out[256]);
int      sh4der_motes_init_masked(void *texture, int txr_format);                                 /* Dreamcast */
int      sh4der_motes_init_flat(void);                                                           /* Dreamcast */
unsigned sh4der_motes_draw_masked(const sh4der_motes_draw_t *d, unsigned n, int all, const sh4der_spark_view_t *view,
                                  const sh4der_spark_screen_t *screen);                           /* Dreamcast, punch-through list */
unsigned sh4der_motes_draw_flat(const sh4der_motes_draw_t *d, unsigned n, const sh4der_spark_view_t *view,
                                const sh4der_spark_screen_t *screen);                             /* Dreamcast, opaque list */
```

`test/motes_test.c`: defaults and bad params, the hash (range, spread, salts differ), the stateless rule (same slot and time, same speck; a new generation starts elsewhere), the curves (opens at clip x alpha_in, colour hold and fade, nothing drawn when dark), the live count (85 - 130 of 210), screen side limits, the vertex colour, the quad corners, the cap keeps the brightest, lod cut, behind the eye, bad arguments, the round mask, no NaN over 200 frames. Both maths back ends.

### Motes tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_MOTES_SLOTS_MAX | 256 | Slots one emitter holds (rate x life_hi, rounded up, at most this) |
| SH4DER_MOTES_DRAW_MAX | 128 | Quads one collect call can hold (params.max_draw is at most this) |
| SH4DER_MOTES_MASK_SIDE | 16 | Side of the round mask texture, texels |

## The deform effect (ground that keeps a dent)

`include/sh4der_deform.h` (all inline, host tested; no `.c`: it draws nothing). The ground is a mesh the caller already owns; this module holds one byte of height a cell under it and answers "how high is the ground here?" for every vertex.

- **Cells.** 128 is the untouched surface (`ceil_y`), 0 the hard bottom (`floor_y`), values above 128 the raised rim; one step is (ceil - floor) / 128. A 160 x 160 window at 0.25 m a cell covers 40 m and is 25,600 B.
- **Stamp.** `sh4der_deform_stamp(x, z, bottom_y, radius, amount)` once a frame for each thing that presses (a foot bone, a capsule's bottom, a wheel). Under the source the ground goes to a bowl, `bottom_y` at the centre and `radius / 2` higher at the edge, never under the hard bottom; `amount` eases it in (1 = at once). It only presses down, so a lifted foot leaves its dent. From the edge out to 1.6 radii a rim rises, `rim` (default 0.25) of the dent's depth; a rim never fills an older dent.
- **Keep or close.** `refill` is steps a second back toward the surface: 0 for snow (dents stay), a few for mud. Fractions carry over, so a slow refill is exact over time.
- **Window.** `sh4der_deform_recentre(x, z)` once a frame with the camera or player: the window moves in whole cells and the storage wraps (no copy); cells that enter are fresh, a dent that leaves is gone.
- **Read.** `sh4der_deform_height` (blended between four cells; the plain surface outside the window, so a larger mesh joins without a step), `sh4der_deform_depth` (0..1, to darken a vertex) and `sh4der_deform_slope` (rise per unit along x and z; the normal is (-sx, 1, -sz)).
- **Sharpness.** A dent is no sharper than the mesh that shows it. For a crisp footprint on a coarse mesh use the print list: `sh4der_deform_prints_add / _tick / _quad` keep the last 32 prints and give each one's quad (four corners, texture coordinates, an alpha that fades at the end of its life) to draw as a blended decal just above the ground. The list also stands alone where the grid is not used.
- **Not measured:** the cost on a console. The header's figures are counts (bytes and loads), not times.

`test/deform_test.c`: defaults and bad arguments, the bowl and its hard bottom, the rim and its limit, ease-in that arrives, no lift by a later stamp, the height blend, depth, slope, refill with carry, the window (dents kept, dropped, a jump past its size, negative coordinates), the print list (size, direction, fade, replacement, triangle area).

### Deform tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_DEFORM_LEVEL | 128 | Cell value of the undisturbed surface (0 = the hard bottom, 255 = the highest rim) |
| SH4DER_DEFORM_GRID_W | 160 | Default window width in cells (40 m at the default cell) |
| SH4DER_DEFORM_GRID_H | 160 | Default window depth in cells |
| SH4DER_DEFORM_CELL | 0.25 | Default cell size, world units (metres) |
| SH4DER_DEFORM_RIM_REACH | 1.6 | The rim ends at this many stamp radii from the stamp's centre |
| SH4DER_DEFORM_RIM | 0.25 | Default rim height as a share of the dent's depth (0 = no rim) |
| SH4DER_DEFORM_PRINTS | 32 | Prints the decal list keeps; the oldest is replaced |

## The mist effect (a bank of slices)

`include/sh4der_mist.h` (all inline, host tested; no `.c`: the caller projects and submits). Table fog tints what is far away but has no shape; this is the shape: a bank lying in a street with clear air above it.

- **The bank.** `slices` upright sheets (default 12, at most 16) spaced evenly through `depth` along a direction in the ground plane, each `2 x half_width` wide, standing from `base_y` to `top_y`. The PVR's translucent sort orders the sheets with the scene, so a figure in the bank is hidden by the sheets in front of it.
- **Soft top.** A sheet is three rows of vertices: base and feather line carry the slice's alpha, the top row 0, so the top `feather` metres fade out. A feather taller than the bank is the whole height; 0 or less is a hard top.
- **Opacity.** `opacity` is the share of light the whole bank stops; `sh4der_mist_slice_alpha` gives the alpha one sheet needs for that (within 0.01 of the exact n-th root for 4 to 16 sheets and opacity up to 0.9).
- **Movement.** Each sheet's texture slides sideways at its own speed (half of them the other way) and wobbles up and down, both hashed from the slice number and `seed`. There is no state: change the block's numbers over time to make the bank grow, thin or roll in.
- **Near the eye.** A sheet fades over `near_fade` metres as the eye approaches it and is not drawn once the eye is level with it, so walking through the bank never shows a hard plane.
- **Noise.** `sh4der_mist_noise` fills a tiling cloud pattern, one byte a texel, once at load; pack it as the alpha of a 16-bit texture.
- **Draw.** `sh4der_mist_slice` gives six vertices in strip order (4 triangles): project them, run `sh4der_tri_area_ok`, submit with blend source alpha / 1 - source alpha, depth test on, depth write off, no culling.
- **Fill is the cost and is not measured.** Every sheet covers its whole area on screen. Keep the bank low and partial (a band, not a wall across the view) or use fewer sheets.

`test/mist_test.c`: defaults, the per-slice alpha against the exact root, a slice's vertices (place, rows, alpha, triangle areas), even spacing, the eye inside the bank, drift with time and seed, a turned bank, feather limits, bad arguments, the noise (range, seed, tiling).

### Mist tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_MIST_MAX_SLICES | 16 | Most slices a bank may have |
| SH4DER_MIST_SLICES | 12 | Default slices |
| SH4DER_MIST_OPACITY | 0.7 | Default share of the light the whole bank stops, looking through all of it |
| SH4DER_MIST_FEATHER | 2.0 | Default height of the soft top, world units (metres) |
| SH4DER_MIST_NEAR_FADE | 3.0 | Default distance at which a slice starts to fade as the eye nears it, world units |
| SH4DER_MIST_DRIFT | 0.02 | Default sideways slide of the texture, texture widths a second |
| SH4DER_MIST_SEETHE | 0.03 | Default up-and-down wobble of the texture, texture heights |
| SH4DER_MIST_SEETHE_RATE | 0.4 | Default wobble speed, radians a second |
| SH4DER_MIST_ALPHA_ROUNDS | 5 | Newton rounds sh4der_mist_slice_alpha takes (each at most 16 multiplies and a divide) |
| SH4DER_MIST_TILE | 8.0 | Default world units one texture width covers across a slice |

## The rise effect (lights that lift off and become stars)

`include/sh4der_rise.h`, header only; the effect table above says what it does. `test/rise_test.c` is its test.

### Rise tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_RISE_TWINKLE_IN | 1.0 | Seconds over which the twinkle depth fades in after arrival (so brightness has no jump) |

## The film effect (the wear of old film)

`include/sh4der_film.h`, header only; the effect table above says what it does. `test/film_test.c` is its test.

### Film tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_FILM_SCRATCH_SLOTS | 3 | Scratches that can be alive at once (one a slot) |
| SH4DER_FILM_SPECKS_MAX | 12 | Most specks one tick can have |

## The timebase effect (a video tape's bent picture)

`include/sh4der_timebase.h`, header only; the effect table above says what it does. `test/timebase_test.c` is its test: the
module against a second, double precision transcription of the model's formulas, at the start of play and five hours in.

The model is a port to C of parts of NTSCRT by Finn McKenty (MIT License, Copyright (c) 2026 Finn McKenty; the notice is in
the header and stays with it). A program that uses this module credits NTSCRT and its author beside SH4DER-PVR.

Order, a displayed frame: `sh4der_timebase_field(&deck, &state, field, shift_px)` gives one sideways shift in pixels for every
row of the picture. Pick the mesh rows you want (every sixth picture row is enough for a smooth hook), put their screen heights
and shifts in two tables, and draw the frame texture with `sh4der_ca_channel_rows` (`sh4der_ca.h`) in place of
`sh4der_ca_channel`. Two table rows at the same height are a break: a step in the picture, as the head switch makes. Give it
an overscan as large as the largest shift, so the bend uncovers nothing at the picture's edge.

Nothing of it is measured on a console: the costs are counts. `sh4der_timebase_field` keeps about 4.2 KB on the stack.

### Timebase tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_TIMEBASE_LINE_US | 63.556 | Length of one line, microseconds |
| SH4DER_TIMEBASE_ACTIVE_US | 52.656 | The part of a line that carries picture, microseconds: the picture's width in time |
| SH4DER_TIMEBASE_FIELD_RATE_N | 60000 | Fields a second as a fraction: 60000 / 1001 (59.94) |
| SH4DER_TIMEBASE_FIELD_RATE_D | 1001 | And its divisor |
| SH4DER_TIMEBASE_LINES_NTSC | 262.5 | Lines of one field of the real signal |
| SH4DER_TIMEBASE_ROWS_MAX | 480 | Most rows (active lines) the module simulates |
| SH4DER_TIMEBASE_VBI_MIN | 12 | Fewest blanking lines above the picture a small raster gets |
| SH4DER_TIMEBASE_LINES_MAX | 520 | Most lines a field then has (rows and blanking): SH4DER_TIMEBASE_ROWS_MAX + 40 |
| SH4DER_TIMEBASE_HEAD_SWITCH_MAX_US | 8.0 | Largest head switching step, microseconds (the model's control range) |
| SH4DER_TIMEBASE_FLUTTER_MAX | 5.0 | Largest flutter amount (1 is a worn deck at its realistic worst) |
| SH4DER_TIMEBASE_STRAIN | 8.0e-4 | Tape stretch at tension 1: a 13 microsecond jump at the head switch |
| SH4DER_TIMEBASE_SEED_LO | 0x53435254 | The model's default seed, low word |
| SH4DER_TIMEBASE_SEED_HI | 0x00004e54 | And its high word |
| SH4DER_TIMEBASE_TAU | 6.2831853 | 2 pi |

## The hexheat effect (heat haze as a lattice of hexagons)

`include/sh4der_hexheat.h`, header only, maths alone; the effect table above says what it does. The design and its formulas
are the owner's. `test/hexheat_test.c` is its test, against a double-precision transcription of those formulas written in
world units (the header works in pixels): the lattice's homes, the ray to the ground and back to the screen, the haze's
point, the field against the numeric derivative, every drawn hexagon's offset, the largest offset and the change a frame.

**The lattice is the screen's.** Pointy-top hexagons of size s (centre to corner, pixels). The home of cell (q, r) is
x = s sqrt(3) (q + r / 2), y = 1.5 s r, from the two integers alone (`sh4der_hexheat_home`), so a home cannot move. Each
frame a hexagon is drawn at home + offset(t). Nothing is added to last frame's place and nothing is rolled at random: the
only "random" numbers are four phases in a table.

**Every number of the physics comes from the world.** A screen point knows nothing of the world, so for each cell a ray goes
from the camera through the cell's home and is met with the ground: `sh4der_hexheat_screen_to_ground`, the same ray-to-ground
step a strategy game uses to find where the mouse points. That gives the world point and its distance D. A ray that lands
beyond the haze, or never lands (the cells over the road's far line), takes its point where it crosses an upright sheet
across the ground at the haze's distance; h is the ray's height over the ground there (`sh4der_hexheat_screen_to_haze`).
Seen from a car's camera the ground between a 64 m cut and the sheet at 70 m is a strip about 2 pixels high, so nearly every cell is on the sheet.

**The field** is a sum of N waves in the world, X across the view and Y up, in metres:
theta_i = k_i (X - U t) + m_i (Y - W t) + phi_i, each layer twice as fine and half as strong as the one before. The offset is
minus A times the gradient of the sum: dX = -A sum a_i k_i cos(theta_i), dY = -A sum a_i m_i cos(theta_i). The envelope is
A = A0 exp(-h / H) (1 - exp(-D / D0)): strongest at the road's far line, fading with height. A0 is tuned so that the largest
offset is `SH4DER_HEXHEAT_PEAK` x s at the sheet's foot. A hexagon also swells and shrinks a little (the lensing scale). The
world offset goes back to pixels as a bend of the line of sight: offset / depth x the focal length. A hexagon keeps one size
on the screen (world-sized hexagons on homes that do not move could not tile), and no offset passes
`SH4DER_HEXHEAT_OFFSET_MAX` x s (2.56 pixels at the defaults), so a hexagon's middle stays in its own cell.

**What the doubling rule gives: stripes.** k and m double together, so every layer runs along one line, (k_1, m_1). The field
is stripes across that line, and every hexagon slides back and forth along it: at the starting values the diagonal, up to
the right and down to the left. `SH4DER_HEXHEAT_WAVE_HEX_UP` turns the line; it does not end the stripes. A layer with a
direction of its own would, and that is not in the owner's formulas, so it is not in the header.
`SH4DER_HEXHEAT_WAVE_HEX` is the wave along the screen's x (and `_UP` along its y); with both at 7 the crests are 7 / sqrt(2) =
4.95 hexagon widths apart (68.6 pixels, 10.2 m in the street demo's view), the middle layer 2.47 and the finest 1.24. So along
a stripe neighbours move together, and across the stripes they do not: two hexagons that share an edge overlap by up to
1.7 pixels and part by up to 2.4 (host, the street demo's view; where they part, the frame under them shows). With 4 layers
the finest wave is 0.62 widths, shorter than a hexagon.

**The camera.** `sh4der_hexheat_follow` puts what the camera did since the frame before into the field's scroll: a turn takes
the pattern sideways with the far picture, and the way forward streams it toward the camera (`SH4DER_HEXHEAT_DRIVE`: 1 is the
field standing in the world; the default is half of that). The stripes cross the view at W + U k_1 / m_1 (28 pixels a second
at the defaults) and the stream takes `SH4DER_HEXHEAT_DRIVE` x the speed x the view's pixels a metre off that. So a slow
camera sees them climb, a fast one sees them run down, and at one speed they stand still and every offset holds: 8.3 m/s
(30 km/h) in the street demo's view, pinned by `test/hexheat_test.c`. A frame's step is also held at a quarter of the finest
layer's wave, so a large step never reads as a small one the other way. A turn's slip has no such hold and no fade: at
0.5 rad/s the offsets change by up to 0.9 pixels a frame at 60 frames a second (0.07 on a straight; host), and a client may
fade the gate in a turn. Worked out, not measured: with the street demo's
view (6.75 pixels a metre at the sheet) and 30 frames a second that hold starts at about 54 m/s (27 m/s with a drive of 1).

Order, a frame:

1. `sh4der_hexheat_view_look` (eye, target, up, the field of view, the screen), then `sh4der_hexheat_view_ground` (the
   ground's plane, the haze sheet's distance, the near cut). On hills give the plane through the sheet's foot (the road's
   height there). The eye may be under that plane on a climb: then no ray lands, every cell is on the sheet, and the haze
   stays on.
2. `sh4der_hexheat_follow` with the `g` and `eye` of the frame before's view (not on the first frame).
3. `sh4der_hexheat_band(ax, ay, &x0, &y0)` with the sheet's foot on the screen. In a render pass that already runs, draw the
   far world as this frame's camera sees it into a 256 x 128 region of the target, a texel a screen pixel, cut to that
   region and to the near cut (the street demo cuts on the SH-4 in its gfx layer, `gfx_window`). The ground itself is not cut
   by depth: a hexagon over the cut's line carries the ground under it. Everything the screen shows beyond the cut must
   be in the band, translucent layers too (a far shadow, smoke, a light's glow): a hexagon replaces what is under it with
   the band. The street demo draws the opaque world and the player's car, and fades the road's baked occlusion out short
   of the cut while a band is drawn.
4. `sh4der_hexheat_mesh(&h, &view, seconds, gate, x0, y0, out, max, &peak)`. Draw each hexagon as a strip of its 6 vertices
   on the translucent list: no blend (one / zero), filtered (bilinear), the band's texel at (u, v) plus the band's place in
   the target, depth tested at the near cut's depth and not written. Send them after the far translucent polygons and
   before every nearer one (with the PVR's autosort on, anywhere in the list). The offsets are 0 at the near cut's line,
   or at the band's lowest home when that line is under it, so no cell comes or goes at the bottom with an offset.

Nothing of it is measured on a console: the costs are counts. Open on a console: whether a hexagon on its home is the
picture under it. Two things stand against it: the filtered sample at a texel a pixel, and the packing, since the band is
packed and dithered into the 16-bit target and then again onto the screen (the lattice may show as a lighter patch even at
rest). The test: a build with `-DSH4DER_HEXHEAT_TEST_STILL` (the same cells drawn with no offset; never a build for players),
once as built and once with the frame buffer's dither off.

### Hexheat tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_HEXHEAT_S | 8.0 | s: a hexagon's centre to corner, screen pixels (13.9 wide, 16 high) |
| SH4DER_HEXHEAT_LAYERS | 3 | N: the stacked waves (3 or 4; with 4 the finest wave is shorter than a hexagon) |
| SH4DER_HEXHEAT_WAVE_HEX | 7.0 | The longest wave along the screen's x, hexagon widths (6 to 8): k_1. With the same upward, crest to crest is 4.95 |
| SH4DER_HEXHEAT_WAVE_HEX_UP | 7.0 | The longest wave upward, hexagon widths: m_1 (the owner's start: the same as across) |
| SH4DER_HEXHEAT_RISE_HEX | 1.5 | W: the field's rise, hexagon widths a second (1 to 2) |
| SH4DER_HEXHEAT_WIND | 0.35 | U as a share of W, sideways (0.2 to 0.5) |
| SH4DER_HEXHEAT_LAYER_FRAC | 0.125 | H: the haze layer's thickness, a share of the screen's height seen at the sheet (0.10 to 0.15) |
| SH4DER_HEXHEAT_D0 | 30.0 | D0: metres from the camera where the haze becomes plain (keep it under about 4 x the near cut on the SH4ZAM path) |
| SH4DER_HEXHEAT_PEAK | 0.25 | The largest offset at the sheet's foot (the road's far line), in s. PEAK x s must be at least 4 x CUT pixels |
| SH4DER_HEXHEAT_LENS | 0.1 | c: the swell and shrink (0: none) |
| SH4DER_HEXHEAT_DRIVE | 0.5 | The camera's travel in the scroll: 1 = the field stands in the world, 0 = no streaming |
| SH4DER_HEXHEAT_OFFSET_MAX | 0.32 | The largest offset on the screen, in s (2.56 pixels; under a third: a hexagon's middle stays in its own cell) |
| SH4DER_HEXHEAT_NEAR_FADE | 0.5 | In s: the offset grows from 0 at the near cut's line to whole this far above it |
| SH4DER_HEXHEAT_EDGE | 24.0 | Pixels: the offset grows from 0 over this from the band's left, right and top |
| SH4DER_HEXHEAT_CUT | 0.25 | Pixels: a cell whose largest offset is under this is not drawn |
| SH4DER_HEXHEAT_CELLS_MAX | 200 | The most hexagons a frame; raise it with a smaller s (the header refuses an s the cap cannot hold) |
| SH4DER_HEXHEAT_BAND_W | 256 | The band's width, screen pixels = target texels |
| SH4DER_HEXHEAT_BAND_H | 128 | Its height |
| SH4DER_HEXHEAT_FOOT | 12.0 | Band rows under the anchor, s + 4 (it follows s): s, the near cut's line and a row to spare |

## The cloudvolume effect (a cloud deck you fly through)

`include/sh4der_cloudvolume.h`, header only, and `demo/cloudvolume.c`; the effect table above says what it does. `test/cloudvolume_test.c` is its test.

### Cloudvolume tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_CLOUDVOLUME_MAP | 128 | Cover map side, texels (a power of two) |
| SH4DER_CLOUDVOLUME_NOISE | 32 | Noise volume side, voxels (a power of two) |
| SH4DER_CLOUDVOLUME_COARSE | 32 | Side of the coarse highest-top map, cells of 4 x 4 texels |
| SH4DER_CLOUDVOLUME_MAX_STEPS | 64 | Most march steps a ray |
| SH4DER_CLOUDVOLUME_MAX_LIGHT | 4 | Most light samples a march sample |
| SH4DER_CLOUDVOLUME_BAKE_TAPS | 40 | Map reads toward the sun a texel in the bake |
| SH4DER_CLOUDVOLUME_TILE | 8192.0 | World units (metres) one cover map covers: 64 m a texel |
| SH4DER_CLOUDVOLUME_THICKNESS | 1100.0 | Slab height, metres |
| SH4DER_CLOUDVOLUME_DENSITY | 0.010 | Extinction a metre in full cloud (63 % hidden after 100 m) |
| SH4DER_CLOUDVOLUME_COVERAGE | 0.60 | Share of the cover map's range that is cloud |
| SH4DER_CLOUDVOLUME_EROSION | 0.42 | How much the noise carves, 0 .. 1 |
| SH4DER_CLOUDVOLUME_SHADOW | 3.0 | Light lost over a full slab height of cloud toward the sun, as an exponent |
| SH4DER_CLOUDVOLUME_HAZE | 9000.0 | Distance at which a cloud's shade is 63 % of the way to the haze, metres |
| SH4DER_CLOUDVOLUME_FAR | 26000.0 | March end, metres |
| SH4DER_CLOUDVOLUME_NEAR | 12.0 | March start, metres |
| SH4DER_CLOUDVOLUME_STEPS | 16 | Default march steps |
| SH4DER_CLOUDVOLUME_GROWTH | 28.0 | Last step length over the first |
| SH4DER_CLOUDVOLUME_HAZE_SHADE | 150 | The shade far cloud fades to (a pale blue-grey on the ramp) |
| SH4DER_CLOUDVOLUME_STOP | 0.03 | The march stops when this share of the light still comes through |

## The pvrx layer (over KallistiOS's PVR driver)

`include/sh4der_pvrx.h` and `src/sh4der_pvrx.c`; the effect table above says what it does, `AGENTS.md` "Call order: pvrx" how to call it. `test/pvrx_test.c` tests the header's inline part.

`sh4der_pvrx_wait_idle()` is the one call that replaces `pvr_wait_ready(); pvr_wait_render_done();` (a wait before a texture or render target is rewritten): the polled wait when the hook is on, KallistiOS's two waits when it is off, 0 or -1. `demo/psm/waitpoll/` is its console test (six shapes, sleeping against polled, graphs). The demos and library passes listed in `AGENTS.md` "Waiting for the PVR" call it; the demos keep `-DWAIT_SLEEP` for an A/B. PROTOTYPE, the A/B is NOT RUN until the owner boots `psm-waitpoll-wp4`.

### Pvrx tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_PVRX_WAIT_US | 100000 | Microseconds after which a polled wait gives up (the town's a3e-poll value) |
| SH4DER_PVRX_DMA_US | 200000 | Microseconds after which a download gives up (the benches' limit; 512 KB takes about 14 ms) |

## The waitrun ending (a timed run that ends on a results page)

`include/sh4der_waitrun.h`; `test/waitrun_test.c` tests the statistics on the host. A program that runs for ever (a live demo) started from the test suite's menu calls `sh4der_waitrun_frame` once a frame; after the time is up `sh4der_waitrun_show` shuts the PVR down and shows a text page (mean, best, worst, frames over 20 ms, wait mode, timeouts) and two graphs of the run. It runs the program once, in the woken wait (the pvrx hook; the polled wait was dropped on 2026-11-28). A live scene cannot be replayed from its start for a second mode, so the sleep-against-wake comparison is `demo/psm/waitpoll`, which draws identical frames in each mode. PROTOTYPE: the pages are NOT RUN on a console.

### Waitrun tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_WAITRUN_SECONDS | 20 | Seconds the run lasts |
| SH4DER_WAITRUN_SKIP | 20 | Frames at the start left out of the figures |
| SH4DER_WAITRUN_MAX | 1500 | Frames kept (4 bytes each: 6 KB) |
| SH4DER_WAITRUN_MODES | 1 | Runs: one, in the woken wait |
| SH4DER_WAITRUN_SLICES | 64 | Points of the graph: the run in this many slices |

## The tilecut pass (a render target through its own tiles)

`include/sh4der_tilecut.h`, header only; `test/tilecut_test.c` tests the table logic on the host. KallistiOS renders a render target through the screen's whole tile table (20 x 15 tiles whatever the target's size), and the PVR visits every tile: about 3.2 ms a pass (PVR-004). `sh4der_tilecut_begin` makes each table end after the target's cw x ch tiles; `sh4der_tilecut_end` writes every changed word back. It keeps a RAM copy of each table from the moment it is found, so a cut only writes video memory: 126 us to begin and 130 us to end a pass at 10 x 8 tiles (PVR-205, MEASURED, one console). `sh4der_tilecut_begin_cols` is the cheapest form: whole columns, one word a table (5 us and 3 us, the same run).

### Calls

```c
sh4der_tilecut_t cut;
sh4der_tilecut_init(&cut, 20, 15);            /* the screen's tiles; cut.need = 2 tables */
sh4der_tilecut_find(&cut);                    /* Dreamcast: after a scene's render, until cut.n == cut.need */
if (sh4der_tilecut_begin(&cut, cw, ch)) {     /* 0: refused, nothing written, the pass runs uncut */
    /* the target's scene, its render, the wait for render done */
    if (sh4der_tilecut_end(&cut)) { /* words still wrong: shut the PVR down and pvr_init again */ }
}
```

A table with rows left out (a port that compacts it to rows 1 to 13 of 15) is `sh4der_tilecut_init_rows(&cut, 20, 13, 1)`: 13 entries a column, the first row numbered 1, and the control words keep their own row numbers. `sh4der_tilecut_begin_at(&cut, cw, ch, ry)` cuts to the cw x ch tiles whose first row is `ry` (a pass drawn at y = 32 x ry); `sh4der_tilecut_begin` is the same at the table's first row. Per-table state (`tbl_on`, `tbl_cw`, `tbl_ch`, `tbl_ry`) also lets a program cut and hold a single table (`sh4der_tilecut_begin_one_at`, `_begin_one`, `_begin_one_cols`, `_end_one`, `_is_cut_at`, `_index`, `_other`, and Dreamcast `sh4der_tilecut_active` from `PVR_ISP_TILEMAT_ADDR`): with one pass and one screen scene each frame, the pass's own table stays cut and the screen uses the other, with no render-done wait until the order breaks (a frame without a pass, exit, or `pvr_shutdown`). After a video mode change (the PVR shut down and started again) the tables are new and may have moved: `sh4der_tilecut_reset(&cut)` forgets them and any cut that was on, then `sh4der_tilecut_find` takes them again. Host tested against a compacted table and per-table held cuts (`test/tilecut_test.c`); compacted tables and single-table held cuts are NOT RUN on a console. The route A pass (`sh4der_psm_a.h`) uses it.

Rules: no render that reads the table may run while it is cut (wait for render done after every cut pass); cut only when every table is found (with one of two, the street demo lost 3.05 ms: PVR-200); the refusal that needs both tables has not run on a console.

### Tilecut tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_TILECUT_TABLES | 2 | Tile tables KallistiOS keeps (one a TA buffer) |
| SH4DER_TILECUT_MAX | 96 | The most tiles a cut keeps |
| SH4DER_TILECUT_TILES | 300 | The most tiles of a table (20 x 15): the size of the RAM copy |
| SH4DER_TILECUT_TRIES | 3 | Read-backs of a restore before `sh4der_tilecut_end` reports it bad |
| SH4DER_TILECUT_VERIFY | 8 | Ends that read their restore back; after them the restore is written and trusted |

## The suite (chaining programs on one disc)

`include/sh4der_suite.h`, header only; `test/suite_test.c` tests the plain part on the host. One disc holds a menu program and every test program as a file. The menu loads a test's binary and starts it in its own place (KallistiOS's `arch_exec`); a test that is done loads the menu's binary and starts that the same way, so the console is never switched off between tests. The two programs pass a 32-byte mail by writing it into the other's image before the start: a 16-byte mark (`PSM-SUITE-MAIL-1`, the text the menu and every test match on byte for byte), then `chain`, `index` and `status`. Nothing else is kept, so nothing depends on RAM that the start leaves alone.

RECOMMENDED (nothing here is measured by this file's own start, return or chain; the `arch_exec` facts are read from KallistiOS's source, NOT MEASURED; the console facts are the suite's notes and the ledger rows named below). Maturity: ALPHA v0.4 ("suite menu" in `wiki/status/maturity.md`).

Call order:

1. Once, early in the test's `main`, after `<kos.h>` (and after `sh4der_fault.h` when the test has it): `suite_init();`. It reads the menu's binary (`/cd/MENU.BIN`) into RAM while all is well. On a disc of the test's own it returns 0 and nothing else happens.
2. In the loop that shows the result pages, on every pass: `if (suite_page(buttons, &page, pages)) redraw = 1;` (v0.5, NOT RUN on a console). It turns the pages itself (A the next, B the one before), goes back to the menu when A + B + X + Y are held together or when A is held for `SH4DER_MENU_HOLD_MS` (2 s) on the last page (a tap there is still the next page; a line under the page says so and a bar fills while A is held), and in a chained run after `SUITE_PAGE_MS` (20 s) on the last page. After a live view started from a page, call `suite_page_forget()` so a button still held is not a press; `suite_page_line(0)` keeps the line off a last page that is a picture. The older form still works and has no hold: `if (suite_tick(buttons, page, pages)) { page++; redraw = 1; }` with the test's own A and B lines.
3. `suite_chained()` is 1 in a chained run: skip what waits for a button. `suite_keys_text(alone)` gives a result page's "ABXY menu" line, or `alone` when there is no menu.
4. `suite_rev_text(buf, size)` writes "HOLLY REV xxxx SB xx" (two register reads, nothing written) for the first line of a test, so a photo says which console it was. A host build writes "HOLLY REV ---- SB --".
5. A new test takes those three lines and goes in a suite list (`demo/psm/suite/build.py --list`). The menu itself is `demo/psm/suite/suite_menu.c`, a demo.
6. The menu's place: the menu writes one word (`sh4der_menu_pack`: its tab, row and scroll) into the mail's `spare` when it starts a test, and `suite_exit` hands it back, so the menu opens where it was left. A test built before v0.5 does not hand it back; the menu then opens on the test's row by its index.

Tuning: none. The header has no `SH4DER_*` macro. Its constants belong to the disc and are set in the header: `SUITE_PAGE_MS` (20000 ms a chained page), `SUITE_FAULT_SHOWS` (120 redraws, 60 s, before the fault page leaves), `SUITE_IMAGE_MAX` (12 MB; a larger binary is not read).

Cost: RAM is the menu's binary, read whole at `suite_init` and kept until the program leaves (its size is not measured here). VRAM 0. Frame cost none. The start itself (KallistiOS's shutdown and boot of the new program) is NOT MEASURED here.

What the suite runs showed (one console; ledger and notes):

- A start from the menu repeated Shadow chain with measured edges' results (PVR-116, `wiki/hardware/ledger-pvr.md`): one test, not the same binary as its own disc. Whether a menu start can change a result is OPEN.
- Every started test's result pages carry "ABXY menu" (MEASURED, `demo/psm/suite/NOTES.md`, "First run of s1"). c4g was started by RUN ALL and its header says "(auto)" (the same table).
- The owner's s7 video shows every started entry going back to the menu (`demo/psm/suite/NOTES.md`, "s7 ran"; no ledger row states it).
- OPEN: the four-button return in a photo, the 5 s countdown, a chained test going back by itself, and the fault page's return (no photo; `demo/psm/suite/NOTES.md`). The fault page's return runs KallistiOS's shutdown from inside a fault and may hang.

Not for: a program that needs its RAM kept across a start (the heap holds what was there), or a mark that is edited (the menu and every test match on it).


## The menu logic

`include/sh4der_menu.h`, header only, plain C; `test/menu_test.c` tests it on the host (`make test`, and `make test-shz`). It draws nothing, reads no pad and has no clock: the caller hands over what the pad says and the time in ms, and draws what the state says. PROTOTYPE v0.1: host tested, NOT RUN on a console as this file.

What is in it:

- **A list with headings.** Rows are `sh4der_menu_row_t` (a kind, `SH4DER_MENU_HEAD` or `SH4DER_MENU_ITEM`, and an id and flags that are the caller's). `sh4der_menu_move` puts the cursor on the next item up or down, over headings, round at the ends. `sh4der_menu_keep` keeps the cursor in a window of rows and shows the headings just above the window's first row. `sh4der_menu_seek` finds an item by its id.
- **Tabs.** `sh4der_menu_t` holds the tab and a place (cursor, first row) a tab; `sh4der_menu_init` puts each cursor on its list's first item, `sh4der_menu_tab` goes left or right, round at the ends.
- **A scrolled text.** `sh4der_menu_scroll_t` (place, total lines, lines on screen), `sh4der_menu_scroll_step` (clamped), `sh4der_menu_scroll_bar` (the thumb's offset and length on a track; 0 when the text fits).
- **A held direction.** `sh4der_menu_repeat`: one step at once, the next after `SH4DER_MENU_REPEAT_FIRST_MS`, then one every `SH4DER_MENU_REPEAT_MS`. `sh4der_menu_stick_dir` turns a stick axis into -1, 0 or 1 with a dead zone.
- **Tap or hold.** `sh4der_menu_hold_step`: `TAP` when the button is let go before the time, `HOLDING` with the progress in thousandths while it is down, `FIRED` once at the time. A button already down when the hold is reset does nothing until it is let go.
- **Word wrap.** `sh4der_menu_wrap`: the next line of a text in a number of columns, cut at a space.
- **The place in one word.** `sh4der_menu_pack` and `sh4der_menu_unpack`: tab, cursor, first row and scroll in 32 bits with a mark, for a program that is started again. An unpack refuses a word without the mark, a tab that is not there, or a cursor that is not on an item.

Call order:

1. Once: fill a `sh4der_menu_row_t` array a tab, a `sh4der_menu_list_t` for each, and call `sh4der_menu_init(&m, lists, tabs, window)`.
2. Each pass of the menu's loop: for the list, `step = sh4der_menu_repeat(&rep, dpad_dir, now_ms, SH4DER_MENU_REPEAT_FIRST_MS, SH4DER_MENU_REPEAT_MS)` and, when it is not 0, `sh4der_menu_move(&lists[m.tab], &m.pos[m.tab], step, window)`. For the text, the same with `sh4der_menu_stick_dir(stick, SH4DER_MENU_STICK_DEAD)` and `sh4der_menu_scroll_step`.
3. Draw from `m.pos[m.tab].first`, `.cursor` and the scroll's place; draw the bar where `sh4der_menu_scroll_bar` returns 1.
4. Before the program starts another: `word = sh4der_menu_pack(&m)`; when it comes back, `sh4der_menu_init` again and `sh4der_menu_unpack(&m, lists, word, window)`.

The client to copy from is `demo/psm/suite/suite_view.h` (the suite disc's menu); `include/sh4der_suite.h` uses the hold for A on a test's last result page.

### Menu tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_MENU_HOLD_MS | 2000 | A button held this long fires, ms (the suite: A on the last result page) |
| SH4DER_MENU_REPEAT_FIRST_MS | 300 | A direction held: the wait before its second step, ms |
| SH4DER_MENU_REPEAT_MS | 80 | ... and between the steps after it, ms |
| SH4DER_MENU_STICK_DEAD | 40 | A stick axis (-128 .. 127) inside this is at rest |
| SH4DER_MENU_TABS_MAX | 4 | The most tabs of a menu (the packed place has 2 bits for the tab) |
| SH4DER_MENU_ROWS_MAX | 511 | The most rows of a list (the packed place has 9 bits for a row) |
| SH4DER_MENU_PLACE_MARK | 2684354560 | The top 4 bits of a packed place (0xA0000000); a word without them is refused |

All four times and the dead zone are ADVISED: chosen on the host, NOT RUN on a console.

Cost: RAM 4 bytes a row (the caller's array), 20 bytes the menu's state, 6 a scroll, 8 a repeat, 8 a hold. VRAM 0. No frame cost of its own; the bar's place takes two integer divides a redraw. Time on the SH-4: NOT MEASURED.

Not for: drawing (the caller's), or a list of more than 511 rows.


## The fault catcher

`include/sh4der_fault.h`, header only: the Dreamcast part is under `_arch_dreamcast`, and one file defines `SH4DER_FAULT_IMPL`. After `pvr_init` (and again after any later one), keep one `sh4der_fault_crumbs_t` and call `sh4der_fault_init(&crumbs)`; once a frame `sh4der_fault_beat`, then the setters for the stage and the counts. On an exception, an assert, an exit or `SH4DER_FAULT_HANG_VBLANKS` vblanks with no beat, it writes the registers, the return addresses found on the stack and the breadcrumbs over the frame buffer in minifont. A hang is caught only while interrupts run. Never build `SH4DER_FAULT_TEST` (its forced faults) into a program for players. `test/fault_test.c` tests the formatting on the host. PROTOTYPE: no console run has shown it catching a fault; PVR-183 and PVR-208 froze with no HANG screen (MEASURED, one run each, the cause OPEN). Moved from `demo/psm/psm_fault.h` on 2026-11-26; wiki/effects/fault.md and wiki/methods/fault-catcher.md.

### Fault tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_FAULT_COLS | 64 | Characters a line |
| SH4DER_FAULT_ROWS | 26 | Lines on the screen |
| SH4DER_FAULT_HANG_VBLANKS | 120 | Vblanks with no heartbeat before HANG (2 s at 60 Hz) |
| SH4DER_FAULT_TRACE_MAX | 12 | Return addresses kept from the stack |
| SH4DER_FAULT_STAGE_LEN | 12 | Stage name length, 11 kept |

`SH4DER_FAULT_VSYNC` is a register bit, on `tools/check_tuning.py`'s exempt list.

## The split screen divider

`include/sh4der_split.h`, header only, plain C; `test/split_test.c` tests it on the host. A frame: `sh4der_split_step` with the held triggers, `sh4der_split_halves` for the two halves, then for each half that is not hidden set its user clip and send its look; `sh4der_split_x` gives the line's pixel column and `sh4der_split_label_x` a place for each half's label. A hidden half is not sent at all (a clip rectangle cannot be empty). The middle position works on a console (PVR-144, PVR-161); a divider that moves has no console run (wiki/status/maturity.md "split divider": OPEN). Moved from `demo/psm/psm_split.h` on 2026-11-26; wiki/effects/split.md.

### Split tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_SPLIT_TILE | 32 | Tile size in pixels (the PVR's user clip unit) |
| SH4DER_SPLIT_COLS | 20 | Tile columns across the screen (640 / 32); the divider has COLS + 1 places |
| SH4DER_SPLIT_ROWS | 15 | Tile rows down the screen (480 / 32) |
| SH4DER_SPLIT_REPEAT | 6 | Frames between steps while a trigger is held (the whole way in 2 s) |
| SH4DER_SPLIT_SAFE_X0 | 64 | Title-safe area, left side in pixels |
| SH4DER_SPLIT_SAFE_X1 | 576 | Title-safe area, right side in pixels |

## The frame read

`sh4der_frame.h`, `src/sh4der_frame.c`: the look and the glow work from the frame the CPU reads off the screen, and that frame is one or two frames old, and which one changes from frame to frame. An effect that takes "last frame's" gain or glow off the read corrects the wrong frame and pulses. So every frame carries a stamp: `sh4der_frame_next` numbers it (0..7) and keeps the eye's gain and the glow's strength it is drawn with; `sh4der_frame_stamp_draw` lays a small block of that number's colour at the top left; `sh4der_frame_read` samples the screen on the grid and reads the stamp back, so the client takes off exactly what the read frame carried (`sh4der_frame_degain`, the glow's `sh4der_glow_unfeed`). The read gives two grids: one sample a cell for the eye, and the darker of two samples a cell for the glow, so one hot pixel alone in its cell (a texture's grain, a letter's stroke) lights nothing. `sh4der_frame_hot` is how hard a light is looked at: a client grows and whitens its sun by it. An emulator shows the CPU a black screen unless it copies the drawn frame into video memory (its framebuffer emulation).

### Calls

```c
void sh4der_frame_hist_reset(sh4der_frame_hist_t *h);
unsigned sh4der_frame_next(sh4der_frame_hist_t *h, float gain, float gain_ln, float glow);
uint32_t sh4der_frame_stamp_argb(unsigned id);
unsigned sh4der_frame_stamp_id(uint16_t px);
uint16_t sh4der_frame_min565(uint16_t p, uint16_t q);
void sh4der_frame_degain(uint16_t *px, unsigned n, float gain);
float sh4der_frame_hot(float facing);
```

### Frame tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_FRAME_GRID_X | 80 | Samples across the frame |
| SH4DER_FRAME_GRID_Y | 60 | Samples down the frame |
| SH4DER_FRAME_STAMP_PX | 4 | The stamp block's side, pixels |
| SH4DER_FRAME_HISTORY | 8 | Frames of history kept |
| SH4DER_FRAME_HOT_FROM | 0.85 | Cosine off the view axis from which a faced sun grows |

## The core (shared parts)

`include/sh4der_core.h` and `src/sh4der_core.c`. Every other module includes this header. It holds what the effects share, so each effect stays a few lines. It draws nothing itself.

- **Calls.** `sh4der_ramp_colours` (EDGE and CORE colours from a texture's average colour, once at load), `sh4der_breath_raw` / `sh4der_breath_reset` / `sh4der_breath_push` (the breath tracker: a pose's own breathing turned into 0 (out) .. 1 (in), pushed once a frame per instance), `sh4der_glint` (a flare quad's brightness by camera depth), and the inline helpers (`sh4der_sat_`, `sh4der_lerp_`, `sh4der_fma_`, `sh4der_dot3_`, `sh4der_norm3_`, `sh4der_sincos_`, `sh4der_feather_`, `sh4der_ramp_` and others). Each helper is one SH4ZAM call on the console and a plain C mirror under `SH4DER_CORE_NO_SH4ZAM`.
- **Cost.** RAM: the breath tracker is a small struct the caller owns; everything else is stateless. VRAM: 0. AICA: 0. CPU: a helper is one SH4ZAM instruction or call.
- **Limits.** The feather is SH4DOW-DC's (`sh4dow_feather_in_`), so `include` must be on the include path. Non-finite inputs read as "no effect this frame".

## The sh4dow effect (soft character shadows)

`include/sh4dow_dc.h` (the API), `src/sh4dow_dc.c` (the limb columns), `src/sh4dow_foot.c` (the foot cones) and `include/sh4dow_heightfield.h` (the sun over a heightfield, experimental). Soft self-shadow for skinned characters, computed per vertex on the CPU. Each limb casts a soft column away from the light; a vertex inside a column gets darker. No shadow map, no second mesh, no extra draw, no video memory.

The floor half is the **foot cones**: two small shadows per character, one under each foot, each a cone that starts at the heel and stretches away from the light. The library works out the quad, its picture and its fade; you draw the quad.

The soft edge every effect here uses is this library's column feather (`sh4dow_feather_in_`, in `sh4dow_dc.h`): see [The core](#the-core-shared-parts).

- **Calls.** Pose the skeleton and light the vertices (the shade only dims a colour that exists). Fill one `sh4dow_limb_t` per limb that casts. `sh4dow_prepare` once per character per frame with `direction` (the way the shadow falls, away from the light). `sh4dow_shade` on the vertices about to be drawn, after lighting and before packing. Or, in your own loop, `sh4dow_axes` once and `sh4dow_strength` per vertex. Fastest: `sh4dow_load_xmtrx`, `sh4dow_project`, `sh4dow_ball_pack`, then `sh4dow_strength_packed_n` per bone (it loads XMTRX on the Dreamcast: load your own matrix again afterwards). Flag a head `SH4DOW_LIMB_HEAD` and its neck `SH4DOW_LIMB_NECK` for the chin rule; `bone` is the vertex's first bone, and `SH4DOW_BONE_NONE` skips nobody.
- **Foot cones.** `sh4dow_pick_feet` once per model, from its bind-pose node spheres. Per foot, per frame: `sh4dow_foot_place` from the posed foot sphere, set `foot.away` to the light's travel at the foot, then `sh4dow_foot_cone` on the floor plane under the foot. A return of 0 means no quad. Draw the quad (TL, TR, BL, BR) with `sh4dow_foot_texture565` (or `sh4dow_foot_texture`), blended `ZERO / INVDESTCOLOR` (the floor keeps `1 - texel x strength`), at a grey strength of your darkness x the cone's fade x `sh4dow_foot_range_fade`. Extra lights: `sh4dow_foot_layer_weight` and `sh4dow_foot_layer_distinct`.
- **Units.** The soft shadow takes your vertex units. The foot cones' distances are in reference units (1 unit = 3.048 m, a standing character about 0.7 tall): `sh4dow_foot_params_scale` carries them to yours. Radii and slopes are ratios.
- **Pitfalls.** A shade that points at the light: flip `direction`. A column of the wrong size: the radius is not in vertex space. The whole character darkens: `outer_scale` times your radii is bigger than the body. A cone that points at the light: `away` is the wrong way. Cones vanish: the normal and `up` disagree (a wall), or the heel is more than `fade_height` above the plane.
- **Limits.** Up to `SH4DOW_COLUMN_MAX` (32) columns a frame, a budget that is not part of the maths. The first client spends 14 on at most six characters (REPORTED, see Cost). How many limbs cast, how many characters a frame, and any fade with distance are the client's choice. The heightfield sun has no overhangs and no off-grid occluders.

### sh4dow cost

- **The shade on the Dreamcast, before and after (REPORTED).** The library's README gives 14.70 ms for the shade before the column lists and the one-FTRV-per-vertex loop, and 4.5 ms after, for one cutscene of six characters: 84 columns, 5,406 vertices, 33 ms frame. That is about 0.83 us, roughly 166 SH-4 cycles, a vertex (544 before). Source: the library's author, one run, written in the library's own README before the fold; not in the ledger, so there is no ledger row. Status REPORTED until a row is written.
- **Columns visited, a PC census (counted, exact, host).** Over 1,063,664 skinned vertices of a client's bind-pose characters under two lights: 13.3 columns a vertex every column, 5.2 the sorted loop, 2.6 the column lists. These are counts, not time.
- **Packed lists.** Not measured on hardware. The bench (`bench/bench.c`, `make bench SH4ZAM=...`) times the three loops on the console; the emulator's cycle numbers in the library's README are an emulator's model, not evidence.
- **Foot cones.** Not measured on hardware: a few dozen multiplies, a handful of dot products and three reciprocals per foot per frame (ESTIMATE from the code).
- **RAM and VRAM.** `sh4dow_frame_t` about 3 KB, one per character drawn (or one reused). The 32 x 32 RGB565 foot picture is 2 KB of texture RAM. VRAM 0.

### Sh4dow tuning

The limb columns (the header's macros are the source; these are its shipped values):

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DOW_INNER | 1.25 | Solid core radius, in limb radii. Raise: fatter shade. Lower: thinner |
| SH4DOW_OUTER | 1.35 | Radius where the shade reaches zero, in limb radii. Raise: wider, softer edge. Keep it above SH4DOW_INNER |
| SH4DOW_LENGTH | 1.8 | How far the column runs along the light. Raise: shade reaches farther down the body |
| SH4DOW_HOLD | 0.8 | Fraction of the length at full strength. Raise: the end stays dark longer |
| SH4DOW_DARK | 0.65 | Colour lost at full strength (1.0 leaves black, 0 leaves the colour alone) |
| SH4DOW_COLLAR | 0.8 | Neck radii past the neck where a head column starts. Raise: starts lower, misses more of the jaw. Lower: can stain the chin |
| SH4DOW_FADE_IN | 0.1 | Limb radii past the start before full strength. Raise: softer start, less flicker |

The foot cones (lengths and widths in foot radii; distances in the reference units above):

| Name | Default | Unit | Meaning |
| --- | --- | --- | --- |
| SH4DOW_FOOT_MIN_LEN | 3.0 | radii | Cone length with the light overhead |
| SH4DOW_FOOT_GAIN | 3.0 | radii per slope | Extra length per unit of the light's slope |
| SH4DOW_FOOT_MAX_SLOPE | 2.0 | slope | Where the length stops growing (9 radii) |
| SH4DOW_FOOT_MIN_FACING | 0.25 | normal . up | The floor must point up more than this (a wall gets no cone) |
| SH4DOW_FOOT_HEEL_BACK | 0.7 | sphere radii | The heel's distance behind the foot sphere's centre |
| SH4DOW_FOOT_WIDTH | 0.8 | sphere radii | Half width at the heel |
| SH4DOW_FOOT_WIDTH_MIN | 0.03 | distance | Smallest half width |
| SH4DOW_FOOT_WIDTH_MAX | 0.12 | distance | Largest half width |
| SH4DOW_FOOT_FADE_HEIGHT | 0.35 | distance | Heel height above the floor where the cone is gone |
| SH4DOW_FOOT_LIFT | 0.012 | distance | The quad's offset off the floor |
| SH4DOW_FOOT_RANGE | 20.0 | distance | Camera distance where the cone is gone |
| SH4DOW_FOOT_RANGE_FADE | 2.0 | distance | The fade out spans this much before the range |
| SH4DOW_FOOT_APEX | 0.25 | fraction of the cone | Where the picture's round heel sits |
| SH4DOW_FOOT_ZONE | 0.25 | fraction of the height | Feet lie in this lowest part of the model |
| SH4DOW_FOOT_LAYER_GAIN | 0.6 | share | An extra light's shadow at distance 0, against the main cone's 1 |
| SH4DOW_FOOT_LAYER_FAR | 0.35 | share | The share of a light with no position |
| SH4DOW_FOOT_LAYER_COS | 0.9659 | cosine | Two lights within this (15 degrees across the floor) cast one cone |

`SH4DOW_FOOT_LAYERS` (4) is the count of extra cones per foot, and `SH4DOW_COLUMN_MAX` (32) the column budget; both are caps, not tuning. `sh4dow_params_default` and `sh4dow_foot_params_default` fill the tables; `NULL` means the same. The tuning rows are checked by hand against the header, not by `tools/check_tuning.py`, which scans only `SH4DER_*` names (see Build and test).

### Heightfield sun (experimental)

`include/sh4dow_heightfield.h`, header only: directional sun visibility for a regular heightfield (water, terrain, any single-valued surface). It sweeps along the sun's dominant horizontal axis, propagates the highest sunward blocker height and feathers the visibility with the library's smooth step. Pass the heights, the grid size, the cell size, the direction toward the sun (it must point above the horizon), a vertical bias, a feather width, an output grid and a scratch buffer of `2 * max(width, height)` floats. Output: 0 lit, 1 blocked. Cost O(width x height), no GPU memory; the caller holds the height and shadow fields. Boundaries are open; overhangs, off-grid blockers and the exact solar-disc penumbra are absent. `test/water_test.c` checks flat fields and ridge shadows on both axes and both maths paths. The ocean demo uses it.

### Sh4dow call order

See `AGENTS.md` "Call order: sh4dow" for the step list. Tests: `test/shade_test.c` and `test/foot_test.c` (both maths paths), `example/example.c`, `example/quickstart.c` and `example/feet.c` (the examples; their output is checked against the text here). `bench/bench.c` is the Dreamcast timing program; `bench/pipeline.sh` runs it in an emulator, which is not evidence.

### Credit line

A client that ships the soft shadows prints this line, as the library's author asks:

> Soft self-shadow: SH4DOW-DC by Jilted Valkyrie. Dreamcast math: SH4ZAM by Falco Girgis.

SH4DOW-DC's origin is in the "Credits and license" section below.

## The uvwarp effect (per-vertex texture offset)

`include/sh4der_uvwarp.h`, header only, all inline. The PVR cannot move a texture lookup per pixel, but it can move a vertex's texture coordinate. A mesh with enough vertices along the motion (an electric arc, a flag, a water strip) gets most of a distortion map's wobble that way. The offset is `weight x table(u + su t, v + sv t)`, read bilinear, both axes wrapping, each channel 0..1 and unsigned.

- **The table.** `sh4der_uvwarp_t`: a pointer to w x h pairs of bytes (R, G), row-major, w and h each 1 .. 256 (`SH4DER_UVWARP_SIDE_MAX`). The caller owns it and builds it by a box filter from the source map; 64 x 16 is 2 KB.
- **The calls.** `sh4der_uvwarp_sample` (the table at one point), `sh4der_uvwarp_offset` (weight x table, with the scroll), and a prepared form for a mesh loop: `sh4der_uvwarp_prepare` once a frame, then `sh4der_uvwarp_offset_p` a vertex (or `_pn` where the caller checked the input once).
- **The weight.** Per vertex, the caller's, 0..1. Fade it to 0 at the ends of a mesh or its ends slide off their anchors.
- **Cost.** RAM: the table, 2 KB at 64 x 16. VRAM: 0. AICA: 0. CPU: one bilinear read (four byte pairs) and a few fused multiply-adds a vertex. SH4ZAM for all maths.
- **Limits.** Coordinates must be finite and under 2^23 in size; anything else, or a bad table, gives 0, 0. Wrap the time to the scroll's period now and then: the plain form loses precision as time grows (the prepared form does not). Accuracy is 1.1e-3 of the table range at worst.

## The water effect (waves and Fresnel)

`include/sh4der_water.h`, header only, all inline, built on `sh4der_sss.h`. A sea or lake surface worked out per vertex, with no texture needed for the shape of the waves.

- **Waves.** `sh4der_water_wave_t` is one directional wave (unit direction, amplitude, wavelength, speed, phase, and a `ripple` flag). Long waves move the geometry; ripples change only the normal. `sh4der_water_sample` adds up a wave table at an (x, z) point and a time and returns the height and slope; its `footprint` argument fades out waves too short for the sampling distance.
- **Shading.** `sh4der_water_normal` (unit normal from the slope), `sh4der_water_reflect` (the reflected sky direction), `sh4der_water_fresnel` (Schlick's term at the air to water value 0.0204).
- **Light through water.** `sh4der_water_transmit` (absorption over a path, a thin wrapper over `sh4der_sss_absorb`) and `sh4der_water_crest_sss` (crest transmission: crest 0..1 is the coverage, the path runs from thick to thin; an artistic proxy, not a multiple-scattering solve).
- **Cost.** RAM: the caller's wave table (28 B a wave). VRAM: 0. AICA: 0. CPU: one sine and cosine pair and a few fused multiply-adds per wave per sample. SH4ZAM for all maths; `SH4DER_CORE_NO_SH4ZAM` for plain C.
- **Limits.** Directions must be unit and wavelength above 0 (others are skipped). The result is a per-vertex value: a mesh needs enough vertices for the waves you want to see.

## The demo disc

`demo/main.c` is one program that shows the effects on a small scene of its own: a dark hall that opens on a sunlit yard, with a few box-built models as actors (a block figure, a station wagon, a jet, crates, a work lamp). The effects with a row on the read-out, switched and tuned live, with the video memory each holds and the frame's time on screen: look (EYE), glow (BLOOM, RAYS, CURVE), torch, shafts (SH4FTS), taa (TAA rows), ca (CA, RES) and SH4DOW-DC (SH4DOW). trail is included with no row.

Build the disc image:

```sh
make demo SH4ZAM=/path/to/sh4zam            # out/cdi/sh4der-demo.cdi
make dc DEMO=1 SH4ZAM=/path/to/sh4zam       # the library, and with the flag DEMO=1 the demo disc as well
```

It needs the KallistiOS environment sourced (`kos-cc`), Python 3 (the models become a C header) and [mkdcdisc](https://gitlab.com/simulant/mkdcdisc) (the image). Name them if they are not on the path: `PYTHON=python MKDCDISC=/path/to/mkdcdisc`. The image runs on a Dreamcast from a burned disc or a GDEMU, and in an emulator. For the eye, the bloom, the rays and the shadow shafts an emulator must have full framebuffer emulation on: they read the finished frame back.

Pad: d-pad left / right picks a row, A switches it on or off, d-pad up / down changes its strength, Y hides the read-out. Left stick: walk and strafe. Second stick: turn and look. R / L trigger: look up / down.

| Row | What it shows |
| --- | --- |
| EYE | the picture adapts like an eye (`sh4der_look.h`) |
| BLOOM, RAYS, CURVE | bright things spill a glow, the sun throws rays; the ramp and its threshold (`sh4der_glow.h`) |
| TORCH | the flashlight (`sh4der_torch.h`) |
| SH4FTS | dark shapes cast shade away from the sun (`sh4der_shafts.h`) |
| CA, RES | chromatic aberration; the frame texture at 640 x 480 or 320 x 240 (`sh4der_ca.h`) |
| TAA, TAAHOLD, TAAMODE | the temporal blend: its share, kept or not, dynamic or fixed (`sh4der_taa.h`) |
| SH4DOW | soft self-shadow on the four nearest actors, foot cones under the two-legged ones (SH4DOW-DC) |
| CAR, WEDGE, NIGHT | drive the station wagon with its flood lamps; dim the world outside their fan; the yard at night |
| PROBE | what the frame read saw, and the bloom's map |
| DR | the world's vertices by KOS direct render, or one `pvr_prim` call each |

The models are made by `demo/tools/make_models.py` (boxes and wedges, no outside assets) and turned into C by `demo/tools/obj2c.py`.

### The street driving demo

`demo/pl4nar`: a second disc, its own program. A coupe on a street through a 45 second day: the street reflected on the car (`sh4der_pl4nar`), lamp pools, headlight beams and blob shadows (`sh4der_pool`), lamp halos that also pass over the car in the reflection (`sh4der_spark_quad`). Two more layers are the demo's own, not the library's: a heat haze over the road's far end by day (`src/heat.h`) and, in the wet build, the road's reflection as a soft pool round the car under a generated grit layer (`tools/grit_gen.py`). `make pl4nar-demo SH4ZAM=...` (or `python demo/pl4nar/build.py`) goes from a clean checkout to an image: it fetches CC0 art that is not in this repository, builds the hero car from numbers, cuts the stock models in Blender and bakes. `demo/pl4nar/README.md` has the controls, the costs and what is not yet proven; `demo/pl4nar/AGENT_WORKFLOW.md` is the checklist for an agent putting the same effects into another engine; `demo/pl4nar/CREDITS.md` every licence.

## The normal map effect

`sh4der_bump.h` (header only; the Dreamcast part sits under `_arch_dreamcast`): a normal map lit by a moving light, put on by ordinary blend passes of grey textures, not by the PowerVR's bump unit. A normal map texel is a surface normal kept as two bytes (S the slope, R the direction it leans in; the KallistiOS `pvrtex` layout, so the maps the tools make work); only the normals are kept, in RAM. The effect is highlight power 2: `(N.H)^2`, H the half vector of the light and the eye. The functions and constants keep the name `bump`, as the texel format and KallistiOS do.

- **The texture.** `sh4der_bump_from_height` writes a twiddled normal map texture from an 8-bit height map (`side` a power of two up to 256, edges wrap). `sh4der_bump_texel` packs one normal. The layout is KallistiOS's `pvrtex` (`pxlFloattoSpherical`); a flat texel is `0xff00`. No mip levels.
- **The normals.** `sh4der_bump_normal_decode` turns the map into 3 int8 normals a texel in RAM, once; `sh4der_bump_normal_at` reads one at a picture coordinate (bilinear, unit length); `sh4der_bump_vertex_light` lights a vertex by it (the vertex methods of the torch walk).
- **The light.** `sh4der_bump_toward` gives the unit direction to the torch; a surface frame (`sh4der_bump_frame_t`: the world directions of u, of v, and the normal) puts it in the texel's frame, `sh4der_bump_frame_flip` says whether the frame is mirrored.
- **The passes (the torch walk shows them).** `(N.H)^2 = sum Ni^2 Hi^2 + sum 2 Hi Hj Ni Nj`. Nine 8-bit grey maps are made once from the normals: the squares `Nx^2`, `Ny^2`, `Nz^2` and the products `NxNy`, `NxNz`, `NyNz` biased 0.5 in a + and a - copy (grey ramp from `sh4der_bump_grey_palette`). A lit cell draws a flat level kept at half, one add pass (ONE x ONE, vertex colour = the weight) for each term that matters (at most six), then the picture with the blend DESTCOLOR x (source colour): twice the picture times the level, so a flat texel stays the picture and a slope facing the half vector goes up to twice it. The bias is taken off the flat level, so the sum is exact up to the clamps.
- **The polygon.** `sh4der_bump_send_header` once for a run of cells, `sh4der_bump_send_fan` for a convex polygon of up to 12 corners as one strip (`sh4der_bump_submit_fan` does both); a header that was never compiled sends nothing (`sh4der_guard.h`).
- **Open items.** Which way the light's azimuth turns against the texture's v axis (`sh4der_bump_frame_t.flip` turns it round; the torch walk's AZIMUTH row), the signs of the axes in the products, and the 8-bit grey maps through a 565 palette (banding).
- **The cost.** Up to eight draws a lit cell (the level, six grey passes at most, the picture), so the lit pixels are filled up to eight times; nine grey maps a surface in video memory (4 KB each at 64 x 64, 16 KB at 128 x 128); the normals in RAM (3 bytes a texel: 12 KB at 64 x 64). No normal map texture is uploaded and nothing is allocated by the header.

### Normal map tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_BUMP_STRENGTH | 0.65 | The strength of the highlight at full light (the value of the game it was first made for; the torch walk scales it by this) |
| SH4DER_BUMP_TWO_PI | 6.28318530718 | 2 pi, for the texel's angle |
| SH4DER_BUMP_HALF_PI | 1.57079632679 | pi / 2, the slope's range |
| SH4DER_BUMP_MAX_FAN | 12 | Most corners a polygon sent by `sh4der_bump_send_fan` may have |

## Recommended settings

The numbers the author's own game and demo disc use, found by eye. Each header repeats its own under "Recommended settings".

| Effect | Setting | Why |
| --- | --- | --- |
| Eye (`sh4der_look.h`) | strength 0.25: transient and open x 0.25, gain 0.9375 .. 1.125; clamp any strength to gain 0.4 .. 2.0; into light 2 s, into dark 6 s; held while a flashlight is lit | felt, not seen; RGB565 bands past the clamps |
| Bloom (`sh4der_glow.h`) | straight ramp from threshold 0.80 (`sh4der_glow_curve(lut, 0.80f, 0)`); strength 1.00 x 0.45, radius 0.5; `keep` 128; strength / the eye's gain when that is under 1 | only lights and the sun glow; no pulse from an old read |
| Rays (`sh4der_glow.h`) | strength 1.00, 6 copies, only with the sun in front | the longest, smoothest streak |
| Shadow shafts (`sh4der_shafts.h`) | strength 0.50, off by default; range 0.25 .. 3.00 | stronger reads as a second shadow |
| Chromatic aberration (`sh4der_ca.h`) | strength 1.00 = 3 pixels in the corner; range 0.25 .. 4.00; 640 x 480; off by default, silent when memory is short | 12 pixels is a damaged lens |
| Temporal blend (`sh4der_taa.h`) | dynamic: old frame 0.675 with the camera still, 0.30 moving, mixed by turning and walking speed, never over 0.9, history kept; only with the 320 x 240 frame texture | still pictures smooth, moving pictures do not ghost; 320 KB instead of 1,280 KB |
| Flashlight (`sh4der_torch.h`) | near light radius 6.0 world units, strength 0.85, tint 0.94 / 0.82; edge about 22 degrees; reach 5.0; carve full under luminance 0.30, none from 0.80 | a warm lamp that darkens nothing in daylight |
| Soft shadows (`sh4dow_dc.h`) | self-shadow on the 4 characters nearest the camera; foot cones under every two-legged character | the cost stays flat as a crowd grows |

The temporal blend's weight, as the demo computes it each frame:

```c
const float still  = clamp(SH4DER_TAA_WEIGHT_HI * 0.90f, 0.0f, 0.9f);              /* 0.675 */
const float moving = 0.30f;
const float f = fminf(out.weight / SH4DER_TAA_WEIGHT_HI,                            /* 1 not turning .. 0 turning fast */
                      1.0f - saturate(walk_speed / walk_full));                     /* 1 standing .. 0 walking */
out.weight = out.cut ? 0.0f : lerp(moving, still, f);                               /* no smoothing over time */
```

## Shared tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_BREATH_WINDOW | 90 | Frames the breath tracker remembers (90 frames, 3.0 s); a tracker is 364 bytes |
| SH4DER_BREATH_SPAN | 0.0013 | Least chest travel that counts as a full breath |
| SH4DER_GLINT_RADIUS | 0.025 | Glint quad radius before the scale |
| SH4DER_GLINT_SCALE_X | 4.0 | Horizontal scale (half width 0.10) |
| SH4DER_GLINT_SCALE_Y | 6.0 | Vertical scale (half height 0.15) |
| SH4DER_GLINT_BRIGHTNESS | 0.4 | Glint brightness at full |
| SH4DER_GLINT_NEAR | 3.0 | Full glint within this camera depth |
| SH4DER_GLINT_FAR | 6.5 | No glint from this camera depth |
| SH4DER_GLINT_OFFSET | 0.0075 | Glint quad moved this far toward the viewer |
| SH4DER_RAMP_EDGE_SAT | 1.6 | EDGE saturation factor (the blade texture's blue texels: saturation 0.55) |
| SH4DER_RAMP_CORE_SAT | 0.25 | CORE saturation factor (the same texture's white-hot texels: saturation 0.09) |
| SH4DER_TUBE_MAX | 2 | Tubes an effect frame holds |
| SH4DER_BLOCKER_MAX | 2 | Blockers an effect frame holds |

```c
void sh4der_ramp_colours(const float avg[3], float white, float edge[3], float core[3]);
```

`sh4der_ramp_colours` turns a texture's average colour into EDGE and CORE at load; `white` (0..1) mixes both toward white (a white test mode). A black average gives cyan.

## Adding an effect

1. Write `include/sh4der_<name>.h` and `src/sh4der_<name>.c` (or, as the look effect, the header alone with inline calls). Include `sh4der_core.h`, prefix every name `sh4der_<name>_` / `SH4DER_<NAME>_`, and use the shared steps: the feather, the shapes, the SH4ZAM helpers. Do not copy SH4DOW-DC or SH4ZAM code in.
2. Write `test/<name>_test.c`: the default numbers, the bad params, both paths.
3. Add the source to `SRC` and `HDR` in the Makefile, and the test to `TESTS`.
4. Add a row to the effect table above and a tuning table with the headers' macro values. `make test` runs `tools/check_tuning.py` (`make check-tuning` alone): it fails when a row's value is not the header's, when a row names no macro, or when its value is not a number. A numeric macro with no row fails too (`make test` runs it with `--strict`); a macro that is not a tuning value (a flag, a derived count, a constant, an input limit) goes in the script's `EXEMPT` list.

## Build and test

```sh
make test                        # host, plain C: test/core_test, test/fakeaf_test, test/ground_test, test/blade_test, test/holo_test, test/bloom_test, test/look_test, test/taa_test, test/frame_test, test/glow_test, test/warp_test, test/trail_test, test/lens_test, test/focus_test, test/visor_test, test/torch_test, test/ao_test, test/guard_test, test/sss_test, test/spark_test, test/bolt_test, test/motes_test, test/pl4nar_test, test/pool_test, test/psm_test, test/psm_raster_test, test/psm_pvr_test, example/blade
make test-demo                   # host: the demos' own tests alone (make test runs them too); a test whose fetched art is missing says NOT RUN
make test-shz SH4ZAM=/path/to/sh4zam   # host, the SH4ZAM path on SH4ZAM's software back-end
make dc SH4ZAM=/path/to/sh4zam   # Dreamcast: build/libsh4der_core.a (KallistiOS environment sourced)
make test-dc SH4ZAM=/path/to/sh4zam  # Dreamcast: build/*_test.elf
```

There is no `SH4DOW=` variable: SH4DOW-DC is built from this folder's `src` and `include`. The Dreamcast build wants SH4ZAM's include path and these flags: `-ffast-math -ffp-contract=fast -mfsrra -mfsca -fno-math-errno`, and defines neither `SH4DER_CORE_NO_SH4ZAM` nor `SH4DOW_DC_NO_SH4ZAM`. The library needs no KallistiOS header. Use SH4ZAM as a library, 0.10 or later.

- `test/shade_test.c` and `test/foot_test.c`: the sh4dow columns, lists and packs, and the foot cones, on both maths paths (see The sh4dow effect).
- `test/core_test.c`: the feather equals SH4DOW-DC's over a grid, the core before the root (a vertex on a tube's line, a zero core), the SH4ZAM steps and their mirror, the breath tracker's and glint's refusals.
- `test/blade_test.c`: the default numbers at fixed ticks and points; every early-out against a plain formula over 200,000 random vertices, ticks and fades; the period closing with no seam; the fade; bad params; the shade and packed-colour loops.
- `test/bloom_test.c`: the default levels and positions for the default and MAX WHITE presets; the colour ramp; MAX WHITE keeping its structure; the counts (24 triangles, every vertex used); the ribbon and the disc facing three eyes; the feathered edges and more at the top; bad params.
- `test/glow_test.c`: the threshold curve and the RGB565 pack, the shrink, the halo split, the darken balance, the ray series, the sun fade.
- `test/warp_test.c`: the ring, column and wobble offsets, the edge clamp, the radial scales and weights, the pixel grid, the dither tile.
- `test/trail_test.c`: the weights by speed, the even pixel shifts, the copy passes, a cut, the RGB565 drift model.
- `test/lens_test.c`: the vignette's vertex colours and banding bound, the grain's mean, the wedge geometry, the flash envelope.
- `test/focus_test.c`: the box blur, the mask falloff for each shape, the mesh counts.
- `test/visor_test.c`: the presets through the grade, the thermal ramp, the affine fit error, the fade envelope.
- `test/torch_test.c`: the profile, the mesh colours, cone to radius, the fog bend, the darkness gate, the power ramp, the anti-aliasing 2X x space.
- `test/ao_test.c`: the crease factor by angle, the cavity cue on synthetic pictures.
- `test/ca_test.c`: the frame texture's sizes and the chromatic aberration's split.
- `test/shafts_test.c`: the shadow shafts' terms and the shade map from a read.
- `test/guard_test.c`: a normal triangle kept; a line, a repeated point, one point three times and three points on one ray dropped; a thin sliver kept and the caller's threshold; a point at or behind the near plane not judged; a non-finite point dropped; the polygon header's command word (zeros refused).
- `test/uvwarp_test.c`: a texel centre returns its texel, the middle of two their mean, both axes wrap (continuous across 1.0, negatives too), the scroll moves the table, a whole turn is no scroll, the offset is weight x table and never negative, weight 0 gives 0, a bad table or a non-finite input gives 0, 0; the prepared form gives the plain one's answer over a grid of u, v, time, scroll and weight (to 2e-4) and over odd sizes (1 x 1, 3 x 5, 7 x 2, 255 x 255: to 1.2e-3), its guards (non-finite u or weight, u below -32, a huge scroll, a NULL table) give 0, 0, a coordinate outside plus or minus 2^23 (3e9, 1e10, 1e30, 3.4e38, infinity, NaN) gives 0, 0 with the table in a field of 0xFF bytes (a read outside it would show), weights are saturated to 0..1; built plain, with SH4ZAM and with -ffast-math.
- `test/pl4nar_test.c`: the materials, a point mirrored about a plane, the view rows against a camera worked out by hand and the mirrored rows against the plain rows of the mirrored point, a view with no direction refused, Fresnel's end points and its equality with the water kernel, the exact planar case, lookups inside 0 .. 1 and alphas inside 0 .. 255 over random vertices, a lost ray's fade, the batch through a stride, the split edge, the sphere against the plane.
- `test/pool_test.c`: a disc's centre and rim (circle, ellipse, alpha, colour, counts), a wedge's apex and arc (length, half angle), every triangle through the area guard, bad arguments.
- `test/frame_test.c`: the stamp there and back through RGB565, the history ring, the darker of two pixels, a gain taken off, the looked-at term, the glow taken off a read, the weight table from a threshold.
- `test/taa_test.c`: the Halton values, the jitter centred over a cycle, a known turn's shift in pixels, the weight falling with speed, a cut.
- `test/look_test.c`: the luma bins and the log average; the eye into light (over-bright, then settled by LIGHT_S) and into dark (darker, then slow); the log and exp series against the C library; every grade layer list against clamp(A x + B) over a ramp; the layer colour; the glare.
- `test/holo_test.c`: the default weights and table entries, the breath tracker over a breathing chest that then leans, a table that never gets darker with more weight or more breath, the fade, bad params, the glint by depth.
- `test/psm_a_test.c`: the accounting (four casters, 2,200 triangles, a caster's cap, a refusal counts, nothing before a fit), a caster's colour by strength, the held fit (a patch that moves moves whole texels), a world triangle into the PVR's triangles with the pass's origin, and a box's shadow on an 8 m patch through the whole route against an exact ray at three suns (0.03 % to 0.3 % of ground points wrong). The pass's fill is the test's own, not the PVR's.
- `test/dofmip_test.c`: the sharp zone gives no blur, the blur rises away from it on each side to the strength and no further, the lens law at infinity and at half the focus distance, reach, a near and a far strength of their own, the curve (every power from 0.1 to 50 against libm, the held ends), the strength clamp and the near end's hold, every refused input, the codes (quarters past none, 4 .. 15) and their buckets, the combine, the depth.
- `test/fakeaf_test.c`: the default table and the table from a floor's numbers (starts a square root of two apart), the shares of the ranges adding to 1, the 3V alpha and edges and cuts, the piece's ranges, the bias code's limits and steps, the stretched copies, the ramp texels, the fill model.
- `test/ground_test.c`: the same seed gives the same texels, another seed others, the x and y seams no harder than inside the tile, 24 tones or more, the red mean kept within 15 % of the base.

## Real hardware notes

What an emulator forgives and a real Dreamcast does not. Each of these was seen on a console while an emulator showed the picture right, so test on hardware (or a GDEMU) before you call an effect done.

- **A triangle with no screen area.** On a real console the PVR drew triangles with no area (three points on a line, two the same) as short bright lines; an emulator drew nothing. A fan, ring or strip has them where the camera looks along a spoke, and a mesh with a centre band at radius 0 has them always. Drop them before you send: `sh4der_tri_area_ok` in `sh4der_guard.h` (the area of the triangle in x / z, y / z; a triangle with a point behind the near plane is left to your clipper).
- **Compile a polygon header before any quad.** Every quad, a full-screen filter quad as much as a model, needs a header filled in by `pvr_poly_compile` first. A header that never was is all zeros, and its command word 0 is "end of list" to the tile accelerator, in the middle of the list. An emulator carries on; a real console can stop. If a header is set up in one code path (a level start, say) and a quad is drawn from another (a cutscene), check that both compile it. `sh4der_poly_hdr_ok` in `sh4der_guard.h` tests a header's command word: use it in a debug assert where the header is sent.
- **Aligned access only.** A 4 byte load or store (a `float`, an `int`, a 32 bit colour) on an address that is not a multiple of 4 is an address error on the SH-4: the console stops with nothing on screen. An emulator reads it anyway. Never cast a byte pointer, a disc buffer or a packed struct field to a wider pointer; copy with `memcpy` or read the bytes. Store queue and SIMD addresses want 32 byte alignment. The calls here that write 32 bit values through a byte stride document the alignment they need (see "What a caller must keep").

### Guard tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_GUARD_MIN_AREA2 | 1.0e-7 | Twice the area of the triangle in (x / z, y / z) below which `sh4der_tri_area_ok` says drop: about 0.01 square pixel at 640 x 480 with a 90 degree field, so only a triangle with no area at all |

## What a caller must keep

- **Aligned colours.** The calls that write 32-bit colours or floats through a byte stride (`sh4der_bloom_argb`, the blade and holo colour writers, SH4DOW-DC's `sh4dow_shade`) need the stride to be a multiple of 4 and the array 4-byte aligned. The SH-4 stops on a misaligned 32-bit store.
- **Sized arrays.** A call that takes a count writes that many entries and no more; a count outside its documented range writes nothing and returns 0 where it returns a count.
- **Numbers.** Angles, gains and sizes that are not finite are treated as "no effect this frame" (a cut for the TAA and the trail), not carried into the next frame.

## PVR glossary

Plain meanings of the terms the sections above use.

- **TA (tile accelerator).** The front end of the PVR. The CPU sends it polygons as vertex commands, and it sorts them into per-tile lists for the renderer.
- **OP list (opaque polygons).** The list for solid polygons. The depth buffer sorts them, so the order you send them in does not matter.
- **TR list (translucent polygons).** The list for blended polygons (additive light, alpha). The PVR sorts them per pixel, so there is no hand sorting. Most effects here draw in this list.
- **PT list (punch-through polygons).** The list for polygons whose texture pixels are either fully solid or fully absent (a 1-bit mask). The motes effect uses it.
- **Punch-through.** A texture pixel that is either drawn solid or skipped, with no blending in between.
- **EOL (end of list flag).** A bit on the last vertex of a strip that tells the TA the strip ends there. A vertex without it makes the next vertices join the same strip.
- **Twiddled texture.** A texture whose texels are stored in a Z-order (interleaved bits of x and y) instead of row by row, which is how the PVR reads fast. The normal map textures here are twiddled.
- **VQ (vector quantised) texture.** A compressed texture: a small codebook of 2 x 2 texel blocks and one byte a block that picks one. The spark atlas (64 x 64) is 3 KB this way.
- **Stride texture.** A texture read row by row at a set width instead of twiddled, which a render target needs. The **stride register** is the PVR's single setting for that width, so every stride texture of a program shares it (`sh4der_ca.h` sets it).
- **Direct render.** Sending vertices straight to the TA through the store queues, with no copy in RAM first. The fastest path for small polygons.
- **Store queue.** Two 32-byte write buffers in the SH-4 that burst a whole 32-byte line to a hardware address (the TA or VRAM). Direct render uses them, so nothing else may use them between a list's begin and finish.
- **Front buffer.** The picture the video output is showing right now. Reading or copying it (`sh4der_trail.h`, `sh4der_frame.h`) gives the frame before without any texture memory.
- **XMTRX.** The SH-4's second bank of four-by-four matrix registers. Load a matrix into it once, then each vertex is one FTRV. Nothing else that uses XMTRX may run in between.
- **FTRV.** The SH-4 instruction that multiplies a four-element vector by the matrix in XMTRX in one go: a whole vertex transform.
- **FSRRA.** The SH-4 instruction for a fast reciprocal square root (1 / sqrt(x)). It makes a vector normalise cheap; SH4ZAM wraps it.
- **SH4DOW-DC.** The shadow and soft-edge helper library, part of this library (`include/sh4dow_dc.h`). The effects use its column feather (a soft edge: full inside, fading to nothing outside).
- **`hand:` in a comment.** Marks a line of arithmetic written by hand because SH4ZAM has no equivalent op for that exact step (a scalar product, a constant scale, a compare). The comment says why. Every other maths step goes through SH4ZAM.

## Credits and license

SH4DER-PVR is by Jilted Valkyrie, 2026. GitHub: [JiltedValkyrie](https://github.com/JiltedValkyrie). The copyright line to show in a client's credits: `SH4DER-PVR Copyright (c) 2026 Jilted Valkyrie`.

SH4DOW-DC, by the same author, is folded into this library. Origin: began as SH4DOW-DC, its own library. The licence is one, the terms of this file; the text SH4DOW-DC came with stays at [sh4dow-dc/LICENSE](sh4dow-dc/LICENSE) (the line naming a game was removed on 2026-11-26).

Dreamcast math: [SH4ZAM](https://github.com/gyrovorbis/sh4zam), Copyright (c) 2025, 2026 Falco Girgis, MIT. Keep that notice with the library.

Dreamcast kernels: [SH4ZAM-DCE](third_party/sh4zam-dce) (based on SH4ZAM by Falco Girgis; **not affiliated with, endorsed by or maintained by Falco Girgis or the SH4ZAM project: report problems with it here, not to them**), MIT, its LICENSE in the folder. It is based on SH4ZAM because it uses SH4ZAM as its library and adds the batch and array kernels SH4ZAM does not have; where SH4ZAM has a routine it only wraps it. Included whole and never edited here; the ocean demo uses `shz_dce_project_vertices` and `shz_dce_vec3_normalize_safe`. Keep that notice with the library.

QR codes: the QR Code generator library (C) by Project Nayuki, MIT License (https://www.nayuki.io/page/qr-code-generator-library). The QR test set (`include/sh4der_qrpage.h`) uses it as a library; its two files are not in this repository (`demo/psm/tools/fetch.py` downloads them, sha256 checked), and its copyright and permission notice stays in them.

Drawing: [KallistiOS](https://github.com/KallistiOS/KallistiOS), under its own license.

The license is in [LICENSE](LICENSE). SH4DOW-DC is under the same terms, one licence now; [sh4dow-dc/LICENSE](sh4dow-dc/LICENSE) keeps the text SH4DOW-DC came with (the line naming a game was removed on 2026-11-26).

## Sunset ocean research demo

`make ocean SH4ZAM=../sh4zam MKDCDISC=<mkdcdisc>` builds the separate sunset
scene. `make demo` still builds the original gallery.
See [demo/OCEAN.md](demo/OCEAN.md) for method, controls, costs and validation.
The demo has a PRESET row (QUALITY, the original look; PERFORMANT: SH4DOW off, a
2,000 triangle water mesh), a WATER TRIANGLES row (114 live mesh sizes in steps of
200 triangles, with the triangle and vertex counts, CPU and render milliseconds and
TA use on the readout), SUN AZIMUTH / SUN ELEVATION rows (the water, transmission,
SH4DOW and reflection follow the sun) and the camera height on LT / RT.

## Torch walk demo

`demo/torchwalk_ab.c`, `make torchwalk-ab SH4ZAM=...` (KallistiOS environment, python and mkdcdisc as for `make demo`; the file and target names stay from the A/B demo it grew out of): a first-person walk (left stick walks, right stick looks with the second stick inverted, X turns the torch on and off) over a dark floor and four walls, to see the normal map (highlight power 2, see above) lit by the torch. The R trigger fires: a flickering point light at the muzzle (6 m reach) that the cells lean to, and the base is lit by it.

A lit cell takes up to eight draws: the flat level, up to six grey passes (three squares, three products), the picture. Video memory is nine 8-bit maps a surface (36 KB at 64 x 64, 144 KB at 128 x 128), nothing for the normal map itself (its normals stay in RAM). A label at the bottom right reads out what it costs: the draws, lit cells, TA bytes the passes sent last frame, the video memory, CPU / render / frame ms.

The costs are from the code (the demo's label measures them on a console); fill is the cost to read: eight layers over the lit cone. Still open here: the signs of the axes in the products and the 565 palette's banding. NORMAL MAP FILTER is bilinear by default for the grey maps (the picture pass is always bilinear); the grey maps have no wrap seam, the nearest filter is a row away.

Rows (D-pad up / down picks a row, left / right / A changes it, Y hides the readout): NORMAL MAP, TORCH, FLOOR (the relief painted into the picture, or a clean picture with the relief put back by the normal map), AZIMUTH (default y down, the frame flipped), GRAIN, VIGNETTE (the lens module), TORCH EDGE, TORCH REACH, TORCH CORE, NORMAL MAP AREA, NORMAL MAP FADE and NORMAL MAP RANGE (the normal map fades out and stops at this distance: 5 m by default, 4 to 24 m; 1 world unit is taken as 3 m), WALLS NORMAL MAP, NORMAL MAP EDGE SOFT, NORMAL MAP FILTER, WIREFRAME and the rows of the tessellation methods. The readout shows the CPU, frame and render times, the effect's own cost and its cost against the effect off (START measures), the TA buffer, and the video memory the effect holds apart from the base.

The textures are made in code (octagon panels, grooves, vent slits and a rib, and a wall of panels), so the program runs from nothing. `TORCHWALK_STAGE=<folder>` puts that folder's files on the disc; a `TWTEX.BIN` in it (textures in the PVR formats; header and entry layout in `load_private` in the demo) replaces the generated ones. The file is never part of this repository. The demo is not a game and has no tests.

## Credits

This library is a collaboration between the mind of Jilted Valkyrie, her visual effects industry experience, her sleepless nights researching Dreamcast hardware, and, with the assistance of the Claude Squad: Lead, Senior, Junior, Scholar, Parity, Gouache, and Cortana; for their expertise in contextual application and all of their time spent debating about the direction and application with each other.
