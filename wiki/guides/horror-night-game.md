---
title: "Horror and night: a recipe for the dark, the torch and the bad picture"
tags: [guide, horror, night, game-type, recipe, effect]
updated: 2026-11-26
sources: [wiki/guides/racing-game.md, wiki/effects/names.md, wiki/effects/torch.md, wiki/effects/look.md, wiki/effects/lens.md, wiki/effects/film.md, wiki/effects/timebase.md, wiki/effects/visor.md, wiki/effects/mist.md, wiki/effects/bolt.md, wiki/effects/motes.md, wiki/effects/warp.md, wiki/effects/aobake.md, wiki/effects/pool.md, wiki/effects/fault.md, wiki/effects/frame.md, wiki/effects/bump.md, wiki/limits/frame-budget.md, wiki/limits/budgets.md, wiki/limits/header-recommended.md, wiki/start-here/call-orders.md, wiki/start-here/genre-camera-and-screen.md, wiki/start-here/genre-light-and-glow.md, wiki/start-here/license-and-credit.md, wiki/glossary.md, wiki/hardware/ledger-pvr.md, wiki/hardware/ledger-sh4.md, wiki/hardware/ledger-bus-dma.md, wiki/hardware/ledger-video.md, wiki/status/maturity.md, include/sh4der_torch.h, include/sh4der_look.h, include/sh4der_lens.h, include/sh4der_film.h, include/sh4der_visor.h, include/sh4der_mist.h, include/sh4der_bolt.h, include/sh4der_fault.h, include/sh4der_timebase.h, include/sh4der_aobake.h, include/sh4der_pool.h, src/sh4der_torch.c, src/sh4der_lens.c, README.md, AGENTS.md]
---
# Horror and night: a recipe for the dark, the torch and the bad picture

**What you will learn:** how to build a dark night scene: a torch as the main light, fog and dust for dread, and one camera feel. You will also learn what each part costs, what to cut first, and what goes wrong.
**For:** a developer making a horror or night game on the Dreamcast who has never used SH4DER-PVR.

**Status tags.** **MEASURED** means a ledger row backs the number (one console; the row is named). **ADVISED** means a value from a header's RECOMMENDED block, a README table or demo source (file and line). **no figures** means the owner has run that part on a console and no number is recorded. **ESTIMATE** means arithmetic or a model, and the model is named. A count read from the code is an ESTIMATE too. A host count is exact, but it is not a console number. Most effects here are PROTOTYPE, so most numbers are not measured. Reader names follow the [names page](../effects/names.md), section 0.

## The short version

- Start with the screen spotlight alone: a round lit patch that darkens the rest of a dark room, with no frame read.
- Build the torch's gate and carve meshes, and check each triangle for zero area before it is sent.
- The whole stack, on paper, is about 21.75 ms of PVR (ESTIMATE), about 5 ms over 60 Hz before the world's triangles are counted.
- Our advice: cut specks, lightning, warp, mist, then grain (about 6.2 ms of PVR, ESTIMATE); the carve goes last.
- Several parts have no figures, and the sources disagree on the torch's near light and the warp's frame texture size.

## What this kind of game needs

- **One main light.** A torch, a lamp or a window. The rest of the room falls into dark.
- **Dark that stays dark.** The picture must not brighten by accident. The light must darken what is round it.
- **Dread from small things.** Fog to walk into, dust in the beam, a lightning flash.
- **One camera feel per scene.** Old film, tape, a night screen or a shock.
- **Counting.** The graphics chip is the PVR. A **translucent** layer is drawn over the picture and blended with it ([glossary](../glossary.md)). A full-screen layer costs about 3.1 ms at 640 x 480 (PVR-002, MEASURED, one console).

## Effects that fit, and why

| Effect | What it gives this game | Where it is | Status |
|---|---|---|---|
| Screen spotlight [torch] | A round lit patch with a hot centre. Dark outside. No change in daylight. | [header](../../include/sh4der_torch.h), [page](../effects/torch.md) | PROTOTYPE v0.1 (maturity.md); demo disc; no ledger row. The sources disagree: `torch.h:48` and the torch page say Version v0.50, runs on the demo disc |
| Exposure and grade [look] | The eye adapts from bright to dark and back. Needs the frame read. | [header](../../include/sh4der_look.h), [page](../effects/look.md) | PROTOTYPE v0.1 (maturity.md); no ledger row. The sources disagree: `look.h:36` and the look page say Version v0.50, runs on the demo disc |
| Screen overlays [lens] | Dark corners, grain, hit wedges, a flash for a jump. | [header](../../include/sh4der_lens.h), [page](../effects/lens.md) | PROTOTYPE; no ledger row |
| Old film wear [film] | Flicker, scratches, dust: a flashback. | [header](../../include/sh4der_film.h), [page](../effects/film.md) | PROTOTYPE; host tests |
| Tape wobble [timebase] | Rows bend sideways: a worn tape or a monitor. | [header](../../include/sh4der_timebase.h), [page](../effects/timebase.md) | PROTOTYPE; no ledger row |
| Screen filters [visor] | Night-vision, amber or thermal look. | [header](../../include/sh4der_visor.h), [page](../effects/visor.md) | PROTOTYPE; no program uses it |
| Mist bank [mist] | Ground fog with a shape the player walks into. | [header](../../include/sh4der_mist.h), [page](../effects/mist.md) | PROTOTYPE; fill not measured |
| Lightning [bolt] | Strands that re-draw a few times a second. | [header](../../include/sh4der_bolt.h), [page](../effects/bolt.md) | PROTOTYPE; no program uses it |
| Drifting specks [motes] | Dust or embers in the torch beam. | [header](../../include/sh4der_motes.h), [page](../effects/motes.md) | PROTOTYPE; host tests |
| Screen warp [warp] | A shock ring or a dream wobble, for a moment. | [header](../../include/sh4der_warp.h), [page](../effects/warp.md) | ALPHA v0.3 (maturity.md). The sources disagree: `warp.h:39` says Version v0.10 |
| Baked corner shading [aobake] | Dark room corners, baked at load. | [header](../../include/sh4der_aobake.h), [page](../effects/aobake.md) | Header ADVISED; bake MEASURED on an earlier copy |
| Light pools [pool] | A lamp's pool of light on the floor. | [header](../../include/sh4der_pool.h), [page](../effects/pool.md) | ALPHA v0.2 for the disc and the wedge (`pool.h:18`); the fade by distance is PROTOTYPE v0.1 (`pool.h:33`) |
| Crash and hang screen [fault] | Development aid: a report when the program dies. | [header](../../include/sh4der_fault.h), [page](../effects/fault.md) | PROTOTYPE |

**Left out.** Screen bloom and dark shafts read the picture back, a second frame read with no console time yet. Sun shadows need a sun. Motion blur shares the PVR's one stride register with the frame texture the tape uses (VID-003; `include/sh4der_ca.h:174`; our reading).

**Also fits.** Normal map ([bump](../effects/bump.md)) gives relief on a torch-lit floor; it has no console time. Match Light (`demo/matchlight`) uses film, lens, mist and motes; its costs are host counts.

**Pick one camera feel per scene.** Our advice, not tested. Two of them cost twice.

## Ghosting with TAA

**What the viewer sees.** A faint copy of the last frame lingers behind moving things. Falling dust or ash smears a little, and the dark room feels unsteady. The Match Light short film demo (`demo/matchlight/`) uses this look in its lens pass; its code notes that above a weight of 0.5 "the falling snow smears" (`demo/matchlight/matchlight.c:126`).

**Where the ghost comes from.** Temporal anti-aliasing (TAA) draws each frame a fraction of a pixel to one side, then blends the frame before into it. The blend smooths edges, and a moving thing leaves a faint copy behind it. The trail effect ([Motion blur [trail]](../effects/trail.md)) gives a stronger smear, by drawing copies along the motion.

**When to use it.** At night, in fear, in a dream, or for a hit reaction: a short smear suggests a shaky eye. Keep it off for a close-up you want sharp. The TAA header drops the history when the view turns more than about 20 degrees in one frame (`include/sh4der_taa.h`, `SH4DER_TAA_CUT_RAD`, 0.35 radians).

**Settings to start with.**
- TAA, the gentle ghost: `SH4DER_TAA_WEIGHT_HI` 0.75, the history weight while the camera is still (the header default, `include/sh4der_taa.h:84`; ADVISED). The film uses 0.45 (`LENS_TAA_WEIGHT`, `demo/matchlight/matchlight.c:126`), a lighter ghost.
- TAA, the jitter: `SH4DER_TAA_PHASES` 8 (the default, `include/sh4der_taa.h:83`; ADVISED).
- Trail, the stronger smear: `SH4DER_TRAIL_COPIES` 3, `SH4DER_TRAIL_WEIGHT_HI` 0.5 (the cap), `SH4DER_TRAIL_SHIFT_MAX` 8 and `SH4DER_TRAIL_SPEED_HI` 8.0 (the header defaults, `include/sh4der_trail.h:67-71`; ADVISED).
- Status: TAA: Run on a console by the owner in Demo disc and Match Light (her test; no measured numbers recorded yet; released in the public Match Light film); the trail: no figures. The film's own figures are counted, not measured (its README: "not measured on a console").

**What it costs.** It is not free.
- TAA draws two full-screen translucent layers in a frame where the weight is above zero: one dims the frame, one adds the history (`include/sh4der_taa.h`, the blend functions). A full-screen translucent layer costs about 3.1 ms of PVR at 640 × 480 (PVR-002, MEASURED, one console). So TAA is about 6 ms of PVR a frame at 640 × 480 (ESTIMATE, two layers). That is about a third of a 60 Hz frame (16.7 ms). Its history reads the front buffer, so it needs no video memory of its own (`wiki/effects/taa.md`, as stated). Its recommended frame texture is 320 × 240, 320 KB.
- The trail draws a keep layer, a mask layer and two layers for each copy: with three copies, eight full-screen layers, about 25 ms of PVR a frame (ESTIMATE, the same arithmetic). That does not fit a 60 Hz frame. Use two copies at most, or TAA.

**The example to read.** The Match Light short film demo (`demo/matchlight/`) is a finished film. Its lens pass runs TAA with the 0.45 weight, the chromatic aberration pass, and no trail effect. In its code the word "trail" means the snow footprint patch, not the motion trail. Only the code was read here; nothing in it was run for this page.

## The order of one frame

The sources give an order for exposure and light pools (`AGENTS.md`, "Call order: look" and "Call order: pool"). The other effects give their own place. No source gives one order for all of them. The list uses those places. It is our advice, not tested.

1. **At load.** Look: `sh4der_look_bins`, `sh4der_look_params_default`, `sh4der_look_eye_reset`. Torch: `sh4der_torch_gate_reset`. Lens: `sh4der_lens_init`, which makes its grain tile in video memory (`src/sh4der_lens.c:215`).
2. **Each frame, before drawing.** Read the frame on screen, with its stamp (frame.md). Step the eye, except while the torch is lit (`include/sh4der_look.h:55-56`). Step the torch gate from baked light, not the read (README:649). Why: a light must not react to its own output.
3. **Light the vertices.** Add the torch's near light to each vertex (README:644). The carve then darkens what this light has lit.
4. **Opaque world.** Into the frame texture if the picture is bent (timebase.md, steps 1 to 3); otherwise onto the screen.
5. **Punch-through list.** Drifting specks (motes.md).
6. **Translucent world effects.** Mist sheets (they sort with the scene, README:1304), light pools after the ground (AGENTS.md, pool step 2), lightning quads (README:1190). Why: they need the ground and walls drawn before them.
7. **Torch layers, then the look's layers.** The torch sits under the look's z values (`include/sh4der_torch.h:37-38`). The look's layers are full screen and under the HUD (README:431).
8. **Film, then lens.** Film's rectangle and specks, then the lens overlays at z 430 to 960, under the HUD at 1000 (README, 'The lens effect'). No source orders film against lens.
9. **HUD and text, straight.** Then the frame stamp, last (frame.md).

## What it costs

A frame is 16.7 ms at 60 Hz and 33.3 ms at 30 Hz. The SH-4 and the PVR overlap, so a frame costs about the larger of the two. If they run one after the other, it costs about their sum ([frame budget](../limits/frame-budget.md), section 1). Each row is one part on its own.

| Part | Video memory | Triangles or vertices | Frame time | Status |
|---|---|---|---|---|
| Spotlight, both meshes | 0 | 288 triangles, 306 vertices (README:651; count) | Carve about 3.1 ms of PVR | Carve: ESTIMATE (PVR-002 model). Rest not measured |
| Frame read | 0 | 4,800 samples (count) | 2.6 ms of SH-4: 9,600 bytes at 3.7 MB/s | ESTIMATE, on SH4-040's rate. A port's log: 1,196 us (VID-005, REPORTED) |
| One exposure layer | 0 | 4 vertices (count) | 3.1 ms of PVR | MEASURED (PVR-002) |
| Screen overlays, three layers | 8 KB grain tile (ESTIMATE, arithmetic; `include/sh4der_lens.h:111`) | Vignette 594 vertices (`src/sh4der_lens.c:19`; count) | 9.3 ms of PVR | ESTIMATE: 3 x PVR-002 |
| Old film wear | 0 | Rectangle, 3 lines, 12 specks (`film.h:73-74`; count) | 3.1 ms of PVR | ESTIMATE, one full-screen layer |
| Tape wobble | 640 KB frame texture (FACT, VID-001; `ca.h:11-12`) | About 960 vertices (ESTIMATE count) | Not measured | no figures |
| Screen filters | Grain 2,048 B; thermal map 16 KB (ESTIMATE, arithmetic; visor.md) | Not stated | Not measured; 3.1 ms per full-screen layer (ESTIMATE) | no figures |
| Mist bank | 8 KB noise tile, yours (ESTIMATE, arithmetic) | 48 triangles (count) | Not measured. Model: screen share x 3.1 ms (ESTIMATE) | Run on a console by the owner in Match Light (her test; no measured numbers recorded yet; released in the public Match Light film) |
| Lightning | 0 | 120 triangles, 240 vertices (README:1191) | Not measured | no figures |
| Drifting specks | 512 B mask, optional (ESTIMATE, arithmetic) | 96 sprites (count) | Not measured | Run on a console by the owner in Match Light (her test; no measured numbers recorded yet; released in the public Match Light film) |
| Screen warp | 256 KB (ESTIMATE, arithmetic; README:511-512). README:64 gives 160 KB: the sources disagree | Mesh 40 x 30: 2,460 vertices (BUS-004) | 2,250 us of SH-4 to send the mesh (BUS-004) | MEASURED, SH-4 |
| Light pool | 0 | 10 triangles a disc (`pool.h:8-9`; count) | Not measured. Model: screen share x 3.1 ms (ESTIMATE) | Run on a console by the owner in Car on a wet night road; Drive a car on hills; Drive a wet night road; Street car demo (her test; no measured numbers recorded yet) |
| Baked corners | 0 | 2,142 vertices (scene A) | 0 a frame; 252 to 254 ms at load | MEASURED on an earlier copy (SH4-076, SH4-082) |

**The whole stack, on paper.** An empty 640 x 480 frame costs 3.15 ms of PVR (PVR-001, MEASURED). Add the spotlight carve, one exposure layer, the three lens layers and the film: 3.15 + 3.1 + 3.1 + 9.3 + 3.1 = 21.75 ms of PVR. That sum is an ESTIMATE: it adds measured unit costs ([budgets](../limits/budgets.md), worked example), and no console run of this stack exists. At 60 Hz it is about 5 ms over, before the world's own triangles. At 30 Hz about 11.6 ms is left.

## Settings to start with

**Screen spotlight**
- `SH4DER_TORCH_DARK_LO` 0.30 and `SH4DER_TORCH_DARK_HI` 0.80: the carve is full up to 0.30 baked light and gone from 0.80 (`torch.h:106-107`). ADVISED: the author's game, by eye.
- `SH4DER_TORCH_NEAR_WU` 7.0 and `SH4DER_TORCH_NEAR_K` 0.425; fog reach 2.0 to 6.0 (README:694-697). ADVISED. The sources disagree: the README's recommended set is radius 6.0 and strength 0.85 (README:1641).
- One world unit is about 3 m in the author's game, so scale the distances (`torch.h:68-69`). ADVISED.

**Exposure and grade**
- Eye strength 0.25: `SH4DER_LOOK_TRANSIENT` (0.5) and `SH4DER_LOOK_OPEN` (0.3) scaled by 0.25, giving 0.125 and 0.075 (`look.h:46-49`). Gain clamped to 0.4 to 2.0 (`look.h:52-53`). ADVISED.
- `SH4DER_LOOK_LIGHT_S` 2.0 and `SH4DER_LOOK_DARK_S` 6.0 (`look.h:87-88`). ADVISED.

**Screen overlays**
- `SH4DER_LENS_VIG_STRENGTH` 0.45, `_INNER` 0.60, `_OUTER` 1.10 (README, 'Lens tuning'). ADVISED.
- `SH4DER_LENS_GRAIN_AMP` 0.06, about plus or minus 3 % (README, 'Lens tuning'). ADVISED.

**Old film (one camera feel)**
- `SH4DER_FILM_FLICKER_MAX` 0.04 (8 % peak to peak) and `SH4DER_FILM_RATE_MAX_HZ` 3.0 (`film.h:75-76`). Design limits, not tested on viewers. ADVISED.

**Tape (one camera feel)**
- Tension 0.375, flutter 0.135, a mesh row every 6 rows, overscan 8 px (`timebase.h:37-39`). ADVISED; no figures.

**Mist bank**
- `SH4DER_MIST_SLICES` 12 and `SH4DER_MIST_OPACITY` 0.7 (README:1320-1321). ADVISED.
- No recommended value yet for its PVR cost. Test: PVR time with the bank on and off (mist.md).

**Lightning**
- `SH4DER_BOLT_STRANDS` 4 and `SH4DER_BOLT_REROLL_S` 0.1333 (`bolt.h:64-78`). ADVISED; no figures.
- Strands at most 8 (`bolt.h:60`). ADVISED.

**Drifting specks**
- `max_draw` 96, with the ceiling `SH4DER_MOTES_DRAW_MAX` 128 (motes.md). ADVISED; Run on a console by the owner in Match Light (her test; no measured numbers recorded yet; released in the public Match Light film).
- At most 256 slots per emitter (motes.md, `motes.h:60`). ADVISED.

**Screen warp (a shock)**
- `SH4DER_WARP_RING_SPEED` 180.0 pixels a second and `SH4DER_WARP_RING_AMP` 5.0 pixels (README:518-520). ADVISED.
- The mesh goes by `pvr_prim` (`SH4DER_WARP_MESH_SEND_PRIM` 1, `src/sh4der_warp.c:50-71`); measured once (BUS-004).

**Baked corners**
- `SH4DER_AOBAKE_RAYS` 64 and `SH4DER_AOBAKE_REACH` 3.0 (`aobake.h:64-65`). ADVISED for this copy.
- Bake at load, not per frame. A moving occluder needs a new bake (`aobake.h:14`).

**Light pools**
- Lift the fan 2 to 4 cm off the floor (`pool.h:8-9`). ADVISED.
- Limit lamps by distance: a pool is translucent fill (pool.md). ADVISED.

**Crash and hang screen (for development)**
- `SH4DER_FAULT_HANG_VBLANKS` 120, about 2 s at 60 Hz (`fault.h:88`). ADVISED; NOT RUN (no console run has shown it firing).
- Never define `SH4DER_FAULT_TEST` in a player's build (`fault.h:15`).

## What to cut first

Our advice, not tested. Cheapest loss of picture first. Steps 3, 5, 6 and 7 carry a number.

1. **Drifting specks.** Lower `max_draw`. You lose dust.
2. **Lightning.** Draw it at a flash. You lose the strands between flashes.
3. **Screen warp.** Draw it for the shock. Its mesh send is 2,250 us of SH-4 (BUS-004).
4. **Mist bank.** Fewer sheets. You lose fog depth.
5. **Grain.** Drop its two layers. Saves about 6.2 ms of PVR (ESTIMATE).
6. **Old film.** Drop its layer. Saves about 3.1 ms of PVR (ESTIMATE).
7. **Exposure.** Skip the read and hold the gain. You lose adaptation. Saves about 2.6 ms of SH-4 (ESTIMATE).
8. **Tape or night screen.** Drop the device look. The tape frees 640 KB (VID-001).
9. **Spotlight boost.** You lose the hot core.
10. **Spotlight carve.** Last. You lose the dark round the circle, which is most of the look.

## Common mistakes

- **Scale x once under AA 2X.** The PVR draws 1,280 wide. Scale once, or the spot is an oval (torch.md; `torch.h:40-46`; VID-010).
- **Gate the torch from the frame.** It feeds back on itself. Use baked light (`torch.h:31-33`).
- **Let the eye chase the torch.** Hold the eye while the torch is lit, and give the step the read with the gain taken off (`look.h:55-58`).
- **Skip the zero-area check.** The boost's innermost band has no area (`src/sh4der_torch.c:39`). A console draws such triangles as bright lines (README:1723). Use [send checks](../effects/guard.md).
- **Send an unbuilt polygon header.** Command word 0 ends the list early on a console (README:1724).
- **Stack layers without counting.** Each full-screen layer is about 3.1 ms (PVR-002).
- **Lens at boot.** With no video memory it draws without grain and does not retry until the PVR restarts (lens.md).
- **Tape from the wrong place.** Call it from the main thread, and give the overscan at least the largest shift (`timebase.h:46-49`).
- **Fast maths in the eye.** SH4ZAM's fast log reads log(1) as 0.040, a grey cast (README:415).

## Try this first

Our recipe, not tested on a console. The calls come from the headers. It is the screen spotlight alone, with no frame read.

1. **Include** `include/sh4der_torch.h` and `include/sh4der_guard.h`. Build with KallistiOS and the SH4ZAM it carries (AGENTS.md). Credit SH4DER-PVR in your game ([licence and credit](../start-here/license-and-credit.md)).
2. **At load:** `sh4der_torch_gate_reset(&gate)`, then `sh4der_torch_headers(hdr)` (`src/sh4der_torch.c:113`) for the multiply and boost headers.
3. **Each frame:** `share = sh4der_torch_gate_step(&gate, baked, dt)`, with `baked` the baked light near the torch, not the frame. `power = sh4der_torch_power_step(power, lit, dt)`; `k = share * power`. Per near vertex, add `sh4der_torch_near(dist2, k)`.
4. **Each frame, the meshes:** `sh4der_torch_radii` and `sh4der_torch_umax` (AA 2X rule), then `sh4der_torch_carve_mesh` and `sh4der_torch_boost_mesh`. Test each triangle with `sh4der_tri_area_ok`. Send with `sh4der_torch_draw` after the world, before the HUD.
5. **What you should see:** a dark room with a round patch of warm light ahead and a hot centre (`torch.h:49-51`). In a sunny hall it adds light and darkens nothing (`torch.h:64-65`). Bright lines mean the zero-area check is missing.

Add exposure and grade after this works. The order is in `AGENTS.md`.

## Related

- [Racing game: a recipe for the car, its shadow and its lights](./racing-game.md): the worked example this page follows.
- [What do you want to make?](../start-here/what-do-you-want.md) and [call orders](../start-here/call-orders.md).
- Genre guides: [camera and screen](../start-here/genre-camera-and-screen.md), [light and glow](../start-here/genre-light-and-glow.md), [tools and safety](../start-here/genre-tools-and-safety.md).
- [Frame budget](../limits/frame-budget.md), [recommended limits](../limits/recommended-limits.md), [PVR ledger](../hardware/ledger-pvr.md).
- [Glossary](../glossary.md) and [licence and credit](../start-here/license-and-credit.md): required when you ship any of this.
