/* SH4DOW-DC: directional sun visibility for a regularly spaced heightfield.
 * Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.
 *
 * RECOMMENDED (host tested by test/water_test.c, both maths paths; wiki/effects/sh4dow.md)
 * Cost:       O(width x height) a sweep, no GPU memory; the caller holds the height and shadow fields and a scratch of
 *             2 * max(width, height) floats. Time on a console: NOT MEASURED (ESTIMATE only).
 * Start with: the sun above the horizon (sun.y > 0), a feather a few cells wide, a bias of zero.
 * Do:         treat it as the shadow of a single-valued surface; read 0 as lit and 1 as blocked.
 * Do not:     use it for overhangs, off-grid occluders or the exact solar-disc penumbra: none of them are in it.
 * Maturity:   EXPERIMENTAL (its header and README say so).
 * Separate from limb columns; no shadow-map render pass or GPU texture.
 */
#ifndef SH4DOW_HEIGHTFIELD_H
#define SH4DOW_HEIGHTFIELD_H
#include "sh4dow_dc.h"

/* Grid axes X,Z; Y up. sun points FROM surface TO sun, need not be unit.
 * Heights and shadow are w*h floats. Scratch: 2*max(w,h) floats.
 * Arrays must be distinct, float-aligned, finite; dimensions >=2, cell>0,
 * sun.y>0, bias>=0, feather>0. Returns 0 on invalid arguments.
 * A dominant-axis sweep propagates the highest sunward obstruction, with
 * linear interpolation across the other axis. Outside grid: unobstructed.
 * shadow=0 lit, 1 blocked. Bias/feather are vertical world distances.
 * Finite-grid, heightfield approximation: no overhangs, off-grid occluders,
 * or physically exact solar-disc penumbra. Cost O(w*h), no per-point ray march.
 */
static inline int sh4dow_heightfield_sun(const float *height, unsigned w, unsigned h,
    float cell, const float sun[3], float bias, float feather,
    float *shadow, float *scratch) {
    unsigned major, minor, row, col, xmajor;
    float ax, az, dominant, shift, drop, inv_feather;
    int step;
    float *prev = scratch, *next;
    if (!height || !sun || !shadow || !scratch || w < 2 || h < 2 ||
        !(cell > 0) || !(sun[1] > 0) || !(bias >= 0) || !(feather > 0)) return 0;
#ifndef SH4DOW_DC_NO_SH4ZAM
    ax = shz_fabsf(sun[0]); az = shz_fabsf(sun[2]);
#else
    ax = fabsf(sun[0]); az = fabsf(sun[2]);
#endif
    xmajor = ax > az;
    dominant = xmajor ? ax : az;
    if (dominant < 1e-6f) {
        for (row = 0; row < w*h; ++row) shadow[row] = 0;
        return 1;
    }
    major = xmajor ? w : h; minor = xmajor ? h : w;
    step = (xmajor ? sun[0] : sun[2]) > 0 ? -1 : 1;
#ifndef SH4DOW_DC_NO_SH4ZAM
    shift = shz_divf(xmajor ? sun[2] : sun[0], dominant);
    drop = shz_divf(cell * sun[1], dominant);
    inv_feather = shz_invf(feather);
#else
    shift = (xmajor ? sun[2] : sun[0]) / dominant;
    drop = cell * sun[1] / dominant;
    inv_feather = 1.0f / feather;
#endif
    next = scratch + minor;
    for (col = 0; col < minor; ++col) prev[col] = -1e20f;
    for (row = 0; row < major; ++row) {
        unsigned a = step < 0 ? major-1-row : row;
        for (col = 0; col < minor; ++col) {
            unsigned idx = xmajor ? col*w+a : a*w+col;
            float q = (float)col + shift, roof = -1e20f;
            if (q >= 0 && q <= (float)(minor-1)) {
                unsigned lo = (unsigned)q, hi = lo+1 < minor ? lo+1 : lo;
#ifndef SH4DOW_DC_NO_SH4ZAM
                roof = shz_lerpf(prev[lo], prev[hi], q-(float)lo) - drop;
#else
                roof = prev[lo] + (prev[hi]-prev[lo])*(q-(float)lo) - drop;
#endif
            }
            shadow[idx] = sh4dow_smooth01((roof-height[idx]-bias)*inv_feather); /* hand: one product, reciprocal hoisted */
            next[col] = roof > height[idx] ? roof : height[idx];
        }
        { float *swap = prev; prev = next; next = swap; }
    }
    return 1;
}
#endif
