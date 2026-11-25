# demo/psm, route AB: the PVR draws the depth map, the SH-4 compares

Research for the owner's idea. Nothing here was built, run or measured.

**The idea.**

1. The PVR draws the casters' depth into a render target. This takes the rasterising, route B's big cost, off the SH-4.
2. The map is copied back to RAM.
3. The SH-4 compares per receiver vertex, as route B does, which gives true self-shadowing.

A one-frame lag is accepted.

**How to read this.**

- **Documented**: a primary source says it, read in full. The primary sources here are Sega's 1999 access table, the KallistiOS source, Comstedt's register notes and the Flycast source.
- **Reported**: forum posts, search snippets or secondary pages; not read in full, or second-hand.
- **Inferred**: my own reasoning.

Emulator source shows how its authors model the hardware. It is strong evidence, but it is not a datasheet.

## 1. Copying the render target back to RAM

**The hardware can do it.**

- **Documented** (Sega, "SH4 access" table, 1999, page 2, "DMA accesses"): the PVR-DMA path lists Texture Memory (PVR I/F) → System Memory at **51.4 MB/s** for both the 64-bit and 32-bit areas. System Memory → Texture Memory is 64.3 MB/s. The table marks these as "simulation optimums".
- **Documented** (Flycast source, `core/hw/holly/sb.h`): the direction register is `SB_PDDIR` at `0x005F7C0C`, "PVR-DMA direction".
- **Documented** (Flycast source, `core/hw/pvr/pvr_sb_regs.cpp`, `do_pvr_dma`): `SB_PDDIR` set means "PVR -> System", clear means "System -> PVR". The source is `SB_PDSTAP` (the VRAM address) and the destination is `SB_PDSTAR` (the system RAM address). The emulator raises the PVR-DMA interrupt at the end.

**KallistiOS has no download.**

- **Documented** (KOS `pvr_dma.c`, master):
  - The SH-4 DMA set-up is `DMA_REQUEST_EXTERNAL_MEM_TO_DEV` on `DMA_CHANNEL_2` for every mode.
  - `SB_PDDIR` (or any direction bit) is never written.
  - `src` must be a 32-byte aligned system RAM address.
- **Documented** (KOS `pvr_dma.h`): the `PVR_DMA_VRAM32_SB` and `PVR_DMA_VRAM64_SB` modes are commented "Transfer to/from VRAM using PVR i/f". The function itself copies "to the PVR or its memory", and no download function exists.
- So a download means writing the PVR-DMA registers ourselves, with `SB_PDDIR` = 1 (**inferred**).
- ~~**Inferred:** the PVR-DMA path rides the SH-4 DMA controller's channel 2, the channel KOS uses for DMA vertex and texture uploads. A download cannot overlap those. Direct render (the store queues) does not use that channel.~~ **Superseded** (lead's correction, 2026-11-24, checked below): KOS's `pvr_dma` and the PVR-DMA are two different blocks.

**Correction: two DMA blocks, two channels, two interrupts.** Lead's correction, checked against the sources:

| | ch2-DMA (what KOS `pvr_dma` drives) | PVR-DMA (the one with a direction bit) |
|---|---|---|
| Holly registers | `0x005F6800` block: `SB_C2DSTAT`, `SB_C2DLEN`, `SB_C2DST`, `SB_LMMODE0/1` at `+0x84/+0x88` | `0x005F7C00` block: `SB_PDSTAP`, `SB_PDSTAR`, `SB_PDLEN`, `SB_PDDIR`, `SB_PDTSEL`, `SB_PDEN`, `SB_PDST` |
| SH-4 DMAC channel | 2 | 0 (in Flycast's model) |
| Direction | system RAM to the TA / texture memory only | either way, by `SB_PDDIR` |
| End interrupt (`ISTNRM`) | bit 19; KOS `ASIC_EVT_PVR_DMA` = `0x0013` | bit 11; no KOS event name |
| Sega table rate | "TA FIFO ch2-DMA": 702.6 MB/s (64-bit), 364.7 MB/s (32-bit) | "PVR-DMA": 64.3 MB/s up, 51.4 MB/s down |

Evidence for the table:

- **Documented** (KOS `pvr_dma.c`, master): the register pointer is `0xa05f6800`, with `PVR_LMMODE0` at `0x84/4` and `PVR_LMMODE1` at `0x88/4`. The SH-4 side is `DMA_CHANNEL_2`, and the interrupt hooked is `ASIC_EVT_PVR_DMA`.
- **Documented** (KOS `dc/asic.h`, master): `ASIC_EVT_PVR_DMA` is `0x0013`, "PVR DMA complete". No event is defined at `0x000b`.
  - **Inferred:** the low bits of these codes are the `ISTNRM` bit number: `RENDERDONE_TSP` = `0x0002` = bit 2, and `0x0013` = bit 19.
  - So KOS's "PVR DMA" event is the ch2-DMA end, not the PVR-DMA end. A download would need its own handler registered at `0x000b` through `asic_evt_set_handler` (inferred; not tried).
- **Documented** (Flycast `core/hw/holly/sb.h`): `0x005F6800 SB_C2DSTAT` "ch2-DMA destination address", and `0x005F7C00`–`0x005F7C18` the "PVR-DMA" registers.
- **Documented** (Flycast `core/hw/holly/holly_intc.h`): `holly_PVR_DMA` is "bit 11 = End of DMA interrupt : PVR-DMA"; `holly_CH2_DMA` is "bit 19 = End of DMA interrupt : ch2-DMA".
- **Documented** (Flycast `core/hw/pvr/pvr_sb_regs.cpp`, `do_pvr_dma`):
  - It refuses to run unless `DMAC_DMAOR & DMAOR_MASK == 0x8201`.
  - At the end it writes `DMAC_SAR(0)`, `DMAC_CHCR(0).TE` and `DMAC_DMATCR(0)`, then raises `holly_PVR_DMA`.
  - The ch2 path (`DMAC_Ch2St`, `core/hw/sh4/modules/dmac.cpp`) reads `DMAC_SAR(2)` and `SB_C2DSTAT` instead, and raises `holly_CH2_DMA`.
- **Documented** (KOS `include/arch/dmac.h`): `DMA_CHANNEL_0` is "On Dreamcast, reserved for hardware".
- **Documented** (KOS `hardware/dmac.c`, `dma_init`): DMAOR is written as `DDT | PR = CH2013 | DME`. That is `0x8000 | 0x0200 | 0x0001` = `0x8201`, the value Flycast requires.
  - The comment says the BIOS sets it on the Dreamcast; KOS writes it itself only on NAOMI.
  - **Inferred:** under KOS on a Dreamcast, DMAOR should already hold the value the PVR-DMA needs.

What this changes:

- **Inferred:** a download does not compete with KOS's DMA vertex and texture uploads for SH-4 channel 2. Read the "cannot overlap" line above as withdrawn.
- **Inferred:** it uses SH-4 channel 0 in the DDT (on-demand) mode that the hardware drives. Whether anything in KOS or the BIOS also uses channel 0 is **not known**; KOS only calls it "reserved for hardware".
- **Not known:** whether the PVR-DMA and a ch2-DMA can run at the same time, or how they share the bus. Nothing read says.
- **Not known:** which SH-4 channel 0 registers (SAR, DAR, DMATCR, CHCR) the programmer must set before `SB_PDST`. Flycast only writes them back at the end, so it does not show the required set-up. This must come from a hardware reference or a test.
- **Inferred:** `SB_PDTSEL` (trigger select) and `SB_PDEN` (enable) must also be set. Flycast starts the transfer on a write of 1 to `SB_PDST`.

**Time for one map** (arithmetic from Sega's 51.4 MB/s; not measured):

| Map | Bytes (16-bit) | PVR-DMA at 51.4 MB/s |
|---|---|---|
| 128 x 128 | 32 KB | 0.64 ms |
| 256 x 256 | 128 KB | 2.55 ms |

The DMA runs while the SH-4 works on other things. The CPU is only needed to start it and to take the interrupt.

**The render must be finished first.**

- **Documented** (KOS `pvr_irq.c` and `pvr_init_shutdown.c`): render-done is the interrupt `ASIC_EVT_PVR_RENDERDONE_TSP`. Its handler clears `pvr_state.render_busy`.
- **Documented** (KOS `pvr_scene.c`): `pvr_wait_render_done()` waits while `render_busy` is set.
- For a render-to-texture scene the page flip is skipped (`was_to_texture`), so that interrupt is the only sign that the target is done.
- **Inferred:** tiles are written to VRAM as each one finishes, so a copy that starts before render-done reads a partly drawn map.
- **Documented** (Sega table, page 1): "More time is required in the event of access contentions." **Inferred:** a download that overlaps the next scene's render competes with it for VRAM. Start the download from the map's render-done and finish it before the screen scene's render begins; a 0.64 ms copy fits that window.

**A one-frame-lag pipeline (inferred).** KOS renders one scene at a time.

1. Submit the map scene N (casters to the render target).
2. Compute the screen's vertex colours from map N-1, which is already in RAM.
3. Submit the screen scene N.
4. On map N's render-done, start the download into the other RAM buffer.
5. At frame N+1's compare, wait on the PVR-DMA interrupt. This is `ISTNRM` bit 11, code `0x000b`, which we hook ourselves; it is not KOS's `ASIC_EVT_PVR_DMA` (see the correction above).

That means two RAM buffers (2 x 32 KB at 128 x 128).

## 2. Reading VRAM directly from the SH-4

**Documented** (Sega, "SH4 access" table, page 1, "CPU accesses", 100 MHz bus clock, "simulation optimums"):

| | Read 2 B | Read 4 B | Read 32 B | Write 2 B | Write 4 B | Write 32 B |
|---|---|---|---|---|---|---|
| Texture memory (PVR I/F), 64-bit area `0400 0000` | 41 | 41 | 61 | 12 | 12 | 38 |
| Texture memory (PVR I/F), 32-bit area `0500 0000` | 41 | 41 | 61 | 12 | 12 | 38 |
| System memory | 7 or 12 | 7 or 12 | 7 or 12 | 4 or 9 | 4 or 9 | 4 or 9 |

(System memory: row hit 7 read / 4 write, row miss 12 / 9.)

**Sparse reads straight from VRAM.** Arithmetic from the table, not measured:

- 4 texels a vertex x 5,000 vertices = 20,000 two-byte reads x 41 bus clocks = **8.2 ms**.
- Two neighbouring texels in one 4-byte read (a non-twiddled row): 10,000 x 41 = **4.1 ms**.
- Copying the map with PVR-DMA first and comparing from RAM costs 0.64 ms of DMA (128 x 128), off the CPU. The compare's reads then hit system RAM and the cache. **Copy first.**

**A CPU copy instead of DMA.**

- **Inferred:** 32-byte reads at 61 bus clocks a line come to about 52 MB/s, so about 0.6 ms for 32 KB, the same as DMA. The CPU stalls through all of it.
- **Inferred:** a 32-byte read is a cache-line fill, so it needs the area mapped through the cache. The lines must be invalidated (`ocbi`) before each frame's copy, or the old map is read.

**Side finding (inferred, arithmetic):**

- `sh4der_frame_read` (`include/sh4der_frame.h`) makes about 4,800 uncached 16-bit reads of the frame on screen, 9,600 with `dark`. That is about **2.0 ms** (3.9 ms with `dark`) at 41 bus clocks a read.
- The README gives the count but no time. The look effect pays this every frame; worth a measurement.

## 3. Render target formats and putting depth in colour

**Render output formats.**

- **Documented** (Comstedt, `mc.pp.se/dc/pvr.html`): the render output register (`FB_W_CTRL`) packs RGB555, RGB565, ARGB4444, ARGB1555, RGB888 or ARGB8888, with a "D - Dither enable" bit for the 16-bit modes.
- **Documented** (Flycast `core/rend/gles/gltex.cpp`, `BindRTT`): pack modes 0 to 3 (the 16-bit ones) are emulated for render-to-texture. Modes 4 to 6 (24 and 32 bit) log "Unsupported render to texture format". Whether the hardware renders them to a texture is not documented in anything read, and an emulator would not show them. **Plan on 16 bits a texel.**
- **Documented** (KOS `pvr_scene.c`): `pvr_scene_begin_rtt` has no format argument. Its pitch is "a 16-bit render pitch in 64-bit units".
  - `FB_W_CTRL` (format and dither) is set in `vid_set_mode` (`dc/video.c`, not read). So whether a KOS render target is dithered is **not known**.
  - This library's route A uses an RGB565 non-twiddled target (`demo/psm/psm_dc.c`).
- **Reported** (DCEmulation, snippet): the PVR works at 32 bits internally and converts to 16-bit RGB565 with dithering on output.

**Why vertex colour is a poor carrier for depth (inferred).**

- A vertex colour is 8 bits a channel, and an RGB565 output keeps 5 or 6 of them.
- Dither, when on, changes the low bits.
- A depth spread over two channels with a carry cannot be interpolated linearly.
- How the PVR interpolates and rounds Gouraud colour (bits, perspective) is not documented in anything read.

Vertex colour gives at most 5 to 6 usable bits. That is too coarse, as the lead says.

**A ramp texture is the better carrier (all inferred, untested).**

- **The texture.** Draw each caster textured with a ramp whose texels hold their own index as the raw 16-bit RGB565 value.
  - Point sampling, no mipmaps.
  - Texture environment REPLACE (decal), not MODULATE: a multiply by white could lose a low bit.
  - No fog, no blending, **dither off** in `FB_W_CTRL` for the map scene.
  - If the colour path is exact, the output pixel is the texel's 16-bit value unchanged. This exactness is the first thing to test.
- **Depth through the PVR's perspective correction.**
  - The PVR's vertex depth is its 1/w (the value it sorts by, here the key k). It interpolates texture coordinates perspective-correct with it (Sega Retro lists perspective-correct texturing; that k is also the divisor is inferred).
  - So a texture coordinate written as u = a / k at each vertex comes out as exactly a / k at each pixel. The interpolated numerator is constant and the denominator is k's own interpolation, linear in screen space.
  - So the map stores a / k: a monotonic code of depth, finer near one end. The SH-4 computes the same a / k for each receiver.
  - Choose k's range (an offset plus the depth) so the steps over the scene's 40 to 60 m stay under route B's bias.
- **How many bits.**
  - One pass with a 1D ramp of 1,024 texels (the largest `PVR_UV_SIZE`): **10 bits**.
  - One pass with a 2D ramp, 1,024 x 64 (128 KB of VRAM): u = a / k clamped gives the coarse bits; v = 1,024 a / k wrapping over the 64 rows gives 6 fine bits. That is **16 bits in one pass**, if the PVR's texture-coordinate precision holds. Nothing read documents it; Direct3D asks 8 fractional bits after scaling and the PVR's figure is unknown. This is the second thing to test.
  - Two passes (coarse and fine into two targets) give the same bits. They do not need the 2D ramp, but they double the map's render.
- **The z-test across two passes.** The same vertices with the same k values go through the same ISP, so the nearest surface should be the same at every pixel in both passes. **Inferred:** the hardware is deterministic, but no source says so. The 2D one-pass ramp avoids the question.

## 4. Depth as colour on PowerVR, and the z-buffer

- **The PVR's z-buffer cannot be read.**
  - **Reported** (Sega Retro, technical specifications, snippet): "on-chip Z-buffering" in the ISP's processing elements.
  - **Reported** (AnandTech on PowerVR's tile rendering, snippet): "all z-calculations ... without having to access an external z-buffer".
  - **Inferred:** depth lives per 32 x 32 tile on the chip and is never written to VRAM, so there is nothing to copy back. Depth has to come out as colour.
- **Mobile PowerVR.** **Reported** (Khronos forum, snippet): developers on iPhone 3GS-era PowerVR SGX packed depth into RGBA because render-to-depth-texture was not supported there. That is a later chip family, but the same reason.
- **Dreamcast depth-as-colour.** Nothing found: no game, no homebrew, no forum post. The DCEmulation thread that may discuss a depth-buffer attempt (see `PRIOR_ART.md`) returned 403.

## 5. Verdict: doubtful but testable. Worth a two-day spike before any design work.

- **The copy-back is workable.** Sega's table documents the hardware path, Flycast models it, and the arithmetic gives about 0.64 ms of DMA per 128 x 128 map, off the CPU. It needs our own PVR-DMA register code, because KOS has no download. ~~and the DMA channel is shared with KOS's DMA uploads~~ **Superseded:** the PVR-DMA is a separate block on SH-4 channel 0 with its own interrupt (bit 11); see the correction in section 1. The open question is now the channel 0 set-up, not channel 2.
- **The SH-4 side is route B's compare, unchanged.** Self-shadowing stays.
- **Biggest risk: getting an exact depth code out of the PVR.** It rests on three undocumented things:
  1. Does a texel pass to a 16-bit render target bit-exact (REPLACE, point sampling, dither off)?
  2. What is the PVR's texture-coordinate precision for the 1,024 x 64 wrap?
  3. Can dither be switched off for the render-to-texture scene under KOS?

  If (1) fails, there is no depth code at all. If (2) fails, the map has 10 bits, not 16.
- **Second risk: the one-frame lag.** Shadows trail moving casters (the walkers) by a frame. At 30 fps and 1.3 m/s a frame is about 4 cm. It is likely invisible at our map's texel size (inferred).
- **The spike:**
  1. Render a ramp-textured quad into a 128 x 128 RGB565 target with dither off. Read it back with the CPU and check every texel equals its code.
  2. Repeat with the 2D wrap ramp on a sloped quad and check the error against a / k computed on the SH-4.
  3. Time a 32 KB PVR-DMA download with `SB_PDDIR` = 1, on the `0x005F7C00` block, ending on `ISTNRM` bit 11.

  All three are on real hardware; an emulator does not model the PVR's precision or the bus contention.

## Hardware result s2

The spike `demo/psm/spike` (image `psm-ab-spike-s2`) was run by the owner on a real Dreamcast on 2026-11-24. The numbers below are **from photos** of its four result pages (`PXL_20261125_0237*.jpg`), read by two readers. Nothing was captured over serial. One console, one run; the KallistiOS on the disc was revision 568c3404.

Everything marked **measured** is what that run printed. Everything marked **inferred** is reasoning about it and has not been tested. Spike s3 exists to test the inferences; its results are not in yet.

### What was measured

**Clocks.** The KallistiOS ns timer steps by 640 ns and runs on performance counter 0. 1 ms of it is 200,006 CPU cycles on performance counter 1. The two clocks agree, so the times below are real.

**The render target is written and read.**

- A plain magenta quad on a green background, drawn into a 128 x 128 RGB565 target that the SH-4 had filled with 0x1234: the quad's 9,216 pixels are all right, and 7,152 of the 7,168 background pixels are right.
- 16 background pixels still hold 0x1234. Three vblanks later nothing had changed.
- `pvr_scene_begin_rtt` returned 0, both waits ended first time, and the render address read back as the one wanted (0x0119A920).

**The ramp codes, dither off** (`FB_CFG_2` = 0x00000001). "Differ" counts pixels of 16,384 that are not the model's value; the model is `spike/ab_model.h`.

| Test | Pixels that differ | Largest error (codes) | Notes |
|---|---|---|---|
| 10 bits FLAT | 16 | 0 | all 16 are coverage errors on row 127 (first at x 96), holding 0x07E0 |
| 10 bits TILT | 336 | 1 | none hard: all at a rounding step |
| 10 bits OVER | 0 | 0 | the nearer quad won in both send orders |
| 16 bits FLAT | 8,124 | 1 | low 6 bits differ in all 8,124; high 10 bits in 254 |
| 16 bits TILT | 3,472 | 1 | 2,016 of them not at a rounding step; first pixel one code over |
| 16 bits OVER | 6,044 | 1 | low 6 bits only; first pixel one code under; no coverage errors |

- The best-fitting sample offset was 0, 0 in every test: the model's pixel centre (x + 0.5, y + 0.5) fits better than a half-pixel shift either way.
- No test had an error over one code with dither off.

**The ramp codes, dither on** (`FB_CFG_2` = 0x00000009).

| Test | Pixels that differ | Largest error (codes) |
|---|---|---|
| 10 bits FLAT | 5,691 | 33 |
| 16 bits FLAT | 10,161 | 2,081 |

**The copy back by PVR-DMA** (`SB_PDDIR` = 1, the 0x005F7C00 register block, our own register writes).

- `DMAOR` was 0x8201 as the BIOS left it. `CHCR0` read 0x000853C3.
- Both PVR address forms worked: 0x04000000 + offset (64-bit area) and 0x05000000 + offset (32-bit area). Each finished, set the end-of-PVR-DMA status bit, wrote all 16,384 pixels, and matched the SH-4's own read of the same window exactly.
- A 512 KB download of an uploaded pattern came back with 0 of 524,288 bytes different.

**Times.**

| What | Time | Rate or ratio |
|---|---|---|
| PVR-DMA download, 32 KB (smallest of 8) | 913 us (182,743 cycles) | 35.9 MB/s |
| PVR-DMA download, 512 KB (smallest of 4) | 14,803 us (2,960,633 cycles) | 35.4 MB/s |
| SH-4 copy of the same 32 KB, 16 bits a read, from video memory | 8,635 us | 9.5 x the DMA |
| 20,000 scattered two-byte reads from video memory | 12,903 us | 14 x the DMA |
| The same 20,000 reads from the copy in RAM | 2,765 us | |

The page also shows "DMA 512 KB: no run finished" under the 512 KB time. That line was a bug in the program (an `else` bound to the wrong `if`), not a result. It is fixed in s3.

### What this settles, and what it does not

**Settled by the run:**

- A texel reaches a 16-bit render target through REPLACE and point sampling with dither off: the 10-bit FLAT test is exact on every pixel the render wrote.
- The depth test picks the nearer of two overlapping quads in one pass, whichever is sent first.
- The download direction of PVR-DMA works on the console with the BIOS's `DMAOR` and no SH-4 channel set-up of ours.
- Dither must be off for the target pass. KallistiOS has the switch (`vid_set_dithering`).
- Copy first, then compare from RAM. Reading the map straight from video memory per vertex costs more than copying all of it.

**Corrections to the sections above** (they were written before any run):

- Section 1 expected 0.64 ms for 32 KB from Sega's 51.4 MB/s. The console gave 0.91 ms, about 36 MB/s.
- Section 1 said the DMA runs while the SH-4 works on other things. `CHCR0` = 0x000853C3 has the burst bit set (bit 7), with 32-byte units, external request, device to memory. **Inferred:** in burst mode the DMA holds the bus until it ends, so the SH-4 does little else for that 0.91 ms. Count it as CPU time until measured otherwise.
- Section 2 expected 8.2 ms for 20,000 two-byte reads; the console gave 12.9 ms including the loop (the same loop on RAM took 2.8 ms).

**Not settled; inferred from the numbers:**

- **The 16 pixels.** After the first render they held the SH-4's prefill. After the second (the 10-bit FLAT test, which prefilled again and drew a black background) they held 0x07E0, the green background of the render before. So the render before did write them, late. **Inferred:** the PVR holds the last 32 bytes of a render and writes them when the next render begins. If so, a read or a download taken at render-done always has those 16 pixels one render old. Their exact place, whether a taller target moves them out of the map, and whether a screen pass brings them out are s3's E test.
- **Sixteen bits to within one code, not exactly.** The 16-bit FLAT test puts every value in the middle of a step, so a pixel one code off is not rounding at a step: the wrapped coordinate (v = 1024 u, up to several hundred) came out up to one code low. **Inferred:** the PVR keeps about 15 significant bits of a texture coordinate. One fit, not a measurement: the OVER quad's v of 706.68 returning row 42 where 43.5 was asked is what a coordinate kept to 1/32 at that size would give. s3's P test measures it at six sizes.
- **The TILT test is the real case** (a code of a / k through the PVR's perspective division) and it stayed within one code at about 345 codes a pixel.

### What one code is worth

For a map whose depth range is 60 m, one code of 65,536 is about 0.9 mm. Route B's compare bias is about 2 cm, some 20 codes. An error of one or two codes is far inside it. **Inferred:** a 16-bit ramp is more than the compare needs, and a ramp with fewer wrapped bits (1,024 x 16, 14 bits, 3.7 mm a code over 60 m) would do with less strain on the coordinate.

### Integration plan for a real AB source

Nothing here is built. Each step names what it rests on.

1. **The map pass** (rests on: FLAT, TILT, OVER measured).
   - A uniform map first. The casters are drawn into the map target as route A draws them, with the ramp header: point sampled, REPLACE, non-twiddled, no culling, opaque list.
   - Vertex depth z = k, a value that grows toward the light, kept in a narrow range (for example 1 to 1.25) so the code is close to linear in depth.
   - Texture coordinate u = B - A / k, v = 1024 u. Then u k = B k - A is linear over the triangle, which is what the PVR interpolates; the pixel gets B - A / k of its own k. A and B are chosen so the code runs from 1 at the far end to 65,535 at the near end.
   - Background black: code 0 is "empty", and a bigger code is nearer the light. This is route B's convention, so the compare's sense does not change.
   - The code is not linear in depth. The SH-4 computes the same B - A / k for a receiver, and the bias is converted to codes at that depth.
2. **Dither** (rests on: dither on destroys the code, measured; the per-pass switch is s3's D test). Dither off before the map pass's list is closed, back on after the map's render-done and before the screen pass's list is closed.
3. **The held pixels** (rests on: the inference above; s3's E test). Until measured: keep the casters out of the map's last tile row, so the pixels that are one render old are background in both renders.
4. **The copy back** (rests on: the download, measured).
   - After the map's render-done: PVR-DMA from the target into the hook's write slot, 0.91 ms for 128 x 128. The slot's buffer must be 32-byte aligned; its cache lines are purged before and invalidated after.
   - Then `psm_mapsrc_ready` (`demo/psm/psm_mapsrc.h`). The fill callback answers `PSM_MAPSRC_PENDING` when it has sent the map pass.
   - KallistiOS has no download, so the register writes are the client's (the spike's `pd_download`). KallistiOS's own PVR DMA (channel 2, uploads) must not run at the same time; a client that sends vertices by the store queues, as this demo does, does not use it.
5. **The frame's order** (rests on: KallistiOS's source, `pvr_irq.c`: a render to a texture starts as soon as its lists are in and the PVR is free; **inferred**, not timed).
   1. Compare the receivers against the read slot (the map of the frame before).
   2. Dither off. Send the map pass for this frame. Close it.
   3. Open the screen pass and send the world with the colours from step 1.
   4. Wait for the map's render-done. Dither on. Download into the write slot. Mark it ready.
   5. Close the screen pass.

   The hook swaps the slots at the start of the next frame: a one-frame lag, as its delay 1 has it.
6. **The compare** (rests on: route B's compare; the new part is not written). Route B's per-vertex compare with the receiver's key as B - A / k: one reciprocal a vertex more than route B. The map is read as it is: no decode pass, as long as the raw 16 bits are used as the code.
7. **The costs to fill in from s3's C lines** before any claim: the PVR's render time for a caster-like pass at 128, 256 and 512; the download at each size (128: measured, 0.91 ms; 512 x 512 = 512 KB: measured as a download, 14.8 ms, which is most of a frame); the SH-4's send time. Route B's raster time is what the route has to beat, and it has not been timed on a console either.

### Open

- Everything in "Not settled".
- Whether the download can be slower or faster with the PVR rendering at the same time. The plan above downloads between two renders.
- A warped (perspective) map: its depth is not the uniform map's, and the code's form for it is not worked out.
- One console. Nothing is known about another unit or a different video cable or mode.

## Could not read

- KOS `dc/video.c` (`vid_set_mode`, where `FB_W_CTRL` is set).
- KOS `dc/pvr.h`.
- `dma_map_src`'s cache behaviour.
- The DCEmulation threads on render-to-texture and on reading textures back (HTTP 403).
- The 1999 Sega developers conference PDF (over 10 MB).
- No source gives the PVR's texture-coordinate precision, its Gouraud colour precision, or whether 24 or 32-bit render-to-texture works on the hardware.

## Sources

- Sega, "SH4 access" table (1999; a 2023 export, 2 pages), read locally with `pdftotext`: https://www.segaretro.org/images/3/3f/SH4_access990312_e.pdf
- KallistiOS `pvr_dma.h` docs: https://kallistiosunchained.github.io/KallistiOS/pvr__dma_8h_source.html
- KallistiOS source, master, read 2026-11-24 (no pinned commit, so lines may drift): `kernel/arch/dreamcast/hardware/pvr/pvr_dma.c`, `pvr_scene.c`, `pvr_misc.c`, `pvr_irq.c`, `pvr_init_shutdown.c`, `pvr_internal.h`, `pvr_buffers.c`. All at https://github.com/KallistiOS/KallistiOS/tree/master/kernel/arch/dreamcast/hardware/pvr
  - For the correction, also `include/dc/asic.h`, `include/arch/dmac.h` and `hardware/dmac.c`, under https://github.com/KallistiOS/KallistiOS/tree/master/kernel/arch/dreamcast
- Flycast source, local checkout `D:\Workspace-Claude\dc-homebrew\flycast`, commit 6d81b9981 (2026-10-23): `core/hw/holly/sb.h`, `core/hw/holly/holly_intc.h`, `core/hw/sh4/modules/dmac.cpp`, `core/hw/pvr/pvr_sb_regs.cpp`, `core/hw/pvr/pvr_regs.h`, `core/rend/gles/gltex.cpp`, `core/rend/TexCache.cpp`
- Marcus Comstedt, PowerVR notes: http://mc.pp.se/dc/pvr.html
- Snippets only: DCEmulation, "Reading texture from PVR back into SH-4 RAM" https://dcemulation.org/phpBB/viewtopic.php?p=1036645 ; Sega Retro, technical specifications https://segaretro.org/Sega_Dreamcast/Technical_specifications ; AnandTech, PowerVR tile rendering https://www.anandtech.com/show/735/3 ; Khronos forum, depth packing https://community.khronos.org/t/trouble-packing-depth-into-color-texture/59374 ; Reedbeta, texture coordinate precision https://reedbeta.com/blog/texture-gathers-and-coordinate-precision
- This repo: `demo/psm/psm_dc.c` (route A's RGB565 target and its `pvr_wait_render_done`), `include/sh4der_frame.h` (the frame read's load count).
