/* SH4ZAM-DCE: a few scattered entries of a bit-packed vertex table

   shz_dce_gather.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.
   NOT affiliated with, endorsed by or maintained by Falco Girgis or the SH4ZAM
   project. Report problems with this library to SH4DER-PVR, not to them.
*/

/** \file    shz_dce_gather.h
    \brief   Read a few scattered vertices of a bit-packed table (shz_dce_unpack3_gather), copy a
             few scattered vertices of an unpacked table (shz_dce_copy3_gather), and fetch the cache
             lines of either ahead of use (shz_dce_prefetch_gather, shz_dce_prefetch_copy3_gather).

    The packed table is a bit stream, least significant bit first, as a little-endian machine lays
    out its words: vertex v occupies W = bx + by + bz bits starting at bit v * W, first its x field
    (bx bits), then its y field (by bits), then its z field (bz bits). Each field is an index into
    its own axis table, so vertex v is the point { ax[x], ay[y], az[z] }. Unpacking vertex v is a
    a small amount of integer work: find the word, or the two words, that hold its bits, shift the W bits down,
    mask the three fields, and copy the three floats. The output is the bits the byte-by-byte reader
    gives, NaN payloads and denormals included: the floats travel as 32-bit integers.

    Reads: 32-bit aligned word loads only. Bit k of the stream is bit (k & 31) of word (k >> 5). A
    vertex needs one word, or two when its last bit is in the next word. The second word is loaded
    only when it holds a bit of the vertex; when it does not, the address falls back to the first
    word (already read), so the routine never reads a word that holds none of the vertex's bits, and
    the last vertex does not read past the stream. The stream must be readable up to the next
    multiple of 4 bytes after its last needed bit: the last word holding a needed bit is read whole.
    A vertex of exactly 32 bits is one aligned word, so it has its own path with no offset shift (its
    fields are still shifted down to their own bits).

    The unpack works in blocks of 8 vertices (SHZ_DCE_GATHER_BLOCK), in four passes per block:
    1. the stream lines the block needs: the first word of each vertex and the last word of it (both
       prefetched; for a 32-bit vertex the two are the same word),
    2. the fields of every vertex, and the addresses of its three table entries,
    3. the three table entries of every vertex (prefetched),
    4. the copies, three words a vertex.
    Pass 3 costs more than its prefetches: in the 32-bit path the compiler reloads 14 of the block's
    24 table addresses from the stack to prefetch them, and each of those prefetches waits for its
    load (traced: 3 issue cycles against 1 for a prefetch from a register, about 3.5 E a vertex).
    Prefetching each entry in pass 2, while its address is in a register, was tried: batches of 64
    to 192 gained 2.4 E a vertex (36.50 to 34.12 E), calls of 8 did not (34.00 to 34.12 E; 34.62 to
    35.38 E for 12, 10, 10), so the four-pass order is kept.
    A full block has constant length, so its passes are unrolled; the tail of a call (fewer than 8
    vertices) runs the same passes in loops. The routines are always inlined: a call per block would
    add a prologue and an epilogue to every eight vertices. Both back ends compile this one C form:
    it is integer work and calls no SH4ZAM routine, so there is no separate _c and _sh4 pair and no
    hand asm (the only asm is the prefetch that SH4ZAM's SHZ_PREFETCH emits on the SH-4).

    Prefetch, honestly: the SH-4 has one external bus and fills one 32-byte line at a time, so
    prefetches do not overlap each other. What a prefetch buys is that the processor keeps working
    while a line arrives. Inside one call that helps only as far as there is other work to overlap
    (the field work of pass 2 and the copies of pass 4). The larger gain is the caller prefetching
    the NEXT batch with shz_dce_prefetch_gather while it unpacks the current one. The interpreter's
    cycle model assumes every access is a cache hit, so it cannot show any of this: every cycle
    figure below is the cache-hit case, an ESTIMATE. In that case a prefetch is one load slot that
    buys nothing, so the prefetch passes show up as extra cycles. The cache-miss case needs a
    measurement on a console, and no number for it is given here.

    Prefetch of the last word: it is issued whether or not it is in another line than the first.
    When it is the same line, the prefetch repeats one just issued. Issuing it only when it is in
    another line would need a branch per index; in the cache-hit model a taken branch costs two
    cycles E and the repeat costs one load slot, so the repeat is issued.

    Call order: the tables are plain arrays; nothing is loaded into the FPU or into XMTRX.
    shz_dce_prefetch_gather (or shz_dce_prefetch_copy3_gather) may run one batch ahead of the unpack.

    Alignment: bits, ax, ay, az, idx, table and out sit on a multiple of 4 (the words are 4 bytes; a
    vertex of out is 12 bytes, and nothing here needs more). Every pointer is asserted in a debug
    build.

    Limits (asserted in a debug build, trusted in a release build): 1 <= bx, by, bz <= 16;
    bx + by + bz <= 32; every index below SHZ_DCE_GATHER_INDEX_LIMIT (2^27). That limit keeps idx x W
    and the vertex's last bit inside 32 bits for every width up to 32. For a narrow stream the exact
    bound is higher; 2^27 is the simple bound the routines state.

    Error bound: none. Exact integer work and copies of 32-bit words.
    Bit-identical to the byte-by-byte reader of the same stream: YES. The test builds that reader and
    compares every width combination against it.

    Where it pays, and where it does not: the cycle figures are in the notes at the end of this file
    (cycles_gather.txt, cache-hit model, ESTIMATES).

    The effect: nothing on screen. Fewer SH-4 cycles to read packed vertex tables.
    Version: v0.10
    Use it for: a few scattered vertices of a packed or an unpacked table per call. A batch of 8 is the
      usual size. Not for a long, sequential read: that is a straight loop over the stream.
*/

#ifndef SHZ_DCE_GATHER_H
#define SHZ_DCE_GATHER_H

#include "shz_dce_cdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Vertices per block: the stack holds one block's bit positions and table addresses. */
#define SHZ_DCE_GATHER_BLOCK 8u

/** \brief Every index must be below this. With it, idx x W and the vertex's last bit fit in 32 bits for every width up to 32. */
#define SHZ_DCE_GATHER_INDEX_LIMIT 0x08000000u

/** \brief Prefetch the cache line that holds p: pref on the SH-4, nothing that changes a result anywhere (SHZ_PREFETCH). */
#define SHZ_DCE_GATHER_PREF(p) SHZ_PREFETCH(p)

#if defined(__GNUC__)
/* The arrays of a block are written in pass 2 and read in pass 4 over the same count. GCC cannot always see that for a loop whose count is
   a variable (the tail of a call) and reports a use of a maybe-uninitialized entry that cannot happen. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

/** \brief Internal: copy one 32-bit word as an integer (the type is 4-byte aligned and may alias the float: the bits come through
    unchanged, NaN payloads and denormals included, and no library call is made). */
SHZ_FORCE_INLINE void shz_dce_gather_move32(float *dst, const float *src) SHZ_NOEXCEPT {
    *(shz_alias_uint32_t *)dst = *(const shz_alias_uint32_t *)src;
}

/** \brief Internal: one block of up to 8 vertices of 32 bits: vertex k is word idx[k] of the stream. Passes as in the file comment. */
SHZ_FORCE_INLINE void shz_dce_gather_block32_(float (*SHZ_RESTRICT out)[3], const uint32_t *SHZ_RESTRICT bits, unsigned bx, unsigned by,
                                              unsigned bz, const float *SHZ_RESTRICT ax, const float *SHZ_RESTRICT ay,
                                              const float *SHZ_RESTRICT az, const uint32_t *SHZ_RESTRICT idx, size_t m, int pf) SHZ_NOEXCEPT {
    const uint32_t mx = (1u << bx) - 1u, my = (1u << by) - 1u, mz = (1u << bz) - 1u;
    const unsigned sz = bx + by;
    const float *ea[SHZ_DCE_GATHER_BLOCK], *eb[SHZ_DCE_GATHER_BLOCK], *ec[SHZ_DCE_GATHER_BLOCK];
    size_t k;

    /* pass 1: the word of each vertex */
#pragma GCC unroll 8
    for (k = 0; k < m; k++) {
        assert(idx[k] < SHZ_DCE_GATHER_INDEX_LIMIT);
        if (pf)
            SHZ_DCE_GATHER_PREF(bits + idx[k]);
    }
    /* pass 2: the three fields, and the addresses of the table entries */
#pragma GCC unroll 8
    for (k = 0; k < m; k++) {
        const uint32_t v = bits[idx[k]];
        ea[k] = ax + (v & mx);
        eb[k] = ay + ((v >> bx) & my);
        ec[k] = az + ((v >> sz) & mz);
    }
    /* pass 3: the table entries, fetched ahead of their loads */
    if (pf) {
#pragma GCC unroll 8
        for (k = 0; k < m; k++) {
            SHZ_DCE_GATHER_PREF(ea[k]);
            SHZ_DCE_GATHER_PREF(eb[k]);
            SHZ_DCE_GATHER_PREF(ec[k]);
        }
    }
    /* pass 4: the copies */
#pragma GCC unroll 8
    for (k = 0; k < m; k++) {
        float *o = out[k];
        shz_dce_gather_move32(o + 0, ea[k]);
        shz_dce_gather_move32(o + 1, eb[k]);
        shz_dce_gather_move32(o + 2, ec[k]);
    }
}

/** \brief Internal: one block of up to 8 vertices of any width 3..31 (the 32-bit width has its own block). Passes as in the file comment. */
SHZ_FORCE_INLINE void shz_dce_gather_block_(float (*SHZ_RESTRICT out)[3], const uint32_t *SHZ_RESTRICT bits, unsigned w, unsigned bx,
                                            unsigned by, unsigned bz, const float *SHZ_RESTRICT ax, const float *SHZ_RESTRICT ay,
                                            const float *SHZ_RESTRICT az, const uint32_t *SHZ_RESTRICT idx, size_t m, int pf) SHZ_NOEXCEPT {
    const uint32_t mx = (1u << bx) - 1u, my = (1u << by) - 1u, mz = (1u << bz) - 1u;
    const unsigned sz = bx + by;
    uint32_t pos[SHZ_DCE_GATHER_BLOCK];
    const float *ea[SHZ_DCE_GATHER_BLOCK], *eb[SHZ_DCE_GATHER_BLOCK], *ec[SHZ_DCE_GATHER_BLOCK];
    size_t k;

    /* pass 1: the stream lines of the vertex: its first word and its last word */
#pragma GCC unroll 8
    for (k = 0; k < m; k++) {
        const uint32_t b = idx[k] * w;
        assert(idx[k] < SHZ_DCE_GATHER_INDEX_LIMIT);
        pos[k] = b;
        if (pf) {
            SHZ_DCE_GATHER_PREF(bits + (b >> 5));
            SHZ_DCE_GATHER_PREF(bits + ((b + w - 1u) >> 5));
        }
    }
    /* pass 2: the fields, and the addresses of the table entries. The second word is read only when it holds a bit of the vertex;
       otherwise the first word is read again. The bits above w are garbage, and the masks drop them. */
#pragma GCC unroll 8
    for (k = 0; k < m; k++) {
        const uint32_t b = pos[k];
        const uint32_t off = b & 31u;
        const uint32_t *p = bits + (b >> 5);
        const uint32_t next = (32u - off - w) >> 31;
        const uint32_t v = (p[0] >> off) | ((p[next] << 1) << (31u - off));
        ea[k] = ax + (v & mx);
        eb[k] = ay + ((v >> bx) & my);
        ec[k] = az + ((v >> sz) & mz);
    }
    /* pass 3: the table entries, fetched ahead of their loads */
    if (pf) {
#pragma GCC unroll 8
        for (k = 0; k < m; k++) {
            SHZ_DCE_GATHER_PREF(ea[k]);
            SHZ_DCE_GATHER_PREF(eb[k]);
            SHZ_DCE_GATHER_PREF(ec[k]);
        }
    }
    /* pass 4: the copies, three words a vertex */
#pragma GCC unroll 8
    for (k = 0; k < m; k++) {
        float *o = out[k];
        shz_dce_gather_move32(o + 0, ea[k]);
        shz_dce_gather_move32(o + 1, eb[k]);
        shz_dce_gather_move32(o + 2, ec[k]);
    }
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

/** \brief Internal: the unpack with its prefetches (pf = 1) or without them (pf = 0: the same code minus the prefetch passes, for the
    cycle comparison in the bench). Same bits either way. Arguments as shz_dce_unpack3_gather(). */
SHZ_FORCE_INLINE void shz_dce_gather_unpack3_core(float (*SHZ_RESTRICT out)[3], const uint32_t *SHZ_RESTRICT bits, unsigned bx, unsigned by, unsigned bz,
                                                  const float *SHZ_RESTRICT ax, const float *SHZ_RESTRICT ay, const float *SHZ_RESTRICT az,
                                                  const uint32_t *SHZ_RESTRICT idx, size_t count, int pf) SHZ_NOEXCEPT {
    const unsigned w = bx + by + bz;
    size_t base = 0;

    if (w == 32u) {
        for (; count - base >= SHZ_DCE_GATHER_BLOCK; base += SHZ_DCE_GATHER_BLOCK)
            shz_dce_gather_block32_(out + base, bits, bx, by, bz, ax, ay, az, idx + base, SHZ_DCE_GATHER_BLOCK, pf);
        if (base < count)
            shz_dce_gather_block32_(out + base, bits, bx, by, bz, ax, ay, az, idx + base, count - base, pf);
    } else {
        for (; count - base >= SHZ_DCE_GATHER_BLOCK; base += SHZ_DCE_GATHER_BLOCK)
            shz_dce_gather_block_(out + base, bits, w, bx, by, bz, ax, ay, az, idx + base, SHZ_DCE_GATHER_BLOCK, pf);
        if (base < count)
            shz_dce_gather_block_(out + base, bits, w, bx, by, bz, ax, ay, az, idx + base, count - base, pf);
    }
}

/** \brief Unpack the vertices idx[0..count-1] of a bit-packed table: out[k] gets { ax[x], ay[y], az[z] } for vertex v = idx[k], where
    x, y, z are the three fields of vertex v (see the file comment).
    out: 12-byte vertices, 4-byte aligned; it overwrites out[0..count-1] and nothing else.
    bits: the stream, 4-byte aligned, readable up to the next multiple of 4 bytes after its last needed bit.
    bx, by, bz: 1..16 each, bx + by + bz <= 32. ax, ay, az: the axis tables, 4-byte aligned, at least 2^bx, 2^by, 2^bz entries.
    idx: 4-byte aligned, every entry below SHZ_DCE_GATHER_INDEX_LIMIT. count may be 0 (nothing happens) or any size.
    Bit-identical to the byte-by-byte reader: YES. Error bound: none (exact).
    Cycles E per vertex, -O3, cache hits, (11, 11, 10): 36.50 with its prefetch passes, 34.00 in calls of 8, 24.12 without the prefetch
    passes, 44.00 for the byte-by-byte reader (the notes at the end give the other width sets). */
SHZ_FORCE_INLINE void shz_dce_unpack3_gather(float (*SHZ_RESTRICT out)[3], const uint32_t *SHZ_RESTRICT bits, unsigned bx, unsigned by, unsigned bz,
                                             const float *SHZ_RESTRICT ax, const float *SHZ_RESTRICT ay, const float *SHZ_RESTRICT az,
                                             const uint32_t *SHZ_RESTRICT idx, size_t count) SHZ_NOEXCEPT {
    SHZ_DCE_ASSERT_ALIGNED(out, 4);
    SHZ_DCE_ASSERT_ALIGNED(bits, 4);
    SHZ_DCE_ASSERT_ALIGNED(ax, 4);
    SHZ_DCE_ASSERT_ALIGNED(ay, 4);
    SHZ_DCE_ASSERT_ALIGNED(az, 4);
    SHZ_DCE_ASSERT_ALIGNED(idx, 4);
    assert(bx >= 1u && bx <= 16u && by >= 1u && by <= 16u && bz >= 1u && bz <= 16u && bx + by + bz <= 32u);
    shz_dce_gather_unpack3_core(out, bits, bx, by, bz, ax, ay, az, idx, count, 1);
}

/** \brief Internal: one block of up to 8 entries of an unpacked table. */
SHZ_FORCE_INLINE void shz_dce_gather_copy3_block_(float (*SHZ_RESTRICT out)[3], const float *SHZ_RESTRICT table,
                                                  const uint32_t *SHZ_RESTRICT idx, size_t m) SHZ_NOEXCEPT {
    size_t k;
#pragma GCC unroll 8
    for (k = 0; k < m; k++) {
        const float *e;
        assert(idx[k] < SHZ_DCE_GATHER_INDEX_LIMIT);
        e = table + 3u * idx[k];
        shz_dce_gather_move32(&out[k][0], e + 0);
        shz_dce_gather_move32(&out[k][1], e + 1);
        shz_dce_gather_move32(&out[k][2], e + 2);
    }
}

/** \brief Copy the vertices idx[0..count-1] of an unpacked table: out[k] gets the three floats table[3 * idx[k] .. 3 * idx[k] + 2].
    out: 12-byte vertices, 4-byte aligned; it overwrites out[0..count-1] and nothing else. table: 4-byte aligned, 3 floats an entry.
    idx: 4-byte aligned, every entry below SHZ_DCE_GATHER_INDEX_LIMIT and inside the table. Bit-identical to the scalar copy: YES
    (the words are copied as integers). Error bound: none. Cycles E per entry, -O3, cache hits: 12.62 in a batch, 13.00 in calls of 8,
    16.00 for the plain scalar copy. */
SHZ_FORCE_INLINE void shz_dce_copy3_gather(float (*SHZ_RESTRICT out)[3], const float *SHZ_RESTRICT table, const uint32_t *SHZ_RESTRICT idx,
                                           size_t count) SHZ_NOEXCEPT {
    size_t base = 0;
    SHZ_DCE_ASSERT_ALIGNED(out, 4);
    SHZ_DCE_ASSERT_ALIGNED(table, 4);
    SHZ_DCE_ASSERT_ALIGNED(idx, 4);
    for (; count - base >= SHZ_DCE_GATHER_BLOCK; base += SHZ_DCE_GATHER_BLOCK)
        shz_dce_gather_copy3_block_(out + base, table, idx + base, SHZ_DCE_GATHER_BLOCK);
    if (base < count)
        shz_dce_gather_copy3_block_(out + base, table, idx + base, count - base);
}

/** \brief Prefetch the cache lines shz_dce_unpack3_gather() will read for the vertices idx[0..count-1] of a packed table of vertices
    bit_width_total bits wide (bx + by + bz, 3..32): the line of each vertex's first word and the line of its last word (for a 32-bit
    width they are the same word). Reads idx (the indices) and no stream data; changes nothing. Ahead of the next batch, this is
    where the gain is (see the file comment: prefetches do not overlap each other). idx: 4-byte aligned.
    Cycles E: 12.00 per index in the cache-hit model (a cost there, not a gain). */
SHZ_FORCE_INLINE void shz_dce_prefetch_gather(const uint32_t *SHZ_RESTRICT bits, unsigned bit_width_total, const uint32_t *SHZ_RESTRICT idx,
                                              size_t count) SHZ_NOEXCEPT {
    const unsigned w = bit_width_total;
    size_t k;
    SHZ_DCE_ASSERT_ALIGNED(bits, 4);
    SHZ_DCE_ASSERT_ALIGNED(idx, 4);
    assert(w >= 3u && w <= 32u);
    for (k = 0; k < count; k++) {
        const uint32_t b = idx[k] * w;
        assert(idx[k] < SHZ_DCE_GATHER_INDEX_LIMIT);
        SHZ_DCE_GATHER_PREF(bits + (b >> 5));
        SHZ_DCE_GATHER_PREF(bits + ((b + w - 1u) >> 5));
    }
}

/** \brief Prefetch the cache lines shz_dce_copy3_gather() will read for the vertices idx[0..count-1] of an unpacked table: the line of
    each entry's first float and of its last float (the same line for most entries). Reads idx and no table data; changes nothing.
    Cycles E: 12.00 per entry in the cache-hit model (a cost there, not a gain). */
SHZ_FORCE_INLINE void shz_dce_prefetch_copy3_gather(const float *SHZ_RESTRICT table, const uint32_t *SHZ_RESTRICT idx, size_t count) SHZ_NOEXCEPT {
    size_t k;
    SHZ_DCE_ASSERT_ALIGNED(table, 4);
    SHZ_DCE_ASSERT_ALIGNED(idx, 4);
    for (k = 0; k < count; k++) {
        const float *e;
        assert(idx[k] < SHZ_DCE_GATHER_INDEX_LIMIT);
        e = table + 3u * idx[k];
        SHZ_DCE_GATHER_PREF(e);
        SHZ_DCE_GATHER_PREF(e + 2);
    }
}

/* Notes. The figures are ESTIMATES (E) from the interpreter's cache-hit model, read from cycles_gather.txt, per vertex or per index
   (-O2 and -O3 give the same figures for this code). A batch figure is the slope between batches of 64 and 192 elements (sh4bench).
   The figures are built without NDEBUG, so the assert of the index limit is in them: its compare waits for the index load, about
   1 cycle E an index (traced: 17 waits in 16 vertices). A build without asserts is not measured here.
   The 32-bit path issues 4 prefetches a vertex (one stream line, three table entries); the general path 5 (two stream lines, three
   table entries). The tests count them.
   - unpack3, 32-bit vertices, (bx, by, bz) = (11, 11, 10): 36.50 E in a batch of 64 to 192; 34.00 E in calls of 8. The same code without
     its prefetch passes: 24.12 E. The byte-by-byte reader written as a plain loop: 44.00 E.
   - unpack3, 32-bit vertices, (12, 10, 10): 36.75 E in a batch; 34.62 E in calls of 8. Without the prefetch passes: 25.12 E. The
     byte-by-byte reader: 43.00 E.
   - unpack3, a 30-bit vertex, (11, 10, 9) (most vertices span two words, so the second word is read): 63.38 E. The byte-by-byte reader: 65.00 E.
   - prefetch_gather: 12.00 E per index. prefetch_copy3_gather: 12.00 E per entry. A caller that prefetches the next batch pays this on top.
   - copy3: 12.62 E per entry in a batch; 13.00 E in calls of 8. The plain scalar copy of the same entries: 16.00 E.
   In the cache-hit model the prefetch passes cost cycles and buy nothing: the unpack with them is 36.50 E against 24.12 E without them.
   Their gain exists only where the console misses in the cache. That case is not measured here, and no figure for it is given.
   Where the cycles go (traced, 16 vertices of the 32-bit path, with the prefetch passes): pass 3 as above, about 3.5 E a vertex;
   pass 4, the copies, about 100 issue cycles per block of 8 (about 12.5 E a vertex), with a load-use wait on nearly every word:
   a pointer load, a dependent word load and a store, per word. The copies are in the no-prefetch core too (24.12 E in all). Not
   changed: a shorter copy needs the register allocation controlled, which C does not give; a hand-scheduled block is not tried. */

#ifdef __cplusplus
}
#endif

#endif /* SHZ_DCE_GATHER_H */
