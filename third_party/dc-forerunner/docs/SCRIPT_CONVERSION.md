# Script conversion: from <Game>'s scripts to a compact campaign package

How the Halo Dreamcast port (repo `dc-halo`, read-only for this guide) turned the original game's mission scripts into its own compiled package and VM. Written so a port of <Game> can do the same for its <source script system> sequences and script functions.

Tags used below:
- **GUESS**: a <Game> claim not checked against the source data.
- **ESTIMATE**: a number from a plan or a host test, not measured on hardware.
- Halo file paths are relative to the `dc-halo` repo root. Halo commit hashes and `#NNN` numbers are `dc-halo` history.

## 1. The short version

Halo's scripts never run on the Dreamcast as script. An offline compiler reads the scenario's script tree and writes one flat binary table per map (`<map>.dccp`). A small VM (`src/campaign.c`) runs that table once per frame. The VM knows nothing about Halo. It knows tracks (threads), predicates (conditions), globals, and events. Anything that touches the engine is an event: the VM raises it, the level code does the work, and the level acks it.

Three rules came out of the port:

1. Required operations fail the build. Optional or presentation-only operations may be skipped, but every skip is written to an audit file that a person reads each build.
2. Numbers are append-only. Event types, predicate ops and instruction ops keep their numbers. The converter and the C header must agree on every number.
3. Saves are keyed by the package's layout id, and a save records what it indexes by name. A save from an older build resumes when the parts it uses did not change.

## 2. Pipeline, end to end

| Step | Tool | Reads | Writes |
| --- | --- | --- | --- |
| 1. Extract | Your source reader, one per source format (Halo: `tools/assets/inspect_campaign.py --map <map>`) | The extracted copy of the source data in `SOURCE_EXTRACT` (the converter needs no source disc or `SOURCE_ISO` at bake time) | `campaign/inventory.json`, `campaign/scripts.json`, `campaign/REPORT.md` |
| 2. Compile | Your converter (Halo: `tools/assets/convert_campaign.py`, class `Compiler`, plus `tools/assets/campaign_maps/<map>.py`) | Inventory, scripts, the map module, the audio manifest | Tracks, instructions, predicates, spawn and door rows, `campaign/runtime_audit.json` |
| 3. Bake cutscenes | Your cinematic baker (Halo: `tools/assets/build_cinematics.py`) | The package and `runtime_audit.json` | `campaign/cinematics.bin` (format `DCCI`, scenes and timed events) |
| 4. Pack | `pack_package()` and `layout_trailer()` (Halo: `convert_campaign.py`) | The parts above | `campaign/<map>.dccp` |
| 5. Run | Runtime VM (Halo: `src/campaign.c`: `campaign_load`, `campaign_start`, `campaign_update`, `campaign_ack`) and the event handler in `src/level.c` | `.dccp`, `cinematics.bin` | The running level |

Build order is set in your build step (Halo: `tools/assets/build_map.py`, around lines 190 to 250): `campaign` (inspect), then `campaign-runtime` (convert), then `cinematics`. The build comment says the cinematics step reads the package and the audit, so the order is fixed. A map with no campaign module gets the scriptless "Alpha" package (`--alpha`). The generic example step is `tools/build_assets.py` in this repo; wire the chain there.

### 2.1 What the scripts look like on input

The shape of the input is whatever your source reader writes. Halo's `scripts.json` holds the script tree as the scenario tag stores it. There are two kinds of records:
- `nodes`: one record per token, with `datum`, `flags`, `child`, `next` and `data`. Arguments are a `next` chain. A call is a node whose `child` is the call's name.
- `expressions`: one record per named root, with `category` (`script` or `global`) and the call list.

The converter walks this tree. It never interprets it at runtime. Write the input shape into your reader's docs, so the next person does not have to guess it.

### 2.2 Per-map module

Each map has a module in `tools/assets/campaign_maps/` (Halo). The generic compiler reads the module for everything map-specific. Fields that matter (Halo; see `campaign_maps/__init__.py` for the full list):

- `NAME`, `PLAYABLE` (false keeps the Alpha package), `STRICT` (true makes unsupported ops fail the build).
- `THREADS`: which scripts start with the map and at which stage gate.
- `CINEMATIC_SCENES`: which static scripts are baked as scenes, with options (`bsp`, `skippable`, `difficulty`, `tail`, `vm`).
- `SKIPPED_STATICS`: static scripts a call site skips and logs.
- `PRUNE_UNUSED_GLOBALS`, `PARK_THREADS`, `FREE_ACTORS`, `NAMED_BIPEDS`, `PLACED_DEAD_OBJECTS`, `SPAWNER_SQUADS`: flags for the cases each map hit.

Halo's a10 is `STRICT = True`. Every other compiled map is `STRICT = False` (skip and log). a30 began as an Alpha package and became playable later.

## 3. How the compiler lowers a script

- `statement()` handles actions. It emits instructions and events. `expression()` handles predicates. It emits tokens. Both look up the Halo call name in long `if / elif` tables.
- Examples (from `_expression` and `_statement`): `game_difficulty_get_real` becomes a `difficulty` token. `game_difficulty_get` becomes `difficulty`, `const 1`, `max`, because OpenCE says that function never returns Easy (`hs.c:9439`, quoted in the converter).
- Static scripts are inlined into the caller's thread, up to 4 levels deep.
- `sleep_until` with no period given uses OpenCE's default of 30 ticks.
- Each conversation becomes its own dormant track with hard timeouts: 45 s for a begin held by distance, 20 s for a line held by a speaker still walking (`CONVERSATION_BEGIN_TIMEOUT`, `CONVERSATION_LINE_TIMEOUT`). A script must never hang on a missing condition.
- `begin_random` becomes a table dispatch with a per-site run mask (see section 4.4).
- Unused scenario globals are dropped on maps that set `PRUNE_UNUSED_GLOBALS`. The rest are renumbered in scenario order.
- Halo's a10 output is frozen: its tables must stay byte-identical unless a change is deliberate. The converter keeps a comment saying so. Do the same for your first stable map.

## 4. The package

### 4.1 Layout

All scalars are little endian. The file is:

1. A 64-byte header: magic `DCAM`, `version`, total `bytes`, body `hash` (FNV-1a over the body), nine table `count`s, `flags`, group count (`reserved[0]`), waypoint point count (`reserved[1]`).
2. Nine tables, in this order: volumes (92 B each), switches, points, spawns (60 B), doors (128 B), tracks (44 B), instructions (20 B: `op, a, b, c, value`), predicates (12 B: `op, arg, value`), globals (float).
3. The group table (40 B each) and the waypoint table (12 B each).
4. Optional 4-byte layout trailer (section 4.3).

The loader (`campaign_load`) checks the magic, the version, the total size, the body hash, every table size against a cap, the trailer, and then `validate()` (jump bounds, expression stack depth, event operands, geometry). A bad file sets `c->error` and the state becomes `CAMP_FAILED`.

### 4.2 Version history

`CAMPAIGN_VERSION` in `src/campaign.h` must equal the converter's `VERSION` in `convert_campaign.py`. The loader refuses any other version, so every map must be rebuilt after a bump. The history in the Halo header comment:

- 2: spawn flags (the editor's disabled spawns).
- 3: header flags, 16 BSPs.
- 4: machine rows and the device group table.
- 5: wake and dormant tracks, 160 tracks, 64 conversations.
- 6: 192 tracks, faction 5, two more AI ops.
- 7: spawn rows shrink from 156 to 60 bytes; waypoints move to their own table.

Caps in `campaign.h` (plan for the same kinds of limit in your game; the Halo values are a starting point, not a target): 192 tracks, 128 globals (slot 127 is the stage), 64 conversations, 16 BSPs, 96 encounters, 640 objects, 1280 spawn rows, 32 device groups, 64 `begin_random` sites with 16 children each.

### 4.3 Keys: which number says "same package"

- **Body hash** (header): integrity only.
- **Layout id** (`package_id`, the 4-byte trailer): FNV-1a over the whole package with sound durations set to zero. Any change to counts, rows, instructions or predicates changes it. A sound that is only longer or shorter does not. Saves are keyed on it, so a save from another layout is refused.
- **Table key** (`table_key`, #391 R2): a hash of what a save indexes (counts and bytes of spawns, doors, volumes, switches, points, groups, waypoints, globals, and each track's name and gate). It has no instructions or predicates, so it changes less often than the layout id.
- **Track code hash** (`campaign_track_hash`): one hash per track, over its instructions by content. Jumps are stored as offsets inside the track, so an edit in one track does not move another track's hash.

### 4.4 Resume across builds (#391 R2, commit `7447b440`)

Before this change, any rebake refused every older save and the player started the level again. `SAVE_VERSION` is now 10 (`src/savegame.h`), and the save carries the table key and each live track's position and hash. The rule in `campaign_resume_remap`:

- Table key must match, or the save is refused.
- A track part way through keeps its place if its code hash still matches. If the code changed and the track was at its first statement, it starts again. Otherwise the save is refused, and the map restarts from its own beginning, never from the campaign start.
- A track on its `END` stays on `END`.

The commit's cost (from its message, ESTIMATE): text +4.05 KB, `save_t` +1,208 B heap transient, VMU +1 to +1,204 B.

## 5. The runtime VM

### 5.1 State and loop

`campaign_snapshot_t` (about 23 KB; its size is pinned by a compile-time assert in `campaign.c`) holds everything a save or a checkpoint needs: per-track `pc`, `wait` and `delay_target`; 128 globals; stage; door position and power; spawn active, dead, health and vitality bits; sound countdowns; conversation status; the HUD timer; the `begin_random` masks.

`campaign_update(dt, player position)` runs once per frame:
- Every running track accrues its wait time.
- Each running track executes up to 128 instructions. It stops at a blocking `WAIT`, a `DELAY`, or an `EVENT`.
- An event is written to `pending` and the VM stops. The event's id is the instruction index plus one. Nothing moves on until the level calls `campaign_ack(id)`. A failed handling leaves the event pending.
- A track runs when its gate is at or below the current stage and it is not dormant. A dormant track starts only after a `CAMP_WAKE` event names it.
- While a cinematic plays, `status` is `CAMP_CINEMATIC` and `campaign_update` does nothing.
- Player volume tests (oriented boxes) switch the BSP. On a10 the switch waits for the stage gates.

### 5.2 Instructions

Eight ops (`CAMP_OP_*`): `END`, `WAIT`, `DELAY`, `SET`, `EVENT`, `BRANCH`, `JUMP`, `SWITCH`.

- `WAIT` is `sleep_until`: a predicate range `a..b`, a timeout in seconds (`value`, negative means none), and a period `c` in ticks (0 means every frame).
- `DELAY` is `sleep`: the duration is an expression, read once on entry.
- `SET` writes a global. Writing global 127 also sets the stage.
- `BRANCH` jumps to `c` when its predicate is false.
- `SWITCH` takes the entry from a jump table. It is used for `begin_random`.

Ticks are 30 Hz throughout. Sound and animation lengths are converted to ticks in the audio manifest step.

### 5.3 Predicates

A predicate range is a postfix list of `(op, arg, value)` evaluated on a 32-entry float stack (`evaluate()`). The op list is the `CAMP_P_*` enum in `campaign.h`. It covers constants, globals, volume tests, BSP, living counts, device state, difficulty, logic (`and`, `or`, `not`), comparisons, arithmetic, random ranges, sound timers, line of sight, player actions, and about a dozen later additions. Later additions use offsets from `CAMP_P_NONSWARM` so the earlier numbers never move (for example `CAMP_P_HUD_TIMER` is `CAMP_P_NONSWARM + 2`).

### 5.4 Events

The event enum `CAMP_*` (about 40 types, in `campaign.h`) is the entire contract between the VM and the level. Examples: `CAMP_MOVIE` (play baked scene n), `CAMP_SPAWN`, `CAMP_ERASE`, `CAMP_CHECKPOINT`, `CAMP_DOOR`, `CAMP_DIALOGUE`, `CAMP_AI` (an AI order), `CAMP_ACTOR`, `CAMP_VEHICLE`, `CAMP_HUD_TIMER`, `CAMP_LOST`, `CAMP_WIN`, and `CAMP_SOUND_AT` (value 40, the last one). Each has `arg`, `aux` and `value` fields. Their meanings are written beside the enum in the header.

`campaign_ack` is the only place that changes state for some events, for example `CAMP_MOVIE` sets the status back to playing, and `CAMP_BSP` sets the BSP and a transition latch.

### 5.5 Object references and scope

An AI reference is one 32-bit datum: top two bits = scope (0 encounter, 1 platoon, 2 squad, 3 named biped), bits 16 to 23 = child index, low 16 bits = encounter (`campaign_spawn_matches`). A named biped has scope 3 and its scenario object name in the flags of its spawn row (`CAMP_AI_NAMED`).

### 5.6 Spawns, devices, globals

- Spawn rows: the converter writes one row per actor of the largest difficulty count of each squad. Flag bits 3 to 6 say on which difficulties a row is not placed. OpenCE places min, average or max counts by difficulty (`encounters.c:2205`).
- Devices: `CAMP_DOOR` events set a door's position, power or automatic mode. Doors belong to groups with initial values and a changes-only-once flag.
- Globals: 128 float slots. Some are reserved scratch slots the converter uses for the conversation line, the shake values, the SFX loop tag, the HUD timer, the random site and the told flag (`CONV_LINE_TEMP`, `SHAKE_TEMP`, `TIMER_TEMP`, `TOLD_TEMP`, and others in the converter's constants block).

### 5.7 Sounds

- Sound durations come from the audio manifest, in ticks. `sound_remaining` counts down per sound slot (32 slots).
- A conversation line waits for its sound's timer, then its post-speech delay, then starts the next line (ports the OpenCE order).
- A sound with a source object (`sound_impulse_start` with a source) is `CAMP_SOUND_AT`. Its value is the source: a spawn reference or the negative of a scenario object name. Without a source it is `CAMP_DIALOGUE` (see section 8.2, #191).

### 5.8 Cinematics

A cinematic is not run by the VM. `CAMP_MOVIE` carries a scene index. The scene is a baked timeline in `cinematics.bin` (`DCCI`: scene table, then timed events with 8 values each, all at 30 Hz). The scene plays in the engine. The VM waits until the level acks the event. Scene options, from the map module: `bsp` (its first camera's BSP), `skippable` (wrapped in OpenCE's skip window or not), `difficulty` (the difficulties it plays on), `tail` (only the statements after a named call), and `vm` (a scene that also runs some campaign effects).

## 6. Stubs, skips, and how they are logged

The compiler has three levels of tolerance. Each has its own output.

1. **Required, strict map (a10 only).** An unsupported required predicate or action raises `ValueError` ("unsupported REQUIRED predicate ..."), and the build stops. The same applies to unresolved actor types, too many tracks, too many globals and `begin_random` sites that do not fit. The converter test `test_unknown_required_predicate_rejected` pins this.
2. **Skip and log (all other maps, `STRICT = False`).** A skipped statement emits nothing; the code, predicates and audit notes it started are rolled back. A skipped predicate emits one constant. The constant is chosen so that a `sleep_until` gate passes when only `and`, `or` and `not` lie between the wait and the predicate (1 at an even `not` depth, 0 at an odd one), else the table default (0, or the module's `SKIP_VALUES`). Each skip is recorded with script, op, kind, behaviour and polarity.
3. **Deliberate simplifications.** `compiler.note()` writes a row to the "approximations" list. Omitted presentation ops have fixed text in `STAGING_NOTES` (for example `unit_open` is rendered only for the cryotube). The build prints the count.

Where the output goes (Halo):
- `campaign/runtime_audit.json`: `approximations` (the notes), `skipped` (folded counts per script, op, kind, behaviour and polarity), plus side lists such as `banshee_flights`, `aim_tracks` and `dropship_squads`.
- `--dry-run` compiles without writing, prints `skip_summary` (per-op totals) and lists every wait made only of skipped predicates, with a `deadlock_risk` flag.

At runtime, `campaign.c` logs one line at load: the layout id and the package hash. `level.c` acks every event before it handles it. The Halo port found no log line for an event type that has no handler in the dispatch block (around lines 40590 to 40850 of `level.c`). A new event type with no handler is therefore silent. That is a gap, not a design choice. Give every event type a handler, or a once-per-type row that says it has none.

The rule that came from this: a note on a required op is an error, not a pass. Section 8.3 has the case that proved it.

## 7. Tests that pin it

- `tests/campaign_test.c` (Halo): host C harness. The build line is at the top of the file: `cc -std=c99 -O2 -Wall -Wextra -Werror -Isrc tests/campaign_test.c src/campaign.c`. It covers the runtime state machine, autosave, the savegame round trip, and the sound-event counts.
- `tests/test_campaign_conversion.py` (48 test methods): converter output on small fixture scripts, the real a10 package and the real machine rows. It calls the C harness with `run_host`. It also checks row widths, the group table, the device events, the a10 final gates, and that every built package loads.
- `tests/campaign_resume_test.c` (#391 R2): save from one build, resume in another.
- `tests/campaign_playthrough_test.c` (F14): loads each map's package and plays it with a trigger director. Every event is checked (scene index inside the scene count, BSP index valid). When stalled for 150 ticks the director moves the player or kills the next squad. This is a static walk, not proof of play.
- `tests/campaign_park_test.c`, `campaign_gate_walk_test.c`, `campaign_load_test.c`, `campaign_synth_test.c`: parking, gates, loading, and the synthetic FIREFIGHT package.
- `tests/test_campaign_skip_mode.py`: skip mode and `begin_random`. It runs the emitted code on a small interpreter for every draw sequence of 1 to 4 children and compares the result with the OpenCE rule.
- `tests/test_campaign_integration.py`: real `level.c` handoff, checkpoint and event helpers with converted assets. It skips when the assets are absent.
- Topic tests, for example `test_script_impulse_source.py` (#191), `test_conv_gate.py`, `test_conv_advance_362.py`, `test_vehicle_scripts.py` (seat label hashes must be distinct), `test_campaign_allegiance.py`.
- Before a converter refactor, the design doc (`docs/archive/feature/VEHICLES_SPEC.md`, D1) used a golden set: a SHA-256 of the a10 outputs taken before the change, compared after it. It was regenerated only on purpose.
- Halo's test runner runs 10 shards (`tools/run_tests.py`). A small change runs only the tests of the files it touches.

## 8. Mistakes that cost time in the Halo port

Each item names the commit or issue, the cause, and the lesson.

**8.1 A renumbered or unmatched event (#191, `67dc3ba3`, then `7a196944`).** The new event `CAMP_SOUND_AT` was appended as value 40, after `CAMP_GC`. The converter's `EVENT` table is a tuple indexed by position, so its order must equal the C enum. A count pin (`campaign_test.c` line 651) counted only `CAMP_DIALOGUE`, so it broke until `7a196944` counted both types. Lesson: append only; a count pin must count every type that carries the thing.

**8.2 Silent drop of an argument (#191).** 652 `sound_impulse_start` calls named a source object. The converter dropped the source with no error, so every impulse played centred. `67dc3ba3` packs the source into the event and plays it at the speaker, with the sound's min and max distance. 97 lines are now sourced. GitHub issue #191 was still open when this guide was checked. Lesson: a dropped argument is a bug, and an audit count is not a fix.

**8.3 A logged note hid an empty squad (#272, `851b83c3`).** `vehicle_load_magic` of a squad into a Wraith or a Ghost was lowered to an "omitted staging" note. The Wraiths were placed and never fired. The note was the bug, and it sat in the audit file where nobody read it. Lesson: a note on a required operation is an error (section 6).

**8.4 Difficulty branches (#402 and #403, open at check time).** Issue #403 says the port compiles the Normal branch for every difficulty. Issue #402 is the example: on Legendary only, the x10 hangar scene plays Sgt. Johnson's line `sgt05e`, and the port did not play it. The cause in code: `build_cinematics.py` folds `game_difficulty_get_real` in a baked scene to one value, the first value of the scene's `difficulty` option, else 1 (Normal). So one baked scene plays one branch on every difficulty (`build_cinematics.py` around lines 1381 and 2168). Earlier, `323aa226` split the two difficulty functions: `game_difficulty_get_real` is the real setting (Easy included), and `game_difficulty_get` is clamped to Normal as in OpenCE. Before that, c20 took its Easy branch on every difficulty. Lesson: two functions that look alike can return different values. Read the OpenCE function and its caller first, then compile each branch as its own branch.

**8.5 Save refusal on every change (v6, then #391 R2, `7447b440`).** Version 6 (2026-10-20) refused every older save during testing. The fix was a resume rule, not another bump. Lesson: build the resume rule before the first version bump that testers will hit.

**8.6 Stale pins.** `test_mp_guest` has a baseline that is a hash of the preprocessed campaign source. Every change to `level.c` moves it, so the port has many "baseline re-pin: level.c moved" commits (for example `82bc04a2`, `4246d9b6`). The a10 x20 handover movie went from 290 to 291 rows (`eb767988`), with the cause not confirmed. One test is still named `test_every_built_package_loads_at_version_5` while the version is 7. Lesson: pin outputs (counts, sizes, hashes), name a pin by what it checks, and say in the commit which change moved the pin.

**8.7 Random branches in a fixed order (#359, `57522588`).** Until then 87 of 94 `begin_random` sites ran their children in a fixed order, which is not OpenCE's rule. The fix matched the OpenCE random draw and seed. Lesson: a documented "order, not random" departure is still a departure. Track it with a row and a reason.

**8.8 Rebakes after a script change.** The converter output depends on every map's script tree. `323aa226` says all compiled maps need their package rebuilt. Lesson: a script or converter change means a rebake of every map it touches, in one build chain.

## 9. Mapping to <Game>

The same split applies to <Game>: an offline compiler writes a flat package, a small VM runs it, the engine work is done by events, and an audit file lists every approximation. Every <Game> claim below is GUESS until your source data checks it.

### 9.1 Script objects to the package

| <source script system> object (GUESS on the exact class list) | Halo equivalent | Notes |
| --- | --- | --- |
| Sequence (one per level, or a sub-sequence) | one script (a Halo script root) | One sequence becomes one or more tracks. |
| Event node (level start, trigger, touch, named remote event, console) | automatic entry tracks (`THREADS`, gate 0) and dormant tracks woken by `CAMP_WAKE` | A named remote event becomes a wake by name. |
| Action node (toggle, delay, play sound, interpolate, teleport, spawn, log) | `CAMP_*` events; a delay becomes `DELAY` | A latent action (an interpolation, a sound that must finish) is an event that the level acks when done. A log node is a skip row, or a serial row printed once. |
| Variable node (int, float, bool) | global slot (float) | Pick slots at bake time; reserve scratch slots. |
| Object variable | a stable object name | Do not store engine object indices in the package. Use names, as Halo does with scenario object names. |
| Output and input links | instruction order; fan-out becomes parallel tracks | Halo: the track order is the scheduler order, not a claim of sequence. |
| Gate, switch, compare | `BRANCH` with a predicate; a gate becomes a predicate | |
| Random | the `begin_random` pattern (`SWITCH` with a run mask) | Match the engine's draw rule and seed, and save the picks. |
| Latent wait for a condition | `WAIT` with a timeout | Every wait gets a timeout. A script must not hang. |

### 9.2 Native functions and script functions

- **Native functions** (implemented in the <source engine> binary, not in script; GUESS for your build): do not port the engine. Make a census of the natives the converted scripts call (Halo's tool for this is `tools/assets/hsc_census.py`) and implement only those. Each one that changes the world becomes an event, the same as a Halo `CAMP_*` op.
- **Script functions** (bytecode in the package export; general knowledge of the engine, not checked on your source data): decide per function. If a script sequence reaches it, compile it into the package like a Halo script. If it is simple and fixed, write it in C, as Halo did for its hand-built startup tracks in `campaign_maps/a10.py`.
- Do not run a bytecode interpreter on the Dreamcast. Halo's rule: the compiler does the interpreting, the device runs a table.
- Keep a per-function row in the tracker, with the reason, for every function that is a departure.

### 9.3 Plot flags and conditionals

- Plot flags probably live in a game-state table or a set of globals (GUESS). Count them from the source data first. Give each a slot and a stable name. In Halo the table key hashes track names and gates, while save bits still index spawn rows by number; the layout id changes whenever a row moves, which is why the save needs the resume rule.
- A script `if` becomes `BRANCH` on a predicate. A per-difficulty `if` becomes its own branch, one per difficulty, and each branch is tested (section 8.4).
- Size the save from the flag count before choosing a format. Halo's save cap was 14,208 bytes (SCRIPTING.md section 5.2). A larger plot state is likely in a bigger game (GUESS).

### 9.4 Startup and menu flow

- Trace the startup flow (boot, first-run check, intro movie, splash, main menu) as a chain of named script states before you write runtime code on it.
- The first state is an automatic entry track with gate 0, like Halo's startup tracks.
- A first-run check is a predicate on a stored flag (GUESS: a first-run or profile flag), then a `BRANCH`.
- An intro movie is a movie event (`CAMP_MOVIE`-like) that names a movie file. Check the movie codec and container against the target before you commit to a playback path. A movie that cannot play on the Dreamcast becomes a still title image, logged as a DEPARTURE row with its reason.
- Caution: a menu table with a variable entry size is a parser risk. Check the parser against the source data before you write runtime code on it (SCRIPTING.md section 12).

### 9.5 Limits to plan for

- Main RAM is 16 MB. Nothing from a `<source format>` package is loaded raw. Source packages can be far larger than RAM (`<your number>` MB unpacked for your largest one; record it in your memory map doc). The bake step must write compact tables, as Halo does.
- Halo's caps (section 4.2) are a starting point. Measure your counts from the source data and raise each cap with a version bump and a resume rule.
- Halo implemented a subset of the original mission opcodes. Its own notes say it does not support all 189 opcodes of the mission closure (`docs/archive/campaign/CAMPAIGN_RUNTIME.md`). Plan for the same: an explicit allowlist of supported natives, and a census that lists the rest.

## 10. Checklist for a new port of <Game>

1. Write a source reader that dumps every sequence, event, action, variable, object name and link to JSON. Keep it read-only and keep its output in `SOURCE_EXTRACT`. (Halo's `inspect_campaign.py` is the model.)
2. Write one map module per level: entry tracks, skipped natives, the strict flag, and the scenes.
3. Compile with strict mode on by default. Put every skip in an explicit list, with a row and a reason.
4. Write `runtime_audit.json`-style output on every bake, and read it on every build. Count the approximations.
5. Give the package a version and a layout id from day one. Key the save on the layout id, hash the names of the tracks and flags into a table key, and build the resume rule before the first save ships.
6. Compile every difficulty branch as its own branch. Test each one (section 8.4).
7. Pin outputs (counts, sizes, hashes), not source text. Name each pin by what it checks.
8. Add a playthrough walker per map before the first map is compiled. Halo's walker caught stalls that the static tests did not.
9. Keep a golden hash of the outputs for each stable map, and regenerate it only on purpose.
10. Rebake every map a converter change touches, in one chain, before any image is built.
