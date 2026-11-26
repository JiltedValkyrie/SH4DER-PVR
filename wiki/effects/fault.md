---
title: Crash and hang screen [fault] (the fault catcher)
tags: [effect, debug, fault]
updated: 2026-11-26
sources: [include/sh4der_fault.h, test/fault_test.c, demo/psm/fault/psm_fault_f1.c, wiki/methods/fault-catcher.md, wiki/limits/header-recommended.md]
---
# Crash and hang screen [fault]: the fault catcher

**What you will learn:** what the fault catcher puts on the screen when a console program raises an exception, hangs, asserts or exits, and what it does not catch.
**For:** a programmer whose test image dies with nothing on screen, and an agent reading a crash photograph.

> **In plain words.** When a program crashes, freezes, fails an assert or quits, this puts a report on the screen instead of a blank picture. The report names the kind of failure, the processor's numbered storage slots (its registers), the places the program was called from (the return addresses on the stack), and the last frame's notes. Use it while building a program that dies with nothing on screen. Do not leave its test switch in a build for players.

## Try this first

It puts a readable report on the screen when the program crashes or freezes.

1. Define `SH4DER_FAULT_IMPL` in one source file, and include `sh4der_fault.h` in the rest (include/sh4der_fault.h:66-68; ADVISED, the header's Use block).
2. Call `sh4der_fault_init(&crumbs)` after `pvr_init`, and again after any later `pvr_init` (include/sh4der_fault.h:14, 70; ADVISED).
3. Once a frame: `sh4der_fault_beat(&crumbs)` (one count), then the setters `sh4der_fault_stage`, `sh4der_fault_frame`, `sh4der_fault_menu`, `sh4der_fault_camera` and `sh4der_fault_counts` as the frame goes (include/sh4der_fault.h:71, 116-145; ADVISED).
4. `SH4DER_FAULT_HANG_VBLANKS` = 120: 120 vertical blanks (the gaps between pictures, about 2 s at 60 Hz) with no beat before the HANG report (include/sh4der_fault.h:88 and its RECOMMENDED line 8; ADVISED, README "Fault tuning").
5. Leave `SH4DER_FAULT_TEST` undefined in any build for players. It adds forced faults and hangs for seeing the screens (include/sh4der_fault.h:72-73, 391; ADVISED).

On a console: NOT RUN. The header says: "no console run has shown it catching a fault or a hang."

Header: [include/sh4der_fault.h](../../include/sh4der_fault.h). Call order: [Call order](#call-order).

## What it does

When the program faults, the catcher draws a report straight into video memory and stops. The first line says which kind: `*** EXCEPTION ***` (an address error, an illegal instruction, a TLB fault), `*** HANG ***` (no frame for 120 vertical blanks), `*** ASSERT ***` or `*** EXIT ***`. Under it: the code, the PC, PR, SR, SP and the registers, the address at fault, the return addresses found on the stack (CALLS), the breadcrumbs the frame last wrote (frame, stage, pad, menu, camera, vertices), and for a hang the main thread's PC and what it waits for.

It uses KallistiOS's own hooks (the unhandled-exception handler, the vblank handler, the assert and exit hooks) and replaces none of them. Nothing is drawn with the PVR or printf, and nothing is allocated once the report is taken.

The plain-C part (the formatter, the watchdog's logic, the stack scan) is tested on the host (`test/fault_test.c`). The KallistiOS part compiles only for the Dreamcast.

Header only. Define `SH4DER_FAULT_IMPL` in one source file.

Version: v0.1, PROTOTYPE (see [maturity](../status/maturity.md)).

## Call order

The steps (the header's use comment; README "The fault catcher" once merged):

1. Keep one `sh4der_fault_crumbs_t` in the program. After `pvr_init`, call `sh4der_fault_init(&crumbs)`. Call it again after any later `pvr_init`.
2. Once a frame, `sh4der_fault_beat(&crumbs)`, then the setters as the frame goes: `sh4der_fault_stage` (at most 11 characters), `sh4der_fault_frame`, `sh4der_fault_menu`, `sh4der_fault_camera`, `sh4der_fault_counts`.
3. Do not define `SH4DER_FAULT_TEST` in a build for players. It adds hooks that fault or hang on purpose, for seeing the screens.

## Cost

| Item | Cost | Status |
|---|---|---|
| CPU, beat | one increment a frame | ESTIMATE, from the code |
| CPU, setters | under 100 instructions a frame | ESTIMATE, from the code |
| CPU, watchdog | about 20 instructions a vertical blank | ESTIMATE, from the code |
| RAM | the breadcrumbs (`SH4DER_FAULT_CRUMBS_BYTES`) and about 3 KB of report buffers | from the code |
| VRAM | none beyond the frame buffer it draws over | from the code |
| Measured on a console | not measured | OPEN |

## Recommended limits

- A hang is caught only when interrupts run. A loop with interrupts masked, or a freeze inside an interrupt handler, shows nothing: REPORTED (the watchdog is an interrupt). PVR-183 and PVR-208 froze with no HANG screen (MEASURED, one run each; the cause OPEN). See [recommended limits](../limits/recommended-limits.md).
- The catcher cannot catch an exception while exceptions are blocked (the CPU resets), or KallistiOS's own panic and abort (no hook). FACT from the header; no console run.
- `SH4DER_FAULT_HANG_VBLANKS` 120 (2 s at 60 Hz) is ADVISED: no console run has shown the watchdog firing.

## Tuning

See `README.md` "The fault catcher" (the fault tuning table). The layout constants (`SH4DER_FAULT_COLS`, `_ROWS`, `_TRACE_MAX`, `_STAGE_LEN`) are fixed by the screen and the report, not for tuning.

## Pitfalls

- The screen assumes 640 x 480 RGB565 (the demos' mode). Another mode draws the report in the wrong place.
- A fault while the catcher is drawing stops the program on the spot (by design).
- CALLS are return addresses found on the stack. Old ones can pass too: read them as "these calls, perhaps not all still open".

## Open

- No console run has shown the catcher firing on a real fault or hang. The one that would prove it: a deliberate `SH4DER_FAULT_TEST` address error and a spin on a test image, photographed.
- Why PVR-183 and PVR-208 froze without a HANG screen.

## Related

- [Send checks](guard.md), [host test first](../methods/host-test-first.md), [the fault catcher method](../methods/fault-catcher.md)
