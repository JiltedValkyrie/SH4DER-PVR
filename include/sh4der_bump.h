/* SH4DER-PVR: normal map lighting

   sh4der_bump.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_bump.h
    \brief   A normal map lit by a moving light on the PowerVR2, put on by ordinary blend passes (no bump unit): pack and decode the map, light a vertex from it,
             send a polygon. The grey maps and the headers of the passes are the client's (demo/torchwalk_ab.c is the worked example).

    The map. A normal map texel is a surface normal in the texture's own frame, kept as two bytes: S, the slope (the angle the normal leans away from
    straight up, 0 = flat), and R, the direction it leans in (0..255 round the full circle). That is the PVR bump texel layout, kept so the maps pvrtex and
    the game already make work; the bump unit itself is not used. Only the normals are kept, in RAM (sh4der_bump_normal_decode, 3 bytes a texel).

    The effect (highlight power 2). The highlight is (N.H)^2, H the half vector of the light and the eye, = sum Ni^2 Hi^2 + sum 2 Hi Hj Ni Nj. The client makes nine 8-bit
    grey textures from the decoded normals (three squares, three signed products biased 0.5 and in a + and a - copy), draws them one pass each with ONE x ONE onto a
    flat level kept at half (vertex colour = the pass's weight), then the picture with the blend DESTCOLOR x (source colour): twice picture x level, so a flat texel stays
    the picture and a slope facing H goes up to twice it. The torch walk's cell cut, strength and passes show the order.

    The conventions, and where each was read:
    - Texel layout: KallistiOS utils/pvrtex/pixel.h pxlFloattoSpherical. S = (altitude / (pi / 2) x 255) xor 0xff, R = azimuth /
      (2 pi) x 255, texel = (S << 8) | R, altitude = acos(nz), azimuth = atan2(ny, nx). Twiddled 16 bit, no mip levels. pvrtex builds the normal from a height
      map (utils/pvrtex/pvr_texture_encoder.c) with nx = h(left) - h(right), ny = h(below) - h(above), image rows running down: a height that rises toward the top
      tilts the normal toward -y, so the texel's y axis runs UP the image, against texture v (x right, y up, z out: right handed). sh4der_bump_from_height does
      the same, with nz = 1 as a plain slope (KallistiOS writes 1 - nx^2 + ny^2 under the root, which looks like a typo for 1 - nx^2 - ny^2; the angles normalise
      the vector, so a plain 1 keeps the slope honest).
    - The light is read in the same frame: x along +u, y along -v (up the image), z along the normal. A sh4der_bump_frame_t gives +v (the way rows run), so
      flip 0 turns it round for you; flip 1 is for a normal map whose y runs DOWN the image.

    The one open item: which way the azimuth runs against the texture's u axis (the y-up frame above is pvrtex's, read from its source; a mirrored
    surface turns the other way). sh4der_bump_frame_t.flip turns it round when it looks wrong.

    No allocation, no globals. The twiddled output is a uint16_t array the caller owns (a uint16_t array is 2-byte aligned;
    give it 32 bytes to upload it with pvr_txr_load). The Dreamcast part reads and writes through KallistiOS's own
    vertex target (32-byte aligned) and copies the header through memcpy for the header check: no byte pointer is cast to a
    wider one. SH4ZAM does the maths (shz_atan2f, shz_sqrtf, shz_clampf); define SH4DER_CORE_NO_SH4ZAM for the plain C path.

    The submit: one header per list run, then one strip per cell: sh4der_bump_send_header once, sh4der_bump_send_fan per cell (32 B and a header check
    less per cell than sh4der_bump_submit_fan, which does both). All through KallistiOS direct render (pvr_dr_target / pvr_dr_commit, the store queues),
    the fast path for small polygons.

    Version: v0.14 (the PVR bump unit passes and the hot passes of v0.13 are gone: the highlight power 2 passes are the method)
    The effect: the grooves and ribs of a flat texture lit by a moving light, per pixel, at the cost of a few add passes over the surface.
    Developed for: Dreamcast clients that want relief on a large flat surface (a floor, a wall) lit by a torch.
    Use it for: any surface where the light moves and the relief is worth the passes.

    \author Jilted Valkyrie

    PVR terms (TA, TR/OP list, EOL, twiddled, direct render): see the PVR glossary in README.md.
*/

#ifndef SH4DER_BUMP_H
#define SH4DER_BUMP_H

#include <stdint.h>
#include <string.h>
#include "sh4der_core.h"
#include "sh4der_guard.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Default tuning
    @{ */
#define SH4DER_BUMP_STRENGTH 0.65f   /**< \brief The strength of the effect at full light (the value of the game it was first made for; the torch walk scales the highlight by it). */
#define SH4DER_BUMP_TWO_PI   6.28318530718f
#define SH4DER_BUMP_HALF_PI  1.57079632679f
#define SH4DER_BUMP_MAX_FAN  12      /**< \brief Most vertices a fan sent by sh4der_bump_send_fan may have. */
/** @} */

/** \brief A surface's frame in world space: the direction texture u grows, the direction v grows (image rows run that way), and the normal
    facing the viewer. All three unit length. flip: 0 for a normal map whose y runs up the image (pvrtex, sh4der_bump_from_height); 1 for one
    whose y runs down, a mirrored surface, or a console that turns the other way. */
typedef struct sh4der_bump_frame {
    float t[3];   /**< \brief World direction of +u. */
    float b[3];   /**< \brief World direction of +v (the way image rows run). */
    float n[3];   /**< \brief Normal, toward the viewer. */
    int flip;
} sh4der_bump_frame_t;

/** \brief The flip a frame needs for a normal map whose y runs up the image (pvrtex, sh4der_bump_from_height): 0 when t x b = -n (the texel frame
    x = +u, y = -v, z = n is right handed: the floor t = +x, b = +z, n = +y), 1 when t x b = +n (the picture is mirrored on that surface, so is
    its relief). b is always the world direction +v grows, up a wall or down it. A map whose y runs down the image wants the other value. */
static inline int sh4der_bump_frame_flip(const float t[3], const float b[3], const float n[3]) {
#ifndef SH4DER_CORE_NO_SH4ZAM
    return shz_vec3_dot(shz_vec3_cross(shz_vec3_init(t[0], t[1], t[2]), shz_vec3_init(b[0], b[1], b[2])), shz_vec3_init(n[0], n[1], n[2])) > 0.0f;
#else
    const float c[3] = { t[1] * b[2] - t[2] * b[1], t[2] * b[0] - t[0] * b[2], t[0] * b[1] - t[1] * b[0] };
    return c[0] * n[0] + c[1] * n[1] + c[2] * n[2] > 0.0f;
#endif
}

/* Helpers (inline; a trailing underscore: not for callers). */

static inline float sh4der_bump_atan2_(float y, float x) {
#ifndef SH4DER_CORE_NO_SH4ZAM
    return shz_atan2f(y, x);
#else
    return atan2f(y, x);
#endif
}

static inline float sh4der_bump_sqrt_(float x) {
#ifndef SH4DER_CORE_NO_SH4ZAM
    return shz_sqrtf(x);
#else
    return sqrtf(x);
#endif
}

static inline float sh4der_bump_clamp_(float x, float lo, float hi) {
#ifndef SH4DER_CORE_NO_SH4ZAM
    return shz_clampf(x, lo, hi);
#else
    return x < lo ? lo : x > hi ? hi : x;
#endif
}

/* Round a 0..255 float to a byte. */
static inline unsigned sh4der_bump_byte_(float x) {
    return (unsigned)(sh4der_bump_clamp_(x, 0.0f, 255.0f) + 0.5f);
}

/* ------------------------------------------------------------------------ */
/* The texture. */

/** \brief The bump texel of a surface normal (nx, ny, nz) in the texture's frame: x along +u, y up the image (-v), z out of the surface. Any length
    (it is normalised by the angles); a normal below the surface plane is clamped to the edge. */
static inline uint16_t sh4der_bump_texel(float nx, float ny, float nz) {
    const float planar = sh4der_bump_sqrt_(sh4der_len2_(nx, ny));
    float alt = sh4der_bump_atan2_(planar, nz);
    float az = planar > 1.0e-12f ? sh4der_bump_atan2_(ny, nx) : 0.0f;
    unsigned s, r;
    if (az < 0.0f)
        az += SH4DER_BUMP_TWO_PI;
    alt = sh4der_bump_clamp_(alt, 0.0f, SH4DER_BUMP_HALF_PI);
    s = sh4der_bump_byte_(alt * (255.0f / SH4DER_BUMP_HALF_PI)) ^ 0xffu;
    r = sh4der_bump_byte_(az * (255.0f / SH4DER_BUMP_TWO_PI)) & 0xffu;
    return (uint16_t)((s << 8) | r);
}

/** \brief The twiddled index of texel (x, y), both under 256: y on the even bits, x on the odd bits. */
static inline unsigned sh4der_bump_twiddle(unsigned x, unsigned y) {
    unsigned r = 0, i;
    for (i = 0; i < 8; i++)
        r |= (((y >> i) & 1u) << (2u * i)) | (((x >> i) & 1u) << (2u * i + 1u));
    return r;
}

/** \brief A whole bump texture from an 8-bit height map (side x side, side a power of two up to 256, rows from the top, edges wrap).
    `relief` is the depth of the full height range in texels (a groove 2 texels deep between 0 and 255 is 2.0). Writes
    `out[side * side]`, twiddled. */
static inline void sh4der_bump_from_height(uint16_t *out, const uint8_t *height, unsigned side, float relief) {
    const unsigned mask = side - 1u;
    const float k = 0.5f * relief * (1.0f / 255.0f);
    unsigned x, y;
    for (y = 0; y < side; y++) {
        for (x = 0; x < side; x++) {
            const unsigned l = (x - 1u) & mask, r = (x + 1u) & mask, u = (y - 1u) & mask, d = (y + 1u) & mask;
            const float nx = (float)((int)height[y * side + l] - (int)height[y * side + r]) * k;
            const float ny = (float)((int)height[d * side + x] - (int)height[u * side + x]) * k;
            out[sh4der_bump_twiddle(x, y)] = sh4der_bump_texel(nx, ny, 1.0f);
        }
    }
}

/* ------------------------------------------------------------------------ */
/* The light. */

/** \brief The unit direction from a surface point toward a light at `light_pos` (zero when they are the same point). */
static inline void sh4der_bump_toward(const float point[3], const float light_pos[3], float out[3]) {
    out[0] = light_pos[0] - point[0];
    out[1] = light_pos[1] - point[1];
    out[2] = light_pos[2] - point[2];
    sh4der_norm3_(out);
}

/* ------------------------------------------------------------------------ */
/* The normal map on the CPU: the normals the texels hold, decoded into RAM once, for the grey maps and to light tessellated vertices (no VRAM, 3 x side^2 bytes of RAM). */

static inline float sh4der_bump_pow16_(float x) {   /* x^16 of x clamped to 0..1: four squarings (shz_powif, exact) */
    x = sh4der_bump_clamp_(x, 0.0f, 1.0f);
#ifndef SH4DER_CORE_NO_SH4ZAM
    return shz_powif(x, 16);
#else
    x *= x; x *= x; x *= x;
    return x * x;
#endif
}

/** \brief Decode a normal map texture (twiddled, side x side) into normals: row-major from the top, 3 int8 a texel (x along +u, y up the image, z out of the surface),
    x 127 rounded. A flat texel decodes to exactly (0, 0, 127). Writes `out[3 * side * side]` (byte access only, any alignment). */
static inline void sh4der_bump_normal_decode(int8_t *out, const uint16_t *tw, unsigned side) {
    unsigned x, y, k;
    for (y = 0; y < side; y++) {
        for (x = 0; x < side; x++) {
            const unsigned t = tw[sh4der_bump_twiddle(x, y)];
            float sa, ca, sz, cz, n[3];
            int8_t *o = out + 3u * (y * side + x);
            sh4der_sincos_((float)((t >> 8) ^ 0xffu) * (SH4DER_BUMP_HALF_PI / 255.0f), &sa, &ca);
            sh4der_sincos_((float)(t & 0xffu) * (SH4DER_BUMP_TWO_PI / 255.0f), &sz, &cz);
            n[0] = sa * cz; n[1] = sa * sz; n[2] = ca;
            for (k = 0; k < 3u; k++)
                o[k] = (int8_t)(int)sh4der_fma_(sh4der_bump_clamp_(n[k], -1.0f, 1.0f), 127.0f, n[k] < 0.0f ? -0.5f : 0.5f);
        }
    }
}

/** \brief The normal at picture coordinates (u, v) (1 = one texture width; wraps like the texture), bilinear between the four texels round it and made unit length
    again (a blend of unit normals is shorter: unrenormalised, a gently curved area reads darker than flat). `n` from sh4der_bump_normal_decode, side a power of two. */
static inline void sh4der_bump_normal_at(const int8_t *n, unsigned side, float u, float v, float out[3]) {
    const float fx = sh4der_fma_(u, (float)side, -0.5f), fy = sh4der_fma_(v, (float)side, -0.5f);
    const float ax = sh4der_frac_(fx), ay = sh4der_frac_(fy);
    const unsigned mask = side - 1u;
    const unsigned x0 = (unsigned)(int)(fx - ax) & mask, x1 = (x0 + 1u) & mask, y0 = (unsigned)(int)(fy - ay) & mask, y1 = (y0 + 1u) & mask;
    const int8_t *p00 = n + 3u * (y0 * side + x0), *p10 = n + 3u * (y0 * side + x1), *p01 = n + 3u * (y1 * side + x0), *p11 = n + 3u * (y1 * side + x1);
    unsigned k;
    for (k = 0; k < 3u; k++)
        out[k] = sh4der_lerp_(sh4der_lerp_((float)p00[k], (float)p10[k], ax), sh4der_lerp_((float)p01[k], (float)p11[k], ax), ay);
    sh4der_norm3_(out);
}

/** \brief Light one vertex by a CPU normal: returns delta = N.L - n.L (the normal `n_tex` from sh4der_bump_normal_at against the flat surface: 0 on flat texels,
    + on slopes facing the light, - facing away) and writes `*hot` = max(0, (N.L)^16 - (n.L)^16), the highlight the slope adds. `light` unit, toward the light,
    same space as the frame. Use: colour += envelope x (kd x delta + ks x hot) (the torch walk: kd 1.3, ks 2.5). */
static inline float sh4der_bump_vertex_light(const float n_tex[3], const sh4der_bump_frame_t *f, const float light[3], float *hot) {
    const float tl = sh4der_dot3_(light, f->t), bl = sh4der_dot3_(light, f->b), nl = sh4der_dot3_(light, f->n);
    const float dn = sh4der_fma_(n_tex[0], tl, sh4der_fma_(f->flip ? n_tex[1] : -n_tex[1], bl, n_tex[2] * nl));   /* texel y runs up the picture (-b), as the highlight passes' frame */
    const float h = sh4der_bump_pow16_(dn) - sh4der_bump_pow16_(nl);
    *hot = h > 0.0f ? h : 0.0f;
    return dn - nl;
}

/* ------------------------------------------------------------------------ */
/* The Dreamcast side: headers and a polygon. Header only, so a client that never calls it takes nothing. */

#ifdef _arch_dreamcast
#include <dc/pvr.h>

/** \brief A screen-space vertex of a normal map or base polygon: x, y, z = 1 / w (above 1 is nearer), u, v, the vertex colour. */
typedef struct sh4der_bump_vert {
    float x, y, z, u, v;
    uint32_t argb;
} sh4der_bump_vert_t;

/** \brief The grey ramp of the grey maps in palette bank `bank` (256 entries, RGB565). The palette format is one register for every bank: this sets
    PVR_PAL_RGB565 (ARGB8888 would halve the fill rate of paletted textures, KallistiOS dc/pvr/pvr_pal.h); a client with other palettes keeps them RGB565. */
static inline void sh4der_bump_grey_palette(unsigned bank) {
    unsigned i;
    pvr_set_pal_format(PVR_PAL_RGB565);
    for (i = 0; i < 256u; i++)
        pvr_set_pal_entry(bank * 256u + i, ((i >> 3) << 11) | ((i >> 2) << 5) | (i >> 3));
}

/** \brief Send the header `hdr` that the strips after it are drawn under (one per run of cells with the same header). Returns 1 when sent;
    0, sending nothing, for a header that was never compiled: then send no strip under it either. */
static inline int sh4der_bump_send_header(const pvr_poly_hdr_t *hdr) {
    uint32_t words[8];
    memcpy(words, hdr, sizeof(words));   /* a header is eight 32-bit words; copy, never cast */
    if (!sh4der_poly_hdr_ok(words))
        return 0;
    pvr_prim((void *)hdr, sizeof(*hdr));
    return 1;
}

/** \brief Send a convex polygon of 3..SH4DER_BUMP_MAX_FAN vertices (in order round the edge) as one strip under the last header sent. A polygon with fewer
    than 3 points, or more than the most, sends nothing. Returns the vertices sent. */
static inline unsigned sh4der_bump_send_fan(const sh4der_bump_vert_t *v, unsigned n) {
    unsigned i, lo = 1, hi, sent = 0;
    if (n < 3u || n > SH4DER_BUMP_MAX_FAN)
        return 0;
    hi = n - 1u;
    for (i = 0; i < n; i++) {
        /* the fan as a strip: v0, v1, v(n-1), v2, v(n-2), ... : every three in a row is one triangle of the fan */
        const unsigned k = i == 0 ? 0u : (i & 1u) ? lo++ : hi--;
        const sh4der_bump_vert_t *s = &v[k];
        pvr_vertex_t *out = pvr_dr_target();
        out->flags = i + 1u == n ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        out->x = s->x;
        out->y = s->y;
        out->z = s->z;
        out->u = s->u;
        out->v = s->v;
        out->argb = s->argb;
        out->oargb = 0u;
        pvr_dr_commit(out);
        sent++;
    }
    return sent;
}

/** \brief The header and one polygon: sh4der_bump_send_header, then sh4der_bump_send_fan. Returns the vertices sent (0 for a bad header). */
static inline unsigned sh4der_bump_submit_fan(const pvr_poly_hdr_t *hdr, const sh4der_bump_vert_t *v, unsigned n) {
    if (n < 3u || n > SH4DER_BUMP_MAX_FAN || !sh4der_bump_send_header(hdr))
        return 0;
    return sh4der_bump_send_fan(v, n);
}
#endif /* _arch_dreamcast */

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_BUMP_H */
