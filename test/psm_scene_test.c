/* SH4DER-PVR: the perspective shadow map demo's scene test

   test/psm_scene_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   Host check of demo/psm/psm_scene.h, the part of the Dreamcast A-vs-B program that is not a KallistiOS call: the
   rings (0.25 m deep near the camera, 2 m at 70 m), the models' counts; every ground vertex of the window in front
   of the camera and the window wider than the screen at any yaw; ground vertices that do not move when the camera
   turns; route B at the three presets and on a yaw sweep (a pole's shadow where it must be, a lit point lit, the
   models neither all dark nor all lit); route A (every vertex in view has coordinates, the casters reach the map,
   the lookup's error in the near cells); the head-on fallback; the pad; and the readout, which shows "--" until
   the console has measured. Walking: the torchwalk pad (the stick looks, the face buttons walk, the menu, START as a shift
   never also a tap), the walk area and the circles round the poles and cows (no way in, no way stuck), the grid's centre
   snapped to its lattice (still inside a cell, one whole step across one, the same grid by any path), the window at every
   pitch and eye offset, both routes away from the origin, the near cut of a model triangle, the worst frame's vertices
   under the buffer. CASTERS: all against movers only on both routes (all is the whole set, byte for byte). Hold to repeat (by
   time held, not frames; a tap is one step; capped a frame) and DRAW DISTANCE (the view's cut, INFINITE as before, the map's
   casters untouched).
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../demo/psm/psm_scene.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static psm_scene_t scene;
static psm_frame_t frame;
static uint16_t depth[256 * 256];

static void scene_counts(void) {
    unsigned i;

    psm_scene_init(&scene);
    printf("  rings %u, ground vertices a frame %u, model vertices %u, triangles %u\n", scene.rows, scene.rows * (PSM_COLS + 1u), scene.mvn, scene.mtn);
    CHECK(scene.rows > 60u && scene.rows <= PSM_ROWS_MAX);
    CHECK(scene.row_r[scene.rows - 1u] >= PSM_ROW_END);
    CHECK(scene.rows * (2u * psm_window_half(PSM_PITCH) + 1u) <= 6000u);   /* the mock-up's guess: 3,000 to 5,000 in view; the walk's window is wider */
    for (i = 1; i < scene.rows; i++) CHECK(scene.row_r[i] > scene.row_r[i - 1u]);
    CHECK(fabsf((scene.row_r[3] - scene.row_r[2]) - 0.25f) < 0.03f);   /* near: 0.25 m */
    for (i = 1; i < scene.rows; i++)
        if (scene.row_r[i - 1u] >= 70.0f) { CHECK(fabsf((scene.row_r[i] - scene.row_r[i - 1u]) - 2.0f) < 0.15f); break; }
    CHECK(scene.mvn >= 240u + 2u * 100u && scene.mvn <= PSM_MV_MAX);
    CHECK(scene.mtn >= 120u + 2u * 60u && scene.mtn <= 120u + 800u);   /* the SH-4's budget: about 800 for the cows */
    for (i = 0; i < scene.mtn; i++) CHECK(scene.mt[i][0] < scene.mvn && scene.mt[i][1] < scene.mvn && scene.mt[i][2] < scene.mvn);
    for (i = 0; i < scene.mvn; i++) CHECK(fabsf(sh4der_len3_(scene.mn[i][0], scene.mn[i][1], scene.mn[i][2]) - 1.0f) < 0.02f);
}

/* the window: in front of the camera, and past the screen's left and right edges on every ring the screen shows */
static void window(void) {
    psm_state_t st;
    float rows[16] = { 0.0f };
    unsigned step;

    psm_state_default(&st);
    for (step = 0; step < 73u; step++) {
        unsigned i, r;

        st.yaw = (float)step * 0.0873f;
        psm_frame_setup(&frame, &scene, &st);
        CHECK(sh4der_psm_cam_rows(&frame.cam, PSM_NEAR, rows));
        for (i = 0; i < frame.gvn; i++) {
            float h[4];

            sh4der_psm_xform(rows, frame.gpos[i], 1.0f, h);
            if (!(h[3] > PSM_NEAR)) { CHECK(h[3] > PSM_NEAR); break; }
        }
        for (r = 0; r < frame.grows; r++) {
            float l[4], rt[4];

            sh4der_psm_xform(rows, frame.gpos[r * frame.gcols], 1.0f, l);
            sh4der_psm_xform(rows, frame.gpos[r * frame.gcols + frame.gcols - 1u], 1.0f, rt);
            if (fabsf(l[1]) > l[3] && fabsf(rt[1]) > rt[3]) continue;          /* the ring's ends are above or below the screen */
            if (!(l[0] < -l[3] && rt[0] > rt[3])) { CHECK(l[0] < -l[3] && rt[0] > rt[3]); break; }
        }
    }
}

/* a ground vertex is a point of the world: a small turn moves the window by whole spokes, not the vertices */
static void fixed_vertices(void) {
    static float keep[PSM_GV_MAX][3];
    psm_state_t st;
    unsigned i, same = 0, cols;

    psm_state_default(&st);
    st.yaw = 1.0f;
    psm_frame_setup(&frame, &scene, &st);
    memcpy(keep, frame.gpos, sizeof keep);
    cols = frame.gcols;
    st.yaw = 1.0f + 3.0f * PSM_COL_STEP;
    psm_frame_setup(&frame, &scene, &st);
    for (i = 0; i + 3u < cols; i++)
        same += fabsf(frame.gpos[i][0] - keep[i + 3u][0]) < 1e-4f && fabsf(frame.gpos[i][2] - keep[i + 3u][2]) < 1e-4f;
    CHECK(same == cols - 3u);
}

/* nearest ground vertex to (x, z) */
static unsigned nearest(float x, float z) {
    unsigned i, best = 0;
    float bd = 1e30f;

    for (i = 0; i < frame.gvn; i++) {
        const float dx = frame.gpos[i][0] - x, dz = frame.gpos[i][2] - z, d = dx * dx + dz * dz;

        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

static void route_b(void) {
    psm_state_t st;
    unsigned w, p, i;

    for (p = 0; p < 3u; p++)
        for (w = 0; w < 2u; w++) {
            unsigned dark = 0, mdark = 0, mlit = 0, v;
            float t, sx, sz;

            psm_state_default(&st);
            st.route = PSM_ROUTE_B; st.warp = (int)w;
            for (i = 0; i < p; i++) psm_row_change(&st, PSM_ROW_PRESET, 1);
            psm_frame_setup(&frame, &scene, &st);
            CHECK(frame.built);
            if (w && frame.head_on) printf("  preset %u warp: head-on, the uniform map stands in\n", p);
            CHECK(psm_frame_b_begin(&frame, &scene, depth));
            psm_frame_b_casters(&frame, &scene, 0);
            psm_frame_b_receivers(&frame, &scene, &st);
            CHECK(frame.tris == scene.mtn && frame.drawn > 0u);
            for (i = 0; i < frame.gvn; i++) dark += frame.glit[i] < 128u;
            for (i = 0; i < scene.mvn; i++) { mdark += frame.mlit[i] == 0u; mlit += frame.mlit[i] > 100u; }
            printf("  B preset %u %-7s kind %-12s drawn %u/%u, ground shadowed %u of %u, model vertices dark %u lit %u\n", p,
                   st.warp && !frame.head_on ? "warp" : "uniform", psm_kind_name(frame.map.kind), frame.drawn, frame.tris, dark, frame.gvn, mdark, mlit);
            CHECK(dark > 0u && dark < frame.gvn / 2u);
            CHECK(mdark > 0u && mlit > 0u);
            /* the shadow of the second pole (x 2.6, z -8.4), 2 m up its 5 m: the ground point under that ray is shadowed */
            t = 2.0f / frame.sun[1];
            sx = 2.6f - frame.sun[0] * t; sz = -8.4f - frame.sun[2] * t;
            v = nearest(sx, sz);
            if (fabsf(frame.gpos[v][0] - sx) < 0.06f && fabsf(frame.gpos[v][2] - sz) < 0.06f && st.warp && !frame.head_on)
                CHECK(frame.glit[v] < 128u);
            /* open ground 3 m ahead, between the poles' rows: lit, unless a shadow runs up the middle (sun ahead or behind) */
            v = nearest(0.0f, -3.0f);
            if (p == 0u) CHECK(frame.glit[v] == 255u);
        }
}

static void route_a(void) {
    static sh4der_psm_pvr_vtx_t out[SH4DER_PSM_PVR_MAX_TRIS][3];
    psm_state_t st;
    float rows[16] = { 0.0f };
    unsigned w, i;

    for (w = 0; w < 2u; w++) {
        unsigned tris = 0, bad = 0, r, c;
        float worst_near = 0.0f, worst = 0.0f, worst_in = 0.0f;

        psm_state_default(&st);
        st.warp = (int)w;
        psm_frame_setup(&frame, &scene, &st);
        CHECK(frame.built && !frame.head_on);
        psm_frame_unshadowed(&frame, &scene);
        psm_frame_a_ground(&frame);
        CHECK(sh4der_psm_cam_rows(&frame.cam, PSM_NEAR, rows));
        for (i = 0; i < frame.gvn; i++) {                                     /* every vertex on screen has coordinates, inside the target */
            float h[4];

            sh4der_psm_xform(rows, frame.gpos[i], 1.0f, h);
            if (!(fabsf(h[0]) < h[3] && fabsf(h[1]) < h[3] && h[2] > 0.0f && h[2] < h[3])) continue;
            if (sh4der_dot3_(frame.gpos[i], frame.gpos[i]) < 2.4f * 2.4f) continue;   /* nearer than the fit's 2 m plane */
            bad += !frame.gok[i] || frame.guv[i][0] < -0.01f || frame.guv[i][0] > 1.01f || frame.guv[i][1] < -0.01f || frame.guv[i][1] > 1.01f;
        }
        CHECK(bad == 0u);
        for (i = 0; i < scene.mtn; i++)
            tris += sh4der_psm_pvr_tris(&frame.map, frame.size, scene.mv[scene.mt[i][0]], scene.mv[scene.mt[i][1]], scene.mv[scene.mt[i][2]], out);
        CHECK(tris > 0u && tris <= scene.mtn * SH4DER_PSM_PVR_MAX_TRIS);
        for (r = 0; r + 1u < frame.grows; r++)                               /* the lookup's error a cell, in texels */
            for (c = 0; c + 1u < frame.gcols; c++) {
                const unsigned a = r * frame.gcols + c, b = a + 1u, d = a + frame.gcols;
                float e;

                if (!frame.gok[a] || !frame.gok[b] || !frame.gok[d]) continue;
                e = sh4der_psm_pvr_uv_error(&frame.map, frame.size, frame.gpos[a], frame.gpos[b], frame.gpos[d]);
                if (scene.row_r[r] < 1.5f) { if (e > worst_in) worst_in = e; continue; }   /* the walk's inner rings: under the eye, nearer than the fit */
                if (e > worst) worst = e;
                if (scene.row_r[r] >= 2.3f && e > worst_near) worst_near = e;   /* the nearest ground the screen shows is at 2.3 m */
            }
        printf("  A %-7s map triangles %u, lookup error: %.3f texels on screen, %.3f in the rings under it, %.3f in the rings under 1.5 m (off screen, nearer than the fit)\n",
               w ? "warp" : "uniform", tris, worst_near, worst, worst_in);
        if (!w) CHECK(worst < 0.01f);                                         /* a uniform map is affine: exact */
        else CHECK(worst_near < 0.75f && worst < 1.5f);                       /* the graded grid keeps the warp's lookup under a texel */
    }
}

static void sweep(void) {
    psm_state_t st;
    unsigned step, head = 0, none = 0;

    psm_state_default(&st);
    st.route = PSM_ROUTE_B; st.warp = 1; st.az = 0.0f; st.el = 20.0f;      /* the sun straight ahead at yaw 0 */
    for (step = 0; step < 72u; step++) {
        unsigned i;

        st.yaw = (float)step * 0.0873f;
        psm_frame_setup(&frame, &scene, &st);
        head += frame.head_on != 0;
        none += !frame.built;
        if (psm_frame_b_begin(&frame, &scene, depth)) {
            psm_frame_b_casters(&frame, &scene, 0);
            psm_frame_b_receivers(&frame, &scene, &st);
        } else {
            psm_frame_unshadowed(&frame, &scene);
        }
        psm_frame_a_ground(&frame);
        for (i = 0; i < frame.gvn; i++) if (frame.gok[i] && !(sh4der_finite_(frame.guv[i][0]) && sh4der_finite_(frame.guv[i][1]))) { CHECK(0); break; }
    }
    printf("  yaw sweep, sun az 0 el 20: head-on fallback on %u of 72 headings, no map on %u\n", head, none);
    CHECK(none == 0u);
}

/* ---- the pad (torchwalk's single stick), the menu, walking ---------------------------------------------------- */

static void in_(psm_state_t *st, const psm_walk_limits_t *lim, unsigned held, float sx, float sy, float lt, float rt, float dt) {
    psm_pad_t p;

    p.held = held; p.sx = sx; p.sy = sy; p.lt = lt; p.rt = rt;
    psm_input(st, lim, &p, dt);
}

static void pad(void) {
    psm_state_t st;
    float z0, x1, y0, p0;
    unsigned i;

    psm_state_default(&st);
    CHECK(st.route == PSM_ROUTE_A && st.warp == 0 && psm_map_sizes[st.size_i] == 256u && st.readout == 1);   /* the boot state */
    CHECK(st.invert_y && !st.menu && !st.movers_only && st.pitch == PSM_PITCH && st.x == 0.0f && st.z == 0.0f);
    in_(&st, NULL, PSM_BTN_UP, 0, 0, 0, 0, 0.016f);                          /* D-pad up: the route, A B AB round */
    CHECK(st.route == PSM_ROUTE_B);
    in_(&st, NULL, PSM_BTN_UP, 0, 0, 0, 0, 0.016f);                          /* held: no repeat */
    CHECK(st.route == PSM_ROUTE_B);
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
    in_(&st, NULL, PSM_BTN_UP, 0, 0, 0, 0, 0.016f);
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
    CHECK(st.route == PSM_ROUTES - 1);                                       /* AB */
    in_(&st, NULL, PSM_BTN_UP, 0, 0, 0, 0, 0.016f);
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
    CHECK(st.route == PSM_ROUTE_A);
    in_(&st, NULL, PSM_BTN_UP, 0, 0, 0, 0, 0.016f);
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
    CHECK(st.route == PSM_ROUTE_B);
    in_(&st, NULL, PSM_BTN_DOWN, 0, 0, 0, 0, 0.016f);                        /* D-pad down: uniform / PSM, once a press */
    CHECK(st.warp == 1 && !st.menu);
    in_(&st, NULL, PSM_BTN_DOWN, 0, 0, 0, 0, 0.5f);
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
    CHECK(st.warp == 1);
    in_(&st, NULL, PSM_BTN_START, 0, 0, 0, 0, 0.016f);                       /* START opens the panel; held, nothing more */
    in_(&st, NULL, PSM_BTN_START, 0, 0, 0, 1.0f, 0.5f);
    CHECK(st.menu && st.grp == PSM_GRP_SHADOW && st.el == 35.0f);            /* open, a trigger does not move the sun */
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
    in_(&st, NULL, PSM_BTN_START, 0, 0, 0, 0, 0.016f);                       /* START closes it */
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
    CHECK(!st.menu && st.warp == 1 && st.route == PSM_ROUTE_B && st.warp_near == 4.0f && !st.movers_only);
    in_(&st, NULL, PSM_BTN_RIGHT, 0, 0, 0, 0, 0.016f);                       /* D-pad right: the sun's azimuth */
    CHECK(st.az == 125.0f);
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
    st.az = 355.0f;
    in_(&st, NULL, PSM_BTN_RIGHT, 0, 0, 0, 0, 0.016f);
    CHECK(st.az == 0.0f);
    in_(&st, NULL, 0u, 0, 0, 0, 1.0f, 0.5f);                                 /* R: the sun up, 30 degrees a second */
    CHECK(fabsf(st.el - 50.0f) < 1e-3f);
    in_(&st, NULL, 0u, 0, 0, 0.05f, 0, 0.5f);                                /* a trigger barely pressed: let go */
    CHECK(fabsf(st.el - 50.0f) < 1e-3f);
    in_(&st, NULL, 0u, 0, 0, 1.0f, 0, 10.0f);
    CHECK(st.el == 5.0f);

    /* the face buttons walk, by frame time: 60 frames at 60 a second, 20 at 20, the same metre count */
    psm_state_default(&st);
    for (i = 0; i < 60u; i++) in_(&st, NULL, PSM_BTN_Y, 0, 0, 0, 0, 1.0f / 60.0f);
    z0 = st.z;
    CHECK(fabsf(st.z + PSM_WALK_SPEED) < 1e-3f && fabsf(st.x) < 1e-4f);      /* yaw 0 looks along -z */
    psm_state_default(&st);
    for (i = 0; i < 20u; i++) in_(&st, NULL, PSM_BTN_Y, 0, 0, 0, 0, 1.0f / 20.0f);
    CHECK(fabsf(st.z - z0) < 1e-3f);
    in_(&st, NULL, PSM_BTN_A, 0, 0, 0, 0, 1.0f);                             /* A: back */
    CHECK(fabsf(st.z) < 1e-3f);
    in_(&st, NULL, PSM_BTN_X, 0, 0, 0, 0, 0.5f);                             /* X: strafe left (-x at yaw 0) */
    x1 = st.x;
    CHECK(fabsf(x1 + 0.5f * PSM_WALK_SPEED) < 1e-3f);
    in_(&st, NULL, PSM_BTN_B, 0, 0, 0, 0, 0.5f);                             /* B: right */
    CHECK(fabsf(st.x) < 1e-3f && fabsf(st.z) < 1e-3f);
    st.yaw = 1.5707963f;                                                     /* facing +x, Y walks +x, B strafes +z */
    in_(&st, NULL, PSM_BTN_Y | PSM_BTN_B, 0, 0, 0, 0, 0.25f);
    CHECK(fabsf(st.x - 0.25f * PSM_WALK_SPEED) < 1e-3f && fabsf(st.z - 0.25f * PSM_WALK_SPEED) < 1e-3f);

    /* the stick looks: a dead zone of 12 / 128, 2.2 radians a second, the yaw wraps, the pitch held */
    psm_state_default(&st);
    in_(&st, NULL, 0u, 0.05f, 0.05f, 0, 0, 1.0f);
    CHECK(st.yaw == 0.0f && st.pitch == PSM_PITCH && st.x == 0.0f && st.z == 0.0f);
    in_(&st, NULL, 0u, 0.5f, 0, 0, 0, 0.1f);
    CHECK(fabsf(st.yaw - 0.11f) < 1e-5f);
    in_(&st, NULL, 0u, -1.0f, 0, 0, 0, 1.0f);
    CHECK(st.yaw > 4.0f && st.yaw < 6.2832f);
    psm_state_default(&st);
    y0 = st.pitch;
    in_(&st, NULL, 0u, 0, -1.0f, 0, 0, 0.05f);                               /* INVERT Y on: the stick up looks down */
    CHECK(fabsf(st.pitch - (y0 + 0.11f)) < 1e-5f);
    st.invert_y = 0;
    p0 = st.pitch;
    in_(&st, NULL, 0u, 0, -1.0f, 0, 0, 0.05f);                               /* off: up looks up */
    CHECK(fabsf(st.pitch - (p0 - 0.11f)) < 1e-5f);
    in_(&st, NULL, 0u, 0, 1.0f, 0, 0, 5.0f);
    CHECK(st.pitch == PSM_PITCH_DOWN);
    in_(&st, NULL, 0u, 0, -1.0f, 0, 0, 5.0f);
    CHECK(st.pitch == -PSM_PITCH_UP);
}

static void menu(void) {
    char ln[PSM_HUD_LINES][PSM_HUD_COLS + 1u], v[48];
    psm_state_t st;
    psm_frame_t *f = &frame;
    unsigned i;
    int r;

    psm_state_default(&st);
    in_(&st, NULL, PSM_BTN_START, 0, 0, 0, 0, 0.016f);                       /* START opens it */
    CHECK(st.menu && st.row == PSM_ROW_ROUTE && st.grp == PSM_GRP_SHADOW);
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
    for (i = 0; i < 30u; i++) in_(&st, NULL, PSM_BTN_Y | PSM_BTN_X, 0, 0, 0, 0, 0.05f);   /* open: the face buttons do not walk */
    CHECK(st.x == 0.0f && st.z == 0.0f);
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
    in_(&st, NULL, PSM_BTN_B | PSM_BTN_A, 0, 0, 0, 0, 0.5f);                 /* nor B or A: A changes the row (the route), B closes */
    CHECK(st.x == 0.0f && st.z == 0.0f && st.route == PSM_ROUTE_B && !st.menu);
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
    in_(&st, NULL, PSM_BTN_START, 0, 0, 0, 0, 0.016f);
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
    in_(&st, NULL, PSM_BTN_DOWN, 0, 0, 0, 0, 0.016f);                        /* open: D-pad down picks the next row */
    CHECK(st.menu && st.row == PSM_ROW_MAP && st.route == PSM_ROUTE_B && st.warp == 0);
    in_(&st, NULL, PSM_BTN_DOWN | PSM_BTN_RIGHT, 0, 0, 0, 0, 0.016f);        /* right changes it */
    CHECK(st.warp == 1);
    in_(&st, NULL, PSM_BTN_DOWN | PSM_BTN_RIGHT | PSM_BTN_LEFT, 0, 0, 0, 0, 0.016f);
    CHECK(st.warp == 0);
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
    in_(&st, NULL, PSM_BTN_UP, 0, 0, 0, 0, 0.016f);
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
    in_(&st, NULL, PSM_BTN_UP, 0, 0, 0, 0, 0.016f);                          /* up past the group's first row: its last */
    {
        int rows[PSM_ROWS];
        const unsigned n = psm_grp_rows(PSM_GRP_SHADOW, rows);

        CHECK(st.row == rows[n - 1u] && st.route == PSM_ROUTE_B);
    }
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
    in_(&st, NULL, PSM_BTN_START, 0, 0, 0, 0, 0.016f);                       /* START closes it */
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
    CHECK(!st.menu && st.warp == 0);
    in_(&st, NULL, PSM_BTN_Y, 0, 0, 0, 0, 0.5f);                             /* closed: walking again */
    CHECK(st.z < -1.0f);

    /* every row changes both ways and reads as a line of its own */
    psm_state_default(&st);
    st.dist_i = 5u;                                                          /* DRAW DISTANCE off its top: +1 changes it */
    psm_frame_setup(f, &scene, &st);
    for (r = 0; r < PSM_ROWS; r++) {
        psm_state_t b = st;

        psm_row_change(&st, r, 1);
        CHECK(memcmp(&b, &st, sizeof b) != 0);
        CHECK(psm_row_value(&st, r, v, sizeof v)[0] != 0);
        CHECK(strlen(psm_row_name[r]) + 2u + strlen(psm_row_value(&st, r, v, sizeof v)) + strlen("   (D-pad row/change, B close)") <= PSM_HUD_COLS);
        st.row = r;
        psm_hud(&st, f, NULL, ln);
        CHECK(strstr(ln[8], psm_row_name[r]) == ln[8]);
        psm_row_change(&st, r, -1);
        CHECK(strlen(psm_row_name[r]) + 2u + strlen(psm_row_value(&st, r, v, sizeof v)) + strlen("   (D-pad row/change, B close)") <= PSM_HUD_COLS);
    }
    psm_state_default(&st);
    psm_row_change(&st, PSM_ROW_READOUT, 1);
    CHECK(st.readout == 2 && st.split);
    psm_row_change(&st, PSM_ROW_READOUT, 1);
    CHECK(st.readout == 0 && !st.split);
    psm_row_change(&st, PSM_ROW_PRESET, 1);
    CHECK(st.az == 0.0f && st.el == 20.0f);
    psm_row_change(&st, PSM_ROW_PRESET, -1);
    CHECK(st.az == 120.0f && st.el == 35.0f);
    st.bias_a = 0.2f; psm_row_change(&st, PSM_ROW_BIAS_A, 1);
    CHECK(st.bias_a == 0.2f);
    st.el = 5.0f; psm_row_change(&st, PSM_ROW_ELEVATION, -1);
    CHECK(st.el == 5.0f);
}

/* the walk area and the circles: no way out, no way in, no way stuck */
static void walk_limits(void) {
    psm_walk_limits_t lim;
    psm_state_t st;
    unsigned i, k, out = 0, in = 0, pocket = 0, seed = 12345u;
    float gx, gz;

    psm_scene_limits(&scene, &lim);
    CHECK(lim.n == 14u);                                                    /* ten poles, two circles a cow */
    for (i = 0; i < lim.n; i++)
        CHECK(lim.circle[i][0] - lim.circle[i][2] > lim.x0 && lim.circle[i][0] + lim.circle[i][2] < lim.x1
              && lim.circle[i][1] - lim.circle[i][2] > lim.z0 && lim.circle[i][1] + lim.circle[i][2] < lim.z1);
    CHECK(lim.circle[0][2] > 0.2122f + PSM_NEAR);                            /* a pole's face is past the near plane */

    psm_state_default(&st);
    st.x = 10.0f;                                                            /* a clear line: the area stops it */
    for (i = 0; i < 400u; i++) in_(&st, &lim, PSM_BTN_Y, 0, 0, 0, 0, 0.1f);
    CHECK(st.z == PSM_AREA_Z0 && st.x == 10.0f);
    for (i = 0; i < 400u; i++) in_(&st, &lim, PSM_BTN_B, 0, 0, 0, 0, 0.1f);
    CHECK(st.x == PSM_AREA_X);

    /* a pole: walk at it, stop at its circle; walk back, free */
    psm_state_default(&st);
    st.x = -2.6f; st.z = 0.0f;                                               /* pole 0 is at (-2.6, -4) */
    for (i = 0; i < 100u; i++) in_(&st, &lim, PSM_BTN_Y, 0, 0, 0, 0, 0.05f);
    CHECK(fabsf(st.z - (-4.0f + lim.circle[0][2])) < 0.01f && fabsf(st.x + 2.6f) < 1e-3f);
    in_(&st, &lim, 0u, 0, 0, 0, 0, 0.016f);
    in_(&st, &lim, PSM_BTN_A, 0, 0, 0, 0, 0.5f);
    CHECK(st.z > -4.0f + lim.circle[0][2] + 1.5f);
    st.x = -2.6f; st.z = -4.0f;                                              /* on the very centre: out, backwards */
    psm_walk_limit(&st.x, &st.z, 0.0f, 1.0f, &lim);
    CHECK(fabsf(st.z - (-4.0f + lim.circle[0][2])) < 0.01f);

    /* a random walk: never out of the area, never inside a circle */
    psm_state_default(&st);
    for (i = 0; i < 20000u; i++) {
        unsigned held;
        float sx;

        seed = seed * 1664525u + 1013904223u;
        held = (seed >> 8) & (PSM_BTN_Y | PSM_BTN_A | PSM_BTN_X | PSM_BTN_B);
        sx = (float)((int)((seed >> 20) & 255u) - 128) * (1.0f / 128.0f);
        in_(&st, &lim, held, sx, 0, 0, 0, 0.05f);
        out += st.x < lim.x0 || st.x > lim.x1 || st.z < lim.z0 || st.z > lim.z1;
        for (k = 0; k < lim.n; k++) {
            const float dx = st.x - lim.circle[k][0], dz = st.z - lim.circle[k][1];

            in += dx * dx + dz * dz < (lim.circle[k][2] - 1e-3f) * (lim.circle[k][2] - 1e-3f);
        }
    }
    /* every point of the area, put through the limit: outside every circle (the overlapping cow and pole settle) */
    for (gx = lim.x0; gx <= lim.x1; gx += 0.1f)
        for (gz = lim.z0; gz <= lim.z1; gz += 0.1f) {
            float x = gx, z = gz;

            psm_walk_limit(&x, &z, 0.0f, 1.0f, &lim);
            for (k = 0; k < lim.n; k++) {
                const float dx = x - lim.circle[k][0], dz = z - lim.circle[k][1];

                pocket += dx * dx + dz * dz < (lim.circle[k][2] - 1e-3f) * (lim.circle[k][2] - 1e-3f);
            }
        }
    printf("  walk: random walk of 20000 steps, %u out of the area, %u in a circle; area scan, %u left in a circle\n", out, in, pocket);
    CHECK(out == 0u && in == 0u && pocket == 0u);
}

/* the grid's centre on its lattice: still while the eye is in a cell, one whole step when it crosses one, the same by any path */
static void grid_recentre(void) {
    static float keep[PSM_GV_MAX][3];
    static psm_frame_t fresh;
    psm_state_t st;
    unsigned i, gen0, gens, bad = 0, same_bad = 0;
    float x;

    psm_state_default(&st);
    st.yaw = 0.7f;
    st.x = 0.06f; st.z = -0.04f;
    psm_frame_setup(&frame, &scene, &st);
    gen0 = frame.gpos_gen;
    memcpy(keep, frame.gpos, sizeof keep);
    CHECK(frame.gc[0] == 0.0f && frame.gc[1] == 0.0f);
    st.x = 0.11f; st.z = 0.1f;                                               /* the same cell */
    psm_frame_setup(&frame, &scene, &st);
    CHECK(frame.gpos_gen == gen0 && memcmp(keep, frame.gpos, frame.gvn * sizeof keep[0]) == 0);
    st.x = 0.13f;                                                            /* across a lattice line: one step in x */
    psm_frame_setup(&frame, &scene, &st);
    CHECK(frame.gpos_gen == gen0 + 1u && frame.gc[0] == PSM_SNAP);
    for (i = 0; i < frame.gvn; i++)
        bad += fabsf(frame.gpos[i][0] - (keep[i][0] + PSM_SNAP)) > 1e-4f || frame.gpos[i][2] != keep[i][2];
    CHECK(bad == 0u);
    memset(&fresh, 0, sizeof fresh);                                         /* any path: a new frame at the same eye */
    psm_frame_setup(&fresh, &scene, &st);
    CHECK(memcmp(fresh.gpos, frame.gpos, frame.gvn * sizeof keep[0]) == 0);

    /* a walk of 2 m in 1 cm steps: the grid moves 8 times (at 0.125, 0.375, ..), and not between */
    st.x = 0.0f; st.z = 0.0f;
    psm_frame_setup(&frame, &scene, &st);
    gens = frame.gpos_gen;
    memcpy(keep, frame.gpos, sizeof keep);
    for (x = 0.01f; x < 2.0f; x += 0.01f) {
        const unsigned g = frame.gpos_gen;

        st.z = -x;
        psm_frame_setup(&frame, &scene, &st);
        if (frame.gpos_gen == g) same_bad += memcmp(keep, frame.gpos, frame.gvn * sizeof keep[0]) != 0;
        else memcpy(keep, frame.gpos, sizeof keep);
    }
    printf("  grid: 2 m walk in 1 cm steps, %u recentres (lattice %.2f m)\n", frame.gpos_gen - gens, (double)PSM_SNAP);
    CHECK(frame.gpos_gen - gens == 8u && same_bad == 0u);
}

/* the ground under the screen at any pitch and any eye offset in a lattice cell: its rings reach past the screen's sides, the
   screen's lower corners and middle land on the window inside its first ring, and looking level or down every window vertex
   is in front */
static void walk_window(void) {
    static const float pitch[6] = { -0.6f, -0.2f, 0.0f, PSM_PITCH, 0.2f, PSM_PITCH_DOWN };
    static const float off[5][2] = { { 0.0f, 0.0f }, { 0.124f, 0.124f }, { -0.124f, 0.124f }, { 0.124f, -0.124f }, { -0.124f, -0.124f } };
    psm_state_t st;
    float rows[16];
    unsigned p, o, step, edge_bad = 0, front_bad = 0, foot_bad = 0, widest = 0;

    for (p = 0; p < 6u; p++)
        for (o = 0; o < 5u; o++)
            for (step = 0; step < 24u; step++) {
                unsigned i, r, k;

                psm_state_default(&st);
                st.pitch = pitch[p];
                st.x = 3.0f + off[o][0]; st.z = -6.0f + off[o][1];
                st.yaw = (float)step * 0.2618f + 0.01f;
                psm_frame_setup(&frame, &scene, &st);
                if (frame.gcols > widest) widest = frame.gcols;
                if (!sh4der_psm_cam_rows(&frame.cam, PSM_NEAR, rows)) { CHECK(0); continue; }
                if (st.pitch >= 0.0f)
                    for (i = 0; i < frame.gvn; i++) {
                        float h[4];

                        sh4der_psm_xform(rows, frame.gpos[i], 1.0f, h);
                        if (!(h[3] > PSM_NEAR)) { front_bad++; break; }
                    }
                for (r = 0; r < frame.grows; r++) {
                    float l[4], rt[4];

                    sh4der_psm_xform(rows, frame.gpos[r * frame.gcols], 1.0f, l);
                    sh4der_psm_xform(rows, frame.gpos[r * frame.gcols + frame.gcols - 1u], 1.0f, rt);
                    if (fabsf(l[1]) > l[3] && fabsf(rt[1]) > rt[3]) continue;
                    if (!(l[0] < -l[3] && rt[0] > rt[3])) { edge_bad++; break; }
                }
                for (k = 0; k < 3u; k++) {                                  /* the screen's lower left, middle and right on the ground */
                    const float sx = ((float)k - 1.0f) * PSM_TAN_HALF * PSM_ASPECT, sy = -PSM_TAN_HALF;
                    float d[3], t, gp[2], dx, dz, ang, rel;

                    for (i = 0; i < 3u; i++) {
                        const float right = i == 0 ? frame.cam.up[1] * frame.cam.fwd[2] - frame.cam.up[2] * frame.cam.fwd[1]
                                          : i == 1 ? frame.cam.up[2] * frame.cam.fwd[0] - frame.cam.up[0] * frame.cam.fwd[2]
                                                   : frame.cam.up[0] * frame.cam.fwd[1] - frame.cam.up[1] * frame.cam.fwd[0];
                        d[i] = frame.cam.fwd[i] + sy * frame.cam.up[i] - sx * right;   /* up x fwd is the left: minus is the right */
                    }
                    if (!(d[1] < -1e-3f)) continue;                         /* that ray does not reach the ground */
                    t = PSM_EYE_H / -d[1];
                    gp[0] = st.x + d[0] * t; gp[1] = st.z + d[2] * t;
                    dx = gp[0] - frame.gc[0]; dz = gp[1] - frame.gc[1];
                    if (dx * dx + dz * dz < scene.row_r[0] * scene.row_r[0]) { foot_bad++; continue; }
                    ang = atan2f(dx, -dz);                                  /* a spoke k is at angle k x step: the turn nearest the heading */
                    while (ang < st.yaw - 3.1415927f) ang += 6.2831853f;
                    while (ang > st.yaw + 3.1415927f) ang -= 6.2831853f;
                    rel = ang / PSM_COL_STEP - (float)frame.gpos_k0;
                    if (!(rel >= 0.0f && rel <= (float)(frame.gcols - 1u))) foot_bad++;
                }
            }
    printf("  window: 6 pitches x 5 eye offsets x 24 headings, widest %u spokes; edges short %u, behind %u, corners off the grid %u\n",
           widest, edge_bad, front_bad, foot_bad);
    CHECK(edge_bad == 0u && front_bad == 0u && foot_bad == 0u && widest <= PSM_COLS + 1u);
}

/* both routes and the head-on fallback away from the origin */
static void walk_routes(void) {
    static const float at[3][3] = { { 4.0f, -20.0f, 4.2f }, { -10.0f, -40.0f, 2.68f }, { 12.0f, 4.0f, 5.0f } };
    psm_state_t st;
    unsigned q, step, head_diff = 0;

    for (q = 0; q < 3u; q++) {
        unsigned i, dark = 0, bad = 0;
        float rows[16];

        psm_state_default(&st);
        st.route = PSM_ROUTE_B; st.warp = 1;
        st.x = at[q][0]; st.z = at[q][1]; st.yaw = at[q][2];
        psm_frame_setup(&frame, &scene, &st);
        CHECK(frame.built);
        CHECK(sh4der_psm_cam_rows(&frame.cam, PSM_NEAR, rows));
        if (psm_frame_b_begin(&frame, &scene, depth)) {
            psm_frame_b_casters(&frame, &scene, 0);
            psm_frame_b_receivers(&frame, &scene, &st);
        } else CHECK(0);
        for (i = 0; i < frame.gvn; i++) dark += frame.glit[i] < 128u;
        psm_frame_a_ground(&frame);
        for (i = 0; i < frame.gvn; i++) {                                   /* route A: every vertex on screen past the fit's near plane has coordinates */
            float h[4];
            const float dx = frame.gpos[i][0] - st.x, dz = frame.gpos[i][2] - st.z;

            sh4der_psm_xform(rows, frame.gpos[i], 1.0f, h);
            if (!(fabsf(h[0]) < h[3] && fabsf(h[1]) < h[3] && h[2] > 0.0f && h[2] < h[3])) continue;
            if (dx * dx + dz * dz < 2.4f * 2.4f) continue;
            bad += !frame.gok[i] || frame.guv[i][0] < -0.01f || frame.guv[i][0] > 1.01f || frame.guv[i][1] < -0.01f || frame.guv[i][1] > 1.01f;
        }
        printf("  walked to (%.0f, %.0f) heading %.1f: kind %s, ground shadowed %u of %u, route A lookups off the map %u\n",
               (double)st.x, (double)st.z, (double)st.yaw, psm_kind_name(frame.map.kind), dark, frame.gvn, bad);
        CHECK(dark > 0u && dark < frame.gvn && bad == 0u);
    }
    /* the head-on fallback goes with the heading, not with the place: a directional sun */
    for (step = 0; step < 36u; step++) {
        int h0;

        psm_state_default(&st);
        st.warp = 1; st.az = 0.0f; st.el = 20.0f;
        st.yaw = (float)step * 0.1745f;
        psm_frame_setup(&frame, &scene, &st);
        h0 = frame.head_on;
        st.x = -7.0f; st.z = -30.0f;
        psm_frame_setup(&frame, &scene, &st);
        head_diff += frame.head_on != h0;
        CHECK(frame.built);
    }
    CHECK(head_diff == 0u);
}

/* a model triangle across the near plane: cut, not dropped */
static void near_cut(void) {
    psm_state_t st;
    psm_cv_t cv[4];
    unsigned i, k, crossing = 0, sent = 0, bad = 0;

    CHECK(psm_mix_argb_(0x00000000u, 0xffffffffu, 0.5f) == 0x80808080u);
    CHECK(psm_mix_argb_(0xff102030u, 0xff102030u, 0.3f) == 0xff102030u);
    psm_state_default(&st);
    st.x = -2.6f + 0.75f; st.z = -4.0f;                                      /* beside pole 0 (inside its circle: placed, not walked) */
    st.yaw = 4.712389f;                                                      /* facing -x, at it */
    st.pitch = PSM_PITCH_DOWN;
    psm_frame_setup(&frame, &scene, &st);
    psm_frame_unshadowed(&frame, &scene);
    psm_place(&frame, &scene);
    for (i = 0; i < scene.mtn; i++) {
        unsigned n;

        if (frame.mtri[i] != 2u) continue;
        crossing++;
        n = psm_clip_near(&frame, &scene, i, cv);
        CHECK(n == 0u || n == 3u || n == 4u);
        sent += n != 0u;
        for (k = 0; k < n; k++) {
            const uint32_t c0 = frame.mcol[scene.mt[i][0]], c1 = frame.mcol[scene.mt[i][1]], c2 = frame.mcol[scene.mt[i][2]];
            unsigned sh;

            bad += !(cv[k].iw > 0.0f && cv[k].iw <= 1.0f / PSM_NEAR) || !sh4der_finite_(cv[k].x) || !sh4der_finite_(cv[k].y);
            for (sh = 0; sh < 32u; sh += 8u) {                              /* a cut colour lies between the corners' */
                const unsigned a = (c0 >> sh) & 255u, b = (c1 >> sh) & 255u, c = (c2 >> sh) & 255u, g = (cv[k].argb >> sh) & 255u;
                const unsigned lo = a < b ? (a < c ? a : c) : (b < c ? b : c), hi = a > b ? (a > c ? a : c) : (b > c ? b : c);

                bad += g + 1u < lo || g > hi + 1u;
            }
        }
    }
    printf("  near cut beside a pole: %u triangles cross the near plane, %u sent cut, %u bad vertices\n", crossing, sent, bad);
    CHECK(crossing > 0u && sent > 0u && bad == 0u);
}

/* the strips psm_dc.c sends (ground and layer: two vertices a spoke a run), the models, the text: the worst frame */
static unsigned ground_verts(const psm_frame_t *f, int layer) {
    unsigned r, c, n = 0;

    for (r = 0; r + 1u < f->grows; r++) {
        c = 0;
        while (c + 1u < f->gcols) {
            unsigned end;

            if (!psm_cell_ok(f, r, c, layer)) { c++; continue; }
            end = psm_run_end(f, r, c, layer);
            n += 2u * (end - c + 1u);
            c = end;
        }
    }
    return n;
}

static void budget(void) {
    static const float pitch[6] = { -1.25f, -0.6f, -0.2f, 0.0f, PSM_PITCH, PSM_PITCH_DOWN };
    static const float at[3][2] = { { 0.0f, 0.0f }, { -1.2f, -10.5f }, { 2.0f, -30.0f } };
    psm_state_t st;
    psm_cv_t cv[4];
    unsigned p, q, step, worst = 0, worst_g = 0;

    for (p = 0; p < 6u; p++)
        for (q = 0; q < 3u; q++)
            for (step = 0; step < 12u; step++) {
                unsigned i, n;

                psm_state_default(&st);
                st.warp = 1; st.pitch = pitch[p];
                st.x = at[q][0]; st.z = at[q][1];
                st.yaw = (float)step * 0.5236f;
                psm_frame_setup(&frame, &scene, &st);
                psm_frame_unshadowed(&frame, &scene);
                psm_frame_a_ground(&frame);
                psm_place(&frame, &scene);
                n = ground_verts(&frame, 0) + ground_verts(&frame, 1);
                if (n > worst_g) worst_g = n;
                for (i = 0; i < scene.mtn; i++) n += frame.mtri[i] == 1u ? 3u : frame.mtri[i] == 2u ? psm_clip_near(&frame, &scene, i, cv) : 0u;
                n += 4u * (2u * PSM_HUD_LINES + 1u);                        /* the text quads, their dark strips, the inset */
                if (n > worst) worst = n;
            }
    printf("  vertices a frame, worst of 216 views: %u (ground and layer %u) of %u (%u KB at 32 B)\n", worst, worst_g, PSM_VTX_BUDGET, worst * 32u / 1024u);
    CHECK(worst < PSM_VTX_BUDGET);
}

/* CASTERS: all against movers only, both routes */
static void casters(void) {
    static uint16_t all[256 * 256], ref[256 * 256], movers[256 * 256], stat[256 * 256];
    psm_state_t st;
    unsigned i, nmov = 0, given = 0, b_given, stat_px = 0;
    size_t bytes;

    for (i = 0; i < scene.mtn; i++) nmov += scene.mtmov[i] != 0u;
    CHECK(nmov == scene.mtn - 120u);                                        /* the ten poles' 120 are static, every cow triangle moves */
    for (i = 0; i < 120u; i++) CHECK(!scene.mtmov[i] && scene.mkind[scene.mt[i][0]] == 0u);

    psm_state_default(&st);
    st.route = PSM_ROUTE_B; st.warp = 1;
    psm_frame_setup(&frame, &scene, &st);
    bytes = (size_t)frame.size * frame.size * sizeof all[0];
    CHECK(psm_frame_b_begin(&frame, &scene, all));
    psm_frame_b_casters(&frame, &scene, 0);                                 /* ALL */
    CHECK(frame.tris == scene.mtn);
    CHECK(psm_frame_b_begin(&frame, &scene, ref));                          /* the reference: every triangle, by hand */
    for (i = 0; i < scene.mtn; i++) sh4der_psm_raster_tri_h(&frame.raster, frame.mh[scene.mt[i][0]], frame.mh[scene.mt[i][1]], frame.mh[scene.mt[i][2]]);
    CHECK(memcmp(all, ref, bytes) == 0);                                    /* ALL is the whole set, byte for byte */

    CHECK(psm_frame_b_begin(&frame, &scene, movers));
    psm_frame_b_casters(&frame, &scene, 1);                                 /* MOVERS ONLY */
    b_given = frame.tris;
    CHECK(psm_frame_b_begin(&frame, &scene, ref));
    for (i = 0; i < scene.mtn; i++) if (scene.mtmov[i]) sh4der_psm_raster_tri_h(&frame.raster, frame.mh[scene.mt[i][0]], frame.mh[scene.mt[i][1]], frame.mh[scene.mt[i][2]]);
    CHECK(b_given == nmov && memcmp(movers, ref, bytes) == 0);
    CHECK(psm_frame_b_begin(&frame, &scene, stat));                         /* the static set alone covers texels, none of them in movers only */
    for (i = 0; i < scene.mtn; i++) if (!scene.mtmov[i]) sh4der_psm_raster_tri_h(&frame.raster, frame.mh[scene.mt[i][0]], frame.mh[scene.mt[i][1]], frame.mh[scene.mt[i][2]]);
    for (i = 0; i < frame.size * frame.size; i++) {
        stat_px += stat[i] != 0u;
        if (stat[i] && !all[i]) CHECK(0);
    }
    CHECK(stat_px > 0u && memcmp(all, movers, bytes) != 0);

    /* route B's ground: the second pole's shadow is there with all, gone with movers only */
    {
        float t, sx, sz;
        unsigned v;

        psm_frame_b_begin(&frame, &scene, depth);
        psm_frame_b_casters(&frame, &scene, 0);
        psm_frame_b_receivers(&frame, &scene, &st);
        t = 2.0f / frame.sun[1];
        sx = 2.6f - frame.sun[0] * t; sz = -8.4f - frame.sun[2] * t;
        v = nearest(sx, sz);
        if (fabsf(frame.gpos[v][0] - sx) < 0.06f && fabsf(frame.gpos[v][2] - sz) < 0.06f) {
            CHECK(frame.glit[v] < 128u);
            psm_frame_b_begin(&frame, &scene, depth);
            psm_frame_b_casters(&frame, &scene, 1);
            psm_frame_b_receivers(&frame, &scene, &st);
            CHECK(frame.glit[v] == 255u);
        }
    }

    /* route A: the program's selection (psm_caster_on, as map_pass): movers only gives the movers, every one, and no static */
    for (i = 0; i < scene.mtn; i++) {
        CHECK(psm_caster_on(&scene, i, 0));
        if (psm_caster_on(&scene, i, 1)) { given++; CHECK(scene.mtmov[i]); }
    }
    CHECK(given == nmov);
    printf("  casters: all %u triangles (map byte for byte), movers only %u (no static texel), static alone covers %u texels\n", scene.mtn, nmov, stat_px);
}

static void readout(void) {
    char ln[PSM_HUD_LINES][PSM_HUD_COLS + 1u];
    psm_state_t st;
    psm_cost_t cost;
    unsigned i;

    psm_state_default(&st);
    psm_frame_setup(&frame, &scene, &st);
    memset(&cost, 0, sizeof cost);
    psm_hud(&st, &frame, &cost, ln);
    CHECK(strstr(ln[0], "ROUTE A") && strstr(ln[0], "UNIFORM"));
    CHECK(strstr(ln[1], "TEXEL/PX") && !strstr(ln[1], "--"));
    CHECK(strstr(ln[2], "256x256") && strstr(ln[2], "128 KB VRAM"));
    CHECK(strstr(ln[3], "self-shadow no") && strstr(ln[3], "CASTERS ALL"));
    CHECK(!strcmp(ln[6], "SH-4 -- ms  PVR -- ms  TA -- v  VRAM -- KB  FPS --"));   /* nothing measured: nothing shown */
    CHECK(strstr(ln[7], "--"));
    cost.have = 1; cost.sh4_ms = 12.44f; cost.pvr_ms = 9.06f; cost.ta_v = 6840u; cost.vram_kb = 128u; cost.fps = 29.7f;
    st.route = PSM_ROUTE_B; st.warp = 1;
    psm_frame_setup(&frame, &scene, &st);
    psm_hud(&st, &frame, &cost, ln);
    CHECK(strstr(ln[0], "ROUTE B") && strstr(ln[0], "MAP PSM"));
    CHECK(!strcmp(ln[6], "SH-4 12.4 ms  PVR 9.1 ms  TA 6840 v  VRAM 128 KB  FPS 30"));
    CHECK(strstr(ln[3], "POINT_") && strstr(ln[3], "self-shadow yes"));
    CHECK(strstr(ln[4], "CAM x 0.0 z 0.0") && strstr(ln[4], "pitch -7"));
    CHECK(strstr(ln[2], "text 256 KB VRAM"));
    CHECK(strstr(ln[8], "ROUTE: ") && strstr(ln[9], "YAXB walk"));
    for (i = 0; i < PSM_HUD_LINES; i++) { CHECK(strlen(ln[i]) <= PSM_HUD_COLS); printf("  | %s\n", ln[i]); }
}

/* ---- placement, the caster and lookup transforms, the caches (psm_place, psm_xf, psm_pvr_tris_h) ------ */

static int close_(float a, float b, float rel) { return fabsf(a - b) <= rel * (1.0f + fabsf(b)); }

/* clip (X, Y, W) of a point by the plain row form, and its cell's two strip triangles: the reference for psm_place */
static void clip_ref(const float rows[16], const float p[3], float c[3]) {
    float h[4];

    sh4der_psm_xform(rows, p, 1.0f, h);
    c[0] = h[0]; c[1] = h[1]; c[2] = h[3];
}

static int cell_ref(const float rows[16], const psm_frame_t *f, unsigned r, unsigned c) {
    const unsigned a = r * f->gcols + c, b = a + 1u, d = a + f->gcols, e = d + 1u;
    float ca[3], cb[3], cd[3], ce[3];

    clip_ref(rows, f->gpos[a], ca); clip_ref(rows, f->gpos[b], cb); clip_ref(rows, f->gpos[d], cd); clip_ref(rows, f->gpos[e], ce);
    if (!(ca[2] > PSM_NEAR && cb[2] > PSM_NEAR && cd[2] > PSM_NEAR && ce[2] > PSM_NEAR)) return 0;
    if ((ca[0] < -ca[2] && cb[0] < -cb[2] && cd[0] < -cd[2] && ce[0] < -ce[2]) || (ca[0] > ca[2] && cb[0] > cb[2] && cd[0] > cd[2] && ce[0] > ce[2])
        || (ca[1] < -ca[2] && cb[1] < -cb[2] && cd[1] < -cd[2] && ce[1] < -ce[2]) || (ca[1] > ca[2] && cb[1] > cb[2] && cd[1] > cd[2] && ce[1] > ce[2]))
        return 0;                                                          /* wholly off one side of the screen */
    return sh4der_tri_area_ok(cd, ca, ce, PSM_NEAR, SH4DER_GUARD_MIN_AREA2) && sh4der_tri_area_ok(ca, ce, cb, PSM_NEAR, SH4DER_GUARD_MIN_AREA2);
}

static void placement(void) {
    static float ref_h[PSM_MV_MAX][4];
    psm_state_t st;
    float rows[16] = { 0.0f };
    unsigned step, i, r, c, clip_bad = 0, scr_bad = 0, cell_bad = 0, run_bad = 0, covered = 0, flagged = 0, tri_bad = 0, col_bad = 0, guv_bad = 0;

    for (step = 0; step < 6u; step++) {
        psm_state_default(&st);
        st.route = PSM_ROUTE_B; st.warp = 1;
        st.yaw = 0.9f * (float)step;
        psm_frame_setup(&frame, &scene, &st);
        CHECK(sh4der_psm_cam_rows(&frame.cam, PSM_NEAR, rows));
        CHECK(memcmp(rows, frame.rows, sizeof rows) == 0);
        if (psm_frame_b_begin(&frame, &scene, depth)) {
            psm_frame_b_casters(&frame, &scene, 0);
            psm_frame_b_receivers(&frame, &scene, &st);
            sh4der_psm_raster_xform_n(&frame.raster, &scene.mv[0][0], 3u * sizeof(float), scene.mvn, ref_h);
            for (i = 0; i < scene.mvn; i++)                             /* the route B caster transform: (X, Y, K, W) */
                for (r = 0; r < 4u; r++) if (!close_(frame.mh[i][r], ref_h[i][r], 1e-4f)) clip_bad++;
        } else {
            psm_frame_unshadowed(&frame, &scene);
        }
        psm_frame_a_ground(&frame);
        psm_place(&frame, &scene);
        for (i = 0; i < frame.gvn; i++) {                               /* the lookup: the plain map's rule */
            float u, v, w;
            int ok = sh4der_psm_map(&frame.map, frame.gpos[i], &u, &v, &w) && w > 0.0f;

            if (ok != (int)frame.gok[i]) guv_bad++;
            else if (ok && (!close_(frame.guv[i][0], 0.5f * u + 0.5f, 1e-4f) || !close_(frame.guv[i][1], 0.5f * v + 0.5f, 1e-4f))) guv_bad++;
        }
        for (i = 0; i < frame.gvn; i++) {                               /* ground: clip and screen against the plain row form */
            float cl[3], h[4], iw;

            clip_ref(rows, frame.gpos[i], cl);
            if (!close_(frame.gclip[i][0], cl[0], 1e-4f) || !close_(frame.gclip[i][1], cl[1], 1e-4f) || !close_(frame.gclip[i][2], cl[2], 1e-4f)) clip_bad++;
            if (!(cl[2] > PSM_NEAR)) continue;
            sh4der_psm_xform(rows, frame.gpos[i], 1.0f, h);
            iw = 1.0f / h[3];
            if (!close_(frame.gscr[i][0], (h[0] * iw * 0.5f + 0.5f) * PSM_SCR_W, 1e-4f) || !close_(frame.gscr[i][1], (0.5f - h[1] * iw * 0.5f) * PSM_SCR_H, 1e-4f)
                || !close_(frame.gscr[i][2], iw, 1e-4f)) scr_bad++;
        }
        for (i = 0; i < scene.mvn; i++) {                               /* models: the same, and the model triangles */
            float cl[3], h[4], iw;

            clip_ref(rows, scene.mv[i], cl);
            if (!close_(frame.mclip[i][0], cl[0], 1e-4f) || !close_(frame.mclip[i][2], cl[2], 1e-4f)) clip_bad++;
            if (!(cl[2] > PSM_NEAR)) continue;
            sh4der_psm_xform(rows, scene.mv[i], 1.0f, h);
            iw = 1.0f / h[3];
            if (!close_(frame.mscr[i][0], (h[0] * iw * 0.5f + 0.5f) * PSM_SCR_W, 1e-4f)) scr_bad++;
        }
        for (i = 0; i < scene.mtn; i++) {
            const unsigned a = scene.mt[i][0], b = scene.mt[i][1], k = scene.mt[i][2];
            float ca[3], cb[3], ck[3];
            int want;

            clip_ref(rows, scene.mv[a], ca); clip_ref(rows, scene.mv[b], cb); clip_ref(rows, scene.mv[k], ck);
            want = ca[2] > PSM_NEAR && cb[2] > PSM_NEAR && ck[2] > PSM_NEAR ? sh4der_tri_area_ok(ca, cb, ck, PSM_NEAR, SH4DER_GUARD_MIN_AREA2)
                 : (ca[2] > PSM_NEAR || cb[2] > PSM_NEAR || ck[2] > PSM_NEAR) ? 2 : 0;
            if (want != (int)frame.mtri[i]) tri_bad++;
        }
        for (r = 0; r + 1u < frame.grows; r++)                          /* cells, and the runs the strips make of them */
            for (c = 0; c + 1u < frame.gcols; c++) {
                const int want = cell_ref(rows, &frame, r, c);

                if (want != (int)frame.cell[r * PSM_COLS + c]) cell_bad++;
                flagged += frame.cell[r * PSM_COLS + c] != 0u;
            }
        for (r = 0; r + 1u < frame.grows; r++) {
            c = 0;
            while (c + 1u < frame.gcols) {
                unsigned end, k;

                if (!psm_cell_ok(&frame, r, c, 0)) { c++; continue; }
                end = psm_run_end(&frame, r, c, 0);
                for (k = c; k < end; k++) { if (!psm_cell_ok(&frame, r, k, 0)) run_bad++; covered++; }
                if (end + 1u < frame.gcols && psm_cell_ok(&frame, r, end, 0)) run_bad++;
                c = end;
            }
        }
        for (i = 0; i < frame.gvn; i += 7u) {                           /* the ground colour: the sun's share of its shadow level */
            const float l = psm_light(frame.sun[1] * (float)frame.glit[i] * (1.0f / 255.0f));
            const uint32_t want = psm_pack_rgb(l, l, l), got = frame.gcol[i];
            const int dr = (int)((got >> 16) & 255u) - (int)((want >> 16) & 255u), dg = (int)((got >> 8) & 255u) - (int)((want >> 8) & 255u);
            const int db = (int)(got & 255u) - (int)(want & 255u);

            if (dr < -1 || dr > 1 || dg < -1 || dg > 1 || db < -1 || db > 1) col_bad++;
        }
    }
    printf("  placement over six headings: %u cells with area, %u model triangles, %u lookups\n", flagged, scene.mtn, frame.gvn);
    CHECK(clip_bad == 0u && scr_bad == 0u && cell_bad == 0u && run_bad == 0u && tri_bad == 0u && col_bad == 0u && guv_bad == 0u);
    CHECK(covered == flagged && flagged > 0u);
}

/* the caches: the positions are kept while the window stays on its spokes, and the placement follows the camera, which moves
   inside a spoke */
static void placement_cache(void) {
    psm_state_t st;
    unsigned mid, gen0;
    float before, after;

    psm_state_default(&st);
    st.yaw = 1.0f;                                                     /* spoke 34 is the centre one: 34 x 0.0295 = 1.003 */
    psm_frame_setup(&frame, &scene, &st);
    mid = (frame.grows / 2u) * frame.gcols + PSM_COLS / 2u;            /* a ring about the middle, the centre spoke */
    psm_frame_unshadowed(&frame, &scene);
    psm_place(&frame, &scene);
    gen0 = frame.gpos_gen;
    CHECK(frame.place_ok && frame.place_gen == gen0);

    frame.gclip[0][0] = 12345.0f;                                      /* a sentinel: a still camera must not redo the placement */
    psm_frame_setup(&frame, &scene, &st);
    CHECK(frame.gpos_gen == gen0);
    psm_place(&frame, &scene);
    CHECK(frame.gclip[0][0] == 12345.0f);

    st.yaw = 1.0f + 0.004f;                                            /* a fraction of a spoke: the same window, a moved camera */
    psm_frame_setup(&frame, &scene, &st);
    CHECK(frame.gpos_gen == gen0);
    before = frame.gscr[mid][0];
    psm_place(&frame, &scene);
    after = frame.gscr[mid][0];
    CHECK(frame.gclip[0][0] != 12345.0f);                              /* the placement followed the camera */
    CHECK(fabsf(after - before) > 0.5f && fabsf(after - before) < 5.0f);   /* about 0.004 rad at a focal length of ~415 px */
    CHECK(frame.place_gen == frame.gpos_gen && frame.place_gen == gen0);

    st.yaw = 1.0f + 3.0f * PSM_COL_STEP;                               /* three spokes on: the positions move, a new gen */
    psm_frame_setup(&frame, &scene, &st);
    CHECK(frame.gpos_gen == gen0 + 1u);
    psm_place(&frame, &scene);
    CHECK(frame.place_gen == gen0 + 1u);
}

/* route A: the casters through the homogeneous path (psm_pvr_tris_h) are the triangles of sh4der_psm_pvr_tris */
static void a_casters(void) {
    static sh4der_psm_pvr_vtx_t ref[SH4DER_PSM_PVR_MAX_TRIS][3], got[SH4DER_PSM_PVR_MAX_TRIS][3];
    psm_state_t st;
    unsigned w, i, j, q, bad = 0, tris = 0;

    for (w = 0; w < 2u; w++) {
        psm_state_default(&st);
        st.warp = (int)w;
        st.yaw = 0.4f;
        psm_frame_setup(&frame, &scene, &st);
        CHECK(frame.built);
        psm_frame_a_casters(&frame, &scene);
        for (i = 0; i < scene.mtn; i++) {
            const unsigned a = scene.mt[i][0], b = scene.mt[i][1], k = scene.mt[i][2];
            const unsigned n = sh4der_psm_pvr_tris(&frame.map, frame.size, scene.mv[a], scene.mv[b], scene.mv[k], ref);
            const unsigned m = psm_pvr_tris_h(frame.ma[a], frame.ma[b], frame.ma[k], frame.size, got);

            if (n != m) { bad++; continue; }
            tris += n;
            for (j = 0; j < n; j++)
                for (q = 0; q < 3u; q++)
                    if (!close_(got[j][q].x, ref[j][q].x, 1e-4f) || !close_(got[j][q].y, ref[j][q].y, 1e-4f) || !close_(got[j][q].invw, ref[j][q].invw, 1e-4f)) bad++;
        }
    }
    printf("  route A casters through the homogeneous path: %u triangles, %u mismatches\n", tris, bad);
    CHECK(bad == 0u && tris > 0u);
}

/* ---- hold to repeat, DRAW DISTANCE ---------------------------------------------------------------------------- */

static unsigned hold_(psm_repeat_t *r, float secs, float dt) {
    unsigned n = 0, i, frames = (unsigned)(secs / dt + 0.5f);

    for (i = 0; i <= frames; i++) n += psm_repeat_step(r, 1, i ? dt : 0.0f);   /* the press, then a frame every dt */
    return n;
}

static void repeat(void) {
    psm_repeat_t r;
    psm_state_t st;
    unsigned a60, a20, i;

    memset(&r, 0, sizeof r);
    CHECK(psm_repeat_step(&r, 1, 0.016f) == 1u);                            /* a tap: exactly one */
    CHECK(psm_repeat_step(&r, 0, 0.016f) == 0u);
    for (i = 0; i < 20u; i++) CHECK(psm_repeat_step(&r, 1, 0.016f) == (i == 0u ? 1u : 0u));   /* held under the 0.4 s delay: the press only */
    CHECK(psm_repeat_step(&r, 0, 0.016f) == 0u);                            /* a release starts it over */
    CHECK(psm_repeat_step(&r, 1, 0.016f) == 1u);
    psm_repeat_step(&r, 0, 0.0f);
    a60 = hold_(&r, 1.0f, 1.0f / 60.0f);
    psm_repeat_step(&r, 0, 0.0f);
    a20 = hold_(&r, 1.0f, 1.0f / 20.0f);
    psm_repeat_step(&r, 0, 0.0f);
    printf("  repeat: 1 s held gives %u steps at 60 fps, %u at 20 fps (2 s: 31, 3 s: 61, a 10 s frame: %u)\n", a60, a20, PSM_REPEAT_CAP);
    CHECK(a60 == 8u && a20 == 8u);                                          /* the press, then 0.6 s ramping from 8 a second: 7.3 due */
    psm_repeat_step(&r, 0, 0.0f);
    CHECK(hold_(&r, 2.0f, 1.0f / 60.0f) == 31u);                            /* 30.4 due by 2 s */
    psm_repeat_step(&r, 0, 0.0f);
    CHECK(hold_(&r, 3.0f, 1.0f / 60.0f) == 61u);                            /* then 30 a second */
    psm_repeat_step(&r, 0, 0.0f);
    CHECK(psm_repeat_step(&r, 1, 0.0f) == 1u);
    CHECK(psm_repeat_step(&r, 1, 10.0f) == PSM_REPEAT_CAP);                 /* a 10 s frame: capped, the rest dropped */
    CHECK(psm_repeat_step(&r, 1, 1.0f / 60.0f) <= 1u);

    /* the menu: a value row held repeats, a toggle row changes once; closed, the sun's azimuth and START + R repeat too */
    psm_state_default(&st);
    st.menu = 1; st.row = PSM_ROW_AZIMUTH;
    for (i = 0; i <= 60u; i++) in_(&st, NULL, PSM_BTN_RIGHT, 0, 0, 0, 0, i ? 1.0f / 60.0f : 0.0f);
    CHECK(st.az == 160.0f);                                                 /* 120 + 8 x 5 */
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
    for (i = 0; i <= 20u; i++) in_(&st, NULL, PSM_BTN_A, 0, 0, 0, 0, i ? 1.0f / 20.0f : 0.0f);   /* A is + in the menu, 20 fps */
    CHECK(st.az == 200.0f);
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
    st.row = PSM_ROW_ROUTE;
    for (i = 0; i <= 60u; i++) in_(&st, NULL, PSM_BTN_RIGHT, 0, 0, 0, 0, i ? 1.0f / 60.0f : 0.0f);
    CHECK(st.route == PSM_ROUTE_B);                                         /* once */
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
    st.row = PSM_ROW_WARP_NEAR;                                             /* the warp's near plane (was START + R) */
    for (i = 0; i <= 60u; i++) in_(&st, NULL, PSM_BTN_RIGHT, 0, 0, 0, 1.0f, i ? 1.0f / 60.0f : 0.0f);
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
    CHECK(st.warp_near == 8.0f && st.warp == 0 && st.el == 35.0f);         /* 4 + 8 x 0.5, the trigger open: the sun still */
    st.menu = 0;
    for (i = 0; i <= 60u; i++) in_(&st, NULL, PSM_BTN_LEFT, 0, 0, 0, 0, i ? 1.0f / 60.0f : 0.0f);
    CHECK(st.az == 160.0f && st.x == 0.0f && st.z == 0.0f);
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
}

/* the settings panel: the groups, the tabs, the rows' window, the lines */
static void panel(void) {
    char ln[PSM_HUD_LINES][PSM_HUD_COLS + 1u], v[48];
    int rows[PSM_ROWS], seen[PSM_ROWS];
    psm_panel_t p;
    psm_state_t st;
    unsigned g, i, n, total = 0, longest = 0;
    int r;

    memset(seen, 0, sizeof seen);
    for (g = 0; g < PSM_GRPS; g++) {                                        /* every row in one group, in table order */
        n = psm_grp_rows((int)g, rows);
        CHECK(n > 0u);
        for (i = 0; i < n; i++) { seen[rows[i]]++; CHECK(psm_grp_index((int)g, rows[i]) == i); CHECK(!i || rows[i] > rows[i - 1u]); }
        total += n;
        longest = n > longest ? n : longest;
    }
    CHECK(total == PSM_ROWS);
    for (r = 0; r < PSM_ROWS; r++) CHECK(seen[r] == 1);
    printf("  panel: %u rows in %u groups, the longest %u, %u shown at once\n", total, (unsigned)PSM_GRPS, longest, PSM_PANEL_ROWS);

    psm_state_default(&st);                                                 /* the tabs: R on, L back, round, the first row */
    in_(&st, NULL, PSM_BTN_START, 0, 0, 0, 0, 0.016f);
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
    in_(&st, NULL, PSM_BTN_DOWN, 0, 0, 0, 0, 0.016f);
    in_(&st, NULL, PSM_BTN_R, 0, 0, 0, 1.0f, 0.016f);
    psm_grp_rows(PSM_GRP_SUN, rows);
    CHECK(st.grp == PSM_GRP_SUN && st.row == rows[0] && st.el == 35.0f);
    in_(&st, NULL, PSM_BTN_R, 0, 0, 0, 1.0f, 0.5f);                          /* held: once */
    CHECK(st.grp == PSM_GRP_SUN);
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
    in_(&st, NULL, PSM_BTN_L, 0, 0, 1.0f, 0, 0.016f);
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
    in_(&st, NULL, PSM_BTN_L, 0, 0, 1.0f, 0, 0.016f);
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
    psm_grp_rows(PSM_GRP_VIEW, rows);
    CHECK(st.grp == PSM_GRP_VIEW && st.row == rows[0] && st.el == 35.0f);   /* L past the first tab: the last */

    psm_state_default(&st);                                                 /* up / down held repeat, round in the group */
    st.menu = 1; st.grp = PSM_GRP_SHADOW; st.row = PSM_ROW_ROUTE;
    n = psm_grp_rows(PSM_GRP_SHADOW, rows);
    for (i = 0; i <= 60u; i++) in_(&st, NULL, PSM_BTN_DOWN, 0, 0, 0, 0, i ? 1.0f / 60.0f : 0.0f);
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
    CHECK(st.row == rows[8u % n]);                                          /* the press, 400 ms, then every 140 ms: 8 steps */
    CHECK(st.route == PSM_ROUTE_A && st.warp == 0 && st.menu);              /* nothing changed by the moving */

    for (g = 0; g < PSM_GRPS; g++) {                                        /* each row of each group: shown, chosen, in width */
        n = psm_grp_rows((int)g, rows);
        for (i = 0; i < n; i++) {
            unsigned k, chosen = 0;

            psm_state_default(&st);
            st.menu = 1; st.grp = (int)g; st.row = rows[i];
            psm_panel_layout(&st, &p);
            CHECK(p.n == n && p.sel == i && p.sel >= p.top && p.sel < p.top + p.shown && p.shown <= PSM_PANEL_ROWS);
            CHECK(p.shown == (n < PSM_PANEL_ROWS ? n : PSM_PANEL_ROWS));
            psm_hud(&st, &frame, NULL, ln);
            psm_panel_lines(&st, &p, ln);
            for (k = 0; k < p.shown; k++) {
                CHECK(strlen(ln[1u + k]) == PSM_PANEL_COLS);
                CHECK(strstr(ln[1u + k], psm_row_name[p.rows[p.top + k]]) == ln[1u + k]);
                CHECK(!strstr(ln[1u + k], "..."));                          /* the boot's values fit whole */
                chosen += p.top + k == p.sel;
            }
            for (; k < PSM_PANEL_ROWS; k++) CHECK(ln[1u + k][0] == 0);
            CHECK(chosen == 1u);
            CHECK(strlen(ln[PSM_PANEL_LN_TITLE]) == PSM_PANEL_COLS && strstr(ln[PSM_PANEL_LN_TITLE], "SETTINGS") == ln[PSM_PANEL_LN_TITLE]);
            CHECK(strlen(ln[PSM_PANEL_LN_FOOT]) <= PSM_PANEL_COLS && strstr(ln[PSM_PANEL_LN_FOOT], "B CLOSE"));
            if (g == 0u && i == n - 1u)
                for (k = 0; k < PSM_HUD_LINES; k++) if (k == PSM_PANEL_LN_TITLE || k == PSM_PANEL_LN_TABS || (k >= 1u && k <= 9u)) printf("  | %s\n", ln[k]);
        }
    }
    for (g = 0; g < PSM_GRPS; g++) {                                        /* the tabs: in order, apart, on the line */
        CHECK(p.tab_c1[g] > p.tab_c0[g] && (!g || p.tab_c0[g] > p.tab_c1[g - 1u]) && p.tab_c1[g] <= PSM_PANEL_COLS);
        CHECK(!memcmp(ln[PSM_PANEL_LN_TABS] + p.tab_c0[g], psm_grp_name[g], p.tab_c1[g] - p.tab_c0[g]));
    }

    psm_state_default(&st);                                                 /* a slider at its end: that chevron left out */
    st.menu = 1; st.row = PSM_ROW_ELEVATION; st.grp = psm_row_def[st.row].group;
    st.el = 85.0f;
    psm_panel_layout(&st, &p);
    psm_panel_lines(&st, &p, ln);
    CHECK(ln[1u + p.sel - p.top][PSM_PANEL_COLS - 1u] == ' ' && strstr(ln[1u + p.sel - p.top], "< 85 deg"));
    st.el = 5.0f;
    psm_panel_lines(&st, &p, ln);
    CHECK(strstr(ln[1u + p.sel - p.top], "  5 deg >"));
    st.el = 40.0f;
    psm_panel_lines(&st, &p, ln);
    CHECK(strstr(ln[1u + p.sel - p.top], "< 40 deg >"));
    st.row = PSM_ROW_AZIMUTH; st.az = 0.0f;                                  /* a slider that goes round keeps both */
    psm_panel_layout(&st, &p);
    psm_panel_lines(&st, &p, ln);
    CHECK(strstr(ln[1u + p.sel - p.top], "< 0 deg >"));
    st.row = PSM_ROW_ROUTE; st.grp = PSM_GRP_SHADOW; st.route = PSM_ROUTES - 1;   /* AB reads as AB */
    psm_panel_layout(&st, &p);
    psm_panel_lines(&st, &p, ln);
    CHECK(strstr(ln[1u + p.sel - p.top], "< AB"));
    st.scene = 1; st.sun_free = 1; st.row = PSM_ROW_TIME; st.grp = psm_row_def[st.row].group;   /* the longest value: whole */
    psm_panel_layout(&st, &p);
    psm_panel_lines(&st, &p, ln);
    CHECK(strlen(ln[1u + p.sel - p.top]) == PSM_PANEL_COLS && strstr(ln[1u + p.sel - p.top], psm_row_value(&st, PSM_ROW_TIME, v, sizeof v)));
    psm_row_change(&st, PSM_ROW_FREEZE, 1);                                  /* FREEZE: a row of its own */
    CHECK(st.freeze);
    psm_row_change(&st, PSM_ROW_FREEZE, 1);
    CHECK(!st.freeze);
}

static void distance(void) {
    char ln[PSM_HUD_LINES][PSM_HUD_COLS + 1u], v[48];
    static uint16_t d_inf[256 * 256], d_cut[256 * 256];
    psm_state_t st;
    float rows[16];
    unsigned i, r, c, steps = 0, sent = 0, cut = 0, bad = 0, mcut = 0;

    psm_state_default(&st);
    CHECK(st.dist_i == PSM_DIST_INF && psm_dist(&st) == 0.0f);              /* the boot: INFINITE, as before */
    psm_row_change(&st, PSM_ROW_DIST, 1);
    CHECK(st.dist_i == PSM_DIST_INF);                                       /* the top stays */
    while (st.dist_i > 0u) { psm_row_change(&st, PSM_ROW_DIST, -1); steps++; }
    CHECK(steps == PSM_DIST_STEPS && psm_dist(&st) == 5.0f);
    psm_row_change(&st, PSM_ROW_DIST, -1);
    CHECK(psm_dist(&st) == 5.0f);
    for (i = 1; i < PSM_DIST_STEPS; i++) CHECK(psm_dist_m[i] > psm_dist_m[i - 1u]);
    CHECK(psm_dist_m[PSM_DIST_STEPS - 1u] == 500u && psm_dist_m[9] == 50u && psm_dist_m[15] == 200u);
    for (st.dist_i = 0; st.dist_i <= PSM_DIST_INF; st.dist_i++)
        CHECK(strlen(psm_row_name[PSM_ROW_DIST]) + 2u + strlen(psm_row_value(&st, PSM_ROW_DIST, v, sizeof v)) + strlen("   (D-pad row/change, B close)") <= PSM_HUD_COLS);

    /* a cut at 20 m: a cell or a model triangle wholly past it in view depth is not sent, the rest is as at INFINITE */
    psm_state_default(&st);
    st.yaw = 0.3f;
    psm_frame_setup(&frame, &scene, &st);
    psm_frame_unshadowed(&frame, &scene);
    psm_place(&frame, &scene);
    {
        static uint8_t cell_inf[PSM_ROWS_MAX * PSM_COLS], mtri_inf[PSM_MT_MAX];

        memcpy(cell_inf, frame.cell, sizeof cell_inf);
        memcpy(mtri_inf, frame.mtri, sizeof mtri_inf);
        st.dist_i = 3u;                                                     /* 20 m */
        frame.gclip[0][0] = 12345.0f;                                       /* the same camera: the distance alone redoes the placement */
        psm_frame_setup(&frame, &scene, &st);
        psm_place(&frame, &scene);
        CHECK(frame.gclip[0][0] != 12345.0f && frame.dist == 20.0f);
        CHECK(sh4der_psm_cam_rows(&frame.cam, PSM_NEAR, rows));
        for (r = 0; r + 1u < frame.grows; r++)
            for (c = 0; c + 1u < frame.gcols; c++) {
                const unsigned a = r * frame.gcols + c, b = a + 1u, d = a + frame.gcols, e = d + 1u, k = r * PSM_COLS + c;
                const int past = frame.gclip[a][2] > 20.0f && frame.gclip[b][2] > 20.0f && frame.gclip[d][2] > 20.0f && frame.gclip[e][2] > 20.0f;

                sent += frame.cell[k] != 0u;
                cut += cell_inf[k] && !frame.cell[k];
                bad += frame.cell[k] != (uint8_t)(past ? 0u : cell_inf[k]);
            }
        for (i = 0; i < scene.mtn; i++) {
            const int past = frame.mclip[scene.mt[i][0]][2] > 20.0f && frame.mclip[scene.mt[i][1]][2] > 20.0f && frame.mclip[scene.mt[i][2]][2] > 20.0f;

            mcut += mtri_inf[i] && !frame.mtri[i];
            bad += frame.mtri[i] != (past ? 0u : mtri_inf[i]);
        }
        printf("  draw distance 20 m: %u cells sent, %u cut; %u model triangles cut; %u wrong\n", sent, cut, mcut, bad);
        CHECK(bad == 0u && cut > 0u && sent > 0u && mcut > 0u);
        st.dist_i = PSM_DIST_INF;                                           /* back to INFINITE: the same flags as before, byte for byte */
        psm_frame_setup(&frame, &scene, &st);
        psm_place(&frame, &scene);
        CHECK(memcmp(cell_inf, frame.cell, sizeof cell_inf) == 0 && memcmp(mtri_inf, frame.mtri, scene.mtn) == 0);
    }

    /* the map's casters do not follow it: route B's map at 5 m is the map at INFINITE */
    psm_state_default(&st);
    st.route = PSM_ROUTE_B; st.warp = 1;
    psm_frame_setup(&frame, &scene, &st);
    CHECK(psm_frame_b_begin(&frame, &scene, d_inf));
    psm_frame_b_casters(&frame, &scene, 0);
    i = frame.tris;
    st.dist_i = 0u;
    psm_frame_setup(&frame, &scene, &st);
    CHECK(psm_frame_b_begin(&frame, &scene, d_cut));
    psm_frame_b_casters(&frame, &scene, 0);
    CHECK(frame.tris == i && memcmp(d_inf, d_cut, (size_t)frame.size * frame.size * sizeof d_inf[0]) == 0);

    psm_hud(&st, &frame, NULL, ln);
    CHECK(strstr(ln[10], "DIST 5 m") && strstr(ln[10], "map casters: all"));
    st.dist_i = PSM_DIST_INF;
    psm_hud(&st, &frame, NULL, ln);
    CHECK(strstr(ln[10], "DIST INF"));
    st.dist_i = PSM_DIST_STEPS - 1u;
    psm_hud(&st, &frame, NULL, ln);
    CHECK(strstr(ln[10], "DIST 500 m") && strstr(ln[10], "375 m") && strstr(ln[10], "all distances"));
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    scene_counts();
    window();
    fixed_vertices();
    route_b();
    route_a();
    sweep();
    placement();
    placement_cache();
    a_casters();
    pad();
    menu();
    panel();
    walk_limits();
    grid_recentre();
    walk_window();
    walk_routes();
    near_cut();
    budget();
    casters();
    repeat();
    distance();
    readout();
    if (fails) { fprintf(stderr, "psm_scene_test: %d failure(s)\n", fails); return 1; }
    printf("psm_scene_test: ok\n");
    return 0;
}
