# The AO split demo (ao4): credits

Copyright (c) 2026 Jilted Valkyrie. The demo is part of SH4DER-PVR and is covered by the repository's `LICENSE`. No third-party asset, texture, model, sound or scan is in it.

| Part | Where it comes from |
|---|---|
| The program, the scenes A and B, the bake, the split | Own work: `ao4_spike.c`, `ao4_scene.h`, `ao4_draw.h`, `ao4_room.h`, `ao4_hens.h`, `../ao_layer.h`; the tile cut it uses is the library's `include/sh4der_tilecut.h` (moved from `tile_cut.h` here) |
| The living room (scene C) | Own work, modelled from numbers by `room/tools/build_room.py` (`room/README.md`); the OBJ files in `room/export` are its output. No reference model was traced |
| The hens | Own work: the procedural hen of the town's mesh (`../tools/bake_town.py`), copied unchanged by `../hero/tools/hero_hens.py`; their walk is `../psm_town.h`'s |
| The room's baked AO and lightmaps | Made at build time by `room_import.py` and `tools/room_bake.c` from the files above |
| The text on screen | Drawn at run time from the console's own BIOS font through KallistiOS's `bfont`, reduced to 8 x 16 by `../psm_page.h`. No font file is shipped |

## Libraries it is built with (not shipped here)

- **SH4ZAM** by Falco Girgis, MIT licence: the SH-4 maths (`shz_*`). Used as the library KallistiOS carries; none of its files are in this repository.
- **KallistiOS**: the Dreamcast system library and its PVR driver. Its own licence applies to it.

## Read, not copied

- Sega's list of known faults of the PowerVR2 core, for the clip guard (CORE 18: a change of user clip takes effect a polygon early). A fact from a document; no code.
- KallistiOS's `pvr_buffers.c` and `pvr_misc.c`, for the layout of the tile table that the tile cut (`include/sh4der_tilecut.h`, once `tile_cut.h` here) checks and rewrites. The layout is restated in our words and tested; no code is copied.
