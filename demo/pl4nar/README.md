# The street driving demo (pl4nar)

A street, a coupe, a chase camera, a 45 second day. It shows three pieces of the library working together on a triangle budget a late 1990s console street could carry (the car is 280 triangles, a frame about 1,400 plus about 800 in the mirror pass):

- **`sh4der_pl4nar`**: the street mirrored about the road into a 256 x 128 render target; the car's paint, glass and chrome look it up per vertex and blend it in by Fresnel. It is always on live; the autorun has the stages that turn it off, so the flat look and the reflective one can be compared.
- **`sh4der_pool`**: at night each street lamp's pool of light on the road and the headlight beams, as small fans of triangles (the blob shadow under each car only with `-DPL4NAR_BLOBS`).
- **`sh4der_spark_quad`**: the lamps' halos, and the same halos drawn into the mirror pass, so the lamps pass over the glass and the paint as the car drives under them.

The hour costs no layer: its tint goes into the vertex colours the frame computes anyway.

Two more pieces are in some builds: the heat haze over the road's far end by day (street, map, hills; **X**), and the wet road (the street built with `PL4NAR_WET`, night in rain). Both are below.

The world's textures come in two sets, switched live with **A**: the low look (the default: the road and four facades at 64 x 64, 16 colours each in a PVR palette bank, point sampled, no mipmaps, 2,048 B a texture) and the full one (128 x 128 and 256 x 256, filtered, with mipmaps).

Status, plainly. **Proven:** it compiles for the host and for the Dreamcast on this library's headers with no warning, and its host preview (a software rasteriser over the same code) draws the same picture as the version that ran on an emulator before it moved into this folder: every mirror target identical, the screens apart only by two renamed labels. **Not proven:** `build.py` end to end (the map build was run from the credits check to the compile on one machine on 2026-11-25, with the image step skipped on purpose; no image from it has booted; the street build's steps were run by hand); the fetch table's links and hashes (never run against the network, nothing pinned); the build without `--rtt` (compiled as a library file, never run); a boot of an image of its own from `build.py`; the order of the two texels in a byte of the 16-colour textures (the host preview assumes the same order as the bake, so only a boot tells). **Ran on a console:** an ELF built in this folder (pl4m c5, `build/pl4m_pl4m-c5/`) ran inside suite disc s7 on 2026-11-25, one console, one run: frame and SH-4 times, the targets' sizes with no NOVRAM, and the split at the middle are MEASURED; its PVR and TA columns are ESTIMATE, from a bug in our sampling (the wiki ledger, PVR-160 to PVR-162). No time in the table below is measured on a console.

## Build

```sh
python demo/pl4nar/build.py --sh4zam /path/to/sh4zam --rtt              # the map (the default)
python demo/pl4nar/build.py --sh4zam /path/to/sh4zam --rtt --street     # the first street layout
python demo/pl4nar/build.py --sh4zam /path/to/sh4zam --rtt --tech       # TECH DEMO, the measuring autorun
python demo/pl4nar/build.py --sh4zam /path/to/sh4zam --rtt --hills      # DRIVE on the map with the hills
```

Run it in a shell with the KallistiOS environment sourced. One command goes from a clean checkout to a disc image. It:

- checks the credits table, and the Controls table below against `src/controls.h`;
- fetches the CC0 art (`fetch_table.json`; nothing third-party is in this repository);
- builds the coupe, the street furniture and the overhead structures from numbers;
- cuts the stock models to the budget in Blender (headless);
- bakes the models and textures;
- lays out the loop (`tools/map.py`) and bakes its ambient occlusion (`tools/map_ao.c`);
- compiles, and writes the image.

Two layouts (the map, `--street`) and two programs, from one source, never share a folder:

- **DRIVE** (the default): free driving. The HUD shows the time, the lap with the last and best lap time (the map), the speed, and one line of keys; START opens the tech overlay, and with it the tech keys (the Controls table's When column).
- **TECH DEMO** (`--tech`): the measuring autorun. It runs its stages and then shows its result pages.
- `--hills` (the map only) adds the hills.

| | generated | objects and ELF | image |
|---|---|---|---|
| map, DRIVE (default) | `build/gen_map` | `build/map_drive` | `out/pl4nar-drive-map-b<N>.cdi` |
| map, `--tech` | `build/gen_map` | `build/map_tech` | `out/pl4nar-tech-map-b<N>.cdi` |
| map, `--hills` (and `--tech`) | `build/gen_map` | `build/map_hills_drive` (`_tech`) | `out/pl4nar-drive-map-hills-b<N>.cdi` (`pl4nar-tech-`) |
| `--street` (and `--tech`) | `build/gen` | `build/street_drive` (`_tech`) | `out/pl4nar-drive-street-b<N>.cdi` (`pl4nar-tech-`) |

An image is never overwritten. A generated `.obj` that is already there is kept. Leave `--rtt` out on a KallistiOS without `pvr_scene_begin_rtt`: the target is then 1024 pixels wide (that path is compiled, not yet run). From the library's root, `make pl4nar-demo SH4ZAM=... PL4NAR_ARGS="--rtt"` runs the same script (add `--street`, `--tech` or `--hills` there).

Needs:

- KallistiOS and its toolchain, SH4ZAM, Blender (only while a model still has to be cut) and mkdcdisc;
- **a Python 3 with Pillow**: the bake reads the textures with it. Run `build.py` with that Python; on Windows the msys Python usually has no Pillow;
- **for the map, a host C compiler**: `HOST_CC`, else `gcc`, `cc` or `clang` on the PATH. `tools/map_ao.c` runs on your machine at build time, so `kos-cc` will not do. Without one, the map build refuses before it does anything; `--street` does not need it;
- the map's occlusion step also builds SH4ZAM's software matrix code (`source/sw/shz_xmtrx_sw.c` in the `--sh4zam` checkout) for the host.

Before the first real run, `python demo/pl4nar/build.py --dry-run [--street] [--rtt]` prints the steps (eleven for the map, eight for `--street`), which tools it can and cannot find (the host compiler included), the credits check and the state of the fetch table. It fetches, writes and builds nothing (`tools/test_build_dry.py` checks this for both builds).

The fetch table is not pinned yet: the first run needs `--pin`, which prints each pack's sha256 to put in `fetch_table.json`. The two Kenney packs have no stable direct link: the script says where to download them by hand.

To put these effects into another engine, follow `AGENT_WORKFLOW.md`. See `CREDITS.md` for every licence.

## Controls

The table is `src/controls.h`, the table the pad is read through and the HUD's key lines come from; `python tools/check_controls.py` fails when this one differs from it. Builds: street (`--street`), map (the default), hills (the map built with `PL4NAR_HILLS`), wet (the street built with `PL4NAR_WET`: the wet road one-off, night only). When: DRIVE (the game program, `build.py` without `--tech`) has two key sets, the drive keys while the overlay is closed and the tech keys while START has it open; "always" is both. Every other program (the live view, the autorun) has the tech keys all the time. On the suite disc (`../psm/psm_suite.h`), A + B + X + Y held together goes back to the menu: in the live programs at any time, in the autorun on its result pages; a "run all" passes the live programs over (they have no pages). On a disc of their own the four do nothing more than their own keys.

| Keys | Action | Builds | When |
|---|---|---|---|
| **stick or D-pad left / right** | steer | all | always |
| **RT** | engine | all | always |
| **LT** | brake, then reverse | all | always |
| **X** | heat haze over the road's far end: OFF (the start), LIGHT, STRONG; by day and dry only | street, map, hills | always |
| **Y** | time of day: AUTO (a 45 second day), DAY, NIGHT, PAUSE | street, map, hills | tech |
| **Y** | the rain: DRY, DAMP, WET in turn (night always) | wet | tech |
| **A** | textures: low (64 x 64, 16 colours, the start) or full | all | tech |
| **A** | restart: the car at the start, stopped; the clock and the laps from 0 | all | drive |
| **D-pad up** | F0 (the reflection's strength) up | all | tech |
| **D-pad down** | F0 down | all | tech |
| **START** | the overlay (times, triangles, memory, the target as a thumbnail) on / off; off at the start on the hills and in DRIVE | all | always |
| **B + stick** | the camera round the car | all | always |
| **B + X** | the fly-by: a fixed camera low at the kerb ahead, the car sweeps past through the heat (on in the shot), then the follow camera again | street, map, hills | always |
| **B + Y** | the whole HUD hidden (the picture alone) / shown; START also shows it again | all | always |
| **B + A** | the target's size: 128 x 64 (the start), 64 x 32, 256 x 128 in turn | wet | tech |
| **B + A** | the target's size: 256 x 128, 128 x 64, 512 x 256 in turn (the start: 256 x 128; the hills: 128 x 64) | street, map, hills | tech |
| **B + D-pad up** | the reflection's night gain up | all | tech |
| **B + D-pad down** | the night gain down | all | tech |
| **B + D-pad right** | the sun: FIXED, SWEEP (to DAY unless NIGHT), HOLD | all | tech |
| **B + D-pad left** | baked occlusion on / off | map, hills | tech |
| **B + START** | the effect labels shown / hidden | map, hills | tech |

No key turns the reflection or the car's shadow off, and there is no split key: in DRIVE and the live view both are always on (the projected shadow; on the hills, laid on the ground mesh). The autorun has no reflection-off, blob or flat-quad stage either (the owner's word): every stage is the reflection on with the mesh shadow, and the car drives on from stage to stage with no jump back to its start (`host/snap_test.c`). Night, the street and the wet road have no car shadow: the mesh shadow is by day on the map, and the blob discs only come back with `-DPL4NAR_BLOBS`. The heat starts OFF in every build until the owner has seen its rework.

In the autorun (the measuring copy) the driver holds the steering, the engine and the brake; every other key acts as it does live and marks the stage TOUCHED on the result page (B + stick changes no work and marks nothing). No key ends the autorun: it runs every stage, then shows its result pages, and a key pressed during a stage only marks it. The tile cut and the small tile set have no key: they are autorun stages only, and the cut is off by default. Its cost: the PVR side of a map pass fell 5.1 -> 1.4 ms but the SH-4 side rose 3.9 ms, measured in another program (d1-c, PVR-192, one console), not yet measured here; nothing more is built on it until the cause is found.

## The wet road (`PL4NAR_WET`)

A one-off of the street at night in rain. **Y** steps the rain: DRY, DAMP, WET. The whole road darkens with it (the road's tone x 1.0, 0.75, 0.5).

- **The reflection** is one added layer over the road (`GFX_MAT_REFLECT_ADD`), each vertex looked up in the mirror pass's target with `sh4der_pl4nar_flat`, its alpha the Fresnel x the rain's gain (0 dry: no grid and no lookup, 1 damp, 2.2 wet). Rain on the film: a jitter of half a texel a vertex, new 15 times a second, and in damp and wet the picture pulled toward the horizon's row so a lamp's image runs toward the viewer.
- **A soft pool round the car.** The grid is centred on the car, not the camera. It is full within 15 m along the road (`WET_FULL`) and across to the kerbs (`WET_KERB`, 8 m from the road's middle), then falls by a smoothstep to 0 at 70 m ahead (`WET_AHEAD`), 40 m behind (`WET_BEHIND`) and the building lines (12 m). The rows are even (14 behind, 2.9 m apart; 20 ahead, 3.5 m apart) and the columns 2 m apart, so the fade spans 9 to 16 rows and the PVR's colour interpolation shows no steps: there is no water line, and the orbit camera (B + stick) looking back sees the pool behind the car too. Nothing is reflected beyond 70 m.
- **The road layer over it** (`GFX_MAT_GRIT`, lifted 1.5 cm over the reflection's 1 cm): one road surface, `tex_road`'s colour (asphalt, paint, kerbs) lightened and darkened by the grey of `tools/grit_gen.py`, with our cracks and patches of newer asphalt. 128 x 128 ARGB4444 with mipmaps, the same placement as `tex_road` (24 m across, 12 m along). Its alpha is how much of the road shows over the reflection: 4-bit levels 7 to 11 by grit_gen's field (mean 0.56), 2.7 % of the texels at 0, the paint, kerbs, cracks and seams at 15 so they stay readable (variant A in `tools/bake.py`, `GRIT_VARIANT`; B, holes and the rest near opaque, was the alternative not chosen). The rain lowers the whole layer's vertex alpha: 1.00 dry, 0.80 damp, 0.55 wet (`GRIT_DRY`, `GRIT_DAMP`, `GRIT_WET`), so the reflection comes through it everywhere a little, more as it gets wetter. The layer uses the pool's fade, so it has no edge either.

Tuned by eye on the host preview. Nothing of the pool or the road layer has run on a console yet.

Its macros, in `src/demo.c`:

| Macro | Value | What it is |
|---|---|---|
| `WET_FULL` | 15 m | each way along the road from the car at full strength |
| `WET_AHEAD` | 70 m | ahead of the car, where the pool reaches 0 |
| `WET_BEHIND` | 40 m | behind the car, where the pool reaches 0 |
| `WET_KERB` | 8 m | full across to the kerbs; 0 at the building lines (12 m) |
| `WET_COLS` | 13 | grid columns, 2 m apart |
| `WET_BACK`, `WET_FWD` | 14, 20 | grid rows behind and ahead of the car (35 rows with the car's: 455 vertices) |
| `WET_LIFT` | 0.01 m | the grid over the road, against depth fighting |
| `GRIT_LIFT` | 0.015 m | the road layer over the reflection |
| `GRIT_DRY` | 1.00 | the road layer's share (the texture's alpha x this): dry |
| `GRIT_DAMP` | 0.80 | damp |
| `GRIT_WET` | 0.55 | wet (the owner's figures, by eye on the host) |

`python tools/grit_gen.py --self-test` checks the generator of the grey and the hole field (both tile; fixed checksums).

## The heat haze

By day on a dry road (street, map, hills), the air over the road's far end shimmers. A window of the frame on screen (256 x 32 pixels round a point of the road 70 m ahead) is copied pixel for pixel into a 256 x 32 texture and drawn back over the same place through a 32 x 4 cell mesh. The samples drift by 0.7 of a pixel at most, in two slow waves that neighbours share, and the layer is at most half of the picture there. **X** steps it: OFF, LIGHT, STRONG; the autoruns run it OFF, so no stage measures its cost yet. It is gone by night (from night level 0.25) and in rain. The overlay's label shows the last copy's time. The code is `src/heat.h`, host tested by `host/heat_test.c`.

This is version 2. Version 1 looked like swimming texels (seen as host pictures; no console run of the heat is in the ledger): its copy was 3.5 pixels to a texel, each mesh corner wobbled on its own by 2 pixels, and it sat 30 m ahead. What version 2 keeps to:

- **Only the far end.** The layer has the depth of the road 70 m ahead. The car, the poles and the near buildings cover it and are not redrawn.
- **It follows a turn.** The copy is one or two frames old. The layer samples it where the far picture was in that frame, worked out from the camera's two directions (`heat_slip`), and fades out while the far picture moves fast.
- **No edge.** The sides fade over four cells, the top and bottom to nothing.

**The hook, three calls** (in `demo.c`; the order matters):

| Where | Call | What it is given |
|---|---|---|
| before any scene is open | `heat_copy(gate)` | the strength, 0 .. 1 (`heat_gate(night, rain)` times the level); it reads the screen and loads the texture |
| in the screen's translucent list, after the world | `heat_view(eye3, at3)` | the camera of this frame: the same `float[3]` eye and target `heat_anchor` gets |
| straight after it | `heat_draw(ax, ay, z, gate, seconds)` | the anchor on screen (the point `heat_anchor` gives, through the screen's matrix) and its 1 / w |

Without `heat_view` nothing is drawn: the layer has no direction to follow. After every `pvr_init`, `heat_reset()`.

**Where to use it, and where not.** The values in the table below are the ones this demo uses. They were judged on host pictures, not on a console: used in the demo, not proven.

- **Use it for** a hot dry day: the far end of a long straight road, tarmac or a runway, a desert horizon, the air over an exhaust or a fire seen from far off (move the anchor there).
- **Do not use it** at night or in rain (the gate gives 0 there, and then it costs nothing), on or in front of near things, or in a window smaller than 256 x 32 pixels.
- **STRONG and LIGHT** are the same layer at a share of 0.5 and 0.25. The read of the screen costs the same in both.
- **Watch for** three things on a console:
  - *Turns.* If the layer trails the picture in a turn, `HEAT_LAG` is wrong: try 1.
  - *Copy lag.* The picture in the layer is one or two frames old, so a fast thing crossing the far end leaves a faint double.
  - *Near edges.* A far pixel right beside a near thing's edge can take up to 0.7 of a pixel of its colour.

Its macros, in `src/heat.h`:

| Macro | Value | What it is |
|---|---|---|
| `HEAT_TW`, `HEAT_TH` | 256 x 32 texels | the texture and the screen window, a texel a pixel (RGB565, not twiddled: 16 KB of video memory) |
| `HEAT_BAND_UP` | 22 px | the window's top over its anchor (to the horizon and a little above it) |
| `HEAT_NX`, `HEAT_NY` | 32 x 4 cells | the mesh (8 x 8 px a cell: 165 vertices, `HEAT_VERTS`) |
| `HEAT_DIST` | 70 m | the anchor: the road this far ahead of the eye, its far end |
| `HEAT_AMP_X`, `HEAT_AMP_Y` | 0.35 px, 0.7 px | how far a sample drifts on screen |
| `HEAT_WAVE_1`, `HEAT_WAVE_2` | 96 px, 64 px | the two waves' lengths across the band (12 and 8 cells) |
| `HEAT_HZ_1`, `HEAT_HZ_2` | 0.55, 0.85 | their turns a second |
| `HEAT_CLIMB` | 0.11 | phase a mesh row: the waves lean as they climb |
| `HEAT_ALPHA` | 0.5 | the layer's share at its middle (STRONG; LIGHT is half) |
| `HEAT_EDGE` | 4 cells | each side fades in over this many |
| `HEAT_LAG` | 2 frames | how old the picture is that the copy reads (1 or 2: an ESTIMATE until a console shows it) |
| `HEAT_SLIP_MAX` | 24 px | the layer is gone when the far picture moved this far since the copied frame |
| `HEAT_FOCAL` | 461 px | this demo's screen: pixels at 45 degrees off its middle (55 degrees high, 480 lines) |
| `HEAT_SLIP_SIGN_X` | -1 | this demo's screen x against (up x forward), checked against its own projection on the host |
| `HEAT_NIGHT_MAX` | 0.25 | the night level (0 day, 1 night) at which the haze is gone |

Its cost is in the Cost table below.

## Camera and thumbnail

The values are macros in `src/demo.h` (each can be set with `-D` through `EXTRA=`).

| Macro | Value | What it is |
|---|---|---|
| `CAM_HEIGHT` | 1.9 m (DRIVE, the wet live view); 1.5 m (the wet autorun); 3.3 m (the map and hills autoruns) | the chase camera over the car's ground |
| `CAM_DIST` | 6.0 m (DRIVE, wet); 7.0 m (the autoruns) | behind the car's centre |
| `CAM_PITCH` | 0.09 rad, about 5 degrees down (DRIVE, the wet live view); 0.10 rad (the wet autorun) | puts the horizon near 41 % from the top at the 55 degree field of view (40 % at 0.10), the car's centre near 61 % |
| `THUMB_MIRROR_W` | 128 px | the mirror inset's width (its height from the mirror's shape) |
| `THUMB_TILES_W` | 96 px | the shadow tiles' inset (the map builds) |
| `THUMB_RIGHT` | 620 px | both insets' right edge |
| `THUMB_TOP` | 20 px on the map, 124 px on the street (under its map) | the mirror inset's top |
| `THUMB_GAP` | 24 px | under an inset: its label, then the next |

The map and hills autoruns keep the old high camera so their stage numbers compare with the earlier console runs. The B + stick orbit turns the low camera round the car as before. The insets show the reflection target (it is not a rear-view mirror) and are drawn only with the tech overlay open (START), so DRIVE starts with neither.

## Cost

| Item | Cost |
| --- | --- |
| Reflection target, RGB565, street build | the mirror's own size: 128 x 64 = 16 KB, 256 x 128 = 64 KB (the street default), 512 x 256 = 256 KB of video memory with `--rtt` (exact) |
| Reflection target, RGB565, map build | twice the mirror's width (`RT_W`: the mirror on the left, the car shadow tiles on the right; the pitch is the width): mirror 128 x 64 = target 256 x 64 = 32 KB (the hills build's default), 256 x 128 = 512 x 128 = 128 KB (the c7 and c8 default), 512 x 256 = 1024 x 256 = 512 KB (exact, with `--rtt`) |
| Glow texture | 32 x 32 RGB565 = 2 KB (exact) |
| World textures, low look | 5 x 64 x 64 at 4 bits a texel: 10 KB, and 80 palette entries (exact) |
| World textures, full look | 4 facades 128 x 128 and the road 256 x 256, RGB565 with mipmaps: 381 KB (exact); both sets stay loaded for the switch |
| Resident in video memory, all at once | full textures 381 KB + low textures 10 KB + glow 2 KB + font 32 KB = 425 KB, plus the target. Street build at 256 x 128: 489 KB. Map build: 457 KB at 128 x 64 (the hills build's default), 553 KB at 256 x 128 (c7, c8; not 489 KB as this table said before), 937 KB at 512 x 256 (exact, with `--rtt`). Add the heat haze's 16 KB in the street, map and hills builds once its texture is made (the first frame it is on). Wet build at 128 x 64: 425 + 16 + 42.7 (the road layer) = 483.7 KB (exact) |
| Heat haze | video memory 16 KB (256 x 32 RGB565, exact), RAM 16 KB (the copy's rows) and under 1 KB of state; 165 vertices, 264 sent, 8.5 KB of TA (exact); the SH-4's 8,192 uncached reads and the store-queue load about 0.5 to 0.9 ms a frame, the PVR's 256 x 32 px translucent layer about 0.1 to 0.2 ms (both ESTIMATE, not measured); 0 when it is off, at night or in rain |
| Wet road, the pool | 35 rows x 13 columns = 455 vertices, each looked up once (455 lookups); 34 x 12 x 2 = 816 triangles a layer, 1,632 for the reflection and the road layer together (exact). Lookup time not measured on a console (the wet result page reads it). Dry: the road layer alone, no lookup |
| Wet road, the road layer | 128 x 128 ARGB4444 with mipmaps: 43,696 B of video memory (exact), the wet build only |
| Lookup | 320 looked-up vertices (364 in the body, the player car only): the page's column read 0.92 to 0.97 ms on a console, one run (PVR-160), about 2.9 to 3.0 us a vertex (DERIVED). (Earlier: 0.43 ms for 1,623 vertices on an emulator, an older, heavier car.) |
| Mirror pass, PVR | not measured (see the library README: a timer round the pass reads the PVR's queue) |
| Pools, halos, shadows | about 100 triangles on the translucent list at night: fill not measured |

## Host tests

From the library's root, `make test` runs these through its `test-demo` target (plain C; nothing here is a console result):

- `host/heat_test.c`: the heat haze's window, copy, slip and mesh.
- `host/controls_test.c`: `src/controls.h` against the pad read it replaced, every pad state, in each of the seven builds (street, map, hills, wet; the first three also as DRIVE); the keys added since (X heat, B + X fly-by, B + Y hide) each on their own.
- `host/snap_test.c`: the autorun never snaps the car or the camera: 1,890 frames through the kinds of stage change, each step within the car's speed. It is the demo itself on the host, so it needs the baked assets (`build/gen/assets_gen.c`) and SH4ZAM's software matrix file; `make test` runs the street layout when both are there and says NOT RUN when not.
- `python tools/grit_gen.py --self-test`, `python tools/check_coupe.py --self-test`: the two build tools that need no Pillow.

## Host preview

```sh
SH4ZAM=/path/to/sh4zam sh tools/preview.sh        # from demo/pl4nar, after the bake: build/preview/pairNN.png (off | on), cropNN.png
```
