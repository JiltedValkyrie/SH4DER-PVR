# SH4DER-PVR: how to call it

This folder is the whole library, with SH4ZAM (the maths) and KallistiOS (the drawing code) as its outside dependencies. SH4DOW-DC is included (`sh4dow-dc`, its `include` on the path): the soft shadows, and the column feather every effect uses.

There is no game header and no game data here. The example client is `example/blade.c`: the blade effect on a glowing blade model. The holo effect is for a hologram's chest. Where the tuning lives: each header's `SH4DER_*` macros, with their default values in the tuning tables of `README.md`. Keep tuning values in the client, and change the maths here, once.

## Files

- `include/sh4der_core.h`: the shared parts:
  - the SH4ZAM switch and helpers (inline)
  - the feather (SH4DOW-DC's)
  - the tube and blocker shapes
  - the breath tracker and the glint
- `src/sh4der_core.c`: the breath tracker and the glint.
- `include/sh4der_blade.h`, `src/sh4der_blade.c`: the blade effect (tubes, blocker, pulse). `sh4der_blade_intensity` is inline.
- `include/sh4der_holo.h`, `src/sh4der_holo.c`: the holo effect (the breathing core table). `sh4der_holo_argb` is inline.
- `include/sh4der_look.h`: the look effect (frame read, two-way eye, grade layers, fog glare). Header only, all inline.
- `include/sh4der_frame.h`, `src/sh4der_frame.c`: the frame read (the grid, the frame stamp and its history, the gain taken off; the .c is the scan-out read and the stamp block, Dreamcast only).
- `include/sh4der_taa.h`: temporal AA maths (jitter, history weight, pan shift). Header only.
- `include/sh4der_glow.h`, `src/sh4der_glow.c`: screen bloom from a small bright map of the frame (darks die, brights stay), a wide halo, a darken layer balanced so a neutral scene keeps its mean, god rays as scaled copies about the sun. The .c is KOS PVR drawing, Dreamcast only.
- `include/sh4der_warp.h`, `src/sh4der_warp.c`: the frame as a texture through a mesh: shockwave ring, heat haze column, underwater wobble; radial speed blur as scaled copies; pixelate and ordered dither. The .c is KOS PVR drawing, Dreamcast only.
- `include/sh4der_trail.h`, `src/sh4der_trail.c`: the frame before over this one through the front-buffer copy (a column mask in destination alpha, two passes a copy): motion blur by speed, and the temporal AA blend. The .c is KOS PVR drawing, Dreamcast only.
- `include/sh4der_lens.h`, `src/sh4der_lens.c`: overlays with no frame read: a vignette mesh, mean-neutral film grain, hit direction wedges and a full-screen flash. The .c is KOS PVR drawing, Dreamcast only.
- `include/sh4der_focus.h`, `src/sh4der_focus.c`: fake depth of field: a small blurred map of the frame drawn through a vertex-alpha mask that keeps a disc, band or rect sharp. The .c is KOS PVR drawing, Dreamcast only.
- `include/sh4der_visor.h`, `src/sh4der_visor.c`: visor looks as number tables (night, amber, thermal): a tint through the look grade, grain, scanlines, a luma ramp for thermal, a fade in and out. The .c is KOS PVR drawing, Dreamcast only.
- `include/sh4der_torch.h`, `src/sh4der_torch.c`: a flashlight hotspot carved on screen: a radial mesh darkens outside the circle and boosts a hot core, the fog table bends to black for reach, gated by the level's darkness with hysteresis. The .c is KOS PVR drawing, Dreamcast only.
- `include/sh4der_guard.h`: real hardware guards: `sh4der_tri_area_ok` (a triangle with no screen area is dropped before it is sent) and `sh4der_poly_hdr_ok` (a polygon header that was never compiled is refused). Header only, all inline; README "Real hardware notes".
- `include/sh4der_bump.h`: the PowerVR's own normal map pass, its bump unit (header only, the Dreamcast part under `_arch_dreamcast`): `sh4der_bump_from_height` / `sh4der_bump_texel` pack a normal map texture (KallistiOS pvrtex layout), `sh4der_bump_oargb` / `sh4der_bump_oargb_kos` pack the light into the offset colour, `sh4der_bump_header_overlay` / `_pass` / `_apply` build the headers, `sh4der_bump_header_core` the hot pass (a grey map of Nz, DESTCOLOR x ONE), `sh4der_bump_submit_fan` sends a convex polygon as a strip. README "The normal map effect"; `demo/torchwalk_ab.c` uses it.
- `include/sh4der_sss.h`: back-lit transmission (the library's sub-surface scattering stand-in): light through a thin part of a surface toward the eye, tinted by absorption over a thickness the caller supplies. Header only, all inline. The water module's `sh4der_water_crest_sss` is a thin wrapper over it. README "The sss effect"; "Call order: sss" below.
- `include/sh4der_uvwarp.h`: per-vertex UV warp: `sh4der_uvwarp_sample` (a w x h table of byte pairs, bilinear, both axes wrap) and `sh4der_uvwarp_offset` (weight x table at (u + su t, v + sv t)), plus the prepared form for a mesh loop (`sh4der_uvwarp_prepare` once, `sh4der_uvwarp_offset_p` a vertex: the scroll and size hoisted, no floor call a vertex; `sh4der_uvwarp_offset_pn` the same without the input gates for a loop that checked once); the build tool downsamples the source map, the caller owns the table and the weights. Header only, byte reads only.
- `include/sh4der_spark.h`, `src/sh4der_spark.c`: pooled additive sparks: a fixed pool of 24 short-lived quads that face the camera, a billboard (flash) and a streak (tracer, spark) with a pixel floor, a retreating tail, a fade along the length, gravity, a burst helper and a pool rule that never loses a KEEP quad to a plain one. The header is the pool and the quad maths (inline, host tested); the .c is KOS PVR drawing (one additive header, both guards), Dreamcast only. README "The spark effect"; "Call order: spark" below.
- `include/sh4der_pl4nar.h`, `src/sh4der_pl4nar.c`, `tools/pl4nar_normals.py`: real-time planar reflection: the world mirrored about a plane into a small render target, looked up per vertex by a body near the plane (`sh4der_pl4nar_shade`) or a surface in it (`sh4der_pl4nar_flat`), blended in as one translucent layer whose vertex alpha is Fresnel. The header is the view rows, the lookup, the materials and the split (inline, host tested); the .c is the KallistiOS target, pass and layer header, Dreamcast only; the tool makes smoothed normals for the lookup. README "The pl4nar effect"; "Call order: pl4nar" and "pl4nar in an engine that has no normals" below.
- `include/sh4der_pool.h`: light pools on a plane: a disc (a street lamp's pool, an ellipse for a blob shadow) and a wedge (a headlight's beam) as fans of triangles whose vertex alpha fades to nothing, for ground too coarse to light per vertex. Header only, all inline. README "The pool effect"; "Call order: pool" below.
- `include/sh4der_ao.h`: small-scale occlusion: crease darkening from the angle between faces for a bake, and a screen cavity cue from a small map.
- `include/sh4der_ca.h`, `src/sh4der_ca.c`: the frame texture (the world into a 640 x 480 or 320 x 240 render target: 640 KB or 160 KB) and chromatic aberration through it. A setup with no video memory holds nothing and says how many KB were short. The .c is KOS PVR drawing, Dreamcast only.
- `include/sh4der_shafts.h`, `src/sh4der_shafts.c`: shadow shafts: the frame's dark shapes cast away from a light, from the frame read. The .c is KOS PVR drawing, Dreamcast only.
- `test/core_test.c`, `test/guard_test.c`, `test/uvwarp_test.c`, `test/sss_test.c`, `test/spark_test.c`, `test/pl4nar_test.c`, `test/pool_test.c`, `test/blade_test.c`, `test/holo_test.c`, `test/look_test.c`, `test/taa_test.c`, `test/frame_test.c`, `test/glow_test.c`, `test/warp_test.c`, `test/trail_test.c`, `test/lens_test.c`, `test/focus_test.c`, `test/visor_test.c`, `test/torch_test.c`, `test/ao_test.c`, `test/ca_test.c`, `test/shafts_test.c` and `example/blade.c` run with `make test`. `make test-shz SH4ZAM=...` runs the tests on the SH4ZAM path. `make dc SH4ZAM=...` builds the Dreamcast library. Every target takes `SH4DOW=`.
- `demo/ocean.c`, `demo/ocean_scene.h`, `demo/OCEAN.md`: the sunset ocean demo (`make ocean`), a separate program. `ocean_scene.h` is the scene (water mesh tables and `ocean_scene_update`, sky texture rows `ocean_env_rows`, the sun `ocean_sun` / `ocean_sun_vector`), `ocean.c` the Dreamcast program (KOS direct render, readout, rows) and a host preview. Rows, picked with left / right: PRESET (QUALITY = the original look, default; PERFORMANT = SH4DOW off, 2,016 water triangles), SWELL, RIPPLE, TRANSMISSION, REFLECTION, SH4DOW, BLOOM, WATER TRIANGLES (400 .. 22,800 in steps of 200, then the original 23,040; `ocean_mesh_grid` picks the grid), SUN AZIMUTH, SUN ELEVATION. A adds one step (WATER TRIANGLES: ten); up / down set a toggle or step a row (200 triangles); LT / RT lower / raise the eye (`ocean_eye_set`, 0.8 to 8.0 m). To add a row: an `ROW_` entry and a name in `ocean.c`, its value in `ocean_value`, its change in `ocean_step`, a line in the OCEAN.md controls table; keep the original value the start state (the host hash check in OCEAN.md). `make test-water` and `build/ocean_scene_test` check the scene; `ocean-preview` renders it on the host (`tris=400..22800`, `mesh=0..113`, `eye=0.8..8`, `az=0..10`, `el=0..9`, `performant`).
- `demo/main.c`, `demo/models`, `demo/tools`: the demo disc, one program that shows every effect as a row switched live. `make demo SH4ZAM=...` builds `build/demo/sh4der-demo.cdi` (needs python and mkdcdisc); `make dc DEMO=1` builds it after the library.
- `demo/pl4nar`: the street driving demo, its own program and its own build script (`python demo/pl4nar/build.py --sh4zam ... [--rtt] [--pin]`, or `make pl4nar-demo SH4ZAM=...`): fetch (CC0 art by `fetch_table.json`, sha256 checked, nothing vendored), coupe, Blender cut, bake, make, mkdcdisc. To run it as an agent: source the KallistiOS environment, pass `--rtt` only on a KallistiOS with `pvr_scene_begin_rtt`, never overwrite an image, and report what you measured apart from what you did not. `tools/check_credits.py` must say CREDITS_OK before a change to the art lands. The client code to copy from is `demo/pl4nar/src/demo.c` (the mirror pass, the layer, the pools, the halos).
- `demo/torchwalk_ab.c`: a first-person walk over a dark floor with the torch, to see the normal map pass (`sh4der_bump.h`) and its hot pass lit by it, next to two blend-pass variants of the same light. The L trigger cycles the variant (0 normal map pass, A2 hot pass, B per-axis grey, C highlight power 2), the R trigger fires a point light, a label shows the variant's cost. `make torchwalk-ab SH4ZAM=...` builds `build/torchwalk_ab/sh4der-torchwalk-ab.cdi` (refuses to overwrite); `TORCHWALK_STAGE=<folder>` puts a folder on the disc, and a private `TWTEX.BIN` there replaces the generated textures (never commit it). No tests by design.
- `README.md` is the explanation:
  - the effect table
  - how to add an effect
  - the tuning tables, which a test checks against every header's macros
- `LICENSE` is the license.

On the Dreamcast, the square roots, sines and cosines are FSRRA and FSCA inside [SH4ZAM](https://github.com/gyrovorbis/sh4zam) (0.10 or later). Compile without `SH4DER_CORE_NO_SH4ZAM` and link SH4ZAM. Use SH4ZAM as a library; do not copy its files into this folder. SH4DOW-DC stays whole in `sh4dow-dc`; do not copy its code into the effect files. Keep Falco Girgis's MIT notice with SH4ZAM.

`SH4DER_CORE_NO_SH4ZAM` compiles the same steps as plain C, for a host tool or a game without SH4ZAM. It also sets `SH4DOW_DC_NO_SH4ZAM`.

## Call order: blade

1. Pick the vertices: the material groups (shaders) you chose, in the effect's space, with x along the axis.
2. Call `sh4der_blade_prepare` once per instance per frame. Pass `tick` plus that instance's phase, and `fade` (1 − a cloak level, or 1).
3. Call `sh4der_blade_argb` with the group's colour (its texture's average, brightest channel 1), or call `sh4der_blade_intensity` per vertex in your own loop. Draw the group additively with that colour.
4. `frame.light` is the point-light intensity for that instance, fade included.

`turns`, `passes` and `pulses` are whole numbers so the period closes. A frame that `sh4der_blade_prepare` refused is dark.

## Call order: holo

1. At load, call `sh4der_holo_weights` on the body's bind positions: one byte a vertex.
2. Each frame, feed `sh4der_breath_push(sh4der_breath_raw(chest, pelvis, forward))`. Then call `sh4der_holo_prepare` with that breath and the fade.
3. Per vertex, `sh4der_holo_argb(frame, weight)`. Draw alpha blended, texture × colour.

## Call order: look

1. At load, `sh4der_look_bins`, `sh4der_look_params_default`, `sh4der_look_eye_reset`.
2. Each frame, read the frame on screen on the grid, count `sh4der_look_luma565` bins, take `sh4der_look_log_average`.
3. `sh4der_look_eye_step` with that read and dt gives the gain. Put it in a grade's `exposure`, `sh4der_look_solve`, draw each layer full screen under the HUD (`sh4der_look_argb` colour, blend per kind as README says).
4. Glare: `sh4der_look_glare` on a copy of the client's fog colour and table, then load them.

## Call order: spark

1. At load: load your atlas (cols x rows cells, no alpha; black adds nothing), then `sh4der_spark_init(texture, w, h, format)`. Zero the pool with `sh4der_spark_reset`.
2. On an event: `sh4der_spark_billboard` / `sh4der_spark_streak` / `sh4der_spark_burst` with the sizes, colour, life and cell from your own table. Give `SH4DER_SPARK_KEEP` to what must never be pushed out (a flash, a tracer).
3. Once a frame: `sh4der_spark_update(pool, dt, gravity)`.
4. Inside the translucent list, after the world: fill a `sh4der_spark_view_t` (camera axes and eye in world space, near plane, focal length in pixels, `SH4DER_SPARK_MIN_PX`, `SH4DER_SPARK_INSET`, atlas cols and rows) and a `sh4der_spark_screen_t`, then `sh4der_spark_draw`. A client with its own vertex path calls `sh4der_spark_quad` and tests both triangles with `sh4der_tri_area_ok`.

## Call order: pl4nar

1. At load: `sh4der_pl4nar_target_init(&target, 256, 128)`; one `sh4der_pl4nar_layer_header` a blend you use (additive 1 for paint, 0 for glass and water); a `sh4der_pl4nar_material_t` a surface kind from `sh4der_pl4nar_preset`, then tuned in your own table. A 0 from the init means no video memory: draw no layer.
2. Each frame, before the screen's scene: `sh4der_pl4nar_view_build` with the plane, the main camera and the target's size (field of view: the main one x `SH4DER_PL4NAR_FOV_SCALE` for a body, x 1 for a flat surface). A 0 means the view has no direction: draw no layer this frame.
3. `sh4der_pl4nar_pass(&target, &view, draw, user)`. Your `draw` opens the opaque list, draws the world with the rows already in XMTRX, closes it. In it: low detail, culling reversed, skip what `sh4der_pl4nar_sphere_side` puts under the plane, skip the reflecting surface.
4. In the screen's scene, draw the opaque surface as always and keep its screen vertices.
5. `sh4der_pl4nar_shade_n` over the surface's world positions and unit world normals (or `sh4der_pl4nar_flat_n` for a surface in the plane, then `sh4der_pl4nar_tri_split` on each triangle's three depths). Put `u` x `target.u_scale`, `v` and `sh4der_pl4nar_argb` in a copy of the opaque vertex: same x, y, z.
6. In the translucent list: the layer header (checked with `sh4der_poly_hdr_ok`), then those triangles (each checked with `sh4der_tri_area_ok` or your own signed area test).
7. On a size change or at exit: `sh4der_pl4nar_target_free`, which waits for the PVR first.

The lookups leave the reflection view in XMTRX: load your own matrix again before you transform anything else. The single calls (`sh4der_pl4nar_shade`, `sh4der_pl4nar_flat`) need `sh4der_pl4nar_begin` first and no XMTRX call of any kind between them. Compile `src/sh4der_pl4nar.c` with `-DSH4DER_PL4NAR_HAVE_RTT` on a KallistiOS that has `pvr_scene_begin_rtt`; without it the target is 1024 pixels wide (4 times the video memory at 256 x 128).

## pl4nar in an engine that has no normals (a port from a console without lighting)

The step by step version, with what must be true before the next step and what each failure looks like, is `demo/pl4nar/AGENT_WORKFLOW.md` (reflection, light pools, halos, the report). This section is the summary.

For an agent adding the effect to an engine ported from an older console, where meshes carry positions, texture coordinates and baked colours and nothing else.

Inputs you must find or make:

- **A plane.** The road or floor: unit normal toward the camera's side and `d` with dot(n, x) = d, in the engine's world units after any fixed-point conversion. A world with hills takes the plane under the camera, rebuilt each frame.
- **The main camera** as eye, target and up in world space, its vertical field of view in radians and its near depth. Take them from where the engine builds its view matrix; do not rebuild them from the matrix.
- **World positions** of the reflective vertices as `float[3]` at a stride that is a multiple of 4. Fixed-point vertices are converted once at load, not every frame.
- **Normals.** Bake them at load or at build for the reflective meshes alone (a car body, not the city): `tools/pl4nar_normals.py`, or the same rule in the engine's loader. Rotate them with the object each frame.
- **Which triangles reflect.** A list by material, palette entry or texture page: paint, glass, chrome. Everything else is not drawn in the layer.
- **A second, cheap world draw.** The engine's own draw with its lowest detail, a shorter far distance, no small props, no HUD, and the cull test reversed.

Order of work (each step ends with something to look at):

1. The target and the pass, with the target shown as a quad in a screen corner. It must show the world upside down: the ground's horizon at the top, the sky at the bottom. If it shows the world upright, the mirror is missing; if it shows nothing, the cull is the wrong way.
2. The layer on one mesh with alpha fixed at 255 and the blend at 0. The picture must sit still on the surface as the camera turns.
3. `sh4der_pl4nar_argb` for the alpha, the material's own blend.
4. The material table, tuned on screen: `f0` first, then `gain`, then `ground_keep`.
5. The costs, read from the engine's own timers, at 128 x 64, 256 x 128 and 512 x 256.

Tuning, in the order that matters: the target's size (the mirror pass is the largest cost: start at 256 x 128, go down before anything else); how much of the world the pass draws; `f0` (physical values look weak on flat-shaded art: 0.14 for paint was right in the first client); `reach` (shorter pulls the picture toward the horizon, longer toward what is far up the road); the field of view scale (wider keeps more rays in the picture and costs sharpness).

Cost table to fill in and report, measured or marked estimate, never mixed: target bytes (exact, w x h x 2); lookup ms and vertex count; mirror pass CPU ms (list begin to list finish of your draw alone) and triangle count; the PVR's render time from `pvr_get_stats` and your two waits for it, apart, at each target size (a timer round the whole pass reads the PVR's queue, not the pass); frames a second with the effect off and on; video memory free after load.

Checks: `make test`, `make test-shz SH4ZAM=<checkout>`, `make dc SH4ZAM=<checkout>`, `python tools/pl4nar_normals.py --self-test`. On the host the software back end of SH4ZAM gives the same numbers as this library's plain C path; a console is the only test of the matrix load and of the render target.

Do not: build the view with a chain of matrix calls (see README "Keep the row form"); draw the layer before the opaque surface; write depth from the layer; allocate the target every frame; put an engine's or a game's name in this folder.

## Call order: pool

1. At load: compile two untextured translucent headers, depth test on, depth write off, no culling: one additive (source alpha / one) for light, one blended (source alpha / 1 - source alpha) for shadow.
2. Each frame, in the translucent list, after the ground: for each lamp near the camera `sh4der_pool_disc` (centre on the ground under the lamp, lifted 2 to 4 cm; two axes in the plane with the radius as their length), for each headlight `sh4der_pool_wedge` (apex at the lamp's foot, the car's forward and right axes). Give the lamp's colour and its brightness as alpha.
3. Transform the vertices with your own matrix and send the fan: disc triangle k is 0, k + 1, (k + 1) % n + 1; wedge triangle k is 0, k + 1, k + 2. Test each with `sh4der_tri_area_ok` or your signed area test.
4. A blob shadow is a disc with colour 0, alpha about 0.4, through the blended header, its axes the car's length and width.

Limit lamps by distance: a pool is fill, and fill is what the translucent list costs. For a lamp's halo use a camera-facing quad (`sh4der_spark_quad`); for its reflection on a body draw that quad in the mirror pass (`sh4der_pl4nar_pass`).

## Call order: normal map pass (PVR hardware bump mapping) and the hot pass

Once: make a normal map texture (`sh4der_bump_from_height` into a 32-byte aligned uint16_t array, then `pvr_txr_load`), and the headers (`sh4der_bump_header_overlay`, or `_pass` and `_apply`). Per surface (a floor is cut into cells, one light each): build a `sh4der_bump_frame_t` (world directions of u, v, the normal), take the unit direction to the light (`sh4der_bump_toward`), pack `sh4der_bump_oargb(light, &frame, SH4DER_BUMP_STRENGTH)`. Overlay: draw the lit base on the opaque list, then `sh4der_bump_submit_fan` of the polygon on the translucent list with vertex colour `sh4der_bump_fade_argb(strength)`. Two pass: the polygon on the opaque list with vertex colour 0, then the base on the translucent list under the apply header. Open items on a real console: the azimuth's turn (`frame.flip`) and the K1 term (`sh4der_bump_oargb_kos`).

Hot pass (the torch's hot core on the normal map; it brightens where the overlay only darkens): once, make a grey map of Nz from the map's slope byte (255 x cos of S, `side` x `side`, 8 bit, twiddled like the texture, upload with `pvr_txr_load`), set the grey ramp with `sh4der_bump_hot_palette(bank)`, build `sh4der_bump_header_core(&hdr, map, side, bank, filter)`. Per frame, after the overlay, over the same lit cells: send the header once for a run of cells, then each cell's polygon (`sh4der_bump_send_fan`, offset colour 0) with the vertex colour at each corner = the cell's strength x boost x the torch's tint (`sh4der_bump_hot_argb`). The blend is the library torch's hot core (DESTCOLOR x ONE: frame x (1 + the map x the colour)): bright texels of the map blow out first, dark ones stay. Cost: one more pass over the lit cells (the vertices sent again, 32 B each, the header once per run), 4 KB of video memory for a 64 x 64 map, nothing in RAM beyond the build buffer. Not checked on a real console.

## Call order: sss (transmission)

`sh4der_sss.h` is the library's sub-surface scattering stand-in, also called transmission: light that comes through a thin or translucent part of a surface toward the eye. It works for any material where light scatters inside the body: skin, ears and fingers, fruit, jelly, honey, cheese, milk, wax, marble, jade, ice, leaves, cloth, paper, and water. Water is one client of it, not the point of it. It is an artistic proxy, per vertex. It is NOT a multiple-scattering solve: no diffusion profile, no blur across the surface. It draws nothing by itself; it returns an RGB for you to add.

`out = tint x exp(-absorption x path) x entry x coverage x back x visibility x gain`

1. At load, fill one `sh4der_sss_t` per material with `sh4der_sss_init(&p, tint, absorption, gain, wrap, distortion)`.
   - `tint`: the colour of the light that enters. Usually the light's own colour; the material's colour comes from `absorption`.
   - `absorption`: loss per unit path, per channel. The channel absorbed least is the colour the material glows: red absorbed least gives skin, wax, honey and candle flame; green absorbed least gives leaves and grapes; blue absorbed least gives ice and deep water.
   - `wrap`: 0 lights only what has the light straight behind it; 0.3 to 1 also lights side-on, which suits soft, strongly scattering bodies (skin, leaves, milk, wax).
   - `distortion`: 0 sends the light straight through; 0.1 to 0.5 bends it along the normal on curved thin shapes (ears, fruit, grapes).
   - `gain`: overall strength.
2. Each frame, take the light direction (unit, from the surface to the light). Per vertex take the view direction (unit, from the surface to the eye) and the normal.
3. Per vertex, supply the four weights and call `sh4der_sss_rgb`:
   - `path`: your thickness proxy, in the unit of 1 / absorption (a path of 1 passes `exp(-absorption)` of a channel). A baked per-vertex thickness is the usual source: the distance through the body along the inward normal, the way an occlusion bake is made. Or a height above a trough, a distance in from an edge, or one constant per material.
   - `coverage` 0..1: how much of this vertex is thin enough to pass light (an ear rim 1, a thick torso 0).
   - `entry` 0..1: the light that enters at all; 1 minus a Fresnel term for water, glass or a glossy skin, 1 if you have none.
   - `visibility` 0..1: whether the light reaches the surface; a shadow term (SH4DOW-DC's heightfield sun visibility fits), 1 if you have none. 0 gives exactly 0.
4. Put the result in the PVR offset colour (the specular add of a textured header: `gen.specular = 1` in `pvr_poly_cxt_t`), or add it to the vertex colour. Clamp it to 0..1 when you pack it. The offset colour is added after the texture, so the glow shows on a dark texture too.
5. For any material, call `sh4der_sss_rgb` directly with your own `path` and `coverage`. Water has a ready wrapper, `sh4der_water_crest_sss(&p, view, light, normal, crest, thin_path, thick_path, entry, visibility, out)`, with crest 0..1 as coverage and the path running from `thick_path` (crest 0) to `thin_path` (crest 1); it is only a convenience over the same call.

Cost: VRAM 0, AICA 0, RAM the 40-byte `sh4der_sss_t`. CPU per vertex: two 3-term dots (one with distortion 0), three `shz_expf`, about 12 multiplies or FMAC. No allocation, no globals. SH4ZAM does the maths; `SH4DER_CORE_NO_SH4ZAM` compiles the plain C path. `make test` runs `test/sss_test.c`.

### Starting points per material

Invented starting values for a tint of the light's own colour and a path near 1, to tune by eye. They are not measured and not from reference data.

| Material | absorption (r, g, b) | wrap | distortion | Notes |
|---|---|---|---|---|
| Skin (ear, fingers, cheeks) | 0.5, 1.8, 3.0 | 0.5 | 0.3 | Path from a thickness bake; gain 0.4 to 0.8. |
| Leaf, thin foliage | 1.2, 0.3, 1.8 | 0.8 | 0.2 | Coverage 1 on the blade, a short constant path. |
| Grape, lime, green fruit | 1.5, 0.4, 1.2 | 0.4 | 0.3 | Thickness bake; the rind stays opaque (coverage 0). |
| Orange, citrus slice, gummy | 0.3, 1.2, 3.0 | 0.3 | 0.4 | Slices are thin: a short path glows bright. |
| Honey, amber, jelly | 0.4, 1.0, 3.5 | 0.2 | 0.4 | Deeper colour as the path grows. |
| Milk, cheese, wax, candle | 0.3, 0.4, 0.9 | 0.6 | 0.2 | Strong scatterers: high wrap, soft glow. |
| Marble, alabaster | 0.5, 0.6, 0.8 | 0.7 | 0.1 | Near-neutral; a faint warm edge. |
| Jade | 1.4, 0.4, 0.9 | 0.5 | 0.2 | Green glow, thick path. |
| Ice, glacial snow | 1.0, 0.25, 0.15 | 0.3 | 0.3 | Blue at depth. |
| Cloth, paper, lantern | 0.2, 0.3, 0.5 | 0.4 | 0.1 | Coverage 1, a small constant path, the lamp behind. |
| Water crest | see `sh4der_water.h` | 0 | 0 | The ocean demo's values. |

### Worked examples: making it work on a real model

Three jobs make any of the materials above work: a thickness for each vertex (`path`), a mask for where the material is thin (`coverage`), and one call per vertex each frame that writes the offset colour. The code below compiles as written on the host with `-DSH4DER_CORE_NO_SH4ZAM`; on the console `pvr_vertex_t` is KallistiOS's and `oargb` is its offset colour.

**1. Bake the thickness, once, in your model tool.** For each vertex, shoot a ray inward along the negative normal and take the distance to the first hit on the model's own triangles. That distance is how much material the light crosses. Cap it, so a vertex that hits nothing counts as thick. Store one float (or one byte scaled to the cap) per vertex next to the vertex data. In Blender the same thing comes from an Ambient Occlusion node with Inside switched on and the distance set to your cap, baked to a vertex colour.

```c
typedef struct { float p[3]; float n[3]; } bvert_t;

static int ray_tri(const float o[3], const float d[3], const float a[3], const float b[3], const float c[3], float *t) {
    float e1[3] = { b[0]-a[0], b[1]-a[1], b[2]-a[2] }, e2[3] = { c[0]-a[0], c[1]-a[1], c[2]-a[2] };
    float h[3] = { d[1]*e2[2]-d[2]*e2[1], d[2]*e2[0]-d[0]*e2[2], d[0]*e2[1]-d[1]*e2[0] };
    float det = e1[0]*h[0] + e1[1]*h[1] + e1[2]*h[2], s[3], q[3], u, v;
    if (fabsf(det) < 1e-9f) return 0;
    det = 1.0f / det;
    s[0] = o[0]-a[0]; s[1] = o[1]-a[1]; s[2] = o[2]-a[2];
    u = (s[0]*h[0] + s[1]*h[1] + s[2]*h[2]) * det;
    if (u < 0 || u > 1) return 0;
    q[0] = s[1]*e1[2]-s[2]*e1[1]; q[1] = s[2]*e1[0]-s[0]*e1[2]; q[2] = s[0]*e1[1]-s[1]*e1[0];
    v = (d[0]*q[0] + d[1]*q[1] + d[2]*q[2]) * det;
    if (v < 0 || u + v > 1) return 0;
    *t = (e2[0]*q[0] + e2[1]*q[1] + e2[2]*q[2]) * det;
    return *t > 1e-4f;
}

void bake_thickness(const bvert_t *v, unsigned nv, const unsigned *idx, unsigned ntri, float max_t, float *out) {
    for (unsigned i = 0; i < nv; ++i) {
        const float in[3] = { -v[i].n[0], -v[i].n[1], -v[i].n[2] };
        float best = max_t;
        for (unsigned t = 0; t < ntri; ++t) {
            float hit;
            if (ray_tri(v[i].p, in, v[idx[3*t]].p, v[idx[3*t+1]].p, v[idx[3*t+2]].p, &hit) && hit < best) best = hit;
        }
        out[i] = best;
    }
}
```

**2. Paint a thin mask.** One byte per vertex: 255 where the material is thin enough to glow (an ear rim, an orange slice's flesh, a leaf blade, the fingertips), 0 where it is solid or opaque (a rind, a torso, a metal fitting). A vertex-colour channel in your modelling tool is enough. Use it as `coverage`.

**3. Each frame, one call per vertex.** `path` is the baked thickness times a scale that puts it in the unit of 1 / absorption (the table's values suit a thickness of 0 to about 1; scale a larger model down). Skin, as the example:

```c
static uint32_t pack_offset(const float rgb[3]) {
    unsigned r = (unsigned)(sh4der_sat_(rgb[0]) * 255.0f + 0.5f), g = (unsigned)(sh4der_sat_(rgb[1]) * 255.0f + 0.5f), b = (unsigned)(sh4der_sat_(rgb[2]) * 255.0f + 0.5f);
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

static sh4der_sss_t skin;
void skin_init(void) {
    const float tint[3] = { 1.0f, 0.95f, 0.85f }, absorb[3] = { 0.5f, 1.8f, 3.0f };
    sh4der_sss_init(&skin, tint, absorb, 0.6f, 0.5f, 0.3f);
}

void skin_frame(pvr_vertex_t *out, const bvert_t *v, const float *thick, const uint8_t *thin_mask,
                unsigned nv, const float eye[3], const float light[3]) {
    for (unsigned i = 0; i < nv; ++i) {
        float view[3] = { eye[0]-v[i].p[0], eye[1]-v[i].p[1], eye[2]-v[i].p[2] }, rgb[3];
        float len2 = view[0]*view[0] + view[1]*view[1] + view[2]*view[2], inv;
        inv = 1.0f / sqrtf(len2 > 1e-12f ? len2 : 1e-12f);
        view[0] *= inv; view[1] *= inv; view[2] *= inv;
        sh4der_sss_rgb(&skin, view, light, v[i].n, thick[i] * 2.0f, thin_mask[i] * (1.0f / 255.0f), 1.0f, 1.0f, rgb);
        out[i].oargb = pack_offset(rgb);
    }
}
```

The tuning that makes the same loop a different material is the `sh4der_sss_init` line, so keep one `sh4der_sss_t` per material and pick it per vertex group:

- **An orange slice or a gummy.** Table row "Orange, citrus slice": `absorb = {0.3, 1.2, 3.0}`, wrap 0.3, distortion 0.4. The thin mask is the flesh; the rind is 0 so it stays opaque. A short `path` (a slice is thin) makes it glow bright; a thick wedge glows deeper and redder.
- **A grape or a lime.** Table row "Grape": `absorb = {1.5, 0.4, 1.2}`, so green passes. Distortion 0.3 bends the light along the curved skin; the skin ring is 0 in the mask.
- **A candle or cheese.** Table row "Milk, cheese, wax, candle": a small absorption and a high wrap (0.6) give the soft, whole-body glow instead of a hard rim; the flame is the light, so pass its direction.
- **A leaf.** Table row "Leaf": coverage 255 on the blade, a short constant `path`, a sun behind the canopy.
- **Moving light.** Recompute `light` each frame (a swinging lamp, a sun on the move) and the glow follows with no rebake; only the thickness and the mask are fixed.

Check the result by looking at the thin part against the light, then against a front-lit view: the glow must fade to nothing as the light moves in front. If it never fades, `view` or `light` points the wrong way (view runs from the surface to the eye, light from the surface to the light; a light direction that runs from the light to the surface has the wrong sign); if it never shows, `path` is too large for your `absorption`.

### Where it looks best on the PVR

This is guidance from how the maths behaves, not a measurement; check it on your own scene.

- **Light behind a thin or small subject, seen from the front.** `back` is full only when the light is behind the surface as the eye sees it, so it belongs to dusk, dawn, a rim light, a window or a lamp behind the subject. Front-lit it gives 0 by design.
- **Characters.** An ear against the sun, fingers over a flame, a cheek and a nose-wing in a rim light. Wrap 0.3 to 1 and a thickness bake. This is the biggest win for a lit face at low polygon counts.
- **Food and props.** A held grape, a lime wedge, an orange slice, jelly, honey, a candle, a cheese rind cut open, a glass of milk. A short path and a warm lamp behind.
- **Stone and ice.** A marble bust against a window, jade, alabaster, a block of ice in a low sun. High wrap, a thick path.
- **Foliage and cloth.** Leaves against the sky, curtains, banners, sails, paper lanterns. Coverage 1 and a constant path.
- **Water.** Wave crests against a low sun (the library's ocean demo): crest height as coverage, a short path at the crest.
- **Needs vertices where the effect varies.** The RGB is per vertex and the PVR interpolates it across each triangle. A thin ear or an orange slice on three large triangles smears the glow; give the thin part enough vertices, or bake the thickness there.
- **Not for thick opaque bodies**, front-lit scenes, or flat-shaded low-poly where one vertex colour covers a big face. Use `sh4der_ao.h` or the glow for those.
- **Pair it with a shadow.** Without `visibility`, transmission shines through a shadowed surface. Pass the shadow term, or the effect lights things the light cannot reach.
