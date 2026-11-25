/* SH4DER-PVR: the modifier volume spike, host test of the volume builders and two models

   demo/psm/compare/test/vol_model_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

   Checks ../vol_model.h: the meshes are closed, each builder's volume is closed (every edge used an even number of
   times), and a point is inside the volume (by the parity of the volume's triangles a ray from it crosses) exactly
   where the caster stands between it and the light. That last check is the PARITY MODEL of the PVR's inside test:
   the console says whether the PVR counts that way.

   It also prints, marked as host or model figures:
     - triangle counts of the per-triangle and the silhouette volume for the 600-triangle sphere and a box
     - host time of both builders (this machine, not an SH-4)
     - a model of reading shadow per vertex from a small picture of the scene made one frame earlier: what a vertex
       reads when it is in view, hidden behind a caster, or outside the picture.
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../vol_model.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static float sph_p[VOL_MAX_V][3], box_p[8][3];
static uint16_t sph_t[VOL_MAX_T][3], sph_e[VOL_MAX_E][4], box_t[12][3], box_e[18][4];
static uint16_t out[VOL_OUT_MAX][3];
static uint8_t side[VOL_MAX_T];
static float vp[2u * VOL_MAX_V][3];

/* Ray from o along d against triangle a b c: 1 and *t when it crosses at t > 0. */
static int ray_tri(const float o[3], const float d[3], const float a[3], const float b[3], const float c[3], float *t) {
    const float e1[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] }, e2[3] = { c[0] - a[0], c[1] - a[1], c[2] - a[2] };
    const float s[3] = { o[0] - a[0], o[1] - a[1], o[2] - a[2] };
    float h[3], q[3], det, u, v;

    vol_cross(d, e2, h);
    det = vol_dot(e1, h);
    if (fabsf(det) < 1e-12f) return 0;
    u = vol_dot(s, h) / det;
    if (u < 0.0f || u > 1.0f) return 0;
    vol_cross(s, e1, q);
    v = vol_dot(d, q) / det;
    if (v < 0.0f || u + v > 1.0f) return 0;
    *t = vol_dot(e2, q) / det;
    return *t > 1e-5f;
}

/* Every undirected edge of the n triangles is used an even number of times. */
static int closed(unsigned n, unsigned nv2) {
    static uint8_t *cnt;
    unsigned i, k, bad = 0;

    if (!cnt) cnt = malloc((size_t)(2u * VOL_MAX_V) * (2u * VOL_MAX_V));
    memset(cnt, 0, (size_t)nv2 * nv2);
    for (i = 0; i < n; i++) {
        for (k = 0; k < 3u; k++) {
            const unsigned a = out[i][k], b = out[i][(k + 1u) % 3u], lo = a < b ? a : b, hi = a < b ? b : a;

            cnt[lo * nv2 + hi] ^= 1u;
        }
    }
    for (i = 0; i < nv2 * nv2; i++) bad += cnt[i];
    return bad == 0;
}

/* Near and far vertices of the mesh for the light, far end on the plane y = -0.2. */
static void ends(const vol_mesh_t *m, const float L[3]) {
    unsigned i;

    for (i = 0; i < m->nv; i++) {
        memcpy(vp[i], m->p[i], sizeof vp[i]);
        vol_far_plane(m->p[i], L, -0.2f, vp[i + m->nv]);
    }
}

/* Points just under the ground (clear of a box's own bottom face): the volume's parity answer against "the mesh is between the point and the light". Returns
   the points where they differ; *in the points in shadow. */
static unsigned parity_check(const vol_mesh_t *m, const float L[3], unsigned n, unsigned *in) {
    const float dir[3] = { 0.1234f, 0.9f, 0.4321f };              /* any direction that is on no triangle's plane */
    unsigned bad = 0, i, t, ix, iz;

    *in = 0;
    for (iz = 0; iz < 40u; iz++) {
        for (ix = 0; ix < 40u; ix++) {
            const float o[3] = { m->p[0][0] - 6.0f + 0.211f * (float)ix, -0.01f, m->p[0][2] - 4.0f + 0.203f * (float)iz };
            unsigned cross = 0;
            int truth = 0;
            float tt;

            for (i = 0; i < n; i++) cross += (unsigned)ray_tri(o, dir, vp[out[i][0]], vp[out[i][1]], vp[out[i][2]], &tt);
            for (t = 0; t < m->nt && !truth; t++) truth = ray_tri(o, L, m->p[m->tri[t][0]], m->p[m->tri[t][1]], m->p[m->tri[t][2]], &tt);
            bad += (cross & 1u) != (unsigned)truth;
            *in += (unsigned)truth;
        }
    }
    return bad;
}

static void builders(const char *name, vol_mesh_t *m, const float L[3]) {
    unsigned lit, sil = 0, n, in, bad, cap;

    CHECK(vol_edges_build(m));
    CHECK(m->ne == 3u * m->nt / 2u);
    lit = vol_facing(m, L, side);
    ends(m, L);
    printf("  %s: %u triangles, %u vertices, %u edges; %u face the light\n", name, m->nt, m->nv, m->ne, lit);
    for (cap = 0; cap < 2u; cap++) {
        const unsigned caps = cap ? lit : m->nt - lit;

        n = vol_extrude_sil(m, side, (int)cap, out, &sil);
        CHECK(n == 2u * caps + 2u * sil);
        CHECK(closed(n, 2u * m->nv));
        bad = parity_check(m, L, n, &in);
        CHECK(bad == 0);
        printf("    silhouette, caps on the %s faces: %u volume triangles (%u silhouette edges); %u of 1600 ground points in shadow, %u wrong\n",
               cap ? "lit" : "dark", n, sil, in, bad);
        n = vol_extrude_tri(m, side, (int)cap, out);
        CHECK(n == 8u * caps);
        CHECK(closed(n, 2u * m->nv));
        bad = parity_check(m, L, n, &in);
        CHECK(bad == 0);
        printf("    per triangle, the %s faces:        %u volume triangles; %u wrong (parity inside ONE volume)\n", cap ? "lit" : "dark", n, bad);
    }
    printf("    silhouette with fan caps (one loop, caster takes no shadow): %u volume triangles (a count, not built)\n", 2u * (sil - 2u) + 2u * sil);
}

static void host_time(vol_mesh_t *m, const float L[3]) {
    enum { REPS = 20000 };
    unsigned i, n = 0;
    clock_t t0, t1, t2;

    t0 = clock();
    for (i = 0; i < REPS; i++) { vol_facing(m, L, side); n += vol_extrude_sil(m, side, 0, out, NULL); }
    t1 = clock();
    for (i = 0; i < REPS; i++) { vol_facing(m, L, side); n += vol_extrude_tri(m, side, 0, out); }
    t2 = clock();
    printf("  HOST time (this machine, not an SH-4), facing + build, sphere: silhouette %.2f us, per triangle %.2f us (%u)\n",
           1e6 * (double)(t1 - t0) / CLOCKS_PER_SEC / REPS, 1e6 * (double)(t2 - t1) / CLOCKS_PER_SEC / REPS, n);
}

/* ---- the model of a per-vertex read from last frame's picture ---------------------- */

#define MW 160
#define MH 120
#define NBOX 8
enum { SKY = 0, LIT = 1, SHADOW = 2, CASTER = 3 };
static uint8_t mask[MW * MH];

static int ray_box(const float o[3], const float d[3], const float lo[3], const float hi[3], float *t) {
    float t0 = 1e-4f, t1 = 1e30f;
    int k;

    for (k = 0; k < 3; k++) {
        if (fabsf(d[k]) < 1e-9f) { if (o[k] < lo[k] || o[k] > hi[k]) return 0; continue; }
        {
            float a = (lo[k] - o[k]) / d[k], b = (hi[k] - o[k]) / d[k];

            if (a > b) { const float s = a; a = b; b = s; }
            if (a > t0) t0 = a;
            if (b < t1) t1 = b;
            if (t0 > t1) return 0;
        }
    }
    *t = t0;
    return 1;
}

static void box_at(unsigned k, float shift, float lo[3], float hi[3]) {
    lo[0] = -7.0f + 2.0f * (float)k + shift; lo[1] = 0.0f; lo[2] = 7.6f + (float)(k % 3u) * 2.5f;
    hi[0] = lo[0] + 0.8f; hi[1] = 1.0f + 0.5f * (float)(k % 3u); hi[2] = lo[2] + 0.8f;
}

static int hits_box(const float o[3], const float d[3], float shift, float limit) {
    float lo[3], hi[3], t;
    unsigned k;

    for (k = 0; k < NBOX; k++) { box_at(k, shift, lo, hi); if (ray_box(o, d, lo, hi, &t) && t < limit) return 1; }
    return 0;
}

/* What a ray from the eye sees: the code and, on the ground, where. */
static int see(const float eye[3], const float d[3], const float L[3], float shift) {
    float tg = d[1] < -1e-6f ? -eye[1] / d[1] : 1e30f, g[3];

    if (hits_box(eye, d, shift, tg)) return CASTER;
    if (tg > 1e29f) return SKY;
    g[0] = eye[0] + d[0] * tg; g[1] = 0.0f; g[2] = eye[2] + d[2] * tg;
    return hits_box(g, L, shift, 1e30f) ? SHADOW : LIT;
}

/* The picture was made with the casters at 0; the vertices are read with the casters at `shift` (what they did in the
   frame between). */
static void lag_model(const float L[3], float shift) {
    const float eye[3] = { 0.0f, 6.0f, -6.0f }, target[3] = { 0.0f, 0.0f, 10.0f };
    unsigned x, z, vis = 0, vis_bad = 0, hid = 0, hid_caster = 0, hid_lit = 0, hid_shadow = 0, off = 0;
    vol_cam_t cam;
    int px, py;

    vol_cam_look(&cam, eye, target, 100.0f, MW * 0.5f, MH * 0.5f);
    for (py = 0; py < MH; py++) {
        for (px = 0; px < MW; px++) {
            const float sx = ((float)px + 0.5f - cam.cx) / cam.focal, sy = -((float)py + 0.5f - cam.cy) / cam.focal;
            const float d[3] = { cam.fwd[0] + sx * cam.right[0] + sy * cam.up[0], cam.fwd[1] + sx * cam.right[1] + sy * cam.up[1],
                                 cam.fwd[2] + sx * cam.right[2] + sy * cam.up[2] };

            mask[py * MW + px] = (uint8_t)see(eye, d, L, 0.0f);
        }
    }
    for (z = 0; z <= 32u; z++) {
        for (x = 0; x <= 32u; x++) {
            const float v[3] = { -16.0f + (float)x, 0.0f, (float)z };
            const float d[3] = { v[0] - eye[0], v[1] - eye[1], v[2] - eye[2] };
            const int truth = hits_box(v, L, shift, 1e30f) ? SHADOW : LIT;
            float s[3];
            int read;

            if (!vol_project(&cam, v, s) || s[0] < 0.0f || s[0] >= (float)MW || s[1] < 0.0f || s[1] >= (float)MH) { off++; continue; }
            read = mask[(int)s[1] * MW + (int)s[0]];
            if (hits_box(eye, d, shift, 0.999f)) {                 /* a caster stands between the eye and the vertex */
                hid++;
                hid_caster += read == CASTER; hid_lit += read == LIT; hid_shadow += read == SHADOW;
            } else {
                vis++;
                vis_bad += read != truth;
            }
        }
    }
    printf("    casters moved %.2f m since the picture: of 1089 ground vertices %u outside the picture; %u in view, %u of them read wrong;\n"
           "      %u hidden behind a caster: they read caster %u, lit %u, shadow %u\n", (double)shift, off, vis, vis_bad, hid, hid_caster, hid_lit, hid_shadow);
    if (shift == 0.0f) CHECK(vis_bad * 20u <= vis);                /* a still scene: only the picture's own pixel edge is wrong */
    CHECK(off + vis + hid == 1089u);
}

int main(void) {
    static const float c[3] = { 0.0f, 1.5f, 6.0f }, lo[3] = { 1.0f, 0.0f, 4.0f }, hi[3] = { 1.8f, 1.6f, 4.8f };
    float L[3] = { 0.5f, 0.8f, -0.3f };
    vol_mesh_t sph = { sph_p, sph_t, sph_e, 0, 0, 0 }, box = { box_p, box_t, box_e, 0, 0, 0 };

    vol_unit(L);
    vol_sphere(&sph, 20u, 16u, 1.0f, c);
    vol_box(&box, lo, hi);
    CHECK(sph.nt == 600u && sph.nv == 302u);
    CHECK(box.nt == 12u && box.nv == 8u);
    printf("  MODEL (parity: a point is inside when a ray from it crosses an odd number of the volume's triangles)\n");
    builders("sphere", &sph, L);
    builders("box", &box, L);
    host_time(&sph, L);
    printf("  MODEL, not a measurement: shadow read per vertex from a %d x %d picture of the scene (%d casters), taken from the camera\n", MW, MH, NBOX);
    lag_model(L, 0.0f);
    lag_model(L, 0.25f);
    lag_model(L, 1.0f);
    printf(fails ? "vol_model_test: %d FAILED\n" : "vol_model_test: ok\n", fails);
    return fails != 0;
}
