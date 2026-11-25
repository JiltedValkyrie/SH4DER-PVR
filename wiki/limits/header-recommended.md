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
   Start with: <the settings to start from: macro names and values, a size, a count>
   Cost:       SH-4 <x> ms, PVR <y> ms at <the case>: MEASURED (<ledger row>)
               or: ESTIMATE (<what it is based on>) / not measured
   Status:     PROVEN | ADVISED; to prove: <the test or stage that would measure it>
   Do:         <a practice from wiki/limits/frame-budget.md, with its number>
               <a second practice, if there is one>
   Do not:     <one or two things that break it or cost the most>
   Maturity:   <label and version from wiki/status/maturity.md>
```

## The rules for each line

| Line | What goes in it | Rule |
|---|---|---|
| Start with | The settings a new client starts from: the macro names and their values, a target size, a pool size | Use the macro's full name (`SH4DER_PL4NAR_REACH`, not `REACH`), and write its value exactly as the macro and the README tuning table give it. If one changes, change all three in the same edit. Write `NAME = value`, never a `#define` line: `tools/check_tuning.py` reads any line that starts with `#define`, even inside a comment. |
| Cost | SH-4 and PVR time for the stated case, or the bytes where the bytes are the cost | Only a number the ledger holds, with its row. A number from a related feature (a full-screen layer for a glow) is an ESTIMATE and says what it is from. No number: "not measured". MEASURED and ESTIMATE never share a number. |
| Status | PROVEN or ADVISED, as [frame budget](frame-budget.md) uses them | PROVEN only where a MEASURED row backs the start settings. A difference worked out from two MEASURED stage means is DERIVED, and so is a per-vertex cost from a count read in the code: say so. Otherwise ADVISED, and name the test that would measure it. An emulator result is never MEASURED. A label that depends on an open owner's call (the client rule in maturity) says so. |
| Do | One or two practices from [frame budget](frame-budget.md) section 3 that apply to this header | Each with its number and its status. Typical ones: cut a render target pass to its own tiles; give a shadow a maximum distance with a fade; the cheapest version first; limit per-vertex work to the vertices that need it. |
| Do not | One or two things that break the effect or cost the most | Take them from the header's own pitfalls, AGENTS.md's call order or the ledger. |
| Maturity | The part's label and version | Copy it from [maturity](../status/maturity.md); update it at each re-rank. |

Wording follows the ledger's rules: no game or engine names; "we know of no earlier use", never "first"; a hang is never tested on the console, so a hang limit is REPORTED or ADVISED only.

## Keeping it true

- **After every console run**, the re-rank of [maturity](../status/maturity.md) also checks the RECOMMENDED block of each header it moves: the cost line, the status and the maturity line.
- **The tuning tables.** `tools/check_tuning.py` checks README's tuning tables against the headers' numeric `SH4DER_*` macros (DIFFERS, MISSING, STALE, UNREADABLE, each with file:line). No make target or test runs it yet; it is to join `make test` (README:1267 and AGENTS.md:86 say no test checks the tables yet). It does not read the RECOMMENDED block, so the block's values are kept equal to the macros by hand. The block must hold no line that starts with `#define`: the checker would take it for a macro.
- **A header's own STATUS line** must agree with the block. Where a console run has since measured something the STATUS line calls unrun, the STATUS line is updated in the same edit.

## Example

The block for `demo/psm/ao/tile_cut.h`:

```
   RECOMMENDED (from the wiki ledger, one console; wiki/limits/header-recommended.md)
   Start with: every render target pass, cut to its own tiles (cw x ch from the target's size); TILE_CUT_TRIES = 3
   Cost:       target pass 3,256 -> 344 us of PVR, the same texel sum (ao4-h, 128 x 128, 20 tiles): MEASURED
               (PVR-156); 3,312 -> 375 us (c4's map, once): MEASURED (PVR-092). SH-4: not measured
   Status:     PROVEN for those two passes; ADVISED for others (mirror, depth map: Q97). To prove: the
               pass's PVR time cut and uncut, and the texel sum of both
   Do:         wait for render done after every cut pass (the table must not change under a render)
   Do not:     cut while the next scene fills during a render; carry on if tile_cut_end returns bad
               (shut the PVR down and pvr_init again)
   Maturity:   ALPHA v0.4 (wiki/status/maturity.md)
```

## Related

- [Frame budget](frame-budget.md): the practices the Do lines come from
- [Recommended limits](recommended-limits.md)
- [Maturity](../status/maturity.md)
- [Hardware ledger](../hardware/README.md)
