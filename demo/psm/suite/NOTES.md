# Test suite disc (image psm-suite-s1): built

Built 2026-11-25 by Senior, with the four town binaries from Junior.

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
- **The fault page's way out** is a hook added to `sh4der_fault.h` (`sh4der_fault_leave`). Not set, the fault page stays for ever, as before.
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
| `include/sh4der_fault.h` | Gained `sh4der_fault_leave` (optional; behaviour unchanged when not set). |

## To add a test to a later disc (s2, never s1 again)

1. In the test: `#include "psm_suite.h"` after `sh4der_fault.h`; `suite_init();` first in main; `if (suite_tick(buttons, page, pages)) { page++; redraw = 1; }` in the result-page loop.
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

## Suite s2: built

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

## Suite s3: built

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
| The menu with the revision line starts and lists 11 tests | built |

## Suite s4: built

A short menu after the owner's runs 2 and 3.

| Item | Value |
|---|---|
| Image | `demo/psm/out/cdi/psm-suite-s4.cdi` |
| sha256 | 817962592b47567066f3d07ab8b4f542768bf84e2eb9f3487d43fc1b94a8451d |
| Size | 5,929,737 bytes |
| Build | `python demo/psm/suite/build.py --tag s4 --list demo/psm/suite/suite_s4.json` |

| Entry | What it is | RUN ALL |
|---|---|---|
| ao4 | the split-screen AO demo on simple scenes (`ao/NOTES.md`) | yes, alone |
| hud1c-1, hud1c-2, hud1c-3 | Junior's bisect of hud1b's hang at frame 1, each with the heartbeat lines | no: one of them may hang by design |
| ao1 | the town's AO demo, the same bytes as on s3 | no |

Dropped from s3: c4g, r3-d, qr-q2 and ao0 (their results are in), a3e-auto, a3f-auto, bake1-auto and hud1b-auto (town v1; hud1b is replaced by hud1c), a3e-t2 and a3e-poll.

**s4b and s4c (same day).** s4b: s4 with s3's ao1 replaced by Junior's ao1c and ao1c-live (the town with a CLAY view). s4c: s4b with ao4-b in place of ao4-a (a dummy polygon before each change of user clip, CORE 18). **s4c is the one to run:** `demo/psm/out/cdi/psm-suite-s4c.cdi`, sha256 36ab1fad1eaf58af80f6ddf535416508a1a1c8cadac0ef92b72884491b8367f4, 6,829,097 bytes, list `suite_s4c.json`. Menu: ao4 (RUN ALL runs it alone), ao1c, ao1c-live, hud1c-1, hud1c-2, hud1c-3. s4 and s4b are superseded.

---

# s5 (built)

`demo/psm/out/cdi/psm-suite-s5.cdi`, sha256 c90eca50eb792d06e0caab7f6b587a341623ef88f996016f07daa9e6b02d0d8d, 7,438,793 bytes. List: `suite_s5.json`. Built 2026-11-25 by Senior. Supersedes s4c: the same entries, with ao4-c in place of ao4-b and hero1 added.

| Entry | ELF sha256 | What | Read |
|---|---|---|---|
| ao4 (RUN ALL 1) | 340e0b83…29ec | AO side by side: spheres, a corner, the living room with the hens. About 70 s by itself | photo every page and the graph; START = live; L / R scene, X split, d-pad down in scene C = hens |
| hero1 (RUN ALL 2) | b444cf82…79c3 (Junior, h1b) | a skinned hero, pass costs, the placeholder room and the hens. About 50 s | photo every page; START = live |
| ao1c, ao1c-live | as s4c | the town's AO with a CLAY view | as s4c |
| hud1c-1, -2, -3 | as s4c | the bisect of hud1b's hang | photo the bottom two lines |

hero1 h1b was built with the placeholder room; the living room's data is ready for it (`build.py --room-only --room demo/psm/ao/room --out <dir>`).

---

# s5b and s6 (built)

- `psm-suite-s5b.cdi`, sha256 e524323ed3a414f363d8fc7018f7787f8fe150c64bc34c82ea9e1cf5c894d16c, 7,483,177 bytes: s5 with Junior's hero1 h1c (the living room; ELF sha 732b6943…658a) in place of h1b. List `suite_s5b.json`.
- **`psm-suite-s6.cdi`, sha256 c255e2346e7c352dd047bc8e0ab9e14e6d6a31b35b196040d6e2777739b7a493, 7,483,177 bytes: s5b with ao4-d (ELF sha 5c29e3ce…476a) in place of ao4-c.** List `suite_s6.json`. It supersedes s5 and s5b if neither has run.

| Entry | What | Read |
|---|---|---|
| ao4 (RUN ALL 1) | as s5, and scene C's mode 3+M (a tile a hen) with the splits 3 \| 3+M, 3+M \| 2, 3+M \| 3+M. About 90 s by itself | photo every page and the graph; START = live; L / R scene, X split, d-pad down in scene C = hens |
| hero1 (RUN ALL 2) | Junior's h1c: the skinned hero, the living room, the hens | photo every page; START = live |
| ao1c, ao1c-live, hud1c-1/2/3 | as s4c | as s4c |

---

# s6b (built)

`psm-suite-s6b.cdi`, sha256 38bf0f23c8c1239f61d63825b539943a38aa72c1933f21e8947ecea6ddaee7ac, 7,936,361 bytes. List `suite_s6b.json`. s6 with one entry added: **hens-eyes** (RUN ALL 3; ELF `build/psm_ao/psm-hen-a.elf`, sha df0b5076…2d16): rays against a collision sweep for the same hens in the room, about 50 s by itself; photo every page and the graph; START = live (X mode, Y hens, A rays drawn, B tick, up / down fan). It supersedes s5, s5b and s6 if none has run. Notes: `ao/NOTES.md` "hens-eyes".

---

# s7: ready, NOT BUILT (one disc at a time)

`suite_s7.json` is ready: s6b with ao4-e in place of ao4-d and the entry pvr-dma added (RUN ALL: ao4, hero1, hens-eyes, pvr-dma). One command when lead says: `python demo/psm/suite/build.py --tag s7 --list demo/psm/suite/suite_s7.json --mkdcdisc <mkdcdisc>` (here: `sh /d/tmp/psmbuild/suite_img.sh '' s7`). ELFs: ao4-e 1ad24f15…7924, pd-a 98c7ff38…b711, hen-a df0b5076…2d16, hero1 h1c 732b6943…658a.

The current disc is **s6b**, the only suite disc in `out/cdi`. s5, s5b and s6 were superseded and recycled by lead. From 2026-11-25 lead builds the discs: Senior delivers ELFs, their sha256 and the suite's list.

**s7's list as of 2026-11-25 (still not built):** ao4-e2 (sha e002381d…8a08), hero1 h1d (Junior, sha 131647dd…5053), hens-eyes hen-b (sha bc71fa68…f3c2), pvr-dma pd-a (sha 98c7ff38…b711), then ao1c, ao1c-live, hud1c-1/2/3 as on s6b. All three room programs are on the room data with the trim lit by its wall's normal.

---

# s7 (built by lead 2026-11-25)

`psm-suite-s7.cdi`, sha256 1af06472f589c0d567394bbc77e6fff075893512d241d9c372116647861e93dc, 7,938,697 bytes (lead's build; commit a26f3c2). List `suite_s7.json`. It replaces the "ready, NOT BUILT" list above.

**The menu has two levels** (`suite_nav.h`, pure, host test `test/suite_nav_test.c`; `suite_menu.c` draws what `nav_grid` gives; the screens as text: `SCREENS_s7.txt`). TOP: TECH DEMOS / DEVELOPER TOOLS. LIST: a group's entries, RUN ALL the group's own (tools once through, demos round and round); B or START back, Y the credits. A list with one group has no TOP, so older lists build as before. A test's "group" is "demos" or "tools" (default tools). `psm_suite.h` and the mail between the menu and a program are unchanged.

**The foot and the CREDITS screens.** Every menu screen ends with the copyright line and "Y: CREDITS". The credits' text is `../psm_credits.h`, GENERATED by `../tools/make_credits.py --kos <KallistiOS tree>` from the licence files themselves (SH4ZAM's LICENSE, KallistiOS's LICENSE.KOS and AUTHORS); `build.py` runs its `--check` and fails if the header no longer matches them. A test with `"credits": "pl4nar"` adds the street demo's art page. New `build.py` options: `--title`, `--prefix`.

| Group | Entry (RUN ALL) | ELF sha256 |
|---|---|---|
| TECH DEMOS | ao4 (1), ao4-h | a77eb2ed…58de |
| | ao-lamp (2), lamp-c | c41122d8…9c87 |
| | hero1 (3), Junior's h1e | 75b96222…f334 |
| | pl4nar (4), Junior's c5 | 9a661de1…40eb |
| DEVELOPER TOOLS | d1 (1), d1-a | 046e692e…907a |
| | hens-eyes (2), hen-d | 6ea63a33…3744 |
| | pvr-dma (3), pd-c | a902ad6f…8575 |
| | ao1c-live, hud1c-2 (not in RUN ALL; old ELFs, no effect labels) | 3021e298…7bd2, e6b7fb3f…3377 |

MENU.BIN 209,924 bytes, sha256 bb426e59…65da. Notes on the entries: `../ao/NOTES.md` (labels, divider, ao-lamp, d1).

**OPEN: Newlib's notices.** The programs link Newlib (94 objects of libc), whose BSD and AT&T notices must go with a binary ("in the documentation and/or other materials provided with the distribution"); s7 carries a sentence that names Newlib, not the notices. libgcc needs nothing (GCC Runtime Library Exception 3.1). Proposed, not done: COPYING.NEWLIB as a text file on every disc. Reported to lead 2026-11-25; the owner decides.

---

# s7 ran (2026-11-25, one console); s8's list is ready, NOT BUILT

**s7's run.** The owner recorded it (`s7.mkv`). The disc booted, both menu levels worked, every entry that was started returned to the menu, no hang and no fault page. What the entries showed: `../ao/NOTES.md` "s7's console run"; the ledger rows are Scholar's.

**s8** (`suite_s8.json`; lead cuts the image when Junior's pl4nar ELF is in). s7 with d1-b, lamp-d, ao4-i, hen-e and pd-d in place of d1-a, lamp-c, ao4-h, hen-d and pd-c (shas in `../ao/NOTES.md`); hero1 and pl4nar as on s7 until Junior's new ELFs; ao1c-live and hud1c-2 as on s7. `--no-image` check on 2026-11-25: all nine return to the menu.

**The menu's credits changed** (`../tools/make_credits.py`, `../psm_credits.h` regenerated): after the line that names SH4ZAM-DCE, "SH4ZAM-DCE is based on SH4ZAM but is not affiliated with, endorsed by or maintained by Falco Girgis or the SH4ZAM project; report its problems to SH4DER-PVR, not to them."; and the second page's heading is "SH4ZAM (and SH4ZAM-DCE, with the same notice): MIT License" (the owner's wording "which carries the same notice" is 67 columns; the page is 64 wide and has no line to spare). MENU.BIN for s8's list as it stands: 210,128 bytes, sha256 a59b4c35370fbe19fb6d27457818f3a07734e9cf25ec355190d9c25760f64a3e; it changes again if an entry's name or text changes.

**Still OPEN:** Newlib's notices on the disc (the owner's decision).
