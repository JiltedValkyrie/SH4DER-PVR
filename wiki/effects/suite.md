---
title: "Test disc menu [suite]: chaining programs on one disc"
tags: [effect, suite, disc, menu, arch_exec, return]
updated: 2026-11-26
sources: [include/sh4der_suite.h, test/suite_test.c, demo/psm/psm_suite.h, demo/psm/suite/suite_menu.c, demo/psm/suite/NOTES.md, demo/psm/suite/build.py, wiki/hardware/ledger-pvr.md "PVR-116", wiki/status/maturity.md "suite menu", README.md "The suite (chaining programs on one disc)"]
---
# Test disc menu [suite]: chaining programs on one disc

**What you will learn:** how one disc holds a menu and many test programs, how a test hands the console back to the menu and the menu starts the next test, what a start through KallistiOS clears and does not clear, what has run on a console and what is still open.
**For:** a programmer with several console test programs who wants them on one disc, reached from a menu, with a way back after each one.

> **In plain words.** One disc, one menu, many test programs. The menu starts a test. The test shows its result pages and, when it is done, starts the menu again, so the console is never switched off between tests. The two programs leave each other a few words by writing them into each other's program before the start. A test built with this header still runs on a disc of its own: with no menu on the disc, every call does nothing.

## Try this first

What it is for: the menu, a test, and the way back, on one disc.

1. Build the disc from a list of tests: `python demo/psm/suite/build.py --tag <tag> --list <list.json> --out <folder>` (the list is one of `demo/psm/suite/suite_*.json`). The menu is `/cd/MENU.BIN`; every test is a file the menu starts. This build is the demo's, not the library's.
2. In the test's `main`, after `<kos.h>` (and after `sh4der_fault.h` when the test has it): call `suite_init();` first, while all is well. It reads the menu's binary into RAM.
3. In the loop that shows the result pages, every pass: `if (suite_tick(buttons, page, pages)) { page++; redraw = 1; }`. It goes back to the menu when A + B + X + Y are held together.
4. Boot the disc. The menu starts. Start one test from the menu, then press A + B + X + Y: the menu should come back. On the suite 13 menu (`demo/psm/suite/suite_menu.c`), start RUN ALL: the tests run one after another and each goes back to the menu. The suite 14 menu (`demo/psm/suite/suite_view.h`) has no RUN ALL.
5. Read the result: a started test's pages say "ABXY menu" (`suite_keys_text`), and the first line of each test carries the console's revision (`suite_rev_text`). Photograph it.

Reference: the manual is at the top of `include/sh4der_suite.h`; the host test is `test/suite_test.c` (run by `make test`). Status: ALPHA v0.4 (`wiki/status/maturity.md`, "suite menu").

## What it does

The disc holds a menu program and every test program as a file. A start from the menu loads the test's binary and starts it in the menu's place with KallistiOS's `arch_exec`. A test that is done loads the menu's binary and starts it the same way. Nothing is switched off in between.

Each program carries one 32-byte mail struct in its data: a 16-byte mark (`PSM-SUITE-MAIL-1`, the text the menu and every test match on byte for byte), then three words, `chain`, `index` and `status`. Before a start, the leaving program writes its words into the other program's image, found by the mark. Exactly one mark must be in the image: none, or two, and the mail is refused. The search steps by 4 bytes, so a mark at an odd offset is not found.

- `chain` 1 means "run all": the test turns its own pages and goes back after the last; the menu then starts the next one.
- `index` is the menu's row of the test, so the menu puts its cursor back there.
- `status` is how the test ended: `SUITE_OK`, `SUITE_KEYS` (the four buttons) or `SUITE_FAULT` (from the fault page).

## What a start clears and does not clear

From KallistiOS's source (`kernel/arch/dreamcast/kernel/exec.c` and `init.c`; read, NOT MEASURED): the leaving program runs KallistiOS's whole shutdown, puts back the status registers it was started with, turns the cache off, copies the new binary to its place and jumps there. The new program starts as from a boot: its own cache setting, its own `.bss` cleared, every KallistiOS subsystem started again.

Not cleared: main RAM outside the new program's image and `.bss` (the heap keeps what was there), the sound RAM, the PVR registers that neither `vid_init` nor `pvr_init` writes (the fog table and the palette), the drive's state, the real-time clock, and what a VMU shows. A test that reads any of those before it writes them reads the last program's values. The mail is kept nowhere else, so nothing in the mail depends on RAM that the start leaves alone.

## Call order

- `suite_init()`: once, first in `main`. Returns 1 on a suite disc, 0 on a disc of the test's own.
- `suite_tick(buttons, page, pages)`: every pass of the result-page loop. Returns 1 when a chained run's page time is up and there is a next page; it does not come back when the four buttons are held, or when a chained run has shown its last page.
- `suite_chained()`: 1 in a chained run. Skip whatever waits for a button.
- `suite_keys_text(alone)`: the "ABXY menu" line for a result page, or `alone` with no menu.
- `suite_rev_text(buf, size)`: "HOLLY REV xxxx SB xx" from two register reads (the PVR core's REVISION and the system bus's SB_SBREV), nothing written.

With `sh4der_fault.h` (`SH4DER_FAULT_IMPL`) in the same program, `suite_init` also has the fault page go back to the menu after `SUITE_FAULT_SHOWS` redraws (60 s). That is an attempt from a broken program: it may hang.

## What the runs showed

All on one console, and all from the suite's own notes and ledger, with the numbers in the rows named.

| Claim | Status | Where |
|---|---|---|
| A start from the menu repeated Shadow chain with measured edges' pixel counts and times (within about 1 %) | MEASURED, one test, not the same binary as its own disc | [PVR-116](../hardware/ledger-pvr.md) |
| Whether a menu start can change a result | OPEN | [PVR-116](../hardware/ledger-pvr.md) |
| Every started test's result pages carry "ABXY menu" (s1) | MEASURED | `demo/psm/suite/NOTES.md`, "First run of s1" |
| c4g was started by RUN ALL (its header says "(auto)") | MEASURED | `demo/psm/suite/NOTES.md`, "First run of s1" |
| The owner's s7 video: both menu levels worked, every entry started returned to the menu, no hang, no fault page | recorded by the owner; the ledger rows of that run do not state the return | `demo/psm/suite/NOTES.md`, "s7 ran" |
| The four-button return in a photo, the 5 s countdown, a chained test going back by itself, the fault page's return | OPEN: no photo | `demo/psm/suite/NOTES.md`, "First run of s1" |
| The first read of `/cd/MENU.BIN` by a test started through `arch_exec` | worked in the six tests of s1 | `demo/psm/suite/NOTES.md` |

The autorun pages (a test that boots into itself and ends on result pages) are a different row: `wiki/status/maturity.md`, "autorun". The suite menu row is ALPHA v0.4; it moves only with a MEASURED row that shows the return and the chain, which the runs above do not yet give.

## Recommended limits

- One menu and one list per disc (ADVISED). The build refuses a binary that does not carry one mail.
- A fault ends a chained run: the menu does not start the next test by itself after a fault page (`demo/psm/suite/NOTES.md`; the fault page's return may hang).
- Do not keep state in RAM across a start that the new program would need; keep it in the mail or on the disc (ADVISED, from the clears above).
- Do not change the mark's text (the menu and every test match on it).

## Tuning

None. The header has no `SH4DER_*` macro. Its constants belong to the disc and are set in the header: `SUITE_PAGE_MS` (20000 ms), `SUITE_FAULT_SHOWS` (120), `SUITE_IMAGE_MAX` (12 MB).

## Not measured and open

- The start itself: how long KallistiOS's shutdown and boot take. NOT MEASURED.
- The menu's binary size in RAM. NOT MEASURED here.
- The four-button return and the chain's own run, in a photo. OPEN.
- Whether a menu start can change a result (PVR-116 is one test). OPEN.

## The last result page: hold A for 2 s

On a test's last result page, `suite_page(buttons, &page, pages)` does the page turning in one call (v0.5). A tap of A goes to the next page, and B to the one before. A held for 2 s (`SH4DER_MENU_HOLD_MS`) on the last page goes back to the menu, on the row the test came from: a line under the page and a fill bar show the hold while A is held, and letting go before the time stays on the page. The hold exists only in a test started from the menu and not in a chained run (RUN ALL): there, A on the last page is a plain next page. A + B + X + Y still goes back to the menu, as before. On a disc of the test's own there is no menu: A and B turn the pages and nothing else happens.

The menu's place (tab, row and scroll, one word packed by `sh4der_menu_pack`) travels in the mail's spare word when the menu starts a test, and `suite_exit` hands it back, so the menu opens where it was left. A test built before v0.5 does not hand it back; the menu then opens on the test's row by its index, as before.

Status: v0.5 for the pager, the hold and the place (host tested, `test/suite_test.c`). New, not yet on a disc: the owner will run it on her console later. The suite 14 menu itself (`demo/psm/suite/suite_view.h`: BY DEMO and BY EFFECT tabs, a card for the picked demo, look B, no skin) has no RUN ALL.

## Related

- [Menu logic [menu]](menu.md): the rules the suite menu and the pager use (the hold, the list, the scroll).

- [What do you want to make?](../start-here/what-do-you-want.md): the starter, for a picture or a goal.
- [Tools and safety](../start-here/genre-tools-and-safety.md): the genre guide that puts the menu with the other tools.
- [Call orders](../start-here/call-orders.md): where each effect's calls are written.
