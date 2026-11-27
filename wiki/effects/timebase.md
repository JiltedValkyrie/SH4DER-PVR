---
title: Tape wobble [timebase] (a video tape's bent picture)
tags: [effect, timebase, tape, frame-texture]
updated: 2026-11-26
sources: [include/sh4der_timebase.h, include/sh4der_ca.h, src/sh4der_ca.c, README.md effect table, test/timebase_test.c]
---
# Tape wobble [timebase]: a video tape's bent picture

**What you will learn:** what the timebase effect works out, how the picture is bent with it, and what it costs.
**For:** a programmer who wants a tape look, or who wants to move parts of the finished picture sideways for another reason.

> **In plain words.** A video tape never delivers its lines exactly on time, so a picture played from one wobbles and tears sideways. This effect works out how far each row of the picture slides in one frame: the top rows hook sideways, the rows weave, and the bottom rows jump where the tape's two heads take over from each other. Use it for a tape look in a cutscene, a security monitor, a flashback or a broken broadcast. The model is ported from parts of NTSCRT by Finn McKenty (MIT), so a program that uses it credits NTSCRT and its author.

## Try this first

Slide the rows of the finished picture sideways, so it looks like a worn tape.

1. At load, call `sh4der_timebase_init(&p, rows, width_px)` (`include/sh4der_timebase.h:265`), then `sh4der_timebase_reset(&st)` (`include/sh4der_timebase.h:282`). Set the header's RECOMMENDED start values on the `sh4der_timebase_t` fields (`include/sh4der_timebase.h:139-151`): for a 640-pixel-wide picture of 360 rows, `tension` = 0.375 (a hook of about 24 pixels at the top row), `flutter` = 0.135, `head_switch_us` = 0.6 (shown 1.5 s in every 20 s), `afc_speed` = 0, `amount` = 1 (`include/sh4der_timebase.h:37-39`, ADVISED).
2. Each displayed frame, call `sh4der_timebase_field(&p, &st, field, shift_px)` (`include/sh4der_timebase.h:397`) from the main thread. It needs about 4.2 KB of stack (`include/sh4der_timebase.h:85-87`, ADVISED).
3. Mesh: a mesh row every 6 picture rows, an overscan of 8 pixels, and `SH4DER_CA_ROWS_MESH_X` = 2 (`include/sh4der_timebase.h:39`, `include/sh4der_ca.h:78-79`, ADVISED). Draw the bands with `sh4der_ca_channel_rows` (`include/sh4der_ca.h:204`).
4. Draw the HUD and text after the bent pass, not through it (`include/sh4der_timebase.h:49`, ADVISED).

On a console: no figures (the header: "nothing below is measured"; the page: "Console time | not measured | OPEN").

Header: [include/sh4der_timebase.h](../../include/sh4der_timebase.h). Order: [How the picture is bent](#how-the-picture-is-bent).

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

1. The world is drawn into a frame texture ([Colour fringe](ca.md): `sh4der_ca_begin`).
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
| VRAM | 0 of its own; the frame texture is 640 KB at 640 x 480 or 160 KB at 320 x 240 | as-stated ([Colour fringe [ca]](ca.md)) |
| CPU | two sines a line; with the line noise on, a logarithm and six hashes a line | as-stated (counts) |
| PVR | the frame texture drawn once more as three channels of bands: 4 vertices a mesh row a channel | as-stated (counts) |
| Console time | not measured | OPEN |

## Pitfalls

- Call `sh4der_timebase_field` from the main thread: a small worker stack does not hold its two arrays.
- An overscan smaller than the largest shift shows what is behind the picture at its edge.
- Do not strip the NTSCRT notice from the header.

## Related

- [Colour fringe](ca.md): the frame texture and the band call
- [Old film wear](film.md): the wear of old film, laid over the same picture
- [Budgets](../limits/budgets.md)
