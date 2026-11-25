---
title: The sound chip as a precompute unit
tags: [research, aica, arm7, dsp, offload]
updated: 2026-11-25
sources: [demo/psm/OFFLOAD_PLAN.md, demo/psm/ARM7_TOOLCHAIN_NOTES.md, wiki/hardware/ledger-aica.md, AICA manual (Yamaha/Sega), ARM7TDMI TRM, Corlett's AICA notes (cited only)]
---
# The sound chip as a precompute unit

**What you will learn:** what the sound chip's three engines (the ARM7, the DSP and the 64 voices) can and cannot compute for shading and shadows, beyond what `demo/psm/OFFLOAD_PLAN.md` already says, and the console test for each idea.
**For:** whoever plans offload work, person or agent. Research, not a recipe.

![The sound chip as an offload unit: the SH-4 reaches sound RAM, the ARM7DI, the DSP and the 64 voices only over the G2 bus, measured on one console at 5.7 MB/s in and 4.9 to 5.0 MB/s back (also 5.0 MB/s straight to texture memory), so a 32 KB map costs about 6.5 ms back (ESTIMATE); only rare, small jobs fit: the ARM's timer A as a pacing clock (S2), ADPCM storage for smooth bulk tables (S3), a muted DSP blur in the background (S1, rests on Q59) and the plan's ground-shadow rebake, while ARM wait states (Q28), multiply timing (Q29), the muted DSP (Q59) and the ring base step (Q60) are OPEN.](../images/aica-offload.svg)

**Status: research only.** Nothing here was built or run. Facts are FACT (the vendor manual, page given) or REPORTED (Corlett's notes, a third party's reverse engineering, cited by heading only; nothing copied). Ideas are ESTIMATE or OPEN. The only measured numbers are the G2 rates already in the ledger (AICA-020 to AICA-023).

## What the offload plan already settled

Read `OFFLOAD_PLAN.md` first; it is not repeated here. In short:

- The ARM7 is a slow background thread; nothing per frame fits.
- Everything crosses the G2 bus: 5.7 MB/s into sound RAM, 4.9-5.0 MB/s back (AICA-020, AICA-021). A 32 KB map costs 6.5 ms back (AICA-025).
- The one job that fits: rebake the static town's ground sun-shadow on the ARM7 when the sun moves, and bring it back by G2 DMA.
- The DSP does not fit per-vertex work.

This page adds what the vendor manual says, which the plan had listed as "not read, wanted".

## Sources

| Key | Source | How it was read |
|---|---|---|
| AICA | Yamaha / Sega, *AICA Sound-block User's Manual*, ver. 1.00 (history to July 1997), 53 pages, segaretro.org/images/d/de/AICA_E.pdf | Fetched and read as extracted text. Superscripts are lost in the text layer (see "Reading notes"). Pages below are PDF pages. |
| TRM | ARM, *ARM7TDMI Technical Reference Manual*, DDI 0029G, documentation-service.arm.com | Fetched; cycle tables read. This is the later ARM7TDMI core, not the ARM7DI in the AICA. |
| Corlett | Neill Corlett, *Yamaha AICA notes* (a reverse-engineering text) | Read from the local reference copy under `third_party/skmp/nulldc3/docs/`, which has no clear licence: cited by heading, nothing copied. REPORTED. |

## New facts from the manual

These extend the [AICA ledger](../hardware/ledger-aica.md) (rows AICA-026 to AICA-031 were added from this reading).

**Clocks.**

- The audio block's clock is 22.5792 MHz (AICA p.41), and one sample at 44.1 kHz is exactly 512 of its clocks (22,579,200 / 44,100: arithmetic). FACT + arithmetic.
- One DSP step is therefore 4 clocks (512 clocks / 128 steps). ESTIMATE (arithmetic on FACT figures).
- A random read or write of sound RAM is 4 clocks (177 ns) at the memory controller (AICA p.44-46). How many of those an ARM access costs is not given: AICA-007 stays OPEN.

**The ARM7's own timers and interrupts** (AICA p.16-19). FACT.

- Three 8-bit timers (A, B, C), each with a prescale of 1 to 128 samples. Timer A at prescale 1 overflows every 256 samples: 5.8 ms, 172 Hz (arithmetic).
- A one-sample interrupt (every 512 clocks), a DMA-end interrupt, a "main CPU wrote" interrupt, and others, routed to the ARM with levels.
- The plan covered only the ARM → SH-4 interrupt (AICA-014). The ARM can also pace itself with no SH-4 poll.

**The DSP** (AICA p.33-38; Corlett "DSP PROGRAM" agrees). FACT unless marked.

- 128 steps a sample, no branches. Each step: one multiply of a 24-bit value by a 13-bit value, shifted, plus an add. At most 5.64 M multiply-adds a second (arithmetic).
- The 13-bit operand can come from a coefficient, from a fraction register, or **from data** (a value latched from the inputs). So data × data is possible.
- Sound-RAM reads and writes only on odd steps: at most 64 a sample (2.8 M a second).
- **Table mode:** an address can be a base plus a 12-bit index taken from data, so a lookup indexed by a computed value is native. A neighbour fetch with a fraction gives a linear-interpolated read: 2 memory steps and 1 multiply-add.
- Inputs: 32 values read from sound RAM, 16 sums of voice outputs (MIXS), 2 CD audio inputs. Outputs: 16 values to the mixer (EFREG, which the CPUs can read, AICA p.9, p.33), and writes to sound RAM.
- No stop bit is documented; the program runs every sample. Its sound can be muted by send level 0 and master volume 0 (AICA p.27-28). Whether a muted DSP still computes and writes as normal is OPEN.
- The program (128 × 64 bits) and the coefficients can be rewritten while it runs (AICA p.8-9, p.33).

**The voices** (AICA p.9-29). FACT unless marked.

- 64 voices, each playing 16-bit, 8-bit or 4-bit ADPCM samples from sound RAM at a set pitch, with interpolation (AICA p.10); Corlett reports it as linear (REPORTED).
- A voice's output can be routed into the DSP's inputs (MIXS) with no CPU work (AICA p.11-12, p.28).
- A voice's envelope level and play position can be read back, one voice at a time, by register reads over G2 (AICA p.15).
- A voice can play noise instead of a sample (AICA p.9); its seed and repeatability are not documented (OPEN).

**Cycle costs of the ARM7 family** (TRM §6.20, Table 6-23). REPORTED for this core: they are the ARM7TDMI's, and the ARM7DI's own table was not read.

- A load costs 3 cycles plus memory waits; a store 2; MUL 1 + m and MLA 2 + m, where m grows with the size of the multiplier (1 to 4).
- So AICA-008 now has a family figure to test against, not a number for our core.

## Ideas

Rates used: G2 5.0 MB/s for a round trip (AICA-020, AICA-021), unless a row says otherwise. Every number in this table is ESTIMATE; the arithmetic is shown.

| # | Idea | Engine | Fits? | Why | Numbers (ESTIMATE) | Console test |
|---|---|---|---|---|---|---|
| S1 | **Silent DSP blur** of a static map kept in sound RAM: soft shadow edges for the baked ground map, a bloom table | DSP | **Background only** | Costs no SH-4 or ARM time, but the map still crosses G2, and the SH-4 could blur the same data quickly | 4-5 steps and 2 memory steps a texel: 25-32 texels a sample, 1.1-1.4 M a second. 128 × 128, one axis: 12-15 ms of DSP time (two axes double it). G2: 32 KB in 5.7 ms, out 6.7 ms | Program a 3-tap blur with linear data, muted; compare with the SH-4's result; time it with the one-sample interrupt |
| S2 | **ARM pacing clock** for the plan's background bake | ARM timers | **Yes, as a mechanism** | Timer A at 172 Hz gives the bake a tick with no SH-4 poll; the one-sample interrupt (44,100 a second) is too frequent for a heavy handler | 5.8 ms a tick | Count timer A overflows against the real-time clock over 60 s; time the ARM's interrupt entry and return |
| S3 | **ADPCM storage for bulk smooth tables** (height fields, sun tables), decoded on the SH-4 after the G2 read | voices' format, SH-4 decoder | **Yes, as lossy storage** | 4 bits a value instead of 16 cuts G2 bytes by 4. Not for shadow bits or masks: the error is in every value | 128 KB of 16-bit table: 26 ms of G2; as 4-bit: 6.5 ms. The SH-4's decode time is unmeasured | Encode the real height map on the host; measure the shading error; time both DMAs and the decode |
| S4 | **The voices as a hardware decompressor** into sound RAM: a voice decodes ADPCM, its output goes through MIXS into the DSP, the DSP writes it to sound RAM; then a G2 DMA to texture memory (AICA-023) | voices + DSP | **Crazy; low use** | Decodes with no CPU, but only at the sample rate: one value a voice a sample, at most 16 voices on 16 MIXS inputs, and the result is 16-bit, so the G2 trip is 4 times longer than the compressed data's | 16 × 44,100 = 0.7 M values a second at most. A 32 KB result takes 6.5 ms of G2 to texture memory | Key one ADPCM voice into one MIXS; write it with MWT; compare with the host decoder bit for bit; then try 16 |
| S5 | **DSP table lookup with interpolation** for a static curve (Fresnel, a highlight falloff) | DSP | **No** per frame | Per-frame per-vertex data is bound by G2 | 32 lookups a sample: 1.4 M a second. But 6,600 vertices × 4 bytes is 26 KB: 5.3 ms of G2 a frame | None needed unless a static set appears |
| S6 | **Per-vertex sun visibility as day-interval tables** for static geometry (the occlusion-interval idea of `GPU_GEMS_NOTES.md`, ch. 13) | ARM | **Background; not better than the plan's ground map** | Storage is small; the ray marches are the cost | 8,700 vertices × 4 times × 2 bytes = 70 KB, moved once in 14 ms. Ray marching: 8,700 × 64 steps × 8 instructions = 4.5 M instructions a time sample, 0.3-0.9 s at 5-15 MIPS | The plan's bench B1 first, then this beside the plan's height-field bake |
| S7 | Sun-path table, fog ramp, palette animation, noise for dither or grain | ARM or voices | **No** | Small tables the SH-4 makes faster than a G2 trip; the voices' noise is not documented as repeatable, and `sh4der_film.h` needs a pattern that is a pure function of time and seed | 1-9 KB: 0.2-1.7 ms each way | None |
| S8 | The voices' envelope or LFO as an arithmetic unit | voices | **No** | The envelope is readable one voice at a time by register reads over G2; the LFO wave has no read port | — | None |
| S9 | The DSP as a vertex transform (the plan's rejected idea, checked again against the manual) | DSP | **No** | The manual's step and memory counts agree with the plan's 9 vertices a sample; the G2 trip decides | 158 KB a frame: 28-32 ms of G2 | None |

**Strongest, in order:** S2 (makes the plan's bake cadence exact, no SH-4 cost), S3 (a 4:1 cut in G2 bytes for smooth bulk data, if the decode is cheap), S1 (the only real compute the sound chip adds, background only).

**The honest summary:** the manual confirms the plan. The sound chip has real arithmetic (a 5.6 M multiply-add a second DSP, 64 interpolating voices), but every result for the picture must cross a 5 MB/s bus. Only work that is done once, or rarely, and kept small, pays.

## Reading notes

- **Superscripts lost in extraction.** The manual's text layer prints the pitch formula as "(210 + FNS)/210". The same text says one step of FNS is 1.69 cents, and its table gives FNS 60.9 for a semitone; both fit a base of 1,024. The "210" is 2 to the power 10 with the superscript flattened, not an error in the manual. Read formulas from the page image, not the text layer.
- **Ring buffer base granularity.** The manual says "1 K word" on one page (p.14) and "4 K word" on another (p.32). Corlett's figure (one step = 2 KB = 1 K words) agrees with the first. DISPUTED until a console test writes at two bases and reads back.
- **Clock pin.** The manual's pin table says 33 MHz for the master clock; the system manual gives 33.8688 MHz through a PLL to 22.5792 MHz (AICA-005). Both arrive at the same audio clock.
- **Filter Q.** The manual's filter section gives Q as 0 to 48 dB; its register table and revision history give −3 to 20.25 dB. The register table is the later text.
- **RAM access blocks.** A register (MRWINH) can stop the DSP, the voices, the ARM or the SH-4 from reaching sound RAM. The manual says the ARM's own block cannot be undone by the ARM; Corlett reports that setting it from the ARM locks the ARM up. Do not set that bit from the ARM.

## Open

New questions (numbered in [open questions](../open-questions.md)):

- Q59: does a muted DSP still compute and write as normal (S1, S4; AICA-029)?
- Q60: the DSP ring buffer's base step (AICA-031).
- Q28 (sharper now that the memory cycle is known): how many clocks an ARM load from sound RAM really takes, with and without voices playing (AICA-007).
- Q29: does the ARM7DI have the ARM7TDMI's multiply timing (AICA-008)?

## Related

- `demo/psm/OFFLOAD_PLAN.md` (the plan this extends), `demo/psm/ARM7_TOOLCHAIN_NOTES.md`
- [AICA ledger](../hardware/ledger-aica.md)
- [Bus and DMA ledger](../hardware/ledger-bus-dma.md)
- [Research notes](README.md)
