---
title: "page (result pages for console tests)"
tags: [effect, page, text, graph, result]
updated: 2026-11-26
sources: [include/sh4der_page.h, test/page_test.c, tools/page_crc.py, README.md "The page kit (result pages)", wiki/hardware/ledger-video.md (VID-015, VID-016), demo/psm/psm_page.h (forward)]
---
# page: result pages for console tests

**What you will learn:** how a console test prints its numbers on a page that can be photographed and checked, how the CRC line works, what the graphs are for, and what is and is not measured.
**For:** a programmer who runs timing or capture tests on a Dreamcast and needs the results on the television, readable and checkable.

> **In plain words.** A test program writes its numbers as lines of text, and this kit draws them as pages on the television: 64 characters a line, 22 lines a page, with a title and a checksum at the top. Someone photographs the page, types it back, and the checksum tells whether the typing matches the console. Graph pages show the shape of a result (a line over frames, bars over passes), and the exact numbers stay on the text pages.

## Try this first

What it is for: showing a test's numbers on screen, with a CRC line that a typed copy can be checked against.

1. After the video mode is 640 x 480 RGB565, call `pg_init()` once (`include/sh4der_page.h`, the function block near `pg_init`).
2. Write the lines with `pg_say("...")`; call `pg_break()` where a new page should start.
3. Add graphs with `pg_plot`, `pg_series` and `pg_point` (or `pg_cat` for bars).
4. Show page `n` with `pg_draw(n, title, keys)` after the PVR is shut down. `pg_pages()` gives the count.
5. To check a photographed page: type its lines into a file and run `python tools/page_crc.py page.txt <CRC from the header>`.

Header: [include/sh4der_page.h](../../include/sh4der_page.h). Call order: [below](#call-order).

## What it does

The text is drawn once into RAM from the BIOS font (12 x 24) and reduced to 8 x 16 by a 3-to-2 box filter, with brightening so that a one-pixel stroke stays bright. A page is 64 x 22 characters under a header line (the title, the page number, the keys, and `CRC xxxxxxxx`), inside the title-safe area x 64 .. 575, y 48 .. 431. A line longer than the page goes on, indented, on the next line; the cut is at a space in the line's second half, so a number is not split.

The CRC is the CRC-32 (zip's) of the page's content: each text line as shown, followed by a line feed, up to the last line that is not empty. For a graph, the content is the title, then each series' name and each point's y value rounded to a whole number. `tools/page_crc.py` makes the same sum from a typed page, and it also tries one more trailing space where the console keeps one at a cut.

Graphs are lines with point marks or grouped bars, up to four series, up to 16 graphs in a run. The axes start at 0 unless a series goes below; the top is rounded to 1, 2 or 5 times a power of ten.

Drawing is by the SH-4 into the frame buffer after the PVR is shut down. No PVR state, no list, no frame cost.

## Call order

1. Once, after the video mode is set: `pg_init()`.
2. As the test runs: `pg_say()` for each line; `pg_break()` for a page break; `pg_plot()`, `pg_series()`, `pg_point()`, `pg_cat()` for graphs.
3. To show a page: `pg_draw(page, title, keys)`. `pg_page_crc(page)` gives its CRC.

Settings are the kit's own macros, defined by the client before the include: `PG_PREFIX` (the serial line's prefix), `PG_BIG` (1 for the BIOS font's own size, 42 x 14), `PG_TOP` (rows kept free under the header line), `PG_LINES_MAX` (900). They are not `SH4DER_*` tuning numbers, so they have no tuning rows in the README.

## Cost

| Item | Cost | Status |
|---|---|---|
| Frame | none: drawn once a page, after the PVR is shut down | by construction |
| RAM, glyphs | 12 KB (96 x 16 x 8 bytes) | FACT (arithmetic in the code) |
| RAM, text lines | 58 KB (900 lines x 65 bytes) | FACT (arithmetic in the code) |
| RAM, graphs | 16 records of about 2.5 KB | FACT by the layout; not measured on the console |
| VRAM | the frame buffer only | FACT |
| Reliability of the numbers | the ledger's own values, one run on one console (VID-015: the CRC line works; VID-016: 21 text pages and 4 graphs agree) | MEASURED (one run each, one console) |

## Recommended limits

- Put every number the ledger is written from on a text page. A graph's values are a shape, not a record.
- Do not draw a live view with it: it writes into the frame buffer after the PVR is shut down.
- Keep `PG_TOP` at 1 when the effect label line takes two rows (see [fxlabel](fxlabel.md)).

## Pitfalls

- A trailing space at a cut is invisible in a photograph. The console keeps one where the text had two spaces; `tools/page_crc.py` tries that space, and says which line needed it.
- The host test stands in for the BIOS font with boxes and digit shapes. It checks the layout and the arithmetic, not how the real font photographs (`test/page_test.c` says so).
- `PG_PREFIX` is needed before the include, or the serial lines carry `pg:`.

## Open

- The RAM of the graphs is by the layout, not measured on the console.
- The names are still the demos' (`pg_*`, `PG_*`). A rename to `sh4der_page_*` would touch every includer; `demo/psm/psm_page.h` is a forward for the old path.

## Related

- [fxlabel](fxlabel.md) (the label line on a page), [qrpage](qrpage.md) (the same text as QR codes), [ledger: video](../hardware/ledger-video.md) (VID-014 to VID-016).
- `demo/psm/` (the programs that print pages): their own pages and results are in their own folders.
