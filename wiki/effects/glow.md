---
title: "Screen bloom [glow]: light from the bright parts, and god rays"
tags: [effect, glow, post]
updated: 2026-11-26
sources: [include/sh4der_glow.h, src/sh4der_glow.c, README.md "The glow effect"]
---
# Screen bloom [glow]: light from the bright parts, and god rays

**What you will learn:** how the screen bloom is built from a small map of the frame, and how to stop it feeding itself.
**For:** a programmer who wants bright lights to bleed into the picture, or sun shafts.

> **In plain words.** Bright parts of the picture bleed light into the darker parts round them, the way a bright lamp looks in a photo. The sun's rays are copies of the bright spots, stretched away from the sun. It works from a small copy of the picture that the console reads back (a [frame read](frame.md)), so it reads the picture each frame. Use it for lamps, neon, a low sun or a white-hot blast, and keep the on-screen numbers and icons (the HUD) out of it.

## Try this first

For bright lights and the sun to bleed light into the picture, from one small map of the frame.

1. Start with the bloom alone, no rays: the header's own starting set (include/sh4der_glow.h:7-9, RECOMMENDED).
2. `SH4DER_GLOW_THRESHOLD` = 0.62, the brightness where the glow starts (include/sh4der_glow.h:113; ADVISED, README:486). The header's note for the author's own game uses a straight ramp from 0.80 instead (include/sh4der_glow.h:68-70).
3. `SH4DER_GLOW_POWER` = 2 (include/sh4der_glow.h:114; ADVISED, README:487).
4. `SH4DER_GLOW_STRENGTH` = 0.45 and `SH4DER_GLOW_RADIUS` = 0.5 (include/sh4der_glow.h:115-116; ADVISED, README:488-489).
5. `SH4DER_GLOW_DARKEN_MAX` = 0.25 (include/sh4der_glow.h:117; ADVISED, README:490). Never draw the darken without the bloom, or the scene's mean brightness drops (include/sh4der_glow.h:16).

On a console: the page says "As-stated. None of it is measured on a console." The header says "ADVISED; to prove: the demo disc's glow row on and off, then rays on, PVR time" (include/sh4der_glow.h:13).

Header: [sh4der_glow.h](../../include/sh4der_glow.h). Steps in full: [Call order](#call-order).

## What it does

A small bright map of the frame: the frame read's 80 × 60 grid, in a 128 × 64 RGB565 texture. Dark texels die and bright ones stay. From it (README:449-451; glow.h:8-45):

- the bloom itself;
- a wide 20 × 15 halo;
- a darken layer balanced so a neutral scene keeps its mean brightness;
- god rays, as scaled copies about the sun.

`src/sh4der_glow.c` is KallistiOS PVR drawing, Dreamcast only. The header holds the maths and a RAM staging struct.

Version: v0.50, runs on screen on the demo disc (glow.h:46).

## Call order

The order is in glow.h:~34-36: upload, `draw_halo`, `draw_rays` or the shaft. `AGENTS.md` has no section of its own. Recommended settings: glow.h:55-75 and README:1220-1221.

## Cost

As-stated. None of it is measured on a console.

| Item | Cost | Source |
|---|---|---|
| RAM staging | 16 KB + 1 KB | glow.h:288-289 |
| VRAM | a 128 × 64 RGB565 texture (16 KB) and a 32 × 16 one | glow.h:288-289 |
| Bloom above 1 | drawn in passes of at most 1 each (the demo uses up to 3) | glow.h:~61 |
| Rays | each copy is a layer; no time stated | |
| Darken | capped at 0.25 | README:469 |

The init returns 0 when video memory is short (glow.h:294). Each full-screen translucent layer costs the PVR about 3.1 ms at 640 × 480 (MEASURED, one console, 2026-11-25, h1 and Compare by blending [c1]; see the [PVR ledger](../hardware/ledger-pvr.md)).

## Recommended limits

- `RAYS` 3 to 6; 6 is the most (README:470; glow.h:~65).
- `POWER` a whole number from 1 to 3 (README:466).
- A useful threshold lies between 0.20 and 0.90 (glow.h:~56).
- Texture sides must be powers of two (glow.h:~12).
- Draw rays only with the sun in front (README:1221).

## Tuning

See `README.md` "Glow tuning" (README:453-478).

## Pitfalls

- Take the frame's own glow and gain off the read, with `sh4der_glow_unfeed` and `sh4der_frame_degain`. If you do not, the glow feeds itself (glow.h:~66).
- Keep anything that must not glow, such as a HUD, under the threshold, or draw it after (glow.h:~68).
- `keep` at 128 stops a bright spot pulsing (glow.h:~64).
- The unfeed uses the nearest texel where the PVR drew bilinear, so it is an estimate (glow.h:152).

## Open

- A "GPU path" (render small, multiply with itself) is described as an alternative with the same maths (glow.h:~30). Nothing says it is built or tested.
- The cost of the glow on a console.

## Related

- [Frame read](frame.md), [Mesh bloom](bloom.md) (the glow with no frame read), [Dark shafts of light](shafts.md) (the dark twin)
- [PVR ledger](../hardware/ledger-pvr.md)
