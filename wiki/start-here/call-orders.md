---
title: Call orders
tags: [start, effect]
updated: 2026-11-26
sources: [AGENTS.md "Call order" sections, README.md effect sections]
---
# Call orders

**What you will learn:** where to find the exact order of calls for each effect, and the shape all of them share.
**For:** a programmer wiring an effect into a game loop.

The steps themselves live in [`AGENTS.md`](../../AGENTS.md) and [`README.md`](../../README.md). This page does not repeat them; it tells you where they are.

## The shape most effects share

1. **At load:**
   - fill a parameter struct from the header's defaults;
   - allocate anything in video memory once;
   - check the result: a 0 means there was no room, so draw nothing for that effect.
2. **Each frame, before drawing:**
   - a `*_prepare` / `*_update` / `*_step` call does the per-frame maths on the SH-4;
   - it can refuse, and a refused frame draws nothing.
3. **Per vertex or per quad:** an inline call gives a colour, alpha or offset colour.
4. **While drawing:**
   - put the result in the right display list (opaque, punch-through or translucent);
   - check every header with `sh4der_poly_hdr_ok`;
   - check every triangle with `sh4der_tri_area_ok` (see [Send checks](../effects/guard.md)).

## Where each one is

| Effect | Call order |
|---|---|
| Glowing tubes [blade] | `AGENTS.md` "Call order: blade" |
| Inner light [holo] | `AGENTS.md` "Call order: holo" |
| Exposure and grade [look] | `AGENTS.md` "Call order: look" |
| Sparks and tracers [spark] | `AGENTS.md` "Call order: spark" |
| Planar reflection [pl4nar] | `AGENTS.md` "Call order: pl4nar"; for an engine with no normals, "pl4nar in an engine that has no normals" and `demo/pl4nar/AGENT_WORKFLOW.md` |
| Light pools [pool] | `AGENTS.md` "Call order: pool" |
| bump (normal map) | `AGENTS.md` "Call order: normal map (highlight power 2)" |
| Light through thin parts [sss] | `AGENTS.md` "Call order: sss (transmission)", with starting values per material |
| every other effect | its section in `README.md` ("The <name> effect") and its header's top comment |

Each [effect page](../effects/) links its own call order again, beside its costs and limits.

## Rules that apply to all of them

- The maths goes through SH4ZAM on the Dreamcast: [the SH4ZAM rule](../methods/sh4zam-rule.md).
- Tuning values stay in your code. Change the library's maths once, here.
- Some calls leave the SH-4's matrix register (XMTRX) changed. pl4nar's lookups do. Load your own matrix again before you transform anything else.
