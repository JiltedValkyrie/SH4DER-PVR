/* SH4ZAM-DCE test tool: sh4run's own test, the SH-4 side

   selftest_sh4.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc (-ml -m4-single -ffp-contract=off, no -ffast-math), linked at 0x8c010000 and run by selftest.c through sh4run.
*/

#include "selftest_funcs.h"

#include <sh4zam/shz_sh4zam.h>

uint32_t sh_st_int(uint32_t *buf, uint32_t n) { return st_int(buf, n); }
uint32_t sh_st_float(float *f, uint32_t n) { return st_float(f, n); }
uint32_t sh_st_double(float *f, uint32_t n) { return st_double(f, n); }
uint32_t sh_st_fma(float *f, uint32_t n) { return st_fma(f, n); }
float sh_st_args(float a, int i, float b, float c, int j, float d, int k, float e, int l) { return st_args(a, i, b, c, j, d, k, e, l); }

/* the vector unit, through SH4ZAM's own routines */
void sh_st_ftrv(const shz_mat4x4_t *m, const float *v, float *out) {
    shz_xmtrx_load_4x4(m);
    const shz_vec4_t r = shz_xmtrx_transform_vec4(shz_vec4_init(v[0], v[1], v[2], v[3]));
    out[0] = r.x; out[1] = r.y; out[2] = r.z; out[3] = r.w;
}

void sh_st_fipr2(const float *l, const float *r1, const float *r2, float *out) {
    const shz_vec2_t d = shz_vec4_dot2(shz_vec4_init(l[0], l[1], l[2], l[3]), shz_vec4_init(r1[0], r1[1], r1[2], r1[3]),
                                       shz_vec4_init(r2[0], r2[1], r2[2], r2[3]));
    out[0] = d.x; out[1] = d.y;
}

void sh_st_fsrra(const float *in, float *out, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) out[i] = shz_inv_sqrtf_fsrra(in[i]);
}

void sh_st_fsca(const uint16_t *in, float *out, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        const shz_sincos_t sc = shz_sincosu16(in[i]);
        out[2 * i] = sc.sin; out[2 * i + 1] = sc.cos;
    }
}

/* XMTRX out again, by pairs (FPSCR.SZ = 1 stores) */
void sh_st_xstore(const shz_mat4x4_t *in, shz_mat4x4_t *out) {
    shz_xmtrx_load_4x4(in);
    shz_xmtrx_store_4x4(out);
}

/* a cache line allocated and only half written: the interpreter must leave poison in the rest */
void sh_st_movca(uint32_t *line) {
    shz_dcache_alloc_line(line);
    line[0] = 1u; line[1] = 2u; line[2] = 3u; line[3] = 4u;
}

/* the faults the interpreter must catch */
uint32_t sh_st_bad_read32(const volatile uint32_t *p) { return *p; }
uint32_t sh_st_bad_read16(const volatile uint16_t *p) { return *p; }
void sh_st_bad_pair(const shz_mat4x4_t *m) { shz_xmtrx_load_4x4(m); }

__attribute__((naked)) void sh_st_bad_r8(void) { __asm__("mov #1, r8\n\trts\n\tnop"); }
__attribute__((naked)) void sh_st_bad_fr12(void) { __asm__("fldi1 fr12\n\trts\n\tnop"); }
__attribute__((naked)) void sh_st_bad_fschg(void) { __asm__("fschg\n\trts\n\tnop"); }
__attribute__((naked)) void sh_st_bad_sp(void) { __asm__("add #-4, r15\n\trts\n\tnop"); }

/* reads a scratch float register it never wrote: two runs must disagree (the interpreter seeds them differently) */
__attribute__((naked)) float sh_st_reads_garbage(void) { __asm__("fmov fr3, fr0\n\trts\n\tnop"); }
