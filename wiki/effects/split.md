---
title: "Split screen [split]: a movable divider for a left and a right half"
tags: [effect, split, clip, divider]
updated: 2026-11-26
sources: [include/sh4der_split.h, test/split_test.c, README.md "The split screen divider", wiki/status/maturity.md "split divider", wiki/hardware/ledger-pvr.md PVR-144, PVR-161]
---
# Split screen [split]: a movable divider for a left and a right half

**What you will learn:** how a screen is cut into two halves at a divider the triggers move, in 32-pixel tile columns, and which clip each half gets.
**For:** a programmer making a side-by-side comparison (one look on the left, another on the right) who wants the divider to move under the player's hands.

## What it does

The PVR's user clip works in tiles of 32 pixels. On a 640 x 480 screen that is 20 columns and 15 rows, so the divider has 21 places, `d = 0` to `20`. The left half is tile columns `0 .. d - 1`; the right half is `d .. 19`. At `d = 0` the left half is hidden; at `d = 20` the right half is hidden. A hidden half has no clip rectangle, so the program sends nothing for it.

The header is maths and held-trigger stepping only. It sends nothing. The program sets each half's clip, and sends that half's look.

Header only, plain C. There is no maths library in it, so there is no SH4ZAM call.

Version: ALPHA v0.2 (`include/sh4der_split.h`, RECOMMENDED block at the top).

## Call order

1. Each frame, step the divider: `sh4der_split_step(&step, d, SH4DER_SPLIT_COLS, lt_down, rt_down)`. A press steps once; held, it repeats every `SH4DER_SPLIT_REPEAT` (6) frames. Both triggers down, or neither: no move.
2. `sh4der_split_halves(d, SH4DER_SPLIT_COLS, SH4DER_SPLIT_ROWS, &left, &right)`.
3. For each half that is not hidden, set its user clip and send that half's look.
4. Draw the dividing line at `sh4der_split_x(d)`, unless `sh4der_split_at_end(d, ...)`.
5. Put each side's label at `sh4der_split_label_x(&half, width)`. It is centred on its half and kept inside the title-safe area (64 to 576 pixels); `-1` means no room.

## Cost

| Item | Cost | Status |
|---|---|---|
| CPU | integer compares per frame | not measured |
| RAM | two `sh4der_split_half_t` and one `sh4der_split_step_t` on the stack or in the program | by size, as-stated |
| PVR | the cost of the two halves' own sends; a hidden half is not sent | not measured |
| VRAM | 0 | |

## Recommended limits

- Start at 20 columns, 15 rows, a repeat of 6 frames. These are the values in the header.
- Send a hidden half not at all, and never give it a clip rectangle: a rectangle cannot be empty.
- Set both halves' clips every frame. Put the dummy polygon before each change of clip, as the core takes a new clip one polygon early (the header's note).

## Evidence

- The middle position works on a console: the split's user clip, with the divider drawn, the layer on the left half only (PVR-161; PVR-144 for the user-clip split running in every scene). MEASURED, one run each.
- A divider that moves has no console run. The status line in `wiki/status/maturity.md` is OPEN for it (Q95). Until a run moves the divider, the stepping is proven by the host test only.

## Test

`test/split_test.c` checks every place of the divider: the two halves cover each tile column once and no column twice, a hidden half is flagged, the rectangles are valid, the trigger stepping (press, repeat, both triggers, the ends), and the label places. It prints `split_test: ok`.

Run it with `make test` (plain C) or `make test-shz` (the SH4ZAM path; the header does not call SH4ZAM, so both agree).
