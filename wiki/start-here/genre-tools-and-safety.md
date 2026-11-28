---
title: "Genre guide: tools and safety"
tags: [start, guide, safety, tools, pvr, hardware]
updated: 2026-11-28
sources: [include/sh4der_guard.h, include/sh4der_fault.h, include/sh4der_tilecut.h, include/sh4der_pvrx.h, include/sh4der_ramp.h, include/sh4der_viewcull.h, include/sh4der_split.h, include/sh4der_frame.h, src/sh4der_frame.c, include/sh4der_core.h, tools/ao_bake_core.h, README.md (the tuning tables and "Real hardware notes"), AGENTS.md (Call order: pvrx), wiki/effects/names.md, wiki/effects/guard.md, fault.md, tilecut.md, ramp.md, viewcull.md, split.md, frame.md, core.md, ao-bake.md, wiki/start-here/what-do-you-want.md, call-orders.md, wiki/glossary.md, wiki/limits/header-recommended.md, recommended-limits.md, wiki/hardware/ledger-pvr.md, ledger-sh4.md, ledger-bus-dma.md, ledger-video.md, wiki/status/maturity.md, wiki/open-questions.md, wiki/methods/fault-catcher.md, Makefile]
---
# Genre guide: tools and safety

**What you will learn:** which helpers stop a Dreamcast program from freezing or drawing stray lines, which ones save PVR time, what each costs, and how much console proof backs it.
**For:** a programmer adding SH4DER-PVR to a Dreamcast project, and any agent that wires a helper into a client.

This genre is about your safety and speed more than about pictures. Most parts here are small helpers. Several have no figures of their own yet, and each part says so.

## What you can make

- **A build that does not freeze on the console.** A triangle with no area, or a polygon header never built, can stop a real Dreamcast while an emulator carries on. The send checks stop both.
- **A crash report.** The fault catcher puts the registers and the frame's last state on screen. It is a prototype.
- **A cheaper small target.** A mirror or a shadow tile, drawn through the tiles it covers, costs the PVR less time.
- **Plain helpers.** A 0 to 1 fade between two distances, a movable split screen, and a frame read that knows which frame it saw.

## The effects

### Send checks [guard]
**What it looks like:** nothing on screen. A triangle with no area (its three points lie on one line) is dropped before it is sent.
**Use it when:** you build your own fans, rings, strips or full-screen quads.
**Where it is:** [header](../../include/sh4der_guard.h) and [effect page](../effects/guard.md).
**Starting settings:** the header has no RECOMMENDED block.
- `SH4DER_GUARD_MIN_AREA2` = 1.0e-7 (include/sh4der_guard.h:61). A default. No limit covers it, so it has no ADVISED or PROVEN mark.
- Check every triangle with `sh4der_tri_area_ok` before you send it. FACT: PVR-080, seen on a console (README.md:1304).
- Check every polygon header with `sh4der_poly_hdr_ok`. FACT: PVR-081, a header never compiled ends the list (README.md:1305).

Console count: 21 and 38 zero-area quads dropped in two stages, and 0 in nine (PVR-182, MEASURED, one run; from [maturity](../status/maturity.md)).

### Crash and hang screen [fault]
**What it looks like:** a report that replaces the picture: the fault's kind, the registers, and the frame's breadcrumbs (a small record of what the frame was doing).
**Use it when:** a test image dies and leaves nothing on the screen.
**Where it is:** [header](../../include/sh4der_fault.h) and [effect page](../effects/fault.md).
**Starting settings:**
- `SH4DER_FAULT_HANG_VBLANKS` = 120 (include/sh4der_fault.h:88): a hang is reported after 120 vertical blanks (the short gap between screen refreshes, 2 s at 60 Hz). ADVISED: no console run has shown the watchdog firing.
- Call `sh4der_fault_init` after `pvr_init`, and `sh4der_fault_beat` once a frame. ADVISED (header block).
- Keep `SH4DER_FAULT_TEST` out of builds for players, because it forces faults. ADVISED (header).

Status: PROTOTYPE v0.1. No console run has shown it catching a fault or a hang. PVR-183 and PVR-208 froze with no HANG screen (MEASURED, one run each; the cause is OPEN).

### Tile-cut pass [tilecut]
**What it looks like:** the picture stays the same, and the PVR's time for a small render target falls.
**Use it when:** a render target pass (a mirror, a shadow tile, a small map) covers part of the screen.
**Where it is:** [header](../../include/sh4der_tilecut.h) and [effect page](../effects/tilecut.md).
**Starting settings:**
- `SH4DER_TILECUT_TRIES` = 3 (include/sh4der_tilecut.h:78): read-backs before a bad restore is reported. ADVISED: no run tests this count alone.
- `SH4DER_TILECUT_MAX` = 96 (line 76). The code refuses a bigger cut (line 153). ADVISED: no console run has hit that refusal.
- `SH4DER_TILECUT_TABLES` = 2 (line 75). Cut after both tables are found. ADVISED.
- Wait for render done (the PVR's signal that a scene has finished drawing) after every cut pass (header "Do"). ADVISED.

Console status, quoted from the header's block: "the PVR saving PROVEN (four runs, three programs); the frame gain PROVEN in d1 alone (one run, held at 60 Hz); ADVISED elsewhere: the street demo lost 3.05 ms with one table of two cut (PVR-200). The need = 2 refusal has not run on a console".

### PVR extras [pvrx]
**What it looks like:** nothing on screen. It adds calls over KallistiOS's PVR driver: a polled wait, a DMA copy of video memory to RAM, error counts, and a user clip (a rectangle the PVR draws inside or outside).
**Use it when:** your program waits for the PVR every frame, reads a render target back, or must know a frame ran out of vertex memory.
**Where it is:** [header](../../include/sh4der_pvrx.h). There is no effect page yet; its rows are in the [PVR ledger](../hardware/ledger-pvr.md).
**Starting settings:** the header has no RECOMMENDED block.
- `SH4DER_PVRX_WAIT_US` = 100000u (include/sh4der_pvrx.h:67): how long a polled wait runs. ADVISED: the town's value, and no test sets it.
- `SH4DER_PVRX_DMA_US` = 200000u (line 68): how long a download runs. ADVISED.
- Poll the render-done wait where it runs every frame. PROVEN for the program that waited every frame (PVR-117 and PVR-133, two runs, one console). PVR-136 (one run) found no gain where the wait ran on one frame in six.

Status: the header says "nothing here has run on a console AS THIS LAYER". Its clip change is "NOT run on a console yet".

### Distance ramp [ramp]
**What it looks like:** nothing on its own. It gives a strength from 0 to 1 that eases in between two distances.
**Use it when:** a shadow, a fog or a light should fade with the camera's distance.
**Where it is:** [header](../../include/sh4der_ramp.h) and [effect page](../effects/ramp.md).
**Starting settings:** no tuning macros (README "The ramp helper"). You pick the two distances.
- Start the fade where the effect begins, and end it where it is full (header block). ADVISED.
- For a loop over many points, prepare the ramp once with `sh4der_ramp_make` (header "Do"). ADVISED.
- Do not use it for a strength the PVR must work out per pixel from a texture (header "Do not"). ADVISED.

Status: ADVISED, no figures. To prove it: the same view faded against a hard step, timed for PVR and frame. This part has no row on the [maturity](../status/maturity.md) page.

### Off-screen skip [viewcull]
**What it looks like:** nothing on screen. Turning the camera should cost the SH-4 less, because ground or objects wholly off the screen are left out before their points are placed.
**Use it when:** a ground window, a terrain grid or a list of boxed objects is placed again every frame the camera turns.
**Where it is:** [header](../../include/sh4der_viewcull.h) and [effect page](../effects/viewcull.md).
**Starting settings:**
- `SH4DER_VIEWCULL_BLOCK` = 4 (README "Viewcull tuning"): 4 x 4 cells a block. ADVISED: no console run has tried another size.
- Prepare the view once a frame with `sh4der_viewcull_prepare`, then test a grid with `sh4der_viewcull_grid` or a box with `sh4der_viewcull_box` (header block). ADVISED.
- Never read a placed value of a point in a block that was left out (header "Do not"). ADVISED.

Status: PROTOTYPE v0.1, host tested, NOT RUN on a console. On the host the walled square's ground window places 0 % of its points looking at the sky, 76 % at the boot view and 72 % looking down, with the same picture to the pixel (DERIVED, host counts). This part has no row on the [maturity](../status/maturity.md) page.

### Small world [world bound]
**What it looks like:** the shadow demo's square with its gates shut, a light fog on the far walls, and sharper ground shadows. The ground inside the walls is unchanged.
**Use it when:** your level is a closed place, and its ground or shadow map reaches past the walls where nobody can see.
**Where it is:** the shadow demo (`demo/psm/psm_scene.h`, `PSM_WORLD_BOUND`; `demo/psm/tools/bake_square.py --closed`) and [method page](../methods/small-world.md).
**Starting settings:**
- `PSM_WORLD_BOUND` = 1 (the bit in `world_opt`), `PSM_WORLD_DEFAULT` = 0 (off at load), `PSM_BOUND_DIST_I` = 10 (fog to 75 m, rising from 32 m) (README "The shadow demo's small world"). ESTIMATE: no console run.
- Build with `SQUARE=closed`. The cut does nothing on a scene without walls all round. ADVISED.

Status: PROTOTYPE, host tested, NOT RUN on a console. On the host the ground points placed a frame fall from 5,400 to 1,943 on average, the ground cells sent from 3,948 to 1,185, and the shadow texel to 0.46 of its old size (DERIVED, host counts). This part has no row on the [maturity](../status/maturity.md) page.

### Split screen [split]
**What it looks like:** the screen is cut in two by a divider, and each half shows its own look.
**Use it when:** you compare two looks side by side and the player moves the divider.
**Where it is:** [header](../../include/sh4der_split.h) and [effect page](../effects/split.md).
**Starting settings:**
- `SH4DER_SPLIT_COLS` = 20u, `SH4DER_SPLIT_ROWS` = 15u and `SH4DER_SPLIT_TILE` = 32u (include/sh4der_split.h:46-48), for a 640 x 480 screen. ADVISED.
- `SH4DER_SPLIT_REPEAT` = 6u (line 49): frames between steps while a trigger is held. ADVISED.
- Start the divider at `sh4der_split_middle(SH4DER_SPLIT_COLS)`, which is 10 (line 63). Never send a hidden half.

Console status, quoted from the header: "The middle position works on a console (PVR-144, PVR-161: the split's user clip); a divider that moves has no console run". The middle position is MEASURED, one run each. The moving divider is OPEN (Q95).

### Frame read [frame]
**What it looks like:** nothing on its own. It reads the picture on the screen back on a grid, and stamps each frame so a reader knows which frame it saw.
**Use it when:** an effect must measure the finished picture, as exposure, glow or shafts do.
**Where it is:** [header](../../include/sh4der_frame.h) and [effect page](../effects/frame.md). The video memory rules are in the [video ledger](../hardware/ledger-video.md).
**Starting settings:** the header has no RECOMMENDED block. Its defaults are `SH4DER_FRAME_GRID_X` = 80 and `SH4DER_FRAME_GRID_Y` = 60 (include/sh4der_frame.h:65-66). These are defaults, not recommendations.
- Invalidate the cache (throw away its copy) before every cached read of video memory. Without it, 238 of 256 lines read back old data. PROVEN (SH4-006, h2, one run, one console).
- Do not read video memory pixel by pixel through the uncached address in a hot loop. That address skips the cache and runs at 3.7 MB/s. PROVEN (SH4-040, h2).
- Draw the stamp last, over everything, and do not read a render target that may be half drawn (header "Order"; [effect page](../effects/frame.md) Pitfalls). ADVISED.

Status: PROTOTYPE v0.1. The read's time on our console is not measured.

### Shared parts [core]
**What it looks like:** nothing on its own. It holds the shapes and helpers the other effects share.
**Use it when:** you build an effect of your own from the shared pieces.
**Where it is:** [header](../../include/sh4der_core.h) and [effect page](../effects/core.md).
**Starting settings:** it has none of its own. The caller sets `sh4der_breath_push` once a frame for each body, and `sh4der_ramp_colours` once at load (README "The core (shared parts)"). Its shared defaults, such as `SH4DER_BREATH_WINDOW` = 90 (include/sh4der_core.h:90), are not recommended.

Status: PROTOTYPE v0.1 on the [maturity](../status/maturity.md) page. The header says v1.00, and the README's version scale ties v1.00 to use in a released game. The maturity page lists that old claim as undecided.

### Bake tool [ao-bake]
**What it looks like:** nothing on the console. On your PC it writes one shading byte a vertex: ambient occlusion, a number for how much a point is shut in from the open sky.
**Use it when:** you bake corner shading into a level before the Dreamcast build.
**Where it is:** [header](../../tools/ao_bake_core.h), which sits under tools/ and not include/, and [effect page](../effects/ao-bake.md).
**Starting settings:**
- `AOB_RAYS` = 64, `AOB_R` = 2.5f (metres), `AOB_PUSH` = 0.02f and `AOB_INSIDE` = 0.25f (tools/ao_bake_core.h:48-51). ADVISED. A host run matched the room's bytes to within 2 for 1,905 vertices (2026-11-26). That is an ESTIMATE, not a console run.
- For a road, a 10 m reach at a scale of `AOB_R` / 10 (header block). ADVISED.
- Fill a point that sits inside a solid from its neighbours (`aob_fill`), or bake it inside its own triangles (`aob_at_in`) (header, "Inside a solid" note). ADVISED.
- Never ship the tool in a console build (header "Do not"). ADVISED.

Status: ALPHA in its header. The maturity page has no row for it. The console's own load-time bake is a different routine: see [Baked corner shading [aobake]](../effects/aobake.md).

## What it costs

Video memory is the 8 MB the graphics chip keeps for frame buffers, render targets, textures and its tile and vertex lists ([glossary](../glossary.md)). A triangle is one flat shape the PVR draws, and a vertex is one corner of it.

| Effect | Video memory | Triangles or vertices | SH-4 time | Status |
|---|---|---|---|---|
| Send checks [guard] | 0 (no state, sh4der_guard.h:31) | None sent by it | Three reciprocals and ten multiplies a triangle (FACT, from the code, sh4der_guard.h:79-88). Not measured (OPEN: time a fan with the check on and off) | ALPHA v0.3 |
| Crash and hang screen [fault] | About 3 KB of report buffers, plus the breadcrumbs (ESTIMATE, from the code) | None sent by it | Under 100 instructions a frame; watchdog about 20 a blank (ESTIMATE, from the code). Not measured (OPEN: a deliberate fault, photographed) | PROTOTYPE v0.1 |
| Tile-cut pass [tilecut] | 0 (tables rewritten in place, FACT). RAM: 14,400 B for the two copies (sh4der_tilecut.h:89) | Not vertices: PVR tile time. 3,312 to 375 us for a 20-tile target (PVR-092). 5,075 to 1,425 us for a 10 x 8 map (PVR-205). MEASURED, one console each | Begin 126 us, end 130 us a pass (PVR-205, MEASURED, one console). Columns form: 5 us and 3 us | ALPHA v0.5 |
| PVR extras [pvrx] | 0 (FACT). Your RAM buffer, in whole 32-byte units (sh4der_pvrx.h:93-102) | Not vertices. A download by interrupt: 32 KB 869 us, 128 KB 3,461 us, 512 KB 13,742 us (BUS-021, MEASURED, one console) | Frame 39.9 to 23.3 ms in one program, both waits polled (PVR-117, MEASURED, two runs). Other calls not measured (OPEN: the pvrx-check entry) | PROTOTYPE v0.1 |
| Distance ramp [ramp] | 0 | None: a helper | One reciprocal a call (ESTIMATE, not measured) | ADVISED, no figures |
| Split screen [split] | 0 | Not measured: the halves' own sends. A hidden half is not sent | Integer compares a frame. Not measured (OPEN: move the divider and time it) | ALPHA v0.2 |
| Frame read [frame] | 0 extra: the stamp is drawn into the frame on screen. Raw grid 9,600 B (80 x 60 cells of 2 B, FACT, src/sh4der_frame.c:23) | 4,800 samples a read, not vertices | Read time not measured (OPEN). Bus rates: 3.7 MB/s uncached (SH4-040), 38.6 to 38.7 MB/s through the cache (SH4-041), MEASURED, one console. Another port's log: 1,196 us (VID-005, REPORTED) | PROTOTYPE v0.1 |
| Shared parts [core] | 0 | Not a vertex cost | One SH4ZAM call or instruction a helper (as stated, not measured). Breath tracker: 364 bytes (README "Shared tuning") | PROTOTYPE v0.1 |
| Bake tool [ao-bake] | None on the console | Host: 1,905 vertices in one host check (ESTIMATE) | Host time not measured (OPEN: time a build). The console's own bake: 254 ms for 2,142 vertices, 393 ms for 2,544 (SH4-076, MEASURED, one console) | ALPHA in its header; no maturity row |

## Try this first

This recipe adds send checks [guard] to one fan or ring. When it works, a ring seen edge-on has no bright stray lines across it.

1. Include `sh4der_guard.h` in the file that sends the triangles. It pulls in `sh4der_core.h`, so the library's `include` folder must be on the include path (`sh4dow_dc.h` is in it), and SH4ZAM (the SH-4 maths library) must be linked, as KallistiOS says. Using the library means following its [licence and credit](license-and-credit.md) rule.
2. At load, compile every polygon header the program draws with, using KallistiOS's `pvr_poly_compile`. A header that was never compiled is all zeros.
3. In a debug build, call `sh4der_poly_hdr_ok` on each header's eight 32-bit words before you draw with it. A 0 means the header was never compiled.
4. For each triangle of the ring, turn its three points into view space (x right, y up, z forward and positive in front). Call `sh4der_tri_area_ok(a, b, c, near_z, SH4DER_GUARD_MIN_AREA2)`, send the triangle on a 1, and count the drops. A point behind `near_z` also returns 1, so clip those triangles before this check.
5. Run it on a console. Turn the camera until you look along a spoke. No bright line should cross the ring, and the drop count should move as you turn. An emulator draws nothing for a zero-area triangle, so this step needs a real console.

The steps follow the shape shared in the [call orders](call-orders.md) page. AGENTS.md has no "Call order: guard" section, because the checks are part of that shape.

## Related

- [What do you want to make?](what-do-you-want.md)
- [Effect index](../effects/README.md)
- [Recommended limits](../limits/recommended-limits.md)
- [Glossary](../glossary.md)
- [Licence and credit](license-and-credit.md)
- [Demo guide](demo-guide.md)
- [The fault catcher method](../methods/fault-catcher.md), [open questions](../open-questions.md)
