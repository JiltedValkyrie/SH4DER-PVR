---
title: Ledger: AICA sound chip and its ARM7DI
tags: [hardware, ledger, aica]
updated: 2026-11-25
sources: [demo/psm/ARM7_TOOLCHAIN_NOTES.md, demo/psm/bench/NOTES.md h2, demo/psm/OFFLOAD_PLAN.md]
---
# Ledger: AICA sound chip and its ARM7DI

**What you will learn:** what is known about the sound chip's processor and memory, for anyone who wants to run their own code on it or use its RAM.
**For:** a programmer thinking of moving work to the sound processor, or of using sound RAM as extra storage.

The AICA has its own ARM processor and 2 MB of its own RAM. The SH-4 reaches that RAM over the slow G2 bus. Each row is one fact; the status legend is in [STRUCTURE.md](../STRUCTURE.md).

**Sources.**

- SysArc = Sega, *Dreamcast/Dev.Box System Architecture* (last update 99/09/03).
- KOS = KallistiOS at commit 568c3404, as read in `demo/psm/ARM7_TOOLCHAIN_NOTES.md`.

## The processor

| ID | Claim | Status | Evidence | Source | Test | Used by |
|---|---|---|---|---|---|---|
| AICA-001 | The sound processor is an ARM7DI. | FACT | SysArc §4.2.2, p. 227 | `demo/psm/ARM7_TOOLCHAIN_NOTES.md` row 1 | — | toolchain choice |
| AICA-002 | ARM7DI is architecture ARMv3 (binutils maps `arm7di` to ARMv3), not ARMv4T. | FACT | binutils 2.45.1 `gas/config/tc-arm.c:30567` | same, row 2 | — | toolchain choice |
| AICA-003 | It has no 16-bit loads or stores (LDRH, STRH, LDRSB), no 64-bit multiply (UMULL, SMULL), no Thumb and no BX. | REPORTED | dc-arm7wrestler, a CPU test run on a console by its author | same, row 6 | Console test 3 in the notes: run one `ldrh` and one `umull` behind a logging undefined-instruction vector. | toolchain, code rules |
| AICA-004 | GCC 9 and later cannot target it. KallistiOS builds its sound driver with GCC 8.5.0, `-mcpu=arm7di`. | FACT | GCC 9 changes; KOS `utils/kos-chain/doc/CHANGELOG.md:111`, `profiles/aica/stable.mk` | same, (1) | — | toolchain choice |
| AICA-005 | The audio block runs at 22.5792 MHz, from the GD-ROM's 33.8688 MHz through a PLL. | FACT | SysArc p. 227 | same, (3) | — | timing |
| AICA-006 | The ARM core itself runs at 22.5792 MHz. | REPORTED (emulator, not evidence) | Flycast `arm7.h` comment | same, (3) | Time a known loop with the AICA timer. | timing |
| AICA-007 | The ARM's wait states on sound RAM. | OPEN | No source read gives a number. | same, (3) | Console test 1 in the notes: N loads timed by the ARM timer, with and without SH-4 G2 traffic. | any ARM offload |
| AICA-008 | Multiply cost on this core. The ARM7TDMI's manual gives MUL 1S + m·I and MLA 1S + (m + 1)·I, m 1 to 4 by the multiplier's size (TRM DDI 0029G §6.20, Table 6-23); that is a later core of the family, not checked for the ARM7DI. | OPEN for this core; REPORTED for the ARM7TDMI | TRM DDI 0029G, read 2026-11-25 ([sound chip page](../research/sound-chip-precompute.md)) | same, (3) | Time MUL and MLA with small and large operands. | DSP-style work |
| AICA-009 | No FPU and no divide instruction. Floats and `/` are library calls. | REPORTED | KOS links `-lgcc` only; the ARM7 family has no FPU | same, (3) | — | code rules |

## Memory, reset and interrupts

| ID | Claim | Status | Evidence | Source | Test | Used by |
|---|---|---|---|---|---|---|
| AICA-010 | Sound RAM is 2 MB of 16-bit SDRAM at 67.7376 MHz (CAS 2, burst read, single write). The ARM, the voices, the DSP and the SH-4 all share it. | FACT | SysArc §4.2.2.4, Table 4-9, p. 231 | same, (3) | — | budgets |
| AICA-011 | The ARM sees sound RAM at address 0 and the registers at 0x00800000. The SH-4 sees them at 0x00800000 and 0x00700000, and may access the AICA area only 4 bytes at a time. | FACT | SysArc Table 4-8, p. 228 | same, (2) | — | drivers |
| AICA-012 | The SH-4 must wait for the G2 FIFO after at most 8 writes to the AICA. | FACT | SysArc §4.2.2.3, p. 230 | same, (2) | — | drivers |
| AICA-013 | ARMRST (register offset 0x2C00, bit 0) holds the ARM in reset at 1 and lets it run at 0. Only the SH-4 can write it. | FACT | SysArc §8.4.5, p. 385; KOS `hardware/spu.c:290-304` | same, (2) | — | loading code |
| AICA-014 | The ARM interrupts the SH-4 by writing 1 to MCIPD bit 5. MCIEB enables it and MCIRE clears it; such interrupts have no levels. The SH-4 sees it as SB_ISTEXT bit 1. | FACT | SysArc §8.4.5, pp. 383-385; SB_ISTEXT p. 294 | same, (2) | Console test 2 in the notes. | ARM to SH-4 signalling |
| AICA-015 | Register offsets MCIEB 0x28B4, MCIPD 0x28B8, MCIRE 0x28BC. | REPORTED | KOS `sound/arm/aica.c:20-25`; SysArc's table does not survive text extraction | same, (2) | — | drivers |
| AICA-016 | KallistiOS never hooks the AICA interrupt. It polls its response queue. | FACT | KOS: `ASIC_EVT_SPU_IRQ` defined, not used | same, (2) | — | drivers |

## Moving data to and from sound RAM

The numbers below are from chip bench h2: one run on one console, 2026-11-25, read from photos `logs/benchmarks/psm-bench-h2/PXL_20261125_072202636.jpg` to `..072218093.jpg`. The write-up is `demo/psm/bench/NOTES.md`, "Hardware result h2", groups 3 and 4.

| ID | Claim | Status | Evidence | Source | Test | Used by |
|---|---|---|---|---|---|---|
| AICA-020 | G2 DMA writes main RAM → sound RAM at 5.7 MB/s at every size from 4 KB to 512 KB (512 KB: 90,816 us). The SH-4 keeps 99-100 % of a register loop meanwhile. | MEASURED | h2, one console | NOTES.md h2 group 3 | bench h2 | sound RAM as storage |
| AICA-021 | G2 DMA reads sound RAM → main RAM at 4.9-5.0 MB/s (512 KB: 104,066 us). It works with KOS's channel 0 settings. | MEASURED | h2 | same | bench h2 | same |
| AICA-022 | The SH-4 writing by store queue (`spu_memload_sq`) is quicker than the DMA, 7.5 MB/s, but the SH-4 does the work. Reading by register reads (`spu_memread`) is 2.5 MB/s, half the DMA. | MEASURED | h2 | same | bench h2 | drivers |
| AICA-023 | One G2 DMA copies sound RAM → texture memory directly: 32 KB in 6,511 us (5.0 MB/s), every byte right, the texture address given as 0x04000000 + offset. A KOS comment says the protection register admits system memory only; that is not what this console did. | MEASURED | h2 | NOTES.md h2 group 4 | bench h2 | streaming textures from sound RAM |
| AICA-024 | Sega gives 11.3 and 14.4 MB/s for AICA DMA ("simulation optimums"). Our console did about half. Plans built on Sega's rate (`OFFLOAD_PLAN.md` Q2) are about twice too hopeful. | DISPUTED (Sega's table against our console) | Sega SH4 access table | `demo/psm/OFFLOAD_PLAN.md:45`; NOTES.md h2 "G2" | — | offload planning |
| AICA-025 | So a 128 × 128 16-bit map (32 KB) made in sound RAM costs 6.5 ms to bring to RAM or to a texture. A per-frame shadow map through sound RAM does not fit a frame. | ESTIMATE (arithmetic on AICA-021/023) | — | NOTES.md h2 "G2" | — | offload planning |

## From the AICA manual

Yamaha / Sega, *AICA Sound-block User's Manual*, ver. 1.00, read 2026-11-25 as extracted text (page = PDF page). What it means for offload work is in [the sound chip as a precompute unit](../research/sound-chip-precompute.md).

| ID | Claim | Status | Evidence | Source | Test | Used by |
|---|---|---|---|---|---|---|
| AICA-026 | The audio clock (MACK) is 22.5792 MHz, so one 44.1 kHz sample is exactly 512 clocks. A random read or write of sound RAM is 4 clocks (177 ns) at the memory controller; what an ARM access adds is not given (AICA-007). | FACT (clock, cycle) + arithmetic (512, 177 ns) | AICA manual p.41, p.44-46 | sound chip page | — | ARM and DSP budgets |
| AICA-027 | The ARM has three 8-bit timers (A, B, C), each counting samples with a prescale of 1 to 128, and interrupts for each timer, for every sample, for DMA end and for a main-CPU write, with levels. Timer A at prescale 1: 256 samples, 5.8 ms (arithmetic). | FACT | AICA manual p.16-19 | sound chip page | count timer A against the real-time clock for 60 s | ARM pacing |
| AICA-028 | The DSP runs 128 steps a sample with no branches; each step is one multiply of a 24-bit value by a 13-bit value (a coefficient, a fraction or data) plus an add. Sound-RAM reads and writes only on odd steps: 64 a sample. A table mode indexes sound RAM by a 12-bit value taken from data. | FACT (manual) + REPORTED (Corlett agrees) | AICA manual p.33-38 | sound chip page | — | DSP ideas |
| AICA-029 | The DSP's 16 outputs (EFREG) and its other buffers can be read and written by the CPUs. No DSP stop bit is documented; its sound is muted by send level 0 and master volume 0. Whether a muted DSP computes and writes as normal is not stated. | FACT (access, mute); OPEN (muted run) | AICA manual p.9, p.27-28, p.33 | sound chip page | run a known DSP program muted; read EFREG and its sound-RAM writes | DSP ideas |
| AICA-030 | A voice's output can be routed into the DSP's inputs (16 MIXS sums) with no CPU work; voices interpolate between samples (method not named; Corlett: linear). A voice's envelope level and position read back one voice at a time. | FACT + REPORTED (linear) | AICA manual p.10-12, p.15, p.28; Corlett "slot" notes | sound chip page | — | voice ideas |
| AICA-031 | The DSP ring buffer's base steps by 1 K words (p.14) or 4 K words (p.32): the manual disagrees with itself. Corlett reports 2 KB steps (1 K words). | DISPUTED | AICA manual p.14, p.32; Corlett "RBP" | sound chip page | write at two bases one step apart; read back | DSP ideas |

## Open

These are also listed in [open-questions.md](../open-questions.md):

- AICA-003: confirm on our own console.
- AICA-006: confirm the ARM clock on a console.
- AICA-007: ARM wait states.
- AICA-008: multiply cost.
- AICA-029: does a muted DSP still compute?
- AICA-031: the ring buffer base step.
- **Found and read 2026-11-25:** the Yamaha / Sega AICA manual (AICA-026 to AICA-031). It has the memory cycle but no ARM wait-state count.
