---
title: Effects index
tags: [effect, index]
updated: 2026-11-28
sources: [README.md effect table, README.md:119 versions, include/*.h version lines]
---
# Effects index

**What you will learn:** every effect in the library in one line, how far along it is, and whether any of its cost has been measured on a real console.
**For:** anyone choosing an effect, or checking how much proof stands behind it.

**Versions** (README:119):

- **v1.00**: used in a released game.
- **v0.50**: runs on screen on the demo disc.
- **v0.10**: passes its maths tests.

**Start with a goal.** [What do you want to make?](../start-here/what-do-you-want.md) groups these effects into six genres, each with starting settings: [light and glow](../start-here/genre-light-and-glow.md), [water and weather](../start-here/genre-water-and-weather.md), [shadows](../start-here/genre-shadows.md), [surfaces and materials](../start-here/genre-surfaces-and-materials.md), [camera and screen](../start-here/genre-camera-and-screen.md), [tools and safety](../start-here/genre-tools-and-safety.md).

**Names.** Many names here come from a first use, not the technique. [Names](names.md) gives what each module actually is, the other names people search for, and the plain names proposed for the owner.

"Console-measured cost" means one of our own console runs timed the effect itself. A size worked out from the code does not count, and nor does an emulator figure.

| Effect | What it does | Version | Console-measured cost |
|---|---|---|---|
| [Shared parts [core]](core.md) | Shared parts: feather, tube and blocker, breath tracker, glint, colour ramp, SH4ZAM helpers | v1.00 | no |
| [Glowing tubes [blade]](blade.md) | Light tubes, a blocker and a pulse in vertex colours, for a glowing blade | v1.00 | no |
| [Inner light [holo]](holo.md) | An inner light in a translucent body that follows its breathing | v1.00 | no |
| [Mesh bloom [bloom]](bloom.md) | Feathered glow polygons round a blade; ring glows for suns and lamps | v1.00 | no |
| [Water waves [water]](water.md) | Waves, ripple normals, Fresnel, reflected direction, crest transmission | v0.50 | no |
| [Exposure and grade [look]](look.md) | Eye adaptation, a grade as full-screen layers, fog glare | v0.50 | no |
| [Frame read [frame]](frame.md) | Reads the frame back on a grid; stamps each frame with a number | v0.50 | no (the bus rates it rests on are measured) |
| [Smooth edges over time [taa]](taa.md) | Jitter, history weight and pan shift for temporal AA | v0.50 | no |
| [Screen bloom [glow]](glow.md) | Screen bloom from a small bright map; halo, darken layer, god rays | v0.50 | no |
| [Screen warp [warp]](warp.md) | The frame through a mesh: shockwave, heat haze, wobble, blur, pixelate, dither | v0.10 | **yes**: h2, one console, 2026-11-25 |
| [Motion blur [trail]](trail.md) | The frame before over this one: motion blur and the TAA blend | v0.10 | no |
| [Screen overlays [lens]](lens.md) | Vignette, film grain, hit wedges, flash | v0.10 | no |
| [Fake focus blur [focus]](focus.md) | Fake depth of field through a vertex-alpha mask | v0.10 | no |
| [Focus pull by texture blur [dofmip]](dofmip.md) | Depth-driven focus blur: a mip bias a polygon from its distance to a focus plane | v0.2 | no |
| [Screen filters [visor]](visor.md) | Night, amber and thermal visor looks | v0.10 | no |
| [Screen spotlight [torch]](torch.md) | A flashlight carved on screen, with fog bent to black | v0.50 | no |
| [Send checks [guard]](guard.md) | Drops zero-area triangles and uncompiled headers | v0.10 | no (the faults were seen on a console) |
| [Normal map [bump]](bump.md) | A normal map lit by blend passes, without the bump unit | v0.14 | no |
| [Light through thin parts [sss]](sss.md) | Back-lit transmission, per vertex | v0.50 | no |
| [Texture ripple [uvwarp]](uvwarp.md) | Per-vertex texture offset from a small scrolling table | v0.10 | no |
| [Sparks and tracers [spark]](spark.md) | A pool of 24 additive camera-facing quads | v0.10 | no |
| [Lightning [bolt]](bolt.md) | Branching lightning strands that re-roll | v0.10 | no |
| [Drifting specks [motes]](motes.md) | Stateless drifting specks | v0.10 | no |
| [Planar reflection [pl4nar]](pl4nar.md) | Planar reflection through a small render target | v0.10 | **yes**: the lookups, Street car demo [pl4m] c5 (PVR-160), one console, one run, 2026-11-25; its PVR columns are ESTIMATE (PVR-162) |
| [Light pools [pool]](pool.md) | Light pools and blob shadows as fading fans | v0.10 | no |
| [softfade](softfade.md) | Soft edges for light: a glow, a pool or a beam fades at a wall the SH-4 knows | v0.10 | no |
| [page](page.md) | Result pages for the console tests: text with a CRC line, graphs, drawn after the PVR is shut down | ALPHA v0.5 | yes: VID-015 and VID-016 (one run each, one console) |
| [fxlabel](fxlabel.md) | The effect label line: each effect by its name, who does the work and when | ALPHA v0.4 / v0.5 | yes: VID-016 (eight programs, three discs) |
| [qrpage](qrpage.md) | The QR test set on result pages, read back from a photograph | ALPHA v0.5 | yes: VID-014 (one run, one phone photograph set) |
| [Distance ramp [ramp]](ramp.md) | A linear 0..1 ramp between two distances (a helper) | v0.10 | no |
| [Off-screen skip [viewcull]](viewcull.md) | Leaves out a box or a block of ground that is wholly off the screen before it is placed (a helper) | PROTOTYPE v0.1 | no |
| [Crash and hang screen [fault]](fault.md) | A report on the screen for an exception or a hang: registers, calls, breadcrumbs | PROTOTYPE v0.1 | no: no run has shown it catching a fault (PVR-183, PVR-208 froze with no HANG screen) |
| [Split screen [split]](split.md) | A movable divider for a left and a right half (tile columns, held triggers) | ALPHA v0.2 | **yes**: the middle position (PVR-144, PVR-161), one console; moving it OPEN |
| [Tile-cut pass [tilecut]](tilecut.md) | A render target pass through a tile table its own size | ALPHA v0.5 | **yes**: Full compare chain on a console [c4]; Room shading, four ways [ao4], build h; Glow that meets a wall [d1], build c; Glow that meets a wall, build d (PVR-092, PVR-156, PVR-192, PVR-205), one console |
| [Moving shadows drawn by the PVR [psm_a]](psm-route-a.md) | Route A as a library pass: four casters, one light, a held 128 map, the tile cut at a row origin | v0.1 | no |
| [Sun shadow map [psm]](psm.md) | Shadow map placement; the SH-4 and PVR map routes | v0.10 | **yes**: the town demo HUD, one console, 2026-11-25 |
| [Corner darkening [ao]](ao.md) | Crease darkening (bake) and a screen cavity cue | v0.10 | no |
| [Baked corner shading [aobake]](aobake.md) | Baked ambient occlusion against exact shapes, at load (the console bake) | ALPHA v0.5 | **yes**: the bake in the scenes of Room shading, four ways (SH4-076, SH4-082), one console |
| [fakeaf](fakeaf.md) | Fake anisotropic filtering: a mip bias a polygon by slope, and distance ranges with stretched copies | ALPHA v0.4 | **yes**: the bias's cost (PVR-180, PVR-207, one console); the level it picks not read back |
| [ground](ground.md) | A paving slab texture made at load from a seed (64 x 64 RGB565, tiles both ways) | PROTOTYPE v0.1 | no: host tested; its load time NOT MEASURED |
| [aomesh](aomesh.md) | Baked ambient occlusion over a triangle mesh, at load (the bake for Walled town square [square]) | PROTOTYPE v0.1 | no: the bake's time and look are NOT MEASURED on a console |
| [A ray against a triangle [raytri]](raytri.md) | One ray-triangle kernel shared by the mesh bake and the ray bench: a distance by the reciprocal form, a hit with no divide | PROTOTYPE v0.1 | no: NOT MEASURED (a routine; its time on a console is not measured) |
| [Test disc menu [suite]](suite.md) | A menu and many test programs on one disc, with a way back after each | ALPHA v0.4 | **yes**: a menu start repeated a test's results (PVR-116, one console); the return and the chain are not measured |
| [Menu logic [menu]](menu.md) | Menu rules with nothing drawn: a list with headings, tabs, a scrolled text, a held button | PROTOTYPE v0.1 | no: new, not yet on a disc |
| [Sky shading [aosky]](aosky.md) | Occlusion of the sky drawn into the sun's shadow target (AO method 2) | PROTOTYPE v0.1 | no: the start settings ran in Room shading, four ways (PVR-140, one console); this library copy has not |
| [Bake tool [ao-bake]](ao-bake.md) | Host build tool: baked occlusion over any triangle list (not a console effect) | v0.1 | no |
| [Colour fringe [ca]](ca.md) | The frame texture and chromatic aberration | v0.50 | no |
| [Dark shafts of light [shafts]](shafts.md) | Dark shapes cast away from a light | v0.50 | no |
| [Soft character shadows [sh4dow]](sh4dow.md) | Soft character shadows by limb columns, foot cones and a heightfield sun, in this library | not stated in the source | no: the shade's numbers are REPORTED (one run); the foot cones and packed loops are NOT MEASURED |
| [Dented ground [deform]](deform.md) | Ground that keeps a dent: footprints and tracks, plus decal prints | v0.10 | no |
| [Mist bank [mist]](mist.md) | A fog bank with a shape, as upright translucent sheets | v0.10 | no |
| [Rising lights [rise]](rise.md) | Lights that lift off, drift up and become stars | v0.10 | no |
| [Old film wear [film]](film.md) | Old projector wear: flicker, scratches, dust, gate weave | v0.1 | no |
| [Tape wobble [timebase]](timebase.md) | A video tape's bent picture: one sideways shift a row, drawn as bands of the frame texture | v0.1 | no |
| [Heat haze hexagons [hexheat]](hexheat.md) | Heat haze as a honeycomb of small hexagons fixed on the screen, each nudged about 2 pixels at most by a smooth rising wave; the picture comes from a band of a render target | v0.1 | no |
| [Cloud deck [cloudvolume]](cloudvolume.md) | A ray-marched cloud deck you fly through | v0.50 | no |

## Related

- [Call orders](../start-here/call-orders.md)
- [Budgets](../limits/budgets.md), [recommended limits](../limits/recommended-limits.md)
- [Open questions](../open-questions.md)
