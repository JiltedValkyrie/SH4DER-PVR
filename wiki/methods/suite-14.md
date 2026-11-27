---
title: "Suite 14: the menu, the hold of A, and what to look for"
tags: [method, suite, run, ledger, menu]
updated: 2026-11-26
sources: [demo/psm/suite/suite_14.json, demo/psm/suite/suite_view.h, demo/psm/suite/suite_menu.c, include/sh4der_suite.h, include/sh4der_menu.h, demo/psm/suite/build.py, demo/psm/Makefile, demo/psm/psm_dc.c, demo/pl4nar/src/main.c, wiki/methods/suite-13.md, wiki/effects/suite.md, wiki/effects/menu.md]
---
# Suite 14: the menu, the hold of A, and what to look for

**What you will learn:** what is on the suite 14 disc, how its menu and the hold of A work, which programs return to the menu by which keys, and what to check when the owner runs it.
**For:** the owner, running the disc; and Scholar, filing the run.

This page follows the committed list (`demo/psm/suite/suite_14.json`). It is suite 13's thirteen programs plus the heat haze street demo (standalone-25, passed final by the owner on 2026-11-26), with the new menu. The disc file is `demo/psm/out/cdi/sh4der-pvr-suite14.cdi`, 14,888,297 bytes, sha256 `f240d86fe43e59d026e2afd5554bfe5a7040df098adb0e9cbe04544ff16a1fb0`, built 2026-11-26 (build log `D:/tmp/psmbuild/logs/suite_14_20261126_193332.log`).

**Status: built, not yet run on a console.** The owner will run it on her console later. Nothing on this page is a result.

## On the disc

- **The menu** (look B, the owner approved it): two tabs, BY DEMO and BY EFFECT. The list is on the left, with headings the cursor skips. The picked demo's card is on the right: WHAT IT IS, EFFECTS USED, CONTROLS, and COST only on the cards that have a measured number. The stick scrolls the card. There is no RUN ALL in this menu.
- **The keys on the menu:** the D-Pad moves the cursor and switches tabs with left and right. A or START starts the picked demo. B goes back (on BY EFFECT, to BY DEMO). Y opens the credits; Y or START goes back from them.
- **The place:** the menu puts its tab, row and scroll into the test's mail, and gets them back when the test ends, so it opens where it was left. A test built before v0.5 does not hand the place back, and the menu opens on the test's row by its index instead.
- **The heat haze** is in the menu under Driving, as Heat haze on a hot road.

## The hold of A: which programs use it

Two kinds of program return to the menu in two ways.

- **The six spikes with result pages** use `suite_page`: Room shading, four ways (`ao4`), Lamp with a lit shade (`ao-lamp`), Space soldier on a street (`hero1`), Glow that meets a wall (`d1`), Hens that see the room (`hens-eyes`), Copy video memory to RAM (`pvr-dma`). On the last result page of one of these, started from the menu: a tap of A is the next page; A held for 2 s (`SH4DER_MENU_HOLD_MS`) goes back to the menu, on the row the test was started from. A line under the page and a fill bar show the hold while A is held, and letting go before the time stays on the page. A + B + X + Y also goes back from any page, and B goes to the page before.
- **The eight programs with no hold** return by A + B + X + Y only. They are the seven street and heat programs, which use `suite_tick` (their source is `demo/pl4nar/src/main.c`), and the walled square, which is a live program with no result pages, so it has no hold: it hands the menu its place back on A + B + X + Y. The programs: Car in a city by day (`car-tech`), Car on hills (`car-hills`), Car on a wet night road (`car-wet`), Drive a car in a city (`drive-map`), Drive a car on hills (`drv-hills`), Drive a wet night road (`drv-wet`) and Heat haze on a hot road (`heat-haze`). They return by A + B + X + Y only. A on their last page does not go to the menu.
- **In a chained run** (RUN ALL, started from suite 13's menu, not this one) the hold does not apply: A on the last page is the next page.

The rules of the hold and the place are in [Test disc menu [suite]](../effects/suite.md) and [Menu logic [menu]](../effects/menu.md).

## What to look for when she runs it

Check each of these on the console and photograph or film it. Note any that do not hold.

1. **The menu:** the cursor skips the headings; left and right switch tabs; the stick scrolls the card's text; a card with no COST line has none.
2. **Start:** START starts the picked demo, and A starts it too.
3. **Return:** A + B + X + Y from a test brings the menu back on the same item the test was started from.
4. **The hold on one of the six:** on the last result page, hold A. The line and the bar fill. Let go before 2 s and the page stays; hold to 2 s and the menu comes back on the same row.
5. **No hold on the other eight (street, heat or square):** on its last page, A does not go back to the menu; A + B + X + Y does.
6. **Credits:** Y opens them; Y or START goes back.
7. **The redraw:** only the changed lines are redrawn. If the serial cable is on, the log's lines `suite: redraw lines y0 .. y1, N us` give the time.
8. **Heat haze on a hot road:** X steps the heat OFF, LIGHT, STRONG (the card says it starts on).
9. **The console revision:** the first line of each test carries it, so every photograph says which console it came from.

## The rows, ready for the run (planned, no results)

Each row is filed only from the video and the pages, with the video time (mm:ss.s) as the photo name. The IDs are proposed, and as for suite 13, the next free one is used if another row takes it first. No row has results yet.

| Row | Claim to fill | Status it can reach | Numbers from |
|---|---|---|---|
| VID-017 | The menu on screen: changed lines only, its redraw time from the serial log, the hold's bar and line in the video | MEASURED for the time (serial log); the look from the video | the serial log; the video |
| PVR-218 | Heat haze on a hot road (standalone-25): the frame, SH-4, PVR and TA with the heat on STRONG, against Car in a city by day's older layer (PVR-212) | MEASURED (one run); differences DERIVED | the standalone's pages |
| PVR-212 to PVR-217, SH4-087, BUS-054 | Suite 13's proposed rows: the same street and spike programs run again in suite 14 (the walled square has no result pages, so no hold to check). They are not filed twice | as in [suite 13](suite-13.md) | their pages |

## Related

- [Suite 13](suite-13.md): the thirteen programs, what each should show, and the rows they go to
- [Test disc menu [suite]](../effects/suite.md): the hold of A, the place in the mail, the way back
- [Menu logic [menu]](../effects/menu.md): the rules the menu uses
- [Demo guide](../start-here/demo-guide.md): what each program shows, with its code name
