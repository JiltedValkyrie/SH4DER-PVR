# SH4DER-PVR

Graphics effects for the Sega Dreamcast: screen post-processing and per-vertex light, on a machine with no shaders.

The SH-4 does the thinking a shader would do: it reads the finished frame back on a small grid, measures it, builds small maps and lights vertices. The PVR does the drawing it is good at: blended full-screen layers, small filtered textures, the fog table. Nothing here is brute force.

Two kinds of effect, each a module of its own (take one header or all of them):

- **Screen effects** (look, frame, glow, torch, warp, trail, taa, lens, focus, visor) are drawn over the finished frame as a few layers. Some use a little video memory: the bloom's two maps are 17 KB together. An effect that uses the whole frame as a texture (warp) needs a frame texture: about 640 KB at 640 x 480, about 160 KB at 320 x 240.
- **Per-vertex effects** (blade, holo, bloom, ao) change the vertex colours you were already going to write: no extra polygons, no video memory.

It needs [KallistiOS](https://github.com/KallistiOS/KallistiOS) for the drawing and [SH4ZAM](https://github.com/gyrovorbis/sh4zam) for the maths.

Repository: [github.com/JiltedValkyrie/SH4DER-PVR](https://github.com/JiltedValkyrie/SH4DER-PVR). In code the names keep the library's first name: the `sh4der_` prefix, `sh4der_core.h`, `SH4DER_CORE_NO_SH4ZAM`, `libsh4der_core.a`.

**[SH4DOW-DC](sh4dow-dc) is included**, in the `sh4dow-dc` folder: the soft self-shadow library, whole, with its own README, tests and license. Use its shadows next to these effects; one download holds both. The soft edge of every effect here is SH4DOW-DC's own column feather (`sh4dow_feather_in_`), called from `sh4dow_dc.h`, not copied. Put `sh4dow-dc/include` on the include path next to `include`. `SH4DER_CORE_NO_SH4ZAM` also sets `SH4DOW_DC_NO_SH4ZAM`, so both take the same path.

## Effects

| Effect | Header | What it does | Drawn | Shipped on |
| --- | --- | --- | --- | --- |
| blade | `sh4der_blade.h` | Light tubes orbit an axis, a blocker runs through them, a pulse breathes over both | untextured, additive | Halo: Dreamcast Evolved's energy sword |
| holo | `sh4der_holo.h` | A light inside a translucent body follows the body's own breathing | alpha blended, texture × colour | a hologram's chest |
| bloom | `sh4der_bloom.h` | A soft glow round a blade: a feathered ribbon facing the camera and a feathered disc at the tip, 24 triangles | untextured, additive | the energy sword's halo |
| look | `sh4der_look.h` | An eye that adapts to the frame both ways, a grade (a straight line per channel), glare in the fog | full-screen untextured layers (multiply, add, invert); the fog table | research: a test image's look rows |
| frame | `sh4der_frame.h` | The frame on screen read back on a grid, a stamp on every frame so a read knows which frame it saw, the gain and glow taken off a read | the scan-out; a 4 x 4 pixel block | research: the demo disc |
| taa | `sh4der_taa.h` | Temporal AA maths: jitter, history weight, pan shift | the frame before as layers | research: none yet |
| glow | `sh4der_glow.h` | Screen bloom from a small bright map of the frame (darks die, brights stay), a wide halo, a darken layer balanced so a neutral scene keeps its mean, god rays as scaled copies about the sun | a 128 x 64 RGB565 map, additive quads | research: none yet |
| warp | `sh4der_warp.h` | The frame as a texture through a mesh: shockwave ring, heat haze column, underwater wobble; radial speed blur as scaled copies; pixelate and ordered dither | a screen grid mesh over the frame texture | research: none yet |
| trail | `sh4der_trail.h` | The frame before over this one through the front-buffer copy (a column mask in destination alpha, two passes a copy): motion blur by speed, and the temporal AA blend | full-screen layers, a 128 B mask | research: none yet |
| lens | `sh4der_lens.h` | Overlays with no frame read: a vignette mesh, mean-neutral film grain, hit direction wedges and a full-screen flash | vertex-colour meshes, a 64 x 64 grain tile | research: none yet |
| focus | `sh4der_focus.h` | Fake depth of field: a small blurred map of the frame drawn through a vertex-alpha mask that keeps a disc, band or rect sharp | a 128 x 64 map, one alpha mesh | research: none yet |
| visor | `sh4der_visor.h` | Visor looks as number tables (night, amber, thermal): a tint through the look grade, grain, scanlines, a luma ramp for thermal, a fade in and out | the look effect's layers, small tiles | research: none yet |
| torch | `sh4der_torch.h` | A flashlight hotspot carved on screen: a radial mesh darkens outside the circle and boosts a hot core, the fog table bends to black for reach, gated by the level's darkness with hysteresis | two radial vertex-colour meshes; the fog table | research: none yet |
| ao | `sh4der_ao.h` | Small-scale occlusion: crease darkening from the angle between faces for a bake, and a screen cavity cue from a small map | vertex colour at bake; a small multiply map | research: none yet |

All of them include `sh4der_core.h`, the shared parts:
- **The feather.** SH4DOW-DC's column feather, with the core answered before any square root.
- **The shapes.** A tube (a soft column of light orbiting the axis) and a blocker (a soft ball on a helix along it).
- **The breath tracker.** A pose's own breathing turned into 0 (out) .. 1 (in).
- **The glint.** A soft quad's brightness by camera depth.
- **The colour ramp.** EDGE and CORE from a texture's average colour; a vertex of level k takes mix(EDGE, CORE, k²) × k, so dim parts read saturated and peaks white-hot.
- **The SH4ZAM steps.** Distances, clamps, sines, cosines and roots, with a plain C mirror for host tools.

## The blade effect

It was made for the energy sword: the two blade parts (two additive material groups of one model) get the effect, coloured with their texture's average colour. The sword has no animated bone of its own, so the effect runs in the blade node's space: x along the blade.

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
  - fade is your 0..1, for example 1 − active camouflage.
  - Ripples along the axis (off by default): with `ripple_depth` > 0 the intensity is multiplied by 1 − `ripple_depth`·½(1 − cos(2π·`ripple_freq`·x − 2π·frac(`pulses`·u))): `ripple_freq` cycles a unit along the axis, travelling one ripple a pulse.
- **Point light.** `frame->light` = (1 − `light_swing`·(1 − pulse))·fade. Use it for a point light that breathes with the glow, or set `light_swing` to 0 for a constant light.

`turns`, `passes` and `pulses` are whole numbers, so the period closes.

### Quick start

```c
/* SH4DER-PVR: the blade effect

   example/blade.c
   Copyright (c) 2026 Jilted Valkyrie

   One sword blade, one frame: the shipped tuning at the top of a pulse,
   three vertices and their packed colours.
*/

#include <stdio.h>
#include "sh4der_blade.h"

int main(void) {
    /* blade vertices in the blade node's space: x along the blade */
    static const float blade[3][3] = {{0.20f, 0.0f, 0.06f}, {0.20f, 0.0f, -0.05f}, {0.33f, 0.0f, 0.01f}};
    static const float cyan[3] = {0.35f, 0.82f, 1.0f};   /* the blade texture's average, brightest channel 1 */
    sh4der_blade_params_t params;
    sh4der_blade_frame_t frame;
    uint32_t argb[3];
    unsigned i;

    sh4der_blade_params_default(&params);
    /* tick 48 + this sword's phase (0 here); fade = 1 - active camouflage */
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

1. Call `sh4der_blade_prepare` once per instance per frame, with the game tick plus the instance's phase. The game uses 57 × the object's index, as the original game offsets object functions, so a squad's swords do not pulse together. On bad params it returns 0 and leaves a dark frame: every intensity 0, light 0.
2. Then call `sh4der_blade_intensity` per vertex, or `sh4der_blade_shade` / `sh4der_blade_argb` over a run. Strides are byte steps.
3. Draw the group's triangles additively with that colour. The library does not touch your texture or your draw.

Which parts get the effect is your choice, made once at load: for example a mask of the model's shader indices whose shader names you listed.

### Blade tuning

The shipped sword values, from `sh4der_blade_params_default`. Lengths are in vertex units (the original game's world units on the sword's node). The period is in ticks (30 a second).

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_BLADE_PERIOD | 192 | Ticks a period: one turn of tube A, 6.4 s. The original glow widget turns at 1 radian a second, 188.5 ticks a turn |
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
| SH4DER_BLADE_K_X0 | -0.10 | Blocker pass start (behind the hilt) |
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

**Cost.** A frame is 144 bytes and can live on the stack; params are 136 bytes, one table per effect (both sizes on the SH-4). Per vertex: two 2D distance tests, one 3D distance test, and a feather only where a vertex is inside a tube or the blocker. The draw is untextured, which costs the PVR less than the textured draw it replaces.

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

**The glint.** The original game's lens flare on the same light: a soft additive quad at the chest marker, half size 0.10 x 0.15 (radius 0.025 x scale 4, 6), brightness 0.4 within 3 units of the camera, fading to nothing at 6.5 (`sh4der_glint(depth, fade)`). Draw it 0.0075 toward the viewer, depth tested, no depth write: the world in front of it hides it per pixel. The texture and tint are your data.

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

The shipped values, from `sh4der_holo_params_default`. Lengths are in vertex units (the bind model's world units).

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
  - 5 columns: edge, mid, centre, mid, edge, with the feather across (0, 0.62, 1, 0.62, 0 for the shipped width).
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
| SH4DER_BLOOM_TIP | 0.90 | Ribbon level at the blade tip: way more at the top |
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

**Cost.** 24 vertices and 24 triangles a sword (the tip disc has 8 rim vertices: it reads round). For each, one tube evaluation per ribbon vertex and one normalise for the disc. No texture, no video memory.

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

Draw the layers untextured over the whole frame, under the HUD, depth always, no depth write: MULTIPLY src ZERO and the source colour in the destination slot, BOOST src destination colour and dst ONE, ADD ONE / ONE, INVERT white with src inverse destination colour and dst ZERO.

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

## The temporal AA maths (research)

`sh4der_taa.h`: a sub-pixel jitter of the projection each frame (Halton 2, 3, `SH4DER_TAA_PHASES` phases, centred) and the frame before laid over this one at a weight. No motion vectors on the PVR: the weight falls with the camera's speed in pixels, a pan shifts the old frame whole, a turn past `SH4DER_TAA_CUT_RAD` in one frame is a cut (no history). Header only, research: no client draws it yet.

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

## The glow effect (research)

`sh4der_glow.h`, `src/sh4der_glow.c` (the KOS PVR drawing, Dreamcast only): screen bloom from a small bright map of the frame (darks die, brights stay), a wide halo, a darken layer balanced so a neutral scene keeps its mean, god rays as scaled copies about the sun. Not booted yet: every cost in its plan is an estimate.

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

## The warp effect (research)

`sh4der_warp.h`, `src/sh4der_warp.c` (the KOS PVR drawing, Dreamcast only): the frame as a texture through a mesh: shockwave ring, heat haze column, underwater wobble; radial speed blur as scaled copies; pixelate and ordered dither. Not booted yet: every cost in its plan is an estimate.

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

## The trail effect (research)

`sh4der_trail.h`, `src/sh4der_trail.c` (the KOS PVR drawing, Dreamcast only): the frame before over this one through the front-buffer copy (a column mask in destination alpha, two passes a copy): motion blur by speed, and the temporal AA blend. Not booted yet: every cost in its plan is an estimate.

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

## The lens effect (research)

`sh4der_lens.h`, `src/sh4der_lens.c` (the KOS PVR drawing, Dreamcast only): overlays with no frame read: a vignette mesh, mean-neutral film grain, hit direction wedges and a full-screen flash. Not booted yet: every cost in its plan is an estimate.

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
| SH4DER_LENS_SPOKES_MAX | 32 | Spokes the maths takes at most |
| SH4DER_LENS_RINGS_MAX | 16 | Rings the maths takes at most |

## The focus effect (research)

`sh4der_focus.h`, `src/sh4der_focus.c` (the KOS PVR drawing, Dreamcast only): fake depth of field: a small blurred map of the frame drawn through a vertex-alpha mask that keeps a disc, band or rect sharp. Not booted yet: every cost in its plan is an estimate.

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

## The visor effect (research)

`sh4der_visor.h`, `src/sh4der_visor.c` (the KOS PVR drawing, Dreamcast only): visor looks as number tables (night, amber, thermal): a tint through the look grade, grain, scanlines, a luma ramp for thermal, a fade in and out. Not booted yet: every cost in its plan is an estimate.

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

`sh4der_torch_near_at` takes its own strength and reach (`inv_r2` = 1 / reach^2); `sh4der_torch_near` is it with the shipped values. Both meshes write `SH4DER_TORCH_VERTS` vertices at most.

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

## The ao effect (research)

`sh4der_ao.h`: small-scale occlusion: crease darkening from the angle between faces for a bake, and a screen cavity cue from a small map. Not booted yet: every cost in its plan is an estimate.

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

## The frame read

`sh4der_frame.h`, `src/sh4der_frame.c`: the look and the glow work from the frame the CPU reads off the screen, and that frame is one or two frames old, which of the two changing from frame to frame. An effect that takes "last frame's" gain or glow off the read corrects the wrong frame and pulses. So every frame carries a stamp: `sh4der_frame_next` numbers it (0..7) and keeps the eye's gain and the glow's strength it is drawn with; `sh4der_frame_stamp_draw` lays a small block of that number's colour at the top left; `sh4der_frame_read` samples the screen on the grid and reads the stamp back, so the client takes off exactly what the read frame carried (`sh4der_frame_degain`, the glow's `sh4der_glow_unfeed`). The read gives two grids: one sample a cell for the eye, and the darker of two samples a cell for the glow, so one hot pixel alone in its cell (a texture's grain, a letter's stroke) lights nothing. `sh4der_frame_hot` is how hard a light is looked at: a client grows and whitens its sun by it. An emulator shows the CPU a black screen unless it copies the drawn frame into video memory (its framebuffer emulation).

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

## Shared tuning

| Name | Default | Meaning |
| --- | --- | --- |
| SH4DER_BREATH_WINDOW | 90 | Frames the breath tracker remembers (one 3.0 s idle loop); a tracker is 364 bytes |
| SH4DER_BREATH_SPAN | 0.0013 | Least chest travel that counts as a full breath |
| SH4DER_GLINT_RADIUS | 0.025 | Glint quad radius before the scale |
| SH4DER_GLINT_SCALE_X | 4.0 | Horizontal scale (half width 0.10) |
| SH4DER_GLINT_SCALE_Y | 6.0 | Vertical scale (half height 0.15) |
| SH4DER_GLINT_BRIGHTNESS | 0.4 | Glint brightness at full |
| SH4DER_GLINT_NEAR | 3.0 | Full glint within this camera depth |
| SH4DER_GLINT_FAR | 6.5 | No glint from this camera depth |
| SH4DER_GLINT_OFFSET | 0.0075 | Glint quad moved this far toward the viewer |
| SH4DER_RAMP_EDGE_SAT | 1.6 | EDGE saturation factor (the energy sword's blue texels: saturation 0.55) |
| SH4DER_RAMP_CORE_SAT | 0.25 | CORE saturation factor (its white-hot texels: saturation 0.09) |

```c
void sh4der_ramp_colours(const float avg[3], float white, float edge[3], float core[3]);
```

`sh4der_ramp_colours` turns a texture's average colour into EDGE and CORE at load; `white` (0..1) mixes both toward white (a white test mode). A black average gives cyan.

## Adding an effect

1. Write `include/sh4der_<name>.h` and `src/sh4der_<name>.c` (or, as the look effect, the header alone with inline calls). Include `sh4der_core.h`, prefix every name `sh4der_<name>_` / `SH4DER_<NAME>_`, and use the shared steps: the feather, the shapes, the SH4ZAM helpers. Do not copy SH4DOW-DC or SH4ZAM code in.
2. Write `test/<name>_test.c`: the shipped numbers, the bad params, both paths.
3. Add the source to `SRC` and `HDR` in the Makefile, and the test to `TESTS`.
4. Add a row to the effect table above and a tuning table: a test checks that every tuning macro of every header is in a table with the same value.

## Build and test

```sh
make test                        # host, plain C: test/core_test, test/blade_test, test/holo_test, test/bloom_test, test/look_test, test/taa_test, test/frame_test, test/glow_test, test/warp_test, test/trail_test, test/lens_test, test/focus_test, test/visor_test, test/torch_test, test/ao_test, example/blade
make test-shz SH4ZAM=/path/to/sh4zam   # host, the SH4ZAM path on SH4ZAM's software back-end
make dc SH4ZAM=/path/to/sh4zam   # Dreamcast: build/libsh4der_core.a (KallistiOS environment sourced)
make test-dc SH4ZAM=/path/to/sh4zam  # Dreamcast: build/*_test.elf
```

Every target takes `SH4DOW=`, SH4DOW-DC's folder (default: the included `sh4dow-dc`). The Dreamcast build wants SH4ZAM's include path and these flags: `-ffast-math -ffp-contract=fast -mfsrra -mfsca -fno-math-errno`, and defines neither `SH4DER_CORE_NO_SH4ZAM` nor `SH4DOW_DC_NO_SH4ZAM`. The library needs no KallistiOS header. Use SH4ZAM as a library, 0.10 or later.

- `test/core_test.c`: the feather equals SH4DOW-DC's over a grid, the core before the root (a vertex on a tube's line, a zero core), the SH4ZAM steps and their mirror, the breath tracker's and glint's refusals.
- `test/blade_test.c`: the shipped numbers at fixed ticks and points; every early-out against a plain formula over 200,000 random vertices, ticks and fades; the period closing with no seam; the fade; bad params; the shade and packed-colour loops.
- `test/bloom_test.c`: the shipped levels and positions for the game and MAX WHITE presets; the colour ramp; MAX WHITE keeping its structure; the counts (24 triangles, every vertex used); the ribbon and the disc facing three eyes; the feathered edges and more at the top; bad params.
- `test/glow_test.c`: the threshold curve and the RGB565 pack, the shrink, the halo split, the darken balance, the ray series, the sun fade.
- `test/warp_test.c`: the ring, column and wobble offsets, the edge clamp, the radial scales and weights, the pixel grid, the dither tile.
- `test/trail_test.c`: the weights by speed, the even pixel shifts, the copy passes, a cut, the RGB565 drift model.
- `test/lens_test.c`: the vignette's vertex colours and banding bound, the grain's mean, the wedge geometry, the flash envelope.
- `test/focus_test.c`: the box blur, the mask falloff for each shape, the mesh counts.
- `test/visor_test.c`: the presets through the grade, the thermal ramp, the affine fit error, the fade envelope.
- `test/torch_test.c`: the profile, the mesh colours, cone to radius, the fog bend, the darkness gate, the power ramp.
- `test/ao_test.c`: the crease factor by angle, the cavity cue on synthetic pictures.
- `test/frame_test.c`: the stamp there and back through RGB565, the history ring, the darker of two pixels, a gain taken off, the looked-at term, the glow taken off a read, the weight table from a threshold.
- `test/taa_test.c`: the Halton values, the jitter centred over a cycle, a known turn's shift in pixels, the weight falling with speed, a cut.
- `test/look_test.c`: the luma bins and the log average; the eye into light (over-bright, then settled by LIGHT_S) and into dark (darker, then slow); the log and exp series against the C library; every grade layer list against clamp(A x + B) over a ramp; the layer colour; the glare.
- `test/holo_test.c`: the shipped weights and table entries, the breath tracker over a breathing chest that then leans, a table that never gets darker with more weight or more breath, the fade, bad params, the glint by depth.

## What a caller must keep

- **Aligned colours.** The calls that write 32-bit colours or floats through a byte stride (`sh4der_bloom_argb`, the blade and holo colour writers, SH4DOW-DC's `sh4dow_shade`) need the stride to be a multiple of 4 and the array 4-byte aligned. The SH-4 stops on a misaligned 32-bit store.
- **Sized arrays.** A call that takes a count writes that many entries and no more; a count outside its documented range writes nothing and returns 0 where it returns a count.
- **Numbers.** Angles, gains and sizes that are not finite are treated as "no effect this frame" (a cut for the TAA and the trail), not carried into the next frame.

## Credits and license

SH4DER-PVR is by Jilted Valkyrie, 2026. GitHub: [JiltedValkyrie](https://github.com/JiltedValkyrie). The line the game shows: `SH4DER-PVR Copyright (c) 2026 Jilted Valkyrie`.

SH4DOW-DC, by the same author, is included in the `sh4dow-dc` folder under its own license ([sh4dow-dc/LICENSE](sh4dow-dc/LICENSE)).

Dreamcast math: [SH4ZAM](https://github.com/gyrovorbis/sh4zam), Copyright (c) 2025, 2026 Falco Girgis, MIT. Keep that notice with the library.

The license is in [LICENSE](LICENSE). It uses the same terms as SH4DOW-DC.
