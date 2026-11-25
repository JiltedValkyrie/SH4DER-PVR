# Versioning and images (<Game> port)

How this port sets its version, build number and image names. Written so a new port can copy the rules.

Sources are the Halo port, dc-halo, unless marked otherwise. Examples marked **Halo example** are real dc-halo rows. Examples marked **plan** are not built yet.

## 1. The version is campaign percent

- The version is how far the port is through the original campaign, in percent: `v0.PP`, where `PP` is the percent, two digits.
  - `v0.00` = 0 %, `v0.55` = 55 %, `v0.86` = 86 %. **Halo example.**
  - `v1.00` = the whole campaign playable from start to end. 1 % steps.
- Spell the version as the percent itself. Halo first wrote it as a triple, `0.8.6` (percent = 100 x MAJOR + 10 x MINOR + PATCH), and switched to the two-digit spelling on 2026-11-03 (dc-halo `docs/VERSIONING.md`, section "Spelling"). Both spellings are the same number and share one build counter. A new port uses the percent spelling only.
- Store it in `src/version.h`.
  - Halo keeps three ints (`HALO_VERSION_MAJOR`, `_MINOR`, `_PATCH`) and one helper that turns them into the percent (dc-halo `tools/build_number.py`, `version()`). The triple only works while MINOR and PATCH stay 0 to 9.
  - **Plan:** a new port keeps one `<GAME>_VERSION_PERCENT` int and skips the triple.
- A second mode (multiplayer, for example) has its own percent. See section 6.

## 2. What counts toward the percent

Counts:
- original campaign levels beaten from start to end on a real image. "Completable" does not count. Beaten needs one full play to the end;
- original scripted hooks and cinematics that are wired and played, in the original's order.

Does not count:
- departures from the original (features the original did not have), unless they sit inside a counted level;
- side modes, menus, options, editor, extras, debug tools, launcher, tests and docs. dc-halo's version rule names FIREFIGHT, OUTPOST, menus, editor, extras and tools as adding nothing (`AGENTS.md`, "Version and image");
- code that is coded but not booted. Counts 0. Halo's log rows from 2026-11 count it 0 (`docs/archive/campaign/CAMPAIGN_PERCENT.md`). Halo's first version rule (2026-10-26) gave coded work half weight. Superseded: Parity ruling 2026-11-15 (owner gave Parity the call): only booted-and-proven work moves the percent.
- an unreached finale or credits. Held as unpaid until someone reaches them (Halo `CAMPAIGN_PERCENT.md`, row 2026-11-15).

The percent never goes down.

## 3. Who decides

- **Lead** is the only role that edits the version constants in `src/version.h`. No other session or person changes them.
- Each commit that could move the percent, Lead asks two reviewers the same question: "what % of the campaign are we through?"
  - **Scholar** (lore and campaign research, no code) answers one line with a reason.
  - **Parity** (original-code reviewer) answers one line with a reason.
  - A reason names the levels beaten, the original hooks wired and the cinematics played.
- Lead logs both answers and the call in `docs/archive/campaign/CAMPAIGN_PERCENT.md`, one row per commit: date, commit, Scholar %, Parity %, Lead call, version.
- **Lead's call** is the average of the two answers, as a whole percent. It never goes below the last logged value.
  - Round DOWN (owner ruling 2026-11-15). Older Halo rows rounded ad hoc (76.5 became 77 on 2026-11-09).
  - Halo took the lower of the two answers until 2026-11-05. After that the owner ordered the average.
  - The owner's boot result can move the call above the average. Halo's 2026-11-07 row called 63 over a 62 / 62 average, on the owner's word that the first beatable level was campaign progress. Log the reason.
- Cadence and rounding: Owner ruling 2026-11-15: ask Scholar AND Parity at every IMAGE BUILD (not every commit); Lead's call = their average ROUNDED DOWN, never below the last logged value.

## 4. Build number

- One counter per version: the number of images made at that version. A version bump restarts it at 1.
- Plain decimal, no leading zeros: `1`, `12`, `103`.
- Only a successful image moves it. A commit, a failed build, a test run, or a build that wrote no new file does not.
- Record: `docs/BUILDS.md`, one row per image: version, build, commit, image name, sha256, date, notes (branch and what changed).
- Tools (dc-halo `tools/build_number.py`):
  - `next` prints the number the next image gets;
  - `name` prints the next image file name;
  - `record <image>` appends the row. It refuses a second row for the same version and build.
- `tools/build-game.ps1` calls `record` only when make exits 0 and writes a new image file. The row's hash comes from `build/version_stamp.h`, which the Makefile writes.
- Every build, Lead states one of two things: "build increment (same version)" or "version bump". A bump is its own commit, made before the image, with the percent in the commit message (for example `Version 0.86`).

## 5. Image names and rules

Name: `<game>-v0.PP-b<N>.cdi`. **Halo example:** `halo-dce-v0.86-b1-full-mp0.33-PARITYTEST.cdi`.

| Image | Name | Who gets it |
|---|---|---|
| Campaign, tester | `<game>-v0.PP-b<N>.cdi` | testers |
| Test image for review | the name above with `-PARITYTEST` as the last part | reviewers only, never testers |
| Second mode only | `...-b<N>-mp<0.PP>.cdi` | label only (section 6) |
| Both halves | `...-b<N>-full-mp<0.PP>.cdi` | as above |

Rules:
- Build only from a clean committed tree. No `-dirty` in the build string. A dirty image is never given out (dc-halo `docs/VERSIONING.md`, "What a tester build must be").
- Never overwrite an image. Name a new file. dc-halo's MP disc target refuses any name that is not numbered and any file that already exists (`docs/BUILD.md`, "MULTIPLAYER test image").
- A name with a second-mode part keeps `-PARITYTEST` until the owner approves that mode.
- Tag a tester build with an annotated local tag, `v0.PP-bN`, on the built commit. Never move or delete a tag that was given out (dc-halo `docs/VERSIONING.md`, "Tags").
- Agents never boot the emulator. The owner boots. (dc-halo `AGENTS.md` says the same; its reviewer exception is Halo-specific.)
- An image is "built", never "verified", until the owner says it boots.

## 6. Second mode percent (multiplayer)

- Own percent, written `MP 0.PP` (Halo: `MP 0.15` = 15 %). Constants: `HALO_MP_VERSION_*` in `src/version.h`.
- Lead alone sets it. It never goes down. Only booted-and-proven work counts; coded-not-booted counts 0.
- Lead's call is the average of Scholar's and Parity's MP percent (dc-halo `docs/archive/multiplayer/MP_PERCENT.md`). The basis is a milestone table whose weights sum to 100 (`MP_PERCENT_BASIS.md`).
- It is a label in image names and on screen. It is not a build counter. Build numbers count images at the campaign version only.
- Work on the second mode does not move the campaign percent.
- Halo's 2026-11-10 MP row (33) was a Lead call on the owner's order, not the average. It is logged that way.

## 7. What the screen shows

- Campaign pages: `v0.PP b<N>` in the footer hint row, bottom right, dim. **Halo example:** `v0.86 b1`.
- Second-mode pages and the pause over a match: `MP 0.PP b<N>`, swapped in place. One version at a time.
- Title screen: both, dim: `v0.PP b<N>  MP 0.PP`.
- Long string, for the log and the launcher: version, build, short hash, and `-dirty` when tracked files are changed. The hash is 7 hex characters of HEAD. Without git the hash is `nogit`. **Halo example:** `v0.8.6 Build 1 (abc1234)`, the older spelling (dc-halo `src/version.h` comment).

## 8. Worked example (Halo, from the log)

From dc-halo `docs/archive/campaign/CAMPAIGN_PERCENT.md`, the 2026-11-15 row for the v0.86 step:

- Scholar: 90 %. Reason: nine of ten maps beaten end to end on the owner's table; the held -3 for the d20 captain and outro released.
- Parity: 82 %. Reason: d20 captain scene and outro, and the c40 x10 scene, confirmed; d40 unbeaten, finale and credits held.
- Lead's call: average of 90 and 82 = 86. Version `v0.86`, build 1 at the new version.
- Image: `halo-dce-v0.86-b1-full-mp0.33-PARITYTEST.cdi` (dc-halo `docs/BUILDS.md`, row 0.86 / 1). The next image at the same version would be b2.

## 9. Set up a new port (checklist)

1. Write the rule in `AGENTS.md`, "Version and image": the percent, who sets it, the image name, and "agents never boot the emulator" if that applies.
2. Add the version to `src/version.h` at 0 until a level is beaten from start to end on a real image.
3. Create `docs/BUILDS.md` with the header and the table: Version | Build | Commit | Image | sha256 | Date | Notes.
4. Create `docs/archive/campaign/CAMPAIGN_PERCENT.md` with the columns: Date | Commit | Reviewer 1 % | Reviewer 2 % | Lead call | Version.
5. Write a build-number tool with `next`, `name` and `record`. Count per version. Refuse a duplicate row. Read the version from `src/version.h`.
6. Stamp the build number and the short hash into a header at build time. Add the BUILDS row only after an image is written and make exits 0.
7. Refuse a tester image from a tree with uncommitted tracked changes.
8. Use one function for the version string. Use it on the title screen, the log and the launcher. Add the second-mode label if there is one.
9. Name test images with a suffix that is always last. Refuse to write over an existing image.
10. Put the percent rule in the team's lead notes too: Lead alone edits the version, every image build asks for the percent, percent never lowers, only booted work counts.
11. Add a test that no local paths or user names are committed (dc-halo `tests/test_no_local_paths.py`), and a test that every BUILDS row matches a real version.
12. Link this file from the root `DREAMCAST_MAGIC.md` table.

## 10. Open points (not settled in dc-halo)

- What happens to the percent when a counted level breaks in a later build. dc-halo's docs do not say. **Unverified.** Decide and log it.
- Cadence and rounding: settled 2026-11-15 (every image build, rounded down).

## Source files (dc-halo)

- `docs/VERSIONING.md` (the rules; its top table still uses the old triple spelling)
- `docs/BUILDS.md` (the image record)
- `docs/BUILD.md` ("Build profiles", "Release path", "Strict build requirements")
- `docs/archive/campaign/CAMPAIGN_PERCENT.md` (the percent log)
- `docs/archive/multiplayer/MP_PERCENT_BASIS.md` and `MP_PERCENT.md` (the second mode's percent)
- `tools/build_number.py` (counter, names, record)
- `tools/build-game.ps1` (adds the BUILDS row after a good make)
- `src/version.h` (the constants)
- `AGENTS.md`, section "Version and image"
