---
title: "Menu logic [menu]: a list with headings, tabs, a scrolled text, tap or hold"
tags: [effect, routine, menu, list, scroll, input, suite, prototype]
updated: 2026-11-26
sources: [include/sh4der_menu.h, test/menu_test.c, include/sh4der_suite.h, test/suite_test.c, demo/psm/suite/suite_view.h, demo/psm/suite/test/suite_view_test.c, README.md "The menu logic", AGENTS.md]
---
# Menu logic [menu]: a list with headings, tabs, a scrolled text, tap or hold

**What you will learn:** what the small decisions of a menu are, how the library makes them once with nothing drawn, how to call them from your own menu, and what has and has not run on a console.
**For:** a programmer who needs a menu, a settings page or a result browser, and does not want to write the cursor and scroll rules again.

> **In plain words.** A menu looks simple, but it is full of small rules. The cursor must jump over the headings. The list must scroll so the cursor stays on screen. A long text needs a scroll bar in the right place. Holding a direction should repeat. A tap and a long press of the same button can mean two things. And when the game comes back from somewhere else, the menu should open where you left it. This file is those rules, and only the rules: it draws nothing. Your program reads the pad, calls these functions, and draws what they say.

## Try this first

Nothing on screen by itself. It is a routine your menu calls. The settings, from the README's "Menu tuning" table (all ADVISED: chosen on the host; new, not yet on a disc):

| Setting | Value | What it is |
|---|---|---|
| `SH4DER_MENU_HOLD_MS` | 2000 | a button held this long fires, in ms |
| `SH4DER_MENU_REPEAT_FIRST_MS` | 300 | a held direction waits this long before its second step, in ms |
| `SH4DER_MENU_REPEAT_MS` | 80 | then steps this often, in ms |
| `SH4DER_MENU_STICK_DEAD` | 40 | a stick axis (-128 to 127) inside this is at rest |

1. Make an array of rows a tab. Each row is a heading (`SH4DER_MENU_HEAD`) or an item (`SH4DER_MENU_ITEM`) with an id of your own.
2. Call `sh4der_menu_init(&m, lists, tabs, window)` once. `window` is how many rows fit on your screen.
3. Each pass: turn the D-Pad into -1, 0 or 1, give it to `sh4der_menu_repeat`, and give its step to `sh4der_menu_move`. Do the same for the stick with `sh4der_menu_stick_dir` and `sh4der_menu_scroll_step`.
4. Draw the rows from `m.pos[m.tab].first`, mark the row `m.pos[m.tab].cursor`, and draw the scroll bar where `sh4der_menu_scroll_bar` returns 1.

Where the numbers come from: `include/sh4der_menu.h` (the macros), README "Menu tuning" (the table), `test/menu_test.c` (the checks, run by `make test`).

## What it does

- **A list with headings.** The cursor is only ever on an item. Up and down step over headings and go round at the ends. A list with no item does not move.
- **A window.** The first row on screen moves as little as it can to keep the cursor visible. When the row just above the window is a heading, it is shown too, so an item at the top has its heading over it.
- **Tabs.** Each tab has its own list and keeps its own cursor and window. Left and right go round.
- **A scrolled text.** A place, clamped at both ends, and the bar's thumb: its offset and length on a track. No bar when the text fits.
- **A held direction.** One step at once, the next after the first wait, then one every repeat. A turn round steps at once.
- **Tap or hold.** A tap when the button is let go before the time. While it is down, the progress in thousandths (for a bar that fills). At the time it fires, once, and the release after it is not a tap. A button that was already down does nothing until it is let go.
- **Word wrap.** A text cut into lines at spaces; a word longer than a line is cut at the line's end.
- **The place in one word.** Tab, cursor, first row and scroll packed in 32 bits with a mark, for a program that is started again. A word without the mark, a tab that is not there or a cursor on a heading is refused, and the menu stays as it was.

## Used by

- `include/sh4der_suite.h`: the hold of A on a test's last result page (a tap is the next page, 2 s goes back to the menu), and the menu's place in the mail's spare word. See [suite](suite.md).
- `demo/psm/suite/suite_view.h`: the suite disc's menu: two tabs (by demo, by effect), the list, and the picked demo's card scrolled by the stick.

## Limits

- **It draws nothing.** The list, the bar and the fill bar of a hold are the caller's to draw.
- **At most 511 rows a list and 4 tabs.** The packed place has 9 bits for a row and 2 for the tab. A scroll over 255 lines is stored as 255.
- **One clock.** The repeat and the hold take the time in ms from the caller; give both the same clock. A 32-bit ms clock's wrap (after 49 days) is handled.
- **The scroll bar divides.** Two integer divides, once a redraw, never a frame.

## Cost

| Item | Cost | Status |
|---|---|---|
| RAM | 4 bytes a row (the caller's array), 20 bytes the menu's state, 6 a scroll, 8 a repeat, 8 a hold | exact, by the code |
| VRAM | 0 | |
| A pass | a few compares | by construction |
| Time on the SH-4 | NOT MEASURED | the suite menu prints each redraw's time on the serial line; that is the menu's drawing, not this file |

## Checks

`test/menu_test.c` (run by `make test` and `make test-shz`): 398 checks. The list (round at both ends, over two headings in a row, a list with no item), the window (headings shown, the last rows filling it), tabs, the scroll and its bar (never past the track, never shorter than its least length), the repeat (steps at 0, 300, 380, 460 ms; the clock's wrap), the hold (a tap, a hold that fires once, a button already down), the wrap (nothing lost, no line over its columns), and the place (every row of two lists packs and unpacks to itself; five kinds of bad word are refused).

The suite's use of it is checked in `test/suite_test.c` (the page loop's decisions) and `demo/psm/suite/test/suite_view_test.c` (the menu's screens, drawn on the host).

## Recommended limits

- Status PROTOTYPE v0.1, host tested. New, not yet on a disc. The times (2 s, 300 ms, 80 ms) and the dead zone (40) are ADVISED, not measured. The owner will run it on her console later; nothing is claimed about a console run now.
- Give the tap the common action and the hold the rare one (leave, confirm): a hold is hard to do by accident.

## Start-here links

- Tools and safety: [genre-tools-and-safety](../start-here/genre-tools-and-safety.md).
- A bigger picture of what each routine is for: [what-do-you-want](../start-here/what-do-you-want.md).

## Related

- [suite](suite.md): the test disc's menu and the way back from a test, the first caller.
- [page](page.md): the result pages a test shows before it goes back.
