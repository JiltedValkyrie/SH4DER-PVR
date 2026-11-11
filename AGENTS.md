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
- `include/sh4der_ao.h`: small-scale occlusion: crease darkening from the angle between faces for a bake, and a screen cavity cue from a small map.
- `include/sh4der_ca.h`, `src/sh4der_ca.c`: the frame texture (the world into a 640 x 480 or 320 x 240 render target: 640 KB or 160 KB) and chromatic aberration through it. A setup with no video memory holds nothing and says how many KB were short. The .c is KOS PVR drawing, Dreamcast only.
- `include/sh4der_shafts.h`, `src/sh4der_shafts.c`: shadow shafts: the frame's dark shapes cast away from a light, from the frame read. The .c is KOS PVR drawing, Dreamcast only.
- `test/core_test.c`, `test/blade_test.c`, `test/holo_test.c`, `test/look_test.c`, `test/taa_test.c`, `test/frame_test.c`, `test/glow_test.c`, `test/warp_test.c`, `test/trail_test.c`, `test/lens_test.c`, `test/focus_test.c`, `test/visor_test.c`, `test/torch_test.c`, `test/ao_test.c`, `test/ca_test.c`, `test/shafts_test.c` and `example/blade.c` run with `make test`. `make test-shz SH4ZAM=...` runs the tests on the SH4ZAM path. `make dc SH4ZAM=...` builds the Dreamcast library. Every target takes `SH4DOW=`.
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
