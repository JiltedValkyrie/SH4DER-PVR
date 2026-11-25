# Loading screens

Scope: every <Game> build. Source of the numbers: the Halo Dreamcast port (dc-halo). File paths below are relative to the dc-halo repo root unless they name another repo. Checked against dc-halo on 2026-11-15.

Labels: **MEASURED** (a boot or serial log), **CODE** (a constant or a line in the source), **ESTIMATE** (our arithmetic, not a boot), **GUESS**.

Generic version: replace the stage names in section 7 with your own load steps. Your engine's load call goes where dc-halo's `loading_show` sits (see section 2).

## 1. The rule (keep it in every project)

1. **The load screen redraws at most every 500 ms.** Between redraws the CPU does the loading. Every call still runs the stream and audio tick. (CODE: `src/loading.c`, `LOADING_DRAW_MS 500`.)
2. **The text sheet is held only while the textures fill.** It is 32 rows, 32 KB of VRAM. It is released before the fit pass, so nothing is held after the load. (CODE: `src/level.c` calls at lines 4052 and 4135, and 4279 and 4327 for the second path.)
3. **No disc read inside a frame.** Loads run inside the loading call. Reads go in 64 KB chunks, and the audio tick runs after each chunk. (CODE: `src/loading.c` `loading_read`.) See the open item in section 6: this is a goal in gameplay, not yet proven there.
4. **One stage line per step**, plus the asset name, and a bar. The stage name prints to the serial log once per stage. (CODE: `loading_show`, `stage_hook`.)
5. **Keep it simple.** One flat backdrop, two untextured triangles, one bar, a few lines of text. No pulsing gradient, no animation on the screen.

## 2. Why the redraw rate matters (MEASURED)

| Build and load | Whole load | Time inside the loading screen | Screens | Source |
|---|---|---|---|---|
| b15, a30_a to a30_b (first throttle missing) | 49,059 ms | 41,262 ms (84 %) | 1,381, about 30 ms each | `docs/PERF_NOTES.md`, entry 2026-11-01 (F141-LOAD) |
| The same load, disc and parse only | 7,797 ms | not on screen | not drawn | same entry |
| d20 v53b1 (200 ms throttle) | about 6.7 s | 459 to 601 ms | 22 to 25 | `docs/plans/FEATURE_GAPS.md`, F-LOAD-SCREEN-500 |

Each drawn screen is two frames that wait for vsync, plus a wait for the render to finish (`progress_draw` in `src/loading.c`). That wait is the cost. The fix is to draw less often, not to draw less text.

Throttle history:

| Value | Tracker row | Status |
|---|---|---|
| 200 ms (first throttle) | F141-LOAD | DONE 2026-11-01, the owner's boot was good |
| 500 ms (now) | F-LOAD-SCREEN-500 | DEPARTURE from OpenCE (OpenCE's progress bar draws every frame; the reason is the Dreamcast load stall). CODED 2026-11-02, boot check (LOADTIME screen row) still listed as owed |

Note: the comment in `src/loading.h` still says 200 ms. The code says 500 ms. Fix the comment when you touch that file.

ESTIMATE: at one drawn screen per 500 ms and 30 ms per screen, the screen takes about 6 % of the load time. At 200 ms it took about 15 %. The b15 log showed 84 % with no throttle.

## 3. How the screen works (`src/loading.c`)

| Function | What it does |
|---|---|
| `loading_show(stage, asset)` | Sets the stage and asset text, counts one step for the bar, then calls `progress`. Prints the stage change once to the stage hook (LOADRAM rows). |
| `progress(done, total, known)` | Runs the stream tick on every call. Draws only when a draw is forced or 500 ms have passed since the last draw. |
| `loading_show_now(stage, asset)` | Draws at once. Use it only for a message that nothing follows: a prompt that waits for the player, a note read during a sleep. |
| `loading_read(file, offset, data, bytes)` | Reads in 64 KB chunks. Runs the tick after each chunk. With `DISC_UNBUFFERED`, chunks end on a 2048-byte sector boundary (`src/discio.h`). |
| `progress_draw` | Submits the screen twice, then waits for the render to finish before it frees anything. |

Overall bar: fraction = loaded / (loaded + 3 MB). Loaded = bytes read + 64 KB for each stage step (CODE: `overall_fraction`). A gap of more than 3 s between calls starts a new session, and the session ends with one serial line.

## 4. Text sheet (VRAM and RAM)

Each text line is ARGB4444, 512 pixels wide, so bytes = 512 x rows x 2.

| Rows | VRAM held | RAM staging per draw (freed after each draw) | Lines on screen |
|---|---|---|---|
| 128 | 128 KB | 128 KB | title, stage, asset, banner or note |
| 64 | 64 KB | 64 KB | title and one line |
| 32 | 32 KB | 32 KB | the title or the stage line only |
| 0 | 0 | 0 | no text, bar only |

- Hold: `loading_hold_text_sheet()` before the texture fill. Release: `loading_release_text_sheet()` before the fit pass. Forget (no free): `loading_forget_text_sheet()` when the PVR is shut down. (CODE: `src/loading.c` lines 130 to 145.)
- Step down: if a texture malloc or a staging malloc fails, the sheet steps 128 to 64 to 32 to 0 for the rest of the session. One line prints each time: `LOADING: text sheet N rows did not fit (VRAM free K KB)`. The choice only ever steps down inside one session.
- RAM after the load: 0 for the sheet. (The staging buffer is freed on every draw.)

## 5. Look (G7 in the tracker)

- Backdrop: the last menu page's flat ink. FEET FIRST uses 0x050a12, DOSSIER uses 0x141c26 (CODE: `mw_wash_theme`). Two untextured triangles, no texture, no RAM, no VRAM.
- Title: the level's name at the menu header size, at the left margin. Boot, credits and other screens print LOADING.
- Bar: a 2 px track at the list column's width, with a 2 px accent fill. Square ends.
- Message (a prompt or an error): note-size lines under the bar. One message line replaces the title on a short sheet.
- Tracker: F-LOADING-G7 (CODED 2026-11-03). Cost (F-FF-PASS2 row, the same change): `loading.o` -236 B text and +12 B bss; `disc_prompt.o` +5 B. No VRAM and no RAM after the load.

## 6. Limits and open items

- Short sheet: "B: BACK TO MENU" and the plain-load banner need the 128-row sheet. Once the 32-row held sheet takes over late in a load, a notice such as "SAVE FROM ANOTHER BUILD" does not show (F-LOADING-G7 limit).
- Disc reads in play: F126b is OPEN as instrumentation. Dialogue, music refills, lazy sound loads, and saves read the disc on the game thread, and the rows showed hitches of 500 ms or more with no known cause (10-31). So "no disc read inside a frame" is the rule, but the gameplay path does not yet meet it. Do not claim it is met until a boot row shows it.
- Boot checks to run on every load-time change: the `LOADING` line (screens, skipped, worst ms, text rows, least VRAM free); the `LOADTIME screen` row (drawn, skipped, tick, vsync wait, draw); a load with a forced message must still show its text.

## 7. Stage names

| Load step | Stage line | Asset line |
|---|---|---|
| Level or area | Loading <area> | the area's file or id |
| Models and animation | Loading model and animation | the model's name |
| Textures | Loading texture index | the pack's name |
| Sound | Loading sound index | the bank's name |
| Prompt waiting for the player | Waiting for player | none (use `loading_show_now`) |

Halo's real stage names (examples from dc-halo `src/level.c`): Loading level geometry, Loading model and animation, Loading collision, Loading texture index, Loading in-engine cutscenes, Loading voices, Loading music.

## 8. Checklist for a new project

1. Put one loading call at each load step, with a stage name and an asset name.
2. Throttle the redraw to 500 ms. Give prompts and error notes their own forced draw.
3. Hold the text sheet around the texture fill only. Release it before the fit pass.
4. Read files in chunks of 64 KB or less, and run the audio tick between chunks.
5. Print one summary line per load session, not one per screen. Serial at 115200 baud costs about 94 ms per KB (dc-halo `AGENTS.md`, MEASURED on the port).
6. Boot test: a load prints its summary line, the bar moves, and a forced message shows its text.

## Sources

- `src/loading.c`, `src/loading.h` (dc-halo)
- `src/discio.h` (dc-halo)
- `src/level.c` lines 4052 to 4327 (the text sheet hold and release) (dc-halo)
- `docs/PERF_NOTES.md` entries 2026-11-01 (F141-LOAD) and the throttle note (dc-halo)
- `docs/plans/FEATURE_GAPS.md` rows F141-LOAD, F-LOAD-SCREEN-500, F-LOADING-G7, F121, F126b (dc-halo)
