Photos covered: files 1 to 31 of 63 by name, PXL_20261125_151635789 to PXL_20261125_152138118 (c4g p1 to p20, r3-d p1 to p4, three town live views, a3e-auto p1 to p4), and for the QR test alone files 42 to 63, PXL_20261125_152918980 to PXL_20261125_152959865 (section 7; files 42 to 47 are also in the file of Cortana). Files 32 to 41 are not read here.

# Suite disc s1, first run: results, photos 1 to 31 (Senior)

- **Image:** `demo/psm/out/cdi/psm-suite-s1.cdi` (sha256 57d2584b…e438). Spec: `demo/psm/suite/NOTES.md`.
- **Run:** by the owner on 2026-11-25, one console.
- **Photos:** `I:\マイドライブ\dcdev\sh4der-pvr\logs\benchmarks\suite_run1\`, all named `PXL_20261125_<time>.jpg`. Below, only `<time>`.
- **Status of every result row:** MEASURED 2026-11-25, one console, one run, unless the row says DERIVED or ESTIMATE.
- **Reading:** every value below was read clearly. No value in the range was unreadable. The 8 × 16 text photographs well.
- **Two pages were checked against their CRC.** c4g p2 and r3-d p2, typed from the photos, give the header's CRC with `tools/page_crc.py` (70999c8d and 57a5cd02). The CRC line works on a console.
- The leading digit or letter of a screen line is its test. It is never part of a result.

## 1. Which photo is which page

| # | Photo | Test, page | What it holds |
|---|---|---|---|
| 1 | 151635789 | c4g p1/20 | placing, test 0 ramp, test 1 window and edges |
| 2 | 151642186 | c4g p2/20 | test 1 edges, test 3 bias (both edges) |
| 3 | 151643320 | c4g p3/20 | test 4 scenes: 150/26 and 100/13 cosine, 150/26 measured |
| 4 | 151644388 | c4g p4/20 | test 4 scene 100/13 measured; sweep S at 150/26, b0 0 to 2.5 |
| 5 | 151645376 | c4g p5/20 | sweep S: 150/26 b0 3.0, 100/13 all b0 |
| 6 | 151646331 | c4g p6/20 | test 5 cost; page C, bands 1 wide |
| 7 | 151717176 | c4g p7/20 | page C, bands 2 and 4, one Q; test 6 start |
| 8 | 151718936 | c4g p8/20 | test 6 end; test 7 DMA; test 8 KallistiOS waits, map |
| 9 | 151720517 | c4g p9/20 | test 8: mask, screen, frames; ready POLLED |
| 10 | 151721842 | c4g p10/20 | test 8: ready POLLED end; 8b ready and done POLLED |
| 11 | 151723069 | c4g p11/20 | 8b end; test 9 punch-through route |
| 12 | 151724289 | c4g p12/20 | two note lines only |
| 13 | 151726702 | c4g p13/20 GRAPH | test 3, cosine edge |
| 14 | 151728377 | c4g p14/20 GRAPH | test 3, measured edge (flat at 0) |
| 15 | 151731065 | c4g p15/20 GRAPH | sweep, sun 150/26 |
| 16 | 151732408 | c4g p16/20 GRAPH | sweep, sun 100/13 |
| 17 | 151734295 | c4g p17/20 GRAPH | page C, the cut in parts |
| 18 | 151735925 | c4g p18/20 GRAPH | test 8, each frame's time |
| 19 | 151738317 | c4g p19/20 GRAPH | test 8, done wait of each pass |
| 20 | 151739858 | c4g p20/20 GRAPH | test 8, vblanks a frame |
| 21 | 151800307 | r3-d p1/4 | lines A to D2; fade rows a 0 to a 160 |
| 22 | 151802416 | r3-d p2/4 | fade rows a 168 to a 255; the rules line |
| 23 | 151805125 | r3-d p3/4 GRAPH | fade curve against three models |
| 24 | 151807612 | r3-d p4/4 PICTURE | six rows, a 16 to a 240 |
| 25 | 151852888 | town, live view | ROUTE A, frame F000201. The HUD's last line is garbled |
| 26 | 152030168 | town, live view | ROUTE B, frame F000561, 14:00 x1 |
| 27 | 152107251 | town, live view | ROUTE B, frame F001001, 14:02 x60 |
| 28 | 152129527 | a3e-auto p1/4 | A still, A turn, B still |
| 29 | 152131266 | a3e-auto p2/4 | B still events, B turn, B x60 |
| 30 | 152133439 | a3e-auto p3/4 GRAPH | frame time strip |
| 31 | 152138118 | a3e-auto p4/4 GRAPH | mean per segment |

- No photo in the range shows the menu.
- Photos 25 to 27 do not name their image. They sit between r3-d and a3e-auto's pages and show a3e-auto's script (A, then B, then B x60), so they are most likely a3e-auto. That is a reading, not a fact from the photo.

## 2. Did the menu's start and return work?

| ID | Claim | Status | Photo |
|---|---|---|---|
| SU-01 | Every result page in the range carries "ABXY menu" in its header: all 20 of c4g, all 3 kit pages of r3-d, all 4 of a3e-auto. So in each of the three tests `suite_init` found and loaded `/cd/MENU.BIN` after a start through `arch_exec`. | MEASURED | 1 to 23, 28 to 31 |
| SU-02 | r3-d p4 (the picture) has no "ABXY menu" text. That page draws its own header; the keys still work there. Not a fault. | FACT from source | 24 |
| SU-03 | c4g's header says "(auto)": it was started by RUN ALL. r3-d's and a3e-auto's headers do not: they were started by hand. | MEASURED | 1, 21, 28 |
| SU-04 | c4g's pages were photographed 1 to 2 s apart, so the owner turned them by hand in a chained run. Then r3-d shows no "(auto)". This fits a return by A+B+X+Y, which ends the chain by design. No photo shows the chain starting a second test by itself. | DERIVED from the photo times | 1 to 21 |
| SU-05 | Three different tests ran in 5 minutes (15:16:35 to 15:21:38). Whether the console was switched off between them cannot be seen in a photo. | — | — |
| SU-06 | Not seen in this range: the menu itself, the 5 s countdown between chained tests, a chained test returning after its last page, a fault page's countdown. | OPEN | — |

**Verdict for (a).** No "ABXY menu" header is missing where one should be. The start through the menu and the load of the menu by the test worked for c4g, r3-d and a3e-auto. The return and the unattended chain are not shown by these photos; the other half of the folder, or the owner, has to say.

## 3. c4g against c4b

c4b ran from its own disc after a power cycle. c4g ran from the menu. Same console, same day.

### 3.1 Counts (pixels, bands, vertices)

| ID | Claim | Photo | c4b | Verdict |
|---|---|---|---|---|
| C4G-01 | PLACING: bsrc 8c1a57a0, bscr 8c1e77a0, bref 8c22a7a0; apart mod 16 KB: 8192, 4096, 12288 | 1 | 8c19ca40 …; the same three distances | addresses moved (new code), distances kept |
| C4G-02 | Test 0: fetch matches 256 words. Plain ramp 2,303 wrong, worst 1 code. 8 sub-steps 348 wrong, worst 1. Whole codes 0 of 16,384 wrong | 1 | the same | same to the pixel |
| C4G-03 | Test 1 window: w 16 thr 10 differ 784 (320 within 2 levels); w 32 thr 38 differ 443 (320); w 64 thr 128 differ 240 (240); holes 0 | 1 | the same | same |
| C4G-04 | EDGES w 32 thr 38: lit from 99 to 157 (cosine 97 to 159); 2 mixed; 256 of 256 seen; 1 under 128 and 1 over unlike the cosine | 1, 2 | the same | **the window edge repeats: 99 to 157** |
| C4G-05 | Test 3, cosine edge, shadow of 34,928 px, b0 0 / .5 / 1 / 1.5 / 2 / 2.5 / 3. Bands 1: 16,867 2,774 2,774 0 0 0 0. Bands 2: 2,981 2,775 2,775 0 0 0 0. Bands 4: 1,485 1,364 1,364 0 0 0 0 | 2 | bands 1 and 4 the same at the six b0 it had | same; bands 2 and b0 2.5 are new |
| C4G-06 | Test 3, measured edge: 0 at all seven b0, bands 1, 2 and 4 | 2 | 0 at six b0, bands 1 and 4 | same; now also bands 2 |
| C4G-07 | Test 4, 150/26, b0 3.0, cosine: map same 16,115, 1 off 269; 322 bands, 1,288 vertices; mask vs model 24 / 15; lit 36,418, shadow 1,378; acne 0, edge 546, rim 0; lit in error 6 + 0 | 3 | the same | same to the pixel |
| C4G-08 | Test 4, 100/13, cosine: same 16,226, 1 off 158; 337 bands, 1,348 vertices; 22 / 14; lit 34,889, shadow 2,907; acne 0, edge 1,254; lit in error 6 + 10 | 3 | the same | same |
| C4G-09 | Test 4, 150/26, measured: 12 / 2; acne 0, edge 495; lit in error 30 + 0 | 3 | the same | same |
| C4G-10 | Test 4, 100/13, measured: 14 / 16; acne 0, edge 1,222; lit in error 27 + 20 | 4 | the same | same |
| C4G-11 | Test 6 punch, ref 128: KallistiOS's header 16,384; no depth write 16,384; depth ALWAYS 16,384; blend ONE/ZERO 32,768; vertex alpha 128: 2,048; bump level as alpha 10,200 of 20,480, model 10,320, differ 120 | 7, 8 | the same | same |
| C4G-12 | Test 9 route: 150/26 shadow px 1555 1,843, punch-through 1,843, differ 0. 100/13: 4,082 and 4,082, differ 0. Colours ffff only | 11 | the same | same |

**Every count in c4g equals c4b's.** That is 12 groups of pixel counts, from a menu start against a power-cycle start.

### 3.2 Times

| ID | Claim | Photo | c4b | Verdict |
|---|---|---|---|---|
| C4G-13 | Test 5, bands 1 / 2 / 4: fit 54 / 56 / 56 us; cut 3,183 / 1,694 / 897 us | 6 | fit 54 / 54 / 55; cut 3,183 / 1,691 / 908 | the cut repeats within 1.2 % |
| C4G-14 | Project: asm 60.0 / 61.2 / 60.8 cycles a vertex; C form 74.7 / 75.1 / 77.2; bytes the same | 6 | 60.8 / 61.7 / 60.5; 76.7 / 77.6 / 79.2 | repeats within 1.3 % (asm) |
| C4G-15 | Send: map 14 us; mask 846 / 466 / 274 us | 6 | 846 / 512 / 273 | bands 2 is 9 % faster here; the other two repeat |
| C4G-16 | PVR: map 3,288 / 3,288 / 3,284 us; mask 9,218 / 6,272 / 5,152 us; screen 3,321 us, with the layer 9,642 us | 6 | 3,292 / 3,287 / 3,292; 9,228 / 6,225 / 5,124; 3,320 / 9,615 | within 0.8 % |
| C4G-17 | Test 7 DMA, 128 KB: 4,020 us (1 ms poll), 3,308 (250 us poll), 3,435 (interrupt). 512 KB: 14,065, 13,489, 13,733. Every copy right | 8 | 4,011 / 3,537 / 3,388; 14,040 / 14,148 / 13,478 | 1 ms poll repeats within 0.3 %; the 250 us poll moved by 5 to 6 % |
| C4G-18 | Test 9 PVR mask pass: 1555 6,135 us, punch-through 7,230 us (150/26); 6,921 and 7,925 us (100/13) | 11 | 6,128 / 7,234; 6,962 / 7,920 | within 0.6 % |

**For the "clean state" question (suite NOTES, OPEN).** A start through the menu gave c4b's counts to the pixel and c4b's times within about 1 %, apart from two rows that also moved between c4 and c4b. This is the first evidence that a menu start does not change a result. It is one test on one console, and c4g is not the same binary as c4b, so the question stays OPEN until the same binary is run both ways.

## 4. The open questions

### Q53: is the scheduler tick the whole cause of the long frame? Yes.

| ID | Claim | Photo |
|---|---|---|
| Q53-1 | KallistiOS waits (sleeping `pvr_wait_render_done`): frame min 38,520, mean 39,909, max 39,934 us. Vblanks a frame 0 .. 3: 0 0 36 24. Frames 1 to 8: 38,520 39,932 39,932 39,933 39,932 39,934 39,932 39,933 | 8, 9 |
| Q53-2 | The same run, by pass, mean / max us. Map: done wait 7,930 / 7,965, render 3,303; **the wait came back 4,645 / 4,699 us after the render-done interrupt.** Mask: 9,047 / 9,053, render 6,134; **back 2,929 / 2,940 us after.** Screen: 19,830 / 19,837, render 9,906; **back 9,940 / 9,981 us after** | 8, 9 |
| Q53-3 | Ready waits polled, done wait still sleeping: frame mean 39,918 us; back 4,655 / 2,935 / 9,939 us after the interrupt. No change | 9, 10 |
| Q53-4 | **8b, ready and done both polled: frame min 22,390, mean 23,264, max 26,535 us.** Vblanks a frame: 0 37 23. Map done 3,278 (render 3,291), back 8 us after the interrupt. Mask done 6,180 (render 6,190), back 7 us. Screen done 9,900 (render 9,953), back 7 us. Frames 1 to 8: 22,440 22,506 22,390 24,920 22,471 22,457 22,550 22,464 | 10, 11 |
| Q53-5 | Mean frame minus mean waits: 3,003 us (KallistiOS waits), 3,004 (ready polled), 3,042 (8b) | 9, 10, 11 |
| Q53-6 | Mean frame by the cycle counter: 3,400 us with KallistiOS's waits, 3,408 with ready polled, **23,266 us in 8b** (ns clock: 23,264) | 9, 10, 11 |

Readings (DERIVED from the rows above):
- **The interrupt is on time; the wake is late.** With the sleeping wait, the program got the render-done interrupt itself at the render's end, and `pvr_wait_render_done` returned 2.9, 4.6 and 9.9 ms later. With the wait polled it returned 7 to 8 us later.
- **The frame falls from 39.9 ms to 23.3 ms**: 42 % shorter, 43 pictures a second against 25. Nothing else changed between the runs.
- **The 8b frame is fully accounted for.** Renders 3,291 + 6,190 + 9,953 = 19,434 us, plus the SH-4's 3,042, is 22,476 us. The typical frame is 22,440 to 22,550.
- **The late wake is on a 10 ms grid.** The sleeping frame is 39,932 us in every frame after the first: 4.00 scheduler ticks of 9,983 us. This is what the reading of KallistiOS's source predicted (the render-done wake does not reschedule; ESTIMATE until this run).
- **Q53 is answered: the tick is the whole cause.** PVR-094 and PVR-104 can close on it.
- **The cycle counter stops while the CPU sleeps.** With sleeping waits it counts 3,400 us of a 39,909 us frame, which is the SH-4's own work. With polled waits it agrees with the ns clock to 2 us (23,266 against 23,264). So the ns clock with the reload step taken out is sound, and a cycle count across a KallistiOS wait is not wall time.
- **New, OPEN: a spike every fifth frame in 8b.** The strip (photo 18) shows about 26.5 ms about every 5 frames. The screen pass's list close is 786 us mean but 4,033 us max. Likely the page flip waiting for a vertical blank; not tested.

What it means for the town (ESTIMATE): any frame that uses `pvr_wait_render_done`, or KallistiOS's `pvr_scene_finish` with a wait, pays up to 10 ms a pass. A polled wait on the render-done event removes it. This is KallistiOS behaviour, to note in our files; nothing in KallistiOS is to be changed.

### Q57: how low can b0 go with the measured edge? To 0, at both suns.

| ID | Sweep, bands 2, grow .75, measured edge; b0 0 / 0.5 / 1 / 1.5 / 2 / 2.5 / 3 | Photo |
|---|---|---|
| Q57-1 | Sun 150/26. Acne: 0 at all seven. Edge: 546 519 519 512 512 495 495. Real shadow shown lit (in bias + other): 6+0, 8+0, 8+0, 14+0, 14+0, 30+0, 30+0. Against the model: 10+12, 8+2, 8+2, 9+2, 9+2, 12+2, 12+2. Rim 0, holes 0 | 4, 5 |
| Q57-2 | Sun 100/13. Acne: 0 at all seven. Edge: 1,254 1,238 1,238 1,230 1,230 1,222 1,222. Shown lit: 4+12, 10+12, 10+14, 15+13, 15+17, 27+16, 27+20. Against the model: 10+8, 10+0, 10+2, 14+4, 12+6, 9+4, 14+16. Rim 0, holes 0 | 5 |
| Q57-3 | The host model's prediction (printed on the page, ESTIMATE): acne 0 at every b0; edge 617 to 503 and 1,262 to 1,225; lit in error 4 to 17 and 17 to 43 | 5 |

Readings (DERIVED):
- **Acne is 0 at every b0 from 0 to 3, in both scenes, at both suns.** With the measured edge, b0 is no longer needed against acne here.
- **b0 now only trades two small errors.** From b0 0 to 3 the false shadow at edges falls by 51 px (546 to 495) and 32 px (1,254 to 1,222). Real shadow shown lit rises by 24 px (6 to 30) and 31 px (16 to 47). All of this is under 0.1 % of the 76,800-pixel mask.
- **The model was right in direction and size.** It gave 0 acne, the edge's fall and the rise of shadow shown lit. It was off by up to 71 px on the edge at b0 0.
- **Q57 is answered for two suns.** The question asked for three; a third sun is still OPEN. A default of b0 0 to 1 is supported; c4b's 3.0 and the live view's 5.25 are higher than needed.

### Q64: why is c4b's band cut a quarter slower than c4's? Not the Q. Cause still OPEN.

| ID | Claim | Photo |
|---|---|---|
| Q64-1 | The cut repeats c4b: 3,183 / 1,694 / 897 us by the ns clock (c4b 3,183 / 1,691 / 908; c4 2,483 / 1,332 / 744) | 6 |
| Q64-2 | Page C, by the cycle counter, mean of 8. Whole cut, bands 1 / 2 / 4: cosine edge 3,207 / 1,711 / 896 us; measured edge 3,218 / 1,711 / 895 us | 6, 7 |
| Q64-3 | One Q: from the measured edge 21.0 cycles, from the cosine's 21.2 (65,536 calls each, loop included) | 7 |
| Q64-4 | Parts, measured edge, us: faces 15; kinds, corners, bias 64; band polygons 2,106 / 1,083 / 570; Q and the strips 1,031 / 548 / 246 | 6, 7 |
| Q64-5 | Parts, cosine edge, us: faces 15; kinds, corners, bias 64; band polygons 2,104 / 1,074 / 568; Q and the strips 1,023 / 557 / 248 | 7 |

Readings (DERIVED):
- **`c4_q_set` against `c4_q` is ruled out.** The two forms of Q cost the same (21.0 and 21.2 cycles), and the whole cut is the same with either edge, within 11 us.
- **Where the time is:** band polygons are 65 % of the cut, Q and the strips 32 %, the rest 2 %.
- **Q64 stays OPEN, with one cause removed.** What is left is the second test Q64 named: the placing of the cut's own arrays against `vpool`. c4's source is the comparison; that run has no parts, so which part grew cannot be read from these photos.
- A cheap next test: build c4's cut and c4g's cut into one image and print both in parts.

### PVR-064: how the product is rounded. None of the four models; one simple rule fits every row.

| ID | Claim | Photo |
|---|---|---|
| R3-01 | Line A: a 255, r 128: n 08, lo 08, px 16,384, clean step (models 8 8 8 8) | 21 |
| R3-02 | Line B: a 128, r 128: n 01, lo 15, px 2,048 (models 1 1 1 1) | 21 |
| R3-03 | Line C: a 40, r 16: n 09, lo 07, px 18,432 (models 9 9 10 10). C2 the same | 21 |
| R3-04 | Line D: a 174, r 128: n 04, lo 12, px 8,192 (models 4 5 5 4). D2 the same | 21 |
| R3-05 | Line F: a 16, r 16: n 01, lo 15, px 2,048 (models 0 1 1 1) | 21 |
| R3-06 | The page's own verdict: "DERIVED NO RULE", and for the fade "VTX256 0 BOTH256 0 ROUND 0 FLOOR 0" | 21, 22 |
| R3-07 | Fade, r 16, vertex alpha → nibbles drawn: 0→0, 8→0, 16→1, 24→5, 32→8, 40→9, 48→10, 56→11, 64→12, 72→12, 80→12, 88→13, 96→13, 104→13, 112→13, 120→13, 128→14, 136 to 240→14 (14 rows), 248→15, 255→15. All 33 rows a clean step | 21, 22 |
| R3-08 | The picture: the drawn part of each row grows from a 16 (a sliver at the right) to a 240 (all but the left end), in steps | 24 |

Readings:
- **No one of VTX256, BOTH256, ROUND, FLOOR fits.** C needs VTX256 or BOTH256, D needs VTX256 or FLOOR, F rules VTX256 out. (DERIVED, as the page says.)
- **A rule that fits all 38 rows** (the 33 fade rows and A, B, C, D, F), found on the host by trying 360 simple forms (`D:\tmp\psmbuild\r3_fit.py`; exactly one of the 360 fits):

  `tested = ((t + 1) × (a + 1)) >> 8`, drawn when `tested > reference`, with `t` the texel's alpha nibble × 17 (0 .. 255) and `a` the vertex alpha.

- **Status of the rule: ESTIMATE.** It is a fit to MEASURED rows, not a reading of the hardware. Other forms outside the 360 may fit too. With a 4-bit texel only 16 values of `t` can be tried.
- **It agrees with what was already measured.** At vertex alpha 255 it becomes `t + 1 > reference`, which is `t >= reference`: PVR-060. And it explains PVR-064's odd row: nibble 15 at vertex alpha 128 gives (256 × 129) >> 8 = 129, which passes 128, where a plain (255 × 128) >> 8 = 127 would not.
- **For the recipes (Cortana's file; not edited by me):** `rcp_pt_product` needs a fifth rule of this form, and the dither fade table should be built from it. At reference 16 the fade is steep at the low end (0 cells at a 8, 5 at a 24, 8 at a 32) and flat above a 128. A fade by vertex alpha at a fixed reference is not linear; the table read (`rcp_dither_table`) is the way to make it even.
- **A test that would raise the rule's status:** the same page at two more references (say 64 and 200) and at vertex alphas 1 apart around each step. If the rule predicts every step, call it MEASURED over that range.

## 5. Town pages in the range (for Junior; transcribed, not judged)

Photo 28 and 29, a3e-auto: 1,200 frames counted, sim step 1/30 s, vertex buffer 1,024 KB × 2. Per segment min / mean / max, ms unless said.

| Segment | Frame | SH-4 | Parts | PVR | TA vertices | Used KB | Est KB | Events |
|---|---|---|---|---|---|---|---|---|
| A still (150 fr) | 54.1 / 56.1 / 83.7 | 12.2 / 13.0 / 26.6 | 2.8 6.2 4.0 0.0 | 3.3 / 3.8 / 12.1 | 30,541 / 30,613 / 30,624 | 65 / 768 / 1,024 | 754 / 754 | PARAM 150, OPB 150, OVERFLOW |
| A turn (300 fr) | 54.2 / 74.2 / 104.3 | 12.3 / 31.7 / 47.3 | 21.8 5.9 4.0 0.0 | 3.3 / 4.0 / 14.3 | 24,890 / 29,057 / 30,640 | 52 / 659 / 1,024 | 724 / 754 | PARAM 56, OPB 300, OVERFLOW |
| B still (150 fr) | 74.2 / 75.9 / 94.6 | 42.7 / 43.1 / 63.2 | 2.9 0.7 15.3 24.1 | 8.6 / 8.8 / 14.3 | 18,824 / 18,832 / 18,832 | 601 / 604 / 890 | 477 / 477 | PARAM 0, OPB 75, OVERFLOW |
| B turn (300 fr) | 75.1 / 90.8 / 115.6 | 42.9 / 59.0 / 65.7 | 21.8 0.7 13.7 22.8 | 7.9 / 8.3 / 10.6 | 13,855 / 17,466 / 18,852 | 456 / 540 / 603 | 449 / 477 | PARAM 0, OPB 150, OVERFLOW |
| B x60 (300 fr) | 73.6 / 75.6 / 101.7 | 42.2 / 42.9 / 62.7 | 2.8 0.7 15.4 24.0 | 8.1 / 8.7 / 11.3 | 18,752 / 18,779 / 18,864 | 600 / 601 / 602 | 476 / 478 | PARAM 0, OPB 150, OVERFLOW |

- Every segment: ISP 0, strip 0, in-err 0, in-ovf 0, KOS max 1,024K, SHED<=0, text 92v.
- The page ends "END. OVERFLOW: the TA ran out of vertex buffer or bins."
- Photo 25 (ROUTE A, F000201): HUD line 2 reads "TA est 754K used 65K max 1024K 100% of 1024K SHED0". The HUD's last line is garbled, and the lines under "WARP … CASTERS ALL" that photo 26 shows are missing.
- Photo 26 (ROUTE B, F000561, 14:00 x1): SH-4 42.8 ms, PVR 8.7 ms, TA 18,832 v, FPS 13; B: build 2.8, clear 0.7, casters 15.3, recv 24.1 ms; tris 1,101 / 1,162; "used 602K".
- Photo 27 (ROUTE B, F001001, 14:02 x60): SH-4 42.8 ms, PVR 8.7 ms, TA 18,776 v, FPS 13; B: build 2.8, clear 0.7, casters 15.4, recv 24.0 ms.
- One cross-reading for Junior: the town's frames are 54 to 115 ms with renders of 3 to 14 ms. If the town waits with KallistiOS's sleeping render-done wait, section 4's Q53 result applies to it: up to 10 ms a pass is the wake, not work.

## 6. A town test goes black: the menu's side checked

**Which test.** Lead's message named bake1-auto. Photos 38 to 41 (Cortana's range) are bake1-auto's four result pages, so bake1 ran to its end. Junior's reading (TOWN-106) is that the test that hangs is hud1-auto: only hud1 has an instructions screen, its boot menu. I agree with that reading. The checks below were made for BAKE1.BIN before that was known; they hold for HUD1.BIN too, which is smaller (805,652 bytes, end 0x8c2df104, the same bytes on the disc as built, one mail mark).

Checked on the host. Nothing here is from a console.

| Check | Result |
|---|---|
| Link and start address of BAKE1's ELF | 0x8c010000, as every other binary (the suite build refuses any other) |
| Size of BAKE1.BIN | 813,108 bytes. A3E.BIN is 804,852 and HUD1.BIN 805,652, and both ran or loaded. It is 8,256 bytes larger: 1 % |
| Image and .bss end | 0x8c2f12a4 (a3e: 0x8c2cea24). 13 MB of RAM is left above it |
| BAKE1.BIN on the disc against the built binary | The same bytes in all 398 sectors (read back out of `psm-suite-s1.cdi`) |
| The mail mark | Exactly one, at a multiple of 4 |
| The exec's copy | The menu holds the image in its heap, above 0x8c010000 + the image's length or overlapping it from above. The copy runs upward to a lower address, so an overlap is safe. This is the same for A3E.BIN, which runs. No size step lies between 804,852 and 813,108 bytes in the loader or in `arch_exec` |
| Source difference to a3e-auto before the first frame | None. The diff of the two `psm_dc.c` copies is the bake only. `main` runs the same lines up to the frame loop |

**Verdict: nothing on the menu or exec side differs for BAKE1 in kind.** I cannot show from the host what hangs. Two things would tell, both for the owner:

1. **When does it go black?** If the town is seen first (the HUD's top lines, with "stick look YAXB walk …") and the black comes when line 1 would change to ROUTE B, about 30 to 40 s in: that is the first bake (`psm_bake_run` is called on the first route B frame only). If it goes black straight from the menu's "loading bake1-auto" screen with no town at all: that is the start.
2. **Boot `sh4der-psm-town-bake1-auto.cdi` by itself** (the image built before the suite). Black there too: the fault is in bake1, not in the suite. Runs there: the suite path is involved.

Observations for Junior from reading `psm_bake.h` (his copy, not edited):
- bake1 has never run on a console before today (TOWN-085 was OPEN), so its first run is this one.
- A black screen with no fault page means the picture was cleared and nothing drew after it. The clear is in `pvr_shutdown` (before the result pages, and in the exit to the menu) and in the fault page's own first step. A fault page whose text does not show would look like this.
- If the black comes at the first bake, the fault catcher should have shown a page within 2 s unless interrupts were masked. That it did not is itself a clue.
- In `psm_bake_bins_` the lower bin indices `u0` and `v0` are converted from float to unsigned with no clamp. A value just under 0 is undefined in C. On the host it gave no fault; on the SH-4 the conversion may differ. I did not find a write out of bounds from it (the upper indices are clamped), so this is a thing to rule out, not a finding.

For hud1 the same verdict holds: nothing on the menu or exec side differs in kind. What hud1 alone does after the exec is on Junior's list (TOWN-107): its boot menu reads the pad and sleeps before `pvr_init`, and it enables its ASIC events before `load()`. Junior's hud1b-auto prints a line at each boot step; it goes on suite s2 in hud1-auto's place.

## 7. QR reading test (qr-q2), photos 42 to 63

Photos 42 to 47 are also in Cortana's file. Read here because lead asked for the QR result as a whole.

### Which photo is which page

| # | Photo | Page | Panel |
|---|---|---|---|
| 42 | 152918980 | p1/9 text, CRC 547c7678 | — |
| 43 | 152920667 | p2/9 text, CRC 71307c17 | — |
| 44 | 152922718 | p3/9, V3 at 10 px, 1 code | 232 |
| 45 | 152924332 | p4/9, V3 at 8 px, 1 code | 232 |
| 46 | 152926808 | p5/9, V5 at 8 px, 1 code | 232 |
| 47 | 152928489 | p6/9, V3 at 6 px, 2 codes | 232 |
| 48 | 152930450 | p7/9, V5 at 6 px, 1 code | 232 |
| 49 | 152932395 | p8/9, V3 at 4 px, 6 codes | 232 |
| 50 | 152933877 | p9/9, V5 at 4 px, 4 codes | 232 |
| 51, 52, 53 | 152949254, 152950084, 152950744 | p4/9 | 255, 176, 232 |
| 54, 55, 56 | 152952231, 152952913, 152953593 | p5/9 | 255, 176, 232 |
| 57, 58, 59 | 152954883, 152955517, 152956255 | p6/9 | 255, 176, 232 |
| 60, 61 | 152957344, 152957965 | p7/9 | 255, 176 |
| 62, 63 | 152959249, 152959865 | p8/9 | 232, 255 |

- Every page has "ABXY menu" or its own QR header. The text pages say "q2", the suite build.
- The panel level of photos 51 to 63 was read from the page's own "panel N" text.

### Results

| ID | Claim | Status | Photo |
|---|---|---|---|
| QR-01 | "RESULT QR.1 codes the encoder refused: 0 of 16". Confirmed; Cortana read the same line | MEASURED | 43 |
| QR-02 | The 16 ids and CRCs on p1 and p2 are the host generator's (3-10-a 5aedd6f3, 3-08-a fe6fd86e, 5-08-a 3a421d5f, …) | MEASURED; Cortana checked both page CRCs | 42, 43 |
| QR-03 | **All 16 codes decode from the first-pass photos (panel 232), each to its exact payload with its CRC right.** Reader: OpenCV's QR detector on the phone's photo, tried at four sizes, three blurs, with and without a threshold (`tools/qr_check.py --photos`) | MEASURED 2026-11-25, one console, one television, one phone, one photo a page | 44 to 50 |
| QR-04 | By module size, first pass: 10 px 1 of 1; 8 px 2 of 2 (V3 and V5); 6 px 3 of 3 (two V3, one V5); 4 px 10 of 10 (six V3, four V5) | MEASURED | 44 to 50 |
| QR-05 | Panel level 255 and 176: the 8 px and 6 px pages decode at both levels, every code (p4, p5, p6, p7) | MEASURED | 51 to 61 |
| QR-06 | The six-code page at 4 px is the weak one: 6 of 6 in photo 49, 5 of 6 in photo 62 (both panel 232), 3 of 6 in photo 63 (panel 255) | MEASURED | 49, 62, 63 |
| QR-07 | One pass of the detector at the photo's own size reads only 7 of the 16 in the first-pass photos. The other 9 need a smaller copy of the photo or a threshold | MEASURED (host, on the same photos) | 44 to 50 |
| QR-08 | In the photos the dark modules carry thin blue vertical stripes. They did not stop a decode | a look, not a count | 45, 47 |

Readings:
- **A QR code on a result page works**, read from a photograph of the television. That is the route the result pages already use: the owner photographs, a reader session decodes. No typing, and each payload checks itself by its CRC.
- **8 px is a safe default**, as planned: both versions, all three panel levels, every photo.
- **6 px is safe on this evidence**: 11 code readings of 11 across three panel levels.
- **4 px works but not every time** when six codes share a page: 14 of 18 readings. Four version 5 codes at 4 px read 4 of 4, in one photo only.
- **The panel level did not matter** at 8 and 6 px. Leave it at 232.
- **Not shown by the photos: whether the phone's own camera reads a code live.** The owner's notes on that are not in the folder. QR-03 is about a decode from the photo.
- **What a page can carry (DERIVED from the capacities):** one V5 code at 8 px, 84 bytes. Two V3 codes at 6 px, 84 bytes; one V5 at 6 px beside text, 84 bytes. Four V5 codes at 4 px, 336 bytes, with a second photo as insurance.
- **Still OPEN:** another television or cable; version 5 at 6 px two to a page; versions above 5 at 4 px; a real result page's numbers in a code beside its text.

## 8. Two cross-checks lead asked for

**OPB on route B: half the frames, not every frame.** In my range a3e-auto's pages (photos 28 and 29) read:

| Segment | Frames | OPB events | PARAM events |
|---|---|---|---|
| A still | 150 | 150 | 150 |
| A turn | 300 | 300 | 56 |
| B still | 150 | **75** | 0 |
| B turn | 300 | **150** | 0 |
| B x60 | 300 | **150** | 0 |

- B still's events line is the first line of p2 ("events ISP 0 strip 0 PARAM 0 OPB 75 in-err 0 in-ovf 0"); its segment starts at the foot of p1. Read as belonging to A turn or to nothing, it is easy to lose.
- So on route B the OPB event fired on exactly half the frames in all three segments. This agrees with Cortana's reading and not with TOWN-104's "every frame of every segment". On route A it is every frame.
- Cortana's two a3e pages were checked against their CRCs, so the digits are not in doubt.

**`tools/page_crc.py` and trailing spaces: fixed in the tool; the console is unchanged.**
- The tool now keeps every trailing space that is typed.
- A photo does not show a trailing space. So when a typed page does not agree, the tool tries one more space at the end of each line that is followed by a continued line, in every combination, and names the lines that needed it.
- Its self-test holds the town line that showed the fault ("… SHED<=0 " before "   text 92v") and the console's CRC of it, 10badf18. `compare/test/psm_page_test.c` has the same line and the same value from the console's own code, so the two cannot drift apart.
- Both pages I had checked (c4g p2, r3-d p2) still agree: neither has a double space at a cut.
