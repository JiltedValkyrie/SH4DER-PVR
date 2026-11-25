# QR reading test (image psm-qr-q1): built, not run

Built 2026-11-25 by Senior. **Not run on a console.** Nothing below is a hardware result.

## The question

A result page is photographed off a television and typed by hand. A QR code on the page would carry the same bytes with no typing. This image tests one thing: **does a phone read a QR code off the console's picture, and down to which module size?**

## The image

| Item | Value |
|---|---|
| Image | `demo/psm/out/cdi/psm-qr-q1.cdi` |
| sha256 | 80904c55173c80ea9c828b6973961678c636ca3d3e8da1e936f2f8469eb0380f |
| Size | 1,715,593 bytes |
| Build | `python demo/psm/qr/build.py --tag q1` (log `D:\tmp\psmbuild\logs\qr_q1_20261125_095443.log`, no compiler warning) |
| Source | `qr/qr_page.h` (the set and the drawing), `qr/qr_spike.c` (the program), `qr/test/qr_test.c` (host test), `tools/qr_check.py` (decoder and checker) |

- The PVR is not started. The SH-4 draws into the frame buffer.
- The fault catcher is in. The only wait is the 20 ms between two reads of the pad.
- Nothing is written but the frame buffer.

## The encoder is not ours

- The codes are made by the **QR Code generator library (C)** by **Project Nayuki**, **MIT License**. I read the licence text at the top of `qrcodegen.c`: it allows use, copying and distribution, and asks that the copyright and permission notice stay with copies.
- It is used as a library through three public calls. No line of it is copied into our files.
- Its two files are not in the repository. `tools/fetch.py` downloads them by `fetch_table.json` (commit 9a217d44, sha256 pinned) into `assets/_dl`, which Git ignores.
- The credit is in `CREDITS.md`, in `qr_page.h`, in `qr_spike.c`, and on the first text page of the image.
- The disc image holds the compiled code of the encoder. Anyone who passes the image on keeps the credit with it.

## The set: 16 codes on 7 pages

Every code is error correction level M, byte mode, filled to capacity: **version 3 = 42 bytes, version 5 = 84 bytes**. Lead gave these as REPORTED. The host test now backs them: the encoder accepted exactly these lengths at the fixed version and level.

| Page | Version | Pixels a module | Codes | Side of a code on screen (with quiet zone) |
|---|---|---|---|---|
| 3 | 3 | 10 | 1 | 370 px |
| 4 | 3 | 8 | 1 | 296 px |
| 5 | 5 | 8 | 1 | 360 px |
| 6 | 3 | 6 | 2 | 222 px |
| 7 | 5 | 6 | 1 | 270 px |
| 8 | 3 | 4 | 6 | 148 px |
| 9 | 5 | 4 | 4 | 180 px |

- Pages 1 and 2 are text: what the test is, and every payload as text. That is the fallback, with the CRC line of the page kit.
- The page numbers above assume two text pages (ESTIMATE from the line count; the header shows the real total).
- **The default, if the test passes at it: 8 px modules, version 3 or 5** (pages 4 and 5).
- One version 5 code at 8 px fills the height of the page. Four of them fit at 4 px, or six version 3 codes. That is why 4 px is in the set.
- Each code is dark on a light panel with the 4-module quiet zone the standard asks for.
- Everything is inside the title-safe area.
- **Y** changes the level of the panel: 232, 255, 176 of 255. A full white panel may bloom on a tube.
- Version 5 at 10 px does not fit the title-safe height (450 px against 384), so it is not in the set.

**The payload.** Text a phone shows as it is:

```
q1 <id> <crc>|<data>
```

- `id` is version-pixels-letter: `3-08-a` is version 3, 8 px, the first such code.
- `data` is 23 or 65 letters and digits from a fixed generator seeded by the id.
- `crc` is the CRC-32 of `data`, as 8 hex digits. A decoded string checks itself.

## Host tests

They test the encoder and the drawing. They do not test the television or the phone.

| Check | Result |
|---|---|
| `qr_test.c`: payload lengths, prefix and CRC; the encoder takes all 16 at the fixed version | pass |
| `qr_test.c`: on each drawn page, every pixel of every code matches the module of the encoder; the quiet zone is clear; nothing is outside title-safe; at all three panel levels | pass |
| `qr_check.py --matrices`: our own decoder, written from the layout in the standard and sharing no code with the encoder (format bits, mask, zigzag, blocks, Reed-Solomon syndromes, byte segment) | 16 of 16 decode to their payload |
| `qr_check.py --photos` on the PNG pages of the host test, with the QR detector of OpenCV (a third decoder, in a throwaway Python environment outside the repository) | 15 of 16 |

- OpenCV misses `3-04-a`, one of the six on page 8, on a perfect picture.
- Our decoder reads the modules of that code correctly, so the code is right.
- The miss is a limit of that detector with many codes in one picture. Take it as a warning for the 6-code page, not as a fault in the image.

## What the owner does

1. Boot `psm-qr-q1.cdi`. A and B turn the page.
2. On each of the 7 code pages, point the camera of the phone at the screen, as for any QR code. Note which codes it reads and how fast: at once, after a few seconds, or never.
3. If a code does not read, press Y to change the panel level and try again. Note the level that worked.
4. Photograph each code page once, with the whole screen in the picture, as for a result page.
5. Send the photos and the notes. Name the television and the cable (composite, S-Video, VGA). If the phone can copy the decoded text, paste the strings into a file, one a line.

One photo a page is enough: 7 photos, plus the 2 text pages if wanted.

## What a reader session does with it

- Strings from the phone: `python demo/psm/tools/qr_check.py read.txt` prints which of the 16 codes were read right, by version and module size.
- Photos: `qr_check.py --photos <folder>` does the same with OpenCV (it needs the `cv2` module). A code OpenCV misses in a photo may still read on a phone, so the ledger says which reader gave each result.

## What the result decides

| Result | Then |
|---|---|
| 8 px reads, version 3 and 5 | Result pages can carry a QR page: one version 5 code a page, 84 bytes. |
| 6 px or 4 px also reads | 2 to 6 codes a page: up to 336 bytes a page (4 codes of 84) at 4 px. |
| Only 10 px reads | One 42-byte code a page. Not worth it; the text page with its CRC stays the only route. |
| Nothing reads | QR is dropped. Text pages with the CRC stay. |

Even the best case is small: a text page holds up to 1,408 characters (64 by 22). A QR page is a help for a few key numbers, not a replacement for the text pages.

## Open

- Does a phone read the codes off the television of the owner at all? OPEN; this image is the test.
- Does interlace or the cable change the smallest size that reads? OPEN; the notes from the run should name both.
- Not built: codes on c4 or r3 result pages. That waits for this result.

## Hardware result (suite s1, first run, qr-q2)

Run by the owner on 2026-11-25: one console, one television, one phone. qr-q2 is qr-q1 with the suite's three lines. The rows with photo names are in `compare/SUITE_RUN1_RESULTS_senior.md`, section 7. Photos: `logs/benchmarks/suite_run1`, files 42 to 63.

| Claim | Status |
|---|---|
| The encoder refused 0 of 16 codes on the console | MEASURED |
| All 16 codes decode from the photos at panel 232, each to its exact payload: 10 px 1 of 1, 8 px 2 of 2, 6 px 3 of 3, 4 px 10 of 10 | MEASURED, one photo a page |
| At panel 255 and 176 the 8 px and 6 px pages decode too, every code | MEASURED |
| The six-code page at 4 px is not reliable: 6, 5 and 3 of 6 in three photos | MEASURED |
| One pass of OpenCV's detector at the photo's own size reads 7 of 16; with smaller copies and a threshold, 16 of 16 | MEASURED on the host, same photos |
| Whether the phone's own camera reads a code live | OPEN: the owner's notes are not in the folder |

**The reader was OpenCV on the phone's photo**, not the phone's camera app. `tools/qr_check.py --photos` now tries each photo at four sizes, three blurs, with and without a threshold.

**Decision this supports.**
- A result page can carry a QR code, read from the photograph that is taken anyway.
- Default: 8 px modules. 6 px is safe on this evidence (11 readings of 11). 4 px only with a second photo.
- Panel 232 stays.
- The first table of "What the result decides" above is settled at its second row: 8 px reads, and 6 px and 4 px also read.

**Next (not built).** Put a code on a real result page: the key numbers of the page as text in one version 5 code at 6 px beside the text, with the page's CRC inside it.
