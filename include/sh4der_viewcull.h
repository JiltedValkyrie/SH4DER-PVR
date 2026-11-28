/* SH4DER-PVR: view cull of boxes, and of a grid of points a block at a time, before they are placed on the screen

   sh4der_viewcull.h
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).

   RECOMMENDED (from the wiki ledger; wiki/limits/header-recommended.md)
   Start with: SH4DER_VIEWCULL_BLOCK = 4 (4 x 4 cells a block of a grid); prepare once a frame with the camera's
               world-to-clip rows, the near plane and the draw distance (0 for none)
   Cost:       not measured. ESTIMATE: 12 FIPR a box (two a plane, six planes), and a pass over the grid's points
               for the blocks' boxes; a block out of view saves its points' transforms and its cells' tests
   Status:     ADVISED; to prove: the walled square's turning stages with PSM_PLACE_GROUND on and off (the view cost
               test, vc-g's script with opt 128), p0 and the frame, on a console
   Do:         cull before placing: a box out of view costs 12 FIPR, its points cost a transform each
   Do not:     read a placed value (clip, screen) of a point in a block the cull left out: it was not written
   Maturity:   PROTOTYPE v0.1, host tested (test/viewcull_test.c); NOT RUN on a console

   Two jobs. sh4der_viewcull_box: is an axis-aligned world box wholly outside the view (behind the near plane, past one
   side of the screen, or wholly past a draw distance)? sh4der_viewcull_grid: the same for a rows x cols grid of world
   points (a terrain, a ground window, a heightfield), a block of SH4DER_VIEWCULL_BLOCK x SH4DER_VIEWCULL_BLOCK cells at a
   time, the box of each block made from its own points. A block it leaves out has no cell that could be on the screen,
   so a caller may skip its points' transforms and its cells' tests.

   The test. Each side of the view is a plane in world space taken from the clip rows: X + W >= 0 (left), W - X >= 0
   (right), Y + W >= 0 (bottom), W - Y >= 0 (top), W - near > 0 (the near plane), far - W >= 0 (the draw distance). A box is
   outside a plane when its corner farthest along the plane's normal is outside; that corner's value is a . c + |a| . e + d
   for the box's centre c and half size e. Two dot products a plane (FIPR on the SH-4), no corner transforms. It answers
   as eight corner transforms would, with a margin of 1e-5 of the terms' size kept on the inside: a box within rounding of
   a side is kept. A value that is not finite keeps the box.

   The rows: m is the world-to-clip matrix in column order, m[4 c + r]: clip X = m[0] x + m[4] y + m[8] z + m[12], Y the
   row r = 1, W (the view depth) r = 3; clip Z is not read. That is the form sh4der_psm_cam_rows writes.

   The maths is SH4ZAM's shz_dot8f (FIPR) and shz_fabsf; define SH4DER_CORE_NO_SH4ZAM for the plain C path. It does not
   load or read XMTRX, so a caller's matrix stays loaded. No allocation, no globals: the caller owns the seen bytes.

   Version: v0.1
   The effect: nothing on screen; less SH-4 work when the camera moves.
   Developed for: the walled square's ground window in the shadow demo (demo/psm/psm_scene.h, PSM_PLACE_GROUND), where
                  a turning camera placed every point of the window each frame (the turn plan, cause 1).
   Use it for: a ground grid, a terrain patch, a heightfield, any list of objects with a box, culled before placement.

   \author Jilted Valkyrie
*/

#ifndef SH4DER_VIEWCULL_H
#define SH4DER_VIEWCULL_H

#include <stdint.h>
#include "sh4der_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Default tuning
    @{ */
#ifndef SH4DER_VIEWCULL_BLOCK
#define SH4DER_VIEWCULL_BLOCK 4u     /**< \brief Cells a side of a grid's block (4 x 4 cells, 5 x 5 points). */
#endif
/** @} */

enum {
    SH4DER_VIEWCULL_PLANES = 6,      /**< \brief Left, right, bottom, top, near, far. */
    SH4DER_VIEWCULL_NEAR = 4         /**< \brief The near plane's index: outside at 0 (W must be over near), the rest at under 0. */
};

/** \brief A frame's view as planes, made by sh4der_viewcull_prepare. */
typedef struct sh4der_viewcull {
    float pl[SH4DER_VIEWCULL_PLANES][4];   /**< \brief a (3) and d of each plane: a . p + d, inside at 0 and up. */
    float ab[SH4DER_VIEWCULL_PLANES][4];   /**< \brief |a| of each, the fourth 0. */
    unsigned n;                            /**< \brief 5, or 6 with a draw distance. 0: no view (everything kept). */
} sh4der_viewcull_t;

/* a . b over four floats: FIPR on the SH-4 */
static inline float sh4der_viewcull_dot4_(const float a[4], float x, float y, float z, float w) {
#ifndef SH4DER_CORE_NO_SH4ZAM
    return shz_dot8f(a[0], a[1], a[2], a[3], x, y, z, w);
#else
    return a[0] * x + a[1] * y + a[2] * z + a[3] * w;
#endif
}

/** \brief The view's planes from the world-to-clip rows m (column order, see the top), the near plane znear (> 0) and a
    draw distance zfar (0: none). Returns 0 and keeps everything (n = 0) for a missing or not finite m. */
static inline int sh4der_viewcull_prepare(sh4der_viewcull_t *vc, const float m[16], float znear, float zfar) {
    unsigned i, k;

    vc->n = 0u;
    if (!m) return 0;
    for (i = 0; i < 16u; i++) if (!sh4der_finite_(m[i])) return 0;
    for (k = 0; k < 4u; k++) {                         /* k: x, y, z, the constant */
        const float x = m[4u * k], y = m[4u * k + 1u], w = m[4u * k + 3u];

        vc->pl[0][k] = w + x; vc->pl[1][k] = w - x;
        vc->pl[2][k] = w + y; vc->pl[3][k] = w - y;
        vc->pl[4][k] = w;     vc->pl[5][k] = -w;
    }
    vc->pl[4][3] -= znear;
    vc->pl[5][3] += zfar;
    for (i = 0; i < SH4DER_VIEWCULL_PLANES; i++) {
        for (k = 0; k < 3u; k++) vc->ab[i][k] = sh4der_abs_(vc->pl[i][k]);
        vc->ab[i][3] = 0.0f;
    }
    vc->n = zfar > 0.0f ? 6u : 5u;
    return 1;
}

/** \brief 0 when the box lo .. hi is wholly outside the view (behind the near plane, past one side, or past the draw
    distance); 1 when some of it may be in view. A view that was not made keeps every box. */
static inline int sh4der_viewcull_box(const sh4der_viewcull_t *vc, const float lo[3], const float hi[3]) {
    const float cx = 0.5f * (lo[0] + hi[0]), cy = 0.5f * (lo[1] + hi[1]), cz = 0.5f * (lo[2] + hi[2]);
    const float ex = 0.5f * (hi[0] - lo[0]), ey = 0.5f * (hi[1] - lo[1]), ez = 0.5f * (hi[2] - lo[2]);
    const float acx = sh4der_abs_(cx), acy = sh4der_abs_(cy), acz = sh4der_abs_(cz);
    unsigned i;

    for (i = 0; i < vc->n; i++) {
        const float mid = sh4der_viewcull_dot4_(vc->pl[i], cx, cy, cz, 1.0f);
        const float ext = sh4der_viewcull_dot4_(vc->ab[i], ex, ey, ez, 0.0f);
        const float tol = 1e-5f * (sh4der_viewcull_dot4_(vc->ab[i], acx, acy, acz, 0.0f) + ext + sh4der_abs_(vc->pl[i][3])) + 1e-12f;
        const float far_corner = mid + ext;

        if (i == SH4DER_VIEWCULL_NEAR ? far_corner <= -tol : far_corner < -tol) return 0;   /* not finite: neither, kept */
    }
    return 1;
}

/** \brief The box of n points p (float[3] each): lo and hi. n must be at least 1: with none, lo is left over hi (an
    empty box, not a box to test). */
static inline void sh4der_viewcull_fit(const float (*p)[3], unsigned n, float lo[3], float hi[3]) {
    unsigned i, c;

    for (c = 0; c < 3u; c++) { lo[c] = 1e30f; hi[c] = -1e30f; }
    for (i = 0; i < n; i++)
        for (c = 0; c < 3u; c++) {
            if (p[i][c] < lo[c]) lo[c] = p[i][c];
            if (p[i][c] > hi[c]) hi[c] = p[i][c];
        }
}

/** \brief Blocks of a rows x cols grid of points ((rows - 1) x (cols - 1) cells), `bs` cells a side: their count down
    (*nbr) and across (*nbc). Block (br, bc) holds the cells of rows br bs .. br bs + bs - 1 and columns bc bs .. bc bs +
    bs - 1, cut at the grid's edge. */
static inline unsigned sh4der_viewcull_grid_blocks(unsigned rows, unsigned cols, unsigned bs, unsigned *nbr, unsigned *nbc) {
    const unsigned r = rows > 1u && bs ? (rows - 2u) / bs + 1u : 0u, c = cols > 1u && bs ? (cols - 2u) / bs + 1u : 0u;

    if (nbr) *nbr = r;
    if (nbc) *nbc = c;
    return r * c;
}

/** \brief The block of cell (r, c) in sh4der_viewcull_grid's `seen` (nbc from sh4der_viewcull_grid_blocks). */
static inline unsigned sh4der_viewcull_grid_index(unsigned r, unsigned c, unsigned bs, unsigned nbc) { return (r / bs) * nbc + c / bs; }

/** \brief Each block of the grid `pts` (rows x cols points in row order) tested against the view: seen[block] 1 when some
    of it may be in view, 0 when none of it can be. A block's box is made from its own points (its cells' corners, the
    shared edges included), so it holds every triangle drawn between them. Returns the blocks seen. */
static inline unsigned sh4der_viewcull_grid(const sh4der_viewcull_t *vc, const float (*pts)[3], unsigned rows, unsigned cols, unsigned bs,
                                            uint8_t *seen) {
    unsigned nbr, nbc, br, bc, r, c, k, n = 0;

    sh4der_viewcull_grid_blocks(rows, cols, bs, &nbr, &nbc);
    for (br = 0; br < nbr; br++)
        for (bc = 0; bc < nbc; bc++) {
            const unsigned r0 = br * bs, c0 = bc * bs;
            const unsigned r1 = r0 + bs < rows - 1u ? r0 + bs : rows - 1u, c1 = c0 + bs < cols - 1u ? c0 + bs : cols - 1u;
            float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };

            for (r = r0; r <= r1; r++)
                for (c = c0; c <= c1; c++) {
                    const float *p = pts[r * cols + c];

                    for (k = 0; k < 3u; k++) {
                        if (p[k] < lo[k]) lo[k] = p[k];
                        if (p[k] > hi[k]) hi[k] = p[k];
                    }
                }
            seen[br * nbc + bc] = (uint8_t)sh4der_viewcull_box(vc, lo, hi);
            n += seen[br * nbc + bc];
        }
    return n;
}

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_VIEWCULL_H */
