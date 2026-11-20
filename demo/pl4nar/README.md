# The street driving demo (pl4nar)

A street, a coupe, a chase camera, a 45 second day. It shows three pieces of the library working together on a triangle budget a late 1990s console street could carry (the car is 280 triangles, a frame about 1,400 plus about 800 in the mirror pass):

- **`sh4der_pl4nar`**: the street mirrored about the road into a 256 x 128 render target; the car's paint, glass and chrome look it up per vertex and blend it in by Fresnel. **X** switches it off and on, so the flat look and the reflective one can be compared on one frame.
- **`sh4der_pool`**: at night each street lamp's pool of light on the road, the headlight beams, and a blob shadow under every car, as small fans of triangles.
- **`sh4der_spark_quad`**: the lamps' halos, and the same halos drawn into the mirror pass, so the lamps pass over the glass and the paint as the car drives under them.

The hour costs no layer: its tint goes into the vertex colours the frame computes anyway.

The world's textures come in two sets, switched live with **A**: the low look (the default: the road and four facades at 64 x 64, 16 colours each in a PVR palette bank, point sampled, no mipmaps, 2,048 B a texture) and the full one (128 x 128 and 256 x 256, filtered, with mipmaps).

Status, plainly. **Proven:** it compiles for the host and for the Dreamcast on this library's headers with no warning, and its host preview (a software rasteriser over the same code) draws the same picture as the version that ran on an emulator before it moved into this folder: every mirror target identical, the screens apart only by two renamed labels. **Not proven:** `build.py` end to end (its steps were run by hand, on art copied in, not fetched); the fetch table's links and hashes (never run against the network, nothing pinned); the build without `--rtt` (compiled as a library file, never run); any boot of an image built from this folder, on an emulator or a console; the order of the two texels in a byte of the 16-colour textures (the host preview assumes the same order as the bake, so only a boot tells). No time below is measured on a console.

## Build

```sh
python demo/pl4nar/build.py --sh4zam /path/to/sh4zam --rtt      # in a shell with the KallistiOS environment sourced
```

One script from a clean checkout to `demo/pl4nar/out/pl4nar-demo-b<N>.cdi`: it checks the credits table, fetches the CC0 art (`fetch_table.json`; nothing third-party is in this repository), builds the coupe from numbers, cuts the stock models to the budget in Blender (headless), bakes, compiles and writes the image. An image is never overwritten. Leave `--rtt` out on a KallistiOS without `pvr_scene_begin_rtt` (the target then costs 256 KB of video memory, not 64 KB; that path is compiled, not yet run). From the library's root, `make pl4nar-demo SH4ZAM=...` runs the same script.

The fetch table is not pinned yet: the first run needs `--pin`, which prints each pack's sha256 to put in `fetch_table.json`. The two Kenney packs have no stable direct link: the script says where to download them by hand.

Needs: KallistiOS and its toolchain, SH4ZAM, Blender, mkdcdisc, Python 3 with Pillow. See `CREDITS.md` for every licence.

## Controls

Stick or D-pad left / right: steer. **Right trigger** engine, **left trigger** brake (then reverse). **X** reflection on / off. **Y** time of day: AUTO (a 45 second day) / DAY / NIGHT / PAUSE. **A** the look: low (64 x 64, 16 colours) or full. **B** + **A** target size (256 x 128, 128 x 64, 512 x 256). D-pad up / down: F0, the reflection's strength. **B** held: the stick turns the camera round the car, D-pad up / down sets the reflection's night gain. **START**: the overlay (frames a second, the PVR's time, the lookup's time, triangles, memory) and the target as a thumbnail, turned right side up.

## Cost

| Item | Cost |
| --- | --- |
| Reflection target, RGB565 | 256 x 128 = 64 KB of video memory with `--rtt` (exact) |
| Glow texture | 32 x 32 RGB565 = 2 KB (exact) |
| World textures, low look | 5 x 64 x 64 at 4 bits a texel: 10 KB, and 80 palette entries (exact) |
| World textures, full look | 4 facades 128 x 128 and the road 256 x 256, RGB565 with mipmaps: 381 KB (exact); both sets stay loaded for the switch |
| Resident in video memory, all at once | full textures 381 KB + low textures 10 KB + glow 2 KB + font 32 KB + target 64 KB = 489 KB (exact, with `--rtt`) |
| Lookup | 0.43 ms for 1,623 vertices on an emulator, with an older, heavier car; the coupe has 364 body vertices: not measured |
| Mirror pass, PVR | not measured (see the library README: a timer round the pass reads the PVR's queue) |
| Pools, halos, shadows | about 100 triangles on the translucent list at night: fill not measured |

## Host preview

```sh
SH4ZAM=/path/to/sh4zam sh tools/preview.sh        # from demo/pl4nar, after the bake: build/preview/pairNN.png (off | on), cropNN.png
```
