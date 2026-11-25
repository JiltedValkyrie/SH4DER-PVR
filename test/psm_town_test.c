/* SH4DER-PVR: host checks of the perspective shadow map demo's town square

   test/psm_town_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   demo/psm/psm_town.h with the mesh tools/bake_town.py writes into the build folder (the kit's when it is fetched, stand-in
   boxes when not; the counts printed say which). The sun on the story's day against TOWN_STORY.md's table; the mesh (strips,
   casters in objects, movers); the cast's walks and the hens; the clock, the presets and the pad; the walk; the caster cull
   against the map's square (a culled object draws no texel: the map with the cull is the map without it, byte for byte; the
   tower out of view is kept and shades); the fit's slab by route; the receivers on the sent vertices only; the vertices and the
   TA's parameter bytes a frame through psm_dc.c's own walks (psm_send.h) against the buffer, and a sweep from the a3 crash's
   camera and over the walk area (strips, end flags, headers, coordinates, tile bins); the readout. Nothing here is a console measurement.
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../demo/psm/psm_scene.h"
#include "../demo/psm/psm_sky.h"
#include "../demo/psm/psm_send.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static psm_scene_t scene;
static psm_frame_t frame;
static uint16_t depth[256 * 256], depth2[256 * 256];

static void in_(psm_state_t *st, const psm_walk_limits_t *lim, unsigned held, float sx, float sy, float lt, float rt, float dt) {
    const psm_pad_t pad = { held, sx, sy, lt, rt };

    psm_input(st, lim, &pad, dt);
}

static float ang_(float a, float b) {
    float d = a - b;

    while (d > 180.0f) d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    return fabsf(d);
}

/* ---- the sun on the day: TOWN_STORY.md's table (rounded there to the degree and the tenth) ----------------------- */
static void sun(void) {
    static const float tab[8][3] = { { 9.0f, 131.0f, 20.5f }, { 12.0f, 180.0f, 31.9f }, { 13.5f, 206.0f, 28.8f }, { 14.0f, 214.0f, 26.5f },
                                     { 14.5f, 221.0f, 23.7f }, { 15.0f, 229.0f, 20.5f }, { 15.5f, 236.0f, 16.9f }, { 16.0f, 242.0f, 12.9f } };
    float worst_a = 0.0f, worst_e = 0.0f, az, el;
    unsigned i;

    for (i = 0; i < 8u; i++) {
        psm_sun_at(tab[i][0], &az, &el);
        if (ang_(az, tab[i][1]) > worst_a) worst_a = ang_(az, tab[i][1]);
        if (fabsf(el - tab[i][2]) > worst_e) worst_e = fabsf(el - tab[i][2]);
    }
    psm_sun_at(14.0f, &az, &el);
    printf("  sun: 14:00 az %.2f el %.2f; against the story's table worst %.2f deg azimuth, %.2f elevation\n", az, el, worst_a, worst_e);
    CHECK(worst_a < 0.6f && worst_e < 0.1f);
}

/* ---- the mesh ------------------------------------------------------------------------------------------------- */
static void mesh(void) {
    unsigned i, k, strips = 0, bad = 0, people = 0, hens = 0, mt_mov = 0, ct_mov = 0;

    psm_scene_init_town(&scene);
    for (i = 0; i < scene.mtn; i++) strips += scene.mstart[i];
    for (i = 0; i < scene.mtn; i++) mt_mov += scene.mtmov[i];
    for (i = 0; i < scene.ctn; i++) ct_mov += scene.ctmov[i];
    for (k = 0; k < scene.movern; k++) { people += scene.mover[k].kind == 0u; hens += scene.mover[k].kind == 1u; }
    printf("  town (kit %s): screen %u triangles in %u strips, %u vertices (%u moving: %u triangles); map %u triangles, %u vertices, %u objects;"
           " %u colours; %u people, %u hens\n", PSM_TOWN_KIT ? "yes" : "NO, stand-in boxes", scene.mtn, strips, scene.mvn, scene.mvn - scene.mdyn0,
           mt_mov, scene.ctn, scene.cvn, scene.con, scene.paln, people, hens);
    CHECK(people == PSM_CAST && hens == PSM_HENS && scene.movern == PSM_CAST + PSM_HENS);
    CHECK(scene.mvn <= PSM_MV_MAX && scene.mtn <= PSM_MT_MAX && scene.cvn <= PSM_CV_MAX && scene.ctn <= PSM_CT_MAX && scene.con <= PSM_CO_MAX);
    CHECK(scene.mtn == PSM_TOWN_T + PSM_CAST * PSM_TOWN_PERSON_T + PSM_HENS * PSM_TOWN_HEN_T);
    CHECK(scene.ctn == PSM_TOWN_CT + PSM_CAST * PSM_TOWN_PERSON_CT + PSM_HENS * PSM_TOWN_HEN_CT);
    CHECK(mt_mov == scene.mtn - scene.tdyn0 && ct_mov == scene.ctn - PSM_TOWN_CT);
    CHECK(scene.ctn >= 900u && scene.ctn <= 2500u);                          /* the map's budget: 1.5 to 2.5k; the town comes under */
    for (i = 0; i < scene.mtn; i++) {
        bad += scene.mt[i][0] >= scene.mvn || scene.mt[i][1] >= scene.mvn || scene.mt[i][2] >= scene.mvn;
        if (i && !scene.mstart[i]) bad += scene.mt[i][0] != scene.mt[i - 1u][1] || scene.mt[i][1] != scene.mt[i - 1u][2];   /* a strip */
    }
    CHECK(scene.mstart[0] && bad == 0u);
    for (i = 0; i < scene.mvn; i++) {
        bad += fabsf(sh4der_len3_(scene.mn[i][0], scene.mn[i][1], scene.mn[i][2]) - 1.0f) > 0.02f;
        bad += scene.mkind[i] >= scene.paln;
    }
    CHECK(bad == 0u);
    for (k = 0; k < scene.con; k++) {                                         /* an object's triangles use its own vertices */
        const psm_cobj_t *o = &scene.co[k];

        for (i = o->t0; i < o->t0 + o->tn; i++) {
            unsigned c;

            for (c = 0; c < 3u; c++) bad += scene.ct[i][c] < o->v0 || scene.ct[i][c] >= o->v0 + o->vn;
            bad += scene.ctmov[i] != o->mover;
        }
        bad += o->lo[0] > o->hi[0] || o->lo[1] > o->hi[1] || o->lo[2] > o->hi[2];
    }
    CHECK(bad == 0u);
    {
        const psm_cobj_t *t = &scene.co[PSM_TOWN_CO_TOWER];                  /* the tower at story (-27, -31), 28 m to its apex */

        CHECK(fabsf(0.5f * (t->lo[0] + t->hi[0]) + 27.0f) < 0.3f && fabsf(0.5f * (t->lo[2] + t->hi[2]) - 31.0f) < 0.3f);
        CHECK(fabsf(t->hi[1] - 28.0f) < 0.01f && t->lo[1] == 0.0f);
    }
}

/* ---- the cast and the hens -------------------------------------------------------------------------------------- */
static void cast(void) {
    static const float stall_z[4] = { 6.0f, 2.0f, -2.0f, -6.0f };
    float clear = 1e9f, fastest = 0.0f;
    unsigned w, bad = 0, i;

    for (w = 0; w < PSM_CAST; w++) {
        const float period = psm_cast_period(w);
        float t, px = 0.0f, pz = 0.0f, b;

        CHECK(period > 5.0f && period < 200.0f);
        for (t = 0.0f; t < period; t += 0.05f) {
            float x, z, d;

            psm_cast_at(w, t, &x, &z, &b);
            bad += !(x == x) || !(z == z) || !(b == b);
            if (t > 0.0f) {
                const float v = sh4der_sqrt_((x - px) * (x - px) + (z - pz) * (z - pz)) / 0.05f;

                if (v < 20.0f && v > fastest) fastest = v;          /* the porter's jump back to the arch is not a walk */
            }
            px = x; pz = z;
            if (z > -12.5f && z < 12.5f) bad += x < -11.5f || x > 11.7f;     /* in the square: off the walls */
            d = sh4der_sqrt_((x + 2.0f) * (x + 2.0f) + (z + 7.0f) * (z + 7.0f)) - 1.0f;                   /* the well */
            if (d < clear) clear = d;
            d = fmaxf(fabsf(x - 4.0f), fabsf(z)) - 1.0f;                                                  /* the cross's step */
            if (d < clear) clear = d;
            d = fmaxf(fabsf(x - 8.5f) - 0.75f, fabsf(z - 0.5f) - 1.5f);                                    /* the pen */
            if (d < clear) clear = d;
            for (i = 0; i < 4u; i++) {                                                                      /* the counters */
                d = fmaxf(fabsf(x - 9.85f) - 0.4f, fabsf(z - stall_z[i]) - 1.3f);
                if (d < clear) clear = d;
            }
        }
    }
    printf("  cast: 8 routes, fastest %.2f m/s, nearest a person's centre comes to the well, the cross, the pen or a counter %.2f m\n", fastest, clear);
    CHECK(bad == 0u && fastest <= PSM_CAST_RATE * 1.02f && clear > 0.25f);

    {                                                                         /* the hens: 2 minutes, then the camera beside one */
        psm_state_t st;
        float d0, d1, dx, dz;
        unsigned k, out = 0;
        psm_mover_t *h = NULL;

        psm_state_town(&st);
        for (i = 0; i < 7200u; i++) {
            psm_town_update(&scene, &st, 1.0f / 60.0f);
            for (k = 0; k < scene.movern; k++) {
                const psm_mover_t *m = &scene.mover[k];

                if (m->kind != 1u) continue;
                out += !(m->x >= m->b[0] - 1e-4f && m->x <= m->b[1] + 1e-4f && m->z >= m->b[2] - 1e-4f && m->z <= m->b[3] + 1e-4f) || !(m->tilt == m->tilt);
            }
        }
        for (k = 0; k < scene.movern; k++) if (scene.mover[k].kind == 1u && scene.mover[k].b[0] < 7.0f) { h = &scene.mover[k]; break; }
        CHECK(h != NULL && out == 0u);
        if (h) {
            dx = 0.5f * (h->b[0] + h->b[1]) - h->x; dz = 0.5f * (h->b[2] + h->b[3]) - h->z;   /* the camera 0.8 m from a loose hen, */
            d1 = sh4der_sqrt_(dx * dx + dz * dz) + 1e-6f;                                        /* on its side away from the middle */
            st.x = h->x - 0.8f * dx / d1; st.z = -(h->z - 0.8f * dz / d1);
            d0 = 0.8f;
            for (i = 0; i < 60u; i++) psm_town_update(&scene, &st, 1.0f / 60.0f);
            dx = h->x - st.x; dz = h->z + st.z;
            d1 = sh4der_sqrt_(dx * dx + dz * dz);
            printf("  hens: 2 min in their bounds (%u out); one 0.8 m from the camera is %.2f m off after 1 s\n", out, d1);
            CHECK(d1 > d0 + 0.5f);
        }
    }
}

/* ---- the clock, the presets, the pad ---------------------------------------------------------------------------- */
static void clock_pad(void) {
    char ln[PSM_HUD_LINES][PSM_HUD_COLS + 1u], v[48];
    psm_state_t st;
    float az, el, c0;
    unsigned i;
    int r;

    psm_state_town(&st);
    psm_sun_at(14.0f, &az, &el);
    CHECK(st.scene == 1 && st.clock == 14.0f && st.az == az && st.el == el && !st.sun_free);
    CHECK(st.x == 0.0f && st.z == 14.0f && st.yaw == 0.0f && fabsf(st.pitch - 4.0f * 0.01745329f) < 1e-6f);
    in_(&st, NULL, PSM_BTN_RIGHT, 0, 0, 0, 0, 0.016f);                       /* a tap: a quarter of an hour, the sun on it */
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
    psm_sun_at(14.25f, &az, &el);
    CHECK(st.clock == 14.25f && st.az == az && st.el == el);
    for (i = 0; i <= 60u; i++) in_(&st, NULL, PSM_BTN_LEFT, 0, 0, 0, 0, i ? 1.0f / 60.0f : 0.0f);   /* held 1 s: 8 steps */
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
    CHECK(fabsf(st.clock - 12.25f) < 1e-4f);
    in_(&st, NULL, 0u, 0, 0, 0, 1.0f, 0.5f);                                  /* the R trigger: the sun off the clock */
    CHECK(st.sun_free && st.el > el);
    c0 = st.clock;
    st.tspeed = 3u;                                                           /* x60: a minute of the clock a second */
    for (i = 0; i < 600u; i++) psm_town_update(&scene, &st, 0.1f);
    CHECK(fabsf(st.clock - (c0 + 1.0f)) < 1e-3f && st.sun_free);             /* the free sun stays where it was set */
    in_(&st, NULL, PSM_BTN_RIGHT, 0, 0, 0, 0, 0.016f);
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
    CHECK(!st.sun_free);                                                      /* a clock step puts it back */
    st.clock = 15.99f;
    psm_town_update(&scene, &st, 2.0f);
    CHECK(st.clock == PSM_CLOCK_T0);                                         /* past 16:00: the sweep again */
    in_(&st, NULL, PSM_BTN_START, 0, 0, 0, 0, 0.016f);                       /* START + B: freeze; START + Y: the next preset */
    in_(&st, NULL, PSM_BTN_START | PSM_BTN_B, 0, 0, 0, 0, 0.016f);
    in_(&st, NULL, PSM_BTN_START, 0, 0, 0, 0, 0.016f);
    in_(&st, NULL, PSM_BTN_START | PSM_BTN_Y, 0, 0, 0, 0, 0.016f);
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
    CHECK(st.freeze && st.tpreset == 1u && st.clock == 9.0f && st.warp == 0);   /* and the release is no tap */
    c0 = st.cast_t;
    psm_town_update(&scene, &st, 1.0f);
    CHECK(st.cast_t == c0 && st.clock > 9.0f);                                /* frozen: the cast stops, the sun's clock goes on */
    c0 = st.clock;
    in_(&st, NULL, PSM_BTN_START, 0, 0, 0, 0, 0.016f);                       /* START + left: the sun's azimuth, free */
    in_(&st, NULL, PSM_BTN_START | PSM_BTN_LEFT, 0, 0, 0, 0, 0.016f);
    in_(&st, NULL, 0u, 0, 0, 0, 0, 0.016f);
    CHECK(st.sun_free && st.clock == c0);
    for (r = PSM_ROW_SCENE; r < PSM_ROWS; r++) {                              /* the town's rows read as lines of their own */
        st.row = r;
        psm_hud(&st, &frame, NULL, ln);
        CHECK(strstr(ln[8], psm_row_name[r]) == ln[8]);
        CHECK(strlen(psm_row_name[r]) + 2u + strlen(psm_row_value(&st, r, v, sizeof v)) + strlen("   (D-pad row/change, B close)") <= PSM_HUD_COLS);
    }
    for (i = 0; i < PSM_TOWN_PRESETS; i++) {
        st.row = PSM_ROW_PRESET;
        psm_row_change(&st, PSM_ROW_PRESET, 1);
        CHECK(strlen(psm_row_name[PSM_ROW_PRESET]) + 2u + strlen(psm_row_value(&st, PSM_ROW_PRESET, v, sizeof v)) + strlen("   (D-pad row/change, B close)") <= PSM_HUD_COLS);
    }
    CHECK(st.tpreset == 1u);
    psm_row_change(&st, PSM_ROW_SCENE, 1);                                    /* to the field: its origin and its sun */
    CHECK(st.scene == 0 && st.x == 0.0f && st.z == 0.0f && st.az == psm_preset_sun[st.preset % 3u][0]);
    psm_row_change(&st, PSM_ROW_SCENE, 1);
    CHECK(st.scene == 1 && st.tpreset == 0u && st.z == 14.0f);
}

/* ---- the walk ----------------------------------------------------------------------------------------------------- */
static void walk(void) {
    psm_walk_limits_t lim;
    psm_state_t st;
    unsigned i, k, esc = 0, inside = 0;
    uint32_t rng = 7u;

    psm_scene_limits(&scene, &lim);
    for (k = 0; k < lim.n; k++) {
        const float *c = lim.circle[k];

        CHECK(c[0] - c[2] > lim.x0 && c[0] + c[2] < lim.x1 && c[1] - c[2] > lim.z0 && c[1] + c[2] < lim.z1);
    }
    for (i = 0; i < PSM_TOWN_PRESETS; i++) {                                  /* every preset stands in the walk */
        float x, z;

        psm_town_preset_set(&st, i);
        x = st.x; z = st.z;
        psm_walk_limit(&x, &z, 0.0f, 1.0f, &lim);
        CHECK(x == st.x && z == st.z);
    }
    psm_state_town(&st);
    for (i = 0; i < 4000u; i++) {
        const float a = psm_rand_(&rng) * 2.0f - 1.0f, b = psm_rand_(&rng) * 2.0f - 1.0f;

        in_(&st, &lim, (b > 0.3f ? PSM_BTN_Y : 0u) | (a > 0.6f ? PSM_BTN_B : 0u), a, 0.0f, 0, 0, 0.05f);
        esc += st.x < lim.x0 || st.x > lim.x1 || st.z < lim.z0 || st.z > lim.z1;
        for (k = 0; k < lim.n; k++) {
            const float dx = st.x - lim.circle[k][0], dz = st.z - lim.circle[k][1];

            inside += dx * dx + dz * dz < lim.circle[k][2] * lim.circle[k][2] * 0.999f;
        }
    }
    printf("  walk: 4000 random steps, %u out of the area, %u in a circle (the well, the cross)\n", esc, inside);
    CHECK(esc == 0u && inside == 0u);
}

/* ---- the caster cull, the slab, the receivers ------------------------------------------------------------------ */

/* 1 when the box is wholly off the view (its corners past one side of the camera's clip volume) */
static int box_out_of_view(const psm_frame_t *f, const float lo[3], const float hi[3]) {
    int lx = 1, hx = 1, ly = 1, hy = 1, bz = 1;
    unsigned c;

    for (c = 0; c < 8u; c++) {
        const float p[3] = { (c & 1u) ? hi[0] : lo[0], (c & 2u) ? hi[1] : lo[1], (c & 4u) ? hi[2] : lo[2] };
        float h[4];

        sh4der_psm_xform(f->rows, p, 1.0f, h);
        lx &= h[0] < -h[3]; hx &= h[0] > h[3]; ly &= h[1] < -h[3]; hy &= h[1] > h[3]; bz &= h[3] < PSM_NEAR;
    }
    return lx || hx || ly || hy || bz;
}

static void cull(void) {
    static sh4der_psm_pvr_vtx_t out[SH4DER_PSM_PVR_MAX_TRIS][3];
    psm_state_t st;
    unsigned p, route, warp, i, k, maps = 0, culled = 0, culled_max = 0, a_bad = 0, b_bad = 0;

    for (p = 0; p < PSM_TOWN_PRESETS; p++)
        for (route = 0; route < 2u; route++)
            for (warp = 0; warp < 2u; warp++) {
                psm_state_town(&st);
                psm_town_preset_set(&st, p);
                st.route = (int)route; st.warp = (int)warp;
                psm_town_update(&scene, &st, 0.0f);
                psm_frame_setup(&frame, &scene, &st);
                CHECK(frame.slab == (route ? PSM_TOWN_SLAB_B : PSM_TOWN_SLAB_A));
                if (!frame.built) continue;
                maps++;
                if (route) {
                    size_t bytes = (size_t)frame.size * frame.size * sizeof depth[0];

                    CHECK(psm_frame_b_begin(&frame, &scene, depth));
                    psm_frame_b_casters(&frame, &scene, 0);
                    CHECK(frame.tris == scene.ctn);
                    CHECK(psm_frame_b_begin(&frame, &scene, depth2));             /* every caster, no cull */
                    for (i = 0; i < scene.ctn; i++) sh4der_psm_raster_tri_h(&frame.raster, frame.mh[scene.ct[i][0]], frame.mh[scene.ct[i][1]], frame.mh[scene.ct[i][2]]);
                    b_bad += memcmp(depth, depth2, bytes) != 0;
                } else {
                    psm_frame_a_casters(&frame, &scene);
                    for (k = 0; k < scene.con; k++) {                            /* a culled object's triangles are all off the target */
                        if (frame.con[k]) continue;
                        for (i = scene.co[k].t0; i < scene.co[k].t0 + scene.co[k].tn; i++) {
                            const unsigned n = psm_pvr_tris_h(frame.ma[scene.ct[i][0]], frame.ma[scene.ct[i][1]], frame.ma[scene.ct[i][2]], frame.size, out);
                            unsigned t;

                            for (t = 0; t < n; t++) {
                                const float sz = (float)frame.size;
                                const int off = (out[t][0].x < 0 && out[t][1].x < 0 && out[t][2].x < 0) || (out[t][0].x > sz && out[t][1].x > sz && out[t][2].x > sz)
                                             || (out[t][0].y < 0 && out[t][1].y < 0 && out[t][2].y < 0) || (out[t][0].y > sz && out[t][1].y > sz && out[t][2].y > sz);

                                a_bad += !off;
                            }
                        }
                    }
                }
                culled += frame.cull_objs;
                if (frame.cull_objs > culled_max) culled_max = frame.cull_objs;
                if (p == 0u) CHECK(frame.con[PSM_TOWN_CO_TOWER]);                 /* the boot view: the tower is in the map */
            }
    printf("  cull: %u maps (6 presets, both routes, uniform and PSM): %u objects culled in all, at most %u of %u in one; route B map"
           " with the cull = without in every map (%u differ), route A: %u culled triangles on the target\n", maps, culled, culled_max, scene.con, b_bad, a_bad);
    CHECK(maps >= 20u && b_bad == 0u && a_bad == 0u && culled > 0u);

    /* the boot view: the tower is out of view and its shadow is in the map (route B, uniform and PSM) */
    for (warp = 0; warp < 2u; warp++) {
        const psm_cobj_t *t = &scene.co[PSM_TOWN_CO_TOWER];

        psm_state_town(&st);
        st.route = PSM_ROUTE_B; st.warp = (int)warp;
        psm_town_update(&scene, &st, 0.0f);
        psm_frame_setup(&frame, &scene, &st);
        CHECK(box_out_of_view(&frame, t->lo, t->hi));
        CHECK(psm_frame_b_begin(&frame, &scene, depth));
        psm_frame_b_casters(&frame, &scene, 0);
        CHECK(psm_frame_b_begin(&frame, &scene, depth2));
        for (i = t->t0; i < t->t0 + t->tn; i++) sh4der_psm_raster_tri_h(&frame.raster, frame.mh[scene.ct[i][0]], frame.mh[scene.ct[i][1]], frame.mh[scene.ct[i][2]]);
        printf("  the tower at the boot view (%s): out of view, kept, draws %u texels' triangles into the map\n", warp ? "PSM" : "uniform", frame.raster.drawn);
        CHECK(frame.raster.drawn > 0u);
    }

    /* the fit's slab: route A 0 .. 0.5 m, route B 0 .. 13 m; head-on, what each fit gives */
    for (route = 0; route < 2u; route++) {
        psm_state_town(&st);
        psm_town_preset_set(&st, 3u);
        st.route = (int)route; st.warp = 1;
        psm_frame_setup(&frame, &scene, &st);
        printf("  head-on 16:00, route %c (slab 0 .. %.1f m): %s, texel/px at 4 m %.1f\n", route ? 'B' : 'A', frame.slab,
               !frame.built ? "no map" : frame.head_on ? "PSM fell back to uniform" : psm_kind_name(frame.map.kind), frame.ratio[0]);
    }
}

static void receivers(void) {
    static uint8_t full[PSM_MV_MAX];
    psm_state_t st;
    unsigned i, vis = 0, bad = 0;

    psm_state_town(&st);
    st.route = PSM_ROUTE_B; st.warp = 1;
    psm_town_update(&scene, &st, 0.0f);
    psm_frame_setup(&frame, &scene, &st);
    CHECK(psm_frame_b_begin(&frame, &scene, depth));
    psm_frame_b_casters(&frame, &scene, 0);
    psm_frame_b_receivers(&frame, &scene, &st);                               /* no placement yet: every vertex */
    memcpy(full, frame.mlit, scene.mvn);
    psm_place_geo(&frame, &scene);
    memset(frame.mlit, 7, scene.mvn);
    psm_frame_b_receivers(&frame, &scene, &st);                               /* the sent vertices only */
    for (i = 0; i < scene.mvn; i++) {
        vis += frame.mvis[i];
        bad += frame.mvis[i] ? frame.mlit[i] != full[i] : frame.mlit[i] != 7u;
    }
    printf("  receivers at the boot view: %u of %u model vertices compared (the ones sent), the same answers\n", vis, scene.mvn);
    CHECK(bad == 0u && vis > 0u && vis < scene.mvn);
}

/* ---- the vertices a frame --------------------------------------------------------------------------------------- */
/* What psm_dc.c sends, through its own walks (demo/psm/psm_send.h): the opaque list (the air, the ground, the models), the
   translucent one (route A's layer, the readout, the inset, the heartbeat), and route A's map pass, its own scene. The
   bytes are the TA's parameter bytes as psm_send.h estimates them; the console's own reading is the readout's line 11. */
static psm_sky_out_t vsky;
static psm_bins_t vbins;
static sh4der_psm_pvr_vtx_t vmap[SH4DER_PSM_PVR_MAX_TRIS][3];

typedef struct view_count {
    psm_sink_t k;               /* the scene at the frame's SHED */
    uint32_t bytes0;            /* the scene at SHED 0 */
    unsigned shed, map_v;
    uint32_t map_bytes;
    psm_bin_use_t bins;
} view_count_t;

/* one view as psm_dc.c makes it: the setup, the placement, the air, the route's work, the colours, then the plan and the send */
static void view_count(psm_state_t *st, view_count_t *vc, int chk) {
    const int a = st->route == PSM_ROUTE_A;
    uint8_t ok[PSM_HK_N];
    psm_sink_t m;

    psm_frame_setup(&frame, &scene, st);
    psm_place_geo(&frame, &scene);
    psm_sky_build(&vsky, &frame, st);
    psm_frame_unshadowed(&frame, &scene);
    memset(vc, 0, sizeof *vc);
    if (a) {
        psm_frame_a_casters(&frame, &scene);
        psm_sink_reset(&m, 0, chk, 0u, NULL);
        psm_send_map(&m, &frame, &scene, st, frame.size, vmap);
        vc->map_v = m.verts; vc->map_bytes = m.bytes;
        if (chk) CHECK(m.bad_short == 0u && m.bad_open == 0u && m.bad_coord == 0u);
        psm_frame_a_ground(&frame);
    }
    psm_colour(&frame, &scene);
    memset(ok, 1, sizeof ok);
    if (!a) ok[PSM_HK_LAYER] = ok[PSM_HK_INSET_A] = 0;
    psm_send_plan(&vc->k, &frame, &scene, st, &vsky, a, ok, PSM_TA_VBUF);
    vc->shed = vc->k.shed;
    if (vc->shed) {
        psm_sink_t z;

        psm_sink_reset(&z, 0, 0, 0u, NULL);
        memcpy(z.hdr_ok, ok, sizeof ok);
        psm_send_op(&z, &frame, &scene, &vsky);
        psm_send_tr(&z, &frame, &scene, st, a, 0u);
        vc->bytes0 = z.bytes + PSM_TA_BKG;
    } else
        vc->bytes0 = vc->k.bytes + PSM_TA_BKG;
    if (chk) {                                        /* the frame as sent, checked: strips, ends, coordinates, the bins */
        const unsigned shed = vc->shed;

        psm_sink_reset(&vc->k, 0, 1, shed, &vbins);
        memcpy(vc->k.hdr_ok, ok, sizeof ok);
        psm_send_op(&vc->k, &frame, &scene, &vsky);
        psm_send_tr(&vc->k, &frame, &scene, st, a, 0u);
        psm_bins_use(&vbins, &vc->bins);
    }
}

static void budget(void) {
    psm_state_t st;
    view_count_t vc;
    unsigned p, q, step, route, views = 0, worst_v = 0, worst_sky = 0, worst_map_v = 0, sheds = 0, worst_p = 0, worst_h = 0, worst_q = 0;
    uint32_t worst_b = 0, worst_b0 = 0;

    for (route = 0; route < 2u; route++)
        for (p = 0; p < PSM_TOWN_PRESETS; p++)
            for (q = 0; q < 3u; q++)
                for (step = 0; step < 24u; step++) {
                    psm_state_town(&st);
                    psm_town_preset_set(&st, p);
                    st.route = (int)route;
                    st.pitch = q == 0u ? st.pitch : q == 1u ? -PSM_PITCH_UP : PSM_PITCH_DOWN;
                    st.yaw = (float)step * 0.261799f;
                    psm_town_update(&scene, &st, 0.0f);
                    view_count(&st, &vc, 0);
                    if (vsky.n_band + vsky.n_cap + vsky.n_hill > worst_sky) worst_sky = vsky.n_band + vsky.n_cap + vsky.n_hill;
                    if (vc.k.verts > worst_v) worst_v = vc.k.verts;
                    if (vc.bytes0 > worst_b0) { worst_b0 = vc.bytes0; worst_p = p; worst_h = step * 15u; worst_q = q; }
                    if (vc.k.bytes + PSM_TA_BKG > worst_b) worst_b = vc.k.bytes + PSM_TA_BKG;
                    if (vc.map_v > worst_map_v) worst_map_v = vc.map_v;
                    sheds += vc.shed != 0u;
                    views++;
                }
    printf("  a frame through psm_dc.c's walks, %u views (2 routes x 6 presets x 3 pitches x 24 headings): at most %u vertices, the TA's"
           " parameters (estimate) %u KB with nothing shed (preset %u, hdg %u, pitch row %u), %u KB as sent, %u views shed; the air %u"
           " vertices at most; route A's map pass (its own scene) %u vertices at most; buffer %u KB, SHED past %u KB\n",
           views, worst_v, (unsigned)(worst_b0 / 1024u), worst_p, worst_h, worst_q, (unsigned)(worst_b / 1024u), sheds, worst_sky, worst_map_v,
           PSM_TA_VBUF / 1024u, (unsigned)(PSM_TA_VBUF * PSM_TA_SHARE / 1024.0f));
    CHECK(worst_b <= (uint32_t)(PSM_TA_VBUF * PSM_TA_SHARE) && worst_sky < PSM_SKY_OUT_MAX);
}

/* The a3 crash's camera (the owner's photo: route A, MAP 256 UNIFORM, 14:00, DIST INF, CAM x -1.7 z -11.8 story, so world
   z 11.8, hdg 337, pitch -10 on the readout) turned through 360 degrees a degree at a time, at readout pitches -10, 0 and +20:
   every strip of 3 or more vertices with its end flag, no header inside a strip or without a vertex (psm_send.h sends a
   header with its first vertex), every coordinate finite and in front, the frame under PSM_TA_SHARE of the buffer as sent,
   the tile bins inside KallistiOS's overflow pool. Then the walk area on a 4 m grid, 12 headings, the same three pitches. */
static void sweep(void) {
    static const int pitch_deg[3] = { -10, 0, 20 };
    psm_state_t st;
    view_count_t vc;
    unsigned q, h, bad = 0, views = 0, sheds = 0, worst_h = 0, worst_q = 0, worst_v = 0, worst_tile[2] = { 0, 0 }, worst_extra = 0, pool = 0;
    uint32_t worst_b0 = 0, worst_b = 0;
    float worst_xy = 0.0f, x, z;

    for (q = 0; q < 3u; q++)
        for (h = 0; h < 360u; h++) {
            psm_state_town(&st);
            st.route = PSM_ROUTE_A; st.warp = 0;
            st.x = -1.7f; st.z = 11.8f;
            st.yaw = (float)h * 0.01745329f;
            st.pitch = -(float)pitch_deg[q] * 0.01745329f;
            psm_town_update(&scene, &st, 0.0f);
            view_count(&st, &vc, 1);
            bad += vc.k.bad_short + vc.k.bad_open + vc.k.bad_coord + (vc.k.hdrs > vc.k.strips);
            bad += vc.k.bytes + PSM_TA_BKG > (uint32_t)(PSM_TA_VBUF * PSM_TA_SHARE);
            bad += vc.bins.extra_blocks > vc.bins.pool_blocks;
            sheds += vc.shed != 0u;
            if (vc.bytes0 > worst_b0) { worst_b0 = vc.bytes0; worst_h = h; worst_q = q; }
            if (vc.k.bytes + PSM_TA_BKG > worst_b) worst_b = vc.k.bytes + PSM_TA_BKG;
            if (vc.k.verts > worst_v) worst_v = vc.k.verts;
            if (vc.k.max_xy > worst_xy) worst_xy = vc.k.max_xy;
            if (vc.bins.worst_tile[0] > worst_tile[0]) worst_tile[0] = vc.bins.worst_tile[0];
            if (vc.bins.worst_tile[1] > worst_tile[1]) worst_tile[1] = vc.bins.worst_tile[1];
            if (vc.bins.extra_blocks > worst_extra) worst_extra = vc.bins.extra_blocks;
            pool = vc.bins.pool_blocks;
            if (h == 337u && q == 0u)
                printf("  the a3 view (hdg 337, pitch -10, route A uniform): %u vertices (ground %u, layer %u, models %u, air %u, text %u),"
                       " %u strips; the TA's parameters %u KB (estimate) of %u, SHED %u; map pass %u vertices %u KB\n", vc.k.verts,
                       vc.k.kv[PSM_HK_GROUND] + vc.k.kv[PSM_HK_GROUND_FOG], vc.k.kv[PSM_HK_LAYER], vc.k.kv[PSM_HK_MODEL] + vc.k.kv[PSM_HK_MODEL_FOG],
                       vc.k.kv[PSM_HK_SKY] + vc.k.kv[PSM_HK_SKYCOL], vc.k.kv[PSM_HK_TEXT] + vc.k.kv[PSM_HK_DIM] + vc.k.kv[PSM_HK_INSET_A],
                       vc.k.strips, (unsigned)((vc.k.bytes + PSM_TA_BKG) / 1024u), PSM_TA_VBUF / 1024u, vc.shed, vc.map_v, (unsigned)(vc.map_bytes / 1024u));
            views++;
        }
    printf("  sweep from the a3 camera, %u views (360 headings x pitch -10, 0, +20): %u faults; at most %u vertices; the TA's parameters"
           " %u KB with nothing shed (hdg %u, pitch %d), %u KB as sent, %u views shed; largest |x| or |y| sent %.0f; tile bins at most %u"
           " (opaque) and %u (translucent) entries in one tile, %u of %u overflow blocks\n", views, bad, worst_v, (unsigned)(worst_b0 / 1024u),
           worst_h, pitch_deg[worst_q], (unsigned)(worst_b / 1024u), sheds, worst_xy, worst_tile[0], worst_tile[1], worst_extra, pool);
    CHECK(bad == 0u);

    views = 0; bad = 0; sheds = 0; worst_b0 = 0; worst_b = 0; worst_extra = 0;
    {
        float wx = 0.0f, wz = 0.0f;
        unsigned wh = 0, wq = 0;

        for (x = scene.x0; x <= scene.x1; x += 4.0f)
            for (z = scene.z0; z <= scene.z1; z += 4.0f) {
                unsigned o, inside = 0;

                for (o = 0; o < scene.obn; o++) {
                    const float dx = x - scene.obst[o][0], dz = z - scene.obst[o][1];

                    inside |= dx * dx + dz * dz < scene.obst[o][2] * scene.obst[o][2];
                }
                if (inside) continue;
                for (q = 0; q < 3u; q++)
                    for (h = 0; h < 360u; h += 30u) {
                        psm_state_town(&st);
                        st.route = PSM_ROUTE_A;
                        st.x = x; st.z = z;
                        st.yaw = (float)h * 0.01745329f;
                        st.pitch = -(float)pitch_deg[q] * 0.01745329f;
                        psm_town_update(&scene, &st, 0.0f);
                        view_count(&st, &vc, 1);
                        bad += vc.k.bad_short + vc.k.bad_open + vc.k.bad_coord + (vc.k.hdrs > vc.k.strips);
                        bad += vc.k.bytes + PSM_TA_BKG > (uint32_t)(PSM_TA_VBUF * PSM_TA_SHARE);
                        bad += vc.bins.extra_blocks > vc.bins.pool_blocks;
                        sheds += vc.shed != 0u;
                        if (vc.bytes0 > worst_b0) { worst_b0 = vc.bytes0; wx = x; wz = z; wh = h; wq = q; }
                        if (vc.k.bytes + PSM_TA_BKG > worst_b) worst_b = vc.k.bytes + PSM_TA_BKG;
                        if (vc.bins.extra_blocks > worst_extra) worst_extra = vc.bins.extra_blocks;
                        views++;
                    }
            }
        printf("  the walk area on a 4 m grid, %u views (12 headings x 3 pitches, route A): %u faults; the TA's parameters %u KB with"
               " nothing shed (story x %.0f z %.0f, hdg %u, pitch %d), %u KB as sent, %u views shed; %u of %u overflow bin blocks at most\n",
               views, bad, (unsigned)(worst_b0 / 1024u), wx, -wz, wh, pitch_deg[wq], (unsigned)(worst_b / 1024u), sheds, worst_extra, pool);
    }
    CHECK(bad == 0u);
}

/* ---- the swim: shadow edges that flip while the camera moves ------------------------------------------------------- */
/* MAP HOLD off against on. The boot view, everything still (no clock, no movers): 60 frames walking forward 2 cm a frame
   (a texel of a 64 map is about a metre), then 60 frames turning 0.1 degree a frame. Fixed probes on the ground, a 0.3 m
   lattice 4 to 40 m ahead and 15 m to each side: in shadow or not, each frame. Nothing in the scene moves, so every probe
   that changes is the map's texels moving under it. Route B: the depth compare, point (B FILTER off, so an edge is an edge).
   Route A on the host: the same map's silhouette (any caster texel, no compare), point, as the PVR's lookup with A FILTER
   point; the bilinear filter softens a flip but moves it the same way. */
#define SWIM_PX 120u
#define SWIM_PZ 120u
static uint8_t swim_prev[SWIM_PX * SWIM_PZ];
static unsigned swim_fr[120];

static int swim_shadowed_(const psm_frame_t *f, const float p[3], int route) {
    if (route == PSM_ROUTE_B) return sh4der_psm_raster_lit(&f->raster, p, 0.02f, 0) < 0.5f;
    {
        float h[4];
        int tx, ty;

        sh4der_psm_xform(f->raster.m, p, 1.0f, h);
        if (!(h[3] > SH4DER_PSM_TRI_EPS)) return 0;
        tx = (int)floorf((h[0] / h[3] + 1.0f) * 0.5f * (float)f->raster.size);
        ty = (int)floorf((h[1] / h[3] + 1.0f) * 0.5f * (float)f->raster.size);
        if (tx < 0 || ty < 0 || tx >= (int)f->raster.size || ty >= (int)f->raster.size) return 0;
        return f->raster.depth[(unsigned)ty * f->raster.size + (unsigned)tx] != 0u;
    }
}

/* flips a frame over the path, and the times the held fit was taken again */
static float swim_run_(int route, int warp, unsigned size_i, int hold, unsigned *steps, unsigned *shadowed) {
    psm_state_t st;
    float x0, z0, yaw0, sy, cy;
    unsigned fr, i, j, flips = 0, shade = 0;

    psm_state_town(&st);
    st.route = route; st.warp = warp; st.size_i = size_i; st.hold = hold; st.pcf = 0;
    psm_town_update(&scene, &st, 0.0f);
    x0 = st.x; z0 = st.z; yaw0 = st.yaw;
    sy = sinf(yaw0); cy = cosf(yaw0);
    memset(&frame.hold, 0, sizeof frame.hold); frame.hold_key = 0u;
    memset(swim_fr, 0, sizeof swim_fr);
    for (fr = 0; fr < 120u; fr++) {
        const float walk = (float)(fr < 60u ? fr : 60u) * 0.02f;

        st.x = x0 + sy * walk; st.z = z0 - cy * walk;
        st.yaw = yaw0 + (fr < 60u ? 0.0f : (float)(fr - 60u) * 0.1f * 0.01745329f);
        psm_frame_setup(&frame, &scene, &st);
        if (!psm_frame_b_begin(&frame, &scene, depth)) continue;
        psm_frame_b_casters(&frame, &scene, 0);
        for (j = 0; j < SWIM_PZ; j++)
            for (i = 0; i < SWIM_PX; i++) {
                const float ahead = 4.0f + 0.3f * (float)j, side = -18.0f + 0.3f * (float)i;
                const float p[3] = { x0 + sy * ahead + cy * side, 0.0f, z0 - cy * ahead + sy * side };
                const uint8_t s = (uint8_t)swim_shadowed_(&frame, p, route);

                if (fr) { flips += s != swim_prev[j * SWIM_PX + i]; swim_fr[fr] += s != swim_prev[j * SWIM_PX + i]; }
                else shade += s;
                swim_prev[j * SWIM_PX + i] = s;
            }
    }
    *steps = frame.hold.steps;
    *shadowed = shade;
    return (float)flips / 119.0f;
}

static void swim(void) {
    unsigned route, warp, si, worst_hold_ratio_bad = 0;

    printf("  swim: shadow flips a frame on %u fixed ground probes, 60 frames walking 2 cm a frame then 60 turning 0.1 degree"
           " (nothing moves but the camera)\n", SWIM_PX * SWIM_PZ);
    for (route = 0; route < 2u; route++)
        for (warp = 0; warp < 2u; warp++)
            for (si = 0; si < 3u; si++) {
                unsigned st0, st1, sh0, sh1;
                const float off = swim_run_((int)route, (int)warp, si, 0, &st0, &sh0), on = swim_run_((int)route, (int)warp, si, 1, &st1, &sh1);

                printf("    route %c %-7s %3u: HOLD off %7.1f flips a frame, on %6.1f (%u refits in 120 frames); probes in shadow at the"
                       " start %u / %u\n", route ? 'B' : 'A', warp ? "PSM" : "uniform", psm_map_sizes[si], off, on, st1, sh0, sh1);
                worst_hold_ratio_bad += warp ? !(on <= off * 0.6f) : !(on <= off * 0.05f + 2.0f);   /* PSM: pops at its steps remain */
                if (getenv("SWIM_DUMP") && warp) { unsigned f_; printf("     on:"); for (f_ = 1; f_ < 120u; f_++) if (swim_fr[f_]) printf(" %u:%u", f_, swim_fr[f_]); printf("\n"); }
                CHECK(sh1 * 10u >= sh0 * 9u && sh1 * 9u <= sh0 * 10u);            /* the same shadows, within a tenth */
            }
    CHECK(worst_hold_ratio_bad == 0u);
}

/* ---- the air: the quadratic fog, the band, the cap and the hills --------------------------------------------------- */
static void air(void) {
    static psm_sky_out_t sky;
    float tbl[129], worst = 0.0f, d;
    unsigned p, q, step, k, bad = 0, strips = 0, cut = 0, n, views = 0;
    psm_state_t st;

    /* the fog: TOWN_LOOK.md's numbers at D = 70, and the table against the curve it stands for */
    CHECK(psm_fog_at(70.0f, 30.0f) == 0.0f && fabsf(psm_fog_at(70.0f, 40.0f) - 0.0616f) < 0.002f && psm_fog_at(70.0f, 70.0f) == 1.0f);
    for (k = 0; k < 4u; k++) {
        const float dist = k == 0u ? 5.0f : k == 1u ? 20.0f : k == 2u ? 70.0f : 500.0f;

        psm_fog_table(dist, tbl);
        CHECK(tbl[0] == 1.0f && tbl[128] == 0.0f);
        for (n = 1; n < 129u; n++) bad += tbl[n] > tbl[n - 1u];               /* from the far depth inward: never thicker */
        for (d = 0.02f * dist; d <= dist; d += 0.005f * dist) {
            const float e = fabsf(psm_fog_table_at(tbl, dist, d) - psm_fog_at(dist, d));

            if (e > worst) worst = e;
        }
    }
    printf("  fog: quadratic 0.43 D .. D; at D 70 the tower (40 m) %.3f, the hall (26 m) %.3f; the table's worst against the curve %.4f\n",
           psm_fog_at(70.0f, 40.0f), psm_fog_at(70.0f, 26.0f), worst);
    CHECK(bad == 0u && worst < 0.02f);

    /* the sky's u turn: the HDR's sun column faces the sun at any azimuth */
    for (k = 0; k < 36u; k++) {
        const float az = 10.0f * (float)k, u = psm_sky_u(az, psm_sky_u_off(az));

        bad += fabsf((u - floorf(u)) - PSM_SKY_SUN_U) > 1e-4f && fabsf((u - floorf(u)) - PSM_SKY_SUN_U) < 1.0f - 1e-4f;
    }
    CHECK(bad == 0u);

    /* every view of the budget: the strips are whole, every vertex finite and in front, the on-screen ones behind the town */
    for (p = 0; p < PSM_TOWN_PRESETS; p++)
        for (q = 0; q < 3u; q++)
            for (step = 0; step < 24u; step++) {
                unsigned run = 0;

                psm_state_town(&st);
                psm_town_preset_set(&st, p);
                st.pitch = q == 0u ? st.pitch : q == 1u ? -PSM_PITCH_UP : PSM_PITCH_DOWN;
                st.yaw = (float)step * 0.261799f;
                psm_frame_setup(&frame, &scene, &st);
                psm_sky_build(&sky, &frame, &st);
                n = sky.n_band + sky.n_cap + sky.n_hill;
                for (k = 0; k < n; k++) {
                    const psm_skyv_t *v = &sky.v[k];
                    const int on = v->x >= 0.0f && v->x <= PSM_SCR_W && v->y >= 0.0f && v->y <= PSM_SCR_H;

                    bad += !sh4der_finite_(v->x) || !sh4der_finite_(v->y) || !(v->z > 0.0f) || !(v->z <= 1.0f / (1.01f * PSM_NEAR) + 1e-3f);
                    bad += on && v->z > 1.0f / 150.0f;                       /* on screen: the air is past 150 m */
                    run++;
                    if (sky.eol[k]) { bad += run < 3u; run = 0; strips++; }
                    bad += (k + 1u == sky.n_band || k + 1u == sky.n_band + sky.n_cap || k + 1u == n) && !sky.eol[k];   /* a run ends with its header's part */
                }
                cut += sky.cut;
                views++;
            }
    printf("  air: %u views, %u strips sent, %u triangles cut at the near plane, 0 bad expected: %u\n", views, strips, cut, bad);
    CHECK(bad == 0u);

    {                                                                         /* the cut on its own: one corner behind, a quad of 4 */
        psm_skyp_t t[3];
        unsigned m = 0, i;

        memset(t, 0, sizeof t);
        memset(&sky, 0, sizeof sky);
        t[0].clip[0] = -1.0f; t[0].clip[2] = 2.0f; t[0].u = 0.0f; t[0].c[0] = 1.0f;
        t[1].clip[0] = 1.0f;  t[1].clip[2] = 2.0f; t[1].u = 1.0f; t[1].c[0] = 1.0f;
        t[2].clip[1] = 1.0f;  t[2].clip[2] = -1.0f; t[2].u = 0.5f; t[2].c[0] = 0.0f;
        for (i = 0; i < 3u; i++) { t[i].scr[2] = 1.0f / t[i].clip[2]; }
        CHECK(psm_sky_tri_(&sky, &m, &t[0], &t[1], &t[2]) == 2u && m == 4u && sky.eol[3] && !sky.eol[2]);
        for (i = 0; i < m; i++) CHECK(sky.v[i].z > 0.0f && sky.v[i].z <= 1.0f / (1.01f * PSM_NEAR) + 1e-4f);
        t[0].clip[2] = t[1].clip[2] = -0.5f;
        m = 0;
        CHECK(psm_sky_tri_(&sky, &m, &t[0], &t[1], &t[2]) == 0u && m == 0u);   /* wholly behind: nothing */
    }

    /* the boot view and the head-on view: the fog colour (TOWN_LOOK.md's pictures: (115, 122, 128) facing north, (153, 156, 146) head-on) */
    psm_state_town(&st);
    psm_frame_setup(&frame, &scene, &st);
    psm_sky_build(&sky, &frame, &st);
    printf("  air at the boot view (%s): band %u, cap %u, hills %u vertices; fog (%u, %u, %u)", PSM_SKY_HAVE ? "the HDR" : "the stand-in",
           sky.n_band, sky.n_cap, sky.n_hill, (unsigned)(sky.fog[0] * 255.0f + 0.5f), (unsigned)(sky.fog[1] * 255.0f + 0.5f), (unsigned)(sky.fog[2] * 255.0f + 0.5f));
    CHECK(sky.n_band > 0u && sky.n_cap == 0u && sky.n_hill > 0u);          /* the view's top is at el 26: no cap */
    st.pitch = -PSM_PITCH_UP;
    psm_frame_setup(&frame, &scene, &st);
    psm_sky_build(&sky, &frame, &st);
    CHECK(sky.n_cap > 0u && sky.n_hill == 0u);                            /* looking up: the cap, no hills */
    st.pitch = 4.0f * 0.01745329f;
    psm_town_preset_set(&st, 3u);
    psm_frame_setup(&frame, &scene, &st);
    psm_sky_build(&sky, &frame, &st);
    printf("; head-on 16:00 fog (%u, %u, %u)\n", (unsigned)(sky.fog[0] * 255.0f + 0.5f), (unsigned)(sky.fog[1] * 255.0f + 0.5f), (unsigned)(sky.fog[2] * 255.0f + 0.5f));
}

/* ---- the readout -------------------------------------------------------------------------------------------------- */
static void readout(void) {
    char ln[PSM_HUD_LINES][PSM_HUD_COLS + 1u];
    psm_state_t st;

    psm_state_town(&st);
    psm_town_update(&scene, &st, 0.0f);
    psm_frame_setup(&frame, &scene, &st);
    psm_hud(&st, &frame, NULL, ln);
    printf("  readout: [%s] [%s] [%s] [%s]\n", ln[0], ln[1], ln[4], ln[10]);
    CHECK(strstr(ln[0], "TOWN") && strstr(ln[1], "SLAB 0-0.5") && strstr(ln[4], "14:00 x1 SUN 214/27") && strstr(ln[4], "z -14.0"));
    CHECK(strstr(ln[9], "L/R time") && strstr(ln[10], "PLACEHOLDER"));
    st.route = PSM_ROUTE_B;
    psm_frame_setup(&frame, &scene, &st);
    psm_hud(&st, &frame, NULL, ln);
    CHECK(strstr(ln[1], "SLAB 0-13.0") && strstr(ln[5], "cull"));
}

int main(void) {
    sun();
    mesh();
    cast();
    clock_pad();
    walk();
    cull();
    receivers();
    budget();
    sweep();
    swim();
    air();
    readout();
    if (fails) { printf("psm_town_test: %d FAILED\n", fails); return 1; }
    printf("psm_town_test: ok\n");
    return 0;
}
