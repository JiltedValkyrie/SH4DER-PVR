/* SH4ZAM-DCE tests: doc/upstream/shz_asin_acos.h, the loops its cycle ESTIMATES are made from

   upstream_bench.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc with the fast-math flags and run by tests/sh4run/sh4bench: every bench_ function is
   `void f(void *out, const void *in0, const void *in1, size_t count)` on buffers of random floats in [-1, 1).
   SH4ZAM's routine (the _shz rows, before) and the proposal (after); then every argument past 1 (x + 2: the add is the same in
   both rows). An ESTIMATE from the interpreter's pipeline model, not a console's: the file's own cost figures are a console's.
*/

#include "../../doc/upstream/shz_asin_acos.h"

#define R SHZ_RESTRICT
#define F1(name, expr) void name(float *R out, const float *R a, const float *R b, size_t n) { (void)b; for (size_t i = 0; i < n; i++) { const float x = a[i]; out[i] = (expr); } }

F1(bench_asinf_shz, shz_asinf(x))
F1(bench_asinf_quartic, shz_asinf_quartic(x))
F1(bench_acosf_shz, shz_acosf(x))
F1(bench_acosf_quartic, shz_acosf_quartic(x))
F1(bench_asinf_past1_quartic, shz_asinf_quartic(x + 2.0f))
F1(bench_acosf_past1_quartic, shz_acosf_quartic(x + 2.0f))
