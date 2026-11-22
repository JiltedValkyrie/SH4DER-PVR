/* SH4DER-PVR: PVR hardware bump mapping

   sh4der_bump.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_bump.h
    \brief   The PowerVR2's own bump mapping, as a few small functions: pack a bump texel, pack the light into an offset
             colour, build the polygon headers, send a polygon.

    What the hardware does. A bump texel is not a colour, it is a surface normal in the texture's own frame, kept as two
    bytes: S, the slope (the angle the normal leans away from straight up, 0 = flat), and R, the direction it leans in
    (0..255 round the full circle). The light travels in the polygon's offset colour: four bytes K1, K2, K3, Q. For every
    pixel the PVR combines the texel and the offset colour and returns a brightness; a flat texel returns the brightness
    of a surface facing the light squarely, a slope facing the light returns more, a slope facing away returns less. The
    brightness leaves the polygon as its colour, so what a bump polygon draws is a light level, to be multiplied onto
    a lit picture. One light, one direction per polygon: a large floor is cut into cells, each with its own light.

    A multiply (DESTCOLOR x ZERO) can only darken: a level of 1 is the picture itself. So the sides facing the light are lit by a third
    pass, HOT (sh4der_bump_header_hot + sh4der_bump_oargb_hot): DESTCOLOR x ONE, the picture times 1 + a level that is 0 on flat texels
    and on slopes facing away. Overlay then hot: dark on one side of a groove, hot on the other, flat unchanged.

    Two ways to put the level on screen, both here:
    - OVERLAY (shipped in Halo: Dreamcast Evolved, level.c make_headers_work / bump_setup): the lit base is drawn first
      (opaque list), then the bump polygon on the translucent list with DESTCOLOR x ZERO: the bump level multiplies what is
      already on screen. The vertex colour is a grey that fades the effect (1 - strength, white = no effect).
    - TWO PASS (the KallistiOS example examples/dreamcast/pvr/bumpmap/bump.c): the bump polygon is drawn on the opaque list
      with colour 0 and the specular term on, so it leaves only the level; then the base is drawn on the translucent list
      with DESTCOLOR x ZERO, so it is multiplied by that level.

    The conventions, and where each was read:
    - Texel layout: KallistiOS utils/pvrtex/pixel.h pxlFloattoSpherical. S = (altitude / (pi / 2) x 255) xor 0xff, R = azimuth /
      (2 pi) x 255, texel = (S << 8) | R, altitude = acos(nz), azimuth = atan2(ny, nx). Twiddled 16 bit, no mip levels
      (a bump map has none here). pvrtex builds the normal from a height map (utils/pvrtex/pvr_texture_encoder.c) with
      nx = h(left) - h(right), ny = h(below) - h(above), image rows running down: a height that rises toward the top tilts
      the normal toward -y, so the texel's y axis runs UP the image, against texture v (x right, y up, z out: right handed).
      sh4der_bump_from_height does the same, with nz = 1 as a plain slope (KallistiOS writes 1 - nx^2 + ny^2 under the root,
      which looks like a typo for 1 - nx^2 - ny^2; the angles normalise the vector, so a plain 1 keeps the slope honest).
    - The light is read in the same frame: x along +u, y along -v (up the image), z along the normal. A sh4der_bump_frame_t
      gives +v (the way rows run), so flip 0 turns it round for you; flip 1 is for a bump map whose y runs DOWN the image.
    - Offset colour: KallistiOS dc/fmath.h pvr_pack_bump: K1 = ~h, K2 = h sin(t), K3 = h cos(t), Q = q / (2 pi); h the
      bumpiness, t the light's elevation over the surface (pi / 2 overhead), q its direction round the surface.
      sh4der_bump_oargb_kos is that. sh4der_bump_oargb is the Halo version: K1 = 255 - K2, so a flat texel facing the light
      stays at unity and the effect only darkens slopes facing away and brightens slopes facing the light.
      Which K1: both are right for their pass. KOS's K1 = 255 - h makes a flat texel read 255 - h + h sin(t): a Lambert
      term weighted by h, full only with the light overhead; use it when the bump pass IS the lighting (TWO PASS over an
      unlit picture). Halo's keeps a flat texel at 255 whatever the elevation; use it when the picture under it is already
      lit (OVERLAY over vertex light), or the light's fall-off with the angle is counted twice.
    - US patent 6,819,319 (cited by KallistiOS) describes the algorithm in the hardware.

    NOT checked on a real console: which way the hardware turns R against the texture's u axis (the y-up frame above
    is pvrtex's, read from its source; a mirrored surface turns the other way), and whether the K1 difference shows.
    sh4der_bump_frame_t.flip turns the azimuth round when it looks wrong.

    No allocation, no globals. The twiddled output is a uint16_t array the caller owns (a uint16_t array is 2-byte aligned;
    give it 32 bytes to upload it with pvr_txr_load). The Dreamcast part reads and writes through KallistiOS's own
    vertex target (32-byte aligned) and copies the header through memcpy for the header check: no byte pointer is cast to a
    wider one. SH4ZAM does the maths (shz_atan2f, shz_sqrtf, shz_clampf); define SH4DER_CORE_NO_SH4ZAM for the plain C path.

    The submit: one header per list run, then one strip per cell. A strip's own vertices carry the offset colour (the light),
    so cells with different lights share a header: sh4der_bump_send_header once, sh4der_bump_send_fan per cell (32 B and a
    header check less per cell than sh4der_bump_submit_fan, which does both). All through KallistiOS direct render
    (pvr_dr_target / pvr_dr_commit, the store queues), the fast path for small polygons.

    Version: v0.13
    The effect: the grooves and ribs of a flat texture lit by a moving light, per pixel, at the cost of one more
      polygon pass over the surface.
    Developed for: Dreamcast clients that want relief on a large flat surface (a floor, a wall) lit by a torch.
    Use it for: any surface where the light moves and the relief is worth a second pass.

    \author Jilted Valkyrie
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
#define SH4DER_BUMP_STRENGTH 0.65f   /**< \brief Share of 255 the light terms K2, K3 reach (Halo's value). */
#define SH4DER_BUMP_TWO_PI   6.28318530718f
#define SH4DER_BUMP_HALF_PI  1.57079632679f
#define SH4DER_BUMP_MAX_FAN  12      /**< \brief Most vertices a fan sent by sh4der_bump_submit_fan may have. */
/** @} */

/** \brief A surface's frame in world space: the direction texture u grows, the direction v grows (image rows run that way), and the normal
    facing the viewer. All three unit length. flip: 0 for a bump map whose y runs up the image (pvrtex, sh4der_bump_from_height); 1 for one
    whose y runs down, a mirrored surface, or a console that turns the other way. */
typedef struct sh4der_bump_frame {
    float t[3];   /**< \brief World direction of +u. */
    float b[3];   /**< \brief World direction of +v (the way image rows run). */
    float n[3];   /**< \brief Normal, toward the viewer. */
    int flip;
} sh4der_bump_frame_t;

/** \brief The flip a frame needs for a bump map whose y runs up the image (pvrtex, sh4der_bump_from_height): 0 when t x b = -n (the texel frame
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

/* The light in the texel's frame: x along +u, y up the image (-v; +v when flip), z along the normal (never negative); q its direction 0..2 pi. */
static inline void sh4der_bump_local_(const float light[3], const sh4der_bump_frame_t *f, float *planar, float *z, float *q) {
    const float x = sh4der_dot3_(light, f->t), y = (f->flip ? 1.0f : -1.0f) * sh4der_dot3_(light, f->b);
    const float p2 = sh4der_len2_(x, y);
    float a = p2 > 1.0e-12f ? sh4der_bump_atan2_(y, x) : 0.0f;
    if (a < 0.0f)
        a += SH4DER_BUMP_TWO_PI;
    *planar = sh4der_min1_(sh4der_bump_sqrt_(p2));
    *z = sh4der_sat_(sh4der_dot3_(light, f->n));
    *q = a;
}

static inline uint32_t sh4der_bump_pack_(unsigned k1, unsigned k2, unsigned k3, float q) {
    return (k1 << 24) | (k2 << 16) | (k3 << 8) | sh4der_bump_byte_(q * (255.0f / SH4DER_BUMP_TWO_PI));
}

/** \brief The offset colour for a unit light direction, Halo's way: K2 = strength x 255 x (light along the normal), K3 = strength x 255 x
    (light along the surface), K1 = 255 - K2 (a flat texel stays at unity). strength 0..1 (SH4DER_BUMP_STRENGTH). */
static inline uint32_t sh4der_bump_oargb(const float light[3], const sh4der_bump_frame_t *f, float strength) {
    float planar, z, q;
    unsigned k2, k3;
    sh4der_bump_local_(light, f, &planar, &z, &q);
    k2 = sh4der_bump_byte_(strength * 255.0f * z);
    k3 = sh4der_bump_byte_(strength * 255.0f * planar);
    return sh4der_bump_pack_(255u - k2, k2, k3, q);
}

/** \brief The offset colour as KallistiOS pvr_pack_bump builds it (K1 = 255 - h x 255, K2 = h sin t, K3 = h cos t), from a light direction. h is the
    bumpiness 0..1. */
static inline uint32_t sh4der_bump_oargb_kos(const float light[3], const sh4der_bump_frame_t *f, float h) {
    float planar, z, q;
    const unsigned hp = sh4der_bump_byte_(h * 255.0f);
    sh4der_bump_local_(light, f, &planar, &z, &q);
    return sh4der_bump_pack_(255u - hp, sh4der_bump_byte_((float)hp * z), sh4der_bump_byte_((float)hp * planar), q);
}

/* ------------------------------------------------------------------------ */
/* The HOT side by direction maps (v0.12; use these). The console showed the bump unit's own hot pass (sh4der_bump_oargb_hot below, K1 = K2 = 0) lighting
   FLAT texels: the hardware's sum does not cancel the flat term the way the KallistiOS / patent form says, and no negative K exists to subtract it. So the
   hot side does not use the bump unit at all: eight small 8-bit maps, made once from the bump texels, hold max(0, slope . d_k) for eight light directions
   d_k round the surface (45 degrees apart). A flat texel has no slope: 0 in every map, EXACTLY, by construction, on any console. A cell draws the two maps
   either side of its light's direction, weighted by how near (two passes; the weights sum to 1), as ordinary paletted textures: MODULATE reads the vertex
   colour, so the envelope (strength x torch cone x range fade, per corner) is Gouraud over the cell, and the blend is a SCREEN (INVDESTCOLOR x ONE:
   dst + src x (1 - dst)), so the highlight brightens toward white and never past it. First order: the vertical part of n.L - n_flat.L (light overhead)
   is left out; the cell's planar light (sh4der_bump_hot_pick) scales the envelope, so an overhead light gives no hot side, a grazing one the most. */
#define SH4DER_BUMP_HOT_DIRS 8

/** \brief The eight hot maps of a bump texture `bump` (side x side twiddled, as uploaded): out[k * side * side + i] for direction k (azimuth k x 45 degrees in
    the texel frame, x = +u, y up the image) and texel i, in the same twiddled order (an 8-bit paletted texture of side x side twiddles the same way). Each
    value is 255 x max(0, slope . d_k) / the texture's largest slope, so the map uses the whole range and the strength lives in the vertex colour. */
static inline void sh4der_bump_hot_maps(uint8_t *out, const uint16_t *bump, unsigned side) {
    const unsigned n = side * side;
    float big = 1.0e-6f, inv;
    unsigned i, k;
    for (i = 0; i < n; i++) {   /* the largest planar slope: sin(altitude) */
        float s, c;
        sh4der_sincos_((float)((bump[i] >> 8) ^ 0xffu) * (SH4DER_BUMP_HALF_PI / 255.0f), &s, &c);
        (void)c;
        big = big > s ? big : s;
    }
    inv = 255.0f / big;
    for (i = 0; i < n; i++) {
        float sa, ca, sz, cz;
        sh4der_sincos_((float)((bump[i] >> 8) ^ 0xffu) * (SH4DER_BUMP_HALF_PI / 255.0f), &sa, &ca);
        sh4der_sincos_((float)(bump[i] & 0xffu) * (SH4DER_BUMP_TWO_PI / 255.0f), &sz, &cz);
        (void)ca;
        for (k = 0; k < SH4DER_BUMP_HOT_DIRS; k++) {
            float sd, cd;
            sh4der_sincos_((float)k * (SH4DER_BUMP_TWO_PI / SH4DER_BUMP_HOT_DIRS), &sd, &cd);
            out[k * n + i] = (uint8_t)sh4der_bump_byte_(sa * (cz * cd + sz * sd) * inv);   /* slope . d_k, below 0 clamped to 0 */
        }
    }
}

/** \brief The two maps a cell draws for the light `light` (unit, world) on frame `f`: k[0], k[1] the directions either side of the light's azimuth, w[0], w[1] their
    weights (sum 1), *planar the light's share along the surface (0 overhead, 1 grazing: multiply the envelope by it). */
static inline void sh4der_bump_hot_pick(const float light[3], const sh4der_bump_frame_t *f, unsigned k[2], float w[2], float *planar) {
    float z, q, t;
    unsigned k0;
    sh4der_bump_local_(light, f, planar, &z, &q);
    t = q * (SH4DER_BUMP_HOT_DIRS / SH4DER_BUMP_TWO_PI);
    k0 = (unsigned)t;
    t -= (float)k0;
    k[0] = k0 % SH4DER_BUMP_HOT_DIRS;
    k[1] = (k0 + 1u) % SH4DER_BUMP_HOT_DIRS;
    w[0] = 1.0f - t;
    w[1] = t;
}

/** \brief A hot vertex colour: `level` 0..1 (the hot strength x envelope x planar x the map's weight) times the tint (the torch's colour, 0..1). */
static inline uint32_t sh4der_bump_hot_argb(float level, const float tint[3]) {
    const float l = sh4der_sat_(level);
    return 0xff000000u | (sh4der_bump_byte_(l * tint[0] * 255.0f) << 16) | (sh4der_bump_byte_(l * tint[1] * 255.0f) << 8) | sh4der_bump_byte_(l * tint[2] * 255.0f);
}

/** \brief The offset colour of the bump unit's HOT pass (v0.11, kept for comparison; the console lights flat texels with it, use the maps above): K1 = 0, K2 = 0, K3 = strength x 255 x (light along the surface), Q the light's direction. With K1
    and K2 at zero the hardware's sum K1 + K2 sin(S) + K3 cos(S) cos(R - Q) is K3 times the texel's slope across the light: 0 on a flat texel, above 0
    on a slope facing the light, below 0 (clamped to 0) on one facing away. That is max(0, n_bump . L - n_flat . L) to first order in the slope: the
    torch-facing sides only. A grazing light gives the most, a light overhead none (no side faces it more than the flat does). strength 0..1 for
    one pass (sh4der_bump_hot_split for more). */
static inline uint32_t sh4der_bump_oargb_hot(const float light[3], const sh4der_bump_frame_t *f, float strength) {
    float planar, z, q;
    sh4der_bump_local_(light, f, &planar, &z, &q);
    (void)z;
    return sh4der_bump_pack_(0u, 0u, sh4der_bump_byte_(sh4der_sat_(strength) * 255.0f * planar), q);
}

/** \brief A HOT offset colour made at strength 1 (sh4der_bump_oargb_hot(light, f, 1.0f)), scaled to strength s 0..1: only K3 changes, Q stays.
    Integer work, no angle: per corner of a cell for a soft edge (one sh4der_bump_oargb_hot per cell, then this per vertex, sent with
    sh4der_bump_send_fan_v). */
static inline uint32_t sh4der_bump_hot_scale(uint32_t oargb_full, float s) {
    const unsigned k3 = (oargb_full >> 8) & 0xffu;
    return (oargb_full & 0xffu) | (sh4der_bump_byte_(sh4der_sat_(s) * (float)k3) << 8);
}

/** \brief A HOT strength 0..2 (0..200 %) as the two passes that make it: `first` up to 1, `second` the rest (0: send one strip only). Each pass
    multiplies the picture by (1 + level), so 100 % lets a fully lit torch-facing side reach twice the picture and 200 % about four times (the
    frame buffer clips at white). Returns the passes to send, 0..2. */
static inline unsigned sh4der_bump_hot_split(float strength, float *first, float *second) {
    const float s = sh4der_bump_clamp_(strength, 0.0f, 2.0f);
    *first = sh4der_min1_(s);
    *second = s - *first;
    return (*first > 0.004f) + (*second > 0.004f);
}

/** \brief The vertex colour of the OVERLAY polygon: the grey 1 - strength (alpha 255). 0 strength is white: the overlay does nothing. */
static inline uint32_t sh4der_bump_fade_argb(float strength) {
    const unsigned g = sh4der_bump_byte_((1.0f - sh4der_sat_(strength)) * 255.0f);
    return 0xff000000u | (g << 16) | (g << 8) | g;
}

/* ------------------------------------------------------------------------ */
/* The bump on the CPU (v0.13): the normals the PVR reads, decoded into RAM once, to light tessellated vertices (no bump pass, no VRAM, 3 x side^2 bytes of RAM). */

static inline float sh4der_bump_pow16_(float x) {   /* x^16 of x clamped to 0..1: four squarings (shz_powif, exact) */
    x = sh4der_bump_clamp_(x, 0.0f, 1.0f);
#ifndef SH4DER_CORE_NO_SH4ZAM
    return shz_powif(x, 16);
#else
    x *= x; x *= x; x *= x;
    return x * x;
#endif
}

/** \brief Decode a bump texture (twiddled, side x side) into normals: row-major from the top, 3 int8 a texel (x along +u, y up the image, z out of the surface),
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
    const float dn = sh4der_fma_(n_tex[0], tl, sh4der_fma_(f->flip ? n_tex[1] : -n_tex[1], bl, n_tex[2] * nl));   /* texel y runs up the picture (-b), as sh4der_bump_local_ */
    const float h = sh4der_bump_pow16_(dn) - sh4der_bump_pow16_(nl);
    *hot = h > 0.0f ? h : 0.0f;
    return dn - nl;
}

/* ------------------------------------------------------------------------ */
/* The Dreamcast side: headers and a polygon. Header only, so a client that never calls it takes nothing. */

#ifdef _arch_dreamcast
#include <dc/pvr.h>

/** \brief A screen-space vertex of a bump or base polygon: x, y, z = 1 / w (above 1 is nearer), u, v, the vertex colour. */
typedef struct sh4der_bump_vert {
    float x, y, z, u, v;
    uint32_t argb;
} sh4der_bump_vert_t;

/** \brief The OVERLAY header (Halo's): translucent list, the bump texture `tex` (side x side, twiddled), DESTCOLOR x ZERO, DECAL against the vertex
    colour, no depth write, depth GEQUAL (it lies on the opaque polygon under it), specular on (the offset colour is the light). */
static inline void sh4der_bump_header_overlay(pvr_poly_hdr_t *hdr, pvr_ptr_t tex, unsigned side) {
    pvr_poly_cxt_t c;
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_BUMP | PVR_TXRFMT_TWIDDLED, side, side, tex, PVR_FILTER_NEAREST);
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.fog_type = PVR_FOG_DISABLE;
    c.gen.specular = PVR_SPECULAR_ENABLE;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    c.depth.comparison = PVR_DEPTHCMP_GEQUAL;
    c.txr.env = PVR_TXRENV_DECAL;
    c.blend.src = PVR_BLEND_DESTCOLOR;
    c.blend.dst = PVR_BLEND_ZERO;
    pvr_poly_compile(hdr, &c);
}

/** \brief The TWO PASS bump header (the KallistiOS example's first polygon): opaque list, colour 0 (send argb 0), specular on, DECAL. It leaves
    the light level and nothing else. */
static inline void sh4der_bump_header_pass(pvr_poly_hdr_t *hdr, pvr_ptr_t tex, unsigned side) {
    pvr_poly_cxt_t c;
    pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, PVR_TXRFMT_BUMP | PVR_TXRFMT_TWIDDLED, side, side, tex, PVR_FILTER_NONE);
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.fog_type = PVR_FOG_DISABLE;
    c.gen.specular = PVR_SPECULAR_ENABLE;
    c.txr.env = PVR_TXRENV_DECAL;
    pvr_poly_compile(hdr, &c);
}

/** \brief The TWO PASS base header (the example's second polygon): the picture `tex` on the translucent list, blend DESTCOLOR x ZERO, so the picture is
    multiplied by the level the first pass left. `fmt` is the texture format flags (PVR_TXRFMT_*), `mips` the mip chain flag. */
static inline void sh4der_bump_header_apply(pvr_poly_hdr_t *hdr, pvr_ptr_t tex, uint32_t fmt, unsigned side, int mips) {
    pvr_poly_cxt_t c;
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, fmt, side, side, tex, PVR_FILTER_BILINEAR);
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.fog_type = PVR_FOG_DISABLE;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    c.depth.comparison = PVR_DEPTHCMP_GEQUAL;
    c.txr.mipmap = mips ? PVR_MIPMAP_ENABLE : PVR_MIPMAP_DISABLE;
    c.blend.src = PVR_BLEND_DESTCOLOR;
    c.blend.dst = PVR_BLEND_ZERO;
    pvr_poly_compile(hdr, &c);
}

/** \brief The CORE header (the textured hot core): translucent list, a grey map `map` (8-bit paletted twiddled, side x side, palette `bank` from sh4der_bump_hot_palette)
    that holds Nz, how much each texel of the normal map faces the viewer (255 x cos of the slope S), MODULATE (the vertex colour is the strength x boost x the torch's
    tint), blend DESTCOLOR x ONE: frame x (1 + the map x the colour), so the bright texels of the map blow out first and the dark ones stay. It is the blend of the
    library's torch hot core (sh4der_torch.c), put on a texture. Send it AFTER the level overlay, over the same cells, with the colour at each corner (the cell's
    strength). No depth write, depth GEQUAL. Not checked on a real console. */
static inline void sh4der_bump_header_core(pvr_poly_hdr_t *hdr, pvr_ptr_t map, unsigned side, unsigned bank, int filter) {
    pvr_poly_cxt_t c;
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_PAL8BPP | PVR_TXRFMT_8BPP_PAL(bank) | PVR_TXRFMT_TWIDDLED, side, side, map, filter);
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.fog_type = PVR_FOG_DISABLE;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    c.depth.comparison = PVR_DEPTHCMP_GEQUAL;
    c.txr.env = PVR_TXRENV_MODULATE;
    c.blend.src = PVR_BLEND_DESTCOLOR;
    c.blend.dst = PVR_BLEND_ONE;
    pvr_poly_compile(hdr, &c);
}

/** \brief The HOT header: translucent list, the bump texture, DESTCOLOR x ONE (the picture times 1 + the level: brighter, in the picture's own
    lit colour, so the torch's warmth rides on it), DECAL, specular on (the offset colour from sh4der_bump_oargb_hot), no depth write, depth GEQUAL.
    Send it AFTER the shadow overlay (an unsorted translucent list draws in order), over the same vertices; the shadow side stays the overlay's. */
static inline void sh4der_bump_header_hot_filtered(pvr_poly_hdr_t *hdr, pvr_ptr_t tex, unsigned side, int filter) {
    pvr_poly_cxt_t c;
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_BUMP | PVR_TXRFMT_TWIDDLED, side, side, tex, filter);
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.fog_type = PVR_FOG_DISABLE;
    c.gen.specular = PVR_SPECULAR_ENABLE;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    c.depth.comparison = PVR_DEPTHCMP_GEQUAL;
    c.txr.env = PVR_TXRENV_DECAL;
    c.blend.src = PVR_BLEND_DESTCOLOR;
    c.blend.dst = PVR_BLEND_ONE;
    pvr_poly_compile(hdr, &c);
}

/** \brief The grey ramp of the hot maps in palette bank `bank` (256 entries, RGB565). The palette format is one register for every bank: this sets
    PVR_PAL_RGB565 (ARGB8888 would halve the fill rate of paletted textures, KallistiOS dc/pvr/pvr_pal.h); a client with other palettes keeps them RGB565. */
static inline void sh4der_bump_hot_palette(unsigned bank) {
    unsigned i;
    pvr_set_pal_format(PVR_PAL_RGB565);
    for (i = 0; i < 256u; i++)
        pvr_set_pal_entry(bank * 256u + i, ((i >> 3) << 11) | ((i >> 2) << 5) | (i >> 3));
}

/** \brief The header of hot map `map` (one of the eight, side x side, 8-bit paletted twiddled, palette `bank`): translucent list, MODULATE (the vertex colour is the
    envelope and the tint), SCREEN blend INVDESTCOLOR x ONE (dst + src x (1 - dst): never past white), no depth write, depth GEQUAL, `filter` as the shadow overlay's. */
static inline void sh4der_bump_header_hot_map(pvr_poly_hdr_t *hdr, pvr_ptr_t map, unsigned side, unsigned bank, int filter) {
    pvr_poly_cxt_t c;
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_PAL8BPP | PVR_TXRFMT_8BPP_PAL(bank) | PVR_TXRFMT_TWIDDLED, side, side, map, filter);
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.fog_type = PVR_FOG_DISABLE;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    c.depth.comparison = PVR_DEPTHCMP_GEQUAL;
    c.txr.env = PVR_TXRENV_MODULATE;
    c.blend.src = PVR_BLEND_INVDESTCOLOR;
    c.blend.dst = PVR_BLEND_ONE;
    pvr_poly_compile(hdr, &c);
}

/** \brief sh4der_bump_header_hot_filtered with the nearest texel (as the overlay header). Give the hot pass the SAME filter as the shadow overlay
    it lies on, or the dark and the hot sides of a groove are sampled differently. */
static inline void sh4der_bump_header_hot(pvr_poly_hdr_t *hdr, pvr_ptr_t tex, unsigned side) {
    sh4der_bump_header_hot_filtered(hdr, tex, side, PVR_FILTER_NEAREST);
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

/** \brief Send a convex polygon of 3..SH4DER_BUMP_MAX_FAN vertices (in order round the edge) as one strip under the last header sent, every
    vertex with the offset colour `oargb`. A polygon with fewer than 3 points, or more than the most, sends nothing. Returns the vertices sent. */
static inline unsigned sh4der_bump_send_fan(const sh4der_bump_vert_t *v, unsigned n, uint32_t oargb) {
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
        out->oargb = oargb;
        pvr_dr_commit(out);
        sent++;
    }
    return sent;
}

/** \brief sh4der_bump_send_fan with an offset colour per vertex (`oargb[i]` for v[i]), Gouraud-shaded across the polygon like the vertex colour.
    Keep Q (the low byte) the same on every vertex of one polygon: Q is an angle, and halfway between 250 and 5 is not 0. Not checked on a real
    console: that the PVR interpolates K3 per pixel in bump mode (the shading is Gouraud on these headers; flat would show the first vertex). */
static inline unsigned sh4der_bump_send_fan_v(const sh4der_bump_vert_t *v, unsigned n, const uint32_t *oargb) {
    unsigned i, lo = 1, hi, sent = 0;
    if (n < 3u || n > SH4DER_BUMP_MAX_FAN)
        return 0;
    hi = n - 1u;
    for (i = 0; i < n; i++) {
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
        out->oargb = oargb[k];
        pvr_dr_commit(out);
        sent++;
    }
    return sent;
}

/** \brief The header and one polygon: sh4der_bump_send_header, then sh4der_bump_send_fan. Returns the vertices sent (0 for a bad header). */
static inline unsigned sh4der_bump_submit_fan(const pvr_poly_hdr_t *hdr, const sh4der_bump_vert_t *v, unsigned n, uint32_t oargb) {
    if (n < 3u || n > SH4DER_BUMP_MAX_FAN || !sh4der_bump_send_header(hdr))
        return 0;
    return sh4der_bump_send_fan(v, n, oargb);
}
#endif /* _arch_dreamcast */

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_BUMP_H */
