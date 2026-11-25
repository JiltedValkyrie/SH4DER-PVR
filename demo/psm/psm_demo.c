/* SH4DER-PVR: software perspective shadow map demo

   demo/psm/psm_demo.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../LICENSE.

   A host program, no console. It draws one scene twice, with the same shadow map size: a
   uniform map and a perspective shadow map (include/sh4der_psm.h), through a real depth
   compare with the chapter's world-space bias and a 2 x 2 PCF. It writes the two pictures,
   the two maps and the numbers that matter (ground a texel covers against ground a pixel
   covers) to a folder, for demo/psm/tools/sheet.py to join.

   The scene: ground, a row of poles to 45 m, two copies of Spot the cow (CC0, fetched, see
   CREDITS.md). Poles at many depths are what shows perspective aliasing.

   Usage: psm_demo [--obj spot.obj] [--out dir] [--map 256] [--size 320x240] [--az 120] [--el 35]
                   [--yaw 0] [--warp-near 4] [--fit-near 2] [--bias-a 0.02] [--bias-b 0] [--nopcf] [--bench]
   az: where the sun is, 0 straight ahead (toward the camera), 90 on the right, 180 behind the camera.
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/sh4der_psm.h"
#include "../../include/sh4der_psm_raster.h"
#include "../../include/sh4der_psm_pvr.h"
#include <time.h>

/* ---- scene ---------------------------------------------------------------- */

typedef struct { float v[3][3]; float col[3]; } tri_t;

static tri_t *tris;
static int ntri, captri;

static void add_tri(const float a[3], const float b[3], const float c[3], const float col[3]) {
    tri_t *t;

    if (ntri == captri) {
        captri = captri ? captri * 2 : 4096;
        tris = (tri_t *)realloc(tris, (size_t)captri * sizeof *tris);
        if (!tris) { fprintf(stderr, "out of memory\n"); exit(2); }
    }
    t = &tris[ntri++];
    memcpy(t->v[0], a, 12); memcpy(t->v[1], b, 12); memcpy(t->v[2], c, 12);
    memcpy(t->col, col, 12);
}

static void add_box(float cx, float cz, float w, float h, const float col[3]) {
    const float x0 = cx - w * 0.5f, x1 = cx + w * 0.5f, z0 = cz - w * 0.5f, z1 = cz + w * 0.5f;
    const float p[8][3] = { { x0, 0, z0 }, { x1, 0, z0 }, { x1, 0, z1 }, { x0, 0, z1 }, { x0, h, z0 }, { x1, h, z0 }, { x1, h, z1 }, { x0, h, z1 } };
    static const int f[12][3] = { { 0, 1, 5 }, { 0, 5, 4 }, { 1, 2, 6 }, { 1, 6, 5 }, { 2, 3, 7 }, { 2, 7, 6 }, { 3, 0, 4 }, { 3, 4, 7 },
                                  { 4, 5, 6 }, { 4, 6, 7 }, { 0, 3, 2 }, { 0, 2, 1 } };
    int i;

    for (i = 0; i < 12; i++) add_tri(p[f[i][0]], p[f[i][1]], p[f[i][2]], col);
}

/* A Wavefront OBJ (v and f lines only), stood on the ground at (x, z), `size` metres long, turned `yaw` radians. */
static int add_obj(const char *path, float x, float z, float size, float yaw, const float col[3]) {
    FILE *f = fopen(path, "r");
    char line[256];
    float (*v)[3] = NULL;
    int nv = 0, capv = 0, nt0 = ntri, i;
    float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f }, s, c, sn;

    if (!f) return 0;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == 'v' && line[1] == ' ') {
            if (nv == capv) { capv = capv ? capv * 2 : 4096; v = (float (*)[3])realloc(v, (size_t)capv * sizeof *v); }
            if (sscanf(line + 2, "%f %f %f", &v[nv][0], &v[nv][1], &v[nv][2]) == 3) nv++;
        }
    }
    for (i = 0; i < nv; i++) {
        int k;
        for (k = 0; k < 3; k++) { if (v[i][k] < lo[k]) lo[k] = v[i][k]; if (v[i][k] > hi[k]) hi[k] = v[i][k]; }
    }
    s = size / (hi[2] - lo[2] > hi[0] - lo[0] ? hi[2] - lo[2] : hi[0] - lo[0]);
    c = cosf(yaw); sn = sinf(yaw);
    rewind(f);
    while (fgets(line, sizeof line, f)) {
        int idx[16], n = 0;
        char *p;

        if (line[0] != 'f' || line[1] != ' ') continue;
        for (p = line + 2; *p && n < 16;) {
            const int k = (int)strtol(p, &p, 10);
            if (k == 0) break;
            idx[n++] = k > 0 ? k - 1 : nv + k;
            while (*p && *p != ' ') p++;                      /* skip /vt/vn */
            while (*p == ' ') p++;
        }
        for (i = 1; i + 1 < n; i++) {
            float q[3][3];
            const int id[3] = { idx[0], idx[i], idx[i + 1] };
            int k;

            for (k = 0; k < 3; k++) {
                const float *a = v[id[k]];
                const float px = (a[0] - 0.5f * (lo[0] + hi[0])) * s, py = (a[1] - lo[1]) * s, pz = (a[2] - 0.5f * (lo[2] + hi[2])) * s;

                q[k][0] = x + px * c + pz * sn; q[k][1] = py; q[k][2] = z - px * sn + pz * c;
            }
            add_tri(q[0], q[1], q[2], col);
        }
    }
    fclose(f);
    free(v);
    return ntri - nt0;
}

/* ---- shadow map: the library's (sh4der_psm_raster.h) --------------------------- */

typedef struct { sh4der_psm_raster_t r; uint16_t *depth; unsigned n; } smap_t;

/* Fill the map from every caster triangle: the key range of all their vertices, then each triangle. */
static int map_fill(smap_t *s, const sh4der_psm_t *place) {
    float lo = 1e30f, hi = -1e30f, a = 0.0f, b = 0.0f;
    int k, i;

    for (k = 0; k < 3; k++) {
        if (!sh4der_psm_raster_key_range(place->light, &tris[0].v[k][0], (unsigned)sizeof(tri_t), (unsigned)ntri, &a, &b)) return 0;
        if (a < lo) lo = a;
        if (b > hi) hi = b;
    }
    if (!sh4der_psm_raster_begin(&s->r, place, s->depth, s->n, lo - 0.5f, hi + 0.5f)) return 0;
    for (i = 0; i < ntri; i++) sh4der_psm_raster_tri(&s->r, tris[i].v[0], tris[i].v[1], tris[i].v[2]);
    return 1;
}

/* 1 lit .. 0 shadowed. The receiver-plane compare (the surface's own depth at each texel read), a bias in metres, 2 x 2 PCF. */
static float shadow(const smap_t *s, const float p[3], const float nrm[3], float bias_a, float bias_b, int pcf) {
    float t1[3], t2[3], h[3] = { 0.0f, 1.0f, 0.0f }, bias = bias_a;

    if (fabsf(nrm[1]) > 0.9f) { h[0] = 1.0f; h[1] = 0.0f; }
    sh4der_cross_(h, nrm, t1); sh4der_norm3_(t1);
    sh4der_cross_(nrm, t1, t2); sh4der_norm3_(t2);
    if (bias_b > 0.0f) {
        float texel = sh4der_psm_texel_m(&s->r.map, s->n, p, t1, t2, SH4DER_PSM_RASTER_DELTA);
        if (texel > SH4DER_PSM_RASTER_TEXEL_MAX) texel = SH4DER_PSM_RASTER_TEXEL_MAX;
        bias = sh4der_psm_bias(bias_a, bias_b, texel);
    }
    return sh4der_psm_raster_lit_plane(&s->r, p, t1, t2, bias, pcf);
}

/* ---- camera view ------------------------------------------------------------ */

typedef struct { unsigned char *rgb; unsigned w, h; } img_t;

static void put(img_t *im, unsigned x, unsigned y, const float c[3]) {
    unsigned char *p = im->rgb + 3u * (y * im->w + x);
    int k;
    for (k = 0; k < 3; k++) { float f = c[k]; f = f < 0 ? 0 : f > 1 ? 1 : f; p[k] = (unsigned char)(powf(f, 1.0f / 2.2f) * 255.0f + 0.5f); }
}

static void write_ppm(const img_t *im, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", path); exit(2); }
    fprintf(f, "P6\n%u %u\n255\n", im->w, im->h);
    fwrite(im->rgb, 3, (size_t)im->w * im->h, f);
    fclose(f);
}

typedef struct { float sun[3], bias_a, bias_b; int pcf; } shade_t;

static void render(const sh4der_psm_cam_t *cam, const smap_t *sm, const shade_t *sh, img_t *out) {
    const unsigned W = out->w, H = out->h;
    float r[3], u[3];
    float *zbuf = (float *)malloc(sizeof(float) * W * H), (*pos)[3] = (float (*)[3])malloc(sizeof(float) * 3 * W * H);
    int *tid = (int *)malloc(sizeof(int) * W * H);
    unsigned x, y;
    int i;

    sh4der_cross_(cam->fwd, cam->up, r); sh4der_norm3_(r);
    sh4der_cross_(r, cam->fwd, u); sh4der_norm3_(u);
    for (i = 0; i < (int)(W * H); i++) { zbuf[i] = 1e30f; tid[i] = -1; }
    for (i = 0; i < ntri; i++) {
        float sx[3], sy[3], iz[3], area;
        int k, ok = 1, x0, x1, y0, y1, px, py;

        for (k = 0; k < 3; k++) {
            float d[3] = { tris[i].v[k][0] - cam->eye[0], tris[i].v[k][1] - cam->eye[1], tris[i].v[k][2] - cam->eye[2] };
            const float z = sh4der_dot3_(d, cam->fwd);

            if (z < cam->znear) { ok = 0; break; }
            iz[k] = 1.0f / z;
            sx[k] = (sh4der_dot3_(d, r) / (z * cam->tan_half * cam->aspect) * 0.5f + 0.5f) * (float)W;
            sy[k] = (0.5f - sh4der_dot3_(d, u) / (z * cam->tan_half) * 0.5f) * (float)H;
        }
        if (!ok) continue;
        area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0]);
        if (!(fabsf(area) > 1e-9f)) continue;
        x0 = (int)floorf(fminf(sx[0], fminf(sx[1], sx[2]))); x1 = (int)ceilf(fmaxf(sx[0], fmaxf(sx[1], sx[2])));
        y0 = (int)floorf(fminf(sy[0], fminf(sy[1], sy[2]))); y1 = (int)ceilf(fmaxf(sy[0], fmaxf(sy[1], sy[2])));
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 > (int)W) x1 = (int)W;
        if (y1 > (int)H) y1 = (int)H;
        for (py = y0; py < y1; py++)
            for (px = x0; px < x1; px++) {
                const float cx = (float)px + 0.5f, cy = (float)py + 0.5f;
                const float b0 = ((sx[1] - cx) * (sy[2] - cy) - (sx[2] - cx) * (sy[1] - cy)) / area;
                const float b1 = ((sx[2] - cx) * (sy[0] - cy) - (sx[0] - cx) * (sy[2] - cy)) / area;
                const float b2 = 1.0f - b0 - b1;
                float den, z;

                if (b0 < 0.0f || b1 < 0.0f || b2 < 0.0f) continue;
                den = b0 * iz[0] + b1 * iz[1] + b2 * iz[2];
                z = 1.0f / den;
                if (z < zbuf[py * (int)W + px]) {
                    float *p = pos[py * (int)W + px];
                    zbuf[py * (int)W + px] = z;
                    tid[py * (int)W + px] = i;
                    for (k = 0; k < 3; k++) p[k] = (b0 * tris[i].v[0][k] * iz[0] + b1 * tris[i].v[1][k] * iz[1] + b2 * tris[i].v[2][k] * iz[2]) * z;
                }
            }
    }
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) {
            const float nx = ((float)x + 0.5f) / (float)W * 2.0f - 1.0f, ny = 1.0f - ((float)y + 0.5f) / (float)H * 2.0f;
            float dir[3], col[3], p[3], n[3] = { 0.0f, 1.0f, 0.0f }, alb[3], tg = 1e30f, l;
            int k, caster = tid[y * (int)W + x];

            for (k = 0; k < 3; k++) dir[k] = cam->fwd[k] + r[k] * nx * cam->tan_half * cam->aspect + u[k] * ny * cam->tan_half;
            if (dir[1] < -1e-6f) tg = -cam->eye[1] / dir[1];                  /* ground y = 0; dir has view depth 1, so tg is view depth */
            if (caster >= 0 && zbuf[y * (int)W + x] < tg) {
                const tri_t *t = &tris[caster];
                float e1[3], e2[3];

                for (k = 0; k < 3; k++) { p[k] = pos[y * (int)W + x][k]; e1[k] = t->v[1][k] - t->v[0][k]; e2[k] = t->v[2][k] - t->v[0][k]; alb[k] = t->col[k]; }
                sh4der_cross_(e1, e2, n); sh4der_norm3_(n);
                { float vd[3] = { cam->eye[0] - p[0], cam->eye[1] - p[1], cam->eye[2] - p[2] }; if (sh4der_dot3_(n, vd) < 0.0f) { n[0] = -n[0]; n[1] = -n[1]; n[2] = -n[2]; } }
            } else if (tg < cam->zfar) {
                const int chk = ((int)floorf(cam->eye[0] + dir[0] * tg) + (int)floorf(cam->eye[2] + dir[2] * tg)) & 1;
                for (k = 0; k < 3; k++) p[k] = cam->eye[k] + dir[k] * tg;
                alb[0] = chk ? 0.50f : 0.38f; alb[1] = chk ? 0.52f : 0.40f; alb[2] = chk ? 0.46f : 0.34f;
                p[1] = 0.0f;
            } else {
                const float sky = 0.5f + 0.5f * (dir[1] > 0 ? dir[1] : 0);
                col[0] = 0.55f * sky; col[1] = 0.70f * sky; col[2] = 0.95f; put(out, x, y, col);
                continue;
            }
            l = sh4der_dot3_(n, sh->sun);
            if (l < 0.0f) l = 0.0f;
            {
                const float lit = l > 0.0f ? shadow(sm, p, n, sh->bias_a, sh->bias_b, sh->pcf) : 0.0f;
                for (k = 0; k < 3; k++) col[k] = alb[k] * (0.22f + 0.95f * l * lit);
            }
            put(out, x, y, col);
        }
    free(zbuf); free(pos); free(tid);
}

/* the map as a picture: the 16-bit depth scaled over what the casters wrote, empty texels black */
static void map_image(const smap_t *s, img_t *im) {
    unsigned lo = 65536u, hi = 0u, i, x, y;

    for (i = 0; i < s->n * s->n; i++) if (s->depth[i]) { if (s->depth[i] < lo) lo = s->depth[i]; if (s->depth[i] > hi) hi = s->depth[i]; }
    im->w = im->h = s->n;
    im->rgb = (unsigned char *)calloc(3, (size_t)s->n * s->n);
    for (y = 0; y < s->n; y++)
        for (x = 0; x < s->n; x++) {
            const unsigned d = s->depth[(s->n - 1 - y) * s->n + x];     /* v up */
            const float g = d && hi > lo ? 0.25f + 0.75f * (float)(d - lo) / (float)(hi - lo) : 0.0f;
            unsigned char *p = im->rgb + 3u * (y * s->n + x);

            p[0] = p[1] = p[2] = (unsigned char)(g * 255.0f);
            if (d) p[2] = (unsigned char)(g * 200.0f + 55.0f);
        }
}

/* ---- bench: the two routes, host numbers ---------------------------------------- */

static double now_ms(void) { return 1000.0 * (double)clock() / (double)CLOCKS_PER_SEC; }

/* Per frame cost of the two routes on this machine, NOT on an SH-4: the SH-4 is far slower and has no big cache.
   Route B: the SH-4 fills a depth map and compares it at every ground vertex. Route A: the PVR fills the map and does
   the lookup, so the SH-4 only clips the casters and gives each ground vertex its coordinates. Counts are exact; the
   Dreamcast's PVR time, TA bytes in flight and frames a second are not measured here. */
static void bench(const sh4der_psm_cam_t *cam, const float sun[3], unsigned N, float warp_near, const float *pts, unsigned np) {
    enum { REPS = 300, MAXG = 20000 };
    static float gv[MAXG][3];
    const float ax[3] = { 1, 0, 0 }, az[3] = { 0, 0, 1 };
    float rows[16], gx, gz;
    unsigned ng = 0, m, rep, i;
    uint16_t *dep = (uint16_t *)malloc(sizeof(uint16_t) * N * N);

    sh4der_psm_cam_rows(cam, warp_near, rows);
    for (gz = -70.0f; gz < -2.0f; gz += 1.0f)
        for (gx = -45.0f; gx <= 45.0f; gx += 1.0f) {
            float h[4];
            const float g[3] = { gx, 0.0f, gz };

            sh4der_psm_xform(rows, g, 1.0f, h);
            if (h[3] > 0.0f && fabsf(h[0]) <= h[3] && fabsf(h[1]) <= h[3] && ng < (unsigned)MAXG) { memcpy(gv[ng], g, 12); ng++; }
        }
    printf("\nBENCH (host x86, one frame, mean of %d; NOT an SH-4: use for ratios and counts only)\n", (int)REPS);
    printf("  scene: %d caster triangles, %u ground vertices on a 1 m grid in view, map %ux%u (%u KB)\n", ntri, ng, N, N, N * N * 2u / 1024u);
    printf("  %-9s %-8s %9s %9s %9s %9s   %s\n", "route", "map", "build ms", "caster ms", "recv. ms", "total ms", "counts");
    for (m = 0; m < 2u; m++) {
        sh4der_psm_t place;
        sh4der_psm_raster_t r;
        double t0, t_build, t_fill, t_cmp, t_clip, t_uv;
        unsigned out_tris = 0, out_verts_ta = 0;
        float sink = 0.0f;

        t0 = now_ms();
        for (rep = 0; rep < (unsigned)REPS; rep++)
            if (!(m == 0 ? sh4der_psm_build_uniform(&place, sun, pts, np) : sh4der_psm_build_warp(&place, cam, warp_near, sun, pts, np))) { printf("  no map\n"); return; }
        t_build = (now_ms() - t0) / (double)REPS;

        /* route B */
        {
            float lo = 1e30f, hi = -1e30f, a = 0.0f, b = 0.0f;
            int k;

            for (k = 0; k < 3; k++) {
                sh4der_psm_raster_key_range(place.light, &tris[0].v[k][0], (unsigned)sizeof(tri_t), (unsigned)ntri, &a, &b);
                if (a < lo) lo = a;
                if (b > hi) hi = b;
            }
            t0 = now_ms();
            for (rep = 0; rep < (unsigned)REPS; rep++) {
                sh4der_psm_raster_begin(&r, &place, dep, N, lo - 0.5f, hi + 0.5f);
                for (i = 0; i < (unsigned)ntri; i++) sh4der_psm_raster_tri(&r, tris[i].v[0], tris[i].v[1], tris[i].v[2]);
            }
            t_fill = (now_ms() - t0) / (double)REPS;
            t0 = now_ms();
            for (rep = 0; rep < (unsigned)REPS; rep++)
                for (i = 0; i < ng; i++) sink += sh4der_psm_raster_lit_plane(&r, gv[i], ax, az, 0.02f, 1);
            t_cmp = (now_ms() - t0) / (double)REPS;
        }
        printf("  %-9s %-8s %9.3f %9.3f %9.3f %9.3f   map texels written n/a, receiver reads %u (4 texels each), extra TA vertices 0\n", "B SH-4", m ? "PSM" : "uniform",
               t_build, t_fill, t_cmp, t_build + t_fill + t_cmp, ng);

        /* route A */
        {
            sh4der_psm_pvr_vtx_t out[SH4DER_PSM_PVR_MAX_TRIS][3];

            t0 = now_ms();
            for (rep = 0; rep < (unsigned)REPS; rep++) {
                out_tris = 0;
                for (i = 0; i < (unsigned)ntri; i++) out_tris += sh4der_psm_pvr_tris(&place, N, tris[i].v[0], tris[i].v[1], tris[i].v[2], out);
            }
            t_clip = (now_ms() - t0) / (double)REPS;
            t0 = now_ms();
            for (rep = 0; rep < (unsigned)REPS; rep++)
                for (i = 0; i < ng; i++) { float u, v; if (sh4der_psm_pvr_uv(&place, gv[i], &u, &v)) sink += u + v; }
            t_uv = (now_ms() - t0) / (double)REPS;
            out_verts_ta = out_tris * 3u + ng;
        }
        printf("  %-9s %-8s %9.3f %9.3f %9.3f %9.3f   map pass %u triangles, ground layer %u vertices: %u TA vertices (%u KB at 32 B), target %u KB (estimates for the PVR)\n",
               "A PVR", m ? "PSM" : "uniform", t_build, t_clip, t_uv, t_build + t_clip + t_uv, out_tris, ng, out_verts_ta, out_verts_ta * 32u / 1024u, N * N * 2u / 1024u);
        if (sink == 12345.678f) printf(" ");
    }
    printf("  A has no self-shadowing and, for PSM, needs ground cells fine near the camera (psm_pvr_test); B has both and\n");
    printf("  costs SH-4 time per caster triangle and per ground vertex. PVR render time and fps need a console.\n");
    free(dep);
}

/* ---- main ------------------------------------------------------------------ */

static const char *arg(int argc, char **argv, const char *name, const char *def) {
    int i;
    for (i = 1; i + 1 < argc; i++) if (!strcmp(argv[i], name)) return argv[i + 1];
    return def;
}

static int flag(int argc, char **argv, const char *name) {
    int i;
    for (i = 1; i < argc; i++) if (!strcmp(argv[i], name)) return 1;
    return 0;
}

int main(int argc, char **argv) {
    const char *obj = arg(argc, argv, "--obj", "demo/psm/assets/_dl/spot.obj"), *outdir = arg(argc, argv, "--out", "demo/psm/out");
    unsigned N = (unsigned)atoi(arg(argc, argv, "--map", "256")), W = 320, H = 240, i;
    const float az = (float)atof(arg(argc, argv, "--az", "120")) * 0.0174533f, el = (float)atof(arg(argc, argv, "--el", "35")) * 0.0174533f;
    const float yaw = (float)atof(arg(argc, argv, "--yaw", "0")) * 0.0174533f, warp_near = (float)atof(arg(argc, argv, "--warp-near", "4")), fit_near = (float)atof(arg(argc, argv, "--fit-near", "2"));
    const float pole[3] = { 0.62f, 0.60f, 0.55f }, cow1[3] = { 0.85f, 0.80f, 0.70f }, cow2[3] = { 0.80f, 0.55f, 0.45f };
    sh4der_psm_cam_t cam;
    shade_t sh;
    smap_t maps[2];
    float corners[8][3], pts[32][3], ground[3] = { 0, 1, 0 }, pitch = 0.12f;
    unsigned np;
    char path[512];
    const char *names[2] = { "uniform", "psm" };
    int k, m;

    sscanf(arg(argc, argv, "--size", "320x240"), "%ux%u", &W, &H);
    if (N < 16u || N > 2048u || W < 16u || H < 16u) { fprintf(stderr, "bad --map or --size\n"); return 2; }

    for (i = 0; i < 10u; i++) add_box((i & 1u) ? 2.6f : -2.6f, -4.0f - 4.4f * (float)i, 0.3f, 5.0f, pole);
    k = add_obj(obj, 0.3f, -7.0f, 2.2f, 0.5f, cow1);
    k += add_obj(obj, -1.6f, -15.0f, 2.6f, -0.9f, cow2);
    if (!k) fprintf(stderr, "note: no %s (python demo/psm/tools/fetch.py); the poles only\n", obj);

    memset(&cam, 0, sizeof cam);
    cam.eye[1] = 1.7f;
    cam.fwd[0] = sinf(yaw) * cosf(pitch); cam.fwd[1] = -sinf(pitch); cam.fwd[2] = -cosf(yaw) * cosf(pitch);
    {
        const float rt[3] = { cosf(yaw), 0.0f, sinf(yaw) };
        sh4der_cross_(rt, cam.fwd, cam.up);
    }
    cam.tan_half = tanf(0.5f * 1.0472f); cam.aspect = (float)W / (float)H; cam.znear = 0.3f; cam.zfar = 70.0f;

    sh.sun[0] = sinf(az) * cosf(el); sh.sun[1] = sinf(el); sh.sun[2] = -cosf(az) * cosf(el);
    sh4der_norm3_(sh.sun);
    sh.bias_a = (float)atof(arg(argc, argv, "--bias-a", "0.02")); sh.bias_b = (float)atof(arg(argc, argv, "--bias-b", "0"));
    sh.pcf = !flag(argc, argv, "--nopcf");

    sh4der_psm_frustum(&cam, fit_near, cam.zfar, corners);
    np = sh4der_psm_slab_pts((const float (*)[3])corners, ground, 0.0f, 0.5f, pts);   /* the receivers: ground and what stands low on it */

    for (m = 0; m < 2; m++) {
        sh4der_psm_t place;
        int ok;
        maps[m].n = N;
        maps[m].depth = (uint16_t *)malloc(sizeof(uint16_t) * N * N);
        ok = m == 0 ? sh4der_psm_build_uniform(&place, sh.sun, &pts[0][0], np)
                    : sh4der_psm_build_warp(&place, &cam, warp_near, sh.sun, &pts[0][0], np);
        if (!ok) {
            fprintf(stderr, "%s map: no fit for this sun (head on?); using the uniform map for it\n", names[m]);
            sh4der_psm_build_uniform(&place, sh.sun, &pts[0][0], np);
        }
        if (!map_fill(&maps[m], &place)) { fprintf(stderr, "map_fill failed\n"); return 2; }
        {
            img_t im, mi;
            const float nrm[3] = { 0, 1, 0 };

            im.w = W; im.h = H; im.rgb = (unsigned char *)malloc(3u * W * H);
            render(&cam, &maps[m], &sh, &im);
            snprintf(path, sizeof path, "%s/%s.ppm", outdir, names[m]);
            write_ppm(&im, path);
            map_image(&maps[m], &mi);
            snprintf(path, sizeof path, "%s/map_%s.ppm", outdir, names[m]);
            write_ppm(&mi, path);
            free(im.rgb); free(mi.rgb);
            {
                const float ax[3] = { 1, 0, 0 }, az3[3] = { 0, 0, 1 };
                const float dist[4] = { 4.0f, 10.0f, 25.0f, 60.0f };
                printf("%-8s map %ux%u kind %d  tris given %u drawn %u  texel / pixel on the ground at 4, 10, 25, 60 m ahead:", names[m], N, N,
                       maps[m].r.map.kind, maps[m].r.tris, maps[m].r.drawn);
                for (k = 0; k < 4; k++) {
                    float p[3];
                    p[0] = cam.eye[0] + cam.fwd[0] / cosf(pitch) * dist[k]; p[1] = 0.0f; p[2] = cam.eye[2] + cam.fwd[2] / cosf(pitch) * dist[k];
                    printf(" %.2f", sh4der_psm_texel_m(&maps[m].r.map, N, p, ax, az3, 0.05f) / sh4der_psm_pixel_m(&cam, H, p, nrm));
                }
                printf("\n");
            }
        }
    }
    if (flag(argc, argv, "--bench")) bench(&cam, sh.sun, N, warp_near, &pts[0][0], np);
    for (m = 0; m < 2; m++) free(maps[m].depth);
    free(tris);
    return 0;
}
