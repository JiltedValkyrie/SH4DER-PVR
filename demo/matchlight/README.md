# Match Light: a 60 second short film demo (scaffold)

A wordless telling of Hans Christian Andersen's public-domain tale "The Little Match Girl" on a snowy night street, played
by a Dreamcast program in ten shots (M1 .. M10) and looping every 60 seconds. This folder is the SCAFFOLD: the program boots
and plays all ten shots in order with placeholder art built from numbers, so the look can be seen and then improved. Nothing
was run on a console or an emulator; the host test passes and the Dreamcast build compiles clean.

## Files

| File | What it is |
|---|---|
| `matchlight_scene.h` | Pure maths and data, no KallistiOS: timeline, shot table (cameras, lens, match state), the street tables, projection, light, cut overlay, sprite atlas values, counted triangle budget. Compiles on a PC. |
| `matchlight.c` | The Dreamcast program: boot stages, pad, clock, drawing, readout, and the three hooks (fog, stars, visions). |
| `../../test/matchlight_scene_test.c` | Host test of `matchlight_scene.h`. |

## Build

Host test (Git Bash, from the library folder):

    gcc -O2 -std=gnu11 -Wall -Wextra -Werror -DSH4DER_CORE_NO_SH4ZAM -Iinclude -Ish4dow-dc/include test/matchlight_scene_test.c -lm -o matchlight_scene_test && ./matchlight_scene_test

Dreamcast ELF (KallistiOS environment sourced, `$Z` = a SH4ZAM checkout):

    kos-cc -O3 -Wall -Wextra -Werror -Iinclude -Ish4dow-dc/include -I$Z/include -ffast-math -ffp-contract=fast -mfsrra -mfsca \
        -fno-math-errno demo/matchlight/matchlight.c src/sh4der_spark.c src/sh4der_motes.c -o build/matchlight/sh4der-matchlight.elf

Needs `include/sh4der_deform.h` (the snow depth grid), which is new and not yet in every checkout. No disc image is made here.
(A compiler note: the `NOINLINE` marks in `matchlight.c` keep the large functions out of `main`; with everything inlined this
toolchain's SH compiler stopped with an internal error in its register reload pass.)

## Pad

| Button | Action |
|---|---|
| A | pause / resume the clock (the snow stops too) |
| D-pad right / left | jump to the start of the next / previous shot (the footprints are replayed so the shot has the prints it should) |
| Y | show / hide the readout: shot, time, frames a second, frame time, TA kilobytes used, triangles sent, VRAM free (hidden at start) |
| X | show / hide the debug caption: the shot's name and a few words on what it shows, bottom left (shown at start) |

## The shots

Camera heights are 1.6 m except the M6 crane. The lens column is the HORIZONTAL field of view (the spec's "Lens (h FOV)"). The
girl is a silhouette at 10.7 m or more from the lens in every shot (counted by the test, along every camera move).

The street is an open lane 14 m wide: the house fronts stand 7.0 to 7.6 m out from its middle, and every camera stands inside
the lane (4.5 m out at the most). The test walks the straight line from each lens to the girl (and to the tree in M6, and to
both ends of the footprint trail in M2) and prints, per shot, the nearest house (3.3 m at the least) and the least height of
that line over the snow.

| Shot | Time | Lens | Match | What it shows now |
|---|---|---|---|---|
| M1 | 0:00 - 0:06 | 60 deg | none | Static street: ten gabled house fronts, pale windows with glows, night sky, the street fading into fog table 1, falling snow, a dark shape (the girl) far down the street in the fog. |
| M2 | 0:06 - 0:12 | 45 deg | none | The girl walks in from 24 m to 15 m; thirteen bare footprints are pressed into the snow behind her as real pits (0.32 m wide, 0.46 m long, 0.20 m deep). |
| M3 | 0:12 - 0:20 | 35 deg | 1.2 m pool | The strike at 0.6 s: warm pool and flame halo, the ground lit warm by distance, the girl lit from below, a stove-coloured placeholder quad. |
| M4 | 0:20 - 0:26 | 40 deg, slow pull | out in 0.4 s | The flame dies; the pool goes; snow falls on; the camera pulls back with an ease. |
| M5 | 0:26 - 0:34 | 40 deg | 1.6 m pool | A larger pool; a goose-coloured placeholder quad. |
| M6 | 0:34 - 0:44 | 50 deg, crane 1.6 to 4.0 m | 1.6 m pool | The small tree (flat cut-out, three tiers) at 3 m across, 17 m along; a green placeholder quad. From 4 s the twelve candles lift off one by one and come to rest as stars (`sh4der_rise`). |
| M7 | 0:44 - 0:48 | 50 deg, tilted up | none | The night sky above the street: ten fixed stars and the twelve candle stars. At 1.4 s a falling star crosses in 0.6 s. |
| M8 | 0:48 - 0:52 | 40 deg | 1.4 m pool | A pale placeholder quad for the grandmother. |
| M9 | 0:52 - 0:56 | 50 deg | 2.5 m pool, pale gold | The largest pool, a gold-white flash on the first two frames (0.85 then 0.43 alpha), ten lights lift off a ring round the flame from 0.6 s. |
| M10 | 0:56 - 1:00 | 70 deg, pull | none | The sky and the ambient light blend to cool dawn (sky (96, 112, 136)), the pits fill with new snow, the walls brighten. No warm colour. |

Between shots there is a 0.15 s dip to black (0.075 s either side of each cut; the cut into M9 is the flash instead).

## What is placeholder

- All shapes are flat vertex-coloured polygons; there is no texture on the set. The houses are boxes without roofs (a front wall,
  a gable triangle and a near side wall). Heights, widths and gables vary by table.
- The girl is four dark quads turned to the camera (dress, shawl, head, hands). No face, no animation (she slides).
- The visions are one soft additive quad each, in the vision's colour, inside the pool (`film_vision_draw`).
- The warm light on the ground is per vertex at 1 m spacing, so the pool's shape comes mostly from the two `sh4der_pool` fans.
  The fans follow the snow height at their 16 rim points only, so a drift can cut into the faint outer fan.
- The candles, the stars and the falling star are soft additive quads from the 4-cell placeholder atlas.
- The fog bank is ten upright sheets (`sh4der_mist`) behind the girl, with a 64 x 64 generated noise texture (8 KB of video memory).
- No sound.

## The snow

The ground is shaped, not flat (`ml_snow_height`, one function, read by the ground grid, the tree's foot and the pool):

| Part | Numbers |
|---|---|
| Wall banks | rise from 3.5 m out to the house fronts; 0.67 to 1.62 m high at the walls, waving along the street |
| Rolling drifts | 0 to 0.44 m, two crossed waves, over the whole lane outside the path |
| Piles | six round hills, 1.6 to 2.4 m radius, 0.75 to 1.20 m high |
| The girl's path | level (height 0) within 1.25 m of the street's middle, so her feet, the pool and the trail sit on a known height |
| Highest point / mean over the lane | 1.64 m / 0.40 m |

A slope is shaded by a fixed moon direction (far down the street, to the right): slopes that face the cameras are dark (down to
40 % of level snow), slopes that face the moon bright (up to 140 %). The heights and shades of the 1,066 ground vertices are built
once at start.

Footprints: the ground grid's 1 m quads cannot show a foot-sized pit (on the first build the prints were flat decals of about
7 x 2 pixels and could not be seen). So the grid's middle column along her walk (x -0.5 .. 0.5, z 14 .. 25) is replaced by the
TRAIL PATCH: 8 x 88 quads at 0.125 m, whose vertices take their height, darkening and slope shade from the `sh4der_deform` grid
(160 x 160 cells at 0.125 m, fixed over the walk). Each foot presses two overlapping round stamps (heel and toe). At the M2
camera a pit is 12 to 21 pixels wide and its dark far wall 8 to 13 pixels high; level snow at night is (60, 69, 100), the pit
floor (28, 32, 47), 47 % of it, and the wall that faces the camera goes down to 19 %. The patch is re-read from the grid only
when a print is added. Its outer ring of vertices is tucked 2 cm under the ground grid so the join has no crack.

## The three hooks

All three are called once a frame inside the translucent list, with `(shot, progress, time)`; each has a comment saying what
it draws. The frame's camera is `cam`, the sprite view `spark_view`, the screen `spark_screen`.

| Hook | Draws |
|---|---|
| `film_fog_draw` | The fog bank: camera-facing slices across the street at 15 - 30 m, 0 - 6 m high, thinner at dawn. Filled: `sh4der_mist`, sent far sheet first. |
| `film_stars_draw` | The star field, the candles lifting into stars (M6), the falling star (M7), the rise of lights (M9). Filled: `sh4der_rise` and two spark pools. |
| `film_vision_draw` | The stove, table, tree and grandmother visions. Currently a placeholder soft warm quad. |

## Library modules used, and not used

Really called: `sh4der_pool` (the two warm pool fans), `sh4der_spark` (flame halo, hot core, window glows, the placeholder vision:
the library's pool, quad maths and draw), `sh4der_motes` (falling snow, collect and the masked draw), `sh4der_deform` (the snow
grid: init, recentre, stamp, height, depth, slope), `sh4der_mist` (the fog bank), `sh4der_rise` (candles and lights lifting), `sh4der_guard` (every header and triangle), `sh4der_core`
(SH4ZAM wrappers for all the maths).

Not used (the scaffold draws its own): `sh4der_lens` (the cut dip and the M9 flash are one own full-screen alpha quad), `sh4der_look`,
`sh4der_sss`, `sh4der_glow`, `sh4der_focus` (these belong to the look pass).

Snow note: `sh4der_motes` works in a z-up space, so the snow's emitters and view are stored with y and z swapped.

## Cost (all COUNTED from the tables and constants, none measured on a console)

- Triangles a frame, upper bound with no culling: 3,690 (M1) to 3,733 (M6); the test prints each shot. Ground 1,978 of them
  (25 x 40 quads less the 11 the trail patch replaces), the trail patch 1,408 (8 x 88 quads), 50 for the house walls, 30 for lit
  windows, 8 for the girl, snow up to 192, pool fans 32 when a match burns. Not in that count: the fog bank (40) and the sky's
  lights (64 at the most).
- TA vertices: about 7,600 at the most, about 245 KB of the 512 KB vertex buffer (counted: 32 bytes a vertex).
- RAM, static: about 195 KB (readout image 64 KB, ground vertices 30 KB and their height and shade tables 8.5 KB, trail patch
  vertices 22.5 KB and its three tables 9.5 KB, snow dent grid 25 KB, fog noise and its texture copy 12 KB, sprite atlas staging
  8 KB, caption image 8 KB, snow collect buffer 4 KB, sprite pools).
- Video memory textures: 88.5 KB (sprite atlas 64 x 64 RGB565 8 KB, snow mask 16 x 16 0.5 KB, fog noise 64 x 64 8 KB, readout
  512 x 64 64 KB, caption 256 x 16 8 KB; the readout texture is allocated even when hidden). Plus the KallistiOS frame buffers
  and two 512 KB TA vertex buffers.
- Sound RAM: 0 KB.
- CPU: not measured. The ground and trail vertex loops (1,066 + 801 vertices: projection and light, no grid read) and the snow
  field (720 slots) are the big pieces.
