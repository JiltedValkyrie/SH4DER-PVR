# demo/psm, the town: results in ledger form

The town demo (`psm_dc.c`, `psm_scene.h`, `psm_town.h`, `psm_sky.h`, `psm_send.h`), images `sh4der-psm-town-a*` in `demo/psm/out/cdi`.
Format and status tags: `wiki/STRUCTURE.md` ("Ledger entry", "Status legend"). One row a fact. Console numbers and host or
estimated numbers never share a row.

**Where the console facts come from.**

- **Boot logs**: the owner's launcher writes each image's console output to `dc-masseffect/logs` (read only for us):
  `20261124-sh4der-psm-ab-b3-2037.txt`, `20261124-sh4der-psm-town-a3-b512-2149.txt`, `20261125-sh4der-psm-town-a3e-0151.txt`.
- **a3 HUD**: one photo of the HUD, taken by the owner on 2026-11-24 and relayed by Lead. I was not given the photo's file name.
- **Owner reports**: what the owner saw on the console, relayed by Lead, with no photo or log name.

Nothing here was repeated, so no number has a spread. One console so far.

## Console

| ID | Claim | Status | Evidence | Source | Test | Used by |
|---|---|---|---|---|---|---|
| TOWN-001 | Town a3, boot-like view (cam x -1.7 z -11.8, hdg 337, pitch -10, 14:00, DIST INF, route A, MAP 256 UNIFORM, CASTERS ALL, FILTER A BILIN B 2x2): SH-4 22.6 ms, PVR 16.0 ms, 28,699 TA vertices, map triangles 600 of 1,162 (14 objects culled), 25 fps. | MEASURED | 2026-11-24, one console, one HUD photo (name not given to me) | Lead's relay, 2026-11-24 | `sh4der-psm-town-a3.cdi`, HUD cost and count lines | TOWN-007; `OFFLOAD_PLAN.md` item 1 |
| TOWN-002 | Town a3-b512 and a3e: video memory free 4,554 KB after `pvr_init` and 4,010 KB after load, with vertex buffers 2 x 1,024 KB. | MEASURED | 2026-11-24 21:49 and 2026-11-25 01:51, one console, boot logs (the two match) | `20261124-sh4der-psm-town-a3-b512-2149.txt`, `20261125-sh4der-psm-town-a3e-0151.txt` | the boot status line, `psm_dc.c` | 512 map size, sky texture, route A target |
| TOWN-003 | The field scene (b3) at boot: video memory free 4,266 KB. | MEASURED | 2026-11-24 20:37, one console, boot log | `20261124-sh4der-psm-ab-b3-2037.txt` | the boot status line | — |
| TOWN-004 | Town a3 crashed once with no message while the camera turned away from the TOWN-001 view. | MEASURED (seen once) | 2026-11-24, one console, owner's report via Lead (no photo of the crash) | Lead's relay | turn from the TOWN-001 view on a3b or later, with the vertex high-water % and `psm_fault` on screen | TOWN-009 |
| TOWN-005 | A later image ran through every setting with no crash. | REPORTED | owner via Lead; the image was not named | Lead's relay | the same run, with the image named | — |
| TOWN-006 | The shadow map swam as the camera moved: map texels flickered on and off surfaces, plainly at 64 x 64. | MEASURED (seen) | 2026-11-24, one console, owner's report via Lead (no photo) | Lead's relay | walk at MAP 64 on a3c or later and watch the shadow edges | TOWN-011 |

## Built facts (true of the image, not of the chip)

| ID | Claim | Status | Evidence | Source | Test | Used by |
|---|---|---|---|---|---|---|
| TOWN-020 | The a3e town holds 9,737 model vertices, 5,840 model triangles and 1,162 caster triangles in 39 objects, with 6,305 ground vertices in the rings. Its textures take 545 KB (text 256, sky 256, inset 32, ground 1), and route A's target 128 KB at MAP 256. | FACT | the program's own count, printed in the a3e boot log | `20261125-sh4der-psm-town-a3e-0151.txt` | — | budgets |
| TOWN-021 | In the a3e ELF, `frame.mscr` starts exactly 147,456 bytes (9 x 16 KB) after `frame.mclip`, and `scene.mn` exactly 147,456 bytes after `scene.mv`. So `mclip[i]` and `mscr[i]` share one operand-cache entry, and so do `mv[i]` and `mn[i]` (the placing trap: SH4-021 in the wiki). The ground arrays do not: `gpos`, `gclip` and `gscr` sit 944 to 2,320 bytes from a shared entry. | FACT | `sh-elf-nm` of `sh4der-psm-ab.elf` (a3e build) and `offsetof` taken with `sh-elf-gcc` (struct sizes match the ELF: 802,044 and 460,068 bytes) | `psm_scene.h` `psm_frame_t`, `psm_scene_t`; the cause is `PSM_MV_MAX` 12,288 x 12 bytes | — | TOWN-030 |
| TOWN-022 | `psm_place_geo` writes `mclip[i]` and `mscr[i]` in one call per model vertex (`psm_scene.h:1334`, `:1357`). Route B reads `mv[i]` and `mn[i]` together (`psm_scene.h:1207`). | FACT | the source | `psm_scene.h` | — | TOWN-030 |

## Host and estimates

| ID | Claim | Status | Evidence | Source | Test | Used by |
|---|---|---|---|---|---|---|
| TOWN-007 | The a3 frame costs about 157.5 SH-4 cycles for each TA vertex. | ESTIMATE | arithmetic: TOWN-001's 22.6 ms x 200,000 cycles a ms / 28,699 vertices. It counts all of the SH-4's frame time as vertex work. | `OFFLOAD_PLAN.md` | — | `OFFLOAD_PLAN.md` |
| TOWN-008 | The host sweep of a3e (1,080 views) sends at most 27,885 vertices and 773 KB of TA data, with 0 faults. | ESTIMATE | host model, `test/psm_town_test.c` | `make test` log | the console count at the same views (TOWN-009) | vertex buffer size |
| TOWN-010 | At 14:00 the sun is at azimuth 213.77 and elevation 26.54 on both host paths (worst against the story's table: 0.46 degrees azimuth, 0.05 elevation). | ESTIMATE | host run, plain C and SH4ZAM software back end | `SHZ_TRIG_PROBE.md` | the a3e readout `SUN 214/27` (TOWN-012) | — |
| TOWN-011 | With MAP HOLD on (a3c), shadow-edge flips a frame under sub-texel camera steps fall from 226 to 240 to 0.0 to 1.5 on the uniform map (both routes, 64 to 256), and from 100 to 146 to 23 to 46 on the PSM, which still pops at each refit. | ESTIMATE | host count, `test/psm_town_test.c` swim test, a3c report | a3c report to Lead | TOWN-006 on a3c or later | — |
| TOWN-030 | Padding `psm_frame_t` so `mscr` (and `mn`) sit 8 KB from a shared entry could save about 1.1 ms on frames that re-place the models. | ESTIMATE | model: the measured clash cost of `mat_transform` (SH4-021's 38.67 against SH4-020's 13.16 cycles a vertex) applied to 8,700 model vertices. `psm_place_pt_`'s pattern (two clashing writes) is not the one bench h2 measured. | this file | an A/B image with and without the pad, SH-4 ms on the HUD at the TOWN-001 view | — |

## Open

| ID | Claim | Status | Evidence | Source | Test | Used by |
|---|---|---|---|---|---|---|
| TOWN-009 | Why a3 crashed (TOWN-004): vertex buffer overflow, tile bin overflow, or a fog register write during a render. | OPEN | — | a3b report to Lead | a3b or later: the HUD's vertex high-water % and the `psm_fault` catcher, on the same turn | — |
| TOWN-012 | a3e on the console: the TA SEND row's SH-4 ms both ways (store-queue runs against one `pvr_dr` write a vertex), the people and hens drawn right with the row-form mover matrix, `SUN 214/27` at 14:00, the sky and fog as before. | OPEN | the a3e boot log exists (TOWN-002); no HUD photo or report yet | `OFFLOAD_PLAN.md` bench B0 | `sh4der-psm-town-a3e.cdi`, the TA SEND row in the VIEW group, the HUD cost line | `OFFLOAD_PLAN.md` item 1 |
| TOWN-013 | `PVR_FOG_VERTEX` on the textured sky header works on the console. | OPEN | — | a2 report | look at the sky at DIST 30 on a2 or later | sky, fog |

## Data for the ray bench (THESIS_SH4_SHADOWS.md P1, P2, P9)

For Senior's ray bench. The data is `demo/psm/town_ray_data.h`, generated from the town file by `demo/psm/tools/town_ray_data.c` (its header says how to rebuild it). Nothing in the town code changed. World coordinates: x east, y up, z = minus the story's north.

| ID | Claim | Status | Evidence | Source | Test | Used by |
|---|---|---|---|---|---|---|
| TOWN-040 | The grid: 64 x 64 cells of 1.5 m (4,096 B, 1 B a cell), corner x -58.5, z -39.0, covering 96 x 96 m. The static town's casters lie in x -38.3 .. 18.0, z -18.0 .. 35.0, so the whole town fits with a margin. | FACT (the layout is the thesis's 5.3, placed on the town's box) | the tool's output | `town_ray_data.h` `town_ray_h` | — | P1, P2 |
| TOWN-041 | A cell holds the tallest static caster surface over it, in steps of 28.0 / 255 m = 11.0 cm. The tallest point is the tower's roof at 28.0 m, not the thesis's 13 m. 506 cells have a caster over them, sampled every 0.2 m on the 842 static caster triangles (of 1,162: the rest are the 12 movers'). | FACT | the tool's output | `TOWN_RAY_H`, `TOWN_RAY_OCCUPIED`, `TOWN_RAY_STATIC_CASTER_TRIS` | — | P1 |
| TOWN-042 | 4 x 4 block maxima, 16 x 16 bytes, for the two-level walk the thesis proposes for the tall tower. | FACT (from the grid) | — | `town_ray_hb` | — | P1 |
| TOWN-043 | 83 cells are overhangs: eaves, the arch's passage, canopies. Over them sit 194 (cell, triangle) entries, 95 distinct static caster triangles, 3,420 B as one list at 36 B a triangle. The rule: the lowest caster surface over the cell is above 1.0 m, and the cell's centre at 0.5 m is outside every closed caster shell (parity of a ray toward +x). | ESTIMATE (the tool's heuristic; open shells or a wall on a cell edge can misjudge a cell) | the tool's output | `town_ray_overhang` bitmap, `TOWN_RAY_OVERHANG_*` | look at the bitmap against the town's pictures | P1 (the proxy list) |
| TOWN-044 | Sun: 14:00 az 213.77, el 26.54; 16:00 az 242.11, el 12.92. | FACT (`psm_sun_at`) | — | `TOWN_RAY_SUN14_*`, `TOWN_RAY_SUN16_*` | — | P1, P9 |
| TOWN-045 | The movers: 8 people with a caster box 0.46 x 1.70 x 0.30 m, and 4 hens with 0.22 x 0.27 x 0.41 m (0.10 to 0.37 m up). Shadow run on flat ground: person 3.40 m at 14:00 and 7.41 m at 16:00; hen 0.74 m and 1.61 m. Places at load (cast time 0), the people's route boxes and the hens' pens are in the table. | FACT (sizes, places, boxes); DERIVED (runs = height / tan el) | the tool's output | `town_ray_movers` | — | P9 |
| TOWN-046 | Where the movers are AT 16:00 is not defined: they walk on the cast's own clock (seconds), not the sun's. For P9 use the 16:00 runs with the load places, or with places anywhere in each route box, and say which. | FACT (`psm_town_update`) | — | `psm_town.h` | — | P9 |
| TOWN-047 | Vertices a frame at the a3 view (hdg 337, pitch -10, route A uniform), host send walk of a3e: 27,554 in all, of which ground 8,656, route A's layer 8,656, models 10,014, sky 144, text 84; 2,721 strips. The console sent 28,699 at the same view on a3 (TOWN-001, MEASURED: an older image). | ESTIMATE (host model) | `test/psm_town_test.c` output, `D:\tmp\junior\shzl_test.log` | — | the a3e HUD at that view | P2 (k), 5.7 |
| TOWN-048 | Receivers the slice would serve: 8,697 static model vertices in the town (8,815 of 9,737 model vertices are sent at the boot view, the movers' included). The ground (6,305 vertices in the rings, re-centred as the eye moves) is not cached per vertex: the thesis samples a world map for it (5.3). | FACT (counts); ESTIMATE (the boot-view share, host) | town boot log; host test | TOWN-020; `psm_town_test.c` | — | P2 |

## c4 with a static sun: band counts (the owner's idea, host only)

The question: if the sun stands still, the c4 chain's receiver bands can be cut once at load. How many bands would the town need, and what would the mask pass and the per-frame project and send cost? The counter is a host program (in the session scratchpad, not in the tree) that runs c4's own rules (`compare/c4_model.h`: AWAY at n.s <= 0.02, STEEP when the bias passes w - 4, bias b0 3.0 + half a band + a texel of slope, window 32) on the town's static triangles plus a ground quad. Sun at 14:00. No town code changed. Every cost below is an ESTIMATE; the only measured numbers are c4's, quoted for scale.

| ID | Claim | Status | Evidence | Source | Test | Used by |
|---|---|---|---|---|---|---|
| TOWN-050 | A fit round the whole static town along the 14:00 sun spans 73.5 m of depth: 0.288 m a code. At map 256 a texel is 0.298 x 0.174 m. Fitting to the square alone barely helps (67 m: the tower and the far roofs still set the depth). | ESTIMATE (host) | the counter's output | this file | — | TOWN-051 |
| TOWN-051 | Of 5,320 static model triangles, 2,232 face away, 5 are steep (21 at map 128), 3,083 compare and are cut into bands. Whole town at map 256: 1-code bands 11,915 (44,577 vertices), 2-code 7,513 (26,969), 4-code 5,295 (18,097). c4's small scene has 2,464 / 1,288 / 692 band vertices (MEASURED, c4 test 5): the town is 18 to 26 times that. The ground quad alone is 940 / 472 / 236 of it. | ESTIMATE (host) | the counter's output | c4 test 5 (`compare/NOTES.md`) | — | TOWN-053 |
| TOWN-052 | At the boot view (x -1.7, z 11.8, heading 337, pitch -10, tan_half 0.577, aspect 4/3, far 70), 2,152 static triangles have a corner in view; their bands: 11,146 / 7,090 / 5,174 vertices at 1 / 2 / 4 codes (models only; add up to the ground's 472 at 2 codes). Culling to the view cuts about 3.8 times, not enough. | ESTIMATE (host) | the counter's output | — | — | TOWN-053 |
| TOWN-053 | Mask pass for the visible static bands, 2-code: 7,090 vertices x 2.4 us = about 17 ms (Lead's per-vertex figure), or x 4.7 us = about 33 ms (c4's whole-pass ratio, 6.1 ms / 1,288 and 10.2 ms / 2,192, MEASURED, fixed costs included). 4-code: 12 to 24 ms. Either way over a 16.7 ms frame on its own. Project and send of the band vertices on top: the transform is 13.16 cycles a vertex clean (SH4-020, MEASURED), so about 0.5 ms for 7,090; the send cost per vertex is OPEN (TOWN-012). | ESTIMATE | arithmetic on the rows named | c4 test 5; SH4-020 | an image with the static bands pre-cut, SH-4 ms on the HUD at the boot view | TOWN-055 |
| TOWN-054 | Movers only: the receivers are 12 ground patches under the movers' shadows (from 0.5 m behind the feet to the run's end, 1 m wide for a person, 0.5 m for a hen), fit to the square only (x, z +-16 m, y 0 .. 2 m), map 128: 0.159 m a code at 14:00, 0.167 m at 16:00. Bands: 14:00 216 / 115 / 65 (864 / 460 / 260 vertices); 16:00 429 / 220 / 114 (1,716 / 880 / 456). The mask pass would be about 1.1 to 2.2 ms at 14:00, 2-code (2.4 or 4.7 us a vertex), 2.1 to 4.1 ms at 16:00. The patches move with the movers, so these bands are cut each frame, not at load: the cut's own cost is OPEN. | ESTIMATE (host) | the counter's output | — | the same image with only mover patches as receivers | TOWN-055 |
| TOWN-055 | Recommendation: do not run c4 on the static town. Static on static goes to a bake (vertex colours for the models, a lightmap or vertex colours for the ground; item 2 of the overnight list). Run c4 only for the movers' shadows on the ground patches round them (TOWN-054), at 2-code bands. Also left to the bake: every AWAY face (it is dark by n.s alone), the 5 to 21 STEEP faces, and the tower and far roofs (their depth is what stretches the fit). Note the 3-code bias at the town fit is about 0.9 m, which would lose a hen's shadow whole (0.37 m tall); the square-only fit (0.16 m a code, bias about 0.5 m) keeps a person's and is marginal for a hen. | ESTIMATE (inference from TOWN-050 .. 054) | — | — | the image of TOWN-054 at 14:00 and 16:00, a photo of a hen's shadow | item 2 |

## Static on static: the bake (design, host prototype)

What follows from TOWN-055: the sun does not move, so the static town's shadow on itself is the same every frame and is computed once. The design, then the numbers from the prototype `demo/psm/tools/town_bake.c` (a host tool; its header says how to build it). No town code changed.

The design:

1. **Models: one byte a static vertex.** At load (or at PC build time, into the mesh header), each of the 8,697 static model vertices gets `bake[i]` = N.L x (1 if a ray from the vertex toward the sun reaches the sky past the static occluders, else 0), 0 .. 255. A frame's `mlit[i]` for a static vertex becomes `bake[i]` (route A) or `bake[i]` x the movers' shadow at that vertex (route B, movers only). The per-frame N.L and the static lookups go away. 8,697 B of RAM.
2. **Ground: a world lightmap, read per ground vertex.** The ground is a polar grid that re-centres on the eye (TOWN-048), so a per-vertex bake would not stay with it. Bake a map over the ray grid's 96 x 96 m square (`town_ray_data.h`'s corner) instead: 256 x 256 at 0.375 m a texel, 1 byte a texel, 64 KB of RAM (128 x 128 at 0.75 m: 16 KB). When the grid re-centres (a 0.25 m step), each ground vertex reads the map bilinearly into `glit`; route B then multiplies the movers' shadow in, as it does now. The near rings are 0.25 m deep, close to the texel, so per-vertex reading loses little near the eye. The far rings (2 m) smear the edges, but far ground is small on screen. A textured second ground pass (PAL8 in video memory, MODULATE) would give sharper edges. It costs a second ground layer of fill and 64 KB of video memory, so it is the fallback if the per-vertex edge looks soft on the console.
3. **Movers receive the static shadow** (a person walking into a building's shadow): a ray a vertex is too dear: the 12 movers have 1,040 vertices (8 x 60 + 4 x 140), about 16,600 tests a frame. Instead cast 3 rays a mover (feet, hip, head) through the same light-space bins and blend those 3 values over the body by height. That is 36 rays, about 600 tests a frame.
4. **Movers cast** onto the ground through c4 on the patches round them (TOWN-054), or through route B's map with only the movers as casters. Not onto the static models: a person's shadow on a wall is left out (OPEN whether it is missed).
5. **Where the bake runs.** At PC build time it costs nothing on the console and the sun is fixed for the cast. At load it lets the time of day change between scenes. Rebaking with the sun moving slowly (sliced over many frames) is item 4's question.

| ID | Claim | Status | Evidence | Source | Test | Used by |
|---|---|---|---|---|---|---|
| TOWN-060 | Static occluders binned on a 96 x 96 grid across the sun (1.35 to 1.39 m a bin): the fullest bin holds 82 caster triangles (14:00) or 103 (16:00). A model vertex's ray tests 18.0 triangles on average at 14:00 and 13.6 at 16:00; a ground texel's 1.0 to 1.2. | ESTIMATE (host) | `town_bake` output | `demo/psm/tools/town_bake.c` | — | TOWN-063 |
| TOWN-061 | Models, caster proxies as occluders, ray from 0.05 m out along the normal. 14:00: 3,783 face away, 2,783 lit, 2,131 shadowed (43.4 % of those facing the sun). 16:00: 3,989 away, 1,227 lit, 3,481 shadowed (73.9 %). | ESTIMATE (host) | `town_bake` output | — | the console picture against the host one | TOWN-064 |
| TOWN-062 | Ground 256 x 256 (0.375 m): 23.8 % shadowed at 14:00 and 30.8 % at 16:00 (128 x 128: 23.7 % and 30.7 %). The shadows fall north-east at 14:00. The tower's (28 m) runs about 56 m at 14:00 and off the map at 16:00. | ESTIMATE (host) | `town_lightmap_14_256.pgm`, `_16_256.pgm` (the session scratchpad; regenerate with the tool) | — | — | — |
| TOWN-063 | Bake cost on the SH-4, per sun: 168,965 ray-triangle tests with the proxies (models 88,328 + ground 256² 80,637), or 577,635 with the screen mesh. At about 50 cycles a test (not measured; Möller-Trumbore, one reciprocal), that is about 0.04 s or 0.14 s at load. | ESTIMATE | test counts from the host; cycles a test assumed | — | time the bake loop on the console (one `timer_us_gettime64` pair) | item 4 (sliced rebake) |
| TOWN-064 | Proxies against the screen mesh as occluders: 43.4 % against 41.6 % of the sun-facing vertices shadowed at 14:00, and 73.9 % against 70.3 % at 16:00. Ground: 23.8 % against 22.4 %. The proxies are good enough to bake with, at a third of the tests. | ESTIMATE (host) | `town_bake` with argument 5 = 1 | — | — | TOWN-063 |
| TOWN-065 | The answer moves with where the ray starts. From 0.3 m out along the normal plus 0.5 m toward the sun, 14:00 falls to 28.5 % (screen mesh) and 16:00 to 63.9 %. The moving vertices sit where walls meet roofs and under eaves, where one ray is a poor sample. A bake that averages a few points inside each vertex's triangles (or the 2 x 2 rule route B uses) is the fix; which offset looks right is OPEN. | ESTIMATE (host) | `town_bake` arguments 3 and 4 swept: 0.05 / 0.2 / (0.05 + 0.3) / (0.3 + 0.5) | — | the same view on the console with two offsets, photographed | — |
| TOWN-066 | Per frame after the bake: static models and ground cost nothing for the sun. Ground re-read on a re-centre: 6,305 bilinear reads (about 30 cycles each, not measured), about 1 ms only on frames that re-centre. Movers receiving: 36 rays, about 600 tests, about 0.15 ms (50 cycles a test); a ray a vertex would be about 16,600 tests, about 4 ms. Movers casting: TOWN-054's 1.1 to 4.1 ms. | ESTIMATE | arithmetic on TOWN-048, TOWN-054, TOWN-060 | — | the console HUD with the bake in | TOWN-055 |
| TOWN-067 | Memory: 8,697 B (models) + 64 KB (ground 256², RAM) or 16 KB (128²); 0 B of video memory unless the textured fallback is used (64 KB at 256² PAL8). | FACT (sizes from the counts) | — | — | — | — |
