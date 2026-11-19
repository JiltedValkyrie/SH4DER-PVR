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
- `include/sh4der_sss.h`: back-lit transmission (the library's sub-surface scattering stand-in): light through a thin part of a surface toward the eye, tinted by absorption over a thickness the caller supplies. Header only, all inline. The water module's `sh4der_water_crest_sss` is a thin wrapper over it. README "The sss effect"; "Call order: sss" below.
- `include/sh4der_ao.h`: small-scale occlusion: crease darkening from the angle between faces for a bake, and a screen cavity cue from a small map.
- `include/sh4der_ca.h`, `src/sh4der_ca.c`: the frame texture (the world into a 640 x 480 or 320 x 240 render target: 640 KB or 160 KB) and chromatic aberration through it. A setup with no video memory holds nothing and says how many KB were short. The .c is KOS PVR drawing, Dreamcast only.
- `include/sh4der_shafts.h`, `src/sh4der_shafts.c`: shadow shafts: the frame's dark shapes cast away from a light, from the frame read. The .c is KOS PVR drawing, Dreamcast only.
- `test/core_test.c`, `test/guard_test.c`, `test/sss_test.c`, `test/blade_test.c`, `test/holo_test.c`, `test/look_test.c`, `test/taa_test.c`, `test/frame_test.c`, `test/glow_test.c`, `test/warp_test.c`, `test/trail_test.c`, `test/lens_test.c`, `test/focus_test.c`, `test/visor_test.c`, `test/torch_test.c`, `test/ao_test.c`, `test/ca_test.c`, `test/shafts_test.c` and `example/blade.c` run with `make test`. `make test-shz SH4ZAM=...` runs the tests on the SH4ZAM path. `make dc SH4ZAM=...` builds the Dreamcast library. Every target takes `SH4DOW=`.
- `demo/main.c`, `demo/models`, `demo/tools`: the demo disc, one program that shows every effect as a row switched live. `make demo SH4ZAM=...` builds `build/demo/sh4der-demo.cdi` (needs python and mkdcdisc); `make dc DEMO=1` builds it after the library.
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

## Call order: sss (transmission)

`sh4der_sss.h` is the library's sub-surface scattering stand-in, also called transmission: light that comes through a thin part of a surface toward the eye. It is an artistic proxy, per vertex. It is NOT a multiple-scattering solve: no diffusion profile, no blur across the surface. It draws nothing by itself; it returns an RGB for you to add.

`out = tint x exp(-absorption x path) x entry x coverage x back x visibility x gain`

1. At load, fill one `sh4der_sss_t` per material with `sh4der_sss_init(&p, tint, absorption, gain, wrap, distortion)`.
   - `tint`: the colour of the light that enters (the light's own colour, say).
   - `absorption`: loss per unit path, per channel. A larger red than blue keeps the colour warm at a short path and moves it toward blue-green as the path grows; the other way round gives the red shift of skin, wax and leaves.
   - `wrap`: 0 lights only what has the light straight behind it; 0.3 to 1 also lights side-on (skin, leaves).
   - `distortion`: 0 sends the light straight through; 0.1 to 0.5 bends it along the normal on curved thin shapes.
   - `gain`: overall strength.
2. Each frame, take the light direction (unit, from the surface to the light). Per vertex take the view direction (unit, from the surface to the eye) and the normal.
3. Per vertex, supply the four weights and call `sh4der_sss_rgb`:
   - `path`: your thickness proxy, in the unit of 1 / absorption. A baked per-vertex thickness, a height above a trough, a distance in from an edge, or one constant per material.
   - `coverage` 0..1: how much of this vertex is thin enough to pass light.
   - `entry` 0..1: the light that enters at all; 1 minus a Fresnel term for water or glass, 1 if you have none.
   - `visibility` 0..1: whether the light reaches the surface; a shadow term (SH4DOW-DC's heightfield sun visibility fits), 1 if you have none. 0 gives exactly 0.
4. Put the result in the PVR offset colour (the specular add of a textured header: `gen.specular = 1` in `pvr_poly_cxt_t`), or add it to the vertex colour. Clamp it to 0..1 when you pack it. The offset colour is added after the texture, so the glow shows on a dark texture too.
5. Water has a wrapper: `sh4der_water_crest_sss(&p, view, light, normal, crest, thin_path, thick_path, entry, visibility, out)`, with crest 0..1 as coverage and the path running from `thick_path` (crest 0) to `thin_path` (crest 1).

Cost: VRAM 0, AICA 0, RAM the 40-byte `sh4der_sss_t`. CPU per vertex: two 3-term dots (one with distortion 0), three `shz_expf`, about 12 multiplies or FMAC. No allocation, no globals. SH4ZAM does the maths; `SH4DER_CORE_NO_SH4ZAM` compiles the plain C path. `make test` runs `test/sss_test.c`.

### Where it looks best on the PVR

This is guidance from how the maths behaves, not a measurement; check it on your own scene.

- **A thin shape seen against a low light.** `back` is full only when the light is behind the surface as the eye sees it, so the effect belongs to dusk, dawn, a rim light and a lamp or sun behind the subject. Front-lit, it gives 0 by design.
- **Wave crests at sunset.** The library's ocean demo: crest height as coverage, a short path at the crest, the sun's colour as the tint. The crest glows and the trough stays dark.
- **Ears, fingers and thin skin.** Wrap 0.3 to 1, a warm tint, red absorbed least. A light behind the head turns the ear red.
- **Leaves and thin foliage.** Wrap high, a green-yellow tint, path from the leaf's thickness. A sun behind a canopy.
- **Wax, ice, jelly, candles.** A short absorption length so the colour deepens with thickness; use a baked per-vertex thickness for the path.
- **Cloth, curtains, banners, paper lanterns.** Coverage 1, a small constant path, the lamp or sun behind the cloth.
- **Needs vertices where the effect varies.** The RGB is per vertex and the PVR interpolates it across each triangle. A thin crest or an ear on three large triangles smears the glow; give the thin part enough vertices, or bake the thickness there.
- **Not for thick opaque bodies**, front-lit scenes, or flat-shaded low-poly where one vertex colour covers a big face. Use `sh4der_ao.h` or the glow for those.
- **Pair it with a shadow.** Without `visibility`, transmission shines through a shadowed surface. Pass the shadow term, or the effect lights things the light cannot reach.
