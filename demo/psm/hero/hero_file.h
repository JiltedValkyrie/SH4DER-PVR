/* SH4DER-PVR: hero1, the hero's file (HERO.BIN): write and read, format in README.md

   demo/psm/hero/hero_file.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   Little-endian, every block a multiple of 4 bytes. The reader refuses a file that does not fit hero_t's tables or
   whose numbers are out of range, and says why (hero_file_error). stdio only; host and Dreamcast. */

#ifndef HERO_FILE_H
#define HERO_FILE_H

#include <stdio.h>
#include "hero_model.h"

#define HERO_FILE_MAGIC   0x4f524548u        /* "HERO" */
#define HERO_FILE_VERSION 1u
#define HERO_FILE_HEAD    32u                /* 32-bit words in the header */

enum { HERO_E_OK = 0, HERO_E_IO = -1, HERO_E_MAGIC = -2, HERO_E_SIZE = -3, HERO_E_RANGE = -4, HERO_E_TEX = -5 };

static inline const char *hero_file_error(int e) {
    return e == HERO_E_OK ? "ok" : e == HERO_E_IO ? "short file" : e == HERO_E_MAGIC ? "not a HERO version 1 file"
         : e == HERO_E_SIZE ? "a count is over the program's tables" : e == HERO_E_RANGE ? "a value out of range" : "a texture's size or format";
}

static inline int hero_put_(FILE *f, const void *p, size_t n) {
    static const uint8_t zero[4] = { 0, 0, 0, 0 };

    if (n && fwrite(p, 1, n, f) != n) return -1;
    return (n & 3u) && fwrite(zero, 1, 4u - (n & 3u), f) != 4u - (n & 3u) ? -1 : 0;
}

static inline int hero_get_(FILE *f, void *p, size_t n) {
    uint8_t pad[4];

    if (n && fread(p, 1, n, f) != n) return -1;
    return (n & 3u) && fread(pad, 1, 4u - (n & 3u), f) != 4u - (n & 3u) ? -1 : 0;
}

/* The header: magic, version, bones, vertices, indices, strips, visor_strip0, clips, fps, frames of clip 0, of clip 1,
   then for each of the three textures w, h, format; the rest 0. */
static inline int hero_file_write(FILE *f, const hero_t *h) {
    uint32_t hd[HERO_FILE_HEAD];
    unsigned c, t, total = 0;

    memset(hd, 0, sizeof hd);
    hd[0] = HERO_FILE_MAGIC; hd[1] = HERO_FILE_VERSION; hd[2] = HERO_BONES; hd[3] = h->vn; hd[4] = h->idx_n; hd[5] = h->strips;
    hd[6] = h->visor_strip0; hd[7] = HERO_CLIPS; hd[8] = HERO_FPS; hd[9] = h->frames[0]; hd[10] = h->frames[1];
    for (t = 0; t < HERO_TEXS; t++) { hd[11u + 3u * t] = h->tex_w[t]; hd[12u + 3u * t] = h->tex_h[t]; hd[13u + 3u * t] = h->tex_fmt[t]; }
    if (hero_put_(f, hd, sizeof hd) || hero_put_(f, h->parent, HERO_BONES) || hero_put_(f, h->head, sizeof h->head)
        || hero_put_(f, h->p, h->vn * sizeof h->p[0]) || hero_put_(f, h->n, h->vn * sizeof h->n[0]) || hero_put_(f, h->uv, h->vn * sizeof h->uv[0])
        || hero_put_(f, h->b0, h->vn) || hero_put_(f, h->b1, h->vn) || hero_put_(f, h->w0, h->vn)
        || hero_put_(f, h->idx, h->idx_n * sizeof h->idx[0]) || hero_put_(f, h->slen, h->strips * sizeof h->slen[0])) return HERO_E_IO;
    for (c = 0; c < HERO_CLIPS; c++) { if (hero_put_(f, h->clip[c], h->frames[c] * sizeof h->clip[c][0])) return HERO_E_IO; total += h->frames[c]; }
    for (t = 0; t < HERO_TEXS; t++) if (hero_put_(f, h->tex[t], h->tex_w[t] * h->tex_h[t] * 2u)) return HERO_E_IO;
    (void)total;
    return HERO_E_OK;
}

static inline int hero_pow2_(uint32_t x) { return x >= 8u && x <= 1024u && !(x & (x - 1u)); }

static inline int hero_file_read(FILE *f, hero_t *h) {
    uint32_t hd[HERO_FILE_HEAD];
    unsigned c, t, i, sum = 0;

    memset(h, 0, sizeof *h);
    if (hero_get_(f, hd, sizeof hd)) return HERO_E_IO;
    if (hd[0] != HERO_FILE_MAGIC || hd[1] != HERO_FILE_VERSION) return HERO_E_MAGIC;
    if (hd[2] != HERO_BONES || hd[3] >= HERO_V_MAX || hd[4] >= HERO_IDX_MAX || hd[5] >= HERO_STRIP_MAX || hd[7] != HERO_CLIPS
        || hd[8] != HERO_FPS || hd[9] < 2u || hd[9] > HERO_FRAMES_MAX || hd[10] < 2u || hd[10] > HERO_FRAMES_MAX) return HERO_E_SIZE;
    h->vn = hd[3]; h->idx_n = hd[4]; h->strips = hd[5]; h->visor_strip0 = hd[6]; h->frames[0] = hd[9]; h->frames[1] = hd[10];
    if (h->visor_strip0 > h->strips) return HERO_E_RANGE;
    for (t = 0; t < HERO_TEXS; t++) {
        h->tex_w[t] = hd[11u + 3u * t]; h->tex_h[t] = hd[12u + 3u * t]; h->tex_fmt[t] = hd[13u + 3u * t];
        if (!hero_pow2_(h->tex_w[t]) || !hero_pow2_(h->tex_h[t]) || h->tex_w[t] * h->tex_h[t] > HERO_TEX_MAX || h->tex_fmt[t] > HERO_FMT_4444) return HERO_E_TEX;
    }
    if (hero_get_(f, h->parent, HERO_BONES) || hero_get_(f, h->head, sizeof h->head)
        || hero_get_(f, h->p, h->vn * sizeof h->p[0]) || hero_get_(f, h->n, h->vn * sizeof h->n[0]) || hero_get_(f, h->uv, h->vn * sizeof h->uv[0])
        || hero_get_(f, h->b0, h->vn) || hero_get_(f, h->b1, h->vn) || hero_get_(f, h->w0, h->vn)
        || hero_get_(f, h->idx, h->idx_n * sizeof h->idx[0]) || hero_get_(f, h->slen, h->strips * sizeof h->slen[0])) return HERO_E_IO;
    for (c = 0; c < HERO_CLIPS; c++) if (hero_get_(f, h->clip[c], h->frames[c] * sizeof h->clip[c][0])) return HERO_E_IO;
    for (t = 0; t < HERO_TEXS; t++) if (hero_get_(f, h->tex[t], h->tex_w[t] * h->tex_h[t] * 2u)) return HERO_E_IO;
    for (i = 0; i < HERO_BONES; i++) if (h->parent[i] != HERO_NOBONE && h->parent[i] >= i) return HERO_E_RANGE;   /* parents first */
    for (i = 0; i < h->vn; i++) {
        if (h->b0[i] >= HERO_BONES || h->b1[i] >= HERO_BONES) return HERO_E_RANGE;
        if (i && (h->b0[i] < h->b0[i - 1u] || (h->b0[i] == h->b0[i - 1u] && h->b1[i] < h->b1[i - 1u]))) return HERO_E_RANGE;   /* sorted */
    }
    for (i = 0; i < h->idx_n; i++) if (h->idx[i] >= h->vn) return HERO_E_RANGE;
    for (i = 0; i < h->strips; i++) { if (h->slen[i] < 3u) return HERO_E_RANGE; sum += h->slen[i]; h->tris += h->slen[i] - 2u; }
    if (sum != h->idx_n) return HERO_E_RANGE;
    h->from_file = 1;
    return HERO_E_OK;
}

#endif
