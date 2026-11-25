# Launcher and map editor for a <Game> port

How the Halo Dreamcast port (repo `dc-halo`) built its player launcher and its map editor, written so your <Game> port can copy the approach. Replace each `<angle bracket>` with your own name.

**How to read this page.** Every claim cites a `dc-halo` file (paths are relative to that repo), a tracker row (`docs/plans/FEATURE_GAPS.md`, IDs such as `F-LAUNCHER-RELEASE`) or a GitHub issue number (`#316`). Status words follow the tracker: DONE, CODED (in the tree, boot not yet proven), OPEN, DEPARTURE. Anything not checked by running code is marked **UNVERIFIED**.

---

## 1. What the launcher is for

The launcher is the player's path from the source to a playable image. The Halo port's rule is six steps, and nothing by hand (`dc-halo` README.md, "Build"):

| # | Step | What the player does | Halo file or row |
|---|---|---|---|
| 1 | Download the source | `git clone` or unpack the ZIP | README step 1 |
| 2 | Run the `.bat` | the batch file checks Python, builds the `.exe` and opens it | `dc-halo.bat`, `tools/build_launcher_exe.py`, F-FIRST-RUN (#317) |
| 3 | Open the `.exe` | from now on, start only this file | `dc-halo.exe` (made per PC, never committed) |
| 4 | SETUP | pick discs, how to play, what to build; each missing tool is offered once | `launcher/launcher.py` (`cmd_setup`, `missing_tools`) |
| 5 | One BUILD button | the whole build, every map at once | `launcher/launcher.py` (`build_steps`, `build_all`), F-LAUNCHER-RELEASE (#316) |
| 6 | PLAY | start the newest image in the chosen emulator, or open its folder | README "Launcher"; `launcher/launcher.py` (`cmd_run`) |

**Rule.** Every manual step is a bug. Each of these was one, and each fix removed the manual step: pymeshlab not installed (#309), the pinned upstream commit missing on a fresh clone (#313), a missing Python 3.12 (#317), a font step never run (#330), a stale bake that told the player to run a command (#329).

**Acceptance test.** One end-to-end walk of the six steps from a fresh clone, with the discs at hand, result logged per step. The owner runs it; the team does not call a build done before it passes (owner note, session memory, not in the repo).

---

## 2. How the launcher is built

### 2.1 Parts and files

| Part | File (in `dc-halo`) | Job |
|---|---|---|
| Entry batch file | `dc-halo.bat` | finds Python 3.12+, offers `winget` install once, makes `dc-halo.exe` on first start, starts the window. Also accepts terminal steps (`dc-halo.bat status`, `build`, ...) |
| Window (view) | `launcher/gui_tk.py` | Tk drawing only: rail with PLAY / BUILD / SETUP / SETTINGS, buttons, dialogs |
| Window (decisions) | `launcher/controller.py` | page models and button rules; no Tk import, so tests run headless |
| Core logic and terminal | `launcher/launcher.py` | config, checks, install offers, build steps, run; `status`, `setup`, `doctor`, `install`, `repos`, `build`, `run` |
| Pure rules | `launcher/launcher_cores.py` | cores a build may use |
| Exe stubs | `launcher/exe/stub.c` (MinGW) and `launcher/exe/Stub.cs` (Windows' own C# compiler) | one source, built four times: `dc-halo.exe`, `dc-halo-cli.exe`, `dce-map-editor.exe`, `setup-repos.exe` |
| Exe builder | `tools/build_launcher_exe.py` | builds the stubs on the player's PC, never committed (`.gitignore`) |
| Build lock | `tools/build_lock.py` | one lock file shared by the launcher and the editor |
| Release path | `tools/release_build.py` | preflight, gates, record, tester image (F-LAUNCHER-RELEASE, #316) |
| Tests | `tests/test_launcher*.py`, `tests/test_first_run_317.py`, `tests/test_launcher_exe.py` | window, controller, build env, updates, exe twins |

Measured: the C# exe build gives 18,944 to 22,528 bytes per exe (F-FIRST-RUN, #317). An unsigned new exe can meet SmartScreen or an antivirus warning, so the exe is built on the player's PC (`tools/build_launcher_exe.py` header).

### 2.2 One config file, found by key

- One file, `config/config.local`, one `KEY=value` per line, git-ignored. Only setup and the window write it (`launcher.py` `save`, `load_file`).
- An environment variable with the same key wins for that run and is never written back to the file (`launcher.py` `load`, `env_overrides`; `docs/BUILD.md` step 3).
- Setup guesses paths from `PATH` and from folders beside the repo (`launcher.py` `guess`: KallistiOS, the compiler, OpenCE, and any sibling `flycast*` or `demul*` folder). It asks only for what it cannot find.
- The Settings page lists every saved path in plain words, with its key in small print (`docs/BUILD.md`, "The window").

Keys the Halo launcher reads (`launcher/launcher.py` `KEYS`, `OPTIONAL`, `LABELS`):

| Key | Meaning |
|---|---|
| `HALO_XISO`, `HALO2_XISO` | the owner's own source discs (both required for Halo) |
| `KOS_BASE`, `KOS_CC_BASE` | KallistiOS and the compiler folder |
| `DC_OUT` | output folder for images |
| `PLAY_MODE` | `emulator`, `gdemu`, `gdrom` or `build` (section 2.4) |
| `BUILD_CONTENT` | `campaign`, `multiplayer` or `both` (section 2.5) |
| `EMU`, `FLYCAST_EXE`, `DC_EMU` | emulator choice and paths |
| `BAKE_RESERVE_THREADS` | cores left free for the rest of the PC (section 2.7) |

### 2.3 Hidden workers, logs, no windows over the screen

- The window runs under `pythonw` (no console). Every child process it starts, and every child of a build step, must not open a console window. `tools/quiet_proc.py` adds `CREATE_NO_WINDOW` to every launch (F-BUILD-SILENT-322, #322, CODED 2026-11-13). Before it, a single build flashed hundreds of terminal windows and took the focus. Measured after the fix: terminal windows 7 to 0 on the probe (`docs/archive/build/SILENT_BUILD_322.md`).
- Every build step writes to a log, not the screen. The window shows the log in its drawer. A window program's error text goes to `dc-halo-logs/launcher-last.txt` beside the repo, and the message names that file (`docs/BUILD.md`, "The .exe launchers").
- Serial logs go to a log folder beside the repo, named `<date>-v<version>b<N>-<HHMM>.txt`, and an existing log is never overwritten (`docs/BUILD.md`).
- Rule (`AGENTS.md`, "Test/build workers hidden"): stdout and stderr to the worker's own log, no console, no error popup.

### 2.4 Play modes: what each one makes

From `launcher/launcher.py` (`MODES`, `MODE_TEXT`) and `docs/BUILD.md` "How to play":

| Mode | What the build makes | Disc layout | Quality |
|---|---|---|---|
| `emulator` | one image, all maps | single image, no swap | full quality |
| `gdemu` | the same image, byte for byte (copy that image to GDemu) | single image | full quality |
| `gdrom` (CD-R) | two images, the game is split between them | disc swap | cuts apply (table below) |
| `build` (Just build) | the same single image as `emulator`, then stops | single image | full quality, same bake record |

**Quality cuts.** Only the `gdrom` mode cuts quality, and only to fit a CD-R. Memory and speed cuts apply in every mode. Every cut is written in the build doc with the MB it saves (`docs/BUILD.md`, "Disc profile"):

| Cut | Modes | Saves | Halo number |
|---|---|---|---|
| `map_mono` | all | the map music is mono | 92.5 MB on a single image |
| `music_rates` | `gdrom` | root songs above 22050 Hz are baked at 22050 Hz | 61 MB, plus 64 MB for four songs |
| `voice_rates` | `gdrom` | cutscene voice at most 33 kHz, dialogue at most 16 kHz | 14.6 MB |

**Rules.**
- The mode is saved once. The bake record stores the cuts. Switching the mode or the cuts makes the verifier refuse audio that was baked with other cuts, and it prints the rebake command (`tools/verify_build_inputs.py`, `tools/assets/disc_profile.py`, `docs/BUILD.md`).
- A CD-R disc size limit is a number, not a guess: 800 MB, 405,000 sectors, with 3 % kept free (`docs/BUILD.md`, "The two release discs").

### 2.5 What to build

`BUILD_CONTENT` picks the half or halves a build makes. The launcher's own sizes (`CONTENT_TEXT`, Halo numbers): campaign about 1,250 MB (or 2 CD-Rs), multiplayer about 300 MB (or 1 CD-R), both about 1,550 MB (or 3 CD-Rs). Missing halves are said on the menu, not hidden (F15.content, `docs/plans/FEATURE_GAPS.md`).

### 2.6 Image names and build number

- Image names are checked by regular expressions in `launcher/launcher.py` (`IMAGE_NAMES`): `halo-dce-v<M.m>-b<N>.cdi` for the campaign, a `-mp<...>` label for multiplayer, `-full-mp<...>` for both. The build number `N` is one counter, read from the build log by `tools/build_number.py` (`docs/VERSIONING.md`, `docs/BUILDS.md`).
- The version (`src/version.h`) is changed by the lead only, from the campaign percent (`docs/VERSIONING.md`).
- **Never overwrite an image.** The launcher refuses to build when a target image or a disc folder already exists, and says the move-away step (`launcher.py` `build_problem`).
- Known doc drift (UNVERIFIED in code): the top table of `docs/VERSIONING.md` still shows the old `v0.3.8` spelling, while the launcher and tracker use the percent spelling (`v0.55`, `MP 0.15`). Check the newest rules before you copy the naming.

### 2.7 The build: steps, gates, cores

**Steps.** `launcher/launcher.py` `build_steps` returns a list of `(label, command)`. It runs them in order and stops at the first failing step with its exit code (`build_all`). Before each step it checks the build lock; in an interactive run, a failed step gets one retry after the recovery offer. The Halo order:

1. prepare the Halo 2 map (the source disc is read here, never copied into the repo)
2. extract and convert the other source maps
3. extract menu audio
4. extract menu graphics
5. the release path (Halo's `emulator` or `both` case) or the compile and image step

**Release path (#316, CODED 2026-11-12).** The button builds the same image the team's tester release uses:

1. `release_build.py preflight`: the tracked tree is clean; the source pins are valid; the hashes of the files a bake must not change are written down.
2. `build-game.ps1 -BakeOnly`: every map at once, one job per map (never one map at a time: #323).
3. `release_build.py gates`: keep-identical (a bake must not change a tracked file, unless its own producer changed), stale-bake check, audio consistency, lock tests, census. It stops at the first failure with one line.
4. `build-game.ps1 ... -Tester`: the image, from the checked bake. The name has no `PARITYTEST`, so the serial log is QUIET.

**Self-checks before the image.** `tools/verify_build_inputs.py` refuses an image built from a stale or wrong asset, and each refusal prints the exact fix command (`docs/BUILD.md`, "Build self-checks"; the font case is #330).

**Stale bake refreshes itself (#329, CODED 2026-11-13).** The gate runs `build_map.py --refresh-stale` hidden, checks once more, and stops only if the map is still stale, with the plain reason. The player never types a command.

**Step stamps (#315).** A step is skipped only when its stamp (source hashes, settings, tool versions) matches and every recorded output has the same bytes (`docs/BUILD.md`, "Bake step stamps"). Otherwise it runs. `DCE_STEP_STAMPS=off` runs every step.

**Cores (#314).** The launcher's Cores number is the only throttle. It is written as `BAKE_RESERVE_THREADS` (logical cores minus cores used). The recommendation keeps 4 cores free from 12 cores up, 2 from 6 to 11, and 1 below that (`launcher/launcher_cores.py`, `docs/BUILD.md`). The number must reach every child: the first version set it on the bake tools only, and the bake used all 32 threads (#314).

**Fresh dependencies (#313, #309).** `tools/fresh_deps.py --ensure` runs before any long step. It fetches the public repos at their pins, and it reports what it cannot fetch in one line. Python packages install without a second question (F-LAUNCHER-PY-DEPS, CODED 2026-11-12).

**Update check.** Opt-in. It looks at the `main` branch at most once a day and applies only a clean fast-forward. It never overwrites the player's changes (`launcher.py` `UPDATE_EVERY`, `apply_update`; `README.md`, "Launcher").

### 2.8 Every failed check says the fix

`launcher/launcher.py` has a `FIX` table: one line per failed check, with the command to run. The terminal `doctor` and the window show the same words. A failed build names the step and the log file.

### 2.9 Play

- PLAY shows the newest image and one big PLAY button. For `gdemu`, `gdrom` and `build`, it opens the folder with the image (README, "Launcher").
- The emulator choice is saved; with two emulators and no saved choice, the terminal asks (`launcher.py` `cmd_run`).
- Flycast's serial console can be saved to a log file. A plain `>` on the stock Flycast exe stays empty, so the launcher runs a console-subsystem copy of the exe, hidden (F-LAUNCHER-SERIAL-LOG, `docs/BUILD.md`).

---

## 3. What went wrong, and the fixes

Issue state is as of 2026-11-15 (checked with `gh issue view`, read-only). All of these are still OPEN on GitHub with the label `needs-boot`: the code fix is in the tree, and the owner's boot closes each one.

| Issue | What broke | Fix in the tree | Tracker row |
|---|---|---|---|
| #309 | launcher skipped the required pymeshlab package; campaign bake failed at BSP | setup checks every required package before a bake | F-LAUNCHER-PY-DEPS (CODED) |
| #313 | fresh clone lacked the pinned upstream commit; the build died late | `fresh_deps.py --ensure` before any long step | F-FRESH-CLONE-DEPS (CODED) |
| #314 | the Cores setting was ignored; the bake used every core | one budget passed to every child | F-BUILD-CORES-CAP (CODED) |
| #315 | the bake reran steps whose inputs had not changed | step stamps; skip only on matching stamp and output bytes (S2 dry run 2026-11-13: 82 would-skip steps, 0 different bytes) | F-BAKE-STAMPS (#315 S1/S2, CODED; check the row for the current skip state) |
| #316 | the launcher built a test path, not the release path | Build calls the release code (preflight, bake, gates, tester image) | F-LAUNCHER-RELEASE (CODED) |
| #317 | first run needed manual installs; no exe was made | `.bat` checks Python, offers winget once, makes the exe; SETUP offers every tool | F-FIRST-RUN (CODED) |
| #322 | a build flashed hundreds of terminal windows and took focus | `quiet_proc` on every child; probe shows 0 windows | F-BUILD-SILENT-322 (CODED) |
| #323 | the launcher built one map at a time | effort choice removed; all maps at once | F-LAUNCHER-ONE-MAP-323 (CODED, by #316) |
| #329 | a stale bake after a model rebake stopped the gate and told the player to run a command | the gate refreshes the stale step itself, checks once | F-STALE-REFRESH (CODED) |
| #330 | the font packs were never built; the image build refused | a stamped font step at the start of the disc stage | F-FONTS-STAGE (CODED) |
| #339 (Halo multiplayer) | the directory config file the online setup needs was not put on the disc; the failure text vanished | the disc build writes it from `DIRECTORY_CONFIG_FILE` (`docs/BUILD.md`, "Build self-checks") | F-ONLINE-SETUP-339 (CODED) |

**Lessons, in the order they cost time.**

1. **One path.** A launcher that builds a different image from the release path hides the bugs the release finds. Make the button call the release code (#316).
2. **Manual steps are bugs.** If a player must type a command, the launcher is not finished (#309, #313, #329, #330).
3. **Child windows steal focus.** Any child started without the no-window flag shows a console for a split second. Test with a window probe, not by eye (#322).
4. **Stale data refreshes itself, or it refuses with the reason and the one fix.** Never a silent skip, never a warning that is read by no one (#329, #330; the self-check rule in `docs/BUILD.md`).
5. **A setting that does not reach every worker is ignored.** Pass one budget to all of them (#314).
6. **A missing build step ships a broken image.** A file the game reads at run time must be in the list of build steps, and the build must check for it (#339).
7. **Show no time left.** Show the step number and the elapsed time (`Step 4 of 9`), never a time left (`F-LAUNCHER-WINDOW`).
8. **Approval flags live in code.** Example: the multiplayer image is labelled as a Parity test image until the owner approves it (`MP_APPROVED` in `launcher/launcher.py`).

---

## 4. The map editor

### 4.1 What it edits

The editor edits the **per-object choices** of a built level or cutscene. It does not edit the source files. It shows what the port's own build contains (`docs/archive/build/DCE_MAP_EDITOR.md`, intro). The choices, each saved as a profile row (`docs/archive/build/DCE_MAP_EDITOR.md`, sections named below):

| Choice (what the user sees) | Profile field | What the port does |
|---|---|---|
| Show as: 3D model / flat card / automatic | `mesh`, `card` | impostor card or model ("Representation") |
| Detail: automatic / always high / always low | `lod` | per-object LOD (`src/placement_lod.h`) |
| In the game (untick = removed) | `disabled` | the row is left out of the build |
| Draw: off (still in game) | `draw_off` | the object exists, its drawing is skipped (`.pl` byte 29, bit `0x40`) |
| Visible up to (metres) | `cull_distance` | drawing stops past that distance (`.pl` bytes 30-31) |
| Hide in cutscene | `cutscene_hidden` | a cutscene does not draw it; gameplay still does |
| Hero shadow | `hero` | shadow volume forced on or off for that actor |
| Move / offset | `offset`, `yaw_offset` | added to the placement position and yaw |
| Glow | `halo` | a feathered disc drawn per placement (`.pl` bytes 32-47) |
| This light affects (five boxes) | `light_mask` | bits 8..15 of the light's flags in `bsp/<name>.dyl` |
| Camera keys (eye, angles, field of view, tick) | `cameras`, `camera_inserts` | applied to the compiled cutscene events |

Each row has a section in `docs/archive/build/DCE_MAP_EDITOR.md`, with the exact bytes and the departure it makes.

### 4.2 How edits are saved: the editor profile

- **Working copy.** Every edit autosaves to a working file. Undo is one step per edit; a dragged number is one step (`docs/archive/build/DCE_MAP_EDITOR.md`, "Workflow").
- **Save.** One click, no questions. It writes an **immutable version** with an automatic name, and pins it as the version the next game build uses. "Save as named version" does the same with a name. "Versions" loads an old one or picks which one the build uses. "Reset to source defaults" resets the working copy and touches no saved version (`docs/archive/build/DCE_MAP_EDITOR.md`, "Versions, pin, dirty").
- **Dirty and stale.** *Unsaved* = the working copy differs from the pinned version. *Stale* = the scenario assets were rebuilt and a row no longer matches (same doc).
- **Files.** Under `assets_editable/dce-map-editor/` (git-ignored): `project.json`, `working.json`, `versions/<id>.json`, `defaults/<hash>.json` (`docs/archive/build/DCE_MAP_EDITOR.md`, "Files"; `.gitignore`).
- **Settings.** In the operating system's settings store (the Windows registry), not in the repo. `MAPEDITOR_SETTINGS_FILE` moves them to a file (`docs/archive/build/DCE_MAP_EDITOR.md`, "Troubleshooting").
- **The owner's saved choices are the owner's decision.** Clicking a box in the editor is the decision; the review does not ask again. The review still requires a DEPARTURE row worded as "saved profile" with its reason, and a harm check (script hangs, display locks, RAM and VRAM budget) (owner rule, session memory `feedback_editor_choices_no_yes`, not in the repo).

### 4.3 How edits reach the disc

```text
editor: Save  ->  version pinned (profile file)
editor: Build game (Ctrl+B)  ->  saves if needed, opens the launcher's BUILD page, builds nothing itself
launcher: BUILD  ->  bake: the profile step writes the pinned profile into the build
bake: converters write placement rows (.pl bytes), cinematics.bin, dynamic-light flags
launcher: compile + image  ->  one new numbered image
```

- The editor never copies or promotes an image, and it never boots an emulator (`docs/archive/build/DCE_MAP_EDITOR.md`, "Install and run" and "Workflow" step 7).
- The launcher is the one build path: the editor's **Build game** only opens it (F-EDITOR-BUILD-LAUNCHER, #316 / #317, CODED 2026-11-12). Command equivalent: `tools/assets/build_map.py --map <map> --only profile-bake` (`docs/archive/build/CUTSCENE_EDITOR.md`).
- Danger: a build that skips the profile step (`SkipProfileBake`, `PROFILE_BAKED=1`) ships the **unsaved** file. The editor warns; save before you use those switches (`docs/archive/build/DCE_MAP_EDITOR.md`, "Versions").
- One build at a time: the editor's **Bake cards** and the launcher's build share the lock file `build/build.lock` (`tools/build_lock.py`). A lock left by a crashed process is taken over.

### 4.4 Departures from the original are tracked

- Every editor choice that departs from the original game gets a tracker row: `DEPARTURE` with its reason and the smallest adaptation (`docs/plans/FEATURE_GAPS.md`). Examples in the Halo tracker: `F-DRAW-OFF-DEPARTURE` (drawing skipped: performance, owner yes 2026-11-08), `F-EDITOR-LIGHT-MASK-299` (the original picks lights by distance and brightness only), `F-OFFSET-RANGE-288` (offset reach raised to +-500 wu).
- The editor lists each departure as info in its Issues dock, so a build never carries one by accident (`docs/archive/build/DCE_MAP_EDITOR.md`, "Dreamcast tuning").
- The departure table has three columns that every row fills: **constraint**, **smallest adaptation**, **remaining difference** (`docs/archive/build/DCE_MAP_EDITOR.md`, "Dreamcast departures").
- The editor's OpenCE trace lists the source file and function for each fact it shows (`docs/archive/build/DCE_MAP_EDITOR.md`, "OpenCE trace"). Write the path in the change, not from memory.

### 4.5 Tools and UI

| Piece | Halo choice | File |
|---|---|---|
| Window | PySide6 (Qt) | `tools/mapeditor/app.py` |
| Viewport | PyOpenGL, OpenGL 3.3 core; 2D plan view if GL fails | `tools/mapeditor/viewport.py`, `render/` |
| Headless core | no Qt, no OpenGL: state, catalog, timeline, camera, formats | `tools/mapeditor/state.py` (`EditorState`), `catalog.py`, `formats.py` |
| Bake runner | `QProcess` steps with live JSON progress; cancel kills the process tree | `tools/mapeditor/bake.py` (`BakeRunner`) |
| Layout | Unreal Engine 5 style: viewport centre, Outliner, Details, Budget and Issues dock, timeline for cutscenes | `app.py` docstring |
| Budget readout | triangles, vertices, chunks, models, estimated ms; red over 6,000 triangles; labelled ESTIMATE | F-EDITOR-VIEW-COST-319 |
| Launch | window program under `pythonw`; console children made quiet; packages checked first, one install offer | `dce-map-editor.bat`, `app.py` `install_quiet_children`, F-FIRST-RUN (#317), F-BUILD-SILENT-322 |

Tests run offscreen: `tests/test_mapeditor_*.py` (core, state, UI with `QT_QPA_PLATFORM=offscreen`, render, quiet children).

### 4.6 Editor lessons

1. **The editor is a front end on the files the build reads.** It shows what the console draws, not a second reading of the source (`docs/archive/build/DCE_MAP_EDITOR.md`, intro). Keep one reader.
2. **Show the game's own numbers.** Position in world units, facing in degrees, and the object ID the serial log uses. That makes a bug report match the log (`docs/archive/build/DCE_MAP_EDITOR.md`, "Position readout"; F-EDITOR-OBJECT-IDS).
3. **Choices are data rows.** Most fixes became profile edits that need a bake and no code change. Gameplay code changes only for a new kind of choice.
4. **Mark estimates.** The view cost is counted from data, so it is labelled ESTIMATE (`F-EDITOR-VIEW-COST-319`).
5. **Round trip test.** Save, bake, compare bytes. Add it on day one, so a profile that does not survive the bake fails a test, not a boot.
6. **Never a silent refusal.** A card bake refused for a missing prerequisite lists the missing file and the command, and launches nothing (`bake.py` `plan_bakes`, `prerequisite_text`).

---

## 5. Build yours: a checklist for a new port

Tick each item before you call the launcher or the editor done. Halo reference in brackets.

- [ ] **One config file**, git-ignored, `KEY=value`, one reader for every tool. Environment variables win for one run and are never written back. [`launcher/launcher.py` `load`]
- [ ] **Only config keys hold machine paths.** Add a test that fails on a local drive path, a user name or a scratch path in committed text. [`tests/test_no_local_paths.py`, `AGENTS.md`]
- [ ] **Fresh dependencies** fetched at their pins before any long step; a one-line report for what cannot be fetched. [`tools/fresh_deps.py`, #313]
- [ ] **One build entry point.** The launcher's Build calls the release code. No second, test-only path. [#316]
- [ ] **Six steps.** Source, `.bat` (or script) that makes the exe, open the exe, SETUP, one BUILD, PLAY. [README "Build"]
- [ ] **Checks before the build.** Missing package, missing disc, missing tool: each stops the build with one line and the fix. [`build_problem`, `FIX`, #309]
- [ ] **Stamp every bake step.** Skip only when stamp and output bytes match. [`tools/assets/step_stamps.py`, #315]
- [ ] **Gates on the image.** Keep-identical, stale check, audio consistency, memory gates (RAM, VRAM, sound RAM in KB). [`tools/release_build.py`, `docs/BUILD.md` "Release path"]
- [ ] **Stale refreshes itself, then re-checks once.** No "run this command" text. [#329]
- [ ] **Play modes** as data: one saved mode; quality cuts only for the disc that needs them; a bake record that stops a stale mix. [`docs/BUILD.md` "Disc profile"]
- [ ] **Image names** with a version, a build number from the build log, and never an overwrite. [`launcher.py` `IMAGE_NAMES`, `docs/VERSIONING.md`]
- [ ] **Hidden children.** Every child process has no console window and writes its own log. Check with a window probe. [`tools/quiet_proc.py`, #322]
- [ ] **One lock** for the launcher, the editor's bake and any other writer of the build folders. [`tools/build_lock.py`]
- [ ] **Cores as one number** passed to every child. [`launcher/launcher_cores.py`, #314]
- [ ] **Editor edits data only.** Versions are immutable; one pinned version feeds the build; unsaved work never reaches a build. [`docs/archive/build/DCE_MAP_EDITOR.md` "Versions, pin, dirty"]
- [ ] **Every editor departure from the source** gets a tracker row: DEPARTURE, reason, smallest adaptation, remaining difference. [`docs/plans/FEATURE_GAPS.md`]
- [ ] **Offscreen tests** for the window and the editor state. [`tests/test_mapeditor_*.py`]
- [ ] **Acceptance:** one fresh-clone walk of the six steps, result logged per step, on the real source disc. [README; `docs/plans/FEATURE_GAPS.md` F-FIRST-RUN]

---

## 6. Status and what is not verified

- **Launcher and editor in `dc-halo`:** the code is in the tree. The fixes in section 3 are CODED and need the owner's boot. The fresh-clone walk has not passed yet (issues #309 to #339 open on GitHub, label `needs-boot`).
- **This page** was written by reading the `dc-halo` files. No launcher, editor or test was run for it.
- **UNVERIFIED:** the exact UI wording beyond the docs quoted; the claim that a fresh clone now builds end to end (not proven until the walk passes).

**Main sources in `dc-halo`:** `README.md`; `dc-halo.bat`; `launcher/launcher.py`; `launcher/controller.py`; `launcher/gui_tk.py`; `launcher/launcher_cores.py`; `launcher/exe/stub.c`; `tools/build_launcher_exe.py`; `tools/quiet_proc.py`; `tools/build_lock.py`; `tools/release_build.py`; `tools/mapeditor/app.py`, `state.py`, `bake.py`; `docs/BUILD.md`; `docs/VERSIONING.md`; `docs/archive/build/DCE_MAP_EDITOR.md`; `docs/archive/build/CUTSCENE_EDITOR.md`; `docs/archive/build/SILENT_BUILD_322.md`; `docs/plans/FEATURE_GAPS.md` (rows named above).
