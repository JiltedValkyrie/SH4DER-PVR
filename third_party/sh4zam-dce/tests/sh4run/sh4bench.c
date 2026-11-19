/* SH4ZAM-DCE test tool: cycle ESTIMATES of batch routines

   sh4bench.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   sh4bench <elf> [n1 n2] <symbol> [<symbol> ...]
   Every symbol is a function `void f(void *out, const void *in0, const void *in1, size_t count)` of the SH-4 ELF (plus one float
   argument, 0.3, for the routines that take a weight). It is called with count n1 and n2 (default 64 and 192) on buffers of random
   floats in [-1, 1) (in0, in1: 32-byte aligned, 64 bytes an element at most); the cost of one element is the difference of the
   two runs over the difference of the counts, so the call's fixed part drops out.

   The cycles are the interpreter's pipeline model (sh4run.c): an ESTIMATE for comparing two versions, every memory access a cache hit.
*/

#include "sh4run.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ELEM 64u

static uint32_t lcg_state = 777u;
static float frand(void) {
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return (float)(int32_t)lcg_state * (1.0f / 2147483648.0f);
}

static int run(sh4_t *s, uint32_t fn, uint32_t n, sh4_stats_t *st) {
    const uint32_t bytes = n * MAX_ELEM;
    const uint32_t out = sh4_alloc(s, bytes, 32u), in0 = sh4_alloc(s, bytes, 32u), in1 = sh4_alloc(s, bytes, 32u);
    float *p0 = (float *)sh4_ptr(s, in0, bytes), *p1 = (float *)sh4_ptr(s, in1, bytes);
    const float t = 0.3f;
    uint32_t i;
    int rc;
    if (!out || !in0 || !in1) { printf("out of simulated RAM\n"); return -1; }
    lcg_state = 777u;
    for (i = 0; i < bytes / 4u; i++) { p0[i] = frand(); p1[i] = frand(); }
    sh4_float_args(s, &t, 1);
    rc = sh4_call(s, fn, out, in0, in1, n);
    *st = *sh4_stats(s);
    sh4_alloc_reset(s);
    return rc;
}

int main(int argc, char **argv) {
    sh4_t *s;
    uint32_t n1 = 64u, n2 = 192u;
    int k = 2, bad = 0;
    if (argc < 3) { printf("usage: sh4bench <elf> [n1 n2] <symbol> ...\n"); return 2; }
    s = sh4_new();
    if (!s || sh4_load_elf(s, argv[1]) != 0) { printf("load failed: %s\n", s ? sh4_error(s) : "no memory"); return 1; }
    if (argc > 4 && argv[2][0] >= '0' && argv[2][0] <= '9') { n1 = (uint32_t)strtoul(argv[2], NULL, 0); n2 = (uint32_t)strtoul(argv[3], NULL, 0); k = 4; }
    printf("%-34s %8s %8s  %s\n", "routine", "cycles", "instrs", "an element: loads stores | fipr ftrv fsrra fsca fmac fmul fadd fsub fdiv fsqrt fcmp cvt taken");
    for (; k < argc; k++) {
        sh4_stats_t a, b;
        const uint32_t fn = sh4_sym(s, argv[k]);
        const double d = (double)(n2 - n1);
        if (!fn) { printf("%-34s no such symbol\n", argv[k]); bad = 1; continue; }
        if (run(s, fn, n1, &a) || run(s, fn, n2, &b)) { printf("%-34s FAILED: %s\n", argv[k], sh4_error(s)); bad = 1; continue; }
#define PER(f) ((double)(b.f - a.f) / d)
        printf("%-34s %8.2f %8.2f  %5.2f %5.2f | %4.2f %4.2f %4.2f %4.2f %5.2f %5.2f %5.2f %5.2f %4.2f %4.2f %5.2f %4.2f %5.2f\n", argv[k],
               (double)(b.cycles - a.cycles) / d, (double)(b.instrs - a.instrs) / d, PER(loads), PER(stores), PER(fipr), PER(ftrv), PER(fsrra), PER(fsca),
               PER(fmac), PER(fmul), PER(fadd), PER(fsub), PER(fdiv), PER(fsqrt), PER(fcmp), PER(cvt), PER(taken));
#undef PER
    }
    sh4_free(s);
    return bad;
}
