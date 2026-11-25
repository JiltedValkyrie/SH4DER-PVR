# demo/psm: look target for the town and the cow scene

The aim: the disc should read as a polished late-life sixth-generation title, not a launch title. Flat kit colours, hard edges and a plain gradient sky read as launch. This page says what makes the late look and what we can afford inside the town's 8 to 12k screen triangles at 30 fps, with route A's translucent shadow layer already on.

It is a target, not code. Costs come in four units: screen triangles, VRAM, TA vertices and **translucent fill in screens** (one full-screen quad = 1 screen). Fill numbers are estimates until measured on the console; the cost strip measures them.

## What reads as late-gen

1. **No flat surfaces.** Every wall, roof and stone has low-contrast detail and its own small variation. Nothing is one colour from edge to edge.
2. **Light has colour.** Warm sun, cool blue shade, warm bounce near lit ground, darker corners. Not just "lit" and "darker".
3. **Air.** Haze toward the horizon, a sky with clouds and a glow round the sun, and a world that ends in hills, not at a line.
4. **One grade over everything**, so the frame looks like one picture.
5. **Things move.** People, hens, cloth, smoke, birds, dust in the sun.

## Ranked: look gained for what it costs

| # | Item | What it is | Cost | Gain |
|---|---|---|---|---|
| 1 | Atmosphere | Fog table, sky band, hills ring, sun disc (below) | Fog: free. Sky + hills: about 350 tris, opaque, paid only where visible (0.1 to 0.6 screen). Sky texture 34 KB (VQ) or 256 KB (565). Sun: about 100 tris additive, under 0.05 screen | Very high: ends the hard horizon |
| 2 | Vertex light bake | AO, contact darkening, height gradient, sky and bounce terms baked into vertex colour | 0 at run time. About 300 to 600 extra tris for wall rows the gradients need (route B wants most of them for the eave bands anyway) | Very high: kills the flat look |
| 3 | Detail textures | Grey detail times the kit colour | 50 to 80 KB VQ for the whole set. Opaque: no extra fill | High |
| 4 | Grade (`sh4der_look`) | Warm lift, slight contrast, mean kept | 1 to 2 full-screen layers: 1 to 2 screens | High; a toggle |
| 5 | Cheap life | Pennant and cloth sway (CPU), birds, chimney smoke, dust motes | Under 400 tris. Smoke about 0.2 screen. Motes: punch-through, at most 96 sprites, 12 KB TA | High for little |
| 6 | Bloom and rays (`sh4der_glow`) | Halo on bright stucco and sky; god rays only with the sun in front (the head-on preset) | 16 KB staging RAM plus the frame read. About 1 screen additive; rays 2 to 3 screens, head-on only | High in head-on; a toggle |
| 7 | Vignette (`sh4der_lens`) | Edge darkening mesh | About 0.3 screen, vertex colour only | Medium; a toggle |
| 8 | Back-lit cloth (`sh4der_sss`) | Pennants and stall cloth glow with the sun behind them | CPU per vertex, 0 fill | Medium, head-on only |
| 9 | TAA (`sh4der_trail` with `sh4der_taa`) | Calms the shimmer on ropes, pennants and roof edges | 320 KB VRAM (320 x 240 frame texture), 2 layers | Medium; last, a toggle |

Left out: `sh4der_focus` (it blurs the far shadows the demo is about), `sh4der_shafts` (reads as a second shadow next to the real one), `sh4der_ca`, `sh4der_warp`, `sh4der_visor`, `sh4der_torch`, `sh4der_pl4nar` (no mirror surface in the square).

Translucent fill target at 30 fps: the shadow layer (about 0.5 screen) plus items 4, 6 and 7 come to about 3 to 4 screens in the head-on view and about 2.5 elsewhere. This is an estimate; if the cost strip says it is over, drop item 6's rays first, then go from 2 grade layers to 1.

## 1. Surfaces

The Modular Village kit is flat `.mtl` colours with no UVs. Keep its palette as vertex colour and add the rest at build time:

- **UVs.** World-space box projection per face (pick the axis by the face normal) at kit units, so every piece tiles to the same grid. A build-tool step.
- **Detail set, grey, modulated by vertex colour:** stucco (low mottling and fine noise, values 0.80 to 1.0), stone courses, timber grain, roof tile rows, cobbles. One 256 x 256 VQ texture each is 8 KB plus a 2 KB codebook; five or six come to 50 to 80 KB. Keep the detail low-contrast so it never beats a shadow edge (the approved rule: wall contrast stays under the ground edge's 2:1).
- **Vertex colour bake** (build tool; `sh4der_ao` crease term plus a short ray-cast occlusion like the sss thickness bake):
  - AO in corners, under eaves, under stall counters and round door and window insets.
  - Contact darkening: 0.65 at a wall's foot, fading to 1.0 over 0.5 m.
  - Height gradient: wall bottoms 5 % darker and a touch greener (damp), roofs lighter toward the ridge.
  - Grime streaks under window sills and at downpipes, as darker vertex rows. No translucent decals unless a test shows the fill is free.
- **Break repetition.** Each house gets its own stucco shade from a small family (cream, ochre, pale pink, lime white), plus or minus 4 % in value. Timber is the same few browns everywhere.
- **Ground.** Cobble detail tiled every 1.5 m. Vertex colour carries worn paths (lighter) between the doors, the well and the stalls, dirt round the stalls and darker gutters along the facades.
- **Windows.** The glass inset gets a vertex gradient from sky grey at the top to dark at the bottom, so it reads as glass, not as a hole.

## 2. Light

- **Already approved:** warm sun, shade = colour x 0.46 x (0.80, 0.88, 1.06), lifted, never black. Keep it.
- **Sun colour by time:** warmer and lower in value as the elevation drops (at el 12.9, about (1.0, 0.86, 0.66)). One constant a frame.
- **Sky light:** the shade term is the sky's colour. Faces that look up get a little more of it (normal y times 0.1).
- **Bounce:** the lower 1.5 m of a wall facing lit cobbles gets plus 0.10 of the ground colour times the sun term. Per vertex, so route B already has the loop.

## 3. Atmosphere (the horizon fix)

Pictures: `demo/psm/out/sky_options_first.png` and `sky_options_headon.png`. Each has 4 tiles: current, then options A, B and C. They are made with the mock renderer's own depth, at the Dreamcast texture size and 565 colour.

**Sky band.** A ring of 16 segments and 4 rows from el -4 to 40 degrees, with a 16-triangle cap (zenith colour) above: about 144 tris. One texture, 512 x 256 over 360 degrees, twiddled. VQ is 34 KB; check it for banding, and if it bands, use RGB565 at 256 KB. Mapping:
- The texture's u is turned each frame so the bright side of the clouds faces the sun's azimuth: clouds turn with the clock, which nobody reads as wrong.
- The bottom ring is warmed toward the sun by vertex colour: times (1, 0.95, 0.84), weighted by cos³ of the bearing to the sun.
- The bottom ring takes vertex fog: offset alpha 1 at el 0, fading to 0 by el 6. Check on the console that vertex fog works on a textured header.

| Option | Source | Why | Why not |
|---|---|---|---|
| **A (pick)** | Poly Haven `kloofendal_48d_partly_cloudy_puresky`. Greg Zaal (original), Jarod Guest (sky edits). The asset page states CC0 | Soft cumulus, blue top, pale horizon. The baked sun is at el 54, above the band, so no second sun shows | 512 across 360 degrees is soft, but cumulus survives it |
| B | Poly Haven `kloppenheim_05_puresky`. Greg Zaal, Jarod Guest. CC0 on the page | Calm, mostly clear, small clouds low. Least busy behind the pennants | Plainer; closer to "gradient" |
| C | Procedural (gradient plus fbm cloud rows, made by the build tool) | No fetch, no licence | Reads smeared; least late-gen |

Rejected: `sunflowers_puresky` (green, muddy cast after the tint).

For the fetch row (Junior adds the sha256): take the 2K `.hdr` (`https://dl.polyhaven.org/file/ph-assets/HDRIs/hdr/2k/kloofendal_48d_partly_cloudy_puresky_2k.hdr`). The build tool tone-maps it, cuts the band and writes the texture. Nothing vendored.

**Fog table** (`PVR_FOG_TABLE`, free per pixel), tied to the DRAW DISTANCE row D:
- Start 0.43 x D, end D, quadratic: f = ((d - start) / (end - start))². At D = 70 that is 30 to 70 m. In the town the hall (26 m) is untouched and the tower (40 m) gets about 6 %. In the cow scene the far pole (44 m) gets about 12 %.
- Nothing is drawn past D. At D everything is full fog, so cutting the ground there pops nothing.
- Fog colour: 0.75 x the sky band's horizon row (averaged over the view's bearings) + 0.25 x land (0.30, 0.36, 0.26). One register write a frame. In the pictures it is (153, 156, 146) head-on and (115, 122, 128) facing north.

**Hills ring**, drawn with the sky (depth write off), radius about 300 m, two strips of 64 segments: about 256 tris, opaque.
- Far hills: smooth, top 0.5 to 3.2 degrees, colour 0.70 x sky horizon + 0.30 x (0.36, 0.44, 0.40).
- Treeline: tree crowns, gaps for fields, top 0.25 to 1.5 degrees, colour 0.5 x fog + 0.5 x (0.20, 0.27, 0.18).
- Both bases go down to -3 degrees and fade to the fog colour. That covers the ground's cut at -atan(1.7 / D) (-1.4 degrees at 70 m), so the floor meets the hills in fog, never the sky at a line.

**Sun.** A `sh4der_bloom_rings` disc with 3 rings at the sun's direction, additive, depth tested, so roofs and the tower cut it.

**The cow scene** uses the same sky, ring and fog. Its far plane is already 70 m.

## 4. Menu toggles for the A/B

One `LOOK` row with `LAUNCH` and `LATE`. LAUNCH means flat colour, no fog, gradient sky: the current picture. LATE is everything above. Under it are separate rows: ATMOSPHERE, GRADE, BLOOM, LENS, TAA. The shadow routes' A/B stays on the A button. A LOOK switch must not change the shadow numbers on the strip.

## 5. Life

- **People and hens:** already planned (8 walkers, 4 hens); the placeholders go.
- **Cloth and pennants:** a sine sway per vertex on the CPU, phase by position, about 0.1 m at the tips. The map casters move with them, so the dotted shadows sway too. That is free proof that the map is live.
- **Chimney smoke:** 3 chimneys, 6 soft translucent puffs each, rising and fading. About 0.2 screen.
- **Birds:** 6 to 10 two-triangle flapping sprites circling the tower and landing on ridges.
- **Dust and pollen:** `sh4der_motes`, one emitter over the lit square, slow drift, warm specks. Visible in the sun and lost in shade, so they also show where the light is.
- **Sparks:** none. `sh4der_spark` stays out unless a smithy is added.
