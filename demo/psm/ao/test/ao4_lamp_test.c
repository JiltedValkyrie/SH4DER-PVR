/* SH4DER-PVR: host test of the lamp look's maths (demo/psm/ao/ao4_lamp.h), and its pictures in software

   demo/psm/ao/test/ao4_lamp_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

   Checks: the lamp is found in the room's model (its axis, the shade's two rings); the shade's mesh (on the cone,
   unit normals outward, triangles of both windings none degenerate, the seam's two columns in one place); which of
   the model's triangles are the shade's; the colours (outside: emission in the base and sss in the offset, brightest
   where the bulb is straight behind; inside: no offset; nothing negative); the halo (its middle on the bulb, the
   outer ring 0, the fade 0 on a plane and 1 away from it, never brighter with the fade than without); the slots.

   With a folder as the argument it also writes pictures there (PPM, 640 x 480): a software stand-in for the PVR
   (z-buffer, a colour a vertex, the 1-bit texture's alpha test, the offset colour, an additive pass with the depth
   test on and no depth write). They show the LOOK and how the halo meets the shade and the walls. They are NOT the
   PVR's picture and no cost.

   Compile with -DSH4DER_CORE_NO_SH4ZAM, the library's include folders, the room's folder (build.py --room-only), and
   src/sh4der_bloom.c (sh4der_bloom_rings). */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../ao4_scene.h"
#include "../ao4_draw.h"
#include "../ao4_room.h"
#include "../ao4_lamp.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static ao4_scene_t scene;
static ao4_lamp_t lamp;
static ao4_lamp_plane_t planes[ROOM_SURFS];
static unsigned lamp_obj, lamp_base;        /* the lamp's object; its first vertex in the scene */

static float lum(uint32_t c) { return (float)((c >> 16) & 255u) + (float)((c >> 8) & 255u) + (float)(c & 255u); }

static void t_find(void) {
    unsigned k, i, base = 0, shade = 0;
    int found = -1;

    /* the scene's vertices are the objects' in three passes (floors, walls, the rest): the lamp's first one */
    {
        unsigned pass;

        for (pass = 0; pass < 3u; pass++)
            for (k = 0; k < ROOM_OBJS; k++) {
                const int surf = room_obj[k].surf;
                const unsigned kind = surf < 0 ? 0u : room_surf[surf].kind;

                if (!((pass == 0u && kind == ROOM_FLOOR) || (pass == 1u && kind == ROOM_WALL) || (pass == 2u && kind == 0u))) continue;
                if (!strcmp(room_obj[k].name, "lamp")) { found = (int)k; lamp_base = base; }
                base += room_obj[k].vn;
            }
    }
    CHECK(found >= 0, "the room has no object named lamp");
    if (found < 0) return;
    lamp_obj = (unsigned)found;
    CHECK(ao4_lamp_find(&lamp, &room_v[room_obj[lamp_obj].v0], room_obj[lamp_obj].vn), "the lamp's shade was not found");
    printf("  lamp: axis (%.3f, %.3f), shade y %.3f .. %.3f, radius %.3f .. %.3f, bulb y %.3f\n", lamp.cx, lamp.cz, lamp.y0, lamp.y1, lamp.r0, lamp.r1, lamp.bulb[1]);
    CHECK(fabsf(lamp.cx + 2.25f) < 0.02f && fabsf(lamp.cz + 2.25f) < 0.02f, "the axis is not the model's");
    CHECK(fabsf(lamp.y0 - 1.36f) < 0.02f && fabsf(lamp.y1 - 1.70f) < 0.02f, "the shade's heights are not the model's");
    CHECK(fabsf(lamp.r0 - 0.27f) < 0.02f && fabsf(lamp.r1 - 0.19f) < 0.02f, "the shade's radii are not the model's");
    CHECK(lamp.bulb[1] > lamp.y0 && lamp.bulb[1] < lamp.y1, "the bulb is not inside the shade");
    for (i = 0; i < room_obj[lamp_obj].tn; i++) {
        const uint16_t *t = room_t[room_obj[lamp_obj].t0 + i];

        shade += (unsigned)ao4_lamp_is_shade(&lamp, room_v[t[0]], room_v[t[1]], room_v[t[2]]);
    }
    printf("  lamp: %u of its %u triangles are the shade's (left out in the look)\n", shade, (unsigned)room_obj[lamp_obj].tn);
    CHECK(shade >= 24u && shade < room_obj[lamp_obj].tn, "shade triangles: %u", shade);
    CHECK(!memcmp(scene.p[lamp_base], room_v[room_obj[lamp_obj].v0], sizeof scene.p[0]), "the lamp's place in the scene's vertices");
    {
        float none[3][3] = { { 0, 0, 0 }, { 0.01f, 1, 0 }, { 0, 2, 0.01f } };
        ao4_lamp_t l2;

        CHECK(!ao4_lamp_find(&l2, (const float (*)[3])none, 3u), "a pole alone is not a lamp with a shade");
        CHECK(!ao4_lamp_find(&l2, (const float (*)[3])none, 0u), "no vertices");
    }
    for (k = 0; k < ROOM_SURFS; k++) for (i = 0; i < 3u; i++) { planes[k].o[i] = room_surf[k].o[i]; planes[k].n[i] = room_surf[k].n[i]; }
}

static void t_mesh(void) {
    unsigned k, t, i;

    ao4_lamp_build(&lamp);
    for (k = 0; k < AO4_LAMP_V; k++) {
        const float f = lamp.uv[k][1], r = ao4_lamp_radius_(&lamp, lamp.p[k]);
        const float out[3] = { lamp.p[k][0] - lamp.cx, 0.0f, lamp.p[k][2] - lamp.cz };

        CHECK(fabsf(r - (lamp.r0 + (lamp.r1 - lamp.r0) * f)) < 1e-4f && fabsf(lamp.p[k][1] - (lamp.y0 + (lamp.y1 - lamp.y0) * f)) < 1e-4f, "vertex %u is off the cone", k);
        CHECK(fabsf(sh4der_dot3_(lamp.n[k], lamp.n[k]) - 1.0f) < 1e-4f, "normal %u is not unit", k);
        CHECK(sh4der_dot3_(lamp.n[k], out) > 0.0f && lamp.n[k][1] > 0.0f, "normal %u does not point out and up (the shade narrows upward)", k);
    }
    for (i = 0; i <= AO4_LAMP_BANDS; i++) {
        const unsigned a = i * (AO4_LAMP_SEG + 1u), b = a + AO4_LAMP_SEG;

        CHECK(fabsf(lamp.p[a][0] - lamp.p[b][0]) < 1e-5f && fabsf(lamp.p[a][2] - lamp.p[b][2]) < 1e-5f, "the seam's columns are apart at ring %u", i);
        CHECK(lamp.uv[a][0] == 0.0f && lamp.uv[b][0] == (float)AO4_LAMP_SEG, "the seam's u");
    }
    for (t = 0; t < AO4_LAMP_T; t++) {
        const float *a = lamp.p[lamp.tri[t][0]], *b = lamp.p[lamp.tri[t][1]], *c = lamp.p[lamp.tri[t][2]];
        const float e1[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] }, e2[3] = { c[0] - a[0], c[1] - a[1], c[2] - a[2] };
        float n[3];

        sh4der_cross_(e1, e2, n);
        CHECK(lamp.tri[t][0] < AO4_LAMP_V && lamp.tri[t][1] < AO4_LAMP_V && lamp.tri[t][2] < AO4_LAMP_V, "triangle %u's indices", t);
        CHECK(sh4der_dot3_(n, n) > 1e-10f, "triangle %u has no area", t);
    }
}

static void t_colours(void) {
    const float eye[3] = { lamp.cx + 3.0f, lamp.bulb[1], lamp.cz };          /* level with the bulb, along +x */
    const float above[3] = { lamp.cx + 0.02f, lamp.y1 + 2.0f, lamp.cz };     /* looking down into the shade */
    float b[3], o[3], best = -1.0f, edge = -1.0f;
    uint32_t base[AO4_LAMP_V], off[AO4_LAMP_V];
    unsigned k, c, near_k = 0;

    for (k = 0; k < AO4_LAMP_V; k++) {
        const float facing = lamp.n[k][0];

        ao4_lamp_vertex(&lamp, k, eye, AO4_CLAY_GREY, b, o);
        for (c = 0; c < 3u; c++) CHECK(b[c] >= 0.0f && o[c] >= 0.0f && b[c] < 1.5f && o[c] < 1.5f, "vertex %u colour %u out of range: %f %f", k, c, b[c], o[c]);
        if (facing > 0.95f && o[0] > best) { best = o[0]; near_k = k; }
        if (facing > 0.2f && facing < 0.6f && o[0] > edge) edge = o[0];
        if (facing < -0.3f) CHECK(o[0] == 0.0f && o[1] == 0.0f && o[2] == 0.0f, "vertex %u is seen from inside and has sss", k);
        if (facing > 0.3f) {
            CHECK(o[0] > 0.0f, "vertex %u is seen from outside and has no sss", k);
            CHECK(o[0] >= o[1] && o[1] >= o[2], "vertex %u: paper absorbs blue most, red least", k);
            CHECK(fabsf(b[0] - AO4_CLAY_GREY * lamp.warm[0] * lamp.emission) < 1e-4f, "vertex %u: the base is the emission level", k);
        }
    }
    CHECK(best > edge && edge > 0.0f, "the sss is strongest where the bulb is straight behind the paper: %f against %f toward the edge", best, edge);
    printf("  shade from the side: sss red %.3f in the middle (vertex %u), at most %.3f toward the edge; base %.3f\n", best, near_k, edge, AO4_CLAY_GREY * lamp.warm[0] * lamp.emission);
    ao4_lamp_shade(&lamp, above, AO4_CLAY_GREY, base, off);
    for (k = 0; k < AO4_LAMP_V; k++) {
        float view[3] = { above[0] - lamp.p[k][0], above[1] - lamp.p[k][1], above[2] - lamp.p[k][2] };

        sh4der_norm3_(view);
        if (sh4der_dot3_(view, lamp.n[k]) < -0.3f) CHECK((off[k] & 0xffffffu) == 0u && lum(base[k]) > 300.0f, "from above the far inside is lit paper with no offset (vertex %u: %08x %08x)", k, (unsigned)base[k], (unsigned)off[k]);
        CHECK((base[k] >> 24) == 255u && (off[k] >> 24) == 255u, "alpha");
    }
}

static void t_halo(void) {
    const float eye[3] = { lamp.cx + 3.0f, 1.6f, lamp.cz + 0.3f };           /* along a wall: the halo, facing the eye, runs into the other wall */
    static float xyz[AO4_LAMP_HALO_V][3], xyz2[AO4_LAMP_HALO_V][3];
    static uint32_t plain[AO4_LAMP_HALO_V], faded[AO4_LAMP_HALO_V], moved[AO4_LAMP_HALO_V];
    static uint8_t tris[AO4_LAMP_HALO_T][3];
    unsigned k, cut = 0, kept = 0;
    float p[3];

    CHECK(sh4der_bloom_rings_tris(AO4_LAMP_HALO_RINGS, AO4_LAMP_HALO_SEG, tris) == AO4_LAMP_HALO_T, "the halo's triangle count");
    ao4_lamp_halo(&lamp, eye, 0.0f, NULL, 0u, xyz, plain);
    CHECK(fabsf(xyz[0][0] - lamp.bulb[0]) < 1e-5f && fabsf(xyz[0][1] - lamp.bulb[1]) < 1e-5f && fabsf(xyz[0][2] - lamp.bulb[2]) < 1e-5f, "the halo's middle is not the bulb");
    CHECK(lum(plain[0]) > 250.0f, "the halo's middle level");
    for (k = AO4_LAMP_HALO_V - AO4_LAMP_HALO_SEG; k < AO4_LAMP_HALO_V; k++) CHECK((plain[k] & 0xffffffu) == 0u, "the outer ring is not 0 at %u", k);
    for (k = 1; k < AO4_LAMP_HALO_V; k++) {
        const float d[3] = { xyz[k][0] - lamp.bulb[0], xyz[k][1] - lamp.bulb[1], xyz[k][2] - lamp.bulb[2] };
        const float v[3] = { eye[0] - lamp.bulb[0], eye[1] - lamp.bulb[1], eye[2] - lamp.bulb[2] };

        CHECK(fabsf(sh4der_dot3_(d, v)) < 1e-3f, "halo vertex %u is not in the plane facing the eye", k);
        CHECK(sqrtf(sh4der_dot3_(d, d)) <= lamp.halo_radius + 1e-4f, "halo vertex %u is outside its radius", k);
    }
    ao4_lamp_halo(&lamp, eye, 0.0f, planes, ROOM_SURFS, xyz2, faded);
    for (k = 0; k < AO4_LAMP_HALO_V; k++) {
        CHECK(!memcmp(xyz[k], xyz2[k], sizeof xyz[0]), "the fade moved vertex %u", k);
        CHECK(lum(faded[k]) <= lum(plain[k]), "the fade brightened vertex %u", k);
        if (lum(plain[k]) > 0.0f && lum(faded[k]) == 0.0f) cut++;
        if (lum(plain[k]) > 0.0f && faded[k] == plain[k]) kept++;
        if (xyz[k][0] < -3.0f || xyz[k][2] < -3.0f || xyz[k][1] < 0.0f) CHECK(lum(faded[k]) == 0.0f, "halo vertex %u is behind a wall or under the floor and is lit", k);
    }
    printf("  halo: %u vertices; with the fade %u go dark (behind a wall), %u keep their level\n", (unsigned)AO4_LAMP_HALO_V, cut, kept);
    CHECK(cut > 0u && kept > 0u, "the lamp is 0.75 m from two walls and its halo 0.95 m: the fade must cut some of it and leave some");
    {   /* the library's fade (sh4der_softfade.h) against the form the halo had before it: plain x ao4_lamp_fade, a channel */
        unsigned worst = 0, differ = 0, ch;

        for (k = 0; k < AO4_LAMP_HALO_V; k++) {
            const float keep = ao4_lamp_fade(&lamp, xyz[k], planes, ROOM_SURFS);

            for (ch = 0; ch < 24u; ch += 8u) {
                const float want = (float)((plain[k] >> ch) & 255u) * keep, got = (float)((faded[k] >> ch) & 255u);
                const unsigned e = (unsigned)(fabsf(got - want) + 0.5f);

                if (e > worst) worst = e;
                differ += fabsf(got - want) > 0.75f;
            }
        }
        printf("  halo: the library's fade against the old form: largest difference %u of 255 (the old level rounded twice), %u of %u channels over 0.75\n",
               worst, differ, 3u * (unsigned)AO4_LAMP_HALO_V);
        CHECK(worst <= 1u, "the library's fade is more than a level from the old form (%u)", worst);
    }
    /* the fade itself */
    p[0] = -3.0f; p[1] = 1.0f; p[2] = 0.0f;
    CHECK(ao4_lamp_fade(&lamp, p, planes, ROOM_SURFS) == 0.0f, "on a wall the fade is 0");
    p[0] = -3.0f + 0.5f * lamp.halo_fade;
    CHECK(fabsf(ao4_lamp_fade(&lamp, p, planes, ROOM_SURFS) - 0.5f) < 0.02f, "half the fade distance from one wall: half");
    p[0] = 0.0f;
    CHECK(ao4_lamp_fade(&lamp, p, planes, ROOM_SURFS) == 1.0f, "in the middle of the room the fade is 1");
    p[1] = -0.2f;
    CHECK(ao4_lamp_fade(&lamp, p, planes, ROOM_SURFS) == 0.0f, "under the floor the fade is 0");
    /* moved toward the eye */
    ao4_lamp_halo(&lamp, eye, 0.3f, planes, ROOM_SURFS, xyz2, moved);
    {
        const float d[3] = { xyz2[0][0] - lamp.bulb[0], xyz2[0][1] - lamp.bulb[1], xyz2[0][2] - lamp.bulb[2] };

        CHECK(fabsf(sqrtf(sh4der_dot3_(d, d)) - 0.3f) < 1e-4f, "toward 0.3 moves the middle 0.3 m");
    }
}

static void t_slots(void) {
    static uint16_t px[AO4_LAMP_TEX * AO4_LAMP_TEX];
    unsigned i, open = 0;

    CHECK(ao4_lamp_slot(0.5f, 0.5f) && ao4_lamp_slot(7.5f, 0.5f) && !ao4_lamp_slot(0.1f, 0.5f) && !ao4_lamp_slot(0.5f, 0.1f) && !ao4_lamp_slot(0.5f, 0.9f), "where a slot is");
    ao4_lamp_pattern(px, 1);
    for (i = 0; i < AO4_LAMP_TEX * AO4_LAMP_TEX; i++) { CHECK(px[i] == 0xffffu || px[i] == 0x7fffu, "a texel is neither paper nor a slot"); open += px[i] == 0x7fffu; }
    for (i = 0; i < AO4_LAMP_TEX; i++) CHECK(px[i] == 0xffffu && px[(AO4_LAMP_TEX - 1u) * AO4_LAMP_TEX + i] == 0xffffu && px[i * AO4_LAMP_TEX] == 0xffffu, "the texture's rim is paper (it repeats)");
    printf("  slots: %u of %u texels cut out (%.0f %% of the paper)\n", open, (unsigned)(AO4_LAMP_TEX * AO4_LAMP_TEX), 100.0f * (float)open / (float)(AO4_LAMP_TEX * AO4_LAMP_TEX));
    CHECK(open > 100u && open < 300u, "slot texels: %u", open);
    ao4_lamp_pattern(px, 0);
    for (i = 0; i < AO4_LAMP_TEX * AO4_LAMP_TEX; i++) CHECK(px[i] == 0xffffu, "slots off is all paper");
}

/* ---- the pictures: a software stand-in ---------------------------------------------------------------------- */

static float img[480 * 640][3], zb[480 * 640];
static uint16_t tex[AO4_LAMP_TEX * AO4_LAMP_TEX];

typedef struct pv { float s[3]; float c[3], o[3], uv[2]; } pv_t;   /* screen x, y, 1 / depth; base, offset; texture */

/* mode 0: opaque (depth test and write). 1: punch-through (the texture's alpha test, x base, + offset; test and
   write). 2: additive (test, no write). */
static void tri(const pv_t *a, const pv_t *b, const pv_t *c, int mode) {
    const float area = (b->s[0] - a->s[0]) * (c->s[1] - a->s[1]) - (b->s[1] - a->s[1]) * (c->s[0] - a->s[0]);
    int x0, x1, y0, y1, x, y;
    float inv;
    unsigned k;

    if (fabsf(area) < 1e-6f) return;
    inv = 1.0f / area;
    x0 = (int)floorf(fminf(a->s[0], fminf(b->s[0], c->s[0]))); x1 = (int)ceilf(fmaxf(a->s[0], fmaxf(b->s[0], c->s[0])));
    y0 = (int)floorf(fminf(a->s[1], fminf(b->s[1], c->s[1]))); y1 = (int)ceilf(fmaxf(a->s[1], fmaxf(b->s[1], c->s[1])));
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > 639) x1 = 639;
    if (y1 > 479) y1 = 479;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            const float px = (float)x + 0.5f, py = (float)y + 0.5f;
            const float w0 = ((b->s[0] - px) * (c->s[1] - py) - (b->s[1] - py) * (c->s[0] - px)) * inv;
            const float w1 = ((c->s[0] - px) * (a->s[1] - py) - (c->s[1] - py) * (a->s[0] - px)) * inv;
            const float w2 = 1.0f - w0 - w1;
            float z, col[3];

            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;
            z = w0 * a->s[2] + w1 * b->s[2] + w2 * c->s[2];
            if (z < zb[y * 640 + x]) continue;
            if (mode == 1) {                /* perspective-correct, as the PVR's: by 1 / depth */
                const float iz = 1.0f / z;
                const float u = (w0 * a->uv[0] * a->s[2] + w1 * b->uv[0] * b->s[2] + w2 * c->uv[0] * c->s[2]) * iz;
                const float v = (w0 * a->uv[1] * a->s[2] + w1 * b->uv[1] * b->s[2] + w2 * c->uv[1] * c->s[2]) * iz;
                const int tx = (int)((u - floorf(u)) * (float)AO4_LAMP_TEX) & (int)(AO4_LAMP_TEX - 1u), ty = (int)(v * (float)AO4_LAMP_TEX);

                if (!(tex[(unsigned)(ty < 0 ? 0 : ty > 31 ? 31 : ty) * AO4_LAMP_TEX + (unsigned)tx] & 0x8000u)) continue;
            }
            for (k = 0; k < 3u; k++) col[k] = w0 * a->c[k] + w1 * b->c[k] + w2 * c->c[k] + (mode == 1 ? w0 * a->o[k] + w1 * b->o[k] + w2 * c->o[k] : 0.0f);
            if (mode == 2) { for (k = 0; k < 3u; k++) img[y * 640 + x][k] += col[k]; }
            else { for (k = 0; k < 3u; k++) img[y * 640 + x][k] = col[k]; zb[y * 640 + x] = z; }
        }
}

static void unpack(uint32_t argb, float scale, float c[3]) {
    c[0] = (float)((argb >> 16) & 255u) * (scale / 255.0f); c[1] = (float)((argb >> 8) & 255u) * (scale / 255.0f); c[2] = (float)(argb & 255u) * (scale / 255.0f);
}

enum { P_OFF = 0, P_GLOW, P_SLOTS, P_HALO_PLAIN, P_HALO_FADE, P_HALO_TOWARD, P_HALO_FADE_SLOTS, P_HALO_TOWARD_SLOTS };

/* One picture. what: P_ above. The camera: angle, radius, height round the lamp, or the room's own view (radius 0). */
static void picture(const char *dir, const char *name, int what, float angle, float radius, float height) {
    static uint32_t col[AO4_V_MAX], base[AO4_LAMP_V], off[AO4_LAMP_V], hc[AO4_LAMP_HALO_V];
    static float scr[AO4_V_MAX][3], hx[AO4_LAMP_HALO_V][3];
    static uint8_t htri[AO4_LAMP_HALO_T][3];
    const int on = what != P_OFF, slots = what == P_SLOTS || what == P_HALO_FADE_SLOTS || what == P_HALO_TOWARD_SLOTS;
    const int halo = what >= P_HALO_PLAIN, fade = what != P_HALO_PLAIN, toward = what == P_HALO_TOWARD || what == P_HALO_TOWARD_SLOTS;
    ao4_cam_t cam;
    unsigned st, i, k = 0, t;
    char path[300];
    FILE *f;

    if (radius > 0.0f) {
        ao4_cam_make(&cam, angle, radius, height, lamp.cx, lamp.cz);
        cam.f[0] = lamp.bulb[0] - cam.e[0]; cam.f[1] = lamp.bulb[1] - cam.e[1]; cam.f[2] = lamp.bulb[2] - cam.e[2];   /* look at the bulb */
        sh4der_norm3_(cam.f);
        { const float up[3] = { 0.0f, 1.0f, 0.0f }; sh4der_cross_(up, cam.f, cam.r); sh4der_norm3_(cam.r); sh4der_cross_(cam.f, cam.r, cam.u); }
        cam.focal = 900.0f;
        cam.r[0] = -cam.r[0]; cam.r[1] = -cam.r[1]; cam.r[2] = -cam.r[2];
    } else ao4_cam_at(&scene, &cam, 0.0f, 0.0f, 0.0f);
    for (i = 0; i < 480u * 640u; i++) { img[i][0] = img[i][1] = img[i][2] = on ? 0.02f : 0.10f; zb[i] = 0.0f; }
    ao4_light(&scene, AO4_BAKED, AO4_CLAY, 0, col);
    for (i = 0; i < scene.vn; i++) ao4_cam_pt(&cam, scene.p[i], scr[i]);
    for (st = 0; st < scene.strips; st++) {
        const unsigned n = scene.slen[st];

        for (t = 0; t + 2u < n; t++) {
            const unsigned ia = scene.idx[k + t], ib = scene.idx[k + t + 1u], ic = scene.idx[k + t + 2u];
            pv_t v[3];
            unsigned q;
            const unsigned id[3] = { ia, ib, ic };

            if (scr[ia][2] > 1.5f || scr[ib][2] > 1.5f || scr[ic][2] > 1.5f) continue;        /* not in front of the eye: left out, no clip here */
            if (on && n == 3u && ia >= lamp_base && ia < lamp_base + room_obj[lamp_obj].vn && ao4_lamp_is_shade(&lamp, scene.p[ia], scene.p[ib], scene.p[ic])) continue;
            for (q = 0; q < 3u; q++) { memcpy(v[q].s, scr[id[q]], sizeof v[q].s); unpack(col[id[q]], on ? AO4_LAMP_DIM : 1.0f, v[q].c); }
            tri(&v[0], &v[1], &v[2], 0);
        }
        k += n;
    }
    if (on) {
        ao4_lamp_pattern(tex, slots);
        ao4_lamp_shade(&lamp, cam.e, AO4_CLAY_GREY, base, off);
        for (t = 0; t < AO4_LAMP_T; t++) {
            pv_t v[3];
            unsigned q, bad = 0;

            for (q = 0; q < 3u; q++) {
                const unsigned x = lamp.tri[t][q];

                bad += !ao4_cam_pt(&cam, lamp.p[x], v[q].s);
                unpack(base[x], 1.0f, v[q].c); unpack(off[x], 1.0f, v[q].o);
                v[q].uv[0] = lamp.uv[x][0]; v[q].uv[1] = lamp.uv[x][1];
            }
            if (!bad) tri(&v[0], &v[1], &v[2], 1);
        }
    }
    if (halo) {
        const unsigned nt = sh4der_bloom_rings_tris(AO4_LAMP_HALO_RINGS, AO4_LAMP_HALO_SEG, htri);

        ao4_lamp_halo(&lamp, cam.e, toward ? 0.3f : 0.0f, planes, fade ? ROOM_SURFS : 0u, hx, hc);
        for (t = 0; t < nt; t++) {
            pv_t v[3];
            unsigned q, bad = 0;

            for (q = 0; q < 3u; q++) { bad += !ao4_cam_pt(&cam, hx[htri[t][q]], v[q].s); unpack(hc[htri[t][q]], 1.0f, v[q].c); }
            if (!bad) tri(&v[0], &v[1], &v[2], 2);
        }
    }
    snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
    f = fopen(path, "wb");
    if (!f) { printf("FAIL: cannot write %s\n", path); fails++; return; }
    fprintf(f, "P6\n640 480\n255\n");
    for (i = 0; i < 480u * 640u; i++) {
        unsigned char rgb[3];

        for (k = 0; k < 3u; k++) rgb[k] = (unsigned char)(sh4der_sat_(img[i][k]) * 255.0f + 0.5f);
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

int main(int argc, char **argv) {
    ao4_build_room(&scene);
    t_find();
    if (!fails) { t_mesh(); t_colours(); t_halo(); t_slots(); }
    if (argc > 1 && !fails) {
        static const struct { const char *name; float angle, radius, height; } view[4] = {
            { "room", 0.0f, 0.0f, 0.0f },                       /* the demo's own view */
            { "front", 0.785398f, 2.4f, 1.75f },                /* from the room's middle, a little above the bulb */
            { "side", 0.12f, 2.4f, 1.55f },                     /* along the wall */
            { "below", 0.785398f, 1.15f, 0.45f },               /* from near the floor, up into the shade */
        };
        static const struct { const char *name; int what; } var[8] = {
            { "off", P_OFF }, { "glow", P_GLOW }, { "slots", P_SLOTS }, { "halo_plain", P_HALO_PLAIN }, { "halo_fade", P_HALO_FADE },
            { "halo_toward", P_HALO_TOWARD }, { "halo_fade_slots", P_HALO_FADE_SLOTS }, { "halo_toward_slots", P_HALO_TOWARD_SLOTS },
        };
        unsigned v, w;
        char name[80];

        for (v = 0; v < 4u; v++)
            for (w = 0; w < 8u; w++) {
                snprintf(name, sizeof name, "lamp_%s_%s", view[v].name, var[w].name);
                picture(argv[1], name, var[w].what, view[v].angle, view[v].radius, view[v].height);
            }
        printf("  32 pictures in %s (a software stand-in: the look, not the PVR's picture, no cost)\n", argv[1]);
    }
    if (fails) printf("ao4_lamp_test: %d FAILED\n", fails); else printf("ao4_lamp_test: all passed\n");
    return fails ? 1 : 0;
}
