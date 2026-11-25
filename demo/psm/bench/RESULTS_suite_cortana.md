Photos covered: files 32 to 47 of 63 by name, PXL_20261125_152223847 to PXL_20261125_152928489 (16 files). For the comparison only, I also read a3e-auto p1 and p2 (files 28 and 29, 152129527 and 152131266). Those two are another reader's files.

# Suite s1, run 1: results from files 32 to 47 (cortana)

- **Disc:** `psm-suite-s1` (`demo/psm/suite/NOTES.md`).
- **Run:** by the owner, 2026-11-25, on one console.
- **Photos:** `I:\マイドライブ\dcdev\sh4der-pvr\logs\benchmarks\suite_run1\`.
- **Status:** every number below is MEASURED 2026-11-25, one console, unless it is marked otherwise.

## Every typed page was checked against its CRC

- Each text page's header ends with "CRC xxxxxxxx", the CRC-32 of the page's lines (`psm_page.h`).
- I rebuilt every text page from my typed numbers, using the program's own format strings (`patches/auto_suite.diff`) and the same wrapping as `pg_say`. Then I computed the CRC.
- **All 8 text pages agree with their headers**: a3e p1 and p2, a3f p1 and p2, bake1 p1 and p2, QR p1 and p2. So every number transcribed below is exact. No digit was unreadable.
- **The two bar-graph pages agree too** (a3f p4 09164cf9, bake1 p4 f8cb023f). Their CRC is built from the per-segment means rounded to whole numbers.
- **The line graphs (p3) carry 64 values each.** Those values cannot be read off the photo, so they are not checked.
- Scripts: `C:\Users\jilte\AppData\Local\Temp\claude\d--Workspace-Claude-dc-homebrew-sh4der-pvr\83d578c6-7e2d-4a68-ae93-423c82ef7f04\scratchpad\suite_crc_raw.py`, `...\g4_crc.py` and `...\qr_crc.py`.

**Tool bug found on the way.** `tools/page_crc.py` drops trailing spaces, but the console counts them (`pg_page_crc` uses `strlen` of the stored line).
- When `pg_say` cuts a line at a double space, one trailing space stays on the line. Examples: "SHED<=0 " before "text 92v", and "in-ovf 0 " before "OVERFLOW".
- So every TOWN AUTO page, typed exactly, gives "DOES NOT AGREE" with the tool as it is. With the trailing spaces kept, all six agree.
- Fix one side or the other: the tool keeps trailing spaces, or `pg_say` drops the space it cuts at. Not changed by me.

## Photos and what each one is

| # | Photo (PXL_20261125_…) | Test | Page |
|---|---|---|---|
| 32 | 152223847 | a3f-auto (by time and numbers, see L1) | live town frame, route B, F000561 |
| 33 | 152324582 | a3f-auto | TOWN AUTO p1/4, CRC 67c663a7 |
| 34 | 152325850 | a3f-auto | p2/4, CRC 81fdda4a |
| 35 | 152328132 | a3f-auto | p3/4 GRAPH (frame time, largest ms a group), CRC db542f7b |
| 36 | 152330612 | a3f-auto | p4/4 GRAPH (mean per segment), CRC 09164cf9 |
| 37 | 152530803 | bake1-auto (by time and numbers, see L2) | live town frame, route A, F000341 |
| 38 | 152643829 | bake1-auto | TOWN AUTO p1/4, CRC 62ab96ca |
| 39 | 152645072 | bake1-auto | p2/4, CRC 433c0b59 |
| 40 | 152647110 | bake1-auto | p3/4 GRAPH, CRC 2557d6a4 |
| 41 | 152649311 | bake1-auto | p4/4 GRAPH, CRC f8cb023f |
| 42 | 152918980 | qr-q2 | QR q1 p1/9 (text), CRC 547c7678 |
| 43 | 152920667 | qr-q2 | QR q1 p2/9 (text), CRC 71307c17 |
| 44 | 152922718 | qr-q2 | p3/9: V3-M 10 px x1, code 3-10-a 5aedd6f3, panel 232 of 255 |
| 45 | 152924332 | qr-q2 | p4/9: V3-M 8 px x1, 3-08-a fe6fd86e, panel 232 |
| 46 | 152926808 | qr-q2 | p5/9: V5-M 8 px x1 (84 bytes a code), 5-08-a 3a421d5f, panel 232 |
| 47 | 152928489 | qr-q2 | p6/9: V3-M 6 px x2, 3-06-a 480c7121 and 3-06-b 5a530b64, panel 232 |

- **No hud1-auto page is in my range.** bake1's last page is at 15:26:49 and the QR test at 15:29:18. Whether hud1 ran, and where its pages are, is for the reader of files 48 to 63 (or the owner) to say.
- **All four town pages have "ABXY menu" in the header.** By NOTES.md, that means the suite's way back to the menu loaded (the test read `/cd/MENU.BIN`).

## The town pages, typed (min/mean/max ms unless said)

- Each run: 1,200 frames counted, sim step 1/30 s, vertex buffer 1,024 KB × 2, one console.
- SH-4 = the sum of the four parts. Route A's parts are build, clip (the map pass), uv, 0. Route B's are build, clear, casters, receivers.
- PVR = the last done render.
- Every row reads KOS max 1024K, SHED<=0 and text 92v. The events ISP, strip, in-err and in-ovf are 0 in every row.

### a3f-auto (files 33, 34)

| Segment | frame | SH-4 | parts | PVR | TAv | used KB | est KB | PARAM | OPB |
|---|---|---|---|---|---|---|---|---|---|
| A still (150 fr) | 54.0/56.1/84.9 | 12.3/13.2/28.2 | 2.5 6.8 4.0 0.0 | 3.3/3.8/12.1 | 30541/30613/30624 | 65/762/1024 | 754/754 | 150 | 150 |
| A turn (300 fr) | 54.1/71.6/102.1 | 12.5/29.4/42.9 | 19.1 6.4 4.0 0.0 | 3.3/4.0/14.5 | 24890/29057/30640 | 52/636/1024 | 724/754 | 56 | 300 |
| B still (150 fr) | 76.3/77.9/94.3 | 45.1/45.5/63.2 | 2.6 0.7 16.0 26.2 | 8.6/8.8/14.3 | 18824/18832/18832 | 601/604/890 | 477/477 | 0 | 75 |
| B turn (300 fr) | 73.8/90.3/111.5 | 45.3/58.7/65.5 | 19.1 0.7 14.4 24.5 | 7.9/8.3/10.6 | 13855/17466/18852 | 456/540/603 | 449/477 | 0 | 150 |
| B x60 (300 fr) | 75.8/77.9/113.7 | 44.7/45.3/62.8 | 2.5 0.7 16.1 26.0 | 8.6/8.7/11.2 | 18752/18779/18864 | 600/601/602 | 476/478 | 0 | 150 |

Every segment ends "OVERFLOW", then "END. OVERFLOW: the TA ran out of vertex buffer or bins."

### bake1-auto (files 38, 39)

| Segment | frame | SH-4 | parts | PVR | TAv | used KB | est KB | PARAM | OPB | bakes |
|---|---|---|---|---|---|---|---|---|---|---|
| A still (150 fr) | 56.3/58.4/85.5 | 13.3/14.1/27.6 | 3.0 7.1 4.0 0.0 | 3.3/3.8/14.6 | 30541/30613/30624 | 65/666/1024 | 754/754 | 150 | 150 | — |
| A turn (300 fr) | 56.5/76.1/108.9 | 13.4/32.2/46.0 | 21.5 6.8 4.0 0.0 | 3.3/4.1/14.3 | 24890/29057/30640 | 52/542/1024 | 724/754 | 56 | 300 | — |
| B still (150 fr) | 67.7/70.5/254.9 | 35.1/35.4/55.0 | 3.2 0.7 2.1 29.5 | 8.6/8.8/11.3 | 18824/18832/18832 | 601/602/603 | 477/477 | 0 | 75 | 1, longest 151,984 µs |
| B turn (300 fr) | 70.9/84.9/105.7 | 35.2/52.1/56.7 | 21.5 0.7 2.0 27.9 | 7.9/8.3/11.3 | 13855/17466/18852 | 456/540/602 | 449/477 | 0 | 150 | — |
| B x60 (300 fr) | 67.4/73.6/225.5 | 34.8/35.4/55.0 | 3.1 0.7 2.5 29.1 | 8.0/8.8/11.3 | 18752/18779/18864 | 600/601/602 | 476/478 | 0 | 150 | 9, longest 151,965 µs |

Every segment ends "OVERFLOW".

### a3e-auto (files 28, 29: another reader's; typed here only for the comparison; both CRCs agree)

| Segment | frame | SH-4 | parts | PVR | used KB |
|---|---|---|---|---|---|
| A still | 54.1/56.1/83.7 | 12.2/13.0/26.6 | 2.8 6.2 4.0 0.0 | 3.3/3.8/12.1 | 65/768/1024 |
| A turn | 54.2/74.2/104.3 | 12.3/31.7/47.3 | 21.8 5.9 4.0 0.0 | 3.3/4.0/14.3 | 52/659/1024 |
| B still | 74.2/75.9/94.6 | 42.7/43.1/63.2 | 2.9 0.7 15.3 24.1 | 8.6/8.8/14.3 | 601/604/890 |
| B turn | 75.1/90.8/115.6 | 42.9/59.0/65.7 | 21.8 0.7 13.7 22.8 | 7.9/8.3/10.6 | 456/540/603 |
| B x60 | 73.6/75.6/101.7 | 42.2/42.9/62.7 | 2.8 0.7 15.4 24.0 | 8.1/8.7/11.3 | 600/601/602 |

TAv, est KB and the events are the same as a3f's in every row.

### Graphs

- **a3f p3** (file 35): the largest frame ms of each of 64 groups, scale 0 to 200. The line runs about 80 at first, then 90 to 100, rising to about 105 to 110 near the middle, with one point near 115. It ends at about 90. Shape only, not checkable.
- **a3f p4** (file 36): bars of the segment means. They agree with p1 and p2 by CRC.
- **bake1 p3** (file 40): scale 0 to 500. The line runs about 80 to 100, with one spike to about 250 in the first half. The last quarter alternates between about 70 and about 200 to 215, with 7 peaks visible. Shape only.
- **bake1 p4** (file 41): bars. They agree by CRC.

## Live frames (HUD as shown)

**L1, file 32.**

```
ROUTE B  SH-4 DEPTH   MAP UNIFORM  TOWN
F000561 TA est  477K used  602K max 1024K 100% of 1024K SHED0
TEXEL/PX 4m 29.1 10m 7.5 25m 1.9 60m 0.5 SLAB 0-13.0
MAP 256x256 128 KB RAM (16-bit depth) text 256 KB VRAM
WARP -- m UNIFORM self-shadow yes CASTERS ALL
14:00 x1 SUN 214/27 CAM x 0.0 z -14.0 hdg 0 pitch -4
BIAS a 20 mm b 0.0 FILTER A BILIN B 2x2 TRIS 1101/1162 cull 1
SH-4 45.4 ms PVR 8.8 ms TA 18832 v VRAM 0 KB FPS 13
B: build 2.5 clear 0.7 casters 16.0 recv 26.3 ms tris 1101/1162
```

- It was taken at 15:22:23, between a3e's pages (15:21:38) and a3f's (15:23:24).
- Frame 561 falls in B still: 60 warm-up frames + 150 + 300 = 510.
- Its B parts are a3f's B still means within 0.1 (2.6 0.7 16.0 26.2), and its TA count is 18832.
- So it is a3f-auto.

**L2, file 37.**

```
ROUTE A  PVR DRAWS THE MAP   MAP UNIFORM  TOWN
F000341 TA est  749K used  985K max 1024K 100% of 1024K SHED0
TEXEL/PX 4m 24.6 10m 6.4 25m 1.6 60m 0.4 SLAB 0-0.5
MAP 256x256 128 KB VRAM (render target) text 256 KB VRAM
WARP -- m UNIFORM self-shadow no CASTERS ALL
14:00 x1 SUN 214/27 CAM x 0.0 z -14.0 hdg 39 pitch -4
LOOKUP per pixel FILTER A BILIN B 2x2 TRIS 1106/1162 cull 1
SH-4 33.5 ms PVR 17.0 ms TA 30234 v VRAM 128 KB FPS 13
A: build 22.2 clip 7.3 uv 4.0 ms map tris 1110 layer v 8318
```

- It was taken at 15:25:30, between a3f's pages and bake1's, so it is bake1-auto.
- Frame 341 falls in A turn (frames 210 to 509), and the HUD shows hdg 39.
- **Odd:** the HUD shows PVR 17.0 ms, but the page's PVR max for A turn is 14.3 (bake1) and A still's is 14.6. Either the HUD and the page time different things, or the HUD value comes from outside the counted frames. OPEN.

## Comparison: a3e against a3f against bake1 (means, ms; DERIVED from the MEASURED rows)

SH-4 per segment:

| Segment | a3e | a3f | a3f − a3e | bake1 | bake1 − a3e |
|---|---|---|---|---|---|
| A still | 13.0 | 13.2 | +0.2 | 14.1 | +1.1 |
| A turn | 31.7 | 29.4 | **−2.3** | 32.2 | +0.5 |
| B still | 43.1 | 45.5 | **+2.4** | 35.4 | **−7.7** |
| B turn | 59.0 | 58.7 | −0.3 | 52.1 | **−6.9** |
| B x60 | 42.9 | 45.3 | **+2.4** | 35.4 | **−7.5** |

Parts against a3e (build or setup / clip or map / casters / receivers):

| Segment | a3f − a3e | bake1 − a3e |
|---|---|---|
| A still | −0.3 / +0.6 / 0 / 0 | +0.2 / +0.9 / 0 / 0 |
| A turn | **−2.7** / +0.5 / 0 / 0 | −0.3 / +0.9 / 0 / 0 |
| B still | −0.3 / 0 / +0.7 / **+2.1** | +0.3 / 0 / **−13.2** / **+5.4** |
| B turn | **−2.7** / 0 / +0.7 / **+1.7** | −0.3 / 0 / **−11.7** / **+5.1** |
| B x60 | −0.3 / 0 / +0.7 / **+2.0** | +0.3 / 0 / **−12.9** / **+5.1** |

Frame mean (a3e / a3f / bake1):

| Segment | a3e | a3f | bake1 |
|---|---|---|---|
| A still | 56.1 | 56.1 | 58.4 |
| A turn | 74.2 | 71.6 | 76.1 |
| B still | 75.9 | 77.9 | 70.5 |
| B turn | 90.8 | 90.3 | 84.9 |
| B x60 | 75.6 | 77.9 | 73.6 |

PVR means are the same in all three, within 0.1 ms.

### a3f (the pads) against the expectation

- **Expectation:** TOWN-030 (ESTIMATE) said the pads save about 1.1 ms on frames that re-place the models. TOWN-021 and TOWN-022 said route B reads `mv[i]` and `mn[i]` together, and the pad between them should help there too.
- **Measured, placing:** the build/setup part on turning frames drops by **2.7 ms on both routes** (21.8 to 19.1). That is more than twice TOWN-030's 1.1 ms.
- **Measured, route B:** the casters part rises by 0.7 ms and the receivers part by 1.7 to 2.1 ms, in every route B segment, the still ones included. **This contradicts the expectation that the mv/mn pad helps route B.** The rise is in the minimum too (B still SH-4 min 42.7 to 45.1), so it is not one slow frame.
- **Net:** route A turning −2.3 ms; route B still and x60 +2.4 ms; route B turning −0.3 ms.
- **Possible cause (ESTIMATE, not tested):** moving the arrays by 8 KB put some other pair into one cache entry. One candidate is the receivers' arrays against the depth map or the ground arrays.
- **What the next test should check:** the array offsets in the a3f ELF (as TOWN-021 did for a3e), then a pad only between `mclip` and `mscr`, the one that helps.
- **Caveat:** one run of each. The script is fixed and the TA counts are identical, so the work is the same. Run-to-run variance in a suite run is not known.

### bake1 against the expectation

- **The bake's cost.** It is **151,984 µs** for the one boot bake (B still) and longest **151,965 µs** over 9 bakes (B x60). TOWN-087 (ESTIMATE) gave about 28 ms at RT's prepared cost and 53 ms at its any-direction cost, and said bake1 is "likely above 53 ms". **Measured: 2.9 times 53 ms.**
  - Per test, using the host's test count of 169,607 (TOWN-081): 0.896 µs, which is about **179 cycles a test**. This is DERIVED, and assumes the console made the host's test count. HUD line 14, which prints the console's count, is not in my photos.
  - TOWN-063's 50 cycles is 3.6 times too low. RT measured 62.49 cycles (any direction, early outs, data in the cache) and 115.21 (its reference). bake1 has a divide, and the town's data is not in the cache.
- **The bake count at x60.** 9 bakes in 300 frames, which is 10 s at x60 = 10 game minutes. TOWN-080 said about one a game minute, so about 10. **Holds.**
- **The hitches.** The frame max is 254.9 ms in B still and 225.5 ms in x60. That is a normal frame of about 70 ms plus the 152 ms bake. The p3 graph shows the same: one spike of about 250 ms, and 7 visible peaks of 200 to 215 ms at x60. Two bakes in one group would show as one peak.
- **Route B casters.** They drop by 11.7 to 13.2 ms (15.3 to 2.1 in B still), because the map takes the movers only. Expected (TOWN-080).
- **Route B receivers.** They rise by **5.1 to 5.4 ms** (24.1 to 29.5 in B still). **This contradicts TOWN-082 (host ESTIMATE): "the apply, under 0.001 ms".**
  - The apply (a minimum for static vertices; the ground and the movers multiplied by the baked map, read bilinear) costs about 5 ms on the console.
  - Part of it may be layout, as with a3f: bake1's .bss is 133 KB larger (TOWN-084). The measured rows cannot separate the two.
  - Net: route B still −7.7 ms SH-4.
- **Route A.** The patch does not touch it (TOWN-080), yet the clip part rises by 0.9 ms in both A segments. a3f's clip rose 0.5 to 0.6 ms as well. **The map pass is sensitive to layout** (ESTIMATE). Route A's numbers are not a clean control between these binaries.
- **Frame mean.** B x60 is 73.6 against a3e's 75.6. The 9 bakes add about 4.6 ms a frame over 300 frames, and the 7.5 ms SH-4 saving covers them.

## Used KB and OVERFLOW events

| Claim | Status | Photos | Expected |
|---|---|---|---|
| Every segment of all three binaries ends "OVERFLOW". The event counts are identical across a3e, a3f and bake1. | MEASURED | 28, 29, 33, 34, 38, 39 | TOWN-094 (ESTIMATE): the buffer or the bins run out at the busy view. **Supported** |
| Route A: PARAM on 150 of 150 frames (A still) and 56 of 300 (A turn); OPB on every frame. used KB max 1024 (the buffer full) and KOS max 1024K, while the estimate says 754 KB (74 %). | MEASURED | as above | VID-011: KOS's largest frame 100 % against our 68 to 72 %. **Repeats.** A still is the boot pose, the TOWN-001 start view where the HUD is lost. The text is sent last (TOWN-092). This fits TOWN-094 |
| Route B: PARAM 0. OPB on exactly half the frames (75 of 150; 150 of 300). used KB mean 540 to 604, max 890. So route B's overflow is the **bin pool (OPB), not the vertex buffer**. | MEASURED (the cause is ESTIMATE) | as above | Q66 candidate (2), `opb_overflow_count` 4. Why exactly every second frame is OPEN (two buffers, one overflowing?) |
| Route A used KB means differ between binaries with identical TA counts: A still 768 / 762 / 666, A turn 659 / 636 / 542 (a3e / a3f / bake1). The route B rows match within 2 KB. On an overflowing frame, the used reading is where the TA stopped, and that may depend on timing. | MEASURED (the cause is OPEN) | as above | none |
| est KB stays under used KB on route B as well (477 against 601 to 604). The estimate is about 21 % low on route B. | MEASURED / DERIVED | as above | VID-011's gap, on route B too |

## QR (qr-q2, files 42 to 47)

| Claim | Status | Photo | Check |
|---|---|---|---|
| Text pages p1 and p2: the 16 payloads and "RESULT QR.1 codes the encoder refused: 0 of 16" | MEASURED | 152918980, 152920667 | Both page CRCs agree with the page rebuilt from `tools/qr_check.py`'s generator. **The console's 16 payloads are exactly the host's** (ids and CRCs: 5aedd6f3, fe6fd86e, 3a421d5f, 480c7121, 5a530b64, 670258d7, 9ab7b7d8, c937f612, 53b8e585, d58d9dad, 1f0d4a44, 2cd74302, 40b58f35, 9ea0503a, 5866e62b, d240d0ea) |
| Code pages p3 to p6, as listed in the photo table, all at panel 232 of 255 | MEASURED (what is shown) | 152922718 to 152928489 | — |
| Which codes a phone read | not in my photos | — | p7 to p9, and the phone's results, are for the next reader or the owner |

- **Naming:** the page header says "QR q1" and the TEST line says "q2". The payloads start "q1" by design (NOTES.md: "same set as qr-q1").
- **Photo observation, not a console claim:** at 8 and 6 px a module, the dark modules carry thin blue vertical lines in the photo; at 10 px they are faint. Whether these come from the camera or the video signal is OPEN. It could matter for reading the codes.

## For Lead: what contradicts the expectations

1. **a3f's pads.** They save 2.7 ms on placing (TOWN-030 said 1.1). But they cost route B 2.4 ms a frame (casters +0.7, receivers +2.1), where the mv/mn pad was meant to help. Net, a3f is worse than a3e on route B still and x60.
2. **bake1's bake** takes 152 ms (TOWN-087: "above 53 ms"), about 179 cycles a test (DERIVED with the host's test count).
3. **bake1's apply** costs about 5 ms on the receivers, against TOWN-082's "under 0.001 ms". It may be partly layout.
4. **Route A's clip part** moves 0.5 to 0.9 ms between binaries whose route A code is the same. Layout sensitivity makes route A a poor control.
5. **The HUD's PVR 17.0 ms** in L2 is above the page's segment max of 14.3. OPEN.
6. **`tools/page_crc.py` strips trailing spaces; the console does not.** No TOWN AUTO page typed exactly can agree with the tool as it is.
