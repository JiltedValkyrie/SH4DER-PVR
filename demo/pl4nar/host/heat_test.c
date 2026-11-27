/* Host test of the heat shimmer's maths (src/heat.h, version 2): the gate, the anchor, the 1 : 1 window and copy, the
   drift (small, smooth, slow), the mesh and its fade, the slip of a turn, and which frame's picture the copy holds.
   Plain C:
   gcc -std=c11 -O2 -Wall -Wextra -Werror -DSH4DER_CORE_NO_SH4ZAM -Iinclude demo/pl4nar/host/heat_test.c -lm */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../src/heat.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

#define SW 640u
#define SH 480u
static uint16_t fb[SW * SH], tex[HEAT_TW * HEAT_TH];
static heat_vtx_t m[HEAT_VERTS], m2[HEAT_VERTS];

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    const float none[2] = { 0.0f, 0.0f };
    heat_win_t w;
    unsigned i, j, n;

    /* the gate: day and dry only */
    CHECK(heat_gate(0.0f, 0u) == 1.0f && heat_gate(0.0f, 1u) == 0.0f && heat_gate(1.0f, 0u) == 0.0f && heat_gate(HEAT_NIGHT_MAX, 0u) == 0.0f);

    {   /* the anchor: HEAT_DIST ahead on the ground: the road's far end, not near */
        const float eye[3] = { 3.0f, 2.0f, -4.0f }, at[3] = { 6.0f, 1.0f, 0.0f }, down[3] = { 3.0f, 0.0f, -4.0f };
        float p[3];
        CHECK(heat_anchor(eye, at, 0.25f, p) && fabsf(hypotf(p[0] - eye[0], p[2] - eye[2]) - HEAT_DIST) < 1e-3f && p[1] == 0.25f);
        CHECK(!heat_anchor(eye, down, 0.0f, p));
        CHECK(HEAT_DIST >= 60.0f);
    }

    /* the window: whole pixels, a texel a pixel, on the screen */
    CHECK(heat_win_set(&w, 320.0f, 134.0f, SW, SH) && w.ok && w.x == 320 - (int)(HEAT_TW / 2u) && w.y == 134 - (int)HEAT_BAND_UP);
    {
        static const float a[][2] = { { -500.0f, -500.0f }, { 2000.0f, 2000.0f }, { 0.0f, 479.0f }, { 639.0f, 0.0f } };
        for (i = 0; i < 4u; i++) {
            CHECK(heat_win_set(&w, a[i][0], a[i][1], SW, SH));
            CHECK(w.x >= 0 && w.y >= 0 && w.x + (int)HEAT_TW <= (int)SW && w.y + (int)HEAT_TH <= (int)SH);
        }
    }
    CHECK(!heat_win_set(&w, 320.0f, 240.0f, 128u, 240u) && !w.ok && !heat_win_set(&w, (float)NAN, 240.0f, SW, SH) && !heat_win_set(&w, 1e9f, 0.0f, SW, SH));

    /* the copy: 1 : 1 */
    for (j = 0; j < SH; j++) for (i = 0; i < SW; i++) fb[j * SW + i] = (uint16_t)((j << 7) ^ i ^ (i << 11));
    CHECK(heat_win_set(&w, 300.0f, 260.0f, SW, SH));
    heat_grab(&w, fb, SW, tex);
    for (n = 0, j = 0; j < HEAT_TH; j++) for (i = 0; i < HEAT_TW; i++) n += tex[j * HEAT_TW + i] != fb[((unsigned)w.y + j) * SW + (unsigned)w.x + i];
    CHECK(n == 0u);

    /* the drift: small, and smooth: neighbours a cell apart differ by a small part of the amplitude; it moves slowly */
    {
        float d[2], e[2], worst = 0.0f, x, y, t;
        for (t = 0.0f; t < 4.0f; t += 0.07f)
            for (y = -24.0f; y <= 12.0f; y += 4.0f)
                for (x = -128.0f; x <= 128.0f; x += 4.0f) {
                    heat_drift(x, y, t, d);
                    CHECK(fabsf(d[0]) <= HEAT_AMP_X + 1e-4f && fabsf(d[1]) <= HEAT_AMP_Y + 1e-4f);
                    heat_drift(x + 8.0f, y, t, e);
                    if (fabsf(e[1] - d[1]) > worst) worst = fabsf(e[1] - d[1]);
                    heat_drift(x, y, t + 1.0f / 60.0f, e);
                    CHECK(fabsf(e[1] - d[1]) < 0.1f && fabsf(e[0] - d[0]) < 0.1f);          /* under a tenth of a pixel a frame */
                }
        CHECK(worst < HEAT_AMP_Y);                                                           /* a cell on: under the amplitude */
        CHECK(HEAT_AMP_Y <= 1.0f && HEAT_AMP_X <= 1.0f);
        printf("drift: at most %.2f x %.2f px; a cell apart differs by at most %.2f px\n", HEAT_AMP_X, HEAT_AMP_Y, worst);
    }

    /* the mesh, no slip: on the window's own pixels; a sample within the amplitude of its own texel; no share and no drift
       on the edge; the fade is smooth across HEAT_EDGE cells */
    CHECK(heat_mesh(&w, none, 1.25f, 1.0f, m) == HEAT_VERTS);
    {
        unsigned amax = 0u;
        for (j = 0; j <= HEAT_NY; j++) for (i = 0; i <= HEAT_NX; i++) {
            const heat_vtx_t *v = &m[j * (HEAT_NX + 1u) + i];
            const unsigned a = v->argb >> 24;
            const float px = v->u * (float)HEAT_TW + (float)w.x, py = v->v * (float)HEAT_TH + (float)w.y;
            CHECK(v->x == (float)w.x + (float)(i * HEAT_TW / HEAT_NX) && v->y == (float)w.y + (float)(j * HEAT_TH / HEAT_NY));
            CHECK(v->u > 0.0f && v->u < 1.0f && v->v > 0.0f && v->v < 1.0f);
            CHECK(fabsf(px - v->x) <= HEAT_AMP_X + 0.51f && fabsf(py - v->y) <= HEAT_AMP_Y + 0.51f);
            if (i == 0u || i == HEAT_NX || j == 0u || j == HEAT_NY) CHECK(a == 0u);
            if (i > 0u && j > 0u && j < HEAT_NY) CHECK(i > HEAT_NX / 2u || a >= (m[j * (HEAT_NX + 1u) + i - 1u].argb >> 24));   /* rising from the left */
            if (a > amax) amax = a;
        }
        CHECK(amax == (unsigned)(HEAT_ALPHA * 255.0f + 0.5f));
    }
    /* the same for the same time, another a little later */
    CHECK(heat_mesh(&w, none, 1.25f, 1.0f, m2) == HEAT_VERTS && memcmp(m, m2, sizeof m) == 0);
    CHECK(heat_mesh(&w, none, 1.75f, 1.0f, m2) == HEAT_VERTS && memcmp(m, m2, sizeof m) != 0);
    /* a slip: the band goes with the picture, the samples stay; the share falls; past HEAT_SLIP_MAX nothing */
    {
        const float s[2] = { 5.0f, -2.0f }, far_[2] = { HEAT_SLIP_MAX, 0.0f }, bad[2] = { 1.0e6f, 1.0e6f };
        CHECK(heat_mesh(&w, s, 1.25f, 1.0f, m2) == HEAT_VERTS);
        for (i = 0; i < HEAT_VERTS; i++) {
            CHECK(m2[i].x == m[i].x + 5.0f && m2[i].y == m[i].y - 2.0f && m2[i].u == m[i].u && m2[i].v == m[i].v);
            CHECK((m2[i].argb >> 24) <= (m[i].argb >> 24));
        }
        CHECK(heat_mesh(&w, far_, 1.25f, 1.0f, m2) == 0u && heat_mesh(&w, bad, 1.25f, 1.0f, m2) == 0u);
    }
    /* night or rain: nothing; a window that is not set: nothing */
    CHECK(heat_mesh(&w, none, 1.25f, heat_gate(1.0f, 0u), m2) == 0u && heat_mesh(&w, none, 1.25f, heat_gate(0.0f, 2u), m2) == 0u);
    w.ok = 0;
    CHECK(heat_mesh(&w, none, 1.25f, 1.0f, m2) == 0u);

    /* the slip of a turn: none for the same look; a small turn is focal x the angle, opposite ways for opposite turns */
    {
        const float f0[3] = { 0.0f, 0.0f, 1.0f }, up[3] = { 0.0f, 1.0f, 0.0f };
        float fl[3] = { 0.01f, 0.0f, 1.0f }, fr[3] = { -0.01f, 0.0f, 1.0f }, fu[3] = { 0.0f, 0.01f, 1.0f }, back[3] = { 0.0f, 0.0f, -1.0f }, s[2], t[2];
        sh4der_norm3_(fl); sh4der_norm3_(fr); sh4der_norm3_(fu);
        heat_slip(f0, f0, HEAT_FOCAL, s); CHECK(fabsf(s[0]) < 1e-4f && fabsf(s[1]) < 1e-4f);
        heat_slip(fl, f0, HEAT_FOCAL, s); heat_slip(fr, f0, HEAT_FOCAL, t);
        CHECK(fabsf(fabsf(s[0]) - HEAT_FOCAL * 0.01f) < 0.01f && fabsf(s[0] + t[0]) < 1e-3f && fabsf(s[1]) < 1e-3f);
        heat_slip(fu, f0, HEAT_FOCAL, s); CHECK(fabsf(s[0]) < 1e-3f && fabsf(s[1] + HEAT_FOCAL * 0.01f) < 0.01f);   /* what was higher is higher on the screen: y less */
        heat_slip(back, f0, HEAT_FOCAL, s); CHECK(s[0] >= 1.0e6f);
        heat_slip(f0, up, HEAT_FOCAL, s); CHECK(s[0] >= 1.0e6f);                                               /* looking straight up: no side */
    }

    /* which picture: with HEAT_LAG 2 the copy made in frame n reads the screen of frame n - 2, through the window and the
       look of that frame; a closed gate forgets everything */
    {
        heat_trk_t t;
        float look[6][3];
        unsigned k;

        heat_trk_reset(&t);
        for (k = 0; k < 6u; k++) {
            look[k][0] = 0.01f * (float)k; look[k][1] = 0.0f; look[k][2] = 1.0f; sh4der_norm3_(look[k]);
            if (heat_trk_copy(&t, 1.0f)) {
                CHECK(HEAT_LAG == 2u ? k >= 2u : k >= 1u);
                CHECK(memcmp(t.gf, look[k - HEAT_LAG], sizeof t.gf) == 0);                 /* the picture HEAT_LAG frames back */
                CHECK(t.got.y == (int)(100 + (int)(k - HEAT_LAG)) - (int)HEAT_BAND_UP);    /* and its window */
            } else CHECK(k < HEAT_LAG);
            heat_trk_draw(&t, look[k], 320.0f, 100.0f + (float)k, 1.0f, SW, SH);
        }
        CHECK(!heat_trk_copy(&t, 0.0f) && t.n == 0u && !t.next.ok);
        heat_trk_draw(&t, look[0], 320.0f, 100.0f, 0.0f, SW, SH);
        CHECK(t.n == 0u && !heat_trk_copy(&t, 1.0f));
    }

    printf("heat v2: %u x %u texels (%u KB), 1 : 1, %u vertices, %u strip vertices (%u B of TA with the header)\n", HEAT_TW, HEAT_TH,
           HEAT_TW * HEAT_TH * 2u / 1024u, HEAT_VERTS, HEAT_NY * 2u * (HEAT_NX + 1u), (HEAT_NY * 2u * (HEAT_NX + 1u) + 1u) * 32u);
    printf(fails ? "heat_test: %d FAILED\n" : "heat_test: ok\n", fails);
    return fails != 0;
}
