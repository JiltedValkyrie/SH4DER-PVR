/* SH4ZAM-DCE tests: shz_dce_atan2.h, the loops its cycle ESTIMATES are made from

   atan2_bench.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc with the fast-math flags and run by tests/sh4run/sh4bench: every bench_ function is
   `void f(void *out, const void *in0, const void *in1, size_t count)` on buffers of random floats in [-1, 1).
   SH4ZAM's routine (the _shz row, before), the public rework (SHZ_DCE_ATAN2_TERMS), and the rework with 2, 5, 6 and 7 terms.
   An ESTIMATE from the interpreter's pipeline model, not a console's.
*/

#include <sh4zam_dce/shz_dce_atan2.h>

#define R SHZ_RESTRICT
#define F2(name, expr) void name(float *R out, const float *R a, const float *R b, size_t n) { for (size_t i = 0; i < n; i++) { const float x = a[i], y = b[i]; out[i] = (expr); } }

F2(bench_atan2f_shz, shz_atan2f(y, x))
F2(bench_atan2f, shz_dce_atan2f(y, x))
F2(bench_atan2f_2, shz_dce_atan2f_n_(y, x, 2))
F2(bench_atan2f_5, shz_dce_atan2f_n_(y, x, 5))
F2(bench_atan2f_6, shz_dce_atan2f_n_(y, x, 6))
F2(bench_atan2f_7, shz_dce_atan2f_n_(y, x, 7))
