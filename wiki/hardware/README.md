---
title: The hardware ledger
tags: [hardware, ledger]
updated: 2026-11-26
sources: [AGENTS.md "Wiki and the hardware ledger", wiki/STRUCTURE.md]
---
# The hardware ledger

**What you will learn:** where each hardware fact is kept, how to read a ledger row, and how to add one after a console run.
**For:** anyone who needs a number about the Dreamcast, and any agent that has just run something on a console.

The ledger is the project's record of what the Dreamcast does. **A console result that is not in the ledger is not kept** (`AGENTS.md`, "Wiki and the hardware ledger").

## The pages

| Page | Prefix | What it holds |
|---|---|---|
| [PVR](ledger-pvr.md) | PVR- (and OBS- for owner-observed runs with no figures) | Fill and render time, render to texture, per-pixel compare tricks, punch-through, textures, coverage; the owner's observed runs |
| [SH-4](ledger-sh4.md) | SH4- | Clock, cache, maths units, transforms, copies, reading video memory |
| [Buses and DMA](ledger-bus-dma.md) | BUS- | The TA send, uploads, PVR-DMA, the DMAC, the G2 bus |
| [AICA](ledger-aica.md) | AICA- | The sound chip's ARM7DI, sound RAM, G2 DMA to and from it |
| [Video memory and the frame](ledger-video.md) | VID- | Frame and target sizes, the frame read, hardware-only faults |

## Reading a row

| Column | Meaning |
|---|---|
| ID | Stable name. Never reuse a number, even when a row is withdrawn. |
| Claim | One fact, in one or two plain sentences. |
| Status | MEASURED, FACT, REPORTED, OBSERVED, ESTIMATE, DISPUTED or OPEN (legend in [STRUCTURE.md](../STRUCTURE.md)). |
| Evidence | The run, its date and the number of consoles; or the document section. |
| Source | File and line where the number is written up. |
| Test | The program that proves it, or the test that would. |
| Used by | Pages and code that depend on it. |

Three rules matter most:

- **Measured and estimated numbers are never mixed in one row.** An arithmetic result from a measured number is its own ESTIMATE row.
- **An emulator's number is never MEASURED.** It is REPORTED, "emulator, not evidence".
- **An OBSERVED row has no figures.** The owner saw the run on a console, but no number is recorded; no limit may rest on it, and no label rises on it.
- **A withdrawn reading stays,** marked withdrawn, with what replaced it. For example, SH4-021 keeps h1's transform figure and says why it was the buffer placing, not the routine.

## How much we have

Everything MEASURED so far is **one console, one run**. No number has a spread yet. The runs are h1 and h2 (chip benches), Compare by blending [c1] and Bump unit as a subtract [c3b] (compare spikes), s2 and s3 (route AB spikes) and the town demo's HUD photos, all on 2026-11-24 or 2026-11-25. A second console and a second run are open for every row.

## Adding a row after a console run

1. Photograph every result page. Note the photo file names. Photos go in `logs/` (see [benchmarking](../methods/benchmarking.md)).
2. Write the run up in the spike's own notes file, splitting "what the console printed" from "what I read into it".
3. Add one ledger row per finding: claim, status with date and console count, the photo or log, the test.
4. If the number changes a limit, update [recommended limits](../limits/recommended-limits.md).
5. If it disagrees with a document or an earlier row, add an [errata](../errata/README.md) entry, and mark the old row.
6. If it raises a question, add it to [open questions](../open-questions.md).
