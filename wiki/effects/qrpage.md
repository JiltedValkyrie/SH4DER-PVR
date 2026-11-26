---
title: "qrpage (QR result pages, the test set)"
tags: [effect, qr, page, photograph, test]
updated: 2026-11-26
sources: [include/sh4der_qrpage.h, test/qrpage_test.c, tools/qr_check.py, demo/psm/qr/build.py, demo/psm/fetch_table.json, wiki/hardware/ledger-video.md (VID-014)]
---
# qrpage: QR result pages (the test set)

**What you will learn:** what the QR test set on a result page is for, which payloads it carries, how the encoder is obtained (it is not in this repository), and what was measured.
**For:** a programmer who wants a result to reach a phone as a code instead of being typed from a photograph.

> **In plain words.** A test page is photographed off a television and typed by hand. A QR code on the same page would carry the same bytes with no typing. Whether a phone reads a code off this picture, and at which size, was not known, so this set draws 16 codes of known payloads at several module sizes. The reader's check (`tools/qr_check.py`) says which codes it read, and whether each one is exactly right.

## Try this first

What it is for: a test set of codes, to find the size a phone or a reader can read off a television.

1. Get the encoder: `python demo/psm/tools/fetch.py` downloads Project Nayuki's `qrcodegen.c` and `qrcodegen.h` into `demo/psm/assets/_dl` (sha256 checked).
2. Include the encoder first, then `sh4der_page.h`, then `sh4der_qrpage.h`.
3. Draw a set: `qr_draw(&qr_sets[k], page, pages, panel_level)` (panel 232 of 255 at the start). It returns 0 if the encoder refuses a code.
4. Test on the host: `make test-demo` runs `test/qrpage_test.c` (it writes the modules of every code) and `tools/qr_check.py --matrices` decodes them. Without the encoder the test says NOT RUN.

Header: [include/sh4der_qrpage.h](../../include/sh4der_qrpage.h). Call order: [below](#call-order).

## What it does

The set has seven pages: version 3 at 10, 8, 6 and 4 pixels a module, version 5 at 8, 6 and 4 pixels a module, 16 codes in all. Each code is error correction level M, byte mode, filled to its version's capacity (42 bytes for version 3, 84 for version 5). Its payload is `q1 <id> <crc>|<data>`: the id is `version-pixels-letter`, the data is 23 or 65 characters from a generator seeded by the id, and the crc is the CRC-32 of the data. So a decoded string checks itself.

Each code sits on a light panel with a 4-module quiet zone, all inside the title-safe area. A page with room at its right has its text there; the others have a header line and the ids below.

## The encoder

The codes are made by Project Nayuki's QR Code generator library (C), MIT License, https://www.nayuki.io/page/qr-code-generator-library. This header calls only `qrcodegen_encodeBinary`, `qrcodegen_getSize` and `qrcodegen_getModule`, and does not copy the library. The two files are fetched by `demo/psm/tools/fetch.py` from the URLs in `demo/psm/fetch_table.json`; the README's "Credits and license" has the credit, and the notice stays in the files.

## Cost

| Item | Cost | Status |
|---|---|---|
| Frame | one page drawn once; the encoder runs once a code | by construction |
| Encoder time on the console | not measured | NOT MEASURED |
| RAM | the code buffer: QR_BUF bytes (version 5); the panel and modules are drawn into the frame buffer | FACT (arithmetic in the code) |
| The codes read back | 16 of 16 from the photographs; each to its exact payload with its CRC right | MEASURED (VID-014, one console, one television, one phone, one run; OpenCV's detector read them) |

## Call order

1. `qr_sets` is the table of the seven sets. Pick one.
2. `qr_draw(set, page, pages, panel)` for each page; a page with `qr_side_text` set has the text at its right.
3. `qr_fill(set, k, payload)` gives code k's payload string; `qr_encode(set, payload, code)` makes its modules.

## Recommended limits

- Send a code as its own payload string; a code repeats the text on the page, it does not replace it.
- Do not use a code for a number that is not already on a text page (see [page](page.md)).
- The panel level is the caller's: 232 at the start; 255 may bloom on a tube.

## Pitfalls

- The header refuses to compile without `sh4der_page.h` first (`#error`), and it needs `qrcodegen.h` before itself.
- A phone's reading of a television photograph is not the same as the reader's: VID-014's reader was OpenCV's detector.

## Tests and tools

- `test/qrpage_test.c` (in `make test-demo`, NOT RUN without the encoder): every payload has its version's length, prefix and CRC; the encoder takes each at its fixed version; a drawn page has every module as the encoder says, a clear quiet zone, and nothing outside the title-safe area.
- `tools/qr_check.py`: `--self-test` (in `make test`), `--matrices <dump>` (a decoder written from the standard's layout that shares no code with the encoder), and a list of strings a phone read.
- `demo/psm/qr/build.py`: the image's build (host test, then the ELF, then the disc).

## Open

- The phone reading at each size has one run of one phone; more photographs are the next evidence.

## Related

- [page](page.md) (the text pages the codes sit beside), [ledger: video](../hardware/ledger-video.md) (VID-014).
