# A target pass through a tile list its own size: scope of the spike

Written 2026-11-25 by Senior for lead. **No code yet; nothing here is new on a console.** MEASURED rows are c4's (one console, 2026-11-25). KallistiOS lines are from the checkout the images are built with: commit 568c3404 (2026-09-27), clean, origin KallistiOS/KallistiOS. I could not read the upstream master of today from here: the lines want a check against it before anything is proposed upstream.

## What costs the 3.2 ms

A render target pass renders through the screen's whole tile table: 20 × 15 = 300 tiles, whatever the target's size. The ISP visits each, draws the background plane into it and writes it out; the pixel clip only throws the pixels away. That is the floor of about 3.2 ms a pass (PVR-004). c4 ended the table after the target's own 20 tiles and the same pass took 375 us in place of 3,312 (PVR-092), the picture the same, and every word restored.

## Where KallistiOS does it

| Step | Where | What |
|---|---|---|
| The table is built, once, at `pvr_init` | `pvr_buffers.c:39` `pvr_init_tile_matrix`, called for both TA buffers at `:131`; allocated at `:293` (6 + 6 × tw × th words each) | A first entry (10000000, five 80000000), then six words a tile, **column by column** (x outer, y inner): a control word `(y << 8) \| (x << 2)` plus the presort bit, and five list pointers (80000000: no list). The last entry has bit 31. |
| A target pass is asked for | `pvr_scene.c:151` `pvr_scene_begin_rtt` | Only notes the size, the pitch and the address. |
| The render starts | `pvr_irq.c:79` `pvr_render_lists`, in the interrupt after the last list, calls `pvr_misc.c:177` `pvr_begin_queued_render` | Writes `PVR_ISP_TILEMAT_ADDR` (`:228`, the region array's base) and `PVR_ISP_VERTBUF_ADDR` (`:229`, the parameter base), the render address, the pixel clip to the target's size (`:246`), then `PVR_ISP_START`. The table is the same one for the screen and for a target. |

So there is no hook between "the table's address is written" and "go": both are in one function in the interrupt.

## Can it be done from outside KallistiOS? Yes, the way c4 did it

No KallistiOS patch is needed. Our own function (in the new `sh4der_pvrx`) does this round a target pass:

1. **Once:** find the two tables (read `PVR_ISP_TILEMAT_ADDR` after two scenes, as c4 did: `pvr_state` is not public), check each is laid out as above (c4's `tm_valid`: refuse and fall back to the plain pass if not), and save the entries that will be overwritten.
2. **Before the pass, with no render running:** write the target's tiles as the table's first w × h entries (their own control words and list pointers, copied; bit 31 on the last). For target M (128 × 130) that is 4 × 5 = 20 entries, 120 words a table.
3. **After the pass's render-done:** write the saved entries back and read them back.

Only the first w × h entries change (the table's tail after the end mark is not read), so the cost is about 240 word writes to video memory a table a pass, both ways: tens of microseconds (ESTIMATE). c4 rewrote and restored the whole 1,806 words once; the per-frame form is the new part.

**The condition:** the table must not be rewritten while a render that uses it is running. Our spikes wait for render-done after every pass (polled), so it holds. A game that fills the next scene while the last still renders must only touch the table of the TA buffer the target pass will use, and restore it before that buffer's next screen render; that needs the buffer's number, which is KallistiOS's private state. For such a pipeline the clean form is a small KallistiOS change instead: a short table a TA buffer, built at `pvr_scene_begin_rtt`, and `pvr_begin_queued_render` pointing `PVR_ISP_TILEMAT_ADDR` at it when `curr_to_texture` is set (about 20 lines in `pvr_misc.c` and `pvr_buffers.c`). That is a proposal to offer upstream, not a patch for us to carry.

## Risks

| Risk | What it looks like | Guard |
|---|---|---|
| The end mark is lost or an entry is wrong | The ISP walks on into other memory: a wrong picture or no render-done | The layout check before any write; bounded waits; the fault catcher; the saved words written back in any case |
| A word does not restore | The next screen frame is a corner of the picture | Read back after the restore, three tries (c4); if it still differs, `pvr_shutdown` and `pvr_init` build the tables anew. A power cycle fixes everything: only video memory is written. |
| A KallistiOS update changes the table | The check refuses; the pass runs uncut | The check is the gate; no version test |
| The rewrite races a render | Corrupt frame | Only between a render-done and the next scene's finish; see the condition above |
| A target not on the tile grid | The last row of tiles is partly outside the pixel clip (130 rows: 5 tile rows) | As c4's 128 × 130 map: measured fine |

`PVR_ISP_VERTBUF_ADDR` (the parameter base), the TA's own registers and the object lists are not touched: the TA does not read the region array, only the ISP does. Nothing of SB_ISP or the ASIC is involved.

## What to expect

| Target | Tiles | Uncut | Cut | Status |
|---|---|---|---|---|
| c4's map, 128 × 130 | 20 | 3,312 us | 375 us | MEASURED (PVR-092) |
| 3+M's target M, 128 × 130, 160 small triangles | 20 | about 3.2 ms | about 0.3 to 0.4 ms: **about 2.9 ms saved a pass** | ESTIMATE (PVR-093; M has less in it than c4's map) |
| A 256 × 258 shadow target (ao4's S and A, route A) | 72 | 3.2 ms + its fill | about 2.4 ms less | ESTIMATE (228 of 300 tiles left out) |

A 4-tile M (one tile a hen, the tiles packed) would be the limit: 4 of 300 tiles.

## The A / B test for the suite

In ao4's scene C, the stage 3+M | 3+M run twice more: target M's pass uncut, then cut. A page line each:
- the PVR's render time of the M pass (and of target S, cut the same way, as a second size);
- a checksum of target M's texels read back after each, **equal or not** (the picture must not change);
- words that did not restore (0 expected), and the SH-4's cycles for the rewrite and the restore;
- if the layout check refuses: "tile table not as read: stage left out".

As in c4, the stage runs **last** in the entry, after the photographs' numbers are safe, and its result line says so. One console is enough to call the saving MEASURED for that target; the per-frame rewrite running for some seconds without a wrong frame is the new evidence.
