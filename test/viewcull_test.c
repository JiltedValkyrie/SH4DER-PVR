/* SH4DER-PVR: view cull test

   test/viewcull_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   Host check of include/sh4der_viewcull.h against the plain answer, eight corner transforms a box:
   - a box the cull leaves out has all eight corners outside one side (behind the near plane, past one side of the screen,
     or past the draw distance), over 20,000 random boxes and 200 cameras;
   - a box it keeps that the corners call outside is within the margin of a side (counted; a handful at most);
   - a grid's block it leaves out has no cell with a corner in view, and every cell with a corner on the screen is in a
     block it keeps (a ground window of rings and spokes, as the shadow demo's);
   - the block count and index sizing; a missing or not finite matrix keeps everything; no draw distance has 5 planes;
   - not finite box values are kept.
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "../include/sh4der_viewcull.h"
#include "../include/sh4der_psm.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static unsigned rng = 12345u;
static float rnd(float lo, float hi) {
    rng = rng * 1664525u + 1013904223u;
    return lo + (hi - lo) * (float)(rng >> 8) * (1.0f / 16777216.0f);
}

/* X, Y, W of p through m (column order), in double: the plain answer */
static void clip(const float m[16], const float p[3], double h[3]) {
    static const unsigned row[3] = { 0u, 1u, 3u };
    unsigned i;

    for (i = 0; i < 3u; i++)
        h[i] = (double)m[row[i]] * p[0] + (double)m[4u + row[i]] * p[1] + (double)m[8u + row[i]] * p[2] + (double)m[12u + row[i]];
}

/* 1: every corner outside one side (the shadow demo's psm_block_seen_ form); *edge: the nearest side value in units of the terms */
static int corners_out(const float m[16], const float lo[3], const float hi[3], float zn, float zf) {
    unsigned c, behind = 1u, left = 1u, right = 1u, below = 1u, above = 1u, far = zf > 0.0f;

    for (c = 0; c < 8u; c++) {
        const float p[3] = { (c & 1u) ? hi[0] : lo[0], (c & 2u) ? hi[1] : lo[1], (c & 4u) ? hi[2] : lo[2] };
        double h[3];

        clip(m, p, h);
        behind &= !(h[2] > zn);
        left &= h[0] < -h[2]; right &= h[0] > h[2];
        below &= h[1] < -h[2]; above &= h[1] > h[2];
        far &= h[2] > zf;
    }
    return behind || left || right || below || above || far;
}

static int cam(float m[16], float yaw, float pitch, const float eye[3]) {
    sh4der_psm_cam_t c;
    const float sy = sinf(yaw), cy = cosf(yaw), sp = sinf(pitch), cp = cosf(pitch);

    c.eye[0] = eye[0]; c.eye[1] = eye[1]; c.eye[2] = eye[2];
    c.fwd[0] = sy * cp; c.fwd[1] = -sp; c.fwd[2] = -cy * cp;
    c.up[0] = sy * sp; c.up[1] = cp; c.up[2] = -cy * sp;
    c.tan_half = 0.6f; c.aspect = 4.0f / 3.0f; c.znear = 0.1f; c.zfar = 300.0f;
    return sh4der_psm_cam_rows(&c, 0.1f, m);
}

static void boxes(void) {
    unsigned k, b, culled = 0, kept_out = 0, total = 0;

    for (k = 0; k < 200u; k++) {
        const float eye[3] = { rnd(-20.0f, 20.0f), rnd(0.5f, 4.0f), rnd(-20.0f, 20.0f) };
        const float zf = (k & 1u) ? 0.0f : rnd(10.0f, 80.0f);
        float m[16];
        sh4der_viewcull_t vc;

        if (!cam(m, rnd(-3.2f, 3.2f), rnd(-1.3f, 1.3f), eye)) continue;
        CHECK(sh4der_viewcull_prepare(&vc, m, 0.1f, zf) == 1);
        CHECK(vc.n == (zf > 0.0f ? 6u : 5u));
        for (b = 0; b < 100u; b++) {
            float lo[3], hi[3];
            unsigned i;
            int out, keep;

            for (i = 0; i < 3u; i++) {
                const float c = eye[i] + rnd(-60.0f, 60.0f), e = rnd(0.0f, 6.0f);

                lo[i] = c - e; hi[i] = c + e;
            }
            if (b % 10u == 0u) lo[1] = hi[1] = 0.0f;              /* flat boxes, as a ground block's */
            out = corners_out(m, lo, hi, 0.1f, zf);
            keep = sh4der_viewcull_box(&vc, lo, hi);
            total++;
            if (!keep) { culled++; CHECK(out); }                  /* never leaves out a box the corners keep */
            if (keep && out) kept_out++;
        }
    }
    printf("boxes: %u tested, %u left out, %u kept that the corners would leave out (the margin)\n", total, culled, kept_out);
    CHECK(culled > total / 10u);                                   /* it does leave boxes out */
    CHECK(kept_out * 200u < total);                               /* the margin keeps few */
}

/* a ground window of rings and spokes round the eye, as the shadow demo builds it */
#define ROWS 40u
#define COLS 33u
static float pts[ROWS * COLS][3];
static uint8_t seen[((ROWS + 3u) / 4u) * ((COLS + 3u) / 4u)];

static void grid(void) {
    unsigned k, r, c, nbr, nbc, culled = 0, blocks = 0, cells_on = 0;

    CHECK(sh4der_viewcull_grid_blocks(ROWS, COLS, 4u, &nbr, &nbc) == nbr * nbc && nbr == 10u && nbc == 8u);
    CHECK(sh4der_viewcull_grid_blocks(5u, 5u, 4u, &nbr, &nbc) == 1u);
    CHECK(sh4der_viewcull_grid_blocks(6u, 5u, 4u, &nbr, &nbc) == 2u && nbr == 2u && nbc == 1u);
    CHECK(sh4der_viewcull_grid_blocks(1u, 5u, 4u, NULL, NULL) == 0u);
    for (k = 0; k < 60u; k++) {
        const float eye[3] = { rnd(-5.0f, 5.0f), 1.6f, rnd(-5.0f, 5.0f) }, yaw = rnd(-3.2f, 3.2f), pitch = rnd(-1.3f, 0.6f);
        const float zf = (k % 3u == 0u) ? 30.0f : 0.0f;
        float m[16];
        sh4der_viewcull_t vc;

        if (!cam(m, yaw, pitch, eye)) continue;
        for (r = 0; r < ROWS; r++)
            for (c = 0; c < COLS; c++) {
                const float rad = 0.5f + 0.15f * (float)(r * r), a = yaw + ((float)c - 16.0f) * 0.06f;

                pts[r * COLS + c][0] = eye[0] + rad * sinf(a); pts[r * COLS + c][1] = 0.0f; pts[r * COLS + c][2] = eye[2] - rad * cosf(a);
            }
        sh4der_viewcull_prepare(&vc, m, 0.1f, zf);
        blocks += sh4der_viewcull_grid_blocks(ROWS, COLS, 4u, &nbr, &nbc);
        culled += nbr * nbc - sh4der_viewcull_grid(&vc, (const float (*)[3])pts, ROWS, COLS, 4u, seen);
        for (r = 0; r + 1u < ROWS; r++)
            for (c = 0; c + 1u < COLS; c++) {
                const unsigned q[4] = { r * COLS + c, r * COLS + c + 1u, (r + 1u) * COLS + c, (r + 1u) * COLS + c + 1u };
                float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
                unsigned i, j;

                for (i = 0; i < 4u; i++)
                    for (j = 0; j < 3u; j++) {
                        if (pts[q[i]][j] < lo[j]) lo[j] = pts[q[i]][j];
                        if (pts[q[i]][j] > hi[j]) hi[j] = pts[q[i]][j];
                    }
                if (!corners_out(m, lo, hi, 0.1f, zf)) {           /* the cell may be on the screen: its block must be kept */
                    cells_on++;
                    CHECK(seen[sh4der_viewcull_grid_index(r, c, 4u, nbc)]);
                }
            }
    }
    printf("grid: %u blocks over 60 views, %u left out; %u cells the corners keep, all in kept blocks\n", blocks, culled, cells_on);
    CHECK(culled > 0u && culled < blocks);
}

static void edges(void) {
    sh4der_viewcull_t vc;
    float m[16], lo[3] = { -1.0f, -1.0f, -1.0f }, hi[3] = { 1.0f, 1.0f, 1.0f };
    const float eye[3] = { 0.0f, 1.6f, 0.0f };
    unsigned i;

    CHECK(sh4der_viewcull_prepare(&vc, NULL, 0.1f, 0.0f) == 0 && vc.n == 0u && sh4der_viewcull_box(&vc, lo, hi) == 1);
    CHECK(cam(m, 0.0f, 0.0f, eye));
    m[5] = NAN;
    CHECK(sh4der_viewcull_prepare(&vc, m, 0.1f, 0.0f) == 0 && vc.n == 0u);
    CHECK(cam(m, 0.0f, 0.0f, eye));
    CHECK(sh4der_viewcull_prepare(&vc, m, 0.1f, 0.0f) == 1);
    lo[2] = 5.0f; hi[2] = 7.0f;                                  /* behind the eye (it looks down -z) */
    CHECK(sh4der_viewcull_box(&vc, lo, hi) == 0);
    lo[2] = -7.0f; hi[2] = -5.0f;                                /* straight ahead */
    CHECK(sh4der_viewcull_box(&vc, lo, hi) == 1);
    lo[0] = NAN;                                                 /* not finite: kept */
    CHECK(sh4der_viewcull_box(&vc, lo, hi) == 1);
    lo[0] = -1.0f; lo[2] = -70.0f; hi[2] = -60.0f;               /* far ahead: kept without a draw distance, left out with 30 m */
    CHECK(sh4der_viewcull_box(&vc, lo, hi) == 1);
    CHECK(sh4der_viewcull_prepare(&vc, m, 0.1f, 30.0f) == 1 && vc.n == 6u && sh4der_viewcull_box(&vc, lo, hi) == 0);
    for (i = 0; i < 6u; i++) CHECK(vc.ab[i][3] == 0.0f && vc.ab[i][0] >= 0.0f && vc.ab[i][1] >= 0.0f && vc.ab[i][2] >= 0.0f);
    {
        float p[2][3] = { { 1.0f, -2.0f, 3.0f }, { -4.0f, 5.0f, 0.5f } };

        sh4der_viewcull_fit((const float (*)[3])p, 2u, lo, hi);
        CHECK(lo[0] == -4.0f && lo[1] == -2.0f && lo[2] == 0.5f && hi[0] == 1.0f && hi[1] == 5.0f && hi[2] == 3.0f);
    }
}

int main(void) {
    boxes();
    grid();
    edges();
    printf(fails ? "viewcull_test: %d FAILED\n" : "viewcull_test: ok\n", fails);
    return fails != 0;
}
