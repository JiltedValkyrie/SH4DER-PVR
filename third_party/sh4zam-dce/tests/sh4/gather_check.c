/* SH4ZAM-DCE tests: shz_dce_gather.h as the SH-4 runs it

   gather_check.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   gather_check <gather_kernels.elf>
   The ELF is gather_kernels.c built by sh-elf-gcc; tests/sh4run executes it instruction by instruction. For each routine:
   - the SH-4 output against the byte-by-byte reader run on the host over the same bytes of the simulated RAM: bit for bit
   - the no-prefetch core against the same (the bench compares the two)
   - every call twice, the scratch registers seeded differently: the output must not change
   - the guard bytes after each output stay as they were
   - a stream at the very top of the simulated RAM, so that its last word is the last word of the RAM: a read past the stream stops the run
   - the negative control of that proof: the last word of the RAM reads, the first byte past it stops the run
   - the prefetch routines change nothing; the prefetches executed are counted (the design: 5 an index for a packed vertex, 4 for a
     32-bit one, 4 an index for the prefetch routines, none for the copy, and none in the no-prefetch core)
   - a stream at the top of the RAM starts on 4 mod 8 (for two or more vertices its word count is odd) and fits the 256 bytes the
     stack leaves free
   Data sits on the weakest alignment the routines allow: arrays on a multiple of 4 that is not a multiple of 8.
*/

#include <sh4zam_dce/shz_dce_gather.h>

#include "gather_jobs.h"
#include "sh4_check.h"

#define SH4_TOP (SH4_RAM_BASE + SH4_RAM_SIZE)   /* one past the last byte of the simulated RAM */
#define GUARD 16u

/* the reference: the field of n bits at pos, read byte by byte (only the bytes that hold the field) */
static uint32_t ref_get(const uint8_t *bytes, uint32_t *pos, unsigned n) {
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

static void ref_unpack3(uint32_t *out, const uint8_t *bytes, unsigned bx, unsigned by, unsigned bz, const uint32_t *ax, const uint32_t *ay,
                        const uint32_t *az, const uint32_t *idx, uint32_t count) {
    const uint32_t w = bx + by + bz;
    uint32_t k;
    for (k = 0; k < count; k++) {
        uint32_t pos = idx[k] * w;
        const uint32_t x = ref_get(bytes, &pos, bx);
        const uint32_t y = ref_get(bytes, &pos, by);
        const uint32_t z = ref_get(bytes, &pos, bz);
        out[3 * k + 0] = ax[x];
        out[3 * k + 1] = ay[y];
        out[3 * k + 2] = az[z];
    }
}

/* the words a stream needs for these vertices: up to the word of the last bit of the largest index */
static uint32_t words_for(const uint32_t *idx, uint32_t count, unsigned w) {
    uint32_t maxv = 0u, k;
    if (!count) return 0u;
    for (k = 0; k < count; k++) if (idx[k] > maxv) maxv = idx[k];
    return ((maxv * w) + w - 1u) / 32u + 1u;
}

/* the bit patterns a copy must keep: zeros of both signs, denormals, the largest and the smallest normal numbers, infinities, NaN payloads */
static const uint32_t special_bits[] = { 0x80000000u, 0x00000000u, 0x00000001u, 0x007fffffu, 0x00800000u, 0x7f7fffffu,
                                         0x7f800000u, 0xff800000u, 0x7fa00001u, 0xffc12345u, 0xffffffffu };

/* the first words of a table at addr hold the special patterns (the rest of the table stays random) */
static void fill_specials(uint32_t addr, uint32_t n) {
    uint32_t *p = (uint32_t *)host(addr, 4u * n);
    uint32_t k;
    for (k = 0; k < n && k < sizeof special_bits / sizeof special_bits[0]; k++) p[k] = special_bits[k];
}

static void fill_words(uint32_t addr, uint32_t n) {
    uint32_t *p = (uint32_t *)host(addr, 4u * n);
    uint32_t k;
    for (k = 0; k < n; k++) p[k] = t_rand();
}

static void check_guard(const uint8_t *o, uint32_t used, const char *what, uint32_t count) {
    uint32_t i;
    for (i = 0; i < GUARD; i++)
        T_CHECK(o[used + i] == 0xa5u, "%s wrote past its last vertex (count %u)", what, (unsigned)count);
}

static void test_unpack3(void) {
    static const uint32_t fixed[] = { 0, 1, 2, 7, 8, 9, 17, 33 };
    int trial, top_runs = 0, runs = 0, w32_runs = 0;
    for (trial = 0; trial < 300; trial++) {
        const int top = (trial % 2) == 0;   /* half the runs: the 32-bit ones among them too */
        const uint32_t aj = ram(sizeof(job_unpack3_t), 4, 0);
        job_unpack3_t *j = (job_unpack3_t *)host(aj, sizeof *j);
        uint32_t bx, by, bz, w, count, nv, nw, k;
        uint32_t idxv[40], exp[3 * 40], keep1[3 * 40], keep2[3 * 40];
        uint32_t aidx, as, aax, aay, aaz, aout, aout2;
        const uint32_t *ax, *ay, *az, *idx;
        const uint8_t *o1, *o2;

        if (trial % 3 == 1) {   /* a 32-bit vertex: the path that reads one word a vertex, with no shifts */
            do {
                bx = 1u + t_rand() % 16u;
                by = 1u + t_rand() % 16u;
                bz = 32u - bx - by;
            } while (bz < 1u || bz > 16u);
            w32_runs++;
        } else {
            do {
                bx = 1u + t_rand() % 16u;
                by = 1u + t_rand() % 16u;
                bz = 1u + t_rand() % 16u;
            } while (bx + by + bz > 32u);
        }
        w = bx + by + bz;
        count = trial < 8 ? fixed[trial] : t_rand() % 41u;
        nv = top ? 1u + t_rand() % 48u : 1u + t_rand() % 96u;
        for (;;) {
            for (k = 0; k < count; k++) {
                if (k == 0) idxv[k] = 0u;
                else if (count > 1u && k == count - 1u) idxv[k] = nv - 1u;
                else idxv[k] = t_rand() % nv;
            }
            nw = words_for(idxv, count, w);
            /* a stream at the top of the RAM starts at SH4_TOP - 4 nw: an odd word count puts that start on 4 mod 8 (the weakest placement) */
            if (!top || count < 2u || (nw & 1u)) break;
            nv++;
        }

        aidx = ram(4u * count + 4u, 8, 4);
        memcpy(host(aidx, 4u * count + 4u), idxv, 4u * count);
        if (top) as = SH4_TOP - 4u * nw;
        else as = ram(4u * nw + 4u, 8, 4);
        if (top) T_CHECK(4u * nw <= 0x100u && (count < 2u || (as & 7u) == 4u),
                         "top stream of %u words at %08x: longer than the 256 bytes the stack leaves, or not on 4 mod 8", (unsigned)nw, (unsigned)as);
        fill_words(as, nw);
        aax = ram(4u << bx, 8, 4);
        aay = ram(4u << by, 8, 4);
        aaz = ram(4u << bz, 8, 4);
        fill_words(aax, 1u << bx);
        fill_words(aay, 1u << by);
        fill_words(aaz, 1u << bz);
        fill_specials(aax, 1u << bx);
        fill_specials(aay, 1u << by);
        fill_specials(aaz, 1u << bz);
        aout = ram(12u * count + GUARD, 8, 4);
        aout2 = ram(12u * count + GUARD, 8, 4);
        memset(host(aout, 12u * count + GUARD), 0xa5, 12u * count + GUARD);
        memset(host(aout2, 12u * count + GUARD), 0xa5, 12u * count + GUARD);

        ax = (const uint32_t *)host(aax, 4u << bx);
        ay = (const uint32_t *)host(aay, 4u << by);
        az = (const uint32_t *)host(aaz, 4u << bz);
        idx = (const uint32_t *)host(aidx, 4u * count + 4u);
        ref_unpack3(exp, (const uint8_t *)host(as, 4u * nw), bx, by, bz, ax, ay, az, idx, count);

        j->out_pf = aout; j->out_nopf = aout2; j->bits = as;
        j->bx = bx; j->by = by; j->bz = bz;
        j->ax = aax; j->ay = aay; j->az = aaz; j->idx = aidx; j->count = count;

        call("_t_unpack3", aj);
        T_CHECK(sh4_stats(S)->pref == count * (w == 32u ? 4u : 5u),
                "unpack3 (%u,%u,%u) count %u: %u prefetches executed, the design issues %u", (unsigned)bx, (unsigned)by, (unsigned)bz,
                (unsigned)count, (unsigned)sh4_stats(S)->pref, (unsigned)(count * (w == 32u ? 4u : 5u)));
        o1 = (const uint8_t *)host(aout, 12u * count + GUARD);
        o2 = (const uint8_t *)host(aout2, 12u * count + GUARD);
        T_CHECK(!memcmp(o1, exp, 12u * count), "unpack3 (%u,%u,%u) count %u: SH-4 output differs from the byte reader", (unsigned)bx, (unsigned)by, (unsigned)bz, (unsigned)count);
        T_CHECK(!memcmp(o2, exp, 12u * count), "unpack3 (%u,%u,%u) count %u: no-prefetch core differs from the byte reader", (unsigned)bx, (unsigned)by, (unsigned)bz, (unsigned)count);
        check_guard(o1, 12u * count, "unpack3", count);
        check_guard(o2, 12u * count, "unpack3 (no-prefetch core)", count);
        memcpy(keep1, o1, 12u * count);
        memcpy(keep2, o2, 12u * count);

        sh4_seed(S, 0x1000u + (uint32_t)trial);   /* other garbage in the scratch registers: a value that changes read a register never written */
        call("_t_unpack3", aj);
        T_CHECK(!memcmp(host(aout, 12u * count), keep1, 12u * count), "unpack3: a second run gave other bits (count %u)", (unsigned)count);
        T_CHECK(!memcmp(host(aout2, 12u * count), keep2, 12u * count), "unpack3 no-prefetch: a second run gave other bits (count %u)", (unsigned)count);

        if (top) top_runs++;
        runs++;
        sh4_alloc_reset(S);
    }
    printf("unpack3: %d runs, %d of them 32-bit vertices, %d with the stream at the top of the RAM (a read past the stream would have stopped the run)\n",
           runs, w32_runs, top_runs);
}

static void test_copy3(void) {
    int trial, runs = 0;
    for (trial = 0; trial < 200; trial++) {
        const uint32_t entries = 1u + t_rand() % 300u;
        const uint32_t count = (trial % 7) == 0 ? 0u : 1u + t_rand() % 40u;
        const uint32_t aj = ram(sizeof(job_copy3_t), 4, 0);
        job_copy3_t *j = (job_copy3_t *)host(aj, sizeof *j);
        uint32_t idxv[40], exp[3 * 40], k;
        uint32_t atab, aidx, aout;
        const uint32_t *tab, *idx;
        const uint8_t *o;

        atab = ram(12u * entries, 8, 4);
        fill_words(atab, 3u * entries);
        fill_specials(atab, 3u * entries);
        for (k = 0; k < count; k++) idxv[k] = t_rand() % entries;
        if (count) { idxv[0] = 0u; idxv[count - 1u] = entries - 1u; }
        aidx = ram(4u * count + 4u, 8, 4);
        memcpy(host(aidx, 4u * count + 4u), idxv, 4u * count);
        aout = ram(12u * count + GUARD, 8, 4);
        memset(host(aout, 12u * count + GUARD), 0xa5, 12u * count + GUARD);

        tab = (const uint32_t *)host(atab, 12u * entries);
        idx = (const uint32_t *)host(aidx, 4u * count + 4u);
        for (k = 0; k < count; k++) {
            exp[3 * k + 0] = tab[3 * idx[k] + 0];
            exp[3 * k + 1] = tab[3 * idx[k] + 1];
            exp[3 * k + 2] = tab[3 * idx[k] + 2];
        }

        j->out = aout; j->table = atab; j->idx = aidx; j->count = count;
        call("_t_copy3", aj);
        T_CHECK(sh4_stats(S)->pref == 0u, "copy3 executed %u prefetches (none expected)", (unsigned)sh4_stats(S)->pref);
        o = (const uint8_t *)host(aout, 12u * count + GUARD);
        T_CHECK(!memcmp(o, exp, 12u * count), "copy3 entries %u count %u: SH-4 output differs from the scalar copy", (unsigned)entries, (unsigned)count);
        check_guard(o, 12u * count, "copy3", count);
        sh4_seed(S, 0x2000u + (uint32_t)trial);
        call("_t_copy3", aj);
        T_CHECK(!memcmp(host(aout, 12u * count), exp, 12u * count), "copy3: a second run gave other bits (count %u)", (unsigned)count);
        runs++;
        sh4_alloc_reset(S);
    }
    printf("copy3: %d runs\n", runs);
}

static void test_prefetch(void) {
    static uint8_t snap_s[4 * 300], snap_t[12 * 64], snap_i[4 * 41];
    int trial, runs = 0;
    for (trial = 0; trial < 60; trial++) {
        const uint32_t aj = ram(sizeof(job_prefetch_t), 4, 0);
        job_prefetch_t *j = (job_prefetch_t *)host(aj, sizeof *j);
        const uint32_t width = 3u + t_rand() % 30u;
        const uint32_t count = t_rand() % 41u;
        uint32_t idxv[41], k;
        const uint32_t as = ram(4u * 300u, 8, 4), at = ram(12u * 64u, 8, 4), ai = ram(4u * count + 4u, 8, 4);

        fill_words(as, 300u);
        fill_words(at, 3u * 64u);
        for (k = 0; k < count; k++) idxv[k] = t_rand() % 60u;   /* inside the table and the stream */
        memcpy(host(ai, 4u * count + 4u), idxv, 4u * count);
        memcpy(snap_s, host(as, 4u * 300u), 4u * 300u);
        memcpy(snap_t, host(at, 12u * 64u), 12u * 64u);
        memcpy(snap_i, host(ai, 4u * count), 4u * count);

        j->bits = as; j->width = width; j->idx = ai; j->count = count; j->table = at;
        call("_t_prefetch", aj);
        T_CHECK(sh4_stats(S)->pref == 4u * count, "prefetch: %u prefetches for %u indices (the design: 4 an index)", (unsigned)sh4_stats(S)->pref, (unsigned)count);
        T_CHECK(!memcmp(snap_s, host(as, 4u * 300u), 4u * 300u), "prefetch changed the stream (width %u count %u)", (unsigned)width, (unsigned)count);
        T_CHECK(!memcmp(snap_t, host(at, 12u * 64u), 12u * 64u), "prefetch changed the table (width %u count %u)", (unsigned)width, (unsigned)count);
        T_CHECK(!memcmp(snap_i, host(ai, 4u * count), 4u * count), "prefetch changed the indices (width %u count %u)", (unsigned)width, (unsigned)count);
        runs++;
        sh4_alloc_reset(S);
    }
    printf("prefetch: %d runs, nothing changed\n", runs);
}

/* the interpreter stops a read past the end of the simulated RAM (the proof the top-of-RAM runs rely on), and not before it */
static void test_probe(void) {
    const uint32_t aj = ram(sizeof(job_probe_t), 4, 0);
    job_probe_t *j = (job_probe_t *)host(aj, sizeof *j);
    j->addr = SH4_TOP - 4u;
    T_CHECK(sh4_call(S, sh4_sym(S, "_t_probe"), aj, 0, 0, 0) == 0, "probe: the last word of the simulated RAM must read");
    j->addr = SH4_TOP;
    T_CHECK(sh4_call(S, sh4_sym(S, "_t_probe"), aj, 0, 0, 0) != 0, "probe: a read past the end of the simulated RAM must stop the run");
    printf("probe: the last word reads; the first byte past the RAM stops the run\n");
    sh4_alloc_reset(S);
}

int main(int argc, char **argv) {
    const int rc = check_open(argc, argv);
    if (rc) return rc;
    test_unpack3();
    test_copy3();
    test_prefetch();
    test_probe();
    return t_done("gather_check");
}
