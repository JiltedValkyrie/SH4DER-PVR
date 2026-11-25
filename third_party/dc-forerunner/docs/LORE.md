# Lore and canon for a Dreamcast port

Source: the Scholar role in the Halo CE Dreamcast port (dc-halo): lore, canon, dialogue and play tests, 2026. Each lore fact in these notes carries one source tag (rule 2 below). GUESS marks a claim that is not yet checked against your game's own data.

This file is a how-to for sourcing a game's lore and canon for a port. The Halo rules are the examples; the checklist at the end is for your game.

## Rules that saved us

1. **Pronouns come from the source, never from a name.** A name does not give gender. Two unsourced characters were left open for a call (Scholar, dialogue gender pass, Halo port).
2. **Every lore fact gets one tag: CONFIRMED, ANECDOTAL or NOT FOUND.** Only CONFIRMED facts go into the port as canon. ANECDOTAL and NOT FOUND facts are written as "not sourced" (Scholar, #391 and #375 research).
3. **A web snippet is not a transcript.** Take exact line text only from the game's script or a verified source (Scholar).
4. **Word choice follows the story.** Check each lore verb against what happens in the story, and use the translators' rows as a cross-check (Scholar, a10 row tag 1800).
5. **Campaign percent moves on boot proof, not on a commit.** Count one map per boot proof (Scholar, Halo's campaign percent log, 2026-11-05 to 2026-11-15).
6. **Only the owner's boot is proof.** Coded and host-tested is not proven (Parity, #393).

## 1. Canon sources

- **Pronouns: copy from the game's own text or codex.** If there is no source, use neutral wording and flag it for a call. Never guess (Scholar). GUESS for your game until the game's text is read.
- **Tag every lore fact** (rule 2). Put the tag in the note next to the fact. Write "not sourced" for ANECDOTAL and NOT FOUND facts.
- **Web research gives the route and the rules, not exact lines.** Some pages did not load; say so in the note (Scholar, #391 checkpoints and #375 trench jump, map research).
- **Lore verbs follow the story.** "led away", not "let", for a fleet leading enemies away from home (Scholar, a10 row tag 1800, after a Gouache review and a Translator push-back). Say when unsure.
- **Address forms are lore.** Lock them in a glossary, in one place, next to the dialogue notes.

## 2. Campaign percent and boot proof

- **A map counts as beaten only when someone plays it end to end on a real image.** "Completable" is not beaten. Halo's percent went 84, 87, 90 as maps were proven on the owner's boots. Unbooted fixes held it (Scholar).
- **Hold unreached finale and credits as unpaid.** Do not raise the percent on a code change (Scholar).
- **Name the boot check for every merge.** Count only booted-and-proven work toward progress (Parity, #393 arena regrow after RESTART LEVEL).
- The percent lives in `src/version.h` (v0.05 = 5 %). Only Lead changes it.

## 3. Play-test practice

- **Name the difficulty branch before a line counts as missing.** A line can sit on one difficulty branch only. Check the branch first (Scholar, Parity).
- **Quote scene ticks as ticks.** Give the animation or scene tick the line is tied to, not a guess at the time (Scholar).
- **Check a commit hash before it goes into an issue.** A wrong hash sends the next person to the wrong code (Scholar).
- **The owner's own run log is the test.** No conclusion from a log the owner did not confirm. Label estimates as ESTIMATE until a boot (Scholar and Parity; PERFORMANCE.md section 8).

## 4. How to source your game's lore and canon (checklist)

- **Canon source, in order of trust:**
  1. The game's own data: script text, codex entries and conversation data, read with your own tools (one reader per source format; see `tools/build_assets.py`).
  2. The owner's own copy of the game, played and checked on the owner's machine.
  3. Official text from the publisher (manuals, art books, released scripts).
  4. Wikis and fan sites: routes and rules only. Never exact lines, and never canon on their own.
- **Web summaries are not canon** until your tools read the game's data. GUESS until then.
- **Build a fact sheet.** One row per lore fact: fact | where it was read (file, page or entry) | tag (CONFIRMED, ANECDOTAL, NOT FOUND) | owner check. Keep it in one file the team can read.
- **Keep one glossary** for names, address forms and pronouns, with the source of each.
- **Check the speaker and the branch** of every voiced line against the game's data before it ships.
- **Check every cinematic claim** (voice, camera, sfx, fx, actor, animation) against the game's data before it is wired. Halo's rule: lore research comes first, the data check second (DEPARTURES.md section 4).
- **Count progress on proof.** Percent moves only for maps the owner has booted and beaten end to end.
