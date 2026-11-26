---
title: timebase (a video tape's bent picture)
tags: [effect, timebase, tape, frame-texture]
updated: 2026-11-26
sources: [include/sh4der_timebase.h, include/sh4der_ca.h, src/sh4der_ca.c, README.md effect table, test/timebase_test.c]
---
# timebase: a video tape's bent picture

**What you will learn:** what the timebase effect works out, how the picture is bent with it, and what it costs.
**For:** a programmer who wants a tape look, or who wants to move parts of the finished picture sideways for another reason.

## What it does

It works out, for one displayed frame, how far each row of the picture sits sideways when the picture is played from a worn
video tape (README effect table):

- the top rows hook sideways (a tape under the wrong tension);
- the rows weave (the drum's flutter);
- the bottom rows jump (the head switching step);
- a television's horizontal lock follows the slow errors and shows the sudden ones.

The result is one shift in pixels for every row. It draws nothing by itself.

The model is a port to C of parts of NTSCRT by Finn McKenty (MIT License). His notice is in the header and stays there. A
program that uses the module credits NTSCRT and its author.

Header only. The header's version line reads "v0.1" (`include/sh4der_timebase.h:99`).

## How the picture is bent

1. The world is drawn into a frame texture ([ca](ca.md): `sh4der_ca_begin`).
2. `sh4der_timebase_field` fills a table: one shift a row.
3. The frame texture is put on the screen as horizontal bands with `sh4der_ca_channel_rows`. Each mesh row keeps its height and
   is moved sideways by its shift. Two rows at the same height are a break: a step in the picture.
4. The picture is drawn a few pixels wider than the screen (the overscan), so the bend uncovers nothing at the edge.
5. Text and a HUD are drawn after, on the screen, and stay straight.

The same band call serves any effect that moves rows of the finished picture sideways, with any table of shifts.

## Cost

| Item | Cost | Status |
|---|---|---|
| RAM | 40 B parameter block, 4 B state | as-stated (README effect table) |
| Stack | about 4.2 KB inside `sh4der_timebase_field` | as-stated (header) |
| VRAM | 0 of its own; the frame texture is 640 KB at 640 x 480 or 160 KB at 320 x 240 | as-stated ([ca](ca.md)) |
| CPU | two sines a line; with the line noise on, a logarithm and six hashes a line | as-stated (counts) |
| PVR | the frame texture drawn once more as three channels of bands: 4 vertices a mesh row a channel | as-stated (counts) |
| Console time | not measured | OPEN |

## Pitfalls

- Call `sh4der_timebase_field` from the main thread: a small worker stack does not hold its two arrays.
- An overscan smaller than the largest shift shows what is behind the picture at its edge.
- Do not strip the NTSCRT notice from the header.

## Related

- [ca](ca.md): the frame texture and the band call
- [film](film.md): the wear of old film, laid over the same picture
- [Budgets](../limits/budgets.md)
