# The street driving demo (pl4nar)

A street, a coupe, a chase camera, a 45 second day. It shows three pieces of the library working together on a triangle budget a late 1990s console street could carry (the car is 280 triangles, a frame about 1,400 plus about 800 in the mirror pass):

- **`sh4der_pl4nar`**: the street mirrored about the road into a 256 x 128 render target; the car's paint, glass and chrome look it up per vertex and blend it in by Fresnel. **X** switches it off and on, so the flat look and the reflective one can be compared on one frame.
- **`sh4der_pool`**: at night each street lamp's pool of light on the road, the headlight beams, and a blob shadow under every car, as small fans of triangles.
- **`sh4der_spark_quad`**: the lamps' halos, and the same halos drawn into the mirror pass, so the lamps pass over the glass and the paint as the car drives under them.

The hour costs no layer: its tint goes into the vertex colours the frame computes anyway.

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

The table is `src/controls.h`, the table the pad is read through and the HUD's key lines come from; `python tools/check_controls.py` fails when this one differs from it. Builds: street (`--street`), map (the default), hills (the map built with `PL4NAR_HILLS`). When: DRIVE (the game program, `build.py` without `--tech`) has two key sets, the drive keys while the overlay is closed and the tech keys while START has it open; "always" is both. Every other program (the live view, the autorun) has the tech keys all the time.

| Keys | Action | Builds | When |
|---|---|---|---|
| **stick or D-pad left / right** | steer | all | always |
| **RT** | engine | all | always |
| **LT** | brake, then reverse | all | always |
| **X** | reflection on / off | all | tech |
| **Y** | time of day: AUTO (a 45 second day), DAY, NIGHT, PAUSE | all | tech |
| **A** | textures: low (64 x 64, 16 colours, the start) or full | all | tech |
| **A** | restart: the car at the start, stopped; the clock and the laps from 0 | all | drive |
| **D-pad up** | F0 (the reflection's strength) up | all | tech |
| **D-pad down** | F0 down | all | tech |
| **START** | the overlay (times, triangles, memory, the target as a thumbnail) on / off; off at the start on the hills and in DRIVE | all | always |
| **B + stick** | the camera round the car | all | always |
| **B + LT / RT** | move the split's divider (with the split on) | all | always |
| **B + X** | the split: the reflection on the left of a divider only | all | tech |
| **B + A** | the target's size: 256 x 128, 128 x 64, 512 x 256 in turn (the start: 256 x 128; the hills: 128 x 64) | all | tech |
| **B + D-pad up** | the reflection's night gain up | all | tech |
| **B + D-pad down** | the night gain down | all | tech |
| **B + D-pad right** | the sun: FIXED, SWEEP (to DAY unless NIGHT), HOLD | all | tech |
| **B + D-pad left** | baked occlusion on / off | map, hills | tech |
| **B + Y** | car shadow tiles on / off (off: blob discs) | map | tech |
| **B + Y** | car shadow: on the ground mesh, one flat quad, off (blob discs) in turn | hills | tech |
| **B + START** | the effect labels shown / hidden | map, hills | tech |

In the autorun (the measuring copy) the driver holds the steering, the engine and the brake; every other key acts as it does live and marks the stage TOUCHED on the result page (B + stick and B + LT / RT change no work and mark nothing). No key ends the autorun: it runs every stage, then shows its result pages, and a key pressed during a stage only marks it. The tile cut and the small tile set have no key: they are autorun stages only.

## Cost

| Item | Cost |
| --- | --- |
| Reflection target, RGB565, street build | the mirror's own size: 128 x 64 = 16 KB, 256 x 128 = 64 KB (the street default), 512 x 256 = 256 KB of video memory with `--rtt` (exact) |
| Reflection target, RGB565, map build | twice the mirror's width (`RT_W`: the mirror on the left, the car shadow tiles on the right; the pitch is the width): mirror 128 x 64 = target 256 x 64 = 32 KB (the hills build's default), 256 x 128 = 512 x 128 = 128 KB (the c7 and c8 default), 512 x 256 = 1024 x 256 = 512 KB (exact, with `--rtt`) |
| Glow texture | 32 x 32 RGB565 = 2 KB (exact) |
| World textures, low look | 5 x 64 x 64 at 4 bits a texel: 10 KB, and 80 palette entries (exact) |
| World textures, full look | 4 facades 128 x 128 and the road 256 x 256, RGB565 with mipmaps: 381 KB (exact); both sets stay loaded for the switch |
| Resident in video memory, all at once | full textures 381 KB + low textures 10 KB + glow 2 KB + font 32 KB = 425 KB, plus the target. Street build at 256 x 128: 489 KB. Map build: 457 KB at 128 x 64 (the hills build's default), 553 KB at 256 x 128 (c7, c8; not 489 KB as this table said before), 937 KB at 512 x 256 (exact, with `--rtt`). The planned heat haze's 128 x 128 RGB565 frame texture would add 32 KB (exact for that size; not built yet) |
| Lookup | 320 looked-up vertices (364 in the body, the player car only): the page's column read 0.92 to 0.97 ms on a console, one run (PVR-160), about 2.9 to 3.0 us a vertex (DERIVED). (Earlier: 0.43 ms for 1,623 vertices on an emulator, an older, heavier car.) |
| Mirror pass, PVR | not measured (see the library README: a timer round the pass reads the PVR's queue) |
| Pools, halos, shadows | about 100 triangles on the translucent list at night: fill not measured |

## Host preview

```sh
SH4ZAM=/path/to/sh4zam sh tools/preview.sh        # from demo/pl4nar, after the bake: build/preview/pairNN.png (off | on), cropNN.png
```
