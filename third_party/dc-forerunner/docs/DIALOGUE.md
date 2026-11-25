# Dialogue lessons for a Dreamcast port

Source: the Halo CE Dreamcast port (repo `dc-halo`), history up to 2026-11-15. Read-only research; nothing in `dc-halo` was changed. These lessons apply to any port with conversations, subtitles, translations, or any other text the player reads. Where the text says `<Game>`, `<source disc>` or `<source format>`, put your own game's name or format there.

Dreamcast budget used below: 16 MB main RAM, 8 MB VRAM, 2 MB AICA sound RAM.

Tags used below:
- **GUESS**: a claim about the source game or the Dreamcast that nobody has checked yet. Verify before use.
- **ESTIMATE**: a number from a plan or a test, not measured on hardware for this exact case.
- `#NNN` = `dc-halo` GitHub issue. `hash` = `dc-halo` commit. Both are history, not facts about your game.

## Rules that saved us

1. Every dialogue line (new or changed) gets a lore check (Scholar role) and a tone check (Designer role) before it ships. The owner asked for this on 2026-11-14 (#344). Do it before the line is wired, not after the boot.
2. Voice audio streams from disc. Never make it resident (AICA sound RAM is 2 MB). Only the subtitle text of the current map or conversation sits in RAM.
3. Text has its own RAM floor. Measure it per map from the boot log. A shared floor silently removed all subtitles on one map (#392).
4. Open the text table on every path that starts dialogue: level load, cold start from a menu, cutscene replay, pause resume. A path that skips the open shows no text (#397).
5. Key every line by stable names (clip name, permutation or string reference), never by a built index that can shift between builds. A shifted index put lines on the wrong voice until the bake was made to check names first (`27eb60ef`).
6. Speech-to-text output is a draft. A human checks each line against the audio. Three garbled lines were found this way (`e861fa84`). Garbled English is a text error until the audio says otherwise (Scholar, Designer review of three EN rows, no issue). Read the PT and RU rows for the intended meaning, fix the EN text, and have someone who can hear the clip confirm it. Nobody fixes audio timing from text alone.
7. Never truncate text silently. Page long lines. Before paging, 53 of 189 a10 lines were cut on screen (`de334bbb`).
8. A missing line is a count problem first, a game bug second. Count what the script asks for, what the bake keeps, and what plays. Then look at the scene (#402, `7a196944`).
9. Write our own translations. Never copy text out of another game's files or the player's install. Ship only what we wrote.
10. Every font pak, language table and subtitle blob the text path needs is built by the launcher and the build step. Test a fresh clone (#330).

## 1. Playback and streaming

### 1.1 Voice streams from disc
- dc-halo planned scripted dialogue as a stream from disc, like cutscene voice, not resident (2026-11-01 call).
- Dialogue cost per map, dc-halo numbers at its converter rates: a10 dialogue 7.1 MB (3.6 MB before the rate change); whole disc 1: 151.5 MB for dialogue + cutscene + music; disc 2: 158.6 MB (`docs/DIALOGUE_CENSUS.md`).
- `<Game>` GUESS: its voice may be far larger than dc-halo's counts. Run the census tool on your own copy of the source game (`<source disc>`) first, and write the MB per level before any design.
- Codec arithmetic (ESTIMATE): 4-bit ADPCM at 22,050 Hz is 11,025 bytes per second per voice, about 40 MB per hour of one voice. Check the read speed of the target drive on hardware before the streaming plan. Do not guess it.

### 1.2 Text has its own RAM floor (#392, `64f116f6`)
- Symptom: on d40 subtitles showed one line, then none. The text blob was refused on every load after the first BSP.
- Cause: the subtitle blob needed a 2,048 KB free floor. Later d40 loads ended with 1,041 to 1,945 KB free. 24 of 27 loads were refused. The voice still played; only the text was missing.
- Fix: a 512 KB floor for the subtitle blob only. The gameplay gate (2 MB free after load) was left alone.
- Rule: each refusal prints one serial row, once per load. A refused text table falls back to no text, never to the wrong language.
- Cost of the blob: 3.2 to 11.7 KB per map in English or PT-BR (ESTIMATE from dc-halo `docs/archive/feature/SUBTITLE_BLOB.md`).

### 1.3 Open the text on every entry path (#397, `9a050e3d`)
- Symptom: cutscenes played from the Theatre menu never showed subtitles, even with subtitles on.
- Cause: the open call sat on the level-load path only. A cold start (Theatre) never opened the blob. The scene loop never ticked it.
- Fix: open after each scene load, tick each frame, close on unload.
- Rule for your game: list every entry point (new game, load game, conversation from a menu, scripted scene, replay). Give each one a boot test with subtitles on.

### 1.4 Start and stop follow the voice, not a timer
- A text line starts when its voice permutation starts and clears when the voice stops (`dialogue.c` hooks in dc-halo).
- A voice stop, a skip, or a script sound stop clears the line at once.
- The pause freezes a hold timer and shows no line.
- Rule: never time text on its own clock. It drifts from the audio on skips and replacements.

### 1.5 Key by names, not indexes (`27eb60ef`)
- dc-halo's blob keys a line by (tag, permutation rank). The rank is the position after sorting by pak offset. Any change to the pak order moves the keys.
- The bake now looks up the audio manifest entry by permutation name first, then by index, then by clip name. A permutation past the tag's count stops the bake.
- Rule for your game: key by the conversation node or the string reference plus the voice cue name. Stop the bake on any mismatch. Do not skip silently.
- **Clip key and permutation key are separate fields** (Parity, #397 tag 561). The subtitle bake reads the clip (the sound path) and the permutation name on their own. A row with the permutation name in the clip field stops the bake. Check the field names in the row before the bake.
- **Subtitle rows point at the real sound path** (Translator, #397). The clip key is copied from the audio manifest, never retyped from the cutscene name: "B40_insert_320_MONITOR" broke the bake, and the real path was c40_insert_320_monitor (Parity sent it back). A test checks that every subtitle row resolves to a sound file.

## 2. Subtitles

### 2.1 Blob format (DCSB, `181b896a`, `docs/archive/feature/SUBTITLE_BLOB.md`)
- One file per map and per language. Header with version, language code, counts. A sorted table of fixed-size entries. Then a pool of NUL-terminated strings in the language's single-byte code page.
- The reader checks everything before use: size, version, zero field, language, every offset inside the pool with a NUL, every speaker index, every entry sorted. A blob that fails any check gives no subtitles and one log row. It never draws garbage.
- Your game: keep the same shape. Key = (string reference or node id, voice cue). Pool = the speaker name and the line.
- The reader checks are as important as the writer. The Python reader did not check what the C reader must.

### 2.2 Which lines go in
- Rule set used by dc-halo: lines are included when the speech-to-text match is strong and the line has enough words. Weak, short, scream, beep or "unsure" clips stay out until a person approves them.
- Your game: include all lines that have a voice, but keep the rule that a line with no approved text shows nothing. Never show a placeholder.

### 2.3 Paging, never cut (`de334bbb`)
- Before the fix, a line longer than two lines was cut after line 2.
- dc-halo now shows long lines as a series of two-line pages over the clip. The clip time is split by character share.
- Page break rules (Designer, 2026-11-15): end a page at a sentence end in line 2; else at a sentence end in line 1 past 60 % of the width; else after a comma, semicolon or colon in the second half of line 2; else fill. A last page under half a line joins the page before. No one-word line 2. A number stays with the word after it.
- The speaker name is on the first page only.
- Your game: the dialogue wheel reply, the NPC line and the subtitle may need different rules. Measure the longest line of each language on the target screen before the rule is set.
- **Build the paging rules into the bake before the first line ships** (Designer). The rule set in 2.3 only holds if the bake applies it. Apply the same care to the dialogue wheel labels.
- **Measure every wrap rule with real font advances** (Cortana, subtitle pages v1 to v3). Test the long rows from the real script in each language. A first "move the lone word" rule moved a word without measuring, and Parity sent it back (pages, de334bbb). Pure fill had ended pages on "...systems. We" and shown "to drown in!" for 0.7 s.
- **One clip is one row; long lines are paged in code** (Translator, #397 c40 Spark line, 249 characters). The line could not be split in the data, so two-line pages of 400 px each were added. Never split a voice line into fake rows.

### 2.4 Non-breaking spaces (`79ddb1e3`, `909c7319`)
- Designer rule: a page never splits a name or a number group. dc-halo uses a non-breaking space (U+00A0) inside multi-word names and in Russian number groups.
- Trap: the bake first folded U+00A0 to a plain space, so the line still broke inside the name. Keeping byte 0xA0 in the text would have opened the Latin font row. That row costs 16 KB of VRAM on its own (ESTIMATE). The fix had to be sequenced with the font-row logic.
- Rule: before you add a special character to text, check which font row or code path it triggers. Count the VRAM and RAM cost first.
- **Protect names and number groups from line breaks** (Translator, dc-halo "Master Chief", "Truth and Reconciliation", RU "25 000"). Join your character names, place names and ship names with NBSP in the source text (for example "<character name>", "<place name>", "<ship name>"), and do the same for number groups. Draw NBSP as a space-width gap with no glyph, so it does not open a font row. Number format per language (ES "25.000", PL "25 000").

### 2.5 Subtitle look (Designer verdict, dc-halo `docs/archive/feature/SUBTITLES_DRAW.md`)
- Bottom centre, at most 2 lines per page, width cap 400 px, text in warm white with a 1 px dark shadow.
- Speaker name in caps in a muted accent colour, then two spaces, then the line.
- No speaker name in a cutscene (design choice for dc-halo; decide it for your game).
- The draw uses the same store-queue direct render as the HUD text (KOS path). One header per texture.
- Per-frame cost with a line showing: about 265 quads and 2 headers (ESTIMATE). Nothing when no line shows.

### 2.6 Font rows and VRAM
- Latin accents: a 256 x 91 row, 16 KB of VRAM while loaded, 13.2 KB on disc.
- Cyrillic: a 256 x 74 row, 16 KB of VRAM, 10.7 KB on disc.
- Rule: a language that needs a new glyph set costs VRAM (8 MB total). Measure it on the VRAM ledger row, with a reserve left for the frame.
- **Load a glyph row only for the language that needs it** (Translator, #397 NBSP). An extra glyph row in the English build cost +16 KB of VRAM for nothing. The English build keeps 0 KB of extra font VRAM. Spanish needs the high row for ¿ ¡ ñ; Polish needs the cp1250 high row. Load it only when the language is active, and state the KB.

### 2.7 Cut-off lines, dashes and ellipses
- **One house form for a broken-off line** (Scholar and Designer, #397). English cut-off is one hyphen with no space ("Keyes? What-"). A mid-line aside is a comma. No double dash in subtitle text: a source "--" was changed to a comma. Match the shipped rows of each language, not a general style guide.
- **Cut-off marks are per language** (Translator). PT uses "-" and RU keeps the «…» form of the shipped rows. ES uses "…" or "-", PL uses "…". The bake folds the ellipsis to "...", and the fold must not break PL quotes („ ”) or ES « ».
- **Decide the marks once and lint new text against them** (Designer). Check each mark against what that language's font row can draw. Do not "clean up" a character's voice: "Aw, this is loco!" stays as written.

## 3. Missing lines

### 3.1 A stale census said voices were silent (`docs/archive/campaign` audit, 2026-11-01)
- The census tool listed only a10 to b40. Disc 2 voices showed as "silent".
- The Senior role read the source and found disc 2 was staged and indexed on all maps. The cost of the fix was zero.
- Rule: the census must read the same map list as the build. Generate the census from the build data, not from a hand list.

### 3.2 Counts after a change (`7a196944`, `67dc3ba3`)
- After #191 turned sourced script lines into their own events, the a10 opening test counted 17 + 22 = 39 lines. The script asks for 40 (CAMP_SOUND_AT).
- The check found nothing lost. The test counted only dialogue lines.
- Rule: when you change how a line is wired, update the count test and say why the count moved.

### 3.3 Still open: a clip that never plays (#402)
- The a10 opening speech by a sergeant (clip 21.7 s) was never seen on the owner's boot. The cause is still open: scene short, line missing, or the shot never plays it.
- Rule: a missing line needs three checks in order: (1) is the line in the script, (2) does the bake keep it, (3) does the run start it (serial "VOICE start" row). Do not guess from the picture.

### 3.4 Text from the script, not from a doc (`a0ccad7b`)
- 8 cutscene lines were missing from the subtitles of three dc-halo maps. The text had been copied from a documentation file that was cut off partway through a scene.
- Fix: take the text from the script's own print data for the build, and check that each line exists in the bake.
- Rule: a text source must be the build data. A doc is a copy and can be cut.
- **A line that ends in "..." in a doc is not the full line** (Scholar, #397 c40 row at tick 4761). A doc cut the Spark monologue mid-sentence. Take print text from the built script data.

### 3.5 Print string is the transcript; the clip is the voice
- **Match each sound tag to its print string first** (Scholar, #397: 8 dialogue rows among 28 missing sound tags). Cutscene lines exist as scenario print strings (text) and dialogue clips (audio), and not every clip has a print. A clip with no print needs the audio or a human transcript before any text is written.
- **Filter the miss list by sound path before writing lines** (Scholar, #397). The miss list also held foley (chip insert, teleport, throw). Only the dialogue folder gets a line; foley and effects get none.

### 3.6 Name the difficulty branch before calling a line missing
- **State which branch each timing or line comes from** (Scholar, #402 a10 hangar scene: Legendary-only lines had no tick in the Normal doc). A line the doc does not cover is said so, and is not placed by guess.
- **Check the difficulty in the boot log first** (Parity, #402: sgt05e is Legendary-only, and the owner's log was Normal). A line missing on the player's difficulty may be correct. See dc-halo SCRIPTING.md section 4.

## 4. Speaker-positioned lines (#191, `67dc3ba3`)
- Symptom: script-triggered voices played centred, not from the speaker. About 650 start calls named a source object, but the converter dropped it.
- Fix: the source object is packed with the dialogue event. It plays at the object's head with the sound's min and max distance. 97 lines were sourced; objective lines that are out of range stay out by design.
- Side effect (#191 follow-up): a voice moved to a new AICA path can change the listener and pan values. Pin the aim and pan tests.
- Your game: an NPC voice must come from the actor, not the camera. Carry the actor (or bone) in the conversation data, and drop it only on purpose.
- Lip sync (GUESS for your game): lip-sync data is probably too large to ship per line. Measure first. dc-halo's diagnostic for mouth motion (`b4cf3624`) printed one row per line, capped per scene, to keep serial traffic low.

## 5. Localisation

### 5.1 Language pick and text tables (dc-halo `docs/archive/ui/LANG_TABLE.md`, #327)
- Languages: English (source), PT-BR (Latin-1 code page), Russian (CP1251 code page). Voice stays English. Subtitles are in all three.
- The English literal stays in the code. A lookup maps it to the translation. English output is byte-for-byte the same as before the change, and a test checks that.
- Four tables by use, each with its own file and its own 65,536-byte cap: menu (M), HUD (H), per-map tag text (T), mode tables (F, O). Only the tables in use are in RAM.
- Your game: split by the same kind of line: menu, HUD, conversation wheel and subtitles, per-map journal and tags. Each table gets its own cap and its own load gate.
- **Write down early what is never translated** (Translator, Lead ruling). The debug readout, credit names and maybe brand names stay English. Write the list before the strings freeze, so nobody "fixes" them.

### 5.2 Hash keys, code pages and caps
- The table holds FNV-1a 32-bit hashes, not the English text. The runtime checks only the hash. A stray hash hit gives a wrong string. The bake refuses a collision between English keys. The stray rate is about 1 in 3 million per text (ESTIMATE, 1,426 keys).
- Rule: keep the English key list next to the bake, and run the collision check in the test.
- Code page per language: 1 byte per letter. Russian in UTF-8 was 68,930 bytes, over the cap. The same text in CP1251 was 39,024 bytes. Choose the code page and count bytes before you translate.
- A language table that will not fit is refused, logged once, and falls back to English.
- **One code page per language; Polish is not Latin-1** (Translator, dc-halo subtitle_blob.py: Latin-1 for EN and PT, cp1251 for RU). ES fits Latin-1 (á é í ó ú ñ ü ¿ ¡). PL needs cp1250 (ą ć ę ł ń ś ź ż). Pick the code page per language at bake time, store it in the blob header, and test that every PL letter round-trips. Never fold ł to l.
- **The width tool uses the same code page as the bake** (Translator; Junior read Cyrillic as UTF-8 bytes and got 310 px against the real 184 px). The width check takes the language and encodes with its code page before it measures. One tool, shared by everyone.

### 5.3 Plurals and arguments (#344, `6bb7199d`)
- Count phrases are whole keys per form: "%u track" and "%u tracks" are two keys, not "track" plus "s". Russian needs its own forms for the number (GUESS: three forms for most nouns; check the translator's list).
- A unit that has no agreement form is written unit first.
- #344: the format string was translated, but the arguments (side names, places) were not. Translate each argument through the lookup, not just the format.
- Rule: a translated sentence with English arguments is a bug. Test with a pseudo-locale.
- **Never build a sentence around a counted noun** (Designer, #399; Translator, RU has three plural forms). "1 of 3 team seats are open" was wrong in English and worse to translate; "Team seats open: 1 of 3." needs no plural rule. Keep a plural-form table per key, or use wording with no counted noun. PL has three forms (1 / 2-4 except 12-14 / everything else), ES has two. Decide before the strings are frozen.
- **Grammar follows the player's gender** (Translator, dc-halo roster field; GUESS for your game until the disc data is read). ES and PL adjectives and past tense agree with the player character (PL "zrobiłeś / zrobiłaś"). Keep M and F variants for every line that addresses or describes the player, chosen at runtime from the save. Do not assume male.

### 5.4 Coverage (`48bbb8f3`, #335)
- A pseudo-locale run found 489 of 1,426 keys still drawn in English after the first pass. Nearly all were texts in files outside the list.
- A translator audit found 4 of 1,497 new keys with no row. A missing row shows English.
- #335: the button configuration page was not translated. It drew through a path with no lookup. Found on the owner's boot, not by the tests.
- Rule: a coverage test that draws every page with a pseudo-locale and counts English text left on screen, per page. Run it on each commit that adds text.
- **An English change ships with its ES and PL rows in the same change** (Translator, #404: "Every model uses its lightest version." changed, and the old PT and RU rows broke `extract --check` and test_lang). The tables are keyed by the exact EN bytes, so the old keys are removed. CI fails on orphan keys and on missing keys.

### 5.5 Width per row
- Russian is longer. dc-halo cut a Russian label at 65 % of its box (`187705fd`: "ДЕТАЛИ ОБЪЕКТОВ" became the shorter "ОБЪЕКТЫ", 79 px, fits at 127 %).
- Other rows were fixed the same way (`97b0d914`).
- Rule: each row has a measured width limit in pixels per language. Check every row. Shorten the wording before you shrink the font.
- **Translation runs 20 to 35 % longer than English. Measure pixels, not characters** (Translator, RU "Свободных мест в команде…" was 306 px in a 300 px panel; fixed by rewording). Find the draw site first, then check every UI key against its draw-site width before the patch is sent. A shrink guard is the backup, not the plan.

### 5.6 Fonts built by the launcher (#330)
- The launcher never ran the font build step. The Russian and subtitle font paks were missing, and the image build refused to run. A fresh clone failed the same way.
- Fix: the launcher runs the font step silently and logs it. A fresh-clone walk is the acceptance test.
- Rule for your game: every asset a language needs is built by the one build command. Keep a fresh-clone test in the tracker.

### 5.7 Legal line (dc-halo `docs/archive/feature/MCC_LOCALIZATION_BIN.md`)
- dc-halo studied the strings in the owner's copy of a remastered collection. Nothing from it was committed, shipped or sent to testers. Our PT-BR text was written in-house.
- Your game: the same rule applies to the owner's install and to any official translation. Study the file format if you like. Commit and ship only text we wrote.

### 5.8 Glossary, speaker labels and address
- **One glossary, set before line one** (Translator, dc-halo GLOSSARY.md: one row per term, Lead rulings logged). Your game's terms (faction, place, vehicle and item names) get one ES and one PL row each. A term may be checked against the official release as a reference, but no sentence is copied (rule 9 of this file).
- **Speaker labels come from the shipped rows** (Scholar, #397 PT and RU name check). A new translation sometimes invents a label ("Marine", "Fuzileiro", "Морпех"). For each speaker, look up the label in an existing shipped row in each language and keep it.
- **Formal and informal address is lore; write it in the glossary** (Translator, dc-halo: Spark uses вы to the Chief, Cortana uses ты). ES tú / usted and PL ty / Pan / Pani are set per speaker pair (GUESS for your game's pairs until checked). One glossary row per pair, the same answer in every file, set by the Scholar rules.

### 5.9 Meaning checks
- **Keep the tense and mood of the source** (Scholar, #397 v2, Spark monologue). "How couldn't you?" is conditional; a past-tense question was wrong. Check that the translated line means the same thing, not only that it reads well.

## 6. Menu note text rules

The menu note rules (WHAT not WHERE, about 60 characters, cut never add, no judgement words) live in [UI.md](UI.md), section 4, with the Designer tips. The same rules apply in every language.

- **Translation keeps the meaning, not the length** (Translator, dc-halo menu-note rule: one sentence, 100 characters max, three lines in the 272 px box). Get the tone owner's wording first, then translate. Check the pixel width after translating. Numbers in the note match the menu.

## 7. Process

- Dialogue review gate: lore + tone for every line before wiring. Include the speaker, the gender of the speaker, and the line's place in the conversation.
- Parity-style code review: the review states the RAM and VRAM cost in numbers. A subtitle or text change with no number is not a PASS.
- Boot test with the owner's own run log is the only proof that text is on screen. Unit tests prove the format; they do not prove the game shows the line.
- One issue per bug, updated at each step: found, diagnosed, committed, booted, fixed. Close only after the owner's boot.
- Look reference: a designer gives the look verdict (layout, size, colour) before the change lands. The verdict is final for look only.
- **Punctuation is voice** (Designer). One exclamation mark would ruin a flat line such as Halo's "Save his head. Dispose of the rest." The same applies to any flat-voiced character in your game (GUESS: check by ear on the voice clip).
- **Transcripts come from the game's own script text, never from memory** (Designer). Two readers: one checks canon, one checks voice. Every new or changed line gets both before it is wired.

## 8. What cost hours

| Item | Cost | Lesson |
|---|---|---|
| Subtitles gone on one map (#392) | two boot rounds | RAM floor is per data type; measure per map |
| Subtitles gone on Theatre scenes (#397) | one boot round | every entry path needs its own boot test |
| Launcher missed font pak (#330) | a manual build step on every fresh clone | one build command |
| Stale census (2026-11-01) | a whole audit of "silent" voices that were staged | census from the build list |
| Speech-to-text draft | a second pass with 67 differences checked | a person checks the text |
| Non-breaking space (`79ddb1e3`) | a font-row trap, +16 KB VRAM | count the side effects of a new byte |
| Russian width and byte cap | label rewrites, table split | count bytes and pixels per language first |
| Hash-only lookup | a stray-hit review | keep the key list and collision test in the bake |

## 9. Source-game checklist

Everything here is GUESS until checked on your own copy of the source game, with your own source reader (one per source format; see `tools/build_assets.py`). Halo's numbers above are not your numbers.

- **Conversation trees.** Expect conversation data in `<source format>`, with entry and reply nodes, flags and links. Each node should carry a text reference and a voice cue. Bake them per map into a compact table, like the DCSB blob: node id, speaker, text offset, voice cue name. Keep only the current conversation's nodes in RAM.
- **Conversation wheel.** The wheel probably shows short reply labels, and the full line is spoken. The label and the full subtitle may be different strings. Give each its own key and its own width check.
- **Branching.** A branch changes which voice plays next. Subtitle hold and clear must follow the voice that actually plays, not the node that was chosen. Test every branch end: a skip mid-branch must clear the text.
- **String tables.** Expect one string table per language, with IDs referenced from the packages. The table is probably large. Do not ship it. Bake only the strings the port uses, into your own format, with your own code page (Latin-1 for EN, FR, DE, ES, IT, PT-BR; CP1251 for RU, or a wider page if Polish or Czech are added). Check the code page of each language before the bake.
- **Voice count.** GUESS: tens of thousands of lines per language. Count them with a census of your own copy before any RAM plan. Stream them. Do not make any voice resident except the few the current scene needs.
- **Voices stay in one language, subtitles in each language.** This is the dc-halo choice. It keeps the voice cost at one set.
- **Lip sync.** Lip-sync data is probably large. Measure first. A cheap mouth curve from the voice envelope is one option; cost it in KB per line.
- **Positional voices.** Carry the speaking actor per line (the #191 fix). Check each conversation camera shot for the right speaker.
- **Disc budget.** Use the dc-halo numbers as the template: MB per level for voice, per language, per disc. dc-halo's disc 2 was 36.9 MB over its CD-R budget before cuts, and music and voice rate cuts only apply to the CD-R build profile. Keep the same profile rule: full quality on the emulator and the GD-ROM build, cuts only where the disc needs them, and write each cut down with the MB saved.

## 10. Open items

- #402 (a10 sergeant speech never seen): still open in dc-halo.
- #335 (button configuration page untranslated): still open in dc-halo.
- #344 (translated format with English arguments; bot and chat lines): open in dc-halo.
- #327 (localisation umbrella): open in dc-halo; the subtitle and PT-BR path needs the owner's boot.
- Your game: confirm the source package layout, the string table layout and the voice codec on your own copy before any plan that depends on them.
