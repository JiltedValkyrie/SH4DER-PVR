# demo/psm/recipes: host models of the pass recipes

Host reference models and tests for four recipes of `demo/psm/RECIPES_NOTES.md`. Host only, for review: nothing in `include/`, `src/` or `test/` uses this folder, and nothing here has run on a console.

| File | Recipe | What it checks |
|---|---|---|
| `recipes.h` | R1, R3, R4, R5 | the SH-4 side as inline functions: SH4ZAM calls, or plain C under `SH4DER_CORE_NO_SH4ZAM` |
| `recipe_cel_test.c` | R1 cel ramp | u against a reference; band edges under both texel-centre conventions; perspective-correct u is the world-space interpolation; mesh error against ramp error on a sphere |
| `recipe_dither_test.c` | R3 dither and dissolve | the punch-through rounding rules against the c3b and c4 counts; the console lines that split the rules still standing; the fade table per rule; the dither index |
| `recipe_fogpulse_test.c` | R4 fog pulse and contours | the table index against an exact reference and c3b's two fog lines; pack and read back; pulse peak, loss and width in metres; contours |
| `recipe_softplane_test.c` | R5 soft particles | the fade against a reference; the error and the alpha left on the contact line by cut size; two planes |
| `CONSOLE_PAGES.md` | R3, R7, R8 | the console pages that would show them (spec only, not built) |
| `R3_PAGE_SPEC.md` | R3 | the punch-through product page in detail, for coding from c4's `punch_pass`: lines, counts per rule, the decision table |
| `SH4ZAM_NOTES.md`, `shz_notes_repro.c` | — | notes for SH4ZAM's author on `shz_ldexpf` / `shz_pow2f` and `shz_log2f`, with a repro (not filed upstream) |

## Run

From this folder:

    make test                         # plain C
    make test-shz SH4ZAM=<checkout>   # SH4ZAM's software back end (default ../../../third_party/sh4zam)

Each test prints `PASS n FAIL m` and exits non-zero on a failure. On Windows with MSYS2, set `TMP` and `TEMP` to a folder you can write to, or gcc stops with "Cannot create temporary file in C:\WINDOWS\".

Run on 2026-11-25, host only (gcc, MSYS2 mingw64), both paths:

| Test | Plain C | SH4ZAM |
|---|---|---|
| recipe_cel_test | PASS 20 FAIL 0 | PASS 20 FAIL 0 |
| recipe_dither_test | PASS 23 FAIL 0 | PASS 23 FAIL 0 |
| recipe_fogpulse_test | PASS 40 FAIL 0 | PASS 40 FAIL 0 |
| recipe_softplane_test | PASS 6 FAIL 0 | PASS 6 FAIL 0 |

## What the models say

These are host results: ESTIMATE for the console until a page is photographed. The console counts they are checked against are MEASURED (one console, one run, 2026-11-25).

**R1: the ramp, not the mesh, limits a cel band.**

- The PVR's perspective-correct u equals the world-space interpolation of the vertices' u to 6e-16. A screen-space interpolation would be up to 0.63 off on the test triangle. So a band edge is a straight line on the triangle in the world.
- On a lit sphere, the share of samples in the wrong band from the mesh alone falls from 0.91 % (8 rings) to 0.014 % (64 rings).
- With a 64-texel ramp the total stays about 0.4 % from 16 rings up: half a texel (1/128 of N·L) is the floor.
- A ramp built for one texel-centre convention and read under the other moves an edge by up to a whole texel (0.0625 in N·L for 8 texels). The console page should check which convention the PVR uses.

**R3: the console has already ruled out 6 of 10 rounding rules.**

- c4 test 6 (vertex alpha 128 drew 2,048) rules out every rule that widens a 4-bit alpha by a shift (k << 4), and the plain (t a) >> 8 product.
- Four rules fit both runs: replicate (k x 17) with VTX256, BOTH256, ROUND or FLOOR.
- Two console lines split all four:
  - **vertex alpha 40 at reference 16**: 9 or 10 nibbles drawn;
  - **vertex alpha 174 at reference 128**: 4 or 5 nibbles drawn.
- The fade table at reference 16 reaches all 16 counts under each of the four rules. Full is 15 of 16 cells, at vertex alpha 233 to 240 by rule.

**R4: a narrow pulse loses brightness between entries.**

- At half width 1 entry, a pulse whose depth falls between two entries peaks at only 153 of 255 (at 20 m); at 2 entries, 204; at 4, 230. A pulse that must stay bright needs a half width of 2 or more, or a depth snapped to whole entries.
- The band at half level spans about 5 to 7 % of its depth at half width 1, about 8 to 12 % at 2, and 16 to 21 % at 4 (scale 256).
- The model matches c3b's fog lines to within one 6-bit step (4 levels) at every sample.

**R5: cut a sprite into 4 rows or more.**

| Rows | Vertices | Worst alpha error | Alpha left on the contact line |
|---|---|---|---|
| 1 | 4 | 0.52 | 0.50 |
| 2 | 6 | 0.30 | 0.26 |
| 4 | 10 | 0.12 | 0.07 |
| 8 | 18 | 0.035 | 0.024 |

**SH4ZAM note (host, software back end, not a console).**

- `shz_ldexpf(x, e)` takes a float exponent and goes through `shz_pow2f`, a fast approximation. With whole exponents it was up to 2.9 % off here. `recipes.h` uses `shz_powif(2, e)` instead, which squares and is exact for a whole power of two.
- `shz_log2f` is the piecewise-linear log2 of the float's bits, offset by 0.0573 to centre its error. That is the same curve the PVR's fog table index follows, so the fog model uses it with the offset taken back out (worst 8e-6 octaves against the exact reading).

Both are worth a note to SH4ZAM's author rather than a change here.
