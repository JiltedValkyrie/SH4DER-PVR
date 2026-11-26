---
title: Screen spotlight [torch] (flashlight)
tags: [effect, torch, post]
updated: 2026-11-26
sources: [include/sh4der_torch.h, src/sh4der_torch.c, README.md "The torch effect"]
---
# Screen spotlight [torch]: a flashlight on screen

**What you will learn:** how the torch fakes a flashlight with no light source, and the trap with AA 2X.
**For:** a programmer adding a first-person flashlight to a dark level.

> **In plain words.** A flashlight with no real light in the scene. A round patch of the screen stays bright with a hot middle, the dark round it is pushed darker, and the light fades with distance. It switches on in dark places and off in light ones, with a band between so it does not flicker. Use it for a torch, a miner's lamp or a vehicle's head lamps; in daylight it darkens nothing.

## Try this first

For a flashlight or lamp in a dark level, with no light source in the scene.

1. The gate reads the level's baked brightness, not the frame: `SH4DER_TORCH_ON_LO` = 0.12 switches it on below that, `SH4DER_TORCH_OFF_HI` = 0.22 switches it off above that (include/sh4der_torch.h:102-103; ADVISED, README:698-699; why: include/sh4der_torch.h:31-33).
2. The carve follows darkness: `SH4DER_TORCH_DARK_LO` = 0.30 is a full carve, `SH4DER_TORCH_DARK_HI` = 0.80 is no carve (include/sh4der_torch.h:106-107; ADVISED, README:702-703).
3. The near light and the reach: `SH4DER_TORCH_NEAR_WU` = 7.0 world units; the reach fades from `SH4DER_TORCH_FOG_NEAR` = 2.0 to `SH4DER_TORCH_FOG_FAR` = 6.0 (include/sh4der_torch.h:99-101; ADVISED, README:695-697). The header's author's-game note gives 6.0 and a reach of 5.0 (include/sh4der_torch.h:58-62), so check these against your own units.
4. The edge: about 22 degrees off the view axis (the author's game, include/sh4der_torch.h:60-61). The carve mesh reaches `SH4DER_TORCH_UMAX` = 1.6 (include/sh4der_torch.h:97; ADVISED, README:693).
5. With AA 2X on, scale x exactly once: in `cx`, in the x scale given to `sh4der_torch_radii`, and in the width given to `sh4der_torch_umax` (include/sh4der_torch.h:40-46; ledger VID-010, FACT).

On a console: the page says "As-stated. None of it is measured on a console."

Header: [sh4der_torch.h](../../include/sh4der_torch.h). Steps in full: [Call order](#call-order).

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
| Two meshes | 9 strips of 34 vertices: 306 vertices, 288 triangles a frame | README:651; torch.h:113 |
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
- Gate the torch from baked luminance, not from the frame read, or the carve feeds back (README:649).
- The boost mesh's first band is made of zero-area triangles (torch.c:39). The [Send checks](guard.md) should drop them. A real console draws zero-area triangles as bright lines (README:1304).
- Distances are scaled for one world unit being about 3 m in the author's game (torch.h:~66). Scale them for your own units.

## Related

- [Exposure and grade](look.md) (hold the eye while the torch is lit), [Send checks](guard.md), [Normal map](bump.md) (relief lit by the torch)
