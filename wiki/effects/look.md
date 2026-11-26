---
title: Exposure and grade [look]
tags: [effect, look, post]
updated: 2026-11-26
sources: [include/sh4der_look.h, README.md "The look effect", README.md "Recommended settings", AGENTS.md "Call order: look"]
---
# Exposure and grade [look]

**What you will learn:** how the look effect adapts exposure, grades the picture and adds fog glare, and what each part costs.
**For:** a programmer who wants auto exposure and a colour grade over the whole screen.

## What it does

Maths only, in a header (README:386-394; look.h:8-60):

- an **eye** that adapts to the frame's brightness: fast into light, slow into dark;
- a **grade**, a straight line per colour channel, turned into at most 6 full-screen layers;
- **fog glare**, which bends a copy of the client's fog colour and table.

The client reads the pixels, draws the quads and loads the fog. The read uses [Frame read](frame.md).

Version: v0.50, runs on screen on the demo disc (look.h:36).

## Call order

The full steps are in `AGENTS.md` "Call order: look" and README:396-410:

1. At load: `sh4der_look_bins`, `sh4der_look_params_default`, `sh4der_look_eye_reset`.
2. Each frame: read the frame on the grid, count `sh4der_look_luma565` bins, take `sh4der_look_log_average`.
3. `sh4der_look_eye_step` gives the gain. Put it in a grade, call `sh4der_look_solve`, and draw each layer full screen under the HUD.
4. Glare: `sh4der_look_glare` on a copy of the fog colour and table, then load them.

## Cost

As-stated (README:431). None of it is measured on a console.

| Item | Cost |
|---|---|
| The read | 4,800 uncached 16-bit loads and a 64-bin count a frame |
| The eye | one log and one exp series a frame |
| The grade | 0 to 6 full-screen quads of 4 vertices, no texture |
| VRAM | 0 |

The read goes through uncached video memory. The h2 bench measured a 16-bit loop from uncached video memory at 3.7 MB/s (MEASURED, one console, 2026-11-25, demo/psm/bench/NOTES.md h2). See the [bus and DMA ledger](../hardware/ledger-bus-dma.md). The read's own time on a console is not measured.

## Recommended limits

- At most 6 layers (README:429).
- Keep the gain between 0.4 and 2.0, because RGB565 bands past that (README:1219; look.h:45-58). `GAIN_HI` defaults to 1.5 (README:424).
- The "Recommended settings" (README:1213-1226; look.h:45-58) were found by eye by the author. They are not measured on hardware.

## Tuning

See `README.md` "Look tuning" (README:412-429) and "Recommended settings" (README:1213-1226).

## Pitfalls

- The log and exp are short series on both paths. SH4ZAM's fast forms read log(1) as 0.040 and exp(-0.012) as 0.963, which gives a grey cast and a drift (README:394).
- Hold the eye while a flashlight is lit (look.h:45-58).
- Hand the step a read with the gain taken off, or the eye chases itself (look.h:45-58). [Frame read](frame.md) takes it off.
- Draw the layers under the HUD, depth always, no depth write (README:410).

## Open

- The time of the 80 × 60 frame read on a console.

## Related

- [Frame read](frame.md), [Screen bloom](glow.md), [Screen spotlight](torch.md), [Screen filters](visor.md)
- [Bus and DMA ledger](../hardware/ledger-bus-dma.md)
