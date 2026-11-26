---
title: "Header RECOMMENDED block: the template"
tags: [limits, practice, headers, agents]
updated: 2026-11-25
sources: [wiki/limits/frame-budget.md, wiki/hardware/ledger-pvr.md, wiki/status/maturity.md, README.md, AGENTS.md]
---
# Header RECOMMENDED block: the template

**What you will learn:** the short block of starting settings that sits at the top of each public header, what goes in each line, and the rules for its numbers.
**For:** whoever writes or updates a header, and any agent that reads a header before it uses one.

Developers choose their own settings. The block gives the place to start, what that start costs on the console, and what is proven about it. An agent that opens a header reads it before the call order.

## The block

It goes in the header's top comment, after the copyright lines and before the long description. Twelve lines at most, plain words.

```
   RECOMMENDED (from the wiki ledger, one console; wiki/limits/header-recommended.md)
   Start with: one target pass cut to its own tiles (cw x ch), timed cut and uncut; TILE_CUT_TRIES = 3
   Cost:       PVR: 3,256 -> 344 us (ao4-h), 3,312 -> 375 us (c4), 5,075 -> 1,425 us (d1-d): MEASURED
               (PVR-156, PVR-092, PVR-205). SH-4 a pass: begin 126 us, end 130 us; the frame 1,869 us faster
               (d1-d): MEASURED (PVR-205). The first form's begin cost 3,930 us (d1-c, PVR-192)
   Status:     the PVR saving PROVEN (four runs, three programs); the frame gain PROVEN in d1 alone (one run,
               held at 60 Hz); ADVISED elsewhere: the street demo lost 3.05 ms with one table of two cut
               (PVR-200). The need = 2 refusal has not run on a console
   Do:         wait for render done after every cut pass; cut only with every tile table found (PVR-200: one of two cut)
   Do not:     cut while the next scene fills during a render; carry on if tile_cut_end returns bad
               (shut the PVR down and pvr_init again)
   Maturity:   ALPHA v0.5
```

## The rules for each line

| Line | What goes in it | Rule |
|---|---|---|
| Start with | The settings a new client starts from: the macro names and their values, a target size, a pool size | Use the macro's full name (`SH4DER_PL4NAR_REACH`, not `REACH`), and write its value exactly as the macro and the README tuning table give it. If one changes, change all three in the same edit. Write `NAME = value`, never a `#define` line: a reader should never mistake the block for code, and a later parser may not strip comments as `tools/check_tuning.py` does. |
| Cost | SH-4 and PVR time for the stated case, or the bytes where the bytes are the cost | Only a number the ledger holds, with its row. A number from a related feature (a full-screen layer for a glow) is an ESTIMATE and says what it is from. No number: "not measured". MEASURED and ESTIMATE never share a number. |
| Status | PROVEN or ADVISED, as [frame budget](frame-budget.md) uses them. The line may split it when the start setting and a cost differ (for example: the mode ADVISED, its cost PROVEN) | PROVEN only where a MEASURED row backs the start settings. A difference worked out from two MEASURED stage means is DERIVED, and so is a per-vertex cost from a count read in the code: say so. Otherwise ADVISED, and name the test that would measure it. An emulator result is never MEASURED. The client rule is decided (demos and suite programs count as clients, [open questions](../open-questions.md) D2): a Maturity line no longer gives a second version for it. |
| Do | One or two practices from [frame budget](frame-budget.md) section 3 that apply to this header | Each with its number and its status. Typical ones: cut a render target pass to its own tiles; give a shadow a maximum distance with a fade; the cheapest version first; limit per-vertex work to the vertices that need it. |
| Do not | One or two things that break the effect or cost the most | Take them from the header's own pitfalls, AGENTS.md's call order or the ledger. |
| Maturity | The part's label and version | Copy it from [maturity](../status/maturity.md); update it at each re-rank. |

Wording follows the ledger's rules: no game or engine names; "we know of no earlier use", never "first"; a hang is never tested on the console, so a hang limit is REPORTED or ADVISED only.

## Keeping it true

- **After every console run**, the re-rank of [maturity](../status/maturity.md) also checks the RECOMMENDED block of each header it moves: the cost line, the status and the maturity line.
- **The tuning tables.** `tools/check_tuning.py` checks README's tuning tables against the headers' numeric `SH4DER_*` macros (DIFFERS, MISSING, STALE, UNREADABLE, each with file:line). `make test` runs it with `--strict` (Makefile:43-44): every numeric macro must be in a table or on its exempt list, or the suite fails. It strips comments before it reads the macros, so it does not read the RECOMMENDED block: the block's values are kept equal to the macros by hand. No `#define` text in a block, for readers and for any future parser.
- **The pilot.** The six pilot blocks (pl4nar, pool, spark, glow, `demo/psm/fakeaf.h`, `demo/psm/ao/tile_cut.h`, moved to `include/sh4der_tilecut.h` on 2026-11-26 (`tile_cut_*` is now `sh4der_tilecut_*`, the test `test/tilecut_test.c`)) are in their headers, as comment-only edits, with the two STATUS lines brought up to date.
- **A header's own STATUS line** must agree with the block. Where a console run has since measured something the STATUS line calls unrun, the STATUS line is updated in the same edit.

## Example

The block for `demo/psm/ao/tile_cut.h` as it was at the pilot (now `include/sh4der_tilecut.h`, whose block has since moved on with PVR-205; the example is kept as written):

```
   RECOMMENDED (from the wiki ledger, one console; wiki/limits/header-recommended.md)
   Start with: one target pass cut to its own tiles (cw x ch), timed cut and uncut; TILE_CUT_TRIES = 3
   Cost:       PVR: 3,256 -> 344 us (ao4-h), 3,312 -> 375 us (c4), 5,111 -> 1,442 us (d1-c): MEASURED
               (PVR-156, PVR-092, PVR-192). SH-4: tile_cut_begin +3,930 us a frame in d1-c, and the frame
               903 us slower: MEASURED (PVR-192)
   Status:     the PVR saving PROVEN (three passes); a frame gain ADVISED: the first form did not pay in d1-c
               (its begin read the whole tables from video memory); this form, tables kept in RAM, has not run
               on a console. To prove: the whole frame cut and uncut, with the begin and end timed apart
   Do:         wait for render done after every cut pass; cut only with every tile table found (PVR-200: one of two cut)
   Do not:     cut while the next scene fills during a render; carry on if tile_cut_end returns bad
               (shut the PVR down and pvr_init again)
   Maturity:   ALPHA v0.5
```

## Car demo headers: done

From section 6 of [frame budget](frame-budget.md); Cortana made the edits (comment only).

- **`demo/pl4nar/src/heat.h`:** its block (from 4cb117d) now gives the PVR layer about 0.3 to 0.5 ms (8 % of the screen, scaled from full-screen translucent bilinear layers, PVR-006; ESTIMATE) and names the test: car-tech stage 11 (stage 1 with the heat on STRONG) against stage 1, and the page's "last copy" line. PROTOTYPE v0.1. **Replaced by version 2** (in the tree, not committed): the block now gives the PVR layer about 0.1 to 0.2 ms (256 × 32 px, under 3 % of the screen, the same scaling from PVR-006; ESTIMATE), TA 8.5 KB, the start values of the smooth wobble (0.7 px up and down, 70 m ahead), and Maturity "PROTOTYPE v0.2", a version the [maturity](../status/maturity.md) scale does not have (PROTOTYPE is v0.1). Its proof line still names car-tech stage 11, which no longer exists since 2a2b3e7; the fly-by stage (heat on, not committed) is the candidate.
- **`include/sh4der_pl4nar.h`:** the Maturity line is now "ALPHA v0.5": the client rule is decided ([open questions](../open-questions.md) D2).

## Related

- [Frame budget](frame-budget.md): the practices the Do lines come from
- [Recommended limits](recommended-limits.md)
- [Maturity](../status/maturity.md)
- [Hardware ledger](../hardware/README.md)
