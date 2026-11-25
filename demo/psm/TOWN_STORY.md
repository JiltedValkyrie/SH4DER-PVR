# demo/psm, the town square: setting, story, layout, cast

Research for the second disc scene (`TOWN_MOCKUP.md`, `DC_MOCKUP.md`). No code. Triangle counts marked "counted" were read from the zips in `demo/psm/assets/3d` (nothing extracted into the repo); every other number is a target or a guess and says so. Sun positions are computed (formula below), not looked up. Period details are marked **(sure)**, **(likely)** or **(guess)**.

Aimed at the look brief: shadows are the subject, one flat palette, the hall arch as the focal point, first view at sun elevation 25 to 35 degrees with about half the square lit.

## What is kept and what is not

| Art | Decision | Why |
|---|---|---|
| Modular Village (Keith / Fertile Soil Productions, CC0 as listed on OpenGameArt; the zip carries no licence file) | **Kept.** The whole square. | Already flat colour: 22 `.mtl` colours, no textures. |
| Fantasy Ruins Pack (3TD Studios, CC0) | **Out.** | Photo textures and normal maps. The one useful piece (the tower corner of `CathedralRuins_01`, 5,312 triangles for the whole ruin, counted) is kept as a fallback only: cut the corner out, drop the maps, vertex colour it from the stone entries. Not planned. |
| Chicken (Čestmír Dammer, CC0) | **Kept only flat.** | Drop `UV-Texture.png` (derived from photos whose licence is not stated) and `ChickenNorm.png`; vertex colour from the palette. If the Blender export or the cut fails, the hens go. |
| KayKit Adventurers (Kay Lousberg, CC0 as listed) | **Unchosen, not fetched.** | One candidate body for the walkers. See "What the art lacks". |
| Kenney packs | Not used. | Not fetched; a second look beside the village kit. |

## 1. The setting

**Place.** A small cloth-and-wool market town in Suffolk, East Anglia, about 52.2 degrees north. The town is invented; call it **Hallowmere** (name invented, not a real place as far as checked). Lime-rendered timber houses with clay tile roofs fit East Anglian towns of the period **(likely)**; the kit's cream stucco and red roofs read as that without change.

**Period.** About 1420 **(guess, chosen)**. Late enough for tiled roofs in a town centre and a stone guildhall; early enough that brewing is still mostly alewives' work **(likely)**.

**Day.** Michaelmas, 29 September, the autumn quarter day: rents due, servants hired, a fair **(sure that it was a quarter day; a fair on it is a guess for this town)**. In 1420 England kept the Julian calendar, so Julian 29 September falls on about 8 October in today's calendar **(sure)**. The sun follows the October date: declination about -5.9 degrees.

**The sun that day** (solar time, 52.2 N, declination -5.9; sunrise 06:31, sunset 17:29):

| Solar time | Azimuth | Elevation | 1.7 m person's shadow | 9 m eave's shadow reaches (from the west ring at x = -12) |
|---|---|---|---|---|
| 09:00 | 131 (SE) | 20.5 | 4.6 m | (west ring lit; the east ring shades) |
| 12:00 | 180 (S) | 31.9 | 2.7 m | x = -12 (none on the square) |
| 13:30 | 206 | 28.8 | 3.1 m | x = -4.9 |
| **14:00** | **214** | **26.5** | **3.4 m** | **x = -2.0 at the north end; the shade is a north-west wedge (the ring ends at z = -12)** |
| 14:30 | 221 | 23.7 | 3.9 m | x = 1.5 |
| 15:00 | 229 | 20.5 | 4.6 m | x = 6.1 |
| 15:30 | 236 | 16.9 | 5.6 m | x = 12.5: the square drowned, except the south-east wedge |
| 16:00 | 242 | 12.9 | 7.4 m | beyond the square |

Azimuth from north, clockwise. Computed with the standard sun position formula (hour angle, latitude, declination); no refraction, no equation of time. The program's clock can use the same formula.

**The hour of the first view: 14:00 solar**, elevation 26.5. It meets the brief: inside 25 to 35 degrees, and about half the square lit. The west ring's roofline shadow lies over the north-west of the open cobbles as a wedge, because the ring ends at z = -12. The mock-up measured 37 % of the open square in shade with the tower at its old place. The church tower at its new place (section 3) adds a long finger of shade from the south-west corner to the foot of the arch. In a box model (eaves, no roofs) the ring alone shades 23 % and the ring with the tower 43 %; with roofs the figure is higher, so Cortana's re-render gives the real number. The market on the east is lit. Every person's shadow is 3.4 m long and runs away from the camera to the front right (bearing 34). The clock's useful sweep is **09:00 to 16:00**; after 16:00 the square is all shade.

**Why the square exists.** The town holds a market charter (many English towns had one granted in the 13th century **(sure)**). The square is where the north road comes in under the guildhall and meets the street south to the parish church. The wool guild built the hall over the road with an open arch at ground level; the market grew in front of it. A market cross marks where trade is lawful **(market crosses: sure; the reason as given: likely)**.

**A fact that changes the mock-up.** At 52 degrees north the sun never stands in the north, so a tower north of the square never shades the square and never stands between the camera and the sun. `TOWN_MOCKUP.md` puts the tower north with "sunset behind the tower (head-on)". This story moves the church tower to the south-west, beyond the open corner, at (-27, -31), where the real afternoon sun stands behind it (section 3). The hall stays north as the focal point. If the tower must stay north, its head-on case needs the free sun azimuth control (START + D-pad), which is a sun off the day's arc.

## 2. The story: one Michaelmas afternoon

No dialogue. Everything the player learns comes from where people go, what they carry, and what changes with the clock.

**What the player can read in a 1 to 2 minute walk:**

- It is market day: four stalls under canvas, goods in open crates, hens in a pen, cloth hanging at the clothier's, pennants strung across the square.
- The tavern has ale for sale: the ale-stake (the sign bracket with a green bush on it) is out **(ale-stake: sure)**. The ale-conner, the town's ale taster **(sure: an office under the Assize of Bread and Ale)**, walks the market and stops at the tavern; the alewife brings him a cup; he moves on.
- The hens are loose by the poulterer's stall and scatter from anyone who walks near them. A boy chases them back.
- A porter keeps bringing sacks in through the arch and stacking them by the produce stall; the stack grows through the afternoon (one sack an hour of the clock).
- The friar crosses the square from the arch to the church street at about 15:00 (None) **(the hour's name is real; the friar's route is story)**.
- From about 16:30 the market packs up: open crates become closed ones, the cloth comes down, barrels are lidded. The clock is legible from the props alone.
- The market cross is a sundial without numbers: its shadow sweeps round the open cobbles as the clock runs.

**Cast.** Ten roles, at most eight in the square at once (the mock-up's `PEOPLE 8`). Positions in metres, origin at the square's centre, +x east, +z north (section 3). Each route is a short waypoint list; walk at 1.2 to 1.4 m/s, pause as given, loop. All of it from the seed and the clock, as the mock-up says.

| # | Who | Accent (palette) | Route and loop | Pause poses | Shadow job |
|---|---|---|---|---|---|
| 1 | **Water girl** | blue kirtle (woad) **(likely: woad blue was common)** | SW house door (-11, -9) → well (-2, -7) → back. About 40 s a loop. | 6 s at the well, winding (idle) | **The near person.** She crosses 3 to 5 m in front of the start camera; her 3.4 m shadow lands on lit cobbles. |
| 2 | **Alewife** | green apron (woad + weld) | Tavern door (-12, 3) → ale-conner's meeting point (-9, 2), or → the poulterer (10, 2) with a jug, → back. 60 s. | 5 s at the door, 4 s handing the cup | Walks out of the tavern's shade into sun and back: the shadow edge on a person (route B self-shadow, if people are receivers). |
| 3 | **Ale-conner** | dark brown, hood | Tavern (-9, 2) → each stall (9, -6 / -2 / 2 / 6) → hall arch (0, 11) → tavern. 90 s. | 4 s at each stop, drinking at the tavern | Long diagonal across the square, in and out of the west ring's shade. |
| 4 | **Poulterer** | red hood (madder) | Behind stall 2 (11, 2) ↔ the hen pen (8.5, 0.5). | idle, bend at the pen | Stands under the awning: the awning's edge on her (route B). |
| 5 | **Clothier** | undyed with a red sleeve | Behind stall 1 (11, 6); steps out to the front of the stall and back every 45 s. | idle | Beside the hung cloth strips (thin flat casters on the counter). |
| 6 | **Sweeper** | green kirtle | NE house door (8, 12) ↔ 3 m of doorstep; once a loop to the well. | sweep idle with the broom | Her broom is a thin caster at foot level. |
| 7 | **Roofer's mate** | brown, no accent | At the foot of the ladder on the NE house (5, 12); now and then to the porter's stack and back. | idle, looking up | The ladder's rungs throw a striped shadow on the lit wall behind him (route B self-shadow). |
| 8 | **Boy** | blue cap | Chases the hens from the pen into the open square and back. 30 s bursts, then sits on a barrel. | sit | Small fast caster; the hens scatter round him. |
| 9 | **Porter** (comes and goes) | brown, sack on the shoulder | Arch (0, 12) → produce stall (11, -2): drops a sack → out down the south street (0, -16). Every 90 s; off the square between. | 2 s to drop | Enters from the far focal point and walks straight at the camera: far to near on one caster. |
| 10 | **Friar** (once, about 15:00) | grey habit **(guess: a Franciscan's grey)** | Arch → past the cross → out of the south-west corner to the churchyard gate (-17.9, -24.9). Once a clock hour while 14:00 to 16:00. | none | Crosses the whole square on the long axis. |

When the porter and the friar are both in, the boy sits out (on the tavern step, off the count) so the count stays at eight.

**Hens.** Four, from the chicken model. Inside the rope pen at the poulterer, with two loose. Loop: peck (a tilt of the whole body about the hip, 0.4 s), step 0.2 m, peck. Flee: when a walker passes within 1.5 m, run 1 to 2 m away, then peck. No skinning needed: the peck and the run are rigid tilts and hops (a guess that it reads; the mock-up's baked poses are the alternative). Hens loose in a town square: **(likely)**; towns more often ruled against loose pigs.

## 3. The layout

Origin at the open square's centre. The kit's grid: 1 u = about 1.5 m (mock-up). Storeys taken as 2 u = 3 m (**guess**): two floors put the eave at about 6 m, three at 9 m, four at 12 m. The open square is 24 x 24 m (x and z from -12 to 12), the house ring about 30 m outside, as in the mock-up.

```
                                 N (z+)
                       north street (through the arch)
     +-----------+------------------------+--------------+
     | NW houses |   GUILDHALL  4 floors  | NE house     |    roofline 2 / 4 / 3
     | 2 floors  |  [ Stone_Arch_Outer ]  | 3 floors     |
     |           |   eave band, lit face  | ladder,door  |
     +-----------+------------------------+--------------+
  W  | TAVERN 3f  .  .  .  .  .  .  .  .  .  .  stall 1  |  E
     | ale-stake  ^^^^ pennant line B (z -1, 4.6 m) ^^^^ | cloth
     | wall lamps        +cross (4, 0)    pen  stall 2  | poultry
     |                                      ##  stall 3  | produce, sacks
     | SW houses ^^^^ pennant line A (z -6, 4.2 m) ^^^^^ | stall 4
     | 2 floors      (well -2, -7)                stall  | cooper (barrels)
     +----+          o                                   |
          |    open south side, the street                |
          |                camera (0, -14)  eye 1.7 m   --+
          |                 facing N
   house 1, house 2  (right of the tower, seen from the head-on camera)
     churchyard wall 1.5 m, gate on the sun line
        church tower (-27, -31), 41 m from the centre at bearing 221
```
### The pieces, by place (all names are kit pieces, counted)

**North: the guildhall and its flanks (z 12 to 18).**

- Guildhall, x -6 to 6, four floors: merged `Stucco_Block` walls (outside faces only, the mock-up's cut), `Stucco_Window_Single_Narrow` cut to about 20 each, `Roof_Straight_Side_Eave` / `Roof_Straight_Side_Eave_Edge` along the south face (taken flat), `Roof_Prop_Chimney_Stone` at one end. Ground floor: `Stone_Arch_Outer` (1,228, counted) on the centre line, cut to about 150. The arch passage is the north street.
- The arch is the focal point of the start view: centre line, 26 m ahead.
- Its south face is lit all day, so the eave band (the mock-up's 0.25 u rows) always carries a shadow. Eave overhang is 0.31 u (`Roof_*_Eave` reaches x 0.81 against the wall at 0.5, counted), about 0.47 m; at 14:00 the eave's shadow on the wall is about 0.3 m deep (**computed for a south face**). The arch's soffit and passage are self-shadow on route B.
- NW house x -15 to -6, two floors; NE house x 6 to 15, three floors: the roofline 2 / 4 / 3 from the brief. The NE house's south face is lit after midday: `Prop_Ladder_1` (420, counted; 5.9 m tall) leans on it, cut to about 80. The rungs' striped shadow on the wall is the best self-shadow test in the scene.
- `Wall_Prop_Door_Simple` on the NE house (268, counted), cut to about 30 (the sweeper's door).

**West: the tavern and houses (x -18 to -12).**

- Tavern z -2 to 8, three floors (eave 9 m): `Stucco_Doorway_Wide_Tall` with `Wall_Prop_Door_Ornate` (472 → about 40), `Wall_Prop_Sign_1` (734 → about 40) as the ale-stake bracket with a green bush lump (8, new), two `Wall_Prop_Lamp` (438 each → about 60) as unlit lantern brackets: thin casters on the tavern wall in the morning, when that face is lit.
- SW houses z -12 to -2, two floors (eave 6 m).
- This side casts the first view's big shadow: at 14:00 the tavern's roofline, with the chimney notches, reaches x = -2 across the square.

**East: the market (stalls x 9 to 12, houses x 12 to 18).**

- Four stalls at z = 6, 2, -2, -6, each 3 m wide, opening west onto the square: `Canopy_Full` or `Canopy_Ramp_Full` on `Canopy_Beam` posts (the mock-up's 240 for the four). A counter under each: a box, new (the kit has none), Wood_Medium.
- Stall 1 clothier: three hung cloth strips under the canopy (flat quads, new). Stall 2 poulterer: a hen pen in front, `Wood_Post_Small` x 4 and `Wood_Rope_Straight` x 6. Stall 3 produce: `Prop_Crate_1_Open` x 3 with a produce top (a flat quad in apple and cabbage colours, new) and the porter's sack stack. Stall 4 cooper: `Prop_Barrel_1` / `Prop_Barrel_2` x 8 with `Prop_Barrel_1_Open`.
- Afternoon sun from the south-west falls on the stall fronts: each awning's edge lies across its counter. Route B draws that; route A cannot.
- Houses behind, two and three floors, plain stucco.

**The open square.**

- **Well** `Prop_Well_1` (1,208, counted; its own little tiled roof and windlass) at (-2, -7): 7 m from the camera, on the left third line. Cut to about 250 (mock-up). At 14:00 its shadow (roof, posts, windlass) falls on lit cobbles toward the cross.
- **Market cross** at (4, 0): new, made from kit shapes: a `Stone_Pillar_Base`-like step, an eight-sided shaft 5 m tall, a small cross head, in the stone greys. At 14:00 its shadow is 10 m long on open cobbles; it turns from bearing 17 to 56 between 13:00 and 15:30. Right of the arch's line, so it does not cover the focal point.
- **Pennant lines** A and B (the designer's pick), each from the west facades (x = -12) to the east ones (x = 12), level, no sag; a sag of up to 0.3 m only lowers them and is safe.
  - **Line A: z = -6, 4.2 m up**, on the SW houses' upper floor and the house behind stall 4.
  - **Line B: z = -1, 4.6 m up**, on the tavern's upper floor and the east house opposite.
  - Pennants: triangles 0.4 m wide at the line, 0.5 m drop, every 0.75 m: 32 a line. Muted dyes from the palette accents (madder red, woad blue, weld green, undyed), repeating.
  - **The boot view and the HUD**, computed for eye 1.7 m, pitch down 4 degrees, 60 degree vertical field, 640 x 480. A level line projects to one screen row. Line A's line is at y = 77 px, its pennant tips at y = 105. Line B's line is at y = 116, its tips at y = 133. The arch's top is at y = 166, so neither line crosses the arch. Assumed HUD: 16 px text rows, so the 2-line top strip ends at y = 32 and the clock row at y = 48. Line A is at least 29 px below that. With 20 px rows the top strip ends at y = 60 and line A still clears it.
  - For comparison: line A at 5.0 m (as the mock-up drew it) puts the line at y = 34, inside the top strip. That is the cut-off the mock-up found. At 4.6 m it sits at y = 56, too close; at 4.2 m it clears.
  - Clearance: the tips are 3.7 m (line A) and 4.1 m (line B) above the cobbles, enough for the porter with his sack; line A passes 1 m north of the well, 1.4 m above its roof.
  - Shadows at 14:00: line A's lands at z = 1.0, 4.7 m east of the line's own place; line B's at z = 6.7, 5.1 m east. Both fall on open cobbles 15 to 21 m from the camera: the far thin-shadow test.
- No street lamps on posts: free-standing street lamps were not a feature of a small 1420 town **(likely; London ordered householders to hang out lanterns from about 1416, which is the wall-lamp kind)**. This drops the mock-up's four `Prop_Lamp_Street`.

**South-west: the church tower, the churchyard wall, two houses.**

The tower is at **(-27, -31)** (the mock-up's correction): 41 m from the centre at bearing 221. Footprint x -30 to -24, z -34 to -28, 24 m tall **(guess: a parish tower of 20 to 30 m)**. It stands at the churchyard's corner; the church itself is beyond it to the south and not modelled **(guess; a detached or corner tower is unusual but real in East Anglia: likely)**. If a nave is wanted later, put it south of the tower (x -27 to -12, z -42 to -35): tested, it shades nothing that matters. Never put it east of the tower: there it shades the start camera at 15:00 to 15:30 instead of the tower.

The SW fill below is placed on the **sun line**, the line from the tower toward the square at bearing 56 (the 15:30 shadow direction). Positions are given in that frame too:

- t: metres from the tower's centre along bearing 56.
- n: metres to the side, + toward bearing 326. That is to the right as seen from the head-on camera.
- To convert: x = -27 + 0.829 t - 0.559 n, z = -31 + 0.559 t + 0.829 n.

| Piece | Where (t, n) | World corners or ends (x, z) | Kit pieces | Screen | Map |
|---|---|---|---|---|---|
| **Churchyard wall**, left run | t = 11, n -12 to -1.5 (10.5 m) | (-11.2, -34.8) to (-17.0, -26.1) | `Stone_Wall_1` to `_5` (1 u = 1.5 m long and 1.5 m high, 98 to 152 each, counted): 7 pieces, 1,000 as is. Merged to one prism in `Stone_Brit_Gray` with a `Stone_Bri_Gray_Lighter` coping strip | 14 | 10 |
| Gate gap, 3 m, on the sun line | t = 11, n -1.5 to 1.5 | (-17.0, -26.1) to (-18.7, -23.6) | Two posts: `Stone_Pillar_Base` (44) + `Stone_Pillar` (84) each, cut to eight-sided, about 24 each | 48 | 16 |
| Churchyard wall, right run | t = 11, n 1.5 to 4.5 (3 m) | (-18.7, -23.6) to (-20.2, -21.2) | 2 `Stone_Wall_*`, merged as above | 14 | 10 |
| **House 1**, 2 floors (eave 6 m) | t -5 to 2, n 6 to 12 | (-34.5, -28.8), (-28.7, -24.9), (-32.1, -19.9), (-37.9, -23.8) | The ring's family: merged `Stucco_Block` walls, `Roof_Straight_Side_Eave` taken flat (ridge along t), 2 `Stucco_Window_Single_Narrow` cut to 20, `Stucco_Doorway_Narrow_Tall` (60), `Roof_Prop_Chimney_Stone` (120, cut to 20) | 120 | 12 |
| **House 2**, 2 floors | t 2 to 9, n 6 to 12 | (-28.7, -24.9), (-22.9, -21.0), (-26.2, -16.0), (-32.1, -19.9) | as house 1, door on the gable that faces the square | 120 | 12 |
| Ground between the square and the wall | t 11 to 30, n -12 to 12 | | the demo's ground grid carried on, coarse (2 m cells; **guess**) | about 400 | 0 |
| **SW fill total** | | | | **about 716** | **60** |

What it does (box model, 1 m steps, roofs ignored):

- **The tower at 15:00 to 16:00.** Its shadow crosses the south-east wedge, the last lit part of the square, and covers the start camera (0, -14) at 15:30 and 16:00. It is a caster 32 m away and out of view shading the ground at the camera: the near / far case. Sampled on the wedge, the tower alone shades 52, 55 and 44 points (of 120) at 15:00, 15:30 and 16:00; house 2 adds 3 at 15:30 and 11 at 16:00, so the two houses do not hide it.
- **The tower at 14:00, in the first view.** Its 48 m shadow runs from the square's south-west corner, (-14, -12), to the foot of the arch, (0, 9 to 11). The tower is out of view, but its shadow is a leading line to the focal point. The well and the cross stay lit. If the designer wants the arch foot lit, boot at 13:30: the tip then stops at about (-8, 8).
- **The wall at 16:00, the head-on preset.** Its 1.5 m height throws a band 6.5 m deep toward the camera along its whole length. The tower's finger crosses that band at the gate. In a sample of the ground just in front of the wall: wall 361 points, tower 156, houses 36, lit 77. That gives the two shadow edges the designer asked for. At 15:30 the band is 5 m deep.
- **The head-on frame** from (6, -9) facing 236 (horizontal field about 75 degrees, so ±37 about the axis):
  - The tower is on the axis at 39.7 m.
  - The wall runs from 22 degrees left to 10 degrees right, at 29 to 31 m.
  - The two houses are 11 to 15 degrees right, at 31 to 46 m.
  - The SW ring corner is at the right edge (25 degrees, 18 m).
  - The ground in between, with the wall's and the tower's shadows on it, fills the lower half.
- The camera at (6, -9) is lit at 16:00 and in the tower's shadow at 15:30.

The map fit must keep the tower in the caster set though it is outside the view frustum (lead's call 3; mock-up finding 6: cull casters by the map, not by the view).

### Where the shadows are, by preset

| Preset (START + Y) | Time | Camera | What it shows |
|---|---|---|---|
| **First view** (boot) | 14:00, el 26.5 | (0, -14) facing N, pitch down 4 | About half the square lit; west roofline shadow with chimney notches as a north-west wedge; the unseen tower's finger from the south-west corner to the arch foot; well and cross shadows; the water girl's shadow near; pennant lines' shadows far; awning edges on the east counters. |
| Morning behind the camera | 09:00, az 131, el 20.5 | (0, -14) facing N | Sun behind right; shadows run away to the front left; the tavern face lit, its lantern brackets casting on it. |
| Tower shadow | 15:30, el 16.9 | (0, -14) facing N | The square drowned but the SE wedge; the tower's band across the wedge at the camera. |
| Head-on, real sky | 16:00, az 242, el 12.9 | (6, -9) facing bearing 236 | The tower against the sun, 6 degrees off it; the wall's band and the tower's finger run at the camera. Replaces "sunset behind the tower". |
| Eave test | 13:00 | (3, 9) facing the hall | The eave band on the lit hall face; the arch's passage self-shadowed. |
| Ladder test | 14:00 | (5, 8) facing the NE house | The rungs' stripes on the wall; press A to see them go. |

The brief's composition holds in the first view: the arch on the centre line at 26 m, the well on the left third at 7 m, the stalls down the right edge, the roofline 2 / 4 / 3 left to right. The map inset (bottom right) sits over the east stalls' lower half, not over the well or the arch.

## 4. Props and cast: cost list

Screen and map (caster proxy) triangles per item. "Counted" is from the zip; everything else is a target. Budgets from `TOWN_MOCKUP.md`: screen 8,000 to 12,000; static map 1,500 to 2,500; people 8 x (screen 300 to 500, map 80 to 150); the brief asks for 300 to 400 screen a person.

| Item | Pieces | As is (counted) | Screen target | Map proxy |
|---|---|---|---|---|
| Houses and hall, all five frontages | mock-up's cut list | 17,252 | 3,500 | 224 |
| Hall arch | `Stone_Arch_Outer` | 1,228 | 150 | (in the hall) |
| Eave bands for route B | new rows | n/a | 400 | 0 |
| Church tower, 24 m | merged block box, four `Stone_Arch` belfry openings (44 each, counted), a pyramid roof | n/a | 250 | 14 |
| Well | `Prop_Well_1` | 1,208 | 250 | 44 |
| Market cross | new, kit greys | n/a | 100 | 30 |
| Stalls x 4 | `Canopy_Full` / `Canopy_Ramp_Full`, `Canopy_Beam` | (mock-up 240) | 240 | 240 |
| Counters x 4 | new boxes | n/a | 48 | 48 |
| Barrels x 8 | `Prop_Barrel_1`, `_2`, `_1_Open` | 3,728 | 256 | 96 |
| Crates x 3, produce tops | `Prop_Crate_1_Open` / `Prop_Crate_1` | 1,524 | 66 | 36 |
| Sack stack, up to 6 | new lumps | n/a | 48 | 24 |
| Ladder | `Prop_Ladder_1` | 420 | 80 | 24 |
| Tavern sign and bush | `Wall_Prop_Sign_1` + new | 734 | 48 | 12 |
| Tavern lantern brackets x 2 | `Wall_Prop_Lamp` | 876 | 120 | 24 |
| Doors x 2 | `Wall_Prop_Door_Ornate`, `_Simple` | 740 | 70 | 0 |
| Hen pen | `Wood_Post_Small` x 4, `Wood_Rope_Straight` x 6 | 536 | 92 (posts; ropes as flat strips) | 112 |
| Pennant lines x 2 | new: 32 triangles a line, the line a map proxy only | n/a | 64 | 88 |
| SW fill: churchyard wall, gate posts, two houses, ground to the wall | see section 3 | n/a | 716 | 60 |
| Hung cloth, stall 1 | new quads | n/a | 6 | 6 |
| Carried props (bucket, jug, broom, staff, sack) | new; `Prop_Broom` 236 counted | n/a | 8 x 20 = 160 | 8 x 8 = 64 |
| Ground | the demo's graded grid | n/a | 4,000 | 0 |
| People, 8 in the square | unchosen body, see below | unknown | 8 x 350 = 2,800 | 8 x 80 to 150 |
| Hens x 4 | the chicken, cut | not counted (needs Blender) | 4 x 120 = 480 | 4 x 20 = 80 |
| **Total** | | | **about 13,900** | **static about 1,130; with people and hens 1,850 to 2,390** |

The map is inside its budget. The screen is about 1,900 over the top of the budget **as a whole scene**. A frame draws only what is in view, so the drawn count is lower; nothing is measured. The start view's horizontal field is about 75 degrees and sees neither the tower nor the SW fill nor the south side: the mock-up's first view came to about 7,900. The head-on view sees the SW fill and the south-west of the square but not the north and east rings. If a drawn count is still over, cut in this order:

1. Hens to 3 at 80 (saves 240).
2. People to 300 (saves 400).
3. Well to 180, lantern brackets to 40 each (saves 110).
4. Pennant lines to 20 triangles each (saves 24 screen, keeps the thin shadow).
5. The mock-up's last resort: houses' back faces and hidden roof tiles.

## 5. What the art lacks, and the cheap fix for each

| Missing | Fix | Cost |
|---|---|---|
| **Townsfolk.** No people in anything on hand. | One body for all ten roles, recoloured by vertex colour from the palette, with a hood, cap or habit as separate small meshes, and the role told by what is carried. **Unchosen:** KayKit Adventurers (CC0 as listed, not fetched; stylised big-headed bodies, triangle count unknown; the Mage's robe could make the friar, the Rogue's hood the ale-conner) or Quaternius Animated Human (in the mock-up's list, not fetched). Both need: a fetch row, a licence check from the download, a count, a cut to 300 to 400 with a clear silhouette, and baked walk and idle poses. | 8 x 350 screen, 8 x 80 to 150 map |
| **A child.** | The same body at 0.75 scale with a cap **(guess that it reads as a boy at this distance)**. | as an adult |
| **A tower.** The kit has a windmill and a waterwheel, no tower. | Build one from kit parts: merged stone walls, four `Stone_Arch` belfry openings, a low pyramid roof in `Dark_Red_Roof` or the stone grey. Fallback: the tower corner of `CathedralRuins_01`, flat recoloured (out unless the kit tower looks wrong). | 250 / 14 |
| **Bunting.** The kit has rope rails, not bunting. | Two pennant lines (the designer's pick over cloth strips): triangles 0.4 m wide, 0.5 m drop, 32 a line, in the muted palette accents. Triangle-flag bunting reads more 19th century than 1420 **(likely)**. It is kept for the read, and fair-day pennants are a guess. The line itself is a map proxy only (the brief: thin ropes shimmer on screen); the pennants are drawn. Heights and HUD clearance in section 3. | 64 / 88 |
| **The church street's end** (the head-on view looked onto empty ground). | A churchyard wall and two houses from the kit, placed in section 3. | 716 / 60 |
| **Stall counters.** | A box under each canopy. | 12 each |
| **Goods.** No food or cloth meshes. | A flat quad on top of each open crate, coloured in blocks (apples, cabbages); cloth as flat quads; sacks as lumps in `Burlap`. | 2 to 8 each |
| **Market cross.** | An eight-sided shaft on a step, from the kit's stone greys. | 100 / 30 |
| **Carried props.** Bucket, jug, staff, sack. | Small new meshes; the broom is `Prop_Broom` cut. | about 20 each |
| **A tree, a fountain, a cart** (in the mock-up's first drawing). | Dropped. The well and the cross do their work; a fountain would be unusual in a small 1420 English town **(likely)**; a cart's spokes would be a good thin caster but cost a new model. Add a cart later only if the thin-caster test wants more. | 0 |
| **Hen texture licence.** | Do not use the texture: vertex colour (Stucco for the body, Red_Roof for the comb, Straw for the legs). | 0 |
| **The hens' animation.** The `.blend` may or may not be rigged (not opened: Blender is not on this machine's path). | Rigid peck tilt and hop, no skinning. | CPU: one matrix a hen |
| **Sound** (bells for the hours). | Out of scope. The friar's walk stands in for None. | 0 |

## Decisions (lead)

1. The church tower stands south-west, at (-27, -31) after the mock-up's correction. The real sun gives the head-on case. The hall stays north as the arch focal point. The SW fill (churchyard wall, two houses) and the pennant heights are the designer's picks, placed in section 3.
2. Ten roles, at most eight in the square at once.
3. The caster set includes casters out of view toward the sun (the tower at 15:30).
4. People are route B receivers.
5. No Blender hen in the first build. The hens are a procedural low-triangle hen from the build tool, flat palette colours; the chicken model is for later. The hen rows above (120 screen, 20 map) are the target for the procedural hen.
