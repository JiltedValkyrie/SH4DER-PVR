# demo/psm: what the PVR can do, per pixel, and which of it could compare depth

Research only. Nothing here was built or run.

**The goal.**

- A PVR-drawn depth map.
- A per-pixel depth compare that also self-shadows (one part of a mesh shadows another part of the same mesh).
- No depth compare on the SH-4, and ideally no depth raster on the SH-4.

**The owner's constraint.** No modifier volumes. They are listed once below and are not ranked.

**How to read this.**

- **DOC**: Sega's own document says it, read in full in this session.
- **REP**: reported. KallistiOS headers, Flycast (its authors' model, never evidence), forums.
- **INF**: my inference, untested.

"SysArc" below is Sega's *Dreamcast/Dev.Box System Architecture* (preliminary, last update 99/09/03), read as text extracted from the PDF. Section numbers are the document's own; page numbers are its printed page numbers.

## Primary documents

| Document | URL | Covers | Read? |
|---|---|---|---|
| Sega, *Dreamcast/Dev.Box System Architecture* (1999-09, 407 pp.) | https://segaretro.org/images/7/78/DreamcastDevBoxSystemArchitecture.pdf | Everything below: the graphics system (§3), TA parameters, ISP/TSP words (§3.7.9), registers (§8.4.2) | **Yes**, the text of §3 and the §8.4.2 graphics registers. Figures came through partly garbled. |
| Sega, *Notes concerning Drawing System Registers* ("Holly TA&CORE") | https://segaretro.org/images/8/82/Holly_TA%26CORE_e.pdf | Product values and "must not be rewritten while drawing" notes for each register | Yes |
| Sega, *How to change interrupt registers with Holly* (1999-03-10) | https://segaretro.org/images/6/62/Holly_int990310_e.pdf | The interrupt mask procedure | Yes |
| Sega, *SH4 access* table (1999) | https://www.segaretro.org/images/3/3f/SH4_access990312_e.pdf | Bus and DMA timings (see ROUTE_AB_NOTES.md) | Yes |
| Sega, Dreamcast Developers Conference 1999, day 2 | https://www.segaretro.org/images/2/21/DreamcastDevelopersConverence_Day2_1999-03-20.pdf | KAMUI slides | **No** (over 10 MB) |
| Sega Retro, official documentation index | https://segaretro.org/Category:Dreamcast_official_documentation_files | The list of the other Sega PDFs | **No**: a bot wall (Anubis) blocks it. Not bypassed. |
| KallistiOS headers, master | https://github.com/KallistiOS/KallistiOS/tree/master/kernel/arch/dreamcast/include/dc | The API: pvr.h, pvr_header.h, pvr_regs.h, pvr_pal.h, pvr_misc.h, asic.h | Yes (REP) |
| Comstedt, PowerVR notes | http://mc.pp.se/dc/pvr.html | Community register notes | Yes, partly (REP) |
| Flycast source | local checkout `flycast`, commit 6d81b9981 | An emulator's model | Read for cross-checks (REP) |

No PDFs are committed. Links only.

## Per-pixel operations: the index

| # | Operation | What it does per pixel | Source | Mark |
|---|---|---|---|---|
| 1 | Depth compare | 8 modes (Never, Less, Equal, LessEq, Greater, NotEqual, GreaterEq, Always) on 1/w; nearer = larger. | SysArc §3.7.9.1 p.203 | DOC |
| 1a | Depth compare: exceptions | Ignored for auto-sort translucent (always GreaterEq) and for punch-through. §3.7.9.1 says punch-through uses "Less or Equal", §3.4.3 says "Greater or Equal": **the document contradicts itself**. | §3.7.9.1, §3.4.3 | DOC |
| 2 | Z write disable | Compares, does not update Z. | §3.7.9.1 p.204 | DOC |
| 3 | Z buffer | Inside the chip, per 32 x 32 tile. No register stores Z to memory; none appears in the §8.4.2 register list. | §3.4.4, §8.4.2 list | DOC (that no Z-store register exists); INF (that there is no Z output) |
| 4 | Z clear / keep per tile | The Region Array "Z Clear" bit: 1 keeps the tile's Z across multipass list passes of **the same screen**. "Flush Accumulate" 1 keeps the colour in the tile. | §3.7.1.3 p.160, §3.7.7 p.197 | DOC |
| 5 | Background plane | A full-tile polygon with its own ISP/TSP parameters, depth from ISP_BACKGND_D (float, low 4 bits cut). | §3.4.1, §8.4.2 p.347 | DOC |
| 6 | Culling | By sign or size of det (screen area); FPU_CULL_VAL. FPU_PERP_VAL for perpendicular triangles. | §3.7.9.1 | DOC |
| 7 | Translucent sort | Auto-sort: per pixel, back to front, (polygons x overlaps) passes, with "discard". Pre-sort: submission order, the depth compare honoured, **no translucent modifier volumes**. | §3.4.2, §3.4.4 | DOC |
| 8 | Punch-through test | Draws a pixel when texel alpha >= PT_ALPHA_REF, writes alpha 1.0, sorts front to back. §3.7.9.2 says instead "only PIX[A] = 1.0 is drawn": **conflict**. | §3.4.3 p.104, §3.7.9.2 p.210 | DOC |
| 9 | Texture/shading combine | Decal: RGB = T + O, A = Ta. Modulate: RGB = C·T + O, A = Ta. Decal alpha: RGB = T·Ta + C·(1 - Ta) + O, A = Ca. Modulate alpha: RGB = C·T + O, A = Ca·Ta. Use Alpha and Ignore Texture Alpha bits. | §3.7.9.2 p.210 | DOC |
| 10 | Offset colour | Added after the combine (see 9); interpolated with Gouraud. | §3.7.9.1 | DOC |
| 11 | **Bump map unit** | Per pixel: alpha = K1 + K2·sin(s') + K3·cos(s')·cos(r' - q'). S and R are the texel's two 8-bit fields; K1, K2, K3 and Q are 8 bits each, per polygon (the third vertex's offset colour slot). RGB is white; the combine (9) then applies. | §3.4.7.3, §3.4.7.3.1 p.118–119, §3.6.1.3 | DOC |
| 11a | Bump: shading | §3.7.9.1 says flat shading must be used with bump; Fig. 3-21 shows Gouraud base colours. **Conflict.** | §3.7.9.1 p.204, Fig. 3-21 | DOC |
| 12 | Fog, lookup table | **A function of the pixel's own 1/w**: (1/w · FOG_DENSITY) is clamped to 1..255.99, its exponent and mantissa form a 7-bit address into 128 entries, interpolated between neighbours, giving an 8-bit coefficient f. pixel = (1 - f)·colour + f·FOG_COL_RAM. | §3.4.8.1 p.122 | DOC |
| 12a | Fog, mode 2 | Base colour alpha = f, RGB = the fog colour, so the 1/w function goes into the alpha. | §3.4.8.1 p.123 | DOC |
| 12b | Fog, per vertex | f from the offset colour alpha, interpolated. | §3.4.8.2 | DOC |
| 13 | Colour clamp | FOG_CLAMP_MIN / MAX, applied before fog. | §3.7.9.2 | DOC |
| 14 | Blend | DST := SRC·f(src instr) + DST·f(dst instr), clamped on overflow. Factors: 0, 1, other colour, 1 - other, SA, 1 - SA, DA, 1 - DA. Each factor is a 4-vector, so alpha is included. | §3.7.9.2 p.207 | DOC |
| 15 | Accumulation buffers | Tile pixels are 8-bit ARGB. A secondary accumulation buffer, also 32-bit ARGB, is selected by the SRC/DST Select bits; "Flush" polygons copy from it. | §3.4.14 p.134, §3.4.6.1 p.111 | DOC |
| 16 | Modifier volumes | A 1-bit flag per pixel, OR for inclusion volumes and AND for exclusion volumes; two-parameter or intensity mode. **Excluded by the owner.** | §3.4.5 | DOC |
| 17 | Texture filter | Point, bilinear, trilinear (two passes), supersample; mip-map D with an adjust. | §3.4.7, §3.7.9.2 | DOC |
| 18 | Write-out to memory | Formats 0555, 565, 4444 (texture only), 1555 (A = alpha >= fb_alpha_threshold), 888, 0888, 8888. Dither bit: **0 discards the low bits (the default)**. | §3.4.14, FB_W_CTRL §8.4.2 | DOC |
| 19 | **Render to texture** | **Only the 16-bit modes (0555, 565, 4444, 1555)**; 888, 0888 and 8888 "cannot be used for drawing to a texture map". The result is non-twiddled or stride. | §3.4.10 p.127 | DOC |
| 20 | Pixel clip | One rectangle (FB_X_CLIP, FB_Y_CLIP) masks the write-out; the polygons are still drawn. | §3.4.9.2 p.126 | DOC |
| 21 | X/Y scaler, flicker filter | Filters during write-out. | §3.4.11, §3.4.12 | DOC |
| 22 | Palette | 1024 entries; one format a frame; 8bpp uses 2 bank bits, 4bpp 6. **Palette textures are twiddled only.** | §3.6.1.4 p.140, §3.6.4 p.150 | DOC |
| 23 | Bump, VQ, YUV422 textures | **Bump map textures may be non-twiddled or stride**, as RGB textures may. | §3.6.4 p.150 | DOC |
| 24 | YUV converter | The TA turns YUV420/422 macroblocks (16 x 16) arriving by DMA into YUV422 textures. Input conversion only, no compare. | §3.8.1 p.214 | DOC |
| 25 | Frame buffer as a texture | The 32-bit area (frame buffer) cannot be used as a texture; render into the 64-bit area for that. | §3.4.10 | DOC |
| 26 | Fog registers | FOG_* "Must not be rewritten while drawing is in progress". | Holly TA&CORE | DOC |

## Ranked candidates (self-shadow, no modifier volume, compare on the PVR)

Every candidate draws the map from the light with the PVR's own Z test. The nearest caster wins, and receivers are casters too, so self-shadowing comes from the depth compare itself (INF). The SH-4 keeps only the per-vertex work it already does in route A: the light-space coordinates, the map UV, and a depth code per vertex or per triangle.

| Rank | Mechanism | Per pixel | Bits | Doc basis | What would falsify it | Smallest test |
|---|---|---|---|---|---|---|
| **1** (**see the bug list: #26 forces a twiddle**) | **Fog-table map + bump-unit compare** | Map: light-view render of all geometry, black, with the fog table as a depth ramp (12), into a 565 target. Compare: receivers drawn with the map read **as a bump texture** (23). Map code in R (the low byte, from the blue channel), S = 0, K1 = K3 = 0.5, Q = the triangle's own code + 90°. Then alpha = 0.5 + 0.5·sin(map - own), and the punch-through test (8) or a blend keeps or darkens. | Map 5 bits (B5 = R 0..31 = 0..44°, no wrap); the own code is in the same 1.4° steps (one Q step equals one map code) | §3.4.7.3.1 formula, §3.6.4 (bump non-twiddled), §3.4.8.1, §3.4.10 | The receiver code is **per triangle** (flat K/Q, 11a), so the bias must cover each triangle's light-depth span. The S/R decode and the sin/cos tables are untested. The punch-through rule conflicts (8). | One quad textured with a bump texture whose R ramps 0..31, Q swept: render to 4444, read the alpha back, check where it crosses 0.5. |
| 2 | **Fog-table map + saturating-blend compare** (senior's c1) | Receiver term: 1 - R, from **Gouraud vertex colour** (perspective-correct, 9 and 10) or fog. Then add the map M, giving clamp(1 - R + M). Invert and double in the secondary buffer (14, 15) to a 2-level mask. | M 5–6 bits (565); R 8 bits per vertex, interpolated per pixel | §3.7.9.2 blend and clamp, §3.4.6.1, §3.4.14 | Per-layer rounding: truncate or round is not stated. Gouraud precision is not stated. Lead measured 6 bits on a PVR-drawn 565 map. | Already measured (lead's c1). Next: R from vertex colour on a sloped quad. |
| 3 | Rank 1 or 2 with **more map bits** | Two light-view renders: a coarse fog table and a sawtooth table (a fine slice), into two 565 targets. Compare the coarse code, then the fine code where the coarse codes are equal. | About 10–11 bits | §3.4.8.1 (128 interpolated entries), §3.4.10 | The table interpolates across each tooth, giving a wrong zone about 1/128 of the range wide at each jump (INF). It costs twice the map render and twice the compare layers. | Two renders of one sloped quad, both codes read back, the combined code checked against 1/w. |
| 4 | ID map ("priority buffer") + bump cos-window equality | The map holds each triangle's or object's ID. Receivers test map == own ID through cos(r' - q') near 0 (11). | 8-bit IDs | §3.4.7.3.1 | cos is flat near 0: a step of one ID changes alpha by under 1 LSB, so the window must be wide (INF). Also self-shadow acne at triangle joins (the published method's known weakness; REP from memory, not read). | The same quad test as rank 1, with Q = r'. |

**More map bits by other routes, checked and closed:**

- 24 or 32-bit render to texture: not allowed (DOC §3.4.10).
- A palette read from a target: palette textures are twiddled only and a target is not (DOC §3.6.4).
- 4444: 4 bits a channel, worse than 565 for one value (DOC §3.4.14).
- The 1555 alpha bit: fb_alpha_threshold gives a 1-bit "alpha >= constant" at write-out (DOC §8.4.2). That is a compare, but against one constant.

## Lead's checklist

| Item | Finding | Mark |
|---|---|---|
| (a) The bump-map texture | Yes: a per-pixel function of the texel's two 8-bit fields and four per-polygon 8-bit constants, with a signed cos(r' - q') term. Used as a compare, it is rank 1. | DOC (formula); INF (the compare) |
| (b) Z modes, Z to texture | 8 modes, Z write disable, per-tile Z keep within one screen. No path from Z to memory or texture. Depth comes out only through fog (12), which turns the pixel's 1/w into 8 bits in the tile. | DOC; INF (no Z path) |
| (c) YUV and other units | The YUV converter only converts input. The scaler and flicker filter only filter. Nothing compares. | DOC |
| (d) Pixel clip | One rectangle on write-out. No compare. | DOC |
| (e) Modifier volumes | Excluded by the owner (a 1-bit in/out flag per pixel, §3.4.5). | DOC |
| (f) Translucent per-pixel sort | Sorts and blends; produces no compare result. Auto-sort forces GreaterEq. | DOC |
| (g) Blending two renders, more bits | Tile 8 bits ARGB, clamped blend (DOC). The target is 16-bit at most, so a PVR map gives 5–6 bits a pass. Rank 3 slices bits over two renders. | DOC; INF (rank 3) |

## Not known, to test on a console

1. Bump unit: the precision of the sin/cos tables, the clamp of K1 + ..., and whether Gouraud base colour works with bump (11a).
2. The punch-through rule: >= PT_ALPHA_REF, or only alpha 1.0 (8)?
3. Rounding of each blend layer, and of fog interpolation, in the 8-bit tile buffer.
4. Whether a target written as 565 and read back as a bump texture decodes S and R from the expected bits. The arithmetic says R = the low byte (G[2:0]:B[4:0]) and S = the high byte.
5. The fog table can hold either the map's depth ramp or the town's haze, not both. Reloading it between the map render and the screen render is allowed while nothing is drawing (Holly TA&CORE).

## Known bugs, errata and quirks that bear on the shadow chain

The primary list is SysArc §9 "Bug List" (pp.402–407).

- It keys every bug to a Holly revision (REVISION 0x11 and SB_REVISION 0x08–0x0B for Holly 2.2–2.42). The product chip is "CLX2.4 (product)", SB_REVISION 0x0A, per Holly TA&CORE.
- The per-revision marks did not survive text extraction. So for each row it is known that the bug exists, not on which revision.
- Rows that say "Will become part of the specifications" are permanent restrictions.

### Sega's bug list (DOC, SysArc §9)

| Sega # | What | Bears on us | Mark | Smallest test |
|---|---|---|---|---|
| CORE 26 | "Operation hangs if a non-twiddled format bump texture is used. Use only twiddled format for bump textures." Marked "will become part of the specifications", so permanent. | **Rank 1 cannot read the render target as a bump texture directly.** The map must be twiddled first: download (0.93 ms measured at 128²), twiddle on the SH-4, upload. That is a copy, not a compare, but it brings back SH-4 time. §3.6.4 lists bump as non-twiddle-capable: the doc contradicts its own bug list. | DOC | Do not test the hang. Test the twiddled path: a 128² map downloaded, twiddled, uploaded, with the time. |
| CORE 25 | "If the polygon edge is located on the negative side (near 0) of the pixel center position, the gap will be written twice." | An additive compare layer (C1) could add twice on a shared edge. c1 measured rag 0 / btw 0 on a two-triangle quad, so no sign of it there (INF). | DOC | C1 on a 32 x 32 triangle grid; count pixels with a double add. |
| CORE 10, 11, 15 | "Pixel write operation to the FB is not performed, and the previous pixels remain"; End_Of_Render then missing. Worse with the X scaler. | Possibly related to our held last 32 bytes (ROUTE_AB s2/s3: the last row's 16 pixels are written at the next render). INF only; the symptoms differ. | DOC | Already worked round: a 130-row pass for a 128 map (measured, s3). |
| CORE 21 | Missing or misshapen tiles. Fix: multipass with 4 region-array entries a tile (opaque, a dummy full-screen translucent, translucent, an accumulation-buffer flush). | Sega's own multipass recipe, the same mechanism as KOS PR #1594 (below). | DOC | — |
| CORE 22 | Switching translucent sort mode per tile can hang. Fix: do not, or register a full-screen opaque background polygon. | Per-tile presort (type 2 region array) is unsafe; set the sort mode per render. | DOC | — |
| CORE 23 | Where a non-twiddled texture meets a palette texture at a polygon edge, 2 pixels on the non-twiddled side get wrong colours. | Do not mix a non-twiddled map and palette textures in one render's compare pass. | DOC | — |
| CORE 8, 16 | TSP cache faults with SPAN_SORT_CFG bit 16 = 1. | Leave bit 16 at 0. | DOC | — |
| CORE 5 | Modifier volumes do not work on sprites (quads). | Excluded route anyway. | DOC | — |
| Register 1 | "Not possible to read registers, palette RAM, or fog table RAM correctly", with a software workaround (not described). | Do not read the fog table back to check it; keep a RAM copy. | DOC | — |
| SB 7 | The C2DMAXL (ch2-DMA maximum burst) register does not work; settings limited to 0, 1, 2. | Only for the TA upload DMA. | DOC | — |

### Where Sega's doc contradicts itself, or the console differs from it

| Point | Doc says | Other evidence | Mark |
|---|---|---|---|
| Bump textures non-twiddled | §3.6.4 allows it | §9 CORE 26: hangs, twiddled only | DOC against DOC |
| Punch-through rule | §3.4.3: texel alpha >= PT_ALPHA_REF; §3.7.9.2: only PIX[A] = 1.0 | c1 line 4: every pixel drawn whatever the product was (C3 "not working") | DOC against DOC; measured: neither rule seen |
| Punch-through depth mode | §3.4.3 GreaterEq, §3.7.9.1 LessEq | Flycast: "Z Write Disable seems to be ignored for punch-through" (fixes Worms World Party, Bust-a-Move 4, Re-Volt) | DOC; REP |
| Bump shading | §3.7.9.1 flat only; Fig. 3-21 Gouraud | — | DOC |
| PVR-DMA direction | §2.6.9 documents only SB_PDDIR = 0 (system → palette RAM / CORE) | **Measured** (ROUTE_AB s2/s3): SB_PDDIR = 1 copies texture memory to RAM, 0 of 524,288 bytes different, about 35 MB/s against Sega's table 51.4 | DOC gap; measured |
| Render-to-texture formats | §3.4.10: the 16-bit modes only | KOS #1506: RTT takes the main frame buffer's pack mode from `vid_set_mode` (RGB0888 breaks RTT) | DOC; REP |
| Height | TA object lists to 2048 x 480, 600 tiles (MetalliC, quoted in KOS #696) | **Measured**: KOS's 512 target renders only to row 479 | REP; measured |

### Our own console findings (from ROUTE_AB_NOTES.md and compare/NOTES.md, measured)

| Finding | Use |
|---|---|
| The last 32 bytes of a render (16 pixels on the last rendered row) are written at the next render | Render 2 rows taller than the map; copy only the map's rows |
| Dither can be switched per pass (`vid_set_dithering`) | Off for the map and compare passes, on for the screen |
| Ramp codes exact with dither off (REPLACE, point sampling) | The texture-ramp map works |
| C1 compare one level off (+1) | Bias of 1. INF: Flycast models intensity as x·sat/256 (255 → 254); an 8-bit multiply by 1.0 that drops 1 LSB would explain it. Untested. |
| Palette: one value a scene | Route D closed |
| Punch-through test did not act (C3) | Route C3 dropped |

### KallistiOS issues and PRs (REP, read on GitHub 2026-11-24)

| # | What | Bears on us |
|---|---|---|
| PR #1594 (open, 2026-11-23) | TA list continuation: up to 8 list groups a scene; "a later group keeps the earlier groups' depth and colour and is depth-tested against them"; per-group presort | Several compare passes, or opaque after translucent, in one render. Same screen view only. |
| PR #1595 (open) | `pvr_list_flush()`: a DMA-collected list sent ahead of direct lists | Send-order control |
| #1506 (open) | RTT uses the global FB pack mode | Keep the main mode 565 or set RTT's own mode by register |
| #696 (open) | Over 480 lines does not work (TA limit); the CORE could do 2048 x 2048 from hand-built object lists, at SH-4 cost (MetalliC) | Map height ≤ 480 through the TA |
| PR #1375 (open) | TA overflow leaves corrupt CORE lists and lockups; also fixes PVR_TA_OPB_END | Size the map pass's bins with margin |
| #878 (closed) | No PVR DMA busy check led to hangs (`pvr_sq_set32`) | Wait on the DMA before list finish |
| PR #1366 (closed 2026-04) | `pvr_set_presort_mode()` was broken (tile matrix offset) | Needs a KOS from after April 2026 |
| PR #962 (closed 2025-04) | RTT was broken by the TA/render decoupling | Same |

### One INF idea from these sources

**Render the same display list twice.**

- **DOC:** a render starts from PARAM_BASE and REGION_BASE through STARTRENDER (§8.4.2).
- **INF:** the CORE can render an object list already in VRAM again, with other registers. For example, a second fog table gives rank 3's second bit slice without sending the casters to the TA again.
- KOS has no API for it.
- The smallest test: start a second render of the map's lists with a changed fog table and compare the two targets.

## Other chips and units: a short ranked list

Lead asked for a short list, so the reading stopped here. Ranked by use to the shadow chain.

- **DOC**: SysArc, or Sega's SH4 access table.
- **REP**: other sources, not read this session.

| # | Unit and idea | Number (source) | Mark | One-line console test |
|---|---|---|---|---|
| 1 | ch2-DMA (TA path) to upload the twiddled map | 702.6 MB/s to texture memory, 64-bit (Sega table) | DOC | Time a 32 KB DMA to VRAM. |
| 2 | Twiddling a 128² map on the SH-4 (needed by bump bug 26) | Not known: a table-driven loop over 16,384 texels | INF | Time the twiddle of 32 KB in RAM. |
| 3 | Store queues straight into texture memory | 32-byte writes; 38 bus clocks a 32-byte write to VRAM, about 84 MB/s (Sega table) | DOC | Time a 32 KB store-queue upload. |
| 4 | PVR-DMA download (known) | 35 MB/s measured; burst mode holds the bus | measured | — |
| 5 | SH-4 operand cache as RAM: part of the 16 KB operand cache as on-chip RAM, for compare tables or a twiddle table | No number read | REP (Hitachi SH7750 manual, not read) | Random 16-bit reads from cache RAM against RAM, 20,000 of them. |
| 6 | SH-4 DMAC channels 1 and 3 (the access table lists "CPU-ch1/ch3") for memory-to-memory copies while the SH-4 computes. They cannot reorder, so they cannot twiddle. | — | DOC (that the channels exist); INF (the use) | A 32 KB channel 1 copy during a compare loop; time both. |
| 7 | The SH-4 FPU's FTRV / XMTRX (4-way dot product) for route B's per-vertex compare arithmetic and the light-space codes | Known; this library already uses it through SH4ZAM | DOC (SH-4) | — |
| 8 | Re-render one display list (STARTRENDER again) with a new fog table: a second bit slice with no second TA send | — | INF from DOC §8.4.2 | Two renders of one list; compare the targets. |
| 9 | The PVR texture "pages": keep each texture inside one 2048-byte page for full speed. A 128² 16-bit map is 16 pages. | §3.6.5 | DOC | Compare pass time with the map aligned vs. unaligned to 2 KB. |
| 10 | The AICA's ARM7DI as a background worker (baking, decompression) with no frame deadline | AICA 22/25 MHz (SysArc §1.2); 2 MB sound RAM on a 16-bit, 67.7 MHz bus; G2 DMA about 11–26 MB/s (Sega table) | DOC | ARM7 loop over 32 KB in sound RAM; time it. |
| 11 | AICA for anything in the frame: **bandwidth kills it**. An SH-4 read of the AICA "sometimes take[s] 16 µs, during which time the CPU bus, the internal bus (Root Bus) and the G2 bus come to a complete stop"; consecutive writes are limited to 8. | §4.2.2.3 | DOC | — |
| 12 | AICA DSP: 128 steps per 44.1 kHz sample (about 5.6 M steps/s); its outputs go to audio. No render use found. | §4.2.2 | DOC | — |
| 13 | The TA YUV converter: YUV420/422 macroblocks into YUV422 textures | §3.8.1 | DOC | No depth use. |
| 14 | Maple DMA | 0.25 MB/s (Sega table) | DOC | No use. |
| 15 | GD-ROM buffer, SH-4 MMU/TLB, UBC, TMU | Not researched (stopped by the instruction above). The SH-4's performance counters (REP) suit timing the tests. | — | — |

## Sources

- Sega, Dreamcast/Dev.Box System Architecture (1999), https://segaretro.org/images/7/78/DreamcastDevBoxSystemArchitecture.pdf : §3.4.1–3.4.14, §3.6.1.3–3.6.4, §3.7.1.3, §3.7.7, §3.7.9.1–3.7.9.2, §3.8.1, §8.4.2 (FB_W_CTRL, FPU_PERP_VAL, ISP_BACKGND_D/T, ISP_FEED_CFG and the register list).
- Sega, Holly TA&CORE notes: https://segaretro.org/images/8/82/Holly_TA%26CORE_e.pdf
- Sega, Holly interrupt procedure: https://segaretro.org/images/6/62/Holly_int990310_e.pdf
- KallistiOS headers (master) and Flycast (local, 6d81b9981), as REP. See ROUTE_AB_NOTES.md for the readback and DMA side.
- The bug list: SysArc §9 (pp.402–407), and §2.6.9 for the PVR-DMA set-up.
- KallistiOS issues and PRs #1594, #1595, #1506, #696, #1375, #878, #1366, #962: https://github.com/KallistiOS/KallistiOS/issues
- Flycast quirks: `core/rend/gles/gldraw.cpp` (punch-through Z write), `core/hw/pvr/ta_vtx.cpp` (intensity rule, background depth bias).
