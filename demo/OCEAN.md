# Sunset ocean

| ID | Name | One-line | Status | Date |
|---|---|---|---|---|
| WATER-1 | Sunset ocean | Sunset ocean, visible sun, dusk gradient/fog, ripples and SH4DOW visibility | Runs on a real Dreamcast; stable 33.3 ms not yet measured on hardware | 2026-11-18 |

Open ocean, low evening sun, eye 2.5 m above water. No disc assets.

## Method

- Four directional sine swells move geometry; three shorter waves perturb
  normals. Analytic derivatives avoid neighbor-height recomputation.
- Mesh rows distribute samples roughly uniformly in screen depth. Wavelengths
  below local mesh sampling footprint fade out to avoid unresolved ripples.
- Sky depth is 0.0002: in front of KOS background plane 0.0001 and behind
  farthest water. b3 incorrectly used 0.000001, so KOS background covered the
  whole sky and sun. Host preview now includes background-plane depth.
- Blue-violet zenith blends through twilight to muted mauve horizon. Solar
  disc uses chord distance, avoiding fragile near-one dot thresholds with
  approximate normalization. Load-time RGB565 quantization reuses SH4DER's
  ordered dither threshold to reduce gradient banding.
- Far water uses KOS `pvr_fog_table_color`, `pvr_fog_far_depth` and
  `pvr_fog_table_custom`: 55 to 520 m linear mist toward the exact sky-horizon
  colour. Table samples follow PVR reciprocal-depth indexing. Sky fog disabled;
  sky itself has a low-altitude haze band. Table loaded once before rendering.
- Reflected sky uses a 512-square upper-hemisphere paraboloid lookup texture.
  Normals bend reflection UVs; Fresnel controls reflection share. Reflected sun
  emerges from those UVs, rather than a painted vertical stripe.
- PVR texture modulation supplies reflection; offset RGB supplies water body
  and thin-crest transmission. Transmission is the generic `sh4der_sss.h`
  module (back-lit transmission) via `sh4der_water_crest_sss`: an artistic
  crest/path-length proxy plus absorption, not a multiple-scattering volume
  solver.
- SH4DOW's new generic CPU heightfield sweep finds sun-blocking crests in a
  67.2 by 86.4 m local field. Cells: 0.60 m. Micro-ripple occlusion below this
  cell size is unresolved; away-facing ripple slopes use normal lighting.
  Shadow fades at field boundaries; far occlusion omitted. Feather approximates
  shadow softness; it does not integrate the solar disc.
- Visibility gates direct illumination, transmission and reflected sun region;
  sky reflection remains in shadow. Broad sun masking is a vertex approximation.
- Optional existing SH4DER bloom: sparse scan-out read, small maps, 0.10 halo
  strength. Previous halo can enter next read; low gain limits feedback. Texture
  updates wait until PVR finishes using their prior contents.

## Build

Source KallistiOS environment; SH4ZAM and mkdcdisc must be available.

```sh
make test-water
make build/shz/water_test SH4ZAM=../sh4zam
build/shz/water_test
make ocean SH4ZAM=../sh4zam MKDCDISC=<mkdcdisc> OCEAN_CDI=build/ocean/sh4der-ocean-b10.cdi
make ocean-preview
build/ocean-preview build/ocean-preview.ppm 6
```

Choose unused build number after source changes; image rule refuses overwrite.
Build increment; no library version bump. Keep local tool paths in environment
or make arguments. Original effects gallery still builds with `make demo`.

## Maths (b6+)

SH4ZAM for all scalar / vector work (`sh4der_fma_` = `shz_fmaf`, `sh4der_dot3_`
= `shz_vec3_dot`, `shz_vec3_reflect`, `shz_invf_fsrra`, `shz_sincosf`,
`shz_expf`). Water pitch + projection: XMTRX `shz_xmtrx_init_rotation_x`, then
SH4ZAM-DCE `shz_dce_project_vertices` per row (FTRV + FSRRA + FMAC, 24 B in,
32 B records, 7.4 KB row buffers). Sky rays: `shz_xmtrx_init_rotation_y` +
`shz_xmtrx_rotate_x`, `shz_xmtrx_transform_vec3`. Normalize: SH4ZAM-DCE
`shz_dce_vec3_normalize_safe`. SH4ZAM-DCE is a header include
(included in `third_party/sh4zam-dce`). Plain host build keeps a marked mirror.
b7 adds SH4DOW sweep: shz_lerpf + hoisted shz_invf(feather). Host preview,
SH4ZAM path, b5 vs b7: max 1/255 per channel, mean 0.0002.

## Real-hardware startup check (b5+)

Each stage clears the displayed frame (KOS `vid_clear`, no PVR render) and
prints one serial line with ms since `main`; held 300 ms (`OCEAN_STAGE_HOLD_MS`).

| Colour | Reached |
|---|---|
| Red | `main` |
| Orange | `pvr_init` |
| Yellow | fog table |
| Green | sky texture built |
| Cyan | VRAM upload + headers |
| Blue | first wave/shadow update |
| Magenta | first frame submit; ocean replaces it once the PVR renders |
| White | 20 waits in a row timed out: PVR never finished a frame |

Serial (once each): stage lines, first update time, "frame 1 rendered",
TA vertex / OPB overflow use after frame 1 and frame 300, stuck line.
HUD third line shows TA use (KB of buffer).

## Controls

| Input | Action |
|---|---|
| Y | Show/hide readout (hidden at start) |
| Left/right D-pad | Select a row: PRESET, SWELL, RIPPLE, TRANSMISSION, REFLECTION, SH4DOW, BLOOM, WATER TRIANGLES, SUN AZIMUTH, SUN ELEVATION |
| A | Toggle the selected effect; on a stepped row (PRESET, WATER TRIANGLES, SUN AZIMUTH, SUN ELEVATION) step up, wrapping (WATER TRIANGLES: ten steps, 2,000 triangles) |
| Up/down D-pad | Toggle on / off; on a stepped row step up / down, stopping at the ends (WATER TRIANGLES: 200 triangles) |
| LT / RT | Lower / raise the camera, 0.8 to 8.0 m |
| X | Pause wave time |
| Stick | Look horizontally/vertically |
| Start | Reset view |

Readout (five lines): the picked row and its value; average/worst complete frame
intervals and the count over 33.334 ms; CPU work excluding explicit PVR waits,
RENDER (the PVR's own time for the frame, `pvr_get_stats` `rnd_last_time`) and
actual free texture VRAM; the water mesh (triangles, vertices, columns x rows) and TA use;
the sun (azimuth / elevation), the sky texture state and the eye height. Two-second windows,
restarted on every change that alters cost;
boot excluded. Diagnostics themselves add work. Serial frame statistics off
by default to avoid serial stalls; define `OCEAN_SERIAL_STATS=1` for optional
two-second logging. Target <= 33.334 ms requires Dreamcast measurement.

## Presets, water triangles, sun, camera height (b10)

Rows work live; every change prints one serial line, none per frame.

**PRESET.** QUALITY is the b8 look and the default: every row on, 23,040 water
triangles (11,737 vertices), original sun, eye 2.5 m. PERFORMANT: SH4DOW off
(`shadow` 0: `ocean_shadow_update` returns before its sweep, and the readout
row reads OFF) and the 2,000 triangle mesh (36 x 28 quads = 2,016 triangles,
1,073 vertices). The other rows stay on: measured on the host they cost almost
nothing (ripples 0 to 3 %, reflection 0 %; transmission 17 % at the original
mesh) and they carry the look. BLOOM stays on too; its cost is the sparse frame
read and the halo draw, only a console can measure it, so it is the first row
to switch off if PERFORMANT is still over 33.3 ms. A preset sets the six
toggles and the mesh; it leaves the sun and the eye height alone. Change any
row and PRESET reads CUSTOM. QUALITY selected gives the b8 output: host check
(plain C and the SH4ZAM software back-end) of the environment texture, the row
and column tables and 96 combinations of the vertex arrays (3 times x 32
toggle sets), b8 header against this one, equal bit for bit.

**WATER TRIANGLES.** 114 steps of 200 triangles: 400, 600 .. 22,800, then the
original 23,040 (2 x 120 x 96). A step is a grid of columns x rows (triangles
2 x c x r, vertices (c + 1) x (r + 1)), the one nearest the step at an aspect
near the original's 1.25 (limits 1.10 to 1.42; a 1 % miss of the step costs as
much as 0.10 of aspect). Every step lands within 3 % of its target (most within
1 %); 400 is 390 triangles (15 x 13). D-pad up / down: 200 triangles; A: ten
steps (2,000 triangles) up, wrapping to the bottom. The readout shows both
counts. Rebuilding on a change is the row and column tables (rows follow the
original 96-row law at the fractional row y x 96 / rows, columns the original
120-column law); every vertex is recomputed each frame as before, so a step
costs no hitch. Memory: the water array stays sized for the original mesh
(375 KB main RAM), nothing new in VRAM.

Grid for every step (target triangles: columns x rows):

```
400: 15x13  600: 20x15  800: 22x18  1000: 25x20  1200: 27x22  1400: 29x24
1600: 32x25  1800: 33x27  2000: 36x28  2200: 38x29  2400: 39x31  2600: 42x31
2800: 41x34  3000: 43x35  3200: 46x35  3400: 46x37  3600: 47x38  3800: 49x39
4000: 50x40  4200: 51x41  4400: 52x42  4600: 55x42  4800: 56x43  5000: 57x44
5200: 58x45  5400: 60x45  5600: 61x46  5800: 60x48  6000: 61x49  6200: 62x50
6400: 64x50  6600: 65x51  6800: 65x52  7000: 66x53  7200: 68x53  7400: 68x54
7600: 69x55  7800: 70x56  8000: 70x57  8200: 72x57  8400: 72x58  8600: 73x59
8800: 75x59  9000: 75x60  9200: 77x60  9400: 77x61  9600: 77x62  9800: 79x62
10000: 79x63  10200: 81x63  10400: 80x65  10600: 83x64  10800: 83x65  11000: 83x66
11200: 85x66  11400: 85x67  11600: 85x68  11800: 87x68  12000: 87x69  12200: 87x70
12400: 88x70  12600: 90x70  12800: 90x71  13000: 90x72  13200: 93x71  13400: 92x73
13600: 92x74  13800: 93x74  14000: 93x75  14200: 96x74  14400: 95x76  14600: 96x76
14800: 96x77  15000: 96x78  15200: 97x78  15400: 99x78  15600: 99x79  15800: 100x79
16000: 100x80  16200: 100x81  16400: 101x81  16600: 101x82  16800: 102x82  17000: 104x82
17200: 104x83  17400: 105x83  17600: 106x83  17800: 106x84  18000: 106x85  18200: 107x85
18400: 107x86  18600: 108x86  18800: 108x87  19000: 109x87  19200: 109x88  19400: 110x88
19600: 110x89  19800: 110x90  20000: 111x90  20200: 112x90  20400: 112x91  20600: 113x91
20800: 114x91  21000: 114x92  21200: 115x92  21400: 115x93  21600: 116x93  21800: 117x93
22000: 117x94  22200: 118x94  22400: 118x95  22600: 119x95  22800: 120x95  23040: 120x96
```

Look at fewer triangles (host renders): near water stays close to the original;
the swell lines of the mid distance flatten and the sun's glitter path breaks
into separate glints (the unresolved-wave filter fades the waves the mesh cannot
sample, and the row spacing grows as the rows shrink). At 2,016 triangles the
water beyond the first bands is a smooth gradient with a few glints.

| Step | Grid | Triangles | Vertices | TA strip data (ESTIMATE) | Scene update CPU, host ratio, SH4DOW on | SH4DOW off |
|---|---|---|---|---|---|---|
| 400 | 15 x 13 | 390 | 224 | 15 KB + sky 77 KB | 53 % | 0.6 % |
| 2,000 (PERFORMANT) | 36 x 28 | 2,016 | 1,073 | 77 KB + 77 KB | 56 % | 3.4 % |
| 3,000 | 43 x 35 | 3,010 | 1,584 | 116 KB + 77 KB | 58 % | 5.4 % |
| 6,200 | 62 x 50 | 6,200 | 3,213 | 238 KB + 77 KB | 65 % | 11.8 % |
| 12,200 | 87 x 70 | 12,180 | 6,248 | 468 KB + 77 KB | 77 % | 23.2 % |
| 18,400 | 107 x 86 | 18,404 | 9,396 | 707 KB + 77 KB | 90 % | 35.8 % |
| 23,040 (original) | 120 x 96 | 23,040 | 11,737 | 885 KB + 77 KB | 100 % | 46.4 % |

TA strip data is blocks of six triangles x 236 B (water) and 384 sky blocks x
204 B; the readout's TA figure is the measured peak (registers) and restarts at
each step. CPU ratios: host, plain C path, scene update only, 100 % = original
mesh with SH4DOW on, best of five runs; the host is not a Dreamcast, only the
ratios carry over. The TA buffer is sized once at `pvr_init` (1,536 KB x2), so
no step frees VRAM live. A build that only ever draws 2,016 triangles needs
about 154 KB a frame (water 77 + sky 77): `OCEAN_VERTEX_BUF_KB=512` would hold
it with a wide margin and return 2,048 KB of texture VRAM (not set here; check
the readout's TA figure on the console first, and remember the TA stops the
list on overflow).

**CAMERA HEIGHT (LT, RT).** LT lowers the eye, RT raises it, up to 2 m/s at a
full trigger (`OCEAN_EYE_RATE`); the readout's fifth line ends with `EYE n.n m`.
Range 0.8 to 8.0 m, start 2.5 m (the original). The swell peaks at about
0.28 m (four swells 0.263 m, three ripples 0.0125 m), so 0.8 m keeps the eye
0.5 m over the highest crest. At 8 m the far edge of the water (2,796 m) is
1.4 px under the horizon, inside the fog that matches the sky; above that the
gap would show. The camera pitch is unchanged, and so are the horizon, the sky
and the sun: the horizon sits at the pitch angle for any height and the sky
rays come from yaw and pitch alone (host check: the sky mesh is bit for bit the
same at 0.8, 1.5, 2.5, 5 and 8 m; renders show the horizon and the sun in the
same place). One thing does change below 2.5 m: the water's near edge starts
3.2 m ahead, which at a lower eye would rise into the bottom of the picture, so
the rows shrink with the eye (x eye / 2.5; the waves are sampled at the shrunk
positions, so the swell looks larger and nearer). At and above 2.5 m the rows
are untouched. The eye does not change the preset.

**SUN AZIMUTH / SUN ELEVATION.** Azimuth -40 to +40 degrees (right of the view
axis positive; original 9.8), elevation 1.5, 2.5, 3.4 (original), 5, 8, 12, 18,
25, 35, 45. One sun vector, (cos el sin az, sin el, cos el cos az), normalized
(SH4ZAM-DCE `shz_dce_vec3_normalize_safe`, FSCA for the angles), drives the
water body light, the crest transmission, SH4DOW's sweep, the reflected sun
mask and the sky texture's disc and halo. The original pair is the original
vector exactly. A new sun repaints the 512 x 512 environment texture, 8 rows a
frame (`OCEAN_SKY_ROWS`; host ratio: 3 % of a quality scene update a frame, about
64 frames in all) in main RAM; the water keeps the old sun until the texture is
whole, then the vertices and the texture change in the same frame, the texture
reaching VRAM (512 KB `pvr_txr_load`, after the PVR wait) that frame. The readout
shows SKY REBUILD n % meanwhile; a second change restarts it.

How the sunset degrades as the sun climbs (host renders of this scene): the
palette is a fixed dusk. The horizon band, the fog colour and the sky gradient
do not follow the sun, only the disc and its halo do. At 5 to 8 degrees the
disc clears the haze and the glitter path thins to a few streaks; at 12 to 18
it is a white disc in a violet sky over a still-orange horizon and the
reflection is one or two glints; at 25 the disc is at the top edge of the frame
(vertical half-field 27 degrees, pitch up to 4.6) and at 35 and 45 it is out of
view, the halo and the glitter are gone and the water is lit by sky reflection
and crest transmission only, so the picture goes dark and flat. The nearest
water row is 3.2 m ahead at an eye height of 2.5 m, so the glitter leaves the
mesh above about 38 degrees whatever the camera does. SH4DOW (measured, host,
swell at t = 6 s, the 112 x 144 field): cells in shadow 37.9 % at 1.5 degrees,
21.7 % at 2.5, 10.6 % at 3.4 (original), 1.6 % at 5, and none from 8 degrees up:
the swell is too gentle (steepest slopes 2 to 3 degrees each, 11 degrees if
all four align) to hide the sun above that. Above about 8 degrees the sweep, half the
CPU work of a frame, draws nothing: switch SH4DOW off (PERFORMANT does); making
it skip itself there would be exact and is not done here. Transmission is
back-lit: it needs the sun behind the crest from the eye, so it fades as the
sun rises and as the azimuth leaves the view. At 1.5 degrees the sun sits on
the horizon: disc half in the haze, glitter path longest. A usable sunrise or
midday look would need a sun-driven palette (zenith, horizon, fog and the fog
table); this demo does not have one.

## Cost and validation

Texture VRAM: environment 512 KB + HUD 128 KB (five lines; was 64 KB for three) + optional bloom 17 KB = 657 KB.
b9 and b10 against b8: VRAM +64 KB (the HUD; free texture VRAM about 2,787 -> 2,723 KB, the boot line prints
the actual), main RAM +64 KB (`sh-elf-size` bss 1,189,336 -> 1,254,936 B, text +5 KB), water mesh and sun
rows 0 KB (arrays unchanged; the environment texture is repainted in place).
TA vertex buffer 1536 KB x2, OPB overflow 6 (b4: 1024 KB, 3). Estimate per
frame: water 3,840 six-triangle strip blocks x 236 B + sky 384 x 204 B = about
962 KB, so b4 had about 6 % margin. Real hardware stops the list on overflow:
no render, black screen; an emulator renders anyway. b5 cost: +1,254 KB VRAM
(texture VRAM free about 4,036 -> 2,787 KB; actual printed at boot).
CPU shadow heights + visibility + scratch: 127 KB. AICA effect cost: 0 KB.
ELF section sizes come from `sh-elf-size`; no audio or external scene assets.

Host checks: finite-difference derivatives, unresolved-wave filter, ripple-only
normal displacement, flat reflection, Fresnel endpoints/bounds, absorption
ordering, flat/ridge/reversed/transposed/vertical shadows. Plain C and SH4ZAM
software backends pass. Offline rasterizer uses same mesh, UVs and vertex
colors without PVR bloom. Host timing/preview is not hardware evidence.
PVR output and stable 30 fps need boot.

b4 regression checks also cover sky/background ordering, far-water ordering,
visible sun with small direction-length error, dusk colour separation,
matching sky/fog horizon, fog table bounds and monotonicity. Both math backends
pass. Fog table adds 0.504 KB transient startup stack; texture VRAM/AICA delta
0 KB. KOS path: hardware table fog, direct PVR submission. SH4ZAM math retained.

Depth evidence: installed KallistiOS `kernel/arch/dreamcast/hardware/pvr/`
`pvr_buffers.c` initializes `zclip=0.0001f`; `pvr_misc.c` writes that depth to
`PVR_BGPLANE_Z`. Reproduce with the configured `KOS_BASE`.

References: [wave synthesis](https://developer.nvidia.com/gpugems/gpugems/part-i-natural-effects/chapter-1-effective-water-simulation-physical-models),
[scattering approximations](https://developer.nvidia.com/gpugems/gpugems/part-iii-materials/chapter-16-real-time-approximations-subsurface-scattering).
Methods adapted to fixed-function PVR; no NVIDIA source copied.
