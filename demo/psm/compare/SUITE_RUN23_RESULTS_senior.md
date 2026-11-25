Photos covered: `logs/benchmarks/suite_run2`, all 23 files, PXL_20261125_161837278 to PXL_20261125_161941715 (c4g p1 to p20, r3-d p1 to p3), and `logs/benchmarks/suite_run3`, files 1 to 4 of 19 by name, PXL_20261125_162319785 to PXL_20261125_162525879 (ao0 p1 and p2, one town live view, ao1 p1). Files 5 to 19 of suite_run3 are not read here.

# Suite s3, runs 2 and 3: the photos in Senior's range

Read by Senior on 2026-11-25. Image: `psm-suite-s3.cdi` (sha256 b41afae5…a244). One console, one television, one phone.

**Status words.** MEASURED: on the page in the photo, 2026-11-25, one console. DERIVED: worked out here from MEASURED rows. OPEN: not answered.

**How sure the digits are.** Every text page in the range was typed from its photo and gives the CRC in its header with `tools/page_crc.py`: 12 pages of c4g, r3-d p1, both pages of ao0, ao1 p1. The typed pages are in `compare/suite_run23_pages/`. The graph pages were not typed; their header CRCs are listed.

## 1. The short answers

| Question | Answer | Status |
|---|---|---|
| Holly revision (PVR-123) | **"HOLLY REV 0011 SB 0B"**: REVISION reads 0x0011, SB_SBREV reads 0x0B. By PVR-123's table that is a 2.x core with the 2.42 system bus value | MEASURED, on two programs' pages (ao0 p1, ao1 p1) |
| Does the depth union work (nearest row PVR-046)? | **No, not as one darkening a direction.** The page says "1 DERIVED UNION FAILS" at every K. It is not plain blending either: see section 3 | MEASURED |
| One side only | **It draws one darkening a caster with half the vertices, as the host test said.** Where two casters' shadows overlap it darkens twice; that is the page's "differs where two casters overlap" | MEASURED |
| Cost of a direction at 1,164 caster triangles, both sides | SH-4 1,824 us, PVR 1,213 us, 3,492 vertices, 68 KB of TA | MEASURED |
| The same, one side | SH-4 about 1,280 us, PVR about 750 us, 1,746 vertices, 34 KB | DERIVED from the K 8 rows |
| TA out of memory | parameters 0, tile bins 46 events, ISP 0, over the whole run. Which rows: not on the page | MEASURED; rows OPEN |
| The menu with the revision line | not photographed | OPEN |
| ao0's live view | not photographed | OPEN |

## 2. Photo table

`suite_run2` (PXL_20261125_1618… to 1619…):

| # | File (time part) | What it shows | Header CRC | Typed page agrees |
|---|---|---|---|---|
| 1 | 161837278 | c4g p1/20: placing, test 0, test 1 | 28af9af6 | yes |
| 2 | 161839389 | c4g p2/20: edges, test 3 | 70999c8d | yes; the same CRC as run 1's p2 |
| 3 | 161840509 | c4g p3/20: test 4 scenes | 7bc18e68 | yes |
| 4 | 161841471 | c4g p4/20: test 4, sweep 150/26 | f1a0876e | yes |
| 5 | 161842449 | c4g p5/20: sweep 100/13, test 5 title | ccbfef6c | yes |
| 6 | 161843361 | c4g p6/20: test 5 cost, page C bands 1 | 62448016 | yes |
| 7 | 161844302 | c4g p7/20: page C, test 6 start | 354c2089 | yes |
| 8 | 161845280 | c4g p8/20: test 6 end, test 7, test 8 start | 23348d40 | yes |
| 9 | 161846332 | c4g p9/20: test 8, KallistiOS waits and ready polled | df542fe2 | yes |
| 10 | 161847258 | c4g p10/20: test 8, 8b start | 240080bd | yes |
| 11 | 161848148 | c4g p11/20: 8b end, test 9 | 91e4a934 | yes |
| 12 | 161849029 | c4g p12/20: two note lines | de3f7641 | yes |
| 13 | 161850603 | c4g p13/20 GRAPH: test 3, cosine edge | da44aa22 | not typed |
| 14 | 161851545 | c4g p14/20 GRAPH: test 3, measured edge | 275eac61 | not typed |
| 15 | 161852500 | c4g p15/20 GRAPH: sweep 150/26 | f831a418 | not typed |
| 16 | 161853443 | c4g p16/20 GRAPH: sweep 100/13 | 7981c48b | not typed |
| 17 | 161854835 | c4g p17/20 GRAPH: the cut in parts | c89bb04e | not typed |
| 18 | 161857241 | c4g p18/20 GRAPH: each frame's time | aaea95ca | not typed |
| 19 | 161902133 | c4g p19/20 GRAPH: done wait of each pass | 25e93c2f | not typed |
| 20 | 161906311 | c4g p20/20 GRAPH: vblanks a frame | 5817e25b | not typed |
| 21 | 161933273 | r3-d p1/4: lines A to D2, fade a 0 to a 160 | 5cee9f4f | yes |
| 22 | 161934962 | r3-d p2/4 | 57a5cd02 | the same CRC as run 1's p2, which was typed then |
| 23 | 161941715 | r3-d p3/4 GRAPH: the fade curve | 4ae02d97 | not typed |

`suite_run3`, files 1 to 4:

| # | File (time part) | What it shows | Header CRC | Typed page agrees |
|---|---|---|---|---|
| 24 | 162319785 | **ao0 p1/2** | 88399579 | yes |
| 25 | 162323520 | **ao0 p2/2** | bf5dc584 | yes |
| 26 | 162353437 | town live view: "ROUTE A  MAP UNIFORM  AO OFF  (X: next)", frame F000121, "TA est 697K used 62K max 974K 95% of 1024K SHED0". The HUD is whole. The time fits ao1's start | none | — |
| 27 | 162525879 | ao1 p1/5 ("TOWN AUTO"): the notes and the two AO OFF segments | 27e4fda8 | yes, with a trailing space on four lines |

- No header says "(auto)": c4g, r3-d, ao0 and ao1 were started by hand, not by RUN ALL.
- Every header says "ABXY menu": each of the four loaded the menu's binary after its start.
- r3-d p4 (the picture) is not in the folder.
- ao1 p1 belongs to Junior's reading; it is typed here because it is in the range and carries the revision.

## 3. ao0: the pages, exactly

Page 1 (CRC 88399579):

```
ao0 b AO, sky directions into the shadow target. This run's
 numbers. U union P plain C one side UC both. 256x256 565
HOLLY REV 0011 SB 0B
1 UNION 48 triangles, no sun. c values/levels b bad of n f flip
1 k1 U c3/2 b477 n63832 f0|P c3 b460|C c3 b460
1 k1 U565 ffff 7bef 39e7
1 k1 C565 ffff 7bef 39e7
1 k2 U c5/3 b1184 n63237 f0|P c5 b870|C c5 b870
1 k2 U565 ffff b5b6 7bef 5acb 39e7
1 k2 C565 ffff b5b6 7bef 5acb 39e7
1 k4 U c8/5 b1310 n62405 f0|P c8 b970|C c8 b970
1 k4 U565 ffff d6ba b596 94b2 7bef 6b4d 52aa 4a49
1 k4 C565 ffff d6ba b596 94b2 7bef 6b4d 52aa 39e7
1 k8 U c15/8 b2541 n61279 f0|P c14 b1764|C c14 b1764
1 k8 U565 ffff ef5d d6ba c638 b596 a534 94b2 8c51 7bef 738e
1 k8 C565 ffff ef5d d6ba c638 b596 a534 94b2 8c51 7bef 738e
1 DERIVED UNION FAILS
1 DERIVED ONE SIDE differs where two casters overlap (b above)
2 COST 1164 triangles + sun. 8 frames a row. us; K = KB
2 k0 U  v0 e68K k68K ao0 sun1822 pvr3877 mx3970
2 k1 U  v3492 e136K k136K ao1824 sun1850 pvr4630 mx4752
2 k2 U  v6984 e205K k205K ao3633 sun1855 pvr5654 mx5702
```

Page 2 (CRC bf5dc584):

```
2 k4 U  v13968 e341K k341K ao7300 sun1848 pvr8298 mx8325
2 k8 U  v27936 e614K k614K ao14592 sun1831 pvr13582 mx13622
2 k8 P  v27936 e614K k614K ao14535 sun1850 pvr16638 mx16704
2 k8 C  v13968 e341K k341K ao10227 sun1852 pvr9900 mx9948
2 k8 UC v13968 e341K k341K ao10242 sun1824 pvr9750 mx9791
2 k4 UC v6984 e205K k205K ao5115 sun1832 pvr6533 mx6593
2 DERIVED a direction (k8 U - k0)/8: SH-4 1824 us, PVR 1213 us
2 TA out-of-memory events: param 0 bins 46 isp 0
```

The U565 and C565 lines print at most 10 values; K 8 has 15 and 14 (the `c` figure), so its last ones are not on the page.

## 4. ao0 test 1: the union (PVR-046's neighbour)

The scene: 48 caster triangles (a house, a lean-to half inside it, a post, a mover), no sun, K directions into a 256 × 256 RGB565 render target, read back and compared texel by texel with a model. `U`: depth test "greater" with depth write on, translucent list in send order. `P`: plain blending, both sides. `C`: plain blending, one side.

| ID | Claim | Status | Photo |
|---|---|---|---|
| AO0-01 | The target is not read back upside down: f 0 at every K | MEASURED | 24 |
| AO0-02 | **U does not give one darkening a direction.** Values against the model's levels: 3 / 2, 5 / 3, 8 / 5, 15 / 8 at K 1, 2, 4, 8. Texels whose value is not their level's: 477 of 63,832 (0.75 %), 1,184 of 63,237 (1.9 %), 1,310 of 62,405 (2.1 %), 2,541 of 61,279 (4.1 %) | MEASURED | 24 |
| AO0-03 | **U is not plain blending either.** At K 1 its values are ffff, 7bef, 39e7: white, one darkening (half), two darkenings (a quarter). A closed caster drawn by plain blending on both sides has no once-darkened texel at all; here all but 477 texels read right, so nearly every shadow texel was darkened once | MEASURED (the values); DERIVED (the reading) | 24 |
| AO0-04 | P and C give the same count of wrong texels at every K: 460, 870, 970, 1,764, and the same number of values (3, 5, 8, 14). That is what "P darkens exactly twice where C darkens once" gives, since the reading goes by rank | MEASURED; the reading DERIVED | 24 |
| AO0-05 | **C (one side) draws one darkening a caster.** Its values at K 1 are ffff, 7bef, 39e7, and its wrong texels (460 at K 1) are the count the host model has for texels under two casters at once | MEASURED; the host figure from `ao0_model_test` | 24 |
| AO0-06 | U and C hold the same values at K 1 and K 2, and at K 4 but for the darkest (U 4a49, C 39e7: U's darkest is 7 darkenings, C's 8). U has somewhat more wrong texels than C: 477 / 460, 1,184 / 870, 1,310 / 970, 2,541 / 1,764 | MEASURED | 24 |
| AO0-07 | **The blend runs at 8 bits a channel and the 565 value is cut from the result once.** K 1: 255 → 127 = 7bef → 63 = 39e7. K 2 (alpha 75): 255 → 180 = b5b6 → 127 = 7bef → 90 = 5acb → 63 = 39e7. Each value is the top 5 / 6 / 5 bits of the 8-bit result; nothing bands by the pass, nothing is dithered (dithering was switched off) | MEASURED (the values); DERIVED (the rule, from two K) | 24 |

**What U does, as far as the page shows (DERIVED).** U behaves like C: about one darkening a caster, two where two casters' shadows overlap, plus a few hundred texels more. So the depth test with write did reject most second hits inside one caster's shadow, and did not reject the second caster. Why is OPEN. PVR-046 says equal depths are undecided on this chip; this is the same question in a render-to-texture translucent pass, and the answer here is "not reliable".

**The test that would say why.** ao0 with one more line a K: of the wrong texels, how many lie in the model's two-caster overlap. If nearly all do, the union fails between casters (objects sent later); if they are spread, it fails on single texels.

**What it decides.**
- `psm_ao2_unite = 1` buys nothing over one side, and costs twice the vertices. The town should draw **plain blending, one side** (`psm_ao2_unite = 0`, `psm_ao2_cull = 1` or -1) once its casters are known to be closed and wound one way. Junior's ao1 on this disc ran U, both sides, K 4: its AO 2 picture is close to what was meant (AO0-03), at twice the vertices.
- Overlapping casters darken twice a direction either way. For AO that is tolerable: where shadows pile up it gets darker.
- PVR-046 gets a neighbour row from AO0-02 and AO0-03, not a closure.

## 5. ao0 test 2: the cost

1,164 caster triangles (the town has about 1,162), the sun's shadow and the white border as route A draws them, then K directions. 8 frames a row. `ao`: the SH-4's time to transform and send the directions; `sun`: the same for the sun's casters. `pvr`: KallistiOS's render time, mean and largest. `e`: TA bytes estimated at 60 a triangle; `k`: KallistiOS's vertex buffer figure.

| Row | Vertices | TA KB est / KOS | SH-4 ao us | SH-4 sun us | PVR us mean / max |
|---|---|---|---|---|---|
| K 0 | 0 | 68 / 68 | 0 | 1,822 | 3,877 / 3,970 |
| K 1 U | 3,492 | 136 / 136 | 1,824 | 1,850 | 4,630 / 4,752 |
| K 2 U | 6,984 | 205 / 205 | 3,633 | 1,855 | 5,654 / 5,702 |
| K 4 U | 13,968 | 341 / 341 | 7,300 | 1,848 | 8,298 / 8,325 |
| K 8 U | 27,936 | 614 / 614 | 14,592 | 1,831 | 13,582 / 13,622 |
| K 8 P | 27,936 | 614 / 614 | 14,535 | 1,850 | 16,638 / 16,704 |
| K 8 C | 13,968 | 341 / 341 | 10,227 | 1,852 | 9,900 / 9,948 |
| K 8 UC | 13,968 | 341 / 341 | 10,242 | 1,824 | 9,750 / 9,791 |
| K 4 UC | 6,984 | 205 / 205 | 5,115 | 1,832 | 6,533 / 6,593 |

All rows MEASURED 2026-11-25, one console, photos 24 and 25.

| ID | Claim | Status |
|---|---|---|
| AO0-08 | **A direction, both sides, union: SH-4 1,824 us, PVR 1,213 us** (the page's own line, (K 8 U − K 0) / 8). The SH-4 figure is the same at K 1 (1,824), K 2 (1,817 each), K 4 (1,825 each) | MEASURED |
| AO0-09 | A direction is 3,492 vertices and 68 KB of TA. **KallistiOS's figure equals the estimate at every row**: 60 bytes a separate triangle is right | MEASURED |
| AO0-10 | The sun's casters, the same 1,164 triangles in the opaque list: 1,822 to 1,855 us on the SH-4. So a sky direction costs the SH-4 what the sun's shadow costs | MEASURED |
| AO0-11 | The SH-4 is 522 ns a vertex sent (1,824 us / 3,492), transform and reject tests included: 776 vertices transformed, 1,164 triangles tested, 3,492 vertices sent in store-queue runs of 256 | DERIVED |
| AO0-12 | **One side: 1,278 us SH-4 and 753 us PVR a direction** ((10,227 − 0) / 8 and (9,900 − 3,877) / 8), 1,746 vertices, 34 KB. That is 70 % of the SH-4 time and 62 % of the PVR time of both sides, for half the vertices | DERIVED from K 8 C |
| AO0-13 | Plain blending costs the PVR more than the union on the same vertices: 16,638 against 13,582 us at K 8, 1,595 against 1,213 us a direction. The depth test with write lets the PVR skip shading | MEASURED; the reason DERIVED |
| AO0-14 | With one side, the union no longer saves PVR time: 9,750 (UC) against 9,900 (C) at K 8 | MEASURED |
| AO0-15 | The map pass with no direction: PVR 3,877 us for the sun's 1,164 triangles (c4's map pass with 68 vertices: 3,291 us, run 2 photo 6). 1,164 opaque triangles add about 0.6 ms to a 256 × 256 target | MEASURED; the difference DERIVED across two programs |
| AO0-16 | TA out-of-memory events over the whole run (the counters start before test 1): parameters 0, tile bins 46, ISP 0. The page does not say in which rows. The run drew 93 map passes: 12 in test 1, 81 in test 2 | MEASURED; rows OPEN |

**Against the estimates in `AO_METHOD.md`.** SH-4 1.7 ms a direction estimated, 1.82 measured. PVR 1 to 2 ms estimated, 1.21 measured. TA 68 KB (the corrected figure), 68 measured.

**What it means for the town (DERIVED; the town itself is Junior's to measure).**

| K, way | SH-4 added | PVR added | TA added, in the map pass's buffer |
|---|---|---|---|
| K 4, both sides, union (what ao1 ran) | 7.3 ms | 4.4 ms | 273 KB |
| K 4, one side | 5.1 ms | 2.7 ms | 137 KB |
| K 2, one side | about 2.6 ms | about 1.5 ms | 68 KB |
| K 8, one side | 10.2 ms | 6.0 ms | 273 KB |

- The SH-4 time is the larger part. One side still transforms every vertex and tests every triangle; it saves only the sending.
- Variant B of the method note (the static town's directions drawn once into an atlas, the movers a frame) is what removes the SH-4 cost. It is now worth building if the owner keeps method 2.
- The tile bins ran out 46 times in a 256 × 256 target (64 tiles). The next ao0 should print the events a row, and the town should not go to K 8 both sides before that is known.

## 6. The console's revision (PVR-123)

| ID | Claim | Status | Photo |
|---|---|---|---|
| REV-01 | ao0 p1, line 3: "HOLLY REV 0011 SB 0B". `suite_rev_text`: REVISION at 0xA05F8004, low 16 bits, 0x0011; SB_SBREV at 0xA05F689C, low 8 bits, 0x0B | MEASURED | 24 |
| REV-02 | ao1 p1, line 2: "one console (HOLLY REV 0011 SB 0B)": a second program, the same two reads, the same values | MEASURED | 27 |
| REV-03 | By PVR-123's table (0x11 on every 2.x; 0x0B for 2.42) the console's Holly is a 2.x core whose system bus reads as 2.42 | DERIVED from PVR-123's table, which is FACT from its source |
| REV-04 | The menu's own revision line | OPEN: no photo of the menu |

## 7. c4g, a second run from the menu (run 2 against run 1)

Run 1 is `SUITE_RUN1_RESULTS_senior.md`. Same binary (C4G.BIN is the same bytes on s1 and s3), same console, same day, about an hour apart.

**Counts.** Every count of tests 0, 1, 3, 4, the sweep, 6 and 9 is the same as run 1's rows C4G-01 to C4G-12, to the pixel. p2 has the same CRC in both runs. (MEASURED)

**Times, run 1 → run 2 (MEASURED both; us unless said):**

| What | Run 1 | Run 2 |
|---|---|---|
| Test 5 cut, bands 1 / 2 / 4 | 3,183 / 1,694 / 897 | 3,173 / 1,682 / 897 |
| Project, asm cycles a vertex | 60.0 / 61.2 / 60.8 | 60.0 / 61.2 / 60.8 |
| Project, C form | 74.7 / 75.1 / 77.2 | 74.7 / 75.2 / 77.2 |
| Send mask | 846 / 466 / 274 | 846 / 467 / 274 |
| PVR map | 3,288 / 3,288 / 3,284 | 3,291 / 3,285 / 3,291 |
| PVR mask | 9,218 / 6,272 / 5,152 | 9,219 / 6,261 / 5,115 |
| PVR screen, with the layer | 3,321, 9,642 | 3,322, 9,634 |
| DMA 128 KB: 1 ms poll / 250 us poll / interrupt | 4,020 / 3,308 / 3,435 | 4,020 / 3,377 / 3,439 |
| DMA 512 KB | 14,065 / 13,489 / 13,733 | 14,066 / 13,264 / 13,725 |
| Test 8, KallistiOS waits: frame min / mean / max | 38,520 / 39,909 / 39,934 | 29,960 / 39,746 / 39,934 |
| back after the interrupt, map / mask / screen (mean) | 4,645 / 2,929 / 9,940 | 4,648 / 2,928 / 9,774 |
| Ready polled: frame mean | 39,918 | 39,917 |
| **8b, ready and done polled: frame min / mean / max** | 22,390 / 23,264 / 26,535 | 22,339 / 23,254 / 26,544 |
| 8b back after the interrupt | 8 / 7 / 7 | 8 / 7 / 7 |
| 8b screen close wait, mean / max | 786 / 4,033 | 777 / 4,014 |
| 8b mean frame by the cycle counter | 23,266 | 23,255 |
| Test 9 PVR mask pass, 1555 / punch-through, 150/26 | 6,135 / 7,230 | 6,137 / 7,176 |
| the same, 100/13 | 6,921 / 7,925 | 6,953 / 7,924 |

| ID | Claim | Status |
|---|---|---|
| C4G2-01 | **Q53's result repeats.** Polled waits: 23,254 us a frame against 23,264 in run 1; the sleeping wait comes back 2.9 / 4.6 / 9.8 ms after the interrupt again; polled, 7 to 8 us | MEASURED, two runs |
| C4G2-02 | The asm projection repeats to the tenth of a cycle: 60.0 / 61.2 / 60.8 in both runs | MEASURED, two runs |
| C4G2-03 | PVR render times repeat within 0.8 %; the SH-4 cut within 0.7 % | MEASURED, two runs |
| C4G2-04 | The spike about every fifth frame in 8b repeats: frames 1 to 8 are 22,470 22,339 24,367 22,445 22,468 22,523 22,510 26,459; the screen pass's close wait is 777 mean, 4,014 max | MEASURED, two runs; cause OPEN |
| C4G2-05 | With KallistiOS's waits the last of the 8 listed frames is 29,960 us (run 1: 39,933). One frame of 60 was three scheduler ticks, not four | MEASURED |

## 8. r3-d, a second run

p1 typed and checked (CRC 5cee9f4f): lines A to D2, "DERIVED NO RULE" and the fade rows a 0 to a 160 are the same as run 1's rows R3-01 to R3-07. p2 has run 1's CRC (57a5cd02). **The punch-through product repeats exactly** (MEASURED, two runs). p4 was not photographed.

## 9. ao1 p1 (in the range; Junior's to interpret)

Typed and checked (CRC 27e4fda8; the console has a trailing space after "SHED<=0" twice, after "in-ovf 0" once, and at the end of the last line, which goes on with "OVERFLOW" on p2).

| ID | Claim | Photo |
|---|---|---|
| AO1-01 | "TOWN AUTORUN ao1: 1350 frames counted"; "AO2 PVR map: ground only, K 4, union" | 27 |
| AO1-02 | OFF still (150 frames): frame 50.7 / 53.2 / 87.0 ms; SH-4 13.1 / 14.0 / 29.6; parts 2.5 7.7 3.7 0.0; PVR 3.3 / 3.9 / 14.2; TAv 27,746 / 27,802 / 27,903; map TAv 3,215 / 3,220 / 3,242; map KB 63 / 63 / 63; used KB 63 / 683 / 976; est KB 699 / 701; KOS max 975K; SHED<=0; events ISP 0 strip 0 PARAM 0 OPB 150, OVERFLOW | 27 |
| AO1-03 | OFF turn (300 frames): frame 53.4 / 67.6 / 99.4; SH-4 13.2 / 29.8 / 42.0; parts 19.1 7.0 3.7 0.0; PVR 3.3 / 4.0 / 16.0; TAv 24,084 / 26,068 / 28,021; map TAv 2,702 / 2,923 / 3,218; map KB 53 / 57 / 63; used KB 53 / 619 / 996; est KB 674 / 710; KOS max 996K; events PARAM 0 OPB 300 | 27 |
| AO1-04 | The town's map pass with AO off is 3,215 to 3,242 vertices and 63 KB: ao0's sun row is 3,492 vertices and 68 KB. The load scene of ao0 is the town's size within 8 % | DERIVED |

## 10. Open after these photos

- Why the union fails between casters (section 4, with its test).
- Which rows of ao0's test 2 ran the tile bins out.
- The town's casters: closed and wound one way? (needed for one side)
- The menu's revision line, ao0's live view, r3-d p4: not photographed.
- A return to the menu, RUN ALL chaining by itself, a fault page's countdown: still not shown by any photo (no header says "(auto)").
- The every-fifth-frame spike in 8b: repeats, cause not tested.
