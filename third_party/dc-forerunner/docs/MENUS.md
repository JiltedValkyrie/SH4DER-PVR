# Menus: settings, presets, notes and debug pages

Scope: every <Game> build. Use this layout as the template for a new project's menus. Source of the numbers: the Halo Dreamcast port (dc-halo). Paths are relative to the dc-halo repo root unless they name another repo. Checked against dc-halo on 2026-11-15.

Labels: **MEASURED**, **CODE**, **ESTIMATE**, **GUESS**.

Generic version: the page names below are dc-halo's. Keep the shape (page list, row types, presets, notes, debug pages), and rename the pages to your game's.

## 1. The rule

Start every new project from this layout: the same page list order, the same row types, the same preset-to-setting mapping, the same note rules, and the same debug gate. Only the names and the values change.

## 2. Page map (dc-halo)

**Main menu** (page ids `MAIN_*` in `src/menu.h`): CAMPAIGN, MULTIPLAYER, FIREFIGHT, GALLERY (the model viewer, named ONI DATABASE in the game), DISCOGRAPHY, SETTINGS, CREDITS, RECORDINGS (THEATRE), EXTRAS.

**EXTRAS** (`EXTRAS_*` in `src/menu.h`): ONI DATABASE, DISCOGRAPHY, THEATRE, ACHIEVEMENTS, SKULLS. Achievement pages are in [ACHIEVEMENTS.md](ACHIEVEMENTS.md).

**SETTINGS** (`SROW_*`, labels in `src/menu_draw.c`, line 3557): LANGUAGE, SUBTITLES, CONTROLS, VIDEO, AUDIO, CONSOLE, THEME, MASTER CHIEF, RESTORE ALL DEFAULTS, DEBUG (DEBUG shows only on dev images, section 7).

**VIDEO tabs** (`VIDEO_TAB_*` in `src/menu.h`): DISPLAY, CAMERA, DISTANCE, OBJECTS, EFFECTS. The L and R triggers move between tabs (`INPUT_TAB_PREV`, `INPUT_TAB_NEXT`). The tab table is `video_tab_rows` (`src/menu.c`).

**Other pages with a fixed job**:

| Page | Job | Source |
|---|---|---|
| Video confirm | a resolution change waits until the player picks it | `PAGE_VIDEO_CONFIRM`, `video_pending` |
| Defaults confirm | KEEP (the default) or RESTORE | `PAGE_DEFAULTS_CONFIRM`, `defaults_choice` |
| Wide test | a live test screen; it reverts after 10 s unless the player accepts (`WIDE_TEST_MS` 10000) | `src/menu.h` |
| Experience (first start) | picks a preset: PERFORMANCE or BALANCED, two rows | `PAGE_EXPERIENCE`, `src/menu_draw.c` around line 3903 |
| Theme, scheme and filter asks | first-start questions | `PAGE_THEME_ASK`, `PAGE_SCHEME_ASK`, `PAGE_FILTER_ASK` |

## 3. Page layout (list and card)

Source: `docs/archive/ui/MENU_LAYOUT.md` (dc-halo, layout dated 2026-10-22; not re-measured for this doc). Menu pixels, 640 x 480.

| Role | Font and size | Colour |
|---|---|---|
| Header | INTERSTATE at 150 %, accent rule under it | 0xafe4fc |
| Item in the list | LARGE at 90 %, row pitch 28 px | 0xa5c3d6 |
| Selected item | LARGE at 90 %, accent bar and glow | 0xf0faff |
| Value in the list | SMALL, right aligned, only if it fits. Longer values go to the card | 0x8cdcff selected, 0x708ea4 otherwise |
| Card name | LARGE, up to two lines | 0xafe4fc |
| Card value | INTERSTATE at 100 % (LARGE if too wide). Accent chevrons when LEFT or RIGHT change it | white |
| Card options | SMALL chips, the current one lit | |
| Note | SMALL, wrapped to the card | 0xbed6e6 |
| Help strip | SMALL with button chips, full width at y 408 | |
| Footer | SMALL, faint | 0x50697d |

Geometry: list x 40 to 272; card x 288 to 614; card top 140; card bottom 400; help strip from 408; the action safe line is 456.

Rules from the same doc: chips never wrap (one row, spacing and size shrink in steps); a list row whose value does not fit beside its label puts the value on a second line, so the row does not change height while the value changes.

## 4. Row types

| Row type | Example in dc-halo | How it behaves | Source |
|---|---|---|---|
| Option step | OBJECT DISTANCE | LEFT and RIGHT step through the values and stop at the ends. A (accept) steps forward and wraps from the last value to the first. The row marks each end (`MENU_END_FIRST`, `MENU_END_LAST`) | `step_option`, `ends_at` in `src/menu.c` |
| Chips | the card's choice chips | every value in one row; the gap and text shrink in steps | `docs/archive/ui/MENU_LAYOUT.md` |
| Toggle | FAR PROPS (Off, On) | two values, the same step rule | `src/menu.c` |
| Preset row | QUALITY | picks a preset and sets many rows at once. Changing any one of those rows after that reads CUSTOM | `video_preset_match`, section 5 |
| Action | RESTORE DEFAULTS | leads to the confirm page | `PAGE_DEFAULTS_CONFIRM` |
| Confirm | video resolution | the change is held until the player chooses | `video_pending` |
| Live test | WIDE test (768 x 480) | reverts after 10 s unless accepted | `WIDE_TEST_MS` |
| Session-only slider | FAR ENEMY CAP, ENEMY FADE START (debug) | not saved; its note says `Not saved.`; default equals the release rule | `src/menu.h` lines 198 to 213 |
| Info row | a live read-out (CARVE START shows the light level where the player stands) | read-only; the note is the live text | `src/menu_draw.c` around line 1240 |

Every row's note is computed in `src/menu_draw.c` (one `switch` on the row id, setting `label`, `note` and `value`).

## 5. Presets: how a preset maps to the options

dc-halo has four preset rows in the table and two active presets:

- `VIDEO_PRESETS` = 4 rows in `video_presets` (`src/menu.c`). `VIDEO_PRESETS_ACTIVE` = 2: PERFORMANCE and BALANCED. The owner's spec of 2026-11-12 removed FIDELITY and XTREME from the menu. The two old rows stay so that an old save reads as BALANCED (`src/menu.h` lines 575 to 581).
- Each preset is a full `video_preset_t` struct: resolution, draw distance, object detail, particles, flash objects, detail distance, bump distance, the reflection rows, object lighting, shadows, anti-aliasing, object distance, armour detail, vehicle particles, cutscene detail, far props, water, mip bias, eye adapt, and (when the FX subset is built) bloom, rays and lens rows.
- `video_preset_match_n` compares every field of the live setting with each active preset, in order. A match returns the preset number. No match returns CUSTOM. Resolution is ignored at 768 x 480 (the preset keeps its name). Some fields accept an "any" value (`VIDEO_*_ANY`) while an old save is migrated.
- `video_apply_options` (`src/level.c`) reads the same values at level start and on every resume.
- Default preset: BALANCED (`VIDEO_PRESET_DEFAULT` 1).

Example, PERFORMANCE and BALANCED on the rows that matter for the look (`src/menu.c` `video_presets`, CODE):

| Row | PERFORMANCE | BALANCED |
|---|---|---|
| Draw distance | DYNAMIC (STRONG), 30 to 90 % | DYNAMIC, 50 to 90 % |
| Object detail | DYNAMIC | DYNAMIC |
| Detail distance | 9 wu = 27 m (index 3) | 16 wu = 49 m (index 5) |
| Object distance | 16 wu (index 1) | 16 wu (index 1) |
| Particles | 32 | 96 |
| Vehicle particles | 50 % | 50 % |
| Anti-aliasing | off | 2X |
| Far props | on | on |

Rule (`AGENTS.md` in dc-halo): when a preset value or a side effect changes, update the menu note, `docs/GAME_GUIDE.md` (the "Video settings" preset table) and `docs/CONTROLS.md` in the same change. `menu.c video_presets` and `video_apply_options` must agree.

Guide status (2026-11-15): `docs/GAME_GUIDE.md` and `docs/CONTROLS.md` still describe four presets in places. The code has two. The fix is in progress in dc-halo. The guide must list two presets: PERFORMANCE and BALANCED.

Generic: two presets to start: a Performance preset and a Balanced default. Add a third only with an owner decision and a cost line (RAM, VRAM, ms).

## 6. Menu notes (what the player reads)

Rules, from dc-halo `AGENTS.md` and `docs/archive/ui/MENU_WORDING.md`:

1. **WHAT, never WHERE.** The note says what the setting does.
2. **One short sentence.** About 60 characters; 100 at most. Cut, never add.
3. **No model, map, tag, pass or engine name. No mechanism.** The player does not need the technique.
4. **Do not repeat the option name** in its note.
5. **Numbers the player can match on screen only**: seconds, %, metres, on or off. No triangle or vertex counts. No world units (wu). Metres = wu x 3.048 (`metres()` in `src/menu_draw.c`).
6. **Player words only.** No map codes (A10, D20_START). Level and enemy names as the game prints them. No spoilers.
7. **No ranking words in a new note** ("slowest", "worst"). State the cost as a fact: "Uses more CPU". The owner's 2026-11-10 ruling: "Recommended" and "Best at" are recommendations, not judgements, and existing notes stay unless the owner names one.

Examples from dc-halo, with their length in characters (CODE):

| Setting | Note | Length |
|---|---|---|
| OBJECT DISTANCE | Props, vehicles and allies fade past this. | 42 |
| OBJECT DETAIL (LOW) | Models use their lightest version. | 34 |
| OBJECT DETAIL (DYNAMIC) | The enemy you aim at is shown in full detail. | 45 |
| CUTSCENE DETAIL | The heroes in cutscenes. MEDIUM and up: Cortana glows. | 54 |
| FAR PROPS | Trees and rocks stay to 152 m. OFF: they fade with objects. Next load. | 70 (over the target: it is a two-sentence note, kept as is; do not copy the length) |
| FAR ENEMY CAP (debug) | Most idle enemies drawn, furthest go first. Not saved. | 54 |

Two more rules for notes that carry numbers:

- Put the number the player sees in the note: "Trees and rocks stay to 152 m." (the same value as the row's far clip).
- Say when a change takes effect: "Next load." or "Quit to the title to change it." Keep the tense plain.

Generic: use your game's units. Keep the rule that a note never says where a setting lives in the code and never names an engine.

## 7. Debug pages

**The gate.** Debug pages exist only in dev images (`menu_dev_image`). The title code (UP UP DOWN DOWN UP UP UP UP on PRESS START) opens SETTINGS > DEBUG for one session. A normal boot hides it (`docs/GAME_GUIDE.md`, "Debug (dev images only)").

**The tabs**: INFO, RENDER, COMPARE, TOOLS, SKULLS (`VIDEO_TAB_INFO`.. `VIDEO_TAB_SKULLS`).

| Tab | What is on it (examples) |
|---|---|
| INFO | FAR ENEMY CAP and ENEMY FADE START (session-only sliders); DEBUG INFO; the live read-outs |
| RENDER | the flashlight beam and lamp tuning rows (session tuning) |
| COMPARE | switches that turn one look off to compare: enemy hide, far trees, far rocks, balanced near set, sphere shadows |
| TOOLS | teleport, next map, god, freeze, give shield or camouflage, the theatre pick, direct render |
| SKULLS | a developer override for one skull |

**Row kinds on the debug pages:**

1. **Info row**: read-only. Its note is a live read-out (`CARVE START`).
2. **Tuner**: a step row that changes how the game draws or plays, for testing only.
3. **Session-only slider**: not saved. Its note says `Not saved.` Its default is the release rule, so an untouched slider changes nothing (`src/menu.h` lines 198 to 213: FAR ENEMY CAP defaults to OFF, ENEMY FADE START to 80 wu).
4. **Serial log row**: switches printing for the session (below).

**Rules**:

- A debug tool must not silently change gameplay (`AGENTS.md` in dc-halo, "OpenCE is bible"). A row that changes gameplay must say so in its note, and it stops achievements for the session (`debug_cheating` in `src/profile_save.h`).

**Serial log rows (dev images):**

| Row | What it does (`docs/GAME_GUIDE.md`, debug table) |
|---|---|
| SERIAL LOG | switches between the full log and the quiet log for one session. A PARITYTEST image starts with the full log; a release image starts quiet (`docs/BUILD.md`) |
| MEMORY LOG | a screen of what each loading step left free: main memory, video memory, the sound blocks that did not load |
| UPFAILS | lists every texture, model, buffer or file that failed to load, with its size and the space left, on screen (pages turn every 3 s) and on the serial log |
| LOG ON SCREEN | the last 100 serial log lines on screen, a page every 4 s |

Serial budget (dc-halo `docs/BUILD.md`): the quiet log is about 4 KB of the 40 KB that a map load prints (MEASURED on the a30 and b25 logs). At 115200 baud, 1 KB costs about 94 ms, so per-frame prints are banned.

## 8. Translations

- **English stays at the call site.** The code's English text is the code it ran. `lang_tr(en)` returns the English text when no language is set (`src/lang.c`, `src/lang.h`).
- **Context key.** `lang_trc(ctx, en)` gives one English text two meanings, with a context key (the `ctx` field in `src/lang_en.json`).
- **Formats.** `lang_snprintf` translates the format and each `%s` piece, so a composed line stays in one language.
- **The table.** `src/lang_en.json` lists every menu string: `text`, the source `files`, an optional `ctx` and an optional `blob`. About 4,800 entries (count of `"text"` keys, 2026-11-15). `src/lang_pt.json` and `src/lang_ru.json` hold the other languages (about 5,000 keys each).
- **The key.** `tools/lang_strings.py` bakes the table. The key is FNV-1a 32 of `ctx`, then 0x1F, then the text (the text alone when there is no ctx). The build fails on a collision.
- **The language code** is stored in the settings block (HALOCFG byte 364, `SETTINGS_SIZE_LANG`). An unknown code reads as English.
- **Rule for new text:** add it to `src/lang_en.json` with its files in the same change. Never change an English string without a re-bake of the other languages.

Departure (dc-halo, tracker F-LANG-TABLE): OpenCE has no string table. A language is therefore our own table, read from the disc. The English text still comes from the game's own code.

Generic: keep one English-keyed table for menu text and one for dialogue or story text. Each key is the English text plus an optional context.

## 9. Checklist for a new project

1. Copy the page map (section 2). Rename the pages; keep the order.
2. Set up the row types in section 4 once, then reuse them.
3. Write the presets as full rows (sections 5 and 10). Two presets to start: PERFORMANCE and BALANCED (default).
4. Write every note to section 6. Count the characters.
5. Add the debug gate (section 7), the session-only sliders and the serial rows.
6. Put every menu string in the translation table at the call site (section 8).
7. Boot test: each row changes the game, each note matches its number on screen, and the preset label reads CUSTOM after one change.
8. Fill the defaults table (section 11): every settings row, the VIDEO values that no preset sets, RESTORE ALL DEFAULTS, and every DEBUG row.

## 10. Video presets, full table

Labels: **CODE** = read from the dc-halo source on 2026-11-15. **UNVERIFIED** = no number in the dc-halo docs; the cost is not measured here.

Source: `src/menu.c` `video_presets` (lines 400 to 407) holds the rows. `src/menu.h` lines 582 to 588 hold the struct `video_preset_t`: 22 stored fields, plus 3 FX fields in an FX build. `video_apply_preset` (`src/menu.c` lines 455 to 487) writes them into the live settings. `video_preset_match` (`src/menu.c` lines 425 to 447) reads them back. Menu rows and values come from `src/menu_draw.c` (value arrays at lines 786 to 840; row cases at lines 841 to 1150). The first number in a preset cell is the stored value. The text after the colon is what the menu shows.

**Proven baseline.** The values below are the dc-halo values that have been booted and proven. Use the same values in this project. Rename the rows to this game's names.

Preset values are the same for all three versions. A preset is applied with `video_apply_preset`, so anything not in this table is not changed by a preset.

| Setting (`video_t` field) | Menu row | PERFORMANCE | BALANCED (default) | Player-facing value | Cost | Source |
|---|---|---|---|---|---|---|
| `res_index` | RESOLUTION | 1: 640x480 | 1: 640x480 | `< 640x480 >` | No cost in a preset. 768x480 WIDE (not a preset value) loses about a quarter of texture RAM (menu.h comment, WIDE). | `menu.c` 401 to 404; `menu.h` 376 to 377 |
| `draw_distance` | DRAW DISTANCE | 15: DYNAMIC (STRONG) | 16: DYNAMIC | `< DYNAMIC (STRONG) >`, 30 to 90 %, 50 % in a vehicle / `< DYNAMIC >`, 50 to 90 % | UNVERIFIED (the rule holds 30 frames a second; no ms in the docs) | `menu.c` 401 to 402; `menu.h` 388 to 389; `menu_draw.c` 894 to 905 |
| `object_detail` | OBJECT DETAIL | 5: DYNAMIC | 5: DYNAMIC | `< DYNAMIC >`. Note: the enemy you aim at is shown in full detail | UNVERIFIED | `menu.h` 420 to 421; `menu_draw.c` 931 to 933 |
| `particles` | PARTICLES | 0: cap 32 | 2: cap 96 | `< 32 >` / `< 96 >` | Scorch and blood marks: 24 / 8 s / 2 s (Performance) and 64 / 22 s / 5 s (Balanced), `docs/GAME_GUIDE.md` table. No ms in the docs. | `menu.c` 187; `menu_draw.c` 1013 to 1022 |
| `flash_objects` | none (no row) | 1: ON | 1: ON | `ON` on every preset; no setting | "zero frame time measured" (code comment) | `menu.c` 824; `menu.c` 401 to 404 |
| `detail_distance` | DETAIL DISTANCE | 3: 9 wu | 5: 16 wu | `< 27 m >` / `< 49 m >` | UNVERIFIED | `menu.c` 188; `menu.h` 445 to 446; `menu_draw.c` 906 to 910 |
| `bump_distance` | none (no row; a10 monitors screen only) | 0: OFF | 1: 3 wu | 9 m (Balanced); not shown in any menu row | UNVERIFIED. Not part of the preset match. | `menu.h` 101, 449; `menu.c` 371 |
| `object_distance` | OBJECT DISTANCE | 1: 16 wu | 1: 16 wu | `< 49 m >` | UNVERIFIED | `menu.c` 213; `menu_draw.c` 911 to 915 |
| `far_props` | FAR PROPS | 1: ON | 1: ON | `ON`. Note: trees and rocks stay to 152 m; next load | Loads the far-far pak files. Extra RAM not measured in the docs (UNVERIFIED). | `menu.h` (`far_props` comment); `menu_draw.c` 928 to 930 |
| `reflection_environment` | WALL + GLASS SHINE | 0: OFF | 4: LOW | `< OFF >` / `< LOW >` (LOW: shine on glass only) | UNVERIFIED | `menu.c` 402 to 404; `menu_draw.c` 802 to 807, 1001 to 1003 |
| `reflection_vehicles` | VEHICLE SHINE | 0: OFF | 1: CHROME, YOURS | `< OFF >` / `< CHROME, YOURS >` | UNVERIFIED | `menu.c` 402 to 404; `menu_draw.c` 808 to 814, 1007 to 1009 |
| `reflection_armour` | CHARACTER SHINE | 1: LIGHT | 2: FINER, FOES TOO | `< LIGHT >` / `< FINER, FOES TOO >` | UNVERIFIED | `menu.c` 401 to 404; `menu_draw.c` 816 to 822, 1010 to 1012 |
| `object_lighting` | OBJECT LIGHTING | 1: SIMPLE | 3: FULL | `< SIMPLE >` / `< FULL >`. FULL note: lamps and gunfire light characters and walls. Uses more CPU. | "Uses more CPU" (menu note). No ms in the docs (UNVERIFIED). | `menu.h` 474 to 476; `menu_draw.c` 786 to 840 (`lighting`, `lighting_notes`) |
| `object_shadows` | OBJECT SHADOWS | 4: FAIRY | 10: FOGGY HILL (shown as ROCKFORT (LOW)) | `< FAIRY >` / `< ROCKFORT (LOW) >` | No VRAM, no modifier list (code comment on the Balanced shadow default) | `menu.h` 490, 511, 517 to 518; `menu_draw.c` 786 to 840 (`shadow_names`), 940 to 962 |
| `reflection_distance` | REFLECTION DISTANCE | 0: 6 wu | 1: 7.2 wu | `< 18 m >` / `< 22 m >` | UNVERIFIED | `menu.c` 372; `menu_draw.c` 1004 to 1006 |
| `anti_aliasing` | ANTI-ALIASING | 0: OFF | 1: 2X | `< OFF >` / `< 2X >` | About 14 KB of texture RAM for 2X at boot (`docs/PERF_NOTES.md` line 983, a boot figure). 2X doubles the PVR tile width (`docs/PERF_NOTES.md` line 884). | `menu.h` 558 to 559; `menu_draw.c` 1146 to 1148 |
| `armour_detail` | ARMOUR DETAIL | 0: OFF | 1: ON | `OFF` / `ON` | UNVERIFIED | `menu_draw.c` 963 to 965 |
| `vehicle_particles` | VEHICLE PARTICLES | 2: 50 % | 2: 50 % | `< 50% >` | Share of OpenCE's particle count. No ms in the docs. | `menu.c` 186; `menu.h` 439, 361; `menu_draw.c` 1126 to 1129 |
| `cutscene_detail` | CUTSCENE DETAIL | 1: MEDIUM | 1: MEDIUM (the Chief alone is HIGH within about 15 m of the camera; see `docs/GAME_GUIDE.md`) | `< MEDIUM >` | UNVERIFIED | `menu.h` 431; `menu_draw.c` 934 to 936 |
| `shader_water` | WATER RIPPLES | 0: OFF | 1: 25 % | `< OFF >` / `< 25% >` | UNVERIFIED | `menu.h` 350, 362; `menu_draw.c` 971 to 975 |
| `shader_mip_bias` | MIP BIAS (test images only) | 1: 0.75 | 1: 0.75 | `< 0.75 >` in a test image; a release image shows no row | None | `menu.h` (`shader_mip_bias` comment); `menu_draw.c` 997 to 1000 |
| `eye_adapt` | EYE ADAPT (campaign only) | 0: OFF | 1: ON | `OFF` / `ON` | None in the docs | `menu.h` 148; `menu_draw.c` 920 to 927 |
| `fx_bloom_on` | BLOOM (FX build only) | 0: OFF | 0: OFF | `OFF` | UNVERIFIED | `menu.h` (PVR_FX comment); `menu_draw.c` 1035 |
| `fx_rays_on` | GODRAYS (FX build only) | 0: OFF | 0: OFF | `OFF` | UNVERIFIED | `menu.h` (PVR_FX comment); `menu_draw.c` 1036 |
| `fx_lens_off` | CAMERA VIGNETTE | 1: held OFF | 0: the player's choice (ON by default) | `OFF` on Performance (turning it on reads CUSTOM) / `ON` by default on Balanced | None | `menu.c` 483 (`video_apply_preset`); `menu.c` 532 (`video_defaults`); `menu_draw.c` 1037 |
| `campaign_fov` | FIELD OF VIEW | 60 degrees (`video_preset_fov`) | 60 degrees | `< 60 DEG >` | None in the docs | `menu.h` 354; `menu_draw.c` 864 to 867 |

GRAIN is not a preset field. The player's choice holds on every preset (`menu.c` 483 comment; `menu.c` 532).

## 11. Defaults for every setting (CODE)

Start point for a new project. The values are the dc-halo values on 2026-11-15. Every value is a default that the player can change, unless the table says otherwise.

### 11a. Settings page rows (SETTINGS)

Row labels: `src/menu_draw.c` line 3557. Restore behaviour: `src/menu.c` lines 31 to 39 (`defaults`), 155 to 162 (`controls_defaults`), 1021 to 1031 (`video_restore`), 1506 to 1525 (the RESTORE ALL DEFAULTS confirm).

| Row | Default | Steps | RESTORE ALL DEFAULTS | Source |
|---|---|---|---|---|
| LANGUAGE | English (`LANG_EN`, code 0) | the offered languages; LEFT / RIGHT / A step and wrap. Shown only past English and off a level | Kept (not reset) | `src/lang.h` line 14; `src/menu.c` line 48 (`settings_shown`); `src/menu_draw.c` line 3581 |
| SUBTITLES | FOLLOW (ON past English, OFF in English) | FOLLOW / OFF / ON | Back to FOLLOW | `src/menu.h` line 284; `src/menu.c` line 37 |
| CONTROLS | Scheme SOUTHPAW (STICK LOOKS, `SCHEME_FACE_MOVE`); invert Y OFF; look speed 5 (range 1 to 10); fishing rod OFF; rumble 75 % (index 3) | Each per row; rumble OFF, 25, 50, 75, 100, 125, 150 % | Back to these; every scheme's button map reset | `src/controls.h` line 30; `src/menu.c` lines 155 to 162, 788; `src/menu.h` lines 873 to 874 |
| VIDEO | BALANCED preset, plus the values in 11b | The five tabs (DISPLAY, CAMERA, DISTANCE, OBJECTS, EFFECTS); rows in `video_tab_rows` | Balanced and the 11b values. Not on a run (the arena's video stays fixed). | `src/menu.c` lines 522 to 532 (`video_defaults`), 1021 to 1031 (`video_restore`) |
| AUDIO | MUSIC VOLUME 8 (shown 80 %); MENU SOUNDS ON | MUSIC VOLUME 0 to 10 (each step 10 %; 0 shows OFF); MENU SOUNDS ON / OFF | 80 % and ON | `src/menu.c` lines 26 to 28, 772 to 784, 987; `src/menu_draw.c` lines 3588 to 3600 |
| CONSOLE (VMU SCREENS) | Campaign SH-4 build: VMU 1 LOADOUT, VMU 2 CORTANA. Multiplayer ELF and host tests: VMU 1 LEADERBOARD, VMU 2 CORTANA | Per VMU screen (`vmu_screen_step`) | Back to the defaults for that build | `src/menu.c` lines 92 to 98 (`vmu_defaults`), 141 to 146 |
| THEME | FEET FIRST (theme 0) | FEET FIRST / DOSSIER | FEET FIRST. The "asked" answer stays. | `src/menu.h` line 668; `src/menu_draw.c` line 3554; `src/menu.c` (RESTORE branch, `m->theme = 0`) |
| MASTER CHIEF (shown only after the unlock) | First model (`MENU_FLAG_ALT_CHIEF` clear) | Steps through the chief models | First model. The unlock stays. | `src/menu.c` line 36; `src/menu.h` line 858; `src/menu_draw.c` line 3574 |
| RESTORE ALL DEFAULTS | Action. The confirm page highlights KEEP MY SETTINGS first. | KEEP MY SETTINGS / RESTORE ALL DEFAULTS | Not applicable | `src/menu.c` lines 1506 to 1525, 2929; `src/menu_draw.c` lines 3833 to 3837 |
| DEBUG | Dev images only, and not on a run. See 11d. | See 11d | DEBUG INFO back to OFF | `src/menu.c` lines 34 and 52 (`settings_shown`) |

### 11b. VIDEO values that no preset sets

Every value below is set by `video_defaults` (`src/menu.c` lines 522 to 532) and reset by `video_restore` (`src/menu.c` lines 1021 to 1031) unless the row says otherwise.

| Row | Default | Steps | Source |
|---|---|---|---|
| SCREEN SHAPE | 4:3 (`widescreen` 0) | 4:3, 16:10, 16:9. At 768x480: 16:10 and 16:9 only. | `src/menu.c` line 523; `src/menu_draw.c` lines 851 to 855 |
| FRAME RATE | 30 CAP (`VIDEO_FPS_CAP30` 0) | 30 CAP / UNCAPPED | `src/menu.c` lines 1021 to 1023; `src/menu_draw.c` lines 874 to 878 |
| HUD MARGIN | MEDIUM (`HUD_MARGIN_DEFAULT` 2) | NONE, SMALL, MEDIUM, LARGE | `src/menu.h` line 374; `src/menu_draw.c` lines 879 to 881 |
| HUD SIZE | 100 % (`hud_size` 0) | 100, 125, 150 % | `src/menu.c` line 525; `src/menu_draw.c` lines 882 to 884 |
| FILTER | OFF | OFF, CRT (FLATSCREEN), AERIALS, REC ROOM | `src/menu.h` line 371; `src/menu_draw.c` lines 885 to 891 |
| BRIGHTNESS | Shown 50 (step 0) | 0 to 100, 50 = neutral | `src/video_grade.h` line 11; `src/menu_draw.c` lines 868 to 870 |
| CONTRAST | Shown 60 (step 2) | 0 to 100, 50 = neutral | `src/video_grade.h` line 12; `src/menu_draw.c` lines 871 to 873 |
| VIGNETTE | ON | ON / OFF. Performance holds it OFF. | `src/menu.c` line 532; `src/menu_draw.c` line 1037 |
| GRAIN | ON | ON / OFF, on every preset | `src/menu.c` line 532; `src/menu_draw.c` line 1038 |
| AGE | 35 | OFF, then 1 to 99 (`VIDEO_AGE_MAX` 99) | `src/menu.h` line 485 (`VIDEO_AGE_DEFAULT`); `src/menu.c` line 528; `src/menu_draw.c` lines 966 to 970 |
| WEAPON SHAKE | ON | ON / OFF | `src/menu.c` lines 34, 1027; `src/menu_draw.c` lines 916 to 919 |
| Test-image rows (SHADERS_TEST builds only): GRASS, SNOW, WATER DEPTH TEST, FORCE DETAIL, DETAIL FADE | OFF, OFF, OFF, OFF, 70 % | Per row | `src/menu.c` line 1024 (zeroed by `video_restore`); `src/menu_draw.c` lines 976 to 996 |

Rows set by a preset (not in this table): RESOLUTION, DRAW DISTANCE, OBJECT DETAIL, PARTICLES, DETAIL DISTANCE, OBJECT DISTANCE, FAR PROPS, the reflection rows, OBJECT LIGHTING, OBJECT SHADOWS, REFLECTION DISTANCE, ANTI-ALIASING, ARMOUR DETAIL, VEHICLE PARTICLES, CUTSCENE DETAIL, WATER RIPPLES, EYE ADAPT, BLOOM, GODRAYS, VIGNETTE (Performance), FIELD OF VIEW. See section 10.

### 11c. What RESTORE ALL DEFAULTS resets and keeps

Source: `src/menu.c` lines 1506 to 1525. The player picks KEEP or RESTORE. KEEP MY SETTINGS is highlighted first (`src/menu.c` line 2929).

Resets (on RESTORE): every row in 11a except LANGUAGE; every value in 11b; the Balanced preset (`video_apply_preset`); audio; rumble; subtitles; VMU screens; the controls scheme, invert, look speed, fishing rod and every scheme's button map; the Master Chief model; the theme (FEET FIRST); DEBUG INFO.

Keeps: LANGUAGE; the first-start answers (theme, filter, experience and the controls answer are not asked again); the unlocks other than the Master Chief model (what was found stays found); achievements, skulls, level progress, recordings and the other saved files. On a run (`main_mode` 2) the VIDEO values are not reset.

Guide note: `docs/GAME_GUIDE.md` line 33 says the multiplayer name and armour are kept. That is a save rule, not a RESTORE rule, and is not in this table.

### 11d. DEBUG rows (dev images only)

Rule: DEBUG shows only on dev images and never on a run (`src/menu.c` line 52). Every debug default is the release behaviour. An untouched debug row changes nothing. `menu_init` zeroes the whole menu (`src/menu.c` line 540), so an unset debug value is 0, which is the release state. A release image clears the debug bytes on load (`menu_sanitize`, `src/menu.h` comments).

| Tab | Row | Default | Saved? | Source |
|---|---|---|---|---|
| INFO | DEBUG INFO | OFF | Yes | `src/menu.c` line 34 |
| INFO | LOG ON SCREEN, MEMORY LOG, KEEP NAMES | OFF (bits clear) | Yes (settings bits) | `src/menu.h` line 177 (`MENU_DBG_INFO`) |
| INFO | UPFAILS | OFF (`debug_upfails` 0) | Yes | `src/menu.h` (`debug_upfails` comment) |
| INFO | SERIAL LOG | The image's own default: QUIET on a release image, FULL on a PARITYTEST image | No (this session only) | `src/menu.h` (`debug_serial` comment); `src/level.c` line 270 |
| INFO | FORCE RAM LADDER | OFF (`debug_ladder` 0) | Yes | `src/menu.h` (`debug_ladder` comment) |
| INFO | FAR ENEMY CAP | OFF (`menu_enemy_cap` 0) | No (session only; its note says "Not saved.") | `src/menu.h` lines 198 to 213; `src/menu.c` line 221 |
| INFO | ENEMY FADE START | 80 wu (`MENU_ENEMY_FADE_DEFAULT` 2) | No | `src/menu.h` lines 198 to 213; `src/menu.c` line 221 |
| RENDER | DISABLE VEHICLE CARDS, DISABLE ALL CARDS | OFF (`debug_ship_cards`, `debug_all_cards` 0) | Yes | `src/menu.h` (debug comment) |
| RENDER | GRENADE FLEE | OFF on every boot | No (`MENU_DBG_GRENADE_FLEE` is above the saved bits) | `src/menu.h` line 172 |
| RENDER | FLASHLIGHT BEAM, CORE, EDGE, REACH, AMBER, POWER; LAMP FILL, LAMP SWEEP | Step 0 = the release look | No | `src/menu.h` (`flash_tune` comment) |
| COMPARE | HIDE, TREES, ROCKS, BALANCED, SPHERE, PERCEPTION, BOULDER HULLS, REJECT OFF, KEEP ON | Look on (bit clear) | Yes (settings bits) | `src/menu.h` lines 177 to 183 |
| COMPARE | CARVE START, CARVE END | 0.30 and 0.80 (release values) | UNVERIFIED | `src/menu.h` lines 193 to 194 |
| TOOLS | TELEPORT, NEXT MAP, GOD, FREEZE, VIS, LIFT, GIVE SHIELD, GIVE CAMO, THEATRE | OFF (bits clear). The cheat rows stop achievements for the session. | Yes (settings bits) | `src/menu.h` lines 177 to 183 (`MENU_DBG_CHEAT`) |
| TOOLS | LOOK DIAGONAL | 50 % (step 0 = the release look) | No | `src/menu.h` (`VIDEO_LOOK_DIAG_PCT`, `look_diag` comment) |
| TOOLS | DIRECT RENDER | OFF | No | `src/menu.c` line 2757 |
| SKULLS | GRUNT BIRTHDAY PARTY | OFF (`skull_party` 0) | No (session only) | `src/menu.h` (`skull_party` comment) |

## 12. Preset rule (owner's rule, 2026-11-15)

- New projects ship exactly two presets: PERFORMANCE and BALANCED. BALANCED is the default.
- The defaults table (section 11) is the starting point for every setting.
- Any change to a preset value updates three things in the same change: the menu note, the player guide (its Video settings preset table) and the controls doc.

## Sources

- `src/menu.h` (page ids, video rows, preset struct, session-only slider limits) (dc-halo)
- `src/menu.c` (`video_presets`, `video_preset_match_n`, `step_option`, `video_object_distance`) (dc-halo)
- `src/menu_draw.c` (row notes, the settings labels at line 3557, the experience page at line 3903) (dc-halo)
- `src/lang.c`, `src/lang.h`, `src/lang_en.json`, `tools/lang_strings.py` (dc-halo)
- `docs/archive/ui/MENU_LAYOUT.md`, `docs/archive/ui/MENU_WORDING.md`, `docs/GAME_GUIDE.md`, `docs/CONTROLS.md` (dc-halo)
- `AGENTS.md` (menu note rule, debug rule, preset update rule) (dc-halo)
