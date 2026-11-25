---
title: motes (drifting specks)
tags: [effect, motes, particles]
updated: 2026-11-25
sources: [include/sh4der_motes.h, src/sh4der_motes.c, README.md "The motes effect"]
---
# motes: a steady field of drifting specks

**What you will learn:** how the motes store nothing per speck, and how many they draw.
**For:** a programmer adding embers, dust, pollen or ash round the player or a source.

## What it does

A steady field of drifting specks. It is stateless: every value comes from the slot number and the time through an integer hash, so nothing is stored per speck. At most `max_draw` of the brightest are drawn, as clipped round 1-bit sprites (README:1046-1073).

`src/sh4der_motes.c` is the KallistiOS drawing: a punch-through list over a 16 × 16 mask (`init_masked`, `draw_masked`), or untextured opaque (`init_flat`, `draw_flat`).

Version: v0.10, passes its maths tests (motes.h:37).

## Call order

See README:1046-1073 and the per-frame order at motes.h:29-31. `AGENTS.md` has no section of its own.

## Cost

As-stated counts for the default look: 3 emitters, 2 in range (README:1054; motes.h:33-35; motes.c:17). There are no times.

| Item | Cost |
|---|---|
| State | none |
| Collect buffer | 32 B × `max_draw`: 3,072 B at 96, 4,096 B at 128 |
| Parameters | 136 B; an emitter 16 B; two headers 64 B |
| VRAM | 512 B for the mask (optional) |
| TA per frame | 96 sprites = 12.3 KB |
| CPU | one hash set and one position per live slot |

## Recommended limits

- `max_draw` from 1 to 128; the default is 96 (README:1052; `SH4DER_MOTES_DRAW_MAX` 128).
- Slots per emitter at most 256 (motes.h:60).
- On the default look, 40 % of the quads are culled (`cull_k`, README:~1051).

## Tuning

There is no README table. The macros are at motes.h:58-64.

## Pitfalls

- The punch-through list draws with depth write on.
- The pattern repeats after hours (`GEN_WRAP`).

## Related

- [spark](spark.md), [bolt](bolt.md)
