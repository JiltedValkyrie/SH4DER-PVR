---
title: "Genre guide: light and glow"
tags: [start, guide, effect, light]
updated: 2026-11-26
sources: [include/sh4der_blade.h, include/sh4der_bloom.h, include/sh4der_glow.h, include/sh4der_torch.h, include/sh4der_pool.h, include/sh4der_shafts.h, include/sh4der_spark.h, include/sh4der_bolt.h, README.md (effect sections and tuning tables), AGENTS.md (pool call order), wiki/effects/names.md (section 0), wiki/effects/blade.md, wiki/effects/bloom.md, wiki/effects/glow.md, wiki/effects/torch.md, wiki/effects/pool.md, wiki/effects/shafts.md, wiki/effects/spark.md, wiki/effects/bolt.md, wiki/effects/frame.md, wiki/limits/recommended-limits.md, wiki/limits/header-recommended.md, wiki/status/maturity.md, wiki/hardware/ledger-pvr.md, wiki/hardware/ledger-sh4.md, wiki/hardware/ledger-bus-dma.md, wiki/hardware/ledger-video.md, wiki/glossary.md, wiki/STRUCTURE.md, wiki/start-here/what-do-you-want.md, wiki/start-here/call-orders.md, wiki/start-here/license-and-credit.md]
---
# Genre guide: light and glow

**What you will learn:** which light and glow effect fits a picture, what each one costs, and how to start it.
**For:** a programmer adding light, glow, sparks or lightning to a Dreamcast game that uses SH4DER-PVR.

This page covers eight effects. Each has a reader name, then its module name in brackets. Most costs are not measured on a console yet, so each number carries a status tag. The [glossary](../glossary.md) explains the PVR (the graphics chip) and other terms.

*Additive* blending adds a colour to the picture under it. *Alpha* is see-through: 0 is clear and 1 is solid. *Vertex colour* is a colour stored at each corner of a triangle.

## What you can make

You can make a glowing blade, a lamp that lights the ground under it, and a sun that bleeds glow into the picture. You can make a flashlight that lights one circle in a dark corridor, and muzzle flashes, tracers, sparks and lightning. Five of the eight use no video memory. Screen bloom and dark shafts read the finished picture, so they cost more.

## The effects

### Glowing tubes [blade]

**What it looks like:** bright streaks circle inside a flat, see-through blade, and a dark knot moves along it.
**Use it when:** a blade, staff, plume or cable should look full of moving energy.
**Where it is:** [header](../../include/sh4der_blade.h) and [effect page](../effects/blade.md).
**Starting settings:**
- No start setting is recommended in the header, the limits or this wiki. The defaults are in the README "Blade tuning" table.

### Mesh bloom [bloom]

**What it looks like:** a soft, feathered glow sits on a blade's tip, or a round glow with rings sits round a lamp or a sun.
**Use it when:** you want a halo or a lens-flare ghost made from geometry, with no frame read.
**Where it is:** [header](../../include/sh4der_bloom.h) and [effect page](../effects/bloom.md).
**Starting settings:**
- No start setting is recommended. The defaults are in the README "Bloom tuning" table.

### Screen bloom [glow]

**What it looks like:** bright parts of the picture spill a soft halo over what is round them, and the sun can throw streaks of light.
**Use it when:** lamps, neon or the sun should bleed light into the picture, and you can afford the frame read.
**Where it is:** [header](../../include/sh4der_glow.h) and [effect page](../effects/glow.md).
**Starting settings:**
- `SH4DER_GLOW_THRESHOLD` = 0.62 and `SH4DER_GLOW_POWER` = 2, with no rays, from the RECOMMENDED block (include/sh4der_glow.h:8). ADVISED (glow.h:13).
- `SH4DER_GLOW_STRENGTH` = 0.45, `SH4DER_GLOW_RADIUS` = 0.5 and `SH4DER_GLOW_DARKEN_MAX` = 0.25 (glow.h:9, 115-117). ADVISED. Never draw the darken without the bloom (glow.h:16).
- The threshold is DISPUTED. The header's note for the author's own game uses a straight ramp from 0.80, `sh4der_glow_curve(lut, 0.80f, 0)`, because a start of 0.62 lets mid-bright walls glow (glow.h:67-71). Both are ADVISED. Try both by eye.

### Screen spotlight [torch]

**What it looks like:** a round patch of light with a hot centre sits on the screen, and the area outside it goes dark.
**Use it when:** a flashlight or head lamp should light one spot in a dark corridor.
**Where it is:** [header](../../include/sh4der_torch.h) and [effect page](../effects/torch.md).
**Starting settings:**
- `SH4DER_TORCH_DARK_LO` = 0.30 and `SH4DER_TORCH_DARK_HI` = 0.80 (torch.h:106-107): the carve is full up to 0.30 baked light and gone from 0.80. Keep LO under HI. ADVISED (no measured number).
- `SH4DER_TORCH_RISE_S` = 0.2 and `SH4DER_TORCH_FALL_S` = 0.8 (torch.h:108-109). ADVISED.
- `SH4DER_TORCH_UMAX` = 1.6 (torch.h:97). ADVISED.
- `SH4DER_TORCH_SPOKES` = 16 (torch.h:110). It must stay 16, because the unit table in src/sh4der_torch.c has 16 spokes. FACT.

The header's distances assume one world unit of about 3 metres (torch.h:68-69). Scale them to your own units.

### Light pools [pool]

**What it looks like:** a soft patch of light lies on the ground, bright at its centre and gone at its rim. A dark version makes a blob shadow.
**Use it when:** a street lamp or a headlight should light coarse ground, or a car needs a blob shadow.
**Where it is:** [header](../../include/sh4der_pool.h) and [effect page](../effects/pool.md).
**Starting settings:**
- A disc of 10 triangles for a lamp, 8 for a blob shadow, and a wedge of 2 to 4 for a headlight (include/sh4der_pool.h:8). ADVISED (pool.h:13).
- `SH4DER_POOL_MAX_N` = 16, the most triangles in one fan (pool.h:9, macro at pool.h:74). ADVISED.
- Lift the fan 2 to 4 cm off the ground, with a blob alpha of about 0.4 (pool.h:9). ADVISED.
- Limit lamps by distance, because a pool's cost is translucent fill. ADVISED ([recommended limits](../limits/recommended-limits.md), Fill).

### Dark shafts of light [shafts]

**What it looks like:** dark shapes in the picture cast darker copies away from a light.
**Use it when:** trees, pillars or window bars stand between the camera and a low sun.
**Where it is:** [header](../../include/sh4der_shafts.h) and [effect page](../effects/shafts.md).
**Starting settings:**
- Strength 0.50, off by default, with a useful range of 0.25 to 3.00. A strong shade reads as a second, wrong shadow. This is the header's note, not a macro (shafts.h:44-46). ADVISED.
- Draw it under the eye's layers and the glow (shafts.h:47-48). Clear the map once when the light leaves the view, not every frame (shafts.h:49). ADVISED.

### Sparks and tracers [spark]

**What it looks like:** short bright flashes, sparks and streaks fly out, fade and fall.
**Use it when:** a muzzle flash, a tracer, an impact spark or a lamp's halo should flash for a few frames.
**Where it is:** [header](../../include/sh4der_spark.h) and [effect page](../effects/spark.md).
**Starting settings:**
- `SH4DER_SPARK_MAX` = 24, which means 96 vertices a frame at most, and `SH4DER_SPARK_MIN_PX` = 0.8 (spark.h:11). ADVISED (spark.h:16).
- `SH4DER_SPARK_INSET` = 0.0078125 for a 64 x 64 atlas (spark.h:12). ADVISED.
- Give `SH4DER_SPARK_KEEP` to a flash or a tracer, the things that must never be pushed out (spark.h:17). ADVISED.

### Lightning [bolt]

**What it looks like:** thin, jagged strands of light branch from a root and re-draw a few times a second.
**Use it when:** an arc, a spark gap or a storm needs lightning that changes shape.
**Where it is:** [header](../../include/sh4der_bolt.h) and [effect page](../effects/bolt.md).
**Starting settings:**
- No start setting is recommended. The defaults are in the README "Bolt tuning" table. NOT RUN on a console.

## What it costs

Video memory is the chip's own memory, where textures live. A triangle is one flat shape the chip draws, and more triangles cost more time.

Screen bloom [glow] and dark shafts [shafts] read the finished picture back, so they need the [frame read](../effects/frame.md). The frame read has no time on our console yet. Screen spotlight [torch] draws its layers over the picture and does not read it back. Its darkness gate uses baked light, so it does not need the frame read either.

| Effect | Video memory | Triangles or vertices | SH-4 time | Status |
|---|---|---|---|---|
| Glowing tubes [blade] | 0 bytes. FACT: the draw is untextured (README, blade cost note) | None added: the work is done in the model's own vertex colours (FACT, blade.h:37-38) | Not measured (OPEN: time `sh4der_blade_prepare` and the vertex loop) | PROTOTYPE v0.1. No console row ([maturity](../status/maturity.md)) |
| Mesh bloom [bloom] | 0 bytes. FACT: no texture (README, bloom cost note) | Blade glow: 24 vertices, 24 triangles. Largest ring glow: 193 and 360 (FACT, bloom.h:59-62) | Rings: 205 us for 129 vertices, a lamp's halo. MEASURED, one console, one run (SH4-083; the row gives no date, only the photo name 03:08.6). Blade glow: not measured (OPEN) | Blade glow: PROTOTYPE v0.1, NOT RUN on a console (host only). Rings: ALPHA v0.3 |
| Screen bloom [glow] | 16 KB texture (128 x 64 at 2 bytes a texel) and a 1 KB shrink. FACT (glow.h:107-112) | Three full-screen layers, plus up to 4 ray copies (glow.h:118). FACT | Frame read: 1,196 us in another port. REPORTED (VID-005). Our time: not measured (OPEN) | PROTOTYPE v0.1. The demo disc runs it (REPORTED). No ledger row |
| Screen spotlight [torch] | 0 bytes. FACT: no texture (README, torch section) | 306 vertices and 288 triangles a frame, in two meshes (FACT, torch.h:113) | Not measured (OPEN: time both meshes and the near add) | PROTOTYPE v0.1. No ledger row |
| Light pools [pool] | 0 bytes. FACT (README, pool cost) | A 10-triangle disc has 11 vertices. FACT (design) | A rim vertex takes one sine and cosine pair and six multiply-adds. FACT (README). Time and PVR fill: not measured (OPEN) | ALPHA v0.2. The street demo's blob stage ran, not the pool alone (PVR-160) |
| Dark shafts of light [shafts] | 16 KB texture, the glow's size. FACT (shafts.h:26-27) | One mesh of 8 x 6 cells on the PVR. FACT (shafts.h:28) | 4,800 cells a frame on the SH-4. FACT (shafts.h:27). Time: not measured (OPEN) | PROTOTYPE v0.1. The demo disc runs it (REPORTED). No ledger row |
| Sparks and tracers [spark] | Your atlas: 3 KB for a 64 x 64 VQ RGB565 atlas. ESTIMATE (as stated in README) | Up to 24 quads: 96 vertices and 48 triangles. FACT (spark.h:11) | 142 ns a vertex in a store-queue run (BUS-003), 260 ns with `pvr_prim` (BUS-001). MEASURED, one console | PROTOTYPE v0.1. No ledger row names the pool. The street demo's halos ran ([maturity](../status/maturity.md)) |
| Lightning [bolt] | 0 bytes. FACT (README, bolt cost) | Default look: 120 triangles, 240 vertices. FACT (README) | Not measured (OPEN: time `sh4der_bolt_draw` on a console) | PROTOTYPE v0.1. NOT RUN on a console: no program uses it |

PVR time (the chip's drawing time) has no column above, so here it is:

- **Screen bloom:** one full-screen translucent layer at 640 x 480 costs about 3,112 us. MEASURED, one console (PVR-002). Three layers come to about 9.3 ms. That total is ESTIMATE, arithmetic on the measured layer.
- **Pools, sparks and lightning:** 256 translucent triangles in one tile took 6,385 us. Spread over the screen, they took 82 us. MEASURED, one console (PVR-009). Spread the quads out, and cap how many overlap.
- **Torch:** not measured (OPEN).

Your RAM, which is not video memory, is small. The spark pool is 1,256 bytes and the bolt state is about 3.3 KB. Both are ESTIMATE from the declared sizes. The spark send is MEASURED per vertex (142 ns or 260 ns), so 96 vertices at 260 ns come to 24.96 us: ESTIMATE, under the 25 us in spark.h:14.

Torch with anti-aliasing 2X: scale the x value exactly once, or the spot sits on the right edge as an oval (VID-010, FACT; torch.h:40-46).

## Try this first

Light pools [pool] make the simplest start, because the header uses no texture. Its call order is in `AGENTS.md` under "Call order: pool". The [call orders](call-orders.md) page lists where the others are. Read [licence and credit](license-and-credit.md) before you use this code in a project.

1. Include `sh4der_pool.h` and `sh4der_guard.h` (the area test). The pool header uses `sh4der_core.h`, so the Dreamcast build must link SH4ZAM. See [setup](setup.md).
2. At load, make two untextured translucent headers, with the depth test on, depth writes off and no culling. One blends additively (source alpha, then one) for light. The other blends source alpha with one minus source alpha, for a blob shadow.
3. Each frame, after the ground, in the translucent list: for each lamp near the camera, call `sh4der_pool_disc`. Lift the centre 2 to 4 cm off the ground, use two axes in the ground plane as the radii, set n to 10, and pass the lamp's colour and brightness as alpha. The output array must hold `SH4DER_POOL_MAX_N` + 2 vertices.
4. Transform the vertices with your own matrix. Send disc triangle k as 0, k + 1 and (k + 1) % n + 1. Test each triangle with `sh4der_tri_area_ok` before you send it.
5. When it works, a soft patch of colour lies on the road under the lamp. It is brightest at the centre and gone at the rim. If it flickers against the ground, lift it a little more. If the edge looks hard, check that the rim alpha is 0.

## Related

- [What do you want to make?](what-do-you-want.md)
- [effect index](../effects/README.md)
- [recommended limits](../limits/recommended-limits.md)
- [glossary](../glossary.md)
- [licence and credit](license-and-credit.md)
- [demo guide](demo-guide.md)
