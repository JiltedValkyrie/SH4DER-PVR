/* SH4ZAM-DCE tests: shz_dce_xform.h as the SH-4 runs it

   xform_check.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   xform_check <xform_kernels.elf>
   - the SH-4 loop against the plain C form, both on the SH-4, for every count from 0 up and the three modes: the 20 bytes each writes
     of every record, every clip code and the returned OR must be the same bytes ("bit-identical: yes")
   - the positions against the exact product (in double) within the vector unit's bound, the clip codes against the compares written
     out on the positions the SH-4 produced (exact: a code is a function of its position's bits)
   - the 12 bytes of a record the routine does not write stay as the cache line allocation left them, the record after the last is
     untouched, XMTRX is unchanged, a second run with other garbage in the scratch registers gives the same bytes
   - the input sits on a multiple of 4 that is not one of 8; the output on a multiple of 32, and once on a multiple of 4 only
*/

#include <sh4zam_dce/shz_dce_xform.h>

#include "sh4_check.h"
#include "xform_jobs.h"

#define BOUND 4.76837158203125e-07   /* 2^-21 */

static void fill_matrix(float m[16]) {   /* a rotation-like 3 x 4 with a translation, column major, last row 0 0 0 1 */
    int c, r;
    for (c = 0; c < 4; c++)
        for (r = 0; r < 4; r++)
            m[4 * c + r] = r == 3 ? (c == 3 ? 1.0f : 0.0f) : c == 3 ? t_frand(-20, 20) : t_frand(-1, 1);
}

static unsigned code_of(const float p[3], int mode, const shz_dce_clip_t *clip) {
    return mode == SHZ_DCE_XFORM_NONE ? 0u : mode == SHZ_DCE_XFORM_SIDES ? shz_dce_clip_sides(p[0], p[1], p[2], clip->kx, clip->ky) : shz_dce_clip_full(p[0], p[1], p[2], clip);
}

static void run(uint32_t count, int mode, int out_align32) {
    const uint32_t aj = ram(sizeof(job_xform_t), 4, 0), am = ram(64, 8, 0), acl = ram(sizeof(shz_dce_clip_t), 4, 0), ain = ram(count * 32 + 8, 8, 4);
    const uint32_t ao1 = ram(count * 32 + 64, 32, out_align32 ? 0 : 4), ao2 = ram(count * 32 + 64, 32, out_align32 ? 0 : 4);
    const uint32_t ac1 = ram(count + 4, 1, 0), ac2 = ram(count + 4, 1, 0);
    job_xform_t *j = (job_xform_t *)host(aj, sizeof *j);
    float *m = (float *)host(am, 64), *in = (float *)host(ain, count * 32 + 8), x[16];
    shz_dce_clip_t *clip = (shz_dce_clip_t *)host(acl, sizeof *clip);
    uint8_t *o1 = (uint8_t *)host(ao1, count * 32 + 64), *o2 = (uint8_t *)host(ao2, count * 32 + 64), *c1 = (uint8_t *)host(ac1, count + 4), *c2 = (uint8_t *)host(ac2, count + 4);
    uint8_t *keep = (uint8_t *)malloc(count * 32 + 1), *keepc = (uint8_t *)malloc(count + 1);
    uint32_t k, any = 0u, ret1;

    fill_matrix(m);
    clip->kx = t_frand(0.5f, 1.5f); clip->ky = t_frand(0.5f, 1.5f); clip->gkx = 2.0f * clip->kx; clip->gky = 2.0f * clip->ky;
    clip->near_z = 0.0625f; clip->far_z = 30.0f;
    for (k = 0; k < count * 8; k++) {
        const uint32_t bits = t_rand();
        if (k % 8 < 3) in[k] = t_frand(-25, 25);
        else memcpy(&in[k], &bits, 4);   /* the words carried through and the bytes not read: any bit pattern */
    }
    memset(o1, 0x5a, count * 32 + 64); memset(o2, 0x5a, count * 32 + 64); memset(c1, 0xee, count + 4); memset(c2, 0xee, count + 4);
    j->matrix = am; j->clip = acl; j->in = ain; j->count = count; j->mode = (uint32_t)mode; j->out_asm = ao1; j->codes_asm = ac1; j->out_c = ao2; j->codes_c = ac2;
    call("_t_xform", aj);
    T_CHECK(j->ret_asm == j->ret_c, "count %u mode %d: returned %x, C form %x", (unsigned)count, mode, (unsigned)j->ret_asm, (unsigned)j->ret_c);
    T_CHECK(!memcmp(c1, c2, count + 4), "count %u mode %d: clip codes differ from the C form (or written past the end)", (unsigned)count, mode);
    T_CHECK(c1[count] == 0xee, "count %u mode %d: a clip code written past the end", (unsigned)count, mode);
    for (k = 0; k < count; k++) {
        const float *p = (const float *)(o1 + 32 * k), *s = in + 8 * k;
        int r, c;
        T_CHECK(!memcmp(o1 + 32 * k, o2 + 32 * k, 20), "count %u mode %d: record %u differs from the C form", (unsigned)count, mode, (unsigned)k);
        T_CHECK(!memcmp(o1 + 32 * k + 12, s + 3, 8), "count %u mode %d: record %u: the two words were not carried through", (unsigned)count, mode, (unsigned)k);
        for (r = 0; r < 3; r++) {   /* row r of the column-major matrix times (x, y, z, 1) */
            double e = (double)m[12 + r], pm = fabs((double)m[12 + r]);
            for (c = 0; c < 3; c++) { const double t = (double)m[4 * c + r] * (double)s[c]; e += t; if (fabs(t) > pm) pm = fabs(t); }
            T_CHECK(fabs((double)p[r] - e) <= (pm + fabs(e)) * BOUND, "count %u: record %u axis %d: %.9g, exact %.9g", (unsigned)count, (unsigned)k, r, (double)p[r], e);
        }
        T_CHECK(c1[k] == code_of(p, mode, clip), "count %u mode %d: record %u code %02x, the compares give %02x", (unsigned)count, mode, (unsigned)k, c1[k], code_of(p, mode, clip));
        any |= c1[k];
        if (out_align32) {   /* the 12 bytes not written: what the line allocation left (the interpreter's poison), not the old contents */
            T_CHECK(o1[32 * k + 20] == 0xa5 && o1[32 * k + 31] == 0xa5, "count %u: record %u: the unwritten 12 bytes are not what a line allocation leaves", (unsigned)count, (unsigned)k);
        } else {
            T_CHECK(o1[32 * k + 20] == 0x5a && o1[32 * k + 31] == 0x5a, "count %u: record %u: bytes 20..31 were written", (unsigned)count, (unsigned)k);
        }
    }
    T_CHECK(j->ret_asm == any, "count %u mode %d: returned %x, the OR of the codes is %x", (unsigned)count, mode, (unsigned)j->ret_asm, (unsigned)any);
    for (k = 0; k < 32; k++) T_CHECK(o1[count * 32 + k] == 0x5a, "count %u mode %d: the record after the last was touched", (unsigned)count, mode);
    sh4_xmtrx_get(S, x);
    T_CHECK(!memcmp(x, m, 64), "count %u mode %d: XMTRX changed", (unsigned)count, mode);
    memcpy(keep, o1, count * 32); memcpy(keepc, c1, count); ret1 = j->ret_asm;
    memset(o1, 0x5a, count * 32); memset(c1, 0xee, count);
    call("_t_xform", aj);   /* again, other garbage in the scratch registers */
    for (k = 0; k < count; k++) T_CHECK(!memcmp(keep + 32 * k, o1 + 32 * k, 20), "count %u mode %d: a second run gave other bytes in record %u", (unsigned)count, mode, (unsigned)k);
    T_CHECK(!memcmp(keepc, c1, count) && ret1 == j->ret_asm, "count %u mode %d: a second run gave other codes", (unsigned)count, mode);
    free(keep); free(keepc);
    sh4_alloc_reset(S);
}

/* Points exactly on every plane of the view volume and one unit in the last place to either side of it, through an identity matrix
   (which the vector unit passes through unchanged): the compares must be the documented ones to the last bit, in the SH-4 loop and in
   the compiled C form alike. A form that compared x + kx z with 0, say, would differ here. */
static void boundary(int mode, uint32_t extra) {
    static const float zs[] = { 0.0625f, 0.7f, 3.3f, 29.999f, 30.0f, 123.456f };
    float pts[6 * 5 * 5 * 3 + 16 * 3];
    uint32_t n = 0, k, zi;
    shz_dce_clip_t c;
    c.kx = 1.1f; c.ky = 0.9f; c.gkx = 2.3f; c.gky = 1.7f; c.near_z = 0.0625f; c.far_z = 30.0f;
    for (zi = 0; zi < 6; zi++) {
        const float z = zs[zi];
        const float xs[5] = { c.kx * z, -(c.kx * z), c.gkx * z, -(c.gkx * z), 0.0f }, ys[5] = { c.ky * z, -(c.ky * z), c.gky * z, -(c.gky * z), 0.0f };
        int a, b;
        for (a = 0; a < 5; a++)
            for (b = 0; b < 5; b++) {
                const float dx = (a + b) % 3 == 0 ? 0.0f : (a + b) % 3 == 1 ? 1.0f : -1.0f;   /* on the plane, one ulp past it, one ulp short */
                /* (around 0 a tiny normal number, not the next float: that one is a denormal, which the SH-4 here reads as 0) */
                pts[n * 3] = dx == 0.0f ? xs[a] : xs[a] == 0.0f ? dx * 1e-30f : nextafterf(xs[a], xs[a] + dx * 100.0f);
                pts[n * 3 + 1] = (b % 2 || dx == 0.0f) ? ys[b] : ys[b] == 0.0f ? -dx * 1e-30f : nextafterf(ys[b], ys[b] - dx * 100.0f);
                pts[n * 3 + 2] = z;
                n++;
            }
    }
    for (k = 0; k < 16; k++) {   /* the depth planes themselves, and negative depths */
        const float base = k < 8 ? c.near_z : c.far_z;
        pts[n * 3] = t_frand(-0.01f, 0.01f); pts[n * 3 + 1] = t_frand(-0.01f, 0.01f);
        pts[n * 3 + 2] = k % 8 == 0 ? base : k % 8 == 1 ? nextafterf(base, 1e9f) : k % 8 == 2 ? nextafterf(base, -1e9f) : k % 8 == 3 ? -base : t_frand(-2, 40);
        n++;
    }
    n -= extra;   /* an odd and an even count */
    {
        const uint32_t aj = ram(sizeof(job_xform_t), 4, 0), am = ram(64, 8, 0), acl = ram(sizeof c, 4, 0), ain = ram(n * 32 + 8, 8, 4);
        const uint32_t ao1 = ram(n * 32 + 64, 32, 0), ao2 = ram(n * 32 + 64, 32, 0), ac1 = ram(n + 4, 1, 0), ac2 = ram(n + 4, 1, 0);
        job_xform_t *j = (job_xform_t *)host(aj, sizeof *j);
        float *m = (float *)host(am, 64), *in = (float *)host(ain, n * 32 + 8);
        uint8_t *o1 = (uint8_t *)host(ao1, n * 32), *o2 = (uint8_t *)host(ao2, n * 32), *c1 = (uint8_t *)host(ac1, n), *c2 = (uint8_t *)host(ac2, n);
        unsigned seen = 0u;
        memset(m, 0, 64); m[0] = m[5] = m[10] = m[15] = 1.0f;
        memcpy(host(acl, sizeof c), &c, sizeof c);
        memset(in, 0, n * 32 + 8);
        for (k = 0; k < n; k++) memcpy(in + 8 * k, pts + 3 * k, 12);
        j->matrix = am; j->clip = acl; j->in = ain; j->count = n; j->mode = (uint32_t)mode; j->out_asm = ao1; j->codes_asm = ac1; j->out_c = ao2; j->codes_c = ac2;
        call("_t_xform", aj);
        for (k = 0; k < n; k++) {
            const float *p = (const float *)(o1 + 32 * k);
            T_CHECK(!memcmp(p, pts + 3 * k, 12), "boundary: an identity matrix changed point %u", (unsigned)k);
            T_CHECK(!memcmp(o1 + 32 * k, o2 + 32 * k, 20), "boundary mode %d: record %u differs from the C form", mode, (unsigned)k);
            T_CHECK(c1[k] == c2[k], "boundary mode %d: point %u (%.9g %.9g %.9g): code %02x, C form %02x", mode, (unsigned)k, (double)p[0], (double)p[1], (double)p[2], c1[k], c2[k]);
            T_CHECK(c1[k] == code_of(p, mode, &c), "boundary mode %d: point %u (%.9g %.9g %.9g): code %02x, the compares give %02x", mode, (unsigned)k, (double)p[0],
                    (double)p[1], (double)p[2], c1[k], code_of(p, mode, &c));
            seen |= c1[k];
        }
        T_CHECK(mode == SHZ_DCE_XFORM_NONE ? seen == 0u : mode == SHZ_DCE_XFORM_SIDES ? seen == 0x3cu : seen == 0x7fu, "boundary mode %d: the points only reached bits %02x", mode, seen);
        sh4_alloc_reset(S);
    }
}

int main(int argc, char **argv) {
    const int rc = check_open(argc, argv);
    uint32_t count;
    int mode;
    if (rc) return rc;
    for (mode = 0; mode <= 2; mode++) {
        for (count = 0; count <= 41; count++) run(count, mode, 1);
        run(257, mode, 1);
        for (count = 0; count <= 9; count++) run(count, mode, 0);
        boundary(mode, 0);
        boundary(mode, 1);
    }
    printf("xform: SH-4 loop == C form on the SH-4 (positions, carried words, clip codes), counts 0..41 and 257, three modes\n");
    return t_done("xform_check");
}
