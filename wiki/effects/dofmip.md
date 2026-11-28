---
title: "Focus pull by texture blur [dofmip] (depth of field by mip bias)"
tags: [effect, focus, dofmip, camera, texture, mip]
updated: 2026-11-27
sources: [include/sh4der_dofmip.h, test/dofmip_test.c, include/sh4der_fakeaf.h, README.md "The dofmip effect", wiki/hardware/ledger-pvr.md PVR-180]
---
# Focus pull by texture blur [dofmip]: depth of field by mip bias

**What you will learn:** how a camera's focus distance, sharp range, near and far blur strengths and falloff turn into one number a polygon, what that blurs and what it cannot, and what has and has not been measured.
**For:** a programmer with a cutscene or close-up camera that keys a focus pull, who wants a depth-driven blur that costs no pass and no video memory.

> **In plain words.** A photo taken with a wide lens aperture has one distance that is sharp and everything nearer or farther goes soft. The PVR can soften a texture by reading a smaller copy of it (a mip level), and each polygon can ask for a little more softness than usual. This header works out, for each polygon or object, how much softness its distance from the focus asks for, and gives the number the polygon header takes. Textures go soft. Flat colours, outlines and silhouettes stay sharp, so on art with little texture the effect is faint. Nothing has been run on a console yet.

## Try this first

What it is for: a focus pull or a soft background behind a close-up, with no extra pass.

1. Once a frame, from the camera's keyed tracks: `sh4der_dofmip_prepare(&d, focus, range, near_strength, far_strength, curve, reach)`. `focus` and `range` are metres. The two strengths are levels of blur at full for the near side and the far side (start with 1.0 to 2.0; each is held to `SH4DER_DOFMIP_LEVELS_MAX` = 2.75). `curve` is the ramp's power (0 takes `SH4DER_DOFMIP_CURVE` = 1.0, a straight ramp; held to 0.1 .. 50). `reach` 0 takes `SH4DER_DOFMIP_REACH` = 1.0 (README "Dofmip tuning"; ADVISED, invented starting values). Nothing is kept between frames: a track that jumps changes the settings in the frame it is given, and any easing is yours.
2. Per polygon (the depth of its middle) or per object (its centre): `sh4der_dofmip_depth(eye, forward, point)`, then `sh4der_dofmip_code(&d, z)`.
3. Batch by `sh4der_dofmip_bucket(code)`, one polygon header a bucket (12 buckets), with the code as the header's mip bias. The textures need mip maps.
4. On a floor that also takes fakeaf's sharper code, `sh4der_dofmip_combine(faf_code, code)`.
5. Look at it before you trust it, and read the mip level back on a console (see Maturity).

Header: [include/sh4der_dofmip.h](../../include/sh4der_dofmip.h). Call order: [below](#call-order).

## What it does

The PVR picks a texture's mip level from the polygon's footprint, then adds the mip bias in the polygon header (a code of 1 .. 15, a quarter level each; 4 is 1.00, none; over 4 is blurrier). This header only chooses the code.

The sharp zone is from `focus - range` to `focus + range` metres along the view. Inside it the code is 4 (none). Outside it, the blur rises with the change in 1 / z from the zone's edge, as a thin lens's blur circle does, so a near object blurs fast and a far wall slowly. It reaches its side's strength (`near_strength` before the zone, `far_strength` past it) when that change is `reach` times 1 / focus, and no further. `curve` is the power of that ramp: above 1.0 the blur holds back near the zone and rises late, under 1.0 most of it comes early. The power is the header's own square-root chain, not SH4ZAM's `shz_powf`, which is coarse near 1 (`shz_powf(0.99, 50)` gives 3.6, the true value is 0.605; host, 2026-11-27). A polygon takes no power: prepare turns the 11 code steps of each side into shares, and a polygon's code is found in 4 compares. With `range` 0 and `reach` 1, the blur is full at infinity and full at half the focus distance. The sharp zone's near end is never closer than `SH4DER_DOFMIP_NEAR_MIN` = 0.1 m.

Header only, all `static inline`; the reciprocals are SH4ZAM's on the console (`SH4DER_CORE_NO_SH4ZAM` for plain C).

## Call order

1. Once a frame: `sh4der_dofmip_prepare`. A 0 return means a number could not be used, and the focus then blurs nothing.
2. Per polygon or object: `sh4der_dofmip_depth`, `sh4der_dofmip_code` (or `sh4der_dofmip_levels` then `sh4der_dofmip_code_of`).
3. Batch with `sh4der_dofmip_bucket`; send one header a bucket.

## Cost

| Item | Cost | Status |
|---|---|---|
| CPU a polygon or object | one reciprocal (FSRRA), a few multiplies and 4 compares | ESTIMATE, not measured |
| CPU a frame (prepare) | 22 steps of about 14 square roots each; none with a straight ramp | ESTIMATE, not measured |
| State | 116 bytes (`sh4der_dofmip_t`) | exact, by the code |
| Extra passes, video memory | none | by design |
| The PVR's cost of a blurrier bias | not known; a sharper one cost +1.0 to +1.5 ms over bilinear (PVR-180), why OPEN | NOT MEASURED |

## Recommended limits

- Textures only. A flat vertex colour, an outline and a silhouette stay sharp. For a blur that includes edges, the frame has to be blurred (see [focus](focus.md) and its limits).
- Most blur is 2.75 levels, about 6 texels at the base level. It is not a wide bokeh. A strength above that is held at 2.75: the field ends at code 15. A wider blur needs the render-target route.
- A step of one quarter level can show where two neighbouring polygons round to different codes. Whether it shows while the focus moves is OPEN.
- Do not call the blur proven. The level the PVR picks for a bias code has never been read back (OPEN, fakeaf, PVR-180).

## Pitfalls

- The textures need mip maps. A texture with none cannot blur this way.
- The depth is along the view (the dot with the camera's forward), not the distance to the eye. Use `sh4der_dofmip_depth`.
- A depth at or behind the eye gives no blur.

## Maturity

PROTOTYPE v0.2. Host test: `test/dofmip_test.c`, on the plain path and on SH4ZAM's software path (both pass, 2026-11-27); every console result is NOT RUN. To prove it: a textured plane in depth, the focus swept, the mip level read back, and the PVR time with the codes against bilinear, one console.

## Related

- [fakeaf](fakeaf.md) (the same bias field, sharper on a floor), [Fake focus blur [focus]](focus.md) (a region blur that does include edges), [Camera and screen guide](../start-here/genre-camera-and-screen.md)
