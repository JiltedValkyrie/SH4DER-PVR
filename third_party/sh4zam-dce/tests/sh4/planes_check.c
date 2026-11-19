/* SH4ZAM-DCE tests: shz_dce_planes.h as the SH-4 runs it

   planes_check.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   planes_check <planes_kernels.elf>
   The ELF is planes_kernels.c built by sh-elf-gcc; tests/sh4run executes it instruction by instruction. For each routine:
   - the SH-4 loop against the plain C form of the same routine, both on the SH-4: bit for bit (they share FTRV / FIPR)
   - the SH-4 results against the exact value, inside the bound the header states
   - every call twice, the scratch registers seeded differently (NaNs the second time): the results must not change
   - the interpreter itself stops a run on an unaligned load or store, a callee-saved register or FPSCR mode bit not put back, a cache
     line allocated and not filled: so a pass here is also the "alignment: checked" of every instruction the routine executed
   Data is placed on the weakest alignment a routine's comment allows (points on a multiple of 4 that is not one of 8, and so on).
*/

#include <sh4zam_dce/shz_dce_planes.h>

#include "planes_jobs.h"
#include "sh4_check.h"

#define BOUND 4.76837158203125e-07   /* 2^-21 */

static double exact_dist(const float pl[4], const float p[3], double *pmax) {
    const double t[4] = { (double)pl[0] * p[0], (double)pl[1] * p[1], (double)pl[2] * p[2], (double)pl[3] };
    int k;
    *pmax = 0.0;
    for (k = 0; k < 4; k++) if (fabs(t[k]) > *pmax) *pmax = fabs(t[k]);
    return t[0] + t[1] + t[2] + t[3];
}

static void random_plane(float pl[4], float reach) {
    float n[3], len;
    do {
        n[0] = t_frand(-1, 1); n[1] = t_frand(-1, 1); n[2] = t_frand(-1, 1);
        len = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    } while (len < 0.1f);
    pl[0] = n[0] / len; pl[1] = n[1] / len; pl[2] = n[2] / len;
    pl[3] = t_frand(-reach, reach);
}

static void test_planes4(void) {
    static const uint32_t strides[] = { 12, 16, 32 };
    int trial, band = 0;
    for (trial = 0; trial < 300; trial++) {
        const uint32_t stride = strides[trial % 3], count = t_rand() % 41u;
        const uint32_t aj = ram(sizeof(job_planes4_t), 4, 0), ap = ram(64, 16, trial & 1 ? 8 : 0), apt = ram(count * stride + 4, 8, 4);
        const uint32_t ac1 = ram(count + 2, 1, 0), ac2 = ram(count + 2, 1, 0);
        job_planes4_t *j = (job_planes4_t *)host(aj, sizeof *j);
        float *pts = (float *)host(apt, count * stride + 4), x[16], pl[4][4];
        uint8_t *c1 = (uint8_t *)host(ac1, count + 2), *c2 = (uint8_t *)host(ac2, count + 2), keep[64];
        shz_dce_planes4_t four;
        uint32_t k, ret1;
        int i;
        for (i = 0; i < 4; i++) random_plane(pl[i], 4.0f);
        shz_dce_planes4_set(&four, shz_vec4_init(pl[0][0], pl[0][1], pl[0][2], pl[0][3]), shz_vec4_init(pl[1][0], pl[1][1], pl[1][2], pl[1][3]),
                            shz_vec4_init(pl[2][0], pl[2][1], pl[2][2], pl[2][3]), shz_vec4_init(pl[3][0], pl[3][1], pl[3][2], pl[3][3]));
        memcpy(host(ap, 64), &four, 64);
        for (k = 0; k < count * stride / 4; k++) pts[k] = t_frand(-6, 6);
        for (k = 0; k < count; k += 3) {   /* every third point put on a plane as nearly as floats allow: the vector unit's last bits decide its side */
            float *p = (float *)((char *)pts + k * stride);
            const float *q = pl[k % 4u];
            double pm;
            const double e = exact_dist(q, p, &pm);
            p[0] = (float)(p[0] - e * q[0]); p[1] = (float)(p[1] - e * q[1]); p[2] = (float)(p[2] - e * q[2]);
        }
        j->planes = ap; j->points = apt; j->stride = stride; j->count = count; j->codes_asm = ac1; j->codes_c = ac2;
        memset(c1, 0xee, count + 2); memset(c2, 0xee, count + 2);
        call("_t_planes4", aj);
        T_CHECK(!memcmp(c1, c2, count + 2), "planes4 stride %u count %u: the SH-4 loop and the C form differ", (unsigned)stride, (unsigned)count);
        T_CHECK(j->ret_asm == j->ret_c, "planes4: returned %x, C form %x", (unsigned)j->ret_asm, (unsigned)j->ret_c);
        T_CHECK(c1[count] == 0xee && c1[count + 1] == 0xee, "planes4: the SH-4 loop wrote past its last code");
        sh4_xmtrx_get(S, x);
        T_CHECK(!memcmp(x, &four, 64), "planes4: XMTRX does not hold the planes after the batch");
        memcpy(keep, c1, count < 64 ? count : 64); ret1 = j->ret_asm;
        call("_t_planes4", aj);   /* again, other garbage in the scratch registers */
        T_CHECK(!memcmp(keep, c1, count < 64 ? count : 64) && ret1 == j->ret_asm, "planes4: a second run gave other codes (a register read before it was written)");
        for (k = 0; k < count; k++) {   /* against the exact side of each plane: a bit may differ only inside the bound */
            const float *p = (const float *)((const char *)pts + k * stride);
            unsigned e_code = 0u, unsure = 0u;
            for (i = 0; i < 4; i++) {
                double pm;
                const double e = exact_dist(pl[i], p, &pm);
                if (e < 0.0) e_code |= 1u << i;
                if (fabs(e) <= (pm + fabs(e)) * BOUND) unsure |= 1u << i;
            }
            T_CHECK(!((c1[k] ^ e_code) & ~unsure), "planes4: code %x, exact %x, outside the bound", c1[k], e_code);
            if ((c1[k] ^ e_code) & unsure) band++;
        }
        sh4_alloc_reset(S);
    }
    printf("planes4: SH-4 loop == C form on the SH-4; %d codes differed from the exact side, all inside the bound\n", band);
}

static void test_planes6(void) {
    static const uint32_t strides[] = { 16, 20, 32 };
    int trial;
    for (trial = 0; trial < 300; trial++) {
        const uint32_t stride = strides[trial % 3], count = t_rand() % 33u;
        const uint32_t aj = ram(sizeof(job_planes6_t), 4, 0), ap = ram(96, 16, trial & 1 ? 8 : 0), as = ram(count * stride + 4, 8, 4);
        const uint32_t ao1 = ram(count + 2, 1, 0), ao2 = ram(count + 2, 1, 0), asg = ram(count * 4 + 4, 4, 0);
        job_planes6_t *j = (job_planes6_t *)host(aj, sizeof *j);
        float *sp = (float *)host(as, count * stride + 4), pl[6][4];
        uint8_t *o1 = (uint8_t *)host(ao1, count + 2), *o2 = (uint8_t *)host(ao2, count + 2), keep[64];
        uint32_t *single = (uint32_t *)host(asg, count * 4 + 4), k;
        shz_vec4_t pv[6];
        uint8_t *sim = (uint8_t *)host(ap, 96);
        int i;
        for (i = 0; i < 6; i++) { random_plane(pl[i], 4.0f); pv[i] = shz_vec4_init(pl[i][0], pl[i][1], pl[i][2], pl[i][3]); }
        {   /* the SH-4 layout: 16 floats column major, then the two cap planes */
            shz_dce_planes4_t four;
            shz_dce_planes4_set(&four, pv[0], pv[1], pv[2], pv[3]);
            memcpy(sim, &four, 64); memcpy(sim + 64, pl[4], 16); memcpy(sim + 80, pl[5], 16);
        }
        for (k = 0; k < count * stride / 4; k++) sp[k] = t_frand(-8, 8);
        for (k = 0; k < count; k++) {
            float *s = (float *)((char *)sp + k * stride);
            s[3] = t_frand(0.0f, 3.0f);
            if (k % 3u == 0u) {   /* every third sphere touching a plane as nearly as floats allow */
                double pm;
                s[3] = (float)fabs(exact_dist(pl[k % 6u], s, &pm));
            }
        }
        j->planes = ap; j->spheres = as; j->stride = stride; j->count = count; j->out_asm = ao1; j->out_c = ao2; j->single = asg;
        memset(o1, 0xee, count + 2); memset(o2, 0xee, count + 2);
        call("_t_planes6", aj);
        T_CHECK(!memcmp(o1, o2, count + 2), "planes6 stride %u count %u: the SH-4 loop and the C form differ", (unsigned)stride, (unsigned)count);
        T_CHECK(o1[count] == 0xee, "planes6: the SH-4 loop wrote past its last mask");
        memcpy(keep, o1, count < 64 ? count : 64);
        call("_t_planes6", aj);
        T_CHECK(!memcmp(keep, o1, count < 64 ? count : 64), "planes6: a second run gave other masks");
        for (k = 0; k < count; k++) {
            const float *s = (const float *)((const char *)sp + k * stride);
            unsigned out_e = 0u, cross_e = 0u, unsure = 0u;
            for (i = 0; i < 6; i++) {
                double pm;
                const double e = exact_dist(pl[i], s, &pm), bound = (pm + fabs(e)) * BOUND;
                if (e < -(double)s[3]) out_e |= 1u << i;
                else if (e <= (double)s[3]) cross_e |= 1u << i;
                if (fabs(e + s[3]) <= bound || fabs(e - s[3]) <= bound) unsure |= 1u << i;
            }
            T_CHECK(o1[k] == SHZ_DCE_SPHERE_OUT(single[k]), "planes6: batch mask %02x, single %02x", o1[k], (unsigned)SHZ_DCE_SPHERE_OUT(single[k]));
            T_CHECK(!((SHZ_DCE_SPHERE_OUT(single[k]) ^ out_e) & ~unsure), "planes6: OUT %02x, exact %02x, outside the bound", (unsigned)SHZ_DCE_SPHERE_OUT(single[k]), out_e);
            T_CHECK(!((SHZ_DCE_SPHERE_CROSS(single[k]) ^ cross_e) & ~unsure), "planes6: CROSS %02x, exact %02x, outside the bound", (unsigned)SHZ_DCE_SPHERE_CROSS(single[k]), cross_e);
        }
        sh4_alloc_reset(S);
    }
    printf("planes6: SH-4 loop == C form == single routine on the SH-4; classes exact outside the bound\n");
}

int main(int argc, char **argv) {
    const int rc = check_open(argc, argv);
    if (rc) return rc;
    test_planes4();
    test_planes6();
    return t_done("planes_check");
}
