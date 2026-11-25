# demo/psm, second disc demo: the town square (mock-up)

A second scene for the same A / B shadow program (`DC_MOCKUP.md`): a medieval town square, low sun, people walking about on their own errands. Same routes, same pad, same cost strip; a different scene and a clock. Mock-up only: no code, nothing fetched, nothing measured. Numbers are budgets or guesses and say so.

## Why this scene

The cow scene proves the maths. This one shows why a game wants it:

- **Moving casters.** People walking cross the map every frame; the map is rebuilt each frame anyway, so nothing new for the library, but a lot more to see.
- **Near and far at once.** A person 3 m from the camera and a tower 40 m away in one view: the PSM gain shows on the near person's shadow.
- **Thin things.** Bunting lines across the square, a well's rope and frame, market stall poles, a lamp's bracket, a cart's wheel spokes: thin shadows are where a uniform map breaks up first.
- **Self-shadowing, A against B.** Eaves on the house walls, a stall's awning on its own counter, an arch on the street under it. Route B draws them; route A cannot (ground only). Walk to a wall and press A: the eave shadow goes away. That is the clearest picture of what the SH-4 route buys.

## The square

```
                 N
   +---------------------------------+
   |  church   |  houses  | tavern   |
   |  (tower)  |          |  (sign)  |
   |-----+  arch  +-------+----------|
   |     |  ~~~~~~~~ bunting ~~~~~~~ |
   | stall  stall      well      cart|
   |     |        (rope, frame)      |
   |  lamp           tree       lamp |
   |                                 |
   | houses       fountain?   houses |
   +-------------[ street ]----------+
                 S  <- camera start, eye 1.7 m, facing N
```

- About 30 x 30 m of cobbles, one graded receiver grid as the cow demo (fine near, coarse far), plus the house fronts as receivers on route B.
- Church tower to the north: the long shadow across the square at low sun, and the head-on case at sunset (sun behind the tower, straight at the camera).
- Bunting on two lines across the middle, a few metres up: the thinnest casters in the scene.

## People

- 6 to 10 walkers. Each has a short list of errands on a small waypoint graph: a door, a stall, the well, the tavern, the street out. Walk to one, stop (idle, a few seconds), pick the next. Now and then one leaves down the street and another comes in, so the square is never empty and never crowded.
- All of it from a seed and the clock: the same seed and time give the same people in the same places. A frozen time (pad below) makes an A / B photo pair of the same frame.
- Animation: a walk and an idle, baked to a few key poses at build (vertex positions per pose) and blended on the SH-4, not skinned at run time. Guess: 8 walk poses, 2 idle.
- Two meshes a person: a screen mesh (budget 300 to 500 triangles) and a map mesh for the casters (budget 80 to 150). Route B's raster pays per map triangle, so the map mesh is what matters.

## Art (CC0 only; nothing verified yet)

Candidates found by search, not opened or downloaded. Every one needs its licence checked from the download, a `fetch_table.json` row (sha256 null until `--pin`, url null where there is no stable direct link), and a `CREDITS.md` row.

| Pack | Author | What for | Licence as listed | Source |
|---|---|---|---|---|
| Fantasy Town Kit | Kenney | houses, stalls, well, cart, lamps | CC0 | [OpenGameArt](https://opengameart.org/content/fantasy-town-kit) |
| Medieval Town (Base) | Kenney | walls, buildings | CC0 | [kenney.nl](https://www.kenney.nl/assets/medieval-town-base) |
| Castle Kit | Kenney | tower, arch | CC0 | [kenney.nl](https://www.kenney.nl/assets/castle-kit) |
| Medieval Village MegaKit | Quaternius | alternative for the buildings (the free tier is only part of the kit) | CC0 | [quaternius.com](https://quaternius.com/packs/medievalvillagemegakit.html) |
| Animated Human Low Poly | Quaternius | walkers (walk, idle, work) | CC0 | [OpenGameArt](https://opengameart.org/content/animated-human-low-poly) |
| KayKit Character Animations | Kay Lousberg | walker clips if the KayKit characters are used | CC0 | [OpenGameArt](https://opengameart.org/content/kaykit-character-animations) |

Pick one building family (Kenney or Quaternius), not both: one look, one texture atlas. Kenney's kits share a colormap the way pl4nar's packs do.

### Art on hand (in `demo/psm/assets/3d`, git-ignored; read from the zips, nothing extracted)

| Zip | What | Licence found | Size | Triangles (counted) |
|---|---|---|---|---|
| `modular_village_collection.zip` | 142 OBJ pieces, one `.mtl` of flat colours (no textures): stucco and stone walls, windows, doors, arches, roofs with eaves, canopies, well, street lamp, barrels, crates, ladder, rope rails, waterwheel, windmill, cobbles | **None in the zip. No author either.** Needs its source page | 4.7 MB | 28,478 in all; most pieces 12 to 150, the largest 1,464 (waterwheel), well 1,208, lamp 462, arch 1,228, cobble floor 1,318 |
| `Fantasy Ruins Pack.zip` | 12 Collada (`.dae`) ruins with Torque `.dts`, 512 to 1K JPG colour + normal maps | CC0 1.0, "3TD Studios (Affirmer)" (`3TD_CC0_License_EULA.pdf`) | 37 MB | small pieces 64 to 500; cathedral about 5,300, speaking stones 5,150, temples 2,400 to 3,800 (polygon counts, can be higher in triangles) |
| `chicken.zip` | one `.blend` (11 MB) and two 1K textures | "License: CC0, Author: Čestmír Dammer"; the texture "derived from photos by Rick Hoppmann" (the photos' licence is not stated) | 13.7 MB | not counted (needs Blender) |

What it means for the square:

- The village kit is the square: flat colours (one small palette texture or vertex colours on the PVR), eaves and canopies for the self-shadow test, the well, lamps and rope rails for thin shadows. Its pieces are cheap enough for the map with no cut; the waterwheel and the well want a low version.
- The ruins do not match the village's flat look; a ruined church or arch at the edge of the square could work if its textures are cut to 128 or 256 and its normal maps are dropped. The big pieces need a low version for the map.
- The chicken can be a walker: chickens pecking about the square, going about their business. It needs a Blender export, a triangle count, and the texture photos' licence checked.
- No people yet. Still needed for the walkers.

### Layout from the village kit (counted from the zip, previewed in Python; not Blender)

The kit is on a grid of 1 unit, about 1.5 m (a door 1.35 u, a barrel 0.5 u). Pieces face +X; roofs slope down to +X with the eave on that side. The square is 16 x 16 u open (24 m) inside a ring of houses (about 30 m outside), with the camera on the open south side.

```
          N   hall with Stone_Arch_Outer through it (4 floors)
   +-------+--------------+-------+
   |houses |  hall (arch) |houses |   north: 2 / 4 / 3 floors
   +-------+--------------+-------+
 W |tavern   lamp      lamp  stall|E
   | (sign,   +-rope-+       stall|   east: market, 4 canopy stalls
   |  wall    | well |       stall|         with barrels
   |  lamps)  +------+       stall|
   |houses   lamp      lamp       |   west: tavern 3 floors, houses 2
   +-------------  open  ---------+
                  S   camera
```

| Group | Pieces | Screen tris as is | Map proxy tris |
|---|---|---|---|
| Houses (5 terraces: blocks, windows, doors, roofs) | 309 | 17,252 | 224 |
| Arch (in the hall) | 1 | 1,228 (counted above) | (in the hall box) |
| Stalls (4 canopies on posts) | 20 | 240 | 240 |
| Barrels (8) | 8 | 3,728 | 96 |
| Well | 1 | 1,208 | 44 |
| Rope rails (16 ropes, 16 posts) | 32 | 1,664 | 384 |
| Street lamps (4, on beams) | 8 | 1,896 | 96 |
| Tavern sign, 2 wall lamps | 3 | 1,610 | 36 |
| **Total** | **381** | **27,598** | **1,120** |

As is, the scene is about 2.3 to 3.4 times the screen budget (8 to 12k), before the ground and the people. The map is fine: 1,120 proxy triangles, plus 8 x 80 to 150 for people gives 1,760 to 2,320 against 1,500 to 2,500.

Too heavy, and what to do (targets are guesses):

| Piece | Each | In layout | Cut to | Saves |
|---|---|---|---|---|
| `Roof_Straight_Side_Eave` / `Roof_Straight_Side` (modelled tiles) | 120 / 88 | 88 tiles, 9,152 | one ridged strip a tile, about 20 | about 7,400 |
| `Stucco_Window_Single_Narrow` | 148 | 29, 4,292 | block face + inset quad, about 20 | about 3,700 |
| `Stucco_Block` (hidden faces between blocks) | 12 | 185, 2,220 | merged walls: outside faces only, about 600 | about 1,600 |
| `Prop_Barrel_1` / `_2` | 452 / 480 | 8, 3,728 | 8-sided, about 32 | about 3,500 |
| `Prop_Lamp_Street` | 462 | 4, 1,848 | about 60 | about 1,600 |
| `Stone_Arch_Outer` | 1,228 | 1 | about 150 | about 1,080 |
| `Prop_Well_1` | 1,208 | 1 | about 250 | about 960 |
| `Wood_Rope_Straight` | 76 | 16, 1,216 | thin box, 12 | about 1,000 |
| `Wall_Prop_Sign_1` | 734 | 1 | about 40 | about 700 |
| `Wall_Prop_Lamp` | 438 | 2 | about 60 | about 760 |
| `Cobblestone_Floor_1` | 1,318 a 2 x 2 u tile | not used | the ground is the demo's own graded grid | n/a |

After the cuts (guess): houses about 3,500, props about 1,800, ground grid about 4,000, wall bands under the eaves subdivided for route B about 400, people 8 x 300 to 400 about 2,400 to 3,200. Total about 12 to 13k: the top of the budget. If it is over, the rope rails go first (they stay as map proxies so the thin shadows remain), then people at 300.

**Checked with the decimator** (`third_party/dc-forerunner/tools/lod/qem_decimate.py`, halo-64, CC0; run read-only from the scratchpad): at settings that keep every colour and every part, the scene comes to about 12.7k before the ground and people, about 20k with them. The 12 to 13k above holds only with four extra steps the decimator does not do: roofs taken flat (its max_dev none: 20 a tile, no colour lost, the modelled tiles gone), a merge of the stucco blocks to outside faces, hand-made windows (about 20), and no rope rails on screen. Per-piece results are in the report to the lead.

Route B receivers: a `Stucco_Block` wall face is one quad of 1.5 m, so a per-vertex compare cannot draw an eave's shadow edge on it. The top floor of each facade wants a band of rows about 0.25 u apart under the eave (counted above as about 400 triangles).

Previews (scratch, not in the repo): `eye_view.png` (the demo's camera, eye 1.7 m), `high_view.png`, `corner_view.png`, made by `layout.py` in the session's scratchpad `village` folder.

## Screen

The cow demo's layout, plus a clock:

```
+----------------------------------------------------------------------------+
| ROUTE B  SH-4 DEPTH      MAP PSM                  17:40  (x8)   [HEAD-ON]  |
|                                                                            |
|                   the square from the street, people walking               |
|                                                                            |
|                                                          +--------------+  |
|                                                          |  map inset   |  |
|                                                          +--------------+  |
| MAP 256x256  WARP 4.0 m POINT_FRONT  SUN az 250 el 12  BIAS a .02 b 0      |
| TEXEL/PX  4 m 0.9   10 m 1.4   25 m 2.8   PEOPLE 8  MAP TRIS 2,410        |
+----------------------------------------------------------------------------+
| SH-4 -- ms  PVR -- ms  TA -- v  VRAM -- KB  FPS --                         |
+----------------------------------------------------------------------------+
```

- The clock drives the sun: morning to sunset along one arc. The time speed is shown (`x8`).
- `PEOPLE` and `MAP TRIS` are counts from the program. Route B's cost follows `MAP TRIS`, so it is on screen.

## Controls (changes from the cow demo)

| Pad | Does |
|---|---|
| D-pad left / right | Time of day (moves the sun along its arc) instead of azimuth |
| D-pad up / down | Time speed: paused, x1, x8, x60 |
| START + D-pad left / right | Free sun azimuth (the cow demo's control), for the head-on case off the arc |
| Stick | Walk the camera (eye height fixed), so you can go to a wall and look at an eave |
| START + B | Freeze the people (the clock for the sun keeps going) |
| START + Y | Presets: noon over the square; sunset behind the tower (head-on); morning behind the camera; at a house wall (eave test) |

A, X, L / R, START and the rest as in `DC_MOCKUP.md`.

## Budgets (guesses, to replace with measurements)

| Item | Budget |
|---|---|
| Map casters, static | 1,500 to 2,500 triangles (low versions of the buildings, stalls, well, bunting) |
| Map casters, people | 8 x 80 to 150 |
| Screen triangles | 8,000 to 12,000 |
| Receiver vertices (route B) | 4,000 to 6,000 ground and wall vertices in view |
| Pose blend | 8 x 300 to 500 vertices a frame |
| Map | 256 x 256 (128 KB RAM on B, 128 KB VRAM on A with RTT) |

Static casters do not move, but the map moves with the camera and the sun, so they are drawn again every frame. A cheap first saving if B is too slow: the static casters' homogeneous transforms only change when the map does.

## Open questions

1. Buildings: Kenney (one shared colormap, already used in pl4nar) or Quaternius (more detail, only part free)?
2. Walkers: Quaternius Animated Human, or KayKit characters with KayKit animations? Both need checking for a low enough triangle count.
3. Bake poses at build (RAM, simple) or skin on the SH-4 (less RAM, more CPU, against route B's own CPU)? The mock-up assumes baked poses.
4. Route B receivers: ground and house fronts only, or the people too (a person in a stall's shadow)? People as receivers add their vertices to the compare every frame.
5. One disc with both scenes (a scene pick on the title screen; START + A is already map size) or two programs?
6. Lights at night (lamps, tavern windows)? Not in the library: `sh4der_psm.h` is directional only. Leave the clock between morning and sunset.
