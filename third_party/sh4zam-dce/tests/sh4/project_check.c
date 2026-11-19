/* SH4ZAM-DCE tests: shz_dce_project.h as the SH-4 runs it

   project_check.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   project_check <project_kernels.elf>
   - the SH-4 loops against the plain C form, both on the SH-4, for every count from 0 up, with and without the clip tests, with and
     without skipped vertices, four strides: the 20 bytes every processed record gets, the 12 bytes of its screen point when the near
     bit is clear, every clip code, every copied word and the returned count must be the same bytes ("bit-identical: yes")
   - in the builds that contract a product and a sum (-ffp-contract=fast): the same against the loop written the plain way
   - a skipped vertex keeps its record, its code (0x80) and its word; without `lazy` a code byte of 0x80 skips nothing
   - the positions against the exact product (in double) within the vector unit's bound; the clip codes against the compares written
     out on the positions the SH-4 produced; the screen point against the quotient in double within the reciprocal root's bound
   - a vertex behind the near plane has the near bit alone and its last 12 bytes are what the line allocation left; the record, the
     code and the word after the last are untouched; XMTRX is unchanged; a second run with other garbage in the scratch registers gives
     the same bytes
   - the input, the view and the words sit on a multiple of 4 that is not one of 8, the codes on an odd address; the output on a
     multiple of 32, and for a few counts on a multiple of 4 only
*/

#include <sh4zam_dce/shz_dce_project.h>

#include "sh4_check.h"
#include "project_jobs.h"

#define BOUND 4.76837158203125e-07   /* 2^-21 */

static unsigned plain_runs, plain_vertices;

static void fill_matrix(float m[16]) {   /* a rotation-like 3 x 4 with a translation, column major, last row 0 0 0 1 */
    int c, r;
    for (c = 0; c < 4; c++)
        for (r = 0; r < 4; r++)
            m[4 * c + r] = r == 3 ? (c == 3 ? 1.0f : 0.0f) : c == 3 ? t_frand(-20, 20) : t_frand(-1, 1);
}

static unsigned code_of(const float p[3], const shz_dce_view4_t *v, int clip) {
    unsigned c = 0u;
    if (!clip) return 0u;
    if (p[2] < v->near_z) return SHZ_DCE_PROJECT_NEAR;
    if (p[0] < v->x_lo * p[2]) c |= SHZ_DCE_PROJECT_X_LO;
    if (p[0] > v->x_hi * p[2]) c |= SHZ_DCE_PROJECT_X_HI;
    if (p[1] < v->y_lo * p[2]) c |= SHZ_DCE_PROJECT_Y_LO;
    if (p[1] > v->y_hi * p[2]) c |= SHZ_DCE_PROJECT_Y_HI;
    return c;
}

typedef struct {
    uint32_t aj, ain, av, am;
    uint32_t ao[3], ac[3], aw[3];
    job_project_t *j;
    uint8_t *o[3], *c[3], *w[3];   /* 0: the routine, 1: the C form, 2: the loop written the plain way */
    uint8_t *in;
    shz_dce_view4_t *view;
    float *m;
} bufs_t;

static void bufs_make(bufs_t *b, uint32_t count, uint32_t stride, int out_align32) {
    int r;
    b->aj = ram(sizeof(job_project_t), 4, 0); b->am = ram(64, 8, 0); b->av = ram(sizeof(shz_dce_view4_t), 8, 4); b->ain = ram(count * stride + 8, 8, 4);
    for (r = 0; r < 3; r++) {
        b->ao[r] = ram(count * 32 + 64, 32, out_align32 ? 0 : 4); b->ac[r] = ram(count + 5, 2, 1); b->aw[r] = ram(count * 4 + 8, 8, 4);
        b->o[r] = (uint8_t *)host(b->ao[r], count * 32 + 64); b->c[r] = (uint8_t *)host(b->ac[r], count + 5); b->w[r] = (uint8_t *)host(b->aw[r], count * 4 + 8);
    }
    b->j = (job_project_t *)host(b->aj, sizeof *b->j); b->m = (float *)host(b->am, 64); b->view = (shz_dce_view4_t *)host(b->av, sizeof *b->view);
    b->in = (uint8_t *)host(b->ain, count * stride + 8);
}

static void bufs_reset(bufs_t *b, uint32_t count, const uint8_t *codes0) {
    int r;
    for (r = 0; r < 3; r++) {
        memset(b->o[r], 0x5a, count * 32 + 64); memset(b->c[r], 0xee, count + 5); memcpy(b->c[r], codes0, count); memset(b->w[r], 0xee, count * 4 + 8);
    }
}

static void job_set(bufs_t *b, uint32_t count, uint32_t stride, int clip, int lazy) {
    job_project_t *j = b->j;
    j->matrix = b->am; j->view = b->av; j->in = b->ain; j->stride = stride; j->count = count; j->clip = (uint32_t)clip; j->lazy = (uint32_t)lazy;
    j->out_asm = b->ao[0]; j->codes_asm = b->ac[0]; j->words_asm = b->aw[0];
    j->out_c = b->ao[1]; j->codes_c = b->ac[1]; j->words_c = b->aw[1];
    j->out_p = b->ao[2]; j->codes_p = b->ac[2]; j->words_p = b->aw[2];
}

/* everything the three runs left, one against the other and against what the comment of the routine promises */
static void compare(bufs_t *b, uint32_t count, uint32_t stride, int clip, int lazy, int out_align32, const uint8_t *codes0, const char *what) {
    const job_project_t *j = b->j;
    const int runs = j->plain ? 3 : 2;
    uint32_t k, want_done = 0;
    int r;

    for (k = 0; k < count; k++) want_done += !(lazy && codes0[k] == SHZ_DCE_PROJECT_SKIP);
    T_CHECK(j->ret_asm == want_done && j->ret_c == want_done, "%s count %u: returned %u, C form %u, processed %u", what, (unsigned)count, (unsigned)j->ret_asm,
            (unsigned)j->ret_c, (unsigned)want_done);
    if (j->plain) { T_CHECK(j->ret_p == want_done, "%s count %u: the plain loop returned %u of %u", what, (unsigned)count, (unsigned)j->ret_p, (unsigned)want_done); plain_runs++; }
    for (r = 0; r < runs; r++) {
        for (k = 0; k < 32; k++) T_CHECK(b->o[r][count * 32 + k] == 0x5a, "%s count %u run %d: the record after the last was touched", what, (unsigned)count, r);
        T_CHECK(b->c[r][count] == 0xee && b->c[r][count + 1] == 0xee, "%s count %u run %d: a code written past the end", what, (unsigned)count, r);
        T_CHECK(b->w[r][count * 4] == 0xee && b->w[r][count * 4 + 3] == 0xee, "%s count %u run %d: a word written past the end", what, (unsigned)count, r);
    }
    for (k = 0; k < count; k++) {
        const uint8_t *s = b->in + k * stride, *o = b->o[0] + 32 * k;
        const int skipped = lazy && codes0[k] == SHZ_DCE_PROJECT_SKIP;
        float p[3], src[3];
        unsigned code;
        int i, c;

        if (skipped) {
            for (r = 0; r < runs; r++) {
                for (i = 0; i < 32; i++) T_CHECK(b->o[r][32 * k + i] == 0x5a, "%s count %u run %d: the record of skipped vertex %u was written", what, (unsigned)count, r, (unsigned)k);
                T_CHECK(b->c[r][k] == SHZ_DCE_PROJECT_SKIP, "%s count %u run %d: the code of skipped vertex %u is %02x", what, (unsigned)count, r, (unsigned)k, b->c[r][k]);
                T_CHECK(b->w[r][4 * k] == 0xee && b->w[r][4 * k + 3] == 0xee, "%s count %u run %d: the word of skipped vertex %u was written", what, (unsigned)count, r, (unsigned)k);
            }
            continue;
        }
        memcpy(p, o, 12); memcpy(src, s, 12);
        code = b->c[0][k];
        T_CHECK(code == code_of(p, b->view, clip), "%s count %u: vertex %u (%.9g %.9g %.9g): code %02x, the compares give %02x", what, (unsigned)count, (unsigned)k,
                (double)p[0], (double)p[1], (double)p[2], code, code_of(p, b->view, clip));
        T_CHECK(!memcmp(o + 12, s + 12, 8), "%s count %u: vertex %u: the two words were not carried through", what, (unsigned)count, (unsigned)k);
        T_CHECK(!memcmp(b->w[0] + 4 * k, s + 20, 4), "%s count %u: vertex %u: the third word did not arrive", what, (unsigned)count, (unsigned)k);
        for (r = 1; r < runs; r++) {
            const char *who = r == 1 ? "the C form" : "the plain loop";
            T_CHECK(!memcmp(o, b->o[r] + 32 * k, 20), "%s count %u: record %u differs from %s", what, (unsigned)count, (unsigned)k, who);
            T_CHECK(b->c[r][k] == code, "%s count %u: vertex %u: code %02x, %s %02x", what, (unsigned)count, (unsigned)k, code, who, b->c[r][k]);
            T_CHECK(!memcmp(b->w[0] + 4 * k, b->w[r] + 4 * k, 4), "%s count %u: vertex %u: the third word differs from %s", what, (unsigned)count, (unsigned)k, who);
            if (!(code & SHZ_DCE_PROJECT_NEAR)) {
                T_CHECK(!memcmp(o + 20, b->o[r] + 32 * k + 20, 12), "%s count %u: the screen point of vertex %u differs from %s: %08x %08x %08x against %08x %08x %08x", what,
                        (unsigned)count, (unsigned)k, who, (unsigned)t_bits(((const float *)(const void *)o)[5]), (unsigned)t_bits(((const float *)(const void *)o)[6]),
                        (unsigned)t_bits(((const float *)(const void *)o)[7]), (unsigned)t_bits(((const float *)(const void *)(b->o[r] + 32 * k))[5]),
                        (unsigned)t_bits(((const float *)(const void *)(b->o[r] + 32 * k))[6]), (unsigned)t_bits(((const float *)(const void *)(b->o[r] + 32 * k))[7]));
            }
            if (r == 2) plain_vertices++;
        }
        for (i = 0; i < 3; i++) {   /* row i of the column-major matrix times (x, y, z, 1) */
            double e = (double)b->m[12 + i], pm = fabs((double)b->m[12 + i]);
            for (c = 0; c < 3; c++) { const double t = (double)b->m[4 * c + i] * (double)src[c]; e += t; if (fabs(t) > pm) pm = fabs(t); }
            T_CHECK(fabs((double)p[i] - e) <= (pm + fabs(e)) * BOUND, "%s count %u: vertex %u axis %d: %.9g, exact %.9g", what, (unsigned)count, (unsigned)k, i, (double)p[i], e);
        }
        if (code & SHZ_DCE_PROJECT_NEAR) {
            T_CHECK(code == SHZ_DCE_PROJECT_NEAR, "%s count %u: vertex %u: near with other bits (%02x)", what, (unsigned)count, (unsigned)k, code);
            if (out_align32) {   /* the 12 bytes not written: what the line allocation left (the interpreter's poison), not the old contents */
                T_CHECK(o[20] == 0xa5 && o[31] == 0xa5, "%s count %u: vertex %u: the unwritten 12 bytes are not what a line allocation leaves", what, (unsigned)count, (unsigned)k);
            } else {
                T_CHECK(o[20] == 0x5a && o[31] == 0x5a, "%s count %u: vertex %u: a screen point was written behind the near plane", what, (unsigned)count, (unsigned)k);
            }
        } else if (fabs((double)p[2]) > 1e-3) {   /* the screen point against the quotient in double: the formula, not the last bits */
            const shz_dce_view4_t *v = b->view;
            float sp[3];
            const double q = 1.0 / fabs((double)p[2]);
            const double tx = (double)v->focal * (double)p[0] * q, ty = (double)v->focal * (double)p[1] * q, tz = (double)v->depth_scale * q;
            memcpy(sp, o + 20, 12);
            T_CHECK(fabs((double)sp[0] - ((double)v->centre_x + tx)) <= (fabs(tx) + fabs((double)v->centre_x)) * BOUND, "%s count %u: vertex %u: sx %.9g, the quotient gives %.9g",
                    what, (unsigned)count, (unsigned)k, (double)sp[0], (double)v->centre_x + tx);
            T_CHECK(fabs((double)sp[1] - ((double)v->centre_y - ty)) <= (fabs(ty) + fabs((double)v->centre_y)) * BOUND, "%s count %u: vertex %u: sy %.9g, the quotient gives %.9g",
                    what, (unsigned)count, (unsigned)k, (double)sp[1], (double)v->centre_y - ty);
            T_CHECK(fabs((double)sp[2] - ((double)v->depth_base + tz)) <= (fabs(tz) + fabs((double)v->depth_base)) * BOUND, "%s count %u: vertex %u: sz %.9g, the quotient gives %.9g",
                    what, (unsigned)count, (unsigned)k, (double)sp[2], (double)v->depth_base + tz);
        }
    }
}

/* codes on entry: marks (0x80) on about three vertices of eight; the others any byte but the mark. mode 1: every byte the mark; 2: none. */
static void fill_codes(uint8_t *codes0, uint32_t count, int mode) {
    uint32_t k;
    for (k = 0; k < count; k++) {
        const uint32_t r = t_rand() >> 8;
        codes0[k] = mode == 1 ? SHZ_DCE_PROJECT_SKIP : (mode == 0 && (r & 7u) < 3u) ? SHZ_DCE_PROJECT_SKIP : (uint8_t)((r >> 3) & 0x7fu);
    }
}

static void run(uint32_t count, uint32_t stride, int clip, int lazy, int out_align32, int code_mode) {
    bufs_t b;
    uint8_t *codes0 = (uint8_t *)malloc(count + 1), *keep = (uint8_t *)malloc(count * 32 + 1), *keepc = (uint8_t *)malloc(count + 1), *keepw = (uint8_t *)malloc(count * 4 + 1);
    float x[16];
    uint32_t k, ret1;

    bufs_make(&b, count, stride, out_align32);
    fill_matrix(b.m);
    b.view->x_lo = t_frand(-1.5f, -0.5f); b.view->x_hi = t_frand(0.5f, 1.5f); b.view->y_lo = t_frand(-1.5f, -0.5f); b.view->y_hi = t_frand(0.5f, 1.5f);
    b.view->near_z = 0.0625f; b.view->focal = t_frand(200, 400); b.view->centre_x = t_frand(100, 500); b.view->centre_y = t_frand(100, 400);
    b.view->depth_base = t_frand(-1, 1); b.view->depth_scale = t_frand(0.5f, 2);
    for (k = 0; k < count * stride + 8; k++) b.in[k] = (uint8_t)(t_rand() >> 13);   /* the words carried through and the bytes not read: any bit pattern */
    for (k = 0; k < count; k++) {
        float pos[3];
        pos[0] = t_frand(-25, 25); pos[1] = t_frand(-25, 25); pos[2] = t_frand(-25, 25);
        memcpy(b.in + k * stride, pos, 12);
    }
    fill_codes(codes0, count, code_mode);
    bufs_reset(&b, count, codes0);
    job_set(&b, count, stride, clip, lazy);
    call("_t_project", b.aj);
    compare(&b, count, stride, clip, lazy, out_align32, codes0, "random");
    sh4_xmtrx_get(S, x);
    T_CHECK(!memcmp(x, b.m, 64), "count %u: XMTRX changed", (unsigned)count);
    memcpy(keep, b.o[0], count * 32); memcpy(keepc, b.c[0], count); memcpy(keepw, b.w[0], count * 4); ret1 = b.j->ret_asm;
    bufs_reset(&b, count, codes0);
    call("_t_project", b.aj);   /* again, other garbage in the scratch registers */
    for (k = 0; k < count; k++) {
        const int near = (keepc[k] & SHZ_DCE_PROJECT_NEAR) && !(lazy && codes0[k] == SHZ_DCE_PROJECT_SKIP);
        T_CHECK(!memcmp(keep + 32 * k, b.o[0] + 32 * k, near ? 20 : 32), "count %u: a second run gave other bytes in record %u", (unsigned)count, (unsigned)k);
    }
    T_CHECK(!memcmp(keepc, b.c[0], count) && !memcmp(keepw, b.w[0], count * 4) && ret1 == b.j->ret_asm, "count %u: a second run gave other codes, words or count", (unsigned)count);
    free(codes0); free(keep); free(keepc); free(keepw);
    sh4_alloc_reset(S);
}

/* Points exactly on every plane of the view volume and one unit in the last place to either side of it, through an identity matrix
   (which the vector unit passes through unchanged): the compares must be the documented ones to the last bit, in the SH-4 loop and in
   the compiled C form alike; and behind the near plane no side bit may show, however far outside the point is. */
static void boundary(int lazy, uint32_t extra) {
    static const float zs[] = { 0.0625f, 0.7f, 3.3f, 29.999f, 123.456f };
    float pts[5 * 5 * 5 * 3 + 16 * 3];
    shz_dce_view4_t v;
    uint32_t n = 0, k, zi;

    v.x_lo = -1.1f; v.x_hi = 1.3f; v.y_lo = -0.7f; v.y_hi = 0.9f; v.near_z = 0.0625f; v.focal = 300.0f; v.centre_x = 320.0f; v.centre_y = 240.0f;
    v.depth_base = 0.25f; v.depth_scale = 1.5f;
    for (zi = 0; zi < 5; zi++) {
        const float z = zs[zi];
        const float xs[5] = { v.x_lo * z, v.x_hi * z, 0.0f, v.x_lo * z, v.x_hi * z }, ys[5] = { v.y_lo * z, v.y_hi * z, 0.0f, v.y_hi * z, v.y_lo * z };
        int a, c;
        for (a = 0; a < 5; a++)
            for (c = 0; c < 5; c++) {
                const float dx = (a + c) % 3 == 0 ? 0.0f : (a + c) % 3 == 1 ? 1.0f : -1.0f;   /* on the plane, one ulp past it, one ulp short */
                /* (around 0 a tiny normal number, not the next float: that one is a denormal, which the SH-4 here reads as 0) */
                pts[n * 3] = dx == 0.0f ? xs[a] : xs[a] == 0.0f ? dx * 1e-30f : nextafterf(xs[a], xs[a] + dx * 100.0f);
                pts[n * 3 + 1] = (c % 2 || dx == 0.0f) ? ys[c] : ys[c] == 0.0f ? -dx * 1e-30f : nextafterf(ys[c], ys[c] - dx * 100.0f);
                pts[n * 3 + 2] = z;
                n++;
            }
    }
    for (k = 0; k < 16; k++) {   /* the near plane itself, a step to either side, behind the eye; far outside the four side planes */
        pts[n * 3] = k % 2 ? 1000.0f : -1000.0f; pts[n * 3 + 1] = k % 4 < 2 ? 900.0f : -900.0f;
        pts[n * 3 + 2] = k % 8 == 0 ? v.near_z : k % 8 == 1 ? nextafterf(v.near_z, 1e9f) : k % 8 == 2 ? nextafterf(v.near_z, -1e9f) : k % 8 == 3 ? -v.near_z :
                         k % 8 == 4 ? 0.0f : t_frand(-2, 0.06f);
        n++;
    }
    n -= extra;   /* an odd and an even count */
    {
        bufs_t b;
        uint8_t *codes0 = (uint8_t *)malloc(n + 1);
        unsigned seen = 0u, near_alone = 0u;

        bufs_make(&b, n, 24, 1);
        memset(b.m, 0, 64); b.m[0] = b.m[5] = b.m[10] = b.m[15] = 1.0f;
        *b.view = v;
        for (k = 0; k < n * 24 + 8; k++) b.in[k] = (uint8_t)(t_rand() >> 13);
        for (k = 0; k < n; k++) memcpy(b.in + 24 * k, pts + 3 * k, 12);
        fill_codes(codes0, n, 2);
        bufs_reset(&b, n, codes0);
        job_set(&b, n, 24, 1, lazy);
        call("_t_project", b.aj);
        compare(&b, n, 24, 1, lazy, 1, codes0, "boundary");
        for (k = 0; k < n; k++) {
            T_CHECK(!memcmp(b.o[0] + 32 * k, pts + 3 * k, 12), "boundary: an identity matrix changed point %u", (unsigned)k);
            seen |= b.c[0][k];
            near_alone += b.c[0][k] == SHZ_DCE_PROJECT_NEAR;
        }
        T_CHECK(seen == 0x1fu, "boundary: the points only reached bits %02x", seen);
        T_CHECK(near_alone >= 8u, "boundary: only %u points behind the near plane", near_alone);
        free(codes0);
        sh4_alloc_reset(S);
    }
}

int main(int argc, char **argv) {
    static const uint32_t strides[] = { 24, 28, 32, 40, 124 };
    const int rc = check_open(argc, argv);
    uint32_t count, si;
    int clip, lazy;
    if (rc) return rc;
    for (clip = 0; clip <= 1; clip++)
        for (lazy = 0; lazy <= 1; lazy++) {
            for (count = 0; count <= 41; count++) run(count, 24, clip, lazy, 1, 0);
            run(127, 24, clip, lazy, 1, 0);
            run(257, 24, clip, lazy, 1, 0);
            for (count = 0; count <= 9; count++) run(count, 24, clip, lazy, 0, 0);
            for (si = 1; si < 5; si++) { run(33, strides[si], clip, lazy, 1, 0); run(6, strides[si], clip, lazy, 0, 0); }
            run(40, 24, clip, lazy, 1, 1);   /* every code the mark: with lazy nothing is processed, without it everything */
            run(40, 24, clip, lazy, 1, 2);   /* no mark */
        }
    run(1500, 24, 1, 0, 1, 2);   /* long batches: more screen points for the comparison with the loop written the plain way */
    run(1500, 24, 1, 1, 1, 0);
    run(1500, 24, 0, 0, 1, 2);
    run(1500, 28, 0, 1, 1, 0);
    boundary(0, 0);
    boundary(0, 1);
    boundary(1, 0);
    printf("project: SH-4 loops == C form on the SH-4 (positions, carried words, clip codes, screen points, counts), counts 0..41, 127, 257, 1500, strides 24 28 32 40 124\n");
    printf("project: the loop written the plain way gave the same bytes in %u runs, %u vertices%s\n", plain_runs, plain_vertices,
           plain_runs ? "" : " (not compared: this build does not contract)");
    return t_done("project_check");
}
