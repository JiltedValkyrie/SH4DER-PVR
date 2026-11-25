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
