# Credits: corr-a, the texture filtering corridor

## Art

None of this art is in the repository. `demo/pl4nar/tools/fetch.py --root demo/psm/corr` checks the pack (table: `fetch_table.json`, with the sha256 of the downloaded pack) and `tools/corr_bake.py` bakes it. It is CC0 1.0, a public domain dedication: no credit is required; this row is thanks, and a record. The legal text is in `licenses/CC0-1.0.txt`.

| Pack | Author | Source | Licence | Used for |
| --- | --- | --- | --- | --- |
| Retro Textures Fantasy | Kenney (kenney.nl) | https://kenney.nl/assets/retro-textures-fantasy | CC0 1.0 | the floor (floor_stone_pattern), the walls (wall_brick_stone_center), the ceiling (floor_wood_planks_wide) and the doorway (door_metal_gate), upscaled and given mipmaps by `tools/corr_bake.py` |

## Own work

The corridor's layout, the torch, the bake tool and every line of the program are this repository's own work, under its `LICENSE` (Copyright (c) 2026 Jilted Valkyrie). The page font is `../psm_page.h`'s.

## Tools and libraries the build calls (none is vendored here, none is claimed)

| Tool | Licence | Role |
| --- | --- | --- |
| KallistiOS | the KallistiOS licence (BSD-style) | the Dreamcast runtime: PVR, controller |
| SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis | MIT | all Dreamcast maths; linked as a library, its notice kept with it |
| Python 3, Pillow | PSF licence, MIT-CMU | the bake script |
