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
make ocean SH4ZAM=../sh4zam MKDCDISC=<mkdcdisc> OCEAN_CDI=build/ocean/sh4der-ocean-b8.cdi
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
| Left/right D-pad | Select swell, ripple, transmission, reflection, SH4DOW, bloom |
| A | Toggle selected effect |
| X | Pause wave time |
| Stick | Look horizontally/vertically |
| Start | Reset view |

Readout: average/worst complete frame intervals, count over 33.334 ms, CPU work
excluding explicit PVR waits, actual free texture VRAM. Two-second windows;
boot excluded. Diagnostics themselves add work. Serial frame statistics off
by default to avoid serial stalls; define `OCEAN_SERIAL_STATS=1` for optional
two-second logging. Target <= 33.334 ms requires Dreamcast measurement.

## Cost and validation

Texture VRAM: environment 512 KB + HUD 64 KB + optional bloom 17 KB = 593 KB.
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
