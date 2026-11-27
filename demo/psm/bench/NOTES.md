# Chip bench: notes

`bench_h1.c` times parts of the console one number a line. This file holds what a real console printed, and what I
read into it. Readings are marked as readings.

## Hardware result h1

One run on one console, read from photos of the result pages (`logs/benchmarks/PXL_20261125_054414414.jpg` to
`..054608113.jpg`). Every number is from those photos. Nothing was repeated, so there is no spread to give.

### Group 1: SH-4

| Line | Console |
|---|---|
| timer | least step 560 ns; 1 ms = 200,186 cycles |
| cache hit, 1 word a line, 8 KB | 3.17 cycles a line |
| cache miss read, 1 MB | 18.41 cycles a line |
| cache miss write, 1 MB | 41.91 cycles a line |
| allocate + write, 1 MB | 16.24 cycles a line |
| write back 8 KB dirty | 12.91 cycles a line |
| purge 8 KB clean | 3.85 cycles a line |
| memcpy 1 MB RAM to RAM | 24,225 us, 43.2 MB/s |
| store queue 1 MB RAM to RAM | 7,042 us, 148.9 MB/s |
| memcpy 4 KB in the cache | 46.5 MB/s |
| mat_transform, 256 vertices in the cache | 38.70 cycles a vertex |
| plain C 4x4 + divide, the same | 80.93 cycles a vertex |
| mat_transform, 32,768 not in the cache | 42.86 cycles a vertex |
| twiddle 128 x 128 16-bit (32 KB) | bit loop 12,580 us; table 1,504 us; same result |

A line is 32 bytes. MB is a million bytes.

**Corrected by h2.** `bufa` and `bufb` of this image are exactly 1 MB apart (its ELF: `bufb` at 8c14a480, `bufa` at
8c24a480), a multiple of the 16 KB operand cache, so in the three copy lines and the three transform lines above
the source and the destination fought for the same cache entries. h2 ran the same work with the destination 8 KB
off: memcpy of 4 KB in the cache 398.0 MB/s (46.6 on this placing), mat_transform 13.16 cycles a vertex (38.67 on
this placing). The six lines are numbers of this test's placing, not of the routines. The store-queue line is not
touched (a store queue does not write through the cache).

### Group 1: PVR

| Line | Console |
|---|---|
| upload 32 KB, store queue | 224 us, 146.2 MB/s |
| upload 32 KB, channel 2 DMA | 98 us, 335.4 MB/s |
| upload 512 KB, store queue | 2,339 us, 224.1 MB/s |
| upload 512 KB, channel 2 DMA | 1,052 us, 498.2 MB/s |
| empty frame, 640 x 480 | 3,150 us |

Render time added by full 640 x 480 layers, us: the first layer / each further layer.

| Kind | First | Each further |
|---|---|---|
| opaque flat | 1 | 0 |
| opaque 565 twiddled, nearest | 1,115 | -2 |
| opaque 565 twiddled, bilinear | 2,081 | 1 |
| opaque 565 raster, nearest | 1,984 | 0 |
| opaque paletted 8-bit, nearest | 3,034 | 0 |
| opaque 4444 raster, bilinear | 3,120 | 2 |
| translucent flat, blend | 3,112 | 3,112 |
| translucent flat, additive | 3,112 | 3,112 |
| translucent 565 twiddled bilinear, blend | 3,989 | 4,006 |
| translucent 4444 raster bilinear, blend | 6,249 | 6,428 |
| punch-through 4444 raster, nearest | 4,303 | 3,294 |

| Line | Console |
|---|---|
| 1024 x 1024 texture, twiddled: 1 texel a pixel / 16 a pixel / 1/16 a pixel | 1,087 / 23,474 / 3 us |
| the same, raster | 1,522 / 3,339 / 4 us |
| 4,080 opaque triangles: spread / in one tile | 588 / 1,256 us |
| 256 translucent triangles (sorted): spread / in one tile | 82 / 6,385 us |
| TA, pvr_prim a vertex (15,000 vertices) | 3,967 us, 264 ns a vertex, 121.0 MB/s |
| TA, direct a vertex | 3,879 us, 258 ns a vertex, 123.7 MB/s |
| TA, one store-queue run | 2,165 us, 144 ns a vertex, 221.6 MB/s |

### Group 2: DMA and the bus

| Line | Console |
|---|---|
| PVR-DMA download 32 KB / 128 KB / 512 KB | 927 / 3,676 / 14,775 us (35.4 MB/s at 512 KB) |
| DMAOR | 8201 |
| DMAC channel 1, burst, 512 KB RAM to RAM | 4,339 us, 120.8 MB/s, copy right |
| DMAC channel 1, cycle steal | 5,445 us, 96.2 MB/s, copy right |
| DMAC channel 3, burst | 4,328 us, 121.1 MB/s, copy right |
| DMAC channel 3, cycle steal | 5,428 us, 96.5 MB/s, copy right |
| SH-4 loop alone | register 23,419 a ms; RAM-miss 6,384 a ms |
| beside PVR-DMA 512 KB, register loop | 22,516 a ms (96 %), took 13,746 us |
| beside PVR-DMA 512 KB, RAM-miss loop | 5,869 a ms (91 %), took 9,595 us |
| beside DMAC channel 1 burst, register loop | 22,695 a ms (96 %), took 11 us |
| beside DMAC channel 1 burst, RAM-miss loop | 6,438 a ms (100 %), took 40 us |
| beside DMAC channel 1 cycle steal, register loop | 21,240 a ms (90 %), took 4,182 us |
| beside DMAC channel 1 cycle steal, RAM-miss loop | 2,931 a ms (45 %), took 5,415 us |
| beside a PVR render of 8 layers, register loop | 23,297 a ms (99 %), took 15,010 us |
| beside a PVR render of 8 layers, RAM-miss loop | 6,433 a ms (100 %), took 15,002 us |
| YUV converter, 256 blocks (96 KB in) | 519 us, 189.3 MB/s in; 256 blocks counted; 32,768 of 32,768 words written; first words 80c08040 80c08040 |

### Group 3: the operand cache as RAM

| Line | Console |
|---|---|
| 20,000 random reads, before the switch | 8 KB of RAM in the cache 1,446 us; 1 MB of RAM 3,968 us |
| CCR | 00000105 before, 00000125 after |
| OC-RAM at 0x7c001000 | 8,192 of 8,192 bytes hold |
| 20,000 random reads, after | OC-RAM 1,401 us; the same 8 KB of RAM 2,299 us |
| 0x7c000000 / 0x7c003000 | a5 / 5a: the area repeats |

### Readings

These are what I take from the numbers. Where a reading rests on more than the number itself it says so.

SH-4
- A miss on a read costs about 15 cycles a line over a hit (18.41 - 3.17). A write that misses costs 41.91; the
  same write after the allocate instruction costs 16.24. Allocate saves about 26 cycles a line where the whole line
  will be written.
- WRONG, withdrawn (h2): "memcpy is slow, and not because of memory; it is a byte-at-a-time loop". It was because
  of memory: the test's own placing (see the correction under the table). On a clean placing memcpy moves 4 KB in
  the cache at 398.0 MB/s and 1 MB at 63.4 MB/s, the rate of a 32-bit loop, held there by a read miss and a write
  miss a line; shz_memcpy does the megabyte at 131.6 MB/s and the store queue at 149.7.
- WRONG, withdrawn (h2): "mat_transform costs 38.7 cycles a vertex" and the ratio to plain C drawn from it. In the
  cache, on a clean placing, mat_transform is 13.16 cycles a vertex. The 80.9 of plain C was taken on the same bad
  placing and has not been taken again.
- Town a3's SH-4 frame is 157.5 cycles a vertex (lead's figures). The bare transform is under a tenth of that.
- The table twiddle is 1,504 us for 32 KB; the bit loop is 8.4 times slower, measured.

PVR
- Uploads are faster than Sega's store-queue figure (84 MB/s): 146 to 224 MB/s here. Channel 2 DMA reached
  335 to 498 MB/s against Sega's 702.6; the 32 KB figure holds the set-up.
- **3,150 us is one screen of pixels.** The empty frame, the opaque flat layer (1 us more) and every translucent
  flat layer (3,112 us) say the same: about 98 million pixels a second, and the background is a layer like any
  other. Opaque layers that are hidden cost nothing that shows (0 +- 2 us each).
- A render target pass in this KallistiOS set-up costs the same floor whatever its size (c1: 3,213 us at
  320 x 240), because it goes through the screen's tiles. Reading; a pass that ends its tile list early was not
  tried.
- Blend mode does not matter (blend and additive both 3,112).
- Texture cost on an opaque screen: 565 twiddled nearest 1,115, bilinear 2,081; raster nearest 1,984; paletted
  3,034. On a translucent layer the 565 bilinear texture added only 877 (3,989 - 3,112) and the 4444 raster one
  3,137. I cannot say why the same 565 texture costs 2,081 in one list and 877 in the other.
- The two 4444 lines also differ from the 565 ones in being raster, not twiddled. The format and the layout are
  confounded in this test.
- **Texture locality is the largest effect in the table.** A twiddled texture shrunk 16 times with no mip levels
  cost 23,474 us a screen, 21 times its cost at one texel a pixel; raster, 3,339 us. Magnified 16 times it cost
  3 us. A shadow map laid over the screen is magnified.
- Depth in one tile: 4,080 opaque triangles cost 588 us spread and 1,256 us in one tile. 256 sorted translucent
  triangles in one tile cost 6,385 us: 25 us a layer for a 32 x 32 tile, about 41 million pixels a second with the
  sort.
- The TA takes 144 ns a vertex from one store-queue run of prepared vertices and about 260 ns a vertex one at a
  time. 28,699 vertices: 4.1 ms and 7.5 ms.

DMA and the bus
- **The burst lines of the "beside" test measured nothing.** The loop's clock starts after the copy is started.
  "took 11 us" and "took 40 us" are one batch of the loop (256 steps), after which the copy was already over. A
  512 KB burst takes 4.3 ms when timed from before its start (the DMAC lines). So the SH-4 did not run while the
  burst ran: it came back from starting the copy when the copy was done. The 96 % and 100 % are of nothing.
  Reading from two tests; a test with its clock before the start is needed to state it as measured. MEASURED in
  h2: the call that starts a 512 KB burst came back after 4,325 us and the copy was over 11 us later.
- **Cycle steal runs beside the SH-4.** Register-only work kept 90 % (84 % in h2), work that misses the cache kept
  45 %, and the copy ran at 96 to 125 MB/s. ("2.2 to 2.9 times memcpy's rate" stood here; memcpy's rate was the
  bad placing's. Against h2's 63.4 MB/s it is 1.5 to 2.0 times.)
- WRONG, withdrawn (h2): "the PVR-DMA figure depends on how often its status is read; the real rate is 55 MB/s or
  more". h2 ended the same 512 KB by its interrupt, with no status read at all: 13,742 us, 38.1 MB/s, the same as
  the 13,746 us here beside the register loop. Reading the status all the time costs 7 % (14,775). The one quick
  run, 9,595 us, was the one where the SH-4 was reading RAM with a cache miss a read while the DMA ran. Why that
  should speed a PVR-DMA up is not known, and it has been seen once.
- The same holds in part for the DMAC cycle steal: 5,445 us polled all the time, 4,182 us beside the register
  loop.
- A PVR render does not slow the SH-4 (99 %, 100 %).
- The YUV converter turned 96 KB of planar 4:2:0 into 128 KB of texture in 519 us. The words read 80c08040: in
  memory order U, Y, V, Y. It is a converter from separate bytes to 16-bit texels, driven by store queues, and it
  works as KallistiOS's example uses it.

OC-RAM
- It holds 8 KB and is no faster than a cache hit (1,401 against 1,446 us). What it gives is that nothing can push
  the data out.
- It costs the rest: the same 8 KB of ordinary RAM took 2,299 us with OC-RAM on (59 % more), because the cache for
  RAM is then 8 KB and those 8 KB no longer fit in it together.
- The area repeats every 8 KB, with entries 128 to 255 at 0x7c000000 and 0x7c001000 and entries 384 to 511 at
  0x7c002000 and 0x7c003000, as the SH7750 manual has it.

### Not measured

- The TA's own DMA vertex mode. Translucent sorting off against on.
- AICA: the ARM7, the DSP. (Sound RAM and G2 DMA: h2.)
- Any second console, any second run.

## Hardware result h2

`bench_h2.c`, built by `python demo/psm/bench/build.py --src bench_h2.c --tag h2`. One run on one console, read
from photos of the result pages (`logs/benchmarks/psm-bench-h2/PXL_20261125_072052571.jpg` to `..072218093.jpg`,
14 photos). Every number is from those photos: MEASURED, once. Nothing was repeated, so there is no spread to
give. All four groups ran to the end; nothing hung and no wait reached its limit.

Where a test has a source and a destination in RAM, "8 KB off" means the destination is half the operand cache
from the source at equal offsets (no cache entry shared) and "1 MB off" means exactly 1 MB (every entry shared:
h1's placing).

### Ledger

One line a fact, in the form of `wiki/STRUCTURE.md`. The photos, all in `logs/benchmarks/psm-bench-h2/`, by the
page they show: p1 `PXL_20261125_072052571`, p2 `..072055579`, p3 `..072059336`, p4 `..072106285`, p5 `..072109498`,
p6 `..072117232`, p7 `..072120744`, p8 `..072123111` (group 1); p9 `..072146021`, p10 `..072149994`, p11 `..072157611`
(group 2); p12 `..072202636`, p13 `..072210079`, p14 `..072218093` (groups 3 and 4). The test is the line of
`bench_h2.c`'s result pages that prints the number; for an ESTIMATE or an OPEN row it is the test that would
settle it. No row mixes a measured number with an estimated one.

| Claim | Status | Photo | Test |
|---|---|---|---|
| The KOS ns timer steps by 560 ns at least; 1 ms is 200,176 CPU cycles. | MEASURED 2026-11-25, 1 console, 1 run | p1 | h2 group 1, timer line |
| A register-only loop runs 23,449 turns a ms with nothing beside it (the base of every "loop" percentage). | MEASURED 2026-11-25, 1 console, 1 run | p1 | h2 group 1, register loop line |
| FIPR and FTRV gave the same bits for all 16,384 four-term sums. | MEASURED 2026-11-25, 1 console, 1 run | p2 | h2 test 1, "FTRV against FIPR" |
| Against the exact sum, FIPR and FTRV missed the nearest float 2,421 times of 16,384 (worst 1.24 units); the plain sum 6,511 times (worst 1.71 units). A unit is 2^-24 of the sum of the four products' sizes. | MEASURED 2026-11-25, 1 console, 1 run | p1, p2 | h2 test 1, the three count lines |
| A 4-term row for 256 points in the cache costs 72.43 cycles a point as a plain sum, 38.10 by FIPR, 16.58 by FTRV with the matrix already in XMTRX. | MEASURED 2026-11-25, 1 console, 1 run | p2 | h2 test 1, cycles line |
| Loading XMTRX costs 15.01 cycles by `shz_xmtrx_load_4x4`, 18.05 by `mat_load`. | MEASURED 2026-11-25, 1 console, 1 run | p2 | h2 test 2, load line |
| `mat_transform`, 256 vectors in the cache, out 8 KB off: 13.16 cycles a vertex. A C loop of `shz_xmtrx_transform_vec4` and `shz_invf_fsrra` on the same job: 28.67. | MEASURED 2026-11-25, 1 console, 1 run | p2 | h2 test 2, "out 8 KB from in" |
| The same two with out exactly 1 MB off (h1's placing): 38.67 and 77.07 cycles a vertex. | MEASURED 2026-11-25, 1 console, 1 run | p2 | h2 test 2, "out 1 MB from in" |
| The two routines' results differ in 340 of 768 numbers, by 2 ulp at most. | MEASURED 2026-11-25, 1 console, 1 run | p2 | h2 test 2, "the two results" |
| 16,384 vectors not in the cache: `mat_transform` 43.77 cycles a vertex, the SH4ZAM C loop 57.30. | MEASURED 2026-11-25, 1 console, 1 run | p2 | h2 test 2, "not in the cache" |
| `shz_dce_pvr_project`, 128 PVR vertices in the cache: 21.15 cycles a vertex; its C form 43.19; the bytes the same. 16,384 not in the cache: 58.09 and 74.07. | MEASURED 2026-11-25, 1 console, 1 run | p2, p3 | h2 test 2, PVR vertex lines |
| 1 MB RAM to RAM, destination 8 KB off: memcpy 63.4 MB/s, a 32-bit loop 63.4, shz_memcpy 131.6, shz_memcpy8 86.9, shz_memcpy32 130.6, shz_memcpy64 130.3, shz_memcpy128 130.3, sq_cpy 149.7, shz_sq_memcpy32 150.1. | MEASURED 2026-11-25, 1 console, 1 run | p3 | h2 test 3, first table |
| The same with the destination exactly 1 MB off: memcpy 12.8 MB/s, a 32-bit loop 13.2, shz_memcpy32 130.9. | MEASURED 2026-11-25, 1 console, 1 run | p3, p4 | h2 test 3, "dst exactly 1 MB" |
| 4 KB in the cache: memcpy 398.0 MB/s 8 KB off and 46.6 at 1 MB off; shz_memcpy 589.3 and 129.8; shz_memcpy8 552.6 and 92.9. | MEASURED 2026-11-25, 1 console, 1 run | p4 | h2 test 3, 4 KB lines |
| 15,000 vertices into the TA: 260 ns a vertex by `pvr_prim`, 142 ns in one store-queue run, by `pvr_sq_load` and by `shz_sq_memcpy32` alike. | MEASURED 2026-11-25, 1 console, 1 run | p4 | h2 test 4 |
| Texture upload: 512 KB at 216.4 MB/s by `pvr_txr_load`, 218.1 by `shz_sq_memcpy32`; 32 KB at 145.5 and 110.1. All read back right. | MEASURED 2026-11-25, 1 console, 1 run | p4, p5 | h2 test 5 |
| Video memory read from its uncached address: a 32-bit loop 7.5 MB/s, memcpy 7.2 to 7.5, shz_memcpy32 7.0 to 7.1, shz_memcpy8 6.7 to 6.8, a 16-bit loop 3.7; the same at 32, 128 and 512 KB. | MEASURED 2026-11-25, 1 console, 1 run | p5, p6 | h2 test 6, reader rows |
| PVR-DMA from video memory, ended by its interrupt (event 0x000b), timed from before its start: 37.6, 37.8, 38.1 MB/s at 32, 128, 512 KB. | MEASURED 2026-11-25, 1 console, 1 run | p5, p6 | h2 test 6, PVR-DMA rows |
| The warp mesh (2,460 vertices) takes 914 ns a vertex sent by `pvr_prim` and 973 ns sent a strip a store-queue run: the run form is 6 % slower. | MEASURED 2026-11-25, 1 console, 1 run | p7 | h2 test 7, mesh lines |
| The warp pixel pass (1,200 vertices) takes 366 ns a vertex by `pvr_prim` and 241 ns by runs. | MEASURED 2026-11-25, 1 console, 1 run | p7 | h2 test 7, pixel pass lines |
| The mesh's maths alone is 632 us of its 2,250 us. | MEASURED 2026-11-25, 1 console, 1 run | p7 | h2 test 7, last line |
| A DMAC burst of 512 KB (channel 1, RAM to RAM) holds the SH-4: the call that starts it came back after 4,325 us and the copy was over 11 us later. | MEASURED 2026-11-25, 1 console, 1 run | p7, p8 | h2 test 8, burst lines |
| The same in cycle steal: the call came back after 537 us, the copy ended 4,236 us later, the register loop kept 84 % of its rate. | MEASURED 2026-11-25, 1 console, 1 run | p8 | h2 test 8, cycle steal lines |
| Video memory read through the operand cache (0x84...): shz_memcpy32 38.6 to 38.7 MB/s, a 32-bit loop 29.6 to 30.3, the loop with a prefetch 30.2 to 30.9; all read back right. | MEASURED 2026-11-25, 1 console, 1 run | p9, p10 | h2 group 2, reader rows |
| `dcache_inval_range` over 512 KB takes 492 us. | MEASURED 2026-11-25, 1 console, 1 run | p10 | h2 group 2, invalidate line |
| 8 KB of video memory read through the cache, rewritten by the store queue and read again: 238 of 256 lines came back old. After `dcache_inval_range`: none. | MEASURED 2026-11-25, 1 console, 1 run | p10 | h2 group 2, stale test |
| 32 KB to sound RAM by `spu_memload_sq`: 7.5 MB/s. 32 KB from sound RAM by `spu_memread`: 2.5 MB/s. | MEASURED 2026-11-25, 1 console, 1 run | p12 | h2 group 3, first two lines |
| G2 DMA (channel 0) RAM to sound RAM: 5.7 MB/s at 4, 32, 128 and 512 KB; the register loop kept 99 to 100 %; both ends of each copy right. | MEASURED 2026-11-25, 1 console, 1 run | p12, p13 | h2 group 3, "to sound RAM by DMA" |
| G2 DMA sound RAM to RAM: 4.9 to 5.0 MB/s at the same sizes; the loop kept 98 to 99 %; every copy right. | MEASURED 2026-11-25, 1 console, 1 run | p12, p13 | h2 group 3, "to RAM by DMA" |
| One G2 DMA moved 32 KB from sound RAM straight into texture memory in 6,511 us; all 32,768 bytes read back as the sound RAM's. | MEASURED 2026-11-25, 1 console, 1 run | p14 | h2 group 4 |
| h1's `bufb` is at 8c14a480 and its `bufa` at 8c24a480: exactly 1 MB apart, so every operand-cache entry is shared. | FACT: the symbol table of `build/psm_bench/psm-bench-h1.elf` (`sh-elf-nm`) | - | h2 tests 2 and 3, the "1 MB" lines, reproduce h1's 38.70 and 46.5 |
| A cached read of 512 KB with its invalidate first is 14,028 us, 2 % over PVR-DMA's 13,742. | ESTIMATE: the sum of two measured lines (13,536 + 492), never timed as one | p10 | a line that times invalidate + read together |
| shz_memcpy32 does not care about the placing because it holds a whole line in registers and allocates the destination line without reading it. | ESTIMATE: a reading of its source and of the two rates (130.6, 130.9) | p3, p4 | - |
| Why h1 copied 1 MB by memcpy at 43.2 MB/s when h2 gives 63.4 (8 KB off) and 12.8 (1 MB off). | OPEN | h1 photos; p3 | h2's copy with the destination 1 MB BELOW the source, both on a multiple of 128 |
| Why the mesh is slower by store-queue runs when the pixel pass and the bare TA test are quicker. | OPEN | p7 | the fill and the send timed apart, and the run's size varied |
| Why h1's one PVR-DMA beside a cache-missing loop took 9,595 us when every other 512 KB run takes 13,742 to 14,775. | OPEN | h1 photos | h1's "beside" test again with the DMA ended by interrupt, 8 times |
| Whether `pvr_txr_load` is quicker than `shz_sq_memcpy32` at 32 KB (225 against 297 us, one sample each). | OPEN | p4 | the 32 KB upload 16 times each, least and mean |
| Plain C against `mat_transform` on a clean placing (h1's 80.93 was on the bad one). | OPEN | - | h2 test 2 with a plain C row |

### Group 1

| Line | Console |
|---|---|
| timer | least step 560 ns; 1 ms = 200,176 cycles |
| SH-4 register loop alone | 23,449 a ms |

1 Rounding: 4,096 points x 4 sums, (x, y, z, w) x a 4 x 4, against the exact sum taken in double on the console.
A unit is 2^-24 of the sum of the four products' sizes.

| Way | Not the nearest float, of 16,384 | Largest distance | Largest error | Cycles a point, 256 in the cache |
|---|---|---|---|---|
| plain sum (what `sh4der_psm.h` was) | 6,511 | 20,368 ulp | 1.71 units | 72.43 |
| FIPR, `shz_mat4x4_transform_vec4` (what it is now) | 2,421 | 3,984 ulp | 1.24 units | 38.10 |
| FTRV, `shz_xmtrx_transform_vec4` | 2,421 | 3,984 ulp | 1.24 units | 16.58 |

FIPR against the plain sum: 6,043 of 16,384 differ, by 16,384 ulp at most. FTRV against FIPR: 0 differ.

2 The transform.

| Line | Console |
|---|---|
| load XMTRX | `mat_load` 18.05 cycles, `shz_xmtrx_load_4x4` 15.01 |
| 256 vectors in the cache, out 8 KB off | `mat_transform` 13.16 cycles a vertex; the same work on SH4ZAM in a C loop (FTRV, 1 / w by FSRRA) 28.67 |
| the same, out 1 MB off | 38.67; 77.07 |
| the two results | 340 of 768 numbers differ, by 2 ulp at most |
| 16,384 vectors not in the cache | `mat_transform` 43.77; the SH4ZAM loop 57.30 |
| 128 PVR vertices in the cache | `shz_dce_pvr_project` 21.15 cycles a vertex; its C form 43.19; the bytes the same |
| 16,384 PVR vertices not in the cache | 58.09; C form 74.07 |

3 Copies, RAM to RAM. 1 MB, the destination out of the cache before each, every copy checked ("right" on all).

| Routine | 8 KB off | 1 MB off |
|---|---|---|
| memcpy | 16,531 us, 63.4 MB/s | 81,789 us, 12.8 MB/s |
| shz_memcpy | 7,965 us, 131.6 MB/s | |
| shz_memcpy8 | 12,056 us, 86.9 MB/s | |
| shz_memcpy32 | 8,026 us, 130.6 MB/s | 8,009 us, 130.9 MB/s |
| shz_memcpy64 | 8,043 us, 130.3 MB/s | |
| shz_memcpy128 | 8,044 us, 130.3 MB/s | |
| a 32-bit loop | 16,520 us, 63.4 MB/s | 79,356 us, 13.2 MB/s |
| sq_cpy | 7,001 us, 149.7 MB/s | |
| shz_sq_memcpy32 | 6,982 us, 150.1 MB/s | |

4 KB that stays in the cache, 256 copies:

| Routine | 8 KB off | 1 MB off |
|---|---|---|
| memcpy | 398.0 MB/s | 46.6 MB/s |
| shz_memcpy | 589.3 MB/s | 129.8 MB/s |
| shz_memcpy8 | 552.6 MB/s | 92.9 MB/s |

4 Vertices into the TA, 15,000 of them, the sending alone.

| Way | Console |
|---|---|
| `pvr_prim` a vertex | 3,906 us, 260 ns a vertex, 122.8 MB/s |
| `pvr_sq_load`, one run | 2,145 us, 142 ns a vertex, 223.8 MB/s |
| `shz_sq_memcpy32` through a locked store queue, one run | 2,144 us, 142 ns a vertex, 223.9 MB/s |

5 Texture uploads, each read back right.

| Size | `pvr_txr_load` | `shz_sq_memcpy32` |
|---|---|---|
| 32 KB | 225 us, 145.5 MB/s | 297 us, 110.1 MB/s |
| 512 KB | 2,422 us, 216.4 MB/s | 2,403 us, 218.1 MB/s |

6 Video memory to RAM, the block at a437a920 (uncached address). Every row came back right.

| Reader | 32 KB | 128 KB | 512 KB |
|---|---|---|---|
| memcpy | 4,320 us, 7.5 MB/s | 17,343 us, 7.5 MB/s | 71,860 us, 7.2 MB/s |
| a 16-bit loop | 8,624 us, 3.7 MB/s | 34,613 us, 3.7 MB/s | 138,391 us, 3.7 MB/s |
| a 32-bit loop | 4,319 us, 7.5 MB/s | 17,357 us, 7.5 MB/s | 69,357 us, 7.5 MB/s |
| shz_memcpy8 (64-bit moves) | 4,813 us, 6.8 MB/s | 19,391 us, 6.7 MB/s | 77,728 us, 6.7 MB/s |
| shz_memcpy32 | 4,644 us, 7.0 MB/s | 18,433 us, 7.1 MB/s | 73,735 us, 7.1 MB/s |
| PVR-DMA, ended by its interrupt, timed from before its start | 869 us, 37.6 MB/s | 3,461 us, 37.8 MB/s | 13,742 us, 38.1 MB/s |
| quickest SH-4 read / PVR-DMA (the page's own line) | 4.96 | 5.01 | 5.04 |

7 The warp (`src/sh4der_warp.c` compiled both ways into the one program), the draw call alone, least / mean of 8
frames, the two ways in turn.

| Pass | A `pvr_prim` a vertex | A strip a store-queue run | PVR render |
|---|---|---|---|
| mesh 40 x 30, 2,460 vertices | 2,250 / 2,275 us, 914 ns a vertex | 2,394 / 2,442 us, 973 ns a vertex | 6,466 / 6,503 us |
| pixel pass, 1,200 vertices | 440 / 440 us, 366 ns a vertex | 290 / 293 us, 241 ns a vertex | 3,184 / 3,183 us |

The mesh's maths alone (31 rows of `sh4der_warp_row`): 632 us.

8 The DMAC, channel 1, 512 KB RAM to RAM, the clock started before the start call.

| Mode | The start call came back after | The copy was done after that | Register loop in that second time | Copy |
|---|---|---|---|---|
| burst | 4,325 us | 11 us | 23,357 a ms (99 % of alone) | right |
| cycle steal | 537 us | 4,236 us | 19,884 a ms (84 % of alone) | right |

### Group 2: video memory through the operand cache

The same block at its cached address, 8437a920. The first line read through the cache was right. Every row came
back right. The times do not hold the invalidate that a reader must do first.

| Reader | 32 KB | 128 KB | 512 KB |
|---|---|---|---|
| a 32-bit loop | 1,079 us, 30.3 MB/s | 4,419 us, 29.6 MB/s | 17,489 us, 29.9 MB/s |
| the loop with a prefetch two lines ahead | 1,059 us, 30.9 MB/s | 4,331 us, 30.2 MB/s | 17,315 us, 30.2 MB/s |
| shz_memcpy32 | 846 us, 38.7 MB/s | 3,389 us, 38.6 MB/s | 13,536 us, 38.7 MB/s |
| quickest SH-4 read / PVR-DMA (the page's own line) | 0.97 | 0.97 | 0.98 |

| Line | Console |
|---|---|
| `dcache_inval_range` over the 512 KB | 492 us |
| 8 KB read, rewritten behind the cache by the store queue, read again | 238 of 256 lines old, 18 new |
| after `dcache_inval_range` | 0 old, 256 new |

### Group 3: G2 DMA, channel 0, RAM and sound RAM

| Line | Console |
|---|---|
| 32 KB to sound RAM, `spu_memload_sq` | 4,355 us, 7.5 MB/s |
| 32 KB from sound RAM, `spu_memread` (register reads) | 12,616 us, 2.5 MB/s, right |

By DMA, timed from before the start to the interrupt, with the register loop's rate beside it:

| Size | To sound RAM | Loop | Both ends | To RAM | Loop | Copy |
|---|---|---|---|---|---|---|
| 4 KB | 714 us, 5.7 MB/s | 23,300 a ms (99 %) | right | 821 us, 4.9 MB/s | 23,346 a ms (99 %) | right |
| 32 KB | 5,681 us, 5.7 MB/s | 23,527 a ms (100 %) | right | 6,510 us, 5.0 MB/s | 23,184 a ms (98 %) | right |
| 128 KB | 22,715 us, 5.7 MB/s | 23,383 a ms (99 %) | right | 26,025 us, 5.0 MB/s | 23,399 a ms (99 %) | right |
| 512 KB | 90,816 us, 5.7 MB/s | 23,429 a ms (99 %) | right | 104,066 us, 5.0 MB/s | 23,438 a ms (99 %) | right |

### Group 4: sound RAM to texture memory in one G2 DMA

| Line | Console |
|---|---|
| 32 KB sound RAM to texture memory | ended after 6,511 us, 5.0 MB/s |
| the texture's bytes afterwards | 32,768 of 32,768 are the sound RAM's; 93 are also what was there before |

(The 93 are bytes where the two random fills happen to agree; 128 would be the mean.)

### Readings

These are what I take from the numbers. Where a reading rests on more than the number itself it says so.

Rounding
- On this console FIPR and FTRV gave the same bits for all 16,384 sums.
- Both were nearer the exact sum than the plain sum the compiler made: 2,421 results off the nearest float against
  6,511, and a worst error of 1.24 units against 1.71. So the change of `sh4der_psm.h` to FIPR lost no accuracy on
  these points; it gained some. The large ulp figures are points whose terms cancel; the unit is the fair measure.
- FIPR took 38.10 cycles a point against the plain sum's 72.43. FTRV with the matrix already in XMTRX took 16.58:
  where XMTRX is free, a header that loads the matrix once and uses FTRV does the row in under half of FIPR's
  time. `sh4der_psm.h` does not do that today (it leaves XMTRX alone).

The transform
- **h1's transform lines were the placing's.** 13.16 cycles a vertex on a clean placing, 38.67 on h1's; h1 had
  38.70.
- **KallistiOS's `mat_transform` is 2.18 times quicker than a C loop over SH4ZAM's calls** (13.16 against 28.67),
  in the cache. Out of the cache 43.77 against 57.30. The results agree to 2 ulp (a divide against FSRRA). A C loop
  of `shz_xmtrx_transform_vec4` and `shz_invf_fsrra` waits out each vertex's FTRV and FSRRA before it starts the
  next. So a loop written that way breaks the library's own "no slow code" rule wherever `mat_transform` would do.
  A batch routine for `sh4zam-dce` that keeps two vertices in flight is the answer proposed; it has not been on a
  console, so it has no number here.
- `shz_dce_pvr_project`: 21.15 cycles a vertex against 43.19 for its C form, 2.04 times, the bytes the same. The
  model said 19.5 and 39. Out of the cache 58.09 against 74.07: there the misses, not the kernel, set the pace
  (a line read and a line written a vertex).
- Loading XMTRX: 15.01 cycles by SH4ZAM, 18.05 by KallistiOS.

Copies
- **The placing is the largest effect in the table.** memcpy of 4 KB in the cache: 398.0 MB/s 8 KB off, 46.6 MB/s
  1 MB off, 8.5 times. Of 1 MB: 63.4 against 12.8. Code that copies or transforms between two buffers must not
  have them a multiple of 16 KB apart.
- shz_memcpy32 does not care: 130.6 and 130.9 MB/s. It reads a whole line into registers and allocates the
  destination's line without reading it, so the two lines never need to be in the cache together. (Reading from
  its source and the two numbers.)
- On a clean placing memcpy does 1 MB at the rate of a 32-bit loop (63.4 MB/s both): a read miss and a write miss a
  line. shz_memcpy doubles that (131.6); the store queue is 14 % above that again (149.7, 150.1), and KallistiOS's
  and SH4ZAM's store-queue copies are the same speed.
- h1's "memcpy 1 MB 43.2 MB/s" is reproduced by neither placing here (63.4, 12.8). h1's destination was 1 MB below
  its source and both on a multiple of 128 bytes; here it is 1 MB above and on a multiple of 32. Not explained.

The TA and uploads
- A store-queue run takes 142 ns a vertex against 260 by `pvr_prim`, 1.83 times, as in h1 (144 and 264). SH4ZAM's
  store-queue copy and KallistiOS's are the same to a microsecond in 2 ms.
- Uploads: the same at 512 KB. At 32 KB `pvr_txr_load` took 225 us and the SH4ZAM route 297; one sample each, not
  explained.

Video memory to RAM
- **Read through the cache with shz_memcpy32, the SH-4 fetches video memory as fast as PVR-DMA does**: 38.7 MB/s
  against 37.6 to 38.1. With the invalidate that must come first (492 us for 512 KB) the 512 KB read is 14,028 us
  against the DMA's 13,742: the same within 2 %.
- From the uncached address the best the SH-4 does is 7.5 MB/s (a 32-bit loop), a fifth of PVR-DMA; SH4ZAM's
  copies gain nothing there (7.0, 6.7). A 16-bit loop, which is what a frame read by pixels is, does 3.7 MB/s:
  PVR-DMA is 10.1 times that at 512 KB (138,391 / 13,742). That is the size of the old "9.3 times": true of the
  16-bit loop, 5.0 against the best uncached read, 1.0 against the cached one.
- The cached read needs care, and the test shows why: after the block was rewritten behind the cache, 238 of 256
  lines read back old. After `dcache_inval_range` none did. A reader must invalidate the range before every read
  of new contents, and must never write through that address.
- What still tells them apart is who is busy: the cached read holds the SH-4 for the whole 13.5 ms; PVR-DMA left
  the register loop 96 % of its rate in h1.
- PVR-DMA's rate is 38 MB/s whether its end is waited for by interrupt or polled every 11 us (h1). See the
  correction under h1.

The warp
- The pixel pass gained: 290 us against 440, 241 ns a vertex against 366. The 125 ns saved a vertex is the TA
  test's saving (118).
- The mesh did not: 2,394 us by runs against 2,250 by `pvr_prim`, 6 % slower, with the same block size (82
  vertices a call against the pixel pass's 80) and the same code building the block either way. Not explained.
  Until it is, the mesh's change is not a gain on this console and the claim "a strip in one run is quicker" holds
  for the pixel pass only.
- The maths is 632 us of the mesh's 2,250: filling and sending is the larger part.

The DMAC
- **A burst holds the SH-4 for the whole copy**: the call that starts it came back 4,325 us later with the copy all
  but done. MEASURED now; it was a reading in h1. The "99 %" beside it is of the last 11 us and says nothing.
- Cycle steal gave the call back after 537 us and finished 4,236 us later with the register loop at 84 %.

G2
- **G2 DMA runs at 5.7 MB/s to sound RAM and 5.0 MB/s back**, at every size from 4 KB to 512 KB, and leaves the
  SH-4 98 to 100 % of its register loop. Sega's 11.3 and 14.4 MB/s ("simulation optimums") are about twice what
  this console did.
- The DMA toward the SH-4 works with KallistiOS's settings for channel 0 (it was not known to).
- For writing, the store queue (`spu_memload_sq`) is quicker than the DMA, 7.5 against 5.7 MB/s, but the SH-4 does
  it. For reading back, the DMA is twice `spu_memread` (5.0 against 2.5 MB/s) and does not hold the SH-4.
- **Sound RAM to texture memory in one G2 DMA works**: 32 KB in 6,511 us, every byte right, with the texture's
  address given as 0x04000000 + its offset. KallistiOS's note that the protection register admits system memory
  only does not describe what this console did.
- So a 128 x 128 map of 16 bits (32 KB) made in sound RAM costs 6.5 ms to bring to RAM or to a texture; 512 KB
  costs 104 ms. The plan's table (`../OFFLOAD_PLAN.md`, Q2) used Sega's rates and is about twice too hopeful.

### Not measured

- The ARM7 and the DSP (no ARM compiler here; see `../ARM7_TOOLCHAIN_NOTES.md`).
- The TA's own DMA vertex mode. Translucent sorting off against on.
- Plain C against `mat_transform` on a clean placing.
- Why the mesh's store-queue send is slower, why h1's 1 MB memcpy was 43.2 MB/s, why a PVR-DMA ran quicker beside
  a loop that misses the cache.
- Any second console, any second run.

## Ray bench (image ray1): built

Built 2026-11-25, 04:2x local, by Senior. No number from it exists yet.

- Image: `demo/psm/out/cdi/psm-bench-ray1.cdi` (1,745,961 bytes, sha256 fe57f78f456dc9a44f70405b4c79a2ff94aeb7ce80755da2d5e3aa33745a8cdc). ELF `build/psm_bench/psm-bench-r1.elf`, built first and on its own, with no warnings at `-Wall -Wextra`.
- Source: `bench_ray.c`, `bench_ray_np.c` (the projecting batch without its prefetch) and `ray_kernels.h` (the kernels and their C references). Build: `python demo/psm/bench/build.py --src bench_ray.c --tag <new tag>`, with the library's Dreamcast flags (`-O3 -ffast-math`), SH4ZAM master ff39547 and SH4ZAM-DCE.
- Host test first: `ray_test.c` gave 20,052 checks and 0 failed, both strict `-O2` and `-O3 -ffast-math` (x86, SH4ZAM's software back end). It checks:
  - The walk kernel gives the reference's bytes: 262,144 points at 14:00 and at 16:00, and 8,192 points at each of 96 other suns. 0 bytes differ.
  - On a grid of zeros the walk's crossings match THESIS 5.3's n. Mean ratio 1.0001; the worst single ray is 1.80 crossings off.
  - Against one wall, the hit lands on the 11th cell.
  - The proxy kernel (early outs) gives the reference's bytes over 454 receivers at 16:00. 88 are shadowed and 18 are in a penumbra.
  - A point halfway along each person's run is dark, and a point 1.5 m to the side is lit.
- Pages: one test a page. A page's first line is "TEST <id> (what is measured, not a result)"; every result line starts "RESULT <id>"; DERIVED lines are marked.
  - CLOCK: the s2 clock pair.
  - P1.1–P1.4: rays of known n, cycles a ray at n 1 to 96, and a straight-line fit (DERIVED) set beside 5.3's ESTIMATE. P1.1 and P1.2 use a grid of zeros at 14:00 and 16:00. P1.3 and P1.4 use the town grid. On the town grid, lit rays only exist from n 24 up, because they must climb to H = 28 m (host count).
  - P2.A and P2.B: a slice of 1,024 receivers at 14:00 and 16:00. Each page has five rows: batched with the grid cold, batched with the grid warm, 28,699 PVR vertices alone (`shz_dce_pvr_project`), the two interleaved (8 rays, then the vertices' share), and the extra cost (DERIVED).
  - P2.C and P2.D: the same with the grid in OC-RAM. They run as group 2 on START, and OC-RAM stays on until the power goes off, as in h1.
  - P9: the proxy pass at 16:00. 8 capsules and 4 spheres at their **load places** (`town_ray_movers` x, z), each against the 0.5 m ground points in its 16:00 run. The page gives cycles a pair, the reference without early outs, and the pass as a share of P2.B's slice and of a 2 ms slice (DERIVED).
  - h3: `mat_transform`, `shz_dce_xmtrx_project_point3_batch` with and without its prefetch, and the plain SH4ZAM loop, for 256 vectors in the cache and 16,384 not. It also compares the bytes of the batch and the loop.
- Stand-ins (stated on the pages and here):
  - The slice's receivers are random points over the casters' box, not the town's vertices: half on the ground, half at 0 to 13 m. The baked mesh is not loaded.
  - The proxies' sizes are the bench's choice: a person is a capsule of radius 0.20 m; a hen is a sphere of radius 0.15 m.
  - The light's angular radius is the real sun's, 0.00465 rad.
  - The vertex stream is synthetic, 28,699 records.
- Placing: one pool with offsets (grid +0; points +5 KB; bytes +72.5 KB and +81.5 KB; P9 points +163.5 KB; vertices in +256 KB, out +1 MB +8 KB beyond that). No two starts are a multiple of 16 KB apart.
- Not in this image:
  - the two-level walk with block maxima (5.3; its cost is OPEN);
  - the fixed-point major-axis walk (5.3);
  - the box proxy (5.6);
  - the movers at other places on their routes.
- Risk to watch: the kernels' bytes were compared on x86 only. The console prints its own comparison (`RESULT ... kernel against reference`) on every page. If a page says BYTES DIFFER, that page's timings stand, but the kernel is not the reference on the SH-4.

## Ray bench ray1: answers to lead's four questions (Senior, host work)

Senior wrote this on 2026-11-25. Sources:
- `bench_ray.c`;
- KallistiOS's `perfctr.c` and `timer.c` (read only, nothing copied);
- Cortana's and Junior's photo transcriptions (`RAY1_RESULTS_cortana.md`, `RAY1_RESULTS_junior.md`); every MEASURED number quoted below is theirs;
- one new host count.



### 1. CLOCK.2: 1,027,528 cycles in 1,000,320 ns

Status: a bench bug. The cause is found in the source; its size is ESTIMATE.

**The source (FACT, `bench_ray.c:149-153`).**
- The counter's start is read, 1 ms of the ns timer runs out, and `t` is taken.
- Then `say("RESULT CLOCK.1 ...")` runs. Only after it does `cyc()` run, inside the argument list of the CLOCK.2 `say`.
- So the cycles include all of CLOCK.1's `say`: a `vsnprintf`, a `printf` to the debug port, and the copy into the page buffer. The ns figure does not, because `t` was taken before the `say`.

**The size (ESTIMATE).**
- At 200.006 MHz, 1,000,320 ns is 200,070 cycles. That leaves 827,458 cycles, 4.14 ms, for one `say`.
- A `printf` of the 49 characters "ray: RESULT CLOCK.1 ns timer least step: 640 ns\n" to a 115,200-baud serial port takes 49 × 86.8 us = 4.25 ms, if the call waits for every character. That is the right size.
- The exact split is not measured: the port and its FIFO decide it.

**The counter is right; the console is stock (owner): 200 MHz, not 1.03 GHz.**
- In every P1 row but two, the cycles and the ns agree at 5 ns a cycle to 0.01 % (Cortana's list).
- Those rows read both clocks round the same code.

**Fix for the next image.** Read `cyc()` into a variable at the same point as `t`, before any `say`. CLOCK.2 stays OPEN as a bench bug until that image prints about 200,000 cycles in 1 ms.

**CLOCK.1, "least step 640 ns".** This is probably the cost of one `timer_ns_gettime64` call, not the timer's step (ESTIMATE).
- KallistiOS's ns time comes from TMU2 at Pck / 4: 80 ns a count if Pck is 50 MHz. The divider is FACT, from `timer.c`'s `TIMER_TPSC`; Pck was not checked.
- The loop takes the least difference between two back-to-back calls. So 640 ns is 8 counts of call time.
- Fix: compute the step from TPSC, and time a call apart.

### 2. The ns that do not match the cycles

The two rows: P1.4 n 48 (6,286.93 ns against 1,135.22 cycles) and P1.2 n 12 (2,321.85 ns against 342.21 cycles). Status: OPEN. The cycles are the trusted figure.

**The extra (my arithmetic from their MEASURED rows).**

| Row | ns a ray − cycles × 4.99985 |
|---|---|
| n 48 | 6,286.93 − 1,135.22 × 4.99985 = 610.9 ns |
| n 12 | 2,321.85 − 342.21 × 4.99985 = 610.8 ns |

- Each row is 16 × 256 = 4,096 rays. So both rows carry an extra 2.502 ms in the ns and none in the cycles, and the two extras are the same to 0.01 %.
- The "640 ns step" match is a coincidence of dividing by the ray count. The event is 2.5 ms long, not one count.

**Why the cycles are trusted.**
- Both rows' cycles lie on their P1 lines (slopes 22.41 / 22.10 / 21.89 round n 48).
- A 2.5 ms event inside the 16 runs would show in the cycles too, about 122 cycles a ray. So the event happened where only one clock sees it.

**Two readings, neither tested.**
- (a) **The read gap.** The reads are `c0 = cyc(); t0 = ns(); … dc = cyc() − c0; dt = ns() − t0` (`bench_ray.c:191-193`). Anything that runs between the last `cyc()` and the last `ns()` lands in `dt` alone.
  - That gap is under 1 us. A 2.5 ms interrupt or thread switch landing there in 2 rows out of 30 would be unlikely, unless something ties it to the `ns()` call itself.
- (b) **The counter stops.** The cycle counter would not count during some 2.5 ms state, for example a sleep in another thread. The SH7750's counting rules in such states are not checked here.

**Test for the next image.**
- Read `ns(), cyc(), ns()` at both ends, and print the larger of the two ns gaps.
- Count interrupts in the timed region: PMCR mode 0x16 on PRFC1, in a second run.
- Repeat P1.2 and P1.4 three times.

Until then no ns figure from ray1 goes in the ledger; the cycles do.

### 3. Why the 16:00 slice is cheaper than the 14:00 one

The console measured P2.B 539.68 against P2.A 554.19 cycles a ray. Status: settled on the host.

**The count.** A new host program, `demo/psm/bench/ray_mean_n.c`, log `D:\tmp\psmbuild\logs\ray_mean_n_20261125.log`. It counts the steps of P2's own 1,024 stand-ins (`ray_points` seed 7) at both suns with `ray_walk_ref`. The console kernel matched that walk byte for byte on every page. This is a HOST count, with no timing.

| Sun | lit / shadow | mean n | lit rays' mean n | shadowed rays' mean n | rays with n 0 to 7 | P1.4 line at these n | console P2 warm (MEASURED) |
|---|---|---|---|---|---|---|---|
| 14:00 (213.8° / 26.5°) | 524 / 500 | 22.08 | 38.61 | 4.75 | 407 | 561.35 cycles | 554.19 (P2.A.2) |
| 16:00 (242.1° / 12.9°) | 432 / 592 | 21.47 | 41.64 | 6.75 | 432 | 547.91 cycles | 539.68 (P2.B.2) |

**What it shows.**
- At 16:00 more stand-ins are shadowed (592 against 500), and a shadowed ray stops after a few steps.
- So the mean n falls from 22.08 to 21.47, although a lit ray walks further (41.64 against 38.61).
- P1.4's fitted line (C_setup 74.09 + 22.07 n), applied ray by ray, gives 561.35 and 547.91. The console measured 554.19 and 539.68:
  - 1.3 % and 1.5 % under the line;
  - in the same ratio, 0.976 on the host and 0.974 on the console.
- The slice's cost is explained by its n.

**What it does not settle.** P2 does not test thesis 5.9's 16:00 figure (mean n 48, on the town's real receivers). That figure stays OPEN. Its test is P2 run on the real receivers, not on the stand-ins.

### 4. The SH4ZAM-DCE projecting batch is slower than `mat_transform`

What stays and what is demoted. This is a recommendation; the owner decides.

**The figures** (MEASURED, h3, one console, Cortana's transcription):

| Routine | Cycles a vertex in the cache | Out of the cache |
|---|---|---|
| `mat_transform` | 13.38 | 42.86 |
| the batch, with prefetch | 14.15 | 45.99 |
| the batch, without prefetch | 14.14 | 48.14 |
| SH4ZAM's C loop | 28.60 | 57.12 |

- The batch's bytes equal SH4ZAM's own calls. `mat_transform` is up to 2 ulp off them (h3.5, h3.6).
- The header's pipeline model said 11.0 cycles in the cache (`shz_dce_xmtrx.h:73`, E). The console says 14.15, so the model is 29 % low.
- The rule (AGENTS.md): "Show a measured gain on a real console for any asm kernel."

**The recommendation, routine by routine.**

| Routine | Measured gain on a console | Recommendation |
|---|---|---|
| `shz_dce_pvr_project` | Yes, over its C form: 21.15 against 43.19 cycles in the cache (h2, SH4-023), and 60.5 to 61.7 against 76.7 to 79.2 in c4b. KallistiOS has no routine that writes PVR vertices. | **Stays.** |
| `shz_dce_xmtrx_project_point3_batch` | Over SH4ZAM's C loop, yes: 2.0 times faster in the cache, 1.24 times out of it. Over `mat_transform`, no: 5.8 % slower in the cache and 7.3 % out of it. | **Demote: not for hot paths.** Use `mat_transform` where 2 ulp does not matter. Keep the batch only where the bytes must equal SH4ZAM's own calls (host and console agreement, tests). Mark the header's cycle table as a model that the console contradicts. |
| The batch's prefetch | 4.5 % out of the cache (48.14 to 45.99); nothing in it. | Keep the switch, default on. |
| `shz_dce_xmtrx_transform_vec4_batch`, `_point3_batch`, `_vec3_batch` | Never timed on a console. | **Not proven.** No hot-path use until a console page shows a gain over the best KallistiOS or SH4ZAM call for the same job. |
| The `shz_dce_` wrappers over SH4ZAM (asinf, acosf, atan2f and the others, from the audit of 2026-11-25) | They are not our asm; they call SH4ZAM. | They stay. The asm rule does not apply. |

**Not changed by this note.** Nothing in `third_party/sh4zam-dce` is deleted or edited. The header comment and the README change only after the owner decides.

**One thing to try before demoting for good** (ESTIMATE, not built).
- `mat_transform` keeps one vector in flight, with its loads interleaved.
- The batch keeps three in flight. It may pay in register moves what the model did not count.
- The next image could time the batch with two vectors in flight.

## Ray bench (image ray2): built

Built 2026-11-25 by Senior. No number from it exists yet.

- Image: `demo/psm/out/cdi/psm-bench-ray2.cdi` (1,745,961 bytes, sha256 7e668d77b3c535f6978d633f7de08a66e31c43d2335eee9d99937a6b7d0b0db1).
- ELF: `build/psm_bench/psm-bench-ray2.elf`, 0 warnings at `-Wall -Wextra`.
- Build log: `D:\tmp\psmbuild\logs\bench_ray_ray2_20261125_092751.log`.
- Host test `ray_test.c`: 20,064 checks, 0 failed, at `-O2` and at `-O3 -ffast-math`.

### What changed from ray1

**CLOCK (Q62, and the CLOCK.2 bug).**
- CLOCK.2 now reads the cycles at the same point as the ns, before anything is printed. Expected: about 200,000 cycles in 1 ms.
- CLOCK.3 is new: the cycles one ns call takes.
- CLOCK.4 is new: over 2.2 s it compares each step of the ns clock with the cycles between the two reads. It prints how many times the ns clock stepped over 1 ms ahead, and the largest step.

**A cause for the 2.502 ms, found in KallistiOS's timer source** (read only; `kernel/arch/dreamcast/kernel/timer.c` and `include/arch/timer.h`, tree 568c3404). Status: ESTIMATE until CLOCK.4 is photographed.
- The ns clock is timer TMU2. KallistiOS reloads it every `TIMER_PCK / 4` = 12,468,720 counts, from its clock constant 199,499,520 Hz.
- It converts a count to 80 ns. So a full reload is 12,468,720 × 80 = 997,497,600 ns.
- At each reload it adds one whole second. So the ns clock steps ahead by 1,000,000,000 − 997,497,600 = **2,502,400 ns once every reload**.
- ray1's two odd rows each carried 2.502 ms extra in the ns and nothing in the cycles (section 2 above). A row that spans a reload gets exactly this step.
- This replaces readings (a) and (b) of section 2 as the leading explanation. They stay untested, not ruled out.
- If CLOCK.4 confirms it: every interval timed with `timer_ns_gettime64` across a reload reads 2.5 ms long, in every bench and spike of this project. The cycle counter does not have the step.

**P1 rows.** Each row now prints a second line:
- the ns less the cycles (as ns) for the row's 16 passes;
- the two gaps between an ns read and the cycle read beside it (`ns, cyc, ns` at both ends);
- 1 if the ns clock's second changed during the row;
- the interrupts counted in a second run of the same passes (counter 1 in interrupt mode, then back on cycles).

P1.2 and P1.4 run three times each (a, b, c).

**Page RT, new: a shadow ray against a triangle.** The town notes' TOWN-063 assumes 50 cycles a test (ESTIMATE); this page measures it.
- Scene: 256 points × 96 triangles, all in the cache, sun at 14:00, each ray tested until its first hit.
- Four forms, cycles a test each:
  - any direction, with early outs (the kernel);
  - any direction, every quantity worked out (its reference);
  - prepared for the sun, with early outs;
  - prepared for the sun, reference.
- "Prepared for the sun": every shadow ray has the sun's direction, so what depends on the direction and the triangle alone is worked out once a triangle (64 bytes a triangle). A test is then one subtraction, one cross product and at most three dot products. No divide in any form.
- The maths is SH4ZAM's (`shz_vec3_dot`, `shz_vec3_cross`, `shz_vec3_sub`).
- The page prints whether each kernel gives its reference's bytes.
- Host test (`ray_test.c`, test 6), HOST:
  - known hit, miss and behind cases pass;
  - kernel equals reference, 0 bytes differ, in both forms, at both suns;
  - over 131,072 pairs, the prepared form differs from the general form in 0, and the general form from a double-precision test in 0;
  - 4,096 points × 128 triangles took 472,197 tests (115.3 a ray); 874 of 4,096 rays were shadowed at 14:00.

**Not built: the batch with two vectors in flight.** It needs a new SH-4 loop in `sh4zam-dce` with its own reference and host test; that is not cheap, and the batch is demoted. Left for a later image if the owner wants it.

### Risks
- The page count is higher (eight P1 runs of about 2.5 pages each).
- The triangle kernels' bytes were compared on x86 only; the console prints its own comparison.
- The interrupt count is taken in a second run of the row, not the timed one.

## Ray bench ray2: the result notes checked against the design (Senior)

Checked 2026-11-25 against `bench_ray.c`, the source the image was built from. I did not read the photos. The values are from `RAY2_RESULTS_cortana.md` (p1 to p19) and `RAY2_RESULTS_junior.md` (p19 to p41).

### The pages match the design

| Design item | In the notes | Verdict |
|---|---|---|
| CLOCK.1 to CLOCK.4 on one page | R2C-01 to R2C-04, p2 | present |
| P1 rows: cycles, ns, then a second line of ns-cyc, two gaps, sec, irq | every P1 row, p3 to p25 | present, all fields |
| P1.2 and P1.4 three times (a, b, c) | P1.2a/b/c, P1.4a/b/c | present |
| Page RT: four forms and two byte comparisons | R2J-34 to R2J-39 | present |
| P2, P9, h3 as in ray1 | R2J-13 to R2J-33 | present |
| Group 2 (OC-RAM) on START | R2J-40 to R2J-46 | present |

No page is missing and no value was marked unreadable.

### Q62: settled

| ID | Claim | Status |
|---|---|---|
| Q62-1 | The ns clock of KallistiOS steps ahead once every timer reload. Predicted from its source: 2,502,400 ns. CLOCK.4: largest step 2,502,321 ns, 2 steps in 2.2 s. | MEASURED 2026-11-25, one console (R2C-04). The prediction holds to 79 ns, which is under one 80 ns count. |
| Q62-2 | The two odd rows of ray1 (P1.4 n 48 and P1.2 n 12) each spanned one reload. | DERIVED: the surplus of each row is one step (2.41 and 2.50 ms). ray1 printed no flag, so this is not MEASURED for those rows. In ray2 no P1 row has sec 1, and none shows the surplus. |
| Q62-3 | The other ns figures of ray1 are sound. | DERIVED: in ray2 every ns a ray is 4.997 to 5.000 times its cycles a ray (R2C-14). |

**Recommendation for the ledger:** close Q62. The cycle counter is the clock for every interval. An ns interval is safe only with the step taken out: `n - n / 1000000000 * 2502400`.

**What this touches outside the ray bench.**
- `compare/c4_spike.c` timed every wait and every frame with the ns clock. In c4b, a frame or wait that spanned a reload read 2.5 ms long: about 2 frames in a 60-frame run.
- The c4b means move by under 0.1 ms. A single largest value may carry the step.
- Image **c4f** takes the step out (see `compare/NOTES.md`).
- `compare/r3_spike.c` does not use the ns clock.

### Points in the notes that the source explains

| Note | Explanation from `bench_ray.c` | Status |
|---|---|---|
| p1 and p2 say "/35", later pages "/41" | By design. Group 1 ends at page 35. START runs group 2, which adds pages 36 to 41. p1 and p2 were photographed before START was pressed. The CLOCK values do not change. | FACT from source |
| CLOCK.1 reads 640 ns, more than one count (80 ns) | CLOCK.1 takes the least difference between two calls in a loop. One call is 92 cycles (460 ns), plus the compare of the loop. So it shows the period of the loop, not the step of the timer. **CLOCK.1 does not measure the resolution. That is my design error.** The count is 80 ns (FACT, KallistiOS source). | FACT from source |
| ns-cyc is about -1,150 ns at zero length (R2C-31) | By construction. At each end the reads are `ns, cyc, ns`, and the ns of the row is taken from the inner pair. So the cycle window is longer than the ns window by part of gap a and part of gap b. Half of (1,400 + 950) ns is 1,175 ns. | DERIVED |
| ns gains 30 to 33 ppm on the cycles (R2C-30) | The cycles of a row are turned into ns at 200.006 MHz. If that figure is right, the count of the timer is 79.998 ns and KallistiOS counts it as 80. The size depends on where 200.006 MHz came from, which I did not check again. | ESTIMATE |
| Gap a is about 400 ns longer than gap b (R2C-32) | Not explained by the source: the two ends are the same three calls. | OPEN |
| irq is counted in a second run | As designed, and listed as a risk. It cannot explain a slow timed row. | FACT from source |

### What I would change in a ray3

- Drop CLOCK.1, or replace it with the count read from the register of the timer.
- Print "once a reload (0.9975 s)" for CLOCK.4, not "once a second".
- Use `psm_page.h`: small text, a CRC line, and a cost-against-n graph for P1.
