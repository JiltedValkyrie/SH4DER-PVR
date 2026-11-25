# demo/psm: what the idle chips can take off the SH-4

A plan, written 2026-11-25. Nothing in it was built or run. Each claim carries a mark:

- **FACT**: read in a cited source (the source is named on the line; the list is at the end).
- **MEASURED**: printed by a real console in this repo's benches (`demo/psm/bench/NOTES.md`, run h1).
- **UNMEASURED GUESS**: my estimate. No console has printed it.

The SH-4 cache sizes are 8 KB of instruction cache and 16 KB of operand cache. FACT (SysArc §2.2.1, Table 2-4: "I$: 8K; D$: 16K").

## The answer in one paragraph

The AICA is a slow island: the ARM7 sees only sound RAM, and everything crosses the 16-bit G2 bus. Sega's own table puts AICA DMA at 11.3 to 14.4 MB/s; our console measured 5.0 to 5.7 MB/s (h2). A per-frame shadow map through sound RAM therefore costs more than the SH-4 spends making it, and it does not fit. What fits is **background work with no frame deadline that ends in a small table**: the static town's sun shadow on the ground, rebaked by the ARM7 when the clock moves the sun, and brought back by G2 DMA with no SH-4 time. The DSP does not fit any shadow work. The largest win available is on the SH-4 itself: the vertex send path (item 1 below). It needs no new chip.

## Ranked list

| Rank | What | Chip | Fits? | Gain (mark) | Cheapest console bench |
|---|---|---|---|---|---|
| 1 | Vertex send path: prepared 32-byte vertices sent in long store-queue runs (or by ch2 DMA), with record layout and prefetch | SH-4 (+ ch2 DMA) | **Yes** | TA feed for 28,699 vertices: 7.5 ms one at a time against 4.1 ms in one SQ run (MEASURED rates, h1); the frame gain after the extra build writes is an UNMEASURED GUESS of 1.5 to 3 ms | Town a3 boot view, one frame sent both ways, timed with the existing cost line |
| 2 | Static ground sun-shadow map, rebaked by the ARM7 when the sun moves; the SH-4 samples it per ground vertex (or uses it as a texture) | ARM7 + G2 DMA | **Yes, as background** | Takes the static casters out of the per-frame map. Per-frame SH-4 shadow work is then the movers only, which matches the hybrid two-map plan. The size of the saving is an UNMEASURED GUESS | Item B2 below: ARM7 loop over 32 KB, then a 16 KB G2 DMA back, both timed |
| 3 | Sound RAM as storage for data used rarely (the other scene's meshes, MAP SIZE 512's buffers while it is not chosen) | Sound RAM + G2 DMA | **Yes, as storage** | Up to about 2 MB of main RAM kept free in this demo (no sound driver loaded, see Q2) | One 512 KB G2 DMA each way, timed |
| 4 | Small per-frame tables from the ARM7 (under about 4 KB, for example a light-direction table) | ARM7 + G2 DMA | Marginal | 0.28 to 0.36 ms of G2 time for 4 KB at Sega's rate (FACT); 0.82 ms measured back to RAM (h2, MEASURED), running beside the SH-4 | As B2, at 4 KB |
| — | Per-frame shadow map through sound RAM (any size) | G2 | **No** | 32 KB costs 2.3 to 2.9 ms each way at Sega's rate (FACT); measured 5.7 ms in and 6.5 ms back (h2, MEASURED). Route B builds its map in RAM already, so sound RAM only adds a trip | — |
| — | ARM7 rasterising the map, or running the warp or perspective maths | ARM7 | **No** | No FPU and no main-RAM access. The warp maths is one matrix a frame (microseconds on the SH-4) | — |
| — | ARM7 twiddling the map for the bump-unit compare | ARM7 | **No** | The round trip (2 x 2.3 to 2.9 ms for 32 KB) is longer than the SH-4's table twiddle (1,504 us for 32 KB, MEASURED) | — |
| — | AICA DSP for any of it | DSP | **No** | See Q3 | — |

## Q1. Can a DMA channel copy sound RAM to main RAM, avoiding slow G2 PIO?

**Yes, by G2-DMA channel 0 (AICA-DMA), direction 1.**

- FACT (SysArc §2.6.6, Example 1): `SB_ADDIR` = 1 is "wave memory to system memory". Sega's own example reads 64 bytes from 0x00800000 to 0x0C001000, after `SB_G2APRO` = 0x4659007F.
- FACT (KOS `g2dma.c`, `g2bus.h`): `g2_dma_transfer(sh4, g2, len, block, cb, data, G2_DMA_TO_SH4, 0, G2_DMA_CHAN_SPU, 0)` does it.
  - Both addresses must be 32-byte aligned. The length is rounded up to 32.
  - KOS sets `SB_G2APRO` to 0x4659007F at `g2_dma_init` (all of main RAM).
  - KOS starts channel 0 with the hardware trigger ("wait until fifo is empty"); channels 2 and 3 use the CPU trigger.
  - Completion: a semaphore, or a callback from the G2 DMA interrupt.
- FACT (KOS `spu.c`): KOS has no read-back DMA helper.
  - `spu_dma_transfer` only writes to the AICA.
  - `spu_memread` is PIO: `g2_read_block_32` under `g2_lock`.
  - So the read-back calls `g2_dma_transfer` directly.
- FACT (Sega SH4 access table): the SH-4 DMAC channels 1 to 3 are listed for the G2 external area, **not** for AICA wave memory. Wave memory lists only GD-DMA and AICA-DMA. So DMAC ch1/ch3 is not a path to sound RAM.
- FACT (SH4 access table): AICA-DMA also lists texture memory (PVR I/F) as a root-bus end. So sound RAM to VRAM is a single G2 DMA, at the same G2 rate.
- **Rate.**
  - FACT (SH4 access table): Sega gives 11.3 and 14.4 MB/s for AICA-DMA between wave memory and system or texture memory, as "simulation optimums". The text extract is garbled, so I cannot say which number belongs to which direction.
  - FACT (SysArc §1.3): the ceiling is the G2 bus, 16 bits at 25 MHz (50 MB/s).
  - MEASURED (h2, 2026-11-25, one console): 5.7 MB/s to sound RAM and 4.9 to 5.0 MB/s back, the same at 4 KB and at 512 KB. That is about half Sega's figures. See Q2.
- **Rules that bear on it.**
  - FACT (SysArc §4.2.2.3): an SH-4 read of the AICA "sometimes take[s] 16 µs, during which time the CPU bus, the internal bus (Root Bus) and the G2 bus come to a complete stop".
  - FACT (SysArc §4.2.2.3): writes must be at most 8 in a row, then wait for the buffer.
  - FACT (SysArc §4.2.2.3): to touch the AICA by PIO while AICA-DMA runs, suspend the DMA and wait for the buffers to empty.
  - FACT (KOS `g2bus.h`): KOS does this in every `g2_read_*` / `g2_write_*`: IRQs off, DMA ch0 to ch2 suspended. So any PIO by any KOS code (sound stream poll, CDDA, `spu_memread`) pauses our DMA for its length.
  - FACT (SysArc §2.6.1): "all DMAs steal cycles".
  - REPORTED (LKML 2007, Linux AICA driver): unguarded interrupts during G2 access caused timeouts and random crashes.
  - REPORTED (LKML 2007): Maple and AICA share G2, and a DMA on one can time out the other.
  - REPORTED and DISPUTED (LKML 2007; Marcus Comstedt disputed it in the same thread): some hardware revisions lock up when a G2 DMA is requested too soon after the previous one. Not confirmed by any primary source I have.
  - Rule for us: one G2 DMA in flight at a time, wait for its interrupt, and never poll its status in a tight loop. The bench showed that polling slows PVR-DMA, MEASURED (h1: 14,775 us polled all the time against 9,595 us polled every 40 us).
- **The ARM7 can start the copy with no SH-4 work.**
  - REPORTED (Corlett's AICA notes): the ARM7 sets bit 5 of `MCIEB`/`MCIPD` to interrupt the SH-4.
  - FACT (SysArc §2.6.6, Example 3): G2-DMA can be started by an AICA interrupt (trigger select 2 plus `SB_G2DTNRM` / `SB_G2DTEXT`), with a register read to order the set-up.
  - Together: the ARM7 finishes a table, raises its interrupt, and the DMA copies it to RAM. UNMEASURED: nobody here has done it.

## Q2. Moving a shadow map from sound RAM, and what fits there

Times for one G2 DMA. The ranges use Sega's 11.3 to 14.4 MB/s (FACT, simulation optimums). The ceiling column is the 50 MB/s bus limit (FACT, the bus spec). KB = 1,024 bytes. These rows are Sega's rates, not timings; the console's are below.

| Map | 8-bit | Sega rate | ceiling | 16-bit | Sega rate | ceiling |
|---|---|---|---|---|---|---|
| 64 x 64 | 4 KB | 0.28–0.36 ms | 0.08 ms | 8 KB | 0.57–0.72 ms | 0.16 ms |
| 128 x 128 | 16 KB | 1.14–1.45 ms | 0.33 ms | 32 KB | 2.28–2.90 ms | 0.66 ms |
| 256 x 256 | 64 KB | 4.55–5.80 ms | 1.31 ms | 128 KB | 9.10–11.60 ms | 2.62 ms |
| 512 x 512 | 256 KB | 18.2–23.2 ms | 5.24 ms | 512 KB | 36.4–46.4 ms | 10.5 ms |

Bench h2 then timed it (MEASURED 2026-11-25, one console, one run; `bench/NOTES.md` "Hardware result h2", groups 3 and 4). G2 DMA ran at 5.7 MB/s into sound RAM and 4.9 to 5.0 MB/s back, about half Sega's figures, at every size. The table above is therefore optimistic by about 2x. Measured times at the sizes h2 ran:

| Bytes | Map it fits | RAM to sound RAM | sound RAM to RAM |
|---|---|---|---|
| 4 KB | 64 x 64, 8-bit | 714 us | 821 us |
| 32 KB | 128 x 128, 16-bit | 5,681 us | 6,510 us |
| 128 KB | 256 x 256, 16-bit | 22,715 us | 26,025 us |
| 512 KB | 512 x 512, 16-bit | 90,816 us | 104,066 us |

During each copy an SH-4 register loop kept 98 to 100 % of its rate. One G2 DMA moved 32 KB from sound RAM straight into texture memory in 6,511 us (5.0 MB/s), with every byte right.

For comparison, all MEASURED (h1):

- main RAM to VRAM by ch2 DMA: 32 KB in 98 us;
- PVR-DMA VRAM to RAM: 32 KB in 927 us (and faster with rare polling);
- a store-queue copy from RAM to RAM: 148.9 MB/s.

Sound RAM is the slowest place in the machine to keep a map that changes every frame.

**What fits in sound RAM:**

- FACT (KOS `spu.c`, `hardware.c`): KOS's `spu_init` runs at boot. It clears all 2 MB and puts a one-word idle loop at address 0 for the ARM7.
- FACT (`psm_dc.c`: `KOS_INIT_FLAGS(INIT_DEFAULT)`, no `snd_init` call): this demo loads no sound driver. So about 2,048 KB is free, less our own ARM7 program.
- FACT (KOS `aica_cmd_iface.h`, `AICA_RAM_START` 0x30000): if a later build loads KOS's sound driver, it takes the first 192 KB, leaving 1,856 KB. dc-forerunner's `docs/AUDIO.md` records the same figure.
- Every map size above fits, the largest (512 KB) with room for four of them.
- UNMEASURED GUESS: an ARM7 program for item 2 is under 16 KB, and its working data is about 100 KB (a 128 x 128 height field and the output map, see B2).

## Q3. Can the ARM7 or the DSP take real work?

### The ARM7: a slow background thread, yes. A per-frame worker, no.

- FACT (SysArc §4.2.2): the ARM7DI sits in the AICA's audio block, clocked at 22.5792 MHz. FACT (SysArc §4.2.2.4): it shares the 16-bit, 67.7 MHz sound RAM with the 64 voices, the DSP and the SH-4.
- FACT (SysArc Table 4-8; Corlett's notes): the ARM7 sees sound RAM at 0 and the AICA registers at 0x800000, and nothing else: no main RAM, no VRAM.
- REPORTED (DCEmulation forum, a poster who hedged it): no cache, so the ARM7 stalls whenever the sound chip holds the memory bus.
- Not sourced here: that it has no FPU. ARM7DI is an integer core (general ARM knowledge, not read in a source this session). All work is fixed point.
- UNMEASURED: its real speed on this bus. That is bench B1, and the first number Senior's h2 should get.
- FACT (this machine): `/opt/toolchains/dc` has `sh-elf` only, and KOS uses `stream.drv.prebuilt`. **There is no ARM compiler here.** An ARM toolchain build (dc-chain's arm-eabi) comes before any ARM7 work. It is a setup cost, not a code risk.
- A later sound build needs our ARM program and KOS's sound driver to share the ARM7, or one replaces the other. That is a real conflict for a game. It is not one for this demo.

**What it can do for the shadows (item 2):**

- Bake the **static** casters' sun shadow on the ground into a world-space ground map, 1 byte a texel, for example 128 x 128 (16 KB) over the town square.
- Input: a height field of the static town, made once at load by the SH-4 and copied in by DMA. For each texel, march toward the sun.
- It is a height field, so overhangs (the arch) are approximate.
- Output: the map, copied back by G2 DMA. The ARM7's interrupt can trigger the copy (Q1).
- The SH-4 then:
  - samples the map per ground vertex (a bilinear read, a few cycles), or uploads it as a 16 KB texture modulated on the ground;
  - draws only the **movers** (people, hens) into its per-frame map, the "model map" of the hybrid plan.
- Rate:
  - UNMEASURED GUESS: 32 steps x about 8 instructions a texel is about 4.2 M instructions a bake. At a guessed 5 to 15 MIPS effective, one bake takes 0.3 to 0.8 s.
  - The town clock moves the sun slowly (x60 time speed: a quarter of a degree a second), so a bake every second or so, cross-faded, keeps up. UNMEASURED.
- Risk:
  - Seams where the ground map's shadow meets the movers' map: the same seam test as the hybrid plan.
  - A stale map while the sun moves fast (TIME SPEED at its top, or the sun moved by hand).
  - Fallback: the SH-4's own map, as today.

**What it cannot do in a frame:** build or twiddle a map, the warp or perspective maths, or per-vertex visibility of moving geometry. Each needs data that lives in main RAM and changes every frame. Q2's table shows the G2 trip alone costs more than the SH-4 work.

### The DSP: a signal path, not a thread. Does not fit.

From Corlett's notes, a register-level reverse-engineering (REPORTED), agreeing with SysArc §4.2.2 (FACT: "128 steps per sample, includes ring buffer function"):

- Program: 128 fixed steps run once per 44.1 kHz sample, with no branches or loops. That is 5.6 M multiply-adds a second, about 94,000 in a 60 Hz frame.
- Arithmetic: 24-bit signed data times **13-bit** coefficients, a 26-bit accumulator. The 128 coefficients are registers the SH-4 or ARM7 writes.
- Memory:
  - reads and writes only inside one sound-RAM ring (8 K to 64 K words, base `RBP`), in a 16-bit float or 16-bit integer form;
  - memory steps are supposed to be on odd steps only, so 64 a sample at most;
  - with `TABLE` = 1 the address is absolute in the ring, plus a computed 12-bit offset (`ADRS_REG`).
- Output: `EFREG` goes to the mixer (audio). Results for us would have to be written to sound RAM with `MWT`, then copied back by G2 DMA.

Verdict:

- UNMEASURED GUESS: a 4 x 3 transform per element in 13-bit coefficients is about 12 multiplies and 6 memory steps, so roughly 9 elements a sample, about 6,600 a frame.
- 13-bit coefficients give 1/4,096 steps, too coarse for clip space.
- The data still crosses G2 both ways: about 158 KB a frame for 6,600 vertices at 12 bytes in and out, which is 11 to 14 ms at the Sega rate (FACT) and about 30 ms at h2's measured 5.0 to 5.7 MB/s (ESTIMATE from MEASURED rates).
- The SH-4 does the same transform in 1.3 ms. MEASURED rate: 38.7 cycles a vertex (`mat_transform`).
- It would also take the DSP from audio effects.
- **Does not fit.**

## Q4. The SH-4 vertex loop: what to change

What the code does now (read, `demo/psm/psm_scene.h` and `psm_send.h`):

- `psm_place_geo` loads the camera rows into XMTRX once (`psm_xf_load` → `shz_xmtrx_load_rows_4x4`) and applies one FTRV a point (`shz_xmtrx_transform_vec4`).
- It writes clip and screen positions into separate 12-byte-stride arrays: `gclip`, `gscr`, `mclip`, `mscr`.
- A second pass (`psm_send_ground`, `psm_send_models`) reads those arrays plus `guv` / `gpos`, `gcol` / `mcol`. It writes each vertex with `pvr_dr_target` / `pvr_dr_commit`: one 32-byte store-queue write a vertex, mixed with the reads and the strip logic.
- XMTRX use is already right: one load a pass, nothing else in between. The ground placement is cached for a still camera; the movers are placed every frame.

MEASURED context (h1, lead's figures):

- the town a3 SH-4 frame is 157.5 cycles a vertex;
- the bare transform is 38.7;
- the TA takes 144 ns a vertex from one long store-queue run against about 258 ns written one at a time.

So most of the frame is **not** the maths. It is memory and the send.

Changes, all under the SH4ZAM rule (`shz_*` calls only, SH4ZAM used as a library, no copied code):

1. **One 32-byte record a vertex, written in TA order.**
   - The placement pass writes a `pvr_vertex_t`-shaped record (flags, x, y, z, u, v, argb, oargb) into a 32-byte-aligned array, instead of three or four separate arrays.
   - Use `shz_dcache_alloc_line` (MOVCA.L) on each record's line: MEASURED, a missed write costs 41.9 cycles a line, and 16.2 after the allocate.
   - The colour pass rewrites only the `argb` word.
   - The send then copies records (ground strips are already in order; the models' strip runs are index lists) with `shz_sq_memcpy32_1` or one `shz_sq_memcpy32` a run.
   - UNMEASURED GUESS: the send drops from about 258 ns toward 144 ns a vertex, about 3 ms of 28,699 vertices, less the extra build writes.
   - RAM cost: 32 bytes x vertex count. For the town, 8,700 model and about 4,000 ground vertices (my count; `PSM_GV_MAX` depends on the rows) is about 400 KB.
   - Senior's hook already does the transform half: `shz_dce_pvr_project(out, in, count)` in `third_party/sh4zam-dce/include/sh4zam_dce/shz_dce_pvr.h`.
     - It takes 32-byte PVR records whose x, y, z are still in world space, runs them through XMTRX and the FSRRA divide, and leaves TA-ready records in RAM (the other five words kept). Then one SQ load sends the block.
     - Senior's figure: 19.5 cycles a vertex, an ESTIMATE, checked on the host and the SH-4 interpreter, with no console number.
     - The near-plane test, the cell decision and the strip order stay ours.
2. **Prefetch.** In the send and colour loops, `SHZ_PREFETCH` the record two or three vertices ahead. MEASURED: a read miss is 15 cycles a line over a hit (18.41 against 3.17). The arrays are far over the 16 KB operand cache, so every line is a miss without it.
3. **Strip order in the placement.** The ground can be placed and sent a row at a time (place row r + 1, decide row r's cells, send row r), so the working set is two rows (about 4 KB) and stays in the operand cache. This applies when the camera moves. A still camera already skips the placement.
4. **ch2 DMA for the send.** UNMEASURED (h1 lists "the TA's own DMA vertex mode" as not measured).
   - With records already in RAM, KOS's PVR DMA vertex mode could feed the TA while the SH-4 builds the next list.
   - Cost: a second vertex buffer in RAM (as big as the frame's parameter bytes, about 780 KB worst in the town sweep), and KOS's DMA mode set at `pvr_init`.
   - Try it only after change 1 shows its number.
5. **Leave alone.**
   - The FTRV path: it is already 2.1 times plain C (MEASURED).
   - FIPR for the lighting dots: the light terms come from small tables (`glut`), not dots.
   - OC-RAM: MEASURED, it costs 59% on everything else that misses, because the cache halves.

Risk: change 1 touches the shared send walk that the host tests count (`psm_send.h`, `test/psm_town_test.c` budget and sweep). Those tests must keep the same vertex and byte counts.

## Later, a separate change: the model placement through `shz_dce_pvr_project` (proposal, nothing built)

Why not now:

- `shz_dce_pvr_project` assumes every vertex has W > 0 and returns screen x, y and 1/W only.
- `psm_place_geo` needs clip X, Y and W for every vertex, for four things:
  - the near-plane test (`mclip[..][2] > PSM_NEAR`);
  - the area guard (`sh4der_tri_area_ok` on clip coordinates);
  - the draw-distance cut (W against DRAW DISTANCE);
  - route B's `mvis`.

The route, per object:

1. **A model object table** (new; the caster objects `co[]` have boxes, the receiver models `mv` / `mt` do not). Each entry holds:
   - first vertex and count;
   - first triangle and count;
   - a world box, fixed for the static town and remade every frame for the movers (`psm_movers_apply` already builds the casters' boxes).
2. **A bound test, once per object per frame.**
   - Take the box's 8 corners through the camera rows (8 FTRV), or its centre and radius against the view axis.
   - Wholly in front means: the smallest W is over `PSM_NEAR` plus a margin, and, with a DRAW DISTANCE set, the largest W is under it.
3. **An object wholly in front** takes the fast path:
   - Its vertices live as 32-byte records (world x, y, z; the colour word written by `psm_colour`).
   - One call, `shz_dce_pvr_project(out, in, vn)`, gives TA-ready records.
   - Its triangles take a screen-space area guard: the same signed-area threshold on screen x, y, in place of the clip form.
   - Every vertex is marked in `mvis`. There is no cut.
4. **Any other object** (across the near plane, past the draw distance, or partly cut) keeps today's path unchanged.
5. **The ground stays on today's path.** Its cell cull needs clip X and Y.

What to expect:

- UNMEASURED GUESS: about 20 cycles a vertex for the fast path (Senior's 19.5-cycle ESTIMATE) against about 40 to 60 today.
  - The town has about 8,700 model vertices.
  - That is roughly 0.2 to 0.35 ms on a frame where the camera moves. A still camera already skips the placement, so it gains nothing.
- RAM: 8,700 x 32 bytes x 2 (in and out) is about 560 KB. That is large for the gain.
- **Measure first.** The placement's time on the console, from a timer around `psm_place_geo` on a moving frame, says whether this is worth doing.

Host test, if built:

- For every object and 216 views, compare the fast path with today's: screen x, y and 1/W within FTRV's error, the same triangle flags, the same `mvis`.
- Count the objects that take the fast path, and those it should refuse (across the near plane, past the cut).

## The benches, cheapest first

Each runs on a real Dreamcast and prints one line a number. Senior's h2 plans the AICA tests; Lead coordinates which of these go into it.

| Id | What it proves | How |
|---|---|---|
| B0 | Item 1 | Town a3 boot view. Send the frame with `pvr_dr` per vertex, then from prepared records by `shz_sq_memcpy32`. Print SH-4 ms for each, with the existing `pvr_get_stats` line. No new chip. |
| B1 | The ARM7's real speed | Load a 200-byte ARM loop (a sum over 32 KB of sound RAM, then a write of a done word). Time it from the SH-4 by polling the done word **once a millisecond** (a G2 read stalls the buses, Q1). Print instructions a second. Needs the ARM toolchain. |
| B2 | Item 2's data path | SH-4 to sound RAM, 32 KB by `spu_dma_transfer`; ARM7 inverts it; ARM7 raises its SH-4 interrupt; G2 DMA back 16 KB, started by that interrupt or by the SH-4. Print each leg in us and check the bytes. |
| B3 | The G2 rate (done: h2, see Q2) | `g2_dma_transfer` to and from sound RAM at 4, 32, 128 and 512 KB, waiting on the semaphore (no polling). Print MB/s each way, and an SH-4 register loop's rate beside it (the h1 "beside" test, with its clock started **before** the DMA). |
| B4 | Sound RAM to VRAM directly (done: h2, 6,511 us for 32 KB) | One G2 DMA of 32 KB from wave memory to a texture address. Print us and show the texture. |

B3 needs no ARM code and answers Q1 and Q2 with numbers. It is the one to run first if only one runs.

## Sources

- Sega, *Dreamcast/Dev.Box System Architecture* (1999-09-03): §1.3 (buses), §2.2.1, §2.6.1, §2.6.6, §2.6.7, §4.2.2 to §4.2.2.4. https://segaretro.org/images/7/78/DreamcastDevBoxSystemArchitecture.pdf (read as extracted text).
- Sega, "SH4 access" table (1999-03-12), read as extracted text; the DMA rate rows are partly garbled. https://www.segaretro.org/images/3/3f/SH4_access990312_e.pdf
- Neill Corlett, *Yamaha AICA notes*: DSP registers and program, main-CPU interrupt registers. Local copy at `flycast/docs/neil_corlett_aica_notes.txt`.
- KallistiOS, local copy under `dc-halo/third_party/KallistiOS` (commit 568c3404): `hardware/g2dma.c`, `include/dc/g2bus.h`, `hardware/g2bus.c`, `hardware/spu.c`, `hardware/hardware.c`, `sound/arm/aica_cmd_iface.h`.
- SH4ZAM, `third_party/sh4zam/include/sh4zam/shz_mem.h`, `shz_xmtrx.h`.
- This repo:
  - `demo/psm/bench/NOTES.md` (h1, every MEASURED line);
  - `demo/psm/PVR_FEATURES.md` rows 10 to 12;
  - `third_party/dc-forerunner/docs/AUDIO.md` (the 192 KB driver figure).
- Reported, not primary:
  - Linux kernel list, 2007, "Fix DMA on Dreamcast" thread: https://lkml.iu.edu/hypermail/linux/kernel/0707.3/1190.html , https://lkml.iu.edu/hypermail/linux/kernel/0707.3/1227.html
  - Linux kernel list, 2007, "[PATCH] Dreamcast AICA sound driver G2 bus handling": https://lkml.iu.edu/hypermail/linux/kernel/0709.1/2517.html
  - DCEmulation forum (ARM7 stalls on the sound bus): https://dcemulation.org/phpBB/viewtopic.php?p=546204
- Not read, wanted (for Scholar, through Lead): Yamaha's AICA datasheet, for the ARM7's wait states and the DSP's memory-step rule. https://segaretro.org/images/d/de/AICA_E.pdf
