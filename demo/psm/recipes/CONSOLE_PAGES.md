# demo/psm/recipes: console pages for R3, R7 and R8 (spec, not built)

Specification of the console test pages that would show recipes R3, R7 and R8 of `demo/psm/RECIPES_NOTES.md`. Nothing here is built. Every expected number comes from a host model and is an ESTIMATE until a page is photographed. The console numbers it builds on are MEASURED (one console, one run, 2026-11-25).

## Rules for every page

These follow the c4 image: same KallistiOS set-up, same `say` text, results one line a fact.

- **Screen.** 640 x 480, 565, dither off for the drawing passes. Text in KallistiOS's 12 x 24 font, white on black, at most 40 characters a line and 16 lines a page. That is large enough to read from a phone photo of a TV.
- **Line order.** Each page's first line is its name and build tag, for example `R7 WATERLINE  psm-r1-a`. Then one line a result: name, console number, model number. The pictures, if any, go in the lower half of the screen, under the text.
- **No timing on these pages.** Times go on a separate page, with `pvr_get_stats` and the KallistiOS ns timer, as c4 test 5 does.
- **Navigation.** The page waits for START, then goes on. A page that fails to render prints `NO RENDER END` and goes on (as c4 does).
- **Reading back.** Counts are read from the frame buffer through its uncached address, 16-bit reads, as `c4_spike.c` `punch_pass` does. Count only the test rectangle, not the text.
- **Models.** Every expected value is computed by the same code as `recipes.h` and the c4 models, compiled into the image. The page prints the model next to the console number. The photo then carries both.

## Page R3: the punch-through product (prerequisite for R8; settles PVR-064)

**Picture.** The c4 test 6 ramp: a 256 x 8 ARGB4444 texture, alpha nibble = x / 16, white. Drawn as a 256 x 128 quad on `PVR_LIST_PT_POLY` with KallistiOS's own header for the list (not ONE / ZERO, PVR-061), `PVR_FILTER_NEAREST`, `txr.env = PVR_TXRENV_MODULATEALPHA`.

**What a line counts.** Each line is one render at a given vertex alpha and reference. The page reports the number of 16-column nibble groups drawn (drawn pixels / 2,048) and the lowest nibble drawn.

| Line | Vertex alpha | Ref | Model, by rule still standing (nibbles drawn) | Why |
|---|---|---|---|---|
| R3 A | 255 | 128 | 8 for all four | repeat of c4 (16,384 px) |
| R3 B | 128 | 128 | 1 for all four | repeat of c4 (2,048 px) |
| R3 C | 40 | 16 | VTX256 9, BOTH256 9, ROUND 10, FLOOR 10 | splits {VTX256, BOTH256} from {ROUND, FLOOR} |
| R3 D | 174 | 128 | VTX256 4, BOTH256 5, ROUND 5, FLOOR 4 | splits within each pair |
| R3 E | 0 to 255 in steps of 8 | 16 | the fade table of each rule (`recipe_dither_test` prints them) | the fade curve itself; 32 short lines over two pages |

**Reading the result.** Lines C and D together name one rule: (9, 4) VTX256, (9, 5) BOTH256, (10, 5) ROUND, (10, 4) FLOOR. Any other pair means the rule is none of the four. Add more candidates to `recipes.h` then, rather than forcing a fit.

**Photograph.** The text, one photo. Then line E's picture: a 4 x 4 dither tile repeated over a 256 x 64 strip at each of 8 vertex alphas, stacked.

## Page R7: waterline by the bump comparator in texture space

**Map.** A 128 x 128 bump texture (twiddled; PVR-070 showed non-twiddled works too). Its compare byte M rises with x: M = 16 + (223 x) / 127, so it runs from 16 to 239 and stays away from the wrap. The other byte is set as c4's compare map sets it (`c4_spike.c`). A vertical window line then falls at a known x for each Q.

**Picture 1, the waterline alone.**

- A 256 x 256 quad (the map at 2 x, nearest) on `PVR_LIST_PT_POLY`.
- `PVR_TXRFMT_BUMP`, `gen.specular = 1`, `oargb = K1 << 24 | K3 << 8 | Q` with K1 128, K3 127 (c4's `KQ`).
- KallistiOS's blend for the list. `PVR_PT_ALPHA_REF` = the threshold for window half-width w.
- The thresholds come from c4's `c4_threshold` table now. Once c4b exists, they come from the window edges measured at boot (Q56).

**Picture 2, coplanar.** The same, drawn over an opaque copy of the quad at equal depth (a grey 565 base). This shows whether the coplanar punch-through pass survives the depth test in that list (PVR-062).

**Lines.**

| Line | Console | Model |
|---|---|---|
| `R7 Q128 W16 DRAWN n` | pixels drawn in picture 1 | pixels with `c4_level[(M - Q) & 255] >= thr` |
| `R7 Q64 W16`, `R7 Q192 W16` | the same at two more levels | the same |
| `R7 Q128 W8`, `R7 Q128 W32` | the same at two more widths | the same |
| `R7 Q128 W16 LEFT x RIGHT x` | the first and last drawn column of row 64 | the model's two edge columns |
| `R7 COPLANAR DRAWN n ALONE n0` | picture 2 against picture 1 | n = n0 if the depth test passes at equal depth |
| `R7 RISE` | Q stepped 16 .. 239 in steps of 8, 28 renders: the left edge column each time | a straight line in Q; prints the worst step off it |

**What it settles.**

- Whether a texture-space bump compare is exact with one Q for the whole polygon. The rows should all match, with no bands.
- How far the window edges sit from the cosine table at this Q (expect a few codes, PVR-096).
- Whether the coplanar punch-through pass draws.

**Photograph.** The text, then the two pictures side by side, with the model's edge columns marked by a 1-pixel red tick above the quad. The photo then shows the overlap by eye.

## Page R8: dissolve with a burning rim

Needs page R3 first: the body's edge depends on the product's rounding.

**Textures.**

- **Body:** 64 x 64 ARGB4444. Alpha nibble = a fixed hash noise 1..15 (the R3 rule cannot draw nibble 0); colour orange.
- **Rim:** a 64 x 64 bump texture holding the same noise as its compare byte, M = nibble x 8 (codes 8 .. 120). That keeps it inside half a turn, so the window acts one-sided.

**Picture.** Four 128 x 128 quads side by side, at dissolve vertex alpha a = 192, 96, 48, 24. On each:

1. the body on `PVR_LIST_PT_POLY` with the R3 settings, reference 128;
2. then the rim, coplanar, on the same list: `PVR_TXRFMT_BUMP`, Q = 8 x (the lowest nibble the body draws at that a), half-width 8 codes, white.

**Lines, one a quad.**

| Line | Console | Model |
|---|---|---|
| `R8 A192 BODY n` | body pixels drawn (rim off, separate render) | `recipes.h` with the rule page R3 named |
| `R8 A192 RIM n` | rim pixels drawn (body off) | the bump window model |
| `R8 A192 GAP n` | body-edge pixels with no rim pixel within 1 | 0 if the two rules meet |
| `R8 A192 OVER n` | rim pixels inside the body's cut-out area | 0 |

The same four lines are printed for a = 96, 48 and 24.

**What it settles.**

- Whether the punch-through product (body) and the bump window (rim) agree on where the edge is.
- How wide the rim reads on a console.

If GAP is not 0, the remedy is a wider rim window, not a change to the body.

**Photograph.** The text, then the four quads (the dissolve read left to right).

## Build notes

- One image could hold all three pages: build tag `psm-r1-<letter>`, from the c4 image's frame (scene open and close, the `say` text, the fault beat, `punch_pass`).
- The `.cdi` goes in `demo/psm/out/cdi`, never over an existing file.
- Not built. Lead decides whether it is.
