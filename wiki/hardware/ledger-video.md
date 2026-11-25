---
title: Ledger: video memory and the frame
tags: [hardware, ledger, video]
updated: 2026-11-25
sources: [README.md, demo/psm/ROUTE_AB_NOTES.md, third_party/dc-forerunner/docs/GRAPHICS.md, include/sh4der_ca.h, include/sh4der_trail.h]
---
# Ledger: video memory and the frame

**What you will learn:** how much video memory a frame and its targets take, and what reading the frame back costs.
**For:** a programmer planning a video memory budget, or reading pixels from the screen.

Each row is one fact; the status legend is in [STRUCTURE.md](../STRUCTURE.md). The Dreamcast has 8 MB of video memory, shared by the frame buffers, the TA's vertex and tile buffers, and textures.

| ID | Claim | Status | Evidence | Source | Test | Used by |
|---|---|---|---|---|---|---|
| VID-001 | A 16-bit frame texture takes 640 KB at 640 × 480 (640 × 512 rows) and 160 KB at 320 × 240. | FACT (arithmetic in the code) | — | `include/sh4der_ca.h:11-12` | — | [ca](../effects/ca.md), [warp](../effects/warp.md) |
| VID-002 | A render target's size is pitch × height × 2 bytes. Without `pvr_scene_begin_rtt` the pitch is 1,024, so a 256 × 128 target takes 4 times the memory. | FACT | — | README.md "The pl4nar effect"; AGENTS.md "Call order: pl4nar" | — | [pl4nar](../effects/pl4nar.md) |
| VID-003 | All stride textures share one stride register; trail and ca cannot use it at once. | FACT (code) | — | `include/sh4der_trail.h` top comment | — | trail, ca |
| VID-004 | Free texture memory after `pvr_init` at 640 × 480 was 1,831 KB (AA off) and 1,817 KB (AA 2X), with 2 MB of TA vertex banks, in one port's boot logs. | REPORTED (another project's console logs, not this repository's) | — | `third_party/dc-forerunner/docs/GRAPHICS.md:42-44` | print `pvr_mem_available()` after load | [budgets](../limits/budgets.md) |
| VID-005 | Reading the frame on an 80 × 60 grid took 1,196 us on a console in the same port. Screen reads come back black on an emulator unless it copies the drawn frame into video memory. | REPORTED (the port's console log); the black read is REPORTED (emulator, not evidence) | — | GRAPHICS.md:22, 166; README.md:1106 | time `sh4der_frame` on our console | look, glow, shafts |
| VID-006 | The frame on screen is one or two frames old, and which one varies, so a "last frame" correction pulses unless each frame is stamped. | FACT (library design, README) | — | README.md:1104-1106 | — | [frame](../effects/frame.md) |
| VID-007 | A negative lit vertex colour wraps into other channels when packed (negative blue shows yellow). Clamp at 0 before packing. | REPORTED (the port's console) | — | GRAPHICS.md:153, 158 | — | every vertex colour |
| VID-008 | ARGB4444 has 16 alpha levels and shows steps on smooth alpha ramps. | FACT (format) | — | GRAPHICS.md:19 | — | texture choice |
| VID-009 | An unaligned 4-byte load or store is an address error on the SH-4: the console stops with nothing on screen. An emulator reads it anyway. | FACT | — | README.md:1306 | — | colour writers (stride a multiple of 4) |
| VID-010 | AA 2X renders 1,280 pixels wide; screen-space x must be scaled exactly once. | FACT (library design) | — | README.md:632 | — | [torch](../effects/torch.md) |
| VID-011 | The town demo's TA line (`F… TA est E used U max M P% of B`) gives this frame's estimate, KOS's last frame, KOS's largest frame since boot, and the largest as a share of the 1,024 KB vertex buffer. Town, one view: route A estimate 693-694 KB (68 % of the buffer); routes B and AB 408 KB (40 %). KOS's largest frame since boot had filled the buffer (1,024 KB, 100 %) in every photo. KOS's "last frame" read 56 K to 966 K on route A for the same view, which is not explained. An earlier "about 94 % in one view" has no photo behind it. | MEASURED (as printed, one console, 2026-11-25); the "used" spread OPEN | photos `logs/benchmarks/older/PXL_20261125_0701*.jpg` | format: `demo/psm/psm_send.h:444-456` | log `pvr_get_stats` beside the estimate each frame | [budgets](../limits/budgets.md) |

| VID-012 | Town demo builds a3-b512 and a3e: video memory free 4,554 KB after `pvr_init` and 4,010 KB after load, with vertex buffers 2 × 1,024 KB. The two boot logs match. | MEASURED | 2026-11-24 21:49 and 2026-11-25 01:51, one console, boot logs `20261124-sh4der-psm-town-a3-b512-2149.txt`, `20261125-sh4der-psm-town-a3e-0151.txt` | `demo/psm/TOWN_NOTES.md` TOWN-002 | the boot status line in `psm_dc.c` | [budgets](../limits/budgets.md) |
| VID-013 | The field scene (build b3) at boot: video memory free 4,266 KB. | MEASURED | 2026-11-24 20:37, one console, boot log `20261124-sh4der-psm-ab-b3-2037.txt` | TOWN_NOTES.md TOWN-003 | same | budgets |

## Open

Also in [open-questions.md](../open-questions.md): VID-005 on our own console; the meaning of VID-011's "last frame" spread. VID-004's port figure is superseded for our builds by VID-012 (different settings, so not a contradiction).
