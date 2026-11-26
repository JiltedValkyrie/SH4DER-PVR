---
title: "fxlabel (the effect label line)"
tags: [effect, label, page, live view, credits]
updated: 2026-11-26
sources: [include/sh4der_fxlabel.h, test/fxlabel_test.c, demo/psm/test/fx_tables_test.c, demo/psm/ao/ao_fx.h, demo/psm/hero/hero_fx.h, demo/pl4nar/src/pl4nar_fx.h, README.md "The effect labels", wiki/hardware/ledger-video.md (VID-016)]
---
# fxlabel: the effect label line

**What you will learn:** how a demo screen says, in one small line, which effects are in use and how each is done, the rules that keep such a label true, and how the tables stay in the program while the rules are the library's.
**For:** a programmer whose demo or test screen should say honestly what it draws.

> **In plain words.** Each screen says, in small text, things like "Ambient Occlusion  SH-4  Baked". The words are the field's usual names, and each entry says who does the work (the SH-4, the PVR, or both) and when it was worked out (baked before the frame, a texture, or every frame). This header turns a table of such entries into two lines of text that fit the screen, and it refuses to shorten a name to make a table fit.

## Try this first

What it is for: a label line that is true, on a result page and on a live view.

1. Write one table of `fx_entry_t` per mode the program has: name, who (`FX_SH4`, `FX_PVR`, `FX_BOTH`), when (`FX_BAKED`, `FX_TEXTURE`, `FX_REALTIME`), and an optional detail.
2. For a live view: `fx_format(&t, table, n)` (or `fx_format_split` for a cut screen), then draw `t.line[0]` and `t.line[1]` with your own text path. A Realtime entry is marked `Realtime<<`, and `t.span` tells you where, for a colour.
3. For a result page: after `pg_draw`, call `fx_page(table, n)`. It draws the label line in the row under the title (with `PG_TOP 1` before the include, a second line too).

Header: [include/sh4der_fxlabel.h](../../include/sh4der_fxlabel.h). Call order: [below](#call-order).

## The rules (from the header)

- **name**: the field's usual name, not a module's name. "Shadow Mapping" only where a depth is compared against a map. Casters drawn into a render target and laid on the ground with no compare are a "Projected Shadow".
- **Bloom** only for a bright map taken from the finished frame. A glow mesh round a light is a "Soft Halo" or "Glow".
- **who**: `FX_SH4`, `FX_PVR`, or `FX_BOTH`. The PVR counts only when it does work that belongs to the effect (a render target pass, a texture that carries the effect, an alpha test). Drawing what the SH-4 computed (a vertex colour, an offset colour) is the SH-4's effect.
- **when**: `FX_BAKED` (before the frame loop, kept in a vertex), `FX_TEXTURE` (before the loop, kept in a texture), `FX_REALTIME` (every frame, marked `<<`).
- **FX_NONE** as who prints the name alone.

## Cost

| Item | Cost | Status |
|---|---|---|
| Run time | none: a table is formatted when the page or the mode changes | by construction |
| RAM | about 0.3 KB: a text of 3 x 65 bytes, 16 spans, the item list | FACT (arithmetic in the code) |
| VRAM | none; the program draws the text | FACT |
| Labels on the console | drawn on every page of eight programs on three discs, off in every measured stage, clear of the graph titles | MEASURED (VID-016, one console) |

## Call order

1. Each mode: a table of `fx_entry_t`.
2. Live view: `fx_format` / `fx_format_split` / `fx_format_merged` into an `fx_text_t`; draw the lines; check `t.fits` (0 means not everything fitted: change the table).
3. Result page: `fx_format_page` (or `_n` for a page of more lines), or `fx_page` after `pg_draw`.
4. Hiding: the program's own chord, through `fx_hide_chord(held, hit, &hidden)`. Result pages always show the line.

## Recommended limits

- A table that does not fit says so; shorten the table, never the names. `FX_MAX` is 16 entries, `FX_COLS` 64 characters, two lines in a live view and three on a page.
- The label is OFF during measured autorun stages: a line of 512 x 16 blended pixels is fill and would move the numbers.

## Pitfalls

- A label that names an effect the program does not draw is a lie the header forbids; check each name against the code it names.
- `fx_page` needs `sh4der_page.h` included first (it draws with `pg_text`).
- Two entries with the same name, who and when are merged into one entry with the side (L, R, L+R) joined; a detail is left out of a page.

## Tests

- `test/fxlabel_test.c` (in `make test`): the text's form, the merge by name, the line break, the Realtime spans, the chord.
- `demo/psm/test/fx_tables_test.c` (in `make test-demo`): every program's table, for every mode it has. A table that does not fit two lines FAILS at build time.

## Open

- The live view's look on a console is judged by the programs' own runs, not by the test.

## Related

- [page](page.md) (the result page the label line sits on), [ledger: video](../hardware/ledger-video.md) (VID-016).
- The tables stay in the programs: `demo/psm/ao/ao_fx.h`, `demo/psm/hero/hero_fx.h`, `demo/pl4nar/src/pl4nar_fx.h`.
