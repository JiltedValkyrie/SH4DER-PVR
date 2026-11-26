---
title: "Names: what each module actually is"
tags: [effects, names, index, migration]
updated: 2026-11-25
sources: [include/, src/, test/, README.md "Effects", AGENTS.md "Files"]
---
# Names: what each module actually is

**What you will learn:** the technique behind each module and demo name, the other names people search for, and the plain names proposed in place of the old ones, with how the old names keep working.
**For:** anyone looking for an effect by what it does, and anyone who will update code to the plain names.

Many module names come from the first use the module had: "blade", "holo", "visor", "torch". These names say nothing about the technique. This page gives the technique first; the use comes after, as an example only.

**Status: ADOPTED for the menu titles** (lead, for the owner, 2026-11-25): the suite menu's titles take the plain names, from s12 (built, superseded before a run) and in s13. The code is not renamed: the compatibility aliases (the migration lines below) are deferred, so the old names stay the only names in the code until they land.

## 0. Reader names used on the wiki pages

**Status: ADOPTED for wiki pages** (2026-11-26, the wiki's plain-names pass). Every wiki page calls an effect by its reader name first and puts the module name in brackets once, for example "Glowing tubes [blade]". The code keeps its names, and the file and macro renames in section 2 stay PROPOSED. Genre guides group the effects as shown below.

Genre guides: [light and glow](../start-here/genre-light-and-glow.md), [water and weather](../start-here/genre-water-and-weather.md), [shadows](../start-here/genre-shadows.md), [surfaces and materials](../start-here/genre-surfaces-and-materials.md), [camera and screen](../start-here/genre-camera-and-screen.md), [tools and safety](../start-here/genre-tools-and-safety.md). Each effect sits in one genre.

| Module | Reader name | In plain words | Genre |
|---|---|---|---|
| blade | Glowing tubes | Soft light tubes that orbit, masked by a sliding ball; for a glowing blade or a power cable | Light and glow |
| bloom | Mesh bloom | Glow made of flat feathered shapes round a point or along a line | Light and glow |
| glow | Screen bloom | Glow and light rays from the bright parts of the picture | Light and glow |
| torch | Screen spotlight | A flashlight circle cut into the picture: dark outside, bright core | Light and glow |
| pool | Light pools | Fading round or wedge-shaped light on the ground: lamp pools, headlight beams, blob shadows | Light and glow |
| shafts | Dark shafts of light | Dark shapes cast as rays away from a light, read from the picture | Light and glow |
| softfade | Soft edges for light | A glow, a pool or a beam that fades out at a wall or a floor, not on a hard line | Light and glow |
| spark | Sparks and tracers | A pool of short bright sprites: sparks, flashes, streaks | Light and glow |
| bolt | Lightning | Branching strands that re-draw a few times a second | Light and glow |
| water | Water waves | A sea surface from waves: shape, reflection, sun glints | Water and weather |
| mist | Mist bank | Stacked see-through sheets that make a fog bank with a shape | Water and weather |
| motes | Drifting specks | Dust, embers and pollen floating; nothing stored per speck | Water and weather |
| rise | Rising lights | Lanterns that lift off, drift up and settle as stars | Water and weather |
| cloudvolume | Cloud deck | A cloud slab you fly through, drawn from a small buffer | Water and weather |
| deform | Dented ground | Snow or mud keeps footprints and tracks | Water and weather |
| psm | Sun shadow map | Shadow map fitting for a sun or a lamp: uniform or perspective (PSM) | Shadows |
| psm_raster | Shadow map on the SH-4 | The shadow map drawn into RAM by the main CPU and compared per vertex | Shadows |
| psm_pvr | Shadow map drawn by the PVR | The shadow map drawn by the graphics chip from the casters' shapes | Shadows |
| ao | Corner darkening | Darkens creases and corners: for a bake, and a cue from the picture (ambient occlusion) | Shadows |
| aobake | Baked corner shading | Corner shading baked into vertex colour at load, against exact shapes | Shadows |
| fakeaf | Sharper floors at an angle | A floor or road seen at a grazing angle kept sharp across the view, by a mip bias per polygon from its slope | Surfaces and materials |
| ground | Paving slab texture | A tiling texture of stone slabs made from a seed at load, so a floor needs no file | Surfaces and materials |
| aomesh | Mesh corner shading | Corner shading baked at load against a whole triangle mesh (walls, buildings), with a byte grid for the ground | Shadows |
| aosky | Sky shading | Darkens flat ground under props, drawn into the sun's shadow target | Shadows |
| sh4dow | Soft character shadows | Soft shadows on characters from limb columns, foot cones and a heightfield sun (folded into this library; it began as SH4DOW-DC) | Shadows |
| bump | Normal map | Lit surface bumps with no bump unit: blend passes | Surfaces and materials |
| pl4nar | Planar reflection | Mirror reflection of the world in a floor, car paint or a wet road | Surfaces and materials |
| uvwarp | Texture ripple | Moves texture coordinates per vertex from a small scrolling table | Surfaces and materials |
| sss | Light through thin parts | Glow where light passes through thin skin, leaves, wax or water (translucency) | Surfaces and materials |
| holo | Inner light | A soft light inside a translucent body that follows its breathing | Surfaces and materials |
| look | Exposure and grade | Brightness that adapts to the scene, plus a colour grade | Camera and screen |
| taa | Smooth edges over time | Blends frames over time to soften jagged edges (TAA) | Camera and screen |
| trail | Motion blur | Blends the last frame over this one: streaks behind moving things | Camera and screen |
| lens | Screen overlays | Vignette (dark corners), film grain, hit wedges, a flash | Camera and screen |
| warp | Screen warp | Bends the whole picture: shockwave, heat haze, wobble, blur, mosaic | Camera and screen |
| hexheat | Heat haze hexagons | Heat shimmer over a far view: a honeycomb of small hexagons, each nudged a little by a smooth rising wave | Camera and screen |
| ca | Colour fringe | Chromatic aberration: coloured edges through a render of the frame | Camera and screen |
| film | Old film wear | Flicker, scratches, dust and gate weave of old film | Camera and screen |
| timebase | Tape wobble | A video tape's timing wobble: each row shifted sideways | Camera and screen |
| visor | Screen filters | Night, amber and thermal looks from number tables | Camera and screen |
| focus | Fake focus blur | Blurs most of the picture and keeps one band or disc sharp; reads no depth | Camera and screen |
| guard | Send checks | Drops empty triangles and polygon headers that were never built | Tools and safety |
| fault | Crash and hang screen | A report on screen when a program crashes, hangs or stops | Tools and safety |
| tilecut | Tile-cut pass | A render pass through a tile table its own size, so small targets cost less | Tools and safety |
| pvrx | PVR extras | Extra calls over the PVR driver: waits, VRAM copy by DMA, error counters, clip | Tools and safety |
| page | Result page | Text with a checksum line, or a graph (lines, bars), for a console test's numbers | Tools and safety |
| fxlabel | Effect label line | Each effect by its name, who does the work and when, in small text on a result or live page | Tools and safety |
| qrpage | QR test set | 16 codes of known payloads, drawn on a result page, to test a photograph reader | Tools and safety |
| ramp | Distance ramp | A 0 to 1 fade between two distances | Tools and safety |
| split | Split screen | A divider that cuts the screen into two halves | Tools and safety |
| frame | Frame read | Reads the picture on screen back so other effects can use it | Tools and safety |
| core | Shared parts | The feather, tube shape, breath tracker and glint that the others share | Tools and safety |
| ao-bake | Bake tool | A host tool that bakes corner shading into a triangle list at build time; not a console effect | Tools and safety |

## 1. What each module is

The "What it actually is" column is also the proposed text for a "What it is" column in README's effect table (section 5).

| Module | What it actually is | Other names people search for | Typical uses (examples only) | Header and test |
|---|---|---|---|---|
| blade | Animated additive glow on a chosen material group: soft tubes of light orbit an axis, a soft ball slides through them along a helix and masks them, a pulse over the whole | energy glow, orbiting light bands, rim swirl, emissive animation | a glowing sword, a power cable, a reactor rod | `sh4der_blade.h`, `src/sh4der_blade.c`; `test/blade_test.c` |
| holo | A soft ball of light inside a translucent body: a weight a vertex at load, a colour table a frame, brightness following the pose's breathing | inner glow, emissive core, heartbeat light | a hologram's chest, a glowing creature, a lantern body | `sh4der_holo.h`, `src/sh4der_holo.c`; `test/holo_test.c` |
| bloom | Bloom made of geometry: a feathered camera-facing ribbon and disc (24 triangles), or rings with their own level each | halo, light bloom, glow sprite, lens-flare ghost, sun disc | a glow round a blade, a lamp, a sun | `sh4der_bloom.h`, `src/sh4der_bloom.c`; `test/bloom_test.c` |
| water | A wave surface as a sum of directional sine waves: height, slope, normal, Fresnel, reflected direction, crest transmission | ocean shader, Gerstner-like waves, wave normals | a sea, a lake, a puddle | `sh4der_water.h`; `test/water_test.c` |
| look | Auto exposure and colour grade after the frame is drawn: the frame's log-average luma from a read grid, an eye that adapts both ways, a per-channel line drawn as full-screen layers, glare through the fog table | eye adaptation, tone mapping, HDR exposure, colour grading, LUT | any lit scene; a dark-to-bright doorway | `sh4der_look.h`; `test/look_test.c` |
| frame | Frame readback: the picture on screen read on a grid, and a colour stamp on each frame so a read knows which frame it saw | framebuffer read, scan-out read, frame tag | feeding exposure, bloom, shafts | `sh4der_frame.h`, `src/sh4der_frame.c`; `test/frame_test.c` |
| taa | Temporal anti-aliasing maths: sub-pixel jitter, history weight, pan shift | TAA, temporal AA, jitter AA | edge smoothing on a still or slow camera | `sh4der_taa.h`; `test/taa_test.c` |
| glow | Screen bloom from a small bright map of the frame (bright parts kept, dark parts dropped), a wide halo, a balancing darken layer, god rays as scaled copies about the sun | bloom, screen glow, light shafts, crepuscular rays, sun rays | sunlight, neon, explosions | `sh4der_glow.h`, `src/sh4der_glow.c`; `test/glow_test.c` |
| warp | The frame as a texture through a moving mesh: ring, column and wobble displacement, radial blur as scaled copies, pixelate, ordered dither | screen distortion, heat haze, shockwave, refraction, underwater, zoom blur, mosaic | an explosion ring, heat over a road, a dream | `sh4der_warp.h`, `src/sh4der_warp.c`; `test/warp_test.c` |
| trail | Frame feedback: last frame's picture, read from the front buffer, blended over this one (a column mask, two passes a copy) | motion blur, frame blend, ghosting, accumulation | speed blur, the TAA history | `sh4der_trail.h`, `src/sh4der_trail.c`; `test/trail_test.c` |
| lens | Screen overlays with no frame read: a vignette mesh, mean-neutral film grain, direction wedges, a full-screen flash | vignette, film grain, damage indicator, screen flash | a camera feel, a hit from one side | `sh4der_lens.h`, `src/sh4der_lens.c`; `test/lens_test.c` |
| focus | Region blur: a small blurred copy of the frame drawn through a vertex-alpha mask that keeps a disc, band or rectangle sharp. It reads no depth | fake depth of field, DoF, tilt-shift, focus pull | a cutscene focus, a menu backdrop | `sh4der_focus.h`, `src/sh4der_focus.c`; `test/focus_test.c` |
| visor | Preset screen filters as number tables: a tint through the grade, grain, scanlines, a vignette, a luma ramp for a false-colour look, a fade in and out | night vision, thermal vision, colour filter, CRT scanlines | a night mode, a thermal camera, an amber display | `sh4der_visor.h`, `src/sh4der_visor.c`; `test/visor_test.c` |
| torch | A spotlight carved in screen space: a multiply layer darkens outside a circle, a boost layer lifts a hot core, the fog table bends to black for reach, gated by the scene's darkness | flashlight, torch, cone light, headlamp | a dark corridor walk | `sh4der_torch.h`, `src/sh4der_torch.c`; `test/torch_test.c` |
| pvrx | Extras over KallistiOS's PVR driver: a polled render-done wait, a PVR-DMA download of video memory, TA error counters, the Holly revision, a user clip change | PVR helpers, render-to-texture readback, VRAM DMA | any program that waits on or reads back the PVR | `sh4der_pvrx.h`, `src/sh4der_pvrx.c`; `test/pvrx_test.c` |
| guard | Pre-send checks: drop a triangle with no screen area, refuse a polygon header that was never compiled | validation, degenerate triangle cull, sanity checks | every draw path | `sh4der_guard.h`; `test/guard_test.c` |
| bump | Normal mapping by ordinary blend passes, with no bump unit: pack and decode a normal map, light a vertex from it, send a polygon | normal map, bump map, dot3, per-pixel lighting | a floor or wall under a moving light | `sh4der_bump.h`; no test of its own (`demo/torchwalk_ab.c` shows it) |
| sss | Back-lit transmission: light through a thin part toward the eye, tinted by absorption over a thickness the caller supplies. A per-vertex stand-in, not a multiple-scattering solve | subsurface scattering, SSS, translucency, back light | an ear against the sun, a leaf, a wave crest, wax | `sh4der_sss.h`; `test/sss_test.c` |
| uvwarp | Per-vertex texture coordinate offset from a small two-channel table, scrolled, bilinear, wrapped | UV distortion, flow map, UV scroll, heat ripple on a texture | a flag, water ripples on a texture, a force field | `sh4der_uvwarp.h`; `test/uvwarp_test.c` |
| spark | A fixed pool of short-lived additive camera-facing quads: billboards and streaks, with gravity, a fading tail and a keep rule | particles, billboards, sprites, tracers, muzzle flash | sparks, flashes, tracers, lamp halos | `sh4der_spark.h`, `src/sh4der_spark.c`; `test/spark_test.c` |
| bolt | Branching jagged strands that re-draw part of themselves a few times a second | lightning, electric arc, tesla, procedural lightning | a storm, an arc between coils | `sh4der_bolt.h`, `src/sh4der_bolt.c`; `test/bolt_test.c` |
| motes | A stateless particle field: every speck's state is a pure function of (slot, time) through an integer hash, so nothing is stored per speck | ambient particles, dust motes, embers, snow | dust in a beam, ash, pollen | `sh4der_motes.h`, `src/sh4der_motes.c`; `test/motes_test.c` |
| deform | A dent grid: a byte-a-cell depth grid under a ground mesh, stamped by contacts, with a raised rim, slow refill and footprint decals | deformable terrain, snow trails, footprints, tyre tracks | snow, mud, sand | `sh4der_deform.h`; `test/deform_test.c` |
| mist | A fog volume as stacked translucent slices with drifting noise and a fading top edge | volumetric fog, fog bank, ground fog, fog cards | a misty field, a swamp | `sh4der_mist.h`; `test/mist_test.c` |
| rise | Stateless rising lights: points that rest, lift off in turn, drift up with sway, shrink, cool and settle as twinkling stars; every value from (light, time, seed) | floating lanterns, fireflies, sky lanterns, embers | a lantern festival, candles | `sh4der_rise.h`; `test/rise_test.c` |
| film | Film wear over a finished picture: exposure flicker, vertical scratches, dust specks, gate weave; no frame read, no state | old film, film grain, sepia, projector look | a flashback, a title | `sh4der_film.h`; `test/film_test.c` |
| timebase | A video tape deck's line timing errors: one sideways shift a row (top hook, flutter, head switching step), drawn as bands of the frame texture; a port of parts of NTSCRT by Finn McKenty (MIT) | tape look, VHS, time-base error, tracking, tape warp | a cutscene on tape, a security monitor | `sh4der_timebase.h`; `sh4der_ca_channel_rows` in `sh4der_ca.h`; `test/timebase_test.c` |
| hexheat | Heat haze as a lattice of hexagons fixed on the screen: each cell is judged at its world point (a ray from the camera to the ground, the pick a strategy game does for the mouse) and moved up to a quarter of its size by the gradient of a stack of rising waves; its picture comes from a band the client draws into a render target it already fills | heat haze, heat shimmer, mirage, refraction, screen distortion, screen to world, ray to ground | a hot road's far end, a desert horizon | `sh4der_hexheat.h`; `test/hexheat_test.c` |
| cloudvolume | A CPU ray march through a cloud slab (cover map, noise erosion, sun light) into a small 8-bit buffer drawn as one filtered quad | volumetric clouds, cloud ray marching | a flight through a cloud deck | `sh4der_cloudvolume.h`; `test/cloudvolume_test.c` |
| pl4nar | Planar reflection: the world mirrored about a plane into a small render target, looked up per vertex, blended as one layer with Fresnel alpha | planar reflection, mirror, render-to-texture reflection, environment reflection | car paint, glass, a wet road, a floor | `sh4der_pl4nar.h`, `src/sh4der_pl4nar.c`; `test/pl4nar_test.c` |
| fault | A report on the screen when a program raises an exception, hangs, asserts or exits: registers, the stack's calls, the last frame's breadcrumbs | crash screen, hang screen, fault page, crash dump | a test image that dies with nothing on screen | `sh4der_fault.h`; `test/fault_test.c` |
| split | A screen cut in a left and a right half by a divider the triggers move, in 32-pixel tile columns, each half its own user clip | split screen, A/B wipe, comparison slider | a look on one half and not the other | `sh4der_split.h`; `test/split_test.c` |
| tilecut | A render target pass through a tile table its own size: the screen's tile tables end after the target's tiles for one pass, then every word is put back | tile list trim, region array cut, partial tile render | a mirror, a shadow tile, a small depth or glow target | `sh4der_tilecut.h`; `test/tilecut_test.c` |
| page | Text pages with a CRC line, and graph pages (lines, bars), drawn into the frame buffer after the PVR is shut down | result page, test page, CRC page, graph page | a console test's numbers on screen | `sh4der_page.h`; `test/page_test.c` |
| fxlabel | The effect label line: each effect by its name, who does the work and when | effect label, credits line, feature list | a demo screen's label for each mode | `sh4der_fxlabel.h`; `test/fxlabel_test.c`, `demo/psm/test/fx_tables_test.c` |
| qrpage | 16 QR codes of known payloads at four module sizes, drawn on a result page | QR test, code test page | a photograph test of the page reader | `sh4der_qrpage.h`; `test/qrpage_test.c` (make test-demo) |
| ramp | A linear 0..1 ramp between two distances: one call, or prepared for a loop | distance fade, range weight, falloff | a shadow's fade by camera distance, a fake AF range weight | `sh4der_ramp.h`; `test/ramp_test.c` |
| softfade | A per-vertex fade of a light near planes and rectangles the caller names: a near test, a fit for a quad, a smooth step a vertex | soft halo, soft light edge, wall fade (no depth is read: not the camera depth fade) | a lamp's halo in a corner, a headlight beam at a barrier | `sh4der_softfade.h`; `test/softfade_test.c` |
| pool | Fading fan decals on a plane: a disc or a wedge of triangles whose vertex alpha fades to nothing | light pool, fake light, blob shadow, projected decal | a street lamp's pool, a headlight beam, a car's shadow | `sh4der_pool.h`; `test/pool_test.c` |
| psm | Shadow map fitting for a directional light: a uniform map and a perspective shadow map, one world-to-map matrix, the depth key, the bias | shadow map, PSM, perspective shadow map, light space fit | any sun shadow | `sh4der_psm.h`; `test/psm_test.c` |
| psm_raster | A shadow map rendered on the SH-4 into RAM: caster triangles as 16-bit depth, a compare a receiver vertex | software shadow map, CPU shadow map, depth compare | a shadow at vertex detail with self-shadowing | `sh4der_psm_raster.h`; `test/psm_raster_test.c` |
| psm_pvr | A shadow map drawn by the PVR: the SH-4 clips the casters for a render target and gives the ground its lookup coordinates; no depth compare | PVR shadow map, silhouette map, projected shadow texture | ground shadows from casters, no self-shadowing | `sh4der_psm_pvr.h`; `test/psm_pvr_test.c` |
| ao | Small-scale occlusion: crease darkening from the angle between faces, for a bake, and a screen cavity cue from a small map | ambient occlusion, AO, cavity, crease shading | corners, folds, contact darkening | `sh4der_ao.h`; `test/ao_test.c` |
| aobake | A baked AO byte a vertex, cast against exact spheres, boxes and a plane, at load | baked ambient occlusion, contact shadow bake, AO bake | static scenery's vertex colour | `sh4der_aobake.h`; `test/aobake_test.c` |
| fakeaf | Fake anisotropic filtering: a mip D adjust a polygon from its slope, and distance ranges with copies of the texture squashed along the view | anisotropic filtering, mip bias, texture LOD bias, floor blur | a long floor, corridor or road at a grazing angle | `sh4der_fakeaf.h`; `test/fakeaf_test.c` |
| ground | A 64 x 64 RGB565 paving slab texture from an integer hash of a seed, tiling both ways | procedural texture, paving, floor tiles, slabs | a square's or a street's ground without a texture file | `sh4der_ground.h`; `test/ground_test.c` |
| aomesh | A baked AO byte a vertex, cast against a triangle mesh through a cell grid (12 rays), with a byte grid over the ground, at load | baked occlusion over a mesh, mesh AO bake, ground AO grid | static scenery built from triangles, a walled street | `sh4der_aomesh.h`; `test/aomesh_test.c` |
| aosky | The sky's occlusion drawn as K slid shadows into the sun's shadow target | sky occlusion, sky AO, shadow-target AO | flat ground under props | `sh4der_aosky.h`; `test/aosky_test.c` |
| ao-bake | Baked ambient occlusion over any triangle list, on the host at build time: one byte a vertex from 64 cosine rays | baked AO, occlusion bake, lightmap bake | a room's and a road's vertex bytes | `tools/ao_bake_core.h`; `test/ao_bake_test.c` |
| ca | The frame texture (the world drawn into a render target, then put on screen) and chromatic aberration through it | chromatic aberration, colour fringe, RGB split, render to texture | a lens edge, a hit, a glitch | `sh4der_ca.h`, `src/sh4der_ca.c`; `test/ca_test.c` |
| shafts | Screen-space dark rays: the frame's dark shapes cast away from a light, from the frame read | shadow shafts, occlusion rays, volumetric shadows | trees against a low sun, a window | `sh4der_shafts.h`, `src/sh4der_shafts.c`; `test/shafts_test.c` |
| core | The shared parts: the feather, the tube and blocker shapes, the breath tracker, the glint, the colour ramp, the SH4ZAM switch | — | every module | `sh4der_core.h`, `src/sh4der_core.c`; `test/core_test.c` |

Two helpers that were in `demo/psm` are named by technique already: `fakeaf.h` (fake anisotropic filtering: distance ranges and a mip bias by slope) and `ao/tile_cut.h` (a render target pass through a tile table cut to its own tiles). `fakeaf.h` moved to `include/sh4der_fakeaf.h` on 2026-11-26 (`faf_*` kept, the test `test/fakeaf_test.c`; `demo/psm/fakeaf.h` is a forward); the tile cut moved to `include/sh4der_tilecut.h` on 2026-11-26 (`tile_cut_*` is now `sh4der_tilecut_*`, the test `test/tilecut_test.c`).

### Demo programs

| Program | What it actually is | Other names | Where |
|---|---|---|---|
| pl4nar street demo | A night street drive showing planar reflection on a car body, light pools, halos and shadow tiles | street demo, car reflection demo | `demo/pl4nar/`; `make pl4nar-demo` |
| Match Light | A scripted 120-second film of a snowy night street: a dent grid, a mist bank, rising lights, spark pools and motes | short film, matchlight | `demo/matchlight/`; `make matchlight`; `test/matchlight_scene_test.c` |
| ocean | A sunset sea: the wave surface, crest transmission, sky reflection and soft sun shadow, with live rows | sunset ocean, water demo | `demo/ocean.c`, `demo/ocean_scene.h`; `make ocean`; `test/ocean_scene_test.c` |
| torch walk | A first-person walk over a dark floor: a normal map lit by the screen spotlight and a fired point light, with a cost label | torchwalk, normal map walk | `demo/torchwalk_ab.c`; `make torchwalk-ab`; no tests by design |
| demo disc | Six screen effects as live rows (exposure and grade, screen bloom, spotlight, dark rays, TAA, chromatic aberration) plus SH4DOW-DC | sh4der-demo | `demo/main.c`; `make demo` |

The demo folders and make targets keep their names: the ledger cites their paths.

## 2. Old names and plain names (PROPOSED)

The plain name is chosen by what the technique is. Each module's header file, function prefix and macro prefix change together: `sh4der_<old>.h`, `sh4der_<old>_*` and `SH4DER_<OLD>_*` become `sh4der_<new>.h`, `sh4der_<new>_*` and `SH4DER_<NEW>_*`. Struct and type names follow the function prefix (`sh4der_<old>_*_t` becomes `sh4der_<new>_*_t`). Field names do not change.

| Old name (header, functions, macros) | Proposed plain name | Migration line | Need |
|---|---|---|---|
| `sh4der_blade.h`, `sh4der_blade_`, `SH4DER_BLADE_` | `sh4der_tubeglow.h`, `sh4der_tubeglow_` | Replace `blade` with `tubeglow` in the include, calls and macros | High: "blade" names a use |
| `sh4der_holo.h`, `sh4der_holo_`, `SH4DER_HOLO_` | `sh4der_innerlight.h`, `sh4der_innerlight_` | Replace `holo` with `innerlight` | High |
| `sh4der_bloom.h`, `sh4der_bloom_`, `SH4DER_BLOOM_` | `sh4der_meshbloom.h`, `sh4der_meshbloom_` | Replace `bloom` with `meshbloom` | High: it is not screen bloom (see risks) |
| `sh4der_water.h`, `sh4der_water_`, `SH4DER_WATER_` | `sh4der_waves.h`, `sh4der_waves_` | Replace `water` with `waves` | Low: "water" is close already |
| `sh4der_look.h`, `sh4der_look_`, `SH4DER_LOOK_` | `sh4der_grade.h`, `sh4der_grade_` | Replace `look` with `grade` | High |
| `sh4der_frame.h`, `sh4der_frame_`, `SH4DER_FRAME_` | `sh4der_readback.h`, `sh4der_readback_` | Replace `frame` with `readback` | High: "frame" is used for many things |
| `sh4der_taa.h` | no change | — | None |
| `sh4der_glow.h`, `sh4der_glow_`, `SH4DER_GLOW_` | `sh4der_screenbloom.h`, `sh4der_screenbloom_` | Replace `glow` with `screenbloom` | Medium |
| `sh4der_warp.h`, `sh4der_warp_`, `SH4DER_WARP_` | `sh4der_screenwarp.h`, `sh4der_screenwarp_` | Replace `warp` with `screenwarp` | Medium: tells it apart from uvwarp |
| `sh4der_trail.h`, `sh4der_trail_`, `SH4DER_TRAIL_` | `sh4der_feedback.h`, `sh4der_feedback_` | Replace `trail` with `feedback` | Medium |
| `sh4der_lens.h`, `sh4der_lens_`, `SH4DER_LENS_` | `sh4der_overlay.h`, `sh4der_overlay_` | Replace `lens` with `overlay` | Medium |
| `sh4der_focus.h`, `sh4der_focus_`, `SH4DER_FOCUS_` | `sh4der_maskblur.h`, `sh4der_maskblur_` | Replace `focus` with `maskblur` | Medium |
| `sh4der_visor.h`, `sh4der_visor_`, `SH4DER_VISOR_` | `sh4der_screenfilter.h`, `sh4der_screenfilter_` | Replace `visor` with `screenfilter` | High |
| `sh4der_torch.h`, `sh4der_torch_`, `SH4DER_TORCH_` | `sh4der_screenspot.h`, `sh4der_screenspot_` | Replace `torch` with `screenspot` | High |
| `sh4der_pvrx.h` | no change | — | None: "PVR extras" |
| `sh4der_guard.h` | no change | — | None |
| `sh4der_bump.h`, `sh4der_bump_`, `SH4DER_BUMP_` | `sh4der_normalmap.h`, `sh4der_normalmap_` | Replace `bump` with `normalmap` | High: "bump" suggests the PVR's bump unit, which this does not use |
| `sh4der_sss.h`, `sh4der_sss_`, `SH4DER_SSS_` | `sh4der_translucency.h`, `sh4der_translucency_` | Replace `sss` with `translucency` | Medium: "sss" promises a scattering solve |
| `sh4der_uvwarp.h` | no change | — | None |
| `sh4der_spark.h`, `sh4der_spark_`, `SH4DER_SPARK_` | `sh4der_sprites.h`, `sh4der_sprites_` | Replace `spark` with `sprites` | Medium |
| `sh4der_bolt.h`, `sh4der_bolt_`, `SH4DER_BOLT_` | `sh4der_forklines.h`, `sh4der_forklines_` | Replace `bolt` with `forklines` | Low |
| `sh4der_motes.h`, `sh4der_motes_`, `SH4DER_MOTES_` | `sh4der_particlefield.h`, `sh4der_particlefield_` | Replace `motes` with `particlefield` | Medium |
| `sh4der_deform.h`, `sh4der_deform_`, `SH4DER_DEFORM_` | `sh4der_dentgrid.h`, `sh4der_dentgrid_` | Replace `deform` with `dentgrid` | Medium: "deform" also means mesh deformation |
| `sh4der_mist.h`, `sh4der_mist_`, `SH4DER_MIST_` | `sh4der_fogslices.h`, `sh4der_fogslices_` | Replace `mist` with `fogslices` | Low |
| `sh4der_rise.h`, `sh4der_rise_`, `SH4DER_RISE_` | `sh4der_risinglights.h`, `sh4der_risinglights_` | Replace `rise` with `risinglights` | Medium |
| `sh4der_film.h`, `sh4der_film_`, `SH4DER_FILM_` | `sh4der_filmwear.h`, `sh4der_filmwear_` | Replace `film` with `filmwear` | Low |
| `sh4der_cloudvolume.h` | no change | — | None |
| `sh4der_pl4nar.h`, `sh4der_pl4nar_`, `SH4DER_PL4NAR_` | `sh4der_planar.h`, `sh4der_planar_` | Replace `pl4nar` with `planar` | Medium: the spelling hides the word from a search |
| `sh4der_pool.h`, `sh4der_pool_`, `SH4DER_POOL_` | `sh4der_fandecal.h`, `sh4der_fandecal_` | Replace `pool` with `fandecal` | Medium: "pool" is also the spark pool |
| `sh4der_psm.h`, `sh4der_psm_raster.h`, `sh4der_psm_pvr.h` | no change proposed | — | None: PSM is the technique's published name (see risks) |
| `sh4der_ao.h` | no change | — | None |
| `sh4der_ca.h`, `sh4der_ca_`, `SH4DER_CA_` | `sh4der_chromatic.h`, `sh4der_chromatic_` | Replace `ca` with `chromatic` | Low |
| `sh4der_shafts.h`, `sh4der_shafts_`, `SH4DER_SHAFTS_` | `sh4der_shadowrays.h`, `sh4der_shadowrays_` | Replace `shafts` with `shadowrays` | Low |
| `sh4der_core.h` | no change | — | None |

Each migration line ends the same way: **the old name still works until a version the owner names; none is named.**

## 3. How the old names keep working

Owner's rule: nothing breaks for anyone using the old names.

- **The new header holds the code.** The old header becomes `#include "sh4der_<new>.h"`, the aliases, and a one-line note: `Deprecated name: use sh4der_<new>.h. Kept until the owner says.`
- **Macros:** each old macro is defined as the new one (`SH4DER_BLADE_TURNS` as `SH4DER_TUBEGLOW_TURNS`).
- **Types:** each old type is a typedef of the new one.
- **Inline functions:** each old name is a `static inline` wrapper that calls the new one, so it type-checks and its address can still be taken.
- **Functions in a .c:** each old symbol stays in the library as an alias of the new one (GCC `__attribute__((alias))`), so an object file built against the old header still links.
- **The one macro a client can override:** `SH4DER_PSM_RASTER_STEP` (`sh4der_psm_raster.h:77`). It is not renamed. If a later rename touches an overridable macro, the new header must honour the old name when a client defines it.
- **README's tuning tables** move to the new macro names and keep the old name in a note. `tools/check_tuning.py`, once written, must accept both.

## 4. The rule for old names (for the wiki and AGENTS.md)

1. **Old names are never removed without the owner's word.** No removal date is set until the owner sets one.
2. **One test per old name.** A compatibility test (proposed: `test/compat_test.c`, run by `make test`) includes each old header, calls each old function and uses each old macro and type, and checks that the result equals the new name's. A rename that breaks an old name fails the suite.
3. A new module gets a technique name from the start, by the rule on this page.

## 5. Proposed README text: a "What it is" column

README's effect table (README:92) gets a column after "Effect": **What it is**. Its text for each row is the "What it actually is" column of section 1, cut at the first colon where it runs long. Examples:

| Effect | What it is |
|---|---|
| blade | Animated additive glow: orbiting light tubes masked by a sliding ball |
| holo | A soft light inside a translucent body, following its breathing |
| bloom | Bloom made of geometry: a feathered ribbon and disc |
| look | Auto exposure and colour grade as full-screen layers |
| visor | Preset screen filters: tint, grain, scanlines, false colour |
| torch | A spotlight carved in screen space |

The README table today lists 26 modules. It has no rows for bolt, motes, deform, mist, rise, film, cloudvolume, psm_raster or psm_pvr. They should be added in the same edit.

## 6. Header first lines

The proposed first sentence of each header's top comment, technique first, use after:

- blade: Animated additive glow on a material group: tubes of light orbit an axis and a soft ball masks them as it passes (for example a glowing blade).
- holo: A soft light inside a translucent body that brightens with the pose's breathing (for example a hologram's chest).
- bloom: Bloom made of geometry: feathered camera-facing polygons round a point or along a line (for example round a blade or a lamp).
- water: A wave surface from directional sine waves, with normals, Fresnel, a reflected direction and crest transmission.
- look: Auto exposure and colour grade after the frame is drawn, as full-screen layers and the fog table.
- frame: Frame readback: the picture on screen read on a grid, and a stamp that says which frame was read.
- taa: Temporal anti-aliasing maths: jitter, history weight and pan shift.
- glow: Screen bloom and god rays from a small bright map of the frame.
- warp: Screen distortion: the frame as a texture through a moving mesh (rings, haze, wobble, radial blur, pixelate).
- trail: Frame feedback: last frame's picture blended over this one through the front buffer (for example motion blur).
- lens: Screen overlays with no frame read: vignette, grain, direction wedges, flash.
- focus: Region blur: a blurred copy of the frame through a mask that keeps one region sharp (a fake depth of field; no depth is read).
- visor: Preset screen filters as number tables: tint, grain, scanlines, vignette, a false-colour ramp, a fade (for example night or thermal vision).
- torch: A spotlight carved in screen space: darken outside a circle, boost the core, fog to black for reach (for example a flashlight).
- pvrx: Extras over KallistiOS's PVR driver: polled waits, VRAM download by DMA, TA error counters, user clip.
- guard: Checks before a send: no zero-area triangle, no header that was never compiled.
- bump: Normal mapping by blend passes, with no bump unit.
- sss: Back-lit transmission per vertex: light through a thin part toward the eye (a stand-in for subsurface scattering).
- uvwarp: Per-vertex texture coordinate offset from a small scrolling table.
- spark: A fixed pool of short-lived additive camera-facing quads: billboards and streaks (for example sparks and tracers).
- bolt: Branching jagged strands that partly re-draw a few times a second (for example lightning).
- motes: A stateless particle field: each speck from a hash of slot and time (for example dust or embers).
- deform: A dent grid under a ground mesh, stamped by contacts, with a rim and refill (for example snow or mud).
- mist: A fog volume as stacked translucent slices with drifting noise.
- rise: Stateless rising lights that rest, lift, drift, cool and settle as stars (for example lanterns).
- film: Film wear over a finished picture: flicker, scratches, dust, weave.
- timebase: A video tape's bent picture: a sideways shift for every row.
- hexheat: Heat haze as a lattice of hexagons fixed on the screen, each moved a little by a smooth rising wave.
- cloudvolume: Volumetric clouds by a CPU ray march into a small buffer drawn as one quad.
- pl4nar: Planar reflection through a small render target, looked up per vertex.
- softfade: A per-vertex fade of a light near the planes and walls the caller names (for example a halo that dies softly at a wall).
- pool: Fading fan decals on a plane: a disc or a wedge (for example a lamp's pool or a blob shadow).
- psm: Shadow map fitting for a directional light: uniform or perspective (PSM).
- psm_raster: A shadow map rendered on the SH-4 into RAM, compared per vertex.
- psm_pvr: The SH-4's half of a shadow map the PVR draws.
- ao: Small-scale ambient occlusion: crease darkening for a bake, and a screen cavity cue.
- ca: The frame texture, and chromatic aberration through it.
- shafts: Screen-space dark rays: the frame's dark shapes cast away from a light.

## 7. Risks and ambiguous names

- **v1.00 modules are in a released game** (README:120): blade, holo, bloom, look and core. Their names are the riskiest to move. The aliases must be in place, and tested, before the new names are documented.
- **bloom against glow.** Today `bloom` is geometry and `glow` is the screen effect. In the plain names both have "bloom" in them (`meshbloom`, `screenbloom`), which is clear when new but could confuse a reader of old code. Keep both qualifiers; never shorten either to `bloom`.
- **Modules that call each other.** visor and torch draw through look's layers (`sh4der_look_solve`); water wraps sss; bolt draws through `sh4der_spark_quad`; blade and bloom use the core's tubes. Rename in dependency order: look, sss and spark first, then the modules that call them, with the aliases of each in place.
- **sss.** `transmit` would collide with `sh4der_water_transmit`, so the proposal is `translucency`.
- **psm.** Three headers share the `sh4der_psm_` prefix, the town demo calls them throughout, and dozens of ledger rows name them. PSM is also the published technique name. I propose no rename.
- **pl4nar.** The ledger, the demo folder and the make target use the name. Rename the API only; the folder and the target stay.
- **frame.** `frame` is also a struct name in other modules (`sh4der_blade_frame_t`). Only the module prefix changes, not the word in other modules' types.
- **focus.** "Depth of field" would promise a depth read it does not have; hence `maskblur`.
- **spark.** `sprites` keeps the function names readable (`sh4der_sprites_billboard`, `sh4der_sprites_streak`, `sh4der_sprites_quad`). `pool` was avoided because it is another module's old name.
- **Effect pages in this wiki** (`wiki/effects/<old>.md`) keep their file names, so links do not break. Each page gets the plain name in its title.

## Related

- [Effects index](README.md)
- [Header RECOMMENDED block](../limits/header-recommended.md)
- [Maturity](../status/maturity.md)
