# Level of detail (LOD): tiers, budget, switch points and fades

Scope: every <Game> build. Source of the numbers: the Halo Dreamcast port (dc-halo). Paths are relative to the dc-halo repo root unless they name another repo. Checked against dc-halo on 2026-11-15. Distances: wu is Halo's world unit; 1 wu = 3.048 m (dc-halo `metres()` in `src/menu_draw.c`, and the 50 wu = 152 m and 9 wu = 27 m pairs in the menu notes).

Labels: **MEASURED** (a boot or serial log), **CODE** (a constant or a line in the source), **CENSUS** (a count from the LOD census doc), **ESTIMATE** (our arithmetic), **GUESS**.

Generic version: the tiers and the switch values are dc-halo's. Set your own budget, your own distances and your own tier names, and keep the rules in section 1.

## 1. The rules (owner rules, 2026-11-15, and the memory they came from)

1. **Budget: about 6,000 triangles on screen for 30 fps.** One 1,000-triangle model is one sixth of the budget. Never call a model of about 1,000 triangles light, far or near.
2. **Every mob gets every LOD tier, on every map it appears on.** Near, far, xfar and super xfar. No per-map gaps. A census first, then the cuts.
3. **Far is a real cut.** Far is at most about 45 % of near (triangles and vertices) where the cut is reachable. A far tier equal to the near tier gives no LOD at all.
4. **Enemies fade in when they first appear** (campaign only). No mesh-swap fade.
5. **Idle enemies fade in by distance from 80 wu** (244 m), fully drawn at 68 wu (207 m). This is fixed. The object distance slider does not move it.
6. **No fade between character LOD tiers.** Tiers swap hard. A tier fade costs a second draw of the model for a half second.
7. **Pop-in is not wanted** for LOD swaps, cards, shadows or decal edges, except the appearance fade in rule 4.
8. **Ships use ship rules, not prop distance.** The Pelican and the dropships follow the ship reach rule (section 4).
9. **Light beams draw their beam part far.** The lamp body follows the object distance slider.
10. **Debug sliders for testing** (FAR ENEMY CAP, ENEMY FADE START) are session-only and change nothing when untouched.
11. **Halo-derived data never goes into git.** Our own authored LOD objects (far and xfar OBJ files that we made) are committed as files, so a fresh checkout builds (owner rule, memory note 2026-11-08).

## 2. The budget in numbers

| Item | Value | Source |
|---|---|---|
| Triangles on screen for 30 fps | about 6,000 | owner rule; `docs/PERF_NOTES.md` (Jackal section, 2026-11-10) |
| Examples (ESTIMATE: 6,000 divided by the model's triangles, nothing else on screen) | | |
| One Jackal, near set | 525 drawn | `docs/PERF_NOTES.md` (Jackal section) |
| Jackal near, as many as fit | about 11 | 6,000 / 525 |
| One grunt, near (census, Performance blob) | 734 triangles, 839 vertices | CENSUS, `docs/archive/graphics/LOD_CENSUS_376.md` |
| One grunt, far (census, after the cut) | 205 triangles, 347 vertices (28 % / 41 %) | CENSUS |
| Grunts far, as many as fit | about 29 | 6,000 / 205 |
| One hunter, far (census, after the cut) | 1,200 triangles (100 % of near) | CENSUS |

A hunter's far tier is one fifth of the budget. It was flagged as "lighter than the near one" for that reason: the census lists it as 100 % of near, so it has no cut yet.

## 3. Tiers and their names

**Characters (enemies, marines, the Chief):**

| Tier | Name in the menu | What it is (dc-halo) | Source |
|---|---|---|---|
| lod0 | PERFORMANCE (the low cut) | Halo's low, or the Performance cut of the model | `src/model_tier.h` (`MODEL_TIER_PERFORMANCE` 0) |
| Balanced | BALANCED | Halo's medium for the Chief; a hand cut of the low, medium and high slots for other characters (side paks) | `MODEL_TIER_BALANCED` 1; `tools/assets/convert_models.py` |
| Fidelity | FIDELITY (kept in the table, not in the menu) | Halo's high | `MODEL_TIER_FIDELITY` 2 |
| Xtreme | XTREME (kept in the table, not in the menu) | Halo's super high | `MODEL_TIER_XTREME` 3 |
| Far | far mesh (Halo's super low, or a hand cut) | the far head; OpenCE's pixel switch goes here | `model_tier_far` in `src/model_tier.h` |
| SUPER XFAR (SXFAR, "xxfar") | a third body, 90 to 110 triangles | one hand-made body per character, past the far mesh | `src/model_tier.h`, census column SX |

The menu offers two values for OBJECT DETAIL: **DYNAMIC** and **LOW** (`video_object_order` in `src/menu.c`: DYNAMIC, PERFORMANCE). DYNAMIC: the Performance models and rules, and the enemy you aim at is shown in full detail (`level_notes` in `src/menu_draw.c`). The stored numbers are 5 (DYNAMIC) and 1 (LOW).

**Props (trees, rocks, crates, scenery):**

| Tier | What it is | Source |
|---|---|---|
| Near mesh | the prop's own model, drawn inside OBJECT DISTANCE | `src/level.c` `object_reach` |
| Far-far cross | two textured quads, one 128 x 128 atlas (front and side), for trees | `cross_quads.pak`; `docs/archive/graphics/IMPOSTORS.md` |
| Far-far low poly | a hand-cut low-poly rock (a whole model of five permutations) | `models_x.pak` |
| XFAR light props | a light prop's own mesh | `src/level.c` `xfar_on` |

**Ships (Banshee, Ghost, Wraith, c_dropship, Pelican):** the near model, then a far card under 32 px (`ENEMY_CARD_PIXELS` in `src/level.c`). Enemy ships use map far, not OBJECT DISTANCE (section 4).

**Pickups** (weapons, ammo, health, grenades lying in the level): reach = 2 x the draw distance's model far, between 9 and 30 wu, capped at 50 wu (`PICKUP_REACH_SCALE`, `PICKUP_REACH_CAP` in `src/level.c`).

**Impostor cards** (one camera-facing quad per object): not used in gameplay, FIREFIGHT, OUTPOST or the viewer since 2026-10-29. Cutscene cards came back on 2026-10-30 for the cutscenes only (section 9).

## 4. Switch points

| What switches | Value | Source |
|---|---|---|
| Character near to far | the model's own pixel switch (OpenCE's), kept in the blob | `model_tier_far` in `src/model_tier.h` |
| Character far never used closer than | 12.2 wu (about 37 m) | `MODEL_TIER_FAR_MIN_WU` in `src/model_tier.h` |
| BALANCED near set | up to 2 near hostiles a frame, the aimed one included; a near one keeps its near body for 0.5 s more | `BALANCED_NEAR_MAX` 2 in `src/balanced_near.h` |
| SUPER XFAR under pixels | under 24 px projected | `MODEL_TIER_SXFAR_PIXELS` 24 in `src/model_tier.h` |
| SUPER XFAR for enemies past distance | past 55 wu (168 m) | `MODEL_TIER_SXFAR_ENEMY_WU` 55 |
| SUPER XFAR hold band | in at edge + 2 x hold, out at edge: never drawn above 24 px | `model_tier_sxfar_hold` |
| SUPER XFAR body size | at most 16 KB a row | `MODEL_SXFAR_MAX_BYTES` 16 KB |
| SUPER XFAR and far props RAM floor | loaded only while the heap keeps 512 KB free | `XFAR_RAM_MARGIN` 512 KB (`src/level.c`) |
| OBJECT DISTANCE (props, vehicles, friendly characters) | 12, 16, 24, 36, 55 wu = 37, 49, 73, 110, 168 m, then FAR (the far clip) | `video_object_distance` in `src/menu.c` |
| Far trees and rocks (FAR PROPS) | reach 50 wu = 152 m | `XFAR_WU` 50 in `src/level.c`; menu note "Trees and rocks stay to 152 m." |
| Enemy ship card | under 32 px diameter | `ENEMY_CARD_PIXELS` 32 |
| Enemy ship reach | map far (the fog), not OBJECT DISTANCE | `ENEMY_SHIP_MODELS` in `src/level.c`; tracker F-PELICAN-SHIP-REACH (#409) |
| Idle enemy reach | 80 wu (244 m), fully drawn at 68 wu (207 m) | `ENEMY_FADE_WU` 80 in `src/level.c`; `menu_enemy_fade_wu` in `src/menu.h` |
| Idle enemy sleeps | past 50 wu (152 m), not fighting | `AI_AWAKE_RANGE` 50 |
| Draw distance (world) | 30 to 100 % in steps of 5 %, plus DYNAMIC (50 to 90 %) and DYNAMIC (STRONG, 30 to 90 %) | `src/menu.h` `VIDEO_DRAW_*` |
| Detail maps | off, 9, 18, 27, 37, 49, 73 m | `docs/GAME_GUIDE.md` (DETAIL DISTANCE row) |

## 5. Fades

| What | Fade rule | Source |
|---|---|---|
| Enemy appears (campaign only) | fades in over 0.5 s on the translucent list; a new hostile starts at alpha 0 | `src/spawn_fade.h` (`SFADE_S` 0.5); tracker F-ENEMY-FADE-IN |
| Enemy idle, by distance | alpha 0 at 80 wu, 1 at 68 wu; the change is met at no more than 1 / 0.5 s a frame, so a state change still fades over half a second | `actor_veil_frame` in `src/level.c`; tracker F-ENEMY-FADE-80 (#265) |
| Enemy fighting | fades in over 0.5 s and stays until it dies or stops fighting | `actor_engaged`, `VEIL_FADE_S` 0.5 |
| Enemy corpse | fades out past the reach | `src/level.c` ENEMY HIDE comment |
| Prop past OBJECT DISTANCE | the last 15 % of the reach fades out (`REACH_FADE` 0.15) | `reach_fade` in `src/level.c` |
| Far prop (XFAR) | the far-far fades in as the near mesh fades out, over 85 to 100 % of OBJECT DISTANCE, and fades out over the last 15 % of 50 wu | `xfar_alpha` in `src/level.c` |
| Light beam (searchlight) | the beam part draws to 50 wu and fades over its last 15 %; the lamp body keeps OBJECT DISTANCE | tracker F-BEAM-FAR-408 (#408); `beam_only_m` in `src/level.c` |
| Character LOD tier change | no fade: hard swap | section 1, rule 6; `src/level.c` ENEMY HIDE comment ("LOD tiers swap hard") |
| Mesh swap (any) | no fade | owner rule (memory note 2026-11-15) |

Owner ruling (2026-11-15): props keep the far-far crossfade with their near mesh. Rule 6 (no fade between tiers) applies to characters only.

## 6. Enemy rules in the campaign (summary)

- Campaign only. Not in cutscenes, not in FIREFIGHT (its own reach rules), not in multiplayer.
- At every OBJECT DISTANCE setting, enemy reach is the same fixed 80 wu rule (tracker F-ENEMY-FADE-80, #265: "not OBJECT DISTANCE").
- Fighters, shooters, snipers and crews (in a vehicle) are not hidden by the distance rule.
- A hidden enemy is not dead: script counts, encounters and kills stay the same. The player's rays pass a hidden enemy (`actor_veiled`).

Debug sliders (dev images, INFO tab, session-only, not saved):

| Slider | Steps | Default (release rule) | Source |
|---|---|---|---|
| FAR ENEMY CAP | OFF, 4, 8, 12, 16, 32 most idle enemies drawn; the furthest go first | OFF | `menu_enemy_cap_value` in `src/menu.h` |
| ENEMY FADE START | 55, 68, 80, 100 wu = 168, 207, 244, 305 m | 80 wu | `menu_enemy_fade_wu` in `src/menu.h` |

## 7. The census rule

Owner rule (2026-11-15): every mob gets every tier on every map.

Method (from `docs/archive/graphics/LOD_CENSUS_376.md`):

1. Census: each character, each map, each tier: near, far, xfar, super xfar. Triangles and vertices of the worst permutation of the Performance blob.
2. Mark every gap: a far tier that is the same as the near tier, a missing SX body, a missing tier on a map.
3. Cut one character at a time. The far tier goes to at most 45 % of the near (triangles and vertices) where reachable.
4. Look verdict from the design owner on each cut (a side-by-side shot), then a code review against the engine source on the tier table.
5. Bake and boot. The emulator is not the hardware: only a real-console log proves a cost.

Sample of the census (CENSUS, Performance near and far, triangles / vertices; SX = super xfar triangles):

| Mob | Maps | Near | Far before | Far now | SX | State |
|---|---|---|---|---|---|---|
| grunt (1,173 placed) | 9 maps | 734 / 839 | 344 / 469 | 205 / 347 (28 % / 41 %) | 90 | landed |
| flood combat elite (654) | c20 c40 d40 | 538 / 541 | 536 / 501 | 250 / 351 | 106 | landed |
| jackal (457) | 7 maps | 597 / 832 | 425 / 657 | 268 / 362 (45 % / 43 %) | 106 | landed (hand cut A) |
| elite (395) | 7 maps | 591 / 881 | 394 / 849 | same (96 % of vertices) | 106 | parked: the tool floor is about 405 triangles |
| elite special (112) | 6 maps | 826 / 1,024 | 595 / 801 | 361 / 580 (44 % / 57 %) | 106 | landed |
| sentinel (143) | 4 maps | 923 / 1,466 | 577 / 907 | 345 / 573 (37 % / 39 %) | 83 | landed |
| hunter (55) | 6 maps | 1,204 / 1,551 | 2,011 / 2,170 (inverted) | 1,200 / 1,440 (100 % / 93 %) | 108 | landed; far is still 100 % of near triangles (no real cut) |

The census lists "inverted" when the far tier was bigger than the near tier before the cut. Those are the rows the rule was made for.

The Jackal numbers in `docs/PERF_NOTES.md` (near 525, far 349 drawn) are a drawn count. The census numbers (597 / 832 near) are the worst permutation of the Performance blob. Do not mix the two in one table.

## 8. Cost of a drawn character

| Item | Value | Basis |
|---|---|---|
| Model vertex cost | about 2.9 us per transformed vertex on a boot shot (19.7 ms for 6,718 vertices) | MEASURED, `docs/PERF_NOTES.md` model table (the firefight shot, the row with "(2.9)") |
| Model vertex cost, range | 2.4 to 3.2 us per vertex | MEASURED on the lifepod note and the emit note, `docs/PERF_NOTES.md` |
| One grunt, near (839 vertices) | 2.0 to 2.7 ms | ESTIMATE: 839 x 2.4 us and 839 x 3.2 us |
| One grunt, far (347 vertices) | 0.8 to 1.1 ms | ESTIMATE: 347 x 2.4 us and 347 x 3.2 us |
| Saving from the far cut, one grunt | 1.2 to 1.6 ms | ESTIMATE: near minus far |
| A dropship's near model, 655 to 2,956 triangles | about +6 ms per dropship on screen | `docs/PERF_NOTES.md` (dropship note); the source's own estimate, not re-measured |
| SUPER XFAR body, unused | about 8 KB a map for a cutscene-only character | CENSUS, the "no placed campaign actor" section |
| SUPER XFAR body, max | 16 KB a row | `MODEL_SXFAR_MAX_BYTES` |

Rule: every LOD change states its cost in KB (RAM and VRAM) and ms (frame time), and the poly count per screen after the change.

## 9. Impostors and cross quads (what replaced the cards)

- **Impostor cards** (one camera-facing 4-vertex quad) were used first. On 2026-10-29 the owner deprecated them everywhere in gameplay, FIREFIGHT, OUTPOST and the viewer. The code switch is `IMPOSTORS_DEPRECATED`. The bake scripts and `src/impostor.[ch]` stay, switched off (`docs/archive/graphics/IMPOSTORS.md`).
- **What replaced them:** the model's own far mesh (ships, characters, weapons), and for trees, rocks and plants the cross quads and the low-poly meshes (`cross_quads.pak`, `models_x.pak`).
- **Cutscenes got cards back** on 2026-10-30, only in baked scenes. Reason (MEASURED): the x10 bridge shot fell from a steady 30 fps to about 19 fps when its editor card rows became meshes (seven fighter bombers at 4,217 vertices each; confirmed on hardware, tracker F53).
- Held weapons stay meshes in cutscenes (`CINE_HELD_CARDS` 0).

Lesson: a card saves triangles, but in a cutscene with many large objects the mesh bill was the bigger cost. Test both in the scene that matters.

## 10. What Halo learned about the trade-offs

1. **Pop-in reads as unprofessional.** The owner's word. Fade the things that appear (enemies, props at their reach). Keep character tier swaps hard: the owner called a tier fade wasteful, since both tiers draw during it.
2. **A real far cut beats a smaller distance.** Where far equalled near (d40, c40, c20 in the census), the owner's rule made the cut real, not a later distance change.
3. **Count on screen, not per model.** One 1,000-triangle model is a sixth of the screen. The per-screen count decides, and a model's own cut is only one input.
4. **Use one rule per kind of object.** Characters use pixel switches (OpenCE's). Props use a distance (OBJECT DISTANCE). Ships use the map's fog distance. A beam draws its far part separately from its lamp.
5. **Limit the number of characters at the near tier.** Balanced draws at most two near hostiles a frame (BALANCED_NEAR_MAX). The rest use the far mesh. Cheap, and the aimed enemy stays near.
6. **Test with the console.** The emulator does not model the console's fill rate or its vertex cost. Only a real-console log is a number (dc-halo `docs/PERF_NOTES.md`).
7. **Cutscenes need their own budget.** Cutscene cards were restored after a 30 to 19 fps drop (section 9).
8. **Look sign-off on every cut.** A cut that looks wrong on screen is not a saving. The design owner's verdict comes before the commit.

## 11. Tools and files

- `tools/lod/README.md`: the tools note named in the Dreamcast table in `DREAMCAST_MAGIC.md`. dc-halo does not have this file at 2026-11-15 (checked). Each project that links it must have it too.
- dc-halo LOD tools: `tools/assets/convert_models.py` (character tiers, the far head), `tools/assets/bake_impostors.py` and `bake_scripted_impostors.py` (cards, switched off), `tools/assets/bake_limits.py`, `tools/assets/bake_ram.py`.
- Census: `docs/archive/graphics/LOD_CENSUS_376.md`.
- Impostor history: `docs/archive/graphics/IMPOSTORS.md`. Culling: `docs/archive/graphics/CULLING.md`.

## 12. Open items and checks

- Prop crossfade: owner ruled 2026-11-15, props keep it (section 5).
- The Jackal far head is said to switch at 50 px in `docs/PERF_NOTES.md` (the name `CHARACTER_FAR_HEAD` is not in `src/`). Not verified in code. Mark it as a note until a source line is found.
- `docs/GAME_GUIDE.md` (the presets table, the "Enemies far away" row) still says "past 41 m". The code uses 80 wu (244 m) for idle enemies at every setting. Fix the guide.
- The enemy-hide comment in `src/level.c` (ENEMY HIDE AND SLEEP) still says "not at OBJECT DISTANCE FAR". The #265 text in the same comment says the reach no longer follows OBJECT DISTANCE. Read both before you copy the rule.

## Sources

- `src/model_tier.h`, `src/balanced_near.h`, `src/spawn_fade.h`, `src/xxfar_band.h` (dc-halo)
- `src/level.c`: ENEMY HIDE AND SLEEP, `actor_veil_frame`, `xfar_on`, `xfar_alpha`, `reach_fade`, `object_reach`, `ENEMY_SHIP_MODELS`, `ENEMY_CARD_PIXELS`, beam code (#408), pickup reach (dc-halo)
- `src/menu.c` (`video_object_distance`, `video_presets`), `src/menu.h` (`VIDEO_*`, `MENU_ENEMY_*`), `src/menu_draw.c` (row notes) (dc-halo)
- `docs/archive/graphics/LOD_CENSUS_376.md`, `docs/archive/graphics/IMPOSTORS.md`, `docs/archive/graphics/CULLING.md` (dc-halo)
- `docs/PERF_NOTES.md` (Jackal section, dropship note, model cost table) (dc-halo)
- `docs/GAME_GUIDE.md` (Video settings, Enemies far away, Object distance rows) (dc-halo)
- `docs/plans/FEATURE_GAPS.md` rows F-ENEMY-FADE-80 (#265), F-PELICAN-SHIP-REACH (#409), F-LIGHT-BEAM-FAR (#408), F53 (dc-halo)
