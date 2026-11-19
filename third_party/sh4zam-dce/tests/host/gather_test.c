/* SH4ZAM-DCE tests: shz_dce_gather.h on the host (SH4ZAM's software back end)

   gather_test.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   What is checked:
   - unpack3, for every (bx, by, bz) with each width 1..16 and bx + by + bz <= 32, against a byte-by-byte reader of the same stream
     written here (the reference: the field of n bits at pos, read byte by byte, pos = v * W at the first vertex of the field set).
     Scattered, repeated, first and last vertices; counts 0, 1, 2, 3, 7 to 9, 15 to 17, 31, 33, 40 and random ones; out is compared
     bit for bit (NaN payloads and denormals in the tables included), and the 16 guard bytes after out must stay as they were.
   - the stream is read no further than its last needed word. Poison after that word (two fills): the answer must not change. Poison
     alone cannot show a read past the stream: every bit a field takes lies inside the vertex's own width, and the masks drop every
     bit above it, so a read past the stream cannot change the answer of a correct routine. The read itself is caught by the access:
     on Windows the stream also ends at the edge of a page whose next page is not accessible, so a read past it stops the test (the
     page-edge runs); the SH-4 side stops at the top of the simulated RAM (gather_check).
   - a far index: width 3, index 2^27 - 1 (a stream of about 48 MB): the arithmetic at the limit. The 32-bit limit of the wide width
     (index 2^27 - 1 times 32, plus 31) is checked by arithmetic: a stream of that size is not allocated here.
   - copy3: every entry of random tables, scattered and repeated indices, bit for bit.
   - the prefetch routines change nothing (a snapshot of the stream, tables and indices before and after).
*/

#include <sh4zam_dce/shz_dce_gather.h>

#include "shz_dce_test.h"

#include <stdlib.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define GATHER_TEST_PAGE_EDGE 1
#else
#define GATHER_TEST_PAGE_EDGE 0
#endif

#define TRIALS 6
#define GUARD 16u
#define POISON_WORDS 4u

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

/* the reference unpack: out[3k .. 3k+2] = the bits of { ax[x], ay[y], az[z] } for v = idx[k] */
static void ref_unpack3(uint32_t *out, const uint8_t *bytes, unsigned bx, unsigned by, unsigned bz, const uint32_t *ax, const uint32_t *ay,
                        const uint32_t *az, const uint32_t *idx, size_t count) {
    const uint32_t w = bx + by + bz;
    size_t k;
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

/* the bit patterns a copy must keep: zeros of both signs, denormals, the largest and the smallest normal numbers, infinities, NaN payloads */
static const uint32_t special_bits[] = { 0x80000000u, 0x00000000u, 0x00000001u, 0x007fffffu, 0x00800000u, 0x7f7fffffu,
                                         0x7f800000u, 0xff800000u, 0x7fa00001u, 0xffc12345u, 0xffffffffu };

/* the first words of a table hold the special patterns (the rest of the table stays random) */
static void fill_specials(uint32_t *t, size_t n) {
    size_t k;
    for (k = 0; k < n && k < sizeof special_bits / sizeof special_bits[0]; k++) t[k] = special_bits[k];
}

/* the 32-bit words a stream needs for the vertices idx[0..count-1] of width w: up to the word of the last bit of the largest index */
static size_t words_needed(const uint32_t *idx, size_t count, unsigned w) {
    uint32_t maxv = 0u;
    size_t k;
    if (!count) return 0u;
    for (k = 0; k < count; k++) if (idx[k] > maxv) maxv = idx[k];
    return (size_t)(((maxv * w) + w - 1u) >> 5) + 1u;
}

/* a stream of nwords words that ends at the edge of a page whose next page is not accessible (Windows only) */
#if GATHER_TEST_PAGE_EDGE
static uint32_t *stream_at_page_edge(size_t nwords, void **block) {
    const size_t pg = 4096u;
    const size_t bytes = nwords * 4u;
    const size_t body = (bytes + pg - 1u) / pg * pg;
    uint8_t *p = (uint8_t *)VirtualAlloc(NULL, body + pg, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    DWORD old = 0;
    if (!p) return NULL;
    if (!VirtualProtect(p + body, pg, PAGE_NOACCESS, &old)) {
        VirtualFree(p, 0, MEM_RELEASE);
        return NULL;
    }
    {   /* the edge is really inaccessible: a read past the stream would stop the run here */
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(p + body, &mbi, sizeof mbi) != sizeof mbi || mbi.Protect != PAGE_NOACCESS) {
            VirtualFree(p, 0, MEM_RELEASE);
            printf("FAIL: the guard page is not inaccessible\n");
            exit(1);
        }
    }
    *block = p;
    return (uint32_t *)(p + body - bytes);
}

static void stream_release(void *block) { VirtualFree(block, 0, MEM_RELEASE); }
#endif

/* One width set, one vertex list: the unpack from the same stream in up to three places: (a) a buffer with poison words after the
   stream, (b) the same with other poison, (c) at a page edge when there is one (nothing may be read past the stream). All of them must
   give the reference's bits, and the guard bytes after out must stay. */
static void unpack_case(unsigned bx, unsigned by, unsigned bz, const uint32_t *stream, const uint32_t *ax, const uint32_t *ay,
                        const uint32_t *az, const uint32_t *idx, size_t count, long *vertices, int *edge_runs) {
    const unsigned w = bx + by + bz;
    const size_t nw = words_needed(idx, count, w);
    const size_t out_bytes = 4u * 3u * count;
    uint32_t *ref = (uint32_t *)malloc(4u * 3u * count + 4u);
    uint8_t *out = (uint8_t *)malloc(out_bytes + GUARD);
    uint32_t *bufa = (uint32_t *)malloc(4u * (nw + POISON_WORDS) + 8u);
    uint32_t *bufb = (uint32_t *)malloc(4u * (nw + POISON_WORDS) + 8u);
    int pass;

    if (!ref || !out || !bufa || !bufb) { printf("FAIL: out of memory\n"); exit(1); }
    ref_unpack3(ref, (const uint8_t *)stream, bx, by, bz, ax, ay, az, idx, count);
    *vertices += (long)count;

    for (pass = 0; pass < 3; pass++) {
        void *block = NULL;
        uint32_t *words;
        size_t k;
        size_t i;

        if (pass == 2) {
#if GATHER_TEST_PAGE_EDGE
            words = stream_at_page_edge(nw, &block);
            if (!words) continue;
            for (k = 0; k < nw; k++) words[k] = stream[k];   /* no poison: the words after the stream are not accessible */
            (*edge_runs)++;
#else
            continue;
#endif
        } else {
            words = (pass == 0) ? bufa : bufb;
            for (k = 0; k < nw; k++) words[k] = stream[k];
            for (k = 0; k < POISON_WORDS; k++)
                words[nw + k] = (pass == 0) ? (0xdeadbeefu ^ (uint32_t)k) : (~(uint32_t)k * 0x9e3779b9u);
        }

        memset(out, 0xa5, out_bytes + GUARD);
        shz_dce_unpack3_gather((float (*)[3])out, words, bx, by, bz, (const float *)ax, (const float *)ay, (const float *)az, idx, count);
        T_CHECK(memcmp(ref, out, out_bytes) == 0, "unpack3 width %u (%u,%u,%u) count %u, run %d: bits differ from the byte reader",
                (unsigned)w, bx, by, bz, (unsigned)count, pass);
        for (i = 0; i < GUARD; i++)
            T_CHECK(out[out_bytes + i] == 0xa5u, "unpack3 wrote past its last vertex (count %u, run %d)", (unsigned)count, pass);

        if (block) stream_release(block);
    }
    free(ref);
    free(out);
    free(bufa);
    free(bufb);
}

int main(void) {
    unsigned bx, by, bz;
    long triples = 0, vertices = 0, copy_entries = 0;
    int edge_runs = 0, cases = 0, prefetch_calls = 0;

    /* -- unpack3 over every width combination, with a fixed or random vertex list each time -- */
    for (bx = 1; bx <= 16; bx++) for (by = 1; by <= 16; by++) for (bz = 1; bz <= 16; bz++) {
        const unsigned w = bx + by + bz;
        uint32_t *ax, *ay, *az;
        int trial;
        if (w > 32u) continue;
        triples++;
        ax = (uint32_t *)malloc(sizeof(uint32_t) << bx);
        ay = (uint32_t *)malloc(sizeof(uint32_t) << by);
        az = (uint32_t *)malloc(sizeof(uint32_t) << bz);
        {
            uint32_t j;
            for (j = 0; j < (1u << bx); j++) ax[j] = t_rand();   /* random bit patterns: NaNs, infinities, denormals */
            for (j = 0; j < (1u << by); j++) ay[j] = t_rand();
            for (j = 0; j < (1u << bz); j++) az[j] = t_rand();
            fill_specials((uint32_t *)ax, 1u << bx);
            fill_specials((uint32_t *)ay, 1u << by);
            fill_specials((uint32_t *)az, 1u << bz);
        }
        for (trial = 0; trial < TRIALS; trial++) {
            static const size_t fixed[] = { 0, 1, 2, 3, 7, 8, 9, 15, 16, 17, 31, 33, 40 };
            const size_t count = trial < 4 ? fixed[(size_t)(triples + trial) % (sizeof fixed / sizeof fixed[0])] : (size_t)(t_rand() % 41u);
            const uint32_t nverts = 1u + (uint32_t)(t_rand() % 96u);
            uint32_t *idx = (uint32_t *)malloc(sizeof(uint32_t) * (count + 1u));
            uint32_t *stream;
            size_t k, nw;
            for (k = 0; k < count; k++) {
                if (k == 0) idx[k] = 0u;                                  /* the first vertex */
                else if (count > 1u && k == count - 1u) idx[k] = nverts - 1u;   /* the last one */
                else idx[k] = (uint32_t)(t_rand() % nverts);
            }
            if (count > 2u && trial == 3) for (k = 0; k < count; k++) idx[k] = (uint32_t)(k % 2u);   /* repeats */
            nw = words_needed(idx, count, w);
            stream = (uint32_t *)malloc(4u * (nw + 1u));
            for (k = 0; k < nw; k++) stream[k] = t_rand();
            unpack_case(bx, by, bz, stream, ax, ay, az, idx, count, &vertices, &edge_runs);
            cases++;
            free(stream);
            free(idx);
        }
        free(ax);
        free(ay);
        free(az);
    }
    printf("unpack3: %ld width combinations, %d cases, %ld vertices, page-edge runs %d\n", triples, cases, vertices, edge_runs);

    /* -- the far index: width 3, the largest index allowed, a stream of about 48 MB -- */
    {
        static const uint32_t far_idx[3] = { SHZ_DCE_GATHER_INDEX_LIMIT - 1u, 0u, 4000001u };
        const unsigned w = 3u;
        const size_t nw = words_needed(far_idx, 3u, w);
        uint32_t *stream = (uint32_t *)malloc(4u * nw + 8u);
        uint32_t ax[2], ay[2], az[2];
        uint32_t ref[9], out[9];
        size_t k;
        if (!stream) { printf("FAIL: far stream not allocated\n"); return 1; }
        for (k = 0; k < nw; k++) stream[k] = t_rand();
        ax[0] = t_rand(); ax[1] = t_rand();
        ay[0] = t_rand(); ay[1] = t_rand();
        az[0] = t_rand(); az[1] = t_rand();
        ref_unpack3(ref, (const uint8_t *)stream, 1u, 1u, 1u, ax, ay, az, far_idx, 3u);
        memset(out, 0xa5, sizeof out);
        shz_dce_unpack3_gather((float (*)[3])out, stream, 1u, 1u, 1u, (const float *)ax, (const float *)ay, (const float *)az, far_idx, 3u);
        T_CHECK(memcmp(ref, out, sizeof ref) == 0, "far index: unpack differs from the byte reader");
        T_CHECK((uint64_t)(SHZ_DCE_GATHER_INDEX_LIMIT - 1u) * 32u + 31u <= 0xffffffffu,
                "index limit: the last bit of the largest vertex of 32 bits must fit in 32 bits");
        vertices += 3;
        free(stream);
        (void)w;
    }
    printf("far index: width 3, index 2^27 - 1, checked\n");

    /* -- copy3: random tables, scattered and repeated indices -- */
    {
        static const uint32_t sizes[] = { 1, 2, 3, 7, 64, 200, 1000 };
        size_t si;
        for (si = 0; si < sizeof sizes / sizeof sizes[0]; si++) {
            const uint32_t entries = sizes[si];
            uint32_t *table = (uint32_t *)malloc(4u * 3u * entries + 8u);
            uint32_t *idx = (uint32_t *)malloc(4u * 41u);
            uint32_t ref[3 * 40 + 4], out[3 * 40 + 4];
            size_t trial, k;
            static const size_t counts[] = { 0, 1, 2, 3, 7, 8, 9, 16, 17, 31, 40 };
            for (k = 0; k < 3u * entries; k++) table[k] = t_rand();
            fill_specials(table, 3u * entries);
            for (trial = 0; trial < 24; trial++) {
                const size_t count = trial < sizeof counts / sizeof counts[0] ? counts[trial] : (size_t)(1u + t_rand() % 40u);
                for (k = 0; k < count; k++) idx[k] = (uint32_t)(t_rand() % entries);
                if (count) { idx[0] = 0u; idx[count - 1u] = entries - 1u; }
                for (k = 0; k < count; k++) {
                    ref[3 * k + 0] = table[3 * idx[k] + 0];
                    ref[3 * k + 1] = table[3 * idx[k] + 1];
                    ref[3 * k + 2] = table[3 * idx[k] + 2];
                }
                memset(out, 0xa5, sizeof out);
                shz_dce_copy3_gather((float (*)[3])out, (const float *)table, idx, count);
                T_CHECK(memcmp(ref, out, 4u * 3u * count) == 0, "copy3: entries %u count %u differ from the scalar copy", (unsigned)entries, (unsigned)count);
                {
                    const uint8_t *ob = (const uint8_t *)out;
                    size_t g;
                    for (g = 0; g < GUARD; g++)
                        T_CHECK(ob[12u * count + g] == 0xa5u, "copy3 wrote past its last vertex (count %u, guard byte %u)", (unsigned)count, (unsigned)g);
                }
                copy_entries += (long)count;
            }
            free(table);
            free(idx);
        }
    }
    printf("copy3: %ld entries copied, bit for bit\n", copy_entries);

    /* -- prefetch: nothing it is given changes -- */
    {
        static uint32_t stream[300], snap_s[300];
        static uint32_t table[3 * 64], snap_t[3 * 64];
        static uint32_t idx[40], snap_i[40];
        size_t k;
        for (k = 0; k < 300; k++) stream[k] = t_rand();
        for (k = 0; k < 3 * 64; k++) table[k] = t_rand();
        for (k = 0; k < 40; k++) idx[k] = (uint32_t)(t_rand() % 60u);   /* inside the 64-entry table and the 300-word stream */
        memcpy(snap_s, stream, sizeof stream);
        memcpy(snap_t, table, sizeof table);
        memcpy(snap_i, idx, sizeof idx);
        shz_dce_prefetch_gather(stream, 29u, idx, 40u);
        shz_dce_prefetch_gather(stream, 32u, idx, 0u);
        shz_dce_prefetch_copy3_gather((const float *)table, idx, 40u);
        shz_dce_prefetch_copy3_gather((const float *)table, idx, 0u);
        prefetch_calls = 4;
        T_CHECK(memcmp(snap_s, stream, sizeof stream) == 0, "prefetch changed the stream");
        T_CHECK(memcmp(snap_t, table, sizeof table) == 0, "prefetch changed the table");
        T_CHECK(memcmp(snap_i, idx, sizeof idx) == 0, "prefetch changed the indices");
    }
    printf("prefetch: %d calls, nothing changed\n", prefetch_calls);

    return t_done("gather_test");
}
