/* Host test of the heat shimmer's maths (src/heat.h): the gate, the anchor, the window, the copy and the mesh. Plain C:
   gcc -std=c11 -O2 -Wall -Wextra -Werror -DSH4DER_CORE_NO_SH4ZAM -Iinclude -Ish4dow-dc/include demo/pl4nar/host/heat_test.c -lm */

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

int main(void) {
    heat_win_t w;
    unsigned i, j, n;

    /* the gate: day and dry only */
    CHECK(heat_gate(0.0f, 0u) == 1.0f && heat_gate(0.0f, 1u) == 0.0f && heat_gate(0.0f, 2u) == 0.0f);
    CHECK(heat_gate(HEAT_NIGHT_MAX, 0u) == 0.0f && heat_gate(1.0f, 0u) == 0.0f && heat_gate(0.5f, 0u) == 0.0f);
    CHECK(heat_gate(HEAT_NIGHT_MAX * 0.5f, 0u) > 0.49f && heat_gate(HEAT_NIGHT_MAX * 0.5f, 0u) < 0.51f);

    {   /* the anchor: HEAT_DIST ahead on the ground, whatever the eye's height; none straight down */
        const float eye[3] = { 3.0f, 2.0f, -4.0f }, at[3] = { 6.0f, 1.0f, 0.0f }, down[3] = { 3.0f, 0.0f, -4.0f };
        float p[3];
        CHECK(heat_anchor(eye, at, 0.25f, p));
        CHECK(fabsf(hypotf(p[0] - eye[0], p[2] - eye[2]) - HEAT_DIST) < 1e-3f && p[1] == 0.25f);
        CHECK(fabsf((p[0] - eye[0]) * 4.0f - (p[2] - eye[2]) * 3.0f) < 1e-3f && p[0] > eye[0]);
        CHECK(!heat_anchor(eye, down, 0.0f, p));
    }

    /* the window: holds the band and the samples' wander, stays on the screen, columns and rows in order */
    CHECK(HEAT_WIN_W >= (unsigned)(HEAT_BAND_W + 2.0f * HEAT_AMP_X) && HEAT_WIN_H >= (unsigned)(HEAT_BAND_UP + HEAT_BAND_DOWN + 2.0f * HEAT_AMP_Y));
    CHECK(heat_win_set(&w, 320.0f, 250.0f, SW, SH) && w.ok);
    CHECK(w.x == 320 - (int)(HEAT_WIN_W / 2u) && w.y <= (int)(250.0f - HEAT_BAND_UP) && w.y + (int)HEAT_WIN_H >= (int)(250.0f + HEAT_BAND_DOWN));
    for (i = 1; i < HEAT_TW; i++) CHECK(w.xs[i] > w.xs[i - 1u]);
    for (i = 1; i < HEAT_TH; i++) CHECK(w.ys[i] > w.ys[i - 1u]);
    CHECK(w.xs[0] >= (unsigned)w.x && w.xs[HEAT_TW - 1u] < (unsigned)w.x + HEAT_WIN_W && w.ys[0] >= (unsigned)w.y && w.ys[HEAT_TH - 1u] < (unsigned)w.y + HEAT_WIN_H);
    {
        static const float a[][2] = { { -500.0f, -500.0f }, { 2000.0f, 2000.0f }, { 0.0f, 479.0f }, { 639.0f, 0.0f } };
        for (i = 0; i < 4u; i++) {
            CHECK(heat_win_set(&w, a[i][0], a[i][1], SW, SH));
            CHECK(w.x >= 0 && w.y >= 0 && w.x + (int)HEAT_WIN_W <= (int)SW && w.y + (int)HEAT_WIN_H <= (int)SH);
            CHECK(w.xs[HEAT_TW - 1u] < SW && w.ys[HEAT_TH - 1u] < SH);
        }
    }
    CHECK(!heat_win_set(&w, 320.0f, 240.0f, 320u, 240u) && !w.ok);      /* a screen smaller than the window */
    CHECK(!heat_win_set(&w, 320.0f, 240.0f, SW, 64u) && !w.ok);
    CHECK(!heat_win_set(&w, (float)NAN, 240.0f, SW, SH) && !w.ok);
    CHECK(!heat_win_set(&w, 1e9f, 240.0f, SW, SH) && !w.ok);

    /* the copy: every texel is the pixel its column and row name, and nothing else is written */
    for (j = 0; j < SH; j++) for (i = 0; i < SW; i++) fb[j * SW + i] = (uint16_t)((j << 7) ^ i ^ (i << 11));
    CHECK(heat_win_set(&w, 300.0f, 260.0f, SW, SH));
    heat_grab(&w, fb, SW, tex);
    for (n = 0, j = 0; j < HEAT_TH; j++) for (i = 0; i < HEAT_TW; i++) n += tex[j * HEAT_TW + i] != fb[(unsigned)w.ys[j] * SW + w.xs[i]];
    CHECK(n == 0u);

    /* the mesh: every vertex on its grid place, samples inside the texture and within the wander of their own pixel,
       no share on the edge, HEAT_ALPHA at most, something at the middle */
    CHECK(heat_mesh(&w, 300.0f, 260.0f, 1.25f, 1.0f, m) == HEAT_VERTS);
    {
        unsigned amax = 0u;
        float dmax = 0.0f;
        for (j = 0; j <= HEAT_NY; j++) for (i = 0; i <= HEAT_NX; i++) {
            const heat_vtx_t *v = &m[j * (HEAT_NX + 1u) + i];
            const unsigned a = v->argb >> 24;
            const float px = v->u * (float)HEAT_WIN_W + (float)w.x, py = v->v * (float)HEAT_WIN_H + (float)w.y;
            CHECK(fabsf(v->x - (300.0f - HEAT_BAND_W * 0.5f + (float)i * HEAT_BAND_W / (float)HEAT_NX)) < 1e-3f);
            CHECK(fabsf(v->y - (260.0f - HEAT_BAND_UP + (float)j * (HEAT_BAND_UP + HEAT_BAND_DOWN) / (float)HEAT_NY)) < 1e-3f);
            CHECK(v->u > 0.0f && v->u < 1.0f && v->v > 0.0f && v->v < 1.0f);
            CHECK(fabsf(px - v->x) <= HEAT_AMP_X + 1e-2f && fabsf(py - v->y) <= HEAT_AMP_Y + 1e-2f);
            CHECK((v->argb & 0x00ffffffu) == 0x00ffffffu);
            if (i == 0u || i == HEAT_NX || j == 0u || j == HEAT_NY) CHECK(a == 0u);
            if (a > amax) amax = a;
            if (fabsf(px - v->x) > dmax) dmax = fabsf(px - v->x);
        }
        CHECK(amax == (unsigned)(HEAT_ALPHA * 255.0f + 0.5f));
        CHECK(dmax > HEAT_AMP_X * 0.8f);                                 /* and the samples do wander */
    }
    /* it moves with time and is the same for the same time; half a gate is half the share */
    CHECK(heat_mesh(&w, 300.0f, 260.0f, 1.25f, 1.0f, m2) == HEAT_VERTS && memcmp(m, m2, sizeof m) == 0);
    CHECK(heat_mesh(&w, 300.0f, 260.0f, 1.30f, 1.0f, m2) == HEAT_VERTS && memcmp(m, m2, sizeof m) != 0);
    CHECK(heat_mesh(&w, 300.0f, 260.0f, 1.25f, 0.5f, m2) == HEAT_VERTS);
    for (i = 0; i < HEAT_VERTS; i++) CHECK((m2[i].argb >> 24) <= (m[i].argb >> 24) / 2u + 1u && m2[i].u == m[i].u);
    /* night or rain: nothing; a window that is not set: nothing */
    memset(m2, 0x55, sizeof m2);
    CHECK(heat_mesh(&w, 300.0f, 260.0f, 1.25f, heat_gate(1.0f, 0u), m2) == 0u && heat_mesh(&w, 300.0f, 260.0f, 1.25f, heat_gate(0.0f, 2u), m2) == 0u);
    w.ok = 0;
    CHECK(heat_mesh(&w, 300.0f, 260.0f, 1.25f, 1.0f, m2) == 0u);
    for (n = 0, i = 0; i < sizeof m2; i++) n += ((const unsigned char *)m2)[i] != 0x55u;
    CHECK(n == 0u);
    /* a band whose anchor left the copied window: the samples stay inside the texture */
    CHECK(heat_win_set(&w, 300.0f, 260.0f, SW, SH) && heat_mesh(&w, 620.0f, 30.0f, 0.0f, 1.0f, m2) == HEAT_VERTS);
    for (i = 0; i < HEAT_VERTS; i++) CHECK(m2[i].u >= 0.5f / (float)HEAT_TW && m2[i].u <= 1.0f - 0.5f / (float)HEAT_TW && m2[i].v >= 0.5f / (float)HEAT_TH && m2[i].v <= 1.0f - 0.5f / (float)HEAT_TH);

    printf("heat: %u x %u texels (%u KB) of a %u x %u window, %u vertices, %u strip vertices (%u B of TA with the header)\n", HEAT_TW, HEAT_TH,
           HEAT_TW * HEAT_TH * 2u / 1024u, HEAT_WIN_W, HEAT_WIN_H, HEAT_VERTS, HEAT_NY * 2u * (HEAT_NX + 1u), (HEAT_NY * 2u * (HEAT_NX + 1u) + 1u) * 32u);
    printf(fails ? "heat_test: %d FAILED\n" : "heat_test: ok\n", fails);
    return fails != 0;
}
