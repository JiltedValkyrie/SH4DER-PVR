---
title: "tilecut (a render target through its own tiles)"
tags: [effect, tilecut, pvr, render-target]
updated: 2026-11-26
sources: [include/sh4der_tilecut.h, test/tilecut_test.c, README.md "The tilecut pass", AGENTS.md Files, wiki/hardware/ledger-pvr.md]
---
# tilecut: a render target through its own tiles

**What you will learn:** why a small render target costs the PVR as much as a whole screen under KallistiOS, how the cut ends the tile table after the target's own tiles for one pass, what that saved on a console, and the rules that keep it safe.
**For:** a programmer with a render target pass (a shadow tile, a mirror, a small map) that is smaller than the screen.

## What it does

KallistiOS renders a render target through the screen's whole tile table: 20 x 15 tiles whatever the target's size, and the PVR's ISP visits every one, about 3.2 ms a pass (PVR-004). `sh4der_tilecut_begin(&cut, cw, ch)` makes each tile table end after the target's cw x ch tiles; `sh4der_tilecut_end` writes every changed word back. Only video memory is written: the header keeps a RAM copy of each table from the moment `sh4der_tilecut_find` takes it. `sh4der_tilecut_begin_cols(&cut, cw)` is the cheapest form: the first cw whole columns, one word written a table.

Header only (`include/sh4der_tilecut.h`); `sh4der_tilecut_find` reads the PVR's region base and is Dreamcast only. Moved from `demo/psm/ao/tile_cut.h` on 2026-11-26 (names `tile_cut_*` became `sh4der_tilecut_*`); the ledger rows before the move name the old path.

Version: ALPHA v0.5 ([maturity](../status/maturity.md)).

## Call order

1. At load: `sh4der_tilecut_init(&cut, 20, 15)` (the screen's tiles; `cut.need` = 2 tables, as KallistiOS has). A program that knows it has one table sets `need` to 1 after the init.
2. After a scene's render, until `cut.n == cut.need`: `sh4der_tilecut_find(&cut)`.
3. Round each target pass: `if (sh4der_tilecut_begin(&cut, cw, ch)) { ... the target's scene, its render, the wait for render done ...; bad = sh4der_tilecut_end(&cut); }`. A 0 from begin means refused, nothing written: the pass runs uncut.
4. If `sh4der_tilecut_end` returns other than 0, the next screen frame would be a corner of the picture: `pvr_shutdown`, then `pvr_init` (it builds the tables anew).

## Cost

| Item | Cost | Status |
|---|---|---|
| PVR, a 20-tile target | 3,312 -> 375 us (c4, PVR-092) | MEASURED, one console |
| PVR, ao4-h's target M | 3,256 -> 344 us (PVR-156) | MEASURED, one console |
| PVR, d1's map, 10 x 8 tiles | 5,075 -> 1,425 us (PVR-205) | MEASURED, one console |
| SH-4 a pass, this form | begin 126 us, end 130 us (PVR-205) | MEASURED, one console |
| SH-4 a pass, columns form | begin 5 us, end 3 us; d1's map 5,075 -> 2,756 us (PVR-205) | MEASURED, one console |
| The frame, d1 | 1,869 us shorter, back to 60 Hz (PVR-205) | MEASURED, one run |
| The first form (both tables read every cut) | begin 3,930 us, more than the PVR time it saved (PVR-192) | MEASURED; that form is gone |
| RAM | 14,400 B in the struct (two tables' copies) | FACT (the header's arrays) |
| VRAM | 0 (KallistiOS's tables rewritten in place) | FACT |

## Recommended limits

- Start with one target pass cut to its own tiles, timed cut and uncut; `SH4DER_TILECUT_TRIES` = 3 (the header's RECOMMENDED block).
- Cut only with every table found. With one of two found, the street demo's mirror pass ran uncut every second frame and the frame was 3.05 ms slower (PVR-200). The refusal that enforces this has not run on a console: ADVISED.
- At most `SH4DER_TILECUT_MAX` (96) tiles in a cut.

## Tuning

See `README.md` "Tilecut tuning": `SH4DER_TILECUT_TABLES`, `_MAX`, `_TILES`, `_TRIES`, `_VERIFY`.

## Pitfalls

- No render that reads the table may run while it is cut. A program that waits for render done after every target pass holds this; one that fills the next scene while the last still renders does not, and that would want a change inside KallistiOS, which this header is not.
- The table layout is read from KallistiOS's `pvr_buffers.c` at one commit; check it against the current master before relying on a newer KallistiOS (the header's THE TABLE note).
- The TA does not read the table, only the ISP: a scene being filled is not disturbed.

## Open

- The refusal when not every table is found, on a console.
- The mirror pass in the street demo: the cut with both tables found (PVR-200 had one).
- Whether the mirror pass walks the screen's tile table at all (Q97, X43).

## Related

- pvrx (`include/sh4der_pvrx.h`, no page yet): the other layer over KallistiOS's PVR driver
- [pl4nar](pl4nar.md), [psm](psm.md): render target passes it can cut
- [Frame budget](../limits/frame-budget.md)
- [Header RECOMMENDED block](../limits/header-recommended.md)
- [Ledger: PVR](../hardware/ledger-pvr.md): PVR-092, PVR-156, PVR-192, PVR-200, PVR-205
