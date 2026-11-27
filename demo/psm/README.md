# demo/psm: the shadow demos, spikes and benches

This folder holds the software shadow demo, the town, and the console test programs (spikes and benches) with their notes. Each has its own notes file; this page holds only the rules that every test program shares.

## Result pages: use `psm_page.h`

A console test prints its numbers on pages that the owner photographs off a television. Every new bench or spike uses `psm_page.h` for those pages. Do not write page code of your own.

**The rules the kit keeps for you**

- **Title-safe area.** Everything is drawn inside x 64 to 575 and y 48 to 431 of the 640 × 480 picture (64 pixels in from the sides, 48 from the top and bottom).
- **Text is 8 × 16.** A page is 64 columns × 22 lines under one header line. The glyphs are the BIOS font reduced by a 3-to-2 box filter. `#define PG_BIG 1` before the include gives the BIOS size back (42 × 14).
- **A CRC line on every page.** The header ends with `CRC xxxxxxxx`, the CRC-32 of the page's lines. `tools/page_crc.py page.txt <crc>` checks a page typed from a photo.
- **Graph pages come after the text pages.** A graph shows shape only. Every value the ledger needs is also on a text page.
- **Markers.** A line starts with `TEST` (a heading), `RESULT` (measured by this run) or `DERIVED` (worked out by the program from its own results). A reader enters only what the marker allows.
- **Long lines wrap at a space**, so a number is never split.

**Use**

```c
#include <kos.h>
#define PG_PREFIX "mybench"            /* the serial line's prefix */
#include "../psm_page.h"               /* after <kos.h> */

pg_say("TEST 1 WALK  rays of known n");                 /* lines, printf form */
pg_say("RESULT 1.1 n %u: %u.%02u cycles a ray", n, c / 100u, c % 100u);
pg_break();                                             /* the next line starts a page */

pg_plot_t *g = pg_plot("1 WALK cycles a ray against n", "n", "cycles", 0);   /* 0 lines, 1 bars */
pg_series(g, "MEASURED this run");
pg_point(g, (float)n, (float)cycles);

/* after the PVR is shut down and the mode is 640 x 480 RGB565: */
pg_init();
pg_draw(page, "RAY ray3", "START: group 2");            /* page 0 .. pg_pages() - 1 */
```

- A program that draws pages of its own after the kit's (a picture, a code) sets `pg_extra_pages` so the header's page total is right.
- Limits: 16 graphs, 4 series a graph, 64 points a series, 12 bar groups, a title of 64 characters, a series name of 27.
- Keep 3 to 5 key numbers of a graph as exact text on a text page.
- The kit draws with the SH-4 into the frame buffer. It uses no PVR state, so call it after `pvr_shutdown`.
- Host test: `test/page_test.c` (the library's; make test). Programs that use the kit: `compare/c4_spike.c`, `compare/r3_spike.c`, `qr/qr_spike.c`.

**Time intervals: use the cycle counter, or take the reload step out**

KallistiOS's `timer_ns_gettime64` steps 2,502,400 ns ahead once every reload of its timer (each 0.9975 s). An interval that spans a reload reads 2.5 ms too long. MEASURED 2026-11-25 on one console (image ray2, CLOCK.4: largest step 2,502,321 ns). For an interval, either read the cycle counter (`perf_cntr_count`), or correct the ns value as `c4_spike.c` does: `n - n / 1000000000 * 2502400`.

**Not known yet**

- Whether 8 × 16 text reads in a phone photo of a television is OPEN. `PG_BIG` is the fallback.
- Whether a page can also carry its numbers as a QR code is OPEN: `qr/NOTES.md` is the test.

## Options a stage can switch

**The colours' options** (`psm_scene.h`, `psm_frame_t.colour_opt`; 0 is every model vertex's colour worked out every frame, as it always was). The view cost stages carry them in their option bits (`psm_viewcost.h`: 16 and 32). Their cost is not measured.

| Macro | Value | What it is |
|---|---|---|
| `PSM_COLOUR_TABLE` | 1 | a model vertex's colour is read from a table (a row a palette entry, a column a sun share), made again only when the light or the palette changes: 64 KB of RAM in `psm_frame_t` |
| `PSM_COLOUR_SEEN` | 2 | only the vertices `psm_place_geo` marked as sent get a colour; call `psm_place_geo` before `psm_colour` |

**The tile cut's tables** (`include/sh4der_tilecut.h`, moved from `ao/tile_cut.h`). `sh4der_tilecut_t.need` is the number of tile tables a cut wants found before `sh4der_tilecut_begin` or `sh4der_tilecut_begin_cols` will write: `SH4DER_TILECUT_TABLES` (2) from `sh4der_tilecut_init`; a program that knows it has one table sets it to 1 after the init. A cut with one table of two cut cost the street demo 3.05 ms (PVR-200, MEASURED, one console). The refusal itself is host tested (`ao/test/tile_cut_test.c`) and

## Baked occlusion and the clean picture (`psm_dc.c`)

**The AO row** (`psm_ao.h`, an adapter over `include/sh4der_aomesh.h`, on by default). At load the SH-4 bakes ambient occlusion for the scene: a byte a static model vertex and a byte grid over the ground, from rays over each vertex's hemisphere against the static triangles and the ground. The byte multiplies the ambient light only: colour = albedo x (ambient x ao + sun x direct), in `psm_colour` (`psm_scene.h`). The movers (people, hens) stay open. The row turns it off and on and shows what the bake took at load; "not baked: all open" means there was no memory for it. Host test: `test/psm_ao_test.c` (the bake, the colours with and without `PSM_COLOUR_TABLE`, the row, the hide). On the town's mesh the host counts 190,884 rays and 1,594,130 ray-triangle tests (exact). The bake's time on a console is not measured.

| Macro | Value | What it is |
|---|---|---|
| `SH4DER_AOMESH_RAYS` | 12 | golden-ratio rays a vertex, cosine weighted |
| `SH4DER_AOMESH_REACH` | 1.6 m | how far a ray looks for an occluder; a hit at t metres closes the ray by 1 - t / reach |
| `SH4DER_AOMESH_PUSH` | 0.03 m | a ray starts this far off its surface |
| `SH4DER_AOMESH_CELL` | 2.0 m | the triangles' grid for the bake (not under the reach, so a ray touches 4 cells at most) |
| `SH4DER_AOMESH_GCELL` | 0.5 m | the ground's grid of bytes |
| `SH4DER_AOMESH_MARGIN` | 8.0 m | the ground's grid goes this far past the walk area |
| `SH4DER_AOMESH_GMAX` | 65,536 | the most cells of the ground's grid |

**Y + A: the clean picture.** With the panel closed, Y and A pressed together hide everything the program draws over the picture (the readout, the panel, the inset, the heartbeat); the same keys, or START, bring it back (`psm_state_t.hide` in `psm_scene.h`, `psm_send_hud` in `psm_send.h`).

Suite 13 (`suite/suite_13.json`) has the square with both, as `square` (`build/sq_senior/sh4der-square-a5.elf`).

## One disc for every test: the suite

New test programs go on the suite disc, not on a disc each. A test takes three lines from `psm_suite.h` (after `sh4der_fault.h`): `suite_init();` first in main, and `if (suite_tick(buttons, page, pages)) { page++; redraw = 1; }` in the loop that shows its result pages. The same ELF still works on a disc of its own. `suite/NOTES.md` has the disc, what a start through the menu clears and does not clear, and how to add a test.

## The walled square (`SQUARE=square`)

The shadow program (`psm_dc.c`, `make psm-dc`) boots into a town. `SQUARE=square` puts the walled square in the town's place: stone walls with merlons, gate towers, a colonnade, a dais, a keep, with a fountain, an obelisk, four stalls, the hen pen and the ladder where the cast and the hens expect them. It uses the same header and the same movers, so the walks, hens, presets and sun are unchanged.

```sh
make psm-dc SQUARE=square BUILD=build/<new folder> PSM_DC_CDI=build/<new folder>/<new name>.cdi SH4ZAM=...
```

- **Bake:** `tools/bake_square.py` writes `psm_town_mesh.h` into `$(BUILD)/psm_square`, and the default town's `tools/bake_town.py` into `$(BUILD)/psm_town`, so the two never share a folder. The walls are quads baked from numbers. The pieces (pillars, the fountain's basin, awnings, banners, lanterns, fences, the dais's stairs, trees) come from the zip, cut by `third_party/dc-forerunner/tools/lod/qem_decimate.py`. With no zip each piece is a box (`PSM_TOWN_KIT 0`). The ground's paving slabs are `psm_ground.h` (64 x 64 RGB565, 8 KB, host test `test/psm_ground_test.c`).
- **The zip:** `kenney_fantasy-town-kit_2.0.zip` in `assets/_dl/`, downloaded by hand from the page in `fetch_table.json` (it has no stable link). `PSM_SQUARE_ZIP=` points at a copy elsewhere. Nothing from it is in the repository.
- **Flags:** the square's build also starts the program with the lazy shed plan and all three placement options: `-DPSM_PLAN_LAZY=1 -DPSM_PLACE_DEFAULT=7u` (`PSM_SQUARE_FLAGS` in the Makefile; the reasons are in `psm_dc.c`). `PSM_DC_EXTRA=` adds flags of your own. The town build has neither.
- **Credits:** `CREDITS.md` has the kit's row (Kenney, CC0 1.0) and says which parts of the square are our own. On the suite disc, a table entry with `"credits": "square"` sets `PSM_CREDITS_SQUARE`, which puts the WALLED SQUARE page into the menu's credits (`tools/make_credits.py` at the repository root: the kit and the sky). A build that carries the square must keep both.
- **Status:** it is on suite disc 12 as `square` (the live program, no result pages; `build/sq_a4`, built by hand with `-DPSM_PLAN_LAZY=1 -DPSM_PLACE_DEFAULT=7u` before the Makefile switch existed). Nothing about it is MEASURED in the ledger yet.
- **Shadow map edge, HOLD off:** with HOLD off the map covers the view only out to 70 m (`PSM_FAR`), and the ground runs to 92 m, where everything past the map is drawn lit. A host test found no visible 70 m shadow cut in the square. It compared the map as built with one pushed to 95 m, over 2,688 views of the walk at 09:00 to 17:00; the worst view lost 15 pixels of shadow. Route B only; route A was not rendered. No fade is applied.
