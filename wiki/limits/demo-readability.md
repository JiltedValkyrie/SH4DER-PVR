---
title: "Demo readability: the standard for every demo and test disc"
tags: [limits, practice, demos, suite]
updated: 2026-11-25
sources: [wiki/effects/names.md, wiki/limits/header-recommended.md, wiki/hardware/ledger-pvr.md, wiki/hardware/ledger-video.md]
---
# Demo readability: the standard for every demo and test disc

**What you will learn:** the six rules every demo program and every test disc keeps, so that someone who has never seen it knows what it shows, what to look at and what the numbers mean.
**For:** whoever writes or changes a demo, a spike or a suite program, and whoever films a console run.

The owner's finding: most of our demos are very hard to read. Someone watching cannot tell what a stage changes, which key does what, or whether a result is good. The same gaps cost us measurements: runs stopped too early, stages with keys pressed, and a video filed under the wrong disc name (PVR-186, PVR-187, PVR-188).

## The six rules

| # | Rule | What it means on screen | Done when |
|---|---|---|---|
| 1 | **The first screen says what this is.** | Two lines before the first stage: what the program shows, and what to look at. Example: "Ambient occlusion: four ways side by side. Look at the floor under each hen." If the run must not be touched, the first screen says so, with how long it takes: "Hands off for 2 minutes: the last two stages measure the tile cut." | A viewer can say what the program is for from that screen alone. |
| 2 | **Every stage or row has a plain title, and names the one thing to compare.** | A title that says what changes, never a code name: "Shadow follows the sun", not "S3B". Under it, one line: what to compare ("left: baked AO; right: no AO. Look at the corner"). The stage number and the count stay ("stage 4 of 19"). | No stage title needs the source code to understand. |
| 3 | **The controls are on screen, only the ones that work now, from one table.** | One line of keys, and only the keys that do something in this mode. The line is printed from the same table that drives the pad, so the two cannot drift apart. In a measured stage, the line says what a key press does to the measurement ("any button: stop and show the pages"). | One table in the code; a host test checks that every listed key does something and every handled key is listed. |
| 4 | **A result page starts with a plain verdict.** | The first line after the title says what the numbers mean, in words: "Baked AO costs 0.2 ms; the PVR map costs 7.5 ms more." Or: "Not measured: the run was stopped before the cut stages." Then the numbers. A stage cut short or touched says so on the page. | A reader who reads only the first line of each page gets the result. |
| 5 | **Names follow [names](../effects/names.md).** | On-screen titles use the plain technique names ("planar reflection", "screen spotlight", "normal map"), not the old module names ("pl4nar", "torch", "bump"). An old name may follow in brackets while both are in use. | No title uses an old module name alone. |
| 6 | **No game or engine names.** | No game, engine or product name in any on-screen text, title, page or file name a viewer sees. | A text search of the program's strings finds none. |

## Also expected

- **The disc's volume name** matches its file name: what the menu shows identifies the disc. The C9T video was filed as "hills" because the two names differed only by one letter (PVR-188).
- **A label on every live view**: the mode and the stage, so a frame grabbed from a video can be filed without guessing.
- **Stage times** long enough to read a page (the suite pages turn every few seconds).
- **The page kit's CRC** stays on every text page (`demo/psm/psm_page.h`), so a transcription can be checked.

## Related

- [Names](../effects/names.md): the plain names
- [Header RECOMMENDED block](header-recommended.md): the same idea for headers
- [Frame budget](frame-budget.md)
- [Video ledger](../hardware/ledger-video.md)
- [Demo readability audit](demo-readability-audit.md): every program scored, 2026-11-25
