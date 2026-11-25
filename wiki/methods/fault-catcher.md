---
title: The fault catcher
tags: [method, debug]
updated: 2026-11-25
sources: [demo/psm/psm_fault.h, demo/psm/fault/psm_fault_f1.c]
---
# The fault catcher

**What you will learn:** what the fault catcher shows when a console crashes or hangs, how to read its screen, and what it cannot catch.
**For:** anyone running a test image on a real console, where there is often no serial cable and no debugger.

## What it is

`demo/psm/psm_fault.h` is a header. Its plain-C part is host tested in `test/psm_fault_test.c`; its KallistiOS part is under `_arch_dreamcast`. The implementation goes into the one file that defines `PSM_FAULT_IMPL` (psm_fault.h:1-3, 51-61).

The test program is `demo/psm/fault/psm_fault_f1.c`, built with `python demo/psm/fault/build.py` into `demo/psm/out/cdi/psm-fault-f1.cdi`.

**Status:** "Not run on a console when this file was written" (psm_fault.h:62). The h1 bench uses it, so that a hang names the test it was in (bench_h1.c:6-8).

## Using it

1. Call `psm_fault_init(&crumbs)` after `pvr_init`.
2. Call `psm_fault_beat` once a frame.
3. Fill the breadcrumbs as the frame goes: stage name, scene, route, menu state, camera, vertices sent.

Cost: a beat is one increment, a setter a few stores; a frame's worth is under 100 instructions. About 3 KB of report buffers plus the breadcrumbs (psm_fault.h:47-50).

## What it catches

The first line of the screen says which kind (psm_fault.h:9-16):

- `*** EXCEPTION ***`: the exception's name and code, PC, PR, SR, SP, all registers, the fault address (TEA), and return addresses from the stack.
- `*** EXCEPTION in an interrupt handler ***`: a double fault.
- `*** HANG ***`: no heartbeat for 120 vblanks (2 s at 60 Hz). It shows the PC, the main thread's state and what it waits for, the ASIC status, whether the TA is busy, and the vertex buffer used and its maximum.
- `*** ASSERT ***` and `*** EXIT ***`.

## Reading the screen

- The top lines give the kind, the event name, the code and the address.
- CALLS are return addresses found on the stack. Old ones left on the stack can pass the test too, so read them as "these calls, perhaps not all still open" (psm_fault.h:284-287).
- For a HANG, PC is the code that was running, and STAGE is where in the frame it was.
- The bottom line says: "Photograph this screen. Power off to restart." Photograph it, and file the photo with the run.

It draws without the PVR, printf, locks or allocation: the KallistiOS minifont straight into video memory at offset 0, interrupts off, redrawn every 30 vblanks. It assumes 640 × 480 RGB565 (psm_fault.h:21-26, 61).

## What it cannot catch

From psm_fault.h:35-45:

- an exception while SH-4 exceptions are blocked (the CPU resets to the boot screen and nothing is shown);
- `arch_panic` and `abort` (KallistiOS resets the PVR; the message goes to serial only);
- a hang with interrupts masked;
- a wild write that destroys the handler table, the catcher or the code;
- floating-point faults (the FPU's traps are off; NaN and divide by zero raise nothing, and nor does an integer divide by zero on the SH-4);
- a wrong picture that still draws.

## Testing the catcher

Build with `-DPSM_FAULT_TEST` for `psm_fault_test()` with ADDRESS, ILLEGAL, SPIN, BLOCK and ASSERT. In `psm_fault_f1.c` the buttons are A for an address error, B for an illegal instruction, X for a spinning hang and Y for a blocked hang (psm_fault_f1.c:99-102).

## Related

- [Benchmarking](benchmarking.md), [guard](../effects/guard.md)
