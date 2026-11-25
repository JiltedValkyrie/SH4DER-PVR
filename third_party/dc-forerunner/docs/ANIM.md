# Animation lessons for a Dreamcast port

Source: lessons from the Halo CE Dreamcast port (dc-halo, KallistiOS, SH-4). `#N` = dc-halo GitHub issue. Hashes = dc-halo commits. Written for any port with skeletal characters, cutscenes and LOD tiers.

Dreamcast budget used below: 16 MB main RAM, 8 MB VRAM, 2 MB AICA sound RAM, SH-4 with SIMD (SH4ZAM), PVR for drawing.

Numbers are measured on dc-halo builds unless marked **ESTIMATE** (calculated from a measured number) or **guess** (no evidence yet). Source-game facts (your own game, its engine and its files) are marked **verify** where they are not yet checked against your game files.

---

## Rules that saved us

1. Measure first. Write the cost (KB of RAM, ms of frame, VRAM, audio chip) in the change note before any code.
2. Never drop data silently. A size cap, a skipped clip or a failed scene load prints one line and keeps a count (#107).
3. Every bake that writes an animation pak gets a test that counts the clips in the baked data (#334).
4. Key attachments, effects and held props by actor instance, never by model (#96, `488937dd`).
5. Check the world-relative flag and the net turn of every clip before "fixing" an actor that floats, sinks or faces the wrong way (#288, #105, #56, #144).
6. Cut a clip only after proof the game never plays it. A cut that was wrong got reversed (`4b9d7a39`).
7. Census every character on every map and every LOD tier. Far is sometimes bigger than near (#376, F-LOD-ALL-TIERS).
8. Fade only on appearance. Never fade a mesh swap or a cutscene actor.
9. Cutscenes get their own memory line. Never borrow gameplay RAM without a number.
10. A fix is not done until it is rebaked and the owner has booted it. Log it as CODED (need rebake / boot) until then.

---

## Compact animation data

The owner calls this the biggest single animation saving. The Halo numbers below are measured on the shipped bake unless marked.

- **Lean player body graph** (Halo commit 1e242490). Bake only the clips the port actually plays. The Chief graph went from 760 KB to 215 to 390 KB per map. Test 7c23854a pins that every seat clip survives on all nine trimmed maps. MEASURED.
- **Anim diet per map** (Halo commit cff30f6c: DIET_MAPS, reader_keep narrowing in convert_anims.py, HELD_ACTIONS kept). Keep only the clips that each actor reader on that map can play. b40 anims.pak went down 508 KB, and the kept clips are byte-identical. MEASURED.
- **Byte-equal clip sharing** (Halo commits 6865557b and b1403667: share_equal_clips, and check_shared_clips as a self-proof). Identical clip data is stored once, and rows share the offset. a30 shipped 35.5 KB less. MEASURED. An earlier figure of 72.7 KB came from a scratch bake made with a wrong flag. Always measure the shipped bake.
- **Like actors share one graph** by a bone-name rig map (DCAR, retarget by node name). Example: extra marines run on the Chief's graph.

Rule for your game: one animation set per skeleton family (GUESS: check the families against your own skeletons; they are not yet checked). Every actor of a family shares it. Bake per area only the clips that area's actors can play. Dedupe identical clips. Prove each bake with a clip-count test.

## 1. Skeletal animation packing (anims.pak)

What it is (dc-halo `docs/archive/reference/ASSETS.md`):
- One `anims.pak` per level. Blobs keyed by the animation graph tag (`DCN2`: node tree, clips, first-person slot table).
- Side blobs per character: `DCAR` rig map (model node -> graph node, by name), `DCDA` death clips, `DCFA` fire clips, `DCWC` weapon-class stance clips, `DCMA` mouth clip, `DCVS` vehicle seat graphs.
- Runtime graph reader: `src/anim.c`. Skin matrix = graph matrix of the mapped node times inverse bind.

Numbers:
- b30 `anims.pak`: 3,432,300 B -> 2,664,748 B after narrowing (-750 KB, F-HEAP-DIET). a30: 2,877,708 B -> 2,507,884 B (-361 KB).
- The Chief body alone is 843,564 B resident (56,656 B model + 778,168 B graph with 195 clips + small blobs). That is 824 KB of the player-body budget.
- Death clips: 2.7-11.4 KB per biped. Missing key = old pose death.

Lessons:
- **What went wrong:** the first narrowing kept only clips a hand list named. The elite sword surprise, dive and evade clips were cut for b30 (167 KB saved). Melee clips were then restored for a30 (+14.3 KB) after a missing swing was found (2026-11-01 fix). Later the evade and dive set was found resident anyway, and the cut was reversed (`4b9d7a39`).
- **Fix:** the keep list comes from the code. A test reads the clip names the source references (`test_clips_the_source_names_stay`). Each rebake prints the clip list diff per map.
- **Rule for your game:** before cutting any clip, grep the scripts and AI for its name. Record the cut with KB per map.
- **What went wrong (stale data):** #334. Female Outpost bodies played idle only, twice. The stale check did not rebuild the body pak. The old file had 35 extra clips (101 KB) missing.
- **Fix:** `0b628ce6` puts the body pak in the stale check. The current file is 152,184 B with 50 clips.
- **Rule for your game:** every body pak has a test that pins the clip count. The boot log prints the count. A stale file must fail the build, not ship.

Compressed graphs: the runtime skips cinematic graphs that are compressed (ASSETS.md). Keep cinematic and gameplay graphs in separate formats.

## 2. Skinning cost on SH-4 and bone counts

Numbers:
- Model vertices cost **2.0-3.2 us per transformed vertex** (PERF_NOTES, the P2 boots). The vertex loop is about 75% of model time.
- First-person arm rig: 1,956 vertices = 4.2-4.4 ms per frame.
- d40 boot (#378, PARITYTEST): frame 37.7 ms (target 33.3). CPU ~100%, GPU 14-20%. Models 11.2 ms, BSP 9.6 ms, vertex loop 6.7 ms, triangle loop 5.4 ms, 3,445 triangles in 17 parts. Skinned vertices are the frame cost.
- Triangle budget on the Dreamcast: about 6,000 on screen for 30 fps (project rule).
- **ESTIMATE:** a 5,000-vertex character at full detail costs 10-16 ms per frame on SH-4 from the numbers above. Measure your rigs before trusting this.

Rules:
- **Pose once, skin once per frame.** Halo poses in both the opaque and translucent pass. The pose-once plan is in `docs/plans/PERF_PLAN.md` (phase 9). Plan for it from the start.
- **Influences:** Halo skins with 2 bones per vertex (node-pair runs). The cost of 3-4 influences is **guess**: not measured. Default to 2. Cap the rest at bake.
- **Bone cap:** the runtime holds `ANIM_MAX_NODES` = 64 nodes (`src/anim.h`). The Chief rig has 19 nodes. Humanoid skeletons in other engines can be larger (**guess**: tens of bones; count them per character).
- **Prune at bake:** drop every bone with no vertex weight in that LOD. Keep deform bones only. Halo's rig map is built by name, so pruned bones need a name map.
- **Posed-node cull:** Halo tests each part's node spheres against the frustum after posing. Node radius is about 0.08 wu (median) against a model radius of 0.64-0.79 wu, so the test is tight. Off-screen crowd no longer pays the vertex loop.
- **Math:** matrix work goes through SH4ZAM (`shz_xmtrx_load_apply_store_3x4` in `anim_mat_mul`). Do not hand-write matrix math.
- **Submit:** big meshes go through the KOS direct-render / store-queue path, not per-vertex submit. Name the path in the review.

## 3. Animation graphs and clip playback

- A character is one graph: node tree plus clips, with a state (idle, walk, move back / left / right, stance per weapon class).
- **Cross-fade:** a new clip blends from the pose it replaces over 6 ticks (`f0412259`, OpenCE `units.c:4146`, the Halo reference source). Use the same blend for cutscenes.
- **Walk clip from throttle, not from state.** Halo picks the walk clip from `a->moving`. A FIREFIGHT body with no walk clip slid (#42).
- **Directional clips** (back, left, right) are found by name per weapon class. Check each class has them. A missing one falls back to the wrong pose.
- **World-relative clips:** some clips carry a flag (Halo: antr flag bit 1, DCN2 spare `0x4000`) that puts node 0 in world space.
  - #288: the d20 captain idle was not world relative. The captain drew 23 wu off and floated. The Chief sank 1.4 wu into a ramp. Fix `4a0df144`.
  - #105 and #56: the c20 Chief drew about 18 wu off until clips were played at their world pose (`4d364048`).
  - Rule: when an actor floats or sinks, check the clip flag before anything else. The bake must carry the flag.
- **Net turn at clip end (#144):** the clip turned the body 165 deg inside the clip, but the object kept its recorded facing (-89 deg). Fix: the bake writes the clip's net node-0 turn into the idle that follows (`8ac39c52`, only for turns of 30 deg or more).
  - The owner's reference footage showed no turn at the clip end, so this is tracked as a DEPARTURE, not a settled fact.
  - Rule: check reference footage before writing the fix.
- **Idle snapping (#281, OPEN):** in cutscenes, idle characters snap between two keyframes instead of breathing. Not traced yet.
  - Candidates: decimation at bake, a 30 Hz clock cut, or a clip-blend bug. Each is still a guess.
  - Rule for your game: a test per idle clip: frame count, key times, loop closes. Look at it in the editor before the first boot.

## 4. Cutscene and cinematic playback

Halo design: OpenCE scenario scripts are compiled by `build_cinematics.py` into one `cinematics.bin` per map. It holds camera keys, actor create / move / animate / attach / detach ops, sound and device events. Each scene is baked; the runtime plays the bake.

Lessons:
- **Attachments on node 18 (#167, closed):** on the source platform the Chief holds his rifle from t0 (ATTACH to node 18). The port showed no rifle. The gap was seen only in an older half of the owner's video. Closed after the owner's boot (v0.82 b2, PASS).
  - Rule: each attach row carries its node (name and index) and its start tick. Check the first frame of every scene against the reference.
- **Attachments keyed by model (`488937dd`, F-CINE-HELD-FIRE):** `att_cinematic` applied every model-keyed template to each cutscene actor of that model. Held rifles, weapon actors and a scenery shotgun got effects. The rifles fired through every cutscene.
  - Rule: key effect templates by instance (actor slot), not by model.
- **Teleport (#96, #106):** the c10 end teleport had four separate causes. Fixing one did not fix the others.
  1. The teleport effect gated on the device position, which stayed at 0 in the cutscene (`1a1d79ce`).
  2. The Chief's scale snapped to 0.1 in one tick. OpenCE ramps drawn nodes over 15 ticks (`object_start_interpolation`, `objects.c:1490`). Ramp added in `52b249b9`.
  3. The held rifle card ignored the placement scale: a full-size rifle sat at the shrunk hand near the floor until its destroy tick (t687).
  4. The teleport sound was in a streamed pak that the attachment code could not play (fixed `52b249b9`).
  - Templates matched by model: teleport tag 1551 and five emitter scenery (model 1776) all ran. Fixed by instance.
  - #106: the effect appeared before the character. Check create and destroy ticks against the character's own create tick.
  - Rings baked in cutscenes: `3db4f3a5`.
  - Cost: campaign `level.o` +132 B text, +288 B bss. `cinematic.o` +252 B text, +2048 B bss. VRAM 0. AICA 0.
  - Rule: a teleport is four things: effect, scale ramp, held weapon, sound. Check all four in every scene that teleports.
- **Interpolation class:** only some OpenCE object types ramp their drawn nodes (projectiles, scenery, machines, controls and placeholders do not). Check the class before adding a ramp.
- **Attach and detach set the pose in one tick (F-X70-SEAT, `f0585491`).** The d40 finale Chief was 555 wu off after boarding because a reposition at tick 210 was lost. Match OpenCE `object_detach`.
- **Device-driven machines (#143, `e04ab585`):** a baked scene bound only one machine row to its device. A scene-created door drew its rest pose, so it never opened. Bind every scene-created machine to its device, and log the count.
- **Screen filters (X1):** desaturation, blur and tint. 77 rows over 13 scenes were MISSING for a long time. Cosmetic. Last in priority.
- **Live path and baked path: one checklist row each** (Cortana, #397 fix 9a050e3d). Subtitles worked in play and never showed in the replay theatre, because the baked scene player neither opened the blob nor ticked it. For each presentation feature (subtitles, grade, lights) keep a row for the live path and one for the baked path, each with a test.
- **Apply the baked scene's end state before gameplay runs** (Senior, #407: the d40 lose scene baked game_lost and unit_suspended). A baked cutscene that swallows script ops leaves the player unsuspended outside the new area, and the player falls. After a baked scene, apply its end state (suspend, fade, lost or won) before gameplay ticks once.
- **Quote scene ticks as ticks** (Scholar, #402). The docs convert ticks to m:ss at about 30 ticks per second. That is an estimate, not checked against the audio clip, until a boot confirms it.

## 5. Cutscene memory budget (kept separate from gameplay)

- **Scene graphs load per scene.** The cap in `open_scene_animations` was 512 KB per entry. A c40 scene entry (the Spark graph, 576,944 B) broke the cap. The whole scene index aborted, so every actor fell to the stance idle (#107, `64c6613b`).
  - Fix: cap raised to 1 MB, with a log line. Cost: RAM +824 KB during the c40 insertion only (scene free about 6.5 MB). VRAM 0.
  - Rule: a size cap must log, and must never abort the whole index for one entry.
- **Preset rule (Balanced, `a8c83b11`):** the 512 KB cutscene allowance is kept for main characters only (Chief, Cortana, Keyes, Johnson). The commit does not say what the other characters get.
- **Cutscene dynamic LOD (#337, F-CINE-DYN-337):** steps happen only at a camera cut, never mid-shot. Floors: 70% in-level, 80% baked (`5df9c37e`). At 70% the far mesh reached marines close enough to read hands and helmets.
- **Gate:** gameplay must keep 2,048 KB free after load (Halo's gate). HEAP after load on the PARITYTEST b25 boot (emulator, not hardware): b30 2,123 KB cold, a30 2,387 KB cold, a50 3,157 KB, d40 4,534 KB. c40 3,767 KB was read after its opening scene, so it is not a cold number.
- **Rule for your game:** one cutscene budget line per scene in KB (actors, anims, cameras, audio, VRAM). Check it in a test. A scene that needs more RAM than gameplay has spare must say so in its row before it is baked.

## 6. Cutscene parity table

- Halo keeps `docs/archive/campaign/CUTSCENE_PARITY_TABLE.md`: one row per scene, with OpenCE counts (actors, anims, cams, shake, fx, sfx, bsp, filters, faces, vehicles, devices, lights) against the port count.
- Status per row: CONFIRMED, DEPARTURE, HYPOTHESIS, MISSING. Priority P0 = reads broken on the first boot.
- Example: c10 x50 has 52 actors, 52 anims, 37 cameras and 61 sound events in OpenCE. The port has 598, 43, 33 and 20 in its C / D / H / M columns. The table does not define those column letters. Define them before you use it.
- Cross-cutting gaps (fix once, hit every scene): faces (about 55 rows MISSING), screen filters (77 rows), voice silent on disc 2 until its bake (X11), vehicles driven offline (HYPOTHESIS).
- **Rule for your game:** start the table on day one. Every scene is a row. Every row has an owner, a status and a boot check.

## 7. Editor camera tools

- Halo map editor, Cameras tab (#319, #324, #325): edit each key's position, orientation and angles. "Pilot" looks through the key's own pose. Start-frame and End-frame buttons. Triangle and vertex columns per shot.
- Cost overlay (`6fe387a7`): shows each shot's cost before the bake. Mark its numbers as estimates.
- CINE VIEW row (`098fccd5`): one serial row per camera cut, PARITYTEST only. Costs 132 B of text. It gave the editor's triangle calibration.
- **Camera-critical rule** (`docs/archive/campaign/CAMERA_CRITICAL.md`): on framing-critical scenes only distance, height and move speed may change. The subject, the eye line and the reveal point stay on the same screen point. The bake refuses a camera edit on a critical row without an owner call (`camera_needs_lead`). Example: #37 was cleared by hand (`f0c00dc4`).
- **Saved editor profile:** the owner's saved profile becomes the recommended default (`cc5712a3`). Keep the rule: the owner's saved file wins, with a DEPARTURE row.
- **Actor offset range:** the editor offset was ±100 wu. An actor placed 22 wu away could not be fixed. It is now ±500 wu (F-OFFSET-RANGE-288, DEPARTURE).
- **Rule for your game:** build the camera tab, the cost overlay and one serial row per camera cut before the first cutscene. Without them every camera fix is a guess.

## 8. Character LOD and fades

Draw distances (`video_object_distance`): {12, 16, 24, 36, 55, far} wu. Presets: Performance 24 wu, Balanced 36 wu, Fidelity 55 wu, Xtreme = far clip. Model far at 15 wu. Posed animation far at 22 wu (`MODEL_ANIM_FAR`).

Lessons:
- **Pose every drawn character.** Allies beyond 22 wu were drawn in the bind pose (a T-pose) until the posed path covered them. Pose is cheap compared to the wrong look.
- **Cut every tier, measure every cut.** Measured cuts (triangles / vertices):
  - Crewman far: 955 -> 298 triangles (31% of near, 56% of vertices). Blob -12.2 KB per map (`40b67a5c`).
  - Elite special: near 826 / 1,024 -> super xfar 106 triangles (`bf9a97bb`).
  - Marine armored super xfar: 88 triangles, +7 KB in `models_x.pak` (`bf9a97bb`).
  - Captain (ingame): super xfar 88 triangles, +8.0 KB on a50 (`4154535b`).
  - Flood captain far: 2,650 -> 1,294 triangles (#328).
- **Census gaps (F-LOD-ALL-TIERS):** far is bigger than near on hunter, pilot, captain and fatigues. Far equals near on crewman, engineer, monitor and the Flood carrier on several maps (#376, the Flood far at 1,024 triangles = the near mesh on d40).
- **Super xfar shapes under about 30 px read as rough shapes (#265).** Test a silhouette at the real screen size before you ship it.
- **Hand cuts:** a tool-made Jackal far cut (281 triangles) was refused: its armored helmet read as a skin-textured lump. A hand cut was accepted. Keep both options in the census.
- **Rule for LOD:** every LOD object is a committed file or a committed script (#156, owner rule). No untracked far files.
- **Enemy fade-in only (`45005be3`, F-ENEMY-FADE-IN):** a new hostile free-standing actor fades from alpha 0 to 1 over 0.5 s, campaign only. Not in cutscenes, FIREFIGHT, Outpost, for allies or in matches. Cost: RAM 120 B, VRAM 0, **ESTIMATE** +0.1-0.25 ms per fading enemy for 0.5 s. Design check: limbs overlapping at alpha 0.2-0.6 can read as a ghost. Review it on a boot.
- **Never fade a mesh swap.** A LOD change that fades reads as two characters. Swap at a cut or at distance with no fade.
- **Dynamic LOD never changes mid-shot.** See section 5.

## 9. Death and corpse animation

- Each biped has baked death clips (`DCDA`). A missing key falls back to the old pose death.
- Corpse lifetime (`level.c corpse_fade_alpha`): hold the last frame 30 ticks (1 s), fade over 15 ticks (0.5 s) on the translucent list, then leave the corpse pool.
- A body costs what a live actor costs to draw (skinned vertices). The port is CPU bound, so the corpse pool needs a cap and the fade.
- **Corpse sink (#164, OPEN):** in MP a dead body sank partly into the floor. The port drew every body level.
  - Fix `4920f0c8`: each tick the body tilts onto the ground normal (OpenCE `bipeds.c:3172-3200`). The minimum upright test is the biped's `minimum_normal_k` = 0.7071, not 0.5.
  - NaN normals: `36eae387` and the b10 fix. Under fast-math a NaN passes the `!(x >= y)` idiom. Use an explicit finite check.
  - Still OPEN after three commits. Needs a boot with a dead body on a slope.
- **Dropped weapons** are separate objects and stay on the floor. A revert left a dropped weapon floating (#37, closed): stale pickup slot IDs survived a revert. Clear pickup physics on every load.
- **Ragdoll:** dc-halo has no ragdoll. **guess:** too costly on SH-4 for crowds. Check what your game's death does before you decide.
- **Rule for your game:** death = baked clip + ground-normal snap + fade + pool cap. Test the snap with a NaN normal and a steep slope.

## 10. Held props and first-person arms

- Held skull (#39, #331, several commits and boots): the first version drew only the prop, with no arms and no swing.
  - Fix: an oddball arm set in `anims.pak` (5 clips, 2 rig records). Cost: +13.9 KB per map (`04cd92ea`). About 23.5 KB RAM on skull maps (**ESTIMATE**, `1edd3266`).
  - Hold span 96 px -> 140 px after the design call on the look (`71435c12`).
  - Fallback: with no arm slot, the old grip is used.
- **Rule for your game:** a held item that needs its own arm pose is a separate arm graph in the pak. Budget it in KB at the start.

## 11. Facial animation for conversations

Halo:
- Lip sync is one mouth clip per character (`DCMA`, 8 B, one frame, slot 10 of the stand seat). Fallback: none (-1).
- **Bug:** the one-frame mouth clip lost its node flags. The generic bake rule removed every node that does not change between frames, so the mouth never moved. Fix: `bake_values tag_flag_clips` keeps the tag's own flags (`tests/test_anim_mouth.py`).
- Faces (`unit_set_emotion`) were MISSING for about 55 dialogue rows (x10 5, x20 11, a50 rescue 3, b40 extraction 9, c40 insertion 13, d40 bridge 11). Neutral faces read as "AI" to the design review.
- Diagnostic: one CINE MOUTH row per dialogue line (48 rows per scene, SERIAL_VERBOSE only, 954 B of text).

Your game:
- Check what lip-sync data your source game's conversations use (**verify**).
- **Budget it:** facial bones are separate from body bones. Use head-only bones for dialogue (**guess**: 12-20 bones) and bake the face per line.
- **ESTIMATE:** a head of 1,500 vertices at 2.0-3.2 us per vertex costs 3-5 ms per character per frame at full detail. Two faces in a conversation is 6-10 ms. Reduce head vertices in dialogue or bake to fewer bones.
- **Rule:** one test that a mouth clip moves (any non-zero change per frame). One diagnostic row per line.

## 12. What cost hours

- **Stale baked data (#334):** the same regression came back twice. A rebake step ran without the body pak in the stale check.
- **Silent cap (#107):** one size check aborted the whole scene index. Every actor in the c40 insertion went to idle, and it took the owner's video to see it.
- **Model-keyed templates (`488937dd`, #96):** every held rifle fired in cutscenes; teleport had four causes.
- **World-relative flag (#288, #105, #56):** a floating character looked like a placement bug for several boots.
- **Fixes waiting on a rebake and a boot:** many tracker rows say "need rebake + boot". Each one is a fix that is not yet proven.
- **Reference footage:** #144 was argued from video. A turn that the video did not show became a DEPARTURE.
- **Held prop arms (#331):** several commits and boots, plus a look call on the size.
- **Corpse sink (#164):** three commits, still open.
- **Idle snapping (#281):** open, not traced, visible in every cutscene.
- **Rule:** one row per animation bug, with the reference clip, the bake step and the boot check named before the code is written.

---

## Source-engine checklist

Verify each item against your own game files. Halo numbers above are not your numbers.

- **Asset chain:** most engines split a character into a skinned mesh, an animation set (the list of its clips) and an animation tree (blend nodes). **verify** the names and the split in your source format.
- **Do not port the animation tree at runtime.** Bake each character's tree into a small state machine with a clip per state, like Halo's graph. The tree is a design tool, not a runtime cost on SH-4.
- **Cinematic sequencer:** tracks for camera movement, animation control (play a clip on an actor), events, sound and cuts. Bake each sequence to one scene timeline, like Halo's `cinematics.bin`: camera keys, poses, attach / detach, sound, events.
- **Camera cuts** are the framing check points. Keep the camera-critical rule (section 7) from the first scene.
- **Attachments:** weapons in hand and any prop attach by node, with a start tick. Check against reference footage (section 4).
- **Facial:** budget per line (section 11).
- **Bone count:** count the bones of every character before you choose a cap. Halo's cap is 64. The Chief rig is 19.
- **Influences:** cap at 2 per vertex. Measure before you allow 3 or 4 (section 2).
- **Death:** baked death clips with a ground snap and a fade (section 9). Ragdoll is **guess**: too costly for crowds.
- **Crowds:** the far tiers are a census job from day one (section 8). Expect far to be bigger than near somewhere.
- **Budgets:** one line per scene in KB (section 5). One line per character in triangles and vertices (section 8).
- **Editor:** the camera tab, the cost overlay and the per-cut serial row come before the first cutscene (section 7).

## Open questions for the new port

- Measure the skinned-vertex cost on a real character from your game (section 2 ESTIMATE). Choose the LOD tiers from that number.
- Count the bones and the influences on every character. Decide the cap before the first rig is baked.
- Decide the death approach (baked clip versus ragdoll) with a cost number.
- Confirm the facial system and its bone count in your files (section 11).
- Define the cutscene budget per scene (section 5) before the first sequence is baked.
