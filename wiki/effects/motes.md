---
title: Drifting specks [motes] (a steady field of dust, embers and pollen)
tags: [effect, motes, particles]
updated: 2026-11-26
sources: [include/sh4der_motes.h, src/sh4der_motes.c, README.md "The motes effect"]
---
# Drifting specks [motes]: a steady field of dust, embers and pollen

**What you will learn:** how the motes store nothing per speck, and how many they draw.
**For:** a programmer adding embers, dust, pollen or ash round the player or a source.

> **In plain words.** A steady field of tiny hard-edged dots, such as embers, dust, pollen or ash, that drift, fade and come back. The program works out each dot's place from the time and a number, so it stores nothing for each dot. The viewer sees small specks drifting round a source. Use it for embers round a fire, dust in a beam of light or pollen in the air.

## Try this first

What it is for: a steady field of tiny dots near a source, with no memory kept for each dot.

1. Start from `sh4der_motes_params_default` (include/sh4der_motes.h:113-127, ADVISED): the embers look, with the header's own values.
2. `max_draw` = 96 (include/sh4der_motes.h:125, ADVISED). Lower it to draw fewer dots a frame. The most one call can hold is `SH4DER_MOTES_DRAW_MAX` = 128 (include/sh4der_motes.h:61, ADVISED).
3. `rate` = 30 and `life_hi` = 7.0 (include/sh4der_motes.h:118, ADVISED) give 30 × 7.0 = 210 slots an emitter, as README 'The motes effect' says. The limit is `SH4DER_MOTES_SLOTS_MAX` = 256 (include/sh4der_motes.h:60, ADVISED); `sh4der_motes_slots` returns 0 above it (include/sh4der_motes.h:137).
4. `seed`: give each emitter its own, 1000 apart from the next (include/sh4der_motes.h:92, ADVISED).
5. `lod_cut` = 3500.0 world units: an emitter further from the eye than this is not drawn (include/sh4der_motes.h:123, ADVISED).

Run on a console by the owner in Match Light (her test; no measured numbers recorded yet; released in the public Match Light film). This page says: 'There are no times.'

Header: [sh4der_motes.h](../../include/sh4der_motes.h). Call order: [Call order](#call-order), below.

## What it does

A steady field of drifting specks. It is stateless: every value comes from the slot number and the time through an integer hash, so nothing is stored per speck. At most `max_draw` of the brightest are drawn, as clipped round 1-bit sprites (README:1046-1073).

`src/sh4der_motes.c` is the KallistiOS drawing: a punch-through list over a 16 × 16 mask (`init_masked`, `draw_masked`), or untextured opaque (`init_flat`, `draw_flat`).

Version: v0.10, passes its maths tests (motes.h:37).

## Call order

See the README section 'The motes effect' and the per-frame order at motes.h:29-31. `AGENTS.md` has no section of its own.

## Cost

As-stated counts for the default look: 3 emitters, 2 in range (README 'The motes effect', cost line at README:1245; motes.h:33-35; motes.c:17). There are no times.

| Item | Cost |
|---|---|
| State | none |
| Collect buffer | 32 B × `max_draw`: 3,072 B at 96, 4,096 B at 128 |
| Parameters | 136 B; an emitter 16 B; two headers 64 B |
| VRAM | 512 B for the mask (optional) |
| TA per frame | 96 sprites = 12.3 KB |
| CPU | one hash set and one position per live slot |

## Recommended limits

- `max_draw` from 1 to 128; the default is 96 (the cost line at README:1245 uses 96; `SH4DER_MOTES_DRAW_MAX` 128, README:1271).
- Slots per emitter at most 256 (motes.h:60).
- On the default look, 40 % of the quads are culled (`cull_k`, README 'The motes effect').

## Tuning

There is no README table. The macros are at motes.h:58-64.

## Pitfalls

- The punch-through list draws with depth write on.
- The pattern repeats after hours (`GEN_WRAP`).

## Related

- [Sparks and tracers](spark.md), [Lightning](bolt.md)
