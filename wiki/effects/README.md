---
title: Effects index
tags: [effect, index]
updated: 2026-11-25
sources: [README.md effect table, README.md:119 versions, include/*.h version lines]
---
# Effects index

**What you will learn:** every effect in the library in one line, how far along it is, and whether any of its cost has been measured on a real console.
**For:** anyone choosing an effect, or checking how much proof stands behind it.

**Versions** (README:119):

- **v1.00**: used in a released game.
- **v0.50**: runs on screen on the demo disc.
- **v0.10**: passes its maths tests.

**Names.** Many names here come from a first use, not the technique. [Names](names.md) gives what each module actually is, the other names people search for, and the plain names proposed for the owner.

"Console-measured cost" means one of our own console runs timed the effect itself. A size worked out from the code does not count, and nor does an emulator figure.

| Effect | What it does | Version | Console-measured cost |
|---|---|---|---|
| [core](core.md) | Shared parts: feather, tube and blocker, breath tracker, glint, colour ramp, SH4ZAM helpers | v1.00 | no |
| [blade](blade.md) | Light tubes, a blocker and a pulse in vertex colours, for a glowing blade | v1.00 | no |
| [holo](holo.md) | An inner light in a translucent body that follows its breathing | v1.00 | no |
| [bloom](bloom.md) | Feathered glow polygons round a blade; ring glows for suns and lamps | v1.00 | no |
| [water](water.md) | Waves, ripple normals, Fresnel, reflected direction, crest transmission | v0.50 | no |
| [look](look.md) | Eye adaptation, a grade as full-screen layers, fog glare | v0.50 | no |
| [frame](frame.md) | Reads the frame back on a grid; stamps each frame with a number | v0.50 | no (the bus rates it rests on are measured) |
| [taa](taa.md) | Jitter, history weight and pan shift for temporal AA | v0.50 | no |
| [glow](glow.md) | Screen bloom from a small bright map; halo, darken layer, god rays | v0.50 | no |
| [warp](warp.md) | The frame through a mesh: shockwave, heat haze, wobble, blur, pixelate, dither | v0.10 | **yes**: h2, one console, 2026-11-25 |
| [trail](trail.md) | The frame before over this one: motion blur and the TAA blend | v0.10 | no |
| [lens](lens.md) | Vignette, film grain, hit wedges, flash | v0.10 | no |
| [focus](focus.md) | Fake depth of field through a vertex-alpha mask | v0.10 | no |
| [visor](visor.md) | Night, amber and thermal visor looks | v0.10 | no |
| [torch](torch.md) | A flashlight carved on screen, with fog bent to black | v0.50 | no |
| [guard](guard.md) | Drops zero-area triangles and uncompiled headers | v0.10 | no (the faults were seen on a console) |
| [bump](bump.md) | A normal map lit by blend passes, without the bump unit | v0.14 | no |
| [sss](sss.md) | Back-lit transmission, per vertex | v0.50 | no |
| [uvwarp](uvwarp.md) | Per-vertex texture offset from a small scrolling table | v0.10 | no |
| [spark](spark.md) | A pool of 24 additive camera-facing quads | v0.10 | no |
| [bolt](bolt.md) | Branching lightning strands that re-roll | v0.10 | no |
| [motes](motes.md) | Stateless drifting specks | v0.10 | no |
| [pl4nar](pl4nar.md) | Planar reflection through a small render target | v0.10 | **yes**: the lookups, pl4m c5 (PVR-160), one console, one run, 2026-11-25; its PVR columns are ESTIMATE (PVR-162) |
| [pool](pool.md) | Light pools and blob shadows as fading fans | v0.10 | no |
| [psm](psm.md) | Shadow map placement; the SH-4 and PVR map routes | v0.10 | **yes**: the town demo HUD, one console, 2026-11-25 |
| [ao](ao.md) | Crease darkening (bake) and a screen cavity cue | v0.10 | no |
| [ca](ca.md) | The frame texture and chromatic aberration | v0.50 | no |
| [shafts](shafts.md) | Dark shapes cast away from a light | v0.50 | no |
| [sh4dow-dc](sh4dow-dc.md) | Soft character shadows by limb columns (separate library) | separate | no: its numbers are the original author's run (REPORTED) |

| [deform](deform.md) | Ground that keeps a dent: footprints and tracks, plus decal prints | v0.10 | no |
| [mist](mist.md) | A fog bank with a shape, as upright translucent sheets | v0.10 | no |
| [rise](rise.md) | Lights that lift off, drift up and become stars | v0.10 | no |
| [film](film.md) | Old projector wear: flicker, scratches, dust, gate weave | v0.1 | no |
| [cloudvolume](cloudvolume.md) | A ray-marched cloud deck you fly through | v0.50 | no |

The last five are in `README.md`'s effect table and in `include/`, but not yet in `AGENTS.md`'s file list.

## Related

- [Call orders](../start-here/call-orders.md)
- [Budgets](../limits/budgets.md), [recommended limits](../limits/recommended-limits.md)
- [Open questions](../open-questions.md)
