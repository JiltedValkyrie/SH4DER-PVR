/* SH4DER-PVR: the bloom effect

   test/bloom_test.c
   Copyright (c) 2026 Jilted Valkyrie

   Host check: the shipped levels and positions at fixed ticks for the game and
   the MAX WHITE presets (tools/sh4der_bloom_storyboard.py bloom_ref), the
   colour ramp, the MAX WHITE blade keeping its structure, the counts, the
   ribbon and the disc facing the eye, the feather across, bad params.
*/

#include <math.h>
#include <stdio.h>
#include "../include/sh4der_bloom.h"

#ifndef SH4DER_TEST_TOL
#define SH4DER_TEST_TOL 0.0002f
#endif
#define TOL SH4DER_TEST_TOL

static int fail(const char *msg) {
    fprintf(stderr, "FAIL %s\n", msg);
    return 1;
}

static int near(float a, float b, float tol) {
    return fabsf(a - b) <= tol;
}

static const float eye[3] = { 0.15f, -2.0f, 0.008f };   /* the storyboard's side view */

/* bloom_ref pins: levels of vertices 6, 7, 8 (the blade-tip row), 12 (tail centre), 15 (disc centre, side-on: the flare's floor), 16 (rim). */
static int shipped(void) {
    static const struct { int max; uint32_t tick; float lv[6]; } pin[] = {
        { 0, 0, { 0.24481f, 0.24750f, 0.15300f, 0.00000f, 0.07425f, 0.00000f } },
        { 0, 48, { 0.55638f, 0.45000f, 0.44511f, 0.00000f, 0.13500f, 0.00000f } },
        { 0, 144, { 0.27819f, 0.45000f, 0.55638f, 0.00000f, 0.13500f, 0.00000f } },
        { 1, 0, { 0.29377f, 0.32000f, 0.19758f, 0.00000f, 0.72000f, 0.00000f } },
        { 1, 48, { 0.49395f, 0.40000f, 0.36721f, 0.00000f, 0.90000f, 0.00000f } },
        { 1, 144, { 0.24697f, 0.40000f, 0.49395f, 0.00000f, 0.90000f, 0.00000f } },
    };
    static const int at[6] = { 6, 7, 8, 12, 15, 16 };   /* the blade-tip row (mid, centre, mid), the tail centre, the disc centre and rim */
    sh4der_blade_params_t bp;
    sh4der_blade_frame_t f;
    sh4der_bloom_params_t p;
    sh4der_bloom_mesh_t m;
    unsigned i;
    int k;

    for (i = 0; i < sizeof pin / sizeof pin[0]; i++) {
        if (pin[i].max) {
            sh4der_blade_params_white(&bp);
            sh4der_bloom_params_max(&p);
        } else {
            sh4der_blade_params_default(&bp);
            sh4der_bloom_params_default(&p);
        }
        if (!sh4der_blade_prepare(&f, &bp, pin[i].tick, 1.0f) || !sh4der_bloom_blade(&m, &p, &bp, &f, eye))
            return fail("prepare");
        for (k = 0; k < 6; k++)
            if (!near(m.level[at[k]], pin[i].lv[k], 0.0001f + TOL)) {
                fprintf(stderr, "preset %d tick %u vertex %d: %f, want %f\n", pin[i].max, (unsigned)pin[i].tick, at[k], m.level[at[k]], pin[i].lv[k]);
                return fail("shipped level");
            }
        /* the ribbon's root edge (vertex 4) and the first rim vertex */
        if (!near(m.xyz[4][0], -0.065f, 1e-5f) || !near(m.xyz[4][1], 0.0f, 1e-4f) ||
            !near(m.xyz[4][2], pin[i].max ? -0.10200f : -0.07950f, 0.0001f + TOL))
            return fail("ribbon edge position");
        if (!near(m.xyz[16][0], pin[i].max ? 0.25861f : 0.29669f, 0.0001f + TOL) || !near(m.xyz[16][1], pin[i].max ? 0.00914f : 0.00533f, 0.0001f + TOL) ||
            !near(m.xyz[16][2], pin[i].max ? -0.21373f : -0.12134f, 0.0001f + TOL))
            return fail("rim position");
    }
    return 0;
}

static int ramp(void) {
    static const float avg[3] = { 0.652f, 0.889f, 1.0f }, edge_want[3] = { 0.44320f, 0.82240f, 1.0f }, core_want[3] = { 0.91300f, 0.97225f, 1.0f };
    static const float half_want[3] = { 0.28033f, 0.42993f, 0.5f };
    float edge[3], core[3], c[3], black[3] = { 0, 0, 0 };
    int i;

    sh4der_ramp_colours(avg, 0.0f, edge, core);
    sh4der_ramp_(edge, core, 0.5f, c);
    for (i = 0; i < 3; i++)
        if (!near(edge[i], edge_want[i], 0.0002f) || !near(core[i], core_want[i], 0.0002f) || !near(c[i], half_want[i], 0.0002f))
            return fail("ramp colours");
    sh4der_ramp_colours(avg, 1.0f, edge, core);
    for (i = 0; i < 3; i++)
        if (edge[i] != 1.0f || core[i] != 1.0f)
            return fail("white ramp");
    sh4der_ramp_colours(black, 0.0f, edge, core);
    if (!(edge[0] < 0.1f && edge[1] == 1.0f && edge[2] == 1.0f))
        return fail("black gives cyan");
    sh4der_ramp_(edge, core, 3.0f, c);
    for (i = 0; i < 3; i++)
        if (c[i] < 0.0f || c[i] > 1.0f)
            return fail("ramp clamps");
    return 0;
}

/* MAX WHITE keeps its structure: across the blade at the top of a pulse the intensity is not flat, the blocker's knot is at full. */
static int white_structure(void) {
    sh4der_blade_params_t bp;
    sh4der_blade_frame_t f;
    float lo = 2.0f, hi = -1.0f;
    int k;

    sh4der_blade_params_white(&bp);
    if (!sh4der_blade_params_valid(&bp) || !sh4der_blade_prepare(&f, &bp, 48, 1.0f))
        return fail("white params");
    if (!near(sh4der_blade_intensity(&f, 0.2f, 0.0f, 0.06f), 1.0f, TOL) || !near(sh4der_blade_intensity(&f, 0.2f, 0.0f, -0.05f), 0.83745f, 0.0001f + TOL) ||
        !near(sh4der_blade_intensity(&f, 0.15f, -0.03f, 0.008f), 1.0f, TOL))
        return fail("white pins");
    for (k = 0; k < 64; k++) {
        const float v = sh4der_blade_intensity(&f, 0.3f, 0.0f, -0.07f + 0.15f * (float)k / 63.0f);

        if (v < lo) lo = v;
        if (v > hi) hi = v;
    }
    if (!(hi - lo > 0.1f) || lo < 0.5f)
        return fail("MAX WHITE is a flat silhouette");
    return 0;
}

static int geometry(void) {
    sh4der_blade_params_t bp;
    sh4der_blade_frame_t f;
    sh4der_bloom_params_t p;
    sh4der_bloom_mesh_t m;
    static const float eyes[3][3] = { { 0.15f, -2.0f, 0.008f }, { 0.4f, 0.7f, 1.5f }, { -1.0f, 0.3f, -0.8f } };
    uint32_t argb[SH4DER_BLOOM_VERTS];
    int e, i, k, used[SH4DER_BLOOM_VERTS] = { 0 };

    if (SH4DER_BLOOM_VERTS != SH4DER_BLOOM_ROWS * SH4DER_BLOOM_COLS + 1 + SH4DER_BLOOM_RIM ||
        SH4DER_BLOOM_TRIS != (SH4DER_BLOOM_ROWS - 1) * (SH4DER_BLOOM_COLS - 1) * 2 + SH4DER_BLOOM_RIM || SH4DER_BLOOM_TRIS > 24)
        return fail("counts");
    for (i = 0; i < SH4DER_BLOOM_TRIS; i++)
        for (k = 0; k < 3; k++) {
            if (sh4der_bloom_tris[i][k] >= SH4DER_BLOOM_VERTS)
                return fail("index");
            used[sh4der_bloom_tris[i][k]] = 1;
        }
    for (i = 0; i < SH4DER_BLOOM_VERTS; i++)
        if (!used[i])
            return fail("an unused vertex");
    sh4der_blade_params_default(&bp);
    sh4der_bloom_params_default(&p);
    sh4der_blade_prepare(&f, &bp, 48, 1.0f);
    for (e = 0; e < 3; e++) {
        const float *c = m.xyz[15];
        float v[3], w[3], d;

        sh4der_bloom_blade(&m, &p, &bp, &f, eyes[e]);
        for (k = 0; k < 3; k++)
            v[k] = eyes[e][k] - c[k];
        d = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        for (i = 16; i < 16 + SH4DER_BLOOM_RIM; i++) {   /* the disc faces the eye: every rim offset is across the view, at the radius */
            float r[3], len, dot;

            for (k = 0; k < 3; k++)
                r[k] = m.xyz[i][k] - c[k];
            len = sqrtf(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
            dot = (r[0] * v[0] + r[1] * v[1] + r[2] * v[2]) / (len * d);
            if (!near(len, p.disc_radius, 0.0005f) || fabsf(dot) > 0.002f)
                return fail("disc does not face the eye");
        }
        for (k = 0; k < 3; k++)   /* the ribbon's width direction: across the axis, facing the eye */
            w[k] = m.xyz[4][k] - m.xyz[0][k];
        if (fabsf(w[0]) > 1e-5f || fabsf(w[1] * (eyes[e][1] - bp.axis[0]) + w[2] * (eyes[e][2] - bp.axis[1])) > 0.002f)
            return fail("ribbon does not face the eye");
        for (i = 0; i < SH4DER_BLOOM_VERTS; i++)
            if (!(m.level[i] >= 0.0f && m.level[i] <= 4.0f))
                return fail("level range");
        if (m.level[0] != 0.0f || m.level[4] != 0.0f || m.level[10] != 0.0f || m.level[14] != 0.0f || !(m.level[7] > m.level[2]))
            return fail("feathered edges, more at the top");
        for (i = 0; i < SH4DER_BLOOM_COLS; i++)   /* the root row is dark: the ribbon starts at the blade's base without an edge */
            if (m.level[i] != 0.0f)
                return fail("ribbon root edge");
    }
    sh4der_bloom_argb(&m, (const float[3]){ 0.44f, 0.82f, 1.0f }, (const float[3]){ 0.91f, 0.97f, 1.0f }, 0xff, argb, sizeof argb[0]);
    if (argb[16] != 0xff000000u || (argb[15] & 0xffu) == 0)
        return fail("argb");
    return 0;
}

/* The tip disc is a lens flare: full head-on (tick 0: 0.495, tick 48: 0.90), 0.084 / 0.152 at 45 degrees, the floor side-on. */
static int flare(void) {
    static const float head_on[3] = { 2.0f, 0.0f, 0.008f }, at45[3] = { 1.0f, -1.0f, 0.008f };
    sh4der_blade_params_t bp;
    sh4der_blade_frame_t f;
    sh4der_bloom_params_t p;
    sh4der_bloom_mesh_t m;

    sh4der_blade_params_default(&bp);
    sh4der_bloom_params_default(&p);
    sh4der_blade_prepare(&f, &bp, 0, 1.0f);
    sh4der_bloom_blade(&m, &p, &bp, &f, head_on);
    if (!near(m.level[15], 0.49500f, 0.0001f + TOL))
        return fail("flare head-on, tick 0");
    sh4der_bloom_blade(&m, &p, &bp, &f, at45);
    if (!near(m.level[15], 0.08386f, 0.0001f + TOL))
        return fail("flare at 45 degrees");
    sh4der_blade_prepare(&f, &bp, 48, 1.0f);
    sh4der_bloom_blade(&m, &p, &bp, &f, head_on);
    if (!near(m.level[15], 0.90000f, 0.0001f + TOL))
        return fail("flare head-on, tick 48");
    sh4der_bloom_blade(&m, &p, &bp, &f, at45);
    if (!near(m.level[15], 0.15247f, 0.0001f + TOL))
        return fail("flare at 45 degrees, tick 48");
    return 0;
}

/* sh4der_bloom_disc_n: a round disc of 16 facing the eye, every rim vertex at the radius and across the view, 0 at the rim; the rim count clamped. */
static int round_disc(void) {
    static const float centre[3] = { 0.1f, 0.2f, 0.3f }, eye[3] = { 1.0f, -2.0f, 0.5f };
    float xyz[1 + SH4DER_BLOOM_DISC_MAX][3], level[1 + SH4DER_BLOOM_DISC_MAX], v[3];
    unsigned k;
    int a;

    for (a = 0; a < 3; a++)
        v[a] = eye[a] - centre[a];
    sh4der_bloom_disc_n(xyz, level, centre, 0.08f, eye, 0.6f, 16);
    if (level[0] != 0.6f)
        return fail("disc_n centre");
    for (k = 1; k <= 16; k++) {
        float r[3], len, dot;

        for (a = 0; a < 3; a++)
            r[a] = xyz[k][a] - centre[a];
        len = sqrtf(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
        dot = (r[0] * v[0] + r[1] * v[1] + r[2] * v[2]) / (len * sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]));
        if (!near(len, 0.08f, 0.0005f) || fabsf(dot) > 0.002f || level[k] != 0.0f)
            return fail("disc_n round and facing");
    }
    for (a = 0; a < 3; a++)   /* opposite rim vertices sum to twice the centre (round, centred) */
        if (!near(xyz[1][a] + xyz[9][a], 2.0f * centre[a], 0.0005f))
            return fail("disc_n centred");
    level[SH4DER_BLOOM_DISC_MAX] = -1.0f;
    sh4der_bloom_disc_n(xyz, level, centre, 0.08f, eye, 0.6f, 99);
    if (level[SH4DER_BLOOM_DISC_MAX] != 0.0f)
        return fail("disc_n clamps the rim to the maximum");
    return 0;
}

/* sh4der_bloom_disc_feather: feather 1 is soft from the centre (the inner ring on the centre), 0 a hard edge (the inner ring on the rim), 0.5 halfway;
   the outer ring at 0, the inner at the peak; 3 x rim triangles using every vertex. */
static int feathered_disc(void) {
    static const float centre[3] = { 0.0f, 0.0f, 0.0f }, eye[3] = { 0.0f, -3.0f, 0.0f };
    float xyz[1 + 2 * SH4DER_BLOOM_DISC_MAX][3], level[1 + 2 * SH4DER_BLOOM_DISC_MAX];
    uint8_t tris[3 * SH4DER_BLOOM_DISC_MAX][3];
    int used[1 + 2 * 16] = { 0 };
    unsigned k, n;
    static const float share[3] = { 1.0f, 0.0f, 0.5f }, want_inner[3] = { 0.0f, 0.2f, 0.1f };
    int i;

    for (i = 0; i < 3; i++) {
        sh4der_bloom_disc_feather(xyz, level, centre, 0.2f, share[i], eye, 0.7f, 16);
        for (k = 1; k <= 16; k++) {
            const float ri = sqrtf(xyz[k][0] * xyz[k][0] + xyz[k][1] * xyz[k][1] + xyz[k][2] * xyz[k][2]);
            const float ro = sqrtf(xyz[16 + k][0] * xyz[16 + k][0] + xyz[16 + k][1] * xyz[16 + k][1] + xyz[16 + k][2] * xyz[16 + k][2]);

            if (!near(ri, want_inner[i], 0.0005f) || !near(ro, 0.2f, 0.0005f) || level[k] != 0.7f || level[16 + k] != 0.0f)
                return fail("feathered disc rings");
        }
    }
    n = sh4der_bloom_disc_feather_tris(16, tris);
    if (n != 48)
        return fail("feathered disc triangle count");
    for (k = 0; k < n; k++)
        for (i = 0; i < 3; i++) {
            if (tris[k][i] > 32)
                return fail("feathered disc index");
            used[tris[k][i]] = 1;
        }
    for (k = 0; k <= 32; k++)
        if (!used[k])
            return fail("feathered disc: a vertex unused");
    return 0;
}

static int bad_params(void) {
    sh4der_blade_params_t bp;
    sh4der_blade_frame_t f;
    sh4der_bloom_params_t p;
    sh4der_bloom_mesh_t m;
    int k, i;

    sh4der_blade_params_default(&bp);
    sh4der_blade_prepare(&f, &bp, 10, 1.0f);
    for (k = 0; k < 6; k++) {
        sh4der_bloom_params_default(&p);
        switch (k) {
        case 0: p.x1 = p.x0; break;
        case 1: p.half_width = 0.0f; break;
        case 2: p.inner = p.half_width; break;
        case 3: p.tube_floor = 1.5f; break;
        case 4: p.disc_peak = NAN; break;
        case 5: p.tip = 9.0f; break;
        }
        if (sh4der_bloom_params_valid(&p) || sh4der_bloom_blade(&m, &p, &bp, &f, eye))
            return fail("bad bloom params accepted");
        for (i = 0; i < SH4DER_BLOOM_VERTS; i++)
            if (m.level[i] != 0.0f)
                return fail("bad params mesh not dark");
    }
    sh4der_bloom_params_default(&p);
    sh4der_blade_params_default(&bp);
    bp.blocker[0].depth = -1.5f;
    if (sh4der_blade_params_valid(&bp))
        return fail("blocker depth past -1");
    return 0;
}

int main(void) {
    if (shipped() || ramp() || white_structure() || geometry() || flare() || round_disc() || feathered_disc() || bad_params())
        return 1;
    printf("bloom_test: ok\n");
    return 0;
}
