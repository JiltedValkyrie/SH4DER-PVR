---
title: Depth fog and glow ideas
tags: [research, pvr, fog, depth]
updated: 2026-11-25
sources: [wiki/hardware/ledger-pvr.md, demo/psm/PVR_FEATURES.md, demo/psm/compare/NOTES.md, public sources listed under Sources]
---
# Depth fog and glow ideas

**What you will learn:** which film and TV fog and light effects depend on depth, what the PVR's own table fog already gives, and whether fog could fill the space between c4's depth bands more cheaply than the shadow mask.
**For:** the owner, and anyone planning atmosphere for a Dreamcast scene. Research, not a recipe.

**Status: research ideas only.** Nothing here has been built or run. Every PVR mapping is **ESTIMATE** or **OPEN**. Costs are built from MEASURED ledger rows; no idea is measured itself.

## What the PVR already does: table fog

Before building anything, know what is free.

- The PVR fogs **each pixel by its own depth** (1/w). It looks up a 128-entry table, 16 entries an octave, far end first, and blends toward one fog colour (PVR-049; `PVR_FEATURES.md:54`; Sega §3.4.8.1).
- Any curve fits the table: linear, exponential, exponential-squared, or a hand-drawn one. The SH-4 pays only when the table changes.
- Fog is switched per polygon.
- **What it cannot do:**
  - one fog colour only, so no per-channel fade rates;
  - no height term: it is by depth from the camera only;
  - no shadowing of the fog: the air is uniformly lit;
  - it cannot remove colour (desaturate), only blend toward its colour.
- Our reading of the top fog code was one step low (errata K5). Keep a RAM copy of the table; it cannot be read back (errata E10).

This library already bends it: `sh4der_look.h` adds glare to the table, and `sh4der_torch.h` bends it to black for a torch's reach.

## Film and TV effects that use depth

| # | Effect | What it looks like | Cause | How it depends on depth | Cheapest PVR route | Already in the library | Status |
|---|---|---|---|---|---|---|---|
| F1 | Aerial perspective | Far things paler and lower in contrast, pulled toward the sky colour | Light scattered by air and haze adds a veil between eye and object | Roughly exponential in distance | **Table fog**, curve 1 − exp(−k·d), fog colour = horizon colour | fog table is the PVR's | ESTIMATE: free |
| F2 | Depth-graded colour | Blue distance, warm foreground | Aerial perspective, used as a grade | Monotonic in depth | Table fog toward a cool colour, plus a warm grade layer for the near scene | `sh4der_look.h` (grade layers) | ESTIMATE |
| F3 | Light shafts (god rays) | Beams through gaps in cloud, trees or a door | Lit air scatters toward the eye; shadowed air does not | Not a distance fade: a shadowed volume | Radial copies of a bright mask about the sun's screen point | `sh4der_glow.h` (god rays as scaled copies), `sh4der_shafts.h` (dark version) | ESTIMATE; console cost OPEN |
| F4 | Volumetric spotlight in haze | A visible cone of light in the air | Haze droplets scatter the beam toward the eye | Integrated along the view ray inside the beam | An additive cone mesh with vertex alpha falling off across and along it, plus a soft fade where it meets geometry (depth-shading idea D1) | `sh4der_pool.h` (the beam's pool on the ground) | ESTIMATE |
| F5 | Haze glow round lamps | A soft halo round a lamp in fog or rain | Strong forward scattering by droplets | Mostly by angle from the lamp; grows with fog density | Camera-facing halo quads, brighter with the fog table's density at that depth | `sh4der_bloom.h` (ring glows), `sh4der_spark.h` (halo quads) | ESTIMATE |
| F6 | Underwater absorption | Red drains first, the far scene goes blue-green | Per-wavelength absorption and scattering (Beer–Lambert) | Exponential, a different rate per channel | Table fog toward blue-green for the distance; a per-vertex SH-4 tint by path length for the per-channel loss | `sh4der_sss.h` (per-channel absorption by path, per vertex) | ESTIMATE |
| F7 | Dust in a beam | Specks glitter only inside the beam | Particles scatter the beam toward the eye | Only inside the lit beam | Specks drawn only where the SH-4 finds them inside the cone | `sh4der_motes.h` (stateless specks) | ESTIMATE |
| F8 | Bloom by depth | Bright sources bleed, more when out of focus | Lens spread and diffusion; out-of-focus blur grows away from the focal plane | Our inference only; no source found | Glow, with its strength from a depth window (depth-shading idea D5) | `sh4der_glow.h`, `sh4der_focus.h` | OPEN (unsourced) |
| F9 | Height fog, ground mist | Fog pools in low ground, clear above, flat top | Haze density falls with height | Density about exp(−h/H) | Per-vertex fog on the SH-4 by height, or mist sheets; the table has no height term | `sh4der_mist.h` (fog with a shape) | ESTIMATE |
| F10 | Smoke layers indoors | Stratified haze lit by windows | A density field, shown only when backlit | Not a distance fade | Mist sheets, lit only on the window side | `sh4der_mist.h` | ESTIMATE |
| F11 | Rain curtains by distance | Far rain is a grey veil, near rain is streaks | Many small drops blur into a veil | Our inference only | Streak quads that fade with distance, plus table fog | `sh4der_spark.h` (streaks) | OPEN (unsourced) |
| F12 | Heat haze | Shimmer over hot ground | Refraction by hot air, not scattering | By heat more than depth | The frame through a wobbling mesh | `sh4der_warp.h` (heat haze column) | ESTIMATE |

Cinematography advice worth keeping (ASC, "Shot Craft: Smoke and Fog"):

- Beams show only when backlit, aimed toward the lens but not into it.
- Too much haze is a grey blob.

In PVR terms: a shaft or a lit haze layer only pays off when the light is in front of the camera.

## The owner's two questions

### (a) Could the space between c4's depth bands be filled with fog instead of a shadow mask?

First, what the bands are. c4's bands lie **on the receiving surfaces** (ground and walls), cut along lines of equal depth **from the light**. They are not slices of air. So "fog between the bands" can mean two things.

**(a1) Fog on the surfaces, steered by the mask. ESTIMATE: yes.**

- Keep c4's mask (lit or shadowed, one bit a pixel, PVR-028).
- In the screen layer, add a haze colour where the bit says lit, instead of black where it says shadowed.
- Sunlit ground glows with haze; shadowed ground stays clear.
- It is the same layer c4 already draws, with another colour and blend.
- It colours the surfaces, not the air between them and the camera. So it gives "sunny haze", not visible shafts.

**(a2) Fog in the air, as light shafts. ESTIMATE: possible in principle; OPEN.**

- Draw K camera-facing slices through the view volume, as `sh4der_mist.h` draws sheets.
- Use each slice as a c4 receiver: cut it into bands of equal light depth, and bump-compare it against the light's map.
- Lit pixels of a slice add a little haze; shadowed pixels add nothing.
- Summed over the slices, the air is lit where the sun reaches it and dark in the shadow volumes. That is a sliced version of volumetric shadowed fog.
- Risks:
  - The slices face the camera, but their depth **from the light** varies across them, so each slice needs bands, and vertices with them (c4 needed 1,288-2,192 for its receivers, PVR-090, PVR-091).
  - The bump unit's 256-code cycle limits the depth range.
  - Too few slices show as steps.

**Test for both:** a c4 variant.

- (a1): the screen layer's colour and blend switched to additive haze.
- (a2): K = 4 and K = 8 slices added as receivers in the mask pass.
- Photograph the picture, and read each pass's time with `pvr_get_stats`.

### (b) Would that fill be cheaper than the mask pass?

**Measured numbers to build on** (one console, 2026-11-25):

| What | Time | Ledger |
|---|---|---|
| c4 mask pass, bands 2 wide, 1,288 vertices | 6,137 us | PVR-090 |
| c4 mask pass, bands 1 wide, 2,192 vertices | 10,171 us | PVR-091 |
| Mask pass, whole tile list vs cut to its 80 tiles | 6,130 vs 3,760 us | PVR-092 |
| Map pass, whole list vs cut to its 20 tiles | 3,312 vs 375 us | PVR-092 |
| Full-screen translucent layer, 640 × 480 | about 3,112 us | PVR-002 |
| Full-screen translucent layer, 320 × 240 target | about 771 us, on a floor of about 3.2 ms a pass | PVR-004 |

**Answers (ESTIMATE):**

- **(a1) does not replace the mask, it reuses it.** It costs no more than c4's own screen layer: a recolour, not a new pass. Fog this way is "free" only because the mask is already paid for.
- **(a2) is not cheaper than the mask; it adds to it.**
  - At 320 × 240 each slice is at least a translucent layer, about 0.77 ms (PVR-004). Eight slices add about 6.2 ms of fill.
  - The slices also add band vertices.
  - That puts eight slices at about the cost of one more mask pass (6.1-10.2 ms, PVR-090, PVR-091).
  - Cutting the tile list (PVR-092, PVR-093) lowers the floor, not the per-slice fill.
- **If shafts are the goal and shadow accuracy is not**, the radial-copy method (F3, already in `sh4der_glow.h`) needs no mask and no depth. It should be tried and timed before (a2).

**Tests that would settle (b):**

1. Time `sh4der_glow.h`'s god rays on a console. That is the baseline.
2. Run the c4 (a2) variant with K = 4 and 8.
3. Compare the PVR render times by `pvr_get_stats`, at the same view as PVR-090.

## Sources

Read for the facts; explained in our own words; nothing copied. F = fetched and read; S = seen in search only.

- "Aerial perspective", Wikipedia, en.wikipedia.org/wiki/Aerial_perspective (F).
- Sean O'Neil, "Accurate Atmospheric Scattering", *GPU Gems 2*, ch. 16, NVIDIA, 2005 (F).
- Eric Bruneton, Fabrice Neyret, "Precomputed Atmospheric Scattering", *Computer Graphics Forum* 27(4), 2008 (S).
- Kenny Mitchell, "Volumetric Light Scattering as a Post-Process", *GPU Gems 3*, ch. 13, NVIDIA, 2007 (F). The radial method: it fails when the light is off screen, and can show far shafts in front of near objects.
- "Theatrical fog and smoke", Wikipedia, en.wikipedia.org/wiki/Theatrical_fog (F).
- American Society of Cinematographers, "Shot Craft: Smoke and Fog", theasc.com/article/shot-craft-smoke-and-fog/ (F).
- Tobias Jendersie, Eugene d'Eon, "An Approximate Mie Scattering Function for Fog and Cloud Rendering", SIGGRAPH 2023 Talks, NVIDIA Research (F).
- "Mie scattering", "Tyndall effect", "Ocean optics", "Bloom (shader effect)", "Mirage": Wikipedia (F).
- Monzon et al., "Real-Time Underwater Spectral Rendering", 2024, graphics.unizar.es (S).
- Naty Hoffman, Arcot J. Preetham, "Real-Time Light-Atmosphere Interactions for Outdoor Scenes", in *Graphics Programming Methods*, Charles River Media, 2003, pp. 337-352 (S).

Two widely cited sources on volumetric fog name a commercial engine or studio. Under the wiki's rule against engine names they are not listed here; lead has them.

## Related

- [Depth shading ideas](depth-shading-ideas.md)
- [Bump compare chain](../shadows/bump-compare-chain.md)
- [PVR ledger](../hardware/ledger-pvr.md)
- [Open questions](../open-questions.md), T1 and T3
