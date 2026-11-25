# demo/psm: prior art for shadow maps on Sega hardware

The question: did any Dreamcast release, late arcade port, NAOMI or Atomiswave game, or homebrew draw shadows from a real-time shadow map, a depth map, or a render-to-texture silhouette? And is there anything like it on the Saturn or other Sega hardware? Research only; nothing here was run or measured.

**How to read this.**

- **Documented**: a primary or first-hand source says it, read in full.
- **Reported**: secondary coverage, emulator authors' notes or a search snippet; the page was not read in full, or the claim is second-hand.
- **Inferred**: my own reasoning, not stated anywhere.

## Short answer

- **Depth shadow maps (a depth compare per pixel or vertex):** no commercial Dreamcast, NAOMI or Atomiswave release found. No homebrew found either.
- **Render-to-texture shadows** (the caster drawn into a texture, the texture laid on the ground): **one counterexample found**. Tony Hawk's Pro Skater 1 and 2 on the Dreamcast, as reported by the libretro emulator team.
- **The common Dreamcast method** was the modifier volume, a hardware shadow volume. That is not a shadow map.

Safe wording for the demo:

> "We found no documented commercial Dreamcast game that used a depth shadow map. Render-to-texture shadows are reported for Tony Hawk's Pro Skater 1 and 2; most games used modifier volumes."

Do not write that this demo is the first of anything.

## Findings

### Counterexample: render-to-texture shadows in Tony Hawk's Pro Skater 1 and 2 (Dreamcast)

- **Reported** (emulator authors, read in full). The libretro team, "Introducing BeetleDC OIT libretro core + updated BeetleDC regular core", 12 June 2018, under "Render to texture buffer":
  > "Not only has render to texture being reimplemented (leading to much faster performance), but certain games such as Tony Hawk's Pro Skater 1/2 would render to VRAM for rendering shadows. The upshot of this is that the shadow looks much more convincing vs. merely using volume modifiers in order to simulate shadows."
  https://www.libretro.com/index.php/introducing-reicast-oit-libretro-core-updated-reicast-regular-core/
- **What it was is not documented.** The post says the game renders to video memory for the shadow. It does not say how. **Inferred:** most likely the skater drawn flat into a texture and laid on the ground under the skater (a projected silhouette), with no depth compare. That is a render-to-texture shadow, not a depth shadow map, and it cannot shadow the skater himself. Unconfirmed.
- **Related, documented** (32bits, "Under the microscope: Tony Hawk's Pro Skater 2", 1 December 2024): THPS2 has a hidden "no shadows" code on the PlayStation, but "the effect doesn't work on Dreamcast ... your shadow persists". So the Dreamcast build's shadow path differs from the PlayStation's at least in that code path. It says nothing about the method. https://32bits.substack.com/p/under-the-microscope-tony-hawks-pro

### The BMX title (Mat Hoffman's Pro BMX, Dreamcast, 2001)

The lead's note says "Mike Hoffman"; the game is **Mat Hoffman's Pro BMX**.

- **Reported** (Wikipedia, snippet): it runs on a tweaked version of the Tony Hawk's Pro Skater engine. If so, its shadow may be the same render-to-texture path as THPS (**inferred**).
- **The owner's "pre-baked 512 x 512 shadow textures": no source found.** The 32bits article on this game (25 June 2025) covers cheat codes only; it has nothing on shadows or textures. https://32bits.substack.com/p/under-the-microscope-mat-hoffmans
- **Reported** (search snippet only; the forum page refused the fetch): in the DCEmulation thread "Shadows/Lights example?", a poster speculated the BMX game is "probably doing a separate render-to-texture and are just drawing the biker's untextured polygons onto a texture as the 'shadow map'". The same thread reportedly mentions a developer who tried a depth-buffer method, got artefacts, and switched to render-to-texture plus projection. https://dcemulation.org/phpBB/viewtopic.php?t=104324
- **To settle it (inferred method):** run the game in an emulator with a texture or frame debugger and look for a texture redrawn each frame that holds the rider's outline. A pre-baked texture would not change as the rider moves.

### Modifier volumes: the usual Dreamcast shadow

These are hardware shadow volumes; not shadow maps, listed for contrast.

- **Reported:** "The Dreamcast made use of volume modifiers in order to simulate shadows in many games" (libretro, same 2018 post).
- **Reported:**
  - Shenmue: d3t named "the modifier volumes used for shadows and light volumes" among the hardest parts of the PC port (PC Gamer, via summaries).
  - Toy Commander: "made very interesting use of shadows" (Imagination Technologies blog).
  - Speed Devils 2: projected headlights with them (libretro Flycast Vulkan post, 11 December 2019, read in full).
  - Jet Set Radio: its translucent characters receive them (same post).
- **Reported:** a 1999 Sega developer conference slide lists a "cheap shadow mode" and a shadow density among the render states (Sega Retro PDF; snippet only, the file was too large to fetch). What it does is not documented in anything read.

### NAOMI, NAOMI 2, Atomiswave

- **Nothing found** on shadow maps or render-to-texture shadows for any title.
- **Reported:** NAOMI 2's geometry chip (Elan) gave up to 16 hardware lights a polygon (Sega Retro, Virtua Fighter 4; snippet). Its shadow method is not documented in anything read.
- **Inferred:** NAOMI uses the same PowerVR2 family as the Dreamcast, so the same two routes were open: modifier volumes, and render-to-texture.

### Homebrew (KallistiOS and others)

- **Nothing found:** no shadow-map or depth-map shadow demo on GitHub or in the search results.
- **Reported** (search snippets; the forum pages refused the fetch):
  - KallistiOS ships a render-to-texture example (`examples/dreamcast/pvr/texture_render`).
  - A forum thread on its limits: one shared set of render-to-texture state, so two texture passes in a row needed a screen pass between them. A maintainer reported duplicating that state.
- **Documented:** current KallistiOS has `pvr_scene_begin_txr` and `pvr_scene_begin_rtt` (kos-docs `pvr.h`, read in full), so the render-to-texture route is open in homebrew.
- **This library's own route B** (the SH-4 draws a 16-bit depth map in RAM and compares it per vertex) is the depth-map case. Nothing else was found that does it on a Dreamcast. That is a statement about this search, not about history.

### Saturn

- **Reported** (Sega's VDP1 manual via infochunk; Sega Retro; snippets):
  - VDP1 has shadow, half-brightness and half-transparent colour modes. "The sprite data of the MSB shadow is created by changing only the MSB to 1" of pixels already in the frame buffer.
  - Most games faked translucency, shadows included, with a dithered mesh that the TV blends.
- **Inferred:** these are flat shadow sprites or polygons; no depth map is involved. Nothing found suggests a Saturn game drew shadows from a map.

### Comparable hardware outside Sega, for context

- **Documented** (Florian Sauer, Factor 5, Star Wars Rogue Leader, GameCube, 2002, read in full):
  - Projected shadows: the caster "pre-rendered from the imaginary point of light using an orthogonal projection".
  - Per-object self-shadowing with "an eight-bit Z texture".
  - That is a depth-map shadow on a sixth-generation console.
  https://www.gamedeveloper.com/programming/shader-integration-merging-shading-technologies-on-the-nintendo-gamecube
- **Documented** (Game Watch, 2005, Shadow of the Colossus, PS2, developers quoted): stencil shadow volumes on low proxies, not shadow maps.
- **Documented** (Matt Godbolt, SWAT, PS2): no stencil or depth buffer support, so precalculated scenery shadows and "black circle" character shadows.

## What was searched

These found nothing beyond the findings above. Searches were in English only; Japanese sources were not searched for this question.

- Web searches:
  - Dreamcast game real-time shadow map / render-to-texture shadows.
  - Mat Hoffman's Pro BMX Dreamcast shadows.
  - Tony Hawk's Pro Skater 2 Dreamcast shadow and render to texture.
  - Spider-Man (Dreamcast, same engine) shadows.
  - NAOMI 2 / Virtua Fighter 4 shadowing.
  - NAOMI / Atomiswave real-time shadows.
  - KallistiOS homebrew shadow mapping (GitHub, GLdc).
  - DCEmulation forum shadow map threads.
  - Flycast render-to-texture game lists.
  - Saturn VDP1 shadows.
- Pages read in full:
  - The libretro BeetleDC and Flycast posts named above, and the libretro Flycast core docs. The docs carry an RTT buffer option with no description and name no shadow game.
  - The two 32bits articles.
  - kos-docs `pvr.h`.
  - Sauer's GameCube article.
- Not reachable:
  - DCEmulation forum pages (HTTP 403).
  - The 1999 Sega developer conference PDF (over 10 MB).
  - The Spider-Man and Dave Mirra Dreamcast builds: no source found on their shadows.

## Sources

- libretro, BeetleDC OIT post (2018-06-12): https://www.libretro.com/index.php/introducing-reicast-oit-libretro-core-updated-reicast-regular-core/
- libretro, Flycast Vulkan renderer post (2019-12-11): https://www.libretro.com/index.php/flycast-is-getting-a-vulkan-renderer-available-later-today-on-retroarch-with-nightly-core/
- libretro Flycast core docs: https://docs.libretro.com/library/flycast/
- 32bits, THPS2 (2024-12-01): https://32bits.substack.com/p/under-the-microscope-tony-hawks-pro
- 32bits, Mat Hoffman's Pro BMX and Dave Mirra (2025-06-25): https://32bits.substack.com/p/under-the-microscope-mat-hoffmans
- Mat Hoffman's Pro BMX, Wikipedia (snippet): https://en.wikipedia.org/wiki/Mat_Hoffman%27s_Pro_BMX
- DCEmulation, "Shadows/Lights example?" (snippet, 403): https://dcemulation.org/phpBB/viewtopic.php?t=104324
- DCEmulation, render-to-texture thread (snippet, 403): https://dcemulation.org/phpBB/viewtopic.php?p=1036610
- KallistiOS docs, `pvr.h`: https://kos-docs.dreamcast.wiki/pvr_8h.html
- Imagination Technologies, Dreamcast 20th anniversary (snippet): https://blog.imaginationtech.com/celebrating-the-20th-anniversary-of-dreamcast-and-powervr
- Sega Retro, Virtua Fighter 4 (snippet): https://segaretro.org/Virtua_Fighter_4
- Sega Retro, Dreamcast developers conference 1999 PDF (snippet, not fetched): https://www.segaretro.org/images/2/21/DreamcastDevelopersConverence_Day2_1999-03-20.pdf
- Saturn VDP1 manual (infochunk) and Sega Retro VDP1 (snippets): https://www.infochunk.com/saturn/segahtml_en/hard/vdp1/hon/p06_37.htm , https://segaretro.org/VDP1
- PC Gamer, d3t on Shenmue (snippet): https://www.pcgamer.com/how-sega-brought-shenmue-back-to-life-on-pc
- Florian Sauer, GameCube shading (read): https://www.gamedeveloper.com/programming/shader-integration-merging-shading-technologies-on-the-nintendo-gamecube
- Game Watch, Shadow of the Colossus (read): https://game.watch.impress.co.jp/docs/20051207/3dwa.htm
- Matt Godbolt, SWAT PS2 (read): https://old-dog.xania.org/201003/swat-ps2-renderer
