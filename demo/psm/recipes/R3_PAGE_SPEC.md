# R3 page: the punch-through product (spec for coding from c4's `punch_pass`)

For: Senior. Status: spec only, not built, not run.

**Purpose.** Settle ledger row PVR-064 (OPEN): how the PVR rounds "texture alpha x vertex alpha" before the punch-through test.

**What we know.**

| Fact | Status |
|---|---|
| The rule is alpha ≥ reference | MEASURED (PVR-060) |
| The tested alpha is the product of texture alpha and vertex alpha | MEASURED (PVR-064) |
| Blend ONE / ZERO writes the failing pixels too | MEASURED (PVR-061) |

**Candidate rules.** The host model (`recipe_dither_test`, `recipes.h`) has ten. The console counts already filed rule out six:

- c3b 5a, and c4 test 6 (vertex alpha 255 and 128 at reference 128);
- out: every rule that widens a 4-bit alpha by a shift (k << 4), and the plain product (t a) >> 8.

Four remain, all with the 4-bit alpha widened as k x 17. Names as in `recipes.h`:

| Rule | Product before the test |
|---|---|
| VTX256 | (t (a + (a >> 7))) >> 8: the vertex's 255 counts as 256 |
| BOTH256 | ((t + (t >> 7)) (a + (a >> 7))) >> 8 |
| ROUND | (t a + 127) / 255 |
| FLOOR | (t a) / 255 |

Every expected number below is the host model's (ESTIMATE until photographed).

## Code: c4's test 6, changed in three places

Source: `demo/psm/compare/c4_spike.c` `t_punch` and `punch_pass` (lines 800–861).

- **Texture:** c4's `a4_mem`, unchanged. ARGB4444, 256 x 8, non-twiddled, texel `((tu >> 4) << 12) | 0x0fff`: alpha nibble = column / 16, colour white.
- **Header:** `pvr_poly_cxt_txr(&c, PVR_LIST_PT_POLY, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED, 256, 8, a4_mem, PVR_FILTER_NEAREST)`, then `c.gen.culling = PVR_CULLING_NONE`, then `pvr_poly_compile`.
  - Nothing else changed. KallistiOS's own settings for the list (its blend, its texture mode "modulate alpha").
  - **Never ONE / ZERO** (PVR-061).
- **Quad:** `rect(rectv, 0, 0, 256, 128, 1.0f, 0, 0.5f, 1, 0.5f, argb, 0)` with `argb = (a << 24) | 0x00ffffff`. Change 1: a is the line's vertex alpha, where c4 had only 0xff and 0x80.
- **Render:** `punch_pass(&h, rectv, 4u, ref, 256u)`. Change 2: ref is the line's reference, where c4 had only 128.
  - It sets `PVR_PT_ALPHA_REF` before and after the lists, renders, reads the 256 x 128 corner through the uncached frame buffer, and returns the drawn pixels.
- **Change 3, per line:** derive from `scr_px`:
  - `n`: drawn pixels / 2048, the nibbles drawn (16 columns x 128 rows a nibble);
  - `lo`: the lowest drawn column of row 64, divided by 16;
  - `step`: 1 when the drawn pixels are exactly the columns from `lo x 16` to 255 on every row, else 0. A 0 means the result is not a clean step and the line is void.

## Page 1: the deciding lines

Text format, at most 40 characters a line:

    R3 PT PRODUCT  <build tag>
    A a255 r128 n08 lo08 px16384 s1

| Line | a | ref | VTX256 | BOTH256 | ROUND | FLOOR | Role |
|---|---|---|---|---|---|---|---|
| A | 255 | 128 | n 8, lo 8, 16,384 px | same | same | same | repeats c4 (MEASURED 16,384); a different number means the set-up differs from c4: stop and say so |
| B | 128 | 128 | n 1, lo 15, 2,048 px | same | same | same | repeats c4 (MEASURED 2,048); same rule |
| C | 40 | 16 | **n 9**, lo 7, 18,432 px | **n 9**, lo 7, 18,432 | **n 10**, lo 6, 20,480 | **n 10**, lo 6, 20,480 | splits {VTX256, BOTH256} from {ROUND, FLOOR} |
| D | 174 | 128 | **n 4**, lo 12, 8,192 px | **n 5**, lo 11, 10,240 | **n 5**, lo 11, 10,240 | **n 4**, lo 12, 8,192 | splits inside each pair |
| F | 16 | 16 | **n 0**, 0 px | n 1, lo 15, 2,048 | n 1, lo 15, 2,048 | n 1, lo 15, 2,048 | confirms VTX256 alone |
| C2, D2 | as C, D | | | | | | C and D again, same frame order, to show the result repeats within the run |

### How a result names a rule

| C (n) | D (n) | F (n) | Rule |
|---|---|---|---|
| 9 | 4 | 0 | VTX256 |
| 9 | 5 | 1 | BOTH256 |
| 10 | 5 | 1 | ROUND |
| 10 | 4 | 1 | FLOOR |
| anything else | | | **none of the four**: the page prints `NO RULE`; add candidates to `recipes.h` and rerun the host model before deciding anything |

- A, B, C2 and D2 must match A, B, C and D, or the run is void.
- The page prints its verdict as the last line: `RULE VTX256` (or the others, or `NO RULE`), worked out by the image from the table above.

## Page 2: the fade curve (reference 16)

Thirty-three lines `E aNNN nNN` for a = 0, 8, 16 … 248 and 255, over two screens of 17 and 16 lines.

**Model** (nibbles drawn: VTX256 / BOTH256 / ROUND / FLOOR).

- **The rows where the rules differ:**

  | a | VTX256 | BOTH256 | ROUND | FLOOR |
  |---|---|---|---|---|
  | 16 | 0 | 1 | 1 | 1 |
  | 24 | 5 | 6 | 6 | 6 |
  | 40 | 9 | 9 | 10 | 10 |
  | 48 | 10 | 10 | 11 | 11 |
  | 80 | 12 | 12 | 13 | 13 |
  | 120 | 13 | 13 | 14 | 14 |

- **The rows where all four agree:**

  | a | Nibbles drawn |
  |---|---|
  | 0, 8 | 0 |
  | 32 | 8 |
  | 56 | 11 |
  | 64, 72 | 12 |
  | 88 to 112 | 13 |
  | 128 to 232 | 14 |
  | 240 to 255 | 15 |

The whole curve is what a fade by this rule looks like: where it climbs fast (a 16 to 40) and where it stalls (a 128 to 232). The fade table in `recipes.h` is built from the rule page 1 names.

## Page 3: the picture

The same quad, 256 x 64, at a = 16, 24, 40, 64, 120 and 240 (reference 16), stacked. Each row is labelled with its a in text on the left.

## Cost (ESTIMATE, from MEASURED rows)

| Part | Cost | Built from |
|---|---|---|
| PVR, one line | about 3.6 ms | an empty 640 x 480 frame 3,150 us (PVR-001) + a full-screen punch-through 4444 layer 4,303 us (PVR-006), scaled to the quad's 10.7 % of the screen |
| Read-back, one line | about 18 ms | 64 KB through the uncached 16-bit loop at 3.7 MB/s (h2, BUS / SH-4 ledger) |
| Pages 1 and 2 | about 41 renders, under 1 s in all | the two rows above |
| TA | 4 vertices a render, nothing measurable | 142 ns each (BUS-003) |
| Video memory | nothing new | c4's 4 KB `a4_mem` |

## Ledger write-up after the run (template)

| Claim | Status | Photo | Test |
|---|---|---|---|
| The punch-through product rounds as <RULE>: line C n = …, D n = …, F n = … | MEASURED <date>, 1 console, 1 run | <photo names> | R3 page 1 lines C, D, F |
| The fade curve at reference 16 | MEASURED, as above | <photo> | R3 page 2 |

Then:

- PVR-064 moves from "rounding OPEN" to the rule found.
- `recipes.h` keeps only that rule as the default.
- `wiki/open-questions.md` Q16's rounding part closes.

If `NO RULE`, PVR-064 stays OPEN, and the photo's counts are filed as MEASURED with the candidates they rule out.

## Where it goes

- Either as test 6b of the next c4 image (c4b), which keeps one image for the punch-through work, or in a new image tagged `psm-r1-a`.
- The `.cdi` goes in `demo/psm/out/cdi`, never over an existing file.
