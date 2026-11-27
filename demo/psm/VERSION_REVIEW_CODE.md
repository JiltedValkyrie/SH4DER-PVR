# Version review, code side (proposal only)

For: lead, to compare with Scholar's evidence side (`VERSION_REVIEW.md`). Written 2026-11-25. **Proposal only:** no header, source or README was edited.

Read for this:

- every `Version:` line in `include/`, `src/` and `third_party/sh4zam-dce/include/`;
- the README effect table (README:91-119);
- the `TESTS` list in the `Makefile`;
- which demo calls which effect (the `#include` lines and the calls actually made);
- `wiki/effects/*.md` for what each page says ran on a console;
- the console log folders on the owner's drive (`logs/benchmarks/`: c4_c4, c4b, c4e, older, psm-bench-h2, ray1).

There are no log folders for the demo disc, the ocean, pl4nar, torchwalk, matchlight or cloudvolume. So "it ran on screen" for those rests on the documents below, not on a photo I could find.

SH4ZAM's own 0.10.0 is upstream and untouched. Nothing here renames it.

## 1. The scale

The present scale (README:119): v1.00 used in a released game; v0.50 runs on screen on the demo disc; v0.10 passes its maths tests.

**The gaps.**

1. Between v0.10 and v0.50 there is no step for:
   - code that builds for the Dreamcast and passes its SH-4 path checks;
   - code measured on a console in a bench or spike, but never shown in a demo.

   Examples: warp (timed in h2), `shz_dce_pvr_project` (timed in h2 and c4b), the grid walk (ray1).
2. Above v0.50 there is no step for "timed on screen in a demo, with a limit backed by a number" (`wiki/limits/`).
3. Below v0.10 there is no step for code that compiles but has no host test (bump).
4. "The demo disc" is too narrow: the town, the ocean and the other demos are their own programs.

**Proposed scale.** Each step needs every step below it. A step's evidence is a row in `wiki/hardware/` or `wiki/limits/`, per AGENTS.md. Emulator results never count; a host model never counts as a console.

| Version | Name | Rule (all must be true) | Who can check it |
|---|---|---|---|
| v0.05 | draft | It compiles in at least one program. It has no host test of its own. | code |
| v0.10 | maths tested | A host test in `make test` (plain C) passes and covers the header's pure functions. | code (`make test`) |
| v0.20 | SH-4 path checked | It passes `make test-shz` (SH4ZAM's path) and builds clean for the Dreamcast (`make dc`, `-Wall -Wextra`). An SH-4 asm kernel must also pass its instruction-level check (`tests/sh4run`). This is still code-side: no console. | code |
| v0.30 | measured in a bench | A console page timed this code, or checked its bytes, in a bench or spike. Status MEASURED, with date, console count and photo. | ledger |
| v0.50 | on screen in a demo | A demo program a user would run (demo disc, ocean, town, pl4nar, torchwalk, matchlight, cloudvolume) showed the effect on a real console, with a photo or log named in the ledger. | ledger |
| v0.70 | timed in a demo | v0.50, plus the effect's own cost in that demo measured on a console (on/off or its own timer), plus a limit in `wiki/limits/` called proven by that number. | ledger, limits |
| v1.00 | released | Used in a released game. | owner |

Two more rules:

- **Demotion.** A version marks evidence, not quality, so a console result that goes against the code does not lower its number. It adds a word. `v0.30, demoted` means it was measured and lost to another call; use the other on a hot path. The AGENTS.md rule ("show a measured gain") is met by the word, not by dropping the number.
- **One form.** Two digits after the point, as `v0.10`. `sh4der_film.h:49` says `v0.1`: that should read `v0.10`.

## 2. Library headers and sources

Key to the columns:

- **Test**: in the Makefile's `TESTS`.
- **Demos**: the effect's calls appear in that demo's source.
- **Console**: what the repository records as run on a real console.

| Item | Now | Implements | Test | Demos | Console | Proposed | Why / to reach the next step |
|---|---|---|---|---|---|---|---|
| `sh4der_core.h`, `src/sh4der_core.c` | v1.00 | SH4ZAM switch, helpers, feather, tube/blocker shapes, breath, glint | core_test | all | released game (README:119) | **v1.00** | unchanged |
| `sh4der_blade.h`, `.c` | v1.00 | tubes, blocker, pulse | blade_test, `example/blade.c` | example | released game | **v1.00** | unchanged |
| `sh4der_holo.h`, `.c` | v1.00 | breathing core table | holo_test | — | released game | **v1.00** | unchanged |
| `sh4der_bloom.h`, `.c` | v1.00 | blade ribbon, tip disc, rings | bloom_test | — | released game | **v1.00** | unchanged |
| `sh4der_look.h` | v0.50 | eye, grade, glare | look_test | demo disc (EYE, CURVE) | demo disc, no log found | **v0.50** if the owner gives the demo disc a ledger row; else v0.20 | v0.70: time the 80 x 60 read on a console |
| `sh4der_frame.h`, `.c` | v0.50 | grid read, stamp, gain off | frame_test | ocean, demo disc (through look) | h2 measured the uncached read rate it depends on (3.7 MB/s), not frame.c itself | **v0.50**, as look | v0.70: time `sh4der_frame` itself |
| `sh4der_taa.h` | v0.50 | jitter, history weight, pan shift | taa_test | demo disc (TAA rows), matchlight | demo disc, no log | **v0.50**, as look | — |
| `sh4der_glow.h`, `.c` | v0.50 | bright map, halo, darken, god rays | glow_test | demo disc (BLOOM, RAYS), ocean | ocean "runs on a real Dreamcast" (OCEAN.md WATER-1, 2026-11-18), not in the ledger | **v0.50**, as look | v0.70: the ocean's BLOOM row cost |
| `sh4der_torch.h`, `.c` | v0.50 | radial meshes, fog bend, gate | torch_test | demo disc (TORCH), torchwalk | demo disc, no log | **v0.50**, as look | — |
| `sh4der_ca.h`, `.c` | v0.50 | frame texture, three-copy CA | ca_test | demo disc (CA, RES), matchlight | demo disc, no log | **v0.50**, as look | — |
| `sh4der_shafts.h`, `.c` | v0.50 | dark copy cast from the light | shafts_test | demo disc (SH4FTS) | demo disc, no log | **v0.50**, as look | — |
| `sh4der_water.h` | **none** (README v0.50) | waves, ripple normals, Fresnel, reflect, crest sss | water_test, ocean_scene_test | ocean, cloudvolume | ocean, as glow | **v0.50**, as look; **add the missing `Version:` line** | — |
| `sh4der_sss.h` | v0.50 | back-lit transmission | sss_test | ocean (through `sh4der_water_crest_sss`) | ocean, as glow | **v0.50**, as look | — |
| `sh4der_warp.h`, `.c` | v0.10 | mesh warp, speed blur, pixelate, dither | warp_test | ocean uses only `sh4der_warp_dither_threshold` (maths) | **the draw timed in h2** (warp.md; MEASURED, one console, 2026-11-25) | **v0.30** | v0.50: a demo that draws the warp |
| `sh4der_trail.h`, `.c` | v0.10 | front-buffer copy, motion blur, TAA blend | trail_test | included by `demo/main.c`, **no call made** | none | **v0.20** if `make dc` and `make test-shz` are clean | v0.50: put it on a demo disc row |
| `sh4der_lens.h`, `.c` | v0.10 | vignette, grain, wedges, flash | lens_test | torchwalk, matchlight | no record | **v0.20** | v0.50: a photo of either demo |
| `sh4der_focus.h`, `.c` | v0.10 | blurred map through a mask | focus_test | none | none | **v0.20** | — |
| `sh4der_visor.h`, `.c` | v0.10 | night, amber, thermal | visor_test | none | none | **v0.20** | — |
| `sh4der_ao.h` | v0.10 | crease bake, cavity cue | ao_test | none | none | **v0.20** if `make test-shz` covers it, else v0.10 | — |
| `sh4der_guard.h` | v0.10 | zero-area and uncompiled-header checks | guard_test | **every console program** (town, c4, ocean, pl4nar, torchwalk, matchlight, cloudvolume, probe) | it ran in every console program on record (town TOWN rows, c4, h2, ray1); the faults it guards were seen on a console (README:1300-1306) | **v0.50** | it draws nothing; "on screen" means it ran in a demo that did |
| `sh4der_bump.h` | v0.14 (off the scale) | normal map pack and decode, vertex light, grey palette, fan send | **none** (torchwalk has no tests by design, but the header's pure functions have none either) | torchwalk | no log found | **v0.05** until a host test exists; then v0.10 / v0.20 | write `test/bump_test.c` for `from_height`, `texel`, `normal_decode`, `frame_flip`, `toward` |
| `sh4der_uvwarp.h` | v0.10 | table warp, prepared forms | uvwarp_test | none | none | **v0.20** | — |
| `sh4der_spark.h`, `.c` | v0.10 | pool, billboard, streak, burst | spark_test | matchlight, pl4nar | no record | **v0.20** | v0.50: a photo of either demo |
| `sh4der_bolt.h`, `.c` | v0.10 | branching strands | bolt_test | none | none | **v0.20** | — |
| `sh4der_motes.h`, `.c` | v0.10 | hashed specks | motes_test | matchlight | no record | **v0.20** | — |
| `sh4der_deform.h` | v0.10 | dent grid, decals | deform_test | matchlight | no record | **v0.20** | — |
| `sh4der_mist.h` | v0.10 | slice bank | mist_test | matchlight | no record | **v0.20** | — |
| `sh4der_rise.h` | v0.10 | rising lights | rise_test | matchlight | no record | **v0.20** | — |
| `sh4der_film.h` | `v0.1` | flicker, scratches, dust, weave | film_test | matchlight | no record | **v0.20**; write it `v0.10` / `v0.20` | — |
| `sh4der_cloudvolume.h` | v0.50 ("host prototype and Dreamcast demo; the console look is her check") | CPU cloud march into an 8-bit buffer | cloudvolume_test | cloudvolume | **no console record**; its CPU numbers are ESTIMATE (its own line 13) | **v0.20** until a console photo of `make cloudvolume`; then v0.50 | its v0.50 says the console check is still to come, so by this scale it is not yet v0.50 |
| `sh4der_pl4nar.h`, `.c` | v0.10 | view rows, lookup, materials, target, pass | pl4nar_test | pl4nar demo | its README: "never booted on an emulator or a console" from this folder | **v0.20** | v0.50: boot the pl4nar image |
| `sh4der_pool.h` | v0.10 | disc and wedge fans | pool_test | pl4nar, matchlight | no record | **v0.20** | — |

## 3. The psm family

| Item | Now | Implements | Test | Demos | Console | Proposed |
|---|---|---|---|---|---|---|
| `sh4der_psm.h` | v0.10 | uniform and warped fit, depth key, bias, footprints, frustum, slab | psm_test, psm_scene_test, psm_town_test | the town (`demo/psm/psm_scene.h`: `sh4der_psm_build_*`, `_frustum`, `_texel_m`, `_pixel_m`) | town on screen and timed by its HUD (TOWN-001 and later, MEASURED 2026-11-24, one console; psm.md) | **v0.50**. v0.70 needs a limit in `wiki/limits/` backed by the town's numbers |
| `sh4der_psm_pvr.h` (route A) | v0.10 (line 35, inline) | caster triangles for a render target, lookup coordinates, uv error | psm_pvr_test | town route A (`sh4der_psm_pvr_part_`) | TOWN-001 is route A on screen | **v0.50** |
| `sh4der_psm_raster.h` (route B) | v0.10 | 16-bit depth raster, compares | psm_raster_test, psm_mapsrc_test | town route B (`sh4der_psm_raster_begin`, `_tri_h`, `_lit_n`) | it is in the town image, but I found **no ledger row that names route B on screen**; its per-texel cost is thesis F14's MODEL | **v0.20**; v0.50 when a TOWN row shows route B on screen |
| `demo/psm` (town, benches, spikes, recipes) | — | programs, not library | many | — | many ledger rows | no version: programs |

The `psm_pvr.h` version sits mid-sentence in its comment (`... the arrays. Version: v0.10`). Move it to its own line, as the others have, so a grep for `^ *Version:` finds it.

## 4. sh4zam-dce

**Does it need its own version? Yes.** It is a separate package with its own README, checks and handover. Give it one package version, plus a `Version:` line in `shz_dce.h`, which has none. Name it "sh4zam-dce v0.xx" everywhere, never "SH4ZAM v0.xx", so it cannot be confused with SH4ZAM's own 0.10.0.

**Proposed package version: v0.20.** Every header passes host tests and the SH-4 checks (`tests/run_checks.py`; full run `dce_20261125_035023_552751.log` rc=0, as commit 915529d records). Only two of its 37 added names have been timed on a console. The package should not claim more than most of its parts.

| Header | Now | What it holds (README table) | Console | Proposed |
|---|---|---|---|---|
| `shz_dce.h` | **none** | umbrella include | — | the package version, **v0.20** |
| `shz_dce_cdefs.h`, `shz_dce_override.h` | v0.10 | definitions; SH4ZAM names on the reworks | — | **v0.20** |
| `shz_dce_atan2.h` | v0.10 | now wrappers of SH4ZAM's routines (taken upstream as cff97e8 after its author's console run, REPORTED in `doc/THESIS.md` §7) | the author's run, not ours | **v0.20** (our evidence); SH4ZAM's own version is upstream's business |
| `shz_dce_sqrt.h`, `_trig.h`, `_vector.h`, `_quat.h`, `_planes.h`, `_gather.h`, `_vertex.h`, `_xform.h`, `_project.h` | v0.10 | batch and reworked routines | **untimed** ("not for hot paths", README:30) | **v0.20** |
| `shz_dce_pvr.h` | v0.10 | `shz_dce_pvr_project` | **MEASURED, gain shown**: 21.15 against 43.19 cycles in the cache, 58.09 against 74.07 out of it (h2 and c4b, SH4-023; kept) | **v0.30**. Not v0.50: it ran on screen only in the c4 spike, not in a demo |
| `shz_dce_xmtrx.h` | v0.10 | the batch transforms and projection | the projection MEASURED in ray1 h3: 14.15 / 45.99 against `mat_transform` 13.38 / 42.86; the other three untimed | **v0.30, demoted** (the projection); the README already says "use `mat_transform` on a hot path" |
| `shz_dce_shadow.h`, `shz_dce_tri_row.h` | v0.10 | stop-gaps until SH4ZAM has its own (README:58) | tri_row: the MODEL only (43 / 12 cycles a texel, thesis F14) | **v0.20**, and mark them "stop-gap" next to the version |
| `tests/sh4run/sh4run.h` | v0.10 | test harness | — | no version needed (a tool); or v0.20 with the package |

## 5. README effect table

**Rows missing from the table** (README:91-117), with their proposed versions:

- bolt (v0.20)
- motes (v0.20)
- deform (v0.20)
- mist (v0.20)
- rise (v0.20)
- film (v0.20)
- cloudvolume (v0.20)
- `sh4der_psm_raster.h` and `sh4der_psm_pvr.h`, inside the psm row or as rows of their own (v0.20 / v0.50)

**Rows to change:**

- psm → v0.50.
- warp → v0.30.
- guard → v0.50.
- bump v0.14 → v0.05.
- cloudvolume's header v0.50 → v0.20.
- trail, lens, focus, visor, ao, uvwarp, spark, pl4nar, pool → v0.20, once `make dc` and `make test-shz` are confirmed clean on today's tree.
  - I did not run those two targets for this review.
  - If either fails, the item stays at v0.10.

**README:119** would become the six-step rule of §1. Its sentence on `sh4der_core.h` stays.

## 6. What the owner must decide

1. **Whether the scale of §1 is adopted**: the v0.05, v0.20, v0.30 and v0.70 steps, and the word "demoted".
2. **The nine v0.50 claims with no ledger row**: look, frame, taa, glow, torch, ca, shafts, water, sss.
   - They rest on the demo disc and the ocean running on a console. OCEAN.md says so for 2026-11-18; I found no photo or log name.
   - AGENTS.md says a console result not in the ledger is not kept. So either the owner gives one photo of the demo disc and one of the ocean, filed as ledger rows, or these nine drop to v0.20 until then.
   - This is Scholar's evidence side to confirm.
3. **cloudvolume's v0.50**: it states its own console check is still to come. Under either scale that is not v0.50 yet.
4. **bump**: either a host test is written (v0.10 and up), or its version reads v0.05 for now.
5. **Whether `make dc` and `make test-shz` are run once on today's tree** before any v0.20 is written. I proposed v0.20 from the code's structure, not from a run today.
