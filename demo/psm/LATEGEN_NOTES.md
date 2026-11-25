# demo/psm: the late sixth-generation look, research notes

Research for the town square demo (`TOWN_STORY.md`, `TOWN_MOCKUP.md`). The goal is a look like a late-life Dreamcast, PS2 or GameCube game, not a launch title. No code here.

**How to read this.**

- **Documented**: a primary or first-hand source says it.
- **Reported**: secondary coverage or a search snippet says it; the page was not read in full.
- **Inferred**: my own reasoning from the hardware and the sources, not stated anywhere.

Every claim names its source. Nothing in this file was measured on a Dreamcast.

## The one-page list: biggest look gain for the lowest PVR cost

Our budget: 30 fps, 8,000 to 12,000 screen triangles. Route A draws the shadow as a multiply layer over the ground on the translucent list, so it already pays for one large translucent layer. Route B carries the shadow in the vertex colour and adds no translucent fill. **Clash** marks a technique that adds translucent fill on top of route A's layer.

| # | Technique | Look gain | PVR cost | Clash? | In this library |
|---|---|---|---|---|---|
| 1 | **Per-vertex light baked at build:** sky light, ambient occlusion (AO) in corners and under eaves, bounce from the cobbles. The sun term and the shadow are added per vertex at run time. | High: the single biggest step from "flat colours" to "lit place". | 0. Gouraud colour is free. | No | `sh4der_ao.h` (crease bake); route B writes the shadow into the same colour |
| 2 | **Table fog as haze**, with its colour following the clock (warm toward the sun, cool away). | High: depth and air. The brief asks for light haze toward the 30 m wall ring. | 0 per pixel. A 129-entry table and a colour. | No (see the notes on the fog) | the look's fog glare writes the table |
| 3 | **Grade in the vertex colours and the fog colour,** not in full-screen layers: time-of-day tint, cool lifted shade, warm sun. | High | 0. The SH-4 already writes every vertex colour. | No. Full-screen grade layers would clash. | `sh4der_look.h` maths, applied on the CPU instead of as 0 to 6 full-screen quads |
| 4 | **Low-contrast detail textures,** modulated by the vertex colour: cobble and plaster grain, VQ for large tiles, mipmaps on the ground. The palette stays flat; the texture only adds grain. | High: kills the "untextured prototype" read. | About 0 fill. VRAM: a few VQ textures (estimate 20 to 40 KB each). | No | none; build tool work |
| 5 | **Sky gradient dome, a sun disc with glow rings, and a skyline of flat cards** past the wall ring: roofs, the church, hills. | High: the town goes on past the square. | About 100 to 300 triangles, opaque or punch-through. The sun's rings are small and additive. | Small | `sh4der_bloom_rings` for the sun |
| 6 | **Punch-through cut-outs** for fine silhouettes: ivy on the hall, a tree's leaves, a ragged sign edge, straw. | Medium to high: detail with no triangles. | Inferred to cost about as much as opaque (no blend). | No | none; art and build tool work |
| 7 | **Specks in the light:** dust motes in the sun, one chimney's smoke. | Medium: makes the light visible as air. | Motes: at most 96 1-bit sprites on the punch-through list. Smoke: a few translucent quads. | Small (smoke only) | `sh4der_motes`, `sh4der_spark` |
| 8 | **Contact darkening** under feet, barrels and the well: a small soft blob of AO. It is not a second shadow. | Medium: people sit on the ground where the map's texels are coarse. | A small fan each, translucent. | Small | `sh4der_pool` blob |
| 9 | **Dither on the 16-bit output.** | Medium: hides banding in the sky gradient and in the cool lifted shade. | 0. One register bit. | No | KOS init setting |
| 10 | **One glow pass** from a small bright map, kept to the sky and sun side. | Medium: the soft late-PS2 halo. | A full-screen additive layer. | **Yes** | `sh4der_glow` |

**Not recommended here:**

- **Frame-feedback motion blur** (`sh4der_trail`): two full-screen passes a copy (**clash**). It blurs the shadow edges, which are the subject. Gain is low at walking speed.
- **Full-screen grade or vignette layers** (`sh4der_look` layers, `sh4der_lens` vignette): **clash**. Put the grade in the vertex colours (item 3).
- **Mist slices** (`sh4der_mist`): their fill is the cost (**clash**); the table fog gives the haze for free.

The order above is my judgement (inferred) from the sources below. The PVR costs are qualitative. Before any item ships, measure it with `pvr_get_stats`, with the item on and off, on route A.

## What is documented, by subject

### The hardware (Dreamcast PowerVR2 / CLX2)

- **Texture formats.** ARGB1555, RGB565, ARGB4444, YUV422, BUMP, and 4bpp and 8bpp palettes; twiddled or strided; VQ; mipmaps (KOS `pvr.h` docs). **Documented.**
- **VQ.** "The Dreamcast graphics chip has special hardware to decode it, so you suffer no performance loss" (dreamcast.wiki). **Documented.**
- **Mipmaps** make "textures 1/3 bigger, texture filtering cheaper and improves rendering performance" (dreamcast.wiki). **Documented.**
- **Palettes.** 1,024 entries in all, global, not per texture (KOS `pvr_pal.h`; dreamcast.wiki). Filtering paletted textures with ARGB8888 entries cuts speed "about in half" (KOS `pvr_pal.h`, paraphrased tail). **Documented.**
- **Texture memory in games.** Typical games used 20 to 30 MB of texture data compressed to about 5 to 6 MB (Sega Retro, hardware comparison). **Reported** (snippet).
- **Fog.** Per polygon: table fog, vertex fog or none. The table has 129 entries, from far to near, 0 to 1; there are exp, exp2 and linear generators and a table fog colour (KOS `pvr_fog.h`). **Documented.**
- **Lists.** Opaque, opaque modifier, translucent, translucent modifier, punch-through (KOS `pvr.h` primitives). **Documented.**
- **Translucent cost.** The order-independent sort "slows down the rendering process" (Copetti), with no numbers. **Documented, qualitative.** Opaque overdraw does not cost texture fill: "the texture fill rate is not affected by overpainting at all" (Comstedt). **Documented.**
- **Dither.** The render-output register has "D - Dither enable ... enables dithering in highcolour modes" (Comstedt). **Documented.**
- **Modifier volumes.** Used for "light beams, shadows, lasers, glowing suns" (Sega Retro, technical specifications). **Reported** (snippet).
  - Toy Commander "made very interesting use of shadows" with them (Imagination blog). **Reported.**
  - Speed Devils 2 used them to project headlights, and Jet Set Radio's translucent characters receive them (libretro / Flycast notes). **Reported.**
  - What the cheap shadow mode does is not documented in anything read (a 1999 Sega conference PDF lists it; the file was too large to fetch).
- **Throughput.** Most games ran 2 to 4 million polygons a second, with about 50,000 polygons a scene on average (Sega Retro, citing its sources). **Reported.** A forum figure of "up to 60k polygons per frame at 30 fps" is unsourced; do not use it.
- **Model counts.** A Sonic Adventure character is 1,001 triangles (Copetti). **Documented.**

### Dreamcast titles

Little primary documentation exists for these. Rendering talks from Sega's and Capcom's Dreamcast teams were not found in English. What there is:

- **Shenmue II.** A day and night cycle, weather, and NPCs on daily schedules (Wikipedia). **Documented** (as features).
  - d3t, who ported it in 2018, said the hardest parts to reproduce were "the modifier volumes used for shadows and light volumes, and the ability to sort transparent geometry on a per pixel level" (PC Gamer, as quoted in summaries). **Reported.**
  - The moon is a 3D object lit by the sun, so it has phases (same coverage). **Reported.**
  - A summary of Digital Foundry's comparison says the Dreamcast version is darker, partly from 16-bit buffers. **Reported, weak** (the summary was cut off).
  - **Inferred:** the town look came from vertex-lit geometry, fog and modifier-volume shadows, not from post effects.
- **Resident Evil Code: Veronica.** The first fully polygonal game in the series, with real-time lighting and camera moves (Wikipedia). A review singles out "candle light that casts Claire's shadow dancing on the walls" (GameSpot). The technique itself is not documented.
- **Headhunter.** Reviews praise its lighting and real-time shadows (Dreamcast Junkyard; Planet DC). Engine and technique not documented.
- **Sonic Adventure 2.** Modders report that stage models carry baked vertex colours. The chunk format allows one vertex layout per mesh, so a mesh carries either normals or colours: lit at run time or prelit, not both (Sonic Retro model-format notes and forum posts). **Reported.**
- **Skies of Arcadia, Soulcalibur, Ikaruga.** No technical source found. Nothing is claimed for them here.

### PS2 and GameCube titles with first-hand write-ups

- **Shadow of the Colossus (PS2).** Developers interviewed in a Game Watch column (Nishikawa, 2005). **Documented**, where the developers are quoted or relayed:
  - The bright-area glow is "the same method as ICO". The far scene is masked by the near scene's depth, scaled down to 64 x 64 with bilinear filtering, blended with the previous frame's result, then scaled up and blended over the frame.
  - The strength is set per "scene box", a volume placed by hand in the world.
  - The afterglow when the camera swings off the sky was an accident they kept.
  - Shadows are stencil volumes on low proxies: the colossus's proxy is about 1/40 of its 18,000 polygons; Wanda is about 3,000.
  - Distant terrain is a "Super Low" backdrop.
  - The author (not the developers) describes the camera motion blur as the frame redrawn translucent and offset by the camera's turn. He gives the frame rate as 60 down to about 15 fps and calls the HDR "pseudo".
- **SWAT (PS2), Matt Godbolt's write-up.** **Documented.**
  - Static scenery lighting and shadows were precalculated.
  - Characters got a "black circle shadow".
  - Exposure came from "an exposure table".
  - A colour bleed effect thresholded bright pixels by subtractive blending, then scaled down and back up.
  - The screen was scaled down to about 32 x 32 for the CPU to read its brightness.
  - The PS2 does not interpolate colour with perspective, so they had to "minimise the colour change over larger polygons".
- **Jak and Daxter (PS2), postmortem.** **Documented:** level-of-detail schemes, "simplified models used to represent faraway backgrounds, and flats used to represent distant geometry", and streaming with no load screens. **Reported** (AWN): the lighting cycled through a 24-minute day.
- **Star Wars Rogue Leader (GameCube), Factor 5's article (Sauer, 2002).** **Documented.**
  - One directional light and one ambient light are "computationally for free".
  - Local lights are per vertex.
  - Vertex colour is "used as pure paint".
  - Layered fog by world height and eye distance comes from a ramp texture; it works only "as long as the camera is reasonably outside the fog volume".
  - Projected shadows and 8-bit self-shadow depth.
  - A 4 x 4 dither.
  - "A typical landscape view uses about 14k triangles."
- **GTA San Andreas (PS2).** Its lighting depends on baked vertex colours; without the prelight the scene renders flat (MTA modding guide). **Reported.** Modding pages also report a separate set of night vertex colours. **Reported, unverified:** the pages were not read in full.

## What follows for the town (inferred)

- **The look lives in the vertex colours.** Every first-hand source above puts most of its lighting per vertex: Rogue Leader, SWAT, GTA, Sonic Adventure 2. The PVR draws Gouraud colour for free, and route B already writes a shade per vertex. So bake sky and AO into the vertex colours at build, and add the sun term and the map's shadow at run time. The receiver rows route B needs (eave bands, fine ground near the camera) also carry the AO, so they pay twice.
- **Watch the colour across large polygons.** A big cobble quad can show none of this light. The ground's graded grid and the eave bands already give the vertices. SWAT's footnote on perspective is about the PS2's colour interpolation; whether the PVR has the same issue is not covered in anything read.
- **Fog is the cheapest "air" there is.** The table fog is per pixel and free. Two cautions:
  - On route A, the multiply shadow layer should be drawn with fog off or matched to it, or the haze darkens twice (inferred; to check on the console).
  - Rogue Leader's height fog needed a ramp texture, a second pass; the table fog gives distance only. Distance only is what the brief asks for.
- **Glow, late-PS2 style, is cheap only when small.** ICO, SotC and SWAT all made the glow from a small scaled-down map (64 x 64, 32 x 32), but they still blend a full-screen layer. On route A that is a second large translucent layer.
  - Keep it off on route A by default and on for route B, or confine it to the sky band and the sun.
  - Cost the glow against route A's layer with `pvr_get_stats`.
- **Texture grain over flat colour.** Late titles read "rich" mainly through texture density (inferred from the texture figures above). A grey detail texture modulated by the kit's flat colour keeps the designer's palette. Keep its contrast low (about ±8 %, a guess) so the shadow edges stay the highest contrast in frame (the brief). VQ for the large tiles and mipmaps on the ground: the hardware decodes VQ at no cost, and mipmaps cut ground shimmer at 640 x 480.
- **Our triangle budget is modest for the class.** The figures above are about 50,000 polygons a scene on the Dreamcast (reported) and 14,000 for a Rogue Leader landscape view (documented). The town's 8 to 12k is low because the SH-4 also draws route B's map (inferred: we are CPU-bound, not PVR-bound). Spend the PVR's spare on the zero-fill items (1 to 6), not on more full-screen layers.
- **What not to copy.**
  - Modifier-volume shadows: our subject is the shadow map. They would also sit beside route A's layer.
  - Stencil volumes: these are SotC's method and not on the PVR.
  - Frame-feedback blur in a walking sim: no gain, and it softens the subject.

## Sources

Read in full (by me or a fetch agent):

- Copetti, "Dreamcast Architecture": https://www.copetti.org/writings/consoles/dreamcast/
- dreamcast.wiki, Texture Formats: https://dreamcast.wiki/Texture_Formats
- KallistiOS docs: https://kos-docs.dreamcast.wiki/pvr_8h.html , https://kos-docs.dreamcast.wiki/pvr__fog_8h_source.html , https://kos-docs.dreamcast.wiki/pvr__pal_8h_source.html , https://kos-docs.dreamcast.wiki/group__pvr__primitives__headers.html , https://kos-docs.dreamcast.wiki/group__pvr__mod__modes.html
- Marcus Comstedt, PowerVR notes: http://mc.pp.se/dc/pvr.html
- Game Watch, Zenji Nishikawa, Shadow of the Colossus graphics (Japanese, 2005-12-07): https://game.watch.impress.co.jp/docs/20051207/3dwa.htm
- Matt Godbolt, "Rendering in SWAT: PlayStation 2": https://old-dog.xania.org/201003/swat-ps2-renderer
- Florian Sauer, "Shader Integration: Merging Shading Technologies on the Nintendo Gamecube": https://www.gamedeveloper.com/programming/shader-integration-merging-shading-technologies-on-the-nintendo-gamecube
- Stephen White, Jak and Daxter postmortem: https://www.gamedeveloper.com/design/postmortem-naughty-dog-s-i-jak-and-daxter-the-precursor-legacy-i-

From search snippets only (not read in full; treat as reported):

- Sega Retro, hardware comparison: https://www.segaretro.org/Sega_Dreamcast/Hardware_comparison and technical specifications: https://segaretro.org/Sega_Dreamcast/Technical_specifications
- Imagination Technologies, Dreamcast 20th anniversary: https://blog.imaginationtech.com/celebrating-the-20th-anniversary-of-dreamcast-and-powervr
- libretro / Flycast progress notes: https://libretro.com/index.php/category/dreamcast
- PC Gamer, d3t on Shenmue: https://www.pcgamer.com/how-sega-brought-shenmue-back-to-life-on-pc and a summary: https://phantomriverstone.com/2018/09/d3ts-toughest-project-interview-on.html
- Shenmue II on Wikipedia: https://wikipedia.com/wiki/Shenmue_2 ; DF comparison summary (Spanish): https://www.noticias3d.com/noticia/69959/gaming-retro-recordando-shenmue-ii.html
- Code: Veronica on Wikipedia: https://en.wikipedia.org/wiki/Resident_Evil_%E2%80%93_Code:_Veronica ; GameSpot review: https://gamespot.com/reviews/resident-evil-code-veronica-review/1900-2540169/
- Headhunter: https://www.thedreamcastjunkyard.co.uk/2017/01/a-quick-look-at-headhunter.html ; https://planetdc.segaretro.org/games/reviews/headhunter
- Sonic Adventure 2 model formats: https://info.sonicretro.org/SCHG:Sonic_Adventure_2/Model_Formats and Sonic Retro forum posts
- AWN, Naughty Dog animation: https://www.awn.com/print/animationworld/animating-games-naughty-dog-style
- MTA forum, vertex colour basics (San Andreas prelight): https://forum.mtasa.com/topic/121674-basics-of-vertex-color-and-alpha/

Not reachable: the 1999 Sega Dreamcast developers conference PDF on Sega Retro (over the fetch tool's 10 MB limit). It may document the cheap shadow mode and Sega's own recommendations; worth a local read.
