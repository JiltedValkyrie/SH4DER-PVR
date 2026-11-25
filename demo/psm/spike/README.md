# Route AB spike

A test program for a real Dreamcast, not an effect. It asks the hardware about the route AB idea of `../ROUTE_AB_NOTES.md`: the PVR draws depth into a render target as a colour code, the picture is copied back to RAM, the SH-4 compares.

Only a real console's run is evidence. Every number the program prints is produced by the run that prints it. What a console has answered so far is in `../ROUTE_AB_NOTES.md`, "Hardware result s2" and "Hardware result s3". The steps of s2 and s3 that are settled (ramp codes, precision bands, dither per pass) are no longer in the program; they are in the history of this folder.

## Build

In a shell with the KallistiOS environment sourced:

```
python demo/psm/spike/build.py --mkdcdisc <path to mkdcdisc>
```

- The image goes to `demo/psm/out/cdi/psm-ab-spike-s4.cdi` (git ignores that folder). An image that is there is never overwritten: `--tag s5` for the next.
- `--host-only` runs just the host test of the model. `--elf-only` stops before the image.
- It stops, and says so, on a KallistiOS without `pvr_scene_begin_rtt`.
- It needs no SH4ZAM and no library file.

## Running it (s4)

The run has two parts.

1. **Part 1** runs at boot and shows its pages. A: next page. B: back.
2. **Part 2** starts on START. It reads video memory through the SH-4's cached mirror, which no run has done before. If it hangs, part 1's pages are still on screen: photograph them first.

The same lines go to the serial line with an `ab:` prefix.

## Reading the pages

**Line 1 is the verdict** of the sanity step: a magenta quad on green, drawn over a target the SH-4 filled with 0x1234. `RENDER OK, 16 px held back (see E lines)` is the expected one.

**E lines: the pixels a render holds back**, for a target 129 and 130 rows tall, two passes each. `E h<rows> <pass>: <n> px x<a>-<b> y<c>-<d> =<value> <what>`. `PRE`: still the SH-4's prefill. `OLD`: the pass before's colour. The question is whether rows 0 to 127 are whole with one spare row (129), as they were with two (130).

**S lines: sending 960 ready vertices (920 triangles)**, three ways, smallest of 3. The vertices are built before the clock starts.

- `pvr_prim`: one call a vertex.
- `sq, 1 vtx`: the store queues, a vertex at a time (`pvr_dr_target` / `pvr_dr_commit`, what the demo does).
- `sq, array`: one store-queue copy of the whole array.
- `covers`: the ball's pixels in the target. The three should agree.

**T lines: the PVR's render time** in microseconds, from `pvr_get_stats` (stamped inside the interrupts), smallest of 3. For a 128 and a 256 target: an empty pass, the ball at radius 0.15, the ball at radius 0.30. The empty pass is the fixed part; the difference between the two balls is what covered pixels cost.

**B lines: copying the map out of video memory**, 32 KB and 128 KB, smallest of 8, in microseconds and CPU cycles. `d` is how many 32-bit words of the copy differ from the PVR-DMA copy, summed over the 8 runs: 0 means always the same.

| Line | What |
|---|---|
| `PVR-DMA` | the download |
| `memcpy P2` | `memcpy` from the uncached window |
| `16-bit P2` | a loop of 16-bit reads (s2's baseline) |
| `32-bit P2` | a loop of 32-bit reads |
| `64-bit P2` | a loop of the FPU's 64-bit moves |
| `cached P1` (part 2) | 32-bit reads through the cached mirror; the invalidate of the source lines is inside the time |
| `P1 + pref` (part 2) | the same, with a prefetch two lines ahead |

Any "times faster" is the ratio of two of these lines and nothing else.

**K lines (part 2): is the cached mirror ever stale.** Words that read differently through the cache and through the uncached window.

- `8 x (render, invalidate, read)`: should be 0.
- `NO invalidate`: rows 96 to 127 are read, redrawn, and read again without an invalidate. Over 0 shows the test can see stale data.
- `invalidate`: the same after an invalidate. Should be 0.

## Files

- `ab_model.h`: the test geometry, the ramps, the expected pixels, the check, the ball. Plain C, shared by the program and the host test.
- `ab_spike.c`: the Dreamcast program (KallistiOS).
- `test/ab_model_test.c`: host test of the model, run by `build.py`.
- `build.py`: the build.

Register names and the direction bit: read from the Flycast source as documentation of the register block (`core/hw/holly/sb.h`, `core/hw/pvr/pvr_sb_regs.cpp`). Area addresses: Sega's 1999 "SH4 access" table. No code is copied from either.
