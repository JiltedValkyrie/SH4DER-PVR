/* SH4DER-PVR: the AO demo's lamp look: a paper shade lit from inside, a glow at the bulb (demo/psm/ao): the maths

   demo/psm/ao/ao4_lamp.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   The room's standing lamp switched on, the room dimmed. Four effects, by the field's names (../psm_effects.h):
     Emission                the shade itself gives light: its colour is its own level, not the room's light or AO
     Subsurface Scattering   the bulb through the paper toward the eye: sh4der_sss.h, a vertex, into the offset colour
     Glow                    a round halo centred on the bulb: sh4der_bloom.h's rings, faded out where it would cut
                             into the floor or a wall (a camera-facing glow passes through whatever is near it)
     Punch-Through Cutout    slots in the shade: a 1-bit texture, the PVR's alpha test in the punch-through list
   This header is all of it that is maths: it draws nothing and has no KallistiOS in it. Host tested
   (test/ao4_lamp_test.c, which also renders the look in software).

   THE SHADE is not the room model's: the model's shade is two rings of 12 with caps, too coarse for a colour a vertex
   to shape the glow. ao4_lamp_find reads the lamp's place and the shade's two rings from the model's vertices,
   ao4_lamp_build makes a shade of AO4_LAMP_SEG x AO4_LAMP_BANDS quads on them, open at both ends (65 vertices, 96
   triangles), and ao4_lamp_is_shade says which of the model's triangles to leave out while the look is on.

   A FRAME
     ao4_lamp_shade   each shade vertex's two colours for the eye: base (x the texture) and offset (added after it).
                      A vertex seen from outside: base = the emission level, offset = the sss. Seen from inside (through
                      the open ends, or through a slot at the far side): base = the paper lit by the bulb, offset 0.
                      Draw with no culling, texture MODULATE, offset colour on.
     ao4_lamp_halo    the halo's vertices and their additive colours (alpha 255, the level in the colour: blend
                      ONE x ONE, depth test on, no depth write, after the shade). `toward` moves the halo's centre from
                      the bulb toward the eye; 0 is at the bulb, where the shade's near side hides its middle.
     ao4_lamp_pattern the slots' texture, ARGB1555, AO4_LAMP_TEX square: alpha 0 in a slot. One slot a segment.

   Cost, MEASURED in suite s7 (one console, one run, 2026-11-25; ledger SH4-083): the shade 187 us for its 65 vertices
   (a normalise for the view, one for the light, sh4der_sss_rgb: three exp), the halo 205 us for its 129 (the planes'
   dots). Sent: 96 + 240 triangles (PVR-157). That halo number is the fade as it was then, written out here; since
   2026-11-26 the halo's fade is sh4der_softfade.h's (one reciprocal a halo, not one a vertex): the same levels on the
   host (ao4_lamp_test), so its time is not known. */

#ifndef AO4_LAMP_H
#define AO4_LAMP_H

#include <stdint.h>
#include <string.h>

#include "sh4der_core.h"
#include "sh4der_sss.h"
#include "sh4der_bloom.h"
#include "sh4der_softfade.h"

#define AO4_LAMP_SEG 12u
#define AO4_LAMP_BANDS 4u
#define AO4_LAMP_V ((AO4_LAMP_SEG + 1u) * (AO4_LAMP_BANDS + 1u))      /* a column twice: the texture's seam */
#define AO4_LAMP_T (AO4_LAMP_SEG * AO4_LAMP_BANDS * 2u)
#define AO4_LAMP_HALO_RINGS 8u
#define AO4_LAMP_HALO_SEG 16u
#define AO4_LAMP_HALO_V (1u + AO4_LAMP_HALO_RINGS * AO4_LAMP_HALO_SEG)
#define AO4_LAMP_HALO_T (AO4_LAMP_HALO_SEG + 2u * AO4_LAMP_HALO_SEG * (AO4_LAMP_HALO_RINGS - 1u))
#define AO4_LAMP_TEX 32u
#define AO4_LAMP_SHADE_MIN 0.15f            /* a lamp vertex further than this from the pole's axis is the shade's */
#define AO4_LAMP_DIM 0.38f                  /* the room's own light x this while the lamp look is on */

typedef struct ao4_lamp_plane { float o[3], n[3]; } ao4_lamp_plane_t;   /* a point on it, its unit normal into the room */

typedef struct ao4_lamp {
    float cx, cz, y0, y1, r0, r1;           /* the axis; the shade's lower and upper ring: height, radius */
    float mid;                              /* half the lamp's height: the shade is above it, the foot below */
    float bulb[3];
    float p[AO4_LAMP_V][3], n[AO4_LAMP_V][3], uv[AO4_LAMP_V][2];
    uint16_t tri[AO4_LAMP_T][3];
    sh4der_sss_t sss;
    float warm[3];                          /* the bulb's colour */
    float emission;                         /* the shade's own level seen from outside, 0 .. 1 */
    float inside;                           /* the paper's level seen from inside, lit by the bulb */
    float thickness;                        /* the paper's path straight through, in units of 1 / absorption */
    float halo_radius, halo_peak, halo_fade;   /* metres; the level at the middle; the distance over which a plane fades it */
} ao4_lamp_t;

static inline float ao4_lamp_radius_(const ao4_lamp_t *l, const float p[3]) {
    const float dx = p[0] - l->cx, dz = p[2] - l->cz;

    return sh4der_sqrt_(dx * dx + dz * dz);
}

/* The lamp from its model's vertices (v, vn of them): the axis is the middle of their box, the shade is what stands
   out from it in the lamp's upper half (a foot stands out too, below). 0 when nothing does (not a lamp with a
   shade). Sets the tuning to its defaults. */
static inline int ao4_lamp_find(ao4_lamp_t *l, const float (*v)[3], unsigned vn) {
    static const float warm[3] = { 1.0f, 0.86f, 0.62f }, absorb[3] = { 0.2f, 0.3f, 0.5f };
    float lo[3], hi[3];
    unsigned i, k, found = 0;

    memset(l, 0, sizeof *l);
    if (!vn) return 0;
    for (k = 0; k < 3u; k++) lo[k] = hi[k] = v[0][k];
    for (i = 1; i < vn; i++) for (k = 0; k < 3u; k++) { if (v[i][k] < lo[k]) lo[k] = v[i][k]; if (v[i][k] > hi[k]) hi[k] = v[i][k]; }
    l->cx = 0.5f * (lo[0] + hi[0]); l->cz = 0.5f * (lo[2] + hi[2]);
    l->mid = 0.5f * (lo[1] + hi[1]);
    {                                       /* the axis again, from the shade alone: a foot need not be round about it */
        float slo[2] = { 0.0f, 0.0f }, shi[2] = { 0.0f, 0.0f };

        for (i = 0; i < vn; i++) {
            if (v[i][1] <= l->mid || ao4_lamp_radius_(l, v[i]) <= AO4_LAMP_SHADE_MIN) continue;
            if (!found || v[i][0] < slo[0]) slo[0] = v[i][0];
            if (!found || v[i][0] > shi[0]) shi[0] = v[i][0];
            if (!found || v[i][2] < slo[1]) slo[1] = v[i][2];
            if (!found || v[i][2] > shi[1]) shi[1] = v[i][2];
            found = 1;
        }
        if (!found) return 0;
        l->cx = 0.5f * (slo[0] + shi[0]); l->cz = 0.5f * (slo[1] + shi[1]);
    }
    found = 0;
    for (i = 0; i < vn; i++) {
        if (v[i][1] <= l->mid || ao4_lamp_radius_(l, v[i]) <= AO4_LAMP_SHADE_MIN) continue;
        if (!found || v[i][1] < l->y0) l->y0 = v[i][1];
        if (!found || v[i][1] > l->y1) l->y1 = v[i][1];
        found = 1;
    }
    if (!found || l->y1 - l->y0 < 0.05f) return 0;
    for (i = 0; i < vn; i++) {
        const float r = ao4_lamp_radius_(l, v[i]);

        if (r <= AO4_LAMP_SHADE_MIN || v[i][1] <= l->mid) continue;
        if (v[i][1] < l->y0 + 0.01f && r > l->r0) l->r0 = r;
        if (v[i][1] > l->y1 - 0.01f && r > l->r1) l->r1 = r;
    }
    l->bulb[0] = l->cx; l->bulb[1] = l->y0 + 0.45f * (l->y1 - l->y0); l->bulb[2] = l->cz;
    for (k = 0; k < 3u; k++) l->warm[k] = warm[k];
    sh4der_sss_init(&l->sss, warm, absorb, 1.0f, 0.4f, 0.1f);   /* AGENTS.md's "Cloth, paper, lantern" row */
    l->emission = 0.55f; l->inside = 0.9f; l->thickness = 0.35f;
    l->halo_radius = 0.95f; l->halo_peak = 0.55f; l->halo_fade = 0.15f;
    return 1;
}

/* 1 when a triangle of the lamp's model is the shade's (or a cap's): leave it out while the look is on. */
static inline int ao4_lamp_is_shade(const ao4_lamp_t *l, const float a[3], const float b[3], const float c[3]) {
    if (a[1] <= l->mid || b[1] <= l->mid || c[1] <= l->mid) return 0;         /* the foot and the pole */
    return ao4_lamp_radius_(l, a) > AO4_LAMP_SHADE_MIN || ao4_lamp_radius_(l, b) > AO4_LAMP_SHADE_MIN || ao4_lamp_radius_(l, c) > AO4_LAMP_SHADE_MIN;
}

/* The shade's mesh: rings j = 0 (lower) .. AO4_LAMP_BANDS, columns i = 0 .. AO4_LAMP_SEG (the last is the first
   again, with u one further); vertex j x (SEG + 1) + i. u counts segments (a slot each), v runs 0 .. 1 upward. */
static inline void ao4_lamp_build(ao4_lamp_t *l) {
    const float slope = sh4der_div_(l->r0 - l->r1, l->y1 - l->y0);   /* the cone: its normal leans up by this */
    unsigned i, j, t = 0;

    for (j = 0; j <= AO4_LAMP_BANDS; j++) {
        const float f = (float)j * (1.0f / (float)AO4_LAMP_BANDS), r = sh4der_lerp_(l->r0, l->r1, f), y = sh4der_lerp_(l->y0, l->y1, f);

        for (i = 0; i <= AO4_LAMP_SEG; i++) {
            const unsigned k = j * (AO4_LAMP_SEG + 1u) + i;
            float sn, cs;

            sh4der_sincos_((float)(i % AO4_LAMP_SEG) * (6.2831853f / (float)AO4_LAMP_SEG), &sn, &cs);
            l->p[k][0] = l->cx + r * cs; l->p[k][1] = y; l->p[k][2] = l->cz + r * sn;
            l->n[k][0] = cs; l->n[k][1] = slope; l->n[k][2] = sn;
            sh4der_norm3_(l->n[k]);
            l->uv[k][0] = (float)i; l->uv[k][1] = f;
        }
    }
    for (j = 0; j < AO4_LAMP_BANDS; j++)
        for (i = 0; i < AO4_LAMP_SEG; i++) {
            const unsigned a = j * (AO4_LAMP_SEG + 1u) + i, b = a + 1u, c = a + AO4_LAMP_SEG + 1u, d = c + 1u;

            l->tri[t][0] = (uint16_t)a; l->tri[t][1] = (uint16_t)c; l->tri[t][2] = (uint16_t)b; t++;
            l->tri[t][0] = (uint16_t)b; l->tri[t][1] = (uint16_t)c; l->tri[t][2] = (uint16_t)d; t++;
        }
}

static inline uint32_t ao4_lamp_pack_(const float rgb[3]) {
    return 0xff000000u | ((uint32_t)(sh4der_sat_(rgb[0]) * 255.0f + 0.5f) << 16) | ((uint32_t)(sh4der_sat_(rgb[1]) * 255.0f + 0.5f) << 8)
         | (uint32_t)(sh4der_sat_(rgb[2]) * 255.0f + 0.5f);
}

/* One shade vertex for the eye: base and offset as RGB 0 .. 1 (not held to 1). */
static inline void ao4_lamp_vertex(const ao4_lamp_t *l, unsigned k, const float eye[3], float grey, float base[3], float offset[3]) {
    float view[3] = { eye[0] - l->p[k][0], eye[1] - l->p[k][1], eye[2] - l->p[k][2] };
    float light[3] = { l->bulb[0] - l->p[k][0], l->bulb[1] - l->p[k][1], l->bulb[2] - l->p[k][2] };
    float facing, w, ndl, sss[3], lit;
    unsigned c;

    sh4der_norm3_(view);
    sh4der_norm3_(light);
    facing = sh4der_dot3_(l->n[k], view);                      /* > 0: the eye sees the outside of the paper here */
    w = sh4der_sat_(facing * 4.0f + 0.5f);                     /* a short blend where the paper is edge-on, so nothing pops */
    ndl = -sh4der_dot3_(l->n[k], light);                       /* the bulb is inside: 0 .. 1 on the inner face */
    if (ndl < 0.0f) ndl = 0.0f;
    /* the paper is crossed aslant away from the bulb's height: a longer path, a deeper colour (held to 1 / 0.35) */
    sh4der_sss_rgb(&l->sss, view, light, l->n[k], l->thickness * sh4der_recip_pos_(ndl > 0.35f ? ndl : 0.35f), 1.0f, 1.0f, 1.0f, sss);
    lit = l->inside * (0.75f + 0.25f * ndl);
    for (c = 0; c < 3u; c++) {
        base[c] = sh4der_lerp_(l->warm[c] * lit, grey * l->warm[c] * l->emission, w);
        offset[c] = sss[c] * w;
    }
}

/* Every shade vertex's packed colours. grey: the look's surface grey (AO4_CLAY_GREY). */
static inline void ao4_lamp_shade(const ao4_lamp_t *l, const float eye[3], float grey, uint32_t base[AO4_LAMP_V], uint32_t offset[AO4_LAMP_V]) {
    unsigned k;

    for (k = 0; k < AO4_LAMP_V; k++) {
        float b[3], o[3];

        ao4_lamp_vertex(l, k, eye, grey, b, o);
        base[k] = ao4_lamp_pack_(b); offset[k] = ao4_lamp_pack_(o);
    }
}

static inline float ao4_lamp_smooth_(float x) { x = sh4der_sat_(x); return x * x * (3.0f - 2.0f * x); }

/* How much of the halo a point keeps: 0 on or behind a plane, 1 from halo_fade metres in front of every plane.
   The form the halo used until 2026-11-26 (a reciprocal a call); ao4_lamp_halo now goes through the library's
   sh4der_softfade_levels (the reciprocal once a halo), and the host test holds the two within a level of 255. */
static inline float ao4_lamp_fade(const ao4_lamp_t *l, const float p[3], const ao4_lamp_plane_t *planes, unsigned np) {
    const float inv = sh4der_recip_pos_(l->halo_fade);
    float f = 1.0f;
    unsigned i;

    for (i = 0; i < np; i++) {
        const float d[3] = { p[0] - planes[i].o[0], p[1] - planes[i].o[1], p[2] - planes[i].o[2] };

        f *= ao4_lamp_smooth_(sh4der_dot3_(d, planes[i].n) * inv);
    }
    return f;
}

/* The halo: AO4_LAMP_HALO_V vertices facing the eye and their additive colours; np = 0 for no fade (the fade is
   sh4der_softfade.h's: the planes turned into its form once a call, SH4DER_SOFTFADE_MAX at a time). The triangles
   are sh4der_bloom_rings_tris(AO4_LAMP_HALO_RINGS, AO4_LAMP_HALO_SEG, ..), AO4_LAMP_HALO_T of them. */
static inline void ao4_lamp_halo(const ao4_lamp_t *l, const float eye[3], float toward, const ao4_lamp_plane_t *planes, unsigned np,
                                 float xyz[AO4_LAMP_HALO_V][3], uint32_t argb[AO4_LAMP_HALO_V]) {
    float radius[AO4_LAMP_HALO_RINGS], level[AO4_LAMP_HALO_RINGS], lv[AO4_LAMP_HALO_V];
    float centre[3] = { l->bulb[0], l->bulb[1], l->bulb[2] }, dir[3] = { eye[0] - l->bulb[0], eye[1] - l->bulb[1], eye[2] - l->bulb[2] };
    sh4der_softfade_plane_t sp[SH4DER_SOFTFADE_MAX];
    sh4der_softfade_t sf;
    unsigned j, k, c, done;

    sh4der_norm3_(dir);
    for (c = 0; c < 3u; c++) centre[c] += dir[c] * toward;
    for (j = 0; j < AO4_LAMP_HALO_RINGS; j++) {
        const float t = (float)(j + 1u) * (1.0f / (float)AO4_LAMP_HALO_RINGS);

        radius[j] = l->halo_radius * t * (0.35f + 0.65f * t);  /* closer rings near the middle, where the level falls fastest */
        level[j] = l->halo_peak * (1.0f - t) * (1.0f - t);     /* the outer ring is 0 */
    }
    sh4der_bloom_rings(xyz, lv, centre, eye, l->halo_peak, radius, level, AO4_LAMP_HALO_RINGS, AO4_LAMP_HALO_SEG);
    sh4der_softfade_make(&sf, l->halo_fade);
    for (done = 0; done < np; done += c) {              /* the planes' keeps multiply, so a batch at a time is the same */
        c = np - done < SH4DER_SOFTFADE_MAX ? np - done : SH4DER_SOFTFADE_MAX;
        for (j = 0; j < c; j++) sh4der_softfade_plane(&sp[j], planes[done + j].o, planes[done + j].n);
        sh4der_softfade_levels(&sf, sp, c, NULL, 0u, &xyz[0][0], 3u, lv, AO4_LAMP_HALO_V);
    }
    for (k = 0; k < AO4_LAMP_HALO_V; k++) {
        const float f = lv[k];
        const float rgb[3] = { l->warm[0] * f, l->warm[1] * f, l->warm[2] * f };

        argb[k] = ao4_lamp_pack_(rgb);
    }
}

/* 1 when (u, v) of the shade is in a slot: u in segments, v 0 .. 1. The texture is this, a texel a sample. */
static inline int ao4_lamp_slot(float u, float v) {
    const float f = u - (float)(int)u;

    return v > 0.2f && v < 0.8f && f > 0.36f && f < 0.64f;
}

/* The slots' texture, ARGB1555 (alpha 0: cut out), AO4_LAMP_TEX x AO4_LAMP_TEX, row 0 at v = 0; white elsewhere.
   slots 0 gives a whole white texture: the shade with no slots through the same header. */
static inline void ao4_lamp_pattern(uint16_t px[AO4_LAMP_TEX * AO4_LAMP_TEX], int slots) {
    unsigned x, y;

    for (y = 0; y < AO4_LAMP_TEX; y++)
        for (x = 0; x < AO4_LAMP_TEX; x++)
            px[y * AO4_LAMP_TEX + x] = slots && ao4_lamp_slot(((float)x + 0.5f) * (1.0f / (float)AO4_LAMP_TEX), ((float)y + 0.5f) * (1.0f / (float)AO4_LAMP_TEX)) ? 0x7fffu : 0xffffu;
}

#endif /* AO4_LAMP_H */
