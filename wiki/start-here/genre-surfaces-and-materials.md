---
title: "Genre guide: surfaces and materials"
tags: [start, guide, genre, surfaces, materials]
updated: 2026-11-26
sources: ["wiki/STRUCTURE.md", "wiki/effects/names.md", "wiki/start-here/what-do-you-want.md", "wiki/glossary.md", "wiki/limits/header-recommended.md", "wiki/limits/recommended-limits.md", "wiki/effects/bump.md", "wiki/effects/pl4nar.md", "wiki/effects/uvwarp.md", "wiki/effects/sss.md", "wiki/effects/holo.md", "wiki/effects/water.md", "wiki/effects/README.md", "wiki/status/maturity.md", "wiki/hardware/ledger-sh4.md", "wiki/hardware/ledger-pvr.md", "wiki/open-questions.md", "wiki/start-here/call-orders.md", "include/sh4der_bump.h", "include/sh4der_pl4nar.h", "include/sh4der_uvwarp.h", "include/sh4der_sss.h", "include/sh4der_holo.h", "README.md (sections on normal map, pl4nar, uvwarp, sss, holo)", "AGENTS.md (starting points per material)"]
---
# Genre guide: surfaces and materials

**What you will learn:** which effect gives a surface its look (bumps, a mirror, a rippling texture, light through a thin edge, or a glow inside a body), what each one costs, and how far each one has been tried on a real Dreamcast.

**For:** a programmer adding surface detail to a Dreamcast game, and any agent choosing an effect for a surface.

This genre has five effects. Three of them have no console number yet, so read each status line before you rely on a cost. Terms are defined on first use or linked to the [glossary](../glossary.md).

## What you can make

A floor or a wall can show grooves and ribs that catch a moving torch, with no bump hardware. A car body, a window or a wet road can show the street mirrored in it. A flag, a banner or an electric arc can ripple, because each vertex's texture coordinate is pushed by a small table. A thin ear, a leaf or a wax candle can glow when the light sits behind it. A hologram's chest or a ghost can hold a soft light that brightens and dims with its breathing.

## The effects

### Normal map [bump]

**What it looks like:** a flat surface shows grooves and ribs that catch a moving light, drawn with blend passes of grey textures instead of the PowerVR's [bump unit](../glossary.md).

**Use it when:** a large flat surface, such as a floor or a wall, is lit by a torch or another light that moves.

**Where it is:** [header](../../include/sh4der_bump.h) and [effect page](../effects/bump.md).

**Starting settings:**
- `SH4DER_BUMP_STRENGTH` = 0.65 (include/sh4der_bump.h:67; README "Normal map tuning"): the highlight at full light. ADVISED. Run on a console by the owner in Torch walk (her test; no measured numbers recorded yet); no ledger row judges it.
- `SH4DER_BUMP_MAX_FAN` = 12 (include/sh4der_bump.h:70): the most corners one sent polygon may have. The code refuses more (bump.h:272). ADVISED: no console number backs it.
- Map side: a power of two up to 256 (include/sh4der_bump.h:152). The README's cost line uses 64 × 64 (README "The normal map effect"). ADVISED.
- The torch walk's vertex light uses kd 1.3 and ks 2.5 (a comment at include/sh4der_bump.h:227; not macros). ADVISED.

### Planar reflection [pl4nar]

**What it looks like:** a car body, glass or a wet road shows the world mirrored in it, from a small second picture drawn by [render to texture](../glossary.md) and looked up per vertex.

**Use it when:** a surface lies in or near one plane, such as a road, a floor, or a car body above a road, and a mirror look is worth a second draw of the world.

**Where it is:** [header](../../include/sh4der_pl4nar.h) and [effect page](../effects/pl4nar.md).

**Starting settings:** the header's block gives these as ADVISED. The street driving demo ([demo/pl4nar](../../demo/pl4nar/README.md)) has run on a console, so no NOT RUN tag applies.
- A target of 256 × 128, 64 KB with `-DSH4DER_PL4NAR_HAVE_RTT` (include/sh4der_pl4nar.h:8, 76-78). ADVISED. The header says to go down in size first, but in the car runs the size did not change the mirror pass's PVR time (PVR-174 and PVR-175, MEASURED, one console), so measure the mirror at each size before you cut.
- `SH4DER_PL4NAR_FOV_SCALE` = 1.55 for a body, and 1 for a flat surface (include/sh4der_pl4nar.h:118). ADVISED.
- `SH4DER_PL4NAR_F0_PAINT` = 0.06 (line 112) and `SH4DER_PL4NAR_F0_GLASS` = 0.10 (line 113). ADVISED. Paint and glass were judged on a picture from the host. The other materials are untested gains (README "Materials").
- `SH4DER_PL4NAR_REACH` = 18.0 (line 116) and `SH4DER_PL4NAR_GROUND_KEEP` = 0.55 (line 121). ADVISED.

### Texture ripple [uvwarp]

**What it looks like:** the texture on a strip, flag or arc slides and wobbles, because each vertex's texture coordinate is pushed by a small scrolling table.

**Use it when:** the original effect used a distortion map that the PowerVR cannot read per pixel, and the mesh has enough vertices along the motion.

**Where it is:** [header](../../include/sh4der_uvwarp.h) and [effect page](../effects/uvwarp.md).

**Starting settings:** NOT RUN on a console. No RECOMMENDED block exists for this effect, so these values come from the README and the header's limits.
- A table of 64 × 16 pairs of bytes, 2 KB (README "The uvwarp effect"; include/sh4der_uvwarp.h:26). ADVISED as a size. The 2 KB is arithmetic (ESTIMATE).
- `SH4DER_UVWARP_SIDE_MAX` = 256 (include/sh4der_uvwarp.h:73) and `SH4DER_UVWARP_UV_MAX` = 8388608.0f, which is 2^23 (line 74): the largest side of a table, and the coordinate limit past which the call gives 0, 0. ADVISED.
- The weight fades to 0 at both ends of the strip, so the ends stay on their anchors (README "The uvwarp effect"). ADVISED.

### Light through thin parts [sss]

**What it looks like:** a thin edge (an ear against the sun, a leaf, a wave crest) glows in the light's colour when the light sits behind it, as a per-vertex shortcut and not a multiple-scattering solve (a model that follows light bouncing many times inside a material).

**Use it when:** the light is behind a thin part of a model and the eye sees the front, as at dusk, against a window or in a low sun; it is not for thick opaque bodies or front-lit scenes (AGENTS.md "Where it looks best on the PVR").

**Where it is:** [header](../../include/sh4der_sss.h) and [effect page](../effects/sss.md).

**Starting settings:** there is no macro table (the header has no `SH4DER_SSS_*` macros). The values go into `sh4der_sss_init`, and AGENTS.md "Starting points per material" gives them. They are invented starting values, not measured and not from reference data, so each is ADVISED.
- Skin: absorption 0.5, 1.8, 3.0; wrap 0.5; distortion 0.3; gain 0.4 to 0.8. ADVISED.
- Leaf: absorption 1.2, 0.3, 1.8; wrap 0.8; distortion 0.2. ADVISED.
- Wax or candle: absorption 0.3, 0.4, 0.9; wrap 0.6; distortion 0.2. ADVISED.

### Inner light [holo]

**What it looks like:** a soft light inside a translucent body, such as a hologram's chest, brightens as the body breathes in and dims as it breathes out.

**Use it when:** a see-through body should look lit from within, and the body's own motion should drive the light.

**Where it is:** [header](../../include/sh4der_holo.h) and [effect page](../effects/holo.md).

**Starting settings:** NOT RUN on a console. The values are the defaults in the README's "Holo tuning" table, which match the header's macros.
- `SH4DER_HOLO_BASE` = 0.60 and `SH4DER_HOLO_AMP` = 0.40: the core's strength at breath out, and the amount added at breath in (include/sh4der_holo.h:61-62). ADVISED.
- `SH4DER_HOLO_BODY` = 0.62 (line 63): bake the texture 1 / 0.62 brighter, so the body away from the core shows it as painted (README "The holo effect"). ADVISED.
- `SH4DER_HOLO_INNER` = 0.010 and `SH4DER_HOLO_OUTER` = 0.055 (lines 68-69): the core's feather. ADVISED. The centre (`SH4DER_HOLO_X`, `_Y`, `_Z`, lines 65-67) is a chest marker for one bind model, so a new body needs its own centre. The centre's values are ADVISED too.

## What it costs

| Effect | Video memory | Triangles or vertices | SH-4 time | Status |
|---|---|---|---|---|
| Normal map [bump] | Nine grey maps, 4 KB each at 64 × 64 (ESTIMATE, arithmetic) | Up to 8 draws a lit cell; fans of up to 12 corners (ESTIMATE, from the pass list) | Not measured (OPEN). Test: the cost label of the [torch walk](../../demo/torchwalk_ab.c) on a console | PROTOTYPE v0.1; no ledger row; two looks OPEN (Q42) |
| Planar reflection [pl4nar], lookup | 64 KB at 256 × 128 (FACT, include/sh4der_pl4nar.h:76-78) | 320 looked-up vertices on the car body (FACT, host count) | 0.92 to 0.97 ms (MEASURED, 1 console, 1 run, PVR-160) | ALPHA v0.5 |
| Planar reflection [pl4nar], mirror pass | The same target | Not counted for a fixed scene (OPEN) | About 41 ms of frame added (ESTIMATE, a difference of two frame times, PVR-160) | ALPHA v0.5 |
| Texture ripple [uvwarp] | None (FACT: the header allocates nothing) | Per vertex; no count | Not measured (OPEN). Test: time a loop of `sh4der_uvwarp_offset_p` with the cycle counter, as in [benchmarking](../methods/benchmarking.md) | PROTOTYPE v0.1; NOT RUN on a console |
| Light through thin parts [sss] | None (FACT: the header allocates nothing) | Per vertex; no count | 187 us for 65 vertices in the lamp program's shade loop, which calls the sss maths and normalises (MEASURED, 1 console, 1 run, SH4-083; the loop is not the library call alone) | ALPHA v0.3 |
| Inner light [holo] | None (FACT: the header allocates nothing) | 64 multiply-adds a frame and one table load a vertex (ESTIMATE, counts from the README) | Not measured (OPEN). Test: time `sh4der_holo_prepare` and a vertex loop with the cycle counter | PROTOTYPE v0.1; NOT RUN on a console |

Video memory is the 8 MB the graphics chip keeps its textures and render targets in, a triangle is one flat shape it draws, and a vertex is one corner the SH-4 works out before the chip draws it.

Some pages call their counts "as-stated". Here those counts are tagged ESTIMATE: arithmetic on the code's constants, not a console run. A per-vertex cost means little until you know your own vertex count.

The effect pages lag the ledger in two places, and the table follows the ledger:
- **Planar reflection.** The effect page lists one console run for the lookup. The ledger adds 0.59 to 0.65 ms (PVR-174) and 0.96 to 0.97 ms (PVR-199), both MEASURED. The mirror pass's PVR time, about 3.2 ms at all three target sizes, is MEASURED (PVR-175). The effect page still marks it OPEN (Q97). The layer's PVR time is ESTIMATE, under 0.5 ms.
- **Light through thin parts.** The effect index says no console cost exists, but SH4-083 times the lamp program's shade loop, which calls the sss maths; it does not time the library call alone. The header's cost line names SH4ZAM's `shz_expf` (include/sh4der_sss.h:33-34); the code uses its own exponential (lines 71-84). A sea demo's wave crests use the same maths through the water module: see the [water effect page](../effects/water.md).

Open items, each with its test, are in [open questions](../open-questions.md): Q42 (bump) and Q43 (planar reflection).

## Try this first

This recipe uses the texture ripple, the smallest effect here: one header, no render target, no video memory. AGENTS.md has no call order for it, so these steps follow the README section "The uvwarp effect". The other call orders are in [call orders](call-orders.md).

1. **Test on the host.** Run `make test`, which includes `test/uvwarp_test.c`. A host pass checks the maths, not the console ([setup](setup.md); [host test first](../methods/host-test-first.md)).
2. **Include the header.** Put `include/` on the include path and write `#include "sh4der_uvwarp.h"`. On the Dreamcast, build without `SH4DER_CORE_NO_SH4ZAM` and link SH4ZAM as KallistiOS says. Keep the credit: see [licence and credit](license-and-credit.md).
3. **At load, fill a table.** Make a `sh4der_uvwarp_t` whose `rg` points at w × h pairs of bytes (red, then green, row by row). Keep w and h from 1 to `SH4DER_UVWARP_SIDE_MAX`, and start at 64 × 16 (2 KB). For a trial run, any pattern will do.
4. **At load, weight the strip.** Give each vertex a weight from 0 to 1, and fade it to 0 at both ends.
5. **Each frame, push the vertices.** Call `sh4der_uvwarp_prepare(&prep, &table, su, sv, time)` once; a return of 0 means skip the loop. Then, for each vertex, call `sh4der_uvwarp_offset_p(&prep, u, v, weight, &du, &dv)` and add `du` and `dv` to the texture coordinate. The prepared form does not need the time wrapped; the plain `sh4der_uvwarp_offset` does (include/sh4der_uvwarp.h:39-43).
6. **What you should see.** The texture slides along the strip and wobbles, and the ends stay put. If an end slides, its weight is above 0.

## Related

- [What do you want to make?](what-do-you-want.md)
- [Effect index](../effects/README.md)
- [Recommended limits](../limits/recommended-limits.md)
- [Glossary](../glossary.md)
- [Licence and credit](license-and-credit.md)
- [Demo guide](demo-guide.md)
- [Call orders](call-orders.md), [open questions](../open-questions.md), and the [README](../../README.md)
