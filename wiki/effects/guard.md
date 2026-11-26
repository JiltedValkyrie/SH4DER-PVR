---
title: Send checks [guard] (real hardware checks)
tags: [effect, guard, hardware]
updated: 2026-11-26
sources: [include/sh4der_guard.h, README.md "Real hardware notes", README.md "Guard tuning"]
---
# Send checks [guard]: checks for what an emulator forgives

**What you will learn:** the two checks every hand-made polygon should pass, and the three faults that stop a real console but not an emulator.
**For:** anyone sending their own quads or fans to the PVR. Read this before your first console run.

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
