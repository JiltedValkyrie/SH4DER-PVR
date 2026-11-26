---
title: PVR bugs as features
tags: [research, pvr, holly, errata, bugs, revisions]
updated: 2026-11-26
sources: [third_party/sega-docs/DreamcastDevBoxSystemArchitecture.pdf (§9, pp.402-407), wiki/hardware/ledger-pvr.md, wiki/errata/README.md, demo/psm/PVR_FEATURES.md, demo/psm/compare/NOTES.md]
---
# PVR bugs as features

**What you will learn:** every bug in Sega's Holly bug list (Dev.Box System Architecture, §9, pp.402-407), in our own words; which Holly revisions each one touches; whether it can touch the stock console; what it breaks; and any way we might use it: a look, a trick, a saving, or a mode we would not otherwise have. Each bug is checked against our measured rows.
**For:** anyone who reads Sega's list and wants to know which items matter on a retail console, and the owner choosing a spike. Research, not a recipe.

**Status:** the revision marks are **FACT** (Sega's document). What the two marks mean is our reading (see "How to read the marks"). **Our console reads Holly 2.42** (REVISION 0x0011, SB_REVISION 0x0B: [PVR-123](../hardware/ledger-pvr.md), MEASURED 2026-11-25 on two programs' pages, one console; the name 2.42 is DERIVED from Sega's table). Every use idea is **ESTIMATE** or **OPEN** until it has its own console row.

**Source.** `third_party/sega-docs/DreamcastDevBoxSystemArchitecture.pdf` (sha256 4e610bd9…9835, 2,564,795 bytes, last update 99/09/03; git-ignored, Sega copyright). Cited by page only; no text or table from it is copied here. The marks were read with `pdftotext -lineprinter`, which keeps the revision columns lined up with the rows; the plain layout mode scrambles them.

**Safety rule for this page.** Several bugs hang the PVR, the TA or the bus. None of those is to be tried on the console. Every test proposed here is a read, a count or a picture, and none of them sets up a listed hang.

## The revisions

Sega's list has five columns: Holly 1.5, 2.2, 2.3, 2.4 and 2.42 (p.402, Table 9-1). The 2.4 column also covers 2.41 (p.402, the note under the table). Two registers tell them apart:

| Holly | REVISION (0x005F8004) | SB_REVISION (0x005F689C) |
|---|---|---|
| 1.5 | 0x01 | 0x02 |
| 2.2 | 0x11 | 0x08 |
| 2.3 | 0x11 | 0x09 |
| 2.4 and 2.41 | 0x11 | 0x0A |
| 2.42 | 0x11 | 0x0B |

FACT, p.402 Table 9-1 (register addresses and values are hardware facts, restated).

- **2.4 and 2.41 read the same.** Nothing in Table 9-1 tells them apart (FACT, p.402). That matters for one bug only, CORE 28.
- **Our console: Holly 2.42 (MEASURED reads, DERIVED name).** Suite 3 [s3] printed "HOLLY REV 0011 SB 0B" on two programs' pages (Town corner shading, build 0 [ao0] and Town corner shading, build 1 [ao1], [PVR-123](../hardware/ledger-pvr.md)). REVISION 0x11 says 2.x; SB_REVISION 0x0B is 2.42 in Table 9-1. [PVR_FEATURES.md:114](../../demo/psm/PVR_FEATURES.md) had "CLX2.4, SB_REVISION 0x0A" from a Sega document (REPORTED); that is not this console ([errata O16](../errata/README.md)).
- **One caveat (OPEN).** Sega names a revision by the pair of registers. If a part could carry a 2.42 bus with an older core, CORE 28 is the one row that would show it (fixed on 2.42, present on 2.4); its probe is kept below as an optional check.

## How to read the marks

Each row carries one mark a revision. Our reading (DERIVED from the rows, not stated in the pages we read; p.401 and p.402 have no legend):

- **×**: the bug is present on that revision.
- **○**: it is absent or fixed.
- **－**: not listed for that revision.
- **?**: not known.
- **∆**: partly (one row, CORE 28 on 2.41, where clamping sometimes cures it).

Why this reading holds: SB 1 is a feature that was added (an interrupt). It reads × on 1.5 and ○ from 2.2, so × must mean "missing there" and ○ "fine there". Every row with a "will become part of the specifications" note (a bug Sega kept) reads × in every column. That fits too.

## Which bugs touch our console (Holly 2.42)

Read the 2.42 column. It differs from the 2.4 column in one row only: CORE 28 is fixed on 2.42 (FACT, p.407).

| Group | **Apply** (× on 2.42) | Do not apply (○ on 2.42) | Not chip bugs |
|---|---|---|---|
| Register (p.402) | R8 | R1, R9 | R2 to R7 (documentation fixes) |
| System bus (p.404) | SB6, SB7 | SB1 to SB5 | — |
| CORE and TA (pp.405-407) | C4, C12, C13, C14, C18, C20, C22, C24, C25, C26, C27 | C1 to C3, C5 to C11, C15 to C17, C19, C21, C23, C28 | — |

FACT (the marks, with the reading above) on a DERIVED revision. **14 bugs apply; 3 of them hang** (SB6, C22, C26 as listed; C26 only with bilinear filtering on our console's evidence, PVR-070, PVR-071). Two are already handled by KallistiOS (C20, C27: [PVR-124](../hardware/ledger-pvr.md), [PVR-125](../hardware/ledger-pvr.md)). Four touch only strip buffer mode, which nothing here uses (R8, C12, C13, C14).

The 2.4 column, for a console that reads 0x0A, adds CORE 28 (texture flicker, worst with VQ; partly cured on 2.41 by clamping).

## The bugs, one row each

Columns:

- **No.**: Sega's number and page.
- **What goes wrong**: our words.
- **Revisions**: where it is present (×).
- **Stock console**: whether it touches ours (Holly 2.42, PVR-123): FACT from the marks, on the DERIVED revision.
- **What it breaks**: in practice.
- **Ways to use it**: each idea is tagged ESTIMATE or OPEN, with a safe spike.

### Register bugs (p.402)

| No. | What goes wrong | Revisions | Stock console | What it breaks | Ways to use it |
|---|---|---|---|---|---|
| R1 | Registers, palette memory and the fog table do not read back right | 1.5 | No (FACT: ○ on every 2.x) | On 1.5, any check that reads a register | **Read-back is a check we may trust on 2.x (ESTIMATE).** Read the palette and the fog table back after an upload and CRC them on the result page. That catches a bad upload without a photo. Spike: write a known fog table and a palette bank, read both back, print a CRC beside the CRC of what was sent. Our Bump unit as a subtract [c3b] read of the punch-through reference gave 0xC0 after 192 was written ([PVR-063](../hardware/ledger-pvr.md)), which is the value written. |
| R2-R7 | Mistakes in Sega's own register pages, since corrected: the soft reset bit order, the ISP_FEED_CFG address (0x005F8098, not 0x005F8090), two vertical blank defaults, a parameter FIFO default, and the strip size field's width in FB_R_CTRL | (documentation) | No: not chip bugs | A driver written from the uncorrected pages | **None as a feature.** Check: compare KallistiOS's register names and addresses with the corrected values (a source read, no console). OPEN. |
| R8 | The lowest bit of the strip buffer size does nothing, so strip sizes come only in 32-line units | every revision | Yes (FACT) | Strip buffer mode (see CORE 12 to 14) | None found. |
| R9 | One span sorter register reads bit 8 into bit 16 too | 1.5 | No (FACT) | — | — |

### System bus bugs (p.404)

| No. | What goes wrong | Revisions | Stock console | What it breaks | Ways to use it |
|---|---|---|---|---|---|
| SB1 | No TA end-of-list interrupt for the punch-through list | 1.5 | No: present from 2.2 (FACT) | — | **KallistiOS already uses it:** it enables the punch-through list done event (`ASIC_EVT_PVR_PTDONE`, [`pvr_init_shutdown.c:132-133`](https://github.com/KallistiOS/KallistiOS/blob/568c3404/kernel/arch/dreamcast/hardware/pvr/pvr_init_shutdown.c), FACT at 568c3404). Nothing new. |
| SB2-SB4 | GD-ROM bus (G1) DMA: some sizes never end; a PIO read during a DMA read loses the DMA or hangs the block | 1.5 | No (FACT) | — | None: a retail part is clear. |
| SB5 | Controller bus (Maple) DMA cannot be started by the vertical blank | 1.5 | No (FACT) | — | **On 2.x the hardware can start the controller read at the vertical blank (ESTIMATE).** That would save the CPU a start a frame, a few microseconds at most. Not worth a spike unless the input read shows in a profile. OPEN. |
| SB6 | A Maple command list that holds certain single-word commands can hang the Maple block | every revision | Yes (FACT) | A driver that sends those commands in the normal list form | None. **Do not test.** |
| SB7 | The ch2 DMA (the TA upload) maximum burst register does not work as written; only 0, 1 and 2 are allowed | every revision | Yes (FACT) | Burst lengths other than 0, 1, 2 | **A TA upload speed knob (ESTIMATE).** Spike: time one fixed TA DMA upload at burst settings 0, 1 and 2 only, never another value, on the cycle counter, and compare with the KallistiOS default. Safe: all three are allowed values. Ties to the bus rows ([ledger-bus](../hardware/ledger-bus-dma.md)). |

### CORE and TA bugs (pp.405-407)

| No. | What goes wrong | Revisions | Stock console | What it breaks | Ways to use it |
|---|---|---|---|---|---|
| C1-C3 | A ghost pixel beside a bright one; lines when the frame buffer sits in the first 4 MB; vertices sometimes drawn wrong | none of the five (○ in every column) | No (FACT) | Only parts before 1.5 | — |
| C4 | PAL non-interlaced vertical sync is 3 lines wide; the video encoder makes it 2.5 | every revision | Yes (FACT) | Nothing: the encoder makes up for it | None. |
| C5 | Modifier volumes do not act on sprites (quads) | 1.5 | No: they work on 2.x (FACT) | — | **A modifier volume over sprite particles, signs or text quads (ESTIMATE).** Our notes ruled this route out as a bug ([PVR_FEATURES.md:129](../../demo/psm/PVR_FEATURES.md)); on 2.x it is not one. Uses: a shadow volume darkening sprite smoke, or a torch cone that lights sprite dust. Spike: one sprite quad under one modifier volume; count the pixels changed, as Compare by blending [c1] did for triangles ([PVR-044](../hardware/ledger-pvr.md)). |
| C6 | Lines shift right on screen | 1.5 | No (FACT) | — | — |
| C7 | Vertex types 11 to 14 hang the TA unless the strip length is 1 or 2 | 1.5 | No (FACT) | — | None needed. Any strip length works on 2.x. |
| C8-C11 | TSP cache faults, FIFO faults, missing pixel writes and a missing end-of-render interrupt | none of the five | No (FACT) | — | — (see the cross-check on PVR-021 below) |
| C12 | In strip buffer mode the first Hazard interrupt comes too early | every revision | Yes (FACT) | Strip buffer mode: ignore the first one | **Strip buffer mode (OPEN).** The PVR renders into a band a few times 32 lines high that video scans out, in place of a whole frame buffer. The use is video memory: a 640 × 480 16-bit frame is 600 KB, and a band of 64 lines is 80 KB. The cost is racing the beam. Spike, safe: render a still picture in strip mode with the first Hazard ignored, photograph it, CRC the band. Large job: KallistiOS has no strip mode. |
| C13 | In strip buffer mode, a band size that divides the screen an odd number of times puts wrong pixels at the top left | every revision | Yes (FACT) | Strip buffer mode | A glitch look at best; no use found. |
| C14 | X clipping in strip buffer mode draws wrong pixels at the right edge | every revision | Yes (FACT) | Strip buffer mode with an X clip | None. |
| C15 | Pixels sometimes go undrawn, and the end-of-render interrupt with them | 1.5 | No (FACT) | — | — |
| C16 | TSP cache fault with a span sorter bit set; the render hangs | 1.5 | No (FACT: ○ on 2.x) | — | Leave the bit clear anyway: C8 is the same bit. **Do not test.** |
| C17 | A user tile clip the size of the screen or smaller can hang the TA | 1.5 | No: user tile clips are safe on 2.x (FACT) | — | **A free scissor (ESTIMATE).** A user tile clip limits a polygon to a box of tiles, so the PVR skips the tiles outside it. Uses: split screen, a picture-in-picture inset, a portal, a mirror. Spike: the town's inset drawn with and without a user clip; the PVR render time from `pvr_get_stats`. KallistiOS writes the clip bits into every header it compiles ([`pvr_prim.c:42, 278, 372`](https://github.com/KallistiOS/KallistiOS/blob/568c3404/kernel/arch/dreamcast/hardware/pvr/pvr_prim.c)). |
| C18 | A change of user tile clip takes effect one polygon (one strip) too soon | every revision | Yes (FACT) | Anything that switches clips mid-list | **A gotcha with our guard (FACT + ESTIMATE).** Sega's fix is a dummy polygon before the switch. A dummy with no screen area would be dropped by our own `sh4der_tri_area_ok` ([Send checks [guard]](../../include/sh4der_guard.h)) before it is sent, so the dummy must have area and sit off screen or fully clipped. Spike: two clips in one list, with and without a dummy; photograph the seam. **suite 7 [s7] (PVR-155):** with no guard, Room shading, four ways [ao4], build h's last polygon of each half stayed in its own half (one program, the opaque list, the divider still): not seen there, MEASURED; not a refutation. |
| C19 | An ISP flag is never cleared, so nothing draws | 2.2 | No (FACT) | — | — |
| C20 | The end-of-video render-done interrupt is sometimes missing; use the end-of-TSP one | 2.2 to 2.42 | Yes (FACT) | A wait on that interrupt | **Already handled.** KallistiOS waits on `ASIC_EVT_PVR_RENDERDONE_TSP` ([`pvr_init_shutdown.c:134-135`](https://github.com/KallistiOS/KallistiOS/blob/568c3404/kernel/arch/dreamcast/hardware/pvr/pvr_init_shutdown.c); [ROUTE_AB_NOTES.md:85](../../demo/psm/ROUTE_AB_NOTES.md)), which is Sega's fix. So the late wake we measured ([PVR-117](../hardware/ledger-pvr.md), [K8](../errata/README.md)) is the scheduler tick, not this bug. |
| C21 | Missing or misshapen tiles; Sega's fix is four region array entries a tile (multipass) | 2.2, 2.3 | No: fixed on 2.4 (FACT) | — | **The fix is the feature.** The multipass region array (opaque, a dummy translucent, translucent, an accumulation flush) is the mechanism that lets one tile be rendered in several passes into one accumulation buffer ([PVR_FEATURES.md:125](../../demo/psm/PVR_FEATURES.md)). On 2.4 it is not needed as a fix, only as a tool. OPEN. |
| C22 | Switching the translucent sort mode per tile can hang the render | every 2.x | Yes (FACT) | Per-tile presort or autosort | None safe. Sega's second fix (a full-screen opaque background) might make a per-tile switch safe, which would allow autosort only in tiles with particles. That is a hang test. **Do not test on the console.** |
| C23 | Where a non-twiddled texture meets a palette texture at an edge, two pixels on the non-twiddled side go wrong | 2.2, 2.3 | No: fixed on 2.4 (FACT) | — | Our advice not to mix them in one compare pass ([PVR_FEATURES.md:127](../../demo/psm/PVR_FEATURES.md)) is not needed on 2.4. Not needed on our console (2.42, PVR-123). |
| C24 | With the Y scaler enlarging and filter on, 32 pixels at the left end of the last line of the last tile come out wrong | every 2.x | Yes (FACT) | Vertical upscale with the filter | **Render fewer lines and let video stretch them (ESTIMATE).** For example, a 640 × 240 render shown at 480 lines halves the pixels the PVR fills and the frame buffer. Sega's fixes are one line fewer, or a dummy last tile placed off the screen at Y = 0. Spike: the town at 480 lines against 240 lines with the scaler; PVR render time and a photo of the bottom-left corner. |
| C25 | When a polygon edge falls just on the low side of a pixel centre, the pixel in the gap is written twice | every 2.x | Yes (FACT) | Additive or blended layers: a seam that adds twice | **An edge line for free (ESTIMATE).** A low-alpha additive mesh lights its own shared edges where pixels double, a wireframe glow for the holo or visor looks with no line pass. **And a risk:** our additive compare layers could add twice on a shared edge. Compare by blending found no doubled pixel on its two-triangle case ([PVR_FEATURES.md:123](../../demo/psm/PVR_FEATURES.md)), so the doubling needs edges at particular sub-pixel places. Spike: an additive fan at alpha 1/255 whose shared edges sweep sub-pixel offsets; count pixels that read 2. Safe: no hang. |
| C26 | A bump texture in non-twiddled form hangs the render | every 2.x | Yes (FACT, as listed) | Reading a render target straight as a bump map | **Our console says: not with point sampling.** Bump unit as a subtract used a non-twiddled render target as the bump texture, point sampled, with no hang and the same result as twiddled ([PVR-070](../hardware/ledger-pvr.md), MEASURED one run). skmp's model hangs only with bilinear filtering ([PVR-071](../hardware/ledger-pvr.md), REPORTED). **Use:** a bump map drawn by the PVR each frame (ripples, footprints, a moving dent) and read back as bump with no twiddle pass (ESTIMATE). **Do not test the bilinear case on the console.** |
| C27 | The Group_En bit in a global parameter does not cover the user clip bits | every revision | Yes (FACT) | A header that leaves the clip bits to the group | None. KallistiOS sets the clip bits in every header ([`pvr_prim.c:42, 278, 372`](https://github.com/KallistiOS/KallistiOS/blob/568c3404/kernel/arch/dreamcast/hardware/pvr/pvr_prim.c), FACT). |
| C28 | Texture data flickers (worst with VQ) and texture memory can even be damaged | 2.2, 2.3, 2.4; partly on 2.41 (clamping sometimes cures it); not 2.42; 1.5 unknown | **No** (○ on 2.42, PVR-123); yes on a 2.4 part | VQ textures; texture memory contents | **A core check, not a look (OPEN).** We know of no use for damaged memory. Our console reads 2.42, where Sega marks it fixed; the probe would only confirm that the core matches the bus value (the caveat above). Spike, safe: a still VQ texture drawn for 600 frames, wrapped and then clamped; CRC the texture memory before and after, and a frame CRC each second, all on one result page. Any change is this bug. |

## Cross-checks with our own rows

- **[PVR-021](../hardware/ledger-pvr.md) (the held last 32 bytes) is not Sega's C10, C11 or C15 on a 2.x part.** C10 and C11 are ○ in every column and C15 is × on 1.5 only (FACT, pp.405-406). Our [errata E8](../errata/README.md) and [PVR_FEATURES.md:124](../../demo/psm/PVR_FEATURES.md) linked them; that link does not hold for a 2.x console. The nearest listed bug is C24: the last line of the last tile, 32 pixels, the place where ours shows. But C24 needs the Y scaler and a filter, which a render target does not use. Same place, so far not the same cause (OPEN). **Spike, safe:** try C24's second fix, a dummy last region array entry off the target. If the held bytes move into the dummy tile, the 2 extra rows ([PVR-021](../hardware/ledger-pvr.md)'s fix) are no longer needed.
- **[PVR-063](../hardware/ledger-pvr.md) read back the value written.** 0xC0 is 192. R1 (no read-back) is a 1.5 bug, so the read is what a 2.x part should give. [Errata E10](../errata/README.md) said we saw R1. We did not.
- **Punch-through ([PVR-060](../hardware/ledger-pvr.md), [PVR-061](../hardware/ledger-pvr.md), [PVR-064](../hardware/ledger-pvr.md), [PVR-120](../hardware/ledger-pvr.md); errata E1 and E2).** No bug on pp.402-407 is about the punch-through alpha test, its product or its depth mode. The only punch-through row is SB1, an interrupt that 2.x has. So our measured rules and the E1/E2 contradictions are not explained by the bug list (FACT by absence).
- **Bump ([PVR-070](../hardware/ledger-pvr.md), [PVR-071](../hardware/ledger-pvr.md), errata E3, E4, E5).** C26 is the only bump row, covered above. Nothing in §9 touches the bump level curve or its shading (E4, E5).
- **[PVR-023](../hardware/ledger-pvr.md) (exact texel copy with dither off):** no listed bug touches it.
- **The depth union ([PVR-127](../hardware/ledger-pvr.md), suite 3) is not in Sega's list either.** A depth test "greater" with write, used to darken once a direction in a translucent render-to-texture pass, gave 477 to 2,541 wrong texels; no row on pp.402-407 covers it (FACT by absence). Why is OPEN (Q76).
- **The modifier volume ([PVR-044](../hardware/ledger-pvr.md)):** C5 (no volumes on sprites) is 1.5 only; triangles were never listed.
- **The render-done wait ([PVR-117](../hardware/ledger-pvr.md), K8):** KallistiOS already uses Sega's fix for C20; the 10 ms is the scheduler.
- **[PVR_FEATURES.md](../../demo/psm/PVR_FEATURES.md) lines 125 and 127** treat C21 and C23 as live. Both are ○ on 2.4 (FACT, p.406-407). Our console reads 2.42 (PVR-123), so neither applies; PVR_FEATURES.md now says so.

## Ideas worth a spike, ranked

All ESTIMATE until run. None sets up a listed hang.

1. **The boot read of REVISION and SB_REVISION: done** (suite 3: 0x0011 and 0x0B, Holly 2.42, PVR-123).
2. **C24, the Y scaler at 240 lines.** Half the fill, half the frame buffer; the only cost is 32 known pixels.
3. **C17, user tile clips as a scissor** for the inset and for any split view.
4. **C25, the shared-edge sweep.** It answers both the risk to our additive layers and the free edge-glow idea.
5. **The PVR-021 dummy-tile test.** It could free 512 bytes and a step of every target pass.
6. **C5, a modifier volume on sprites.**
7. **C28 as a probe**, optional: only to confirm the core matches the 2.42 bus value.

## Dead ends

- **Hangs as features:** SB6, C16, C22 and C26 with bilinear filtering. No test on the console.
- **1.5-only bugs:** R1, R9, SB1 to SB5, C5 to C7, C15 to C17. A retail part is clear of them; what they block on 1.5 (read-back, sprite volumes, user clips) is simply available.
- **C1 to C3 and C8 to C11:** absent on all five listed revisions.

## Sources

- Sega, *Dreamcast/Dev.Box System Architecture*, §9 Bug List: p.402 (Table 9-1, the revision registers; the register bug table), p.404 (the system bus table), pp.405-407 (the CORE and TA table). Local copy `third_party/sega-docs/DreamcastDevBoxSystemArchitecture.pdf`, cited only.
- KallistiOS 568c3404 (2026-10-11): `kernel/arch/dreamcast/hardware/pvr/pvr_init_shutdown.c:132-135` (the PTDONE and RENDERDONE_TSP events), `pvr_prim.c:42, 278, 372` (user clip bits in every compiled header).
- Our rows: [ledger-pvr](../hardware/ledger-pvr.md) PVR-021, PVR-023, PVR-044, PVR-060 to PVR-064, PVR-070, PVR-071, PVR-117, PVR-120; [errata](../errata/README.md) E1 to E5, E8, E10, K8; [PVR_FEATURES.md](../../demo/psm/PVR_FEATURES.md):114-137.
- skmp's polly2-rtl, read for PVR-071 ([POLLY2_NOTES.md](../../demo/psm/POLLY2_NOTES.md) (6)); cited only, credit skmp.
