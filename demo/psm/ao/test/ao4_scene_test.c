/* SH4DER-PVR: host test and preview of demo/psm/ao/ao4_scene.h (the split-screen AO demo's scenes)

   demo/psm/ao/test/ao4_scene_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

   Checks: the scenes fit their arrays and have no strip under 3 vertices; the bake's rays against known cases; the
   AO bytes at places whose answer is known (open ground 255, the ground under a sphere 0, a wall's foot about half);
   every caster drawn one side (AO4_CULL) covers what both sides cover; the camera keeps every vertex in front.
   With a folder as its argument it also writes preview pictures there (PPM, 640 x 480): a software render of what the
   Dreamcast program is meant to show, split left | right. A PICTURE FROM THE HOST: it shows the look, it is not the
   PVR's picture and no cost.

   Build: gcc -O2 -std=c99 -Wall -Wextra -Werror -DSH4DER_CORE_NO_SH4ZAM -Iinclude -Ish4dow-dc/include
          demo/psm/ao/test/ao4_scene_test.c -lm */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "../ao4_scene.h"
#include "../ao4_draw.h"
#ifdef AO4_TEST_ROOM                        /* scene C: -I the folder of room_mesh.h, room_ao.h and hero_hens.h */
#include "../ao4_room.h"
#include "../ao4_hens.h"
#endif

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static ao4_scene_t scene;
static float tgt[3][AO4_SIZE * AO4_SIZE];  /* [2]: the movers' atlas in its top left corner */
static uint8_t img[480 * 640 * 3];
static float zb[480 * 640];
static uint32_t col[2][AO4_V_MAX];
static float scr[AO4_V_MAX][3];

static unsigned nearest_ground(float x, float z) {
    unsigned i, best = 0;
    float bd = 1e30f;

    for (i = 0; i < scene.ground_vn; i++) {
        const float d = (scene.p[i][0] - x) * (scene.p[i][0] - x) + (scene.p[i][2] - z) * (scene.p[i][2] - z);

        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

/* ---- the software target (ao4_draw.h gives the triangles; this fills them) ---- */
static unsigned tgt_which;
static void tgt_tri(void *user, const float a[2], const float b[2], const float c[2], float z, uint32_t argb, int blend) {
    float x0 = a[0], x1 = a[0], y0 = a[1], y1 = a[1];
    const float alpha = (float)(argb >> 24) * (1.0f / 255.0f), grey = (float)(argb & 255u) * (1.0f / 255.0f);
    int x, y;

    (void)user; (void)z;
    if (b[0] < x0) x0 = b[0];
    if (c[0] < x0) x0 = c[0];
    if (b[0] > x1) x1 = b[0];
    if (c[0] > x1) x1 = c[0];
    if (b[1] < y0) y0 = b[1];
    if (c[1] < y0) y0 = c[1];
    if (b[1] > y1) y1 = b[1];
    if (c[1] > y1) y1 = c[1];
    for (y = y0 < 0.0f ? 0 : (int)y0; y <= (int)y1 && y < (int)AO4_SIZE; y++)
        for (x = x0 < 0.0f ? 0 : (int)x0; x <= (int)x1 && x < (int)AO4_SIZE; x++) {
            const float px = (float)x + 0.5f, py = (float)y + 0.5f;
            const float e0 = (b[0] - a[0]) * (py - a[1]) - (b[1] - a[1]) * (px - a[0]);
            const float e1 = (c[0] - b[0]) * (py - b[1]) - (c[1] - b[1]) * (px - b[0]);
            const float e2 = (a[0] - c[0]) * (py - c[1]) - (a[1] - c[1]) * (px - c[0]);
            float *t = &tgt[tgt_which][(unsigned)y * AO4_SIZE + (unsigned)x];

            if (!((e0 >= 0.0f && e1 >= 0.0f && e2 >= 0.0f) || (e0 <= 0.0f && e1 <= 0.0f && e2 <= 0.0f))) continue;
            if (blend) *t *= 1.0f - alpha;
            else *t = grey;
        }
}

static void tgt_make(unsigned which, unsigned k, unsigned look, int exaggerate) {
    unsigned i;

    tgt_which = which;
    for (i = 0; i < AO4_SIZE * AO4_SIZE; i++) tgt[which][i] = 1.0f;
    if (k == 99u) {     /* a look that is not built: 32 faint directions at 4 reaches, what a jitter over frames would add up to */
        unsigned j;

        ao4_draw_sun(&scene, look, tgt_tri, NULL);
        for (j = 0; j < 32u; j++) {
            const float ang = 6.2831853f * ((float)j + 0.5f) / 32.0f, reach = AO_COT_EL * sqrtf(((float)(j % 4u) + 0.5f) / 4.0f);
            const float slope[2] = { reach * cosf(ang), reach * sinf(ang) };
            const float a = 1.0f - powf(AO_FLOOR, 1.0f / 32.0f) * 1.0f;

            ao4_draw_dir(&scene, slope, 1.5f, (uint32_t)((exaggerate ? 2.0f : 1.0f) * a * 255.0f + 0.5f) << 24, AO4_CULL, 1, tgt_tri, NULL);
        }
    } else
    ao4_draw_map(&scene, k, look, exaggerate, ao4_reach[AO4_REACH_DEFAULT], tgt_tri, NULL);
    for (i = 0; i < AO4_SIZE; i++) tgt[which][i] = tgt[which][(AO4_SIZE - 1u) * AO4_SIZE + i] = tgt[which][i * AO4_SIZE] = tgt[which][i * AO4_SIZE + AO4_SIZE - 1u] = 1.0f;
}

static float tgt_at(unsigned which, float u, float v) {
    const float x = u * (float)AO4_SIZE - 0.5f, y = v * (float)AO4_SIZE - 0.5f;
    int x0 = (int)floorf(x), y0 = (int)floorf(y), x1, y1;
    const float fx = x - (float)x0, fy = y - (float)y0;

    x1 = x0 + 1; y1 = y0 + 1;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (int)AO4_SIZE - 1) x1 = (int)AO4_SIZE - 1;
    if (y1 > (int)AO4_SIZE - 1) y1 = (int)AO4_SIZE - 1;
    if (x0 > x1) x0 = x1;
    if (y0 > y1) y0 = y1;
    return (tgt[which][y0 * (int)AO4_SIZE + x0] * (1.0f - fx) + tgt[which][y0 * (int)AO4_SIZE + x1] * fx) * (1.0f - fy)
         + (tgt[which][y1 * (int)AO4_SIZE + x0] * (1.0f - fx) + tgt[which][y1 * (int)AO4_SIZE + x1] * fx) * fy;
}

/* ---- the software screen ---- */
static int lm_surf = -1, lm_x2;             /* a lightmap's quad is being drawn: its surface */
static uint32_t lm_offset;
static void scr_tri(unsigned a, unsigned b, unsigned c, const uint32_t *argb, int ground, unsigned which, int xlo, int xhi) {
    const float *A = scr[a], *B = scr[b], *C = scr[c];
    const float area = (B[0] - A[0]) * (C[1] - A[1]) - (C[0] - A[0]) * (B[1] - A[1]);
    float x0 = A[0], x1 = A[0], y0 = A[1], y1 = A[1], ia;
    int x, y, k;

    if (area > -1e-6f && area < 1e-6f) return;
    ia = 1.0f / area;
    if (B[0] < x0) x0 = B[0];
    if (C[0] < x0) x0 = C[0];
    if (B[0] > x1) x1 = B[0];
    if (C[0] > x1) x1 = C[0];
    if (B[1] < y0) y0 = B[1];
    if (C[1] < y0) y0 = C[1];
    if (B[1] > y1) y1 = B[1];
    if (C[1] > y1) y1 = C[1];
    for (y = y0 < 0.0f ? 0 : (int)y0; y <= (int)y1 && y < 480; y++)
        for (x = x0 < (float)xlo ? xlo : (int)x0; x <= (int)x1 && x < xhi; x++) {
            const float px = (float)x + 0.5f, py = (float)y + 0.5f;
            const float w0 = ((B[0] - px) * (C[1] - py) - (C[0] - px) * (B[1] - py)) * ia;
            const float w1 = ((C[0] - px) * (A[1] - py) - (A[0] - px) * (C[1] - py)) * ia;
            const float w2 = 1.0f - w0 - w1;
            float z, m = 1.0f;

            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;
            z = w0 * A[2] + w1 * B[2] + w2 * C[2];
            if (z <= zb[y * 640 + x]) continue;
            zb[y * 640 + x] = z;
            if (ground) {                                       /* perspective-correct world x, z, then the target */
                const float iz = 1.0f / z;
                const float wx = (w0 * scene.p[a][0] * A[2] + w1 * scene.p[b][0] * B[2] + w2 * scene.p[c][0] * C[2]) * iz;
                const float wz = (w0 * scene.p[a][2] * A[2] + w1 * scene.p[b][2] * B[2] + w2 * scene.p[c][2] * C[2]) * iz;

                m = tgt_at(which, ao4_ground_u(wx), ao4_ground_v(wz));
            }
#ifdef AO4_TEST_ROOM
            if (lm_surf >= 0) {                                 /* the lightmap at this pixel's world point, bilinear, clamped */
                const room_surf_t *f = &room_surf[lm_surf];
                const float iz = 1.0f / z;
                float w[3], u, v, fx, fy;
                int i0, j0, i1, j1;
                const uint8_t *t = room_lm[lm_surf];

                for (k = 0; k < 3; k++) w[k] = (w0 * scene.p[a][k] * A[2] + w1 * scene.p[b][k] * B[2] + w2 * scene.p[c][k] * C[2]) * iz - f->o[k];
                u = sh4der_dot3_(w, f->du) / sh4der_dot3_(f->du, f->du) * (float)f->w - 0.5f;
                v = sh4der_dot3_(w, f->dv) / sh4der_dot3_(f->dv, f->dv) * (float)f->h - 0.5f;
                i0 = (int)floorf(u); j0 = (int)floorf(v); fx = u - (float)i0; fy = v - (float)j0; i1 = i0 + 1; j1 = j0 + 1;
                if (i0 < 0) i0 = 0;
                if (j0 < 0) j0 = 0;
                if (i1 > f->w - 1) i1 = f->w - 1;
                if (j1 > f->h - 1) j1 = f->h - 1;
                if (i0 > i1) i0 = i1;
                if (j0 > j1) j0 = j1;
                m = (ao4_open(t[j0 * f->w + i0], lm_x2) * (1.0f - fx) + ao4_open(t[j0 * f->w + i1], lm_x2) * fx) * (1.0f - fy)
                  + (ao4_open(t[j1 * f->w + i0], lm_x2) * (1.0f - fx) + ao4_open(t[j1 * f->w + i1], lm_x2) * fx) * fy;
            }
#endif
            for (k = 0; k < 3; k++) {
                const float ca = (float)((argb[a] >> (16 - 8 * k)) & 255u), cb = (float)((argb[b] >> (16 - 8 * k)) & 255u), cc = (float)((argb[c] >> (16 - 8 * k)) & 255u);
                float v = (w0 * ca + w1 * cb + w2 * cc) * m + (lm_surf >= 0 ? (float)((lm_offset >> (16 - 8 * k)) & 255u) : 0.0f);

                img[(y * 640 + x) * 3 + k] = (uint8_t)(v < 0.0f ? 0.0f : v > 255.0f ? 255.0f : v);
            }
        }
}

#ifdef AO4_TEST_ROOM
static ao4_hens_t hens;
static float mq_lo[2], mq_hi[2];
static unsigned mq_n;
static void mq_box(void *user, const float a[2], const float b[2], const float c[2], float z, uint32_t argb, int blend) {
    const float *p[3] = { a, b, c };
    unsigned i, k;

    (void)user; (void)z; (void)argb; (void)blend;
    for (i = 0; i < 3u; i++) for (k = 0; k < 2u; k++) { if (p[i][k] < mq_lo[k]) mq_lo[k] = p[i][k]; if (p[i][k] > mq_hi[k]) mq_hi[k] = p[i][k]; }
    mq_n++;
}

/* A hen's quad over the picture: what is there x its tile, where the quad is not behind what is drawn. */
static void mq_draw(const ao4_cam_t *cam, unsigned k, int xlo, int xhi) {
    static const uint8_t tri[2][3] = { { 0, 1, 2 }, { 0, 2, 3 } };
    float sp[4][3], uv[4][2], p[3];
    unsigned t, c;
    int x, y;

    for (c = 0; c < 4u; c++) { ao4_hen_quad(&hens, k, c, p, uv[c]); ao4_cam_pt(cam, p, sp[c]); }
    for (t = 0; t < 2u; t++) {
        const float *A = sp[tri[t][0]], *B = sp[tri[t][1]], *C = sp[tri[t][2]];
        const float *ua = uv[tri[t][0]], *ub = uv[tri[t][1]], *uc = uv[tri[t][2]];
        const float area = (B[0] - A[0]) * (C[1] - A[1]) - (C[0] - A[0]) * (B[1] - A[1]), ia = 1.0f / area;

        for (y = 0; y < 480; y++)
            for (x = xlo; x < xhi; x++) {
                const float px = (float)x + 0.5f, py = (float)y + 0.5f;
                const float w0 = ((B[0] - px) * (C[1] - py) - (C[0] - px) * (B[1] - py)) * ia, w1 = ((C[0] - px) * (A[1] - py) - (A[0] - px) * (C[1] - py)) * ia, w2 = 1.0f - w0 - w1;
                float z, u, v, m;

                if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;
                z = w0 * A[2] + w1 * B[2] + w2 * C[2];
                if (z < zb[y * 640 + x]) continue;
                u = (w0 * ua[0] * A[2] + w1 * ub[0] * B[2] + w2 * uc[0] * C[2]) / z; v = (w0 * ua[1] * A[2] + w1 * ub[1] * B[2] + w2 * uc[1] * C[2]) / z;
                m = tgt_at(2u, u * (float)AO4_HEN_ATLAS / (float)AO4_SIZE, v * (float)AO4_HEN_ATLAS / (float)AO4_SIZE);
                for (c = 0; c < 3u; c++) img[(y * 640 + x) * 3 + (int)c] = (uint8_t)((float)img[(y * 640 + x) * 3 + (int)c] * m);
            }
    }
}
#endif

/* One preview: left and right methods, the look, exaggerate, k directions, the camera at time t. */
static void preview(const char *dir, const char *name, unsigned left, unsigned right, unsigned look, int exaggerate, unsigned k, float t) {
    const unsigned method[2] = { left, right };
    ao4_cam_t cam;
    char path[512];
    unsigned side, i, sidx, st;
    FILE *f;

    ao4_cam_at(&scene, &cam, t, 0.0f, 0.0f);
    for (i = 0; i < scene.vn; i++) ao4_cam_pt(&cam, scene.p[i], scr[i]);
    for (i = 0; i < 480u * 640u; i++) {
        zb[i] = 0.0f;
        img[i * 3u] = (uint8_t)(AO4_BACKDROP_R * 255.0f); img[i * 3u + 1u] = (uint8_t)(AO4_BACKDROP_G * 255.0f); img[i * 3u + 2u] = (uint8_t)(AO4_BACKDROP_B * 255.0f);
    }
    for (side = 0; side < 2u; side++) {
        tgt_make(side, method[side] == AO4_MAP ? k : 0u, look, exaggerate);
        ao4_light(&scene, method[side], look, exaggerate, col[side]);
#ifdef AO4_TEST_ROOM
        if (method[side] >= AO4_LMAP) {     /* each flat surface as one quad: four more vertices at the arrays' end */
            unsigned sf, c;

            for (sf = 0; sf < ROOM_SURFS; sf++) {
                const unsigned q = AO4_V_MAX - 4u;
                uint32_t base;
                float uv[2];

                ao4_room_colours(&scene, sf, look, &base, &lm_offset);
                for (c = 0; c < 4u; c++) { ao4_room_quad(sf, c, scene.p[q + c], uv); ao4_cam_pt(&cam, scene.p[q + c], scr[q + c]); col[side][q + c] = base; }
                lm_surf = (int)sf; lm_x2 = exaggerate;
                scr_tri(q, q + 1u, q + 2u, col[side], 0, side, side ? 320 : 0, side ? 640 : 320);
                scr_tri(q, q + 2u, q + 3u, col[side], 0, side, side ? 320 : 0, side ? 640 : 320);
                lm_surf = -1;
            }
        }
#endif
        for (sidx = 0, st = 0; st < scene.strips; sidx += scene.slen[st], st++)
            for (i = 0; i + 2u < scene.slen[st] && !(method[side] >= AO4_LMAP && st < scene.surf_strips); i++)
                scr_tri(scene.idx[sidx + i], scene.idx[sidx + i + 1u], scene.idx[sidx + i + 2u], col[side], st < scene.ground_strips, side, side ? 320 : 0, side ? 640 : 320);
    }
#ifdef AO4_TEST_ROOM
    for (side = 0; side < 2u; side++)
        if (method[side] == AO4_LMAP_M) {   /* after everything opaque, as the translucent list is */
            tgt_which = 2u;
            for (i = 0; i < AO4_SIZE * AO4_SIZE; i++) tgt[2][i] = 1.0f;
            ao4_hens_tiles(&scene, &hens, ao_count(k), exaggerate, ao4_reach[AO4_REACH_DEFAULT], tgt_tri, NULL);
            for (i = 0; i < AO4_HENS; i++) mq_draw(&cam, i, side ? 320 : 0, side ? 640 : 320);
        }
#endif
    for (i = 0; i < 480u; i++) { img[(i * 640u + 319u) * 3u] = img[(i * 640u + 319u) * 3u + 1u] = img[(i * 640u + 319u) * 3u + 2u] = 20u; memcpy(&img[(i * 640u + 320u) * 3u], &img[(i * 640u + 319u) * 3u], 3u); }
    snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
    f = fopen(path, "wb");
    if (!f) { printf("cannot write %s\n", path); return; }
    fprintf(f, "P6\n640 480\n255\n");
    fwrite(img, 1, sizeof img, f);
    fclose(f);
}

/* every caster drawn one side covers what both sides cover, from the sun and from 8 sky directions */
static uint8_t cover_bits[3][AO4_SIZE * AO4_SIZE];
static unsigned cover_which, cover_bit;
static void cover_tri(void *user, const float a[2], const float b[2], const float c[2], float z, uint32_t argb, int blend) {
    float x0 = a[0], x1 = a[0], y0 = a[1], y1 = a[1];
    int x, y;

    (void)user; (void)z; (void)argb; (void)blend;
    if (b[0] < x0) x0 = b[0];
    if (c[0] < x0) x0 = c[0];
    if (b[0] > x1) x1 = b[0];
    if (c[0] > x1) x1 = c[0];
    if (b[1] < y0) y0 = b[1];
    if (c[1] < y0) y0 = c[1];
    if (b[1] > y1) y1 = b[1];
    if (c[1] > y1) y1 = c[1];
    for (y = y0 < 0.0f ? 0 : (int)y0; y <= (int)y1 && y < (int)AO4_SIZE; y++)
        for (x = x0 < 0.0f ? 0 : (int)x0; x <= (int)x1 && x < (int)AO4_SIZE; x++) {
            const float px = (float)x + 0.5f, py = (float)y + 0.5f;
            const float e0 = (b[0] - a[0]) * (py - a[1]) - (b[1] - a[1]) * (px - a[0]);
            const float e1 = (c[0] - b[0]) * (py - b[1]) - (c[1] - b[1]) * (px - b[0]);
            const float e2 = (a[0] - c[0]) * (py - c[1]) - (a[1] - c[1]) * (px - c[0]);

            if ((e0 >= 0.0f && e1 >= 0.0f && e2 >= 0.0f) || (e0 <= 0.0f && e1 <= 0.0f && e2 <= 0.0f)) cover_bits[cover_which][(unsigned)y * AO4_SIZE + (unsigned)x] |= (uint8_t)(1u << cover_bit);
        }
}

static void t_casters(unsigned which) {
    unsigned d, i, n[3], differ[3] = { 0, 0, 0 };
    static const int cull[3] = { 0, 1, -1 };

    memset(cover_bits, 0, sizeof cover_bits);
    for (cover_which = 0; cover_which < 3u; cover_which++) {
        n[cover_which] = 0;
        for (d = 0; d < 8u; d++) {
            float slope[2];

            ao_slope(8u, d, slope);
            cover_bit = d;
            n[cover_which] += ao4_draw_dir(&scene, slope, 1.0f, 0u, cull[cover_which], 1, cover_tri, NULL);
        }
    }
    for (i = 0; i < AO4_SIZE * AO4_SIZE; i++) {
        const unsigned x = i % AO4_SIZE, y = i / AO4_SIZE;
        int e = x == 0u || y == 0u || x == AO4_SIZE - 1u || y == AO4_SIZE - 1u, dx, dy;

        for (dy = -1; !e && dy <= 1; dy++) for (dx = -1; dx <= 1; dx++) if (cover_bits[0][i + (unsigned)(dy * (int)AO4_SIZE + dx)] != cover_bits[0][i]) e = 1;
        if (e) continue;
        differ[1] += cover_bits[1][i] != cover_bits[0][i];
        differ[2] += cover_bits[2][i] != cover_bits[0][i];
    }
    printf("  scene %c casters: %u triangles, %u vertices; one side sends %u (side 1) / %u (side -1) of %u a direction x 8; texels that differ %u / %u\n",
           'A' + which, scene.ctn, scene.cvn, n[1], n[2], n[0], differ[1], differ[2]);
    CHECK(differ[AO4_CULL > 0 ? 1 : 2] == 0u, "scene %c: side AO4_CULL differs from both sides at %u texels", 'A' + which, differ[AO4_CULL > 0 ? 1 : 2]);
    CHECK(which >= AO4_SCENES || (n[1] + n[2] == n[0] && n[AO4_CULL > 0 ? 1 : 2] * 2u <= n[0] + 16u), "scene %c: one side is not about half", 'A' + which);
}

int main(int argc, char **argv) {
    float dirs[AO4_RAYS][3];
    unsigned which, i, sidx, st;

    ao4_rays(dirs);
    {
        float sum[3] = { 0, 0, 0 };

        for (i = 0; i < AO4_RAYS; i++) {
            const float l = sqrtf(dirs[i][0] * dirs[i][0] + dirs[i][1] * dirs[i][1] + dirs[i][2] * dirs[i][2]);

            CHECK(fabsf(l - 1.0f) < 1e-5f && dirs[i][1] > 0.0f, "ray %u is not a unit vector in the upper half", i);
            sum[0] += dirs[i][0]; sum[1] += dirs[i][1]; sum[2] += dirs[i][2];
        }
        CHECK(fabsf(sum[0]) < 2.0f && fabsf(sum[2]) < 2.0f && fabsf(sum[1] / (float)AO4_RAYS - 2.0f / 3.0f) < 0.02f, "the ray set is not cosine-weighted: %f %f %f", sum[0], sum[1], sum[2]);
    }
    for (which = 0; which < AO4_SCENES; which++) {
        unsigned lo = 255, hi = 0, far_ground, cam_bad = 0;
        double mean = 0.0;
        float t;

        ao4_build(&scene, which);
        CHECK(scene.vn > scene.ground_vn && scene.vn < AO4_V_MAX && scene.idxn < AO4_IDX_MAX && scene.strips < AO4_STRIP_MAX && scene.cvn < AO4_CV_MAX && scene.ctn < AO4_CT_MAX,
              "scene %c overflows: v %u idx %u strips %u cv %u ct %u", 'A' + which, scene.vn, scene.idxn, scene.strips, scene.cvn, scene.ctn);
        CHECK(scene.ground_vn == (AO4_GRID + 1u) * (AO4_GRID + 1u) && scene.ground_strips == AO4_GRID, "the ground");
        for (sidx = 0, st = 0; st < scene.strips; sidx += scene.slen[st], st++) {
            CHECK(scene.slen[st] >= 3u, "a short strip");
            for (i = 0; i < scene.slen[st]; i++) CHECK(scene.idx[sidx + i] < scene.vn, "an index past the vertices");
        }
        CHECK(sidx == scene.idxn, "strip lengths do not add up");
        for (i = 0; i < scene.vn; i++) CHECK(fabsf(sh4der_dot3_(scene.n[i], scene.n[i]) - 1.0f) < 1e-3f, "normal %u", i);
        CHECK(fabsf(sh4der_dot3_(scene.sun, scene.sun) - 1.0f) < 1e-5f && scene.sun[1] > 0.5f, "the sun");
        ao4_bake(&scene, NULL);
        for (i = 0; i < scene.vn; i++) { if (scene.ao[i] < lo) lo = scene.ao[i]; if (scene.ao[i] > hi) hi = scene.ao[i]; mean += scene.ao[i]; }
        far_ground = nearest_ground(5.5f, -5.5f);
        printf("  scene %c: %u vertices (%u ground), %u strips, %u triangles on screen; AO byte min %u mean %.0f max %u; open ground %u\n", 'A' + which, scene.vn,
               scene.ground_vn, scene.strips, scene.tris, lo, mean / scene.vn, hi, scene.ao[far_ground]);
        CHECK(scene.ao[far_ground] == 255u && hi == 255u, "open ground is not open");
        if (which == 0u) {
            const unsigned under = nearest_ground(-1.0f, 0.4f), beside = nearest_ground(-1.0f + 2.6f, 0.4f);

            CHECK(scene.ao[under] < 40u, "the ground under the big sphere: %u", scene.ao[under]);
            CHECK(scene.ao[beside] > 100u && scene.ao[beside] < 250u, "the ground beside the big sphere: %u", scene.ao[beside]);
            printf("    ground under the big sphere %u, 2.6 m from its middle %u\n", scene.ao[under], scene.ao[beside]);
        } else {
            const unsigned foot = nearest_ground(-4.0f, 0.0f), corner = nearest_ground(-4.0f, 4.0f), mid = nearest_ground(1.0f, -1.0f);

            CHECK(scene.ao[foot] > 90u && scene.ao[foot] < 170u, "the ground at a wall's foot: %u", scene.ao[foot]);
            CHECK(scene.ao[corner] < scene.ao[foot], "the corner is not darker than a wall's foot: %u %u", scene.ao[corner], scene.ao[foot]);
            CHECK(scene.ao[mid] > 235u, "the open floor: %u", scene.ao[mid]);
            printf("    ground at a wall's foot %u, in the corner %u, on the open floor %u\n", scene.ao[foot], scene.ao[corner], scene.ao[mid]);
        }
        t_casters(which);
        for (t = 0.0f; t < 40.0f; t += 0.5f) {                      /* the camera keeps everything in front */
            ao4_cam_t cam;
            float s[3];

            ao4_cam_at(&scene, &cam, t, 0.0f, 0.0f);
            for (i = 0; i < scene.vn; i++) cam_bad += !ao4_cam_pt(&cam, scene.p[i], s);
            ao4_cam_at(&scene, &cam, t, 0.6f, -4.5f);
            for (i = 0; i < scene.vn; i++) cam_bad += !ao4_cam_pt(&cam, scene.p[i], s);
        }
        CHECK(cam_bad == 0u, "scene %c: %u vertices came within 0.5 m of the eye", 'A' + which, cam_bad);
        CHECK(ao4_shadow_share(&scene, AO4_AO_ONLY) == 1.0f && ao4_shadow_share(&scene, AO4_CLAY) < 0.85f, "the shadow's share");
        if (argc > 1) {
            char name[64];

            snprintf(name, sizeof name, "ao4_%c_baked_map", 'a' + which); preview(argv[1], name, AO4_BAKED, AO4_MAP, AO4_CLAY, 0, 4u, 3.0f);
            snprintf(name, sizeof name, "ao4_%c_off_baked", 'a' + which); preview(argv[1], name, AO4_OFF, AO4_BAKED, AO4_CLAY, 0, 4u, 3.0f);
            snprintf(name, sizeof name, "ao4_%c_off_map", 'a' + which); preview(argv[1], name, AO4_OFF, AO4_MAP, AO4_CLAY, 0, 4u, 3.0f);
            snprintf(name, sizeof name, "ao4_%c_baked_map_x2", 'a' + which); preview(argv[1], name, AO4_BAKED, AO4_MAP, AO4_CLAY, 1, 4u, 3.0f);
            snprintf(name, sizeof name, "ao4_%c_aoonly_baked_map8", 'a' + which); preview(argv[1], name, AO4_BAKED, AO4_MAP, AO4_AO_ONLY, 0, 8u, 3.0f);
            snprintf(name, sizeof name, "ao4_%c_try32_baked_map", 'a' + which); preview(argv[1], name, AO4_BAKED, AO4_MAP, AO4_CLAY, 0, 99u, 3.0f);
            snprintf(name, sizeof name, "ao4_%c_try32_aoonly", 'a' + which); preview(argv[1], name, AO4_BAKED, AO4_MAP, AO4_AO_ONLY, 0, 99u, 3.0f);
            snprintf(name, sizeof name, "ao4_%c_colour_off_baked", 'a' + which); preview(argv[1], name, AO4_OFF, AO4_BAKED, AO4_COLOUR, 0, 4u, 9.0f);
        }
    }
#ifdef AO4_TEST_ROOM
    {                                       /* scene C: the room, and the hens in it */
        unsigned cam_bad = 0, k, lo = 255, n, out = 0, pen_out = 0, tile_out = 0;
        double mean = 0.0;
        float t, far[AO4_HENS] = { 0, 0, 0, 0 }, x0[AO4_HENS], z0[AO4_HENS];

        ao4_build_room(&scene);
        CHECK(scene.vn == ROOM_V && scene.tris == ROOM_T && scene.strips < ROOM_T && scene.ctn == ROOM_CT && scene.cvn == ROOM_CV, "the room's counts");
        CHECK(scene.ground_strips > 0u && scene.surf_strips > scene.ground_strips && scene.surf_strips < scene.strips, "the room's surfaces come first");
        for (i = 0; i < scene.ground_vn; i++) CHECK(fabsf(scene.p[i][1]) < 1e-4f && fabsf(scene.p[i][0]) <= AO4_SPAN && fabsf(scene.p[i][2]) <= AO4_SPAN, "floor vertex %u", i);
        for (i = 0; i < scene.vn; i++) { CHECK(fabsf(sh4der_dot3_(scene.n[i], scene.n[i]) - 1.0f) < 1e-3f, "room normal %u", i); if (scene.ao[i] < lo) lo = scene.ao[i]; mean += scene.ao[i]; }
        for (i = 0, n = 0; i < scene.ground_vn; i++) n += scene.ao[i] > 235u;
        printf("  scene C (the room): %u vertices (%u floor), %u triangles on screen, %u flat-surface strips, %u caster triangles; AO byte min %u mean %.0f; floor vertices open (over 235) %u\n",
               scene.vn, scene.ground_vn, scene.tris, scene.surf_strips, scene.ctn, lo, mean / scene.vn, n);
        CHECK(n > scene.ground_vn / 8u && n < scene.ground_vn, "the floor's AO: %u of %u open", n, scene.ground_vn);
        t_casters(2u);
        CHECK(ao4_hens_add(&scene, &hens), "the hens do not fit");
        CHECK(scene.tris == ROOM_T + AO4_HEN_TRIS && scene.ctn == ROOM_CT + AO4_HEN_CTRIS, "the hens' counts");
        for (k = 0; k < AO4_HENS; k++) ao4_hen_at(&hens.h[k], &x0[k], &z0[k]);
        for (t = 0.0f; t < 120.0f; t += 1.0f / 50.0f) {             /* two minutes at 50 frames a second */
            ao4_hens_step(&hens, 1.0f / 50.0f);
            for (k = 0; k < AO4_HENS; k++) {
                float x, z, d;

                ao4_hen_at(&hens.h[k], &x, &z);
                out += !(fabsf(x) < 2.9f && fabsf(z) < 2.9f);
                d = sqrtf((x - x0[k]) * (x - x0[k]) + (z - z0[k]) * (z - z0[k]));
                if (d > far[k]) far[k] = d;
                if (k == 0u && ((unsigned)(t * 50.0f + 0.5f) % 25u) == 0u) {     /* twice a second: every hen's triangles stay in its tile, 8 directions, the longest reach */
                    unsigned q;

                    ao4_hens_place(&scene, &hens);
                    for (q = 0; q < AO4_HENS; q++) {
                        ao4_hens_t one = hens;
                        float tx, ty;
                        unsigned r;

                        for (r = 0; r < AO4_HENS; r++) if (r != q) { one.h[r].x = 1.0e4f; }      /* the others' land far outside and are dropped */
                        mq_lo[0] = mq_lo[1] = 1e30f; mq_hi[0] = mq_hi[1] = -1e30f; mq_n = 0;
                        ao4_hens_tiles(&scene, &one, 8u, 0, 1.0f, mq_box, NULL);
                        ao4_hen_tile(q, &tx, &ty);
                        tile_out += !(mq_n > 0u && mq_lo[0] >= tx + 1.0f && mq_lo[1] >= ty + 1.0f && mq_hi[0] <= tx + (float)AO4_HEN_TILE - 1.0f && mq_hi[1] <= ty + (float)AO4_HEN_TILE - 1.0f);
                    }
                }
                if (k < 2u) pen_out += !(x >= -0.85f + AO4_HEN_DX - 1e-3f && x <= 1.85f + AO4_HEN_DX + 1e-3f && z >= 7.9f - AO4_HEN_DZ - 1e-3f && z <= 9.1f - AO4_HEN_DZ + 1e-3f);
            }
        }
        ao4_hens_place(&scene, &hens);
        printf("    hens: %u triangles, %u caster triangles; farthest from home in 120 s: %.2f %.2f %.2f %.2f m (two in the pen, two loose)\n", (unsigned)AO4_HEN_TRIS,
               (unsigned)AO4_HEN_CTRIS, far[0], far[1], far[2], far[3]);
        CHECK(out == 0u && pen_out == 0u, "a hen left the room (%u) or its pen (%u)", out, pen_out);
        CHECK(tile_out == 0u, "a hen's caster left its tile %u times", tile_out);
        CHECK(far[0] < 3.0f && far[1] < 3.0f && far[2] > 1.0f && far[3] > 1.0f && far[0] > 0.2f, "the walks");
        for (i = hens.v0; i < scene.vn; i++) CHECK(fabsf(sh4der_dot3_(scene.n[i], scene.n[i]) - 1.0f) < 0.03f && scene.p[i][1] > -0.05f && scene.p[i][1] < 0.6f, "hen vertex %u", i);
        t_casters(2u);
        for (t = 0.0f; t < 40.0f; t += 0.5f) {
            ao4_cam_t cam;
            float sp[3];

            ao4_cam_at(&scene, &cam, t, 0.6f, -4.5f);
            for (i = 0; i < scene.vn; i++) cam_bad += !ao4_cam_pt(&cam, scene.p[i], sp);
        }
        CHECK(cam_bad == 0u, "scene C: %u vertices came within 0.5 m of the eye", cam_bad);
        if (argc > 1) {
            preview(argv[1], "ao4_c_baked_lmap_hens", AO4_BAKED, AO4_LMAP, AO4_CLAY, 0, 4u, 0.0f);
            preview(argv[1], "ao4_c_lmap_lmapm_hens", AO4_LMAP, AO4_LMAP_M, AO4_CLAY, 0, 4u, 0.0f);
            preview(argv[1], "ao4_c_lmapm_map_hens", AO4_LMAP_M, AO4_MAP, AO4_CLAY, 0, 4u, 0.0f);
            preview(argv[1], "ao4_c_lmapm_map_hens_x2", AO4_LMAP_M, AO4_MAP, AO4_CLAY, 1, 8u, 0.0f);
            preview(argv[1], "ao4_c_lmap_map_hens", AO4_LMAP, AO4_MAP, AO4_CLAY, 0, 4u, 0.0f);
            preview(argv[1], "ao4_c_lmap_map8_hens_aoonly", AO4_LMAP, AO4_MAP, AO4_AO_ONLY, 0, 8u, 0.0f);
            ao4_hens_show(&scene, &hens, 0);
            preview(argv[1], "ao4_c_off_baked", AO4_OFF, AO4_BAKED, AO4_CLAY, 0, 4u, 0.0f);
            preview(argv[1], "ao4_c_off_lmap", AO4_OFF, AO4_LMAP, AO4_CLAY, 0, 4u, 0.0f);
            preview(argv[1], "ao4_c_baked_lmap_x2", AO4_BAKED, AO4_LMAP, AO4_CLAY, 1, 4u, 4.0f);
            preview(argv[1], "ao4_c_baked_lmap_colour", AO4_BAKED, AO4_LMAP, AO4_COLOUR, 0, 4u, 0.0f);
        }
    }
#endif
    CHECK(ao4_open(255u, 0) == 1.0f && ao4_open(255u, 1) == 1.0f && ao4_open(0u, 1) == 0.0f && fabsf(ao4_open(191u, 1) - 0.498f) < 0.01f && ao4_open(100u, 1) == 0.0f, "ao4_open");
    CHECK(ao4_map_argb(4u, 0) == ao_argb(4u, 1.0f) && (ao4_map_argb(4u, 1) >> 24) == 81u && (ao4_map_argb(1u, 1) >> 24) == 255u, "ao4_map_argb");
    if (fails) { printf("ao4_scene_test: %d FAILED\n", fails); return 1; }
    printf("ao4_scene_test: all passed\n");
    return 0;
}
