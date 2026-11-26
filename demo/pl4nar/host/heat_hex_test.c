/* Host test of the street demo's hexagon glue (src/heat_hex.h): the frame's camera is the demo's own projection
   (sh4der_pl4nar_viewproj_rows, the rows demo.c loads) and heat.h's focal; the haze sheet stands at heat_anchor's point
   and the near cut HEXH_NEAR_BACK short of it; the first view after a reset adds nothing to the scroll, a still camera
   nothing, a drive streams the pattern down, a turn takes it with the far picture, a cut adds nothing. The lattice, the
   screen-to-world step and the field are test/hexheat_test.c's. Plain C:
   gcc -std=c99 -O2 -Wall -Wextra -DSH4DER_CORE_NO_SH4ZAM -Iinclude demo/pl4nar/host/heat_hex_test.c -lm */
#include <math.h>
#include <stdio.h>

#include "../src/heat_hex.h"
#include "sh4der_pl4nar.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

int main(void) {
    /* the demo's high camera behind a car at the origin that heads along +z; and its low one (1.9 m, 0.09 rad down) */
    const float eye[3] = { 0.0f, 3.3f, -7.0f }, at[3] = { 0.0f, 1.0f, 1.2f }, up[3] = { 0.0f, 1.0f, 0.0f };
    const float eye_low[3] = { 0.0f, 1.9f, -6.0f }, at_low[3] = { 0.0f, 1.9f - 10.0f * sinf(0.09f), -6.0f + 10.0f * cosf(0.09f) };
    const float fov = 0.96f;
    sh4der_hexheat_view_t v, v2;
    hexh_t x;
    float hp[3] = { 0.0f, 0.0f, 0.0f };

    /* the camera: the demo's own projection, heat.h's focal, the haze where heat_anchor puts it */
    CHECK(hexh_camera(&v, eye, at, fov, 640.0f, 480.0f, 0.0f) && v.ok);
    CHECK(fabsf(v.focal - HEAT_FOCAL) < 0.5f);
    CHECK(v.sheet == HEAT_DIST && v.near_cut == HEAT_DIST - HEXH_NEAR_BACK && HEXH_NEAR_BACK > 0.0f && HEXH_NEAR_BACK < HEAT_DIST);
    CHECK(heat_anchor(eye, at, 0.0f, hp));
    {
        sh4der_pl4nar_row_t row[4];
        sh4der_hexheat_hit_t hit;
        float sx = 0.0f, sy = 0.0f, w, px, py, x0 = 0.0f, y0 = 0.0f;
        memset(&hit, 0, sizeof hit);
        CHECK(sh4der_pl4nar_viewproj_rows(row, NULL, eye, at, up, fov, 0.3f, 640.0f, 480.0f));
        w = row[3].x * hp[0] + row[3].y * hp[1] + row[3].z * hp[2] + row[3].w;
        px = (row[0].x * hp[0] + row[0].y * hp[1] + row[0].z * hp[2] + row[0].w) / w;
        py = (row[1].x * hp[0] + row[1].y * hp[1] + row[1].z * hp[2] + row[1].w) / w;
        CHECK(sh4der_hexheat_world_to_screen(&v, hp, &sx, &sy) && fabsf(sx - px) < 0.02f && fabsf(sy - py) < 0.02f);
        CHECK(fabsf(w - v.w0) < 0.01f);                                   /* the anchor is the sheet's foot: its depth is the lattice's scale */
        CHECK(sh4der_hexheat_screen_to_haze(&v, sx, sy - 0.25f, &hit) && hit.ground == 0 && hit.h < 0.1f);
        CHECK(sh4der_hexheat_screen_to_haze(&v, sx, sy + 0.25f, &hit) && hit.ground == 1 && hit.up > -1.5f);
        /* the band's foot holds the lowest hexagon: the near cut's line, a hexagon's corner under it, a row to spare */
        {
            const float cut_pt[3] = { 0.0f, 0.0f, eye[2] + (HEAT_DIST - HEXH_NEAR_BACK) };
            float cx = 0.0f, cyy = 0.0f;
            CHECK(sh4der_hexheat_world_to_screen(&v, cut_pt, &cx, &cyy) && sh4der_hexheat_band(sx, sy, &x0, &y0));
            CHECK(cyy > sy && cyy + SH4DER_HEXHEAT_S + 1.0f <= y0 + (float)SH4DER_HEXHEAT_BAND_H);
            printf("the high camera: the 70 m line at y %.1f, the 64 m line %.1f px under it; the band's top left (%.0f, %.0f)\n", (double)sy, (double)(cyy - sy), (double)x0, (double)y0);
        }
    }
    /* the low camera too */
    {
        const float cut_pt[3] = { 0.0f, 0.0f, eye_low[2] + (HEAT_DIST - HEXH_NEAR_BACK) }, foot[3] = { 0.0f, 0.0f, eye_low[2] + HEAT_DIST };
        float sx = 0.0f, sy = 0.0f, cx = 0.0f, cyy = 0.0f, x0 = 0.0f, y0 = 0.0f;
        CHECK(hexh_camera(&v2, eye_low, at_low, fov, 640.0f, 480.0f, 0.0f));
        CHECK(sh4der_hexheat_world_to_screen(&v2, foot, &sx, &sy) && sh4der_hexheat_world_to_screen(&v2, cut_pt, &cx, &cyy) && sh4der_hexheat_band(sx, sy, &x0, &y0));
        CHECK(cyy > sy && cyy + SH4DER_HEXHEAT_S + 1.0f <= y0 + (float)SH4DER_HEXHEAT_BAND_H);
        printf("the low camera:  the 70 m line at y %.1f, the 64 m line %.1f px under it; the band's top left (%.0f, %.0f)\n", (double)sy, (double)(cyy - sy), (double)x0, (double)y0);
    }
    /* the hills: the road's height at the anchor is the ground's; on a climb that road is over the eye: the view holds
       (the haze must not drop out on a hill), and no ray lands */
    CHECK(hexh_camera(&v2, eye, at, fov, 640.0f, 480.0f, 1.25f) && fabsf(v2.e - (3.3f - 1.25f)) < 1e-5f);
    {
        float p[3], d;
        CHECK(hexh_camera(&v2, eye, at, fov, 640.0f, 480.0f, 4.0f) && v2.ok && v2.e < 0.0f);
        CHECK(!sh4der_hexheat_screen_to_ground(&v2, 320.0f, 400.0f, p, &d));
    }

    /* the scroll: nothing on the first view, nothing while the camera is still */
    hexh_reset(&x);
    hexh_view(&x, &v);
    CHECK(x.frame == 1u && x.h.sx == 0.0f && x.h.sy == 0.0f);
    hexh_view(&x, &v);
    CHECK(x.frame == 2u && fabsf(x.h.sx) < 1e-3f && fabsf(x.h.sy) < 1e-3f);
    /* driving: the camera a quarter of a metre on along the road: the pattern DRIVE x 0.25 m x ppm toward the camera */
    {
        const float e2[3] = { 0.0f, 3.3f, -6.75f }, a2[3] = { 0.0f, 1.0f, 1.45f };
        CHECK(hexh_camera(&v2, e2, a2, fov, 640.0f, 480.0f, 0.0f));
        hexh_view(&x, &v2);
        CHECK(fabsf(x.h.sy - SH4DER_HEXHEAT_DRIVE * 0.25f * v.ppm) < 1e-3f && fabsf(x.h.sx) < 1e-3f);
    }
    /* a cut (the camera 50 m away at once, as a stage change or the fly-by's start): not summed */
    {
        const float e3[3] = { 0.0f, 3.3f, 43.25f }, a3[3] = { 0.0f, 1.0f, 51.45f }, was = x.h.sy;
        CHECK(hexh_camera(&v2, e3, a3, fov, 640.0f, 480.0f, 0.0f));
        hexh_view(&x, &v2);
        CHECK(x.h.sy == was && fabsf(x.h.sx) < 1e-3f);
    }
    /* a turn: the scroll is the far picture's slip on the screen (a far point's own travel) */
    {
        const float yaw = 0.02f, at2[3] = { 8.2f * sinf(yaw), 1.0f, -7.0f + 8.2f * cosf(yaw) }, far_pt[3] = { 0.0f, 0.3f, 63.0f };
        float sx = 0.0f, sy = 0.0f, sx2 = 0.0f, sy2 = 0.0f;
        hexh_reset(&x);
        hexh_view(&x, &v);
        CHECK(hexh_camera(&v2, eye, at2, fov, 640.0f, 480.0f, 0.0f));
        hexh_view(&x, &v2);
        CHECK(sh4der_hexheat_world_to_screen(&v, far_pt, &sx, &sy) && sh4der_hexheat_world_to_screen(&v2, far_pt, &sx2, &sy2));
        CHECK(fabsf(sx2 - sx) > 8.0f && fabsf(x.h.sx - sh4der_hexheat_wrap_(sx2 - sx, sh4der_hexheat_lx_())) < 0.15f);
        hexh_reset(&x);
        CHECK(x.frame == 0u && x.h.sx == 0.0f);
    }

    printf(fails ? "heat_hex_test: %d FAILED\n" : "heat_hex_test: ok\n", fails);
    return fails != 0;
}
