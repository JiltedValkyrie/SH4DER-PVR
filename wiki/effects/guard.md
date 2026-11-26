---
title: Send checks [guard] (real hardware checks)
tags: [effect, guard, hardware]
updated: 2026-11-26
sources: [include/sh4der_guard.h, README.md "Real hardware notes", README.md "Guard tuning"]
---
# Send checks [guard]: checks for what an emulator forgives

**What you will learn:** the two checks every hand-made polygon should pass, and the three faults that stop a real console but not an emulator.
**For:** anyone sending their own quads or fans to the PVR. Read this before your first console run.

> **In plain words.** Two checks to run before you send your own shapes to the console. One check drops a triangle that has no area (its three points sit in a line), because a real console draws it as a bright stray line while an emulator draws nothing. The other refuses a shape's settings block (its polygon header) that was never set up, which can stop a real console. Use both on every fan, ring, strip or full-screen quad you build yourself.

## Try this first

It stops two faults that real consoles show and emulators hide.

1. Each polygon header: compile it before it is sent (the header says `pvr_poly_compile`, include/sh4der_guard.h:27-29). Then check it with `sh4der_poly_hdr_ok(hdr)`: 1 means it was compiled, 0 means refuse it (include/sh4der_guard.h:93-102; ADVISED, a debug check).
2. Each projected triangle: `sh4der_tri_area_ok(a, b, c, near_z, SH4DER_GUARD_MIN_AREA2)`. 1 sends it, 0 drops it (include/sh4der_guard.h:64-69; ADVISED).
3. `SH4DER_GUARD_MIN_AREA2` = 1.0e-7, twice the triangle's area in (x / z, y / z). That is about 0.01 square pixel at 640 x 480 with a 90 degree field, so a triangle is dropped when it has no area at all (include/sh4der_guard.h:61; ADVISED, README "Guard tuning").
4. `near_z`, the depth of your projection's near plane (more than 0): no recommended value yet. A triangle with a point nearer than it is not judged here, so clip it first. The test that would settle any other value is a console run of a fan that crosses the near plane.

On a console: the page says the faults were "seen on a console by the author" (FACT), and "They are not yet in the PVR ledger with a photo."

Header: [include/sh4der_guard.h](../../include/sh4der_guard.h).

## What it does

Two inline checks in a header:

- `sh4der_tri_area_ok`: drops a triangle with no screen area before it is sent.
- `sh4der_poly_hdr_ok`: refuses a polygon header that was never compiled.

Version: v0.10 (guard.h:39). The faults it guards against were seen on a real console (README:1300-1306).

## The faults (README "Real hardware notes", README:1300-1306)

| Fault | On a real console | On an emulator |
|---|---|---|
| A triangle with no screen area (three points on a line, or two the same) | short bright lines | nothing drawn |
| A polygon header never compiled (its command word is 0, which means "end of list") | the list ends; the console can stop | carries on |
| An unaligned 4-byte load or store on the SH-4 | address error; the console stops with nothing on screen | reads it anyway |

These are FACT as the README records them: seen on a console by the author. They are not yet in the [PVR ledger](../hardware/ledger-pvr.md) with a photo.

## Call order

There is none of its own. The checks sit inside the call orders of [Sparks and tracers](spark.md), [Lightning](bolt.md), [Drifting specks](motes.md), [Light pools](pool.md) and [Planar reflection](pl4nar.md) (`AGENTS.md`).

## Cost

None stated. A few multiplies a triangle (README effect table).

## Tuning

`SH4DER_GUARD_MIN_AREA2` (README:1308-1312; guard.h:61).

## Pitfalls

- A triangle with a point behind the near plane is left to your clipper (README:~1306).
- The header must be 4-byte aligned for the check (guard.h:33).
- Colour writers need a stride that is a multiple of 4 (README:1316).
- Fans seen edge-on, such as [Light pools](pool.md) on flat ground or the [Screen spotlight](torch.md) boost's first band, make zero-area triangles. Test every one.

## Related

- [Errata](../errata/README.md), [PVR ledger](../hardware/ledger-pvr.md), [fault catcher](../methods/fault-catcher.md)
