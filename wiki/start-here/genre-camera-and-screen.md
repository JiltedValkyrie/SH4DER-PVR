---
title: "Genre guide: camera and screen"
tags: [genre, guide, camera, screen, post, effect]
updated: 2026-11-26
sources: [wiki/STRUCTURE.md, wiki/effects/names.md, wiki/start-here/what-do-you-want.md, wiki/glossary.md, wiki/limits/header-recommended.md, wiki/limits/recommended-limits.md, wiki/limits/budgets.md, wiki/status/maturity.md, wiki/hardware/ledger-video.md, wiki/hardware/ledger-pvr.md, wiki/hardware/ledger-bus-dma.md, wiki/open-questions.md, wiki/effects/look.md, wiki/effects/taa.md, wiki/effects/trail.md, wiki/effects/lens.md, wiki/effects/warp.md, wiki/effects/hexheat.md, wiki/effects/ca.md, wiki/effects/film.md, wiki/effects/timebase.md, wiki/effects/visor.md, wiki/effects/focus.md, wiki/effects/frame.md, wiki/research/heat-haze-reference.md, include/sh4der_look.h, include/sh4der_taa.h, include/sh4der_trail.h, include/sh4der_lens.h, include/sh4der_warp.h, include/sh4der_hexheat.h, include/sh4der_ca.h, include/sh4der_film.h, include/sh4der_timebase.h, include/sh4der_visor.h, include/sh4der_focus.h, src/sh4der_warp.c, README.md, AGENTS.md]
---
# Genre guide: camera and screen

**What you will learn:** which screen effect fits the look you want, what each one starts from, and what it costs.
**For:** a programmer adding a camera feel or a screen look to a Dreamcast program that uses SH4DER-PVR.

These effects change the whole picture, or part of it, after the world is drawn. Most of them have no console timing yet. Read the status tag beside each number before you trust it. Reader names follow the [names page](../effects/names.md), section 0.

**Ghosting.** TAA's blend gives a faint trail of the last frame: [Ghosting with TAA](../guides/horror-night-game.md#ghosting-with-taa) in the horror and night recipe. The Match Light short film demo uses it: [demo/matchlight](../../demo/matchlight/).

## What you can make

A player's eyes can adjust when they walk from a dark room into sun. A boost can get a speed blur, a cutscene can get smoother edges, and a close-up can keep one face sharp. A horror scene can get dark corners and grain, and a flashback can get worn film or a tape wobble. Most of these cover the whole screen, so each one takes part of the frame time.

## The effects

Three of these work on the picture itself. [look](../effects/look.md) reads the frame on screen back on a grid (the [frame read](../effects/frame.md)). [trail](../effects/trail.md) copies the front buffer, which is the picture already on screen. [ca](../effects/ca.md) draws the world into a render target, a picture the chip reads back as a texture. Other terms are in the [glossary](../glossary.md).

### Exposure and grade [look]

**What it looks like:** The picture brightens or darkens as an eye adapts, and colour layers tint, lift or fade it.
**Use it when:** a player walks from a dark place into bright light, or a scene needs a day, night or fade look.
**Where it is:** [header](../../include/sh4der_look.h) and [effect page](../effects/look.md).
**Starting settings:**
- `SH4DER_LOOK_GAIN_HI` = 1.5 (README.md:441). Keep the gain between 0.4 and 2.0, because RGB565 bands past that (sh4der_look.h:45-58). ADVISED (author's eye).
- Eye strength 0.25 on `SH4DER_LOOK_TRANSIENT` and `SH4DER_LOOK_OPEN`; `SH4DER_LOOK_LIGHT_S` = 2.0 and `SH4DER_LOOK_DARK_S` = 6.0 (sh4der_look.h:46-54, 87-88). ADVISED (author's eye, not measured).
- Hold the eye while a torch is lit (sh4der_look.h:55-56). ADVISED.

### Smooth edges over time [taa]

**What it looks like:** Jagged edges soften, because each frame is blended with the one before, shifted by a fraction of a pixel.
**Use it when:** the camera rests, as in a cutscene, a menu backdrop or a photo mode.
**Where it is:** [header](../../include/sh4der_taa.h) and [effect page](../effects/taa.md).
**Starting settings:**
- `SH4DER_TAA_WEIGHT_HI` = 0.75 (README.md:459): the still weight is 0.675 (0.90 times this), and 0.30 while moving (sh4der_taa.h:47-50). ADVISED (author's eye).
- Keep the history weight at 0.9 or below (sh4der_taa.h:57). ADVISED (recommended limits, Effects).
- `SH4DER_TAA_CUT_RAD` = 0.35 (README.md:463): a turn this big in one frame is a cut. ADVISED (no measured number).

### Motion blur [trail]

**What it looks like:** When the camera turns, the picture smears into streaks, because the last frame is laid over this one and moved.
**Use it when:** a boost, a stun or a dizzy view needs a speed blur, or moving things should leave trails.
**Where it is:** [header](../../include/sh4der_trail.h) and [effect page](../effects/trail.md).
**Starting settings:**
- `SH4DER_TRAIL_COPIES` = 3 (README.md:537): history copies, two or three. ADVISED (no measured number).
- `SH4DER_TRAIL_SHIFT_MAX` = 8 (README.md:542): the longest copy offset, in pixels. ADVISED (no measured number).
- Never both at once with [ca](../effects/ca.md): they share one stride register. Hard rule (VID-003, FACT, from the code).
- NOT RUN on a console: the demo disc includes this header and never calls it (maturity.md).

### Screen overlays [lens]

**What it looks like:** Dark corners, moving grain, red wedges at the screen edge that show where a hit came from, and a white flash.
**Use it when:** a horror scene needs dark corners, a cutscene needs grain, or a hit needs a direction marker or a flash.
**Where it is:** [header](../../include/sh4der_lens.h) and [effect page](../effects/lens.md).
**Starting settings:**
- `SH4DER_LENS_VIG_SPOKES` = 32 (README.md:558): a shading error of 2.3 of 255 at 32 spokes. That figure is an ESTIMATE from a host sweep. ADVISED.
- `SH4DER_LENS_GRAIN_AMP` = 0.06 (README.md:564): grain of about plus or minus 3 %. ADVISED (no measured number).
- `SH4DER_LENS_Z_VIG` = 430.0 to `SH4DER_LENS_Z_FLASH` = 960.0 (README.md:584-587): under the HUD at 1000. ADVISED (layout rule).

### Screen warp [warp]

**What it looks like:** The picture ripples out from a point, shimmers like heat, wobbles like water, or breaks into large square pixels.
**Use it when:** an explosion needs a shockwave, a fire needs haze, a level is underwater, or a boost needs a radial blur.
**Where it is:** [header](../../include/sh4der_warp.h) and [effect page](../effects/warp.md).
**Starting settings:**
- `SH4DER_WARP_SOURCES` = 4 (README.md:509) and `SH4DER_WARP_BLUR_COPIES` = 4 (README.md:524), at most. ADVISED (no measured number).
- `SH4DER_WARP_PIXEL_CELL` = 16 (README.md:526), which divides 320 and 240. ADVISED (no measured number).
- `SH4DER_WARP_MESH_SEND_PRIM` = 1 and `SH4DER_WARP_PIXEL_SEND_PRIM` = 0 (src/sh4der_warp.c:67, 70). The pixelate choice is PROVEN. The mesh choice is MEASURED once; why it loses by runs is OPEN (BUS-004, Q9).

### Heat haze hexagons [hexheat]

**Read this before you start.** The street demo has two heat hazes. Its own layer ([heat.h](../../demo/pl4nar/src/heat.h), version 2) is a smooth wobble under a pixel over a copy of the frame. The hexagon effect below is the library's; the demo draws it in a build with `-DPL4NAR_HEAT_HEX` ([effect page](../effects/hexheat.md)). Neither has run on a console. The hexagons' numbers are the owner's start values, not console results.

**What it looks like:** The far end of a hot view shimmers. It is cut into a honeycomb of small hexagons, and a slow, smooth wave nudges each one up to 2 pixels. Nothing pops or flickers.
**Use it when:** a hot far view by day, such as a road's far end or a desert horizon, needs heat, and a spare part of a render target you already draw can hold it.
**Where it is:** [header](../../include/sh4der_hexheat.h) and [effect page](../effects/hexheat.md).
**Starting settings:**
- `SH4DER_HEXHEAT_S` = 8.0 and `SH4DER_HEXHEAT_PEAK` = 0.25 (README.md:1588, 1596): a hexagon 13.9 pixels wide, moved at most a quarter of its size, 2 pixels. ADVISED (the owner's start values; no measured number).
- `SH4DER_HEXHEAT_WAVE_HEX` = 7.0, `SH4DER_HEXHEAT_RISE_HEX` = 1.5 and `SH4DER_HEXHEAT_WIND` = 0.35 (README.md:1590, 1592-1593): the wave's length in hexagon widths, its rise in widths a second, and the wind as a share of the rise. ADVISED (no measured number).
- A band of `SH4DER_HEXHEAT_BAND_W` = 256 by `SH4DER_HEXHEAT_BAND_H` = 128 (sh4der_hexheat.h:116-117), sampled filtered. ADVISED: the header's own start.
- In a driving game, read "Stripes, and the speed where they stand still" on the effect page first: with `SH4DER_HEXHEAT_DRIVE` = 0.5 (README.md:1598) the shimmer stands still at one speed, 30 km/h in the street scene (host test).
- NOT RUN on a console. Every number in the header's block is an ESTIMATE (sh4der_hexheat.h:6-22).

### Colour fringe [ca]

**What it looks like:** Red and blue fringes sit toward the corners of the screen and none in the middle, as a cheap lens makes them.
**Use it when:** a camera or scope needs a colour fringe, or a hit or glitch look is wanted.
**Where it is:** [header](../../include/sh4der_ca.h) and [effect page](../effects/ca.md).
**Starting settings:**
- `SH4DER_CA_SPLIT_PX` = 3.0 (README.md:803): strength 1 puts red and blue 3 pixels apart in the corner. The range is 0.25 to 4.00 (sh4der_ca.h:46-47). ADVISED (author's eye).
- 640 x 480 for the fringe alone; 320 x 240 when a temporal blend rides on it or video memory is short (sh4der_ca.h:48-49). ADVISED (author's eye).
- `SH4DER_CA_TEXTURES` = 2 (README.md:807): a history for a temporal blend. ADVISED (no measured number).

### Old film wear [film]

**What it looks like:** The brightness swings a little, scratches come and go, dust specks appear, and the picture weaves by a pixel.
**Use it when:** a flashback, a title card or an old newsreel inside a scene needs the look of worn film.
**Where it is:** [header](../../include/sh4der_film.h) and [effect page](../effects/film.md).
**Starting settings:**
- `SH4DER_FILM_FLICKER_MAX` = 0.04 and `SH4DER_FILM_RATE_MAX_HZ` = 3.0 (sh4der_film.h:75-76): at most an 8 % brightness swing, peak to peak, at 3 Hz or slower. ADVISED (design limits, not tested on viewers).
- `SH4DER_FILM_SCRATCH_SLOTS` = 3 and `SH4DER_FILM_SPECKS_MAX` = 12 (README.md:1269-1270). ADVISED (no measured number).
- NOT RUN on a console: host tests only (maturity.md).

### Tape wobble [timebase]

**What it looks like:** Rows of the picture slide sideways by different amounts, with a hook at the top and a jump near the bottom, as a worn tape plays.
**Use it when:** a cutscene, a security monitor or a broken broadcast should look as if it plays from video tape.
**Where it is:** [header](../../include/sh4der_timebase.h) and [effect page](../effects/timebase.md).
**Starting settings:**
- The header's block (sh4der_timebase.h:37-39): tension 0.375, flutter 0.135, head_switch_us 0.6, a mesh row every 6 picture rows, an overscan of 8 pixels. ADVISED. NOT RUN on a console; the ledger has no row.
- `SH4DER_CA_ROWS_MESH_X` = 2 (README.md:806): cells across the banded mesh. ADVISED (no measured number).
- Required credit, not a setting: keep the NTSCRT notice in the header, and credit NTSCRT and its author, Finn McKenty (README.md:1277-1278; [licence and credit](license-and-credit.md)).

### Screen filters [visor]

**What it looks like:** The picture turns into green night vision, amber or thermal, with grain, scanlines and a vignette, and it fades in and out.
**Use it when:** goggles, a thermal scope or an old security monitor needs a device's look.
**Where it is:** [header](../../include/sh4der_visor.h) and [effect page](../effects/visor.md).
**Starting settings:**
- `SH4DER_VISOR_FADE_IN_S` = 0.25 and `SH4DER_VISOR_FADE_OUT_S` = 0.40 (README.md:623-624): seconds from off to on, and on to off. ADVISED (no measured number).
- `SH4DER_VISOR_VIG_R` = 280.0 and `SH4DER_VISOR_VIG_FEATHER` = 200.0 (README.md:630-631): the clear disc and its falloff, in pixels. ADVISED (no measured number).
- NOT RUN on a console: no program uses it (maturity.md).

### Fake focus blur [focus]

**What it looks like:** One band, disc or rectangle stays sharp while the rest of the picture goes soft.
**Use it when:** a dialogue close-up or a blurred backdrop needs one part sharp. It reads no depth, so the focus is a fake.
**Where it is:** [header](../../include/sh4der_focus.h) and [effect page](../effects/focus.md).
**Starting settings:**
- `SH4DER_FOCUS_FEATHER` = 48.0 (README.md:607): pixels over which the sharp part falls to blur. ADVISED (no measured number).
- `SH4DER_FOCUS_DISC_R` = 120.0, `SH4DER_FOCUS_BAND_H` = 60.0, `SH4DER_FOCUS_RECT_W` = 100.0 and `SH4DER_FOCUS_RECT_H` = 80.0 (README.md:609-612): the shape sizes, in pixels. ADVISED (no measured number).
- `SH4DER_FOCUS_TEX_W` = 128 and `SH4DER_FOCUS_TEX_H` = 64 (README.md:601-602): powers of two. ADVISED (the effect page's limits).
- NOT RUN on a console: no program uses it (maturity.md).

## What it costs

| Effect | Video memory | Triangles or vertices | SH-4 time | Status |
|---|---|---|---|---|
| [look](../effects/look.md) | 0 (FACT, README.md:448) | 0 to 6 full-screen quads of 4 vertices (FACT, README.md:448) | Read: 4,800 uncached loads a frame (FACT). Read time not measured (OPEN: time `sh4der_frame` on a console) | Runs on the demo disc (REPORTED, README.md:135); cost OPEN |
| [taa](../effects/taa.md) | 320 KB for two history textures at 320 x 240 (FACT, arithmetic, sh4der_taa.h:62-63) | Its layers are drawn by trail; not counted | Not measured (OPEN: time the blend on a console) | Runs on the demo disc (REPORTED); no row |
| [trail](../effects/trail.md) | 128 B mask (FACT, sh4der_trail.h:157); history from the front buffer, 0 B | 8 full-screen layers at the default 3 copies (FACT count, sh4der_trail.h:26-28, 67) | Not measured (OPEN) | NOT RUN on a console |
| [lens](../effects/lens.md) | 8 KB grain tile, 64 x 64 x 2 B (FACT, arithmetic, sh4der_lens.h:111) | Vignette: 32 spokes by 8 rings by default (FACT, sh4der_lens.h:106-107); vertex count 594 and 288 triangles (`src/sh4der_lens.c:19`) | Not measured (OPEN) | No console row (maturity.md). The 2.3 of 255 shading error is ESTIMATE (host sweep) |
| [warp](../effects/warp.md) | 256 KB: a 512 x 256 RGB565 texture (FACT, arithmetic, sh4der_warp.h:13-14) | Mesh 40 x 30: 2,460 vertices; pixelate pass: 1,200 (MEASURED, h2) | Mesh 2,250 us by `pvr_prim`, 2,394 us by runs; pixelate 440 us by `pvr_prim`, 290 us by runs (MEASURED, h2, least of 8 frames) | "Measured on a console. h2 bench, one console, one run, 2026-11-25." PVR render: mesh 6,466 us, pixelate 3,183 us (least, same run) |
| [hexheat](../effects/hexheat.md) | 64 KB: a 256 x 128 band at 16 bits (FACT, arithmetic, sh4der_hexheat.h:116-117) | 131 hexagons of 4 triangles a frame (786 vertices as strips); the street demo's far world sent again, mean 599 triangles a frame (ESTIMATE, host count) | About 180 cells walked a frame, a ray each; not measured (OPEN) | NOT RUN on a console; every count is a host ESTIMATE (2026-11-26) |
| [ca](../effects/ca.md) | 640 KB at 640 x 480, or 160 KB at 320 x 240, per texture (FACT, VID-001) | Three meshes of 63 vertices (FACT count, sh4der_ca.h:28-29) | Not timed (OPEN: a multiply and an add per vertex) | Runs on the demo disc (REPORTED); second scene OPEN |
| [film](../effects/film.md) | None of its own; 92-byte parameter block in RAM (FACT, sh4der_film.h:85) | One multiply rectangle, up to 3 lines and 12 specks a frame (FACT count, sh4der_film.h:73-74) | Two sines a frame (FACT count); not measured (OPEN) | NOT RUN on a console |
| [timebase](../effects/timebase.md) | None of its own; frame texture 640 KB at 640 x 480 (FACT, VID-001) | 4 vertices a mesh row a channel (FACT count, sh4der_timebase.h:42-43) | About 4.2 KB of stack, 40 B of RAM and 4 B of state (FACT, header counts); not measured (OPEN) | ADVISED; NOT RUN in the ledger |
| [visor](../effects/visor.md) | Grain 2,048 B and scanline 128 B (FACT, sh4der_visor.h:79-80); thermal map is the focus map, 16 KB | Not stated (drawn through the look layers) | Thermal ramp 128 B of RAM (FACT, sh4der_visor.h:81); not measured (OPEN) | NOT RUN on a console (no program uses it) |
| [focus](../effects/focus.md) | 16,384 B: a 128 x 64 RGB565 blur texture (FACT, sh4der_focus.h:78) | 408 strip vertices and 384 triangles a frame (FACT, sh4der_focus.h:80-81) | The box blur runs on the SH-4; not measured (OPEN); 1,440 B of RAM (FACT, focus.md) | NOT RUN on a console (no program uses it) |

Video memory is the picture memory the graphics chip keeps its textures and drawing targets in, and triangles are the flat shapes sent to it, so a bigger count means more work for the chip.

Trail's layers, the film rectangle and the lens flash are full-screen layers. Their PVR time is an ESTIMATE from PVR-002, which measured about 3,112 us for one such layer at 640 x 480 (MEASURED, [PVR ledger](../hardware/ledger-pvr.md)).

## Try this first

Start with exposure and grade [look], which is header only. The full order is under "Call order: look" in AGENTS.md; see [call orders](call-orders.md).

1. Include `include/sh4der_look.h` and the frame read, `include/sh4der_frame.h` ([frame](../effects/frame.md)). The frame read's Dreamcast part is `src/sh4der_frame.c`.
2. At load, call `sh4der_look_bins`, `sh4der_look_params_default` and `sh4der_look_eye_reset`.
3. Each frame, read the picture on screen on the 80 x 60 grid, count each sample's luma bin with `sh4der_look_luma565`, and take `sh4der_look_log_average`.
4. Call `sh4der_look_eye_step` with that read and the frame time. Put the gain in a grade's exposure, call `sh4der_look_solve`, and draw each layer full screen under the HUD, with depth test always and no depth write.
5. Check it on a console. Walk from a dark room into sun: the picture should brighten and reach 95 % of the change in about 2 seconds (`SH4DER_LOOK_LIGHT_S` = 2.0). If it pulses, the read still carries last frame's gain (sh4der_look.h:55-58).

## Related

- [What do you want to make?](what-do-you-want.md)
- [effect index](../effects/README.md)
- [recommended limits](../limits/recommended-limits.md)
- [glossary](../glossary.md)
- [licence and credit](license-and-credit.md)
- [demo guide](demo-guide.md)
- [call orders](call-orders.md)
