---
title: What do you want to make?
tags: [start, guide, effect]
updated: 2026-11-28
sources: [wiki/effects/names.md section 0, wiki/effects/README.md, README.md effect table, demo/psm/suite/suite_13.json]
---
# What do you want to make?

**What you will learn:** which effect to look at for the picture you have in mind, and which genre guide explains it in more depth.
**For:** anyone who wants a visual effect on the Dreamcast and does not yet know the library's names for things.

Pick the thing you want to see. Each row names the effect in plain words first, then its module name in brackets, then the genre guide that covers it. Each effect has its own page with its numbers and their status.

| I want to make | Start with | Genre guide |
|---|---|---|
| A glowing sword, a power cable, a rim of light | Glowing tubes [blade], and the glow round it, Mesh bloom [bloom] | [Light and glow](genre-light-and-glow.md) |
| A lamp, a sun or a neon sign that glows | Screen bloom [glow], Light pools [pool] | [Light and glow](genre-light-and-glow.md) |
| A torch walk through a dark corridor | Screen spotlight [torch] | [Light and glow](genre-light-and-glow.md) |
| Sparks, muzzle flashes, tracers | Sparks and tracers [spark] | [Light and glow](genre-light-and-glow.md) |
| Lightning or an electric arc | Lightning [bolt] | [Light and glow](genre-light-and-glow.md) |
| Dark shafts of light through trees or a window | Dark shafts of light [shafts] | [Light and glow](genre-light-and-glow.md) |
| A sea or a lake | Water waves [water] | [Water and weather](genre-water-and-weather.md) |
| A bank of fog, or mist that has a shape | Mist bank [mist] | [Water and weather](genre-water-and-weather.md) |
| Dust, embers, pollen or snow in the air | Drifting specks [motes] | [Water and weather](genre-water-and-weather.md) |
| Lanterns that lift off and become stars | Rising lights [rise] | [Water and weather](genre-water-and-weather.md) |
| Clouds you fly through | Cloud deck [cloudvolume] | [Water and weather](genre-water-and-weather.md) |
| Footprints or tyre tracks in snow or mud | Dented ground [deform] | [Water and weather](genre-water-and-weather.md) |
| A sun shadow or a lamp shadow on the ground | Sun shadow map [psm] | [Shadows and depth](genre-shadows.md) |
| A few characters' shadows on the ground, fading in and out | Moving shadows drawn by the PVR [psm_a] | [Shadows and depth](genre-shadows.md) |
| Darker corners and creases, without a bake tool | Corner darkening [ao] | [Shadows and depth](genre-shadows.md) |
| Corner shading baked into the scenery at load | Baked corner shading [aobake] | [Shadows and depth](genre-shadows.md) |
| Light glowing through a thin ear, a leaf or wax | Light through thin parts [sss] | [Surfaces and materials](genre-surfaces-and-materials.md) |
| A mirror-like floor, car paint or a wet road | Planar reflection [pl4nar] | [Surfaces and materials](genre-surfaces-and-materials.md) |
| Bumps that catch a moving light | Normal map [bump] | [Surfaces and materials](genre-surfaces-and-materials.md) |
| A flag or a force field with a scrolling ripple | Texture ripple [uvwarp] | [Surfaces and materials](genre-surfaces-and-materials.md) |
| A soft light inside a translucent body | Inner light [holo] | [Surfaces and materials](genre-surfaces-and-materials.md) |
| Exposure that adapts as you walk into a bright doorway | Exposure and grade [look] | [Camera and screen](genre-camera-and-screen.md) |
| Motion blur, or streaks behind a fast thing | Motion blur [trail] | [Camera and screen](genre-camera-and-screen.md) |
| A shockwave, a heat haze, a dream wobble | Screen warp [warp], Heat haze hexagons [hexheat] | [Camera and screen](genre-camera-and-screen.md) |
| Dark corners, film grain, a damage flash | Screen overlays [lens] | [Camera and screen](genre-camera-and-screen.md) |
| Night vision, thermal or an amber display | Screen filters [visor] | [Camera and screen](genre-camera-and-screen.md) |
| Old film, or a video tape that wobbles | Old film wear [film], Tape wobble [timebase] | [Camera and screen](genre-camera-and-screen.md) |
| Blur that keeps one band of the picture sharp | Fake focus blur [focus] | [Camera and screen](genre-camera-and-screen.md) |
| A focus pull, a soft background behind a close-up | Focus pull by texture blur [dofmip] | [Camera and screen](genre-camera-and-screen.md) |
| Smoother edges on a still or slow camera | Smooth edges over time [taa] | [Camera and screen](genre-camera-and-screen.md) |
| A colour fringe at the lens edge | Colour fringe [ca] | [Camera and screen](genre-camera-and-screen.md) |
| A split screen for comparing two looks | Split screen [split] | [Tools and safety](genre-tools-and-safety.md) |
| A crash or hang screen while you develop | Crash and hang screen [fault] | [Tools and safety](genre-tools-and-safety.md) |
| Fewer wasted triangles in a render target | Tile-cut pass [tilecut] | [Tools and safety](genre-tools-and-safety.md) |
| Less SH-4 work when the camera turns over a big ground or terrain | Off-screen skip [viewcull] | [Tools and safety](genre-tools-and-safety.md) |
| A walled place (a courtyard, an arena, a room) that draws nothing past its walls | Small world [world bound] | [Tools and safety](genre-tools-and-safety.md) |
| A racing game: the car, its shadow, its reflection and its lights, as one recipe | Planar reflection [pl4nar], Light pools [pool], Sun shadow map [psm] | [Racing game recipe](../guides/racing-game.md) |

## Before you start

- Build and test on the host first: [Setup](setup.md) and [Host test first](../methods/host-test-first.md).
- Read the [glossary](../glossary.md) for PVR, TA and the other words a page uses.
- Every number on a page carries a status (MEASURED, FACT, REPORTED, ESTIMATE, DISPUTED or OPEN). Most effects have no console number yet: check the [effect index](../effects/README.md) before you trust a cost.
- Use the library under its licence and credit: [Licence and credit](license-and-credit.md).

## Other ways in

- [Demo guide](demo-guide.md): the programs on the demo discs, what each one shows, and which effects it uses.
- [Call orders](call-orders.md): the order of calls for each effect.
- [Recommended limits](../limits/recommended-limits.md): the limits to keep, with the number behind each.
- [Effect index](../effects/README.md): every effect in one table, with its version.
