---
title: Screen spotlight [torch] (flashlight)
tags: [effect, torch, post]
updated: 2026-11-26
sources: [include/sh4der_torch.h, src/sh4der_torch.c, README.md "The torch effect"]
---
# Screen spotlight [torch]: a flashlight on screen

**What you will learn:** how the torch fakes a flashlight with no light source, and the trap with AA 2X.
**For:** a programmer adding a first-person flashlight to a dark level.

## What it does

A flashlight hotspot carved on screen, with no light source in the scene (README:617-632; torch.h:8-40):

- a near add per vertex;
- a screen-space multiply carve that darkens outside the circle;
- a boost layer with a hot core and a corona;
- the [fog table](../glossary.md) bent to black for reach.

It switches on by the level's baked darkness, with hysteresis.

`src/sh4der_torch.c` has `sh4der_torch_headers` and `sh4der_torch_draw`. The header goes in by `pvr_prim` and the vertices by direct render (torch.c:~109).

Version: v0.50, runs on screen on the demo disc (torch.h:48).

## Call order

See README:634-658 "Calls" and the recommended settings at README:1225 and torch.h:57-70. `AGENTS.md` has no section of its own.

## Cost

As-stated. None of it is measured on a console.

| Item | Cost | Source |
|---|---|---|
| Two meshes | 9 strips of 34 vertices: 306 vertices, 288 triangles a frame | README:630; torch.h:113 |
| Vertex buffer | `TORCH_VERTS` × 16 B (4.9 KB) | torch.h:113 |
| Texture, VRAM | none | |

## Recommended limits

- The unit table in `sh4der_torch.c` has 16 spokes, so `SPOKES` must be 16 (README:685).
- Carve `UMAX` 1.6, with the corners at 1.25 (README:672). `umax` is never under the default (torch.h:279).
- Keep `DARK_LO` under `DARK_HI` (torch.h:~64).

## Tuning

See `README.md` "Torch tuning" (README:660-687).

## Pitfalls

- **AA 2X.** With AA 2X on, the PVR renders 1,280 wide. Scale x exactly once: in `cx`, the x scale given to `sh4der_torch_radii`, and the width given to `sh4der_torch_umax`. Scale it twice and the spot sits on the right edge as an oval (README:632; torch.h:~40-46).
- Gate the torch from baked luminance, not from the frame read, or the carve feeds back (README:628).
- The boost mesh's first band is made of zero-area triangles (torch.c:39). The [Send checks](guard.md) should drop them. A real console draws zero-area triangles as bright lines (README:1304).
- Distances are scaled for one world unit being about 3 m in the author's game (torch.h:~66). Scale them for your own units.

## Related

- [Exposure and grade](look.md) (hold the eye while the torch is lit), [Send checks](guard.md), [Normal map](bump.md) (relief lit by the torch)
