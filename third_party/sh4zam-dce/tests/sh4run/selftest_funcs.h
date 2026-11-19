/* SH4ZAM-DCE test tool: sh4run's own test, the functions both sides run

   selftest_funcs.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Plain C with one answer on any IEEE machine (no contraction: build both sides with -ffp-contract=off, no -ffast-math). The host test
   runs these natively and through the interpreter on the SH-4 build of the same lines; the buffers and the results must agree bit for bit.
   That checks the interpreter itself (decoder, integer and float instructions, calling convention), before it is trusted with a kernel.
*/

#ifndef SELFTEST_FUNCS_H
#define SELFTEST_FUNCS_H

#include <stdint.h>
#include <string.h>

typedef uint16_t __attribute__((may_alias)) st_u16a;

/* integers: shifts, rotates, multiplies (32 and 64 bit), divisions (library calls on the SH-4), sign extensions, byte and word access */
static uint32_t st_int(uint32_t *buf, uint32_t n) {
    uint32_t h = 0x811c9dc5u, i;
    for (i = 0; i < n; i++) {
        const uint32_t v = buf[i];
        const uint8_t *b = (const uint8_t *)&buf[i];
        const st_u16a *w = (const st_u16a *)&buf[i];
        h ^= v; h *= 16777619u;
        h = (h << 7) | (h >> 25);
        h += (uint32_t)((int32_t)v >> 3);
        h ^= (uint32_t)(((uint64_t)v * h) >> 32);
        h -= (uint32_t)(((int64_t)(int32_t)v * (int32_t)h) >> 29);
        if (v & 1u) h = ~h; else h += v / 7u + v % 13u;
        h ^= (uint32_t)((int32_t)(v | 1u) / (int32_t)((h & 0xffu) + 3u));
        h += (uint32_t)(int32_t)(int8_t)b[1] + (uint32_t)(int32_t)(int16_t)w[1] + b[2] + w[0];
        h ^= h >> (v & 31u);
        h += (uint32_t)((int32_t)h >> (h & 15u));
        if ((int32_t)h < (int32_t)v) h += 0x9e3779b9u;
        if (h > v) h ^= 0x7f4a7c15u;
        switch (h & 7u) { case 0: h += 3u; break; case 1: h ^= 0x55u; break; case 2: h = h * 5u + 1u; break; case 5: h -= i; break; default: h += v; break; }
        buf[i] = h;
    }
    return h;
}

/* single precision: + - * / sqrt, compares, int <-> float */
static uint32_t st_float(float *f, uint32_t n) {
    float acc = 1.0f;
    uint32_t bits = 0u, i;
    for (i = 0; i < n; i++) {
        const float x = f[i];
        float y = x * 1.5f;
        uint32_t b;
        y = y + acc;
        y = y - x / 3.0f;
        if (y > 100.0f) y = y * 0.01f;
        if (y < -100.0f) y = -y;
        y = y + __builtin_sqrtf(x < 0.0f ? -x : x);
        acc = acc * 0.5f;
        acc = acc + (float)(int32_t)(y * 16.0f) * 0.0625f;
        if (y == x) acc = acc + 1.0f;
        f[i] = y;
        memcpy(&b, &y, 4);
        bits = bits * 31u + b;
    }
    return bits;
}

/* double precision: the SH-4 does it with FPSCR.PR = 1 */
static uint32_t st_double(float *f, uint32_t n) {
    double acc = 0.25;
    uint32_t bits = 0u, i;
    for (i = 0; i < n; i++) {
        const double x = (double)f[i];
        double t = acc * 0.75;
        t = t + x * x;
        t = t - x / 7.0;
        if (t > 1.0e6) t = t * 1.0e-6;
        if (t < -1.0e6) t = -t * 1.0e-6;
        acc = t;
        f[i] = (float)acc;
        bits = bits * 33u + (uint32_t)(int32_t)(acc * 100.0);
        bits += (uint32_t)(acc > x) + 2u * (uint32_t)(acc == x);
    }
    return bits;
}

/* one fused multiply-add a value: FMAC on the SH-4 */
static uint32_t st_fma(float *f, uint32_t n) {
    float acc = 0.5f;
    uint32_t bits = 0u, i;
    for (i = 0; i + 1u < n; i++) {
        const float y = __builtin_fmaf(f[i], f[i + 1u], acc);
        uint32_t b;
        acc = __builtin_fmaf(acc, 0.999f, -y * 0.001f);
        f[i] = y;
        memcpy(&b, &y, 4);
        bits = bits * 31u + b;
    }
    return bits;
}

/* arguments: four integers and five floats, a float result */
static float st_args(float a, int i, float b, float c, int j, float d, int k, float e, int l) {
    float r = a - b;
    r = r * 2.0f;
    r = r + c;
    r = r - d * 0.5f;
    r = r + e * (float)(i - j);
    return r + (float)(k * 3 - l);
}

#endif /* SELFTEST_FUNCS_H */
