/* SH4DER-PVR: hero1's space stretch, host test: the autorun replayed; it fails if the hero or his shadow is ever hidden.
   Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

   cc -O2 -std=gnu11 -Wall -Wextra -DSH4DER_CORE_NO_SH4ZAM -Iinclude -Idemo/psm/hero -I<dir of hero_strip_data.h> -I<dir of hero_hens.h> \
      demo/psm/hero/test/hero_strip_test.c -lm -o hero_strip_test && ./hero_strip_test

   Every stage of the autorun (hero_strip.h's table and camera), 30 frames a second. In every frame:
     - the eye is in the street (strip_eye_ok);
     - no set triangle that faces the eye is between the eye and the hero's head, chest or feet, or his shadow's spot;
     - every vertex of the hero is in front of the camera by a metre or more, and nearly all of them are on the screen;
     - every vertex of his shadow is inside the shadow target, off its white border;
     - the shadow's spot is on the ground, on the screen, and its ground object is not left out by strip_seen;
     - no object with a vertex on the screen is left out by strip_seen;
     - no hen is between the eye and the hero's head, chest or feet, or his shadow's spot, and a hen with a vertex on
       the screen is not left out.
   And apart from the replay: the run is continuous and loops, the place matrix turns and moves as it says, the near
   clip keeps the part in front, the live view's yaw limit keeps the eye in the street, the hens stand clear of his run
   and inside the street with every vertex in their sphere, and each blob lies on the ground under its hen. */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "hero_model.h"
#include "hero_view.h"
#include "hero_strip.h"
#include "hero_hens.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { if (fails++ < 40) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } } while (0)

static hero_t h;
static float pose[HERO_BONES][12], pal[HERO_BONES][16], scr[HERO_V_MAX][3], shs[HERO_V_MAX][3];

static int ray_tri(const float o[3], const float d[3], const float a[3], const float b[3], const float c[3]) {
    const float e1[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] }, e2[3] = { c[0] - a[0], c[1] - a[1], c[2] - a[2] };
    const float n[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
    const float hh[3] = { d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2], d[0] * e2[1] - d[1] * e2[0] };
    float det = e1[0] * hh[0] + e1[1] * hh[1] + e1[2] * hh[2], s[3], q[3], u, v, t;

    if (n[0] * (o[0] - a[0]) + n[1] * (o[1] - a[1]) + n[2] * (o[2] - a[2]) <= 0.0f) return 0;   /* its back to the eye: not drawn */
    if (fabsf(det) < 1e-12f) return 0;
    det = 1.0f / det;
    s[0] = o[0] - a[0]; s[1] = o[1] - a[1]; s[2] = o[2] - a[2];
    u = (s[0] * hh[0] + s[1] * hh[1] + s[2] * hh[2]) * det;
    if (u < 0.0f || u > 1.0f) return 0;
    q[0] = s[1] * e1[2] - s[2] * e1[1]; q[1] = s[2] * e1[0] - s[0] * e1[2]; q[2] = s[0] * e1[1] - s[1] * e1[0];
    v = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) * det;
    if (v < 0.0f || u + v > 1.0f) return 0;
    t = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * det;
    return t > 0.001f && t < 0.995f;
}

/* The object whose triangle hides p from the eye, or -1. */
static int hidden_by(const float eye[3], const float p[3]) {
    const float d[3] = { p[0] - eye[0], p[1] - eye[1], p[2] - eye[2] };
    unsigned k, i;

    for (k = 0; k < STRIP_OBJS; k++) {
        const strip_obj_t *o = &strip_obj[k];

        for (i = o->t0; i < (unsigned)o->t0 + o->tn; i++)
            if (ray_tri(eye, d, strip_v[o->v0 + strip_t[i][0]], strip_v[o->v0 + strip_t[i][1]], strip_v[o->v0 + strip_t[i][2]])) return (int)k;
    }
    return -1;
}

/* The hen whose sphere the way from the eye to p passes through, or -1. */
static int hen_between(const float eye[3], const float p[3]) {
    const float d[3] = { p[0] - eye[0], p[1] - eye[1], p[2] - eye[2] }, dd = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
    unsigned h;

    for (h = 0; h < STRIP_HENS; h++) {
        const float c[3] = { strip_hen_home[h][0] - eye[0], STRIP_HEN_CY - eye[1], strip_hen_home[h][1] - eye[2] };
        float t = (c[0] * d[0] + c[1] * d[1] + c[2] * d[2]) / dd, q[3];

        t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
        q[0] = c[0] - d[0] * t; q[1] = c[1] - d[1] * t; q[2] = c[2] - d[2] * t;
        if (q[0] * q[0] + q[1] * q[1] + q[2] * q[2] < STRIP_HEN_R * STRIP_HEN_R) return (int)h;
    }
    return -1;
}

static int on_screen(const float s[3]) { return s[0] >= 0.0f && s[0] <= 640.0f && s[1] >= 0.0f && s[1] <= 480.0f; }

static unsigned frames, culled_sum, tri_sum, tri_max, clipped_max, hens_seen;

static void frame(const hero_cam_t *cam, float z, float heading, unsigned clip, float clip_t, const char *what, float t) {
    float place[16], vph[16], smat[16], smh[16], o[3];
    const float sy = strip_sun[1];
    const float pt[3][3] = { { 0.0f, 1.7f, z }, { 0.0f, 1.1f, z }, { 0.0f, 0.1f, z } };
    const float spot[3] = { -strip_sun[0] * 1.1f / sy, 0.0f, z - strip_sun[2] * 1.1f / sy };   /* the chest's shadow */
    unsigned i, k, in = 0, tris = 0, clipped = 0;
    int hb;

    frames++;
    CHECK(strip_eye_ok(cam->eye), "%s t %.2f: the eye (%.2f %.2f %.2f) is not in the street", what, t, cam->eye[0], cam->eye[1], cam->eye[2]);
    for (i = 0; i < 3u; i++) {
        hb = hidden_by(cam->eye, pt[i]);
        CHECK(hb < 0, "%s t %.2f: the hero's %s is behind object %d", what, t, i == 0u ? "head" : i == 1u ? "chest" : "feet", hb);
    }
    for (i = 0; i < 3u; i++) CHECK(hen_between(cam->eye, pt[i]) < 0, "%s t %.2f: hen %d is in front of the hero", what, t, hen_between(cam->eye, pt[i]));
    CHECK(hen_between(cam->eye, spot) < 0, "%s t %.2f: hen %d is in front of the shadow's spot", what, t, hen_between(cam->eye, spot));
    for (i = 0; i < STRIP_HENS; i++) {
        const float cs[3] = { strip_hen_home[i][0], STRIP_HEN_CY, strip_hen_home[i][1] };
        float m[16], bearing, tilt, w[4], sc[3];
        unsigned vis = 0;

        strip_hen_pose(i, t, &bearing, &tilt);
        strip_hen_place(cs[0], cs[2], bearing, tilt, m);
        for (k = 0; k < PSM_TOWN_HEN_V; k++) { hero_xf_(m, psm_town_hen_v[k], w); if (hero_view_pt(cam->vp, w, sc) && on_screen(sc)) vis++; }
        CHECK(strip_seen(cam, cs, STRIP_HEN_R) || !vis, "%s t %.2f: hen %u has %u vertices on the screen and is left out", what, t, i, vis);
        hens_seen += vis != 0u;
    }
    hb = hidden_by(cam->eye, spot);
    CHECK(hb < 0, "%s t %.2f: the shadow's spot is behind object %d", what, t, hb);
    CHECK(fabsf(spot[0]) < STRIP_GROUND_X && fabsf(spot[2]) < STRIP_GROUND_Z, "%s t %.2f: the shadow's spot is off the ground", what, t);
    CHECK(hero_view_pt(cam->vp, spot, o) && on_screen(o), "%s t %.2f: the shadow's spot is off the screen", what, t);

    hero_pose(&h, clip, clip_t, pose);
    strip_place(heading, 0.0f, z, place);
    strip_mul(vph, cam->vp, place);
    hero_palette(vph, (const float (*)[12])pose, pal);
    hero_skin(&h, (const float (*)[16])pal, 1, HERO_NEAR, scr);
    for (i = 0; i < h.vn; i++) {
        CHECK(scr[i][2] > 0.0f && scr[i][2] <= 1.0f, "%s t %.2f: hero vertex %u is %s", what, t, i, scr[i][2] > 0.0f ? "nearer than 1 m" : "behind the near plane");
        in += on_screen(scr[i]);
    }
    CHECK(in * 100u >= h.vn * 97u, "%s t %.2f: %u of the hero's %u vertices are on the screen", what, t, in, h.vn);

    strip_shadow_matrix(strip_sun, 256.0f, 0.0f, z, smat);
    strip_mul(smh, smat, place);
    hero_palette(smh, (const float (*)[12])pose, pal);
    hero_skin(&h, (const float (*)[16])pal, 0, 0.5f, shs);
    for (i = 0; i < h.vn; i++)
        CHECK(shs[i][2] == 1.0f && shs[i][0] > 1.5f && shs[i][0] < 254.5f && shs[i][1] > 1.5f && shs[i][1] < 254.5f,
              "%s t %.2f: shadow vertex %u at (%.1f, %.1f) is off the target's inside", what, t, i, shs[i][0], shs[i][1]);
    {   /* the lookup and the target agree: the spot's uv x 256 is where the matrix puts the chest */
        const float chest[3] = { 0.0f, 1.1f, z };
        float uv[2], m[4];

        strip_ground_uv(spot, 0.0f, z, uv);
        hero_xf_(smat, chest, m);
        CHECK(fabsf(uv[0] * 256.0f - m[0]) < 0.01f && fabsf(uv[1] * 256.0f - m[1]) < 0.01f, "%s t %.2f: the ground's lookup and the shadow matrix disagree", what, t);
    }

    for (k = 0; k < STRIP_OBJS; k++) {
        const strip_obj_t *ob = &strip_obj[k];
        const int seen = strip_seen(cam, ob->c, ob->r);
        unsigned vis = 0;

        for (i = ob->v0; i < (unsigned)ob->v0 + ob->vn; i++) if (hero_view_pt(cam->vp, strip_v[i], o) && on_screen(o)) vis++;
        CHECK(seen || !vis, "%s t %.2f: object %u has %u vertices on the screen and is left out", what, t, k, vis);
        if (ob->floor && fabsf(spot[2] - ob->c[2]) <= 4.0f) CHECK(seen, "%s t %.2f: the ground under the shadow (object %u) is left out", what, t, k);
        if (!seen) { culled_sum++; continue; }
        for (i = ob->t0; i < (unsigned)ob->t0 + ob->tn; i++) {
            strip_cv_t c[3], q[4];
            unsigned e, n;

            for (e = 0; e < 3u; e++) {
                float w[4];

                hero_xf_(cam->vp, strip_v[ob->v0 + strip_t[i][e]], w);
                c[e].x = w[0]; c[e].y = w[1]; c[e].w = w[3]; c[e].u = c[e].v = 0.0f; c[e].argb = 0u;
            }
            n = strip_clip(c, HERO_NEAR, q);
            if (!n) continue;
            if (c[0].w < HERO_NEAR || c[1].w < HERO_NEAR || c[2].w < HERO_NEAR) clipped++;
            {
                const float a[3] = { q[0].x / q[0].w, q[0].y / q[0].w, 0 }, b[3] = { q[1].x / q[1].w, q[1].y / q[1].w, 0 }, d[3] = { q[2].x / q[2].w, q[2].y / q[2].w, 0 };

                if (hero_area2(a, b, d) < 0.0f) tris += n - 2u;
            }
        }
    }
    tri_sum += tris;
    if (tris > tri_max) tri_max = tris;
    if (clipped > clipped_max) clipped_max = clipped;
}

int main(void) {
    unsigned s, i;
    float run_t = 0.0f, secs = 0.0f;

    CHECK(hero_build(&h), "a table is too small");

    /* the run: starts at +RUN facing -z, never jumps, loops */
    {
        float z, hd, ct, z0, hd0;
        unsigned clip;

        strip_run(0.0f, &z, &hd, &clip, &ct);
        CHECK(z == STRIP_RUN && fabsf(hd - STRIP_PI) < 1e-6f && clip == HERO_RUN_AIM && ct == 0.0f, "the run's start");
        z0 = z; hd0 = hd;
        for (i = 1; i <= 4000u; i++) {
            const float t = (float)i * (STRIP_LAP_S * 2.0f / 4000.0f);
            float dh;

            strip_run(t, &z, &hd, &clip, &ct);
            dh = fabsf(remainderf(hd - hd0, 2.0f * STRIP_PI));
            CHECK(fabsf(z - z0) <= STRIP_SPEED * (STRIP_LAP_S * 2.0f / 4000.0f) + 1e-3f, "the run jumps at t %.3f: z %.3f -> %.3f", t, z0, z);
            CHECK(dh <= STRIP_PI / STRIP_TURN_S * (STRIP_LAP_S * 2.0f / 4000.0f) + 1e-3f, "the heading jumps at t %.3f", t);
            CHECK(z >= -STRIP_RUN - 1e-3f && z <= STRIP_RUN + 1e-3f && ct >= 0.0f, "the run leaves its stretch at t %.3f", t);
            if (clip == HERO_RUN_AIM) CHECK(hd == 0.0f || fabsf(hd - STRIP_PI) < 1e-6f, "he runs sideways at t %.3f", t);   /* he runs facing the way he goes */
            else CHECK(clip == HERO_IDLE && fabsf(fabsf(z) - STRIP_RUN) < 1e-3f, "he stands away from an end at t %.3f", t);
            z0 = z; hd0 = hd;
        }
        strip_run(STRIP_LAP_S * 0.25f, &z, &hd, &clip, &ct);
        strip_run(STRIP_LAP_S * 3.25f, &z0, &hd0, &clip, &ct);
        CHECK(fabsf(z - z0) < 0.01f && fabsf(hd - hd0) < 1e-3f, "the run does not loop");
    }

    /* the place: +z at rest goes to the heading; strip_unturn undoes the turn; the product is a b */
    {
        float m[16], o[4], d[3], id[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 }, p[16];
        const float fz[3] = { 0, 0, 1 }, any[3] = { 0.3f, -0.5f, 0.8f };

        strip_place(STRIP_PI, 2.0f, -7.0f, m);
        hero_xf_(m, fz, o);
        CHECK(fabsf(o[0] - 2.0f) < 1e-5f && fabsf(o[2] + 8.0f) < 1e-5f && o[3] == 1.0f, "facing -z at heading pi");
        strip_place(0.7f, 0.0f, 0.0f, m);
        hero_xf_(m, any, o);
        strip_unturn(0.7f, o, d);
        CHECK(fabsf(d[0] - any[0]) < 1e-5f && fabsf(d[1] - any[1]) < 1e-5f && fabsf(d[2] - any[2]) < 1e-5f, "strip_unturn");
        strip_mul(p, m, id);
        for (i = 0; i < 16u; i++) CHECK(p[i] == m[i], "strip_mul by the identity");
    }

    /* the near clip */
    {
        strip_cv_t in[3] = { { 0, 0, 1.0f, 0, 0, 0xff000000u }, { 10, 0, 1.0f, 1, 0, 0xffffffffu }, { 0, 10, -0.8f, 0, 1, 0xff000000u } }, out[4];
        unsigned n = strip_clip(in, 0.1f, out);

        CHECK(n == 4u, "one vertex behind: %u vertices", n);
        for (i = 0; i < n; i++) CHECK(out[i].w >= 0.1f - 1e-6f, "a clipped vertex is behind the plane");
        CHECK(fabsf(out[2].w - 0.1f) < 1e-6f && fabsf(out[3].w - 0.1f) < 1e-6f && fabsf(out[3].y - 5.0f) < 1e-4f && fabsf(out[3].v - 0.5f) < 1e-5f, "the cut's place");
        CHECK(fabsf(out[2].u - 0.5f) < 1e-5f && ((out[2].argb >> 8) & 255u) >= 126u && ((out[2].argb >> 8) & 255u) <= 129u && (out[2].argb >> 24) == 255u, "the cut's lookup and colour");
        in[1].w = -1.0f;
        CHECK(strip_clip(in, 0.1f, out) == 3u, "two vertices behind");
        in[0].w = -1.0f;
        CHECK(strip_clip(in, 0.1f, out) == 0u, "all behind");
        in[0].w = in[1].w = in[2].w = 2.0f;
        CHECK(strip_clip(in, 0.1f, out) == 3u && out[0].x == in[0].x && out[2].y == in[2].y, "all in front");
    }

    /* the live view: at the yaw limit the eye is in the street, at any distance and pitch the keys allow, either way up it */
    {
        float dist, pitch;

        for (dist = 1.6f; dist <= 8.01f; dist += 0.4f)
            for (pitch = 0.05f; pitch <= 1.26f; pitch += 0.15f) {
                const float ym = strip_yaw_max(dist, cosf(pitch)), tg[3] = { 0.0f, 1.0f, -STRIP_RUN }, tg2[3] = { 0.0f, 1.0f, STRIP_RUN };
                hero_cam_t cam;

                hero_cam_make(&cam, ym, pitch, dist, tg2); CHECK(strip_eye_ok(cam.eye), "yaw limit %.2f at dist %.1f pitch %.2f: eye x %.2f z %.2f", ym, dist, pitch, cam.eye[0], cam.eye[2]);
                hero_cam_make(&cam, -ym, pitch, dist, tg); CHECK(strip_eye_ok(cam.eye), "yaw limit -%.2f at dist %.1f", ym, dist);
                hero_cam_make(&cam, STRIP_PI + ym, pitch, dist, tg); CHECK(strip_eye_ok(cam.eye), "yaw limit pi + %.2f at dist %.1f", ym, dist);
                hero_cam_make(&cam, STRIP_PI - ym, pitch, dist, tg2); CHECK(strip_eye_ok(cam.eye), "yaw limit pi - %.2f at dist %.1f", ym, dist);
            }
    }

    /* the hens: clear of his run, inside the street, on the ground, every vertex in the sphere whatever the pose; the blob */
    for (i = 0; i < STRIP_HENS; i++) {
        const float x = strip_hen_home[i][0], z = strip_hen_home[i][1];
        float b[STRIP_BLOB_N + 1u][3], t;
        unsigned k;

        CHECK(fabsf(x) - STRIP_HEN_R > 1.2f, "hen %u stands in his way", i);
        CHECK(fabsf(x) + STRIP_HEN_R < STRIP_CLEAR && fabsf(z) < STRIP_RUN, "hen %u is not in the street", i);
        for (t = 0.0f; t < 40.0f; t += 0.05f) {
            float m[16], bearing, tilt, w[4];

            strip_hen_pose(i, t, &bearing, &tilt);
            CHECK(tilt >= 0.0f && tilt <= 0.7001f, "hen %u tilt %.2f at t %.2f", i, tilt, t);
            strip_hen_place(x, z, bearing, tilt, m);
            for (k = 0; k < PSM_TOWN_HEN_V; k++) {
                hero_xf_(m, psm_town_hen_v[k], w);
                CHECK(w[1] > -0.02f, "hen %u vertex %u is under the ground at t %.2f (y %.3f)", i, k, t, w[1]);
                CHECK((w[0] - x) * (w[0] - x) + (w[1] - STRIP_HEN_CY) * (w[1] - STRIP_HEN_CY) + (w[2] - z) * (w[2] - z) <= STRIP_HEN_R * STRIP_HEN_R,
                      "hen %u vertex %u leaves its sphere at t %.2f", i, k, t);
            }
        }
        strip_hen_blob(x, z, strip_sun, b);
        for (k = 0; k <= STRIP_BLOB_N; k++) {
            CHECK(b[k][1] == STRIP_BLOB_Y && fabsf(b[k][0]) < STRIP_GROUND_X && fabsf(b[k][2]) < STRIP_GROUND_Z, "hen %u blob point %u is off the ground", i, k);
            CHECK(hypotf(b[k][0] - x, b[k][2] - z) < 0.6f, "hen %u blob point %u is not under it", i, k);
            CHECK(fabsf(b[k][0]) > 1.0f, "hen %u's blob is in his way", i);
        }
        CHECK((b[0][0] - x) * strip_sun[0] <= 0.0f && (b[0][2] - z) * strip_sun[2] <= 0.0f, "hen %u's blob is slid toward the sun", i);
    }

    /* the autorun, replayed */
    for (s = 0; s < STRIP_STAGES; s++) {
        const strip_stage_t *st = &strip_stage[s];
        const unsigned n = (unsigned)(st->secs * 30.0f);

        secs += st->secs;
        for (i = 0; i < n; i++) {
            hero_cam_t cam;
            const float tl = (float)i * (1.0f / 30.0f);          /* a stage starts the run and the camera's swing again, as the program does */
            float z = STRIP_RUN, hd = STRIP_PI, ct = tl;
            unsigned clip = HERO_IDLE;

            if (st->run) { strip_run(tl, &z, &hd, &clip, &ct); if (tl > run_t) run_t = tl; }
            strip_auto_cam(&cam, tl, st->dist, z);
            frame(&cam, z, hd, clip, ct, st->name, tl);
        }
    }
    CHECK(run_t >= STRIP_LAP_S, "the autorun's longest run stage (%.1f s) is shorter than a lap (%.1f s)", run_t, STRIP_LAP_S);
    CHECK(secs <= 60.0f, "the autorun is %.0f s", secs);

    /* the live view's far corners: both ends of the run, both yaw limits, both ways up the street, near and far */
    {
        static const float dists[3] = { 1.6f, 5.4f, 8.0f }, pitches[3] = { 0.05f, 0.24f, 1.25f };
        unsigned a, b, c, e, w;

        for (a = 0; a < 3u; a++) for (b = 0; b < 3u; b++) for (c = 0; c < 2u; c++) for (e = 0; e < 3u; e++) for (w = 0; w < 2u; w++) {
            const float z = e == 0u ? STRIP_RUN : e == 1u ? 0.0f : -STRIP_RUN, ym = strip_yaw_max(dists[a], cosf(pitches[b]));
            const float tg[3] = { 0.0f, 1.0f, z };
            hero_cam_t cam;

            if (dists[a] < 3.0f && pitches[b] < 0.2f) continue;      /* that close and that low the figure fills the screen: not a view to test */
            if (pitches[b] > 1.0f && dists[a] < 3.0f) continue;
            hero_cam_make(&cam, (w ? STRIP_PI : 0.0f) + (c ? ym : -ym), pitches[b], dists[a], tg);
            if (!strip_eye_ok(cam.eye)) continue;                    /* the program turns such a view back (the far end's limit) */
            {
                const float head[3] = { 0.0f, 1.7f, z }, feet[3] = { 0.0f, 0.1f, z };

                CHECK(hidden_by(cam.eye, head) < 0 && hidden_by(cam.eye, feet) < 0, "live view dist %.1f pitch %.2f yaw %s%.2f at z %.0f: the hero is hidden", dists[a], pitches[b], w ? "pi+" : "", c ? ym : -ym, z);
            }
        }
    }

    printf("space stretch: %u objects, %u vertices, %u triangles; a lap %.1f s, the autorun %.0f s, %u frames replayed\n", STRIP_OBJS, STRIP_V, STRIP_T, STRIP_LAP_S, secs, frames);
    printf("  set triangles sent a frame: mean %u, most %u (of %u); cut at the near plane, most in a frame: %u; objects left out a frame: mean %.1f of %u\n",
           tri_sum / (frames ? frames : 1u), tri_max, STRIP_T, clipped_max, (float)culled_sum / (float)(frames ? frames : 1u), STRIP_OBJS);
    printf("  hens: %u beside the road, %u triangles each and a blob of %u; a hen on the screen in %u hen-frames of %u\n", STRIP_HENS, PSM_TOWN_HEN_T, STRIP_BLOB_N,
           hens_seen, frames * STRIP_HENS);
    printf(fails ? "hero_strip_test: %d FAILED\n" : "hero_strip_test: ok\n", fails);
    return fails != 0;
}
