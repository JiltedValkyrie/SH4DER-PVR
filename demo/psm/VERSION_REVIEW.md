# Version review: evidence for each "Version:" line (proposal, nothing edited)

Written 2026-11-25 by Scholar from the wiki ledgers, the effect pages and the demo sources, for lead and the owner. Cortana writes the code-side review (feature completeness, tests); lead compares the two; the owner approves. **No header was changed.** SH4ZAM's own 0.10.0 is upstream and out of scope.

## The scale (README.md:119)

- **v1.00**: used in a released game.
- **v0.50**: runs on screen on the demo disc.
- **v0.10**: passes its maths tests.

**How evidence is graded below.**

- **MEASURED**: a ledger row from our console, with its photo.
- **On screen (REPORTED)**: a program that includes the header is said to have run on a console, but no photo or ledger row backs it.
- **Host**: host tests only, or the `sh4run` SH-4 interpreter. `sh4run` checks alignment, registers and bit-exact FPU models on the host. It is not a console, and its cycle counts are ESTIMATE (`third_party/sh4zam-dce/tests/sh4run/sh4run.h:1-35`).

**Decision 1 for the owner (the scale).** The scale names one disc, `demo/main.c`. Several headers have run, MEASURED, on a console in other programs: the town shadow demo, the h2 bench, the c4 spikes. By the letter of the scale these stay at v0.10. Two ways to read it:

- **(a)** Read "the demo disc" as "a demo or bench disc run on a console, with a ledger row". Then the items marked ▲ below go to v0.50.
- **(b)** Keep the letter and add a tier: **v0.20 = ran on a real console, with a MEASURED ledger row, outside the demo disc**. The items marked ▲ go to v0.20.

I recommend (b). It keeps v0.50 tied to the demo disc and still records the console evidence. Every proposal below gives both.

## Summary of proposals

| Item | Now | Proposed | Why, in one line |
|---|---|---|---|
| `sh4der_psm.h`, `sh4der_psm_raster.h`, `sh4der_psm_pvr.h` ▲ | v0.10 | v0.20 (b) / v0.50 (a) | All three ran on screen in the town demo on a console; 8 MEASURED rows (PVR-108 to PVR-115) |
| `shz_dce_pvr.h` (`shz_dce_pvr_project`) ▲ | v0.10 | v0.20 (b) / v0.50 (a) | MEASURED gain over its C form on a console (SH4-023); ran in c4, c4b and ray1 (SH4-046, SH4-048, SH4-053) |
| `sh4der_warp.h`, `src/sh4der_warp.c` ▲ | v0.10 | v0.20 (b) / v0.50 (a) | Drawn on a console in bench h2 and timed (BUS-004, photo `PXL_20261125_072120744`) |
| `sh4der_guard.h` ▲ | v0.10 | v0.20 (b) / v0.50 (a) | Compiled into every console program that ran (town, ocean); it draws nothing. Its faults are FACT only from README:1304-1306, with no photo |
| `sh4der_bump.h` | v0.14 | v0.50 **if** a torch-walk photo is filed | The torch walk shows its cost label on a console (REPORTED, `wiki/effects/bump.md:40`); no photo in the ledger. It is not on the demo disc |
| `sh4der_film.h` | "v0.1" | v0.10 | Typing only: every other line reads v0.10 |
| `sh4der_water.h` | (no line) | add "Version: v0.50" | README's table says v0.50; the header has no Version line |
| `shz_dce_xmtrx.h` (`shz_dce_xmtrx_project_point3_batch`) | v0.10 | **keep v0.10**; owner decides whether it stays | Right bytes, but 5.8 % / 7.3 % slower than `mat_transform` on a console (SH4-050): no measured gain under AGENTS.md's rule |
| Everything else | as now | no change | Details below |

## Library headers and sources

| Item | Now | Evidence | Next level needs |
|---|---|---|---|
| `sh4der_core.h`, `src/sh4der_core.c` | v1.00 | Used in a released game (README:119; REPORTED by the author). No console cost measured (`wiki/effects/core.md:33`). | Top of the scale. A MEASURED console cost would back the number, but the scale does not ask for one. |
| `sh4der_blade.h`, `.c` | v1.00 | Same. "None of it is measured on a console" (`wiki/effects/blade.md:29`). | As core. |
| `sh4der_bloom.h`, `.c` | v1.00 | Same (`wiki/effects/bloom.md:32`). | As core. |
| `sh4der_holo.h`, `.c` | v1.00 | Same (`wiki/effects/holo.md:28`). | As core. |
| `sh4der_look.h` | v0.50 | On the demo disc: row EYE (`demo/main.c:94-97`), on in the owner's defaults of 2026-11-11 (main.c:97). On screen REPORTED; no ledger row for the effect. The read method it uses is MEASURED (SH4-040, SH4-041, SH4-006). | v1.00: a released game. Worth filing: the 80 × 60 read's console time (Q50). |
| `sh4der_frame.h`, `.c` | v0.50 | On the demo disc through look, glow and shafts, and in the ocean demo. Read methods MEASURED (SH4-040, SH4-041, SH4-006); the read's own time is not measured (`wiki/effects/frame.md:36`). | v1.00: a released game. |
| `sh4der_glow.h`, `.c` | v0.50 | Demo disc rows BLOOM and RAYS (main.c:94); ocean demo. On screen REPORTED. Its layer cost follows from PVR-002 (MEASURED); the glow's own cost is not measured (Q39). | v1.00: a released game. |
| `sh4der_shafts.h`, `.c` | v0.50 | Demo disc row SH4FTS. On screen REPORTED. | As glow. |
| `sh4der_taa.h` | v0.50 | Demo disc rows TAA, TAAHOLD, TAAMODE. On screen REPORTED. | As glow. |
| `sh4der_torch.h`, `.c` | v0.50 | Demo disc row TORCH; also the torch walk. On screen REPORTED. | As glow. |
| `sh4der_ca.h`, `.c` | v0.50 | Demo disc row CA, on in the owner's defaults (main.c:97). VID-001 (frame texture sizes); VID-003 (a stride register it shares with trail). | As glow. The second scene's console cost is OPEN (`wiki/effects/ca.md:51`). |
| `sh4der_sss.h` | v0.50 | In the ocean demo through `sh4der_water_crest_sss`. OCEAN.md:5 (2026-11-18) says the ocean runs on a real Dreamcast: REPORTED. It is not on `demo/main.c`'s disc. | Keep v0.50 if the ocean disc counts as "the demo disc" (Decision 1). v1.00: a released game. |
| `sh4der_water.h` | none in the header; README v0.50 | Ocean demo on a real Dreamcast (REPORTED, OCEAN.md:5). "A stable 33.3 ms not yet measured on hardware." | **Add the missing "Version: v0.50" line.** Worth filing: the ocean's frame time on a console. |
| `sh4der_cloudvolume.h` | v0.50 | Its own Dreamcast demo; the header says "the console look is her check". The march's cost is ESTIMATE, not measured (`wiki/effects/cloudvolume.md:26`). No ledger row. | Keep v0.50 on the owner's word. A photo plus the march's counters on a console would make it MEASURED. |
| `sh4der_bump.h` | v0.14 | Host tests. The torch walk (`demo/torchwalk_ab.c`) shows a cost label on the console (REPORTED, `wiki/effects/bump.md:40`), but no photo is filed. The PVR bump-unit ledger rows (PVR-045 on) are **not** this header's method since v0.14 (`demo/psm/BUMP_REVIEW_NOTES.md`). | v0.50: a filed photo of the torch walk's cost label, as a ledger row. Open on a console: the azimuth's turn (`frame.flip`) and the 565 palette's banding (AGENTS.md). |
| `sh4der_warp.h`, `.c` ▲ | v0.10 | **MEASURED** in bench h2: pixelate pass 241 against 366 ns a vertex by store-queue runs; mesh 973 against 914 (BUS-004; photo `PXL_20261125_072120744`; `wiki/effects/warp.md:32-58`). It is not on the demo disc. | (b) v0.20 now. v0.50: a demo disc row. Open: why the mesh is slower by runs (Q9). |
| `sh4der_guard.h` ▲ | v0.10 | In every console program: town (`psm_dc.c`), ocean, torch walk, cloudvolume, street, film. The town run's rows (PVR-108 to PVR-115) were drawn with it. The faults it guards are FACT from README:1304-1306, seen by the author, with no photo (PVR-080, PVR-081, VID-009). | (b) v0.20 now. A photo of a fault it stops would make PVR-080 and PVR-081 MEASURED. |
| `sh4der_trail.h`, `.c` | v0.10 | Included by `demo/main.c:47`, but main.c calls nothing from it. Its two copies are about 19 ms of PVR time (Q40, ESTIMATE). The front-buffer constants are unverified (`wiki/effects/trail.md:55`). | v0.50: a demo disc row that calls it, seen on screen. Its console cost (Q40). |
| `sh4der_lens.h`, `.c` | v0.10 | In the torch walk and the Match Light film. No console record. | v0.50: a demo disc row; or (a) a filed torch-walk photo. |
| `sh4der_pl4nar.h`, `.c` | v0.10 | The street demo compiles for the Dreamcast. Its numbers are from an emulator: "not evidence" (`wiki/effects/pl4nar.md:38-45`; `demo/pl4nar/README.md:13`). | A console run of the street demo and the cost table in `demo/pl4nar/AGENT_WORKFLOW.md:60-71`. |
| `sh4der_pool.h` | v0.10 | Street demo and Match Light; no console record. The fan's fill is not measured (`wiki/effects/pool.md:38`). | As pl4nar. |
| `sh4der_spark.h`, `.c` | v0.10 | Street demo and Match Light; no console record. | A console run of either. |
| `sh4der_motes.h`, `.c` | v0.10 | Match Light; no console record. | A console run of Match Light. |
| `sh4der_deform.h`, `sh4der_mist.h`, `sh4der_rise.h` | v0.10 | Match Light; host tests. Console time not measured (`wiki/effects/deform.md:40`, `mist.md:37`, `rise.md:28`). | A console run of Match Light, with its counts. |
| `sh4der_film.h` | "v0.1" | Match Light; host tests (`test/film_test.c`). | **Correct the line to v0.10.** Then as Match Light. |
| `sh4der_bolt.h`, `.c` | v0.10 | Host tests only (`test/bolt_test.c`); in no demo. | A demo row, seen on a console. |
| `sh4der_focus.h`, `.c` | v0.10 | Host tests only; in no demo (`wiki/effects/focus.md:28`). | As bolt. |
| `sh4der_visor.h`, `.c` | v0.10 | Host tests only; in no demo (`wiki/effects/visor.md:34`). | As bolt. |
| `sh4der_ao.h` | v0.10 | Host tests only. | As bolt. |
| `sh4der_uvwarp.h` | v0.10 | Host tests only. | As bolt. |

## The psm family

| Item | Now | Evidence | Next level needs |
|---|---|---|---|
| `sh4der_psm.h` ▲ | v0.10 | Host tests pin the chapter's equations (`test/psm_test.c`). It is in the town demo (`demo/psm/psm_scene.h`) for the uniform and PSM maps, **MEASURED on screen**: PVR-108 to PVR-115, one console, 2026-11-25. Texels per pixel at 4 / 10 / 25 / 60 m are on each row. | (b) v0.20 now. v0.50: a demo disc row. v1.00: a released game. |
| `sh4der_psm_raster.h` ▲ | v0.10 | Host tests (`test/psm_raster_test.c`). Route B in the town: SH-4 40.2 / 43.9 ms, 14 / 13 fps (PVR-112, PVR-113, MEASURED). Its MOVCA clear rests on SH4-004 (MEASURED). | (b) v0.20 now. Note its measured limit: too slow for a whole town at 30 fps (recommended limits). |
| `sh4der_psm_pvr.h` ▲ | v0.10 | Host tests (`test/psm_pvr_test.c`). Route A in the town: PVR 15.9 ms, 18 fps (PVR-108 to PVR-111, MEASURED). The header's own note says its render-target pass and layer are "not written yet"; the town demo has its own. | (b) v0.20 now. For v0.50 the library's own target pass and layer should exist, not only the demo's. |
| The spikes and benches in `demo/psm/` (c1, c3b, c4, c4b, h1, h2, ray1) | no Version lines | The c4 chain is MEASURED (PVR-090 to PVR-107). These are spikes, not library files. | Nothing to version until the chain becomes a header. |

## sh4zam-dce

All of these are v0.10. Host tests run through `third_party/sh4zam-dce/tests/run_checks.py`, and the SH-4 build runs in `sh4run` (host; ESTIMATE cycles). The full run on 2026-11-25, `dce_20261125_035023_552751.log`, returned rc=0 (commit 915529d).

| Header | Now | Console evidence | Proposal and next level |
|---|---|---|---|
| `shz_dce_pvr.h` ▲ | v0.10 | **MEASURED.** `shz_dce_pvr_project`: 21.15 cycles a vertex against 43.19 for its C form in the cache, 58.09 against 74.07 out of it, same bytes (SH4-023, h2). In c4b, with its arrays placed apart, 60.5-61.7 against 76.7-79.2 (SH4-048). In ray1, 58.91 / 59.04 (SH4-053). It ran on screen in the c4 and c4b live views (PVR-090, PVR-107). It has a measured gain on a console, as AGENTS.md requires of an asm kernel. | (b) v0.20 now. The in-cache figure in a real frame (CACHE_NOTES item 2). |
| `shz_dce_xmtrx.h` | v0.10 | **MEASURED, no gain.** `shz_dce_xmtrx_project_point3_batch`: 14.15 / 45.99 cycles against `mat_transform`'s 13.38 / 42.86 (SH4-050). | **Keep v0.10.** Under AGENTS.md's rule it has no measured gain; whether it stays is the owner's call. |
| `shz_dce_project.h` | v0.10 | None found in a console image. | A console bench with a gain. |
| `shz_dce_trig.h`, `shz_dce_atan2.h`, `shz_dce_sqrt.h` | v0.10 | Host only. SH4ZAM's software back end was checked on the host (SH4-019); the console back end is unchecked. Since 915529d, asinf, acosf and atan2f are wrappers of SH4ZAM's. | The trig probe on a console (Q27). |
| `shz_dce_override.h`, `shz_dce_cdefs.h` | v0.10 | Infrastructure. `cdefs` is compiled into the c4, c4b and ray1 images through `shz_dce_pvr.h`, so it ran on a console, but there is nothing in it to measure. | Keep. A version step here would follow its users. |
| `shz_dce_gather.h`, `shz_dce_planes.h`, `shz_dce_quat.h`, `shz_dce_shadow.h`, `shz_dce_tri_row.h`, `shz_dce_vector.h`, `shz_dce_vertex.h`, `shz_dce_xform.h` | v0.10 | Host tests and `sh4run` only. No console image found that includes them. | A console bench each, with a gain against SH4ZAM or KOS. |
| `tests/sh4run/sh4run.h` | v0.10 | A host tool; it does not run on a console. | Not on this scale. |

## What I did not check

- The README effect table's other columns, and whether every row matches its header's line. I checked water (no header line) and film ("v0.1").
- `sh4dow-dc/` (its own project; no "Version:" line found in its headers).
- Whether the owner has run the street demo or Match Light on a console. No record says so; if they have, a photo turns several "no change" rows into proposals.
- Code completeness and test coverage. That is Cortana's half.
