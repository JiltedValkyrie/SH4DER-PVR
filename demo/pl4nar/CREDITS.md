# Credits: the street driving demo

## Art

None of this art is in the repository. `tools/fetch.py` downloads each pack (table: `fetch_table.json`, with the sha256 of the downloaded pack) and the build bakes it. All of it is CC0 1.0, a public domain dedication: no credit is required; these rows are thanks, and a record. The legal text is in `licenses/CC0-1.0.txt`.

| Pack | Author | Source | Licence | Used for |
| --- | --- | --- | --- | --- |
| Car Kit | Kenney (kenney.nl) | https://kenney.nl/assets/car-kit | CC0 1.0 | the parked taxi, police car and van, cut to the triangle budget by `tools/car_lod.py` |
| City Kit Roads | Kenney (kenney.nl) | https://kenney.nl/assets/city-kit-roads | CC0 1.0 | street lamp, pole, traffic light, cone, dumpster, cut the same way |
| Asphalt025A | ambientCG | https://ambientcg.com/view?id=Asphalt025A | CC0 1.0 | the road texture's base, lane lines drawn by `tools/bake.py` |
| Bricks075A | ambientCG | https://ambientcg.com/view?id=Bricks075A | CC0 1.0 | two facade textures, windows drawn by `tools/bake.py` |
| Concrete034 | ambientCG | https://ambientcg.com/view?id=Concrete034 | CC0 1.0 | a facade texture and the sidewalk |
| PaintedPlaster017 | ambientCG | https://ambientcg.com/view?id=PaintedPlaster017 | CC0 1.0 | a facade texture |

## Own work

The hero car (`tools/coupe.py`, a coupe built from numbers), the street layout, the sky, the glow texture and every line of the demo's code are this repository's own work, under its `LICENSE` (Copyright (c) 2026 Jilted Valkyrie). The font is Pillow's built-in bitmap font, rendered by `tools/bake.py`.

## Tools and libraries the build calls (none is vendored here, none is claimed)

| Tool | Licence | Role |
| --- | --- | --- |
| KallistiOS | the KallistiOS licence (BSD-style) | the Dreamcast runtime: PVR, render-to-texture, controller |
| SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis | MIT | all Dreamcast maths; linked as a library, its notice kept with it |
| Blender | GPL | the triangle cut of the stock models, as a command line tool; nothing of Blender is shipped |
| mkdcdisc | its own licence (see its repository) | writes the disc image |
| Python 3, Pillow | PSF licence, MIT-CMU | the bake and build scripts |
