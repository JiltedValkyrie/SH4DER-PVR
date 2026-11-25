# demo/psm, second disc scene: Hallowmere market square (mock-up)

The second scene of the A / B shadow disc (`DC_MOCKUP.md`), built from the story in `TOWN_STORY.md`: one Michaelmas afternoon in an invented Suffolk market town, about 1420, at 52.2 N. Same routes, same pad, same cost strip as the cow scene; a different scene and a clock. It ships after the cow scene, on the same disc, picked on the title screen.

This is a mock-up. No demo code. The pictures come from a scratch renderer that uses the library's own map maths. Nothing in them was measured on a Dreamcast; every triangle count is a target or a count from the zip, and says which.

## The pictures

All in `demo/psm/out` (git-ignored). Each is 1280 x 480: route A on the left, route B on the right, each a 640 x 480 screen.

| File | Time, sun | Camera (story metres) | What it shows |
|---|---|---|---|
| `town_first_ab.png` | 14:00 solar, az 214, el 26.5 | (0, -14), eye 1.7 m, facing N, pitched down 4 degrees | The boot view, map inset off ("Y: map off"). The hall arch centred at 26 m, the well on the left third line at 7 m, the stalls down the right edge. The water girl beside the well at (-0.6, -8.2): her shadow lies on lit open cobble, apart from the well's. The west ring's shade over the north-west of the square; on route B the east house's diagonal eave shadow. Pennant lines A and B across the upper third, their dotted shadows on the far cobbles. The tower (out of view) shades the ground from the south-west corner to the arch: **the arch foot and the porter under it are in shade.** 54 % of the open square is in cast shade (route B; 55 % route A), computed on a 0.25 m grid with the library's compare. |
| `town_first1330_ab.png` | 13:30, az 206, el 28.8 | as the boot view | Not a preset (the designer kept the boot at 14:00); kept as a note. The tower's shadow stops short of the arch, so the arch foot and the porter are lit. 30 % in cast shade (route B; 31 % route A). Rendered before the head-on changes below. |
| `town_headon_ab.png` | 16:00, az 242, el 12.9 | (8, 8) facing bearing 234 | Rejected by the designer, not re-rendered: the head-on preset with the tower at the story's first place, (-33, -22). Its shadow falls inside the west ring's shade (see "Findings"). |
| `town_headon_tower2_ab.png` | 16:00, az 242, el 12.9 | (6, -9) facing bearing 236 | The head-on preset, map inset on, bottom right. The tower at (-27, -31) against the sun; the churchyard wall with its gate across the middle distance; house 2 right of the tower, and house 1 behind it, three storeys, its gable and chimney above house 2's roof (staggered gables). Two shadow edges run at the camera: the wall's band and the tower's finger through the gate. The taller house 1 also shades the ground right of the finger, so the lit ground is a strip beside the finger (near lit cobble pixels: route A 27,470 to 12,711, route B 21,294 to 6,439, against two storeys). On route A (PSM kept) the edges are sharp near the camera; on route B (PSM fell back to uniform, `[HEAD-ON]`) they are blurred. No person is in this view, so the inset covers none. |

How the pictures are made: `town_scene.py` places the kit pieces from the zip (nothing extracted). `town_render.c` builds the map with `sh4der_psm_build_warp` / `_uniform` and fills it with `sh4der_psm_raster_tri`, compiled on the host with `SH4DER_CORE_NO_SH4ZAM`. `town_hud.py` draws the strips. All three live in the session scratchpad, not in the repo.

- **Route A** in the picture: the map is a silhouette and only the ground receives it, looked up per pixel with a 2 x 2 filter. On the console the lookup coordinates come per vertex, so a warped map is off between vertices (`sh4der_psm_pvr_uv_error`).
- **Route B** in the picture: the library's plane compare per pixel (`sh4der_psm_raster_lit_plane`, bias a 0.02 + b 1.5 texels, PCF). On the console it is per vertex, so this is its best case: an edge is only as sharp as the receiver's vertex spacing (the eave bands below).
- **Kit as is:** the pieces are drawn uncut (32,592 triangles in the first view). The on-screen count is the story's target scaled by how much of each item is in view.
- **The ground** is a plain 1.5 m grid here, not the demo's graded receiver grid.

## The look (the designer's brief, and how the pictures meet it)

| Brief | In the pictures |
|---|---|
| One flat palette: warm mid-stone cobbles, cream walls, timber frames, terracotta roofs | The kit's `.mtl` colours with four overrides. Stucco (0.78, 0.75, 0.68). Window and door frames a mid timber (0.44, 0.365, 0.29); `Wood_Darker` kept only for posts and beams (canopy beams, wood posts, prop supports, stall posts). Tower stone (0.78, 0.70, 0.64), the kit's light grey stone x 1.18. Cobbles (0.75, 0.665, 0.54) / (0.725, 0.64, 0.52). Cloth in muted dyes: (0.66, 0.40, 0.34), (0.46, 0.53, 0.66), (0.52, 0.60, 0.42), (0.86, 0.81, 0.70). |
| Cool lifted shadows, never black | Shade = colour x 0.48 x (0.80, 0.88, 1.06); sun = + 1.00 x N.L x (1.04, 0.98, 0.88). Darkest cobble in shade is about 0.28. |
| Shadow edges the highest contrast in frame | Measured luminance in the first view (route B): near cobble 0.571 lit, 0.278 shade (2.05 : 1). Lit stucco 0.819, lit frames 0.423 (1.94 : 1), below the ground edge. The pennants are muted so they stay below it too. |
| Pennants, none cut by the top strip | Line A (z -6, 4.2 m) sits at screen y about 77, its tips about 104; line B (z -1, 4.6 m) at about 116, tips about 133. The top strip is two 14 px rows, clock on the second, and ends at y 32. Line A clears it by about 45 px. |
| Head-on: shaded faces together | Churchyard wall stone 1.51 x the kit's `Stone_Brit_Gray` = (0.73, 0.64, 0.56), coping and gate-post shafts 1.15 x that = (0.84, 0.74, 0.65). Its shaded face measures 0.276 (was 0.19 in the kit grey), with the shaded cobble 0.278 and the tower 0.302. |
| Head-on: tower dark against a warm sky | Tower in shade 0.302 (measured, tower-2 view). Sky a gradient from zenith blue to a warm pale gold (0.97, 0.88, 0.68) at the horizon on the sun's side. No sun disc (it would cost a fan of triangles; none counted). |
| First view at el 25 to 35, about half the square lit | 14:00, el 26.5: 46 % of the open square lit, the arch foot in the tower's shade. 13:30, el 28.8: 70 % lit, the arch foot lit. |
| Hall arch focal, well 6 to 8 m out on a third line, stalls on the E edge | Arch on the centre line at 26 m; well at 7 m, image x about 200 of 640 (third line 213); stalls on the right edge. |
| HUD in the top and bottom strips only, map inset clear of the arch and the well | Top strip 2 lines, bottom strip 4 lines. The inset is off at boot ("Y: map off" bottom right); Y shows it bottom right, over ground. It is shown in the tower-2 picture, where no person is in view. |
| No art we do not have | People are grey blocks with an accent band, hens small grey blocks, labelled PLACEHOLDER in the bottom strip and "MOCK-UP: placeholders" in the top strip. |
| Counts on the picture against 8 to 12k | Bottom strip: "TRIS in view ~7,864 target / 8-12k OK (scene 13.2k; kit as is 32,592)" in the first view; the map casters' target (about 2,330 with people and hens) against 1.5 to 2.5k. |

## Screen, 640 x 480

```
+----------------------------------------------------------------------------+
| ROUTE B  SH-4 depth map  PSM 256                     MOCK-UP: placeholders |  top strip
| depth 128 KB RAM  kind POINT_BEHIND  bias a.02 b1.5 PCF  self-shadow: yes  |
|                                                       14:00 sun az214 el26.5
|                                                                            |
|                 the square from the south street, eye 1.7 m                |
|                                                                            |
|                                                                            |
|                                                                            |
|                                                                 Y: map off |  inset off at boot
+----------------------------------------------------------------------------+
| TRIS in view ~7,823 target / 8-12k OK  (scene 13.2k; kit as is 32,590)     |  bottom strip
| map casters ~2,330 target / 1.5-2.5k  TEXEL/PX 4m 2.7 10m 2.4 25m 2.3 ...  |
| SH-4 -- ms  PVR -- ms  TA -- v  VRAM -- KB  FPS --   square in shade 54%   |
| grey blocks = PLACEHOLDER people (8) and hens (4), not final art           |
+----------------------------------------------------------------------------+
```

On the console the first bottom line is the program's own count of triangles sent. The cost line stays `--` until it has a measured sample.

## The square (from `TOWN_STORY.md`, section 3)

Story coordinates: metres, x east, z north, origin the square's centre. The open square runs -12 to 12 each way. The kit's unit is 1.5 m; a storey is 2 kit units (3 m).

| Place | What | Built in the mock-up from |
|---|---|---|
| North, facades at z = 12 | NW houses 2 storeys, guildhall 4 storeys with the arch on the centre line, NE house 3 storeys with the ladder and the sweeper's door | `Stucco_Block`, `Stucco_Window_Single_Narrow_Tall`, `Stucco_Doorway_Narrow_Tall`, `Stone_Arch_Outer`, `Roof_Straight_Side(_Eave)`, `Roof_Prop_Chimney_Stone`, `Prop_Ladder_1`, `Wall_Prop_Door_Simple`; gable triangles new |
| West, facades at x = -12 | SW houses 2 storeys, tavern 3 storeys (wide door, ornate door, ale-stake with a green bush, two lantern brackets), corner house 2 storeys | as above, plus `Stucco_Doorway_Wide_Tall`, `Wall_Prop_Door_Ornate`, `Wall_Prop_Sign_1`, `Wall_Prop_Lamp`; the bush a new box |
| East, facades at x = 12 | houses 2 and 3 storeys behind four stalls at z 6, 2, -2, -6 | stalls: four posts, a 2 x 2 `Canopy_Top`, a counter box (new) |
| Stall 1 / 2 / 3 / 4 | clothier (three hung cloth strips), poulterer (hen pen: `Wood_Post_Small` x 4, `Wood_Rope_Straight` x 6), produce (`Prop_Crate_1_Open` x 3 at 0.55 scale with produce tops, sacks), cooper (8 barrels) | |
| Open square | well at (-2, -7), market cross at (4, 0) (step, eight-sided shaft 5 m, head; new), pennant lines A (z -6, 4.2 m) and B (z -1, 4.6 m), x -12 to 12, level, 32 pennants each (0.4 m wide, 0.5 m drop, every 0.75 m), the rope a map caster only | `Prop_Well_1`; the rest new |
| South-west | church tower 6 x 6 x 24 m at (-27, -31), belfry openings, pyramid roof (new) | new |
| South-west fill, in the story's sun-line frame (t along bearing 56 from the tower, n to its right) | churchyard wall at t = 11: left run n -12 to -1.5, gate 3 m, right run n 1.5 to 4.5; 1.5 m high, 0.5 m thick with a lighter coping; two eight-sided gate posts 2 m. Houses 1 and 2, 2 storeys, n 6 to 12, ridge along t, house 2's door on the gable facing the square | wall and posts new boxes in `Stone_Brit_Gray` / `Stone_Bri_Gray_Lighter`; houses the ring's terrace pieces turned to the sun line, `Wall_Prop_Door_Simple` |
| Ground | 1.5 m cobble grid, x -42 to 33, z -40.5 to 21 (carried south past the churchyard wall) | new |
| People at 14:00 | water girl (-0.6, -8.2), beside the well, alewife (-10.4, 2.6), ale-conner (3, 6), poulterer (10.9, 2), clothier (10.9, 6.2), sweeper (10, 11), roofer's mate (7.4, 10.6), porter (0.6, 9) | grey block placeholders with the role's accent band |
| Hens | (8.2, 0), (8.8, 1.2) in the pen, (7.0, -1.6), (6.3, 0.9) loose | small grey block placeholders (the procedural hen from the build tool replaces them) |

## Findings for the story and the coder

1. **The tower's shadow does not do what the story says.** From (-33, -22) at 15:30 (sun az 236) the tower's shadow points to bearing 56. The start camera at (0, -14) is at bearing 76 from the tower, so the shadow misses it by 20 degrees. It reaches the camera's feet only near 17:00 (sun about az 254, el about 4), when the square is all shade. At 16:00 the shadow falls inside the west ring's shade and cannot be seen (`town_headon_ab.png`).
   - **Adopted in `TOWN_STORY.md`:** the tower moves to (-27, -31), 41 m from the centre at bearing 221. Its 15:30 to 16:00 shadow then crosses the lit south-east wedge, and the head-on preset stands in that wedge at (6, -9) facing bearing 236, with the sun 6 degrees off the tower (`town_headon_tower2_ab.png`).
2. **The tower's shadow takes the arch foot at 14:00.** With the tower at (-27, -31) its shadow runs in from the south-west corner and reaches the hall: the arch foot and the porter under it are in shade, and 54 % of the open square is in cast shade (the story's box model: 43 %; without the tower it was 37 %). The well and the cross stay lit. At 13:30 the shadow stops short of the arch and 30 % is in shade (`town_first1330_ab.png`). The designer kept the boot at 14:00.
3. **The head-on view's south-west is filled** (the story's churchyard wall and two houses). The houses are 5 kit tiles (7.5 m) long, not the story's 7 m: house 1 is t -5.5 to 2, house 2 t 2 to 9.5, leaving 1.25 m to the wall. As the story placed them, house 1 stood behind house 2 on the same bearing from the head-on camera. No slide along n shows a third of it beside house 2: house 2's silhouette ends at 22 degrees right and the SW ring's south house covers everything past 24.5 degrees, so at most 2.5 of house 1's 10.7 degrees (23 %) could show; a slide left meets the tower (its right edge at 6.2 degrees, house 2's left at 9.4). So house 1 stays at n 6 to 12 and is three storeys (eave 9 m, gable top about 12 m), its roof above house 2's. That keeps the sky gap to the tower and the 1.25 m to the wall. Its longer shadow (about 52 m at el 12.9) shades the ground right of the tower's finger.
4. **Whether PSM survives head-on depends on the fit's receiver slab.** In the head-on pictures route A (receiver slab 0 to 0.5 m: the ground) keeps PSM (POINT_FRONT, texel / pixel 7.4 at 4 m). Route B (slab 0 to 13 m: ground and walls) falls back to uniform (31 at 4 m). The coder should choose the slab per route and show the fallback, as the pictures do.
5. **Route B's walls need the eave bands.** A `Stucco_Block` face is one 1.5 m quad, so a per-vertex compare cannot draw an eave's or the ladder's shadow on it (about 400 triangles of rows, counted in the story's budget).
6. **The out-of-view caster** (the lead's call 3): the tower is outside the view in the first picture and still in the map, because every caster triangle is rasterised. A caster cull by the view frustum would lose it; cull by the map's own square instead.

## Budget (from `TOWN_STORY.md`, section 4)

Scene target about 13,200 screen triangles, map about 1,070 static and 1,790 to 2,330 with people and hens. In the first view the targets scaled by what is in view come to about 7,900. That is under the 12k top, because the view does not see the tower, the south side or the backs of the rings.

If a drawn count is over, cut in the story's order: hens to 3 at 80, people to 300, well to 180 and lantern brackets to 40 each, cloth lines to 10 strips, then the houses' back faces and hidden roof tiles.

What the cuts need, from the decimator run (`third_party/dc-forerunner/tools/lod/qem_decimate.py`, halo-64, CC0; credit it in `CREDITS.md` if a cut piece ships):

- Clean (no crack, no flip, no colour lost): `Stone_Arch_Outer` 1,228 to 170; `Prop_Barrel_2` 480 to 66; windows 148 to 76; roofs 120 / 88 to 50 / 44 (or 20 each, flat).
- Not clean: `Prop_Barrel_1` loses `Wood_Dark` at every setting, so use `_2`; the well keeps every colour only at 786, and loses one wood part at 384; the lamps lose `Candlelight` below about 130.
- The walls' 12-triangle blocks need a merge to outside faces, which the decimator does not do.

## Controls (changes from the cow scene)

| Pad | Does |
|---|---|
| D-pad left / right | Time of day, 09:00 to 16:00 solar (the story's sweep), moves the sun on the day's arc |
| D-pad up / down | Time speed: paused, x1, x8, x60 |
| START + D-pad left / right | Free sun azimuth (off the arc), the cow scene's control |
| Stick | Walk the camera, eye 1.7 m |
| START + B | Freeze the people and hens (the sun's clock keeps going) |
| START + Y | Presets, from the story: first view (14:00); morning behind the camera (09:00); tower shadow (15:30); head-on (16:00, facing the tower); eave test (13:00, facing the hall); ladder test (14:00, facing the NE house) |

A, X, L / R, START and the rest as in `DC_MOCKUP.md`.

## Settled (the lead's calls on the story's questions)

1. Tower to the south-west; the real sun gives the head-on case; the hall stays north as the focal point. (Its exact place: finding 1.)
2. Ten roles, eight in the square at once.
3. The map's caster set includes casters outside the view toward the sun (finding 6).
4. People are route B receivers.
5. No Blender hen in the first build: a procedural low-triangle hen from the build tool.
6. The designer: boot at 14:00, no 13:30 preset. Frame contrast stays as it is (frames 0.42, stucco 0.82); nothing may raise it.

## Open

1. Walker body: KayKit Adventurers or Quaternius Animated Human, neither fetched nor counted yet.
