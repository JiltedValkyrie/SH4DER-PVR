---
title: sss (back-lit transmission)
tags: [effect, sss]
updated: 2026-11-25
sources: [include/sh4der_sss.h, README.md "The sss effect", AGENTS.md "Call order: sss (transmission)"]
---
# sss: back-lit transmission

**What you will learn:** what the sss call gives you, what it does not, and where its starting values come from.
**For:** a programmer who wants light through a thin part of a model: an ear against the sun, a leaf, wax, ice, a wave crest.

## What it does

An artistic stand-in for sub-surface scattering. It is per vertex: light comes through a thin or translucent part of a surface toward the eye, tinted by absorption over a thickness you supply. It returns an RGB for you to add, for example into the offset colour. It is **not** a multiple-scattering solve: there is no diffusion profile and no blur across the surface (`AGENTS.md` "Call order: sss"; README:779-805).

Header only. No PVR drawing.

Version: v0.50 (sss.h:39).

## Call order

The full steps, a material table and worked examples are in `AGENTS.md` "Call order: sss (transmission)":

1. At load: one `sh4der_sss_t` per material, from `sh4der_sss_init`.
2. Each frame: the light direction.
3. Per vertex: `sh4der_sss_rgb` with a path (thickness), coverage, entry and visibility.
4. Put the result in the offset colour, or add it to the vertex colour. Clamp it to 0..1 when you pack it.

Water has a wrapper, `sh4der_water_crest_sss`; see [water](water.md).

## Cost

As-stated counts (README:792; sss.h:33). There are no times.

| Item | Cost |
|---|---|
| RAM | the 40-byte `sh4der_sss_t` per material |
| CPU per vertex | two 3-term dots (one when distortion is 0), three `shz_expf`, about 12 multiplies or FMACs |
| VRAM, sound RAM | 0 |

## Recommended limits

- The weights run from 0 to 1. No other limit is stated.

## Tuning

There is no `SH4DER_SSS_*` macro table. The starting values per material are in `AGENTS.md` "Starting points per material". They are invented starting values, not measured and not from reference data.

## Pitfalls

From `AGENTS.md` "Where it looks best on the PVR":

- The result is per vertex. A thin part on three large triangles smears the glow; give it enough vertices.
- It is not for thick opaque bodies, front-lit scenes, or flat-shaded low-poly models.
- Pair it with a shadow term (`visibility`). Without one, light shines through surfaces the light cannot reach.
- Signs: `view` runs from the surface to the eye, `light` from the surface to the light. If the glow never fades as the light moves in front, one of them points the wrong way.
- If the glow never shows, `path` is too large for the absorption.

## Related

- [water](water.md), [ao](ao.md), [sh4dow-dc](sh4dow-dc.md) (its heightfield visibility fits `visibility`)
