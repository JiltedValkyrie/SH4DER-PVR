---
title: sh4dow-dc (soft character shadows)
tags: [effect, shadows, dependency]
updated: 2026-11-25
sources: [sh4dow-dc/README.md]
---
# sh4dow-dc: soft shadows on characters

**What you will learn:** what SH4DOW-DC does, what its published numbers are worth, and which of its limits are proven.
**For:** a programmer putting soft shadows on characters, and anyone setting a budget for how many characters cast.

## What it does

SH4DOW-DC is a separate library, included whole in `sh4dow-dc/`. It darkens characters by limb "columns" cast along the light. Each vertex is shaded as colour × (1 − dark × strength). It also has foot cones, and a separate, experimental heightfield sunlight module (`sh4dow-dc/README.md`).

SH4DER-PVR takes its column feather for every effect (see [core](core.md)). Do not copy its code into the effect files.

The library needs no KallistiOS header. A Dreamcast build wants the SH4ZAM include path and `-ffast-math -ffp-contract=fast -mfsrra -mfsca -fno-math-errno`. Elsewhere, define `SH4DOW_DC_NO_SH4ZAM` (sh4dow-dc/README.md:313).

## Call order

See `sh4dow-dc/README.md`: "Quick start" (line 31), "API" (line 84: once, per frame, per bone), and the rules of use (about lines 137-142). Prepare after posing and before the vertex pass.

## Cost

**None of these numbers was measured on our console.**

| Claim | Number | Status |
|---|---|---|
| Shade, before column sorting (one cutscene: six characters, 84 columns, 5,406 vertices, a 33 ms frame) | 14.70 ms; 2.72 us, 544 cycles a vertex | REPORTED: one run on one Dreamcast by the original game's author; no date (sh4dow-dc/README.md:274, 284) |
| Shade, after column lists and a one-FTRV loop | 4.5 ms (3.3 times faster); about 0.83 us, 166 cycles a vertex | REPORTED, as above (line 278) |
| Columns visited per vertex: every column / sorted loop / column lists | 13.3 / 5.2 / 2.6 (1,063,664 vertices) | measured on a PC, not a console (line 276) |
| Packed lists | every column 6,294 us, list 4,273, packed 2,490, packed_n 2,278 | REPORTED (emulator, not evidence): Flycast's pipeline model; it moves 10-15 % with code placement (lines 286-297) |
| Foot cones | a few dozen multiplies, dot products and three reciprocals per foot per frame | as-stated; not measured on hardware (line 270) |
| Drawing and storage | nothing extra; a soft edge costs the same as a hard one | as-stated (line 301) |
| Heightfield | O(width × height); 0 KB of video memory; you hold the height and shadow fields | as-stated (lines 346-358) |

## Recommended limits

- `SH4DOW_COLUMN_MAX` is 32 columns. A frame's state is about 3 KB (line 135). A pack entry is 32 bytes.
- Foot layers: 4 extra cones per foot (line 208). The foot fades from full at 18 units from the camera to none at 20 (line 207).
- **The original's budget is not a proven limit.** The original game used 14 casting limbs per character and the six nearest characters per frame (line 160). That is one game's choice, timed once on one console by its author.
- **"The first N casters within R m" is OPEN.** There is no measured number for N or R on our console. See [recommended limits](../limits/recommended-limits.md) for the bench that would set it. Until then, do not cast shadows from every character in the scene.

## Tuning

See `sh4dow-dc/README.md` "Tuning" (lines 144-158) and the foot tuning (lines 225-240).

## Pitfalls

From `sh4dow-dc/README.md` line 162:

- The shade falls toward the light: the light direction is flipped.
- The whole character darkens: `outer_scale` × the radii is too large.
- Nothing darkens: the direction is flipped, or the units or spaces are wrong.

Also:

- Foot units: in the original game one unit was 3.048 m. Scale with `sh4dow_foot_params_scale` (line 211).
- Heightfield: open boundaries, no blockers off the grid, no overhangs, no penumbra from the sun's disc (line 358). The sun must be above the horizon. Scratch is 2 × max(w, h) floats (lines 354-358).
- Use SH4ZAM as a library; do not copy its files (line 313). Credit is required, and it may not be sold on its own (lines 338-340).

## Open

- Packed lists and foot cones on a console (lines 270, 286).
- The shade cost per vertex on our console, with a date.
- N casters within R m: the bench in [recommended limits](../limits/recommended-limits.md).

## Related

- [core](core.md), [psm](psm.md), [pool](pool.md) (blob shadows), [sss](sss.md) (the heightfield as `visibility`)
- [Licence and credit](../start-here/license-and-credit.md)
