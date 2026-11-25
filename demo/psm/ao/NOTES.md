# ao0: the hardware questions of AO method 2

**It ran on 2026-11-25 (suite s3, one console). The result, in ledger form with the pages typed and CRC-checked, is `../compare/SUITE_RUN23_RESULTS_senior.md`, sections 3 to 5.** In short: the depth union fails as one darkening a direction (it acts like one side: about one darkening a caster); one side works as the host test said; a direction costs the SH-4 1,824 us and the PVR 1,213 us at 1,164 caster triangles, 68 KB of TA; one side 1,278 us, 753 us and 34 KB. The rest of this file is the note written before the run.

Built 2026-11-25 by Senior. **Not run on a console.** Nothing below is a hardware result. The method: `../AO_METHOD.md` and `../ao_layer.h`.

## The image

| Item | Value |
|---|---|
| Image | `demo/psm/out/cdi/psm-ao0-b.cdi` |
| sha256 | 5e930450b232d3da6cc0d373fa842e5ab6b70ebb9630ceec2e07855e88203356 |
| Size | 1,752,969 bytes |
| ELF for the suite | `build/psm_ao/psm-ao0-b.elf`, sha256 bd154e0eb1a3f2792e4208ab9ed3aeab28ccbe762c9f765d0cb8b67fee937c78 |
| Build | `python demo/psm/ao/build.py --tag b` (log `D:\tmp\psmbuild\logs\ao0_b_20261125_111144.log`, no compiler warning) |
| Source | `ao/ao0_spike.c` (the program), `ao/ao0_model.h` (scenes and the model of the target), `ao_layer.h` (the method) |

- The standalone images `psm-ao0-a.cdi` and `psm-ao0-b.cdi` were recycled by lead the same day: the owner wants one disc. ao0 reaches the console as the suite's entry (s3), from the ELF above.
- It runs by itself: no pad needed. It ends on 2 text pages. START on the pages opens a live view.
- Only PVR registers and video memory are written. The fault catcher is in. "Ready" and "render done" are polled on the timer; no wait is longer than 200 ms.

## What it asks

| Question | Test | How the page answers |
|---|---|---|
| Does depth "greater" with depth write, in a translucent list drawn in send order, give one darkening a direction in a render-to-texture pass? (nearest ledger row: PVR-046) | 1, rows `U` | `1 DERIVED UNION WORKS` or `FAILS`. Per K: `c` values / levels the model has, `b` texels that read wrong of `n` compared |
| Does one side of the triangles alone give the same? | 1, rows `C` | `1 DERIVED ONE SIDE ...`; `b` is where two casters' shadows overlap |
| Which 565 values does the target hold after 0 .. K darkenings? | 1, lines `U565`, `C565` | the values, lightest first. The host's guess for K = 8 (8-bit blend, top bits kept): ffff ef5d d6ba c638 b5b6 a534 9cd3 8410; ESTIMATE |
| Is the target read back upside down? | 1, `f` | 0 or 1 |
| What does a direction cost at the town's caster count? | 2 | `v` vertices, `e` TA KB estimated, `k` KallistiOS's figure, `ao` and `sun` SH-4 us, `pvr` PVR us mean, `mx` largest; `2 DERIVED a direction` |
| Does the TA run out? | 2, last line | counts of the three out-of-memory events |

The rows of test 2: K = 0, 1, 2, 4, 8 with the union; K = 8 plain (`P`), one side (`C`), union and one side (`UC`); K = 4 `UC`.

## Host tests (they test the maths and the program's reading, not the PVR)

| Check | Result |
|---|---|
| `ao_layer_test.c`: directions, alphas, depths | pass |
| the slide matrix against a ray to the ground; "slide then map" against the two steps | pass, 2,000 cases |
| a ground point is under a slid triangle exactly when a ray from it toward the sky direction hits the triangle | pass, 19,976 cases, 426 hits |
| a software target (depth "greater" with write, in order): each texel holds `ao_level(k, directions blocked)`; plain blending is never lighter and somewhere darker | pass, K = 1, 2, 4, 8 |
| `psm_ao2_send` through the town's counting sink: vertices, one header, no sink fault, an object off the map left out | pass |
| `ao0_model_test.c`: scene counts (48 and 1,164 triangles); one side covers what both sides cover with half the triangles; the reading is right for a union-drawn target either way up and wrong for a plain-drawn one; the K + 1 levels are distinct 565 values | pass |

Numbers the host gives for the load scene, exact counts: 3,492 vertices and 68 KB of TA a direction; K = 8: 27,936 vertices, 545 KB. One side: half.

## What the result decides

| Result | Then |
|---|---|
| UNION WORKS | the town keeps `psm_ao2_unite = 1`. PVR-046's nearest question gets a MEASURED row. |
| UNION FAILS, ONE SIDE reads as the union | the town uses plain blending with `psm_ao2_cull`, if its casters are closed and wound one way (not checked). Overlapping casters darken twice. |
| both fail | plain blending, both sides: every direction darkens a closed caster's shadow twice. The alpha is then set for two darkenings a direction. |
| the cost a direction is near the estimate (about 1.7 ms SH-4, 1 to 2 ms PVR) | variant A at K = 4 fits; variant B (the static town into an atlas once) is built only if the owner wants K = 8 |
| TA out-of-memory events at K = 8 | K = 8 is for one side only, or not offered |

## Open

- Everything in "What it asks": OPEN until the image runs.
- The town's casters: closed and wound one way? OPEN; a host compare of the map with cull 0 and cull 1 answers it.
- The blend in a 565 target: at 8 bits with one write at the end, or banded a pass? The `U565` lines against the guess answer it.

---

# ao4: AO side by side on simple scenes (built, not run)

Built 2026-11-25 by Senior, after the owner could not tell whether the town's AO was visible. **Not run on a console.** The pictures in `ao/preview/` are a software render on the host of what the program is meant to show: they show the look; they are not the PVR's picture and no cost.

| Item | Value |
|---|---|
| ELF for the suite | `build/psm_ao/psm-ao4-b.elf`, sha256 8e9ac90f872b845edc47888b0befeb68949804328462ca484860f13be0326913 |
| On | `psm-suite-s4c.cdi` as entry ao4. (ao4-a, on s4 and s4b, changed the user clip with no dummy polygon before it: by Sega's bug list, CORE 18, the last strip of the left half would take the right half's clip. ao4-b sends a dummy triangle first. Do not run ao4-a.) |
| Build | `python demo/psm/ao/build.py --spike ao4 --tag b --elf-only`, no compiler warning |
| Source | `ao/ao4_spike.c` (the program), `ao/ao4_scene.h` (scenes, bake, light, camera), `ao/ao4_draw.h` (the shadow target's triangles), `ao/test/ao4_scene_test.c` (host test and preview) |

**What it shows.** The same scene twice, split down the middle by the PVR's user clip: left one method, right another.
- Scene A: a big sphere, a small sphere and a box on a plain ground. Scene B: two walls meeting in a corner, a sphere in it. (Scene C, the living room, is not built: it waits for the room's files and an importer.)
- OFF; 1 BAKED (a byte a vertex, 64 rays at load against the exact shapes, on the ambient light: ground, walls and shapes); 2 PVR MAP (the casters into the sun's shadow target from K sky directions, one side, plain blending: the ground only).
- Looks: CLAY (grey, ambient 0.72, sun 0.30: the default), COLOUR, AO ONLY.
- It runs by itself first (scene A then B, the splits OFF | 1, OFF | 2, 1 | 2, 6 s each), then one result page and one graph. START on the pages is the live view.
- Live: X split, Y swap sides, A EXAGGERATE x2, B look, L / R scene, up / down K (2, 4, 8), left / right method 2's reach, stick the camera.

**Counts (exact, host test).**

| | Scene A | Scene B |
|---|---|---|
| Vertices (ground) | 2,142 (1,369) | 2,544 (1,369) |
| Triangles on screen, one half | 3,816 | 4,656 |
| Caster triangles (one side sends half) | 1,076 | 648 |

The scene is drawn twice a frame, so the screen is about 7,600 to 9,300 triangles. That is above the "1 to 2 thousand" asked for; the ground's third-of-a-metre grid is what method 1 needs to show a contact shadow in vertex colours.

**Host test (the maths and the scenes, not the PVR).** The ray set is cosine-weighted; open ground bakes to 255, the ground under the big sphere to 0, a wall's foot to 115, the corner to 42; every caster drawn one side covers what both sides cover (0 texels differ, 8 directions, both scenes); the camera keeps every vertex in front with the pad at its limits.

**What the host preview showed, and what was changed for it.**
- Method 1 is plainly visible in CLAY: under the spheres, at the box's foot, along the walls' feet and up the corner.
- Method 2 at ao_layer.h's 60 degrees of elevation looks like K extra shadows, not like contact darkening: a 5 m wall darkens 2.9 m of floor in steps. So ao4 has a "reach" setting, the slide's length as a share of ao_layer.h's: 35, 60 (default), 80, 100 %. At 60 % the darkening stays near the caster's foot. The town's `psm_ao2_send` has no such setting yet.
- With K 4 the four discs under a sphere are still told apart. K 8 is smoother. That is the method's nature, and the split screen is there to show it.

**ESTIMATE of the costs (until run), from ao0's measured rates.** Method 2, scene A, K 4, one side: 538 triangles a direction, about 0.6 ms a direction on the SH-4 by ao0's 522 ns a vertex sent, so about 2.4 ms; PVR about +0.4 ms a direction. Method 1: one multiply a vertex, under 0.1 ms. The bake at load: 2,142 and 2,544 vertices × 64 rays against 4 or fewer shapes; well under a second each (ESTIMATE).

**Not tested on hardware before this build:** the user clip (first use here), a render target used as a texture in the opaque list of the next scene (ao0's live view did it; it was not photographed), the text texture upload between frames.

# The town's casters: closed and wound one way? No.

`ao/test/ao_casters_test.c` (host), run on the town's scene with the movers placed: **39 caster objects, 1,162 triangles**. For each object it rasters the shadow from 8 sky directions three times (both sides, side 1, side -1) and counts the texels where one side is not the shadow of both.

| Result | Objects |
|---|---|
| Either side draws the same shadow | 2 (objects 10, 17) |
| Side 1 only | 17: objects 8, 9, 16, 24, 25 and all 12 movers (27 to 38) |
| Side -1 only | 0 |
| **Both sides needed** | **20**: 0 to 7, 11 to 15, 18 to 23, 26 |

- The 20 are the houses and the larger props: they have open edges (16 to 96 an object), eight of them an edge wound the wrong way, and their signed volumes are of both signs (14 objects positive, 25 negative). They are shells, not closed solids.
- So **one side is not available for the town as it is.** It could be used for the 17 objects that allow it (546 of the 1,162 triangles, a saving of 273 a direction, 23 %), with a flag an object. Closing the houses is a change to `bake_town.py`'s casters (Junior's), not a trivial one.
- What the town can run today is what ao1 ran: both sides with the depth union, which ao0 showed acts as about one darkening a caster.

**The look tried once on the host: 32 faint directions.** `preview/ao4_a_try32_*.png`, `ao4_b_try32_baked_map.png`: method 2 with 32 directions at four reaches, each a 32nd of the darkening. It reads as soft contact darkening, close to method 1 on the ground; the stacked discs are gone. It is not built: 32 directions a frame is about 8 times K 4's cost. Two ways to get it for less, both ESTIMATE and not built: (1) variant B, the static casters' 32 directions drawn once into an atlas (the cost is at load; a frame draws one quad); (2) accumulate over frames, each frame's target starting from the last one (drawn as one textured quad, faded a little toward white) with 2 to 4 new jittered directions on top; a moving caster then leaves a soft trail a few frames long.

---

# ao4-c: scene C, the living room, and the hens (built, not run)

Built 2026-11-25 by Senior. **Not run on a console.** The pictures `ao/preview/ao4_c_*.png` are a software render on the host: the look, not the PVR's picture and no cost.

| Item | Value |
|---|---|
| ELF | `build/psm_ao/psm-ao4-c.elf`, sha256 340e0b8358cf1330a7dacaba87fe24a7ede508362350841a5d59c7c7a98629ec, no compiler warning |
| On | `psm-suite-s5.cdi` as entry ao4 |
| Build | `python demo/psm/ao/build.py --spike ao4 --tag c --elf-only` (makes the room's data and the hen's header in `build/psm_ao/room` first) |
| New source | `ao/ao4_room.h` (the room as a scene, the lightmap quads), `ao/ao4_hens.h` (the hens), `ao/room_import.py`, `ao/tools/room_bake.c`, `ao/room_data.h` |

**The room** is Cortana's export (`ao/room/export`, 15 objects, 1,282 triangles), through the importer:
- The floor and the two walls come as one quad each and are cut into a 0.25 m grid for the vertex AO (625 + 325 + 325 vertices, a strip a row). Their lightmaps are 128 × 128 (floor), 64 × 128 and 128 × 64 (walls), 8-bit AO, loaded as RGB565 greys.
- `rug` and `floor_border` are **left out**: they lie flat on the floor and would cover its AO (vertex, lightmap and PVR map alike). The rug's tint goes on the floor's vertices under it (COLOUR look, vertex side only).
- On screen: 3,323 vertices, 3,536 triangles; 1,000 caster triangles. Every caster but the two plants is closed and wound one way (host check: one side covers what both sides cover, 0 texels differ, 8 directions).
- The bake (host, 64 rays a point, reach 2.5 m): a point of a flat surface that starts inside a solid (the furniture sinks 1 cm into the floor, the trim into the walls) is not baked but filled from the points beside it; an object's own vertex inside another of its shells is baked a little way into its triangles. Without that the walls baked blotchy and the picture in the frame black.
- The camera is the model picture's eye at 0.7 of its distance, swinging 0.45 rad each way. The room is right-handed; scenes A and B were drawn mirrored and stay so (`cam_rh`).

**Modes in scene C** (X): OFF | 1 vertex AO, 1 | 3 lightmap, OFF | 3, 3 | 2 PVR map. The sun shades by N.L and casts nothing here. Method 3 draws each flat surface as one quad (texel × ambient + the sun's share as the offset colour) and the furniture with its vertex bytes.

**The hens** (`ao4_hens.h`): the town's hen template untouched (through Junior's `hero/tools/hero_hens.py`), the town's walk (peck, turn, step; same homes, bounds and seeds), turned a quarter and moved into the room: two keep to a pen of 1.2 × 2.7 m on the open floor in front, two roam 2.1 × 5.6 m over the rug's place and under the table. No collision: a hen can pass through an ottoman's corner. What is left out of the town's rule: the run from a walker. The walk advances in steps of 1/60 s, so a run repeats. D-pad down in scene C: hens on / off. They add 280 triangles a half and 80 caster triangles.
- What they show: sides 1 and 3 do not darken the floor under a hen (the bake does not know it); side 2 does, every frame.

**Counts and ESTIMATES (until run).**

| | 1 vertex AO, a half | 3 lightmap, a half |
|---|---|---|
| Vertices sent (with hens) | 6,680 (surfaces 2,424, furniture 3,696, hens 560) | 4,268 (surfaces 12) |
| TA bytes (ESTIMATE, psm_send.h's rule) | about 157 KB | about 91 KB |
| Video memory | 0 | 64 KB (the three lightmaps; ao4 holds a second set for EXAGGERATE) |

- The furniture is sent as single triangles in both, so it hides part of the gap; the surfaces alone are 2,424 vertices against 12. (Corrected 2026-11-25 from the console's own count on s6b's photographs, 6,680 and 4,268: the first version of this table had 6,936 and 4,548, the hens counted as three vertices a triangle where they are strips of four, and one wall's rows miscounted. The TA estimates on the screen were 147 KB and 83 KB, not the 157 and 91 below.)
- SH-4 send by ao0's 522 ns a vertex (ESTIMATE): 3.6 ms against 2.4 ms a half. The PVR's fill is the same picture in both; what a textured quad costs it against flat-shaded strips is OPEN.
- Method 2 in the room, K 4: 1,080 caster triangles, about 580 a direction sent (one side; the plants both), about 1,750 vertices a direction: about 0.9 ms a direction on the SH-4, 3.7 ms for four (ESTIMATE). With the hens off 40 fewer triangles a direction.
- The hens' walk and placing: 4 × 156 points through a rotation each frame; ESTIMATE under 0.2 ms. The program times it and prints it on the cost line and the page.

**The run that counts** is now 11 stages of 6 s: A and B as before, then C: OFF | 1, 1 | 3, OFF | 3, 3 | 2 with the hens, 3 | 2 without. The page is about two pages now, and the graph.

**Not tested on hardware (besides ao4-b's list):** a textured opaque polygon with an offset colour inside the user clip; rectangular non-twiddled 565 textures (128 × 64, 64 × 128).

**Host preview, what it showed.** OFF | 3 is the clearest pair: the room goes from flat to grounded. 1 against 3: the same AO; the lightmap is sharper at the furniture's feet and the wall panels (0.047 m a texel against a 0.25 m grid). 3 | 2 with K 4: method 2's four slid shadows of the lamp and the plants read as extra shadows, as in scene A.

---

# ao4-d: mode 3+M, a tile a hen (built, not run)

Built 2026-11-25 by Senior on lead's go for the small spike of `../AO2_DESIGN.md` (the movers' half only). **Not run on a console.**

| Item | Value |
|---|---|
| ELF | `build/psm_ao/psm-ao4-d.elf`, sha256 5c29e3ceb2cac2be5d6c1f4b04512764bd913f828f3dcccaf3d7c5e8cf88476a, no compiler warning |
| On | `psm-suite-s6.cdi` as entry ao4 |
| Source | `ao/ao4_hens.h` (`ao4_hens_tiles`, `ao4_hen_quad`), `ao/ao4_spike.c` (`m_pass`, `m_quads`) |

**What 3+M is.** Scene C's lightmaps, and for each hen a 32-pixel tile (1.28 m, 4 cm a texel) in a 128 × 128 target M: the hen's caster from the K sky directions, one side, plain blending, every frame; then one quad a hen on the floor (1 cm over it), a multiply layer (destination colour × texel) in the translucent list, inside the user clip. The tiles stand a tile apart, and the host test walks the hens two minutes and checks every triangle stays a texel inside its tile at K 8 and the longest reach: no clip between tiles is needed. A tile is one PVR tile.

**New splits in scene C** (X): 3 | 3+M, 3+M | 2, 3+M | 3+M. The run that counts has 15 stages of 6 s now (about 90 s); the last two are 3+M | 3+M with and without target M and its quads, so the page gives the pass and the quads' fill apart:
- "C target M: send, vertices, PVR, TA": the SH-4's send of the tiles and the PVR's render of target M (the pass's floor, PVR-004, is expected: about 3.2 ms).
- "C 4 quads, N px: screen PVR with / without": the fill of four small blended quads, with their area in pixels, so a rate a pixel can be DERIVED for larger quads. Four hen-sized quads do not answer T3 for a town's 39 house-sized quads by themselves.

**ESTIMATES until run.** Target M's send: 4 hens × 10 triangles a direction (one side) × K 4 = 160 triangles, 480 vertices, about 0.25 ms by 522 ns a vertex; TA about 10 KB. Against 3 | 2's whole map in the room: about 3.7 ms of sending. PVR: one more target pass.

**The PVR-092 cut is not in.** It rewrites KallistiOS's tile tables in video memory (c4's last stage did it once, and shut the PVR down if a word did not restore). Doing that every frame is not the cheap version lead asked for; a user tile clip on the polygons does not shorten the PVR's walk over the target's tiles. So the page reports the uncut pass. If the pass's floor is what makes 3+M lose to 3 | 2, the cut is the next spike.

**Not tested on hardware (new in d):** a 128 × 130 render target; a destination-colour × zero blend with a target as its texture inside the user clip in the translucent list; the user clip changed in the translucent list (a translucent dummy polygon first, CORE 18); four target passes and the screen in one frame (3+M | 2).

**Host preview** (`ao/preview/ao4_c_lmapm_*.png`, `ao4_c_lmap_lmapm_hens.png`): the hens get a soft dark patch under them on the 3+M side, like the 2 side's, while the furniture's AO stays the lightmap's.

---

# hens-eyes, entry hen-a: rays against a collision sweep (built, not run)

Built 2026-11-25 by Senior on Cortana's modules (`ao/hen_eyes.h`, `ao/hen_sweep.h`, `ao/HEN_EYES.md`). **Not run on a console.**

| Item | Value |
|---|---|
| ELF | `build/psm_ao/psm-hen-a.elf`, sha256 df0b507657a7953ec40c41472d71d2779267ce13058ddadfab7971f16be42d16, no compiler warning |
| On | `psm-suite-s6b.cdi` as entry hens-eyes |
| Build | `python demo/psm/ao/build.py --spike hen --tag a --elf-only` |
| Source | `ao/hen_spike.c` (the program), `ao/hen_room.h` (the hens, the four ways of steering, the timing points), `ao/test/hen_room_test.c` |

**What it is.** The living room with its vertex AO (CLAY) and the town's hens (4, 8, 16 or 32), steered by EYES (9 rays and 2 down on a 0.1 s tick), SWEEP GRID every step, SWEEP GRID on the tick, or SWEEP NAIVE (every triangle, every step). The hens, their starts and the walk are the same in every mode; the walk goes in steps of 1/60 s, and a change of mode starts them again, so a mode's run repeats. The first four hens are ao4's (the town's homes, bounds and seeds); from the fifth on they start at fixed-seed places in the roamers' bounds.

**What the page gives, a stage:** the cycle counter (PRFC1) round the eyes or the sweep alone for all the hens, a step: mean and dearest step; the push and the hens-apart part beside it; the exact counts for 100 steps (ticks, rays, box, triangle and pair tests; or casts, triangle tests, rejects, cells, pairs); the modules' model on those counts (ESTIMATE); steps, frames, frame ms, PVR us. The run that counts: 4 and 16 hens in the four modes, 32 hens with EYES and SWEEP GRID step, 5 s each (50 s). Then a bar graph of the mean cycles a step.

**Host test** (`hen_room_test.c`, the real room: 9 objects, 1,000 triangles, a 16 × 16 grid with 788 items): for 4, 16 and 32 hens in each mode, a minute: two runs give the same places bit for bit; no hen leaves the floor or stands inside a footprint; the hens move; the pair count is n (n − 1) / 2 a step.

**The model's cycles a step on the host's counts (ESTIMATE, Cortana's cost tables; the console run replaces them):**

| Hens | EYES | SWEEP GRID step | SWEEP GRID tick | SWEEP NAIVE |
|---|---|---|---|---|
| 4 | 4,240 | 10,348 | 1,641 | 88,299 |
| 16 | 35,191 | 101,991 | 14,844 | 409,502 |
| 32 | 110,482 | 257,641 | 41,676 | 873,919 |

- By the model the sweep on the same tick is cheaper than the eyes, and the eyes are cheaper than a sweep every step. The ranking rests on two guesses (220 cycles a capsule-triangle test, 74 a ray's own work); only the triangle test's 62.49 is measured. The console's "pre" line is what settles it.
- The eyes look with a fan and see the room; the sweep on a tick knows only what is straight ahead. The costs are of the same job only as far as "do not walk into the furniture" goes.

**Choices made here.** The viewer is not given to the eyes (no sight ray): the camera is over 6 m off and a run must repeat. The rays drawn (A) are screen-space quads 1.5 pixels wide, added; a segment under a pixel long is left out. In the sweep modes only the first debug segment is written by the sweep, as Cortana's note says.

**Not tested on hardware:** this program's whole path (it shares ao4's plumbing: polled render-done, the text texture, the page kit); 32 hens are 2,240 more triangles as strips of four.

---

# ao4-e: the clip guard's test and the tile list cut (built, not run, not on a disc yet)

Built 2026-11-25 by Senior. **Not run on a console. No disc holds it: `suite/suite_s7.json` is ready and s7 is cut when lead says.**

| Item | Value |
|---|---|
| ELF | `build/psm_ao/psm-ao4-e.elf`, sha256 1ad24f15830d666649b5713b1fc54c30ebc6a4783ffdbb9fc0a6471ae4e27924, no compiler warning |
| New source | `ao/tile_cut.h` (the cut, standalone), `ao/test/tile_cut_test.c` |
| The run that counts | 19 stages, about 108 s: the 15 of ao4-d, then stages 16 and 17 (the clip guard, 3 s each), then 18 and 19 (the tile cut, 6 s each), LAST |

**The clip guard (CORE 18).** `user_clip` sends a polygon with area, off the screen, before each change of user clip, because the change is said to take effect one polygon early. Until now nothing showed what happens without it.
- Stage 16 runs with the guard OFF, stage 17 with it ON, scene A, 1 | 2. In both a magenta square is the left half's last polygon (just left of the dividing line, near the top) and a cyan one the right half's. The label at the bottom right says CLIP GUARD ON or OFF.
- **How to read the photographs:** magenta missing while the label says OFF, and there while it says ON: the early change is seen on this console and the guard works. Magenta there both times: the change is not early here (or the guard is not needed for this order of polygons). Cyan is the control: it must be there both times.
- Live: hold d-pad left and press Y: the guard off / on, with the marks. Every other stage runs with the guard on; the page says which stage ran in which state, and names any stage that did not run as planned.

**The tile list cut** (`../TILE_CUT_SCOPE.md`). Stages 18 and 19 are 3+M | 3+M with the hens standing at their start: target M's pass through KallistiOS's whole tile table (300 tiles), then through a table cut to its own 4 × 5 tiles for the length of that pass.
- Guards: the two tables are found by reading the PVR's region base after a render and are taken only if they read as KallistiOS lays them out; the layout is checked again before every cut; a refused cut writes nothing and the pass runs uncut (the page counts refusals); the rewrite is done only with no render running (every pass here waits for its render); after the pass every word is written back and read back, three tries; a word that does not restore stops the PVR with a line on the page (its next start builds the tables anew). Bounded waits and the fault catcher as everywhere.
- The page: tables found; the PVR's time of M's pass uncut and cut; a sum of target M's texels after each and THE SAME or DIFFERENT; passes cut and refused; the rewrite and restore in us a pass; words not restored.
- ESTIMATE: about 3.2 ms uncut, 0.3 to 0.4 ms cut (PVR-092's 3,312 to 375 us was the same 20 tiles, once, as c4's last stage). 120 words a table are written and as many restored.
- The cut is OFF everywhere but stage 19, and there is no live switch for it. **If the s6b run shows a hang in 3+M, stage 19 stays out until that is understood** (build with the last stage removed).
- KallistiOS's lines in `tile_cut.h` and the scope note are from commit 568c3404; they want a new check against the current master before anything is offered upstream.
- `tile_cut.h` is standalone (plain C but one register read under `_arch_dreamcast`) with a host test of the table logic: a walk of the cut table, as the ISP reads it, visits exactly the target's tiles with their own list pointers; the end restores every word; what must be refused is, with nothing written. Its status until the page is photographed: **ran only in a spike, not console-proven.**

**Not tested on hardware (new in e):** the per-pass rewrite of the tile tables; the user clip changed with no polygon before it (that is the test).

# pd-a: the PVR-DMA download proof (built, not run, not on a disc yet)

| Item | Value |
|---|---|
| ELF | `build/psm_ao/psm-pd-a.elf`, sha256 98c7ff38922fef693fa6c8578b5be5c03ff8317d92c3299d58208ceb5391b711, no compiler warning |
| Source | `ao/pd_spike.c` (it lives with ao's build script; `build.py --spike pd`) |

One page for outsiders and a bar graph: what it proves in a line; a 524,288-byte pattern written to video memory, copied back by PVR-DMA through the 04 and the 05 area and compared byte by byte ("N of 524,288 wrong"); the register sequence with this run's own values (SB_PDSTAP, SB_PDSTAR, SB_PDLEN, SB_PDDIR = 1, SB_PDEN, SB_PDST, and the end); a table of us and MB/s for 32, 128 and 512 KB: PVR-DMA polled (04, 05), PVR-DMA by interrupt, `memcpy` from A4 and from A5 (uncached), and a cached RAM-to-RAM copy; the Holly revision; the page's CRC. Best of 3, cycle counter. About 2 s; nothing is drawn.
- The 05 copy is compared with the CPU's own read through A5, because the two areas show the same memory in a different order.
- The cached copy is a loop of SH4ZAM's inline `shz_memcpy32_1`, not `shz_memcpy32`: that one hands a large copy to a routine in SH4ZAM's library, which no build here links (no libsh4zam.a in the checkout or in this KallistiOS).
- The sequence is c4's test 7 and bench_h1's, which ran on one console; this program itself has not run.

---

# s6b's first photographs (ao4-d live view): the dashed lines

Photographs `logs/benchmarks/suite_run6b`, PXL_20261125_173806884 to 173847168 (8, the live view only; one console, 2026-11-25). Read by Senior for the two artefacts lead named. The costs on these photographs are lead's to file; the user clip split ran with no hang.

**Scene B, method 2: dark dashes along the wall's foot** (photo 173834648, right half). Cause, by reading (ESTIMATE until the test below): the ground under the wall is the darkest place of the map (every sky direction's shadow covers the wall's own footprint: four darkenings, where the floor just in front has about two), and the map's bilinear filter carries that darkness half a texel (about 2.3 cm) past the wall's face. That fringe is under a pixel wide on the screen, so it shows only on the scanlines where a pixel's centre lands in it: one dash at each step of the slanted edge, which is the pattern in the photograph. Not a T-junction (the wall and the ground share no edge), not the clip (it is in the middle of the right half).
- Test that would prove it: the ground's map with point sampling (the dashes go), or EXAGGERATE (they get darker with the footprint).
- Fix proposed, not built: draw each caster's footprint (the caster with no slide) into target A first, with the darkening of K / 2 directions and depth write, at a depth the directions' triangles fail against; the footprint then holds about what the floor at its foot holds and nothing darker is there to leak. One more pass of the casters a frame (the cost of one direction).

**Scene C: light dashes along the wall panels' rails, with AO off, baked and lightmap alike** (photos 173841635, 173847168). Cause: the trim's own geometry is under two pixels on the screen. The rails stand 2 to 5 cm proud of the wall, so their top faces (lit by the sun, brighter than the wall) are 1.1 to 2.9 cm deep as seen from the eye's 35 degrees, and a pixel on the far wall is about 1.8 cm. A face about a pixel wide, point sampled, is drawn on some scanlines and not on others: dashes. The host's software preview of the same scene shows the same dashes (`ao/preview/ao4_c_*.png`), and it has no tile clip, no T-junction problem and exact depth: so it is the model against the pixel grid, not the PVR. Not the 1 cm overlap either: the trim's back is 1 cm behind the wall and never coplanar with it.
- Ways out, none taken: (1) leave it and say so (the PVR has no edge anti-aliasing here); (2) light the trim with the wall's normal in the importer, so a rail's top is the wall's shade and the moulding reads by its AO alone (a few lines, mine); (3) rails at least 7 cm deep in the model (Cortana); (4) the eye nearer.

**Decided by lead, 2026-11-25.** The wall-foot dashes of method 2: no fix now (the baked AO wins on look and cost on the console; method 2 is likely for movers only); the estimate and its proof test above stand, and the town's method 2 has the same leak at every house's foot. The trim's dashes: way (2), done: `room_import.py` lights trim with its wall's normal (384 vertices of `room_trim`; places, triangles and counts unchanged: 3,323 vertices, 3,536 triangles, a half still 6,680 and 4,268 vertices). With AO off the mouldings now have the wall's shade and are not seen; with AO they show by their baked crease (`ao/preview/ao4_c_off_baked_trimflat.png`, host). Rebuilt on that room data, not run, on no disc: **ao4-e2** (`build/psm_ao/psm-ao4-e2.elf`, sha256 e002381dfe8a5e750b48fda11a03c77a41f95aef4f0db8ff9ffc6f94eceb8a08) and **hen-b** (`build/psm_ao/psm-hen-b.elf`, sha256 bc71fa68be46fda0360aed4578632ba8fa7fdeb9b45c96a368cafc4da71ff3c2); they replace ao4-e and hen-a in `suite/suite_s7.json`. Host tests pass (ao4_scene_test, tile_cut_test, hen_room_test, ao_layer_test).

---

# The "tile bins out of memory" count: a host estimate, and what the counts themselves say

Written 2026-11-25 by Senior for PVR-143 (ao4-d: 4,948 events), with PVR-121 (the town) and PVR-132 (ao0). **Nothing new was run on a console.**

**1. The host estimate (ESTIMATE: a model of the TA's binning, `ao/test/ao4_bins_test.c`).** Every pass's polygons are put into 32-pixel tiles as ao4 sends them (a strip in pieces of six triangles, a piece one entry in each tile a triangle's box touches, the user clip keeping a half to its ten columns) and the overflow blocks a tile's list needs are summed against the pool KallistiOS was given (16-word bins for two lists, `opb_overflow_count` 4: 2,400 blocks a scene, read from `pvr_buffers.c` and `pvr_misc.c`).

| Pass | Most entries in a tile | Overflow blocks of 2,400 |
|---|---|---|
| Screen, scene A / B, any split | 55 / 57 | 58 / 46 |
| Screen, scene C with the hens | 214 (vertex AO), 159 (lightmap) | 288 to 339 |
| Target S | 129 (A), 139 (B), 3 (C) | 46, 29, 0 |
| Target A, K 4 | 620 (A), 574 (B), 379 (C) | 248, 157, 169 |
| Target A, K 8 | 1,242 (A) | 462 |
| Target M | 48 | 12 |

By the model **no pass of ao4 comes near the pool**: the worst is under a fifth of it.

**2. What the counts say (DERIVED from measured rows; the cause an ESTIMATE).** The events are close to **one for every second scene, whatever is drawn**:
- ao4-d: scenes in the run, from the page's frame counts (A and B exact, C from its frame times): about 10,020; half is 5,010; counted 4,948.
- ao0: 93 map passes; half is 46.5; counted 46 (PVR-132).
- The town: route A, two scenes a frame, an event on every frame; route B, one scene a frame, an event on exactly every second frame (PVR-121).
KallistiOS has two TA buffers and uses them in turn. So the event comes with one of the two buffers, not with a full bin. A cause that fits: when the TA's registers are moved to the other buffer (`pvr_sync_reg_buffer`), the limit of the low buffer is written while the TA's own position still holds the high buffer's, so the "past the limit" interrupt fires once at each switch from the high buffer to the low one. Not proven.
- **If that is right, PVR-121, PVR-132 and PVR-143 are not overflows**, the pictures were whole, and the town's garbled HUD line (PVR-122) needs another cause (the parameter overflow of the same run is real: it has its own counter).
- **The test that proves it** (for the tests-off ao4 after s7, a line on the page): the events counted against the scenes begun, and the TA's own position and limit registers read inside the handler at the first few events (the position above the limit, in the other buffer's range, with nothing sent yet).

---

# Effect labels, the movable divider, ao4-h, hen-d, pd-c (built, not run; on s7)

Written 2026-11-25 by Senior. **Nothing here has run on a console.**

**Effect labels** (`../psm_effects.h`, host test `../test/psm_effects_test.c`; the tables of these programs are in `ao_fx.h`). A line says what a view shows, who does the work each frame and when: `Name (detail): WHO When`, WHO one of SH4, PVR, SH4+PVR; When one of Baked, Texture, Realtime (written `Realtime<<`, in yellow). Rules, as lead ruled them:
- WHO is who works each frame. The PVR counts only for work the effect itself needs (a render target pass, a texture that carries the effect, an alpha test), not for drawing the triangles.
- "Shadow Mapping" only where a depth compare is done; otherwise "Projected Shadow". "Bloom" only for `sh4der_glow.h`; a glow mesh is "Soft Halo".
- The lines are OFF in the measured autorun stages (a blended line is fill the stage would count) and ON in the live view and on the result pages. The page's label rows are outside the page CRC.
- Each program names its own hide chord: ao4 and ao-lamp d-pad up + B, hens-eyes d-pad left + B, hero1 d-pad right, d1 d-pad up + Y.
- A page with the labels takes one or two rows under the title: `PG_TOP` in `../psm_page.h` (0 by default, so older programs are unchanged).

**The movable divider** (`../psm_split.h`, host test `../test/psm_split_test.c`). The line between the two halves moves by tile columns (32 pixels), 0 .. 20, with LT / RT (a press, then a step every 6 frames). A half the divider hides is not lit and not sent. The halves are user clip rectangles, as the fixed split's were.

**ao4-h** (`build/psm_ao/psm-ao4-h.elf`, sha256 a77eb2ede031e373b3b982eae0d3e88e5103a12cb641f7ae51109a86065758de): ao4-e2 with the labels and, in the live view only, the divider. Keys changed: LT / RT move the divider; BOTH triggers together are the next scene (was: either trigger). The measured stages keep the divider at the middle, tiles 0 .. 9 and 10 .. 19 as before. OPEN on a console: a user clip that moves between frames (the fixed one ran on s6b).

**hen-d** (sha256 6ea63a33b790f0f0f3f2defca502ada6ed4abd39167ee73f6201b8c3bd243744) and **pd-c** (sha256 a902ad6f821f2c17aacef8962c0af5cc94d6c7c9964c17a3b50222d2dc048575): hen-b and pd-a with the labels, no other change. hens-eyes names its modes "Ray Casting (steering)" and "Collision Detection (swept)" (Cortana's correction: the entry passes no viewer, so "line of sight" was untrue); its page says the push and the separation run in both modes. pvr-dma's page says "No effect: a hardware transfer test".

---

# ao-lamp, entry lamp-c: the lamp look (built, not run; on s7)

Written 2026-11-25 by Senior. **Not run on a console.** Program `lamp_spike.c`, the look's maths `ao4_lamp.h`, host test `test/ao4_lamp_test.c` (it also renders the previews `preview/lamp_sheet_{room,front,side,below}.png` and `lamp_crop_slots.png` with a software renderer: host pictures, not the PVR's). ELF `build/psm_ao/psm-lamp-c.elf`, sha256 c41122d85e351ad6f498a2a742cffc9a97ef81ff423410c16b39a26400179c87.

**What it draws.** The room's own lamp, found in the room data by name: the shade is rebuilt as a lathe of 65 vertices and 96 triangles and lit as paper with the bulb inside it.
- The shade: `sh4der_sss_rgb` per vertex (absorption 0.2 / 0.3 / 0.5, wrap 0.4, distortion 0.1, a warm tint), plus an emission term, into the vertex's base and offset colours. Label: "Emission" is the lampshade (the owner's wording), "Subsurface Scattering" the paper.
- Slots: a 32 x 32 punch-through pattern (16 % cut) on the shade, through the punch-through list.
- The halo: `sh4der_bloom_rings`, 8 rings of 16, 129 vertices and 240 triangles, at the bulb (0 m toward the eye: moving it toward the eye flattened the paper's look on the host pictures).
- ROOM LIGHT: the room's vertices lit by the bulb as a point light (its own toggle), over the room dimmed to 0.38.
- A near camera (the room's eye, a long lens aimed at the bulb) on A.

**Stages** (4 s each, labels off): 0 lamp off on a PVR started without the punch-through bin; 1 the same with it (the cost of an opened third list); 2 OFF | ON; 3 slots; 4 room light; 5 OFF | ON near; 6 ON alone; 7 OFF alone. The punch-through list, where the PVR has its bin, is opened every frame and never left empty (an off-screen triangle first).

**OPEN on a console:** every cost on its page; the look of the sss through the PVR's vertex interpolation at 12 segments; whether the halo's aura reads round the shade's outline (on the host it is hidden behind the near paper and shows as a wash through the slots and the top); the plane fade (`ao4_lamp_fade`), which this room rarely shows.

---

# d1, entry d1-a: a halo that fades at the scene (built, not run; on s7)

Written 2026-11-25 by Senior. **Not run on a console. The design is an ESTIMATE until T1 and T3 have run.** Program `d1_spike.c`, the arithmetic `d1_model.h`, host test `test/d1_model_test.c`. ELF `build/psm_ao/psm-d1-a.elf`, sha256 046e692e82ad851856d2f75867a0bcb7343ed34cd5d3d260d2a0fce64460907a. It is in this folder, not in `compare/`, because this folder's build makes the room's data.

**The design.** The room is drawn once more from the camera into a 320 x 240 target through a 256-texel ramp that repeats: vertex z = 1 / depth and u = depth / 1.2 m, so the PVR's perspective-correct lookup gives each pixel a code linear in its own depth, 256 codes a turn of 1.2 m. That target is read as a BUMP texture; the bump unit gives K1 + K3 cos(2 pi (M - Q) / 256) for the map's code M and the polygon's Q. With K1 0, K3 255 and Q = the halo's code + 64 the level is 0 where the scene is at the halo's depth and full 0.3 m behind it. Four quads in the translucent list, sent in order (autosort off):
1. CLEAR: colour (1, 1, 1, 0), ZERO x DESTCOLOR: destination alpha to 0, colour kept.
2. FADE: the bump quad on a black base, ONE x ONE, depth test on: (0, 0, 0, level) added.
3. FAR: (0, 0, 0, 255) from a quad 0.3 m behind the halo, depth test on: everything further is full.
4. HALO: the glow, DESTALPHA x ONE.

| Claim | Status |
|---|---|
| Destination alpha is kept during a render; CLEAR by ZERO x DESTCOLOR; an alpha-only ONE x ONE add; DESTALPHA as a source factor; autosort must be off | MEASURED by c1 (wiki PVR-040; `compare/cmp_spike.c`) |
| The bump level as a compare, a cosine of M - Q | MEASURED by c4 (wiki PVR-045) as a colour; as an alpha-only add on a black base: OPEN, T1 |
| Which texture environment carries the level into alpha (MODULATE or MODULATEALPHA) | OPEN, T1 bands 0 and 2 |
| K1 0 / K3 255: a negative level reads as 0 | OPEN, T1 |
| The repeating ramp gives a code linear in depth | ESTIMATE (the PVR's interpolation rule); T2 |
| The level alone is not a fade (it comes round every turn); the FAR quad is needed | ESTIMATE, a design claim; T3 stage 4 shows it or refutes it |
| A half-size map looked up in screen space is good enough at a silhouette | OPEN, T3 (a fringe of a map pixel is expected) |

**The run** (about 45 s): T1 five bands read back (PVR-DMA, checked against direct reads at first use), a picture held 5 s; T2a a flat and a tilted quad, T2b the room's codes at the floor's vertices, a picture each; T3 five stages of 5 s (no glow; plain | plain; plain | alpha as grey; plain | fade; plain | fade without FAR), costs and a graph. T3 takes its FADE from whichever of T1's bands 0 and 2 read as the model says; with neither it draws the plain glow and its page says so. Each test has its own page.

**What to photograph:** the three held pictures, each T3 stage, every page and the graph. PASS for the look: right of the divider the glow's edge at the wall is soft and the rest of the glow is as on the left.
