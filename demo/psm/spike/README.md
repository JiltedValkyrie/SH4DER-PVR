# Route AB spike

A test program for a console, not an effect. It asks whether the route AB idea of `../ROUTE_AB_NOTES.md` holds on the hardware: the PVR draws depth into a render target as a colour code, the picture is copied back to RAM, the SH-4 compares.

**Status: built, never run.** Not on a console, not in an emulator. Every number it prints is produced by the run that prints it. An emulator run shows the emulator's model, not the PVR's precision or the bus.

## Build

In a shell with the KallistiOS environment sourced:

```
python demo/psm/spike/build.py --mkdcdisc <path to mkdcdisc>
```

It makes `build/psm_ab_spike/psm-ab-spike-s2.cdi` and refuses to overwrite an image (`--tag s3` for the next). `--host-only` runs just the host test of the model; `--elf-only` stops before the image. It stops, and says so, on a KallistiOS without `pvr_scene_begin_rtt`. It needs no SH4ZAM and no library file.

## Reading the pages

A: next page. B: back. START: run again. The same lines go to the serial line with an `ab:` prefix.

**The first line is the verdict** of the sanity step (s2). Read it before anything else:

| Verdict | Meaning |
|---|---|
| `SANITY OK: target written and read` | a plain magenta quad on green came back as drawn: the ramp lines can be believed |
| `TARGET UNCHANGED: render did not reach VRAM` | the target still holds the 0x1234 the SH-4 put there. An emulator that keeps render targets on the host GPU reads so (Flycast: the video option "Copy Rendered Textures to VRAM"); on a console, the render address is wrong |
| `ALL ZERO: target not written or read wrong` | something wrote zeros, or the read is of the wrong place |
| `NO PASS: ...` | KallistiOS refused the pass, or the render never started or never ended; the `rtt rc / start / end` line says which |
| `TARGET WRONG: ...` | part right, part not: the counts in `0 SANITY b` say how |

s1's first run read 0 in every pixel. Its log shows `CHCR0` going from 0 to 2 after the first download, which is what the Flycast source does at the end of its PVR-DMA model, so that run was most likely Flycast with render targets kept off its video memory. s2 tells that case from the others.

**Sanity lines** (`0 SANITY`): the target's pointer and video memory offset; `a`, the SH-4 writing the target and reading it back; `b`, the plain picture: the pass's return values (`rtt rc`, `start`, `end`, with the count of 100 ms waits that timed out first), the render address the PVR was given against the one wanted, how many pixels are right, still prefill, zero or other, and whether the target changed three vblanks later. When the picture is not there, all 8 MB of video memory is searched for it in both areas.

**Clock lines**: the smallest step of the KallistiOS ns timer, and how many CPU cycles (SH-4 performance counter 1) pass in 1 ms of it: about 200,000 on a console. Every time is printed by both clocks.

**Ramp lines**, one a test: `bits test dither: bad hrd cov max`.

- `bits`: 10 (the 1024 x 8 ramp) or 16 (the 1024 x 64 ramp, u clamped, v wrapped).
- `test`: `FLAT` (one quad, a different code at each pixel), `TILT` (depth halves across the quad; the code is a / k), `OVER` (two quads that overlap, sent in both orders).
- `d0` / `d1`: the dither bit off / on.
- `bad`: pixels of the 16,384 that differ from the model.
- `hrd`: of those, the ones rounding at a step does not explain.
- `cov`: of those, a quad's pixel that is background or the other way round.
- `max`: the largest error in codes.
- When a test has bad pixels, it adds the first one and the sample offset (in half pixels) that fits best. A best offset other than 0,0 with few bad pixels means the PVR samples somewhere else in the pixel, which a real route can allow for.

What the answers would mean:

| Result | Meaning |
|---|---|
| 10 FLAT d0 all 0 | a texel reaches a 16-bit target bit-exact: the idea has a depth code |
| 10 FLAT d0 bad, d1 the same | the dither bit does not explain it; look at the first bad pixel |
| 16 FLAT bad, low 6 bits wrong | the wrapped v loses precision: the map has fewer than 16 bits |
| TILT max large, FLAT clean | the perspective division is coarser than the model's |
| OVER cov or hrd over 0 | the depth test did not pick the nearer quad |

**Copy lines.** `DMAOR` and `CHCR0` as found. Then one block for each PVR address form (64-bit area, 32-bit area): done or `TIMEOUT`, the status bit, the time, how many pixels the DMA wrote and how many differ from the SH-4's own read. A form that fails also prints the PVR-DMA counters.

**Time lines.** The PVR-DMA download of 32 KB and 512 KB (smallest and mean), 20,000 sparse reads from video memory and from RAM, and a whole 32 KB copy by the SH-4.

## What the program does not know

- The form of the PVR address for a download (it tries two).
- Whether `SB_PDAPRO` is needed, and whether the value written (0x6702007F, from memory of the register's description) is right.
- Whether SH-4 DMA channel 0 needs more than `DMAOR` = 0x8201. The notes guessed channel 2; the Flycast source models this DMA on channel 0.
- Whether a download that never ends can hold the bus. The program stops waiting after 50 ms, but it cannot promise the console stays alive.

## Files

- `ab_model.h`: the test geometry, the ramps, the expected pixels, the check. Plain C, shared by the program and the host test.
- `ab_spike.c`: the Dreamcast program (KallistiOS).
- `test/ab_model_test.c`: host test of the model, run by `build.py`.
- `build.py`: the build.

Register names and the direction bit: the Flycast source (`core/hw/holly/sb.h`, `core/hw/pvr/pvr_sb_regs.cpp`). Area addresses: Sega's 1999 "SH4 access" table. No code is copied from either.
