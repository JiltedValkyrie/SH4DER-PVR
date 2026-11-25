# Notes for SH4ZAM: the fast exp / log family (`shz_pow2f`, `shz_ldexpf`, `shz_expf`, `shz_powf`, `shz_logf`, `shz_expm1f`, `shz_log2f`)

For: Falco Girgis (SH4ZAM's author). Not filed upstream; the owner decides whether to send them.

From: the SH4DER-PVR project, which uses SH4ZAM as a library under its MIT licence. Thank you for it.

**Seen on:**

- SH4ZAM master `ff39547` (2026-11-22);
- the software back end (`source/sw`), on the host: gcc, MSYS2 mingw64, x86-64;
- **not on a Dreamcast.** The functions below are plain C with no SH-4 assembly, so the same numbers are expected there. That is an inference, not tested.

**Repro:** `shz_notes_repro.c` in this folder. Its build line is in its header. Arguments pass through `volatile`, so gcc cannot fold them.

## 1. `shz_ldexpf(x, e)` is 2.865 % low for every whole exponent

`shz_ldexpf(x, exp)` returns `x * shz_pow2f(exp)`. `shz_pow2f(p)` builds the float's bits as `(1 << 23) * (p + 126.94269504f)`. For whole p, that puts a mantissa of 1.94269504 one exponent down, that is 0.97134752 x 2^p. Every whole power of two comes out 2.865 % low:

| e | `ldexpf(1, e)` | `shz_ldexpf(1, e)` / `shz_pow2f(e)` | Relative |
|---|---|---|---|
| −4 | 0.0625 | 0.0607092 | −0.02865 |
| 0 | 1 | 0.971348 | −0.02865 |
| 3 | 8 | 7.77075 | −0.02866 |
| 4 | 16 | 15.5415 | −0.02866 |

- The worst over x in [1, 2) and e in −20..20 is 0.02866.
- The constant 126.94269504 centres `shz_pow2f`'s error over fractional p (the usual fast exp2 trick). That is a fair choice for `pow2f`. But "ldexp" reads as an exact scaling by 2^e, and callers will pass whole numbers.
- **A constant argument gives a different answer.** With `__builtin_constant_p`, gcc folds `shz_pow2f(3.0f)` to the builtin's exact 8. At run time the same call gives 7.77075. Code can then behave differently after a refactor that turns a literal into a variable.

**What we did instead:** `shz_powif(2.0f, e)` (squarings), which was exact for whole e in our repro.

**Possible fixes**, offered only:

- an exact `shz_ldexpf` for a whole exponent, adding e to the exponent field with the usual range checks;
- or a documented note that `shz_ldexpf` is approximate, the same as `shz_pow2f`.

## 2. `shz_log2f` is the piecewise-linear log2, plus 0.0573

`shz_log2f(x)` returns `(float)(int32_t)bits * 2^-23 - 126.94269504f`, which is (exponent + mantissa − 1) + 0.05730496:

| Measured on 65,536 points in [1, 256) | Value |
|---|---|
| `shz_log2f` − piecewise-linear log2 | 0.05729675 to 0.05731201; the spread is the int-to-float conversion's rounding of the 30-bit integer |
| Worst against `log2f` | 0.0573, at the powers of two |
| `shz_log2f(1)` | 0.057304 |
| `shz_log2f(2)` | 1.057304 |
| `shz_log2f(256)` | 8.057304 |
| `shz_log2f(8.0f)` folded at compile time | 3 |
| `shz_log2f(8.0f)` at run time | 3.0573 |

This one is not a bug report.

- Centring the error is a reasonable choice for a fast log2.
- The note is that the doc comment ("Fast approximation for C's log2f()") could say that the result is the linear-mantissa log2 offset by 0.0573. Then a caller that wants the uncentred curve knows to subtract it.

Why we care: the PowerVR2's fog table indexes 1/w by exactly that uncentred curve, 16 entries an octave, linear inside one (our console reading, `compare/NOTES.md` c3b 6a). With the offset taken out, `shz_log2f` models it to 8e-6 octaves. `recipes.h` `rcp_log2lin_` does that.

## 3. The functions built on `shz_pow2f` and `shz_log2f`

Read from `inline/shz_scalar.inl.h`:

- `shz_expf(p) = shz_pow2f(1.442695 p)`
- `shz_pow10f(p) = shz_expf(2.302585 p)`
- `shz_powf(x, p) = shz_pow2f(p shz_log2f(x))`
- `shz_logf(x) = 0.693147 shz_log2f(x)`
- `shz_scalbnf = shz_ldexpf`
- `shz_expm1f(x)`: a series for |x| < 0.1, else `shz_expf(x) - 1`

Measured by the same repro (run time, host, software back end):

| Function | Range | Error | Examples |
|---|---|---|---|
| `shz_pow2f(p)` | p in [−8, 8] | relative −3.89 % .. +2.01 % | the range is not centred on 0 |
| `shz_expf(x)` | x in [−10, 10] | −3.89 % .. +2.01 %; up to 3.9 % for abs(x) < 0.05 | `shz_expf(0)` = 0.97135; `shz_expf(−0.012)` = 0.96269 where exp gives 0.98807 |
| `shz_pow10f(p)` | p in [−3, 3] | 3.89 % worst | `shz_pow10f(2)` = 101.539 |
| `shz_powf(x, p)` | x in [0.1, 10], p in [−3, 3] | **14.2 % worst**: the log2 offset (0.0573) is multiplied by p, then pow2's error is added | `shz_powf(2, 3)` = 8.917 |
| `shz_logf(x)` | x in [0.01, 100] | 0.0397 absolute worst | `shz_logf(1)` = 0.03972 |
| `shz_scalbnf(x, e)` | — | as `shz_ldexpf`: 2.865 % low at whole e | `shz_scalbnf(1, 3)` = 7.77075 |
| `shz_expm1f(x)` | across abs(x) = 0.1 | **steps down** from 0.105060 at 0.0999 to 0.087112 at 0.1001 (−0.0179), where expm1 rises 0.0002 | the series below the cutoff is accurate; the `shz_expf - 1` above it carries the 3 % into a small number |

**Possible fixes**, offered only:

- Re-centre `shz_pow2f` and `shz_log2f` so that `exp(0)`, `log(1)` and whole powers of two are exact. For example, use a mantissa correction polynomial on the fraction, the usual next step after the bit trick.
- Or name the fast forms as approximations (`*_approx`), and make the plain names accurate.
- For `shz_expm1f`, the step at the cutoff matters more than the size of the error: the function is not monotonic there. A wider series, or the same correction, would close it.

## 4. Does SH4DER-PVR hit any of this?

No live code path does. Searched `include/`, `src/`, `demo/`, `test/`, `example/`, `tools/`, `sh4dow-dc/` and `third_party/sh4zam-dce/` for every function above:

| Where | What it does | Status |
|---|---|---|
| `include/sh4der_sss.h:72` | its own exp series, "Not shz_expf: its fast form reads exp(−0.012) = 0.963" | avoids it (FACT) |
| `include/sh4der_water.h:40`, `demo/ocean_scene.h` | `sh4der_water_exp`, the same series | avoids it (FACT) |
| `include/sh4der_look.h:137` | log and exp by short series, "Not shz_logf / shz_expf: their fast forms read log(1) = 0.040" | avoids it (FACT) |
| `include/sh4der_cloudvolume.h:54, 180` | Newton roots; says "shz_powf and shz_expf are within 3 %" | avoids them. Its note understates `shz_powf`: 14.2 % here. The header's text is not wrong for what it uses. |
| `third_party/sh4zam-dce`, `sh4dow-dc` | none of these functions; `ldexpf` appears only in a host test of sh4zam-dce (libm, not SH4ZAM) | none |
| `demo/psm/recipes/recipes.h` | `shz_log2f` with the offset taken out; `shz_powif` instead of `shz_ldexpf` | avoids the error (this folder) |

**Stale text, not code:** three documents still name `shz_expf` where the code uses the library's own series. Lead to decide; this note does not edit them.

- `include/sh4der_sss.h:34`: "three exp (SH4ZAM shz_expf)". Line 72 of the same file says not.
- `AGENTS.md`, "Call order: sss", cost: "three `shz_expf`".
- `demo/OCEAN.md:66`: lists `shz_expf` under the demo's maths. `ocean_scene.h` uses `sh4der_water_exp`.

## What we have not checked

- Any of these functions on a Dreamcast. The SH-4 back end may route some through other code; we read only the inline C above.
- `shz_log1pf`, `shz_cexpf`, `shz_cpowf`, `shz_clogf`.
