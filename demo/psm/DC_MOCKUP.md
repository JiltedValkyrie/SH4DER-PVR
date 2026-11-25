# demo/psm on the Dreamcast: mock-up

A mock-up of one disc program that puts the two routes of the psm effect side by side on a console. Layout, flow, controls and the cost label only. No engine code here, nothing built, nothing measured. Every number below is arithmetic from the headers or marked as a guess.

The host demo (`psm_demo.c`) stays as it is. The disc program reuses its scene, not its renderer.

## What it compares

| | Route A: the PVR draws the map | Route B: the SH-4 draws the map |
|---|---|---|
| Header | `sh4der_psm_pvr.h` | `sh4der_psm_raster.h` |
| Map lives in | VRAM, a render target | RAM, `uint16_t` size x size |
| Who fills it | PVR: casters flat dark on white (`sh4der_psm_pvr_tris`) | SH-4: depth (`sh4der_psm_raster_tri`) |
| Lookup | PVR per pixel, ground UV from `sh4der_psm_pvr_uv`, multiply layer | SH-4 per vertex, `sh4der_psm_raster_lit_n`, byte into vertex colour |
| Self-shadow | No: poles and cows are casters, ground is the only receiver | Yes: cows and poles receive too |
| PSM catch | UV exact only at vertices: ground cells must be fine near the camera (`sh4der_psm_pvr_uv_error`) | Shadow edge only as sharp as the receiver's vertex spacing |

And on each route, uniform map against PSM, at the same map size.

## Scene (same as the host demo)

- Ground: a graded grid, not a uniform one. Rows 0.25 m deep near the camera, growing to 2 m at 70 m. One grid serves both routes: route A needs the fine near cells for the PSM lookup, route B needs them to carry the shadow edge. Guess: 3,000 to 5,000 vertices in view.
- Checker on the ground (1 m squares), so a texel's size is readable by eye. A 16 x 16 texture, not maths a pixel.
- 10 poles, 0.3 x 5 m boxes, alternating sides, 4 m to 44 m ahead (12 triangles each).
- 2 Spot cows (CC0, `fetch_table.json`) at 7 m and 15 m. Spot is about 5,800 triangles: too many for the SH-4 raster each frame. A build step cuts it to a low version for the map (guess 400 to 800 triangles) and keeps a medium one for the screen. See question 2.
- Camera: eye 1.7 m, pitched down 7 degrees, 60 degree vertical field of view, near 0.3, far 70 (the host demo's).
- Sun: directional, azimuth and elevation as the host demo's `--az` / `--el` (az 0 = straight toward the camera).

## Screen, 640 x 480

```
+----------------------------------------------------------------------------+
| ROUTE B  SH-4 DEPTH      MAP PSM                         [HEAD-ON]  (flag) |
|                                                                            |
|                                                                            |
|                                                                            |
|              main view, full screen                                        |
|              poles receding left and right, two cows, checker ground       |
|                                                                            |
|                                                                            |
|                                                          +--------------+  |
|                                                          |  map inset   |  |
|                                                          |  128 x 128   |  |
|                                                          |  (debug tex) |  |
|                                                          +--------------+  |
| MAP     256 x 256   B: 128 KB RAM   A: 128 KB VRAM                         |
| WARP    near 4.0 m   kind POINT_FRONT                                      |
| SUN     az 120  el 35                                                      |
| BIAS    a 0.02 m  b 0.0   PCF 2x2                                          |
| TEXEL/PX  4 m 0.9   10 m 1.4   25 m 2.8   60 m 6.1                         |
+----------------------------------------------------------------------------+
| SH-4 12.4 ms  PVR 9.1 ms  TA 6,840 v  VRAM 214 KB  FPS 30       (cost label)|
+----------------------------------------------------------------------------+
```

The numbers in the drawing are placeholders, not results.

- **Top line:** route and map kind in large text, so a photo of the screen says what it shows. `[HEAD-ON]` lights when the warp build returned 0 and the program fell back to uniform (the host demo's "no fit for this sun" case): the text then reads `MAP PSM -> UNIFORM`.
- **Readout (bottom left), on START:** map size and where its bytes are; the warp near and `kind` (UNIFORM, ORTHO, POINT_FRONT, POINT_BEHIND, from `sh4der_psm_t.kind`); sun; bias; texel over pixel at 4, 10, 25, 60 m ahead (`sh4der_psm_texel_m` / `sh4der_psm_pixel_m`, the host demo's line: above 1 shows blocks). The ratio line is the proof of PSM, so it stays on even with the readout off.
- **Map inset (right), on Y:** route B: the RAM depth map as a grey ramp, uploaded to a 128 x 128 debug texture (32 KB VRAM, counted in the label while it is on). Route A: the render target itself, drawn as a quad, no copy. Off by default when timing.
- **Cost label (bottom strip), always on:** see below.

## The cost label

One line, always on, the same five fields on both routes so a pair of photos compares directly.

| Field | Source | Notes |
|---|---|---|
| SH-4 ms | timer round the SH-4 work of the frame: build + caster pass + receiver pass | Route B: build, `raster_begin` (clear), caster fill, receiver compare. Route A: build, caster clip, ground UV. Hold B (button) to split it into three. |
| PVR ms | `pvr_get_stats`, render time | Route A adds the map target's render. Measured apart from the SH-4 wait, not a timer round the whole frame (AGENTS.md: a timer round the pass reads the PVR's queue). |
| TA v | vertices sent this frame, counted by the program | Route A: map pass triangles x 3 + ground layer vertices + the scene. Route B: the scene only (the shade rides in the vertex colour). |
| VRAM KB | `pvr_mem_available()` at load minus now, plus the label's own texture | Route A: the target (256 x 256 x 2 = 128 KB with `pvr_scene_begin_rtt`; 1024 x 256 x 2 = 512 KB without it, the pl4nar rule). Route B: 0 for the map, 32 KB if the inset is on. |
| FPS | frames counted over the last second | Vsync 60 or 30; the label says which the frame hit. |

Each field shows `--` until it has a real sample. Nothing is filled from host numbers.

A held B button opens the split line under it:

```
B: build 0.3  clear 0.4  casters 7.9  receivers 3.8   (ms)   tris 1,040 drawn 812   verts 4,212
A: build 0.3  clip   1.1  uv       1.2               (ms)   map tris 1,388         layer v 4,212
```

`tris` / `drawn` come from `sh4der_psm_raster_t` (route B), `map tris` from the sum of `sh4der_psm_pvr_tris` (route A).

## Controls

| Pad | Does |
|---|---|
| A | Route A / route B |
| X | Uniform / PSM |
| Y | Map inset on / off |
| B (hold) | Split cost line |
| D-pad left / right | Sun azimuth, 5 degree steps (0 = toward the camera, the head-on case) |
| D-pad up / down | Sun elevation, 5 .. 85, 5 degree steps |
| L / R triggers | Warp near, 0.5 m steps, 1 .. 12 m (shown when PSM is on; ignored on uniform) |
| Stick left / right | Camera yaw (the sun stays put, so turning walks the head-on case past) |
| START | Readout on / off |
| START + A | Map size: 64, 128, 256, 512 |
| START + X | PCF 2x2 / one texel (route B) |
| START + D-pad up / down | Bias a, 0.005 m steps |
| START + L / R | Bias b, 0.5 steps |

Two presets on START + Y, cycling: the host demo's default (az 120, el 35); head-on (az 0, el 20: sun straight ahead, low); behind (az 180, el 20). These three are what a reviewer wants photos of.

## Frame flow

Route B:

1. Read the pad, update sun, camera, map size, warp near.
2. Fit points: `sh4der_psm_frustum` (fit near 2 m), `sh4der_psm_slab_pts` (ground slab 0 .. 0.5 m).
3. Build: `sh4der_psm_build_warp`, on 0 `sh4der_psm_build_uniform` and the head-on flag.
4. Key range over the low caster mesh, `sh4der_psm_raster_begin`, `sh4der_psm_raster_tri` per caster triangle.
5. Receivers: ground through `sh4der_psm_raster_lit_n` with the x / z axes (the plane compare); cows and poles with NULL axes (bias a only).
6. PVR scene: opaque list, ground and models, shade x sun term in the vertex colour. HUD, inset, label in the translucent list.

Route A:

1. to 3. as route B.
4. Caster pass: `sh4der_psm_pvr_tris` per caster triangle into the target, culling off, flat dark on a white clear.
5. Screen scene: opaque ground and models lit without shadow; then the ground again in the translucent list, multiply blend, UV from `sh4der_psm_pvr_uv`, depth test on, depth write off, `sh4der_poly_hdr_ok` and `sh4der_tri_area_ok`.
6. HUD, inset, label.

## What it does not show

- Route A does not shadow the cows on themselves or the poles on the cows: by design. The readout says `self-shadow: no` on route A so a photo is not misread.
- No point light. `sh4der_psm.h` is directional only.
- No moving casters. Can come later; the map is rebuilt each frame anyway.

## Open questions

1. **Ground grid.** One graded grid for both routes (fair comparison, route B pays for vertices route A needs), or each route its own grid (route A fine near, route B as coarse as its edge allows)? The mock-up assumes one.
2. **Cows.** Who makes the low Spot: a build step in `demo/psm/tools` (decimate at build, from the fetched file, nothing vendored), or a hand-made low mesh? What triangle budget for the SH-4 map: 400, 800?
3. **RTT.** Require `pvr_scene_begin_rtt` for route A, or also run the 1024-wide fallback and show its VRAM in the label?
4. **Map sizes.** Is 512 on route B worth having (512 KB RAM, cache misses), or cap at 256?
5. **Disc build.** Its own `build.py` like `demo/pl4nar`, or a `make psm-dc SH4ZAM=...` target? Text drawing: reuse the text-to-texture way `demo/torchwalk_ab.c` does its label?
6. **Default boot state.** Route B PSM 256 at the host demo's sun, or route A uniform (the plain baseline) so the first toggle shows the gain?
