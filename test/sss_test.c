/* Back-lit transmission: bounds, path monotony, blocked light, back-light geometry, wrap, distortion. */
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include "sh4der_sss.h"
#ifndef SH4DER_TEST_TOL
#define SH4DER_TEST_TOL 0.0001f
#endif
static int closef(float a, float b, float tol) { return fabsf(a - b) <= tol; }
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    const float tint[3] = {1.0f, 0.58f, 0.30f}, absorb[3] = {0.50f, 0.13f, 0.07f};
    const float view[3] = {0, 0, 1}, behind[3] = {0, 0, -1}, front[3] = {0, 0, 1}, side[3] = {1, 0, 0}, n[3] = {0, 1, 0};
    const float tol = 4 * SH4DER_TEST_TOL;
    const float exp_rel = 0.00002f; /* sh4der_sss_exp_ is a series, relative error under 3e-7 (shz_expf read 3 % low near 0) */
    sh4der_sss_t p, w, d;
    float out[3], prev[3] = {2, 2, 2};
    unsigned i, c;

    /* the exponential: exactly 1 at 0, the series against the C library over -20 .. 10, the clamps */
    assert(sh4der_sss_exp_(0.0f) == 1.0f);
    assert(closef(sh4der_sss_exp_(-0.012f), expf(-0.012f), 1e-6f));
    for (i = 0; i <= 600; ++i) { const float x = -20.0f + 0.05f * (float)i, e = expf(x); assert(fabsf(sh4der_sss_exp_(x) - e) <= 2e-6f * e + 1e-30f); }
    assert(sh4der_sss_exp_(-260.0f) < 1e-30f && sh4der_sss_exp_(-1.0e9f) < 1e-30f && sh4der_sss_exp_(200.0f) > 1e37f && sh4der_sss_exp_(200.0f) < 3.4e38f);
    sh4der_sss_init(&p, tint, absorb, 0.25f, 0.0f, 0.0f);
    /* back-light geometry: full straight behind, none side-on or front-lit */
    assert(closef(sh4der_sss_back(&p, view, behind, n), 1, tol));
    assert(sh4der_sss_back(&p, view, side, n) == 0);
    assert(sh4der_sss_back(&p, view, front, n) == 0);
    /* blocked light (visibility 0), no coverage, no entry: exactly zero */
    sh4der_sss_rgb(&p, view, behind, n, 1, 1, 1, 0, out); for (c = 0; c < 3; ++c) assert(out[c] == 0);
    sh4der_sss_rgb(&p, view, behind, n, 1, 0, 1, 1, out); for (c = 0; c < 3; ++c) assert(out[c] == 0);
    sh4der_sss_rgb(&p, view, behind, n, 1, 1, 0, 1, out); for (c = 0; c < 3; ++c) assert(out[c] == 0);
    sh4der_sss_rgb(&p, view, front, n, 1, 1, 1, 1, out); for (c = 0; c < 3; ++c) assert(out[c] == 0);
    /* path 0: tint x gain; negative path counts as 0 */
    sh4der_sss_rgb(&p, view, behind, n, 0, 1, 1, 1, out);
    for (c = 0; c < 3; ++c) assert(closef(out[c], tint[c] * 0.25f, tol + exp_rel * tint[c] * 0.25f));
    sh4der_sss_rgb(&p, view, behind, n, -3, 1, 1, 1, out);
    for (c = 0; c < 3; ++c) assert(closef(out[c], tint[c] * 0.25f, tol + exp_rel * tint[c] * 0.25f));
    /* bounds and monotone fall with path, every channel */
    for (i = 0; i <= 40; ++i) {
        sh4der_sss_rgb(&p, view, behind, n, i * 0.25f, 1, 1, 1, out);
        for (c = 0; c < 3; ++c) {
            assert(out[c] >= 0 && out[c] <= tint[c] * 0.25f + tol);
            assert(out[c] <= prev[c] + tol);
            assert(closef(out[c], tint[c] * expf(-absorb[c] * i * 0.25f) * 0.25f, tol + exp_rel * tint[c] * 0.25f));
            prev[c] = out[c];
        }
    }
    /* deep path shifts toward the low-absorption channels */
    sh4der_sss_absorb(tint, absorb, 6, out); assert(out[0] / tint[0] < out[1] / tint[1] && out[1] / tint[1] < out[2] / tint[2]);
    /* weights are linear */
    { float half[3]; sh4der_sss_rgb(&p, view, behind, n, 1, 1, 1, 1, out); sh4der_sss_rgb(&p, view, behind, n, 1, 0.5f, 1, 1, half);
      for (c = 0; c < 3; ++c) assert(closef(half[c], out[c] * 0.5f, tol)); }
    /* wrap lights side-on, still 0..1, still 0 front-lit */
    sh4der_sss_init(&w, tint, absorb, 1, 1, 0);
    assert(closef(sh4der_sss_back(&w, view, side, n), 0.5f, tol));
    assert(closef(sh4der_sss_back(&w, view, behind, n), 1, tol));
    assert(sh4der_sss_back(&w, view, front, n) <= tol);
    /* distortion 0 ignores the normal; distortion bends toward it */
    { const float n2[3] = {0, 0, 1}; assert(sh4der_sss_back(&p, view, behind, n) == sh4der_sss_back(&p, view, behind, n2)); }
    sh4der_sss_init(&d, tint, absorb, 1, 0, 0.5f);
    { const float n_back[3] = {0, 0, -1}; float b = sh4der_sss_back(&d, view, side, n_back); assert(closef(b, 0.5f, tol)); }
    printf("sss_test OK\n");
    return 0;
}
