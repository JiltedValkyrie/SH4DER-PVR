/* SH4ZAM-DCE tests: the record project_check.c hands to project_kernels.c, and the loop written the plain way

   project_jobs.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.
*/

#ifndef PROJECT_JOBS_H
#define PROJECT_JOBS_H

#include <stdint.h>

/* matrix: a shz_mat4x4_t to load into XMTRX first; view: a shz_dce_view4_t; in: records of `stride` bytes; out_*: 32-byte records;
   codes_*: count bytes, read and written; words_*: count 32-bit words. The three runs: the routine, its plain C form, and (plain = 1:
   the kernels were built with -ffp-contract=fast) the loop written the plain way below. */
typedef struct {
    uint32_t matrix, view, in, stride, count, clip, lazy;
    uint32_t out_asm, codes_asm, words_asm, ret_asm;
    uint32_t out_c, codes_c, words_c, ret_c;
    uint32_t out_p, codes_p, words_p, ret_p, plain;
} job_project_t;

#ifdef PROJECT_PLAIN_LOOP
/* a source record as a caller declares it: a position, two floats carried along, a 32-bit word */
typedef struct { float x, y, z, a, b; uint32_t w; } SHZ_ALIASING project_plain_rec_t;

/* What a caller has before it uses the routine: the transform, the clip code and the projection written as plain expressions, no
   fused step spelled out and nothing hidden from the optimiser. Built with -ffast-math -ffp-contract=fast the compiler fuses the
   three sums the way the routine does, and the two must then give the same bytes (project_check.c compares them). */
static inline size_t project_plain_loop(float *SHZ_RESTRICT out, uint8_t *SHZ_RESTRICT codes, uint32_t *SHZ_RESTRICT words, const void *SHZ_RESTRICT in,
                                        size_t stride, size_t count, const shz_dce_view4_t *view, int clip, int lazy) {
    const float xl = view->x_lo, xh = view->x_hi, yl = view->y_lo, yh = view->y_hi, nz = view->near_z;
    const float f = view->focal, cx = view->centre_x, cy = view->centre_y, zb = view->depth_base, zs = view->depth_scale;
    const int alloc = SHZ_DCE_IS_ALIGNED(out, 32);
    size_t done = 0, i;

    for (i = 0; i < count; i++) {
        const project_plain_rec_t *s = (const project_plain_rec_t *)((uintptr_t)in + i * stride);
        float *o = out + 8 * i;
        shz_vec3_t p;
        unsigned code = 0u;

        if (lazy && codes[i] == 0x80u)
            continue;
        done++;
        SHZ_PREFETCH(s + 4);
        if (alloc) shz_dcache_alloc_line(o);
        p = shz_xmtrx_transform_point3(shz_vec3_init(s->x, s->y, s->z));
        o[0] = p.x; o[1] = p.y; o[2] = p.z;
        o[3] = s->a; o[4] = s->b;   /* float moves: any bit pattern arrives as it is */
        words[i] = s->w;
        if (clip) {
            if (p.z < nz)
                code = 1u;
            else {
                if (p.x < xl * p.z) code |= 2u;
                if (p.x > xh * p.z) code |= 4u;
                if (p.y < yl * p.z) code |= 8u;
                if (p.y > yh * p.z) code |= 16u;
            }
        }
        if (!(code & 1u)) {
            const float inv = shz_invf_fsrra(p.z);
            o[5] = cx + f * p.x * inv;
            o[6] = cy - f * p.y * inv;
            o[7] = zb + zs * inv;
        }
        codes[i] = (uint8_t)code;
    }
    return done;
}
#endif

#endif /* PROJECT_JOBS_H */
