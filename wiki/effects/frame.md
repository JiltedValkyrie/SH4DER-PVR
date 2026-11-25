---
title: frame (the frame read)
tags: [effect, frame, post]
updated: 2026-11-25
sources: [include/sh4der_frame.h, src/sh4der_frame.c, README.md "The frame read"]
---
# frame: the frame read

**What you will learn:** how the library reads the finished frame back, and why each frame carries a number.
**For:** a programmer using [look](look.md), [glow](glow.md), [shafts](shafts.md), [focus](focus.md) or [ao](ao.md), which all measure the picture.

## What it does

It reads the frame on screen back on an 80 × 60 grid. Each frame also carries a 4 × 4-pixel stamp holding a number from 0 to 7. So a read knows which frame it saw, and can take that frame's gain and glow off (README:1104-1106; frame.h:8-50).

The stamp matters because the frame on screen is 1 or 2 frames old, and which one varies. A correction that assumes "last frame" without the stamp pulses (README:1106).

`src/sh4der_frame.c` does the scan-out read and the stamp block, on the Dreamcast only (frame.c:25, :49). The rest is inline maths.

Version: v0.50, runs on screen on the demo disc (frame.h:37).

## Call order

See README:1104-1118 "The frame read" and the order in frame.h:~28-35. `AGENTS.md` has no section of its own.

## Cost

| Item | Cost | Status |
|---|---|---|
| Stamp | a 4 × 4-pixel block | as-stated |
| History | 8 frames (the stamp's 3 bits) | as-stated (README:1127; frame.h:68) |
| The read | 4,800 uncached loads (see [look](look.md)) | as-stated (README:431) |
| A 16-bit loop from uncached video memory | 3.7 MB/s | MEASURED, one console, 2026-11-25 (demo/psm/bench/NOTES.md h2) |
| Reading video memory through the cache | about 38.7 MB/s, but the range must be invalidated first | MEASURED, one console, 2026-11-25 (h2) |

The read's own time on our console is not measured.

## Recommended limits

- The read returns 0 and reads nothing if `fb_w` or `fb_h` is below 2 (frame.c:~24).

## Tuning

See `README.md` "Frame tuning" (README:1120-1128).

## Pitfalls

- Do not read the render target. It may be half drawn (frame.c:~22).
- Draw the stamp last, over everything (frame.h:~28).
- An emulator may show the CPU a black screen unless it copies the drawn frame into video memory (README:1106). That is an emulator gap; a console reads the real frame.
- If you read through the cache instead, invalidate the range before every read. In h2, without the invalidate, 238 of 256 lines read back old data (MEASURED, one console, 2026-11-25).

## Open

- The read's time on a console.
- Whether a cached read pays for the frame read, which reads a sparse grid and not a block.

## Related

- [look](look.md), [glow](glow.md), [shafts](shafts.md)
- [Bus and DMA ledger](../hardware/ledger-bus-dma.md)
