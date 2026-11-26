---
title: Glowing tubes [blade]
tags: [effect, blade]
updated: 2026-11-26
sources: [include/sh4der_blade.h, src/sh4der_blade.c, README.md "The blade effect", AGENTS.md "Call order: blade"]
---
# Glowing tubes [blade]

**What you will learn:** what the blade effect draws, how to call it and what it costs.
**For:** a programmer adding a glowing energy blade, rod or tube to a model.

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

- Bad parameters give a dark frame: intensity 0 and light 0. `prepare` returns 0 (README:209).
- Give each object its own phase (57 × the object's index), so a group of them does not pulse together (README:209).
- Strides are in bytes and must be a multiple of 4 (README:1316). An unaligned 4-byte write stops a real console.

## Related

- [Shared parts](core.md)
- [Mesh bloom](bloom.md), the glow round a blade
- [Send checks](guard.md)
