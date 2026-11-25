# Route AB spike

A test program for a real Dreamcast, not an effect. It asks the hardware about the route AB idea of `../ROUTE_AB_NOTES.md`: the PVR draws depth into a render target as a colour code, the picture is copied back to RAM, the SH-4 compares.

Only a real console's run is evidence. Every number the program prints is produced by the run that prints it. What a console has answered so far is in `../ROUTE_AB_NOTES.md`, "Hardware result s2".

## Build

In a shell with the KallistiOS environment sourced:

```
python demo/psm/spike/build.py --mkdcdisc <path to mkdcdisc>
```

- The image goes to `demo/psm/out/cdi/psm-ab-spike-s3.cdi` (git ignores that folder). An image that is there is never overwritten: `--tag s4` for the next.
- `--host-only` runs just the host test of the model. `--elf-only` stops before the image.
- It stops, and says so, on a KallistiOS without `pvr_scene_begin_rtt`.
- It needs no SH4ZAM and no library file.

## Reading the pages (s3)

A: next page. B: back. START: run again. The same lines go to the serial line with an `ab:` prefix.

**Line 1 is the verdict** of the sanity step: a magenta quad on green, drawn over a target the SH-4 filled with 0x1234.

| Verdict | Meaning |
|---|---|
| `RENDER OK: target written and read` | the picture came back as drawn |
| `RENDER OK, N px held back (see E lines)` | the picture is right, but N background pixels still hold the 0x1234 |
| `TARGET UNCHANGED: render did not reach VRAM` | every pixel still holds the 0x1234 |
| `ALL ZERO: target not written or read wrong` | something wrote zeros, or the read is of the wrong place |
| `NO PASS: ...` | KallistiOS refused the pass, or the render never started or never ended |
| `TARGET WRONG: see 0 SANITY` | part right, part not |

On a failure the program adds the target's pointer, the pass's return values, the render address given against the one wanted, and a search of all 8 MB of video memory for the magenta.

**E lines: the pixels a render holds back.** `E h<rows> <pass>: <n> px x<a>-<b> y<c>-<d> =<value> <what>`.

- Each pass draws a new background colour (red, blue, red) into a target 128, 130, 136 and 160 rows tall. Every pixel that is not the background is counted; the box round them and the first one's value are printed.
- `PRE`: the pixel still holds the SH-4's prefill, so the render never wrote it.
- `OLD`: it holds the background of the pass before, so that pass's pixels arrived late.
- `E h128 sc`: the same, with an ordinary screen pass drawn before the read.

**R lines: the ramp tests**, dither off. `<bits> <test> -1:<n> +1:<n> more:<n> cov:<n>`.

- `bits`: 10 (the 1024 x 8 ramp) or 16 (the 1024 x 64 ramp, u clamped, v wrapped).
- `FLAT`: one quad, a different code at each pixel. `TILT`: depth halves across the quad; the code is a / k. `OVER`: two quads that overlap, sent in both orders.
- `-1` / `+1`: pixels that came back one code under / over the model. `more`: further off. `cov`: a quad's pixel that is background, or the other way round (with its box on the next line).

**P lines: precision of the 16-bit ramp.** Six bands whose code rises by one a pixel, starting at 64, 1024, 4096, 16384, 32768 and 57344. Each line gives the errors by sign for its band (1,792 pixels). A band with no errors keeps all its low bits at that size of code.

**D lines: dither off for the target pass alone.** The register is written before the target pass and put back after its render-done; then a screen pass is drawn.

- `D target: -1:0 +1:0 more:0` means the target was exact.
- `D screen: N/16384 px differ from undithered`: over 0 means the screen pass kept its dither.

**2 lines: the copy back.** One PVR-DMA download of the 32 KB target against the SH-4's own read, the DMA registers, then the smallest, largest and mean of 8 runs.

**C lines: the cost by target size** (128, 256, 512), the smallest of 3 runs each.

- `send`: the SH-4 time to send a 920-triangle ball (a stand-in for a caster) with the 16-bit ramp.
- `PVR render`: render start to render-done, as the program's thread saw the two interrupts.
- `DMA`: the download of the whole target.
- `20000 taps`: 20,000 two-byte reads of the copy in RAM, 4 a vertex for 5,000 vertices. This is the fetches of a compare, not the compare.
- KallistiOS renders a target through the screen's tile grid (640 x 480), so a 512-row target is drawn to row 479 only. The ball fits inside that.

## Files

- `ab_model.h`: the test geometry, the ramps, the expected pixels, the check. Plain C, shared by the program and the host test.
- `ab_spike.c`: the Dreamcast program (KallistiOS).
- `test/ab_model_test.c`: host test of the model, run by `build.py`.
- `build.py`: the build.

Register names and the direction bit: read from the Flycast source as documentation of the register block (`core/hw/holly/sb.h`, `core/hw/pvr/pvr_sb_regs.cpp`). Area addresses: Sega's 1999 "SH4 access" table. No code is copied from either.
