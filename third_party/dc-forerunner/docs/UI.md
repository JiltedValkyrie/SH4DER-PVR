# Menu, HUD and text design lessons for a Dreamcast port

Source: the Halo CE Dreamcast port (dc-halo), menu, HUD and text work, 2026. Issues are cited as #nnn (dc-halo issues), commits by hash. Tags used: MEASURED (a boot log), ESTIMATE (arithmetic), GUESS (not checked against your game's data). Role names (Designer, Junior, Senior, Parity, Scholar, Cortana, Translator, Lead) are the session roles of the port team (`dc-<project>-<role>`).

Menu note rules are in section 4. Video option rules are in section 10. Subtitle and dialogue text is in [DIALOGUE.md](DIALOGUE.md). Look verdicts for every visual change are in the graphics doc.

## Rules that saved us

1. **Set the column width before the strings.** Set the max pixel width of each value column, write the strings, then render every language in the host mock (Junior, #400).
2. **One accent per screen, with one job.** Values are white, labels are dim grey. Two greys, decided once (Designer, #400).
3. **A price and a balance never look alike.** Different word, different place (Designer, #400).
4. **Cannot afford is grey, not red.** Remove the action; the missing prompt is the message (Designer, #400).
5. **Notes say what, never where, and never judge.** Cut, never add (section 4).
6. **Check the real render in every theme and font.** No one-word last line (Designer, #399).
7. **A widget flag can do nothing in one build.** Check which build it is compiled in before you debug the draw code (Junior, #400).
8. **Tie an "only once" prompt to an event that already happens once.** No save flag for a hint (Junior, #399).
9. **A fact is not a button.** Facts go in the note or the title band. One verb per footer key (Designer, #399).
10. **Every look change comes with a designer verdict and a cost number** (Parity, #394; see section 7).

## 1. Accent and colour

- **One accent, not for numbers** (Designer, #400). The first SUPPLY mock drew the balance in amber on white and blue, and it shouted. Pick one accent per screen and give it one job, such as the active tab or the cursor. Values are white.
- **Source-game colour codes carry meaning** (Designer). If your source game uses colours with a meaning (for example, the good and bad reply colours of a dialogue wheel), nothing else in the port may borrow them. GUESS: check the colour coding on the source screens before you copy it.
- **Cannot afford is grey** (Designer, #400). The unaffordable row goes to the locked grey, and the buy prompt leaves the footer. No warning colour and no "NOT ENOUGH" text.

## 2. Stat rows, balance and price

- **Dim label, lit value** (Designer, #400). "LV 0   COST 100" on six rows read as COST six times until the words went grey and the digits stayed white. On every stat row (credits, healing items, upgrade items, level, cost) the word is quiet and the number is bright. Decide the two greys once, and never add a third.
- **Balance and price are different words in different places** (Designer, #400, owner's boot). "CREDITS 1250" is the balance and "COST 300" is the price. The balance has one fixed spot (title band, right) on every shop, squad and inventory page, and it never moves. The owner could not find the real balance under a price.

## 3. Rows and footer hints

- **Rows are actions or settings; facts go in the note or the title band** (Designer, #399). A lit, selectable row that said "TEAM 3 SEATS" did nothing a row should do.
- **One verb per button in the footer** (Designer, #400). "A BUY / START  B BACK" pushed the version number off the footer in one theme. Do not print two prompts that do the same thing. Test the longest hint in the widest font and in every language.

## 4. Menu notes

The menu note rules live here. DIALOGUE.md section 6 points here. The same rules apply in every language.

- **One sentence: what, not where.** About 60 characters, 100 at most. No model, map, tag, pass, engine name or mechanism. No repeat of the option name.
- **Numbers match the menu.** Seconds, %, metres, on or off. No triangle or vertex counts and no internal units.
- **No judgement words** such as slow, best or prettier. Say the cost plainly ("Uses more CPU") and let the player decide (Designer).
- **When a rule gets an exception, cut the claim; do not name the exception** (Designer, #404). "Every model uses its lightest version." became untrue for one character. The fix was "Models use their lightest version.", and the detail lives in the game guide.
- **Cut, never add. Check every note you touch, in every language and theme** (Designer).

## 5. Text width and wrap

- **Cut the text, never shrink the font** (Junior, #400). Set the width first, render the host mock for every language, and keep shrink-to-fit only as the backup.
- **Check where every note wraps, in every theme and font** (Designer, #399). "...can join your team now." left "now." alone on a second line in one theme, and one word was cut. Look at the real render at 640x480 in every theme before passing wording. A larger source game may have far more menu text than Halo, so this runs on every change.
- **Counts go after a colon** (Designer, #399). Plural rules for translated counts are in [DIALOGUE.md](DIALOGUE.md) section 5.3.
- **Translated lengths run 20 to 35 % longer than English.** Check every key against its draw-site width (DIALOGUE.md section 5.5).

## 6. Menu flow and build-gated widgets

- **A dead flag is a build flag** (Junior, #400). LIST_VALUE_DIM sat inside a multiplayer-only branch, so the dim row did nothing in the campaign build. When a widget flag does nothing, check which build it is compiled in before you debug the draw code. Add a new flag rather than reuse one from the other configuration.
- **"Only once" prompts attach to an event that already happens once** (Junior, #399). The roster note fired from the same place as the reward page, where the first clear is read before the run. It showed once with no saved bit. Do not add persistent state for a UI hint.

## 7. Settings and presets

- **One table of defaults; presets are generated from it** (Cortana, #394). "Grain on by default" had to change at both default sites, the preset apply and the preset match, or the menu showed the wrong preset name. Preset apply and preset match come from one table, and a test checks that every preset round-trips.
- **Look defaults need the designer's verdict** (Parity, #394, #404, #405). The sign-off rule and the cost note are in the departure log (dc-halo DEPARTURES.md, section 3.2).
- **Story characters keep their own override** on every low setting (Senior, #404).

## 8. Look rules and release notes

- **Restraint, not chrome** (Designer). Black card, hairline rules, one type family, two greys, one accent, and whitespace doing the work. GUESS: the source game's own interface may be cool and quiet. Copy its quiet, not its chrome, after a check against the source screens.
- **What reads "AI", and what to do instead** (Designer). Flat yellow ribbons, glossy buttons, generic gradients, icons and badges where a word would do, a coin next to a number, a new colour for each new feature, exclamation marks, and notes that sell ("for the best experience"). Use a word instead of an icon.
- **Release notes follow the same taste** (Designer). Say what changed for the player, in the numbers the menu shows (seconds, %, metres, on or off). No triangle counts, no internal names, and nothing claimed that the owner has not booted.

## 9. Source-game checklist

- Plan the width tests before the first menu page is built. A larger source game has more menu text than Halo (Designer). GUESS on the text volume until it is counted from your own copy of the game (`<source disc>`).
- Subtitles and dialogue wheel labels follow the paging rules in [DIALOGUE.md](DIALOGUE.md) section 2.3 and the dash rules in section 2.7.

## 10. Video settings

Rules for every video option, preset value, quality side effect and default. These rules are new-repo rules, not Halo source text.

- **Three files change together, in one commit.** Any change to a video option, preset value, quality side effect or default updates the menu note, the "Video settings" section of `docs/GAME_GUIDE.md` (the full preset table), and `docs/CONTROLS.md`. A change that updates only one of them is not finished.
- **The code agrees with the menu.** The preset table in `menu.c` (`video_presets`) and the apply function (`video_apply_options`) must agree. Check both in the same change.
- **Player text uses the numbers the menu shows** (seconds, %, metres, on or off). No triangle or vertex counts and no internal units.
- **Menu note rules are in section 4.** WHAT, never WHERE. One short sentence, about 60 characters (100 max). No model, map, tag, pass or engine name. No mechanism. Do not repeat the option name. Cut, never add. Check every note you touch.
- **A default change is a visible change.** It follows section 7 (the designer's verdict, and a departure row when it departs from the source).
- **README stays short.** It says what the project is, how to build it, and where the level editor is (if the port has one). No feature detail there; feature detail goes in `docs/GAME_GUIDE.md`.

## 11. Loading screens

Engine-neutral rules for any port. A port names its own files and hooks in its own docs.

- **Purpose.** A loading screen says what the player waits for, and that the game is still working. It is not a place for art. A movie that plays while the disc reads is a cost the port may not be able to pay.
- **Progress is honest.** Show the stage (what is being built) and the asset (the file being read), in plain words. No percentage bar that does not match real work. If a step has no measure, show the stage only.
- **Redraw on a timer, not per frame.** Redraw at most every 200 ms. A message that nothing follows (a prompt waiting for the player, an error) is drawn at once.
- **No disc read in the frame.** Read assets between frames, inside the load. Never read the disc from the draw or from the audio callback. Long reads call a tick hook so audio keeps running.
- **Memory.** The backdrop is flat fill or one resident texture, not a movie. Keep text in one small sheet, held only while the big fill runs, then released before the fit pass. Write the sheet's size in KB in the port's budget.
- **Text fits.** Squeeze a long line to the sheet width with a margin. Drop the far end of a path only when the squeeze would go below a set minimum. Never cut the start of a word silently.
- **Log once per stage.** Print the free memory at each stage change (not per frame). Measure the time spent drawing the loading screen, so the load time can be split into disc time and screen time.
- **Hooks per port.** Title line (level name or LOADING), a short note line under the bar, a hold colour for a mid-scene swap, a tick callback, and a stage hook for logs. Each port maps its own level and file names to these.
- **Idle hardware.** While the screen waits for input, show nothing that moves faster than the eye needs. Keep the device screen (memory card or similar) on its logo if the platform has one.
- **Sign-off.** The look (ink, bar, type) needs the designer's verdict before it lands (section 8). The cost (KB, ms) goes in the change, the same as any other visual.
