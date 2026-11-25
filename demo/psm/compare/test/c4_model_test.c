/* SH4DER-PVR: the chain spike (c4), host test of the model

   demo/psm/compare/test/c4_model_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

   Checks ../c4_model.h and prints what the MODEL of the chain gives for the spike's scene. A model in plain C, not a
   measurement: the map and the mask here are made by rays, the way the PVR is expected to fill them. Built twice:
   plain C (SH4DER_CORE_NO_SH4ZAM) and on SH4ZAM's software back end.

     - the tables: every level is the cosine in real arithmetic rounded to a byte, every threshold its whole part
     - the window: lit exactly while the map's code is inside it, for every pair of codes
     - the bands: they tile each face (areas add up), every corner's code is inside its band, and a corner two bands
       share has the same bits in both (no crack between them)
     - the camera's matrix: a point through it lands on the pixel whose ray found it
     - the chain against a ray to the sun, for every pixel of the 320 x 240 mask, at three suns and three band widths:
         acne     the chain says shadow, the ray says lit, and the map pixel read holds the pixel's OWN face
         edge     the same, but the map pixel holds another face nearer the light: the map's pixel edge
         rim      the same, but the map pixel holds a face far BEHIND (its centre is off the edge of the pixel's
                  face): farther than the window reaches, so the cyclic compare reads shadow
         in bias  the chain says lit, the ray says shadow, and the caster is nearer than bias + 2 codes
         other    the same, farther: the map's pixel edge, or the wrap
         wrap     shadowed pixels whose caster is so much nearer the light that the cyclic compare reads lit
       acne must be 0 with the bias rule as it stands. The faces are grown in the map pass by 0 and by 0.75 pixel.
*/

#include <math.h>
#include <stdio.h>

#include "../c4_model.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static void levels(void) {
    unsigned d, bad = 0, badt = 0;

    for (d = 0; d < 256u; d++) bad += c4_level[d] != (unsigned)floor(128.0 + 127.0 * cos(6.283185307179586 * (double)d / 256.0) + 0.5);
    for (d = 0; d <= 128u; d++) badt += c4_threshold[d] != (unsigned)floor(128.0 - 127.0 * cos(6.283185307179586 * (double)d / 256.0) + 1e-9);
    printf("  MODEL tables: %u of 256 levels and %u of 129 thresholds differ from the cosine in double\n", bad, badt);
    CHECK(bad == 0);
    CHECK(badt == 0);
}

static void window(void) {
    static const unsigned ws[3] = { 16u, 32u, 64u };
    unsigned i, r, m, bad = 0;

    for (i = 0; i < 3u; i++) {
        c4_set_t st;
        unsigned lit = 0;

        c4_set_init(&st, ws[i], 0u, 1.5f, 0.0f);
        for (r = 0; r < 256u; r++) {
            const unsigned q = c4_q((float)r, st.w, 2.0f);

            for (m = 0; m < 256u; m++) {
                const int delta = (int)((m - r + 2u + 256u + 128u) & 255u) - 128;    /* m - r + bias, as -128 .. 127 */
                const int in = delta > 1 && delta < (int)(2u * st.w) - 1;          /* inside the window, its two end codes left out */
                const int s = c4_shadow(&st, m, q);

                if (in && s) bad++;                                                /* the threshold is a whole number: an end may fall either way */
                if (!s) lit++;
            }
        }
        printf("  MODEL window w %u: threshold %u, %u codes of 256 read lit\n", st.w, st.thr, lit / 256u);
    }
    CHECK(bad == 0);
}

static float area(const c4_corner_t *p, unsigned n) {
    float a[3] = { 0.0f, 0.0f, 0.0f };
    unsigned i;

    for (i = 1; i + 1u < n; i++) {
        const float e1[3] = { p[i].p[0] - p[0].p[0], p[i].p[1] - p[0].p[1], p[i].p[2] - p[0].p[2] };
        const float e2[3] = { p[i + 1u].p[0] - p[0].p[0], p[i + 1u].p[1] - p[0].p[1], p[i + 1u].p[2] - p[0].p[2] };
        float c[3];

        sh4der_cross_(e1, e2, c);
        a[0] += c[0]; a[1] += c[1]; a[2] += c[2];
    }
    return 0.5f * sqrtf(c4_dot(a, a));
}

static void bands(const c4_light_t *L, unsigned gs) {
    c4_face_t f[C4_FACES];
    unsigned i, k, j, nb = 0, nc = 0, lit_faces = 0, shared = 0, cracked = 0;

    c4_faces(f, 0.0f);
    for (i = 0; i < C4_FACES; i++) {
        c4_corner_t face[4], out[C4_BAND_MAX], before[C4_BAND_MAX];
        float inv[4], lo = 1e30f, hi = -1e30f, sum = 0.0f;
        unsigned nbefore = 0;
        int b;

        if (c4_dot(f[i].n, L->s) <= C4_FACING) continue;
        lit_faces++;
        c4_face_corners(L, &f[i], face, inv);
        for (k = 0; k < 4u; k++) { if (face[k].m[2] < lo) lo = face[k].m[2]; if (face[k].m[2] > hi) hi = face[k].m[2]; }
        for (b = c4_band(lo, gs); b <= c4_band(hi, gs); b++) {
            const float edge = (float)(b * (1 << gs)) - 0.5f;
            const unsigned n = c4_band_poly(face, inv, b, gs, out);

            for (k = 0; k < nbefore; k++) {                      /* the corners the band before left on this band's lower level */
                int found = 0;

                if (before[k].m[2] != edge) continue;
                shared++;
                for (j = 0; j < n; j++) found |= !memcmp(&before[k], &out[j], sizeof out[j]);
                cracked += !found && n != 0u;
            }
            nbefore = n; memcpy(before, out, sizeof before);
            if (n < 3u) continue;
            nb++; nc += n;
            sum += area(out, n);
            for (k = 0; k < n; k++) CHECK(out[k].m[2] >= edge - 0.001f && out[k].m[2] <= edge + (float)(1 << gs) + 0.001f);
        }
        CHECK(fabsf(sum - area(face, 4u)) < 1e-2f * area(face, 4u) + 1e-3f);
    }
    printf("    bands %u codes wide: %u faces toward the sun cut into %u bands, %u corners; %u corners shared by two bands, %u not the same bits\n",
           1u << gs, lit_faces, nb, nc, shared, cracked);
    CHECK(cracked == 0);
    CHECK(shared > nb / 2u);
}

static void camera(const c4_light_t *L) {
    static uint8_t map[C4_MAP * C4_MAP];
    c4_cam_t cam;
    c4_set_t st;
    float m[16], worst = 0.0f;
    unsigned x, y, n = 0;

    c4_camera(&cam, C4_MASK_W, C4_MASK_H);
    c4_cam_matrix(&cam, m);
    c4_set_init(&st, 32u, 1u, 1.5f, 0.0f);
    memset(map, 255, sizeof map);
    for (y = 3; y < C4_MASK_H; y += 7u) {
        for (x = 2; x < C4_MASK_W; x += 9u) {
            const float ifo = 1.0f / cam.focal, sx = ((float)x + 0.5f - cam.cx) * ifo, sy = -((float)y + 0.5f - cam.cy) * ifo;
            const float d[3] = { cam.fwd[0] + sx * cam.right[0] + sy * cam.up[0], cam.fwd[1] + sx * cam.right[1] + sy * cam.up[1], cam.fwd[2] + sx * cam.right[2] + sy * cam.up[2] };
            float t, nrm[3], p[3], X, Y, W, e;
            unsigned face;

            if (!c4_ray(cam.eye, d, 0.0f, &t, nrm, &face)) continue;
            p[0] = cam.eye[0] + t * d[0]; p[1] = cam.eye[1] + t * d[1]; p[2] = cam.eye[2] + t * d[2];
            X = m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12];
            Y = m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13];
            W = m[3] * p[0] + m[7] * p[1] + m[11] * p[2] + m[15];
            CHECK(W > 1.0f);
            e = fabsf(X / W - ((float)x + 0.5f)); if (e > worst) worst = e;
            e = fabsf(Y / W - ((float)y + 0.5f)); if (e > worst) worst = e;
            n++;
        }
    }
    printf("  MODEL camera matrix: %u points, the farthest lands %.4f pixel from its ray's pixel\n", n, (double)worst);
    CHECK(n > 500u && worst < 0.02f);
    (void)L;
}

static void chain(float az, float el, unsigned w, float grow) {
    static uint8_t map[C4_MAP * C4_MAP], ids[C4_MAP * C4_MAP];
    c4_light_t L;
    c4_cam_t cam;
    unsigned gs, x, y;

    c4_light_fit(&L, az, el);
    c4_camera(&cam, C4_MASK_W, C4_MASK_H);
    c4_model_map(&L, grow, map, ids);
    printf("  MODEL sun azimuth %.0f elevation %.1f, window %u, faces grown %.2f: a code is %.1f cm of depth, a map pixel %.1f x %.1f cm\n", (double)az, (double)el, w, (double)grow,
           100.0 / (double)L.sd, 100.0 * (double)L.iu, 100.0 * (double)L.iv);
    for (gs = 0; gs < 3u; gs++) {
        static const float up[3] = { 0.0f, 1.0f, 0.0f };
        unsigned lit = 0, sh = 0, acne = 0, edge = 0, rim = 0, inbias = 0, other = 0, wrap = 0;
        c4_set_t st;

        c4_set_init(&st, w, gs, 1.5f, grow);
        bands(&L, gs);
        for (y = 0; y < C4_MASK_H; y++) {
            for (x = 0; x < C4_MASK_W; x++) {
                c4_pixel_t px;
                const int m = c4_model_pixel(&L, &cam, map, &st, x, y, &px);
                const float b = c4_bias(&L, up, st.b0, st.gs);   /* the ground's, as the yardstick */

                if (m == C4_SKY) continue;
                if (px.truth == C4_LIT) {
                    lit++;
                    if (m == C4_SHADOW) { if (ids[px.my * C4_MAP + px.mx] == px.face) acne++; else if (px.sep > 0.0f) edge++; else rim++; }
                } else {
                    sh++;
                    if (m == C4_LIT) { if (px.sep < b + 2.0f) inbias++; else other++; }
                    if (px.sep > 256.0f - 2.0f * (float)w + b) wrap++;
                }
            }
        }
        printf("      ground bias %.1f codes; pixels lit %u, shadow %u; acne %u, edge %u, rim %u; lit in error: in bias %u, other %u; wrap %u\n",
               (double)c4_bias(&L, up, st.b0, st.gs), lit, sh, acne, edge, rim, inbias, other, wrap);
        CHECK(acne == 0);
        CHECK(lit > 10000u && sh > 500u);
    }
}

int main(void) {
    c4_light_t L;

    levels();
    window();
    c4_light_fit(&L, 150.0f, 26.5f);
    camera(&L);
    chain(150.0f, 26.5f, 32u, 0.0f);
    chain(150.0f, 26.5f, 32u, 0.75f);
    chain(150.0f, 26.5f, 64u, 0.75f);
    chain(210.0f, 45.0f, 32u, 0.75f);
    chain(100.0f, 13.0f, 32u, 0.0f);
    chain(100.0f, 13.0f, 32u, 0.75f);
    chain(100.0f, 13.0f, 64u, 0.75f);
    printf(fails ? "c4_model_test: %d FAILED\n" : "c4_model_test: ok\n", fails);
    return fails != 0;
}
