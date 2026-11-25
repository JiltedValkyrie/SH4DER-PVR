/* SH4DER-PVR: the perspective shadow map demo's scene test

   test/psm_scene_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   Host check of demo/psm/psm_scene.h, the part of the Dreamcast A-vs-B program that is not a KallistiOS call: the
   rings (0.25 m deep near the camera, 2 m at 70 m), the models' counts; every ground vertex of the window in front
   of the camera and the window wider than the screen at any yaw; ground vertices that do not move when the camera
   turns; route B at the three presets and on a yaw sweep (a pole's shadow where it must be, a lit point lit, the
   models neither all dark nor all lit); route A (every vertex in view has coordinates, the casters reach the map,
   the lookup's error in the near cells); the head-on fallback; the pad; and the readout, which shows "--" until
   the console has measured.
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
    CHECK(scene.rows * (PSM_COLS + 1u) <= 5400u);                      /* the mock-up's guess: 3,000 to 5,000 in view */
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
            for (i = 0; i < p; i++) { psm_input(&st, PSM_BTN_START, 0.0f, 0.016f); psm_input(&st, PSM_BTN_START | PSM_BTN_Y, 0.0f, 0.016f); psm_input(&st, 0u, 0.0f, 0.016f); }
            psm_frame_setup(&frame, &scene, &st);
            CHECK(frame.built);
            if (w && frame.head_on) printf("  preset %u warp: head-on, the uniform map stands in\n", p);
            CHECK(psm_frame_b_begin(&frame, &scene, depth));
            psm_frame_b_casters(&frame, &scene);
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
        float worst_near = 0.0f, worst = 0.0f;

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
                if (e > worst) worst = e;
                if (scene.row_r[r] >= 2.3f && e > worst_near) worst_near = e;   /* the nearest ground the screen shows is at 2.3 m */
            }
        printf("  A %-7s map triangles %u, lookup error: %.3f texels on screen, %.3f in the rings under it\n", w ? "warp" : "uniform", tris, worst_near, worst);
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
            psm_frame_b_casters(&frame, &scene);
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

static void pad(void) {
    psm_state_t st;

    psm_state_default(&st);
    CHECK(st.route == PSM_ROUTE_A && st.warp == 0 && psm_map_sizes[st.size_i] == 256u && st.readout);   /* the boot state */
    psm_input(&st, PSM_BTN_A, 0.0f, 0.016f);
    CHECK(st.route == PSM_ROUTE_B);
    psm_input(&st, PSM_BTN_A, 0.0f, 0.016f);                               /* held: no repeat */
    CHECK(st.route == PSM_ROUTE_B);
    psm_input(&st, 0u, 0.0f, 0.016f);
    psm_input(&st, PSM_BTN_L, 0.0f, 0.016f);                               /* the warp near is ignored on uniform */
    CHECK(st.warp_near == 4.0f);
    psm_input(&st, PSM_BTN_X, 0.0f, 0.016f);
    CHECK(st.warp == 1);
    psm_input(&st, PSM_BTN_R, 0.0f, 0.016f);
    CHECK(st.warp_near == 4.5f);
    psm_input(&st, PSM_BTN_LEFT, 0.0f, 0.016f);
    CHECK(st.az == 115.0f);
    psm_input(&st, PSM_BTN_START, 0.0f, 0.016f);                           /* a tap of START: the readout */
    psm_input(&st, 0u, 0.0f, 0.016f);
    CHECK(!st.readout);
    psm_input(&st, PSM_BTN_START, 0.0f, 0.016f);                           /* START + A: the map size, and not the readout, not the route */
    psm_input(&st, PSM_BTN_START | PSM_BTN_A, 0.0f, 0.016f);
    psm_input(&st, 0u, 0.0f, 0.016f);
    CHECK(psm_map_sizes[st.size_i] == 64u && !st.readout && st.route == PSM_ROUTE_B);
    psm_input(&st, PSM_BTN_B, 0.5f, 0.1f);
    CHECK(st.split && fabsf(st.yaw - 0.06f) < 1e-5f);
    psm_input(&st, 0u, -1.0f, 1.0f);                                       /* the yaw wraps */
    CHECK(!st.split && st.yaw > 5.0f && st.yaw < 6.2832f);
    st.el = 85.0f;
    psm_input(&st, PSM_BTN_UP, 0.0f, 0.016f);
    CHECK(st.el == 85.0f);
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
    CHECK(strstr(ln[3], "self-shadow: no"));
    CHECK(!strcmp(ln[6], "SH-4 -- ms  PVR -- ms  TA -- v  VRAM -- KB  FPS --"));   /* nothing measured: nothing shown */
    CHECK(strstr(ln[7], "--"));
    cost.have = 1; cost.sh4_ms = 12.44f; cost.pvr_ms = 9.06f; cost.ta_v = 6840u; cost.vram_kb = 128u; cost.fps = 29.7f;
    st.route = PSM_ROUTE_B; st.warp = 1;
    psm_frame_setup(&frame, &scene, &st);
    psm_hud(&st, &frame, &cost, ln);
    CHECK(strstr(ln[0], "ROUTE B") && strstr(ln[0], "MAP PSM"));
    CHECK(!strcmp(ln[6], "SH-4 12.4 ms  PVR 9.1 ms  TA 6840 v  VRAM 128 KB  FPS 30"));
    CHECK(strstr(ln[3], "POINT_") && strstr(ln[3], "self-shadow: yes"));
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
            psm_frame_b_casters(&frame, &scene);
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
            want = ca[2] > PSM_NEAR && cb[2] > PSM_NEAR && ck[2] > PSM_NEAR && sh4der_tri_area_ok(ca, cb, ck, PSM_NEAR, SH4DER_GUARD_MIN_AREA2);
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

int main(void) {
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
    readout();
    if (fails) { fprintf(stderr, "psm_scene_test: %d failure(s)\n", fails); return 1; }
    printf("psm_scene_test: ok\n");
    return 0;
}
