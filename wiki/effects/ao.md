---
title: ao (small-scale occlusion)
tags: [effect, ao]
updated: 2026-11-25
sources: [include/sh4der_ao.h, README.md "The ao effect"]
---
# ao: small-scale occlusion

**What you will learn:** the two kinds of darkening the ao header gives, and when each is computed.
**For:** a programmer who wants crevice shading baked into a model, or a cheap cavity cue on a finished frame.

## What it does

Header only (README:689-704; ao.h:10, 18):

- **Crease darkening** from the angle between faces. It is for a bake: the result goes into the vertex colour, so it costs nothing at run time.
- **A screen cavity cue** from the small look grid (80 × 60 by default). It is one frame behind.

Version: v0.10 (ao.h:29).

## Call order

See README:689-704. `AGENTS.md` has no section of its own.

## Cost

As-stated. There are no numbers.

| Item | Cost |
|---|---|
| Crease bake | nothing at run time (a baked vertex colour) |
| Screen cue | "no runtime cost" in the header's summary line; it reuses the [frame](frame.md) read |

## Recommended limits

- The grid is 80 × 60 by default, to match the [look](look.md) read.

## Tuning

See `README.md` "Ao tuning" (README:693-704).

## Pitfalls

- The screen term is one frame behind (ao.h:10).
- A black read needs a floor (`SCR_FLOOR`), or black areas read as deep cavities.

## Related

- [frame](frame.md), [look](look.md), [sss](sss.md)
