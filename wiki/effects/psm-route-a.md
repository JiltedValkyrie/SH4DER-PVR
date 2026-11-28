---
title: "Moving shadows drawn by the PVR [psm_a] (route A pass)"
tags: [effect, shadow, psm, psm_a, pvr, render-target, tilecut, pvrx]
updated: 2026-11-28
sources: [include/sh4der_psm_a.h, src/sh4der_psm_a.c, test/psm_a_test.c, include/sh4der_tilecut.h, include/sh4der_pvrx.h, src/sh4der_pvrx.c, README.md "The psm route A pass", wiki/hardware/ledger-pvr.md PVR-108 PVR-113 PVR-117 PVR-118 PVR-197]
---
# Moving shadows drawn by the PVR [psm_a]: the route A pass

**What you will learn:** how up to four characters get a shadow on a patch of ground from one light, why it is the cheaper way to do it, how to keep its tile cut from stalling the frame, what it cannot do, and what has and has not been run.
**For:** a programmer with a few moving characters on a floor who wants their shadows without a depth compare on the main CPU.

> **In plain words.** Stand a character in front of a lamp and look at the wall behind: the shadow is just the character's outline, filled in dark. This does the same. From the light's side, the graphics chip draws each character flat black on white into a small picture (128 by 128 dots). Then the ground is drawn once more with that picture stuck on it and multiplied over what is already there, so the ground goes dark where the picture is dark. The main CPU only has to place the characters' triangles in the picture. The catch: a character cannot shadow itself (its arm on its own body), and there is one light. Each character has a strength from 0 to 1, so a shadow can fade in or out, or hand over to a simple blob shadow.

## Try this first

What it is for: a few people or animals on a flat floor, one sun or lamp, shadows you can fade.

1. At load, after `pvr_init`: call `sh4der_pvrx_wait_hook(1)` if you want polled render-done waits (and again after any later `pvr_init`; call `sh4der_pvrx_wait_hook(0)` before `pvr_shutdown`). Then `sh4der_psm_a_cfg_default(&cfg)`; `size` is `SH4DER_PSM_A_SIZE` = 128, `cut` is `SH4DER_PSM_A_CUT_PASS` (1) for per-pass cut or `SH4DER_PSM_A_CUT_HOLD` (2) for holding the cut on the pass's own table (`SH4DER_PSM_A_CUT_NONE` = 0 for uncut). Call `sh4der_psm_a_target_init(&t, &cfg)` (0 means no video memory: draw no shadow) and `sh4der_psm_a_init(&a, 128, row)`. On an ordinary screen `row` is 0. For a screen whose tile table has rows left out, see [Where the pass sits](#where-the-pass-sits).
2. Each frame: `sh4der_psm_a_begin_frame(&a, shadow)`; `shadow` is how dark the full shadow is, a level 0 to 1 a colour channel (`sh4der_psm_a_shadow_level(ambient, sun x cosine)` gives it from the lighting). Then `sh4der_psm_a_fit(&a, light, centre, 4.0f, y_lo, y_hi)`: the patch of ground, here 8 m across (`SH4DER_PSM_A_SQUARE_M`, ADVISED, the starting patch for a 128 map).
3. `sh4der_psm_a_begin`, then `sh4der_psm_a_caster` for each character (at most `SH4DER_PSM_A_CASTERS` = 4, at most `SH4DER_PSM_A_TRIS` = 2,200 triangles together, strongest first), then `sh4der_psm_a_end` (ends the scene through `sh4der_pvrx_scene_finish`) and `sh4der_psm_a_settle`. On a frame without a pass in `SH4DER_PSM_A_CUT_HOLD` mode (or before exit / `pvr_shutdown`), call `sh4der_psm_a_release(&t)` before opening the screen scene.
4. In the screen's translucent list, after the ground: send `sh4der_psm_a_layer_header`, then the ground patch's triangles with `sh4der_psm_a_layer_vertex` for each vertex. When `sh4der_pvrx_wait_hook(1)` is on, finish the screen scene with `sh4der_pvrx_scene_finish()` (never `pvr_scene_finish()` directly).

Settings are the README "Psm route A tuning" values, taken from the town demo where it had one: ADVISED. Per-pass cut with sleeping waits ran in build ra2; polled waits and held single-table cut are NOT RUN on a console yet.

Header: [include/sh4der_psm_a.h](../../include/sh4der_psm_a.h); source: [src/sh4der_psm_a.c](../../src/sh4der_psm_a.c). Guide: [Shadows and depth](../start-here/genre-shadows.md#moving-shadows-drawn-by-the-pvr-psm_a).

## What it does

- **The map.** The library's uniform map ([psm](psm.md)) fitted to the ground patch and held still: the fit's size is kept and its centre moves in whole dots, so a moving camera moves no dot and the shadow's edge does not crawl. On the host, an 8 m patch under a sun 64 degrees up gives a dot of 9.8 cm on 128 dots (`test/psm_a_test.c`). A bigger patch needs a bigger map or a rougher shadow.
- **The pass.** One scene of its own, drawn before the screen's: white ground, each character's triangles clipped to the picture and drawn flat in their own grey, a white frame one dot wide so a lookup that lands outside is lit. The grey is white at strength 0 and the full shadow level at strength 1.
- **The tile cut and waits.** Three modes in `cfg.cut`:
  - `SH4DER_PSM_A_CUT_NONE` (0): uncut baseline.
  - `SH4DER_PSM_A_CUT_PASS` (1): cuts both tile tables for the pass and puts them back in `sh4der_psm_a_settle`. Render-done waits go through `sh4der_pvrx_wait_render_done_poll` followed unconditionally by `pvr_wait_render_done` (which, by reading KallistiOS's `pvr_scene.c` (DERIVED, NOT RUN), returns at once when idle and sleeps only if a scene skipped `sh4der_pvrx_scene_finish` or the polled wait timed out); `pvr_wait_ready` stays sleeping so any stall on the console separates ready from render-done. When `sh4der_pvrx_wait_hook(1)` is off, render-done falls back to KallistiOS's sleeping `pvr_wait_render_done` (ra2 MEASURED a 20.0 ms frame; the cause — two 10 ms scheduler ticks, as in PVR-117 — is DERIVED from reading KallistiOS's source and to be tested by ra5). With `sh4der_pvrx_wait_hook(1)` on, it spins on the interrupt counter instead — NOT RUN on a console as this pass yet.
  - `SH4DER_PSM_A_CUT_HOLD` (2): KallistiOS flips between its two tile tables at every render start, so with one pass and one screen each frame the pass uses one table and the screen the other. After `pvr_wait_ready`, `PVR_ISP_TILEMAT_ADDR` names the table of the render in flight, so the pass cuts only the other table (`sh4der_tilecut_begin_one_at`) and leaves it cut across frames, removing the render-done wait in `begin` and both the wait and the restore in `settle`. HOLD needs the pass to be every second render: a third render per frame (such as a mirror pass) puts another scene through the held table every other frame. Put it back with `sh4der_psm_a_release` whenever the order breaks (a frame without a pass, another scene before the screen, exit, or `pvr_shutdown`). Host tested (`test/tilecut_test.c`), NOT RUN on a console yet.
- **The layer.** The ground patch as one piece, drawn after the ground with the picture as its texture and a multiply blend. Each corner carries where it lands in the picture.
- **The accounting.** A character over its own triangle cap, a fifth character, or one past the pass's triangle total is refused (counted in `a.refused`): draw a blob for it.
- **Check on the host.** A box's shadow on an 8 m patch, through the whole route against an exact ray from each ground point to the light, at three suns: 0.03 % to 0.3 % of ground points read wrong, all at the shadow's edge (`test/psm_a_test.c`, HOST; the fill is the test's own, not the PVR's).

## Where the pass sits

A render-target scene is drawn from tile row 0. A port that shows a 640 x 360 band inside the 640 x 480 signal and compacts its tile table to rows 1 to 13 has no entry for row 0. So the pass is drawn at tile row `cfg.row` (2 for that port, y = 64) and the render base is set so the texture is exactly 128 x 128 from there: the render is `row x 32 + size` lines tall, and the texture starts `row x 32` lines after the allocation. KallistiOS sets a render target's pixel clip to the target's own size, not the screen's, so the narrowed screen clip does not cut it. For that port: `cfg.row = 2; cfg.rows = 13; cfg.row0 = 1`. The tile cut ([tilecut](tilecut.md)) writes only the target's 4 x 4 tiles: `sh4der_tilecut_begin_at(&cut, 4, 4, row)` (or `sh4der_tilecut_begin_one_at` for held mode) on a table made with `sh4der_tilecut_init_rows(&cut, 20, 13, 1)`.

## Cost

| Item | Cost | Status |
|---|---|---|
| Map pass PVR, build ra2, uncut vs per-pass cut | 3.2 ms uncut -> 0.2 ms cut at 128, 0.8 ms at 256 (same picture) | MEASURED, one console, one run (build ra2) |
| Frame time, build ra2, sleeping waits | 16.6 ms uncut -> 20.0 ms cut (worst 25 ms; pass total 19.9 ms); cause (two 10 ms sleep ticks) is DERIVED from KallistiOS source, to be tested by ra5 | MEASURED, one console, one run (build ra2); cause DERIVED |
| Polled waits (`SH4DER_PSM_A_CUT_PASS` + `sh4der_pvrx_wait_hook(1)`) | not run as this pass (PVR-117 saw 39.9 -> 23.3 ms in another test; PVR-118 saw ~1 in 5 frames at 26.5 ms) | NOT RUN |
| Held single-table cut (`SH4DER_PSM_A_CUT_HOLD`) | no render-done wait in `begin` or `settle`, no per-frame table rewrite | NOT RUN |
| The route in the town, SH-4 a frame | 13.3 to 13.6 ms, against 40.2 to 43.9 ms for the SH-4 depth map (route B), same scene | MEASURED, one console, one run (PVR-108 to PVR-113) |
| TA data a caster triangle | 3 vertices, 96 bytes after clipping; 2,200 triangles at most 211 KB | ESTIMATE |
| Video memory | size x (row x 32 + size) x 2 bytes: 48 KB at row 2, 32 KB at row 0 | exact, by the code |

## Recommended limits

- Four characters and 2,200 triangles a pass is the port's budget, not a measured limit. Prove it with a console run before calling it a limit ([limits](../limits/recommended-limits.md)).
- A low-triangle stand-in for each character (180 to 330 triangles) is enough for a silhouette; send the body without the head if it has no low tier.
- No self-shadow. For that, use the SH-4 depth map ([psm_raster](psm.md)), which costs about three times the SH-4 time.
- One light. A second needs a second map.

## Pitfalls

- Call `sh4der_psm_a_settle` before the screen's scene opens, and shut the PVR down and start it again if it returns other than 0.
- While `sh4der_pvrx_wait_hook(1)` is on, finish EVERY scene (screen and target) with `sh4der_pvrx_scene_finish()`, never `pvr_scene_finish()` directly, or the wait counter goes out of step. Call `sh4der_pvrx_wait_hook(0)` before `pvr_shutdown` and `sh4der_pvrx_wait_hook(1)` again after any new `pvr_init`.
- In `SH4DER_PSM_A_CUT_HOLD` mode, call `sh4der_psm_a_release(&t)` before any frame without a pass, before another scene that would use the pass's table, at exit, or before `pvr_shutdown`. If `sh4der_psm_a_release` is forgotten, the screen renders through the cut table so only the pass's few tiles are drawn and the rest of that buffer is stale; the library sees it only at the next `begin` (`cut_broken`). HOLD also needs the pass to be every second render: a third render per frame (such as a mirror pass) puts another scene through the held table every other frame.
- Do not run it from a menu screen (RGB888P).
- An overlap keeps the first character drawn at equal depth: send the strongest first.
- The receiver must be one piece, a few centimetres above the floor, or the shadow is drawn twice where pieces overlap.

## Maturity

PROTOTYPE v0.2. Host tests pass (`test/psm_a_test.c`, `test/tilecut_test.c`, `test/pvrx_test.c`). Build ra2 ran on one console (per-pass cut with sleeping waits: PVR fell from 3.2 ms to 0.2 ms at 128 and 0.8 ms at 256 with the same picture, and the frame stalled from 16.6 to 20.0 ms; the sleeping-tick cause is DERIVED from KallistiOS's source). NOT RUN on a console: polled render-done waits via `pvrx` (`ra5`), held single-table cut (`SH4DER_PSM_A_CUT_HOLD`, `ra5`), and a compacted table with row 0 left out.

## Related

- [Sun shadow map [psm]](psm.md), [routes A, B and AB](../shadows/psm-routes.md), [Tile-cut pass [tilecut]](tilecut.md), [Soft character shadows [sh4dow]](../start-here/genre-shadows.md#soft-character-shadows-sh4dow)
