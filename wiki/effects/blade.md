---
title: Glowing tubes [blade]
tags: [effect, blade]
updated: 2026-11-26
sources: [include/sh4der_blade.h, src/sh4der_blade.c, README.md "The blade effect", AGENTS.md "Call order: blade"]
---
# Glowing tubes [blade]

**What you will learn:** what the blade effect draws, how to call it and what it costs.
**For:** a programmer adding a glowing energy blade, rod or tube to a model.

> **In plain words.** The blade glows with light that moves. Thin tubes of light spin inside it, a dark spot slides along it, and the glow pulses. For each corner of the mesh (a vertex) you work out a brightness, then draw the mesh with no texture and its colour added to the picture, so it looks like light. Use it for a sword, a staff, a neon tube or an engine's flame.

## Try this first

For a moving glow on a blade, rod or tube, with one untextured draw per material group.

1. Start from the defaults: `sh4der_blade_params_default` fills the `SH4DER_BLADE_*` values (include/sh4der_blade.h:135).
2. `SH4DER_BLADE_PERIOD` = 192 ticks for one loop, 6.4 s at the 30-ticks-a-second counter (include/sh4der_blade.h:62; ADVISED, README:241).
3. `SH4DER_BLADE_PULSES` = 2 glow pulses in each loop (include/sh4der_blade.h:63; ADVISED, README:242).
4. Give each object its own phase: 57 × the object's index, added to the tick, so a group does not pulse together (README:229; ADVISED).
5. Call `sh4der_blade_prepare` once per instance per frame (include/sh4der_blade.h:146), then `sh4der_blade_argb` with the group's colour (include/sh4der_blade.h:158). A frame it refuses is dark (include/sh4der_blade.h:143-145).

On a console: no figures for the cost. The page says: "None of it is measured on a console."

Header: [sh4der_blade.h](../../include/sh4der_blade.h). Steps in full: [Call order](#call-order).

## What it does

Light tubes orbit an axis. A soft blocker slides along the axis on a helix, and a pulse breathes over both. The result is a brightness for each vertex. You draw the mesh untextured and additive with that colour (README:129-151; blade.h:8-40).

Version: v1.00, used in a released game (blade.h:34).

## Call order

The full steps are in `AGENTS.md` "Call order: blade" and README:192-213 "Calls". In short:

1. Pick the vertices, in the blade's own space, with x along the axis.
2. Call `sh4der_blade_prepare` once per instance per frame.
3. Call `sh4der_blade_argb` per material group. Or call the inline `sh4der_blade_intensity` per vertex in your own loop.
4. `frame.light` is the point-light level for that instance.

## Cost

All as-stated (README:256). None of it is measured on a console.

| Item | Cost |
|---|---|
| Frame state | 156 B (`sizeof` on the host, 2026-11-25, Parity; the ripple fields were added after README:256's 144 B. No pointers, so the SH-4 size matches) |
| Parameters | 160 B (the same probe; README:256 says 136 B) |
| Per vertex | two 2D distance tests and one 3D test; a feather only inside a tube or the blocker |
| PVR | an untextured draw, said to cost the PVR less than the textured draw it replaces (not measured) |
| VRAM | 0 |

## Recommended limits

- `tube_count` runs from 0 to `SH4DER_TUBE_MAX`, and `blocker_count` from 0 to `SH4DER_BLOCKER_MAX` (blade.h:103-104).
- `turns`, `passes` and `pulses` must be whole numbers, or the period does not close (README:151).

## Tuning

See `README.md` "Blade tuning" (README:215-255), with the WHITE preset (README:249-254).

## Pitfalls

- Bad parameters give a dark frame: intensity 0 and light 0. `prepare` returns 0 and leaves the frame dark (`include/sh4der_blade.h:144`, the comment on `sh4der_blade_prepare`).
- Give each object its own phase (57 × the object's index), so a group of them does not pulse together (README:229).
- Strides are in bytes and must be a multiple of 4 (README:1316). An unaligned 4-byte write stops a real console.

## Related

- [Shared parts](core.md)
- [Mesh bloom](bloom.md), the glow round a blade
- [Send checks](guard.md)
