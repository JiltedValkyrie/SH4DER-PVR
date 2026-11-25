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
- **memcpy is slow, and not because of memory.** 43.2 MB/s is 4.6 cycles a byte; in the cache it is 46.5 MB/s, the
  same. A miss is 15 cycles in 32 bytes, half a cycle a byte: it cannot be the limit. So this memcpy is a
  byte-at-a-time loop for these arguments (reading; the routine was not opened). The store queue copies the same
  megabyte 3.4 times faster, measured.
- mat_transform costs 38.7 cycles a vertex against 80.9 for plain C: 2.09 times, measured. Out of the cache it
  loses only 4 cycles a vertex, far under two misses a line; the routine probably asks for lines ahead (not
  checked).
- Town a3's SH-4 frame is 157.5 cycles a vertex (lead's figures). The bare transform is a quarter of that.
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
  Reading from two tests; a test with its clock before the start is needed to state it as measured.
- **Cycle steal runs beside the SH-4.** Register-only work kept 90 %, work that misses the cache kept 45 %, and the
  copy ran at 96 to 125 MB/s. That is a copy 2.2 to 2.9 times memcpy's rate while the SH-4 keeps most of its time.
- **The PVR-DMA figure depends on how often its status is read.** 512 KB took 14,775 us with the status polled all
  the time, 13,746 us polled every 11 us, 9,595 us polled every 40 us (54.6 MB/s). The status register is on the
  same bus the transfer uses. So the 35 MB/s of route AB's spikes (s2, s3) was held down by my own polling loop,
  and the real rate is 55 MB/s or more. Reading from three lines; a test that polls once a millisecond, or waits
  for the interrupt, would give the rate.
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
- AICA: the ARM7, sound RAM, G2 DMA, the DSP.
- A burst copy timed from before its start. PVR-DMA with rare polling.
- A word loop and a 64-bit loop against memcpy.
- Any second console, any second run.
