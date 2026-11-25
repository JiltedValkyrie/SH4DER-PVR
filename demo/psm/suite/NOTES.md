# Test suite disc (image psm-suite-s1): built, not run

Built 2026-11-25 by Senior, with the four town binaries from Junior. **Not run on a console.** Nothing below is a hardware result. The whole return path (a test starting the menu, the menu starting a test) has never run anywhere; the first boot of this disc is its test.

## What it is

One disc in place of seven. The disc boots a menu. Each test is its own binary, a file on the disc. The menu starts a test; the test goes back to the menu when it is done. The console is not switched off between tests.

| Item | Value |
|---|---|
| Image | `demo/psm/out/cdi/psm-suite-s1.cdi` |
| sha256 | 57d2584b692ab039816f4e9bf419d78c1ca7408de4c5cfbd7fe125e2ae14e438 |
| Size | 6,474,025 bytes |
| Build | `python demo/psm/suite/build.py --tag s1 --list demo/psm/suite/suite_s1.json` |
| Log | `D:\tmp\psmbuild\logs\suite_s1_20261125_100835.log` (no compiler warning) |
| Manifest | `build/psm_suite/s1/manifest.txt` (every file's bytes and sha256) |

## The tests on it, and what to read from each

| # in RUN ALL | Menu name | File | What it is | Read this |
|---|---|---|---|---|
| 1 | c4g | C4G.BIN | The PVR shadow chain: bias, sweep, band cut, frame waits. Same tests as c4f. | Photograph every text page (about 12). The 8 graph pages if wanted. |
| 2 | r3-d | R3D.BIN | The punch-through product: 7 lines and the fade curve. Same tests as r3-c. | Photograph all 4 pages: 2 text, the graph, the picture. |
| 3 | a3e-auto | A3E.BIN | Town a3e on the fixed script, then result pages. No pad needed. | Photograph every page (text, 2 graphs). |
| 4 | a3f-auto | A3F.BIN | Town a3f (the cache pads) on the same script. | Photograph every page. It is read against a3e-auto. |
| 5 | bake1-auto | BAKE1.BIN | Town with the static bake on the same script. | Photograph every page. The "bakes" lines are the result. |
| 6 | hud1-auto | HUD1.BIN | Town HUD loss test: three states, the DIAG box. | Watch whether the HUD goes. Photograph every page (the "events" lines). |
| not in it | qr-q2 | QR.BIN | The QR reading test: 16 codes at 10, 8, 6, 4 px. Same set as qr-q1. | Phone at each of the 7 code pages; one photo a page; Y changes the panel. |

- The run times shown in the menu ("~1 min", "~2 min") are ESTIMATE. None was measured.
- c4g, r3-d and qr-q2 are c4f, r3-c and qr-q1 with three added lines each for the suite. No test in them changed.
- The town binaries are Junior's autorun builds with the same three hooks (`demo/psm/patches/auto_suite.diff`).
- Older benches (ray2, h2, the fault test) are not on s1. They have no way back to the menu, and their results are already in the ledger.

## How the owner uses it

1. Boot the disc. The menu lists the tests.
2. **Up / down** picks. **A** starts the test.
3. In a test, on its result pages: **A / B** turn the page as before. **Hold A + B + X + Y** to go back to the menu.
4. **RUN ALL** (top row) runs tests 1 to 6 in order. Each test turns its own pages, 20 s a page, then goes back; the menu waits 5 s and starts the next. **B** during those 5 s stops the run.
5. After the last test the menu says "RUN ALL is done".

RUN ALL takes long: about 20 pages of c4 alone are 7 minutes at 20 s a page (ESTIMATE). For photographs taken at leisure, start each test by hand instead.

## When the console must still be switched off

| Case | What happens |
|---|---|
| A test shows a fault page (exception, hang, assert) | The page counts down 60 s ("PHOTOGRAPH NOW"), then tries the menu. This is a try from a broken program. If the picture stays, switch off. |
| A test hangs with interrupts masked | Nothing is shown, as before. Switch off. |
| The CPU resets (an exception inside the exception entry) | The console goes to its boot screen and boots the disc again: the menu comes up by itself. |
| The menu cannot read a file | It says so on its last line and stays in the menu. |

A fault ends RUN ALL: the menu does not start the next test by itself after a fault page.

## How it works

- **Same address.** Every binary is linked to load and start at 0x8c010000. The build checks each ELF's entry and first load address, and refuses any other.
- **Start.** The menu reads a test's binary from the disc into its heap and calls `arch_exec` of KallistiOS, which copies it to 0x8c010000 and jumps there.
- **Return.** A test reads `/cd/MENU.BIN` into its heap at its very start (`suite_init`), while everything is well. Going back needs no disc read. `MENU.BIN` is the same program as the boot file.
- **No menu on the disc** (a test on a disc of its own): `suite_init` finds no file and every suite call does nothing. The same ELF works alone.
- **State across a start.** The chain flag, the menu row and how the test ended are written into a marked 32-byte struct inside the other program's image before it is started. Nothing depends on RAM that survives a start. The build counts the mark in every binary: exactly one, or the binary is refused.
- **The fault page's way out** is a hook added to `psm_fault.h` (`psm_fault_leave`). Not set, the fault page stays for ever, as before.
- **The menu's disc read and the watchdog.** A read can block longer than the fault catcher's 2 s. A second thread gives the heartbeat during the read, for 30 s at most.

## Clean state: what a start through arch_exec does

From the KallistiOS source (`kernel/arch/dreamcast/kernel/exec.c`, `init.c`, `hardware/hardware.c`, `hardware/asic.c`, `hardware/pvr/pvr_init_shutdown.c`, tree 568c3404). **FACT from source; not measured on a console.**

| Reset or shut down by the start | How |
|---|---|
| PVR | `pvr_shutdown` (by the suite, with interrupts on, before the exec): core reset, its six events unhooked, PVR DMA shut, video memory cleared. Then `pvr_init` by the new test, with the test's own parameters. |
| Vertex buffer size, bin sizes | The test's own `pvr_init` parameters. hud1's 1,024 KB and its bin pool belong to hud1's binary alone. |
| ASIC interrupt masks | All event masks written to 0 at shutdown, and again at the new program's `asic_init`; pending events acknowledged there. A test's own event handlers (Junior's counters, c4's render-done hook) are gone and are hooked again by the test that wants them. |
| Timers | `timer_shutdown`, then `timer_init`. The ns clock starts again. |
| Cache | Turned off and invalidated before the copy (CCR = 0x808), set again by the new program. OC-RAM mode does not carry over. |
| Performance counters | Started again by the new program's init. |
| Maple, CD, sound, G2 DMA, video mode, threads, file systems | Shut down, then started again. |
| .bss | Cleared by the new program. |
| SR, VBR, FPSCR | Put back to what the leaving program was started with. |
| Watchdog timer, user break controller | Disabled by the new program's first lines. |
| `KOS_INIT_FLAGS` | Each binary's own. |

| NOT cleared by the start | Why it matters |
|---|---|
| Main RAM outside the new image and its .bss (the heap's old contents) | A test that reads heap memory before writing it reads the last program's bytes. |
| PVR registers that neither `vid_init` nor `pvr_init` writes: the fog table, the palette, and any register a test set by hand | A test that relies on a boot value of such a register may see the last test's. OPEN: which registers; none of our tests is known to rely on one. |
| Sound RAM | No test here uses sound. |
| The drive's state, the real-time clock, a VMU's screen | Not used by a result. |
| Store queue addresses (QACR) | Each user sets them before use. |
| Temperature of the console | A test late in RUN ALL runs on a warmer console than a cold boot. Not known to matter. |

**The rule "a result counts only from a clean state".** A start through the menu is a full KallistiOS shutdown and a full start. It is not a power cycle. Whether any result differs between the two is OPEN. The test of it is cheap: run one test from the menu and once from its own disc (c4f against c4g, a3e-auto against A3E.BIN) and compare the pages.

## Host checks (all pass)

| Check | Result |
|---|---|
| `suite/test/suite_test.c`: finding the mail in an image (none, one, two, a part, at the end) | pass |
| Every ELF: SH-4, entry and load address 0x8c010000 | 8 of 8 |
| Every binary: exactly one mail mark, at a multiple of 4 | 8 of 8, so all 7 tests return |
| Every image and .bss end below the stack's room | highest end 0x8c304a88 (c4g), of 0x8d000000 |
| The town ELFs' sha256 against Junior's message | 4 of 4 match |
| The image holds all 9 file names | yes |
| Compiler warnings: menu, c4g, r3-d, qr-q2 | 0 |

## Not tested, and the risks

- **Nothing here ran on a console.** The start, the return, the chain and the fault page's return are all untested. Emulators are not evidence and were not used.
- **The first read of `/cd/MENU.BIN` by a test started through arch_exec.** It relies on KallistiOS starting the CD drive again cleanly. If it fails, the test still runs, but as a test alone: no way back, switch off after it. The page header then shows the test's old keys text and not "ABXY menu". That header is the sign to look for.
- **The fault page's return** runs KallistiOS's shutdown from inside a fault. It may hang.
- **Largest binary.** 813 KB is read from the disc before each town test: a few seconds (ESTIMATE).

## Files

| File | What |
|---|---|
| `demo/psm/psm_suite.h` | The kit: the mail, `suite_init`, `suite_tick`, `suite_exit`, the menu's loader. Its header comment is the manual. |
| `demo/psm/suite/suite_menu.c` | The menu. |
| `demo/psm/suite/build.py` | Checks the ELFs, writes the list, builds the menu and the image. Builds no test. |
| `demo/psm/suite/suite_s1.json` | The table of s1: names, ELFs, order, the two lines of each test. |
| `demo/psm/suite/test/suite_test.c` | Host test. |
| `demo/psm/psm_fault.h` | Gained `psm_fault_leave` (optional; behaviour unchanged when not set). |

## To add a test to a later disc (s2, never s1 again)

1. In the test: `#include "psm_suite.h"` after `psm_fault.h`; `suite_init();` first in main; `if (suite_tick(buttons, page, pages)) { page++; redraw = 1; }` in the result-page loop.
2. Build its ELF with its own build.
3. Add a row to a new table `suite_s2.json` and run `build.py --tag s2 --list ...`.

## First run of s1 (2026-11-25, one console): what it showed about the disc

Rows with photo names: `compare/SUITE_RUN1_RESULTS_senior.md`, `bench/RESULTS_suite_cortana.md`, and Junior's rows in `TOWN_NOTES.md`.

| Claim | Status |
|---|---|
| The menu started c4g, r3-d, a3e-auto, a3f-auto, bake1-auto and qr-q2, and each loaded `/cd/MENU.BIN` after its start: every one of their result pages carries "ABXY menu" | MEASURED |
| c4g was started by RUN ALL: its header says "(auto)" | MEASURED |
| c4g from the menu gives c4b's counts to the pixel and c4b's times within about 1 % | MEASURED; one test, not the same binary |
| The page CRC works on a console: 10 text pages and 2 bar-graph pages typed by two readers agree with their headers | MEASURED |
| hud1-auto did not reach a page: after its boot menu the screen went black and stayed (TOWN-106) | REPORTED by the owner; the test named by Junior's reading |
| A return to the menu, the 5 s countdown, a chained test going back by itself, a fault page's countdown | not in any photo: OPEN |

- The risk listed above as the first one (the test's read of the menu file after a start) did not happen in six tests.
- hud1's hang is before its first frame, where the fault catcher is not yet armed. Nothing on the menu's side differs for HUD1.BIN: same address, a size between a3e's and bake1's, the same bytes on the disc as built.

## Suite s2: built, not run

s2 is s1 with **hud1b-auto in place of hud1-auto**. Nothing else changed: the other six binaries are the same bytes as on s1 (same sha256 in both manifests). The menu was built again for the new list.

| Item | Value |
|---|---|
| Image | `demo/psm/out/cdi/psm-suite-s2.cdi` |
| sha256 | 4c765d64a804f9072a8627f662fe69d3c86a2c52202664606b3dc9d1db7e6606 |
| Size | 6,474,025 bytes |
| Build | `python demo/psm/suite/build.py --tag s2 --list demo/psm/suite/suite_s2.json` |
| Log | `D:\tmp\psmbuild\logs\suite_s2_20261125_105214.log` |
| HUD1B.BIN | 805,812 bytes, ends 0x8c2df524, one mail mark; ELF sha256 4978110f…df63 as Junior gave it |

**hud1b-auto** (Junior's, `demo/psm/patches/hud1b.diff`): the fault catcher is armed first, and each boot step prints a "BOOT n <stage>" line to the screen. What the owner photographs is one of three: the town and then its pages; a HANG or EXCEPTION page that names the step; or a still screen with the BOOT list, whose last line is the step that hung.

Menu row 6 reads: "HUD loss test, boot steps printed. ~3 min" and "photo the pages; if it stops, photo the BOOT list/fault".

## Suite s3: built, not run

s3 is s2 plus four tests, with the menu's RUN ALL order changed. The seven binaries of s2 are the same bytes (same sha256 in both manifests). The menu was built again.

| Item | Value |
|---|---|
| Image | `demo/psm/out/cdi/psm-suite-s3.cdi` |
| sha256 | b41afae5d0553344725c6738855dac861456e6983922186dfb26b5c81059a244 |
| Size | 9,496,809 bytes |
| Build | `python demo/psm/suite/build.py --tag s3 --list demo/psm/suite/suite_s3.json` |
| Log | `D:\tmp\psmbuild\logs\suite_s3_20261125_111702.log` |

**New entries.**

| Entry | What it is | ELF sha256 |
|---|---|---|
| ao0 | AO method 2's hardware questions (`ao/NOTES.md`): the depth union, one side only, the cost a direction | bd154e0e…7c78 |
| ao1 | Junior's town AO demo: OFF, 1 baked, 2 the PVR map (K 4, union), on the town with the arch fixed | 5e5b3e32…876d |
| a3e-t2 | Junior's a3e-auto-t2: a3e on the town with the arch fixed, KallistiOS's waits timed | bb8113b7…f2e8 |
| a3e-poll | the same with the PVR waits polled | a339c051…4e55 |

**RUN ALL** runs, in this order: ao0, ao1, a3e-t2, a3e-poll, hud1b-auto. These are the five that have given no pages yet. c4g, r3-d, a3e-auto, a3f-auto, bake1-auto and qr-q2 ran on s1 and are picked by hand.

**Town v1 and v2.** a3e-auto, a3f-auto, bake1-auto and hud1b-auto are the builds of s1 and s2, on the town before the arch fix (v1). ao1, a3e-t2 and a3e-poll are on the fixed town (v2). The menu rows say which.

**The console's revision on the menu.** `psm_suite.h` has `suite_rev_text()`: two reads through the uncached mirror, the PVR core's REVISION (0xA05F8004) and the system bus's SB_SBREV (0xA05F689C); nothing is written. The menu prints "HOLLY REV xxxx SB xx" at the right of its title line. ao0 prints it as line 3 of its first page; Junior's three new town builds print it on page 1. c4g, r3-d and qr-q2 were not rebuilt for it.

| Claim | Status |
|---|---|
| The console's Holly and system-bus revisions | OPEN until a photo of the s3 menu |
| The menu with the revision line starts and lists 11 tests | built; not run |
