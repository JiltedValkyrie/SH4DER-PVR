/* SH4ZAM-DCE tests: the few lines every host test shares

   shz_dce_test.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../LICENSE.
*/

#ifndef SHZ_DCE_TEST_H
#define SHZ_DCE_TEST_H

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int t_fails, t_checks;
static uint32_t t_state = 0x12345u;

static inline uint32_t t_rand(void) { t_state = t_state * 1664525u + 1013904223u; return t_state; }
static inline float t_frand(float lo, float hi) { return lo + (hi - lo) * (float)(t_rand() >> 8) * (1.0f / 16777216.0f); }
static inline uint32_t t_bits(float f) { uint32_t b; memcpy(&b, &f, 4); return b; }
static inline float t_float(uint32_t b) { float f; memcpy(&f, &b, 4); return f; }

/* units in the last place between two floats of the same sign (a large number when the signs differ) */
static inline uint32_t t_ulps(float a, float b) {
    const uint32_t x = t_bits(a), y = t_bits(b);
    if ((x ^ y) & 0x80000000u) return (x & 0x7fffffffu) + (y & 0x7fffffffu);
    return x > y ? x - y : y - x;
}

#define T_CHECK(cond, ...) do { t_checks++; if (!(cond)) { if (t_fails++ < 20) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } } while (0)

static inline int t_done(const char *name) {
    printf("%s: %d checks, %d failed\n", name, t_checks, t_fails);
    if (!t_fails) printf("%s OK\n", name);
    return t_fails ? 1 : 0;
}

#endif /* SHZ_DCE_TEST_H */
