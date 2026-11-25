# Scripting lessons for a Dreamcast port of <Game>

Source: the Halo CE Dreamcast port (repo `dc-halo`), history up to 2026-11-15. Read-only research; nothing in `dc-halo` was changed. The lessons and numbers below are Halo's. They are kept with their labels because they are the best evidence we have. Where a number is yours to measure, the line says `<your number>`.

Tags used below:
- **GUESS**: a claim about <Game> or Dreamcast that nobody has checked yet. Verify before use.
- **ESTIMATE**: a number from a plan or a host test, not measured on hardware for this exact case.
- `#NNN` = `dc-halo` GitHub issue. `hash` = `dc-halo` commit. Both are history, not facts about <Game>.
- Role names (Senior, Parity, Scholar, Junior) are the review roles used by the Halo team.
- Dialogue, voice and subtitles are in [DIALOGUE.md](DIALOGUE.md). This file is about the rest of the script.

## Rules that saved us

1. Trace the original function and its caller before you change behaviour. Write the source address or path in the commit. (#114, #391, #402, #403.)
2. The converter fails loud. Any script node it cannot compile is a bake error, or a logged skip that a person reviews every build. A silent no-op note is not a pass. (#272, #191.)
3. Every difficulty branch is compiled as a branch. Never copy the Normal branch to all difficulties. (#402, #403.)
4. A save is a delta from the level start, keyed by stable names, and it has one version number. Bumping the version refuses every older save, so prefer a resume rule over a bump. (#391 R2, #384.)
5. No VMU write at a checkpoint. Checkpoints live in RAM; the card is written by an explicit save. (#79, 2026-11-07 call.)
6. Add a state row (serial, once per event) before you fix a device bug. Close the issue on the boot, not on the code. (#86, #87.)
7. Never remove or "tidy" an object that a script creates. The script decides what exists. (#114.)
8. One bake chain runs as one step. Running one step alone made a whole image unusable. (Halo b10 images, 2026-11-10.)
9. Test pins check counts, sizes and package hashes, not source text. Re-pin with a note naming the commit that moved the number. (24 re-pin commits.)

## 1. Source of truth and tracing

**Halo fact.** The truth source was OpenCE (the open Halo CE source), and the Halo rule was: trace the OpenCE function and its caller, then cite the path in the commit or doc. The port broke when a fix copied the function but not its caller.

**Your source.** The truth source is the original game data from your <source disc> and the original binary, read with your own tools (your reader writes into `SOURCE_EXTRACT`). A reading aid from another port may help, but it is never the truth source. The same rule holds: trace the original function and its caller first, then change.

- #114 (elevator sealed shut, closed 2026-11-15). A departure made in 10-29 emptied `START_PLACED_OBJECTS`. That made an invisible 1.3 wu blast panel solid across the elevator doorway. The squad was sealed in, the player was sealed out, and no shot passed. The fix went back to the original: the script (`enc5_3`) creates the panel when all players are in the cab volume (`1d508906`). Lesson: the script placed the object, not the level loader. Trace the caller.
- #402 and #403 (open): Sgt Johnson's Legendary-only speech never played. The original branches on the difficulty in the script; the port compiled the Normal branch. Lesson: a per-difficulty line is a branch in the original script. Find the branch before you find the audio.
- Rule: write the original address (or function name in the source binary) in the commit message and in the doc row. Mark the evidence: confirmed (read in the source), hypothesis (not traced), adaptation (our change).
- **A bug may be the original's behaviour** (Parity, #387 sentinel pass-through flags, #391 checkpoint give-up rule). Halo's "bugs" were often OpenCE behaviour the port had not traced. Before a change to gameplay, AI, collision, save or audio, trace the function and its caller, and write the path in the commit.

## 2. Mission script conversion (bake time, not runtime)

**Halo pipeline (history).** `tools/assets/convert_campaign.py` is a generic compiler. One module per map (`tools/assets/campaign_maps/`) says what is map-specific. It writes a `.dccp` package: tracks, instructions, predicates, globals, spawns, doors, groups, waypoints. `src/campaign.c` runs that package as a small VM at runtime. No script interpretation happens on the Dreamcast.

**Your rule.** Do the same. Convert your <source script system> sequences to a small VM at bake time. Do not interpret source script objects at runtime: there is no reflection budget in 16 MB RAM (ESTIMATE on the budget; the rule is from the Halo port).

Hard caps in Halo (`src/campaign.h`), for sizing. Fill in the last column for your game:

| Limit | Halo value | Note | Yours |
| --- | ---: | --- | ---: |
| tracks | 192 | b40 compiles to 170, a50 to 139 | <your number> |
| begin_random sites | 64 | 16 children each | <your number> |
| encounters | 96 | c40 has 67, c20 81 | <your number> |
| objects | 640 | a10 needs 522 | <your number> |
| device groups | 32 | a50 has 26 | <your number> |
| spawns | 1280 | c10 has 1249 | <your number> |

Caps are checked in the converter. Halo raised them with a version bump each time a map needed more. A cap raised late costs a save version (see section 5).

**Strict or skip-and-log.** Halo's a10 is STRICT: an unsupported operation fails the conversion. Other maps (a30 first) use skip-and-log: an unsupported action emits nothing, an unsupported predicate emits one constant, and every skip becomes a row in `runtime_audit.json`.

- #272 (found by the owner's boot 2026-11-11). Wraiths were placed but empty and never fired. The converter lowered `vehicle_load_magic` for a squad into a Wraith or Ghost into a no-op note: "Omitted staging / presentation". Fix: a load_squad event with the seat mask (`851b83c3`). Lesson: the note was the bug. The count was in the audit file and nobody read it.
- #191 (`67dc3ba3`). 652 `sound_impulse_start` calls named a source object. The converter dropped the source, so every impulse sound played centred. Fix: the source is packed into the dialogue event. 97 lines are now sourced. Cost RAM +256 B, text +504 B (ESTIMATE from the commit).
- F-X1-SCREEN-FX (tracker). Cutscene screen filters were MISSING in every scene that called them (about 60 events). Same family: an op with no compile rule and no error.

Rules for your port:
- STRICT by default. Skip-and-log only for a named list of ops, each with its own row.
- Every skip is printed once per bake and counted in an audit file. A build is not reviewed until the audit is read (Halo: `runtime_audit.json`).
- Every argument of an op reaches the package, or the bake fails. A dropped argument is a bug, not a simplification.

**Package key.** Halo's package has a layout id: a hash of the package with duration fields zeroed. A save indexes spawns, tracks, doors, objects, globals and conversations, so any change to those changes the key and refuses old saves (`layout_trailer` in `convert_campaign.py`). A sound length change does not change the key (ESTIMATE of the effect: none, by design). The key is kept separate from the file hash, so a rebuild with new sound lengths does not throw saves away (Senior, #391 layout_trailer).

## 3. Triggers, volumes and waits

- Halo waits are `sleep_until` on a predicate. Predicates are built from volume, living count, device position, difficulty, BSP (Halo's level-geometry section; use your own term), and similar leaves.
- In skip-and-log mode a skipped predicate becomes a constant: 1 under an even `not` depth, 0 under an odd one. The converter then lists every wait that depends only on skipped predicates (`gated_waits`) with `deadlock_risk`.
- A playthrough walker (`tests/campaign_playthrough_test.c`, Halo) walks each map's scripts with default answers. When stalled 150 ticks it satisfies blocked leaves (volume: move the player there; living: kill the set; device: set it). It is a static pass, not proof of play.
- Skip windows: a cutscene is skippable only where the original's skip window wraps it. d20 and c20 were locked wrongly because the row looked at the scene, not its caller. The original wraps `cinematic_intro` in `cinematic_skip_start` (`d85151b9`). Rule: find the wrapper, not the scene.
- Scene-level difficulty: a cutscene scene can carry a difficulty tuple; the compiler emits a `branch` on `difficulty` before its `movie` event (`convert_campaign.py movie_events`). d40's x70 has two scenes for this.
- **Port-only safety kills stand down when the script holds the player** (Senior, #290 rule vs #407). The stranded and out-of-map kills must not fire while the script has the player suspended, the mission lost, or a cutscene running. Every port-only rule lists the script states it ignores, and gets a test in each.
- **Count the overlaps in the spawn data before blaming AI** (Junior, #387). 74 overlapping sentinel pairs sat in authored starting locations (c40 e59_b alone had 57). The original engine fixes it on tick one. The port's pair-separation pass costs 0.02 to 0.05 ms with 8 to 12 alive (see PERFORMANCE.md section 6).

## 4. Difficulty branches and difficulty-only content (#402, #403)

What the Halo original does (history, from OpenCE):
- `game_difficulty_get_real` returns the real setting, Easy included.
- `game_difficulty_get` never returns Easy. It returns Normal (the function's own comment says it lies on purpose). The converter maps the two to different ops. Do not merge them.
- Squad counts per difficulty: Easy and Normal use the minimum, Heroic uses the middle, Legendary uses the maximum (`encounter_create`, `encounters.c:2205`).

Halo port numbers (actors placed, all squads, `CAMPAIGN_RUNTIME.md`):

| map | before (all difficulties) | Easy / Normal | Heroic | Legendary |
| --- | ---: | ---: | ---: | ---: |
| a10 | 233 | 210 | 193 | 213 |
| a50 | 689 | 467 | 457 | 462 |
| b40 | 628 | 523 | 553 | 625 |

Before the fix the port placed one count on every difficulty. Easy and Normal had too many enemies on most maps, Legendary had too few. The fix made each squad row carry a skip mask per difficulty (`SPAWN_SKIP_SHIFT` bits 3..6). Your table: `<your number>` per map and per difficulty.

Rules for your port:
- Read the difficulty enum from your source data, not from Halo's names. Do not assume your difficulty names or count match Halo's.
- Each difficulty branch is compiled as its own branch. A line or a spawn that exists only on the hardest difficulty must not be lost on the normal one. Lines on a branch that the player never hears are a count problem first (DIALOGUE.md, rule 8).
- Add a test per difficulty for each cutscene and each squad count. Halo pins the squad table against the original's rule (`tests/test_squad_counts.py`).
- Halo audit (#403) is the model: list every difficulty branch on every map, with the original path, before any fix.
- **Check the player's difficulty in the boot log before calling a line or a spawn missing** (Parity, #402). See DIALOGUE.md section 3.6.

## 5. Checkpoints and saves

### 5.1 What the original does (Halo, history)

- A checkpoint is a request. The game checks every 11 ticks; it takes the fourth safe check in a row, then writes. A request gives up after 240 ticks.
- Safe means: no aware enemy, nothing dangerous in flight or dying, the player on the ground and alive.
- Death reverts to the last checkpoint after 92 game ticks (`src/death_timer.h`).
- Quit writes the checkpoint to the persistent store. Continue reads it back and restores (`game_state.c`).

Your source: trace the save calls of the source platform and their callers before you decide where saves go. (GUESS until traced: the names are unknown.)

### 5.2 Save size (Halo VMU)

- Dreamcast VMU: a block is 512 bytes; a card holds 128 blocks (SAVEGAME.md).
- Halo save cap: 30 blocks minus the file overhead = **14,208 bytes** (`SAVE_MAX_BYTES`).
- A typical a10 save is about 11 KB (24 blocks). The a50 late game is 14,147 of 14,208 bytes, with 56 of 200 units kept (test numbers, `tests/savegame_test.c`).
- Cut order when full: the script, devices and groups are never cut. Units farthest from the player are dropped first and restart at spawn after Continue. A save that still does not fit is refused whole; the older save stays.

ESTIMATE for your game: a game with more plot state than Halo (for example, party or inventory state) may not fit one card. Count the plot flags and the per-save state from your source data before choosing a format. Do not assume it fits one card.

### 5.3 VMU write cost and where writes happen

- A VMU write stalls the machine for 1 to 2 seconds (history, SAVEGAME.md).
- Halo first wrote a checkpoint to the VMU on every checkpoint. A throttle (once per 5 minutes, test image only) followed. On 2026-11-07 the decision was: checkpoints are RAM only. The VMU is written by an explicit pause-menu SAVE row, by save-and-quit, and by a won map (`07159629`, `c6e5708d`). The pause SAVE row is a DEPARTURE (OpenCE saves on quit only) at the player's request.

Rule: decide the write points as a list in the design doc. Anything else writes RAM only.

### 5.4 Save format versions

Halo `SAVE_VERSION` went from 2 to 10 over the build history. Each step is documented in `src/savegame.h`. The big one: version 6 (2026-10-20) refused every older save. That was allowed then because no saves existed yet. It is not allowed once testers have saves.

Rule: a version bump is a player-visible cost. Bump only with a reason, and with a resume rule in the same change (section 5.5).

### 5.5 Resume across builds (#391 R2, #384, `7447b440`, SAVE_VERSION 10)

Problem: any build change refused every older save with "starting from the beginning". The owner's boot 2026-11-15 (v0.82 b2): "Save from another build; starting from the beginning is quite annoying."

Fix, two keys:
- `table_key`: a hash of what a save indexes: spawns, doors, volumes, switches, points, groups, waypoints, globals, and track names and gates. It excludes instructions and predicates.
- Per-track code hash: for each track that was part way through (live), its place (pc from start) and a hash of its code.

A save from another build resumes when the table key matches and every live track's code is unchanged. A track at its start or at its end starts or ends in the new code. Otherwise the map restarts from its beginning, never from the campaign start. Same-package saves use the old path.

Cost (`7447b440`): text +4.05 KB (campaign 2,420 B, savegame 1,444 B), `save_t` +1,208 B heap transient, VMU +1 to +1,204 B. 23 tests OK. Boot not yet proven: GitHub #384 and #391 are open.

Rules for your port:
- Build the two-key resume from day one. It was built after the fact in Halo, and it cost a save version and a test set.
- Key by stable names (script sequence name, object name, flag name), never by array index.
- The "start over" notice belongs on the mission-select help text, not on the loading screen (#384, open).
- **Refuse a save that does not match, and say why in one log row** (Parity, #391 R2). A save from another build resumes only when the data it indexes still matches and every live script part is unchanged. Otherwise it is refused, and the reason is printed once.

### 5.6 Section and level switch saves (#391, #393)

- OpenCE's d40 run has four save calls. The port did not fire them. The original never saves on a BSP switch. The owner's order (2026-11-15) was a save at each BSP switch in the run, as a DEPARTURE (`b5dd4457`). The owner's correction the same day asked for both: the OpenCE save calls must fire as in OpenCE too (`5a411cd1`). Check the tracker row F-D40-RUN-SAVES for the current status.
- Safe-save rules still apply: no save mid-air or in danger. A once-per-load serial row says when a save gives up.
- A BSP switch frees RAM. #393: the d40 arena shrinks to the d40h size at the first checkpoint, +434 KB free (`6e07784c`). Rule: a section switch is the moment RAM is most free; plan the arena and the save together.
- A save is valid only for the package that wrote it. Halo's package key is the layout id above.
- **Switch, checkpoint, revert, reconcile is one test** (Senior, #406: a vehicle destroyed after the revert). Any state the level stream sets during a live area switch must also come back on a revert and on Continue. Write one host test per switch-then-revert pair, for every system that keeps per-area state.
- **Give-up rules need a serial row** (Parity, #391: a CHECKPOINT GAVE UP row, added after the first boot showed no saves). A silent give-up cannot be told from a bug on the owner's boot. Add one once-per-event row naming the rule that held it.

### 5.7 What is not kept (Halo)

Projectiles, effects, sounds in flight, the voice queue, dropped weapons, corpses, AI squad state (command lists restart), device animation beyond position and power, the scripted camera, the walk state of a boarding marine. Players see these as small resets after Continue. Write the list into the design doc before the first save ships.

## 6. Devices, doors and lifts (#36, #86, #87, #114, #187)

- #114 (above, section 1): a placed object removed as a "departure" sealed a lift room. Rule: a departure may not remove objects that a script creates.
- #36: the lift rule went back to the old rule: only the six Maw pistons stand on top (`9e0a752e`). An ELEV serial row (one line per start and stop, 64 per level, verbose images only) was added to see lift state (`05e909a7`).
- #86 and #87 (c10 lift gone after a ride, fall to death on load). Order of work: a LIFTPAIR serial row first (`9dca1395`: names, position, campaign bits at BSP load, create and destroy, checkpoint revert; no behaviour change). A debug teleport onto the lift top so the ride could be tested (`64a12f63`). Closed 2026-11-10 after the owner's b12 boot (`d0fe2e94`). Rule: state row, then test lever, then fix, then close on the owner's boot.
- Door walkability. The port had a step-up invention (`device_step`) that let a player climb a jamb lip. It was deleted (K2, 2026-10-21). Replacement: the door's open pose decides walkability. `tests/door_open_walk_test.c` poses every baked door model at open, closed and part-open values. Measured (ESTIMATE of the walk test, not a boot): with the door open, a10 lets 248 of 470 sweep lines through. Of the blocked lines, 12 would pass only with the body raised 0.12 wu; those lips are no longer rescued. Rule: a test over every baked model, not a sample.
- Device save: the save stores device position and power only where they differ from the level start, by object name (`save_device_t`).

Your source: doors and lifts may be script-driven movers (toggle, track or interpolation). Trace the mover state, its caller, and the save fields before the first door ships. (GUESS until traced.)

## 7. Allegiance and factions (#358, SAVE_VERSION 7 and 8)

Halo keeps a faction-pair word in the save: one bit per unordered pair of the five fighting factions, plus the player with each. Version 7 raised it to 15 pairs (`SAVE_ALLEGIANCE_PAIRS`). Version 8 added the incident counters.

The betrayal rule (OpenCE `game_allegiance.c`, ported in `gameplay.c`):
- threshold 5
- betrayal +3, accident +1
- decay 300, 450, 1200 or 2700 ticks by difficulty
- a witness of the victim's team within 18 wu

#358 fix (`453c7bda`): an accidental grenade, explosive or vehicle kill does not count, and a grenade kill is never a betrayal (+200 B text, RAM 0).

Lesson: the census note said the betrayal rule was "not ported". It was stale. The code had it since v0.5.0. Grep the code before you write a gap.

Rules for your port:
- Count the factions from your source table. Size the pair word from that count. (The count is not known until you count it.)
- Save the friend / enemy state; keep the incident counters if the original has them.

## 8. begin_random and random branches (#359)

- Before #359, 87 of 94 random sites ran in a fixed order. That was wrong against the original.
- Now: the original rule (`hs_runtime.c:2368-2423`). The seed is 0xDEADBEEF at map start, and it is not restored on a revert or a load, as in the original.
- Run masks are saved (SAVE_VERSION 9): a count byte, then site byte and a 16-bit mask for each site part way through.
- Cost: a50 resident package +5,656 B; about 2,116 KB free (ESTIMATE; send back under 2 MB).

Rule for your port: match the original's RNG and seed rule. Save the picks that already happened, so a reload does not re-roll a branch the player already saw. If the original does not save its RNG state, do not save it either.

## 9. Impulse sounds and sources (#191)

- The source object of a scripted sound is part of the sound. Without it every sound plays centred.
- Halo fix: the source is packed in the dialogue event; the sound plays at the object's head with the sound tag's min and max distance (`67dc3ba3`).
- Leftovers, documented: head = position + 0.9 height; objects use the origin; the sound is not cut when the speaker dies.
- Rule: a sound test checks that a sourced line is placed at its speaker.

Your source (GUESS): a scripted sound may name a source actor and an attenuation curve. Check the script node's source field before you convert it.

## 10. Test pins that go stale (what cost hours)

- `test_mp_guest` baseline (Halo): 24 commits "re-pin: level.c moved at <hash>". The baseline is the preprocessed campaign source. Every edit to `level.c` moves it.
- `tests/test_save_package_change.py` matches C source text with regexes. Any move of the code breaks it.
- `tests/test_encounter_cap.py` pins struct byte sizes (for example 5056 and 26696). Each new field breaks it.
- Track counts per map (c10 99 to 98, d40 129 to 127, b40 174 to 173 on the cutscene music change). They move with the converter.
- About 170 commit messages mention "stale". One shard run had 23 failures, none a game fault (F-SHARD-R2-REPINS).

Rules:
- Pin outputs (counts, sizes, package hash), not source text.
- A pin change names the commit that moved it, in the same change.
- Read the diff before you re-pin. Never edit a pin only to make it pass.

## 11. Bake and build rules (what cost hours)

- The Halo b10 images were superseded on 2026-11-10. One step of a chain ran alone: the a30 audio was out of step with the audio stitch, and the H2 ambience was lost. Five data tests caught it after the build. Rule: one hidden bake per chain, a consistency check on every map, then the build (`cb74d719` in `docs/BUILD.md`).
- Halo commit messages mention rebake about 270 times. Each script change needs a rebake of every map it touches. Plan that cost into the build.
- Serial logs: 115200 baud. 1 KB = 94 ms. A script trace row must be once per load or once per event, with a cap. Halo's tracker rule: "no per-frame printf without once-guard or cap".

## 12. What your source game may bring (checklist)

Each item is a GUESS until you count it in your source data. Check them before the design is written.

- **Script node kinds.** Cover these first: sequence start; actions (spawn, destroy, toggle, sound, dialogue, camera, scripted movie); variable and global read and write; compare and condition; random; delay and wait-for-trigger. Map each one to the Halo shapes in section 2 of [SCRIPT_CONVERSION.md](SCRIPT_CONVERSION.md) (tracks, instructions, predicates, globals, spawns, doors, groups).
- **Startup and menu flow.** Trace the boot, first-run check, intro and main menu as named script states before you write runtime code. A menu table with a variable entry size is a parser risk: check the parser against the source data first.
- **Plot flags.** Count them from the source data. Give each a slot and a stable name, then choose a bit array. Measure before you pick the format. ESTIMATE: no format is safe until measured.
- **Per-save state.** A large RPG-style or open-world game may carry far more state than Halo's save. Measure it, then size the save against the card (a VMU card is 128 blocks, 64 KB; Halo's cap per save was 30 blocks).
- **Factions and difficulty.** Count the factions from the source table (section 7). Read the difficulty enum from the source (section 4).
- **Save points.** Trace the source's save calls and their callers (section 5.1). Write the list of write points in the design doc (section 5.3).
- **Engine natives.** Do not port the engine. Census the native calls the scripts make (Halo's census tool: `tools/assets/hsc_census.py`) and implement only those.
- **Test from day one.** Halo's playthrough walker and squad-count pins caught most mistakes before a boot. Build the equivalent before the first map is converted.

## 13. Short list of what cost hours in Halo

- Silent drops found by the owner's boot, not by a test: Wraiths never fired (#272), 652 sourced sounds played centred (#191), screen filters missing in every scene (F-X1-SCREEN-FX).
- A departure that removed a script's object sealed a lift room (#114).
- A save version bump refused every save during testing (v6, 2026-10-20), then the resume rule came in v10 (#391 R2).
- Lift bugs (#86, #87) took several builds with diagnostic rows before a fix.
- A half-run bake chain made the b10 images unusable (2026-11-10).
- Test pins that copied source text or counts went stale on every change (section 10).
