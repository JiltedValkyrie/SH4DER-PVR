# Tools: lessons from the Halo CE Dreamcast port

Source: the Halo CE Dreamcast port (dc-halo). "#NN" means a dc-halo GitHub issue. "commit xxxxxxxx" means a dc-halo commit. Numbers are as recorded there. Items marked GUESS were not measured.

## Rules that saved us

1. Bake, build and test from a clean committed tree. An image with "-dirty" in its name never leaves the team.
2. Never run one step of a bake chain alone. Run the whole chain the tool names. A stale record means rebake, not a hand edit.
3. A build step must refuse to build on a stale or wrong asset. Each refusal names the file, the reason and the one command that fixes it.
4. Name every image with a number and write its build-log row before building. Never overwrite an image.
5. One bake or build at a time per tree. Launcher and editor share one lock file.
6. Every child process runs hidden and logs to a file. No console windows over the user's screen.
7. Scratch, worktrees and test trees go on the data drive, one folder per task. Never on the system drive (it ran out overnight once).
8. Nothing is deleted. Finished scratch is moved to a trash folder, then the user empties it.
9. Pins follow code in test-only commits. A pin that goes stale is not a regression. A regression is a behaviour change nobody meant.
10. Run the full test suite in 10 shards. Put the modules that fail under load on a serial list.

## 1. Bake pipeline

### 1.1 Convert tools and the per-map driver

- One driver per map (dc-halo: `tools/assets/build_map.py`) runs named steps (`--only a,b,c`). Converters write the paks: models, bitmaps, anims, vehicles, collision, placements, campaign scripts, cinematics, audio, footsteps, sfx, visibility.
- Step order matters. The campaign audio chain (audio, stitch-music, dialogue, aivox, footsteps, ambient, weapon-audio, vehicle-audio, sfx, sfx-use) runs whole.
  - Wrong: running `audio` alone broke the b10 images.
  - Fix: the tool prints the exact chain to run next (`rebake_cmd`).
  - Rule for the next port: keep one table in the repo: step, inputs, outputs, what must rerun after it.
- Rebake order for textures matters. A bare `--only bitmaps` drops the swapped character's textures, about 125 KB on four maps.
  - Rule: bitmaps, then custom models, then preferred models, then extra terminal steps. Later steps append to the same paks.

### 1.2 Stale-bake refresh

- Wrong (#329): a models rebake ran after the audio chain on the a10 map. It sliced off the fanfare row in the tag index. The release gate stopped at step 7 with "stale bake on 1 map".
- Fix: `build_map.py --refresh-stale` reruns only steps whose generator files changed. The build runs it first, then checks once. A map still stale after that stops the build with the plain reason.
- Rule: after any model rebake, rerun the audio chain for that map. Let the refresh do it; do not patch the files.
- Step stamps (#315): a step is skipped only when its stamp (source hashes, settings, tool versions) is unchanged AND every recorded output still has the same bytes. Any other case reruns.
  - Numbers: a dry run showed 82 would-skip steps wrote the same bytes, 0 different.
- Keep-identical gate: a bake must not change tracked files (BSP, collision, visibility). If one changes, the build stops and names it, unless its own producer changed.
- **A data-looking bug may be a stale bake** (reviewer session, #340 WDIAG pins, #386 bake-dependent tests). Check the bake record and the build number before you change code. A scale-of-data fix can hide a logic bug.

### 1.2b Cache every slow bake (do this from day one)

Rule: a bake that took minutes once never runs again unless its real input changed. Fresh clone and empty build folder included.

- **Locked static bakes** (#310, chunk visibility): the slow ray-cast result (a visible-chunk set per cell, `bsp/<name>.vis`) lives in version control under `tools/assets/static_bakes/` with a manifest. Key = only the data the exporter reads (chunk order and bounds, triangle positions, opacity, recipe version), not UVs, colours, textures or scripts.
  - Build: local file matches = reuse; missing or other bytes = restore from the store; no record (new map) = bake once, then lock it.
  - Key moved = the build stops and prints the mismatch. Never a silent hours-long rebake, never a stale table on the disc. Refresh one BSP on purpose: `--refresh` (candidate, check, publish).
  - Same pattern fits any slow, rarely-changing bake: lightmaps, nav meshes, impostor sheets, reflection probes.
- **Step stamps** (1.2): skip a step whose stamp (scripts + imports, input data, settings, tool versions) is unchanged AND whose outputs still have their bytes. Default ON; `DCE_STEP_STAMPS=off` runs all, `=dry` runs all and reports would-skips to compare.
- **Step output cache**: when a later step rewrites a pak in place, the skipped step keeps its own outputs in `assets_build/<map>/.step_cache/` and copies them back on skip. A step with no cache whose output a later step rewrites is never current: it always runs.
- **Numba cache**: every `@njit` bake kernel uses `cache=True` so the compile (seconds per kernel per map process) is paid once per machine, not every bake.
- **Measure before caching**: say MB of cache vs seconds saved. dc-halo left campaign audio uncached: 456 MB more cache for about 1,410 s summed per bake (maps run side by side, so less wall time).
- Cache data never ships: the disc gets the same bytes a full rebake makes (dry-run proof: 82 would-skip steps, 0 different bytes).

### 1.3 All maps at once

- Rule: bake every map in one batch, phase by phase. Do not bake one map at a time.
- Wrong (#323, open): the launcher's "Max (Sequential)" setting passed effort 1, so every phase ran one map at a time. The build crawled (c10, then d20, one after the other).
  - Proposed fix (#323): "Max" means effort 0 (all maps at once). The thread budget is shared: nine maps at once get budget / 9 threads each.
- Wrong (#316, coded 2026-11-12): the launcher's build was not the release path. It used the test configuration and skipped the release gates.
  - Fix: the launcher's Build calls the same release code as the team's release (preflight, bake, gates, record, tester image).

### 1.4 MemoryError when baking in parallel

- Cause: each parallel chunk process holds a full copy of its map. Many maps at once, each with many chunk processes, exhausts RAM.
- Fix (commit 25c918cb, Refs #315, #314): bitmaps bake in chunks with a RAM guard (`tools/assets/bake_ram.py`).
  - Each chunk process budgeted at 700 MB (`CHUNK_RAM_MB`).
  - Reserve 2048 MB free RAM (3072 MB while the editor and launcher are open).
  - Slots shared across all maps baking at once: one file per slot, dead owners taken over.
  - OOM retry once.
- Numbers: c40 bitmaps 99.6 s -> 21.5 s with 8 chunks. Output byte-identical on 23 maps.
- Note: no dedicated MemoryError issue was found. The rules above come from the commit and the code comments.

**Rule for the next port:** budget RAM per worker process as well as threads. Print the planned process count and RAM before starting.

### 1.5 Numba and thread budget

- Numba is optional but used by the visibility export (median-split BVH occluders), the env_reflect step and the bitmap jit path. The build venv installs it. Halo docs say it is "several times faster" (no figure recorded; GUESS on size of speed-up).
- `bake_limits.apply(width)` sets NUMBA_NUM_THREADS, OMP_NUM_THREADS and OPENBLAS_NUM_THREADS at each tool's entry point: budget // width. Child tools inherit BAKE_WIDTH.
- Wrong (#314): the launcher's Cores setting was 28, but the bake used all 32. The setting did not reach every process.
- Fix: one budget value, read by every entry point (bake, make -j, pools, Numba). A test reads it and asserts every launcher path passes it.
- Budget formula: budget = logical threads - BAKE_RESERVE_THREADS (empty = 2).

**Rule for the next port:** one config key for the worker count. Test that every entry point reads it.

### 1.6 Never hand-edit baked files

- A baked file that is wrong is fixed by rebake, not by editing bytes. A hand edit is lost on the next bake and hides the bug.
- Converter bugs are fixed in the converter; then the map is rebaked by the chain.

## 2. Map editor

- Profile = the saved file. Save is one click and makes a new named version. The launcher bakes only the saved profile. Unsaved work is never in a build.
  - Rule from the editor doc: a bake that skips the profile step (`SkipProfileBake`) ships the unsaved file. Save before using such a switch.
- Per-object choices are data rows, not code: Draw off, Remove from game, Hide in cutscene, hero shadow auto/always/never, Offset, Detail LOD, Move actor here, Glow.
  - Each row is baked into the object's placement bytes or cinematic ops. Gameplay code does not change.
  - Rule: keep "remove" and "hide in cutscene" separate. Cutscene hide keeps gameplay drawing the object.
- Light masks (#299): a per-light object mask. Map editor opt-in, default OFF, one byte per light. Logged as a DEPARTURE because the original engine picks lights by distance and brightness only.
- Cutscene camera keys (#319, partly done): the Cameras tab edits eye, angles, field of view and tick, then Apply. Pilot camera plays the shot. Reason: the user wanted to optimise cutscene angles that ran slowly.
- View cost readout (#319): shows triangles, vertices, chunks, models and lights on screen. Use it while placing things. It is the cheapest check of the polygon budget (see PERFORMANCE.md).
- The repo records no dated time saving. The honest claim is that many fixes became data edits, which need a bake and no code change.
- Rules:
  - Every editor departure from the source engine gets a tracker row (DEPARTURE + reason).
  - The editor is a front end on the same data files the bake reads. Test round trip: save, bake, same bytes.
  - Bake cards from the editor use the same lock as the build.

## 3. Image build

### 3.1 Entry point and release path

- Build entry: `tools/build.ps1` (dc-halo: `tools/build-game.ps1`). Run it from the MSYS2 shell or PowerShell. Never from Git Bash: MSYS2 make started there loses its arguments and environment.
- Release path (#316): preflight (clean tracked tree; pins file present and valid), bake all maps, gates (keep-identical, stale, audio consistency, lock tests, census), write the record, then the tester image.
- Rule: the launcher's Build calls this same code. Test images and release images differ only by the name, not by the steps.

### 3.2 Numbered images and build rows

- Name: `dc-<project>-v<0.PP>-b<N>.cdi`. The version is the progress percent in `src/version.h` (dc-halo used `halo-dce-v<ver>-b<N>.cdi`). Only the lead changes the version.
- Build number comes from the rows of the build log (`tools/build_number.py`). Commit the row BEFORE building; the number is read from committed text.
- Never write `batch##` names. A batch is not an identity.
- Never leave `.tmp.cdi` files. Rename after the emulator releases the file.

### 3.3 Test image vs tester image (serial tier)

- A name containing `PARITYTEST` builds FULL serial logging. The tester name builds QUIET. Dev and test images carry more rows; the tester image must not.
- Wrong (#346, open): a tester build made after a PARITYTEST build in the same tree kept 9 verbose crossplay objects (+1,064 B text). The tester was not QUIET.
- Proposed fix (#346): stamp the object set with its flags (the verbose flag and the console setup flag). Measured: ELF delta 0 B when stamped. Check the ELF after switching modes.
- Rule: after changing a flag that affects objects, make the build recompile those objects. Or build from a fresh tree.

### 3.4 Self-checks before the image

- `tools/verify_build_inputs.py` runs before the compile and before `make disc`. It refuses when:
  - a map holds the wrong model (a retail Halo CE model swapped in by mistake);
  - a staged file differs from the built file;
  - a staged file is older than its generator;
  - a required font or menu pak is missing (#330: the launcher build never ran `convert_fonts.py`; the image was refused).
- Rule: an image is never made from a stale or hand-patched asset. Each refusal prints the exact fix command.
- Check tree for compile: run the touched objects with the image's real compile flags (`make -n` for the line, then compile).

### 3.5 Launcher

- Six steps the user should need: get source, the bat file builds the exe, open the exe, SETUP, BUILD, PLAY. Nothing by hand. Acceptance: one fresh-clone walk.
- #309 (open): the launcher skipped a required Python package (pymeshlab), and the campaign bake failed at BSP. Rule: setup checks every required package before any bake.
- #313: fresh clone lacked the pinned OpenCE commit. Fix: fetch the pinned commit before any long step.
- #317: first run. The bat file builds the exe and SETUP offers every prerequisite.
- #322: a build opened hundreds of console windows that stole focus. Cause: child processes started without the no-window flag. Fix: a quiet process wrapper (dc-halo: `tools/quiet_proc.py`) installed at launcher start; a test pins its name.
- Rule: no console over the user's screen. Every child hidden, output to the build log.

### 3.6 Disc profiles (GDemu and emulator vs CD-R)

- Emulator and GDemu: one image, full quality, all maps. A GDemu copy is byte-for-byte the emulator image.
- CD-R (the `gdrom` profile) is the only profile that cuts quality to fit. Cuts apply only in `gdrom` mode. Every cut is written in the build doc with the MB it saves.
- Halo numbers:
  - CD-R 800 MB = 405,000 sectors. Keep 3 % free = 392,850 sectors.
  - Disc 2 was 36.9 MB over the CD-R before cuts.
  - Cut `map_mono` (mono map music): 92.5 MB saved, all modes.
  - Cut `music_rates` (22050 Hz for root songs): 61 MB, plus 64 MB for the four OUTPOST songs.
  - Cut `voice_rates` (cutscene voice 33 kHz, dialogue 16 kHz): 14.6 MB.
- Rule: `PLAY_MODE` is saved once. The bake record stores the cuts. A mode switch makes the verifier refuse audio baked with other cuts.
- Rule: memory and speed cuts apply in every mode. Quality cuts only in CD-R mode.

## 4. Test strategy

### 4.1 Runner and shards

- The generic shard rule: run the full suite in 10 shards at once, with hidden workers. Each worker has its own log and its own TEMP folder. Split the modules by size so the shards finish together. Then run the serial list.
- Reference: dc-halo's runner is `tools/run_tests.py`. Its serial list is `tools/test_serial.txt` (five modules in dc-halo). Each line gives the reason.
- Wrong: a serial run of the whole suite took 48 min. Fix: 10 shards.
- Under load, some modules fail only when the shards run together (F-SUITE-LOAD-ARTIFACT):
  - a MemoryError in zlib decompress (audio audit);
  - `build_cinematics` failing only in the shard run;
  - `make -n` on the disc targets timing out at 300 s under 10-way load (57 s alone with KOS_BASE set). Put such a module on the serial list with its number. Do not raise the timeout (QA session, suite gate commit 4c98c004).
- Rule: a MemoryError in one module reruns only that module, alone, and the report says so.
- Scope rule: a small change runs the modules it touches. The full suite runs before an image build and before a merge.

### 4.2 Environment keys

- Export only the keys the runner exports from the config file: KOS_BASE, KOS_CC_BASE, SOURCE_ISO (the source disc path; dc-halo called it HALO2_XISO).
- Wrong: an absolute output folder in the environment (DC_OUT) made four makefile tests fail. Those tests expect the default folder.
- Wrong (#131): tests read only the environment variable and raised "not set". Fix: one config reader (environment first, then the config file), and a named error that names the key.
- A git worktree has no config file (it is git-ignored). Tests there skip and name the missing key. Do not copy the config into the worktree.

**Rule for the next port:** one config reader used by the launcher, the builder and the tests.

### 4.3 Stale pins vs regressions

- Stale pin: a test pins a commit hash, a count or a byte value that moved on purpose.
  - Fix: re-pin in the NEXT commit, test-only, and write the measured reason in the pin's comment.
  - Halo history: 24 "baseline re-pin" commits for one test, each after a level-code change. A comment-only change does not move the pin.
  - Example: a fix made a fallback walk hit 389 blocked places instead of 214. The ratio pin moved from 3x to 2x, with the measured reason in the comment (commit da5b6e4c).
- Regression: a behaviour pin fails and the change was not meant to change behaviour.
  - Fix: find the commit (git bisect on the harness), then fix the code or the harness. Do not just move the pin.
  - Example (#166, closed): nine base-tree tests failed before any new change. Each failure was bisected to its commit before fixing.
- Rule: a pin change says "stale" or "regression" in its commit message. Never both.
- **Pin the guard and the call, not the neighbour line** (QA session, #379 and #399; code session, #393 insert). A source-text pin breaks when a patch inserts a line next to it. Pin the order of calls, not a character window. If a window is needed, give it slack and a comment naming who moved it, and make the failure text name the commit that moved the pin.
- **Pins that quote source text break when a patch edits that line** (reviewer session, #169 and #371). Check every pin that quotes a changed line in the same change.
- **Identity baselines move on purpose; re-pin on the lead's word** (reviewer session, test_mp_guest, #392 and #397). A wrong baseline is not a code failure. Re-pin in a test-only commit, never inside a review.
- **A fallback path's numbers can move by one after a correct fix** (code session, #395 v2: AiWalkOldMover 50 to 49). Measure base against fix leg by leg, write the reason next to the new pin, and keep the shipped path's numbers identical.

### 4.4 C host harnesses

- Many C test harnesses (`tests/*_test.c`) compile with the host gcc and run on the PC. About 490 harness files at the time of writing.
- Wrong: Git Bash gcc fails silently. Fix: put MinGW `bin` FIRST on PATH, or use the shim that `tools/setup_cc_venv.ps1` sets up.
- Wrong: the `-m4-single-only` fallback hid a compiler crash (an ICE in the cold-size level build, b11). Fix: compile with the real flags (`-m4-single`). The Makefile refuses the fallback.
- Rule: the host harness proves behaviour; the real KOS compile proves the code builds for the target. Run both for any touched object.
- Numbers: one collision harness file ran 16 tests in 33-39 s on the host.
- **Harness stub lists live in one place** (QA session, tip 63a1ee3a). New statics in level.c (campaign_arena_cap, sfade, autosave_why) broke three host harnesses that stub it. Every host test that compiles game source with stubs keeps its stubbed names in one list. A new static in the file means one grep for the stub lists in the same commit.
- **Regex-extracted helpers need every new name in the list** (code session, #404 and #406: test_vehicle_draw, test_scripted_impostors). A host test that extracts functions from the big level file by regex needs every new helper in its list, and its fixture needs every new local. Keep extracted helpers small and self-contained, and update the harness list in the same patch.
- **Size counter arrays from the enum, never a literal** (code session, CAMP_SOUND_AT = 40 against seen_events[40] in campaign_test.c). A new event type left the literal-sized array silently wrong. Size arrays by the last enum value + 1 and guard with it.

### 4.5 Paths on MSYS2 and Windows

- Pass one path form per tool. The tests helper reads MSYS-style paths (a drive letter after a leading slash) as native paths (commit 1008eae6).
- Wrong (commit cac6fa91): the OpenCE pin check used `rev^{commit}`; MSYS2 git inside make brace-expanded it and the check failed. Fix: use `rev^0`.
- Rule: test the pin and path helpers under both MSYS2 and native Python.
- Rule (`tests/test_no_local_paths`): committed text never holds a local drive path, a user name or a scratch path. Use a config key or a placeholder. Add this test at the start of a new port.

### 4.6 Golden and lock tests

- Golden and lock tests guard displays and shared code. Run them whenever shared code changes (a10 golden, bridge isolation, displays lock in dc-halo).
- A display lock test compares output bytes, not intent. It replaced a whole-map byte compare when it was narrowed to the displays.

### 4.7 Checks that pass for the wrong reason

- **A clean syntax check is not a build** (QA session, #393 v1). A "syntax check" passed on an empty log from a toolchain with missing DLLs, and the reviewer sent it back. Check a change only with the real ELF build, and read the return code, never an empty log.
- **A tree scan scopes itself** (QA session, test_quiet_proc_322). The scan walked tools/ and read pip, numba and PIL inside the bundled venvs. Scan tracked code only, and skip site-packages and every venv by name.
- **Temp dirs stay out of the repo, or git is told to stop** (QA session, test_not_a_checkout_says_so). A temp dir inside the repo made a "not a repo" test find the parent checkout. Tests that need "not a repo" set the git ceiling to the temp parent.

### 4.8 Scripted edits and patches

- **A CRLF and LF mix breaks a scripted edit** (QA session: four patch builds failed assert-replace). Load each edit script with newlines preserved, normalise them, and restore them. Grep for broken string literals: a heredoc turned a backslash-n inside a C printf into a real newline three times. Build the backslash with chr(92).
- **Rebuild patch hunks against the current tip** (QA session, #394 row, #340 neglit). Patches built from an older archive overwrote other sessions' tracker rows and code. Rebuild the tracker hunk from the live tip just before sending, check it with `git apply --check` on the tip, and rebase code hunks with `git apply`, never by copying a whole file.

## 5. Collision and physics bake lessons

- Thin planes (#375): near-duplicate collision planes (a 0.0004 wu sliver) were read as solid in the hull test. The Warthog hit an invisible wall in a tunnel.
  - Fix (commit a804a62e): test the far child at the exact crossing t = d1 / (d1 - d2), as the original engine does.
  - Regression fix (commit 62f9af90): near-duplicate planes are read only at the crossing, with a band of 0.0005 wu and a margin of 0.0001 wu. The margin is a known risk; re-check it on the real map.
  - Rule for the next port: an epsilon or snap change is tested on the real map crossings, not only on a synthetic sheet.
- Phantom ceiling (#395, commit 9d318817): the far-solid walk made a ramp under a thin overhang solid. The vehicle could not climb it.
  - Fix: the mid walk may only CLEAR (make air), never make solid, unless the original engine does.
  - Cost: text +56 B, RAM/VRAM 0.
  - Rule: a bake-time walk never invents solid space. Add the real route as a test.
- Box tests per frame (Halo Boot A vs Boot B): 1,264-23,384 box tests per frame before the grid change, 0-136 after. Next step (#381, coded): reject by surface box before reading vertices.
- Vehicle collision crawl (#373, open): the Warthog run slowed to a crawl while collision tests ran. Measure each pass before changing one.
- Compile flags per file (#137): `-O3` on the collision files cost +18,112 B text; collision was 29 % of the a50 frame. Turned on by default after a measured A/B. The big level file failed to compile at -O3 in one study (only -O2 built), so flags are set per file.
- Independent oracle: a hand port of the walk (`bipmove_oracle.c`) compared 12,000 moves per BSP. Mismatches were 0.01-0.04 %, all at grazing contacts. Use an oracle when a rewrite touches physics.
- Probes and phases (#382, #383): see PERFORMANCE.md section 6.
- **Measure the candidate rule on the real data before shipping it** (QA session, #375 option A). The 13 thin-solid collision pairs were a no-op because the pairs sat inside solid; a second variant moved 2 pass-to-hit. Run the candidate on the real collision data first, report the hit and pass table, and drop it with evidence rather than ship a fix that changes nothing.
- **Check the floor probe against the map, not the arena centre** (QA session, #398). The kill floor was the lowest probed ground minus 4, but a cliff pit counted as ground, so the floor sat 150 wu under where the player plays. A "lowest ground" rule needs a per-map table of real heights (tests/outpost_killz_test.c has a 23-circle table) and a case for a pit inside the circle.
- **Drive the real code over the shipped asset** (code session, #395 ramp_drive, tests/collision_sliver_375_test.c). Rays alone lie. Every collision fix gets a drive-it-through host test on the built BSP, not a synthetic box only.
- **Near-duplicate planes: read them at the crossing, and check the raw source** (code session, #375 d40f tunnel wall, #395 9d318817). Source planes a fraction of a wu apart read as phantom walls or ceilings under a fixed epsilon. Before you blame the map for a stuck spot, check it against the raw source BSP.

## 6. Launcher, editor and build doc: where things live

- Build doc: `docs/BUILD.md`. Every cut, every gate, every rule for images.
- Build rows: `docs/BUILDS.md`. One row per image.
- Tracker: one file (`docs/plans/FEATURE_GAPS.md`). Each change updates its row in the same commit. No row = not finished.
- Issues: one GitHub issue per bug with severity, area, and status labels. Comment on each step (find, diagnose, commit, boot result). Close only after the user's boot OK.
- **Check a hash before you cite it** (lore session, the no-owner issue audit, #323 and #30). Two issue comments cited wrong commit hashes, and one cited a commit that did not touch the code it named. Before a hash goes into an issue, check that the commit exists and touches the file you name.

## 7. Operations rules (hidden, logs, deletes, scratch)

- Hidden workers: every test, bake and build child runs with no window. Output goes to its own log file.
- Serial logs: one folder, one name per run (date, version, build, time).
- Never delete: no `rm`, `Remove-Item`, `del`, `git clean`, `worktree remove --force`, `branch -D`. Ask first with the exact path and size.
- Trash, not Bin: finished scratch is MOVED by rename into a trash folder on the data drive. The Recycle Bin on the dc-halo PC had a cap (about 7.8 GB) and evicted files for good. The user empties the trash folder.
- Scratch: all on the data drive, one folder per task. No junction or symlink into a shared folder. Temp copies use `git archive` or a worktree with real data. Rebuild a scratch tree from a clean archive plus the whole third_party set and the config (code session, #395 reruns: missing sh4zam source, invader schema, config). Section 4.2 keeps the config out of a worktree; a clean archive copy needs it, so copy it in. Diff a failure against the base build before calling it the patch's fault.
- Worktrees and branches: one integration tree. Sessions reuse it; they do not make copies (disk space). Branch switches are the lead's job.
- Reparse points: refuse to move or delete a folder that is a junction or symlink.
- **Never edit a shared checkout while someone commits** (graphics session, commit 82e97f2f: a `commit -a` swept half-done edits into an unrelated commit). Each session works in its own exported copy and hands over a patch that applies on the tip. Only the lead commits.

## Start a new port (from the dc-halo tool notes)

The dc-halo tool notes described the source port's repo. The new repo's own tools differ, so check each item against them.

- Tools in the new repo: `tools/fresh_deps.py`, `tools/build.ps1`, `tools/build_assets.py` (a generic example asset step: textures through pvrtex, music through wav2adpcm), `tools/sh4zam_lib.mk`, `tools/cfg.py`, and `tools/lod/*` (generic LOD tools).
- Source reader: one per source format. Write it as a step that `tools/build_assets.py` calls. The source disc is never a repo file. Keep it out of git.
- No source asset goes into git: the owner supplies the source files (SOURCE_ISO, or SOURCE_EXTRACT for files taken from the disc, outside the repo), and the tools convert at build time. Keep builds, outputs, `*.cdi` and `config/config.local` out of git.
- Budgets: 16 MB RAM, 8 MB VRAM, 2 MB AICA. Gate: load end under 2 MB free RAM = send back. Texture upload failure above 0 = send back.
- Record your own current costs (RAM, VRAM, AICA, disc size, ELF size) in the tracker at the first boot. Do not copy dc-halo's numbers.
- Source movies (video in the source format): decide early whether to convert frames to a still or skip them. Park the decision in the tracker.
- Disc: the source disc may be far bigger than the CD-R target. The CD-R cut table (section 3.6) is the first real budget. Plan it early, not at the end.
- Reference projects (other ports, recompilers): check the licence before you copy code from a GPL project.
- Start the tools in this order (from the Halo lessons): the test that bans local paths, the one config reader, the bake-chain table, then the build self-checks.
