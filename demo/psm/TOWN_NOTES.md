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
