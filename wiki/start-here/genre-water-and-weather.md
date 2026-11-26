---
title: "Genre guide: water and weather"
tags: [start, guide, effect, water, mist, motes, rise, cloudvolume, deform]
updated: 2026-11-26
sources: [include/sh4der_water.h, include/sh4der_mist.h, include/sh4der_motes.h, include/sh4der_rise.h, include/sh4der_cloudvolume.h, include/sh4der_deform.h, README.md (the water, mist, motes, rise, cloudvolume and deform sections and tuning tables), AGENTS.md (call order spark), demo/OCEAN.md, wiki/effects/water.md, wiki/effects/mist.md, wiki/effects/motes.md, wiki/effects/rise.md, wiki/effects/cloudvolume.md, wiki/effects/deform.md, wiki/effects/names.md, wiki/status/maturity.md, wiki/hardware/ledger-pvr.md, wiki/limits/recommended-limits.md, wiki/start-here/what-do-you-want.md, wiki/start-here/call-orders.md, wiki/glossary.md, wiki/STRUCTURE.md]
---
# Genre guide: water and weather

**What you will learn:** which water and weather effect to use, what it looks like, what it costs, and which numbers nobody has measured yet.
**For:** a programmer adding a sea, fog, drifting specks, rising lights, clouds or footprints to a Dreamcast game built on SH4DER-PVR.

This genre makes the air and the ground look alive. None of the six has a measured frame time on a console yet, so the settings below are starting points. A host test runs on a PC, not on the Dreamcast. Terms such as PVR and video memory are in the [glossary](../glossary.md).

## What you can make

You can make a sea or lake with rolling waves and light that shows through the crests. You can make a fog bank with a shape that a figure can walk into. You can make dust, embers or snow that drifts through the air without storing anything for each speck. You can make lanterns that lift off and settle as stars. You can make a cloud deck to fly through, and footprints that stay in snow or close up in mud. The sunset sea demo, the short film and the cloud deck demo show these effects (see the [demo guide](demo-guide.md)).

## The effects

### Water waves [water]

- **What it looks like:** a sea or lake of rolling waves, with ripples, a sky reflection, and light showing through the crests.
- **Use it when:** you need a wave surface shaded per vertex, for a sea, a lake or a puddle.
- **Where it is:** [header](../../include/sh4der_water.h) and [effect page](../effects/water.md).
- **Starting settings:** no start setting is recommended. The header has no RECOMMENDED block, and the README has no water tuning table.
  - `SH4DER_WATER_TAU` = 6.28318530718f (2 pi) is a constant, not a knob (include/sh4der_water.h:22). Status: FACT.
  - Set `footprint` to your vertex spacing, or to 0 to turn the fade off. A wave fades from full strength at 6 footprints to nothing at 2 (include/sh4der_water.h:54-57). Status: ADVISED.
  - The sea demo's PERFORMANT preset uses a 2,016-triangle mesh with the soft shadow (SH4DOW-DC) off (demo/OCEAN.md:122-126). That is the demo's choice, not a limit. Status: ADVISED.

### Mist bank [mist]

- **What it looks like:** a low fog bank with a shape: upright see-through sheets stacked one behind another, each with a soft top and slow drifting noise (a random-looking pattern that changes smoothly).
- **Use it when:** fog must have a place and a shape, such as mist on a street or steam over water.
- **Where it is:** [header](../../include/sh4der_mist.h) and [effect page](../effects/mist.md).
- **Starting settings:** Run on a console by the owner in Match Light (her test; no measured numbers recorded yet; released in the public Match Light film).
  - `SH4DER_MIST_SLICES` = 12, the default sheet count (include/sh4der_mist.h:89). Use fewer sheets, or keep the bank low and partial (wiki/effects/mist.md). Status: ADVISED. Test: PVR render time from `pvr_get_stats`, bank on and off.
  - `SH4DER_MIST_OPACITY` = 0.7, the share of light the bank stops (include/sh4der_mist.h:91). Status: ADVISED (a default).
  - `SH4DER_MIST_FEATHER` = 2.0, the metres of soft top (include/sh4der_mist.h:92). Status: ADVISED (a default).

### Drifting specks [motes]

- **What it looks like:** a steady field of small round dots (dust, embers, pollen, ash) that are born, drift, fall, fade and are born again.
- **Use it when:** you want life in the air round a player or a light, without storing anything for each speck.
- **Where it is:** [header](../../include/sh4der_motes.h) and [effect page](../effects/motes.md).
- **Starting settings:** Run on a console by the owner in Match Light (her test; no measured numbers recorded yet; released in the public Match Light film).
  - `max_draw` = 96 in the default parameters (include/sh4der_motes.h:125), inside `SH4DER_MOTES_DRAW_MAX` = 128 (include/sh4der_motes.h:61). Status: ADVISED (a default).
  - `flat_px` = 3.0: specks this wide or less may use the untextured opaque draw (include/sh4der_motes.h:124). Status: ADVISED (a default).
  - Spread the specks: 256 sorted translucent triangles in one tile took 6,385 us, and 82 us spread out (PVR-009, MEASURED, one console). Status: PROVEN ([recommended limits](../limits/recommended-limits.md), Fill).

### Rising lights [rise]

- **What it looks like:** points of light that sit still, lift off one after another, drift up with a small sway, shrink and cool, then settle as twinkling stars.
- **Use it when:** you want lanterns, candles or fireflies that leave their place and become stars.
- **Where it is:** [header](../../include/sh4der_rise.h) and [effect page](../effects/rise.md).
- **Starting settings:** Run on a console by the owner in Match Light (her test; no measured numbers recorded yet; released in the public Match Light film).
  - `SH4DER_RISE_TWINKLE_IN` = 1.0, seconds for the twinkle to fade in after arrival (include/sh4der_rise.h:69; README "Rise tuning"). Status: ADVISED (a default).
  - Spread the lights, as for the specks: 6,385 us stacked in one tile, 82 us spread (PVR-009, MEASURED, one console). Status: PROVEN.

### Cloud deck [cloudvolume]

- **What it looks like:** a sunlit cumulus layer you fly over and into. Moving noise carves its edges, and it is drawn as one filtered quad over the sky.
- **Use it when:** you want clouds to fly through, and the processor can run a ray march (tracing light rays step by step through the cloud) each frame.
- **Where it is:** [header](../../include/sh4der_cloudvolume.h) and [effect page](../effects/cloudvolume.md).
- **Starting settings:** NOT RUN on a console.
  - `SH4DER_CLOUDVOLUME_STEPS` = 16, the default march steps (include/sh4der_cloudvolume.h:97). Status: ADVISED. The cost behind it is an ESTIMATE.
  - `SH4DER_CLOUDVOLUME_MAX_STEPS` = 64, the most steps one ray may take (include/sh4der_cloudvolume.h:81). Status: ADVISED.
  - Treat the march as the scene's main cost: 14 to 22 ms a frame at the default (ESTIMATE, wiki/effects/cloudvolume.md:32). Status: ADVISED until measured (wiki/effects/cloudvolume.md:41).

### Dented ground [deform]

- **What it looks like:** snow or mud that keeps the dent of a foot or a wheel, with a raised rim. Footprint decals (flat pictures laid on the ground) add crisp prints on a coarse mesh.
- **Use it when:** a character or vehicle crosses ground you already draw, and the marks must stay (snow) or close up (mud).
- **Where it is:** [header](../../include/sh4der_deform.h) and [effect page](../effects/deform.md).
- **Starting settings:** Run on a console by the owner in Match Light (her test; no measured numbers recorded yet; released in the public Match Light film).
  - Snow keeps its dents: the struct field `refill` = 0. Mud closes them over a few steps a second (include/sh4der_deform.h:114; README "The deform effect"). Status: ADVISED.
  - `SH4DER_DEFORM_CELL` = 0.25, metres a cell (include/sh4der_deform.h:84), so the default 160 x 160 window covers 40 m. Status: ADVISED (a default).
  - A refill pass goes over every cell, 25,600 bytes, and runs only when the refill is not 0 (include/sh4der_deform.h:19-21). Status: FACT (the header's count).

## What it costs

SH-4 time is the main processor's time. PVR time is the graphics chip's time. FACT means read from a source. ESTIMATE means a count or a model. OPEN means not yet measured.

| Effect | Video memory | Triangles or vertices | SH-4 time | Status |
|---|---|---|---|---|
| Water waves [water] | 0 (FACT, include/sh4der_water.h:11) | Sea demo mesh: 2,016 to 23,040 triangles (FACT, demo/OCEAN.md:122-126) | Not measured (OPEN). A count: a sine-cosine pair a wave a point (ESTIMATE, include/sh4der_water.h:11-12) | REPORTED: the sea demo runs on a real console (demo/OCEAN.md:5). Frame time OPEN |
| Mist bank [mist] | 8 KB, the 64 x 64 16-bit noise texture you make (FACT, include/sh4der_mist.h:14) | 4 triangles a sheet, 48 at the default (FACT, include/sh4der_mist.h:15) | Not measured (OPEN). A count: about 120 multiplies and a sine-cosine pair a sheet (ESTIMATE, include/sh4der_mist.h:16-17) | Run on a console by the owner in Match Light (her test; no measured numbers recorded yet; released in the public Match Light film). Fill OPEN |
| Drifting specks [motes] | 512 B for the round mask, optional (FACT, include/sh4der_motes.h:34) | 4 vertices a sprite; 96 sprites, 12.3 KB of TA data a frame (FACT, README "The motes effect") | Not measured (OPEN). A count: one hash set and one position a live slot (ESTIMATE, README) | Run on a console by the owner in Match Light (her test; no measured numbers recorded yet; released in the public Match Light film) |
| Rising lights [rise] | 0 of its own (FACT, include/sh4der_rise.h:14) | At most one quad a light, additive (FACT, include/sh4der_rise.h:15-16) | Not measured (OPEN). A count: about 50 multiplies and 6 hashes a light (ESTIMATE, include/sh4der_rise.h:14-15) | Run on a console by the owner in Match Light (her test; no measured numbers recorded yet; released in the public Match Light film) |
| Cloud deck [cloudvolume] | 16 KB as an 8-bit palette, or 32 KB as RGB565, at 128 x 128 (FACT, include/sh4der_cloudvolume.h:16-17) | One full-screen quad (FACT, include/sh4der_cloudvolume.h:21) | 14 to 22 ms a frame at the default (ESTIMATE, wiki/effects/cloudvolume.md:32) | NOT RUN on a console. The SH-4 figure is an ESTIMATE |
| Dented ground [deform] | 0 for the grid (FACT, include/sh4der_deform.h:16) | None of its own: it gives the height under your mesh (README "The deform effect") | Not measured (OPEN). A count: four byte loads a ground vertex (ESTIMATE, include/sh4der_deform.h:19-20) | Run on a console by the owner in Match Light (her test; no measured numbers recorded yet; released in the public Match Light film) |

Video memory is the picture memory the graphics chip reads from. The machine has 8 MB of it (FACT, [glossary](../glossary.md)). A triangle is one flat shape with three corners, called vertices.

RAM is not in the table:

- Water: 28 B a wave (FACT, include/sh4der_water.h:11).
- Mist: 68 B of state (ESTIMATE, a host `sizeof` run, wiki/effects/mist.md:34).
- Motes: no state; a 32 B collect buffer a sprite, 3,072 B at 96 (FACT, include/sh4der_motes.h:33).
- Rise: an 84 B parameter block and 0 B a light (FACT, include/sh4der_rise.h:13).
- Cloud deck: 66,560 B of maps, a 1,072 B frame block and 24 KB of buffers (FACT, include/sh4der_cloudvolume.h:14-16). The effect page's total, about 143 KB, does not add up from these parts (wiki/effects/cloudvolume.md:30). OPEN until checked.
- Deform: 25,600 B for the 160 x 160 grid, 644 B for 32 prints (FACT, include/sh4der_deform.h:13-16).

For scale: one full-screen translucent layer at 640 x 480 adds about 3,112 us of PVR time (PVR-002, MEASURED, one console; the ledger row has no date, wiki/hardware/ledger-pvr.md:36). The cloud deck draws one such quad, so it should cost about that much. That is an ESTIMATE, not a measurement.

## Try this first

Run on a console by the owner in Match Light (her test; no measured numbers recorded yet; released in the public Match Light film). Start with the drifting specks [motes]: they need no state for each speck and a 512-byte mask. AGENTS.md has no call order for motes, so these steps follow the header's per-frame order (include/sh4der_motes.h:29-31). The shape all call orders share is in [call orders](call-orders.md).

1. Add `sh4der_motes.h` and `src/sh4der_motes.c` to the build with KallistiOS and SH4ZAM (the maths library), as [setup](setup.md) says. Credit the library as [licence and credit](license-and-credit.md) asks.
2. At load, `sh4der_motes_params_default` fills the parameters; check `sh4der_motes_params_valid` returns 1. Set one emitter: an `anchor` near the player and a `seed`.
3. At load, `sh4der_motes_mask_make` fills 256 values for a 16 x 16 round mask. Load them into video memory as a twiddled ARGB1555 texture, then call `sh4der_motes_init_masked`. It returns 1 when the header passes.
4. Each frame, fill the view and screen as the spark recipe does (AGENTS.md "Call order: spark"). Call `sh4der_motes_collect` with the parameters, the emitter, the time in seconds, the view, the screen and an array of `SH4DER_MOTES_DRAW_MAX` entries. It returns how many sprites it kept.
5. Inside an open punch-through list, after the world, call `sh4der_motes_draw_masked(draw, n, 1, &view, &screen)`. The `1` sends the flat specks too.
6. On the console, the viewer should see round dots that drift, fall, and fade in and out with no pop. Square dots would point to a mask that did not load. Time the collect and the draw with the SH-4 cycle counter, and log them as MEASURED with the date, console count and photo, as [benchmarking](../methods/benchmarking.md) asks.

## Related

- [What do you want to make?](what-do-you-want.md): pick an effect by the picture you want.
- [Effect index](../effects/README.md): every effect in one table.
- [Recommended limits](../limits/recommended-limits.md): the limits to keep, with their numbers.
- [Glossary](../glossary.md): PVR, translucent list, video memory and other terms.
- [Licence and credit](license-and-credit.md): what to credit when you use the library.
- [Demo guide](demo-guide.md): the sunset sea demo, the short film and the cloud deck demo.
- [Scenes](../limits/scenes.md) and [maturity](../status/maturity.md): the scenes behind the numbers, and how far each effect has got.
