/* SH4ZAM-DCE tests: shz_dce_project.h, the loops its cycle ESTIMATES are made from

   project_bench.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc with the fast-math flags and run by tests/sh4run/sh4bench: every bench_ function is
   `void f(void *out, const void *in0, const void *in1, size_t count)` on buffers of random floats in [-1, 1); the input is read as
   24-byte records. Three forms of every case: the routine, its plain C form, and the loop written the plain way (the one a caller
   has before it uses the routine). The cases: every vertex in front of the near plane with the clip tests (clip), the same with the
   skip test and no vertex marked (lazy), no clip test (inside), every vertex behind the near plane (near), every vertex marked (skip).
   The matrix moves the points to depth 2..4 (or -4..-2): loading it is the call's fixed part, which drops out.
*/

#include <sh4zam_dce/shz_dce_project.h>

#define PROJECT_PLAIN_LOOP
#include "project_jobs.h"

#define R SHZ_RESTRICT
#define N_MAX 256

static const shz_dce_view4_t bench_view = { 0.9f, -0.7f, 1.3f, -1.1f, 0.25f, 240.0f, 320.0f, 0.1f, 300.0f, 1.5f };
static const shz_mat4x4_t bench_front = { .elem = { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 3, 1 } };
static const shz_mat4x4_t bench_behind = { .elem = { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, -3, 1 } };
static uint8_t bench_codes[N_MAX];   /* zero: no vertex marked, and no run leaves a mark */
static uint8_t bench_marks[N_MAX] = { [0 ... N_MAX - 1] = SHZ_DCE_PROJECT_SKIP };
static uint32_t bench_words[N_MAX];
size_t bench_sink;

#define CASE(name, fn, matrix, codes, clip, lazy) \
    void name(float *R out, const float *R in, const float *R unused, size_t n) { \
        (void)unused; shz_xmtrx_load_4x4(&matrix); bench_sink = fn(out, codes, bench_words, in, 24, n, &bench_view, clip, lazy); }

CASE(bench_project_clip, shz_dce_project_vertices, bench_front, bench_codes, 1, 0)
CASE(bench_project_clip_c, shz_dce_project_vertices_c, bench_front, bench_codes, 1, 0)
CASE(bench_project_clip_plain, project_plain_loop, bench_front, bench_codes, 1, 0)
CASE(bench_project_lazy, shz_dce_project_vertices, bench_front, bench_codes, 1, 1)
CASE(bench_project_lazy_c, shz_dce_project_vertices_c, bench_front, bench_codes, 1, 1)
CASE(bench_project_lazy_plain, project_plain_loop, bench_front, bench_codes, 1, 1)
CASE(bench_project_inside, shz_dce_project_vertices, bench_front, bench_codes, 0, 0)
CASE(bench_project_inside_c, shz_dce_project_vertices_c, bench_front, bench_codes, 0, 0)
CASE(bench_project_inside_plain, project_plain_loop, bench_front, bench_codes, 0, 0)
CASE(bench_project_near, shz_dce_project_vertices, bench_behind, bench_codes, 1, 1)
CASE(bench_project_near_c, shz_dce_project_vertices_c, bench_behind, bench_codes, 1, 1)
CASE(bench_project_near_plain, project_plain_loop, bench_behind, bench_codes, 1, 1)
CASE(bench_project_skip, shz_dce_project_vertices, bench_front, bench_marks, 1, 1)
CASE(bench_project_skip_c, shz_dce_project_vertices_c, bench_front, bench_marks, 1, 1)
CASE(bench_project_skip_plain, project_plain_loop, bench_front, bench_marks, 1, 1)
