/* SH4DER-PVR: corr-a's host layout picture: the same polygon list as the console's, drawn by a small software rasteriser

   demo/psm/corr/tools/corr_preview.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

   LAYOUT ONLY. It samples each texture's top level by nearest, with no mip map: it cannot show the PVR's bilinear,
   trilinear or mip choice, so no picture from it stands for a filter mode. It shows where the corridor, the cells,
   the gate, the pillars, the torches, the light, the fog and the split's divider land.

   cc -DSH4DER_CORE_NO_SH4ZAM -I include -I <bake folder> demo/psm/corr/tools/corr_preview.c -lm
   ./a.out out.ppm s x yaw split_d (split_d 0: no divider) */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "../corr_scene.h"
#include "corr_tex_data.h"

static const unsigned mip_top_words[9] = { 0x3, 0x4, 0x8, 0x18, 0x58, 0x158, 0x558, 0x1558, 0x5558 };

static unsigned twid(unsigned x, unsigned y) {
    unsigned r = 0, i;

    for (i = 0; i < 8; i++) r |= (((y >> i) & 1u) << (2u * i)) | (((x >> i) & 1u) << (2u * i + 1u));
    return r;
}

static const uint16_t *tex_for(unsigned kind, unsigned *side) {
    *side = kind == CORR_T_CEIL || kind == CORR_T_DOOR ? 128u : 256u;
    return kind == CORR_T_FLOOR ? corr_tex_floor256 : kind == CORR_T_WALL ? corr_tex_wall256 : kind == CORR_T_CEIL ? corr_tex_ceil128 : corr_tex_door128;
}

static float zbuf[CORR_HGT][CORR_W];
static unsigned char img[CORR_HGT][CORR_W][3];
static corr_kind_t kinds[CORR_KINDS];
static corr_list_t list;

static void tri(const corr_poly_t *p, unsigned a, unsigned b, unsigned c) {
    const float x0 = p->x[a], y0 = p->y[a], x1 = p->x[b], y1 = p->y[b], x2 = p->x[c], y2 = p->y[c];
    const float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
    int xa = (int)floorf(fminf(x0, fminf(x1, x2))), xb = (int)ceilf(fmaxf(x0, fmaxf(x1, x2)));
    int ya = (int)floorf(fminf(y0, fminf(y1, y2))), yb = (int)ceilf(fmaxf(y0, fmaxf(y1, y2)));
    unsigned side = 0;
    const uint16_t *tx = CORR_TEXTURED(p->tex) ? tex_for(p->tex, &side) : NULL;
    int x, y;

    if (fabsf(area) < 1e-6f) return;
    if (xa < 0) xa = 0;
    if (ya < 0) ya = 0;
    if (xb > (int)CORR_W - 1) xb = CORR_W - 1;
    if (yb > (int)CORR_HGT - 1) yb = CORR_HGT - 1;
    for (y = ya; y <= yb; y++)
        for (x = xa; x <= xb; x++) {
            const float px = (float)x + 0.5f, py = (float)y + 0.5f;
            const float w0 = ((x1 - px) * (y2 - py) - (x2 - px) * (y1 - py)) / area, w1 = ((x2 - px) * (y0 - py) - (x0 - px) * (y2 - py)) / area, w2 = 1.0f - w0 - w1;
            float w, rgb[3], fog;
            unsigned k;

            if (w0 < 0 || w1 < 0 || w2 < 0) continue;
            w = w0 * p->z[a] + w1 * p->z[b] + w2 * p->z[c];
            if (w <= zbuf[y][x]) continue;
            for (k = 0; k < 3u; k++) {
                const unsigned s = 16u - 8u * k;
                rgb[k] = (w0 * p->z[a] * (float)((p->argb[a] >> s) & 255u) + w1 * p->z[b] * (float)((p->argb[b] >> s) & 255u) + w2 * p->z[c] * (float)((p->argb[c] >> s) & 255u)) / w / 255.0f;
            }
            if (tx) {
                const float u = (w0 * p->z[a] * p->u[a] + w1 * p->z[b] * p->u[b] + w2 * p->z[c] * p->u[c]) / w;
                const float v = (w0 * p->z[a] * p->v[a] + w1 * p->z[b] * p->v[b] + w2 * p->z[c] * p->v[c]) / w;
                const unsigned tu = (unsigned)(int)floorf((u - floorf(u)) * (float)side) & (side - 1u), tv = (unsigned)(int)floorf((v - floorf(v)) * (float)side) & (side - 1u);
                const unsigned lvl = side == 256u ? 8u : 7u;
                const uint16_t t = tx[mip_top_words[lvl] + twid(tu, tv)];

                rgb[0] *= (float)((t >> 11) & 31u) / 31.0f; rgb[1] *= (float)((t >> 5) & 63u) / 63.0f; rgb[2] *= (float)(t & 31u) / 31.0f;
            }
            if (p->tex == CORR_T_FLAME) for (k = 0; k < 3u; k++) rgb[k] += img[y][x][k] / 255.0f;   /* added */
            else zbuf[y][x] = w;
            fog = (1.0f / w - 24.0f) / (52.0f - 24.0f);
            fog = fog < 0 ? 0 : fog > 1 ? 1 : fog;
            for (k = 0; k < 3u; k++) { const float c = rgb[k] * (1.0f - fog); img[y][x][k] = (unsigned char)(c > 1 ? 255 : c < 0 ? 0 : c * 255.0f + 0.5f); }
        }
}

int main(int argc, char **argv) {
    corr_eye_t e;
    unsigned i, j, d;
    FILE *f;

    if (argc < 6) { fprintf(stderr, "out.ppm s x yaw split_d\n"); return 2; }
    e.s = (float)atof(argv[2]); e.x = (float)atof(argv[3]); e.yaw = (float)atof(argv[4]); d = (unsigned)atoi(argv[5]);
    corr_build(kinds);
    corr_frame(kinds, &e, 1, CORR_CAP, &list);
    for (i = 0; i < list.n; i++) if (list.p[i].tex != CORR_T_FLAME) for (j = 0; j + 2u < list.p[i].n; j++) tri(&list.p[i], j, j + 1u, j + 2u);
    for (i = 0; i < list.n; i++) if (list.p[i].tex == CORR_T_FLAME) for (j = 0; j + 2u < list.p[i].n; j++) tri(&list.p[i], j, j + 1u, j + 2u);
    if (d) for (j = 0; j < CORR_HGT; j++) for (i = d * 32u - 1u; i <= d * 32u; i++) img[j][i][0] = img[j][i][1] = img[j][i][2] = 230;
    f = fopen(argv[1], "wb");
    if (!f) return 1;
    fprintf(f, "P6 %u %u 255\n", CORR_W, CORR_HGT);
    fwrite(img, 1, sizeof img, f);
    fclose(f);
    printf("%u polygons, %u culled, %u clipped, %u dropped\n", list.n, list.culled, list.clipped, list.dropped);
    return 0;
}
