---
title: Light through thin parts [sss] (back-lit transmission)
tags: [effect, sss]
updated: 2026-11-26
sources: [include/sh4der_sss.h, README.md "The sss effect", AGENTS.md "Call order: sss (transmission)"]
---
# Light through thin parts [sss]: back-lit transmission

**What you will learn:** what the sss call gives you, what it does not, and where its starting values come from.
**For:** a programmer who wants light through a thin part of a model: an ear against the sun, a leaf, wax, ice, a wave crest.

> **In plain words.** Light that shines through a thin edge of a thing makes that edge glow: an ear held up to a lamp, a leaf against the sun, wax, or the top of a wave. This effect works out the glow at each corner of a model and gives back a colour to add to the picture. It is a quick trick, not a real light simulation: nothing spreads through the material. Use it on thin edges that have a light behind them, because a surface lit from the front shows nothing.

## Try this first

**What it is for:** a soft glow on a thin edge that faces a light behind it.

1. **Start from a row.** Skin: absorption 0.5, 1.8, 3.0; wrap 0.5; distortion 0.3 (AGENTS.md:259, "Starting points per material"). Not a tested value: AGENTS.md:255 calls the table's values invented starting values, not measured and not from reference data, so tune them by eye. Tint: the light's own colour (AGENTS.md, "Call order: sss", step 1).
2. **Build it at load.** `sh4der_sss_init(&p, tint, absorption, gain, wrap, distortion)` (include/sh4der_sss.h:87). The skin's gain is 0.4 to 0.8 (AGENTS.md:259). That is a range, not one value: no single recommended gain yet.
3. **Feed each vertex.** `sh4der_sss_rgb(&p, view, light, normal, path, coverage, entry, visibility, out)` (include/sh4der_sss.h:114). Coverage is 1 on the thin part and 0 on the thick part. Entry and visibility are 1 when you have no Fresnel or shadow term (AGENTS.md, "Call order: sss", step 3).
4. **Set the path.** The path is a thickness in units of 1 / absorption. No recommended value yet. The table's values suit a thickness of 0 to about 1, and a larger model is scaled down (AGENTS.md, worked examples). Test the glow on the thin part against a front-lit view.
5. **Check the sign.** The glow must fade to nothing as the light moves in front. If it never fades, `view` or `light` points the wrong way (AGENTS.md, worked examples).

Run on a console by the owner in Lamp with a lit shade and Sunset sea (her test; no measured numbers recorded yet). The page gives "There are no times" (Cost), and no ledger row covers sss.

Read next: [the header](../../include/sh4der_sss.h), [the call order](../../AGENTS.md#call-order-sss-transmission).

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

Water has a wrapper, `sh4der_water_crest_sss`; see [Water waves](water.md).

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

- [Water waves](water.md), [Corner darkening](ao.md), [Soft character shadows](sh4dow.md) (its heightfield visibility fits `visibility`)
