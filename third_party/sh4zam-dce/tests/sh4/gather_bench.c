/* SH4ZAM-DCE tests: shz_dce_gather.h, the loops its cycle ESTIMATES are made from

   gather_bench.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc with the fast-math flags and run by tests/sh4run/sh4bench: every bench_ function is
   `void f(void *out, const void *in0, const void *in1, size_t count)`. The packed stream is in0, read as words (random floats, so
   random bits). The axis tables and the index list are static in this file: the cycles do not depend on their values (the code takes
   the same path for every value), and the indices stay below 1024, inside the stream of the smallest run.
   For each width set: the routine (count = the whole run), the same routine in calls of 8 (the real shape: a block of 8 a call), the
   same code with its prefetch passes removed, and the byte-by-byte reader written as a plain loop (the reference the tests use).
   The figures are the interpreter's pipeline model (an ESTIMATE, cache hits assumed), not a console's.
*/

#include <sh4zam_dce/shz_dce_gather.h>

#include <stddef.h>

/* the axis tables: 2^bx, 2^by, 2^bz entries at most for the widths below (the values are not read by the cycle model) */
static float bench_ax[4096], bench_ay[4096], bench_az[4096];
static float bench_tab[3 * 1024];

/* 192 scattered indices, each below 1024 */
static const uint32_t bench_idx[192] = {
     773,  265,  327,  677,  967,  856,  883,  683,  669,  332,  817,  747,
     940,  973,  218,   38,  582,  817,  890,    9, 1007,  576,  971,   53,
     856,  304,  825,  311,  870,  168,  735,  399,  554,  192,  648,  172,
    1001, 1022,  942,  494,  534,  602,  125,  930,  594,   58,  719,  520,
     370,  374,  817,  591,  631,   84,  988,  405,  406,  139,  955, 1003,
     817,  579,  363,  847,  732,  274,  565,  688,  600,  768,  276,  236,
     154,  899,  437,  980,  196,  643,  881,  805,  297,  854,   84,  912,
     588,  964,   24,  434,  481,  771,  809,  283,  457,  186,  418,  843,
     793,    1,  590,  687,  275,  606,  423,  423,   42,  713,  217,  386,
     258,  168,  191,  383,  876,  723,  227,  461,  365,  656,  641,  651,
     567,  662,  900,  712,  301,  525,  390,  898,  226,  907,  211,  682,
     538,   24,  870,  542,  711,  554, 1001,  684,  268,  169,  184,  789,
     572,  491,  734,  774,  474,  696,  277,  544,  153,  326,  714,  752,
     861,   60,  789,  502,  568,  434,  964,  161,  877,  558,   80,   80,
     623,  936,  229,  339,  737,  134,  350,  487,  919,  672,   69,  315,
     403,  572,  470,  624,  776,  273,  764,  916,  601,  327,   81,  166
};

/* the byte-by-byte reader, one field at a time: the bytes that hold the field, nothing more (the reference of the tests) */
static inline uint32_t bench_get(const uint8_t *bytes, uint32_t *pos, unsigned n) {
    const uint8_t *p = bytes + (*pos >> 3);
    const unsigned sh = *pos & 7u;
    const unsigned nb = (sh + n + 7u) >> 3;
    uint32_t value = p[0];
    if (nb > 1u) value |= (uint32_t)p[1] << 8;
    if (nb > 2u) value |= (uint32_t)p[2] << 16;
    if (nb > 3u) value |= (uint32_t)p[3] << 24;
    *pos += n;
    return (value >> sh) & ((1u << n) - 1u);
}

/* ---- width set 11, 11, 10 (32 bits a vertex) ---- */

void bench_unpack3_11_11_10(void *out, const void *in0, const void *in1, size_t n) {
    (void)in1;
    shz_dce_unpack3_gather((float (*)[3])out, (const uint32_t *)in0, 11, 11, 10, bench_ax, bench_ay, bench_az, bench_idx, n);
}

void bench_unpack3_x8_11_11_10(void *out, const void *in0, const void *in1, size_t n) {
    float (*o)[3] = (float (*)[3])out;
    size_t b;
    (void)in1;
    for (b = 0; b < n; b += 8) {
        const size_t m = n - b < 8 ? n - b : 8;
        shz_dce_unpack3_gather(o + b, (const uint32_t *)in0, 11, 11, 10, bench_ax, bench_ay, bench_az, bench_idx + b, m);
    }
}

void bench_unpack3_nopf_11_11_10(void *out, const void *in0, const void *in1, size_t n) {
    (void)in1;
    shz_dce_gather_unpack3_core((float (*)[3])out, (const uint32_t *)in0, 11, 11, 10, bench_ax, bench_ay, bench_az, bench_idx, n, 0);
}

void bench_unpack3_bytes_11_11_10(void *out, const void *in0, const void *in1, size_t n) {
    const uint8_t *bytes = (const uint8_t *)in0;
    float (*o)[3] = (float (*)[3])out;
    size_t k;
    (void)in1;
    for (k = 0; k < n; k++) {
        uint32_t pos = bench_idx[k] * 32u;
        const uint32_t x = bench_get(bytes, &pos, 11), y = bench_get(bytes, &pos, 11), z = bench_get(bytes, &pos, 10);
        o[k][0] = bench_ax[x];
        o[k][1] = bench_ay[y];
        o[k][2] = bench_az[z];
    }
}

/* ---- width set 12, 10, 10 (32 bits a vertex) ---- */

void bench_unpack3_12_10_10(void *out, const void *in0, const void *in1, size_t n) {
    (void)in1;
    shz_dce_unpack3_gather((float (*)[3])out, (const uint32_t *)in0, 12, 10, 10, bench_ax, bench_ay, bench_az, bench_idx, n);
}

void bench_unpack3_x8_12_10_10(void *out, const void *in0, const void *in1, size_t n) {
    float (*o)[3] = (float (*)[3])out;
    size_t b;
    (void)in1;
    for (b = 0; b < n; b += 8) {
        const size_t m = n - b < 8 ? n - b : 8;
        shz_dce_unpack3_gather(o + b, (const uint32_t *)in0, 12, 10, 10, bench_ax, bench_ay, bench_az, bench_idx + b, m);
    }
}

void bench_unpack3_nopf_12_10_10(void *out, const void *in0, const void *in1, size_t n) {
    (void)in1;
    shz_dce_gather_unpack3_core((float (*)[3])out, (const uint32_t *)in0, 12, 10, 10, bench_ax, bench_ay, bench_az, bench_idx, n, 0);
}

void bench_unpack3_bytes_12_10_10(void *out, const void *in0, const void *in1, size_t n) {
    const uint8_t *bytes = (const uint8_t *)in0;
    float (*o)[3] = (float (*)[3])out;
    size_t k;
    (void)in1;
    for (k = 0; k < n; k++) {
        uint32_t pos = bench_idx[k] * 32u;
        const uint32_t x = bench_get(bytes, &pos, 12), y = bench_get(bytes, &pos, 10), z = bench_get(bytes, &pos, 10);
        o[k][0] = bench_ax[x];
        o[k][1] = bench_ay[y];
        o[k][2] = bench_az[z];
    }
}

/* ---- width set 11, 10, 9 (30 bits a vertex: most vertices straddle two words, so the second word is used) ---- */

void bench_unpack3_11_10_9(void *out, const void *in0, const void *in1, size_t n) {
    (void)in1;
    shz_dce_unpack3_gather((float (*)[3])out, (const uint32_t *)in0, 11, 10, 9, bench_ax, bench_ay, bench_az, bench_idx, n);
}

void bench_unpack3_bytes_11_10_9(void *out, const void *in0, const void *in1, size_t n) {
    const uint8_t *bytes = (const uint8_t *)in0;
    float (*o)[3] = (float (*)[3])out;
    size_t k;
    (void)in1;
    for (k = 0; k < n; k++) {
        uint32_t pos = bench_idx[k] * 30u;
        const uint32_t x = bench_get(bytes, &pos, 11), y = bench_get(bytes, &pos, 10), z = bench_get(bytes, &pos, 9);
        o[k][0] = bench_ax[x];
        o[k][1] = bench_ay[y];
        o[k][2] = bench_az[z];
    }
}

/* ---- unpacked table: three floats an entry ---- */

void bench_copy3(void *out, const void *in0, const void *in1, size_t n) {
    (void)in0;
    (void)in1;
    shz_dce_copy3_gather((float (*)[3])out, bench_tab, bench_idx, n);
}

/* the scalar copy the unpacked gather replaces: three float moves an entry, in a plain loop */
void bench_copy3_scalar(void *out, const void *in0, const void *in1, size_t n) {
    float (*o)[3] = (float (*)[3])out;
    size_t k;
    (void)in0;
    (void)in1;
    for (k = 0; k < n; k++) {
        const float *e = bench_tab + 3u * bench_idx[k];
        o[k][0] = e[0];
        o[k][1] = e[1];
        o[k][2] = e[2];
    }
}

void bench_copy3_x8(void *out, const void *in0, const void *in1, size_t n) {
    float (*o)[3] = (float (*)[3])out;
    size_t b;
    (void)in0;
    (void)in1;
    for (b = 0; b < n; b += 8) {
        const size_t m = n - b < 8 ? n - b : 8;
        shz_dce_copy3_gather(o + b, bench_tab, bench_idx + b, m);
    }
}

/* ---- the prefetch routines alone: what a caller pays to fetch the next batch ahead (per index) ---- */

void bench_prefetch_gather_11_11_10(void *out, const void *in0, const void *in1, size_t n) {
    (void)out;
    (void)in1;
    shz_dce_prefetch_gather((const uint32_t *)in0, 32u, bench_idx, n);
}

void bench_prefetch_copy3(void *out, const void *in0, const void *in1, size_t n) {
    (void)out;
    (void)in0;
    (void)in1;
    shz_dce_prefetch_copy3_gather(bench_tab, bench_idx, n);
}
