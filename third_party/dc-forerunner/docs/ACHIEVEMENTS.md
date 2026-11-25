# Achievements, toasts and the profile page

Scope: every <Game> build. Source of the numbers: the Halo Dreamcast port (dc-halo). Paths are relative to the dc-halo repo root unless they name another repo. Checked against dc-halo on 2026-11-15.

Labels: **MEASURED**, **CODE**, **ESTIMATE**, **GUESS** (see the same labels in [LOADING.md](LOADING.md)).

Achievements are an addition. OpenCE and the original game have no achievements and no toasts (`src/profile.h`, `src/profile_save.h`, GAME_GUIDE "ACHIEVEMENTS and SKULLS"). Keep that in mind when you copy the design to a game that already has achievements on another platform.

Generic version: fill the list in section 8 from your game's achievement source. The look, the fanfare, the toast timing, the page layout and the save design below carry over unchanged.

## 1. The rule (keep it in every project)

Keep these identical in every project:

1. **The toast**: place, size, colours, fade times, one at a time, first in first out.
2. **The fanfare**: one sound per toast, played when the toast appears, not when it is queued.
3. **The page**: list plus card, locked rows show `???`, earned rows show `DONE`, a block count at the header and at each block header.
4. **The save**: two files, one block each, a CRC, a generation number, one write per change set.
5. **The hold**: a toast never pops during a load or a cutscene that the player did not start.

Only the list of achievements changes: the names, the one-line asks, the points if you use them, and the counts.

## 2. The toast (pop-up)

Menu pixels, 640 x 480 (`src/level.c`, `profile_overlay_draw`; constants `PRF_TOAST_*`).

| Item | Value | Source |
|---|---|---|
| Place | x 344, y 84: top right, under the shield meter | `PRF_TOAST_X`, `PRF_TOAST_Y` |
| Size | 280 x 34 | `PRF_TOAST_W`, `PRF_TOAST_H` |
| Slab | one wash, alpha 99 of 255 (39 %); FEET FIRST colour 050a12 with a 3 px left tick in 8fb8d8; DOSSIER colour 141c26 with a 1 px rule above (e8eef4) and a 1 px hair below (7f97b0) | `profile_slab` |
| Name | left, scale 0.8, white (f4f7f9 FEET FIRST, ffffff DOSSIER), squeezed down to 0.45 to fit | `profile_text_fit` |
| Count | right, scale 0.55, colour 7f97b0. Campaign: `n / 24`. OUTPOST and MULTIPLAYER: `n / 11` for its own block. A skull: `SKULL FOUND` | `profile_overlay_draw` |
| Timing | fade in 150 ms, hold 4000 ms, fade out 300 ms, no slide. Total 4450 ms | `PRF_TOAST_IN_MS`, `PRF_TOAST_HOLD_MS`, `PRF_TOAST_OUT_MS` |
| Queue | one at a time, first in first out. A skull found shows first, then the achievements in order. Up to 16 waiting | `prf_toast`, `PRF_TOAST_MAX` (`src/profile_save.h`) |
| Hold | a mission's toasts wait until the next level's gameplay runs: 90 game ticks (3 s at 30 Hz), no cutscene, no pause. Every other unlock shows at once, over a cutscene too | `PROFILE_TOAST_HOLD_TICKS` 90; tracker F-ACH-TOAST-HOLD (#261, #369), CODED 2026-11-10, boot owed |

GAME_GUIDE says the toast stays "four seconds". The hold is 4 s; the fades add 0.45 s more (CODE, see the table above).

Cost of the toast: RAM 8 B for the hold flag and tick (F-ACH-TOAST-HOLD row), VRAM 0, AICA 0. Code: `level.o` text +228 B for campaign, +184 B for match (same row).

## 3. The fanfare

- One sound per toast. It plays on the frame the toast appears: `sfx_play(pickup_snd[3], 255, 128)` in `profile_overlay_draw` (`src/level.c`).
- Sound used by dc-halo: `sound\sfx\ui\grunt_birthday_party` (the owner's own clip). Baked about 6 dB under the mix (the comment in `src/level.c`; not re-measured).
- The same clip plays when a Grunt Birthday Party skull is used on a headshot (`src/level.c`, around line 33699). A reward sound that means "something new" should be one clip only, used for every reward.
- Volume 255, pan centre (128). No second sound for skulls.

Generic: one fanfare clip for every toast. Record its size in your audio doc. Measure, do not guess.

## 4. The SAVING corner

- Shown while the profile is written: bottom right, 120 x 34, at x 504 and y 430. One word, `SAVING`, colour 7f97b0 (CODE: `profile_overlay_draw`).
- Shown at least 500 ms, so it never flickers. The write runs one frame after the corner is drawn (`prf_saving_drawn`).
- The toast and the corner never overlap: the toast is top right, the corner bottom right.

## 5. The achievements page (EXTRAS > ACHIEVEMENTS)

Code: `page_achievements` in `src/menu_draw.c`. Reached from the Extras menu. Menu row count: `PAGE_ACHIEVEMENTS` (`src/menu.h`).

| Part | What the player sees |
|---|---|
| Header | `ACHIEVEMENTS` and the count at the right: `n OF 24` for the campaign rows |
| List, campaign block | one row per shown achievement, in bit order (24 rows) |
| List, OUTPOST block | a header row `OUTPOST` with `n OF 11` at its right, then 11 rows |
| List, MULTIPLAYER block | a header row `MULTIPLAYER` with `n OF 11`, then 11 rows |
| Total list | 48 rows (24 + 2 x 12). The page list holds up to 64 rows (`PRF_PAGE_ROWS_MAX`) |
| Earned row | the name in white, `DONE` at the right |
| Locked row | `???` in a dim colour, the right side empty |
| Selected row | a theme tick at the left. No bar on either theme (the tick only: `md_tick_only`) |
| Card under the list | the name of the selected row and its note. A locked row shows what it asks, so the player knows what to do |
| Footer | `B BACK` only |

Skulls page (EXTRAS > SKULLS): found skulls show their name and ON or OFF. Unfound skulls show `?`, faint, and the cursor skips them.

Count rule: the campaign count is the earned rows divided by the shown rows (`profile_earned_count` over `profile_shown_count`). Shown means shipped: a row that has no hook in this build is not listed (PRF_SKULL_SHIPPED, PRF_EGG_SHIPPED in `src/profile.h`).

## 6. The rows (dc-halo)

Campaign block, 24 rows in bit order (`src/menu_draw.c` `prf_text`; the asks are the page's own notes):

| Row | Ask (note on the card) |
|---|---|
| Pillar of Autumn | Escape the Pillar of Autumn. |
| Halo | Set foot on the ring. |
| Truth and Reconciliation | Rescue Captain Keyes. |
| The Silent Cartographer | Find the map room. |
| Assault on the Control Room | Reach the control room. |
| 343 Guilty Spark | Meet the Monitor. |
| The Library | Take the Index. |
| Two Betrayals | Destroy the pulse generators. |
| Keyes | Find the Captain. |
| The Maw | Destroy the Pillar of Autumn. |
| Finish the Fight | Finish the campaign on any difficulty. |
| Fresh Out of Cryo | Finish the campaign on Easy. |
| Birth of a Spartan | Finish the campaign on Normal. |
| Believe in a Hero | Finish the campaign on Heroic. |
| Living Legend | Finish the campaign on Legendary. |
| Bandanna | Find the Bandanna skull. |
| Grunt Birthday Party | Find the Grunt Birthday Party skull. |
| Mythic | Find the Mythic skull. |
| Boom | Find the Boom skull. |
| Famine | Find the Famine skull. |
| Piñata | Find the Piñata skull. |
| Headhunter | Find every skull. |
| Time Paradox | Find the corrupt file in the ONI Database. |
| It's Thinking | Unlock every other campaign achievement. |

Not shown in this build (no hook yet, so no row): the five egg rows (Megg, Madrigal Debut, No Smoking, Dry Throat, T-Rex) and the Iron skull. The skull and egg masks are in `src/profile.h`.

OUTPOST block, 11 rows (`prf_text2`): Suit Up (Earn your first armour.), Second Skin (Earn your second armour.), The Long Watch (Clear every contract.), Hold the Door (Win a firefight on Legendary.), The Weld Holds (Win every firefight on Legendary.), A Room with a Door (Open your first base.), Load Bearing (Fully upgrade a base.), Forward Outpost (Fully upgrade every base.), Millionaire (Earn a million supply.), Got to Know You (Reach full trust with a teammate.), Crush Crush (Grow close to a teammate with romance on.).

MULTIPLAYER block, 11 rows: Lock and Load, Slayer, Capture the Flag, King of the Hill, Oddball, Race, Killtacular, Running Riot, Full House, Grand Tour, How Did We Get Here? (asks as in GAME_GUIDE "ACHIEVEMENTS and SKULLS").

Generic: keep the three blocks if your game has the same split. Map each story chapter to one row, each collectible type to one row, and one "all of them" row per set.

## 7. Save storage

| Item | Value | Source |
|---|---|---|
| Files | `HALOPROF` and `HALOPRF2` on the first memory card. Two files, one block each (two blocks in all) | `src/profile_save.h` header |
| File size | 64 B: a 16 B header and a 48 B body. 26 B of body written now. Package with its 128 B header: 192 B | `src/profile_save.h` (PRF_FILE_BYTES, PRF_BODY_BYTES) |
| Block | a block is read as 512 B (`PRF_READ_CAP`); the package pads the data to it | `src/profile_save.h` |
| CRC and generation | CRC-32 covers the version, the used length, the generation and the body. The newest good file wins; a write goes to the file that does not hold the newest good data, so a power cut leaves the other file whole | `src/profile_save.h` |
| Write rule | one write for each change set, after the SAVING corner has been drawn. Never one write per toast | `src/profile_save.h`, WRITE RULES |
| Failure | a failed write keeps the flag set. Three failed writes in play stop the retries until a menu is open (`PRF_MAX_FAILS`) | `src/profile_save.h` |
| Separate from the campaign save | the profile is never in the campaign save, and a new game does not clear it | `src/profile_save.h` header |
| Cost | RAM: toast queue 16 B, hold flag and tick 8 B. VRAM 0. AICA 0 | tracker F-ACH-TOAST-HOLD |

Rule: the profile is permanent. A new game does not reset it (`src/profile_save.h` header).

## 8. The list for a new project

Fill this table from your game's source. Keep the columns.

| Bit | Name | Ask (one line, what the player does) | Block |
|---|---|---|---|
| 0 | <name> | <ask> | story |
| ... | ... | ... | ... |

Generic rows: fill from your game. A row without its unlock code is not listed.

## 9. Rules that are not up for a copy

- A cheat, a debug row that changes gameplay, or an assist that changes the rules stops earning. dc-halo sets `debug_cheating` and no achievement is earned (`src/profile_save.h`). A skull that is on in a Halo game also stops earning, except the Grunt Birthday Party skull, which is for fun and never stops one (GAME_GUIDE).
- Toasts never draw over a load screen (F-ACH-TOAST-HOLD).
- A toast that waits keeps its fanfare for when it shows (FIFO).
- Points: each row has a point value in the table (`prf_text`, `prf_text2`), but dc-halo has no caller that shows them (grep of `src/`, 2026-11-15). Do not build a score on them until a caller exists.

## Sources

- `src/level.c` `profile_overlay_draw`, `profile_map_won`, the `PRF_TOAST_*` and `PROFILE_TOAST_HOLD_TICKS` defines (dc-halo)
- `src/profile_save.h`, `src/profile.h`, `src/profile_vmu.c` (dc-halo)
- `src/menu_draw.c` `page_achievements`, `prf_text`, `prf_text2`, `md_tick_only` (dc-halo)
- `docs/GAME_GUIDE.md` "ACHIEVEMENTS and SKULLS" (dc-halo)
- `docs/plans/FEATURE_GAPS.md` rows F-ACH-TOAST-HOLD, F-SAVING-LINE (dc-halo)
